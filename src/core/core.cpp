/*****************************************************************************

-MODULE-
Core: The core library provides system calls and controls for the Parasol system.

The Parasol Core is a function library that provides the features typically found in a system kernel, but with an
abstraction layer that allows it to work on multiple platforms.  It also features an extensive object oriented
programming interface.

The portability of the core has been safe-guarded by keeping the functions as generalised as possible for potential host
environments.  It is vital that when writing application code for a target platform, the temptation to use the host's
functions are avoided.  Making direct calls to the host platform will lower the level of compatibility with other
platforms that are supported by Parasol.

For summarised information about how the system works, please refer to the introductory manuals which cover all aspects
of the design and object orientation in the system.  All of the information provided in this manual is technical and
intended for reference.

-END-

*****************************************************************************/

#define PRV_CORE
#define PRV_CORE_MODULE

#ifdef __CYGWIN__
#undef __unix__
#endif

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>

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

#ifdef DEBUG // KMSG() prints straight to stderr without going through the log.
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

extern "C" ERROR add_archive_class(void);
extern "C" ERROR add_compressed_stream_class(void);
extern "C" ERROR add_compression_class(void);
extern "C" ERROR add_script_class(void);
extern "C" ERROR add_task_class(void);
extern "C" ERROR add_thread_class(void);
extern "C" ERROR add_module_class(void);
extern "C" ERROR add_config_class(void);
extern "C" ERROR add_time_class(void);
#ifdef __ANDROID__
extern "C" ERROR add_asset_class(void);
#endif
extern "C" ERROR add_file_class(void);
extern "C" ERROR add_storage_class(void);

LONG InitCore(void);
extern "C" EXPORT void CloseCore(void);
extern "C" EXPORT struct CoreBase * OpenCore(OpenInfo *);
static ERROR open_shared_control(BYTE);
static ERROR init_shared_control(void);
static ERROR load_modules(void);
static ERROR init_filesystem(void);

#ifdef _WIN32
static WINHANDLE glSharedControlID = 0; // Shared memory ID.
#endif

#if defined(__unix__) && !defined(__ANDROID__)
static LONG glSharedControlID = -1;
#endif

#ifdef _WIN32
#define DLLCALL // __declspec(dllimport)
#define WINAPI  __stdcall
#define HKEY_LOCAL_MACHINE 0x80000002
#define KEY_READ 0x20019
DLLCALL LONG WINAPI RegOpenKeyExA(LONG,STRING,LONG,LONG,APTR *);
DLLCALL LONG WINAPI RegQueryValueExA(APTR,STRING,LONG *,LONG *,BYTE *,LONG *);
DLLCALL void WINAPI CloseHandle(APTR);
#endif

static TIMER glProcessJanitor = 0;
static CSTRING glUserHomeFolder = NULL;

static void print_class_list(void) __attribute__ ((unused));
static void print_class_list(void)
{
   parasol::Log log("Class List");
   char buffer[1024];
   size_t pos = 0;
   LONG *offsets = CL_OFFSETS(glClassDB);
   for (WORD i=0; i < glClassDB->Total; i++) {
      ClassItem *item = (ClassItem *)(((BYTE *)glClassDB) + offsets[i]);
      for (WORD j=0; (item->Name[j]) AND (pos < sizeof(buffer)-2); j++) buffer[pos++] = item->Name[j];
      if ((i < glClassDB->Total-1) AND (pos < sizeof(buffer)-1)) buffer[pos++] = ' ';
   }
   buffer[pos] = 0;
   log.trace("Total: %d, %s", glClassDB->Total, buffer);
}

/*****************************************************************************
** The _init symbol is used by dlopen() to initialise libraries.
*/

#ifdef __unix__
/*
void _init(void)
{
   //InitCore();
}
*/
#endif

//****************************************************************************

