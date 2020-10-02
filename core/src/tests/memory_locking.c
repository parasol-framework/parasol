/*****************************************************************************

The source code of the Parasol project is made publicly available under the
terms described in the LICENSE.TXT file that is distributed with this package.
Please refer to it for further information on licensing.

******************************************************************************

This program tests the locking of private and public memory between threads.
Use parameter '-public' to test public memory locking.

*****************************************************************************/

#include <pthread.h>

#include <parasol/startup.h>

STRING ProgName      = "MemoryLocking";
STRING ProgAuthor    = "Paul Manias";
STRING ProgDate      = "July 2014";
STRING ProgCopyright = "Paul Manias (c) 2014";
LONG   ProgDebug = 8;
FLOAT  ProgCoreVersion = 1.0;

static struct StringsBase *StringsBase;
static struct FileSystemBase *FileSystemBase;
extern struct CoreBase *CoreBase;
static volatile MEMORYID glMemoryID = 0;
static LONG glTotalThreads = 2;
static LONG glLockAttempts = 20;
static BYTE glTerminateMemory = FALSE;
static LONG glAccessGap = 2000;
static BYTE glPublic = FALSE, glTestAllocation = FALSE;

struct thread_info{
   pthread_t thread;
   int index;
};

/*****************************************************************************
** Internal: test_locking()
*/

static void * test_locking(struct thread_info *info)
{
   LONG i;
   ERROR error;
   BYTE *memory;

   info->index = GetResource(RES_THREAD_ID);
   LogMsg("----- Thread %d is starting now.", info->index);

   for (i=0; i < glLockAttempts; i++) {
      if (!glMemoryID) break;
      //LogF("~","Attempt %d.%d: Acquiring the memory.", info->index, i);

      if (!(error = AccessMemory(glMemoryID, MEM_READ_WRITE, 30000, &memory))) {
         memory[0]++;
         LogMsg("%d.%d: Memory acquired.", info->index, i);
         WaitTime(0, 2000);
         if (memory[0] > 1) LogErrorMsg("--- MAJOR ERROR %d: More than one thread has access to this memory!", info->index);
         memory[0]--;

         // Test that object removal works in ReleasePrivateObject() and that waiting threads fail peacefully.

         if (glTerminateMemory) {
            if (i >= glLockAttempts-2) {
               FreeResource(memory);
               ReleaseMemoryID(glMemoryID);
               memory = NULL;
               break;
            }
         }

         ReleaseMemoryID(glMemoryID);

         LogMsg("%d: Memory released.", info->index);

         #ifdef __unix__
            pthread_yield();
         #endif
         if (glAccessGap > 0) WaitTime(0, glAccessGap);
      }
      else LogMsg("Attempt %d.%d: Failed to acquire a lock, error: %s", info->index, i, GetErrorMsg(error));
   }

   LogMsg("----- Thread %d is finished.", info->index);
   return NULL;
}

/*****************************************************************************
** Internal: test_allocation()
**
** Allocate and free sets of memory blocks at random intervals.
*/

#define TOTAL_ALLOC 2000

static void * test_allocation(struct thread_info *info)
{
   APTR memory[TOTAL_ALLOC];

   LONG i, j, start;
   start = 0;
   for (i=0; i < TOTAL_ALLOC; i++) {
      AllocMemory(1024, MEM_DATA|MEM_NO_CLEAR, &memory[i], NULL);
      if (RandomNumber(10) > 7) {
         for (j=start; j < i; j++) {
            FreeResource(memory[j]);
         }
         start = j;
      }
   }

   for (j=start; j < i; j++) {
      FreeResource(memory[j]);
   }

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
         else if (!StrMatch(args[i], "-terminate")) glTerminateMemory = TRUE;
         else if (!StrMatch(args[i], "-public")) glPublic = TRUE;
         else if (!StrMatch(args[i], "-alloc")) glTestAllocation = TRUE;
      }
   }

   if (glPublic) AllocMemory(10000, MEM_DATA|MEM_PUBLIC, NULL, (MEMORYID *)&glMemoryID);
   else AllocMemory(10000, MEM_DATA, NULL, (MEMORYID *)&glMemoryID);

   print("Spawning %d threads...\n", glTotalThreads);

   struct thread_info glThreads[glTotalThreads];

   for (i=0; i < glTotalThreads; i++) {
      glThreads[i].index = i;
      if (glTestAllocation) pthread_create(&glThreads[i].thread, NULL, (void *)&test_allocation, &glThreads[i]);
      else pthread_create(&glThreads[i].thread, NULL, (void *)&test_locking, &glThreads[i]);
   }

   // Main block now waits for both threads to terminate, before it exits.  If main block exits, both threads exit,
   // even if the threads have not finished their work

   print("Waiting for thread completion.\n");

   for (i=0; i < glTotalThreads; i++) {
      pthread_join(glThreads[i].thread, NULL);
   }

   FreeResourceID(glMemoryID);

   print("Testing complete.\n");
}
