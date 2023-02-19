/*********************************************************************************************************************

The source code of the Parasol Framework is made publicly available under the terms described in the LICENSE.TXT file
that is distributed with this package.  Please refer to it for further information on licensing.

-CATEGORY-
Name: Objects
-END-

*********************************************************************************************************************/

#ifdef __unix__
#include <unistd.h>
#ifndef __ANDROID__
#include <sys/msg.h>
#endif
#endif

#include <stdlib.h>

#include "defs.h"

using namespace parasol;

#define SIZE_ACTIONBUFFER 2048

//********************************************************************************************************************
// These globals pertain to action subscriptions.  Variables are global to all threads, so need to be locked via
// glSubLock.

struct subscription {
   OBJECTPTR Object;
   ACTIONID ActionID;
   FUNCTION Callback;

   subscription(OBJECTPTR pObject, ACTIONID pAction, FUNCTION pCallback) :
      Object(pObject), ActionID(pAction), Callback(pCallback) { }
};

struct unsubscription {
   OBJECTPTR Object;
   ACTIONID ActionID;

   unsubscription(OBJECTPTR pObject, ACTIONID pAction) : Object(pObject), ActionID(pAction) { }
};

static std::recursive_mutex glSubLock; // The following variables are locked by this mutex
static std::unordered_map<OBJECTID, std::unordered_map<ACTIONID, std::vector<ActionSubscription> > > glSubscriptions;
static std::vector<unsubscription> glDelayedUnsubscribe;
static std::vector<subscription> glDelayedSubscribe;
static LONG glSubReadOnly = 0; // To prevent modification of glSubscriptions

//********************************************************************************************************************
// Deal with any un/subscriptions that occurred inside a client callback.

static void process_delayed_subs(void)
{
   if (!glDelayedSubscribe.empty()) {
      for (auto &entry : glDelayedSubscribe) {
         SubscribeAction(entry.Object, entry.ActionID, &entry.Callback);
      }
      glDelayedSubscribe.clear();
   }

   if (!glDelayedUnsubscribe.empty()) {
      for (auto &entry : glDelayedUnsubscribe) {
         UnsubscribeAction(entry.Object, entry.ActionID);
      }
      glDelayedUnsubscribe.clear();
   }
}

//********************************************************************************************************************

CSTRING action_name(OBJECTPTR Object, LONG ActionID)
{
   if (ActionID > 0) {
      if (ActionID < AC_END) return ActionTable[ActionID].Name;
      else return "Action";
   }
   else if ((Object) and (Object->Class) and (Object->Class->Methods)) {
      return Object->Class->Methods[-ActionID].Name;
   }
   else return "Method";
}

//********************************************************************************************************************
// Refer to ActionThread() to see how this is used.  It calls an action on a target object and then sends a callback
// notification via the internal message queue.

struct thread_data {
   OBJECTPTR Object;
   ACTIONID  ActionID;
   LONG      Key;
   FUNCTION  Callback;
   BYTE      Parameters;
};

static ERROR thread_action(extThread *Thread)
{
   ERROR error;
   thread_data *data = (thread_data *)Thread->Data;
   OBJECTPTR obj = data->Object;

   if (!(error = LockObject(obj, 5000))) { // Access the object and process the action.
      __sync_sub_and_fetch(&obj->ThreadPending, 1);
      error = Action(data->ActionID, obj, data->Parameters ? (data + 1) : NULL);

      if (data->Parameters) { // Free any temporary buffers that were allocated.
         if (data->ActionID > 0) local_free_args(data + 1, ActionTable[data->ActionID].Args);
         else local_free_args(data + 1, obj->Class->Methods[-data->ActionID].Args);
      }

      if (obj->defined(NF::FREE)) obj = NULL; // Clear the obj pointer because the object will be deleted on release.
      ReleaseObject(data->Object);
   }
   else {
      __sync_sub_and_fetch(&obj->ThreadPending, 1);
   }

   // Send a callback notification via messaging if required.  The receiver is in msg_threadaction() in class_thread.c

   if (data->Callback.Type) {
      ThreadActionMessage msg = {
         .Object   = obj,
         .ActionID = data->ActionID,
         .Key      = data->Key,
         .Error    = error,
         .Callback = data->Callback
      };
      SendMessage(0, MSGID_THREAD_ACTION, MSF_ADD, &msg, sizeof(msg));
   }

   threadpool_release(Thread);
   return error;
}

// Free all private memory resources tracked to an object.

static void free_private_children(OBJECTPTR Object)
{
   parasol::Log log;

   ThreadLock lock(TL_PRIVATE_MEM, 4000);
   if (lock.granted()) {
      {
         const auto children = glObjectChildren[Object->UID]; // Take an immutable copy of the resource list

         for (const auto id : children) {
            auto it = glPrivateMemory.find(id);
            if ((it IS glPrivateMemory.end()) or (!it->second.Address)) continue;
            auto &mem = it->second;

            if ((mem.Flags & MEM_DELETE) or (!mem.Object)) continue;

            if (mem.Object->OwnerID != Object->UID) {
               log.warning("Failed sanity test: Child object #%d has owner ID of #%d that does not match #%d.", mem.Object->UID, mem.Object->OwnerID, Object->UID);
               continue;
            }

            if (!mem.Object->defined(NF::UNLOCK_FREE)) {
               if (mem.Object->defined(NF::INTEGRAL)) {
                  log.warning("Found unfreed child object #%d (class %s) belonging to %s object #%d.", mem.Object->UID, ResolveClassID(mem.Object->ClassID), Object->className(), Object->UID);
               }
               acFree(mem.Object);
            }
         }
      }

      {
         const auto list = glObjectMemory[Object->UID]; // Take an immutable copy of the resource list

         for (const auto id : list) {
            auto it = glPrivateMemory.find(id);
            if ((it IS glPrivateMemory.end()) or (!it->second.Address)) continue;
            auto &mem = it->second;

            if ((mem.Flags & MEM_DELETE) or (!mem.Address)) continue;

            if (glLogLevel >= 3) {
               if (mem.Flags & MEM_STRING) {
                  log.warning("Unfreed string \"%.40s\" (%p, #%d)", (CSTRING)mem.Address, mem.Address, mem.MemoryID);
               }
               else if (mem.Flags & MEM_MANAGED) {
                  auto res = (ResourceManager **)((char *)mem.Address - sizeof(LONG) - sizeof(LONG) - sizeof(ResourceManager *));
                  if (res[0]) {
                     log.warning("Unfreed %s resource at %p.", res[0]->Name, mem.Address);
                  }
                  else log.warning("Unfreed resource at %p.", mem.Address);
               }
               else log.warning("Unfreed memory block %p, Size %d", mem.Address, mem.Size);
            }

            if (FreeResource(mem.Address) != ERR_Okay) log.warning("Error freeing tracked address %p", mem.Address);
         }
      }

      glObjectChildren.erase(Object->UID);
      glObjectMemory.erase(Object->UID);
   }
}