EXPORT struct CoreBase * OpenCore(OpenInfo *Info)
{
   #ifdef __unix__
      struct timeval tmday;
      struct timezone tz;
      #ifndef __ANDROID__
         BYTE hold_priority;
      #endif
   #endif
   rkTask *localtask;
   LONG i;
   OBJECTPTR SystemTask;
   ERROR error;
   UBYTE solo;

   if (!Info) return NULL;
   if (Info->Flags & OPF_ERROR) Info->Error = ERR_Failed;
   glOpenInfo = Info;
   tlMainThread = TRUE;
   glCodeIndex = 0; // Reset the code index so that CloseCore() will work.

   if (glProcessID) {
      fprintf(stderr, "Core module has already been initialised (OpenCore() called more than once.)\n");
   }

   if (alloc_private_lock(TL_GENERIC, 0)) return NULL; // A misc. internal mutex, strictly not recursive.
   if (alloc_private_lock(TL_TIMER, 0)) return NULL; // For timer subscriptions.
   if (alloc_private_lock(TL_MSGHANDLER, ALF_RECURSIVE)) return NULL;
   if (alloc_private_lock(TL_MEMORY_PAGES, 0)) return NULL; // For controlling access to glMemoryPages
   if (alloc_private_lock(TL_PRINT, 0)) return NULL; // For message logging only.
   if (alloc_private_lock(TL_THREADPOOL, 0)) return NULL;
   if (alloc_private_lock(TL_OBJECT_LOOKUP, ALF_RECURSIVE)) return NULL;
   if (alloc_private_lock(TL_PRIVATE_MEM, ALF_RECURSIVE)) return NULL;
   if (alloc_private_cond(CN_PRIVATE_MEM, 0)) return NULL;

   if (alloc_private_lock(TL_PRIVATE_OBJECTS, ALF_RECURSIVE)) return NULL;
   if (alloc_private_cond(CN_OBJECTS, 0)) return NULL;

   // Allocate a private POSIX semaphore for AccessPrivateObject() and ReleasePrivateObject()

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

   hold_priority = FALSE;

   // If the user has given us suid-root rights, we can use direct video access.  The first thing that we're going to
   // do here is actually drop this level of access so that we do not put system security at risk.  This is also
   // important for ensuring any created files have the user's original login and group id.  We can always regain root
   // access later when we need it for the video setup.
   //
   // Any area of code that needs to regain admin access simply uses the following code segment:
   //
   //    if (SetResource(RES_PRIVILEGEDUSER, TRUE) IS ERR_Okay) {
   //       ...admin access...
   //       SetResource(RES_PRIVILEGEDUSER, FALSE);
   //    }

   seteuid(glUID);  // Ensure that the rest of our code is run under the real user name instead of admin
   setegid(glGID);  // Ensure that we run under the user's default group (important for file creation)

   // Test if this is a genuine OS installation

   if (!access("/etc/sysconfig/.parasol", F_OK)) glFullOS = TRUE;

#elif _WIN32
   // Make some Windows calls and generate the global share names.  The share names are based on the library location -
   // this is very important for preventing multiple installs and versions from clashing with each other (e.g. a USB
   // stick installation clashing with a base installation on C: drive)
   //
   // NOTE: A globally recognisable name only matters when a global instance is active for resource sharing.  Otherwise
   // the names will be rewritten so that they are unique (see open_shared_control())

   LONG id = 0;
   #ifdef DEBUG
      winInitialise(&id, NULL); // Don't set a break handler, this will allow GDB intercept CTRL-C.
   #else
      winInitialise(&id, (APTR)&BreakHandler);
   #endif

   const char lookup[] = "0123456789ABCDEF";
   WORD p;
   for (p=1; p < PL_END; p++) {
      glPublicLocks[p].Name[0] = 'r';
      glPublicLocks[p].Name[1] = 'k';
      glPublicLocks[p].Name[2] = 'a' + p;
      for (i=3; i < 11; i++) {
         glPublicLocks[p].Name[i] = lookup[id & 0xf];
         glPublicLocks[p].Name[i] = lookup[id & 0xf];
         glPublicLocks[p].Name[i] = lookup[id & 0xf];
         glPublicLocks[p].Name[i] = lookup[id & 0xf];
         id = id>>4;
      }
      glPublicLocks[p].Name[i] = 0;
   }

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

   ClearMemory(glAlphaNumeric, sizeof(glAlphaNumeric));
   for (i='a'; i <= 'z'; i++) glAlphaNumeric[i] = TRUE;
   for (i='A'; i <= 'Z'; i++) glAlphaNumeric[i] = TRUE;
   for (i='0'; i <= '9'; i++) glAlphaNumeric[i] = TRUE;

   if (Info->Flags & OPF_ROOT_PATH) SetResourcePath(RP_ROOT_PATH, Info->RootPath);

   if (Info->Flags & OPF_MODULE_PATH) SetResourcePath(RP_MODULE_PATH, Info->ModulePath);

   if (Info->Flags & OPF_SYSTEM_PATH) SetResourcePath(RP_SYSTEM_PATH, Info->SystemPath);

   if (!glRootPath[0])   {
      #ifdef _WIN32
         glRootPath[0] = 0;

         LONG len;
         if (winGetExeDirectory(sizeof(glRootPath), glRootPath)) {
            len = StrLength(glRootPath);
            while ((len > 1) AND (glRootPath[len-1] != '/') AND (glRootPath[len-1] != '\\') AND (glRootPath[len-1] != ':')) len--;
            glRootPath[len] = 0;
         }
         else if ((!winGetCurrentDirectory(sizeof(glRootPath), glRootPath))) {
            fprintf(stderr, "Failed to determine root folder.\n");
            return NULL;
         }
      #else
         // Get the folder of the running process.
         char procfile[50];
         snprintf(procfile, sizeof(procfile), "/proc/%d/exe", getpid());

         LONG len;
         if ((len = readlink(procfile, glRootPath, sizeof(glRootPath)-1)) > 0) {
            while (len > 0) { // Strip the process name
               if (glRootPath[len-1] IS '/') break;
               len--;
            }
            glRootPath[len] = 0;

            // If the binary is in a 'bin' folder then the root is considered to be the parent folder.
            if (!StrCompare("bin/", glRootPath+len-4, 4, 0)) {
               glRootPath[len-4] = 0;
            }
        }
      #endif
   }

   if (!glSystemPath[0]) {
      // When no system path is specified then treat the install as 'run-anywhere' so that "parasol:" == "system:"
      StrCopy(glRootPath, glSystemPath, sizeof(glSystemPath));
   }

   // Process the Information structure

   if (Info->Flags & OPF_CORE_VERSION) {
      if (Info->CoreVersion > VER_CORE) {
         KMSG("This program requires version %.1f of the Parasol Core.\n", Info->CoreVersion);
         if (Info->Flags & OPF_ERROR) Info->Error = ERR_CoreVersion;
         return NULL;
      }
   }

   // Debug processing

   if (Info->Flags & OPF_DETAIL)  glLogLevel = (WORD)Info->Detail;
   if (Info->Flags & OPF_MAX_DEPTH) glMaxDepth = (WORD)Info->MaxDepth;

   if (Info->Flags & (OPF_SHOW_PUBLIC_MEM|OPF_SHOW_MEMORY)) glShowPublic = TRUE;
   if (Info->Flags & OPF_SHOW_MEMORY) glShowPrivate = TRUE;

   // Android sets an important JNI pointer on initialisation.

   if ((Info->Flags & OPF_OPTIONS) AND (Info->Options)) {
      for (LONG i=0; Info->Options[i].Tag != TAGEND; i++) {
         switch (Info->Options[i].Tag) {
            case TOI_ANDROID_ENV: {
               glJNIEnv = Info->Options[i].Value.Pointer;
               break;
            }
         }
      }
   }

   // Check if the privileged flag has been set, which means "don't drop administration privileges if my binary is known to be suid".

#if defined(__unix__) && !defined(__ANDROID__)
   if ((Info->Flags & OPF_PRIVILEGED) AND (geteuid() != getuid())) {
      glPrivileged = TRUE;
   }
#endif

   // Process arguments
   //
   // --solo:
   //   Wait for all other running tasks to stop (unload the main semaphore).  Once this has occurred, continue the Core
   //   initialisation.  Useful for relaunching the system from a new disk location with new glSemaphore stuff - module
   //   references etc.

   CSTRING newargs[Info->ArgCount];
   LONG na = 0;
   solo = FALSE;
   if (Info->Flags & OPF_ARGS) {
      for (i=1; i < Info->ArgCount; i++) {
         if (!StrMatch(Info->Args[i], "--log-memory")) {
            glShowPrivate = TRUE;
            glShowPublic  = TRUE;
            glDebugMemory = TRUE;
         }
         else if (!StrMatch(Info->Args[i], "--log-shared-memory")) {
            glShowPublic = TRUE;
         }
         else if (!StrMatch(Info->Args[i], "--instance")) {
            if (i < Info->ArgCount-1) {
               glInstanceID = StrToInt(Info->Args[i+1]);
               i++;
            }
         }
         else if (!StrCompare(Info->Args[i], "--gfx-driver=", 13, 0)) {
            StrCopy(Info->Args[i]+13, glDisplayDriver, sizeof(glDisplayDriver));
         }
         else if (!StrMatch(Info->Args[i], "--global"))      Info->Flags |= OPF_GLOBAL_INSTANCE;
         else if (!StrMatch(Info->Args[i], "--solo"))        solo = TRUE;
         else if (!StrMatch(Info->Args[i], "--sync"))        glSync = TRUE;
         else if (!StrMatch(Info->Args[i], "--log-none"))    glLogLevel = 0;
         else if (!StrMatch(Info->Args[i], "--log-error"))   glLogLevel = 1;
         else if (!StrMatch(Info->Args[i], "--log-warn"))    glLogLevel = 2;
         else if (!StrMatch(Info->Args[i], "--log-warning")) glLogLevel = 2;
         else if (!StrMatch(Info->Args[i], "--log-info"))    glLogLevel = 4; // Levels 3/4 are for applications (no internal detail)
         else if (!StrMatch(Info->Args[i], "--log-api"))     glLogLevel = 5; // Default level for API messages
         else if (!StrMatch(Info->Args[i], "--log-extapi"))  glLogLevel = 6;
         else if (!StrMatch(Info->Args[i], "--log-debug"))   glLogLevel = 7;
         else if (!StrMatch(Info->Args[i], "--log-all"))     glLogLevel = 9; // 9 is the absolute maximum
         else if (!StrMatch(Info->Args[i], "--time"))        glTimeLog = PreciseTime();
         #if defined(__unix__) && !defined(__ANDROID__)
         else if (!StrMatch(Info->Args[i], "--holdpriority")) hold_priority = TRUE;
         #endif
         // Define the user: volume.  If the user folder does not exist then we create it so that the volume can be made.
         //
         // Note: In the volumes config file, 'user' is a special section that can define a name for the user folder other
         // than 'ParasolXX'.  This is useful for customised distributions.  If the folder is named as 'default' then no
         // user-specific folder is assigned.
         else if (!StrCompare("--home=", Info->Args[i], 7, 0)) glUserHomeFolder = Info->Args[i] + 7;
         else newargs[na++] = Info->Args[i];
      }

      if (glLogLevel > 2) {
         char cmdline[160];
         size_t pos = 0;
         for (LONG i=0; (i < Info->ArgCount) AND (pos < sizeof(cmdline)-1); i++) {
            if (i > 0) cmdline[pos++] = ' ';
            for (LONG j=0; (Info->Args[i][j]) AND (pos < sizeof(cmdline)-1); j++) {
               cmdline[pos++] = Info->Args[i][j];
            }
         }
         cmdline[pos] = 0;
         KMSG("Parameters: %s\n", cmdline);
      }
   }

   glShowIO = (Info->Flags & OPF_SHOW_IO) ? TRUE : FALSE;

#if defined(__unix__) && !defined(__ANDROID__)
   // It is possible to write so much data to the terminal that the process can sleep on a printf().  If this
   // happens while the process has important system resources locked, dead-locking can occur between the task and
   // the terminal.  E.g. A process hangs on printf() while RPM_SurfaceList is locked; ZTerm tries to access
   // RPM_SurfaceList and is dead-locked because the task cannot return until ZTerm clears the stdout buffer.
   //
   // A non-blocking stdout solves this problem at the cost of dropping output.  There would be better
   // solutions, but this is a simple one.

   //if (glSync IS FALSE) {
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
   // Subscribe to the following signals so that we can handle system crashes correctly.

   struct sigaction sig;

   sig.sa_flags = SA_SIGINFO;
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

   parasol::Log log("Core");

#ifdef _WIN32

   activate_console(glLogLevel > 0); // This works for the MinGW runtime libraries but not MSYS2

   // If the log detail is less than 3, use an exception handler to deal with crashes.  Otherwise, do not set the
   // handler because the developer may want to intercept the crash using a debugger.

   #ifndef DEBUG
   if (glLogLevel <= 5) winSetUnhandledExceptionFilter(&CrashHandler);
   #endif

   if (!(Info->Flags & OPF_GLOBAL_INSTANCE)) {
      // This process isn't a global instance, so check if there's an existing global process that we can attach to.
      // If not, generate resource names that are unique to this process and limit resource access so that they are
      // only available to child processes.

      WINHANDLE handle;
      UBYTE standalone = TRUE;

      if (!open_public_lock(&handle, glPublicLocks[PL_FORBID].Name)) { // Existing mutex resource is accessible
         free_public_lock(handle);

         if (!open_public_waitlock(&handle, glPublicLocks[CN_SEMAPHORES].Name)) { // Existing event resource is accessible
            free_public_waitlock(handle);
            log.trace("This process will attach to an existing global process.");
            standalone = FALSE;
         }
         else log.trace("Unable to access all parent process' locks, will run standalone");
      }

      if (standalone) { // Process will run standalone - generate unique resource names based on our process ID.
         LONG id = glProcessID;
         static const char lookup[] = "0123456789ABCDEF";
         while (1) {
            WORD p, i;
            for (p=1; p < PL_END; p++) {
               LONG mid = id;
               for (i=3; i < 11; i++) {
                  glPublicLocks[p].Name[i] = lookup[mid & 0xf];
                  mid = mid>>4;
               }
            }

            // Check that the resource name is unique, otherwise keep looping.
            if (open_public_lock(&handle, glPublicLocks[PL_FORBID].Name) != ERR_Okay) break;

            free_public_lock(handle);
            id += 17; // Alter the id with a prime number
         }
      }
   }

   // If going solo, we need to wait for all other programs using this install to release the shared control semaphore

   if (solo) {
      log.trace("Process will only run solo - will wait if other processes are running.");
      while (1) {
         WINHANDLE handle;
         if (!open_public_lock(&handle, glPublicLocks[PL_FORBID].Name)) {
            free_public_lock(handle);
            winSleep(20);
         }
         else break;
      }
   }
#endif

   // Shared memory set-up

   if (open_shared_control((Info->Flags & OPF_GLOBAL_INSTANCE) ? TRUE : FALSE) != ERR_Okay) {
      CloseCore();
      if (Info->Flags & OPF_ERROR) Info->Error = ERR_Failed;
      return NULL;
   }

   // Determine shared addresses

   glSharedBlocks = (PublicAddress *)ResolveAddress(glSharedControl, glSharedControl->BlocksOffset);
   shSemaphores   = (SemaphoreEntry *)ResolveAddress(glSharedControl, glSharedControl->SemaphoreOffset);
   shTasks        = (TaskList *)ResolveAddress(glSharedControl, glSharedControl->TaskOffset);

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
                  if (Info->Flags & OPF_ERROR) Info->Error = ERR_SystemCall;
                  return NULL;
               }
            }
            else {
               if (Info->Flags & OPF_ERROR) Info->Error = ERR_SystemCall;
               return NULL;
            }
         }
      }
      else {
         KERR("Failed to create a new socket communication point.\n");
         if (Info->Flags & OPF_ERROR) Info->Error = ERR_SystemCall;
         return NULL;
      }

      RegisterFD(glSocket, RFD_READ, NULL, NULL);
   #endif

   if (!SysLock(PL_FORBID, 4000)) {
      if (glSharedControl->GlobalInstance) {
         if (Info->Flags & OPF_GLOBAL_INSTANCE) {
            // If a global instance is already active and OPF_GLOBAL_INSTANCE is set, we cannot proceed.

            SysUnlock(PL_FORBID);
            CloseCore();
            if (Info->Flags & OPF_ERROR) Info->Error = ERR_GlobalInstanceLocked;
            return NULL;
         }
         else {
            // When a global instance is active, all tasks must relate to the same instance ID.

            glInstanceID = glSharedControl->GlobalInstance;
            glMasterTask = FALSE;
         }
      }

      // Register our process in the task control array.  First, check if a slot has been pre-allocated for our
      // task by the parent that launched us.  Otherwise, allocate a new slot.

      for (i=0; i < MAX_TASKS; i++) {
         if (shTasks[i].ProcessID IS glProcessID) {
            // A slot has been pre-allocated by a parent process that launched us
            glInstanceID = shTasks[i].InstanceID;
            break;
         }
      }

      if (i IS MAX_TASKS) {
         for (i=0; (i < MAX_TASKS) AND (shTasks[i].ProcessID); i++); // Find an empty slot

         // If all slots are in use, check if there are any dead slots (pre-allocated slots that haven't been assigned
         // to a process due to execution errors or whatever).

         if (i IS MAX_TASKS) {
            for (i=0; i < MAX_TASKS; i++) {
               if ((shTasks[i].ProcessID)) { // AND ((PreciseTime()/1000LL) - shTasks[i].CreationTime > 1000)) {
                  if ((!shTasks[i].TaskID))  {
                     ClearMemory(shTasks + i, sizeof(shTasks[0]));
                     break;
                  }
                  else {
                     #ifdef _WIN32
                        // For some reason this doesn't work as expected?  We use CheckMemory() as backup...

                        if (winCheckProcessExists(shTasks[i].ProcessID) IS FALSE) {
                           KMSG("Core: Found dead process slot (index %d, process %d)\n", i, shTasks[i].ProcessID);
                           ClearMemory(shTasks + i, sizeof(shTasks[0]));
                           break;
                        }
                        else if (CheckMemoryExists(shTasks[i].MessageID) != ERR_Okay) {
                           KMSG("Core: Found dead message queue (index %d, process %d)\n", i, shTasks[i].ProcessID);
                           ClearMemory(shTasks + i, sizeof(shTasks[0]));
                           break;
                        }
                     #else
                        if ((kill(shTasks[i].ProcessID, 0) IS -1) AND (errno IS ESRCH)) {
                           ClearMemory(shTasks + i, sizeof(shTasks[0]));
                           break;
                        }
                     #endif
                  }
               }
            }
         }
      }

      if (i < MAX_TASKS) {
         shTasks[i].ProcessID    = glProcessID;
         shTasks[i].InstanceID   = glInstanceID;
         #ifdef _WIN32
            shTasks[i].Lock = get_threadlock(); // The main semaphore (for waking the message queue)
         #endif
         shTasks[i].Index        = i;
         shTasks[i].CreationTime = (PreciseTime()/1000LL);
         glTaskEntry = shTasks + i;
      }
      else {
         KMSG("Core: The system has reached its limit of %d processes.\n", MAX_TASKS);
/*
         for (i=0; i < MAX_TASKS; i++) {
            KMSG("Core: %d: Instance: %d, Process: %d, Created: %d", i, shTasks[i].InstanceID, shTasks[i].ProcessID, (LONG)shTasks[i].CreationTime);
         }
*/
         if (Info->Flags & OPF_ERROR) Info->Error = ERR_ArrayFull;
         SysUnlock(PL_FORBID);
         CloseCore();
         return NULL;
      }

      // Use a process ID as a unique instance ID by default

      if (!glInstanceID) {
         glMasterTask = TRUE;
         if ((glInstanceID = glProcessID) < 0) glInstanceID = -glInstanceID;

         if (Info->Flags & OPF_GLOBAL_INSTANCE) {
            glSharedControl->GlobalInstance = glInstanceID;
         }

         glSharedControl->InstanceMsgPort = glTaskMessageMID;

         FUNCTION call;
         SET_FUNCTION_STDC(call, (APTR)&process_janitor);
         SubscribeTimer(60, &call, &glProcessJanitor);
      }
      else {
         glMasterTask = FALSE;

         #ifdef _WIN32
            // In win32, child tasks need to run at a lower priority than the core task
            // in order to prevent them sucking up the core task's time.  (This applies
            // at least to Win2K and earlier).

            winLowerPriority();
         #endif
      }

      SysUnlock(PL_FORBID);
   }
   else {
      if (Info->Flags & OPF_ERROR) Info->Error = ERR_LockFailed;
      CloseCore();
      return NULL;
   }

   // Print task information

   log.msg("Version: %.1f : Process: %d, Instance: %d, MemPool Address: %p", VER_CORE, glProcessID, glInstanceID, glSharedControl);
   log.msg("Blocks Used: %d, MaxBlocks: %d, Sync: %s, Root: %s", glSharedControl->BlocksUsed, glSharedControl->MaxBlocks, (glSync) ? "Y" : "N", glRootPath);
