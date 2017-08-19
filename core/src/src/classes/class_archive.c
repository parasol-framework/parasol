/*****************************************************************************

The source code of the Parasol project is made publicly available under the
terms described in the LICENSE.TXT file that is distributed with this package.
Please refer to it for further information on licensing.

******************************************************************************

-CLASS-
FileArchive: Creates simple read-only volumes backed by compressed archives.

The FileArchive class is an internal support class that makes it possible to create virtual file system volumes that
are based on compressed file archives.  There is no need for client programs to instantiate a FileArchive to make use
of this functionality.  Instead, create a @Compression object that declares the path of the source archive file and an
ArchiveName for reference.  In the example below, also take note of the use of NF_UNTRACKED to prevent the
@Compression object from being automatically collected when it goes out of scope:

<pre>
CreateObject(ID_COMPRESSION, NF_UNTRACKED, &archive,
   FID_Path|TSTR,        "user:documents/myfile.zip",
   FID_ArchiveName|TSTR, "myfiles",
   TAGEND);
</pre>

With the Compression object in place, opening files within the archive is as simple as using the correct path
reference.  The format is `archive:ArchiveName/path/to/file.ext` and the example below illustrates:

`obj.new('file', { path='archive:myfiles/readme.txt', flags='!READ' })`

-END-

*****************************************************************************/

#include "zlib.h"

#ifndef __system__
#define __system__
#endif

#define PRV_FILE
#define PRV_COMPRESSION

#include "../defs.h"
#include <parasol/main.h>

#define LEN_ARCHIVE 8 // "archive:" length

struct prvFileArchive {
   struct ZipFile Info;
   z_stream Stream;
   objFile *FileStream;
   objCompressedStream *CompressedStream;
   APTR OutputBuffer;
   UBYTE Inflating:1;
};

static const struct ActionArray clArchiveActions[];
static const struct MethodArray clArchiveMethods[];
static const struct FieldArray clArchiveFields[];

static objCompression *glArchives = NULL;

static ERROR close_folder(struct DirInfo *);
static ERROR open_folder(struct DirInfo *);
static ERROR get_info(CSTRING Path, struct FileInfo *, LONG InfoSize);
static ERROR scan_folder(struct DirInfo *);
static ERROR test_path(CSTRING, LONG, LONG *);
static ERROR convert_error(z_stream *Stream, LONG Result) __attribute__((unused));

static ERROR convert_error(z_stream *Stream, LONG Result)
{
   if (Stream->msg) LogErrorMsg("%s", Stream->msg);
   else LogErrorMsg("Zip error: %d", Result);

   switch(Result) {
      case Z_STREAM_ERROR:  return ERR_Failed;
      case Z_DATA_ERROR:    return ERR_InvalidData;
      case Z_MEM_ERROR:     return ERR_Memory;
      case Z_BUF_ERROR:     return ERR_BufferOverflow;
      case Z_VERSION_ERROR: return ERR_WrongVersion;
      default:              return ERR_Failed;
   }
}

//****************************************************************************
// Return the portion of the string that follows the last discovered '/' or '\'

INLINE CSTRING name_from_path(CSTRING Path)
{
   LONG i;
   for (i=0; Path[i]; i++) {
      if ((Path[i] IS '/') OR (Path[i] IS '\\')) {
         Path = Path + i + 1;
         i = -1;
      }
   }
   return Path;
}

//****************************************************************************

ERROR add_archive_class(void)
{
   return CreateObject(ID_METACLASS, 0, (OBJECTPTR *)&glArchiveClass,
      FID_BaseClassID|TLONG, ID_FILE,
      FID_SubClassID|TLONG,  ID_FILEARCHIVE,
      FID_Name|TSTRING,      "FileArchive",
      FID_Actions|TPTR,      clArchiveActions,
      FID_Methods|TARRAY,    clArchiveMethods,
      FID_Fields|TARRAY,     clArchiveFields,
      FID_Path|TSTR,         "modules:core",
      TAGEND);
}

