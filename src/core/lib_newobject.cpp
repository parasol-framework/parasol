/*****************************************************************************
-CATEGORY-
Name: Objects
-END-
*****************************************************************************/

#include "defs.h"
#include <parasol/main.h>

extern "C" ERROR CLASS_Free(extMetaClass *, APTR);
extern "C" ERROR CLASS_Init(extMetaClass *, APTR);

static LONG add_shared_object(OBJECTPTR, OBJECTID, WORD);

/*****************************************************************************

-FUNCTION-
CreateObject: Provides a simplified means of creating and initialising new objects.

CreateObject() combines the ~NewObject() and ~SetFields() functions into a stream-lined creation process and
initialises the resulting object.  This is a convenience function and is not an integral part of the object management
services.

Please refer to the ~NewObject() function for information on the allocation of new objects.  Also see the ~SetFields()
function for an outline of the formatting details of the tag arguments.

-INPUT-
large ClassID: Set to a class ID obtained from the "system/register.h" file or from ~ResolveClassName().
int(NF) Flags: Optional flags to directly pass to ~NewObject().
&obj Object: Pointer to an address variable that will store a reference to the new object.
vtags Tags: Field tags are specified here.  See the ~SetFields() function for information on the structure of the tags.  Remember to terminate the tag-list with TAGEND.

-ERRORS-
Okay
Args
NewObject
SetField

*****************************************************************************/

ERROR CreateObject(LARGE ClassID, LONG Flags, OBJECTPTR *argObject, ...)
{
   va_list list;
   va_start(list, argObject);
   ERROR error = CreateObjectF(ClassID, Flags, argObject, list);
   va_end(list);
   return error;
}

ERROR CreateObjectF(LARGE ClassID, LONG Flags, OBJECTPTR *argObject, va_list List)
{
   parasol::Log log("CreateObject");

   if (glLogLevel > 2) {
      extMetaClass **ptr;
      if (!KeyGet(glClassMap, ClassID, (APTR *)&ptr, NULL)) {
         log.branch("Class: %s", ptr[0]->ClassName);
      }
      else log.branch("Class: $%.8x", (ULONG)ClassID);
   }
   else log.branch("Class: $%.8x", (ULONG)ClassID);

   OBJECTPTR object;
   ERROR error;

   if (!NewObject(ClassID, Flags|NF_SUPPRESS_LOG, &object)) {
      if (!SetFieldsF(object, List)) {
         if (!(error = acInit(object))) {
            if (argObject) *argObject = object;
            else if (object->UID < 0) ReleaseObject(object);
            return ERR_Okay;
         }
      }
      else error = log.warning(ERR_SetField);

      if (object->UID < 0) {
         acFree(object);
         ReleaseObject(object);
      }
      else acFree(object);
   }
   else error = ERR_NewObject;

   if (argObject) *argObject = NULL;
   return error;
}

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
int(NF) Flags:  Optional flags.
&obj Object: Pointer to an address variable that will store a reference to the new object.

-ERRORS-
Okay
NullArgs
MissingClass: The ClassID is invalid or refers to a class that is not installed.
Failed
ObjectExists: An object with the provided Name already exists in the system (applies only when the NF_UNIQUE flag has been used).
-END-

*****************************************************************************/

