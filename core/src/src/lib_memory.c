/*****************************************************************************

The memory functions use stdlib.h malloc() and free() to get the memory on Linux.  This can be changed according to the
particular platform.  Where possible it is best to call the host platform's own memory management functions.

-CATEGORY-
Name: Memory
-END-

*****************************************************************************/

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

static ERROR add_mem_entry(void);
static volatile BYTE glPrivateCompressed = FALSE;
static volatile WORD glPrivateCompression = 500;

static void compress_public_memory(struct SharedControl *);
static void compress_private_memory(void);
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
INLINE void set_publicmem_lock(struct PublicAddress *Address)
{
   Address->ProcessLockID  = glProcessID;
   Address->ThreadLockID   = get_thread_id();
}
#else
INLINE void set_publicmem_lock(struct PublicAddress *Address)
{
   Address->ProcessLockID = glProcessID;
   Address->ThreadLockID  = get_thread_id();
}
#endif

//****************************************************************************
// This function is called whenever memory blocks are freed.  It is useful for debugging applications that are
// suspected to be using memory blocks after they have been deallocated.  Copies '0xdeadbeef' so that it's obvious.

#ifdef RANDOMISE_MEM
static void randomise_memory(UBYTE *Address, ULONG Size)
{
   if ((Size > RANDOMISE_MEM) OR (Size < 8)) return;

   ULONG number = 0xdeadbeef;
   ULONG i;
   for (i=0; i < (Size>>2)-1; i++) {
      ((ULONG *)Address)[i] = number;
   }
}
#endif

/*****************************************************************************

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

You will notice that you have the option of receiving the memory allocation as an address pointer and/or as a unique
memory ID.  When allocating private memory, you can generally just accept an address result and drive the ID
argument to NULL.  However when allocating public memory, you should always retrieve the ID, and optionally the
Address pointer if you need immediate access to the block.

If the block is allocated as private and you retrieve both the ID and Address pointer, or if the allocation is
public and you choose to retrieve the Address pointer, an internal call will be made to ~AccessMemory() to
lock the memory block and resolve its address.  This means that before freeing the memory block, you must make a call
to the ~ReleaseMemory() function to remove the lock, or it will remain in memory till your Task is terminated.

Memory that is allocated through AllocMemory() is automatically cleared with zero-byte values.  When allocating large
blocks it may be wise to turn off this feature - you can do this by setting the MEM_NO_CLEAR flag.

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

*****************************************************************************/

