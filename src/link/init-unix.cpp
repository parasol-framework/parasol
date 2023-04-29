/*********************************************************************************************************************

This file is in the public domain and may be distributed and modified without
restriction.

*********************************************************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <dlfcn.h>
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>

#include <parasol/main.h>

#ifndef _ROOT_PATH
#define _ROOT_PATH "/usr/local"
#endif

extern "C" void program(void);

struct CoreBase *CoreBase;

extern "C" void close_parasol(void);

//********************************************************************************************************************

void usererror(CSTRING Message)
{
   printf("%s\n", Message);
}

//********************************************************************************************************************
// Main execution point.

static APTR glCoreHandle = 0;
typedef void CLOSECORE(void);
static CLOSECORE *closecore = NULL;

extern "C" const char * init_parasol(int argc, CSTRING *argv)
{
   glCoreHandle = NULL;
   CSTRING msg  = NULL;

   char root_path[232] = ""; // NB: Assigned to info.RootPath
   char core_path[256] = "";

   struct OpenInfo info;
   info.Detail    = 0;
   info.MaxDepth  = 14;
   info.Args      = argv;
   info.ArgCount  = argc;
   info.CoreVersion = 0; // Minimum required core version
   info.CompiledAgainst = VER_CORE; // The core that this code is compiled against
   info.Error     = ERR_Okay;
   info.RootPath  = root_path;
   info.Flags     = OPF::CORE_VERSION|OPF::COMPILED_AGAINST|OPF::ARGS|OPF::ERROR|OPF::ROOT_PATH;

   // Check for a local installation in the CWD.

   struct stat corestat = { .st_size = -1 };
   if (!stat("lib/core.so", &corestat)) {
      // The working directory will form the root path to the Parasol Framework
      if (getcwd(root_path, sizeof(root_path))) {
         LONG i = strlen(root_path);
         if (root_path[i-1] != '/') root_path[i++] = '/';
         root_path[i] = 0;
      }
      snprintf(core_path, sizeof(core_path), "%slib/core.so", root_path);
   }
   else {
      // Determine if there is a valid 'lib' folder in the binary's folder.
      // Retrieving the path of the binary only works on Linux (most types of Unix don't provide any support for this).

      char procfile[48];
      snprintf(procfile, sizeof(procfile), "/proc/%d/exe", getpid());

      LONG path_len;
      if ((path_len = readlink(procfile, root_path, sizeof(root_path)-1)) > 0) {
         // Strip the process name
         while ((path_len > 0) and (root_path[path_len-1] != '/')) path_len--;
         root_path[path_len] = 0;

         snprintf(core_path, sizeof(core_path), "%slib/core.so", root_path);
         if (stat(core_path, &corestat)) {
            // Check the parent folder of the binary
            path_len--;
            while ((path_len > 0) and (root_path[path_len-1] != '/')) path_len--;
            root_path[path_len] = 0;

            snprintf(core_path, sizeof(core_path), "%slib/core.so", root_path);
            if (stat(core_path, &corestat)) { // Support for fixed installations
               strncpy(root_path, _ROOT_PATH"/", sizeof(root_path));
               strncpy(core_path, _ROOT_PATH"/lib/parasol/core.so", sizeof(core_path));
               if (stat(core_path, &corestat)) {
                  msg = "Failed to find the location of the core.so library";
                  goto failed_lib_open;
               }
            }
         }
      }
   }

   if ((!core_path[0]) or (!(glCoreHandle = dlopen(core_path, RTLD_NOW)))) {
      fprintf(stderr, "%s: %s\n", core_path, dlerror());
      msg = "Failed to open the core library.";
      goto failed_lib_open;
   }

   typedef struct CoreBase * OPENCORE(struct OpenInfo *);

   OPENCORE *opencore;
   if (!(opencore = (OPENCORE *)dlsym(glCoreHandle, "OpenCore"))) {
      msg = "Could not find the OpenCore symbol in the Core library.";
      goto failed_lib_sym;
   }

   if (!(closecore = (CLOSECORE *)dlsym(glCoreHandle, "CloseCore"))) {
      msg = "Could not find the CloseCore symbol.";
      goto failed_lib_sym;
   }

   if ((CoreBase = opencore(&info)));
   else if (info.Error IS ERR_CoreVersion) msg = "This program requires the latest version of the Parasol framework.\nPlease visit www.parasol.ws to upgrade.";
   else msg = "Failed to initialise Parasol.  Run again with --log-info.";

failed_lib_sym:
failed_lib_open:
   return msg;
}

//********************************************************************************************************************

extern "C" void close_parasol(void)
{
   if (closecore) closecore();
   if (glCoreHandle) dlclose(glCoreHandle);
}
