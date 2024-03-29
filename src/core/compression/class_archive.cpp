/*********************************************************************************************************************

The source code of the Parasol project is made publicly available under the terms described in the LICENSE.TXT file
that is distributed with this package.  Please refer to it for further information on licensing.

**********************************************************************************************************************

-CLASS-
FileArchive: Creates simple read-only volumes backed by compressed archives.

The FileArchive class makes it possible to create virtual file system volumes that are based on compressed file
archives.  It is not necessary for client programs to instantiate a FileArchive to make use of this functionality.
Instead, create a @Compression object that declares a Path to the source archive file and set an
ArchiveName for reference.  In the C++ example below, take note of the `untracked` specifier to prevent the
@Compression object from being collected when it goes out of scope:

<pre>
objCompression::create::untracked(
   fl::Path("user:documents/myfile.zip"),
   fl::ArchiveName("myfiles")
);
</pre>

With the Compression object in place, opening files within the archive only requires the correct path
reference.  The format is `archive:ArchiveName/path/to/file.ext` and the Fluid example below illustrates:

`obj.new('file', { path='archive:myfiles/readme.txt', flags='!READ' })`

-END-

*********************************************************************************************************************/

using namespace pf;

constexpr LONG LEN_ARCHIVE = 8; // "archive:" length

struct prvFileArchive {
   ZipFile  Info;
   z_stream Stream;
   extFile  *FileStream;
   extCompression *Archive;
   UBYTE    InputBuffer[SIZE_COMPRESSION_BUFFER];
   UBYTE    OutputBuffer[SIZE_COMPRESSION_BUFFER];
   UBYTE    *ReadPtr;      // Current position within OutputBuffer
   LONG     InputLength;
   bool     Inflating;
   bool     InvalidState; // Set to true if the archive is corrupt.
};

struct ArchiveDriver {
   std::list<ZipFile>::iterator Index;
};

static std::unordered_map<ULONG, extCompression *> glArchives;

static ERROR close_folder(DirInfo *);
static ERROR open_folder(DirInfo *);
static ERROR get_info(CSTRING, FileInfo *, LONG);
static ERROR scan_folder(DirInfo *);
static ERROR test_path(STRING, RSF, LOC *);

//********************************************************************************************************************

static void reset_state(extFile *Self)
{
   auto prv = (prvFileArchive *)Self->ChildPrivate;

   if (prv->Inflating) { inflateEnd(&prv->Stream); prv->Inflating = false; }

   prv->Stream.avail_in = 0;
   prv->ReadPtr = NULL;
   Self->Position = 0;
}

//********************************************************************************************************************

static ERROR seek_to_item(extFile *Self)
{
   auto prv = (prvFileArchive *)Self->ChildPrivate;
   if (prv->InvalidState) return ERR_InvalidState;

   auto &item = prv->Info;

   acSeekStart(prv->FileStream, item.Offset + HEAD_EXTRALEN);
   prv->ReadPtr = NULL;

   UWORD extra_len;
   if (flReadLE(prv->FileStream, &extra_len)) return ERR_Read;
   ULONG stream_start = item.Offset + HEAD_LENGTH + item.NameLen + extra_len;
   if (acSeekStart(prv->FileStream, stream_start) != ERR_Okay) return ERR_Seek;

   if (item.CompressedSize > 0) {
      Self->Flags |= FL::FILE;

      if (item.DeflateMethod IS 0) { // The file is stored rather than compressed
         Self->Size = item.CompressedSize;
         return ERR_Okay;
      }
      else if ((item.DeflateMethod IS 8) and (!inflateInit2(&prv->Stream, -MAX_WBITS))) {
         prv->Inflating = true;
         Self->Size = item.OriginalSize;
         return ERR_Okay;
      }
      else return ERR_Failed;
   }
   else { // Folder or empty file
      if (item.IsFolder) Self->Flags |= FL::FOLDER;
      else Self->Flags |= FL::FILE;
      Self->Size = 0;
      return ERR_Okay;
   }
}

//********************************************************************************************************************
// Insert a new compression object as an archive.

extern void add_archive(extCompression *Compression)
{
   glArchives[Compression->ArchiveHash] = Compression;
}

//********************************************************************************************************************

