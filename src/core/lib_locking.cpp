/****************************************************************************

The source code of the Parasol Framework is made publicly available under the
terms described in the LICENSE.TXT file that is distributed with this package.
Please refer to it for further information on licensing.

-CATEGORY-
Name: Locks
-END-

CORE LOCKING MANAGEMENT
-------------------------
Most technical code regarding system locking is managed in this area.  Also check out lib_semaphores.c and lib_messages.c.

*****************************************************************************/

#ifndef DBG_LOCKS // Debugging is off unless DBG_LOCKS is explicitly defined.
#undef DEBUG
#endif

//#define DBG_OBJECTLOCKS

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifdef __unix__
  #include <errno.h>
  #include <sys/socket.h>
  #ifndef USE_SHM
    #include <sys/mman.h>
  #endif
  #ifndef __ANDROID__
    #include <sys/shm.h>
  #endif
#endif

#include "defs.h"

using namespace parasol;

static BYTE glAlwaysUnpage = FALSE; // Forces unpaging of memory in all circumstances (slows system, good for debugging)

#ifdef _WIN32
THREADVAR WORD tlMessageBreak = FALSE; // This variable is set by ProcessMessages() to allow breaking when Windows sends OS messages
#endif

//****************************************************************************
// POSIX compatible lock allocation functions.
// Note: THREADLOCK == pthread_mutex_t; CONDLOCK == pthread_cond_t

#ifdef __unix__
static ERROR alloc_lock(THREADLOCK *Lock, WORD Flags);
static ERROR alloc_cond(CONDLOCK *Lock, WORD Flags);
static void free_cond(CONDLOCK *Cond);
static void free_lock(THREADLOCK *Lock);

static THREADLOCK glPrivateLocks[TL_END];
static CONDLOCK   glPrivateCond[CN_END];

#ifdef __APPLE__
struct sockaddr_un * get_socket_path(LONG ProcessID, socklen_t *Size)
{
   // OSX doesn't support anonymous sockets, so we use /tmp instead.
   static THREADVAR struct sockaddr_un tlSocket;
   tlSocket.sun_family = AF_UNIX;
   *Size = sizeof(sa_family_t) + snprintf(tlSocket.sun_path, sizeof(tlSocket.sun_path), "/tmp/parasol.%d", ProcessID) + 1;
   return &tlSocket;
}
#else
struct sockaddr_un * get_socket_path(LONG ProcessID, socklen_t *Size)
{
   static THREADVAR struct sockaddr_un tlSocket;
   static THREADVAR UBYTE init = FALSE;

   if (!init) {
      tlSocket.sun_family = AF_UNIX;
      ClearMemory(tlSocket.sun_path, sizeof(tlSocket.sun_path));
      tlSocket.sun_path[0] = '\0';
      tlSocket.sun_path[1] = 'p';
      tlSocket.sun_path[2] = 's';
      tlSocket.sun_path[3] = 'l';
      init = TRUE;
   }

   ((LONG *)(tlSocket.sun_path+4))[0] = ProcessID;
   *Size = sizeof(sa_family_t) + 4 + sizeof(LONG);
   return &tlSocket;
}
#endif

ERROR alloc_public_lock(UBYTE LockIndex, WORD Flags)
{
   if ((LockIndex < 1) or (LockIndex >= PL_END)) return ERR_Args;
   if (!glSharedControl) return ERR_Failed;
   ERROR error;
   error = alloc_lock(&glSharedControl->PublicLocks[LockIndex].Mutex, Flags|ALF_SHARED);
   if (!error) {
      if ((error = alloc_cond(&glSharedControl->PublicLocks[LockIndex].Cond, Flags|ALF_SHARED))) {
         free_lock(&glSharedControl->PublicLocks[LockIndex].Mutex);
      }
   }
   return error;
}

void free_public_lock(UBYTE LockIndex)
{
   free_lock(&glSharedControl->PublicLocks[LockIndex].Mutex);
   free_cond(&glSharedControl->PublicLocks[LockIndex].Cond);
}

static ERROR alloc_lock(THREADLOCK *Lock, WORD Flags)
{
   LONG result;

   if (Flags) {
      pthread_mutexattr_t attrib;
      pthread_mutexattr_init(&attrib);

      if (Flags & ALF_SHARED) {
         pthread_mutexattr_setpshared(&attrib, PTHREAD_PROCESS_SHARED); // Allow the mutex to be used across foreign processes.
         #if !defined(__ANDROID__) && !defined(__APPLE__)
            // If someone crashes holding the mutex, a robust mutex results in EOWNERDEAD being returned to the next
            // guy who must then call pthread_mutex_consistent() and pthread_mutex_unlock()
            pthread_mutexattr_setrobust(&attrib, PTHREAD_MUTEX_ROBUST);
         #endif
      }
      if (Flags & ALF_RECURSIVE) pthread_mutexattr_settype(&attrib, PTHREAD_MUTEX_RECURSIVE);
      result = pthread_mutex_init(Lock, &attrib); // Create it.
      pthread_mutexattr_destroy(&attrib);
   }
   else result = pthread_mutex_init(Lock, NULL);

   if (!result) return ERR_Okay;
   else return ERR_Init;
}

ERROR alloc_private_lock(UBYTE Index, WORD Flags)
{
   return alloc_lock(&glPrivateLocks[Index], Flags);
}

ERROR alloc_private_cond(UBYTE Index, WORD Flags)
{
   return alloc_cond(&glPrivateCond[Index], Flags);
}

void free_private_lock(UBYTE Index)
{
   free_lock(&glPrivateLocks[Index]);
}

void free_private_cond(UBYTE Index)
{
   free_cond(&glPrivateCond[Index]);
}

static ERROR alloc_cond(CONDLOCK *Lock, WORD Flags)
{
   LONG result;

   if (Flags) {
      pthread_condattr_t attrib;
      if (!(result = pthread_condattr_init(&attrib))) {
         if (Flags & ALF_SHARED) pthread_condattr_setpshared(&attrib, PTHREAD_PROCESS_SHARED); // Allow the mutex to be used across foreign processes.
         result = pthread_cond_init(Lock, &attrib);
         pthread_condattr_destroy(&attrib);
      }
   }
   else result = pthread_cond_init(Lock, NULL);

   if (!result) return ERR_Okay;
   else return ERR_Init;
}

static void free_lock(THREADLOCK *Lock)
{
   if (Lock) {
      pthread_mutex_destroy(Lock);
      ClearMemory(Lock, sizeof(*Lock));
   }
}

static void free_cond(CONDLOCK *Cond)
{
   if (Cond) {
      pthread_cond_destroy(Cond);
      ClearMemory(Cond, sizeof(*Cond));
   }
}

static ERROR pthread_lock(THREADLOCK *Lock, LONG Timeout) // Timeout in milliseconds
{
   parasol::Log log(__FUNCTION__);
   LONG result;

retry:

#ifdef __ANDROID__
   if (Timeout <= 0) result = pthread_mutex_lock(Lock);
   else result = pthread_mutex_lock_timeout_np(Lock, Timeout); // This is an Android-only function
#else
   if (Timeout > 0) {
      result = pthread_mutex_trylock(Lock); // Attempt a quick-lock without resorting to the very slow clock_gettime()
      if (result IS EBUSY) {
         #ifdef __APPLE__
            LARGE end = PreciseTime() + (Timeout * 1000LL);
            do {
               struct timespec ts = { .tv_sec = 0, .tv_nsec = 1000000 }; // Equates to 1000 checks per second
               nanosleep(&ts, &ts);
            } while ((pthread_mutex_trylock(Lock) IS EBUSY) and (PreciseTime() < end));
         #else
             struct timespec timestamp;
             clock_gettime(CLOCK_REALTIME, &timestamp); // Slow!

             // This subroutine is intended to perform addition with overflow management as quickly as possible (avoid a
             // modulo operation).  Note that nsec is from 0 - 1000000000.

             LARGE tn = timestamp.tv_nsec + (1000000LL * (LARGE)Timeout);
             while (tn > 1000000000LL) {
                timestamp.tv_sec++;
                tn -= 1000000000LL;
             }
             timestamp.tv_nsec = (LONG)tn;

             result = pthread_mutex_timedlock(Lock, &timestamp);
         #endif
      }
   }
   else if (Timeout IS 0) result = pthread_mutex_trylock(Lock);
   else result = pthread_mutex_lock(Lock);
#endif

   if ((result IS ETIMEDOUT) or (result IS EBUSY)) return ERR_TimeOut;
   else if (result IS EOWNERDEAD) { // The previous mutex holder crashed while holding it.
      log.warning("Resetting the state of a crashed mutex.");
      #if !defined(__ANDROID__) && !defined(__APPLE__)
         pthread_mutex_consistent(Lock);
      #endif
      pthread_mutex_unlock(Lock);
      goto retry;
   }
   else if (result) return ERR_LockFailed;

   return ERR_Okay;
}

ERROR thread_lock(UBYTE Index, LONG Timeout)
{
   return pthread_lock(&glPrivateLocks[Index], Timeout);
}

void thread_unlock(UBYTE Index)
{
   pthread_mutex_unlock(&glPrivateLocks[Index]);
}

ERROR public_cond_wait(THREADLOCK *Lock, CONDLOCK *Cond, LONG Timeout)
{
   parasol::Log log(__FUNCTION__);

#ifdef __ANDROID__
   if (Timeout <= 0) pthread_cond_wait(Cond, Lock);
   else pthread_cond_timeout_np(Cond, Lock, Timeout); // This is an Android-only function
#else
   if (Timeout > 0) {
      struct timespec timestamp;
      clock_gettime(CLOCK_REALTIME, &timestamp); // Slow!

      // This subroutine is intended to perform addition with overflow management as quickly as possible (avoid a
      // modulo operation).  Note that nsec is from 0 - 1000000000.

      LARGE tn = timestamp.tv_nsec + (1000000LL * (LARGE)Timeout);
      while (tn > 1000000000) {
         timestamp.tv_sec++;
         tn -= 1000000000;
      }
      timestamp.tv_nsec = (LONG)tn;

      LONG result = pthread_cond_timedwait(Cond, Lock, &timestamp);
      if (result) log.warning("Error: %s", strerror(errno));
      if ((result IS ETIMEDOUT) or (result IS EAGAIN)) return ERR_TimeOut;
      else if (result) return ERR_Failed;
   }
   else pthread_cond_wait(Cond, Lock);
#endif

   return ERR_Okay;
}

ERROR cond_wait(UBYTE Lock, UBYTE Cond, LONG Timeout)
{
   return public_cond_wait(&glPrivateLocks[Lock], &glPrivateCond[Cond], Timeout);
}

// NOTE: You MUST have a already acquired a lock on the mutex associated with the condition.

void cond_wake_single(UBYTE Index)
{
   pthread_cond_signal(&glPrivateCond[Index]); // Unblocks one or more threads waiting on the condition variable.
}

void cond_wake_all(UBYTE Index)
{
   pthread_cond_broadcast(&glPrivateCond[Index]); // Unblocks ALL threads waiting on the condition variable.
}

