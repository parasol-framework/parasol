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

using namespace pf;

static const LONG SIZE_ACTIONBUFFER = 2048;

//********************************************************************************************************************
// These globals pertain to action subscriptions.  Variables are global to all threads, so need to be locked via
// glSubLock.

struct subscription {
   OBJECTID ObjectID;
   ACTIONID ActionID;
   FUNCTION Callback;

   subscription(OBJECTID pObject, ACTIONID pAction, FUNCTION pCallback) :
      ObjectID(pObject), ActionID(pAction), Callback(pCallback) { }
};

struct unsubscription {
   OBJECTID ObjectID;
   ACTIONID ActionID;

   unsubscription(OBJECTID pObject, ACTIONID pAction) : ObjectID(pObject), ActionID(pAction) { }
};

static std::recursive_mutex glSubLock; // The following variables are locked by this mutex
static std::unordered_map<OBJECTID, std::unordered_map<ACTIONID, std::vector<ActionSubscription> > > glSubscriptions;
static std::vector<unsubscription> glDelayedUnsubscribe;
static std::vector<subscription> glDelayedSubscribe;
static LONG glSubReadOnly = 0; // To prevent modification of glSubscriptions

static void free_children(OBJECTPTR Object);

//********************************************************************************************************************

static ERROR object_free(BaseClass *Object)
{
   pf::Log log("Free");

   Object->threadLock();
   ObjectContext new_context(Object, AC_Free);

   auto mc = Object->ExtClass;
   if (!mc) {
      log.trace("Object %p #%d is missing its class pointer.", Object, Object->UID);
      Object->threadRelease();
      return ERR_Okay;
   }

   // Check to see if the object is currently locked from AccessObject().  If it is, we mark it for deletion so that we
   // can safely get rid of it during ReleaseObject().

   if ((Object->Locked) or (Object->ThreadPending)) {
      log.debug("Object #%d locked; marking for deletion.", Object->UID);
      Object->Flags |= NF::UNLOCK_FREE;
      Object->threadRelease();
      return ERR_InUse;
   }

   // Return if the object is currently in the process of being freed (i.e. avoid recursion)

   if (Object->defined(NF::FREE)) {
      log.trace("Object already marked with NF::FREE.");
      Object->threadRelease();
      return ERR_InUse;
   }

   if (Object->ActionDepth > 0) {
      // Free() is being called while the object itself is still in use.
      // This can be an issue with objects that haven't been locked with AccessObject().
      log.trace("Free() attempt while object is in use.");
      if (!Object->defined(NF::COLLECT)) {
         Object->Flags |= NF::COLLECT;
         SendMessage(0, MSGID_FREE, 0, &Object->UID, sizeof(OBJECTID));
      }
      Object->threadRelease();
      return ERR_InUse;
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
         else {
            Object->threadRelease();
            return ERR_InUse;
         }
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
            else {
               Object->threadRelease();
               return ERR_InUse;
            }
         }
      }
   }

   // Mark the object as being in the free process.  The mark prevents any further access to the object via
   // AccessObject().  Classes may also use the flag to check if an object is in the process of being freed.

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

   free_children(Object);

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

   if (Object->Name[0]) { // Remove the object from the name lookup list
      ThreadLock lock(TL_OBJECT_LOOKUP, 4000);
      if (lock.granted()) remove_object_hash(Object);
   }

   // Clear the object header.  This helps to raise problems in any areas of code that may attempt to use the object
   // after it has been destroyed.

   Object->Class   = NULL;
   Object->ClassID = 0;
   Object->UID     = 0;
   return ERR_Okay;
}

static ResourceManager glResourceObject = {
   "Object",
   (ERROR (*)(APTR))&object_free
};

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

