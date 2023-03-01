#ifndef DEFS_H
#define DEFS_H 1

#ifndef PLATFORM_CONFIG_H
#include <parasol/config.h>
#endif

#include <set>
#include <functional>
#include <mutex>
#include <sstream>

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
#define SIZE_SYSTEM_PATH 100  // Max characters for the Parasol system path

#define MAX_SEMAPHORES    40  // Maximum number of semaphores that can be allocated in the system
#define MAX_THREADS       20  // Maximum number of threads per process.
#define MAX_NB_LOCKS      20  // Non-blocking locks apply when locking 'free-for-all' public memory blocks.  The maximum value is per-task, so keep the value low.
#define MAX_WAITLOCKS     60  // This value is effectively imposing a limit on the maximum number of threads/processes that can be active at any time.

#define CLASSDB_HEADER 0x7f887f88

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

enum class NF : ULONG;
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
   TL_VOLUMES,
   TL_CLASSDB,
   TL_FIELDKEYS,
   TL_END
};

enum {
   CN_PRIVATE_MEM=0,
   CN_OBJECTS,
   CN_END
};

//********************************************************************************************************************

struct ActionSubscription {
   OBJECTPTR Context;
   void (*Callback)(OBJECTPTR, ACTIONID, ERROR, APTR);

   ActionSubscription() : Context(NULL), Callback(NULL) { }
   ActionSubscription(OBJECTPTR pContext, void (*pCallback)(OBJECTPTR, ACTIONID, ERROR, APTR)) : Context(pContext), Callback(pCallback) { }
   ActionSubscription(OBJECTPTR pContext, APTR pCallback) : Context(pContext), Callback((void (*)(OBJECTPTR, ACTIONID, ERROR, APTR))pCallback) { }
};

struct virtual_drive {
   ULONG VirtualID;   // Hash name of the volume, not including the trailing colon
   LONG  DriverSize;  // The driver may reserve a private area for its own structure attached to DirInfo.
   char  Name[32];    // Volume name, including the trailing colon at the end
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
   inline bool is_default() const { return VirtualID IS 0; }
   inline bool is_virtual() const { return VirtualID != 0; }
};

extern const virtual_drive glFSDefault;
extern std::unordered_map<ULONG, virtual_drive> glVirtual;

//********************************************************************************************************************
// Resource definitions.

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
   bool Event; // Set to true if the lock is for a broadcast-able event
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
   using create = pf::Create<extMetaClass>;
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
   using create = pf::Create<extFile>;
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
   using create = pf::Create<extConfig>;
   ULONG    CRC;   // CRC32, for determining if config data has been altered
};

class extStorageDevice : public objStorageDevice {
   public:
   using create = pf::Create<extStorageDevice>;
   STRING DeviceID;   // Unique ID for the filesystem, if available
   STRING Volume;
};

class extThread : public objThread {
   public:
   using create = pf::Create<extThread>;
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
   using create = pf::Create<extTask>;
   MEMORYID MessageMID;
   STRING   LaunchPath;
   STRING   Path;
   STRING   ProcessPath;
   STRING   Location;         // Where to load the task from (string)
   CSTRING  *Parameters;      // Arguments (string array)
   char     Name[32];         // Name of the task, if specified (string)
   LONG     ParametersSize;   // Byte size of the arguments structure
   STRING   Fields[100];      // Variable field storage
   BYTE     ReturnCodeSet;    // TRUE if the ReturnCode has been set
   FUNCTION ErrorCallback;
   FUNCTION OutputCallback;
   FUNCTION ExitCallback;
   FUNCTION InputCallback;
   struct MsgHandler *MsgAction;
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
   using create = pf::Create<extModule>;
   char   Name[60];      // Name of the module
   APTR   prvMBMemory;   // Module base memory
};

//********************************************************************************************************************
// Class database.

