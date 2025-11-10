
#ifndef PARASOL_STATIC

#include <string>

extern "C" {
DLLCALL APTR WINAPI LoadLibraryA(CSTRING);
DLLCALL int WINAPI FreeLibrary(APTR);
DLLCALL int WINAPI FindClose(APTR);
DLLCALL APTR WINAPI FindFirstFileA(STRING, void *);
DLLCALL APTR WINAPI GetProcAddress(APTR, CSTRING);
DLLCALL int WINAPI RegOpenKeyExA(int,CSTRING,int,int,APTR *);
DLLCALL int WINAPI RegQueryValueExA(APTR,CSTRING,int *,int *, char *,int *);
DLLCALL void WINAPI CloseHandle(APTR);
DLLCALL int  WINAPI MessageBoxA(int,CSTRING,CSTRING,int);
DLLCALL int WINAPI GetCurrentDirectoryA(int, CSTRING);
DLLCALL int WINAPI GetModuleFileNameA(APTR, CSTRING, int);
DLLCALL int WINAPI SetDllDirectoryA(CSTRING);
DLLCALL int WINAPI SetDefaultDllDirectories(int DirectoryFlags);
DLLCALL void * AddDllDirectory(STRING NewDirectory);
}

#define HKEY_LOCAL_MACHINE 0x80000002
#define KEY_READ 0x20019
#define MAX_PATH 260
#define INVALID_HANDLE_VALUE (void *)(-1)

typedef struct _FILETIME {
	int dwLowDateTime;
	int dwHighDateTime;
} FILETIME,*PFILETIME,*LPFILETIME;

typedef struct _WIN32_FIND_DATAW {
	int dwFileAttributes;
	FILETIME ftCreationTime;
	FILETIME ftLastAccessTime;
	FILETIME ftLastWriteTime;
	int nFileSizeHigh;
	int nFileSizeLow;
	int dwReserved0;
	int dwReserved1;
   uint16_t cFileName[MAX_PATH];
	uint16_t cAlternateFileName[14];
} WIN32_FIND_DATAW,*LPWIN32_FIND_DATAW;

typedef WIN32_FIND_DATAW WIN32_FIND_DATA,*LPWIN32_FIND_DATA;


//********************************************************************************************************************

static APTR find_core(std::string &PathBuffer)
{
   size_t pos = 0;

   PathBuffer.clear();

   if (PathBuffer.empty()) {
      int len, i;
      void *handle;
      WIN32_FIND_DATA find;
      char buffer[MAX_PATH];

      // Check local directories for base installation

      if ((len = GetModuleFileNameA(nullptr, buffer, sizeof(buffer)))) {
         for (i = len; i > 0; i--) {
            if (buffer[i] IS '\\') {
               buffer[i+1] = 0;
               AddDllDirectory(buffer);
               break;
            }
         }
         PathBuffer = buffer;
      }

      // If GetModuleFileName() failed, try GetCurrentDirectory()

      if (PathBuffer.empty() and GetCurrentDirectoryA(sizeof(buffer), buffer)) {
         PathBuffer = buffer;
      }

      if (!PathBuffer.empty()) {
         pos = PathBuffer.length();
         PathBuffer.append("lib\\core.dll");

         handle = INVALID_HANDLE_VALUE;
         while ((len = PathBuffer.length()) and ((handle = FindFirstFileA(PathBuffer.data(), &find)) IS INVALID_HANDLE_VALUE)) {
            // Regress by one folder to get to the root of the installation.

            if (PathBuffer[len-1] IS '\\') {
               PathBuffer.pop_back();
               len--;
            }
            while ((len > 0) and (PathBuffer[len-1] != '\\')) {
               PathBuffer.pop_back();
               len--;
            }

            PathBuffer.append("lib\\core.dll");
            //printf("Scan: %s\n", PathBuffer.c_str());
            pos = len;
         }

         if (handle != INVALID_HANDLE_VALUE) { // Success in finding the core.dll file
            FindClose(handle);
         }
         else {
            PathBuffer.clear();
         }
      }
   }

   if (PathBuffer.empty()) { // If local core library not found, check the Windows registry
      APTR keyhandle;
      if (!RegOpenKeyExA(HKEY_LOCAL_MACHINE, "Software\\Parasol", 0, KEY_READ, &keyhandle)) {
         char buffer[MAX_PATH] = {};
         int j = sizeof(buffer);
         if (!RegQueryValueExA(keyhandle, "Location", 0, 0, buffer, &j)) {
            PathBuffer = buffer;
            pos = PathBuffer.length();
            PathBuffer.append("lib\\core.dll");
         }
         CloseHandle(keyhandle);
         keyhandle = nullptr;
      }
   }

   // Prior to loading the core we must add the root and 3rdparty lib folder to the DLL search path.

   if (pos + 4 < PathBuffer.length()) {
      PathBuffer.replace(pos, PathBuffer.length() - pos, "lib");
      SetDllDirectoryA(PathBuffer.c_str());
      PathBuffer.replace(pos, PathBuffer.length() - pos, "core.dll");
   }

   {
      APTR handle;
      if (!(handle = LoadLibraryA(PathBuffer.c_str()))) return nullptr;

      PathBuffer.erase(pos);
      return handle;
   }
}

#endif // PARASOL_STATIC
