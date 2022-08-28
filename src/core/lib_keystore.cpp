/****************************************************************************

The source code of the Parasol Framework is made publicly available under the
terms described in the LICENSE.TXT file that is distributed with this package.
Please refer to it for further information on licensing.

-CATEGORY-
Name: KeyStore
-END-

Each key-value pair is stored as an independent block of allocated memory from malloc() to minimise resource usage and
tracking requirements.

Keys cannot be deleted, only marked as dead and then the space is reclaimed when the KeyStore is rehashed.

*****************************************************************************/

#include "defs.h"

#include <stdlib.h>
#include <string.h>

#define KPF_STRING    0x0001
#define KPF_DEAD      0x0002
#define KPF_PREHASHED 0x0004 // Pre-hashed keys have no string name.

struct KeyPair {
   UWORD ValueOffset;
   UWORD Flags;
   ULONG KeyHash;
   ULONG ValueLength;
   // Key name follows
};

INLINE CSTRING GET_KEY_VALUE(KeyPair *KP) { return (CSTRING)(((char *)KP) + KP->ValueOffset); }
INLINE CSTRING GET_KEY_NAME(KeyPair *KP) { return (KP->Flags & KPF_PREHASHED) ? NULL : ((CSTRING)(KP + 1)); }
INLINE void DEAD_KEY(KeyPair *KP) { KP->Flags |= KPF_DEAD; }
INLINE LONG CHECK_DEAD_KEY(KeyPair *KP) { return (KP->Flags & KPF_DEAD) != 0; }
INLINE LONG GET_KEY_SIZE(KeyPair *KP) { return KP->ValueOffset + KP->ValueLength; }

#define INITIAL_SIZE 128 // The table size must always be a power of 2
#define BUCKET_SIZE 8

#define MOD_TABLESIZE(val,size) ((val) & (size - 1))
#define HEAD_SIZE sizeof(KeyPair)

//****************************************************************************
// Resource management for KeyStore.

static void KeyStore_free(APTR Address)
{
   parasol::Log log(__FUNCTION__);
   KeyStore *store = (KeyStore *)Address;

   if (store->Flags & KSF_AUTO_REMOVE) {
      ULONG key = 0;
      void **ptr;
      LONG size;
      while (!KeyIterate(store, key, &key, (APTR *)&ptr, &size)) {
         if (size IS sizeof(APTR)) FreeResource(ptr[0]);
         else log.trace("Key $%.8x has unexpected size %d", key, size);
      }
   }

   if (store->Data) {
      for (LONG i=0; i < store->TableSize; i++) {
         if (store->Data[i]) free((APTR)store->Data[i]);
      }
      free(store->Data);
      store->Data = NULL;
   }

   if ((store->Flags & KSF_THREAD_SAFE) and (store->Mutex)) {
      FreeMutex(store->Mutex);
      store->Mutex = NULL;
   }
}

static ResourceManager glResourceKeyStore = {
   "KeyStore",
   &KeyStore_free
};

//****************************************************************************

INLINE LONG hm_hash_index(LONG TableSize, ULONG KeyHash) {
   ULONG hash = KeyHash * PRIME_HASH; // This operation greatly improves hash distribution with a more even spread.
   return MOD_TABLESIZE(hash, TableSize);
}

//****************************************************************************

static KeyPair * build_key_pair(KeyStore *Store, CSTRING Key, const void *Value, LONG Length)
{
   LONG key_len = StrLength(Key) + 1;
   KeyPair *kp;
   if ((kp = (KeyPair *)malloc(HEAD_SIZE + key_len + Length))) {
      kp->ValueOffset = HEAD_SIZE + key_len;
      kp->Flags       = 0;
      kp->KeyHash     = StrHash(Key, (Store->Flags & KSF_CASE) != 0);
      kp->ValueLength = Length;
      CopyMemory(Key, kp + 1, key_len);
      if (Value) CopyMemory(Value, (STRING)GET_KEY_VALUE(kp), Length);
      return kp;
   }
   else return NULL;
}

static KeyPair * build_hashed_key_pair(KeyStore *Store, ULONG Key, const void *Value, LONG Length)
{
   KeyPair *kp;
   if ((kp = (KeyPair *)malloc(HEAD_SIZE + 1 + Length))) {
      kp->ValueOffset = HEAD_SIZE + 1;
      kp->Flags       = KPF_PREHASHED;
      kp->KeyHash     = Key;
      kp->ValueLength = Length;
      ((UBYTE *)(kp + 1))[0] = 0;
      if (Value) CopyMemory(Value, (STRING)GET_KEY_VALUE(kp), Length);
      return kp;
   }
   else return NULL;
}

//****************************************************************************
// Doubles the size of the hashmap, and rehashes all the elements

