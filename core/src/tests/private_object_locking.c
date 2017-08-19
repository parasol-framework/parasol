/*****************************************************************************

The source code of the Parasol project is made publicly available under the
terms described in the LICENSE.TXT file that is distributed with this package.
Please refer to it for further information on licensing.

******************************************************************************

This program tests the locking of private objects between threads.

*****************************************************************************/

#ifdef __unix__
#define _GNU_SOURCE
#endif

#include <pthread.h>

#include <parasol/startup.h>


STRING ProgName      = "ObjectLocking";
STRING ProgAuthor    = "Paul Manias";
STRING ProgDate      = "February 2014";
STRING ProgCopyright = "Paul Manias (c) 2014";
LONG   ProgDebug = 8;
FLOAT  ProgCoreVersion = 1.0;

static struct StringsBase *StringsBase;
static struct FileSystemBase *FileSystemBase;
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
      SUB_QUEUE(Object); // Put the lock count back to normal before AccessPrivateObject()
      return AccessPrivateObject(Object, -1);
   }
}

INLINE void prv_release(OBJECTPTR Object)
{
   if (Object->SleepQueue > 0) ReleasePrivateObject(Object);
   else SUB_QUEUE(Object);
}
#endif

/*****************************************************************************
** Internal: thread_entry()
*/

static void * thread_entry(struct thread_info *info)
{
   LONG i;
   ERROR error;

   info->index = GetResource(RES_THREAD_ID);
   LogMsg("----- Thread %d is starting now.", info->index);

   for (i=0; i < glLockAttempts; i++) {
      if (!glConfig) break;
      //LogF("~","Attempt %d.%d: Acquiring the object.", info->index, i);
      #ifdef QUICKLOCK
      if (!(error = prv_access(glConfig, info->index))) {
      #else
      if (!(error = AccessPrivateObject(glConfig, 30000))) {
      #endif
         glConfig->ActionDepth++;
         LogMsg("%d.%d: Object acquired.", info->index, i);
         WaitTime(0, 2000);
         if (glConfig->ActionDepth > 1) LogErrorMsg("--- MAJOR ERROR: More than one thread has access to this object!");
         glConfig->ActionDepth--;

         // Test that object removal works in ReleasePrivateObject() and that waiting threads fail peacefully.

         if (glTerminateObject) {
            if (i >= glLockAttempts-2) {
               acFree(glConfig);
               #ifdef QUICKLOCK
               prv_release(glConfig);
               #else
               ReleasePrivateObject(glConfig);
               #endif
               glConfig = NULL;
               //LogBack();
               break;
            }
         }

         #ifdef QUICKLOCK
         prv_release(glConfig);
         #else
         ReleasePrivateObject(glConfig);
         #endif

         #ifdef __unix__
            pthread_yield();
         #endif
         if (glAccessGap > 0) WaitTime(0, glAccessGap);
      }
      else LogMsg("Attempt %d.%d: Failed to acquire a lock, error: %s", info->index, i, GetErrorMsg(error));
      //LogBack();
   }

   LogMsg("----- Thread %d is finished.", info->index);
   return NULL;
}

/*****************************************************************************
** Main.
*/

void program(void)
{
   LONG i;
   STRING *args;

   StringsBase = GetResourcePtr(RES_STRINGS);
   FileSystemBase = GetResourcePtr(RES_FILESYSTEM);

   if ((GetPointer(CurrentTask(), FID_Parameters, &args) IS ERR_Okay) AND (args)) {
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


   CreateObject(ID_CONFIG, 0, (OBJECTPTR *)&glConfig,
      TAGEND);

   #ifdef QUICKLOCK
      LogMsg("Quick-locking will be tested.");
   #endif

   LogMsg("Spawning %d threads...", glTotalThreads);

   struct thread_info glThreads[glTotalThreads];

   for (i=0; i < glTotalThreads; i++) {
      glThreads[i].index = i;
      pthread_create(&glThreads[i].thread, NULL, (void *)&thread_entry, &glThreads[i]);
   }

   // Main block now waits for both threads to terminate, before it exits
   // If main block exits, both threads exit, even if the threads have not
   // finished their work

   LogMsg("Waiting for thread completion.");

   for (i=0; i < glTotalThreads; i++) {
      pthread_join(glThreads[i].thread, NULL);
   }

   acFree(glConfig);

   print("Testing complete.\n");
}
