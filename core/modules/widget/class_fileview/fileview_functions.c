
static void add_file_item(objFileView *Self, objXML *XML, struct FileInfo *Info)
{
   if (!Self->ShowHidden) {
      if (Info->Flags & RDF_HIDDEN) return;
   }

   FMSG("add_file_item()","%s, $%.8x", Info->Name, Info->Flags);

   //DebugState(FALSE, -1);

   LONG i;
   CSTRING user, group;
   char buffer[1024], total_size[50], free_space[50], filename[256], strpermissions[24];
   buffer[0] = 0;
   LONG len = 0;
   if (Info->Flags & RDF_VOLUME) { // Get the icon to use for displaying the volume
      if (!(Info->Flags & RDF_HIDDEN)) {
         CSTRING icon = get_file_icon(Info->Name);

         // Determine the size and available space on the device

         total_size[0] = 0;
         free_space[0] = 0;
         objStorageDevice *device;

         if (!CreateObject(ID_STORAGEDEVICE, 0, &device,
               FID_Volume|TSTR, Info->Name,
               TAGEND)) {
            if (device->DeviceFlags & (DEVICE_FLOPPY_DISK|DEVICE_HARD_DISK|DEVICE_COMPACT_DISC)) {
               if (device->DeviceSize >= 1) StrFormat(total_size, sizeof(total_size), "%014.0f", (DOUBLE)device->DeviceSize);
               if (device->BytesFree >= 1) StrFormat(free_space, sizeof(free_space), "%014.0f", (DOUBLE)device->BytesFree);
            }
         }

         STRING label = NULL;
         if (Info->Tags) {
            // Check for a label
            for (i=0; Info->Tags[i]; i++) {
               if (!StrCompare("LABEL:", Info->Tags[i], 6, 0)) {
                  label = Info->Tags[i] + 6;
               }
            }
         }

         if (label) {
            len = StrFormat(buffer, sizeof(buffer), "<dir icon=\"%s\" sort=\"\001%s\" insensitive name=\"%s\">%s (%s)<totalsize>%s</><freespace>%s</></dir>", icon, Info->Name, Info->Name, Info->Name, label, total_size, free_space);
         }
         else len = StrFormat(buffer, sizeof(buffer), "<dir icon=\"%s\" sort=\"\001%s\" insensitive>%s<totalsize>%s</><freespace>%s</></dir>", icon, Info->Name, Info->Name, total_size, free_space);

         FreeMemory(icon);
      }
   }
   else if (Info->Flags & RDF_FOLDER) {
      i = StrFormat(buffer, sizeof(buffer), "%s%s", Self->Path, Info->Name);

      CSTRING icon = get_file_icon(buffer);

      struct DateTime *time = &Info->Modified;

      if (buffer[i-1] IS '/') {
         len = StrFormat(buffer, sizeof(buffer), "<dir icon=\"%s\" sort=\"\001%s\" name=\"%s\" insensitive>", icon, Info->Name, Info->Name);
      }
      else len = StrFormat(buffer, sizeof(buffer), "<dir icon=\"%s\" sort=\"\001%s\" name=\"%s/\" insensitive>", icon, Info->Name, Info->Name);

      // Determine what display name we're going to use

      if (!(Self->View->Flags & VWF_NO_ICONS)) {
         for (i=0; Info->Name[i]; i++);
         if ((i > 0) AND (Info->Name[i-1] IS '/')) Info->Name[i-1] = 0;
      }

      if (time->Year) {
         len += StrFormat(buffer+len, sizeof(buffer)-len, "%s<date sort=\"D%012.0f\">%04d%02d%02d %02d:%02d:%02d</>", Info->Name, (DOUBLE)Info->TimeStamp, time->Year, time->Month, time->Day, time->Hour, time->Minute, time->Second);
      }
      else len += StrFormat(buffer+len, sizeof(buffer)-len, "%s", Info->Name);

      convert_permissions(Info->Permissions, strpermissions);
      if (!(user = ResolveUserID(Info->UserID))) user = "";
      if (!(group = ResolveGroupID(Info->GroupID))) group = "";
      len += StrFormat(buffer+len, sizeof(buffer)-len, "<owner>%s</><group>%s</><permissions>%s</></dir>", user, group, strpermissions);

      FreeMemory(icon);
   }
   else if (Info->Flags & RDF_FILE) {
      if (Self->Flags & FVF_NO_FILES) return;

      LONG display;
      if (Self->Filter[0]) {
         if (!StrCompare(Self->Filter, Info->Name, 0, STR_WILDCARD)) display = TRUE;
         else display = FALSE;
      }
      else display = TRUE;

      if (display) {
         StrFormat(buffer, sizeof(buffer), "%s%s", Self->Path, Info->Name);
         CSTRING icon = get_file_icon(buffer);

         if (Self->Flags & FVF_NO_EXTENSIONS) {
            StrCopy(Info->Name, filename, sizeof(filename));
            strip_extension(filename);
            len = StrFormat(buffer, sizeof(buffer), "<file icon=\"%s\" name=\"%s\">%s<size sort=\"%014.0f\">%.0f</><date sort=\"F%012.0f\">%04d%02d%02d %02d:%02d:%02d</>",
               icon, Info->Name, filename, (DOUBLE)Info->Size, (DOUBLE)Info->Size, (DOUBLE)Info->TimeStamp, Info->Modified.Year, Info->Modified.Month, Info->Modified.Day, Info->Modified.Hour, Info->Modified.Minute, Info->Modified.Second);
         }
         else len = StrFormat(buffer, sizeof(buffer), "<file icon=\"%s\" name=\"%s\">%s<size sort=\"%014.0f\">%.0f</><date sort=\"F%012.0f\">%04d%02d%02d %02d:%02d:%02d</>",
            icon, Info->Name, Info->Name, (DOUBLE)Info->Size, (DOUBLE)Info->Size, (DOUBLE)Info->TimeStamp, Info->Modified.Year, Info->Modified.Month, Info->Modified.Day, Info->Modified.Hour, Info->Modified.Minute, Info->Modified.Second);

         convert_permissions(Info->Permissions, strpermissions);
         if (!(user = ResolveUserID(Info->UserID))) user = "";
         if (!(group = ResolveGroupID(Info->GroupID))) group = "";
         len += StrFormat(buffer+len, sizeof(buffer)-len, "<owner>%s</><group>%s</><permissions>%s</></file>", user, group, strpermissions);

         FreeMemory(icon);
      }
   }

   if (buffer[0]) acDataXML(XML, buffer);

   //DebugState(TRUE, -1);
}

