/*********************************************************************************************************************

The memory functions use stdlib.h malloc() and free() to get the memory on Linux.  This can be changed according to the
particular platform.  Where possible it is best to call the host platform's own memory management functions.

-CATEGORY-
Name: Memory
-END-

*********************************************************************************************************************/

#include <stdlib.h> // Contains free(), malloc() etc

#ifdef __unix__
#include <errno.h>
#endif

#ifdef __ANDROID__
#include <android/log.h>
#endif

#include "defs.h"
#include <parasol/modules/core.h>

#define freemem(a)  free(a)

using namespace pf;

#ifdef RANDOMISE_MEM
static void randomise_memory(UBYTE *, ULONG Size);
#else
#define randomise_memory(a,b)
#endif

//********************************************************************************************************************
// This function is called whenever memory blocks are freed.  It is useful for debugging applications that are
// suspected to be using memory blocks after they have been deallocated.  Copies '0xdeadbeef' so that it's obvious.

#ifdef RANDOMISE_MEM
static void randomise_memory(UBYTE *Address, ULONG Size)
{
   if ((Size > RANDOMISE_MEM) or (Size < 8)) return;

   ULONG number = 0xdeadbeef;
   ULONG i;
   for (i=0; i < (Size>>2)-1; i++) {
      ((ULONG *)Address)[i] = number;
   }
}
#endif

/*********************************************************************************************************************

-FUNCTION-
AllocMemory: Allocates a new memory block on the heap.

The AllocMemory() function will allocate a new block of memory on the program's heap.  The client will need to define
the minimum byte Size, optional Flags and a variable to store the resulting Address and/or ID of the memory block.
Here is an example:

<pre>
APTR address;
if (!AllocMemory(1000, MEM_DATA, &address, NULL)) {
   ...
   FreeResource(address);
}
</>

A number of flag definitions are available that affect the memory allocation process.  They are:

<types lookup="MEM"/>

Notice that memory allocation can be returned as an address pointer and/or as a unique memory ID.  Typically a private
address with no ID reference is sufficient.

If the client retrieves both the ID and Address pointer, an internal call will be made to ~AccessMemoryID() to lock the
memory block.  This means that before freeing the memory block the client must call ~ReleaseMemory() to unlock it.
Blocks that are persistently locked will remain in memory until the process is terminated.

Memory that is allocated through AllocMemory() is automatically cleared with zero-byte values.  When allocating large
blocks it may be wise to turn off this feature, achieved by setting the `MEM_NO_CLEAR` flag.

-INPUT-
int Size:     The size of the memory block.
int(MEM) Flags: Optional flags.
&ptr Address: Set this argument to refer to an APTR type to store an address reference to the allocated memory block.
&mem ID:      Set this argument to refer to a MEMORYID type to store a unique ID reference to the allocated memory block.

-ERRORS-
Okay:
Args:
Failed:         The block could not be allocated due to insufficient memory space.
ArrayFull:      Although memory space for the block was available, all available memory records are in use.
SystemCorrupt:  The internal tables that manage memory allocations are corrupt.
AccessMemory:   The block was allocated but access to it was not granted, causing failure.
-END-

*********************************************************************************************************************/

