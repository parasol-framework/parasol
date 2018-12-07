
static void folder_free(APTR Address)
{
   struct DirInfo *folder = (struct DirInfo *)Address;

   // Note: Virtual file systems should focus on destroying handles as fs_closedir() will take care of memory and list
   // deallocations.

   if ((folder->prvVirtualID) AND (folder->prvVirtualID != DEFAULT_VIRTUALID)) {
      ULONG v;
      for (v=0; v < glVirtualTotal; v++) {
         if (glVirtual[v].VirtualID IS folder->prvVirtualID) {
            FMSG("CloseDir()","Virtual file driver function @ %p", glVirtual[v].CloseDir);
            if (glVirtual[v].CloseDir) glVirtual[v].CloseDir(folder);
            break;
         }
      }
   }

   fs_closedir(folder);
}

static struct ResourceManager glResourceFolder = {
   "Folder",
   &folder_free
};

/*****************************************************************************

-FUNCTION-
OpenDir: Opens a folder for content scanning.

The OpenDir() function is used to open a folder for scanning via the ~ScanDir() function.  If the provided Path can be
accessed, a DirInfo structure will be returned in the Info parameter, which will need to be passed to ~ScanDir().  Once
the scanning process is complete, call the ~FreeResource() function.

When opening a folder, it is necessary to indicate the type of files that are of interest.  If no flags are defined,
the scanner will return file and folder names only.  Only a subset of the available RDF flags may be used, namely
SIZE, DATE, PERMISSIONS, FILE, FOLDER, QUALIFY, TAGS.

-INPUT-
cstr Path: The folder location to be scanned.  Using an empty string will scan for volume names.
int(RDF) Flags: Optional flags.
!resource(DirInfo) Info: A DirInfo structure will be returned in the pointer referenced here.

-ERRORS-
Okay
Args
NullArgs
DirEmpty
AllocMemory

*****************************************************************************/

ERROR OpenDir(CSTRING Path, LONG Flags, struct DirInfo **Result)
{
   if ((!Path) OR (!Result)) return LogError(ERH_OpenDir, ERR_NullArgs);

   FMSG("~OpenDir()","Path: '%s'", Path);

   *Result = NULL;

   if (!(Flags & (RDF_FOLDER|RDF_FILE))) Flags |= RDF_FOLDER|RDF_FILE;

   ERROR error;
   STRING resolved_path;
   if (!Path[0]) Path = ":";
   if (!(error = ResolvePath(Path, 0, &resolved_path))) {
      // NB: We use MAX_FILENAME rather than resolve_len in the allocation size because fs_opendir() requires more space.
      LONG path_len = StrLength(Path) + 1;
      LONG resolve_len = StrLength(resolved_path) + 1;
      struct DirInfo *dir;
      if (AllocMemory(sizeof(struct DirInfo) + sizeof(struct FileInfo) + MAX_FILENAME + path_len + MAX_FILENAME,
            MEM_DATA|MEM_MANAGED, (APTR *)&dir, NULL) != ERR_Okay) {
         FreeResource(resolved_path);
         STEP();
         return ERR_AllocMemory;
      }

      set_memory_manager(dir, &glResourceFolder);

      dir->Info         = (struct FileInfo *)(dir + 1);
      dir->Info->Name   = (STRING)(dir->Info + 1);
      dir->prvPath      = dir->Info->Name + MAX_FILENAME;
      dir->prvFlags     = Flags | RDF_OPENDIR;
      dir->prvVirtualID = DEFAULT_VIRTUALID;
      dir->prvResolvedPath = dir->prvPath + path_len;
      dir->prvResolveLen = resolve_len;
      #ifdef _WIN32
         dir->prvHandle = (WINHANDLE)-1;
      #endif
      CopyMemory(Path, dir->prvPath, path_len);
      CopyMemory(resolved_path, dir->prvResolvedPath, resolve_len);

      FreeResource(resolved_path);

      if ((Path[0] IS ':') OR (!Path[0])) {
         if (!(Flags & RDF_FOLDER)) {
            FreeResource(dir);
            STEP();
            return ERR_DirEmpty;
         }
         *Result = dir;
         STEP();
         return ERR_Okay;
      }

      const struct virtual_drive *virtual = get_fs(dir->prvResolvedPath);

      if (!virtual->OpenDir) {
         FreeResource(dir);
         STEP();
         return ERR_DirEmpty;
      }

      if (!(error = virtual->OpenDir(dir))) {
         dir->prvVirtualID = virtual->VirtualID;
         *Result = dir;
         STEP();
         return ERR_Okay;
      }

      FreeResource(dir);
      STEP();
      return error;
   }
   else {
      STEP();
      return LogError(ERH_OpenDir, ERR_ResolvePath);
   }
}

