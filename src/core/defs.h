#ifndef DEFS_H
#define DEFS_H 1

#ifndef PLATFORM_CONFIG_H
#include <parasol/config.h>
#endif

#include <set>
#include <functional>

#define PRV_CORE
#define PRV_CORE_MODULE
#define PRV_THREAD
#ifndef __system__
#define __system__
#endif

#ifdef __unix__
 #include <sys/un.h>
 #include <sys/socket.h>
 #include <pthread.h>
 #include <semaphore.h>
 #undef NULL
 #define NULL 0
#endif

#define PRV_METACLASS 1
#define PRV_MODULE 1

#include "microsoft/windefs.h"

// See the makefile for optional defines

#define RoundPageSize(size) ((size) + glPageSize - ((size) % glPageSize))

//#define USE_GLOBAL_EVENTS 1 // Use a global locking system for resources in Windows (equivalent to Linux)

#define MAX_TASKS    50  // Maximum number of tasks allowed to run at once
#define MAX_SEMLOCKS 40  // Maximum number of semaphore allocations per task

#define MSG_MAXARGSIZE   512   // The maximum allowable size of data based arguments before they have to be allocated as public memory blocks when messaging
#define MAX_BLOCKS       2048  // The maximum number of public memory blocks (system-wide) that the Core can handle at once
#define AUTOLOAD_MAX     30
#define SIZE_SYSTEM_PATH 100  // Max characters for the Parasol system path

#define MAX_SEMAPHORES    40  // Maximum number of semaphores that can be allocated in the system
#define MAX_THREADS       20  // Maximum number of threads per process.
#define MAX_NB_LOCKS      20  // Non-blocking locks apply when locking 'free-for-all' public memory blocks.  The maximum value is per-task, so keep the value low.
#define MAX_WAITLOCKS     60  // This value is effectively imposing a limit on the maximum number of threads/processes that can be active at any time.

#ifndef O_LARGEFILE
#define O_LARGEFILE 0
#endif

#ifndef O_NONBLOCK
#define O_NONBLOCK 0
#endif

#ifdef _WIN32
#define WIN32OPEN O_BINARY
#else
#define WIN32OPEN 0
#endif

#define LEN_VOLUME_NAME 40

#define DRIVETYPE_REMOVABLE 1
#define DRIVETYPE_CDROM     2
#define DRIVETYPE_FIXED     3
#define DRIVETYPE_NETWORK   4

#define DEFAULT_VIRTUALID 0xffffffff

#ifdef _WIN32
   typedef void * MODHANDLE;
   typedef void * THREADLOCK;
   typedef void * CONDLOCK;
#else
   typedef void * MODHANDLE;
   typedef pthread_mutex_t THREADLOCK;
   typedef pthread_cond_t CONDLOCK;
#endif

#define BREAKPOINT { UBYTE *nz = 0; nz[0] = 0; }

#include <parasol/system/types.h>

#include <stdarg.h>

struct translate {
   BYTE Replaced;  // TRUE if the translation table has been replaced with a new one
   LONG Total;     // Total number of array entries
   char Language[4]; // 3 letter language code + null byte

   // An array of STRING pointers, to a maximum of Total follows, sorted alphabetically

   // The strings themselves then follow
};

struct ChildEntry;
struct ObjectInfo;
struct MemInfo;
struct ListTasks;
struct DateTime;
struct KeyStore;
struct RGB8;
struct rkBase64Decode;
struct FileInfo;
struct DirInfo;
struct CacheFile;
class objFile;
class objStorageDevice;
class objConfig;
class objMetaClass;
struct ActionTable;
struct FileFeedback;
struct ResourceManager;
struct MsgHandler;

//********************************************************************************************************************
// Private memory management structures.

class PrivateAddress {
public:
   union {
      APTR      Address;
      OBJECTPTR Object;
   };
   MEMORYID MemoryID;   // Unique identifier
   OBJECTID OwnerID;    // The object that allocated this block.
   ULONG    Size;       // 4GB max
   volatile LONG ThreadLockID = 0;
   WORD     Flags;
   volatile WORD AccessCount = 0; // Total number of locks

   PrivateAddress(APTR aAddress, MEMORYID aMemoryID, OBJECTID aOwnerID, ULONG aSize, WORD aFlags) :
      Address(aAddress), MemoryID(aMemoryID), OwnerID(aOwnerID), Size(aSize), Flags(aFlags) { };
};

struct rkWatchPath {
   LARGE      Custom;    // User's custom data pointer or value
   HOSTHANDLE Handle;    // The handle for the file being monitored, can be a special reference for virtual paths
   FUNCTION   Routine;   // Routine to call on event trigger
   LONG       Flags;     // Event mask (original flags supplied to Watch)
   ULONG      VirtualID; // If monitored path is virtual, this refers to an ID in the glVirtual table

#ifdef _WIN32
   LONG WinFlags;
#endif
};

#define STAT_FOLDER 0x0001

#include "prototypes.h"

#include <parasol/main.h>

enum {
   TL_GENERIC=0,
   TL_TIMER,
   TL_MEMORY_PAGES,
   TL_OBJECT_LOOKUP,
   TL_PRIVATE_MEM,
   TL_PRINT,
   TL_PRIVATE_OBJECTS,
   TL_MSGHANDLER,
   TL_THREADPOOL,
   TL_END
};

enum {
   CN_PRIVATE_MEM=0,
   CN_OBJECTS,
   CN_END
};

//********************************************************************************************************************

struct Stats {
   union {
      MEMORYID ID; // Action subscriptions (struct ActionSubscription)
      APTR Ptr;
   } ActionSubscriptions;
   LONG     NotifyFlags[2];     // Action notification flags - space for 64 actions max
   char     Name[MAX_NAME_LEN]; // The name of the object (optional)
   UWORD    SubscriptionSize;   // Size of the ActionSubscriptions array
   UWORD    Empty;
};

// Subscription structures

typedef struct ActionSubscription {
   ACTIONID ActionID;       // Monitored action
   OBJECTID SubscriberID;   // Object to be notified
   MEMORYID MessagePortMID; // Message port for the object
   CLASSID  ClassID;        // Class of the subscribed object
} ActionSubscription;