//****************************************************************************

static ERROR rename_file_item(objFileView *Self, CSTRING Name, CSTRING NewName)
{
   if (Self->Watch) return ERR_Okay;

   LogMsg("Rename %s to %s", Name, NewName);

   struct XMLTag *tag;
   if ((tag = find_tag(Name, Self->View->XML->Tags[0]))) {
      LONG i = StrLength(NewName);
      UBYTE buffer[i+2];

      if (!StrMatch("dir", tag->Attrib->Name)) {
         StrCopy(NewName, buffer, COPY_ALL);
         buffer[i++] = '/';
         buffer[i] = 0;
         NewName = buffer;
      }

      // Update the real name

      for (i=1; i < tag->TotalAttrib; i++) {
         if (!StrMatch("name", tag->Attrib[i].Name)) {
            LONG index = tag->Index;
            xmlSetAttrib(Self->View->XML, tag->Index, 0, 0, NewName);
            tag = Self->View->XML->Tags[index];
            break;
         }
      }

      // Update the name by setting the content string

      xmlSetAttrib(Self->View->XML, tag->Child->Index, 0, 0, NewName);

      acSort(Self->View);
      acRefresh(Self->View);
   }

   return ERR_Okay;
}

//****************************************************************************

static void strip_extension(STRING String)
{
   LONG i;
   for (i=0; String[i]; i++);
   while ((i > 0) AND (String[i] != '.')) i--;
   if (i > 0) String[i] = 0;
}

//****************************************************************************

static STRING extract_filename(struct XMLTag *Tag)
{
   if (!Tag) return NULL;

   LONG i;
   for (i=1; i < Tag->TotalAttrib; i++) {
      if (!StrMatch("name", Tag->Attrib[i].Name)) return Tag->Attrib[i].Value;
   }

   if (Tag->Child) return Tag->Child->Attrib->Value;

   return NULL;
}

//******************************************************************************
// Returns the XML tag for a given file or folder name.

static struct XMLTag * find_tag(CSTRING Name, struct XMLTag *List)
{
   struct XMLTag *tag;

   for (tag=List; tag; tag=tag->Next) {
      // Compare against the content tag first.  This is the quickest method so long as there is no trailing text attached.

      if (tag->Child) {
         if (!StrCompare(Name, tag->Child->Attrib->Value, 0, STR_MATCH_CASE|STR_MATCH_LEN)) {
            return tag;
         }
      }

      LONG i;
      for (i=1; i < tag->TotalAttrib; i++) {
         if (!StrMatch("name", tag->Attrib[i].Name)) {
            if (!StrCompare(Name, tag->Attrib[i].Value, 0, STR_MATCH_CASE|STR_MATCH_LEN)) {
               return tag;
            }
            break;
         }
      }
   }

   return NULL;
}

//****************************************************************************
// Launches a separate process for pasting files to a destination.

static ERROR paste_to(objFileView *Self, CSTRING Folder, LONG Cluster)
{
   ERROR error;

   LogF("~paste_to()","Cluster: %d, %s", Cluster, Folder);

#ifdef EXTERNAL_CLIP
   UBYTE args[512];
   LONG i;
   if ((i = StrFormat(args, sizeof(args), "commands:pastefiles.dml \"dest=%s\"", Folder)) < sizeof(args)-1) {
      if (Cluster) i += StrFormat(args+i, sizeof(args)-i, " cluster=%d", Cluster);

      if (i < sizeof(args)-1) {
         OBJECTPTR run;
         if (!(error = CreateObject(ID_RUN, NF_INTEGRAL, &run,
               FID_Location|TSTR, "bin:parasol-gui",
               FID_Args|TSTR,     args,
               TAGEND))) {
            error = acActivate(run);
            acFree(run);
         }
      }
      else error = ERR_BufferOverflow;
   }
   else error = ERR_BufferOverflow;
#else
   error = ERR_NoSupport;
#endif

   if (error != ERR_Okay) {
      // If the pastefiles script failed, resort to direct clipboard access

      objClipboard *clipboard;
      if (!CreateObject(ID_CLIPBOARD, 0, &clipboard, TAGEND)) {
         if (Cluster) {
            LONG oldcluster = clipboard->ClusterID;
            clipboard->ClusterID = Cluster;
            error = ActionTags(MT_ClipPasteFiles, clipboard, Folder, NULL);
            clipboard->ClusterID = oldcluster;
         }
         else error = ActionTags(MT_ClipPasteFiles, clipboard, Folder, NULL);

         acFree(clipboard);
      }

      if (!Self->Watch) acRefresh(Self);
   }

   LogBack();
   return error;
}

//****************************************************************************
// Translates permission flags into a readable string.

