/*********************************************************************************************************************

-MODULE-
Core: The core library provides system calls and controls for the Parasol system.

The Parasol Core is a system library that provides a universal API that works on multiple platforms.  It follows an
object oriented design with granular resource tracking to minimise resource usage and memory leaks.

The portability of the core has been safe-guarded by keeping the functions as generalised as possible.  When writing
code for a target platform it will be possible for the application to be completely sandboxed if the host's system
calls are avoided.

This documentation is intended for technical reference and is not suitable as an introductory guide to the framework.

-END-

*********************************************************************************************************************/

#define PRV_CORE
#define PRV_CORE_MODULE

#ifdef __CYGWIN__
#undef __unix__
#endif

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#ifdef _MSC_VER
 #include <io.h>
#else
 #include <unistd.h>
#endif

#include <forward_list>
#include <sstream>

#ifdef _WIN32
#include <time.h>
#endif

#ifdef __unix__
 #ifndef __USE_GNU
  #define __USE_GNU
 #endif
 #include <dlfcn.h>
 #include <sys/ipc.h>
 #include <sys/stat.h>

 #ifndef __ANDROID__ // The following are not available for Android
  #include <sys/sem.h>
  #include <sys/msg.h>
  #include <sys/shm.h>
  #include <execinfo.h> // backtrace() is unavailable (unreliable) on Android/ARM
 #endif

 #include <pthread.h>

 #include <sys/time.h>
 #include <sys/wait.h>
 #include <string.h>
 #include <signal.h>
 #include <time.h>
 #include <fcntl.h>
 #include <sys/mman.h>
 #include <sys/utsname.h>
 #include <sys/socket.h>
 #include <sys/un.h>
 #include <sys/resource.h>

#endif // __unix__

#ifdef __ANDROID__
#include <linux/ashmem.h>
#include <android/log.h>
#endif

#include "defs.h"
#include <parasol/modules/core.h>

#ifdef _DEBUG // KMSG() prints straight to stderr without going through the log.
#define KMSG(...) //fprintf(stderr, __VA_ARGS__)
#else
#define KMSG(...)
#endif

#define KERR(...) fprintf(stderr, __VA_ARGS__)

#ifdef __unix__
static void CrashHandler(LONG, siginfo_t *, APTR) __attribute__((unused));
static void NullHandler(LONG, siginfo_t *Info, APTR)  __attribute__((unused));
static void child_handler(LONG, siginfo_t *Info, APTR)  __attribute__((unused));
static void DiagnosisHandler(LONG, siginfo_t *Info, APTR)  __attribute__((unused));
#elif _WIN32
static LONG CrashHandler(LONG Code, APTR Address, LONG Continuable, LONG *Info) __attribute__((unused));
static void BreakHandler(void)  __attribute__((unused));
#endif

extern "C" ERR add_archive_class(void);
extern "C" ERR add_compressed_stream_class(void);
extern "C" ERR add_compression_class(void);
extern "C" ERR add_script_class(void);
extern "C" ERR add_task_class(void);
extern "C" ERR add_thread_class(void);
extern "C" ERR add_module_class(void);
extern "C" ERR add_config_class(void);
extern "C" ERR add_time_class(void);
#ifdef __ANDROID__
extern "C" ERR add_asset_class(void);
#endif
extern "C" ERR add_file_class(void);
extern "C" ERR add_storage_class(void);

LONG InitCore(void);
__export void CloseCore(void);
__export ERR OpenCore(OpenInfo *, struct CoreBase **);
static ERR init_volumes(const std::forward_list<std::string> &);

#ifdef _WIN32
#define DLLCALL // __declspec(dllimport)
#define WINAPI  __stdcall
#define HKEY_LOCAL_MACHINE 0x80000002
#define KEY_READ 0x20019
DLLCALL LONG WINAPI RegOpenKeyExA(LONG,STRING,LONG,LONG,APTR *);
DLLCALL LONG WINAPI RegQueryValueExA(APTR,STRING,LONG *,LONG *,BYTE *,LONG *);
DLLCALL void WINAPI CloseHandle(APTR);
#endif

static std::string glHomeFolderName;

#include "static_modules.cpp"

//********************************************************************************************************************

static void print_class_list(void) __attribute__ ((unused));
static void print_class_list(void)
{
   pf::Log log("Class List");
   std::ostringstream out;
   for (auto & [ cid, v ] : glClassDB) {
      out << v.Name << " ";
   }
   log.msg("Total: %d, %s", (LONG)glClassDB.size(), out.str().c_str());
}

//********************************************************************************************************************
// The _init symbol is used by dlopen() to initialise libraries.

#ifdef __unix__
/*
void _init(void)
{
   //InitCore();
}
*/
#endif

//********************************************************************************************************************