struct virtual_drive {
   ULONG VirtualID;  // Hash name of the volume, not including the trailing colon
   char Name[32];    // Volume name, including the trailing colon at the end
   ULONG CaseSensitive:1;
   ERROR (*ScanDir)(DirInfo *);
   ERROR (*Rename)(STRING, STRING);
   ERROR (*Delete)(STRING, FUNCTION *);
   ERROR (*OpenDir)(DirInfo *);
   ERROR (*CloseDir)(DirInfo *);
   ERROR (*Obsolete)(CSTRING, DirInfo **, LONG);
   ERROR (*TestPath)(CSTRING, LONG, LONG *);
   ERROR (*WatchPath)(class extFile *);
   void  (*IgnoreFile)(class extFile *);
   ERROR (*GetInfo)(CSTRING, FileInfo *, LONG);
   ERROR (*GetDeviceInfo)(CSTRING, objStorageDevice *);
   ERROR (*IdentifyFile)(STRING, CLASSID *, CLASSID *);
   ERROR (*CreateFolder)(CSTRING, LONG);
   ERROR (*SameFile)(CSTRING, CSTRING);
   ERROR (*ReadLink)(STRING, STRING *);
   ERROR (*CreateLink)(CSTRING, CSTRING);
};

extern const struct virtual_drive glFSDefault;
extern LONG glVirtualTotal;
extern struct virtual_drive glVirtual[20];

//********************************************************************************************************************
// Resource definitions.

#define PRIVATE_TABLE_CHUNK  300
#define PUBLIC_TABLE_CHUNK   1000  // Maximum number of public objects (system-wide)
#define PAGE_TABLE_CHUNK     32
#define MEMHEADER            12    // 8 bytes at start for MEMH and MemoryID, 4 at end for MEMT

// Turning off USE_SHM means that the shared memory pool is available to all processes by default.

#ifdef __ANDROID__
  #undef USE_SHM // Should be using ashmem
  #define SHMKEY 0x0009f830 // Keep the key value low as we will be incrementing it

  #ifdef USE_SHM
    #define INITIAL_PUBLIC_SIZE  0
  #else
    #define INITIAL_PUBLIC_SIZE  1024768
    extern LONG glMemoryFD;
  #endif
#elif __unix__
  #define USE_SHM TRUE
  #define SHMKEY 0x0009f830 // Keep the key value low as we will be incrementing it

  #ifdef USE_SHM
    #define MEMORYFILE           "/tmp/parasol.mem"
    #define INITIAL_PUBLIC_SIZE  0
  #else
    // To mount a 32MB RAMFS filesystem for this method:
    //
    //    mkdir -p /RAM1
    //    mount -t ramfs none /tmp/ramfs -o maxsize=32000

    #define MEMORYFILE           "/tmp/ramfs/parasol.mem"
    #define INITIAL_PUBLIC_SIZE  1024768

    extern LONG glMemoryFD;
  #endif
#elif _WIN32
  #define INITIAL_PUBLIC_SIZE (2 * 1048576)
  #define STATIC_MEMORY_POOL TRUE // The entire memory-pool is pre-paged.  No paging of individual memory blocks is performed.
#endif

#ifdef _WIN32

struct public_lock {
   char Name[12];
   WINHANDLE Lock;
   LONG PID;
   WORD Count;
   UBYTE Event:1; // Set to TRUE if the lock is for a broadcast-able event
};

extern struct public_lock glPublicLocks[PL_END];

#endif

#define MPF_LOCAL 0x0001       // The page is owned by the task

struct MemoryPage {
   APTR     Address;           // Map address
   MEMORYID MemoryID;          // Represented memory ID
   WORD     AccessCount;       // Access count
   WORD     Flags;             // Special flags
   #ifdef __unix__
      LARGE    Size;
   #endif
};

enum {
   RT_MEMORY=1,
   RT_SEMAPHORE,
   RT_OBJECT
};

struct WaitLock {
   LONG ProcessID;
   LONG ThreadID;
   #ifdef _WIN32
      #ifndef USE_GLOBAL_EVENTS
         WINHANDLE Lock;
      #endif
   #endif
   LARGE WaitingTime;
   LONG WaitingForProcessID;
   LONG WaitingForThreadID;
   LONG WaitingForResourceID;
   LONG WaitingForResourceType;
   UBYTE Flags; // WLF flags
};

#define WLF_REMOVED 0x01  // Set if the resource was removed by the thread that was holding it.

//********************************************************************************************************************
// Shared object management.

struct SharedObjectHeader {
   LONG Offset;          // Offset of the main array - sizeof(struct PublicObjectHeader)
   LONG NextEntry;       // Next available entry within the array
   LONG ArraySize;       // Actual size of the array
};

struct SharedObject {
   OBJECTID  ObjectID;           // The object's ID
   OBJECTID  OwnerID;            // The owner of the object (note: Can be a private or public object)
   MEMORYID  MessageMID;         // If the object is private, this field refers to the Task MessageMID that owns it
   OBJECTPTR Address;            // Pointer the object address (if in private memory)
   CLASSID   ClassID;            // Class ID of the object
   char      Name[MAX_NAME_LEN]; // Name of the object
   UWORD     Flags;              // NF flags
   LONG      InstanceID;         // Reference to the instance that this object is restricted to
};

//********************************************************************************************************************
// This structure is used for internally timed broadcasting.

class CoreTimer {
public:
   LARGE     NextCall;       // Cycle when PreciseTime() reaches this value (us)
   LARGE     LastCall;       // PreciseTime() recorded at the last call (us)
   LARGE     Interval;       // The amount of microseconds to wait at each interval
   OBJECTPTR Subscriber;     // The object that is subscribed (pointer, if private)
   OBJECTID  SubscriberID;   // The object that is subscribed
   FUNCTION  Routine;        // Routine to call if not using AC_Timer - ERROR Routine(OBJECTID, LONG, LONG);
   UBYTE     Cycle;
   bool      Locked;
};

//********************************************************************************************************************
// Crash index numbers.  Please note that the order of this index must match the order in which resources are freed in
// the shutdown process.

enum {
   CP_START=1,
   CP_PRINT_CONTEXT,
   CP_PRINT_ACTION,
   CP_REMOVE_PRIVATE_LOCKS,
   CP_REMOVE_PUBLIC_LOCKS,
   CP_FREE_PUBLIC_MEMORY,
   CP_BROADCAST,
   CP_REMOVE_TASK,
   CP_REMOVE_TABLES,
   CP_FREE_ACTION_MANAGEMENT,
   CP_FREE_COREBASE,
   CP_FREE_MEMORY_PAGES,
   CP_FREE_PRIVATE_MEMORY,
   CP_FINISHED
};