ERROR AllocMemory(LONG Size, LONG Flags, APTR *Address, MEMORYID *MemoryID)
{
   LONG reserved_id, offset;
   LONG i, memid;
   OBJECTID object_id;
   #ifdef _WIN32
      WINHANDLE handle;
   #endif

   if ((Size <= 0) OR ((!Address) AND (!MemoryID))) {
      LogF("@AllocMemory()","Bad args - Size %d, Address %p, MemoryID %p", Size, Address, MemoryID);
      return ERR_Args;
   }

   if (MemoryID) {
      reserved_id = *MemoryID;
      *MemoryID = 0;
      if (Flags & MEM_RESERVED) {
         if (reserved_id > 0) reserved_id = -reserved_id;
         if (!reserved_id) return LogError(ERH_AllocMemory, ERR_Args);
      }
   }
   else reserved_id = 0;

   if (Address) *Address = NULL;

   // Figure out what object the memory block will belong to.  The preferred default is for it to belong to the current context.

   if (Flags & MEM_HIDDEN)         object_id = 0;
   else if (Flags & MEM_UNTRACKED) object_id = 0;
   else if (Flags & MEM_TASK)      object_id = glCurrentTaskID;
   else if (Flags & MEM_CALLER) {
      if (tlContext->Stack) object_id = tlContext->Stack->Object->UniqueID;
      else object_id = glCurrentTaskID;
   }
   else if (tlContext != &glTopContext) object_id = tlContext->Object->UniqueID;
   else object_id = SystemTaskID;

   // Allocate the memory block according to whether it is public or private.

   if (Flags & MEM_PUBLIC) {
      if (!MemoryID) return LogError(ERH_AllocMemory, ERR_NullArgs);

      if (LOCK_PUBLIC_MEMORY(5000) != ERR_Okay) return LogError(ERH_AllocMemory, ERR_SystemLocked);

      // Check that there is room for more public memory allocations

      if ((glSharedControl->NextBlock < 0) OR (glSharedControl->NextBlock >= glSharedControl->MaxBlocks)) {
         compress_public_memory(glSharedControl);
         if (glSharedControl->NextBlock >= glSharedControl->MaxBlocks) {
            LogF("@AllocPublicMemory","The maximum number of public memory blocks (%d) has been exhausted.", glSharedControl->MaxBlocks);
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
            LogF("@AllocMemory","winAllocPublic() failed.");
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
                     if ((i > 0) AND (!glSharedBlocks[i-1].MemoryID)) {
                        // The block(s) just behind the point of insertion are empty - let's make use of this
                        while ((i > 0) AND (!glSharedBlocks[i-1].MemoryID)) i--;
                     }
                     else {
                        // Make space in the array for our block entry
                        LONG end;
                        for (end=i; (glSharedBlocks[end].MemoryID) AND (end < glSharedControl->NextBlock); end++);
                        if ((end IS glSharedControl->NextBlock) AND (glSharedControl->NextBlock IS glSharedControl->MaxBlocks)) {
                           UNLOCK_PUBLIC_MEMORY();
                           return ERR_ArrayFull;
                        }
                        CopyMemory(glSharedBlocks + i, glSharedBlocks + i + 1, sizeof(struct PublicAddress) * (end - i));
                        if (end IS glSharedControl->NextBlock) glSharedControl->NextBlock++;
                     }
                     blk = i;
                     break;
                  }
                  else offset = glSharedBlocks[i].Offset + glSharedBlocks[i].Size;
               }
            }

            if (i IS glSharedControl->NextBlock) {
               LogF("!AllocMemory","Out of public memory space.  Limited to %d bytes.", INITIAL_PUBLIC_SIZE);
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
                     LogF("@AllocMemory()","shmget(Create, $%.8x, ID %d) %s", memkey, memid, strerror(errno));
                  }
               }
               else {
                  // IPC_RMID usually fails due to permission errors (e.g. the user ran our program as root and is now running it as a normal user).

                  LogF("@AllocMemory","shmctl(Remove, Key $%.8x, ID %d) %s", memkey, memid, strerror(errno));

                  if (Flags & MEM_RESERVED) {
                     UNLOCK_PUBLIC_MEMORY();
                     return ERR_AllocMemory;
                  }
                  else goto retry;
               }
            }
            else {
               LogF("@AllocMemory","shmget(Key $%.8x, ID %d) %s", memkey, memid, strerror(errno));

               if (Flags & MEM_RESERVED) {
                  UNLOCK_PUBLIC_MEMORY();
                  return ERR_AllocMemory;
               }
               else goto retry;
            }
         }
         else LogF("@AllocMemory","shmget(Key $%.8x, ID %d) %s", memkey, memid, strerror(errno));

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
         for (i=0; i < glSharedControl->NextBlock; i++) {
            if (glSharedBlocks[i].MemoryID) {
               if ((current_offset + page_size) < glSharedBlocks[i].Offset) {
                  if ((i > 0) AND (!glSharedBlocks[i-1].MemoryID)) {
                     while ((i > 0) AND (!glSharedBlocks[i-1].MemoryID)) i--;
                  }
                  else CopyMemory(glSharedBlocks + i, glSharedBlocks + i + 1, sizeof(struct PublicAddress) * (glSharedControl->NextBlock - i));

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
            LogF("@AllocPublicMemory","Failed to increase memory pool size to %d bytes.", glSharedControl->MemoryOffset + offset + page_size);
            UNLOCK_PUBLIC_MEMORY();
            return ERR_Failed;
         }

         glSharedControl->PoolSize = offset + page_size;
      }
#endif

      // Record the memory allocation

      ClearMemory(glSharedBlocks + blk, sizeof(struct PublicAddress));

      glSharedBlocks[blk].MemoryID = memid;
      glSharedBlocks[blk].Size     = Size;
      glSharedBlocks[blk].ObjectID = object_id;
      glSharedBlocks[blk].Flags    = Flags;
      glSharedBlocks[blk].Offset   = offset;
      #ifdef _WIN32
         glSharedBlocks[blk].OwnerProcess = glProcessID;
         glSharedBlocks[blk].Handle    = handle;
      #endif

      // Record the task that the memory block should be tracked to.  The current context plays an important part in
      // this, because if the object is shared, then we want the allocation to track back to the task responsible for
      // maintaining that object, rather than having it track back to our own task.

      if (Flags & (MEM_UNTRACKED|MEM_HIDDEN));
      else if ((tlContext->Object->Stats) AND (tlContext->Object->TaskID)) {
         glSharedBlocks[blk].TaskID = tlContext->Object->TaskID;
      }
      else glSharedBlocks[blk].TaskID = glCurrentTaskID;

      // Gain exclusive access if an address pointer has been requested

      if (Address) {
         if (page_memory(glSharedBlocks + blk, Address) != ERR_Okay) {
            ClearMemory(glSharedBlocks + blk, sizeof(struct PublicAddress));
            UNLOCK_PUBLIC_MEMORY();
            LogF("@AllocMemory","Paging the newly allocated block of size %d failed.", Size);
            return ERR_LockFailed;
         }

         if (Flags & MEM_NO_BLOCKING) {
            for (i=0; glTaskEntry->NoBlockLocks[i].MemoryID; i++);
            if (i < MAX_NB_LOCKS) {
               glTaskEntry->NoBlockLocks[i].MemoryID    = glSharedBlocks[blk].MemoryID;
               glTaskEntry->NoBlockLocks[i].AccessCount = 1;
            }
            else {
               LogF("@AllocPublicMemory","Out of memory locks.");
               ClearMemory(glSharedBlocks + blk, sizeof(struct PublicAddress));
               unpage_memory(*Address);
               UNLOCK_PUBLIC_MEMORY();
               return ERR_ArrayFull;
            }
         }
         else set_publicmem_lock(&glSharedBlocks[blk]);

         if (Flags & MEM_TMP_LOCK) tlPreventSleep++;
         glSharedBlocks[blk].AccessCount = 1;
         glSharedBlocks[blk].ContextID = tlContext->Object->UniqueID; // For debugging, indicates the object that acquired the first lock.
         glSharedBlocks[blk].ActionID  = tlContext->Action;           // For debugging.

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

      glSharedControl->BlocksUsed++;
      if (blk IS glSharedControl->NextBlock) glSharedControl->NextBlock++;

      UNLOCK_PUBLIC_MEMORY();

      if (glShowPublic) LogMsg("AllocPublic(#%d, %d, $%.8x, Index: %d, Owner: %d)", *MemoryID, Size, Flags, blk, object_id);

      return ERR_Okay;
   }
   else {
      //LONG aligned_size = ALIGN32(Size);
      LONG full_size = Size + MEMHEADER;
      if (Flags & MEM_MANAGED) full_size += sizeof(struct ResourceManager *);

      APTR start_mem;
      if (!(Flags & MEM_NO_CLEAR)) start_mem = calloc(1, full_size);
      else start_mem = malloc(full_size);

      if (!start_mem) {
         LogF("@AllocMemory","Could not allocate %d bytes.", Size);
         return ERR_AllocMemory;
      }

      APTR data_start = start_mem + sizeof(LONG) + sizeof(LONG); // Skip MEMH and unique ID.
      if (Flags & MEM_MANAGED) data_start += sizeof(struct ResourceManager *); // Skip managed resource reference.

      if (!thread_lock(TL_PRIVATE_MEM, 4000)) { // For keeping threads synchronised, it is essential that this lock is made early on.
         MEMORYID unique_id = __sync_fetch_and_add(&glSharedControl->PrivateIDCounter, 1);

         // Configure the memory header and place boundary cookies at the start and end of the memory block.

         APTR header = start_mem;
         if (Flags & MEM_MANAGED) {
            ((struct ResourceManager **)header)[0] = NULL;
            header += sizeof(struct ResourceManager *);
         }

         ((LONG *)header)[0]  = unique_id;
         header += sizeof(LONG);

         ((LONG *)header)[0]  = CODE_MEMH;
         header += sizeof(LONG);

         ((LONG *)(start_mem + full_size - 4))[0] = CODE_MEMT;

         // Remember the memory block's details such as the size, ID, flags and object that it belongs to.  This helps us
         // with resource tracking, identifying the memory block and freeing it later on.  Hidden blocks are never recorded.

         if (!(Flags & MEM_HIDDEN)) {
            if (!add_mem_entry()) {
               glPrivateMemory[glNextPrivateAddress].Address     = data_start;
               glPrivateMemory[glNextPrivateAddress].MemoryID    = unique_id;
               glPrivateMemory[glNextPrivateAddress].Flags       = Flags;
               glPrivateMemory[glNextPrivateAddress].Size        = Size;
               glPrivateMemory[glNextPrivateAddress].ObjectID    = object_id;
               glPrivateMemory[glNextPrivateAddress].AccessCount = 0;
               #ifdef __unix__
               glPrivateMemory[glNextPrivateAddress].ThreadLockID = 0;
               #endif

               glNextPrivateAddress++;
            }
         }

         // Gain exclusive access if both the address pointer and memory ID have been specified.

         if ((MemoryID) AND (Address)) {
            if (Flags & MEM_NO_LOCK) *Address = data_start;
            else if (AccessMemory(unique_id, MEM_READ_WRITE, 2000, Address) != ERR_Okay) {
               thread_unlock(TL_PRIVATE_MEM);
               LogF("@AllocMemory","Memory block %d stolen during allocation!", *MemoryID);
               return ERR_AccessMemory;
            }
            *MemoryID = unique_id;
         }
         else {
            if (Address)  *Address  = data_start;
            if (MemoryID) *MemoryID = unique_id;
         }

         glPrivateBlockCount++;
         thread_unlock(TL_PRIVATE_MEM);

         if (glShowPrivate) LogMsg("AllocMemory(%p/#%d, %d, $%.8x, Owner: #%d)", data_start, unique_id, Size, Flags, object_id);
         return ERR_Okay;
      }
      else {
         freemem(start_mem);
         return ERR_LockFailed;
      }
   }
}