ERR OpenCore(OpenInfo *Info, struct CoreBase **JumpTable)
{
   #ifdef __unix__
      struct timeval tmday;
      struct timezone tz;
      #ifndef __ANDROID__
         bool hold_priority;
      #endif
   #endif
   LONG i;

   if (!Info) return ERR::Failed;
   if ((Info->Flags & OPF::ERROR) != OPF::NIL) Info->Error = ERR::Failed;
   glOpenInfo   = Info;
   tlMainThread = true;
   glCodeIndex  = 0; // Reset the code index so that CloseCore() will work.

   if (glProcessID) fprintf(stderr, "Core module has already been initialised (OpenCore() called more than once.)\n");

#ifdef __unix__
   // Record the 'original' user id and group id, which we need to know in case the binary has been run with the suid
   // bit set.  E.g. If I am user 500 and the program is run as suid, then the ID's are:
   //
   //  EUID: 0 (root);  UID: 500 (user)

   glEUID = geteuid();
   glEGID = getegid();
   glUID  = getuid();
   glGID  = getgid();

   // Reset the file anti-mask for new files.  This is important in case the file system needs to set permission flags
   // for the 'group' and 'other' security flags.

   umask(0);
#endif

#if !defined(__ANDROID__) && defined(__unix__)
   hold_priority = false;

   // If the executable has suid-root rights, we can use direct video access.  The first thing that we're going to
   // do here is actually drop this level of access so that we do not put system security at risk.  This is also
   // important for ensuring any created files have the user's original login and group id.  Privileges can be regained
   // later if needed for the video setup.
   //
   // Any code that needs to regain admin access can use the following code segment:
   //
   //    if (!SetResource(RES::PRIVILEGED_USER, TRUE)) {
   //       ...admin access...
   //       SetResource(RES::PRIVILEGED_USER, FALSE);
   //    }

   seteuid(glUID);  // Ensure that the rest of our code is run under the real user name instead of admin
   setegid(glGID);  // Ensure that we run under the user's default group (important for file creation)

#elif _WIN32
   LONG id = 0;
   #ifdef _DEBUG
      winInitialise(&id, NULL); // Don't set a break handler, this will allow GDB intercept CTRL-C.
   #else
      winInitialise(&id, (APTR)&BreakHandler);
   #endif
#endif

   // Randomise the internal random variables

   #ifdef __unix__
      gettimeofday(&tmday, &tz);
      srand(tmday.tv_sec + tmday.tv_usec);
   #elif _WIN32
      srand(winGetTickCount());
   #else
      #error Platform needs randomisation support.
   #endif

   // Get the ID of the current process

   #ifdef __unix__
      glProcessID = getpid();
   #elif _WIN32
      glProcessID = winGetCurrentProcessId();
      glProcessHandle = winGetCurrentProcess();
   #else
      #error Require code to obtain the process ID.
   #endif

   if ((Info->Flags & OPF::ROOT_PATH) != OPF::NIL) SetResourcePath(RP::ROOT_PATH, Info->RootPath);
   if ((Info->Flags & OPF::MODULE_PATH) != OPF::NIL) SetResourcePath(RP::MODULE_PATH, Info->ModulePath);
   if ((Info->Flags & OPF::SYSTEM_PATH) != OPF::NIL) SetResourcePath(RP::SYSTEM_PATH, Info->SystemPath);

   if (glRootPath.empty())   {
      #ifdef _WIN32
         char buffer[128];
         if (winGetExeDirectory(sizeof(buffer), buffer)) glRootPath = buffer;
         else if (winGetCurrentDirectory(sizeof(buffer), buffer)) glRootPath = buffer;
         else {
            fprintf(stderr, "Failed to determine root folder.\n");
            return ERR::Failed;
         }
         if (glRootPath.back() != '\\') glRootPath += '\\';
      #else
         // Get the folder of the running process.
         char buffer[128];
         char procfile[50];
         snprintf(procfile, sizeof(procfile), "/proc/%d/exe", getpid());

         if (auto len = readlink(procfile, buffer, sizeof(buffer)-1); len > 0) {
            glRootPath.assign(buffer, len);
            // Strip process name
            auto i = glRootPath.find_last_of("/");
            if (i != std::string::npos) glRootPath.resize(i+1);

            // If the binary is in a 'bin' folder then the root is considered to be the parent folder.
            if (glRootPath.ends_with("bin/")) glRootPath.resize(glRootPath.size()-4);
        }
      #endif
   }

   if (glSystemPath.empty()) {
      // When no system path is specified then treat the install as 'run-anywhere' so that "parasol:" == "system:"
      glSystemPath = glRootPath;
   }

   // Debug processing

   if ((Info->Flags & OPF::DETAIL) != OPF::NIL)      glLogLevel = (WORD)Info->Detail;
   if ((Info->Flags & OPF::MAX_DEPTH) != OPF::NIL)   glMaxDepth = (WORD)Info->MaxDepth;
   if ((Info->Flags & OPF::SHOW_MEMORY) != OPF::NIL) glShowPrivate = true;

   // Android sets an important JNI pointer on initialisation.

   if (((Info->Flags & OPF::OPTIONS) != OPF::NIL) and (Info->Options)) {
      for (LONG i=0; LONG(Info->Options[i].Tag) != TAGEND; i++) {
         switch (Info->Options[i].Tag) {
            case TOI::ANDROID_ENV: {
               glJNIEnv = Info->Options[i].Value.Pointer;
               break;
            }
            default:
               break;
         }
      }
   }

   // Check if the privileged flag has been set, which means "don't drop administration privileges if my binary is known to be suid".

#if defined(__unix__) && !defined(__ANDROID__)
   if (((Info->Flags & OPF::PRIVILEGED) != OPF::NIL) and (geteuid() != getuid())) {
      glPrivileged = true;
   }
#endif

   std::forward_list<std::string> volumes;

   pf::vector<std::string> newargs;
   if ((Info->Flags & OPF::ARGS) != OPF::NIL) {
      for (i=1; i < Info->ArgCount; i++) {
         auto arg = Info->Args[i];
         if ((arg[0] != '-') or (arg[1] != '-')) { newargs.push_back(arg); continue; }
         arg += 2; // Skip '--' as this prepends all Core arguments

         if (ERR::Okay IS StrMatch(arg, "log-memory")) {
            glShowPrivate = true;
            glDebugMemory = true;
         }
         else if (ERR::Okay IS StrCompare(arg, "gfx-driver=", 11)) {
            StrCopy(arg+11, glDisplayDriver, sizeof(glDisplayDriver));
         }
         else if ((ERR::Okay IS StrMatch(arg, "set-volume")) and (i+1 < Info->ArgCount)) { // --set-volume scripts=my:location/
            volumes.emplace_front(Info->Args[++i]);
         }
         else if (ERR::Okay IS StrMatch(arg, "no-crash-handler")) glEnableCrashHandler = false;
         else if (ERR::Okay IS StrMatch(arg, "sync"))        glSync = true;
         else if (ERR::Okay IS StrMatch(arg, "log-threads")) glLogThreads = true;
         else if (ERR::Okay IS StrMatch(arg, "log-none"))    glLogLevel = 0;
         else if (ERR::Okay IS StrMatch(arg, "log-error"))   glLogLevel = 1;
         else if (ERR::Okay IS StrMatch(arg, "log-warn"))    glLogLevel = 2;
         else if (ERR::Okay IS StrMatch(arg, "log-warning")) glLogLevel = 2;
         else if (ERR::Okay IS StrMatch(arg, "log-info"))    glLogLevel = 4; // Levels 3/4 are for applications (no internal detail)
         else if (ERR::Okay IS StrMatch(arg, "log-api"))     glLogLevel = 5; // Default level for API messages
         else if (ERR::Okay IS StrMatch(arg, "log-extapi"))  glLogLevel = 6;
         else if (ERR::Okay IS StrMatch(arg, "log-debug"))   glLogLevel = 7;
         else if (ERR::Okay IS StrMatch(arg, "log-trace"))   glLogLevel = 9;
         else if (ERR::Okay IS StrMatch(arg, "log-all"))     glLogLevel = 9; // 9 is the absolute maximum
         else if (ERR::Okay IS StrMatch(arg, "time"))        glTimeLog = PreciseTime();
         #if defined(__unix__) && !defined(__ANDROID__)
         else if (ERR::Okay IS StrMatch(arg, "holdpriority")) hold_priority = true;
         #endif
         else if (ERR::Okay IS StrCompare("home=", arg, 7)) glHomeFolderName.assign(arg + 7);
         else newargs.push_back(Info->Args[i]);
      }

      if (glLogLevel > 2) {
         char cmdline[160];
         size_t pos = 0;
         for (LONG i=0; (i < Info->ArgCount) and (pos < sizeof(cmdline)-1); i++) {
            if (i > 0) cmdline[pos++] = ' ';
            for (LONG j=0; (Info->Args[i][j]) and (pos < sizeof(cmdline)-1); j++) {
               cmdline[pos++] = Info->Args[i][j];
            }
         }
         cmdline[pos] = 0;
         KMSG("Parameters: %s\n", cmdline);
      }
   }

   glShowIO = ((Info->Flags & OPF::SHOW_IO) != OPF::NIL);

#if defined(__unix__) && !defined(__ANDROID__)
   // Setting stdout to non-blocking can prevent dead-locks at the cost of dropping excess output.  It is only
   // necessary if the terminal has the means to lock a resource that is in use by the running program.

   //if (!glSync) {
   //   glStdErrFlags = fcntl(STDERR_FILENO, F_GETFL);
   //   if (!fcntl(STDERR_FILENO, F_SETFL, glStdErrFlags|O_NONBLOCK)) glStdErrFlags |= O_NONBLOCK;
   //}

   // Ensure that the process priority starts out at zero

   if (!hold_priority) {
      LONG p = getpriority(PRIO_PROCESS, 0);
      if (p) nice(-p);
   }
#endif

#ifdef __unix__
   struct sigaction sig;

   sig.sa_flags = SA_SIGINFO;
   if (glEnableCrashHandler) {
      // Subscribe to the following signals for active crash management.
      sig.sa_sigaction = CrashHandler;
      sigaction(SIGINT,  &sig, 0);  // Interrupt from keyboard
      sigaction(SIGHUP,  &sig, 0);  // Hang up detected on controlling terminal
      sigaction(SIGQUIT, &sig, 0);  // Quit from keyboard (ctrl-c)
      sigaction(SIGTERM, &sig, 0);  // Termination signal
      sigaction(SIGSEGV, &sig, 0);  // Illegal read/write of a memory location
      sigaction(SIGFPE,  &sig, 0);  // Floating point exception
      sigaction(SIGILL,  &sig, 0);  // Illegal instruction

      sig.sa_sigaction = DiagnosisHandler;
      sigaction(SIGUSR1, &sig, 0);      // Print a status report for user signal #1
   }

   sig.sa_sigaction = NullHandler;
   sigaction(SIGALRM, &sig, 0);       // Do nothing when alarms are signalled

   //signal(SIGIO,   NullHandler);     // SIGIO is handled by the filesystem module
   signal(SIGPIPE, SIG_IGN);           // Broken pipe: Write to pipe with no readers
   signal(SIGABRT, SIG_IGN);           // Signal originating from abort()
   signal(SIGXFSZ, SIG_IGN);           // Do not signal if writing a large file (e.g. > 2GB)

   //sig.sa_flags |= SA_NOCLDWAIT;
   sig.sa_sigaction = child_handler;
   sigaction(SIGCHLD, &sig, 0);      // This handler is important for responding to dead child tasks immediately (the default for SIGCHILD is ignore)
   sig.sa_flags &= ~SA_NOCLDWAIT;

   // Remove file size limits so that we can R/W files bigger than 2GB

   struct rlimit rlp;
   rlp.rlim_cur = RLIM_INFINITY;
   rlp.rlim_max = RLIM_INFINITY;
   setrlimit(RLIMIT_FSIZE, &rlp);
#endif

   pf::Log log("Core");

#ifdef _WIN32
   activate_console(glLogLevel > 0); // This works for the MinGW runtime libraries but not MSYS2

   // An exception handler deals with crashes unless the program is being debugged.

   if (!winIsDebuggerPresent()) {
      winSetUnhandledExceptionFilter(&CrashHandler);
   }
   else log.msg("A debugger is active.");
#endif

   // Sockets are used on Unix systems to tell our processes when new messages are available for them to read.

   #ifdef __unix__
      if ((glSocket = socket(PF_UNIX, SOCK_DGRAM, 0)) != -1) {
         fcntl(glSocket, F_SETFL, O_NONBLOCK);

         socklen_t socklen;
         struct sockaddr_un *sockpath = get_socket_path(glProcessID, &socklen);

         #ifdef __APPLE__
            unlink(sockpath->sun_path);
         #endif

         if (bind(glSocket, (struct sockaddr *)sockpath, socklen) IS -1) {
            KERR("bind() failed on '%s' [%d]: %s\n", sockpath->sun_path, errno, strerror(errno));
            if (errno IS EADDRINUSE) {
               LONG reuse;

               // If you open-close-open the Core, the socket needs to be bound to an existing bind address.

               KMSG("Attempting to re-use an earlier bind().\n");
               if (setsockopt(glSocket, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) IS -1) {
                  if ((Info->Flags & OPF::ERROR) != OPF::NIL) Info->Error = ERR::SystemCall;
                  return ERR::SystemCall;
               }
            }
            else {
               if ((Info->Flags & OPF::ERROR) != OPF::NIL) Info->Error = ERR::SystemCall;
               return ERR::SystemCall;
            }
         }
      }
      else {
         KERR("Failed to create a new socket communication point.\n");
         if ((Info->Flags & OPF::ERROR) != OPF::NIL) Info->Error = ERR::SystemCall;
         return ERR::SystemCall;
      }

      RegisterFD(glSocket, RFD::READ, NULL, NULL);
   #endif

   log.msg("Process: %d, Sync: %s, Root: %s", glProcessID, (glSync) ? "Y" : "N", glRootPath.c_str());
#ifdef __unix__
   log.msg("UID: %d (%d), EUID: %d (%d); GID: %d (%d), EGID: %d (%d)", getuid(), glUID, geteuid(), glEUID, getgid(), glGID, getegid(), glEGID);
#endif

   init_metaclass();

   if (add_task_class() != ERR::Okay)    { CloseCore(); return ERR::AddClass; }
   if (add_thread_class() != ERR::Okay)  { CloseCore(); return ERR::AddClass; }
   if (add_module_class() != ERR::Okay)  { CloseCore(); return ERR::AddClass; }
   if (add_time_class() != ERR::Okay)    { CloseCore(); return ERR::AddClass; }
   if (add_config_class() != ERR::Okay)  { CloseCore(); return ERR::AddClass; }
   if (add_storage_class() != ERR::Okay) { CloseCore(); return ERR::AddClass; }
   if (add_file_class() != ERR::Okay)    { CloseCore(); return ERR::AddClass; }
   if (add_script_class() != ERR::Okay)  { CloseCore(); return ERR::AddClass; }
   if (add_archive_class() != ERR::Okay) { CloseCore(); return ERR::AddClass; }
   if (add_compressed_stream_class() != ERR::Okay) { CloseCore(); return ERR::AddClass; }
   if (add_compression_class() != ERR::Okay) { CloseCore(); return ERR::AddClass; }
   #ifdef __ANDROID__
   if (add_asset_class() != ERR::Okay) { CloseCore(); return ERR::AddClass; }
   #endif

   if (!(glCurrentTask = extTask::create::untracked())) {
      CloseCore();
      return ERR::CreateObject;
   }

   if (init_volumes(volumes) != ERR::Okay) {
      KERR("Failed to initialise the filesystem.");
      CloseCore();
      return ERR::Failed;
   }

   fs_initialised = true;

#ifndef PARASOL_STATIC
   if ((Info->Flags & OPF::SCAN_MODULES) IS OPF::NIL) {
      ERR error;
      auto file = objFile::create { fl::Path(glClassBinPath), fl::Flags(FL::READ) };

      if (file.ok()) {
         LONG filesize;
         file->get(FID_Size, &filesize);

         LONG hdr;
         file->read(&hdr, sizeof(hdr));
         if (hdr IS CLASSDB_HEADER) {
            while (file->Position + ClassRecord::MIN_SIZE < filesize) {
               ClassRecord item;
               if ((error = item.read(*file)) != ERR::Okay) break;

               if (glClassDB.contains(item.ClassID)) {
                  log.warning("Invalid class dictionary file, %s is registered twice.", item.Name.c_str());
                  error = ERR::Failed;
                  break;
               }
               glClassDB[item.ClassID] = item;
            }

            if (error != ERR::Okay) glScanClasses = true;
         }
         else {
            // File is probably from an old version and requires recalculation.
            glScanClasses = true;
         }
      }
      else glScanClasses = true; // If no file, a database rebuild is required.
   }
#endif

   if (!newargs.empty()) SetArray(glCurrentTask, FID_Parameters, &newargs, newargs.size());

   // In Windows, set the PATH environment variable so that DLL's installed under modules:lib can be found.

#ifdef _WIN32
   {
      STRING libpath;
      if (ResolvePath("modules:lib", RSF::NO_FILE_CHECK, &libpath) IS ERR::Okay) {
         winSetDllDirectory(libpath);
         FreeResource(libpath);
      }
      else log.trace("Failed to resolve modules:lib");
   }
#endif

#ifndef PARASOL_STATIC
   // Generate the Core table for our new task
   LocalCoreBase = (struct CoreBase *)build_jump_table(glFunctions);
#else
   LocalCoreBase = NULL;

   register_static_modules();

   // Initialise all the modules because we don't retain a class database in static builds.
   // Note that the order of initialisation is variable because glStaticModules is a map.
   // This can lead to rare bugs in custom builds where modules have dependencies on each other.

   {
      pf::Log log("Core");
      log.branch("Initialising %d static modules.", LONG(std::ssize(glStaticModules)));
      for (auto & [ name, hdr ] : glStaticModules) {
         objModule::create mod = { pf::FieldValue(FID_Name, name.c_str()) };
      }
   }
#endif

   // Broadcast the creation of the new task

   evTaskCreated task_created = { EVID_SYSTEM_TASK_CREATED, glCurrentTask->UID };
   BroadcastEvent(&task_created, sizeof(task_created));

#ifndef PARASOL_STATIC
   if ((Info->Flags & OPF::SCAN_MODULES) != OPF::NIL) {
      log.msg("Class scanning has been enforced by user request.");
      glScanClasses = true;
   }

   if (glScanClasses) scan_classes();
#endif

   #ifdef _DEBUG
      print_class_list();
   #endif

   log.msg("PROGRAM OPENED");

   glSystemState = 0; // Indicates that initialisation is complete.
   if ((Info->Flags & OPF::ERROR) != OPF::NIL) Info->Error = ERR::Okay;

   *JumpTable = LocalCoreBase;
   return ERR::Okay;
}

