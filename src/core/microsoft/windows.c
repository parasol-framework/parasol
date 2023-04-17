#define PRV_FILESYSTEM

#define _WIN32_WINNT 0x0600 // Required for CRITICAL_SECTION - min version. Windows Vista
#define PSAPI_VERSION 1
#include <stdio.h>
#include <wtypes.h>
#include <memory.h>
#include <winnt.h>
#include <winbase.h>
#include <psapi.h>
#include <io.h>
#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <winioctl.h>
#include <shlobj.h>

#include <sys/time.h>
#ifdef __CYGWIN__
#include <sys/timespec.h>
#endif
#include <tchar.h>
#include <imagehlp.h>

#ifndef PLATFORM_CONFIG_H
#include <parasol/config.h>
#endif

#include "windefs.h"
#include <parasol/system/errors.h>

#define STD_TIMEOUT 1000
#define IS ==
#define AND &&
#define OR ||

#ifdef DEBUG
#define MSG(...) printf(__VA_ARGS__)
#else
#define MSG(...)
#endif

#define WAITLOCK_EVENTS 1 // Use events instead of semaphores for waitlocks (recommended)

enum {
   STAGE_STARTUP=1,
   STAGE_ACTIVE,
   STAGE_SHUTDOWN
};

extern LONG glProcessID;
extern HANDLE glProcessHandle;
extern BYTE glProgramStage;

static HANDLE glInstance = 0;
static HANDLE glMsgWindow = 0;
HANDLE glValidationSemaphore = 0;

LONG glMutexLockSize = sizeof(CRITICAL_SECTION);

WINBASEAPI VOID WINAPI InitializeConditionVariable(PCONDITION_VARIABLE ConditionVariable);
WINBASEAPI WINBOOL WINAPI SleepConditionVariableCS(PCONDITION_VARIABLE ConditionVariable, PCRITICAL_SECTION CriticalSection, DWORD dwMilliseconds);
WINBASEAPI WINBOOL WINAPI SleepConditionVariableSRW(PCONDITION_VARIABLE ConditionVariable, PSRWLOCK SRWLock, DWORD dwMilliseconds, ULONG Flags);
WINBASEAPI VOID WINAPI WakeAllConditionVariable(PCONDITION_VARIABLE ConditionVariable);
WINBASEAPI VOID WINAPI WakeConditionVariable(PCONDITION_VARIABLE ConditionVariable);

LONG plAllocPrivateSemaphore(HANDLE *Semaphore, LONG InitialValue);
void plFreePrivateSemaphore(HANDLE *Semaphore);
LONG plLockSemaphore(HANDLE *Semaphore, LONG TimeOut);
void plUnlockSemaphore(HANDLE *Semaphore);
long long winGetTickCount(void);

static LRESULT CALLBACK window_procedure(HWND, UINT, WPARAM, LPARAM);

extern LONG validate_process(LONG ProcessID);

typedef void * APTR;
typedef unsigned char UBYTE;

static UINT glDeadProcessMsg; // Read only.

#define LEN_OUTPUTBUFFER 1024

LONG ExceptionFilter(LPEXCEPTION_POINTERS Args);
char * winFormatMessage(LONG, char *Buffer, LONG BufferSize);
static BOOL break_handler(DWORD CtrlType);

static LONG (*glCrashHandler)(LONG, APTR, LONG, APTR) = 0;
static void (*glBreakHandler)(void) = 0;

struct stdpipe {
   HANDLE Read;
   HANDLE Write;
   HANDLE Event;
   OVERLAPPED OvRead;
   OVERLAPPED OvWrite;
   char Buffer[1024];
};

struct winprocess {
   APTR Task;
   HANDLE Handle;       // The handle to the created process
   struct stdpipe PipeOut;
   struct stdpipe PipeIn;
   struct stdpipe PipeErr;
   OVERLAPPED OutOverlap;
   OVERLAPPED ErrOverlap;
   HANDLE StdErrEvent;
   HANDLE StdOutEvent;
   char OutBuffer[4];
   char ErrBuffer[4];
   DWORD OutTotalRead;
   DWORD ErrTotalRead;
};

#define MAX_HANDLES 20

static struct {
   LONG   OtherProcess;
   HANDLE OtherHandle;
   HANDLE LocalHandle;
} glHandleBank[MAX_HANDLES];

static WORD glHandleCount = 0;
static CRITICAL_SECTION csHandleBank;
static CRITICAL_SECTION csJob;


//typedef unsigned char * STRING;
typedef long long LARGE;
typedef void * APTR;
//typedef void * OBJECTPTR;
typedef unsigned char UBYTE;


typedef struct DateTime {
   LONG Year;
   LONG Month;
   LONG Day;
   LONG Hour;
   LONG Minute;
   LONG Second;
   LONG TimeZone;
} DateTime;

#define IS ==
#define OR ||
#define AND &&

#define DRIVETYPE_REMOVABLE 1
#define DRIVETYPE_CDROM     2
#define DRIVETYPE_FIXED     3
#define DRIVETYPE_NETWORK   4

#define MFF_READ 0x00000001
#define MFF_MODIFY 0x00000002
#define MFF_CREATE 0x00000004
#define MFF_DELETE 0x00000008
#define MFF_MOVED 0x00000010
#define MFF_ATTRIB 0x00000020
#define MFF_OPENED 0x00000040
#define MFF_CLOSED 0x00000080
#define MFF_UNMOUNT 0x00000100
#define MFF_FOLDER 0x00000200
#define MFF_FILE 0x00000400
#define MFF_SELF 0x00000800
#define MFF_DEEP 0x00001000
#define MFF_RENAME (MFF_MOVED)
#define MFF_WRITE (MFF_MODIFY)

// Return codes available to the feedback routine

#define FFR_OKAY 0
#define FFR_CONTINUE 0
#define FFR_SKIP 1
#define FFR_ABORT 2

#define PERMIT_READ 0x00000001
#define PERMIT_WRITE 0x00000002
#define PERMIT_EXEC 0x00000004
#define PERMIT_DELETE 0x00000008
#define PERMIT_GROUP_READ 0x00000010
#define PERMIT_GROUP_WRITE 0x00000020
#define PERMIT_GROUP_EXEC 0x00000040
#define PERMIT_GROUP_DELETE 0x00000080
#define PERMIT_OTHERS_READ 0x00000100
#define PERMIT_OTHERS_WRITE 0x00000200
#define PERMIT_OTHERS_EXEC 0x00000400
#define PERMIT_OTHERS_DELETE 0x00000800
#define PERMIT_HIDDEN 0x00001000
#define PERMIT_ARCHIVE 0x00002000
#define PERMIT_PASSWORD 0x00004000
#define PERMIT_USERID 0x00008000
#define PERMIT_GROUPID 0x00010000
#define PERMIT_INHERIT 0x00020000
#define PERMIT_OFFLINE 0x00040000
#define PERMIT_NETWORK 0x00080000
#define PERMIT_USER_READ (PERMIT_READ)
#define PERMIT_USER_WRITE (PERMIT_WRITE)
#define PERMIT_USER_EXEC (PERMIT_EXEC)
#define PERMIT_EVERYONE_READ (PERMIT_READ|PERMIT_GROUP_READ|PERMIT_OTHERS_READ)
#define PERMIT_EVERYONE_WRITE (PERMIT_WRITE|PERMIT_GROUP_WRITE|PERMIT_OTHERS_WRITE)
#define PERMIT_EVERYONE_EXEC (PERMIT_EXEC|PERMIT_GROUP_EXEC|PERMIT_OTHERS_EXEC)
#define PERMIT_EVERYONE_DELETE (PERMIT_DELETE|PERMIT_GROUP_DELETE|PERMIT_OTHERS_DELETE)
#define PERMIT_ALL_READ (PERMIT_EVERYONE_READ)
#define PERMIT_ALL_WRITE (PERMIT_EVERYONE_WRITE)
#define PERMIT_ALL_EXEC (PERMIT_EVERYONE_EXEC)
#define PERMIT_ALL_DELETE (PERMIT_EVERYONE_DELETE)
#define PERMIT_EVERYONE_ACCESS (PERMIT_EVERYONE_READ|PERMIT_EVERYONE_WRITE|PERMIT_EVERYONE_EXEC|PERMIT_EVERYONE_DELETE)
#define PERMIT_EVERYONE_READWRITE (PERMIT_EVERYONE_READ|PERMIT_EVERYONE_WRITE)
#define PERMIT_USER (PERMIT_READ|PERMIT_WRITE|PERMIT_EXEC|PERMIT_DELETE)
#define PERMIT_GROUP (PERMIT_GROUP_READ|PERMIT_GROUP_WRITE|PERMIT_GROUP_EXEC|PERMIT_GROUP_DELETE)
#define PERMIT_OTHERS (PERMIT_OTHERS_READ|PERMIT_OTHERS_WRITE|PERMIT_OTHERS_EXEC|PERMIT_OTHERS_DELETE)

#define LOC_DIRECTORY 1
#define LOC_FOLDER 1
#define LOC_VOLUME 2
#define LOC_FILE 3

struct FileFeedback {
   LARGE  Size;          // Size of the file
   LARGE  Position;      // Current seek position within the file if moving or copying
   STRING Path;
   STRING Dest;          // Destination file/path if moving or copying
   LONG   FeedbackID;    // Set to one of the FDB integers
   char   Reserved[32];  // Reserved in case of future expansion
};

extern LONG CALL_FEEDBACK(struct FileFeedback *);
extern LONG convert_errno(LONG Error, LONG Default);

LONG winReadPipe(HANDLE FD, APTR Buffer, DWORD *Size);

//********************************************************************************************************************
// Console checker for Cygwin

BYTE is_console(HANDLE h)
{
   if (FILE_TYPE_UNKNOWN == GetFileType(h) && ERROR_INVALID_HANDLE == GetLastError()) {
       if ((h = CreateFile("CONOUT$", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL))) {
           CloseHandle(h);
           return TRUE;
       }
   }

   CONSOLE_FONT_INFO cfi;
   return GetCurrentConsoleFont(h, FALSE, &cfi) != 0;
}

//********************************************************************************************************************
// If the program is launched from a console, attach to it.  Otherwise create a new console window and redirect output
// to it (e.g. if launched from a desktop icon).

