/*********************************************************************************************************************
-CATEGORY-
Name: Objects
-END-
*********************************************************************************************************************/

#include "defs.h"
#include <parasol/main.h>

extern "C" ERROR CLASS_Free(extMetaClass *, APTR);
extern "C" ERROR CLASS_Init(extMetaClass *, APTR);

static bool master_sorted = false;

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

*********************************************************************************************************************/

ERROR NewObject(LARGE ClassID, NF Flags, OBJECTPTR *Object)
{
   pf::Log log(__FUNCTION__);

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
         master_sorted = true;
      }
   }
   else if (!(mc = (extMetaClass *)FindClass(class_id))) {
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

   if (!AllocMemory(mc->Size, MEM_OBJECT|MEM_NO_LOCK|(((Flags & NF::UNTRACKED) != NF::NIL) ? MEM_UNTRACKED : 0), (APTR *)&head, &head_id)) {
      head->UID     = head_id;
      head->ClassID = mc->BaseClassID;
      head->Class   = (extMetaClass *)mc;
      head->Flags   = Flags;

      if (mc->BaseClassID IS mc->SubClassID) head->SubID = 0; // Object derived from a base class
      else head->SubID = mc->SubClassID; // Object derived from a sub-class

      // Tracking for our new object is configured here.

      if (mc->Flags & CLF_NO_OWNERSHIP) { }
      else if ((Flags & NF::UNTRACKED) != NF::NIL) {
         if (class_id IS ID_MODULE); // Untracked modules have no owner, due to the expunge process.
         else {
            // If the object is private and untracked, set its owner to the current task.  This will ensure that the object
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

This function is able to resolve a valid class identifier to its equivalent name.  The name is resolved by scanning the
class database, so the class must be registered in the database for this function to return successfully.

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
