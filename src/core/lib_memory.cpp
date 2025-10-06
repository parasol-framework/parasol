/*********************************************************************************************************************

The memory management functions provide a comprehensive memory allocation system with automatic ownership tracking,
resource management, and debugging capabilities. The implementation uses platform-specific memory allocation
functions (typically stdlib malloc/free on Linux) with additional framework features for object lifecycle management.

-CATEGORY-
Name: Memory
-END-

*********************************************************************************************************************/

#include <stdlib.h> // Contains free(), malloc() etc

#ifdef _WIN32
#include <malloc.h> // For _aligned_malloc, _aligned_free
#endif

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

// Align to 64-byte cache line boundaries for better performance on modern CPUs
constexpr size_t CACHE_LINE_SIZE = 64;

/*********************************************************************************************************************

-FUNCTION-
AllocMemory: Allocates a managed memory block on the heap.

AllocMemory() provides comprehensive memory allocation with automatic ownership tracking, resource management, and
debugging features. The function allocates a new block of memory and associates it with the current execution context,
allowing it to be automatically cleaned up when the context is destroyed.

Example usage:

<pre>
APTR address;
if (AllocMemory(1000, MEM::DATA, &address, nullptr) == ERR::Okay) {
   // Use memory block...
   FreeResource(address);
}
</pre>

Memory allocation behavior is controlled through MEM flags:

<types lookup="MEM"/>

The function can return both a memory address pointer and a unique memory identifier. For most applications,
retrieving only the address pointer is sufficient. When both parameters are requested, the memory block is
automatically locked, requiring an explicit call to ReleaseMemory() before freeing.

The resulting memory block is zero-initialized unless the `MEM::NO_CLEAR` flag is specified. For large
allocations where initialization overhead is a concern, utilising `MEM::NO_CLEAR` is recommended.

Memory blocks are automatically associated with their owning object context, enabling automatic cleanup when
the owner is destroyed. This prevents memory leaks in object-oriented code.

-INPUT-
int Size:     The size of the memory block in bytes. Must be greater than zero.
int(MEM) Flags: Optional allocation flags controlling behavior and ownership.
&ptr Address: Pointer to store the address of the allocated memory block.
&mem ID:      Pointer to store the unique identifier of the allocated memory block.

-ERRORS-
Okay: Memory block successfully allocated.
Args: Invalid parameters (size <= 0 or both Address and ID are NULL).
AllocMemory: Insufficient memory available for the requested allocation.
ArrayFull: Memory tracking structures are full, preventing allocation tracking.
AccessMemory: Memory block was allocated but could not be locked when both Address and ID were requested.
SystemLocked: Memory management system is currently locked by another thread.
-END-

*********************************************************************************************************************/

