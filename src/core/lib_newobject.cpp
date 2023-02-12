/*****************************************************************************
-CATEGORY-
Name: Objects
-END-
*****************************************************************************/

#include "defs.h"
#include <parasol/main.h>

extern "C" ERROR CLASS_Free(extMetaClass *, APTR);
extern "C" ERROR CLASS_Init(extMetaClass *, APTR);

/*****************************************************************************

-FUNCTION-
NewObject: Creates new objects.

The NewObject() function is used to create new objects and register them for use within the Core.  After creating
a new object, the client can proceed to set the object's field values and initialise it with #Init() so that it
can be used as intended.

The new object will be modeled according to the class blueprint indicated by ClassID.  Pre-defined class ID's are
defined in the `parasol/system/register.h` include file and a complete list of known classes is available in the Class
Index Guide.  ID's for unregistered classes can be found dynamically by using the ~ResolveClassName() function.

A pointer to the new object will be returned in the Object parameter.  By default, new objects are always owned by the
object that holds the current context.  It is possible to track a new object to a different owner by using the
~SetOwner() function after calling NewObject().

To destroy an object, use the #Free() action.

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

*****************************************************************************/

ERROR NewObject(LARGE ClassID, NF Flags, OBJECTPTR *Object)
{
   parasol::Log log(__FUNCTION__);
   static BYTE master_sorted = FALSE;

   ULONG class_id = (ULONG)(ClassID & 0xffffffff);
   if ((!class_id) or (!Object)) return log.warning(ERR_NullArgs);

   extMetaClass *mc;
   if (class_id IS ID_METACLASS) {
      mc = &glMetaClass;
      glMetaClass.ActionTable[AC_Free].PerformAction = (ERROR (*)(OBJECTPTR, APTR))CLASS_Free;
      glMetaClass.ActionTable[AC_Init].PerformAction = (ERROR (*)(OBJECTPTR, APTR))CLASS_Init;
      // Initialise the glMetaClass fields if this has not already been done.

      if (!master_sorted) {
         sort_class_fields(&glMetaClass, glMetaClass.prvFields);
         master_sorted = TRUE;
      }
   }
   else if (!(mc = (extMetaClass *)FindClass(class_id))) {
      extMetaClass **ptr;
      if (!KeyGet(glClassMap, ClassID, (APTR *)&ptr, NULL)) {
         log.function("Class %s was not found in the system.", ptr[0]->ClassName);
      }
      else log.function("Class $%.8x was not found in the system.", class_id);
      return ERR_MissingClass;
   }

   if (Object) *Object = NULL;

   Flags = Flags & (NF::UNTRACKED|NF::INTEGRAL|NF::UNIQUE|NF::NAME|NF::SUPPRESS_LOG); // Very important to eliminate any internal flags.

   // If the object is integral then turn off use of the UNTRACKED flag (otherwise the child will
   // end up being tracked to its task rather than its parent object).

   if ((Flags & NF::INTEGRAL) != NF::NIL) Flags = Flags & (~NF::UNTRACKED);

   // Force certain flags on the class' behalf

   if (mc->Flags & CLF_NO_OWNERSHIP)   Flags |= NF::UNTRACKED;

   if ((Flags & NF::SUPPRESS_LOG) IS NF::NIL) log.branch("%s #%d, Flags: $%x", mc->ClassName, glSharedControl->PrivateIDCounter, LONG(Flags));

   OBJECTPTR head = NULL;
   MEMORYID head_id = 0;
   ERROR error = ERR_Okay;

   if (!AllocMemory(mc->Size + sizeof(Stats), MEM_OBJECT|MEM_NO_LOCK|(((Flags & NF::UNTRACKED) != NF::NIL) ? MEM_UNTRACKED : 0), (APTR *)&head, &head_id)) {
      head->Stats     = (Stats *)ResolveAddress(head, mc->Size);
      head->UID       = head_id;
      head->ClassID   = mc->BaseClassID;
      if (mc->BaseClassID IS mc->SubClassID) { // Object derived from a base class
         head->SubID = 0;
      }
      else { // Object derived from a sub-class
         head->SubID = mc->SubClassID;
      }

      head->Class = (extMetaClass *)mc;

      if ((Flags & NF::UNTRACKED) IS NF::NIL) { // Don't track untracked objects to specific threads.
         head->ThreadMsg = tlThreadWriteMsg; // If the object needs to belong to a thread, this will record it.
      }

      // Note that the untracked flag is turned off because if it is left on, objects will allocate their children as
      // being untracked (due to use of ObjectFlags as a reference for child allocation).   We don't want our tracking to
      // be screwed up do we...

      set_object_flags(head, Flags);
      head->Flags |= NF::NEW_OBJECT;

      // Tracking for our new object is configured here.

      OBJECTPTR task;
      if (mc->Flags & CLF_NO_OWNERSHIP) {
      }
      else if ((Flags & NF::UNTRACKED) != NF::NIL) {
         if (class_id IS ID_MODULE); // Untracked modules have no owner, due to the expunge process.
         else {
            // If the object is private and untracked, set its owner to the current task.  This will ensure that the object
            // is deallocated correctly when the Core is closed.

            if (glCurrentTaskID) {
               if (!AccessObject(glCurrentTaskID, 5000, &task)) {
                  SetOwner(head, task);
                  ReleaseObject(task);
               }
               else log.msg("Unable to access local task (ID: %d).", glCurrentTaskID);
            }
         }
      }
      else if (tlContext != &glTopContext) { // Track the object to the current context
         SetOwner(head, tlContext->resource());
      }
      else if (glCurrentTaskID) { // If no current context is available then track the object to the local task
         if (!AccessObject(glCurrentTaskID, 3000, &task)) {
            SetOwner(head, task);
            ReleaseObject(task);
         }
         else log.msg("Unable to access the local task #%d", glCurrentTaskID);
      }
      else if (SystemTaskID) { // If no current task is available then track the object to the system task
         if (!AccessObject(SystemTaskID, 3000, &task)) {
            SetOwner(head, task);
            ReleaseObject(task);
         }
         else log.msg("Unable to access the system task #%d", SystemTaskID);
      }

      // After the header has been created we can set the context, then call the base class's NewObject() support.  If the
      // class is a child, we will also call its supporting NewObject() action if it has specified one.
      //
      // Note: The NewObject support caller has a special feature where it passes the expected object context in the args pointer.

      if (!error) {
         parasol::SwitchContext context(head); // Scope must be limited to the PerformAction() call

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
      }

      if (!error) {
         ((extMetaClass *)head->Class)->OpenCount++;
         if (mc->Base) mc->Base->OpenCount++;

         head->Flags = head->Flags & (~NF::NEW_OBJECT);
         *Object = head;
         return ERR_Okay;
      }
   }
   else error = ERR_AllocMemory;

   if (head) {
      head->Flags = head->Flags & (~NF::NEW_OBJECT);
      FreeResource(head);
   }

   return error;
}