ERROR NewObject(LARGE ClassID, LONG Flags, OBJECTPTR *Object)
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

   Flags &= (NF_UNTRACKED|NF_INTEGRAL|NF_UNIQUE|NF_NAME|NF_PUBLIC|NF_SUPPRESS_LOG); // Very important to eliminate any internal flags.

   // If the object is integral then turn off use of the UNTRACKED flag (otherwise the child will
   // end up being tracked to its task rather than its parent object).

   if (Flags & NF_INTEGRAL) Flags &= ~NF_UNTRACKED;

   // Force certain flags on the class' behalf

   if (mc->Flags & CLF_PUBLIC_OBJECTS) Flags |= NF_PUBLIC;
   if (mc->Flags & CLF_NO_OWNERSHIP)   Flags |= NF_UNTRACKED;

   if (Flags & NF_PUBLIC) {
      log.warning("Request to allocate public object denied, use NewLockedObject().");
      return ERR_Args;
   }

   if (!(Flags & NF_SUPPRESS_LOG)) log.branch("%s #%d, Flags: $%x", mc->ClassName, glSharedControl->PrivateIDCounter, Flags);

   OBJECTPTR head = NULL;
   MEMORYID head_id = 0;
   ERROR error = ERR_Okay;

   if (!AllocMemory(mc->Size + sizeof(Stats), MEM_OBJECT|MEM_NO_LOCK|(Flags & NF_UNTRACKED ? MEM_UNTRACKED : 0), (APTR *)&head, &head_id)) {
      head->Stats     = (Stats *)ResolveAddress(head, mc->Size);
      head->UID       = head_id;
      head->MemFlags |= MEM_NO_LOCK; // Prevents private memory allocations made by this class from being automatically locked.
      head->ClassID   = mc->BaseClassID;
      if (mc->BaseClassID IS mc->SubClassID) { // Object derived from a base class
         head->SubID = 0;
      }
      else { // Object derived from a sub-class
         head->SubID = mc->SubClassID;
      }

      head->Class = (extMetaClass *)mc;
      if ((glCurrentTaskID != SystemTaskID) and (!(mc->Flags & CLF_NO_OWNERSHIP))) {
         head->TaskID = glCurrentTaskID;
      }

      if (!(Flags & NF_UNTRACKED)) { // Don't track untracked objects to specific threads.
         head->ThreadMsg = tlThreadWriteMsg; // If the object needs to belong to a thread, this will record it.
      }

      // Note that the untracked flag is turned off because if it is left on, objects will allocate their children as
      // being untracked (due to use of ObjectFlags as a reference for child allocation).   We don't want our tracking to
      // be screwed up do we...

      set_object_flags(head, (WORD)Flags);
      head->Flags |= NF_NEW_OBJECT;

      // Tracking for our new object is configured here.

      OBJECTPTR task;
      if (mc->Flags & CLF_NO_OWNERSHIP) {
      }
      else if (Flags & NF_UNTRACKED) {
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

         head->Flags &= ~NF_NEW_OBJECT;
         *Object = head;
         return ERR_Okay;
      }
   }
   else error = ERR_AllocMemory;

   if (head) {
      head->Flags &= ~NF_NEW_OBJECT;
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
int(NF) Flags: Optional flags.
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

ERROR NewLockedObject(LARGE ClassID, LONG Flags, OBJECTPTR *Object, OBJECTID *ObjectID, CSTRING Name)
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

   Flags &= (NF_UNTRACKED|NF_INTEGRAL|NF_UNIQUE|NF_NAME|NF_PUBLIC|NF_SUPPRESS_LOG); // Very important to eliminate any internal flags.

   // If the object is to be a child of a larger object, turn off use of the UNTRACKED flag (otherwise the child will
   // end up being tracked to its task rather than its parent object).

   if (Flags & NF_INTEGRAL) Flags &= ~NF_UNTRACKED;

   // Force certain flags on the class' behalf

   if (mc->Flags & CLF_PUBLIC_OBJECTS) Flags |= NF_PUBLIC;
   if (mc->Flags & CLF_NO_OWNERSHIP)   Flags |= NF_UNTRACKED;

   log.branch("%s #%d, Flags: $%x", mc->ClassName, (Flags & NF_PUBLIC) ? glSharedControl->IDCounter : glSharedControl->PrivateIDCounter, Flags);

   OBJECTPTR head = NULL;
   MEMORYID head_id = 0;
   APTR sharelock = NULL;
   BYTE resourced = FALSE;
   ERROR error = ERR_Okay;

   if (((Flags & NF_UNIQUE) and (Name)) or (Flags & NF_PUBLIC)) {
      // Locking RPM_SharedObjects for the duration of this function will ensure that other tasks do not create shared
      // objects with the same name when NF_UNIQUE is in use.

      if (!AccessMemory(RPM_SharedObjects, MEM_READ_WRITE, 2000, &sharelock)) {
         if (Flags & NF_UNIQUE) {
            OBJECTID search_id;
            LONG count = 1;
            if ((!FindObject(Name, class_id, FOF_INCLUDE_SHARED|FOF_SMART_NAMES, &search_id, &count)) and (search_id)) {
               *ObjectID = search_id;
               ReleaseMemoryID(RPM_SharedObjects);
               return ERR_ObjectExists; // Return ERR_ObjectExists so that the caller knows that the failure was not caused by an object creation error.
            }
         }
      }
      else return log.warning(ERR_AccessMemory);
   }

   if ((Flags & NF_PUBLIC) and (mc->Flags & CLF_PRIVATE_ONLY)) {
      log.warning("Public objects cannot be allocated from class $%.8x.", class_id);
      Flags &= ~NF_PUBLIC;
   }

   if (Flags & NF_PUBLIC) {
      if (!AllocMemory(mc->Size + sizeof(Stats), MEM_PUBLIC|MEM_OBJECT, (void **)&head, &head_id)) {
         head->Stats = (Stats *)ResolveAddress(head, mc->Size);
         head->MemFlags |= MEM_PUBLIC;
         head->UID = head_id;
      }
      else error = ERR_AllocMemory;
   }
   else if (!AllocMemory(mc->Size + sizeof(Stats), MEM_OBJECT|MEM_NO_LOCK, (APTR *)&head, &head_id)) {
      head->Stats = (Stats *)ResolveAddress(head, mc->Size);
      head->UID = head_id;
      head->MemFlags |= MEM_NO_LOCK; // Prevents private memory allocations made by this class from being automatically locked.
   }
   else error = ERR_AllocMemory;

   if (!error) {
      head->ClassID = mc->BaseClassID;
      if (mc->BaseClassID IS mc->SubClassID) { // Object derived from a base class
         head->SubID = 0;
      }
      else head->SubID = mc->SubClassID; // Object derived from a sub-class

      head->Class = mc;
      if ((glCurrentTaskID != SystemTaskID) and (!(mc->Flags & CLF_NO_OWNERSHIP))) {
         head->TaskID = glCurrentTaskID;
      }

      if (!(Flags & (NF_PUBLIC|NF_UNTRACKED))) { // Don't track public and untracked objects to specific threads.
         head->ThreadMsg = tlThreadWriteMsg; // If the object needs to belong to a thread, this will record it.
      }

      // Add the object to our lists.  This must be done before calling the NewObject action because we will otherwise
      // risk stuffing up the list order.

      if (Flags & NF_PUBLIC) {
         if (!(error = add_shared_object(head, head_id, (WORD)Flags))) resourced = TRUE;
      }
   }

   if (!error) {
      // Note that the untracked flag is turned off because if it is left on, objects will allocate their children as
      // being untracked (due to use of ObjectFlags as a reference for child allocation).

      set_object_flags(head, (WORD)Flags);
      head->Flags |= NF_NEW_OBJECT;

      // Tracking for our newly created object is configured here.

      OBJECTPTR task;
      if (mc->Flags & CLF_NO_OWNERSHIP); // The class mandates that objects have no owner.
      else if (Flags & NF_UNTRACKED) {
         if (class_id IS ID_MODULE); // Untracked modules have no owner, due to the expunge process.
         else if (!(Flags & NF_PUBLIC)) {
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
   }

   if ((!error) and (Object) and (!(Flags & NF_PUBLIC))) {
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

      if (!(Flags & NF_PUBLIC)) {
         head->ExtClass->OpenCount++;
         if (mc->Base) mc->Base->OpenCount++;
      }

      *ObjectID = head_id;

      if ((Flags & (NF_UNIQUE|NF_NAME)) and (Name)) SetName(head, Name); // Set the object's name if it was specified

      head->Flags &= ~NF_NEW_OBJECT;
      if (Flags & NF_PUBLIC) {
         if (Object) { // All we need to do is set the Locked flag rather than making a call to AccessObject()
            head->Locked = TRUE;
            *Object = head;
         }
         else ReleaseMemoryID(head_id);
      }
      else if (Object) *Object = head;

      if (sharelock) ReleaseMemoryID(RPM_SharedObjects);
      return ERR_Okay;
   }
   else {
      if (head) {
         head->Flags &= ~NF_NEW_OBJECT;

         if (Flags & NF_PUBLIC) {
            if (resourced) remove_shared_object(head_id);
            ReleaseMemoryID(head_id);
            FreeResourceID(head_id);
         }
         else {
            if (private_lock) ReleasePrivateObject(head);
            FreeResource(head);
         }
      }

      if (sharelock) ReleaseMemoryID(RPM_SharedObjects);
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
   parasol::Log log(__FUNCTION__);

   if ((!ClassName) or (!*ClassName)) {
      log.warning(ERR_NullArgs);
      return 0;
   }

   ClassItem *item;
   if ((item = find_class(StrHash(ClassName, FALSE)))) {
      return item->ClassID;
   }
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
   parasol::Log log(__FUNCTION__);
   ClassItem *item;

   if ((item = find_class(ID))) return item->Name;
   else {
      log.warning("Failed to resolve ID $%.8x", ID);
      return NULL;
   }
}

//****************************************************************************

ERROR find_public_object_entry(SharedObjectHeader *Header, OBJECTID ObjectID, LONG *Position)
{
   auto array = (SharedObject *)ResolveAddress(Header, Header->Offset);

   LONG floor   = 0;
   LONG ceiling = Header->NextEntry;
   LONG i       = ceiling>>1;
   for (LONG j=0; j < 2; j++) {
      while ((!array[i].ObjectID) and (i > 0)) i--;

      if (ObjectID < array[i].ObjectID)      floor = i + 1;
      else if (ObjectID > array[i].ObjectID) ceiling = i;
      else {
         *Position = i;
         return ERR_Okay;
      }
      i = floor + ((ceiling - floor)>>1);
  }

   while ((!array[i].ObjectID) and (i > 0)) i--;

   if (ObjectID < array[i].ObjectID) {
      while (i < ceiling) {
         if (array[i].ObjectID IS ObjectID) {
            *Position = i;
            return ERR_Okay;
         }
         i++;
      }
   }
   else {
      while (i >= 0) {
         if (array[i].ObjectID IS ObjectID) {
            *Position = i;
            return ERR_Okay;
         }
         i--;
      }
   }

   return ERR_Search;
}

/*****************************************************************************
** These functions are responsible for maintaining the public object list table.
*/

static ERROR add_shared_object(OBJECTPTR Object, OBJECTID ObjectID, WORD Flags)
{
   parasol::Log log(__FUNCTION__);

   // This routine guarantees that the public_objects table will be sorted in the order of object creation (ObjectID)
   // so long as the ObjectID is incremented correctly.

   if (ObjectID >= 0) return log.warning(ERR_Args);

   parasol::ScopedAccessMemory<SharedObjectHeader> lock(RPM_SharedObjects, MEM_READ_WRITE, 2000);

   if (!lock.granted()) return log.warning(ERR_AccessMemory);

   auto hdr = lock.ptr;
   auto objects = (SharedObject *)ResolveAddress(hdr, hdr->Offset);

   // If the table is at its limit, compact it first by eliminating entries that no longer contain any data.

   if (hdr->NextEntry >= hdr->ArraySize) {
      LONG last_entry = 0;
      LONG entry_size = sizeof(SharedObject)>>1;
      for (LONG i=0; i < hdr->ArraySize; i++) {
         if (!objects[i].ObjectID) {
            LONG j;
            for (j=i+1; (j < hdr->ArraySize) and (!objects[j].ObjectID); j++);
            if (j < hdr->ArraySize) {
               // Move the record at position j to position i
               for (LONG k=0; k < entry_size; k++) {
                  ((WORD *)(objects+i))[k] = ((WORD *)(objects+j))[k];
               }
               // Kill the moved record at its previous position
               objects[j].ObjectID = 0;
               objects[j].OwnerID  = 0;
               last_entry = i;
            }
            else break;
         }
         else last_entry = i;
      }
      if (last_entry < hdr->ArraySize-1) hdr->NextEntry = last_entry + 1;
      else hdr->NextEntry = hdr->ArraySize;
      log.msg("Public object array compressed from %d entries to %d entries.", hdr->ArraySize, hdr->NextEntry);
   }

   // If the table is at capacity, we must allocate more space for new records

   if (hdr->NextEntry >= hdr->ArraySize) {
      log.warning("The public object array is at capacity (%d blocks)", hdr->ArraySize);
      return ERR_ArrayFull;
   }

   // "Pull-back" the NextPublicObject position if there are null entries present at the tail-end of the array (occurs if objects are allocated then quickly freed).

   while ((hdr->NextEntry > 0) and (objects[hdr->NextEntry-1].ObjectID IS 0)) {
      hdr->NextEntry--;
   }

   // Add the entry to the next available space

   objects[hdr->NextEntry].ObjectID = ObjectID;
   objects[hdr->NextEntry].OwnerID  = Object->OwnerID;

   if (Object->ExtClass->Flags & CLF_NO_OWNERSHIP) {
      objects[hdr->NextEntry].MessageMID = 0;
   }
   else objects[hdr->NextEntry].MessageMID = glTaskMessageMID;

/* We should use this routine to set the message ID in case the object is going to be a child of another that is
public and belongs to a different task.

   if (Object->TaskID IS glCurrentTaskID) {
      objects[hdr->NextEntry].MessageMID = glTaskMessageMID;
   }
   else if (LOCK_TASKS() IS ERR_Okay) {
      for (i=0; i < MAX_TASKS; i++) {
         if (shTasks[i].TaskID IS Object->TaskID) {
            objects[hdr->NextEntry].MessageMID = shTasks[i].MessageID;
            break;
         }
      }
      if (i IS MAX_TASKS) objects[hdr->NextEntry].MessageMID = glTaskMessageMID;
      UNLOCK_TASKS();
   }
*/
   if (Flags & NF_PUBLIC) objects[hdr->NextEntry].Address = NULL;
   else objects[hdr->NextEntry].Address = Object;

   objects[hdr->NextEntry].ClassID    = Object->ClassID;
   objects[hdr->NextEntry].Name[0]    = 0;
   objects[hdr->NextEntry].Flags      = Flags;
   objects[hdr->NextEntry].InstanceID = glInstanceID;
   hdr->NextEntry++;

   return ERR_Okay;
}

//****************************************************************************

void remove_shared_object(OBJECTID ObjectID)
{
   parasol::Log log(__FUNCTION__);
   SharedObjectHeader *publichdr;

   if (!AccessMemory(RPM_SharedObjects, MEM_READ_WRITE, 2000, (void **)&publichdr)) {
      LONG pos;
      if (find_public_object_entry(publichdr, ObjectID, &pos) IS ERR_Okay) {
         SharedObject *obj = (SharedObject *)ResolveAddress(publichdr, publichdr->Offset);
         obj[pos].ObjectID   = 0;
         obj[pos].ClassID    = 0;
         obj[pos].OwnerID    = 0;
         obj[pos].Name[0]    = 0;
         obj[pos].Flags      = 0;
         obj[pos].InstanceID = 0;
      }
      else log.warning("Object #%d is not registered in the public object list.", ObjectID);
      ReleaseMemoryID(RPM_SharedObjects);
   }
   else log.warning(ERR_AccessMemory);
}