void activate_console(BYTE AllowOpenConsole)
{
   static BYTE activated = FALSE;

   if (!activated) {
      char value[8];
      if (GetEnvironmentVariable("TERM", value, sizeof(value)) OR
          GetEnvironmentVariable("PROMPT", value, sizeof(value))) { // TERM defined by Cygwin, Mingw, PROMPT defined by cmd.exe

         // NB: Cygwin stdout/err handling is broken and requires the following workaround for ensuring that stdout
         // and stderr are managed correctly for both standard console output and file redirection.

         HANDLE current_out = GetStdHandle(STD_OUTPUT_HANDLE);
         HANDLE current_err = GetStdHandle(STD_ERROR_HANDLE);

         AttachConsole(ATTACH_PARENT_PROCESS);

         if (is_console(current_out)) freopen("CON", "w", stdout);  // Redirect stdout and stderr descriptors to the attached console.
         if (is_console(current_err)) freopen("CON", "w", stderr);
      }
      else if (AllowOpenConsole) { // Assume that executable was launched from desktop without a console
         AllocConsole();
         AttachConsole(GetCurrentProcessId());
         freopen("CON", "w", stdout);  // Redirect stdout and stderr descriptors to the attached console.
         freopen("CON", "w", stderr);
      }
      else return;

      activated = TRUE;
   }
}

//********************************************************************************************************************

static inline unsigned int LCASEHASH(char *String)
{
   unsigned int hash = 5381;
   unsigned char c;
   while ((c = *String++)) {
      if ((c >= 'A') AND (c <= 'Z')) {
         hash = (hash<<5) + hash + c - 'A' + 'a';
      }
      else hash = (hash<<5) + hash + c;
   }
   return hash;
}

//********************************************************************************************************************

#ifdef DEBUG
static char glSymbolsLoaded = FALSE;
static void windows_print_stacktrace(CONTEXT* context)
{
   if (!glSymbolsLoaded) return;

   STACKFRAME frame = { {0} };

   // setup initial stack frame
   #ifdef _LP64
      frame.AddrPC.Offset    = context->Rip;
      frame.AddrStack.Offset = context->Rsp;
      frame.AddrFrame.Offset = context->Rbp;
   #else
      frame.AddrPC.Offset    = context->Eip;
      frame.AddrStack.Offset = context->Esp;
      frame.AddrFrame.Offset = context->Ebp;
   #endif
   frame.AddrPC.Mode    = AddrModeFlat;
   frame.AddrStack.Mode = AddrModeFlat;
   frame.AddrFrame.Mode = AddrModeFlat;

   while (StackWalk(IMAGE_FILE_MACHINE_I386, GetCurrentProcess(), GetCurrentThread(), &frame, context, 0, SymFunctionTableAccess, SymGetModuleBase, 0)) {
      // Declare an image help symbol structure to hold symbol info and name up to 256 chars This struct is of variable length though so it must be declared as a raw byte buffer.

      static char symbolBuffer[sizeof(IMAGEHLP_SYMBOL) + 255];
      memset( symbolBuffer, 0, sizeof(IMAGEHLP_SYMBOL) + 255);

      IMAGEHLP_SYMBOL *symbol = (IMAGEHLP_SYMBOL *)symbolBuffer;
      symbol->SizeOfStruct  = sizeof(IMAGEHLP_SYMBOL) + 255;
      symbol->MaxNameLength = 254;

      #ifdef _LP64
         DWORD64 displacement = 0;
      #else
         DWORD displacement = 0;
      #endif
      if (SymGetSymFromAddr(GetCurrentProcess(), frame.AddrPC.Offset, &displacement, symbol)) {
         fprintf(stderr, "0x%p %s\n", (APTR)frame.AddrPC.Offset, symbol->Name);

         IMAGEHLP_LINE line;
         line.SizeOfStruct = sizeof(line);
         line.LineNumber = 0;
         DWORD line_displacement = 0;
         if (SymGetLineFromAddr(GetCurrentProcess(), frame.AddrPC.Offset, &line_displacement, &line)) {
            fprintf(stderr, "Line: %s, %d\n", line.FileName, (int)line.LineNumber);
         }
         else {
            //char msg[400];
            //fprintf(stderr, "SymGetLineFromAddr(): %s\n", winFormatMessage(GetLastError(), msg, sizeof(msg)));
         }
      }
      else {
         fprintf(stderr, "0x%p\n", (APTR)frame.AddrPC.Offset);
      }
   }

   SymCleanup(GetCurrentProcess());
}
#endif

//********************************************************************************************************************

static const char *glMsgClass = "RKLMessageClass";

LONG winInitialise(unsigned int *PathHash, void *BreakHandler)
{
   MEMORY_BASIC_INFORMATION mbiInfo;
   char path[255];
   LONG len;

   #ifdef DEBUG
      // This is only needed if the application crashes and a stack trace is printed.
      SymSetOptions(SymGetOptions() | SYMOPT_LOAD_LINES | SYMOPT_DEFERRED_LOADS);
      if (SymInitialize(GetCurrentProcess(), 0, TRUE)) glSymbolsLoaded = TRUE;
   #endif

   // This turns off dialog boxes that Microsoft forces upon the user in certain situations (e.g. "No Disk in Drive"
   // when checking floppy drives).

   SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOOPENFILEERRORBOX); // SEM_NOGPFAULTERRORBOX

   // Calculate a unique hash from the Core's DLL path.  This hash can then be used for telling which Parasol
   // programs are using the same set of binaries.

   if (PathHash) {
      *PathHash = 0;

      SetLastError(ERROR_SUCCESS);
      if (VirtualQuery(winInitialise, &mbiInfo, sizeof(mbiInfo))) {
         if ((len = GetModuleFileName((HINSTANCE)mbiInfo.AllocationBase, path, sizeof(path)))) {
            *PathHash = LCASEHASH(path);
         }
      }
   }

   // These commands turn off buffering, which results in all output being flushed.  This will cause noticeable
   // slowdown when debugging if you enable it.
   //setbuf(stdout, NULL);
   //setbuf(stderr, NULL);

   // Setup a handler for intercepting CTRL-C and CTRL-BREAK signals.  This function is limited to Windows 2000 Professional and above.

   if (BreakHandler) {
      glBreakHandler = BreakHandler;
      SetConsoleCtrlHandler((PHANDLER_ROUTINE)&break_handler, TRUE);
   }

   InitializeCriticalSection(&csHandleBank);
   InitializeCriticalSection(&csJob);

   // Register a blocking (message style) semaphore for signalling that process validation is required.

   if (plAllocPrivateSemaphore(&glValidationSemaphore, 1)) return ERR_Failed;

   glDeadProcessMsg = RegisterWindowMessage("RKL_DeadProcess");

   // Create a dummy window for receiving messages.

   WNDCLASSEX wx;
   ZeroMemory(&wx, sizeof(wx));
   wx.cbSize        = sizeof(WNDCLASSEX);
   wx.lpfnWndProc   = window_procedure;
   wx.hInstance     = glInstance;
   wx.lpszClassName = glMsgClass;
   if (RegisterClassEx(&wx)) {
      glMsgWindow = CreateWindowEx(0, glMsgClass, "Parasol",
         0, // WS flags
         0, 0, // Coordinates
         CW_USEDEFAULT, CW_USEDEFAULT,
         HWND_MESSAGE, (HMENU)NULL, glInstance, NULL);
   }

   return ERR_Okay;
}

//********************************************************************************************************************
// Platform specific semaphore handling functions.

LONG plAllocPrivateSemaphore(HANDLE *Semaphore, LONG InitialValue)
{
   SECURITY_ATTRIBUTES security = {
      .nLength = sizeof(SECURITY_ATTRIBUTES),
      .lpSecurityDescriptor = NULL,
      .bInheritHandle = FALSE
   };
   if (!(*Semaphore = CreateSemaphore(&security, 0, InitialValue, NULL))) return ERR_Failed;
   else return ERR_Okay;
}

void plFreePrivateSemaphore(HANDLE *Semaphore)
{
   if (*Semaphore) { CloseHandle(*Semaphore); *Semaphore = 0; }
}

LONG plLockSemaphore(HANDLE *Semaphore, LONG TimeOut)
{
   switch(WaitForSingleObject(*Semaphore, TimeOut)) {
      case WAIT_OBJECT_0:  return ERR_Okay;
      case WAIT_TIMEOUT:   return ERR_TimeOut;
      case WAIT_ABANDONED: return ERR_DoesNotExist;
      default: return ERR_SystemCall;
   }
}

void plUnlockSemaphore(HANDLE *Semaphore)
{
   LONG prev;
   ReleaseSemaphore(*Semaphore, 1, &prev);
}

//********************************************************************************************************************
// Broadcast a message saying that our process is dying.  Status should be 0 for an initial broadcast (closure is
// starting) and 1 if the process has finished and closed the Core cleanly.

void winDeathBringer(LONG Status)
{
   static LONG last_status = -1;
   if (Status > last_status) {
      last_status = Status;
      SendMessage(HWND_BROADCAST, glDeadProcessMsg, glProcessID, Status);
   }
}

//********************************************************************************************************************

LONG winIsDebuggerPresent(void)
{
   return IsDebuggerPresent();
}

//********************************************************************************************************************
// Remove all allocations here, called at the end of CloseCore()

void winShutdown(void)
{
   if (glValidationSemaphore) plFreePrivateSemaphore(&glValidationSemaphore);

   if (glMsgWindow) { DestroyWindow(glMsgWindow); glMsgWindow = 0; }
   UnregisterClass(glMsgClass, glInstance);

   EnterCriticalSection(&csHandleBank);
      WORD i;
      for (i=0; i < glHandleCount; i++) {
         if (glHandleBank[i].LocalHandle) CloseHandle(glHandleBank[i].LocalHandle);
      }
      glHandleCount = 0;
   LeaveCriticalSection(&csHandleBank);

   DeleteCriticalSection(&csHandleBank);
   DeleteCriticalSection(&csJob);
}

//********************************************************************************************************************
// Return a duplicate handle linked to some other process.  A cache is used so that re-duplication is minimised.

