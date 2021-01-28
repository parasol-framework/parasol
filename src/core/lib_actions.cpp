/****************************************************************************

The source code of the Parasol Framework is made publicly available under the
terms described in the LICENSE.TXT file that is distributed with this package.
Please refer to it for further information on licensing.

-CATEGORY-
Name: Objects
-END-

****************************************************************************/

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

//****************************************************************************
// Action subscription array locking.

static ActionSubscription * lock_subscribers(OBJECTPTR Object)
{
   ActionSubscription *list;

   if (Object->UniqueID < 0) {
      if (Object->Stats->ActionSubscriptions.ID) {
         if (!AccessMemory(Object->Stats->ActionSubscriptions.ID, MEM_READ, 2000, (APTR *)&list)) {
            return list;
         }
         else return NULL;
      }
      else return NULL;
   }
   else return (ActionSubscription *)Object->Stats->ActionSubscriptions.Ptr;
}

INLINE void unlock_subscribers(OBJECTPTR Object)
{
   if (Object->UniqueID < 0) ReleaseMemoryID(Object->Stats->ActionSubscriptions.ID);
}

//****************************************************************************

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

//****************************************************************************
// Refer to ActionThread() to see how this is used.  It calls an action on a target object and then sends a callback
// notification via the internal message queue.

struct thread_data {
   OBJECTPTR Object;
   ACTIONID  ActionID;
   LONG      Key;
   FUNCTION  Callback;
   BYTE      Parameters;
};