// Free all public memory resources tracked to this object

static void free_public_children(OBJECTPTR Object)
{
   parasol::Log log;

   if (!Object->defined(NF::HAS_SHARED_RESOURCES)) return;

   ScopedSysLock lock(PL_PUBLICMEM, 5000);
   if (lock.granted()) {
      for (LONG i=glSharedControl->NextBlock-1; i >= 0; i--) {
         if ((glSharedBlocks[i].ObjectID IS Object->UID) and (glSharedBlocks[i].MemoryID)) {
            if (glSharedBlocks[i].Flags & MEM_DELETE) continue; // Ignore blocks already marked for deletion

            log.warning("Unfreed public memory: #%d, Size %d, Object #%d, Access %d.", glSharedBlocks[i].MemoryID, glSharedBlocks[i].Size, glSharedBlocks[i].ObjectID, glSharedBlocks[i].AccessCount);
            FreeResourceID(glSharedBlocks[i].MemoryID);
         }
      }
   }
}

/*********************************************************************************************************************

-FUNCTION-
Action: This function is responsible for executing action routines.

This function is the key entry point for executing actions and method routines.  An action is a predefined function
call that can be called on any object, while a method is a function call that is specific to a class implementation.
You can find a complete list of available actions and their associated details in the Action List document.
The actions and methods supported by any class will be referenced in their auto-generated documentation.

Here are two examples that demonstrate how to make an action call.  The first performs an initialisation, which
does not require any additional arguments.  The second performs a move operation, which requires three additional
arguments to be passed to the Action() function:

<pre>
1. Action(AC_Init, Picture, NULL);

2. struct acMove move = { 30, 15, 0 };
   Action(AC_Move, Window, &move);
</pre>

In all cases, action calls in C++ can be simplified by using their corresponding helper functions:

<pre>
1.  acInit(Picture);

2a. acMove(Window, 30, 15, 0);

2b. Window->move(30, 15, 0);
</pre>

If the class of an object does not support the action ID, an error code of `ERR_NoSupport` is returned.  To test
an object to see if its class supports a particular action, use the ~CheckAction() function.

In circumstances where an object ID is known without its pointer, the use of ~ActionMsg() or ~QueueAction() may be
desirable to avoid acquiring an object lock.

-INPUT-
int(AC) Action: An action or method ID must be specified here (e.g. AC_Query).
obj Object:     A pointer to the object that is going to perform the action.
ptr Parameters: If the action or method is documented as taking parameters, point to the relevant parameter structure here.

-ERRORS-
Okay:
NullArgs:
IllegalActionID: The Action parameter is invalid.
NoAction:        The action is not supported by the object's supporting class.
ObjectCorrupt:   The object that was received is badly corrupted in a critical area.
-END-

**********************************************************************************************************************/

ERROR Action(LONG ActionID, OBJECTPTR argObject, APTR Parameters)
{
   parasol::Log log(__FUNCTION__);

   if (!argObject) return log.warning(ERR_NullArgs);

   auto obj = argObject;
   const OBJECTID object_id = obj->UID;

   obj->threadLock();

   ObjectContext new_context(obj, ActionID);

   obj->ActionDepth++;

   auto cl = obj->ExtClass;

#ifdef DEBUG
   auto log_depth = tlDepth;
#endif

   ERROR error;
   if (ActionID > 0) {
      // Action precedence is as follows:
      //
      // 1. Managed actions.
      // 2. If applicable, the object's sub-class (e.g. Picture:JPEG).
      // 3. The base-class.

      if (ActionID >= AC_END) error = log.warning(ERR_IllegalActionID);
      else if (ManagedActions[ActionID]) error = ManagedActions[ActionID](obj, Parameters);
      else if (cl->ActionTable[ActionID].PerformAction) { // Can be base or sub-class
         error = cl->ActionTable[ActionID].PerformAction(obj, Parameters);
         if (error IS ERR_NoAction) {
            if ((cl->Base) and (cl->Base->ActionTable[ActionID].PerformAction)) { // Base is set only if this is a sub-class
               error = cl->Base->ActionTable[ActionID].PerformAction(obj, Parameters);
            }
         }
      }
      else if ((cl->Base) and (cl->Base->ActionTable[ActionID].PerformAction)) { // Base is set only if this is a sub-class
         error = cl->Base->ActionTable[ActionID].PerformAction(obj, Parameters);
      }
      else error = ERR_NoAction;
   }
   else { // Method call
      if ((cl->Methods) and (cl->Methods[-ActionID].Routine)) {
         // Note that sub-classes may return ERR_NoAction if propagation to the base class is desirable.
         auto routine = (ERROR (*)(OBJECTPTR, APTR))cl->Methods[-ActionID].Routine;
         error = routine(obj, Parameters);
      }
      else error = ERR_NoAction;

      if ((error IS ERR_NoAction) and (cl->Base)) {  // If this is a child, check the base class
         if ((cl->Base->Methods) and (cl->Base->Methods[-ActionID].Routine)) {
            auto routine = (ERROR (*)(OBJECTPTR, APTR))cl->Base->Methods[-ActionID].Routine;
            error = routine(obj, Parameters);
         }
      }
   }

   // If the object has action subscribers, check if any of them are listening to this particular action, and if so, notify them.

   if (error & ERF_Notified) {
      error &= ~ERF_Notified;
   }
   else if ((ActionID > 0) and (obj->NotifyFlags[ActionID>>5] & (1<<(ActionID & 31)))) {
      std::lock_guard<std::recursive_mutex> lock(glSubLock);

      glSubReadOnly++;

      if ((glSubscriptions.contains(object_id)) and (glSubscriptions[object_id].contains(ActionID))) {
         for (auto &list : glSubscriptions[object_id][ActionID]) {
            parasol::SwitchContext ctx(list.Context);
            list.Callback(obj, ActionID, (error IS ERR_NoAction) ? ERR_Okay : error, Parameters);
         }
      }

      glSubReadOnly--;
   }

   if (ActionID != AC_Free) obj->ActionDepth--;

   obj->threadRelease();

#ifdef DEBUG
   if (log_depth != tlDepth) {
      if (ActionID >= 0) {
         log.warning("Call to #%d.%s() failed to debranch the log correctly (%d <> %d).", object_id, ActionTable[ActionID].Name, log_depth, tlDepth);
      }
      else log.warning("Call to #%d.%s() failed to debranch the log correctly (%d <> %d).", object_id, cl->Base->Methods[-ActionID].Name, log_depth, tlDepth);
   }
#endif

   return error;
}

