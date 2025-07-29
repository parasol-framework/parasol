#ifndef DEFS_H
#define DEFS_H 1

#ifndef PLATFORM_CONFIG_H
#include <parasol/config.h>
#endif

#include <set>
#include <functional>
#include <mutex>
#include <sstream>
#include <condition_variable>
#include <chrono>
#include <array>
#include <atomic>
#include <thread>

#include <thread_pool/thread_pool.h>

using namespace std::chrono_literals;

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
#endif

#define PRV_METACLASS 1
#define PRV_MODULE 1

#include "microsoft/windefs.h"

// See the makefile for optional defines

constexpr int MAX_TASKS = 50;  // Maximum number of tasks allowed to run at once

constexpr int SIZE_SYSTEM_PATH = 100;  // Max characters for the Parasol system path

constexpr int MAX_THREADS   = 20;  // Maximum number of threads per process.
constexpr int MAX_NB_LOCKS  = 20;  // Non-blocking locks apply when locking 'free-for-all' public memory blocks.  The maximum value is per-task, so keep the value low.
constexpr int MAX_WAITLOCKS = 60;  // This value is effectively imposing a limit on the maximum number of threads/processes that can be active at any time.

#define CLASSDB_HEADER 0x7f887f89

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

constexpr int LEN_VOLUME_NAME = 40;

constexpr int DRIVETYPE_REMOVABLE = 1;
constexpr int DRIVETYPE_CDROM     = 2;
constexpr int DRIVETYPE_FIXED     = 3;
constexpr int DRIVETYPE_NETWORK   = 4;
constexpr int DRIVETYPE_USB       = 5;

#define DEFAULT_VIRTUALID 0xffffffff

#define CODE_MEMH 0x4D454D48L
#define CODE_MEMT 0x4D454D54L

#ifdef _WIN32
   typedef void * MODHANDLE;
   typedef void * THREADLOCK;
   typedef void * CONDLOCK;
#else
   typedef void * MODHANDLE;
   typedef pthread_mutex_t THREADLOCK;
   typedef pthread_cond_t CONDLOCK;
#endif

#define BREAKPOINT { uint8_t *nz = 0; nz[0] = 0; }

#include <parasol/system/errors.h>
#include <parasol/system/types.h>
#include <parasol/system/registry.h>

#include <stdarg.h>

struct ChildEntry;
struct ObjectInfo;
struct MemInfo;
struct ListTasks;
struct DateTime;
struct RGB8;
struct pfBase64Decode;
struct FileInfo;
struct DirInfo;
class objFile;
class objStorageDevice;
class objConfig;
class objMetaClass;
class objTask;
struct ActionTable;
struct FileFeedback;
struct ResourceManager;
struct MsgHandler;

enum class RES    : int;
enum class RP     : int;
enum class IDTYPE : int;
enum class TSTATE : int8_t;
enum class LOC    : int;
enum class STT    : int;
enum class NF     : uint32_t;
enum class FOF    : uint32_t;
enum class RFD    : uint32_t;
enum class PMF    : uint32_t;
enum class MSF    : uint32_t;
enum class RDF    : uint32_t;
enum class RSF    : uint32_t;
enum class LDF    : uint32_t;
enum class VOLUME : uint32_t;
enum class STR    : uint32_t;
enum class SCF    : uint32_t;
enum class SBF    : uint32_t;
enum class SMF    : uint32_t;
enum class VLF    : uint32_t;
enum class MFF    : uint32_t;
enum class DEVICE : int64_t;
enum class PERMIT : uint32_t;
enum class CCF    : uint32_t;
enum class MEM    : uint32_t;
enum class ALF    : uint16_t;
enum class EVG    : int;
enum class AC     : int;
enum class MSGID  : int;

#define STAT_FOLDER 0x0001

struct THREADID : strong_typedef<THREADID, int> { // Internal thread ID, unrelated to the host platform.
   // Make constructors available
   using strong_typedef::strong_typedef;
   bool operator==(const THREADID & other) const { return int(*this) == int(other); }
};

struct rkWatchPath {
   int64_t    Custom;    // User's custom data pointer or value
   HOSTHANDLE Handle;    // The handle for the file being monitored, can be a special reference for virtual paths
   FUNCTION   Routine;   // Routine to call on event trigger
   MFF        Flags;     // Event mask (original flags supplied to Watch)
   uint32_t   VirtualID; // If monitored path is virtual, this refers to an ID in the glVirtual table

#ifdef _WIN32
   int WinFlags;
#endif
};

#include <parasol/vector.hpp>
#include "prototypes.h"

#include <parasol/main.h>
#include <parasol/strings.hpp>

using namespace pf;

struct ThreadMessage {
   OBJECTID ThreadID;    // Internal
   FUNCTION Callback;
};

struct ThreadActionMessage {
   OBJECTPTR Object;    // Direct pointer to a target object.
   AC        ActionID;  // The action to execute.
   int       Key;       // Internal
   ERR       Error;     // The error code resulting from the action's execution.
   FUNCTION  Callback;  // Callback function to execute on action completion.
};

//********************************************************************************************************************

extern std::mutex glmPrint;               // For message logging only.

extern std::timed_mutex glmGeneric;       // A misc. internal mutex, strictly not recursive.
extern std::timed_mutex glmObjectLocking; // For LockObject() and ReleaseObject()
extern std::timed_mutex glmVolumes;       // For glVolumes
extern std::timed_mutex glmClassDB;       // For glClassDB
extern std::timed_mutex glmFieldKeys;     // For glFields

