/*****************************************************************************

The source code of the Parasol Framework is made publicly available under the
terms described in the LICENSE.TXT file that is distributed with this package.
Please refer to it for further information on licensing.

-CATEGORY-
Name: System
-END-

*****************************************************************************/

#include <stdlib.h>

#ifdef __unix__
 #include <stdio.h>
 #include <unistd.h>
 #include <signal.h>

 #ifndef __ANDROID__
  #include <sys/msg.h>
 #endif

 #include <sys/time.h>
 #ifdef __linux__
  #include <sys/sysinfo.h>
 #elif __APPLE__
  #include <sys/sysctl.h>
 #endif
 #include <fcntl.h>
 #include <time.h>
 #include <string.h> // Required for memset() on OS X

#elif _WIN32
 #include <string.h> // Required for memmove()
#endif

#ifdef __ANDROID__
 #include <android/log.h>
#endif

#include <math.h>

#include "defs.h"

/*****************************************************************************

-FUNCTION-
AllocateID: Generates unique ID's for general purposes.

This function generates unique ID's that can be used in other Core functions.  It requires the type of the ID that is
needed, and will respond by returning a unique identifier for that type.

ID allocations are permanent, so there is no need to free the allocated ID once it is no longer required.

-INPUT-
int(IDTYPE) Type: The type of ID that is required.

-RESULT-
int: A unique ID matching the requested type will be returned.  This function can return zero if the Type is unrecognised, or if an internal error occurred.

*****************************************************************************/

LONG AllocateID(LONG Type)
{
   if (Type IS IDTYPE_MESSAGE) {
      if (glSharedControl->MessageIDCount < 10000) glSharedControl->MessageIDCount = 10000;
      LONG id = __sync_add_and_fetch(&glSharedControl->MessageIDCount, 1);

      LogF("AllocateID()","MessageID: %d", id);
      return id;
   }
   else if (Type IS IDTYPE_GLOBAL) {
      LONG id = __sync_add_and_fetch(&glSharedControl->GlobalIDCount, 1);
      return id;
   }
   else if (Type IS IDTYPE_FUNCTION) {
      UWORD id = __sync_add_and_fetch(&glFunctionID, 1);
      return id;
   }

   return 0;
}

/*****************************************************************************

-FUNCTION-
CheckObjectExists: Checks if a particular object is still available in the system.
Category: Objects

The CheckObjectExists() function checks for the existence of an object within the system.  It is commonly used to check
for the presence of shared objects, which can be removed from the system at any time by other threads and processes.

This function allows for ID and name based testing.  Please note that in the case of name checking, it is possible for
multiple objects with the same name to exist at the time of calling this function.

-INPUT-
oid Object: The object ID that you want to look for.
cstr Name: If the ID is not known, specify the Name of the object here.  Otherwise set to NULL.

-ERRORS-
True:  The object exists.
False: The object ID does not exist.
Args:  Neither of the ObjectID or Name arguments were specified.

*****************************************************************************/

ERROR CheckObjectExists(OBJECTID ObjectID, CSTRING Name)
{
   LONG i, j;

   if (Name) {
      // Check the private object key-store for the name.

      if (!thread_lock(TL_OBJECT_LOOKUP, 4000)) {
         if (!VarGet(glObjectLookup, Name, NULL, NULL)) {
            thread_unlock(TL_OBJECT_LOOKUP);
            return ERR_True;
         }
         thread_unlock(TL_OBJECT_LOOKUP);
      }

      BYTE buffer[MAX_NAME_LEN+1];
      for (i=0; (Name[i]) AND (i < MAX_NAME_LEN); i++) { // Change the Name string to all-lower case and build a hash
         UBYTE c = Name[i];
         if ((c >= 'A') AND (c <= 'Z')) c = c - 'A' + 'a';
         buffer[i] = c;
      }
      buffer[i] = 0;

      struct SharedObjectHeader *header;
      if (!AccessMemory(RPM_SharedObjects, MEM_READ, 2000, (void **)&header)) {
         struct SharedObject *list = ResolveAddress(header, header->Offset);

         for (i=0; i < header->NextEntry; i++) {
            if ((list[i].ObjectID) AND ((!list[i].InstanceID) OR (list[i].InstanceID IS glInstanceID))) {
               for (j=0; list[i].Name[j]; j++) {
                  if (list[i].Name[j] != buffer[j]) break;
               }
               if ((!list[i].Name[j]) AND (!buffer[j])) {
                  ReleaseMemoryID(RPM_SharedObjects);
                  return ERR_True;
               }
            }
         }

         ReleaseMemoryID(RPM_SharedObjects);
         return ERR_False;
      }
      else return LogError(ERH_CheckObjectExists, ERR_AccessMemory);
   }
   else if (ObjectID < 0) {
      if (!LOCK_PUBLIC_MEMORY(4000)) {
         if (!find_public_mem_id(glSharedControl, ObjectID, NULL)) {
            UNLOCK_PUBLIC_MEMORY();
            return ERR_True;
         }
         UNLOCK_PUBLIC_MEMORY();
         return ERR_False;
      }
      else return LogError(ERH_CheckObjectExists, ERR_SystemLocked);
   }
   else if (ObjectID > 0) {
      if (ObjectID IS SystemTaskID) return ERR_True;

      if (!thread_lock(TL_PRIVATE_MEM, 4000)) {
         LONG pos;
         LONG result = ERR_False;
         if ((pos = find_private_mem_id(ObjectID, NULL)) != -1) {
            if (((OBJECTPTR)glPrivateMemory[pos].Address)->Flags & NF_UNLOCK_FREE);
            else result = ERR_True;
         }
         thread_unlock(TL_PRIVATE_MEM);
         return result;
      }
      else return LogError(ERH_CheckObjectExists, ERR_LockFailed);
   }
   else return LogError(ERH_CheckObjectExists, ERR_Args);
}

/*****************************************************************************

-FUNCTION-
ClearMemory: Clears large blocks of memory very quickly.
Category: Memory

Use the ClearMemory() function when you need to clear a block of memory as efficiently as possible.

-INPUT-
ptr Memory: Pointer to the memory block that you want to clear.
int Length: The total number of bytes that you want to clear.

-ERRORS-
Okay: The memory was cleared.
NullArgs: The Memory argument was not specified.

*****************************************************************************/

ERROR ClearMemory(APTR Memory, LONG Length)
{
   if (!Memory) return ERR_NullArgs;
   memset(Memory, 0, Length); // memset() is assumed to be optimised by the compiler.
   return ERR_Okay;
}

/*****************************************************************************

-FUNCTION-
CopyMemory: Copies a block of bytes from a source to a destination address.
Category: Memory

This function copies a block of bytes from a source address to a destination address. Ideally the fastest algorithm
available for the machine will be used to perform the copy operation.  If a fine-tuned algorithm is unavailable, a
stock copy operation is used.

-INPUT-
cptr Src: Source address to copy.
ptr Dest: Destination address.
int Size: The total number of bytes to copy from the source to the destination.

-ERRORS-
Okay:
NullArgs: Src and/or Dest parameters were not provided.
Args:

*****************************************************************************/

ERROR CopyMemory(const void *Src, APTR Dest, LONG Length)
{
   if ((!Src) OR (!Dest)) return ERR_NullArgs;
   if (Length < 0) return ERR_Args;
   if (Src IS Dest) return ERR_Okay;

   // As of 2013 we are presuming that memmove() is suitably pre-optimised by either the compiler or the host platform.

   memmove(Dest, Src, Length);
   return ERR_Okay;
}

/*****************************************************************************

-FUNCTION-
CurrentContext: Returns a pointer to the object that has the current context.
Category: Objects

The CurrentContext() function returns a pointer to the object that has the current context.  In order for an object to
have the context of the current @Task, it must have been successfully set from the ~SetContext()
function.

CurrentContext() is typically used to find out what object is currently receiving resource allocations.  It is useful
in improving the management of resources that your code may be allocating.

To get the parent context (technically the 'context of the current context'), use GetParentContext(), which is
implemented as a macro.  This is used in method and action routines where the context of the object's caller may be
desired.

-RESULT-
obj: Returns an object pointer (of which the Task has exclusive access to).  This function rarely returns a NULL pointer - this is only possible during the initial start-up and shut-down sequences of the Core.

*****************************************************************************/

OBJECTPTR CurrentContext(void)
{
   return tlContext->Object;
}

/*****************************************************************************

-FUNCTION-
CurrentField: Returns a pointer to the field meta data representing an active get or set function.
Category: Fields

The CurrentField() function returns field meta data for the active thread, if it is operating within a field's get or
set operator.  In all other situations, NULL is returned.

This is a technical function with limited use cases.  Dynamic languages that need to use generic get and set functions
to support virtual field operations will find it useful.

-RESULT-
struct(*Field): Returns the active field meta data or NULL if the thread is not executing a get or set operation.

*****************************************************************************/

struct Field * CurrentField(void)
{
   return tlContext->Field;
}

/*****************************************************************************

-FUNCTION-
CurrentTask: Returns the active Task object.

The CurrentTask() function returns the @Task object of the active process.

If there is a legitimate circumstance where there is no current task (for example if this function is called during
Core initialisation) then the "system task" may be returned (the system task controls and maintains the Core).

-RESULT-
obj: Returns a pointer to the current Task object or NULL if failure.

*****************************************************************************/

OBJECTPTR CurrentTask(void)
{
   return (OBJECTPTR)glCurrentTask;
}

/*****************************************************************************

-FUNCTION-
FastFindObject: Searches for objects by name.
Category: Objects

The FastFindObject() function is an optimised implementation of the ~FindObject() function.  Use it to search for
objects in the system by their name and/or class.  Unlike ~FindObject(), which will return an allocated memory block
that lists all of the objects that were found, FastFindObject() requires a pre-allocated buffer to write the results
to.  This saves the cost of allocation time, which can be expensive in some situations.

The following code example is a typical illustration of this function's use.  It finds the most recent object created
with a given name:

<pre>
OBJECTID id;
FastFindObject("SystemPointer", ID_POINTER, &id, 1, NULL);
</pre>

If FastFindObject() cannot find any objects with the name that you are looking for, it will return an error code.

The list is sorted so that the oldest private object is placed at the start of the list, and the most recent public object
is placed at the end.  Take advantage of this fact to get the oldest or youngest object with the Name that is being
searched for.  Preference is also given to objects that have been created by the calling process, thus foreign objects
are pushed towards the beginning of the array.

-INPUT-
cstr Name:     The name of an object to search for.
cid ClassID:   Set to a class ID to filter the results down to a specific class type.
&oid Array:    Pointer to the array that will store the results.
int ArraySize: Indicates the size of Array, measured in elements.  Must be set to a value of 1 or greater.
&int Count:    This parameter will be updated with the total number of objects stored in Array.  Can be NULL if not required.

-ERRORS-
Okay: At least one object was found and stored in the supplied array.
Args:
Search: No objects matching the given name could be found.
AccessMemory: Access to the RPM_SharedObjects memory block was denied.
LockFailed:
EmptyString:
DoesNotExist:
-END-

*****************************************************************************/