/*********************************************************************************************************************

-FUNCTION-
ActionList: Returns a pointer to the global action table.

This function returns an array of all actions supported by the Core, including name, arguments and structure
size.  The ID of each action is indicated by its index within the array.

The Name field specifies the name of the action.  The Args field refers to the action's argument definition structure,
which lists the argument names and their relevant types.  This is matched by the Size field, which indicates the
byte-size of the action's related argument structure.  If the action does not support arguments, the Args and Size
fields will be set to NULL.  The following illustrates two argument definition examples:

<pre>
struct FunctionField argsCopyData[] = {
   { "Destination", FD_LONG  },
   { NULL, 0 }
};

struct FunctionField argsResize[] = {
   { "Width",  FD_DOUBLE },
   { "Height", FD_DOUBLE },
   { "Depth",  FD_DOUBLE },
   { NULL, 0 }
};
</pre>

The argument types that can be used by actions are limited to those listed in the following table:

<types prefix="FD">
<type name="LONG">A 32-bit integer value ranging from -2,147,483,647 to 2,147,483,648.</>
<type name="LARGE">A 64-bit integer value.</>
<type name="PTR">A standard address space pointer.</>
<type name="STRING">A pointer to a null-terminated string.</>
<type name="DOUBLE">A 64-bit floating point value.</>
<type name="OBJECT">This flag is sometimes set in conjunction with the FD_LONG type.  It indicates that the argument refers to an object ID.</>
<type name="PTRSIZE">This argument type can only be used if it follows an FD_PTR type, and if the argument itself is intended to reflect the size of the buffer referred to by the previous FD_PTR argument.</>
<type name="RESULT">This special flag is set in conjunction with the other data-based argument types. Example: If the developer is required to supply a pointer to a LONG field in which the function will store a result, the correct argument definition will be FD_RESULT|FD_LONG|FD_PTR. To make the definition of these argument types easier, FD_PTRRESULT and FD_LONGRESULT macros are also available for use.</>
</>

-INPUT-
&array(struct(ActionTable)) Actions: A pointer to the Core's action table (struct ActionTable *) is returned. Please note that the first entry in the ActionTable list has all fields driven to NULL, because valid action ID's start from one, not zero.  The final action in the list is also terminated with NULL fields in order to indicate an end to the list.  Knowing this is helpful when scanning the list or calculating the total number of actions supported by the Core.
&arraysize Size: Total number of elements in the returned list.

*********************************************************************************************************************/

void ActionList(struct ActionTable **List, LONG *Size)
{
   if (List) *List = (struct ActionTable *)ActionTable;
   if (Size) *Size = AC_END;
}

/*********************************************************************************************************************

-FUNCTION-
ActionMsg: Execute an action or method by way of object ID.

Use ActionMsg() to execute an action where only the object ID is known.

-INPUT-
int Action: The ID of the action or method to be executed.
oid Object: The target object.
ptr Args:   The parameter structure required by Action.

-ERRORS-
Okay: The action was either executed or queued for another thread.
NullArgs:
OutOfRange:
Failed: Failed to build buffered arguments.
-END-

*********************************************************************************************************************/

ERROR ActionMsg(LONG ActionID, OBJECTID ObjectID, APTR Args)
{
   parasol::Log log(__FUNCTION__);

   if ((!ActionID) or (!ObjectID)) log.warning(ERR_NullArgs);
   if (ActionID >= AC_END) return log.warning(ERR_OutOfRange);

   OBJECTPTR object;
   if (!AccessObjectID(ObjectID, 3000, &object)) {
      ERROR error = Action(ActionID, object, Args);
      ReleaseObject(object);
      return error;
   }
   else return log.warning(ERR_AccessObject);
}

