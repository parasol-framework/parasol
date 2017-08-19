
#include <windows.h>

#define WINAPI  __stdcall

extern BOOL WINAPI DllMainCRTStartup(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
   return TRUE;
}
