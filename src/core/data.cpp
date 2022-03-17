
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

char glProgName[32] = "Program";

char glRootPath[SIZE_SYSTEM_PATH] = "" ROOT_PATH "";
char glSystemPath[SIZE_SYSTEM_PATH] = "" SYSTEM_PATH "";
char glModulePath[SIZE_SYSTEM_PATH] = "" MODULE_PATH "";

char glDisplayDriver[28] = "";

CSTRING glClassBinPath = "system:config/classes.bin";
CSTRING glModuleBinPath = "system:config/modules.bin";
struct rkMetaClass *ModuleMasterClass = 0;
struct rkMetaClass *ModuleClass = 0;
struct rkMetaClass *TaskClass = 0;
struct rkMetaClass *ThreadClass = 0;
struct rkMetaClass *TimeClass = 0;
struct rkMetaClass *ConfigClass = 0;
struct rkMetaClass *glFileClass = 0;
struct rkMetaClass *glScriptClass = 0;
struct rkMetaClass *glArchiveClass = 0;
struct rkMetaClass *glCompressionClass = 0;
struct rkMetaClass *glCompressedStreamClass = 0;
#ifdef __ANDROID__
struct rkMetaClass *glAssetClass = 0;
#endif
struct rkMetaClass *glStorageClass = NULL;
BYTE fs_initialised = FALSE;
APTR glPageFault = NULL;
BYTE glScanClasses = FALSE;
LONG glDebugMemory = FALSE;
struct CoreBase *LocalCoreBase = NULL;

// NB: During shutdown, elements in glPrivateMemory are not erased but will have their fields cleared.
std::unordered_map<MEMORYID, PrivateAddress> glPrivateMemory;
std::unordered_map<OBJECTID, std::set<OBJECTID, std::greater<OBJECTID>>> glObjectChildren;
std::unordered_map<OBJECTID, std::set<MEMORYID, std::greater<MEMORYID>>> glObjectMemory;

struct PublicAddress  *glSharedBlocks  = 0;
struct SortedAddress  *glSortedBlocks  = 0;
struct ModuleMaster   *glModuleList    = 0;
struct SharedAccess   *SharedAccess    = 0;
struct SharedControl  *glSharedControl = 0;
struct TaskList       *shTasks         = 0;
struct TaskList       *glTaskEntry     = 0;
struct SemaphoreEntry *shSemaphores    = 0;
struct MemoryPage     *glMemoryPages   = 0;
struct KeyStore       *glObjectLookup  = 0;
struct ClassHeader    *glClassDB       = 0;
struct ModuleHeader   *glModules       = 0;
struct OpenInfo       *glOpenInfo      = 0;
struct MsgHandler     *glMsgHandlers   = 0, *glLastMsgHandler = 0;
std::list<CoreTimer> glTimers;
OBJECTID glClassFileID = 0;
APTR glJNIEnv = 0;
UWORD glFunctionID = 3333;
struct rkTask *glCurrentTask = 0;
OBJECTID  glCurrentTaskID  = 0;
OBJECTID  SystemTaskID     = 0;
struct KeyStore *glClassMap = NULL;
struct KeyStore *glFields = NULL;
LONG glPageSize = 4096; // Default page size is 4k
LONG glTotalPages = 0;
LONG glStdErrFlags = 0;
TIMER glCacheTimer = 0;
LONG glMemoryFD = -1;
LONG glKeyState = 0;
LONG glTaskMessageMID = 0;
LONG glValidateProcessID = 0;
LONG glProcessID  = 0;
LONG glInstanceID = 0;
LONG glMemRegSize = 0;
LONG glActionCount = AC_END;
LONG glEUID = -1, glEGID = -1, glGID = -1, glUID = -1;
struct rkConfig *glVolumes = NULL; // Volume management object
struct rkConfig *glDatatypes = NULL;
struct FDTable *glFDTable = NULL;
WORD glTotalFDs = 0, glLastFD = 0;
UBYTE glTimerCycle = 1;
CSTRING glIDL = MOD_IDL;
std::unordered_map<OBJECTID, ObjectSignal> glWFOList;

#ifdef __unix__
  THREADVAR LONG glSocket = -1; // Implemented as thread-local because we don't want threads other than main to utilise the messaging system.
  sem_t glObjectSemaphore;
#elif _WIN32
  WINHANDLE glProcessHandle = 0;
  struct public_lock glPublicLocks[PL_END] = {
     { "", 0, 0, 0, FALSE }, // 0
     { "rka", 0, 0, 0, FALSE }, // PL_WAITLOCKS
     { "rkb", 0, 0, 0, FALSE }, // PL_PUBLICMEM
     { "rkc", 0, 0, 0, FALSE }, // PL_FORBID
     { "rkd", 0, 0, 0, FALSE }, // PL_PROCESSES
     { "rke", 0, 0, 0, FALSE }, // PL_SEMAPHORES
     { "rkf", 0, 0, 0, TRUE },  // CN_PUBLICMEM
     { "rkg", 0, 0, 0, TRUE }   // CN_SEMAPHORES
  };
#endif

HOSTHANDLE glConsoleFD = (HOSTHANDLE)-1; // Managed by GetResource()

LARGE glTimeLog   = 0;
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
WORD glShowIO       = FALSE;
WORD glShowPrivate  = FALSE;
WORD glShowPublic   = FALSE;
BYTE *SharedMemory  = 0;
BYTE glMasterTask   = FALSE;
BYTE glProgramStage = STAGE_STARTUP;
BYTE glFullOS       = FALSE;
BYTE glPrivileged   = FALSE;
BYTE glSync         = FALSE;
UBYTE glTaskState   = TSTATE_RUNNING;

LONG glMaxDocViews = 0, glTotalDocViews = 0;

struct KeyStore *glCache = NULL;
LONG glInotify = -1;
struct DocView *glDocView = NULL;

const struct virtual_drive glFSDefault = {
   0xffffffff, ":",
#ifdef _WIN32
   FALSE,     // Windows is not case sensitive by default
#else
   TRUE,      // Unix file systems are usually case sensitive
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

LONG glVirtualTotal = 0;
struct virtual_drive glVirtual[20];

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
#ifdef _WIN32
THREADVAR WINHANDLE tlThreadReadMsg = 0;
THREADVAR WINHANDLE tlThreadWriteMsg = 0;
#else
THREADVAR LONG tlThreadReadMsg = 0;
THREADVAR LONG tlThreadWriteMsg = 0;
#endif

static struct Head glDummyObject;
struct ObjectContext glTopContext = { .Stack = NULL, .Object = &glDummyObject, .Field = NULL }; // Top-level context is a dummy and can be thread-shared
THREADVAR struct ObjectContext *tlContext = &glTopContext;
OBJECTPTR glLocale = NULL;
objTime *glTime = NULL;
struct translate *glTranslate = NULL;

THREADVAR WORD tlMsgRecursion = 0;
THREADVAR struct Message *tlCurrentMsg   = 0;

LONG  (**ManagedActions)(struct Head *Object, APTR Parameters) = 0;
ERROR (*glMessageHandler)(struct Message *) = 0;
void (*glVideoRecovery)(void) = 0;
void (*glKeyboardRecovery)(void) = 0;
void (*glNetProcessMessages)(LONG, APTR) = 0;

// Imported string variables

char glAlphaNumeric[256];
#ifdef __ANDROID__
static struct AndroidBase *AndroidBase = 0;
#endif

//****************************************************************************

#include "data_functions.c"
#include "data_errors.c"
#include "data_locale.c"