/*********************************************************************************************************************

-FUNCTION-
ActionThread: Execute an action in parallel, via a separate thread.

This function follows the same principles of execution as the Action() function, with the difference of executing the
action in parallel via a dynamically allocated thread.  Please refer to the ~Action() function for general
information on action execution.

To receive feedback of the action's completion, use the Callback parameter and supply a function.  The function
prototype for the callback routine is `callback(ACTIONID ActionID, OBJECTPTR Object, ERROR Error, LONG Key)`

It is crucial that the target object is not destroyed while the thread is executing.  Use the Callback routine to
receive notification of the thread's completion and then free the object if desired.  The callback will be processed
in the next call to ~ProcessMessages(), so as to maintain an orderly execution process within the application.

The 'Error' parameter in the callback reflects the error code returned by the action after it has been called.  Note
that if ActionThread() fails, the callback will never be executed because the thread attempt will have been aborted.

Please note that there is some overhead involved when safely initialising and executing a new thread.  This function is
at its most effective when used to perform lengthy processes such as the loading and parsing of data.

-INPUT-
int(AC) Action: An action or method ID must be specified here.
obj Object: A pointer to the object that is going to perform the action.
ptr Args: If the action or method is documented as taking parameters, point to the relevant parameter structure here.  Pre-defined parameter structures are obtained from the "system/actions.h" include file.
ptr(func) Callback: This function will be called after the thread has finished executing the action.
int Key: An optional key value to be passed to the callback routine.

-ERRORS-
Okay
NullArgs
IllegalMethodID
MissingClass
NewObject
Init
-END-

*********************************************************************************************************************/

ERROR ActionThread(ACTIONID ActionID, OBJECTPTR Object, APTR Parameters, FUNCTION *Callback, LONG Key)
{
   parasol::Log log(__FUNCTION__);

   if ((!ActionID) or (!Object)) return ERR_NullArgs;

   log.traceBranch("Action: %d, Object: %d, Parameters: %p, Callback: %p, Key: %d", ActionID, Object->UID, Parameters, Callback, Key);

   __sync_add_and_fetch(&Object->ThreadPending, 1);

   ERROR error;
   extThread *thread = NULL;
   if (!(error = threadpool_get(&thread))) {
      // Prepare the parameter buffer for passing to the thread routine.

      LONG argssize;
      BYTE call_data[sizeof(thread_data) + SIZE_ACTIONBUFFER];
      BYTE free_args = FALSE;
      const FunctionField *args = NULL;

      if (Parameters) {
         if (ActionID > 0) {
            args = ActionTable[ActionID].Args;
            if ((argssize = ActionTable[ActionID].Size) > 0) {
               if (!(error = local_copy_args(args, argssize, (BYTE *)Parameters, call_data + sizeof(thread_data), SIZE_ACTIONBUFFER, &argssize, ActionTable[ActionID].Name))) {
                  free_args = TRUE;
               }

               argssize += sizeof(thread_data);
            }
            else argssize = sizeof(thread_data);
         }
         else if (auto cl = Object->ExtClass) {
            if ((-ActionID) < cl->TotalMethods) {
               args = cl->Methods[-ActionID].Args;
               if ((argssize = cl->Methods[-ActionID].Size) > 0) {
                  if (!(error = local_copy_args(args, argssize, (BYTE *)Parameters, call_data + sizeof(thread_data), SIZE_ACTIONBUFFER, &argssize, cl->Methods[-ActionID].Name))) {
                     free_args = TRUE;
                  }
               }
               else log.trace("Ignoring parameters provided for method %s", cl->Methods[-ActionID].Name);

               argssize += sizeof(thread_data);
            }
            else error = log.warning(ERR_IllegalMethodID);
         }
         else error = log.warning(ERR_MissingClass);
      }
      else argssize = sizeof(thread_data);

      // Execute the thread that will call the action.  Refer to thread_action() for the routine.

      if (!error) {
         thread->Routine = make_function_stdc(thread_action);

         auto call = (thread_data *)call_data;
         call->Object   = Object;
         call->ActionID = ActionID;
         call->Key      = Key;
         call->Parameters = Parameters ? TRUE : FALSE;
         if (Callback) call->Callback = *Callback;
         else call->Callback.Type = 0;

         struct thSetData setdata = {
            .Data = call,
            .Size = argssize
         };
         Action(MT_ThSetData, thread, &setdata);

         error = Action(AC_Activate, thread, NULL);
      }

      if (error) {
         threadpool_release(thread);
         if (free_args) local_free_args(call_data + sizeof(thread_data), args);
      }
   }
   else error = ERR_NewObject;

   if (error) __sync_sub_and_fetch(&Object->ThreadPending, 1);

   return error;
}

/*********************************************************************************************************************

-FUNCTION-
CheckAction: Checks objects to see whether or not they support certain actions.

This function checks if an object's class supports a given action or method ID.  For instance:

<pre>
if (!CheckAction(pic, AC_Query)) {
   // The Query action is supported.
}
</pre>

-INPUT-
obj Object: The target object.
int Action: A registered action ID.

-ERRORS-
Okay: The object supports the specified action.
False: The action is not supported.
NullArgs:
LostClass: The object has lost its class reference (object corrupt).

*********************************************************************************************************************/

ERROR CheckAction(OBJECTPTR Object, LONG ActionID)
{
   parasol::Log log(__FUNCTION__);

   if ((Object) and (ActionID)) {
      if (Object->ClassID IS ID_METACLASS) {
         if (((extMetaClass *)Object)->ActionTable[ActionID].PerformAction) return ERR_Okay;
         else return ERR_False;
      }
      else if (auto cl = Object->ExtClass) {
         if (cl->ActionTable[ActionID].PerformAction) return ERR_Okay;
         else if (cl->Base) {
            if (cl->Base->ActionTable[ActionID].PerformAction) return ERR_Okay;
         }
         return ERR_False;
      }
      else return log.warning(ERR_LostClass);
   }
   else return log.warning(ERR_NullArgs);
}

/*********************************************************************************************************************

-FUNCTION-
GetActionMsg: Returns a message structure if called from an action that was executed by the message system.

This function is for use by action and method support routines only.  It will return a Message structure if the
action currently under execution has been called directly from the ~ProcessMessages() function.  In all other
cases a NULL pointer is returned.

The Message structure reflects the contents of a standard ~GetMessage() call.  Of particular interest may be
the Time field, which indicates the time-stamp at which the action message was originally sent to the object.

-RESULT-
resource(Message): A Message structure is returned if the function is called in valid circumstances, otherwise NULL.  The Message structure's fields are described in the ~GetMessage() function.

*********************************************************************************************************************/

