
/*****************************************************************************

-FUNCTION-
ResolvePath: Converts volume-based paths into absolute paths applicable to the host platform.

This function will convert a file path to its resolved form, according to the host system.  For example, a Linux
system might resolve `drive1:documents/readme.txt` to `/documents/readme.txt`.  A Windows system might
resolve the path to `c:\documents\readme.txt`.

The resulting path is guaranteed to be absolute, meaning the use of sequences such as '..', '//' and './' will be
eliminated.

If the path can be resolved to more than one file, the ResolvePath() method will attempt to guess the correct path by
checking the validity of each possible location.  For instance, if resolving a path of "user:document.txt"
and the "user:" volume refers to both "system:users/joebloggs/" and "system:users/default/", the routine will check
both directories for the existence of the "document.txt" file to determine the correct location.  While helpful, this
can cause problems if the intent is to create a new file.  To circumvent this feature, use the RSF_NO_FILE_CHECK
setting in the Flags parameter.

When checking for the location of a file, ResolvePath() will only accept an exact file name match.  If the path must be
treated as an approximation (i.e. file extensions can be ignored) then use the RSF_APPROXIMATE flag to tell the
function to ignore extensions for the purpose of file name matching.

To resolve the location of executable programs on Unix systems, use the RSF_PATH flag.  This uses the PATH environment
variable to resolve the file name specified in the Path parameter.

The resolved path will be returned in the Result parameter as an allocated memory block.  It must be removed once it is
no longer required with FreeResource().

<types lookup="RSF"/>

If the path resolves to a virtual drive, it may not be possible to confirm whether the target file exists if the
virtual driver does not support this check.  This is common when working with network drives.

-INPUT-
cstr Path: The path to be resolved.
int(RSF) Flags: Optional flags.
!str Result: Must point to an empty STRING variable so that the resolved path can be stored.  If NULL, ResolvePath() will work as normal and return a valid error code without the result string.

-ERRORS-
Okay:            The path was resolved.
NullArgs:        Invalid arguments were specified.
AllocMemory:     The result string could not be allocated.
ExclusiveDenied: Access to the SystemVolumes object was denied.
Search:          The given volume does not exist.
FileNotFound:    The path was resolved, but the referenced file or folder does not exist (use RSF_NO_FILE_CHECK if you need to avoid this error code).
Loop:            The volume refers back to itself.

-END-

*****************************************************************************/

static ERROR resolve(objConfig *, STRING, STRING, LONG);
static ERROR resolve_path_env(CSTRING RelativePath, STRING *Result);
static THREADVAR UBYTE tlClassLoaded;
#define SIZE_RESBUFFER 250

