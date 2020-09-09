
//****************************************************************************

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
               break;
            }
         }
      }

      // If GetModuleFileName() failed, try GetCurrentDirectory()

      if (!PathBuffer[0]) GetCurrentDirectoryA(Size, PathBuffer);

      if (PathBuffer[0]) {
         for (len=0; PathBuffer[len]; len++);
         pos = len;

         strncpy(PathBuffer+len, "system\\modules\\core.dll", Size-len-1);

         //printf("Find: %s\n", PathBuffer);
         handle = INVALID_HANDLE_VALUE;
         while ((len) AND ((handle = FindFirstFileA(PathBuffer, &find)) IS INVALID_HANDLE_VALUE)) {
            // Regress by one folder to get to the root of the installation.

            if (PathBuffer[len-1] IS '\\') len--;
            while ((len > 0) AND (PathBuffer[len-1] != '\\')) {
               len--;
            }

            strncpy(PathBuffer+len, "system\\modules\\core.dll", Size-len-1);
            //printf("Find: %s\n", PathBuffer);
            pos = len;
         }

         if (handle != INVALID_HANDLE_VALUE) {
            // Success in finding the core.dll file
            FindClose(handle);
         }
         else PathBuffer[0] = 0;
      }
   }

   if (!PathBuffer[0]) {
      APTR keyhandle;
      if (!RegOpenKeyExA(HKEY_LOCAL_MACHINE, "Software\\Parasol", 0, KEY_READ, &keyhandle)) {
         LONG j = Size;
         if (!RegQueryValueExA(keyhandle, "Location", 0, 0, PathBuffer, &j)) {
            LONG i;
            for (i=0; PathBuffer[i]; i++);
            pos = i;
            strncpy(PathBuffer+i, "system\\modules\\core.dll", Size-i-1);
         }
         CloseHandle(keyhandle);
         keyhandle = NULL;
      }
   }

   {
      APTR handle;
      if (!(handle = LoadLibraryA(PathBuffer))) return NULL;

      PathBuffer[pos] = 0;
      return handle;
   }
}
