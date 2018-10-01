
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
   if ((!Name) OR (!Name[0])) return ERR_NullArgs;

   LogF("~2DeleteVolume()","Name: %s", Name);

   LONG i, j;
   if (!AccessPrivateObject((OBJECTPTR)glVolumes, 8000)) {
      struct ConfigEntry *entries;
      if ((entries = glVolumes->Entries)) {
         BYTE buffer[40+1];
         for (i=0; (Name[i]) AND (Name[i] != ':') AND (i < sizeof(buffer)-1); i++) buffer[i] = Name[i];
         buffer[i] = 0;

         // Check that the name of the volume is not reserved by the system

         if ((!StrMatch("parasol", buffer)) OR (!StrMatch("programs", buffer)) OR
             (!StrMatch("system", buffer)) OR (!StrMatch("temp", buffer)) OR
             (!StrMatch("user", buffer))) {
            ReleasePrivateObject((OBJECTPTR)glVolumes);
            return ERR_NoPermission;
         }

         for (j=0; j < glVolumes->AmtEntries; j++) {
            if (!StrMatch("Name", entries[j].Key)) {
               if (!StrMatch(buffer, entries[j].Data)) {
                  cfgDeleteSection(glVolumes, entries[j].Section);
                  break;
               }
            }
         }

         // Delete the volume if it appears in the user:config/volumes.cfg file.

         objConfig *userconfig;
         if (!CreateObject(ID_CONFIG, 0, (OBJECTPTR *)&userconfig,
               FID_Path|TSTR, "user:config/volumes.cfg",
               TAGEND)) {

            if ((entries = userconfig->Entries)) {
               for (j=0; j < userconfig->AmtEntries; j++) {
                  if (!StrMatch("Name", entries[j].Key)) {
                     if (!StrMatch(buffer, entries[j].Data)) {
                        cfgDeleteSection(userconfig, entries[j].Section);
                        SaveObjectToFile((OBJECTPTR)userconfig, "user:config/volumes.cfg", PERMIT_READ|PERMIT_WRITE);

                        // Broadcast the change

                        LONG strlen;

                        for (strlen=0; buffer[strlen]; strlen++);
                        strlen++;
                        UBYTE evbuf[sizeof(EVENTID) + strlen];
                        ((EVENTID *)evbuf)[0] = GetEventID(EVG_FILESYSTEM, "volume", "deleted");
                        CopyMemory(buffer, evbuf + sizeof(EVENTID), strlen);
                        BroadcastEvent(evbuf, sizeof(EVENTID) + strlen);

                        break;
                     }
                  }
               }
            }

            acFree(&userconfig->Head);
         }

         ReleasePrivateObject((OBJECTPTR)glVolumes);
         LogBack();
         return ERR_Okay;
      }

      ReleasePrivateObject((OBJECTPTR)glVolumes);
      LogError(ERH_Volume, ERR_GetField);
      LogBack();
      return ERR_GetField;
   }
   else {
      LogError(ERH_Volume, ERR_ExclusiveDenied);
      LogBack();
      return ERR_ExclusiveDenied;
   }
}

/*****************************************************************************

-PRIVATE-
RenameVolume: Renames a volume.
-END-

*****************************************************************************/

ERROR RenameVolume(CSTRING Volume, CSTRING Name)
{
   struct ConfigEntry *entries;
   UBYTE buffer[200], section[100];

   if (!AccessPrivateObject((OBJECTPTR)glVolumes, 5000)) {
      if ((entries = glVolumes->Entries)) {
         LONG i;
         for (i=0; (Volume[i]) AND (Volume[i] != ':') AND (i < sizeof(buffer)-1); i++) buffer[i] = Volume[i];
         buffer[i] = 0;

         for (i=0; i < glVolumes->AmtEntries; i++) {
            if ((!StrMatch("Name", entries[i].Key)) AND (!StrMatch(buffer, entries[i].Data))) {
               StrCopy(entries[i].Section, section, sizeof(section));
               cfgWriteValue(glVolumes, section, "Name", Name);

               // Broadcast the change

               LONG strlen;

               for (strlen=0; buffer[strlen]; strlen++);
               strlen++;
               UBYTE evdeleted[sizeof(EVENTID) + strlen];
               ((EVENTID *)evdeleted)[0] = GetEventID(EVG_FILESYSTEM, "volume", "deleted");
               CopyMemory(buffer, evdeleted + sizeof(EVENTID), strlen);
               BroadcastEvent(evdeleted, sizeof(EVENTID) + strlen);

               for (strlen=0; Name[strlen]; strlen++);
               strlen++;
               UBYTE evcreated[sizeof(EVENTID) + strlen];
               ((EVENTID *)evcreated)[0] = EVID_FILESYSTEM_VOLUME_CREATED;
               CopyMemory(Name, evcreated + sizeof(EVENTID), strlen);
               BroadcastEvent(evcreated, sizeof(EVENTID) + strlen);

               break;
            }
         }
      }

      ReleasePrivateObject((OBJECTPTR)glVolumes);
      return ERR_Okay;
   }
   else return ERR_AccessObject;
}

