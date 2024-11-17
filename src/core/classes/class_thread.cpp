/*********************************************************************************************************************

The source code of the Parasol project is made publicly available under the terms described in the LICENSE.TXT file
that is distributed with this package.  Please refer to it for further information on licensing.

**********************************************************************************************************************

-CLASS-
Thread: Threads are created and managed by the Thread class.

The Thread class provides the means to execute and manage threads within an application.

The following code illustrates how to create a temporary thread that is automatically destroyed after the
`thread_entry()` function has completed:

<pre>
static ERR thread_entry(objThread *Thread) {
   return ERR::Okay;
}

objThread::create thread = { fl::Routine(thread_entry), fl::Flags(THF::AUTO_FREE) };
if (thread.ok()) thread->activate(thread);
</pre>

To initialise the thread with data, call #SetData() prior to execution and read the #Data field from within the
thread routine.

-END-

*********************************************************************************************************************/

#ifdef __unix__
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <fcntl.h>
#include <unistd.h>
#endif

#include "../defs.h"
#include <parasol/main.h>

static const LONG THREADPOOL_MAX = 6;

static void thread_entry_cleanup(void *);

struct AsyncAction {
   extThread *Thread;
   bool InUse;

   // Move constructor
   AsyncAction(AsyncAction &&Other) noexcept : Thread(NULL), InUse(false) {
      Thread = Other.Thread;
      InUse  = Other.InUse;
      Other.Thread = NULL;
      Other.InUse  = false;
   }

   AsyncAction() : Thread(0), InUse(0) { }

   AsyncAction(extThread *pThread) {
      Thread = pThread;
      InUse  = pThread ? true : false;
   }

   ~AsyncAction() {
      if (InUse) {
         pf::Log log(__FUNCTION__);
         log.warning("Pooled thread #%d is still in use on closure.", Thread->UID);
      }
   }
};

static std::vector<struct AsyncAction> glAsyncActions;

//********************************************************************************************************************
// Returns a unique ID for the active thread.  The ID has no relationship with the host operating system.

static THREADVAR THREADID tlUniqueThreadID(0);
static std::atomic_int glThreadIDCount = 1;

THREADID get_thread_id(void)
{
   if (tlUniqueThreadID.defined()) return tlUniqueThreadID;
   tlUniqueThreadID = THREADID(glThreadIDCount++);
   return tlUniqueThreadID;
}

//********************************************************************************************************************
// Retrieve a thread object from the thread pool.

ERR threadpool_get(extThread **Result)
{
   pf::Log log;

   log.traceBranch();

   glmThreadPool.lock();

      for (auto &at : glAsyncActions) {
         if ((at.Thread) and (!at.InUse)) {
            at.InUse = true;
            *Result = at.Thread;
            glmThreadPool.unlock();
            return ERR::Okay;
         }
      }

   glmThreadPool.unlock();

   if (auto thread = extThread::create::untracked(fl::Name("AsyncAction"))) {
      glmThreadPool.lock();
      if (glAsyncActions.size() < THREADPOOL_MAX) {
         glAsyncActions.emplace_back(thread);
         thread->Pooled = true;
      }
      *Result = thread;
      glmThreadPool.unlock();
      return ERR::Okay;
   }
   else return log.warning(ERR::CreateObject);
}

//********************************************************************************************************************
// Mark a thread in the pool as no longer in use.  The thread object will be destroyed if it is not in the pool.

void threadpool_release(extThread *Thread)
{
   pf::Log log;

   log.traceBranch("Thread: #%d, Total: %d", Thread->UID, (LONG)glAsyncActions.size());

   glmThreadPool.lock();
   for (auto &at : glAsyncActions) {
      if (at.Thread IS Thread) {
         at.InUse = false;
         Thread->Active = false; // For pooled threads we mark them as inactive manually.
         glmThreadPool.unlock();
         return;
      }
   }

   // If the thread object is not pooled, assume it was allocated dynamically from threadpool_get() and destroy it.

   glmThreadPool.unlock();
   FreeResource(Thread);
}

//********************************************************************************************************************
// Destroy the entire thread pool.  For use on application shutdown only.

void remove_threadpool(void)
{
   if (!glAsyncActions.empty()) {
      pf::Log log("Core");
      log.branch("Removing the action thread pool of %d threads.", (LONG)glAsyncActions.size());
      std::lock_guard lock(glmThreadPool);
      glAsyncActions.clear();
   }
}

//********************************************************************************************************************
// Called whenever a MSGID_THREAD_ACTION message is caught by ProcessMessages().  See thread_action() for usage.