#ifdef __unix__
   log.msg("UID: %d (%d), EUID: %d (%d); GID: %d (%d), EGID: %d (%d)", getuid(), glUID, geteuid(), glEUID, getgid(), glGID, getegid(), glEGID);
#endif
   log.msg("Public Offsets: %d, %d, %d", glSharedControl->BlocksOffset, glSharedControl->SemaphoreOffset, glSharedControl->TaskOffset);

   // Private memory block handling

   glMemRegSize = 1000;
   log.msg("Allocating an initial private memory register of %d entries.", glMemRegSize);
   if (!(glPrivateMemory = (PrivateAddress *)malloc(sizeof(PrivateAddress) * glMemRegSize))) {
      if (Info->Flags & OPF_ERROR) Info->Error = ERR_AllocMemory;
      CloseCore();
      return NULL;
   }

   // Allocate the page management table for public memory blocks.

   glTotalPages = PAGE_TABLE_CHUNK;
   if (!(glMemoryPages = (MemoryPage *)calloc(glTotalPages, sizeof(MemoryPage)))) {
      log.warning("Failed to allocate the memory page management table.");
      CloseCore();
      if (Info->Flags & OPF_ERROR) Info->Error = ERR_AllocMemory;
      return NULL;
   }

   // Allocate the public object table

   {
      SharedObjectHeader *publichdr;

      log.msg("Allocating public object table.");

      MEMORYID memid = RPM_SharedObjects;
      if (!(error = AllocMemory(sizeof(SharedObjectHeader) + (sizeof(SharedObject) * PUBLIC_TABLE_CHUNK), MEM_UNTRACKED|MEM_RESERVED|MEM_PUBLIC|MEM_READ_WRITE, (void **)&publichdr, &memid))) {
         publichdr->Offset    = sizeof(SharedObjectHeader);
         publichdr->NextEntry = 0;
         publichdr->ArraySize = PUBLIC_TABLE_CHUNK;
         ReleaseMemoryID(memid);
      }
      else if (error != ERR_ResourceExists) {
         log.warning("Failed to allocate the public object memory table, error %d.", error);
         if (Info->Flags & OPF_ERROR) Info->Error = ERR_AllocMemory;
         CloseCore();
         return NULL;
      }
   }

   if (AllocSemaphore(NULL, 0, 0, &glSharedControl->ClassSemaphore)) {
      CloseCore();
      return NULL;
   }

   // Local action management routines

   ManageAction(AC_Init, (APTR)MGR_Init);
   ManageAction(AC_Free, (APTR)MGR_Free);
   ManageAction(AC_Rename, (APTR)MGR_Rename);
   ManageAction(AC_Seek, (APTR)MGR_Seek);

   if (!(glClassMap = VarNew(0, KSF_THREAD_SAFE))) {
      fprintf(stderr, "Failed to allocate glClassMap.\n");
      CloseCore();
      return NULL;
   }

   if (!(glObjectLookup = VarNew(0, 0))) {
      fprintf(stderr, "Failed to allocate glObjectLookup.\n");
      CloseCore();
      return NULL;
   }

   if (add_task_class() != ERR_Okay) {
      fprintf(stderr, "Failed call to add_task_class().\n");
      CloseCore();
      return NULL;
   }

   // Allocate the System Task

   if (!(error = NewLockedObject(ID_TASK, NF_NO_TRACK|NF_PUBLIC|NF_UNIQUE, &SystemTask, &SystemTaskID, "SystemTask"))) {
      if (Action(AC_Init, SystemTask, NULL) != ERR_Okay) {
         if (Info->Flags & OPF_ERROR) Info->Error = ERR_Init;
         ReleaseObject(SystemTask);
         CloseCore();
         return NULL;
      }
      ReleaseObject(SystemTask);
   }
   else if (error != ERR_ObjectExists) {
      if (Info->Flags & OPF_ERROR) Info->Error = ERR_NewObject;
      CloseCore();
      return NULL;
   }

   // Register Core classes in the system

   {
      parasol::Log log("Core");
      log.branch("Registering Core classes.");

      if (add_thread_class() != ERR_Okay)  { CloseCore(); return NULL; }
      if (add_module_class() != ERR_Okay)  { CloseCore(); return NULL; }
      if (add_time_class() != ERR_Okay)    { CloseCore(); return NULL; }
      if (add_config_class() != ERR_Okay)  { CloseCore(); return NULL; }
      if (add_storage_class() != ERR_Okay) { CloseCore(); return NULL; }
      if (add_file_class() != ERR_Okay)    { CloseCore(); return NULL; }
      if (add_script_class() != ERR_Okay)  { CloseCore(); return NULL; }
      if (add_archive_class() != ERR_Okay) { CloseCore(); return NULL; }
      if (add_compressed_stream_class() != ERR_Okay) { CloseCore(); return NULL; }
      if (add_compression_class() != ERR_Okay) { CloseCore(); return NULL; }

      #ifdef __ANDROID__
      if (add_asset_class() != ERR_Okay) { CloseCore(); return NULL; }
      #endif
   }

   if (init_filesystem()) {
      KERR("Failed to initialise the filesystem.");
      CloseCore();
      return NULL;
   }

   fs_initialised = TRUE;

   if (!(Info->Flags & OPF_SCAN_MODULES)) {
      if (!(error = load_modules())) {
         error = load_classes(); // See class_metaclass.c
         Expunge(FALSE); // A final expunge is essential to normalise the system state.
      }
   }

   if (error != ERR_Okay) {
      log.warning("Failed to load the system classes.");
      if (Info->Flags & OPF_ERROR) Info->Error = error;
      CloseCore();
      return NULL;
   }

   // Create our task object.  This is expected to be local to our process, so do not allocate this object publicly.

   if (!NewPrivateObject(ID_TASK, NF_NO_TRACK, (OBJECTPTR *)&localtask)) {
      localtask->Flags |= TSF_DUMMY;

      if (Info->Flags & OPF_NAME) {
         SetField((OBJECTPTR)localtask, FID_Name|TSTRING, Info->Name);
         StrCopy(Info->Name, glProgName, sizeof(glProgName));
      }

      if (Info->Flags & OPF_AUTHOR)    SetField((OBJECTPTR)localtask, FID_Author|TSTR, Info->Author);
      if (Info->Flags & OPF_COPYRIGHT) SetField((OBJECTPTR)localtask, FID_Copyright|TSTR, Info->Copyright);
      if (Info->Flags & OPF_DATE)      SetField((OBJECTPTR)localtask, FID_Date|TSTR, Info->Date);

      if (!acInit(&localtask->Head)) {
         // NB: The glCurrentTask and glCurrentTaskID variables are set on task initialisation

         if (na > 0) SetArray(&localtask->Head, FID_Parameters, newargs, na);

         if (!acActivate(&localtask->Head)) {

         }
         else {
            CloseCore();
            return NULL;
         }
      }
      else {
         CloseCore();
         return NULL;
      }
   }
   else {
      CloseCore();
      return NULL;
   }

   // In Windows, set the PATH environment variable so that DLL's installed under modules:lib can be found.

#ifdef _WIN32
   {
      STRING libpath;
      if (!ResolvePath("modules:lib", RSF_NO_FILE_CHECK, &libpath)) {
         winSetDllDirectory(libpath);
         FreeResource(libpath);
      }
      else log.trace("Failed to resolve modules:lib");
   }