static void convert_permissions(LONG Permissions, STRING Output)
{
   if (Permissions & PERMIT_READ)  *Output++ = 'r'; else *Output++ = '-';
   if (Permissions & PERMIT_WRITE) *Output++ = 'w'; else *Output++ = '-';
   if (Permissions & PERMIT_EXEC)  *Output++ = 'x'; else *Output++ = '-';

   *Output++ = ' ';

   if (Permissions & PERMIT_GROUP_READ)  *Output++ = 'r'; else *Output++ = '-';
   if (Permissions & PERMIT_GROUP_WRITE) *Output++ = 'w'; else *Output++ = '-';
   if (Permissions & PERMIT_GROUPID) *Output++ = 'g';
   else if (Permissions & PERMIT_GROUP_EXEC) *Output++ = 'x'; else *Output++ = '-';

   *Output++ = ' ';

   if (Permissions & PERMIT_OTHERS_READ)  *Output++ = 'r'; else *Output++ = '-';
   if (Permissions & PERMIT_OTHERS_WRITE) *Output++ = 'w'; else *Output++ = '-';
   if (Permissions & PERMIT_OTHERS_EXEC)  *Output++ = 'x'; else *Output++ = '-';

   if (Permissions & PERMIT_USERID) { *Output++ = ' '; *Output++ = 's'; }

   *Output++ = 0;
}

//****************************************************************************

static void error_dialog(objFileView *Self, CSTRING Title, CSTRING Message)
{
   OBJECTPTR confirmdialog;

   LogF("~error_dialog()", "%s", Message);

   if (!CreateObject(ID_DIALOG, NF_INTEGRAL, &confirmdialog,
         FID_Type|TLONG,    DT_ERROR,
         FID_Options|TSTR,  "OK:Okay",
         FID_Title|TSTR,    Title,
         FID_String|TSTR,   Message,
         FID_PopOver|TLONG, Self->WindowID,
         FID_Flags|TLONG,   DF_MODAL,
         TAGEND)) {
      acShow(confirmdialog);
   }

   LogBack();
}

//****************************************************************************

static ERROR path_watch(objFile *File, CSTRING Path, LARGE Custom, LONG Flags)
{
   UBYTE buffer[512];
   LONG i;

   objFileView *Self = (objFileView *)CurrentContext();

   // Do nothing if the fileview is currently in the process of refreshing itself.

   if (Self->Refresh) return ERR_Okay;

   BYTE refresh = FALSE;
   BYTE sort = FALSE;

   if (!Flags) {
      // If no flags were specified, a change has occurred but the host is unable to tell us exactly what happened.

      Self->Flags |= FVF_TOTAL_REFRESH;
      acRefresh(Self);
      return ERR_Okay;
   }

   if (!Path) {
      // If no file is given, the monitored folder was affected by something.

      if ((Flags & MFF_DELETE) AND (Flags & MFF_SELF)) {
         LogF("~path_watch()","[Parent deleted] %s", Self->Path);

         if (!StrCompare("cd", Self->Path, 2, 0));
         else if (!StrCompare("disk", Self->Path, 2, 0));
         else {
            // Check that the folder is actually gone and that this is not a mis-report.

            if (AnalysePath(Self->Path, 0) != ERR_Okay) {
               SetString(Self, FID_Path, "");
            }
         }

         LogBack();
      }
      else if (Flags & MFF_UNMOUNT) {
         // If the host filesystem has been unmounted, change the location back to the root view because the file view
         // has become invalid.

         LogF("~path_watch()","[Unmounted] %s", Self->Path);
         if (!StrCompare("cd", Self->Path, 2, 0));
         else if (!StrCompare("disk", Self->Path, 2, 0));
         else if (AnalysePath(Self->Path, 0) != ERR_Okay) {
            SetString(Self, FID_Path, "");
         }
         LogBack();
      }

      return ERR_Okay;
   }

   struct XMLTag *scan, *tag;
   struct FileInfo info;
   ERROR infoerror;
   if (!(Flags & MFF_DELETE)) {
      if (Path) i = StrFormat(buffer, sizeof(buffer), "%s%s", Self->Path, Path);
      else i = StrCopy(Self->Path, buffer, sizeof(buffer));

      infoerror = GetFileInfo(buffer, &info);
      if (!infoerror) {
         if ((Self->Flags & FVF_NO_FILES) AND (info.Flags & RDF_FILE)) {
            return ERR_Okay;
         }

         // Get the true file name with the trailing slash if it is a directory (important for symbolically linked directories).

         Path = info.Name;
      }
   }
   else infoerror = ERR_Failed;

   objXML *xml = Self->View->XML;
//   taglist = xml->Tags;
//   tag = xml->Tags[0];

   if (Flags & MFF_CREATE) {
      LogF("~path_watch()","Create: %s", Path);

      if (infoerror IS ERR_Okay) {
         add_file_item(Self, xml, (APTR)&info);
         refresh = TRUE;
         sort = TRUE;
      }

      if (!refresh) LogF("@FileView:","File '%s' does not exist.", Path);
   }
   else if (Flags & MFF_DELETE) {
      LogF("~path_watch()","Delete: %s", Path);
      refresh = delete_item(xml->Tags[0], Self->View, Path);
   }
   else if (Flags & MFF_MOVED) {
      // Determine if the file has been moved in or moved out

      if (infoerror IS ERR_Okay) {
         // It is possible that the file being moved could replace an already existing file in the view (i.e. rename
         // operation), so we do a tag-check first.

         LogF("~path_watch()","Moved-In: %s", Path);

         if (find_tag(Path, xml->Tags[0])) {
            delete_item(xml->Tags[0], Self->View, Path);
         }

         add_file_item(Self, xml, (APTR)&info);
         refresh = TRUE;
         sort = TRUE;
      }
      else {
         LogF("~path_watch()","Moved-Out: %s", Path);
         refresh = delete_item(xml->Tags[0], Self->View, Path);
      }
   }
   else if ((Flags & (MFF_ATTRIB|MFF_CLOSED)) AND (File)) {
      // Get the new file attributes and update the existing entry

      STRING fname;

      LogF("~path_watch()","Attrib: %s", Path);

      if (!infoerror) {
         for (tag=xml->Tags[0]; tag; tag=tag->Next) {
            fname = extract_filename(tag);
            if (!StrCompare(Path, fname, 0, STR_MATCH_LEN|STR_MATCH_CASE)) {
               LogF("path_watch:","Entry found (%s / %s), updating attributes.", Path, fname);
               // Update the size and date tags

               for (scan=tag->Child; scan; scan=scan->Next) {
                  if (!StrMatch("date", scan->Attrib->Name)) {
                     for (i=1; i < scan->TotalAttrib; i++) {
                        if (!StrMatch("sort", scan->Attrib[i].Name)) {
                           StrFormat(buffer, sizeof(buffer), "%c" PF64() "", (info.Flags & RDF_FOLDER) ? 'D' : 'C', info.TimeStamp);
                           LONG j = scan->Index;
                           xmlSetAttrib(xml, j, i, 0, buffer);
                           //ActionTags(MT_SetViewItem, view, -1, scan->Index, "date", "sort", buffer);

                           scan = xml->Tags[j]; // Regain the address because SetXMLAttrib() invalidates it
                           if (scan->Child) {
                              StrFormat(buffer, sizeof(buffer), "%04d%02d%02d %02d:%02d:%02d", info.Modified.Year, info.Modified.Month, info.Modified.Day, info.Modified.Hour, info.Modified.Minute, info.Modified.Second);
                              xmlSetAttrib(xml, scan->Child->Index, 0, 0, buffer);
                              //ActionTags(MT_SetViewItem, view, -1, scan->Child->Index, 0, 0, buffer);
                              scan = xml->Tags[j];
                           }
                        }
                     }
                  }
                  else if (!StrMatch("size", scan->Attrib->Name)) {
                     for (i=1; i < scan->TotalAttrib; i++) {
                        if (!StrMatch("sort", scan->Attrib[i].Name)) {
                           StrFormat(buffer, sizeof(buffer), "%014.0f", (DOUBLE)info.Size);
                           LONG j = scan->Index;
                           xmlSetAttrib(xml, j, i, 0, buffer);
                           //ActionTags(MT_SetViewItem, view, -1, scan->Index, "size", "sort", buffer);

                           scan = xml->Tags[j]; // Regain the address because SetXMLAttribute() invalidates it
                           if (scan->Child) {
                              StrFormat(buffer, sizeof(buffer), "%.0f", (DOUBLE)info.Size);
                              xmlSetAttrib(xml, scan->Child->Index, 0, 0, buffer);
                              //ActionTags(MT_SetViewItem, view, -1, scan->Child->Index, 0, 0, buffer);
                           }
                        }
                     }
                  }
               }

               refresh = TRUE;
               sort = TRUE;
               break;
            }
         }

         if (!refresh) {
            // File not in list - add it
            LogF("path_watch:","File \"%s\" not in list - adding it...", Path);
            add_file_item(Self, xml, &info);
            refresh = TRUE;
            sort = TRUE;
         }
      }
      else {
         LogF("@path_watch:","Attrib change misreported - file does not exist.");
         refresh = delete_item(xml->Tags[0], Self->View, Path);
      }
   }

   if (sort)    acSort(Self->View);
   if (refresh) acRefresh(Self->View);

   LogBack();
   return ERR_Okay;
}

