
static void free_private_memory(void);
#ifdef __unix__
static void free_shared_control(void);
#endif

//********************************************************************************************************************

EXPORT void CloseCore(void)
{
   pf::Log log("Shutdown");

   if (glCodeIndex IS CP_FINISHED) return;

   log.msg("PROGRAM TERMINATING");

   #ifdef __unix__
      log.msg("UID: %d, EUID: %d, CrashStatus: %d", getuid(), geteuid(), glCrashStatus);
   #endif

   glProgramStage = STAGE_SHUTDOWN;

   // Critical variables are re-calculated for the close process - this will 'repair' any possible damage that may have
   // been caused to our internal data.

   tlContext = &glTopContext;
   tlDepth   = 0;

   if (glClassFile) { FreeResource(glClassFile); glClassFile = NULL; }

   free_events(); // Remove event subscriptions

   // Deallocate any semaphores that we've left in the system early if we've crashed

   if (glCrashStatus) remove_semaphores();

   // Destroy all other tasks in our instance that we have created.

   {
      pf::Log log("Shutdown");
      log.branch("Removing any child processes...");

      #ifdef KILL_PROCESS_GROUP
         // Kill all processes that have been created by this process and its children
         killpg(0, SIGHUP);
      #else
         for (auto &task : glTasks) {
            log.msg("Removing sub-process #%d (pid %d).", task.TaskID, task.ProcessID);

            #ifdef __unix__
               // SIGHUP will convert to MSGID_QUIT in the signal handlers.  The main reason for us to use it is
               // to stop foreign processes that we've launched.
               kill(task.ProcessID, SIGHUP);
            #else
               SendMessage(task.TaskID, MSGID_QUIT, 0, NULL, 0);
            #endif

            WaitTime(0, -100000);
         }
      #endif
   }

   // Wait for sub-tasks to die within the designated time limit

   log.msg("Waiting for %d child processes to terminate...", LONG(glTasks.size()));

   static const LONG TIME_TO_DIE = 6; // Seconds to wait before a task has to die
   auto wait_until = PreciseTime() + (TIME_TO_DIE * 1000000);
   while ((!glTasks.empty()) and (PreciseTime() < wait_until)) {
      for (auto it = glTasks.begin(); it != glTasks.end(); ) {
         if (it->ProcessID) {
            #ifdef __unix__
            if (kill(it->ProcessID, 0)) { // Process exists?
               it = glTasks.erase(it);
               continue;
            }
            #endif
            log.msg("Process %d is still live.", it->ProcessID);
         }
         it++;
      }

      WaitTime(0, -100000);
   }

   // If the time-to-die has elapsed and sub-tasks are still in the system, send kill messages to force them out.

   #ifdef __unix__
      if (!glTasks.empty()) {
         for (auto &task : glTasks) {
            log.warning("Sending a kill signal to sub-task #%d (process %d).", task.TaskID, task.ProcessID);
            if ((task.ProcessID) and (task.ProcessID != glProcessID)) {
               kill(task.ProcessID, SIGTERM);
            }
         }
         WaitTime(0, -200000);
         glTasks.clear();
      }
   #endif

   glTaskMessageMID = 0;

   // Run the video recovery routine if one has been set and we have crashed

   if ((glCrashStatus) and (glVideoRecovery)) {
      void (*routine)(void);
      routine = glVideoRecovery;
      glVideoRecovery = NULL;
      routine();
   }

   remove_threadpool();

   if ((glCurrentTask) or (glProcessID)) remove_process_waitlocks();

   if (!glCrashStatus) { // This code is only safe to execute if the process hasn't crashed.
      if (glLocale) { FreeResource(glLocale); glLocale = NULL; } // Allocated by StrReadLocale()
      if (glTime) { FreeResource(glTime); glTime = NULL; }

      // Removing objects that are tracked to the task before the first expunge will make for a cleaner exit.

      if (glCurrentTask) {
         const auto children = glObjectChildren[glCurrentTask->UID]; // Take an immutable copy of the resource list

         if (children.size() > 0) {
            log.branch("Freeing %d objects allocated to task #%d.", (LONG)children.size(), glCurrentTask->UID);

            for (const auto id : children) FreeResource(id);
         }
         else log.msg("There are no child objects belonging to task #%d.", glCurrentTask->UID);
      }

      // Make our first attempt at expunging all modules.

      Expunge(false);

      if (glCacheTimer) {
         auto id = glCacheTimer;
         glCacheTimer = 0;
         UpdateTimer(id, 0);
      }

      if (glProcessJanitor) {
         auto id = glProcessJanitor;
         glProcessJanitor = 0;
         UpdateTimer(id, 0);
      }

      // Remove the Task object and remaining children

      if (glCurrentTask) {
         pf::Log log("Shutdown");
         log.branch("Freeing the task object and its resources.");
         FreeResource(glCurrentTask);
      }

      // Remove locks on any private objects that have not been unlocked yet

      for (const auto & [ id, mem ] : glPrivateMemory) {
         if ((mem.Flags & MEM_OBJECT) and (mem.AccessCount > 0)) {
            if (auto obj = mem.Object) {
               log.warning("Removing locks on object #%d, Owner: %d, Locks: %d", obj->UID, obj->OwnerID, mem.AccessCount);
               for (auto count=mem.AccessCount; count > 0; count--) ReleaseObject(obj);
            }
         }
      }

      Expunge(false); // Second expunge.  Safety measures are still engaged.

      if (!glCrashStatus) {
         #ifdef __linux__
            if (glFileMonitor) { FreeResource(glFileMonitor); glFileMonitor = NULL; }
         #endif

         free_file_cache();

         if (glInotify != -1) { close(glInotify); glInotify = -1; }

         free_iconv();
      }

      Expunge(true); // Third and final expunge.  Forcibly unloads modules.

      VirtualVolume("archive", VAS_DEREGISTER, TAGEND);

      while (glMsgHandlers) FreeResource(glMsgHandlers);
      glLastMsgHandler = NULL;

      remove_semaphores();

      #ifdef __ANDROID__
      if (glAssetClass) { FreeResource(glAssetClass); glAssetClass = 0; }
      #endif
      if (glCompressedStreamClass) { FreeResource(glCompressedStreamClass); glCompressedStreamClass  = 0; }
      if (glArchiveClass)      { FreeResource(glArchiveClass);      glArchiveClass      = 0; }
      if (glCompressionClass)  { FreeResource(glCompressionClass);  glCompressionClass  = 0; }
      if (glScriptClass)       { FreeResource(glScriptClass);       glScriptClass       = 0; }
      if (glFileClass)         { FreeResource(glFileClass);         glFileClass         = 0; }
      if (glStorageClass)      { FreeResource(glStorageClass);      glStorageClass      = 0; }
      if (glConfigClass)       { FreeResource(glConfigClass);       glConfigClass       = 0; }
      if (glTimeClass)         { FreeResource(glTimeClass);         glTimeClass         = 0; }
      if (glModuleClass)       { FreeResource(glModuleClass);       glModuleClass       = 0; }
      if (glThreadClass)       { FreeResource(glThreadClass);       glThreadClass       = 0; }
      if (glRootModuleClass)   { FreeResource(glRootModuleClass);   glRootModuleClass   = 0; }

      #ifdef __unix__
         if (glSocket != -1) RegisterFD(glSocket, RFD_REMOVE, NULL, NULL);
      #endif

      // Report FD's that have not been removed by the client

      if (!glCrashStatus) {
         for (auto &fd : glFDTable) {
            log.warning("FD %" PF64 " was not deregistered prior to program close.  Routine: %p, Data: %p, Flags: $%.8x", (LARGE)fd.FD, fd.Routine, fd.Data, fd.Flags);
         }
      }
   }

   if (glCodeIndex < CP_REMOVE_PRIVATE_LOCKS) {
      glCodeIndex = CP_REMOVE_PRIVATE_LOCKS;

      log.msg("Removing all resource locks.");

      for (auto & [ id, mem ] : glPrivateMemory) {
         if ((mem.Address) and (mem.AccessCount > 0)) {
            if (!glCrashStatus) log.msg("Removing %d locks on private memory block #%d, size %d.", mem.AccessCount, mem.MemoryID, mem.Size);
            mem.AccessCount = 0;
         }
      }
   }

   if (!glCrashStatus) {
      if (glTaskClass) { FreeResource(glTaskClass); glTaskClass = 0; }
   }

   if (glCodeIndex < CP_FREE_COREBASE) {
      glCodeIndex = CP_FREE_COREBASE;
      if (LocalCoreBase) { FreeResource(LocalCoreBase); LocalCoreBase = NULL; }
   }

   if (glCodeIndex < CP_FREE_PRIVATE_MEMORY) {
      glCodeIndex = CP_FREE_PRIVATE_MEMORY;
      free_private_memory();
   }

   log.debug("Detaching from the shared memory control structure.");

   #ifdef _WIN32
      if (glSharedControl) {
         auto tmp = glSharedControl;
         glSharedControl = NULL;
         winUnmapViewOfFile(tmp);
      }
   #endif

   #ifdef __unix__
      free_shared_control();
   #endif

   #ifdef _WIN32
      free_threadlocks();

      // Remove semaphore controls

      for (LONG i=1; i < PL_END; i++) {
         if (glPublicLocks[i].Event) free_public_waitlock(glPublicLocks[i].Lock);
         else free_public_lock(glPublicLocks[i].Lock);
      }

      if (glSharedControlID) { winCloseHandle(glSharedControlID); glSharedControlID = 0; }

      winShutdown();
   #endif

   free_private_lock(TL_FIELDKEYS);
   free_private_lock(TL_CLASSDB);
   free_private_lock(TL_VOLUMES);
   free_private_lock(TL_GENERIC);
   free_private_lock(TL_TIMER);
   free_private_lock(TL_MSGHANDLER);
   free_private_lock(TL_OBJECT_LOOKUP);
   free_private_lock(TL_THREADPOOL);
   free_private_lock(TL_PRIVATE_MEM);
   free_private_lock(TL_PRINT);       // NB: After TL_PRINT is freed, any calls to message printing functions will result in a crash.
   free_private_cond(CN_PRIVATE_MEM);

   #ifdef __APPLE__
      LONG socklen;
      struct sockaddr_un *sockpath = get_socket_path(glProcessID, &socklen);
      unlink(sockpath->sun_path);
   #endif

   glProcessID = 0;

   if (glCodeIndex < CP_FINISHED) glCodeIndex = CP_FINISHED;

   fflush(stdout);
   fflush(stderr);

   // NOTE: LeakSanitizer can sometimes report segfault errors on closure.  These can go away on their own and may
   // not be easily duplicated.  One possible explanation is tom-foolery from LuaJIT resulting in false positives that
   // are hard to replicate.  Another is C++ destructors.  Module expunging can also result in false positives.
}

