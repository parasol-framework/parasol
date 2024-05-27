
inline CSTRING get_extension(CSTRING Path)
{
   ULONG i;
   for (i=0; Path[i]; i++);
   while ((i > 0) and (Path[i] != '.') and (Path[i] != ':') and (Path[i] != '/') and (Path[i] != '\\')) i--;
   if (Path[i] IS '.') return Path+i+1;
   else return NULL;
}

inline CSTRING get_filename(CSTRING Path)
{
   ULONG i;
   for (i=0; Path[i]; i++);
   while ((i > 0) and (Path[i-1] != '/') and (Path[i-1] != '\\') and (Path[i-1] != ':')) i--;
   if (Path[i]) return Path+i;
   else return NULL;
}

/*********************************************************************************************************************

-FUNCTION-
IdentifyFile: Analyse a file and identify a class that can process it.

This function examines the relationship between file data and installed classes.  It allows for instance, a JPEG file
to be identified as a datatype of the @Picture class, or an MP3 file to be identified as a datatype of the @Sound
class.

The method involves analysing the `Path`'s file extension and comparing it to the supported extensions of all available
classes.  If a class supports the file extension then the ID of that class will be returned. If the file extension is
not listed in the class dictionary or if it is listed more than once, the first 80 bytes of the file's data will be
loaded and checked against classes that can match against file header information.  If a match is found, the ID of the
matching class will be returned.

This function returns an error code of `ERR::Search` in the event that a suitable class is not available to match
against the given file.

-INPUT-
cstr Path:     The location of the object data.
&cid Class:    Must refer to a `CLASSID` variable that will store the resulting class ID.
&cid SubClass: Optional argument that can refer to a variable that will store the resulting sub-class ID (if the result is a base-class, this variable will receive a value of zero).

-ERRORS-
Okay
Args
Search: A suitable class could not be found for the data source.
FileNotFound
Read
-END-

*********************************************************************************************************************/

