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

#define MAX_TASKS    50  // Maximum number of tasks allowed to run at once

#define MSG_MAXARGSIZE   512   // The maximum allowable size of data based arguments before they have to be allocated as memory blocks when messaging
#define SIZE_SYSTEM_PATH 100  // Max characters for the Parasol system path

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

#define BREAKPOINT { UBYTE *nz = 0; nz[0] = 0; }

#include <parasol/system/types.h>

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

enum class RES    : LONG;
enum class RP     : LONG;
enum class IDTYPE : LONG;
enum class TSTATE : BYTE;
enum class LOC    : LONG;
enum class STT    : LONG;
enum class NF     : ULONG;
enum class FOF    : ULONG;
enum class RFD    : ULONG;
enum class PMF    : ULONG;
enum class MSF    : ULONG;
enum class RDF    : ULONG;
enum class RSF    : ULONG;
enum class LDF    : ULONG;
enum class VOLUME : ULONG;
enum class STR    : ULONG;
enum class SCF    : ULONG;
enum class SBF    : ULONG;
enum class SMF    : ULONG;
enum class VLF    : ULONG;
enum class MFF    : ULONG;
enum class DEVICE : LARGE;
enum class PERMIT : ULONG;
enum class CCF    : ULONG;
enum class MEM    : ULONG;
enum class ALF    : UWORD;
enum class EVG    : LONG;

#define STAT_FOLDER 0x0001

struct rkWatchPath {
   LARGE      Custom;    // User's custom data pointer or value
   HOSTHANDLE Handle;    // The handle for the file being monitored, can be a special reference for virtual paths
   FUNCTION   Routine;   // Routine to call on event trigger
   MFF        Flags;     // Event mask (original flags supplied to Watch)
   ULONG      VirtualID; // If monitored path is virtual, this refers to an ID in the glVirtual table

#ifdef _WIN32
   LONG WinFlags;
#endif
};

#include <parasol/vector.hpp>
#include "prototypes.h"

#include <parasol/main.h>

using namespace pf;

struct ActionEntry {
   ERROR (*PerformAction)(OBJECTPTR, APTR);     // Internal
};

struct ThreadMessage {
   OBJECTID ThreadID;    // Internal
   FUNCTION Callback;
};

struct ThreadActionMessage {
   OBJECTPTR Object;    // Direct pointer to a target object.
   LONG      ActionID;  // The action to execute.
   LONG      Key;       // Internal
   ERROR     Error;     // The error code resulting from the action's execution.
   FUNCTION  Callback;  // Callback function to execute on action completion.
};

//********************************************************************************************************************

extern std::mutex glmPrint;               // For message logging only.
extern std::mutex glmThreadPool;

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
   ULONG    Size;       // 4GB max
   volatile LONG ThreadLockID = 0;
   MEM      Flags;
   WORD     AccessCount = 0; // Total number of locks

   PrivateAddress(APTR aAddress, MEMORYID aMemoryID, OBJECTID aOwnerID, ULONG aSize, MEM aFlags) :
      Address(aAddress), MemoryID(aMemoryID), OwnerID(aOwnerID), Size(aSize), Flags(aFlags) { };

   void clear() {
      Address  = 0;
      MemoryID = 0;
      OwnerID  = 0;
      Flags    = MEM::NIL;
      ThreadLockID = 0;
   }
};

//********************************************************************************************************************

struct ActionSubscription {
   OBJECTPTR Subscriber; // The object that initiated the subscription
   OBJECTID SubscriberID; // Required for sanity checks on subscriber still existing.
   void (*Callback)(OBJECTPTR, ACTIONID, ERROR, APTR, APTR);
   APTR Meta;

   ActionSubscription() : Subscriber(NULL), SubscriberID(0), Callback(NULL), Meta(NULL) { }

   ActionSubscription(OBJECTPTR pContext, void (*pCallback)(OBJECTPTR, ACTIONID, ERROR, APTR, APTR), APTR pMeta) :
      Subscriber(pContext), SubscriberID(pContext->UID), Callback(pCallback), Meta(pMeta) { }

