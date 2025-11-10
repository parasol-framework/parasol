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

static APTR find_core()
{
   std::string core_lib, folder;

   folder.clear();

   if (folder.empty()) {
      WIN32_FIND_DATA find;
      char buffer[MAX_PATH] = {};

      // Check local directories for base installation

      if (auto len = GetModuleFileNameA(nullptr, buffer, sizeof(buffer))) {
         for (int i = len; i > 0; i--) {
            if (buffer[i] IS '\\') {
               buffer[i+1] = 0;
               AddDllDirectory(buffer);
               break;
            }
         }
         folder = buffer;
      }

      // If GetModuleFileName() failed, try GetCurrentDirectory()

      if (folder.empty() and GetCurrentDirectoryA(sizeof(buffer), buffer)) {
         folder = buffer;
      }

      if (not folder.empty()) {
         auto test_lib = folder + "lib\\core.dll";
         auto handle = INVALID_HANDLE_VALUE;
         while ((handle = FindFirstFileA(test_lib.data(), &find)) IS INVALID_HANDLE_VALUE) {
            // Traverse one folder towards the root of the installation.

            if (folder.back() IS '\\') folder.pop_back();
            while ((not folder.empty()) and (folder.back() != '\\')) {
               folder.pop_back();
            }

            if (folder.empty()) break;  // No more parent directories

            test_lib = folder + "lib\\core.dll";
            //printf("Scan: %s\n", test_lib.c_str());
         }

         if (handle != INVALID_HANDLE_VALUE) { // Success in finding the core.dll file
            FindClose(handle);
            core_lib = test_lib;
         }
         else folder.clear();
      }
   }

   if (core_lib.empty()) { // If local core library not found, check the Windows registry
      APTR keyhandle;
      if (not RegOpenKeyExA(HKEY_LOCAL_MACHINE, "Software\\Parasol", 0, KEY_READ, &keyhandle)) {
         char buffer[MAX_PATH] = {};
         int j = sizeof(buffer);
         if (not RegQueryValueExA(keyhandle, "Location", 0, 0, buffer, &j)) {
            folder.assign(buffer);
            core_lib = folder + "lib\\core.dll";
         }
         CloseHandle(keyhandle);
         keyhandle = nullptr;
      }
   }

   // Prior to loading the core we must add the root and 3rdparty lib folder to the DLL search path.

   auto dll_folder = folder + "lib";
   SetDllDirectoryA(folder.c_str());

   APTR handle;
   core_lib = folder + "lib\\core.dll";
   if (!(handle = LoadLibraryA(core_lib.c_str()))) return nullptr;
   return handle;
}

#endif // PARASOL_STATIC