#elif _WIN32

// Windows functions for locking are described in windows.c, and use critical sections (InitializeCriticalSection() et al)

#else

#error Platform requires support for mutexes and conditional locking.

#endif

//****************************************************************************
// Note: This function must be called in a LOCK_PUBLIC_MEMORY() zone.
//
// If the memory is already paged in, then an AccessCount is incremented and the already-paged address is returned.
// Otherwise, the memory is paged-in, an array entry is allocated  for it and the address is returned.
//
// This function is 'dumb' in that it does not manage locking of any kind.

ERROR page_memory(PublicAddress *Block, APTR *Address)
{
   parasol::Log log(__FUNCTION__);

   ThreadLock lock(TL_MEMORY_PAGES, 4000);
   if (lock.granted()) {
      // Determine the index at which we will insert the paged address (or if the address is already paged, we will be
      // increasing the access count for this memory block).

      for (LONG index=0; index < glTotalPages; index++) {
         if (Block->MemoryID IS glMemoryPages[index].MemoryID) {
            glMemoryPages[index].AccessCount++;
            *Address = glMemoryPages[index].Address;
            return ERR_Okay;
         }
      }

      // Find an empty array entry

      LONG index;
      for (index=0; (index < glTotalPages) and (glMemoryPages[index].MemoryID); index++);

      if (index >= glTotalPages) {
         log.msg("Increasing the size of the memory page table from %d to %d entries.", glTotalPages, glTotalPages + PAGE_TABLE_CHUNK);

         MemoryPage *table;
         if ((table = (MemoryPage *)calloc(glTotalPages + PAGE_TABLE_CHUNK, sizeof(MemoryPage)))) {
            CopyMemory(glMemoryPages, table, glTotalPages * sizeof(MemoryPage));
            free(glMemoryPages);
            glMemoryPages = table;
            glTotalPages += PAGE_TABLE_CHUNK;
         }
         else {
            log.warning("calloc() failed.");
            return ERR_AllocMemory;
         }
      }

      // Attach the memory to our process

      #ifdef _WIN32
         APTR addr = NULL;

         #ifdef STATIC_MEMORY_POOL
            if (!Block->Handle) addr = ResolveAddress(glSharedControl, glSharedControl->MemoryOffset + Block->Offset);
         #endif

         if (!addr) {
            ERROR error;
            if ((error = winMapMemory(Block->Handle, Block->OwnerProcess, &addr)) != ERR_Okay) {
               if (error IS ERR_MemoryDoesNotExist) {
                  // Mark the process for validation and signal sleep_task() so that we can take care of this problem
                  // at a safe point in the code.
                  log.trace("Marking process %d for validation.", Block->OwnerProcess);
                  glValidateProcessID = Block->OwnerProcess;
                  plUnlockSemaphore(&glValidationSemaphore); // Signals the semaphore.
                  return ERR_MemoryDoesNotExist;
               }
            }
         }

         if (addr) {
            glMemoryPages[index].MemoryID    = Block->MemoryID;
            glMemoryPages[index].Address     = addr;
            glMemoryPages[index].AccessCount = 1;
            //glMemoryPages[index].Size        = Block->Size;
            if (Block->TaskID IS glCurrentTaskID) glMemoryPages[index].Flags = MPF_LOCAL;
            else glMemoryPages[index].Flags = 0;
            *Address = addr;
            return ERR_Okay;
         }
         else {
            log.warning("winMapMemory() failed to map handle " PF64() " (ID: %d) of process %d.  Offset %d", (MAXINT)Block->Handle, Block->MemoryID, Block->OwnerProcess, Block->Offset);
            return ERR_LockFailed;
         }
      #else
         APTR addr = NULL;

         #ifdef STATIC_MEMORY_POOL
            if (!Block->Handle) addr = ResolveAddress(glSharedControl, glSharedControl->MemoryOffset + Block->Offset);
         #endif

         #ifdef USE_SHM
            if (!addr) addr = shmat(Block->Offset, NULL, 0);
         #else
            if (!addr) addr = mmap(0, Block->Size, PROT_READ|PROT_WRITE|PROT_EXEC, MAP_SHARED, glMemoryFD, glSharedControl->MemoryOffset + Block->Offset);
         #endif

         if ((addr != (APTR)-1) and (addr != NULL)) {
            glMemoryPages[index].MemoryID    = Block->MemoryID;
            glMemoryPages[index].Size        = Block->Size;
            glMemoryPages[index].Address     = addr;
            glMemoryPages[index].AccessCount = 1;
            if (Block->TaskID IS glCurrentTaskID) glMemoryPages[index].Flags = MPF_LOCAL;
            else glMemoryPages[index].Flags = 0;
            *Address = addr;
            return ERR_Okay;
         }
         else {
            log.warning("Memory map failed: %s.", strerror(errno));
            return ERR_LockFailed;
         }
      #endif
   }
   else return ERR_SystemLocked;
}

//****************************************************************************
// This function does not need to be called in a LOCK_PUBLIC_MEMORY() zone as pages are managed locally.

ERROR unpage_memory(APTR Address)
{
   parasol::Log log(__FUNCTION__);

   ThreadLock lock(TL_MEMORY_PAGES, 4000);
   if (lock.granted()) {
      LONG index;
      for (index=0; (index < glTotalPages) and (glMemoryPages[index].Address != Address); index++);

      if (index < glTotalPages) {
         glMemoryPages[index].AccessCount--;
         if (!glMemoryPages[index].AccessCount) {
            if ((glMemoryPages[index].Flags & MPF_LOCAL) and (!glAlwaysUnpage)) {
               // Do nothing to unmap locally allocated memory blocks (this provides speed increases if the page is
               // mapped again later).  We will be relying on FreeResourceID() to do the final detach for local pages.
            }
            else {
               #ifdef STATIC_MEMORY_POOL
                  APTR pool = ResolveAddress(glSharedControl, glSharedControl->MemoryOffset);
                  if ((Address >= pool) and (Address < (char *)pool + glSharedControl->PoolSize)) {
                     ClearMemory(glMemoryPages + index, sizeof(MemoryPage));
                     return ERR_Okay;
                  }
               #endif

               #ifdef _WIN32
                  winUnmapViewOfFile(Address);
               #elif USE_SHM
                  shmdt(Address);
               #else
                  if (munmap(Address, glMemoryPages[index].Size) IS -1) { // Used by ashmem on Android
                     log.warning("munmap() failed on %p, size " PF64() ", error: %s", Address, glMemoryPages[index].Size, strerror(errno));
                  }
               #endif

               ClearMemory(glMemoryPages + index, sizeof(MemoryPage));
            }
         }

         return ERR_Okay;
      }
      else {
         log.msg("%p [Search Failed]", Address);
         return ERR_Search;
      }
   }
   else return ERR_SystemLocked;
}

//****************************************************************************

ERROR unpage_memory_id(MEMORYID MemoryID)
{
   parasol::Log log(__FUNCTION__);

   ThreadLock lock(TL_MEMORY_PAGES, 4000);
   if (lock.granted()) {
      LONG index;
      for (index=0; (index < glTotalPages) and (glMemoryPages[index].MemoryID != MemoryID); index++);

      if (index < glTotalPages) {
         glMemoryPages[index].AccessCount--;
         if (!glMemoryPages[index].AccessCount) {
            if ((glMemoryPages[index].Flags & MPF_LOCAL) and (!glAlwaysUnpage)) {
               // Do nothing to unmap locally allocated memory blocks (this provides speed increases if the page is
               // mapped again later).  We will be relying on FreeResourceID() to do the final detach for local pages.
            }
            else {
               #ifdef STATIC_MEMORY_POOL
                  APTR pool = ResolveAddress(glSharedControl, glSharedControl->MemoryOffset);
                  if ((glMemoryPages[index].Address >= pool) and (glMemoryPages[index].Address < (char *)pool + glSharedControl->PoolSize)) {
                     ClearMemory(glMemoryPages + index, sizeof(MemoryPage));
                     return ERR_Okay;
                  }
               #endif

               #ifdef _WIN32
                  winUnmapViewOfFile(glMemoryPages[index].Address);
               #elif USE_SHM
                  shmdt(glMemoryPages[index].Address);
               #else
                  if (munmap(glMemoryPages[index].Address, glMemoryPages[index].Size) IS -1) { // Used by ashmem on Android
                     log.warning("munmap() failed on %p, size " PF64() ", error: %s", glMemoryPages[index].Address, glMemoryPages[index].Size, strerror(errno));
                  }
               #endif

               ClearMemory(glMemoryPages + index, sizeof(MemoryPage));
            }
         }

         return ERR_Okay;
      }
      else {
         log.msg("#%d [Search Failed]", MemoryID);
         return ERR_Search;
      }
   }
   else return ERR_SystemLocked;
}

/*****************************************************************************
** Prepare a thread for going to sleep on a resource.  Checks for deadlocks in advance.  Once a thread has added a
** WakeLock entry, it must keep it until either the thread or process is destroyed.
**
** Used by AccessMemory() and AccessPrivateObject()
*/

static THREADVAR WORD glWLIndex = -1;

ERROR init_sleep(LONG OtherProcessID, LONG OtherThreadID, LONG ResourceID, LONG ResourceType, WORD *Index)
{
   parasol::Log log(__FUNCTION__);

   //log.trace("Sleeping on thread %d.%d for resource #%d, Total Threads: %d", OtherProcessID, OtherThreadID, ResourceID, glSharedControl->WLIndex);

   LONG our_thread = get_thread_id();
   if (OtherThreadID IS our_thread) return log.warning(ERR_Args);

   ScopedSysLock lock(PL_WAITLOCKS, 3000);
   if (lock.granted()) {
      WaitLock *locks = (WaitLock *)ResolveAddress(glSharedControl, glSharedControl->WLOffset);

      WORD i;
      if (glWLIndex IS -1) {
         if (glSharedControl->WLIndex >= MAX_WAITLOCKS-1) { // Check for space on the array.
            return log.warning(ERR_ArrayFull);
         }

         // NOTE: An array slot is considered used if the ThreadID value has been set.  You can remove an array entry
         // without obtaining a lock on PL_WAITLOCKS only if you clear the ThreadID value LAST.

         WORD empty = -1;
         for (i=glSharedControl->WLIndex-1; i >= 0; i--) {    // Check for deadlocks.  If a deadlock will occur then we return immediately.
            if (locks[i].ThreadID IS OtherThreadID) {
               if (locks[i].WaitingForThreadID IS our_thread) {
                  log.warning("Thread %d.%d holds resource #%d and is waiting for us (%d.%d) to release #%d.", locks[i].ProcessID, locks[i].ThreadID, ResourceID, glProcessID, our_thread, locks[i].WaitingForResourceID);
                  return ERR_DeadLock;
               }
            }
            else if (!locks[i].ThreadID) empty = i;
         }

         if (empty != -1) i = empty;     // Thread entry does not exist, use an empty slot.
         else i = __sync_fetch_and_add(&glSharedControl->WLIndex, 1); // Thread entry does not exist, add an entry to the end of the array.
         glWLIndex = i;
         locks[i].ThreadID  = our_thread;
         locks[i].ProcessID = glProcessID;
      }
      else { // Our thread is already registered.
         // Check for deadlocks.  If a deadlock will occur then we return immediately.
         for (i=glSharedControl->WLIndex-1; i >= 0; i--) {
            if (locks[i].ThreadID IS OtherThreadID) {
               if (locks[i].WaitingForThreadID IS our_thread) {
                  log.warning("Thread %d.%d holds resource #%d and is waiting for us (%d.%d) to release #%d.", locks[i].ProcessID, locks[i].ThreadID, ResourceID, glProcessID, our_thread, locks[i].WaitingForResourceID);
                  return ERR_DeadLock;
               }
            }
         }

         i = glWLIndex;
      }

      locks[i].WaitingForResourceID   = ResourceID;
      locks[i].WaitingForResourceType = ResourceType;
      locks[i].WaitingForProcessID    = OtherProcessID;
      locks[i].WaitingForThreadID     = OtherThreadID;
      #if defined(_WIN32) && !defined(USE_GLOBAL_EVENTS)
         locks[i].Lock = get_threadlock();
      #endif

      *Index = i;
      return ERR_Okay;
   }
   else return log.warning(ERR_SystemLocked);
}

