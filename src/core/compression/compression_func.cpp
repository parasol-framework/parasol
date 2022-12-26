
//*********************************************************************************************************************

static void print(extCompression *Self, CSTRING Buffer)
{
   parasol::Log log;

   if (Self->OutputID) {
      struct acDataFeed feed = {
         .ObjectID = Self->UID,
         .DataType = DATA_TEXT,
         .Buffer   = Buffer
      };
      feed.Size = StrLength(Buffer) + 1;
      ActionMsg(AC_DataFeed, Self->OutputID, &feed, 0, 0);
   }
   else log.msg("%s", Buffer);
}

//*********************************************************************************************************************

static ERROR compress_folder(extCompression *Self, CSTRING Location, CSTRING Path)
{
   parasol::Log log(__FUNCTION__);

   log.branch("Compressing folder \"%s\" to \"%s\"", Location, Path);

   if ((!Self) or (!Location)) return ERR_NullArgs;
   if (!Path) Path = "";

   objFile *file;
   if (CreateObject(ID_FILE, NF_INTEGRAL, (OBJECTPTR *)&file, FID_Path|TSTR, Location, TAGEND) != ERR_Okay) {
      return log.warning(ERR_File);
   }

   if ((file->Flags & FL_LINK) and (!(Self->Flags & CMF_NO_LINKS))) {
      log.msg("Folder is a link.");
      acFree(file);
      return compress_file(Self, Location, Path, TRUE);
   }

   if (Self->OutputID) {
      StrFormat((char *)Self->prvOutput, SIZE_COMPRESSION_BUFFER, "  Compressing folder \"%s\".", Location);
      print(Self, (CSTRING)Self->prvOutput);
   }

   // Send feedback if requested to do so

   CompressionFeedback feedback = {
      .FeedbackID     = FDB_COMPRESS_FILE,
      .Index          = Self->prvFileIndex,
      .Path           = Location,
      .Dest           = Path,
      .Progress       = 0,
      .OriginalSize   = 0,
      .CompressedSize = 0
   };

   ERROR error = send_feedback(Self, &feedback);

   Self->prvFileIndex++;
   if ((error IS ERR_Terminate) or (error IS ERR_Cancelled)) {
      acFree(file);
      return ERR_Cancelled;
   }
   else if (error IS ERR_Skip) {
      acFree(file);
      return ERR_Okay;
   }
   else error = ERR_Okay;

   // Clear default variables

   ZipFile *fileexists = NULL;
   ZipFile *entry      = NULL;

   LONG i, j, len;
   LONG pathlen = StrLength(Path);

   error = ERR_Failed;
   if (pathlen > 0) {
      // Seek to the position at which this new directory entry will be added

      ZipFile *chain;
      ULONG dataoffset = 0;
      if ((chain = Self->prvFiles)) {
         while (chain->Next) chain = (ZipFile *)chain->Next;
         if (acSeekStart(Self->FileIO, chain->Offset + HEAD_NAMELEN) != ERR_Okay) {
            acFree(file);
            return log.warning(ERR_Seek);
         }
         UWORD namelen    = read_word(Self->FileIO);
         UWORD extralen   = read_word(Self->FileIO);
         dataoffset = chain->Offset + HEAD_LENGTH + namelen + extralen + chain->CompressedSize;
      }

      if (acSeekStart(Self->FileIO, dataoffset) != ERR_Okay) goto exit;

      // If a matching file name already exists in the archive, make a note of its position

      for (fileexists=Self->prvFiles; fileexists; fileexists=(ZipFile *)fileexists->Next) {
         if (!StrMatch(fileexists->Name, Location)) break;
      }

      // Allocate the file entry structure and set up some initial variables.

      if (AllocMemory(sizeof(ZipFile) + pathlen + 1, MEM_DATA, (APTR *)&entry, NULL) != ERR_Okay) {
         acFree(file);
         return ERR_AllocMemory;
      }

      entry->Name = (STRING)(entry+1);
      entry->NameLen = pathlen;
      for (i=0; i < pathlen; i++) entry->Name[i] = Path[i];
      entry->Name[i] = 0;
      entry->CRC        = 0;
      entry->Offset     = dataoffset;
      entry->CompressedSize = 0;
      entry->OriginalSize   = 0;
      entry->DeflateMethod  = 0;

      // Convert the file date stamp into a DOS time stamp for zip

      DateTime *tm;
      if (!file->getPtr(FID_Date, &tm)) {
         if (tm->Year < 1980) entry->TimeStamp = 0x00210000;
         else entry->TimeStamp = ((tm->Year-1980)<<25) | (tm->Month<<21) | (tm->Day<<16) | (tm->Hour<<11) | (tm->Minute<<5) | (tm->Second>>1);
      }

      // Write the compression file entry

      if (acSeekStart(Self->FileIO, entry->Offset) != ERR_Okay) goto exit;

      UBYTE header[sizeof(glHeader)];
      CopyMemory(glHeader, header, sizeof(glHeader));

      wrb_word(entry->DeflateMethod, header + HEAD_DEFLATEMETHOD);
      wrb_long(entry->TimeStamp, header + HEAD_TIMESTAMP);
      wrb_long(entry->CRC, header + HEAD_CRC);
      wrb_long(entry->CompressedSize, header + HEAD_COMPRESSEDSIZE);
      wrb_long(entry->OriginalSize, header + HEAD_FILESIZE);
      wrb_word(entry->NameLen, header + HEAD_NAMELEN);
      if (acWriteResult(Self->FileIO, header, HEAD_LENGTH) != HEAD_LENGTH) goto exit;
      if (acWriteResult(Self->FileIO, entry->Name, entry->NameLen) != entry->NameLen) goto exit;

      // Add the entry to the file chain

      if ((chain = Self->prvFiles)) {
         while (chain->Next) chain = (ZipFile *)chain->Next;
         entry->Prev = (CompressedFile *)chain;
         chain->Next = (CompressedFile *)entry;
      }
      else Self->prvFiles = entry;

      // If this new data replaces an existing directory, remove the old directory now

      if (fileexists) remove_file(Self, &fileexists);

      Self->prvCompressionCount++;
   }

   // Enter the directory and compress its contents

   DirInfo *dir;
   if (!OpenDir(Location, RDF_FILE|RDF_FOLDER|RDF_QUALIFY, &dir)) {
      len = StrLength(Location);
      pathlen = StrLength(Path);
      while (!ScanDir(dir)) { // Recurse for each directory in the list
         FileInfo *scan = dir->Info;
         if ((scan->Flags & RDF_FOLDER) and (!(scan->Flags & RDF_LINK))) {
            j = StrLength(scan->Name);
            char location[len+j+1];
            char path[pathlen+j+1];
            StrFormat(location, sizeof(location), "%s%s", Location, scan->Name);
            StrFormat(path, sizeof(path), "%s%s", Path, scan->Name);
            compress_folder(Self, location, path);
         }
         else if (scan->Flags & (RDF_FILE|RDF_LINK)) {
            char location[len+StrLength(scan->Name)+1];
            j = StrCopy(Location, location, sizeof(location));
            StrCopy(scan->Name, location + j, sizeof(location) - j);
            compress_file(Self, location, Path, (scan->Flags & RDF_LINK) ? TRUE : FALSE);
         }
      }

      FreeResource(dir);
   }

   acFree(file);
   return ERR_Okay;

exit:
   if (entry) {
      FreeFromLL((CompressedFile *)entry, (CompressedFile *)Self->prvFiles, (CompressedFile **)&Self->prvFiles);
      FreeResource(entry);
   }

   acFree(file);
   return error;
}