//****************************************************************************

static void key_event(objFileView *Self, evKey *Event, LONG Size)
{
   if (!(Event->Qualifiers & KQ_PRESSED)) return;

   if ((Self->Flags & FVF_SYS_KEYS) AND (Event->Qualifiers & KQ_CTRL)) {
      // Note: Syskeys should only be enabled if cut/copy/paste keyboard support is not implemented elsewhere,
      // such as menu items.

      switch(Event->Code) {
         case K_C: Action(MT_FvCopyFiles, Self, NULL); break;
         case K_X: Action(MT_FvCutFiles, Self, NULL); break;
         case K_V: Action(MT_FvPasteFiles, Self, NULL); break;
         case K_A: viewSelectAll(Self->View); break;
      }
   }
   else switch(Event->Code) {
       case K_DELETE: Action(MT_FvDeleteFiles, Self, NULL); break;
   }
}

//****************************************************************************

static BYTE delete_item(struct XMLTag *Tags, objView *view, CSTRING File)
{
   struct XMLTag *tag;

   if (Tags) {
      for (tag=Tags; (tag); tag = tag->Next) {
         if (!tag->Child) continue;

         if (!StrCompare(File, extract_filename(tag), 0, STR_MATCH_LEN|STR_MATCH_CASE)) {
            LogMsg("Detected deleted file \"%s\"", File);
            viewRemoveTag(view, tag->Index, 1);
            return TRUE;
         }
      }

      // The deleted file may actually be a directory - this is especially true of symbolic links that are linked to folders.

      UBYTE buffer[256];
      LONG i = StrCopy(File, buffer, sizeof(buffer)-1);
      if (buffer[i-1] != '/') {
         buffer[i++] = '/';
         buffer[i++] = 0;

         for (tag=Tags; (tag); tag = tag->Next) {
            if (!tag->Child) continue;

            if (!StrCompare(buffer, extract_filename(tag), 0, STR_MATCH_LEN|STR_MATCH_CASE)) {
               LogMsg("Detected deleted file \"%s\"", buffer);
               viewRemoveTag(view, tag->Index, 1);
               return TRUE;
            }
         }
      }
   }

   LogErrorMsg("I have no record of deleted file '%s'", File);
   return FALSE;
}

//****************************************************************************
// Check if there is a document tag associated with the current path.