ERROR ResolvePath(CSTRING Path, LONG Flags, STRING *Result)
{
   LONG i, loop;
   UBYTE src[SIZE_RESBUFFER];
   UBYTE dest[SIZE_RESBUFFER];

   FMSG("~ResolvePath()","%s, Flags: $%.8x", Path, Flags);

   if (!Path) {
      LogError(ERH_ResolvePath, ERR_NullArgs);
      STEP();
      return ERR_NullArgs;
   }

   if (Result) *Result = NULL;

   tlClassLoaded = FALSE;

   if (Path[0] IS '~') {
      Flags |= RSF_APPROXIMATE;
      Path++;
   }

   if (!StrCompare("string:", Path, 7, 0)) {
      if ((*Result = StrClone(Path))) { STEP(); return ERR_Okay; }
      else { STEP(); return LogError(ERH_ResolvePath, ERR_AllocMemory); }
   }

   // Check if the Path argument contains a volume character.  If it does not, make a clone of the string and return it.

   UBYTE resolved = FALSE;
#ifdef _WIN32
   if ((LCASE(Path[0]) >= 'a') AND (LCASE(Path[0]) <= 'z') AND (Path[1] IS ':')) {
      resolved = TRUE; // Windows drive letter reference discovered
      if ((Path[2] != '/') AND (Path[2] != '\\')) {
         // Ensure that the path is correctly formed in order to pass test_path()
         src[0] = Path[0];
         src[1] = ':';
         src[2] = '\\';
         StrCopy(Path+2, src+3, sizeof(src)-3);
         Path = src;
      }
   }
   else if ((Path[0] IS '/') AND (Path[1] IS '/')) {
      resolved = TRUE; // UNC path discovered
   }
   else if ((Path[0] IS '\\') AND (Path[1] IS '\\')) {
      resolved = TRUE; // UNC path discovered
   }
#elif __unix__
   if ((Path[0] IS '/') OR (Path[0] IS '\\')) resolved = TRUE;
#endif

   // Use the PATH environment variable to resolve the filename.  This can only be done if the path is relative
   // (ideally with no leading folder references).

   if ((!resolved) AND (Flags & RSF_PATH)) {
      if (!resolve_path_env(Path, Result)) {
         STEP();
         return ERR_Okay;
      }
   }

   if (!resolved) {
      for (i=0; (Path[i]) AND (Path[i] != ':') AND (Path[i] != '/') AND (Path[i] != '\\'); i++);
      if (Path[i] != ':') resolved = TRUE;
   }

   if (resolved) {
      if (Flags & RSF_APPROXIMATE) {
         StrCopy(Path, dest, sizeof(dest));
         if (!test_path(dest, RSF_APPROXIMATE)) Path = dest;
         else {
            STEP();
            return ERR_FileNotFound;
         }
      }
      else if (!(Flags & RSF_NO_FILE_CHECK)) {
         StrCopy(Path, dest, sizeof(dest));
         if (!test_path(dest, 0)) Path = dest;
         else {
            STEP();
            return ERR_FileNotFound;
         }
      }

      if (!Result) {
         STEP();
         return ERR_Okay;
      }
      else if ((*Result = cleaned_path(Path))) {
         STEP();
         return ERR_Okay;
      }
      else if ((*Result = StrClone(Path))) {
         STEP();
         return ERR_Okay;
      }
      else {
         LogError(ERH_ResolvePath, ERR_AllocMemory);
         STEP();
         return ERR_AllocMemory;
      }
   }

   StrCopy(Path, src, sizeof(src)); // Copy the Path argument to our internal buffer

   // Keep looping until the volume is resolved

   dest[0] = 0;
   if (!AccessPrivateObject((OBJECTPTR)glVolumes, 4000)) {
      ERROR error = ERR_Failed;
      for (loop=10; loop > 0; loop--) {
         error = resolve(glVolumes, src, dest, Flags);

         if (error IS ERR_VirtualVolume) {
            FMSG("ResolvePath","Detected virtual volume '%s'", dest);

            // If RSF_CHECK_VIRTUAL is set, return ERR_VirtualVolume for reserved volume names.

            if (!(Flags & RSF_CHECK_VIRTUAL)) error = ERR_Okay;

            if (Result) {
               if (!(*Result = StrClone(dest))) error = ERR_AllocMemory;
            }

            break;
         }
         else if (error) break;
         else {
            #ifdef _WIN32 // UNC network path check
               if (((dest[0] IS '\\') AND (dest[1] IS '\\')) OR ((dest[0] IS '/') AND (dest[1] IS '/'))) {
                  goto resolved_path;
               }
            #endif

            // Check if the path has been resolved by looking for a ':' character

            for (i=0; (dest[i]) AND (dest[i] != ':') AND (dest[i] != '/') AND (dest[i] != '\\'); i++);

            #ifdef _WIN32
            if ((dest[i] IS ':') AND (i > 1)) {
            #else
            if (dest[i] IS ':') {
            #endif
               // Copy the destination to the source buffer and repeat the resolution process.

               if (Flags & RSF_NO_DEEP_SCAN) {
                  STEP();
                  return ERR_Failed;
               }

               StrCopy(dest, src, sizeof(src));
               continue; // Keep resolving
            }
         }

#ifdef _WIN32
resolved_path:
#endif
         if (Result) {
            if (!(*Result = cleaned_path(dest))) {
               if (!(*Result = StrClone(dest))) error = ERR_AllocMemory;
            }
         }

         break;
      } // for()

      ReleasePrivateObject((OBJECTPTR)glVolumes);

      if (loop > 0) { // Note that loop starts at 10 and decrements to zero
         if ((!error) AND (!dest[0])) error = ERR_Failed;
         STEP();
         return error;
      }
      else {
         STEP();
         return ERR_Loop;
      }
   }
   else {
      LogError(ERH_ResolvePath, ERR_AccessObject);
      STEP();
      return ERR_ExclusiveDenied;
   }
}

//****************************************************************************
// For resolving file references via the host environment's PATH variable.  This will only work for relative paths.

#ifdef __unix__

static ERROR resolve_path_env(CSTRING RelativePath, STRING *Result)
{
   struct stat64 info;
   CSTRING path;
   UBYTE src[512];

   // If a path to the file isn't available, scan the PATH environment variable. In Unix the separator is :

   if ((path = getenv("PATH")) AND (path[0])) {
      LONG j = 0, k;
      while (path[j]) {
         for (k=0; (path[j+k]) AND (path[j+k] != ':') AND (k < sizeof(src)-1); k++) {
            src[k] = path[j+k];
         }
         if ((k > 0) AND (src[k-1] != '/')) src[k++] = '/';

         StrCopy(RelativePath, src+k, sizeof(src)-k);

         if (!stat64(src, &info)) {
            if (!S_ISDIR(info.st_mode)) { // Successfully identified file location
               if (Result) *Result = cleaned_path(src);
               return ERR_Okay;
            }
         }

         while (path[j+k] IS ':') k++; // Go to the next path in the list
         j += k;
      }
   }
   else MSG("Failed to read PATH environment variable.");

   return ERR_NothingDone;
}

#elif _WIN32

static ERROR resolve_path_env(CSTRING RelativePath, STRING *Result)
{
   struct stat64 info;
   CSTRING path;
   UBYTE src[512];

   // If a path to the file isn't available, scan the PATH environment variable. In Windows the separator is ;

   if ((path = getenv("PATH")) AND (path[0])) {
      MSG("Got PATH: %s", path);

      LONG j = 0, k;
      while (path[j]) {
         for (k=0; (path[j+k]) AND (path[j+k] != ';') AND (k < sizeof(src)-1); k++) {
            src[k] = path[j+k];
         }
         j += k;
         if ((k > 0) AND (src[k-1] != '/')) src[k++] = '/';

         StrCopy(RelativePath, src+k, sizeof(src)-k);

         if (!stat64(src, &info)) {
            if (!S_ISDIR(info.st_mode)) { // Successfully identified file location
               if (Result) *Result = cleaned_path(src);
               return ERR_Okay;
            }
         }

         while (path[j] IS ';') j++; // Go to the next path in the list
      }
   }
   else MSG("Failed to read PATH environment variable.");

   return ERR_NothingDone;
}

#endif

/*****************************************************************************
** Note: This function calls itself recursively.  For use by ResolvePath() only.
**
** Config  - The SystemVolumes configuration object.
** Source  - The file string that we are trying to resolve.
** Dest    - Buffer area; the resolved location will be stored here.
** Flags   - Optional RSF flags.
*/

static ERROR resolve_object_path(STRING, STRING, STRING, LONG);

static ERROR resolve(objConfig *Config, STRING Source, STRING Dest, LONG Flags)
{
   char fullpath[SIZE_RESBUFFER];
   char buffer[SIZE_RESBUFFER];
   LONG i, j, k, pos, loop;
   ERROR error;

   struct virtual_drive *vdrive;
   if ((vdrive = get_virtual(Source))) {
      StrCopy(Source, Dest, COPY_ALL);
      return ERR_VirtualVolume;
   }

   for (pos=0; Source[pos] != ':'; pos++) {
      if (!Source[pos]) return LogError(ERH_ResolvePath, ERR_InvalidData);
   }
   pos++;

   Source[pos-1] = 0; // Remove the volume symbol for the string comparison
   fullpath[0] = 0;
   for (i=0; i < Config->AmtEntries; i++) {
      if ((!StrMatch("Name", Config->Entries[i].Key)) AND (!StrMatch(Config->Entries[i].Data, Source))) {
         while ((i > 0) AND (!StrMatch(Config->Entries[i].Section, Config->Entries[i-1].Section))) i--;

         for (j=i; j < Config->AmtEntries; j++) {
            if (!StrMatch("Path", Config->Entries[j].Key)) {
               StrCopy(Config->Entries[j].Data, fullpath, sizeof(fullpath));
               break;
            }
         }
         break;
      }
   }

   if (!fullpath[0]) {
      LogF("ResolvePath","No matching volume for \"%s\".", Source);
      Source[pos-1] = ':'; // Put back the volume symbol
      return ERR_Search;
   }

   Source[pos-1] = ':'; // Put back the volume symbol

   // Handle the ":ObjectName" case

   if (fullpath[0] IS ':') return resolve_object_path(fullpath+1, Source, Dest, sizeof(fullpath)-1);

   FMSG("~resolve()","%s, Resolved Path: %s, Flags: $%.8x", Source, fullpath, Flags);

   STRING path = fullpath;

   // Check if the CLASS: reference is used.  If so, respond by loading the class that handles the volume.  The class'
   // module should then create a public object and set the volume's path with the format ":ObjectName".  We'll then
   // discover it on our next recursive attempt.

   if (!StrCompare("CLASS:", path, 6, STR_MATCH_CASE)) {
      for (i=0; Source[i]; i++) Dest[i] = Source[i];  // Return an exact duplicate of the original source string
      Dest[i] = 0;

      if (get_virtual(Source)) {
         STEP();
         return ERR_VirtualVolume;
      }

      if (tlClassLoaded) { // We already attempted to load this class on a previous occasion - we must fail
         STEP();
         return ERR_Failed;
      }

      FindClass(ResolveClassName(path + 6));

      FMSG("resolve","Found virtual volume from class %s", path + 6);

      tlClassLoaded = TRUE; // This setting will prevent recursion

      STEP();
      return ERR_VirtualVolume;
   }

   while (*path) {
      // Copy the resolved volume path to the destination buffer

      for (k=0; (*path) AND (*path != '|') AND (k < SIZE_RESBUFFER-1);) {
         if (*path IS ';') LogF("@resolve","Use of ';' obsolete, use | in path %s", fullpath);

         if (k > 0) {
            if ((*path IS '\\') AND (path[1] IS '\\')) path++; // Eliminate dual slashes - with an exception for UNC paths
            else if ((*path IS '/') AND (path[1] IS '/')) path++;
            else Dest[k++] = *path++;
         }
         else Dest[k++] = *path++;
      }

      if ((Dest[k-1] != '/') AND (Dest[k-1] != '\\') AND (k < SIZE_RESBUFFER-1)) Dest[k++] = '/'; // Add a trailing slash if it is missing

      // Copy the rest of the source to the destination buffer

      j = pos;
      while ((Source[j] IS '/') OR (Source[j] IS '\\')) j++;
      while ((Source[j]) AND (k < SIZE_RESBUFFER-1)) Dest[k++] = Source[j++];
      Dest[k++] = 0;

      // Fully resolve the path to a system folder before testing it (e.g. "scripts:" to "parasol:scripts/" to "c:\parasol\scripts\" will be resolved through this recursion).

      #ifdef _WIN32
         if ((Dest[1] IS ':') AND ((Dest[2] IS '/') OR (Dest[2] IS '\\'))) j = 0;
         else if ((Dest[0] IS '/') AND (Dest[1] IS '/')) j = 0;
         else if ((Dest[0] IS '\\') AND (Dest[1] IS '\\')) j = 0;
         else for (j=0; (Dest[j]) AND (Dest[j] != ':') AND (Dest[j] != '/'); j++);
      #else
         for (j=0; (Dest[j]) AND (Dest[j] != ':') AND (Dest[j] != '/'); j++);
      #endif

      error = -1;
      for (loop=10; loop > 0; loop--) {
         if ((Dest[j] IS ':') AND (j > 1)) { // Remaining ':' indicates more path resolution is required.
            error = resolve(Config, Dest, buffer, Flags);

            if (!error) {
               // Copy the result from buffer to Dest.
               for (j=0; buffer[j]; j++) Dest[j] = buffer[j];
               Dest[j] = 0;

               // Reexamine the result for the presence of a colon.
               for (j=0; (Dest[j]) AND (Dest[j] != ':') AND (Dest[j] != '/'); j++);
            }
            else break; // Path not resolved or virtual volume detected.
         }
         else break;
      }

      if (loop <= 0) {
         LogF("@resolve","Infinite loop on path '%s'", Dest);
         STEP();
         return ERR_Loop;
      }

      if (!error) {
         STEP();
         return ERR_Okay;
      }

      // Return now if no file checking is to be performed

      if (Flags & RSF_NO_FILE_CHECK) {
         FMSG("resolve","No file check will be performed.");
         STEP();
         return ERR_Okay;
      }

      if (!test_path(Dest, Flags)) {
         FMSG("resolve","File found, path resolved successfully.");
         STEP();
         return ERR_Okay;
      }

      FMSG("resolve","File does not exist at %s", Dest);

      if (Flags & RSF_NO_DEEP_SCAN) {
         FMSG("resolve","No deep scanning - additional paths will not be checked.");
         break;
      }

      if (*path) path++;
   }

   FMSG("resolve","Resolved path but no matching file for %s\"%s\".", (Flags & RSF_APPROXIMATE) ? "~" : "", Source);
   STEP();
   return ERR_FileNotFound;
}

//****************************************************************************
// For cases such as ":SystemIcons", we find the referenced object and ask it to resolve the path for us.  (In effect,
// the object will be used as a plugin for volume resolution).
//
// If the path is merely ":" or resolve_virtual() returns ERR_VirtualVolume, return the VirtualVolume error code to
// indicate that no further resolution is required.

static ERROR resolve_object_path(STRING Path, STRING Source, STRING Dest, LONG PathSize)
{
   ERROR (*resolve_virtual)(OBJECTPTR, STRING, STRING, LONG);
   ERROR error = ERR_VirtualVolume;

   if (Path[0]) {
      OBJECTID volume_id;
      if (!FastFindObject(Path, NULL, &volume_id, 1, NULL)) {
         OBJECTPTR object;
         if (!AccessObject(volume_id, 5000, &object)) {
            if ((!GetPointer(object, FID_ResolvePath, &resolve_virtual)) AND (resolve_virtual)) {
               error = resolve_virtual(object, Source, Dest, PathSize);
            }
            ReleaseObject(object);
         }
      }
   }

   if (error IS ERR_VirtualVolume) { // Return an exact duplicate of the original source string
      StrCopy(Source, Dest, COPY_ALL);
      return ERR_VirtualVolume;
   }
   else if (error != ERR_Okay) return LogError(ERH_ResolvePath, error);
   else return ERR_Okay;
}