ERROR FastFindObject(CSTRING InitialName, CLASSID ClassID, OBJECTID *Array, LONG ArraySize, LONG *ObjectCount)
{
   struct SharedObjectHeader *header;
   struct SharedObject *entry;
   LONG i;

   if ((!Array) OR (ArraySize < 1)) return LogError(ERH_FastFindObject, ERR_Args);

   if (ObjectCount) *ObjectCount = 0;
   LONG count = 0;

   if ((!InitialName) AND (ClassID)) {
      // Private object search

      if (!thread_lock(TL_PRIVATE_MEM, 4000)) {
         for (i=0; i < glNextPrivateAddress; i++) {
            OBJECTPTR object;
            if ((glPrivateMemory[i].Flags & MEM_OBJECT) AND (object = glPrivateMemory[i].Address)) {
               if (ClassID IS object->ClassID) {
                  if (count < ArraySize) {
                     Array[count] = object->UniqueID;
                     count++;
                  }
                  else if (Array[count-1] < object->UniqueID) {
                     Array[count-1] = object->UniqueID;
                  }
               }
            }
         }
         thread_unlock(TL_PRIVATE_MEM);
      }
      else return LogError(ERH_FastFindObject, ERR_LockFailed);

      // Public object search

      if (!AccessMemory(RPM_SharedObjects, MEM_READ, 2000, (void **)&header)) {
         entry = ResolveAddress(header, header->Offset);
         for (i=0; i < header->NextEntry; i++) {
            if ((entry[i].ObjectID) AND (ClassID IS entry[i].ClassID)) {
               if ((!entry[i].InstanceID) OR (entry[i].InstanceID IS glInstanceID)) {
                  if (count < ArraySize) {
                     Array[count] = entry[i].ObjectID;
                     count++;
                  }
                  else if (Array[count-1] > entry[i].ObjectID) {
                     Array[count-1] = entry[i].ObjectID;
                  }
               }
            }
         }
         ReleaseMemoryID(RPM_SharedObjects);
      }
      else return LogError(ERH_FastFindObject, ERR_AccessMemory);
   }
   else if (InitialName) {
      if (!InitialName[0]) return LogError(ERH_FastFindObject, ERR_EmptyString);

      // If an integer based name (defined by #num) is passed, we translate it to an ObjectID rather than searching for
      // an object of name "#1234".

      BYTE number = FALSE;
      if (InitialName[0] IS '#') number = TRUE;
      else {
         // If the name consists entirely of numbers, it must be considered an object ID (we can make this check because
         // it is illegal for a name to consist entirely of figures).

         if (InitialName[0] IS '-') i = 1;
         else i = 0;
         for (; InitialName[i]; i++) {
            if (InitialName[i] < '0') break;
            if (InitialName[i] > '9') break;
         }
         if (!InitialName[i]) number = TRUE;
      }

      if (number) {
         OBJECTID objectid;
         if ((objectid = (OBJECTID)StrToInt(InitialName))) {
            if (!CheckObjectExists(objectid, NULL)) {
               *Array = objectid;
               if (ObjectCount) *ObjectCount = 1;
               return ERR_Okay;
            }
            else return ERR_Search;
         }
         else return ERR_Search;
      }

      if (!StrMatch("owner", InitialName)) {
         if ((tlContext != &glTopContext) AND (tlContext->Object->OwnerID)) {
            if (!CheckObjectExists(tlContext->Object->OwnerID, NULL)) {
               *Array = tlContext->Object->OwnerID;
               if (ObjectCount) *ObjectCount = 1;
               return ERR_Okay;
            }
            else return ERR_DoesNotExist;
         }
         else return ERR_DoesNotExist;
      }

      struct sortlist {
         OBJECTID id;           // The ID of the object
         MEMORYID messagemid;   // MessageMID of the task that created the object, for sort purposes
      } objlist[ArraySize];

      // Private object search

      if (!thread_lock(TL_OBJECT_LOOKUP, 4000)) {
         OBJECTPTR *list;
         LONG list_size;
         if (!VarGet(glObjectLookup, InitialName, (APTR *)&list, &list_size)) {
            list_size = list_size / sizeof(OBJECTPTR);
            for (i=0; i < list_size; i++) {
               OBJECTPTR object = list[i];
               if ((object) AND ((!ClassID) OR (ClassID IS object->ClassID))) {
                  if (count < ArraySize) {
                     objlist[count].id = object->UniqueID;
                     objlist[count].messagemid = glTaskMessageMID;
                     count++;
                  }
                  else if (objlist[count-1].id < object->UniqueID) {
                     objlist[count-1].id = object->UniqueID;
                     objlist[count-1].messagemid = glTaskMessageMID;
                  }
               }

               i++;
            }
         }
         thread_unlock(TL_OBJECT_LOOKUP);
      }

      // Public object search.  When looking for publicly named objects we need to keep them arranged so that preference
      // is given to objects that this process created.  This causes some mild overhead but is vital for ensuring
      // that programs aren't confused when they use identically named objects.

      ULONG hash = 5381;
      UBYTE name[MAX_NAME_LEN+1];
      for (i=0; (InitialName[i]) AND (i < MAX_NAME_LEN - 1); i++) {
         UBYTE c = InitialName[i];
         if ((c >= 'A') AND (c <= 'Z')) c = c - 'A' + 'a';
         name[i] = c;
         hash = ((hash<<5) + hash) + c;
      }
      name[i] = 0;

      if (!AccessMemory(RPM_SharedObjects, MEM_READ, 2000, (void **)&header)) {
         entry = ResolveAddress(header, header->Offset);
         for (i=0; i < header->NextEntry; i++) {
            if (entry[i].ObjectID) {
               if ((!entry[i].InstanceID) OR (entry[i].InstanceID IS glInstanceID)) {
                  if ((ClassID) AND (ClassID != entry[i].ClassID));
                  else if (entry[i].Name[0] IS name[0]) {
                     if (!StrCompare(entry[i].Name, name, 0, STR_CASE|STR_MATCH_LEN)) {
                        if (count < ArraySize) {
                           objlist[count].id = entry[i].ObjectID;
                           objlist[count].messagemid = entry[i].MessageMID;
                           count++;
                        }
                        else if (objlist[count-1].id > entry[i].ObjectID) {
                           // The discovered object has a more recent ID than the last entry in the list, so replace it
                           // (assuming that it won't replace something that was created from our own task space).

                           if ((objlist[count-1].messagemid IS glTaskMessageMID) AND (entry[i].MessageMID != glTaskMessageMID)) continue;
                           objlist[count-1].id = entry[i].ObjectID;
                           objlist[count-1].messagemid = entry[i].MessageMID;
                        }
                     }
                  }
               }
            }
         }

         ReleaseMemoryID(RPM_SharedObjects);

         if (count > 0) {
            if (ObjectCount) *ObjectCount = count;

            for (i=0; i < count - 1; i++) {
               // Smaller ID's are more recent, so they bubble down the list.  Object's created in our own task space
               // are also given preference.

               if ((objlist[i].id < objlist[i+1].id) OR
                  ((objlist[i].messagemid IS glTaskMessageMID) AND (objlist[i+1].messagemid != glTaskMessageMID))) {
                  struct sortlist swap = objlist[i];
                  objlist[i] = objlist[i+1];
                  objlist[i+1] = swap;
                  i = -1;
               }
            }

            for (i=0; i < count; i++) Array[i] = objlist[i].id;

            return ERR_Okay;
         }
         else return ERR_Search;
      }
      else return LogError(ERH_FastFindObject, ERR_AccessMemory);
   }
   else return LogError(ERH_FastFindObject, ERR_NullArgs);

   // Sort the list so that the highest number is at the top and the lowest number is at the bottom.

   if (count > 0) {
      OBJECTID swap;

      if (ObjectCount) *ObjectCount = count;

      for (i=0; i < count-1; i++) {
         if (Array[i] < Array[i+1]) {
            swap = Array[i];
            Array[i] = Array[i+1];
            Array[i+1] = swap;
            i = -1;
         }
      }

      return ERR_Okay;
   }
   else {
      if (glLogLevel >= 4) LogF("FindFast","Could not find object \"%s\".", InitialName);
      return ERR_Search;
   }
}

/*****************************************************************************

-FUNCTION-
FindClass: Returns all class objects for a given class ID.
Category: Objects

This function is used to find a specific class by ID.  If a matching class is not already loaded, the class database
is checked to determine if the class is installed on the system.  If a match is discovered, the corresponding module
will be loaded into memory and the class will be returned.  In the event of failure or if there is no matching class
registered, NULL is returned.

If the ID of the class is not known, please call ~ResolveClassName() and then pass the resulting ID to this
function.

-INPUT-
cid ClassID: A class ID such as one retrieved from ~ResolveClassName().

-RESULT-
obj(MetaClass): Returns a pointer to the MetaClass structure that has been found as a result of the search, or NULL if no matching class was found.

*****************************************************************************/

struct rkMetaClass * FindClass(CLASSID ClassID)
{
   if (ClassID IS ID_METACLASS) { // Return the internal pointer to the MetaClass.
      return &glMetaClass;
   }
   else if (!ClassID) {
      return NULL;
   }
   else {
      // A simple KeyGet() works for base-classes and sub-classes because the hash map is indexed by class name.

      struct rkMetaClass **ptr;
      if (!KeyGet(glClassMap, ClassID, (APTR *)&ptr, NULL)) {
         return ptr[0];
      }

      // If we are shutting down, do not go any further as we do not want to load new modules during the shutdown process.

      if (glProgramStage IS STAGE_SHUTDOWN) return NULL;

      // Since the class is not in the system, we need to try and find a master in the references.  If we find one, we can
      // initialise the module and then find the new Class.
      //
      // Note: Children of the class are not automatically loaded into memory if they are unavailable at the time.  Doing so
      // would result in lost CPU and memory resources due to loading code that may not be needed.

      struct ClassItem *item;
      CSTRING path = NULL;
      if ((item = find_class(ClassID))) {
         if (item->PathOffset) path = (CSTRING)item + item->PathOffset;
      }

      if (!path) {
         struct ModuleItem *mod;
         if ((mod = find_module(ClassID))) path = (STRING)(mod + 1);
      }

      struct rkMetaClass *class = NULL;
      if (path) {
         // Load the module from the associated location and then find the class that it contains.  If the module fails,
         // we keep on looking for other installed modules that may handle the class.

         LogF("~FindClass()","Attempting to load module \"%s\" for class $%.8x.", path, ClassID);

         OBJECTPTR module;
         if (!CreateObject(ID_MODULE, NF_UNTRACKED, &module,
               FID_Name|TSTR, path,
               TAGEND)) {

            if (!KeyGet(glClassMap, ClassID, (APTR *)&ptr, NULL)) {
               class = ptr[0];
            }

            acFree(module);  // Free the module object - the code and any classes it created will continue to remain in memory.
         }

         LogBack();
      }

      if (class) LogF("FindClass","Found class \"%s\"", class->ClassName);
      else LogF("@FindClass","Could not find class $%.8x in memory or in class references.", ClassID);

      return class;
   }
}

/*****************************************************************************

-FUNCTION-
FindObject: Searches for objects by name and class.
Category: Objects

The FindObject() function is used to search for objects in the system by name and class type.  If the function cannot
find any object with the name or class that you are looking for, then it will return an error code.  If it does find
one or more matching objects, then a complete list of them will be returned in an allocated memory block.

The List is sorted so that the oldest object is placed at the start of the list, and the most recent is placed at the
end.  This will assist you in situations where you may be looking for the oldest or youngest object that has the
particular name that you have searched for.

The List is terminated with a NULL entry.

The List is returned as an allocated memory block - please use ~FreeResource() to deallocate it.

-INPUT-
cstr Name: The name of the object that you are looking for.
cid ClassID: Setting this field to a class ID will filter the results down to a specific class type.
&!ptr(oid) List: On success, a memory block containing a list of ObjectID's will be placed in this variable.
&int Count: The total number of objects listed in the List array will be stored in this variable.

-ERRORS-
Okay:         One or more objects have been found and are listed.
Search:       There are no objects in the system with the given name or class type.
NullArgs:
Memory:       The function failed to allocate enough memory for the List array.
AccessMemory: The function failed to gain access to the PublicObjects structure (internal error).

*****************************************************************************/

ERROR FindObject(CSTRING InitialName, CLASSID ClassID, OBJECTID **List, LONG *ObjectCount)
{
   if ((!ObjectCount) OR (!List)) return LogError(ERH_FindObject, ERR_NullArgs);

   OBJECTID array[500];
   ERROR error;
   if (!(error = FastFindObject(InitialName, ClassID, array, ARRAYSIZE(array), ObjectCount))) {
      OBJECTID *alloc;
      if (!AllocMemory(sizeof(OBJECTID) * (ObjectCount[0] + 1), MEM_NO_CLEAR, (APTR *)&alloc, NULL)) {
         CopyMemory(array, alloc, sizeof(OBJECTID) * ObjectCount[0]);
         alloc[*ObjectCount] = 0; // Terminate the array
         *List = alloc;
         return ERR_Okay;
      }
      else return LogError(ERH_FindObject, ERR_AllocMemory);
   }
   else return error;
}

/*****************************************************************************

-FUNCTION-
FindPrivateObject: Searches for objects by name.
Category: Objects

The FindPrivateObject() function is provided as a simple implementation of the FastFindObject() function.  This
implementation is specifically limited to finding private objects by name only.  It is capable of returning only
one object address in its search results.  Unlike FastFindObject(), this function returns directly accessible
object addresses that may be used immediately after a successful search.

If multiple objects with the same name exists, the most recently created object will be returned by this function.

If you require more advanced functionality for object searches, please use the FastFindObject() function.

-INPUT-
cstr Name:   The name of the object to find.
&obj Object: A pointer to the discovered object will be returned in this parameter.

-ERRORS-
Okay: A matching object was found.
NullArgs
Search: No objects matching the given name could be found.
LockFailed
EmptyString

*****************************************************************************/