static ERROR hm_rehash(KeyStore *Store)
{
   parasol::Log log(__FUNCTION__);
   KeyPair **nv;
   KeyPair **old = Store->Data;
   LONG old_size = Store->TableSize;
   LONG new_size = 2 * Store->TableSize; // Doubling the table size ensures that it's always a power of 2.

   log.traceBranch("Store: %p", Store);
retry:
   if ((nv = (KeyPair **)malloc(new_size * sizeof(KeyPair **)))) {
      ClearMemory(nv, new_size * sizeof(KeyPair **));
      for (LONG i=0; i < old_size; i++) {
         if (old[i]) {
            // Check for dead keys during rehashing - these must be removed.

            if (CHECK_DEAD_KEY(old[i])) {
               free((APTR)old[i]);
               old[i] = NULL;
               continue;
            }

            // Find a place to store this value in the new hashmap.

            LONG index = hm_hash_index(new_size, old[i]->KeyHash);
            LONG c;
            for (c=0; c < BUCKET_SIZE; c++) {
               if (!nv[index]) { // Empty key found.
                  nv[index] = old[i];
                  break;
               }
               index = MOD_TABLESIZE(index + 1, new_size);
            }

            if (c >= BUCKET_SIZE) { // Rare case of too many hash collisions - will need to re-expand the list.
               new_size = new_size * 2;
               free(nv);
               goto retry;
            }
         }
      }

      Store->Data = nv;
      Store->TableSize = new_size;
      free(old);

      return ERR_Okay;
   }
   else return ERR_AllocMemory;
}

//****************************************************************************
// Used for setting values in the map, this returns the integer of the location to store a new item, or -1.

static LONG hm_newkey(KeyStore *Store, KeyPair *KeyPair)
{
   parasol::Log log(__FUNCTION__);

   if (Store->Total >= Store->TableSize / 2) {
      log.trace("Hashmap is full and requires expansion.");
      return -1;
   }

   LONG index = hm_hash_index(Store->TableSize, KeyPair->KeyHash);
   for (LONG i=0; i < BUCKET_SIZE; i++) {
      if (!Store->Data[index]) return index;
      index = MOD_TABLESIZE(index + 1, Store->TableSize);
   }
   return -1;
}

//****************************************************************************
// Note: It is presumed that you have checked for duplicates because this routine does not check for an existing key
// with the same name.

static LONG hm_put(KeyStore *Store, KeyPair *KeyPair)
{
   LONG index = hm_newkey(Store, KeyPair); // Find a place to put our value
   while (index IS -1) {
      if (hm_rehash(Store) IS ERR_AllocMemory) return -1;
      index = hm_newkey(Store, KeyPair);
   }

   Store->Data[index] = KeyPair;
   Store->Total++;
   return index;
}

//****************************************************************************
// Get the index of a key, or -1 if it's not present.  Note: Dead keys are not ignored, so can be returned.
// This function also breaks immediately if a NULL pointer is found in the bucket.  This is a useful optimisation, but
// presumes that keys are never removed and that they can only become 'dead'.  Such keys are cleaned up during a rehash.

static LONG hm_get(KeyStore *Store, CSTRING Key)
{
   ULONG key_hash = StrHash(Key, (Store->Flags & KSF_CASE) != 0);
   LONG index = hm_hash_index(Store->TableSize, key_hash);
   for (LONG i=0; i < BUCKET_SIZE; i++){
      if (!Store->Data[index]) break;
      if (Store->Data[index]->KeyHash IS key_hash) {
         if (Store->Flags & KSF_CASE) {
            if (!strcmp(Key, GET_KEY_NAME(Store->Data[index]))) return index;
         }
         else if (!strcasecmp(Key, GET_KEY_NAME(Store->Data[index]))) return index;
      }
      index = MOD_TABLESIZE(index + 1, Store->TableSize);
   }

   return -1;
}

static LONG hm_get_hashed(KeyStore *Store, ULONG Key)
{
   LONG index = hm_hash_index(Store->TableSize, Key);
   for (LONG i=0; i < BUCKET_SIZE; i++){
      if (!Store->Data[index]) break;
      if (Store->Data[index]->KeyHash IS Key) return index;
      index = MOD_TABLESIZE(index + 1, Store->TableSize);
   }

   return -1;
}

/*****************************************************************************

-FUNCTION-
VarCopy: Copies all keys in a KeyStore source to a destination KeyStore.

This function will copy all variables in Source to Dest.  Any key-values that already exist in Dest will be
overwritten by this operation.  If an error occurs during the process, the operation will abort and the destination
KeyStore will contain an unknown number of variables from the source.

-INPUT-
resource(KeyStore) Source: The source variable store.
resource(KeyStore) Dest: The destination variable store.

-ERRORS-
Okay
NullArgs
AllocMemory

*****************************************************************************/