struct ClassRecord {
   CLASSID ClassID;
   CLASSID ParentID;
   LONG Category;
   std::string Name;
   std::string Path;
   std::string Match;
   std::string Header;

   static const LONG MIN_SIZE = sizeof(CLASSID) + sizeof(CLASSID) + sizeof(LONG) + (sizeof(LONG) * 4);

   ClassRecord() { }

   inline ClassRecord(extMetaClass *pClass, std::optional<std::string> pPath = std::nullopt) {
      ClassID  = pClass->SubClassID;
      ParentID = (pClass->BaseClassID IS pClass->SubClassID) ? 0 : pClass->BaseClassID;
      Category = pClass->Category;

      Name.assign(pClass->ClassName);

      if (pPath.has_value()) pPath.value();
      else if (pClass->Path) Path.assign(pClass->Path);

      if (pClass->FileExtension) Match.assign(pClass->FileExtension);
      if (pClass->FileHeader) Header.assign(pClass->FileHeader);
   }

   inline ClassRecord(CLASSID pClassID, std::string pName, CSTRING pMatch = NULL, CSTRING pHeader = NULL) {
      ClassID  = pClassID;
      ParentID = 0;
      Category = CCF_SYSTEM;
      Name     = pName;
      Path     = "modules:core";
      if (pMatch) Match   = pMatch;
      if (pHeader) Header = pHeader;
   }

   inline ERROR write(objFile *File) {
      if (File->write(&ClassID, sizeof(ClassID), NULL)) return ERR_Write;
      if (File->write(&ParentID, sizeof(ParentID), NULL)) return ERR_Write;
      if (File->write(&Category, sizeof(Category), NULL)) return ERR_Write;

      LONG size = Name.size();
      File->write(&size, sizeof(size));
      File->write(Name.c_str(), size);

      size = Path.size();
      File->write(&size, sizeof(size));
      if (size) File->write(Path.c_str(), size);

      size = Match.size();
      File->write(&size, sizeof(size));
      if (size) File->write(Match.c_str(), size);

      size = Header.size();
      File->write(&size, sizeof(size));
      if (size) File->write(Header.c_str(), size);

      return ERR_Okay;
   }

