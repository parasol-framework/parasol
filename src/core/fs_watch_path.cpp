
//********************************************************************************************************************

#ifdef __linux__

void fs_ignore_file(extFile *File)
{
   inotify_rm_watch(glInotify, File->prvWatch->Handle);
}

#elif _WIN32

void fs_ignore_file(extFile *File)
{
   RegisterFD(File->prvWatch->Handle, RFD::REMOVE|RFD::READ|RFD::WRITE|RFD::EXCEPT, 0, 0); // remove operation
   winCloseHandle(File->prvWatch->Handle);
}

#elif __APPLE__

// OSX uses an FSEvents device https://en.wikipedia.org/wiki/FSEvents

void fs_ignore_file(extFile *File)
{

}

#endif

//********************************************************************************************************************

extern "C" void path_monitor(HOSTHANDLE, extFile *);

#ifdef __linux__

ERROR fs_watch_path(extFile *File)
{
   pf::Log log;
   STRING path;
   if ((path = StrClone(File->prvResolvedPath))) {
      strip_folder(path); // Remove trailing slash if there is one

      // Add a watch for this file

      LONG nflags = 0;
      if (File->prvWatch->Flags & MFF_READ) nflags |= IN_ACCESS;
      if (File->prvWatch->Flags & MFF_MODIFY) nflags |= IN_MODIFY;
      if (File->prvWatch->Flags & MFF_CREATE) nflags |= IN_CREATE;
      if (File->prvWatch->Flags & MFF_DELETE) nflags |= IN_DELETE | IN_DELETE_SELF;
      if (File->prvWatch->Flags & MFF_OPENED) nflags |= IN_OPEN;
      if (File->prvWatch->Flags & MFF_ATTRIB) nflags |= IN_ATTRIB;
      if (File->prvWatch->Flags & MFF_CLOSED) nflags |= IN_CLOSE_WRITE | IN_CLOSE_NOWRITE;
      if (File->prvWatch->Flags & (MFF_MOVED|MFF_RENAME))  nflags |= IN_MOVED_FROM | IN_MOVED_TO;

      LONG handle = inotify_add_watch(glInotify, path, nflags);

      if (handle IS -1) {
         log.warning("%s", strerror(errno));
         FreeResource(path);
         return ERR_SystemCall;
      }

      File->prvWatch->Handle = handle;

      FreeResource(path);
      return ERR_Okay;
   }

   return ERR_AllocMemory;
}

#elif _WIN32

ERROR fs_watch_path(extFile *File)
{
   pf::Log log(__FUNCTION__);
   HOSTHANDLE handle;
   LONG winflags;
   ERROR error;

   // The path_monitor() function will be called whenever the Path or its content is modified.

   if (!(error = winWatchFile(LONG(File->prvWatch->Flags), File->prvResolvedPath, (File->prvWatch + 1), &handle, &winflags))) {
      File->prvWatch->Handle   = handle;
      File->prvWatch->WinFlags = winflags;
      if (!(error = RegisterFD(handle, RFD::READ, (void (*)(HOSTHANDLE, void*))&path_monitor, File))) {
      }
      else log.warning("Failed to register folder handle.");
   }
   else log.warning("Failed to watch path %s, %s", File->prvResolvedPath, GetErrorMsg(error));

   return error;
}

#else

ERROR fs_watch_path(extFile *File)
{
   return ERR_NoSupport;
}

#endif

//********************************************************************************************************************