ERROR VarCopy(KeyStore *Source, KeyStore *Dest)
{
   parasol::Log log(__FUNCTION__);

   if ((!Source) or (!Dest)) return ERR_NullArgs;

   if (!Source->Total) return ERR_Okay; // Nothing to be merged

   log.traceBranch("%p to %p", Source, Dest);

   if (Source->Flags & KSF_THREAD_SAFE) LockMutex(Source->Mutex, 0x7fffffff);
   if (Dest->Flags & KSF_THREAD_SAFE) LockMutex(Dest->Mutex, 0x7fffffff);

   LONG i;
   for (i=0; i < Source->TableSize; i++) {
      if (!Source->Data[i]) continue;
      if (CHECK_DEAD_KEY(Source->Data[i])) continue;

      LONG size = GET_KEY_SIZE(Source->Data[i]);

      KeyPair *clone = (KeyPair *)malloc(size);
      if (clone) {
         CopyMemory(Source->Data[i], clone, size);

         LONG ki;
         if (Source->Data[i]->Flags & KPF_PREHASHED) {
            if ((ki = hm_get_hashed(Dest, Source->Data[i]->KeyHash)) >= 0) { // Key already exists.  Replace it.
               free((APTR)Dest->Data[ki]);
               Dest->Data[ki] = clone;
            }
            else if (hm_put(Dest, clone) < 0) {
               if (Source->Flags & KSF_THREAD_SAFE) UnlockMutex(Source->Mutex);
               if (Dest->Flags & KSF_THREAD_SAFE) UnlockMutex(Dest->Mutex);
               return ERR_AllocMemory;
            }
         }
         else if ((ki = hm_get(Dest, GET_KEY_NAME(Source->Data[i]))) >= 0) { // Key already exists.  Replace it.
            free((APTR)Dest->Data[ki]);
            Dest->Data[ki] = clone;
         }
         else if (hm_put(Dest, clone) < 0) {
            if (Source->Flags & KSF_THREAD_SAFE) UnlockMutex(Source->Mutex);
            if (Dest->Flags & KSF_THREAD_SAFE) UnlockMutex(Dest->Mutex);
            return ERR_AllocMemory;
         }
      }
      else {
         if (Source->Flags & KSF_THREAD_SAFE) UnlockMutex(Source->Mutex);
         if (Dest->Flags & KSF_THREAD_SAFE) UnlockMutex(Dest->Mutex);
         return ERR_AllocMemory;
      }
   }

   if (Source->Flags & KSF_THREAD_SAFE) UnlockMutex(Source->Mutex);
   if (Dest->Flags & KSF_THREAD_SAFE) UnlockMutex(Dest->Mutex);
   return ERR_Okay;
}

/*****************************************************************************

-FUNCTION-
VarGet: Retrieve the value of a key, by name.

Named variables can be retrieved from variable stores by calling VarGet().  If a matching Name is discovered,
a direct pointer to its associated data is returned.

In addition to values set with ~VarSet(), this function also works with those set by ~VarSet().

This function is the fastest means of testing for the existence of a key if Data and Size are set to NULL.

-INPUT-
resource(KeyStore) Store: Must refer to a variable storage structure.
cstr Name: The name of the variable to lookup.
&ptr Data: A pointer to the data associated with Name is returned in this parameter.
&bufsize Size: Optional.  The size of the Data area is returned in this parameter.

-ERRORS-
Okay
NullArgs
DoesNotExist

*****************************************************************************/

ERROR VarGet(KeyStore *Store, CSTRING Name, APTR *Data, LONG *Size)
{
   parasol::Log log(__FUNCTION__);

   if ((!Name) or (!*Name) or (!Store)) return ERR_NullArgs;

   log.traceBranch("%s", Name);

   if (Store->Flags & KSF_THREAD_SAFE) LockMutex(Store->Mutex, 0x7fffffff);

   LONG ki;
   if ((ki = hm_get(Store, Name)) >= 0) {
      if (CHECK_DEAD_KEY(Store->Data[ki])) {
         if (Store->Flags & KSF_THREAD_SAFE) UnlockMutex(Store->Mutex);
         return ERR_DoesNotExist;
      }

      if (Data) *Data = (APTR)GET_KEY_VALUE(Store->Data[ki]);
      if (Size) *Size = Store->Data[ki]->ValueLength;

      if (Store->Flags & KSF_THREAD_SAFE) UnlockMutex(Store->Mutex);
      return ERR_Okay;
   }

   if (Store->Flags & KSF_THREAD_SAFE) UnlockMutex(Store->Mutex);
   return ERR_DoesNotExist;
}

/*****************************************************************************

-FUNCTION-
VarGetString: Retrieve a key value from a key store.

Key values that have been stored as strings can be retrieved with VarGetString().  If a matching Key is discovered, the
relevant string value is returned.  If an error occurs or the variable does not exist, NULL is returned.

-INPUT-
resource(KeyStore) Store: Must refer to a variable storage structure.
cstr Key: The name of the key-pair to lookup.

-RESULT-
cstr: The value for the given key is returned.  If no match is possible then a NULL pointer is returned.

*****************************************************************************/

