/*********************************************************************************************************************

The source code of the Parasol Framework is made publicly available under the terms described in the LICENSE.TXT file
that is distributed with this package.  Please refer to it for further information on licensing.

*********************************************************************************************************************/

#include "defs.h"
#include <atomic>
#include <chrono>
#include <unordered_map>
#include <unordered_set>
#include <concepts>
#include <array>
#include <mutex>
#include <shared_mutex>

using namespace pf;
using namespace std::chrono;

//********************************************************************************************************************
// Hash function for THREADID to enable use in std::unordered_map

namespace std {
   template<>
   struct hash<THREADID> {
      std::size_t operator()(const THREADID& tid) const noexcept {
         return std::hash<int>{}(int(tid));
      }
   };
}

#ifdef _WIN32
thread_local bool tlMessageBreak = false; // This variable is set by ProcessMessages() to allow breaking when Windows sends OS messages
#endif

//********************************************************************************************************************
// C++20 Concepts for type safety

template<typename T>
concept Lockable = requires(T t) {
   { t.Queue } -> std::convertible_to<std::atomic<char>&>;
   { t.ThreadID } -> std::convertible_to<int&>;
   { t.SleepQueue } -> std::convertible_to<std::atomic<char>&>;
   { t.UID } -> std::convertible_to<OBJECTID>;
};

//********************************************************************************************************************
// Modern deadlock detection system

class DeadlockDetector {
private:
   mutable std::shared_mutex detector_mutex;
   std::unordered_map<THREADID, THREADID> waiting_for;

public:
   bool would_deadlock(THREADID Requester, THREADID Holder) const {
      std::shared_lock lock(detector_mutex);

      THREADID current = Holder;
      std::unordered_set<THREADID> visited;

      while (true) {
         if (visited.contains(current)) return false; // Cycle detected, but not involving requester
         visited.insert(current);

         auto it = waiting_for.find(current);
         if (it IS waiting_for.end()) return false;
         if (it->second IS Requester) return true;
         current = it->second;
      }
   }

   void add_wait(THREADID Waiter, THREADID Holder) {
      std::lock_guard lock(detector_mutex);
      waiting_for[Waiter] = Holder;
   }

   void remove_wait(THREADID Waiter) {
      std::lock_guard lock(detector_mutex);
      waiting_for.erase(Waiter);
   }

   void clear_all() {
      std::lock_guard lock(detector_mutex);
      waiting_for.clear();
   }
};

static DeadlockDetector glDeadlockDetector;

//********************************************************************************************************************
// Modern thread lock management with RAII

#ifdef _WIN32
class ThreadLockManager {
private:
   std::array<std::atomic<WINHANDLE>, MAX_THREADS> thread_locks{};
   std::atomic<int> next_index{1};
   std::once_flag init_flag;

   WINHANDLE allocate_lock() {
      for (int attempts = 0; attempts < MAX_THREADS; ++attempts) {
         int index = next_index.fetch_add(1, std::memory_order_relaxed) % MAX_THREADS;
         if (index IS 0) index = 1; // Skip index 0

         WINHANDLE expected = WINHANDLE(0);
         WINHANDLE new_lock;

         if (alloc_public_waitlock(&new_lock, nullptr) IS ERR::Okay) {
            if (thread_locks[index].compare_exchange_weak(expected, new_lock, std::memory_order_acquire)) {
               pf::Log log("ThreadLockManager");
               log.trace("Allocated thread-lock #%d for thread #%d", index, get_thread_id());
               return new_lock;
            }
            free_public_waitlock(new_lock);
         }
      }

      return WINHANDLE(0); // Graceful failure instead of exit(0)
   }

public:
   ThreadLockManager() {
      std::call_once(init_flag, [this]() {
         // Initialize each atomic individually since they can't be copied
         for (auto& lock : thread_locks) {
            lock.store(WINHANDLE(0), std::memory_order_relaxed);
         }
      });
   }

   WINHANDLE get_thread_lock() {
      thread_local WINHANDLE tl_lock = allocate_lock();
      return tl_lock;
   }

   void free_all_locks() {
      for (auto& lock_atomic : thread_locks) {
         WINHANDLE lock = lock_atomic.exchange(WINHANDLE(0), std::memory_order_acquire);
         if (lock) {
            free_public_waitlock(lock);
         }
      }
   }