ERR msg_threadaction(APTR Custom, LONG MsgID, LONG MsgType, APTR Message, LONG MsgSize)
{
   auto msg = (ThreadActionMessage *)Message;
   if (!msg) return ERR::Okay;

   if (msg->Callback.isC()) {
      auto routine = (void (*)(ACTIONID, OBJECTPTR, ERR, LONG, APTR))msg->Callback.Routine;
      routine(msg->ActionID, msg->Object, msg->Error, msg->Key, msg->Callback.Meta);
   }
   else if (msg->Callback.isScript()) {
      auto script = msg->Callback.Context;
      if (LockObject(script, 5000) IS ERR::Okay) {
         sc::Call(msg->Callback, std::to_array<ScriptArg>({
            { "ActionID", LONG(msg->ActionID) },
            { "Object",   msg->Object, FD_OBJECTPTR },
            { "Error",    LONG(msg->Error) },
            { "Key",      msg->Key }
         }));
         ReleaseObject(script);
      }
   }

   return ERR::Okay;
}

//********************************************************************************************************************
// Called whenever a MSGID_THREAD_CALLBACK message is caught by ProcessMessages().  See thread_entry() for usage.

ERR msg_threadcallback(APTR Custom, LONG MsgID, LONG MsgType, APTR Message, LONG MsgSize)
{
   pf::Log log;

   auto msg = (ThreadMessage *)Message;
   auto uid = msg->ThreadID;

   log.branch("Executing completion callback for thread #%d", uid);

   if (msg->Callback.isC()) {
      auto callback = (void (*)(OBJECTID, APTR))msg->Callback.Routine;
      callback(uid, msg->Callback.Meta);
   }
   else if (msg->Callback.isScript()) {
      ScopedObjectLock script(msg->Callback.Context, 5000);
      if (script.granted()) sc::Call(msg->Callback, std::to_array<ScriptArg>({ { "Thread", uid, FD_OBJECTID } }));
   }

   // NB: Assume 'msg' is unstable after this point because the callback may have modified the message table.

   ScopedObjectLock<extThread> thread(uid, 10000);
   if (thread.granted()) {
      // NB: If a client wants notification of the thread ending, they can use WaitForObjects()
      // if not using callbacks.
      thread->Active = false;
      if ((thread->Flags & THF::AUTO_FREE) != THF::NIL) {
         FreeResource(*thread);
      }
      else acSignal(*thread); // Convenience for the client
   }
   else log.warning(ERR::AccessObject);

   return ERR::Okay;
}

//********************************************************************************************************************
// This is the entry point for all threads.

THREADVAR BYTE tlThreadCrashed;
THREADVAR extThread *tlThreadRef;

#ifdef _WIN32
static int thread_entry(extThread *Self)
#elif __unix__
static void * thread_entry(extThread *Self)
#endif
{
   auto uid = Self->UID;

   // Note that the Active flag will have been set to true prior to entry, and will remain until msg_threadcallback()
   // is called.

   // Configure crash indicators and a cleanup operation

   tlThreadCrashed = true;
   tlThreadRef     = Self;
   #ifdef __GNUC__
   pthread_cleanup_push(&thread_entry_cleanup, Self);
   #endif

   ThreadMessage msg = { .ThreadID = uid, .Callback = Self->Callback };
   bool pooled = Self->Pooled;

   {
      // Replace the default dummy context with one that pertains to the thread
      ObjectContext thread_ctx(Self, AC::NIL);

      if (Self->Routine.isC()) {
         auto routine = (ERR (*)(extThread *, APTR))Self->Routine.Routine;
         Self->Error = routine(Self, Self->Routine.Meta);
      }
      else if (Self->Routine.isScript()) {
         ScopedObjectLock script(Self->Routine.Context, 5000);
         if (script.granted()) {
            sc::Call(Self->Routine, std::to_array<ScriptArg>({ { "Thread", Self, FD_OBJECTPTR } }));
         }
      }
   }

   // Please no references to Self after this point.  It is possible that the Thread object has been forcibly removed
   // if the client routine is persistently running during shutdown.

   // See msg_threadcallback()
   if (!pooled) SendMessage(MSGID_THREAD_CALLBACK, MSF::ADD|MSF::WAIT, &msg, sizeof(msg));

   // Reset the crash indicators and invoke the cleanup code.
   tlThreadRef     = NULL;
   tlThreadCrashed = false;
   #ifdef __GNUC__
   pthread_cleanup_pop(true);
   #endif
   return 0;
}

//********************************************************************************************************************
// Cleanup on completion of a thread.  Note that this will also run in the event that the thread throws an exception.

static void thread_entry_cleanup(void *Arg)
{
   if (tlThreadCrashed) {
      pf::Log log("thread_cleanup");
      log.error("A thread in this program has crashed.");
      if (tlThreadRef) tlThreadRef->Active = false;
   }

   #ifdef _WIN32
      free_threadlock();
   #endif
}

