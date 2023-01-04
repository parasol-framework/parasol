
static void free_private_memory(void);
static void remove_private_locks(void);
static void free_shared_objects(void);
static ERROR free_shared_object(LONG ObjectID, OBJECTID OwnerID);
#ifdef __unix__
static void free_shared_control(void);
#endif

//****************************************************************************

EXPORT void CloseCore(void)
{
   parasol::Log log("Shutdown");
   LONG i, j, count;

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

   if (glClassFileID) {
      ActionMsg(AC_Free, glClassFileID, NULL, 0, 0);
      glClassFileID = 0;
   }

   free_events(); // Remove event subscriptions

   // Deallocate any semaphores that we've left in the system early if we've crashed

   if (glCrashStatus) remove_semaphores();

   // If we are a master task, destroy all other tasks in our instance that we have created.

   if (glMasterTask) {

      {
         parasol::Log log("Shutdown");
         log.branch("Removing any child processes...");

         #ifdef KILL_PROCESS_GROUP
            // Kill all processes that have been created by this process and its children
            killpg(0, SIGHUP);
         #else
            for (LONG i=0; i < MAX_TASKS; i++) {
               if ((shTasks[i].ProcessID) and (shTasks[i].ProcessID != glProcessID)) {
                  log.msg("Removing sub-process #%d (pid %d).", shTasks[i].TaskID, shTasks[i].ProcessID);

                  #ifdef __unix__
                     // SIGHUP will convert to MSGID_QUIT in the signal handlers.  The main reason for us to use it is
                     // to stop foreign processes that we've launched.
                     kill(shTasks[i].ProcessID, SIGHUP);
                  #else
                     if (shTasks[i].MessageID) SendMessage(shTasks[i].MessageID, MSGID_QUIT, 0, NULL, 0);
                  #endif

                  WaitTime(0, -100000);
               }
            }
         #endif
      }

      // Wait for sub-tasks to die within the designated time limit

      log.msg("Waiting for child processes to terminate...");

      #define TIMETODIE 6 // Seconds to wait before a task has to die
      LONG cycle;
      for (cycle=0; cycle < (TIMETODIE * 10); cycle++) {
         for (j=0; j < MAX_TASKS; j++) { // Break if any other process is found in the task array.
            if ((shTasks[j].ProcessID) and (shTasks[j].ProcessID != glProcessID)) {
               log.msg("Process %d is still live.", shTasks[j].ProcessID);
               break;
            }
         }

         if (j >= MAX_TASKS) break;
         WaitTime(0, -100000);
      }

      // If the time-to-die has elapsed and sub-tasks are still in the system, send kill messages to force them out.

      #ifdef __unix__
         if (cycle >= TIMETODIE) {
            for (LONG j=0; j < MAX_TASKS; j++) {
               log.warning("Sending a kill signal to sub-task #%d (process %d).", shTasks[j].TaskID, shTasks[j].ProcessID);
               if ((shTasks[j].ProcessID) and (shTasks[j].ProcessID != glProcessID)) {
                  kill(shTasks[j].ProcessID, SIGTERM);
               }
            }
            WaitTime(0, -200000);
         }
      #endif
   }

   // Clear the glTaskMessageMID if we have crashed.  Otherwise do not alter this variable as we still need it for
   // destroying public objects that belong to the Task.

   if (glCrashStatus) glTaskMessageMID = 0;

   // Run the video recovery routine if one has been set and we have crashed

   if ((glCrashStatus) and (glVideoRecovery)) {
      void (*routine)(void);
      routine = glVideoRecovery;
      glVideoRecovery = NULL;
      routine();
   }

   // Clear up the internal thread pool.

   remove_threadpool();

   // Make sure that the task list shows that we are not waiting for anyone to send us messages.  If necessary, wake up
   // foreign tasks that are sleeping on our process. Please note that we do not remove our task completely from the global
   // list just yet, as cooperation with other tasks is often needed when shared objects are freed during the shutdown
   // process.

   if ((glCurrentTaskID) or (glProcessID)) remove_process_waitlocks();

   // Remove locks from private and public memory blocks

   if (glCrashStatus) {
      log.msg("Forcibly removing all resource locks.");

      if (glCodeIndex < CP_REMOVE_PRIVATE_LOCKS) {
         glCodeIndex = CP_REMOVE_PRIVATE_LOCKS;
         remove_private_locks();
      }

      if (glCodeIndex < CP_REMOVE_PUBLIC_LOCKS) {
         glCodeIndex = CP_REMOVE_PUBLIC_LOCKS;
         remove_public_locks(glProcessID);
      }

      // Free all public memory blocks that are tracked to this Task

      if (glCodeIndex < CP_FREE_PUBLIC_MEMORY) {
         glCodeIndex = CP_FREE_PUBLIC_MEMORY;
         free_public_resources(glCurrentTaskID);
      }
   }
   else { // This code is only safe to execute if the process hasn't crashed.
      // Remove locks on public objects that we have not unlocked yet.  We do this by setting the lock-count to zero so
      // that others can then gain access to the public object.

      if ((glSharedControl) and (glSharedBlocks)) {
         log.trace("Removing locks on public objects.");

         if (!LOCK_PUBLIC_MEMORY(4000)) {
            for (i=glSharedControl->NextBlock-1; i >= 0; i--) {
               if ((glSharedBlocks[i].ProcessLockID IS glProcessID) and (glSharedBlocks[i].AccessCount > 0)) {
                  if (glSharedBlocks[i].Flags & MEM_OBJECT) {
                     if (auto header = (OBJECTPTR)resolve_public_address(glSharedBlocks + i)) {
                        log.warning("Removing %d exclusive locks on object #%d (memory %d).", glSharedBlocks[i].AccessCount, header->UID, glSharedBlocks[i].MemoryID);
                        for (count=glSharedBlocks[i].AccessCount; count > 0; count--) {
                           ReleaseObject(header);
                        }
                     }
                  }
               }
            }
            UNLOCK_PUBLIC_MEMORY();
         }
      }

      if (glLocale) { acFree(glLocale); glLocale = NULL; } // Allocated by StrReadLocale()
      if (glTime) { acFree(glTime); glTime = NULL; }

      // Removing any objects that are tracked to the task before we perform the first expunge will help make for a
      // cleaner exit.

      if (glCurrentTask) {
         const auto children = glObjectChildren[glCurrentTask->UID]; // Take an immutable copy of the resource list

         if (children.size() > 0) {
            log.branch("Freeing %d objects allocated to task #%d.", (LONG)children.size(), glCurrentTask->UID);

            for (const auto id : children) ActionMsg(AC_Free, id, NULL, 0, 0);
         }
         else log.msg("There are no child objects belonging to task #%d.", glCurrentTask->UID);
      }

      // Make our first attempt at expunging all modules.  Notice that we terminate all public objects that are owned
      // by our processes first - we need to do this before the expunge because otherwise the module code won't be
      // available to destroy the public objects.

      free_shared_objects();

      Expunge(FALSE);

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

      // Remove the Task structure and child objects

      if (glCurrentTask) {
         parasol::Log log("Shutdown");
         log.branch("Freeing the task object and its resources.");
         acFree(glCurrentTask);

         // Remove allocated objects that are public/shared

         //free_shared_objects();
      }

      // Free objects that are owned by an object in a foreign process (such objects fall out of the natural object
      // hierarchy, so they can be left in limbo if their owner is freed in the foreign process).

      {
         parasol::Log log("Shutdown");
         log.branch("Freeing objects owned by foreign processes.");

         for (const auto & [id, mem ] : glPrivateMemory) {
            if (mem.Flags & MEM_OBJECT) {
               auto obj = mem.Object;
               if (obj) {
                  if ((obj->Stats) and (obj->Flags & NF_FOREIGN_OWNER)) {
                     acFree(obj);
                     // Don't be concerned about the stability of glPrivateMemory.  FreeResource() takes measures
                     // to avoid destabilising it during shutdown.
                  }
               }
            }
         }
      }

      // Remove locks on any private objects that have not been unlocked yet

      for (const auto & [ id, mem ] : glPrivateMemory) {
         if ((mem.Flags & MEM_OBJECT) and (mem.AccessCount > 0)) {
            auto obj = mem.Object;
            if (obj) {
               log.warning("Removing locks on object #%d, Owner: %d, Locks: %d", obj->UID, obj->OwnerID, mem.AccessCount);
               for (count=mem.AccessCount; count > 0; count--) ReleaseObject(obj);
            }
         }
      }

      Expunge(FALSE);

      if (!glCrashStatus) {
         #ifdef __linux__
            if (glFileMonitor) { FreeResource(glFileMonitor); glFileMonitor = NULL; }
         #endif

         free_file_cache();

         if (glInotify != -1) { close(glInotify); glInotify = -1; }

         free_iconv();
      }

      Expunge(TRUE);

      VirtualVolume("archive", VAS_DEREGISTER, TAGEND);

      if (glVolumes) { acFree(glVolumes); glVolumes = NULL; }

      // Remove all message handlers

      while (glMsgHandlers) FreeResource(glMsgHandlers);
      glLastMsgHandler = NULL;

      if ((glCrashStatus) and (glSharedControl) and (glSharedControl->InstanceMsgPort) and (!glMasterTask)) {
         ValidateMessage msg;
         msg.ProcessID = glProcessID;
         SendMessage(glSharedControl->InstanceMsgPort, MSGID_VALIDATE_PROCESS, 0, &msg, sizeof(msg));
      }

      // Remove semaphore allocations

      remove_semaphores();

      // Remove system classes

      #ifdef __ANDROID__
      if (glAssetClass) { acFree(glAssetClass); glAssetClass = 0; }
      #endif
      if (glCompressedStreamClass) { acFree(glCompressedStreamClass); glCompressedStreamClass  = 0; }
      if (glArchiveClass)     { acFree(glArchiveClass);     glArchiveClass  = 0; }
      if (glCompressionClass) { acFree(glCompressionClass); glCompressionClass = 0; }
      if (glScriptClass)      { acFree(glScriptClass);      glScriptClass  = 0; }
      if (glFileClass)        { acFree(glFileClass);        glFileClass    = 0; }
      if (glStorageClass)     { acFree(glStorageClass);     glStorageClass = 0; }
      if (ConfigClass)        { acFree(ConfigClass);        ConfigClass    = 0; }
      if (TimeClass)          { acFree(TimeClass);          TimeClass      = 0; }
      if (ModuleClass)        { acFree(ModuleClass);        ModuleClass    = 0; }
      if (ThreadClass)        { acFree(ThreadClass);        ThreadClass    = 0; }
      if (ModuleMasterClass)  { acFree(ModuleMasterClass);  ModuleMasterClass = 0; }

      // Remove access to class database

      if (glMasterTask) {
         if (glClassDB) {
            MemInfo info;
            log.debug("Removing class database.");
            if (!MemoryPtrInfo(glClassDB, &info, sizeof(info))) {
               FreeResourceID(info.MemoryID); // Mark for deletion
            }
         }

         if (glModules) {
            MemInfo info;
            log.debug("Removing module database.");
            if (!MemoryPtrInfo(glModules, &info, sizeof(info))) {
               FreeResourceID(info.MemoryID); // Mark for deletion
            }
         }
      }

      if (glClassDB) { ReleaseMemory(glClassDB); glClassDB = NULL; }
      if (glModules) { ReleaseMemory(glModules); glModules = NULL; }

      // Check FD list and report FD's that have not been removed

      #ifdef __unix__
         if (glSocket != -1) RegisterFD(glSocket, RFD_REMOVE, NULL, NULL);
      #endif

      if ((!glCrashStatus) and (glFDTable)) {
         for (LONG i=0; i < glTotalFDs; i++) {
            if (glFDTable[i].FD) {
               log.warning("FD %" PF64 " was not deregistered prior to program close.  Routine: %p, Data: %p, Flags: $%.8x", (LARGE)glFDTable[i].FD, glFDTable[i].Routine, glFDTable[i].Data, glFDTable[i].Flags);
            }
         }
      }

      if (glFDTable) {
         free(glFDTable);
         glFDTable = NULL;
         glTotalFDs = 0;
      }

      log.trace("Removing private and public memory locks.");

      remove_private_locks();

      remove_public_locks(glProcessID);

      // Free all public memory blocks that are tracked to this process.

      if (glCodeIndex < CP_FREE_PUBLIC_MEMORY) {
         glCodeIndex = CP_FREE_PUBLIC_MEMORY;
         free_public_resources(glCurrentTaskID);
      }
   }

   // Remove our process from the global list completely IF THIS IS THE MASTER TASK.  Note that from this point onwards
   // we will not be able to interact with any other processes, so all types of sharing/locking is disallowed henceforth.

   if ((glMasterTask) and (glTaskEntry)) {
      if (LOCK_PROCESS_TABLE(4000) IS ERR_Okay) {
         ClearMemory(glTaskEntry, sizeof(TaskList));
         glTaskEntry = NULL;
         UNLOCK_PROCESS_TABLE();
      }
   }

   // The object name lookup table is no longer required

   if (glObjectLookup) { FreeResource(glObjectLookup); glObjectLookup = NULL; }
   if (glFields) { FreeResource(glFields); glFields = NULL; }

   // Unless we have crashed, free the Task class

   if (!glCrashStatus) {
      if (TaskClass) { acFree(TaskClass); TaskClass = 0; }
      if (glClassMap) { FreeResource(glClassMap); glClassMap = NULL; }
   }

   // Remove the action management structure

   if (glCodeIndex < CP_FREE_ACTION_MANAGEMENT) {
      glCodeIndex = CP_FREE_ACTION_MANAGEMENT;
      if (ManagedActions) { FreeResource(ManagedActions); ManagedActions = NULL; }
   }

   // Free the program's personal function base as it won't be making any more calls.

   if (glCodeIndex < CP_FREE_COREBASE) {
      glCodeIndex = CP_FREE_COREBASE;
      if (LocalCoreBase) { FreeResource(LocalCoreBase); LocalCoreBase = NULL; }
   }

   // Free memory pages

   if (glCodeIndex < CP_FREE_MEMORY_PAGES) {
      glCodeIndex = CP_FREE_MEMORY_PAGES;
      if (glMemoryPages) { free(glMemoryPages); glMemoryPages = NULL; }
   }

   // Free private memory blocks

   if (glCodeIndex < CP_FREE_PRIVATE_MEMORY) {
      glCodeIndex = CP_FREE_PRIVATE_MEMORY;
      free_private_memory();
   }

   log.debug("Detaching from the shared memory control structure.");

   // Detach from the shared memory control structure

   #ifdef _WIN32
      if (glSharedControl) {
         SharedControl *tmp = glSharedControl;
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

      for (i=1; i < PL_END; i++) {
         if (glPublicLocks[i].Event) free_public_waitlock(glPublicLocks[i].Lock);
         else free_public_lock(glPublicLocks[i].Lock);
      }

      if (glSharedControlID) { winCloseHandle(glSharedControlID); glSharedControlID = 0; }

      //winDeathBringer(1); // Doesn't work properly if you CTRL-C to exit...

      winShutdown();
   #endif

   free_private_lock(TL_GENERIC);
   free_private_lock(TL_TIMER);
   free_private_lock(TL_MSGHANDLER);
   free_private_lock(TL_MEMORY_PAGES);
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

   glCurrentTask = NULL;
   glCurrentTaskID = 0;
   glProcessID = 0;

   if (glCodeIndex < CP_FINISHED) glCodeIndex = CP_FINISHED;

   fflush(stdout);
   fflush(stderr);
}

/*****************************************************************************
** Calls all loaded modules with an Expunge notification.
**
** NOTE: If forced expunging occurs, it usually means that two modules have loaded each other.  This means that they
** will always have an open count of at least 1 each.
*/

#warning TODO: Expunging cannot occur while other threads are active.  In that case, the expunge should be delayed until no additional threads are running.

EXPORT void Expunge(WORD Force)
{
   parasol::Log log(__FUNCTION__);

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
               if ((mc) and (mc->ClassID IS ID_METACLASS) and (mc->OpenCount > 0)) {
                  log.msg("Module %s manages a class that is in use - Class: %s, Count: %d.", mod_master->Name, mc->ClassName, mc->OpenCount);
                  class_in_use = true;
               }
            }

            if (!class_in_use) {
               if (mod_master->Expunge) {
                  parasol::Log log(__FUNCTION__);
                  log.branch("Sending expunge request to the %s module, routine %p, master #%d.", mod_master->Name, mod_master->Expunge, mod_master->UID);
                  if (!mod_master->Expunge()) {
                     ccount++;
                     if (acFree(mod_master)) {
                        log.warning("ModuleMaster data is corrupt");
                        mod_count = ccount; // Break the loop because the chain links are broken.
                        break;
                     }
                  }
                  else log.msg("Module \"%s\" does not want to be flushed.",mod_master->Name);
               }
               else {
                  ccount++;
                  if (acFree(mod_master)) {
                     log.warning("ModuleMaster data is corrupt");
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
      // can be times where private objects are tracked outside of the process space and therefore will not be
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
                  if ((mc) and (mc->ClassID IS ID_METACLASS) and (mc->OpenCount > 0)) {
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
            parasol::Log log(__FUNCTION__);
            log.branch("Forcing the expunge of stubborn module %s.", mod_master->Name);
            mod_master->Expunge();
            mod_master->NoUnload = TRUE; // Do not actively destroy the module code as a precaution
            acFree(mod_master);
         }
         else {
            ccount++;
            acFree(mod_master);
         }
         mod_master = next;
      }
   }
}

/*****************************************************************************
** Scans for public/shared objects and kills them if they belong to our process.
*/

static void free_shared_objects(void)
{
   parasol::Log log("Shutdown");

   if ((!glSharedControl) or (!glCurrentTaskID)) return;

   log.branch("Freeing public objects allocated by process %d.", glCurrentTaskID);

   if (!LOCK_PUBLIC_MEMORY(4000)) {
      // First, remove objects that have no owners (i.e. the top most objects).  This ensures that child objects are
      // removed correctly, as it means that the deallocation process follows normal hierarchical rules.

      for (LONG i=glSharedControl->NextBlock-1; i >= 0; i--) {
         if ((glSharedBlocks[i].TaskID IS glCurrentTaskID) and (glSharedBlocks[i].Flags & MEM_OBJECT)) {
            OBJECTPTR hdr;
            if (page_memory(glSharedBlocks + i, (APTR *)&hdr) IS ERR_Okay) {
               if (hdr->OwnerID) {
                  // The object has an owner, so scan towards the topmost object within our process space
                  OBJECTID id = hdr->UID;
                  OBJECTID owner = hdr->OwnerID;
                  unpage_memory(hdr);
                  if (free_shared_object(id, owner) != ERR_Okay) break;
               }
               else {
                  unpage_memory(hdr);
                  OBJECTID id = glSharedBlocks[i].MemoryID;
                  UNLOCK_PUBLIC_MEMORY();
                  ActionMsg(AC_Free, id, NULL, 0, 0);
                  if (LOCK_PUBLIC_MEMORY(4000) != ERR_Okay) break;
               }
            }
         }
      }

      for (LONG i=glSharedControl->NextBlock-1; i >= 0; i--) {
         if ((glSharedBlocks[i].TaskID IS glCurrentTaskID) and (glSharedBlocks[i].Flags & MEM_OBJECT)) {
            OBJECTPTR hdr;
            if (page_memory(glSharedBlocks + i, (APTR *)&hdr) IS ERR_Okay) {
               OBJECTID id;
               if ((!hdr->OwnerID) or (hdr->OwnerID IS glCurrentTaskID)) id = glSharedBlocks[i].MemoryID;
               else id = 0;
               unpage_memory(hdr);
               UNLOCK_PUBLIC_MEMORY();

               if (id) ActionMsg(AC_Free, id, NULL, 0, 0);

               if (LOCK_PUBLIC_MEMORY(4000) != ERR_Okay) break;
            }
         }
      }

      // Now deallocate all objects related to our process regardless of their position in the hierarchy.

      for (LONG i=glSharedControl->NextBlock-1; i >= 0; i--) {
         if ((glSharedBlocks[i].TaskID IS glCurrentTaskID) and (glSharedBlocks[i].Flags & MEM_OBJECT)) {
            OBJECTID id = glSharedBlocks[i].MemoryID;
            UNLOCK_PUBLIC_MEMORY();

            ActionMsg(AC_Free, id, NULL, 0, 0);

            if (LOCK_PUBLIC_MEMORY(4000) != ERR_Okay) break;
         }
      }
      UNLOCK_PUBLIC_MEMORY();
   }
}

// This function requires LOCK_PUBLIC_MEMORY() to be in use and can only be called from free_shared_objects()

static ERROR free_shared_object(LONG ObjectID, OBJECTID OwnerID)
{
   for (LONG i=glSharedControl->NextBlock-1; i >= 0; i--) {
      if (glSharedBlocks[i].MemoryID IS OwnerID) { // Owner found
         if ((glSharedBlocks[i].TaskID IS glCurrentTaskID) and (glSharedBlocks[i].Flags & MEM_OBJECT)) { // Does the owner belong to our process?
            if (auto header = (OBJECTPTR)resolve_public_address(glSharedBlocks + i)) {
               if (header->OwnerID) {
                  return free_shared_object(OwnerID, header->OwnerID);
               }
               else {
                  UNLOCK_PUBLIC_MEMORY();
                  ActionMsg(AC_Free, ObjectID, NULL, 0, 0);
                  return LOCK_PUBLIC_MEMORY(4000);
               }
            }
         }
         else break; // Break loop and free the object because we know that the current object is top-most, relative to our process
      }
   }

   // If the owner does not exist or the owner belongs to a different process then the loop drops down to here and we
   // can free the object.

   UNLOCK_PUBLIC_MEMORY();
   ActionMsg(AC_Free, ObjectID, NULL, 0, 0);
   return LOCK_PUBLIC_MEMORY(4000);
}

//**********************************************************************

static void free_private_memory(void)
{
   parasol::Log log("Shutdown");

   log.branch("Freeing private memory allocations...");

   // Free strings first

   LONG count = 0;
   for (auto & [ id, mem ] : glPrivateMemory) {
      if ((mem.Address) and (mem.Flags & MEM_STRING)) {
         if (!glCrashStatus) log.warning("Unfreed private string \"%.80s\" (%p).", (CSTRING)mem.Address, mem.Address);
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
               log.warning("Unfreed private object #%d, Size %d, Class: $%.8x, Container: #%d.", mem.MemoryID, mem.Size, mem.Object->ClassID, mem.OwnerID);

               if (mem.Object->Flags & NF_PUBLIC) remove_shared_object(mem.MemoryID);
            }
            else log.warning("Unfreed private memory #%d/%p, Size %d, Container: #%d.", mem.MemoryID, mem.Address, mem.Size, mem.OwnerID);
         }
         mem.AccessCount = 0;
         FreeResource(mem.Address);
         mem.Address = NULL;
         count++;
      }
   }

   if ((glCrashStatus) and (count > 0)) log.msg("%d private memory blocks were freed.", count);
}