extern void remove_archive(extCompression *Compression)
{
   glArchives.erase(Compression->ArchiveHash);
}

//********************************************************************************************************************
// Return the archive referenced by 'archive:[NAME]/...'

extern extCompression * find_archive(CSTRING Path, std::string &FilePath)
{
   Log log(__FUNCTION__);

   if (!Path) return NULL;

   // Compute the hash of the referenced archive.

   CSTRING path = Path + LEN_ARCHIVE;
   ULONG hash = 5381;
   char c;
   while ((c = *path++) and (c != '/') and (c != '\\')) {
      if ((c >= 'A') and (c <= 'Z')) hash = (hash<<5) + hash + c - 'A' + 'a';
      else hash = (hash<<5) + hash + c;
   }

   if (glArchives.contains(hash)) {
      FilePath.assign(path);
      return glArchives[hash];
   }
   else {
      log.warning("No match for path '%s'", Path);
      return NULL;
   }
}

//********************************************************************************************************************

static ERROR ARCHIVE_Activate(extFile *Self, APTR Void)
{
   Log log;

   auto prv = (prvFileArchive *)Self->ChildPrivate;

   if (!prv->Archive) return log.warning(ERR_SystemCorrupt);

   if (prv->FileStream) return ERR_Okay; // Already activated

   log.msg("Allocating file stream for item %s", prv->Info.Name.c_str());

   if ((prv->FileStream = extFile::create::integral(
      fl::Name("ArchiveFileStream"),
      fl::Path(prv->Archive->Path),
      fl::Flags(FL::READ)))) {

      ERROR error = seek_to_item(Self);
      if (error) log.warning(error);
      return error;
   }
   else return ERR_File;
}

//********************************************************************************************************************

static ERROR ARCHIVE_Free(extFile *Self, APTR Void)
{
   auto prv = (prvFileArchive *)Self->ChildPrivate;

   if (prv) {
      if (prv->FileStream) { FreeResource(prv->FileStream); prv->FileStream = NULL; }
      if (prv->Inflating)  { inflateEnd(&prv->Stream); prv->Inflating = false; }
      prv->~prvFileArchive();
   }

   return ERR_Okay;
}

//********************************************************************************************************************

static ERROR ARCHIVE_Init(extFile *Self, APTR Void)
{
   Log log;

   if (!Self->Path) return ERR_FieldNotSet;

   if (StrCompare("archive:", Self->Path, LEN_ARCHIVE) != ERR_Okay) return ERR_NoSupport;

   if ((Self->Flags & (FL::NEW|FL::WRITE)) != FL::NIL) return log.warning(ERR_ReadOnly);

   ERROR error = ERR_Search;
   if (!AllocMemory(sizeof(prvFileArchive), MEM::DATA, &Self->ChildPrivate, NULL)) {
      auto prv = (prvFileArchive *)Self->ChildPrivate;
      new (prv) prvFileArchive;

      if (Self->Path[StrLength(Self->Path)-1] IS ':') { // Nothing is referenced
         return ERR_Okay;
      }
      else {
         std::string file_path;

         prv->Archive = find_archive(Self->Path, file_path);

         if (prv->Archive) {
            // TODO: This is a slow scan and could be improved if a hashed directory structure was
            // generated during add_archive() first; then we could perform a quick lookup here.

            auto it = prv->Archive->Files.begin();
            for (; it != prv->Archive->Files.end(); it++) {
               if (!StrCompare(file_path, it->Name, 0, STR::CASE|STR::MATCH_LEN)) break;
            }

            if ((it IS prv->Archive->Files.end()) and ((Self->Flags & FL::APPROXIMATE) != FL::NIL)) {
               file_path.append(".*");
               for (it = prv->Archive->Files.begin(); it != prv->Archive->Files.end(); it++) {
                  if (!StrCompare(file_path, it->Name, 0, STR::WILDCARD)) break;
               }
            }

            if (it != prv->Archive->Files.end()) {
               prv->Info = *it;
               if (!(error = Self->activate())) {
                  error = Self->query();
               }
            }
         }
      }

      if (error) {
         prv->~prvFileArchive();
         FreeResource(Self->ChildPrivate);
         Self->ChildPrivate = NULL;
      }
   }
   else error = ERR_AllocMemory;

   return error;
}