static HANDLE handle_cache(LONG OtherProcess, HANDLE OtherHandle, BYTE *Free)
{
   HANDLE result = 0;
   HANDLE foreignprocess;
   WORD i;

   *Free = FALSE;

   if ((OtherProcess IS glProcessID) OR (!OtherProcess)) return OtherHandle;

   EnterCriticalSection(&csHandleBank);

      for (i=0; i < glHandleCount; i++) { // Return the handle if it is already registered.
         if ((glHandleBank[i].OtherProcess IS OtherProcess) AND (glHandleBank[i].OtherHandle IS OtherHandle)) {
            result = glHandleBank[i].LocalHandle;
            LeaveCriticalSection(&csHandleBank);
            return result;
         }
      }

      if ((foreignprocess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, OtherProcess))) { // Duplicate the handle
         if ((DuplicateHandle(foreignprocess, OtherHandle, glProcessHandle, &result, 0, FALSE, DUPLICATE_SAME_ACCESS))) {
            if (glHandleCount < MAX_HANDLES) { // Store the handle in the cache.
               i = glHandleCount++;
               glHandleBank[i].OtherProcess = OtherProcess;
               glHandleBank[i].OtherHandle  = OtherHandle;
               glHandleBank[i].LocalHandle  = result;
            }
            else *Free = TRUE; // If the handle can't be cached, the caller needs to free it.
         }
         CloseHandle(foreignprocess);
      }

   LeaveCriticalSection(&csHandleBank);
   return result;
}

//********************************************************************************************************************
// Windows compatible locking allocation functions.  See lib_locking.c for the POSIX versions.

int alloc_public_lock(HANDLE *Lock, const char *Name)
{
   HANDLE mutex;

   if ((mutex = OpenMutex(SYNCHRONIZE, FALSE, Name))) {
      *Lock = mutex;
      return ERR_Okay;
   }

   if ((mutex = CreateMutex(NULL, FALSE, Name))) {
      *Lock = mutex;
      return ERR_Okay;
   }
   else return ERR_SystemCall;
}

int open_public_lock(HANDLE *Lock, const char *Name)
{
   HANDLE mutex;

   if ((mutex = OpenMutex(SYNCHRONIZE, FALSE, Name))) {
      *Lock = mutex;
      return ERR_Okay;
   }
   else return ERR_SystemCall;
}

void free_public_lock(HANDLE Lock)
{
   CloseHandle(Lock);
}

int public_thread_lock(HANDLE Lock, LONG TimeOut)
{
   if (TimeOut < 1) TimeOut = 1;

   switch(WaitForSingleObject(Lock, TimeOut)) {
      case WAIT_OBJECT_0:  return ERR_Okay;
      case WAIT_TIMEOUT:   return ERR_TimeOut;
      case WAIT_ABANDONED: return ERR_DoesNotExist;
      default: return ERR_SystemCall;
   }
}

void public_thread_unlock(HANDLE Lock)
{
   ReleaseMutex(Lock);
}

//********************************************************************************************************************
// The SysLock() function uses these publicly accessible handles for synchronising Core processes.

int alloc_public_waitlock(HANDLE *Lock, const char *Name)
{
#ifdef WAITLOCK_EVENTS
   HANDLE event;

   if (Name) {
      if ((event = OpenEvent(SYNCHRONIZE|EVENT_MODIFY_STATE, FALSE, Name))) {
         *Lock = event;
         return ERR_Okay;
      }
   }

   SECURITY_ATTRIBUTES sa;
   sa.nLength = sizeof(sa);
   sa.lpSecurityDescriptor = NULL;
   sa.bInheritHandle = FALSE;

   if ((event = CreateEvent(&sa, FALSE, FALSE, Name))) {
      *Lock = event;
      return ERR_Okay;
   }
   else return ERR_SystemCall;
#else
   SECURITY_ATTRIBUTES security;

   security.nLength = sizeof(SECURITY_ATTRIBUTES);
   security.lpSecurityDescriptor = NULL;
   security.bInheritHandle = FALSE;
   if ((*Lock = CreateSemaphore(&security, 0, 1, Name))) return ERR_Okay;
   else return ERR_SystemCall;
#endif
}

int open_public_waitlock(HANDLE *Lock, const char *Name)
{
   HANDLE event;

   if ((event = OpenEvent(SYNCHRONIZE|EVENT_MODIFY_STATE, FALSE, Name))) {
      *Lock = event;
      return ERR_Okay;
   }
   else return ERR_SystemCall;
}

void free_public_waitlock(HANDLE Lock)
{
   CloseHandle(Lock);
}

LONG wake_waitlock(HANDLE Lock, LONG TotalSleepers)
{
   if (!Lock) return ERR_NullArgs;

   LONG error = ERR_Okay;

   #ifdef WAITLOCK_EVENTS
      while (TotalSleepers-- > 0) {
         if (!SetEvent(Lock)) {
            char msg[100];
            fprintf(stderr, "SetEvent() failed: %s\n", winFormatMessage(GetLastError(), msg, sizeof(msg)));
            error = ERR_SystemCall;
            break;
         }
      }
   #else
      LONG prev;
      if (!ReleaseSemaphore(Lock, 1, &prev)) error = ERR_SystemCall;
   #endif

   return error;
}

LONG sleep_waitlock(HANDLE Handle, LONG Time)
{
   LONG result;

   if (Time IS -1) Time = INFINITE;

   result = WaitForSingleObject(Handle, Time);

   if (result IS WAIT_OBJECT_0) return 0;
   else if (result IS WAIT_TIMEOUT) return 1;
   else if (result IS WAIT_ABANDONED) return 2;
   else return 3;
}

//********************************************************************************************************************

#define MAX_LOCKS 32

static CRITICAL_SECTION locks[MAX_LOCKS];

int alloc_private_lock(UBYTE Index, int Flags)
{
   //MSG("alloc_private_lock(%d, $%x)\n", (int)Index, Flags);

   if ((Index >= 0) AND (Index < MAX_LOCKS)) {
      InitializeCriticalSection(&locks[Index]);
      return ERR_Okay;
   }
   else return ERR_OutOfBounds;
}

void free_private_lock(UBYTE Index)
{
   DeleteCriticalSection(&locks[Index]);
   ZeroMemory(&locks[Index], sizeof(locks[0]));
}

void winInitializeCriticalSection(APTR Lock)
{
   InitializeCriticalSection(Lock);
}

void winDeleteCriticalSection(APTR Lock)
{
   DeleteCriticalSection(Lock);
}

//********************************************************************************************************************

static CONDITION_VARIABLE conds[MAX_LOCKS];

int alloc_private_cond(UBYTE Index)
{
   //MSG("alloc_private_cond(%d)\n", (int)Index);

   if ((Index >= 0) AND (Index < MAX_LOCKS)) {
      InitializeConditionVariable(&conds[Index]);
      return ERR_Okay;
   }
   else return ERR_OutOfBounds;
}

void free_private_cond(UBYTE Index)
{
   // Deallocation of condition variables is not necessary on Windows.
   ZeroMemory(&conds[Index], sizeof(conds[0]));
}

/*********************************************************************************************************************
** Windows compatible locking management functions.
*/

int thread_lock(UBYTE Index, int Timeout)
{
   EnterCriticalSection(&locks[Index]); // Always succeeds (if it returns)
   return ERR_Okay;
}

void thread_unlock(UBYTE Index)
{
   LeaveCriticalSection(&locks[Index]);
}

// You must call this function with a thread-lock already established.

LONG cond_wait(UBYTE LockIndex, UBYTE CondIndex, LONG Timeout)
{
   BOOL result;
   if (Timeout IS -1) result = SleepConditionVariableCS(&conds[CondIndex], &locks[LockIndex], INFINITE);
   else result = SleepConditionVariableCS(&conds[CondIndex], &locks[LockIndex], Timeout);

   if (!result) {
      if (GetLastError() IS ERROR_TIMEOUT) return ERR_TimeOut;
      else return ERR_Failed;
   }
   else return ERR_Okay;
}

// A matching thread-lock is a good idea for this call.

void cond_wake_all(UBYTE CondIndex)
{
   WakeAllConditionVariable(&conds[CondIndex]); // Wake all threads waiting on the condition variable.
}

void cond_wake_single(UBYTE CondIndex)
{
   WakeConditionVariable(&conds[CondIndex]); // Wake only one thread waiting on the condition variable.
}

void winEnterCriticalSection(APTR Section)
{
   EnterCriticalSection(Section); // Critical sections may nest.
}

void winLeaveCriticalSection(APTR Section)
{
   LeaveCriticalSection(Section);
}

LONG winTryEnterCriticalSection(APTR Section)
{
   if (TryEnterCriticalSection(Section)) return ERR_Okay;
   else return ERR_Failed;
}

#ifdef __CYGWIN__
static int strnicmp(const char *s1, const char *s2, size_t n)
{
   for (; n > 0; s1++, s2++, --n) {
      unsigned char c1 = *s1;
      unsigned char c2 = *s2;
      if ((c1 >= 'A') || (c1 <= 'Z')) c1 = c1 - 'A' + 'a';
      if ((c2 >= 'A') || (c2 <= 'Z')) c2 = c2 - 'A' + 'a';

      if (c1 != c2) return ((*(unsigned char *)s1 < *(unsigned char *)s2) ? -1 : +1);
      else if (c1 == '\0') return 0;
   }
   return 0;
}
#endif

//********************************************************************************************************************