//****************************************************************************
// Used by ReleaseMemory(), ReleaseMemoryID(), ReleaseSemaphore()

void wake_sleepers(LONG ResourceID, LONG ResourceType)
{
   parasol::Log log(__FUNCTION__);

   log.trace("Resource: %d, Type: %d, Total: %d", ResourceID, ResourceType, glSharedControl->WLIndex);

   if (!SysLock(PL_WAITLOCKS, 2000)) {
      WaitLock *locks = (WaitLock *)ResolveAddress(glSharedControl, glSharedControl->WLOffset);
      LONG i;
      #ifdef USE_GLOBAL_EVENTS
         LONG count = 0;
      #endif

      for (i=0; i < glSharedControl->WLIndex; i++) {
         if ((locks[i].WaitingForResourceID IS ResourceID) and (locks[i].WaitingForResourceType IS ResourceType)) {
            locks[i].WaitingForResourceID   = 0;
            locks[i].WaitingForResourceType = 0;
            locks[i].WaitingForProcessID    = 0;
            locks[i].WaitingForThreadID     = 0;
            #ifdef _WIN32
               #ifndef USE_GLOBAL_EVENTS
                  if (ResourceType != RT_OBJECT) wake_waitlock(locks[i].Lock, locks[i].ProcessID, 1);
               #endif
            #else
               // On Linux this version doesn't do any waking (the caller is expected to manage that)
            #endif
         }
         #ifdef USE_GLOBAL_EVENTS
            if (locks[i].WaitingForResourceType IS ResourceType) count++;
         #endif
      }

      #ifdef USE_GLOBAL_EVENTS // Windows only.  Note that the RT_OBJECTS type is ignored because it is private.
         if (count > 0) {
            if (ResourceType IS RT_MEMORY) wake_waitlock(glPublicLocks[CN_PUBLICMEM].Lock, 0, count);
            else if (ResourceType IS RT_SEMAPHORE) wake_waitlock(glPublicLocks[CN_SEMAPHORES].Lock, 0, count);
         }
      #endif

      SysUnlock(PL_WAITLOCKS);
   }
   else log.warning(ERR_SystemLocked);
}

//****************************************************************************
// Remove all the wait-locks for the current process (affects all threads).  Lingering wait-locks are indicative of
// serious problems, as all should have been released on shutdown.

void remove_process_waitlocks(void)
{
   parasol::Log log("Shutdown");

   log.trace("Removing process waitlocks...");

   if (!glSharedControl) return;

   {
      ScopedSysLock lock(PL_WAITLOCKS, 5000);
      if (lock.granted()) {
         WaitLock *locks = (WaitLock *)ResolveAddress(glSharedControl, glSharedControl->WLOffset);
         #ifdef USE_GLOBAL_EVENTS
            LONG count = 0;
         #endif

         for (LONG i=glSharedControl->WLIndex-1; i >= 0; i--) {
            if (locks[i].ProcessID IS glProcessID) {
               ClearMemory(locks+i, sizeof(locks[0])); // Remove the entry.
               #ifdef USE_GLOBAL_EVENTS
                  count++;
               #endif
            }
            else if (locks[i].WaitingForProcessID IS glProcessID) { // Foreign thread is waiting on us, wake it up.
               #ifdef _WIN32
                  log.warning("Waking foreign thread %d.%d, which is sleeping on our process", locks[i].ProcessID, locks[i].ThreadID);
                  locks[i].WaitingForResourceID   = 0;
                  locks[i].WaitingForResourceType = 0;
                  locks[i].WaitingForProcessID    = 0;
                  locks[i].WaitingForThreadID     = 0;
                  #ifndef USE_GLOBAL_EVENTS
                     wake_waitlock(locks[i].Lock, locks[i].ProcessID, 1);
                  #else
                     count++;
                  #endif
               #endif
            }
            else continue;
         }
      }
   }

   #ifdef USE_GLOBAL_EVENTS
      if (count > 0) {
         wake_waitlock(glPublicLocks[CN_PUBLICMEM].Lock, 0, count);
         wake_waitlock(glPublicLocks[CN_SEMAPHORES].Lock, 0, count);
      }
   #endif

   #ifdef __unix__
      // Lazy wake-up, just wake everybody and they will go back to sleep if their desired lock isn't available.

      {
         ScopedSysLock lock(PL_PUBLICMEM, 5000);
         if (lock.granted()) {
            pthread_cond_broadcast(&glSharedControl->PublicLocks[PL_PUBLICMEM].Cond);
         }
      }

      {
         ScopedSysLock lock(PL_SEMAPHORES, 5000);
         if (lock.granted()) {
            pthread_cond_broadcast(&glSharedControl->PublicLocks[PL_SEMAPHORES].Cond);
         }
      }
   #endif
}

//****************************************************************************
// Clear the wait-lock of the active thread.  This does not remove our thread from the wait-lock array.
// Returns ERR_DoesNotExist if the resource was removed while waiting.

ERROR clear_waitlock(WORD Index)
{
   parasol::Log log(__FUNCTION__);
   ERROR error = ERR_Okay;

   // A sys-lock is not required so long as we only operate on our thread entry.

   WaitLock *locks = (WaitLock *)ResolveAddress(glSharedControl, glSharedControl->WLOffset);
   if (Index IS -1) {
      WORD i;
      LONG our_thread = get_thread_id();
      for (i=glSharedControl->WLIndex-1; i >= 0; i--) {
         if (locks[i].ThreadID IS our_thread) {
            Index = i;
         }
      }
   }

   if (locks[Index].Flags & WLF_REMOVED) {
      log.warning("TID %d: The private resource no longer exists.", get_thread_id());
      error = ERR_DoesNotExist;
   }

   locks[Index].Flags = 0;
   locks[Index].WaitingForResourceID   = 0;
   locks[Index].WaitingForResourceType = 0;
   locks[Index].WaitingForProcessID    = 0;
   locks[Index].WaitingForThreadID     = 0; // NB: Important that you clear this last if you are to avoid threading conflicts.
   return error;
}

//****************************************************************************
// Windows thread-lock support.  Each thread gets its own semaphore.  Note that this is intended for handling public
// resources only.  Internally, use critical sections for synchronisation between threads.

#ifdef _WIN32
static WORD glThreadLockIndex = 1;           // Shared between all threads.
static BYTE glTLInit = FALSE;
static WINHANDLE glThreadLocks[MAX_THREADS]; // Shared between all threads, used for resource tracking allocated wake locks.
static THREADVAR WINHANDLE tlThreadLock = 0; // Local to the thread.

// Returns the thread-lock semaphore for the active thread.

WINHANDLE get_threadlock(void)
{
   parasol::Log log(__FUNCTION__);

   if (tlThreadLock) return tlThreadLock; // Thread-local, no problem...

   if (!glTLInit) {
      glTLInit = TRUE;
      ClearMemory(glThreadLocks, sizeof(glThreadLocks));
   }

   LONG index = __sync_fetch_and_add(&glThreadLockIndex, 1);
   LONG end = index - 1;
   while (index != end) {
      if (index >= ARRAYSIZE(glThreadLocks)) index = glThreadLockIndex = 1; // Has the array reached exhaustion?  If so, we need to wrap it.
      if (!glThreadLocks[index]) {
         WINHANDLE lock;
         if (!alloc_public_waitlock(&lock, NULL)) {
            glThreadLocks[index] = lock; // For resource tracking.
            tlThreadLock = lock;
            log.trace("Allocated thread-lock #%d for thread #%d", index, get_thread_id());
            return lock;
         }
      }
      index = __sync_fetch_and_add(&glThreadLockIndex, 1);
   }

   log.warning("Failed to allocate a new wake-lock.  Index: %d/%d", glThreadLockIndex, MAX_THREADS);
   exit(0); // This is a permanent failure.
   return 0;
}

void free_threadlocks(void)
{
   for (LONG i=0; i < glThreadLockIndex; i++) {
      free_public_waitlock(glThreadLocks[i]);
      glThreadLocks[i] = 0;
   }
}

void free_threadlock(void)
{
   if (tlThreadLock) {
      for (LONG i=glThreadLockIndex-1; i >= 0; i--) {
         if (glThreadLocks[i] IS tlThreadLock) {
            glThreadLocks[i] = 0;
         }
      }

      winCloseHandle(tlThreadLock);
   }
}
#endif

/*****************************************************************************

-FUNCTION-
AccessMemory: Grants access to public memory blocks.
Category: Memory

The AccessMemory() function is used to page public memory blocks into the caller's memory space.  If the target block
is available, a valid address pointer will be returned that can be used for direct read/write operations.  For
convenience, this function may also be used to resolve the memory addresses of private memory block IDs.

Public memory blocks should never be locked for extended periods of time.  Ensure that all locks are matched with a
call to ~ReleaseMemory() within the same code block.

The resulting address is specific to the caller's process and is not transferable to other running processes.

-INPUT-
mem Memory:       The ID of the memory block that you want to access.
int(MEM) Flags:   Set to MEM_READ, MEM_WRITE or MEM_READ_WRITE according to your requirements.
int MilliSeconds: The millisecond interval that you are prepared to wait before a timeout occurs.  Do not set below 40ms for consistent operation.
&ptr Result:      Must point to an APTR variable that will store the resolved address.

-ERRORS-
Okay
Args
NullArgs
LockFailed
DeadLock: A process has locked the memory block and is currently sleeping on a response from the caller.
TimeOut
SystemCorrupt: The task has lost its process ID.
MemoryDoesNotExist: The MemoryID that you supplied does not refer to an existing memory block.
MarkedForDeletion: The memory block cannot be accessed because it has been marked for deletion.
SystemLocked
-END-

*****************************************************************************/