CSTRING VarGetString(KeyStore *Store, CSTRING Key)
{
   if ((!Key) or (!*Key) or (!Store)) return NULL;

   if (Store->Flags & KSF_THREAD_SAFE) LockMutex(Store->Mutex, 0x7fffffff);

   LONG ki;
   if ((ki = hm_get(Store, Key)) >= 0) {
      if (Store->Data[ki]->Flags & KPF_STRING) {
         if (CHECK_DEAD_KEY(Store->Data[ki])) {
            if (Store->Flags & KSF_THREAD_SAFE) UnlockMutex(Store->Mutex);
            return NULL;
         }

         CSTRING val = GET_KEY_VALUE(Store->Data[ki]);

         if (Store->Flags & KSF_THREAD_SAFE) UnlockMutex(Store->Mutex);
         return val;
      }
   }

   if (Store->Flags & KSF_THREAD_SAFE) UnlockMutex(Store->Mutex);
   return NULL;
}

/*****************************************************************************

-FUNCTION-
VarIterate: Iteratively scan the key-store for all key values.

Use the VarIterate() function to scan all named keys and values in a key-store resource.  A new scan is initiated by setting
the Index parameter to NULL.  Thereafter, the Index should be set to the previously returned Key string and the
function called repetitively until ERR_Finished is returned to indicate that all keys have been processed.  The
following loop illustrates.  Notice how a single variable is used for both Index and Key to create an optimal loop:

<pre>
CSTRING key = NULL, value;
LONG value_len;
while (!VarIterate(keystore, key, &key, &value, &value_len)) {
   printf("%s = %s\n", key, value);
}
</pre>

The following equivalent can be used in Fluid:

<pre>
local err, key, value = mSys.VarIterate(glVars, nil)
while (err == ERR_Okay) do
   print(key, " = ", value)
   err, key, value = mSys.VarIterate(glVars, key)
end
</pre>

It is critical that the key-store is not modified during the scan.  Doing so may result in the keys being reshuffled
and consequently some of the previously returned keys would reappear during the iteration.

Retrieval of the value associated with a key is optional, so if not required please set Data and Size to NULL.  If
the key-store includes pre-hashed keys from ~KeySet(), use ~KeyIterate() instead of this function
as it works with named keys only.

-INPUT-
resource(KeyStore) Store: Must refer to a variable storage structure.
cstr Index: Set to NULL to initiate a scan.
&cstr Key: The next discovered key name is returned in this parameter.
&ptr Data: Optional.  A pointer to the data associated with Key is returned in this parameter.
&bufsize Size: Optional.  The size of the Data area is returned in this parameter.

-ERRORS-
Okay: A key has been discovered.
NullArgs
NotFound: The Index does not match a known key.
Finished: All keys have been iterated.

*****************************************************************************/

ERROR VarIterate(KeyStore *Store, CSTRING Index, CSTRING *Key, APTR *Data, LONG *Size)
{
   if (!Store) return ERR_NullArgs;

   if (Store->Flags & KSF_THREAD_SAFE) LockMutex(Store->Mutex, 0x7fffffff);

   LONG ki, i;

   if (Index) {
      ki = hm_get(Store, Index);
      if (ki < 0) {
         if (Store->Flags & KSF_THREAD_SAFE) UnlockMutex(Store->Mutex);
         return ERR_NotFound;
      }
   }
   else ki = -1;

   for (i=ki+1; i < Store->TableSize; i++) {
      if ((!Store->Data[i]) or (CHECK_DEAD_KEY(Store->Data[i])) or (Store->Data[i]->Flags & KPF_PREHASHED)) continue;

      if (Key)  *Key = GET_KEY_NAME(Store->Data[i]);
      if (Data) *Data = (APTR)GET_KEY_VALUE(Store->Data[i]);
      if (Size) *Size = Store->Data[i]->ValueLength;

      if (Store->Flags & KSF_THREAD_SAFE) UnlockMutex(Store->Mutex);
      return ERR_Okay;
   }

   if (Key)  *Key = NULL;
   if (Data) *Data = NULL;
   if (Size) *Size = 0;

   if (Store->Flags & KSF_THREAD_SAFE) UnlockMutex(Store->Mutex);
   return ERR_Finished;
}

/*****************************************************************************

-FUNCTION-
VarLock: Acquire a lock on a key store.

Call VarLock() to acquire a key store's internal lock, preventing other threads from utilising key store functions
on the resource until the lock is released.  Calls to VarLock() will nest, and each will need to be matched with a call
to ~VarUnlock().

The KeyStore must have been allocated with the THREAD_SAFE flag in the call to ~VarNew() in order to enable
locking functionality.  If not done so, an error of ERR_BadState is returned.

-INPUT-
resource(KeyStore) Store: The key store to lock.
int Timeout: Timeout measured in milliseconds.

-ERRORS-
Okay
BadState
TimeOut

*****************************************************************************/