//****************************************************************************

ERROR create_archive_volume(void)
{
   return VirtualVolume("archive",
      VAS_OPEN_DIR,  &open_folder,
      VAS_SCAN_DIR,  &scan_folder,
      VAS_CLOSE_DIR, &close_folder,
      VAS_TEST_PATH, &test_path,
      VAS_GET_INFO,  &get_info,
      0);
}

//****************************************************************************
// Insert a new compression object as an archive.

extern void add_archive(objCompression *Compression)
{
   if (glArchives) Compression->NextArchive = glArchives;

   glArchives = Compression;
}

//****************************************************************************

extern void remove_archive(objCompression *Compression)
{
   if (glArchives IS Compression) {
      glArchives = Compression->NextArchive;
   }
   else {
      objCompression *scan;
      for (scan=glArchives; scan; scan=scan->NextArchive) {
         if (scan->NextArchive IS Compression) {
            scan->NextArchive = Compression->NextArchive;
            break;
         }
      }
   }
}

//****************************************************************************
// Return the archive referenced by 'archive:[NAME]/...'

extern objCompression * find_archive(CSTRING Path, CSTRING *FilePath)
{
   if (!Path) return NULL;

   // Compute the hash of the referenced archive.

   CSTRING path = Path + LEN_ARCHIVE;
   ULONG hash = 5381;
   UBYTE c;
   while ((c = *path++) AND (c != '/') AND (c != '\\')) {
      if ((c >= 'A') AND (c <= 'Z')) hash = (hash<<5) + hash + c - 'A' + 'a';
      else hash = (hash<<5) + hash + c;
   }

   if (FilePath) {
      if (c) *FilePath = path;
      else *FilePath = NULL;
   }

   // Find the compression object with the referenced hash.

   objCompression *cmp = glArchives;
   while (cmp) {
      if (cmp->ArchiveHash IS hash) {
         FMSG("find_archive","Found matching archive for %s", Path);
         return cmp;
      }
      cmp = cmp->NextArchive;
   }

   FMSG("@find_archive","No match for path %s", Path);
   return NULL;
}

//****************************************************************************

static ERROR ARCHIVE_Activate(objFile *Self, APTR Void)
{
   MSG("Activating archive object...");

   struct prvFileArchive *prv = Self->Head.ChildPrivate;

   CSTRING file_path;
   objCompression *cmp = find_archive(Self->Path, &file_path);

   if (!cmp) return PostError(ERR_Search);

   ERROR error;
   if (!prv->FileStream) {
      if (!CreateObject(ID_FILE, NF_INTEGRAL, (OBJECTPTR *)&prv->FileStream,
            FID_Location|TSTR, cmp->Location,
            FID_Flags|TLONG,   FL_READ,
            TAGEND)) {

         struct ZipFile *item = cmp->prvFiles;
         while (item) {
            if (!StrCompare(file_path, item->Name, 0, STR_WILDCARD)) break;
            else item = (struct ZipFile *)item->Next;
         }

         acSeekStart(prv->FileStream, item->Offset + HEAD_EXTRALEN);

         UWORD extralen = read_word(prv->FileStream);
         ULONG stream_start = item->Offset + HEAD_LENGTH + item->NameLen + extralen;
         if (acSeekStart(prv->FileStream, stream_start) != ERR_Okay) {
            error = PostError(ERR_Seek);
         }
         else if (item->CompressedSize > 0) {
            Self->Flags |= FL_FILE;

            if (item->DeflateMethod IS 0) { // The file is stored rather than compressed
               Self->Size = item->CompressedSize;
               error = ERR_Okay;
            }
            else if ((item->DeflateMethod IS 8) AND (!inflateInit2(&prv->Stream, -MAX_WBITS))) {
               prv->Inflating = TRUE;
               error = ERR_Okay;
            }
            else error = ERR_Failed;
         }
         else { // Folder or empty file
            if (item->IsFolder) Self->Flags |= FL_FOLDER;
            else Self->Flags |= FL_FILE;
            error = ERR_Okay;
         }
      }
      else error = ERR_File;
   }
   else error = ERR_Okay;

   return error;
}

