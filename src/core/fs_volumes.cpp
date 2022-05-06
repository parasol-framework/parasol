/*****************************************************************************
-CATEGORY-
Name: Files
-END-
*****************************************************************************/

/*****************************************************************************

-FUNCTION-
DeleteVolume: Deletes volumes from the system.

This function deletes volume names from the system.  Once a volume is deleted, any further references to it will result
in errors unless the volume is recreated.  All paths that are related to the volume are destroyed as a result of
calling this function.

-INPUT-
cstr Name: The name of the volume.

-ERRORS-
Okay: The volume was removed.
Args:
ExclusiveDenied: Access to the SystemVolumes object was denied.
-END-

*****************************************************************************/

ERROR DeleteVolume(CSTRING Name)
{
   parasol::Log log(__FUNCTION__);

   if ((!Name) or (!Name[0])) return ERR_NullArgs;

   log.branch("Name: %s", Name);

   parasol::ScopedObjectLock<objConfig> volumes((OBJECTPTR)glVolumes, 5000);
   if (!volumes.granted()) return log.warning(ERR_AccessObject);

   ConfigGroups *groups;
   if (!GetPointer(glVolumes, FID_Data, &groups)) {
      std::string vol;
      LONG i;
      for (i=0; (Name[i]) and (Name[i] != ':'); i++);
      vol.append(Name, i);

      // Check that the name of the volume is not reserved by the system

      if ((!StrMatch("parasol", vol.c_str())) or (!StrMatch("programs", vol.c_str())) or
          (!StrMatch("system", vol.c_str())) or (!StrMatch("temp", vol.c_str())) or
          (!StrMatch("user", vol.c_str()))) {
         return ERR_NoPermission;
      }

      for (auto& [group, keys] : groups[0]) {
         if (!StrMatch(vol.c_str(), keys["Name"].c_str())) {
            cfgDeleteGroup(glVolumes, group.c_str());
            break;
         }
      }

      // Delete the volume if it appears in the user:config/volumes.cfg file.

      objConfig *userconfig;
      if (!CreateObject(ID_CONFIG, 0, (OBJECTPTR *)&userconfig, FID_Path|TSTR, "user:config/volumes.cfg", TAGEND)) {
         if (!GetPointer(userconfig, FID_Data, &groups)) {
            for (auto& [group, keys] : groups[0]) {
               if (!StrMatch(vol.c_str(), keys["Name"].c_str())) {
                  cfgDeleteGroup(userconfig, group.c_str());
                  SaveObjectToFile((OBJECTPTR)userconfig, "user:config/volumes.cfg", PERMIT_READ|PERMIT_WRITE);

                  // Broadcast the change

                  LONG strlen = vol.size() + 1;
                  UBYTE evbuf[sizeof(EVENTID) + strlen];
                  ((EVENTID *)evbuf)[0] = GetEventID(EVG_FILESYSTEM, "volume", "deleted");
                  CopyMemory(vol.c_str(), evbuf + sizeof(EVENTID), strlen);
                  BroadcastEvent(evbuf, sizeof(EVENTID) + strlen);
                  break;
               }
            }
         }

         acFree(&userconfig->Head);
      }

      return ERR_Okay;
   }

   return log.warning(ERR_GetField);
}

/*****************************************************************************

-PRIVATE-
RenameVolume: Renames a volume.
-END-

*****************************************************************************/

ERROR RenameVolume(CSTRING Volume, CSTRING Name)
{
   parasol::Log log(__FUNCTION__);

   parasol::ScopedObjectLock<objConfig> volumes((OBJECTPTR)glVolumes, 5000);
   if (volumes.granted()) {
      ConfigGroups *groups;
      if (!GetPointer(glVolumes, FID_Data, &groups)) {
         std::string vol;
         LONG i;

         for (i=0; (Volume[i]) and (Volume[i] != ':'); i++);
         vol.append(Volume, i);

         for (auto& [group, keys] : groups[0]) {
            if (!StrMatch(keys["Name"].c_str(), vol.c_str())) {
               keys["Name"] = Name;

               // Broadcast the change

               LONG strlen = vol.size() + 1;
               UBYTE evdeleted[sizeof(EVENTID) + strlen];
               ((EVENTID *)evdeleted)[0] = GetEventID(EVG_FILESYSTEM, "volume", "deleted");
               CopyMemory(vol.c_str(), evdeleted + sizeof(EVENTID), strlen);
               BroadcastEvent(evdeleted, sizeof(EVENTID) + strlen);

               strlen = StrLength(Name) + 1;
               UBYTE evcreated[sizeof(EVENTID) + strlen];
               ((EVENTID *)evcreated)[0] = EVID_FILESYSTEM_VOLUME_CREATED;
               CopyMemory(Name, evcreated + sizeof(EVENTID), strlen);
               BroadcastEvent(evcreated, sizeof(EVENTID) + strlen);
               return ERR_Okay;
            }
         }

         return ERR_Search;
      }
      else return log.warning(ERR_GetField);
   }
   else return log.warning(ERR_AccessObject);
}

