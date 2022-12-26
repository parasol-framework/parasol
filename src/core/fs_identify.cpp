
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

/*****************************************************************************

-FUNCTION-
IdentifyFile: Identifies the class and/or command that may be used to load a file.

This function examines the relationship between file data and the available system classes.  It allows a JPEG file to
be identified as a datatype of the picture object, or an MP3 file to be identified as a datatype of the sound object
for instance.

The function works by analysing the Path's file extension and comparing it to the supported extensions of all available
classes.  If a class supports the file extension then the ID of that class will be returned. If the file extension is
not listed in the class dictionary or if it is listed more than once, the first 80 bytes of the file's data will be
loaded and checked against classes that can match against file header information.  If a match is found, the ID of the
matching class will be returned.

This function returns an error code of ERR_Search in the event that a suitable class is not available to match against
the given file.

-INPUT-
cstr Path:     The location of the object data.
cstr Mode:     Common modes are "Open", "View", "Edit" or "Print".  The default mode is "Open".
int(IDF) Flags: Use IDF_HOST to query the host platform's file associations only.  IDF_IGNORE_HOST queries the internal file assocations only.
&cid Class:    Must refer to a CLASSID variable that will store the resulting class ID.
&cid SubClass: Optional argument that can refer to a variable that will store the resulting sub-class ID (if the result is a base-class, this variable will receive a value of zero).
!str Command:  The command associated with the datatype will be returned here.  Set to NULL if this is not required.  The resulting string must be destroyed once no longer required.

-ERRORS-
Okay
Args
Search: A suitable class could not be found for the data source.
FileNotFound
Read
-END-

*****************************************************************************/