DWORD winGetExeDirectory(DWORD Length, unsigned char *String)
{
   LONG len, i;
   WCHAR **list;

   // Use GetModuleFileName() to get the path of our executable and then extract the folder location from that.

   if ((len = GetModuleFileName(NULL, String, Length))) {
      for (i=len; i > 0; i--) {
         if (String[i] IS '\\') {
            String[i+1] = 0;
            return i;
         }
      }
      return len;
   }

   // Attempt to get our .exe path using CommandLinetoArgv()

   int args;
   if ((list = CommandLineToArgvW(GetCommandLineW(), &args))) {
      wcstombs(String, list[0], Length);
      LocalFree(list);

      for (i=0; String[i]; i++);
      while (i > 0) {
         if ((String[i] == '/') || (String[i] == '\\')) {
            String[i+1] = 0;
            return i;
         }
         i--;
      }
   }

   // Windows has not prepended the path to the executable.  (Observed in Windows 7 64).  Try another method...

   if ((len = GetProcessImageFileNameA(GetCurrentProcess(), String, Length)) > 0) {
      char tmp[MAX_PATH] = "";

      if (GetLogicalDriveStrings(sizeof(tmp)-1, tmp)) {
         char devname[MAX_PATH];
         char szDrive[3] = " :";
         char *p = tmp;

         do {
            szDrive[0] = p[0];

            if (QueryDosDevice(szDrive, devname, sizeof(devname))) {
               int devlen = strlen(devname);
               if (strnicmp(String, devname, devlen) == devlen) {
                  if (String[devlen] == '\\') {
                     // Replace device path with DOS path
                     char tmpfile[MAX_PATH];
                     snprintf(tmpfile, MAX_PATH, "%s%s", szDrive, String+devlen);
                     strncpy(String, tmpfile, Length-1);
                     String[Length-1] = 0; // Enforce null termination on overflow
                     return strlen(String);
                  }
               }
            }

            while (*p++);
         } while (p[0]);
      }
   }

   // Last resort - use the current folder

   return GetCurrentDirectory(Length, String);
}

//********************************************************************************************************************
// Warning: DispatchMessage() can hang for modal operations.  An example of a modal operation is window resizing.  If
// necessary, this issue can be subverted with threading.
//
// See also: display/windows.c

void winProcessMessages(void)
{
   MSG msg;
   long long time = winGetTickCount() + 100000;
   ZeroMemory(&msg, sizeof(msg));
   while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
      if (winGetTickCount() > time) break; // This timer-break prevents any chance of infinite looping
   }
}

//********************************************************************************************************************

void winLowerPriority(void)
{
   SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
}

//********************************************************************************************************************

LONG winGetCurrentThreadId(void)
{
   return GetCurrentThreadId();
}

//********************************************************************************************************************
// A null-terminator will be included in the output buffer.  Returns 0 on error.

DWORD winGetCurrentDirectory(DWORD Length, unsigned char *String)
{
   return GetCurrentDirectory(Length, String);
}

//********************************************************************************************************************
// Checks if a process exists by attempting to open it.

LONG winCheckProcessExists(DWORD ProcessID)
{
   HANDLE process;

   if ((process = OpenProcess(STANDARD_RIGHTS_REQUIRED, FALSE, ProcessID))) {
      CloseHandle(process);
      return 1;
   }
   else return 0;
}

//********************************************************************************************************************

LONG winFreeLibrary(HMODULE Module)
{
   return FreeLibrary(Module);
}

//********************************************************************************************************************

HANDLE winLoadLibrary(LPCSTR Name)
{
   HANDLE h = LoadLibraryExA(Name, NULL, LOAD_LIBRARY_SEARCH_APPLICATION_DIR|LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_USER_DIRS);
   return h;
}

//********************************************************************************************************************

FARPROC winGetProcAddress(HMODULE Module, LPCSTR Name)
{
   return GetProcAddress(Module, Name);
}

//********************************************************************************************************************

HANDLE winGetCurrentProcess(void)
{
   return GetCurrentProcess();
}

//********************************************************************************************************************

LONG winGetCurrentProcessId(void)
{
   return GetCurrentProcessId();
}

//********************************************************************************************************************
// Returns a handle if successful, otherwise NULL.

HANDLE winOpenSemaphore(unsigned char *Name)
{
   return OpenSemaphore(SEMAPHORE_ALL_ACCESS, FALSE, Name);
}

//********************************************************************************************************************

static BYTE glConsoleMode = TRUE; // Assume running from a terminal by default.

LONG winReadStdInput(HANDLE FD, APTR Buffer, DWORD BufferSize, DWORD *Size)
{
   *Size = 0;

   if ((glConsoleMode) AND (ReadConsoleA(GetStdHandle(STD_INPUT_HANDLE), Buffer, BufferSize, Size, NULL))) {
      return 0; // Read at least 1 character
   }

   *Size = BufferSize;
   return winReadPipe(FD, Buffer, Size);
}

//********************************************************************************************************************

HANDLE winGetStdInput(void)
{
   static HANDLE in = 0;
   if (!in) {
      in = GetStdHandle(STD_INPUT_HANDLE);
      if (!SetConsoleMode(in, ENABLE_PROCESSED_INPUT)) {
         glConsoleMode = FALSE;
      }
   }
   return in;
}

//********************************************************************************************************************
// To be used on local handles only.

LONG winWaitForSingleObject(HANDLE Handle, LONG Time)
{
   if (Time IS -1) Time = INFINITE;

   LONG result = WaitForSingleObject(Handle, Time);

   if (result IS WAIT_OBJECT_0) return 0;
   else if (result IS WAIT_TIMEOUT) return 1;
   else if (result IS WAIT_ABANDONED) return 2; // WAIT_ABANDONED means that the wait was successful, but indicates that the previous process that held the lock crashed before releasing it.
   else return 3;
}

//********************************************************************************************************************
// Return Values:
//   -1 = Timeout
//   -2 = A handle has been abandoned/freed/is invalid.  Handles[0] will be updated to reflect the bad handle.
//   -3 = A message was received in the windows message queue.
//   -4 = Unknown result returned from windows.

LONG winWaitForObjects(LONG Total, HANDLE *Handles, LONG Time, BYTE WinMsgs)
{
   if (Time IS -1) Time = INFINITE;

   int input_flags = WinMsgs ? (QS_INPUT|QS_POSTMESSAGE|QS_TIMER|QS_PAINT|QS_HOTKEY|QS_SENDMESSAGE) : 0;

   LONG result = MsgWaitForMultipleObjects(Total, Handles, FALSE, Time, input_flags);

   if (result IS WAIT_TIMEOUT) return -1;
   else if ((result >= WAIT_ABANDONED_0) AND (result < WAIT_ABANDONED_0+Total)) {
      Handles[0] = Handles[result - WAIT_ABANDONED_0];
      return -2; // One of the handles has been abandoned (freed)
   }
   else if ((result >= WAIT_OBJECT_0) AND (result < WAIT_OBJECT_0+Total)) {
      return result - WAIT_OBJECT_0; // Result is the index of the signalled handle
   }
   else if (result IS WAIT_OBJECT_0+Total) {
      return -3; // A message was received in the windows message queue
   }
   else {
      DWORD error = GetLastError();
      if (error IS ERROR_INVALID_HANDLE) {
         Handles[0] = 0; // Find out which handle is to blame
         for (LONG i=0; i < Total; i++) {
            if (MsgWaitForMultipleObjects(1, Handles+i, FALSE, 1, (WinMsgs) ? QS_ALLINPUT : 0) IS result) {
               if (GetLastError() IS ERROR_INVALID_HANDLE) {
                  Handles[0] = Handles[i];
                  break;
               }
            }
         }
         return -2;
      }
      else {
         char msg[400];
         fprintf(stderr, "MsgWaitForMultipleObjects(%d) result: %d, error: %s\n", (int)Total, (int)result, winFormatMessage(error, msg, sizeof(msg)));
         return -4;
      }
   }
}

//********************************************************************************************************************

void winSleep(LONG Time)
{
   Sleep(Time);
}

//********************************************************************************************************************
// Retrieve the 'counts per second' value if this hardware supports a high frequency timer.  Otherwise use
// GetTickCount().

long long winGetTickCount(void)
{
   static LARGE_INTEGER freq;
   static BYTE init = 0;
   static long long start = 0;

   if (!init) {
      int r = QueryPerformanceFrequency(&freq);
      if (r) {
         init = 1;
         // Record a base-line so that we know we're starting from zero and not some arbitrarily large number.
         LARGE_INTEGER time;
         QueryPerformanceCounter(&time);
         start = time.QuadPart;
      }
      else init = -1; // Hardware does not support this feature.
   }

   if (init == 1) {
      LARGE_INTEGER time;
      QueryPerformanceCounter(&time); // Get tick count
      return (time.QuadPart - start) * 1000000LL / freq.QuadPart; // Convert ticks to microseconds, then divide by 'frequency counts per second'
   }
   else return GetTickCount() * 1000LL;
}

//********************************************************************************************************************
// Designed for reading from pipes.  Returns -1 on general error, -2 if the pipe is broken, e.g. child process is dead.

LONG winReadPipe(HANDLE FD, APTR Buffer, DWORD *Size)
{
   // Check if there is data available on the pipe

   DWORD avail = 0;
   if (!PeekNamedPipe(FD, NULL, 0, NULL, &avail, NULL)) {
      *Size = 0;
      if (GetLastError() == ERROR_BROKEN_PIPE) return -2;
      else return -1;
   }

   if (!avail) {
      *Size = 0;
      return 0;
   }

   if (ReadFile(FD, Buffer, *Size, Size, 0)) {
      return 0; // Success
   }
   else {
      *Size = 0;
      if (GetLastError() == ERROR_BROKEN_PIPE) return -2;
      else return -1;
   }
}

//********************************************************************************************************************
// WARNING: If it is not possible to write all data to the pipe, windows will block until the other side of the pipe
// has been read.  This does not match expected/documented functionality but there is no simple way to implement
// non-blocking anonymous pipes on windows.

LONG winWritePipe(HANDLE FD, APTR Buffer, DWORD *Size)
{
   if (WriteFile(FD, Buffer, *Size, Size, 0)) {
      return 0; // Success
   }
   else {
      if (GetLastError() == ERROR_BROKEN_PIPE) return -2;
      else return -1;
   }
}

//********************************************************************************************************************
// Used by class_thread.c only.

LONG winCreatePipe(HANDLE *Read, HANDLE *Write)
{
   SECURITY_ATTRIBUTES sa;

   sa.nLength = sizeof(sa);
   sa.lpSecurityDescriptor = NULL;
   sa.bInheritHandle = FALSE;

   if (CreatePipe(Read, Write, &sa, 0)) {
      return ERR_Okay;
   }
   else return ERR_Failed;
}

//********************************************************************************************************************
// Returns zero on failure.

LONG winCloseHandle(HANDLE Handle)
{
   if (Handle == (HANDLE)-1) return 1;
   return CloseHandle(Handle);
}

//********************************************************************************************************************
// Returns zero on failure.

LONG winUnmapViewOfFile(void *Address)
{
   return UnmapViewOfFile(Address);
}

//********************************************************************************************************************