static void check_docview(objFileView *Self)
{
   if (Self->ShowDocs) {
      CSTRING docfile;
      CSTRING path = Self->Path;
      if (!path[0]) path = ":";
      if ((docfile = GetDocView(path))) {
         LogBranch("Using folder presentation file: %s", docfile);

         if (!Self->Doc) {
            if (!NewObject(ID_DOCUMENT, NF_INTEGRAL, &Self->Doc)) {
               SetFields(Self->Doc,
                  FID_Location|TSTR, docfile,
                  FID_Surface|TLONG, Self->View->Layout->SurfaceID,
                  FID_Flags|TLONG,   DCF_UNRESTRICTED,
                  TAGEND);

               SetPointer(Self->View, FID_Document, Self->Doc);

               char buffer[32];
               IntToStr(Self->Head.UniqueID, buffer, sizeof(buffer));
               acSetVar(Self->Doc, "FileView", buffer);
            }
         }
         else {
            SetString(Self->Doc, FID_Location, docfile);
            SetPointer(Self->View, FID_Document, Self->Doc);
         }

         LogBack();
      }
   }
   else SetPointer(Self->View, FID_Document, NULL);
}

//****************************************************************************
// Executes a Run object with a given Mode.  All files listed in the Tags argument will be passed to the Run object as
// a mass execution operation.

static ERROR open_files(objFileView *Self, LONG *Tags, CSTRING Mode)
{
   if (!Tags) return ERR_Okay;

   if ((!Self->Path[0]) OR (Self->Path[0] IS ':')) {
      LogBack();
      return ERR_Okay;
   }

   LONG i;
   LONG size = 0;
   LONG len = StrLength(Self->Path);
   for (i=0; Tags[i] != -1; i++) {
      size += len + StrLength(extract_filename(Self->View->XML->Tags[Tags[i]])) + 4;
   }

   STRING buffer;
   if (!AllocMemory(size, MEM_STRING|MEM_NO_CLEAR, &buffer, NULL)) {
      LONG pos = 0;
      for (i=0; Tags[i] != -1; i++) {
         if (i > 0) buffer[pos++] = ',';
         pos += StrFormat(buffer+pos, size - pos, "\"%s%s\"", Self->Path, extract_filename(Self->View->XML->Tags[Tags[i]]));
      }

      OBJECTPTR run;
      if (!CreateObject(ID_RUN, 0, &run,
            FID_Mode|TSTR,     Mode, // Open, Edit, View
            FID_Location|TSTR, buffer,
            TAGEND)) {
         acActivate(run);
         acFree(run);

         FreeMemory(buffer);
         return ERR_Okay;
      }
      else {
         FreeMemory(buffer);
         return ERR_CreateObject;
      }
   }
   else return ERR_AllocMemory;
}

//****************************************************************************
// Load user preferences for the file view.

static void load_prefs(void)
{
   objConfig *config;
   CSTRING str;

   if (!CreateObject(ID_CONFIG, 0, &config,
         FID_Location|TSTR, "user:config/filesystem.cfg",
         TAGEND)) {

      if (!cfgReadValue(config, "FileView", "RenameReplace", &str)) {
         if (StrToInt(str) IS 1) glRenameReplace = TRUE;
         else glRenameReplace = FALSE;
      }

      if (!cfgReadValue(config, "FileView", "AllowDocuments", &str)) {
         if (StrToInt(str) IS 0) glShowDocs = FALSE;
         else glShowDocs = TRUE;
      }

      if (!cfgReadValue(config, "FileView", "ShowHidden", &str)) {
         if (StrToInt(str) IS 0) glShowHidden = FALSE;
         else glShowHidden = TRUE;
      }

      if (!cfgReadValue(config, "FileView", "ShowSystem", &str)) {
         if (StrToInt(str) IS 0) glShowSystem = FALSE;
         else glShowSystem = TRUE;
      }

      acFree(config);
   }
}

//****************************************************************************

static void event_volume_created(OBJECTID *FileViewID, evAssignCreated *Info, LONG InfoSize)
{
   struct FileInfo info;
   objFileView *fileview;

   if (!AccessObject(*FileViewID, 3000, &fileview)) {
      if ((fileview->Path[0] IS ':') OR (!fileview->Path[0])) {
         LogMsg("New volume '%s' created.", Info->Name);

         UBYTE buffer[80];
         StrFormat(buffer, sizeof(buffer), "%s:", (STRING)Info->Name);

         if (!find_tag(buffer, fileview->View->XML->Tags[0])) {
            // The volume does not exist in our list, so add it
            if (!GetFileInfo(buffer, &info)) {
               if (!(info.Flags & RDF_HIDDEN)) {
                  add_file_item(fileview, fileview->View->XML, &info);
                  acSort(fileview->View);
                  acRefresh(fileview->View);
               }
            }
         }
      }
      ReleaseObject(fileview);
   }
}

//****************************************************************************

static void event_volume_deleted(OBJECTID *FileViewID, evAssignDeleted *Info, LONG InfoSize)
{
   objFileView *fileview;

   if (!AccessObject(*FileViewID, 3000, &fileview)) {
      if ((fileview->Path[0] IS ':') OR (!fileview->Path[0])) {
         UBYTE buffer[80];
         StrFormat(Info->Name, sizeof(buffer), "%s:", Info->Name);
         if (delete_item(fileview->View->XML->Tags[0], fileview->View, buffer)) {
            acRefresh(fileview->View);
         }
      }
      ReleaseObject(fileview);
   }
}

//****************************************************************************

static void report_event(objFileView *Self, LONG Event)
{
   LogF("~report_event()","$%.8x", Event);

   if (Event & Self->EventMask) {
      if (Self->EventCallback.Type) {
          if (Self->EventCallback.Type IS CALL_STDC) {
            void (*routine)(objFileView *, LONG);
            OBJECTPTR context = SetContext(Self->EventCallback.StdC.Context);
               routine = (void (*)(objFileView *, LONG)) Self->EventCallback.StdC.Routine;
               routine(Self, Event);
            SetContext(context);
         }
         else if (Self->EventCallback.Type IS CALL_SCRIPT) {
            OBJECTPTR script;
            if ((script = Self->EventCallback.Script.Script)) {
               const struct ScriptArg args[] = {
                  { "FileView", FD_OBJECTPTR, { .Address = Self } },
                  { "Event",    FD_LONG,      { .Long = Event } }
               };
               scCallback(script, Self->EventCallback.Script.ProcedureID, args, ARRAYSIZE(args));
            }
         }
      }
   }

   LogBack();
}