   inline ERROR read(objFile *File) {
      if (File->read(&ClassID, sizeof(ClassID))) return ERR_Read;
      if (File->read(&ParentID, sizeof(ParentID))) return ERR_Read;
      if (File->read(&Category, sizeof(Category))) return ERR_Read;

      char buffer[256];
      LONG size = 0;
      File->read(&size, sizeof(size));
      if (size < (LONG)sizeof(buffer)) {
         File->read(buffer, size);
         Name.assign(buffer, size);
      }
      else return ERR_BufferOverflow;

      File->read(&size, sizeof(size));
      if (size < (LONG)sizeof(buffer)) {
         File->read(buffer, size);
         Path.assign(buffer, size);
      }
      else return ERR_BufferOverflow;

      File->read(&size, sizeof(size));
      if (size < (LONG)sizeof(buffer)) {
         File->read(buffer, size);
         Match.assign(buffer, size);
      }
      else return ERR_BufferOverflow;

      File->read(&size, sizeof(size));
      if (size < (LONG)sizeof(buffer)) {
         File->read(buffer, size);
         Header.assign(buffer, size);
      }
      else return ERR_BufferOverflow;

      return ERR_Okay;
   }
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

struct CaseInsensitiveMap {
   bool operator() (const std::string &lhs, const std::string &rhs) const {
      return ::strcasecmp(lhs.c_str(), rhs.c_str()) < 0;
   }
};

//********************************************************************************************************************
// Global data variables.

extern extMetaClass glMetaClass;
extern LONG glEUID, glEGID, glUID, glGID;
extern std::string glSystemPath;
extern std::string glModulePath;
extern std::string glRootPath;
extern char glDisplayDriver[28];
extern bool glShowIO, glShowPrivate, glShowPublic;
extern WORD glLogLevel, glMaxDepth;
extern UBYTE glTaskState;
extern LARGE glTimeLog;
extern struct ModuleMaster   *glModuleList;    // Locked with TL_GENERIC.  Maintained as a linked-list; hashmap unsuitable.
extern struct PublicAddress  *glSharedBlocks;  // Locked with PL_PUBLICMEM
extern struct SortedAddress  *glSortedBlocks;
extern struct SharedControl  *glSharedControl; // Locked with PL_FORBID
extern struct TaskList       *shTasks, *glTaskEntry; // Locked with PL_PROCESSES
extern struct SemaphoreEntry *shSemaphores;    // Locked with PL_SEMAPHORES
extern struct MemoryPage     *glMemoryPages;   // Locked with TL_MEMORY_PAGES
extern struct OpenInfo       *glOpenInfo;      // Read-only.  The OpenInfo structure initially passed to OpenCore()
extern objTask *glCurrentTask;
extern const struct ActionTable ActionTable[];
extern const struct Function    glFunctions[];
extern std::list<CoreTimer> glTimers;           // Locked with TL_TIMER
extern std::map<std::string, std::vector<BaseClass *>, CaseInsensitiveMap> glObjectLookup;  // Locked with TL_OBJECT_LOOKUP
extern std::unordered_map<MEMORYID, PrivateAddress> glPrivateMemory;  // Locked with TL_PRIVATE_MEM: Note that best performance for looking up ID's is achieved as a sorted array.
extern std::unordered_map<OBJECTID, std::set<MEMORYID, std::greater<MEMORYID>>> glObjectMemory; // Locked with TL_PRIVATE_MEM.  Sorted with the most recent private memory first
extern std::unordered_map<OBJECTID, std::set<OBJECTID, std::greater<OBJECTID>>> glObjectChildren; // Locked with TL_PRIVATE_MEM.  Sorted with most recent object first
extern std::unordered_map<CLASSID, ClassRecord> glClassDB;
extern std::unordered_map<CLASSID, extMetaClass *> glClassMap;
extern std::unordered_map<ULONG, std::string> glFields; // Reverse lookup for converting field hashes back to their respective names.
extern std::unordered_map<OBJECTID, ObjectSignal> glWFOList;
extern std::map<std::string, ConfigKeys, CaseInsensitiveMap> glVolumes; // VolumeName = { Key, Value }
extern CSTRING glMessages[ERR_END];       // Read-only table of error messages.
extern const LONG glTotalMessages;
extern LONG glTotalPages; // Read-only
extern MEMORYID glTaskMessageMID;        // Read-only
extern LONG glProcessID;   // Read only
extern HOSTHANDLE glConsoleFD;
extern LONG glStdErrFlags; // Read only
extern LONG glValidateProcessID; // Not a threading concern
extern LONG glMutexLockSize; // Read only constant
extern WORD glCrashStatus, glCodeIndex, glLastCodeIndex;
extern UWORD glFunctionID;
extern BYTE glProgramStage;
extern bool glPrivileged, glSync;
extern LONG glPageSize; // Read only
extern TIMER glCacheTimer;
extern APTR glJNIEnv;
extern class ObjectContext glTopContext; // Read-only, not a threading concern.
extern OBJECTPTR modIconv;
extern OBJECTPTR glLocale;
extern objTime *glTime;
extern objConfig *glDatatypes;
extern objFile *glClassFile;
extern CSTRING glIDL;
extern struct BaseClass glDummyObject;

extern CSTRING glClassBinPath;
extern objMetaClass *glModuleMasterClass;
extern objMetaClass *glModuleClass;
extern objMetaClass *glTaskClass;
extern objMetaClass *glThreadClass;
extern objMetaClass *glTimeClass;
extern objMetaClass *glConfigClass;
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
extern bool glScanClasses;
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

extern std::array<LONG (*)(struct BaseClass *, APTR), AC_END> ManagedActions;
extern ERROR (*glMessageHandler)(struct Message *);
extern void (*glVideoRecovery)(void);
extern void (*glKeyboardRecovery)(void);
extern void (*glNetProcessMessages)(LONG, APTR);

#ifdef _WIN32
extern WINHANDLE glProcessHandle;
extern WINHANDLE glValidationSemaphore;
extern THREADVAR WORD tlMessageBreak;
#endif

#ifdef __unix__
extern THREADVAR LONG glSocket;
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
// File Descriptor table for RegisterFD()

struct FDRecord {
   HOSTHANDLE FD;                         // The file descriptor that is managed by this record.
   void (*Routine)(HOSTHANDLE, APTR);     // The routine that will process read/write messages for the FD.
   APTR Data;                             // A user specific data pointer.
   LONG Flags;                            // Set to RFD_READ, RFD_WRITE or RFD_EXCEPT.