//*********************************************************************************************************************

static ERROR compress_file(extCompression *Self, CSTRING Location, CSTRING Path, BYTE Link)
{
   parasol::Log log(__FUNCTION__);

   if ((!Self) or (!Location) or (!Path)) return ERR_NullArgs;

   log.branch("Compressing file \"%s\" to \"%s\"", Location, Path);

   ZipFile *fileexists = NULL;
   ZipFile *entry = NULL;
   STRING symlink = NULL;
   UBYTE deflateend = FALSE;
   ERROR error = ERR_Failed;
   objFile *file = NULL;
   ZipFile *chain;
   ULONG dataoffset = 0;
   char filename[512] = "";
   LONG i, len;
   WORD level = Self->CompressionLevel / 10;
   if (level < 0) level = 0;
   else if (level > 9) level = 9;

   // Open the source file for reading only

   if (CreateObject(ID_FILE, NF_INTEGRAL, (OBJECTPTR *)&file,
         FID_Path|TSTR,   Location,
         FID_Flags|TLONG, (Link IS TRUE) ? 0 : FL_READ,
         TAGEND) != ERR_Okay) {
      if (Self->OutputID) {
         StrFormat((char *)Self->prvOutput, SIZE_COMPRESSION_BUFFER, "  Error opening file \"%s\".", Location);
         print(Self, (CSTRING)Self->prvOutput);
      }
      error = log.warning(ERR_OpenFile);
      goto exit;
   }

   if ((Link) and (!(file->Flags & FL_LINK))) {
      log.warning("Internal Error: Expected a link, but the file is not.");
      error = ERR_Failed;
      goto exit;
   }

   // Determine the name that will be used for storing this file

   for (i=0; Location[i]; i++);
   if ((i > 0) and ((Location[i-1] IS '/') or (Location[i-1] IS '\\'))) i--; // Ignore trailing slashes for symbolically linked folders
   while ((i > 0) and (Location[i-1] != ':') and (Location[i-1] != '/') and (Location[i-1] != '\\')) i--;
   if (Path) {
      len = StrCopy(Path, filename, sizeof(filename));
      len += StrCopy(Location+i, filename+len, sizeof(filename)-len);
   }
   else len = StrCopy(Location+i, filename, sizeof(filename));

   if (Link) {
      if (filename[len-1] IS '/') filename[len-1] = 0;
   }

   // Send feedback

   CompressionFeedback fb;
   fb.FeedbackID     = FDB_COMPRESS_FILE;
   fb.Index          = Self->prvFileIndex;
   fb.Path           = Location;
   fb.Dest           = filename;
   file->get(FID_Size, &fb.OriginalSize);
   fb.CompressedSize = 0;
   fb.Progress       = 0;
   error = send_feedback(Self, &fb);

   if ((error IS ERR_Terminate) or (error IS ERR_Cancelled)) { error = ERR_Cancelled; goto exit; }
   else if (error IS ERR_Skip) { error = ERR_Okay; goto exit; }
   else error = ERR_Okay;

   // Send informative output to the user

   if (Self->OutputID) {
      StrFormat((char *)Self->prvOutput, SIZE_COMPRESSION_BUFFER, "  Compressing file \"%s\".", Location);
      print(Self, (CSTRING)Self->prvOutput);
   }

   // Seek to the position at which this new file will be added

   if ((chain = Self->prvFiles)) {
      while (chain->Next) chain = (ZipFile *)chain->Next;
      if (acSeek(Self->FileIO, chain->Offset + HEAD_NAMELEN, SEEK_START) != ERR_Okay) return log.warning(ERR_Seek);
      UWORD namelen    = read_word(Self->FileIO);
      UWORD extralen   = read_word(Self->FileIO);
      dataoffset = chain->Offset + HEAD_LENGTH + namelen + extralen + chain->CompressedSize;
   }

   if (acSeek(Self->FileIO, dataoffset, SEEK_START) != ERR_Okay) goto exit;

   // Initialise the compression algorithm

   Self->prvCompressionCount++;

   Self->prvZip.next_in   = 0;
   Self->prvZip.avail_in  = 0;
   Self->prvZip.next_out  = 0;
   Self->prvZip.avail_out = 0;
   Self->prvZip.total_in  = 0;
   Self->prvZip.total_out = 0;

   if (deflateInit2(&Self->prvZip, level, Z_DEFLATED, -MAX_WBITS, ZLIB_MEM_LEVEL, Z_DEFAULT_STRATEGY) IS ERR_Okay) {
      deflateend = TRUE;
      Self->prvZip.next_out  = Self->prvOutput;
      Self->prvZip.avail_out = SIZE_COMPRESSION_BUFFER;
   }
   else {
      error = ERR_Failed;
      goto exit;
   }

   // If a matching file name already exists in the archive, make a note of its position

   for (fileexists=Self->prvFiles; fileexists; fileexists=(ZipFile *)fileexists->Next) {
      if (!StrCompare(fileexists->Name, filename, 0, STR_MATCH_LEN)) break;
   }

   // Allocate the file entry structure and set up some initial variables.

   len = StrLength(filename);
   if (!AllocMemory(sizeof(ZipFile) + len + 1, MEM_DATA, (APTR *)&entry, NULL)) {
      entry->Name = (STRING)(entry+1);
      entry->NameLen = len;
      for (i=0; i < entry->NameLen; i++) entry->Name[i] = filename[i];
      entry->Name[i] = 0;
      entry->CRC        = 0;
      entry->Offset     = dataoffset;
      entry->Comment    = 0;
      entry->CommentLen = 0;

      if ((!(Self->Flags & CMF_NO_LINKS)) and (file->Flags & FL_LINK)) {
         if (!file->get(FID_Link, &symlink)) {
            log.msg("Note: File \"%s\" is a symbolic link to \"%s\"", filename, symlink);
            entry->Flags |= ZIP_LINK;
         }
      }

      // Convert the file date stamp into a DOS time stamp for zip

      DateTime *time;
      if (!file->getPtr(FID_Date, &time)) {
         if (time->Year < 1980) entry->TimeStamp = 0x00210000;
         else entry->TimeStamp = ((time->Year-1980)<<25) | (time->Month<<21) | (time->Day<<16) | (time->Hour<<11) | (time->Minute<<5) | (time->Second>>1);
      }
      else entry->TimeStamp = 0;
   }
   else {
      error = ERR_AllocMemory;
      goto exit;
   }

   LONG permissions;
   if (!file->get(FID_Permissions, &permissions)) {
      if (permissions & PERMIT_USER_READ) entry->Flags |= ZIP_UREAD;
      if (permissions & PERMIT_GROUP_READ) entry->Flags |= ZIP_GREAD;
      if (permissions & PERMIT_OTHERS_READ) entry->Flags |= ZIP_OREAD;

      if (permissions & PERMIT_USER_WRITE) entry->Flags |= ZIP_UWRITE;
      if (permissions & PERMIT_GROUP_WRITE) entry->Flags |= ZIP_GWRITE;
      if (permissions & PERMIT_OTHERS_WRITE) entry->Flags |= ZIP_OWRITE;

      if (permissions & PERMIT_USER_EXEC) entry->Flags |= ZIP_UEXEC;
      if (permissions & PERMIT_GROUP_EXEC) entry->Flags |= ZIP_GEXEC;
      if (permissions & PERMIT_OTHERS_EXEC) entry->Flags |= ZIP_OEXEC;
   }

   entry->Flags &= 0xffffff00; // Do not write anything to the low order bits, they have meaning exclusive to MSDOS

   // Skip over the PKZIP header that will be written for this file (we will be updating the header later).

   if (acWriteResult(Self->FileIO, NULL, HEAD_LENGTH + entry->NameLen + entry->CommentLen) != HEAD_LENGTH + entry->NameLen + entry->CommentLen) goto exit;

   // Specify the limitations of our buffer so that the compression routine doesn't overwrite its boundaries.  Then
   // start the compression of the input file.

   if (entry->Flags & ZIP_LINK) {
      // Compress the symbolic link to the zip file, rather than the data
      len = StrLength(symlink);
      Self->prvZip.next_in   = (Bytef *)symlink;
      Self->prvZip.avail_in  = len;
      Self->prvZip.next_out  = Self->prvOutput;
      Self->prvZip.avail_out = SIZE_COMPRESSION_BUFFER;
      if (deflate(&Self->prvZip, Z_NO_FLUSH) != ERR_Okay) {
         log.warning("Failure during data compression.");
         goto exit;
      }
      entry->CRC = GenCRC32(entry->CRC, symlink, len);
   }
   else {
      struct acRead read = { .Buffer = Self->prvInput, .Length = SIZE_COMPRESSION_BUFFER };
      while ((!Action(AC_Read, file, &read)) and (read.Result > 0)) {
         Self->prvZip.next_in  = Self->prvInput;
         Self->prvZip.avail_in = read.Result;

         while (Self->prvZip.avail_in) {
            if (!Self->prvZip.avail_out) {
               // Write out the compression buffer because it is at capacity
               struct acWrite write = { .Buffer = Self->prvOutput, .Length = SIZE_COMPRESSION_BUFFER };
               Action(AC_Write, Self->FileIO, &write);

               // Reset the compression buffer
               Self->prvZip.next_out  = Self->prvOutput;
               Self->prvZip.avail_out = SIZE_COMPRESSION_BUFFER;

               fb.CompressedSize = Self->prvZip.total_out;
               fb.Progress       = Self->prvZip.total_in; //fb.CompressedSize * 100 / fb.OriginalSize;
               send_feedback(Self, &fb);
            }

            if (deflate(&Self->prvZip, Z_NO_FLUSH) != ERR_Okay) {
               log.warning("Failure during data compression.");
               goto exit;
            }
         }

         entry->CRC = GenCRC32(entry->CRC, Self->prvInput, read.Result);
      }
   }

   if (acFlush(Self) != ERR_Okay) goto exit;
   deflateEnd(&Self->prvZip);
   deflateend = FALSE;

   // Finalise entry details

   entry->CompressedSize = Self->prvZip.total_out;
   entry->OriginalSize   = Self->prvZip.total_in;

   if (entry->OriginalSize > 0) {
      entry->DeflateMethod = 8;
   }
   else {
      entry->DeflateMethod = 0;
      entry->CompressedSize = 0;
   }

   if ((chain = Self->prvFiles)) {
      while (chain->Next) chain = (ZipFile *)chain->Next;
      entry->Prev = (CompressedFile *)chain;
      chain->Next = (CompressedFile *)entry;
   }
   else Self->prvFiles = entry;

   // Update the header that we earlier wrote for our file entry.  Note that the header stores only some of the file's
   // meta information.  The majority is stored in the directory at the end of the zip file.

   if (acSeek(Self->FileIO, (DOUBLE)entry->Offset, SEEK_START) != ERR_Okay) goto exit;

   UBYTE header[sizeof(glHeader)];
   CopyMemory(glHeader, header, sizeof(glHeader));
   wrb_word(entry->DeflateMethod, header + HEAD_DEFLATEMETHOD);
   wrb_long(entry->TimeStamp, header + HEAD_TIMESTAMP);
   wrb_long(entry->CRC, header + HEAD_CRC);
   wrb_long(entry->CompressedSize, header + HEAD_COMPRESSEDSIZE);
   wrb_long(entry->OriginalSize, header + HEAD_FILESIZE);
   wrb_word(entry->NameLen, header + HEAD_NAMELEN);
   if (acWriteResult(Self->FileIO, header, HEAD_LENGTH) != HEAD_LENGTH) goto exit;
   if (acWriteResult(Self->FileIO, entry->Name, entry->NameLen) != entry->NameLen) goto exit;

   // Send updated feedback if necessary

   if (fb.Progress < fb.OriginalSize) {
      fb.CompressedSize = entry->CompressedSize;
      fb.Progress       = fb.OriginalSize; // 100
      send_feedback(Self, &fb);
   }

   // If this new data replaces an existing file, remove the old file now

   if (fileexists) remove_file(Self, &fileexists);

   acFree(file);
   Self->prvFileIndex++;
   return ERR_Okay;

exit:
   if (deflateend IS TRUE) deflateEnd(&Self->prvZip);

   if (entry) {
      FreeFromLL((CompressedFile *)entry, (CompressedFile *)Self->prvFiles, (CompressedFile **)&Self->prvFiles);
      FreeResource(entry);
   }

   if (file) acFree(file);

   Self->prvFileIndex++;
   return error;
}