class extMetaClass : public objMetaClass {
   public:
   using create = parasol::Create<extMetaClass>;
   class extMetaClass *Base;            // Reference to the base class if this is a sub-class
   struct Field *prvFields;             // Internal field structure
   const struct FieldArray *SubFields;  // Extra fields defined by the sub-class
   struct ModuleMaster *Master;         // Master module that owns this class, if any.
   UBYTE Children[8];                   // Child objects (field indexes), in order
   STRING Location;                     // Location of the class binary, this field exists purely for caching the location string if the user reads it
   struct ActionEntry ActionTable[AC_END];
   WORD OriginalFieldTotal;
};

class extFile : public objFile {
   public:
   using create = parasol::Create<extFile>;
   struct DateTime prvModified;  // [28 byte structure]
   struct DateTime prvCreated;  // [28 byte structure]
   LARGE Size;
   #ifdef _WIN32
      LONG  Stream;
   #else
      APTR  Stream;
   #endif
   STRING Path;
   STRING prvResolvedPath;  // Used on initialisation to speed up processing (nb: string deallocated after initialisation).
   STRING prvLink;
   STRING prvLine;
   CSTRING prvIcon;
   struct rkWatchPath *prvWatch;
   OBJECTPTR ProgressDialog;
   struct DirInfo *prvList;
   LARGE  ProgressTime;
   LONG   Permissions;
   LONG   prvType;
   LONG   Handle;         // Native system file handle
   WORD   prvLineLen;
};

class extConfig : public objConfig {
   public:
   using create = parasol::Create<extConfig>;
   ConfigGroups *Groups;
   ULONG    CRC;   // CRC32, for determining if config data has been altered
};

class extStorageDevice : public objStorageDevice {
   public:
   using create = parasol::Create<extStorageDevice>;
   STRING DeviceID;   // Unique ID for the filesystem, if available
   STRING Volume;
};

class extThread : public objThread {
   public:
   using create = parasol::Create<extThread>;
   #ifdef __unix__
      pthread_t PThread;
      LONG Msgs[2];
   #elif _WIN32
      WINHANDLE Handle;
      LONG ThreadID;
      WINHANDLE Msgs[2];
   #endif
   BYTE Active;
   BYTE Waiting;
   FUNCTION Routine;
   FUNCTION Callback;
};

class extTask : public objTask {
   public:
   using create = parasol::Create<extTask>;
   MEMORYID MessageMID;
   MEMORYID LocationMID;       // Where to load the task from (string)
   MEMORYID ParametersMID;     // Arguments (string)
   MEMORYID CopyrightMID;      // Copyright details (string)
   MEMORYID PathMID;
   MEMORYID ProcessPathMID;
   MEMORYID LaunchPathMID;
   STRING   LaunchPath;
   STRING   Path;
   STRING   ProcessPath;
   STRING   Location;         // Where to load the task from (string)
   CSTRING  *Parameters;      // Arguments (string array)
   STRING   Copyright;        // Copyright details (string)
   char     Name[32];         // Name of the task, if specified (string)
   char     Author[60];       // Who wrote the program (string)
   char     Date[20];         // Date of compilation (string)
   char     Short[80];        // Short description of program (string)
   LONG     ParametersSize;   // Byte size of the arguments structure
   STRING   Fields[100];      // Variable field storage
   BYTE     ReturnCodeSet;    // TRUE if the ReturnCode has been set
   FUNCTION ErrorCallback;
   FUNCTION OutputCallback;
   FUNCTION ExitCallback;
   FUNCTION InputCallback;
   struct MsgHandler *MsgAction;
   struct MsgHandler *MsgGetField;
   struct MsgHandler *MsgSetField;
   struct MsgHandler *MsgActionResult;
   struct MsgHandler *MsgDebug;
   struct MsgHandler *MsgWaitForObjects;
   struct MsgHandler *MsgValidateProcess;
   struct MsgHandler *MsgQuit;
   struct MsgHandler *MsgEvent;
   struct MsgHandler *MsgThreadCallback;
   struct MsgHandler *MsgThreadAction;

   #ifdef __unix__
      LONG InFD;             // stdin FD for receiving output from launched task
      LONG ErrFD;            // stderr FD for receiving output from launched task
   #endif
   #ifdef _WIN32
      STRING Env;
      APTR Platform;
   #endif
   struct ActionEntry Actions[AC_END]; // Action routines to be intercepted by the program
};

class extModule : public objModule {
   public:
   using create = parasol::Create<extModule>;
   char   Name[60];      // Name of the module
   APTR   prvMBMemory;   // Module base memory
   struct KeyStore *Vars;
};

//********************************************************************************************************************
// These values are set against glProgramStage to indicate the current state of the program (either starting up, active
// or shutting down).

enum {
   STAGE_STARTUP=1,
   STAGE_ACTIVE,
   STAGE_SHUTDOWN
};

//********************************************************************************************************************

struct ModuleHeader {
   LONG Total;          // Total number of registered modules
};

struct ModuleItem {
   ULONG Hash;       // Hash of the module file name
   LONG  Size;       // Size of the item structure, all accompanying strings and byte alignment
   // Followed by path
};

//********************************************************************************************************************
// Memory messaging structure.

struct MemoryMessageDetail {
   BYTE buffer[4];
};

struct MemoryMessage {
   #ifdef __unix__
      long MType;                   // <-- This long field is a Linux requirement
      struct MemoryMessageDetail Detail;
   #else
      LONG MemoryID;
   #endif
};

//********************************************************************************************************************
// Global data variables.