#endif

   // Generate the Core table for our new task

   if (!(Info->Flags & OPF_JUMPTABLE)) Info->JumpTable = MHF_STRUCTURE;

   LocalCoreBase = (struct CoreBase *)build_jump_table(Info->JumpTable, glFunctions, MEM_UNTRACKED);
   if (Info->Flags & OPF_COMPILED_AGAINST) fix_core_table(LocalCoreBase, Info->CompiledAgainst);

   // Broadcast the creation of the new task

   evTaskCreated task_created = { EVID_SYSTEM_TASK_CREATED, glCurrentTaskID };
   BroadcastEvent(&task_created, sizeof(task_created));

   if (Info->Flags & OPF_SCAN_MODULES) {
      log.msg("Class scanning has been enforced by user request.");
      glScanClasses = TRUE;
   }

   if (glScanClasses) {
      scan_classes();
   }

   #ifdef DEBUG
      print_class_list();
   #endif

   log.msg("PROGRAM OPENED");

   glSharedControl->SystemState = 0; // Indicates that initialisation is complete.
   if (Info->Flags & OPF_ERROR) Info->Error = ERR_Okay;

   return LocalCoreBase;
}

//****************************************************************************
// Performs a system cleanup by expunging unused memory, modules etc.

EXPORT void CleanSystem(LONG Flags)
{
   parasol::Log log("Core");
   log.msg("Flags: $%.8x", Flags);
   Expunge(FALSE);
}

/*****************************************************************************
** If GlobalInstance is TRUE (because OPF_GLOBALINSTANCE was specified to OpenCore()), then the permissions and
** nature of the public memory can be quite different to that of the standalone runtime environment.
**
** Basically, a global instance uses loose shared memory permissions and a bigger shared heap because apps will be
** sharing more information and using common objects as audio and display surfaces.  OTOH a standalone runtime only
** needs to concern itself with sharing information with the child processes that it creates (if any).
*/

#define MAGICKEY 0x58392712

static LONG glMemorySize = sizeof(SharedControl) + (sizeof(PublicAddress) * MAX_BLOCKS) +
                           (sizeof(SemaphoreEntry) * MAX_SEMAPHORES) +
                           (sizeof(WaitLock) * MAX_WAITLOCKS) +
                           (sizeof(TaskList) * MAX_TASKS);

#ifdef _WIN32
static ERROR open_shared_control(BYTE GlobalInstance)
{
   parasol::Log log(__FUNCTION__);

   log.trace("Global: %d", GlobalInstance);

   ERROR error;

   for (LONG i=1; i < PL_END; i++) {
      if (glPublicLocks[i].Event) {
         if ((error = alloc_public_waitlock(&glPublicLocks[i].Lock, glPublicLocks[i].Name))) { // Event
            log.warning("Failed to allocate system waitlock %d '%s', error %d", i, glPublicLocks[i].Name, error);
            return error;
         }
      }
      else if ((error = alloc_public_lock(&glPublicLocks[i].Lock, glPublicLocks[i].Name))) { // Mutex
         log.warning("Failed to allocate system lock, error %d", error);
         return error;
      }
   }

   // Allocate the public memory pool

   if (LOCK_PUBLIC_MEMORY(4000) != ERR_Okay) {
      KERR("Failure in call to LOCK_PUBLIC_MEMORY().\n");
      return ERR_LockFailed;
   }

   glPageSize = 64; // byte size of the page alignment.  In Windows, public memory is in a pool, so we can set whatever alignment we want

   LONG init;
   char sharename[12];
   CopyMemory(glPublicLocks[PL_FORBID].Name, sharename, 12);
   sharename[2] = 'z';
   if ((init = winCreateSharedMemory(sharename, glMemorySize, glMemorySize + INITIAL_PUBLIC_SIZE, &glSharedControlID, (APTR *)&glSharedControl)) < 0) {
      KERR("Failed to create the shared memory pool in call to winCreateSharedMemory(), code %d.\n", init);
      UNLOCK_PUBLIC_MEMORY();
      return ERR_Failed;
   }

   if (init) error = init_shared_control();
   else error = ERR_Okay;

   UNLOCK_PUBLIC_MEMORY();
   return error;
}

#else

static ERROR open_shared_control(BYTE GlobalInstance)
{
   parasol::Log log("Core");

   // Generate a resource sharing key based on the unique file inode of the core library.  This prevents
   // conflicts with other installations of the system core on this machine.

   LONG memkey = 0xd39f7ea1;

   Dl_info dlinfo;
   if (dladdr((CPTR)OpenCore, &dlinfo)) {
      struct stat flinfo;
      if (!stat(dlinfo.dli_fname, &flinfo)) {
         memkey ^= flinfo.st_ino;
      }
   }

   KMSG("open_shared_control(%d) Key: $%.8x.\n", GlobalInstance, memkey);

   // Allocate the public memory pool

   #ifdef __ANDROID__
      // Helpful ashmem article: http://notjustburritos.tumblr.com/post/21442138796/an-introduction-to-android-shared-memory

      // There are two ways to go about managing shared memory: Allocate each request independently as a system memory block
      // or allocate a memory pool and manage it with our own functionality.

      if ((glMemoryFD = open("/dev/ashmem", O_RDWR, S_IRWXO|S_IRWXG|S_IRWXU)) IS -1) {
         LOGE("Failed to open /dev/ashmem");
         return ERR_Failed;
      }

      char ashname[32];
      snprintf(ashname, sizeof(ashname), "rkl_%d", memkey); // Use the memkey as the system-wide name.  This means that APK based installs are secluded, but a system installation of the core would be discoverable by everyone.

      LONG ret = ioctl(glMemoryFD, ASHMEM_SET_NAME, ashname);
      if (ret < 0) {
         LOGE("Failed to set name of the ashmem region that we want.");
         close(glMemoryFD);
         glMemoryFD = -1;
         return ERR_Failed;
      }

      ret = ioctl(glMemoryFD, ASHMEM_SET_SIZE, glMemorySize + INITIAL_PUBLIC_SIZE);
      if (ret < 0) {
         LOGE("Failed to set size of the ashmem region to %d.", glMemorySize + INITIAL_PUBLIC_SIZE);
         close(glMemoryFD);
         glMemoryFD = -1;
         return ERR_Failed;
      }

      // Map the first portion of the file into our process (i.e. the system stuff that needs to be permanently mapped).
      // Mapping of blocks in the global pool will only be done on request in AccessMemory() and page_memory().

      if ((glSharedControl = mmap(0, glMemorySize, PROT_READ|PROT_WRITE, MAP_SHARED, glMemoryFD, 0)) != (APTR)-1) {
         // Call init_shared_control() if this is the first time that the block has been created.

         if (glSharedControl->MagicKey != MAGICKEY) init_shared_control();
         else LOGI("Shared control structure already initialised.");
      }
      else {
         KERR("mmap() failed on public memory pool: %s.\n", strerror(errno));
         return ERR_LockFailed;
      }

   #elif USE_SHM
      // NOTE: Do not ever use shmctl(IPC_STAT) for any reason, as the resulting structure is not compatible between all Linux versions.

      BYTE init = FALSE;
      if ((glSharedControlID = shmget(memkey, 0, SHM_R|SHM_W)) IS -1) {
         KMSG("No existing memory block exists for the given key (will create a new one).  Error: %s\n", strerror(errno));

         if (!GlobalInstance) {
            // Since there's no global instance running and we've been told on initialisation not to be one, change the memkey with our PID to make it unique.
            memkey ^= getpid();
         }

         if ((glSharedControlID = shmget(memkey, glMemorySize, IPC_CREAT|IPC_EXCL|SHM_R|SHM_W|S_IRWXO|S_IRWXG|S_IRWXU)) IS -1) {
            if (errno IS EEXIST); // The shared memory block already exists
            else if (errno IS EIDRM) { // The shared memory block is marked for termination and some other process is still using it
               KERR("Cannot load the Core because an existing process has permanently locked the public memory pool.\nUse 'ps aux' to find the process and kill it.\n");
               return ERR_Failed;
            }
            else {
               KERR("\033[1mshmget() failed on the public memory pool, error %d: %s\033[0m\n", errno, strerror(errno));
               return ERR_Failed;
            }
         }
         else {
            init = TRUE;
         }
      }
      else KMSG("Shared memory block already exists - will attach to it.\n");

      if ((glSharedControl = (SharedControl *)shmat(glSharedControlID, 0, SHM_R|SHM_W)) IS (void *)-1) {
         glSharedControl = NULL;
         KERR("\033[1mFailed to connect to the public memory pool.\033[0m\n");
         return ERR_Failed;
      }

      // Initialise/Reset the memory if we created it, OR it can be determined that a crash has occurred on previous
      // execution and all shared memory allocations need to be destroyed.

      if ((init) OR (glSharedControl->MagicKey != MAGICKEY)) {
         KMSG("Initialisation of glSharedControl is required.\n");
         init_shared_control();
      }
      else if (glSharedControl->GlobalInstance) {
         KMSG("Checking existing glSharedControl is valid (instance PID %d).\n", glSharedControl->GlobalInstance);

         UBYTE cleanup = FALSE;

         if ((kill(glSharedControl->GlobalInstance, 0) IS -1) AND (errno IS ESRCH)) cleanup = TRUE;

         if (cleanup) {
            // The global instance no longer exists - this indicates that a crash occurred and the IPC's weren't
            // terminated correctly.

            log.warning("Cleaning up system failure detected for previous execution.");

            // Mark all previous shared memory blocks for deletion.
            // You can check the success of this routine by running "ipcs", which lists allocated shm blocks.

            LONG id;

            shSemaphores = (SemaphoreEntry *)ResolveAddress(glSharedControl, glSharedControl->SemaphoreOffset);
            shTasks      = (TaskList *)ResolveAddress(glSharedControl, glSharedControl->TaskOffset);

            if (glSharedControl->BlocksOffset) {
               glSharedBlocks = (PublicAddress *)ResolveAddress(glSharedControl, glSharedControl->BlocksOffset);
               for (LONG i=glSharedControl->NextBlock-1; i >= 0; i--) {
                  if (glSharedBlocks[i].MemoryID) {
                     if ((id = shmget(SHMKEY + glSharedBlocks[i].MemoryID, glSharedBlocks[i].Size, S_IRWXO|S_IRWXG|S_IRWXU)) != -1) {
                        shmctl(id, IPC_RMID, NULL);
                     }
                  }
               }
            }

            init_shared_control();
         }
      }

   #else
      struct stat info;

      KMSG("Using mmap() sharing functionality.\n");

      // Open the memory file, creating it if it does not exist.  If it does exist, the file is simply opened.

      if ((glMemoryFD = open(MEMORYFILE, O_RDWR|O_CREAT, S_IRWXO|S_IRWXG|S_IRWXU)) IS -1) {
         KERR("Failed to open memory base file \"%s\".\n", MEMORYFILE);
         return ERR_Failed;
      }

      // Expand the size of the page file if it is too small

      if (!fstat(glMemoryFD, &info)) {
         if (info.st_size < glMemorySize) {
            if (ftruncate(glMemoryFD, glMemorySize + INITIAL_PUBLIC_SIZE) IS -1) {
               KERR("Failed to increase the filesize of \"%s\" to %d bytes.\n", MEMORYFILE, glMemorySize + INITIAL_PUBLIC_SIZE);
               return ERR_Failed;
            }
         }
      }

      // Map the file into our process

      if ((glSharedControl = mmap(0, glMemorySize, PROT_READ|PROT_WRITE, MAP_SHARED, glMemoryFD, 0)) != (APTR)-1) {
         // Call init_shared_control() if this is the first time that the block has been created.

         if (glSharedControl->MagicKey != MAGICKEY) init_shared_control();
      }
      else {
         KERR("mmap() failed on public memory pool: %s.\n", strerror(errno));
         return ERR_LockFailed;
      }

   #endif

   glSharedControl->SystemState = -1; // System state of -1 == initialising

   return ERR_Okay;
}
#endif