//*********************************************************************************************************************

ERROR remove_file(extCompression *Self, ZipFile **File)
{
   parasol::Log log(__FUNCTION__);
   ZipFile *file = *File;

   log.branch("Deleting \"%s\"", file->Name);

   // Seek to the end of the compressed file.  We are going to delete the file by shifting all the data after the file
   // to the start of the file's position.

   if (acSeekStart(Self->FileIO, file->Offset + HEAD_NAMELEN) != ERR_Okay) return log.warning(ERR_Seek);
   UWORD namelen    = read_word(Self->FileIO);
   UWORD extralen   = read_word(Self->FileIO);
   LONG chunksize  = HEAD_LENGTH + namelen + extralen + file->CompressedSize;
   DOUBLE currentpos = file->Offset + chunksize;
   if (acSeekStart(Self->FileIO, currentpos) != ERR_Okay) return log.warning(ERR_Seek);

   DOUBLE writepos = file->Offset;

   struct acRead read = { Self->prvInput, SIZE_COMPRESSION_BUFFER };
   while ((!Action(AC_Read, Self->FileIO, &read)) and (read.Result > 0)) {
      if (acSeekStart(Self->FileIO, writepos) != ERR_Okay) return log.warning(ERR_Seek);
      struct acWrite write = { Self->prvInput, read.Result };
      if (Action(AC_Write, Self->FileIO, &write) != ERR_Okay) return log.warning(ERR_Write);
      writepos += write.Result;

      currentpos += read.Result;
      if (acSeekStart(Self->FileIO, currentpos) != ERR_Okay) return log.warning(ERR_Seek);
   }

   Self->FileIO->set(FID_Size, writepos);

   // Remove the file reference from the chain

   ZipFile *next = (ZipFile *)file->Next;
   FreeFromLL((CompressedFile *)file, (CompressedFile *)Self->prvFiles, (CompressedFile **)&Self->prvFiles);
   FreeResource(file);

   // Adjust the offset of files that were ahead of this one

   for (file=next; file != NULL; file=(ZipFile *)file->Next) file->Offset -= chunksize;

   *File = next;
   return ERR_Okay;
}

