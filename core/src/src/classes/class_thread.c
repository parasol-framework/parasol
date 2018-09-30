/*****************************************************************************

The source code of the Parasol project is made publicly available under the
terms described in the LICENSE.TXT file that is distributed with this package.
Please refer to it for further information on licensing.

******************************************************************************

-CLASS-
Thread: Threads are created and managed by the Thread class.

The Thread class provides the means to execute and manage threads within an application.

The following code illustrates how to create a temporary thread that is automatically destroyed after the
thread_entry() function has completed:

<pre>
static ERROR thread_entry(objThread *Thread)
{
   return ERR_Okay;
}

if (!CreateObject(ID_THREAD, 0, &thread,
   FID_Routine|TPTR,    &thread_main,
   FID_Flags|TLONG,     THF_AUTO_FREE,
   TAGEND)) {

   acActivate(thread);
}
</pre>

To initialise the thread with data, call #SetData() prior to execution and read the #Data field from
within the thread routine.

-END-

*****************************************************************************/

#ifdef __unix__
#define _GNU_SOURCE
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <fcntl.h>
#include <unistd.h>
#endif

#include "../defs.h"
#include <parasol/main.h>

static const struct FieldArray clFields[];
static const struct ActionArray clThreadActions[];
static const struct MethodArray clThreadMethods[];

#define THREADPOOL_MAX 6

static LONG glActionThreadsIndex = 0;
static struct {
   objThread *Thread;
   BYTE InUse;
} glActionThreads[THREADPOOL_MAX];

//****************************************************************************

ERROR add_thread_class(void)
{
   ERROR error;

   if (!NewPrivateObject(ID_METACLASS, NULL, (OBJECTPTR *)&ThreadClass)) {
      if (!SetFields((OBJECTPTR)ThreadClass,
            FID_ClassVersion|TFLOAT, VER_THREAD,
            FID_Name|TSTR,      "Thread",
            FID_Category|TLONG, CCF_SYSTEM,
            FID_Actions|TPTR,   clThreadActions,
            FID_Methods|TARRAY, clThreadMethods,
            FID_Fields|TARRAY,  clFields,
            FID_Size|TLONG,     sizeof(objThread),
            FID_Path|TSTR,      "modules:core",
            TAGEND)) {
         error = acInit(&ThreadClass->Head);
      }
      else error = ERR_SetField;
   }
   else error = ERR_NewObject;

   return error;
}

//****************************************************************************
// Retrieve a thread object from the thread pool.

ERROR threadpool_get(objThread **Result)
{
   objThread *thread = NULL;
   ERROR error = ERR_Okay;

   FMSG("~threadpool_get()","");

   if (!thread_lock(TL_THREADPOOL, 2000)) {
      LONG i;
      for (i=0; i < glActionThreadsIndex; i++) {
         if ((glActionThreads[i].Thread) AND (!glActionThreads[i].InUse)) {
            thread = glActionThreads[i].Thread;
            glActionThreads[i].InUse = TRUE;
            break;
         }
      }

      if (!thread) { // Allocate a new thread.
         if (!(error = NewPrivateObject(ID_THREAD, NF_UNTRACKED, (OBJECTPTR *)&thread))) {
            SetName(&thread->Head, "ActionThread");
            if (!(error = acInit(&thread->Head))) {
               if ((i = glActionThreadsIndex) < THREADPOOL_MAX) { // Record the thread in the pool, if there is room for it.
                  glActionThreads[i].Thread = thread;
                  glActionThreads[i].InUse = TRUE;
                  glActionThreadsIndex++;
               }
            }
            else { acFree(&thread->Head); thread = NULL; }
         }
      }

      thread_unlock(TL_THREADPOOL);
   }
   else error = LogError(ERH_Core, ERR_Lock);

   if (thread) *Result = thread;

   STEP();
   return error;
}

//****************************************************************************
// Mark a thread in the pool as no longer in use.  The thread object will be destroyed if it is not in the pool.

void threadpool_release(objThread *Thread)
{
   FMSG("~threadpool_release()","Thread: #%d, Total: %d", Thread->Head.UniqueID, glActionThreadsIndex);

   if (!thread_lock(TL_THREADPOOL, 2000)) {
      LONG i;
      for (i=0; i < glActionThreadsIndex; i++) {
         if (glActionThreads[i].Thread IS Thread) {
            glActionThreads[i].InUse = FALSE;
            thread_unlock(TL_THREADPOOL);
            STEP();
            return;
         }
      }

      thread_unlock(TL_THREADPOOL);

      // If the thread object is not pooled, assume it was allocated dynamically from threadpool_get() and destroy it.

      acFree(&Thread->Head);
   }

   STEP();
}