//********************************************************************************************************************

static ERROR ARCHIVE_Query(extFile *Self, APTR Void)
{
   auto prv = (prvFileArchive *)(Self->ChildPrivate);

   // Activate the source if this hasn't been done already.

   ERROR error;
   if (!prv->FileStream) {
      error = Self->activate();
      if (error) return error;
   }

   // If security flags are present, convert them to file system permissions.

   auto &item = prv->Info;
   if (item.Flags & ZIP_SECURITY) {
      PERMIT permissions = PERMIT::NIL;
      if (item.Flags & ZIP_UEXEC) permissions |= PERMIT::USER_EXEC;
      if (item.Flags & ZIP_GEXEC) permissions |= PERMIT::GROUP_EXEC;
      if (item.Flags & ZIP_OEXEC) permissions |= PERMIT::OTHERS_EXEC;

      if (item.Flags & ZIP_UREAD) permissions |= PERMIT::USER_READ;
      if (item.Flags & ZIP_GREAD) permissions |= PERMIT::GROUP_READ;
      if (item.Flags & ZIP_OREAD) permissions |= PERMIT::OTHERS_READ;

      if (item.Flags & ZIP_UWRITE) permissions |= PERMIT::USER_WRITE;
      if (item.Flags & ZIP_GWRITE) permissions |= PERMIT::GROUP_WRITE;
      if (item.Flags & ZIP_OWRITE) permissions |= PERMIT::OTHERS_WRITE;

      Self->Permissions = permissions;
   }

   return ERR_Okay;
}

//********************************************************************************************************************

static ERROR ARCHIVE_Read(extFile *Self, struct acRead *Args)
{
   Log log;

   if ((!Args) or (!Args->Buffer)) return log.warning(ERR_NullArgs);
   else if (Args->Length == 0) return ERR_Okay;
   else if (Args->Length < 0) return ERR_OutOfRange;

   auto prv = (prvFileArchive *)Self->ChildPrivate;

   if (prv->InvalidState) return ERR_InvalidState;

   if (prv->Info.DeflateMethod IS 0) {
      ERROR error = acRead(prv->FileStream, Args->Buffer, Args->Length, &Args->Result);
      if (!error) Self->Position += Args->Result;
      return error;
   }
   else {
      Args->Result = 0;

      auto &zf = prv->Info;

      //log.trace("Decompressing %d bytes to %d, buffer size %d", zf.CompressedSize, zf.OriginalSize, Args->Length);

      if ((prv->Inflating) and (!prv->Stream.avail_in)) { // Initial setup
         struct acRead read = {
            .Buffer = prv->InputBuffer,
            .Length = (zf.CompressedSize < SIZE_COMPRESSION_BUFFER) ? (LONG)zf.CompressedSize : SIZE_COMPRESSION_BUFFER
         };

         if (Action(AC_Read, prv->FileStream, &read)) return ERR_Read;
         if (read.Result <= 0) return ERR_Read;

         prv->ReadPtr          = prv->OutputBuffer;
         prv->InputLength      = zf.CompressedSize - read.Result;
         prv->Stream.next_in   = prv->InputBuffer;
         prv->Stream.avail_in  = read.Result;
         prv->Stream.next_out  = prv->OutputBuffer;
         prv->Stream.avail_out = SIZE_COMPRESSION_BUFFER;
      }

      while (true) {
         // Output any buffered data to the client first
         if (prv->ReadPtr < (UBYTE *)prv->Stream.next_out) {
            LONG len = (LONG)(prv->Stream.next_out - (Bytef *)prv->ReadPtr);
            if (len > Args->Length) len = Args->Length;
            CopyMemory(prv->ReadPtr, (char *)Args->Buffer + Args->Result, len);
            prv->ReadPtr   += len;
            Args->Result   += len;
            Self->Position += len;
         }

         // Stop if necessary

         if (prv->Stream.total_out IS zf.OriginalSize) break; // All data decompressed
         if (Args->Result >= Args->Length) return ERR_Okay;
         if (!prv->Inflating) return ERR_Okay;

         // Reset the output buffer and decompress more data

         prv->Stream.next_out  = prv->OutputBuffer;
         prv->Stream.avail_out = SIZE_COMPRESSION_BUFFER;

         LONG result = inflate(&prv->Stream, (prv->Stream.avail_in) ? Z_SYNC_FLUSH : Z_FINISH);

         prv->ReadPtr = prv->OutputBuffer;

         if ((result) and (result != Z_STREAM_END)) {
            prv->InvalidState = true;
            return convert_zip_error(&prv->Stream, result);
         }

         // Read more data from the source if necessary

         if ((prv->Stream.avail_in <= 0) and (prv->InputLength > 0) and (result != Z_STREAM_END)) {
            struct acRead read = { .Buffer = prv->InputBuffer };

            if (prv->InputLength < SIZE_COMPRESSION_BUFFER) read.Length = prv->InputLength;
            else read.Length = SIZE_COMPRESSION_BUFFER;

            if (Action(AC_Read, prv->FileStream, &read)) return ERR_Read;
            if (read.Result <= 0) return ERR_Read;

            prv->InputLength -= read.Result;
            prv->Stream.next_in  = prv->InputBuffer;
            prv->Stream.avail_in = read.Result;
         }
      }

      if (prv->Inflating) {
         inflateEnd(&prv->Stream);
         prv->Inflating = false;
      }

      return ERR_Okay;
   }
}