//****************************************************************************

static ERROR ARCHIVE_Free(objFile *Self, APTR Void)
{
   struct prvFileArchive *prv = Self->Head.ChildPrivate;

   if (prv) {
      if (prv->FileStream) { acFree(&prv->FileStream->Head); prv->FileStream = NULL; }
      if (prv->CompressedStream) { acFree(&prv->CompressedStream->Head); prv->CompressedStream = NULL; }
      if (prv->OutputBuffer) { FreeMemory(prv->OutputBuffer); prv->OutputBuffer = NULL; }
   }

   return ERR_Okay;
}

//****************************************************************************

static ERROR ARCHIVE_Init(objFile *Self, APTR Void)
{
   if (!Self->Path) return ERR_FieldNotSet;

   if (StrCompare("archive:", Self->Path, LEN_ARCHIVE, 0) != ERR_Okay) return ERR_NoSupport;

   if (Self->Flags & (FL_NEW|FL_WRITE)) return PostError(ERR_ReadOnly);

   ERROR error = ERR_Failed;
   if (!AllocMemory(sizeof(struct prvFileArchive), Self->Head.MemFlags, &Self->Head.ChildPrivate, NULL)) {
      LONG len;
      for (len=0; Self->Path[len]; len++);

      if (Self->Path[len-1] IS ':') { // Nothing is referenced
         return ERR_Okay;
      }
      else {
         CSTRING file_path;
         objCompression *cmp = find_archive(Self->Path, &file_path);

         if (cmp) {
            struct ZipFile *item = cmp->prvFiles;
            while (item) {
               if (!StrCompare(file_path, item->Name, 0, STR_WILDCARD)) break;
               else item = (struct ZipFile *)item->Next;
            }

            if (item) {
               ((struct prvFileArchive *)(Self->Head.ChildPrivate))->Info = *item;

               error = acActivate((OBJECTPTR)Self);

               if (!error) error = acQuery((OBJECTPTR)Self);
            }
            else error = ERR_Search;
         }
         else error = ERR_Search;
      }
   }
   else error = ERR_AllocMemory;

   if (error) {
      if (Self->Head.ChildPrivate) { FreeMemory(Self->Head.ChildPrivate); Self->Head.ChildPrivate = NULL; }
   }

   return error;
}

//****************************************************************************

static ERROR ARCHIVE_Query(objFile *Self, APTR Void)
{
   struct prvFileArchive *prv = (struct prvFileArchive *)(Self->Head.ChildPrivate);

   // Activate the source if this hasn't been done already.

   ERROR error;
   if (!prv->FileStream) {
      error = acActivate((OBJECTPTR)Self);
      if (error) return error;
   }

   struct ZipFile *item = &prv->Info;

   // If security flags are present, convert them to file system permissions.

   if (item->Flags & ZIP_SECURITY) {
      LONG permissions = 0;
      if (item->Flags & ZIP_UEXEC) permissions |= PERMIT_USER_EXEC;
      if (item->Flags & ZIP_GEXEC) permissions |= PERMIT_GROUP_EXEC;
      if (item->Flags & ZIP_OEXEC) permissions |= PERMIT_OTHERS_EXEC;

      if (item->Flags & ZIP_UREAD) permissions |= PERMIT_USER_READ;
      if (item->Flags & ZIP_GREAD) permissions |= PERMIT_GROUP_READ;
      if (item->Flags & ZIP_OREAD) permissions |= PERMIT_OTHERS_READ;

      if (item->Flags & ZIP_UWRITE) permissions |= PERMIT_USER_WRITE;
      if (item->Flags & ZIP_GWRITE) permissions |= PERMIT_GROUP_WRITE;
      if (item->Flags & ZIP_OWRITE) permissions |= PERMIT_OTHERS_WRITE;

      Self->Permissions = permissions;
   }

   return ERR_Okay;
}