extern extMetaClass glMetaClass;
extern LONG glEUID, glEGID, glUID, glGID;
extern LONG glKeyState;
extern char glSystemPath[SIZE_SYSTEM_PATH];
extern char glModulePath[SIZE_SYSTEM_PATH];
extern char glRootPath[SIZE_SYSTEM_PATH];
extern char glProgName[32];
extern char glDisplayDriver[28];
extern OBJECTID SystemTaskID, glCurrentTaskID; // Read-only
extern WORD glLogLevel, glShowIO, glShowPrivate, glShowPublic, glMaxDepth;
extern UBYTE glTaskState;
extern LARGE glTimeLog;
extern char glAlphaNumeric[256];
extern struct ModuleMaster  *glModuleList;    // Locked with TL_GENERIC.  Maintained as a linked-list; hashmap unsuitable.
extern struct PublicAddress *glSharedBlocks;  // Locked with PL_PUBLICMEM
extern struct SortedAddress *glSortedBlocks;
extern struct SharedControl *glSharedControl; // Locked with PL_FORBID
extern struct TaskList      *shTasks, *glTaskEntry; // Locked with PL_PROCESSES
extern struct SemaphoreEntry *shSemaphores;     // Locked with PL_SEMAPHORES
extern const struct ActionTable ActionTable[];  // Read only
extern const struct Function    glFunctions[];  // Read only
extern std::list<CoreTimer> glTimers;          // Locked with TL_TIMER
extern std::unordered_map<MEMORYID, PrivateAddress> glPrivateMemory;  // Locked with TL_PRIVATE_MEM: Note that best performance for looking up ID's is achieved as a sorted array.
extern std::unordered_map<OBJECTID, std::set<MEMORYID, std::greater<MEMORYID>>> glObjectMemory; // Locked with TL_PRIVATE_MEM.  Sorted with the most recent private memory first
extern std::unordered_map<OBJECTID, std::set<OBJECTID, std::greater<OBJECTID>>> glObjectChildren; // Locked with TL_PRIVATE_MEM.  Sorted with most recent object first
extern struct MemoryPage   *glMemoryPages;      // Locked with TL_MEMORY_PAGES
extern struct KeyStore *glObjectLookup;         // Locked with TL_OBJECT_LOOKUP
extern struct ClassHeader  *glClassDB;          // Read-only.  Class database.
extern struct ModuleHeader *glModules;          // Read-only.  Module database.
extern struct OpenInfo *glOpenInfo;             // Read-only.  The OpenInfo structure initially passed to OpenCore()
extern objTask *glCurrentTask;            // Threads should use glCurrentTaskID to manage access.
extern CSTRING glMessages[ERR_END];       // Read-only table of error messages.
extern const LONG glTotalMessages;
extern LONG glTotalPages; // Read-only
extern MEMORYID glTaskMessageMID;        // Read-only
extern LONG glActionCount, glMemRegSize; // Read-only
extern LONG glProcessID, glInstanceID;   // Read only
extern HOSTHANDLE glConsoleFD;
extern LONG glStdErrFlags; // Read only
extern LONG glValidateProcessID; // Not a threading concern
extern LONG glMutexLockSize; // Read only constant
extern WORD glCrashStatus, glCodeIndex, glLastCodeIndex;
extern UWORD glFunctionID;
extern BYTE glMasterTask, glProgramStage, glPrivileged, glSync;
extern LONG glPageSize; // Read only
extern LONG glBufferSize;
extern TIMER glCacheTimer;
extern STRING glBuffer;
extern APTR glJNIEnv;
extern class ObjectContext glTopContext; // Read-only, not a threading concern.
extern OBJECTPTR modIconv;
extern OBJECTPTR glLocale;
extern objTime *glTime;
extern objConfig *glVolumes; // Volume management object, contains all FS volume names and their meta data.  Use AccessPrivateObject() to lock.
extern objConfig *glDatatypes;
extern struct KeyStore *glClassMap; // Register of all classes.
extern struct KeyStore *glFields; // Reverse lookup for converting field hashes back to their respective names.
extern OBJECTID glClassFileID;
extern CSTRING glIDL;
extern std::unordered_map<OBJECTID, ObjectSignal> glWFOList;
extern struct BaseClass glDummyObject;

extern CSTRING glClassBinPath;
extern CSTRING glModuleBinPath;
extern objMetaClass *ModuleMasterClass;
extern objMetaClass *ModuleClass;
extern objMetaClass *TaskClass;
extern objMetaClass *ThreadClass;
extern objMetaClass *TimeClass;
extern objMetaClass *ConfigClass;
extern objMetaClass *glFileClass;
extern objMetaClass *glStorageClass;
extern objMetaClass *glScriptClass;
extern objMetaClass *glArchiveClass;
extern objMetaClass *glCompressionClass;
extern objMetaClass *glCompressedStreamClass;
#ifdef __ANDROID__
extern objMetaClass *glAssetClass;
#endif
extern BYTE fs_initialised;
extern APTR glPageFault;
extern BYTE glScanClasses;
extern UBYTE glTimerCycle;
extern LONG glDebugMemory;
extern struct CoreBase *LocalCoreBase;

//********************************************************************************************************************
// Thread specific variables - these do not require locks.

extern THREADVAR class ObjectContext *tlContext;
extern THREADVAR struct Message *tlCurrentMsg;
extern THREADVAR WORD tlMsgRecursion;
extern THREADVAR WORD tlDepth;
extern THREADVAR WORD tlLogStatus;
extern THREADVAR BYTE tlMainThread;
extern THREADVAR WORD tlPreventSleep;
extern THREADVAR WORD tlPublicLockCount;
extern THREADVAR WORD tlPrivateLockCount;
extern THREADVAR LONG glForceUID, glForceGID, glDefaultPermissions;

//********************************************************************************************************************

extern LONG (**ManagedActions)(OBJECTPTR Object, APTR Parameters);
extern ERROR (*glMessageHandler)(struct Message *);
extern void (*glVideoRecovery)(void);
extern void (*glKeyboardRecovery)(void);
extern void (*glNetProcessMessages)(LONG, APTR);

#ifdef _WIN32
extern WINHANDLE glProcessHandle;
extern WINHANDLE glValidationSemaphore;
extern THREADVAR WORD tlMessageBreak;
extern THREADVAR WINHANDLE tlThreadReadMsg, tlThreadWriteMsg;
#endif

#ifdef __unix__
extern THREADVAR LONG glSocket;
extern THREADVAR LONG tlThreadReadMsg, tlThreadWriteMsg;
extern struct FileMonitor *glFileMonitor;
#endif

//********************************************************************************************************************
// Message handler chain structure.

extern struct MsgHandler *glMsgHandlers, *glLastMsgHandler;

//********************************************************************************************************************
// Message structure and internal ID's for standard Task-to-Task messages.

#define SIZE_MSGBUFFER (1024 * 64)

struct TaskMessage {
   LARGE Time;
   LONG UniqueID;   // Unique identifier for this particular message
   LONG Type;       // Message type ID
   LONG DataSize;   // Size of the data (does not include the size of the TaskMessage structure)
   LONG NextMsg;    // Offset to the next message
   /*
   #ifdef _WIN32
      LONG MsgProcess;
      WINHANDLE MsgSemaphore;
   #endif
   #ifdef __unix__
      THREADLOCK Mutex;
   #endif
   */
   // Data follows
};

struct MessageHeader {
   LONG NextEntry;     // Byte offset for the next message to be stored
   WORD Count;         // Count of messages stored in the buffer
   WORD TaskIndex;     // Process that owns this message queue (refers to an index in the Task array)
   LONG CompressReset; // Manages message queue compression
   BYTE Buffer[SIZE_MSGBUFFER + sizeof(struct TaskMessage)];
};

struct ValidateMessage {
   LONG ProcessID;
};