long long winGetFileSize(char *Path)
{
   WIN32_FIND_DATA find;
   HANDLE handle;

   if ((handle = FindFirstFile(Path, &find)) IS INVALID_HANDLE_VALUE) {
      return 0;
   }

   long long size = (find.nFileSizeHigh * (MAXDWORD+1)) + find.nFileSizeLow;

   FindClose(handle);
   return size;
}

//********************************************************************************************************************

HANDLE glMemoryPool;

LONG winCreateSharedMemory(char *Name, LONG mapsize, LONG initial_size, HANDLE *ControlID, void **Address)
{
   // Create the shared memory area.  If it already exists, this call will not recreate the area, but link to it.

   if ((*ControlID = CreateFileMapping((HANDLE)-1, NULL, PAGE_READWRITE, 0, initial_size, Name))) {
      glMemoryPool = *ControlID;
      LONG init = (GetLastError() != ERROR_ALREADY_EXISTS) ? 1 : 0;
      if ((*Address = MapViewOfFile(*ControlID, FILE_MAP_WRITE, 0, 0, initial_size))) {
         return init;
      }
      else return -2;
   }
   else return -1;
}

//********************************************************************************************************************

int winDeleteFile(const char *Path)
{
   return DeleteFile(Path);
}

//********************************************************************************************************************

int winGetEnv(const char *Name, char *Buffer, int Size)
{
   return GetEnvironmentVariable(Name, Buffer, Size);
}

//********************************************************************************************************************

int winSetEnv(const char *Name, const char *Value)
{
   return SetEnvironmentVariable(Name, Value);
}

//********************************************************************************************************************

void winTerminateThread(HANDLE Handle)
{
   TerminateThread(Handle, 0);
}

//********************************************************************************************************************

LONG winWaitThread(HANDLE Handle, LONG TimeOut)
{
   if (WaitForSingleObject(Handle, TimeOut) IS WAIT_TIMEOUT) return ERR_TimeOut;
   else return ERR_Okay;
}

//********************************************************************************************************************

static BOOL break_handler(DWORD CtrlType)
{
   if (glBreakHandler) glBreakHandler();

   return FALSE;
}

//********************************************************************************************************************

void winSetUnhandledExceptionFilter(LONG (*Function)(LONG, APTR, LONG, APTR))
{
   if (Function) glCrashHandler = Function;
   else if (!glCrashHandler) return;  // If we're set with NULL and no crash handler already exists, do not set or change the exception filter.
   SetUnhandledExceptionFilter((LPTOP_LEVEL_EXCEPTION_FILTER)&ExceptionFilter);
}

//********************************************************************************************************************
// https://docs.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-exception_record

LONG ExceptionFilter(LPEXCEPTION_POINTERS Args)
{
   LONG continuable, code, err;

   #ifdef DEBUG
   if (Args->ExceptionRecord->ExceptionCode != EXCEPTION_STACK_OVERFLOW) {
      windows_print_stacktrace(Args->ContextRecord);
   }
   #endif

   if (Args->ExceptionRecord->ExceptionFlags & EXCEPTION_NONCONTINUABLE) {
      continuable = FALSE;
   }
   else continuable = TRUE;

   if (Args->ExceptionRecord->ExceptionCode IS EXCEPTION_NONCONTINUABLE_EXCEPTION) return EXCEPTION_CONTINUE_SEARCH;

   switch(Args->ExceptionRecord->ExceptionCode) {
      case EXCEPTION_ACCESS_VIOLATION:       code = EXP_ACCESS_VIOLATION; break;
      case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:  code = EXP_ACCESS_VIOLATION; break;
      case EXCEPTION_BREAKPOINT:             code = EXP_BREAKPOINT; break;
      case EXCEPTION_DATATYPE_MISALIGNMENT:  code = EXP_MISALIGNED_DATA; break;
      case EXCEPTION_FLT_DENORMAL_OPERAND:   code = EXP_INVALID_CALCULATION; break;
      case EXCEPTION_FLT_DIVIDE_BY_ZERO:     code = EXP_DIVIDE_BY_ZERO; break;
      case EXCEPTION_FLT_INEXACT_RESULT:     code = EXP_INVALID_CALCULATION; break;
      case EXCEPTION_FLT_INVALID_OPERATION:  code = EXP_INVALID_CALCULATION; break;
      case EXCEPTION_FLT_OVERFLOW:           code = EXP_INVALID_CALCULATION; break;
      case EXCEPTION_FLT_STACK_CHECK:        code = EXP_INVALID_CALCULATION; break;
      case EXCEPTION_FLT_UNDERFLOW:          code = EXP_INVALID_CALCULATION; break;
      case EXCEPTION_ILLEGAL_INSTRUCTION:    code = EXP_ILLEGAL_INSTRUCTION; break;
      case EXCEPTION_IN_PAGE_ERROR:          code = EXP_ACCESS_VIOLATION; break;
      case EXCEPTION_INT_DIVIDE_BY_ZERO:     code = EXP_DIVIDE_BY_ZERO; break;
      case EXCEPTION_INT_OVERFLOW:           code = EXP_INVALID_CALCULATION; break;
      case EXCEPTION_PRIV_INSTRUCTION:       code = EXP_ILLEGAL_INSTRUCTION; break;
      case EXCEPTION_STACK_OVERFLOW:         code = EXP_STACK_OVERFLOW; break;
      default: code = 0;
   }

   if (glCrashHandler) {
      err = glCrashHandler(code, Args->ExceptionRecord->ExceptionAddress, continuable, Args->ExceptionRecord->ExceptionInformation);

      if (err IS 0) return EXCEPTION_CONTINUE_EXECUTION;
      else if (err IS 1) return EXCEPTION_CONTINUE_SEARCH;
      else return EXCEPTION_EXECUTE_HANDLER;
   }
   else return EXCEPTION_EXECUTE_HANDLER;
}

// Sockets can use select() to differentiate between read/write states

void winSelect(int FD, char *Read, char *Write)
{
   fd_set fread, fwrite;
   struct timeval tv;

   FD_ZERO(&fread);
   FD_ZERO(&fwrite);

   if (*Read) FD_SET(FD, &fread);
   if (*Write) FD_SET(FD, &fwrite);

   tv.tv_sec = 0;
   tv.tv_usec = 0;
   select(0, &fread, &fwrite, 0, &tv);

   *Read = FD_ISSET(FD, &fread);
   *Write = FD_ISSET(FD, &fwrite);
}

static BOOL CALLBACK TerminateAppEnum( HWND hwnd, LPARAM lParam ) ;

int winTerminateApp(int dwPID, int dwTimeout)
{
   HANDLE hProc ;
   int dwRet ;

   // If we can't open the process with PROCESS_TERMINATE rights, then we give up immediately.

   hProc = OpenProcess(SYNCHRONIZE|PROCESS_TERMINATE, FALSE, dwPID);

   if (hProc IS NULL) return ERR_Failed;

   // TerminateAppEnum() posts WM_CLOSE to all windows whose PID matches your process's.

   EnumWindows((WNDENUMPROC)TerminateAppEnum, (LPARAM) dwPID) ;

   // Wait on the handle. If it signals, great. If it times out, then you kill it.

   if (WaitForSingleObject(hProc, dwTimeout) != WAIT_OBJECT_0) {
      dwRet = (TerminateProcess(hProc,0) ? ERR_Okay : ERR_Failed);
   }
   else dwRet = ERR_Okay;

   CloseHandle(hProc);
   return dwRet;
}

static BOOL CALLBACK TerminateAppEnum( HWND hwnd, LPARAM lParam )
{
   DWORD dwID;

   GetWindowThreadProcessId(hwnd, &dwID);

   if (dwID IS (DWORD)lParam) {
      PostMessage(hwnd, WM_CLOSE, 0, 0);
   }

   return TRUE;
}

//********************************************************************************************************************

extern int ProcessMessages(int Flags, int Timeout);

static LRESULT CALLBACK window_procedure(HWND window, UINT msgcode, WPARAM wParam, LPARAM lParam)
{
   if (glProgramStage IS STAGE_SHUTDOWN) return DefWindowProc(window, msgcode, wParam, lParam);

   if (msgcode IS glDeadProcessMsg) {
      validate_process(wParam);
      return 0;
   }
   else return DefWindowProc(window, msgcode, wParam, lParam);
}

//********************************************************************************************************************

int AllocMutex(int Flags, CRITICAL_SECTION **Result)
{
   CRITICAL_SECTION *cs;
   if (!Result) return ERR_NullArgs;

   //if (!(Flags & ALF_RECURSIVE)) {
      // TODO: Non-recursive mutexes can be supported by using InitialiseSRWLock(), AcquireSRWLockExclusive(), etc...
   //}

   if ((cs = calloc(1, sizeof(CRITICAL_SECTION)))) {
      InitializeCriticalSection(cs);
      *Result = cs;
      return ERR_Okay;
   }
   else return ERR_AllocMemory;
}

//********************************************************************************************************************

void FreeMutex(void *Mutex)
{
   if (Mutex) {
      DeleteCriticalSection(Mutex);
      free(Mutex);
   }
}

//********************************************************************************************************************
// Calls to TryEnterCriticalSection() will nest.

int LockMutex(void *Mutex, int MilliSeconds)
{
   if (!Mutex) return ERR_NullArgs;
   if (MilliSeconds IS 0) {
      if (!TryEnterCriticalSection(Mutex)) return ERR_Okay; // May nest.
      else return ERR_TimeOut;
   }
   else EnterCriticalSection(Mutex); // Always succeeds if it returns.  May nest
   return ERR_Okay;
}

//********************************************************************************************************************

void UnlockMutex(void *Mutex)
{
   if (Mutex) LeaveCriticalSection(Mutex);
}

//********************************************************************************************************************

int LockSharedMutex(void *Mutex, int Timeout)
{
   if (Timeout < 1) Timeout = 1;

   switch (WaitForSingleObject(Mutex, Timeout)) { // Will return WAIT_OBJECT_0 if ownership was granted.
      case WAIT_OBJECT_0:  return ERR_Okay;
      case WAIT_TIMEOUT:   return ERR_TimeOut;
      case WAIT_ABANDONED: return ERR_DoesNotExist;
      default: return ERR_SystemCall;
   }
}

//********************************************************************************************************************

