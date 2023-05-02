/*********************************************************************************************************************

The source code of the Parasol Framework is made publicly available under the terms described in the LICENSE.TXT file
that is distributed with this package.  Please refer to it for further information on licensing.

*********************************************************************************************************************/

#include "defs.h"

using namespace pf;

#ifdef _WIN32
THREADVAR bool tlMessageBreak = false; // This variable is set by ProcessMessages() to allow breaking when Windows sends OS messages
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

// Returns the thread-lock semaphore for the active thread.  This is required for putting a thread to sleep in a way that
// is compatible with the Win32 API.

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
Args: The MilliSeconds value is less or equal to zero.
NullArgs
SystemLocked
TimeOut
MemoryDoesNotExist: The MemoryID that you supplied does not refer to an existing memory block.
-END-

*********************************************************************************************************************/

ERROR AccessMemory(MEMORYID MemoryID, MEM Flags, LONG MilliSeconds, APTR *Result)
{
   pf::Log log(__FUNCTION__);

   if ((!MemoryID) or (!Result)) return log.warning(ERR_NullArgs);
   if (MilliSeconds <= 0) return log.warning(ERR_Args);

   // NB: Logging AccessMemory() calls is usually a waste of time unless the process is going to sleep.
   //log.trace("MemoryID: %d, Flags: $%x, TimeOut: %d", MemoryID, LONG(Flags), MilliSeconds);

   *Result = NULL;
   if (auto lock = std::unique_lock{glmMemory}) {
      auto mem = glPrivateMemory.find(MemoryID);
      if ((mem != glPrivateMemory.end()) and (mem->second.Address)) {
         auto our_thread = get_thread_id();

         // This loop condition verifies that the block is available and protects against recursion.
         // wait_for() is awoken with a global wake-up, not necessarily on the desired block, hence the need for while().

         LARGE end_time = 0;
         while ((mem->second.AccessCount > 0) and (mem->second.ThreadLockID != our_thread)) {
            auto current_time = PreciseTime();
            if (!end_time) end_time = (current_time / 1000LL) + MilliSeconds;
            auto timeout = end_time - (current_time / 1000LL);
            if (timeout <= 0) return log.warning(ERR_TimeOut);
            else {
               //log.msg("Sleep on memory #%d, Access %d, Threads %d/%d", MemoryID, mem->second.AccessCount, (LONG)mem->second.ThreadLockID, our_thread);
               if (cvResources.wait_for(glmMemory, std::chrono::milliseconds{timeout}) IS std::cv_status::timeout) {
                  return log.warning(ERR_TimeOut);
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
NoMatchingObject
TimeOut
SystemLocked
-END-

*********************************************************************************************************************/

ERROR AccessObject(OBJECTID ObjectID, LONG MilliSeconds, OBJECTPTR *Result)
{
   pf::Log log(__FUNCTION__);

   if ((!Result) or (!ObjectID)) return log.warning(ERR_NullArgs);
   if (MilliSeconds <= 0) log.warning(ERR_Args); // Warn but do not fail

   if (auto lock = std::unique_lock{glmMemory}) {
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

   // Problem: What if ReleaseObject() in another thread were to release the object prior to our glmObjectLocking lock?  This means that we would never receive the wake signal.
   // Solution: Prior to wait_until(), increment the object queue to attempt a lock.  This is *slightly* less efficient than doing it after the cond_wait(), but
   //           it will prevent us from sleeping on a signal that we would never receive.

   LARGE end_time, current_time;
   if (Timeout < 0) end_time = 0x0fffffffffffffffLL; // Do not alter this value.
   else end_time = (PreciseTime() / 1000LL) + Timeout;

   Object->SleepQueue++; // Increment the sleep queue first so that ReleaseObject() will know that another thread is expecting a wake-up.

   if (auto lock = std::unique_lock{glmObjectLocking, std::chrono::milliseconds(Timeout)}) {
      //log.function("TID: %d, Sleeping on #%d, Timeout: %d, Queue: %d, Locked By: %d", our_thread, Object->UID, Timeout, Object->Queue, Object->ThreadID);

      ERROR error = ERR_TimeOut;
      if (!init_sleep(Object->ThreadID, Object->UID, RT_OBJECT)) { // Indicate that our thread is sleeping.
         while ((current_time = (PreciseTime() / 1000LL)) < end_time) {
            auto tmout = end_time - current_time;
            if (tmout < 0) tmout = 0;

            if (glWaitLocks[glWLIndex].Flags & WLF_REMOVED) {
               glWaitLocks[glWLIndex].notWaiting();
               Object->SleepQueue--;
               return ERR_DoesNotExist;
            }

            if (++Object->Queue IS 1) { // Increment the lock count - also doubles as a prv_access() call if the lock value is 1.
               glWaitLocks[glWLIndex].notWaiting();
               Object->Locked = false;
               Object->ThreadID = our_thread;
               Object->SleepQueue--;
               return ERR_Okay;
            }
            else --Object->Queue;

            if (cvObjects.wait_for(glmObjectLocking, std::chrono::milliseconds{tmout}) IS std::cv_status::timeout) break;
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

      Object->SleepQueue--;
      return error;
   }
   else {
      Object->SleepQueue--;
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

   std::lock_guard lock(glmMemory);
   auto mem = glPrivateMemory.find(MemoryID);

   if ((mem IS glPrivateMemory.end()) or (!mem->second.Address)) { // Sanity check; this should never happen
      if (tlContext->object()->Class) log.warning("Unable to find a record for memory address #%d [Context %d, Class %s].", MemoryID, tlContext->object()->UID, tlContext->object()->className());
      else log.warning("Unable to find a record for memory #%d.", MemoryID);
      return ERR_Search;
   }

   WORD access;
   if (mem->second.AccessCount > 0) { // Sometimes ReleaseMemory() is called on addresses that aren't actually locked.  This is OK - we simply don't do anything in that case.
      access = --mem->second.AccessCount;
      tlPrivateLockCount--;
   }
   else access = -1;

   if (!access) {
      mem->second.ThreadLockID = 0;

      if ((mem->second.Flags & MEM::DELETE) != MEM::NIL) {
         log.trace("Deleting marked memory block #%d (MEM::DELETE)", MemoryID);
         FreeResource(mem->second.Address);
      }
      else if ((mem->second.Flags & MEM::EXCLUSIVE) != MEM::NIL) {
         mem->second.Flags &= ~MEM::EXCLUSIVE;
      }

      cvResources.notify_all(); // Wake up any threads sleeping on this memory block.
   }

   return ERR_Okay;
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

      if (auto lock = std::unique_lock{glmObjectLocking}) {
         if (Object->defined(NF::FREE|NF::FREE_ON_UNLOCK)) { // We have to tell other threads that the object is marked for deletion.
            // NB: A lock on glWaitLocks is not required because we're already protected by the glmObjectLocking
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

         cvObjects.notify_all();
      }
   }
   else if (Object->defined(NF::FREE_ON_UNLOCK) and (!Object->defined(NF::FREE))) {
      Object->Flags &= ~NF::FREE_ON_UNLOCK;
      FreeResource(Object);
   }
}
