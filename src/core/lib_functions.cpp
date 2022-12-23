/*****************************************************************************

The source code of the Parasol Framework is made publicly available under the
terms described in the LICENSE.TXT file that is distributed with this package.
Please refer to it for further information on licensing.

-CATEGORY-
Name: System
-END-

*********************************************************************************************************************/

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

using namespace parasol;

/*********************************************************************************************************************

-FUNCTION-
AllocateID: Generates unique ID's for general purposes.

This function generates unique ID's that can be used in other Core functions.  A type ID is required and the resulting
number will be unique to that type only.

ID allocations are permanent, so there is no need to free the allocated ID once it is no longer required.

-INPUT-
int(IDTYPE) Type: The type of ID that is required.

-RESULT-
int: A unique ID matching the requested type will be returned.  This function can return zero if the Type is unrecognised, or if an internal error occurred.

*********************************************************************************************************************/

LONG AllocateID(LONG Type)
{
   parasol::Log log(__FUNCTION__);

   if (Type IS IDTYPE_MESSAGE) {
      if (glSharedControl->MessageIDCount < 10000) glSharedControl->MessageIDCount = 10000;
      LONG id = __sync_add_and_fetch(&glSharedControl->MessageIDCount, 1);

      log.function("MessageID: %d", id);
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

/*********************************************************************************************************************

-FUNCTION-
CheckObjectExists: Checks if a particular object is still available in the system.
Category: Objects

The CheckObjectExists() function checks for the presence of any object created by NewObect().  Support for shared
objects that exist outside the current process space is included.

-INPUT-
oid Object: The object ID that you want to look for.

-ERRORS-
True:  The object exists.
False: The object ID does not exist.
NullArgs:
SystemLocked:
LockFailed:

*********************************************************************************************************************/

ERROR CheckObjectExists(OBJECTID ObjectID)
{
   parasol::Log log(__FUNCTION__);

   if (ObjectID < 0) {
      ScopedSysLock lock(PL_PUBLICMEM, 4000);
      if (lock.granted()) {
         if (!find_public_mem_id(glSharedControl, ObjectID, NULL)) return ERR_True;
         else return ERR_False;
      }
      else return log.warning(ERR_SystemLocked);
   }
   else if (ObjectID > 0) {
      if (ObjectID IS SystemTaskID) return ERR_True;

      ThreadLock lock(TL_PRIVATE_MEM, 4000);
      if (lock.granted()) {
         LONG result = ERR_False;
         auto mem = glPrivateMemory.find(ObjectID);
         if ((mem != glPrivateMemory.end()) and (mem->second.Object)) {
            if (mem->second.Object->Flags & NF_UNLOCK_FREE);
            else result = ERR_True;
         }
         return result;
      }
      else return log.warning(ERR_LockFailed);
   }
   else return log.warning(ERR_NullArgs);
}

/*********************************************************************************************************************

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

*********************************************************************************************************************/

ERROR CopyMemory(const void *Src, APTR Dest, LONG Length)
{
   if ((!Src) or (!Dest)) return ERR_NullArgs;
   if (Length < 0) return ERR_Args;
   if (Src IS Dest) return ERR_Okay;

   // As of 2013 we are presuming that memmove() is suitably pre-optimised by either the compiler or the host platform.

   memmove(Dest, Src, Length);
   return ERR_Okay;
}

/*********************************************************************************************************************

-FUNCTION-
CurrentContext: Returns a pointer to the object that has the current context.
Category: Objects

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
CurrentTask: Returns the active Task object.

This function returns the @Task object of the active process.

If there is a legitimate circumstance where there is no current task (e.g. if the function is called during
Core initialisation) then the "system task" may be returned, which has ownership of Core resources.

-RESULT-
obj: Returns a pointer to the current Task object or NULL if failure.

*********************************************************************************************************************/

OBJECTPTR CurrentTask(void)
{
   return glCurrentTask;
}

/*********************************************************************************************************************

-FUNCTION-
FindClass: Returns all class objects for a given class ID.
Category: Objects

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
   parasol::Log log(__FUNCTION__);

   if (ClassID IS ID_METACLASS) { // Return the internal pointer to the MetaClass.
      return (objMetaClass *)&glMetaClass;
   }
   else if (!ClassID) {
      return NULL;
   }
   else {
      // A simple KeyGet() works for base-classes and sub-classes because the hash map is indexed by class name.

      extMetaClass **ptr;
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

      ClassItem *item;
      CSTRING path = NULL;
      if ((item = find_class(ClassID))) {
         if (item->PathOffset) path = (CSTRING)item + item->PathOffset;
      }

      if (!path) {
         ModuleItem *mod;
         if ((mod = find_module(ClassID))) path = (STRING)(mod + 1);
      }

      extMetaClass *mc = NULL;
      if (path) {
         // Load the module from the associated location and then find the class that it contains.  If the module fails,
         // we keep on looking for other installed modules that may handle the class.

         log.branch("Attempting to load module \"%s\" for class $%.8x.", path, ClassID);

         OBJECTPTR module;
         if (!CreateObject(ID_MODULE, NF_UNTRACKED, &module, FID_Name|TSTR, path, TAGEND)) {
            if (!KeyGet(glClassMap, ClassID, (APTR *)&ptr, NULL)) mc = ptr[0];
            acFree(module);  // Free the module object - the code and any classes it created will continue to remain in memory.
         }
      }

      if (mc) log.msg("Found class \"%s\"", mc->ClassName);
      else log.warning("Could not find class $%.8x in memory or in class references.", ClassID);

      return mc;
   }
}

/*********************************************************************************************************************

-FUNCTION-
FindObject: Searches for objects by name.
Category: Objects

The FindObject() function searches for all objects that match a given name and can filter by class.  A pre-allocated
buffer is required for the output of the results.

The following example is a typical illustration of this function's use.  It finds the most recent object created
with a given name:

<pre>
OBJECTID id;
LONG count = 1;
FindObject("SystemPointer", ID_POINTER, 0, &id, &count);
</pre>

If FindObject() cannot find any matching objects then it will return an error code.

The list is sorted so that the oldest private object is placed at the start of the list, and the most recent public object
is placed at the end.  Take advantage of this fact to get the oldest or youngest object with the Name that is being
searched for.  Preference is also given to objects that have been created by the calling process, thus foreign objects
are pushed towards the end of the array.

-INPUT-
cstr Name:     The name of an object to search for.
cid ClassID:   Optional.  Set to a class ID to filter the results down to a specific class type.
int(FOF) Flags: Optional flags.
buf(array(oid)) Array:    Pointer to the array that will store the results.
&arraysize Count: Indicates the size of Array, measured in elements.  Must be set to a value of 1 or greater.

-ERRORS-
Okay: At least one object was found and stored in the supplied array.
Args:
Search: No objects matching the given name could be found.
AccessMemory: Access to the RPM_SharedObjects memory block was denied.
LockFailed:
EmptyString:
DoesNotExist:
-END-

*********************************************************************************************************************/

ERROR FindObject(CSTRING InitialName, CLASSID ClassID, LONG Flags, OBJECTID *Array, LONG *Count)
{
   parasol::Log log(__FUNCTION__);

   if ((!Array) or (!InitialName) or (!Count)) return ERR_NullArgs;
   if (*Count < 1) return log.warning(ERR_Args);
   if (!InitialName[0]) return log.warning(ERR_EmptyString);

   if (Flags & FOF_SMART_NAMES) {
      // If an integer based name (defined by #num) is passed, we translate it to an ObjectID rather than searching for
      // an object of name "#1234".

      BYTE number = FALSE;
      if (InitialName[0] IS '#') number = TRUE;
      else {
         // If the name consists entirely of numbers, it must be considered an object ID (we can make this check because
         // it is illegal for a name to consist entirely of digits).

         LONG i = (InitialName[0] IS '-') ? 1 : 0;
         for (; InitialName[i]; i++) {
            if (InitialName[i] < '0') break;
            if (InitialName[i] > '9') break;
         }
         if (!InitialName[i]) number = TRUE;
      }

      if (number) {
         OBJECTID objectid;
         if ((objectid = (OBJECTID)StrToInt(InitialName))) {
            if (!CheckObjectExists(objectid)) {
               *Array = objectid;
               *Count = 1;
               return ERR_Okay;
            }
            else return ERR_Search;
         }
         else return ERR_Search;
      }

      if (!StrMatch("owner", InitialName)) {
         if ((tlContext != &glTopContext) and (tlContext->object()->OwnerID)) {
            if (!CheckObjectExists(tlContext->object()->OwnerID)) {
               *Array = tlContext->object()->OwnerID;
               *Count = 1;
               return ERR_Okay;
            }
            else return ERR_DoesNotExist;
         }
         else return ERR_DoesNotExist;
      }
   }

   class sortobj {
      public: OBJECTID id; MEMORYID messagemid;
      sortobj(OBJECTID a, MEMORYID b) : id(a), messagemid(b) { };
   };

   std::list<sortobj> objlist;

   // Private object search

   {
      ThreadLock lock(TL_OBJECT_LOOKUP, 4000);
      if (lock.granted()) {
         OBJECTPTR *list;
         LONG list_size;
         if (!VarGet(glObjectLookup, InitialName, (APTR *)&list, &list_size)) {
            list_size = list_size / sizeof(OBJECTPTR);
            for (LONG i=0; i < list_size; i++) {
               OBJECTPTR object = list[i];
               if ((object) and ((!ClassID) or (ClassID IS object->ClassID))) {
                  if (objlist.size() < (size_t)Count[0]) {
                     objlist.emplace_back(object->UID, glTaskMessageMID);
                  }
                  else if (objlist.back().id < object->UID) {
                     objlist.back().id = object->UID;
                     objlist.back().messagemid = glTaskMessageMID;
                  }
               }
            }
         }
      }
   }

   if ((Flags & FOF_INCLUDE_SHARED) and (objlist.size() < (size_t)Count[0])) {
      // Public object search.  When looking for publicly named objects we need to keep them arranged so that preference
      // is given to objects that this process created.  This causes some mild overhead but is vital for ensuring
      // that programs aren't confused when they use identically named objects.

      LONG i;
      char name[MAX_NAME_LEN+1];
      for (i=0; (InitialName[i]) and (i < MAX_NAME_LEN - 1); i++) {
         char c = InitialName[i];
         if ((c >= 'A') and (c <= 'Z')) name[i] = c - 'A' + 'a';
         else name[i] = c;
      }
      name[i] = 0;

      SharedObjectHeader *header;
      if (!AccessMemory(RPM_SharedObjects, MEM_READ, 2000, (void **)&header)) {
         auto entry = (SharedObject *)ResolveAddress(header, header->Offset);
         for (LONG i=0; i < header->NextEntry; i++) {
            if (!entry[i].ObjectID) continue;
            if ((!entry[i].InstanceID) or (entry[i].InstanceID IS glInstanceID)) {
               if ((ClassID) and (ClassID != entry[i].ClassID));
               else if (entry[i].Name[0] IS name[0]) {
                  if (!StrCompare(entry[i].Name, name, 0, STR_CASE|STR_MATCH_LEN)) {
                     if (objlist.size() < (size_t)Count[0]) {
                        objlist.emplace_back(entry[i].ObjectID, entry[i].MessageMID);
                     }
                     else if (objlist.back().id > entry[i].ObjectID) {
                        // The discovered object has a more recent ID than the last entry in the list, so replace it
                        // (assuming that it won't replace something that was created from our own task space).

                        if ((objlist.back().messagemid IS glTaskMessageMID) and (entry[i].MessageMID != glTaskMessageMID)) continue;
                        objlist.back().id = entry[i].ObjectID;
                        objlist.back().messagemid = entry[i].MessageMID;
                     }
                  }
               }
            }
         }

         ReleaseMemoryID(RPM_SharedObjects);
      }
      else return log.warning(ERR_AccessMemory);
   }

   if (objlist.size() > 0) {
      if (objlist.size() IS 1) {
         Array[0] = objlist.front().id;
      }
      else {
         objlist.sort([](const sortobj &a, const sortobj &b) {
            return ((a.id < b.id) or ((a.messagemid IS glTaskMessageMID) and (b.messagemid != glTaskMessageMID)));
         });

         LONG i = 0;
         for (const auto & obj : objlist) Array[i++] = obj.id;
      }

      *Count = objlist.size();
      return ERR_Okay;
   }
   else return ERR_Search;
}

/*********************************************************************************************************************

-FUNCTION-
FindPrivateObject: Search for an object by name.
Category: Objects

The FindPrivateObject() function is a simple implementation of ~FindObject().  It differs in being limited
to finding private objects without class filtering, and can only return one result as a pointer.
Care may need to be taken if using this function to access objects that are shared between threads.

The most recently created object is returned by this function if there are objects sharing the same name.

For more advanced functionality in object searches please use the ~FindObject() function.

-INPUT-
cstr Name:   The name of the object to find.
&obj Object: A pointer to the discovered object will be returned in this parameter.

-ERRORS-
Okay: A matching object was found.
NullArgs
Search: No objects matching the given name could be found.
LockFailed
EmptyString

*********************************************************************************************************************/

ERROR FindPrivateObject(CSTRING InitialName, OBJECTPTR *Object)
{
   parasol::Log log(__FUNCTION__);

   if ((!InitialName) or (!Object)) return log.warning(ERR_NullArgs);

   *Object = NULL;

   if (!*InitialName) return log.warning(ERR_EmptyString);

   // If an integer based name (defined by #num) is passed, we translate it to an ObjectID rather than searching for an
   // object of name "#1234".

   bool number = false;
   if (InitialName[0] IS '#') number = true;
   else {
      // If the name consists entirely of numbers, it must be considered an object ID (we can make this check because
      // it is illegal for a name to consist entirely of figures).

      LONG i;
      for (i=0; InitialName[i]; i++) {
         if (((InitialName[i] < '0') or (InitialName[i] > '9')) and (InitialName[i] != '-')) break;
      }
      if (!InitialName[i]) number = true;
   }

   if (number) {
      OBJECTID objectid;
      if ((objectid = (OBJECTID)StrToInt(InitialName))) {
         ThreadLock lock(TL_PRIVATE_MEM, 4000);
         if (lock.granted()) {
            auto mem = glPrivateMemory.find(objectid);
            if ((mem != glPrivateMemory.end()) and (mem->second.Object)) {
               *Object = mem->second.Object;
               return ERR_Okay;
            }
         }
         else return log.warning(ERR_LockFailed);
      }
      return ERR_Search;
   }
   else if (!StrMatch("owner", InitialName)) {
      if ((tlContext != &glTopContext) and (tlContext->object()->OwnerID)) {
         ThreadLock lock(TL_PRIVATE_MEM, 4000);
         if (lock.granted()) {
            auto mem = glPrivateMemory.find(tlContext->object()->OwnerID);
            if (mem != glPrivateMemory.end()) {
               if ((*Object = mem->second.Object)) {
                  return ERR_Okay;
               }
            }
         }
         else return log.warning(ERR_LockFailed);
      }
      return ERR_Search;
   }

   ThreadLock lock(TL_OBJECT_LOOKUP, 4000);
   if (lock.granted()) {
      LONG i, list_size;
      OBJECTPTR *list;
      if (!VarGet(glObjectLookup, InitialName, (APTR *)&list, &list_size)) {
         // Return the most recently created object, i.e. the one at the end of the list.
         for (i=(list_size / sizeof(OBJECTPTR)) - 1; i >= 0; i--) {
            if (list[i]) {
               *Object = list[i];
               return ERR_Okay;
            }
         }
      }
   }

   if (*Object) return ERR_Okay;
   else return ERR_Search;
}

/*********************************************************************************************************************

-FUNCTION-
GetClassID: Returns the class ID of an ID-referenced object.
Category: Objects

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
   parasol::Log log(__FUNCTION__);

   if (!ObjectID) return 0;

   OBJECTPTR object;
   if (ObjectID < 0) {
      SharedObjectHeader *header;
      CLASSID id;
      if (!AccessMemory(RPM_SharedObjects, MEM_READ, 2000, (void **)&header)) {
         auto shared_obj = (SharedObject *)ResolveAddress(header, header->Offset);
         LONG pos;
         if (!find_public_object_entry(header, ObjectID, &pos)) id = shared_obj[pos].ClassID;
         else {
            id = 0;
            log.function("Object #%d does not exist.", ObjectID);
         }
         ReleaseMemoryID(RPM_SharedObjects);
         return id;
      }
      else log.warning(ERR_AccessMemory);
   }
   else if ((object = GetObjectPtr(ObjectID))) return object->ClassID;
   else log.function("Failed to access private object #%d, no longer exists or ID invalid.", ObjectID);

   return 0;
}

/*********************************************************************************************************************

-FUNCTION-
GetErrorMsg: Translates error codes into human readable strings.
Category: Logging

The GetErrorMsg() function converts error codes into human readable strings.  If the Code is invalid, a string of
"Unknown error code" is returned.

-INPUT-
error Error: The error code to lookup.

-RESULT-
cstr: A human readable string for the error code is returned.  By default error codes are returned in English, however if a translation table exists for the user's own language, the string will be translated.

*********************************************************************************************************************/

CSTRING GetErrorMsg(ERROR Code)
{
   if ((Code < glTotalMessages) and (Code > 0)) {
      return glMessages[Code];
   }
   else if (!Code) return "Operation successful.";
   else return "Unknown error code.";
}

/*********************************************************************************************************************

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

*********************************************************************************************************************/

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

   BYTE *buf = (BYTE *)Data;
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
      for (n = 0; (size_t)n < sizeof(p)/sizeof(UBYTE); n++) {
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

/*********************************************************************************************************************

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

*********************************************************************************************************************/

LONG GetMsgPort(OBJECTID ObjectID)
{
   parasol::Log log(__FUNCTION__);

   log.trace("Object: #%d", ObjectID);

   SharedObjectHeader *header;

   if (ObjectID > 0) return glTaskMessageMID;
   else if (!AccessMemory(RPM_SharedObjects, MEM_READ, 2000, (void **)&header)) {
      LONG pos;
      if (!find_public_object_entry(header, ObjectID, &pos)) {
         auto list = (SharedObject *)ResolveAddress(header, header->Offset);
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

/*********************************************************************************************************************

-FUNCTION-
GetName: Retrieves object names.
Category: Objects

This function will return the name of the object referenced by the Object pointer. If the target object has not been
assigned a name, then you will receive a null-string.

-INPUT-
obj Object: Pointer to the object that you want to get the name of.

-RESULT-
cstr: A string containing the object name is returned.  If the object has no name or the parameter is invalid, a null-terminated string is returned.

*********************************************************************************************************************/

CSTRING GetName(OBJECTPTR Object)
{
   if ((Object) and (Object->Stats)) return Object->Stats->Name;
   else return "";
}

/*********************************************************************************************************************

-FUNCTION-
GetObjectPtr: Returns the object address for any private object ID.
Category: Objects

This function translates private object ID's (owned by the process) to their respective address pointers.  Public
object ID's are not supported.

-INPUT-
oid Object: The ID of the object to lookup.

-RESULT-
obj: The address of the object is returned, or NULL if the ID does not relate to a private object.

*********************************************************************************************************************/

OBJECTPTR GetObjectPtr(OBJECTID ObjectID)
{
   ThreadLock lock(TL_PRIVATE_MEM, 4000);
   if (lock.granted()) {
      auto mem = glPrivateMemory.find(ObjectID);
      if (mem != glPrivateMemory.end()) {
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
Category: Objects

This function returns an identifier for the owner of any valid object.  This is the fastest way to retrieve the
owner of an object if only the ID is known.

If the object address is already known then the fastest means of retrieval is via the ownerID() C++ class method.

-INPUT-
oid Object: The ID of the object that you want to examine.

-RESULT-
oid: Returns the ID of the object's owner.  If the object does not have a owner (i.e. if it is untracked) or if the ID that you provided is invalid, this function will return NULL.

*********************************************************************************************************************/

OBJECTID GetOwnerID(OBJECTID ObjectID)
{
   OBJECTID ownerid = 0;
   if (ObjectID < 0) {
      SharedObjectHeader *header;
      if (!AccessMemory(RPM_SharedObjects, MEM_READ, 2000, (void **)&header)) {
         LONG pos;
         if (!find_public_object_entry(header, ObjectID, &pos)) {
            auto list = (SharedObject *)ResolveAddress(header, header->Offset);
            ownerid = list[pos].OwnerID;
         }
         ReleaseMemoryID(RPM_SharedObjects);
      }
   }
   else {
      ThreadLock lock(TL_PRIVATE_MEM, 4000);
      if (lock.granted()) {
         auto mem = glPrivateMemory.find(ObjectID);
         if (mem != glPrivateMemory.end()) {
            if (mem->second.Object) return mem->second.Object->OwnerID;
         }
      }
   }
   return ownerid;
}

/*********************************************************************************************************************

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

*********************************************************************************************************************/

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
      case RES_DISPLAY_DRIVER:  if (glDisplayDriver[0]) return (MAXINT)glDisplayDriver; else return 0;

      case RES_PARENT_CONTEXT: {
         // Return the first parent context that differs to the current context.  This avoids any confusion
         // arising from the the current object making calls to itself.
         auto parent = tlContext->Stack;
         while ((parent) and (parent->object() IS tlContext->object())) parent = parent->Stack;
         return parent ? (MAXINT)parent->object() : 0;
      }

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
         char str[2048];
         LONG result;
         LARGE freemem = 0;
         if (!ReadFileToBuffer("/proc/meminfo", str, sizeof(str)-1, &result)) {
            LONG i = 0;
            while (i < result) {
               if (!StrCompare("Cached", str+i, sizeof("Cached")-1, 0)) {
                  freemem += (LARGE)StrToInt(str+i) * 1024LL;
               }
               else if (!StrCompare("Buffers", str+i, sizeof("Buffers")-1, 0)) {
                  freemem += (LARGE)StrToInt(str+i) * 1024LL;
               }
               else if (!StrCompare("MemFree", str+i, sizeof("MemFree")-1, 0)) {
                  freemem += (LARGE)StrToInt(str+i) * 1024LL;
               }

               while ((i < result) and (str[i] != '\n')) i++;
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
               if (!StrCompare("cpu Mhz", line, sizeof("cpu Mhz")-1, 0)) {
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

      default: //log.warning("Unsupported resource ID %d.", Resource);
         break;
   }

   return 0;
}
/*********************************************************************************************************************

-FUNCTION-
GetSystemState: Returns miscellaneous data values from the Core.

The GetSystemState() function is used to retrieve miscellaneous resource and environment values, such as resource
paths, the Core's version number and the name of the host platform.

-RESULT-
cstruct(*SystemState): A read-only SystemState structure is returned.

*********************************************************************************************************************/

const SystemState * GetSystemState(void)
{
   static bool initialised = false;
   static SystemState state;

   if (!initialised) {
      initialised = true;

      state.ConsoleFD     = glConsoleFD;
      state.CoreVersion   = VER_CORE;
      state.CoreRevision  = REV_CORE;
      state.InstanceID    = glInstanceID;
      state.ErrorMessages = glMessages;
      state.TotalErrorMessages = ARRAYSIZE(glMessages);
      state.RootPath      = glRootPath;
      state.SystemPath    = glSystemPath;
      state.ModulePath    = glModulePath;
      #ifdef __unix__
         state.Platform = "Linux";
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

/*********************************************************************************************************************

-FUNCTION-
ListChildren: Returns a list of all children belonging to an object.
Category: Objects

The ListChildren() function returns a list of an object's children in a single function call.

The client must provide an empty array of ChildEntry structures for the function to write its results.  The Count
argument must point to a `LONG` value that indicates the size of the supplied List.  Before returning, the
ListChildren() function will update the Count variable so that it reflects the total number of children that were
written to the array.

Objects marked with the `INTEGRAL` flag are not returned as they are private members of the targeted object.

-INPUT-
oid Object: The ID of the object that you wish to examine.
int IncludeShared: If TRUE, shared objects will be included in the list.  Penalises performance.
buf(array(resource(ChildEntry))) List: Must refer to an array of ChildEntry structures.
&arraysize Count:  Set to the maximum number of elements in ChildEntry.  Before returning, this parameter will be updated with the total number of entries listed in the array.

-ERRORS-
Okay: Zero or more children were found and listed.
Args
NullArgs

*********************************************************************************************************************/

ERROR ListChildren(OBJECTID ObjectID, LONG IncludeShared, ChildEntry *List, LONG *Count)
{
   parasol::Log log(__FUNCTION__);

   if ((!ObjectID) or (!List) or (!Count)) return log.warning(ERR_NullArgs);
   if ((*Count < 0) or (*Count > 3000)) return log.warning(ERR_Args);

   log.trace("#%d, List: %p, Array Size: %d", ObjectID, List, *Count);

   ERROR error = ERR_Okay;
   LONG i = 0;

   if (IncludeShared) {
      SharedObjectHeader *header;
      if (!AccessMemory(RPM_SharedObjects, MEM_READ, 2000, (void **)&header)) {
         auto list = (SharedObject *)ResolveAddress(header, header->Offset);
         for (LONG j=0; j < header->NextEntry; j++) {
            if ((list[j].OwnerID IS ObjectID) and (!(list[j].Flags & NF_INTEGRAL))) {
               List[i].ObjectID = list[j].ObjectID;
               List[i].ClassID  = list[j].ClassID;
               if (++i >= *Count) break;
            }
         }
         ReleaseMemoryID(RPM_SharedObjects);
      }
   }

   // Build the list of private objects

   if (i < *Count) {
      ThreadLock lock(TL_PRIVATE_MEM, 4000);
      if (lock.granted()) {
         for (const auto id : glObjectChildren[ObjectID]) {
            auto mem = glPrivateMemory.find(id);
            if (mem IS glPrivateMemory.end()) continue;

            if (auto child = mem->second.Object) {
               if (!(child->Flags & NF_INTEGRAL)) {
                  List[i].ObjectID = child->UID;
                  List[i].ClassID  = child->ClassID;
                  if (++i >= *Count) break;
               }
            }
         }
      }
   }

   *Count = i;
   return error;
}

/*********************************************************************************************************************

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

*********************************************************************************************************************/

ERROR ListTasks(LONG Flags, struct ListTasks **Detail)
{
   parasol::Log log(__FUNCTION__);

   if (!Detail) return ERR_NullArgs;

   ScopedSysLock lock(PL_PROCESSES, 4000);
   if (lock.granted()) {
      WORD taskcount = 0;
      LONG memlocks = 0;
      struct ListTasks *list;
      for (LONG i=0; i < MAX_TASKS; i++) {
         if ((shTasks[i].ProcessID) and (shTasks[i].TaskID) and (shTasks[i].MessageID)) {
            if (Flags & LTF_CURRENT_PROCESS) {
               if (shTasks[i].TaskID != glCurrentTaskID) continue;
            }

            taskcount++;
            for (LONG j=0; j < ARRAYSIZE(shTasks[i].NoBlockLocks); j++) {
                if (shTasks[i].NoBlockLocks[j].MemoryID) memlocks++;
            }
         }
      }

      if (!AllocMemory((sizeof(struct ListTasks) * (taskcount + 1)) + (sizeof(shTasks[0].NoBlockLocks[0]) * memlocks), MEM_NO_CLEAR, (void **)&list, NULL)) {
         *Detail = list;

         LONG j = 0;
         for (LONG i=0; (i < MAX_TASKS) and (j < taskcount); i++) {
            if ((shTasks[i].ProcessID) and (shTasks[i].TaskID) and (shTasks[i].MessageID)) {
               if (Flags & LTF_CURRENT_PROCESS) {
                  if (shTasks[i].TaskID != glCurrentTaskID) continue;
               }

               list->ProcessID   = shTasks[i].ProcessID;
               list->TaskID      = shTasks[i].TaskID;
               list->MessageID   = shTasks[i].MessageID;
               list->OutputID    = shTasks[i].OutputID;
               list->InstanceID  = shTasks[i].InstanceID;
               list->ModalID     = shTasks[i].ModalID;
               list->MemoryLocks = (MemoryLocks *)(list + 1);

               memlocks = 0; // Insert memory locks for this task entry
               for (LONG k=0; k < ARRAYSIZE(shTasks[i].NoBlockLocks); k++) {
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

         return ERR_Okay;
      }
      else return ERR_AllocMemory;
   }
   else return ERR_SystemLocked;
}

/*********************************************************************************************************************

-FUNCTION-
RegisterFD: Registers a file descriptor for monitoring when the task is asleep.

This function will register a file descriptor that will be monitored for activity when the task is sleeping.  If
activity occurs on the descriptor then the function specified in the Routine parameter will be called.  The routine
must read all of information from the descriptor, as the running process will not be able to sleep until all the
data is cleared.

The file descriptor should be configured as non-blocking before registration.  Blocking descriptors may cause your
program to hang if not handled carefully.

File descriptors support read and write states simultaneously and a callback routine can be applied to either state.
Set the `RFD_READ` flag to apply the Routine to the read callback and `RFD_WRITE` for the write callback.  If neither
flag is specified, `RFD_READ` is assumed.  A file descriptor may have up to 1 subscription per flag, for example a read
callback can be registered, followed by a write callback in a second call. Individual callbacks can be removed by
combining the read/write flags with `RFD_REMOVE`.

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

*********************************************************************************************************************/

#ifdef _WIN32
ERROR RegisterFD(HOSTHANDLE FD, LONG Flags, void (*Routine)(HOSTHANDLE, APTR), APTR Data)
#else
ERROR RegisterFD(LONG FD, LONG Flags, void (*Routine)(HOSTHANDLE, APTR), APTR Data)
#endif
{
   parasol::Log log(__FUNCTION__);

   // Note that FD's < -1 are permitted for the registering of functions marked with RFD_ALWAYS_CALL

#ifdef _WIN32
   if (FD IS (HOSTHANDLE)-1) return log.warning(ERR_Args);
   if (Flags & RFD_SOCKET) return log.warning(ERR_NoSupport); // In MS Windows, socket handles are managed as window messages (see Network module's Windows code)
#else
   if (FD IS -1) return log.warning(ERR_Args);
#endif

   if (glTotalFDs >= MAX_FDS) return log.warning(ERR_ArrayFull);

   if (!glFDTable) {
      if (!(glFDTable = (FDTable *)malloc(sizeof(FDTable) * MAX_FDS))) return ERR_AllocMemory;
   }

   if (Flags & RFD_REMOVE) {
      if (!(Flags & (RFD_READ|RFD_WRITE|RFD_EXCEPT|RFD_ALWAYS_CALL))) Flags |= RFD_READ|RFD_WRITE|RFD_EXCEPT|RFD_ALWAYS_CALL;

      for (LONG i=glTotalFDs-1; i >= 0; i--) {
         if ((glFDTable[i].FD IS FD) and ((glFDTable[i].Flags & (RFD_READ|RFD_WRITE|RFD_EXCEPT|RFD_ALWAYS_CALL)) & Flags)) {
            // If the routine address was specified with the remove option, the routine must match.

            if ((Routine) and (glFDTable[i].Routine != Routine)) continue;

            if (i+1 < glTotalFDs) {
               CopyMemory(glFDTable+i+1, glFDTable+i, sizeof(FDTable) * (glTotalFDs-i-1));
            }

            glTotalFDs--;
         }
      }
      return ERR_Okay;
   }

   if (!(Flags & (RFD_READ|RFD_WRITE|RFD_EXCEPT|RFD_REMOVE|RFD_ALWAYS_CALL))) Flags |= RFD_READ;

   LONG i;
   for (i=0; i < glTotalFDs; i++) {
      if ((glFDTable[i].FD IS FD) and (Flags & (glFDTable[i].Flags & (RFD_READ|RFD_WRITE|RFD_EXCEPT|RFD_ALWAYS_CALL)))) break;
   }

   if (i >= MAX_FDS) return log.warning(ERR_ArrayFull);

   if (i IS glTotalFDs) log.function("FD: " PF64() ", Routine: %p, Flags: $%.2x (New)", (MAXINT)FD, Routine, Flags);

#ifdef _WIN32
   // Nothing to do for Win32
#else
   if ((!Routine) and (FD > 0)) fcntl(FD, F_SETFL, fcntl(FD, F_GETFL) | O_NONBLOCK); // Ensure that the FD is non-blocking
#endif

   glFDTable[i].FD      = FD;
   glFDTable[i].Routine = Routine;
   glFDTable[i].Data    = Data;
   glFDTable[i].Flags   = Flags;
   if (i >= glTotalFDs) glTotalFDs++;

   return ERR_Okay;
}

/*********************************************************************************************************************

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

*********************************************************************************************************************/

ERROR SetOwner(OBJECTPTR Object, OBJECTPTR Owner)
{
   parasol::Log log(__FUNCTION__);

   if ((!Object) or (!Owner)) return log.warning(ERR_NullArgs);

   if (Object->OwnerID IS Owner->UID) return ERR_Okay;

   if (Object->ExtClass->Flags & CLF_NO_OWNERSHIP) {
      log.traceWarning("Cannot set the object owner as CLF_NO_OWNERSHIP is set in its class.");
      return ERR_Okay;
   }

   if (Object IS Owner) {
      log.warning("Illegal attempt to set an object owner to loop back to itself (%p).", Object);
      return ERR_Args;
   }

   //log.msg("Object: %d, New Owner: %d, Current Owner: %d", Object->UID, Owner->UID, Object->OwnerID);

   // Send a new child alert to the owner.  If the owner returns an error then we return immediately.

   ScopedObjectAccess objlock(Object);

   if (!CheckAction(Owner, AC_NewChild)) {
      ERROR error;
      struct acNewChild newchild = { .Object = Object };
      if ((error = Action(AC_NewChild, Owner, &newchild)) != ERR_NoSupport) {
         if (error != ERR_Okay) { // If the owner has passed the object through to another owner, return ERR_Okay, otherwise error.
            if (error IS ERR_OwnerPassThrough) return ERR_Okay;
            else return error;
         }
      }
   }

   struct acNewOwner newowner = {
      .NewOwnerID = Owner->UID, // Send a owner alert to the object
      .ClassID    = Owner->ClassID
   };
   Action(AC_NewOwner, Object, &newowner);

   // Make the change

   //if (Object->OwnerID) log.trace("SetOwner:","Changing the owner for object %d from %d to %d.", Object->UID, Object->OwnerID, Owner->UID);

   if (Object->Flags & NF_FOREIGN_OWNER) { // Remove subscription to AC_OwnerDestroyed
      OBJECTPTR obj;
      if (!AccessObject(Object->OwnerID, 3000, &obj)) {
         auto context = SetContext(Object);
         UnsubscribeAction(obj, AC_OwnerDestroyed);
         SetContext(context);
         ReleaseObject(obj);
      }
   }

   if (Object->UID < 0) { // Public object
      ScopedAccessMemory<SharedObjectHeader> header(RPM_SharedObjects, MEM_READ, 2000);
      if (header.granted()) {
         LONG pos;
         if (!find_public_object_entry(header.ptr, Object->UID, &pos)) {
            auto list = (SharedObject *)ResolveAddress(header.ptr, header.ptr->Offset);

            if (Object->OwnerID) { // Remove reference from the now previous owner
               auto it = glObjectChildren.find(Object->OwnerID);
               if (it != glObjectChildren.end()) it->second.erase(Object->UID);
            }

            Object->OwnerID = Owner->UID;
            list[pos].OwnerID = Owner->UID;
         }
         else return log.warning(ERR_Search);
      }
      else return log.warning(ERR_AccessMemory);

      // Track the object's memory header to the new owner

      ScopedSysLock lock(PL_PUBLICMEM, 4000);
      if (lock.granted()) {
         LONG i;
         if ((i = find_public_address(glSharedControl, Object)) != -1) {
            glSharedBlocks[i].ObjectID = Owner->UID;
         }
         else return log.warning(ERR_Search);
      }
      else return log.warning(ERR_Lock);
   }
   else {
      { // Track the object's memory header to the new owner
         ThreadLock lock(TL_PRIVATE_MEM, 4000);
         if (lock.granted()) {
            auto mem = glPrivateMemory.find(Object->UID);
            if (mem IS glPrivateMemory.end()) return log.warning(ERR_SystemCorrupt);
            mem->second.OwnerID = Owner->UID;

            // Remove reference from the now previous owner
            if (Object->OwnerID) glObjectChildren[Object->OwnerID].erase(Object->UID);

            Object->OwnerID = Owner->UID;

            glObjectChildren[Owner->UID].insert(Object->UID);
         }
         else return log.warning(ERR_Lock);
      }

      // If the owner is public and belongs to another task, subscribe to the FreeResources action so
      // that we can receive notification when the owner is destroyed.
      //
      // TODO: Would it be better if public object termination was broadcast via events and processes could
      // check their own glObjectChildren for any references?

      if ((Owner->UID < 0) and (Owner->TaskID) and (Owner->TaskID != glCurrentTaskID) and (Owner->TaskID != SystemTaskID)) {
         log.msg("Owner %d is in task %d, will monitor for termination.", Owner->UID, Owner->TaskID);
         parasol::SwitchContext ctx(Object);
         SubscribeAction(Owner, AC_OwnerDestroyed);
         Object->Flags |= NF_FOREIGN_OWNER;
      }
   }

   return ERR_Okay;
}

/*********************************************************************************************************************

-FUNCTION-
SetContext: Alters the nominated owner of newly created objects.
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
acFree(display); // The bitmap and memory are terminated here
acFree(bitmap); // Reference is no longer valid
FreeResource(memory); // Reference is no longer valid
</pre>

As the bitmap and memory block would have been freed as members of the display, their references are invalid when
manually terminated.

SetContext() is intended for use by modules and classes.  Do not use it unless conditions necessitate its use.  The
Core automatically manages the context when calling class actions, methods and interactive fields.

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
Category: Objects

This function sets the name of an object.  This enhances log messages and allows the object to be found in searches.
Please note that the length of the Name will be limited to the value indicated in the `main.h` include file, under
the `MAX_NAME_LEN` definition.  If the Name is longer than the allowed length, it will be trimmed to fit.

Object names are limited to alpha-numeric characters and the underscore symbol.  Invalid characters are replaced with
an underscore.

-INPUT-
obj Object: The target object.
cstr Name: The new name for the object.

-ERRORS-
Okay:
NullArgs:
Search:       The Object is not recognised by the system - the address may be invalid.
AccessMemory: The function could not gain access to the shared objects table (internal error).

*********************************************************************************************************************/

ERROR SetName(OBJECTPTR Object, CSTRING NewName)
{
   parasol::Log log(__FUNCTION__);
   SharedObjectHeader *header;
   LONG i, pos;

   if ((!Object) or (!NewName)) return log.warning(ERR_NullArgs);

   ScopedObjectAccess objlock(Object);

   // Remove any existing name first.

   if ((Object->Stats->Name[0]) and (Object->UID > 0)) {
      ThreadLock lock(TL_OBJECT_LOOKUP, 4000);
      if (lock.granted()) remove_object_hash(Object);
   }

   bool illegal = false;
   char c;
   for (i=0; ((c = NewName[i])) and (i < (MAX_NAME_LEN-1)); i++) {
      if ((c >= 'A') and (c <= 'Z')) {
         c = c - 'A' + 'a';
      }
      else if (((c >= 'a') and (c <= 'z')) or ((c >= '0') and (c <= '9')) or ((c IS '_'))) {
      }
      else {
         // Anything that is not alphanumeric is not permitted in the object name.
         if (!illegal) {
            illegal = true;
            log.msg("Illegal character '%c' in proposed name '%s'", c, NewName);
         }
         c = '_';
      }

      Object->Stats->Name[i] = c;
   }
   Object->Stats->Name[i] = 0;

   if (Object->UID >= 0) {
      if (Object->Stats->Name[0]) {
         ThreadLock lock(TL_OBJECT_LOOKUP, 4000);
         if (lock.granted()) {
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
         }
         else return log.warning(ERR_Lock);
      }
      return ERR_Okay;
   }
   else if (!AccessMemory(RPM_SharedObjects, MEM_READ_WRITE, 2000, (void **)&header)) {
      if (!find_public_object_entry(header, Object->UID, &pos)) {
         auto list = (SharedObject *)ResolveAddress(header, header->Offset);
         for (i=0; (Object->Stats->Name[i]) and (i < (MAX_NAME_LEN-1)); i++) {
            list[pos].Name[i] = Object->Stats->Name[i];
         }
         list[pos].Name[i] = 0;
         ReleaseMemoryID(RPM_SharedObjects);
         return ERR_Okay;
      }
      else{
         ReleaseMemoryID(RPM_SharedObjects);
         return log.warning(ERR_Search);
      }
   }
   else return log.warning(ERR_AccessMemory);
}

/*********************************************************************************************************************

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

*********************************************************************************************************************/

ERROR SetResourcePath(LONG PathType, CSTRING Path)
{
   parasol::Log log(__FUNCTION__);

   if (!PathType) return ERR_NullArgs;

   log.function("Type: %d, Path: %s", PathType, Path);

   switch(PathType) {
      case RP_ROOT_PATH:
         if (Path) {
            WORD i;
            for (i=0; (Path[i]) and ((size_t)i < sizeof(glRootPath)-2); i++) glRootPath[i] = Path[i];
            if ((glRootPath[i-1] != '/') and (glRootPath[i-1] != '\\')) {
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
            for (i=0; (Path[i]) and ((size_t)i < sizeof(glSystemPath)-2); i++) glSystemPath[i] = Path[i];
            if ((glSystemPath[i-1] != '/') and (glSystemPath[i-1] != '\\')) {
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
            for (i=0; (Path[i]) and ((size_t)i < sizeof(glModulePath)-2); i++) glModulePath[i] = Path[i];
            if ((glModulePath[i-1] != '/') and (glModulePath[i-1] != '\\')) {
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

/*********************************************************************************************************************

-FUNCTION-
SetResource: Sets miscellaneous resource identifiers.

The SetResource() function is used to set miscellaneous resource ID's that are either global or local to your process.
To set a resource you need to make a reference to it with the Resource parameter and provide a new setting in the
Value parameter.  Currently the following resource ID's are available:

<types prefix="RES" type="Resource">
<type name="ALLOC_MEM_LIMIT">Adjusts the memory limit imposed on AllocMemory().  The Value specifies the memory limit in bytes.</>
<type name="LOG_LEVEL">Adjusts the current debug level.  The Value must be between 0 and 9, where 1 is the lowest level of debug output (errors only) and 0 is off.</>
<type name="PRIVILEGED_USER">If the Value is set to 1, this resource option puts the process in privileged mode (typically this enables full administrator rights).  This feature will only work for Unix processes that are granted admin rights when launched.  Setting the Value to 0 reverts to the user's permission settings.  SetResource() will return an error code indicating the level of success.</>
</>

-INPUT-
int(RES) Resource: The ID of the resource to be set.
large Value:    The new value to set for the resource.

-RESULT-
large: Returns the previous value used for the resource that you have set.  If the resource ID that you provide is invalid, NULL is returned.
-END-

*********************************************************************************************************************/

LARGE SetResource(LONG Resource, LARGE Value)
{
   parasol::Log log(__FUNCTION__);

#ifdef __unix__
   static WORD privileged = 0;
#endif

   LARGE oldvalue = 0;

   switch(Resource) {
      case RES_CONSOLE_FD: glConsoleFD = (HOSTHANDLE)(MAXINT)Value; break;

      case RES_KEY_STATE: glKeyState = (LONG)Value; break;

      case RES_EXCEPTION_HANDLER:
         // Note: You can set your own crash handler, or set a value of NULL - this resets the existing handler which is useful if an external DLL function is suspected to have changed the filter.

         #ifdef _WIN32
            winSetUnhandledExceptionFilter((LONG (*)(LONG, APTR, LONG, LONG *))L64PTR(Value));
         #endif
         break;

      case RES_LOG_LEVEL:
         if ((Value >= 0) and (Value <= 9)) glLogLevel = Value;
         break;

      case RES_LOG_DEPTH: tlDepth = Value; break;

#ifdef _WIN32
      case RES_NET_PROCESSING: glNetProcessMessages = (void (*)(LONG, APTR))L64PTR(Value); break;
#else
      case RES_NET_PROCESSING: break;
#endif

      case RES_GLOBAL_INSTANCE:
         log.function("Global instance can only be requested on Core initialisation.");
         break;

      case RES_JNI_ENV: glJNIEnv = L64PTR(Value); break;

      case RES_PRIVILEGED_USER:
#ifdef __unix__
         log.trace("Privileged User: %s, Current UID: %d, Depth: %d", (Value) ? "TRUE" : "FALSE", geteuid(), privileged);

         if (glPrivileged) return ERR_Okay; // In privileged mode, the user is always an admin

         if (Value) { // Enable admin privileges
            oldvalue = ERR_Okay;
            if (!privileged) {
               if (glUID) {
                  if (glUID != glEUID) {
                     seteuid(glEUID);
                     privileged++;
                  }
                  else {
                     log.msg("Admin privileges not available.");
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
         log.warning("Unrecognised resource ID: %d, Value: " PF64(), Resource, Value);
   }

   return oldvalue;
}

/*********************************************************************************************************************

-FUNCTION-
SubscribeTimer: Subscribes an object or function to the timer service.

This function creates a new timer subscription that will be called at regular intervals for the calling object.

A callback function must be provided that follows this prototype: `ERROR Function(OBJECTPTR Subscriber, LARGE Elapsed, LARGE CurrentTime)`

The Elapsed parameter is the total number of microseconds that have elapsed since the last call.  The CurrentTime
parameter is set to the ~PreciseTime() value just prior to the Callback being called.  The callback function
can return `ERR_Terminate` at any time to cancel the subscription.  All other error codes are ignored.  Fluid callbacks
should call `check(ERR_Terminate)` to perform the equivalent of this behaviour.

To change the interval, call ~UpdateTimer() with the new value.  To release a timer subscription, call
~UpdateTimer() with the resulting SubscriptionID and an Interval of zero.

Timer management is provisioned by the ~ProcessMessages() function.  Failure to regularly process incoming
messages will lead to unreliable timer cycles.  It should be noted that the smaller the Interval that has been used,
the more imperative regular message checking becomes.  Prolonged processing inside a timer routine can also impact on
other timer subscriptions that are waiting to be processed.

-INPUT-
double Interval:   The total number of seconds to wait between timer calls.
ptr(func) Callback: A callback function is required that will be called on each time cycle.
&ptr Subscription: Optional.  The subscription will be assigned an identifier that is returned in this parameter.

-ERRORS-
Okay:
NullArgs:
Args:
ArrayFull: The task's timer array is at capacity - no more subscriptions can be granted.
InvalidState: The subscriber is marked for termination.
SystemLocked:

*********************************************************************************************************************/

ERROR SubscribeTimer(DOUBLE Interval, FUNCTION *Callback, APTR *Subscription)
{
   parasol::Log log(__FUNCTION__);

   if ((!Interval) or (!Callback)) return log.warning(ERR_NullArgs);
   if (Interval < 0) return log.warning(ERR_Args);

   auto subscriber = tlContext->object();
   if (subscriber->collecting()) return log.warning(ERR_InvalidState);

   if (Callback->Type IS CALL_SCRIPT) log.msg(VLF_BRANCH|VLF_FUNCTION|VLF_DEBUG, "Interval: %.3fs", Interval);
   else log.msg(VLF_BRANCH|VLF_FUNCTION|VLF_DEBUG, "Callback: %p, Interval: %.3fs", Callback->StdC.Routine, Interval);

   ThreadLock lock(TL_TIMER, 200);
   if (lock.granted()) {
      LARGE usInterval = (LARGE)(Interval * 1000000.0); // Scale the interval to microseconds
      if (usInterval <= 40000) {
         // TODO: Rapid timers should be synchronised with other existing timers to limit the number of
         // interruptions that occur per second.
      }

      auto it = glTimers.emplace(glTimers.end());
      LARGE subscribed = PreciseTime();
      it->SubscriberID = subscriber->UID;
      it->Interval     = usInterval;
      it->LastCall     = subscribed;
      it->NextCall     = subscribed + usInterval;
      it->Routine      = *Callback;
      it->Locked       = false;
      it->Cycle        = glTimerCycle - 1;

      if (subscriber->UID > 0) it->Subscriber = subscriber;
      else it->Subscriber = NULL;

      // For resource tracking purposes it is important for us to keep a record of the subscription so that
      // we don't treat the object address as valid when it's been removed from the system.

      subscriber->Flags |= NF_TIMER_SUB;

      if (Subscription) *Subscription = &*it;

      return ERR_Okay;
   }
   else return log.warning(ERR_SystemLocked);
}

/*********************************************************************************************************************

-FUNCTION-
PreciseTime: Returns the current system time, in microseconds.

This function returns the current 'system time' in microseconds (1 millionth of a second).  The value is monotonic
if the host platform allows it (typically expressed as the amount of time that has elapsed since the system was
switched on).  The benefit of monotonic time is that it is unaffected by changes to the system clock, such as daylight
savings adjustments or manual changes by the user.

-RESULT-
large: Returns the system time in microseconds.  An error is extremely unlikely, but zero is returned in the event of one.

*********************************************************************************************************************/

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

/*********************************************************************************************************************

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

*********************************************************************************************************************/

ERROR UpdateTimer(APTR Subscription, DOUBLE Interval)
{
   parasol::Log log(__FUNCTION__);

   if (!Subscription) return log.warning(ERR_NullArgs);

   log.msg(VLF_EXTAPI|VLF_BRANCH|VLF_FUNCTION, "Subscription: %p, Interval: %.4f", Subscription, Interval);

   ThreadLock lock(TL_TIMER, 200);
   if (lock.granted()) {
      auto timer = (CoreTimer *)Subscription;
      if (Interval < 0) {
         // Special mode: Preserve existing timer settings for the subscriber (ticker values are not reset etc)
         LARGE usInterval = -((LARGE)(Interval * 1000000.0));
         if (usInterval < timer->Interval) timer->Interval = usInterval;
         return ERR_Okay;
      }
      else if (Interval > 0) {
         LARGE usInterval = (LARGE)(Interval * 1000000.0);
         timer->Interval = usInterval;
         timer->NextCall = PreciseTime() + usInterval;
         return ERR_Okay;
      }
      else {
         if (timer->Locked) {
            // A timer can't be removed during its execution, but we can nullify the function entry
            // and ProcessMessages() will automatically terminate it on the next cycle.
            timer->Routine.Type = 0;
            return log.warning(ERR_AlreadyLocked);
         }

         lock.release();

         if (timer->Routine.Type IS CALL_SCRIPT) {
            scDerefProcedure(timer->Routine.Script.Script, &timer->Routine);
         }

         for (auto it=glTimers.begin(); it != glTimers.end(); it++) {
            if (timer IS &(*it)) {
               glTimers.erase(it);
               break;
            }
         }

         return ERR_Okay;
      }
   }
   else return log.warning(ERR_SystemLocked);
}

/*********************************************************************************************************************

-FUNCTION-
WaitTime: Waits for a specified amount of seconds and/or microseconds.

This function waits for a period of time as specified by the Seconds and MicroSeconds arguments.  While waiting, your
task will continue to process incoming messages in order to prevent the process' message queue from developing a
back-log.

WaitTime() can return earlier than the indicated timeout if a message handler returns ERR_Terminate, or if a `MSGID_QUIT`
message is sent to the task's message queue.

-INPUT-
int Seconds:      The number of seconds to wait for.
int MicroSeconds: The number of microseconds to wait for.  Please note that a microsecond is one-millionth of a second - 1/1000000.  The value cannot exceed 999999.

-END-

*********************************************************************************************************************/

void WaitTime(LONG Seconds, LONG MicroSeconds)
{
   bool processmsg;

   if (!tlMainThread) {
      processmsg = false; // Child threads never process messages.
      if (Seconds < 0) Seconds = -Seconds;
      if (MicroSeconds < 0) MicroSeconds = -MicroSeconds;
    }
   else {
      // If the Seconds or MicroSeconds arguments are negative, turn off the ProcessMessages() support.
      processmsg = true;
      if (Seconds < 0) { Seconds = -Seconds; processmsg = false; }
      if (MicroSeconds < 0) { MicroSeconds = -MicroSeconds; processmsg = false; }
   }

   while (MicroSeconds >= 1000000) {
      MicroSeconds -= 1000000;
      Seconds++;
   }

   if (processmsg) {
      LARGE current = PreciseTime() / 1000LL;
      LARGE end = current + (Seconds * 1000) + (MicroSeconds / 1000);
      do {
         if (ProcessMessages(0, end - current) IS ERR_Terminate) break;
         current = (PreciseTime() / 1000LL);
      } while (current < end);
   }
   else {
      #ifdef __unix__
         struct timespec nano;
         nano.tv_sec  = Seconds;
         nano.tv_nsec = MicroSeconds * 100;
         while (nanosleep(&nano, &nano) == -1) continue;
      #elif _WIN32
         winSleep((Seconds * 1000) + (MicroSeconds / 1000));
      #else
         #warn Platform needs support for WaitTime()
      #endif
   }
}

//********************************************************************************************************************
// NOTE: To be called with TL_OBJECT_LOOKUP only.

void remove_object_hash(OBJECTPTR Object)
{
   parasol::Log log(__FUNCTION__);

   if (Object->UID < 0) return; // Public objects not supported by this function

   OBJECTPTR *list;
   LONG list_size;
   if (!VarGet(glObjectLookup, Object->Stats->Name, (APTR *)&list, &list_size)) {
      list_size = list_size / sizeof(OBJECTPTR);

      LONG count_others = 0;
      if (list_size > 1) {
         for (LONG i=0; i < list_size; i++) {
            if (list[i] IS Object) list[i] = NULL;
            else count_others++;
         }
      }

      if (!count_others) { // If no other objects exist for this key, remove the key.
         VarSet(glObjectLookup, Object->Stats->Name, NULL, 0);
      }
   }
   else log.trace("No hash entry for object '%s'", Object->Stats->Name);
}

//********************************************************************************************************************

void set_object_flags(OBJECTPTR Object, LONG Flags)
{
   parasol::Log log(__FUNCTION__);

   Object->Flags = Flags;

   if (Object->UID < 0) {
      SharedObjectHeader *header;
      if (!AccessMemory(RPM_SharedObjects, MEM_READ, 2000, (void **)&header)) {
         auto pubobj = (SharedObject *)ResolveAddress(header, header->Offset);
         LONG index;
         if (!find_public_object_entry(header, Object->UID, &index)) {
            pubobj[index].Flags = Flags;
         }
         ReleaseMemoryID(RPM_SharedObjects);
      }
      else log.warning("Failed to access the PublicObjects array.");
   }
}