extern std::recursive_timed_mutex glmTimer;        // For timer subscriptions.
extern std::recursive_timed_mutex glmObjectLookup; // For glObjectLookup

extern std::recursive_mutex glmMemory;
extern std::recursive_mutex glmMsgHandler;

extern std::condition_variable_any cvResources;
extern std::condition_variable_any cvObjects;

//********************************************************************************************************************

struct CaseInsensitiveMap {
   bool operator() (const std::string &lhs, const std::string &rhs) const {
      return ::strcasecmp(lhs.c_str(), rhs.c_str()) < 0;
   }
};

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
   uint32_t Size;       // 4GB max
   THREADID ThreadLockID = THREADID(0);
   MEM      Flags;
   int16_t  AccessCount = 0; // Total number of locks

   PrivateAddress(APTR aAddress, MEMORYID aMemoryID, OBJECTID aOwnerID, uint32_t aSize, MEM aFlags) :
      Address(aAddress), MemoryID(aMemoryID), OwnerID(aOwnerID), Size(aSize), Flags(aFlags) { };

   void clear() {
      Address  = 0;
      MemoryID = 0;
      OwnerID  = 0;
      Flags    = MEM::NIL;
      ThreadLockID = THREADID(0);
   }
};

//********************************************************************************************************************

struct ActionSubscription {
   OBJECTPTR Subscriber; // The object that initiated the subscription
   OBJECTID SubscriberID; // Required for sanity checks on subscriber still existing.
   void (*Callback)(OBJECTPTR, ACTIONID, ERR, APTR, APTR);
   APTR Meta;

   ActionSubscription() : Subscriber(nullptr), SubscriberID(0), Callback(nullptr), Meta(nullptr) { }

   ActionSubscription(OBJECTPTR pContext, void (*pCallback)(OBJECTPTR, ACTIONID, ERR, APTR, APTR), APTR pMeta) :
      Subscriber(pContext), SubscriberID(pContext->UID), Callback(pCallback), Meta(pMeta) { }

   ActionSubscription(OBJECTPTR pContext, APTR pCallback, APTR pMeta) :
      Subscriber(pContext), SubscriberID(pContext->UID), Callback((void (*)(OBJECTPTR, ACTIONID, ERR, APTR, APTR))pCallback), Meta(pMeta) { }
};

struct virtual_drive {
   uint32_t VirtualID;   // Hash name of the volume, not including the trailing colon
   int  DriverSize;  // The driver may reserve a private area for its own structure attached to DirInfo.
   std::string Name;  // Volume name, including the trailing colon at the end
   bool CaseSensitive;
   ERR (*ScanDir)(DirInfo *);
   ERR (*Rename)(std::string_view, std::string_view);
   ERR (*Delete)(std::string_view, FUNCTION *);
   ERR (*OpenDir)(DirInfo *);
   ERR (*CloseDir)(DirInfo *);
   ERR (*Obsolete)(std::string_view, DirInfo **, LONG);
   ERR (*TestPath)(std::string &, RSF, LOC *);
   ERR (*WatchPath)(class extFile *);
   void  (*IgnoreFile)(class extFile *);
   ERR (*GetInfo)(std::string_view, FileInfo *, LONG);
   ERR (*GetDeviceInfo)(std::string_view, objStorageDevice *);
   ERR (*IdentifyFile)(std::string_view, CLASSID *, CLASSID *);
   ERR (*CreateFolder)(std::string_view, PERMIT);
   ERR (*SameFile)(std::string_view, std::string_view);
   ERR (*ReadLink)(std::string_view, STRING *);
   ERR (*CreateLink)(std::string_view, std::string_view);
   inline bool is_default() const { return VirtualID IS 0; }
   inline bool is_virtual() const { return VirtualID != 0; }
};

extern const virtual_drive glFSDefault;
extern std::unordered_map<uint32_t, virtual_drive> glVirtual;

//********************************************************************************************************************
// Resource definitions.

#define MEMHEADER 12    // 8 bytes at start for MEMH and MemoryID, 4 at end for MEMT

// Turning off USE_SHM means that the shared memory pool is available to all processes by default.

#ifdef __ANDROID__
  #undef USE_SHM // Should be using ashmem
  #define SHMKEY 0x0009f830 // Keep the key value low as we will be incrementing it

  #ifdef USE_SHM
  #else
    extern int glMemoryFD;
  #endif
#elif __unix__
  #define USE_SHM TRUE
  #define SHMKEY 0x0009f830 // Keep the key value low as we will be incrementing it

  #ifdef USE_SHM
    #define MEMORYFILE           "/tmp/parasol.mem"
  #else
    // To mount a 32MB RAMFS filesystem for this method:
    //
    //    mkdir -p /RAM1
    //    mount -t ramfs none /tmp/ramfs -o maxsize=32000

    #define MEMORYFILE           "/tmp/ramfs/parasol.mem"

    extern int glMemoryFD;
  #endif
#endif

enum {
   RT_OBJECT
};

//********************************************************************************************************************
// This structure is used for internally timed broadcasting.