//********************************************************************************************************************

static const CSTRING signals[] = {
   "00: UNKNOWN",
   "01: SIGHUP",
   "02: SIGINT",
   "03: Quit from keyboard",
   "04: SIGILL",
   "05: SIGTRAP",
   "06: SIGABRT",
   "07: SIGBUS",
   "08: SIGFPE",
   "09: Kill Signal",
   "10: SIGUSR1",
   "11: Invalid memory access",
   "13: SIGPIPE",
   "14: Alarm",
   "15: SIGTERM",
   "16: User Signal #1",
   "17: User Signal #2",
   "18: SIGCHLD",
   "19: SIGCONT",
   "20: SIGSTOP",
   "21: SIGTSTP",
   "22: SIGTTOU",
   "23: SIGSTOP",
   "24: SIGTSTP",
   "25: SIGCONT",
   "26: SIGTTIN",
   "27: SIGTTOU",
	"28: SIGURG",
	"29: SIGXCPU",
	"30: SIGXFSZ",
	"31: SIGVTALRM",
	"32: SIGPROF",
	"33: SIGWINCH",
	"34: SIGIO",
	"35: SIGPOLL",
	"36: SIGPWR",
	"37: SIGSYS"
};

#ifdef __ANDROID__
void print_diagnosis(LONG Signal)
{
   LOGE("Application diagnosis, signal %d.", Signal);

   auto ctx = tlContext;

   if (glCodeIndex != CP_PRINT_CONTEXT) {
      if (Signal) {
         if ((Signal > 0) and (Signal < ARRAYSIZE(signals))) {
            LOGE("  Signal ID:      %s", signals[Signal]);
         }
         else LOGE("  Signal ID:      %d", Signal);
      }
      glCodeIndex = CP_PRINT_CONTEXT;

      if ((ProcessID IS glProcessID) and (ctx != &glTopContext)) {
         CLASSID class_id;
         STRING class_name;
         if ((class_id = ctx->object()->ClassID)) class_name = ResolveClassID(ctx->object()->ClassID);
         else class_name = "None";
         LOGE("  Object Context: #%d / %p [Class: %s / $%.8x]", ctx->object()->UID, ctx->object(), class_name, class_id);
      }

      glPageFault = 0;
   }

   // Print the last action to be executed at the time of the crash.  If this code fails, it indicates a corrupt action table.

   if (glCodeIndex != CP_PRINT_ACTION) {
      glCodeIndex = CP_PRINT_ACTION;
      if (ctx->Action > 0) {
         if (ctx->Field) {
            LOGE("  Last Action:    Set.%s", ctx->Field->Name);
         }
         else LOGE("  Last Action:    %s", ActionTable[ctx->Action].Name);
      }
      else if (ctx->Action < 0) {
         LOGE("  Last Method:    %d", ctx->Action);
      }
   }
   else LOGE("  The action table is corrupt.");
}