static void free_children(OBJECTPTR Object)
{
   pf::Log log;

   ThreadLock lock(TL_PRIVATE_MEM, 4000);
   if (lock.granted()) {
      if (!glObjectChildren[Object->UID].empty()) {
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
               FreeResource(mem.Object);
            }
         }
      }

      if (!glObjectMemory[Object->UID].empty()) {
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
                  if (res[0]) log.warning("Unfreed %s resource at %p.", res[0]->Name, mem.Address);
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

/*********************************************************************************************************************

-FUNCTION-
Action: This function is responsible for executing action routines.

This function is the key entry point for executing actions and method routines.  An action is a predefined function
call that can be called on any object, while a method is a function call that is specific to a class implementation.
You can find a complete list of available actions and their associated details in the Action List document.
The actions and methods supported by any class will be referenced in their auto-generated documentation.

Here are two examples that demonstrate how to make an action call.  The first performs an activation, which
does not require any additional arguments.  The second performs a move operation, which requires three additional
arguments to be passed to the Action() function:

<pre>
1. Action(AC_Activate, Picture, NULL);

2. struct acMove move = { 30, 15, 0 };
   Action(AC_Move, Window, &move);
</pre>

In all cases, action calls in C++ can be simplified by using their corresponding helper functions:

<pre>
1.  acActivate(Picture);

2a. acMove(Window, 30, 15, 0);

2b. Window->move(30, 15, 0);
</pre>

If the class of an object does not support the action ID, an error code of `ERR_NoSupport` is returned.  To test
an object to see if its class supports an action, use the ~CheckAction() function.

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

ERROR Action(LONG ActionID, OBJECTPTR Object, APTR Parameters)
{
   if (!Object) return ERR_NullArgs;

   Object->threadLock();
   ObjectContext new_context(Object, ActionID);
   Object->ActionDepth++;
   auto cl = Object->ExtClass;

#ifdef DEBUG
   auto log_depth = tlDepth;
#endif

   ERROR error;
   if (ActionID >= 0) {
      if (cl->ActionTable[ActionID].PerformAction) { // Can be a base-class or sub-class call
         error = cl->ActionTable[ActionID].PerformAction(Object, Parameters);
         if (error IS ERR_NoAction) {
            if ((cl->Base) and (cl->Base->ActionTable[ActionID].PerformAction)) { // Base is set only if this is a sub-class
               error = cl->Base->ActionTable[ActionID].PerformAction(Object, Parameters);
            }
         }
      }
      else if ((cl->Base) and (cl->Base->ActionTable[ActionID].PerformAction)) { // Base is set only if this is a sub-class
         error = cl->Base->ActionTable[ActionID].PerformAction(Object, Parameters);
      }
      else error = ERR_NoAction;
   }
   else { // Method call
      if ((cl->Methods) and (cl->Methods[-ActionID].Routine)) {
         // Note that sub-classes may return ERR_NoAction if propagation to the base class is desirable.
         auto routine = (ERROR (*)(OBJECTPTR, APTR))cl->Methods[-ActionID].Routine;
         error = routine(Object, Parameters);
      }
      else error = ERR_NoAction;

      if ((error IS ERR_NoAction) and (cl->Base)) {  // If this is a child, check the base class
         if ((cl->Base->Methods) and (cl->Base->Methods[-ActionID].Routine)) {
            auto routine = (ERROR (*)(OBJECTPTR, APTR))cl->Base->Methods[-ActionID].Routine;
            error = routine(Object, Parameters);
         }
      }
   }

   // If the object has action subscribers, check if any of them are listening to this particular action, and if so, notify them.

   if (error & ERF_Notified) {
      error &= ~ERF_Notified;
   }
   else if ((ActionID > 0) and (Object->NotifyFlags[ActionID>>5] & (1<<(ActionID & 31)))) {
      std::lock_guard<std::recursive_mutex> lock(glSubLock);

      glSubReadOnly++;

      if ((glSubscriptions.contains(Object->UID)) and (glSubscriptions[Object->UID].contains(ActionID))) {
         for (auto &list : glSubscriptions[Object->UID][ActionID]) {
            pf::SwitchContext ctx(list.Context);
            list.Callback(Object, ActionID, (error IS ERR_NoAction) ? ERR_Okay : error, Parameters);
         }
      }

      glSubReadOnly--;
   }

   Object->ActionDepth--;
   Object->threadRelease();

#ifdef DEBUG
   if (log_depth != tlDepth) {
      pf::Log log(__FUNCTION__);
      if (ActionID >= 0) {
         log.warning("Call to #%d.%s() failed to debranch the log correctly (%d <> %d).", Object->UID, ActionTable[ActionID].Name, log_depth, tlDepth);
      }
      else log.warning("Call to #%d.%s() failed to debranch the log correctly (%d <> %d).", Object->UID, cl->Base->Methods[-ActionID].Name, log_depth, tlDepth);
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

Use ActionMsg() to execute an action when only the object ID is known.

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
   OBJECTPTR object;
   if (auto error = AccessObject(ObjectID, 3000, &object); !error) {
      error = Action(ActionID, object, Args);
      ReleaseObject(object);
      return error;
   }
   else {
      pf::Log log(__FUNCTION__);
      return log.warning(error);
   }
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
   pf::Log log(__FUNCTION__);

   if ((!ActionID) or (!Object)) return ERR_NullArgs;

   log.traceBranch("Action: %d, Object: %d, Parameters: %p, Callback: %p, Key: %d", ActionID, Object->UID, Parameters, Callback, Key);

   __sync_add_and_fetch(&Object->ThreadPending, 1);

   ERROR error;
   extThread *thread = NULL;
   if (!(error = threadpool_get(&thread))) {
      // Prepare the parameter buffer for passing to the thread routine.

      LONG argssize;
      BYTE call_data[sizeof(thread_data) + SIZE_ACTIONBUFFER];
      bool free_args = false;
      const FunctionField *args = NULL;

      if (Parameters) {
         if (ActionID > 0) {
            args = ActionTable[ActionID].Args;
            if ((argssize = ActionTable[ActionID].Size) > 0) {
               if (!(error = copy_args(args, argssize, (BYTE *)Parameters, call_data + sizeof(thread_data), SIZE_ACTIONBUFFER, &argssize, ActionTable[ActionID].Name))) {
                  free_args = true;
               }

               argssize += sizeof(thread_data);
            }
            else argssize = sizeof(thread_data);
         }
         else if (auto cl = Object->ExtClass) {
            if ((-ActionID) < cl->TotalMethods) {
               args = cl->Methods[-ActionID].Args;
               if ((argssize = cl->Methods[-ActionID].Size) > 0) {
                  if (!(error = copy_args(args, argssize, (BYTE *)Parameters, call_data + sizeof(thread_data), SIZE_ACTIONBUFFER, &argssize, cl->Methods[-ActionID].Name))) {
                     free_args = true;
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
         call->Parameters = Parameters ? true : false;
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
   pf::Log log(__FUNCTION__);

   if (Object) {
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
CheckObjectExists: Checks if a particular object is still available in the system.

The CheckObjectExists() function verifies the presence of any object created by ~NewObject().

-INPUT-
oid Object: The object identity to verify.

-ERRORS-
True:  The object exists.
False: The object ID does not exist.
LockFailed:

*********************************************************************************************************************/

ERROR CheckObjectExists(OBJECTID ObjectID)
{
   ThreadLock lock(TL_PRIVATE_MEM, 4000);
   if (lock.granted()) {
      LONG result = ERR_False;
      auto mem = glPrivateMemory.find(ObjectID);
      if ((mem != glPrivateMemory.end()) and (mem->second.Object)) {
         if (mem->second.Object->defined(NF::UNLOCK_FREE));
         else result = ERR_True;
      }
      return result;
   }
   else {
      pf::Log log(__FUNCTION__);
      return log.warning(ERR_LockFailed);
   }
}

/*********************************************************************************************************************

-FUNCTION-
CurrentContext: Returns a pointer to the object that has the current context.

This function returns a pointer to the object that has the current context.  Context is primarily used to manage
resource allocations.  Manipulating the context is sometimes necessary to ensure that a resource is tracked to
the correct object.

To get the parent context (technically the 'context of the current context'), use GetParentContext(), which is
implemented as a macro.  This is used in method and action routines where the context of the object's caller may be
needed.

-RESULT-
obj: Returns an object pointer (of which the Task has exclusive access to).  Cannot return NULL except in the initial start-up and late shut-down sequence of the Core.

*********************************************************************************************************************/

OBJECTPTR CurrentContext(void)
{
   return tlContext->object();
}

/*********************************************************************************************************************

-FUNCTION-
FindClass: Returns all class objects for a given class ID.

This function will find a specific class by ID and return its @MetaClass.  If the class is not in memory, the internal
dictionary is checked to discover a module binary registered with that ID.  If this succeeds, the module is loaded
into memory and the class will be returned.  In any event of failure, NULL is returned.

If the ID of a named class is not known, call ~ResolveClassName() first and pass the resulting ID to this function.

-INPUT-
cid ClassID: A class ID such as one retrieved from ~ResolveClassName().

-RESULT-
obj(MetaClass): Returns a pointer to the MetaClass structure that has been found as a result of the search, or NULL if no matching class was found.

*********************************************************************************************************************/

objMetaClass * FindClass(CLASSID ClassID)
{
   auto it = glClassMap.find(ClassID);
   if (it != glClassMap.end()) return it->second;

   if (glProgramStage IS STAGE_SHUTDOWN) return NULL; // No new module loading during shutdown

   // Class is not loaded.  Try and find a master in the dictionary.  If we find one, we can
   // initialise the module and then find the new Class.
   //
   // Note: Children of the class are not automatically loaded into memory if they are unavailable at the time.  Doing so
   // would result in lost CPU and memory resources due to loading code that may not be needed.

   pf::Log log(__FUNCTION__);
   if (glClassDB.contains(ClassID)) {
      auto &path = glClassDB[ClassID].Path;

      if (!path.empty()) {
         // Load the module from the associated location and then find the class that it contains.  If the module fails,
         // we keep on looking for other installed modules that may handle the class.

         log.branch("Attempting to load module \"%s\" for class $%.8x.", path.c_str(), ClassID);

         objModule::create mod = { fl::Name(path) };
         if (mod.ok()) {
            it = glClassMap.find(ClassID);
            if (it != glClassMap.end()) return it->second;
            else log.warning("Module \"%s\" did not configure class \"%s\"", path.c_str(), glClassDB[ClassID].Name.c_str());
         }
         else log.warning("Failed to load module \"%s\"", path.c_str());
      }
      else log.warning("No module path defined for class \%s\"", glClassDB[ClassID].Name.c_str());
   }
   else log.warning("Could not find class $%.8x in memory or in dictionary.", ClassID);

   return NULL;
}

/*********************************************************************************************************************

-FUNCTION-
FindObject: Searches for objects by name.

The FindObject() function searches for all objects that match a given name and can filter by class.

The following example is a typical illustration of this function's use.  It finds the most recent object created
with a given name:

<pre>
OBJECTID id;
FindObject("SystemPointer", ID_POINTER, 0, &id);
</pre>

If FindObject() cannot find any matching objects then it will return an error code.

-INPUT-
cstr Name:      The name of an object to search for.
cid ClassID:    Optional.  Set to a class ID to filter the results down to a specific class type.
int(FOF) Flags: Optional flags.
&oid ObjectID:  An object id variable for storing the result.

-ERRORS-
Okay: At least one matching object was found and stored in the ObjectID.
Args:
Search: No objects matching the given name could be found.
LockFailed:
EmptyString:
DoesNotExist:
-END-

*********************************************************************************************************************/

ERROR FindObject(CSTRING InitialName, CLASSID ClassID, LONG Flags, OBJECTID *Result)
{
   pf::Log log(__FUNCTION__);

   if ((!Result) or (!InitialName)) return ERR_NullArgs;
   if (!InitialName[0]) return log.warning(ERR_EmptyString);

   if (Flags & FOF_SMART_NAMES) {
      // If an integer based name (defined by #num) is passed, we translate it to an ObjectID rather than searching for
      // an object of name "#1234".

      bool number = false;
      if (InitialName[0] IS '#') number = true;
      else {
         // If the name consists entirely of numbers, it must be considered an object ID (we can make this check because
         // it is illegal for a name to consist entirely of digits).

         LONG i = (InitialName[0] IS '-') ? 1 : 0;
         for (; InitialName[i]; i++) {
            if (InitialName[i] < '0') break;
            if (InitialName[i] > '9') break;
         }
         if (!InitialName[i]) number = true;
      }

      if (number) {
         if (auto objectid = (OBJECTID)StrToInt(InitialName)) {
            if (!CheckObjectExists(objectid)) {
               *Result = objectid;
               return ERR_Okay;
            }
            else return ERR_Search;
         }
         else return ERR_Search;
      }

      if (!StrMatch("owner", InitialName)) {
         if ((tlContext != &glTopContext) and (tlContext->object()->OwnerID)) {
            if (!CheckObjectExists(tlContext->object()->OwnerID)) {
               *Result = tlContext->object()->OwnerID;
               return ERR_Okay;
            }
            else return ERR_DoesNotExist;
         }
         else return ERR_DoesNotExist;
      }
   }

   ThreadLock lock(TL_OBJECT_LOOKUP, 4000);
   if (lock.granted()) {
      if (glObjectLookup.contains(InitialName)) {
         auto &list = glObjectLookup[InitialName];
         if (!ClassID) {
            *Result = list.back()->UID;
            return ERR_Okay;
         }

         for (auto it=list.rbegin(); it != list.rend(); it++) {
            auto obj = *it;
            if (obj->ClassID IS ClassID) {
               *Result = obj->UID;
               return ERR_Okay;
            }
         }
      }
   }

   return ERR_Search;
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
GetClassID: Returns the class ID of an ID-referenced object.

Call this function with any valid object ID to learn the identifier for its base class.  This is the quickest way to
retrieve the class of an object without having to gain exclusive access to the object first.

Note that if the object's pointer is already known, the quickest way to learn of its class is to read the ClassID
field in the object header.

-INPUT-
oid Object: The object to be examined.

-RESULT-
cid: Returns the base class ID of the object or NULL if failure.

*********************************************************************************************************************/

CLASSID GetClassID(OBJECTID ObjectID)
{
   if (auto object = GetObjectPtr(ObjectID)) return object->ClassID;
   else return 0;
}

/*********************************************************************************************************************

-FUNCTION-
GetObjectPtr: Returns a direct pointer for any object ID.

This function translates object ID's to their respective address pointers.

-INPUT-
oid Object: The ID of the object to lookup.

-RESULT-
obj: The address of the object is returned, or NULL if the ID does not relate to an object.

*********************************************************************************************************************/

OBJECTPTR GetObjectPtr(OBJECTID ObjectID)
{
   ThreadLock lock(TL_PRIVATE_MEM, 4000);
   if (lock.granted()) {
      if (auto mem = glPrivateMemory.find(ObjectID); mem != glPrivateMemory.end()) {
         if ((mem->second.Flags & MEM_OBJECT) and (mem->second.Object)) {
            if (mem->second.Object->UID IS ObjectID) {
               return mem->second.Object;
            }
         }
      }
   }

   return NULL;
}

/*********************************************************************************************************************

-FUNCTION-
GetOwnerID: Returns the unique ID of an object's owner.

This function returns an identifier for the owner of any valid object.  This is the fastest way to retrieve the
owner of an object if only the ID is known.

If the object address is already known then the fastest means of retrieval is via the ownerID() C++ class method.

-INPUT-
oid Object: The ID of an object to query.

-RESULT-
oid: Returns the ID of the object's owner.  If the object does not have a owner (i.e. if it is untracked) or if the provided ID is invalid, this function will return NULL.

*********************************************************************************************************************/

OBJECTID GetOwnerID(OBJECTID ObjectID)
{
   ThreadLock lock(TL_PRIVATE_MEM, 4000);
   if (lock.granted()) {
      if (auto mem = glPrivateMemory.find(ObjectID); mem != glPrivateMemory.end()) {
         if (mem->second.Object) return mem->second.Object->OwnerID;
      }
   }
   return 0;
}

/*********************************************************************************************************************

-FUNCTION-
InitObject: Initialises an object so that it is ready for use.

This function initialises objects so that they can be used for their intended purpose. The process of initialisation
is compulsory, and a client may not use any other actions on an object until it has been initialised.  Exceptions to
this rule only apply to the GetVar() and SetVar() actions.

If the initialisation of an object fails due to a support problem (for example, if a PNG @Picture object attempts to
load a JPEG file), the initialiser will search for a sub-class that can handle the data.  If a sub-class that can
provide ample support exists, a partial transfer of ownership will occur and the object's  management will be shared
between both the base class and the sub-class.

If an object does not support the data or its configuration, an error code of `ERR_NoSupport` will be returned.
Other appropriate error codes can be returned if initialisation fails.

-INPUT-
obj Object: The object to initialise.

-ERRORS-
Okay: The object was initialised.
LostClass
ObjectCorrupt

*********************************************************************************************************************/

ERROR InitObject(OBJECTPTR Object)
{
   pf::Log log("Init");

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

   Object->threadLock();
   ObjectContext new_context(Object, AC_Init);

   bool use_subclass = false;
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

      Object->threadRelease();
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

      std::array<extMetaClass *, 16> sublist;
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

            Object->threadRelease();
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
            for (auto & [ id, class_ptr ] : glClassMap) {
               if (i >= LONG(sublist.size())-1) break;
               if ((Object->ClassID IS class_ptr->BaseClassID) and (class_ptr->BaseClassID != class_ptr->SubClassID)) {
                  sublist[i++] = class_ptr;
               }
            }

            sublist[i] = NULL;
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
                     Object->threadRelease();
                     return ERR_Okay;
                  }
               }
               else {
                  Object->threadRelease();
                  return ERR_Okay;
               }
            }
            else log.warning("Failed to load module for class #%d.", Object->SubID);
         }
      }
      else log.warning("File '%s' does not belong to class '%s', got $%.8x.", path, Object->className(), classid);

      Object->Class = cl;  // Put back the original to retain object integrity
      Object->SubID = cl->SubClassID;
   }

   Object->threadRelease();
   return error;
}