   ActionSubscription(OBJECTPTR pContext, APTR pCallback, APTR pMeta) :
      Subscriber(pContext), SubscriberID(pContext->UID), Callback((void (*)(OBJECTPTR, ACTIONID, ERROR, APTR, APTR))pCallback), Meta(pMeta) { }
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
   ERROR (*TestPath)(CSTRING, RSF, LOC *);
   ERROR (*WatchPath)(class extFile *);
   void  (*IgnoreFile)(class extFile *);
   ERROR (*GetInfo)(CSTRING, FileInfo *, LONG);
   ERROR (*GetDeviceInfo)(CSTRING, objStorageDevice *);
   ERROR (*IdentifyFile)(STRING, CLASSID *, CLASSID *);
   ERROR (*CreateFolder)(CSTRING, PERMIT);
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

#define MEMHEADER 12    // 8 bytes at start for MEMH and MemoryID, 4 at end for MEMT

// Turning off USE_SHM means that the shared memory pool is available to all processes by default.

#ifdef __ANDROID__
  #undef USE_SHM // Should be using ashmem
  #define SHMKEY 0x0009f830 // Keep the key value low as we will be incrementing it

  #ifdef USE_SHM
  #else
    extern LONG glMemoryFD;
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

    extern LONG glMemoryFD;
  #endif
#endif

enum {
   RT_OBJECT
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
   const struct FieldArray *SubFields;  // Extra fields defined by the sub-class
   class RootModule *Root;             // Root module that owns this class, if any.
   UBYTE Integral[8];                   // Integral object references (by field indexes), in order
   STRING Location;                     // Location of the class binary, this field exists purely for caching the location string if the client reads it
   ActionEntry ActionTable[AC_END];
   WORD OriginalFieldTotal;
   UWORD BaseCeiling;                   // FieldLookup ceiling value for the base-class fields
};

class extFile : public objFile {
   public:
   using create = pf::Create<extFile>;
   struct DateTime prvModified;
   struct DateTime prvCreated;
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
   PERMIT Permissions;
   LONG   prvType;
   LONG   Handle;         // Native system file handle
   WORD   prvLineLen;
};

class extConfig : public objConfig {
   public:
   using create = pf::Create<extConfig>;
   ULONG CRC;   // CRC32, for determining if config data has been altered
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
   FUNCTION Routine;
   FUNCTION Callback;
   std::atomic_bool Active;
   bool Pooled;
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
      LONG InFD;             // stdin FD for receiving output from launched task
      LONG ErrFD;            // stderr FD for receiving output from launched task
   #endif
   #ifdef _WIN32
      STRING Env;
      APTR Platform;
   #endif
   struct ActionEntry Actions[AC_END]; // Action routines to be intercepted by the program
};

//********************************************************************************************************************

struct TaskRecord {
   LARGE    CreationTime;  // Time at which the task slot was created
   LONG     ProcessID;     // Core process ID
   OBJECTID TaskID;        // Representative task object.
   LONG     ReturnCode;    // Return code
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
   char   Name[60];      // Name of the module
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

   static const LONG MIN_SIZE = sizeof(CLASSID) + sizeof(CLASSID) + sizeof(LONG) + (sizeof(LONG) * 4);

   ClassRecord() { }

   inline ClassRecord(extMetaClass *pClass, std::optional<std::string> pPath = std::nullopt) {
      ClassID  = pClass->ClassID;
      ParentID = (pClass->BaseClassID IS pClass->ClassID) ? 0 : pClass->BaseClassID;
      Category = pClass->Category;

      Name.assign(pClass->ClassName);

      if (pPath.has_value()) Path.assign(pPath.value());
      else if (pClass->Path) Path.assign(pClass->Path);

      if (pClass->FileExtension) Match.assign(pClass->FileExtension);
      if (pClass->FileHeader) Header.assign(pClass->FileHeader);
   }

   inline ClassRecord(CLASSID pClassID, std::string pName, CSTRING pMatch = NULL, CSTRING pHeader = NULL) {
      ClassID  = pClassID;
      ParentID = 0;
      Category = CCF::SYSTEM;
      Name     = pName;
      Path     = "modules:core";
      if (pMatch) Match   = pMatch;
      if (pHeader) Header = pHeader;
   }

