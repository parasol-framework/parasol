/*********************************************************************************************************************
-CATEGORY-
Name: Files
-END-
*********************************************************************************************************************/

// included by lib_filesystem.cpp

/*********************************************************************************************************************

-FUNCTION-
DeleteVolume: Deletes volumes from the system.

This function deletes volume names from the system.  Once a volume is deleted, any further references to it will result
in errors unless the volume is recreated.

-INPUT-
cstr Name: The name of the volume.

-ERRORS-
Okay: The volume was removed.
NullArgs:
LockFailed:
NoPermission: An attempt to delete a system volume was denied.
-END-

*********************************************************************************************************************/

ERROR DeleteVolume(CSTRING Name)
{
   pf::Log log(__FUNCTION__);

   if ((!Name) or (!Name[0])) return ERR_NullArgs;

   log.branch("Name: %s", Name);

   if (auto lock = std::unique_lock{glmVolumes, 4s}) {
      unsigned i;
      for (i=0; (Name[i]) and (Name[i] != ':'); i++);
      std::string vol(Name, i);

      if (glVolumes.contains(vol)) {
         if (glVolumes[vol]["System"] == "Yes") return log.warning(ERR_NoPermission);

         glVolumes.erase(vol);
      }

      return ERR_Okay;
   }
   else return log.warning(ERR_SystemLocked);
}

/*********************************************************************************************************************

-PRIVATE-
RenameVolume: Renames a volume.
-END-

*********************************************************************************************************************/

ERROR RenameVolume(CSTRING Volume, CSTRING Name)
{
   pf::Log log(__FUNCTION__);

   if (auto lock = std::unique_lock{glmVolumes, 6s}) {
      LONG i;
      for (i=0; (Volume[i]) and (Volume[i] != ':'); i++);

      std::string vol;
      vol.append(Volume, i);

      if (glVolumes.contains(vol)) {
         glVolumes[Name] = glVolumes[vol];
         glVolumes.erase(vol);

         // Broadcast the change

         auto evdeleted = std::make_unique<UBYTE[]>(sizeof(EVENTID) + vol.size() + 1);
         ((EVENTID *)evdeleted.get())[0] = GetEventID(EVG::FILESYSTEM, "volume", "deleted");
         CopyMemory(vol.c_str(), evdeleted.get() + sizeof(EVENTID), vol.size() + 1);
         BroadcastEvent(evdeleted.get(), sizeof(EVENTID) + vol.size() + 1);

         LONG namelen = StrLength(Name) + 1;
         auto evcreated = std::make_unique<UBYTE[]>(sizeof(EVENTID) + namelen);
         ((EVENTID *)evcreated.get())[0] = EVID_FILESYSTEM_VOLUME_CREATED;
         CopyMemory(Name, evcreated.get() + sizeof(EVENTID), namelen);
         BroadcastEvent(evcreated.get(), sizeof(EVENTID) + namelen);
         return ERR_Okay;
      }

      return ERR_Search;
   }
   else return log.warning(ERR_LockFailed);
}

/*********************************************************************************************************************

-FUNCTION-
SetVolume: Adds a new volume name to the system.

The SetVolume() function is used to create a volume that is associated with one or more paths.  If the volume already
exists, it possible to append more paths or replace them entirely.

Flags that may be passed are as follows:

<types lookup="VOLUME"/>

-INPUT-
cstr Name: Required.  The name of the volume.
cstr Path: Required.  The path to be associated with the volume.  If setting multiple paths, separate each path with a semi-colon character.  Each path must terminate with a forward slash to denote a folder.
cstr Icon: An icon can be associated with the volume so that it has graphical representation when viewed in the UI.  The required icon string format is 'category/name'.
cstr Label: An optional label or short comment may be applied to the volume.  This may be useful if the volume name has little meaning to the user (e.g. drive1, drive2 ...).
cstr Device: If the volume references the root of a device, specify a code of 'disk', 'hd', 'cd', 'network' or 'usb'.
int(VOLUME) Flags: Optional flags.

-ERRORS-
Okay: The volume was successfully added.
NullArgs: A valid name and path string was not provided.
LockFailed:
-END-

*********************************************************************************************************************/

ERROR SetVolume(CSTRING Name, CSTRING Path, CSTRING Icon, CSTRING Label, CSTRING Device, VOLUME Flags)
{
   pf::Log log(__FUNCTION__);

   if ((!Name) or (!Path)) return log.warning(ERR_NullArgs);

   std::string name;

   LONG i;
   for (i=0; (Name[i]) and (Name[i] != ':'); i++);
   name.append(Name, 0, i);

   if (Label) log.branch("Name: %s (%s), Path: %s", Name, Label, Path);
   else log.branch("Name: %s, Path: %s", Name, Path);

   if (auto lock = std::unique_lock{glmVolumes, 6s}) {
      // If we are not in replace mode, check if the volume already exists with configured path.  If so, add the path as a complement
      // to the existing volume.  In this mode nothing else besides the path is changed, even if other tags are specified.

      if ((Flags & VOLUME::REPLACE) IS VOLUME::NIL) {
         if (glVolumes.contains(name)) {
            auto &keys = glVolumes[name];
            if ((Flags & VOLUME::PRIORITY) != VOLUME::NIL) keys["Path"] = std::string(Path) + "|" + keys["Path"];
            else keys["Path"] = keys["Path"] + "|" + Path;
            return ERR_Okay;
         }
      }

      auto &keys = glVolumes[name];

      keys["Path"] = Path;

      if (Icon)   keys["Icon"]   = Icon;
      if (Label)  keys["Label"]  = Label;
      if (Device) keys["Device"] = Device;

      if ((Flags & VOLUME::HIDDEN) != VOLUME::NIL) keys["Hidden"] = "Yes";
      if ((Flags & VOLUME::SYSTEM) != VOLUME::NIL) keys["System"] = "Yes";

      auto evbuf = std::make_unique<UBYTE[]>(sizeof(EVENTID) + name.size() + 1);
      ((EVENTID *)evbuf.get())[0] = GetEventID(EVG::FILESYSTEM, "volume", "created");
      CopyMemory(name.c_str(), evbuf.get() + sizeof(EVENTID), name.size() + 1);
      BroadcastEvent(evbuf.get(), sizeof(EVENTID) + name.size() + 1);
      return ERR_Okay;
   }
   else return log.warning(ERR_SystemLocked);
}