ERROR VarLock(KeyStore *Store, LONG Timeout)
{
   if (!Store) return ERR_NullArgs;
   if (!Store->Mutex) return ERR_BadState;
   return LockMutex(Store->Mutex, Timeout);
}

/*****************************************************************************

-FUNCTION-
VarNew: Creates a new resource for storing key values.

Call VarNew() to create a resource for storing key values.  Key pairs are managed using hashmap routines that have been
optimised for the storage of key-value strings and arbitrary data buffers.

Key names are case-insensitive by default, but specifying KSF_CASE in the Flags parameter will enable case sensitivity.

The key store must be removed with ~FreeResource() once it is no longer required.

-INPUT-
int InitialSize: A sizing hint that indicates the minimum number of expected key values for storage.  Set to zero for the default.
int(KSF) Flags: Optional flags.

-RESULT-
resource(KeyStore): The allocated resource is returned or NULL if a memory allocation error occurred.

*****************************************************************************/

KeyStore * VarNew(LONG InitialSize, LONG Flags)
{
   parasol::Log log(__FUNCTION__);

   if (InitialSize < INITIAL_SIZE) InitialSize = INITIAL_SIZE;

   // Ensure that InitialSize is rounded up to a power of 2

   InitialSize = 1<<((__builtin_clz(InitialSize-1) ^ 31) + 1);

   KeyStore *vs;

   ERROR error;
   LONG mem_flags = MEM_DATA|MEM_MANAGED;
   if (Flags & KSF_UNTRACKED) mem_flags |= MEM_UNTRACKED;
   error = AllocMemory(sizeof(KeyStore), mem_flags, (APTR *)&vs, NULL);
   if (!error) set_memory_manager(vs, &glResourceKeyStore);

   if (error) {
      log.traceWarning("Failed to allocate memory.");
      return NULL;
   }

   if ((vs->Data = (KeyPair **)malloc(InitialSize * sizeof(STRING)))) {
      ClearMemory(vs->Data, InitialSize * sizeof(STRING));

      if (Flags & KSF_THREAD_SAFE) {
         if ((error = AllocMutex(ALF_RECURSIVE, &vs->Mutex)) != ERR_Okay) {
            log.traceWarning("AllocMutex() failed: %s", GetErrorMsg(error));
            FreeResource(vs);
            return NULL;
         }
      }

      vs->TableSize = InitialSize;
      vs->Total = 0;
      vs->Flags = Flags;
      return vs;
   }
   else FreeResource(vs);

   return NULL;
}

/*****************************************************************************

-FUNCTION-
VarSetString: Set a key-value string pair in a key store.

The VarSetString() function will store a provided key-value pair in a KeyStore resource.  The variable will be
retrievable from ~VarGetString() if it completes successfully.

A Key name and accompanying Value must be supplied.  If the Value is NULL, any existing key with a matching name
will be removed and this function will return immediately.

-INPUT-
resource(KeyStore) Store: The targeted variable store.
cstr Key: The name of a new or existing key.
cstr Value: A value to associate with the given Key.  If NULL, a remove operation is performed and ERR_Okay is returned immediately.

-ERRORS-
Okay
NullArgs
AllocMemory

*****************************************************************************/

ERROR VarSetString(KeyStore *Store, CSTRING Key, CSTRING Value)
{
   parasol::Log log(__FUNCTION__);

   if ((!Key) or (!Store)) return ERR_NullArgs;

   log.traceBranch("%p: %s = %.60s", Store, Key, Value);

   if (Key[0] IS '+') log.error("The use of '+' to for appending keys is no longer supported: %s", Key);

   if (Store->Flags & KSF_THREAD_SAFE) LockMutex(Store->Mutex, 0x7fffffff);

   LONG ki;
   if ((ki = hm_get(Store, Key)) >= 0) { // Key already exists.  Replace it.
      if (!Value) { // Client request to delete the key, so mark it as dead and return.
         DEAD_KEY(Store->Data[ki]);
         if (Store->Flags & KSF_THREAD_SAFE) UnlockMutex(Store->Mutex);
         return ERR_Okay;
      }

      // Free the existing key-pair and replace it with a new one.

      KeyPair *kp = build_key_pair(Store, Key, Value, StrLength(Value) + 1);
      if (kp) {
         kp->Flags |= KPF_STRING;
         free((APTR)Store->Data[ki]);
         Store->Data[ki] = kp;
         if (Store->Flags & KSF_THREAD_SAFE) UnlockMutex(Store->Mutex);
         return ERR_Okay;
      }
   }
   else if (!Value) { // Client requested deletion of a key that doesn't exist - ignore it.
      if (Store->Flags & KSF_THREAD_SAFE) UnlockMutex(Store->Mutex);
      return ERR_Okay;
   }
   else { // Brand new key
      KeyPair *kp = build_key_pair(Store, Key, Value, StrLength(Value) + 1);
      if ((kp) and ((ki = hm_put(Store, kp)) >= 0)) {
         kp->Flags |= KPF_STRING;
         if (Store->Flags & KSF_THREAD_SAFE) UnlockMutex(Store->Mutex);
         return ERR_Okay;
      }
   }

   if (Store->Flags & KSF_THREAD_SAFE) UnlockMutex(Store->Mutex);
   return ERR_AllocMemory;
}

