/*********************************************************************************************************************
-CATEGORY-
Name: Locks/Semaphores
-END-
*********************************************************************************************************************/

// ***
// To reduce system complexity, semaphores are not available for client use.  Mutex functions are public and available
// in lib_locking.c
// ***

#ifdef __unix__
   #include <sys/time.h>
   #ifndef __ANDROID__
      #include <sys/sem.h>
   #endif
   #include <sys/stat.h>
   #include <fcntl.h>
   #include <string.h>
   #include <errno.h>
   #include <signal.h>
   #include <time.h>
#endif

#include "defs.h"

using namespace pf;

//#define DBG_SEMAPHORES TRUE

#ifdef __APPLE__

ERROR plAllocPrivateSemaphore(APTR Semaphore, LONG InitialValue)
{
   static int num = 0;
   char name[32] = "parasol000000000000";
   num++;
   IntToStr(num, name+7, sizeof(name)-7);
   sem_t *sem = sem_open(name, O_CREAT|O_EXCL, InitialValue);
   if (sem IS SEM_FAILED) return ERR_SystemCall;
   ((sem_t **)Semaphore)[0] = sem;
   return ERR_Okay;
}

void plFreePrivateSemaphore(APTR Semaphore)
{
   sem_close((sem_t *)Semaphore);
}

ERROR plLockSemaphore(APTR Semaphore, LONG TimeOut)
{
   if (!sem_wait((sem_t *)Semaphore)) {
      return ERR_Okay;
   }
   else if (errno IS EINVAL) return ERR_DoesNotExist;
   else if (errno IS EINTR) return ERR_TimeOut;
   else if (errno IS EDEADLK) return ERR_DeadLock;
   else return ERR_Failed;
}

void plUnlockSemaphore(APTR Semaphore)
{
   sem_post((sem_t *)Semaphore);
}

#elif __unix__

ERROR plAllocPrivateSemaphore(APTR Semaphore, LONG InitialValue)
{
   if (sem_init((sem_t *)Semaphore, 0, InitialValue) IS -1) {
      return ERR_SystemCall;
   }
   else return ERR_Okay;
}

void plFreePrivateSemaphore(APTR Semaphore)
{
   sem_destroy((sem_t *)Semaphore);
}

ERROR plLockSemaphore(APTR Semaphore, LONG TimeOut)
{
   if (!sem_wait((sem_t *)Semaphore)) {
      return ERR_Okay;
   }
   else if (errno IS EINVAL) return ERR_DoesNotExist;
   else if (errno IS EINTR) return ERR_TimeOut;
   else if (errno IS EDEADLK) return ERR_DeadLock;
   else return ERR_Failed;
}

void plUnlockSemaphore(APTR Semaphore)
{
   sem_post((sem_t *)Semaphore);
}

#endif

/*********************************************************************************************************************
** Called by the close / crash recovery process to remove our semaphores.
*/

void remove_semaphores(void)
{
   pf::Log log(__FUNCTION__);

   log.debug("Removing semaphores.");

   if ((!glSharedControl) or (!glSharedControl->SemaphoreOffset)) return;

   ScopedSysLock lock(PL_SEMAPHORES, 4000);
   if (lock.granted()) {
      auto semlist = (SemaphoreEntry *)ResolveAddress(glSharedControl, glSharedControl->SemaphoreOffset);

      for (WORD index=1; index < MAX_SEMAPHORES; index++) {
         if (semlist[index].InstanceID IS glInstanceID) {
            for (WORD j=0; j < ARRAYSIZE(semlist[index].Processes); j++) {
               if (semlist[index].Processes[j].ProcessID IS glProcessID) {
                  #ifdef DBG_SEMAPHORES
                     log.msg("Deallocating semaphore #%d.", index);
                  #endif

                  if (semlist[index].Processes[j].AccessCount) semlist[index].Counter++;
                  if (semlist[index].Processes[j].BlockCount)  semlist[index].Counter += semlist[index].BlockingValue;

                  // Remove this process from the semaphore registrar

                  ClearMemory(semlist[index].Processes + j, sizeof(semlist[index].Processes[j]));
                  break;
               }
            }
         }
      }
   }
}

//********************************************************************************************************************