#else

void print_diagnosis(LONG Signal)
{
   FILE *fd;
#ifndef _WIN32
   char filename[64];
#endif

   //if (glLogLevel <= 1) return;

   fd = stderr;
   fprintf(fd, "Diagnostic Information:\n\n");

   // Print details of the object context at the time of the crash.  If this code fails, it indicates that the object context is corrupt.

   auto ctx = tlContext;

   if (glCodeIndex != CP_PRINT_CONTEXT) {
      #ifdef __unix__
         fprintf(fd, "  Page Fault:     %p\n", glPageFault);
      #endif
      fprintf(fd, "  Task ID:        %d\n", glCurrentTask->UID);
      fprintf(fd, "  Process ID:     %d\n", glCurrentTask->ProcessID);
      if (Signal) {
         if ((Signal > 0) and (Signal < ARRAYSIZE(signals))) {
            fprintf(fd, "  Signal ID:      %s\n", signals[Signal]);
         }
         else fprintf(fd, "  Signal ID:      %d\n", Signal);
      }
      glCodeIndex = CP_PRINT_CONTEXT;

      if (ctx->object()) {
         CLASSID class_id = 0;
         CSTRING class_name;
         if (ctx != &glTopContext) {
            if ((class_id = ctx->object()->classID())) class_name = ResolveClassID(class_id);
            else class_name = "None";
         }
         else class_name = "None";

         fprintf(fd, "  Object Context: #%d / %p [Class: %s / $%.8x]\n", ctx->object()->UID, ctx->object(), class_name, class_id);
      }

      glPageFault = 0;
   }

   // Print the last action to be executed at the time of the crash.  If this code fails, it indicates a corrupt action table.

   if (glCodeIndex != CP_PRINT_ACTION) {
      glCodeIndex = CP_PRINT_ACTION;
      if (ctx->Action > 0) {
         if (ctx->Field) fprintf(fd, "  Last Action:    Set.%s\n", ctx->Field->Name);
         else fprintf(fd, "  Last Action:    %s\n", ActionTable[ctx->Action].Name);
      }
      else if (ctx->Action < 0) fprintf(fd, "  Last Method:    %d\n", ctx->Action);
   }
   else fprintf(fd, "  The action table is corrupt.\n");

   fprintf(fd, "\n");

   // Backtrace it
#if defined(__unix__) && !defined(__ANDROID__)
   void *trace[16];
   char **messages = (char **)NULL;
   int i, trace_size = 0;

   trace_size = backtrace(trace, ARRAYSIZE(trace));
   // overwrite sigaction with caller's address
   //trace[1] = pnt;

   messages = backtrace_symbols(trace, trace_size);
   // skip first stack frame (points here)
   fprintf(fd, "Execution path:\n");
   for (i=1; i < trace_size; ++i) fprintf(fd, " %s\n", messages[i]);
#endif

#ifndef _WIN32
   // If the output was to a file, now print that file to stderr

   if (fd != stderr) {
      char buffer[4096];

      rewind(fd);
      if (auto len = fread(buffer, 1, sizeof(buffer)-1, fd); len > 0) {
         buffer[len] = 0;
         fflush(NULL);
         fsync(STDERR_FILENO);
         fprintf(stderr, "%s", buffer);

         // Copy process status to the output file

         snprintf(filename, sizeof(filename), "/proc/%d/status", glCurrentTask->ProcessID);
         if (auto pf = fopen(filename, "r")) {
            if ((len = fread(buffer, 1, sizeof(buffer)-1, pf)) > 0) {
               buffer[len] = 0;
               fprintf(fd, "\n%s\n", buffer);
            }
            fclose(pf);
         }
      }
      fclose(fd);
   }
#endif
}
#endif