   void free_thread_lock(WINHANDLE Lock) {
      if (!Lock) return;

      for (auto& lock_atomic : thread_locks) {
         WINHANDLE expected = Lock;
         if (lock_atomic.compare_exchange_weak(expected, WINHANDLE(0), std::memory_order_release)) {
            winCloseHandle(Lock);
            break;
         }
      }
   }
};

static ThreadLockManager glThreadLockManager;
#endif

//********************************************************************************************************************

struct WaitLock {
   THREADID ThreadID; // The thread represented by this wait-lock
   #ifdef _WIN32
   WINHANDLE Lock;
   #endif
   int64_t WaitingTime;
   THREADID WaitingForThreadID;
   int  WaitingForResourceID;
   int  WaitingForResourceType;
   uint8_t Flags; // WLF flags

   #define WLF_REMOVED 0x01  // Set if the resource was removed by the thread that was holding it.

   WaitLock() : ThreadID(0) { }
   WaitLock(THREADID pThread) : ThreadID(pThread) { }

   void setThread(const THREADID pThread) { ThreadID = pThread; }

   void notWaiting() {
      Flags = 0;
      WaitingForResourceID = 0;
      WaitingForResourceType = 0;
      WaitingForThreadID = THREADID(0);  // NB: Important that you clear this last if you are to avoid threading conflicts.
   }
};

static thread_local int16_t glWLIndex = -1; // The current thread's index within glWaitLocks
static std::vector<WaitLock> glWaitLocks;
static std::mutex glWaitLockMutex;

//********************************************************************************************************************
// Prepare a thread for going to sleep on a resource.  Checks for deadlocks in advance.  Once a thread has added a
// WakeLock entry, it must keep it until either the thread or process is destroyed.
//
// Used by AccessMemory() and LockObject()