/*********************************************************************************************************************

-FUNCTION-
ListChildren: Returns a list of all children belonging to an object.

The ListChildren() function returns a list of all children belonging to an object.  The client must provide an empty
vector of ChildEntry structures to host the results, which include unique object ID's and their class identifiers.

Note that any child objects marked with the `INTEGRAL` flag will be excluded because they are private members of the
targeted object.

-INPUT-
oid Object: An object to query.
ptr(cpp(array(resource(ChildEntry)))) List: Must refer to an array of ChildEntry structures.

-ERRORS-
Okay: Zero or more children were found and listed.
Args
NullArgs

*********************************************************************************************************************/

ERROR ListChildren(OBJECTID ObjectID, pf::vector<ChildEntry> *List)
{
   pf::Log log(__FUNCTION__);

   if ((!ObjectID) or (!List)) return log.warning(ERR_NullArgs);

   log.trace("#%d, List: %p", ObjectID, List);

   ThreadLock lock(TL_PRIVATE_MEM, 4000);
   if (lock.granted()) {
      for (const auto id : glObjectChildren[ObjectID]) {
         auto mem = glPrivateMemory.find(id);
         if (mem IS glPrivateMemory.end()) continue;

         if (auto child = mem->second.Object) {
            if (!child->defined(NF::INTEGRAL)) {
               List->emplace_back(child->UID, child->ClassID);
            }
         }
      }
      return ERR_Okay;
   }
   else return ERR_LockFailed;
}