//****************************************************************************
// Destroy the entire thread pool.  For use on application shutdown only.

void remove_threadpool(void)
{
   FMSG("~threadpool_free()","Removing the internal thread pool, size %d.", glActionThreadsIndex);

   if (!thread_lock(TL_THREADPOOL, 2000)) {
      LONG i;
      for (i=0; i < glActionThreadsIndex; i++) {
         if (glActionThreads[i].Thread) {
            if (glActionThreads[i].InUse) LogF("@Core","Pooled thread #%d is still in use on shutdown.", i);
            acFree(&glActionThreads[i].Thread->Head);
            glActionThreads[i].Thread = NULL;
            glActionThreads[i].InUse = FALSE;
         }
      }
      thread_unlock(TL_THREADPOOL);
   }

   STEP();
}

//****************************************************************************
// Called whenever a MSGID_THREAD_ACTION message is caught by ProcessMessages().  See thread_action() in lib_actions.c
// for usage.

ERROR msg_threadaction(APTR Custom, LONG MsgID, LONG MsgType, APTR Message, LONG MsgSize)
{
   struct ThreadActionMessage *msg;

   if (!(msg = (struct ThreadActionMessage *)Message)) return ERR_Okay;

   if (msg->Callback.Type IS CALL_STDC) {
      void (*routine)(ACTIONID, OBJECTPTR, ERROR, LONG) = msg->Callback.StdC.Routine;
      routine(msg->ActionID, msg->Object, msg->Error, msg->Key);
   }
   else if (msg->Callback.Type IS CALL_SCRIPT) {
      OBJECTPTR script;
      if ((script = msg->Callback.Script.Script)) {
         if (!AccessPrivateObject(script, 5000)) {
            const struct ScriptArg args[] = {
               { "ActionID", FD_LONG,      { .Long = msg->ActionID } },
               { "Object",   FD_OBJECTPTR, { .Address = msg->Object } },
               { "Error",    FD_LONG,      { .Long = msg->Error } },
               { "Key",      FD_LONG,      { .Long = msg->Key } }
            };
            scCallback(script, msg->Callback.Script.ProcedureID, args, ARRAYSIZE(args));
            ReleasePrivateObject(script);
         }
      }
   }

   return ERR_Okay;
}

//****************************************************************************
// Called whenever a MSGID_THREAD_CALLBACK message is caught by ProcessMessages().  See thread_entry() for usage.

ERROR msg_threadcallback(APTR Custom, LONG MsgID, LONG MsgType, APTR Message, LONG MsgSize)
{
   struct ThreadMessage *msg;
   if (!(msg = (struct ThreadMessage *)Message)) return ERR_Okay;

   objThread *thread;
   if (AccessObject(msg->ThreadID, 5000, (OBJECTPTR *)&thread)) {
      thread->prv.Active = FALSE;

      if (thread->prv.Callback.Type IS CALL_STDC) {
         void (*callback)(objThread *) = thread->prv.Callback.StdC.Routine;
         callback(thread);
      }
      else if (thread->prv.Callback.Type IS CALL_SCRIPT) {
         OBJECTPTR script;
         if ((script = thread->prv.Callback.Script.Script)) {
            if (!AccessPrivateObject(script, 5000)) {
               const struct ScriptArg args[] = { { "Thread", FD_OBJECTPTR, { .Address = thread } } };
               scCallback(script, thread->prv.Callback.Script.ProcedureID, args, ARRAYSIZE(args));
               ReleasePrivateObject(script);
            }
         }
      }

      if (thread->Flags & THF_AUTO_FREE) acFree(&thread->Head);

      ReleaseObject((OBJECTPTR)thread);
   }

   return ERR_Okay;
}

//****************************************************************************
// This is the entry point for all threads.