ERROR FindPrivateObject(CSTRING InitialName, OBJECTPTR *Object)
{
   if ((!InitialName) OR (!Object)) return LogError(ERH_FindPrivateObject, ERR_NullArgs);

   *Object = NULL;

   if (!*InitialName) return LogError(ERH_FindPrivateObject, ERR_EmptyString);

   // If an integer based name (defined by #num) is passed, we translate it to an ObjectID rather than searching for an
   // object of name "#1234".

   UBYTE number = FALSE;
   if (InitialName[0] IS '#') number = TRUE;
   else {
      // If the name consists entirely of numbers, it must be considered an object ID (we can make this check because
      // it is illegal for a name to consist entirely of figures).

      LONG i;
      for (i=0; InitialName[i]; i++) {
         if (((InitialName[i] < '0') OR (InitialName[i] > '9')) AND (InitialName[i] != '-')) break;
      }
      if (!InitialName[i]) number = TRUE;
   }

   if (number) {
      OBJECTID objectid;
      if ((objectid = (OBJECTID)StrToInt(InitialName))) {
         if (!thread_lock(TL_PRIVATE_MEM, 4000)) {
            LONG i;
            if ((i = find_private_mem_id(objectid, NULL)) != -1) {
               *Object = glPrivateMemory[i].Address;
               thread_unlock(TL_PRIVATE_MEM);
               return ERR_Okay;
            }
            else {
               thread_unlock(TL_PRIVATE_MEM);
               return ERR_Search;
            }
         }
         else return LogError(ERH_FindPrivateObject, ERR_LockFailed);
      }
      else return ERR_Search;
   }
   else if (!StrMatch("owner", InitialName)) {
      if ((tlContext != &glTopContext) AND (tlContext->Object->OwnerID)) {
         if (!thread_lock(TL_PRIVATE_MEM, 4000)) {
            LONG i;
            if ((i = find_private_mem_id(tlContext->Object->OwnerID, NULL)) != -1) {
               *Object = glPrivateMemory[i].Address;
               thread_unlock(TL_PRIVATE_MEM);
               return ERR_Okay;
            }
            else {
               thread_unlock(TL_PRIVATE_MEM);
               return ERR_Search;
            }
         }
         else return LogError(ERH_FindPrivateObject, ERR_LockFailed);
      }
      else return ERR_Search;
   }

   // Copy the name to our buffer in lower case

   char name[MAX_NAME_LEN+1];
   ULONG hash = 5381;
   {
      LONG i;
      for (i=0; (InitialName[i]) AND (i < MAX_NAME_LEN - 1); i++) {
         UBYTE c = InitialName[i];
         if ((c >= 'A') AND (c <= 'Z')) c = c - 'A' + 'a';
         name[i] = c;
         hash = ((hash<<5) + hash) + c;
      }
      name[i] = 0;
   }

   if (!thread_lock(TL_OBJECT_LOOKUP, 4000)) {
      LONG i, list_size;
      OBJECTPTR *list;
      if (!VarGet(glObjectLookup, InitialName, (APTR *)&list, &list_size)) {
         // Return the most recently created object, i.e. the one at the end of the list.
         for (i = (list_size / sizeof(OBJECTPTR)) - 1; i >= 0; i--) {
            if (list[i]) {
               *Object = list[i];
               break;
            }
         }
      }
      thread_unlock(TL_OBJECT_LOOKUP);
   }

   if (*Object) return ERR_Okay;
   else return ERR_Search;
}

/****************************************************************************

-FUNCTION-
GetFeedList: Private.  Retrieves the data feed subscriptions of an object.

Private

-INPUT-
obj Object: The object to query.

-RESULT-
mem: A memory ID that refers to the feed list is returned, or NULL if no subscriptions are present.

*****************************************************************************/

MEMORYID GetFeedList(OBJECTPTR Object)
{
   if ((Object) AND (Object->Stats)) {
      return Object->Stats->MID_FeedList;
   }
   else return 0;
}

/****************************************************************************

-FUNCTION-
GetClassID: Returns the class ID of an object.
Category: Objects

This function can be used on any valid object ID to retrieve the ID of its class.  This is the quickest way to
retrieve the class of an object without having to gain exclusive access to the object first.

Please note that if you already have access to an object through an address pointer, the quickest way to learn of its
class is to read the ClassID field in the object header.

-INPUT-
oid Object: The object to be examined.

-RESULT-
cid: Returns the class ID of the object or NULL if failure.

*****************************************************************************/

CLASSID GetClassID(OBJECTID ObjectID)
{
   if (!ObjectID) return 0;

   OBJECTPTR object;
   if (ObjectID < 0) {
      struct SharedObjectHeader *header;
      LONG id;
      if (!AccessMemory(RPM_SharedObjects, MEM_READ, 2000, (void **)&header)) {
         struct SharedObject *shared_obj = ResolveAddress(header, header->Offset);
         LONG pos;
         if (!find_public_object_entry(header, ObjectID, &pos)) {
            id = shared_obj[pos].ClassID;
         }
         else {
            id = 0;
            LogF("GetClassID()","Object #%d does not exist.", ObjectID);
         }
         ReleaseMemoryID(RPM_SharedObjects);
         return id;
      }
      else LogError(ERH_GetClassID, ERR_AccessMemory);
   }
   else if ((object = GetObjectPtr(ObjectID))) return object->ClassID;
   else LogF("GetClassID()","Failed to access private object #%d, no longer exists or ID invalid.", ObjectID);

   return 0;
}

/*****************************************************************************

-FUNCTION-
GetErrorMsg: Translates error codes into human readable strings.
Category: Logging

The GetErrorMsg() function converts error codes into human readable strings.  If the Code is invalid, a string of
"Unknown error code" is returned.

-INPUT-
error Error: The error code to lookup.

-RESULT-
cstr: A human readable string for the error code is returned.  By default error codes are returned in English, however if a translation table exists for the user's own language, the string will be translated.

*****************************************************************************/

CSTRING GetErrorMsg(ERROR Code)
{
   if ((Code < glTotalMessages) AND (Code > 0)) {
      return glMessages[Code];
   }
   else if (!Code) return "Operation successful.";
   else return "Unknown error code.";
}

/*****************************************************************************

-FUNCTION-
GenCRC32: Generates 32-bit CRC checksum values.

This function is used internally for the generation of 32-bit CRC checksums.  You may use it for your own purposes to
generate CRC values over a length of buffer space.  This function may be called repeatedly by feeding it previous
CRC values, making it ideal for processing streamed data.

-INPUT-
uint CRC: If streaming data to this function, this value should reflect the most recently returned CRC integer.  Otherwise set to zero.
ptr Data: The data to generate a CRC value for.
uint Length: The length of the Data buffer.

-RESULT-
uint: Returns the computed 32 bit CRC value for the given data.
-END-

*****************************************************************************/

#if 1

static const ULONG crc_table[256] = {
  0x00000000L, 0x77073096L, 0xee0e612cL, 0x990951baL, 0x076dc419L,
  0x706af48fL, 0xe963a535L, 0x9e6495a3L, 0x0edb8832L, 0x79dcb8a4L,
  0xe0d5e91eL, 0x97d2d988L, 0x09b64c2bL, 0x7eb17cbdL, 0xe7b82d07L,
  0x90bf1d91L, 0x1db71064L, 0x6ab020f2L, 0xf3b97148L, 0x84be41deL,
  0x1adad47dL, 0x6ddde4ebL, 0xf4d4b551L, 0x83d385c7L, 0x136c9856L,
  0x646ba8c0L, 0xfd62f97aL, 0x8a65c9ecL, 0x14015c4fL, 0x63066cd9L,
  0xfa0f3d63L, 0x8d080df5L, 0x3b6e20c8L, 0x4c69105eL, 0xd56041e4L,
  0xa2677172L, 0x3c03e4d1L, 0x4b04d447L, 0xd20d85fdL, 0xa50ab56bL,
  0x35b5a8faL, 0x42b2986cL, 0xdbbbc9d6L, 0xacbcf940L, 0x32d86ce3L,
  0x45df5c75L, 0xdcd60dcfL, 0xabd13d59L, 0x26d930acL, 0x51de003aL,
  0xc8d75180L, 0xbfd06116L, 0x21b4f4b5L, 0x56b3c423L, 0xcfba9599L,
  0xb8bda50fL, 0x2802b89eL, 0x5f058808L, 0xc60cd9b2L, 0xb10be924L,
  0x2f6f7c87L, 0x58684c11L, 0xc1611dabL, 0xb6662d3dL, 0x76dc4190L,
  0x01db7106L, 0x98d220bcL, 0xefd5102aL, 0x71b18589L, 0x06b6b51fL,
  0x9fbfe4a5L, 0xe8b8d433L, 0x7807c9a2L, 0x0f00f934L, 0x9609a88eL,
  0xe10e9818L, 0x7f6a0dbbL, 0x086d3d2dL, 0x91646c97L, 0xe6635c01L,
  0x6b6b51f4L, 0x1c6c6162L, 0x856530d8L, 0xf262004eL, 0x6c0695edL,
  0x1b01a57bL, 0x8208f4c1L, 0xf50fc457L, 0x65b0d9c6L, 0x12b7e950L,
  0x8bbeb8eaL, 0xfcb9887cL, 0x62dd1ddfL, 0x15da2d49L, 0x8cd37cf3L,
  0xfbd44c65L, 0x4db26158L, 0x3ab551ceL, 0xa3bc0074L, 0xd4bb30e2L,
  0x4adfa541L, 0x3dd895d7L, 0xa4d1c46dL, 0xd3d6f4fbL, 0x4369e96aL,
  0x346ed9fcL, 0xad678846L, 0xda60b8d0L, 0x44042d73L, 0x33031de5L,
  0xaa0a4c5fL, 0xdd0d7cc9L, 0x5005713cL, 0x270241aaL, 0xbe0b1010L,
  0xc90c2086L, 0x5768b525L, 0x206f85b3L, 0xb966d409L, 0xce61e49fL,
  0x5edef90eL, 0x29d9c998L, 0xb0d09822L, 0xc7d7a8b4L, 0x59b33d17L,
  0x2eb40d81L, 0xb7bd5c3bL, 0xc0ba6cadL, 0xedb88320L, 0x9abfb3b6L,
  0x03b6e20cL, 0x74b1d29aL, 0xead54739L, 0x9dd277afL, 0x04db2615L,
  0x73dc1683L, 0xe3630b12L, 0x94643b84L, 0x0d6d6a3eL, 0x7a6a5aa8L,
  0xe40ecf0bL, 0x9309ff9dL, 0x0a00ae27L, 0x7d079eb1L, 0xf00f9344L,
  0x8708a3d2L, 0x1e01f268L, 0x6906c2feL, 0xf762575dL, 0x806567cbL,
  0x196c3671L, 0x6e6b06e7L, 0xfed41b76L, 0x89d32be0L, 0x10da7a5aL,
  0x67dd4accL, 0xf9b9df6fL, 0x8ebeeff9L, 0x17b7be43L, 0x60b08ed5L,
  0xd6d6a3e8L, 0xa1d1937eL, 0x38d8c2c4L, 0x4fdff252L, 0xd1bb67f1L,
  0xa6bc5767L, 0x3fb506ddL, 0x48b2364bL, 0xd80d2bdaL, 0xaf0a1b4cL,
  0x36034af6L, 0x41047a60L, 0xdf60efc3L, 0xa867df55L, 0x316e8eefL,
  0x4669be79L, 0xcb61b38cL, 0xbc66831aL, 0x256fd2a0L, 0x5268e236L,
  0xcc0c7795L, 0xbb0b4703L, 0x220216b9L, 0x5505262fL, 0xc5ba3bbeL,
  0xb2bd0b28L, 0x2bb45a92L, 0x5cb36a04L, 0xc2d7ffa7L, 0xb5d0cf31L,
  0x2cd99e8bL, 0x5bdeae1dL, 0x9b64c2b0L, 0xec63f226L, 0x756aa39cL,
  0x026d930aL, 0x9c0906a9L, 0xeb0e363fL, 0x72076785L, 0x05005713L,
  0x95bf4a82L, 0xe2b87a14L, 0x7bb12baeL, 0x0cb61b38L, 0x92d28e9bL,
  0xe5d5be0dL, 0x7cdcefb7L, 0x0bdbdf21L, 0x86d3d2d4L, 0xf1d4e242L,
  0x68ddb3f8L, 0x1fda836eL, 0x81be16cdL, 0xf6b9265bL, 0x6fb077e1L,
  0x18b74777L, 0x88085ae6L, 0xff0f6a70L, 0x66063bcaL, 0x11010b5cL,
  0x8f659effL, 0xf862ae69L, 0x616bffd3L, 0x166ccf45L, 0xa00ae278L,
  0xd70dd2eeL, 0x4e048354L, 0x3903b3c2L, 0xa7672661L, 0xd06016f7L,
  0x4969474dL, 0x3e6e77dbL, 0xaed16a4aL, 0xd9d65adcL, 0x40df0b66L,
  0x37d83bf0L, 0xa9bcae53L, 0xdebb9ec5L, 0x47b2cf7fL, 0x30b5ffe9L,
  0xbdbdf21cL, 0xcabac28aL, 0x53b39330L, 0x24b4a3a6L, 0xbad03605L,
  0xcdd70693L, 0x54de5729L, 0x23d967bfL, 0xb3667a2eL, 0xc4614ab8L,
  0x5d681b02L, 0x2a6f2b94L, 0xb40bbe37L, 0xc30c8ea1L, 0x5a05df1bL,
  0x2d02ef8dL
};

#define DO1(buf) crc = crc_table[((int)crc ^ (*buf++)) & 0xff] ^ (crc >> 8);
#define DO2(buf)  DO1(buf); DO1(buf);
#define DO4(buf)  DO2(buf); DO2(buf);
#define DO8(buf)  DO4(buf); DO4(buf);

ULONG GenCRC32(ULONG crc, APTR Data, ULONG len)
{
   if (!Data) return 0;

   BYTE *buf = Data;
   crc = crc ^ 0xffffffff;
   while (len >= 8) {
      DO8(buf);
      len -= 8;
   }

   if (len) do {
      DO1(buf);
   } while (--len);

   return crc ^ 0xffffffff;
}

#else

// CRC calculation routine from Zlib, written by Rodney Brown <rbrown64@csc.com.au>

typedef ULONG u4;
typedef int ptrdiff_t;

#define REV(w) (((w)>>24)+(((w)>>8)&0xff00)+(((w)&0xff00)<<8)+(((w)&0xff)<<24))
static ULONG crc32_little(ULONG, const UBYTE *, unsigned);
static ULONG crc32_big(ULONG, const UBYTE *, unsigned);
static volatile int crc_table_empty = 1;
static ULONG crc_table[8][256];