ERROR IdentifyFile(CSTRING Path, CSTRING Mode, LONG Flags, CLASSID *ClassID, CLASSID *SubClassID, STRING *Command)
{
   parasol::Log log(__FUNCTION__);
   CSTRING filename;
   LONG i, bytes_read;
   #define HEADER_SIZE 80

   if ((!Path) or (!ClassID)) return log.warning(ERR_NullArgs);

   log.branch("File: %s, Mode: %s, Command: %s, Flags: $%.8x", Path, Mode, Command ? "Yes" : "No", Flags);

   // Determine the class type by examining the Path file name.  If the file extension does not tell us the class
   // that supports the data, we then load the first 256 bytes from the file and then compare file headers.

   ERROR error = ERR_Okay;
   STRING res_path = NULL;
   STRING cmd = NULL;
   if (!Mode) Mode = "Open";
   *ClassID = 0;
   if (SubClassID) *SubClassID = 0;
   if (Command) *Command = NULL;
   UBYTE buffer[400] = { 0 };

   if (Flags & IDF_HOST) goto host_platform;

   if ((error = load_datatypes())) { // Load the associations configuration file
      return log.warning(error);
   }

   // Scan for device associations.  A device association, e.g. http: can link to a class or provide the appropriate
   // command in its datatype group.

   ConfigGroups *groups;
   if (!glDatatypes->getPtr(FID_Data, &groups)) {
      for (auto& [group, keys] : groups[0]) {
         if (!group.compare(0, 4, "DEV:")) {
            LONG j = group.size() - 4;
            if (!StrCompare(group.c_str() + 4, Path, j, 0)) {
               if (Path[j] != ':') continue;

               CSTRING datatype;
               if (keys.contains("Datatype")) datatype = keys["Datatype"].c_str();
               else if (keys.contains("Mode")) cmd = StrClone(keys["Mode"].c_str());

               // Found device association

               if ((!cmd) and (datatype)) {
                  for (auto& [group, keys] : groups[0]) {
                     if (!StrMatch(datatype, group.c_str())) {
                        if (keys.contains(Mode)) {
                           cmd = StrClone((Flags & IDF_SECTION) ? group.c_str() : keys[Mode].c_str());
                        }
                        break;
                     }
                  }

                  if (!cmd) log.trace("Datatype '%s' missing mode '%s'", datatype, Mode);
               }
               else log.warning("No datatype reference for group '%s'", group.c_str());

               goto restart;
            }
         }
      }
   }

   ERROR reserror;
   if ((reserror = ResolvePath(Path, RSF_APPROXIMATE|RSF_PATH|RSF_CHECK_VIRTUAL, &res_path)) != ERR_Okay) {
      if (reserror IS ERR_VirtualVolume) {
         // Virtual volumes may support the IdentifyFile() request as a means of speeding up file identification.  This
         // is often useful when probing remote file systems.  If the FS doesn't support this option, we can still
         // fall-back to the standard file reading option.
         //
         // Note: A virtual volume may return ERR_Okay even without identifying the class of the queried file.  This
         // means that the file was analysed but belongs to no known class.

         virtual_drive *vd;
         if ((vd = get_virtual(res_path))) {
            if (vd->IdentifyFile) {
               if (!vd->IdentifyFile(res_path, ClassID, SubClassID)) {
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

         if (!StrCompare("string:", Path, 7, 0)) { // Do not check for '|' when string: is in use
            return ERR_FileNotFound;
         }

         for (i=0; (Path[i]) and (Path[i] != '|'); i++) {
            if (Path[i] IS ';') log.warning("Use of ';' obsolete, use '|' in path %s", Path);
         }

         if (Path[i] IS '|') {
            STRING tmp = StrClone(Path);
            tmp[i] = 0;
            if (ResolvePath(tmp, RSF_APPROXIMATE, &res_path) != ERR_Okay) {
               FreeResource(tmp);
               return ERR_FileNotFound;
            }
            else FreeResource(tmp);
         }
         else return ERR_FileNotFound;
      }
   }

   // Check against the class registry to identify what class and sub-class that this data source belongs to.

   ClassHeader *classes;

   if ((classes = glClassDB)) {
      LONG *offsets = CL_OFFSETS(classes);

      // Check extension

      log.trace("Checking extension against class database.");

      if (!*ClassID) {
         if ((filename = get_filename(res_path))) {
            for (LONG i=0; (i < classes->Total) and (!*ClassID); i++) {
               ClassItem *item = (ClassItem *)((char *)classes + offsets[i]);
               if (item->MatchOffset) {
                  if (!StrCompare((STRING)item + item->MatchOffset, filename, 0, STR_WILDCARD)) {
                     if (item->ParentID) {
                        *ClassID = item->ParentID;
                        if (SubClassID) *SubClassID = item->ClassID;
                     }
                     else *ClassID = item->ClassID;
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

         if ((!ReadFileToBuffer(res_path, buffer, HEADER_SIZE, &bytes_read)) and (bytes_read >= 4)) {
            log.trace("Checking file header data (%d bytes) against %d classes....", bytes_read, classes->Total);
            for (LONG i=0; (i < classes->Total) and (!*ClassID); i++) {
               ClassItem *item = (ClassItem *)((char *)classes + offsets[i]);
               if (!item->HeaderOffset) continue;

               CSTRING header = (CSTRING)item + item->HeaderOffset; // Compare the header to the Path buffer
               BYTE match = TRUE;  // Headers use an offset based hex format, for example: [8:$958a9b9f9301][24:$939a9fff]
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

                        if (offset >= bytes_read) {
                           match = FALSE;
                           break;
                        }

                        if (byte != buffer[offset++]) {
                           match = FALSE;
                           break;
                        }
                     }
                  }
                  else {
                     while ((*header) and (*header != ']')) {
                        if (offset >= bytes_read) {
                           match = FALSE;
                           break;
                        }

                        if (*header != buffer[offset++]) {
                           match = FALSE;
                           break;
                        }
                        header++;
                     }
                  }

                  if (match IS FALSE) {
                     while ((*header) and (*header != '|')) header++; // Look for an or operation
                     if (*header IS '|') {
                        match = TRUE; // Continue comparisons
                        header++;
                     }
                  }
               }

               if (match) {
                  if (item->ParentID) {
                     *ClassID = item->ParentID;
                     if (SubClassID) *SubClassID = item->ClassID;
                  }
                  else *ClassID = item->ClassID;
                  break;
               }
            } // for all classes
         }
         else error = log.warning(ERR_Read);
      }
   }
   else {
      log.warning("Class database not available.");
      error = ERR_Search;
   }

class_identified:
   if (res_path) FreeResource(res_path);

   if (!error) {
      if (*ClassID) log.debug("File belongs to class $%.8x:$%.8x", *ClassID, (SubClassID) ? *SubClassID : 0);
      else {
         log.debug("Failed to identify file \"%s\"", Path);
         error = ERR_Search;
      }
   }

   if (!Command) { // Return now if there is no request for a command string
      if (!(*ClassID)) return ERR_Search;
      else return error;
   }

   // Note that if class identification failed, it will be because the data does not belong to a specific class.
   // The associations.cfg file will help us load files that do not have class associations later in this subroutine.

   if ((*ClassID != ID_TASK) and (!StrMatch("Open", Mode))) {
      // Testing the X file bit is only reliable with a native installation, as other Linux systems often
      // mount FAT partitions with +X on everything.

      const SystemState *state = GetSystemState();
      FileInfo info;
      if (!StrMatch("Native", state->Platform)) {
         log.trace("Checking for +x permissions on file %s", Path);
         if (!fs_getinfo(Path, &info, sizeof(info))) {
            if ((info.Flags & RDF_FILE) and (info.Permissions & PERMIT_ALL_EXEC)) {
               log.trace("Path carries +x permissions.");
               *ClassID = ID_TASK;
            }
         }
      }
      else if (!StrMatch("Linux", state->Platform)) {
         STRING resolve;
         if (!ResolvePath(Path, RSF_NO_FILE_CHECK, &resolve)) {
            if ((!StrCompare("/usr/", resolve, 5, 0)) or
                (!StrCompare("/opt/", resolve, 5, 0)) or
                (!StrCompare("/bin/", resolve, 5, 0))) {
               log.trace("Checking for +x permissions on file %s", resolve);
               if (!fs_getinfo(resolve, &info, sizeof(info))) {
                  if ((info.Flags & RDF_FILE) and (info.Permissions & PERMIT_ALL_EXEC)) {
                     log.trace("Path carries +x permissions");
                     *ClassID = ID_TASK;
                  }
               }
            }
            else log.trace("Path is not supported for +x checks.");
            FreeResource(resolve);
         }
         else log.trace("Failed to resolve location '%s'", Path);
      }
      else log.trace("No +x support for platform '%s'", state->Platform);
   }
   else log.trace("Skipping checks for +x permission flags.");

   // If the file is an executable, return a clone of the location path

   if (*ClassID IS ID_TASK) {
      if (!AllocMemory(StrLength(Path)+3, MEM_STRING, (APTR *)&res_path, NULL)) {
         *Command = res_path;
         *res_path++ = '"';
         while (*Path) *res_path++ = *Path++;
         *res_path++ = '"';
         *res_path = 0;
         return ERR_Okay;
      }
      else return log.warning(ERR_AllocMemory);
   }

   // Check device names

   if (*ClassID) {
      // Get the name of the class and sub-class so that we can search the datatypes object.

      if ((!SubClassID) or (!*SubClassID)) {
         get_class_cmd(Mode, glDatatypes, Flags, *ClassID, &cmd);
      }
      else if (get_class_cmd(Mode, glDatatypes, Flags, *SubClassID, &cmd) != ERR_Okay) {
         get_class_cmd(Mode, glDatatypes, Flags, *ClassID, &cmd);
      }

      log.msg("Class command: %s", cmd);
   }
   else log.msg("No class was identified for file '%s'.", Path);

   // Scan for customised file associations.  These override the default class settings, so the user can come up with
   // his own personal settings in circumstances where a class association is not suitable.

   if ((filename = get_filename(Path))) {
      log.msg("Scanning associations config to match: %s", filename);

      if (!glDatatypes->getPtr(FID_Data, &groups)) {
         for (auto& [group, keys] : groups[0]) {
            if (not keys.contains("Match")) continue;

            if (!StrCompare(keys["Match"].c_str(), filename, 0, STR_WILDCARD)) {
               if (Flags & IDF_SECTION) cmd = StrClone(group.c_str());
               else if (keys.contains(Mode)) cmd = StrClone(keys[Mode].c_str());
               else if (StrMatch("Open", Mode) != ERR_Okay) {
                  if (keys.contains("Open")) cmd = StrClone(keys["Open"].c_str());
               }
               break;
            }
         }
      }
   }

restart:
   if ((!(Flags & IDF_SECTION)) and (cmd) and (!StrCompare("[PROG:", cmd, 6, 0))) { // Command is referencing a program
      STRING str;
      if (!(TranslateCmdRef(cmd, &str))) {
         FreeResource(cmd);
         cmd = str;
      }
      else {
         log.warning("Reference to program '%s' is invalid.", buffer);
         FreeResource(cmd);
         cmd = NULL;
         goto exit; // Abort if the program is not available
      }
   }

host_platform:
#ifdef _WIN32
   log.trace("Windows execution process...");

   if ((!cmd) and (!(Flags & (IDF_SECTION|IDF_IGNORE_HOST)))) { // Check if Windows supports the file type
      if (!ResolvePath(Path, RSF_APPROXIMATE, &res_path)) {

         if (!StrCompare("http:", res_path, 5, NULL)) { // HTTP needs special support
            char buffer[300];
            if (winReadRootKey("http\\shell\\open\\command", NULL, buffer, sizeof(buffer))) {
               std::string open(buffer);
               auto param = open.find("%1");
               if (param != std::string::npos) open.insert(param, "[@file]", 2);
               else open += " \"[@file]\"";

               cmd = StrClone(open.c_str());
            }
         }
         else {
            char key[300];
            CSTRING ext = get_extension(res_path);

            if ((ext) and ((i = winReadRootKey(ext-1, NULL, key, sizeof(key))))) {
               char buffer[300];
               StrCopy("\\Shell\\Open\\Command", key+i, sizeof(key)-i);

               if (winReadRootKey(key, NULL, buffer, sizeof(buffer))) {
                  std::string rootkey(buffer);
                  auto param = rootkey.find("%1");
                  if (param != std::string::npos) rootkey.replace(param, 2, "[@file]");
                  else rootkey += " \"[@file]\"";

                  // Use of %systemroot% is common

                  auto sysroot = rootkey.find("%SystemRoot%");
                  if (sysroot != std::string::npos) {
                     char sr[100];
                     sr[0] = 0;
                     if (!winReadKey("\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion", "SystemRoot", sr, sizeof(sr))) {
                        if (!winReadKey("\\SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", "SystemRoot", sr, sizeof(sr))) {
                           // Failure
                        }
                     }

                     if (sr[0]) rootkey.replace(sysroot, 12, sr);
                  }

                  // Check if an absolute path was given.  If not, we need to resolve the .exe to its absolute path.

                  if ((rootkey.size() >= 2) and (rootkey[1] IS ':')); // Path is absolute
                  else {
                     STRING abs;

                     std::string path;
                     if (rootkey[0] IS '"') {
                        auto end = rootkey.find('"', 1);
                        if (end IS std::string::npos) end = rootkey.size();
                        path = rootkey.substr(1, end);
                     }
                     else {
                        LONG end;
                        for (end=0; rootkey[end] > 0x20; end++);
                        path = rootkey.substr(0, end);
                     }

                     if (!ResolvePath(path.c_str(), RSF_PATH, &abs)) {
                        rootkey.insert(0, abs);
                        FreeResource(abs);
                     }
                  }

                  cmd = StrClone(rootkey.c_str());
               }
               else log.trace("Failed to read key %s", key);
            }
            else log.trace("Windows has no mapping for extension %s", ext);

            if (!cmd) {
               char buffer[300];
               if (!winGetCommand(res_path, buffer, sizeof(buffer))) {
                  i = StrLength(buffer);
                  StrCopy(" \"[@file]\"", buffer+i, sizeof(buffer)-i);
                  cmd = StrClone(buffer);
               }
               else log.msg("Windows cannot identify path: %s", res_path);
            }
         }

         FreeResource(res_path);
      }
   }
#endif

   // If no association exists for the file then use the default options if they are available.

   if (!cmd) {
      CSTRING str;
      if (Flags & IDF_HOST); // Return NULL if the host platform has no association
      else if (Flags & IDF_SECTION); // Return NULL and not 'default' when this option is used
      else if ((!cfgReadValue(glDatatypes, "default", Mode, &str)) and (str)) {
         cmd = StrClone(str);
         goto restart;
      }
   }

   *Command = cmd;

exit:
   log.trace("File belongs to class $%.8x", *ClassID);
   if ((!*ClassID) and (!cmd)) return ERR_Search;
   else return ERR_Okay;
}

//****************************************************************************
// Scan the class database to extract the correct name for ClassID.  Then scan the Associations object for an entry
// that is registered against the given class type.

ERROR get_class_cmd(CSTRING Mode, objConfig *Associations, LONG Flags, CLASSID ClassID, STRING *Command)
{
   parasol::Log log("IdentifyFile");

   if ((!ClassID) or (!Command) or (!Associations)) return log.warning(ERR_NullArgs);

   auto item = find_class(ClassID);

   if (item) {
      ConfigGroups *groups;
      if (!Associations->getPtr(FID_Data, &groups)) {
         for (auto& [group, keys] : groups[0]) {
            if ((keys.contains("Class")) and (!StrMatch(keys["Class"].c_str(), item->Name))) {
               if (Flags & IDF_SECTION) *Command = StrClone(group.c_str());
               else if (keys.contains(Mode)) *Command = StrClone(keys[Mode].c_str());
               else return ERR_NoData;

               if (!*Command) return ERR_AllocMemory;
               else return ERR_Okay;
            }
         }
      }
   }

   return ERR_Search;
}