class CoreTimer {
public:
   int64_t     NextCall;       // Cycle when PreciseTime() reaches this value (us)
   int64_t     LastCall;       // PreciseTime() recorded at the last call (us)
   int64_t     Interval;       // The amount of microseconds to wait at each interval
   OBJECTPTR Subscriber;     // The object that is subscribed (pointer, if private)
   OBJECTID  SubscriberID;   // The object that is subscribed
   FUNCTION  Routine;        // Routine to call if not using AC::Timer - ERR Routine(OBJECTID, LONG, LONG);
   uint8_t     Cycle;
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
   CP_BROADCAST,
   CP_REMOVE_TASK,
   CP_REMOVE_TABLES,
   CP_FREE_ACTION_MANAGEMENT,
   CP_FREE_COREBASE,
   CP_FREE_PRIVATE_MEMORY,
   CP_FINISHED
};

class extMetaClass : public objMetaClass {
   public:
   using create = pf::Create<extMetaClass>;
   class extMetaClass *Base;            // Reference to the base class if this is a sub-class
   std::vector<Field> FieldLookup;      // Field dictionary for base-class fields
   std::vector<MethodEntry> Methods;    // Original method array supplied by the module.
   std::vector<extMetaClass *> SubClasses;
   const struct FieldArray *SubFields;  // Extra fields defined by the sub-class
   class RootModule *Root;              // Root module that owns this class, if any.
   uint8_t Local[8];                      // Local object references (by field indexes), in order
   STRING Location;                     // Location of the class binary, this field exists purely for caching the location string if the client reads it
   ActionEntry ActionTable[LONG(AC::END)];
   int16_t OriginalFieldTotal;
   uint16_t BaseCeiling;                   // FieldLookup ceiling value for the base-class fields
};

class extFile : public objFile {
   public:
   using create = pf::Create<extFile>;
   struct DateTime prvModified;
   struct DateTime prvCreated;
   std::string Path;
   std::string prvIcon;
   std::string prvLine;
   std::string prvResolvedPath;
   #ifdef __unix__
      std::string prvLink;
   #endif
   int64_t Size;
   int64_t ProgressTime;
   #ifdef _WIN32
      int  Stream;
   #else
      APTR  Stream;
   #endif
   struct rkWatchPath *prvWatch;
   OBJECTPTR ProgressDialog;
   struct DirInfo *prvList;
   PERMIT Permissions;
   bool   isFolder;
   int   Handle;         // Native system file handle
};

class extConfig : public objConfig {
   public:
   using create = pf::Create<extConfig>;
   uint32_t CRC;   // CRC32, for determining if config data has been altered
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

   std::jthread::native_handle_type Handle;
   std::jthread::id ThreadID;
   std::jthread *CPPThread;
   FUNCTION Routine;
   FUNCTION Callback;
   std::atomic_bool Active;
};

class extTask : public objTask {
   public:
   using create = pf::Create<extTask>;
   std::map<std::string, std::string, CaseInsensitiveMap> Fields; // Variable field storage
   pf::vector<std::string> Parameters; // Arguments (string array)
   MEMORYID MessageMID;
   STRING   LaunchPath;
   STRING   Path;
   STRING   ProcessPath;
   STRING   Location;         // Where to load the task from (string)
   char     Name[32];         // Name of the task, if specified (string)
   bool     ReturnCodeSet;    // TRUE if the ReturnCode has been set
   FUNCTION ErrorCallback;
   FUNCTION OutputCallback;
   FUNCTION ExitCallback;
   FUNCTION InputCallback;
   struct MsgHandler *MsgAction;
   struct MsgHandler *MsgFree;
   struct MsgHandler *MsgDebug;
   struct MsgHandler *MsgWaitForObjects;
   struct MsgHandler *MsgQuit;
   struct MsgHandler *MsgEvent;
   struct MsgHandler *MsgThreadCallback;
   struct MsgHandler *MsgThreadAction;

   #ifdef __unix__
      int InFD;             // stdin FD for receiving output from launched task
      int ErrFD;            // stderr FD for receiving output from launched task
   #endif
   #ifdef _WIN32
      STRING Env;
      APTR Platform;
   #endif
   struct ActionEntry Actions[LONG(AC::END)]; // Action routines to be intercepted by the program
};

//********************************************************************************************************************

struct TaskRecord {
   int64_t  CreationTime;  // Time at which the task slot was created
   int      ProcessID;     // Core process ID
   OBJECTID TaskID;        // Representative task object.
   int      ReturnCode;    // Return code
   bool     Returned;      // Process has finished (the ReturnCode is set)
   #ifdef _WIN32
      WINHANDLE Lock;      // The semaphore to signal when a message is sent to the task
   #endif

   TaskRecord(extTask *Task) {
      ProcessID    = Task->ProcessID;
      CreationTime = PreciseTime() / 1000LL;
      TaskID       = Task->UID;
      ReturnCode   = 0;
      Returned     = false;
   }
};

//********************************************************************************************************************

class extModule : public objModule {
   public:
   using create = pf::Create<extModule>;
   std::string Name;     // Name of the module
   APTR   prvMBMemory;   // Module base memory
};

//********************************************************************************************************************
// Class database.

struct ClassRecord {
   CLASSID ClassID;
   CLASSID ParentID;
   CCF Category;
   std::string Name;
   std::string Path;
   std::string Match;
   std::string Header;
   std::string Icon;

   static const int MIN_SIZE = sizeof(CLASSID) + sizeof(CLASSID) + sizeof(LONG) + (sizeof(LONG) * 4);

   ClassRecord() { }

