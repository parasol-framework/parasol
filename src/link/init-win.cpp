/*********************************************************************************************************************

This file is in the public domain and may be distributed and modified without
restriction.

******************************************************************************

This version of the initialisation process does not define a main() function.
Parasol must be initialised via the init_parasol() function and closed with
close_parasol().  Another difference vs startup-win.c is that error messages
are returned by the function and not automatically presented in a dialog box.

*********************************************************************************************************************/

#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include <parasol/main.h>
#include <parasol/system/errors.h>

#define DLLCALL // __declspec(dllimport)
#define WINAPI  __stdcall
#define HKEY_LOCAL_MACHINE 0x80000002
#define KEY_READ 0x20019
#define MAX_PATH 260
#define INVALID_HANDLE_VALUE (void *)(-1)
extern "C" {
DLLCALL APTR WINAPI LoadLibraryA(CSTRING);
DLLCALL LONG WINAPI FreeLibrary(APTR);
DLLCALL LONG WINAPI FindClose(APTR);
DLLCALL APTR WINAPI FindFirstFileA(STRING, void *);
DLLCALL APTR WINAPI GetProcAddress(APTR, CSTRING);
DLLCALL LONG WINAPI RegOpenKeyExA(LONG,CSTRING,LONG,LONG,APTR *);
DLLCALL LONG WINAPI RegQueryValueExA(APTR,CSTRING,LONG *,LONG *,BYTE *,LONG *);
DLLCALL void WINAPI CloseHandle(APTR);
DLLCALL int  WINAPI MessageBoxA(LONG,CSTRING,CSTRING,LONG);
DLLCALL LONG WINAPI GetCurrentDirectoryA(LONG, CSTRING);
DLLCALL LONG WINAPI GetModuleFileNameA(APTR, CSTRING, LONG);
DLLCALL LONG WINAPI SetDllDirectoryA(CSTRING);
DLLCALL LONG WINAPI SetDefaultDllDirectories(LONG DirectoryFlags);
DLLCALL void * AddDllDirectory(STRING NewDirectory);
}
#define LOAD_LIBRARY_SEARCH_APPLICATION_DIR 0x00000200
#define LOAD_LIBRARY_SEARCH_USER_DIRS 0x00000400
#define LOAD_LIBRARY_SEARCH_SYSTEM32 0x00000800

typedef struct _FILETIME {
	LONG dwLowDateTime;
	LONG dwHighDateTime;
} FILETIME,*PFILETIME,*LPFILETIME;

typedef struct _WIN32_FIND_DATAW {
	LONG dwFileAttributes;
	FILETIME ftCreationTime;
	FILETIME ftLastAccessTime;
	FILETIME ftLastWriteTime;
	LONG nFileSizeHigh;
	LONG nFileSizeLow;
	LONG dwReserved0;
	LONG dwReserved1;
   UWORD cFileName[MAX_PATH];
	UWORD cAlternateFileName[14];
} WIN32_FIND_DATAW,*LPWIN32_FIND_DATAW;

typedef WIN32_FIND_DATAW WIN32_FIND_DATA,*LPWIN32_FIND_DATA;

#define MB_ICONERROR 16
#define MB_OK 0

extern void program(void);

struct CoreBase *CoreBase;

static ERROR PROGRAM_DataFeed(OBJECTPTR, struct acDataFeed *);
extern "C" void close_parasol(void);
static APTR find_core(char *PathBuffer, int Size);

//********************************************************************************************************************

typedef struct CoreBase * OPENCORE(struct OpenInfo *);
typedef void CLOSECORE(void);

APTR corehandle = NULL;
CLOSECORE *closecore = NULL;
static char msgbuf[120];

extern "C" const char * init_parasol(int argc, CSTRING *argv)
{
   #define MAX_ARGS 30
   struct OpenInfo info;
   const char *msg = NULL;
   APTR *actions;
   char path_buffer[256];

   corehandle = find_core(path_buffer, sizeof(path_buffer));
   if (!corehandle) return "Failed to open Parasol's core library.";

   auto opencore = (OPENCORE *)GetProcAddress((APTR)corehandle, "OpenCore");
   if (!opencore) {
      msg = "Could not find the OpenCore symbol in Parasol.";
      goto exit;
   }

   if (!(closecore = (CLOSECORE *)GetProcAddress((APTR)corehandle, "CloseCore"))) {
      msg = "Could not find the CloseCore symbol in Parasol.";
      goto exit;
   }

   info.Detail    = 0;
   info.MaxDepth  = 14;
   info.Args      = argv;
   info.ArgCount  = argc;
   info.CoreVersion = 0; // Minimum required core version
   info.CompiledAgainst = VER_CORE; // The core that this code is compiled against
   info.Error = ERR_Okay;
   info.Flags = OPF_CORE_VERSION|OPF_COMPILED_AGAINST|OPF_ARGS|OPF_ERROR;

   if ((CoreBase = opencore(&info))) {
      OBJECTPTR task = CurrentTask();

      if (!task->getPtr(FID_Actions, &actions)) {
         actions[AC_DataFeed] = (APTR)PROGRAM_DataFeed;
      }
   }
   else if (info.Error IS ERR_CoreVersion) msg = "This program requires the latest version of the Parasol framework.\nPlease visit www.parasol.ws to upgrade.";
   else {
      snprintf(msgbuf, sizeof(msgbuf), "Failed to initialise Parasol, error code %d.", info.Error);
      return msgbuf;
   }

   return msg;

exit:
   close_parasol();
   return msg;
}

//********************************************************************************************************************

void close_parasol(void)
{
   if (closecore) closecore();
   if (corehandle) FreeLibrary(corehandle);
}

#include "common-win.c"
#include "startup-common.c"
