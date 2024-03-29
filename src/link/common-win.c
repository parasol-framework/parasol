
#ifndef PARASOL_STATIC

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

#define HKEY_LOCAL_MACHINE 0x80000002
#define KEY_READ 0x20019
#define MAX_PATH 260
#define INVALID_HANDLE_VALUE (void *)(-1)

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


//********************************************************************************************************************

static APTR find_core(char *PathBuffer, int Size)
{
   WORD pos = 0;

   PathBuffer[0] = 0;

   if (!PathBuffer[0]) {
      LONG len, i;
      void *handle;
      WIN32_FIND_DATA find;

      // Check local directories for base installation

      if ((len = GetModuleFileNameA(NULL, PathBuffer, Size))) {
         for (i=len; i > 0; i--) {
            if (PathBuffer[i] IS '\\') {
               PathBuffer[i+1] = 0;
               AddDllDirectory(PathBuffer);
               break;
            }
         }
      }

      // If GetModuleFileName() failed, try GetCurrentDirectory()

      if (!PathBuffer[0]) GetCurrentDirectoryA(Size, PathBuffer);

      if (PathBuffer[0]) {
         for (len=0; PathBuffer[len]; len++);
         pos = len;

         strncpy(PathBuffer+len, "lib\\core.dll", Size-len-1);

         handle = INVALID_HANDLE_VALUE;
         while ((len) && ((handle = FindFirstFileA(PathBuffer, &find)) IS INVALID_HANDLE_VALUE)) {
            // Regress by one folder to get to the root of the installation.

            if (PathBuffer[len-1] IS '\\') len--;
            while ((len > 0) && (PathBuffer[len-1] != '\\')) {
               len--;
            }

            strncpy(PathBuffer+len, "lib\\core.dll", Size-len-1);
            //printf("Scan: %s\n", PathBuffer);
            pos = len;
         }

         if (handle != INVALID_HANDLE_VALUE) { // Success in finding the core.dll file
            FindClose(handle);
         }
         else PathBuffer[0] = 0;
      }
   }

   if (!PathBuffer[0]) { // If local core library not found, check the Windows registry
      APTR keyhandle;
      if (!RegOpenKeyExA(HKEY_LOCAL_MACHINE, "Software\\Parasol", 0, KEY_READ, &keyhandle)) {
         LONG j = Size;
         if (!RegQueryValueExA(keyhandle, "Location", 0, 0, PathBuffer, &j)) {
            LONG i;
            for (i=0; PathBuffer[i]; i++);
            pos = i;
            strncpy(PathBuffer+i, "lib\\core.dll", Size-i-1);
         }
         CloseHandle(keyhandle);
         keyhandle = NULL;
      }
   }

   // Prior to loading the core we must add the root and 3rdparty lib folder to the DLL search path.

   strncpy(PathBuffer+pos+4, "lib", 4);
   SetDllDirectoryA(PathBuffer);
   strncpy(PathBuffer+pos+4, "core.dll", 9);

   {
      APTR handle;
      if (!(handle = LoadLibraryA(PathBuffer))) return NULL;

      PathBuffer[pos] = 0;
      return handle;
   }
}

#endif // PARASOL_STATIC