/*****************************************************************************

-FUNCTION-
SetVolume: Adds a new volume name to the system.

The SetVolume() function is used to assign one or more paths to a volume name.  It can preserve any existing
paths that are attributed to the volume if the name already exists.  If the volume does not already exist, a new one
will be created from scratch.

This function uses tags to create new volumes.  The following table lists all tags that are accepted by this
function.  Remember to terminate the taglist with `TAGEND`.

<types type="Tag">
<type name="AST_NAME">Required.  Follow this tag with the string name of the volume.</>
<type name="AST_PATH">Required.  Follow this tag with the path to be set against the volume.  If setting multiple paths, separate each path with a semi-colon character.  Each path must terminate with a forward slash to denote a folder.</>
<type name="AST_COMMENT">A user comment string may be set against the volume with this tag.</>
<type name="AST_FLAGS">Optional flags.  See below for more details.</>
<type name="AST_ICON">You may set an icon to be associated with the volume, so that it has graphical representation when viewed in a file viewer for example.  The required icon string format is "category/name".</>
<type name="AST_ID">A unique ID string can be specified against the volume.  This is useful for programs that want to identify volumes that they have created.</>
<type name="AST_LABEL">An optional label or short comment may be applied to the volume.  This may be useful if the volume name has little meaning to the user (e.g. drive1, drive2 ...).</>
</types>

Flags that may be passed with the `AST_FLAGS` tag are as follows:

<types lookup="VOLUME"/>

-INPUT-
vtags Tags: A list of AST tags, terminated with a single TAGEND entry.

-ERRORS-
Okay:         The volume was successfully added.
Args:         A valid name and path string was not provided.
AccessObject: Access to the SystemVolumes shared object was denied.
AllocMemory:
-END-

*****************************************************************************/