   inline ClassRecord(extMetaClass *pClass, std::optional<std::string> pPath = std::nullopt) {
      ClassID  = pClass->ClassID;
      ParentID = (pClass->BaseClassID IS pClass->ClassID) ? CLASSID::NIL : pClass->BaseClassID;
      Category = pClass->Category;

      Name.assign(pClass->ClassName);

      if (pPath.has_value()) Path.assign(pPath.value());
      else if (pClass->Path) Path.assign(pClass->Path);

      if (pClass->FileExtension) Match.assign(pClass->FileExtension);
      if (pClass->FileHeader) Header.assign(pClass->FileHeader);
      if (pClass->Icon) Icon.assign(pClass->Icon);
   }

   inline ClassRecord(CLASSID pClassID, std::string pName, CSTRING pMatch = nullptr, CSTRING pHeader = nullptr, CSTRING pIcon = nullptr) {
      ClassID  = pClassID;
      ParentID = CLASSID::NIL;
      Category = CCF::SYSTEM;
      Name     = pName;
      Path     = "modules:core";
      if (pMatch)  Match  = pMatch;
      if (pHeader) Header = pHeader;
      if (pIcon)   Icon   = pIcon;
   }

   inline ERR write(objFile *File) {
      if (File->write(&ClassID, sizeof(ClassID), nullptr) != ERR::Okay) return ERR::Write;
      if (File->write(&ParentID, sizeof(ParentID), nullptr) != ERR::Okay) return ERR::Write;
      if (File->write(&Category, sizeof(Category), nullptr) != ERR::Okay) return ERR::Write;

      auto size = LONG(Name.size());
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

      size = Icon.size();
      File->write(&size, sizeof(size));
      if (size) File->write(Icon.c_str(), size);

      return ERR::Okay;
   }

   inline ERR read(objFile *File) {
      if (File->read(&ClassID, sizeof(ClassID)) != ERR::Okay) return ERR::Read;
      if (File->read(&ParentID, sizeof(ParentID)) != ERR::Okay) return ERR::Read;
      if (File->read(&Category, sizeof(Category)) != ERR::Okay) return ERR::Read;

      char buffer[256];
      int size = 0;
      File->read(&size, sizeof(size));
      if (size < std::ssize(buffer)) {
         if (size > 0) {
            File->read(buffer, size);
            Name.assign(buffer, size);
         }
      }
      else return ERR::BufferOverflow;

      File->read(&size, sizeof(size));
      if (size < std::ssize(buffer)) {
         if (size > 0) {
            File->read(buffer, size);
            Path.assign(buffer, size);
         }
      }
      else return ERR::BufferOverflow;

      File->read(&size, sizeof(size));
      if (size < std::ssize(buffer)) {
         if (size > 0) {
            File->read(buffer, size);
            Match.assign(buffer, size);
         }
      }
      else return ERR::BufferOverflow;

      File->read(&size, sizeof(size));
      if (size < std::ssize(buffer)) {
         if (size > 0) {
            File->read(buffer, size);
            Header.assign(buffer, size);
         }
      }
      else return ERR::BufferOverflow;

      File->read(&size, sizeof(size));
      if (size < std::ssize(buffer)) {
         if (size > 0) {
            File->read(buffer, size);
            Icon.assign(buffer, size);
         }
      }
      else return ERR::BufferOverflow;

      return ERR::Okay;
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
   int Total;          // Total number of registered modules
};

struct ModuleItem {
   uint32_t Hash;       // Hash of the module file name
   int  Size;       // Size of the item structure, all accompanying strings and byte alignment
   // Followed by path
};

//********************************************************************************************************************
// Memory messaging structure.

struct MemoryMessageDetail {
   int8_t buffer[4];
};

struct MemoryMessage {
   #ifdef __unix__
      long MType;                   // <-- This long field is a Linux requirement
      struct MemoryMessageDetail Detail;
   #else
      int MemoryID;
   #endif
};

//********************************************************************************************************************
// Global data variables.

extern extMetaClass glMetaClass;
extern int glEUID, glEGID, glUID, glGID;
extern std::string glSystemPath;
extern std::string glModulePath;
extern std::string glRootPath;
extern std::string glDisplayDriver;
extern bool glShowIO, glShowPrivate, glEnableCrashHandler;
extern bool glJanitorActive;
extern bool glLogThreads;
extern int16_t glLogLevel, glMaxDepth;
extern TSTATE glTaskState;
extern int64_t glTimeLog;
extern RootModule *glModuleList;    // Locked with glmGeneric.  Maintained as a linked-list; hashmap unsuitable.
extern OpenInfo *glOpenInfo;      // Read-only.  The OpenInfo structure initially passed to OpenCore()
extern extTask *glCurrentTask;
extern "C" const ActionTable ActionTable[];
extern const Function    glFunctions[];
extern std::list<CoreTimer> glTimers;           // Locked with glmTimer
extern std::map<std::string, std::vector<Object *>, CaseInsensitiveMap> glObjectLookup;  // Locked with glmObjectlookup
extern std::unordered_map<std::string, struct ModHeader *> glStaticModules;
extern std::unordered_map<MEMORYID, PrivateAddress> glPrivateMemory;  // Locked with glmMemory: Note that best performance for looking up ID's is achieved as a sorted array.
extern std::unordered_map<OBJECTID, std::set<MEMORYID, std::greater<MEMORYID>>> glObjectMemory; // Locked with glmMemory.  Sorted with the most recent private memory first
extern std::unordered_map<OBJECTID, std::set<OBJECTID, std::greater<OBJECTID>>> glObjectChildren; // Locked with glmMemory.  Sorted with most recent object first
extern std::unordered_map<CLASSID, ClassRecord> glClassDB; // Class DB populated either by static_modules.cpp or by pre-generated file if modular.
extern std::unordered_map<CLASSID, extMetaClass *> glClassMap;
extern std::unordered_map<uint32_t, std::string> glFields; // Reverse lookup for converting field hashes back to their respective names.
extern std::unordered_map<OBJECTID, ObjectSignal> glWFOList;
extern std::map<std::string, ConfigKeys, CaseInsensitiveMap> glVolumes; // VolumeName = { Key, Value }
extern std::unordered_multimap<uint32_t, CLASSID> glWildClassMap; // Fast lookup for identifying classes by file extension
extern int glWildClassMapTotal;
extern std::vector<TaskRecord> glTasks;
extern const CSTRING glMessages[LONG(ERR::END)+1];       // Read-only table of error messages.
extern const int glTotalMessages;
extern "C" int glProcessID;   // Read only
extern HOSTHANDLE glConsoleFD;
extern int glStdErrFlags; // Read only
extern int glValidateProcessID; // Not a threading concern
extern std::atomic_int glMessageIDCount;
extern std::atomic_int glGlobalIDCount;
extern std::atomic_int glPrivateIDCounter;
extern int16_t glCrashStatus, glCodeIndex, glLastCodeIndex, glSystemState;
extern std::atomic_ushort glFunctionID;
extern "C" int8_t glProgramStage;
extern bool glPrivileged, glSync;
extern TIMER glCacheTimer;
extern APTR glJNIEnv;
extern class extObjectContext glTopContext; // Read-only, not a threading concern.
extern objTime *glTime;
extern objFile *glClassFile;
extern Object glDummyObject;
extern TIMER glProcessJanitor;
extern int glEventMask;
extern struct ModHeader glCoreHeader;

#ifndef PARASOL_STATIC
extern CSTRING glClassBinPath;
#endif

extern objMetaClass *glRootModuleClass;
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
extern int8_t fs_initialised;
extern APTR glPageFault;
extern bool glScanClasses;
extern uint8_t glTimerCycle;
extern bool glDebugMemory;
extern struct CoreBase *LocalCoreBase;
extern std::atomic_int glUniqueMsgID;

//********************************************************************************************************************
// Thread specific variables - these do not require locks.

extern THREADVAR class extObjectContext *tlContext;
extern THREADVAR class TaskMessage *tlCurrentMsg;
extern THREADVAR bool tlMainThread;
extern THREADVAR int16_t tlMsgRecursion;
extern THREADVAR int16_t tlDepth;
extern THREADVAR int16_t tlLogStatus;
extern THREADVAR int16_t tlPreventSleep;
extern THREADVAR int16_t tlPublicLockCount;
extern THREADVAR int16_t tlPrivateLockCount;
extern THREADVAR int glForceUID, glForceGID;
extern THREADVAR PERMIT glDefaultPermissions;

//********************************************************************************************************************

extern ERR (*glMessageHandler)(struct Message *);
extern void (*glVideoRecovery)(void);
extern void (*glKeyboardRecovery)(void);
extern void (*glNetProcessMessages)(LONG, APTR);

#ifdef _WIN32
extern "C" WINHANDLE glProcessHandle;
extern THREADVAR bool tlMessageBreak;
extern WINHANDLE glTaskLock;
#endif

#ifdef __unix__
extern THREADVAR int glSocket;
extern struct FileMonitor *glFileMonitor;
#endif

//********************************************************************************************************************
// Message handler chain structure.

extern struct MsgHandler *glMsgHandlers, *glLastMsgHandler;

class TaskMessage {
   public:
   // struct Message - START
   int64_t Time;
   int     UID;
   MSGID   Type;
   int     Size;
   // struct Message - END
   private:
   char *ExtBuffer;
   std::array<char, 64> Buffer;