ERROR AccessMemory(MEMORYID MemoryID, LONG Flags, LONG MilliSeconds, APTR *Result)
{
   parasol::Log log(__FUNCTION__);
   LONG i, entry;

   if ((!MemoryID) or (!Result)) return log.warning(ERR_NullArgs);

   if (!glProcessID) return log.warning(ERR_SystemCorrupt);

   if (MilliSeconds <= 0) {
      log.warning("MemoryID: %d, Flags: $%x, TimeOut: %d - Invalid timeout", MemoryID, Flags, MilliSeconds);
      return ERR_Args;
   }

   // NB: Printing AccessMemory() calls is usually a waste of time unless the process is going to sleep.
   //log.trace("MemoryID: %d, Flags: $%x, TimeOut: %d", MemoryID, Flags, MilliSeconds);

   *Result = NULL;
   if (MemoryID < 0) {
      LARGE start_time = PreciseTime() / 1000LL;
      LARGE endtime = start_time + MilliSeconds;

      if (tlPreventSleep) {
         log.warning("AccessMemory() Cannot proceed as a MEM_TMP_LOCK memory block is locked.  This lock must be released before calling AccessMemory().");
         log.warning("Details - MemoryID: %d, Flags: $%x, TimeOut: %d. NoSleepTracker: %d", MemoryID, Flags, MilliSeconds, tlPreventSleep);
         //BREAKPOINT
         return ERR_LockFailed;
      }

      // Check that the we're not holding a global lock prior to LOCK_PUBLIC_MEMORY().  Sleeping with a lock on the
      // control block will cause a system lock-up, so we must avoid it.

      BYTE nosleeping = FALSE;
      if (tlPublicLockCount > 0) {
         nosleeping = TRUE;
         log.warning("Calling this function while holding %d global locks is not allowed.", tlPublicLockCount);
      }

      ScopedSysLock memlock(PL_PUBLICMEM, MilliSeconds);
      if (memlock.granted()) {
         if (find_public_mem_id(glSharedControl, MemoryID, &entry) != ERR_Okay) {
            return ERR_MemoryDoesNotExist;
         }

         PublicAddress *addr = glSharedBlocks + entry;

         // If this function has been called from AccessObject() then the MEM_OBJECT flag will be set, which requires
         // us to fail if the memory block does not form an object header.

         if ((Flags & MEM_OBJECT) and (!(addr->Flags & MEM_OBJECT))) {
            return log.warning(ERR_ObjectCorrupt);
         }

         // If NO_BLOCKING is set against the memory block, everyone has free access to the memory block.

         if (addr->Flags & MEM_NO_BLOCKING) { // Non-blocking access must be recorded for all memory blocks
            if (!glTaskEntry) return log.warning(ERR_NotInitialised);

            for (i=0; (glTaskEntry->NoBlockLocks[i].MemoryID) and (glTaskEntry->NoBlockLocks[i].MemoryID != MemoryID); i++);
            if (i >= MAX_NB_LOCKS) return log.warning(ERR_ArrayFull);

            APTR ptr;
            if (!page_memory(glSharedBlocks + entry, &ptr)) {
               glTaskEntry->NoBlockLocks[i].MemoryID = MemoryID;
               glTaskEntry->NoBlockLocks[i].AccessCount++;

               __sync_fetch_and_add(&addr->AccessCount, 1); // Prevents deallocation during usage.
               addr->AccessTime = PreciseTime() / 1000LL;
               *Result = ptr;
               return ERR_Okay;
            }
            else return ERR_Memory;
         }

         // Check the lock to see if somebody else is using this memory block.  If so, we will need to wait for the
         // memory block to become available.

         LONG attempt = 0;
         LONG our_thread = get_thread_id();
         while (((addr->ThreadLockID) and (addr->ThreadLockID != our_thread))) { // Keep looping until address is not locked
            attempt++;

            if (nosleeping) {
               log.warning("Cannot sleep due to global locks being held prior to this call.");
               return ERR_LockFailed;
            }

            if (addr->Flags & MEM_DELETE) { // Do not sleep on blocks that are about to be terminated
               return ERR_MarkedForDeletion;
            }

            // Check if the target thread happens to be waiting for a lock on one of our resources.

            #ifdef _WIN32
               WORD wl;
               if (init_sleep(addr->ProcessLockID, addr->ThreadLockID, MemoryID, RT_MEMORY, &wl) != ERR_Okay) {
                  return ERR_DeadLock;
               }

               //log.warning("Memory block %d is in use by task #%d:%d - will need to wait.", MemoryID, addr->ProcessLockID, addr->ThreadLockID);

               LONG sleep_timeout = endtime - (PreciseTime() / 1000LL);
               if (sleep_timeout <= 1) { // NB: Windows doesn't sleep on 1ms, so we timeout if the value is that small.
                  log.warning("Time-out of %dms on block #%d locked by process %d:%d.  Reattempted lock %d times.", MilliSeconds, MemoryID, addr->ProcessLockID, addr->ThreadLockID, attempt);
                  clear_waitlock(wl);
                  return ERR_TimeOut;
               }

               addr->ExternalLock = TRUE;  // Set the external lock so that the other task knows that we are waiting.

               memlock.release();

               //LARGE current_time = (PreciseTime()/1000LL);
               #ifdef USE_GLOBAL_EVENTS
                  sleep_waitlock(glPublicLocks[CN_PUBLICMEM].Lock, sleep_timeout);  // Go to sleep and wait for a wakeup or time-out.
               #else
                  sleep_waitlock(get_threadlock(), sleep_timeout);  // Go to sleep and wait for a wakeup or time-out.
               #endif
               //log.msg("I was asleep for %dms", (LONG)((PreciseTime()/1000LL) - current_time));

               // We have been woken or a time-out has occurred.  Restart the process.

               clear_waitlock(wl);

               LONG relock_timeout = endtime - (PreciseTime() / 1000LL);
               if (relock_timeout < 1) relock_timeout = 1;
               if (memlock.acquire(relock_timeout) != ERR_Okay) {
                  return log.warning(ERR_SystemLocked);
               }

            #else
               ERROR error;
               WORD wl;

               if (init_sleep(addr->ProcessLockID, addr->ThreadLockID, MemoryID, RT_MEMORY, &wl)) {
                  return ERR_DeadLock;
               }

               //log.function("Memory block %d is in use by task %d:%d - will need to wait.", MemoryID, addr->ProcessLockID, addr->ThreadLockID);

               // This simple locking method goes to sleep on the public memory conditional.
               LONG timeout = endtime - (PreciseTime()/1000LL);
               if (timeout > 0) {
                  addr->ExternalLock = TRUE; // The other process has to know that someone is waiting.
                  error = public_cond_wait(&glSharedControl->PublicLocks[PL_PUBLICMEM].Mutex, &glSharedControl->PublicLocks[PL_PUBLICMEM].Cond, timeout);
               }
               else error = ERR_TimeOut;

               clear_waitlock(wl);

               if (error) return log.warning(error);
            #endif

            if (find_public_mem_id(glSharedControl, MemoryID, &entry) != ERR_Okay) {
               return ERR_MemoryDoesNotExist;
            }

            addr = glSharedBlocks + entry;
         } // while()

         // The memory block is available.  Page-in the memory and mark it as locked.

         APTR ptr;
         if (!page_memory(glSharedBlocks + entry, &ptr)) {
            addr->ProcessLockID = glProcessID;
            addr->ThreadLockID  = get_thread_id();
            __sync_fetch_and_add(&addr->AccessCount, 1);
            addr->AccessTime = (PreciseTime() / 1000LL);
            if (addr->AccessCount IS 1) {
               // Record the object and the action responsible for this first lock (subsequent locks are not recorded for debugging)
               addr->ContextID = tlContext->Object->UID;
               addr->ActionID  = tlContext->Action;
               if (addr->Flags & MEM_TMP_LOCK) tlPreventSleep++;
            }

            #ifdef DBG_LOCKS
               //log.function("#%d, %p, Index %d, Count %d, by object #%d", MemoryID, ptr, entry, addr->AccessCount, addr->ContextID);
            #endif

            *Result = ptr;
            return ERR_Okay;
         }
         else return ERR_Memory;
      }
      else return log.warning(ERR_SystemLocked);
   }
   else {
      ThreadLock lock(TL_PRIVATE_MEM, 4000);
      if (lock.granted()) {
         auto mem = glPrivateMemory.find(MemoryID);
         if ((mem != glPrivateMemory.end()) and (mem->second.Address)) {
            LONG thread_id = get_thread_id();
            // This loop looks odd, but will prevent sleeping if we already have a lock on the memory.
            // cond_wait() will be met with a global wake-up, not necessarily on the desired block, hence the need for while().

            LARGE end_time = (PreciseTime() / 1000LL) + MilliSeconds;
            ERROR error = ERR_TimeOut;
            while ((mem->second.AccessCount > 0) and (mem->second.ThreadLockID != thread_id)) {
               LONG timeout = end_time - (PreciseTime() / 1000LL);
               if (timeout <= 0) {
                  return log.warning(ERR_TimeOut);
               }
               else {
                  //log.msg("Sleep on memory #%d, Access %d, Threads %d/%d", MemoryID, mem->second.AccessCount, (LONG)mem->second.ThreadLockID, thread_id);
                  if ((error = cond_wait(TL_PRIVATE_MEM, CN_PRIVATE_MEM, timeout))) {
                     return log.warning(error);
                  }
               }
            }

            mem->second.ThreadLockID = thread_id;
            __sync_fetch_and_add(&mem->second.AccessCount, 1);
            tlPrivateLockCount++;

            *Result = mem->second.Address;
            return ERR_Okay;
         }
         else log.traceWarning("Cannot find private memory ID #%d", MemoryID); // This is not uncommon, so trace only
      }
      else return log.warning(ERR_SystemLocked);
   }

   return ERR_MemoryDoesNotExist;
}

/*****************************************************************************

-FUNCTION-
AccessObject: Grants exclusive access to public objects.
Category: Objects

This function resolves an object ID to its address and acquires a lock on the object so that other processes and
threads cannot use it simultaneously.

If the object is in shared memory, it will be paged into the calling process.  If the object is locked, the
function will wait until the object becomes available.   This must occur within the amount of time specified in the
Milliseconds parameter.  If the time expires, the function will return with an ERR_TimeOut error code.  If successful,
ERR_Okay is returned and a reference to the object's address is stored in the Result variable.

It is crucial that calls to AccessObject() are followed with a call to ~ReleaseObject() once the lock is no
longer required.  Calls to AccessObject() will also nest, so they must be paired with ~ReleaseObject()
correctly.

If AccessObject() fails, the Result variable will be automatically set to a NULL pointer on return.

Hint: If the name of the target object is known but not the ID, use ~FindObject() to resolve it.

-INPUT-
oid Object: The unique ID of the target object.
int MilliSeconds: The limit in milliseconds before a timeout occurs.  The maximum limit is 60000, and 100 is recommended.
&obj Result: A pointer storage variable that will store the resulting object address.

-ERRORS-
Okay
NullArgs
MarkedForDeletion
MissingClass
NoMatchingObject
TimeOut
-END-

*****************************************************************************/

