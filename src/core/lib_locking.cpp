/*********************************************************************************************************************

The source code of the Parasol Framework is made publicly available under the terms described in the LICENSE.TXT file
that is distributed with this package.  Please refer to it for further information on licensing.

CORE LOCKING MANAGEMENT
-------------------------
Most technical code regarding system locking is managed in this area.  Also check out lib_semaphores.c and lib_messages.c.

*********************************************************************************************************************/

#ifndef DBG_LOCKS // Debugging is off unless DBG_LOCKS is explicitly defined.
#undef DEBUG
#endif

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
THREADVAR bool tlMessageBreak = false; // This variable is set by ProcessMessages() to allow breaking when Windows sends OS messages
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
   static THREADVAR bool init = false;

   if (!init) {
      tlSocket.sun_family = AF_UNIX;
      ClearMemory(tlSocket.sun_path, sizeof(tlSocket.sun_path));
      tlSocket.sun_path[0] = '\0';
      tlSocket.sun_path[1] = 'p';
      tlSocket.sun_path[2] = 's';
      tlSocket.sun_path[3] = 'l';
      init = true;
   }

   ((LONG *)(tlSocket.sun_path+4))[0] = ProcessID;
   *Size = sizeof(sa_family_t) + 4 + sizeof(LONG);
   return &tlSocket;
}
#endif

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