//*********************************************************************************************************************
// Scans a zip file and adds file entries to the Compression object.
//
// This technique goes to the end of the zip file and reads the file entries from a huge table.  This is very fast, but
// if the zip file is damaged or partially downloaded, it will fail.  In the event that the directory is unavailable,
// the function will fallback to scan_zip().

static ERROR fast_scan_zip(extCompression *Self)
{
   parasol::Log log(__FUNCTION__);
   ziptail tail;

   log.traceBranch("");

   if (acSeek(Self->FileIO, TAIL_LENGTH, SEEK_END) != ERR_Okay) return ERR_Seek; // Surface error, fail
   if (acRead(Self->FileIO, &tail, TAIL_LENGTH, NULL) != ERR_Okay) return ERR_Read; // Surface error, fail

   if (0x06054b50 != ((ULONG *)&tail)[0]) {
      // Tail not available, use the slow scanner instead
      return scan_zip(Self);
   }

#ifndef REVERSE_BYTEORDER
   tail.filecount  = le16_cpu(tail.filecount);
   tail.listsize   = le32_cpu(tail.listsize);
   tail.listoffset = le32_cpu(tail.listoffset);
#endif

   if (acSeek(Self->FileIO, tail.listoffset, SEEK_START) != ERR_Okay) return ERR_Seek;

   zipentry *list, *scan;
   LONG total_files = 0;
   if (!AllocMemory(tail.listsize, MEM_DATA|MEM_NO_CLEAR, (APTR *)&list, NULL)) {
      log.trace("Reading end-of-central directory from index %d, %d bytes.", tail.listoffset, tail.listsize);
      if (acRead(Self->FileIO, list, tail.listsize, NULL) != ERR_Okay) {
         FreeResource(list);
         return scan_zip(Self);
      }

      ZipFile *lastentry = NULL;
      #pragma GCC diagnostic ignored "-Waddress-of-packed-member"
      ULONG *head = (ULONG *)list;
      #pragma GCC diagnostic warning "-Waddress-of-packed-member"
      LONG i;
      for (i=0; i < tail.filecount; i++) {
         ZipFile *zf, *next;

         if (0x02014b50 != head[0]) {
            log.warning("Zip file has corrupt end-of-file signature.");
            for (zf=Self->prvFiles; zf != NULL; zf=next) {
               next = (ZipFile *)zf->Next;
               FreeResource(zf);
            }
            Self->prvFiles = NULL;
            FreeResource(list);
            return scan_zip(Self);
         }

         scan = (zipentry *)(head + 1);

#ifndef REVERSE_BYTEORDER
         scan->deflatemethod  = le16_cpu(scan->deflatemethod);
         scan->timestamp      = le32_cpu(scan->timestamp);
         scan->crc32          = le32_cpu(scan->crc32);
         scan->compressedsize = le32_cpu(scan->compressedsize);
         scan->originalsize   = le32_cpu(scan->originalsize);
         scan->namelen        = le16_cpu(scan->namelen);
         scan->extralen       = le16_cpu(scan->extralen);
         scan->commentlen     = le16_cpu(scan->commentlen);
         scan->diskno         = le16_cpu(scan->diskno);
         scan->ifile          = le16_cpu(scan->ifile);
         scan->attrib         = le32_cpu(scan->attrib);
         scan->offset         = le32_cpu(scan->offset);
#endif

         // Check the header validity

         if (!AllocMemory(sizeof(ZipFile) + scan->namelen + 1 + scan->commentlen + 1, MEM_DATA, (APTR *)&zf, NULL)) {
            total_files++;

            // Build the file entry structure

            zf->NameLen        = scan->namelen;
            zf->CommentLen     = scan->commentlen;
            zf->CompressedSize = scan->compressedsize;
            zf->OriginalSize   = scan->originalsize;
            zf->DeflateMethod  = scan->deflatemethod;
            zf->TimeStamp      = scan->timestamp;
            zf->CRC            = scan->crc32;
            zf->Offset         = scan->offset;

            if (scan->ostype IS ZIP_PARASOL) {
               zf->Flags = scan->attrib;
            }
            else zf->Flags = 0;

            // Read string information

            STRING str = ((STRING)head) + LIST_LENGTH;

            zf->Name = (STRING)(zf+1);

            if ((str[0] IS '.') and (str[1] IS '/')) { // Get rid of any useless './' prefix that sometimes make their way into zip files
               CopyMemory(str+2, zf->Name, scan->namelen-2);
               zf->Name[zf->NameLen] = 0;
               zf->Name[zf->NameLen-1] = 0;
               zf->Name[zf->NameLen-2] = 0;
            }
            else {
               CopyMemory(str, zf->Name, scan->namelen);
               zf->Name[zf->NameLen] = 0;
            }

            str += scan->namelen;
            str += scan->extralen;

            zf->Comment = (STRING)(zf+1) + scan->namelen + 1;
            CopyMemory(str, zf->Comment, scan->commentlen);
            zf->Comment[scan->commentlen] = 0;
            str += scan->commentlen;

            if (zf->Flags & ZIP_LINK);
            else if (zf->OriginalSize == 0) {
               if (zf->Name[StrLength(zf->Name)-1] IS '/') zf->IsFolder = TRUE;
               //else zf->IsFolder = TRUE;
            }
         }
         else {
            FreeResource(list);
            return ERR_AllocMemory;
         }

         // Linked-list management

         if (lastentry) {
            zf->Prev = (CompressedFile *)lastentry;
            lastentry->Next = (CompressedFile *)zf;
         }
         else Self->prvFiles = zf;
         lastentry = zf;

         head = (ULONG *)(((UBYTE *)head) + LIST_LENGTH + scan->commentlen + scan->namelen + scan->extralen);
      }
   }

   FreeResource(list);
   return ERR_Okay;
}