   // Constructors

   public:
   TaskMessage() : Size(0), ExtBuffer(nullptr) { }

   TaskMessage(MSGID pType, APTR pData = nullptr, int pSize = 0) {
      Time = PreciseTime();
      UID  = ++glUniqueMsgID;
      Type = pType;
      Size = 0;
      ExtBuffer = nullptr;
      if ((pData) and (pSize)) setBuffer(pData, pSize);
   }

   ~TaskMessage() {
      if (ExtBuffer) { delete[] ExtBuffer; ExtBuffer = nullptr; }
   }

   // Move constructor
   TaskMessage(TaskMessage &&other) noexcept {
      ExtBuffer = nullptr;
      copy_from(other);
      other.Size = 0;
      other.ExtBuffer = nullptr; // Source loses its buffer
   }

   // Copy constructor
   TaskMessage(const TaskMessage &other) {
      ExtBuffer = nullptr;
      copy_from(other);
   }

   // Move assignment
   TaskMessage& operator=(TaskMessage &&other) noexcept {
      if (this == &other) return *this;
      copy_from(other);
      other.Size = 0;
      other.ExtBuffer = nullptr; // Source loses its buffer
      return *this;
   }

   // Copy assignment
   TaskMessage& operator=(const TaskMessage& other) {
      if (this == &other) return *this;
      copy_from(other);
      return *this;
   }

   // Public methods

   char * getBuffer() { return ExtBuffer ? ExtBuffer : Buffer.data(); }

   void setBuffer(APTR pData, size_t pSize) {
      if (ExtBuffer) { delete[] ExtBuffer; ExtBuffer = nullptr; }

      if (pSize <= Buffer.size()) copymem(pData, Buffer.data(), pSize);
      else {
         ExtBuffer = new (std::nothrow) char[pSize];
         if (ExtBuffer) copymem(pData, ExtBuffer, pSize);
      }

      Size = pSize;
   }