//****************************************************************************

static void response_rename(objDialog *Dialog, LONG Response)
{
   LogF("~response_rename()","Response %d", Response);

   if (Response & RSF_POSITIVE) {
      OBJECTPTR file;
      UBYTE src[512], dest[512], dir;
      ERROR error;

      objFileView *Self = (objFileView *)CurrentContext();

      STRING errorstr = NULL;

      if (!GetVar(Dialog, "Dest", dest, sizeof(dest))) {
         if (!GetVar(Dialog, "Src", src, sizeof(src))) {
            if (!MoveFile(src, dest)) {
               DelayMsg(AC_Refresh, Self->Head.UniqueID, NULL);
            }
            else errorstr = "The rename operation failed.";
         }
      }
      else {
         if (GetVar(Dialog, "Src", src, sizeof(src)) != ERR_Okay) src[0] = 0;

         STRING newname;
         GetString(Dialog, FID_UserInput, &newname);

         LONG i = StrCopy(src, dest, sizeof(dest));
         if ((dest[i-1] IS '/') OR (dest[i-1] IS ':') OR (dest[i-1] IS '\\')) {
            i--; // The source being renamed is a directory
            dir = TRUE;
         }
         else dir = FALSE;

         while ((i > 0) AND (dest[i-1] != '/') AND (dest[i-1] != '\\') AND (dest[i-1] != ':')) i--;
         LONG fstart = i;
         i += StrCopy(newname, dest+i, sizeof(dest)-i-1);
         if (dir) {
            dest[i++] = '/';
            dest[i] = 0;
         }

         LogMsg("Rename: %s TO %s, Dir: %d", src, dest, dir);

         if (StrCompare(src, dest, 0, STR_MATCH_LEN|STR_MATCH_CASE) != ERR_Okay) {
            STRING path;
            if (!(error = ResolvePath(dest, RSF_NO_FILE_CHECK|RSF_CASE_SENSITIVE, &path))) { // Use ResolvePath() to prevent problems with multi-directory volumes
               error = AnalysePath(path, 0);
               FreeMemory(path);
            }

            if (!error) {
               // The destination exists

               if (dir) dest[i-1] = 0;

               if (glRenameReplace) {
                  OBJECTPTR confirmdialog;
                  if (!CreateObject(ID_DIALOG, NF_INTEGRAL, &confirmdialog,
                        FID_Type|TLONG,    DT_QUESTION,
                        FID_Options|TSTR,  "CANCEL:No; YES:Yes",
                        FID_Title|TSTR,    "Confirmation Required",
                        FID_String|TSTR,   "A file already exists that uses this name.  Are you sure that you want to overwrite it?",
                        FID_PopOver|TLONG, Self->WindowID,
                        FID_Flags|TLONG,   DF_MODAL,
                        TAGEND)) {
                     acSetVar(confirmdialog, "Src", src);
                     acSetVar(confirmdialog, "Dest", dest);
                     SetFunctionPtr(confirmdialog, FID_Feedback, &response_rename);
                     acShow(confirmdialog);
                  }
               }
               else error_dialog(Self, "Rename Failure", "A file with this name already exists.  Please specify a different file name.");
            }
            else if (!CreateObject(ID_FILE, NF_INTEGRAL, &file,
                       FID_Path|TSTR,   src,
                       FID_Flags|TLONG, FL_READ,
                       TAGEND)) {

               if (!acRename(file, newname)) {
                  rename_file_item(Self, src+fstart, newname);
               }
               else errorstr = "Failed to rename the file.";

               acFree(file);
            }
            else errorstr = "Failed to open file for renaming.";
         }
      }

      if (errorstr) error_dialog(Self, "File Rename Failure", errorstr);
   }

   LogBack();
}

static void response_createdir(objDialog *Dialog, LONG Response)
{
   UBYTE buffer[270];
   LONG i;
   STRING name;

   LogF("~response_createdir()","Response %d", Response);

   if (Response & RSF_POSITIVE) {
      objFileView *Self = (objFileView *)CurrentContext();

      GetVar(Dialog, "Dir", buffer, sizeof(buffer));
      for (i=0; buffer[i]; i++);

      if ((!GetString(Dialog, FID_UserInput, &name)) AND (name) AND (name[0])) {
         i += StrCopy(name, buffer+i, sizeof(buffer)-i);
         buffer[i++] = '/';
         buffer[i] = 0;
         if (!CreateFolder(buffer, 0)) {
            if (!Self->Watch) {
               struct FileInfo info;
               if (!GetFileInfo(buffer, &info)) {
                  add_file_item(Self, Self->View->XML, &info);
                  acSort(Self->View);
                  acRefresh(Self->View);
               }
            }
         }
         else LogErrorMsg("Failed to create dir \"%s\"", buffer);
      }
      else LogErrorMsg("No name provided for dir creation.");
   }
   LogBack();
}