#define DO1 crc = crc_table[0][((int)crc ^ (*buf++)) & 0xff] ^ (crc >> 8)
#define DO8 DO1; DO1; DO1; DO1; DO1; DO1; DO1; DO1
#define DOLIT4 c ^= *buf4++; \
        c = crc_table[3][c & 0xff] ^ crc_table[2][(c >> 8) & 0xff] ^ \
            crc_table[1][(c >> 16) & 0xff] ^ crc_table[0][c >> 24]
#define DOLIT32 DOLIT4; DOLIT4; DOLIT4; DOLIT4; DOLIT4; DOLIT4; DOLIT4; DOLIT4
#define DOBIG4 c ^= *++buf4; \
        c = crc_table[4][c & 0xff] ^ crc_table[5][(c >> 8) & 0xff] ^ \
            crc_table[6][(c >> 16) & 0xff] ^ crc_table[7][c >> 24]
#define DOBIG32 DOBIG4; DOBIG4; DOBIG4; DOBIG4; DOBIG4; DOBIG4; DOBIG4; DOBIG4

ULONG GenCRC32(ULONG crc, APTR Data, ULONG len)
{
   if (!Data) return 0;

   UBYTE *buf = Data;
   if (crc_table_empty) {
      ULONG c;
      LONG n, k;
      ULONG poly;
      // terms of polynomial defining this crc (except x^32):
      static const UBYTE p[] = {0,1,2,4,5,7,8,10,11,12,16,22,23,26};

      // make exclusive-or pattern from polynomial (0xedb88320UL)

      poly = 0;
      for (n = 0; n < sizeof(p)/sizeof(UBYTE); n++) {
         poly |= 1 << (31 - p[n]);
      }

      // generate a crc for every 8-bit value

      for (n = 0; n < 256; n++) {
         c = (ULONG)n;
         for (k = 0; k < 8; k++) {
            c = c & 1 ? poly ^ (c >> 1) : c >> 1;
         }
         crc_table[0][n] = c;
      }

      // Generate CRC for each value followed by one, two, and three zeros, and then the byte reversal of those as well
      // as the first table

      for (n = 0; n < 256; n++) {
         c = crc_table[0][n];
         crc_table[4][n] = REV(c);
         for (k = 1; k < 4; k++) {
            c = crc_table[0][c & 0xff] ^ (c >> 8);
            crc_table[k][n] = c;
            crc_table[k + 4][n] = REV(c);
         }
      }

      crc_table_empty = 0;
   }

   if (sizeof(void *) == sizeof(ptrdiff_t)) {
      u4 endian;
      endian = 1;
      if (*((UBYTE *)(&endian))) {
         return crc32_little(crc, buf, len);
      }
      else return crc32_big(crc, buf, len);
   }

   crc = crc ^ 0xffffffff;
   while (len >= 8) {
      DO8;
      len -= 8;
   }

   if (len) do {
      DO1;
   } while (--len);

   return crc ^ 0xffffffff;
}

static ULONG crc32_little(ULONG crc, const UBYTE *buf, unsigned len)
{
   register u4 c;
   register const u4 *buf4;

   c = (u4)crc;
   c = ~c;
   while (len && ((ptrdiff_t)buf & 3)) {
      c = crc_table[0][(c ^ *buf++) & 0xff] ^ (c >> 8);
      len--;
   }

   buf4 = (const u4 *)(const void *)buf;
   while (len >= 32) {
      DOLIT32;
      len -= 32;
   }
   while (len >= 4) {
      DOLIT4;
      len -= 4;
   }
   buf = (const UBYTE *)buf4;

   if (len) do {
      c = crc_table[0][(c ^ *buf++) & 0xff] ^ (c >> 8);
   } while (--len);
   c = ~c;
   return (ULONG)c;
}

static ULONG crc32_big(ULONG crc, const UBYTE *buf, unsigned len)
{
   register u4 c;
   register const u4 *buf4;

   c = REV((u4)crc);
   c = ~c;
   while (len && ((ptrdiff_t)buf & 3)) {
       c = crc_table[4][(c >> 24) ^ *buf++] ^ (c << 8);
       len--;
   }

   buf4 = (const u4 *)(const void *)buf;
   buf4--;
   while (len >= 32) {
       DOBIG32;
       len -= 32;
   }
   while (len >= 4) {
       DOBIG4;
       len -= 4;
   }
   buf4++;
   buf = (const UBYTE *)buf4;

   if (len) do {
       c = crc_table[4][(c >> 24) ^ *buf++] ^ (c << 8);
   } while (--len);
   c = ~c;
   return (ULONG)(REV(c));
}

#endif

/*****************************************************************************

-FUNCTION-
GetMsgPort: Returns the message port used for communication with an object.
Category: Messages

This function will return the ID of the message port used for communication with an object.  The message port is the
same as that used for general communication with the task and is valid for inter-process communication via
~SendMessage().

-INPUT-
oid Object: Reference to the object to be queried.

-RESULT-
int: The number of the message port is returned or 0 if failure occurs.  Failure will most likely occur if the ObjectID is invalid.

*****************************************************************************/

LONG GetMsgPort(OBJECTID ObjectID)
{
   FMSG("GetMsgPort()","Object: #%d", ObjectID);

   struct SharedObjectHeader *header;

   if (ObjectID > 0) return glTaskMessageMID;
   else if (!AccessMemory(RPM_SharedObjects, MEM_READ, 2000, (void **)&header)) {
      LONG pos;
      if (!find_public_object_entry(header, ObjectID, &pos)) {
         struct SharedObject *list = ResolveAddress(header, header->Offset);
         LONG msgport = list[pos].MessageMID;
         ReleaseMemoryID(RPM_SharedObjects);

         return msgport ? msgport : glTaskMessageMID;
      }
      else {
         ReleaseMemoryID(RPM_SharedObjects);
         return 0;
      }
   }
   else return 0;
}

/*****************************************************************************

-FUNCTION-
GetName: Retrieves object names.
Category: Objects

This function will return the name of the object referenced by the Object pointer. If the target object has not been
assigned a name, then you will receive a null-string.

-INPUT-
obj Object: Pointer to the object that you want to get the name of.

-RESULT-
cstr: A string containing the object name is returned.  If the object has no name or the parameter is invalid, a null-terminated string is returned.

*****************************************************************************/

CSTRING GetName(OBJECTPTR Object)
{
   if ((Object) AND (Object->Stats)) return Object->Stats->Name;
   else return "";
}

/*****************************************************************************

-FUNCTION-
GetObjectPtr: Returns the object address for any private object ID.
Category: Objects

This function converts private object ID's into their respective private address pointers.  This function will fail if
given a public object ID, or an ID that does not relate to a private object.

-INPUT-
oid Object: The ID of the object to lookup.

-RESULT-
obj: The address of the object is returned, or NULL if the ID does not relate to a private object.

*****************************************************************************/

OBJECTPTR GetObjectPtr(OBJECTID ObjectID)
{
   OBJECTPTR object = NULL;
   if (!thread_lock(TL_PRIVATE_MEM, 4000)) {
      LONG i;
      for (i=0; i < glNextPrivateAddress; i++) {
         if ((glPrivateMemory[i].Flags & MEM_OBJECT) AND (glPrivateMemory[i].Address)) {
            if (((OBJECTPTR)glPrivateMemory[i].Address)->UniqueID IS ObjectID) {
               object = glPrivateMemory[i].Address;
               break;
            }
         }
      }
      thread_unlock(TL_PRIVATE_MEM);
   }

   return object;
}

/*****************************************************************************

-FUNCTION-
GetOwnerID: Returns the unique ID of an object's owner.
Category: Objects

This function can be used on any valid object ID to retrieve the unique ID of its owner.  This is the quickest way to
retrieve the owner of an object without having to gain exclusive access to the object first.

Please note that if you already have access to an object through an address pointer, the quickest way to learn of its
owner is to read the OwnerID field in the object header.

-INPUT-
oid Object: The ID of the object that you want to examine.

-RESULT-
oid: Returns the ID of the object's owner.  If the object does not have a owner (i.e. if it is untracked) or if the ID that you provided is invalid, this function will return NULL.

*****************************************************************************/

OBJECTID GetOwnerID(OBJECTID ObjectID)
{
   OBJECTID ownerid = 0;
   if (ObjectID < 0) {
      struct SharedObjectHeader *header;
      if (!AccessMemory(RPM_SharedObjects, MEM_READ, 2000, (void **)&header)) {
         LONG pos;
         if (!find_public_object_entry(header, ObjectID, &pos)) {
            struct SharedObject *list = ResolveAddress(header, header->Offset);
            ownerid = list[pos].OwnerID;
         }
         ReleaseMemoryID(RPM_SharedObjects);
      }
   }
   else {
      if (!thread_lock(TL_PRIVATE_MEM, 4000)) {
         LONG pos;
         if ((pos = find_private_mem_id(ObjectID, NULL)) != -1) {
            ownerid = ((OBJECTPTR)glPrivateMemory[pos].Address)->OwnerID;
         }
         thread_unlock(TL_PRIVATE_MEM);
      }
   }
   return ownerid;
}

/*****************************************************************************

-FUNCTION-
GetResource: Retrieves miscellaneous resource identifiers.

The GetResource() function is used to retrieve miscellaneous resource ID's that are either global or local to your task.
To retrieve a resource you need to make a reference to it with the Resource parameter.  Currently the following resource
ID's are available:

<types lookup="RES" type="Resource">
<type name="CURRENT_MSG">Returns a Message structure if the program is currently processing a message - otherwise returns NULL.  This resource type is meaningful only during a ~ProcessMessages() call.</>
<type name="KEY_STATE">The state of keyboard qualifiers such as caps-lock and the shift keys are reflected here.  The relevant KQ flags are found in the Keyboard module.</>
<type name="LOG_LEVEL">The current level of log detail (larger numbers indicate more detail).</>
<type name="LOG_DEPTH">The current depth of log messages.</>
<type name="GLOBAL_INSTANCE">If a global instance is active, this resource holds the instance ID.  Otherwise the value is 0.</>
<type name="MAX_PROCESSES">The maximum number of processes that can be supported at any moment in time.</>
<type name="MESSAGE_QUEUE">Use this resource to retrieve the message queue ID of the current task.</>
<type name="OPEN_INFO">Undocumented.</>
<type name="PARENT_CONTEXT">Returns a pointer to the parent object of the current context.</>
<type name="PRIVILEGED">This is set to TRUE if the process has elevated privileges (such as superuser or administrative rights).</>
<type name="TOTAL_SHARED_MEMORY">The total amount of shared memory in use.</>
<type name="TASK_CONTROL">Undocumented.</>
<type name="TASK_LIST">Undocumented.</>
<type name="TOTAL_MEMORY">The total amount of installed RAM in bytes.</>
<type name="TOTAL_SWAP">The total amount of available swap space.</>
<type name="USER_ID">Undocumented.</>
</types>

-INPUT-
int(RES) Resource: The ID of the resource that you want to obtain.

-RESULT-
large: Returns the value of the resource that you have requested.  If the resource ID is not known by the Core, NULL is returned.
-END-

*****************************************************************************/