   private:
   inline void copy_from(const TaskMessage &Source, bool Constructor = false) {
      Time = Source.Time;
      UID  = Source.UID;
      Type = Source.Type;
      Size = Source.Size;
      if (Source.ExtBuffer) setBuffer(Source.ExtBuffer, Size);
      else if (Size) copymem(Source.Buffer.data(), Buffer.data(), Size);
   }
};

//********************************************************************************************************************
// ObjectContext is used to represent the object that has the current context in terms of the run-time call stack.
// It is primarily used for the resource tracking of newly allocated memory and objects, as well as for message logs
// and analysis of the call stack.

class extObjectContext : public ObjectContext {
   public:
   extObjectContext() { // Dummy initialisation
      stack  = nullptr;
      obj    = &glDummyObject;
      field  = nullptr;
      action = AC::NIL;
   }

   extObjectContext(OBJECTPTR pObject, AC pAction, struct Field *pField = nullptr) {
      stack  = tlContext;
      obj    = pObject;
      field  = pField;
      action = pAction;
      #pragma GCC diagnostic push
      #pragma GCC diagnostic ignored "-Wdangling-pointer"
      tlContext = this;
      #pragma GCC diagnostic pop
   }

   ~extObjectContext() {
      if (stack) tlContext = stack;
   }

   // Return the nearest object for resourcing purposes.  Note that an action ID of 0 has special meaning and indicates
   // that resources should be tracked to the next object on the stack (this feature is used by GetField*() functionality).

   inline OBJECTPTR resource() const {
      if (action != AC::NIL) return obj;
      else {
         for (auto ctx = stack; ctx; ctx=ctx->stack) {
            if (action != AC::NIL) return ctx->obj;
         }
         return &glDummyObject;
      }
   }

   inline OBJECTPTR setContext(OBJECTPTR pObject) {
      auto old = obj;
      obj = pObject;
      return old;
   }