static void response_delete(objDialog *Dialog, LONG Response)
{
   LogF("~response_delete()","Response %d", Response);

   if (Response IS RSF_YES_ALL) {
      objFileView *Self = (objFileView *)CurrentContext();

      struct ChildEntry list[5];
      LONG count = ARRAYSIZE(list);
      ListChildren(Dialog->Head.UniqueID, list, &count);

      OBJECTID configid = 0;
      LONG i;
      for (i=0; i < ARRAYSIZE(list); i++) {
         if (list[i].ClassID IS ID_CONFIG) {
            configid = list[i].ObjectID;
            break;
         }
      }

      objConfig *config;
      if ((configid) AND (!AccessObject(configid, 5000, &config))) {
         ERROR error;

         // Clear any existing file clip records, then add all selected files to the clipboard.

         if (!Self->DeleteClip) {
            if (CreateObject(ID_CLIPBOARD, 0, &Self->DeleteClip,
                  FID_Cluster|TLONG, 0, // Create a clipboard with a new file cluster
                  TAGEND) != ERR_Okay) {
               return;
            }
         }
         else ActionTags(MT_ClipDelete, Self->DeleteClip, CLIPTYPE_FILE);

         for (i=0; i < config->AmtEntries; i++) {
            LogMsg("Delete: %s", config->Entries[i].Data);
            ActionTags(MT_ClipAddFile, Self->DeleteClip, CLIPTYPE_FILE, config->Entries[i].Data, CEF_EXTEND);
         }

         viewSelectNone(Self->View);

#ifdef EXTERNAL_CLIP
         OBJECTPTR run;

         UBYTE buffer[270];
         StrFormat(buffer, sizeof(buffer), "commands:deleteclipfiles.dml cluster=%d", Self->DeleteClip->ClusterID);

         if (!(error = CreateObject(ID_RUN, NF_INTEGRAL, &run,
               FID_Location|TSTR, "bin:parasol-gui",
               FID_Args|TSTR,     buffer,
               TAGEND))) {
            error = acActivate(run);
            acFree(run);
         }
#else
         error = ERR_NoSupport;
#endif
         if (error != ERR_Okay) {
            // If the script failed, resort to direct clipboard access
            error = ActionTags(MT_ClipDeleteFiles, Self->DeleteClip, NULL);
            if (!Self->Watch) acRefresh(Self);
         }

         ReleaseObject(config);
      }
   }

   LogBack();
}

//****************************************************************************

CSTRING get_file_icon(CSTRING Path)
{
   STRING icon = NULL;

   objFile *file;
   if (!CreateObject(ID_FILE, NF_INTEGRAL, &file,
         FID_Path|TSTR, Path,
         TAGEND)) {
      if (!GetString(file, FID_Icon, &icon)) icon = StrClone(icon);
      acFree(file);
   }
   else LogF("@get_file_icon","Failed to get icon for path '%s'", Path);

   if (!icon) return StrClone("folders/folder");
   else return icon;
}

//****************************************************************************
// Returns the surface that is acting as the window.  Returns 0 if no window surface is detected.

static OBJECTID parent_window(OBJECTID SurfaceID)
{
   struct SurfaceControl *ctl;

   if ((ctl = drwAccessList(ARF_READ))) {
      LONG i;
      if ((i = FIND_SURFACE_INDEX(ctl, SurfaceID)) != -1) {
         struct SurfaceList *list = (APTR)ctl + ctl->ArrayIndex + (i * ctl->EntrySize);
         OBJECTID parent_id;
         if ((parent_id = list->ParentID)) {
            while (i >= 0) {
               if (list->SurfaceID IS parent_id) {
                  OBJECTID owner_id = GetOwnerID(list->SurfaceID);
                  LONG class_id = GetClassID(owner_id);
                  if (class_id IS ID_WINDOW) {
                     drwReleaseList(ARF_READ);
                     return parent_id;
                  }
                  parent_id = list->ParentID;
               }

               i--;
               list = (APTR)list - ctl->EntrySize;
            }
         }
      }

      drwReleaseList(ARF_READ);
   }

   return 0;
}

//****************************************************************************