//********************************************************************************************************************

#ifdef __unix__
static void DiagnosisHandler(LONG SignalNumber, siginfo_t *Info, APTR Context)
{
   if (glLogLevel < 2) return;
   print_diagnosis(0);
   fflush(NULL);
}
#endif

//********************************************************************************************************************

#ifdef __unix__
static void CrashHandler(LONG SignalNumber, siginfo_t *Info, APTR Context)
{
   pf::Log log("Core");

   if (glCrashStatus > 1) {
      if ((glCodeIndex) and (glCodeIndex IS glLastCodeIndex)) {
         fprintf(stderr, "Unable to recover - exiting immediately.\n");
         exit(255);
      }

      glLastCodeIndex = glCodeIndex;
   }

   if (!glCodeIndex) glCodeIndex = CP_START;

   // Analyse the type of signal that has occurred and respond appropriately

   if (glCrashStatus IS 0) {
      if (((SignalNumber IS SIGQUIT) or (SignalNumber IS SIGHUP)))  {
         log.msg("Termination request - SIGQUIT or SIGHUP.");
         SendMessage(MSGID_QUIT, MSF::NIL, NULL, 0);
         glCrashStatus = 1;
         return;
      }

      if (glLogLevel >= 5) {
         log.msg("Process terminated.\n");
      }
      else if ((SignalNumber > 0) and (SignalNumber < ARRAYSIZE(signals))) {
         fprintf(stderr, "\nProcess terminated, signal %s.\n\n", signals[SignalNumber]);
      }
      else fprintf(stderr, "\nProcess terminated, signal %d.\n\n", SignalNumber);

      if ((SignalNumber IS SIGILL) or (SignalNumber IS SIGFPE) or
          (SignalNumber IS SIGSEGV) or (SignalNumber IS SIGBUS)) {
         glPageFault = Info->si_addr;
      }
      else glPageFault = 0;
   }
   else {
      fprintf(stderr, "Secondary crash or hangup request at code index %d (last %d).\n", glCodeIndex, glLastCodeIndex);
      kill(getpid(), SIGKILL);
      exit(255);
   }

   glCrashStatus = 2;

   print_diagnosis(SignalNumber);

   CloseCore();
   exit(255);
}
#endif

//********************************************************************************************************************

#ifdef __unix__
static void NullHandler(LONG SignalNumber, siginfo_t *Info, APTR Context)
{
   //printf("Alarm signalled (sig %d).\n", SignalNumber);
}
#endif