LARGE GetResource(LONG Resource)
{
#ifdef __linux__
   struct sysinfo sys;
#endif

   switch(Resource) {
      case RES_MESSAGE_QUEUE:   return glTaskMessageMID;
      case RES_SHARED_CONTROL:  return (MAXINT)glSharedControl;
      case RES_GLOBAL_INSTANCE: return glSharedControl->GlobalInstance;
      case RES_PRIVILEGED:      return glPrivileged;
      case RES_PARENT_CONTEXT:  if (tlContext->Stack) return (MAXINT)tlContext->Stack->Object;
                                else return NULL;
      case RES_KEY_STATE:       return glKeyState;
      case RES_LOG_LEVEL:       return glLogLevel;
      case RES_SHARED_BLOCKS:   return (MAXINT)glSharedBlocks;
      case RES_TASK_CONTROL:    return (MAXINT)glTaskEntry;
      case RES_TASK_LIST:       return (MAXINT)shTasks;
      case RES_PROCESS_STATE:   return (MAXINT)glTaskState;
      case RES_MAX_PROCESSES:   return MAX_TASKS;
      case RES_LOG_DEPTH:       return tlDepth;
      case RES_CURRENT_MSG:     return (MAXINT)tlCurrentMsg;
      case RES_OPEN_INFO:       return (MAXINT)glOpenInfo;
      case RES_JNI_ENV:         return (MAXINT)glJNIEnv;
      case RES_THREAD_ID:       return (MAXINT)get_thread_id();
      case RES_CORE_IDL:        return (MAXINT)glIDL;

#ifdef __linux__
      // NB: This value is not cached.  Although unlikely, it is feasible that the total amount of physical RAM could
      // change during runtime.

      case RES_TOTAL_MEMORY: {
         if (!sysinfo(&sys)) return (LARGE)sys.totalram * (LARGE)sys.mem_unit;
         else return -1;
      }

      case RES_FREE_MEMORY: {
   #if 0
         // Unfortunately sysinfo() does not report on cached ram, which can be significant
         if (!sysinfo(&sys)) return (LARGE)(sys.freeram + sys.bufferram) * (LARGE)sys.mem_unit; // Buffer RAM is considered as 'free'
   #else
         UBYTE str[2048];
         LONG result;
         LARGE freemem = 0;
         if (!pReadFile("/proc/meminfo", str, sizeof(str)-1, &result)) {
            LONG i = 0;
            while (i < result) {
               if (!StrCompare("Cached", str+i, sizeof("Cached")-1, NULL)) {
                  freemem += (LARGE)StrToInt(str+i) * 1024LL;
               }
               else if (!StrCompare("Buffers", str+i, sizeof("Buffers")-1, NULL)) {
                  freemem += (LARGE)StrToInt(str+i) * 1024LL;
               }
               else if (!StrCompare("MemFree", str+i, sizeof("MemFree")-1, NULL)) {
                  freemem += (LARGE)StrToInt(str+i) * 1024LL;
               }

               while ((i < result) AND (str[i] != '\n')) i++;
               i++;
            }
         }

         return freemem;
   #endif
      }

      case RES_TOTAL_SHARED_MEMORY:
         if (!sysinfo(&sys)) return (LARGE)sys.sharedram * (LARGE)sys.mem_unit;
         else return -1;

      case RES_TOTAL_SWAP:
         if (!sysinfo(&sys)) return (LARGE)sys.totalswap * (LARGE)sys.mem_unit;
         else return -1;

      case RES_FREE_SWAP:
         if (!sysinfo(&sys)) return (LARGE)sys.freeswap * (LARGE)sys.mem_unit;
         else return -1;

      case RES_CPU_SPEED: {
         OBJECTPTR file;
         CSTRING line;
         static LONG cpu_mhz = 0;

         if (cpu_mhz) return cpu_mhz;

         if (!CreateObject(ID_FILE, 0, &file,
               FID_Path|TSTR,   "drive1:proc/cpuinfo",
               FID_Flags|TLONG, FL_READ|FL_BUFFER,
               TAGEND)) {

            while ((line = flReadLine(file))) {
               if (StrCompare("cpu Mhz", line, sizeof("cpu Mhz")-1, NULL) IS ERR_Okay) {
                  cpu_mhz = StrToInt(line);
               }
            }
            acFree(file);
         }

         return cpu_mhz;
      }
#elif __APPLE__
#warning TODO: Support for sysctlbyname()
#endif

      default: //LogF("@GetResource()","Unsupported resource ID %d.", Resource);
         break;
   }

   return NULL;
}
/*****************************************************************************

-FUNCTION-
GetSystemState: Returns miscellaneous data values from the Core.

The GetSystemState() function is used to retrieve miscellaneous resource and environment values, such as resource
paths, the Core's version number and the name of the host platform.

-RESULT-
cstruct(*SystemState): A read-only SystemState structure is returned.

*****************************************************************************/

const struct SystemState * GetSystemState(void)
{
   static LONG initialised = FALSE;
   static struct SystemState state;

   if (!initialised) {
      initialised = TRUE;

      state.ConsoleFD     = glConsoleFD;
      state.CoreVersion   = VER_CORE;
      state.CoreRevision  = REV_CORE;
      state.InstanceID    = glInstanceID;
      state.ErrorMessages = glMessages;
      state.ErrorHeaders  = glHeaders;
      state.TotalErrorMessages = ARRAYSIZE(glMessages);
      state.TotalErrorHeaders  = ARRAYSIZE(glHeaders);
      state.RootPath   = glRootPath;
      state.SystemPath = glSystemPath;
      state.ModulePath = glModulePath;
      #ifdef __unix__
         if (glFullOS) state.Platform = "Native";
         else state.Platform = "Linux";
      #elif _WIN32
         state.Platform = "Windows";
      #elif __APPLE__
         state.Platform = "OSX";
      #else
         state.Platform = "Unknown";
      #endif
   }

   state.Stage = glSharedControl->SystemState;
   return &state;
}

/*****************************************************************************

-FUNCTION-
ListChildren: Returns a list of all children belonging to an object.
Category: Objects

The ListChildren() function returns a list of an object's children in a single function call.

The client must provide an empty array of ChildEntry structures for the function to write its results.  The Count
argument must point to a LONG value that indicates the size of the array that you have supplied.  Before returning, the
ListChildren() function will update the Count variable so that it reflects the total number of children that were
written to the array.

Objects that are specially marked with the CHILD flag are not returned in the resulting list; such objects are
considered to be private extensions of the targeted parent.

-INPUT-
oid Object: The ID of the object that you wish to examine.
buf(array(resource(ChildEntry))) List: Must refer to an array of ChildEntry structures.
&arraysize Count:  The integer that this argument refers to must be set with a value that indicates the size of the supplied list array.  Before returning, this argument will be updated with the total number of entries listed in the array.

-ERRORS-
Okay: Zero or more children were found and listed.
Args
NullArgs

*****************************************************************************/

ERROR ListChildren(OBJECTID ObjectID, struct ChildEntry *List, LONG *Count)
{
   if ((!ObjectID) OR (!List) OR (!Count)) return LogError(ERH_ListChildren, ERR_NullArgs);
   if ((*Count < 0) OR (*Count > 3000)) return LogError(ERH_ListChildren, ERR_Args);

   ERROR error = ERR_Okay;
   LONG i = 0;

   // Build the list of public objects
   // TODO: Optimisation.  Most objects also won't have public children, making this redundant.

   struct SharedObjectHeader *header;
   if (!AccessMemory(RPM_SharedObjects, MEM_READ, 2000, (void **)&header)) {
      LONG j;
      struct SharedObject *list = ResolveAddress(header, header->Offset);
      for (j=0; j < header->NextEntry; j++) {
         if ((list[j].OwnerID IS ObjectID) AND (!(list[j].Flags & NF_INTEGRAL))) {
            List[i].ObjectID = list[j].ObjectID;
            List[i].ClassID  = list[j].ClassID;
            i++;
            if (i >= *Count) break;
         }
      }
      ReleaseMemoryID(RPM_SharedObjects);
   }

   // Build the list of private objects
   // TODO: Optimisation; all private addresses are being scanned.

   if (i < *Count) {
      if (!thread_lock(TL_PRIVATE_MEM, 4000)) {
         LONG j;
         for (j=0; j < glNextPrivateAddress; j++) {
            if ((glPrivateMemory[j].Flags & MEM_OBJECT) AND (glPrivateMemory[j].ObjectID IS ObjectID)) {
               OBJECTPTR object;
               if ((object = glPrivateMemory[j].Address)) {
                  if (!(object->Flags & NF_INTEGRAL)) {
                     List[i].ObjectID = object->UniqueID;
                     List[i].ClassID  = object->ClassID;
                     i++;
                     if (i >= *Count) break;
                  }
               }
            }
         }
         thread_unlock(TL_PRIVATE_MEM);
      }
   }

   *Count = i;
   return error;
}

/*****************************************************************************

-FUNCTION-
ListTasks: Returns a list of all active processes that are in the system.
Status: private

Limited to use by the desktop and some other internal programs.

-INPUT-
int(LTF) Flags: Optional flags.
&struct(ListTasks) List: A reference to the process list is returned in this parameter.

-ERRORS-
Okay
NullArgs

*****************************************************************************/

ERROR ListTasks(LONG Flags, struct ListTasks **Detail)
{
   if (!Detail) return ERR_NullArgs;

   if (!LOCK_PROCESS_TABLE(4000)) {
      WORD taskcount = 0;
      LONG memlocks = 0;

      struct ListTasks *list;
      LONG j, i, k;
      for (i=0; i < MAX_TASKS; i++) {
         if ((shTasks[i].ProcessID) AND (shTasks[i].TaskID) AND (shTasks[i].MessageID)) {
            if (Flags & LTF_CURRENT_PROCESS) {
               if (shTasks[i].TaskID != glCurrentTaskID) continue;
            }

            taskcount++;
            for (j=0; j < ARRAYSIZE(shTasks[i].NoBlockLocks); j++) {
                if (shTasks[i].NoBlockLocks[j].MemoryID) memlocks++;
            }
         }
      }

      if (!AllocMemory((sizeof(struct ListTasks) * (taskcount + 1)) + (sizeof(shTasks[0].NoBlockLocks[0]) * memlocks), MEM_NO_CLEAR, (void **)&list, NULL)) {
         *Detail = list;

         j = 0;
         for (i=0; (i < MAX_TASKS) AND (j < taskcount); i++) {
            if ((shTasks[i].ProcessID) AND (shTasks[i].TaskID) AND (shTasks[i].MessageID)) {
               if (Flags & LTF_CURRENT_PROCESS) {
                  if (shTasks[i].TaskID != glCurrentTaskID) continue;
               }

               list->ProcessID   = shTasks[i].ProcessID;
               list->TaskID      = shTasks[i].TaskID;
               list->MessageID   = shTasks[i].MessageID;
               list->OutputID    = shTasks[i].OutputID;
               list->InstanceID  = shTasks[i].InstanceID;
               list->ModalID     = shTasks[i].ModalID;
               list->MemoryLocks = (APTR)(list + 1);

               memlocks = 0; // Insert memory locks for this task entry
               for (k=0; k < ARRAYSIZE(shTasks[i].NoBlockLocks); k++) {
                  if (shTasks[i].NoBlockLocks[k].MemoryID) {
                     list->MemoryLocks[memlocks].MemoryID = shTasks[i].NoBlockLocks[k].MemoryID;
                     list->MemoryLocks[memlocks].Locks = shTasks[i].NoBlockLocks[k].AccessCount;
                  }
               }
               list->TotalMemoryLocks = memlocks;

               if (!memlocks) list->MemoryLocks = NULL;

               // Next task

               list = (struct ListTasks *) ( ((BYTE *)(list+1)) + (memlocks * sizeof(shTasks[0].NoBlockLocks[0])) );
               j++;
            }
         }

         ClearMemory(list, sizeof(struct ListTasks));

         UNLOCK_PROCESS_TABLE();
         return ERR_Okay;
      }
      else {
         UNLOCK_PROCESS_TABLE();
         return ERR_AllocMemory;
      }
   }
   else return ERR_SystemLocked;
}

/*****************************************************************************

-FUNCTION-
RandomNumber: Generates random numbers.

This function generates a random number as quickly as possible.  In some cases it will use various hardware attributes
in order to create guaranteed random numbers.  The routine uses one divide to determine the range and will
automatically change the random seed value each time you call it.  Remember that all generated numbers fall one value
below the Range that you specify. Add 1 to your Range if maximum value is inclusive.

The unique seed used by the PRNG is generated when the Core is opened for the first time.  You can change the seed
by calling ~SetResource() with the RES_RANDOM_SEED option.

-INPUT-
int Range: A range between 1 and 2,147,483,648.  An invalid value will result in 0 being returned.

-RESULT-
int: Returns a number greater or equal to 0, and <i>less than</i> Range.

*****************************************************************************/

LONG RandomNumber(LONG Range)
{
   if (Range <= 0) return 0;

   if (Range > 32768) {
      #ifdef __unix__
         return ((random() & 0xffff) | (rand()<<16)) % Range;
      #else
         return ((rand() & 0xffff) | (rand()<<16)) % Range;
      #endif
   }
   else {
      #ifdef __unix__
         return random() % Range;
      #else
         return rand() % Range;
      #endif
   }
}

/*****************************************************************************

-FUNCTION-
RegisterFD: Registers a file descriptor for monitoring when the task is asleep.

This function will register a file descriptor that will be monitored for activity when the task is sleeping.  If
activity occurs on the descriptor then the function specified in the Routine parameter will be called.  The routine
must read all of information from the descriptor, as the running process will not be able to sleep until all the
data is cleared.

The file descriptor should be configured as non-blocking before registration.  Blocking descriptors may cause your
program to hang if not handled carefully.

File descriptors support read and write states simultaneously and a callback routine can be applied to either state.
Set the RFD_READ flag to apply the Routine to the read callback and RFD_WRITE for the write callback.  If neither
flag is specified, RFD_READ is assumed.  A file descriptor may have up to 1 subscription per flag, for example a read
callback can be registered, followed by a write callback in a second call. Individual callbacks can be removed by
combining the read/write flags with RFD_REMOVE.

The capabilities of this function and FD handling in general is developed to suit the host platform. On POSIX
compliant systems, standard file descriptors are used.  In Microsoft Windows, object handles are used and blocking
restrictions do not apply, except to sockets.

Call the DeregisterFD() macro to simplify unsubscribing once the file descriptor is no longer needed or is destroyed.

-INPUT-
hhandle FD: The file descriptor that is to be watched.
int(RFD) Flags: Set to one or more of the flags RFD_READ, RFD_WRITE, RFD_EXCEPT, RFD_REMOVE.
fptr(void hhandle ptr) Routine: The routine that will read from the descriptor when data is detected on it.  The template for the function is "void Routine(LONG FD, APTR Data)".
ptr Data: User specific data pointer that will be passed to the Routine.  Separate data pointers apply to the read and write states of operation.

-ERRORS-
Okay: The FD was successfully registered.
Args: The FD was set to a value of -1.
ArrayFull: The maximum number of registrable file descriptors has been reached.
NoSupport: The host platform does not support file descriptors.
-END-

*****************************************************************************/