static LONG DeadSemaphoreProcesses(SemaphoreEntry *Semaphore)
{
   pf::Log log(__FUNCTION__);

   bool dead = false;
   for (LONG i=0; i < ARRAYSIZE(Semaphore->Processes); i++) {
      if (Semaphore->Processes[i].ProcessID) {
         // Check to see if the process exists.  If it doesn't, remove it from the list

         bool exists = true;
         #ifdef __unix__
            if ((kill(Semaphore->Processes[i].ProcessID, 0) IS -1) and (errno IS ESRCH)) {
               exists = false;
            }
         #elif _WIN32
            if (winCheckProcessExists(Semaphore->Processes[i].ProcessID) IS false) {
               exists = false;
            }
         #else
            #error Platform requires process checking.
         #endif

         if (!exists) {
            log.warning("Dead process #%d found at %d - cleaning up...", Semaphore->Processes[i].ProcessID, i);
            if (Semaphore->Processes[i].AccessCount) Semaphore->Counter++;
            if (Semaphore->Processes[i].BlockCount)  Semaphore->Counter += Semaphore->BlockingValue;

            // Remove this process from the semaphore registrar

            ClearMemory(Semaphore->Processes+i, sizeof(Semaphore->Processes[i]));
            dead = true;
         }
      }
   }

   return dead;
}

/*********************************************************************************************************************

-FUNCTION-
AccessSemaphore: Grants access to semaphores.
Status: Private

The AccessSemaphore() function can be used to grant an exclusive lock on a semaphore, or adjust a semaphore's internal
counter.  If the semaphore is available immediately then the function will return without sleeping.  If the semaphore
is blocked, the process will be put to sleep for a time not exceeding the MilliSeconds value.  If MilliSeconds is zero,
the function will return immediately with an ERR_Timeout error.

Each time AccessSemaphore() is called, the system will try to 'block' the semaphore by default.  This means that the
caller can acquire exclusive access to the resource represented by the semaphore.  If exclusivity is not required, set
the SMF_NON_BLOCKING flag.  In this mode, the system will decrement the semaphore counter by 1 and return.  The
semaphore will only block on future access attempts if its counter has reached a value of zero.

Successful calls to AccessSemaphore() will nest and must be matched with a call to ~ReleaseSemaphore().

-INPUT-
int Semaphore: The ID of an allocated semaphore.
int MilliSeconds: The total number of milliseconds to wait before timing out.
int(SMF) Flags: Optional flags.

-END-

*********************************************************************************************************************/