ERR AllocMemory(int Size, MEM Flags, APTR *Address, MEMORYID *MemoryID)
{
   pf::Log log(__FUNCTION__);

   if ((Size <= 0) or ((!Address) and (!MemoryID))) {
      log.warning("Bad args - Size %d, Address %p, MemoryID %p", Size, Address, MemoryID);
      return ERR::Args;
   }

   if (MemoryID) *MemoryID = 0;
   if (Address) *Address = nullptr;

   // Determine the object that will own the memory block.  The preferred default is for it to belong to the current context.

   OBJECTID object_id = 0;
   if ((Flags & (MEM::HIDDEN|MEM::UNTRACKED)) != MEM::NIL);
   else if ((Flags & MEM::CALLER) != MEM::NIL) {
      // Rarely used, but this feature allows methods to return memory that is tracked to the caller.
      if (tlContext->stack) object_id = tlContext->stack->resource()->UID;
      else object_id = glCurrentTask->UID;
   }
   else if (tlContext != &glTopContext) object_id = tlContext->resource()->UID;
   else if (glCurrentTask) object_id = glCurrentTask->UID;

   auto full_size = Size + MEMHEADER;
   if ((Flags & MEM::MANAGED) != MEM::NIL) full_size += sizeof(ResourceManager *);

   full_size = ((full_size + CACHE_LINE_SIZE - 1) / CACHE_LINE_SIZE) * CACHE_LINE_SIZE;

   APTR start_mem;
   #ifdef _WIN32
      start_mem = _aligned_malloc(full_size, CACHE_LINE_SIZE);
   #else
      if (posix_memalign(&start_mem, CACHE_LINE_SIZE, full_size) != 0) start_mem = nullptr;
   #endif

   if (!start_mem) {
      log.warning("Failed to allocate %d bytes.", Size);
      return ERR::AllocMemory;
   }

   if ((Flags & MEM::NO_CLEAR) IS MEM::NIL) pf::clearmem(start_mem, full_size);

   APTR data_start = (char *)start_mem + sizeof(int) + sizeof(int); // Skip MEMH and unique ID.
   if ((Flags & MEM::MANAGED) != MEM::NIL) data_start = (char *)data_start + sizeof(ResourceManager *); // Skip managed resource reference.

   if (auto lock = std::unique_lock{glmMemory}) { // To keep threads synced, it is essential that this lock is made early.
      MEMORYID unique_id = glPrivateIDCounter++;

      // Configure the memory header and place boundary cookies at the start and end of the memory block.

      APTR header = start_mem;
      if ((Flags & MEM::MANAGED) != MEM::NIL) {
         ((ResourceManager **)header)[0] = nullptr;
         header = (char *)header + sizeof(ResourceManager *);
      }

      ((int *)header)[0]  = unique_id;
      header = (char *)header + sizeof(int);

      ((int *)header)[0]  = CODE_MEMH;
      header = (char *)header + sizeof(int);

      ((int *)((char *)data_start + Size))[0] = CODE_MEMT;

      // Remember the memory block's details such as the size, ID, flags and object that it belongs to.  This helps us
      // with resource tracking, identifying the memory block and freeing it later on.  Hidden blocks are never recorded.

      if ((Flags & MEM::HIDDEN) IS MEM::NIL) {
         glPrivateMemory.insert(std::pair<MEMORYID, PrivateAddress>(unique_id, PrivateAddress(data_start, unique_id, object_id, (uint32_t)Size, Flags)));
         if ((Flags & MEM::OBJECT) != MEM::NIL) {
            if (object_id) glObjectChildren[object_id].insert(unique_id);
         }
         else glObjectMemory[object_id].insert(unique_id);
      }

      // Gain exclusive access if both the address pointer and memory ID have been specified.

      if ((MemoryID) and (Address)) {
         if ((Flags & MEM::NO_LOCK) != MEM::NIL) *Address = data_start;
         else if (AccessMemory(unique_id, MEM::READ_WRITE, 2000, Address) != ERR::Okay) {
            log.warning("Memory block %d stolen during allocation!", *MemoryID);
            return ERR::AccessMemory;
         }
         *MemoryID = unique_id;
      }
      else {
         if (Address)  *Address  = data_start;
         if (MemoryID) *MemoryID = unique_id;
      }

      if (glShowPrivate) log.pmsg("AllocMemory(%p/#%d, %d, $%.8x, Owner: #%d)", data_start, unique_id, Size, int(Flags), object_id);
      return ERR::Okay;
   }
   else {
      #ifdef _WIN32
         _aligned_free(start_mem);
      #else
         free(start_mem);
      #endif
      return log.warning(ERR::SystemLocked);
   }
}

/*********************************************************************************************************************

-FUNCTION-
CheckMemoryExists: Verifies the existence of a memory block.

CheckMemoryExists() validates whether a memory block with the specified identifier still exists in the system's
memory tracking structures. This function is useful for defensive programming when working with memory identifiers
that may have been freed by other code paths.

-INPUT-
mem ID: The unique identifier of the memory block to verify.

-ERRORS-
True: The memory block exists and is valid.
False: The memory block does not exist or has been freed.
-END-

*********************************************************************************************************************/

ERR CheckMemoryExists(MEMORYID MemoryID)
{
   if (auto lock = std::unique_lock{glmMemory}) {
      if (glPrivateMemory.contains(MemoryID)) return ERR::True;
   }
   return ERR::False;
}