//****************************************************************************

#define MIN_OUTPUT_SIZE ((32 * 1024) + 2048)

static ERROR ARCHIVE_Read(objFile *Self, struct acRead *Args)
{
   if ((!Args) OR (!Args->Buffer)) return PostError(ERR_NullArgs);
   else if (Args->Length == 0) return ERR_Okay;
   else if (Args->Length < 0) return ERR_OutOfRange;

   struct prvFileArchive *prv = Self->Head.ChildPrivate;

   if (prv->Info.DeflateMethod IS 0) {
      ERROR error = acRead(prv->FileStream, Args->Buffer, Args->Length, &Args->Result);
      if (!error) Self->Position += Args->Result;
      return error;
   }
   else {
      LONG inputsize = prv->Info.CompressedSize < 1024 ? prv->Info.CompressedSize : 1024;
      UBYTE inputstream[inputsize];
      LONG length;

      if (!prv->Inflating) {
         Args->Result = 0;
         return ERR_Okay;
      }

      // Output preparation

      Args->Result = 0;
      APTR output = Args->Buffer;
      LONG outputsize = Args->Length;
      if (outputsize < MIN_OUTPUT_SIZE) {
         // An internal buffer will need to be allocated if the one supplied to Read() is not large enough.
         outputsize = MIN_OUTPUT_SIZE;
         if (!(output = prv->OutputBuffer)) {
            if (AllocMemory(MIN_OUTPUT_SIZE, MEM_DATA|MEM_NO_CLEAR, &prv->OutputBuffer, NULL)) return ERR_AllocMemory;
            output = prv->OutputBuffer;
         }
      }

      // The outer loop tries to fill the entirety of the client buffer.

      LONG out_offset = 0;
      ERROR error = ERR_Okay;
      while ((Args->Result < prv->Info.OriginalSize) AND (Args->Result < Args->Length)) {

         if (acRead(prv->FileStream, inputstream, inputsize, &length)) return PostError(ERR_Read);

         if (length <= 0) return ERR_Okay;

         // Inner loop processes all of the read data.

         prv->Stream.next_in  = inputstream;
         prv->Stream.avail_in = length;

         LONG result = Z_OK;
         while ((result IS Z_OK) AND (prv->Stream.avail_in > 0) AND (out_offset < outputsize)) {
            prv->Stream.next_out  = output + out_offset;
            prv->Stream.avail_out = outputsize;
            result = inflate(&prv->Stream, Z_SYNC_FLUSH);

            if ((result) AND (result != Z_STREAM_END)) {
               error = convert_error(&prv->Stream, result);
               break;
            }

            LONG total_output = outputsize - prv->Stream.avail_out;

            if (output IS Args->Buffer) { // Decompression is direct to the client buffer.
               out_offset += total_output;
            }
            else { // Decompression is to the temporary buffer.
               CopyMemory(output, Args->Buffer + Args->Result, total_output);
            }

            Args->Result += total_output;

            if (result IS Z_STREAM_END) {
               // Decompression is complete
               MSG("Decompression complete.  Output %d bytes.", out_offset);
               prv->Inflating = FALSE;
               error = ERR_Okay;
               break;
            }
         }
      }

      return error;
   }
}

//****************************************************************************

static ERROR ARCHIVE_Seek(objFile *Self, struct acSeek *Args)
{
   LONG pos;
   if (Args->Position IS SEEK_START) pos = Args->Offset;
   else if (Args->Position IS SEEK_END) pos = Self->Size - Args->Offset;
   else if (Args->Position IS SEEK_CURRENT) pos = Self->Position + Args->Offset;
   else return PostError(ERR_Args);

   struct prvFileArchive *prv = Self->Head.ChildPrivate;
   ERROR error = acSeek(prv->CompressedStream, Args->Position, Args->Offset);
   if (!error) {
      if (pos < 0) pos = 0;
      else if (pos > Self->Size) pos = Self->Size;
      Self->Position = pos;
   }

   return error;
}