//********************************************************************************************************************
// ObjectContext is used to represent the object that has the current context in terms of the run-time call stack.
// It is primarily used for the resource tracking of newly allocated memory and objects, as well as for message logs
// and analysis of the call stack.

class ObjectContext {
   public:
   class ObjectContext *Stack; // Call stack.
   struct Field *Field;        // Set if the context is linked to a get/set field operation.  For logging purposes only.
   WORD Action;                // Set if the context enters an action or method routine.

   protected:
   OBJECTPTR Object;           // Required.  The object that currently has the operating context.

   public:
   ObjectContext() { // Dummy initialisation
      Stack  = NULL;
      Object = &glDummyObject;
      Field  = NULL;
      Action = 0;
   }

   ObjectContext(OBJECTPTR pObject, WORD pAction, struct Field *pField = NULL) {
      Stack  = tlContext;
      Object = pObject;
      Field  = pField;
      Action = pAction;

      tlContext = this;
   }

   ~ObjectContext() {
      if (Stack) tlContext = Stack;
   }

   // Return the nearest object for resourcing purposes.  Note that an action ID of 0 has special meaning and indicates
   // that resources should be tracked to the next object on the stack (this feature is used by GetField*() functionality).

   inline OBJECTPTR resource() {
      if (Action) return Object;
      else {
         for (auto ctx = Stack; ctx; ctx=ctx->Stack) {
            if (Action) return ctx->Object;
         }
         return &glDummyObject;
      }
   }

   inline OBJECTPTR setContext(OBJECTPTR NewObject) {
      auto old = Object;
      Object = NewObject;
      return old;
   }

   constexpr inline OBJECTPTR object() { // Return the object that has the context (but not necessarily for resourcing)
      return Object;
   }
};

//********************************************************************************************************************

#ifdef __ANDROID__
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "Parasol:Core", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "Parasol:Core", __VA_ARGS__)
#endif

//********************************************************************************************************************

#define ZIP_PARASOL 0x7e // Use this identifier to declare Parasol zipped files

// The following flags can be tagged to each file entry in the zip file and are Parasol-specific (identifiable by the
// ZIP_PARASOL OS tag).  NOTE: The low order bits aren't used because WinZip, WinRar and so forth assume that
// those bits have meaning.

#define ZIP_LINK   0x00010000 // The entry is a symbolic link
#define ZIP_UEXEC  0x00020000 // Executable-access allowed (user)
#define ZIP_GEXEC  0x00040000 // Executable-access allowed (group)
#define ZIP_OEXEC  0x00080000 // Executable-access allowed (others/everyone)
#define ZIP_UREAD  0x00100000 // Read-access allowed (user)
#define ZIP_GREAD  0x00200000 // Read-access allowed (group)
#define ZIP_OREAD  0x00400000 // Read-access allowed (others/everyone)
#define ZIP_UWRITE 0x00800000 // Write-access allowed (user)
#define ZIP_GWRITE 0x01000000 // Write-access allowed (group)
#define ZIP_OWRITE 0x02000000 // Write-access allowed (others/everyone)

#define ZIP_SECURITY (ZIP_UEXEC | ZIP_GEXEC | ZIP_OEXEC | ZIP_UREAD | ZIP_GREAD | ZIP_OREAD | ZIP_UWRITE | ZIP_GWRITE | ZIP_OWRITE)

// This structure is used by the FileList field

struct CompressedFile {
   struct CompressedFile *Next;
   struct CompressedFile *Prev;
   STRING Name;
   STRING Comment;
   ULONG  CompressedSize;
   ULONG  OriginalSize;
   LONG   Year;
   UBYTE  Month;
   UBYTE  Day;
   UBYTE  Hour;
   UBYTE  Minute;
};

struct ZipFile : public CompressedFile {
   ULONG TimeStamp;     // Time stamp information
   ULONG CRC;           // CRC validation number
   ULONG Offset;        // Byte offset of the file within the archive
   UWORD NameLen;       // Length of name string
   UWORD CommentLen;    // Length of comment string
   UWORD DeflateMethod; // Set to 8 for normal deflation
   LONG  Flags;         // These match the zip 'attrib' value
   UBYTE IsFolder:1;
};

#define SIZE_COMPRESSION_BUFFER 16384

//********************************************************************************************************************
// File header.  Compressed data is prefixed with this information.

#define HEAD_DEFLATEMETHOD  8
#define HEAD_TIMESTAMP      10
#define HEAD_CRC            14
#define HEAD_COMPRESSEDSIZE 18
#define HEAD_FILESIZE       22
#define HEAD_NAMELEN        26   // File name
#define HEAD_EXTRALEN       28   // System specific information
#define HEAD_LENGTH         30   // END

//********************************************************************************************************************
// Central folder structure for each archived file.  This appears at the end of the zip file.

#define LIST_SIGNATURE      0
#define LIST_VERSION        4
#define LIST_OS             5
#define LIST_REQUIRED_VER   6
#define LIST_REQUIRED_OS    7
#define LIST_FLAGS          8
#define LIST_METHOD         10
#define LIST_TIMESTAMP      12
#define LIST_CRC            16  // Checksum
#define LIST_COMPRESSEDSIZE 20
#define LIST_FILESIZE       24  // Original file size
#define LIST_NAMELEN        28  // File name
#define LIST_EXTRALEN       30  // System specific information
#define LIST_COMMENTLEN     32  // Optional comment
#define LIST_DISKNO         34  // Disk number start
#define LIST_IFILE          36  // Internal file attributes (pkzip specific)
#define LIST_ATTRIB         38  // System specific file attributes
#define LIST_OFFSET         42  // Relative offset of local header
#define LIST_LENGTH         46  // END

struct zipentry {
   UBYTE version;
   UBYTE ostype;
   UBYTE required_version;
   UBYTE required_os;
   UWORD flags;
   UWORD deflatemethod;
   ULONG timestamp;
   ULONG crc32;
   ULONG compressedsize;
   ULONG originalsize;
   UWORD namelen;
   UWORD extralen;
   UWORD commentlen;
   UWORD diskno;
   UWORD ifile;
   ULONG attrib;
   ULONG offset;
} __attribute__((__packed__));

//********************************************************************************************************************

#define TAIL_FILECOUNT      8
#define TAIL_TOTALFILECOUNT 10
#define TAIL_FILELISTSIZE   12
#define TAIL_FILELISTOFFSET 16
#define TAIL_COMMENTLEN     20
#define TAIL_LENGTH         22

struct ziptail {
   ULONG header;
   ULONG size;
   UWORD filecount;
   UWORD diskfilecount;
   ULONG listsize;
   ULONG listoffset;
   UWORD commentlen;
} __attribute__((__packed__));