ERROR SetVolume(LARGE TagID, ...)
{
   parasol::Log log(__FUNCTION__);
   LONG i;

   va_list list;
   va_start(list, TagID);

   LONG flags = 0;
   std::string path;
   CSTRING comment = NULL;
   CSTRING icon    = NULL;
   CSTRING label   = NULL;
   CSTRING device  = NULL;
   CSTRING devpath = NULL;
   CSTRING devid   = NULL;
   char name[LEN_VOLUME_NAME];
   name[0] = 0;
   LONG count = 0;
   LARGE tagid = TagID;
   while (tagid) {
      ULONG type = tagid>>32;
      if ((!type) or (type & FD_STRING)) {
         switch ((ULONG)tagid) {
            case AST_NAME: {
               CSTRING str = va_arg(list, CSTRING);
               for (i=0; (str[i]) and (str[i] != ':') and ((size_t)i < sizeof(name)-1); i++) name[i] = str[i];
               name[i] = 0;
               goto next;
            }

            case AST_DEVICE_PATH: devpath = va_arg(list, CSTRING); goto next;
            case AST_PATH:    path = std::string(va_arg(list, CSTRING)); goto next;
            case AST_ICON:    icon = va_arg(list, CSTRING); goto next;
            case AST_COMMENT: comment = va_arg(list, CSTRING); goto next;
            case AST_DEVICE:  device = va_arg(list, CSTRING); goto next;

            case AST_LABEL:
               label = va_arg(list, CSTRING);
               if ((label) and (!label[0])) label = NULL;
               goto next;

            case AST_ID: // A unique ID string that can be associated with volumes, used by mountdrives
               devid = va_arg(list, CSTRING);
               goto next;
         }
      }

      if ((!type) or (type & FD_LONG)) {
         switch ((ULONG)tagid) {
            case AST_FLAGS: flags = va_arg(list, LONG); goto next;
         }
      }

      if ((!type) or (type & FD_DOUBLE)) {
         switch ((ULONG)tagid) {
            case AST_FLAGS: flags = F2T(va_arg(list, DOUBLE)); goto next;
         }
      }

      log.warning("Bad tag ID $%.8x%.8x, unrecognised flags $%.8x @ tag-pair %d.", (ULONG)(tagid>>32), (ULONG)tagid, type, count);
      va_end(list);
      return log.warning(ERR_WrongType);
next:
      tagid = va_arg(list, LARGE);
      count++;
   }

   va_end(list);

   if ((!name[0]) or (path.empty())) return log.warning(ERR_NullArgs);

   if (label) log.branch("Name: %s (%s), Path: %s", name, label, path.c_str());
   else log.branch("Name: %s, Path: %s", name, path.c_str());

   parasol::ScopedObjectLock<objConfig> volumes((OBJECTPTR)glVolumes, 8000);
   if (!volumes.granted()) return log.warning(ERR_AccessObject);

   std::string savefile;
   LONG savepermissions;
   if (flags & VOLUME_SYSTEM) {
      savefile = "config:volumes.cfg";
      savepermissions = PERMIT_ALL_READ|PERMIT_WRITE|PERMIT_GROUP_WRITE;
   }
   else {
      savefile = "user:config/volumes.cfg";
      savepermissions = PERMIT_READ|PERMIT_WRITE;
   }

   ConfigGroups *groups;
   if (GetPointer(glVolumes, FID_Data, &groups)) {
      return log.warning(ERR_FieldNotSet);
   }

   // If we are not in replace mode, check if the volume already exists with configured path.  If so, add the path as a complement
   // to the existing volume.  In this mode nothing else besides the path is changed, even if other tags are specified.

   if (!(flags & VOLUME_REPLACE)) {
      for (auto& [group, keys] : groups[0]) {
         if (keys.contains("Name") and (!StrMatch(name, keys["Name"].c_str()))) {
            if (keys.contains("Path")) {
               if (flags & VOLUME_PRIORITY) keys["Path"] = path + "|" + keys["Path"];
               else keys["Path"] = keys["Path"] + "|" + path;

               if (flags & VOLUME_SAVE) { // Save the volume permanently
                  objConfig *userconfig;
                  if (!CreateObject(ID_CONFIG, 0, (OBJECTPTR *)&userconfig, FID_Path|TSTR, savefile.c_str(), TAGEND)) {
                     cfgWriteValue(userconfig, group.c_str(), "Path", keys["Path"].c_str());
                     SaveObjectToFile((OBJECTPTR)userconfig, savefile.c_str(), savepermissions);
                     acFree(&userconfig->Head);
                  }
               }

               return ERR_Okay;
            }
         }
      }
   }

   cfgWriteValue(glVolumes, name, "Name", name); // Ensure that an entry for the volume exists before we search for it.

   for (auto& [group, keys] : groups[0]) {
      if (!StrMatch(group.c_str(), name)) {
         keys["Path"] = path;

         if (icon)    keys["Icon"]       = icon;
         if (comment) keys["Comment"]    = comment;
         if (label)   keys["Label"]      = label;
         if (device)  keys["Device"]     = device;
         if (devpath) keys["DevicePath"] = devpath;
         if (devid)   keys["ID"]         = devid;

         if (flags & VOLUME_HIDDEN) keys["Hidden"] = "Yes";

         if (flags & VOLUME_SAVE) { // Save the volume permanently
            objConfig *userconfig;
            if (!CreateObject(ID_CONFIG, 0, (OBJECTPTR *)&userconfig, FID_Path|TSTR, savefile.c_str(), TAGEND)) {
               cfgWriteValue(userconfig, name, "Name", name); // Ensure that an entry for the volume exists before we search for it.
               ConfigGroups *usergroups;
               if (GetPointer(userconfig, FID_Data, &usergroups)) {
                  for (auto& [group, ukeys] : usergroups[0]) {
                     if (!StrMatch(group.c_str(), name)) {
                        ukeys["Path"] = path;
                        if (icon)    ukeys["Icon"]    = icon;
                        if (comment) ukeys["Comment"] = comment;
                        if (devid)   ukeys["ID"]      = devid;
                        if (flags & VOLUME_HIDDEN) ukeys["Hidden"] = "Yes";
                        break;
                     }
                  }
               }

               SaveObjectToFile((OBJECTPTR)userconfig, savefile.c_str(), savepermissions);
               acFree(&userconfig->Head);
            }
         }
         break;
      }
   }

   LONG strlen = StrLength(name) + 1;
   UBYTE evbuf[sizeof(EVENTID) + strlen];
   ((EVENTID *)evbuf)[0] = GetEventID(EVG_FILESYSTEM, "volume", "created");
   CopyMemory(name, evbuf + sizeof(EVENTID), strlen);
   BroadcastEvent(evbuf, sizeof(EVENTID) + strlen);
   return ERR_Okay;
}