void UnlockSharedMutex(APTR Mutex)
{
   ReleaseMutex(Mutex);
}

//********************************************************************************************************************

#include "processes.c"

//********************************************************************************************************************
// Called on file system initialisation to create assignments specific to windows.

void winEnumSpecialFolders(void (*enumfolder)(char *, char *, char *, char *, char))
{
   static const struct {
      int id;
      char *assign;
      char *label;
      char *icon;
      char hidden;
   } folders[] = {
      //CSIDL_NETWORK
      //CSIDL_PRINTHOOD
      //CSIDL_DESKTOPDIRECTORY
      //{ CSIDL_PRINTERS, "printers:",  "Printers",        "devices/printer" },
      //{ CSIDL_DRIVES,   "computer:",  "My Computer",     "programs/filemanager" }

      { CSIDL_NETHOOD,  "network:",   "Network Places", "devices/network", 0 },
      { CSIDL_PERSONAL, "documents:", "Documents",      "office/documents", 0 },
      { CSIDL_DESKTOPDIRECTORY, "desktop:", "Desktop",  "devices/harddisk", 0 }
   };
   char path[MAX_PATH];
   int i;

   for (i=0; i < sizeof(folders)/sizeof(folders[0]); i++) {
      if (SHGetFolderPath(NULL, folders[i].id, NULL, 0, path) IS S_OK) {
         enumfolder(folders[i].assign, folders[i].label, path, folders[i].icon, folders[i].hidden);
      }
   }

   if (GetTempPath(sizeof(path), path) < sizeof(path)) {
      enumfolder("HostTemp:", "Temp", path, "items/trash", 1);
   }
}

//********************************************************************************************************************

LONG winGetFullPathName(const char *Path, LONG PathLength, char *Output, char **NamePart)
{
   return GetFullPathName(Path, PathLength, Output, NamePart);
}

//********************************************************************************************************************

char * winFormatMessage(LONG Error, char *Buffer, LONG BufferSize)
{
   DWORD i = FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, 0, Error ? Error : GetLastError(), 0, Buffer, BufferSize, NULL);
   while ((i > 0) AND (Buffer[i-1] <= 0x20)) i--; // Windows puts whitespace at the end of error strings for some reason
   Buffer[i] = 0;
   return Buffer;
}

static void printerror(void)
{
   LPVOID lpMsgBuf;
   FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL,
      GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR)&lpMsgBuf, 0, NULL);
   fprintf(stderr, "WinError: %s", (LPTSTR)lpMsgBuf);
   LocalFree(lpMsgBuf);
}

//********************************************************************************************************************

BYTE winGetCommand(char *Path, char *Buffer, LONG BufferSize)
{
   if (BufferSize < MAX_PATH+3) return 1;

   HINSTANCE result = FindExecutable(Path, NULL, Buffer+1);
   if ((result > (HINSTANCE)32) AND (Buffer[1])) { /* Success */
      *Buffer++ = '"';
      while (*Buffer) Buffer++;
      *Buffer++ = '"';
      *Buffer = 0;
      return 0;
   }
   else return 1;
}

//********************************************************************************************************************

LONG winCurrentDirectory(char *Buffer, LONG BufferSize)
{
   WORD i, len;

   Buffer[0] = 0;
   if ((len = GetModuleFileNameA(NULL, Buffer, BufferSize))) {
      for (i=len; i > 0; i--) {
         if (Buffer[i] IS '\\') {
            Buffer[i+1] = 0;
            break;
         }
      }
   }

   // If GetModuleFileName() failed, try GetCurrentDirectory()

   if (!Buffer[0]) GetCurrentDirectoryA(BufferSize, Buffer);

   if (Buffer[0]) return 1;
   else return 0;
}

//********************************************************************************************************************

static void convert_time(FILETIME *Source, struct DateTime *Dest)
{
   FILETIME filetime;
   SYSTEMTIME time;

   FileTimeToLocalFileTime(Source, &filetime);
   if (FileTimeToSystemTime(&filetime, &time)) {
      Dest->Year   = time.wYear;
      Dest->Month  = time.wMonth;
      Dest->Day    = time.wDay;
      Dest->Hour   = time.wHour;
      Dest->Minute = time.wMinute;
      Dest->Second = time.wSecond;
   }
}

//********************************************************************************************************************

int winGetFileAttributesEx(const char *Path, BYTE *Hidden, BYTE *ReadOnly, BYTE *Archive, BYTE *Folder, LARGE *Size,
   struct DateTime *LastWrite, struct DateTime *LastAccess, struct DateTime *LastCreate)
{
   WIN32_FILE_ATTRIBUTE_DATA info;

   if (!GetFileAttributesEx(Path, 0, &info)) return ERR_Failed;

   if (info.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) *Hidden = TRUE;
   else *Hidden = FALSE;

   if (info.dwFileAttributes & FILE_ATTRIBUTE_READONLY) *ReadOnly = TRUE;
   else *ReadOnly = FALSE;

   if (info.dwFileAttributes & FILE_ATTRIBUTE_ARCHIVE) *Archive = TRUE;
   else *Archive = FALSE;

   if (info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
      *Folder = TRUE;
      *Size = 0;
   }
   else {
      *Folder = FALSE;
      *Size = (info.nFileSizeHigh * (MAXDWORD+1)) + info.nFileSizeLow;
   }

   if (LastWrite) convert_time(&info.ftLastWriteTime, LastWrite);
   if (LastAccess) convert_time(&info.ftLastWriteTime, LastAccess);
   if (LastCreate) convert_time(&info.ftLastWriteTime, LastCreate);

   return ERR_Okay;
}

//********************************************************************************************************************

int winCreateDir(const char *Path)
{
   int result = CreateDirectory(Path, NULL);
   if (result) return ERR_Okay;
   else {
       result = GetLastError();
       if (result IS ERROR_ALREADY_EXISTS) return ERR_FileExists;
       else if (result IS ERROR_PATH_NOT_FOUND) return ERR_FileNotFound;
       else return ERR_Failed;
   }
}

//********************************************************************************************************************
// Returns TRUE on success.

LONG winGetFreeDiskSpace(unsigned char Drive, long long *TotalSpace, long long *BytesUsed)
{
   DWORD sectors, bytes_per_sector, free_clusters, total_clusters;
   unsigned char location[4];

   *TotalSpace = 0;
   *BytesUsed = 0;

   location[0] = Drive;
   location[1] = ':';
   location[2] = '\\';
   location[3] = 0;

   if (GetDiskFreeSpace(location, &sectors, &bytes_per_sector, &free_clusters, &total_clusters)) {
      *TotalSpace = (double)sectors * (double)bytes_per_sector * (double)free_clusters;
      *BytesUsed  = ((double)sectors * (double)bytes_per_sector * (double)total_clusters);
      return 1;
   }
   else return 0;
}

//********************************************************************************************************************
// This function retrieves the original Creation date of the file and applies it to the Modification and Access date/times.

LONG winResetDate(STRING Location)
{
   HANDLE handle;

   if ((handle = CreateFile(Location, GENERIC_WRITE, FILE_SHARE_READ|FILE_SHARE_WRITE, NULL,
         OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL)) != INVALID_HANDLE_VALUE) {

      FILETIME filetime;
      GetFileTime(handle, &filetime, NULL, NULL);
      LONG err = SetFileTime(handle, NULL, &filetime, &filetime);
      CloseHandle(handle);
      if (err) return 1;
   }

   return 0;
}

//********************************************************************************************************************

void winFindNextChangeNotification(HANDLE Handle)
{
   FindNextChangeNotification(Handle);
}

//********************************************************************************************************************

void winFindCloseChangeNotification(HANDLE Handle)
{
   FindCloseChangeNotification(Handle);
}

//********************************************************************************************************************

LONG winGetWatchBufferSize(void)
{
   return sizeof(OVERLAPPED) + sizeof(FILE_NOTIFY_INFORMATION) + MAX_PATH;
}

//********************************************************************************************************************

void winSetDllDirectory(LPCSTR Path)
{
   SetDllDirectoryA(Path);
}

//********************************************************************************************************************

LONG winWatchFile(LONG Flags, CSTRING Path, APTR WatchBuffer, HANDLE *Handle, LONG *WinFlags)
{
   if ((!Path) OR (!Path[0])) return ERR_Args;

   LONG nflags = 0;
   if (Flags & MFF_READ) nflags |= FILE_NOTIFY_CHANGE_LAST_ACCESS;
   if (Flags & MFF_MODIFY) nflags |= FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_SIZE;
   if (Flags & MFF_CREATE) nflags |= FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME;
   if (Flags & MFF_DELETE) nflags |= FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME;
   if (Flags & MFF_OPENED) nflags |= FILE_NOTIFY_CHANGE_LAST_ACCESS;
   if (Flags & MFF_ATTRIB) nflags |= FILE_NOTIFY_CHANGE_SECURITY | FILE_NOTIFY_CHANGE_CREATION | FILE_NOTIFY_CHANGE_ATTRIBUTES;
   if (Flags & MFF_CLOSED) nflags |= 0;
   if (Flags & (MFF_MOVED|MFF_RENAME)) nflags |= FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME;

   if (nflags) {
      LONG i;
      char strip_path[MAX_PATH];
      for (i=0; (Path[i]) AND (i < MAX_PATH); i++) strip_path[i] = Path[i];
      if (strip_path[i-1] IS '\\') strip_path[i-1] = 0;

      *Handle = CreateFile(strip_path, FILE_LIST_DIRECTORY, FILE_SHARE_READ|FILE_SHARE_DELETE|FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
         FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, NULL);

      if (*Handle != INVALID_HANDLE_VALUE) {
         // Use ReadDirectoryChanges() to setup an asynchronous monitor on the target folder.

         OVERLAPPED *ovlap = WatchBuffer;
         FILE_NOTIFY_INFORMATION *fni = (FILE_NOTIFY_INFORMATION *)(ovlap + 1);

         LONG empty;
         if (!ReadDirectoryChangesW(*Handle, fni,
               sizeof(FILE_NOTIFY_INFORMATION) + MAX_PATH - 1, // The -1 is to give us room to impose a null byte
               TRUE, nflags, &empty, ovlap, NULL)) {

            CloseHandle(*Handle);
            *Handle = NULL;
            return ERR_SystemCall;
         }
      }

      *WinFlags = nflags;
      return ERR_Okay;
   }
   else return ERR_NoSupport;
}