Message * GetActionMsg(void)
{
   if (auto obj = tlContext->resource()) {
      if (obj->defined(NF::MESSAGE) and (obj->ActionDepth IS 1)) {
         return tlCurrentMsg;
      }
   }
   return NULL;
}

/*********************************************************************************************************************

-FUNCTION-
NotifySubscribers: Used to send notification messages to action subscribers.

This function can be used by classes that need total control over notification management.  The system default for
notifying action subscribers is to call them immediately after an action has taken place.  This may be inconvenient
if the code for an action needs to perform a procedure after the subscribers have been notified.  Using
NotifySubscribers() allows for such a scenario.  Another possible use is customising the parameter values of the
called action so that the original values are not sent to the subscriber(s).

NOTE: Calling NotifySubscribers() does nothing to prevent the core from sending out an action notification as it
normally would, thus causing duplication.  To prevent this scenario, you must logical-or the return code of your
action support function with `ERF_Notified`, e.g. `ERR_Okay|ERF_Notified`.

-INPUT-
obj Object: Pointer to the object that is to receive the notification message.
int(AC) Action: The action ID for notification.
ptr Args: Pointer to an action parameter structure that is relevant to the ActionID.
error Error: The error code that is associated with the action result.

-END-

*********************************************************************************************************************/

void NotifySubscribers(OBJECTPTR Object, LONG ActionID, APTR Parameters, ERROR ErrorCode)
{
   parasol::Log log(__FUNCTION__);

   // No need for prv_access() since this function is called from within class action code only.

   if (!Object) { log.warning(ERR_NullArgs); return; }
   if ((ActionID <= 0) or (ActionID >= AC_END)) { log.warning(ERR_Args); return; }

   if (!(Object->NotifyFlags[ActionID>>5] & (1<<(ActionID & 31)))) return;

   const std::lock_guard<std::recursive_mutex> lock(glSubLock);
   glSubReadOnly++;

   if ((!glSubscriptions[Object->UID].empty()) and (!glSubscriptions[Object->UID][ActionID].empty())) {
      for (auto &sub : glSubscriptions[Object->UID][ActionID]) {
         if (sub.Context) {
            parasol::SwitchContext ctx(sub.Context);
            sub.Callback(Object, ActionID, ErrorCode, Parameters);
         }
      }

      process_delayed_subs();
   }
   else {
      log.warning("Unstable subscription flags discovered for object #%d, action %d: %.8x %.8x", Object->UID, ActionID, Object->NotifyFlags[0], Object->NotifyFlags[1]);
      __atomic_and_fetch(&Object->NotifyFlags[ActionID>>5], ~(1<<(ActionID & 31)), __ATOMIC_RELAXED);
   }

   glSubReadOnly--;
}

/*********************************************************************************************************************

-FUNCTION-
QueueAction: Delay the execution of an action by adding the call to the message queue.

Use QueueAction() to execute an action by way of the local message queue.  This means that the supplied Action and
the Args will be bundled into a message that will be placed in the queue.  This function then returns immediately.

The action will be executed on the next cycle of ~ProcessMessages() in line with the FIFO order of queued messages.

-INPUT-
int Action: The ID of an action or method to execute.
oid Object: The target object.
ptr Args:   The relevant argument structure for the Action, or NULL if not required.

-ERRORS-
Okay:
NullArgs:
OutOfRange: The ActionID is invalid.
NoMatchingObject:
MissingClass:
Failed:
IllegalMethodID:
-END-

*********************************************************************************************************************/

ERROR QueueAction(LONG ActionID, OBJECTID ObjectID, APTR Args)
{
   parasol::Log log(__FUNCTION__);

   if ((!ActionID) or (!ObjectID)) log.warning(ERR_NullArgs);
   if (ActionID >= AC_END) return log.warning(ERR_OutOfRange);

   struct msgAction {
      ActionMessage Action;
      BYTE Buffer[SIZE_ACTIONBUFFER];
   } msg;

   msg.Action = {
      .ObjectID = ObjectID,
      .Time     = 0,
      .ActionID = ActionID,
      .SendArgs = false
   };

   const FunctionField *fields = NULL;
   LONG msgsize = 0;

   if (Args) {
      LONG argssize = 0;
      if (ActionID > 0) {
         if (ActionTable[ActionID].Size) {
            fields   = ActionTable[ActionID].Args;
            argssize = ActionTable[ActionID].Size;
            WORD waitresult;
            if (copy_args(fields, argssize, (BYTE *)Args, msg.Buffer, SIZE_ACTIONBUFFER, &msgsize, &waitresult, ActionTable[ActionID].Name) != ERR_Okay) {
               log.warning("Failed to buffer arguments for action \"%s\".", ActionTable[ActionID].Name);
               return ERR_Failed;
            }

            msg.Action.SendArgs = true;
         }
      }
      else {
         if (auto cl = (extMetaClass *)FindClass(GetClassID(ObjectID))) {
            if ((-ActionID) < cl->TotalMethods) {
               fields   = cl->Methods[-ActionID].Args;
               argssize = cl->Methods[-ActionID].Size;
               WORD waitresult;
               if (copy_args(fields, argssize, (BYTE *)Args, msg.Buffer, SIZE_ACTIONBUFFER, &msgsize, &waitresult, cl->Methods[-ActionID].Name) != ERR_Okay) {
                  log.warning("Failed to buffer arguments for method \"%s\".", cl->Methods[-ActionID].Name);
                  return ERR_Failed;
               }
               msg.Action.SendArgs = true;
            }
            else {
               log.warning("Illegal method ID %d executed on class %s.", ActionID, cl->ClassName);
               return ERR_IllegalMethodID;
            }
         }
         else return log.warning(ERR_MissingClass);
      }
   }

   ERROR error = SendMessage(glTaskMessageMID, MSGID_ACTION, 0, &msg.Action, msgsize + sizeof(ActionMessage));

   if (error) {
      if (ActionID > 0) {
         log.warning("Action %s on object #%d failed, SendMsg error: %s", ActionTable[ActionID].Name, ObjectID, glMessages[error]);
      }
      else log.warning("Method %d on object #%d failed, SendMsg error: %s", ActionID, ObjectID, glMessages[error]);

      if (error IS ERR_MemoryDoesNotExist) error = ERR_NoMatchingObject;
   }

   return error;
}