//********************************************************************************************************************

static ERROR ARCHIVE_Seek(extFile *Self, struct acSeek *Args)
{
   Log log;
   LARGE pos;

   log.traceBranch("Seek to offset %.2f from seek position %d", Args->Offset, LONG(Args->Position));

   if (Args->Position IS SEEK::START) pos = F2T(Args->Offset);
   else if (Args->Position IS SEEK::END) pos = Self->Size - F2T(Args->Offset);
   else if (Args->Position IS SEEK::CURRENT) pos = Self->Position + F2T(Args->Offset);
   else return log.warning(ERR_Args);

   if (pos < 0) return log.warning(ERR_OutOfRange);

   if (pos < Self->Position) { // The position must be reset to zero if we need to backtrack
      reset_state(Self);

      ERROR error = seek_to_item(Self);
      if (error) return log.warning(error);
   }

   UBYTE buffer[2048];
   while (Self->Position < pos) {
      struct acRead read = { .Buffer = buffer, .Length = (LONG)(pos - Self->Position) };
      if ((size_t)read.Length > sizeof(buffer)) read.Length = sizeof(buffer);
      if (Action(AC_Read, Self, &read)) return ERR_Decompression;
   }

   return ERR_Okay;
}

//********************************************************************************************************************

static ERROR ARCHIVE_Write(extFile *Self, struct acWrite *Args)
{
   Log log;
   return log.warning(ERR_NoSupport);
}

//********************************************************************************************************************

static ERROR ARCHIVE_GET_Size(extFile *Self, LARGE *Value)
{
   auto prv = (prvFileArchive *)Self->ChildPrivate;
   if (prv) {
      *Value = prv->Info.OriginalSize;
      return ERR_Okay;
   }
   else return ERR_NotInitialised;
}

//********************************************************************************************************************
// Open the archive: volume for scanning.

static ERROR open_folder(DirInfo *Dir)
{
   std::string file_path;
   Dir->prvIndex = 0;
   Dir->prvTotal = 0;
   Dir->prvHandle = find_archive(Dir->prvResolvedPath, file_path);
   if (!Dir->prvHandle) return ERR_DoesNotExist;
   return ERR_Okay;
}

//********************************************************************************************************************
// Scan the next entry in the folder.

