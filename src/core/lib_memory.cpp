/*********************************************************************************************************************

The memory functions use stdlib.h malloc() and free() to get the memory on Linux.  This can be changed according to the
particular platform.  Where possible it is best to call the host platform's own memory management functions.

-CATEGORY-
Name: Memory
-END-

*********************************************************************************************************************/

#include <stdlib.h> // Contains free(), malloc() etc

#ifdef __unix__
#include <sys/ipc.h>
#ifndef __ANDROID__
#include <sys/shm.h>
#endif
#include <sys/stat.h>

#include <sys/mman.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#endif

#ifdef __ANDROID__
#include <android/log.h>
#endif

#include "defs.h"
#include <parasol/modules/core.h>

#define freemem(a)  free(a)

using namespace parasol;

static void compress_public_memory(SharedControl *);
#ifdef RANDOMISE_MEM
static void randomise_memory(UBYTE *, ULONG Size);
#else
#define randomise_memory(a,b)
#endif

extern LONG glPageSize;

#ifdef __unix__
extern LONG glMemoryFD;
#endif

// Platform specific function for marking a public memory block as locked.
#ifdef _WIN32
INLINE void set_publicmem_lock(PublicAddress *Address)
{
   Address->ProcessLockID  = glProcessID;
   Address->ThreadLockID   = get_thread_id();
}
#else
INLINE void set_publicmem_lock(PublicAddress *Address)
{
   Address->ProcessLockID = glProcessID;
   Address->ThreadLockID  = get_thread_id();
}
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
address with no ID reference is sufficient.  However when allocating public memory, the ID is essential and optionally
the Address pointer to gain immediate access to the block.

If the block is allocated as private and the caller retrieves both the ID and Address pointer, or if the allocation is
public and the Address pointer is requested, an internal call will be made to ~AccessMemoryID() to lock the memory
block and resolve its address.  This means that before freeing the memory block the caller must call ~ReleaseMemory()
to unlock it.  Blocks that are persistently locked will remain in memory until the process is terminated.

Memory that is allocated through AllocMemory() is automatically cleared with zero-byte values.  When allocating large
blocks it may be wise to turn off this feature, achieved by setting the `MEM_NO_CLEAR` flag.

-INPUT-
int Size:     The size of the memory block.
int(MEM) Flags: Optional flags.
&ptr Address: Set this argument to refer to an APTR type to store an address reference to the allocated memory block.
&mem ID:      Set this argument to refer to a MEMORYID type to store a unique ID reference to the allocated memory block.  This is compulsory when allocating public memory blocks.

-ERRORS-
Okay:
Args:
Failed:         The block could not be allocated due to insufficient memory space.
ArrayFull:      Although memory space for the block was available, all available memory records are in use.
LockFailed:     The function failed to gain access to the public memory controller.
SystemCorrupt:  The internal tables that manage memory allocations are corrupt.
AccessMemory:   The block was allocated but access to it was not granted, causing failure.
ResourceExists: This error is returned if MEM_RESERVED was used and the memory block ID was found to already exist.
-END-

*********************************************************************************************************************/