/*****************************************************************************
** Frees public objects and memory blocks.  Please note that this routine is also used by validate_process().
*/

void free_public_resources(OBJECTID TaskID)
{
   parasol::Log log(__FUNCTION__);

   if (!TaskID) return;

   log.msg("Freeing all public objects & memory belonging to task #%d", TaskID);

   parasol::ScopedSysLock lock(PL_PUBLICMEM, 4000);
   if (lock.granted()) {
      for (LONG i=glSharedControl->NextBlock-1; i >= 0; i--) {
         if ((glSharedBlocks[i].TaskID IS TaskID) or (glSharedBlocks[i].ObjectID IS TaskID)) {
            if (glSharedBlocks[i].Flags & MEM_OBJECT) {
               if (TaskID IS glCurrentTaskID) {
                  APTR address;
                  if ((address = resolve_public_address(glSharedBlocks + i))) {
                     for (LONG j=glSharedBlocks[i].AccessCount; j > 0; j--) ReleaseMemory(address);
                  }
               }

               if (glSharedBlocks[i].AccessCount > 0) { // Forcibly remove locks if ReleaseMemory() couldn't
                  ClearMemory(glSharedBlocks+i, sizeof(glSharedBlocks[0]));
               }

               log.msg("Freeing public object header #%d.", glSharedBlocks[i].MemoryID);

               OBJECTID id = glSharedBlocks[i].MemoryID;
               FreeResourceID(glSharedBlocks[i].MemoryID);

               lock.release();

               remove_shared_object(id);  // Remove the object entry from the shared object list

               if (lock.acquire(5000) != ERR_Okay) break;
            }
            else if (glSharedBlocks[i].MemoryID) {
               log.msg("Freeing public memory block #%d.", glSharedBlocks[i].MemoryID);
               FreeResourceID(glSharedBlocks[i].MemoryID);
            }
         }
      }
   }
}