/*********************************************************************************************************************

-FUNCTION-
SubscribeAction: Monitor action calls made against an object.

The SubscribeAction() function provides a means for receiving an immediate notification after an action is called on
an object.  We refer to this as "action monitoring".  Action monitoring is especially useful for responding to
events in the UI and the termination of objects.

Subscriptions are context sensitive and thus owned by the caller.

The following example illustrates how to listen to a Surface object's Redimension action to be alerted to resize
events:

<pre>
auto callback = make_function_stdc(notify_resize);
SubscribeAction(surface, AC_Redimension, &callback);
</pre>

The template below illustrates how the Callback function should be constructed:

<pre>
void notify_resize(OBJECTPTR Object, ACTIONID Action, ERROR Result, APTR Parameters)
{
   auto Self = (objClassType *)CurrentContext();

   // Code here...
   if ((Result == ERR_Okay) and (Parameters)) {
      auto resize = (struct acRedimension *)Parameters;
   }
}
</pre>

The Object is the original subscription target, as-is the Action ID.  The Result is the error code that was generated
at the end of the action call.  If this is not set to `ERR_Okay`, assume that the action did not have an effect on
state.  The Parameters are the original arguments provided by the client - be aware that these can legitimately be
NULL even if an action specifies a required parameter structure.  Notice that because subscriptions are context
sensitive, we can use ~CurrentContext() to get a reference to the object that initiated the subscription.

To terminate an action subscription, use the ~UnsubscribeAction() function.  Subscriptions are not resource tracked,
so it is critical to match the original call with an unsubscription.

-INPUT-
obj Object: The target object.
int Action: The ID of the action that will be monitored.  Methods are not supported.
ptr(func) Callback: A C/C++ function to callback when the action is triggered.

-ERRORS-
Okay:
NullArgs:
Args:
OutOfRange: The Action parameter is invalid.

*********************************************************************************************************************/

ERROR SubscribeAction(OBJECTPTR Object, ACTIONID ActionID, FUNCTION *Callback)
{
   parasol::Log log(__FUNCTION__);

   if ((!Object) or (!Callback)) return log.warning(ERR_NullArgs);
   if ((ActionID < 0) or (ActionID >= AC_END)) return log.warning(ERR_OutOfRange);
   if (Callback->Type != CALL_STDC) return log.warning(ERR_Args);

   if (glSubReadOnly) {
      glDelayedSubscribe.emplace_back(Object, ActionID, *Callback);
   }
   else {
      std::lock_guard<std::recursive_mutex> lock(glSubLock);

      glSubscriptions[Object->UID][ActionID].emplace_back(Callback->StdC.Context, Callback->StdC.Routine);
      __atomic_or_fetch(&Object->NotifyFlags[ActionID>>5], 1<<(ActionID & 31), __ATOMIC_RELAXED);
   }

   return ERR_Okay;
}

/*********************************************************************************************************************

-FUNCTION-
UnsubscribeAction: Terminates action subscriptions.

The UnsubscribeAction() function will terminate subscriptions made by ~SubscribeAction().

To terminate multiple subscriptions in a single call, set the Action parameter to zero.

-INPUT-
obj Object: The object that you are unsubscribing from.
int(AC) Action: The ID of the action that will be unsubscribed, or zero for all actions.

-ERRORS-
Okay:
NullArgs:
Args:
-END-

*********************************************************************************************************************/

ERROR UnsubscribeAction(OBJECTPTR Object, ACTIONID ActionID)
{
   parasol::Log log(__FUNCTION__);

   if (!Object) return log.warning(ERR_NullArgs);
   if ((ActionID < 0) or (ActionID >= AC_END)) return log.warning(ERR_Args);

   if (glSubReadOnly) {
      glDelayedUnsubscribe.emplace_back(Object, ActionID);
      return ERR_Okay;
   }

   std::lock_guard<std::recursive_mutex> lock(glSubLock);

   if (!ActionID) { // Unsubscribe all actions associated with the subscriber.
      if (glSubscriptions.contains(Object->UID)) {
         auto subscriber = tlContext->object()->UID;
restart:
         for (auto & [action, list] : glSubscriptions[Object->UID]) {
            for (auto it = list.begin(); it != list.end(); ) {
               if (it->Context->UID IS subscriber) it = list.erase(it);
               else it++;
            }

            if (list.empty()) {
               __atomic_and_fetch(&Object->NotifyFlags[action>>5], ~(1<<(action & 31)), __ATOMIC_RELAXED);

               if ((!Object->NotifyFlags[0]) and (!Object->NotifyFlags[1])) {
                  glSubscriptions.erase(Object->UID);
                  break;
               }
               else {
                  glSubscriptions[Object->UID].erase(action);
                  goto restart;
               }
            }
         }
      }
   }
   else if ((glSubscriptions.contains(Object->UID)) and (glSubscriptions[Object->UID].contains(ActionID))) {
      auto subscriber = tlContext->object()->UID;

      auto &list = glSubscriptions[Object->UID][ActionID];
      for (auto it = list.begin(); it != list.end(); ) {
         if (it->Context->UID IS subscriber) it = list.erase(it);
         else it++;
      }

      if (list.empty()) {
         __atomic_and_fetch(&Object->NotifyFlags[ActionID>>5], ~(1<<(ActionID & 31)), __ATOMIC_RELAXED);

         if ((!Object->NotifyFlags[0]) and (!Object->NotifyFlags[1])) {
            glSubscriptions.erase(Object->UID);
         }
         else glSubscriptions[Object->UID].erase(ActionID);
      }
   }

   return ERR_Okay;
}