#ifdef _WIN32
static int thread_entry(objThread *Self)
#elif __unix__
static void * thread_entry(objThread *Self)
#endif
{
   if (Self->Flags & THF_MSG_HANDLER) {
      tlThreadReadMsg  = Self->prv.Msgs[0];
      tlThreadWriteMsg = Self->prv.Msgs[1];
   }

   // ENTRY

   if (Self->prv.Routine.Type) {
      Self->prv.Active = TRUE;

      if (Self->prv.Routine.Type IS CALL_STDC) {
         ERROR (*routine)(objThread *) = Self->prv.Routine.StdC.Routine;
         Self->Error = routine(Self);
      }
      else if (Self->prv.Routine.Type IS CALL_SCRIPT) {
         OBJECTPTR script;
         if ((script = Self->prv.Routine.Script.Script)) {
            if (!AccessPrivateObject(script, 5000)) {
               const struct ScriptArg args[] = { { "Thread", FD_OBJECTPTR, { .Address = Self } } };
               scCallback(script, Self->prv.Routine.Script.ProcedureID, args, ARRAYSIZE(args));
               ReleasePrivateObject(script);
            }
         }
      }

      if (Self->Flags & THF_MSG_HANDLER) {
         while (!ProcessMessages(0, -1));
      }

      if (Self->prv.Callback.Type) {
         // A message needs to be placed on the process' message queue with a reference to the thread object
         // so the callback can be processed by the main program thread.  See msg_threadcallback()

         struct ThreadMessage msg;
         msg.ThreadID = Self->Head.UniqueID;
         SendMessage(0, MSGID_THREAD_CALLBACK, MSF_ADD, &msg, sizeof(msg));

         //Self->prv.Active = FALSE; // Commented out because we don't want the active flag to be disabled until the callback is processed (for safety reasons).
      }
      else if (Self->Flags & THF_AUTO_FREE) {
         Self->prv.Active = FALSE;
         if (!AccessPrivateObject((OBJECTPTR)Self, 10000)) {
            acFree(&Self->Head);
            ReleasePrivateObject((OBJECTPTR)Self);
         }
      }
      else Self->prv.Active = FALSE;
   }

   // EXIT

   if (Self->Flags & THF_MSG_HANDLER) {
      tlThreadReadMsg  = 0;
      tlThreadWriteMsg = 0;
   }

   #ifdef _WIN32
      free_threadlock();
   #endif

   return 0;
}

/*****************************************************************************
-ACTION-
Activate: Spawn a new thread that calls the function referenced in the #Routine field.
-END-
*****************************************************************************/

static ERROR THREAD_Activate(objThread *Self, APTR Void)
{
   if (Self->prv.Active) return ERR_NothingDone;

#ifdef __unix__
   pthread_attr_t attr;
   pthread_attr_init(&attr);
   // On Linux it is better not to set the stack size, as it implies you intend to manually allocate the stack and guard it...
   //pthread_attr_setstacksize(&attr, Self->StackSize);
   if (!pthread_create(&Self->prv.PThread, &attr, (APTR)&thread_entry, Self)) {
      pthread_attr_destroy(&attr);
      return ERR_Okay;
   }
   else {
      char errstr[80];
      strerror_r(errno, errstr, sizeof(errstr));
      LogErrorMsg("pthread_create() failed with error: %s.", errstr);
      pthread_attr_destroy(&attr);
      return ERR_Failed;
   }

#elif _WIN32

   if ((Self->prv.Handle = winCreateThread(&thread_entry, Self, Self->StackSize, &Self->prv.ThreadID))) {
      return ERR_Okay;
   }
   else {
      return PostError(ERR_Failed);
   }

#else
   #error Platform support for threads is required.
#endif

}

/*****************************************************************************
-ACTION-
Deactivate: Stops a thread.

Deactivating an active thread will cause it to stop immediately.  Stopping a thread in this manner is dangerous and
should only be attempted if the circumstances require it.
-END-
*****************************************************************************/

static ERROR THREAD_Deactivate(objThread *Self, APTR Void)
{
   if (Self->prv.Active) {
      #ifdef __ANDROID__
         return ERR_NoSupport;
      #elif __unix__
         pthread_cancel(Self->prv.PThread);
      #elif _WIN32
         winTerminateThread(Self->prv.Handle);
      #else
         #warning Add code to kill threads.
      #endif

      Self->prv.Active = FALSE;
   }

   return ERR_Okay;
}

/*****************************************************************************
-ACTION-
Free: Remove the object and its resources.

Terminating a thread object will destroy the object unless the thread is currently active.  If an attempt to free
an active thread is made, it will be marked for termination so as to avoid the risk of system corruption.
-END-
*****************************************************************************/

static ERROR THREAD_Free(objThread *Self, APTR Void)
{
   if ((Self->Data) AND (Self->DataSize > 0)) {
      FreeMemory(Self->Data);
      Self->Data = NULL;
      Self->DataSize = 0;
   }

   #ifdef __unix__
      if (Self->prv.Msgs[0] != -1) { close(Self->prv.Msgs[0]); Self->prv.Msgs[0] = -1; }
      if (Self->prv.Msgs[1] != -1) { close(Self->prv.Msgs[1]); Self->prv.Msgs[1] = -1; }
   #elif _WIN32
      if (Self->prv.Msgs[0]) { winCloseHandle(Self->prv.Msgs[0]); Self->prv.Msgs[0] = 0; }
      if (Self->prv.Msgs[1]) { winCloseHandle(Self->prv.Msgs[1]); Self->prv.Msgs[1] = 0; }
   #endif

   return ERR_Okay;
}