/*****************************************************************************

-FUNCTION-
NewLockedObject: Creates a new object with a lock immediately applied.

The NewLockedObject() function extends ~NewObject() with the addition of applying a lock to the new object at the time
of its allocation.  Its use is also required in cases where the client needs to allocate a public object that can be
shared between processes.

References to the created object are available as a pointer and unique object ID.  Once the lock is no longer required,
a call to ~ReleaseObject() is necessary to release it.  If the lock is not released, any attempt to free the
object will fail and this can eventually lead to program closure problems.  Problems such as this can be debugged
quickly by checking the application log for unreleased lock errors.

-INPUT-
large ClassID: A class ID from "system/register.h" or generated by ~ResolveClassName().
flags(NF) Flags: Optional flags.
&obj Object: Optional.  Pointer to an address variable that will store a reference to the new object.  If set to NULL, the acquired lock is released before returning to the client.
&oid ID: Compulsory.  The unique ID of the new object will be returned in this parameter.
cstr Name: Enabled only if NF_NAME or NF_UNIQUE are specified as Flags.  The Name refers to the name that you want to give the object.

-ERRORS-
Okay
NullArgs
MissingClass: The ClassID is invalid or refers to a class that is not installed.
Failed
ObjectExists: An object with the provided Name already exists in the system (applies only when the NF_UNIQUE flag has been used).
-END-

*****************************************************************************/