/*****************************************************************************
** This function does not release locks on public objects (special handling for objects can be found elsewhere in the
** shutdown program flow).  NOTE:  This function is also used by validate_process() to clear zombie resource locks.
*/

void remove_public_locks(LONG ProcessID)
{
   parasol::Log log(__FUNCTION__);

   if ((!glSharedControl) or (!glSharedBlocks)) return;

   log.branch("Process: %d", ProcessID);

   parasol::ScopedSysLock lock(PL_PUBLICMEM, 4000);
   if (lock.granted()) {
      // Release owned public blocks

      for (LONG i=glSharedControl->NextBlock-1; i >= 0; i--) {
         if ((glSharedBlocks[i].ProcessLockID IS ProcessID) and (glSharedBlocks[i].AccessCount > 0)) {
            if (glSharedBlocks[i].Flags & MEM_OBJECT) {
               if (auto header = (OBJECTPTR)resolve_public_address(glSharedBlocks + i)) {
                  header->Locked = 0;
               }
               log.msg("Removing %d locks on shared object #%d.", glSharedBlocks[i].AccessCount, glSharedBlocks[i].MemoryID);
            }
            else log.msg("Removing %d locks on shared memory block #%d.", glSharedBlocks[i].AccessCount, glSharedBlocks[i].MemoryID);

            APTR address;
            if ((address = resolve_public_address(glSharedBlocks + i))) {
               for (LONG j=glSharedBlocks[i].AccessCount; j > 0; j--) ReleaseMemory(address);
            }

            if (glSharedBlocks[i].AccessCount > 0) { // Forcibly remove locks if ReleaseMemory() couldn't
               ClearMemory(glSharedBlocks+i, sizeof(glSharedBlocks[0]));
            }
         }
      }

      // Release any *non-blocking* locks.  These are usually attributed to the RPM's (Reserved Public Memory ID's).

      if (shTasks) {
         LONG task;
         for (task=0; (task < MAX_TASKS) and (shTasks[task].ProcessID != ProcessID); task++);

         if (task < MAX_TASKS) {
            for (LONG i=MAX_NB_LOCKS-1; i >= 0; i--) {
               if (!shTasks[task].NoBlockLocks[i].MemoryID) continue;

               log.warning("Removing %d non-blocking locks on memory block #%d.", shTasks[task].NoBlockLocks[i].AccessCount, shTasks[task].NoBlockLocks[i].MemoryID);

               LONG block;
               if (!find_public_mem_id(glSharedControl, shTasks[task].NoBlockLocks[i].MemoryID, &block)) {
                  APTR address;
                  if ((address = resolve_public_address(glSharedBlocks + block))) {
                     for (LONG j=shTasks[task].NoBlockLocks[i].AccessCount; j > 0; j--) ReleaseMemory(address);
                  }
               }
            }
         }
      }
   }
}