ERROR AccessSemaphore(LONG SemaphoreID, LONG Timeout, LONG Flags)
{
   pf::Log log(__FUNCTION__);

   if ((SemaphoreID < 1) or (SemaphoreID > MAX_SEMAPHORES)) return log.warning(ERR_Args);

   LARGE end_time = (PreciseTime()/1000LL) + Timeout;

   ScopedSysLock lock(PL_SEMAPHORES, Timeout);
   if (lock.granted()) {
      auto semlist = (SemaphoreEntry *)ResolveAddress(glSharedControl, glSharedControl->SemaphoreOffset);
      auto semaphore = semlist + SemaphoreID;

      // Each semaphore has a list of processes that have currently gained or are waiting to access to it.  Search for a process entry that we can
      // use, or perhaps use an existing entry if we already have access.

      WORD pi;
      for (pi=0; (pi < ARRAYSIZE(semaphore->Processes)) and (semaphore->Processes[pi].ProcessID != glProcessID); pi++);
      if (pi >= ARRAYSIZE(semaphore->Processes)) return ERR_Okay;

      SemaphoreEntry::SemProcess *process = &semaphore->Processes[pi];

      #ifdef DBG_SEMAPHORES
         if (Flags & SMF_NON_BLOCKING) log.function("ID: %d, Non-Blocking, Counter: %d/%d, Internal: %d:%d,%d", SemaphoreID, semaphore->Counter, semaphore->MaxValue, process->AccessCount, process->BufferCount, process->BlockCount);
         else log.function("ID: %d, Blocking, Counter: %d/%d, Internal: %d:%d,%d", SemaphoreID, semaphore->Counter, semaphore->MaxValue, process->AccessCount, process->BufferCount, process->BlockCount);
      #endif

      if (semaphore->MaxValue <= 0) {
         log.warning("Semaphore #%d has a bad maxvalue of %d, we cannot lock it.", SemaphoreID, semaphore->MaxValue);
         return ERR_SystemCorrupt;
      }

      while (semaphore->Counter < semaphore->MaxValue) {
         // The semaphore is blocked or in use, so we may need to wait for it to become available

         if (process->BlockCount) {
            // According to our process entry we've already acquired blocking access on this semaphore
            break;
         }

         if (process->AccessCount) { // We've already acquired read-access to this semaphore
            if (Flags & SMF_NON_BLOCKING) break;
            else { // If we want blocking access and have already acquired read access, check if we are the only task using the semaphore.
               if ((semaphore->MaxValue - semaphore->Counter) IS 1) break;
            }
         }

         if ((Flags & SMF_NON_BLOCKING) and (semaphore->Counter > 0)) {
            // We have requested a non-blocking lock and the counter will allow us to grab it
            break;
         }

         // Return immediately if there is no timeout left

         if ((PreciseTime() / 1000LL) >= end_time) {
            lock.release();
            log.warning("Timeout occurred in attempting to access semaphore #%d.", SemaphoreID);
            if (glLogLevel > 2) print_diagnosis(glProcessID, 0);
            DeadSemaphoreProcesses(semaphore); // Check for dead processes
            return ERR_TimeOut;
         }

         // We have to go to sleep and wait for the semaphore to become available.  Start by putting our process on the queue.

         #ifdef DBG_SEMAPHORES
            if (semaphore->BlockingProcess) log.function("Sleeping on blocking process %d, time-out %d...", Timeout);
            else log.function("Going to sleep, time-out %d...", Timeout);
         #endif

         // Set ourselves up to wait on this semaphore

         #ifdef _WIN32
            WORD wl;
            if (init_sleep(semaphore->BlockingProcess, semaphore->BlockingThread, SemaphoreID, RT_SEMAPHORE, &wl) != ERR_Okay) {
               return ERR_DeadLock;
            }

            LONG sleep_timeout = end_time - (PreciseTime()/1000LL);
            if (sleep_timeout <= 0) {
               log.warning("Time-out of %dms on semaphore #%d locked by process %d.", Timeout, SemaphoreID, semaphore->BlockingProcess);
               clear_waitlock(wl);
               return ERR_TimeOut;
            }

            lock.release();

            #ifdef USE_GLOBAL_EVENTS
               sleep_waitlock(glPublicLocks[CN_SEMAPHORES].Lock, sleep_timeout);  // Go to sleep and wait for a wakeup or time-out.
            #else
               sleep_waitlock(get_threadlock(), sleep_timeout);  // Go to sleep and wait for a wakeup or time-out.
            #endif

            clear_waitlock(wl);

            // We have been woken or a time-out has occurred.  Restart the process.

            LONG relock_timeout = end_time - (PreciseTime()/1000LL);
            if (relock_timeout < 1) relock_timeout = 1;
            if (lock.acquire(relock_timeout) != ERR_Okay) {
               return log.warning(ERR_SystemLocked);
            }

         #else

            ERROR error;
            LONG timeout = end_time - (PreciseTime()/1000LL);
            WORD wl;
            if (timeout > 0) {
               if (!(error = init_sleep(semaphore->BlockingProcess, semaphore->BlockingThread, SemaphoreID, RT_SEMAPHORE, &wl))) { // For deadlocking prevention.
                  error = public_cond_wait(&glSharedControl->PublicLocks[PL_SEMAPHORES].Mutex, &glSharedControl->PublicLocks[PL_SEMAPHORES].Cond, timeout);
                  clear_waitlock(wl);
               }
            }
            else error = ERR_TimeOut;

            if (error) return log.warning(error);

         #endif
      } // while()

      // If we get out of the loop to get to this part of the routine, then it is safe for us to complete a lock on this semaphore.

      if (Flags & SMF_NON_BLOCKING) {
         // For non-blocking locks, decrement the counter by 1 and record our lock internally.

         if ((process->BufferCount) or (process->BlockCount)) process->BufferCount++;
         else {
            if (!process->AccessCount) {
               if (semaphore->Counter <= 0) log.warning("Semaphore counter is already at %d!", semaphore->Counter);
               semaphore->Counter--;
            }
            process->AccessCount++;
         }

         return ERR_Okay;
      }
      else {
         // For blocking locks, reduce the counter to zero and record our lock internally.

         if (process->BlockCount <= 0) {
            if (semaphore->Counter <= 0) {
               log.warning("Cannot get block-access - semaphore counter is at zero and sleeping is disabled.");
               return ERR_SystemCorrupt;
            }

            semaphore->BlockingValue = semaphore->Counter;
            semaphore->BlockingProcess = glProcessID;
         }
         process->BlockCount++;
         semaphore->Counter = 0;
         return ERR_Okay;
      }
   }
   else return log.warning(ERR_Lock);
}