static ERROR thread_action(rkThread *Thread)
{
   ERROR error;
   thread_data *data = (thread_data *)Thread->Data;
   OBJECTPTR obj = data->Object;

   if (!(error = AccessPrivateObject(obj, 5000))) { // Access the object and process the action.
      __sync_sub_and_fetch(&obj->ThreadPending, 1);
      error = Action(data->ActionID, obj, data->Parameters ? (data + 1) : NULL);

      if (data->Parameters) { // Free any temporary buffers that were allocated.
         if (data->ActionID > 0) local_free_args(data + 1, ActionTable[data->ActionID].Args);
         else local_free_args(data + 1, obj->Class->Methods[-data->ActionID].Args);
      }

      if (obj->Flags & NF_FREE) obj = NULL; // Clear the obj pointer because the object will be deleted on release.
      ReleasePrivateObject(data->Object);
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

// Free all private memory resources tracked to an object.  Do this before deallocating public objects
// because private objects may want to remove resources from objects that are in public memory.

static void free_private_children(OBJECTPTR Object)
{
   parasol::Log log;

   ThreadLock lock(TL_PRIVATE_MEM, 4000);
   if (lock.granted()) {
      {
         const auto children = glObjectChildren[Object->UniqueID]; // Take an immutable copy of the resource list

         for (const auto id : children) {
            auto it = glPrivateMemory.find(id);
            if (it IS glPrivateMemory.end()) continue;
            auto &mem = it->second;

            if ((mem.Flags & MEM_DELETE) or (!mem.Object)) continue;

            if (mem.Object->OwnerID != Object->UniqueID) {
               log.warning("Failed sanity test: Child object #%d has owner ID of #%d that does not match #%d.", mem.Object->UniqueID, mem.Object->OwnerID, Object->UniqueID);
               continue;
            }

            if (!(mem.Object->Flags & NF_UNLOCK_FREE)) {
               if (mem.Object->Flags & NF_INTEGRAL) {
                  log.warning("Found unfreed child object #%d (class %s) belonging to %s object #%d.", mem.Object->UniqueID, ResolveClassID(mem.Object->ClassID), Object->Class->ClassName, Object->UniqueID);
               }
               acFree(mem.Object);
            }
         }
      }

      {
         const auto list = glObjectMemory[Object->UniqueID]; // Take an immutable copy of the resource list

         for (const auto id : list) {
            auto it = glPrivateMemory.find(id);
            if (it IS glPrivateMemory.end()) continue;
            auto &mem = it->second;

            if ((mem.Flags & MEM_DELETE) or (!mem.Address)) continue;

            if (glLogLevel >= 3) {
               if (mem.Flags & MEM_STRING) {
                  log.warning("Unfreed string \"%.40s\"", (CSTRING)mem.Address);
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

      glObjectChildren.erase(Object->UniqueID);
      glObjectMemory.erase(Object->UniqueID);
   }
}

// Free all public memory resources and objects tracked to this object

static void free_public_children(OBJECTPTR Object)
{
   parasol::Log log;

   if (!(Object->Flags & NF_HAS_SHARED_RESOURCES)) return;

   ScopedSysLock lock(PL_PUBLICMEM, 5000);
   if (lock.granted()) {
      for (LONG i=glSharedControl->NextBlock-1; i >= 0; i--) {
         if ((glSharedBlocks[i].ObjectID IS Object->UniqueID) and (glSharedBlocks[i].MemoryID)) {
            if (glSharedBlocks[i].Flags & MEM_DELETE) continue; // Ignore blocks already marked for deletion

            OBJECTPTR child;
            if (!(glSharedBlocks[i].Flags & MEM_OBJECT)) {
               log.warning("Unfreed public memory: #%d, Size %d, Object #%d, Access %d.", glSharedBlocks[i].MemoryID, glSharedBlocks[i].Size, glSharedBlocks[i].ObjectID, glSharedBlocks[i].AccessCount);
               FreeResourceID(glSharedBlocks[i].MemoryID);
            }
            else if (!page_memory(glSharedBlocks + i, (APTR *)&child)) {
               OBJECTID id = child->UniqueID;
               if (!(child->Flags & (NF_UNLOCK_FREE|NF_FREE_MARK))) {
                  child->Flags |= NF_FREE_MARK;
                  if (child->Flags & NF_INTEGRAL) log.warning("Found unfreed object #%d (class $%.8x).", id, child->ClassID);
                  unpage_memory(child);
                  lock.release();

                  ActionMsg(AC_Free, id, NULL, 0, 0);

                  if (lock.acquire(5000) != ERR_Okay) {
                     log.warning(ERR_SystemLocked);
                     break;
                  }
                  i = glSharedControl->NextBlock-1; // Reset the counter because we gave up control
               }
               else unpage_memory(child);
            }
         }
      }
   }
}

/*****************************************************************************

-FUNCTION-
Action: This function is responsible for executing action routines.

This function is the key entry point for executing actions and method routines.  An action is a predefined function
call that can be called on any object, while a method is a function call that is specific to a particular object
type.  You can find a complete list of available actions and their associated details in the Action List document.
If an object supports methods, they will be listed in the object's class document.

Here are two examples that demonstrate how to make an action call.  The first performs an initialisation, which
does not require any additional arguments.  The second performs a move operation, which requires three additional
arguments to be passed to the Action() function:

<pre>
1. Action(AC_Init, Picture, NULL);

2. struct acMove move = { 30, 15, 0 };
   Action(AC_Move, Window, &move);
</pre>

In all cases, action calls can be simplified by using their corresponding macros:

<pre>
1. acInit(Picture);

2. acMove(Window, 30, 15, 0);
</pre>

If the target object does not support the action code that has been specified, an error code of ERR_NoSupport will be
returned.  If you need to test an object to see if it supports a particular action, use the ~CheckAction()
function.

If you need to send an action to an object that does not belong to your task space, use the ~ActionMsg()
function.  If you're in a situation where you only have an object ID and are unsure as to whether or not the object is
in your task space, use ~ActionMsg() anyway as it will divert to the Action() function if the object is local.

If you are writing a class and need to know how to add support for a particular action, look it up in the Action
Support Guide.

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

*****************************************************************************/

ERROR Action(LONG ActionID, OBJECTPTR argObject, APTR Parameters)
{
   parasol::Log log(__FUNCTION__);

   if (!argObject) return log.warning(ERR_NullArgs);

   auto obj = (OBJECTPTR)argObject;
   OBJECTID object_id = obj->UniqueID;

   prv_access(obj);

   // If the object is private, check that the task making this call has ownership of its memory.  If not, we must fail
   // as foreign exclusive access does not allow use of Action().

   #ifdef DEBUG
   if ((ActionID != AC_AccessObject) and (ActionID != AC_ReleaseObject)) {
      if (!(obj->Flags & NF_PUBLIC)) {
         if ((obj->TaskID) and (glCurrentTaskID != obj->TaskID) and (obj->TaskID != SystemTaskID)) {
            if (object_id < 0) {
               log.warning("Public object #%d corrupt - missing the NF_PUBLIC flag ($%.8x)", object_id, obj->Flags);
               prv_release(obj);
               return ERR_ObjectCorrupt;
            }
            log.warning("Cannot execute action %s on non-public object #%d, class $%.8x (belongs to task %d, I am %d).", action_name(obj, ActionID), object_id, obj->ClassID, obj->TaskID, glCurrentTaskID);
            prv_release(obj);
            return ERR_IllegalActionAttempt;
         }
      }
   }
   #endif

   // Set the current context to the given object

   ObjectContext new_context = { .Stack = tlContext, .Object = obj, .Field = NULL, .Action = (WORD)ActionID };
   tlContext = &new_context;

   obj->ActionDepth++;

   rkMetaClass *cl = obj->Class;
   auto log_depth = tlDepth;

   ERROR error;
   if (ActionID > 0) {
      // Action precedence is as follows:
      //
      // 1. Managed actions.
      // 2. If applicable, the object's sub-class (e.g. Picture:JPEG).
      // 3. The base-class.

      if (ActionID > glActionCount) error = log.warning(ERR_IllegalActionID);
      else if (ManagedActions[ActionID]) error = ManagedActions[ActionID](obj, Parameters);
      else if (cl->ActionTable[ActionID].PerformAction) error = cl->ActionTable[ActionID].PerformAction(obj, Parameters);
      else if ((cl->Base) and (cl->Base->ActionTable[ActionID].PerformAction)) {
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
   else if (obj->Stats->ActionSubscriptions.Ptr) {  // Check if there are any objects subscribed to this action before continuing
      LONG result;
      if (ActionID > 0) result = obj->Stats->NotifyFlags[ActionID>>5] & (1<<(ActionID & 31));
      else result = obj->Stats->MethodFlags[(-ActionID)>>5] & (1<<((-ActionID) & 31));

      if (result) {
         ActionSubscription *list;
         if ((list = lock_subscribers(obj))) {
            acActionNotify notify;
            notify.ObjectID = object_id;
            notify.Args     = Parameters;
            if (ActionID > 0) notify.Size = ActionTable[ActionID].Size;
            else if (cl->Base) {
               if (cl->Base->Methods) notify.Size = cl->Base->Methods[-ActionID].Size;
               else notify.Size = 0;
            }
            else if (cl->Methods) notify.Size = cl->Methods[-ActionID].Size;
            else notify.Size = 0;
            notify.ActionID = ActionID;
            if (error IS ERR_NoAction) notify.Error = ERR_Okay;
            else notify.Error = error;

            LONG recursionsave = tlMsgRecursion;
            tlMsgRecursion = 255; // This prevents ProcessMessages() from being used while inside notification routines

            for (WORD i=0; (i < obj->Stats->SubscriptionSize); i++) {
               if (list[i].ActionID IS ActionID) {
                  if (ActionMsg(AC_ActionNotify, list[i].SubscriberID, &notify, list[i].MessagePortMID, list[i].ClassID) IS ERR_MemoryDoesNotExist) {
                     // If the message port does not exist, remove the object from the list.

                     log.trace("Removing object #%d from the notification list for object #%d.", list[i].SubscriberID, object_id);
                     list[i].SubscriberID   = 0;
                     list[i].ActionID       = 0;
                     list[i].MessagePortMID = 0;
                     list[i].ClassID        = 0;
                  }
               }
            }

            tlMsgRecursion = recursionsave;

            unlock_subscribers(obj);
         }
         else log.traceWarning("Denied access to ActionSubscriptions #%d of object #%d.", obj->Stats->ActionSubscriptions.ID, object_id);
      }
   }

   if (ActionID != AC_Free) obj->ActionDepth--;

   prv_release(obj);
   tlContext = tlContext->Stack;

   if (log_depth != tlDepth) {
      if (ActionID >= 0) {
         log.warning("Call to #%d.%s() failed to debranch the log correctly (%d <> %d).", object_id, ActionTable[ActionID].Name, log_depth, tlDepth);
      }
      else log.warning("Call to #%d.%s() failed to debranch the log correctly (%d <> %d).", object_id, cl->Base->Methods[-ActionID].Name, log_depth, tlDepth);
   }

   return error;
}

/*****************************************************************************

-FUNCTION-
ActionList: Returns a pointer to the system's action table.

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

*****************************************************************************/

void ActionList(struct ActionTable **List, LONG *Size)
{
   if (List) *List = (struct ActionTable *)ActionTable;
   if (Size) *Size = AC_END;
}

/*****************************************************************************

-FUNCTION-
ActionMsg: Provides a mechanism for sending actions to objects that belong to other tasks.

ActionMsg() will execute an action on any shared object that does not reside within the caller's process space.  It
first tests the target object to check if it it belongs to the calling process.  If the object is local then the
action will be called immediately, as if ~Action() was called under normal circumstances.  Otherwise the
process that owns the object will be sent a message that requests the action be called with the given parameters.
Functionality then returns to the caller.

After sending the action, assume that the other process will respond to the message and execute the action in a short
time frame.  This will occur when the process makes its next call to ProcessMessages().

Some alternative macros are available in addition to ActionMsg().  The DelayMsg() macro will force a message to be
queued if the target object belongs to your program.  The action will not be processed until ProcessMessages() is
called.

The WaitMsg() macro will cause your process to sleep while the other process executes the action.  The function will
return with the result parameters and the error code from the other process unless a timeout occurs, in which case
ERR_TimeOut is returned.

-INPUT-
int Action: The ID of the action that you want to execute.
oid Object: The ID of the object to receive the action.
ptr Args:   The argument structure related to the specific action that you are calling.
mem MessageID: Set to zero.
cid ClassID: Set to zero.

-ERRORS-
Okay: The message was successfully passed to the object (this is not indicative of whether or not the object was actually successful in executing the action).
Args:
NoMatchingObject: There is no object for the given Object.
TimeOut: Timeout limit reached while waiting for a result from the foreign process.  Applies to WaitMsg() only.
-END-

*****************************************************************************/

struct msgAction {
   struct Message Message;
   struct ActionMessage Action;
   BYTE Buffer[SIZE_ACTIONBUFFER];
};

ERROR ActionMsg(LONG ActionID, OBJECTID ObjectID, APTR Args, MEMORYID MessageMID, CLASSID ClassID)
{
   parasol::Log log(__FUNCTION__);

   if ((!ActionID) or (ActionID >= AC_END)) {
      log.warning("Invalid arguments: Action: %d, Object: %d", ActionID, ObjectID);
      return ERR_Args;
   }

   if (!ObjectID) {
      if (ActionID > 0) log.function("Object: 0, Action: %s", ActionTable[ActionID].Name);
      else log.function("Object: 0, Method: %d", ActionID);
      return ERR_NullArgs;
   }

   // If the ClassID has been passed as -1, this indicates that the DelayMsg() function macro has been used to call this
   // function.  Delaying the call guarantees that the action is queued if the object is in our process space.

   BYTE wait = FALSE;
   BYTE delay = FALSE;
   if (ClassID IS (CLASSID)-1) {
      delay = TRUE;
      ClassID = 0;
   }
   else if (ClassID IS (CLASSID)-2) {
      wait = TRUE;
      ClassID = 0;
   }

   // Class ID can be zero if executing an action, and is only required when executing a method (it is necessary to
   // lookup the method structure - its size, fields etc and we need the class ID to do that).

   // Get the message port that manages this object

   SharedObjectHeader *header;
   SharedObject *list;
   LONG pos, msgsize;

   if (!MessageMID) {
      if (ObjectID > 0) { // Object is private (local)
         MessageMID = glTaskMessageMID;
         ClassID = 0;
      }
      else if (!AccessMemory(RPM_SharedObjects, MEM_READ, 2000, (void **)&header)) {
         if (!find_public_object_entry(header, ObjectID, &pos)) {
            list = (SharedObject *)ResolveAddress(header, header->Offset);
            MessageMID = list[pos].MessageMID;
            ClassID    = list[pos].ClassID;
            ReleaseMemoryID(RPM_SharedObjects);

            // The MessageMID can be zero if the object is not assigned an owner (e.g. CLF_NO_OWNERSHIP is defined by the object's class).

            if (!MessageMID) MessageMID = glTaskMessageMID;
         }
         else {
            ReleaseMemoryID(RPM_SharedObjects);
            return ERR_NoMatchingObject;
         }
      }
      else return log.warning(ERR_AccessMemory);
   }

   // If the object belongs to the message port of our task, execute the action immediately (unless a delay has been requested).

#ifdef _WIN32
   WINHANDLE thread_msg = 0;
#else
   LONG thread_msg = 0;
#endif

   ERROR error;
   if ((MessageMID IS glTaskMessageMID) and (delay IS FALSE)) {
      OBJECTPTR object;
      if (!(error = AccessObject(ObjectID, 1000, &object))) {
         if ((ObjectID > 0) and ((thread_msg = object->ThreadMsg) != tlThreadWriteMsg)) {
            // The object belongs to a separate internal thread, so release the object and let the other thread handle it.
            ReleaseObject(object);
         }
         else {
            error = Action(ActionID, object, Args);
            ReleaseObject(object);
            return error;
         }
      }
      else if (error != ERR_TimeOut) return error;
   }
/*
   if (ActionID > 0) log.msg("Passing action %s to object #%d.", ActionTable[ActionID].Name, ObjectID);
   else log.msg("Passing method %d to object #%d.", ActionID, ObjectID);
*/
   // Copy the argument structure to the message argument section

   msgAction msg;

   msg.Action.ObjectID      = ObjectID;
   msg.Action.ActionID      = ActionID;
   msg.Action.SendArgs      = FALSE;
   msg.Action.ReturnResult  = FALSE;
   msg.Action.Delayed       = delay;
   msg.Action.Error         = ERR_Okay;
   msg.Action.Time          = 0;
   msg.Action.ReturnMessage = glTaskMessageMID;

   const FunctionField *fields = NULL;
   LONG argssize = 0;

   if (Args) {
      if (ActionID > 0) {
         if (ActionTable[ActionID].Size) {
            fields   = ActionTable[ActionID].Args;
            argssize = ActionTable[ActionID].Size;
            WORD waitresult;
            if (copy_args(fields, argssize, (BYTE *)Args, msg.Buffer, SIZE_ACTIONBUFFER, &msgsize, &waitresult, ActionTable[ActionID].Name) != ERR_Okay) {
               log.warning("Failed to buffer arguments for action \"%s\".", ActionTable[ActionID].Name);
               return ERR_Failed;
            }

            msgsize += sizeof(ActionMessage);
            msg.Action.SendArgs = TRUE;
         }
         else msgsize = sizeof(ActionMessage);
      }
      else {
         if (!ClassID) {
            if (!(ClassID = GetClassID(ObjectID))) {
               log.warning("Class ID indeterminable for object %d - cannot execute action %d.", ObjectID, ActionID);
               return ERR_Failed;
            }
         }

         rkMetaClass *Class;
         if ((Class = FindClass(ClassID))) {
            if ((-ActionID) < Class->TotalMethods) {
               fields   = Class->Methods[-ActionID].Args;
               argssize = Class->Methods[-ActionID].Size;
               WORD waitresult;
               if (copy_args(fields, argssize, (BYTE *)Args, msg.Buffer, SIZE_ACTIONBUFFER, &msgsize, &waitresult, Class->Methods[-ActionID].Name) != ERR_Okay) {
                  log.warning("Failed to buffer arguments for method \"%s\".", Class->Methods[-ActionID].Name);
                  return ERR_Failed;
               }
               msgsize += sizeof(ActionMessage);
               msg.Action.SendArgs = TRUE;
            }
            else {
               log.warning("Illegal method ID %d executed on class %s.", ActionID, Class->ClassName);
               return ERR_IllegalMethodID;
            }
         }
         else return log.warning(ERR_MissingClass);
      }
   }
   else msgsize = sizeof(ActionMessage);

   if (wait IS TRUE) msg.Action.ReturnResult = TRUE;
   else msg.Action.ReturnResult = FALSE;

retry:
   if (thread_msg) error = send_thread_msg(thread_msg, MSGID_ACTION, &msg.Action, msgsize);
   else error = SendMessage(MessageMID, MSGID_ACTION, 0, &msg.Action, msgsize);

   if (error) {
      if ((error IS ERR_MemoryDoesNotExist) and (ObjectID < 0)) {
         // If the message queue does not exist for a shared object, the object can still remain in memory if it is
         // untracked - so try to execute the action directly.

         log.warning("Object #%d is orphaned and will now be disowned.", ObjectID);

         if (!AccessMemory(RPM_SharedObjects, MEM_READ, 2000, (void **)&header)) {
            // Wipe the message MID so that all future executions are within the caller's process space
            if (find_public_object_entry(header, ObjectID, &pos) IS ERR_Okay) {
               list = (SharedObject *)ResolveAddress(header, header->Offset);
               list[pos].MessageMID = 0;
            }
            ReleaseMemoryID(RPM_SharedObjects);
         }

         OBJECTPTR object;
         if (delay) {  // Since a delay is enforced, try again but with our task message queue.
            if (!AccessObject(ObjectID, 1000, &object)) {
               object->TaskID = 0; // The originating task no longer exists
               ReleaseObject(object);
            }
            MessageMID = glTaskMessageMID;
            goto retry;
         }

         if (!AccessObject(ObjectID, 1000, &object)) {
            object->TaskID = 0; // The originating task no longer exists
            error = Action(ActionID, object, Args);
            ReleaseObject(object);
            return error;
         }
      }

      if (ActionID > 0) {
         if (ActionID IS AC_ActionNotify) {
            struct acActionNotify *notify = (struct acActionNotify *)Args;
            log.warning("Action %s on object #%d failed, SendMsg error: %s", ActionTable[notify->ActionID].Name, ObjectID, glMessages[error]);
         }
         else log.warning("Action %s on object #%d failed, SendMsg error: %s", ActionTable[ActionID].Name, ObjectID, glMessages[error]);
      }
      else log.warning("Method %d on object #%d failed, SendMsg error: %s", ActionID, ObjectID, glMessages[error]);

      if (error IS ERR_MemoryDoesNotExist) error = ERR_NoMatchingObject; // If the queue does not exist, the object does not exist

      return error;
   }

   // Wait for the other task to send back a result if we are required to do so.  If the task disappears or does not
   // respond within 10 seconds we cancel the wait and return a time-out error.

   if ((wait IS TRUE) and (glTaskMessageMID != MessageMID)) {
      msgAction receive;
      if (!GetMessage(glTaskMessageMID, MSGID_ACTION_RESULT, MSF_WAIT, &receive, msgsize + sizeof(Message))) {
         LONG j;

         // Here we convert the returned structure over to the current process space. We are mostly interested in
         // pointer and result variables, and we also free any allocated memory blocks.
         //
         //    The src_msg contains offsets and memory ID's.
         //    The result_msg contains structural results (keep in mind that any address pointers come from the foreign task).
         //    The Dest is the argument structure that we need to copy the results to.

         //log.msg("[%d] Action: %d, Error: %d", receive.Action.ObjectID, receive.Action.ActionID, receive.Action.Error);

         BYTE *src_msg = msg.Buffer;
         BYTE *result_msg = receive.Buffer;
         BYTE *dest = (BYTE *)Args;
         LONG pos = 0;
         for (LONG i=0; fields[i].Name; i++) {
            if (fields[i].Type & FD_RESULT) {
               if (fields[i].Type & FD_LONG) {
                  LONG *dest_long = (LONG *)(dest + pos);
                  *dest_long = ((LONG *)(result_msg + pos))[0];
               }
               else if (fields[i].Type & (FD_DOUBLE|FD_LARGE)) {
                  LARGE *dest_large = (LARGE *)(dest + pos);
                  *dest_large = ((LARGE *)(result_msg + pos))[0];
               }
               else if (fields[i].Type & FD_STR) {
                  LONG *dest_long = ((LONG **)(dest + pos))[0];
                  LONG *src_long  = (LONG *)(result_msg + ((LONG *)(src_msg+pos))[0]);
                  *dest_long = *src_long;
               }
               else if (fields[i].Type & FD_PTR) {
                  if (fields[i+1].Type & FD_PTRSIZE) {
                     // Copy the data from the memory buffer we allocated earlier to the buffer in the destination
                     // argument structure.

                     if (((MEMORYID *)(src_msg + pos))[0]) {
                        MEMORYID id = ((MEMORYID *)(src_msg + pos))[0];
                        BYTE *src;
                        if (!AccessMemory(id, MEM_READ_WRITE, 2000, (void **)&src)) {
                           BYTE *copy = ((BYTE **)(dest + pos))[0];
                           for (j=0; j < ((LONG *)(dest + pos + sizeof(APTR)))[0]; j++) copy[j] = src[j];
                           ReleaseMemoryID(id);
                        }
                        FreeResourceID(((MEMORYID *)(src_msg + pos))[0]);
                     }
                  }
               }
               else log.warning("Bad type definition for argument \"%s\".", fields[i].Name);

               pos += sizeof(APTR);
            }
            else {
               if (fields[i].Type & (FD_DOUBLE|FD_LARGE)) pos += sizeof(LARGE);
               else if (fields[i].Type & FD_PTR) pos += sizeof(APTR);
               else pos += sizeof(LONG);
            }
         }

         return receive.Action.Error;
      }
      else {
         log.warning("Time-out waiting for foreign process to return action results.");
         return ERR_TimeOut;
      }
   }
   else return ERR_Okay;
}

/*****************************************************************************

-FUNCTION-
ActionTags: This is a tag-based version of the Action() function.

This function is identical to the ~Action() function in every respect, except that it allows you to send
action arguments as tags rather than in a structure.  Here are a few examples:

<pre>
ActionTags(AC_Resize, Object, 100, 300, 200);
ActionTags(MT_Rename, Object, "user:documents.txt");
</pre>

Whether you choose to use ActionTags() or ~Action() to make your action calls is a matter of personal
preference.  However, be aware that ActionTags() does not support actions or methods that return results in the
argument structure - e.g. Read and Write actions are not supported.

-INPUT-
int Action: The ID of the action or method that you want to execute.
obj Object: The object that is to receive the action.
tags Tags:   Each argument that would normally be specified in the action structure must be listed as tags, <i>in the same order</i> as the fields in the original action structure.  There is no need to terminate the tag-list.

-ERRORS-
Okay
Args
NullArgs
ObjectCorrupt
IllegalActionAttempt
IllegalActionID
NoAction
IllegalMethodID: The Action is not a supported method of the target object.
-END-

*****************************************************************************/

ERROR ActionTags(LONG ActionID, OBJECTPTR argObject, ...)
{
   parasol::Log log(__FUNCTION__);
   ActionSubscription *list;
   ERROR error;
   LONG result;
   va_list vlist;

   if (!argObject) return log.warning(ERR_NullArgs);
   if (ActionID > glActionCount) return log.warning(ERR_IllegalActionID);

   OBJECTPTR obj = (OBJECTPTR)argObject;
   OBJECTID object_id = obj->UniqueID;
   rkMetaClass *sysclass = obj->Class;

   prv_access(obj);

   // If the object is private, check that the task making this call has ownership of its memory.  If not, we must fail
   // as foreign exclusive access does not allow use of Action().

   if ((ActionID != AC_AccessObject) and (ActionID != AC_ReleaseObject)) {
      if (!(obj->Flags & NF_PUBLIC)) {
         if ((obj->TaskID) and (glCurrentTaskID != obj->TaskID) and (obj->TaskID != SystemTaskID)) {
            if (object_id < 0) {
               log.warning("Public object #%d corrupt - missing the NF_PUBLIC flag ($%.8x)", object_id, obj->Flags);
               prv_release(obj);
               return ERR_ObjectCorrupt;
            }
            if (ActionID > 0) {
               log.warning("Cannot execute action %s on non-public object #%d, class $%.8x (belongs to task %d, I am %d).", ActionTable[ActionID].Name, object_id, obj->ClassID, obj->TaskID, glCurrentTaskID);
            }
            else log.warning("Cannot execute method %s on non-public object #%d (belongs to task %d, I am %d).", sysclass->Methods[-ActionID].Name, object_id, obj->TaskID, glCurrentTaskID);
            prv_release(obj);
            return ERR_IllegalActionAttempt;
         }
      }
   }

   const FunctionField *fields;
   LONG argssize;
   if (ActionID > 0) {
      fields   = ActionTable[ActionID].Args;
      argssize = ActionTable[ActionID].Size;
   }
   else if ((sysclass->Base) and ((-ActionID) < sysclass->Base->TotalMethods)) { // The base class takes precedence for the correct definition of method arguments. Sub-classes should typically avoid declaration of method arguments.
      fields   = sysclass->Base->Methods[-ActionID].Args;
      argssize = sysclass->Base->Methods[-ActionID].Size;
   }
   else if ((-ActionID) < sysclass->TotalMethods) { // A sub-class can legally define its own methods, but should choose ID's >= 50 in order to avoid the potential for conflict with new methods appearing in the base class.
      fields   = sysclass->Methods[-ActionID].Args;
      argssize = sysclass->Methods[-ActionID].Size;
   }
   else {
      log.warning("Illegal method ID %d executed on class %s.", ActionID, sysclass->ClassName);
      prv_release(obj);
      return ERR_IllegalMethodID;
   }

   if (argssize <= 0) {
      // Calling ActionTags() on an action that does not declare arguments makes no sense and is not supported (use
      // Action() instead)
      log.warning("Do not call ActionTags() on an action that does not declare arguments. Use Action() instead.");
      prv_release(obj);
      return ERR_IllegalActionAttempt;
   }

   // Set the current context to the given object

   struct ObjectContext new_context = {
      .Stack  = tlContext,
      .Object = (OBJECTPTR)obj,
      .Field  = NULL,
      .Action = (WORD)ActionID
   };
   tlContext = &new_context;

   obj->ActionDepth++;

   char params[argssize];
   char *dest = params;

   va_start(vlist, argObject);

   for (WORD i=0; fields[i].Name; i++) {
      if (fields[i].Type & FD_LONG) {
         ((LONG *)dest)[0] = va_arg(vlist, LONG);
         dest += sizeof(LONG);
      }
      else if (fields[i].Type & (FD_DOUBLE|FD_LARGE)) {
         ((LARGE *)dest)[0] = va_arg(vlist, LARGE);
         dest += sizeof(LARGE);
      }
      else if (fields[i].Type & (FD_STR|FD_PTR)) {
         ((APTR *)dest)[0] = va_arg(vlist, APTR);
         dest += sizeof(APTR);
      }
      else log.warning("Bad type definition for argument \"%s\".", fields[i].Name);
   }

   va_end(vlist);

   if (ActionID > 0) {
      if (ManagedActions[ActionID]) {
         error = ManagedActions[ActionID](obj, params);
      }
      else if (sysclass->ActionTable[ActionID].PerformAction) {
         error = sysclass->ActionTable[ActionID].PerformAction(obj, params);
      }
      else if (sysclass->Base) {
         if (sysclass->Base->ActionTable[ActionID].PerformAction) {
            error = sysclass->Base->ActionTable[ActionID].PerformAction(obj, params);
         }
         else error = ERR_NoAction;
      }
      else error = ERR_NoAction;
   }
   else { // Method call
      ERROR (*routine)(OBJECTPTR, APTR);

      if (sysclass->Methods) {
         if (sysclass->Methods[-ActionID].Routine) {
            routine = (ERROR (*)(OBJECTPTR, APTR))sysclass->Methods[-ActionID].Routine;
            error = routine(obj, params);
         }
         else error = ERR_NoAction;
      }
      else error = ERR_NoAction;

      // If this is a child, check the base class

      if (((error IS ERR_NoAction) /*OR (!error)*/) and (sysclass->Base)) {
         if (sysclass->Base->Methods) {
            if (sysclass->Base->Methods[-ActionID].Routine) {
               routine = (ERROR (*)(OBJECTPTR, APTR))sysclass->Base->Methods[-ActionID].Routine;
               error = routine(obj, params);
            }
         }
      }
   }

   // If the object has action subscribers, check if any of them are listening to this particular action, and if so,
   // notify them.

   if (error & ERF_Notified) {
      error &= ~ERF_Notified;
   }
   else if (obj->Stats->ActionSubscriptions.Ptr) {
      // Check if there are any objects subscribed to this action before continuing

      if (ActionID > 0) result = obj->Stats->NotifyFlags[ActionID>>5] & (1<<(ActionID & 31));
      else result = obj->Stats->MethodFlags[(-ActionID)>>5] & (1<<((-ActionID) & 31));

      if (result) {
         if ((list = lock_subscribers(obj))) {
            acActionNotify notify;
            notify.ObjectID = object_id;
            notify.Args     = params;
            if (ActionID > 0) notify.Size = ActionTable[ActionID].Size;
            else if (sysclass->Base) {
               if (sysclass->Base->Methods) notify.Size = sysclass->Base->Methods[-ActionID].Size;
               else notify.Size = 0;
            }
            else {
               if (sysclass->Methods) notify.Size = sysclass->Methods[-ActionID].Size;
               else notify.Size = 0;
            }
            notify.ActionID = ActionID;
            if (error IS ERR_NoAction) notify.Error = ERR_Okay;
            else notify.Error = error;

            LONG recursionsave = tlMsgRecursion;
            tlMsgRecursion = 255; // This prevents ProcessMessages() from being used while inside notification routines

            for (LONG i=0; (i < obj->Stats->SubscriptionSize); i++) {
               if (list[i].ActionID IS ActionID) {
                  if (ActionMsg(AC_ActionNotify, list[i].SubscriberID, &notify, list[i].MessagePortMID, list[i].ClassID) IS ERR_MemoryDoesNotExist) {
                     // If the message port does not exist, remove the object from the list.

                     log.trace("Removing object #%d from the notification list for object #%d.", list[i].SubscriberID, object_id);
                     list[i].SubscriberID   = 0;
                     list[i].ActionID       = 0;
                     list[i].MessagePortMID = 0;
                     list[i].ClassID        = 0;
                  }
               }
            }

            tlMsgRecursion = recursionsave;

            unlock_subscribers(obj);
         }
         else log.traceWarning("Denied access to ActionSubscriptions #%d of object #%d.", obj->Stats->ActionSubscriptions.ID, object_id);
      }
   }

   // Put the context settings back

   if (ActionID != AC_Free) obj->ActionDepth--;
   prv_release(obj);
   tlContext = tlContext->Stack;
   return error;
}

/*****************************************************************************

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

*****************************************************************************/

ERROR ActionThread(ACTIONID ActionID, OBJECTPTR Object, APTR Parameters, FUNCTION *Callback, LONG Key)
{
   parasol::Log log(__FUNCTION__);

   if ((!ActionID) or (!Object)) return ERR_NullArgs;

   log.traceBranch("Action: %d, Object: %d, Parameters: %p, Callback: %p, Key: %d", ActionID, Object->UniqueID, Parameters, Callback, Key);

   __sync_add_and_fetch(&Object->ThreadPending, 1);

   ERROR error;
   objThread *thread = NULL;
   if (!(error = threadpool_get(&thread))) {
      // Prepare the parameter buffer for passing to the thread routine.

      LONG argssize;
      BYTE call_data[sizeof(thread_data) + SIZE_ACTIONBUFFER];
      BYTE free_args = FALSE;
      const FunctionField *args = NULL;
      rkMetaClass *metaclass;

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
         else if ((metaclass = Object->Class)) {
            if ((-ActionID) < metaclass->TotalMethods) {
               args = metaclass->Methods[-ActionID].Args;
               if ((argssize = metaclass->Methods[-ActionID].Size) > 0) {
                  if (!(error = local_copy_args(args, argssize, (BYTE *)Parameters, call_data + sizeof(thread_data), SIZE_ACTIONBUFFER, &argssize, metaclass->Methods[-ActionID].Name))) {
                     free_args = TRUE;
                  }
               }
               else log.trace("Ignoring parameters provided for method %s", metaclass->Methods[-ActionID].Name);

               argssize += sizeof(thread_data);
            }
            else error = log.warning(ERR_IllegalMethodID);
         }
         else error = log.warning(ERR_MissingClass);
      }
      else argssize = sizeof(thread_data);

      // Execute the thread that will call the action.  Refer to thread_action() for the routine.

      if (!error) {
         SET_FUNCTION_STDC(thread->prv.Routine, (APTR)&thread_action);

         thread_data *call = (thread_data *)call_data;
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
         Action(MT_ThSetData, &thread->Head, &setdata);

         error = Action(AC_Activate, &thread->Head, NULL);
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

/*****************************************************************************

-FUNCTION-
CheckAction: Checks objects to see whether or not they support certain actions.

This function checks if an object supports a given action or method ID.  To check if a @Picture object supports the
Query action for instance, a code segment such as this would be sufficient:

<pre>
if (!CheckAction(pic, AC_Query)) {
   ...
}
</pre>

-INPUT-
obj Object: The object to query.
int Action: A registered action ID.

-ERRORS-
Okay: The object supports the specified action.
False: The action is not supported.
NullArgs:
LostClass: The object has lost its class reference (object corrupt).

*****************************************************************************/

ERROR CheckAction(OBJECTPTR Object, LONG ActionID)
{
   parasol::Log log(__FUNCTION__);

   if ((Object) and (ActionID)) {
      if (Object->ClassID IS ID_METACLASS) {
         if (((rkMetaClass *)Object)->ActionTable[ActionID].PerformAction) return ERR_Okay;
         else return ERR_False;
      }
      else if (Object->Class) {
         if (Object->Class->ActionTable[ActionID].PerformAction) return ERR_Okay;
         else if (Object->Class->Base) {
            if (Object->Class->Base->ActionTable[ActionID].PerformAction) return ERR_Okay;
         }
         return ERR_False;
      }
      else return log.warning(ERR_LostClass);
   }
   else return log.warning(ERR_NullArgs);
}

/*****************************************************************************

-FUNCTION-
GetActionMsg: Returns a message structure if called from an action that was executed by the message system.

This function is designed to be called from action and method support routines only.  It will return a Message
structure if the action currently under execution has been called directly from the ~ProcessMessages() function.
In all other cases a NULL pointer is returned.

The Message structure reflects the contents of a standard ~GetMessage() call.  Of particular interest may be
the Time field, which indicates the time-stamp at which the action message was originally sent to the object.

-RESULT-
resource(Message): A Message structure is returned if the function is called in valid circumstances, otherwise NULL.  The Message structure's fields are described in the ~GetMessage() function.

*****************************************************************************/

Message * GetActionMsg(void)
{
   if (tlContext->Object) {
      if (tlContext->Object->Flags & NF_MESSAGE) {
         if (tlContext->Object->ActionDepth IS 1) {
            return tlCurrentMsg;
         }
      }
   }
   return NULL;
}

/*****************************************************************************

-FUNCTION-
ManageAction: Allows modules to intercept and manage action calls.
Status: Private

Any module can manage an action of its choosing by calling this function with a pointer to a management routine.  To
remove management of an action, call this function with NULL in the Routine argument.

The routine that is used to manage the action must be in the format `ERROR Routine(OBJECTPTR, APTR Parameters)`.
An incorrectly defined routine could crash the system if it mis-reads the stack arguments.

This function is not intended to be called by executable based programs, as the 'area of effect' will be limited to
the program itself rather than having a global impact.

-INPUT-
int Action: The action that is to be managed.
ptr Routine:  Pointer to the routine that will manage the object.

-ERRORS-
Okay
Args
AllocMemory: The management array could not be expanded to accommodate the management routine (internal error).

*****************************************************************************/

ERROR ManageAction(LONG ActionID, APTR Routine)
{
   parasol::Log log(__FUNCTION__);

   log.branch("ActionID: %d, Routine: %p", ActionID, Routine);

   if (ActionID > 0) {
      // Check if the ActionID extends the amount of registered actions.  If so, reallocate the action management table.

      ThreadLock lock(TL_GENERIC, 50);
      if (lock.granted()) {
         if (ActionID > glActionCount) {
            glActionCount = ActionID;
            if (ManagedActions) {
               if (ReallocMemory((ULONG *)ManagedActions, sizeof(APTR) * glActionCount, (APTR *)&ManagedActions, NULL) != ERR_Okay) {
                  return log.warning(ERR_AllocMemory);
               }
            }
         }

         // Allocate the action management table if it has not been allocated already.  (NB: This address will be freed
         // in the expunge sequence).

         if (!ManagedActions) {
            if (AllocMemory(sizeof(APTR) * glActionCount, MEM_TASK, (APTR *)&ManagedActions, NULL) != ERR_Okay) {
               return log.warning(ERR_AllocMemory);
            }
         }
      }

      // Set the action management routine

      ManagedActions[ActionID] = (LONG (*)(OBJECTPTR, APTR))Routine;

      return ERR_Okay;
   }
   else return log.warning(ERR_Args);
}

/*****************************************************************************

-FUNCTION-
NotifySubscribers: Used to send notification messages to action subscribers.

This function can be used by classes that need total control over the timing of action subscriber notification.  The
system default for notifying action subscribers is to call them after an action has taken place.  However, this can
be inconvenient if the code for an action wants to perform some extra procedures after the subscribers have been
notified.  The solution is to use NotifySubscribers(), then continue on with the rest of the routine.

The Flags argument currently accepts the following options:

<types lookup="NSF"/>

-INPUT-
obj Object: Pointer to the object that is to receive the notification message.
int(AC) Action: The action ID for notification.
ptr Args: Pointer to the action arguments relevant to the ActionID.
int(NSF) Flags: Optional flags are specified here.
error Error: The error code that is associated with the action result.

-RESULT-
int: The total number of subscribers that were called is returned.
-END-

*****************************************************************************/

// No need for prv_access() since this function is called from within class action code only.

LONG NotifySubscribers(OBJECTPTR Object, LONG ActionID, APTR Parameters, LONG Flags, ERROR ErrorCode)
{
   parasol::Log log(__FUNCTION__);

   if (!Object) {
      log.warning(ERR_NullArgs);
      return 0;
   }

   if (!Object->Stats->ActionSubscriptions.Ptr) return 0;

   LONG result;
   if (ActionID >= 0) result = Object->Stats->NotifyFlags[ActionID>>5] & (1<<(ActionID & 31));
   else result = Object->Stats->MethodFlags[(-ActionID)>>5] & (1<<((-ActionID) & 31));

   if (!result) return 0;

   LONG recursionsave;

   recursionsave = tlMsgRecursion;
   tlMsgRecursion = 255; // This prevents ProcessMessages() from being used while inside notification routines

   WORD count = 0;
   ActionSubscription *list;
   if ((list = lock_subscribers(Object))) {
      struct acActionNotify notify = {
         .ActionID = ActionID,
         .ObjectID = Object->UniqueID,
         .Args     = Parameters,
         .Error    = ErrorCode
      };
      if (ActionID > 0) notify.Size = ActionTable[ActionID].Size;
      else notify.Size = result = Object->Stats->MethodFlags[(-ActionID)>>5] & (1<<((-ActionID) & 31));

      // Generate a shadow copy of the list - this is required in case calls to SubscribeAction() or
      // UnsubscribeAction() are made during the execution of the subscriber routines.

      ActionSubscription shadow[Object->Stats->SubscriptionSize];

      count = 0;
      for (LONG i=0; (i < Object->Stats->SubscriptionSize); i++) {
         if (list[i].ActionID IS ActionID) shadow[count++] = list[i];
         else if (!list[i].ActionID) break; // End of array
      }

      unlock_subscribers(Object);

      if (count) {
         for (LONG i=0; i < count; i++) {
            ERROR error = ERR_Okay;
            if (Flags & NSF_LOCAL_TASK) { // The LOCALTASK option means that only objects that are within the local task's address space will be sent the message.
               if ((shadow[i].MessagePortMID) and (glTaskMessageMID) and (shadow[i].MessagePortMID != glTaskMessageMID)) {
                  //log.warning("Subscriber %d skipped, belongs to port %d (we are %d).", shadow[i].SubscriberID, shadow[i].MessagePortMID, glTaskMessageMID);
                  continue;
               }
            }
            else if (Flags & NSF_OTHER_TASKS) {
               if (!((shadow[i].MessagePortMID) and (glTaskMessageMID) and (shadow[i].MessagePortMID != glTaskMessageMID))) {
                  continue;
               }
            }

            if (Flags & NSF_EXCLUSIVE) {
               if ((shadow[i].MessagePortMID) and (glTaskMessageMID) and (shadow[i].MessagePortMID != glTaskMessageMID)) {
                  // Use this routine for objects that belong to foreign tasks.  This involves sending a message to the
                  // other Task to perform the ActionNotify process, then we wait for it to respond before continuing.

                  error = ActionMsg(AC_ActionNotify, shadow[i].SubscriberID, &notify, shadow[i].MessagePortMID, -2);
               }
               else { // Use this routine for objects that belong to our task or are public
                  OBJECTPTR subscriber;
                  if (!(error = AccessObject(shadow[i].SubscriberID, 4000, &subscriber))) {
                     Action(AC_ActionNotify, subscriber, &notify);
                     ReleaseObject(subscriber);
                  }
               }
            }
            else if (Flags & NSF_FORCE_DELAY) error = ActionMsg(AC_ActionNotify, shadow[i].SubscriberID, &notify, shadow[i].MessagePortMID, -1);
            else error = ActionMsg(AC_ActionNotify, shadow[i].SubscriberID, &notify, shadow[i].MessagePortMID, shadow[i].ClassID);

            // If the message port or the object does not exist, remove the object from the list.

            if ((error IS ERR_MemoryDoesNotExist) or (error IS ERR_NoMatchingObject) or (error IS ERR_MarkedForDeletion)) {
               log.trace("Removing object #%d (port %d) from #%d's notification list (%s).", shadow[i].SubscriberID, shadow[i].MessagePortMID, Object->UniqueID, glMessages[error]);
               UnsubscribeActionByID(Object, 0, shadow[i].SubscriberID);
            }
         }
      }
      else { // If there are no subscribers for the indicated action ID, clear the associated bit flag for that action.
         if (ActionID > 0) Object->Stats->NotifyFlags[ActionID>>5] &= (~(1<<(ActionID & 31)));
         else Object->Stats->MethodFlags[(-ActionID)>>5] &= (~(1<<((-ActionID) & 31)));
      }
   }
   else log.warning(ERR_AccessMemory);

   tlMsgRecursion = recursionsave;

   return count;
}

/*****************************************************************************

-FUNCTION-
SubscribeAction: Listens for actions that may be performed on an object.

The SubscribeAction() function is provided for objects that need to be notified when actions are being executed on
foreign objects.  This is referred to as "action monitoring".  Action monitoring is used for a wide
variety of purposes and is especially useful for responding to events in the user interface, including pointer
movement, window resizing and graphics drawing.

To subscribe to the actions of another object, you need to acquire its address pointer, then call this function with
the action ID that you want to monitor.  The Subscriber argument needs to indicate what object you represent - if you
don't represent an object, you need to gain access to your local Task object and point to it in the Subscriber
argument.  The following example illustrates how to listen to a Surface object's Draw action, for the purposes of
drawing graphics to it:

<pre>
if (!AccessObject(SurfaceID, 3000, &surface)) {
   SubscribeAction(surface, AC_Draw, Self);
   ReleaseObject(surface);
}
</pre>

When a process calls a matching action on the object that is being listened to, its code will be executed first, then
all relevant action subscribers will be notified of the event.  This is done by sending each subscriber an action
message (AC_ActionNotify) with information on the action ID, the ID of the object that was called, and a copy of the
arguments that were used.  For more detail, refer to the technical support for #ActionNotify(). If you are
writing a standard executable rather than a class, refer to one the graphical example programs provided with the
Parasol SDK to see how ActionNotify events can be intercepted through the @Task object.

This function does not support subscriptions to methods.

When you need to terminate an action subscription, use the ~UnsubscribeAction() function.

-INPUT-
obj Object: Pointer to the object that you wish to subscribe to.
int Action: The ID of the action that you wish to monitor.

-ERRORS-
Okay:
Args:
AccessMemory: Access to the Object's subscription list was denied.
AllocMemory:  A subscription list could not be allocated for the object.

*****************************************************************************/

ERROR SubscribeAction(OBJECTPTR Object, ACTIONID ActionID)
{
   return SubscribeActionTags(Object, ActionID, TAGEND);
}

/*****************************************************************************

-FUNCTION-
SubscribeActionTags: Listens for actions that may be performed on an object.

This function is identical to SubscribeAction() in every aspect except that it is tag based.  The following example
illustrates typical usage `SubscribeActionTags(object, AC_Draw, AC_Redimension, TAGEND)`.

Two special tags are also available: SUB_WARN_EXISTS will result in an error of ERR_Exists being returned if it is
discovered that there is an existing match for the Subscriber and at least one of the actions.  SUB_FAIL_EXISTS
will return with ERR_Exists immediately if an action is already subscribed, meaning no further tags (if
present) will be processed.

-INPUT-
obj Object: The target object to subscribe to.
tags Actions: The ID of the action(s) that will be monitored.  The action list must be terminated with TAGEND.

-ERRORS-
Okay:
NullArgs:
AccessMemory: Access to the Object's subscription list was denied.
AllocMemory:  A subscription list could not be allocated for the object.
Exists:       A matching subscription already exists.

*****************************************************************************/

ERROR SubscribeActionTags(OBJECTPTR Object, ...)
{
   parasol::Log log(__FUNCTION__);

   if (!Object) return log.warning(ERR_NullArgs);

   if (Object IS &glMetaClass.Head) return ERR_NoSupport; // Monitoring the MetaClass is prohibited.

   prv_access(Object);

   LONG memflags;
   if (Object->Flags & NF_PUBLIC) memflags = Object->MemFlags|MEM_PUBLIC;
   else memflags = Object->MemFlags;

   LONG count = 0;  // Count the total number of actions to add
   va_list vlist;
   va_start(vlist, Object);
   while (va_arg(vlist, ACTIONID)) count++;
   va_end(vlist);

   ERROR error;
   if (!Object->Stats->ActionSubscriptions.Ptr) { // Allocate the subscription array if it does not exist
      OBJECTPTR context = SetContext(Object);
      if (count < 10) count = 10; // Allocate at least 10 slots

      if (Object->UniqueID < 0) {
         error = AllocMemory(sizeof(ActionSubscription) * count, memflags, NULL, &Object->Stats->ActionSubscriptions.ID);
      }
      else error = AllocMemory(sizeof(ActionSubscription) * count, memflags, &Object->Stats->ActionSubscriptions.Ptr, NULL);

      if (error) {
         SetContext(context);
         prv_release(Object);
         return log.warning(ERR_AllocMemory);
      }

      Object->Stats->SubscriptionSize = count;
      SetContext(context);
   }


   LONG i, j;
   ERROR warnerror = ERR_Okay;
   LONG warntag = 0;
   ActionSubscription *list;
   if ((list = lock_subscribers(Object))) {
      va_start(vlist, Object);
      ACTIONID actionid;
      while ((actionid = va_arg(vlist, ACTIONID))) {
         if (actionid IS SUB_WARN_EXISTS) {
            warntag = SUB_WARN_EXISTS;
            continue;
         }
         else if (actionid IS SUB_FAIL_EXISTS) {
            warntag = SUB_FAIL_EXISTS;
            continue;
         }

         // Scan the subscription array.  If the subscription already exists (the action ID and subscriber ID are
         // identical), we move it to the bottom of the list.

         for (i=0; (i < Object->Stats->SubscriptionSize) and (list[i].ActionID); i++) {
            if ((list[i].ActionID IS actionid) and (list[i].SubscriberID IS tlContext->Object->UniqueID)) {
               if (warntag IS SUB_WARN_EXISTS) {
                  // Return ERR_Exists to warn the caller that there is already a subscription against this action.
                  // We will not shift the subscription to the front of the queue.
                  warnerror = ERR_Exists;
                  continue;
               }
               else if (warntag IS SUB_FAIL_EXISTS) { // Return ERR_Exists immediately
                  unlock_subscribers(Object);
                  prv_release(Object);
                  return ERR_Exists;
               }

               // Check if there are other actions in front of this location
               if ((i < Object->Stats->SubscriptionSize-1) and (list[i+1].ActionID)) {
                  for (j=i; (j < Object->Stats->SubscriptionSize-1) and (list[j].ActionID); j++);
                  // Shift the actions down a notch
                  CopyMemory(list+i+1, list+i, sizeof(ActionSubscription) * (Object->Stats->SubscriptionSize - i));
                  i = j; // Set the insertion point to the end of the list
               }
               break;
            }
         }

         if (i >= Object->Stats->SubscriptionSize) { // Enlarge the size of the object's subscription list
            log.debug("Extending array for object %d to %d entries.", Object->UniqueID, Object->Stats->SubscriptionSize);
            if (glLogLevel >= 5) { // Report any dead subscribers if we're in log mode
               for (i=0; (i < Object->Stats->SubscriptionSize) and (list[i].ActionID); i++) {
                  if (CheckObjectExists(list[i].SubscriberID, 0) != ERR_True) {
                     log.warning("Dead subscriber @ index %d, Action %d, Object %d - Fix your code!", i, list[i].ActionID, list[i].SubscriberID);
                  }
               }
            }

            ActionSubscription *newlist;

            {
               parasol::SwitchContext context(Object);

               MEMORYID newlistid;
               if (Object->UniqueID < 0) error = AllocMemory(sizeof(ActionSubscription)*(Object->Stats->SubscriptionSize+10), memflags, (APTR *)&newlist, &newlistid);
               else error = AllocMemory(sizeof(ActionSubscription)*(Object->Stats->SubscriptionSize+10), memflags, (APTR *)&newlist, NULL);

               if (!error) {
                  CopyMemory(list, newlist, Object->Stats->SubscriptionSize * sizeof(ActionSubscription));

                  unlock_subscribers(Object);

                  if (Object->UniqueID < 0) {
                     FreeResourceID(Object->Stats->ActionSubscriptions.ID);
                     Object->Stats->ActionSubscriptions.ID = newlistid;
                  }
                  else {
                     FreeResource(Object->Stats->ActionSubscriptions.Ptr);
                     Object->Stats->ActionSubscriptions.Ptr = newlist;
                  }

                  Object->Stats->SubscriptionSize += 10;
                  list = newlist;
               }
            }

            if (error) {
               unlock_subscribers(Object);
               va_end(vlist);
               prv_release(Object);
               return log.warning(ERR_AllocMemory);
            }
         }

         list[i].ActionID       = actionid;
         list[i].SubscriberID   = tlContext->Object->UniqueID;
         list[i].ClassID        = tlContext->Object->ClassID;
         list[i].MessagePortMID = glTaskMessageMID;

         if (actionid > 0) {
            i = actionid>>5;
            if ((i >= 0) and (i < ARRAYSIZE(Object->Stats->NotifyFlags))) {
               Object->Stats->NotifyFlags[i] |= 1<<(actionid & 31);
            }
         }
         else {
            i = (-actionid)>>5;
            if ((i >= 0) and (i < ARRAYSIZE(Object->Stats->MethodFlags))) {
               Object->Stats->MethodFlags[i] |= 1<<((-actionid) & 31);
            }
         }
      }

      unlock_subscribers(Object);
   }
   else {
      va_end(vlist);
      prv_release(Object);
      return log.warning(ERR_AccessMemory);
   }

   va_end(vlist);
   prv_release(Object);
   return warnerror;
}

/*****************************************************************************

-FUNCTION-
UnsubscribeAction: Removes action subscriptions from external objects.

If you have subscribed to the action/s of a foreign object using the ~SubscribeAction() function, you can
terminate the subscription by calling the UnsubscribeAction() function. Simply provide the ID of the action that you
want to unsubscribe from as well as the unique ID of the subscriber that you used in the original subscription call.

If you have more than one subscription and you want to terminate them all, set the Action argument to NULL.

-INPUT-
obj Object: The object that you are unsubscribing from.
int(AC) Action: The ID of the action that will be unsubscribed, or NULL for all actions.

-ERRORS-
Okay: The termination of service was successful.
NullArgs:
AccessMemory: Access to the internal subscription array was denied.
-END-

*****************************************************************************/

ERROR UnsubscribeAction(OBJECTPTR Object, ACTIONID ActionID)
{
   return UnsubscribeActionByID(Object, ActionID, tlContext->Object->UniqueID);
}

ERROR UnsubscribeActionByID(OBJECTPTR Object, ACTIONID ActionID, OBJECTID SubscriberID)
{
   parasol::Log log(__FUNCTION__);

   if (!Object) return log.warning(ERR_NullArgs);

   prv_access(Object);

   //log.msg("UnsubscribeAction(%d, Subscriber %d, Action %d)", Object->UniqueID, ActionID);

   ActionSubscription *list;
   if ((list = lock_subscribers(Object))) {
      // Clear matching array entries and perform compaction of the array so that there are no gaps.

      LONG j = 0;
      for (LONG i=0; i < Object->Stats->SubscriptionSize; i++) {
         if ((list[i].SubscriberID IS SubscriberID) and ((!ActionID) or (ActionID IS list[i].ActionID))) {
            list[i].ActionID = 0;
         }
         else if (list[i].ActionID) { // Compact the array by moving the current entry back a few places, then clear the reference.
            if (i > j) {
               list[j] = list[i];
               list[i].ActionID = 0;
            }
            j++;
         }
         else break; // If no ActionID present then the end of the list has been reached
      }

      // If there is nothing left in this list, destroy it

      if (!list[0].ActionID) {
         //log.msg("Object %d list empty - removing list.", Object->UniqueID);
         unlock_subscribers(Object);

         parasol::SwitchContext context(Object);

         if (Object->UniqueID < 0) {
            FreeResourceID(Object->Stats->ActionSubscriptions.ID);
            Object->Stats->ActionSubscriptions.ID = 0;
         }
         else {
            FreeResource(Object->Stats->ActionSubscriptions.Ptr);
            Object->Stats->ActionSubscriptions.Ptr = NULL;
         }

         Object->Stats->SubscriptionSize  = 0;
         for (WORD i=0; i < ARRAYSIZE(Object->Stats->NotifyFlags); i++) Object->Stats->NotifyFlags[i] = 0;
         for (WORD i=0; i < ARRAYSIZE(Object->Stats->MethodFlags); i++) Object->Stats->MethodFlags[i] = 0;
      }
      else unlock_subscribers(Object);
   }

   prv_release(Object);
   return ERR_Okay;
}

/*****************************************************************************
** Action: Free()
*/

ERROR MGR_Free(OBJECTPTR Object, APTR Void)
{
   parasol::Log log("Free");
   rkMetaClass *mc;

   Object->ActionDepth--; // See Action() regarding this

   if (!(mc = Object->Class)) {
      log.trace("Object %p #%d is missing its class pointer.", Object, Object->UniqueID);
      return log.warning(ERR_ObjectCorrupt)|ERF_Notified;
   }

   // Check to see if the object is currently locked from AccessObject().  If it is, we mark it for deletion so that we
   // can safely get rid of it during ReleaseObject().

   if ((Object->Locked) or (Object->ThreadPending)) {
      log.debug("Object #%d locked; marking for deletion.", Object->UniqueID);
      set_object_flags(Object, Object->Flags|NF_UNLOCK_FREE);
      return ERR_Okay|ERF_Notified;
   }

   // Return if the object is currently in the process of being freed (i.e. avoid recursion)

   if (Object->Flags & NF_FREE) {
      log.trace("Object already marked with NF_FREE.");
      return ERR_Okay|ERF_Notified;
   }

   if (Object->ActionDepth > 0) { // Free() is being called while the object itself is still in use.  This can be an issue with private objects that haven't been locked with AccessObject().
      log.trace("Free() attempt while object is in use.");
      if (!(Object->Flags & NF_FREE_MARK)) {
         set_object_flags(Object, Object->Flags|NF_FREE_MARK);
         ActionMsg(AC_Free, Object->UniqueID, NULL, 0, -1);
      }
      return ERR_Okay|ERF_Notified;
   }

   if (Object->ClassID IS ID_METACLASS)   log.branch("%s, Owner: %d", ((rkMetaClass *)Object)->ClassName, Object->OwnerID);
   else if (Object->ClassID IS ID_MODULE) log.branch("%s, Owner: %d", ((rkModule *)Object)->Name, Object->OwnerID);
   else if (Object->Stats->Name[0])       log.branch("Name: %s, Owner: %d", Object->Stats->Name, Object->OwnerID);
   else log.branch("Owner: %d", Object->OwnerID);

   // If the object wants to be warned when the free process is about to be executed, it will subscribe to the
   // FreeWarning action.  The process can be aborted by returning ERR_InUse.

   if (mc->ActionTable[AC_FreeWarning].PerformAction) {
      if (mc->ActionTable[AC_FreeWarning].PerformAction(Object, NULL) IS ERR_InUse) {
         if ((Object->Flags & (NF_FREE_MARK|NF_FREE))) {
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
            if ((Object->Flags & (NF_FREE_MARK|NF_FREE))) {
               // If the object is marked for deletion then it is not possible to avoid destruction (this prevents
               // objects from locking up the shutdown process).
               log.msg("Object will be destroyed despite being in use.");
            }
            else return ERR_InUse|ERF_Notified;
         }
      }
   }

   // Mark the object as being in the free process.  The mark prevents any further access to the object via
   // AccessObject().  Classes may also use the flag to check if an object is in the process of being freed.

   set_object_flags(Object, (Object->Flags|NF_FREE) & (~NF_UNLOCK_FREE));

   NotifySubscribers(Object, AC_Free, NULL, 0, ERR_Okay);

   // AC_OwnerDestroyed is internal, it notifies objects in foreign tasks that are resource-linked to the object.
   // Refer to SetOwner() for more info.

   NotifySubscribers(Object, AC_OwnerDestroyed, NULL, 0, ERR_Okay);

   if (mc->ActionTable[AC_Free].PerformAction) {  // If the class that formed the object is a sub-class, we call its Free() support first, and then the base-class to clean up.
      mc->ActionTable[AC_Free].PerformAction(Object, NULL);
   }

   if (mc->Base) { // Sub-class detected, so call the base class
      if (mc->Base->ActionTable[AC_Free].PerformAction) {
         mc->Base->ActionTable[AC_Free].PerformAction(Object, NULL);
      }
   }

   if (Object->Stats->MID_FeedList) { FreeResourceID(Object->Stats->MID_FeedList); Object->Stats->MID_FeedList = 0; }

   if (Object->Stats->ActionSubscriptions.ID) { // Close the action subscription list
      if (Object->UniqueID < 0) FreeResourceID(Object->Stats->ActionSubscriptions.ID);
      else if (FreeResource(Object->Stats->ActionSubscriptions.Ptr)) log.warning("Invalid ActionSubscriptions address %p.", Object->Stats->ActionSubscriptions.Ptr);

      Object->Stats->ActionSubscriptions.Ptr = NULL;
   }

   // If a private child structure is present, remove it

   if (Object->ChildPrivate) {
      if (FreeResource(Object->ChildPrivate)) log.warning("Invalid ChildPrivate address %p.", Object->ChildPrivate);
      Object->ChildPrivate = NULL;
   }

   free_private_children(Object);
   free_public_children(Object);

   if (Object->Flags & NF_TIMER_SUB) {
      ThreadLock lock(TL_TIMER, 200);
      if (lock.granted()) {
         CoreTimer *timer, *next;
         for (timer=glTimers; timer; timer=next) {
            next = timer->Next;
            if (timer->SubscriberID IS Object->UniqueID) {
               log.warning("%s object #%d has an unfreed timer subscription.", mc->ClassName, Object->UniqueID);

               if (timer->Next) timer->Next->Prev = timer->Prev;
               if (timer->Prev) timer->Prev->Next = timer->Next;
               if (glTimers IS timer) glTimers = timer->Next;
               free(timer);
            }
         }
      }
   }

   if (Object->Flags & NF_PUBLIC) remove_shared_object(Object->UniqueID);  // If the object is shared, remove it from the shared list

   if (!(Object->Flags & NF_PUBLIC)) { // Decrement the counters associated with the class that this object belongs to.
      if ((mc->Base) and (mc->Base->OpenCount > 0)) mc->Base->OpenCount--; // Child detected
      if (mc->OpenCount > 0) mc->OpenCount--;
   }

   if ((glObjectLookup) and (Object->Stats->Name[0])) { // Remove the object from the name lookup list
      ThreadLock lock(TL_OBJECT_LOOKUP, 4000);
      if (lock.granted()) remove_object_hash(Object);
   }

   if (Object->UniqueID < 0) {
      FreeResourceID(Object->UniqueID);
   }
   else {
      // Clear the object header.  This helps to raise problems in any areas of code that may attempt to use the object
      // after it has been destroyed.
      ClearMemory(Object, sizeof(Head));
      FreeResource(Object);
   }

   return ERR_Okay|ERF_Notified;  // On returning we set the ERF_Notified flag to prevent Action() from trying to interact with the Object->Stats structure (which no longer exists after the object memory is freed).
}

/*****************************************************************************
** Action: Init()
*/

ERROR MGR_Init(OBJECTPTR Object, APTR Void)
{
   parasol::Log log("Init");

   if (!Object->Stats) return log.warning(ERR_NoStats);

   rkMetaClass *baseclass;
   if (!(baseclass = Object->Class)) return log.warning(ERR_LostClass);

   if (Object->ClassID != baseclass->BaseClassID) {
      log.warning("Cannot initialise object #%d - the Object.ClassID ($%.8x) does not match the Class.BaseClassID ($%.8x)", Object->UniqueID, Object->ClassID, baseclass->BaseClassID);
      return ERR_ObjectCorrupt;
   }

   if (Object->Flags & NF_INITIALISED) {  // Initialising twice does not cause an error, but send a warning and return
      log.warning(ERR_DoubleInit);
      return ERR_Okay;
   }

   if (Object->Stats->Name[0]) log.branch("Name: %s, Owner: %d", Object->Stats->Name, Object->OwnerID);
   else log.branch("Owner: %d", Object->OwnerID);

   BYTE use_subclass = FALSE;
   ERROR error = ERR_Okay;
   if (Object->SubID) {
      // For sub-classes, the base-class gets called first.  It should check the SubID in the header to determine that
      // the object is sub-classed so as to prevent it from doing 'too much' initialisation.

      if (Object->Class->Base->ActionTable[AC_Init].PerformAction) {
         error = Object->Class->Base->ActionTable[AC_Init].PerformAction(Object, NULL);
      }

      if (!error) {
         if (Object->Class->ActionTable[AC_Init].PerformAction) {
            error = Object->Class->ActionTable[AC_Init].PerformAction(Object, NULL);
         }

         if (!error) set_object_flags(Object, Object->Flags|NF_INITIALISED);
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

      rkMetaClass * sublist[16];
      LONG sli = -1;

      while (Object->Class) {
         if (Object->Class->ActionTable[AC_Init].PerformAction) {
            error = Object->Class->ActionTable[AC_Init].PerformAction(Object, NULL);
            if (!error) {
               set_object_flags(Object, Object->Flags|NF_INITIALISED);

               if (Object->Class != baseclass) {
                  // Due to the switch, increase the open count of the sub-class (see NewObject() for details on object
                  // reference counting).

                  log.msg("Object class switched to sub-class \"%s\".", Object->Class->ClassName);

                  if (!(Object->Flags & NF_PUBLIC)) Object->Class->OpenCount++;

                  Object->SubID = Object->Class->SubClassID;
                  Object->Flags |= NF_RECLASSED; // This flag indicates that the object originally belonged to the base-class
               }

               return ERR_Okay;
            }

            if (error IS ERR_UseSubClass) {
               log.trace("Requested to use registered sub-class.");
               use_subclass = TRUE;
            }
            else if (error != ERR_NoSupport) break;
         }
         else return ERR_Okay;

         if (sli IS -1) {
            // Initialise a list of all sub-classes already in memory for querying in sequence.
            sli = 0;
            LONG i = 0;
            rkMetaClass **ptr;
            CSTRING key = NULL;
            while ((i < ARRAYSIZE(sublist)-1) and (!VarIterate(glClassMap, key, &key, (APTR *)&ptr, NULL))) {
               rkMetaClass *mc = ptr[0];
               if ((Object->ClassID IS mc->BaseClassID) and (mc->BaseClassID != mc->SubClassID)) {
                  sublist[i++] = mc;
               }
            }
            sublist[i++] = NULL;
         }

         // Attempt to initialise with the next known sub-class.

         if ((Object->Class = sublist[sli++])) {
            log.trace("Attempting initialisation with sub-class '%s'", Object->Class->ClassName);
            Object->SubID = Object->Class->SubClassID;
         }
      }
   }

   Object->Class = baseclass;  // Put back the original to retain integrity
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
      if (!IdentifyFile(path, NULL, 0, &classid, &Object->SubID, NULL)) {
         if ((classid IS Object->ClassID) and (Object->SubID)) {
            log.msg("Searching for subclass $%.8x", Object->SubID);
            if ((Object->Class = FindClass(Object->SubID))) {
               if (Object->Class->ActionTable[AC_Init].PerformAction) {
                  if (!(error = Object->Class->ActionTable[AC_Init].PerformAction(Object, NULL))) {
                     log.msg("Object class switched to sub-class \"%s\".", Object->Class->ClassName);
                     set_object_flags(Object, Object->Flags|NF_INITIALISED);

                     if (!(Object->Flags & NF_PUBLIC)) { // Increase the open count of the sub-class
                        Object->Class->OpenCount++;
                     }
                     return ERR_Okay;
                  }
               }
               else return ERR_Okay;
            }
            else log.warning("Failed to load module for class #%d.", Object->SubID);
         }
      }
      else log.warning("File '%s' does not belong to class '%s', got $%.8x.", path, Object->Class->ClassName, classid);

      Object->Class = baseclass;  // Put back the original to retain object integrity
      Object->SubID = baseclass->SubClassID;
   }

   return error;
}

/*****************************************************************************
** Action: OwnerDestroyed
*/

ERROR MGR_OwnerDestroyed(OBJECTPTR Object, APTR Void)
{
   parasol::Log log("OwnerDestroyed");
   log.function("Owner %d has been destroyed.", Object->UniqueID);
   acFree(Object);
   return ERR_Okay;
}

/*****************************************************************************
** Action: Rename
*/

ERROR MGR_Rename(OBJECTPTR Object, struct acRename *Args)
{
   parasol::Log log("Rename");

   if (!Args) return log.warning(ERR_NullArgs);

   if (Object->Class) {
      if (Object->Class->ActionTable[AC_Rename].PerformAction) {
         return Object->Class->ActionTable[AC_Rename].PerformAction(Object, Args);
      }
      else return SetField(Object, FID_FileName|TPTR, Args->Name);
   }
   else return log.warning(ERR_LostClass);
}

/*****************************************************************************
** Action: Seek
*/

ERROR MGR_Seek(OBJECTPTR Object, struct acSeek *Args)
{
   parasol::Log log("Seek");

   if (!Args) return log.warning(ERR_NullArgs);

   ERROR error;
   if (Object->Class) {
      if (Object->Class->ActionTable[AC_Seek].PerformAction) {
         // Check and correct the Pos and Mode if SEEK_END

         if ((Args->Position IS SEEK_END) and (Args->Offset < 0)) Args->Offset = -Args->Offset;
         else if ((Args->Position IS SEEK_START) and (Args->Position < 0)) return log.warning(ERR_Args);
         return Object->Class->ActionTable[AC_Seek].PerformAction(Object, Args);
      }
      else error = ERR_NoSupport;
   }
   else error = ERR_LostClass;

   return log.warning(error);
}

/*****************************************************************************
** Action: Signal
*/

ERROR MGR_Signal(OBJECTPTR Object, APTR Void)
{
   Object->Flags |= NF_SIGNALLED;
   return ERR_Okay;
}
