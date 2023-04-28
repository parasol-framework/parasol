/*********************************************************************************************************************

The source code of the Parasol project is made publicly available under the terms described in the LICENSE.TXT file
that is distributed with this package.  Please refer to it for further information on licensing.

**********************************************************************************************************************

This program tests the locking of memory between threads.

*********************************************************************************************************************/

#include <pthread.h>
#include <parasol/startup.h>

CSTRING ProgName = "MemoryLocking";
extern struct CoreBase *CoreBase;
static volatile MEMORYID glMemoryID = 0;
static LONG glTotalThreads = 2;
static ULONG glLockAttempts = 20;
static bool glTerminateMemory = false;
static LONG glAccessGap = 2000;
static bool glTestAllocation = false;

struct thread_info{
   pthread_t thread;
   int index;
};

//********************************************************************************************************************

static void * test_locking(void *Arg)
{
   pf::Log log(__FUNCTION__);
   auto info = (thread_info *)Arg;

   info->index = GetResource(RES::THREAD_ID);
   log.msg("----- Thread %d is starting now.", info->index);

   for (unsigned i=0; i < glLockAttempts; i++) {
      if (!glMemoryID) break;
      //log.branch("Attempt %d.%d: Acquiring the memory.", info->index, i);

      BYTE *memory;
      if (auto error = AccessMemory(glMemoryID, MEM::READ_WRITE, 30000, &memory); !error) {
         memory[0]++;
         log.msg("%d.%d: Memory acquired.", info->index, i);
         WaitTime(0, 2000);
         if (memory[0] > 1) log.warning("--- MAJOR ERROR %d: More than one thread has access to this memory!", info->index);
         memory[0]--;

         // Test that object removal works in ReleaseObject() and that waiting threads fail peacefully.

         if (glTerminateMemory) {
            if (i >= glLockAttempts-2) {
               FreeResource(memory);
               ReleaseMemory(glMemoryID);
               memory = NULL;
               break;
            }
         }

         ReleaseMemory(glMemoryID);

         log.msg("%d: Memory released.", info->index);

         #ifdef __unix__
            pthread_yield();
         #endif
         if (glAccessGap > 0) WaitTime(0, glAccessGap);
      }
      else log.msg("Attempt %d.%d: Failed to acquire a lock, error: %s", info->index, i, GetErrorMsg(error));
   }

   log.msg("----- Thread %d is finished.", info->index);
   return NULL;
}

//********************************************************************************************************************
// Allocate and free sets of memory blocks at random intervals.

static const LONG TOTAL_ALLOC = 2000;

static void * test_allocation(void *Arg)
{
   APTR memory[TOTAL_ALLOC];

   LONG i, j;
   LONG start = 0;
   for (i=0; i < TOTAL_ALLOC; i++) {
      AllocMemory(1024, MEM::DATA|MEM::NO_CLEAR, &memory[i], NULL);
      if (rand() % 10 > 7) {
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

//********************************************************************************************************************

int main(int argc, CSTRING *argv)
{
   LONG i;
   pf::vector<std::string> *args;

   if (auto msg = init_parasol(argc, argv)) {
      print(msg);
      return -1;
   }

   if ((CurrentTask()->getPtr(FID_Parameters, &args) IS ERR_Okay) and (args)) {
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
         else if (!StrMatch(args[0][i], "-terminate")) glTerminateMemory = true;
         else if (!StrMatch(args[0][i], "-alloc")) glTestAllocation = true;
      }
   }

   AllocMemory(10000, MEM::DATA, NULL, (MEMORYID *)&glMemoryID);

   print("Spawning %d threads...\n", glTotalThreads);

   thread_info glThreads[glTotalThreads];

   for (i=0; i < glTotalThreads; i++) {
      glThreads[i].index = i;
      if (glTestAllocation) pthread_create(&glThreads[i].thread, NULL, &test_allocation, &glThreads[i]);
      else pthread_create(&glThreads[i].thread, NULL, &test_locking, &glThreads[i]);
   }

   // Main block now waits for both threads to terminate, before it exits.  If main block exits, both threads exit,
   // even if the threads have not finished their work

   print("Waiting for thread completion.\n");

   for (i=0; i < glTotalThreads; i++) {
      pthread_join(glThreads[i].thread, NULL);
   }

   FreeResource(glMemoryID);

   print("Testing complete.\n");

   close_parasol();
}