/*********************************************************************************************************************

-FUNCTION-
AllocSemaphore: Allocates a new public semaphore.
Status: Private

The AllocSemaphore() function is used to create or discover semaphores.  To share a semaphore with other processes,
a name string can be assigned to the semaphore to simplify identification.  The Value argument assigns a counter to the
semaphore - this feature should only be used if a program requires complex signal management.

Semaphores are most commonly used to control access to shared resources, typically in shared memory areas.  For
instance, if you create a shared memory area for read/write operations between tasks, you need a control system to
prevent the tasks from writing to the memory at the same time.  Using a semaphore is appropriate for controlling this
situation.

For a simple blocking semaphore, set a value of 1 in the Value parameter.  To allow multiple processes to read from
the resource, set the Value higher - 100 or more.  In this mode, there are two ways for processes to access the
semaphore - blocking and non-blocking mode.  Access is achieved through the ~AccessSemaphore() function.  Blocking
mode is the default and will grant full access to the resource if it succeeds.  Non-blocking access grants limited
access to the resource - typically considered 'read only' access. Multiple processes can have non-blocking access at
the same time, but only one process may have access when blocking mode is required.  As an example, if Value is 100
then the number of non-blocking accesses will be limited to 100 processes.  Any more processes than this wishing to
use the semaphore will need to wait until some of the locks are released.  If 50 processes currently have read access
and a new process requires blocking access, it will have to wait until all 50 read accesses are released.  The
specifics of this are discussed in the documentation for ~AccessSemaphore() and ~ReleaseSemaphore().

The handle returned in the Semaphore argument is global, so if you want to secretly share an anonymous semaphore with
other processes, you may do so if you pass the handle to them.  The other processes will still need to call
AllocSemaphore(), setting the `SMF_EXISTS` flag and also passing the semaphore handle in the Semaphore parameter.

To free a semaphore after allocating it, call ~FreeSemaphore().  AllocSemaphore() will nest if it
is called multiple times with the same Name.  Each call must be matched with a ~FreeSemaphore() call.  A
semaphore will not be completely freed from the system until all processes that have access drop their control of the
semaphore.

-INPUT-
cstr Name: Optional.  The name of the semaphore (up to 15 characters, CASE SENSITIVE) to create or find.
int Value: The starting value of the semaphore - this indicates the number of locks that can be made before the semaphore blocks.  The minimum starting value is 1.
int(SMF) Flags: Optional flags, currently SMF_EXISTS is supported.
&int Semaphore: A reference to the semaphore handle will be returned through this pointer.

-END-

*********************************************************************************************************************/

#define KEY_SEMAPHORE 0x125af902

