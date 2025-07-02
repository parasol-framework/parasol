
#define PRV_CORE_DATA TRUE

#include "defs.h"
#include <parasol/main.h>
#include <parasol/modules/core.h>

#ifdef __unix__
// In Unix/Linux builds it is assumed that the install location is static.  Dynamic loading is enabled
// during the build by setting the ROOT_PATH definition to a blank string.
   #ifndef _ROOT_PATH
      #define _ROOT_PATH /usr/local/
   #endif
   #ifndef _SYSTEM_PATH
      #define _SYSTEM_PATH /usr/local/share/parasol/
   #endif
   #ifndef _MODULE_PATH
      #define _MODULE_PATH /usr/local/lib/parasol/
   #endif
#else
// In Windows, path information is read from the registry.  If there are no registry entries, the system
// path is the program's working folder.
   #ifndef _ROOT_PATH
      #define _ROOT_PATH ""
   #endif
   #ifndef _SYSTEM_PATH
      #define _SYSTEM_PATH ""
   #endif
   #ifndef _MODULE_PATH
      #define _MODULE_PATH ""
   #endif
#endif

std::string glRootPath   = "" _ROOT_PATH "";
std::string glSystemPath = "" _SYSTEM_PATH "";
std::string glModulePath = "" _MODULE_PATH ""; // NB: This path will be updated to its resolved-form during Core initialisation.

std::string glDisplayDriver;

#ifndef PARASOL_STATIC
CSTRING glClassBinPath = "system:config/classes.bin";
#endif
objMetaClass *glRootModuleClass  = 0;
objMetaClass *glModuleClass      = 0;
objMetaClass *glTaskClass        = 0;
objMetaClass *glThreadClass      = 0;
objMetaClass *glTimeClass        = 0;
objMetaClass *glConfigClass      = 0;
objMetaClass *glFileClass        = 0;
objMetaClass *glScriptClass      = 0;
objMetaClass *glArchiveClass     = 0;
objMetaClass *glStorageClass     = 0;
objMetaClass *glCompressionClass = 0;
objMetaClass *glCompressedStreamClass = 0;
#ifdef __ANDROID__
objMetaClass *glAssetClass = 0;
#endif
int8_t fs_initialised  = FALSE;
APTR glPageFault     = nullptr;
bool glScanClasses   = false;
bool glJanitorActive = false;
bool glDebugMemory   = false;
bool glEnableCrashHandler = true;
struct CoreBase *LocalCoreBase = nullptr;

// NB: During shutdown, elements in glPrivateMemory are not erased but will have their fields cleared.
std::unordered_map<MEMORYID, PrivateAddress> glPrivateMemory;

std::condition_variable_any cvObjects;
std::condition_variable_any cvResources;

std::list<CoreTimer> glTimers;
std::list<FDRecord> glFDTable;

std::map<std::string, ConfigKeys, CaseInsensitiveMap> glVolumes;
std::map<std::string, std::vector<Object *>, CaseInsensitiveMap> glObjectLookup;

std::mutex glmPrint;
std::recursive_mutex glmMemory;
std::recursive_mutex glmMsgHandler;
std::recursive_timed_mutex glmObjectLookup;
std::recursive_timed_mutex glmTimer;
std::timed_mutex glmClassDB;
std::timed_mutex glmFieldKeys;
std::timed_mutex glmGeneric;
std::timed_mutex glmObjectLocking;
std::timed_mutex glmVolumes;

std::unordered_map<std::string, struct ModHeader *> glStaticModules;
std::unordered_map<CLASSID, ClassRecord> glClassDB;
std::unordered_map<CLASSID, extMetaClass *> glClassMap;
std::unordered_map<OBJECTID, ObjectSignal> glWFOList;
std::unordered_map<OBJECTID, std::set<MEMORYID, std::greater<MEMORYID>>> glObjectMemory;
std::unordered_map<OBJECTID, std::set<OBJECTID, std::greater<OBJECTID>>> glObjectChildren;
std::unordered_map<uint32_t, std::string> glFields;

std::unordered_multimap<uint32_t, CLASSID> glWildClassMap;

std::vector<FDRecord> glRegisterFD;
std::vector<TaskRecord> glTasks;

class RootModule  *glModuleList  = nullptr;
struct OpenInfo   *glOpenInfo    = nullptr;
struct MsgHandler *glMsgHandlers = nullptr, *glLastMsgHandler = 0;

objFile *glClassFile   = nullptr;
extTask *glCurrentTask = nullptr;