//*********************************************************************************************************************
// Scans a zip file and adds file entries to the Compression object.

static ERROR scan_zip(extCompression *Self)
{
   parasol::Log log(__FUNCTION__);

   log.traceBranch("");

   if (acSeek(Self->FileIO, 0.0, SEEK_START) != ERR_Okay) return log.warning(ERR_Seek);

   ZipFile *lastentry = NULL;
   LONG type, result;
   LONG total_files = 0;
   while ((type = read_long(Self->FileIO))) {
      if (type IS 0x04034b50) {
         // PKZIP file header entry detected

         if (acSeek(Self->FileIO, (DOUBLE)HEAD_COMPRESSEDSIZE-4, SEEK_CURRENT) != ERR_Okay) return log.warning(ERR_Seek);

         struct {
            ULONG compressedsize;
            ULONG originalsize;
            UWORD namelen;
            UWORD extralen;
         } header;

         if (acRead(Self->FileIO, &header, sizeof(header), &result) != ERR_Okay) return log.warning(ERR_Read);

#ifndef REVERSE_BYTEORDER
         header.compressedsize = le32_cpu(header.compressedsize);
         header.originalsize   = le32_cpu(header.originalsize);
         header.namelen        = le16_cpu(header.namelen);
         header.extralen       = le16_cpu(header.extralen);
#endif

         if (acSeekCurrent(Self->FileIO, header.compressedsize + header.namelen + header.extralen) != ERR_Okay) return log.warning(ERR_Seek);
      }
      else if (type IS 0x02014b50) {
         // PKZIP list entry detected

         total_files++;

         zipentry zipentry;
         if (acRead(Self->FileIO, &zipentry, sizeof(zipentry), &result) != ERR_Okay) return log.warning(ERR_Read);

#ifndef REVERSE_BYTEORDER
         zipentry.deflatemethod  = le16_cpu(zipentry.deflatemethod);
         zipentry.timestamp      = le32_cpu(zipentry.timestamp);
         zipentry.crc32          = le32_cpu(zipentry.crc32);
         zipentry.compressedsize = le32_cpu(zipentry.compressedsize);
         zipentry.originalsize   = le32_cpu(zipentry.originalsize);
         zipentry.namelen        = le16_cpu(zipentry.namelen);
         zipentry.extralen       = le16_cpu(zipentry.extralen);
         zipentry.commentlen     = le16_cpu(zipentry.commentlen);
         zipentry.diskno         = le16_cpu(zipentry.diskno);
         zipentry.ifile          = le16_cpu(zipentry.ifile);
         zipentry.attrib         = le32_cpu(zipentry.attrib);
         zipentry.offset         = le32_cpu(zipentry.offset);
#endif

         // Note: Hidden memory is allocated to save time and resource tracking space when loading huge compressed files.

         ZipFile *entry;
         if (AllocMemory(sizeof(ZipFile) + zipentry.namelen + 1 + zipentry.commentlen + 1, MEM_DATA, (APTR *)&entry, NULL) != ERR_Okay) return log.warning(ERR_Memory);

         // Read the file name string

         entry->Name = (STRING)(entry+1);
         if (!acRead(Self->FileIO, entry->Name, zipentry.namelen, &result)) {
            entry->Name[zipentry.namelen] = 0;
         }
         else return log.warning(ERR_Read);

         // Read extra file information

         char strbuffer[256];
         if (zipentry.extralen > 0) {
            struct acRead read = { strbuffer, zipentry.extralen };
            if (Action(AC_Read, Self->FileIO, &read)) return log.warning(ERR_Read);
         }

         // Read the file comment string, if any

         if (zipentry.commentlen > 0) {
            entry->Comment = entry->Name + zipentry.namelen + 1;
            if (!acRead(Self->FileIO, strbuffer, zipentry.commentlen, &result)) {
               CopyMemory(strbuffer, entry->Comment, zipentry.commentlen);
               entry->Comment[zipentry.commentlen] = 0;
            }
            else return log.warning(ERR_Read);
         }

         // Build the file entry structure

         entry->NameLen        = zipentry.namelen;
         entry->CommentLen     = zipentry.commentlen;
         entry->Comment        = NULL;
         entry->CompressedSize = zipentry.compressedsize;
         entry->OriginalSize   = zipentry.originalsize;
         entry->DeflateMethod  = zipentry.deflatemethod;
         entry->TimeStamp      = zipentry.timestamp;
         entry->CRC            = zipentry.crc32;
         entry->Offset         = zipentry.offset;
         if (zipentry.ostype IS ZIP_PARASOL) {
            entry->Flags = zipentry.attrib;
         }
         else entry->Flags = 0;

         if ((entry->Name[0] IS '.') and (entry->Name[1] IS '/')) {
            // Get rid of any useless './' prefix that sometimes make their way into zip files
            CopyMemory(entry->Name + 2, entry->Name, (entry->NameLen - 2) + 1);
            entry->Name[entry->NameLen-1] = 0;
            entry->Name[entry->NameLen-2] = 0;
         }

         if (entry->Flags & ZIP_LINK);
         else if (entry->OriginalSize == 0) {
            if (entry->Name[StrLength(entry->Name)-1] IS '/') entry->IsFolder = TRUE;
         }

         // Chain management

         if (lastentry) {
            entry->Prev = (CompressedFile *)lastentry;
            lastentry->Next = (CompressedFile *)entry;
         }
         else Self->prvFiles = entry;

         lastentry = entry;
      }
      else if (type IS 0x06054b50) {
         // PKZIP end of file directory signature detected
         log.trace("End of central directory signature detected.");
         break; // End the loop
      }
      else {
         // Unrecognised PKZIP data
         log.warning("Unrecognised PKZIP entry $%.8x in the central directory.", type);
         return ERR_InvalidData;
      }
   }

   log.trace("Detected %d files.", total_files);
   return ERR_Okay;
}

