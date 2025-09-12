
//********************************************************************************************************************

#ifdef __linux__

void fs_ignore_file(extFile *File)
{
   inotify_rm_watch(glInotify, File->prvWatch->Handle);
}

#elif _WIN32

void fs_ignore_file(extFile *File)
{
   if ((File->prvWatch) and (File->prvWatch->Handle)) {
      RegisterFD(File->prvWatch->Handle, RFD::REMOVE|RFD::READ|RFD::WRITE|RFD::EXCEPT, 0, 0);
      winCloseHandle(File->prvWatch->Handle);
      File->prvWatch->Handle = nullptr;
   }
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

ERR fs_watch_path(extFile *File)
{
   int nflags = 0;
   if ((File->prvWatch->Flags & MFF::READ) != MFF::NIL) nflags |= IN_ACCESS;
   if ((File->prvWatch->Flags & MFF::MODIFY) != MFF::NIL) nflags |= IN_MODIFY;
   if ((File->prvWatch->Flags & MFF::CREATE) != MFF::NIL) nflags |= IN_CREATE;
   if ((File->prvWatch->Flags & MFF::DELETE) != MFF::NIL) nflags |= IN_DELETE | IN_DELETE_SELF;
   if ((File->prvWatch->Flags & MFF::OPENED) != MFF::NIL) nflags |= IN_OPEN;
   if ((File->prvWatch->Flags & MFF::ATTRIB) != MFF::NIL) nflags |= IN_ATTRIB;
   if ((File->prvWatch->Flags & MFF::CLOSED) != MFF::NIL) nflags |= IN_CLOSE_WRITE | IN_CLOSE_NOWRITE;
   if ((File->prvWatch->Flags & (MFF::MOVED|MFF::RENAME)) != MFF::NIL)  nflags |= IN_MOVED_FROM | IN_MOVED_TO;

   auto path = File->prvResolvedPath;
   if (path.ends_with('/')) path.pop_back();
   if (auto handle = inotify_add_watch(glInotify, path.c_str(), nflags); handle != -1) {
      File->prvWatch->Handle = handle;
      return ERR::Okay;
   }
   else {
      pf::Log log;
      log.warning("%s", strerror(errno));
      return ERR::SystemCall;
   }
}

#elif _WIN32

ERR fs_watch_path(extFile *File)
{
   pf::Log log(__FUNCTION__);
   HOSTHANDLE handle;
   int winflags;
   ERR error;

   // The path_monitor() function will be called whenever the Path or its content is modified.

   if ((error = winWatchFile(int(File->prvWatch->Flags), File->prvResolvedPath.c_str(), (File->prvWatch + 1), &handle, &winflags)) IS ERR::Okay) {
      if ((error = RegisterFD(handle, RFD::READ, (void (*)(HOSTHANDLE, void*))&path_monitor, File)) IS ERR::Okay) {
         File->prvWatch->Handle   = handle;
         File->prvWatch->WinFlags = winflags;
      }
      else {
         log.warning("Failed to register folder handle.");
         winCloseHandle(handle);
         File->prvWatch->Handle = nullptr;
      }
   }
   else log.warning("Failed to watch path %s, %s", File->prvResolvedPath.c_str(), GetErrorMsg(error));

   return error;
}

#else

ERR fs_watch_path(extFile *File)
{
   return ERR::NoSupport;
}

#endif

//********************************************************************************************************************

#ifdef __linux__
void path_monitor(HOSTHANDLE FD, extFile *File)
{
#if 0
   pf::Log log(__FUNCTION__);
   static thread_local int8_t recursion = FALSE; // Recursion avoidance is essential for correct queuing
   if (recursion) return;
   recursion = TRUE;

   AdjustLogLevel(2);

   log.branch("File monitoring event received (FD %d).", FD);

   // Read and process each event in sequence

   int result, i;
   uint8_t buffer[2048];
   int count = 0;
   int buffersize = 0;
   while (((result = read(FD, buffer+buffersize, sizeof(buffer)-buffersize)) > 0) or (buffersize > 0)) {
      if (result > 0) buffersize += result;

      struct inotify_event *event = (struct inotify_event *)buffer;

      log.msg("Descriptor: %d, Name: %s", event->wd, event->name);

      // Use the watch descriptor to determine what user routine we are supposed to call.

      for (int i=0; i < MAX_FILEMONITOR; i++) {
         if (!glFileMonitor[i].UID) continue;
         if (FD != glInotify) continue;
         if (event->wd != glFileMonitor[i].Handle) continue;

         // Apply the user's filtering rules, if any

         if ((glFileMonitor[i].Flags & MFF::FOLDER) != MFF::NIL) {
            if (!(event->mask & IN_ISDIR)) break;
         }
         else if ((glFileMonitor[i].Flags & MFF::FILE) != MFF::NIL) {
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

            uint8_t fnbuffer[256];
            if ((path[0] IS '/') and (path[1] IS 0)) path = nullptr;
            else if (((glFileMonitor[i].Flags & MFF::QUALIFY) != MFF::NIL) and (event->mask & IN_ISDIR)) {
               int j = StrCopy(path, fnbuffer, sizeof(fnbuffer)-1);
               fnbuffer[j++] = '/';
               fnbuffer[j] = 0;
               path = fnbuffer;
            }
         }
         else path = nullptr;

         MFF flags = MFF::NIL;
         if (event->mask & IN_Q_OVERFLOW) log.warning("A buffer overflow has occurred in the file monitor.");
         if (event->mask & IN_ACCESS) flags |= MFF::READ;
         if (event->mask & IN_MODIFY) flags |= MFF::MODIFY;
         if (event->mask & IN_CREATE) flags |= MFF::CREATE;
         if (event->mask & IN_DELETE) flags |= MFF::DELETE;
         if (event->mask & IN_DELETE_SELF) flags |= MFF::DELETE|MFF::SELF;
         if (event->mask & IN_OPEN)   flags |= MFF::OPENED;
         if (event->mask & IN_ATTRIB) flags |= MFF::ATTRIB;
         if (event->mask & (IN_CLOSE_WRITE|IN_CLOSE_NOWRITE)) flags |= MFF::CLOSED;
         if (event->mask & (IN_MOVED_FROM|IN_MOVED_TO)) flags |= MFF::MOVED;

         if (event->mask & IN_UNMOUNT) flags |= MFF::UNMOUNT;

         if (event->mask & IN_ISDIR) flags |= MFF::FOLDER;
         else flags |= MFF::FILE;

         ERR error;
         if (flags != MFF::NIL) {
            if (glFileMonitor[i].Routine.isC()) {
               ERR (*routine)(extFile *, CSTRING path, int64_t Custom, MFF Flags, APTR);
               routine = glFileMonitor[i].Routine.Routine;
               pf::SwitchContext context(glFileMonitor[i].Routine.Context);
               error = routine(glFileMonitor[i].File, path, glFileMonitor[i].Custom, flags, glFileMonitor[i].Routine.Meta);
            }
            else if (glFileMonitor[i].Routine.isScript()) {
               if (sc::Call(glFileMonitor[i].Routine, std::to_array<ScriptArg>({
                     { "File",   glFileMonitor[i].File },
                     { "Path",   path },
                     { "Custom", glFileMonitor[i].Custom },
                     { "Flags",  int(flags) }
                  }), error)) error = ERR::Function;
            }

            if (error IS ERR::Terminate) Action(fl::Watch::id, glFileMonitor[i].File, nullptr);
         }
         else log.warning("Flags $%.8x not recognised.", int(flags));
         break;
      }

      int event_size = sizeof(struct inotify_event) + event->len;

      if (buffersize > event_size) copymem(buffer + event_size, buffer, buffersize - event_size);
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

   static thread_local bool recursion = false; // Recursion avoidance is essential for correct queuing
   if ((recursion) or (!File->prvWatch)) return;
   recursion = true;

   AdjustLogLevel(2);

   log.branch("File monitoring event received (Handle %p, File #%d).", Handle, File->UID);

   ERR error;
   if (File->prvWatch->Handle) {
      std::string path;
      int status;

      // Keep in mind that the state of the File object might change during the loop due to the code in the user's callback.
      // Validate resources before each iteration to prevent crashes

      path.resize(256);
      while ((File->prvWatch) and (File->prvWatch->Handle IS Handle)) {
         ERR read_result = winReadChanges(File->prvWatch->Handle, (APTR)(File->prvWatch + 1), File->prvWatch->WinFlags, path.data(), sizeof(path), &status);
         if (read_result != ERR::Okay) {
            // If we get an error other than NothingDone, stop monitoring
            if (read_result != ERR::NothingDone) {
               log.warning("winReadChanges() failed with error %s", GetErrorMsg(read_result));
               break;
            }
            break; // NothingDone -> no more events
         }

         if ((File->prvWatch->Flags & MFF::DEEP) IS MFF::NIL) { // Ignore if path is in a sub-folder and the deep option is not enabled.
            if (path.find('\\') != std::string::npos) continue;
         }

         if (File->prvWatch->Routine.isC()) {
            pf::SwitchContext context(File->prvWatch->Routine.Context);
            auto routine = (ERR (*)(extFile *, CSTRING, int64_t, int, APTR))File->prvWatch->Routine.Routine;
            error = routine(File, path.c_str(), File->prvWatch->Custom, status, File->prvWatch->Routine.Meta);
         }
         else if (File->prvWatch->Routine.isScript()) {
            if (sc::Call(File->prvWatch->Routine, std::to_array<ScriptArg>({
               { "File",   File, FD_OBJECTPTR },
               { "Path",   path.c_str() },
               { "Custom", File->prvWatch->Custom },
               { "Flags",  status }
            }), error) != ERR::Okay) error = ERR::Function;
         }
         else error = ERR::Terminate;

         if (error IS ERR::Terminate) {
            Action(fl::Watch::id, File, nullptr);
            break;
         }

         if (!File->prvWatch) { // Sanity check
            log.traceWarning("Watch removed during callback.");
            break;
         }
      }
   }
   else {
      if (File->prvWatch->Routine.isC()) {
         auto routine = (ERR (*)(extFile *, CSTRING, int64_t, int, APTR))File->prvWatch->Routine.Routine;
         pf::SwitchContext context(File->prvWatch->Routine.Context);
         error = routine(File, File->Path.c_str(), File->prvWatch->Custom, 0, File->prvWatch->Routine.Meta);

         if (error IS ERR::Terminate) Action(fl::Watch::id, File, nullptr);
      }
   }

   if (winValidateHandle(Handle)) {
      winFindNextChangeNotification(Handle);
   }
   else {
      log.warning("Handle invalid, cease monitoring File #%d.", File->UID);
      Action(fl::Watch::id, File, nullptr);
   }

   recursion = false;

   AdjustLogLevel(-2);
}

#endif