/*****************************************************************************

-FUNCTION-
CheckMemoryExists: Checks if a memory block still exists.

If you need to know whether or not a specific memory block still exists, you can check by using the CheckMemoryExists()
function.  Simply provide it with the ID of the block that you are interested in and it will return an error code of
ERR_Okay if it is in the system at the time of calling.

-INPUT-
mem ID: The ID of the memory block that you wish to look for.

-ERRORS-
Okay: The block exists.
False: The block does not exist.
NullArgs:
SystemCorrupt: The internal memory tables are corrupt.
-END-

*****************************************************************************/

ERROR CheckMemoryExists(MEMORYID MemoryID)
{
   LONG i;

   if (!MemoryID) return LogError(ERH_CheckMemoryExists, ERR_NullArgs);

   if (MemoryID < 0) {
      if (!LOCK_PUBLIC_MEMORY(5000)) {
         if (find_public_mem_id(glSharedControl, MemoryID, NULL) IS ERR_Okay) {
            UNLOCK_PUBLIC_MEMORY();
            return ERR_True;
         }
         UNLOCK_PUBLIC_MEMORY();
      }
      else LogError(ERH_CheckMemoryExists, ERR_SystemLocked);

      return ERR_False;
   }
   else {
      if (!glPrivateMemory) return LogError(ERH_CheckMemoryExists, ERR_SystemCorrupt);

      if (!thread_lock(TL_PRIVATE_MEM, 4000)) {
         if ((i = find_private_mem_id(MemoryID, NULL)) != -1) {
            thread_unlock(TL_PRIVATE_MEM);
            return ERR_True;
         }
         thread_unlock(TL_PRIVATE_MEM);
      }
   }

   return ERR_False;
}

/*****************************************************************************

-FUNCTION-
CloneMemory: Creates an exact duplicate of an existing memory block.

This function allows you to make a duplicate of any memory block that has been allocated from ~AllocMemory().
The new memory block will be completely identical to the block you have specified, except for the type of memory,
which you can alter through the Flags argument.  The contents of the original memory block will be copied over to the
duplicate block.

You have the option of receiving the cloned memory block as an address pointer and/or as a unique memory ID.  When
allocating private memory, you can request an address result and leave the NewID argument as NULL.  However when
cloning public memory, you should always request the NewID, and optionally the NewAddress if you need immediate access
to the cloned block.

If the block is cloned as private memory and you retrieve both the memory ID and address pointer, or if the allocation
is public and you choose to retrieve the address pointer, an internal call will be made to ~AccessMemory()
to lock the memory block.  This means that before freeing the memory block, you must make a call to the
~ReleaseMemory() function to remove the lock, or it will remain in memory.

Remember to free the cloned memory block when the program no longer requires it.

-INPUT-
ptr Address:     Pointer to the memory block that will be cloned.
int(MEM) Flags:  Optional memory flags to define the type of the new block.
&ptr NewAddress: Refers to a pointer that will store the address of the new memory block.
&mem NewID:      Refers to a MEMORYID type that will store the unique ID of the cloned memory block.

-ERRORS-
Okay:         A duplicate of the memory block was successfully created.
NullArgs:     Invalid arguments were supplied to the function.
AccessMemory: Access to the newly created memory block was denied to the function.
AllocMemory:  Failed to allocate the duplicate memory block.
-END-

*****************************************************************************/

ERROR CloneMemory(APTR Address, LONG Flags, APTR *NewAddress, MEMORYID *MemoryID)
{
   struct MemInfo info;
   APTR clone;

   if (glShowPrivate) LogF("CloneMemory()","Memory: %p, Flags: $%.8x", Address, Flags);

   if ((!Address) OR ((!NewAddress) AND (!MemoryID))) {
      return LogError(ERH_CloneMemory, ERR_NullArgs);
   }

   if (!MemoryPtrInfo(Address, &info, sizeof(info))) {
      if (!AllocMemory(info.Size, Flags|MEM_NO_CLEAR, &clone, MemoryID)) {
         CopyMemory(Address, clone, info.Size);
         if (NewAddress) *NewAddress = clone;
         else ReleaseMemory(clone);
         return ERR_Okay;
      }
      else return LogError(ERH_CloneMemory, ERR_AllocMemory);
   }
   else return LogError(ERH_CloneMemory, ERR_Memory);
}

/*****************************************************************************

-FUNCTION-
FreeResource: Frees private memory blocks allocated from AllocMemory().

This function frees memory areas allocated from ~AllocMemory().  Crash protection is incorporated into
various areas of this function. If the memory header or tail is missing from the block, then it is assumed that a
routine has has over-written the memory boundaries, or you are attempting to free a non-existent allocation.
Such problems are immediately reported to the system debugger.  Bear in mind that it does pay to save your
development work if such a message appears, as it indicates that important memory areas could have been destroyed.

This function only works with private memory blocks.  To free a public memory block, use the ~FreeResourceID()
function instead.

-INPUT-
cptr Address: Points to the start of a memory block to be freed.

-ERRORS-
Okay:
NullArgs:
Memory: The supplied memory address is not a recognised memory block.
-END-

*****************************************************************************/