static ERROR init_shared_control(void)
{
   KMSG("init_shared_control()\n");

   // Clear the entire shared control structure

   ClearMemory(glSharedControl, glMemorySize);

   glSharedControl->PoolSize         = INITIAL_PUBLIC_SIZE;
   glSharedControl->BlocksUsed       = 0;
   glSharedControl->IDCounter        = -10000;
   glSharedControl->PrivateIDCounter = 500;
   glSharedControl->MagicKey         = MAGICKEY;
   glSharedControl->MaxBlocks        = MAX_BLOCKS;
   glSharedControl->MessageIDCount   = 1;
   glSharedControl->ClassIDCount     = 1;
   glSharedControl->GlobalIDCount    = 1;
   glSharedControl->ThreadIDCount    = 0;

   LONG offset = sizeof(SharedControl);

   glSharedControl->BlocksOffset = offset;
   offset += sizeof(PublicAddress) * glSharedControl->MaxBlocks;

   glSharedControl->SemaphoreOffset = offset;
   offset += sizeof(SemaphoreEntry) * MAX_SEMAPHORES;

   glSharedControl->WLOffset = offset;
   offset += sizeof(WaitLock) * MAX_WAITLOCKS;

   glSharedControl->TaskOffset = offset;
   offset += sizeof(TaskList) * MAX_TASKS;

   glSharedControl->MemoryOffset = RoundPageSize(glMemorySize);

   // Allocate public locks (for sharing between processes)

   #ifdef __unix__
      UBYTE i;
      KMSG("Initialising %d global locks\n", PL_END-1);
      for (i=1; i < PL_END; i++) {
         if (alloc_public_lock(i, ALF_RECURSIVE)) { CloseCore(); return ERR_Failed; };
      }
   #endif

   return ERR_Okay;
}

/*****************************************************************************

-FUNCTION-
PrintDiagnosis: Prints program state information to stdout.
Category: Logging

An analysis of any task's current state can be printed to stdout by calling this function.  This information is useful
solely for the purpose of debugging, particularly in difficult situations where reviewing the current context and
resource data may prove useful.

-INPUT-
int Process: Process ID of the task to analyse - if zero, the current task is analysed.
int Signal: Intended for internal use only (set to zero), indicates a signal number in the event of a crash.
-END-

*****************************************************************************/

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
void PrintDiagnosis(LONG ProcessID, LONG Signal)
{
   if (!ProcessID) ProcessID = glProcessID;

   LOGE("Application diagnosis, process %d, signal %d.", ProcessID, Signal);

   if (!shTasks) return;

   WORD j, index;
   TaskList *task = NULL;
   for (j=0; j < MAX_TASKS; j++) {
      if (shTasks[j].ProcessID IS ProcessID) {
         task = &shTasks[j];
         break;
      }
   }

   if (!task) {
      LOGE("Process %d is not listed in the task management array.", ProcessID);
      return;
   }

   if (glCodeIndex != CP_PRINT_CONTEXT) {
      if (Signal) {
         if ((Signal > 0) AND (Signal < ARRAYSIZE(signals))) {
            LOGE("  Signal ID:      %s", signals[Signal]);
         }
         else LOGE("  Signal ID:      %d", Signal);
      }
      glCodeIndex = CP_PRINT_CONTEXT;

      if ((ProcessID IS glProcessID) AND (tlContext != &glTopContext)) {
         LONG class_id;
         STRING classname;
         if ((class_id = tlContext->Object->ClassID)) {
            classname = ResolveClassID(tlContext->Object->ClassID);
         }
         else classname = "None";
         LOGE("  Object Context: #%d / %p [Class: %s / $%.8x]", tlContext->Object->UniqueID, tlContext->Object, classname, tlContext->Object->ClassID);
      }

      glPageFault = 0;
   }

   // Print the last action to be executed at the time of the crash.  If this code fails, it indicates a corrupt action table.

   if (ProcessID IS glProcessID) {
      if (glCodeIndex != CP_PRINT_ACTION) {
         glCodeIndex = CP_PRINT_ACTION;
         if (tlContext->Action > 0) {
            if (tlContext->Field) {
               LOGE("  Last Action:    Set.%s", tlContext->Field->Name);
            }
            else LOGE("  Last Action:    %s", ActionTable[tlContext->Action].Name);
         }
         else if (tlContext->Action < 0) {
            LOGE("  Last Method:    %d", tlContext->Action);
         }
      }
      else LOGE("  The action table is corrupt.");
   }

   #ifdef DBG_DIAGNOSTICS
   if (glLogLevel > 2) {
      if (ProcessID IS glProcessID) LOGE("  Total Pages:    %d", glTotalPages);

      if (!LOCK_PUBLIC_MEMORY(4000)) {
         // Print memory locking information

         PublicAddress *memblocks = ResolveAddress(glSharedControl, glSharedControl->BlocksOffset);

         for (index=0; index < glSharedControl->MaxBlocks; index++) {
            if (!memblocks[index].MemoryID) continue;

            if (memblocks[index].ProcessLockID IS ProcessID) {
               char msg[80];
               LONG m;
               m = snprintf(msg, sizeof(msg), "  MemLock[%.4d]:  %d", index, memblocks[index].MemoryID);

               #ifdef DEBUG
               if (memblocks[index].AccessTime)  m += snprintf(msg+m, sizeof(msg)-m, ", MSec: %dms", (LONG)((PreciseTime()/1000LL) - memblocks[index].AccessTime));
               #endif
               if (memblocks[index].AccessCount) m += snprintf(msg+m, sizeof(msg)-m, ", Locks: %d", memblocks[index].AccessCount);
               if (memblocks[index].ContextID)   m += snprintf(msg+m, sizeof(msg)-m, ", Context: %d", memblocks[index].ContextID);
               if (memblocks[index].ActionID > 0) m += snprintf(msg+m, sizeof(msg)-m, ", Action: %s", ActionTable[memblocks[index].ActionID].Name);
               else if (memblocks[index].ActionID < 0) m += snprintf(msg+m, sizeof(msg)-m, ", Method: %d", memblocks[index].ActionID);
               LOGE(msg);
            }
         }
         UNLOCK_PUBLIC_MEMORY();
      }

      if (!LOCK_SEMAPHORES(4000)) {
         // Print semaphore locking information

         SemaphoreEntry *semlist = ResolveAddress(glSharedControl, glSharedControl->SemaphoreOffset);
         for (index=1; index < MAX_SEMAPHORES; index++) {
            for (j=0; j < ARRAYSIZE(semlist[index].Processes); j++) {
               if (semlist[index].Processes[j].ProcessID IS ProcessID) {
                  LOGE("  Semaphore[%.4d]:  Access: %d,  Blocking: %d", index, semlist[index].Processes[j].AccessCount, semlist[index].Processes[j].BlockCount);
                  break;
               }
            }
         }
         UNLOCK_SEMAPHORES();
      }
   }
   #endif
}

#else