//********************************************************************************************************************

#ifdef __unix__
static void child_handler(LONG SignalNumber, siginfo_t *Info, APTR Context)
{
#if 0
   parasol:Log log(__FUNCTION__);

   LONG childprocess = Info->si_pid;

   // Get the return code

   LONG status = 0;
   waitpid(Info->si_pid, &status, WNOHANG);
   LONG result = WEXITSTATUS(status);

   log.warning("Process #%d exited, return-code %d.", childprocess, result);

   // Store the return code for this process in any Task object that is associated with it.
   //
   // !!! TODO: The slow methodology of this loop needs attention !!!

   for (const auto & mem : glPrivateMemory) {
      if (!(mem.Flags & MEM::OBJECT)) continue;

      objTask *task;
      if ((task = mem.Address)) {
         if ((task->ClassID IS ID_TASK) and (task->ProcessID IS childprocess)) {
            task->ReturnCode    = result;
            task->ReturnCodeSet = true;
            break;
         }
      }
   }

   validate_process(childprocess);
#endif
}
#endif

//********************************************************************************************************************
// 2014-01-01: The crash handler is having major problems on Windows, it seems to be killed early on in the cleanup
// process and never in the same place.  Log output doesn't flush properly either.

#ifdef _WIN32

#include "microsoft/windefs.h"

const CSTRING ExceptionTable[EXP_END] = {
   "Unknown exception type",
   "Access violation",
   "Breakpoint encountered",
   "Misaligned data access",
   "Invalid numeric calculation",
   "Division by zero",
   "Illegal instruction execution",
   "Stack overflow"
};

APTR glExceptionAddress = 0;

static LONG CrashHandler(LONG Code, APTR Address, LONG Continuable, LONG *Info)
{
   pf::Log log("Core");

   //winDeathBringer(0);  // Win7 doesn't like us calling SendMessage() during our handler?

   if (!glProcessID) return 1;

   if (glCrashStatus > 1) {
      if ((glCodeIndex) and (glCodeIndex IS glLastCodeIndex)) {
         fprintf(stderr, "Unable to recover - exiting immediately.\n");
         fflush(NULL);
         return 1;
      }
      glLastCodeIndex = glCodeIndex;
   }

   if (Code < EXP_END) {
      if (glCrashStatus IS 0) {
         if (glLogLevel >= 5) {
            log.warning("CRASH!"); // Using LogF is helpful because branched output can indicate where the crash occurred.
         }
         else fprintf(stderr, "\n\nCRASH!");

         fprintf(stderr, "\n%s (%s), at address: %p\n", ExceptionTable[Code], (Continuable) ? "Continuable" : "Fatal", Address);
         if ((Code IS EXP_ACCESS_VIOLATION) and (Info)) {
            CSTRING type;
            if (Info[0] IS 1) type = "write";
            else if (Info[0] IS 0) type = "read";
            else if (Info[0] IS 8) type = "execution";
            else type = "access";
            fprintf(stderr, "Attempted %s on address %p\n", type, ((void **)(Info+1))[0]);
         }
         fprintf(stderr, "\n");
      }
      else {
         fprintf(stderr, "Recovering from secondary crash (%s) at code index %d.\n", ExceptionTable[Code], glCodeIndex);
         return 1;
      }
   }
   else fprintf(stderr, "\n\nCRASH!  Exception code of %d is unrecognised.\n\n", Code);

   glCrashStatus = 2;

   print_diagnosis(0);
   fflush(NULL);

   CloseCore();
   return 2; // Force immediate termination of the process
}
#endif

//********************************************************************************************************************

extern "C" ERR convert_errno(LONG Error, ERR Default)
{
   switch (Error) {
      case 0:       return ERR::Okay;
      case ENAMETOOLONG: return ERR::BufferOverflow;
      case EACCES:  return ERR::NoPermission;
      case EPERM:   return ERR::NoPermission;
      case EBUSY:   return ERR::Locked;
      case EROFS:   return ERR::ReadOnly;
      case EMFILE:  return ERR::ArrayFull;
      case ENFILE:  return ERR::ArrayFull;
      case ENOENT:  return ERR::FileNotFound;
      case ENOMEM:  return ERR::AllocMemory;
      case ENOTDIR: return ERR::FileNotFound;
      case EEXIST:  return ERR::FileExists;
      case ENOSPC:  return ERR::OutOfSpace;
      case EFAULT:  return ERR::IllegalAddress;
      case EIO:     return ERR::Failed;
      #ifdef ELOOP
      case ELOOP:   return ERR::Loop;
      #endif
      default:      return Default;
   }
}

//********************************************************************************************************************

#ifdef _WIN32
static void BreakHandler(void)
{
   pf::Log log("Core");

   //winDeathBringer(0);  // Win7 doesn't like us calling SendMessage() during our handler?

   if (glLogLevel >= 5) {
      log.warning("USER BREAK"); // Using log is helpful for branched output to indicate where the crash occurred
   }
   else fprintf(stderr, "\nUSER BREAK");

   glCrashStatus = 1;

   print_diagnosis(0);
   fflush(NULL);

   CloseCore();
}
#endif

//********************************************************************************************************************

#ifdef _WIN32
static void win32_enum_folders(CSTRING Volume, CSTRING Label, CSTRING Path, CSTRING Icon, BYTE Hidden)
{
   SetVolume(Volume, Path, Icon, Label, NULL, VOLUME::REPLACE | (Hidden ? VOLUME::HIDDEN : VOLUME::NIL));
}
#endif

//********************************************************************************************************************