#ifdef _WIN32
ERROR RegisterFD(HOSTHANDLE FD, LONG Flags, void (*Routine)(HOSTHANDLE, APTR), APTR Data)
#else
ERROR RegisterFD(LONG FD, LONG Flags, void (*Routine)(HOSTHANDLE, APTR), APTR Data)
#endif
{
   UBYTE i;

#ifdef _WIN32
   if (FD IS (HOSTHANDLE)-1) return LogError(ERH_RegisterFD, ERR_Args);
   if (Flags & RFD_SOCKET) return LogError(ERH_RegisterFD, ERR_NoSupport); // In MS Windows, socket handles are managed as window messages (see Network module's Windows code)
#else
   if (FD IS -1) return LogError(ERH_RegisterFD, ERR_Args);
#endif

   if (glTotalFDs >= MAX_FDS) return LogError(ERH_RegisterFD, ERR_ArrayFull);

   if (!glFDTable) {
      if (!(glFDTable = malloc(sizeof(struct FDTable) * MAX_FDS))) return ERR_AllocMemory;
   }

   if (Flags & RFD_REMOVE) {
      if (!(Flags & (RFD_READ|RFD_WRITE|RFD_EXCEPT))) Flags |= RFD_READ|RFD_WRITE|RFD_EXCEPT;

      for (i=0; i < glTotalFDs; i++) {
         if ((glFDTable[i].FD IS FD) AND ((glFDTable[i].Flags & (RFD_READ|RFD_WRITE|RFD_EXCEPT)) & Flags)) {
            // If the routine address was specified with the remove option, the routine must match.

            if ((Routine) AND (glFDTable[i].Routine != Routine)) continue;

            if (i+1 < glTotalFDs) {
               CopyMemory(glFDTable+i+1, glFDTable+i, sizeof(struct FDTable) * (glTotalFDs-i-1));
            }

            glTotalFDs--;
            i = -1; // Repeat loop to catch multiple FD registrations
         }
      }
      return ERR_Okay;
   }

   if (!(Flags & (RFD_READ|RFD_WRITE|RFD_EXCEPT|RFD_REMOVE))) Flags |= RFD_READ;

   for (i=0; i < glTotalFDs; i++) {
      if ((glFDTable[i].FD IS FD) AND (Flags & (glFDTable[i].Flags & (RFD_READ|RFD_WRITE|RFD_EXCEPT)))) break;
   }

   if (i >= MAX_FDS) return LogError(ERH_RegisterFD, ERR_ArrayFull);

   if (i IS glTotalFDs) LogF("3RegisterFD()","FD: %d, Routine: %p, Flags: $%.2x (New)", (LONG)FD, Routine, Flags);

#ifdef _WIN32
   // Nothing to do for Win32
#else
   if (!Routine) fcntl(FD, F_SETFL, fcntl(FD, F_GETFL) | O_NONBLOCK); // Ensure that the FD is non-blocking
#endif

   glFDTable[i].FD      = FD;
   glFDTable[i].Routine = Routine;
   glFDTable[i].Data    = Data;
   glFDTable[i].Flags   = Flags;
   if (i >= glTotalFDs) glTotalFDs++;

   return ERR_Okay;
}

/*****************************************************************************

-FUNCTION-
SelfDestruct: Destroys the process and frees its resources.

This function will immediately free all resources allocated by the process and then exit.  This is the cleanest way
for a process to destroy itself when a normal exit procedure is not possible.

This function will not return.

*****************************************************************************/

extern void CloseCore(void);

void SelfDestruct(void)
{
   LogF("SelfDestruct()","This process will self-destruct.");

   CloseCore();
   exit(0);

//#ifdef __unix__
//      kill(getpid(), SIGKILL);
//#endif
}

/*****************************************************************************

-FUNCTION-
SetOwner: Changes object ownership dynamically.
Category: Objects

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

*****************************************************************************/

ERROR SetOwner(OBJECTPTR Object, OBJECTPTR Owner)
{
   if ((!Object) OR (!Owner)) return LogError(ERH_SetOwner, ERR_NullArgs);

   if (Object->OwnerID IS Owner->UniqueID) return ERR_Okay;

   if (((struct rkMetaClass *)Object->Class)->Flags & CLF_NO_OWNERSHIP) {
      FMSG("@SetOwner()","Cannot set the object owner as CLF_NO_OWNERSHIP is set in its class.");
      return ERR_Okay;
   }

   if (Object IS Owner) {
      LogF("@SetOwner()","Illegal attempt to set an object owner to loop back to itself (%p).", Object);
      return ERR_Args;
   }

   // Send a child alert to the owner.  If the owner sends back an error, then we return immediately.

   prv_access(Object);
   if (!CheckAction(Owner, AC_NewChild)) {
      ERROR error;
      struct acNewChild newchild;
      newchild.NewChildID = Object->UniqueID;
      if ((error = Action(AC_NewChild, Owner, &newchild)) != ERR_NoSupport) {
         if (error != ERR_Okay) { // If the owner has passed the object through to another owner, return ERR_Okay, otherwise error.
            prv_release(Object);
            if (error IS ERR_OwnerPassThrough) return ERR_Okay;
            else return error;
         }
      }
   }

   struct acNewOwner newowner = {
      .NewOwnerID = Owner->UniqueID, // Send a owner alert to the object
      .ClassID    = Owner->ClassID
   };
   Action(AC_NewOwner, Object, &newowner);

   // Make the change

   //if (Object->OwnerID) FMSG("SetOwner:","Changing the owner for object %d from %d to %d.", Object->UniqueID, Object->OwnerID, Owner->UniqueID);

   if (Object->Flags & NF_FOREIGN_OWNER) {
      OBJECTPTR obj;
      if (!AccessObject(Object->OwnerID, 3000, &obj)) {
         OBJECTPTR context = SetContext(Object);
         UnsubscribeAction(obj, AC_OwnerDestroyed);
         SetContext(context);
         ReleaseObject(obj);
      }
   }

   Object->OwnerID = Owner->UniqueID;

   if (Object->UniqueID < 0) {
      struct SharedObjectHeader *header;
      if (!AccessMemory(RPM_SharedObjects, MEM_READ, 2000, (void **)&header)) {
         LONG pos;
         if (!find_public_object_entry(header, Object->UniqueID, &pos)) {
            struct SharedObject *list = ResolveAddress(header, header->Offset);
            list[pos].OwnerID = Owner->UniqueID;
         }
         ReleaseMemoryID(RPM_SharedObjects);
      }

      // Track the object's memory header to the new owner

      if (!LOCK_PUBLIC_MEMORY(4000)) {
         LONG i;
         if ((i = find_public_address(glSharedControl, Object)) != -1) {
            glSharedBlocks[i].ObjectID = Owner->UniqueID;
         }
         UNLOCK_PUBLIC_MEMORY();
      }
   }
   else {
      // Track the object's memory header to the new owner

      if (!thread_lock(TL_PRIVATE_MEM, 4000)) {
         LONG pos;
         if ((pos = find_private_mem_id(Object->UniqueID, Object)) != -1) {
            glPrivateMemory[pos].ObjectID = Owner->UniqueID;
         }
         else LogF("@SetOwner:","Failed to find private object %p / #%d.", Object, Object->UniqueID);
         thread_unlock(TL_PRIVATE_MEM);
      }

      // If the owner is public and belongs to another task, subscribe to the FreeResources action so
      // that we can receive notification when the owner is destroyed.

      if ((Owner->UniqueID < 0) AND (Owner->TaskID) AND (Owner->TaskID != glCurrentTaskID) AND (Owner->TaskID != SystemTaskID)) {
         LogF("SetOwner:","Owner %d is in task %d, will monitor for termination.", Owner->UniqueID, Owner->TaskID);
         OBJECTPTR context = SetContext(Object);
         SubscribeAction(Owner, AC_OwnerDestroyed);
         SetContext(context);
         Object->Flags |= NF_FOREIGN_OWNER;
      }
   }

   prv_release(Object);
   return ERR_Okay;
}

/*****************************************************************************

-FUNCTION-
SetContext: Assign the ownership of new resources to an object.
Category: Objects

This function provides a means for instructing the system which object has control of the current thread.  Once
called, all further resource allocations will be assigned to the supplied object.  This is particularly important
for memory and object handling. For example:

<pre>
acInit(display);
prev_context = SetContext(display);

   NewObject(ID_BITMAP, 0, &bitmap);
   AllocMemory(1000, MEM_DATA, &memory, NULL);;

SetContext(prev_context);
acFree(display);
</pre>

The above code allocates a Bitmap and a memory block, both of which will be contained by the display. When the #Free()
action is called, both the bitmap and memory block will be automatically removed as they have a dependency on the
display's existence.  Please keep in mind that the following is incorrect:

<pre>
acInit(display);
prev_context = SetContext(display);

   NewObject(ID_BITMAP, 0, &bitmap);
   AllocMemory(1000, MEM_DATA, &memory, NULL);

SetContext(prev_context);
acFree(display);
acFree(bitmap);
FreeResource(memory);
</pre>

Freeing the bitmap and memory block after the display would be invalid.  They can however be removed before the display
is terminated, if necessary.

SetContext() is intended for use by modules and classes.  Do not use it unless conditions necessitate its use.  The
Core will automatically set the correct context when calling any action or method of a class, as do the field
management functions.

-INPUT-
obj Object: Pointer to the object that will take on the new context.  If NULL, no change to the context will be made.

-RESULT-
obj: Returns a pointer to the previous context.  Because contexts will nest, you need to call SetContext() a second time with the returned pointer, in order to keep the Task stable.

*****************************************************************************/

OBJECTPTR SetContext(OBJECTPTR Object)
{
   if (Object) {
      OBJECTPTR old = tlContext->Object;
      tlContext->Object = Object;
      return old;
   }
   else return tlContext->Object;
}

/*****************************************************************************

-FUNCTION-
SetName: Sets the name of an object.
Category: Objects

To set the name of an object, use this function.  Please note that the length of the Name will be limited to the value
indicated in the "main.h" include file, under the MAX_NAME_LEN definition.  If the Name is longer than the allowed
length, it will be trimmed to fit.

Object names are limited to alpha-numeric characters and the underscore symbol.  Invalid characters will be skipped
while setting the object name.

-INPUT-
obj Object: Pointer to the object that you want to set the name of.
cstr Name: The name that you want to set for the object.

-ERRORS-
Okay:
NullArgs:
Search:       The Object is not recognised by the system - the address may be invalid.
AccessMemory: The function could not gain access to the shared objects table (internal error).

*****************************************************************************/

ERROR SetName(OBJECTPTR Object, CSTRING String)
{
   struct SharedObjectHeader *header;
   LONG i, pos, c;

   if ((!Object) OR (!String)) return LogError(ERH_SetName, ERR_NullArgs);

   prv_access(Object);

   // Remove any existing name first.

   if ((Object->Stats->Name[0]) AND (Object->UniqueID > 0)) {
      if (!thread_lock(TL_OBJECT_LOOKUP, 4000)) {
         remove_object_hash(Object);
         thread_unlock(TL_OBJECT_LOOKUP);
      }
   }

   BYTE illegal = FALSE;
   for (i=0; ((c = String[i])) AND (i < (MAX_NAME_LEN-1)); i++) {
      if ((c >= 'A') AND (c <= 'Z')) {
         c = c - 'A' + 'a';
      }
      else if (((c >= 'a') AND (c <= 'z')) OR ((c >= '0') AND (c <= '9')) OR ((c IS '_'))) {
      }
      else {
         // Anything that is not alphanumeric is not permitted in the object name.
         if (!illegal) {
            illegal = TRUE;
            LogF("@SetName","Illegal character '%c' in proposed name '%s'", c, String);
            c = '_';
         }
      }

      Object->Stats->Name[i] = c;
   }
   Object->Stats->Name[i] = 0;

   if (Object->UniqueID >= 0) {
      if (Object->Stats->Name[0]) {
         if (!thread_lock(TL_OBJECT_LOOKUP, 4000)) {
            OBJECTPTR *list;
            LONG list_size;
            if (!VarGet(glObjectLookup, Object->Stats->Name, (APTR *)&list, &list_size)) {
               list_size = list_size / sizeof(OBJECTPTR);
               OBJECTPTR new_list[list_size + 1];
               LONG j = 0;
               for (i=0; i < list_size; i++) {
                  if (list[i]) new_list[j++] = list[i];
               }
               new_list[j++] = Object;

               VarSet(glObjectLookup, Object->Stats->Name, &new_list, sizeof(OBJECTPTR) * j);
            }
            else VarSet(glObjectLookup, Object->Stats->Name, &Object, sizeof(OBJECTPTR));

            thread_unlock(TL_OBJECT_LOOKUP);
         }
      }
      prv_release(Object);
      return ERR_Okay;
   }
   else if (!AccessMemory(RPM_SharedObjects, MEM_READ_WRITE, 2000, (void **)&header)) {
      if (!find_public_object_entry(header, Object->UniqueID, &pos)) {
         struct SharedObject *list = ResolveAddress(header, header->Offset);
         for (i=0; (Object->Stats->Name[i]) AND (i < (MAX_NAME_LEN-1)); i++) {
            list[pos].Name[i] = Object->Stats->Name[i];
         }
         list[pos].Name[i] = 0;
         ReleaseMemoryID(RPM_SharedObjects);
         prv_release(Object);
         return ERR_Okay;
      }
      else{
         ReleaseMemoryID(RPM_SharedObjects);
         prv_release(Object);
         return LogError(ERH_SetName, ERR_Search);
      }
   }
   else {
      prv_release(Object);
      return LogError(ERH_SetName, ERR_AccessMemory);
   }
}

