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

   char path_buf[256]; // NB: Assigned to info.SystemPath

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

   glCoreHandle = dlopen("/usr/local/lib/parasol/core.so", RTLD_NOW);

   if (!glCoreHandle) {
      //fprintf(stderr, "%s\n", dlerror());

      char *core_path = "lib/core.so";
      glCoreHandle = dlopen(core_path, RTLD_NOW);

      if (glCoreHandle) {
         if (getcwd(path_buf, sizeof(path_buf))) {
            LONG i, len;
            for (len=0; path_buf[len]; len++);
            for (i=0; ("/lib/"[i]) AND (len < sizeof(path_buf)-1); i++) path_buf[len++] = "/lib/"[i];
            path_buf[len] = 0;
            info.SystemPath = path_buf;
            info.Flags |= OPF_SYSTEM_PATH; // Must conform to the content of "%ROOT%/share/parasol"
         }
      }
      else {
         // Determine if there is a valid 'lib' folder accompanying the binary.
         // This method of path retrieval only works on Linux (most types of Unix don't provide any support for this).

         char procfile[50];
         snprintf(procfile, sizeof(procfile), "/proc/%d/exe", getpid());

         LONG len;
         if ((len = readlink(procfile, path_buf, sizeof(path_buf)-1)) > 0) {
            while (len > 0) { // Strip the process name
               if (path_buf[len-1] IS '/') break;
               len--;
            }

            LONG i;
            LONG ins = len;
            for (i=0; core_path[i]; i++) path_buf[ins++] = core_path[i];
            path_buf[ins++] = 0;

            glCoreHandle = dlopen(path_buf, RTLD_NOW);
            if (!glCoreHandle) {
               msg = dlerror();
               goto failed_lib_open;
            }

            path_buf[len] = 0;
            for (i=0; ("lib/"[i]) AND (len < sizeof(path_buf)-1); i++) path_buf[len++] = "lib/"[i];
            path_buf[len] = 0;
            info.SystemPath = path_buf;
            info.Flags |= OPF_SYSTEM_PATH; // Must conform to the content of "%ROOT%/share/parasol"
        }
      }

      if (!glCoreHandle) {
         msg = "Failed to find or open the core library.";
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
   else msg = "Failed to initialise Parasol.";

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