//********************************************************************************************************************
// File Descriptor table.  This is for RegisterFD()

#define MAX_FDS 40
extern struct FDTable *glFDTable;
extern WORD glTotalFDs, glLastFD;
extern LONG glInotify;

#define LRT_Exclusive 1

//********************************************************************************************************************

class ModuleMaster : public BaseClass {
   public:
   class ModuleMaster *Next;   // Next module in list
   class ModuleMaster *Prev;   // Previous module in list
   struct ModHeader  *Header;  // Pointer to module header - for memory resident modules only.
   struct CoreBase *CoreBase;  // Module's personal Core reference
   #ifdef __unix__
      APTR LibraryBase;        // Module code
   #else
      MODHANDLE LibraryBase;
   #endif
   CSTRING Name;               // Name of the module (as declared by the header)
   struct ModHeader *Table;
   WORD   Version;
   WORD   OpenCount;           // Amount of programs with this module open
   FLOAT  ModVersion;          // Version of this module
   LONG   Flags;
   UBYTE  NoUnload;
   UBYTE  DLL;                 // TRUE if the module is a Windows DLL
   LONG   (*Init)(OBJECTPTR, struct CoreBase *);
   void   (*Close)(OBJECTPTR);
   LONG   (*Open)(OBJECTPTR);
   LONG   (*Expunge)(void);
   struct ActionEntry prvActions[AC_END]; // Action routines to be intercepted by the program
   char   LibraryName[40]; // Name of the library loaded from disk
};