   FDRecord(HOSTHANDLE pFD, void (*pRoutine)(HOSTHANDLE, APTR), APTR pData, LONG pFlags) :
      FD(pFD), Routine(pRoutine), Data(pData), Flags(pFlags) { }
};

extern std::list<FDRecord> glFDTable;
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
ERROR MGR_Signal(OBJECTPTR, APTR);

//********************************************************************************************************************

ERROR AccessSemaphore(LONG, LONG, LONG);
ERROR AllocSemaphore(CSTRING, LONG, LONG, LONG *);
ERROR FreeSemaphore(LONG SemaphoreID);
ERROR SetFieldF(OBJECTPTR, FIELD, va_list);
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

void print_diagnosis(LONG ProcessID, LONG Signal);
CSTRING action_name(OBJECTPTR Object, LONG ActionID);
APTR   build_jump_table(LONG, const struct Function *, LONG);
ERROR  copy_args(const struct FunctionField *, LONG, BYTE *, BYTE *, LONG, LONG *, WORD *, CSTRING);
ERROR  copy_field_to_buffer(OBJECTPTR Object, struct Field *Field, LONG DestFlags, APTR Result, CSTRING Option, LONG *TotalElements);
ERROR  create_archive_volume(void);
ERROR  delete_tree(STRING, LONG, FUNCTION *, struct FileFeedback *);
struct ClassItem * find_class(CLASSID);
LONG   find_public_address(struct SharedControl *, APTR);
ERROR  find_private_object_entry(OBJECTID, LONG *);
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
ERROR  remove_memlock(void);
void   remove_process_waitlocks(void);
void   remove_public_locks(LONG);
void   remove_semaphores(void);
ERROR  resolve_args(APTR, const struct FunctionField *);
APTR   resolve_public_address(struct PublicAddress *);
void   scan_classes(void);
ERROR  sort_class_fields(extMetaClass *, struct Field *);
void   remove_threadpool(void);
ERROR  threadpool_get(extThread **);
void   threadpool_release(extThread *);
ERROR  unpage_memory(APTR);
ERROR  unpage_memory_id(MEMORYID MemoryID);
void   wake_sleepers(LONG ResourceID, LONG ResourceType);
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
void merge_groups(ConfigGroups &Dest, ConfigGroups &Source);

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
LONG winIsDebuggerPresent(void);
void winDeleteCriticalSection(APTR Lock);
LONG winLaunchProcess(APTR, STRING, STRING, BYTE Group, BYTE Redirect, APTR *ProcessResult, BYTE, STRING, STRING, LONG *);
void winLeaveCriticalSection(APTR);
WINHANDLE winLoadLibrary(CSTRING);
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

//********************************************************************************************************************
// NOTE: To be called with TL_OBJECT_LOOKUP only.

inline void remove_object_hash(OBJECTPTR Object)
{
   std::erase(glObjectLookup[Object->Name], Object);
   if (glObjectLookup[Object->Name].empty()) glObjectLookup.erase(Object->Name);
}

#endif // DEFS_H
