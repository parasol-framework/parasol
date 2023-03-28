
#include <windows.h>
#include <windowsx.h>

#define KEY_SURFACE 0x03929323

//****************************************************************************

HDC winGetDC(HWND Window)
{
   return GetDC(Window);
}

void winReleaseDC(HWND Window, HDC DC)
{
   ReleaseDC(Window, DC);
}

void winSetSurfaceID(HWND Window, LONG SurfaceID)
{
   SetProp(Window, "SurfaceID", LongToHandle(SurfaceID));
   SetWindowLong(Window, 0, SurfaceID);
   SetWindowLong(Window, 4, KEY_SURFACE);
}