/*********************************************************************************************************************

-FUNCTION-
NewObject: Creates new objects.

The NewObject() function is used to create new objects and register them for use within the Core.  After creating
a new object, the client can proceed to set the object's field values and initialise it with #Init() so that it
can be used as intended.

The new object will be modeled according to the class blueprint indicated by ClassID.  Pre-defined class ID's are
defined in their documentation and the `parasol/system/register.h` include file.  ID's for unregistered classes can
be computed using the ~ResolveClassName() function.

A pointer to the new object will be returned in the Object parameter.  By default, object allocations are context
sensitive and will be collected when their owner is terminated.  It is possible to track an object to a different
owner by using the ~SetOwner() function.

To destroy an object, call ~FreeResource().

-INPUT-
large ClassID: A class ID from "system/register.h" or generated by ~ResolveClassName().
flags(NF) Flags:  Optional flags.
&obj Object: Pointer to an address variable that will store a reference to the new object.

-ERRORS-
Okay
NullArgs
MissingClass: The ClassID is invalid or refers to a class that is not installed.
Failed
ObjectExists: An object with the provided Name already exists in the system (applies only when the NF_UNIQUE flag has been used).
-END-

*********************************************************************************************************************/

ERROR NewObject(LARGE ClassID, NF Flags, OBJECTPTR *Object)
{
   pf::Log log(__FUNCTION__);

   auto class_id = ULONG(ClassID & 0xffffffff);
   if ((!class_id) or (!Object)) return log.warning(ERR_NullArgs);

   auto mc = (extMetaClass *)FindClass(class_id);
   if (!mc) {
      if (glClassMap.contains(class_id)) {
         log.function("Class %s was not found in the system.", glClassMap[class_id]->ClassName);
      }
      else log.function("Class $%.8x was not found in the system.", class_id);
      return ERR_MissingClass;
   }

   if (Object) *Object = NULL;

   Flags &= (NF::UNTRACKED|NF::INTEGRAL|NF::UNIQUE|NF::NAME|NF::SUPPRESS_LOG); // Very important to eliminate any internal flags.

   // If the object is integral then turn off use of the UNTRACKED flag (otherwise the child will
   // end up being tracked to its task rather than its parent object).

   if ((Flags & NF::INTEGRAL) != NF::NIL) Flags = Flags & (~NF::UNTRACKED);

   // Force certain flags on the class' behalf

   if (mc->Flags & CLF_NO_OWNERSHIP) Flags |= NF::UNTRACKED;

   if ((Flags & NF::SUPPRESS_LOG) IS NF::NIL) log.branch("%s #%d, Flags: $%x", mc->ClassName, glPrivateIDCounter, LONG(Flags));

   OBJECTPTR head = NULL;
   MEMORYID head_id;

   if (!AllocMemory(mc->Size, MEM_MANAGED|MEM_OBJECT|MEM_NO_LOCK|(((Flags & NF::UNTRACKED) != NF::NIL) ? MEM_UNTRACKED : 0), (APTR *)&head, &head_id)) {
      set_memory_manager(head, &glResourceObject);

      head->UID     = head_id;
      head->ClassID = mc->BaseClassID;
      head->Class   = (extMetaClass *)mc;
      head->Flags   = Flags;

      if (mc->BaseClassID IS mc->SubClassID) head->SubID = 0; // Object derived from a base class
      else head->SubID = mc->SubClassID; // Object derived from a sub-class

      // Tracking for our new object is configured here.

      if (mc->Flags & CLF_NO_OWNERSHIP) { } // Used by classes like RootModule to avoid tracking back to the task.
      else if ((Flags & NF::UNTRACKED) != NF::NIL) {
         if (class_id IS ID_MODULE); // Untracked modules have no owner, due to the expunge process.
         else {
            // Untracked objects are owned by the current task.  This ensures that the object
            // is deallocated correctly when the Core is closed.

            if (glCurrentTask) {
               ScopedObjectAccess lock(glCurrentTask);
               SetOwner(head, glCurrentTask);
            }
         }
      }
      else if (tlContext != &glTopContext) { // Track the object to the current context
         SetOwner(head, tlContext->resource());
      }
      else if (glCurrentTask) {
         ScopedObjectAccess lock(glCurrentTask);
         SetOwner(head, glCurrentTask);
      }

      // After the header has been created we can set the context, then call the base class's NewObject() support.  If this
      // object belongs to a sub-class, we will also call its supporting NewObject() action if it has specified one.

      pf::SwitchContext context(head);

      ERROR error = ERR_Okay;
      if (mc->Base) {
         if (mc->Base->ActionTable[AC_NewObject].PerformAction) {
            if ((error = mc->Base->ActionTable[AC_NewObject].PerformAction(head, NULL))) {
               log.warning(error);
            }
         }
         else error = log.warning(ERR_NoAction);
      }

      if ((!error) and (mc->ActionTable[AC_NewObject].PerformAction)) {
         if ((error = mc->ActionTable[AC_NewObject].PerformAction(head, NULL))) {
            log.warning(error);
         }
      }

      if (!error) {
         ((extMetaClass *)head->Class)->OpenCount++;
         if (mc->Base) mc->Base->OpenCount++;

         *Object = head;
         return ERR_Okay;
      }

      FreeResource(head);
      return error;
   }
   else return ERR_AllocMemory;
}