#ifdef __linux__
void path_monitor(HOSTHANDLE FD, extFile *File)
{
#if 0
   pf::Log log(__FUNCTION__);
   static THREADVAR BYTE recursion = FALSE; // Recursion avoidance is essential for correct queuing
   if (recursion) return;
   recursion = TRUE;

   AdjustLogLevel(2);

   log.branch("File monitoring event received (FD %d).", FD);

   // Read and process each event in sequence

   LONG result, i;
   UBYTE buffer[2048];
   LONG count = 0;
   LONG buffersize = 0;
   while (((result = read(FD, buffer+buffersize, sizeof(buffer)-buffersize)) > 0) or (buffersize > 0)) {
      if (result > 0) buffersize += result;

      struct inotify_event *event = (struct inotify_event *)buffer;

      log.msg("Descriptor: %d, Name: %s", event->wd, event->name);

      // Use the watch descriptor to determine what user routine we are supposed to call.

      for (LONG i=0; i < MAX_FILEMONITOR; i++) {
         if (!glFileMonitor[i].UID) continue;
         if (FD != glInotify) continue;
         if (event->wd != glFileMonitor[i].Handle) continue;

         // Apply the user's filtering rules, if any

         if (glFileMonitor[i].Flags & MFF_FOLDER) {
            if (!(event->mask & IN_ISDIR)) break;
         }
         else if (glFileMonitor[i].Flags & MFF_FILE) {
            if (event->mask & IN_ISDIR) break;
         }

         // If the event has a name attached, read it

         CSTRING path;
         if (event->len > 0) {
            path = event->name;

            if (event->len > sizeof(buffer) - sizeof(struct inotify_event)) {
               // If this is a buffer overflow attempt or system error then we won't process it.

               lseek(FD, sizeof(struct inotify_event) + event->len - sizeof(buffer), SEEK_CUR);
            }

            UBYTE fnbuffer[256];
            if ((path[0] IS '/') and (path[1] IS 0)) path = NULL;
            else if ((glFileMonitor[i].Flags & MFF_QUALIFY) and (event->mask & IN_ISDIR)) {
               LONG j = StrCopy(path, fnbuffer, sizeof(fnbuffer)-1);
               fnbuffer[j++] = '/';
               fnbuffer[j] = 0;
               path = fnbuffer;
            }
         }
         else path = NULL;

         LONG flags = 0L;
         if (event->mask & IN_Q_OVERFLOW) {
            log.warning("A buffer overflow has occurred in the file monitor.");
         }
         if (event->mask & IN_ACCESS) flags |= MFF_READ;
         if (event->mask & IN_MODIFY) flags |= MFF_MODIFY;
         if (event->mask & IN_CREATE) flags |= MFF_CREATE;
         if (event->mask & IN_DELETE) flags |= MFF_DELETE;
         if (event->mask & IN_DELETE_SELF) flags |= MFF_DELETE|MFF_SELF;
         if (event->mask & IN_OPEN)   flags |= MFF_OPENED;
         if (event->mask & IN_ATTRIB) flags |= MFF_ATTRIB;
         if (event->mask & (IN_CLOSE_WRITE|IN_CLOSE_NOWRITE)) flags |= MFF_CLOSED;
         if (event->mask & (IN_MOVED_FROM|IN_MOVED_TO)) flags |= MFF_MOVED;

         if (event->mask & IN_UNMOUNT) flags |= MFF_UNMOUNT;

         if (event->mask & IN_ISDIR) flags |= MFF_FOLDER;
         else flags |= MFF_FILE;

         ERROR error;
         if (flags) {
            if (glFileMonitor[i].Routine.Type IS CALL_STDC) {
               ERROR (*routine)(extFile *File, CSTRING path, LARGE Custom, LONG Flags);
               routine = glFileMonitor[i].Routine.StdC.Routine;

               OBJECTPTR context;
               if (glFileMonitor[i].Context) context = SetContext(glFileMonitor[i].Context);
               else context = NULL;

               error = routine(glFileMonitor[i].File, path, glFileMonitor[i].Custom, flags);

               if (context) SetContext(context);
            }
            else if (glFileMonitor[i].Routine.Type IS CALL_SCRIPT) {
               OBJECTPTR script;
               if ((script = tlFeedback.Script.Script)) {
                  const ScriptArg args[] = {
                     { "File",   FD_STRING,  { .Address = glFileMonitor[i].File } },
                     { "Path",   FD_STRING,  { .Address = path } },
                     { "Custom", FD_LARGE,   { .Large = glFileMonitor[i].Custom } },
                     { "Flags",  FD_LONG,    { .Long = flags } }
                  };
                  if (scCallback(script, tlFeedback.Script.ProcedureID, args, ARRAYSIZE(args), &error)) error = ERR_Failed;
               }
               else error = ERR_Terminate;
            }

            if (error IS ERR_Terminate) Action(MT_FlWatch, glFileMonitor[i].File, NULL);
         }
         else log.warning("Flags $%.8x not recognised.", flags);
         break;
      }

      LONG event_size = sizeof(struct inotify_event) + event->len;

      if (buffersize > event_size) CopyMemory(buffer + event_size, buffer, buffersize - event_size);
      buffersize -= event_size;

      if (++count > 50) {
         log.warning("Excessive looping detected - bailing out.");
         break;
      }
   }

   recursion = FALSE;

   AdjustLogLevel(-2);
#endif
}