/*****************************************************************************

-FUNCTION-
SetVolume: Adds a new volume name to the system.

The SetVolume() function is used to assign one or more paths to a volume name.  It can preserve any existing
paths that are attributed to the volume if the name already exists.  If the volume does not already exist, a new one
will be created from scratch.

This function uses tags to create new volumes.  The following table lists all tags that are accepted by this
function.  Remember to terminate the taglist with TAGEND.

<types type="Tag">
<type name="AST_NAME">Required.  Follow this tag with the string name of the volume.</>
<type name="AST_PATH">Required.  Follow this tag with the path to be set against the volume.  If setting multiple paths, separate each path with a semi-colon character.  Each path must terminate with a forward slash to denote a folder.</>
<type name="AST_COMMENT">A user comment string may be set against the volume with this tag.</>
<type name="AST_FLAGS">Optional flags.  See below for more details.</>
<type name="AST_ICON">You may set an icon to be associated with the volume, so that it has graphical representation when viewed in a file viewer for example.  The required icon string format is "category/name".</>
<type name="AST_ID">A unique ID string can be specified against the volume.  This is useful for programs that want to identify volumes that they have created.</>
<type name="AST_LABEL">An optional label or short comment may be applied to the volume.  This may be useful if the volume name has little meaning to the user (e.g. drive1, drive2 ...).</>
</types>

Flags that may be passed with the AST_FLAGS tag are as follows:

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
   LONG i, j;

   va_list list;
   va_start(list, TagID);

   LONG flags = 0;
   CSTRING path    = NULL;
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
      if ((!type) OR (type & FD_STRING)) {
         switch ((ULONG)tagid) {
            case AST_NAME: {
               CSTRING str = va_arg(list, CSTRING);
               for (i=0; (str[i]) AND (str[i] != ':') AND (i < sizeof(name)-1); i++) {
                  name[i] = str[i];
               }
               name[i] = 0;
               goto next;
            }

            case AST_DEVICE_PATH: devpath = va_arg(list, CSTRING); goto next;
            case AST_PATH:    path = va_arg(list, CSTRING); goto next;
            case AST_ICON:    icon = va_arg(list, CSTRING); goto next;
            case AST_COMMENT: comment = va_arg(list, CSTRING); goto next;
            case AST_DEVICE:  device = va_arg(list, CSTRING); goto next;

            case AST_LABEL:
               label = va_arg(list, CSTRING);
               if ((label) AND (!label[0])) label = NULL;
               goto next;

            case AST_ID: // A unique ID string that can be associated with volumes, used by mountdrives
               devid = va_arg(list, CSTRING);
               goto next;
         }
      }

      if ((!type) OR (type & FD_LONG)) {
         switch ((ULONG)tagid) {
            case AST_FLAGS: flags = va_arg(list, LONG); goto next;
         }
      }

      if ((!type) OR (type & FD_DOUBLE)) {
         switch ((ULONG)tagid) {
            case AST_FLAGS: flags = F2T(va_arg(list, DOUBLE)); goto next;
         }
      }

      LogF("@SetVolume()","Bad tag ID $%.8x%.8x, unrecognised flags $%.8x @ tag-pair %d.", (ULONG)(tagid>>32), (ULONG)tagid, type, count);
      va_end(list);
      return LogError(ERH_Volume, ERR_WrongType);
next:
      tagid = va_arg(list, LARGE);
      count++;
   }

   va_end(list);

   if ((!name[0]) OR (!path)) return LogError(ERH_Volume, ERR_NullArgs);

   if (label) LogF("~SetVolume()","Name: %s (%s), Path: %s", name, label, path);
   else LogF("~SetVolume()","Name: %s, Path: %s", name, path);

   if ((!glVolumes) OR (AccessPrivateObject((OBJECTPTR)glVolumes, 8000) != ERR_Okay)) {
      LogBack();
      return PostError(ERR_AccessObject);
   }

   CSTRING savefile;
   LONG savepermissions;
   if (flags & VOLUME_SYSTEM) {
      savefile = "config:volumes.cfg";
      savepermissions = PERMIT_ALL_READ|PERMIT_WRITE|PERMIT_GROUP_WRITE;
   }
   else {
      savefile = "user:config/volumes.cfg";
      savepermissions = PERMIT_READ|PERMIT_WRITE;
   }

   // If we are not in replace mode, check if the volume already exists with configured path.  If so, add the path as a complement
   // to the existing volume.  In this mode nothing else besides the path is changed, even if other tags are specified.

   if (!(flags & VOLUME_REPLACE)) {
      struct ConfigEntry *entries;
      if ((entries = glVolumes->Entries)) {
         for (i=0; i < glVolumes->AmtEntries; i++) {
            if ((!StrMatch("Name", entries[i].Key)) AND (!StrMatch(name, entries[i].Data))) {
               while ((i > 0) AND (!StrMatch(entries[i].Section, entries[i-1].Section))) i--;

               for (; i < glVolumes->AmtEntries; i++) {
                  if (!StrMatch("Path", entries[i].Key)) {
                     STRING str;
                     if (!AllocMemory(StrLength(path) + 1 + StrLength(entries[i].Data) + 1, MEM_STRING, (APTR *)&str, NULL)) {
                        if (flags & VOLUME_PRIORITY) {
                           // Put the new path at the start of the list, followed by old paths
                           j = StrCopy(path, str, COPY_ALL);
                           str[j++] = '|';
                           StrCopy(entries[i].Data, str + j, COPY_ALL);
                        }
                        else {
                           // Retain original path order and add new path to the end of the list
                           j = StrCopy(entries[i].Data, str, COPY_ALL);
                           str[j++] = '|';
                           StrCopy(path, str + j, COPY_ALL);
                        }

                        cfgWriteValue(glVolumes, entries[i].Section, entries[i].Key, str);

                        if (flags & VOLUME_SAVE) {
                           // Save the volume permanently
                           objConfig *userconfig;
                           if (!CreateObject(ID_CONFIG, 0, (OBJECTPTR *)&userconfig,
                                 FID_Path|TSTR, savefile,
                                 TAGEND)) {
                              CSTRING result;
                              if (!cfgReadValue(userconfig, entries[i].Section, entries[i].Key, &result)) {
                                 cfgWriteValue(userconfig, entries[i].Section, entries[i].Key, str);
                                 SaveObjectToFile((OBJECTPTR)userconfig, savefile, savepermissions);
                              }
                              acFree(&userconfig->Head);
                           }
                        }

                        FreeMemory(str);
                        ReleasePrivateObject((OBJECTPTR)glVolumes);
                        LogBack();
                        return ERR_Okay;
                     }
                     else {
                        ReleasePrivateObject((OBJECTPTR)glVolumes);
                        LogBack();
                        return LogError(ERH_Volume, ERR_AllocMemory);
                     }
                  }
               }
            }
         }
      }
   }

   // Write the volume out

   LONG configflags = 0;
   if (!(flags & VOLUME_REPLACE)) {
      GetLong(glVolumes, FID_Flags, &configflags);
      configflags |= CNF_LOCK_RECORDS;
      SetLong(glVolumes, FID_Flags, configflags);
   }

   cfgWriteValue(glVolumes, name, "Name", name);
   cfgWriteValue(glVolumes, name, "Path", path);
   if (icon)    cfgWriteValue(glVolumes, name, "Icon", icon);
   if (comment) cfgWriteValue(glVolumes, name, "Comment", comment);
   if (label)   cfgWriteValue(glVolumes, name, "Label", label);
   if (device)  cfgWriteValue(glVolumes, name, "Device", device);
   if (devpath) cfgWriteValue(glVolumes, name, "DevicePath", devpath);
   if (devid)   cfgWriteValue(glVolumes, name, "ID", devid);
   if (flags & VOLUME_HIDDEN) cfgWriteValue(glVolumes, name, "Hidden", "Yes");

   if (flags & VOLUME_SAVE) {
      // Save the volume permanently

      objConfig *userconfig;
      if (!CreateObject(ID_CONFIG, 0, (OBJECTPTR *)&userconfig,
            FID_Path|TSTR, savefile,
            TAGEND)) {

         cfgWriteValue(userconfig, name, "Name", name);
         cfgWriteValue(userconfig, name, "Path", path);
         if (icon) cfgWriteValue(userconfig, name, "Icon", icon);
         if (comment) cfgWriteValue(userconfig, name, "Comment", comment);
         if (devid) cfgWriteValue(userconfig, name, "ID", devid);
         if (flags & VOLUME_HIDDEN) cfgWriteValue(userconfig, name, "Hidden", "Yes");

         SaveObjectToFile((OBJECTPTR)userconfig, savefile, savepermissions);

         acFree(&userconfig->Head);
      }
   }

   if (!(flags & VOLUME_REPLACE)) {
      configflags &= ~CNF_LOCK_RECORDS;
      SetLong(glVolumes, FID_Flags, configflags);
   }

   ReleasePrivateObject((OBJECTPTR)glVolumes);

   LONG strlen;
   for (strlen=0; name[strlen]; strlen++);
   strlen++;
   UBYTE evbuf[sizeof(EVENTID) + strlen];
   ((EVENTID *)evbuf)[0] = GetEventID(EVG_FILESYSTEM, "volume", "created");
   CopyMemory(name, evbuf + sizeof(EVENTID), strlen);
   BroadcastEvent(evbuf, sizeof(EVENTID) + strlen);

   LogBack();
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
   if ((!Name) OR (!Name[0])) return LogError(ERH_Volume, ERR_NullArgs);

   LogF("VirtualVolume()","%s", Name);

   ULONG name_hash = StrHash(Name, FALSE);

   // Check if the volume already exists, otherwise use the first empty entry
   // Overwriting or interfering with other volumes via hash collisions is prevented.

   LONG index;
   for (index=0; index < glVirtualTotal; index++) {
      if (name_hash IS glVirtual[index].VirtualID) return ERR_Exists;
   }

   if (index >= ARRAYSIZE(glVirtual)) return LogError(ERH_Volume, ERR_ArrayFull);

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

         // If the deregister option is used, remove the virtual volume from the system

         case VAS_DEREGISTER:
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
            glVirtual[index].CloseDir = va_arg(list, APTR);
            break;

         case VAS_DELETE:
            glVirtual[index].Delete = va_arg(list, APTR);
            break;

         case VAS_GET_INFO:
            glVirtual[index].GetInfo =  va_arg(list, APTR);
            break;

         case VAS_GET_DEVICE_INFO:
            glVirtual[index].GetDeviceInfo = va_arg(list, APTR);
            break;

         case VAS_IDENTIFY_FILE:
            glVirtual[index].IdentifyFile = va_arg(list, APTR);
            break;

         case VAS_IGNORE_FILE:
            glVirtual[index].IgnoreFile = va_arg(list, APTR);
            break;

         case VAS_MAKE_DIR:
            glVirtual[index].CreateFolder = va_arg(list, APTR);
            break;

         case VAS_OPEN_DIR:
            glVirtual[index].OpenDir = va_arg(list, APTR);
            break;

         case VAS_RENAME:
            glVirtual[index].Rename = va_arg(list, APTR);
            break;

         case VAS_SAME_FILE:
            glVirtual[index].SameFile = va_arg(list, APTR);
            break;

         case VAS_SCAN_DIR:
            glVirtual[index].ScanDir = va_arg(list, APTR);
            break;

         case VAS_TEST_PATH:
            glVirtual[index].TestPath = va_arg(list, APTR);
            break;

         case VAS_WATCH_PATH:
            glVirtual[index].WatchPath = va_arg(list, APTR);
            break;

         default:
            LogF("@VirtualVolume()","Bad VAS tag $%.8x @ pair index %d, aborting.", tagid, arg);
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
