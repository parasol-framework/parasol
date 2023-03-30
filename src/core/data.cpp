
#define PRV_CORE_DATA TRUE

#include "defs.h"
#include <parasol/main.h>
#include <parasol/modules/core.h>
#include "idl.h"

#ifdef __unix__
// In Unix/Linux builds it is assumed that the install location is static.  Dynamic loading is enabled
// during the build by setting the ROOT_PATH definition to a blank string.
   #ifndef ROOT_PATH
      #define ROOT_PATH /usr/local/
   #endif
   #ifndef SYSTEM_PATH
      #define SYSTEM_PATH /usr/local/share/parasol/
   #endif
   #ifndef MODULE_PATH
      #define MODULE_PATH /usr/local/lib/parasol/
   #endif
#else
// In Windows, path information is read from the registry.  If there are no registry entries, the system
// path is the program's working folder.
   #ifndef ROOT_PATH
      #define ROOT_PATH ""
   #endif
   #ifndef SYSTEM_PATH
      #define SYSTEM_PATH ""
   #endif
   #ifndef MODULE_PATH
      #define MODULE_PATH ""
   #endif
#endif

std::string glRootPath   = "" ROOT_PATH "";
std::string glSystemPath = "" SYSTEM_PATH "";
std::string glModulePath = "" MODULE_PATH ""; // NB: This path will be updated to its resolved-form during Core initialisation.

char glDisplayDriver[28] = "";

CSTRING glClassBinPath = "system:config/classes.bin";
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
BYTE fs_initialised = FALSE;
APTR glPageFault = NULL;
bool glScanClasses = false;
LONG glDebugMemory = FALSE;
struct CoreBase *LocalCoreBase = NULL;

// NB: During shutdown, elements in glPrivateMemory are not erased but will have their fields cleared.
std::unordered_map<MEMORYID, PrivateAddress> glPrivateMemory;
std::unordered_map<OBJECTID, std::set<OBJECTID, std::greater<OBJECTID>>> glObjectChildren;
std::unordered_map<OBJECTID, std::set<MEMORYID, std::greater<MEMORYID>>> glObjectMemory;
std::unordered_map<CLASSID, ClassRecord> glClassDB;
std::unordered_map<OBJECTID, ObjectSignal> glWFOList;
std::map<std::string, std::vector<BaseClass *>, CaseInsensitiveMap> glObjectLookup;
std::unordered_map<CLASSID, extMetaClass *> glClassMap;
std::unordered_map<ULONG, std::string> glFields;
std::map<std::string, ConfigKeys, CaseInsensitiveMap> glVolumes;
std::list<FDRecord> glFDTable;
std::list<CoreTimer> glTimers;
std::vector<TaskRecord> glTasks;

struct RootModule     *glModuleList    = NULL;
struct SharedControl  *glSharedControl = NULL;
struct OpenInfo       *glOpenInfo      = NULL;
struct MsgHandler     *glMsgHandlers   = NULL, *glLastMsgHandler = 0;

objFile *glClassFile   = NULL;
objTask *glCurrentTask = NULL;
objConfig *glDatatypes = NULL;

APTR glJNIEnv = 0;
UWORD glFunctionID = 3333; // IDTYPE_FUNCTION
LONG glStdErrFlags = 0;
TIMER glCacheTimer = 0;
LONG glMemoryFD = -1;
LONG glTaskMessageMID = 0;
LONG glValidateProcessID = 0;
LONG glProcessID = 0;
LONG glEUID = -1, glEGID = -1, glGID = -1, glUID = -1;
LONG glPrivateIDCounter = 500;
LONG glMessageIDCount = 10000;
LONG glGlobalIDCount = 1;
LONG glEventMask = 0;
TIMER glProcessJanitor = 0;
UBYTE glTimerCycle = 1;
CSTRING glIDL = MOD_IDL;

#ifdef __unix__
  THREADVAR LONG glSocket = -1; // Implemented as thread-local because we don't want threads other than main to utilise the messaging system.
  sem_t glObjectSemaphore;
#elif _WIN32
  WINHANDLE glProcessHandle = 0;
  struct public_lock glPublicLocks[PL_END] = {
     { "", 0, 0, 0, FALSE }, // 0
     { "rka", 0, 0, 0, FALSE }, // PL_WAITLOCKS
     { "rkc", 0, 0, 0, FALSE }, // PL_FORBID
     { "rke", 0, 0, 0, FALSE }, // PL_SEMAPHORES
     { "rkg", 0, 0, 0, TRUE }   // CN_SEMAPHORES
  };
#endif

HOSTHANDLE glConsoleFD = (HOSTHANDLE)-1; // Managed by GetResource()

LARGE glTimeLog      = 0;
WORD glCrashStatus   = 0;
WORD glCodeIndex     = CP_FINISHED;
WORD glLastCodeIndex = 0;
WORD glFunctionIndex = 0;
#ifdef DEBUG
   WORD glLogLevel = 8; // Thread global
#else
   WORD glLogLevel  = 0;
#endif
WORD glMaxDepth     = 20; // Thread global
bool glShowIO       = false;
bool glShowPrivate  = false;
bool glPrivileged   = false;
bool glSync         = false;
BYTE glProgramStage = STAGE_STARTUP;
UBYTE glTaskState   = TSTATE_RUNNING;
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
LONG glMutexLockSize = sizeof(THREADLOCK);
struct FileMonitor *glFileMonitor = NULL;
#endif

THREADVAR char tlFieldName[10]; // $12345678\0
THREADVAR LONG glForceUID = -1, glForceGID = -1, glDefaultPermissions = 0;
THREADVAR WORD tlDepth     = 0;
THREADVAR WORD tlLogStatus = 1;
THREADVAR BYTE tlMainThread = FALSE; // Will be set to TRUE on open, any other threads will remain FALSE.
THREADVAR WORD tlPreventSleep = 0;
THREADVAR WORD tlPublicLockCount = 0; // This variable is controlled by GLOBAL_LOCK() and can be used to check if locks are being held prior to sleeping.
THREADVAR WORD tlPrivateLockCount = 0; // Count of private *memory* locks held per-thread

struct BaseClass glDummyObject;
class ObjectContext glTopContext; // Top-level context is a dummy and can be thread-shared
THREADVAR struct ObjectContext *tlContext = &glTopContext;

OBJECTPTR glLocale = NULL;
objTime *glTime = NULL;

THREADVAR WORD tlMsgRecursion = 0;
THREADVAR struct Message *tlCurrentMsg   = 0;

std::array<LONG (*)(struct BaseClass *, APTR), AC_END> ManagedActions;
ERROR (*glMessageHandler)(struct Message *) = 0;
void (*glVideoRecovery)(void) = 0;
void (*glKeyboardRecovery)(void) = 0;
void (*glNetProcessMessages)(LONG, APTR) = 0;

// Imported string variables

#ifdef __ANDROID__
static struct AndroidBase *AndroidBase = 0;
#endif

//********************************************************************************************************************

#include "data_functions.c"
#include "data_errors.c"