//****************************************************************************

static ERROR THREAD_FreeWarning(objThread *Self, APTR Void)
{
   if (Self->prv.Active) {
      Self->Flags |= THF_AUTO_FREE;
      return ERR_InUse;
   }
   else return ERR_Okay;
}

//****************************************************************************

static ERROR THREAD_Init(objThread *Self, APTR Void)
{
   if (Self->StackSize < 1024) Self->StackSize = 1024;
   else if (Self->StackSize > 1024 * 1024) return PostError(ERR_OutOfRange);

   if (Self->Flags & THF_MSG_HANDLER) {
      #ifdef _WIN32
         if (winCreatePipe(&Self->prv.Msgs[0], &Self->prv.Msgs[1])) return PostError(ERR_SystemCall);
      #else
         if (!pipe(Self->prv.Msgs)) {
            //fcntl(Self->prv.Msgs[0], F_SETFL, fcntl(Self->prv.Msgs[0], F_GETFL)|O_NONBLOCK); // Do not block on read
            fcntl(Self->prv.Msgs[1], F_SETFL, fcntl(Self->prv.Msgs[1], F_GETFL)|O_NONBLOCK); // Do not block on write (see send_thread_msg())
         }
         else return PostError(ERR_SystemCall);
      #endif
   }

   return ERR_Okay;
}

//****************************************************************************

static ERROR THREAD_NewObject(objThread *Self, APTR Void)
{
   Self->StackSize = 16384;
   #ifdef __unix__
      Self->prv.Msgs[0] = -1;
      Self->prv.Msgs[1] = -1;
   #endif
   return ERR_Okay;
}

/*****************************************************************************

-METHOD-
SetData: Attaches data to the thread.

Use the SetData method prior to activating a thread so that it can be initialised with user data.  The thread will be
able to read the data from the #Data field.

A copy of the provided data buffer will be stored with the thread object, so there is no need to retain the original
data after this method has returned.  In some cases it may be desirable to store a direct pointer value with no data
copy operation.  To do this, set the Size parameter to zero.

-INPUT-
buf(ptr) Data: Pointer to the data buffer.
bufsize Size: Size of the data buffer.  If zero, the pointer is stored directly, with no copy operation taking place.

-ERRORS-
Okay
NullArgs
Args
AllocMemory
-END-

*****************************************************************************/

static ERROR THREAD_SetData(objThread *Self, struct thSetData *Args)
{
   if ((!Args) OR (!Args->Data)) return PostError(ERR_NullArgs);
   if (Args->Size < 0) return PostError(ERR_Args);

   if (Self->Data) {
      FreeMemory(Self->Data);
      Self->Data = NULL;
      Self->DataSize = 0;
   }

   if (!Args->Size) { // If no size is provided, we simply copy the provided pointer.
      Self->Data = Args->Data;
      return ERR_Okay;
   }
   else if (!AllocMemory(Args->Size, MEM_DATA, &Self->Data, NULL)) {
      Self->DataSize = Args->Size;
      CopyMemory(Args->Data, Self->Data, Args->Size);
      return ERR_Okay;
   }
   else return PostError(ERR_AllocMemory);
}

/*****************************************************************************

-METHOD-
Wait: Waits for a thread to be completed.

Call the Wait method to wait for a thread to complete its activity.  Because waiting for a thread will normally cause
the caller to halt all processing, the MsgInterval parameter can be used to make periodic calls to ~ProcessMessages()
every X milliseconds.  If the MsgInterval is set to -1 then no periodic message checks will be made.

Limitations: Android and OSX implementations do not currently support the TimeOut or MsgInterval parameters.

-INPUT-
int TimeOut: A timeout value measured in milliseconds.
int MsgInterval: Check for incoming messages every X milliseconds.

-ERRORS-
Okay
NullArgs
Args: The TimeOut value is invalid.
TimeOut: The timeout was reached before the thread completed.
-END-

*****************************************************************************/