/*********************************************************************************************************************

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

*********************************************************************************************************************/

using CALL_CLOSE_DIR       = ERROR (*)(DirInfo *);
using CALL_DELETE          = ERROR (*)(STRING, FUNCTION *);
using CALL_GET_INFO        = ERROR (*)(CSTRING, FileInfo*, LONG);
using CALL_GET_DEVICE_INFO = ERROR (*)(CSTRING, objStorageDevice*);
using CALL_IDENTIFY_FILE   = ERROR (*)(STRING, CLASSID*, CLASSID*);
using CALL_IGNORE_FILE     = void  (*)(extFile*);
using CALL_MAKE_DIR        = ERROR (*)(CSTRING, PERMIT);
using CALL_OPEN_DIR        = ERROR (*)(DirInfo*);
using CALL_RENAME          = ERROR (*)(STRING, STRING);
using CALL_SAME_FILE       = ERROR (*)(CSTRING, CSTRING);
using CALL_SCAN_DIR        = ERROR (*)(DirInfo*);
using CALL_TEST_PATH       = ERROR (*)(CSTRING, RSF, LOC *);
using CALL_WATCH_PATH      = ERROR (*)(extFile*);

ERROR VirtualVolume(CSTRING Name, ...)
{
   pf::Log log(__FUNCTION__);

   if ((!Name) or (!Name[0])) return log.warning(ERR_NullArgs);

   log.branch("%s", Name);

   auto id = StrHash(Name, FALSE);

   if (glVirtual.contains(id)) return ERR_Exists;

   LONG i = StrCopy(Name, glVirtual[id].Name, sizeof(glVirtual[id].Name)-2);
   glVirtual[id].Name[i++]     = ':';
   glVirtual[id].Name[i]       = 0;
   glVirtual[id].VirtualID     = id; // Virtual ID = Hash of the name, not including the colon
   glVirtual[id].CaseSensitive = false;

   va_list list;
   va_start(list, Name);
   LONG arg = 0;
   while (auto tagid = va_arg(list, LONG)) {
      switch (VAS(tagid)) {
         case VAS::DEREGISTER:
            glVirtual.erase(id);
            va_end(list);
            return ERR_Okay; // The volume has been removed, so any further tags are redundant.

         case VAS::DRIVER_SIZE:     glVirtual[id].DriverSize = va_arg(list, LONG); break;
         case VAS::CASE_SENSITIVE:  glVirtual[id].CaseSensitive = va_arg(list, LONG) ? true : false; break;
         case VAS::CLOSE_DIR:       glVirtual[id].CloseDir = va_arg(list, CALL_CLOSE_DIR); break;
         case VAS::DELETE:          glVirtual[id].Delete = va_arg(list, CALL_DELETE); break;
         case VAS::GET_INFO:        glVirtual[id].GetInfo =  va_arg(list, CALL_GET_INFO); break;
         case VAS::GET_DEVICE_INFO: glVirtual[id].GetDeviceInfo = va_arg(list, CALL_GET_DEVICE_INFO); break;
         case VAS::IDENTIFY_FILE:   glVirtual[id].IdentifyFile = va_arg(list, CALL_IDENTIFY_FILE); break;
         case VAS::IGNORE_FILE:     glVirtual[id].IgnoreFile = va_arg(list, CALL_IGNORE_FILE); break;
         case VAS::MAKE_DIR:        glVirtual[id].CreateFolder = va_arg(list, CALL_MAKE_DIR); break;
         case VAS::OPEN_DIR:        glVirtual[id].OpenDir = va_arg(list, CALL_OPEN_DIR); break;
         case VAS::RENAME:          glVirtual[id].Rename = va_arg(list, CALL_RENAME); break;
         case VAS::SAME_FILE:       glVirtual[id].SameFile = va_arg(list, CALL_SAME_FILE); break;
         case VAS::SCAN_DIR:        glVirtual[id].ScanDir = va_arg(list, CALL_SCAN_DIR); break;
         case VAS::TEST_PATH:       glVirtual[id].TestPath = va_arg(list, CALL_TEST_PATH); break;
         case VAS::WATCH_PATH:      glVirtual[id].WatchPath = va_arg(list, CALL_WATCH_PATH); break;

         default:
            log.warning("Bad VAS tag $%.8x @ pair index %d, aborting.", tagid, arg);
            va_end(list);
            return ERR_Args;
      }
      arg++;
   }

   va_end(list);
   return ERR_Okay;
}