ERR IdentifyFile(CSTRING Path, CLASSID *ClassID, CLASSID *SubClassID)
{
   pf::Log log(__FUNCTION__);
   LONG i, bytes_read;
   #define HEADER_SIZE 80

   if ((!Path) or (!ClassID)) return log.warning(ERR::NullArgs);

   log.branch("File: %s", Path);

   // Determine the class type by examining the Path file name.  If the file extension does not tell us the class
   // that supports the data, we then load the first 256 bytes from the file and then compare file headers.

   ERR error = ERR::Okay;
   STRING res_path = NULL;
   if (ClassID) *ClassID = 0;
   if (SubClassID) *SubClassID = 0;
   UBYTE buffer[400] = { 0 };

   if ((error = load_datatypes()) != ERR::Okay) { // Load the associations configuration file
      return log.warning(error);
   }

   ERR reserror;
   if ((reserror = ResolvePath(Path, RSF::APPROXIMATE|RSF::PATH|RSF::CHECK_VIRTUAL, &res_path)) != ERR::Okay) {
      if (reserror IS ERR::VirtualVolume) {
         // Virtual volumes may support the IdentifyFile() request as a means of speeding up file identification.  This
         // is often useful when probing remote file systems.  If the FS doesn't support this option, we can still
         // fall-back to the standard file reading option.
         //
         // Note: A virtual volume may return ERR::Okay even without identifying the class of the queried file.  This
         // means that the file was analysed but belongs to no known class.

         if (auto vd = get_virtual(res_path)) {
            if (vd->IdentifyFile) {
               if (vd->IdentifyFile(res_path, ClassID, SubClassID) IS ERR::Okay) {
                  log.trace("Virtual volume identified the target file.");
                  goto class_identified;
               }
               else log.trace("Virtual volume reports no support for %d:%d", *ClassID, *SubClassID);
            }
            else log.trace("Virtual volume does not support IdentifyFile()");
         }
      }
      else {
         // Before we assume failure - check for the use of semicolons that split the string into multiple file names.

         log.warning("ResolvePath() failed on '%s', error '%s'", Path, GetErrorMsg(reserror));

         if (StrCompare("string:", Path, 7) IS ERR::Okay) { // Do not check for '|' when string: is in use
            return ERR::FileNotFound;
         }

         for (i=0; (Path[i]) and (Path[i] != '|'); i++) {
            if (Path[i] IS ';') log.warning("Use of ';' obsolete, use '|' in path %s", Path);
         }

         if (Path[i] IS '|') {
            STRING tmp = StrClone(Path);
            tmp[i] = 0;
            if (ResolvePath(tmp, RSF::APPROXIMATE, &res_path) != ERR::Okay) {
               FreeResource(tmp);
               return ERR::FileNotFound;
            }
            else FreeResource(tmp);
         }
         else return ERR::FileNotFound;
      }
   }

   // Check against the class registry to identify what class and sub-class that this data source belongs to.

   if (!glClassDB.empty()) {
      // Check extension

      log.trace("Checking extension against class database.");

      if (!*ClassID) {
         if (auto filename = get_filename(res_path)) {
            for (auto it = glClassDB.begin(); it != glClassDB.end(); it++) {
               auto &rec = it->second;
               if (!rec.Match.empty()) {
                  if (StrCompare(rec.Match.c_str(), filename, 0, STR::WILDCARD) IS ERR::Okay) {
                     if (rec.ParentID) {
                        *ClassID = rec.ParentID;
                        if (SubClassID) *SubClassID = rec.ClassID;
                     }
                     else *ClassID = rec.ClassID;
                     log.trace("File identified as class $%.8x", *ClassID);
                     break;
                  }
               }
            }
         }
      }

      // Check data

      if (!*ClassID) {
         log.trace("Loading file header to identify '%s' against class registry", res_path);

         if ((ReadFileToBuffer(res_path, buffer, HEADER_SIZE, &bytes_read) IS ERR::Okay) and (bytes_read >= 4)) {
            log.trace("Checking file header data (%d bytes) against %d classes....", bytes_read, glClassDB.size());
            for (auto it = glClassDB.begin(); it != glClassDB.end(); it++) {
               auto &rec = it->second;
               if (rec.Header.empty()) continue;

               CSTRING header = rec.Header.c_str(); // Compare the header to the Path buffer
               bool match = true;  // Headers use an offset based hex format, for example: [8:$958a9b9f9301][24:$939a9fff]
               while (*header) {
                  if (*header != '[') {
                     if ((*header IS '|') and (match)) break;
                     header++;
                     continue;
                  }

                  header++;
                  LONG offset = StrToInt(header);
                  while ((*header) and (*header != ':')) header++;
                  if (!header[0]) break;

                  header++; // Skip ':'
                  if (*header IS '$') { // Find and compare the data, one byte at a time
                     header++; // Skip '$'
                     while (*header) {
                        if (((*header >= '0') and (*header <= '9')) or
                            ((*header >= 'A') and (*header <= 'F')) or
                            ((*header >= 'a') and (*header <= 'f'))) break;
                        header++;
                     }

                     UBYTE byte;
                     while (*header) {
                        // Nibble 1
                        if ((*header >= '0') and (*header <= '9')) byte = (*header - '0')<<4;
                        else if ((*header >= 'A') and (*header <= 'F')) byte = (*header - 'A' + 0x0a)<<4;
                        else if ((*header >= 'a') and (*header <= 'f')) byte = (*header - 'a' + 0x0a)<<4;
                        else break;
                        header++;

                        // Nibble 2
                        if ((*header >= '0') and (*header <= '9')) byte |= *header - '0';
                        else if ((*header >= 'A') and (*header <= 'F')) byte |= *header - 'A' + 0x0a;
                        else if ((*header >= 'a') and (*header <= 'f')) byte |= *header - 'a' + 0x0a;
                        else break;
                        header++;

                        if (offset >= bytes_read) { match = false; break; }
                        if (byte != buffer[offset++]) { match = false; break; }
                     }
                  }
                  else {
                     while ((*header) and (*header != ']')) {
                        if (offset >= bytes_read) { match = false; break; }
                        if (*header != buffer[offset++]) { match = false; break; }
                        header++;
                     }
                  }

                  if (!match) {
                     while ((*header) and (*header != '|')) header++; // Look for an or operation
                     if (*header IS '|') {
                        match = true; // Continue comparisons
                        header++;
                     }
                  }
               }

               if (match) {
                  if (rec.ParentID) {
                     *ClassID = rec.ParentID;
                     if (SubClassID) *SubClassID = rec.ClassID;
                  }
                  else *ClassID = rec.ClassID;
                  break;
               }
            } // for all classes
         }
         else error = log.warning(ERR::Read);
      }
   }
   else {
      log.warning("Class database not available.");
      error = ERR::Search;
   }

class_identified:
   if (res_path) FreeResource(res_path);

   if (error IS ERR::Okay) {
      if (*ClassID) log.detail("File belongs to class $%.8x:$%.8x", *ClassID, (SubClassID) ? *SubClassID : 0);
      else {
         log.detail("Failed to identify file \"%s\"", Path);
         error = ERR::Search;
      }
   }

   if (!(*ClassID)) return ERR::Search;
   else return error;
}