   constexpr inline OBJECTPTR object() const { // Return the object that has the context (but not necessarily for resourcing)
      return obj;
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
   RFD  Flags;                            // Set to RFD::READ, RFD::WRITE or RFD::EXCEPT.

   FDRecord(HOSTHANDLE pFD, void (*pRoutine)(HOSTHANDLE, APTR), APTR pData, RFD pFlags) :
      FD(pFD), Routine(pRoutine), Data(pData), Flags(pFlags) { }
};

extern std::list<FDRecord> glFDTable;
extern int glInotify;
extern int8_t glFDProtected;
extern std::vector<FDRecord> glRegisterFD;

#define LRT_Exclusive 1

//********************************************************************************************************************
// The RootModule class is used to represent the first instantation of a loaded module library.  It is managed
// internally.  Clients interface with modules via the Module class.

class RootModule : public Object {
   public:
   class RootModule *Next;     // Next module in list
   class RootModule *Prev;     // Previous module in list
   struct ModHeader *Header;   // Pointer to module header - for memory resident modules only.
   struct CoreBase *CoreBase;  // Module's personal Core reference
   #ifdef __unix__
      APTR LibraryBase;        // Module code
   #else
      MODHANDLE LibraryBase;
   #endif
   std::string Name;           // Name of the module (as declared by the header)
   struct ModHeader *Table;
   int16_t Version;
   int16_t OpenCount;           // Amount of programs with this module open
   float  ModVersion;          // Version of this module
   MHF    Flags;
   bool   NoUnload;
   bool   DLL;                 // TRUE if the module is a Windows DLL
   ERR    (*Init)(OBJECTPTR, struct CoreBase *);
   void   (*Close)(OBJECTPTR);
   ERR    (*Open)(OBJECTPTR);
   ERR    (*Expunge)(void);
   struct ActionEntry prvActions[LONG(AC::END)]; // Action routines to be intercepted by the program
   std::string LibraryName; // Name of the library loaded from disk
};

THREADID get_thread_id(void);

//********************************************************************************************************************

ERR fs_closedir(DirInfo *);
ERR fs_createlink(std::string_view, std::string_view);
ERR fs_delete(std::string_view, FUNCTION *);
ERR fs_getinfo(std::string_view, FileInfo *, LONG);
ERR fs_getdeviceinfo(std::string_view, objStorageDevice *);
void  fs_ignore_file(class extFile *);
ERR fs_makedir(std::string_view, PERMIT);
ERR fs_opendir(DirInfo *);
ERR fs_readlink(std::string_view, STRING *);
ERR fs_rename(std::string_view, std::string_view);
ERR fs_samefile(std::string_view, std::string_view);
ERR fs_scandir(DirInfo *);
ERR fs_testpath(std::string &, RSF, LOC *);
ERR fs_watch_path(class extFile *);

const virtual_drive * get_fs(std::string_view Path);
void  free_storage_class(void);

ERR    convert_zip_error(struct z_stream_s *, LONG);
ERR    check_cache(OBJECTPTR, int64_t, int64_t);
ERR    fs_copy(std::string_view, std::string_view, FUNCTION *, bool);
ERR    fs_copydir(std::string &, std::string &, FileFeedback *, FUNCTION *, int8_t);
PERMIT get_parent_permissions(std::string_view, int *, int *);
ERR    RenameVolume(CSTRING, CSTRING);
ERR    findfile(std::string &);
PERMIT convert_fs_permissions(LONG);
LONG   convert_permissions(PERMIT);
ERR    get_file_info(std::string_view, FileInfo *, LONG);
extern "C" ERR convert_errno(LONG Error, ERR Default);
void free_file_cache(void);

__export void Expunge(int16_t);

extern void add_archive(class extCompression *);
extern void remove_archive(class extCompression *);

void   print_diagnosis(LONG);
CSTRING action_name(OBJECTPTR Object, int ActionID);
#ifndef PARASOL_STATIC
APTR   build_jump_table(const Function *);
#endif
void   stop_async_actions(void);
ERR    copy_args(const FunctionField *, int, int8_t *, std::vector<int8_t> &);
ERR    create_archive_volume(void);
ERR    delete_tree(std::string &, FUNCTION *, FileFeedback *);
struct ClassItem * find_class(CLASSID);
ERR    find_private_object_entry(OBJECTID, int *);
void   free_events(void);
void   free_module_entry(RootModule *);
void   free_wakelocks(void);
void   init_metaclass(void);
ERR    init_sleep(THREADID, LONG, LONG);
Field * lookup_id(OBJECTPTR, uint32_t, OBJECTPTR *);
ERR    msg_event(APTR, LONG, LONG, APTR, LONG);
ERR    msg_threadcallback(APTR, LONG, LONG, APTR, LONG);
ERR    msg_threadaction(APTR, LONG, LONG, APTR, LONG);
ERR    msg_free(APTR, LONG, LONG, APTR, LONG);
void   optimise_write_field(Field &);
void   PrepareSleep(void);
ERR    process_janitor(OBJECTID, LONG, LONG);
void   remove_process_waitlocks(void);
CLASSID lookup_class_by_ext(CLASSID, std::string_view);

#ifndef PARASOL_STATIC
void   scan_classes(void);
#endif

ERR  writeval_default(OBJECTPTR, Field *, LONG, const void *, LONG);
ERR  check_paths(CSTRING, PERMIT);
void merge_groups(ConfigGroups &, ConfigGroups &);
extern "C" ERR validate_process(LONG);

#ifdef _WIN32
   extern "C" WINHANDLE get_threadlock(void);
   extern "C" ERR  open_public_waitlock(WINHANDLE *, CSTRING);
   extern "C" void free_threadlocks(void);
   extern "C" ERR  wake_waitlock(WINHANDLE, LONG);
   extern "C" ERR  alloc_public_waitlock(WINHANDLE *, const char *Name);
   extern "C" void free_public_waitlock(WINHANDLE);
   extern "C" ERR  send_thread_msg(WINHANDLE, int Type, APTR, LONG);
   extern "C" int sleep_waitlock(WINHANDLE, LONG);
#else
   struct sockaddr_un * get_socket_path(LONG, socklen_t *);
   ERR alloc_public_cond(CONDLOCK *, ALF);
   void  free_public_cond(CONDLOCK *);
   ERR public_cond_wait(THREADLOCK *, CONDLOCK *, LONG);
   ERR send_thread_msg(LONG, LONG, APTR, LONG);
#endif

#ifdef _WIN32
extern "C" void activate_console(int8_t);
extern "C" void free_threadlock(void);
extern "C" int winCheckProcessExists(LONG);
extern "C" int winCloseHandle(WINHANDLE);
extern "C" int winCreatePipe(WINHANDLE *Read, WINHANDLE *Write);
extern "C" int winCreateSharedMemory(STRING, LONG, LONG, WINHANDLE *, APTR *);
extern "C" WINHANDLE winCreateThread(APTR Function, APTR Arg, int StackSize, int *ID);
extern "C" int winGetCurrentThreadId(void);
extern "C" void winDeathBringer(LONG Value);
extern "C" int winDuplicateHandle(LONG, LONG, LONG, int *);
extern "C" void winEnterCriticalSection(APTR);
extern "C" STRING winFormatMessage(LONG, char *, LONG);
extern "C" int winFreeLibrary(WINHANDLE);
extern "C" void winFreeProcess(APTR);
extern "C" int winGetEnv(CSTRING, STRING, LONG);
extern "C" int winGetExeDirectory(LONG, STRING);
extern "C" int winGetCurrentDirectory(LONG, STRING);
extern "C" WINHANDLE winGetCurrentProcess(void);
extern "C" int winGetCurrentProcessId(void);
extern "C" int winGetExitCodeProcess(WINHANDLE, int *Code);
extern "C" long long winGetFileSize(STRING);
extern "C" size_t winGetProcessMemoryUsage(LONG ProcessID);
extern "C" APTR winGetProcAddress(WINHANDLE, CSTRING);
extern "C" WINHANDLE winGetStdInput(void);
extern "C" int64_t winGetTickCount(void);
extern "C" void winInitialise(int *, void *);
extern "C" void winInitializeCriticalSection(APTR Lock);
extern "C" int winIsDebuggerPresent(void);
extern "C" void winDeleteCriticalSection(APTR Lock);
extern "C" int winLaunchProcess(APTR, STRING, STRING, int8_t Group, int8_t Redirect, APTR *ProcessResult, int8_t, STRING, STRING, int *);
extern "C" void winLeaveCriticalSection(APTR);
extern "C" WINHANDLE winLoadLibrary(CSTRING);
extern "C" void winLowerPriority(void);
extern "C" int winSetProcessPriority(int Priority);
extern "C" void winProcessMessages(void);
extern "C" int winReadStd(APTR, LONG, APTR Buffer, int *Size);
extern "C" int winReadPipe(WINHANDLE FD, APTR Buffer, int *Size);
extern "C" void winResetStdOut(APTR, APTR Buffer, int *Size);
extern "C" void winResetStdErr(APTR, APTR Buffer, int *Size);
extern "C" int winWritePipe(WINHANDLE FD, CPTR Buffer, int *Size);
extern "C" void winSelect(WINHANDLE FD, char *Read, char *Write);
extern "C" void winSetEnv(CSTRING, CSTRING);
extern "C" void winSetUnhandledExceptionFilter(LONG (*Function)(LONG, APTR, LONG, int *));
extern "C" void winShutdown(void);
extern "C" void winSleep(LONG);
extern "C" int winTerminateApp(int dwPID, int dwTimeout);
extern "C" void winTerminateThread(WINHANDLE);
extern "C" int winTryEnterCriticalSection(APTR);
extern "C" int winUnmapViewOfFile(APTR);
extern "C" int winWaitForSingleObject(WINHANDLE, LONG);
extern "C" int winWaitForObjects(LONG Total, WINHANDLE *, int Time, int8_t WinMsgs);
extern "C" int winWaitThread(WINHANDLE, LONG);
extern "C" int winWriteStd(APTR, CPTR Buffer, int Size);
extern "C" int winDeleteFile(char *Path);
extern "C" int winCheckDirectoryExists(CSTRING);
extern "C" ERR winCreateDir(CSTRING);
extern "C" int winCurrentDirectory(STRING, LONG);
extern "C" int winFileInfo(CSTRING, long long *, struct DateTime *, int8_t *);
extern "C" void winFindClose(WINHANDLE);
extern "C" void winFindNextChangeNotification(WINHANDLE);
extern "C" void winGetAttrib(CSTRING, int *);
extern "C" int8_t winGetCommand(char *, char *, LONG);
extern "C" int winGetFreeDiskSpace(char, int64_t *, int64_t *);
extern "C" int winGetLogicalDrives(void);
extern "C" int winGetLogicalDriveStrings(STRING, LONG);
extern ERR winGetVolumeInformation(STRING Volume, std::string &Label, std::string &FileSystem, int &Type);
extern "C" int winGetFullPathName(const char *Path, int PathLength, char *Output, char **NamePart);
extern "C" int winGetUserFolder(STRING, LONG);
extern "C" int winGetUserName(STRING, LONG);
extern "C" int winGetWatchBufferSize(void);
extern "C" int winMoveFile(STRING, STRING);
extern "C" int winReadChanges(WINHANDLE, APTR, int NotifyFlags, char *, LONG, int *);
extern "C" int winReadKey(CSTRING, CSTRING, STRING, LONG);
extern "C" int winReadRootKey(CSTRING, STRING, STRING, LONG);
extern "C" int winReadStdInput(WINHANDLE FD, APTR Buffer, int BufferSize, int *Size);
extern "C" int winScan(APTR *, STRING, STRING, int64_t *, struct DateTime *, struct DateTime *, int8_t *, int8_t *, int8_t *, int8_t *);
extern "C" int winSetAttrib(CSTRING, LONG);
extern "C" int winSetEOF(CSTRING, int64_t);
extern "C" int winTestLocation(CSTRING, int8_t);
extern "C" ERR winWatchFile(LONG, CSTRING, APTR, WINHANDLE *, int *);
extern "C" void winFindCloseChangeNotification(WINHANDLE);
extern "C" APTR winFindDirectory(STRING, APTR *, STRING);
extern "C" APTR winFindFile(STRING, APTR *, STRING);
extern "C" int winSetFileTime(CSTRING, bool, int16_t Year, int16_t Month, int16_t Day, int16_t Hour, int16_t Minute, int16_t Second);
extern "C" int winResetDate(STRING);
extern "C" void winSetDllDirectory(CSTRING);
extern "C" void winEnumSpecialFolders(void (*callback)(CSTRING, CSTRING, CSTRING, CSTRING, int8_t));

#endif

//********************************************************************************************************************
// Internal function to set the manager for an allocated resource.

inline void set_memory_manager(APTR Address, ResourceManager *Manager)
{
   ResourceManager **address_mgr = (ResourceManager **)((char *)Address - sizeof(LONG) - sizeof(LONG) - sizeof(ResourceManager *));
   address_mgr[0] = Manager;
}

//********************************************************************************************************************

inline int64_t calc_timestamp(struct DateTime *Date) {
   return(Date->Second +
          ((int64_t)Date->Minute * 60LL) +
          ((int64_t)Date->Hour * 60LL * 60LL) +
          ((int64_t)Date->Day * 60LL * 60LL * 24LL) +
          ((int64_t)Date->Month * 60LL * 60LL * 24LL * 31LL) +
          ((int64_t)Date->Year * 60LL * 60LL * 24LL * 31LL * 12LL));
}

inline uint16_t reverse_word(uint16_t Value) {
    return (((Value & 0x00FF) << 8) | ((Value & 0xFF00) >> 8));
}

inline uint32_t reverse_long(uint32_t Value) {
    return (((Value & 0x000000FF) << 24) |
            ((Value & 0x0000FF00) <<  8) |
            ((Value & 0x00FF0000) >>  8) |
            ((Value & 0xFF000000) >> 24));
}

//********************************************************************************************************************
// NOTE: To be called with glmObjectLookup only.

inline void remove_object_hash(OBJECTPTR Object)
{
   std::erase(glObjectLookup[Object->Name], Object);
   if (glObjectLookup[Object->Name].empty()) glObjectLookup.erase(Object->Name);
}

#endif // DEFS_H
