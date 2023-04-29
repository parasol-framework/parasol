/*********************************************************************************************************************

This file is in the public domain and may be distributed and modified without restriction.

*********************************************************************************************************************/

#include <stdio.h>

#include <parasol/main.h>

#define DLLCALL // __declspec(dllimport)
#define WINAPI  __stdcall

extern "C" {
DLLCALL APTR WINAPI GetProcAddress(APTR, CSTRING);
DLLCALL LONG WINAPI FreeLibrary(APTR);
}

extern void program(void);

struct CoreBase *CoreBase;

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
   struct OpenInfo info;
   CSTRING msg = NULL;
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
   info.Flags = OPF::CORE_VERSION|OPF::COMPILED_AGAINST|OPF::ARGS|OPF::ERROR;

   if ((CoreBase = opencore(&info)));
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