ERROR AllocMemory(LONG Size, LONG Flags, APTR *Address, MEMORYID *MemoryID)
{
   pf::Log log(__FUNCTION__);

   if ((Size <= 0) or ((!Address) and (!MemoryID))) {
      log.warning("Bad args - Size %d, Address %p, MemoryID %p", Size, Address, MemoryID);
      return ERR_Args;
   }

   if (MemoryID) *MemoryID = 0;
   if (Address) *Address = NULL;

   // Determine the object that will own the memory block.  The preferred default is for it to belong to the current context.

   OBJECTID object_id = 0;
   if (Flags & (MEM_HIDDEN|MEM_UNTRACKED));
   else if (Flags & MEM_CALLER) {
      // Rarely used, but this feature allows methods to return memory that is tracked to the caller.
      if (tlContext->Stack) object_id = tlContext->Stack->resource()->UID;
      else object_id = glCurrentTask->UID;
   }
   else if (tlContext != &glTopContext) object_id = tlContext->resource()->UID;
   else if (glCurrentTask) object_id = glCurrentTask->UID;

   LONG full_size = Size + MEMHEADER;
   if (Flags & MEM_MANAGED) full_size += sizeof(ResourceManager *);

   APTR start_mem;
   if (!(Flags & MEM_NO_CLEAR)) start_mem = calloc(1, full_size);
   else start_mem = malloc(full_size);

   if (!start_mem) {
      log.warning("Could not allocate %d bytes.", Size);
      return ERR_AllocMemory;
   }

   APTR data_start = (char *)start_mem + sizeof(LONG) + sizeof(LONG); // Skip MEMH and unique ID.
   if (Flags & MEM_MANAGED) data_start = (char *)data_start + sizeof(ResourceManager *); // Skip managed resource reference.

   ThreadLock lock(TL_PRIVATE_MEM, 4000);
   if (lock.granted()) { // For keeping threads synchronised, it is essential that this lock is made early on.
      MEMORYID unique_id = __sync_fetch_and_add(&glPrivateIDCounter, 1);

      // Configure the memory header and place boundary cookies at the start and end of the memory block.

      APTR header = start_mem;
      if (Flags & MEM_MANAGED) {
         ((ResourceManager **)header)[0] = NULL;
         header = (char *)header + sizeof(ResourceManager *);
      }

      ((LONG *)header)[0]  = unique_id;
      header = (char *)header + sizeof(LONG);

      ((LONG *)header)[0]  = CODE_MEMH;
      header = (char *)header + sizeof(LONG);

      ((LONG *)((char *)start_mem + full_size - 4))[0] = CODE_MEMT;

      // Remember the memory block's details such as the size, ID, flags and object that it belongs to.  This helps us
      // with resource tracking, identifying the memory block and freeing it later on.  Hidden blocks are never recorded.

      if (!(Flags & MEM_HIDDEN)) {
         glPrivateMemory.insert(std::pair<MEMORYID, PrivateAddress>(unique_id, PrivateAddress(data_start, unique_id, object_id, (ULONG)Size, (WORD)Flags)));
         if (Flags & MEM_OBJECT) {
            if (object_id) glObjectChildren[object_id].insert(unique_id);
         }
         else glObjectMemory[object_id].insert(unique_id);
      }

      // Gain exclusive access if both the address pointer and memory ID have been specified.

      if ((MemoryID) and (Address)) {
         if (Flags & MEM_NO_LOCK) *Address = data_start;
         else if (AccessMemoryID(unique_id, MEM_READ_WRITE, 2000, Address) != ERR_Okay) {
            log.warning("Memory block %d stolen during allocation!", *MemoryID);
            return ERR_AccessMemory;
         }
         *MemoryID = unique_id;
      }
      else {
         if (Address)  *Address  = data_start;
         if (MemoryID) *MemoryID = unique_id;
      }

      if (glShowPrivate) log.pmsg("AllocMemory(%p/#%d, %d, $%.8x, Owner: #%d)", data_start, unique_id, Size, Flags, object_id);
      return ERR_Okay;
   }
   else {
      freemem(start_mem);
      return ERR_LockFailed;
   }
}

/*********************************************************************************************************************

-FUNCTION-
CheckMemoryExists: Checks if a memory block still exists.

Use CheckMemoryExists() to confirm if a specific memory block still exists by referencing its ID.

-INPUT-
mem ID: The ID of the memory block that will be checked.

-ERRORS-
Okay: The block exists.
False: The block does not exist.
NullArgs:
SystemCorrupt: The internal memory tables are corrupt.
-END-

*********************************************************************************************************************/

ERROR CheckMemoryExists(MEMORYID MemoryID)
{
   pf::Log log(__FUNCTION__);

   if (!MemoryID) return log.warning(ERR_NullArgs);

   ThreadLock lock(TL_PRIVATE_MEM, 4000);
   if (lock.granted()) {
      if (glPrivateMemory.contains(MemoryID)) return ERR_True;
   }

   return ERR_False;
}

/*********************************************************************************************************************

-FUNCTION-
FreeResource: Frees private memory blocks allocated from AllocMemory().

This function will free a memory block originating from ~AllocMemory().

The process of freeing the block will not necessarily take place immediately.  If the block is locked then
it will be marked for deletion and not removed until the lock count reaches zero.

Crash protection measures are built-in.  If the memory header or tail is missing from the block, it is assumed that a
routine has has over-written the memory boundaries, or the caller is attempting to free a non-existent allocation.
Double-freeing can be caught but is not guaranteed.  Freeing memory blocks that are out of scope will result in
a warning.  All caught errors are reported to the application log and warrant priority attention.

-INPUT-
cptr Address: Points to the start of a memory block to be freed.

-ERRORS-
Okay:
NullArgs:
Memory: The supplied memory address is not a recognised memory block.
-END-

*********************************************************************************************************************/