/*********************************************************************************************************************
** Action: Free()
*/

ERROR MGR_Free(OBJECTPTR Object, APTR Void)
{
   parasol::Log log("Free");
   extMetaClass *mc;

   Object->ActionDepth--; // See Action() regarding this

   if (!(mc = Object->ExtClass)) {
      log.trace("Object %p #%d is missing its class pointer.", Object, Object->UID);
      return log.warning(ERR_ObjectCorrupt)|ERF_Notified;
   }

   // Check to see if the object is currently locked from AccessObjectID().  If it is, we mark it for deletion so that we
   // can safely get rid of it during ReleaseObject().

   if ((Object->Locked) or (Object->ThreadPending)) {
      log.debug("Object #%d locked; marking for deletion.", Object->UID);
      Object->Flags |= NF::UNLOCK_FREE;
      return ERR_Okay|ERF_Notified;
   }

   // Return if the object is currently in the process of being freed (i.e. avoid recursion)

   if (Object->defined(NF::FREE)) {
      log.trace("Object already marked with NF::FREE.");
      return ERR_Okay|ERF_Notified;
   }

   if (Object->ActionDepth > 0) { // Free() is being called while the object itself is still in use.  This can be an issue with private objects that haven't been locked with AccessObjectID().
      log.trace("Free() attempt while object is in use.");
      if (!Object->defined(NF::COLLECT)) {
         Object->Flags |= NF::COLLECT;
         ActionMsg(AC_Free, Object->UID, NULL);
      }
      return ERR_Okay|ERF_Notified;
   }

   if (Object->ClassID IS ID_METACLASS)   log.branch("%s, Owner: %d", Object->className(), Object->OwnerID);
   else if (Object->ClassID IS ID_MODULE) log.branch("%s, Owner: %d", ((extModule *)Object)->Name, Object->OwnerID);
   else if (Object->Name[0])              log.branch("Name: %s, Owner: %d", Object->Name, Object->OwnerID);
   else log.branch("Owner: %d", Object->OwnerID);

   // If the object wants to be warned when the free process is about to be executed, it will subscribe to the
   // FreeWarning action.  The process can be aborted by returning ERR_InUse.

   if (mc->ActionTable[AC_FreeWarning].PerformAction) {
      if (mc->ActionTable[AC_FreeWarning].PerformAction(Object, NULL) IS ERR_InUse) {
         if (Object->collecting()) {
            // If the object is marked for deletion then it is not possible to avoid destruction (this prevents objects
            // from locking up the shutdown process).

            log.msg("Object will be destroyed despite being in use.");
         }
         else return ERR_InUse|ERF_Notified;
      }
   }

   if (mc->Base) { // Sub-class detected, so call the base class
      if (mc->Base->ActionTable[AC_FreeWarning].PerformAction) {
         if (mc->Base->ActionTable[AC_FreeWarning].PerformAction(Object, NULL) IS ERR_InUse) {
            if (Object->collecting()) {
               // If the object is marked for deletion then it is not possible to avoid destruction (this prevents
               // objects from locking up the shutdown process).
               log.msg("Object will be destroyed despite being in use.");
            }
            else return ERR_InUse|ERF_Notified;
         }
      }
   }

   // Mark the object as being in the free process.  The mark prevents any further access to the object via
   // AccessObjectID().  Classes may also use the flag to check if an object is in the process of being freed.

   Object->Flags = (Object->Flags|NF::FREE) & (~NF::UNLOCK_FREE);

   NotifySubscribers(Object, AC_Free, NULL, ERR_Okay);

   if (mc->ActionTable[AC_Free].PerformAction) {  // If the class that formed the object is a sub-class, we call its Free() support first, and then the base-class to clean up.
      mc->ActionTable[AC_Free].PerformAction(Object, NULL);
   }

   if (mc->Base) { // Sub-class detected, so call the base class
      if (mc->Base->ActionTable[AC_Free].PerformAction) {
         mc->Base->ActionTable[AC_Free].PerformAction(Object, NULL);
      }
   }

   if ((Object->NotifyFlags[0]) or (Object->NotifyFlags[1])) {
      const std::lock_guard<std::recursive_mutex> lock(glSubLock);
      glSubscriptions.erase(Object->UID);
   }

   // If a private child structure is present, remove it

   if (Object->ChildPrivate) {
      if (FreeResource(Object->ChildPrivate)) log.warning("Invalid ChildPrivate address %p.", Object->ChildPrivate);
      Object->ChildPrivate = NULL;
   }

   free_private_children(Object);
   free_public_children(Object);

   if (Object->defined(NF::TIMER_SUB)) {
      ThreadLock lock(TL_TIMER, 200);
      if (lock.granted()) {
         for (auto it=glTimers.begin(); it != glTimers.end(); ) {
            if (it->SubscriberID IS Object->UID) {
               log.warning("%s object #%d has an unfreed timer subscription, routine %p, interval %" PF64, mc->ClassName, Object->UID, &it->Routine, it->Interval);
               it = glTimers.erase(it);
            }
            else it++;
         }
      }
   }

   if ((mc->Base) and (mc->Base->OpenCount > 0)) mc->Base->OpenCount--; // Child detected
   if (mc->OpenCount > 0) mc->OpenCount--;

   if ((glObjectLookup) and (Object->Name[0])) { // Remove the object from the name lookup list
      ThreadLock lock(TL_OBJECT_LOOKUP, 4000);
      if (lock.granted()) remove_object_hash(Object);
   }

   // Clear the object header.  This helps to raise problems in any areas of code that may attempt to use the object
   // after it has been destroyed.
   ClearMemory(Object, sizeof(BaseClass));
   FreeResource(Object);

   return ERR_Okay|ERF_Notified; // Prevent notifications after termination.
}

/*********************************************************************************************************************
** Action: Init()
*/