#ifdef  __cplusplus
extern "C" {
#endif

//********************************************************************************************************************
// Action managers.

ERROR MGR_Init(OBJECTPTR, APTR);
ERROR MGR_Free(OBJECTPTR, APTR);
ERROR MGR_GetField(OBJECTPTR, struct acGetVar *);
ERROR MGR_OwnerDestroyed(OBJECTPTR, APTR);
ERROR MGR_Seek(OBJECTPTR, struct acSeek *);
ERROR MGR_SetField(OBJECTPTR, struct acSetVar *);
ERROR MGR_Signal(OBJECTPTR, APTR);

//********************************************************************************************************************

ERROR AccessSemaphore(LONG, LONG, LONG);
ERROR AllocSemaphore(CSTRING, LONG, LONG, LONG *);
ERROR FreeSemaphore(LONG SemaphoreID);
ERROR SetFieldF(OBJECTPTR, FIELD, va_list);
ERROR SetFieldsF(OBJECTPTR, va_list);
ERROR CreateObjectF(LARGE, LONG, OBJECTPTR *, va_list List);
ERROR pReleaseSemaphore(LONG, LONG);

ERROR fs_closedir(struct DirInfo *);
ERROR fs_createlink(CSTRING, CSTRING);
ERROR fs_delete(STRING, FUNCTION *);
ERROR fs_getinfo(CSTRING, struct FileInfo *, LONG);
ERROR fs_getdeviceinfo(CSTRING, objStorageDevice *);
void  fs_ignore_file(class extFile *);
ERROR fs_makedir(CSTRING, LONG);
ERROR fs_opendir(struct DirInfo *);
ERROR fs_readlink(STRING, STRING *);
ERROR fs_rename(STRING, STRING);
ERROR fs_samefile(CSTRING, CSTRING);
ERROR fs_scandir(struct DirInfo *);
ERROR fs_testpath(CSTRING, LONG, LONG *);
ERROR fs_watch_path(class extFile *);

const struct virtual_drive * get_fs(CSTRING Path);
void free_storage_class(void);

ERROR convert_zip_error(struct z_stream_s *, LONG);
ERROR check_cache(OBJECTPTR, LARGE, LARGE);
ERROR get_class_cmd(CSTRING, objConfig *, LONG, CLASSID, STRING *);
ERROR fs_copy(CSTRING, CSTRING, FUNCTION *, BYTE);
ERROR fs_copydir(STRING, STRING, struct FileFeedback *, FUNCTION *, BYTE);
LONG  get_parent_permissions(CSTRING, LONG *, LONG *);
ERROR load_datatypes(void);
ERROR RenameVolume(CSTRING, CSTRING);
ERROR findfile(STRING);
LONG  convert_fs_permissions(LONG);
LONG  convert_permissions(LONG);
void set_memory_manager(APTR, struct ResourceManager *);
BYTE  strip_folder(STRING) __attribute__ ((unused));
ERROR get_file_info(CSTRING, struct FileInfo *, LONG);
ERROR convert_errno(LONG Error, ERROR Default);
void free_file_cache(void);

EXPORT void Expunge(WORD);

extern void add_archive(class extCompression *);
extern void remove_archive(class extCompression *);
extern void zipfile_to_item(struct ZipFile *ZF, struct CompressedItem *Item);

CSTRING action_name(OBJECTPTR Object, LONG ActionID);
APTR   build_jump_table(LONG, const struct Function *, LONG);
ERROR  copy_args(const struct FunctionField *, LONG, BYTE *, BYTE *, LONG, LONG *, WORD *, CSTRING);
ERROR  copy_field_to_buffer(OBJECTPTR Object, struct Field *Field, LONG DestFlags, APTR Result, CSTRING Option, LONG *TotalElements);
ERROR  create_archive_volume(void);
ERROR  critical_janitor(OBJECTID, LONG, LONG);
ERROR  delete_tree(STRING, LONG, FUNCTION *, struct FileFeedback *);
struct ClassItem * find_class(CLASSID);
struct ModuleItem * find_module(ULONG);
LONG   find_public_address(struct SharedControl *, APTR);
ERROR  find_private_object_entry(OBJECTID, LONG *);
ERROR  find_public_object_entry(struct SharedObjectHeader *, OBJECTID, LONG *);
ERROR  find_public_mem_id(struct SharedControl *, MEMORYID, LONG *);
void   fix_core_table(struct CoreBase *, FLOAT);
void   free_events(void);
void   free_module_entry(struct ModuleMaster *);
ERROR  free_ptr_args(APTR, const struct FunctionField *, WORD);
void   free_public_resources(OBJECTID);
void   free_wakelocks(void);
LONG   get_thread_id(void);
void   init_metaclass(void);
ERROR  init_sleep(LONG OtherProcessID, LONG GlobalThreadID, LONG ResourceID, LONG ResourceType, WORD *);
ERROR  load_classes(void);
ERROR  local_copy_args(const struct FunctionField *, LONG, BYTE *, BYTE *, LONG, LONG *, CSTRING);
void   local_free_args(APTR, const struct FunctionField *);
struct Field * lookup_id(OBJECTPTR Object, ULONG FieldID, OBJECTPTR *Result);
ERROR  msg_event(APTR Custom, LONG MsgID, LONG MsgType, APTR Message, LONG MsgSize);
ERROR  msg_threadcallback(APTR Custom, LONG MsgID, LONG MsgType, APTR Message, LONG MsgSize);
ERROR  msg_threadaction(APTR Custom, LONG MsgID, LONG MsgType, APTR Message, LONG MsgSize);
void   optimise_write_field(struct Field *Field);
ERROR  page_memory(struct PublicAddress *, APTR *);
void   PrepareSleep(void);
ERROR  process_janitor(OBJECTID, LONG, LONG);
ERROR  register_class(CSTRING, CLASSID ParentID, LONG Category, CSTRING Path, CSTRING FileMatch, CSTRING);
ERROR  remove_memlock(void);
void   remove_object_hash(OBJECTPTR);
void   remove_process_waitlocks(void);
void   remove_public_locks(LONG);
void   remove_semaphores(void);
void   remove_shared_object(OBJECTID);
ERROR  resolve_args(APTR, const struct FunctionField *);
APTR   resolve_public_address(struct PublicAddress *);
void   scan_classes(void);
void   set_object_flags(OBJECTPTR, LONG);
ERROR  sort_class_fields(extMetaClass *, struct Field *);
void   remove_threadpool(void);
ERROR  threadpool_get(extThread **);
void   threadpool_release(extThread *);
ERROR  unpage_memory(APTR);
ERROR  unpage_memory_id(MEMORYID MemoryID);
ERROR  UnsubscribeActionByID(OBJECTPTR Object, ACTIONID ActionID, OBJECTID SubscriberID);
void   wake_sleepers(LONG ResourceID, LONG ResourceType);
ERROR  write_class_item(struct ClassItem *);
ERROR  writeval_default(OBJECTPTR, struct Field *, LONG, const void *, LONG);
ERROR  validate_process(LONG);
void   free_iconv(void);

#define REF_WAKELOCK           get_threadlock()

#define LOCK_PUBLIC_MEMORY(t)  SysLock(PL_PUBLICMEM,(t))
#define UNLOCK_PUBLIC_MEMORY() SysUnlock(PL_PUBLICMEM)

#define LOCK_PROCESS_TABLE(t)  SysLock(PL_PROCESSES,(t))
#define UNLOCK_PROCESS_TABLE() SysUnlock(PL_PROCESSES)

#define LOCK_SEMAPHORES(t)  SysLock(PL_SEMAPHORES,(t))
#define UNLOCK_SEMAPHORES() SysUnlock(PL_SEMAPHORES)

#ifdef _WIN32
   ERROR alloc_public_lock(WINHANDLE *, CSTRING);
   ERROR open_public_lock(WINHANDLE *, CSTRING);
   ERROR open_public_waitlock(WINHANDLE *, CSTRING);
   void  free_public_lock(WINHANDLE);
   ERROR public_thread_lock(WINHANDLE, LONG);
   void  public_thread_unlock(WINHANDLE);
   WINHANDLE get_threadlock(void);
   void  free_threadlocks(void);
   ERROR wake_waitlock(WINHANDLE Lock, LONG ProcessID, LONG TotalSleepers);
   ERROR alloc_public_waitlock(WINHANDLE *Lock, const char *Name);
   void  free_public_waitlock(WINHANDLE Lock);
   ERROR send_thread_msg(WINHANDLE Handle, LONG Type, APTR Data, LONG Size);
   LONG  sleep_waitlock(WINHANDLE, LONG);
#else
   struct sockaddr_un * get_socket_path(LONG ProcessID, socklen_t *Size);
   ERROR alloc_public_lock(UBYTE, WORD Flags);
   ERROR alloc_public_cond(CONDLOCK *, WORD Flags);
   void  free_public_lock(UBYTE);
   void  free_public_cond(CONDLOCK *);
   ERROR public_cond_wait(THREADLOCK *Lock, CONDLOCK *Cond, LONG Timeout);
   ERROR send_thread_msg(LONG Handle, LONG Type, APTR Data, LONG Size);
#endif

ERROR alloc_private_lock(UBYTE, WORD);
ERROR alloc_private_cond(UBYTE, WORD);
void  free_private_lock(UBYTE);
void  free_private_cond(UBYTE);
ERROR thread_lock(UBYTE, LONG);
void  thread_unlock(UBYTE);
ERROR cond_wait(UBYTE, UBYTE, LONG Timeout);

void cond_wake_all(UBYTE);
void cond_wake_single(UBYTE);
ERROR clear_waitlock(WORD);

// Platform specific semaphore functionality.

ERROR plAllocPrivateSemaphore(APTR, LONG InitialValue);
void  plFreePrivateSemaphore(APTR);
ERROR plLockSemaphore(APTR, LONG TimeOut);
void  plUnlockSemaphore(APTR);

ERROR check_paths(CSTRING, LONG);

ERROR convert_errno(LONG Error, ERROR Default);

#ifdef _WIN32
void activate_console(BYTE);
void free_threadlock(void);
WINHANDLE winAllocPublic(LONG);
LONG winCheckProcessExists(LONG);
LONG winCloseHandle(WINHANDLE);
LONG winCreatePipe(WINHANDLE *Read, WINHANDLE *Write);
LONG winCreateSharedMemory(STRING, LONG, LONG, WINHANDLE *, APTR *);
WINHANDLE winCreateThread(APTR Function, APTR Arg, LONG StackSize, LONG *ID);
LONG winGetCurrentThreadId(void);
void winDeathBringer(LONG Value);
LONG winDuplicateHandle(LONG, LONG, LONG, LONG *);
void winEnterCriticalSection(APTR);
STRING winFormatMessage(LONG, char *, LONG);
LONG winFreeLibrary(WINHANDLE);
void winFreeProcess(APTR);
LONG winGetEnv(CSTRING, STRING, LONG);
LONG winGetExeDirectory(LONG, STRING);
LONG winGetCurrentDirectory(LONG, STRING);
WINHANDLE winGetCurrentProcess(void);
LONG winGetCurrentProcessId(void);
LONG winGetExitCodeProcess(WINHANDLE, LONG *Code);
long long winGetFileSize(STRING);
LONG winGetPageSize(void);
APTR winGetProcAddress(WINHANDLE, CSTRING);
WINHANDLE winGetStdInput(void);
LARGE winGetTickCount(void);
void winInitialise(int *, void *);
void winInitializeCriticalSection(APTR Lock);
void winDeleteCriticalSection(APTR Lock);
LONG winLaunchProcess(APTR, STRING, STRING, BYTE Group, BYTE Redirect, APTR *ProcessResult, BYTE, STRING, STRING, LONG *);
void winLeaveCriticalSection(APTR);
WINHANDLE winLoadLibrary(STRING);
void winLowerPriority(void);
ERROR winMapMemory(WINHANDLE, LONG, APTR *);
WINHANDLE winOpenSemaphore(unsigned char *Name);
void winProcessMessages(void);
LONG winReadStd(APTR, LONG, APTR Buffer, LONG *Size);
LONG winReadPipe(WINHANDLE FD, APTR Buffer, LONG *Size);
void winResetStdOut(APTR, APTR Buffer, LONG *Size);
void winResetStdErr(APTR, APTR Buffer, LONG *Size);
LONG winWritePipe(WINHANDLE FD, CPTR Buffer, LONG *Size);
void winSelect(WINHANDLE FD, char *Read, char *Write);
void winSetEnv(CSTRING, CSTRING);
void winSetUnhandledExceptionFilter(LONG (*Function)(LONG, APTR, LONG, LONG *));
void winShutdown(void);
void winSleep(LONG);
int winTerminateApp(int dwPID, int dwTimeout);
void winTerminateThread(WINHANDLE);
LONG winTryEnterCriticalSection(APTR);
LONG winUnmapViewOfFile(APTR);
LONG winWaitForSingleObject(WINHANDLE, LONG);
LONG winWaitForObjects(LONG Total, WINHANDLE *, LONG Time, BYTE WinMsgs);
LONG winWaitThread(WINHANDLE, LONG);
LONG winWriteStd(APTR, CPTR Buffer, LONG Size);
int winDeleteFile(char *Path);
LONG winCheckDirectoryExists(CSTRING);
LONG winCreateDir(CSTRING);
LONG winCurrentDirectory(STRING, LONG);
LONG winFileInfo(CSTRING, long long *, struct DateTime *, BYTE *);
void winFindClose(WINHANDLE);
void winFindNextChangeNotification(WINHANDLE);
void winGetAttrib(CSTRING, LONG *);
BYTE winGetCommand(char *, char *, LONG);
LONG winGetFreeDiskSpace(UBYTE, LARGE *, LARGE *);
LONG winGetLogicalDrives(void);
LONG winGetLogicalDriveStrings(STRING, LONG);
LONG winGetDriveType(STRING);
LONG winGetFullPathName(const char *Path, LONG PathLength, char *Output, char **NamePart);
LONG winGetUserFolder(STRING, LONG);
LONG winGetUserName(STRING, LONG);
LONG winGetWatchBufferSize(void);
LONG winMoveFile(STRING, STRING);
LONG winReadChanges(WINHANDLE, APTR, LONG NotifyFlags, char *, LONG, LONG *);
LONG winReadKey(CSTRING, CSTRING, STRING, LONG);
LONG winReadRootKey(CSTRING, STRING, STRING, LONG);
LONG winReadStdInput(WINHANDLE FD, APTR Buffer, LONG BufferSize, LONG *Size);
LONG winScan(APTR *, STRING, STRING, LARGE *, struct DateTime *, struct DateTime *, BYTE *, BYTE *, BYTE *, BYTE *);
LONG winSetAttrib(CSTRING, LONG);
LONG winSetEOF(CSTRING, LARGE);
LONG winTestLocation(CSTRING, BYTE);
LONG winWatchFile(LONG, CSTRING, APTR, WINHANDLE *, LONG *);
void winFindCloseChangeNotification(WINHANDLE);
APTR winFindDirectory(STRING, APTR *, STRING);
APTR winFindFile(STRING, APTR *, STRING);
LONG winSetFileTime(CSTRING, WORD Year, WORD Month, WORD Day, WORD Hour, WORD Minute, WORD Second);
LONG winResetDate(STRING);
void winSetDllDirectory(CSTRING);
void winEnumSpecialFolders(void (*callback)(CSTRING, CSTRING, CSTRING, CSTRING, BYTE));

#endif

#ifdef  __cplusplus
}
#endif