/*********************************************************************************************************************
-ACTION-
Activate: Spawn a new thread that calls the function referenced in the #Routine field.
-END-
*********************************************************************************************************************/

static ERR THREAD_Activate(extThread *Self)
{
   pf::Log log;

   if (Self->Active) return ERR::ThreadAlreadyActive;

   // Thread objects MUST be locked prior to activation.  There is otherwise a genuine risk that the object
   // could be terminated by code operating outside of the thread space while it is active.

   if (Self->Queue.load() < 1) return log.warning(ERR::ThreadNotLocked);

   Self->Active = true; // Indicate that the thread is running

#ifdef __unix__

   pthread_attr_t attr;
   pthread_attr_init(&attr);
   // On Linux it is better not to set the stack size, as it implies you intend to manually allocate the stack and guard it...
   //pthread_attr_setstacksize(&attr, Self->StackSize);
   if (!pthread_create(&Self->PThread, &attr, (APTR (*)(APTR))&thread_entry, Self)) {
      pthread_attr_destroy(&attr);
      return ERR::Okay;
   }
   else {
      char errstr[80];
      strerror_r(errno, errstr, sizeof(errstr));
      log.warning("pthread_create() failed with error: %s.", errstr);
      pthread_attr_destroy(&attr);
      Self->Active = false;
      return log.warning(ERR::SystemCall);
   }

#elif _WIN32

   if ((Self->Handle = winCreateThread((APTR)&thread_entry, Self, Self->StackSize, &Self->ThreadID))) {
      return ERR::Okay;
   }
   else {
      Self->Active = false;
      return log.warning(ERR::SystemCall);
   }

#else
   #error Platform support for threads is required.
#endif
}

/*********************************************************************************************************************
-ACTION-
Deactivate: Stops a thread.

Deactivating an active thread will cause it to stop immediately.  Stopping a thread in this manner is dangerous and
could result in an unstable application.
-END-
*********************************************************************************************************************/

static ERR THREAD_Deactivate(extThread *Self)
{
   if (Self->Active) {
      #ifdef __ANDROID__
         return ERR::NoSupport;
      #elif __unix__
         pthread_cancel(Self->PThread);
      #elif _WIN32
         winTerminateThread(Self->Handle);
      #else
         #warning Add code to kill threads.
      #endif

      Self->Active = false;
   }

   return ERR::Okay;
}

/*********************************************************************************************************************
-ACTION-
Free: Remove the object and its resources.

Terminating a thread object will destroy the object unless the thread is currently active.  If an attempt to free
an active thread is made then it will be marked for termination so as to avoid the risk of system corruption.
-END-
*********************************************************************************************************************/

static ERR THREAD_Free(extThread *Self)
{
   if ((Self->Data) and (Self->DataSize > 0)) {
      FreeResource(Self->Data);
      Self->Data = NULL;
      Self->DataSize = 0;
   }

   #ifdef __unix__
      if (Self->Msgs[0] != -1) { close(Self->Msgs[0]); Self->Msgs[0] = -1; }
      if (Self->Msgs[1] != -1) { close(Self->Msgs[1]); Self->Msgs[1] = -1; }
   #elif _WIN32
      if (Self->Msgs[0]) { winCloseHandle(Self->Msgs[0]); Self->Msgs[0] = 0; }
      if (Self->Msgs[1]) { winCloseHandle(Self->Msgs[1]); Self->Msgs[1] = 0; }
   #endif

   Self->~extThread();
   return ERR::Okay;
}

//********************************************************************************************************************

static ERR THREAD_FreeWarning(extThread *Self)
{
   if (!Self->Active) return ERR::Okay;
   else {
      pf::Log log;
      log.detail("Thread is still running, marking for auto termination.");
      Self->Flags |= THF::AUTO_FREE;
      return ERR::InUse;
   }
}

//********************************************************************************************************************

static ERR THREAD_Init(extThread *Self)
{
   pf::Log log;

   if (Self->StackSize < 1024) Self->StackSize = 1024;
   else if (Self->StackSize > 1024 * 1024) return log.warning(ERR::OutOfRange);

   return ERR::Okay;
}

//********************************************************************************************************************

static ERR THREAD_NewPlacement(extThread *Self)
{
   new (Self) extThread;
   Self->StackSize = 16384;
   #ifdef __unix__
      Self->Msgs[0] = -1;
      Self->Msgs[1] = -1;
   #endif
   return ERR::Okay;
}

/*********************************************************************************************************************

-METHOD-
SetData: Attaches data to the thread.

Use the SetData() method prior to activating a thread so that it can be initialised with user data.  The thread will be
able to read the data from the #Data field.

A copy of the provided data buffer will be stored with the thread object, so there is no need to retain the original
data after this method has returned.  In some cases it may be desirable to store a direct pointer value and bypass the
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

*********************************************************************************************************************/