/*****************************************************************************

-FUNCTION-
VarSet: Set a key-value pair with raw data.

The VarSet() function will copy a key-value pair to a KeyStore resource.  The stored data will be retrievable from
~VarGet() if it completes successfully.  Raw key-pairs stored with this function are not compatible with the
~VarGetString() function.

If the Data pointer is NULL, any existing key with a matching name will be removed and this function will return
immediately.  Removing a key in this way maintains the existing state of the key store, which means that keys can
be removed in a loop without concern for the key data pointers being rearranged.

Note that the contents of Data will be copied to the KeyStore.  If the data is stored elsewhere in a permanent area
that will outlast the KeyStore, consider storing a pointer or index instead as this will prevent unnecessary
duplication of the data.  Some design patterns may be able to take advantage of ~VarSetSized() as a means
of bypassing the copy operation.

-INPUT-
resource(KeyStore) Store: The targeted variable store.
cstr Key: The name of a new or existing key.
buf(ptr) Data: A pointer to raw data to associate with the given Key.  If NULL, a remove operation is performed and ERR_Okay is returned immediately.
bufsize Size: The byte-size of the Data buffer.

-RESULT-
ptr: A pointer to the cached version of the data is returned, or NULL if failure.

*****************************************************************************/

APTR VarSet(KeyStore *Store, CSTRING Key, APTR Data, LONG Size)
{
   parasol::Log log(__FUNCTION__);

   if ((!Key) or (!Store)) return NULL;

   log.traceBranch("%p: %s = %p", Store, Key, Data);

   if (Store->Flags & KSF_THREAD_SAFE) LockMutex(Store->Mutex, 0x7fffffff);

   LONG ki;
   if ((ki = hm_get(Store, Key)) >= 0) { // Key already exists.  Replace it.
      if (!Data) { // Client request to delete the key, so mark it as dead and return.
         DEAD_KEY(Store->Data[ki]);
         if (Store->Flags & KSF_THREAD_SAFE) UnlockMutex(Store->Mutex);
         return NULL;
      }

      // Free the existing key-pair and replace it with a new one.

      KeyPair *kp = build_key_pair(Store, Key, Data, Size);
      if (kp) {
         free((APTR)Store->Data[ki]);
         Store->Data[ki] = kp;
         if (Store->Flags & KSF_THREAD_SAFE) UnlockMutex(Store->Mutex);
         return (APTR)GET_KEY_VALUE(kp);
      }
   }
   else if (!Data) { // Client requested deletion of a key that doesn't exist - ignore it.
      if (Store->Flags & KSF_THREAD_SAFE) UnlockMutex(Store->Mutex);
      return NULL;
   }
   else { // Brand new key
      KeyPair *kp = build_key_pair(Store, Key, Data, Size);
      if ((kp) and ((ki = hm_put(Store, kp)) >= 0)) {
         if (Store->Flags & KSF_THREAD_SAFE) UnlockMutex(Store->Mutex);
         return (APTR)GET_KEY_VALUE(kp);
      }
   }

   if (Store->Flags & KSF_THREAD_SAFE) UnlockMutex(Store->Mutex);
   return NULL;
}

/*****************************************************************************

-FUNCTION-
VarSetSized: Create a key-value pair with an empty pre-sized data buffer.

This function follows the same pattern as ~VarSet() but eliminates the process of taking a copy of existing data.
This can lead to efficiency gains for certain design patterns that would be disadvantaged by preparing key-value data
in advance of a copy operation.  It is assumed that the client will manually write to the data buffer after calling
this function.

It is important to note that thread safety is not provisioned in the scenarios facilitated by this function.
The client will need a locking mechanism in place to manage the read-write conflicts of a threaded environment.

-INPUT-
resource(KeyStore) Store: The targeted variable store.
cstr Key: The name of a new or existing key.
int Size: The byte-size of the new data buffer.
&ptr Data: A pointer to the allocated data buffer will be returned here.
&bufsize DataSize: Optional.  The size of Data is returned here (identical to Size, this is present for typesafe languages).

-ERRORS-
Okay:
NullArgs:
AllocMemory:

*****************************************************************************/