ERROR FreeResource(const void *Address)
{
   if (!Address) return LogError(ERH_FreeResource, ERR_NullArgs);

   if (!glPrivateMemory) { // This part is required for very early initialisation or expunging of the Core.
      freemem(((LONG *)Address)-2);
      return ERR_Okay;
   }

   APTR start_mem = (APTR)Address - sizeof(LONG) - sizeof(LONG);

   if (!thread_lock(TL_PRIVATE_MEM, 4000)) {
      // Find the memory block in our registered list.  NOTE: If the program passes a public memory address to this
      // function, a crash will occur here because the -2 index will likely cause a segfault.

      LONG id = ((LONG *)start_mem)[0];
      ULONG head = ((LONG *)start_mem)[1];

      LONG pos;
      if ((pos = find_private_mem_id(id, Address)) IS -1) {
         thread_unlock(TL_PRIVATE_MEM);
         if (head IS CODE_MEMH) LogF("@FreeResource","Second attempt at freeing address %p detected.", Address);
         else LogF("@FreeResource","Address %p is not a known private memory block.", Address);
         #ifdef DEBUG
         PrintDiagnosis(0, 0);
         #endif
         return ERR_Memory;
      }

      if (glShowPrivate) {
         LogMsg("FreeResource(%p, Size: %d, $%.8x, Owner: #%d)", Address, glPrivateMemory[pos].Size,
            glPrivateMemory[pos].Flags, glPrivateMemory[pos].ObjectID);
      }

      if ((glPrivateMemory[pos].ObjectID) AND (tlContext->Object->UniqueID) AND (glPrivateMemory[pos].ObjectID != tlContext->Object->UniqueID)) {
         LogF("@FreeResource","Attempt to free address %p (size %d), which is owned by #%d.", Address, glPrivateMemory[pos].Size, glPrivateMemory[pos].ObjectID);
      }

      if (glPrivateMemory[pos].AccessCount > 0) {
         FMSG("FreeResource","Address %p of object #%d marked for deletion (open count %d).", Address,
            glPrivateMemory[pos].ObjectID, glPrivateMemory[pos].AccessCount);

         glPrivateMemory[pos].Flags |= MEM_DELETE;
         thread_unlock(TL_PRIVATE_MEM);
         return ERR_Okay;
      }

      // If the block has a resource manager, call its Free() implementation.

      if (glPrivateMemory[pos].Flags & MEM_MANAGED) {
         start_mem -= sizeof(struct ResourceManager *);

         struct ResourceManager *rm = ((struct ResourceManager **)start_mem)[0];
         if ((rm) AND (rm->Free)) {
            rm->Free((APTR)Address);
            // Reacquire the current block position - it can move if glPrivateMemory was compressed.
            if ((pos = find_private_mem_id(id, Address)) IS -1) {
               thread_unlock(TL_PRIVATE_MEM);
               return LogError(ERH_FreeResource, ERR_SystemCorrupt);
            }
         }
         else LogMsg("@FreeResource","Resource manager not defined for block #%d.", id);
      }

      LONG size = glPrivateMemory[pos].Size;
      BYTE *end  = ((BYTE *)Address) + size;

      if (head != CODE_MEMH) LogF("@FreeResource","Bad header on address %p, size %d.", Address, size);
      if (((LONG *)end)[0] != CODE_MEMT) LogF("@FreeResource","Bad tail on address %p, size %d.", Address, size);

      // Remove all references to this memory block

      glPrivateMemory[pos].Address  = 0;
      glPrivateMemory[pos].MemoryID = 0;
      glPrivateMemory[pos].ObjectID = 0;
      #ifdef __unix__
      glPrivateMemory[pos].ThreadLockID = 0;
      #endif

      glPrivateBlockCount--;
      glPrivateCompressed = FALSE;

      if (--glPrivateCompression <= 0) compress_private_memory();

      randomise_memory((APTR)Address, size);

      freemem(start_mem);

      thread_unlock(TL_PRIVATE_MEM);
      return ERR_Okay;
   }
   else return ERR_LockFailed;
}

/*****************************************************************************

-FUNCTION-
FreeResourceID: Frees public memory blocks allocated from AllocMemory().

When a public memory block is no longer required, it can be freed from the system with this function.  The action of
freeing the block will not necessarily take place immediately - if another task is using the block for example,
deletion cannot occur for safety reasons.  In a case such as this, the block will be marked for deletion and will be
freed once all tasks have stopped using it.

-INPUT-
mem ID: The unique ID of the memory block.

-ERRORS-
Okay: The memory block was freed or marked for deletion.
NullArgs
MemoryDoesNotExist
LockFailed: Failed to lock the public memory controller.
-END-

*****************************************************************************/