ERROR AccessObject(OBJECTID ObjectID, LONG MilliSeconds, OBJECTPTR *Result)
{
   parasol::Log log(__FUNCTION__);
   OBJECTPTR obj;
   ERROR error;

   if ((!Result) or (!ObjectID)) return log.warning(ERR_NullArgs);

   *Result = NULL;

   if (MilliSeconds <= 0) log.warning("Object: %d, MilliSeconds: %d - This is bad practice.", ObjectID, MilliSeconds);

   if (ObjectID > 0) {
      auto mem = glPrivateMemory.find(ObjectID);
      if ((mem != glPrivateMemory.end()) and (mem->second.Address)) {
         if (!(error = AccessPrivateObject((OBJECTPTR)mem->second.Address, MilliSeconds))) {
            *Result = (OBJECTPTR)mem->second.Address;
            return ERR_Okay;
         }
         else return error;
      }
      else if (ObjectID IS glMetaClass.Head.UID) { // Access to the MetaClass requires this special case handler.
         if (!(error = AccessPrivateObject(&glMetaClass.Head, MilliSeconds))) {
            *Result = &glMetaClass.Head;
            return ERR_Okay;
         }
         else return error;
      }
      else return ERR_NoMatchingObject;
   }
   else if (!(error = AccessMemory(ObjectID, MEM_READ_WRITE|MEM_OBJECT, MilliSeconds, (void **)&obj))) {
      if (obj->Flags & NF_FREE) { // If the object is currently being freed, access cannot be permitted even if the object is operating in the same task space.
         ReleaseMemory(obj);
         return ERR_MarkedForDeletion;
      }

      // This check prevents anyone from gaining access while ReleaseObject() is busy

      if ((obj->Flags & NF_UNLOCK_FREE) and (!obj->Locked)) {
         ReleaseMemory(obj);
         return ERR_MarkedForDeletion;
      }

      if (obj->Locked) { // Return if we already have an exclusive lock on this object
         *Result = obj;
         #ifdef DBG_OBJECTLOCKS
            log.function("Object: %d, Address: %p [Already Locked]", ObjectID, obj);
         #endif
         return ERR_Okay;
      }

      // Resolve the Stats address by finding the Object's class and calculating the offset of the Stats structure from
      // the object size.  If the object is private, resolving these addresses is not necessary.

      if (obj->UID < 0) {
         if (obj->SubID) obj->Class = FindClass(obj->SubID);
         else obj->Class = FindClass(obj->ClassID);

         if (!obj->Class) {
            log.msg("Cannot grab object %d as the %s class is not loaded.", ObjectID, ResolveClassID(obj->ClassID));
            ReleaseMemory(obj);
            return ERR_MissingClass;
         }

         obj->Stats = (Stats *)ResolveAddress(obj, ((rkMetaClass *)obj->Class)->Size);
      }

      // Tell the object that an exclusive call is being made

      if (obj->Flags & NF_PUBLIC) {
         if (obj->Flags & NF_NEW_OBJECT) {
            // If the object is currently in the process of being created for the first time (NF_NEW_OBJECT is set),
            // do not call the AccessObject support routine if NewObject support has been written by the developer
            // (the NewObject support routine is expected to do the equivalent of AccessObject).

            if (!(((rkMetaClass *)obj->Class)->ActionTable[AC_NewObject].PerformAction)) error = Action(AC_AccessObject, obj, NULL);
         }
         else error = Action(AC_AccessObject, obj, NULL);
      }
      else error = ERR_Okay;

      if ((error IS ERR_Okay) or (error IS ERR_NoAction)) {
         obj->Locked = 1; // Set the lock and return the object address
         *Result = obj;

         #ifdef DBG_OBJECTLOCKS
            log.function("Object: %d, Address: %p [New Lock]", ObjectID, obj);
         #endif

         return ERR_Okay;
      }
      else {
         ReleaseMemory(obj);
         return error;
      }
   }
   else if (error IS ERR_TimeOut) return ERR_TimeOut;
   else if (error IS ERR_MemoryDoesNotExist) return ERR_NoMatchingObject;
   else return error;
}

/*****************************************************************************

-FUNCTION-
AccessPrivateObject: Lock an object to prevent contention between threads.
Category: Objects

Use AccessPrivateObject() to gain exclusivity to an object at thread-level.  This function provides identical
behaviour to that of ~AccessObject(), but with a slight speed advantage as the object ID does not need to be
resolved to an address.  Calls to AccessPrivateObject() will nest, and must be matched with a call to
~ReleasePrivateObject() to unlock the object.

This function only needs to be called on private objects that are used between more than one thread.  Calling it on
public objects (identified by their negative object ID) is an error.

-INPUT-
obj Object: The address of the object to lock.
int MilliSeconds: The total number of milliseconds to wait before giving up.  If -1, the function will wait indefinitely.

-ERRORS-
Okay:
NullArgs:
MarkedForDeletion:
SystemLocked:
TimeOut:
-END-

*****************************************************************************/

ERROR AccessPrivateObject(OBJECTPTR Object, LONG Timeout)
{
   parasol::Log log(__FUNCTION__);

   if (!Object) {
      DEBUG_BREAK
      return log.warning(ERR_NullArgs);
   }

   #ifdef DEBUG
      if (Object->UID < 0) log.error("Thread-based locking of public objects is bad form.  Fix the code.");
   #endif

   LONG our_thread = get_thread_id();

   do {
      // Using an atomic increment we can achieve a 'quick lock' of the object without having to resort to locks.
      // This is quite safe so long as the developer is being careful with use of the object between threads (i.e. not
      // destroying the object when other threads could potentially be using it).

      if (INC_QUEUE(Object) IS 1) {
         /*if (Object->Flags & NF_FREE) { // Disallow access to objects being freed.
            SUB_QUEUE(Object);
            return ERR_MarkedForDeletion;
         }*/
         Object->Locked = 1;
         Object->ThreadID = our_thread;
         return ERR_Okay;
      }

      if (our_thread IS Object->ThreadID) { // Support nested locks.
         /*if (Object->Flags & NF_FREE) { // Disallow access to objects being freed.
            SUB_QUEUE(Object);
            return ERR_MarkedForDeletion;
         }*/
         return ERR_Okay;
      }

      // Problem: If a ReleaseObject() were to occur inside this loop area, it receives a queue value of 1 instead of
      //    zero.  As a result it would not send a signal, because it mistakenly thinks it still has a lock.
      // Solution: When restoring the queue, we check for zero.  If true, we try to re-lock because we know that the
      //    object is free.  By not sleeping, we don't have to be concerned about the missing signal.
   } while (SUB_QUEUE(Object) IS 0); // Make a correction because we didn't obtain the lock.  Repeat loop if the object lock is at zero (available).

   if (Object->Flags & (NF_FREE|NF_UNLOCK_FREE)) return ERR_MarkedForDeletion; // If the object is currently being removed by another thread, sleeping on it is pointless.

   // Problem: What if ReleaseObject() in another thread were to release the object prior to our TL_PRIVATE_OBJECTS lock?  This means that we would never receive the wake signal.
   // Solution: Prior to cond_wait(), increment the object queue to attempt a lock.  This is *slightly* less efficient than doing it after the cond_wait(), but
   //           it will prevent us from sleeping on a signal that we would never receive.

   LARGE end_time, current_time;
   if (Timeout < 0) end_time = 0x0fffffffffffffffLL; // Do not alter this value.
   else end_time = (PreciseTime() / 1000LL) + Timeout;

   INC_SLEEP(Object); // Increment the sleep queue first so that ReleasePrivateObject() will know that another thread is expecting a wake-up.

   ThreadLock lock(TL_PRIVATE_OBJECTS, Timeout);
   if (lock.granted()) {
      //log.function("TID: %d, Sleeping on #%d, Timeout: %d, Queue: %d, Locked By: %d", our_thread, Object->UID, Timeout, Object->Queue, Object->ThreadID);

      WaitLock *locks = (WaitLock *)ResolveAddress(glSharedControl, glSharedControl->WLOffset);
      ERROR error = ERR_TimeOut;
      WORD wl;
      if (!init_sleep(glProcessID, Object->ThreadID, Object->UID, RT_OBJECT, &wl)) { // Indicate that our thread is sleeping.
         while ((current_time = (PreciseTime() / 1000LL)) < end_time) {
            LONG tmout = (LONG)(end_time - current_time);
            if (tmout < 0) tmout = 0;

            if (locks[wl].Flags & WLF_REMOVED) {
               locks[wl].WaitingForResourceID   = 0;
               locks[wl].WaitingForResourceType = 0;
               locks[wl].WaitingForProcessID    = 0;
               locks[wl].WaitingForThreadID     = 0;
               locks[wl].Flags = 0;
               SUB_SLEEP(Object);
               return ERR_DoesNotExist;
            }

            if (INC_QUEUE(Object) IS 1) { // Increment the lock count - also doubles as a prv_access() call if the lock value is 1.
               locks[wl].WaitingForResourceID   = 0;
               locks[wl].WaitingForResourceType = 0;
               locks[wl].WaitingForProcessID    = 0;
               locks[wl].WaitingForThreadID     = 0;
               locks[wl].Flags = 0;
               Object->Locked = 0;
               Object->ThreadID = our_thread;
               SUB_SLEEP(Object);
               return ERR_Okay;
            }
            else SUB_QUEUE(Object);

            cond_wait(TL_PRIVATE_OBJECTS, CN_OBJECTS, tmout);
         } // end while()

         // Failure: Either a timeout occurred or the object no longer exists.

         if (clear_waitlock(wl) IS ERR_DoesNotExist) error = ERR_DoesNotExist;
         else {
            log.traceWarning("TID: %d, #%d, Timeout occurred.", our_thread, Object->UID);
            error = ERR_TimeOut;
         }
      }
      else error = log.error(ERR_Failed);

      SUB_SLEEP(Object);
      return error;
   }
   else {
      SUB_SLEEP(Object);
      return ERR_SystemLocked;
   }
}

/*****************************************************************************

-FUNCTION-
AllocMutex: Allocate a mutex suitable for managing synchronisation between threads.

This function allocates a mutex that is suitable for keeping threads synchronised (inter-process synchronisation is not
supported).  Mutexes are locked and unlocked using the ~LockMutex() and ~UnlockMutex() functions.

The underlying implementation is dependent on the host platform.  In Microsoft Windows, critical sections will be
used and may nest (ALF_RECURSIVE always applies).  Unix systems employ pthread mutexes and will only nest if the
ALF_RECURSIVE option is specified.

-INPUT-
int(ALF) Flags: Optional flags.
&ptr Result: A reference to the new mutex will be returned in this variable if the call succeeds.

-ERRORS-
Okay
NullArgs
AllocMemory

*****************************************************************************/

#ifdef __unix__
ERROR AllocMutex(LONG Flags, APTR *Result)
{
   parasol::Log log(__FUNCTION__);

   if (!Result) return ERR_NullArgs;

   THREADLOCK *m;
   if ((m = (THREADLOCK *)malloc(sizeof(THREADLOCK)))) {
      ERROR error = alloc_lock(m, Flags);
      if (error) {
         log.traceWarning("alloc_lock() failed: %s", GetErrorMsg(error));
         free(m);
      }
      else *Result = m;
      return error;
   }
   else return ERR_AllocMemory;
}
#elif _WIN32 // Please refer to windows.c
#else
#error No support for AllocMutex()
#endif