static ERR THREAD_SetData(extThread *Self, struct th::SetData *Args)
{
   pf::Log log;

   if ((!Args) or (!Args->Data)) return log.warning(ERR::NullArgs);
   if (Args->Size < 0) return log.warning(ERR::Args);

   if (Self->Data) {
      FreeResource(Self->Data);
      Self->Data = NULL;
      Self->DataSize = 0;
   }

   if (!Args->Size) { // If no size is provided, we simply copy the provided pointer.
      Self->Data = Args->Data;
      return ERR::Okay;
   }
   else if (AllocMemory(Args->Size, MEM::DATA, &Self->Data, NULL) IS ERR::Okay) {
      Self->DataSize = Args->Size;
      copymem(Args->Data, Self->Data, Args->Size);
      return ERR::Okay;
   }
   else return log.warning(ERR::AllocMemory);
}

/*********************************************************************************************************************

-FIELD-
Callback: A function reference that will be called when the thread is started.

Set a function reference here to receive a notification when the thread finishes processing.  The
callback will be executed in the context of the main program loop to minimise resource locking issues.

The prototype for the callback routine is `void Callback(objThread *Thread)`.

*********************************************************************************************************************/

static ERR GET_Callback(extThread *Self, FUNCTION **Value)
{
   if (Self->Callback.defined()) {
      *Value = &Self->Callback;
      return ERR::Okay;
   }
   else return ERR::FieldNotSet;
}

static ERR SET_Callback(extThread *Self, FUNCTION *Value)
{
   if (Value) Self->Callback = *Value;
   else Self->Callback.clear();
   return ERR::Okay;
}

/*********************************************************************************************************************

-FIELD-
Data: Pointer to initialisation data for the thread.

The Data field will point to a data buffer if the #SetData() method has previously been called to store data in
the thread object.  It is paired with the #DataSize field, which reflects the size of the data buffer.

*********************************************************************************************************************/

static ERR GET_Data(extThread *Self, APTR *Value, LONG *Elements)
{
   *Value = Self->Data;
   *Elements = Self->DataSize;
   return ERR::Okay;
}

/*********************************************************************************************************************

-FIELD-
DataSize: The size of the buffer referenced in the Data field.

-FIELD-
Error: Reflects the error code returned by the thread routine.

-FIELD-
Flags: Optional flags can be defined here.
Lookup: THF

-FIELD-
Routine: A function reference that will be called when the thread is started.

The routine that will be executed when the thread is activated must be specified here.  The function prototype is
`ERR routine(objThread *Thread)`.

When the routine is called, a reference to the thread object is passed as a parameter.  Once the routine has
finished processing, the resulting error code will be stored in the thread object's #Error field.

*********************************************************************************************************************/

static ERR GET_Routine(extThread *Self, FUNCTION **Value)
{
   if (Self->Routine.defined()) {
      *Value = &Self->Routine;
      return ERR::Okay;
   }
   else return ERR::FieldNotSet;
}

static ERR SET_Routine(extThread *Self, FUNCTION *Value)
{
   if (Value) Self->Routine = *Value;
   else Self->Routine.clear();
   return ERR::Okay;
}

/*********************************************************************************************************************
-FIELD-
StackSize: The stack size to allocate for the thread.

The initial size of the thread's stack can be modified by setting this field.  It may also be read prior to activation
in order to check the default stack size.  Changes to the stack size when the thread is active will have no effect.

On some platforms it may not be possible to preset the stack size and the provided value will be ignored.
-END-
*********************************************************************************************************************/

#include "class_thread_def.c"

static const FieldArray clFields[] = {
   { "Data",      FDF_ARRAY|FDF_BYTE|FDF_R, GET_Data },
   { "DataSize",  FD_LONG|FDF_R },
   { "StackSize", FDF_LONG|FDF_RW },
   { "Error",     FDF_LONG|FDF_R },
   { "Flags",     FDF_LONG|FDF_RI, NULL, NULL, &clThreadFlags },
   // Virtual fields
   { "Callback",  FDF_FUNCTIONPTR|FDF_RW, GET_Callback, SET_Callback },
   { "Routine",   FDF_FUNCTIONPTR|FDF_RW, GET_Routine, SET_Routine },
   END_FIELD
};

//********************************************************************************************************************

extern ERR add_thread_class(void)
{
   glThreadClass = objMetaClass::create::global(
      fl::ClassVersion(VER_THREAD),
      fl::Name("Thread"),
      fl::Category(CCF::SYSTEM),
      fl::Actions(clThreadActions),
      fl::Methods(clThreadMethods),
      fl::Fields(clFields),
      fl::Size(sizeof(extThread)),
      fl::Path("modules:core"));

   return glThreadClass ? ERR::Okay : ERR::AddClass;
}