APTR glJNIEnv = 0;
std::atomic_ushort glFunctionID = 3333; // IDTYPE_FUNCTION
int glStdErrFlags = 0;
TIMER glCacheTimer = 0;
int glMemoryFD = -1;
int glValidateProcessID = 0;
int glProcessID = 0;
int glEUID = -1, glEGID = -1, glGID = -1, glUID = -1;
int glWildClassMapTotal = 0;
std::atomic_int glPrivateIDCounter = 500;
std::atomic_int glMessageIDCount = 10000;
std::atomic_int glGlobalIDCount = 1;
int glEventMask = 0;
TIMER glProcessJanitor = 0;
uint8_t glTimerCycle = 1;
int8_t glFDProtected = 0;
std::atomic_int glUniqueMsgID = 1;

#ifdef __unix__
  THREADVAR int glSocket = -1; // Implemented as thread-local because we don't want threads other than main to utilise the messaging system.
#elif _WIN32
  WINHANDLE glProcessHandle = 0;
  WINHANDLE glTaskLock = 0;
#endif

HOSTHANDLE glConsoleFD = (HOSTHANDLE)-1; // Managed by GetResource()

int64_t glTimeLog    = 0;
int16_t glCrashStatus   = 0;
int16_t glCodeIndex     = CP_FINISHED;
int16_t glLastCodeIndex = 0;
int16_t glSystemState   = -1; // Initialisation state is -1
#ifdef _DEBUG
   int16_t glLogLevel = 8; // Thread global
#else
   int16_t glLogLevel  = 0;
#endif
int16_t glMaxDepth     = 20; // Thread global
bool glShowIO       = false;
bool glShowPrivate  = false;
bool glPrivileged   = false;
bool glSync         = false;
bool glLogThreads   = false;
int8_t glProgramStage = STAGE_STARTUP;
TSTATE glTaskState  = TSTATE::RUNNING;
int glInotify = -1;

const struct virtual_drive glFSDefault = {
   0, 0, ":",
#ifdef _WIN32
   false,     // Windows is not case sensitive by default
#else
   true,      // Unix file systems are usually case sensitive
#endif
   fs_scandir,
   fs_rename,
   fs_delete,
   fs_opendir,
   fs_closedir,
   nullptr,
   fs_testpath,
   fs_watch_path,
   fs_ignore_file,
   fs_getinfo,
   fs_getdeviceinfo,
   nullptr,
   fs_makedir,
   fs_samefile,
   fs_readlink,
   fs_createlink
};

std::unordered_map<uint32_t, virtual_drive> glVirtual;

#ifdef __unix__
struct FileMonitor *glFileMonitor = nullptr;
#endif

THREADVAR char tlFieldName[10]; // $12345678\0
THREADVAR int glForceUID = -1, glForceGID = -1;
THREADVAR PERMIT glDefaultPermissions = PERMIT::NIL;
THREADVAR int16_t tlDepth     = 0;
THREADVAR int16_t tlLogStatus = 1;
THREADVAR bool tlMainThread = false; // Will be set to TRUE on open, any other threads will remain FALSE.
THREADVAR int16_t tlPreventSleep = 0;
THREADVAR int16_t tlPublicLockCount = 0; // This variable is controlled by GLOBAL_LOCK() and can be used to check if locks are being held prior to sleeping.
THREADVAR int16_t tlPrivateLockCount = 0; // Count of private *memory* locks held per-thread

struct Object glDummyObject = {
   .Class = nullptr, .ChildPrivate = nullptr, .CreatorMeta = nullptr, .Owner = nullptr, .NotifyFlags = 0,
   .ThreadPending = 0, .Queue = 0, .SleepQueue = 0, .ActionDepth = 0,
   .UID = 0, .Flags = NF::NIL, .ThreadID = 0, .Name = ""
};
class ObjectContext glTopContext; // Top-level context is a dummy and can be thread-shared
THREADVAR ObjectContext *tlContext = &glTopContext;

objTime *glTime = nullptr;

THREADVAR int16_t tlMsgRecursion = 0;
THREADVAR TaskMessage *tlCurrentMsg = nullptr;

ERR (*glMessageHandler)(struct Message *) = nullptr;
void (*glVideoRecovery)(void) = nullptr;
void (*glKeyboardRecovery)(void) = nullptr;
void (*glNetProcessMessages)(int, APTR) = nullptr;

#ifdef __ANDROID__
static struct AndroidBase *AndroidBase = nullptr;
#endif

//********************************************************************************************************************

#include "data_functions.c"
#include "data_errors.c"