ERROR FreeResource(const void *Address)
{
   pf::Log log(__FUNCTION__);

   if (!Address) return log.warning(ERR_NullArgs);

   APTR start_mem = (char *)Address - sizeof(LONG) - sizeof(LONG);

   ThreadLock lock(TL_PRIVATE_MEM, 4000);
   if (lock.granted()) {
      // Find the memory block in our registered list.

      LONG id = ((LONG *)start_mem)[0];
      ULONG head = ((LONG *)start_mem)[1];

      auto it = glPrivateMemory.find(id);

      if ((it IS glPrivateMemory.end()) or (!it->second.Address)) {
         if (head IS CODE_MEMH) log.warning("Second attempt at freeing address %p detected.", Address);
         else log.warning("Address %p is not a known private memory block.", Address);
         #ifdef DEBUG
         print_diagnosis(0);
         #endif
         return ERR_Memory;
      }

      auto &mem = it->second;

      if (glShowPrivate) {
         log.pmsg("FreeResource(%p, Size: %d, $%.8x, Owner: #%d)", Address, mem.Size, mem.Flags, mem.OwnerID);
      }

      if ((mem.OwnerID) and (tlContext->object()->UID) and (mem.OwnerID != tlContext->object()->UID)) {
         log.warning("Attempt to free address %p (size %d) owned by #%d.", Address, mem.Size, mem.OwnerID);
      }

      if (mem.AccessCount > 0) {
         log.trace("Address %p owned by #%d marked for deletion (open count %d).", Address, mem.OwnerID, mem.AccessCount);
         mem.Flags |= MEM_DELETE;
         return ERR_Okay;
      }

      // If the block has a resource manager then call its Free() implementation.

      if (mem.Flags & MEM_MANAGED) {
         start_mem = (char *)start_mem - sizeof(ResourceManager *);

         auto rm = ((ResourceManager **)start_mem)[0];
         if ((rm) and (rm->Free)) rm->Free((APTR)Address);
         else log.warning("Resource manager not defined for block #%d.", id);
      }

      auto size = mem.Size;
      BYTE *end = ((BYTE *)Address) + size;

      if (head != CODE_MEMH) log.warning("Bad header on address %p, size %d.", Address, size);
      if (((LONG *)end)[0] != CODE_MEMT) {
         log.warning("Bad tail on address %p, size %d.", Address, size);
         DEBUG_BREAK
      }

      if (mem.Flags & MEM_OBJECT) {
         if (glObjectChildren.contains(mem.OwnerID)) glObjectChildren[mem.OwnerID].erase(id);
      }
      else if (glObjectMemory.contains(mem.OwnerID)) glObjectMemory[mem.OwnerID].erase(id);

      mem.Address  = 0;
      mem.MemoryID = 0;
      mem.OwnerID  = 0;
      mem.Flags    = 0;
      #ifdef __unix__
      mem.ThreadLockID = 0;
      #endif

      randomise_memory((APTR)Address, size);

      freemem(start_mem);

      // NB: Guarantee the stability of glPrivateMemory by not erasing records during shutdown (just clear the values).

      if (glProgramStage != STAGE_SHUTDOWN) {
         glPrivateMemory.erase(id);
      }

      return ERR_Okay;
   }
   else return ERR_LockFailed;
}

/*********************************************************************************************************************

-FUNCTION-
FreeResourceID: Frees memory blocks allocated from AllocMemory().

This function will free a memory block with the ID as the identifier.

The process of freeing the block will not necessarily take place immediately.  If the block is locked then
it will be marked for deletion and not removed until the lock count reaches zero.

-INPUT-
mem ID: The unique ID of the memory block.

-ERRORS-
Okay: The memory block was freed or marked for deletion.
NullArgs
MemoryDoesNotExist
-END-

*********************************************************************************************************************/