/*****************************************************************************

-FUNCTION-
AllocSharedMutex: Allocate a mutex suitable for managing synchronisation between processes.

This function allocates a mutex that is suitable for keeping both processes and threads synchronised.  Shared mutexes
are named, which allows other processes to find them.  It is recommended that names consist of random characters so
as to limit the potential for cross-contamination with other programs.

Mutexes are locked and unlocked using the ~LockSharedMutex() and ~UnlockSharedMutex() functions.  Shared mutexes carry
a speed penalty in comparison to private mutexes, and therefore should only be used as circumstances dictate.

-INPUT-
cstr Name:  A unique identifier for the mutex, expressed as a string.
&ptr Mutex: A reference to the new mutex will be returned in this variable.

-ERRORS-
Okay
NullArgs
AllocMemory

*****************************************************************************/

#ifdef __unix__
ERROR AllocSharedMutex(CSTRING Name, APTR *Result)
{
   LONG sem_id;
   ERROR error = AllocSemaphore(Name, 1, 0, &sem_id);
   if (!error) {
      *Result = (APTR)(MAXINT)sem_id;
      return ERR_Okay;
   }
   return error;
}
#elif _WIN32 // Please refer to windows.c
#else
#error No support for AllocSharedMutex()
#endif

/*****************************************************************************

-FUNCTION-
FreeMutex: Deallocate a private mutex.

This function will deallocate a mutex.  It is vital that no thread is sleeping on the mutex at the time this function
is called.  The outcome when calling this function on a mutex still in use is undefined.

-INPUT-
ptr Mutex: Pointer to a mutex.

*****************************************************************************/

#ifdef __unix__
void FreeMutex(APTR Mutex)
{
   if (!Mutex) return;
   free_lock((THREADLOCK *)Mutex);
   free(Mutex);
}
#elif _WIN32
// See windows.c FreeMutex()
#else
#error No support for FreeMutex()
#endif


/*****************************************************************************

-FUNCTION-
FreeSharedMutex: Deallocate a shared mutex.

This function will deallocate a shared mutex.  It is vital that no thread is sleeping on the mutex at the time this
function is called.  The outcome when calling this function on a mutex still in use is undefined.

-INPUT-
ptr Mutex: Reference to a shared mutex.

*****************************************************************************/

#ifdef __unix__
void FreeSharedMutex(APTR Mutex)
{
   if (!Mutex) return;
   FreeSemaphore((MAXINT)Mutex);
}
#elif _WIN32 // See windows.c FreeSharedMutex()
#else
#error No support for FreeSharedMutex()
#endif

/*****************************************************************************

-FUNCTION-
LockMutex: Acquire a lock on a mutex.

This function will lock a mutex allocated by ~AllocMutex().  If the mutex is already locked by another thread, the
caller will sleep until the mutex is released or a time-out occurs.  If multiple threads are waiting on the mutex, the
order of acquisition is dependent on the rules of the host platform.  It is recommended that the client makes no
assumption as to the queue order and that the next thread to acquire the mutex will be randomly selected.

If the mutex was acquired with the `ALF_RECURSIVE` flag, then multiple calls to this function within the same thread
will nest.  It will be necessary to call UnlockMutex() for every lock that has been acquired.

Please note that in Microsoft Windows, mutexes are implemented as critical sections and the time-out is not supported
in the normal manner.  If the MilliSeconds parameter is set to zero, the mutex will be tested immediately and the
return is immediate.  For any other value, the thread will sleep until the mutex is available and the requested timeout
will not be honoured.

-INPUT-
ptr Mutex: Reference to a mutex allocated from AllocMutex()
int MilliSeconds: Timeout in milliseconds.

-ERRORS-
Okay
NullArgs
TimeOut

*****************************************************************************/

#ifdef __unix__
ERROR LockMutex(APTR Mutex, LONG MilliSeconds)
{
   if (!Mutex) return ERR_NullArgs;
   return pthread_lock((THREADLOCK *)Mutex, MilliSeconds);
}
#elif _WIN32
// Refer to windows.c LockMutex()
#else
#error No support for LockMutex()
#endif

/*****************************************************************************

-FUNCTION-
LockSharedMutex: Acquire a lock on a shared mutex.

This function will lock a shared mutex allocated by ~AllocSharedMutex().  If the mutex is already locked by another
thread or process, the caller will sleep until the mutex is released or a time-out occurs.  If multiple threads are
waiting on the mutex, the order of acquisition is dependent on the rules of the host platform.  It is recommended that
the client makes no assumption as to the queue order and that the next thread to acquire the mutex will be randomly
selected.

If the mutex was acquired with the `ALF_RECURSIVE` flag, then multiple calls to this function within the same thread
will nest.  It will be necessary to call ~UnlockSharedMutex() for every lock that has been acquired.

-INPUT-
ptr Mutex: Reference to a mutex allocated from ~AllocSharedMutex()
int MilliSeconds: Timeout in milliseconds.

-ERRORS-
Okay
NullArgs
TimeOut

*****************************************************************************/

#ifdef __unix__
ERROR LockSharedMutex(APTR Mutex, LONG MilliSeconds)
{
   return AccessSemaphore((MAXINT)Mutex, MilliSeconds, 0);
}
#elif _WIN32
// Refer to windows.c LockMutex()
#else
#error No support for LockMutex()
#endif

/*****************************************************************************

-FUNCTION-
ReleaseMemory: Releases memory blocks from access locks.
Category: Memory

Successful calls to ~AccessMemory() must be paired with a call to ReleaseMemory or ~ReleaseMemoryID() so that the
memory can be made available to other processes.  By releasing the memory, the access count will decrease, and if
applicable, a process that is in the queue for access may then be able to gain a lock.

-INPUT-
ptr Address: Pointer to the memory address that you want to release.

-RESULT-
mem: Returns the memory ID of the block that was released, or zero if an error occurred.
-END-

*****************************************************************************/

MEMORYID ReleaseMemory(APTR Address)
{
   parasol::Log log(__FUNCTION__);

   if (!Address) {
      log.warning(ERR_NullArgs);
      return 0;
   }

   MEMORYID id;

   BYTE wake = FALSE;

   // If the LOCK_PUBLIC_MEMORY() fails, we'll risk releasing the block without it, because permanently locked memory
   // blocks can put the system into a bad state.

   {
      ScopedSysLock lock(PL_PUBLICMEM, 5000);

      if (!lock.granted()) {
         log.warning("PL_PUBLICMEM lock failed.  Will risk releasing memory address %p...", Address);
         PrintDiagnosis(0, 0);
      }

      LONG entry = find_public_address(glSharedControl, Address);
      if (entry != -1) {
         PublicAddress *addr = &glSharedBlocks[entry];

         if ((addr->ThreadLockID) and (addr->ThreadLockID != get_thread_id())) {
            log.warning("Illegal attempt to release block #%d.  You are process %d:%d, block is locked by process %d:%d.", addr->MemoryID, glProcessID, get_thread_id(), addr->ProcessLockID, addr->ThreadLockID);
            return 0;
         }

         if (!unpage_memory(Address)) {
            //log.function("%p, #%d, Count: %d, NextBlock: %d", Address, addr->MemoryID, addr->AccessCount, glSharedControl->NextBlock);

            id = addr->MemoryID;

            if (addr->AccessCount < 1) { // Sanity check
               log.warning("Process %d:%d attempt to release block %p / #%d @ %d without an existing lock (access count %d), locked by %d:%d", glProcessID, get_thread_id(), Address, id, entry, addr->AccessCount, addr->ProcessLockID, addr->ThreadLockID);

               PrintDiagnosis(0, 0);
               return id;
            }

            // If the memory block was open for non-blocking access we need to drop the count in our local memory resource list.

            if ((addr->Flags & MEM_NO_BLOCKING) and (glTaskEntry)) {
               for (LONG j=0; glTaskEntry->NoBlockLocks[j].MemoryID; j++) {
                  if (glTaskEntry->NoBlockLocks[j].MemoryID IS id) {
                     glTaskEntry->NoBlockLocks[j].AccessCount--;
                     if (glTaskEntry->NoBlockLocks[j].AccessCount < 1) {
                        CopyMemory(glTaskEntry->NoBlockLocks+j+1, glTaskEntry->NoBlockLocks+j, sizeof(glTaskEntry->NoBlockLocks[0]) * (MAX_NB_LOCKS-j));
                     }
                     break;
                  }
               }
            }

            LONG access_count = __sync_sub_and_fetch(&addr->AccessCount, 1);

            if (access_count <= 0) {
               addr->ProcessLockID = 0;
               addr->ThreadLockID  = 0;
               addr->AccessTime    = 0;
               addr->ContextID     = 0;
               addr->ActionID      = 0;

               if (addr->Flags & MEM_EXCLUSIVE) addr->Flags &= ~MEM_EXCLUSIVE;
               if (addr->Flags & MEM_TMP_LOCK) tlPreventSleep--;

               // If one or more externals lock are present, we need to wake up the sleeping processes and let them fight for
               // the memory block after we release the SharedControl structure.

               if (addr->ExternalLock) {
                  addr->ExternalLock = FALSE;
                  wake = TRUE;

                  #ifdef DBG_LOCKS
                     log.msg("Will wake processes sleeping on memory block #%d.", addr->MemoryID);
                  #endif
               }

               if (addr->Flags & MEM_DELETE) {
                  log.trace("Deleting marked public memory block #%d (MEM_DELETE)", id);
                  FreeResourceID(id);
                  addr = NULL; // Accessing addr following FreeResource() would be an error.
               }
            }

            #ifdef DBG_LOCKS
               //log.function("ID #%d, Address %p", id, Address);
            #endif

            #ifdef __unix__
               // Conditional broadcast has to be done while the PL_PUBLICMEM lock is active.
               if ((wake) and (lock.granted())) {
                  wake_sleepers(id, RT_MEMORY); // Clear deadlock indicators
                  pthread_cond_broadcast(&glSharedControl->PublicLocks[PL_PUBLICMEM].Cond);
               }
            #endif

            if (lock.granted()) lock.release();

            #ifdef _WIN32
               if (wake) wake_sleepers(id, RT_MEMORY);
            #endif

            return id;
         }
         else {
            log.warning("unpage_memory() failed for address %p.", Address);
            return 0;
         }
      }
   }

   // Address was not found in the public memory list - drop through to private list

   if (((LONG *)Address)[-1] != CODE_MEMH) {
      log.warning("Address %p is not a recognised address, or the header is corrupt.", Address);
      return 0;
   }

   ThreadLock lock(TL_PRIVATE_MEM, 4000);
   if (lock.granted()) {
      auto mem = glPrivateMemory.find(((LONG *)Address)[-2]);

      if ((mem IS glPrivateMemory.end()) or (!mem->second.Address)) {
         if (tlContext->Object->Class) log.warning("Unable to find a record for memory address %p, ID %d [Context %d, Class %s].", Address, ((LONG *)Address)[-2], tlContext->Object->UID, ((rkMetaClass *)tlContext->Object->Class)->ClassName);
         else log.warning("Unable to find a record for memory address %p.", Address);
         if (glLogLevel > 1) PrintDiagnosis(glProcessID, 0);
         return 0;
      }

      id = mem->second.MemoryID;

      WORD access;
      if (mem->second.AccessCount > 0) { // Sometimes ReleaseMemory() is called on private addresses that aren't actually locked.  This is OK - we simply don't do anything in that case.
         access = __sync_sub_and_fetch(&mem->second.AccessCount, 1);
         tlPrivateLockCount--;
      }
      else access = -1;

      #ifdef DBG_LOCKS
         log.trace("MemoryID: %d, Address: %p, Locks: %d", id, Address, access);
      #endif

      if (!access) {
         #ifdef __unix__
            mem->second.ThreadLockID = 0; // This is more for peace of mind (it's the access count that matters)
         #endif

         if (mem->second.Flags & MEM_DELETE) {
            log.trace("Deleting marked private memory block #%d (MEM_DELETE)", id);
            FreeResource(mem->second.Address); // NB: The block entry will no longer be valid from this point onward
            cond_wake_all(CN_PRIVATE_MEM); // Wake up any threads sleeping on this memory block.
            return id;
         }
         else if (mem->second.Flags & MEM_EXCLUSIVE) {
            mem->second.Flags &= ~MEM_EXCLUSIVE;
         }

         cond_wake_all(CN_PRIVATE_MEM); // Wake up any threads sleeping on this memory block.
      }

      return id;
   }
   else return 0;
}