//*********************************************************************************************************************

static ERROR send_feedback(extCompression *Self, CompressionFeedback *Feedback)
{
   parasol::Log log(__FUNCTION__);
   ERROR error;

   if (Self->Feedback.Type) {
      Self->FeedbackInfo = Feedback;

      if (Self->Feedback.Type IS CALL_STDC) {
         auto routine = (ERROR (*)(extCompression *, CompressionFeedback *))Self->Feedback.StdC.Routine;
         if (Self->Feedback.StdC.Context) {
            parasol::SwitchContext context(Self->Feedback.StdC.Context);
            error = routine(Self, Feedback);
         }
         else error = routine(Self, Feedback);
      }
      else if (Self->Feedback.Type IS CALL_SCRIPT) {
         OBJECTPTR script = Self->Feedback.Script.Script;
         if (script) {
            const ScriptArg args[] = {
               { "Compression", FD_OBJECTPTR, { .Address = Self } },
               { "Feedback",    FD_POINTER, { .Address = Feedback } }
            };
            if (scCallback(script, Self->Feedback.Script.ProcedureID, args, ARRAYSIZE(args), &error)) error = ERR_Failed;
         }
         else error = ERR_Failed;
      }
      else {
         log.warning("Callback function structure does not specify a recognised Type.");
         error = ERR_Terminate;
      }

      Self->FeedbackInfo = NULL;
   }
   else error = ERR_Okay;

   return error;
}