//****************************************************************************

static ERROR ARCHIVE_Write(objFile *Self, struct acWrite *Args)
{
   return PostError(ERR_NoSupport);
}

//****************************************************************************

static ERROR ARCHIVE_GET_Size(objFile *Self, LARGE *Value)
{
   struct prvFileArchive *prv = Self->Head.ChildPrivate;
   if (prv) {
      *Value = prv->Info.OriginalSize;
      return ERR_Okay;
   }
   else return ERR_NotInitialised;
}

//****************************************************************************
// Open the archive: volume for scanning.

static ERROR open_folder(struct DirInfo *Dir)
{
   Dir->prvIndex = 0;
   Dir->prvTotal = 0;
   Dir->prvHandle = find_archive(Dir->prvResolvedPath, NULL);
   if (!Dir->prvHandle) return ERR_DoesNotExist;
   return ERR_Okay;
}

//****************************************************************************
// Scan the next entry in the folder.

static ERROR scan_folder(struct DirInfo *Dir)
{
   // Retrieve the file path, skipping the "archive:name/" part.

   CSTRING path = Dir->prvResolvedPath + LEN_ARCHIVE + 1;
   while ((*path) AND (*path != '/') AND (*path != '\\')) path++;
   if ((*path IS '/') OR (*path IS '\\')) path++;

   FMSG("~scan_folder()","Path: \"%s\", Flags: $%.8x", path, Dir->prvFlags);

   objCompression *archive = Dir->prvHandle;

   struct ZipFile *zf = archive->prvFiles;
   if (Dir->prvIndexPtr) zf = Dir->prvIndexPtr;

   for (; zf; zf = (struct ZipFile *)zf->Next) {
      if (*path) {
         if (StrCompare(path, zf->Name, 0, 0) != ERR_Okay) continue;
      }

      FMSG("scan_folder:", "%s: %s, $%.8x", path, zf->Name, zf->Flags);

      // Single folders will appear as 'ABCDEF/'
      // Single files will appear as 'ABCDEF.ABC' (no slash)

      LONG name_len = StrLength(zf->Name);
      LONG path_len = StrLength(path);

      if (name_len <= path_len) continue;

      // Is this item in a sub-folder?  If so, ignore it.

      {
         LONG i;
         for (i=path_len; (zf->Name[i]) AND (zf->Name[i] != '/') AND (zf->Name[i] != '\\'); i++);
         if (zf->Name[i]) continue;
      }

      if ((Dir->prvFlags & RDF_FILE) AND (!zf->IsFolder)) {

         if (Dir->prvFlags & RDF_PERMISSIONS) {
            Dir->Info->Flags |= RDF_PERMISSIONS;
            Dir->Info->Permissions = PERMIT_READ|PERMIT_GROUP_READ|PERMIT_OTHERS_READ;
         }

         if (Dir->prvFlags & RDF_SIZE) {
            Dir->Info->Flags |= RDF_SIZE;
            Dir->Info->Size = zf->OriginalSize;
         }

         if (Dir->prvFlags & RDF_DATE) {
            Dir->Info->Flags |= RDF_DATE;
            Dir->Info->Modified.Year   = zf->Year;
            Dir->Info->Modified.Month  = zf->Month;
            Dir->Info->Modified.Day    = zf->Day;
            Dir->Info->Modified.Hour   = zf->Hour;
            Dir->Info->Modified.Minute = zf->Minute;
            Dir->Info->Modified.Second = 0;
         }

         Dir->Info->Flags |= RDF_FILE;
         StrCopy(name_from_path(zf->Name), Dir->Info->Name, MAX_FILENAME);

         Dir->prvIndexPtr = zf->Next;
         Dir->prvTotal++;
         STEP();
         return ERR_Okay;
      }

      if ((Dir->prvFlags & RDF_FOLDER) AND (zf->IsFolder)) {
         Dir->Info->Flags |= RDF_FOLDER;

         LONG i = StrCopy(name_from_path(zf->Name), Dir->Info->Name, MAX_FILENAME-2);

         if (Dir->prvFlags & RDF_QUALIFY) {
            Dir->Info->Name[i++] = '/';
            Dir->Info->Name[i++] = 0;
         }

         if (Dir->prvFlags & RDF_PERMISSIONS) {
            Dir->Info->Flags |= RDF_PERMISSIONS;
            Dir->Info->Permissions = PERMIT_READ|PERMIT_GROUP_READ|PERMIT_OTHERS_READ;
         }

         Dir->prvIndexPtr = zf->Next;
         Dir->prvTotal++;
         STEP();
         return ERR_Okay;
      }
   }

   STEP();
   return ERR_DirEmpty;
}