//**********************************************************************

static void remove_private_locks(void)
{
   parasol::Log log("Shutdown");

   for (auto & [ id, mem ] : glPrivateMemory) {
      if ((mem.Address) and (mem.AccessCount > 0)) {
         if (!glCrashStatus) log.msg("Removing %d locks on private memory block #%d, size %d.", mem.AccessCount, mem.MemoryID, mem.Size);
         mem.AccessCount = 0;
      }
   }
}

//****************************************************************************

#ifdef __unix__
static void free_shared_control(void)
{
   parasol::Log log("Shutdown");

   KMSG("free_shared_control()\n");

   glTaskEntry = NULL;

   if (!SysLock(PL_FORBID, 4000)) {
      LONG i;
      LONG taskcount = 0;

      // Check if we are the last active task

      if ((glMasterTask) and (glSharedControl->GlobalInstance) and
          (glInstanceID IS glSharedControl->GlobalInstance)) {
         // If we're the master, global task for everything, we're taking all the resources down, no matter what else
         // we have running.
      }
      else for (LONG i=0; i < MAX_TASKS; i++) {
         if (shTasks[i].ProcessID) {
            if ((kill(shTasks[i].ProcessID, 0) IS -1) and (errno IS ESRCH));
            else taskcount++;
         }
      }

      if (taskcount < 1) {
         for (i=1; i < PL_END; i++) free_public_lock(i);
      }

      #ifdef USE_SHM
         if (taskcount < 1) {
            // Mark all shared memory blocks for deletion.
            //
            // You can check the success of this routine by running "ipcs", which lists allocated shm blocks.

            for (LONG i=glSharedControl->NextBlock-1; i >= 0; i--) {
               if (glSharedBlocks[i].MemoryID) {
                  LONG id;
                  if ((id = shmget(SHMKEY + glSharedBlocks[i].MemoryID, glSharedBlocks[i].Size, S_IRWXO|S_IRWXG|S_IRWXU)) != -1) {
                     shmctl(id, IPC_RMID, NULL);
                  }
               }
            }
         }

         SysUnlock(PL_FORBID);

         shmdt(glSharedControl);
         glSharedControl = NULL;

         if (taskcount < 1) {
            KMSG("This is the last process - now marking the shared mempool for deletion.\n");

            if (glSharedControlID != -1) {
               if (shmctl(glSharedControlID, IPC_RMID, NULL) IS -1) {
                  KERR("shmctl() failed to kill the public memory pool: %s\n", strerror(errno));
               }
               glSharedControlID = -1;
            }
         }
         else log.msg("There are %d tasks left in the system.", taskcount);

      #else

         SysUnlock(PL_FORBID);

         munmap(glSharedControl, glSharedControl->MemoryOffset);
         glSharedControl = NULL;

         if (glMemoryFD != -1) {
            close(glMemoryFD);
            glMemoryFD = -1;
         }

         // Delete the memory mapped file if this is the last process to use it

         if (taskcount < 1) {
            #ifndef __ANDROID__
               if (glDebugMemory IS FALSE) {
                  log.msg("I am the last task - now closing the memory mapping.");
                  unlink(MEMORYFILE);
               }
            #endif
         }
         else log.msg("There are %d tasks left in the system.", taskcount);

      #endif
   }
   else log.msg("Unable to SysLock() for closing the public control structure.");

   if (glSocket != -1) {
      close(glSocket);
      glSocket = -1;
   }
}
#endif