//********************************************************************************************************************

extern THREADVAR char tlFieldName[10]; // $12345678\0

INLINE CSTRING GET_FIELD_NAME(ULONG FieldID)
{
   CSTRING name;
   if (!KeyGet(glFields, FieldID, (APTR *)&name, NULL)) return name;
   else {
      snprintf(tlFieldName, sizeof(tlFieldName), "$%.8x", FieldID);
      return tlFieldName;
   }
}

//********************************************************************************************************************

class ScopedObjectAccess {
   private:
      OBJECTPTR obj;

   public:
      ERROR error;

      ScopedObjectAccess(OBJECTPTR Object) {
         error = Object->threadLock();
         obj = Object;
      }

      ~ScopedObjectAccess() { if (!error) obj->threadRelease(); }

      bool granted() { return error == ERR_Okay; }

      void release() {
         if (!error) {
            obj->threadRelease();
            error = ERR_NotLocked;
         }
      }
};

//********************************************************************************************************************

inline LARGE calc_timestamp(struct DateTime *Date) {
   return(Date->Second +
          ((LARGE)Date->Minute * 60LL) +
          ((LARGE)Date->Hour * 60LL * 60LL) +
          ((LARGE)Date->Day * 60LL * 60LL * 24LL) +
          ((LARGE)Date->Month * 60LL * 60LL * 24LL * 31LL) +
          ((LARGE)Date->Year * 60LL * 60LL * 24LL * 31LL * 12LL));
}

inline UWORD reverse_word(UWORD Value) {
    return (((Value & 0x00FF) << 8) | ((Value & 0xFF00) >> 8));
}

inline ULONG reverse_long(ULONG Value) {
    return (((Value & 0x000000FF) << 24) |
            ((Value & 0x0000FF00) <<  8) |
            ((Value & 0x00FF0000) >>  8) |
            ((Value & 0xFF000000) >> 24));
}

//********************************************************************************************************************

class ThreadLock { // C++ wrapper for terminating resources when scope is lost
   private:
      UBYTE lock_type;

   public:
      ERROR error;

      ThreadLock(UBYTE Lock, LONG Timeout) {
         lock_type = Lock;
         error = thread_lock(Lock, Timeout);
      }

      ~ThreadLock() { if (!error) thread_unlock(lock_type); }

      bool granted() { return error == ERR_Okay; }

      void release() {
         if (!error) {
            thread_unlock(lock_type);
            error = ERR_NotLocked;
         }
      }
};

#endif // DEFS_H