ERROR MGR_Init(OBJECTPTR Object, APTR Void)
{
   parasol::Log log("Init");

   extMetaClass *cl;
   if (!(cl = Object->ExtClass)) return log.warning(ERR_LostClass);

   if (Object->ClassID != cl->BaseClassID) {
      log.warning("Cannot initialise object #%d - the Object.ClassID ($%.8x) does not match the Class.BaseClassID ($%.8x)", Object->UID, Object->ClassID, cl->BaseClassID);
      return ERR_ObjectCorrupt;
   }

   if (Object->initialised()) {  // Initialising twice does not cause an error, but send a warning and return
      log.warning(ERR_DoubleInit);
      return ERR_Okay;
   }

   if (Object->Name[0]) log.branch("Name: %s, Owner: %d", Object->Name, Object->OwnerID);
   else log.branch("Owner: %d", Object->OwnerID);

   BYTE use_subclass = FALSE;
   ERROR error = ERR_Okay;
   if (Object->SubID) {
      // For sub-classes, the base-class gets called first.  It should check the SubID in the header to determine that
      // the object is sub-classed so as to prevent it from doing 'too much' initialisation.

      if (cl->Base->ActionTable[AC_Init].PerformAction) {
         error = cl->Base->ActionTable[AC_Init].PerformAction(Object, NULL);
      }

      if (!error) {
         if (cl->ActionTable[AC_Init].PerformAction) {
            error = cl->ActionTable[AC_Init].PerformAction(Object, NULL);
         }

         if (!error) Object->Flags |= NF::INITIALISED;
      }

      return error;
   }
   else {
      // Meaning of special error codes:
      //
      // ERR_NoSupport: The source data is not recognised.  Search for a sub-class that might have better luck.  Note
      //   that in the first case we can only support classes that are already in memory.  The second part of this
      //   routine supports checking of sub-classes that aren't loaded yet.
      //
      // ERR_UseSubClass: Similar to ERR_NoSupport, but avoids scanning of sub-classes that aren't loaded in memory.

      extMetaClass * sublist[16];
      LONG sli = -1;

      while (Object->ExtClass) {
         if (Object->ExtClass->ActionTable[AC_Init].PerformAction) {
            error = Object->ExtClass->ActionTable[AC_Init].PerformAction(Object, NULL);
         }
         else error = ERR_Okay; // If no initialiser defined, auto-OK

         if (!error) {
            Object->Flags |= NF::INITIALISED;

            if (Object->ExtClass != cl) {
               // Due to the switch, increase the open count of the sub-class (see NewObject() for details on object
               // reference counting).

               log.msg("Object class switched to sub-class \"%s\".", Object->className());

               Object->ExtClass->OpenCount++;
               Object->SubID = Object->ExtClass->SubClassID;
               Object->Flags |= NF::RECLASSED; // This flag indicates that the object originally belonged to the base-class
            }

            return ERR_Okay;
         }

         if (error IS ERR_UseSubClass) {
            log.trace("Requested to use registered sub-class.");
            use_subclass = TRUE;
         }
         else if (error != ERR_NoSupport) break;

         if (sli IS -1) {
            // Initialise a list of all sub-classes already in memory for querying in sequence.
            sli = 0;
            LONG i = 0;
            extMetaClass **ptr;
            CSTRING key = NULL;
            while ((i < ARRAYSIZE(sublist)-1) and (!VarIterate(glClassMap, key, &key, (APTR *)&ptr, NULL))) {
               extMetaClass *mc = ptr[0];
               if ((Object->ClassID IS mc->BaseClassID) and (mc->BaseClassID != mc->SubClassID)) {
                  sublist[i++] = mc;
               }
            }
            sublist[i++] = NULL;
         }

         // Attempt to initialise with the next known sub-class.

         if ((Object->Class = sublist[sli++])) {
            log.trace("Attempting initialisation with sub-class '%s'", Object->className());
            Object->SubID = Object->Class->SubClassID;
         }
      }
   }

   Object->Class = cl;  // Put back the original to retain integrity
   Object->SubID = Object->Class->SubClassID;

   // If the base class and its loaded sub-classes failed, check the object for a Path field and check the data
   // against sub-classes that are not currently in memory.
   //
   // This is the only way we can support the automatic loading of sub-classes without causing undue load on CPU and
   // memory resources (loading each sub-class into memory just to check whether or not the data is supported is overkill).

   CSTRING path;
   if (use_subclass) { // If ERR_UseSubClass was set and the sub-class was not registered, do not call IdentifyFile()
      log.warning("ERR_UseSubClass was used but no suitable sub-class was registered.");
   }
   else if ((error IS ERR_NoSupport) and (!GetField(Object, FID_Path|TSTR, &path)) and (path)) {
      CLASSID classid;
      if (!IdentifyFile(path, &classid, &Object->SubID)) {
         if ((classid IS Object->ClassID) and (Object->SubID)) {
            log.msg("Searching for subclass $%.8x", Object->SubID);
            if ((Object->ExtClass = (extMetaClass *)FindClass(Object->SubID))) {
               if (Object->ExtClass->ActionTable[AC_Init].PerformAction) {
                  if (!(error = Object->ExtClass->ActionTable[AC_Init].PerformAction(Object, NULL))) {
                     log.msg("Object class switched to sub-class \"%s\".", Object->className());
                     Object->Flags |= NF::INITIALISED;
                     Object->ExtClass->OpenCount++;
                     return ERR_Okay;
                  }
               }
               else return ERR_Okay;
            }
            else log.warning("Failed to load module for class #%d.", Object->SubID);
         }
      }
      else log.warning("File '%s' does not belong to class '%s', got $%.8x.", path, Object->className(), classid);

      Object->Class = cl;  // Put back the original to retain object integrity
      Object->SubID = cl->SubClassID;
   }

   return error;
}

/*********************************************************************************************************************
** Action: Signal
*/

ERROR MGR_Signal(OBJECTPTR Object, APTR Void)
{
   Object->Flags |= NF::SIGNALLED;
   return ERR_Okay;
}