ERROR FreeResourceID(MEMORYID MemoryID)
{
   LONG i;

   if (MemoryID < 0) { // Search the public memory control table
      if (!LOCK_PUBLIC_MEMORY(5000)) {
         LONG entry;
         if (!find_public_mem_id(glSharedControl, MemoryID, &entry)) {

            if (glShowPublic) LogMsg("FreeResourceID(#%d, Index %d, Count: %d)", MemoryID, entry, glSharedBlocks[entry].AccessCount);

            if (glSharedBlocks[entry].AccessCount > 0) {
               //if ((glSharedBlocks[entry].Flags & MEM_NO_BLOCKING) AND (glCurrentTaskID) AND
               //    (glSharedBlocks[entry].TaskID IS glCurrentTaskID)) {
                  // Non-blocking memory blocks fall through to the deallocation process regardless of the access count (assuming the block belongs to the task attempting the deallocation).
               //}
               //else {
                  // Mark the block for deletion.  This will leave the block in memory until the ReleaseMemory() function is called, which will then do the actual free.

                  FMSG("FreeResourceID","Public memory ID %d marked for deletion (open count %d).", MemoryID, glSharedBlocks[entry].AccessCount);
                  glSharedBlocks[entry].Flags |= MEM_DELETE;
                  UNLOCK_PUBLIC_MEMORY();
                  return ERR_Okay;
               //}
            }

            glSharedControl->BlocksUsed--;

            #ifdef _WIN32
               APTR pool = ResolveAddress(glSharedControl, glSharedControl->MemoryOffset);

               // Check for an optimised page entry and remove it if we find one.  NOTE: If an entry for this block is found
               // in the page-list, this confirms that the block has been allocated independently of the core memory pool and
               // thus we must unmap it.

               if (!thread_lock(TL_MEMORY_PAGES, 4000)) {
                  for (i=0; i < glTotalPages; i++) {
                     if (glMemoryPages[i].MemoryID IS MemoryID) {
                        #ifdef STATIC_MEMORY_POOL
                           if ((glMemoryPages[i].Address >= pool) AND (glMemoryPages[i].Address < pool + glSharedControl->PoolSize)) {
                              ClearMemory(glMemoryPages + i, sizeof(struct MemoryPage));
                              break;
                           }
                        #endif

                        if (!winUnmapViewOfFile(glMemoryPages[i].Address)) {
                           UBYTE buffer[80];
                           winFormatMessage(0, buffer, sizeof(buffer));
                           LogF("@FreeResourceID","winUnmapViewOfFile(%p) failed: %s", glMemoryPages[i].Address, buffer);
                        }

                        ClearMemory(glMemoryPages + i, sizeof(struct MemoryPage));
                        break;
                     }
                  }
                  thread_unlock(TL_MEMORY_PAGES);
               }

               if (glSharedBlocks[entry].Handle) {
                  if (!winCloseHandle(glSharedBlocks[entry].Handle)) {
                     char buffer[80];
                     winFormatMessage(0, buffer, sizeof(buffer));
                     LogF("@FreeResourceID","winCloseHandle(%d) failed: %s", (LONG)glSharedBlocks[entry].Handle, buffer);
                  }
               }

            #elif USE_SHM
               // Check for an optimised page entry and remove it if we find one.

               if (!thread_lock(TL_MEMORY_PAGES, 4000)) {
                  for (i=0; i < glTotalPages; i++) {
                     if (glMemoryPages[i].MemoryID IS MemoryID) {
                        shmdt(glMemoryPages[i].Address);
                        ClearMemory(glMemoryPages + i, sizeof(struct MemoryPage));
                        break;
                     }
                  }
                  thread_unlock(TL_MEMORY_PAGES);
               }

               // Delete the memory block

               shmctl(glSharedBlocks[entry].Offset, IPC_RMID, NULL);
            #else
               // Do nothing for mmap'ed memory since it uses the offset method
            #endif

            ClearMemory(glSharedBlocks + entry, sizeof(struct PublicAddress));

            UNLOCK_PUBLIC_MEMORY();
            return ERR_Okay;
         }
         UNLOCK_PUBLIC_MEMORY();
      }
      else return LogError(ERH_FreeResourceID, ERR_SystemLocked);
   }
   else if (MemoryID > 0) { // Search the private memory control table
      if (glShowPrivate) LogF("FreeResourceID()","#%d", MemoryID);

      if (!thread_lock(TL_PRIVATE_MEM, 4000)) {
         if ((i = find_private_mem_id(MemoryID, NULL)) != -1) {
            ERROR error = ERR_Okay;
            if (glPrivateMemory[i].AccessCount > 0) {
               FMSG("FreeResourceID","Private memory ID #%d marked for deletion (open count %d).", MemoryID, glPrivateMemory[i].AccessCount);
               glPrivateMemory[i].Flags |= MEM_DELETE;
            }
            else {
               BYTE *mem_end = ((BYTE *)glPrivateMemory[i].Address) + glPrivateMemory[i].Size;

               if (((LONG *)glPrivateMemory[i].Address)[-1] != CODE_MEMH) {
                  LogF("@FreeResourceID","Bad header on block #%d, address %p, size %d.", MemoryID, glPrivateMemory[i].Address, glPrivateMemory[i].Size);
                  error = ERR_InvalidData;
               }

               if (((LONG *)mem_end)[0] != CODE_MEMT) {
                  LogF("@FreeResourceID","Bad tail on block #%d, address %p, size %d.", MemoryID, glPrivateMemory[i].Address, glPrivateMemory[i].Size);
                  error = ERR_InvalidData;
               }

               randomise_memory(glPrivateMemory[i].Address, glPrivateMemory[i].Size);
               freemem(((LONG *)glPrivateMemory[i].Address)-2);

               glPrivateMemory[i].Address  = 0;
               glPrivateMemory[i].MemoryID = 0;
               glPrivateMemory[i].ObjectID = 0;
               #ifdef __unix__
               glPrivateMemory[i].ThreadLockID = 0;
               #endif

               if (--glPrivateCompression <= 0) compress_private_memory();
            }

            thread_unlock(TL_PRIVATE_MEM);
            return error;
         }

         thread_unlock(TL_PRIVATE_MEM);
      }
   }
   else return LogError(ERH_FreeResourceID, ERR_NullArgs);

   LogF("@FreeResourceID","Memory ID #%d does not exist.", MemoryID);
   return ERR_MemoryDoesNotExist;
}

/*****************************************************************************

-FUNCTION-
GetMemAddress: Returns the address of private memory blocks identified by ID.

The GetMemAddress() function provides a fast method for obtaining the address of private memory blocks when only the
memory ID is known.  It may also be used to check the validity of private memory blocks, as it will return NULL if
the memory block no longer exists.

This function does not work on public memory blocks (identified as negative integers).  Use ~AccessMemory()
for resolving public memory addresses.

-INPUT-
mem ID: Reference to a private memory ID.

-RESULT-
ptr: Returns the address of the memory ID, or NULL if the ID does not refer to a private memory block.
-END-

*****************************************************************************/

APTR GetMemAddress(MEMORYID MemoryID)
{
   if (MemoryID > 0) {
      if (!thread_lock(TL_PRIVATE_MEM, 4000)) {
         LONG i;
         if ((i = find_private_mem_id(MemoryID, NULL)) != -1) {
            thread_unlock(TL_PRIVATE_MEM);
            return glPrivateMemory[i].Address;
         }
         thread_unlock(TL_PRIVATE_MEM);
      }
   }
   return NULL;
}

/*****************************************************************************

-FUNCTION-
MemoryIDInfo: Returns information on memory ID's.

This function can be used to get special details on the attributes of a memory block.  It will return information on
the start address, parent object, memory ID, size and flags of the memory block that you are querying.  The following
code segment illustrates correct use of this function:

<pre>
struct MemInfo info;
if (!MemoryIDInfo(memid, &info)) {
   LogMsg("Memory block #%d is %d bytes large.", info.MemoryID, info.Size);
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

*****************************************************************************/

