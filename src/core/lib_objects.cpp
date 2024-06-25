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
static std::unordered_map<OBJECTID, std::unordered_map<LONG, std::vector<ActionSubscription> > > glSubscriptions;
static std::vector<unsubscription> glDelayedUnsubscribe;
static std::vector<subscription> glDelayedSubscribe;
static LONG glSubReadOnly = 0; // To prevent modification of glSubscriptions

static void free_children(OBJECTPTR Object);

//********************************************************************************************************************

ERR msg_free(APTR Custom, LONG MsgID, LONG MsgType, APTR Message, LONG MsgSize)
{
   // Lock the object via conventional means to guarantee thread safety.
   OBJECTPTR obj;
   if (AccessObject(((OBJECTID *)Message)[0], 10000, &obj) IS ERR::Okay) {
      obj->Locked = false; // Required to allow the object to be freed while maintaining a lock via the Queue mechanism.
      FreeResource(obj);
   }
   return ERR::Okay;
}

//********************************************************************************************************************
// Object termination hook for FreeResource()

static ERR object_free(Object *Object)
{
   pf::Log log("Free");

   ScopedObjectAccess objlock(Object);
   if (!objlock.granted()) return ERR::AccessObject;

   ObjectContext new_context(Object, AC::Free);

   auto mc = Object->ExtClass;
   if (!mc) {
      log.trace("Object %p #%d is missing its class pointer.", Object, Object->UID);
      return ERR::Okay;
   }

   // If the object is locked then we mark it for collection and return.
   // Collection is achieved via the message queue for maximum safety.

   if (Object->Locked) {
      log.detail("Object #%d locked; marking for deletion.", Object->UID);
      if ((Object->Owner) and (Object->Owner->collecting())) Object->Owner = NULL; // The Owner pointer is no longer safe to use
      Object->Flags |= NF::FREE_ON_UNLOCK;
      return ERR::InUse;
   }

   if (Object->terminating()) {
      log.trace("Object already being terminated.");
      return ERR::InUse;
   }

   if (Object->ActionDepth > 0) {
      // The object is still in use.  This should only be triggered if the object wasn't locked with LockObject().
      log.trace("Object in use; marking for collection.");
      if ((Object->Owner) and (Object->Owner->collecting())) Object->Owner = NULL;
      if (!Object->defined(NF::COLLECT)) {
         Object->Flags |= NF::COLLECT;
         SendMessage(MSGID_FREE, MSF::NIL, &Object->UID, sizeof(OBJECTID));
      }
      return ERR::InUse;
   }

   if (Object->classID() IS CLASSID::METACLASS)   log.branch("%s, Owner: %d", Object->className(), Object->ownerID());
   else if (Object->classID() IS CLASSID::MODULE) log.branch("%s, Owner: %d", ((extModule *)Object)->Name.c_str(), Object->ownerID());
   else if (Object->Name[0])                      log.branch("Name: %s, Owner: %d", Object->Name, Object->ownerID());
   else log.branch("Owner: %d", Object->ownerID());

   // If the object wants to be warned when the free process is about to be executed, it will subscribe to the
   // FreeWarning action.  The process can be aborted by returning ERR::InUse.

   if (mc->ActionTable[LONG(AC::FreeWarning)].PerformAction) {
      if (mc->ActionTable[LONG(AC::FreeWarning)].PerformAction(Object, NULL) IS ERR::InUse) {
         if (Object->collecting()) {
            // If the object is marked for deletion then it is not possible to avoid destruction (this prevents objects
            // from locking up the shutdown process).

            log.msg("Object will be destroyed despite being in use.");
         }
         else {
            if ((Object->Owner) and (Object->Owner->collecting())) Object->Owner = NULL;
            return ERR::InUse;
         }
      }
   }

   if (mc->Base) { // Sub-class detected, so call the base class
      if (mc->Base->ActionTable[LONG(AC::FreeWarning)].PerformAction) {
         if (mc->Base->ActionTable[LONG(AC::FreeWarning)].PerformAction(Object, NULL) IS ERR::InUse) {
            if (Object->collecting()) {
               // If the object is marked for deletion then it is not possible to avoid destruction (this prevents
               // objects from locking up the shutdown process).
               log.msg("Object will be destroyed despite being in use.");
            }
            else {
               if ((Object->Owner) and (Object->Owner->collecting())) Object->Owner = NULL;
               return ERR::InUse;
            }
         }
      }
   }

   // Mark the object as being in the free process.  The mark prevents any further access to the object via
   // AccessObject().  Classes may also use the flag to check if an object is in the process of being freed.

   Object->Flags = (Object->Flags|NF::FREE) & (~NF::FREE_ON_UNLOCK);

   NotifySubscribers(Object, AC::Free, NULL, ERR::Okay);

   if (mc->ActionTable[LONG(AC::Free)].PerformAction) {  // Could be sub-class or base-class
      mc->ActionTable[LONG(AC::Free)].PerformAction(Object, NULL);
   }

   if (mc->Base) { // Sub-class detected, so call the base class
      if (mc->Base->ActionTable[LONG(AC::Free)].PerformAction) {
         mc->Base->ActionTable[LONG(AC::Free)].PerformAction(Object, NULL);
      }
   }

   if (Object->NotifyFlags.load()) {
      const std::lock_guard<std::recursive_mutex> lock(glSubLock);
      glSubscriptions.erase(Object->UID);
   }

   // If a private child structure is present, remove it

   if (Object->ChildPrivate) {
      if (FreeResource(Object->ChildPrivate) != ERR::Okay) log.warning("Invalid ChildPrivate address %p.", Object->ChildPrivate);
      Object->ChildPrivate = NULL;
   }

   free_children(Object);

   if (Object->defined(NF::TIMER_SUB)) {
      if (auto lock = std::unique_lock{glmTimer, 200ms}) {
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
      if (auto olock = std::unique_lock{glmObjectLookup, 4s}) {
         remove_object_hash(Object);
      }
   }

   // Clear the object header.  This helps to raise problems in any areas of code that may attempt to use the object
   // after it has been destroyed.

   Object->Class = NULL;
   Object->UID   = 0;
   return ERR::Okay;
}

static ResourceManager glResourceObject = {
   "Object",
   (ERR (*)(APTR))&object_free
};

//********************************************************************************************************************

CSTRING action_name(OBJECTPTR Object, ACTIONID ActionID)
{
   if (ActionID > AC::NIL) {
      if (ActionID < AC::END) return ActionTable[LONG(ActionID)].Name;
      else return "Action";
   }
   else if ((Object) and (!((extMetaClass *)Object->Class)->Methods.empty())) {
      return ((extMetaClass *)Object->Class)->Methods[-LONG(ActionID)].Name;
   }
   else return "Method";
}

//********************************************************************************************************************
// Refer to AsyncAction() to see how this is used.  It calls an action on a target object and then sends a callback
// notification via the internal message queue.

struct thread_data {
   OBJECTPTR Object;
   ACTIONID  ActionID;
   FUNCTION  Callback;
   BYTE      Parameters;
};

static ERR thread_action(extThread *Thread)
{
   ERR error;
   auto data = (thread_data *)Thread->Data;
   OBJECTPTR obj = data->Object;

   if ((error = LockObject(obj, 5000)) IS ERR::Okay) { // Access the object and process the action.
      --obj->ThreadPending;
      error = Action(data->ActionID, obj, data->Parameters ? (data + 1) : NULL);

      if (data->Parameters) { // Free any temporary buffers that were allocated.
         if (data->ActionID > AC::NIL) local_free_args(data + 1, ActionTable[LONG(data->ActionID)].Args);
         else local_free_args(data + 1, ((extMetaClass *)obj->Class)->Methods[-LONG(data->ActionID)].Args);
      }

      if (obj->terminating()) obj = NULL; // Clear the obj pointer because the object will be deleted on release.
      ReleaseObject(data->Object);
   }
   else --obj->ThreadPending;

   // Send a callback notification via messaging if required.  The MSGID_THREAD_ACTION receiver is msg_threadaction()

   if (data->Callback.defined()) {
      ThreadActionMessage msg = {
         .Object   = obj,
         .ActionID = data->ActionID,
         .Error    = error,
         .Callback = data->Callback
      };
      SendMessage(MSGID_THREAD_ACTION, MSF::ADD, &msg, sizeof(msg));
   }

   threadpool_release(Thread);
   return error;
}

// Free all private memory resources tracked to an object.

static void free_children(OBJECTPTR Object)
{
   pf::Log log;

   if (auto lock = std::unique_lock{glmMemory}) {
      if (!glObjectChildren[Object->UID].empty()) {
         const auto children = glObjectChildren[Object->UID]; // Take an immutable copy of the resource list

         for (const auto id : children) {
            auto it = glPrivateMemory.find(id);
            if ((it IS glPrivateMemory.end()) or (!it->second.Address)) continue;
            auto &mem = it->second;

            if (((mem.Flags & MEM::DELETE) != MEM::NIL) or (!mem.Object)) continue;

            if ((mem.Object->Owner) and (mem.Object->Owner != Object)) {
               log.warning("Failed sanity test: Child object #%d has owner ID of #%d that does not match #%d.", mem.Object->UID, mem.Object->ownerID(), Object->UID);
               continue;
            }

            if (!mem.Object->defined(NF::FREE_ON_UNLOCK)) {
               if (mem.Object->defined(NF::LOCAL)) {
                  log.warning("Found unfreed child object #%d (class %s) belonging to %s object #%d.", mem.Object->UID, ResolveClassID(mem.Object->classID()), Object->className(), Object->UID);
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

            if (((mem.Flags & MEM::DELETE) != MEM::NIL) or (!mem.Address)) continue;

            if (glLogLevel >= 3) {
               if ((mem.Flags & MEM::STRING) != MEM::NIL) {
                  log.warning("Unfreed string \"%.40s\" (%p, #%d)", (CSTRING)mem.Address, mem.Address, mem.MemoryID);
               }
               else if ((mem.Flags & MEM::MANAGED) != MEM::NIL) {
                  auto res = (ResourceManager **)((char *)mem.Address - sizeof(LONG) - sizeof(LONG) - sizeof(ResourceManager *));
                  if (res[0]) log.warning("Unfreed %s resource at %p.", res[0]->Name, mem.Address);
                  else log.warning("Unfreed resource at %p.", mem.Address);
               }
               else log.warning("Unfreed memory block %p, Size %d", mem.Address, mem.Size);
            }

            if (FreeResource(mem.Address) != ERR::Okay) log.warning("Error freeing tracked address %p", mem.Address);
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
You can find a complete list of available actions and their associated details in the Parasol Wiki.
The actions and methods supported by any class will be referenced in their auto-generated documentation.

Here are two examples that demonstrate how to make an action call.  The first performs an activation, which
does not require any additional arguments.  The second performs a move operation, which requires three additional
arguments to be passed to the Action() function:

<pre>
1. Action(AC::Activate, Picture, NULL);

2. struct acMove move = { 30, 15, 0 };
   Action(AC::Move, Window, &move);
</pre>

In all cases, action calls in C++ can be simplified by using their corresponding stub functions:

<pre>
1.  acActivate(Picture);

2a. acMove(Window, 30, 15, 0);

2b. Window->move(30, 15, 0);
</pre>

If the class of an object does not support the `Action` ID, an error code of `ERR::NoSupport` is returned.  To test
an object to see if its class supports an action, use the ~CheckAction() function.

-INPUT-
int(AC) Action: An action or method ID must be specified.
obj Object:     The target object.
ptr Parameters: Optional parameter structure associated with `Action`.

-ERRORS-
Okay:
NullArgs:
IllegalActionID: The `Action` parameter is invalid.
NoAction:        The `Action` is not supported by the object's supporting class.
ObjectCorrupt:   The `Object` state is corrupted.
-END-

**********************************************************************************************************************/

ERR Action(ACTIONID ActionID, OBJECTPTR Object, APTR Parameters)
{
   if (!Object) return ERR::NullArgs;

   ScopedObjectAccess lock(Object);
   if (!lock.granted()) return ERR::AccessObject;

   ObjectContext new_context(Object, ActionID);
   Object->ActionDepth++;
   auto cl = Object->ExtClass;

   ERR error;
   if (ActionID >= AC::NIL) {
      if (cl->ActionTable[LONG(ActionID)].PerformAction) { // Can be a base-class or sub-class call
         error = cl->ActionTable[LONG(ActionID)].PerformAction(Object, Parameters);
         if (error IS ERR::NoAction) {
            if ((cl->Base) and (cl->Base->ActionTable[LONG(ActionID)].PerformAction)) { // Base is set only if this is a sub-class
               error = cl->Base->ActionTable[LONG(ActionID)].PerformAction(Object, Parameters);
            }
         }
      }
      else if ((cl->Base) and (cl->Base->ActionTable[LONG(ActionID)].PerformAction)) { // Base is set only if this is a sub-class
         error = cl->Base->ActionTable[LONG(ActionID)].PerformAction(Object, Parameters);
      }
      else error = ERR::NoAction;
   }
   else { // Method call
      // Note that sub-classes may return ERR::NoAction if propagation to the base class is desirable.
      auto routine = (ERR (*)(OBJECTPTR, APTR))cl->Methods[-LONG(ActionID)].Routine;
      if (routine) error = routine(Object, Parameters);
      else error = ERR::NoAction;

      if ((error IS ERR::NoAction) and (cl->Base)) {  // If this is a child, check the base class
         auto routine = (ERR (*)(OBJECTPTR, APTR))cl->Base->Methods[-LONG(ActionID)].Routine;
         if (routine) error = routine(Object, Parameters);
      }
   }

   // If the object has action subscribers, check if any of them are listening to this particular action, and if so, notify them.

   if (LONG(error) & LONG(ERR::Notified)) {
      error = ERR(LONG(error) & ~LONG(ERR::Notified));
   }
   else if ((ActionID > AC::NIL) and (Object->NotifyFlags.load() & (1LL<<(LONG(ActionID) & 63)))) {
      std::lock_guard<std::recursive_mutex> lock(glSubLock);

      glSubReadOnly++;

      if (auto it = glSubscriptions.find(Object->UID); it != glSubscriptions.end()) {
         if (it->second.contains(LONG(ActionID))) {
            for (auto &list : it->second[LONG(ActionID)]) {
               pf::SwitchContext ctx(list.Subscriber);
               list.Callback(Object, ActionID, (error IS ERR::NoAction) ? ERR::Okay : error, Parameters, list.Meta);
            }
         }
      }

      glSubReadOnly--;
   }

   Object->ActionDepth--;
   return error;
}

/*********************************************************************************************************************

-FUNCTION-
ActionList: Returns a pointer to the global action table.

This function returns an array of all actions supported by the Core, including name, arguments and structure
size.  The ID of each action is indicated by its index within the array.

The `Name` field specifies the name of the action.  The `Args` field refers to the action's argument definition structure,
which lists the argument names and their relevant types.  This is matched by the `Size` field, which indicates the
byte-size of the action's related argument structure.  If the action does not support arguments, the `Args` and `Size`
fields will be set to `NULL`.  The following illustrates two argument definition examples:

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

<types lookup="FD">
<type name="LONG">A 32-bit integer value ranging from -2,147,483,647 to 2,147,483,648.</>
<type name="LARGE">A 64-bit integer value.</>
<type name="PTR">A standard address space pointer.</>
<type name="STRING">A pointer to a null-terminated string.</>
<type name="DOUBLE">A 64-bit floating point value.</>
<type name="OBJECT">This flag is sometimes set in conjunction with the `FD_LONG` type.  It indicates that the argument refers to an object ID.</>
<type name="PTRSIZE">This argument type can only be used if it follows an `FD_PTR` type, and if the argument itself is intended to reflect the size of the buffer referred to by the previous `FD_PTR` argument.</>
<type name="RESULT">This special flag is set in conjunction with the other data-based argument types. Example: If the developer is required to supply a pointer to a `LONG` field in which the function will store a result, the correct argument definition will be `FD_RESULT|FD_LONG|FD_PTR`. To make the definition of these argument types easier, `FD_PTRRESULT` and `FD_LONGRESULT` macros are also available for use.</>
</>

-INPUT-
&array(struct(ActionTable)) Actions: A pointer to the Core's action table `struct ActionTable *` is returned. Please note that the first entry in the `ActionTable` list has all fields driven to `NULL`, because valid action ID's start from one, not zero.  The final action in the list is also terminated with `NULL` fields in order to indicate an end to the list.  Knowing this is helpful when scanning the list or calculating the total number of actions supported by the Core.
&arraysize Size: Total number of elements in the returned list.

*********************************************************************************************************************/

void ActionList(struct ActionTable **List, LONG *Size)
{
   if (List) *List = (struct ActionTable *)ActionTable;
   if (Size) *Size = LONG(AC::END);
}

/*********************************************************************************************************************

-FUNCTION-
AsyncAction: Execute an action in parallel, via a separate thread.

This function follows the same principles of execution as the Action() function, with the difference of executing the
action in parallel via a dynamically allocated thread.  Please refer to the ~Action() function for general
information on action execution.

To receive feedback of the action's completion, use the `Callback` parameter and supply a function.  The
prototype for the callback routine is `callback(ACTIONID ActionID, OBJECTPTR Object, ERR Error, APTR Meta)`

It is crucial that the target object is not destroyed while the thread is executing.  Use the `Callback` routine to
receive notification of the thread's completion and then free the object if desired.  The callback will be processed
when the main thread makes a call to ~ProcessMessages(), so as to maintain an orderly execution process within the 
application.

The 'Error' parameter in the callback reflects the error code returned by the action after it has been called.  Note
that if AsyncAction() fails, the callback will never be executed because the thread attempt will have been aborted.

Please note that there is some overhead involved when safely initialising and executing a new thread.  This function is
at its most effective when used to perform lengthy processes such as the loading and parsing of data.

-INPUT-
int(AC) Action: An action or method ID must be specified here.
obj Object: A pointer to the object that is going to perform the action.
ptr Args: If the action or method is documented as taking parameters, provide the correct parameter structure here.
ptr(func) Callback: This function will be called after the thread has finished executing the action.

-ERRORS-
Okay
NullArgs
IllegalMethodID
MissingClass
NewObject
Init
-END-

*********************************************************************************************************************/

ERR AsyncAction(ACTIONID ActionID, OBJECTPTR Object, APTR Parameters, FUNCTION *Callback)
{
   pf::Log log(__FUNCTION__);

   if ((ActionID IS AC::NIL) or (!Object)) return ERR::NullArgs;

   log.traceBranch("Action: %d, Object: %d, Parameters: %p, Callback: %p", LONG(ActionID), Object->UID, Parameters, Callback);

   ++Object->ThreadPending;

   ERR error;
   extThread *thread = NULL;
   if ((error = threadpool_get(&thread)) IS ERR::Okay) {
      // Prepare the parameter buffer for passing to the thread routine.

      LONG argssize = 0;
      BYTE call_data[sizeof(thread_data) + SIZE_ACTIONBUFFER];
      bool free_args = false;
      const FunctionField *args = NULL;

      if (Parameters) {
         if (LONG(ActionID) > 0) {
            args = ActionTable[LONG(ActionID)].Args;
            if ((argssize = ActionTable[LONG(ActionID)].Size) > 0) {
               if ((error = copy_args(args, argssize, (BYTE *)Parameters, call_data + sizeof(thread_data), SIZE_ACTIONBUFFER, &argssize, ActionTable[LONG(ActionID)].Name)) IS ERR::Okay) {
                  free_args = true;
               }

               argssize += sizeof(thread_data);
            }
            else argssize = sizeof(thread_data);
         }
         else if (auto cl = Object->ExtClass) {
            args = cl->Methods[-LONG(ActionID)].Args;
            if ((argssize = cl->Methods[-LONG(ActionID)].Size) > 0) {
               if ((error = copy_args(args, argssize, (BYTE *)Parameters, call_data + sizeof(thread_data), SIZE_ACTIONBUFFER, &argssize, cl->Methods[-LONG(ActionID)].Name)) IS ERR::Okay) {
                  free_args = true;
               }
            }
            else log.trace("Ignoring parameters provided for method %s", cl->Methods[-LONG(ActionID)].Name);

            argssize += sizeof(thread_data);
         }
         else error = log.warning(ERR::MissingClass);
      }
      else argssize = sizeof(thread_data);

      // Execute the thread that will call the action.  Refer to thread_action() for the routine.

      if (error IS ERR::Okay) {
         thread->Routine = C_FUNCTION(thread_action);

         auto call = (thread_data *)call_data;
         call->Object   = Object;
         call->ActionID = ActionID;
         call->Parameters = Parameters ? true : false;
         if (Callback) call->Callback = *Callback;
         else call->Callback.Type = CALL::NIL;

         thread->setData(call, argssize);

         error = thread->activate();
      }

      if (error != ERR::Okay) {
         threadpool_release(thread);
         if (free_args) local_free_args(call_data + sizeof(thread_data), args);
      }
   }
   else error = ERR::NewObject;

   if (error != ERR::Okay) --Object->ThreadPending;

   return error;
}

/*********************************************************************************************************************

-FUNCTION-
CheckAction: Checks objects to see whether or not they support certain actions.

This function returns `ERR::True` if an object's class supports a given action or method ID.  For example:

<pre>
if (CheckAction(pic, AC::Query) IS ERR::True) {
   // The Query action is supported.
}
</pre>

-INPUT-
obj Object: The target object.
int(AC) Action: A registered action or method ID.

-ERRORS-
True: The object supports the specified action.
False: The action is not supported.
NullArgs:
LostClass:

*********************************************************************************************************************/

ERR CheckAction(OBJECTPTR Object, ACTIONID ActionID)
{
   pf::Log log(__FUNCTION__);

   if (Object) {
      if (!Object->Class) return ERR::False;
      else if (Object->classID() IS CLASSID::METACLASS) {
         if (((extMetaClass *)Object)->ActionTable[LONG(ActionID)].PerformAction) return ERR::True;
         else return ERR::False;
      }
      else if (auto cl = Object->ExtClass) {
         if (cl->ActionTable[LONG(ActionID)].PerformAction) return ERR::True;
         else if (cl->Base) {
            if (cl->Base->ActionTable[LONG(ActionID)].PerformAction) return ERR::True;
         }
         return ERR::False;
      }
      else return log.warning(ERR::LostClass);
   }
   else return log.warning(ERR::NullArgs);
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

ERR CheckObjectExists(OBJECTID ObjectID)
{
   if (auto lock = std::unique_lock{glmMemory}) {
      ERR result = ERR::False;
      auto mem = glPrivateMemory.find(ObjectID);
      if ((mem != glPrivateMemory.end()) and (mem->second.Object)) {
         if (mem->second.Object->defined(NF::FREE_ON_UNLOCK));
         else result = ERR::True;
      }
      return result;
   }
   else {
      pf::Log log(__FUNCTION__);
      return log.warning(ERR::LockFailed);
   }
}

/*********************************************************************************************************************

-FUNCTION-
CurrentContext: Returns a pointer to the object that has the current context.

This function returns a pointer to the object that has the current context.  Context is primarily used to manage
resource allocations.  Manipulating the context is sometimes necessary to ensure that a resource is tracked to
the correct object.

To get the context of the caller (the client), use ~ParentContext().

-RESULT-
obj: Returns an object pointer (of which the process has exclusive access to).  Cannot return `NULL` except in the initial start-up and late shut-down sequence of the Core.

*********************************************************************************************************************/

OBJECTPTR CurrentContext(void)
{
   return tlContext->object();
}

/*********************************************************************************************************************

-FUNCTION-
ParentContext: Returns the context of the client.

This function is used to return the context of the caller (the client), as opposed to ~CurrentContext(), which
returns the operating context.  This feature is commonly used by methods that need to acquire a reference to the
client for resource management reasons.

Note that this function can return `NULL` if called when running at process-level, although this would never be
the case when called from an action or method.

-RESULT-
obj: An object reference is returned, or `NULL` if there is no parent context.

*********************************************************************************************************************/

OBJECTPTR ParentContext(void)
{
   auto parent = tlContext->stack;
   while ((parent) and (parent->object() IS tlContext->object())) parent = parent->stack;
   return parent ? parent->object() : (OBJECTPTR)NULL;
}

/*********************************************************************************************************************

-FUNCTION-
FindClass: Returns all class objects for a given class ID.

This function will find a specific class by ID and return its @MetaClass.  If the class is not in memory, the internal
dictionary is checked to discover a module binary registered with that ID.  If this succeeds, the module is loaded
into memory and the class will be returned.  In any event of failure, `NULL` is returned.

If the ID of a named class is not known, call ~ResolveClassName() first and pass the resulting ID to this function.

-INPUT-
cid ClassID: A class ID such as one retrieved from ~ResolveClassName().

-RESULT-
obj(MetaClass): Returns a pointer to the @MetaClass structure that has been found as a result of the search, or `NULL` if no matching class was found.

*********************************************************************************************************************/

objMetaClass * FindClass(CLASSID ClassID)
{
   auto it = glClassMap.find(ClassID);
   if (it != glClassMap.end()) return it->second;

   if (glProgramStage IS STAGE_SHUTDOWN) return NULL; // No new module loading during shutdown

   // Class is not loaded.  Try and find the class in the dictionary.  If we find one, we can
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

         log.branch("Attempting to load module \"%s\" for class $%.8x.", path.c_str(), ULONG(ClassID));

         objModule::create mod = { fl::Name(path) };
         if (mod.ok()) {
            it = glClassMap.find(ClassID);
            if (it != glClassMap.end()) return it->second;
            else log.warning("Module \"%s\" did not configure class \"%s\"", path.c_str(), glClassDB[ClassID].Name.c_str());
         }
         else log.warning("Failed to load module \"%s\"", path.c_str());
      }
      else log.warning("No module path defined for class \"%s\"", glClassDB[ClassID].Name.c_str());
   }
   else log.warning("Could not find class $%.8x in memory or dictionary (%d registered).", ULONG(ClassID), LONG(std::ssize(glClassDB)));

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
FindObject("SystemPointer", CLASSID::POINTER, 0, &id);
</pre>

If FindObject() cannot find any matching objects then it will return an error code.

-INPUT-
cstr Name:      The name of an object to search for.
cid ClassID:    Optional.  Set to a class ID to filter the results down to a specific class type.
int(FOF) Flags: Optional flags.
&oid ObjectID:  An object id variable for storing the result.

-ERRORS-
Okay: At least one matching object was found and stored in the `ObjectID`.
Args:
Search: No objects matching the given name could be found.
LockFailed:
EmptyString:
DoesNotExist:
-END-

*********************************************************************************************************************/

ERR FindObject(CSTRING InitialName, CLASSID ClassID, FOF Flags, OBJECTID *Result)
{
   pf::Log log(__FUNCTION__);

   if ((!Result) or (!InitialName)) return ERR::NullArgs;
   if (!InitialName[0]) return log.warning(ERR::EmptyString);

   if ((Flags & FOF::SMART_NAMES) != FOF::NIL) {
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
         if (auto objectid = (OBJECTID)strtol(InitialName, NULL, 0)) {
            if (CheckObjectExists(objectid) IS ERR::Okay) {
               *Result = objectid;
               return ERR::Okay;
            }
            else return ERR::Search;
         }
         else return ERR::Search;
      }

      if (iequals("owner", InitialName)) {
         if ((tlContext != &glTopContext) and (tlContext->object()->Owner)) {
            *Result = tlContext->object()->Owner->UID;
            return ERR::Okay;
         }
         else return ERR::DoesNotExist;
      }
   }

   if (auto lock = std::unique_lock{glmObjectLookup, 4s}) {
      if (glObjectLookup.contains(InitialName)) {
         auto &list = glObjectLookup[InitialName];
         if (ClassID IS CLASSID::NIL) {
            *Result = list.back()->UID;
            return ERR::Okay;
         }

         for (auto it=list.rbegin(); it != list.rend(); it++) {
            auto obj = *it;
            if ((obj->classID() IS ClassID) or (obj->Class->BaseClassID IS ClassID)) {
               *Result = obj->UID;
               return ERR::Okay;
            }
         }
      }
   }

   return ERR::Search;
}

/*********************************************************************************************************************

-FUNCTION-
GetActionMsg: Returns a message structure if called from an action that was executed by the message system.

This function is for use by action and method support routines only.  It will return a !Message structure if the
action currently under execution has been called directly from the ~ProcessMessages() function.  In all other
cases a `NULL` pointer is returned.

The !Message structure reflects the contents of a standard ~GetMessage() call.  Of particular interest may be
the `Time` field, which indicates the time-stamp at which the action message was originally sent to the object.

-RESULT-
resource(Message): A !Message structure is returned if the function is called in valid circumstances, otherwise `NULL`.  The !Message structure's fields are described in the ~GetMessage() function.

*********************************************************************************************************************/

Message * GetActionMsg(void)
{
   if (auto obj = tlContext->resource()) {
      if (obj->defined(NF::MESSAGE) and (obj->ActionDepth IS 1)) {
         return (Message *)tlCurrentMsg;
      }
   }
   return NULL;
}

/*********************************************************************************************************************

-FUNCTION-
GetClassID: Returns the class ID of an ID-referenced object.

Call this function with any valid object ID to learn the identifier for its base class.  This is the quickest way to
retrieve the class of an object without having to gain exclusive access to the object first.

Note that if the object's pointer is already known, the quickest way to learn of its class is to call the `classID()`
C++ method.

-INPUT-
oid Object: The object to be examined.

-RESULT-
cid: Returns the base class ID of the object or zero if failure.

*********************************************************************************************************************/

CLASSID GetClassID(OBJECTID ObjectID)
{
   if (auto object = GetObjectPtr(ObjectID)) return object->Class->BaseClassID;
   else return CLASSID::NIL;
}

/*********************************************************************************************************************

-FUNCTION-
GetObjectPtr: Returns a direct pointer for any object ID.

This function translates an object ID to its respective address pointer.

-INPUT-
oid Object: The ID of the object to lookup.

-RESULT-
obj: The address of the object is returned, or `NULL` if the ID does not relate to an object.

*********************************************************************************************************************/

OBJECTPTR GetObjectPtr(OBJECTID ObjectID)
{
   if (auto lock = std::unique_lock{glmMemory}) {
      if (auto mem = glPrivateMemory.find(ObjectID); mem != glPrivateMemory.end()) {
         if (((mem->second.Flags & MEM::OBJECT) != MEM::NIL) and (mem->second.Object)) {
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

If the object address is already known then the fastest means of retrieval is via the `ownerID()` C++ class method.

-INPUT-
oid Object: The ID of an object to query.

-RESULT-
oid: Returns the ID of the object's owner.  If the object does not have a owner (i.e. if it is untracked) or if the provided ID is invalid, this function will return NULL.

*********************************************************************************************************************/

OBJECTID GetOwnerID(OBJECTID ObjectID)
{
   if (auto lock = std::unique_lock{glmMemory}) {
      if (auto mem = glPrivateMemory.find(ObjectID); mem != glPrivateMemory.end()) {
         if (mem->second.Object) return mem->second.Object->ownerID();
      }
   }
   return 0;
}

/*********************************************************************************************************************

-FUNCTION-
InitObject: Initialises an object so that it is ready for use.

This function initialises objects so that they can be used for their intended purpose. Initialisation is compulsory,
and a client may not call any actions or methods on an object until it has been initialised.  Exceptions to
this rule only apply to the `GetKey()` and `SetKey()` actions.

If the initialisation of an object fails due to a support problem (for example, if a PNG @Picture object attempts to
load a JPEG file), the initialiser will search for a sub-class that can handle the data.  If a sub-class that can
support the object's configuration is available, the object's interface will be shared between both the base-class
and the sub-class.

If an object does not support the data or its configuration, an error code of `ERR::NoSupport` will be returned.
Other appropriate error codes can be returned if initialisation fails.

-INPUT-
obj Object: The object to initialise.

-ERRORS-
Okay: The object was initialised.
LostClass
DoubleInit
ObjectCorrupt

*********************************************************************************************************************/

ERR InitObject(OBJECTPTR Object)
{
   pf::Log log("Init");

   ScopedObjectAccess objlock(Object);

   auto cl = Object->ExtClass;

   if (Object->initialised()) {  // Initialising twice does not cause an error, but send a warning and return
      log.warning(ERR::DoubleInit);
      return ERR::Okay;
   }

   if (Object->Name[0]) log.branch("%s #%d, Name: %s, Owner: %d", cl->ClassName, Object->UID, Object->Name, Object->ownerID());
   else log.branch("%s #%d, Owner: %d", cl->ClassName, Object->UID, Object->ownerID());

   ObjectContext new_context(Object, AC::Init);

   bool use_subclass = false;
   ERR error = ERR::Okay;
   if (Object->isSubClass()) {
      // For sub-classes, the base-class gets called first.  It should verify that
      // the object is sub-classed so as to prevent it from doing 'too much' initialisation.

      if (cl->Base->ActionTable[LONG(AC::Init)].PerformAction) {
         error = cl->Base->ActionTable[LONG(AC::Init)].PerformAction(Object, NULL);
      }

      if (error IS ERR::Okay) {
         if (cl->ActionTable[LONG(AC::Init)].PerformAction) {
            error = cl->ActionTable[LONG(AC::Init)].PerformAction(Object, NULL);
         }

         if (error IS ERR::Okay) Object->Flags |= NF::INITIALISED;
      }

      return error;
   }
   else {
      // Meaning of special error codes:
      //
      // ERR::NoSupport: The source data is not recognised.  Search for a sub-class that might have better luck.  Note
      //   that in the first case we can only support classes that are already in memory.  The second part of this
      //   routine supports checking of sub-classes that aren't loaded yet.
      //
      // ERR::UseSubClass: Similar to ERR::NoSupport, but avoids scanning of sub-classes that aren't loaded in memory.

      std::unordered_map<CLASSID, extMetaClass *>::const_iterator subindex = glClassMap.begin();

      while (subindex != glClassMap.end()) {
         if (Object->ExtClass->ActionTable[LONG(AC::Init)].PerformAction) {
            error = Object->ExtClass->ActionTable[LONG(AC::Init)].PerformAction(Object, NULL);
         }
         else error = ERR::Okay; // If no initialiser defined, auto-OK

         if (error IS ERR::Okay) {
            Object->Flags |= NF::INITIALISED;

            if (Object->ExtClass != cl) {
               // Due to the sub-class switch, increase the open count of the sub-class (see NewObject() for details on object
               // reference counting).

               log.msg("Object class switched to sub-class \"%s\".", Object->className());

               Object->ExtClass->OpenCount++;
               Object->Flags |= NF::RECLASSED; // This flag indicates that the object originally belonged to the base-class
            }

            return ERR::Okay;
         }

         if (error IS ERR::UseSubClass) {
            log.trace("Requested to use registered sub-class.");
            use_subclass = true;
         }
         else if (error != ERR::NoSupport) break;

         // Attempt to initialise with the next known sub-class.

         while (subindex != glClassMap.end()) {
            if (Object->Class IS subindex->second);
            else if ((Object->Class->BaseClassID IS subindex->second->BaseClassID) and (subindex->second->ClassID != subindex->second->BaseClassID)) {
               Object->Class = subindex->second;
               log.trace("Attempting initialisation with sub-class '%s'", Object->className());
               break;
            }
            subindex++;
         }
      }
   }

   Object->Class = cl;  // Put back the original to retain integrity

   // If the base class and its loaded sub-classes failed, check the object for a Path field and check the data
   // against sub-classes that are not currently in memory.
   //
   // This is the only way we can support the automatic loading of sub-classes without causing undue load on CPU and
   // memory resources (loading each sub-class into memory just to check whether or not the data is supported is overkill).

   CSTRING path;
   if (use_subclass) { // If ERR::UseSubClass was set and the sub-class was not registered, do not call IdentifyFile()
      log.warning("ERR::UseSubClass was used but no suitable sub-class was registered.");
   }
   else if ((error IS ERR::NoSupport) and (GetField(Object, FID_Path|TSTR, &path) IS ERR::Okay) and (path)) {
      CLASSID class_id, subclass_id;
      if (IdentifyFile(path, &class_id, &subclass_id) IS ERR::Okay) {
         if ((class_id IS Object->classID()) and (subclass_id != CLASSID::NIL)) {
            log.msg("Searching for subclass $%.8x", ULONG(subclass_id));
            if ((Object->ExtClass = (extMetaClass *)FindClass(subclass_id))) {
               if (Object->ExtClass->ActionTable[LONG(AC::Init)].PerformAction) {
                  if ((error = Object->ExtClass->ActionTable[LONG(AC::Init)].PerformAction(Object, NULL)) IS ERR::Okay) {
                     log.msg("Object class switched to sub-class \"%s\".", Object->className());
                     Object->Flags |= NF::INITIALISED;
                     Object->ExtClass->OpenCount++;
                     return ERR::Okay;
                  }
               }
               else return ERR::Okay;
            }
            else log.warning("Failed to load module for class #%d.", ULONG(subclass_id));
         }
      }
      else log.warning("File '%s' does not belong to class '%s', got $%.8x.", path, Object->className(), ULONG(class_id));

      Object->Class = cl;  // Put back the original to retain object integrity
   }

   return error;
}

/*********************************************************************************************************************

-FUNCTION-
ListChildren: Returns a list of all children belonging to an object.

The ListChildren() function returns a list of all children belonging to an object.  The client must provide an empty
vector of !ChildEntry structures to host the results, which include unique object ID's and their class identifiers.

Note that any child objects marked with the `LOCAL` flag will be excluded because they are private members of the
targeted object.

-INPUT-
oid Object: An object to query.
ptr(cpp(array(resource(ChildEntry)))) List: Must refer to an array of !ChildEntry structures.

-ERRORS-
Okay: Zero or more children were found and listed.
Args
NullArgs
LockFailed

*********************************************************************************************************************/

ERR ListChildren(OBJECTID ObjectID, pf::vector<ChildEntry> *List)
{
   pf::Log log(__FUNCTION__);

   if ((!ObjectID) or (!List)) return log.warning(ERR::NullArgs);

   log.trace("#%d, List: %p", ObjectID, List);

   if (auto lock = std::unique_lock{glmMemory}) {
      for (const auto id : glObjectChildren[ObjectID]) {
         auto mem = glPrivateMemory.find(id);
         if (mem IS glPrivateMemory.end()) continue;

         if (auto child = mem->second.Object) {
            if (!child->defined(NF::LOCAL)) {
               List->emplace_back(child->UID, child->classID());
            }
         }
      }
      return ERR::Okay;
   }
   else return ERR::LockFailed;
}

/*********************************************************************************************************************

-FUNCTION-
NewObject: Creates new objects.

The NewObject() function is used to create new objects and register them for use within the Core.  After creating
a new object, the client can proceed to set the object's field values and initialise it with #Init() so that it
can be used as intended.

The new object will be modeled according to the class blueprint indicated by `ClassID`.  Pre-defined class ID's are
defined in their documentation and the `parasol/system/register.h` include file.  ID's for unregistered classes can
be computed using the ~ResolveClassName() function.

A pointer to the new object will be returned in the `Object` parameter.  By default, object allocations are context
sensitive and will be collected when their owner is terminated.  It is possible to track an object to a different
owner by using the ~SetOwner() function.

To destroy an object, call ~FreeResource().

-INPUT-
cid ClassID: A class ID from `system/register.h` or generated by ~ResolveClassName().
flags(NF) Flags:  Optional flags.
&obj Object: Pointer to an address variable that will store a reference to the new object.

-ERRORS-
Okay
NullArgs
MissingClass: The `ClassID` is invalid or refers to a class that is not installed.
AllocMemory
-END-

*********************************************************************************************************************/

ERR NewObject(CLASSID ClassID, NF Flags, OBJECTPTR *Object)
{
   pf::Log log(__FUNCTION__);

   auto class_id = ClassID;
   if ((class_id IS CLASSID::NIL) or (!Object)) return log.warning(ERR::NullArgs);

   auto mc = (extMetaClass *)FindClass(class_id);
   if (!mc) return log.warning(ERR::MissingClass);

   if (Object) *Object = NULL;

   Flags &= (NF::UNTRACKED|NF::LOCAL|NF::UNIQUE|NF::NAME|NF::SUPPRESS_LOG); // Very important to eliminate any internal flags.

   // If the object is local then turn off use of the UNTRACKED flag (otherwise the child will
   // end up being tracked to its task rather than its parent object).

   if ((Flags & NF::LOCAL) != NF::NIL) Flags = Flags & (~NF::UNTRACKED);

   // Force certain flags on the class' behalf

   if ((mc->Flags & CLF::NO_OWNERSHIP) != CLF::NIL) Flags |= NF::UNTRACKED;

   if ((Flags & NF::SUPPRESS_LOG) IS NF::NIL) log.branch("%s #%d, Flags: $%x", mc->ClassName, glPrivateIDCounter.load(std::memory_order_relaxed), LONG(Flags));

   OBJECTPTR head = NULL;
   MEMORYID head_id;

   if (AllocMemory(mc->Size, MEM::MANAGED|MEM::OBJECT|MEM::NO_LOCK|(((Flags & NF::UNTRACKED) != NF::NIL) ? MEM::UNTRACKED : MEM::NIL), (APTR *)&head, &head_id) IS ERR::Okay) {
      set_memory_manager(head, &glResourceObject);

      head->UID     = head_id;
      head->Class   = (extMetaClass *)mc;
      head->Flags   = Flags;

      // Tracking for our new object is configured here.

      if ((mc->Flags & CLF::NO_OWNERSHIP) != CLF::NIL) { } // Used by classes like RootModule to avoid tracking back to the task.
      else if ((Flags & NF::UNTRACKED) != NF::NIL) {
         if (class_id IS CLASSID::MODULE); // Untracked modules have no owner, due to the expunge process.
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
         if (auto obj = tlContext->resource(); obj IS &glDummyObject) {
            if (glCurrentTask) {
               ScopedObjectAccess lock(glCurrentTask);
               SetOwner(head, glCurrentTask);
            }
         }
         else SetOwner(head, obj);
      }
      else if (glCurrentTask) {
         ScopedObjectAccess lock(glCurrentTask);
         SetOwner(head, glCurrentTask);
      }

      // After the header has been created we can set the context, then call the base class's NewObject() support.  If this
      // object belongs to a sub-class, we will also call its supporting NewObject() action if it has specified one.

      pf::SwitchContext context(head);

      ERR error = ERR::Okay;
      if (mc->Base) {
         if (mc->Base->ActionTable[LONG(AC::NewObject)].PerformAction) {
            if ((error = mc->Base->ActionTable[LONG(AC::NewObject)].PerformAction(head, NULL)) != ERR::Okay) {
               log.warning(error);
            }
         }
         else error = log.warning(ERR::NoAction);
      }

      if ((error IS ERR::Okay) and (mc->ActionTable[LONG(AC::NewObject)].PerformAction)) {
         if ((error = mc->ActionTable[LONG(AC::NewObject)].PerformAction(head, NULL)) != ERR::Okay) {
            log.warning(error);
         }
      }

      if (error IS ERR::Okay) {
         ((extMetaClass *)head->Class)->OpenCount++;
         if (mc->Base) mc->Base->OpenCount++;

         *Object = head;
         return ERR::Okay;
      }

      FreeResource(head);
      return error;
   }
   else return ERR::AllocMemory;
}

/*********************************************************************************************************************

-FUNCTION-
NotifySubscribers: Used to send notification messages to action subscribers.

This function can be used by classes that need total control over notification management.  It allows an action to
notify its subscribers manually, rather than deferring to the system default of notifying on return.

Another useful feature is that parameter values can be customised for the recipients.

NOTE: Calling NotifySubscribers() does nothing to prevent the core from sending out an action notification as it
normally would, thus causing duplication.  To prevent this, the client must logical-or the return code of
the action function with `ERR::Notified`, e.g. `ERR::Okay|ERR::Notified`.

-INPUT-
obj Object: Pointer to the object that is to receive the notification message.
int(AC) Action: The action ID for notification.
ptr Args: Pointer to an action parameter structure that is relevant to the `Action` ID.
error Error: The error code that is associated with the action result.

-END-

*********************************************************************************************************************/

void NotifySubscribers(OBJECTPTR Object, AC ActionID, APTR Parameters, ERR ErrorCode)
{
   pf::Log log(__FUNCTION__);

   // No need for prv_access() since this function is called from within class action code only.

   if (!Object) { log.warning(ERR::NullArgs); return; }
   if ((ActionID <= AC::NIL) or (ActionID >= AC::END)) { log.warning(ERR::Args); return; }

   if (!(Object->NotifyFlags.load() & (1LL<<(LONG(ActionID) & 63)))) return;

   const std::lock_guard<std::recursive_mutex> lock(glSubLock);

   if ((!glSubscriptions[Object->UID].empty()) and (!glSubscriptions[Object->UID][LONG(ActionID)].empty())) {
      glSubReadOnly++; // Prevents changes to glSubscriptions while we're processing it.
      for (auto &sub : glSubscriptions[Object->UID][LONG(ActionID)]) {
         if (sub.Subscriber) {
            pf::SwitchContext ctx(sub.Subscriber);
            sub.Callback(Object, ActionID, ErrorCode, Parameters, sub.Meta);
         }
      }
      glSubReadOnly--;

      if (!glSubReadOnly) {
         if (!glDelayedSubscribe.empty()) {
            for (auto &entry : glDelayedSubscribe) {
               glSubscriptions[entry.ObjectID][LONG(entry.ActionID)].emplace_back(entry.Callback.Context, entry.Callback.Routine, entry.Callback.Meta);
            }
            glDelayedSubscribe.clear();
         }

         if (!glDelayedUnsubscribe.empty()) {
            for (auto &entry : glDelayedUnsubscribe) {
               if (Object->UID IS entry.ObjectID) UnsubscribeAction(Object, ActionID);
               else {
                  OBJECTPTR obj;
                  if (AccessObject(entry.ObjectID, 3000, &obj) IS ERR::Okay) {
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
      log.warning("Unstable subscription flags discovered for object #%d, action %d", Object->UID, LONG(ActionID));
      Object->NotifyFlags.fetch_and(~(1<<(LONG(ActionID) & 63)), std::memory_order::relaxed);
   }
}

/*********************************************************************************************************************

-FUNCTION-
QueueAction: Delay the execution of an action by adding the call to the message queue.

Use QueueAction() to execute an action by way of the local message queue.  This means that the supplied `Action` and
`Args` will be combined into a message for the queue.  This function then returns immediately.

The action will be executed on the next cycle of ~ProcessMessages() in line with the FIFO order of queued messages.

-INPUT-
int(AC) Action: The ID of an action or method to execute.
oid Object: The target object.
ptr Args:   The relevant argument structure for the `Action`, or `NULL` if not required.

-ERRORS-
Okay:
NullArgs:
OutOfRange: The `Action` ID is invalid.
NoMatchingObject:
MissingClass:
Failed:
IllegalMethodID:
-END-

*********************************************************************************************************************/

ERR QueueAction(AC ActionID, OBJECTID ObjectID, APTR Args)
{
   pf::Log log(__FUNCTION__);

   if ((ActionID IS AC::NIL) or (!ObjectID)) log.warning(ERR::NullArgs);
   if (ActionID >= AC::END) return log.warning(ERR::OutOfRange);

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
      if (ActionID > AC::NIL) {
         if (ActionTable[LONG(ActionID)].Size) {
            auto fields   = ActionTable[LONG(ActionID)].Args;
            auto argssize = ActionTable[LONG(ActionID)].Size;
            if (auto error = copy_args(fields, argssize, (BYTE *)Args, msg.Buffer, SIZE_ACTIONBUFFER, &msgsize, ActionTable[LONG(ActionID)].Name); error != ERR::Okay) {
               return error;
            }

            msg.Action.SendArgs = true;
         }
      }
      else if (auto cl = (extMetaClass *)FindClass(GetClassID(ObjectID))) {
         auto fields   = cl->Methods[-LONG(ActionID)].Args;
         auto argssize = cl->Methods[-LONG(ActionID)].Size;
         if (auto error = copy_args(fields, argssize, (BYTE *)Args, msg.Buffer, SIZE_ACTIONBUFFER, &msgsize, cl->Methods[-LONG(ActionID)].Name); error != ERR::Okay) {
            return error;
         }
         msg.Action.SendArgs = true;
      }
      else return log.warning(ERR::MissingClass);
   }

   if (auto error = SendMessage(MSGID_ACTION, MSF::NIL, &msg.Action, msgsize + sizeof(ActionMessage)); error != ERR::Okay) {
      if (ActionID > AC::NIL) {
         log.warning("#%d.%s() failed, SendMsg error: %s", ObjectID, ActionTable[LONG(ActionID)].Name, glMessages[LONG(error)]);
      }
      else log.warning("#%d method %d failed, SendMsg error: %s", ObjectID, LONG(ActionID), glMessages[LONG(error)]);

      if (error IS ERR::MemoryDoesNotExist) return ERR::NoMatchingObject;
      return error;
   }
   else return error;
}

/*********************************************************************************************************************

-FUNCTION-
ResolveClassName: Resolves any class name to a `CLASSID` UID.

This function will resolve a class `Name` to its `CLASSID` UID and verifies that the class is installed.  It is case
insensitive.

-INPUT-
cstr Name: The name of the class that requires resolution.

-RESULT-
cid: Returns the class ID identified from the class name, or `NULL` if the class could not be found.
-END-

*********************************************************************************************************************/

CLASSID ResolveClassName(CSTRING ClassName)
{
   if ((!ClassName) or (!*ClassName)) {
      pf::Log log(__FUNCTION__);
      log.warning(ERR::NullArgs);
      return CLASSID::NIL;
   }

   auto cid = CLASSID(strihash(ClassName));
   if (glClassDB.contains(cid)) return cid;
   else return CLASSID::NIL;
}

/*********************************************************************************************************************

-FUNCTION-
ResolveClassID: Resolve a valid `CLASSID` to its name.

This function will resolve a valid class ID to its equivalent name.  The name is resolved by checking the
class database, so the class must be registered in the database for this function to return successfully.

Registration is achieved by ensuring that the class is compiled into the build.

-INPUT-
cid ID: The ID of the class that needs to be resolved.

-RESULT-
cstr: Returns the name of the class, or `NULL` if the ID is not recognised.  Standard naming conventions apply, so it can be expected that the string is capitalised and without spaces, e.g. `NetSocket`.
-END-

*********************************************************************************************************************/

CSTRING ResolveClassID(CLASSID ID)
{
   if (glClassDB.contains(ID)) return glClassDB[ID].Name.c_str();

   pf::Log log(__FUNCTION__);
   log.warning("Failed to resolve ID $%.8x", ULONG(ID));
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

If the `Object` does not support the NewOwner action, or the `Owner` does not support the NewChild action, then the
process will not fail.  It will continue on the assumption that neither party is concerned about ownership management.

-INPUT-
obj Object: The object to modify.
obj Owner: The new owner for the `Object`.

-ERRORS-
Okay
NullArgs
Args
Recursion
SystemLocked
-END-

*********************************************************************************************************************/

ERR SetOwner(OBJECTPTR Object, OBJECTPTR Owner)
{
   pf::Log log(__FUNCTION__);

   if ((!Object) or (!Owner)) return log.warning(ERR::NullArgs);

   if (Object->Owner IS Owner) return ERR::Okay;

   if ((Object->ExtClass->Flags & CLF::NO_OWNERSHIP) != CLF::NIL) {
      log.traceWarning("Cannot set the object owner as CLF::NO_OWNERSHIP is set in its class.");
      return ERR::Okay;
   }

   if (Object IS Owner) return log.warning(ERR::Recursion);

   //log.msg("Object: %d, New Owner: %d, Current Owner: %d", Object->UID, Owner->UID, Object->ownerID());

   // Send a new child alert to the owner.  If the owner returns an error then we return immediately.

   ScopedObjectAccess objlock(Object);

   if (CheckAction(Owner, AC::NewChild) IS ERR::Okay) {
      struct acNewChild newchild = { .Object = Object };
      if (auto error = Action(AC::NewChild, Owner, &newchild); error != ERR::NoSupport) {
         if (error != ERR::Okay) { // If the owner has passed the object through to another owner, return ERR::Okay, otherwise error.
            if (error IS ERR::OwnerPassThrough) return ERR::Okay;
            else return error;
         }
      }
   }

   struct acNewOwner newowner = { .NewOwner = Owner };
   Action(AC::NewOwner, Object, &newowner);

   //if (Object->Owner) log.trace("SetOwner:","Changing the owner for object %d from %d to %d.", Object->UID, Object->ownerID(), Owner->UID);

   // Track the object's memory header to the new owner

   if (auto lock = std::unique_lock{glmMemory}) {
      auto mem = glPrivateMemory.find(Object->UID);
      if (mem IS glPrivateMemory.end()) return log.warning(ERR::SystemCorrupt);
      mem->second.OwnerID = Owner->UID;

      // Remove reference from the now previous owner
      if (Object->Owner) glObjectChildren[Object->Owner->UID].erase(Object->UID);

      Object->Owner = Owner;

      glObjectChildren[Owner->UID].insert(Object->UID);
      return ERR::Okay;
   }
   else return log.warning(ERR::SystemLocked);
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

   NewObject(CLASSID::BITMAP, &bitmap);
   AllocMemory(1000, MEM::DATA, &memory, NULL);

SetContext(ctx);
FreeResource(display->UID);
</pre>

The above code allocates a @Bitmap and a memory block, both of which will be contained by the display. When
~FreeResource() is called, both the bitmap and memory block will be automatically removed as they have a dependency
on the display's existence.  Please keep in mind that the following is incorrect:

<pre>
InitObject(display);
auto ctx = SetContext(display);

   NewObject(CLASSID::BITMAP, &bitmap);
   AllocMemory(1000, MEM::DATA, &memory, NULL);

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
obj Object: Pointer to the object that will take on the new context.  If `NULL`, no change to the context will be made.

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

This function sets the name of an `Object`.  This enhances log messages and allows the object to be found in searches.
Please note that the length of the `Name` will be limited to the value indicated in the core header file, under
the `MAX_NAME_LEN` definition.  Names exceeding the allowed length are trimmed to fit.

Object names are limited to alpha-numeric characters and the underscore symbol.  Invalid characters are replaced with
an underscore.

-INPUT-
obj Object: The target object.
cstr Name: The new name for the object.

-ERRORS-
Okay:
NullArgs:
Search: The `Object` is not recognised by the system - the address may be invalid.
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

ERR SetName(OBJECTPTR Object, CSTRING NewName)
{
   pf::Log log(__FUNCTION__);

   if ((!Object) or (!NewName)) return log.warning(ERR::NullArgs);

   ScopedObjectAccess objlock(Object);

   if (auto lock = std::unique_lock{glmObjectLookup, 4s}) {
      // Remove any existing name first.

      if (Object->Name[0]) remove_object_hash(Object);

      LONG i;
      for (i=0; (i < (MAX_NAME_LEN-1)) and (NewName[i]); i++) Object->Name[i] = sn_lookup[UBYTE(NewName[i])];
      Object->Name[i] = 0;

      if (Object->Name[0]) glObjectLookup[Object->Name].push_back(Object);
      return ERR::Okay;
   }
   else return log.warning(ERR::LockFailed);
}

/*********************************************************************************************************************

-FUNCTION-
SubscribeAction: Monitor action calls made against an object.

The SubscribeAction() function allows a client to receive a callback each time that an action is executed on
an object.  This strategy is referred to as "action monitoring" and is often used for responding to UI
events and the termination of objects.

Subscriptions are context sensitive, so the `Callback` will execute in the space attributed to to the caller.

The following example illustrates how to listen to a @Surface object's Redimension action and respond to resize
events:

<pre>
SubscribeAction(surface, AC::Redimension, C_FUNCTION(notify_resize, meta_ptr));
</pre>

The template below illustrates how the `Callback` function should be constructed:

<pre>
void notify_resize(OBJECTPTR Object, ACTIONID Action, ERR Result, APTR Parameters, APTR CallbackMeta)
{
   auto Self = (objClassType *)CurrentContext();

   // Code here...
   if ((Result == ERR::Okay) and (Parameters)) {
      auto resize = (struct acRedimension *)Parameters;
   }
}
</pre>

The `Object` is the original subscription target, as-is the Action ID.  The Result is the error code that was generated
at the end of the action call.  If this is not set to `ERR::Okay`, assume that the action did not have an effect on
state.  The `Parameters` are the original arguments provided by the client - be aware that these can legitimately be
`NULL` even if an action specifies a required parameter structure.  Notice that because subscriptions are context
sensitive, ~CurrentContext() can be used to get a reference to the object that initiated the subscription.

To terminate an action subscription, use the ~UnsubscribeAction() function.  Subscriptions are not resource tracked,
so it is critical to match the original call with an unsubscription.

-INPUT-
obj Object: The target object.
int(AC) Action: The ID of the action that will be monitored.  Methods are not supported.
ptr(func) Callback: A C/C++ function to callback when the action is triggered.

-ERRORS-
Okay:
NullArgs:
Args:
OutOfRange: The Action parameter is invalid.

*********************************************************************************************************************/

ERR SubscribeAction(OBJECTPTR Object, ACTIONID ActionID, FUNCTION *Callback)
{
   pf::Log log(__FUNCTION__);

   if ((!Object) or (!Callback)) return log.warning(ERR::NullArgs);
   if ((ActionID < AC::NIL) or (ActionID >= AC::END)) return log.warning(ERR::OutOfRange);
   if (!Callback->isC()) return log.warning(ERR::Args);
   if (Object->collecting()) return ERR::Okay;

   if (glSubReadOnly) {
      glDelayedSubscribe.emplace_back(Object->UID, ActionID, *Callback);
      Object->NotifyFlags.fetch_or(1LL<<(LONG(ActionID) & 63), std::memory_order::relaxed);
   }
   else {
      std::lock_guard<std::recursive_mutex> lock(glSubLock);

      glSubscriptions[Object->UID][LONG(ActionID)].emplace_back(Callback->Context, Callback->Routine, Callback->Meta);
      Object->NotifyFlags.fetch_or(1LL<<(LONG(ActionID) & 63), std::memory_order::relaxed);
   }

   return ERR::Okay;
}

/*********************************************************************************************************************

-FUNCTION-
UnsubscribeAction: Terminates action subscriptions.

The UnsubscribeAction() function will terminate subscriptions made by ~SubscribeAction().

To terminate multiple subscriptions in a single call, set the `Action` parameter to zero.

-INPUT-
obj Object: The object that you are unsubscribing from.
int(AC) Action: The ID of the action that will be unsubscribed, or zero for all actions.

-ERRORS-
Okay:
NullArgs:
Args:
-END-

*********************************************************************************************************************/

ERR UnsubscribeAction(OBJECTPTR Object, ACTIONID ActionID)
{
   pf::Log log(__FUNCTION__);

   if (!Object) return log.warning(ERR::NullArgs);
   if ((ActionID < AC::NIL) or (ActionID >= AC::END)) return log.warning(ERR::Args);

   if (glSubReadOnly) {
      glDelayedUnsubscribe.emplace_back(Object->UID, ActionID);
      return ERR::Okay;
   }

   std::lock_guard<std::recursive_mutex> lock(glSubLock);

   if (ActionID IS AC::NIL) { // Unsubscribe all actions associated with the subscriber.
      if (glSubscriptions.contains(Object->UID)) {
         auto subscriber = tlContext->object()->UID;
restart:
         for (auto & [action, list] : glSubscriptions[Object->UID]) {
            for (auto it = list.begin(); it != list.end(); ) {
               if (it->SubscriberID IS subscriber) it = list.erase(it);
               else it++;
            }

            if (list.empty()) {
               Object->NotifyFlags.fetch_and(~(1<<(action & 63)), std::memory_order::relaxed);

               if (!Object->NotifyFlags.load()) {
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
   else if ((glSubscriptions.contains(Object->UID)) and (glSubscriptions[Object->UID].contains(LONG(ActionID)))) {
      auto subscriber = tlContext->object()->UID;

      auto &list = glSubscriptions[Object->UID][LONG(ActionID)];
      for (auto it = list.begin(); it != list.end(); ) {
         if (it->SubscriberID IS subscriber) it = list.erase(it);
         else it++;
      }

      if (list.empty()) {
         Object->NotifyFlags.fetch_and(~(1<<(LONG(ActionID) & 63)), std::memory_order::relaxed);

         if (!Object->NotifyFlags.load()) {
            glSubscriptions.erase(Object->UID);
         }
         else glSubscriptions[Object->UID].erase(LONG(ActionID));
      }
   }

   return ERR::Okay;
}