ERROR NewLockedObject(LARGE ClassID, NF Flags, OBJECTPTR *Object, OBJECTID *ObjectID, CSTRING Name)
{
   parasol::Log log(__FUNCTION__);

   static BYTE master_sorted = FALSE;
   static BYTE private_lock = FALSE;

   ULONG class_id = (ULONG)(ClassID & 0xffffffff);
   if ((!class_id) or (!ObjectID)) return log.warning(ERR_NullArgs);

   extMetaClass *mc;
   if (class_id IS ID_METACLASS) {
      mc = &glMetaClass;
      glMetaClass.ActionTable[AC_Free].PerformAction = (ERROR (*)(OBJECTPTR, APTR))CLASS_Free;
      glMetaClass.ActionTable[AC_Init].PerformAction = (ERROR (*)(OBJECTPTR, APTR))CLASS_Init;
      // Initialise the glMetaClass fields if this has not already been done.

      if (!master_sorted) {
         sort_class_fields(&glMetaClass, glMetaClass.prvFields);
         master_sorted = TRUE;
      }
   }
   else if (!(mc = (extMetaClass *)FindClass(class_id))) {
      extMetaClass **ptr;
      if (!KeyGet(glClassMap, ClassID, (APTR *)&ptr, NULL)) {
         log.function("Class %s was not found in the system.", ptr[0]->ClassName);
      }
      else log.function("Class $%.8x was not found in the system.", class_id);
      return ERR_MissingClass;
   }

   if (Object) *Object = NULL;
   *ObjectID = 0;

   Flags = Flags & (NF::UNTRACKED|NF::INTEGRAL|NF::UNIQUE|NF::NAME|NF::SUPPRESS_LOG); // Very important to eliminate any internal flags.

   // If the object is to be a child of a larger object, turn off use of the UNTRACKED flag (otherwise the child will
   // end up being tracked to its task rather than its parent object).

   if ((Flags & NF::INTEGRAL) != NF::NIL) Flags = Flags & (~NF::UNTRACKED);

   // Force certain flags on the class' behalf

   if (mc->Flags & CLF_NO_OWNERSHIP)   Flags |= NF::UNTRACKED;

   log.branch("%s #%d, Flags: $%x", mc->ClassName, glSharedControl->PrivateIDCounter, LONG(Flags));

   OBJECTPTR head   = NULL;
   MEMORYID head_id = 0;
   ERROR error      = ERR_Okay;

   if (((Flags & NF::UNIQUE) != NF::NIL) and (Name)) {
      if ((Flags & NF::UNIQUE) != NF::NIL) {
         OBJECTID search_id;
         LONG count = 1;
         if ((!FindObject(Name, class_id, FOF_INCLUDE_SHARED|FOF_SMART_NAMES, &search_id, &count)) and (search_id)) {
            *ObjectID = search_id;
            ReleaseMemoryID(RPM_SharedObjects);
            return ERR_ObjectExists; // Must be ERR_ObjectExists for client benefit.
         }
      }
   }

   if (!AllocMemory(mc->Size + sizeof(Stats), MEM_OBJECT|MEM_NO_LOCK|(((Flags & NF::UNTRACKED) != NF::NIL) ? MEM_UNTRACKED : 0), (APTR *)&head, &head_id)) {
      head->Stats = (Stats *)ResolveAddress(head, mc->Size);
      head->UID = head_id;
   }
   else error = ERR_AllocMemory;

   if (!error) {
      head->ClassID = mc->BaseClassID;
      if (mc->BaseClassID IS mc->SubClassID) { // Object derived from a base class
         head->SubID = 0;
      }
      else head->SubID = mc->SubClassID; // Object derived from a sub-class

      head->Class = mc;

      if ((Flags & NF::UNTRACKED) IS NF::NIL) {
         head->ThreadMsg = tlThreadWriteMsg; // If the object needs to belong to a thread, this will record it.
      }
   }

   if (!error) {
      // Note that the untracked flag is turned off because if it is left on, objects will allocate their children as
      // being untracked (due to use of ObjectFlags as a reference for child allocation).

      set_object_flags(head, Flags);
      head->Flags = head->Flags | NF::NEW_OBJECT;

      // Tracking for our newly created object is configured here.

      OBJECTPTR task;
      if (mc->Flags & CLF_NO_OWNERSHIP); // The class mandates that objects have no owner.
      else if ((Flags & NF::UNTRACKED) != NF::NIL) {
         if (class_id IS ID_MODULE); // Untracked modules have no owner, due to the expunge process.
         else {
            // Set the owner to the current task.  This will ensure that the object
            // is deallocated correctly when the Core is closed.

            if (glCurrentTaskID) {
               if (!AccessObject(glCurrentTaskID, 5000, &task)) {
                  SetOwner(head, task);
                  ReleaseObject(task);
               }
               else log.msg("Unable to access local task (ID: %d).", glCurrentTaskID);
            }
         }
      }
      else if (tlContext != &glTopContext) { // Track the object to the current context
         SetOwner(head, tlContext->resource());
      }
      else if (glCurrentTaskID) { // If no current context is available then track the object to the local task
         if (!AccessObject(glCurrentTaskID, 3000, &task)) {
            SetOwner(head, task);
            ReleaseObject(task);
         }
         else log.msg("Unable to access the local task #%d", glCurrentTaskID);
      }
      else if (SystemTaskID) { // If no current task is available then track the object to the system task
         if (!AccessObject(SystemTaskID, 3000, &task)) {
            SetOwner(head, task);
            ReleaseObject(task);
         }
         else log.msg("Unable to access the system task #%d", SystemTaskID);
      }
   }

   if ((!error) and (Object)) {
      if (AccessPrivateObject(head, 0x7fffffff)) error = ERR_AccessObject;
      else private_lock = TRUE;
   }

   if (!error) {
      // Call the base class's NewObject() support.  If the class is a child, we will also call its supporting
      // NewObject() action if it has specified one.  Note: The NewObject support caller has a special feature
      // where it passes the expected object context in the args pointer.

      parasol::SwitchContext context(head); // Scope must be limited to the PerformAction() call

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
   }

   if (!error) {
      // Increment the class' open count if the object is private.  We do not increment the count for public objects,
      // because this prevents the Core from expunging modules correctly during shutdown.

      head->ExtClass->OpenCount++;
      if (mc->Base) mc->Base->OpenCount++;

      *ObjectID = head_id;

      if (((Flags & (NF::UNIQUE|NF::NAME)) != NF::NIL) and (Name)) SetName(head, Name); // Set the object's name if it was specified

      head->Flags = head->Flags & (~NF::NEW_OBJECT);
      if (Object) *Object = head;

      return ERR_Okay;
   }
   else {
      if (head) {
         head->Flags = head->Flags & (~NF::NEW_OBJECT);

         if (private_lock) ReleasePrivateObject(head);
         FreeResource(head);
      }

      *ObjectID = 0;
      return error;
   }
}