/*****************************************************************************

-FUNCTION-
VirtualVolume: Creates virtual volumes.
Status: private

Private

-INPUT-
cstr Name: The name of the volume.
tags Tags: Options to apply to the volume.

-ERRORS-
Okay
Args
NullArgs
Exists: The named volume already exists.
-END-

*****************************************************************************/

ERROR VirtualVolume(CSTRING Name, ...)
{
   parasol::Log log(__FUNCTION__);

   if ((!Name) or (!Name[0])) return log.warning(ERR_NullArgs);

   log.branch("%s", Name);

   ULONG name_hash = StrHash(Name, FALSE);

   // Check if the volume already exists, otherwise use the first empty entry
   // Overwriting or interfering with other volumes via hash collisions is prevented.

   LONG index;
   for (index=0; index < glVirtualTotal; index++) {
      if (name_hash IS glVirtual[index].VirtualID) return ERR_Exists;
   }

   if (index >= ARRAYSIZE(glVirtual)) return log.warning(ERR_ArrayFull);

   LONG i = StrCopy(Name, glVirtual[index].Name, sizeof(glVirtual[0].Name)-2);
   glVirtual[index].Name[i++] = ':';
   glVirtual[index].Name[i] = 0;

   glVirtual[index].VirtualID = name_hash; // Virtual ID = Hash of the name, not including the colon
   glVirtual[index].CaseSensitive = FALSE;

   va_list list;
   va_start(list, Name);
   LONG arg = 0;
   LONG tagid;
   while ((tagid = va_arg(list, LONG))) {
      switch (tagid) {
         case VAS_DEREGISTER: // If the deregister option is used, remove the virtual volume from the system
            if (index < glVirtualTotal) {
               if (index < ARRAYSIZE(glVirtual)-1) {
                  CopyMemory(glVirtual + index + 1, glVirtual + index, sizeof(glVirtual[0]) * (ARRAYSIZE(glVirtual) - index - 1));
                  glVirtualTotal--;
               }
               else ClearMemory(glVirtual + index, sizeof(glVirtual[0]));
            }
            va_end(list);
            return ERR_Okay; // The volume has been removed, so any further tags are redundant.

         case VAS_CASE_SENSITIVE:
            if (va_arg(list, LONG)) glVirtual[index].CaseSensitive = TRUE;
            else glVirtual[index].CaseSensitive = FALSE;
            break;

         case VAS_CLOSE_DIR:
            glVirtual[index].CloseDir = va_arg(list, ERROR (*)(DirInfo*));
            break;

         case VAS_DELETE:
            glVirtual[index].Delete = va_arg(list, ERROR (*)(STRING, FUNCTION*));
            break;

         case VAS_GET_INFO:
            glVirtual[index].GetInfo =  va_arg(list, ERROR (*)(CSTRING, FileInfo*, LONG));
            break;

         case VAS_GET_DEVICE_INFO:
            glVirtual[index].GetDeviceInfo = va_arg(list, ERROR (*)(CSTRING, rkStorageDevice*));
            break;

         case VAS_IDENTIFY_FILE:
            glVirtual[index].IdentifyFile = va_arg(list, ERROR (*)(STRING, CLASSID*, CLASSID*));
            break;

         case VAS_IGNORE_FILE:
            glVirtual[index].IgnoreFile = va_arg(list, void (*)(rkFile*));
            break;

         case VAS_MAKE_DIR:
            glVirtual[index].CreateFolder = va_arg(list, ERROR (*)(CSTRING, LONG));
            break;

         case VAS_OPEN_DIR:
            glVirtual[index].OpenDir = va_arg(list, ERROR (*)(DirInfo*));
            break;

         case VAS_RENAME:
            glVirtual[index].Rename = va_arg(list, ERROR (*)(STRING, STRING));
            break;

         case VAS_SAME_FILE:
            glVirtual[index].SameFile = va_arg(list, ERROR (*)(CSTRING, CSTRING));
            break;

         case VAS_SCAN_DIR:
            glVirtual[index].ScanDir = va_arg(list, ERROR (*)(DirInfo*));
            break;

         case VAS_TEST_PATH:
            glVirtual[index].TestPath = va_arg(list, ERROR (*)(CSTRING, LONG, LONG*));
            break;

         case VAS_WATCH_PATH:
            glVirtual[index].WatchPath = va_arg(list, ERROR (*)(rkFile*));
            break;

         default:
            log.warning("Bad VAS tag $%.8x @ pair index %d, aborting.", tagid, arg);
            va_end(list);
            return ERR_Args;
      }
      arg++;
   }

   // Increase the total if the virtual volume is new

   if (index >= glVirtualTotal) glVirtualTotal++;

   va_end(list);
   return ERR_Okay;
}