ERROR AllocSemaphore(CSTRING Name, LONG Value, LONG Flags, LONG *SemaphoreID)
{
   pf::Log log(__FUNCTION__);
   SemaphoreEntry *semaphore;
   LONG index;

   if (!SemaphoreID) return log.warning(ERR_NullArgs);

   if (Value <= 0) Value = 1;
   else if (Value > 255) Value = 255;

   if (Flags & SMF_EXISTS) index = *SemaphoreID;
   else {
      *SemaphoreID = 0;
      index = 0;
   }

   ScopedSysLock lock(PL_SEMAPHORES, 4000);
   if (lock.granted()) {
      auto semlist = (SemaphoreEntry *)ResolveAddress(glSharedControl, glSharedControl->SemaphoreOffset);

      // If a name is given, look for the existing semaphore.  Otherwise, find an empty space in the semaphore list.

      if (!index) {
         if ((Name) and (*Name)) {
            ULONG name_id = StrHash(Name, TRUE);
            for (index=1; index < MAX_SEMAPHORES; index++) {
               if ((semlist[index].InstanceID IS glInstanceID) and (semlist[index].NameID IS name_id)) break;
            }

            if (index IS MAX_SEMAPHORES) {
               for (index=1; (index < MAX_SEMAPHORES) and (semlist[index].MaxValue); index++);
            }
         }
         else for (index=1; (index < MAX_SEMAPHORES) and (semlist[index].MaxValue); index++);
      }

      if ((index < 1) or (index >= MAX_SEMAPHORES)) {
         log.warning("All of the available semaphore slots are in use.");
         return ERR_ArrayFull;
      }

      semaphore = semlist + index;

      // Find an empty slot for process registration against this semaphore.  The process slots are used for the
      // purposes of basic resource tracking.

      LONG pi;
      for (pi=0; pi < ARRAYSIZE(semaphore->Processes); pi++) {
         if (semaphore->Processes[pi].ProcessID IS glProcessID) {
            semaphore->Processes[pi].ProcessID = glProcessID;
            break;
         }
      }

      if (pi >= ARRAYSIZE(semaphore->Processes)) {
restart:
         for (pi=0; pi < ARRAYSIZE(semaphore->Processes); pi++) {
            if (!semaphore->Processes[pi].ProcessID) {
               semaphore->Processes[pi].ProcessID = glProcessID;
               break;
            }
         }

         if (pi >= ARRAYSIZE(semaphore->Processes)) {
            // Check if any of the processes are dead
            if (DeadSemaphoreProcesses(semaphore) IS TRUE) {
               goto restart;
            }
            else {
               log.warning("All process slots for semaphore #%d are in use.", *SemaphoreID);
               return ERR_ArrayFull;
            }
         }
      }

      // Record the details for the semaphore if we created it

      if (!semaphore->MaxValue) {
         semaphore->MaxValue   = Value;
         semaphore->InstanceID = glInstanceID;
         semaphore->Flags      = Flags & MEM_UNTRACKED;
         semaphore->Counter    = Value;
         semaphore->Data       = 0;
         if (Name) semaphore->NameID = StrHash(Name, TRUE);
      }

      // Record locking information for our process

      semaphore->Processes[pi].AllocCount++;

      log.function("Name: %s, Value: %d, Flags: $%.8x, ID: %d", Name, Value, Flags, index);

      *SemaphoreID = index; // Result
      return ERR_Okay;
   }
   else return log.warning(ERR_Lock);
}

/*********************************************************************************************************************

-FUNCTION-
FreeSemaphore: Frees an allocated semaphore.
Status: Private

The FreeSemaphore() function is used to deallocate semaphores when they are no longer required.  If active locks are
present on the target semaphore, it will be marked for deletion and is not removed until those locks are released.
Semaphores that are marked for deletion are not otherwise restricted in their capabilities.

-INPUT-
Semaphore: The handle of the semaphore that you want to deallocate.

-END-

*********************************************************************************************************************/

ERROR FreeSemaphore(LONG SemaphoreID)
{
   pf::Log log(__FUNCTION__);

   if (SemaphoreID <= 0) return log.warning(ERR_Args);

   ScopedSysLock lock(PL_SEMAPHORES, 4000);
   if (lock.granted()) {
      auto semlist = (SemaphoreEntry *)ResolveAddress(glSharedControl, glSharedControl->SemaphoreOffset);
      auto semaphore = semlist + SemaphoreID;

      LONG pi;
      for (pi=0; (pi < ARRAYSIZE(semaphore->Processes)) and (semaphore->Processes[pi].ProcessID != glProcessID); pi++);
      if (pi >= ARRAYSIZE(semaphore->Processes)) return ERR_Okay;
      SemaphoreEntry::SemProcess *process = &semaphore->Processes[pi];

      // Drop the internal allocation counter.  Do not proceed if it is still > 0

      process->AllocCount--;
      if (process->AllocCount > 0) {
         log.function("ID: %d [Allocation Count: %d]", SemaphoreID, process->AllocCount);
         return ERR_Okay;
      }

      // If we still have locks on this block, we cannot free it yet. The ReleaseSemaphore() function will take care of this later, as it includes a check to test the AllocCount variable.

      if ((process->AccessCount > 0) or (process->BlockCount > 0)) {
         log.warning("ID: %d - Remaining Non-Blocking Locks: %d, Blocking Locks: %d", SemaphoreID, process->AccessCount, process->BlockCount);
         return ERR_Okay;
      }

      // Remove our process' registration from the global semaphore databaes

      ClearMemory(process, sizeof(SemaphoreEntry::SemProcess));

      // Check if there are no more tasks utilising the semaphore by scanning the locks.

      DeadSemaphoreProcesses(semaphore);

      for (LONG i=0; i < ARRAYSIZE(semaphore->Processes); i++) {
         if (semaphore->Processes[i].ProcessID) {
            // The semaphore is obviously in use, so do not remove it
            log.warning("ID: %d [Still in use by other processes]", SemaphoreID);
            return ERR_Okay;
         }
      }

      ClearMemory(semaphore, sizeof(SemaphoreEntry)); // Destroy the semaphore

      log.function("ID: %d", SemaphoreID);
      return ERR_Okay;
   }
   else return log.warning(ERR_Lock);
}