//*********************************************************************************************************************

static void write_eof(extCompression *Self)
{
   ZipFile *chain;

   if ((Self->FileIO) and (!Self->SubID) and (Self->prvCompressionCount > 0)) {
      if ((chain = Self->prvFiles)) {
         // Determine the start of the list offset

         while (chain->Next) chain = (ZipFile *)chain->Next;
         acSeekStart(Self->FileIO, chain->Offset + HEAD_NAMELEN);
         UWORD namelen = read_word(Self->FileIO);
         UWORD extralen = read_word(Self->FileIO);
         acSeekCurrent(Self->FileIO, chain->CompressedSize + namelen + extralen);
         ULONG listoffset = chain->Offset + chain->CompressedSize + namelen + extralen + HEAD_LENGTH;

         // Write out the central directory

         ULONG listsize   = 0;
         UWORD filecount  = 0;
         for (chain=Self->prvFiles; chain != NULL; chain=(ZipFile *)chain->Next) {
            UBYTE elist[sizeof(glList)];
            CopyMemory(glList, elist, sizeof(glList));

            wrb_word(chain->DeflateMethod, elist+LIST_METHOD);
            wrb_long(chain->TimeStamp, elist+LIST_TIMESTAMP);
            wrb_long(chain->CRC, elist+LIST_CRC);
            wrb_long(chain->CompressedSize, elist+LIST_COMPRESSEDSIZE);
            wrb_long(chain->OriginalSize, elist+LIST_FILESIZE);
            wrb_word(chain->NameLen, elist+LIST_NAMELEN);
            wrb_word(0, elist+LIST_EXTRALEN);
            wrb_word(chain->CommentLen, elist+LIST_COMMENTLEN);
            wrb_word(0, elist+LIST_DISKNO);
            wrb_word(0, elist+LIST_IFILE);
            wrb_long(chain->Flags, elist+LIST_ATTRIB);
            wrb_long(chain->Offset, elist+LIST_OFFSET);

            acWriteResult(Self->FileIO, elist, LIST_LENGTH);

            acWriteResult(Self->FileIO, chain->Name, chain->NameLen);
            if (chain->Comment) acWriteResult(Self->FileIO, chain->Comment, chain->CommentLen);

            listsize += LIST_LENGTH + chain->NameLen + chain->CommentLen;
            filecount++;
         }

         UBYTE tail[sizeof(glTail)];
         CopyMemory(glTail, tail, sizeof(glTail));

         wrb_word(filecount,  tail + TAIL_FILECOUNT); // File count for this file
         wrb_word(filecount,  tail + TAIL_TOTALFILECOUNT); // File count for all zip files when spanning multiple archives
         wrb_long(listsize,   tail + TAIL_FILELISTSIZE);
         wrb_long(listoffset, tail + TAIL_FILELISTOFFSET);
         acWriteResult(Self->FileIO, tail, TAIL_LENGTH);
      }
      else SetFields(Self->FileIO, FID_Size|TLONG, 0, TAGEND);

      Self->prvCompressionCount = 0;
   }
}