//****************************************************************************

static ERROR close_folder(struct DirInfo *Dir)
{
   return ERR_Okay;
}

//****************************************************************************

static ERROR get_info(CSTRING Path, struct FileInfo *Info, LONG InfoSize)
{
   struct CompressedItem *item;
   objCompression *cmp;
   CSTRING file_path;
   ERROR error;

   if ((cmp = find_archive(Path, &file_path))) {
      struct cmpFind find = { .Path=file_path, .Flags=STR_CASE|STR_MATCH_LEN };
      if ((error = Action(MT_CmpFind, &cmp->Head, &find))) {
         return error;
      }
      item = find.Item;
   }
   else return ERR_DoesNotExist;

   Info->Size = item->OriginalSize;
   Info->Flags = 0;
   Info->Created = item->Created;
   Info->Modified = item->Modified;

   if (item->Flags & FL_FOLDER) Info->Flags |= RDF_FOLDER;
   else Info->Flags |= RDF_FILE|RDF_SIZE;

   // Extract the file name

   LONG len = StrLength(Path);
   LONG i = len;
   if ((Path[i-1] IS '/') OR (Path[i-1] IS '\\')) i--;
   while ((i > 0) AND (Path[i-1] != '/') AND (Path[i-1] != '\\') AND (Path[i-1] != ':')) i--;
   i = StrCopy(Path + i, Info->Name, MAX_FILENAME-2);

   if (Info->Flags & RDF_FOLDER) {
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

//****************************************************************************
// Test an archive: location.

static ERROR test_path(CSTRING Path, LONG Flags, LONG *Type)
{
   FMSG("~test_path","%s", Path);

   CSTRING file_path;
   objCompression *cmp;
   if (!(cmp = find_archive(Path, &file_path))) {
      STEP();
      return ERR_DoesNotExist;
   }

   if ((!file_path) OR (!file_path[0])) {
      *Type = LOC_VOLUME;
      STEP();
      return ERR_Okay;
   }

   struct CompressedItem *item;
   ERROR error;
   if ((error = cmpFind(cmp, file_path, STR_CASE|STR_MATCH_LEN, &item))) {
      FMSG("test_path","cmpFind() did not find %s, %s", file_path, GetErrorMsg(error));
      STEP();
      if (error IS ERR_Search) return ERR_DoesNotExist;
      else return error;
   }

   if (item->Flags & FL_FOLDER) *Type = LOC_FOLDER;
   else *Type = LOC_FILE;

   STEP();
   return ERR_Okay;
}

//****************************************************************************

static const struct ActionArray clArchiveActions[] = {
   { AC_Activate, ARCHIVE_Activate },
   { AC_Free,     ARCHIVE_Free },
   { AC_Init,     ARCHIVE_Init },
   { AC_Query,    ARCHIVE_Query },
   { AC_Read,     ARCHIVE_Read },
   { AC_Seek,     ARCHIVE_Seek },
   { AC_Write,    ARCHIVE_Write },
   { 0, NULL }
};

static const struct MethodArray clArchiveMethods[] = {
   { 0, NULL, NULL, NULL, 0 }
};

static const struct FieldArray clArchiveFields[] = {
   { "Size", FDF_LARGE|FDF_R, 0, ARCHIVE_GET_Size, NULL },
    END_FIELD
};