/*********************************************************************************************************************

-FUNCTION-
ReleaseSemaphore: Releases a locked semaphore.
Status: Private

Use the ReleaseSemaphore() function to release locks acquired by ~AccessSemaphore().  This function
must be passed the same Flags that were used in the matching call to ~AccessSemaphore().  Failure to do so
may affect the semaphore's behaviour.

This function returns immediately with ERR_Failed if there are no locks on the target semaphore.

-INPUT-
Semaphore: The semaphore handle that you want to release.
Flags:     Must be set to the flags originally passed to AccessSemaphore().

-END-

*********************************************************************************************************************/

ERROR pReleaseSemaphore(LONG SemaphoreID, LONG Flags)
{
   pf::Log log(__FUNCTION__);

   if ((SemaphoreID < 1) or (SemaphoreID > MAX_SEMAPHORES)) return log.warning(ERR_Args);

   ScopedSysLock lock(PL_SEMAPHORES, 4000);
   if (lock.granted()) {
      auto semaphores = (SemaphoreEntry *)ResolveAddress(glSharedControl, glSharedControl->SemaphoreOffset);
      auto semaphore = semaphores + SemaphoreID;

      WORD pi;
      for (pi=0; (pi < ARRAYSIZE(semaphore->Processes)) and (semaphore->Processes[pi].ProcessID != glProcessID); pi++);
      if (pi >= ARRAYSIZE(semaphore->Processes)) {
         return ERR_Okay;
      }

      SemaphoreEntry::SemProcess *process = &semaphore->Processes[pi];

      #ifdef DBG_SEMAPHORES
         if (Flags & SMF_NON_BLOCKING) log.function("ID: %d, Non-Blocking, Counter: %d/%d, Internal: %d:%d,%d", SemaphoreID, semaphore->Counter, semaphore->MaxValue, process->AccessCount, process->BufferCount, process->BlockCount);
         else log.function("ID: %d, Blocking, Counter: %d/%d, Internal: %d:%d,%d", SemaphoreID, semaphore->Counter, semaphore->MaxValue, process->AccessCount, process->BufferCount, process->BlockCount);
      #endif

      // Release the lock according to the type of lock it is

      UBYTE wake = FALSE;
      if (Flags & SMF_NON_BLOCKING) {
         if (process->BufferCount > 0) {
            process->BufferCount--;
            return ERR_Okay;
         }

         if (process->AccessCount < 1) {
            log.warning("This task does not have a non-blocking lock on semaphore #%d.", SemaphoreID);
            return ERR_Failed;
         }

         process->AccessCount--;

         if (!process->AccessCount) {
            semaphore->Counter++;
            wake = TRUE;
         }
      }
      else {
         if (process->BlockCount < 1) {
            log.warning("This task does not have a blocking lock on semaphore #%d.", SemaphoreID);
            return ERR_Failed;
         }

         process->BlockCount--;

         if (!process->BlockCount) {
            if (semaphore->BlockingValue <= 0) {
               log.warning("Bad blocking value %d.", semaphore->BlockingValue);
               semaphore->Counter = semaphore->MaxValue;
            }
            else semaphore->Counter += semaphore->BlockingValue;

            wake = TRUE;
         }
      }

      if (wake) {
         // We have changed the counter, so if anyone is waiting for access on the FIFO list, wake them up.

         #ifdef _WIN32
            wake_sleepers(SemaphoreID, RT_SEMAPHORE);
         #else
            wake_sleepers(SemaphoreID, RT_SEMAPHORE);
            pthread_cond_broadcast(&glSharedControl->PublicLocks[PL_SEMAPHORES].Cond);
         #endif
      }

      if (process->AllocCount <= 0) FreeSemaphore(SemaphoreID);

      return ERR_Okay;
   }
   else return log.warning(ERR_Lock);
}