static ERROR scan_folder(DirInfo *Dir)
{
   Log log(__FUNCTION__);

   // Retrieve the file path, skipping the "archive:name/" part.

   CSTRING name = Dir->prvResolvedPath + LEN_ARCHIVE + 1;
   while ((*name) and (*name != '/') and (*name != '\\')) name++;
   if ((*name IS '/') or (*name IS '\\')) name++;

   log.traceBranch("Path: \"%s\", Flags: $%.8x", name, LONG(Dir->prvFlags));

   std::string path(name);

   auto archive = (extCompression *)Dir->prvHandle;

   auto it = archive->Files.begin();
   if (Dir->prvTotal) {
      it = ((ArchiveDriver *)Dir->Driver)->Index;
      it++;
   }

   for (; it != archive->Files.end(); it++) {
      ZipFile &zf = *it;

      if (!path.empty()) {
         if (StrCompare(path, zf.Name) != ERR_Okay) continue;
      }

      // Single folders will appear as 'ABCDEF/'
      // Single files will appear as 'ABCDEF.ABC' (no slash)

      if (zf.Name.size() <= path.size()) continue;

      // Is this item in a sub-folder?  If so, ignore it.

      {
         LONG i;
         for (i=path.size(); (zf.Name[i]) and (zf.Name[i] != '/') and (zf.Name[i] != '\\'); i++);
         if (zf.Name[i]) continue;
      }

      if (((Dir->prvFlags & RDF::FILE) != RDF::NIL) and (!zf.IsFolder)) {

         if ((Dir->prvFlags & RDF::PERMISSIONS) != RDF::NIL) {
            Dir->Info->Flags |= RDF::PERMISSIONS;
            Dir->Info->Permissions = PERMIT::READ|PERMIT::GROUP_READ|PERMIT::OTHERS_READ;
         }

         if ((Dir->prvFlags & RDF::SIZE) != RDF::NIL) {
            Dir->Info->Flags |= RDF::SIZE;
            Dir->Info->Size = zf.OriginalSize;
         }

         if ((Dir->prvFlags & RDF::DATE) != RDF::NIL) {
            Dir->Info->Flags |= RDF::DATE;
            Dir->Info->Modified.Year   = zf.Year;
            Dir->Info->Modified.Month  = zf.Month;
            Dir->Info->Modified.Day    = zf.Day;
            Dir->Info->Modified.Hour   = zf.Hour;
            Dir->Info->Modified.Minute = zf.Minute;
            Dir->Info->Modified.Second = 0;
         }

         Dir->Info->Flags |= RDF::FILE;
         auto offset = zf.Name.find_last_of("/\\");
         if (offset IS std::string::npos) offset = 0;
         StrCopy(zf.Name.c_str() + offset, Dir->Info->Name, MAX_FILENAME);

         ((ArchiveDriver *)Dir->Driver)->Index = it;
         Dir->prvTotal++;
         return ERR_Okay;
      }

      if (((Dir->prvFlags & RDF::FOLDER) != RDF::NIL) and (zf.IsFolder)) {
         Dir->Info->Flags |= RDF::FOLDER;

         auto offset = zf.Name.find_last_of("/\\");
         if (offset IS std::string::npos) offset = 0;
         LONG i = StrCopy(zf.Name.c_str() + offset, Dir->Info->Name, MAX_FILENAME-2);

         if ((Dir->prvFlags & RDF::QUALIFY) != RDF::NIL) {
            Dir->Info->Name[i++] = '/';
            Dir->Info->Name[i++] = 0;
         }

         if ((Dir->prvFlags & RDF::PERMISSIONS) != RDF::NIL) {
            Dir->Info->Flags |= RDF::PERMISSIONS;
            Dir->Info->Permissions = PERMIT::READ|PERMIT::GROUP_READ|PERMIT::OTHERS_READ;
         }

         ((ArchiveDriver *)Dir->Driver)->Index = it;
         Dir->prvTotal++;
         return ERR_Okay;
      }
   }

   return ERR_DirEmpty;
}

//********************************************************************************************************************

static ERROR close_folder(DirInfo *Dir)
{
   return ERR_Okay;
}

//********************************************************************************************************************