ERROR AllocMemory(LONG Size, LONG Flags, APTR *Address, MEMORYID *MemoryID)
{
   parasol::Log log(__FUNCTION__);
   LONG offset;
   LONG i, memid;
   OBJECTID object_id;
   #ifdef _WIN32
      WINHANDLE handle;
   #endif

   if ((Size <= 0) or ((!Address) and (!MemoryID))) {
      log.warning("Bad args - Size %d, Address %p, MemoryID %p", Size, Address, MemoryID);
      return ERR_Args;
   }

   LONG reserved_id;
   if (MemoryID) {
      reserved_id = *MemoryID;
      *MemoryID = 0;
      if (Flags & MEM_RESERVED) {
         if (reserved_id > 0) reserved_id = -reserved_id;
         if (!reserved_id) return log.warning(ERR_Args);
      }
   }
   else reserved_id = 0;

   if (Address) *Address = NULL;

   // Figure out what object the memory block will belong to.  The preferred default is for it to belong to the current context.

   if (Flags & MEM_HIDDEN)         object_id = 0;
   else if (Flags & MEM_UNTRACKED) object_id = 0;
   else if (Flags & MEM_CALLER) {
      // Rarely used, but this feature allows methods to return memory that is tracked to the caller.
      if (tlContext->Stack) object_id = tlContext->Stack->resource()->UID;
      else object_id = glCurrentTask->UID;
   }
   else if (tlContext != &glTopContext) object_id = tlContext->resource()->UID;
   else if (glCurrentTask) object_id = glCurrentTask->UID;
   else object_id = 0;

   // Allocate the memory block according to whether it is public or private.

   if (Flags & MEM_PUBLIC) {
      if (!MemoryID) return log.warning(ERR_NullArgs);

      if (LOCK_PUBLIC_MEMORY(5000) != ERR_Okay) return log.warning(ERR_SystemLocked);

      // Check that there is room for more public memory allocations

      if ((glSharedControl->NextBlock < 0) or (glSharedControl->NextBlock >= glSharedControl->MaxBlocks)) {
         compress_public_memory(glSharedControl);
         if (glSharedControl->NextBlock >= glSharedControl->MaxBlocks) {
            log.warning("The maximum number of public memory blocks (%d) has been exhausted.", glSharedControl->MaxBlocks);
            UNLOCK_PUBLIC_MEMORY();
            return ERR_ArrayFull;
         }
      }

      // If the memory block is reserved, check if the ID already exists

      if (Flags & MEM_RESERVED) {
         if (find_public_mem_id(glSharedControl, reserved_id, NULL) IS ERR_Okay) {
            UNLOCK_PUBLIC_MEMORY();
            return ERR_ResourceExists;
         }
      }

#ifdef USE_SHM
retry:
#endif
      if (Flags & MEM_RESERVED) memid = reserved_id;
      else memid = __sync_fetch_and_sub(&glSharedControl->IDCounter, 1);

      LONG blk;

#ifdef _WIN32

      handle = NULL;
      if (Flags & MEM_NO_POOL) {
         if (!(handle = winAllocPublic(Size))) {
            log.warning("winAllocPublic() failed.");
            UNLOCK_PUBLIC_MEMORY();
            return ERR_AllocMemory;
         }
         offset = -1; // An offset of -1 means that the block is not in the pool area
         blk = glSharedControl->NextBlock;
      }
      else {
         offset = 0; // Get the offset of the last block on the list
         for (blk=glSharedControl->NextBlock-1; blk >= 0; blk--) {
            if (glSharedBlocks[blk].MemoryID) {
               if (glSharedBlocks[blk].Offset IS -1) continue;
               offset = glSharedBlocks[blk].Offset + glSharedBlocks[blk].Size;
               break;
            }
         }

         blk = glSharedControl->NextBlock;

         // Please remember that the address list must always be sorted on the offset field

         if ((offset + Size) > glSharedControl->PoolSize) { // Check the memory pool for available space so that we can avoid expanding the size of the pool.
            offset = 0;
            for (i=0; i < glSharedControl->NextBlock; i++) {
               if (glSharedBlocks[i].MemoryID) {
                  if (glSharedBlocks[i].Offset IS -1) continue;
                  if ((offset + Size) < glSharedBlocks[i].Offset) { // We have found some space for our block
                     if ((i > 0) and (!glSharedBlocks[i-1].MemoryID)) {
                        // The block(s) just behind the point of insertion are empty - let's make use of this
                        while ((i > 0) and (!glSharedBlocks[i-1].MemoryID)) i--;
                     }
                     else {
                        // Make space in the array for our block entry
                        LONG end;
                        for (end=i; (glSharedBlocks[end].MemoryID) and (end < glSharedControl->NextBlock); end++);
                        if ((end IS glSharedControl->NextBlock) and (glSharedControl->NextBlock IS glSharedControl->MaxBlocks)) {
                           UNLOCK_PUBLIC_MEMORY();
                           return ERR_ArrayFull;
                        }
                        CopyMemory(glSharedBlocks + i, glSharedBlocks + i + 1, sizeof(PublicAddress) * (end - i));
                        if (end IS glSharedControl->NextBlock) __sync_fetch_and_add(&glSharedControl->NextBlock, 1);
                     }
                     blk = i;
                     break;
                  }
                  else offset = glSharedBlocks[i].Offset + glSharedBlocks[i].Size;
               }
            }

            if (i IS glSharedControl->NextBlock) {
               log.error("Out of public memory space.  Limited to %d bytes.", INITIAL_PUBLIC_SIZE);
               UNLOCK_PUBLIC_MEMORY();
               return ERR_Failed;
            }
         }
      }

#elif USE_SHM
      LONG memkey = SHMKEY + memid;

      // All shm memory blocks are tracked back to the original user ID if we were launched as suid.

      LONG current_uid = -1;
      if (glEUID != glUID) {
         if ((current_uid = geteuid()) != -1) {
            seteuid(glUID);
         }
      }

      offset = shmget(memkey, Size, IPC_CREAT|IPC_EXCL|S_IRWXO|S_IRWXG|S_IRWXU);

      if (current_uid != -1) seteuid(current_uid);

      if (offset IS -1) {
         if (errno IS EEXIST) { // The block exists from an older (probably crashed) process.  Let's kill it and try again!
            if ((offset = shmget(memkey, 1, S_IRWXO|S_IRWXG|S_IRWXU)) != -1) { // Get the shm memory id
               if (!shmctl(offset, IPC_RMID, NULL)) { // Now try to delete it
                  // Now try creating the memory block again
                  if ((offset = shmget(memkey, Size, IPC_CREAT|IPC_EXCL|S_IRWXO|S_IRWXG|S_IRWXU)) IS -1) {
                     log.warning("shmget(Create, $%.8x, ID %d) %s", memkey, memid, strerror(errno));
                  }
               }
               else {
                  // IPC_RMID usually fails due to permission errors (e.g. the user ran our program as root and is now running it as a normal user).

                  log.warning("shm key $%.8x / %d exists from previous run, shmctl(IPC_RMID) reports: %s", memkey, memid, strerror(errno));

                  if (Flags & MEM_RESERVED) {
                     UNLOCK_PUBLIC_MEMORY();
                     return ERR_AllocMemory;
                  }
                  else goto retry;
               }
            }
            else {
               log.warning("shmget(Key $%.8x, ID %d) %s", memkey, memid, strerror(errno));

               if (Flags & MEM_RESERVED) {
                  UNLOCK_PUBLIC_MEMORY();
                  return ERR_AllocMemory;
               }
               else goto retry;
            }
         }
         else log.warning("shmget(Key $%.8x, ID %d) %s", memkey, memid, strerror(errno));

         if (offset IS -1) {
            UNLOCK_PUBLIC_MEMORY();
            return ERR_AllocMemory;
         }
      }

      blk = glSharedControl->NextBlock;

#else

      // Get the offset of the last block on the list

      offset = 0;
      for (blk=glSharedControl->NextBlock-1; blk >= 0; blk--) {
         if (glSharedBlocks[blk].MemoryID) {
            offset = glSharedBlocks[blk].Offset + RoundPageSize(glSharedBlocks[blk].Size);
            break;
         }
      }

      LONG page_size = Size + glPageSize - (Size % glPageSize);
      blk = glSharedControl->NextBlock;

      if ((offset + Size) > glSharedControl->PoolSize) {
         // Check the memory pool for available space so that we can avoid expanding the size of the pool.

         LONG current_offset = 0;
         for (LONG i=0; i < glSharedControl->NextBlock; i++) {
            if (glSharedBlocks[i].MemoryID) {
               if ((current_offset + page_size) < glSharedBlocks[i].Offset) {
                  if ((i > 0) and (!glSharedBlocks[i-1].MemoryID)) {
                     while ((i > 0) and (!glSharedBlocks[i-1].MemoryID)) i--;
                  }
                  else CopyMemory(glSharedBlocks + i, glSharedBlocks + i + 1, sizeof(PublicAddress) * (glSharedControl->NextBlock - i));

                  offset = current_offset;
                  blk = i;
                  break;
               }
               else current_offset = glSharedBlocks[i].Offset + RoundPageSize(glSharedBlocks[i].Size);
            }
         }
      }

      // Expand the size of the page file if the end of the block exceeds the pool's capacity.

      if (offset + page_size > glSharedControl->PoolSize) {
         if (ftruncate(glMemoryFD, glSharedControl->MemoryOffset + offset + page_size) IS -1) {
            log.warning("Failed to increase memory pool size to %d bytes.", glSharedControl->MemoryOffset + offset + page_size);
            UNLOCK_PUBLIC_MEMORY();
            return ERR_Failed;
         }

         glSharedControl->PoolSize = offset + page_size;
      }
#endif

      // Record the memory allocation

      ClearMemory(glSharedBlocks + blk, sizeof(PublicAddress));

      glSharedBlocks[blk].MemoryID = memid;
      glSharedBlocks[blk].Size     = Size;
      glSharedBlocks[blk].ObjectID = object_id;
      glSharedBlocks[blk].Flags    = Flags;
      glSharedBlocks[blk].Offset   = offset;
      #ifdef _WIN32
         glSharedBlocks[blk].OwnerProcess = glProcessID;
         glSharedBlocks[blk].Handle       = handle;
      #endif

      if (Flags & (MEM_UNTRACKED|MEM_HIDDEN));
      else if (glCurrentTask) glSharedBlocks[blk].TaskID = glCurrentTask->UID;

      // Gain exclusive access if an address pointer has been requested

      if (Address) {
         if (page_memory(glSharedBlocks + blk, Address) != ERR_Okay) {
            ClearMemory(glSharedBlocks + blk, sizeof(PublicAddress));
            UNLOCK_PUBLIC_MEMORY();
            log.warning("Paging the newly allocated block of size %d failed.", Size);
            return ERR_LockFailed;
         }

         if (Flags & MEM_NO_BLOCKING) {
            for (i=0; glTaskEntry->NoBlockLocks[i].MemoryID; i++);
            if (i < MAX_NB_LOCKS) {
               glTaskEntry->NoBlockLocks[i].MemoryID    = glSharedBlocks[blk].MemoryID;
               glTaskEntry->NoBlockLocks[i].AccessCount = 1;
            }
            else {
               log.warning("Out of memory locks.");
               ClearMemory(glSharedBlocks + blk, sizeof(PublicAddress));
               unpage_memory(*Address);
               UNLOCK_PUBLIC_MEMORY();
               return ERR_ArrayFull;
            }
         }
         else set_publicmem_lock(&glSharedBlocks[blk]);

         if (Flags & MEM_TMP_LOCK) tlPreventSleep++;
         glSharedBlocks[blk].AccessCount = 1;
         glSharedBlocks[blk].ContextID = tlContext->resource()->UID; // For debugging, indicates the object that acquired the first lock.
         glSharedBlocks[blk].ActionID  = tlContext->Action;          // For debugging.

         if (Flags & MEM_STRING) ((STRING)(*Address))[0] = 0; // Strings are easily 'cleared' by setting the first byte.
         else if (!(Flags & MEM_NO_CLEAR)) {  // Clear the memory block unless told otherwise.
            ClearMemory(*Address, Size);
         }
      }
      else {
         if (!(Flags & MEM_NO_CLEAR)) { // Clearing the memory block will require that we attach and detach quickly
            LONG *memory;
            if (!page_memory(glSharedBlocks + blk, (APTR *)&memory)) {
               if (Flags & MEM_STRING) ((STRING)memory)[0] = 0;
               else ClearMemory(memory, Size);
               unpage_memory(memory);
            }
         }
      }

      *MemoryID = glSharedBlocks[blk].MemoryID;

      __sync_fetch_and_add(&glSharedControl->BlocksUsed, 1);
      if (blk IS glSharedControl->NextBlock) __sync_fetch_and_add(&glSharedControl->NextBlock, 1);

      UNLOCK_PUBLIC_MEMORY();

      if (glShowPublic) log.pmsg("AllocPublic(#%d, %d, $%.8x, Index: %d, Owner: %d)", *MemoryID, Size, Flags, blk, object_id);

      return ERR_Okay;
   }
   else {
      //LONG aligned_size = ALIGN32(Size);
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
         MEMORYID unique_id = __sync_fetch_and_add(&glSharedControl->PrivateIDCounter, 1);

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
   parasol::Log log(__FUNCTION__);

   if (!MemoryID) return log.warning(ERR_NullArgs);

   if (MemoryID < 0) {
      parasol::ScopedSysLock lock(PL_PUBLICMEM, 5000);

      if (lock.granted()) {
         if (find_public_mem_id(glSharedControl, MemoryID, NULL) IS ERR_Okay) {
            return ERR_True;
         }
      }
      else log.warning(ERR_SystemLocked);

      return ERR_False;
   }
   else {
      ThreadLock lock(TL_PRIVATE_MEM, 4000);
      if (lock.granted()) {
         if (glPrivateMemory.contains(MemoryID)) return ERR_True;
      }
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

This function is for private memory blocks only.  To free a public memory block, use ~FreeResourceID().

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
   parasol::Log log(__FUNCTION__);

   if (!Address) return log.warning(ERR_NullArgs);

   APTR start_mem = (char *)Address - sizeof(LONG) - sizeof(LONG);

   ThreadLock lock(TL_PRIVATE_MEM, 4000);
   if (lock.granted()) {
      // Find the memory block in our registered list.  NOTE: If the program passes a public memory address to this
      // function, a crash will occur here because the -2 index will likely cause a segfault.

      LONG id = ((LONG *)start_mem)[0];
      ULONG head = ((LONG *)start_mem)[1];

      auto it = glPrivateMemory.find(id);

      if ((it IS glPrivateMemory.end()) or (!it->second.Address)) {
         if (head IS CODE_MEMH) log.warning("Second attempt at freeing address %p detected.", Address);
         else log.warning("Address %p is not a known private memory block.", Address);
         #ifdef DEBUG
         print_diagnosis(0, 0);
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

This function will free a memory block with the ID as the identifier.  It is a requirement that public memory blocks
are terminated with this function.

The process of freeing the block will not necessarily take place immediately.  If the block is locked then
it will be marked for deletion and not removed until the lock count reaches zero.

-INPUT-
mem ID: The unique ID of the memory block.

-ERRORS-
Okay: The memory block was freed or marked for deletion.
NullArgs
MemoryDoesNotExist
LockFailed: Failed to lock the public memory controller.
-END-

*********************************************************************************************************************/

ERROR FreeResourceID(MEMORYID MemoryID)
{
   parasol::Log log(__FUNCTION__);

   if (MemoryID < 0) { // Search the public memory control table
      ScopedSysLock lock(PL_PUBLICMEM, 5000);
      if (lock.granted()) {
         LONG entry;
         if (!find_public_mem_id(glSharedControl, MemoryID, &entry)) {
            if (glShowPublic) log.pmsg("FreeResourceID(#%d, Index %d, Count: %d)", MemoryID, entry, glSharedBlocks[entry].AccessCount);

            if (glSharedBlocks[entry].AccessCount > 0) {
               //if ((glSharedBlocks[entry].Flags & MEM_NO_BLOCKING) and (glCurrentTask->UID) and
               //    (glSharedBlocks[entry].TaskID IS glCurrentTask->UID)) {
                  // Non-blocking memory blocks fall through to the deallocation process regardless of the access count (assuming the block belongs to the task attempting the deallocation).
               //}
               //else {
                  // Mark the block for deletion.  This will leave the block in memory until the ReleaseMemory() function is called, which will then do the actual free.

                  log.trace("Public memory ID %d marked for deletion (open count %d).", MemoryID, glSharedBlocks[entry].AccessCount);
                  glSharedBlocks[entry].Flags |= MEM_DELETE;
                  return ERR_Okay;
               //}
            }

            __sync_fetch_and_sub(&glSharedControl->BlocksUsed, 1);

            #ifdef _WIN32
               APTR pool = ResolveAddress(glSharedControl, glSharedControl->MemoryOffset);

               // Check for an optimised page entry and remove it if we find one.  NOTE: If an entry for this block is found
               // in the page-list, this confirms that the block has been allocated independently of the core memory pool and
               // thus we must unmap it.

               ThreadLock lock(TL_MEMORY_PAGES, 4000);
               if (lock.granted()) {
                  for (LONG i=0; i < glTotalPages; i++) {
                     if (glMemoryPages[i].MemoryID IS MemoryID) {
                        #ifdef STATIC_MEMORY_POOL
                           if ((glMemoryPages[i].Address >= pool) and (glMemoryPages[i].Address < (char *)pool + glSharedControl->PoolSize)) {
                              ClearMemory(glMemoryPages + i, sizeof(MemoryPage));
                              break;
                           }
                        #endif

                        if (!winUnmapViewOfFile(glMemoryPages[i].Address)) {
                           char buffer[80];
                           winFormatMessage(0, buffer, sizeof(buffer));
                           log.warning("winUnmapViewOfFile(%p) failed: %s", glMemoryPages[i].Address, buffer);
                        }

                        ClearMemory(glMemoryPages + i, sizeof(MemoryPage));
                        break;
                     }
                  }
               }

               if (glSharedBlocks[entry].Handle) {
                  if (!winCloseHandle(glSharedBlocks[entry].Handle)) {
                     char buffer[80];
                     winFormatMessage(0, buffer, sizeof(buffer));
                     log.warning("winCloseHandle(%" PF64 ") failed: %s", (MAXINT)glSharedBlocks[entry].Handle, buffer);
                  }
               }

            #elif USE_SHM
               // Check for an optimised page entry and remove it if we find one.

               ThreadLock lock(TL_MEMORY_PAGES, 4000);
               if (lock.granted()) {
                  for (LONG i=0; i < glTotalPages; i++) {
                     if (glMemoryPages[i].MemoryID IS MemoryID) {
                        shmdt(glMemoryPages[i].Address);
                        ClearMemory(glMemoryPages + i, sizeof(MemoryPage));
                        break;
                     }
                  }
               }

               // Delete the memory block

               shmctl(glSharedBlocks[entry].Offset, IPC_RMID, NULL);
            #else
               // Do nothing for mmap'ed memory since it uses the offset method
            #endif

            ClearMemory(glSharedBlocks + entry, sizeof(PublicAddress));
            return ERR_Okay;
         }
      }
      else return log.warning(ERR_SystemLocked);
   }
   else if (MemoryID > 0) { // Search the private memory control table
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
   }
   else return log.warning(ERR_NullArgs);

   log.warning("Memory ID #%d does not exist.", MemoryID);
   return ERR_MemoryDoesNotExist;
}

/*********************************************************************************************************************

-FUNCTION-
MemoryIDInfo: Returns information on memory ID's.

This function can be used to get special details on the attributes of a memory block.  It will return information on
the start address, parent object, memory ID, size and flags of the memory block that you are querying.  The following
code segment illustrates correct use of this function:

<pre>
MemInfo info;
if (!MemoryIDInfo(memid, &info)) {
   log.msg("Memory block #%d is %d bytes large.", info.MemoryID, info.Size);
}
</pre>

If the call to MemoryIDInfo() fails, the MemInfo structure's fields will be driven to NULL and an error code will be returned.

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

ERROR MemoryIDInfo(MEMORYID MemoryID, struct MemInfo *MemInfo, LONG Size)
{
   parasol::Log log(__FUNCTION__);
   LONG entry;

   if ((!MemInfo) or (!MemoryID)) return log.warning(ERR_NullArgs);
   if ((size_t)Size < sizeof(struct MemInfo)) return log.warning(ERR_Args);

   ClearMemory(MemInfo, Size);

   if (MemoryID < 0) { // Search public memory blocks
      ScopedSysLock lock(PL_PUBLICMEM, 5000);
      if (lock.granted()) {
         if (!find_public_mem_id(glSharedControl, MemoryID, &entry)) {
            MemInfo->Start       = NULL; // Not applicable for shared memory blocks
            MemInfo->ObjectID    = glSharedBlocks[entry].ObjectID;
            MemInfo->Size        = glSharedBlocks[entry].Size;
            MemInfo->AccessCount = glSharedBlocks[entry].AccessCount;
            MemInfo->Flags       = glSharedBlocks[entry].Flags;
            MemInfo->MemoryID    = MemoryID;
            MemInfo->TaskID      = glSharedBlocks[entry].TaskID;
            MemInfo->Handle      = glSharedBlocks[entry].Offset;
            return ERR_Okay;
         }
         else return ERR_MemoryDoesNotExist;
      }
      else return log.warning(ERR_SystemLocked);
   }
   else { // Search private memory blocks
      ThreadLock lock(TL_PRIVATE_MEM, 4000);
      if (lock.granted()) {
         auto mem = glPrivateMemory.find(MemoryID);
         if ((mem != glPrivateMemory.end()) and (mem->second.Address)) {
            MemInfo->Start       = mem->second.Address;
            MemInfo->ObjectID    = mem->second.OwnerID;
            MemInfo->Size        = mem->second.Size;
            MemInfo->AccessCount = mem->second.AccessCount;
            MemInfo->Flags       = mem->second.Flags | MEM_PUBLIC;
            MemInfo->MemoryID    = mem->second.MemoryID;
            MemInfo->TaskID      = 0;
            MemInfo->Handle      = 0;
            return ERR_Okay;
         }
         else return ERR_MemoryDoesNotExist;
      }
      else return log.warning(ERR_SystemLocked);
   }
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

If the call to MemoryPtrInfo() fails then the MemInfo structure's fields will be driven to NULL and an error code will be
returned.

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

ERROR MemoryPtrInfo(APTR Memory, struct MemInfo *MemInfo, LONG Size)
{
   parasol::Log log(__FUNCTION__);
   LONG i;

   if ((!MemInfo) or (!Memory)) return log.warning(ERR_NullArgs);
   if ((size_t)Size < sizeof(struct MemInfo)) return log.warning(ERR_Args);

   ClearMemory(MemInfo, Size);

   // Determine whether or not the address is public

   BYTE publicmem = FALSE;

   #ifdef STATIC_MEMORY_POOL
      APTR pool = ResolveAddress(glSharedControl, glSharedControl->MemoryOffset);
      if ((Memory >= pool) and (Memory < (char *)pool + glSharedControl->PoolSize)) {
         publicmem = TRUE;
      }
   #else
      {
         ThreadLock pagelock(TL_MEMORY_PAGES, 4000);
         if (pagelock.granted()) {
            for (LONG i=0; i < glTotalPages; i++) {
               if (glMemoryPages[i].Address IS Memory) {
                  publicmem = TRUE;
                  break;
               }
            }
         }
         else return log.warning(ERR_SystemLocked);
      }
   #endif

   if (publicmem IS TRUE) {
      ScopedSysLock lock(PL_PUBLICMEM, 5000);
      if (lock.granted()) {
         if ((i = find_public_address(glSharedControl, Memory)) != -1) {
            MemInfo->Start       = Memory;
            MemInfo->ObjectID    = glSharedBlocks[i].ObjectID;
            MemInfo->Size        = glSharedBlocks[i].Size;
            MemInfo->AccessCount = glSharedBlocks[i].AccessCount;
            MemInfo->Flags       = glSharedBlocks[i].Flags;
            MemInfo->MemoryID    = glSharedBlocks[i].MemoryID;
            MemInfo->TaskID      = glSharedBlocks[i].TaskID;
            return ERR_Okay;
         }

         log.warning("Unable to resolve public memory address %p.", Memory);
         return ERR_MemoryDoesNotExist;
      }
      else return log.warning(ERR_SystemLocked);
   }

   // Search private memory areas.  This is a bit slow, but if the memory pointer is guaranteed to have
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
            MemInfo->TaskID      = glCurrentTask->UID;
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

If the memory block is public, the original address will be automatically released and freed on success.  The ID of the
original memory block will also be preserved in the new block when reallocating public memory.

-INPUT-
ptr Memory:  Pointer to a memory block obtained from AllocMemory().
int Size:    The size of the new memory block.
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
   parasol::Log log(__FUNCTION__);
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

   if (MemoryPtrInfo(Address, &meminfo, sizeof(meminfo)) != ERR_Okay) {
      log.warning("MemoryPtrInfo() failed for address %p.", Address);
      return ERR_Memory;
   }

   if (meminfo.Size IS NewSize) return ERR_Okay;

   if ((glShowPrivate) or (glShowPublic)) log.branch("Address: %p, NewSize: %d", Address, NewSize);

   // Allocate the new memory block and copy the data across

   if (!AllocMemory(NewSize, meminfo.Flags, Memory, MemoryID)) {
      if (NewSize < meminfo.Size) copysize = NewSize;
      else copysize = meminfo.Size;

      CopyMemory(Address, *Memory, copysize);

      // Free the old memory block.  If it is public or locked then we also release it for the caller.

      if (meminfo.Flags & MEM_PUBLIC) {
         ReleaseMemory(Address);
         FreeResourceID(meminfo.MemoryID);
      }
      else if (meminfo.AccessCount > 0) {
         ReleaseMemory(Address);
         FreeResource(Address);
      }
      else FreeResource(Address);

      return ERR_Okay;
   }
   else return log.error(ERR_AllocMemory);
}

//********************************************************************************************************************
// Compresses the public memory table, keeping it in the correct record order.

static void compress_public_memory(SharedControl *Control)
{
   LONG i;

   // Find the first empty index.
   for (i=0; (i < Control->NextBlock) and (glSharedBlocks[i].MemoryID); i++);

   if (i >= Control->NextBlock) return;

   for (LONG j=i+1; j < Control->NextBlock; j++) {
      if (glSharedBlocks[j].MemoryID) { // Move the record at position j to position i
         glSharedBlocks[i] = glSharedBlocks[j];
         ClearMemory(glSharedBlocks + j, sizeof(PublicAddress));
         i++;
      }
   }

   Control->NextBlock = i;
}

/*********************************************************************************************************************
** This function works on the principle that the glSharedBlocks memory array is sorted by the address offset.  This
** function is known to be utilised by ReleaseMemory() and MemoryPtrInfo().
**
** Please note that page_memory() is responsible for managing the pages that this function needs to reference.
**
** Return Codes
** ------------
** -1 : Total failure.
** -2 : The address is within the public memory pool range, but not registered as a memory block.
*/

LONG find_public_address(SharedControl *Control, APTR Address)
{
   parasol::Log log;

   // Check if the address is in the shared memory pool (in which case it will not be found in the page list).

   // This section is ineffective if the public memory pool is not used for this host system (the PoolSize will be zero).

#ifdef STATIC_MEMORY_POOL
   APTR pool = ResolveAddress(glSharedControl, glSharedControl->MemoryOffset);
   if ((Address >= pool) and (Address < (char *)pool + glSharedControl->PoolSize)) {
      for (LONG block=0; block < Control->NextBlock; block++) {
         if (Address IS ResolveAddress(glSharedControl, glSharedControl->MemoryOffset + glSharedBlocks[block].Offset)) {
            return block;
         }
      }

      // The address is within the public memory pool range, but not paged as a memory block.

      //log.warning("%p address lies in the memory pool area, but is not registered as a block.", Address);
      return -1;
   }
#endif

   // Scan the pages to get the address, then get the equivalent memory ID so that we can scan the public memory list.

   ThreadLock lock(TL_MEMORY_PAGES, 4000);
   if (lock.granted()) {
      for (LONG i=0; i < glTotalPages; i++) {
         if (glMemoryPages[i].Address IS Address) {
            if (!glMemoryPages[i].MemoryID) {
               log.warning("Address %p is missing its reference to its memory ID.", Address);
               break; // Drop through for error
            }

            for (LONG block=0; block < Control->NextBlock; block++) {
               if (glSharedBlocks[block].MemoryID IS glMemoryPages[i].MemoryID) {
                  return block;
               }
            }

            log.warning("Address %p, block #%d is paged but is not in the public memory table.", glMemoryPages[i].Address, glMemoryPages[i].MemoryID);

            #ifdef DEBUG
               // Sanity check - are there multiple maps for this address?

               i++;
               while (i < glTotalPages) {
                  if (glMemoryPages[i].Address IS Address) log.warning("Multiple maps found: Address %p, block #%d.", Address, glMemoryPages[i].MemoryID);
                  i++;
               }
            #endif

            break; // Drop through for error
         }
      }
   }

   return -1;
}

//********************************************************************************************************************
// Internal function to set the manager for an allocated resource.  Note: At this stage managed resources are not to
// be exposed in the published API.

void set_memory_manager(APTR Address, ResourceManager *Manager)
{
   ResourceManager **address_mgr = (ResourceManager **)((char *)Address - sizeof(LONG) - sizeof(LONG) - sizeof(ResourceManager *));
   address_mgr[0] = Manager;
}

//********************************************************************************************************************
// Finds public memory blocks via ID.  For thread safety, this function should be called within a lock_private_memory() zone.

ERROR find_public_mem_id(SharedControl *Control, MEMORYID MemoryID, LONG *EntryPos)
{
   if (EntryPos) *EntryPos = 0;

#if 0
   // Binary search disabled because glSortedBlocks is not implemented yet.
   LONG floor = 0;
   LONG ceiling = Control->NextBlock;
   while (floor < ceiling) {
      LONG i = (floor + ceiling)>>1;
      if (MemoryID < glSortedBlocks[i].MemoryID) floor = i + 1;
      else if (MemoryID > glSortedBlocks[i].MemoryID) ceiling = i;
      else {
         if (EntryPos) *EntryPos = glSortedBlocks[i].Index;
         return ERR_Okay;
      }
   }
#else
   for (LONG block=0; block < Control->NextBlock; block++) {
      if (MemoryID IS glSharedBlocks[block].MemoryID) {
         if (EntryPos) *EntryPos = block;
         return ERR_Okay;
      }
   }
#endif

   return ERR_MemoryDoesNotExist;
}

//********************************************************************************************************************
// Returns the address of a public block that has been *mapped*.  If not mapped, then NULL is returned.

APTR resolve_public_address(PublicAddress *Block)
{
#ifdef STATIC_MEMORY_POOL
   // Check if the address is in the shared memory pool
   char *pool    = (char *)ResolveAddress(glSharedControl, glSharedControl->MemoryOffset);
   char *address = (char *)ResolveAddress(glSharedControl, glSharedControl->MemoryOffset + Block->Offset);
   if ((address >= pool) and (address < pool + glSharedControl->PoolSize)) {
      return address;
   }
#endif

   // Check memory mapped pages.

   ThreadLock lock(TL_MEMORY_PAGES, 1000);
   if (lock.granted()) {
      for (LONG index=0; index < glTotalPages; index++) {
         if (Block->MemoryID IS glMemoryPages[index].MemoryID) {
            return glMemoryPages[index].Address;
         }
      }
   }

   return NULL;
}