/*********************************************************************************************************************

-FUNCTION-
FreeResource: Safely deallocates memory blocks allocated by AllocMemory().

FreeResource() provides safe deallocation of memory blocks with comprehensive validation and cleanup. The function
accepts memory identifiers for optimal safety, though C++ headers also provide pointer-based variants for convenience.

The deallocation process includes boundary validation to detect buffer overruns, lock-aware deallocation that respects
access counting, resource manager integration for managed memory blocks, and automatic cleanup of ownership tracking
structures.

When a memory block is currently locked (AccessCount > 0), it is marked for delayed collection rather than
immediate deallocation. This prevents use-after-free errors while ensuring eventual cleanup when all references
are released.

Memory corruption detection is performed by validating header and trailer markers. Any detected corruption is
logged as a high-priority error requiring immediate attention, as this indicates potential buffer overrun or
memory management bugs in the application code.

-INPUT-
mem ID: The unique identifier of the memory block to be freed.

-ERRORS-
Okay: The memory block was successfully freed or marked for delayed collection.
NullArgs: Invalid memory identifier provided.
InvalidData: Memory corruption detected - header or trailer markers are damaged.
MemoryDoesNotExist: The specified memory block identifier is not valid or already freed.
SystemLocked: Memory management system is currently locked by another thread.
-END-

*********************************************************************************************************************/

ERR FreeResource(MEMORYID MemoryID)
{
   pf::Log log(__FUNCTION__);

   if (auto lock = std::unique_lock{glmMemory}) {
      auto it = glPrivateMemory.find(MemoryID);
      if ((it != glPrivateMemory.end()) and (it->second.Address)) {
         auto &mem = it->second;

         if (glShowPrivate) log.branch("FreeResource(#%d, %p, Size: %d, $%.8x, Owner: #%d)", MemoryID, mem.Address, mem.Size, int(mem.Flags), mem.OwnerID);
         ERR error = ERR::Okay;
         if (mem.AccessCount > 0) {
            log.msg("Block #%d marked for collection (open count %d).", MemoryID, mem.AccessCount);
            mem.Flags |= MEM::COLLECT;
         }
         else {
            // If the block has a resource manager then call its Free() implementation.

            auto start_mem = (char *)mem.Address - sizeof(int) - sizeof(int);
            if ((mem.Flags & MEM::MANAGED) != MEM::NIL) {
               start_mem -= sizeof(ResourceManager *);
               if (!glCrashStatus) { // Resource managers are not considered safe in an uncontrolled shutdown
                  auto rm = ((ResourceManager **)start_mem)[0];
                  if (rm->Free((APTR)mem.Address) IS ERR::InUse) {
                     // Memory block is in use - the manager will be entrusted to handle this situation appropriately.
                     return ERR::Okay;
                  }
               }
            }

            auto mem_end = ((int8_t *)mem.Address) + mem.Size;

            if (((int *)mem.Address)[-1] != CODE_MEMH) {
               log.warning("Bad header on block #%d, address %p, size %d.", MemoryID, mem.Address, mem.Size);
               error = ERR::InvalidData;
            }

            if (((int *)mem_end)[0] != CODE_MEMT) {
               log.warning("Bad tail on block #%d, address %p, size %d.", MemoryID, mem.Address, mem.Size);
               error = ERR::InvalidData;
               DEBUG_BREAK
            }

            #ifdef _WIN32
               _aligned_free(start_mem);
            #else
               free(start_mem);
            #endif

            if ((mem.Flags & MEM::OBJECT) != MEM::NIL) {
               if (auto it = glObjectChildren.find(mem.OwnerID); it != glObjectChildren.end()) {
                  it->second.erase(MemoryID);
               }
            }
            else if (auto it = glObjectMemory.find(mem.OwnerID); it != glObjectMemory.end()) {
               it->second.erase(MemoryID);
            }

            mem.clear();
            if (glProgramStage != STAGE_SHUTDOWN) glPrivateMemory.erase(MemoryID);
         }

         return error;
      }
      log.traceWarning("Memory ID #%d does not exist.", MemoryID);
      return ERR::MemoryDoesNotExist;
   }
   else return log.warning(ERR::SystemLocked);
}