/*****************************************************************************

-FUNCTION-
ScanDir: Scans the content of a folder, by item.

The ScanDir() function is used to scan for files and folders in a folder that you have opened using the
~OpenDir() function. The ScanDir() function is intended to be used in a simple loop, returning a single item
for each function call that you make.  The following code sample illustrates typical usage:

<pre>
struct DirInfo *info;
if (!OpenDir(path, RDF_FILE|RDF_FOLDER, &info)) {
   while (!ScanDir(info)) {
      LogMsg("File: %s", info->Name);
   }
   FreeResource(info);
}
</pre>

For each item that you scan, you will be able to read the Info structure for information on that item.  The DirInfo
structure contains a FileInfo pointer that consists of the following fields:

<struct lookup="FileInfo"/>

RDF flags that may be returned in the Flags field are VOLUME, FOLDER, FILE, LINK.

-INPUT-
resource(DirInfo) Info: Pointer to a DirInfo structure for storing scan results.

-ERRORS-
Okay: An item was successfully scanned from the folder.
Args
DirEmpty: There are no more items to scan.
-END-

*****************************************************************************/

ERROR ScanDir(struct DirInfo *Dir)
{
   if (!Dir) return LogError(ERH_ScanDir, ERR_NullArgs);

   struct FileInfo *file;
   if (!(file = Dir->Info)) { FMSG("ScanDir","Missing Dir->Info"); return LogError(ERH_ScanDir, ERR_InvalidData); }
   if (!file->Name) { FMSG("ScanDir","Missing Dir->Info->Name"); return LogError(ERH_ScanDir, ERR_InvalidData); }

   file->Name[0] = 0;
   file->Flags   = 0;
   file->Permissions = 0;
   file->Size    = 0;
   file->UserID  = 0;
   file->GroupID = 0;

   if (file->Tags) { FreeResource(file->Tags); file->Tags = NULL; }

   // Support for scanning of volume names

   if ((Dir->prvPath[0] IS ':') OR (!Dir->prvPath[0])) {
      LONG i;
      if (!AccessPrivateObject((OBJECTPTR)glVolumes, 8000)) {
         struct ConfigEntry *entries = glVolumes->Entries;

         if ((!entries) OR (glVolumes->AmtEntries <= 0)) {
            ReleasePrivateObject((OBJECTPTR)glVolumes);
            return ERR_DirEmpty;
         }

         // Go to the volume that is indexed

         if (Dir->prvIndex > 0) {
            LONG count = 0;
            CSTRING section = entries[0].Section;
            for (i=0; i < glVolumes->AmtEntries; i++) {
               if (StrMatch(entries[i].Section, section) != ERR_Okay) {
                  if (++count >= Dir->prvIndex) break;
                  section = entries[i].Section;
               }
            }

            if (i >= glVolumes->AmtEntries) {
               ReleasePrivateObject((OBJECTPTR)glVolumes);
               return ERR_DirEmpty;
            }
         }
         else i = 0;

         CSTRING section = entries[i].Section;

         while ((i < glVolumes->AmtEntries) AND (!StrMatch(section, entries[i].Section))) {
            if (!StrMatch("Name", entries[i].Key)) {
               LONG j = StrCopy(entries[i].Data, file->Name, MAX_FILENAME-2);
               if (Dir->prvFlags & RDF_QUALIFY) {
                  file->Name[j++] = ':';
                  file->Name[j] = 0;
               }
               file->Flags |= RDF_VOLUME;
            }
            else if ((!StrMatch("Hidden", entries[i].Key)) AND (!StrMatch("Yes", entries[i].Data))) {
               file->Flags |= RDF_HIDDEN;
            }
            else if (Dir->prvFlags & RDF_TAGS) {
               if (!StrMatch("Label", entries[i].Key)) {
                  if (entries[i].Data[0]) {
                     AddInfoTag(file, "Label", entries[i].Data);
                  }
               }
            }

            i++;
         }

         Dir->prvIndex++;

         ReleasePrivateObject((OBJECTPTR)glVolumes);

         if (file->Name[0]) return ERR_Okay;
         else return ERR_DirEmpty;
      }
      else return LogError(ERH_ScanDir, ERR_AccessObject);
   }

   // In all other cases, pass functionality to the filesystem driver.

   ERROR error = ERR_NoSupport;
   if (Dir->prvVirtualID IS DEFAULT_VIRTUALID) {
      error = fs_scandir(Dir);
   }
   else {
      UWORD v;
      for (v=0; v < glVirtualTotal; v++) {
         if (glVirtual[v].VirtualID != Dir->prvVirtualID) continue;
         if (glVirtual[v].ScanDir) error = glVirtual[v].ScanDir(Dir);
         break;
      }
   }

   if ((file->Name[0]) AND (Dir->prvFlags & RDF_DATE)) {
      file->TimeStamp = calc_timestamp(&file->Modified);
   }

   return error;
}