ERR init_sleep(THREADID OtherThreadID, int ResourceID, int ResourceType)
{
   //log.trace("Sleeping on thread %d for resource #%d, Total Threads: %d", OtherThreadID, ResourceID, int(glWaitLocks.size()));

   auto our_thread = get_thread_id();
   if (OtherThreadID IS our_thread) return ERR::Args;

   const std::lock_guard<std::mutex> lock(glWaitLockMutex);

   if (glWLIndex IS -1) { // New thread that isn't registered yet
      unsigned i = 0;
      for (; i < glWaitLocks.size(); i++) {
         if (!glWaitLocks[i].ThreadID.defined()) break;
      }

      glWLIndex = i;
      if (i IS glWaitLocks.size()) glWaitLocks.push_back(our_thread);
      else glWaitLocks[glWLIndex].setThread(our_thread);
   }
   else { // Check for deadlocks using modern detector
      if (glDeadlockDetector.would_deadlock(our_thread, OtherThreadID)) {
         pf::Log log(__FUNCTION__);
         log.warning("Deadlock: Thread %d would create circular wait with thread %d for resource #%d.", int(our_thread), int(OtherThreadID), ResourceID);
         return ERR::DeadLock;
      }
   }

   glWaitLocks[glWLIndex].WaitingForResourceID   = ResourceID;
   glWaitLocks[glWLIndex].WaitingForResourceType = ResourceType;
   glWaitLocks[glWLIndex].WaitingForThreadID     = OtherThreadID;
   #ifdef _WIN32
   glWaitLocks[glWLIndex].Lock = get_threadlock();
   #endif

   // Add to modern deadlock detector
   glDeadlockDetector.add_wait(our_thread, OtherThreadID);

   return ERR::Okay;
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

   // Clear deadlock detector entries for this thread
   glDeadlockDetector.remove_wait(our_thread);

   for (unsigned i=0; i < glWaitLocks.size(); i++) {
      if (glWaitLocks[i].ThreadID IS our_thread) {
         glWaitLocks[i].notWaiting();
      }
      else if (glWaitLocks[i].WaitingForThreadID IS our_thread) { // A thread is waiting on us, wake it up.
         #ifdef _WIN32
            log.warning("Waking thread %d", int(glWaitLocks[i].ThreadID));
            glDeadlockDetector.remove_wait(glWaitLocks[i].ThreadID);
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
// Modern thread lock functions using RAII manager

WINHANDLE get_threadlock(void)
{
   return glThreadLockManager.get_thread_lock();
}

void free_threadlocks(void)
{
   glThreadLockManager.free_all_locks();
}

void free_threadlock(void)
{
   // Thread-local cleanup is now handled automatically by the manager
   // Individual thread lock cleanup happens when the thread terminates
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
mem Memory:       The ID of the memory block to access.
int(MEM) Flags:   Set to `READ`, `WRITE` or `READ_WRITE`.
int MilliSeconds: The millisecond interval to wait before a timeout occurs.  Use at least 40ms for best results.
&ptr Result:      Must refer to an `APTR` for storing the resolved address.

-ERRORS-
Okay
Args: The `MilliSeconds` value is less or equal to zero.
NullArgs
SystemLocked
TimeOut
MemoryDoesNotExist: The supplied `Memory` ID does not refer to an existing memory block.
-END-

*********************************************************************************************************************/

ERR AccessMemory(MEMORYID MemoryID, MEM Flags, int MilliSeconds, APTR *Result)
{
   pf::Log log(__FUNCTION__);

   if ((!MemoryID) or (!Result)) return log.warning(ERR::NullArgs);
   if (MilliSeconds <= 0) return log.warning(ERR::Args);

   // NB: Logging AccessMemory() calls is usually a waste of time unless the process is going to sleep.
   //log.trace("MemoryID: %d, Flags: $%x, TimeOut: %d", MemoryID, int(Flags), MilliSeconds);

   *Result = nullptr;
   if (auto lock = std::unique_lock{glmMemory}) {
      auto mem = glPrivateMemory.find(MemoryID);
      if ((mem != glPrivateMemory.end()) and (mem->second.Address)) {
         auto our_thread = get_thread_id();

         // This loop condition verifies that the block is available and protects against recursion.
         // wait_for() is awoken with a global wake-up, not necessarily on the desired block, hence the need for while().

         auto end_time = steady_clock::now() + milliseconds(MilliSeconds);
         while ((mem->second.AccessCount > 0) and (mem->second.ThreadLockID != our_thread)) {
            auto now = steady_clock::now();
            if (now >= end_time) return log.warning(ERR::TimeOut);

            auto timeout_remaining = end_time - now;
            //log.msg("Sleep on memory #%d, Access %d, Threads %d/%d", MemoryID, mem->second.AccessCount, (int)mem->second.ThreadLockID, our_thread);
            if (cvResources.wait_for(glmMemory, timeout_remaining) IS std::cv_status::timeout) {
               return log.warning(ERR::TimeOut);
            }
         }

         mem->second.ThreadLockID = our_thread;
         mem->second.AccessCount++;
         tlPrivateLockCount++;

         *Result = mem->second.Address;
         return ERR::Okay;
      }
      else log.traceWarning("Cannot find memory ID #%d", MemoryID); // This is not uncommon, so trace only
   }
   else return log.warning(ERR::SystemLocked);

   return ERR::MemoryDoesNotExist;
}

/*********************************************************************************************************************

-FUNCTION-
AccessObject: Grants exclusive access to objects via unique ID.
Category: Objects

This function resolves an object ID to its address and acquires a lock on the object so that other threads cannot use
it simultaneously.

If the `Object` is already locked, the function will wait until it becomes available.   This must occur within the amount
of time specified in the `Milliseconds` parameter.  If the time expires, the function will return with an `ERR::TimeOut`
error code.  If successful, `ERR::Okay` is returned and a reference to the object's address is stored in the `Result`
variable.

It is crucial that calls to AccessObject() are followed with a call to ~ReleaseObject() once the lock is no
longer required.  Calls to AccessObject() will also nest, so they must be paired with ~ReleaseObject()
correctly.

It is recommended that C++ developers use the `ScopedObjectLock` class to acquire object locks rather than making
direct calls to AccessObject().  The following example illustrates lock acquisition within a 1 second time limit:

<pre>
{
   pf::ScopedObjectLock&lt;OBJECTPTR&gt; obj(my_object_id, 1000);
   if (lock.granted()) {
      obj.acDraw();
   }
}
</pre>


-INPUT-
oid Object: The unique ID of the target object.
int MilliSeconds: The limit in milliseconds before a timeout occurs.  The maximum limit is `60000`, and `100` is recommended.
&obj Result: A pointer storage variable that will store the resulting object address.

-ERRORS-
Okay
NullArgs
NoMatchingObject
TimeOut
SystemLocked
-END-

*********************************************************************************************************************/

ERR AccessObject(OBJECTID ObjectID, int MilliSeconds, OBJECTPTR *Result)
{
   pf::Log log(__FUNCTION__);

   if ((!Result) or (!ObjectID)) return log.warning(ERR::NullArgs);
   if (MilliSeconds <= 0) log.warning(ERR::Args); // Warn but do not fail

   if (auto lock = std::unique_lock{glmMemory}) {
      auto mem = glPrivateMemory.find(ObjectID);
      if ((mem != glPrivateMemory.end()) and (mem->second.Address)) {
         if (auto error = LockObject((OBJECTPTR)mem->second.Address, MilliSeconds); error IS ERR::Okay) {
            *Result = (OBJECTPTR)mem->second.Address;
            return ERR::Okay;
         }
         else return error;
      }
      else if (ObjectID IS glMetaClass.UID) { // Access to the MetaClass requires this special case handler.
         if (auto error = LockObject(&glMetaClass, MilliSeconds); error IS ERR::Okay) {
            *Result = &glMetaClass;
            return ERR::Okay;
         }
         else return error;
      }
      else return ERR::NoMatchingObject;
   }
   else return log.warning(ERR::SystemLocked);
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
int MilliSeconds: The total number of milliseconds to wait before giving up.  If `-1`, the function will wait indefinitely.

-ERRORS-
Okay:
NullArgs:
MarkedForDeletion:
SystemLocked:
TimeOut:
-END-

*********************************************************************************************************************/

ERR LockObject(OBJECTPTR Object, int Timeout)
{
   if (!Object) {
      DEBUG_BREAK
      return ERR::NullArgs;
   }

   auto our_thread = get_thread_id();

   // Using an atomic increment we can achieve a 'quick lock' of the object without having to resort to locks.
   // This is quite safe so long as the developer is being careful with use of the object between threads (i.e. not
   // destroying the object when other threads could potentially be using it).

   // Use proper atomic compare-and-swap for thread-safe lock acquisition

   char expected = 0;
   if (Object->Queue.compare_exchange_weak(expected, 1, std::memory_order_acquire, std::memory_order_relaxed)) {
      Object->ThreadID = int(our_thread);
      return ERR::Okay;
   }

   // Support nested locks - check if we already own the lock

   if (int(our_thread) IS Object->ThreadID) {
      Object->Queue.fetch_add(1, std::memory_order_relaxed);
      return ERR::Okay;
   }

   if (Object->defined(NF::FREE|NF::FREE_ON_UNLOCK)) return ERR::MarkedForDeletion; // If the object is currently being removed by another thread, sleeping on it is pointless.

   // Problem: What if ReleaseObject() in another thread were to release the object prior to our glmObjectLocking lock?  This means that we would never receive the wake signal.
   // Solution: Prior to wait_until(), increment the object queue to attempt a lock.  This is *slightly* less efficient than doing it after the cond_wait(), but
   //           it will prevent us from sleeping on a signal that we would never receive.

   steady_clock::time_point end_time;
   if (Timeout < 0) end_time = steady_clock::time_point::max();
   else end_time = steady_clock::now() + milliseconds(Timeout);

   Object->SleepQueue.fetch_add(1, std::memory_order_relaxed); // Increment the sleep queue first so that ReleaseObject() will know that another thread is expecting a wake-up.

   if (auto lock = std::unique_lock{glmObjectLocking, std::chrono::milliseconds(Timeout)}) {
      pf::Log log(__FUNCTION__);

      //log.function("TID: %d, Sleeping on #%d, Timeout: %d, Queue: %d, Locked By: %d", our_thread, Object->UID, Timeout, Object->Queue, Object->ThreadID);

      ERR error = ERR::TimeOut;
      if (init_sleep(THREADID(Object->ThreadID), Object->UID, RT_OBJECT) IS ERR::Okay) { // Indicate that our thread is sleeping.
         while (steady_clock::now() < end_time) {
            if (glWaitLocks[glWLIndex].Flags & WLF_REMOVED) {
               glWaitLocks[glWLIndex].notWaiting();
               glDeadlockDetector.remove_wait(our_thread);
               Object->SleepQueue.fetch_sub(1, std::memory_order_release);
               return ERR::DoesNotExist;
            }

            // Use proper atomic compare-and-swap for lock acquisition
            char expected = 0;
            if (Object->Queue.compare_exchange_weak(expected, 1, std::memory_order_acquire, std::memory_order_relaxed)) {
               glWaitLocks[glWLIndex].notWaiting();
               glDeadlockDetector.remove_wait(our_thread);
               Object->ThreadID = int(our_thread);
               Object->SleepQueue.fetch_sub(1, std::memory_order_release);
               return ERR::Okay;
            }

            auto timeout_remaining = end_time - steady_clock::now();
            if (timeout_remaining <= milliseconds(0)) break;

            if (cvObjects.wait_for(glmObjectLocking, timeout_remaining) IS std::cv_status::timeout) break;
         }

         // Failure: Either a timeout occurred or the object no longer exists.

         if (glWaitLocks[glWLIndex].Flags & WLF_REMOVED) {
            log.warning("TID %d: The resource no longer exists.", int(get_thread_id()));
            error = ERR::DoesNotExist;
         }
         else {
            log.traceWarning("TID: %d, #%d, Timeout occurred.", our_thread, Object->UID);
            error = ERR::TimeOut;
         }

         glWaitLocks[glWLIndex].notWaiting();
         glDeadlockDetector.remove_wait(our_thread);
      }
      else error = log.error(ERR::LockFailed);

      Object->SleepQueue.fetch_sub(1, std::memory_order_release);
      return error;
   }
   else {
      Object->SleepQueue.fetch_sub(1, std::memory_order_release);
      return ERR::SystemLocked;
   }
}

/*********************************************************************************************************************

-FUNCTION-
ReleaseMemory: Releases a lock from a memory based resource.
Category: Memory

Successful calls to ~AccessMemory() must be paired with a call to ReleaseMemory() so that the memory can be made
available to other processes.  Releasing the resource decreases the access count, and if applicable a
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

ERR ReleaseMemory(MEMORYID MemoryID)
{
   pf::Log log(__FUNCTION__);

   if (!MemoryID) return log.warning(ERR::NullArgs);

   std::lock_guard lock(glmMemory);
   auto mem = glPrivateMemory.find(MemoryID);

   if ((mem IS glPrivateMemory.end()) or (!mem->second.Address)) { // Sanity check; this should never happen
      if (tlContext->object()->Class) log.warning("Unable to find a record for memory address #%d [Context %d, Class %s].", MemoryID, tlContext->object()->UID, tlContext->object()->className());
      else log.warning("Unable to find a record for memory #%d.", MemoryID);
      return ERR::Search;
   }

   int16_t access;
   if (mem->second.AccessCount > 0) { // Sometimes ReleaseMemory() is called on addresses that aren't actually locked.  This is OK - we simply don't do anything in that case.
      access = --mem->second.AccessCount;
      tlPrivateLockCount--;
   }
   else access = -1;

   if (!access) {
      mem->second.ThreadLockID = THREADID(0);

      if ((mem->second.Flags & MEM::COLLECT) != MEM::NIL) {
         log.trace("Collecting memory block #%d", MemoryID);
         FreeResource(mem->second.Address);
      }
      else if ((mem->second.Flags & MEM::EXCLUSIVE) != MEM::NIL) {
         mem->second.Flags &= ~MEM::EXCLUSIVE;
      }

      cvResources.notify_all();
   }

   return ERR::Okay;
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

   if (Object->Queue.fetch_sub(1, std::memory_order_release) > 1) return;

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

         cvObjects.notify_all(); // Multiple threads may be waiting on this object
      }
   }
   else if (Object->defined(NF::FREE_ON_UNLOCK) and (!Object->defined(NF::FREE))) {
      Object->Flags &= ~NF::FREE_ON_UNLOCK;
      FreeResource(Object);
   }
}