static ERROR THREAD_Wait(objThread *Self, struct thWait *Args)
{
   if (!Args) return PostError(ERR_NullArgs);
   if (Args->TimeOut < 0) return PostError(ERR_Args);
   if (Args->MsgInterval < -1) return PostError(ERR_Args);

#ifdef __ANDROID__
   pthread_join(Self->prv.PThread, NULL);
   return ERR_Okay;
#elif __APPLE__
   // TODO: Simulation of pthread_timedjoin_np() is possible by creating locked semaphores for the threads, then
   // wait on the semaphore here for the lock to be released.
   pthread_join(Self->prv.PThread, NULL);
   return ERR_Okay;
#else
   LARGE current_time = (PreciseTime()/1000LL);
   LARGE end_time = current_time + Args->TimeOut;

    __sync_add_and_fetch(&Self->prv.Waiting, 1);

    do {
      LONG time_left = end_time - current_time;

      if (Args->MsgInterval != -1) { // -1 means do not use message intervals.
         if (time_left > Args->MsgInterval) time_left = Args->MsgInterval;
      }
      #ifdef _WIN32
      if (!winWaitThread(Self->prv.Handle, Args->TimeOut)) {
      #else
      struct timespec timeout;
      APTR result;
      timeout.tv_sec = time_left / 1000;
      timeout.tv_nsec = (time_left % 1000) * 1000000; // 1 in 1,000,000,000
      if (!pthread_timedjoin_np(Self->prv.PThread, &result, &timeout)) {
      #endif
         __sync_sub_and_fetch(&Self->prv.Waiting, 1);
         return ERR_Okay;
      }
      else break;

      if (ProcessMessages(0, 0) IS ERR_Terminate) break;

      current_time = (PreciseTime()/1000LL);
   } while (current_time < end_time);

   __sync_sub_and_fetch(&Self->prv.Waiting, 1);
   return ERR_TimeOut;
#endif
}

/*****************************************************************************

-FIELD-
Data: Pointer to initialisation data for the thread.

The Data field will point to a data buffer if the #SetData() method has previously been called to store data in
the thread object.  It is paired with the #DataSize field, which reflects the size of the data buffer.

*****************************************************************************/

static ERROR GET_Data(objThread *Self, APTR *Value, LONG *Elements)
{
   *Value = Self->Data;
   *Elements = Self->DataSize;
   return ERR_Okay;
}

static ERROR SET_Data(objThread *Self, APTR Value, LONG Elements)
{
   if ((Value) AND (Elements > 0)) {
      Self->Data = Value;
      Self->DataSize = Elements;
   }
   else {
      Self->Data = NULL;
      Self->DataSize = 0;
   }
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
DataSize: The size of the buffer referenced in the Data field.

-FIELD-
Error: Reflects the error code returned by the thread routine.

-FIELD-
Flags: Optional flags can be defined here.
Lookup: THF

-FIELD-
Routine: A function reference that will be called when the thread is started.

The routine that will be executed when the thread is activated must be specified here.  The function synopsis is
`ERROR routine(objThread *Thread)`.

When the routine is called, a reference to the thread object is passed as a parameter.  Once the routine has
finished processing, the resulting error code will be stored in the thread object's #Error field.

*****************************************************************************/

static ERROR GET_Routine(objThread *Self, FUNCTION **Value)
{
   if (Self->prv.Routine.Type != CALL_NONE) {
      *Value = &Self->prv.Routine;
      return ERR_Okay;
   }
   else return ERR_FieldNotSet;
}

static ERROR SET_Routine(objThread *Self, FUNCTION *Value)
{
   if (Value) Self->prv.Routine = *Value;
   else Self->prv.Routine.Type = CALL_NONE;
   return ERR_Okay;
}

/*****************************************************************************
-FIELD-
StackSize: The stack size to allocate for the thread.

The initial size of the thread's stack can be modified by setting this field.  It may also be read prior to activation
in order to check the default stack size.  Changes to the stack size when the thread is active will have no effect.

Note that on some platforms it may not be possible to preset the stack size, in which case the provided value will
be ignored.
-END-
*****************************************************************************/

static const struct FieldDef clThreadFlags[] = {
   { "AutoFree",   THF_AUTO_FREE },
   { "MsgHandler", THF_MSG_HANDLER },
   { NULL, 0 }
};

static const struct FieldArray clFields[] = {
   { "Data",      FDF_ARRAY|FDF_BYTE|FDF_R, 0, GET_Data, SET_Data },
   { "DataSize",  FD_LONG|FDF_R,       0, NULL, NULL },
   { "StackSize", FDF_LONG|FDF_RW,     0, NULL, NULL },
   { "Error",     FDF_LONG|FDF_R,      0, NULL, NULL },
   { "Flags",     FDF_LONG|FDF_RI,     (MAXINT)&clThreadFlags, NULL, NULL },
   // Virtual fields
   { "Routine",   FDF_FUNCTIONPTR|FDF_RW,  0, GET_Routine, SET_Routine },
   END_FIELD
};

#include "class_thread_def.c"
