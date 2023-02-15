/*****************************************************************************

The source code of the Parasol project is made publicly available under the
terms described in the LICENSE.TXT file that is distributed with this package.
Please refer to it for further information on licensing.

******************************************************************************

This program tests the locking of private objects between threads.

*****************************************************************************/

#include <pthread.h>

#include <parasol/startup.h>

STRING ProgName      = "ObjectLocking";
extern struct CoreBase *CoreBase;
static volatile OBJECTPTR glConfig = NULL;
static LONG glTotalThreads = 8;
static LONG glLockAttempts = 200;
static BYTE glTerminateObject = FALSE;
static LONG glAccessGap = 200000;

struct thread_info{
   pthread_t thread;
   int index;
};

#define QUICKLOCK

#ifdef QUICKLOCK
#define INC_QUEUE(Object) __sync_add_and_fetch(&(Object)->Queue, 1)
#define SUB_QUEUE(Object) __sync_sub_and_fetch(&(Object)->Queue, 1)
#define INC_SLEEP(Object) __sync_add_and_fetch(&(Object)->SleepQueue, 1)
#define SUB_SLEEP(Object) __sync_sub_and_fetch(&(Object)->SleepQueue, 1)

INLINE ERROR prv_access(OBJECTPTR Object, LONG ThreadID)
{
   if (INC_QUEUE(Object) IS 1) {
      Object->ThreadID = ThreadID;
      return ERR_Okay;
   }
   else {
      if (Object->ThreadID IS ThreadID) return ERR_Okay;
      SUB_QUEUE(Object); // Put the lock count back to normal before LockObject()
      return LockObject(Object, -1);
   }
}

INLINE void prv_release(OBJECTPTR Object)
{
   if (Object->SleepQueue > 0) ReleaseObject(Object);
   else SUB_QUEUE(Object);
}
#endif

/*****************************************************************************
** Internal: thread_entry()
*/

static void * thread_entry(struct thread_info *info)
{
   parasol::Log log(__FUNCTION__);
   LONG i;
   ERROR error;

   info->index = GetResource(RES_THREAD_ID);
   log.msg("----- Thread %d is starting now.", info->index);

   for (i=0; i < glLockAttempts; i++) {
      if (!glConfig) break;
      //LogF("~","Attempt %d.%d: Acquiring the object.", info->index, i);
      #ifdef QUICKLOCK
      if (!(error = prv_access(glConfig, info->index))) {
      #else
      if (!(error = LockObject(glConfig, 30000))) {
      #endif
         glConfig->ActionDepth++;
         log.msg("%d.%d: Object acquired.", info->index, i);
         WaitTime(0, 2000);
         if (glConfig->ActionDepth > 1) log.warning("--- MAJOR ERROR: More than one thread has access to this object!");
         glConfig->ActionDepth--;

         // Test that object removal works in ReleaseObject() and that waiting threads fail peacefully.

         if (glTerminateObject) {
            if (i >= glLockAttempts-2) {
               acFree(glConfig);
               #ifdef QUICKLOCK
               prv_release(glConfig);
               #else
               ReleaseObject(glConfig);
               #endif
               glConfig = NULL;
               //LogReturn();
               break;
            }
         }

         #ifdef QUICKLOCK
         prv_release(glConfig);
         #else
         ReleaseObject(glConfig);
         #endif

         #ifdef __unix__
            pthread_yield();
         #endif
         if (glAccessGap > 0) WaitTime(0, glAccessGap);
      }
      else log.msg("Attempt %d.%d: Failed to acquire a lock, error: %s", info->index, i, GetErrorMsg(error));
      //LogReturn();
   }

   log.msg("----- Thread %d is finished.", info->index);
   return NULL;
}

/*****************************************************************************
** Main.
*/

void program(void)
{
   LONG i;
   STRING *args;

   if ((CurrentTask()->getPtr(FID_Parameters, &args) IS ERR_Okay) and (args)) {
      for (i=0; args[i]; i++) {
         if (!StrMatch(args[i], "-threads")) {
            if (args[++i]) glTotalThreads = StrToInt(args[i]);
            else break;
         }
         else if (!StrMatch(args[i], "-attempts")) {
            if (args[++i]) glLockAttempts = StrToInt(args[i]);
            else break;
         }
         else if (!StrMatch(args[i], "-gap")) {
            if (args[++i]) glAccessGap = StrToInt(args[i]);
            else break;
         }
         else if (!StrMatch(args[i], "-terminate")) glTerminateObject = TRUE;
      }
   }

   glConfig = objConfig::create::global();

   #ifdef QUICKLOCK
      log.msg("Quick-locking will be tested.");
   #endif

   log.msg("Spawning %d threads...", glTotalThreads);

   struct thread_info glThreads[glTotalThreads];

   for (i=0; i < glTotalThreads; i++) {
      glThreads[i].index = i;
      pthread_create(&glThreads[i].thread, NULL, (void *)&thread_entry, &glThreads[i]);
   }

   // Main block now waits for both threads to terminate, before it exits
   // If main block exits, both threads exit, even if the threads have not
   // finished their work

   log.msg("Waiting for thread completion.");

   for (i=0; i < glTotalThreads; i++) {
      pthread_join(glThreads[i].thread, NULL);
   }

   acFree(glConfig);

   print("Testing complete.\n");
}
