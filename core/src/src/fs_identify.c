
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
int(IDF) Flags: Set to IDF_HOST if you want to query the host platform's file associations only.  IDF_IGNORE_HOST queries the internal file assocations only.
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
   CSTRING filename;
   LONG i, j, bytes_read;
   #define HEADER_SIZE 80

   if ((!Path) OR (!ClassID)) return LogError(ERH_IdentifyFile, ERR_NullArgs);

   LogF("~IdentifyFile()","File: %s, Mode: %s, Command: %s", Path, Mode, Command ? "Yes" : "No");

   // Determine the class type by examining the Path file name.  If the file extension does not tell us the class
   // that supports the data, we then load the first 256 bytes from the file and then compare file headers.

   ERROR error  = ERR_Okay;
   STRING res_path = NULL;
   STRING cmd = NULL;
   if (!Mode) Mode = "Open";
   *ClassID = 0;
   if (SubClassID) *SubClassID = 0;
   if (Command) *Command = NULL;

   if (Flags & IDF_HOST) goto host_platform;

   if ((error = load_datatypes())) { // Load the associations configuration file
      LogError(ERH_IdentifyFile, error);
      LogBack();
      return error;
   }

   // Scan for device associations.  A device association, e.g. http: can link to a class or provide the appropriate
   // command in its datatype section.

   struct ConfigEntry *entries;
   if ((entries = glDatatypes->Entries)) {
      STRING datatype, str;

      for (i=0; i < glDatatypes->AmtEntries; i++) {
         if (!StrCompare("DEV:", entries[i].Section, 4, NULL)) {
            j = StrLength(entries[i].Section + 4);
            if (!StrCompare(entries[i].Section + 4, Path, j, NULL)) {
               str = entries[i].Section;

               if (Path[j] != ':') continue;

               datatype = NULL;
               while ((i < glDatatypes->AmtEntries) AND (!StrMatch(entries[i].Section, str))) {
                  if (!StrMatch("Datatype", entries[i].Key)) {
                     datatype = entries[i].Data;
                     break;
                  }
                  else if (!StrMatch(entries[i].Key, Mode)) {
                     cmd = StrClone(entries[i].Data);
                     break;
                  }
                  i++;
               }

               // Found device association

               if ((!cmd) AND (datatype)) {
                  for (i=0; i < glDatatypes->AmtEntries; i++) {
                     if (!StrMatch(datatype, entries[i].Section)) {
                        FMSG("IdentifyFile","Found datatype '%s'", datatype);
                        while (!StrMatch(datatype, entries[i].Section)) {
                           if (!StrMatch(Mode, entries[i].Key)) {
                              if (Flags & IDF_SECTION) cmd = StrClone(entries[i].Section);
                              else cmd = StrClone(entries[i].Data);
                              break;
                           }
                           i++;
                        }
                        break;
                     }
                  }

                  if (!cmd) FMSG("IdentifyFile","Datatype '%s' missing mode '%s'", datatype, Mode);
               }
               else LogF("@IdentifyFile","No datatype reference for section '%s'", str);

               goto restart; // Jump to the restart label
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

         struct virtual_drive *virtual;
         if ((virtual = get_virtual(res_path))) {
            if (virtual->IdentifyFile) {
               if (!virtual->IdentifyFile(res_path, ClassID, SubClassID)) {
                  FMSG("IdentifyFile","Virtual volume identified the target file.");
                  goto class_identified;
               }
               else FMSG("IdentifyFile","Virtual volume reports no support for %d:%d", *ClassID, *SubClassID);
            }
            else FMSG("IdentifyFile","Virtual volume does not support IdentifyFile()");
         }
      }
      else {
         // Before we assume failure - check for the use of semicolons that split the string into multiple file names.

         LogF("@IdentifyFile","ResolvePath() failed on '%s', error '%s'", Path, GetErrorMsg(reserror));

         if (!StrCompare("string:", Path, 7, NULL)) {
            // Do not check for '|' when string: is in use
            LogBack();
            return ERR_FileNotFound;
         }

         for (i=0; (Path[i]) AND (Path[i] != '|'); i++) {
            if (Path[i] IS ';') LogF("@IdentifyFile","Use of ';' obsolete, use '|' in path %s", Path);
         }

         if (Path[i] IS '|') {
            STRING tmp = StrClone(Path);
            tmp[i] = 0;
            if (ResolvePath(tmp, RSF_APPROXIMATE, &res_path) != ERR_Okay) {
               FreeResource(tmp);
               LogBack();
               return ERR_FileNotFound;
            }
            else FreeResource(tmp);
         }
         else {
            LogBack();
            return ERR_FileNotFound;
         }
      }
   }

   // Check against the class registry to identify what class and sub-class that this data source belongs to.

   struct ClassHeader *classes;
   struct ClassItem *item;
   LONG *offsets;
   UBYTE buffer[400] = { 0 };

   if ((classes = glClassDB)) {
      offsets = CL_OFFSETS(classes);

      // Check extension

      FMSG("IdentifyFile","Checking extension against class database.");

      if (!*ClassID) {
         if ((filename = get_filename(res_path))) {
            for (i=0; (i < classes->Total) AND (!*ClassID); i++) {
               item = ((APTR)classes) + offsets[i];
               if (item->MatchOffset) {
                  if (!StrCompare((STRING)item + item->MatchOffset, filename, 0, STR_WILDCARD)) {
                     if (item->ParentID) {
                        *ClassID = item->ParentID;
                        if (SubClassID) *SubClassID = item->ClassID;
                     }
                     else *ClassID = item->ClassID;
                     FMSG("IdentifyFile","File identified as class $%.8x", *ClassID);
                     break;
                  }
               }
            }
         }
      }

      // Check data

      if (!*ClassID) {
         FMSG("IdentifyFile","Loading file header to identify '%s' against class registry", res_path);

         if ((!ReadFileToBuffer(res_path, buffer, HEADER_SIZE, &bytes_read)) AND (bytes_read >= 4)) {
            FMSG("IdentifyFile","Checking file header data (%d bytes) against %d classes....", bytes_read, classes->Total);
            for (i=0; (i < classes->Total) AND (!*ClassID); i++) {
               item = ((APTR)classes) + offsets[i];
               if (!item->HeaderOffset) continue;

               STRING header = (STRING)item + item->HeaderOffset; // Compare the header to the Path buffer
               BYTE match = TRUE;  // Headers use an offset based hex format, for example: [8:$958a9b9f9301][24:$939a9fff]
               while (*header) {
                  if (*header != '[') {
                     if ((*header IS '|') AND (match)) break;
                     header++;
                     continue;
                  }

                  header++;
                  LONG offset = StrToInt(header);
                  while ((*header) AND (*header != ':')) header++;
                  if (!header[0]) break;

                  header++; // Skip ':'
                  if (*header IS '$') { // Find and compare the data, one byte at a time
                     header++; // Skip '$'
                     while (*header) {
                        if (((*header >= '0') AND (*header <= '9')) OR
                            ((*header >= 'A') AND (*header <= 'F')) OR
                            ((*header >= 'a') AND (*header <= 'f'))) break;
                        header++;
                     }

                     UBYTE byte;
                     while (*header) {
                        // Nibble 1
                        if ((*header >= '0') AND (*header <= '9')) byte = (*header - '0')<<4;
                        else if ((*header >= 'A') AND (*header <= 'F')) byte = (*header - 'A' + 0x0a)<<4;
                        else if ((*header >= 'a') AND (*header <= 'f')) byte = (*header - 'a' + 0x0a)<<4;
                        else break;
                        header++;

                        // Nibble 2
                        if ((*header >= '0') AND (*header <= '9')) byte |= *header - '0';
                        else if ((*header >= 'A') AND (*header <= 'F')) byte |= *header - 'A' + 0x0a;
                        else if ((*header >= 'a') AND (*header <= 'f')) byte |= *header - 'a' + 0x0a;
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
                     while ((*header) AND (*header != ']')) {
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
                     while ((*header) AND (*header != '|')) header++; // Look for an OR operation
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
         else error = LogError(ERH_IdentifyFile, ERR_Read);
      }
   }
   else {
      LogF("@IdentifyFile","Class database not available.");
      error = ERR_Search;
   }

class_identified:
   if (res_path) FreeResource(res_path);

   if (!error) {
      if (*ClassID) LogF("6IdentifyFile","File belongs to class $%.8x:$%.8x", *ClassID, (SubClassID) ? *SubClassID : 0);
      else {
         LogF("6IdentifyFile","Failed to identify file \"%s\"", Path);
         error = ERR_Search;
      }
   }

   if (!Command) { // Return now if there is no request for a command string
      LogBack();
      if (!(*ClassID)) return ERR_Search;
      else return error;
   }

   // Note that if class identification failed, it will be because the data does not belong to a specific class.
   // The associations.cfg file will help us load files that do not have class associations later in this subroutine.

   if ((*ClassID != ID_TASK) AND (!StrMatch("Open", Mode))) {
      // Testing the X file bit is only reliable in the commercial version of Parasol, as other Linux systems often
      // mount FAT partitions with +X on everything.

      const struct SystemState *state = GetSystemState();
      struct FileInfo info;
      if (!StrMatch("Native", state->Platform)) {
         MSG("Checking for +x permissions on file %s", Path);
         UBYTE filename[MAX_FILENAME];
         if (!get_file_info(Path, &info, sizeof(info), filename, sizeof(filename))) {
            if (info.Flags & RDF_FILE) {
               if (info.Permissions & PERMIT_ALL_EXEC) {
                  MSG("Path carries +x permissions.");
                  *ClassID = ID_TASK;
               }
            }
         }
      }
      else if (!StrMatch("Linux", state->Platform)) {
         STRING resolve;
         if (!ResolvePath(Path, RSF_NO_FILE_CHECK, &resolve)) {
            if ((!StrCompare("/usr/", resolve, 5, 0)) OR
                (!StrCompare("/opt/", resolve, 5, 0)) OR
                (!StrCompare("/bin/", resolve, 5, 0))) {
               FMSG("IdentifyFile","Checking for +x permissions on file %s", resolve);
               UBYTE filename[MAX_FILENAME];
               if (!get_file_info(resolve, &info, sizeof(info), filename, sizeof(filename))) {
                  if (info.Flags & RDF_FILE) {
                     if (info.Permissions & PERMIT_ALL_EXEC) {
                        FMSG("IdentifyFile","Path carries +x permissions");
                        *ClassID = ID_TASK;
                     }
                  }
               }
            }
            else FMSG("IdentifyFile","Path is not supported for +x checks.");
            FreeResource(resolve);
         }
         else FMSG("IdentifyFile","Failed to resolve location '%s'", Path);
      }
      else FMSG("IdentifyFile","No +x support for platform '%s'", state->Platform);
   }
   else FMSG("IdentifyFile","Skipping checks for +x permission flags.");

   // If the file is an executable, return a clone of the location path

   if (*ClassID IS ID_TASK) {
      if (!AllocMemory(StrLength(Path)+3, MEM_STRING, (APTR *)&res_path, NULL)) {
         *Command = res_path;
         *res_path++ = '"';
         while (*Path) *res_path++ = *Path++;
         *res_path++ = '"';
         *res_path = 0;
         LogBack();
         return ERR_Okay;
      }
      else {
         LogError(ERH_IdentifyFile, ERR_AllocMemory);
         LogBack();
         return ERR_AllocMemory;
      }
   }

   // Check device names

   if (*ClassID) {
      // Get the name of the class and sub-class so that we can search the datatypes object.

      if ((!SubClassID) OR (!*SubClassID)) {
         get_class_cmd(Mode, glDatatypes, Flags, *ClassID, &cmd);
      }
      else if (get_class_cmd(Mode, glDatatypes, Flags, *SubClassID, &cmd) != ERR_Okay) {
         get_class_cmd(Mode, glDatatypes, Flags, *ClassID, &cmd);
      }

      LogF("IdentifyFile","Class command: %s", cmd);
   }
   else LogF("IdentifyFile","No class was identified for file '%s'.", Path);

   // Scan for customised file associations.  These override the default class settings, so the user can come up with
   // his own personal settings in circumstances where a class association is not suitable.

   if ((filename = get_filename(Path))) {
      LogF("IdentifyFile","Scanning associations config to match: %s", filename);

      if ((entries = glDatatypes->Entries)) {
         CSTRING str;
         for (i=0; i < glDatatypes->AmtEntries; i++) {
            if (StrMatch(entries[i].Key, "Match") != ERR_Okay) continue;

            if (!StrCompare(entries[i].Data, filename, 0, STR_WILDCARD)) {
               if (Flags & IDF_SECTION) cmd = StrClone(entries[i].Section);
               else if (!cfgReadValue(glDatatypes, entries[i].Section, Mode, &str)) {
                  cmd = StrClone(str);
               }
               else if (StrMatch("Open", Mode) != ERR_Okay) {
                  if (!cfgReadValue(glDatatypes, entries[i].Section, "Open", &str)) {
                     cmd = StrClone(str);
                  }
               }
               break;
            }
         }
      }
   }

restart:
   if ((!(Flags & IDF_SECTION)) AND (cmd) AND (!StrCompare("[PROG:", cmd, 6, 0))) { // Command is referencing a program
      STRING str;
      if (!(TranslateCmdRef(cmd, &str))) {
         FreeResource(cmd);
         cmd = str;
      }
      else {
         LogF("@IdentifyFile","Reference to program '%s' is invalid.", buffer);
         FreeResource(cmd);
         cmd = NULL;
         goto exit; // Abort if the program is not available
      }
   }

host_platform:
#ifdef _WIN32
   FMSG("IdentifyFile","Windows execution process...");

   if ((!cmd) AND (!(Flags & (IDF_SECTION|IDF_IGNORE_HOST)))) { // Check if Windows supports the file type
      if (!ResolvePath(Path, RSF_APPROXIMATE, &res_path)) {
         UBYTE buffer[300];

         if (!StrCompare("http:", res_path, 5, NULL)) { // HTTP needs special support
            if (winReadRootKey("http\\shell\\open\\command", NULL, buffer, sizeof(buffer))) {
               i = StrSearch("%1", buffer, STR_MATCH_CASE);
               if (i != -1) {
                  StrInsert("[@file]", buffer, sizeof(buffer), i, 2);
                  cmd = StrClone(buffer);
               }
               else {
                  for (i=0; buffer[i]; i++);
                  StrCopy(" \"[@file]\"", buffer+i, sizeof(buffer)-i);
                  cmd = StrClone(buffer);
               }
            }
         }
         else {
            UBYTE key[300];
            CSTRING ext = get_extension(res_path);

            if ((ext) AND ((i = winReadRootKey(ext-1, NULL, key, sizeof(key))))) {
               StrCopy("\\Shell\\Open\\Command", key+i, sizeof(key)-i);

               if (winReadRootKey(key, NULL, buffer, sizeof(buffer))) {
                  i = StrSearch("%1", buffer, STR_MATCH_CASE);
                  if (i != -1) {
                     StrInsert("[@file]", buffer, sizeof(buffer), i, 2);
                  }
                  else {
                     for (i=0; buffer[i]; i++);
                     StrCopy(" \"[@file]\"", buffer+i, sizeof(buffer)-i);
                  }

                  // Use of %systemroot% is common

                  i = StrSearch("%SystemRoot%", buffer, 0);
                  if (i != -1) {
                     UBYTE sysroot[100];
                     sysroot[0] = 0;
                     if (!winReadKey("\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion", "SystemRoot", sysroot, sizeof(sysroot))) {
                        if (!winReadKey("\\SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", "SystemRoot", sysroot, sizeof(sysroot))) {
                           // Failure
                        }
                     }

                     if (sysroot[0]) StrInsert(sysroot, buffer, sizeof(buffer), i, 12);
                  }

                  // Check if an absolute path was given.  If not, we need to resolve the .exe to its absolute path.

                  if ((buffer[0]) AND (buffer[1] IS ':')); // Path is absolute
                  else {
                     STRING abs;
                     LONG end, start;
                     ERROR reserror;
                     UBYTE save;

                     if (*buffer IS '"') {
                        start = 1;
                        for (end=0; (buffer[end]) AND (buffer[end] != '"'); end++);
                     }
                     else {
                        start = 0;
                        for (end=0; buffer[end] > 0x20; end++);
                     }

                     save = buffer[end];
                     buffer[end] = 0;
                     reserror = ResolvePath(buffer+start, RSF_PATH, &abs);
                     buffer[end] = save;

                     if (!reserror) {
                        StrInsert(abs, buffer, sizeof(buffer), start, end-start);
                        FreeResource(abs);
                     }
                  }

                  cmd = StrClone(buffer);
               }
               else FMSG("IdentifyFile","Failed to read key %s", key);
            }
            else FMSG("IdentifyFile","Windows has no mapping for extension %s", ext);

            if (!cmd) {
               if (!winGetCommand(res_path, buffer, sizeof(buffer))) {
                  for (i=0; buffer[i]; i++);
                  StrCopy(" \"[@file]\"", buffer+i, sizeof(buffer)-i);
                  cmd = StrClone(buffer);
               }
               else LogF("IdentifyFile","Windows cannot identify path: %s", res_path);
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
      else if ((!cfgReadValue(glDatatypes, "default", Mode, &str)) AND (str)) {
         cmd = StrClone(str);
         goto restart;
      }
   }

   *Command = cmd;

exit:
   FMSG("IdentifyFile","File belongs to class $%.8x", *ClassID);
   LogBack();
   if ((!*ClassID) AND (!cmd)) return ERR_Search;
   else return ERR_Okay;
}

//****************************************************************************
// Scan the class database to extract the correct name for ClassID.  Then scan the Associations object for an entry
// that is registered against the given class type.

ERROR get_class_cmd(CSTRING Mode, objConfig *Associations, LONG Flags, CLASSID ClassID, STRING *Command)
{
   if ((!ClassID) OR (!Command) OR (!Associations)) return LogError(ERH_IdentifyFile, ERR_NullArgs);

   struct ClassItem *item = find_class(ClassID);

   if (item) {
      struct ConfigEntry *entries;
      if ((entries = Associations->Entries)) {
         LONG i;
         for (i=0; i < Associations->AmtEntries; i++) {
            if (!StrMatch(entries[i].Key, "Class")) {
               if (!StrMatch(entries[i].Data, item->Name)) {
                  CSTRING str;
                  if (Flags & IDF_SECTION) *Command = StrClone(entries[i].Section);
                  else if (!cfgReadValue(Associations, entries[i].Section, Mode, &str)) *Command = StrClone(str);
                  if (!*Command) return ERR_AllocMemory;
                  else return ERR_Okay;
               }
            }
         }
      }
   }

   return ERR_Search;
}