/*****************************************************************************

-FUNCTION-
ResolveClassName: Resolves any class name to a unique identification ID.

This function will resolve a class name to its unique ID.

Class ID's are used by functions such as ~NewObject() for fast processing.

-INPUT-
cstr Name: The name of the class that requires resolution.

-RESULT-
cid: Returns the class ID identified from the class name, or NULL if the class could not be found.
-END-

*****************************************************************************/

CLASSID ResolveClassName(CSTRING ClassName)
{
   if ((!ClassName) or (!*ClassName)) {
      parasol::Log log(__FUNCTION__);
      log.warning(ERR_NullArgs);
      return 0;
   }

   if (auto item = find_class(StrHash(ClassName, FALSE))) return item->ClassID;

   else return 0;
}

/*****************************************************************************

-FUNCTION-
ResolveClassID: Converts a valid class ID to its equivalent name.

This function is able to resolve a valid class identifier to its equivalent name.  The name is resolved by scanning the
class database, so the class must be registered in the database for this function to return successfully.

-INPUT-
cid ID: The ID of the class that needs to be resolved.

-RESULT-
cstr: Returns the name of the class, or NULL if the ID is not recognised.  Standard naming conventions apply, so it can be expected that the string is capitalised and without spaces, e.g. "NetSocket".
-END-

*****************************************************************************/

CSTRING ResolveClassID(CLASSID ID)
{
   if (auto item = find_class(ID)) return item->Name;

   parasol::Log log(__FUNCTION__);
   log.warning("Failed to resolve ID $%.8x", ID);
   return NULL;
}