ERROR MemoryIDInfo(MEMORYID MemoryID, struct MemInfo *MemInfo, LONG Size)
{
   LONG i, entry;

   if ((!MemInfo) OR (!MemoryID)) return LogError(ERH_MemoryIDInfo, ERR_NullArgs);
   if (Size < sizeof(struct MemInfo)) return LogError(ERH_MemoryIDInfo, ERR_Args);

   ClearMemory(MemInfo, Size);

   if (MemoryID < 0) { // Search public memory blocks
      if (!LOCK_PUBLIC_MEMORY(5000)) {
         if (!find_public_mem_id(glSharedControl, MemoryID, &entry)) {
            MemInfo->Start       = NULL; // Not applicable for shared memory blocks
            MemInfo->ObjectID    = glSharedBlocks[entry].ObjectID;
            MemInfo->Size        = glSharedBlocks[entry].Size;
            MemInfo->AccessCount = glSharedBlocks[entry].AccessCount;
            MemInfo->Flags       = glSharedBlocks[entry].Flags;
            MemInfo->MemoryID    = MemoryID;
            MemInfo->TaskID      = glSharedBlocks[entry].TaskID;
            MemInfo->Handle = glSharedBlocks[entry].Offset;
            UNLOCK_PUBLIC_MEMORY();
            return ERR_Okay;
         }
         UNLOCK_PUBLIC_MEMORY();
         return ERR_MemoryDoesNotExist;
      }
      else {
         LogF("@MemoryIDInfo()","LOCK_PUBLIC_MEMORY() failed.");
         return ERR_SystemLocked;
      }
   }
   else { // Search private memory blocks
      if (!glPrivateMemory) return LogError(ERH_MemoryIDInfo, ERR_SystemCorrupt);

      if (!thread_lock(TL_PRIVATE_MEM, 4000)) {
         if ((i = find_private_mem_id(MemoryID, NULL)) != -1) {
            MemInfo->Start       = glPrivateMemory[i].Address;
            MemInfo->ObjectID    = glPrivateMemory[i].ObjectID;
            MemInfo->Size        = glPrivateMemory[i].Size;
            MemInfo->AccessCount = glPrivateMemory[i].AccessCount;
            MemInfo->Flags       = glPrivateMemory[i].Flags|MEM_PUBLIC;
            MemInfo->MemoryID    = glPrivateMemory[i].MemoryID;
            MemInfo->TaskID      = glCurrentTaskID;
            MemInfo->Handle      = 0;
            thread_unlock(TL_PRIVATE_MEM);
            return ERR_Okay;
         }
         thread_unlock(TL_PRIVATE_MEM);
         return ERR_MemoryDoesNotExist;
      }
      else return ERR_SystemLocked;
   }
}

/*****************************************************************************

-FUNCTION-
MemoryPtrInfo: Returns information on memory addresses.

This function can be used to get special details on the attributes of a memory block.  It will return information on
the start address, parent object, memory ID, size and flags of the memory address that you are querying.  The
following code segment illustrates correct use of this function:

<pre>
struct MemInfo info;
if (!MemoryPtrInfo(ptr, &info)) {
   LogMsg("Address %p is %d bytes large.", info.Start, info.Size);
}
</pre>

If the call to MemoryPtrInfo() fails, the MemInfo structure's fields will be driven to NULL and an error code will be
returned.

-INPUT-
ptr Address:  Pointer to a valid memory area.
buf(struct(MemInfo)) MemInfo: Pointer to a MemInfo structure to be filled out.
structsize Size: Size of the MemInfo structure.

-ERRORS-
Okay
NullArgs
MemoryDoesNotExist

*****************************************************************************/

ERROR MemoryPtrInfo(APTR Memory, struct MemInfo *MemInfo, LONG Size)
{
   LONG i;

   if ((!MemInfo) OR (!Memory)) return LogError(ERH_MemoryPtrInfo, ERR_NullArgs);
   if (Size < sizeof(struct MemInfo)) return LogError(ERH_MemoryPtrInfo, ERR_Args);

   ClearMemory(MemInfo, Size);

   if (!glPrivateMemory) return LogError(ERH_MemoryPtrInfo, ERR_SystemCorrupt);

   // Determine whether or not the address is public

   BYTE publicmem = FALSE;

   #ifdef STATIC_MEMORY_POOL
      APTR pool = ResolveAddress(glSharedControl, glSharedControl->MemoryOffset);
      if ((Memory >= pool) AND (Memory < pool + glSharedControl->PoolSize)) {
         publicmem = TRUE;
      }
   #else
      if (!thread_lock(TL_MEMORY_PAGES, 4000)) {
         for (i=0; i < glTotalPages; i++) {
            if (glMemoryPages[i].Address IS Memory) {
               publicmem = TRUE;
               break;
            }
         }
         thread_unlock(TL_MEMORY_PAGES);
      }
      else return LogError(ERH_MemoryPtrInfo, ERR_SystemLocked);
   #endif

   if (publicmem IS TRUE) {
      if (!LOCK_PUBLIC_MEMORY(5000)) {
         if ((i = find_public_address(glSharedControl, Memory)) != -1) {
            MemInfo->Start       = Memory;
            MemInfo->ObjectID    = glSharedBlocks[i].ObjectID;
            MemInfo->Size        = glSharedBlocks[i].Size;
            MemInfo->AccessCount = glSharedBlocks[i].AccessCount;
            MemInfo->Flags       = glSharedBlocks[i].Flags;
            MemInfo->MemoryID    = glSharedBlocks[i].MemoryID;
            MemInfo->TaskID      = glSharedBlocks[i].TaskID;
            UNLOCK_PUBLIC_MEMORY();
            return ERR_Okay;
         }

         UNLOCK_PUBLIC_MEMORY();

         LogF("@MemoryPtrInfo()","Unable to resolve public memory address %p.", Memory);
         return ERR_MemoryDoesNotExist;
      }
      else return LogError(ERH_MemoryPtrInfo, ERR_SystemLocked);
   }

   // Search private memory areas.  The speed critical routine is fast, but at the cost of a potential crash if the supplied memory address is invalid.

   if (!thread_lock(TL_PRIVATE_MEM, 4000)) {
      #ifdef __speed__
         if ((i = find_private_mem_id(((LONG *)Memory)[-2], Memory)) != -1) {
            MemInfo->Start       = Memory;
            MemInfo->ObjectID    = glPrivateMemory[i].ObjectID;
            MemInfo->Size        = glPrivateMemory[i].Size;
            MemInfo->AccessCount = glPrivateMemory[i].AccessCount;
            MemInfo->Flags       = glPrivateMemory[i].Flags;
            MemInfo->MemoryID    = glPrivateMemory[i].MemoryID;
            MemInfo->TaskID      = glCurrentTaskID;
            thread_unlock(TL_PRIVATE_MEM);
            return ERR_Okay;
         }
      #else
         for (i=0; i < glNextPrivateAddress; i++) {
            if (Memory IS glPrivateMemory[i].Address) {
               MemInfo->Start       = Memory;
               MemInfo->ObjectID    = glPrivateMemory[i].ObjectID;
               MemInfo->Size        = glPrivateMemory[i].Size;
               MemInfo->AccessCount = glPrivateMemory[i].AccessCount;
               MemInfo->Flags       = glPrivateMemory[i].Flags;
               MemInfo->MemoryID    = glPrivateMemory[i].MemoryID;
               MemInfo->TaskID      = glCurrentTaskID;
               thread_unlock(TL_PRIVATE_MEM);
               return ERR_Okay;
            }
         }
      #endif
      thread_unlock(TL_PRIVATE_MEM);
   }

   LogF("@MemoryPtrInfo()","Private memory address %p is not valid.", Memory);
   return ERR_MemoryDoesNotExist;
}