static ERROR get_info(CSTRING Path, FileInfo *Info, LONG InfoSize)
{
   Log log(__FUNCTION__);
   CompressedItem *item;
   std::string file_path;
   ERROR error;

   log.traceBranch("%s", Path);

   if (auto cmp = find_archive(Path, file_path)) {
      struct cmpFind find = { .Path=file_path.c_str(), .Flags=STR::CASE|STR::MATCH_LEN };
      if ((error = Action(MT_CmpFind, cmp, &find))) return error;
      item = find.Item;
   }
   else return ERR_DoesNotExist;

   Info->Size     = item->OriginalSize;
   Info->Flags    = RDF::NIL;
   Info->Created  = item->Created;
   Info->Modified = item->Modified;

   if ((item->Flags & FL::FOLDER) != FL::NIL) Info->Flags |= RDF::FOLDER;
   else Info->Flags |= RDF::FILE|RDF::SIZE;

   // Extract the file name

   LONG len = StrLength(Path);
   LONG i = len;
   if ((Path[i-1] IS '/') or (Path[i-1] IS '\\')) i--;
   while ((i > 0) and (Path[i-1] != '/') and (Path[i-1] != '\\') and (Path[i-1] != ':')) i--;
   i = StrCopy(Path + i, Info->Name, MAX_FILENAME-2);

   if ((Info->Flags & RDF::FOLDER) != RDF::NIL) {
      if (Info->Name[i-1] IS '\\') Info->Name[i-1] = '/';
      else if (Info->Name[i-1] != '/') {
         Info->Name[i++] = '/';
         Info->Name[i] = 0;
      }
   }

   Info->Permissions = item->Permissions;
   Info->UserID      = item->UserID;
   Info->GroupID     = item->GroupID;
   Info->Tags        = NULL;
   return ERR_Okay;
}

//********************************************************************************************************************
// Test an archive: location.

static ERROR test_path(STRING Path, RSF Flags, LOC *Type)
{
   Log log(__FUNCTION__);

   log.traceBranch("%s", Path);

   std::string file_path;
   extCompression *cmp;
   if (!(cmp = find_archive(Path, file_path))) return ERR_DoesNotExist;

   if (file_path.empty()) {
      *Type = LOC::VOLUME;
      return ERR_Okay;
   }

   CompressedItem *item;
   ERROR error = cmpFind(cmp, file_path.c_str(), STR::CASE|STR::MATCH_LEN, &item);

   if ((error) and ((Flags & RSF::APPROXIMATE) != RSF::NIL)) {
      file_path.append(".*");
      if (!(error = cmpFind(cmp, file_path.c_str(), STR::CASE|STR::WILDCARD, &item))) {
         // Point the path to the discovered item
         LONG i;
         for (i=0; (Path[i] != '/') and (Path[i]); i++);
         if (Path[i] IS '/') StrCopy(item->Path, Path + i + 1, MAX_FILENAME);
      }
   }

   if (error) {
      log.trace("cmpFind() did not find %s, %s", file_path.c_str(), GetErrorMsg(error));

      if (error IS ERR_Search) return ERR_DoesNotExist;
      else return error;
   }

   if ((item->Flags & FL::FOLDER) != FL::NIL) *Type = LOC::FOLDER;
   else *Type = LOC::FILE;

   return ERR_Okay;
}

//********************************************************************************************************************

static const ActionArray clArchiveActions[] = {
   { AC_Activate, ARCHIVE_Activate },
   { AC_Free,     ARCHIVE_Free },
   { AC_Init,     ARCHIVE_Init },
   { AC_Query,    ARCHIVE_Query },
   { AC_Read,     ARCHIVE_Read },
   { AC_Seek,     ARCHIVE_Seek },
   { AC_Write,    ARCHIVE_Write },
   { 0, NULL }
};

static const MethodEntry clArchiveMethods[] = {
   { 0, NULL, NULL, NULL, 0 }
};

static const struct FieldArray clArchiveFields[] = {
   { "Size", FDF_LARGE|FDF_R, ARCHIVE_GET_Size },
    END_FIELD
};

//********************************************************************************************************************

extern "C" ERROR add_archive_class(void)
{
   glArchiveClass = extMetaClass::create::global(
      fl::BaseClassID(ID_FILE),
      fl::ClassID(ID_FILEARCHIVE),
      fl::Name("FileArchive"),
      fl::Actions(clArchiveActions),
      fl::Methods(clArchiveMethods),
      fl::Fields(clArchiveFields),
      fl::Path("modules:core"));

   return glArchiveClass ? ERR_Okay : ERR_AddClass;
}

//********************************************************************************************************************

extern "C" ERROR create_archive_volume(void)
{
   return VirtualVolume("archive",
      VAS::DRIVER_SIZE, sizeof(ArchiveDriver),
      VAS::OPEN_DIR,    &open_folder,
      VAS::SCAN_DIR,    &scan_folder,
      VAS::CLOSE_DIR,   &close_folder,
      VAS::TEST_PATH,   &test_path,
      VAS::GET_INFO,    &get_info,
      0);
}