/*****************************************************************************

-FUNCTION-
ReleaseMemoryID: Releases locked memory blocks by ID.
Category: Memory

Successful calls to ~AccessMemory() must be paired with a call to ~ReleaseMemory() or ReleaseMemoryID so that the
memory can be made available to other processes.  By releasing the memory, the access count will decrease, and if
applicable, a process that is in the queue for access may then be able to gain a lock.

This function is both faster and safer than the ~ReleaseMemory() function.

-INPUT-
mem MemoryID: A reference to a memory block for release.

-ERRORS-
Okay
NullArgs
-END-

*****************************************************************************/

ERROR ReleaseMemoryID(MEMORYID MemoryID)
{
   parasol::Log log(__FUNCTION__);
   LONG entry;
   BYTE permit;

   if (!MemoryID) return log.warning(ERR_NullArgs);

   if (MemoryID < 0) {
      // If the LOCK_PUBLIC_MEMORY() fails, we'll risk releasing the block without it, because permanently locked
      // memory blocks can put the system into a bad state.

      if (!LOCK_PUBLIC_MEMORY(20000)) permit = TRUE;
      else {
         log.warning("LOCK_PUBLIC_MEMORY() failed.  Will risk releasing memory address #%d...", MemoryID);
         permit = FALSE;
         PrintDiagnosis(0, 0);
      }

      if (find_public_mem_id(glSharedControl, MemoryID, &entry) != ERR_Okay) {
         if (permit) UNLOCK_PUBLIC_MEMORY();
         return log.warning(ERR_Search);
      }

      PublicAddress *addr = &glSharedBlocks[entry];

      if ((addr->ThreadLockID) and (addr->ThreadLockID != get_thread_id())) {
         log.warning("Illegal attempt to release block #%d.  You are process %d:%d, block is locked by process %d:%d", addr->MemoryID, glProcessID, get_thread_id(), addr->ProcessLockID, addr->ThreadLockID);
         if (permit) UNLOCK_PUBLIC_MEMORY();
         return ERR_Failed;
      }

      if (!unpage_memory_id(MemoryID)) {
         if (addr->AccessCount < 1) { // Sanity check
            log.warning("Process %d:%d attempt to release block #%d, index %d without an existing lock (access count %d), locked by %d:%d", glProcessID, get_thread_id(), MemoryID, entry, addr->AccessCount, addr->ProcessLockID, addr->ThreadLockID);
            PrintDiagnosis(0, 0);
            if (permit) UNLOCK_PUBLIC_MEMORY();
            return ERR_Okay;
         }

         // If the memory block was open for non-blocking access we need to drop the count in our local memory resource list.

         if ((addr->Flags & MEM_NO_BLOCKING) and (glTaskEntry)) {
            for (LONG j=0; glTaskEntry->NoBlockLocks[j].MemoryID; j++) {
               if (glTaskEntry->NoBlockLocks[j].MemoryID IS MemoryID) {
                  glTaskEntry->NoBlockLocks[j].AccessCount--;
                  if (glTaskEntry->NoBlockLocks[j].AccessCount < 1) {
                     CopyMemory(glTaskEntry->NoBlockLocks+j+1, glTaskEntry->NoBlockLocks+j, sizeof(glTaskEntry->NoBlockLocks[0]) * (MAX_NB_LOCKS-j));
                  }
                  break;
               }
            }
         }

         __sync_fetch_and_sub(&addr->AccessCount, 1);

         BYTE wake = FALSE;

         if (addr->AccessCount <= 0) {
            addr->ProcessLockID = 0;
            addr->ThreadLockID  = 0;
            addr->AccessTime    = 0;
            addr->ContextID     = 0;
            addr->ActionID      = 0;

            if (addr->Flags & MEM_EXCLUSIVE) addr->Flags &= ~MEM_EXCLUSIVE;
            if (addr->Flags & MEM_TMP_LOCK) tlPreventSleep--;

            // If one or more externals lock are present, we need to wake up the sleeping processes and let them fight for
            // the memory block after we release the SharedControl structure.

            if (addr->ExternalLock) {
               addr->ExternalLock = FALSE;
               wake = TRUE;

               #ifdef DBG_LOCKS
                  if ((addr->MemoryID IS -10171) or (addr->MemoryID IS -10162)) log.msg("Will wake processes sleeping on memory block #%d.", addr->MemoryID);
               #endif
            }

            if (addr->Flags & MEM_DELETE) {
               log.trace("Deleting marked public memory block #%d (MEM_DELETE)", MemoryID);
               FreeResourceID(MemoryID);
               addr = NULL; // Accessing addr following FreeResource() would be an error.
            }
         }

         #ifdef DBG_LOCKS
            log.function("ID #%d", MemoryID);
         #endif

         #ifdef __unix__
            // Conditional broadcast has to be done while the PL_PUBLICMEM lock is active.
            if ((wake) and (permit)) {
               wake_sleepers(MemoryID, RT_MEMORY); // Clear deadlock indicators
               pthread_cond_broadcast(&glSharedControl->PublicLocks[PL_PUBLICMEM].Cond);
            }
         #endif

         if (permit) UNLOCK_PUBLIC_MEMORY();

         #ifdef _WIN32
            if (wake) wake_sleepers(MemoryID, RT_MEMORY);
         #endif

         return ERR_Okay;
      }
      else {
         if (permit) UNLOCK_PUBLIC_MEMORY();
         log.warning("unpage_memory() failed for address #%d", MemoryID);
         return ERR_Failed;
      }
   }
   else {
      ThreadLock lock(TL_PRIVATE_MEM, 4000);
      if (lock.granted()) {
         auto mem = glPrivateMemory.find(MemoryID);

         if ((mem IS glPrivateMemory.end()) or (!mem->second.Address)) {
            if (tlContext->Object->Class) log.warning("Unable to find a record for memory address #%d [Context %d, Class %s].", MemoryID, tlContext->Object->UID, ((rkMetaClass *)tlContext->Object->Class)->ClassName);
            else log.warning("Unable to find a record for memory #%d.", MemoryID);
            if (glLogLevel > 1) PrintDiagnosis(glProcessID, 0);
            return ERR_Search;
         }

         WORD access;
         if (mem->second.AccessCount > 0) { // Sometimes ReleaseMemory() is called on private addresses that aren't actually locked.  This is OK - we simply don't do anything in that case.
            access = __sync_sub_and_fetch(&mem->second.AccessCount, 1);
            tlPrivateLockCount--;
         }
         else access = -1;

         #ifdef DBG_LOCKS
            log.function("MemoryID: %d, Locks: %d", MemoryID, access);
         #endif

         if (!access) {
            #ifdef __unix__
               mem->second.ThreadLockID = 0; // This is more for peace of mind (it's the access count that matters)
            #endif

            if (mem->second.Flags & MEM_DELETE) {
               log.trace("Deleting marked private memory block #%d (MEM_DELETE)", MemoryID);
               FreeResource(mem->second.Address);
               cond_wake_all(CN_PRIVATE_MEM); // Wake up any threads sleeping on this memory block.
               return ERR_Okay;
            }
            else if (mem->second.Flags & MEM_EXCLUSIVE) {
               mem->second.Flags &= ~MEM_EXCLUSIVE;
            }

            cond_wake_all(CN_PRIVATE_MEM); // Wake up any threads sleeping on this memory block.
         }

         return ERR_Okay;
      }
      else return log.warning(ERR_SystemLocked);
   }
}

/*****************************************************************************

-FUNCTION-
ReleaseObject: Releases objects from exclusive use.
Category: Objects

Release a lock previously obtained from ~AccessObject().  Locks will nest, so a release is required for every lock that
has been granted.  For public objects, the object address will be invalid once it is released.

-INPUT-
obj Object: Pointer to a locked object.

-ERRORS-
Okay
NullArgs
NoStats
NotLocked
Memory

*****************************************************************************/

ERROR ReleaseObject(OBJECTPTR Object)
{
   parasol::Log log(__FUNCTION__);
   MemInfo info;

   if (!Object) return log.warning(ERR_NullArgs);

   if (Object->UID > 0) {
      if (Object->Queue > 0) {
         ReleasePrivateObject(Object);
         return ERR_Okay;
      }
      else return log.warning(ERR_NotLocked);
   }
   else if (!MemoryIDInfo(Object->UID, &info, sizeof(info))) {
      if (info.AccessCount <= 0) {
         log.warning("[Process:%d] Attempt to free a non-existent lock on object %d.", glProcessID, Object->UID);
         return ERR_NotLocked;
      }

      if (info.AccessCount > 1) {
         ReleaseMemory(Object);
         #ifdef DBG_OBJECTLOCKS
            log.function("#%d, Remaining Locks: %d", info.MemoryID, info.AccessCount);
         #endif
         return ERR_Okay;
      }
      else { // Send a ReleaseObject notification to the object

         if (Object->Flags & NF_PUBLIC) {
            if (Object->Flags & NF_UNLOCK_FREE) {
               // Objects are not called with ReleaseObject() if they are marked for deletion.  This allows
               // the developer to maintain locks during the Free() action and release them manually when he is ready.
            }
            else Action(AC_ReleaseObject, Object, NULL);

            // If a child structure is active, automatically release the block for the developer
            if (Object->ChildPrivate) {
               ReleaseMemory(Object->ChildPrivate);
               Object->ChildPrivate = NULL;
            }
         }

         // Clean up

         if (Object->Flags & NF_UNLOCK_FREE) {
            Object->Flags &= ~(NF_UNLOCK_FREE|NF_FREE);
            Object->Locked = 0;
            if (Object->UID < 0) { // If public, free the object and then release the memory block to destroy it.
               acFree(Object);
               ReleaseMemory(Object);
            }
            else { // For private objects we can release the block first to optimise the free process.
               ReleaseMemory(Object);
               acFree(Object);
            }
         }
         else {
            Object->Locked = 0;
            ReleaseMemory(Object);
         }

         #ifdef DBG_OBJECTLOCKS
            log.function("#%d [Unlocked]", info.MemoryID);
         #endif
         return ERR_Okay;
      }
   }
   else {
      log.msg("MemoryIDInfo() failed for object #%d @ %p", Object->UID, Object);
      return ERR_Memory;
   }
}