//********************************************************************************************************************

#elif _WIN32

void path_monitor(HOSTHANDLE Handle, extFile *File)
{
   pf::Log log(__FUNCTION__);

   static THREADVAR BYTE recursion = FALSE; // Recursion avoidance is essential for correct queuing
   if ((recursion) or (!File->prvWatch)) return;
   recursion = TRUE;

   AdjustLogLevel(2);

   log.branch("File monitoring event received (Handle %p, File #%d).", Handle, File->UID);

   ERROR error;
   ERROR (*routine)(extFile *, CSTRING path, LARGE Custom, LONG Flags);
   if (File->prvWatch->Handle) {
      char path[256];
      LONG status;

      // Keep in mind that the state of the File object might change during the loop due to the code in the user's callback.

      while ((File->prvWatch) and (!winReadChanges(File->prvWatch->Handle, (APTR)(File->prvWatch + 1), File->prvWatch->WinFlags, path, sizeof(path), &status))) {
         if ((File->prvWatch->Flags & MFF::DEEP) IS MFF::NIL) { // Ignore if path is in a sub-folder and the deep option is not enabled.
            LONG i;
            for (i=0; (path[i]) and (path[i] != '\\'); i++);
            if (path[i] IS '\\') continue;
         }

         if (File->prvWatch->Routine.StdC.Context) {
            pf::SwitchContext context(File->prvWatch->Routine.StdC.Context);

            if (File->prvWatch->Routine.Type IS CALL_STDC) {
               routine = (ERROR (*)(extFile *, CSTRING, LARGE, LONG))File->prvWatch->Routine.StdC.Routine;
               error = routine(File, path, File->prvWatch->Custom, status);
            }
            else if (File->prvWatch->Routine.Type IS CALL_SCRIPT) {
               OBJECTPTR script;
               if ((script = File->prvWatch->Routine.Script.Script)) {
                  const ScriptArg args[] = {
                     { "File",   FD_OBJECTPTR, { .Address = File } },
                     { "Path",   FD_STRING,  { .Address = path } },
                     { "Custom", FD_LARGE,   { .Large = File->prvWatch->Custom } },
                     { "Flags",  FD_LONG,    { .Long = 0 } }
                  };
                  if (scCallback(script, File->prvWatch->Routine.Script.ProcedureID, args, ARRAYSIZE(args), &error)) error = ERR_Failed;
               }
               else error = ERR_Terminate;
            }
            else error = ERR_Terminate;
         }
         else {
            routine = (ERROR (*)(extFile *, CSTRING, LARGE, LONG))File->prvWatch->Routine.StdC.Routine;
            error = routine(File, path, File->prvWatch->Custom, status);
         }

         if (error IS ERR_Terminate) Action(MT_FlWatch, File, NULL);
      }
   }
   else {
      routine = (ERROR (*)(extFile *, CSTRING, LARGE, LONG))File->prvWatch->Routine.StdC.Routine;
      if (File->prvWatch->Routine.StdC.Context) {
         pf::SwitchContext context(File->prvWatch->Routine.StdC.Context);
         error = routine(File, File->Path, File->prvWatch->Custom, 0);
      }
      else error = routine(File, File->Path, File->prvWatch->Custom, 0);

      if (error IS ERR_Terminate) Action(MT_FlWatch, File, NULL);
   }

   winFindNextChangeNotification(Handle);

   recursion = FALSE;

   AdjustLogLevel(-2);
}

#endif