//********************************************************************************************************************

LONG winReadChanges(HANDLE Handle, APTR WatchBuffer, LONG NotifyFlags, char *PathOutput, LONG PathSize, LONG *Status)
{
   DWORD bytes_out;
   OVERLAPPED *ovlap = WatchBuffer;
   FILE_NOTIFY_INFORMATION *fni = (FILE_NOTIFY_INFORMATION *)(ovlap + 1);

   if (GetOverlappedResult(Handle, ovlap, &bytes_out, FALSE)) {
      if (fni->Action) {
         ((char *)fni)[bytes_out] = 0; // Ensure path is null terminated
         wcstombs(PathOutput, fni->FileName, PathSize); // Convert to UTF8

         if (fni->Action IS FILE_ACTION_ADDED) *Status = MFF_CREATE;
         else if (fni->Action IS FILE_ACTION_REMOVED) *Status = MFF_DELETE;
         else if (fni->Action IS FILE_ACTION_MODIFIED) *Status = MFF_MODIFY|MFF_ATTRIB;
         else if (fni->Action IS FILE_ACTION_RENAMED_OLD_NAME) *Status = MFF_MOVED;
         else if (fni->Action IS FILE_ACTION_RENAMED_NEW_NAME) *Status = MFF_MOVED;
         else *Status = 0;

         fni->Action = 0;

         // Re-subscription is required to receive the next queued notification

         LONG empty;
         ReadDirectoryChangesW(Handle, fni, sizeof(FILE_NOTIFY_INFORMATION) + MAX_PATH - 1, TRUE, NotifyFlags, &empty, ovlap, NULL);

         return ERR_Okay;
      }
      else return ERR_NothingDone;
   }
   else return ERR_NothingDone;
}

//********************************************************************************************************************

LONG winSetFileTime(STRING Location, WORD Year, WORD Month, WORD Day, WORD Hour, WORD Minute, WORD Second)
{
   SYSTEMTIME time;
   FILETIME filetime, localtime;
   HANDLE handle;
   LONG err;

   if ((handle = CreateFile(Location, GENERIC_WRITE, FILE_SHARE_READ|FILE_SHARE_WRITE, NULL,
         OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL)) != INVALID_HANDLE_VALUE) {
      time.wYear         = Year;
      time.wMonth        = Month;
      time.wDayOfWeek    = 0;
      time.wDay          = Day;
      time.wHour         = Hour;
      time.wMinute       = Minute;
      time.wSecond       = Second;
      time.wMilliseconds = 0;

      SystemTimeToFileTime(&time, &localtime);
      LocalFileTimeToFileTime(&localtime, &filetime);

      err = SetFileTime(handle, &filetime, &filetime, &filetime);
      CloseHandle(handle);
      if (err) return 1;
   }

   return 0;
}

//********************************************************************************************************************

void winFindClose(HANDLE Handle)
{
   FindClose(Handle);
}

//********************************************************************************************************************

LONG winReadKey(LPBYTE Key, LPBYTE Value, LPBYTE Buffer, LONG Length)
{
   HKEY handle;
   LONG err = 0;
   DWORD length = Length;
   if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, Key, 0, KEY_READ, &handle) == ERROR_SUCCESS) {
      if (RegQueryValueEx(handle, Value, 0, 0, Buffer, &length) == ERROR_SUCCESS) {
         err = length-1;
      }
      CloseHandle(handle);
   }
   return err;
}

//********************************************************************************************************************

LONG winReadRootKey(LPBYTE Key, LPBYTE Value, LPBYTE Buffer, LONG Length)
{
   HKEY handle;
   LONG err = 0;
   DWORD length = Length;
   if (RegOpenKeyEx(HKEY_CLASSES_ROOT, Key, 0, KEY_READ, &handle) == ERROR_SUCCESS) {
      if (RegQueryValueEx(handle, Value, 0, 0, Buffer, &length) == ERROR_SUCCESS) {
         err = length-1;
      }
      CloseHandle(handle);
   }
   return err;
}

//********************************************************************************************************************

LONG winGetUserName(STRING Buffer, LONG Length)
{
   DWORD len = Length;
   return GetUserName(Buffer, &len);
}

//********************************************************************************************************************

LONG winGetUserFolder(STRING Buffer, LONG Size)
{
   LPITEMIDLIST list;
   char path[MAX_PATH];
   LONG i = 0;
   if (SHGetSpecialFolderLocation(NULL, CSIDL_APPDATA, &list) == NOERROR) {
      if (SHGetPathFromIDList(list, path)) {
         for (i=0; (i < Size-1) AND (path[i]); i++) Buffer[i] = path[i];
         if (Buffer[i-1] != '\\') Buffer[i++] = '\\';
         Buffer[i] = 0;
      }

      LPMALLOC malloc;
      if (SHGetMalloc(&malloc) IS NOERROR) { // An awkward (but necessary) method for freeing folder string
         malloc->lpVtbl->Free(malloc, list);
         malloc->lpVtbl->Release(malloc);
      }
   }
   return i;
}

//********************************************************************************************************************

LONG winMoveFile(STRING oldname, STRING newname)
{
   return MoveFile(oldname, newname);
}

//********************************************************************************************************************

LONG winSetEOF(CSTRING Location, __int64 Size)
{
   HANDLE handle;
   LARGE_INTEGER li;

   li.QuadPart = Size;
   if ((handle = CreateFile(Location, GENERIC_READ|GENERIC_WRITE, FILE_SHARE_READ|FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL)) != INVALID_HANDLE_VALUE) {
      li.LowPart = SetFilePointer(handle, li.LowPart, &li.HighPart, FILE_BEGIN);

      if (li.LowPart == INVALID_SET_FILE_POINTER && GetLastError() != NO_ERROR) {
         printerror();
      }
      else if (SetEndOfFile(handle)) {
         CloseHandle(handle);
         return 1;
      }
      else printerror();

      CloseHandle(handle);
   }
   else printerror();
   return 0;
}

//********************************************************************************************************************

LONG winGetLogicalDrives(void)
{
   return GetLogicalDrives();
}

//********************************************************************************************************************

LONG winGetLogicalDriveStrings(STRING Buffer, LONG Length)
{
   return GetLogicalDriveStrings(Length, Buffer);
}

//********************************************************************************************************************

LONG winGetDriveType(STRING Name)
{
   LONG flags = GetDriveType(Name);

   if (flags IS DRIVE_CDROM) return DRIVETYPE_CDROM;
   else if (flags IS DRIVE_FIXED) return DRIVETYPE_FIXED;
   else if (flags IS DRIVE_REMOVABLE) return DRIVETYPE_REMOVABLE;
   else if (flags IS DRIVE_REMOTE) return DRIVETYPE_NETWORK;
   else return 0;
}

//********************************************************************************************************************

LONG winTestLocation(STRING Location, BYTE CaseSensitive)
{
   LONG len, result;
   HANDLE handle;
   WIN32_FIND_DATA find;
   char save;
   int i, savepos;

   for (len=0; Location[len]; len++);
   if ((Location[len-1] IS '/') OR (Location[len-1] IS '\\')) {

      if (len IS 3) {
         // Checking for the existence of a drive letter - does not necessarily mean that there is media in the device.

         char volname[60], fsname[40];
         DWORD volserial, maxcomp, fileflags;

         if (GetVolumeInformation(Location, volname, sizeof(volname), &volserial, &maxcomp, &fileflags, fsname, sizeof(fsname))) {
            return LOC_DIRECTORY;
         }
         else return 0;
      }
      else {
         // We have been asked to check for the explicit existence of a folder.

         result = 0;
         savepos = len-1;
         save = Location[savepos];
         Location[savepos] = 0; // Remove the trailing slash
         if ((handle = FindFirstFile(Location, &find)) != INVALID_HANDLE_VALUE) {
            if (find.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) result = LOC_DIRECTORY;
            else while (FindNextFile(handle, &find)) {
               if (find.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                  result = LOC_DIRECTORY;
                  break;
               }
            }
            FindClose(handle);
         }

         if (CaseSensitive) {
            // Check that the filename of the given location matches that of the actual name set on the file system.

            len--;
            while ((len > 0) AND (Location[len-1] != '/') AND (Location[len-1] != '\\')) len--;
            for (i=0; (Location[len+i] IS find.cFileName[i]) AND (find.cFileName[i]) AND (Location[len+i]); i++);
            if ((!Location[len+i]) AND (!find.cFileName[i])) return result; // Match
            else return 0; // Not a case sensitive match
         }

         Location[savepos] = save;
      }

      return result;
   }
   else if ((handle = FindFirstFile(Location, &find)) != INVALID_HANDLE_VALUE) {
      if (find.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
         result = LOC_DIRECTORY;
      }
      else result = LOC_FILE;

      FindClose(handle);

      if (CaseSensitive) {
         // Check that the filename of the given location matches that of the actual name set on the file system.

         while ((len > 0) AND (Location[len-1] != '/') AND (Location[len-1] != '\\')) len--;
         for (i=0; (Location[len+i] IS find.cFileName[i]) AND (find.cFileName[i]) AND (Location[len+i]); i++);
         if ((!Location[len+i]) AND (!find.cFileName[i])) return result; /* Match */
         else return 0; /* Not a case sensitive match */
      }

      return result;
   }
   else return 0;
}

//********************************************************************************************************************

