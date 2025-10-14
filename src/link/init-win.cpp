/*********************************************************************************************************************

This file is in the public domain and may be distributed and modified without restriction.

*********************************************************************************************************************/

#include <stdio.h>

#include <parasol/main.h>

#ifndef PARASOL_STATIC
#define DLLCALL // __declspec(dllimport)
#define WINAPI  __stdcall

extern "C" {
DLLCALL APTR WINAPI GetProcAddress(APTR, CSTRING);
DLLCALL int WINAPI FreeLibrary(APTR);
}
using OPENCORE = ERR(struct OpenInfo *, struct CoreBase **);
using CLOSECORE = void(void);

struct CoreBase *CoreBase;
static APTR find_core(char *PathBuffer, int Size);
static APTR corehandle = nullptr;
CLOSECORE *CloseCore = nullptr;
#else
static struct CoreBase *CoreBase; // Dummy
#endif

//********************************************************************************************************************

extern "C" const char * init_parasol(int argc, CSTRING *argv)
{
#ifndef PARASOL_STATIC
   char path_buffer[256];
   corehandle = find_core(path_buffer, sizeof(path_buffer));
   if (!corehandle) return "Failed to open Parasol's core library.";

   auto OpenCore = (OPENCORE *)GetProcAddress((APTR)corehandle, "OpenCore");
   if (!OpenCore) {
      FreeLibrary(corehandle);
      return "Could not find the OpenCore symbol in Parasol.";
   }

   if (!(CloseCore = (CLOSECORE *)GetProcAddress((APTR)corehandle, "CloseCore"))) {
      FreeLibrary(corehandle);
      return "Could not find the CloseCore symbol in Parasol.";
   }
#endif

   struct OpenInfo info;
   info.Detail      = 0;
   info.MaxDepth    = 14;
   info.Args        = argv;
   info.ArgCount    = argc;
   info.Error = ERR::Okay;
   info.Flags = OPF::ARGS|OPF::ERROR;

   if (OpenCore(&info, &CoreBase) IS ERR::Okay) return nullptr;
   else if (info.Error IS ERR::CoreVersion) {
      return "This program requires the latest version of the Parasol framework.\nPlease visit www.parasol.ws to upgrade.";
   }
   else {
      static char msgbuf[120];
      snprintf(msgbuf, sizeof(msgbuf), "Failed to initialise Parasol, error code %d.", int(info.Error));
      return msgbuf;
   }
}

//********************************************************************************************************************

extern "C" void close_parasol(void)
{
   CloseCore();
#ifndef PARASOL_STATIC
   FreeLibrary(corehandle);
#endif
}

#include "common-win.c"