void PrintDiagnosis(LONG ProcessID, LONG Signal)
{
   FILE *fd;
#ifndef _WIN32
   char filename[64];
#endif

   //if (glLogLevel <= 1) return;

   if (!ProcessID) ProcessID = glProcessID;

#ifdef _WIN32
   fd = stderr;
#else
   if ((glFullOS) AND (Signal != SIGINT)) {
      snprintf(filename, sizeof(filename), "/tmp/%d-exception.txt", ProcessID);
      if (!(fd = fopen(filename, "w+"))) {
         // Failure, use stderr
         fd = stderr;
      }
   }
   else fd = stderr;
#endif

   fprintf(fd, "Diagnostic Information:\n\n");

   // Print details of the object context at the time of the crash.  If this code fails, it indicates that the object context is corrupt.

   TaskList *task;
   if (!(task = glTaskEntry)) {
      fprintf(fd, "This process has no registered task entry.\n");
      return;
   }

   if (glCodeIndex != CP_PRINT_CONTEXT) {
      #ifdef __unix__
         fprintf(fd, "  Page Fault:     %p\n", glPageFault);
      #endif
      fprintf(fd, "  Task ID:        %d\n", task->TaskID);
      if (task->ProcessID != glProcessID) fprintf(fd, "  Process ID:     %d (Foreign)\n", task->ProcessID);
      else fprintf(fd, "  Process ID:     %d (Self)\n", task->ProcessID);
      fprintf(fd, "  Message Queue:  %d\n", task->MessageID);
      if (Signal) {
         if ((Signal > 0) AND (Signal < ARRAYSIZE(signals))) {
            fprintf(fd, "  Signal ID:      %s\n", signals[Signal]);
         }
         else fprintf(fd, "  Signal ID:      %d\n", Signal);
      }
      glCodeIndex = CP_PRINT_CONTEXT;

      if ((ProcessID IS glProcessID) AND (tlContext->Object)) {
         LONG class_id;
         CSTRING classname;
         if (tlContext != &glTopContext) {
            if ((class_id = tlContext->Object->ClassID)) {
               classname = ResolveClassID(tlContext->Object->ClassID);
            }
            else classname = "None";
         }
         else {
            classname = "None";
            class_id = 0;
         }
         fprintf(fd, "  Object Context: #%d / %p [Class: %s / $%.8x]\n", tlContext->Object->UniqueID, tlContext->Object, classname, tlContext->Object->ClassID);
      }

      glPageFault = 0;
   }

   // Print the last action to be executed at the time of the crash.  If this code fails, it indicates a corrupt action table.

   if (ProcessID IS glProcessID) {
      if (glCodeIndex != CP_PRINT_ACTION) {
         glCodeIndex = CP_PRINT_ACTION;
         if (tlContext->Action > 0) {
            if (tlContext->Field) {
               fprintf(fd, "  Last Action:    Set.%s\n", tlContext->Field->Name);
            }
            else fprintf(fd, "  Last Action:    %s\n", ActionTable[tlContext->Action].Name);
         }
         else if (tlContext->Action < 0) {
            fprintf(fd, "  Last Method:    %d\n", tlContext->Action);
         }
      }
      else fprintf(fd, "  The action table is corrupt.\n");
   }

   #ifdef DBG_DIAGNOSTICS
   if (glLogLevel > 2) {
      if (ProcessID IS glProcessID) fprintf(fd, "  Total Pages:    %d\n", glTotalPages);

      if (!LOCK_PUBLIC_MEMORY(4000)) {
         // Print memory locking information

         PublicAddress *memblocks = (PublicAddress *)ResolveAddress(glSharedControl, glSharedControl->BlocksOffset);

         LONG index;
         for (index=0; index < glSharedControl->MaxBlocks; index++) {
            if (!memblocks[index].MemoryID) continue;

            if (memblocks[index].ProcessLockID IS ProcessID) {
               fprintf(fd, "  MemLock[%.4d]:  %d", index, memblocks[index].MemoryID);

               #ifdef DEBUG
               if (memblocks[index].AccessTime) fprintf(fd, ", MSec: %dms", (LONG)((PreciseTime()/1000LL) - memblocks[index].AccessTime));
               #endif
               if (memblocks[index].AccessCount) fprintf(fd, ", Locks: %d", memblocks[index].AccessCount);
               if (memblocks[index].ContextID) fprintf(fd, ", Context: %d", memblocks[index].ContextID);

               if (memblocks[index].ActionID > 0) fprintf(fd, ", Action: %s\n", ActionTable[memblocks[index].ActionID].Name);
               else if (memblocks[index].ActionID < 0) fprintf(fd, ", Method: %d\n", memblocks[index].ActionID);
               else fprintf(fd, "\n");
            }
         }
         UNLOCK_PUBLIC_MEMORY();
      }

      if (!LOCK_SEMAPHORES(4000)) {
         // Print semaphore locking information

         SemaphoreEntry *semlist = (SemaphoreEntry *)ResolveAddress(glSharedControl, glSharedControl->SemaphoreOffset);

         for (LONG index=1; index < MAX_SEMAPHORES; index++) {
            if (semlist[index].InstanceID IS glInstanceID) {
               for (LONG j=0; j < ARRAYSIZE(semlist[index].Processes); j++) {
                  if (semlist[index].Processes[j].ProcessID IS ProcessID) {
                     fprintf(fd, "  Semaphore[%.4d]:  Access: %d,  Blocking: %d\n", index, semlist[index].Processes[j].AccessCount, semlist[index].Processes[j].BlockCount);
                     break;
                  }
               }
            }
         }

         UNLOCK_SEMAPHORES();
      }
   }
   #endif

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
      LONG len;

      rewind(fd);
      if ((len = fread(buffer, 1, sizeof(buffer)-1, fd)) > 0) {
         buffer[len] = 0;
         fflush(NULL);
         fsync(STDERR_FILENO);

         fprintf(stderr, "%s", buffer);

         // Copy process status to the output file

         FILE *pf;
         snprintf(filename, sizeof(filename), "/proc/%d/status", ProcessID);
         if ((pf = fopen(filename, "r"))) {
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

//**********************************************************************

#ifdef __unix__
static void DiagnosisHandler(LONG SignalNumber, siginfo_t *Info, APTR Context)
{
   if (glLogLevel < 2) return;
   PrintDiagnosis(glProcessID, 0);
   fflush(NULL);
}
#endif

//**********************************************************************

#ifdef __unix__
static void CrashHandler(LONG SignalNumber, siginfo_t *Info, APTR Context)
{
   parasol::Log log("Core");

   if (glCrashStatus > 1) {
      if ((glCodeIndex) AND (glCodeIndex IS glLastCodeIndex)) {
         fprintf(stderr, "Unable to recover - exiting immediately.\n");
         exit(255);
      }

      glLastCodeIndex = glCodeIndex;
   }

   if (!glCodeIndex) glCodeIndex = CP_START;

   // Analyse the type of signal that has occurred and respond appropriately

   if (glCrashStatus IS 0) {
      if (((SignalNumber IS SIGQUIT) OR (SignalNumber IS SIGHUP)))  {
         log.msg("Termination request - SIGQUIT or SIGHUP.");
         SendMessage(glTaskMessageMID, MSGID_QUIT, 0, NULL, 0);
         glCrashStatus = 1;
         return;
      }

      if (glLogLevel >= 5) {
         log.msg("Process terminated.\n");
      }
      else if ((SignalNumber > 0) AND (SignalNumber < ARRAYSIZE(signals))) {
         fprintf(stderr, "\nProcess terminated, signal %s.\n\n", signals[SignalNumber]);
      }
      else fprintf(stderr, "\nProcess terminated, signal %d.\n\n", SignalNumber);

      if ((SignalNumber IS SIGILL) OR (SignalNumber IS SIGFPE) OR
          (SignalNumber IS SIGSEGV) OR (SignalNumber IS SIGBUS)) {
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

   PrintDiagnosis(glProcessID, SignalNumber);

   CloseCore();
   exit(255);
}
#endif

//**********************************************************************

#ifdef __unix__
static void NullHandler(LONG SignalNumber, siginfo_t *Info, APTR Context)
{
   //printf("Alarm signalled (sig %d).\n", SignalNumber);
}
#endif

//**********************************************************************

#ifdef __unix__
static void child_handler(LONG SignalNumber, siginfo_t *Info, APTR Context)
{
#if 0
   parasol:Log log(__FUNCTION__);
   rkTask *task;
   LONG childprocess, status, result, i;

   childprocess = Info->si_pid;

   // Get the return code

   status = 0;
   waitpid(Info->si_pid, &status, WNOHANG);
   result = WEXITSTATUS(status);

   log.warning("Process #%d exited, return-code %d.", childprocess, result);

   // Store the return code for this process in any Task object that is associated with it.
   //
   // !!! TODO: The slow methodology of this loop needs attention !!!

   for (i=0; i < glNextPrivateAddress; i++) {
      if (!(glPrivateMemory[i].Flags & MEM_OBJECT)) continue;

      if ((task = glPrivateMemory[i].Address)) {
         if ((task->Head.ClassID IS ID_TASK) AND (task->ProcessID IS childprocess)) {
            task->ReturnCode    = result;
            task->ReturnCodeSet = TRUE;
            break;
         }
      }
   }

   validate_process(childprocess);
#endif
}
#endif

//**********************************************************************
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
   parasol::Log log("Core");

   //winDeathBringer(0);  // Win7 doesn't like us calling SendMessage() during our handler?

   if (!glProcessID) return 1;

   if (glCrashStatus > 1) {
      if ((glCodeIndex) AND (glCodeIndex IS glLastCodeIndex)) {
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
         if ((Code IS EXP_ACCESS_VIOLATION) AND (Info)) {
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

   PrintDiagnosis(0,0);
   fflush(NULL);

   CloseCore();
   return 2; // Force immediate termination of the process
}
#endif

/*****************************************************************************
** Loads the module cache file.  If the cache file does not exist, it is created.
**
** The cache merely holds the location of each system module that is installed in the system.  No modules are loaded
** for inspection by this routine.
*/

static ERROR load_modules(void)
{
   parasol::Log log(__FUNCTION__);
   LONG i, j;

   // Entry structure:
   //
   //   ULONG Hash
   //   LONG  Size
   //   UBYTE Path[]

   OBJECTPTR file;
   ERROR error;
   if (glSharedControl->ModulesMID) {
      if (!AccessMemory(glSharedControl->ModulesMID, MEM_READ, 2000, (APTR *)&glModules)) {
         return ERR_Okay;
      }
      else return log.warning(ERR_AccessMemory);
   }
   else if (!CreateObject(ID_FILE, 0, &file,
         FID_Path|TSTR,   glModuleBinPath,
         FID_Flags|TLONG, FL_READ,
         TAGEND)) {
      LONG size;
      if (!(error = GetLong(file, FID_Size, &size))) {
         if (!(error = AllocMemory(size, MEM_NO_CLEAR|MEM_PUBLIC|MEM_UNTRACKED|MEM_NO_BLOCK, (APTR *)&glModules, &glSharedControl->ModulesMID))) {
            error = acRead(file, glModules, size, NULL);
         }
      }
      acFree(file);

      if (!error) return ERR_Okay;
      else {
         log.error("Failed to read %s", glModuleBinPath);
         return ERR_File;
      }
   }

   log.branch("Scanning for available modules.");
   char modules[16384];

   size_t pos = 0;
   LONG total = 0;

   DirInfo *dir;
   if (!OpenDir("modules:", RDF_QUALIFY, &dir)) {
      while ((!ScanDir(dir)) AND (pos < (sizeof(modules)-256))) {
         FileInfo *folder = dir->Info;
         if (folder->Flags & RDF_FILE) {
            ModuleItem *item = (ModuleItem *)(modules + pos);

            #ifdef __ANDROID__
               UBYTE modname[60];
               CSTRING foldername = folder->Name;

               // Android modules are in the format "libcategory_modname.so"

               if ((foldername[0] IS 'l') AND (foldername[1] IS 'i') AND (foldername[2] IS 'b')) {
                  foldername += 3;

                  // Skip category if one is specified, since we just want the module's short name.

                  for (LONG j=0; foldername[j]; j++) {
                     if (foldername[j] IS '_') {
                        foldername += j + 1;
                        break;
                     }
                  }

                  for (i=0; foldername[i] AND (foldername[i] != '.') AND (i < sizeof(modname)); i++) modname[i] = foldername[i];
                  modname[i] = 0;

                  item->Hash = StrHash(modname, FALSE);

                  pos += sizeof(ModuleItem);
                  pos += StrCopy("modules:", modules+pos, sizeof(modules)-pos-1);
                  for (i=0; folder->Name[i] AND (folder->Name[i] != '.') AND (pos < sizeof(modules)-1); i++) modules[pos++] = folder->Name[i]; // Copy everything up to the extension.
                  modules[pos++] = 0; // Include the null byte.
               }
               else continue;  // Anything not starting with 'lib' is ignored.
            #else
               char modname[60];

               for (i=0; folder->Name[i] AND (folder->Name[i] != '.') AND (i < (LONG)sizeof(modname)); i++) modname[i] = folder->Name[i];
               modname[i] = 0;

               item->Hash = StrHash(modname, FALSE);

               pos += sizeof(ModuleItem);
               pos += StrCopy("modules:", modules+pos, sizeof(modules)-pos-1);
               pos += StrCopy(modname, modules+pos, sizeof(modules)-pos-1);
               pos++; // Include the null byte.
            #endif

            item->Size = (MAXINT)(modules + pos) - (MAXINT)item;

            total++;
         }
      }
      FreeResource(dir);
   }

   if ((total > 0) AND (!(error = AllocMemory(sizeof(ModuleHeader) + (total * sizeof(LONG)) + pos, MEM_NO_CLEAR|MEM_PUBLIC|MEM_UNTRACKED|MEM_NO_BLOCK, (APTR *)&glModules, &glSharedControl->ModulesMID)))) {
      glModules->Total = total;

      // Generate the offsets

      LONG *offsets = (LONG *)(glModules + 1);
      ModuleItem *item = (ModuleItem *)((APTR)modules);
      for (LONG i=0; i < total; i++) {
         offsets[i] = (MAXINT)item - (MAXINT)modules + sizeof(ModuleHeader) + (total<<2);
         item = (ModuleItem *)(((char *)item) + item->Size);
      }

      CopyMemory(modules, offsets + total, pos);

      // Sort the offsets

      LONG h = 1;
      while (h < total / 9) h = 3 * h + 1;
      for (; h > 0; h /= 3) {
         for (LONG i=h; i < total; i++) {
            LONG temp = offsets[i];
            for (j=i; (j >= h) AND (((ModuleItem *)((char *)glModules + offsets[j-h]))->Hash > ((ModuleItem *)((char *)glModules + temp))->Hash); j -= h) {
               offsets[j] = offsets[j - h];
            }
            offsets[j] = temp;
         }
      }

      if (!CreateObject(ID_FILE, 0, &file,
            FID_Path|TSTR,         glModuleBinPath,
            FID_Flags|TLONG,       FL_NEW|FL_WRITE,
            FID_Permissions|TLONG, PERMIT_USER_READ|PERMIT_USER_WRITE|PERMIT_GROUP_READ|PERMIT_GROUP_WRITE|PERMIT_OTHERS_READ,
            TAGEND)) {
         acWrite(file, &total, sizeof(total), NULL);
         acWrite(file, offsets, total * sizeof(LONG), NULL);
         acWrite(file, modules, pos, NULL);
         acFree(file);
      }
   }
   else {
      log.warning("Failed to find anything in 'modules:'");
      error = ERR_Search;
   }

   return error;
}

//**********************************************************************

ERROR convert_errno(LONG Error, ERROR Default)
{
   switch (Error) {
      case 0:       return ERR_Okay;
      case ENAMETOOLONG: return ERR_BufferOverflow;
      case EACCES:  return ERR_NoPermission;
      case EPERM:   return ERR_NoPermission;
      case EBUSY:   return ERR_Locked;
      case EROFS:   return ERR_ReadOnly;
      case EMFILE:  return ERR_ArrayFull;
      case ENFILE:  return ERR_ArrayFull;
      case ENOENT:  return ERR_FileNotFound;
      case ENOMEM:  return ERR_AllocMemory;
      case ENOTDIR: return ERR_FileNotFound;
      case EEXIST:  return ERR_FileExists;
      case ENOSPC:  return ERR_OutOfSpace;
      case EFAULT:  return ERR_IllegalAddress;
      case EIO:     return ERR_Failed;
      #ifdef ELOOP
      case ELOOP:   return ERR_Loop;
      #endif
      default:      return Default;
   }
}

//**********************************************************************

#ifdef _WIN32
static void BreakHandler(void)
{
   parasol::Log log("Core");

   //winDeathBringer(0);  // Win7 doesn't like us calling SendMessage() during our handler?

   if (glLogLevel >= 5) {
      log.warning("USER BREAK"); // Using log is helpful for branched output to indicate where the crash occurred
   }
   else fprintf(stderr, "\nUSER BREAK");

   glCrashStatus = 1;

   PrintDiagnosis(0,0);
   fflush(NULL);

   CloseCore();
}
#endif

//****************************************************************************

#ifdef _WIN32
static void win32_enum_folders(CSTRING Volume, CSTRING Label, CSTRING Path, CSTRING Icon, BYTE Hidden)
{
   SetVolume(AST_NAME, Volume,
      AST_PATH,  Path,
      AST_FLAGS, VOLUME_REPLACE | (Hidden ? VOLUME_HIDDEN : 0),
      AST_ICON,  Icon,
      AST_LABEL, Label,
      TAGEND);
}
#endif

//****************************************************************************

static ERROR init_filesystem(void)
{
   parasol::Log log("Core");
   LONG i;
   char buffer[300];

   log.branch("Initialising filesystem.");

   glVirtualTotal = 1;
   glVirtual[0] = glFSDefault;

   // If the public volume list is not already in memory, load it. The SystemVolumes object provides us with a
   // way to resolve volume names to file locations, as well as providing other classes with a way to
   // peruse the volumes registered within the system.

   log.trace("Attempting to create SystemVolumes object.");

   ERROR error;
   if (!(error = NewObject(ID_CONFIG, NF_NO_TRACK, (OBJECTPTR *)&glVolumes))) {
      SetName(&glVolumes->Head, "SystemVolumes");

      #ifndef __ANDROID__ // For security reasons we do not use an external volume file for the Android build.
         {
            #ifdef _WIN32
               StrFormat(buffer, sizeof(buffer), "%sconfig\\volumes.cfg", glSystemPath);
            #else
               StrFormat(buffer, sizeof(buffer), "%sconfig/volumes.cfg", glSystemPath);
            #endif
            SetString(glVolumes, FID_Path, buffer);
         }
      #endif

      if (acInit(&glVolumes->Head) != ERR_Okay) {
         acFree(&glVolumes->Head);
         return log.warning(ERR_CreateObject);
      }

      // Add system volumes that require run-time determination.  For the avoidance of doubt, on Unix systems the
      // default settings for a fixed installation are:
      //
      // OPF_ROOT_PATH   : parasol : glRootPath   = /usr/local
      // OPF_MODULE_PATH : modules : glModulePath = %ROOT%/lib/parasol
      // OPF_SYSTEM_PATH : system  : glSystemPath = %ROOT%/share/parasol

      #ifdef _WIN32
         SetVolume(AST_NAME, "parasol", AST_PATH, glRootPath, AST_FLAGS, VOLUME_REPLACE|VOLUME_HIDDEN, AST_ICON, "users/user", TAGEND);
         SetVolume(AST_NAME, "system", AST_PATH, glRootPath, AST_FLAGS, VOLUME_REPLACE|VOLUME_HIDDEN, AST_ICON, "programs/tool", TAGEND);

         if (glModulePath[0]) {
            SetVolume(AST_NAME, "modules", AST_PATH, glModulePath, AST_FLAGS, VOLUME_REPLACE|VOLUME_HIDDEN, AST_ICON, "programs/tool", TAGEND);
         }
         else {
            SetVolume(AST_NAME, "modules", AST_PATH, "system:lib/", AST_FLAGS, VOLUME_REPLACE|VOLUME_HIDDEN, AST_ICON, "programs/tool", TAGEND);
         }
      #elif __unix__
         // If device volumes are already set by the user, do not attempt to discover such devices.

         BYTE cd_set = FALSE;
         BYTE hd_set = FALSE;
/*
         ConfigEntry *entries;
         if ((entries = config->Entries)) {
            for (i=0; i < config->AmtEntries; i++) {
               if (!StrMatch("Name", entries[i].Item)) {
                  if (!StrMatch("cd1", entries[i].Data))         cd_set = TRUE;
                  else if (!StrMatch("drive1", entries[i].Data)) hd_set = TRUE;
                  else if (!StrMatch("usb1", entries[i].Data))   usb_set = TRUE;
               }
            }
         }
*/
         SetVolume(AST_NAME, "parasol", AST_PATH, glRootPath, AST_FLAGS, VOLUME_REPLACE|VOLUME_HIDDEN, AST_ICON, "users/user",  TAGEND);
         SetVolume(AST_NAME, "system", AST_PATH, glSystemPath, AST_FLAGS, VOLUME_REPLACE, AST_ICON, "programs/tool",  TAGEND);

         if (glModulePath[0]) {
            SetVolume(AST_NAME, "modules", AST_PATH, glModulePath, AST_FLAGS, VOLUME_REPLACE|VOLUME_HIDDEN, AST_ICON, "programs/tool",  TAGEND);
         }
         else {
            char path[200];
            StrFormat(path, sizeof(path), "%slib/parasol/", glRootPath);
            SetVolume(AST_NAME, "modules", AST_PATH, path, AST_FLAGS, VOLUME_REPLACE|VOLUME_HIDDEN, AST_ICON, "programs/tool",  TAGEND);
         }

         if (!hd_set) {
            if (glFullOS) {
               SetVolume(AST_NAME, "drive1", AST_PATH, "/", AST_LABEL, "Parasol", AST_FLAGS, VOLUME_REPLACE, AST_ICON, "devices/storage", AST_DEVICE, "hd", TAGEND);
            }
            else SetVolume(AST_NAME, "drive1", AST_PATH, "/", AST_LABEL, "Linux", AST_FLAGS, VOLUME_REPLACE, AST_ICON, "devices/storage", AST_DEVICE, "hd", TAGEND);
         }
      #endif

      // Configure some standard volumes.

      #ifdef __ANDROID__
         SetVolume(AST_NAME, "assets", AST_PATH, "CLASS:FileAssets", AST_FLAGS, VOLUME_REPLACE|VOLUME_HIDDEN, TAGEND);
         SetVolume(AST_NAME, "templates", AST_PATH, "assets:templates/", AST_FLAGS, VOLUME_HIDDEN, AST_ICON, "filetypes/empty",  TAGEND);
         SetVolume(AST_NAME, "config", AST_PATH, "localcache:config/|assets:config/", AST_FLAGS, VOLUME_HIDDEN, AST_ICON, "filetypes/empty",  TAGEND);
      #else
         SetVolume(AST_NAME, "templates", AST_PATH, "system:scripts/templates/", AST_FLAGS, VOLUME_HIDDEN, AST_ICON, "filetypes/empty",  TAGEND);
         SetVolume(AST_NAME, "config", AST_PATH, "system:config/", AST_FLAGS, VOLUME_HIDDEN, AST_ICON, "filetypes/empty",  TAGEND);
         if (!AnalysePath("parasol:bin/", NULL)) { // Bin is the location of the fluid and parasol binaries
            SetVolume(AST_NAME, "bin", AST_PATH, "parasol:bin/", AST_FLAGS, VOLUME_HIDDEN, TAGEND);
         }
         else SetVolume(AST_NAME, "bin", AST_PATH, "parasol:", AST_FLAGS, VOLUME_HIDDEN, TAGEND);
      #endif

      SetVolume(AST_NAME, "temp", AST_PATH, "user:temp/", AST_FLAGS, VOLUME_HIDDEN, AST_ICON, "items/trash",  TAGEND);
      SetVolume(AST_NAME, "fonts", AST_PATH, "system:fonts/", AST_FLAGS, VOLUME_HIDDEN, AST_ICON, "items/charset",  TAGEND);
      SetVolume(AST_NAME, "styles", AST_PATH, "system:styles/", AST_FLAGS, VOLUME_HIDDEN, AST_ICON, "tools/image_gallery",  TAGEND);

      // Some platforms need to have special volumes added - these are provided in the OpenInfo structure passed to
      // the Core.

      if ((glOpenInfo->Flags & OPF_OPTIONS) AND (glOpenInfo->Options)) {
         for (i=0; glOpenInfo->Options[i].Tag != TAGEND; i++) {
            switch (glOpenInfo->Options[i].Tag) {
               case TOI_LOCAL_CACHE: {
                  SetVolume(AST_NAME, "localcache", AST_PATH, glOpenInfo->Options[i].Value.String, AST_FLAGS, VOLUME_REPLACE|VOLUME_HIDDEN, TAGEND);
                  break;
               }
               case TOI_LOCAL_STORAGE: {
                  SetVolume(AST_NAME, "localstorage", AST_PATH, glOpenInfo->Options[i].Value.String, AST_FLAGS, VOLUME_REPLACE|VOLUME_HIDDEN, TAGEND);
                  break;
               }
            }
         }
      }

      if (!glUserHomeFolder) {
         if ((cfgReadValue(glVolumes, "User", "Name", &glUserHomeFolder) != ERR_Okay) OR (!glUserHomeFolder)) glUserHomeFolder = "parasol";
      }

      if (!StrMatch("default", glUserHomeFolder)) {
         CSTRING path;
         if (!cfgReadValue(glVolumes, "User", "Path", &path)) {
            i = StrCopy(path, buffer, sizeof(buffer));
         }
         else i = StrCopy("config:users/", buffer, sizeof(buffer));
      }
      else {
         #ifdef __unix__
            STRING homedir, logname;
            if ((homedir = getenv("HOME")) AND (homedir[0]) AND (StrMatch("/", homedir) != ERR_Okay)) {
               log.msg("Home folder is \"%s\".", homedir);
               for (i=0; (homedir[i]) AND (i < (LONG)sizeof(buffer)-1); i++) buffer[i] = homedir[i];
               while ((i > 0) AND (buffer[i-1] IS '/')) i--;
               i += StrFormat(buffer+i, sizeof(buffer)-i, "/.%s%d/", glUserHomeFolder, F2T(VER_CORE));
            }
            else if ((logname = getenv("LOGNAME")) AND (logname[0])) {
               log.msg("Login name for home folder is \"%s\".", logname);
               i = StrFormat(buffer, sizeof(buffer), "config:users/%s/", logname);
               buffer[i] = 0;
            }
            else {
               log.msg("Unable to determine home folder, using default.");
               i = StrCopy("config:users/default/", buffer, COPY_ALL);
            }
         #elif _WIN32
            // Attempt to get the path of the user's personal folder.  If the Windows system doesn't have this
            // facility, attempt to retrieve the login name and store the user files in the system folder.

            if ((i = winGetUserFolder(buffer, sizeof(buffer)-40))) {
               StrFormat(buffer+i, sizeof(buffer)-i, "%s%d%d\\", glUserHomeFolder, F2T(VER_CORE), REV_CORE);
               while (buffer[i]) i++;
            }
            else {
               i = StrCopy("config:users/", buffer, 0);
               if ((winGetUserName(buffer+i, sizeof(buffer)-i) AND (buffer[i]))) {
                  while (buffer[i]) i++;
                  buffer[i++] = '/';
                  buffer[i] = 0;
               }
               else i += StrCopy("default/", buffer+i, COPY_ALL);
            }
         #else
            i = StrCopy("config:users/default/", buffer, COPY_ALL);
         #endif

         // Copy the default configuration files to the user: folder.  This also has the effect of creating the user
         // folder if it does not already exist.

         if (StrMatch("config:users/default/", buffer) != ERR_Okay) {
            LONG location_type = 0;
            if ((AnalysePath(buffer, &location_type) != ERR_Okay) OR (location_type != LOC_DIRECTORY)) {
               buffer[i-1] = 0;
               SetDefaultPermissions(-1, -1, PERMIT_READ|PERMIT_WRITE);
                  CopyFile("config:users/default/", buffer, NULL);
               SetDefaultPermissions(-1, -1, 0);
               buffer[i-1] = '/';
            }

            i += StrCopy("|config:users/default/", buffer+i, sizeof(buffer)-i);
            buffer[i] = 0;
         }
      }

      // Reset the user: volume

      log.msg("Home Folder: %s", buffer);

      SetVolume(AST_NAME, "user:", AST_PATH, buffer, AST_Flags, VOLUME_REPLACE, AST_ICON, "users/user", TAGEND);

      // Make sure that certain default directories exist

      CreateFolder("user:config/", PERMIT_READ|PERMIT_EXEC|PERMIT_WRITE);
      CreateFolder("user:temp/", PERMIT_READ|PERMIT_EXEC|PERMIT_WRITE);

      // Set the default folder from temp:.  It is possible for the user to overwrite this in volumes.cfg if he
      // would like temporary files to be written somewhere else.

      if (AnalysePath("temp:", NULL) != ERR_Okay) {
         SetVolume(AST_NAME, "temp:", AST_PATH, "user:temp/", AST_FLAGS, VOLUME_REPLACE|VOLUME_HIDDEN, AST_ICON, "items/trash", TAGEND);
      }

      if (AnalysePath("clipboard:", NULL) != ERR_Okay) {
         SetVolume(AST_NAME, "clipboard:", AST_PATH, "temp:clipboard/", AST_FLAGS, VOLUME_REPLACE|VOLUME_HIDDEN, AST_ICON, "items/clipboard", TAGEND);
      }

      // Look for the following drive types:
      //
      // CD-ROMS:               CD1/CD2/CD3
      // Removable Media:       Disk1/Disk2 (floppies etc)
      // Hard Drive Partitions: HD1/HD2
      //
      // NOTE: In the native release all media, including volumes, are controlled by the mountdrives program.
      // Mountdrives also happens to manage the system:hardware/drives.cfg file.

      const SystemState *state = GetSystemState();
      if (StrMatch("Native", state->Platform) != ERR_Okay) {
#ifdef _WIN32
         LONG len;
         if ((len = winGetLogicalDriveStrings(buffer, sizeof(buffer))) > 0) {
            char disk[] = "disk1";
            char cd[]   = "cd1";
            char hd[]   = "drive1";
            char net[]  = "net1";

            for (i=0; i < len; i++) {
               LONG type = winGetDriveType(buffer+i);

               buffer[i+2] = '/';

               char label[2];
               label[0] = buffer[i];
               label[1] = 0;

               if (type IS DRIVETYPE_REMOVABLE) {
                  SetVolume(AST_NAME, disk, AST_PATH, buffer+i, AST_LABEL, label, AST_ICON, "devices/storage", AST_DEVICE, "disk", TAGEND);
                  disk[4]++;
               }
               else if (type IS DRIVETYPE_CDROM) {
                  SetVolume(AST_NAME, cd, AST_PATH, buffer+i, AST_LABEL, label, AST_ICON, "devices/compactdisc", AST_DEVICE, "cd", TAGEND);
                  cd[2]++;
               }
               else if (type IS DRIVETYPE_FIXED) {
                  SetVolume(AST_Name, hd, AST_Path, buffer+i, AST_LABEL, label, AST_ICON, "devices/storage", AST_DEVICE, "hd", TAGEND);
                  hd[5]++;
               }
               else if (type IS DRIVETYPE_NETWORK) {
                  SetVolume(AST_NAME, net, AST_PATH, buffer+i, AST_LABEL, label, AST_ICON, "devices/network", AST_DEVICE, "network", TAGEND);
                  net[3]++;
               }
               else log.warning("Drive %s identified as unsupported type %d.", buffer+i, type);

               while (buffer[i]) i++;
            }
         }

         winEnumSpecialFolders(&win32_enum_folders);
#endif

#if defined(__linux__) && !defined(__ANDROID__)

         if (!hd_set) {
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
               if (!AllocMemory(size, MEM_NO_CLEAR, (APTR *)&buffer, NULL)) {
                  size = read(file, buffer, size);
                  buffer[size] = 0;

                  CSTRING str = buffer;
                  while (*str) {
                     if (!StrCompare("/dev/hd", str, 0, 0)) {
                        // Extract mount point

                        i = 0;
                        while ((*str) AND (*str > 0x20)) {
                           if (i < (LONG)sizeof(devpath)-1) devpath[i++] = *str;
                           str++;
                        }
                        devpath[i] = 0;

                        while ((*str) AND (*str <= 0x20)) str++;
                        for (i=0; (*str) AND (*str > 0x20) AND (i < (LONG)sizeof(mount)-1); i++) mount[i] = *str++;
                        mount[i] = 0;

                        if ((mount[0] IS '/') AND (!mount[1]));
                        else {
                           IntToStr(driveno++, drivename+5, 3);
                           SetVolume(AST_NAME, drivename, AST_DEVICE_PATH, devpath, AST_PATH, mount, AST_ICON, "devices/storage", AST_DEVICE, "hd", TAGEND);
                        }
                     }

                     // Next line
                     while ((*str) AND (*str != '\n')) str++;
                     while ((*str) AND (*str <= 0x20)) str++;
                  }
                  FreeResource(buffer);
               }
               else log.warning(ERR_AllocMemory);

               close(file);
            }
            else log.warning(ERR_File);
         }
         else log.msg("Not scanning for hard disks because user has defined drive1.");

         if (!cd_set) {
            CSTRING cdroms[] = {
               "/mnt/cdrom", "/mnt/cdrom0", "/mnt/cdrom1", "/mnt/cdrom2", "/mnt/cdrom3", "/mnt/cdrom4", "/mnt/cdrom5", "/mnt/cdrom6", // RedHat
               "/cdrom0", "/cdrom1", "/cdrom2", "/cdrom3" // Debian
            };
            char cdname[] = "cd1";

            for (i=0; i < ARRAYSIZE(cdroms); i++) {
               if (!access(cdroms[i], F_OK)) {
                  SetVolume(AST_Name, cdname, AST_Path, cdroms[i], AST_ICON, "devices/compactdisc", AST_DEVICE, "cd", TAGEND);
                  cdname[2] = cdname[2] + 1;
               }
            }
         }
#endif
      }

      // Merge drive mounts into the system volume list

      cfgMergeFile(glVolumes, "config:hardware/drives.cfg");

      // Merge user preferences into the system volume list

      cfgMergeFile(glVolumes, "user:config/volumes.cfg");
   }
   else if (error != ERR_ObjectExists) {
      log.warning("Failed to create the SystemVolumes object.");
      return ERR_NewObject;
   }

   // FileView document templates

   if (!AnalysePath("templates:fileview/root.rpl", NULL)) SetDocView(":", "templates:fileview/root.rpl");
   if (!AnalysePath("templates:fileview/fonts.rpl", NULL)) SetDocView("fonts:", "templates:fileview/fonts.rpl");
   if (!AnalysePath("templates:fileview/pictures.rpl", NULL)) SetDocView("pictures:", "templates:fileview/pictures.rpl");
   if (!AnalysePath("templates:fileview/icons.rpl", NULL)) SetDocView("icons:", "templates:fileview/icons.rpl");

   // Create the 'archive' volume (non-essential)

   create_archive_volume();

   return ERR_Okay;
}

#include "core_close.cpp"