/*****************************************************************************

-FUNCTION-
SetResourcePath: Redefines the location of a system resource path.

The SetResourcePath() function changes the default locations of the Core's resource paths.

To read a resource path, use the ~GetSystemState() function.

-INPUT-
int(RP) PathType: The ID of the resource path to set.
cstr Path: The new location to set for the resource path.

-ERRORS-
Okay:
NullArgs:

*****************************************************************************/

ERROR SetResourcePath(LONG PathType, CSTRING Path)
{
   if (!PathType) return ERR_NullArgs;

   switch(PathType) {
      case RP_ROOT_PATH:
         if (Path) {
            WORD i;
            for (i=0; (Path[i]) AND (i < sizeof(glRootPath)-2); i++) glRootPath[i] = Path[i];
            if ((glRootPath[i-1] != '/') AND (glRootPath[i-1] != '\\')) {
               #ifdef _WIN32
                  glRootPath[i++] = '\\';
               #else
                  glRootPath[i++] = '/';
               #endif
            }
            glRootPath[i] = 0;
         }
         return ERR_Okay;

      case RP_SYSTEM_PATH:
         if (Path) {
            WORD i;
            for (i=0; (Path[i]) AND (i < sizeof(glSystemPath)-2); i++) glSystemPath[i] = Path[i];
            if ((glSystemPath[i-1] != '/') AND (glSystemPath[i-1] != '\\')) {
               #ifdef _WIN32
                  glSystemPath[i++] = '\\';
               #else
                  glSystemPath[i++] = '/';
               #endif
            }
            glSystemPath[i] = 0;
         }
         return ERR_Okay;

      case RP_MODULE_PATH: // An alternative path to the system modules.  This was introduced for Android, which holds the module binaries in the assets folders.
         if (Path) {
            WORD i;
            for (i=0; (Path[i]) AND (i < sizeof(glModulePath)-2); i++) glModulePath[i] = Path[i];
            if ((glModulePath[i-1] != '/') AND (glModulePath[i-1] != '\\')) {
               #ifdef _WIN32
                  glModulePath[i++] = '\\';
               #else
                  glModulePath[i++] = '/';
               #endif
            }
            glModulePath[i] = 0;
         }
         return ERR_Okay;

      default:
         return ERR_Args;
   }
}

/*****************************************************************************

-FUNCTION-
SetResource: Sets miscellaneous resource identifiers.

The SetResource() function is used to set miscellaneous resource ID's that are either global or local to your task.
To set a resource you need to make a reference to it with the Resource parameter and provide a new setting in the
Value parameter.  Currently the following resource ID's are available:

<types prefix="RES" type="Resource">
<type name="ALLOC_MEM_LIMIT">Adjusts the memory limit imposed on AllocMemory().  The Value specifies the memory limit in bytes.</>
<type name="LOG_LEVEL">Adjusts the current debug level.  The Value must be between 0 and 9, where 1 is the lowest level of debug output (errors only) and 0 is off.</>
<type name="PRIVILEGED_USER">If the Value is set to 1, this resource option puts the process in privileged mode (typically this enables full administrator rights).  This feature will only work for Unix processes that are granted admin rights when launched.  Setting the Value to 0 reverts to the user's permission settings.  SetResource() will return an error code indicating the level of success.</>
<type name="RANDOM_SEED">Sets the PRNG seed to the number indicated in Value.</>
</>

-INPUT-
int(RES) Resource: The ID of the resource to be set.
large Value:    The new value to set for the resource.

-RESULT-
large: Returns the previous value used for the resource that you have set.  If the resource ID that you provide is invalid, NULL is returned.
-END-

*****************************************************************************/

LARGE SetResource(LONG Resource, LARGE Value)
{
#ifdef __unix__
   static WORD privileged = 0;
#endif

   LARGE oldvalue = 0;

   switch(Resource) {
      case RES_CONSOLE_FD: glConsoleFD = (HOSTHANDLE)(MAXINT)Value; break;

      case RES_RANDOM_SEED: srand(Value); break;

      case RES_KEY_STATE: glKeyState = (LONG)Value; break;

      case RES_EXCEPTION_HANDLER:
         // Note: You can set your own crash handler, or set a value of NULL - this resets the existing handler which is useful if an external DLL function is suspected to have changed the filter.

         #ifdef _WIN32
            winSetUnhandledExceptionFilter(L64PTR(Value));
         #endif
         break;

      case RES_LOG_LEVEL:
         if ((Value >= 0) AND (Value <= 9)) glLogLevel = Value;
         break;

      case RES_LOG_DEPTH: tlDepth = Value; break;

#ifdef _WIN32
      case RES_NET_PROCESSING: glNetProcessMessages = (APTR)(MAXINT)Value; break;
#else
      case RES_NET_PROCESSING: break;
#endif

      case RES_GLOBAL_INSTANCE:
         LogF("SetResource()","Global instance can only be requested on Core initialisation.");
         break;

      case RES_JNI_ENV: glJNIEnv = L64PTR(Value); break;

#ifdef __unix__
      case RES_X11_FD: glX11FD = Value; break;
#endif

      case RES_PRIVILEGED_USER:
#ifdef __unix__
         FMSG("SetResource()","Privileged User: %s, Current UID: %d, Depth: %d", (Value) ? "TRUE" : "FALSE", geteuid(), privileged);

         if (glPrivileged) return ERR_Okay; // In privileged mode, the user is always an admin

         if (Value) {
            // Enable admin privileges

            oldvalue = ERR_Okay;

            if (!privileged) {
               if (glUID) {
                  if (glUID != glEUID) {
                     seteuid(glEUID);
                     privileged++;
                  }
                  else {
                     LogF("SetResource:","Admin privileges not available.");
                     oldvalue = ERR_Failed; // Admin privileges are not available
                  }
               }
               else privileged++;; // The user already has admin privileges
            }
            else privileged++;
         }
         else {
            // Disable admin privileges
            if (privileged > 0) {
               privileged--;
               if (!privileged) {
                  if (glUID != glEUID) seteuid(glUID);
               }
            }
         }
#else
         return ERR_Okay;
#endif
         break;

      default:
         LogF("@SetResource()","Unrecognised resource ID: %d, Value: " PF64(), Resource, Value);
   }

   return oldvalue;
}

/*****************************************************************************

*****************************************************************************/
/*
ERROR SetSubscriptionPriority(OBJECTPTR Object, ACTIONID ActionID, OBJECTID SubscriberID, LONG Priority)
{
   struct ActionSubscription *list, *newlist;
   MEMORYID newlistid;
   LONG i, error, memflags;
   APTR context;

   if ((!Object) OR (!ActionID) OR (!SubscriberID)) return ERR_Args;

   if (AccessMemory(Object->Stats->ActionSubscriptions.ID, MEM_READ_WRITE, 2000, (APTR *)&list) IS ERR_Okay) {
      for (i=0; (i < Object->Stats->SubscriptionSize) AND (list[i].ActionID); i++) {
         if ((list[i].ActionID IS ActionID) AND (list[i].SubscriberID IS Subscriber->UniqueID)) break;
      }

      if (i >= Object->Stats->SubscriptionSize) {

         context = SetContext(Object);
            error = AllocMemory(sizeof(struct ActionSubscription)*(Object->Stats->SubscriptionSize+10),
                                memflags, NULL, &newlistid);
         SetContext(context);

         if (error IS ERR_Okay) {
            if (AccessMemory(newlistid, MEM_READ_WRITE, 2000, (APTR *)&newlist) IS ERR_Okay) {
               for (i=0; (list[i].ActionID) AND (i < Object->Stats->SubscriptionSize); i++) {
                  newlist[i].ActionID       = list[i].ActionID;
                  newlist[i].SubscriberID   = list[i].SubscriberID;
                  newlist[i].MessagePortMID = list[i].MessagePortMID;
                  newlist[i].ClassID        = list[i].ClassID;
               }

               ReleaseMemory(list);
               FreeResourceID(Object->Stats->ActionSubscriptions.ID);

               Object->Stats->ActionSubscriptions.ID = newlistid;
               Object->Stats->SubscriptionSize += 10;
               list = newlist;
            }
            else {
               FreeResourceID(newlistid);
               ReleaseMemory(list);
               return LogError(ERH_SubscribeAction, ERR_AccessMemory);
            }
         }
         else {
            ReleaseMemory(list);
            return LogError(ERH_SubscribeAction, ERR_AllocMemory);
         }
      }

      list[i].ActionID       = ActionID;
      list[i].SubscriberID   = Subscriber->UniqueID;
      list[i].ClassID        = Subscriber->ClassID;
      list[i].MessagePortMID = glTaskMessageMID;

      if (ActionID > 0) Object->Stats->NotifyFlags[ActionID>>5] |= 1<<(ActionID & 31);
      else Object->Stats->MethodFlags[(-ActionID)>>5] |= 1<<((-ActionID) & 31);
      ReleaseMemory(list);
      return ERR_Okay;
   }
   else return LogError(ERH_SubscribeAction, ERR_AccessMemory);

   return ERR_Okay;
}
*/

/*****************************************************************************

-FUNCTION-
SubscribeFeed: Listens to an object's incoming data feed.
Category: Objects

This function is used to subscribe to objects that support data feeds.  Hardware and I/O based classes such as
@Surface are good examples of feed supportive class types.

After subscribing to a data feed, the target object will intermittently send data messages to the caller via the
#DataFeed() action.  The exact format of the data should be documented by the class of that object.  Refer to the
DataFeed section of the Action List document for further information on known data types.

-INPUT-
obj Object: The target object for a data feed subscription.

-ERRORS-
Okay:
NullArgs:
AllocMemory:  The function could not allocate a feed list for the target object.
AccessMemory: Access to the target object's feed list was denied.

*****************************************************************************/

ERROR SubscribeFeed(OBJECTPTR Object)
{
   struct FeedSubscription *list, *newlist;
   ERROR error;
   MEMORYID newlistid;
   LONG i, memflags;

   if (!Object) return LogError(ERH_SubscribeFeed, ERR_NullArgs);

   prv_access(Object);

   FMSG("SubscribeFeed()","%s: %d", ((struct rkMetaClass *)Object->Class)->ClassName, Object->UniqueID);

   if (Object->Flags & NF_PUBLIC) memflags = Object->MemFlags|MEM_PUBLIC;
   else memflags = Object->MemFlags;

   if (!Object->Stats->MID_FeedList) {
      // Allocate a feed list for the first time

      OBJECTPTR context = SetContext(Object);
         error = AllocMemory(sizeof(struct FeedSubscription)*2, MEM_NO_CLEAR|memflags, NULL, &Object->Stats->MID_FeedList);
      SetContext(context);

      if (!error) {
         if (!AccessMemory(Object->Stats->MID_FeedList, MEM_WRITE, 2000, (APTR *)&list)) {
            list[0].SubscriberID   = tlContext->Object->UniqueID;
            list[0].MessagePortMID = glTaskMessageMID;
            list[0].ClassID        = tlContext->Object->ClassID;
            list[1].SubscriberID   = 0;
            ReleaseMemoryID(Object->Stats->MID_FeedList);
         }
      }
      else {
         prv_release(Object);
         return LogError(ERH_SubscribeFeed, ERR_AllocMemory);
      }
   }
   else if (!AccessMemory(Object->Stats->MID_FeedList, MEM_READ_WRITE, 2000, (APTR *)&list)) {
      // Reallocate the feed list from scratch

      for (i=0; list[i].SubscriberID; i++);

      OBJECTPTR context = SetContext(Object);
         error = AllocMemory(sizeof(struct FeedSubscription) * (i + 2), MEM_NO_CLEAR|memflags, NULL, &newlistid);
      SetContext(context);

      if (!error) {
         if (!AccessMemory(newlistid, MEM_READ_WRITE, 2000, (APTR *)&newlist)) {
            // Copy the object list over to the new array and insert the new object ID.

            for (i=0; list[i].SubscriberID; i++) newlist[i] = list[i];

            newlist[i].SubscriberID   = tlContext->Object->UniqueID;
            newlist[i].MessagePortMID = glTaskMessageMID;
            newlist[i].ClassID        = tlContext->Object->ClassID;
            newlist[i+1].SubscriberID   = 0;
            newlist[i+1].MessagePortMID = 0;
            newlist[i+1].ClassID        = 0;

            // Free the old list

            ReleaseMemoryID(Object->Stats->MID_FeedList);
            FreeResourceID(Object->Stats->MID_FeedList);

            // Insert the new list

            Object->Stats->MID_FeedList = newlistid;

            ReleaseMemoryID(newlistid);
            prv_release(Object);
            return ERR_Okay;
         }
         else {
            prv_release(Object);
            return LogError(ERH_SubscribeFeed, ERR_AccessMemory);
         }
         FreeResourceID(newlistid);
      }
      else {
         prv_release(Object);
         return LogError(ERH_SubscribeFeed, ERR_AllocMemory);
      }
      ReleaseMemoryID(Object->Stats->MID_FeedList);
   }

   prv_release(Object);
   return ERR_Okay;
}