static ERROR fileview_timer(objFileView *Self, LARGE Elapsed, LARGE CurrentTime)
{
   #define BUFSIZE 512
   struct XMLTag *tag, *scan;
   LONG i, j, flags;
   BYTE filecount, restart;
   STRING path, buffer;
   ERROR error;

   // Do nothing if the fileview is currently in the process of refreshing itself.

   if ((Self->Refresh) OR (Self->Watch)) return ERR_Okay;

   if (!Self->Dir) {
      flags = RDF_FILES|RDF_FOLDERS|RDF_QUALIFY|RDF_PERMISSIONS|RDF_DATE|RDF_SIZE|RDF_TAGS;
      if (Self->Flags & FVF_NO_FILES) flags &= ~RDF_FILE; // Do not read files

      if (OpenDir(Self->Path, flags, &Self->Dir) != ERR_Okay) {
         MSG("Failed to open '%s'", Self->Path);
         return ERR_Okay;
      }
   }

   if (AllocMemory(BUFSIZE, MEM_STRING|MEM_NO_CLEAR, &buffer, NULL)) {
      return ERR_AllocMemory;
   }

   FMSG("~","");

   BYTE refresh = FALSE;
   BYTE sort = FALSE;
   objXML *xml = Self->View->XML;

   struct DirInfo *dirinfo = Self->Dir;
   for (filecount=0; filecount < 5; filecount++) {
      if (ScanDir(dirinfo) != ERR_Okay) {
         // We have reached the end of the directory - reset the scan
         CloseDir(Self->Dir);
         Self->Dir = NULL;
         if (Self->ResetTimer) {
            if ((Self->Watch) OR (!Self->Path[0]) OR (Self->Path[0] IS ':')) {
               if (Self->Timer) { UNSUB_TIMER(Self->Timer); Self->Timer = 0; }
            }
            else SUB_TIMER(Self->RefreshRate, &Self->Timer);
         }
         break;
      }

      // Ignore hidden volumes

      struct FileInfo *info = dirinfo->Info;

      if ((info->Flags & RDF_VOLUME) AND (info->Flags & RDF_HIDDEN)) continue;

      if (!xml->TagCount) {
         // The view is empty and this is the first file to appear in the directory.

         add_file_item(Self, xml, info);
         acRefresh(Self->View);
         break;
      }
      else if ((tag = find_tag(info->Name, xml->Tags[0]))) {
         // File found - extract the timestamp and test it against the file.  Note that we don't really need to test
         // any other attributes, because if the user changes the file size or permission details, programs should
         // update the timestamp to reflect those differences (and if it doesn't, to hell with it)

         LARGE timestamp = 0;
         LARGE size = 0;
         for (scan=tag->Child; scan; scan=scan->Next) {
            if (!StrMatch("date", scan->Attrib->Name)) {
               for (i=1; i < scan->TotalAttrib; i++) {
                  if (!StrMatch("sort", scan->Attrib[i].Name)) {
                     timestamp = StrToInt(scan->Attrib[i].Value);
                     break;
                  }
               }
            }
            else if (!StrMatch("size", scan->Attrib->Name)) {
               for (i=1; i < scan->TotalAttrib; i++) {
                  if (!StrMatch("sort", scan->Attrib[i].Name)) {
                     size = StrToInt(scan->Attrib[i].Value);
                     break;
                  }
               }
            }
         }

         if (((timestamp) AND (info->TimeStamp != timestamp)) OR (size != info->Size)) {
            LogMsg("Date/Size change \"%s\" (" PF64() "/" PF64() ", " PF64() "/" PF64() ")", info->Name, info->Size, size, info->TimeStamp, timestamp);

            // Update the size and date tags

            for (scan=tag->Child; scan; scan=scan->Next) {
               if (!StrMatch("date", scan->Attrib->Name)) {
                  for (i=1; i < scan->TotalAttrib; i++) {
                     if (!StrMatch("sort", scan->Attrib[i].Name)) {
                        if (info->Flags & RDF_FOLDER) StrFormat(buffer, BUFSIZE, "D" PF64(), info->TimeStamp);
                        else StrFormat(buffer, BUFSIZE, "F" PF64(), info->TimeStamp);
                        j = scan->Index;
                        xmlSetAttrib(xml, j, i, NULL, buffer);

                        scan = xml->Tags[j]; // Regain the address because SetXMLAttribute() invalidates it
                        if (scan->Child) {
                           StrFormat(buffer, BUFSIZE, "%04d%02d%02d %02d:%02d:%02d", info->Modified.Year, info->Modified.Month, info->Modified.Day, info->Modified.Hour, info->Modified.Minute, info->Modified.Second);
                           xmlSetAttrib(xml, scan->Child->Index, 0, NULL, buffer);
                           scan = xml->Tags[j];
                        }
                     }
                  }
               }
               else if (!StrMatch("size", scan->Attrib->Name)) {
                  for (i=1; i < scan->TotalAttrib; i++) {
                     if (!StrMatch("sort", scan->Attrib[i].Name)) {
                        StrFormat(buffer, BUFSIZE, "%014.0f", (DOUBLE)info->Size);
                        j = scan->Index;
                        xmlSetAttrib(xml, j, i, NULL, buffer);

                        scan = xml->Tags[j]; // Regain the address because SetXMLAttribute() invalidates it
                        if (scan->Child) {
                           StrFormat(buffer, BUFSIZE, "%.0f", (DOUBLE)info->Size);
                           xmlSetAttrib(xml, scan->Child->Index, 0, NULL, buffer);
                        }
                     }
                  }
               }
            }

            refresh = TRUE;
         }
      }
      else {
         // The file does not exist in XML, so add it and re-sort the view
         add_file_item(Self, xml, info);
         sort = TRUE;
         refresh = TRUE;
      }
   }

   // Now check for deleted files

   if (xml->TagCount > 0) {
      MSG("Checking for deleted files.");

      if (Self->Path[0] IS ':') {
         j = 0;
         buffer[0] = 0;
      }
      else j = StrCopy(Self->Path, buffer, BUFSIZE);

      if ((Self->Path[0] IS ':') OR (!Self->Path[0])) {
         // Do not resolve the path when at the root view
      }
      else if (!ResolvePath(buffer, RSF_NO_FILE_CHECK, &path)) { // Use ResolvePath() to prevent problems with multi-directory volumes
         j = StrCopy(path, buffer, BUFSIZE);
         FreeMemory(path);
      }

      if ((buffer[0] IS '\\') AND (buffer[1] IS '\\')) {
         // Windows doesn't seem to cope well when constantly refreshing UNC paths (folders sometimes fail analysis).

      }
      else {
         BYTE delete_count = 0;
         restart = TRUE;
         while (restart) {
            restart = FALSE;

            // Figure out our tag position since our last delete

            tag = xml->Tags[0];
            for (i=0; (i < Self->DeleteIndex) AND (tag); i++) tag = tag->Next;

            if (!tag) {
               Self->DeleteIndex = 0;
               tag = xml->Tags[0];
            }


            for (; (delete_count < 10) AND (tag); Self->DeleteIndex++, tag = tag->Next) {
               delete_count++;

               if (!tag->Child) continue;
               if (StrCompare("...", tag->Child->Attrib->Value, 0, STR_MATCH_LEN|STR_MATCH_CASE) IS ERR_Okay) continue;

               // Extract the full name of the file for analysis

               StrCopy(extract_filename(tag), buffer + j, BUFSIZE - j);
               error = ResolvePath(buffer, RSF_CASE_SENSITIVE, NULL);

               if (error != ERR_Okay) {
                  LogMsg("Detected deleted file \"%s\" (err: %s)", buffer, GetErrorMsg(error));
                  viewRemoveTag(Self->View, tag->Index, 1);
                  refresh = TRUE;
                  restart = TRUE;
                  break;
               }
            }
         }
      }
   }

   MSG("Sorting and refreshing the view.");

   if (sort)    acSort(Self->View);
   if (refresh) acRefresh(Self->View);

   FreeMemory(buffer);
   STEP();
   return ERR_Okay;
}