/*********************************************************************************************************************

-FUNCTION-
NotifySubscribers: Used to send notification messages to action subscribers.

This function can be used by classes that need total control over notification management.  The system default for
notifying action subscribers is to call them immediately after an action has taken place.  This may be inconvenient
if the code for an action needs to execute code post-notification.  Using NotifySubscribers() allows these scenarios
to be addressed.  Another possible use is for customised parameter values to be sent to subscribers instead of
the original values.

NOTE: Calling NotifySubscribers() does nothing to prevent the core from sending out an action notification as it
normally would, thus causing duplication.  To prevent this the client must logical-or the return code of
the action function with `ERF_Notified`, e.g. `ERR_Okay|ERF_Notified`.

-INPUT-
obj Object: Pointer to the object that is to receive the notification message.
int(AC) Action: The action ID for notification.
ptr Args: Pointer to an action parameter structure that is relevant to the ActionID.
error Error: The error code that is associated with the action result.

-END-

*********************************************************************************************************************/

void NotifySubscribers(OBJECTPTR Object, LONG ActionID, APTR Parameters, ERROR ErrorCode)
{
   pf::Log log(__FUNCTION__);

   // No need for prv_access() since this function is called from within class action code only.

   if (!Object) { log.warning(ERR_NullArgs); return; }
   if ((ActionID <= 0) or (ActionID >= AC_END)) { log.warning(ERR_Args); return; }

   if (!(Object->NotifyFlags[ActionID>>5] & (1<<(ActionID & 31)))) return;

   const std::lock_guard<std::recursive_mutex> lock(glSubLock);

   if ((!glSubscriptions[Object->UID].empty()) and (!glSubscriptions[Object->UID][ActionID].empty())) {
      glSubReadOnly++; // Prevents changes to glSubscriptions while we're processing it.
      for (auto &sub : glSubscriptions[Object->UID][ActionID]) {
         if (sub.Context) {
            pf::SwitchContext ctx(sub.Context);
            sub.Callback(Object, ActionID, ErrorCode, Parameters);
         }
      }
      glSubReadOnly--;

      if (!glSubReadOnly) {
         if (!glDelayedSubscribe.empty()) {
            for (auto &entry : glDelayedSubscribe) {
               glSubscriptions[entry.ObjectID][entry.ActionID].emplace_back(entry.Callback.StdC.Context, entry.Callback.StdC.Routine);
            }
            glDelayedSubscribe.clear();
         }

         if (!glDelayedUnsubscribe.empty()) {
            for (auto &entry : glDelayedUnsubscribe) {
               if (Object->UID IS entry.ObjectID) UnsubscribeAction(Object, ActionID);
               else {
                  OBJECTPTR obj;
                  if (!AccessObject(entry.ObjectID, 3000, &obj)) {
                     UnsubscribeAction(obj, entry.ActionID);
                     ReleaseObject(obj);
                  }
               }
            }
            glDelayedUnsubscribe.clear();
         }
      }
   }
   else {
      log.warning("Unstable subscription flags discovered for object #%d, action %d: %.8x %.8x", Object->UID, ActionID, Object->NotifyFlags[0], Object->NotifyFlags[1]);
      __atomic_and_fetch(&Object->NotifyFlags[ActionID>>5], ~(1<<(ActionID & 31)), __ATOMIC_RELAXED);
   }
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
   pf::Log log(__FUNCTION__);

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

   LONG msgsize = 0;

   if (Args) {
      if (ActionID > 0) {
         if (ActionTable[ActionID].Size) {
            auto fields   = ActionTable[ActionID].Args;
            auto argssize = ActionTable[ActionID].Size;
            if (copy_args(fields, argssize, (BYTE *)Args, msg.Buffer, SIZE_ACTIONBUFFER, &msgsize, ActionTable[ActionID].Name) != ERR_Okay) {
               log.warning("Failed to buffer arguments for action \"%s\".", ActionTable[ActionID].Name);
               return ERR_Failed;
            }

            msg.Action.SendArgs = true;
         }
      }
      else if (auto cl = (extMetaClass *)FindClass(GetClassID(ObjectID))) {
         if (-ActionID < cl->TotalMethods) {
            auto fields   = cl->Methods[-ActionID].Args;
            auto argssize = cl->Methods[-ActionID].Size;
            if (copy_args(fields, argssize, (BYTE *)Args, msg.Buffer, SIZE_ACTIONBUFFER, &msgsize, cl->Methods[-ActionID].Name) != ERR_Okay) {
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

   ERROR error = SendMessage(0, MSGID_ACTION, 0, &msg.Action, msgsize + sizeof(ActionMessage));

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
ResolveClassName: Resolves any class name to a unique identification ID.

This function will resolve a class name to its unique ID.

Class ID's are used by functions such as ~NewObject() for fast processing.

-INPUT-
cstr Name: The name of the class that requires resolution.

-RESULT-
cid: Returns the class ID identified from the class name, or NULL if the class could not be found.
-END-

*********************************************************************************************************************/

CLASSID ResolveClassName(CSTRING ClassName)
{
   if ((!ClassName) or (!*ClassName)) {
      pf::Log log(__FUNCTION__);
      log.warning(ERR_NullArgs);
      return 0;
   }

   CLASSID cid = StrHash(ClassName, FALSE);
   if (glClassDB.contains(cid)) return cid;
   else return 0;
}

/*********************************************************************************************************************

-FUNCTION-
ResolveClassID: Converts a valid class ID to its equivalent name.

This function will resolve a valid class ID to its equivalent name.  The name is resolved by scanning the
class database, so the class must be registered in the database for this function to return successfully.
Registration is achieved by loading the module that hosts the class, after which the class is permanently saved
in the database.

-INPUT-
cid ID: The ID of the class that needs to be resolved.

-RESULT-
cstr: Returns the name of the class, or NULL if the ID is not recognised.  Standard naming conventions apply, so it can be expected that the string is capitalised and without spaces, e.g. "NetSocket".
-END-

*********************************************************************************************************************/

CSTRING ResolveClassID(CLASSID ID)
{
   if (glClassDB.contains(ID)) return glClassDB[ID].Name.c_str();

   pf::Log log(__FUNCTION__);
   log.warning("Failed to resolve ID $%.8x", ID);
   return NULL;
}

/*********************************************************************************************************************

-FUNCTION-
SetOwner: Changes object ownership dynamically.

This function changes the ownership of an existing object.  Ownership is an attribute that affects an object's
placement within the object hierarchy as well as impacting on the resource tracking of the object in question.
Internally, setting a new owner will cause three things to happen:

<list type="sorted">
<li>The new owner's class will receive notification via the #NewChild() action.  If the owner rejects the object by sending back an error, SetOwner() will fail immediately.</li>
<li>The object's class will then receive notification via the #NewOwner() action.</li>
<li>The resource tracking of the new owner will be modified so that the object is accepted as its child.  This means that if and when the owning object is destroyed, the new child object will be destroyed with it.</li>
</list>

If the Object does not support the NewOwner action, or the Owner does not support the NewChild action, then the
process will not fail.  It will continue on the assumption that neither party is concerned about ownership management.

-INPUT-
obj Object: Pointer to the object to modify.
obj Owner: Pointer to the new owner for the object.

-ERRORS-
Okay
NullArgs
Args
-END-

*********************************************************************************************************************/

ERROR SetOwner(OBJECTPTR Object, OBJECTPTR Owner)
{
   pf::Log log(__FUNCTION__);

   if ((!Object) or (!Owner)) return log.warning(ERR_NullArgs);

   if (Object->OwnerID IS Owner->UID) return ERR_Okay;

   if (Object->ExtClass->Flags & CLF_NO_OWNERSHIP) {
      log.traceWarning("Cannot set the object owner as CLF_NO_OWNERSHIP is set in its class.");
      return ERR_Okay;
   }

   if (Object IS Owner) return log.warning(ERR_Recursion);

   //log.msg("Object: %d, New Owner: %d, Current Owner: %d", Object->UID, Owner->UID, Object->OwnerID);

   // Send a new child alert to the owner.  If the owner returns an error then we return immediately.

   ScopedObjectAccess objlock(Object);

   if (!CheckAction(Owner, AC_NewChild)) {
      struct acNewChild newchild = { .Object = Object };
      if (auto error = Action(AC_NewChild, Owner, &newchild); error != ERR_NoSupport) {
         if (error) { // If the owner has passed the object through to another owner, return ERR_Okay, otherwise error.
            if (error IS ERR_OwnerPassThrough) return ERR_Okay;
            else return error;
         }
      }
   }

   struct acNewOwner newowner = { .NewOwner = Owner };
   Action(AC_NewOwner, Object, &newowner);

   //if (Object->OwnerID) log.trace("SetOwner:","Changing the owner for object %d from %d to %d.", Object->UID, Object->OwnerID, Owner->UID);

   // Track the object's memory header to the new owner

   ThreadLock lock(TL_PRIVATE_MEM, 4000);
   if (lock.granted()) {
      auto mem = glPrivateMemory.find(Object->UID);
      if (mem IS glPrivateMemory.end()) return log.warning(ERR_SystemCorrupt);
      mem->second.OwnerID = Owner->UID;

      // Remove reference from the now previous owner
      if (Object->OwnerID) glObjectChildren[Object->OwnerID].erase(Object->UID);

      Object->OwnerID = Owner->UID;

      glObjectChildren[Owner->UID].insert(Object->UID);
      return ERR_Okay;
   }
   else return log.warning(ERR_SystemLocked);
}

/*********************************************************************************************************************

-FUNCTION-
SetContext: Alters the nominated owner of newly created objects.

This function defines the object that has control of the current thread.  Once called, all further resource
allocations are assigned to that object.  This is significant for the automatic collection of memory and object
allocations.  For example:

<pre>
InitObject(display);
auto ctx = SetContext(display);

   NewObject(ID_BITMAP, &bitmap);
   AllocMemory(1000, MEM_DATA, &memory, NULL);

SetContext(ctx);
FreeResource(display->UID);
</pre>

The above code allocates a Bitmap and a memory block, both of which will be contained by the display. When
~FreeResource() is called, both the bitmap and memory block will be automatically removed as they have a dependency
on the display's existence.  Please keep in mind that the following is incorrect:

<pre>
InitObject(display);
auto ctx = SetContext(display);

   NewObject(ID_BITMAP, &bitmap);
   AllocMemory(1000, MEM_DATA, &memory, NULL);

SetContext(ctx);
FreeResource(display->UID); // The bitmap and memory would be auto-collected
FreeResource(bitmap->UID);  // Reference is no longer valid
FreeResource(memory);  // Reference is no longer valid
</pre>

As the bitmap and memory block would have been freed as members of the display, their references are invalid when
manually terminated in the following instructions.

SetContext() is intended for use by modules and classes.  Do not use it in an application unless conditions
necessitate its use.  The Core automatically manages the context when calling class actions, methods and interactive
fields.

-INPUT-
obj Object: Pointer to the object that will take on the new context.  If NULL, no change to the context will be made.

-RESULT-
obj: Returns a pointer to the previous context.  Because contexts nest, the client must call SetContext() a second time with this pointer in order to keep the process stable.

*********************************************************************************************************************/

OBJECTPTR SetContext(OBJECTPTR Object)
{
   if (Object) return tlContext->setContext(Object);
   else return tlContext->object();
}

/*********************************************************************************************************************

-FUNCTION-
SetName: Sets the name of an object.

This function sets the name of an object.  This enhances log messages and allows the object to be found in searches.
Please note that the length of the Name will be limited to the value indicated in the core header file, under
the `MAX_NAME_LEN` definition.  Names exceeding the allowed length are trimmed to fit.

Object names are limited to alpha-numeric characters and the underscore symbol.  Invalid characters are replaced with
an underscore.

-INPUT-
obj Object: The target object.
cstr Name: The new name for the object.

-ERRORS-
Okay:
NullArgs:
Search:       The Object is not recognised by the system - the address may be invalid.
LockFailed:

*********************************************************************************************************************/

static const char sn_lookup[256] = {
   '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_',
   '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_',
   '_', '_', '_', '_', '0', '1', '2', '3', '4', '5', '6', '7', '8', '_', '_', '_', '_', '_', '_', '_', '_', 'a',
   'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w',
   'x', 'y', '_', '_', '_', '_', '_', '_', '_', 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm',
   'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', '_', '_', '_', '_', '_', '_',
   '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_',
   '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_',
   '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_',
   '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_',
   '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_',
   '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_'
};

ERROR SetName(OBJECTPTR Object, CSTRING NewName)
{
   pf::Log log(__FUNCTION__);

   if ((!Object) or (!NewName)) return log.warning(ERR_NullArgs);

   ScopedObjectAccess objlock(Object);
   ThreadLock lock(TL_OBJECT_LOOKUP, 4000);
   if (!lock.granted()) return log.warning(ERR_LockFailed);

   // Remove any existing name first.

   if (Object->Name[0]) remove_object_hash(Object);

   LONG i;
   for (i=0; (i < (MAX_NAME_LEN-1)) and (NewName[i]); i++) Object->Name[i] = sn_lookup[UBYTE(NewName[i])];
   Object->Name[i] = 0;

   if (Object->Name[0]) glObjectLookup[Object->Name].push_back(Object);

   return ERR_Okay;
}

/*********************************************************************************************************************

-FUNCTION-
SubscribeAction: Monitor action calls made against an object.

The SubscribeAction() function allows a client to receive a callback each time that an action is executed on
an object.  This technique is referred to as "action monitoring" and is often used for responding to UI
events and the termination of objects.

Subscriptions are context sensitive, so the Callback will execute in the space attributed to to the caller.

The following example illustrates how to listen to a Surface object's Redimension action and respond to resize
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
   pf::Log log(__FUNCTION__);

   if ((!Object) or (!Callback)) return log.warning(ERR_NullArgs);
   if ((ActionID < 0) or (ActionID >= AC_END)) return log.warning(ERR_OutOfRange);
   if (Callback->Type != CALL_STDC) return log.warning(ERR_Args);
   if (Object->collecting()) return ERR_Okay;

   if (glSubReadOnly) {
      glDelayedSubscribe.emplace_back(Object->UID, ActionID, *Callback);
      __atomic_or_fetch(&Object->NotifyFlags[ActionID>>5], 1<<(ActionID & 31), __ATOMIC_RELAXED);
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
   pf::Log log(__FUNCTION__);

   if (!Object) return log.warning(ERR_NullArgs);
   if ((ActionID < 0) or (ActionID >= AC_END)) return log.warning(ERR_Args);

   if (glSubReadOnly) {
      glDelayedUnsubscribe.emplace_back(Object->UID, ActionID);
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