static ERROR alloc_cond(CONDLOCK *Lock, ALF Flags)
{
   LONG result;

   if (Flags != ALF::NIL) {
      pthread_condattr_t attrib;
      if (!(result = pthread_condattr_init(&attrib))) {
         if ((Flags & ALF::SHARED) != ALF::NIL) pthread_condattr_setpshared(&attrib, PTHREAD_PROCESS_SHARED); // Allow the mutex to be used across foreign processes.
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

ERROR cond_wait(UBYTE Lock, UBYTE Cond, LONG Timeout)
{
   pf::Log log(__FUNCTION__);

#ifdef __ANDROID__
   if (Timeout <= 0) pthread_cond_wait(&glPrivateCond[Cond], &glPrivateLocks[Lock]);
   else pthread_cond_timeout_np(&glPrivateCond[Cond], &glPrivateLocks[Lock], Timeout); // This is an Android-only function
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

      LONG result = pthread_cond_timedwait(&glPrivateCond[Cond], &glPrivateLocks[Lock], &timestamp);
      if (result) log.warning("Error: %s", strerror(errno));
      if ((result IS ETIMEDOUT) or (result IS EAGAIN)) return ERR_TimeOut;
      else if (result) return ERR_Failed;
   }
   else pthread_cond_wait(&glPrivateCond[Cond], &glPrivateLocks[Lock]);
#endif

   return ERR_Okay;
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

//********************************************************************************************************************

struct WaitLock {
   LONG ThreadID; // The thread represented by this wait-lock
   #ifdef _WIN32
   WINHANDLE Lock;
   #endif
   LARGE WaitingTime;
   LONG  WaitingForThreadID;
   LONG  WaitingForResourceID;
   LONG  WaitingForResourceType;
   UBYTE Flags; // WLF flags

   #define WLF_REMOVED 0x01  // Set if the resource was removed by the thread that was holding it.

   WaitLock() : ThreadID(0) { }
   WaitLock(LONG pThread) : ThreadID(pThread) { }

   void setThread(LONG pThread) { ThreadID = pThread; }

   void notWaiting() {
      Flags = 0;
      WaitingForResourceID = 0;
      WaitingForResourceType = 0;
      WaitingForThreadID = 0;  // NB: Important that you clear this last if you are to avoid threading conflicts.
   }
};

static THREADVAR WORD glWLIndex = -1; // The current thread's index within glWaitLocks
static std::vector<WaitLock> glWaitLocks;
static std::mutex glWaitLockMutex;

/*********************************************************************************************************************
** Prepare a thread for going to sleep on a resource.  Checks for deadlocks in advance.  Once a thread has added a
** WakeLock entry, it must keep it until either the thread or process is destroyed.
**
** Used by AccessMemory() and LockObject()
*/

ERROR init_sleep(LONG OtherThreadID, LONG ResourceID, LONG ResourceType)
{
   //log.trace("Sleeping on thread %d for resource #%d, Total Threads: %d", OtherThreadID, ResourceID, LONG(glWaitLocks.size()));

   auto our_thread = get_thread_id();
   if (OtherThreadID IS our_thread) return ERR_Args;

   const std::lock_guard<std::mutex> lock(glWaitLockMutex);

   if (glWLIndex IS -1) { // New thread that isn't registered yet
      unsigned i = 0;
      for (; i < glWaitLocks.size(); i++) {
         if (!glWaitLocks[i].ThreadID) break;
      }

      glWLIndex = i;
      if (i IS glWaitLocks.size()) glWaitLocks.push_back(our_thread);
      else glWaitLocks[glWLIndex].setThread(our_thread);
   }
   else { // Check for deadlocks.  If a deadlock will occur then we return immediately.
      for (unsigned i=0; i < glWaitLocks.size(); i++) {
         if (glWaitLocks[i].ThreadID IS OtherThreadID) {
            if (glWaitLocks[i].WaitingForThreadID IS our_thread) {
               pf::Log log(__FUNCTION__);
               log.warning("Deadlock: Thread %d holds resource #%d and is waiting for us (%d) to release #%d.", glWaitLocks[i].ThreadID, ResourceID, our_thread, glWaitLocks[i].WaitingForResourceID);
               return ERR_DeadLock;
            }
         }
      }
   }

   glWaitLocks[glWLIndex].WaitingForResourceID   = ResourceID;
   glWaitLocks[glWLIndex].WaitingForResourceType = ResourceType;
   glWaitLocks[glWLIndex].WaitingForThreadID     = OtherThreadID;
   #ifdef _WIN32
   glWaitLocks[glWLIndex].Lock = get_threadlock();
   #endif

   return ERR_Okay;
}

//********************************************************************************************************************

void warn_threads_of_object_removal(OBJECTID UID)
{
   // We use WLF_REMOVED to tell other threads in LockObject() that an object has been removed.

   if (!thread_lock(TL_OBJECT_LOCKING, -1)) {
      for (unsigned i=0; i < glWaitLocks.size(); i++) {
         if ((glWaitLocks[i].WaitingForResourceID IS UID) and
             (glWaitLocks[i].WaitingForResourceType IS RT_OBJECT)) {
            glWaitLocks[i].Flags |= WLF_REMOVED;
         }
      }

      cond_wake_all(CN_OBJECTS);
      thread_unlock(TL_OBJECT_LOCKING);
   }
}

//********************************************************************************************************************
// Remove all the wait-locks for the current process (affects all threads).  Lingering wait-locks are indicative of
// serious problems, as all should have been released on shutdown.

void remove_process_waitlocks(void)
{
   pf::Log log("Shutdown");
   log.trace("Removing process waitlocks...");

   auto const our_thread = get_thread_id();

   const std::lock_guard<std::mutex> lock(glWaitLockMutex);

   for (unsigned i=0; i < glWaitLocks.size(); i++) {
      if (glWaitLocks[i].ThreadID IS our_thread) {
         glWaitLocks[i].notWaiting();
      }
      else if (glWaitLocks[i].WaitingForThreadID IS our_thread) { // A thread is waiting on us, wake it up.
         #ifdef _WIN32
            log.warning("Waking thread %d", glWaitLocks[i].ThreadID);
            glWaitLocks[i].notWaiting();
            wake_waitlock(glWaitLocks[i].Lock, 1);
         #endif
      }
   }
}

//********************************************************************************************************************
// Windows thread-lock support.  Each thread gets its own semaphore.  Note that this is intended for handling public
// resources only.  Internally, use critical sections for synchronisation between threads.

#ifdef _WIN32
static std::atomic_int glThreadLockIndex = 1; // Shared between all threads.
static bool glTLInit = false;
static WINHANDLE glThreadLocks[MAX_THREADS]; // Shared between all threads, used for resource tracking allocated wake locks.
static THREADVAR WINHANDLE tlThreadLock = 0; // Local to the thread.

// Returns the thread-lock semaphore for the active thread.

WINHANDLE get_threadlock(void)
{
   pf::Log log(__FUNCTION__);

   if (tlThreadLock) return tlThreadLock; // Thread-local, no problem...

   if (!glTLInit) {
      glTLInit = true;
      ClearMemory(glThreadLocks, sizeof(glThreadLocks));
   }

   auto index = glThreadLockIndex++;
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
      index = glThreadLockIndex++;
   }

   log.warning("Failed to allocate a new wake-lock.  Index: %d/%d", glThreadLockIndex.load(), MAX_THREADS);
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
int(MEM) Flags:   Set to READ, WRITE or READ_WRITE according to requirements.
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

ERROR AccessMemory(MEMORYID MemoryID, MEM Flags, LONG MilliSeconds, APTR *Result)
{
   pf::Log log(__FUNCTION__);

   if ((!MemoryID) or (!Result)) return log.warning(ERR_NullArgs);

   if (MilliSeconds <= 0) {
      log.warning("MemoryID: %d, Flags: $%x, TimeOut: %d - Invalid timeout", MemoryID, LONG(Flags), MilliSeconds);
      return ERR_Args;
   }

   // NB: Logging AccessMemory() calls is usually a waste of time unless the process is going to sleep.
   //log.trace("MemoryID: %d, Flags: $%x, TimeOut: %d", MemoryID, LONG(Flags), MilliSeconds);

   *Result = NULL;
   ThreadLock lock(TL_PRIVATE_MEM, 4000);
   if (lock.granted()) {
      auto mem = glPrivateMemory.find(MemoryID);
      if ((mem != glPrivateMemory.end()) and (mem->second.Address)) {
         auto our_thread = get_thread_id();
         // This loop looks odd, but will prevent sleeping if we already have a lock on the memory.
         // cond_wait() will be met with a global wake-up, not necessarily on the desired block, hence the need for while().

         LARGE end_time = (PreciseTime() / 1000LL) + MilliSeconds;
         ERROR error = ERR_TimeOut;
         while ((mem->second.AccessCount > 0) and (mem->second.ThreadLockID != our_thread)) {
            LONG timeout = end_time - (PreciseTime() / 1000LL);
            if (timeout <= 0) return log.warning(ERR_TimeOut);
            else {
               //log.msg("Sleep on memory #%d, Access %d, Threads %d/%d", MemoryID, mem->second.AccessCount, (LONG)mem->second.ThreadLockID, our_thread);
               if ((error = cond_wait(TL_PRIVATE_MEM, CN_PRIVATE_MEM, timeout))) {
                  return log.warning(error);
               }
            }
         }

         mem->second.ThreadLockID = our_thread;
         mem->second.AccessCount++;
         tlPrivateLockCount++;

         *Result = mem->second.Address;
         return ERR_Okay;
      }
      else log.traceWarning("Cannot find memory ID #%d", MemoryID); // This is not uncommon, so trace only
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

Use LockObject() to gain exclusive access to an object at thread-level.  This function provides identical behaviour
to that of ~AccessObject(), but with a slight speed advantage as the object ID does not need to be resolved to an
address.  Calls to LockObject() will nest, and must be matched with a call to ~ReleaseObject() to unlock the object.

Be aware that while this function is faster than ~AccessObject(), it is unsafe if other threads could terminate the
object without a suitable barrier in place.

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

   auto our_thread = get_thread_id();

   do {
      // Using an atomic increment we can achieve a 'quick lock' of the object without having to resort to locks.
      // This is quite safe so long as the developer is being careful with use of the object between threads (i.e. not
      // destroying the object when other threads could potentially be using it).

      if (++Object->Queue IS 1) {
         Object->Locked = true;
         Object->ThreadID = our_thread;
         return ERR_Okay;
      }

      if (our_thread IS Object->ThreadID) { // Support nested locks.
         return ERR_Okay;
      }

      // Problem: If a ReleaseObject() were to occur inside this while loop, it receives a queue value of 1 instead of
      //    zero.  As a result it would not send a signal, because it mistakenly thinks it still has a lock.
      // Solution: When restoring the queue, we check for zero.  If true, we try to re-lock because we know that the
      //    object is free.  By not sleeping, we don't have to be concerned about the missing signal.
   } while (--Object->Queue IS 0); // Make a correction because we didn't obtain the lock.  Repeat loop if the object lock is at zero (available).

   if (Object->defined(NF::FREE|NF::FREE_ON_UNLOCK)) return ERR_MarkedForDeletion; // If the object is currently being removed by another thread, sleeping on it is pointless.

   // Problem: What if ReleaseObject() in another thread were to release the object prior to our TL_OBJECT_LOCKING lock?  This means that we would never receive the wake signal.
   // Solution: Prior to cond_wait(), increment the object queue to attempt a lock.  This is *slightly* less efficient than doing it after the cond_wait(), but
   //           it will prevent us from sleeping on a signal that we would never receive.

   LARGE end_time, current_time;
   if (Timeout < 0) end_time = 0x0fffffffffffffffLL; // Do not alter this value.
   else end_time = (PreciseTime() / 1000LL) + Timeout;

   Object->incSleep(); // Increment the sleep queue first so that ReleaseObject() will know that another thread is expecting a wake-up.

   ThreadLock lock(TL_OBJECT_LOCKING, Timeout);
   if (lock.granted()) {
      //log.function("TID: %d, Sleeping on #%d, Timeout: %d, Queue: %d, Locked By: %d", our_thread, Object->UID, Timeout, Object->Queue, Object->ThreadID);

      ERROR error = ERR_TimeOut;
      if (!init_sleep(Object->ThreadID, Object->UID, RT_OBJECT)) { // Indicate that our thread is sleeping.
         while ((current_time = (PreciseTime() / 1000LL)) < end_time) {
            auto tmout = end_time - current_time;
            if (tmout < 0) tmout = 0;

            if (glWaitLocks[glWLIndex].Flags & WLF_REMOVED) {
               glWaitLocks[glWLIndex].notWaiting();
               Object->subSleep();
               return ERR_DoesNotExist;
            }

            if (++Object->Queue IS 1) { // Increment the lock count - also doubles as a prv_access() call if the lock value is 1.
               glWaitLocks[glWLIndex].notWaiting();
               Object->Locked = false;
               Object->ThreadID = our_thread;
               Object->subSleep();
               return ERR_Okay;
            }
            else --Object->Queue;

            cond_wait(TL_OBJECT_LOCKING, CN_OBJECTS, tmout);
         } // end while()

         // Failure: Either a timeout occurred or the object no longer exists.

         if (glWaitLocks[glWLIndex].Flags & WLF_REMOVED) {
            pf::Log log(__FUNCTION__);
            log.warning("TID %d: The resource no longer exists.", get_thread_id());
            error = ERR_DoesNotExist;
         }
         else {
            log.traceWarning("TID: %d, #%d, Timeout occurred.", our_thread, Object->UID);
            error = ERR_TimeOut;
         }

         glWaitLocks[glWLIndex].notWaiting();
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
      if (mem->second.AccessCount > 0) { // Sometimes ReleaseMemory() is called on addresses that aren't actually locked.  This is OK - we simply don't do anything in that case.
         access = --mem->second.AccessCount;
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

         if ((mem->second.Flags & MEM::DELETE) != MEM::NIL) {
            log.trace("Deleting marked memory block #%d (MEM::DELETE)", MemoryID);
            FreeResource(mem->second.Address);
            cond_wake_all(CN_PRIVATE_MEM); // Wake up any threads sleeping on this memory block.
            return ERR_Okay;
         }
         else if ((mem->second.Flags & MEM::EXCLUSIVE) != MEM::NIL) {
            mem->second.Flags &= ~MEM::EXCLUSIVE;
         }

         cond_wake_all(CN_PRIVATE_MEM); // Wake up any threads sleeping on this memory block.
      }

      return ERR_Okay;
   }
   else return log.warning(ERR_SystemLocked);
}

/*********************************************************************************************************************

-FUNCTION-
ReleaseObject: Release a locked object.
Category: Objects

Release a lock previously obtained from ~AccessObject() or ~LockObject().  Locks will nest, so a release is required
for every lock that has been granted.

-INPUT-
obj Object: Pointer to the object to be released.

*********************************************************************************************************************/

void ReleaseObject(OBJECTPTR Object)
{
   if (!Object) return;

   if (--Object->Queue > 0) return;

   Object->Locked = false;

   if (Object->SleepQueue > 0) { // Other threads are waiting on this object
      pf::Log log(__FUNCTION__);
      log.traceBranch("Waking %d threads for this object.", Object->SleepQueue.load());

      if (!thread_lock(TL_OBJECT_LOCKING, -1)) {
         if (Object->defined(NF::FREE|NF::FREE_ON_UNLOCK)) { // We have to tell other threads that the object is marked for deletion.
            // NB: A lock on glWaitLocks is not required because we're already protected by the TL_OBJECT_LOCKING
            // barrier (which is common between LockObject() and ReleaseObject()
            for (unsigned i=0; i < glWaitLocks.size(); i++) {
               if ((glWaitLocks[i].WaitingForResourceID IS Object->UID) and
                   (glWaitLocks[i].WaitingForResourceType IS RT_OBJECT)) {
                  glWaitLocks[i].Flags |= WLF_REMOVED;
               }
            }
         }

         // Destroy the object if marked for deletion.

         if (Object->defined(NF::FREE_ON_UNLOCK) and (!Object->defined(NF::FREE))) {
            Object->Flags &= ~NF::FREE_ON_UNLOCK;
            FreeResource(Object);
         }

         cond_wake_all(CN_OBJECTS);
         thread_unlock(TL_OBJECT_LOCKING);
      }
   }
   else if (Object->defined(NF::FREE_ON_UNLOCK) and (!Object->defined(NF::FREE))) {
      Object->Flags &= ~NF::FREE_ON_UNLOCK;
      FreeResource(Object);
   }
}