ERROR VarSetSized(KeyStore *Store, CSTRING Key, LONG Size, APTR *Data, LONG *DataSize)
{
   parasol::Log log(__FUNCTION__);

   if ((!Key) or (!Store) or (!Size) or (!Data)) return ERR_NullArgs;

   log.traceBranch("%p: %s, Size: %d", Store, Key, Size);

   if (Store->Flags & KSF_THREAD_SAFE) LockMutex(Store->Mutex, 0x7fffffff);

   LONG ki;
   if ((ki = hm_get(Store, Key)) >= 0) { // Key already exists.  Replace it.
      // Free the existing key-pair and replace it with a new one.

      KeyPair *kp = build_key_pair(Store, Key, NULL, Size);
      if (kp) {
         free((APTR)Store->Data[ki]);
         Store->Data[ki] = kp;
         if (Store->Flags & KSF_THREAD_SAFE) UnlockMutex(Store->Mutex);
         *Data = (APTR)GET_KEY_VALUE(kp);
         if (DataSize) *DataSize = Size;
         return ERR_Okay;
      }
   }
   else { // Brand new key
      KeyPair *kp = build_key_pair(Store, Key, NULL, Size);
      if ((kp) and ((ki = hm_put(Store, kp)) >= 0)) {
         if (Store->Flags & KSF_THREAD_SAFE) UnlockMutex(Store->Mutex);
         *Data = (APTR)GET_KEY_VALUE(kp);
         if (DataSize) *DataSize = Size;
         return ERR_Okay;
      }
   }

   if (Store->Flags & KSF_THREAD_SAFE) UnlockMutex(Store->Mutex);
   return ERR_AllocMemory;
}

/*****************************************************************************

-FUNCTION-
VarUnlock: Release a lock acquired against a key store.

Call VarUnlock() to release a lock previously acquired with VarLock().

-INPUT-
resource(KeyStore) Store: The key store to lock.

*****************************************************************************/

void VarUnlock(KeyStore *Store)
{
   if ((!Store) or (!Store->Mutex)) return;
   UnlockMutex(Store->Mutex);
}

/*****************************************************************************

-FUNCTION-
KeyGet: Retrieve a raw data value from a KeyStore.

Key values that have been set with ~KeySet() can be retrieved with this function.  If the requested Key is
found, a direct pointer to its data is returned.

-INPUT-
resource(KeyStore) Store: Must refer to a KeyStore resource.
uint Key: A unique identifier to access.
&ptr Data: A pointer to the data associated with Key is returned in this parameter.
&bufsize Size: Optional.  The size of the Data area is returned in this parameter.

-ERRORS-
Okay
NullArgs
DoesNotExist

*****************************************************************************/

ERROR KeyGet(KeyStore *Store, ULONG Key, APTR *Data, LONG *Size)
{
   if ((!Store) or (!Data)) return ERR_NullArgs;

   if (Store->Flags & KSF_THREAD_SAFE) LockMutex(Store->Mutex, 0x7fffffff);

   LONG ki;
   if ((ki = hm_get_hashed(Store, Key)) >= 0) {
      if (CHECK_DEAD_KEY(Store->Data[ki])) {
         if (Store->Flags & KSF_THREAD_SAFE) UnlockMutex(Store->Mutex);
         return ERR_DoesNotExist;
      }

      *Data = (APTR)GET_KEY_VALUE(Store->Data[ki]);
      if (Size) *Size = Store->Data[ki]->ValueLength;
      if (Store->Flags & KSF_THREAD_SAFE) UnlockMutex(Store->Mutex);
      return ERR_Okay;
   }

   if (Store->Flags & KSF_THREAD_SAFE) UnlockMutex(Store->Mutex);
   return ERR_DoesNotExist;
}

/*****************************************************************************

-FUNCTION-
KeyIterate: Iteratively scan the key-store for all key values.

Use the KeyIterate() function to scan all keys and values in a key-store resource.  A new scan is initiated by setting
the Index parameter to zero.  Thereafter, the Index should be set to the previously returned Key value and the
function called repetitively until ERR_Finished is returned to indicate that all keys have been processed.  The
following loop illustrates.  Notice how a single variable is used for both Index and Key to create an optimal loop:

<pre>
ULONG key = 0, value;
LONG value_len;
while (!KeyIterate(keystore, key, &key, &value, &value_len)) {
   printf("$%.8x = %s\n", key, value);
}
</>

The following equivalent can be used in Fluid:

<pre>
local err, key, value = mSys.KeyIterate(glVars, key)
while (err == ERR_Okay) do
   print(key, " = ", value)
   err, key, value = mSys.KeyIterate(glVars, key)
end
</>

It is critical that the key-store is not modified during the scan.  Doing so may result in the keys being reshuffled
and consequently some of the previously returned keys would reappear during the iteration.

Retrieval of the value associated with a key is optional, so if not required please set Data and Size to NULL.

-INPUT-
resource(KeyStore) Store: Must refer to a variable storage structure.
uint Index: Set to zero to initiate a scan.
&uint Key: The next discovered key value is returned in this parameter.
&ptr Data: Optional.  A pointer to the data associated with Key is returned in this parameter.
&bufsize Size: Optional.  The size of the Data area is returned in this parameter.

-ERRORS-
Okay: A key has been discovered.
NullArgs
NotFound: The Index does not match a known key.
Finished: All keys have been iterated.

*****************************************************************************/