/*********************************************************************************************************************

-FUNCTION-
MemoryIDInfo: Returns information on memory ID's.

This function returns the attributes of a memory block, including the start address, parent object, memory ID, size
and flags.  The following example illustrates correct use of this function:

<pre>
MemInfo info;
if (!MemoryIDInfo(memid, &info)) {
   log.msg("Memory block #%d is %d bytes large.", info.MemoryID, info.Size);
}
</pre>

If the call fails, the !MemInfo structure's fields will be driven to `NULL` and an error code is returned.

-INPUT-
mem ID: Pointer to a valid memory ID.
buf(struct(MemInfo)) MemInfo:  Pointer to a !MemInfo structure.
structsize Size: Size of the !MemInfo structure.

-ERRORS-
Okay
NullArgs
Args
MemoryDoesNotExist
SystemLocked
-END-

*********************************************************************************************************************/

ERR MemoryIDInfo(MEMORYID MemoryID, MemInfo *MemInfo, int Size)
{
   pf::Log log(__FUNCTION__);

   if ((!MemInfo) or (!MemoryID)) return log.warning(ERR::NullArgs);
   if ((size_t)Size < sizeof(MemInfo)) return log.warning(ERR::Args);

   clearmem(MemInfo, Size);

   if (auto lock = std::unique_lock{glmMemory}) {
      auto mem = glPrivateMemory.find(MemoryID);
      if ((mem != glPrivateMemory.end()) and (mem->second.Address)) {
         MemInfo->Start       = mem->second.Address;
         MemInfo->ObjectID    = mem->second.OwnerID;
         MemInfo->Size        = mem->second.Size;
         MemInfo->AccessCount = mem->second.AccessCount;
         MemInfo->Flags       = mem->second.Flags;
         MemInfo->MemoryID    = mem->second.MemoryID;
         return ERR::Okay;
      }
      else return ERR::MemoryDoesNotExist;
   }
   else return log.warning(ERR::SystemLocked);
}

/*********************************************************************************************************************

-FUNCTION-
MemoryPtrInfo: Returns information on memory addresses.

This function returns the attributes of a memory block.  Information includes the start address, parent object,
memory ID, size and flags of the memory address that you are querying.  The following code segment illustrates
correct use of this function:

<pre>
MemInfo info;
if (!MemoryPtrInfo(ptr, &info)) {
   log.msg("Address %p is %d bytes large.", info.Start, info.Size);
}
</pre>

If the call to MemoryPtrInfo() fails then the !MemInfo structure's fields will be driven to `NULL` and an error code
will be returned.

Please note that referencing by a pointer requires a slow reverse-lookup to be employed in this function's search
routine.  We recommend that calls to this function are avoided unless circumstances absolutely require it.

-INPUT-
ptr Address:  Pointer to a valid memory area.
buf(struct(MemInfo)) MemInfo: Pointer to a !MemInfo structure to be populated.
structsize Size: Size of the !MemInfo structure.

-ERRORS-
Okay
NullArgs
MemoryDoesNotExist

*********************************************************************************************************************/

ERR MemoryPtrInfo(APTR Memory, MemInfo *MemInfo, int Size)
{
   pf::Log log(__FUNCTION__);

   if ((!MemInfo) or (!Memory)) return log.warning(ERR::NullArgs);
   if ((size_t)Size < sizeof(MemInfo)) return log.warning(ERR::Args);

   clearmem(MemInfo, Size);

   // Search private addresses.  This is a bit slow, but if the memory pointer is guaranteed to have
   // come from AllocMemory() then the optimal solution for the client is to pull the ID from
   // (int *)Memory)[-2] first and call MemoryIDInfo() instead.

   if (auto lock = std::unique_lock{glmMemory}) {
      for (const auto & [ id, mem ] : glPrivateMemory) {
         if (Memory IS mem.Address) {
            MemInfo->Start       = Memory;
            MemInfo->ObjectID    = mem.OwnerID;
            MemInfo->Size        = mem.Size;
            MemInfo->AccessCount = mem.AccessCount;
            MemInfo->Flags       = mem.Flags;
            MemInfo->MemoryID    = mem.MemoryID;
            return ERR::Okay;
         }
      }
      log.warning("Private memory address %p is not valid.", Memory);
      return ERR::MemoryDoesNotExist;
   }
   else return log.warning(ERR::SystemLocked);
}