ERROR FreeResourceID(MEMORYID MemoryID)
{
   pf::Log log(__FUNCTION__);

   if (glShowPrivate) log.function("#%d", MemoryID);

   ThreadLock lock(TL_PRIVATE_MEM, 4000);
   if (lock.granted()) {
      auto it = glPrivateMemory.find(MemoryID);
      if ((it != glPrivateMemory.end()) and (it->second.Address)) {
         auto &mem = it->second;
         ERROR error = ERR_Okay;
         if (mem.AccessCount > 0) {
            log.msg("Private memory ID #%d marked for deletion (open count %d).", MemoryID, mem.AccessCount);
            mem.Flags |= MEM_DELETE;
         }
         else {
            BYTE *mem_end = ((BYTE *)mem.Address) + mem.Size;

            if (((LONG *)mem.Address)[-1] != CODE_MEMH) {
               log.warning("Bad header on block #%d, address %p, size %d.", MemoryID, mem.Address, mem.Size);
               error = ERR_InvalidData;
            }

            if (((LONG *)mem_end)[0] != CODE_MEMT) {
               log.warning("Bad tail on block #%d, address %p, size %d.", MemoryID, mem.Address, mem.Size);
               error = ERR_InvalidData;
            }

            randomise_memory(mem.Address, mem.Size);
            freemem(((LONG *)mem.Address)-2);

            if (mem.Flags & MEM_OBJECT) {
               if (glObjectChildren.contains(mem.OwnerID)) glObjectChildren[mem.OwnerID].erase(MemoryID);
            }
            else if (glObjectMemory.contains(mem.OwnerID)) glObjectMemory[mem.OwnerID].erase(MemoryID);

            mem.Address  = 0;
            mem.MemoryID = 0;
            mem.OwnerID  = 0;
            mem.Flags    = 0;
            #ifdef __unix__
            mem.ThreadLockID = 0;
            #endif

            if (glProgramStage != STAGE_SHUTDOWN) {
               glPrivateMemory.erase(MemoryID);
            }
         }

         return error;
      }
   }

   log.warning("Memory ID #%d does not exist.", MemoryID);
   return ERR_MemoryDoesNotExist;
}

/*********************************************************************************************************************

-FUNCTION-
MemoryIDInfo: Returns information on memory ID's.

This function returns the attributes of a memory block, including the start address, parent object, memory ID, size
and flags.  The following code segment illustrates correct use of this function:

<pre>
MemInfo info;
if (!MemoryIDInfo(memid, &info)) {
   log.msg("Memory block #%d is %d bytes large.", info.MemoryID, info.Size);
}
</pre>

If the call fails, the MemInfo structure's fields will be driven to NULL and an error code is returned.

-INPUT-
mem ID: Pointer to a valid memory ID.
buf(struct(MemInfo)) MemInfo:  Pointer to a MemInfo structure.
structsize Size: Size of the MemInfo structure.

-ERRORS-
Okay
NullArgs
Args
MemoryDoesNotExist
SystemCorrupt: Internal memory tables are corrupt.
-END-

*********************************************************************************************************************/

ERROR MemoryIDInfo(MEMORYID MemoryID, MemInfo *MemInfo, LONG Size)
{
   pf::Log log(__FUNCTION__);

   if ((!MemInfo) or (!MemoryID)) return log.warning(ERR_NullArgs);
   if ((size_t)Size < sizeof(MemInfo)) return log.warning(ERR_Args);

   ClearMemory(MemInfo, Size);

   ThreadLock lock(TL_PRIVATE_MEM, 4000);
   if (lock.granted()) {
      auto mem = glPrivateMemory.find(MemoryID);
      if ((mem != glPrivateMemory.end()) and (mem->second.Address)) {
         MemInfo->Start       = mem->second.Address;
         MemInfo->ObjectID    = mem->second.OwnerID;
         MemInfo->Size        = mem->second.Size;
         MemInfo->AccessCount = mem->second.AccessCount;
         MemInfo->Flags       = mem->second.Flags;
         MemInfo->MemoryID    = mem->second.MemoryID;
         return ERR_Okay;
      }
      else return ERR_MemoryDoesNotExist;
   }
   else return log.warning(ERR_SystemLocked);
}

/*********************************************************************************************************************

-FUNCTION-
MemoryPtrInfo: Returns information on memory addresses.

This function can be used to get details on the attributes of a memory block.  It will return information on
the start address, parent object, memory ID, size and flags of the memory address that you are querying.  The
following code segment illustrates correct use of this function:

<pre>
MemInfo info;
if (!MemoryPtrInfo(ptr, &info)) {
   log.msg("Address %p is %d bytes large.", info.Start, info.Size);
}
</pre>

If the call to MemoryPtrInfo() fails then the MemInfo structure's fields will be driven to NULL and an error code
will be returned.

Please note that referencing by a pointer requires a slow reverse-lookup to be employed in this function's search
routine.  We recommend that calls to this function are avoided unless circumstances absolutely require it.

-INPUT-
ptr Address:  Pointer to a valid memory area.
buf(struct(MemInfo)) MemInfo: Pointer to a MemInfo structure to be filled out.
structsize Size: Size of the MemInfo structure.

-ERRORS-
Okay
NullArgs
MemoryDoesNotExist

*********************************************************************************************************************/