/*****************************************************************************

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

*****************************************************************************/

ERROR ReallocMemory(APTR Address, LONG NewSize, APTR *Memory, MEMORYID *MemoryID)
{
   struct MemInfo meminfo;
   ULONG copysize;

   if (Memory) *Memory = Address; // If we fail, the result must be the same memory block

   if ((!Address) OR (NewSize <= 0)) {
      LogF("ReallocMemory()","Address: %p, NewSize: %d, &Memory: %p, &MemoryID: %p", Address, NewSize, Memory, MemoryID);
      return LogError(ERH_Realloc, ERR_Args);
   }

   if ((!Memory) AND (!MemoryID)) {
      LogF("ReallocMemory()","Address: %p, NewSize: %d, &Memory: %p, &MemoryID: %p", Address, NewSize, Memory, MemoryID);
      return LogError(ERH_Realloc, ERR_NullArgs);
   }

   // Check the validity of what we have been sent

   if (MemoryPtrInfo(Address, &meminfo, sizeof(meminfo)) != ERR_Okay) {
      LogF("@ReallocMemory","MemoryPtrInfo() failed for address %p.", Address);
      return ERR_Memory;
   }

   if (meminfo.Size IS NewSize) return ERR_Okay;

   if ((glShowPrivate) OR (glShowPublic)) {
      LogF("~ReallocMemory()","Address: %p, NewSize: %d", Address, NewSize);
   }

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

      if ((glShowPrivate) OR (glShowPublic)) LogBack();
      return ERR_Okay;
   }
   else {
      if ((glShowPrivate) OR (glShowPublic)) LogBack();
      return LogError(ERH_Realloc, ERR_AllocMemory);
   }
}

//****************************************************************************
// Compresses the public memory table, keeping it in the correct record order.

static void compress_public_memory(struct SharedControl *Control)
{
   LONG i, j;

   // Find the first empty index.
   for (i=0; (i < Control->NextBlock) AND (glSharedBlocks[i].MemoryID); i++);

   if (i >= Control->NextBlock) return;

   for (j=i+1; j < Control->NextBlock; j++) {
      if (glSharedBlocks[j].MemoryID) { // Move the record at position j to position i
         glSharedBlocks[i] = glSharedBlocks[j];
         ClearMemory(glSharedBlocks + j, sizeof(struct PublicAddress));
         i++;
      }
   }

   Control->NextBlock = i;
}

//****************************************************************************
// Acquire a lock on TL_PRIVATE_MEM before calling this function.

static void compress_private_memory(void)
{
   LogF("4CompressMemory","Starting memory block compression...");

   if (glPrivateCompressed) return; // Do nothing if there are no memory holes

   // Find the first empty index.

   LONG i, j;
   for (i=0; (i < glNextPrivateAddress) AND (glPrivateMemory[i].Address); i++);
   if (i >= glNextPrivateAddress) return;

   for (j=i+1; j < glNextPrivateAddress; j++) {
      if (glPrivateMemory[j].Address) {
         glPrivateMemory[i] = glPrivateMemory[j];  // Move the record at position j to position i
         glPrivateMemory[j].Address  = NULL;  // Kill the moved record at its previous position
         glPrivateMemory[j].MemoryID = 0;
         i++;
      }
   }

   LogF("4CompressMemory","Private memory array compressed from %d entries to %d entries.", glNextPrivateAddress, i);
   glNextPrivateAddress = i;
   glPrivateCompression = 500;
   glPrivateCompressed = TRUE;
}