/*****************************************************************************

-FUNCTION-
ReleasePrivateObject: Release a locked private object.
Category: Objects

Release a lock previously obtained from ~AccessPrivateObject().  Locks will nest, so a release is required for every
lock that has been granted.

-INPUT-
obj Object: Pointer to the object to be released.

*****************************************************************************/

void ReleasePrivateObject(OBJECTPTR Object)
{
   parasol::Log log(__FUNCTION__);

   if (!Object) return;

   #ifdef DEBUG
      LONG our_thread = get_thread_id();
      if (Object->ThreadID != our_thread) {
         log.traceWarning("Invalid call to ReleasePrivateObject(), locked by thread #%d, we are #%d", Object->ThreadID, our_thread);
         return;
      }
   #endif

   // If the queue reaches zero, check if there are other threads sleeping on this object.  If so, use a signal to
   // wake at least one of them.

   if (SUB_QUEUE(Object) > 0) return;

   Object->Locked = 0;

   if (Object->SleepQueue > 0) {
      #ifdef DEBUG
         log.traceBranch("Thread: %d - Waking 1 of %d threads", our_thread, Object->SleepQueue);
      #endif

      if (!thread_lock(TL_PRIVATE_OBJECTS, -1)) {
         if (Object->Flags & (NF_FREE|NF_UNLOCK_FREE)) { // We have to tell other threads that the object is marked for deletion.
            // NB: A lock on PL_WAITLOCKS is not required because we're already protected by the TL_PRIVATE_OBJECTS
            // barrier (which is common between AccessPrivateObject() and ReleasePrivateObject()
            WaitLock *locks = (WaitLock *)ResolveAddress(glSharedControl, glSharedControl->WLOffset);
            for (WORD i=0; i < glSharedControl->WLIndex; i++) {
               if ((locks[i].WaitingForResourceID IS Object->UID) and (locks[i].WaitingForResourceType IS RT_OBJECT)) {
                  locks[i].Flags |= WLF_REMOVED;
               }
            }
         }

         // Destroy the object if marked for deletion.  NOTE: It is the responsibility of the programmer
         // to ensure that once an object has been marked for deletion, references to the object pointer
         // are removed so that no thread will attempt to use it during deallocation.

         if ((Object->Flags & NF_UNLOCK_FREE) and (!(Object->Flags & NF_FREE))) {
            set_object_flags(Object, Object->Flags & (~NF_UNLOCK_FREE));
            acFree(Object);

            cond_wake_all(CN_OBJECTS);
         }
         else cond_wake_single(CN_OBJECTS); // This will either be a critical section (Windows) or private conditional (Posix)

         thread_unlock(TL_PRIVATE_OBJECTS);
      }
      else exit(0);
   }
   else if ((Object->Flags & NF_UNLOCK_FREE) and (!(Object->Flags & NF_FREE))) {
      set_object_flags(Object, Object->Flags & (~NF_UNLOCK_FREE));
      acFree(Object);
   }
}

/*****************************************************************************

-FUNCTION-
SysLock: Locks internal system mutexes.
Status: private

Use the SysLock() function to lock one of the Core's internal mutexes.  This allows you to use the shared areas of
the Core without encountering race conditions caused by other tasks trying to access the same areas.  Calls
to SysLock() must be followed with a call to ~SysUnlock() to undo the lock.

Due to the powerful nature of this function, you should only ever use it in small code segments and never in areas
that involve communication with other processes.  Locking for extended periods of time (over 4 seconds) may also result
in the process being automatically terminated by the system.

Multiple calls to SysLock() will nest.

-INPUT-
int Index: The index number of the system mutex that needs to be locked.
int MilliSeconds: Timeout in milliseconds.

-ERRORS-
Okay
LockFailed
TimeOut
-END-

*****************************************************************************/

#ifdef __unix__

ERROR SysLock(LONG Index, LONG Timeout)
{
   parasol::Log log(__FUNCTION__);
   LONG result;

   if (!glSharedControl) {
      log.warning("No glSharedControl.");
      return ERR_Failed;
   }

retry:
   #ifdef __ANDROID__
      result = pthread_mutex_lock(&glSharedControl->PublicLocks[Index].Mutex);
   #else
      if (Timeout > 0) {
         // Attempt a quick-lock without resorting to the very slow clock_gettime()
         result = pthread_mutex_trylock(&glSharedControl->PublicLocks[Index].Mutex);
         if (result IS EBUSY) {
            #ifdef __APPLE__
               LARGE end = PreciseTime() + (Timeout * 1000LL);
               do {
                  struct timespec ts = { .tv_sec = 0, .tv_nsec = 1000000 }; // Equates to 1000 checks per second
                  nanosleep(&ts, &ts);
               } while ((pthread_mutex_trylock(&glSharedControl->PublicLocks[Index].Mutex) IS EBUSY) and (PreciseTime() < end));
            #else
               struct timespec timestamp;
               clock_gettime(CLOCK_REALTIME, &timestamp); // Slow!

               // This subroutine is intended to perform addition with overflow management as quickly as possible (avoid a modulo operation).
               // Note that nsec is from 0 - 1000000000.

               LARGE tn = timestamp.tv_nsec + (1000000LL * (LARGE)Timeout);
               while (tn > 1000000000) {
                  timestamp.tv_sec++;
                  tn -= 1000000000;
               }
               timestamp.tv_nsec = (LONG)tn;

               result = pthread_mutex_timedlock(&glSharedControl->PublicLocks[Index].Mutex, &timestamp);
            #endif
         }
      }
      else result = pthread_mutex_lock(&glSharedControl->PublicLocks[Index].Mutex);
   #endif

   if ((result IS ETIMEDOUT) or (result IS EBUSY)) {
      log.warning("Timeout locking mutex %d with timeout %d, locked by process %d.", Index, Timeout, glSharedControl->PublicLocks[Index].PID);
      return ERR_TimeOut;
   }
   else if (result IS EOWNERDEAD) { // The previous mutex holder crashed while holding it.
      log.warning("Resetting the state of a crashed mutex.");
      #if !defined(__ANDROID__) && !defined(__APPLE__)
         pthread_mutex_consistent(&glSharedControl->PublicLocks[Index].Mutex);
      #endif
      pthread_mutex_unlock(&glSharedControl->PublicLocks[Index].Mutex);
      goto retry;
   }
   else if (result) {
      log.warning("Failed to lock mutex %d with timeout %d, locked by process %d. Error: %s", Index, Timeout, glSharedControl->PublicLocks[Index].PID, strerror(result));
      return ERR_LockFailed;
   }

   glSharedControl->PublicLocks[Index].Count++;
   glSharedControl->PublicLocks[Index].PID = glProcessID;
   tlPublicLockCount++;
   return ERR_Okay;
}

#elif _WIN32

ERROR SysLock(LONG Index, LONG Timeout)
{
   parasol::Log log(__FUNCTION__);
   LONG result;

   result = winWaitForSingleObject(glPublicLocks[Index].Lock, Timeout);
   switch(result) {
      case 2: // Abandoned mutex - technically successful, so we drop through to the success case after printing a warning.
         log.warning("Warning - mutex #%d abandoned by crashed process.", Index);
         glPublicLocks[Index].Count = 0; // Correct the nest count due to the abandonment.
      case 0:
         glPublicLocks[Index].PID = glProcessID;
         glPublicLocks[Index].Count++;
         tlPublicLockCount++;
         return ERR_Okay;
      case 1:
         log.warning("Timeout occurred while waiting for mutex.");
         break;
      default:
         log.warning("Unknown result #%d.", result);
         break;
   }
   return ERR_LockFailed;
}

#endif

/*****************************************************************************

-FUNCTION-
SysUnlock: Releases a lock obtained from SysLock().

Use the SysUnlock() function to undo a previous call to ~SysLock() for a given mutex.

-INPUT-
int Index: The index number of the system mutex that needs to be unlocked.

-ERRORS-
Okay
-END-

*****************************************************************************/

#ifdef __unix__

ERROR SysUnlock(LONG Index)
{
   parasol::Log log(__FUNCTION__);

   if (glSharedControl) {
      tlPublicLockCount--;
      if (!(--glSharedControl->PublicLocks[Index].Count)) glSharedControl->PublicLocks[Index].PID = 0;
      pthread_mutex_unlock(&glSharedControl->PublicLocks[Index].Mutex);
      return ERR_Okay;
   }
   else {
      log.warning("Warning - no glSharedControl.");
      return ERR_SystemCorrupt;
   }
}

#elif _WIN32

ERROR SysUnlock(LONG Index)
{
   tlPublicLockCount--;
   if (!(--glPublicLocks[Index].Count)) glPublicLocks[Index].PID = 0;
   public_thread_unlock(glPublicLocks[Index].Lock);
   return ERR_Okay;
}

#endif

/*****************************************************************************

-FUNCTION-
UnlockMutex: Release a locked mutex.

This function will unlock any mutex that has been locked with the ~LockMutex() function.  If the mutex is nested, this
function must be called enough times to match the calls to ~LockMutex() before it will be released to other processes.

If the target mutex has no lock, or if the lock belongs to another thread, the resulting behaviour is undefined and
may result in an exception.

-INPUT-
ptr Mutex: Reference to a locked mutex.
-END-

*****************************************************************************/

#ifdef __unix__
void UnlockMutex(APTR Mutex)
{
   if (Mutex) pthread_mutex_unlock((THREADLOCK *)Mutex);
}
#elif _WIN32
// Refer to windows.c UnlockMutex()
#else
#error No support for UnlockMutex()
#endif

/*****************************************************************************

-FUNCTION-
UnlockSharedMutex: Release a locked mutex.

This function will unlock any mutex that has been locked with the ~LockSharedMutex() function.  As shared
mutexes are nested, this function must be called enough times to match the calls to ~LockSharedMutex() before
it will be released to other processes.

If the target mutex has no lock, or if the lock belongs to another thread, the resulting behaviour is undefined and
may result in an exception.

-INPUT-
ptr Mutex: Reference to a locked mutex.
-END-

*****************************************************************************/

#ifdef __unix__
void UnlockSharedMutex(APTR Mutex)
{
   pReleaseSemaphore((MAXINT)Mutex, 0);
}
#elif _WIN32 // Refer to windows.c UnlockSharedMutex()
#else
#error No support for UnlockSharedMutex()
#endif