LONG delete_tree(STRING Path, int Size, struct FileFeedback *Feedback)
{
   LONG len, cont, i, attrib;
   WIN32_FIND_DATA find;
   HANDLE handle;

   if (Feedback) {
      Feedback->Path = Path;
      i = CALL_FEEDBACK(Feedback);
      if (i IS FFR_ABORT) return ERR_Cancelled;
      else if (i IS FFR_SKIP) return ERR_Okay;
   }

   for (len=0; Path[len]; len++);
   Path[len] = '\\';
   Path[len+1] = '*';
   Path[len+2] = 0;

   if ((handle = FindFirstFile(Path, &find)) != INVALID_HANDLE_VALUE) {
      cont = 1;
      while (cont) {
         if ((find.cFileName[0] IS '.') AND (find.cFileName[1] IS 0));
         else if ((find.cFileName[0] IS '.') AND (find.cFileName[1] IS '.') AND (find.cFileName[2] IS 0));
         else {
            for (i=0; find.cFileName[i]; i++) Path[len+1+i] = find.cFileName[i];
            Path[len+1+i] = 0;

            if (find.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
               delete_tree(Path, Size, Feedback);
            }
            else {
               if (find.dwFileAttributes & FILE_ATTRIBUTE_READONLY) {
                  find.dwFileAttributes &= ~FILE_ATTRIBUTE_READONLY;
                  SetFileAttributes(Path, find.dwFileAttributes);
               }

               unlink(Path); // Delete a file or empty folder
            }
         }

         cont = FindNextFile(handle, &find);
      }

      FindClose(handle);
   }

   Path[len] = 0;

   // Remove the file/folder itself - first check if it is read-only

   attrib = GetFileAttributes(Path);
   if (attrib != INVALID_FILE_ATTRIBUTES) {
      if (attrib & FILE_ATTRIBUTE_READONLY) {
         attrib &= ~FILE_ATTRIBUTE_READONLY;
         SetFileAttributes(Path, attrib);
      }
   }

   if (attrib & FILE_ATTRIBUTE_DIRECTORY) {
      if (RemoveDirectory(Path)) return ERR_Okay;
      else return ERR_Failed;
   }
   else if (!unlink(Path)) {
      return ERR_Okay;
   }
   else {
      #ifdef __CYGWIN__
      return convert_errno(*__errno(), ERR_Failed);
      #else
      return convert_errno(errno, ERR_Failed);
      #endif
   }
}

//********************************************************************************************************************

HANDLE winFindDirectory(STRING Location, HANDLE *Handle, STRING Result)
{
   WIN32_FIND_DATA find;
   LONG i;

   if (*Handle) {
      while (FindNextFile(*Handle, &find)) {
         if (find.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            for (i=0; find.cFileName[i]; i++) Result[i] = find.cFileName[i];
            Result[i] = 0;
            return *Handle;
         }
      }

      FindClose(*Handle);
   }
   else if ((*Handle = FindFirstFile(Location, &find)) != INVALID_HANDLE_VALUE) {
      do {
         if (find.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            for (i=0; find.cFileName[i]; i++) Result[i] = find.cFileName[i];
            Result[i] = 0;
            return *Handle;
         }
      } while (FindNextFile(*Handle, &find));

      FindClose(*Handle);
   }

   *Handle = NULL;
   return NULL;
}

//********************************************************************************************************************

HANDLE winFindFile(CSTRING Location, HANDLE *Handle, STRING Result)
{
   WIN32_FIND_DATA find;
   LONG i;

   if (*Handle) {
      while (FindNextFile(*Handle, &find)) {
         if (!(find.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            for (i=0; find.cFileName[i]; i++) Result[i] = find.cFileName[i];
            Result[i] = 0;
            return *Handle;
         }
      }
      FindClose(*Handle);
   }
   else if ((*Handle = FindFirstFile(Location, &find)) != INVALID_HANDLE_VALUE) {
      do {
         if (!(find.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            for (i=0; find.cFileName[i]; i++) Result[i] = find.cFileName[i];
            Result[i] = 0;
            return *Handle;
         }
      } while (FindNextFile(*Handle, &find));

      FindClose(*Handle);
   }

   *Handle = NULL;
   return NULL;
}

/*********************************************************************************************************************
** Function: winScan()
** Short:    Used by fs_scandir()
*/

LONG winScan(HANDLE *Handle, CSTRING Path, STRING Name, long long *Size, struct DateTime *CreateTime, struct DateTime *WriteTime, BYTE *Dir, BYTE *Hidden, BYTE *ReadOnly, BYTE *Archive)
{
   WIN32_FIND_DATA find;
   LONG i;

   while (1){
      if (*Handle IS (HANDLE)-1) {
         *Handle = FindFirstFile(Path, &find);
         if (*Handle IS INVALID_HANDLE_VALUE) return 0;
      }
      else if (!FindNextFile(*Handle, &find)) return 0;

      if ((find.cFileName[0] IS '.') AND (find.cFileName[1] IS 0)) continue;
      if ((find.cFileName[0] IS '.') AND (find.cFileName[1] IS '.') AND (find.cFileName[2] IS 0)) continue;

      if (find.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
         *Dir = TRUE;
         *Size = 0;
      }
      else {
         *Dir = FALSE;
         *Size = (find.nFileSizeHigh * (MAXDWORD+1)) + find.nFileSizeLow;
      }

      if (find.dwFileAttributes & (FILE_ATTRIBUTE_SYSTEM|FILE_ATTRIBUTE_HIDDEN)) *Hidden = TRUE;
      else *Hidden = FALSE;

      if (find.dwFileAttributes & FILE_ATTRIBUTE_READONLY) *ReadOnly = TRUE;
      else *ReadOnly = FALSE;

      if (find.dwFileAttributes & FILE_ATTRIBUTE_ARCHIVE) *Archive = TRUE;
      else *Archive = FALSE;

      for (i=0; (find.cFileName[i]) AND (i < 254); i++) Name[i] = find.cFileName[i];
      Name[i] = 0;

      if (CreateTime) convert_time(&find.ftCreationTime, CreateTime);
      if (WriteTime) convert_time(&find.ftLastWriteTime, WriteTime);
      //if (LastAccessTime) convert_time(&find.ftLastAccessTime, LastAccessTime); // Last access time is not dependable despite it being a feature...

      return 1;
   }

   return 0;
}

//********************************************************************************************************************

int winSetAttrib(CSTRING Path, LONG Flags)
{
   int attrib = GetFileAttributes(Path);
   if (attrib == INVALID_FILE_ATTRIBUTES) return 1;

   if (Flags & PERMIT_HIDDEN) attrib |= FILE_ATTRIBUTE_HIDDEN;
   else attrib &= ~FILE_ATTRIBUTE_HIDDEN;

   if (Flags & PERMIT_ARCHIVE) attrib |= FILE_ATTRIBUTE_ARCHIVE;
   else attrib &= ~FILE_ATTRIBUTE_ARCHIVE;

   if (Flags & PERMIT_WRITE) attrib &= ~FILE_ATTRIBUTE_READONLY;
   else attrib |= FILE_ATTRIBUTE_READONLY;

   SetFileAttributes(Path, attrib);

   return 0;
}

//********************************************************************************************************************

void winGetAttrib(CSTRING Path, LONG *Flags)
{
   *Flags = 0;

   int attrib = GetFileAttributes(Path);
   if (attrib == INVALID_FILE_ATTRIBUTES) return;

   if (attrib & FILE_ATTRIBUTE_HIDDEN)   *Flags |= PERMIT_HIDDEN;
   if (attrib & FILE_ATTRIBUTE_ARCHIVE)  *Flags |= PERMIT_ARCHIVE;
   if (attrib & FILE_ATTRIBUTE_OFFLINE)  *Flags |= PERMIT_OFFLINE;

   if (attrib & FILE_ATTRIBUTE_READONLY) *Flags |= PERMIT_READ;
   else *Flags |= PERMIT_READ|PERMIT_WRITE;
}

//********************************************************************************************************************

LONG winFileInfo(CSTRING Path, long long *Size, struct DateTime *Time, BYTE *Folder)
{
   if (!Path) return 0;

   int len;
   for (len=0; Path[len]; len++);
   if (len < 1) return 0;

   WIN32_FIND_DATA find;
   HANDLE handle;

   if ((Path[len-1] IS '/') OR (Path[len-1] IS '\\')) {
      char short_path[len];
      CopyMemory(short_path, Path, len-1);
      short_path[len-1] = 0;
      handle = FindFirstFile(short_path, &find);
   }
   else handle = FindFirstFile(Path, &find);

   if (handle IS INVALID_HANDLE_VALUE) return 0;

   if (Folder) {
      if (find.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) *Folder = TRUE;
      else *Folder = FALSE;
   }

   if (Size) *Size = (find.nFileSizeHigh * (MAXDWORD+1)) + find.nFileSizeLow;

   if (Time) {
      if (find.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
         convert_time(&find.ftCreationTime, Time);
      }
      else convert_time(&find.ftLastWriteTime, Time);
   }

   FindClose(handle);

   return 1;
}

//********************************************************************************************************************
// Returns TRUE if the folder exists.

LONG winCheckDirectoryExists(CSTRING Path)
{
   LONG len;

   for (len=0; Path[len]; len++);
   if (len <= 3) return 1; // Return TRUE if the path is a drive letter

   if ((Path[0] IS '\\') AND (Path[1] IS '\\')) {
      // UNC handling.  Use the widechar version of FindFirstFile() because it is required for UNC paths.

      #define SIZE_WSTR 400
      wchar_t wstr[SIZE_WSTR] = { '\\', '\\', '?', '\\', 'U', 'N', 'C', '\\' };
      int i;
      if ((i = MultiByteToWideChar(CP_UTF8, 0, Path+2, -1, wstr+8, SIZE_WSTR-12)) > 0) {
         i += 7;

         if (wstr[i-1] != '\\') wstr[i++] = '\\';
         wstr[i++] = '*';
         wstr[i] = 0;

         HANDLE handle;
         WIN32_FIND_DATAW find;
         if ((handle = FindFirstFileW(wstr, &find)) != INVALID_HANDLE_VALUE) {
            do {
               if (find.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                  FindClose(handle);
                  return 1;
               }
               //else printf("Path exists but is not a folder.\n");

            } while (FindNextFileW(handle, &find));

            FindClose(handle);
         }
         //else printerror();
      }

      return 0;
   }
   else {
      WIN32_FIND_DATA find;
      HANDLE handle;

      if ((Path[len-1] IS '/') OR (Path[len-1] IS '\\')) {
         char short_path[len];
         CopyMemory(short_path, Path, len-1);
         short_path[len-1] = 0;
         handle = FindFirstFileA(short_path, &find);
      }
      else handle = FindFirstFileA(Path, &find);

      if (handle != INVALID_HANDLE_VALUE) {
         do {
            if (find.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
               FindClose(handle);
               return 1;
            }
            //else fprintf(stderr, "Path exists but is not a folder.\n");

         } while (FindNextFileA(handle, &find));

         FindClose(handle);
      }
      //else printerror();

      return 0;
   }
}