/*********************************************************************************************************************

-FUNCTION-
ReallocMemory: Reallocates memory blocks.

This function is used to reallocate memory blocks to new lengths. You can shrink or expand a memory block as you
wish.  The data of your original memory block will be copied over to the new block.  If the new block is of a
larger size, the left-over bytes will be populated with zero-byte values. If the new block is smaller, you will
lose some of the original data.

The original block will be destroyed as a result of calling this function unless the reallocation process fails, in
which case your existing memory block will remain valid.

-INPUT-
ptr Memory:   Pointer to a memory block obtained from ~AllocMemory().
uint Size:    The size of the new memory block.
!ptr Address: Point to an `APTR` variable to store the resulting pointer to the new memory block.
&mem ID:      Point to a `MEMORYID` variable to store the resulting memory block's unique ID.

-ERRORS-
Okay
Args
NullArgs
AllocMemory
Memory: The memory block to be re-allocated is invalid.
-END-

*********************************************************************************************************************/

ERR ReallocMemory(APTR Address, uint32_t NewSize, APTR *Memory, MEMORYID *MemoryID)
{
   pf::Log log(__FUNCTION__);

   if (Memory) *Memory = Address; // If we fail, the result must be the same memory block

   if ((!Address) or (NewSize <= 0)) {
      log.function("Address: %p, NewSize: %d, &Memory: %p, &MemoryID: %p", Address, NewSize, Memory, MemoryID);
      return log.warning(ERR::Args);
   }

   if ((!Memory) and (!MemoryID)) {
      log.function("Address: %p, NewSize: %d, &Memory: %p, &MemoryID: %p", Address, NewSize, Memory, MemoryID);
      return log.warning(ERR::NullArgs);
   }

   // Check the validity of what we have been sent

   MemInfo meminfo;
   if (MemoryIDInfo(GetMemoryID(Address), &meminfo, sizeof(meminfo)) != ERR::Okay) {
      log.warning("MemoryPtrInfo() failed for address %p.", Address);
      return ERR::Memory;
   }

   if (meminfo.Size IS NewSize) return ERR::Okay;

   if (glShowPrivate) log.branch("Address: %p, NewSize: %d", Address, NewSize);

   // Allocate the new memory block and copy the data across

   if (AllocMemory(NewSize, meminfo.Flags, Memory, MemoryID) IS ERR::Okay) {
      auto copysize = (NewSize < meminfo.Size) ? NewSize : meminfo.Size;
      copymem(Address, *Memory, copysize);

      // Free the old memory block.  If it is locked then we also release it for the caller.

      if (meminfo.AccessCount > 0) ReleaseMemory(Address);
      FreeResource(Address);

      return ERR::Okay;
   }
   else return log.error(ERR::AllocMemory);
}

/*********************************************************************************************************************

-FUNCTION-
SetResourceMgr: Define a resource manager for a memory block originating from ~AllocMemory().

SetResourceMgr() associates a !ResourceManager with a memory block that was allocated with the `MEM::MANAGED` flag.
This allows customised memory management logic to be used when an event is triggered on a memory block, such as
the block being destroyed.  Most commonly, resource managers are used to allow C++ destructors to be integrated with
Parasol's memory management system.

This working example from the XPath module ensures that `XPathNode` objects are properly destructed when passed to 
~FreeResource():

<pre>
static ERR xpnode_free(APTR Address)
{
   ((XPathNode *)Address)->&#126;XPathNode();
   return ERR::Okay;
}

static ResourceManager glNodeManager = {
   "XPathNode",  // Name of the custom resource type
   &xpnode_free  // Custom destructor function
};

   if (AllocMemory(sizeof(XPathNode), MEM::MANAGED, (APTR *)&node, nullptr) IS ERR::Okay) {
      SetResourceMgr(node, &glNodeManager);
      new (node) XPathNode(); // Placement new
   }
</pre>

-INPUT-
ptr Address: The address of a `MEM::MANAGED` memory block allocated by ~AllocMemory().
ptr(struct(ResourceManager)) Manager: Must refer to an initialised ResourceManager structure.

-END-

*********************************************************************************************************************/

void SetResourceMgr(APTR Address, ResourceManager *Manager)
{
   auto address_mgr = (ResourceManager **)((char *)Address - sizeof(int) - sizeof(int) - sizeof(ResourceManager *));
   address_mgr[0] = Manager;
}
