/*********************************************************************************************************************

The source code of the Parasol project is made publicly available under the terms described in the LICENSE.TXT file
that is distributed with this package.  Please refer to it for further information on licensing.

**********************************************************************************************************************

This program tests the locking of private objects between threads.

*********************************************************************************************************************/

#include <pthread.h>
#include <parasol/startup.h>
#include <parasol/vector.hpp>

CSTRING ProgName = "ObjectLocking";
extern struct CoreBase *CoreBase;
static volatile OBJECTPTR glConfig = NULL;
static ULONG glTotalThreads = 8;
static ULONG glLockAttempts = 200;
static bool glTerminateObject = false;
static LONG glAccessGap = 200000;

struct thread_info{
   pthread_t thread;
   int index;
};

#define QUICKLOCK

#ifdef QUICKLOCK
INLINE ERROR prv_access(OBJECTPTR Object, LONG ThreadID) {
   if (Object->incQueue() IS 1) {
      Object->ThreadID = ThreadID;
      return ERR_Okay;
   }
   else {
      if (Object->ThreadID IS ThreadID) return ERR_Okay;
      Object->subQueue(); // Put the lock count back to normal before LockObject()
      return LockObject(Object, -1);
   }
}

INLINE void prv_release(OBJECTPTR Object) {
   if (Object->SleepQueue > 0) ReleaseObject(Object);
   else Object->subQueue();
}
#endif

//********************************************************************************************************************

static void * thread_entry(void *Arg)
{
   pf::Log log(__FUNCTION__);
   ERROR error;

   auto info = (struct thread_info *)Arg;

   info->index = GetResource(RES::THREAD_ID);
   log.msg("----- Thread %d is starting now.", info->index);

   for (unsigned i=0; i < glLockAttempts; i++) {
      if (!glConfig) break;
      //log.branch("Attempt %d.%d: Acquiring the object.", info->index, i);
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
               FreeResource(glConfig);
               #ifdef QUICKLOCK
               prv_release(glConfig);
               #else
               ReleaseObject(glConfig);
               #endif
               glConfig = NULL;
               break;
            }
         }

         #ifdef QUICKLOCK
         prv_release(glConfig);
         #else
         ReleaseObject(glConfig);
         #endif

         #ifdef __unix__
            sched_yield();
         #endif
         if (glAccessGap > 0) WaitTime(0, glAccessGap);
      }
      else log.msg("Attempt %d.%d: Failed to acquire a lock, error: %s", info->index, i, GetErrorMsg(error));
   }

   log.msg("----- Thread %d is finished.", info->index);
   return NULL;
}

//********************************************************************************************************************
// Main.

int main(int argc, CSTRING *argv)
{
   pf::Log log;
   pf::vector<std::string> *args;

   if (auto msg = init_parasol(argc, argv)) {
      printf("%s\n", msg);
      return -1;
   }

   if ((!CurrentTask()->getPtr(FID_Parameters, &args)) and (args)) {
      for (unsigned i=0; i < args->size(); i++) {
         if (!StrMatch(args[0][i], "-threads")) {
            if (++i < args->size()) glTotalThreads = StrToInt(args[0][i]);
            else break;
         }
         else if (!StrMatch(args[0][i], "-attempts")) {
            if (++i < args->size()) glLockAttempts = StrToInt(args[0][i]);
            else break;
         }
         else if (!StrMatch(args[0][i], "-gap")) {
            if (++i < args->size()) glAccessGap = StrToInt(args[0][i]);
            else break;
         }
         else if (!StrMatch(args[0][i], "-terminate")) glTerminateObject = TRUE;
      }
   }

   glConfig = objConfig::create::global();

   #ifdef QUICKLOCK
      log.msg("Quick-locking will be tested.");
   #endif

   log.msg("Spawning %d threads...", glTotalThreads);

   struct thread_info glThreads[glTotalThreads];

   for (unsigned i=0; i < glTotalThreads; i++) {
      glThreads[i].index = i;
      pthread_create(&glThreads[i].thread, NULL, &thread_entry, &glThreads[i]);
   }

   // Main block now waits for all threads to terminate before it exits.
   // If main block exits, all threads exit, even if the threads have not
   // finished their work

   log.msg("Waiting for thread completion.");

   for (unsigned i=0; i < glTotalThreads; i++) {
      pthread_join(glThreads[i].thread, NULL);
   }

   FreeResource(glConfig);

   printf("Testing complete.\n");

   close_parasol();
}
