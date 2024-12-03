
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
BYTE fs_initialised  = FALSE;
APTR glPageFault     = NULL;
bool glScanClasses   = false;
bool glJanitorActive = false;
bool glDebugMemory   = false;
bool glEnableCrashHandler = true;
struct CoreBase *LocalCoreBase = NULL;

// NB: During shutdown, elements in glPrivateMemory are not erased but will have their fields cleared.
std::unordered_map<MEMORYID, PrivateAddress> glPrivateMemory;

std::condition_variable_any cvObjects;
std::condition_variable_any cvResources;

std::list<CoreTimer> glTimers;
std::list<FDRecord> glFDTable;

std::map<std::string, ConfigKeys, CaseInsensitiveMap> glVolumes;
std::map<std::string, std::vector<Object *>, CaseInsensitiveMap> glObjectLookup;

std::mutex glmPrint;
std::mutex glmThreadPool;
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
std::unordered_map<ULONG, std::string> glFields;

std::unordered_map<ULONG, CLASSID> glWildClassMap;

std::vector<FDRecord> glRegisterFD;
std::vector<TaskMessage> glQueue;
std::vector<TaskRecord> glTasks;

class RootModule  *glModuleList  = NULL;
struct OpenInfo   *glOpenInfo    = NULL;
struct MsgHandler *glMsgHandlers = NULL, *glLastMsgHandler = 0;

objFile *glClassFile   = NULL;
extTask *glCurrentTask = NULL;

APTR glJNIEnv = 0;
std::atomic_ushort glFunctionID = 3333; // IDTYPE_FUNCTION
LONG glStdErrFlags = 0;
TIMER glCacheTimer = 0;
LONG glMemoryFD = -1;
LONG glValidateProcessID = 0;
LONG glProcessID = 0;
LONG glEUID = -1, glEGID = -1, glGID = -1, glUID = -1;
LONG glWildClassMapTotal = 0;
std::atomic_int glPrivateIDCounter = 500;
std::atomic_int glMessageIDCount = 10000;
std::atomic_int glGlobalIDCount = 1;
LONG glEventMask = 0;
TIMER glProcessJanitor = 0;
UBYTE glTimerCycle = 1;
BYTE glFDProtected = 0;
std::atomic_int glUniqueMsgID = 1;

#ifdef __unix__
  THREADVAR LONG glSocket = -1; // Implemented as thread-local because we don't want threads other than main to utilise the messaging system.
#elif _WIN32
  WINHANDLE glProcessHandle = 0;
  WINHANDLE glTaskLock = 0;
#endif

HOSTHANDLE glConsoleFD = (HOSTHANDLE)-1; // Managed by GetResource()

LARGE glTimeLog      = 0;
WORD glCrashStatus   = 0;
WORD glCodeIndex     = CP_FINISHED;
WORD glLastCodeIndex = 0;
WORD glSystemState   = -1; // Initialisation state is -1
#ifdef _DEBUG
   WORD glLogLevel = 8; // Thread global
#else
   WORD glLogLevel  = 0;
#endif
WORD glMaxDepth     = 20; // Thread global
bool glShowIO       = false;
bool glShowPrivate  = false;
bool glPrivileged   = false;
bool glSync         = false;
bool glLogThreads   = false;
BYTE glProgramStage = STAGE_STARTUP;
TSTATE glTaskState  = TSTATE::RUNNING;
LONG glInotify = -1;

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
   NULL,
   fs_testpath,
   fs_watch_path,
   fs_ignore_file,
   fs_getinfo,
   fs_getdeviceinfo,
   NULL,
   fs_makedir,
   fs_samefile,
   fs_readlink,
   fs_createlink
};

std::unordered_map<ULONG, virtual_drive> glVirtual;

#ifdef __unix__
struct FileMonitor *glFileMonitor = NULL;
#endif

THREADVAR char tlFieldName[10]; // $12345678\0
THREADVAR LONG glForceUID = -1, glForceGID = -1;
THREADVAR PERMIT glDefaultPermissions = PERMIT::NIL;
THREADVAR WORD tlDepth     = 0;
THREADVAR WORD tlLogStatus = 1;
THREADVAR bool tlMainThread = false; // Will be set to TRUE on open, any other threads will remain FALSE.
THREADVAR WORD tlPreventSleep = 0;
THREADVAR WORD tlPublicLockCount = 0; // This variable is controlled by GLOBAL_LOCK() and can be used to check if locks are being held prior to sleeping.
THREADVAR WORD tlPrivateLockCount = 0; // Count of private *memory* locks held per-thread

struct Object glDummyObject = {
   .Class = NULL, .ChildPrivate = NULL, .CreatorMeta = NULL, .Owner = NULL, .NotifyFlags = 0,
   .ThreadPending = 0, .Queue = 0, .SleepQueue = 0, .ActionDepth = 0,
   .UID = 0, .Flags = NF::NIL, .ThreadID = 0, .Name = "", .PermitTerminate = false
};
class ObjectContext glTopContext; // Top-level context is a dummy and can be thread-shared
THREADVAR ObjectContext *tlContext = &glTopContext;

objTime *glTime = NULL;

THREADVAR WORD tlMsgRecursion = 0;
THREADVAR TaskMessage *tlCurrentMsg = NULL;

ERR (*glMessageHandler)(struct Message *) = NULL;
void (*glVideoRecovery)(void) = NULL;
void (*glKeyboardRecovery)(void) = NULL;
void (*glNetProcessMessages)(LONG, APTR) = NULL;

#ifdef __ANDROID__
static struct AndroidBase *AndroidBase = NULL;
#endif

//********************************************************************************************************************

#include "data_functions.c"
#include "data_errors.c"