   inline ERROR write(objFile *File) {
      if (File->write(&ClassID, sizeof(ClassID), NULL)) return ERR_Write;
      if (File->write(&ParentID, sizeof(ParentID), NULL)) return ERR_Write;
      if (File->write(&Category, sizeof(Category), NULL)) return ERR_Write;

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
// Global data variables.

extern extMetaClass glMetaClass;
extern LONG glEUID, glEGID, glUID, glGID;
extern std::string glSystemPath;
extern std::string glModulePath;
extern std::string glRootPath;
extern char glDisplayDriver[28];
extern bool glShowIO, glShowPrivate, glEnableCrashHandler;
extern bool glJanitorActive;
extern bool glLogThreads;
extern WORD glLogLevel, glMaxDepth;
extern TSTATE glTaskState;
extern LARGE glTimeLog;
extern RootModule *glModuleList;    // Locked with glmGeneric.  Maintained as a linked-list; hashmap unsuitable.
extern OpenInfo *glOpenInfo;      // Read-only.  The OpenInfo structure initially passed to OpenCore()
extern extTask *glCurrentTask;
extern "C" const ActionTable ActionTable[];
extern const Function    glFunctions[];
extern std::list<CoreTimer> glTimers;           // Locked with glmTimer
extern std::map<std::string, std::vector<BaseClass *>, CaseInsensitiveMap> glObjectLookup;  // Locked with glmObjectlookup
extern std::unordered_map<std::string, struct ModHeader *> glStaticModules;
extern std::unordered_map<MEMORYID, PrivateAddress> glPrivateMemory;  // Locked with glmMemory: Note that best performance for looking up ID's is achieved as a sorted array.
extern std::unordered_map<OBJECTID, std::set<MEMORYID, std::greater<MEMORYID>>> glObjectMemory; // Locked with glmMemory.  Sorted with the most recent private memory first
extern std::unordered_map<OBJECTID, std::set<OBJECTID, std::greater<OBJECTID>>> glObjectChildren; // Locked with glmMemory.  Sorted with most recent object first
extern std::unordered_map<CLASSID, ClassRecord> glClassDB;
extern std::unordered_map<CLASSID, extMetaClass *> glClassMap;
extern std::unordered_map<ULONG, std::string> glFields; // Reverse lookup for converting field hashes back to their respective names.
extern std::unordered_map<OBJECTID, ObjectSignal> glWFOList;
extern std::map<std::string, ConfigKeys, CaseInsensitiveMap> glVolumes; // VolumeName = { Key, Value }
extern std::vector<TaskRecord> glTasks;
extern const CSTRING glMessages[ERR_END+1];       // Read-only table of error messages.
extern const LONG glTotalMessages;
extern "C" LONG glProcessID;   // Read only
extern HOSTHANDLE glConsoleFD;
extern LONG glStdErrFlags; // Read only
extern LONG glValidateProcessID; // Not a threading concern
extern std::atomic_int glMessageIDCount;
extern std::atomic_int glGlobalIDCount;
extern std::atomic_int glPrivateIDCounter;
extern WORD glCrashStatus, glCodeIndex, glLastCodeIndex, glSystemState;
extern std::atomic_ushort glFunctionID;
extern "C" BYTE glProgramStage;
extern bool glPrivileged, glSync;
extern TIMER glCacheTimer;
extern APTR glJNIEnv;
extern class ObjectContext glTopContext; // Read-only, not a threading concern.
extern OBJECTPTR modIconv;
extern OBJECTPTR glLocale;
extern objTime *glTime;
extern objConfig *glDatatypes;
extern objFile *glClassFile;
extern BaseClass glDummyObject;
extern TIMER glProcessJanitor;
extern LONG glEventMask;
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
extern BYTE fs_initialised;
extern APTR glPageFault;
extern bool glScanClasses;
extern UBYTE glTimerCycle;
extern bool glDebugMemory;
extern struct CoreBase *LocalCoreBase;
extern std::atomic_int glUniqueMsgID;

//********************************************************************************************************************
// Thread specific variables - these do not require locks.

extern THREADVAR class ObjectContext *tlContext;
extern THREADVAR class TaskMessage *tlCurrentMsg;
extern THREADVAR bool tlMainThread;
extern THREADVAR WORD tlMsgRecursion;
extern THREADVAR WORD tlDepth;
extern THREADVAR WORD tlLogStatus;
extern THREADVAR WORD tlPreventSleep;
extern THREADVAR WORD tlPublicLockCount;
extern THREADVAR WORD tlPrivateLockCount;
extern THREADVAR LONG glForceUID, glForceGID;
extern THREADVAR PERMIT glDefaultPermissions;

//********************************************************************************************************************

extern ERROR (*glMessageHandler)(struct Message *);
extern void (*glVideoRecovery)(void);
extern void (*glKeyboardRecovery)(void);
extern void (*glNetProcessMessages)(LONG, APTR);

#ifdef _WIN32
extern "C" WINHANDLE glProcessHandle;
extern THREADVAR bool tlMessageBreak;
extern WINHANDLE glTaskLock;
#endif

#ifdef __unix__
extern THREADVAR LONG glSocket;
extern struct FileMonitor *glFileMonitor;
#endif

//********************************************************************************************************************
// Message handler chain structure.

extern struct MsgHandler *glMsgHandlers, *glLastMsgHandler;

class TaskMessage {
   public:
   // struct Message - START
   LARGE Time;
   LONG  UID;
   LONG  Type;
   LONG  Size;
   // struct Message - END
   private:
   char *ExtBuffer;
   std::array<char, 64> Buffer;

   // Constructors

   public:
   TaskMessage() : Size(0), ExtBuffer(NULL) { }

   TaskMessage(LONG pType, APTR pData = NULL, LONG pSize = 0) {
      Time = PreciseTime();
      UID  = ++glUniqueMsgID;
      Type = pType;
      Size = 0;
      ExtBuffer = NULL;
      if ((pData) and (pSize)) setBuffer(pData, pSize);
   }

   ~TaskMessage() {
      if (ExtBuffer) { delete[] ExtBuffer; ExtBuffer = NULL; }
   }

   // Move constructor
   TaskMessage(TaskMessage &&other) noexcept {
      ExtBuffer = NULL;
      copy_from(other);
      other.Size = 0;
      other.ExtBuffer = NULL; // Source loses its buffer
   }

   // Copy constructor
   TaskMessage(const TaskMessage &other) {
      ExtBuffer = NULL;
      copy_from(other);
   }

   // Move assignment
   TaskMessage& operator=(TaskMessage &&other) noexcept {
      if (this == &other) return *this;
      copy_from(other);
      other.Size = 0;
      other.ExtBuffer = NULL; // Source loses its buffer
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
      if (ExtBuffer) { delete[] ExtBuffer; ExtBuffer = NULL; }

      if (pSize <= Buffer.size()) CopyMemory(pData, Buffer.data(), pSize);
      else {
         ExtBuffer = new (std::nothrow) char[pSize];
         if (ExtBuffer) CopyMemory(pData, ExtBuffer, pSize);
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
      else if (Size) CopyMemory(Source.Buffer.data(), Buffer.data(), Size);
   }
};

extern std::vector<TaskMessage> glQueue;

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
   RFD  Flags;                            // Set to RFD::READ, RFD::WRITE or RFD::EXCEPT.

   FDRecord(HOSTHANDLE pFD, void (*pRoutine)(HOSTHANDLE, APTR), APTR pData, RFD pFlags) :
      FD(pFD), Routine(pRoutine), Data(pData), Flags(pFlags) { }
};

extern std::list<FDRecord> glFDTable;
extern LONG glInotify;
extern BYTE glFDProtected;
extern std::vector<FDRecord> glRegisterFD;

#define LRT_Exclusive 1

//********************************************************************************************************************

class RootModule : public BaseClass {
   public:
   class RootModule *Next;     // Next module in list
   class RootModule *Prev;     // Previous module in list
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
   MHF    Flags;
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

ERROR SetFieldF(OBJECTPTR, FIELD, va_list);

ERROR fs_closedir(DirInfo *);
ERROR fs_createlink(CSTRING, CSTRING);
ERROR fs_delete(STRING, FUNCTION *);
ERROR fs_getinfo(CSTRING, FileInfo *, LONG);
ERROR fs_getdeviceinfo(CSTRING, objStorageDevice *);
void  fs_ignore_file(class extFile *);
ERROR fs_makedir(CSTRING, PERMIT);
ERROR fs_opendir(DirInfo *);
ERROR fs_readlink(STRING, STRING *);
ERROR fs_rename(STRING, STRING);
ERROR fs_samefile(CSTRING, CSTRING);
ERROR fs_scandir(DirInfo *);
ERROR fs_testpath(CSTRING, RSF, LOC *);
ERROR fs_watch_path(class extFile *);

const virtual_drive * get_fs(CSTRING Path);
void  free_storage_class(void);

ERROR  convert_zip_error(struct z_stream_s *, LONG);
ERROR  check_cache(OBJECTPTR, LARGE, LARGE);
ERROR  get_class_cmd(CSTRING, objConfig *, LONG, CLASSID, STRING *);
ERROR  fs_copy(CSTRING, CSTRING, FUNCTION *, BYTE);
ERROR  fs_copydir(STRING, STRING, FileFeedback *, FUNCTION *, BYTE);
PERMIT get_parent_permissions(CSTRING, LONG *, LONG *);
ERROR  load_datatypes(void);
ERROR  RenameVolume(CSTRING, CSTRING);
ERROR  findfile(STRING);
PERMIT convert_fs_permissions(LONG);
LONG   convert_permissions(PERMIT);
void   set_memory_manager(APTR, ResourceManager *);
BYTE   strip_folder(STRING) __attribute__ ((unused));
ERROR  get_file_info(CSTRING, FileInfo *, LONG);
extern "C" ERROR  convert_errno(LONG Error, ERROR Default);
void   free_file_cache(void);

__export void Expunge(WORD);

extern void add_archive(class extCompression *);
extern void remove_archive(class extCompression *);

void   print_diagnosis(LONG);
CSTRING action_name(OBJECTPTR Object, LONG ActionID);
#ifndef PARASOL_STATIC
APTR   build_jump_table(const Function *);
#endif
ERROR  copy_args(const FunctionField *, LONG, BYTE *, BYTE *, LONG, LONG *, CSTRING);
ERROR  copy_field_to_buffer(OBJECTPTR, Field *, LONG, APTR, CSTRING, LONG *);
ERROR  create_archive_volume(void);
ERROR  delete_tree(STRING, LONG, FUNCTION *, FileFeedback *);
struct ClassItem * find_class(CLASSID);
ERROR  find_private_object_entry(OBJECTID, LONG *);
void   free_events(void);
void   free_module_entry(RootModule *);
void   free_wakelocks(void);
LONG   get_thread_id(void);
void   init_metaclass(void);
ERROR  init_sleep(LONG, LONG, LONG);
void   local_free_args(APTR, const FunctionField *);
Field * lookup_id(OBJECTPTR, ULONG, OBJECTPTR *);
ERROR  msg_event(APTR, LONG, LONG, APTR, LONG);
ERROR  msg_threadcallback(APTR, LONG, LONG, APTR, LONG);
ERROR  msg_threadaction(APTR, LONG, LONG, APTR, LONG);
ERROR  msg_free(APTR, LONG, LONG, APTR, LONG);
void   optimise_write_field(Field &);
void   PrepareSleep(void);
ERROR  process_janitor(OBJECTID, LONG, LONG);
void   remove_process_waitlocks(void);
ERROR  resolve_args(APTR, const FunctionField *);

#ifndef PARASOL_STATIC
void   scan_classes(void);
#endif

void   remove_threadpool(void);
ERROR  threadpool_get(extThread **);
void   threadpool_release(extThread *);
ERROR  writeval_default(OBJECTPTR, Field *, LONG, const void *, LONG);
extern "C" ERROR validate_process(LONG);
void   free_iconv(void);
ERROR  check_paths(CSTRING, PERMIT);
void   merge_groups(ConfigGroups &, ConfigGroups &);

#ifdef _WIN32
   extern "C" ERROR open_public_waitlock(WINHANDLE *, CSTRING);
   extern "C" WINHANDLE get_threadlock(void);
   extern "C" void  free_threadlocks(void);
   extern "C" ERROR wake_waitlock(WINHANDLE, LONG);
   extern "C" ERROR alloc_public_waitlock(WINHANDLE *, const char *Name);
   extern "C" void  free_public_waitlock(WINHANDLE);
   extern "C" ERROR send_thread_msg(WINHANDLE, LONG Type, APTR, LONG);
   extern "C" LONG  sleep_waitlock(WINHANDLE, LONG);
#else
   struct sockaddr_un * get_socket_path(LONG, socklen_t *);
   ERROR alloc_public_cond(CONDLOCK *, ALF);
   void  free_public_cond(CONDLOCK *);
   ERROR public_cond_wait(THREADLOCK *, CONDLOCK *, LONG);
   ERROR send_thread_msg(LONG, LONG, APTR, LONG);
#endif

#ifdef _WIN32
extern "C" void activate_console(BYTE);
extern "C" void free_threadlock(void);
extern "C" LONG winCheckProcessExists(LONG);
extern "C" LONG winCloseHandle(WINHANDLE);
extern "C" LONG winCreatePipe(WINHANDLE *Read, WINHANDLE *Write);
extern "C" LONG winCreateSharedMemory(STRING, LONG, LONG, WINHANDLE *, APTR *);
extern "C" WINHANDLE winCreateThread(APTR Function, APTR Arg, LONG StackSize, LONG *ID);
extern "C" LONG winGetCurrentThreadId(void);
extern "C" void winDeathBringer(LONG Value);
extern "C" LONG winDuplicateHandle(LONG, LONG, LONG, LONG *);
extern "C" void winEnterCriticalSection(APTR);
extern "C" STRING winFormatMessage(LONG, char *, LONG);
extern "C" LONG winFreeLibrary(WINHANDLE);
extern "C" void winFreeProcess(APTR);
extern "C" LONG winGetEnv(CSTRING, STRING, LONG);
extern "C" LONG winGetExeDirectory(LONG, STRING);
extern "C" LONG winGetCurrentDirectory(LONG, STRING);
extern "C" WINHANDLE winGetCurrentProcess(void);
extern "C" LONG winGetCurrentProcessId(void);
extern "C" LONG winGetExitCodeProcess(WINHANDLE, LONG *Code);
extern "C" long long winGetFileSize(STRING);
extern "C" APTR winGetProcAddress(WINHANDLE, CSTRING);
extern "C" WINHANDLE winGetStdInput(void);
extern "C" LARGE winGetTickCount(void);
extern "C" void winInitialise(int *, void *);
extern "C" void winInitializeCriticalSection(APTR Lock);
extern "C" LONG winIsDebuggerPresent(void);
extern "C" void winDeleteCriticalSection(APTR Lock);
extern "C" LONG winLaunchProcess(APTR, STRING, STRING, BYTE Group, BYTE Redirect, APTR *ProcessResult, BYTE, STRING, STRING, LONG *);
extern "C" void winLeaveCriticalSection(APTR);
extern "C" WINHANDLE winLoadLibrary(CSTRING);
extern "C" void winLowerPriority(void);
extern "C" void winProcessMessages(void);
extern "C" LONG winReadStd(APTR, LONG, APTR Buffer, LONG *Size);
extern "C" LONG winReadPipe(WINHANDLE FD, APTR Buffer, LONG *Size);
extern "C" void winResetStdOut(APTR, APTR Buffer, LONG *Size);
extern "C" void winResetStdErr(APTR, APTR Buffer, LONG *Size);
extern "C" LONG winWritePipe(WINHANDLE FD, CPTR Buffer, LONG *Size);
extern "C" void winSelect(WINHANDLE FD, char *Read, char *Write);
extern "C" void winSetEnv(CSTRING, CSTRING);
extern "C" void winSetUnhandledExceptionFilter(LONG (*Function)(LONG, APTR, LONG, LONG *));
extern "C" void winShutdown(void);
extern "C" void winSleep(LONG);
extern "C" int winTerminateApp(int dwPID, int dwTimeout);
extern "C" void winTerminateThread(WINHANDLE);
extern "C" LONG winTryEnterCriticalSection(APTR);
extern "C" LONG winUnmapViewOfFile(APTR);
extern "C" LONG winWaitForSingleObject(WINHANDLE, LONG);
extern "C" LONG winWaitForObjects(LONG Total, WINHANDLE *, LONG Time, BYTE WinMsgs);
extern "C" LONG winWaitThread(WINHANDLE, LONG);
extern "C" LONG winWriteStd(APTR, CPTR Buffer, LONG Size);
extern "C" int winDeleteFile(char *Path);
extern "C" LONG winCheckDirectoryExists(CSTRING);
extern "C" LONG winCreateDir(CSTRING);
extern "C" LONG winCurrentDirectory(STRING, LONG);
extern "C" LONG winFileInfo(CSTRING, long long *, struct DateTime *, BYTE *);
extern "C" void winFindClose(WINHANDLE);
extern "C" void winFindNextChangeNotification(WINHANDLE);
extern "C" void winGetAttrib(CSTRING, LONG *);
extern "C" BYTE winGetCommand(char *, char *, LONG);
extern "C" LONG winGetFreeDiskSpace(UBYTE, LARGE *, LARGE *);
extern "C" LONG winGetLogicalDrives(void);
extern "C" LONG winGetLogicalDriveStrings(STRING, LONG);
extern "C" LONG winGetDriveType(STRING);
extern "C" LONG winGetFullPathName(const char *Path, LONG PathLength, char *Output, char **NamePart);
extern "C" LONG winGetUserFolder(STRING, LONG);
extern "C" LONG winGetUserName(STRING, LONG);
extern "C" LONG winGetWatchBufferSize(void);
extern "C" LONG winMoveFile(STRING, STRING);
extern "C" LONG winReadChanges(WINHANDLE, APTR, LONG NotifyFlags, char *, LONG, LONG *);
extern "C" LONG winReadKey(CSTRING, CSTRING, STRING, LONG);
extern "C" LONG winReadRootKey(CSTRING, STRING, STRING, LONG);
extern "C" LONG winReadStdInput(WINHANDLE FD, APTR Buffer, LONG BufferSize, LONG *Size);
extern "C" LONG winScan(APTR *, STRING, STRING, LARGE *, struct DateTime *, struct DateTime *, BYTE *, BYTE *, BYTE *, BYTE *);
extern "C" LONG winSetAttrib(CSTRING, LONG);
extern "C" LONG winSetEOF(CSTRING, LARGE);
extern "C" LONG winTestLocation(CSTRING, BYTE);
extern "C" LONG winWatchFile(LONG, CSTRING, APTR, WINHANDLE *, LONG *);
extern "C" void winFindCloseChangeNotification(WINHANDLE);
extern "C" APTR winFindDirectory(STRING, APTR *, STRING);
extern "C" APTR winFindFile(STRING, APTR *, STRING);
extern "C" LONG winSetFileTime(CSTRING, WORD Year, WORD Month, WORD Day, WORD Hour, WORD Minute, WORD Second);
extern "C" LONG winResetDate(STRING);
extern "C" void winSetDllDirectory(CSTRING);
extern "C" void winEnumSpecialFolders(void (*callback)(CSTRING, CSTRING, CSTRING, CSTRING, BYTE));

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
         error = Object->lock();
         obj = Object;
      }

      ~ScopedObjectAccess() { if (!error) obj->unlock(); }

      bool granted() { return error == ERR_Okay; }

      void release() {
         if (!error) {
            obj->unlock();
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
// NOTE: To be called with glmObjectLookup only.

inline void remove_object_hash(OBJECTPTR Object)
{
   std::erase(glObjectLookup[Object->Name], Object);
   if (glObjectLookup[Object->Name].empty()) glObjectLookup.erase(Object->Name);
}

#endif // DEFS_H