static ERR init_volumes(const std::forward_list<std::string> &Volumes)
{
   pf::Log log("Core");

   log.branch("Initialising filesystem volumes.");

   glVirtual[0] = glFSDefault;

   log.trace("Attempting to create SystemVolumes object.");

   // Add system volumes that require run-time determination.  For the avoidance of doubt, on Unix systems the
   // default settings for a fixed installation are:
   //
   // OPF::ROOT_PATH   : parasol : glRootPath   = /usr/local
   // OPF::MODULE_PATH : modules : glModulePath = %ROOT%/lib/parasol
   // OPF::SYSTEM_PATH : system  : glSystemPath = %ROOT%/share/parasol

   #ifdef _WIN32
      SetVolume("parasol", glRootPath.c_str(), "programs/filemanager", NULL, NULL, VOLUME::REPLACE|VOLUME::HIDDEN|VOLUME::SYSTEM);
      SetVolume("system", glRootPath.c_str(), "misc/brick", NULL, NULL, VOLUME::REPLACE|VOLUME::HIDDEN|VOLUME::SYSTEM);

      #ifndef PARASOL_STATIC
      if (!glModulePath.empty()) {
         SetVolume("modules", glModulePath.c_str(), "misc/brick", NULL, NULL, VOLUME::REPLACE|VOLUME::HIDDEN|VOLUME::SYSTEM);
      }
      else SetVolume("modules", "system:lib/", "misc/brick", NULL, NULL, VOLUME::REPLACE|VOLUME::HIDDEN|VOLUME::SYSTEM);
      #endif
   #elif __unix__
      SetVolume("parasol", glRootPath.c_str(), "programs/filemanager", NULL, NULL, VOLUME::REPLACE|VOLUME::HIDDEN|VOLUME::SYSTEM);
      SetVolume("system", glSystemPath.c_str(), "misc/brick", NULL, NULL, VOLUME::REPLACE|VOLUME::SYSTEM);

      #ifndef PARASOL_STATIC
      if (!glModulePath.empty()) {
         SetVolume("modules", glModulePath.c_str(), "misc/brick", NULL, NULL, VOLUME::REPLACE|VOLUME::HIDDEN|VOLUME::SYSTEM);
      }
      else {
         std::string path = glRootPath + "lib/parasol/";
         SetVolume("modules", path.c_str(), "misc/brick", NULL, NULL, VOLUME::REPLACE|VOLUME::HIDDEN|VOLUME::SYSTEM);
      }
      #endif

      SetVolume("drive1", "/", "devices/storage", "Linux", "hd", VOLUME::REPLACE|VOLUME::SYSTEM);
      SetVolume("etc", "/etc", "tools/cog", NULL, NULL, VOLUME::REPLACE|VOLUME::SYSTEM);
      SetVolume("usr", "/usr", NULL, NULL, NULL, VOLUME::REPLACE|VOLUME::SYSTEM);
   #endif

   // Configure some standard volumes.

   #ifdef __ANDROID__
      SetVolume("assets", "EXT:FileAssets", NULL, NULL, NULL, VOLUME::REPLACE|VOLUME::HIDDEN|VOLUME::SYSTEM);
      SetVolume("templates", "assets:templates/", "misc/openbook", NULL, NULL, VOLUME::HIDDEN|VOLUME::SYSTEM);
      SetVolume("config", "localcache:config/|assets:config/", "tools/cog", NULL, NULL, VOLUME::HIDDEN|VOLUME::SYSTEM);
   #else
      SetVolume("templates", "scripts:templates/", "misc/openbook", NULL, NULL, VOLUME::HIDDEN|VOLUME::SYSTEM);
      SetVolume("config", "system:config/", "tools/cog", NULL, NULL, VOLUME::HIDDEN|VOLUME::SYSTEM);
      if (AnalysePath("parasol:bin/", NULL) IS ERR::Okay) { // Bin is the location of the fluid and parasol binaries
         SetVolume("bin", "parasol:bin/", NULL, NULL, NULL, VOLUME::HIDDEN|VOLUME::SYSTEM);
      }
      else SetVolume("bin", "parasol:", NULL, NULL, NULL, VOLUME::HIDDEN|VOLUME::SYSTEM);
   #endif

   SetVolume("temp", "user:temp/", "items/trash", NULL, NULL, VOLUME::HIDDEN|VOLUME::SYSTEM);
   SetVolume("fonts", "system:config/fonts/", "items/font", NULL, NULL, VOLUME::HIDDEN|VOLUME::SYSTEM);
   SetVolume("scripts", "system:scripts/", "filetypes/source", NULL, NULL, VOLUME::HIDDEN|VOLUME::SYSTEM);
   SetVolume("styles", "system:config/styles/", "tools/image_gallery", NULL, NULL, VOLUME::HIDDEN);

   // Some platforms need to have special volumes added - these are provided in the OpenInfo structure passed to
   // the Core.

   if (((glOpenInfo->Flags & OPF::OPTIONS) != OPF::NIL) and (glOpenInfo->Options)) {
      for (LONG i=0; LONG(glOpenInfo->Options[i].Tag) != TAGEND; i++) {
         switch (glOpenInfo->Options[i].Tag) {
            case TOI::LOCAL_CACHE: {
               SetVolume("localcache", glOpenInfo->Options[i].Value.String, NULL, NULL, NULL, VOLUME::REPLACE|VOLUME::HIDDEN|VOLUME::SYSTEM);
               break;
            }
            case TOI::LOCAL_STORAGE: {
               SetVolume("localstorage", glOpenInfo->Options[i].Value.String, NULL, NULL, NULL, VOLUME::REPLACE|VOLUME::HIDDEN|VOLUME::SYSTEM);
               break;
            }
            default:
               break;
         }
      }
   }

   // The client can specify glHomeFolderName on the command-line if desired.

   if (glHomeFolderName.empty()) glHomeFolderName.assign("parasol");

   std::string buffer("config:users/default/");

   #ifdef __unix__
      if (auto homedir = getenv("HOME"); homedir and homedir[0] and (StrMatch("/", homedir) != ERR::Okay)) {
         buffer = homedir;
         if (buffer.back() IS '/') buffer.pop_back();

         SetVolume("home", buffer.c_str(), "users/user", NULL, NULL, VOLUME::REPLACE);

         buffer += "/." + glHomeFolderName + "/";
      }
      else if (auto logname = getenv("LOGNAME"); logname and (logname[0])) {
         buffer = std::string("config:users/") + logname + "/";
      }
   #elif _WIN32
      // Attempt to get the path of the user's personal folder.  If the Windows system doesn't have this
      // facility, attempt to retrieve the login name and store the user files in the system folder.

      char user_folder[256];
      if (winGetUserFolder(user_folder, sizeof(user_folder))) {
         buffer = user_folder + glHomeFolderName + "\\";
      }
      else if (winGetUserName(user_folder, sizeof(user_folder)) and (user_folder[0])) {
         buffer.append(user_folder);
         buffer += "/";
      }
   #endif

   // Copy the default configuration files to the user: folder.  This also has the effect of creating the user
   // folder if it does not already exist.

   if (buffer != "config:users/default/") {
      LOC location_type = LOC::NIL;
      if ((AnalysePath(buffer.c_str(), &location_type) != ERR::Okay) or (location_type != LOC::DIRECTORY)) {
         buffer.pop_back();
         SetDefaultPermissions(-1, -1, PERMIT::READ|PERMIT::WRITE);
            CopyFile("config:users/default/", buffer.c_str(), NULL);
         SetDefaultPermissions(-1, -1, PERMIT::NIL);
         buffer += '/';
      }

      buffer += "|config:users/default/";
   }

   SetVolume("user", buffer.c_str(), "users/user", NULL, NULL, VOLUME::REPLACE|VOLUME::SYSTEM);

   // Make sure that certain default directories exist

   CreateFolder("user:config/", PERMIT::READ|PERMIT::EXEC|PERMIT::WRITE);
   CreateFolder("user:temp/", PERMIT::READ|PERMIT::EXEC|PERMIT::WRITE);

   if (AnalysePath("temp:", NULL) != ERR::Okay) {
      SetVolume("temp", "user:temp/", "items/trash", NULL, NULL, VOLUME::REPLACE|VOLUME::HIDDEN|VOLUME::SYSTEM);
   }

   if (AnalysePath("clipboard:", NULL) != ERR::Okay) {
      SetVolume("clipboard", "temp:clipboard/", "items/clipboard", NULL, NULL, VOLUME::REPLACE|VOLUME::HIDDEN|VOLUME::SYSTEM);
   }

   // Look for the following drive types:
   //
   // CD-ROMS:               CD1/CD2/CD3
   // Removable Media:       Disk1/Disk2 (floppies etc)
   // Hard Drive Partitions: HD1/HD2
   //
   // NOTE: In the native release all media, including volumes, are controlled by the mountdrives program.
   // Mountdrives also happens to manage the system:hardware/drives.cfg file.

#ifdef _WIN32
   {
      char buffer[256];
      if (auto len = winGetLogicalDriveStrings(buffer, sizeof(buffer)); len > 0) {
         char disk[] = "disk1";
         char cd[]   = "cd1";
         char hd[]   = "drive1";
         char net[]  = "net1";

         for (LONG i=0; i < len; i++) {
            auto type = winGetDriveType(buffer+i);

            buffer[i+2] = '/';

            char label[2] = { buffer[i], 0 };

            if (type IS DRIVETYPE_REMOVABLE) {
               SetVolume(disk, buffer+i, "devices/storage", label, "disk", VOLUME::NIL);
               disk[4]++;
            }
            else if (type IS DRIVETYPE_CDROM) {
               SetVolume(cd, buffer+i, "devices/compactdisc", label, "cd", VOLUME::NIL);
               cd[2]++;
            }
            else if (type IS DRIVETYPE_FIXED) {
               SetVolume(hd, buffer+i, "devices/storage", label, "hd", VOLUME::NIL);
               hd[5]++;
            }
            else if (type IS DRIVETYPE_NETWORK) {
               SetVolume(net, buffer+i, "devices/network", label, "network", VOLUME::NIL);
               net[3]++;
            }
            else log.warning("Drive %s identified as unsupported type %d.", buffer+i, type);

            while (buffer[i]) i++;
         }
      }

      winEnumSpecialFolders(&win32_enum_folders);
   }
#endif

#if defined(__linux__) && !defined(__ANDROID__)

   // /proc/mounts contains a list of all mounted file systems, one for each line.
   //
   // Format:  devicename mountpoint fstype access 0 0
   // Example: /dev/hda1  /winnt     ntfs   ro     0 0
   //
   // We extract all lines with /dev/hd** and convert those into drives.

   char mount[80], drivename[] = "driveXXX", devpath[40];
   LONG file;

   log.msg("Scanning /proc/mounts for hard disks");

   LONG driveno = 2; // Drive 1 is already assigned to root, so start from #2
   if ((file = open("/proc/mounts", O_RDONLY)) != -1) {
      LONG size = lseek(file, 0, SEEK_END);
      lseek(file, 0, SEEK_SET);
      if (size < 1) size = 8192;

      STRING buffer;
      if (AllocMemory(size, MEM::NO_CLEAR, (APTR *)&buffer, NULL) IS ERR::Okay) {
         size = read(file, buffer, size);
         buffer[size] = 0;

         CSTRING str = buffer;
         while (*str) {
            if (ERR::Okay IS StrCompare("/dev/hd", str)) {
               // Extract mount point

               LONG i = 0;
               while ((*str) and (*str > 0x20)) {
                  if (i < (LONG)sizeof(devpath)-1) devpath[i++] = *str;
                  str++;
               }
               devpath[i] = 0;

               while ((*str) and (*str <= 0x20)) str++;
               for (i=0; (*str) and (*str > 0x20) and (i < (LONG)sizeof(mount)-1); i++) mount[i] = *str++;
               mount[i] = 0;

               if ((mount[0] IS '/') and (!mount[1]));
               else {
                  IntToStr(driveno++, drivename+5, 3);
                  SetVolume(drivename, mount, "devices/storage", NULL, "hd", VOLUME::NIL);
               }
            }

            // Next line
            while ((*str) and (*str != '\n')) str++;
            while ((*str) and (*str <= 0x20)) str++;
         }
         FreeResource(buffer);
      }
      else log.warning(ERR::AllocMemory);

      close(file);
   }
   else log.warning(ERR::File);

   const CSTRING cdroms[] = {
      "/mnt/cdrom", "/mnt/cdrom0", "/mnt/cdrom1", "/mnt/cdrom2", "/mnt/cdrom3", "/mnt/cdrom4", "/mnt/cdrom5", "/mnt/cdrom6", // RedHat
      "/cdrom", "/cdrom0", "/cdrom1", "/cdrom2", "/cdrom3" // Debian
   };
   char cdname[] = "cd1";

   for (LONG i=0; i < ARRAYSIZE(cdroms); i++) {
      if (!access(cdroms[i], F_OK)) {
         SetVolume(cdname, cdroms[i], "devices/compactdisc", NULL, "cd", VOLUME::NIL);
         cdname[2] = cdname[2] + 1;
      }
   }
#endif

   // Create the 'archive' volume (non-essential)

   create_archive_volume();

   // Custom volumes and overrides specified from the command-line

   for (auto vol : Volumes) {
      if (auto v = vol.find('='); v != std::string::npos) {
         std::string name(vol, 0, v);
         std::string path(vol, v + 1, vol.size() - (v + 1));

         VOLUME flags = glVolumes.contains(name) ? VOLUME::NIL : VOLUME::HIDDEN;

         SetVolume(name.c_str(), path.c_str(), NULL, NULL, NULL, VOLUME::PRIORITY|flags);
      }
   }

#ifndef PARASOL_STATIC
   // Change glModulePath to an absolute path to optimise the loading of modules.
   STRING mpath;
   if (ResolvePath("modules:", RSF::NO_FILE_CHECK, &mpath) IS ERR::Okay) {
      glModulePath = mpath;
      FreeResource(mpath);
   }
#endif

   return ERR::Okay;
}

#include "core_close.cpp"
