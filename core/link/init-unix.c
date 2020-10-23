/*****************************************************************************

This file is in the public domain and may be distributed and modified without
restriction.

*****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <dlfcn.h>
#include <unistd.h>

#include <parasol/main.h>

#ifndef ROOT_PATH
#define ROOT_PATH "/usr/local"
#endif

extern void program(void);
extern STRING ProgCopyright;
extern STRING ProgAuthor;
extern STRING ProgName;
extern STRING ProgDate;
extern LONG ProgDebug;
extern FLOAT ProgCoreVersion;

struct CoreBase *CoreBase;

static ERROR PROGRAM_DataFeed(OBJECTPTR, struct acDataFeed *);
void close_parasol(void);

//****************************************************************************

void usererror(CSTRING Message)
{
   printf("%s\n", Message);
}

//****************************************************************************
// Main execution point.

static APTR glCoreHandle = 0;
static void (*closecore)(void) = 0;

const char * init_parasol(int argc, CSTRING *argv)
{
   #define MAX_ARGS 30
   APTR *actions;

   glCoreHandle = NULL;
   closecore    = NULL;
   CSTRING msg  = NULL;

   char path_buf[256]; // NB: Assigned to info.RootPath

   struct OpenInfo info;
   info.Detail    = ProgDebug;
   info.MaxDepth  = 10;
   info.Name      = ProgName;
   info.Author    = ProgAuthor;
   info.Date      = ProgDate;
   info.Copyright = ProgCopyright;
   info.Args      = argv;
   info.ArgCount  = argc;
   info.CoreVersion = ProgCoreVersion; // Minimum required core version
   info.CompiledAgainst = VER_CORE; // The core that this code is compiled against
   info.Error           = ERR_Okay;
   info.Flags = OPF_CORE_VERSION|OPF_COMPILED_AGAINST|OPF_NAME|OPF_AUTHOR|OPF_DATE|OPF_COPYRIGHT|OPF_ARGS|OPF_ERROR;
   if (ProgDebug > 0) info.Flags |= OPF_DETAIL|OPF_MAX_DEPTH;
   if (ProgDebug IS -1) {
      info.Detail = 0;
      info.MaxDepth = 0;
      info.Flags |= OPF_DETAIL|OPF_MAX_DEPTH;
   }

   // Check for a local installation in the CWD.

   if ((glCoreHandle = dlopen("lib/core.so", RTLD_NOW))) {
      // The Core will need to know the root path to the Parasol Framework
      if (getcwd(path_buf, sizeof(path_buf))) {
         LONG i;
         for (i=0; path_buf[i]; i++);
         if (path_buf[i-1] != '/') path_buf[i++] = '/';
         path_buf[i] = 0;
         info.RootPath = path_buf;
         info.Flags |= OPF_ROOT_PATH;
      }
   }
   else {
      // Determine if there is a valid 'lib' folder in the binary's folder.
      // Retrieving the path of the binary only works on Linux (most types of Unix don't provide any support for this).

      char procfile[48];
      snprintf(procfile, sizeof(procfile), "/proc/%d/exe", getpid());

      LONG path_len;
      if ((path_len = readlink(procfile, path_buf, sizeof(path_buf)-1)) > 0) {
         // Strip the process name
         while ((path_len > 0) AND (path_buf[path_len-1] != '/')) path_len--;

         LONG ins = path_len;
         for (LONG i=0; "lib/core.so"[i]; i++) path_buf[ins++] = "lib/core.so"[i];
         path_buf[ins++] = 0;
         if (!(glCoreHandle = dlopen(path_buf, RTLD_NOW))) {
            // Check the parent folder of the binary
            while ((path_len > 0) AND (path_buf[path_len-1] != '/')) path_len--;

            LONG ins = path_len;
            for (LONG i=0; "lib/core.so"[i]; i++) path_buf[ins++] = "lib/core.so"[i];
            path_buf[ins++] = 0;

            glCoreHandle = dlopen(path_buf, RTLD_NOW);
         }

         if (glCoreHandle) {
            path_buf[path_len] = 0;
            info.RootPath = path_buf;
            info.Flags |= OPF_ROOT_PATH;
         }
      }
   }

   if (!glCoreHandle) { // Support for fixed installations
      if (!(glCoreHandle = dlopen(ROOT_PATH"/lib/parasol/core.so", RTLD_NOW))) {
         fprintf(stderr, "%s\n", dlerror());
         msg = "Failed to find or open the core library at "ROOT_PATH"/lib/parasol/core.so";
         goto failed_lib_open;
      }
   }

   struct CoreBase * (*opencore)(struct OpenInfo *);
   if (!(opencore = dlsym(glCoreHandle,"OpenCore"))) {
      msg = "Could not find the OpenCore symbol in the Core library.";
      goto failed_lib_sym;
   }

   if (!(closecore = dlsym(glCoreHandle,"CloseCore"))) {
      msg = "Could not find the CloseCore symbol.";
      goto failed_lib_sym;
   }

   if ((CoreBase = opencore(&info))) {
      OBJECTPTR task = CurrentTask();

      if (!GetPointer(task, FID_Actions, &actions)) {
         actions[AC_DataFeed] = PROGRAM_DataFeed;
      }
   }
   else if (info.Error IS ERR_CoreVersion) msg = "This program requires the latest version of the Parasol framework.\nPlease visit www.parasol.ws to upgrade.";
   else msg = "Failed to initialise Parasol.  Run again with --log-info.";

failed_lib_sym:
failed_lib_open:
   return msg;
}

//****************************************************************************

void close_parasol(void)
{
   if (closecore) closecore();
   if (glCoreHandle) dlclose(glCoreHandle);
}

//****************************************************************************

#include "startup-common.c"