//*********************************************************************************************************************

void zipfile_to_item(ZipFile *ZF, CompressedItem *Item)
{
   ClearMemory(Item, sizeof(*Item));

   Item->Modified.Year   = 1980 + ((ZF->TimeStamp>>25) & 0x3f);
   Item->Modified.Month  = (ZF->TimeStamp>>21) & 0x0f;
   Item->Modified.Day    = (ZF->TimeStamp>>16) & 0x1f;
   Item->Modified.Hour   = (ZF->TimeStamp>>11) & 0x1f;
   Item->Modified.Minute = (ZF->TimeStamp>>5)  & 0x3f;
   Item->Modified.Second = (ZF->TimeStamp>>1)  & 0x0f;
   Item->Path            = ZF->Name;
   Item->OriginalSize    = ZF->OriginalSize;
   Item->CompressedSize  = ZF->CompressedSize;

   if (ZF->Flags & ZIP_LINK) Item->Flags |= FL_LINK;
   else {
      if (Item->OriginalSize == 0) {
         LONG len;
         for (len=0; ZF->Name[len]; len++);
         if (ZF->Name[len-1] IS '/') Item->Flags |= FL_FOLDER;
         else Item->Flags |= FL_FOLDER;
      }
      else Item->Flags |= FL_FILE;
   }

   if (ZF->Flags & ZIP_SECURITY) {
      LONG permissions = 0;
      if (ZF->Flags & ZIP_UEXEC) permissions |= PERMIT_USER_EXEC;
      if (ZF->Flags & ZIP_GEXEC) permissions |= PERMIT_GROUP_EXEC;
      if (ZF->Flags & ZIP_OEXEC) permissions |= PERMIT_OTHERS_EXEC;

      if (ZF->Flags & ZIP_UREAD) permissions |= PERMIT_USER_READ;
      if (ZF->Flags & ZIP_GREAD) permissions |= PERMIT_GROUP_READ;
      if (ZF->Flags & ZIP_OREAD) permissions |= PERMIT_OTHERS_READ;

      if (ZF->Flags & ZIP_UWRITE) permissions |= PERMIT_USER_WRITE;
      if (ZF->Flags & ZIP_GWRITE) permissions |= PERMIT_GROUP_WRITE;
      if (ZF->Flags & ZIP_OWRITE) permissions |= PERMIT_OTHERS_WRITE;

      Item->Permissions = permissions;
   }
}