//********************************************************************************************************************
// Calls all loaded modules with an Expunge notification.
//
// NOTE: If forced expunging occurs, it usually means that two modules have loaded each other.  This means that they
// will always have an open count of at least 1 each.

#warning TODO: Expunging cannot occur while other threads are active.  In that case, the expunge should be delayed until no additional threads are running.

EXPORT void Expunge(WORD Force)
{
   pf::Log log(__FUNCTION__);

   if (!tlMainThread) {
      log.warning("Only the main thread can expunge modules.");
      return;
   }

   log.branch("Sending expunge call to all loaded modules.");

   WORD mod_count = -1;
   WORD ccount   = 0;
   WORD Pass     = 1;

   while (ccount > mod_count) {
      mod_count = ccount;
      auto mod_master = glModuleList;
      log.msg("Stage 1 pass #%d", Pass++);

      while (mod_master) {
         auto next = mod_master->Next;

         if (mod_master->OpenCount <= 0) {
            // Search for classes that have been created by this module and check their open count values to figure out
            // if the module code is in use.

            bool class_in_use = false;
            for (const auto & id : glObjectChildren[mod_master->UID]) {
               auto mem = glPrivateMemory.find(id);
               if (mem IS glPrivateMemory.end()) continue;

               auto mc = (extMetaClass *)mem->second.Address;
               if ((mc) and (mc->Class->ClassID IS ID_METACLASS) and (mc->OpenCount > 0)) {
                  log.msg("Module %s manages a class that is in use - Class: %s, Count: %d.", mod_master->Name, mc->ClassName, mc->OpenCount);
                  class_in_use = true;
               }
            }

            if (!class_in_use) {
               if (mod_master->Expunge) {
                  pf::Log log(__FUNCTION__);
                  log.branch("Sending expunge request to the %s module #%d.", mod_master->Name, mod_master->UID);
                  if (!mod_master->Expunge()) {
                     ccount++;
                     if (FreeResource(mod_master)) {
                        log.warning("RootModule data is corrupt");
                        mod_count = ccount; // Break the loop because the chain links are broken.
                        break;
                     }
                  }
                  else log.msg("Module \"%s\" does not want to be flushed.",mod_master->Name);
               }
               else {
                  ccount++;
                  if (FreeResource(mod_master)) {
                     log.warning("RootModule data is corrupt");
                     mod_count = ccount; // Break the loop because the chain links are broken.
                     break;
                  }
               }
            }
         }
         else log.msg("Module \"%s\" has an open count of %d.", mod_master->Name, mod_master->OpenCount);
         mod_master = next;
      }
   }

   if (Force) {
      // Any modules that are still in the system are probably there because they have created classes that have
      // objects still in use.  This routine prints warning messages to let the developer know about this.  (NB: There
      // can be times where objects are tracked outside of the process space and therefore will not be
      // destroyed by earlier routines.  This is normal and they will be taken out when the private memory resources
      // are deallocated).

      log.msg("Stage 2 expunge testing.");

      mod_count = -1;
      ccount   = 0;
      while (ccount > mod_count) {
         mod_count = ccount;
         auto mod_master = glModuleList;
         log.msg("Stage 2 pass #%d", Pass++);
         while (mod_master) {
            auto next = mod_master->Next;
            if (mod_master->OpenCount <= 0) {
               // Search for classes that have been created by this module and check their open count values to figure
               // out if the module code is in use.

               for (const auto & id : glObjectChildren[mod_master->UID]) {
                  auto mem = glPrivateMemory.find(id);
                  if (mem IS glPrivateMemory.end()) continue;

                  auto mc = (extMetaClass *)mem->second.Address;
                  if ((mc) and (mc->Class->ClassID IS ID_METACLASS) and (mc->OpenCount > 0)) {
                     log.warning("Warning: The %s module holds a class with existing objects (Class: %s, Objects: %d)", mod_master->Name, mc->ClassName, mc->OpenCount);
                  }
               }
            }
            else log.msg("Module \"%s\" has an open count of %d.", mod_master->Name, mod_master->OpenCount);
            mod_master = next;
         }
      }

      // If we are shutting down, force the expunging of any stubborn modules

      auto mod_master = glModuleList;
      while (mod_master) {
         auto next = mod_master->Next;
         if (mod_master->Expunge) {
            pf::Log log(__FUNCTION__);
            log.branch("Forcing the expunge of stubborn module %s.", mod_master->Name);
            mod_master->Expunge();
            mod_master->NoUnload = true; // Do not actively destroy the module code as a precaution
            FreeResource(mod_master);
         }
         else {
            ccount++;
            FreeResource(mod_master);
         }
         mod_master = next;
      }
   }
}

