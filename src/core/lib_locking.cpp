/*********************************************************************************************************************

The source code of the Parasol Framework is made publicly available under the terms described in the LICENSE.TXT file
that is distributed with this package.  Please refer to it for further information on licensing.

-CATEGORY-
Name: Locks
-END-

CORE LOCKING MANAGEMENT
-------------------------
Most technical code regarding system locking is managed in this area.  Also check out lib_semaphores.c and lib_messages.c.

*********************************************************************************************************************/

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

using namespace pf;

#ifdef _WIN32
THREADVAR WORD tlMessageBreak = FALSE; // This variable is set by ProcessMessages() to allow breaking when Windows sends OS messages
#endif

//********************************************************************************************************************
// POSIX compatible lock allocation functions.
// Note: THREADLOCK == pthread_mutex_t; CONDLOCK == pthread_cond_t

#ifdef __unix__
static ERROR alloc_lock(THREADLOCK *, ALF);
static ERROR alloc_cond(CONDLOCK *, ALF);
static void free_cond(CONDLOCK *);
static void free_lock(THREADLOCK *);

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

ERROR alloc_public_lock(UBYTE LockIndex, ALF Flags)
{
   if ((LockIndex < 1) or (LockIndex >= PL_END)) return ERR_Args;
   if (!glSharedControl) return ERR_Failed;
   ERROR error = alloc_lock(&glSharedControl->PublicLocks[LockIndex].Mutex, Flags|ALF::SHARED);
   if (!error) {
      if ((error = alloc_cond(&glSharedControl->PublicLocks[LockIndex].Cond, Flags|ALF::SHARED))) {
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

static ERROR alloc_lock(THREADLOCK *Lock, ALF Flags)
{
   LONG result;

   if (Flags != ALF::NIL) {
      pthread_mutexattr_t attrib;
      pthread_mutexattr_init(&attrib);

      if ((Flags & ALF::SHARED) != ALF::NIL) {
         pthread_mutexattr_setpshared(&attrib, PTHREAD_PROCESS_SHARED); // Allow the mutex to be used across foreign processes.
         #if !defined(__ANDROID__) && !defined(__APPLE__)
            // If someone crashes holding the mutex, a robust mutex results in EOWNERDEAD being returned to the next
            // guy who must then call pthread_mutex_consistent() and pthread_mutex_unlock()
            pthread_mutexattr_setrobust(&attrib, PTHREAD_MUTEX_ROBUST);
         #endif
      }
      if ((Flags & ALF::RECURSIVE) != ALF::NIL) pthread_mutexattr_settype(&attrib, PTHREAD_MUTEX_RECURSIVE);
      result = pthread_mutex_init(Lock, &attrib); // Create it.
      pthread_mutexattr_destroy(&attrib);
   }
   else result = pthread_mutex_init(Lock, NULL);

   if (!result) return ERR_Okay;
   else return ERR_Init;
}

ERROR alloc_private_lock(UBYTE Index, ALF Flags)
{
   return alloc_lock(&glPrivateLocks[Index], Flags);
}

ERROR alloc_private_cond(UBYTE Index, ALF Flags)
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
         if (Flags & ALF::SHARED) pthread_condattr_setpshared(&attrib, PTHREAD_PROCESS_SHARED); // Allow the mutex to be used across foreign processes.
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
   pf::Log log(__FUNCTION__);
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
   pf::Log log(__FUNCTION__);

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

/*********************************************************************************************************************
** Prepare a thread for going to sleep on a resource.  Checks for deadlocks in advance.  Once a thread has added a
** WakeLock entry, it must keep it until either the thread or process is destroyed.
**
** Used by AccessMemory() and LockObject()
*/

static THREADVAR WORD glWLIndex = -1;

ERROR init_sleep(LONG OtherProcessID, LONG OtherThreadID, LONG ResourceID, LONG ResourceType, WORD *Index)
{
   pf::Log log(__FUNCTION__);

   //log.trace("Sleeping on thread %d.%d for resource #%d, Total Threads: %d", OtherProcessID, OtherThreadID, ResourceID, glSharedControl->WLIndex);

   LONG our_thread = get_thread_id();
   if (OtherThreadID IS our_thread) return log.warning(ERR_Args);

   ScopedSysLock lock(PL_WAITLOCKS, 3000);
   if (lock.granted()) {
      auto locks = (WaitLock *)ResolveAddress(glSharedControl, glSharedControl->WLOffset);

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

//********************************************************************************************************************
// Used by ReleaseSemaphore()

void wake_sleepers(LONG ResourceID, LONG ResourceType)
{
   pf::Log log(__FUNCTION__);

   log.trace("Resource: %d, Type: %d, Total: %d", ResourceID, ResourceType, glSharedControl->WLIndex);

   if (!SysLock(PL_WAITLOCKS, 2000)) {
      auto locks = (WaitLock *)ResolveAddress(glSharedControl, glSharedControl->WLOffset);
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
            if (ResourceType IS RT_SEMAPHORE) wake_waitlock(glPublicLocks[CN_SEMAPHORES].Lock, 0, count);
         }
      #endif

      SysUnlock(PL_WAITLOCKS);
   }
   else log.warning(ERR_SystemLocked);
}

//********************************************************************************************************************
// Remove all the wait-locks for the current process (affects all threads).  Lingering wait-locks are indicative of
// serious problems, as all should have been released on shutdown.

void remove_process_waitlocks(void)
{
   pf::Log log("Shutdown");

   log.trace("Removing process waitlocks...");

   if (!glSharedControl) return;

   {
      ScopedSysLock lock(PL_WAITLOCKS, 5000);
      if (lock.granted()) {
         auto locks = (WaitLock *)ResolveAddress(glSharedControl, glSharedControl->WLOffset);
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
         wake_waitlock(glPublicLocks[CN_SEMAPHORES].Lock, 0, count);
      }
   #endif

   #ifdef __unix__
      // Lazy wake-up, just wake everybody and they will go back to sleep if their desired lock isn't available.

      {
         ScopedSysLock lock(PL_SEMAPHORES, 5000);
         if (lock.granted()) {
            pthread_cond_broadcast(&glSharedControl->PublicLocks[PL_SEMAPHORES].Cond);
         }
      }
   #endif
}

//********************************************************************************************************************
// Clear the wait-lock of the active thread.  This does not remove our thread from the wait-lock array.
// Returns ERR_DoesNotExist if the resource was removed while waiting.

ERROR clear_waitlock(WORD Index)
{
   pf::Log log(__FUNCTION__);
   ERROR error = ERR_Okay;

   // A sys-lock is not required so long as we only operate on our thread entry.

   auto locks = (WaitLock *)ResolveAddress(glSharedControl, glSharedControl->WLOffset);
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

//********************************************************************************************************************
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
   pf::Log log(__FUNCTION__);

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

/*********************************************************************************************************************

-FUNCTION-
AccessMemory: Grants access to memory blocks by identifier.
Category: Memory

Call AccessMemory() to resolve a memory ID to its address and acquire a lock so that it is inaccessible to other
threads.

Memory blocks should never be locked for extended periods of time.  Ensure that all locks are matched with a
call to ~ReleaseMemory() within the same code block.

-INPUT-
mem Memory:       The ID of the memory block that you want to access.
int(MEM) Flags:   Set to MEM_READ, MEM_WRITE or MEM_READ_WRITE according to requirements.
int MilliSeconds: The millisecond interval to wait before a timeout occurs.  Do not set below 40ms for consistent operation.
&ptr Result:      Must point to an APTR variable that will store the resolved address.

-ERRORS-
Okay
Args
NullArgs
LockFailed
DeadLock: A process has locked the memory block and is currently sleeping on a response from the caller.
TimeOut
MemoryDoesNotExist: The MemoryID that you supplied does not refer to an existing memory block.
MarkedForDeletion: The memory block cannot be accessed because it has been marked for deletion.
SystemLocked
-END-

*********************************************************************************************************************/

ERROR AccessMemory(MEMORYID MemoryID, LONG Flags, LONG MilliSeconds, APTR *Result)
{
   pf::Log log(__FUNCTION__);

   if ((!MemoryID) or (!Result)) return log.warning(ERR_NullArgs);

   if (MilliSeconds <= 0) {
      log.warning("MemoryID: %d, Flags: $%x, TimeOut: %d - Invalid timeout", MemoryID, Flags, MilliSeconds);
      return ERR_Args;
   }

   // NB: Printing AccessMemory() calls is usually a waste of time unless the process is going to sleep.
   //log.trace("MemoryID: %d, Flags: $%x, TimeOut: %d", MemoryID, Flags, MilliSeconds);

   *Result = NULL;
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
            if (timeout <= 0) return log.warning(ERR_TimeOut);
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

   return ERR_MemoryDoesNotExist;
}

/*********************************************************************************************************************

-FUNCTION-
AccessObject: Grants exclusive access to objects via unique ID.
Category: Objects

This function resolves an object ID to its address and acquires a lock on the object so that other threads cannot use
it simultaneously.

If the object is already locked, it will wait until the object becomes available.   This must occur within the amount
of time specified in the Milliseconds parameter.  If the time expires, the function will return with an `ERR_TimeOut`
error code.  If successful, `ERR_Okay` is returned and a reference to the object's address is stored in the Result
variable.

It is crucial that calls to AccessObject() are followed with a call to ~ReleaseObject() once the lock is no
longer required.  Calls to AccessObject() will also nest, so they must be paired with ~ReleaseObject()
correctly.

It is recommended that C++ developers use the `ScopedObjectLock` class to acquire object locks rather than making
direct calls to AccessObject().  The following example illustrates lock acquisition within a 1 second time limit:

<pre>
{
   pf::ScopedObjectLock<OBJECTPTR> obj(my_object_id, 1000);
   if (lock.granted()) {
      obj.acDraw();
   }
}
</pre>


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

*********************************************************************************************************************/

ERROR AccessObject(OBJECTID ObjectID, LONG MilliSeconds, OBJECTPTR *Result)
{
   pf::Log log(__FUNCTION__);

   if ((!Result) or (!ObjectID)) return log.warning(ERR_NullArgs);
   if (MilliSeconds <= 0) log.warning(ERR_Args); // Warn but do not fail

   ThreadLock lock(TL_PRIVATE_MEM, 4000);
   if (lock.granted()) {
      auto mem = glPrivateMemory.find(ObjectID);
      if ((mem != glPrivateMemory.end()) and (mem->second.Address)) {
         if (auto error = LockObject((OBJECTPTR)mem->second.Address, MilliSeconds); !error) {
            *Result = (OBJECTPTR)mem->second.Address;
            return ERR_Okay;
         }
         else return error;
      }
      else if (ObjectID IS glMetaClass.UID) { // Access to the MetaClass requires this special case handler.
         if (auto error = LockObject(&glMetaClass, MilliSeconds); !error) {
            *Result = &glMetaClass;
            return ERR_Okay;
         }
         else return error;
      }
      else return ERR_NoMatchingObject;
   }
   else return log.warning(ERR_SystemLocked);
}

/*********************************************************************************************************************

-FUNCTION-
LockObject: Lock an object to prevent contention between threads.
Category: Objects

Use LockObject() to gain exclusive access to an object at thread-level.  This function provides identical
behaviour to that of ~AccessObject(), but with a slight speed advantage as the object ID does not need to be
resolved to an address.  Calls to LockObject() will nest, and must be matched with a call to
~ReleaseObject() to unlock the object.

Be aware that while this function is faster than ~AccessObject(), its use may be considered unsafe if other threads
could terminate the object without a suitable barrier in place.

If it is guaranteed that an object is not being shared between threads, object locking is unnecessary.

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

*********************************************************************************************************************/

ERROR LockObject(OBJECTPTR Object, LONG Timeout)
{
   pf::Log log(__FUNCTION__);

   if (!Object) {
      DEBUG_BREAK
      return log.warning(ERR_NullArgs);
   }

   LONG our_thread = get_thread_id();

   do {
      // Using an atomic increment we can achieve a 'quick lock' of the object without having to resort to locks.
      // This is quite safe so long as the developer is being careful with use of the object between threads (i.e. not
      // destroying the object when other threads could potentially be using it).

      if (Object->incQueue() IS 1) {
         Object->Locked = true;
         Object->ThreadID = our_thread;
         return ERR_Okay;
      }

      if (our_thread IS Object->ThreadID) { // Support nested locks.
         return ERR_Okay;
      }

      // Problem: If a ReleaseObject() were to occur inside this loop area, it receives a queue value of 1 instead of
      //    zero.  As a result it would not send a signal, because it mistakenly thinks it still has a lock.
      // Solution: When restoring the queue, we check for zero.  If true, we try to re-lock because we know that the
      //    object is free.  By not sleeping, we don't have to be concerned about the missing signal.
   } while (Object->subQueue() IS 0); // Make a correction because we didn't obtain the lock.  Repeat loop if the object lock is at zero (available).

   if (Object->defined(NF::FREE|NF::UNLOCK_FREE)) return ERR_MarkedForDeletion; // If the object is currently being removed by another thread, sleeping on it is pointless.

   // Problem: What if ReleaseObject() in another thread were to release the object prior to our TL_PRIVATE_OBJECTS lock?  This means that we would never receive the wake signal.
   // Solution: Prior to cond_wait(), increment the object queue to attempt a lock.  This is *slightly* less efficient than doing it after the cond_wait(), but
   //           it will prevent us from sleeping on a signal that we would never receive.

   LARGE end_time, current_time;
   if (Timeout < 0) end_time = 0x0fffffffffffffffLL; // Do not alter this value.
   else end_time = (PreciseTime() / 1000LL) + Timeout;

   Object->incSleep(); // Increment the sleep queue first so that ReleaseObject() will know that another thread is expecting a wake-up.

   ThreadLock lock(TL_PRIVATE_OBJECTS, Timeout);
   if (lock.granted()) {
      //log.function("TID: %d, Sleeping on #%d, Timeout: %d, Queue: %d, Locked By: %d", our_thread, Object->UID, Timeout, Object->Queue, Object->ThreadID);

      auto locks = (WaitLock *)ResolveAddress(glSharedControl, glSharedControl->WLOffset);
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
               Object->subSleep();
               return ERR_DoesNotExist;
            }

            if (Object->incQueue() IS 1) { // Increment the lock count - also doubles as a prv_access() call if the lock value is 1.
               locks[wl].WaitingForResourceID   = 0;
               locks[wl].WaitingForResourceType = 0;
               locks[wl].WaitingForProcessID    = 0;
               locks[wl].WaitingForThreadID     = 0;
               locks[wl].Flags = 0;
               Object->Locked = false;
               Object->ThreadID = our_thread;
               Object->subSleep();
               return ERR_Okay;
            }
            else Object->subQueue();

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

      Object->subSleep();
      return error;
   }
   else {
      Object->subSleep();
      return ERR_SystemLocked;
   }
}

/*********************************************************************************************************************

-FUNCTION-
AllocMutex: Allocate a mutex suitable for managing synchronisation between threads.

This function allocates a mutex that is suitable for keeping threads synchronised (inter-process synchronisation is not
supported).  Mutexes are locked and unlocked using the ~LockMutex() and ~UnlockMutex() functions.

The underlying implementation is dependent on the host platform.  In Microsoft Windows, critical sections will be
used and may nest (`ALF::RECURSIVE` always applies).  Unix systems employ pthread mutexes and will only nest if the
`ALF::RECURSIVE` option is specified.

-INPUT-
int(ALF) Flags: Optional flags.
&ptr Result: A reference to the new mutex will be returned in this variable if the call succeeds.

-ERRORS-
Okay
NullArgs
AllocMemory

*********************************************************************************************************************/

#ifdef __unix__
ERROR AllocMutex(ALF Flags, APTR *Result)
{
   pf::Log log(__FUNCTION__);

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

/*********************************************************************************************************************

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

*********************************************************************************************************************/

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

/*********************************************************************************************************************

-FUNCTION-
FreeMutex: Deallocate a private mutex.

This function will deallocate a mutex.  It is vital that no thread is sleeping on the mutex at the time this function
is called.  The outcome when calling this function on a mutex still in use is undefined.

-INPUT-
ptr Mutex: Pointer to a mutex.

*********************************************************************************************************************/

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


/*********************************************************************************************************************

-FUNCTION-
FreeSharedMutex: Deallocate a shared mutex.

This function will deallocate a shared mutex.  It is vital that no thread is sleeping on the mutex at the time this
function is called.  The outcome when calling this function on a mutex still in use is undefined.

-INPUT-
ptr Mutex: Reference to a shared mutex.

*********************************************************************************************************************/

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

/*********************************************************************************************************************

-FUNCTION-
LockMutex: Acquire a lock on a mutex.

This function will lock a mutex allocated by ~AllocMutex().  If the mutex is already locked by another thread, the
caller will sleep until the mutex is released or a time-out occurs.  If multiple threads are waiting on the mutex, the
order of acquisition is dependent on the rules of the host platform.  It is recommended that the client makes no
assumption as to the queue order and that the next thread to acquire the mutex will be randomly selected.

If the mutex was acquired with the `ALF::RECURSIVE` flag, then multiple calls to this function within the same thread
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

*********************************************************************************************************************/

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

/*********************************************************************************************************************

-FUNCTION-
LockSharedMutex: Acquire a lock on a shared mutex.

This function will lock a shared mutex allocated by ~AllocSharedMutex().  If the mutex is already locked by another
thread or process, the caller will sleep until the mutex is released or a time-out occurs.  If multiple threads are
waiting on the mutex, the order of acquisition is dependent on the rules of the host platform.  It is recommended that
the client makes no assumption as to the queue order and that the next thread to acquire the mutex will be randomly
selected.

If the mutex was acquired with the `ALF::RECURSIVE` flag, then multiple calls to this function within the same thread
will nest.  It will be necessary to call ~UnlockSharedMutex() for every lock that has been acquired.

-INPUT-
ptr Mutex: Reference to a mutex allocated from ~AllocSharedMutex()
int MilliSeconds: Timeout in milliseconds.

-ERRORS-
Okay
NullArgs
TimeOut

*********************************************************************************************************************/

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

/*********************************************************************************************************************

-FUNCTION-
ReleaseMemory: Releases a lock from a memory based resource.
Category: Memory

Successful calls to ~AccessMemory() must be paired with a call to ReleaseMemory() so that the memory can be made
available to other processes.  By releasing the resource, the access count will decrease, and if applicable a
thread that is in the queue for access may then be able to acquire a lock.

-INPUT-
mem MemoryID: A reference to a memory resource for release.

-ERRORS-
Okay
NullArgs
Search
SystemLocked
-END-

*********************************************************************************************************************/

ERROR ReleaseMemory(MEMORYID MemoryID)
{
   pf::Log log(__FUNCTION__);

   if (!MemoryID) return log.warning(ERR_NullArgs);

   ThreadLock lock(TL_PRIVATE_MEM, 4000);
   if (lock.granted()) {
      auto mem = glPrivateMemory.find(MemoryID);

      if ((mem IS glPrivateMemory.end()) or (!mem->second.Address)) {
         if (tlContext->object()->Class) log.warning("Unable to find a record for memory address #%d [Context %d, Class %s].", MemoryID, tlContext->object()->UID, tlContext->object()->className());
         else log.warning("Unable to find a record for memory #%d.", MemoryID);
         if (glLogLevel > 1) print_diagnosis(0);
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

/*********************************************************************************************************************

-FUNCTION-
ReleaseObject: Release a locked private object.
Category: Objects

Release a lock previously obtained from ~AccessObject() or ~LockObject().  Locks will nest, so a release is
required for every lock that has been granted.

-INPUT-
obj Object: Pointer to the object to be released.

*********************************************************************************************************************/

void ReleaseObject(OBJECTPTR Object)
{
   pf::Log log(__FUNCTION__);

   if (!Object) return;

   #ifdef DEBUG
      LONG our_thread = get_thread_id();
      if (Object->ThreadID != our_thread) {
         log.traceWarning("Invalid call to ReleaseObject(), locked by thread #%d, we are #%d", Object->ThreadID, our_thread);
         return;
      }
   #endif

   // If the queue reaches zero, check if there are other threads sleeping on this object.  If so, use a signal to
   // wake at least one of them.

   if (Object->subQueue() > 0) return;

   Object->Locked = false;

   if (Object->SleepQueue > 0) {
      #ifdef DEBUG
         log.traceBranch("Thread: %d - Waking 1 of %d threads", our_thread, Object->SleepQueue);
      #endif

      if (!thread_lock(TL_PRIVATE_OBJECTS, -1)) {
         if (Object->defined(NF::FREE|NF::UNLOCK_FREE)) { // We have to tell other threads that the object is marked for deletion.
            // NB: A lock on PL_WAITLOCKS is not required because we're already protected by the TL_PRIVATE_OBJECTS
            // barrier (which is common between LockObject() and ReleaseObject()
            auto locks = (WaitLock *)ResolveAddress(glSharedControl, glSharedControl->WLOffset);
            for (WORD i=0; i < glSharedControl->WLIndex; i++) {
               if ((locks[i].WaitingForResourceID IS Object->UID) and (locks[i].WaitingForResourceType IS RT_OBJECT)) {
                  locks[i].Flags |= WLF_REMOVED;
               }
            }
         }

         // Destroy the object if marked for deletion.  NOTE: It is the responsibility of the programmer
         // to ensure that once an object has been marked for deletion, references to the object pointer
         // are removed so that no thread will attempt to use it during deallocation.

         if (Object->defined(NF::UNLOCK_FREE) and (!Object->defined(NF::FREE))) {
            Object->Flags &= ~NF::UNLOCK_FREE;
            FreeResource(Object);

            cond_wake_all(CN_OBJECTS);
         }
         else cond_wake_single(CN_OBJECTS); // This will either be a critical section (Windows) or private conditional (Posix)

         thread_unlock(TL_PRIVATE_OBJECTS);
      }
      else exit(0);
   }
   else if (Object->defined(NF::UNLOCK_FREE) and (!Object->defined(NF::FREE))) {
      Object->Flags &= ~NF::UNLOCK_FREE;
      FreeResource(Object);
   }
}

/*********************************************************************************************************************

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

*********************************************************************************************************************/

#ifdef __unix__

ERROR SysLock(LONG Index, LONG Timeout)
{
   pf::Log log(__FUNCTION__);
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
   pf::Log log(__FUNCTION__);
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

/*********************************************************************************************************************

-FUNCTION-
SysUnlock: Releases a lock obtained from SysLock().
Status: private

Use the SysUnlock() function to undo a previous call to ~SysLock() for a given mutex.

-INPUT-
int Index: The index number of the system mutex that needs to be unlocked.

-ERRORS-
Okay
-END-

*********************************************************************************************************************/

#ifdef __unix__

ERROR SysUnlock(LONG Index)
{
   pf::Log log(__FUNCTION__);

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

/*********************************************************************************************************************

-FUNCTION-
UnlockMutex: Release a locked mutex.

This function will unlock any mutex that has been locked with the ~LockMutex() function.  If the mutex is nested, this
function must be called enough times to match the calls to ~LockMutex() before it will be released to other processes.

If the target mutex has no lock, or if the lock belongs to another thread, the resulting behaviour is undefined and
may result in an exception.

-INPUT-
ptr Mutex: Reference to a locked mutex.
-END-

*********************************************************************************************************************/

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

/*********************************************************************************************************************

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

*********************************************************************************************************************/

#ifdef __unix__
void UnlockSharedMutex(APTR Mutex)
{
   pReleaseSemaphore((MAXINT)Mutex, 0);
}
#elif _WIN32 // Refer to windows.c UnlockSharedMutex()
#else
#error No support for UnlockSharedMutex()
#endif