ERROR MemoryPtrInfo(APTR Memory, MemInfo *MemInfo, LONG Size)
{
   pf::Log log(__FUNCTION__);

   if ((!MemInfo) or (!Memory)) return log.warning(ERR_NullArgs);
   if ((size_t)Size < sizeof(MemInfo)) return log.warning(ERR_Args);

   ClearMemory(MemInfo, Size);

   // Search private addresses.  This is a bit slow, but if the memory pointer is guaranteed to have
   // come from AllocMemory() then the optimal solution for the client is to pull the ID from
   // (LONG *)Memory)[-2] first and call MemoryIDInfo() instead.

   ThreadLock memlock(TL_PRIVATE_MEM, 4000);
   if (memlock.granted()) {
      for (const auto & [ id, mem ] : glPrivateMemory) {
         if (Memory IS mem.Address) {
            MemInfo->Start       = Memory;
            MemInfo->ObjectID    = mem.OwnerID;
            MemInfo->Size        = mem.Size;
            MemInfo->AccessCount = mem.AccessCount;
            MemInfo->Flags       = mem.Flags;
            MemInfo->MemoryID    = mem.MemoryID;
            return ERR_Okay;
         }
      }
   }

   log.warning("Private memory address %p is not valid.", Memory);
   return ERR_MemoryDoesNotExist;
}

/*********************************************************************************************************************

-FUNCTION-
ReallocMemory: Reallocates memory blocks.

This function is used to reallocate memory blocks to new lengths. You can shrink or expand a memory block as you wish.
The data of your original memory block will be copied over to the new block.  If the new block is of a larger size, the
left-over bytes will be filled with zero-byte values. If the new block is smaller, you will lose some of the original
data.

The original block will be destroyed as a result of calling this function unless the reallocation process fails, in
which case your existing memory block will remain valid.

-INPUT-
ptr Memory:   Pointer to a memory block obtained from AllocMemory().
int Size:     The size of the new memory block.
!ptr Address: Point to an APTR variable to store the resulting pointer to the new memory block.
&mem ID:      Point to a MEMORYID variable to store the resulting memory block's unique ID.

-ERRORS-
Okay
Args
NullArgs
AllocMemory
Memory: The memory block to be re-allocated is invalid.
-END-

*********************************************************************************************************************/

ERROR ReallocMemory(APTR Address, LONG NewSize, APTR *Memory, MEMORYID *MemoryID)
{
   pf::Log log(__FUNCTION__);
   MemInfo meminfo;
   ULONG copysize;

   if (Memory) *Memory = Address; // If we fail, the result must be the same memory block

   if ((!Address) or (NewSize <= 0)) {
      log.function("Address: %p, NewSize: %d, &Memory: %p, &MemoryID: %p", Address, NewSize, Memory, MemoryID);
      return log.warning(ERR_Args);
   }

   if ((!Memory) and (!MemoryID)) {
      log.function("Address: %p, NewSize: %d, &Memory: %p, &MemoryID: %p", Address, NewSize, Memory, MemoryID);
      return log.warning(ERR_NullArgs);
   }

   // Check the validity of what we have been sent

   if (MemoryIDInfo(GetMemoryID(Address), &meminfo, sizeof(meminfo)) != ERR_Okay) {
      log.warning("MemoryPtrInfo() failed for address %p.", Address);
      return ERR_Memory;
   }

   if (meminfo.Size IS NewSize) return ERR_Okay;

   if (glShowPrivate) log.branch("Address: %p, NewSize: %d", Address, NewSize);

   // Allocate the new memory block and copy the data across

   if (!AllocMemory(NewSize, meminfo.Flags, Memory, MemoryID)) {
      if (NewSize < meminfo.Size) copysize = NewSize;
      else copysize = meminfo.Size;

      CopyMemory(Address, *Memory, copysize);

      // Free the old memory block.  If it is locked then we also release it for the caller.

      if (meminfo.AccessCount > 0) ReleaseMemory(Address);
      FreeResource(Address);

      return ERR_Okay;
   }
   else return log.error(ERR_AllocMemory);
}

//********************************************************************************************************************
// Internal function to set the manager for an allocated resource.  Note: At this stage managed resources are not to
// be exposed in the published API.

void set_memory_manager(APTR Address, ResourceManager *Manager)
{
   ResourceManager **address_mgr = (ResourceManager **)((char *)Address - sizeof(LONG) - sizeof(LONG) - sizeof(ResourceManager *));
   address_mgr[0] = Manager;
}