//********************************************************************************************************************

static void free_private_memory(void)
{
   pf::Log log("Shutdown");

   log.branch("Freeing private memory allocations...");

   // Free strings first

   LONG count = 0;
   for (auto & [ id, mem ] : glPrivateMemory) {
      if ((mem.Address) and (mem.Flags & MEM_STRING)) {
         if (!glCrashStatus) log.warning("Unfreed string \"%.80s\" (%p, #%d)", (CSTRING)mem.Address, mem.Address, mem.MemoryID);
         mem.AccessCount = 0;
         FreeResource(mem.Address);
         mem.Address = NULL;
         count++;
      }
   }

   // Free all other memory blocks

   for (auto & [ id, mem ] : glPrivateMemory) {
      if (mem.Address) {
         if (!glCrashStatus) {
            if (mem.Flags & MEM_OBJECT) {
               log.warning("Unfreed object #%d, Size %d, Class: $%.8x, Container: #%d.", mem.MemoryID, mem.Size, mem.Object->Class->ClassID, mem.OwnerID);
            }
            else log.warning("Unfreed memory #%d/%p, Size %d, Container: #%d.", mem.MemoryID, mem.Address, mem.Size, mem.OwnerID);
         }
         mem.AccessCount = 0;
         FreeResource(mem.Address);
         mem.Address = NULL;
         count++;
      }
   }

   if ((glCrashStatus) and (count > 0)) log.msg("%d memory blocks were freed.", count);
}

//********************************************************************************************************************

#ifdef __unix__
static void free_shared_control(void)
{
   pf::Log log("Shutdown");

   KMSG("free_shared_control()\n");

   #ifdef USE_SHM
      shmdt(glSharedControl);
      glSharedControl = NULL;

      KMSG("This is the last process - now marking the shared mempool for deletion.\n");

      if (glSharedControlID != -1) {
         if (shmctl(glSharedControlID, IPC_RMID, NULL) IS -1) {
            KERR("shmctl() failed to kill the public memory pool: %s\n", strerror(errno));
         }
         glSharedControlID = -1;
      }
   #else
      if (glMemoryFD != -1) {
         close(glMemoryFD);
         glMemoryFD = -1;
      }

      // Delete the memory mapped file if this is the last process to use it

      #ifndef __ANDROID__
         if (glDebugMemory IS false) {
            log.msg("I am the last task - now closing the memory mapping.");
            unlink(MEMORYFILE);
         }
      #endif

   #endif

   if (glSocket != -1) {
      close(glSocket);
      glSocket = -1;
   }
}
#endif