/*********************************************************************************************************************

-FUNCTION-
SemaphoreCtrl: Manipulates semaphore details.
Status: Private

This function provides a generic means for controlling the details of a semaphore.  It executes a command, as listed in
the following table, against the handle referenced in the Semaphore parameter.

<types type="Command">
<type name="SEM_GET_VAL">Get the maximum value for the counter, as originally set in ~AllocSemaphore().  Associated tag must be a pointer to a 32-bit integer.</>
<type name="SEM_GET_COUNTER">Get the current counter value (affected by ~AccessSemaphore() and ~ReleaseSemaphore()).  Associated tag must be a pointer to a 32-bit integer.</>
<type name="SEM_GET_DATA_LARGE">Get the user customisable data value associated with the semaphore.  The subsequent tag must be a pointer to a 64-bit integer.</>
<type name="SEM_GET_DATA_LONG">Get the user customisable data value associated with the semaphore.  The subsequent tag must be a pointer to a 32-bit integer.</>
<type name="SEM_GET_DATA_PTR">Get the user customisable data value associated with the semaphore.  The subsequent tag must be a pointer to an address pointer.</>
<type name="SEM_SET_DATA_LARGE">Set the user customisable data value associated with the semaphore.  The subsequent tag must be a 64-bit integer.</>
<type name="SEM_SET_DATA_LONG">Set the user customisable data value associated with the semaphore.  The subsequent tag must be a 32-bit integer.</>
<type name="SEM_SET_DATA_PTR">Set the user customisable data value associated with the semaphore.  The subsequent tag must be an address pointer.</>
</>

It is not necessary to have locked the semaphore in order to execute any of the available commands.

-INPUT-
int Semaphore: The handle of the semaphore to be queried.
int Command: A command type to execute.
tags Tag: A tag value that is relevant to the Command.

-END-

*********************************************************************************************************************/

ERROR SemaphoreCtrl(LONG SemaphoreID, LONG Command, ...)
{
   pf::Log log(__FUNCTION__);
   va_list list;

   if ((SemaphoreID < 1) or (SemaphoreID > MAX_SEMAPHORES)) return log.warning(ERR_Args);

   ScopedSysLock lock(PL_SEMAPHORES, 4000);
   if (lock.granted()) {
      auto semaphores = (SemaphoreEntry *)ResolveAddress(glSharedControl, glSharedControl->SemaphoreOffset);
      auto semaphore = semaphores + SemaphoreID;
      va_start(list, Command);

      switch(Command) {
         case SEM_GET_VAL: {
            LONG *tag;
            tag = va_arg(list, LONG *);
            tag[0] = semaphore->MaxValue;
            break;
         }

         case SEM_GET_COUNTER: {
            LONG *tag;
            tag = va_arg(list, LONG *);
            tag[0] = semaphore->Counter;
            break;
         }

         // Get Data

         case SEM_GET_DATA_DOUBLE:
         case SEM_GET_DATA_LARGE: {
            LARGE *tag;
            tag = va_arg(list, LARGE *);
            tag[0] = semaphore->Data;
            break;
         }

         case SEM_GET_DATA_PTR: {
            APTR *tag;
            tag = va_arg(list, APTR *);
            tag[0] = (APTR)(MAXINT)(semaphore->Data);
            break;
         }

         case SEM_GET_DATA_LONG: {
            LONG *tag;
            tag = va_arg(list, LONG *);
            tag[0] = semaphore->Data;
            break;
         }

         // Set Data

         case SEM_SET_DATA_DOUBLE:
         case SEM_SET_DATA_LARGE: {
            semaphore->Data = va_arg(list, LARGE);
            break;
         }

         case SEM_SET_DATA_PTR: {
            semaphore->Data = (MAXINT)va_arg(list, APTR);
            break;
         }

         case SEM_SET_DATA_LONG: {
            semaphore->Data = va_arg(list, LONG);
            break;
         }

         default:
            va_end(list);
            return ERR_NoSupport;
      }

      va_end(list);
      return ERR_Okay;
   }
   else return log.warning(ERR_Lock);
}