/*****************************************************************************

-FUNCTION-
SubscribeTimer: Subscribes an object or function to the timer service.

This function creates a new timer subscription that will be called at regular intervals for the calling object.

A callback function must be provided that follows this prototype: `ERROR Function(OBJECTPTR Subscriber, LARGE Elapsed, LARGE CurrentTime)`

The Elapsed parameter is the total number of microseconds that have elapsed since the last call.  The CurrentTime
parameter is set to the ~PreciseTime() value just prior to the Callback being called.  The callback function
can return ERR_Terminate at any time to cancel the subscription.  All other error codes are ignored.  Fluid callbacks
should call check(ERR_Terminate) to perform the equivalent of this behaviour.

To change the interval, call ~UpdateTimer() with the new value.  To release a timer subscription, call
~UpdateTimer() with the resulting SubscriptionID and an Interval of zero.

Timer management is provisioned by the ~ProcessMessages() function.  Failure to regularly process incoming
messages will lead to unreliable timer cycles.  It should be noted that the smaller the Interval that has been used,
the more imperative regular message checking becomes.  Prolonged processing inside a timer routine can also impact on
other timer subscriptions that are waiting to be processed.

-INPUT-
double Interval:   The total number of seconds to wait between timer calls.
ptr(func) Callback: A callback function is required that will be called on each time cycle.
&ptr Subscription: The subscription will be assigned an identifier that is returned in this parameter.

-ERRORS-
Okay:
NullArgs:
Args:
ArrayFull: The task's timer array is at capacity - no more subscriptions can be granted.
BadState: The subscriber is marked for termination.
SystemLocked:

*****************************************************************************/

ERROR SubscribeTimer(DOUBLE Interval, FUNCTION *Callback, APTR *Subscription)
{
   if ((!Interval) OR (!Callback)) return LogError(ERH_SubscribeTimer, ERR_NullArgs);
   if (Interval < 0) return LogError(ERH_SubscribeTimer, ERR_Args);

   OBJECTPTR subscriber = tlContext->Object;
   if (subscriber->Flags & (NF_FREE|NF_FREE_MARK)) return ERR_BadState;

   if (glLogLevel >= 7) {
      if (Callback->Type IS CALL_SCRIPT) {
         LogF("7SubscribeTimer()", "Interval: %.3fs", Interval);
      }
      else LogF("7SubscribeTimer()", "Callback: %p, Interval: %.3fs", Callback->StdC.Routine, Interval);
   }

   if (!thread_lock(TL_TIMER, 200)) {
      LARGE usInterval = (LARGE)(Interval * 1000000.0); // Scale the interval to microseconds
      if (usInterval <= 40000) {
         // TODO: Rapid timers should be synchronised with other existing timers to limit the number of
         // interruptions that occur per second.
      }

      struct CoreTimer *timer;
      if ((timer = malloc(sizeof(struct CoreTimer)))) {
         LARGE subscribed    = PreciseTime();
         timer->SubscriberID = subscriber->UniqueID;
         timer->Interval     = usInterval;
         timer->LastCall     = subscribed;
         timer->NextCall     = subscribed + usInterval;
         timer->Routine      = *Callback;
         timer->Locked       = FALSE;
         timer->Cycle        = glTimerCycle - 1;

         if (subscriber->UniqueID > 0) timer->Subscriber = subscriber;
         else timer->Subscriber = NULL;

         // For resource tracking purposes it is important for us to keep a record of the subscription so that
         // we don't treat the object address as valid when it's been removed from the system.

         subscriber->Flags |= NF_TIMER_SUB;

         if (Subscription) *Subscription = timer;

         timer->Prev = NULL;
         timer->Next = glTimers;
         if (glTimers) glTimers->Prev = timer;
         glTimers = timer;

         thread_unlock(TL_TIMER);
         return ERR_Okay;
      }
      else {
         thread_unlock(TL_TIMER);
         return ERR_AllocMemory;
      }
   }
   else return ERR_SystemLocked;
}

/*****************************************************************************

-FUNCTION-
PreciseTime: Returns the current system time, in microseconds.

This function returns the current 'system time', in microseconds (1 millionth of a second).  The value is monotonic
if the host platform allows it (typically expressed as the amount of time that has elapsed since the system was
switched on).  The benefit of monotonic time is that it is unaffected by changes to the system clock, such as daylight
savings adjustments or manual changes by the user.

-RESULT-
large: Returns the system time in microseconds.  An error is extremely unlikely, but zero is returned in the event of one.

*****************************************************************************/

LARGE PreciseTime(void)
{
#ifdef __unix__
   struct timespec time;

   if (!clock_gettime(CLOCK_MONOTONIC, &time)) {
      return ((LARGE)time.tv_sec * 1000000LL) + ((LARGE)time.tv_nsec / 1000LL);
   }
   else return 0;
#else
   return winGetTickCount(); // NB: This timer does start from the boot time, but can be adjusted - therefore is not 100% on monotonic status
#endif
}

/*****************************************************************************

-FUNCTION-
UnsubscribeFeed: Removes data feed subscriptions from an external object.
Category: Objects

This function will remove subscriptions made by ~SubscribeFeed().

-INPUT-
obj Object: The object to unsubscribe from.

-ERRORS-
Okay:         The termination of service was successful.
NullArgs:
Search:       The object referred to by the SubscriberID was not in the subscription list.
AccessMemory: Access to the object's feed subscription array was denied.

*****************************************************************************/

ERROR UnsubscribeFeed(OBJECTPTR Object)
{
   FMSG("UnsubscribeFeed()","%s: %d", ((struct rkMetaClass *)Object->Class)->ClassName, Object->UniqueID);

   if (!Object) return LogError(ERH_CloseFeed, ERR_NullArgs);

   if (!Object->Stats->MID_FeedList) return ERR_Search;

   prv_access(Object);

   struct FeedSubscription *list;
   if (!AccessMemory(Object->Stats->MID_FeedList, MEM_READ_WRITE, 2000, (APTR *)&list)) {
      LONG i;
      for (i=0; list[i].SubscriberID; i++) {
         if (list[i].SubscriberID IS tlContext->Object->UniqueID) {
            while (list[i+1].SubscriberID) { // Compact the list
               list[i] = list[i+1];
               i++;
            }

            list[i].SubscriberID   = 0;
            list[i].MessagePortMID = 0;
            list[i].ClassID        = 0;

            ReleaseMemoryID(Object->Stats->MID_FeedList);

            if (i <= 0) {
               // Destroy the subscription list
               FreeResourceID(Object->Stats->MID_FeedList);
               Object->Stats->MID_FeedList  = 0;
            }

            prv_release(Object);
            return ERR_Okay;
         }
      }
      ReleaseMemoryID(Object->Stats->MID_FeedList);
      prv_release(Object);
      return ERR_Search;
   }
   else {
      prv_release(Object);
      return LogError(ERH_CloseFeed, ERR_AccessMemory);
   }
}

/*****************************************************************************

-FUNCTION-
UpdateTimer: Modify or remove a subscription created by SubscribeTimer().

This function complements ~SubscribeTimer().  It can change the interval for an existing timer subscription,
or remove it if the Interval is set to zero.

-INPUT-
ptr Subscription: The timer subscription to modify.
double Interval: The new interval for the timer (measured in seconds), or zero to remove.

-ERRORS-
Okay:
NullArgs:
SystemLocked:
Search:

*****************************************************************************/

ERROR UpdateTimer(APTR Subscription, DOUBLE Interval)
{
   if (!Subscription) return LogError(ERH_UpdateTimer, ERR_NullArgs);

   if (glLogLevel >= 7) LogF("7UpdateTimer()", "Subscription: %p, Interval: %.4f", Subscription, Interval);

   if (!thread_lock(TL_TIMER, 200)) {
      struct CoreTimer *timer = Subscription;
      if (Interval < 0) {
         // Special mode: Preserve existing timer settings for the subscriber (ticker values are not reset etc)
         LARGE usInterval = -((LARGE)(Interval * 1000000.0));
         if (usInterval < timer->Interval) timer->Interval = usInterval;
         thread_unlock(TL_TIMER);
         return ERR_Okay;
      }
      else if (Interval > 0) {
         LARGE usInterval = (LARGE)(Interval * 1000000.0);
         timer->Interval = usInterval;
         timer->NextCall = PreciseTime() + usInterval;
         thread_unlock(TL_TIMER);
         return ERR_Okay;
      }
      else {
         if (timer->Locked) {
            // A timer can't be removed during its execution, but we can nullify the function entry
            // and ProcessMessages() will automatically terminate it on the next cycle.
            timer->Routine.Type = 0;
            thread_unlock(TL_TIMER);
            return LogError(ERH_UpdateTimer, ERR_AlreadyLocked);
         }

         if (timer->Next) timer->Next->Prev = timer->Prev;
         if (timer->Prev) timer->Prev->Next = timer->Next;
         if (glTimers IS timer) glTimers = timer->Next;
         thread_unlock(TL_TIMER);

         if (timer->Routine.Type IS CALL_SCRIPT) {
            scDerefProcedure(timer->Routine.Script.Script, &timer->Routine);
         }

         free(timer);
         return ERR_Okay;
      }
   }
   else return LogError(ERH_UpdateTimer, ERR_SystemLocked);
}

/*****************************************************************************

-FUNCTION-
WaitTime: Waits for a specified amount of seconds and/or microseconds.

This function waits for a period of time as specified by the Seconds and MicroSeconds arguments.  While waiting, your
task will continue to process incoming messages in order to prevent the process' message queue from developing a
back-log.

WaitTime() can return earlier than the indicated timeout if a message handler returns ERR_Terminate, or if a MSGID_QUIT
message is sent to the task's message queue.

-INPUT-
int Seconds:      The number of seconds to wait for.
int MicroSeconds: The number of microseconds to wait for.  Please note that a microsecond is one-millionth of a second - 1/1000000.  The value cannot exceed 999999.

-END-

*****************************************************************************/

void WaitTime(LONG Seconds, LONG MicroSeconds)
{
   BYTE processmsg;

   if (!tlMainThread) {
      processmsg = FALSE; // Child threads never process messages.
      if (Seconds < 0) Seconds = -Seconds;
      if (MicroSeconds < 0) MicroSeconds = -MicroSeconds;
    }
   else {
      // If the Seconds or MicroSeconds arguments are negative, turn off the ProcessMessages() support.
      processmsg = TRUE;
      if (Seconds < 0) { Seconds = -Seconds; processmsg = FALSE; }
      if (MicroSeconds < 0) { MicroSeconds = -MicroSeconds; processmsg = FALSE; }
   }

   while (MicroSeconds >= 1000000) {
      MicroSeconds -= 1000000;
      Seconds++;
   }

   if (processmsg) {
      LARGE current = PreciseTime() / 1000LL;
      LARGE end = current + (Seconds * 1000) + (MicroSeconds/1000);
      do {
         if (ProcessMessages(0, end - current) IS ERR_Terminate) break;
         current = (PreciseTime()/1000LL);
      } while (current < end);
   }
   else {
      #ifdef __unix__
         struct timespec nano;
         nano.tv_sec  = Seconds;
         nano.tv_nsec = MicroSeconds * 100;
         while (nanosleep(&nano, &nano) == -1) continue;
      #elif _WIN32
         winSleep((Seconds * 1000) + (MicroSeconds/1000));
      #else
         #warn Platform needs support for WaitTime()
      #endif
   }
}

//****************************************************************************
// NOTE: To be called with TL_OBJECT_LOOKUP only.

void remove_object_hash(OBJECTPTR Object)
{
   if (Object->UniqueID < 0) return; // Public objects not supported by this function

   OBJECTPTR *list;
   LONG list_size;
   if (!VarGet(glObjectLookup, Object->Stats->Name, (APTR *)&list, &list_size)) {
      list_size = list_size / sizeof(OBJECTPTR);

      LONG count_others = 0;
      LONG i;
      if (list_size > 1) {
         for (i=0; i < list_size; i++) {
            if (list[i] IS Object) list[i] = NULL;
            else count_others++;
         }
      }

      if (!count_others) { // If no other objects exist for this key, remove the key.
         VarSet(glObjectLookup, Object->Stats->Name, NULL, 0);
      }
   }
   else FMSG("@remove_obj_hash","No hash entry for object '%s'", Object->Stats->Name);
}

//****************************************************************************

void set_object_flags(OBJECTPTR Object, LONG Flags)
{
   Object->Flags = Flags;

   if (Object->UniqueID < 0) {
      struct SharedObjectHeader *header;
      if (!AccessMemory(RPM_SharedObjects, MEM_READ, 2000, (void **)&header)) {
         struct SharedObject *pubobj = ResolveAddress(header, header->Offset);
         LONG index;
         if (!find_public_object_entry(header, Object->UniqueID, &index)) {
            pubobj[index].Flags = Flags;
         }
         ReleaseMemoryID(RPM_SharedObjects);
      }
      else LogF("@set_object_flags","Failed to access the PublicObjects array.");
   }
}