ERROR KeyIterate(KeyStore *Store, ULONG Index, ULONG *Key, APTR *Data, LONG *Size)
{
   if (!Store) return ERR_NullArgs;

   if (Store->Flags & KSF_THREAD_SAFE) LockMutex(Store->Mutex, 0x7fffffff);

   LONG ki, i;

   if (Index) {
      ki = hm_get_hashed(Store, Index);
      if (ki < 0) {
         if (Store->Flags & KSF_THREAD_SAFE) UnlockMutex(Store->Mutex);
         return ERR_NotFound;
      }
   }
   else ki = -1;

   for (i=ki+1; i < Store->TableSize; i++) {
      if ((!Store->Data[i]) or (CHECK_DEAD_KEY(Store->Data[i]))) continue;

      if (Key)  *Key = Store->Data[i]->KeyHash;
      if (Data) *Data = (APTR)GET_KEY_VALUE(Store->Data[i]);
      if (Size) *Size = Store->Data[i]->ValueLength;

      if (Store->Flags & KSF_THREAD_SAFE) UnlockMutex(Store->Mutex);
      return ERR_Okay;
   }

   if (Key)  *Key = 0;
   if (Data) *Data = NULL;
   if (Size) *Size = 0;

   if (Store->Flags & KSF_THREAD_SAFE) UnlockMutex(Store->Mutex);
   return ERR_Finished;
}

/*****************************************************************************

-FUNCTION-
KeySet: Set a key-value pair with raw data.

The KeySet() function will store a raw key-value pair in a KeyStore resource, using a unique integer base identifier of
your choosing.  It is strongly recommended that the identifier is a hash and not part of a numerical sequence, as
this would result in keys that are poorly distributed in the internal hashmap.

The key value will be retrievable from ~KeyGet() only.  Raw key-pairs are not compatible with functions that
use hashed key names such as ~VarGetString().

If the Data pointer is NULL, any existing key that matches will be removed and this function will return immediately.

Note that the contents of Data will be copied to the KeyStore.  If the data is stored elsewhere in a permanent area
that will outlast the KeyStore, consider storing a pointer or index instead as this will prevent unnecessary
duplication of the data.

-INPUT-
resource(KeyStore) Store: The targeted variable store.
uint Key: A unique identifier for the data value.
buf(cptr) Data: A pointer to raw data to associate with the given Key.  If NULL, a remove operation is performed and ERR_Okay is returned immediately.
bufsize Size: The byte-size of the Data buffer.

-ERRORS-
Okay
NullArgs
AllocMemory
-END-

*****************************************************************************/

ERROR KeySet(KeyStore *Store, ULONG Key, const void *Data, LONG Size)
{
   if (!Store) return ERR_NullArgs;

   if (Store->Flags & KSF_THREAD_SAFE) LockMutex(Store->Mutex, 0x7fffffff);

   LONG ki;
   if ((ki = hm_get_hashed(Store, Key)) >= 0) { // Key already exists.  Replace it.
      if (!Data) { // Client request to delete the key, so mark it as dead and return.
         DEAD_KEY(Store->Data[ki]);
         if (Store->Flags & KSF_THREAD_SAFE) UnlockMutex(Store->Mutex);
         return ERR_Okay;
      }

      // Free the existing key-pair and replace it with a new one.

      KeyPair *kp = build_hashed_key_pair(Store, Key, Data, Size);
      if (kp) {
         free((APTR)Store->Data[ki]);
         Store->Data[ki] = kp;
         if (Store->Flags & KSF_THREAD_SAFE) UnlockMutex(Store->Mutex);
         return ERR_Okay;
      }
   }
   else if (!Data) { // Client requested deletion of a key that doesn't exist - ignore it.
      if (Store->Flags & KSF_THREAD_SAFE) UnlockMutex(Store->Mutex);
      return ERR_Okay;
   }
   else { // Brand new key
      if ((Size > 64 * 1024) or (Size < 0)) {
         if (Store->Flags & KSF_THREAD_SAFE) UnlockMutex(Store->Mutex);
         return ERR_DataSize;
      }

      KeyPair *kp = build_hashed_key_pair(Store, Key, Data, Size);
      if ((kp) and ((ki = hm_put(Store, kp)) >= 0)) {
         if (Store->Flags & KSF_THREAD_SAFE) UnlockMutex(Store->Mutex);
         return ERR_Okay;
      }
   }

   if (Store->Flags & KSF_THREAD_SAFE) UnlockMutex(Store->Mutex);
   return ERR_AllocMemory;
}