/*****************************************************************************
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

LONG find_public_address(struct SharedControl *Control, APTR Address)
{
   LONG i, block;

   // Check if the address is in the shared memory pool (in which case it will not be found in the page list).

   // This section is ineffective if the public memory pool is not used for this host system (the PoolSize will be zero).

#ifdef STATIC_MEMORY_POOL
   APTR pool = ResolveAddress(glSharedControl, glSharedControl->MemoryOffset);
   if ((Address >= pool) AND (Address < pool + glSharedControl->PoolSize)) {
      for (block=0; block < Control->NextBlock; block++) {
         if (Address IS ResolveAddress(glSharedControl, glSharedControl->MemoryOffset + glSharedBlocks[block].Offset)) {
            return block;
         }
      }

      // The address is within the public memory pool range, but not paged as a memory block.

      //LogF("@find_public_address","%p address lies in the memory pool area, but is not registered as a block.", Address);
      return -1;
   }
#endif

   // Scan the pages to get the address, then get the equivalent memory ID so that we can scan the public memory list.

   if (!thread_lock(TL_MEMORY_PAGES, 4000)) {
      for (i=0; i < glTotalPages; i++) {
         if (glMemoryPages[i].Address IS Address) {
            if (!glMemoryPages[i].MemoryID) {
               LogF("@find_public_address","Address %p is missing its reference to its memory ID.", Address);
               break; // Drop through for error
            }

            for (block=0; block < Control->NextBlock; block++) {
               if (glSharedBlocks[block].MemoryID IS glMemoryPages[i].MemoryID) {
                  thread_unlock(TL_MEMORY_PAGES);
                  return block;
               }
            }

            LogF("@find_public_address","Address %p, block #%d is paged but is not in the public memory table.", glMemoryPages[i].Address, glMemoryPages[i].MemoryID);

            #ifdef DEBUG
               // Sanity check - are there multiple maps for this address?

               i++;
               while (i < glTotalPages) {
                  if (glMemoryPages[i].Address IS Address) LogF("@find_public_address","Multiple maps found: Address %p, block #%d.", Address, glMemoryPages[i].MemoryID);
                  i++;
               }
            #endif

            break; // Drop through for error
         }
      }
      thread_unlock(TL_MEMORY_PAGES);
   }

   return -1;
}

//****************************************************************************
// Finds private memory blocks via ID.  Ensure that this function is called with a TL_PRIVATE_MEM lock.

LONG find_private_mem_id(MEMORYID MemoryID, const void *CheckAddress)
{
   if (!glPrivateMemory) return -1;

   const LONG MAX_ITERATIONS = 8;

   LONG j;
   LONG floor   = 0;
   LONG ceiling = glNextPrivateAddress;
   LONG i       = ceiling>>1;
   for (j=0; j < MAX_ITERATIONS; j++) {
      while ((!glPrivateMemory[i].MemoryID) AND (i > 0)) i--;
      if (MemoryID < glPrivateMemory[i].MemoryID)      ceiling = i;
      else if (MemoryID > glPrivateMemory[i].MemoryID) floor = i+1;
      else goto found;
      i = floor + ((ceiling - floor)>>1);
  }

   while ((!glPrivateMemory[i].MemoryID) AND (i > 0)) i--;
   if (MemoryID > glPrivateMemory[i].MemoryID) {
      while (i < ceiling) {
         if (glPrivateMemory[i].MemoryID IS MemoryID) goto found;
         i++;
      }
   }
   else {
      while (i >= 0) {
         if (glPrivateMemory[i].MemoryID IS MemoryID) goto found;
         i--;
      }
   }

   // If the memory ID was not found and a check address was supplied, there is a chance that the supplied
   // memory ID is invalid (memory ID's are stored a couple of bytes early, in the header of the memory block).
   // The only way to be sure is to check the memory list to find a matching address.

   #ifdef DEBUG
   if (CheckAddress) {
      for (i=0; i < glNextPrivateAddress; i++) {
         if (glPrivateMemory[i].Address IS CheckAddress) {
            LogF("@FindMemory","Requested private memory block #%d is not registered, but the check address %p is valid (block header corruption?).  Ceiling: %d", MemoryID, CheckAddress, glNextPrivateAddress);
            break;
         }
      }
   }
   #endif

   return -1;

found:
   #ifdef DEBUG
   if (CheckAddress) {
      if (glPrivateMemory[i].Address != CheckAddress) {
         // If the address does not match then the memory ID that we were given was corrupt.

         LogF("@FindMemory","Private memory block #%d is registered as address %p, but cross-check mismatches as %p", MemoryID, glPrivateMemory[i].Address, CheckAddress);

         for (i=0; i < glNextPrivateAddress; i++) {
            if (glPrivateMemory[i].Address IS CheckAddress) {
               LogF("@FindMemory","A registration for check address %p was found with a block ID of #%d, size %d.", CheckAddress, glPrivateMemory[i].MemoryID, glPrivateMemory[i].Size);
               break;
            }
         }

         return -1;
      }
   }
   #endif

   return i;
}

//****************************************************************************
// Internal function to set the manager for an allocated resource.  Note: At this stage managed resources are not to
// be exposed in the published API.

void set_memory_manager(APTR Address, struct ResourceManager *Manager)
{
   struct ResourceManager **address_mgr = Address - sizeof(LONG) - sizeof(LONG) - sizeof(struct ResourceManager *);
   address_mgr[0] = Manager;
}

/*****************************************************************************
** To be used by AllocMemory() only.  Must be used within a TL_PRIVATE_MEM lock.  The original order of the table is
** maintained at all times and this function guarantees that the glPrivateMemory table is sorted by MemoryID - as long
** as MemoryID's are allocated using an incremented counter by AllocMemory().
*/

static ERROR add_mem_entry(void)
{
   if (!glPrivateMemory) return ERR_Okay;

   // If the table is at its limit, compact it first by eliminating entries that no longer contain any data.

   if (glNextPrivateAddress >= glMemRegSize) compress_private_memory();

   LONG blocksize;
   if (glMemRegSize < 3000) blocksize = 1000;
   else if (glMemRegSize < 5000) blocksize = 2000;
   else if (glMemRegSize < 10000) blocksize = 4000;
   else blocksize = 8000;

   if (glNextPrivateAddress >= glMemRegSize - 500) {
      // Table is at capacity.  Allocate more space for new entries

      LogF("7add_mem_entry","Memory array at near capacity (%d blocks) - allocating more space.", glMemRegSize);

      struct PrivateAddress *newmem;
      if ((newmem = malloc(sizeof(struct PrivateAddress) * (glMemRegSize + blocksize)))) {
         CopyMemory(glPrivateMemory, newmem, glMemRegSize * sizeof(struct PrivateAddress));
         ClearMemory(newmem + glMemRegSize, blocksize * sizeof(struct PrivateAddress));
         free(glPrivateMemory);
         glPrivateMemory = newmem;
         glMemRegSize += blocksize;
      }
      else {
         LogF("@add_mem_entry","Failed to increase available memory space.");
         return ERR_Memory;
      }
   }

   // "Pull-back" the NextPrivateEntry position if there are null entries present at the tail-end of the array.

   while ((glNextPrivateAddress > 0) AND (glPrivateMemory[glNextPrivateAddress-1].Address IS NULL)) {
      glNextPrivateAddress--;
   }

   return ERR_Okay;
}

//****************************************************************************
// Finds public memory blocks via ID.  For thread safety, this function should be called within a lock_private_memory() zone.

ERROR find_public_mem_id(struct SharedControl *Control, MEMORYID MemoryID, LONG *EntryPos)
{
   if (EntryPos) *EntryPos = 0;

   LONG block;
   for (block=0; block < Control->NextBlock; block++) {
      if (MemoryID IS glSharedBlocks[block].MemoryID) {
         if (EntryPos) *EntryPos = block;
         return ERR_Okay;
      }
   }

   return ERR_MemoryDoesNotExist;
}

//****************************************************************************
// Returns the address of a public block that has been *mapped*.  If not mapped, then NULL is returned.

APTR resolve_public_address(struct PublicAddress *Block)
{
#ifdef STATIC_MEMORY_POOL
   // Check if the address is in the shared memory pool
   APTR pool = ResolveAddress(glSharedControl, glSharedControl->MemoryOffset);
   APTR address = ResolveAddress(glSharedControl, glSharedControl->MemoryOffset + Block->Offset);
   if ((address >= pool) AND (address < pool + glSharedControl->PoolSize)) {
      return address;
   }
#endif

   // Check memory mapped pages.

   if (!thread_lock(TL_MEMORY_PAGES, 1000)) {
      LONG index;
      for (index=0; index < glTotalPages; index++) {
         if (Block->MemoryID IS glMemoryPages[index].MemoryID) {
            thread_unlock(TL_MEMORY_PAGES);
            return glMemoryPages[index].Address;
         }
      }
      thread_unlock(TL_MEMORY_PAGES);
   }

   return NULL;
}

