// Cygwin users: libuuid-devel will interfere with the resolution of IID_Unknown if installed.
// Removing lib/libuuid.la and lib/uuid.dll.a will resolve the compilation issue.

//#define DEBUG
//#define DBGMSG
//#define DBGMOUSE

#define _WIN32_WINNT 0x0600 // Allow Windows Vista function calls
#define WINVER 0x0600

#include <parasol/system/keys.h>
#include <windows.h>
#include <windowsx.h>
//#include <resource.h>
#include <winuser.h>
#include <shlobj.h>
#include <objidl.h>

#include <parasol/system/errors.h>

#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include "windows.h"

typedef unsigned char UBYTE;

struct winextra {
   int surface_id;   // 0
   int key;          // 4
   int interactive;  // 8
   int borderless;   // 12
};

#define IDT_RESIZE_WINDOW 1

#define WE_SURFACE     0
#define WE_KEY         4
#define WE_INTERACTIVE 8
#define WE_BORDERLESS  12

#define WNS_PLUGIN 0x00000001

#define KEY_SURFACE 0x03929323

#define OR  ||
#define AND &&
#define IS  ==

#define WIN_LMB 0x0001
#define WIN_RMB 0x0002
#define WIN_MMB 0x0004
#define WIN_DBL 0x8000

#define BORDERSIZE 6
#define WM_ICONNOTIFY (WM_USER + 101)
#define ID_TRAY 100

enum {
   CT_DATA=0,
   CT_AUDIO,
   CT_IMAGE,
   CT_FILE,
   CT_OBJECT,
   CT_TEXT,
   CT_END      // End
};

#define CLIP_DATA    (1<<CT_DATA)   // 1
#define CLIP_AUDIO   (1<<CT_AUDIO)  // 2
#define CLIP_IMAGE   (1<<CT_IMAGE)  // 4
#define CLIP_FILE    (1<<CT_FILE)   // 8
#define CLIP_OBJECT  (1<<CT_OBJECT) // 16
#define CLIP_TEXT    (1<<CT_TEXT)   // 32

#define HIDA_GetPIDLFolder(pida) (LPCITEMIDLIST)(((LPBYTE)pida)+(pida)->aoffset[0])
#define HIDA_GetPIDLItem(pida, i) (LPCITEMIDLIST)(((LPBYTE)pida)+(pida)->aoffset[i+1])

#ifdef _DEBUG
#define MSG(...) fprintf(stderr, __VA_ARGS__)
#else
#define MSG(...)
#endif

static HANDLE glHeap = NULL;
static BYTE glOleInit = 0;
static UINT fmtShellIDList;

enum { // From core.h
   DATA_TEXT=1,    // Standard ascii text
   DATA_RAW,       // Raw unprocessed data
   DATA_DEVICE_INPUT, // Device activity
   DATA_XML,       // Markup based text data.  NOTE: For clipboard data, the top-level encapsulating tag must declare the type of XML, e.g. <html>, <ripple>.  For plain XML, use <xml>
   DATA_AUDIO,     // Audio file data, recognised by the Sound class
   DATA_RECORD,    // Database record
   DATA_IMAGE,     // Image file data, recognised by the Image class
   DATA_REQUEST,   // Make a request for item data
   DATA_RECEIPT,   // Receipt for item data, in response to an earlier request
   DATA_FILE,      // File location (the data will reflect the complete file path)
   DATA_CONTENT    // Document content (between XML tags) - sent by document objects only
};

typedef struct rkDropTarget {
   IDropTarget idt;
   LONG lRefCount;

   struct WinDT *DataItems;
   int    TotalItems;
	char   *tb_pDragFile;
   IDataObject *CurrentDataObject;
   void *ItemData;
} RK_IDROPTARGET;

typedef struct rkDropTargetVTable
{
   BEGIN_INTERFACE
      HRESULT (STDMETHODCALLTYPE *QueryInterface)(struct rkDropTarget *pThis, REFIID riid, void  **ppvObject);
      ULONG (STDMETHODCALLTYPE *AddRef)(struct rkDropTarget *pThis);
      ULONG (STDMETHODCALLTYPE *Release)(struct rkDropTarget *pThis);
      HRESULT (STDMETHODCALLTYPE *DragEnter)(struct rkDropTarget *pThis, IDataObject *pDataObject, DWORD dwKeyState, POINTL pt, DWORD *pdwEffect);
      HRESULT (STDMETHODCALLTYPE *DragOver)(struct rkDropTarget *pThis, DWORD dwKeyState, POINTL pt, DWORD *pdwEffect);
      HRESULT (STDMETHODCALLTYPE *DragLeave)(struct rkDropTarget *pThis);
      HRESULT (STDMETHODCALLTYPE *Drop)(struct rkDropTarget *pThis, IDataObject *pDataObject, DWORD dwKeyState, POINT pt, DWORD *pdwEffect);
   END_INTERFACE
} RK_IDROPTARGET_VTBL;

static ULONG   STDMETHODCALLTYPE RKDT_AddRef(struct rkDropTarget *Self);
static HRESULT STDMETHODCALLTYPE RKDT_Drop(struct rkDropTarget *Self, IDataObject *Data, DWORD grfKeyState, POINT pt, DWORD * pdwEffect);
static int     STDMETHODCALLTYPE RKDT_AssessDatatype(struct rkDropTarget *Self, IDataObject *Data, char *Result, int Length);
static HRESULT STDMETHODCALLTYPE RKDT_DragEnter(struct rkDropTarget *Self, IDataObject *Data, DWORD grfKeyState, POINTL pt, DWORD *pdwEffect);

extern HINSTANCE glInstance;
void RepaintWindow(int, int, int, int, int);
void KillMessageHook(void);
static LRESULT CALLBACK WindowProcedure(HWND, UINT, WPARAM, LPARAM);

void report_windows_clip_text(void *);
void report_windows_clip_utf16(void *);
void report_windows_files(LPIDA, int);
void report_windows_hdrop(LPIDA, int);
void win_clipboard_updated(void);

void winCopyClipboard(void);

static HWND glMainScreen = 0;
static char glCursorEntry = FALSE;
static HCURSOR glDefaultCursor = 0;
static HWND glDeferredActiveWindow = 0;
char glTrayIcon = FALSE, glTaskBar = TRUE, glStickToFront = FALSE;
struct WinCursor *glCursors = 0;
HCURSOR glCurrentCursor = 0;
static BYTE glScreenClassInit = 0;
static DWORD glIgnoreClip = 0;
static UINT fmtShellIDList = 0;
static UINT fmtPasteSucceeded = 0;
static UINT fmtPerformedDropEffect = 0;
static UINT fmtPreferredDropEffect = 0;
static UINT fmtParasolClip = 0;
static int glClipboardUpdates = 0;

#ifdef DBGMSG
static struct {
   int code;
   char *name;
} wincmd[] = {
#ifdef DBGMOUSE
 { WM_SETCURSOR, "WM_SETCURSOR" },
 { WM_NCMOUSEHOVER, "WM_NCMOUSEHOVER" },
 { WM_NCMOUSELEAVE, "WM_NCMOUSELEAVE" }, { WM_NCMOUSEMOVE, "WM_NCMOUSEMOVE" },
 { WM_MOUSEACTIVATE, "WM_MOUSEACTIVATE" }, { WM_MOUSEMOVE, "WM_MOUSEMOVE" }, { WM_LBUTTONDOWN, "WM_LBUTTONDOWN" }, { WM_LBUTTONUP, "WM_LBUTTONUP" },
 { WM_LBUTTONDBLCLK, "WM_LBUTTONDBLCLK" }, { WM_RBUTTONDOWN, "WM_RBUTTONDOWN" }, { WM_RBUTTONUP, "WM_RBUTTONUP" }, { WM_RBUTTONDBLCLK, "WM_RBUTTONDBLCLK" }, { WM_MBUTTONDOWN, "WM_MBUTTONDOWN" }, { WM_MBUTTONUP, "WM_MBUTTONUP" },
 { WM_MBUTTONDBLCLK, "WM_MBUTTONDBLCLK" }, { WM_MOUSEWHEEL, "WM_MOUSEWHEEL" }, { WM_MOUSEFIRST, "WM_MOUSEFIRST" }, { WM_XBUTTONDOWN, "WM_XBUTTONDOWN" },
 { WM_XBUTTONUP, "WM_XBUTTONUP" }, { WM_XBUTTONDBLCLK, "WM_XBUTTONDBLCLK" }, { WM_MOUSELAST, "WM_MOUSELAST" }, { WM_MOUSEHOVER, "WM_MOUSEHOVER" }, { WM_MOUSELEAVE, "WM_MOUSELEAVE" },
 { WM_NCHITTEST, "WM_NCHITTEST" }, { WM_NCLBUTTONDBLCLK, "WM_NCLBUTTONDBLCLK" }, { WM_NCLBUTTONDOWN, "WM_NCLBUTTONDOWN" },
#endif
 { WM_APP, "WM_APP" }, { WM_ACTIVATE, "WM_ACTIVATE" }, { WM_ACTIVATEAPP, "WM_ACTIVATEAPP" }, { WM_AFXFIRST, "WM_AFXFIRST" }, { WM_MOVE, "WM_MOVE" },
 { WM_AFXLAST, "WM_AFXLAST" }, { WM_ASKCBFORMATNAME, "WM_ASKCBFORMATNAME" }, { WM_CANCELJOURNAL, "WM_CANCELJOURNAL" }, { WM_CANCELMODE, "WM_CANCELMODE" },
 { WM_CAPTURECHANGED, "WM_CAPTURECHANGED" }, { WM_CHANGECBCHAIN, "WM_CHANGECBCHAIN" }, { WM_CHAR, "WM_CHAR" }, { WM_CHARTOITEM, "WM_CHARTOITEM" },
 { WM_CHILDACTIVATE, "WM_CHILDACTIVATE" }, { WM_CLEAR, "WM_CLEAR" }, { WM_CLOSE, "WM_CLOSE" }, { WM_COMMAND, "WM_COMMAND" },
 { WM_COMPACTING, "WM_COMPACTING" }, { WM_COMPAREITEM, "WM_COMPAREITEM" }, { WM_CONTEXTMENU, "WM_CONTEXTMENU" }, { WM_COPY, "WM_COPY" },
 { WM_COPYDATA, "WM_COPYDATA" }, { WM_CREATE, "WM_CREATE" }, { WM_CTLCOLORBTN, "WM_CTLCOLORBTN" }, { WM_CTLCOLORDLG, "WM_CTLCOLORDLG" },
 { WM_CTLCOLOREDIT, "WM_CTLCOLOREDIT" }, { WM_CTLCOLORLISTBOX, "WM_CTLCOLORLISTBOX" }, { WM_CTLCOLORMSGBOX, "WM_CTLCOLORMSGBOX" }, { WM_CTLCOLORSCROLLBAR, "WM_CTLCOLORSCROLLBAR" },
 { WM_CTLCOLORSTATIC, "WM_CTLCOLORSTATIC" }, { WM_CUT, "WM_CUT" }, { WM_DEADCHAR, "WM_DEADCHAR" }, { WM_DELETEITEM, "WM_DELETEITEM" },
 { WM_DESTROY, "WM_DESTROY" }, { WM_DESTROYCLIPBOARD, "WM_DESTROYCLIPBOARD" }, { WM_DEVICECHANGE, "WM_DEVICECHANGE" }, { WM_DEVMODECHANGE, "WM_DEVMODECHANGE" }, { WM_DISPLAYCHANGE, "WM_DISPLAYCHANGE" },
 { WM_DRAWCLIPBOARD, "WM_DRAWCLIPBOARD" }, { WM_DRAWITEM, "WM_DRAWITEM" }, { WM_DROPFILES, "WM_DROPFILES" }, { WM_ENABLE, "WM_ENABLE" },
 { WM_ENDSESSION, "WM_ENDSESSION" }, { WM_ENTERIDLE, "WM_ENTERIDLE" }, { WM_ENTERMENULOOP, "WM_ENTERMENULOOP" }, { WM_ENTERSIZEMOVE, "WM_ENTERSIZEMOVE" },
 { WM_ERASEBKGND, "WM_ERASEBKGND" }, { WM_EXITMENULOOP, "WM_EXITMENULOOP" }, { WM_EXITSIZEMOVE, "WM_EXITSIZEMOVE" }, { WM_FONTCHANGE, "WM_FONTCHANGE" },
 { WM_GETDLGCODE, "WM_GETDLGCODE" }, { WM_GETFONT, "WM_GETFONT" }, { WM_GETHOTKEY, "WM_GETHOTKEY" }, { WM_GETICON, "WM_GETICON" },
 { WM_GETMINMAXINFO, "WM_GETMINMAXINFO" }, { WM_GETTEXT, "WM_GETTEXT" }, { WM_GETTEXTLENGTH, "WM_GETTEXTLENGTH" }, { WM_HANDHELDFIRST, "WM_HANDHELDFIRST" },
 { WM_HANDHELDLAST, "WM_HANDHELDLAST" }, { WM_HELP, "WM_HELP" }, { WM_HOTKEY, "WM_HOTKEY" }, { WM_HSCROLL, "WM_HSCROLL" },
 { WM_HSCROLLCLIPBOARD, "WM_HSCROLLCLIPBOARD" }, { WM_ICONERASEBKGND, "WM_ICONERASEBKGND" }, { WM_INITDIALOG, "WM_INITDIALOG" }, { WM_INITMENU, "WM_INITMENU" },
 { WM_INITMENUPOPUP, "WM_INITMENUPOPUP" }, { WM_INPUTLANGCHANGE, "WM_INPUTLANGCHANGE" }, { WM_INPUTLANGCHANGEREQUEST, "WM_INPUTLANGCHANGEREQUEST" },
 { WM_KEYDOWN, "WM_KEYDOWN" }, { WM_KEYUP, "WM_KEYUP" }, { WM_KILLFOCUS, "WM_KILLFOCUS" }, { WM_MDIACTIVATE, "WM_MDIACTIVATE" },
 { WM_MDICASCADE, "WM_MDICASCADE" }, { WM_MDICREATE, "WM_MDICREATE" }, { WM_MDIDESTROY, "WM_MDIDESTROY" }, { WM_MDIGETACTIVE, "WM_MDIGETACTIVE" },
 { WM_MDIICONARRANGE, "WM_MDIICONARRANGE" }, { WM_MDIMAXIMIZE, "WM_MDIMAXIMIZE" }, { WM_MDINEXT, "WM_MDINEXT" }, { WM_MDIREFRESHMENU, "WM_MDIREFRESHMENU" }, { WM_MDIRESTORE, "WM_MDIRESTORE" },
 { WM_MDISETMENU, "WM_MDISETMENU" }, { WM_MDITILE, "WM_MDITILE" }, { WM_MEASUREITEM, "WM_MEASUREITEM" }, { WM_UNINITMENUPOPUP, "WM_UNINITMENUPOPUP" },
 { WM_MENURBUTTONUP, "WM_MENURBUTTONUP" }, { WM_MENUCOMMAND, "WM_MENUCOMMAND" }, { WM_MENUGETOBJECT, "WM_MENUGETOBJECT" }, { WM_MENUDRAG, "WM_MENUDRAG" },
 { WM_MENUCHAR, "WM_MENUCHAR" }, { WM_MENUSELECT, "WM_MENUSELECT" }, { WM_NEXTMENU, "WM_NEXTMENU" }, { WM_SHOWWINDOW, "WM_SHOWWINDOW" },
 { WM_MOVING, "WM_MOVING" }, { WM_NCACTIVATE, "WM_NCACTIVATE" }, { WM_NCCALCSIZE, "WM_NCCALCSIZE" }, { WM_NCCREATE, "WM_NCCREATE" },
 { WM_NCDESTROY, "WM_NCDESTROY" }, { WM_WINDOWPOSCHANGING, "WM_WINDOWPOSCHANGING" }, { WM_WINDOWPOSCHANGED, "WM_WINDOWPOSCHANGED" },
 { WM_NCLBUTTONUP, "WM_NCLBUTTONUP" }, { WM_NCMBUTTONDBLCLK, "WM_NCMBUTTONDBLCLK" }, { WM_NCMBUTTONDOWN, "WM_NCMBUTTONDOWN" }, { WM_NCMBUTTONUP, "WM_NCMBUTTONUP" },
 { WM_NCXBUTTONDOWN, "WM_NCXBUTTONDOWN" }, { WM_NCXBUTTONUP, "WM_NCXBUTTONUP" }, { WM_NCXBUTTONDBLCLK, "WM_NCXBUTTONDBLCLK" },
 { WM_NCPAINT, "WM_NCPAINT" }, { WM_NCRBUTTONDBLCLK, "WM_NCRBUTTONDBLCLK" }, { WM_SYSCOMMAND, "WM_SYSCOMMAND" }, { WM_GETMINMAXINFO, "WM_GETMINMAXINFO" },
 { WM_NCRBUTTONDOWN, "WM_NCRBUTTONDOWN" }, { WM_NCRBUTTONUP, "WM_NCRBUTTONUP" }, { WM_NEXTDLGCTL, "WM_NEXTDLGCTL" }, { WM_NEXTMENU, "WM_NEXTMENU" }, { WM_NOTIFY, "WM_NOTIFY" }, { WM_NOTIFYFORMAT, "WM_NOTIFYFORMAT" },
 { WM_NULL, "WM_NULL" }, { WM_PAINT, "WM_PAINT" }, { WM_PAINTCLIPBOARD, "WM_PAINTCLIPBOARD" }, { WM_PAINTICON, "WM_PAINTICON" }, { WM_PALETTECHANGED, "WM_PALETTECHANGED" }, { WM_PALETTEISCHANGING, "WM_PALETTEISCHANGING" },
 { WM_PARENTNOTIFY, "WM_PARENTNOTIFY" }, { WM_PASTE, "WM_PASTE" }, { WM_PENWINFIRST, "WM_PENWINFIRST" }, { WM_PENWINLAST, "WM_PENWINLAST" },
 { WM_POWER, "WM_POWER" }, { WM_POWERBROADCAST, "WM_POWERBROADCAST" }, { WM_PRINT, "WM_PRINT" }, { WM_PRINTCLIENT, "WM_PRINTCLIENT" }, { WM_QUERYDRAGICON, "WM_QUERYDRAGICON" }, { WM_QUERYENDSESSION, "WM_QUERYENDSESSION" },
 { WM_QUERYNEWPALETTE, "WM_QUERYNEWPALETTE" }, { WM_QUERYOPEN, "WM_QUERYOPEN" }, { WM_QUEUESYNC, "WM_QUEUESYNC" }, { WM_QUIT, "WM_QUIT" }, { WM_RENDERALLFORMATS, "WM_RENDERALLFORMATS" }, { WM_RENDERFORMAT, "WM_RENDERFORMAT" }, { WM_SETFOCUS, "WM_SETFOCUS" },
 { WM_SETFONT, "WM_SETFONT" }, { WM_SETHOTKEY, "WM_SETHOTKEY" }, { WM_SETICON, "WM_SETICON" }, { WM_SETREDRAW, "WM_SETREDRAW" }, { WM_SETTEXT, "WM_SETTEXT" }, { WM_SETTINGCHANGE, "WM_SETTINGCHANGE" },
 { WM_SHOWWINDOW, "WM_SHOWWINDOW" }, { WM_SIZE, "WM_SIZE" }, { WM_SIZECLIPBOARD, "WM_SIZECLIPBOARD" }, { WM_SIZING, "WM_SIZING" },
 { WM_SPOOLERSTATUS, "WM_SPOOLERSTATUS" }, { WM_STYLECHANGED, "WM_STYLECHANGED" }, { WM_STYLECHANGING, "WM_STYLECHANGING" }, { WM_SYSCHAR, "WM_SYSCHAR" }, { WM_SYSCOLORCHANGE, "WM_SYSCOLORCHANGE" },
 { WM_SYSCOMMAND, "WM_SYSCOMMAND" }, { WM_SYSDEADCHAR, "WM_SYSDEADCHAR" }, { WM_SYSKEYDOWN, "WM_SYSKEYDOWN" }, { WM_SYSKEYUP, "WM_SYSKEYUP" }, { WM_TCARD, "WM_TCARD" },
 { WM_TIMECHANGE, "WM_TIMECHANGE" }, { WM_TIMER, "WM_TIMER" }, { WM_UNDO, "WM_UNDO" }, { WM_USER, "WM_USER" }, { WM_USERCHANGED, "WM_USERCHANGED" }, { WM_VKEYTOITEM, "WM_VKEYTOITEM" },
 { WM_VSCROLL, "WM_VSCROLL" }, { WM_VSCROLLCLIPBOARD, "WM_VSCROLLCLIPBOARD" }, { WM_WINDOWPOSCHANGED, "WM_WINDOWPOSCHANGED" }, { WM_WINDOWPOSCHANGING, "WM_WINDOWPOSCHANGING" },
 { WM_WININICHANGE, "WM_WININICHANGE" }, { WM_KEYFIRST, "WM_KEYFIRST" }, { WM_KEYLAST, "WM_KEYLAST" }, { WM_SYNCPAINT, "WM_SYNCPAINT" },
 { 0, 0 }
};

#endif

static LONG winInitDragDrop(HWND Window);

/*
static void printerror(void)
{
   LPVOID lpMsgBuf;
   FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL,
      GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR)&lpMsgBuf, 0, NULL);
   printf(lpMsgBuf);
   LocalFree(lpMsgBuf);
}
*/

//********************************************************************************************************************

void winGetCoords(HWND Window, int *WinX, int *WinY, int *WinWidth, int *WinHeight, int *ClientX, int *ClientY,
   int *ClientWidth, int *ClientHeight)
{
   WINDOWINFO info;

   info.cbSize = sizeof(info);
   if (GetWindowInfo(Window, &info)) {
      if (WinX)         *WinX      = info.rcWindow.left;
      if (WinY)         *WinY      = info.rcWindow.top;
      if (WinWidth)     *WinWidth  = info.rcWindow.right - info.rcWindow.left;
      if (WinHeight)    *WinHeight = info.rcWindow.bottom - info.rcWindow.top;
      if (ClientX)      *ClientX      = info.rcClient.left;
      if (ClientY)      *ClientY      = info.rcClient.top;
      if (ClientWidth)  *ClientWidth  = info.rcClient.right - info.rcClient.left;
      if (ClientHeight) *ClientHeight = info.rcClient.bottom - info.rcClient.top;
   }
   else {
      if (WinX)         *WinX      = 0;
      if (WinY)         *WinY      = 0;
      if (WinWidth)     *WinWidth  = 0;
      if (WinHeight)    *WinHeight = 0;
      if (ClientX)      *ClientX      = 0;
      if (ClientY)      *ClientY      = 0;
      if (ClientWidth)  *ClientWidth  = 0;
      if (ClientHeight) *ClientHeight = 0;
   }
}

//********************************************************************************************************************

HDC winGetDC(HWND Window)
{
   return GetDC(Window);
}

void winReleaseDC(HWND Window, HDC DC)
{
   ReleaseDC(Window, DC);
}

//********************************************************************************************************************
// If an error occurs, the DPI values are not updated.

void winGetDPI(LONG *HDPI, LONG *VDPI)
{
   // The SetProcessDPIAware() function was introduced in Windows Vista - we use it dynamically.

   static BYTE dpi_called = FALSE;
   if (!dpi_called) {
      dpi_called = TRUE;
      HMODULE hUser32 = LoadLibrary("user32.dll");
      typedef BOOL (*SetProcessDPIAwareFunc)();
      SetProcessDPIAwareFunc setDPIAware = (SetProcessDPIAwareFunc)GetProcAddress(hUser32, "SetProcessDPIAware");
      if (setDPIAware) setDPIAware();
      FreeLibrary(hUser32);
   }

   HDC screen;
   if ((screen = GetDC(NULL))) {
      *HDPI = GetDeviceCaps(screen, LOGPIXELSX);
      *VDPI = GetDeviceCaps(screen, LOGPIXELSY);
      if (*HDPI < 96) *HDPI = 96;
      if (*VDPI < 96) *VDPI = 96;
      ReleaseDC(NULL, screen);
   }
}

#if 0 // Alternative method for DPI resolution - does it work better than the other method?
#include <SetupApi.h>

#define DISPLAY_DEVICE_ACTIVE    0x00000001
#define DISPLAY_DEVICE_ATTACHED  0x00000002

#define NAME_SIZE 128
const GUID GUID_CLASS_MONITOR = DEFINE_GUID(GUID_DEVCLASS_MONITOR, 0x4d36e96e, 0xe325, 0x11ce, 0xbf, 0xc1, 0x08, 0x00, 0x2b, 0xe1, 0x03, 0x18);

char GetMonitorSizeFromEDID(const HKEY hDevRegKey, short *WidthMm, short *HeightMm)
{
    DWORD dwType, AcutalValueNameLength = NAME_SIZE;
    TCHAR valueName[NAME_SIZE];
    BYTE EDIDdata[1024];
    DWORD edidsize=sizeof(EDIDdata);
    int i, retValue;

    for (i = 0, retValue = ERROR_SUCCESS; retValue != ERROR_NO_MORE_ITEMS; ++i) {
        retValue = RegEnumValue ( hDevRegKey, i, &valueName[0], &AcutalValueNameLength, NULL, &dwType, EDIDdata, &edidsize);
        if (retValue != ERROR_SUCCESS || 0 != _tcscmp(valueName,_T("EDID"))) continue;
        *WidthMm  = ((EDIDdata[68] & 0xF0) << 4) + EDIDdata[66];
        *HeightMm = ((EDIDdata[68] & 0x0F) << 8) + EDIDdata[67];
        return TRUE; // valid EDID found
    }
    return FALSE; // EDID not found
}

char GetSizeForDevID(const char * *TargetDevID, short *WidthMm, short *HeightMm)
{
    HDEVINFO devInfo = SetupDiGetClassDevsEx(
        &GUID_CLASS_MONITOR, //class GUID
        NULL, //enumerator
        NULL, //HWND
        DIGCF_PRESENT, // Flags //DIGCF_ALLCLASSES|
        NULL, // device info, create a new one.
        NULL, // machine name, local machine
        NULL);// reserved

    if (NULL == devInfo) return FALSE;

    char bRes = FALSE;

    unsigned int i;
    for (i=0; ERROR_NO_MORE_ITEMS != GetLastError(); ++i) {
      SP_DEVINFO_DATA devInfoData;
      memset(&devInfoData,0,sizeof(devInfoData));
      devInfoData.cbSize = sizeof(devInfoData);

      if (SetupDiEnumDeviceInfo(devInfo,i,&devInfoData)) {
         HKEY hDevRegKey = SetupDiOpenDevRegKey(devInfo,&devInfoData, DICS_FLAG_GLOBAL, 0, DIREG_DEV, KEY_READ);
         if(!hDevRegKey || (hDevRegKey == INVALID_HANDLE_VALUE)) continue;
         bRes = GetMonitorSizeFromEDID(hDevRegKey, WidthMm, HeightMm);
         RegCloseKey(hDevRegKey);
      }
   }
   SetupDiDestroyDeviceInfoList(devInfo);
   return bRes;
}

void get_dpi(void)
{
    short WidthMm, HeightMm;

    DISPLAY_DEVICE dd;
    dd.cb = sizeof(dd);
    DWORD dev = 0; // device index

   char bFoundDevice = FALSE;
   while (EnumDisplayDevices(0, dev, &dd, 0) && !bFoundDevice) {
      DISPLAY_DEVICE ddMon;
      ZeroMemory(&ddMon, sizeof(ddMon));
      ddMon.cb = sizeof(ddMon);
      DWORD devMon = 0;

      while (EnumDisplayDevices(dd.DeviceName, devMon, &ddMon, 0) && !bFoundDevice) {
         if (ddMon.StateFlags & DISPLAY_DEVICE_ACTIVE && !(ddMon.StateFlags & DISPLAY_DEVICE_MIRRORING_DRIVER)) {
            // THIS IS UNTESTED
            char device_id[9], buffer[80];
            vsprintf(buffer, sizeof(buffer), "%s", ddMon.DeviceID);
            for (i=9; buffer[i] AND (buffer[i] != '\\'); i++);
            StrCopy(buffer+i, device_id, 8);
            bFoundDevice = GetSizeForDevID(&device_id, &WidthMm, &HeightMm);
         }
         devMon++;
         ZeroMemory(&ddMon, sizeof(ddMon));
         ddMon.cb = sizeof(ddMon);
      }

      ZeroMemory(&dd, sizeof(dd));
      dd.cb = sizeof(dd);
      dev++;
   }
}
#endif

//********************************************************************************************************************
// Key conversion table.  This is used for translating raw values from the keyboard into our keyboard values.

static unsigned char keyconv[256] = {
   0,0,0,0,0,0,0,0,K_BACKSPACE,K_TAB,0,0,K_CLEAR,K_ENTER,0,0,                              // 0x00
   K_L_SHIFT,K_WIN_CONTROL,0,K_PAUSE,K_CAPS_LOCK,0,0,0,0,0,0,K_ESCAPE,0,0,0,0,             // 0x10
   K_SPACE,K_PAGE_UP,K_PAGE_DOWN,K_END,K_HOME,K_LEFT,K_UP,K_RIGHT,K_DOWN,K_SELECT,K_PRINT,K_EXECUTE,K_PRT_SCR,K_INSERT,K_DELETE,K_HELP, // 0x20
   K_ZERO,K_ONE,K_TWO,K_THREE,K_FOUR,K_FIVE,K_SIX,K_SEVEN,K_EIGHT,K_NINE,0,0,0,0,0,0,      // 0x30
   0,K_A,K_B,K_C,K_D,K_E,K_F,K_G,K_H,K_I,K_J,K_K,K_L,K_M,K_N,K_O,                          // 0x40
   K_P,K_Q,K_R,K_S,K_T,K_U,K_V,K_W,K_X,K_Y,K_Z,K_L_COMMAND,K_R_COMMAND,K_MENU,0,K_SLEEP,   // 0x50
   K_NP_0,K_NP_1,K_NP_2,K_NP_3,K_NP_4,K_NP_5,K_NP_6,K_NP_7,K_NP_8,K_NP_9,K_NP_MULTIPLY,K_NP_PLUS,K_NP_BAR,K_NP_MINUS,K_NP_DOT,K_NP_DIVIDE, // 0x60 Numeric keypad keys
   K_F1,K_F2,K_F3,K_F4,K_F5,K_F6,K_F7,K_F8,K_F9,K_F10,K_F11,K_F12,K_F13,K_F14,K_F15,K_F16,  // 0x70
   K_F17,K_F18,K_F19,K_F20,K_F1,K_F2,K_F3,K_F4,0,0,0,0,0,0,0,0,                             // 0x80
   K_NUM_LOCK,K_SCR_LOCK,0,0,0,0,0,0,0,0,0,0,0,0,0,0,                                       // 0x90
   K_L_SHIFT,K_R_SHIFT,K_L_CONTROL,K_R_CONTROL,K_L_COMMAND,K_R_COMMAND,0,0,0,0,0,0,0,0,0,0, // 0xa0
   0,0,0,0,0,0,0,0,0,0,K_SEMI_COLON,K_EQUALS,K_COMMA,K_MINUS,K_DOT,K_SLASH,                 // 0xb0
   K_REVERSE_QUOTE,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,                                           // 0xc0
   0,0,0,0,0,0,0,0,0,0,0,K_L_SQUARE,K_BACK_SLASH,K_R_SQUARE,K_APOSTROPHE,0,                 // 0xd0
   0,0,K_BACK_SLASH,0,0,0,0,0,0,0,0,0,0,0,0,0,                                              // 0xe0
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,                                                         // 0xf0
};

static int glQualifiers = 0;

static const LPCTSTR glWinCursors[] = {
   IDC_ARROW,
   IDC_SIZENESW,
   IDC_SIZENWSE,
   IDC_SIZENWSE,
   IDC_SIZENESW,
   IDC_SIZEWE,
   IDC_SIZEWE,
   IDC_SIZENS,
   IDC_SIZENS,
   IDC_CROSS,
   IDC_WAIT,
   IDC_SIZEALL,
   IDC_SIZENS,
   IDC_SIZEWE,
   IDC_HAND,
   IDC_HAND,
   IDC_HAND,
   IDC_HAND,
   IDC_IBEAM,
   IDC_ARROW,
   IDC_NO,
   NULL,        // The invisible cursor is the NULL type
   NULL,
   IDC_SIZEALL
};

static int glCancelAutoPlayMsg = 0;

//********************************************************************************************************************

void winSetCursor(HCURSOR Cursor)
{
   glCurrentCursor = Cursor;
   SetCursor(Cursor);
}

//********************************************************************************************************************

void winSetClassCursor(HWND Window, HCURSOR Cursor)
{
   SetClassLong(Window, GCLP_HCURSOR, (LONG_PTR)Cursor);
}

//********************************************************************************************************************

void winInitCursors(struct WinCursor *Cursor, int Total)
{
   int i;

   for (i=0; i < Total; i++) {
      if (glWinCursors[i]) Cursor[i].WinCursor = LoadCursor(NULL, glWinCursors[i]);
      else Cursor[i].WinCursor = NULL;
   }

   glCursors = Cursor;
   glCurrentCursor = Cursor[0].WinCursor;
}

//********************************************************************************************************************

void winSetCursorPos(double X, double Y)
{
   POINT point;
   if (glMainScreen) {
      point.x = X;
      point.y = Y;
      ClientToScreen(glMainScreen, &point);
      SetCursorPos(point.x, point.y);
   }
}

//********************************************************************************************************************

void winShowCursor(int State)
{
   ShowCursor(State);
}

//********************************************************************************************************************

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"
int winLookupSurfaceID(HWND Window)
{
   return (int)GetProp(Window, "SurfaceID");
   //return GetWindowLong(Window, WE_SURFACE);
}
#pragma GCC diagnostic pop

//********************************************************************************************************************

void winFindClose(HANDLE Handle)
{
   FindClose(Handle);
}

//********************************************************************************************************************

HANDLE winFindWindow(char *Class, char *Window)
{
   return FindWindow(Class, Window);
}

//********************************************************************************************************************

void winMinimiseWindow(HWND Window)
{
#if 0

   WINDOWPLACEMENT win;

   win.length = sizeof(win);
   GetWindowPlacement(Window, &win);
   win.showCmd = SW_MINIMIZE;
   win.flags = 0; //WPF_SETMINPOSITION;
   win.ptMinPosition.x = 0;
   win.ptMinPosition.y = 3000;
   SetWindowPlacement(Window, &win);

#else

   ShowWindow(Window, SW_MINIMIZE);

#endif
}

//********************************************************************************************************************

int winReadKey(char *Key, char *Value, char *Buffer, int Length)
{
   HKEY handle;
   DWORD length;
   int errnum;

   errnum = 0;
   length = Length;
   if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, Key, 0, KEY_READ, &handle) == ERROR_SUCCESS) {
      if (RegQueryValueEx(handle, Value, 0, 0, Buffer, &length) == ERROR_SUCCESS) {
         errnum = 1;
      }
      CloseHandle(handle);
   }
   return errnum;
}

/*********************************************************************************************************************
** Function: winGetDisplaySettings()
*/

int winGetDisplaySettings(int *bits, int *bytes, int *amtcolours)
{
   DEVMODE devmode;
   devmode.dmSize = sizeof(DEVMODE);
   devmode.dmDriverExtra = 0;

   if (EnumDisplaySettings(NULL, -1, &devmode)) {
      *bits  = devmode.dmBitsPerPel;

      if (*bits <= 8) *bits = 24; // Pretend that the screen is 24 bit even though it is 256 colours, as this produces better results

      if (*bits <= 8) { *amtcolours = 256; *bytes = 1; }
      else if (*bits <= 15) { *amtcolours = 32768; *bytes = 2; }
      else if (*bits <= 16) { *amtcolours = 65536; *bytes = 2; }
      else if (*bits <= 24) { *amtcolours = 16777216; *bytes = 3; }
      else if (*bits <= 32) { *amtcolours = 16777216; *bytes = 4; }

      return 1;
   }
   else return 0;
}

//********************************************************************************************************************

int winGetWindowInfo(HWND Window, int *X, int *Y, int *Width, int *Height, int *Maximised)
{
   if (Window) {
      WINDOWPLACEMENT info;
      info.length = sizeof(WINDOWPLACEMENT);
      if (GetWindowPlacement(Window, &info)) {
         RECT rect;
         if (GetWindowRect(Window, &rect)) {
            if (X) *X = rect.left;
            if (X) *Y = rect.top;
            if (GetClientRect(Window, &rect)) {
               if (Width) *Width  = rect.right  - rect.left;
               if (Height) *Height = rect.bottom - rect.top;

               int rectwidth = rect.right - rect.left;
               HWND winDesktop = GetDesktopWindow();
               GetWindowRect(winDesktop, &rect);

               if (rectwidth >= (rect.right - rect.left)) {
                  *Maximised = 1;
                  if (X) *X = 0;
                  if (Y) *Y = 0;
               }
               else *Maximised = 0;

               return 1;
            }
         }
      }
   }
   return 0;
}

//********************************************************************************************************************

static void HandleMovement(HWND window, WPARAM wparam, LPARAM lparam)
{
   if (glCursorEntry IS FALSE) {
      winSetCursor(glDefaultCursor);
      glCursorEntry = TRUE;

      {
         TRACKMOUSEEVENT event;
         ZeroMemory(&event, sizeof(event));
         event.cbSize      = sizeof(event);
         event.dwFlags     = TME_LEAVE;
         event.hwndTrack   = window;
         event.dwHoverTime = HOVER_DEFAULT;
         TrackMouseEvent(&event);
      }
   }

   // Get the absolute position of the mouse pointer relative to the desktop, then convert the coordinates relative to
   // the main window.

   int surface_id;
   if ((surface_id = winLookupSurfaceID(window))) {
      POINT point;
      GetCursorPos(&point);
      MsgMovement(surface_id, point.x, point.y, (int)(lparam & 0xffff), (lparam>>16) & 0xffff);
   }
}

//********************************************************************************************************************

static void HandleWheel(HWND window, WPARAM wparam, LPARAM lparam)
{
   // Get the absolute position of the mouse pointer relative to the desktop, then convert the coordinates relative to
   // the main window.

   int surface_id;
   if ((surface_id = winLookupSurfaceID(window))) {
      double delta = -((DOUBLE)GET_WHEEL_DELTA_WPARAM(wparam) / (DOUBLE)WHEEL_DELTA) * 3.0;
      MsgWheelMovement(surface_id, delta);
   }
}

//********************************************************************************************************************

static void HandleButtonPress(HWND window, int button)
{
   // Ensure that the clicked window will have the keyboard focus. This is especially important when running in an
   // embedded window.

   SetFocus(window);

   SetCapture(window);

   // Send a Parasol message

   MsgButtonPress(button, 1);
}

//********************************************************************************************************************

static void HandleButtonRelease(HWND window, int button)
{
   ReleaseCapture();
   MsgButtonPress(button, 0);
}

//********************************************************************************************************************
// Processes MSG_KEYDOWN messages, which are raw character values (useful for detecting keypresses that have no
// character representation).

static void HandleKeyPress(WPARAM value)
{
   RECT winrect, client, desktop;
   int left, top, width, height;
   POINT point;

   if ((glQualifiers & KQ_CTRL) AND (value IS VK_F11)) {
      // If CTRL+F11 is pressed, maximise the window to full screen

      if (glMainScreen) {
         GetWindowRect(glMainScreen, &winrect);
         GetClientRect(glMainScreen, &client);
         GetWindowRect(GetDesktopWindow(), &desktop);
         point.x = 0;
         point.y = 0;
         ClientToScreen(glMainScreen, &point);
         left = ((winrect.right - winrect.left) - (client.right - client.left)) / 2;
         top = point.y - winrect.top;

         if ((-left IS winrect.left) AND (-top IS winrect.top)) {
            SetWindowPos(glMainScreen, HWND_NOTOPMOST, 0, 0, desktop.right, desktop.bottom, 0);
         }
         else {
            GetWindowRect(GetDesktopWindow(), &desktop);
            width = desktop.right - desktop.left + (left * 2);
            height = (desktop.bottom - desktop.top) + ((winrect.bottom - winrect.top) - (client.bottom - client.top));
            ShowWindow(glMainScreen, SW_RESTORE);
            SetWindowPos(glMainScreen, HWND_TOPMOST, -left, -top, width, height, 0);
         }
      }
   }

   // Process normal key presses

   UBYTE keystate[256];
   WCHAR printable[2];
   LONG result;

   printable[0] = 0;

   if (GetKeyboardState(keystate)) {
      glQualifiers = 0;
      if (keystate[VK_LMENU] & 0x80)    glQualifiers |= KQ_L_ALT;
      if (keystate[VK_RMENU] & 0x80)    glQualifiers |= KQ_R_ALT;
      if (keystate[VK_LSHIFT] & 0x80)   glQualifiers |= KQ_L_SHIFT;
      if (keystate[VK_RSHIFT] & 0x80)   glQualifiers |= KQ_R_SHIFT;
      if (keystate[VK_LCONTROL] & 0x80) glQualifiers |= KQ_L_CONTROL;
      if (keystate[VK_RCONTROL] & 0x80) glQualifiers |= KQ_R_CONTROL;
      if (keystate[VK_LWIN] & 0x80)     glQualifiers |= KQ_L_COMMAND;
      if (keystate[VK_RWIN] & 0x80)     glQualifiers |= KQ_R_COMMAND;
      if (keystate[VK_CAPITAL] & 0x80)  glQualifiers |= KQ_CAPS_LOCK;

      // ToUnicode() is a Microsoft function that returns the key status, e.g. the number of keys and whether it is a
      // dead-key or not, and also writes the translated key to the printable buffer.

      result = ToUnicode(value, MapVirtualKey(value, 0), keystate, printable, sizeof(printable)/sizeof(printable[0]), 0);

      int flags = 0;
      if ((value >= 0x60) && (value < 0x70)) flags |= KQ_NUM_PAD;
      if (LOWORD(GetKeyState(VK_CAPITAL)) IS 1) flags |= KQ_CAPS_LOCK;
      if (keyconv[value]) MsgKeyPress(flags|glQualifiers, keyconv[value], printable[0]);
      else MSG("No equivalent key value for MS key %d.\n", (int)value);
   }
   else MSG("GetKeyboardState() failed.\n");
}

//********************************************************************************************************************

static void HandleKeyRelease(WPARAM value)
{
   char keystate[256];
   glQualifiers = 0;
   if (GetKeyboardState(keystate)) {
      if (keystate[VK_LMENU] & 0x80)    glQualifiers |= KQ_L_ALT;
      if (keystate[VK_RMENU] & 0x80)    glQualifiers |= KQ_R_ALT;
      if (keystate[VK_LSHIFT] & 0x80)   glQualifiers |= KQ_L_SHIFT;
      if (keystate[VK_RSHIFT] & 0x80)   glQualifiers |= KQ_R_SHIFT;
      if (keystate[VK_LCONTROL] & 0x80) glQualifiers |= KQ_L_CONTROL;
      if (keystate[VK_RCONTROL] & 0x80) glQualifiers |= KQ_R_CONTROL;
      if (keystate[VK_LWIN] & 0x80)     glQualifiers |= KQ_L_COMMAND;
      if (keystate[VK_RWIN] & 0x80)     glQualifiers |= KQ_R_COMMAND;
      if (keystate[VK_CAPITAL] & 0x80)  glQualifiers |= KQ_CAPS_LOCK;
   }

   if (keyconv[value]) MsgKeyRelease(glQualifiers, keyconv[value]);
}

void CALLBACK msg_timeout(HWND hwnd, UINT msg, UINT_PTR idEvent, DWORD time)
{
   MsgTimer();
}

//********************************************************************************************************************

static LRESULT CALLBACK WindowProcedure(HWND window, UINT msgcode, WPARAM wParam, LPARAM lParam)
{
   PAINTSTRUCT paint;
   HDC hdc;
   int newwidth, newheight, surfaceid;
   LPRECT rect;
   RECT winrect, client;

#ifdef DBGMSG
   int i;
   for (i=0; wincmd[i].code; i++) {
      if (msgcode IS wincmd[i].code) {
         fprintf(stderr, "WinProc: %s, $%.8x, $%.8x, Window: %p\n", wincmd[i].name, (int)wParam, (int)lParam, window);
         break;
      }
   }

   if (!wincmd[i].code) {
      fprintf(stderr, "WinProc: %d, $%.8x, $%.8x, Window: %p\n", msgcode, (int)wParam, (int)lParam, window);
   }
#endif

   switch (msgcode) {
      case WM_CLIPBOARDUPDATE:
         // Clipboard content has changed by some other application.  NOTE: It is common for some
         // applications to open and close the clipboard multiple times in succession, and this
         // causes multiple event triggers.  This problem is combated using intervals.

         // TODO: A better methodology would be to use a 1 second timer delay to process the clipboard

         glClipboardUpdates++;
         if (GetTickCount() - glIgnoreClip < 2000) return 1;
         else {
            win_clipboard_updated();
            return 0;
         }

      case WM_TIMER:
         MsgTimer(); // Calls ProcessMessages()
         return 0;

      case WM_SETCURSOR:
         switch(LOWORD(lParam)) {
            case HTBOTTOM:       SetCursor(glCursors[7].WinCursor); return TRUE;
            case HTBOTTOMLEFT:   SetCursor(glCursors[4].WinCursor); return TRUE;
            case HTBOTTOMRIGHT:  SetCursor(glCursors[3].WinCursor); return TRUE;
            case HTRIGHT:        SetCursor(glCursors[5].WinCursor); return TRUE;
            case HTLEFT:         SetCursor(glCursors[5].WinCursor); return TRUE;
            case HTTOP:          SetCursor(glCursors[7].WinCursor); return TRUE;
            case HTTOPLEFT:      SetCursor(glCursors[2].WinCursor); return TRUE;
            case HTTOPRIGHT:     SetCursor(glCursors[1].WinCursor); return TRUE;
            default:             SetCursor(glCurrentCursor); return TRUE;
         }

      case WM_CREATE:
         return DefWindowProc(window, msgcode, wParam, lParam);

      case WM_NCACTIVATE: // "Sent to a window when its nonclient area needs to be changed to indicate an active or inactive state."
         // When a window is about to be activated, we have the opportunity to prevent that activation.  We'll do this
         // if the surface is marked as non-interactive (e.g. a menu, which will prevent the main window from dropping its focus).
         //
         // lParam = The window that is going to be activated.  This will be 0 if the window belongs to some other task.

         if ((!lParam) OR (GetWindowLong((HWND)lParam, WE_INTERACTIVE) IS TRUE)) {
            MSG("WM_NCACTIVATE: Allow activation of window %p\n", (HWND)lParam);
            glDeferredActiveWindow = 0;
            return DefWindowProc(window, msgcode, wParam, lParam);
         }
         else { // Tell windows to avoid activating this window
            MSG("WM_NCACTIVATE: Do not activate window %p\n", (HWND)lParam);
            glDeferredActiveWindow = (HWND)lParam;
            return FALSE;
         }

      case WM_SYSCOMMAND:
         // If a popup window is active in our application and the user interacts with the main window menu
         // bar, no defocus event is sent for the popup window because the focus lies with the main window.
         // This little hack ensures that the popup window refers a lost-focus event.

         if (glDeferredActiveWindow) {
            MsgFocusState(winLookupSurfaceID(glDeferredActiveWindow), FALSE);
            glDeferredActiveWindow = 0;
         }
         return DefWindowProc(window, msgcode, wParam, lParam);

      case WM_ACTIVATE:
         //HWND otherwindow = lParam;
         if (LOWORD(wParam) IS WA_ACTIVE) {
            // Window activated by some method other than a mouse click
         }
         else if (LOWORD(wParam) IS WA_CLICKACTIVE) {
            // Window activated by a mouse click
         }
         else if (LOWORD(wParam) IS WA_INACTIVE) {
            // Window deactivated
         }

         return DefWindowProc(window, msgcode, wParam, lParam);

      case WM_MOUSEACTIVATE:
         // This messages indicates that the mouse has been used on a window that is marked as inactive.  Returning MA_NOACTIVATE prevents the window
         // being activated while also telling windows to convert the mouse action into a message for us to process.

         return MA_NOACTIVATE;

      case WM_ACTIVATEAPP:
         MSG("WM_ACTIVATEAPP: Focus: %d\n", (int)wParam);
         if (wParam) {
            // We have the focus
            MsgFocusState(winLookupSurfaceID(window), TRUE);
         }
         else {
            // We have lost the focus
            MsgFocusState(winLookupSurfaceID(window), FALSE);
         }
         return 0;

      case WM_MOVE: {
         int wx, wy, wwidth, wheight, cx, cy, cwidth, cheight;
         winGetCoords(window, &wx, &wy, &wwidth, &wheight, &cx, &cy, &cwidth, &cheight);
         MsgResizedWindow(winLookupSurfaceID(window), wx, wy, wwidth, wheight, cx, cy, cwidth, cheight);
         return 0;
      }

      case WM_SHOWWINDOW:
         return 0;

      case WM_PAINT: {
         if ((hdc = BeginPaint(window, &paint))) {
            RepaintWindow(winLookupSurfaceID(window), paint.rcPaint.left, paint.rcPaint.top, paint.rcPaint.right - paint.rcPaint.left, paint.rcPaint.bottom - paint.rcPaint.top);
            EndPaint(window, &paint);
         }
         return 0;
      }

      case WM_SIZE: {
         // Note that the WM_SIZE function tells us the size of the client area

         int cwidth  = LOWORD(lParam);
         int cheight = HIWORD(lParam);

         // If the window has just been maximised, check if the surface object has restrictions on the width and height.  If so, force the window back to its previous dimensions so that it obeys the developer's requirements.

         if (wParam & SIZE_MAXIMIZED) {
            WINDOWPLACEMENT win;

            if ((surfaceid = winLookupSurfaceID(window))) {
               newwidth  = cwidth;
               newheight = cheight;
               CheckWindowSize(surfaceid, &newwidth, &newheight);
               if ((newwidth != cwidth) OR (newheight != cheight)) {
                  ZeroMemory(&win, sizeof(WINDOWPLACEMENT));
                  win.length = sizeof(WINDOWPLACEMENT);
                  GetWindowPlacement(window, &win);
                  win.flags = 0;
                  win.showCmd = SW_RESTORE;
                  SetWindowPlacement(window, &win);
                  return 0;
               }
            }
         }

         // Send a resize message to the surface object

         int wx, wy, wwidth, wheight, cx, cy;
         winGetCoords(window, &wx, &wy, &wwidth, &wheight, &cx, &cy, &cwidth, &cheight);
         MsgResizedWindow(winLookupSurfaceID(window), wx, wy, wwidth, wheight, cx, cy, cwidth, cheight);
         return 0;
      }

      case WM_WINDOWPOSCHANGING: {
         LPWINDOWPOS winpos = (LPWINDOWPOS)lParam;
         winpos->flags |= SWP_NOCOPYBITS|SWP_NOREDRAW;
         return 0;
      }

      case WM_SIZING: {
         // This procedure is called when the user is resizing a window by its anchor points.

         GetWindowRect(window, &winrect);
         GetClientRect(window, &client);
         rect = (LPRECT)lParam;
         int cwidth  = (rect->right - rect->left) - ((winrect.right - winrect.left) - (client.right - client.left));
         int cheight = (rect->bottom - rect->top) - ((winrect.bottom- winrect.top) - (client.bottom - client.top));
         CheckWindowSize(winLookupSurfaceID(window), &cwidth, &cheight);
         if ((wParam IS WMSZ_BOTTOMRIGHT) OR (wParam IS WMSZ_RIGHT) OR (wParam IS WMSZ_TOPRIGHT))    rect->right  = rect->left + cwidth + ((winrect.right - winrect.left) - (client.right - client.left));
         if ((wParam IS WMSZ_BOTTOMRIGHT) OR (wParam IS WMSZ_BOTTOM) OR (wParam IS WMSZ_BOTTOMLEFT)) rect->bottom = rect->top + cheight + (winrect.bottom - client.bottom - winrect.top);
         if ((wParam IS WMSZ_BOTTOMLEFT) OR (wParam IS WMSZ_LEFT) OR (wParam IS WMSZ_TOPLEFT))       rect->left   = rect->right - cwidth - ((winrect.right - winrect.left) - (client.right - client.left));
         if ((wParam IS WMSZ_TOPLEFT) OR (wParam IS WMSZ_TOP) OR (wParam IS WMSZ_TOPRIGHT))          rect->top    = rect->bottom - cheight - ((winrect.bottom - winrect.top) - (client.bottom - client.top));
         return 0;
      }

      case WM_KILLFOCUS:     // Window has lost the focus.  Also see WM_SETFOCUS
                             glQualifiers = 0; // Kill stored qualifiers when the keyboard is lost
                             return 0;

      case WM_KEYUP:         HandleKeyRelease(wParam); return 0;

      case WM_KEYDOWN:       HandleKeyPress(wParam); return 0;

      case WM_SYSKEYDOWN:    // The ALT keys are treated differently to everything else
                             if (wParam IS VK_MENU) {
                                if (lParam & (1<<24)) HandleKeyPress(VK_RMENU);
                                else HandleKeyPress(VK_MENU);
                             }
                             else HandleKeyPress(wParam);
                             return 0;

      case WM_SYSKEYUP:      if (wParam IS VK_MENU) {
                                if (lParam & (1<<24)) HandleKeyRelease(VK_RMENU);
                                else HandleKeyRelease(VK_MENU);
                             }
                             else HandleKeyRelease(wParam);
                             return 0;

      case WM_MOUSEMOVE:     HandleMovement(window, wParam, lParam); return 0;

      case WM_MOUSELEAVE:    glCursorEntry = FALSE;
                             return 0;

      case WM_MOUSEWHEEL:    HandleWheel(window, wParam, lParam); return 0;

      case WM_LBUTTONDOWN:   HandleButtonPress(window, WIN_LMB); return 0;
      case WM_RBUTTONDOWN:   HandleButtonPress(window, WIN_RMB); return 0;
      case WM_MBUTTONDOWN:   HandleButtonPress(window, WIN_MMB); return 0;
      case WM_LBUTTONDBLCLK: HandleButtonPress(window, WIN_DBL|WIN_LMB); return 0;
      case WM_RBUTTONDBLCLK: HandleButtonPress(window, WIN_DBL|WIN_RMB); return 0;
      case WM_MBUTTONDBLCLK: HandleButtonPress(window, WIN_DBL|WIN_MMB); return 0;
      case WM_LBUTTONUP:     HandleButtonRelease(window, WIN_LMB); return 0;
      case WM_RBUTTONUP:     HandleButtonRelease(window, WIN_RMB); return 0;
      case WM_MBUTTONUP:     HandleButtonRelease(window, WIN_MMB); return 0;

      case WM_ICONNOTIFY:
         if (lParam IS WM_LBUTTONDOWN) {
            ShowWindow(window, SW_SHOWNORMAL); // Bring window out of minimisation
            SetForegroundWindow(window); // Focus is required in order to go to the front
            SetWindowPos(window, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE|SWP_NOSIZE);
/*
            int surface_id;
            if ((surface_id = winLookupSurfaceID(window))) {
               MsgShowObject(surface_id);
            }
*/
         }
         else if (lParam IS WM_RBUTTONUP) {
#if 0
            ShowWindow(window, SW_SHOWNORMAL);

            POINT point;
            HMENU hMenu, hSubMenu;
            // Get mouse position
            GetCursorPos(&point);
            // Popup context menu
            hMenu = LoadMenu(hInst, MAKEINTRESOURCE(IDR_MYMENU));
            hSubMenu = GetSubMenu(hMenu, 0);
            SetMenuDefaultItem(hSubMenu, IDM_DEFAULTCMD, FALSE);
            SetForegroundWindow(hMainDlg);         // Per KB Article Q135788
            TrackPopupMenu(hSubMenu, TPM_LEFTBUTTON|TPM_RIGHTBUTTON|TPM_LEFTALIGN, point.x, point.y, 0, hWnd, NULL);
            PostMessage(hMainDlg, WM_NULL, 0, 0);   // Per KB Article Q135788
            DestroyMenu(hMenu);
#endif
         }
         break;

      //case WM_CAPTURECHANGED:
         // Sent to the window that is losing the mouse capture, refers to the window gaining the capture.
         // return 0;

      case WM_CLOSE: MsgWindowClose(winLookupSurfaceID(window));
                     return 0;

      case WM_DESTROY: MsgWindowDestroyed(winLookupSurfaceID(window));
                       return 0;

      case WM_SETFOCUS:
         // The window has gained the keyboard focus.  MSDN says this is for displaying a caret if the window accepts text input.

         MSG("WM_SETFOCUS: Surface: %d\n", winLookupSurfaceID(window));
         MsgSetFocus(winLookupSurfaceID(window));
         return 0;

      // Windows hangs on DispatchMessage() when the user tries to resize a window.  Handlers for this can be
      // setup when receiving the following two windows messages.

      case WM_ENTERSIZEMOVE:
         MSG("WM_ENTERSIZEMOVE\n");
         SetTimer(window, IDT_RESIZE_WINDOW, 20, &msg_timeout); // Set a timer to help when DispatchMessage() doesn't return in good time.
         return 0;

      case WM_EXITSIZEMOVE:
         MSG("WM_EXITSIZEMOVE\n");
         KillTimer(window, IDT_RESIZE_WINDOW);
         return 0;

      default:
         // AutoPlay handling - allows you to cancel autoplay, but only if your window is in the foreground.

         if (!glCancelAutoPlayMsg) {
            glCancelAutoPlayMsg = RegisterWindowMessage(TEXT("QueryCancelAutoPlay"));
         }

         if (msgcode IS glCancelAutoPlayMsg) {
            fprintf(stderr, "An AutoPlay window has been cancelled.\n");
            return TRUE;       // cancel auto-play
         }

         return DefWindowProc(window, msgcode, wParam, lParam);
   }

   return 0;
}

//********************************************************************************************************************

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
void winSetSurfaceID(HWND Window, int SurfaceID)
{
   SetProp(Window, "SurfaceID", (HANDLE)SurfaceID);
   SetWindowLong(Window, WE_SURFACE, SurfaceID);
   SetWindowLong(Window, WE_KEY, KEY_SURFACE);
}
#pragma GCC diagnostic pop

//********************************************************************************************************************

void winDisableBatching(void)
{
   GdiSetBatchLimit(1);
}

//********************************************************************************************************************

int winGetDesktopSize(int *width, int *height)
{
   HWND window;
   RECT rect;

   *width = 800;
   *height = 600;
   if ((window = GetDesktopWindow())) {
      if (GetWindowRect(window, &rect)) {
         *width  = rect.right;
         *height = rect.bottom;
         return 1;
      }
   }
   return 0;
}

//********************************************************************************************************************

int winCreateScreenClass(void)
{
   WNDCLASSEX winclass;

   if (!fmtShellIDList) fmtShellIDList = RegisterClipboardFormat(CFSTR_SHELLIDLIST);
   if (!fmtPasteSucceeded) fmtPasteSucceeded = RegisterClipboardFormat(CFSTR_PASTESUCCEEDED);
   if (!fmtPerformedDropEffect) fmtPerformedDropEffect = RegisterClipboardFormat(CFSTR_PERFORMEDDROPEFFECT);
   if (!fmtPreferredDropEffect) fmtPreferredDropEffect = RegisterClipboardFormat(CFSTR_PREFERREDDROPEFFECT);
   if (!fmtParasolClip) fmtParasolClip = RegisterClipboardFormat("Parasol");

   if (!glCancelAutoPlayMsg) {
      glCancelAutoPlayMsg = RegisterWindowMessage(TEXT("QueryCancelAutoPlay"));
   }

   glDefaultCursor = LoadCursor(NULL, IDC_ARROW);

   winclass.cbSize        = sizeof(winclass);
   winclass.style         = CS_DBLCLKS;
   winclass.lpfnWndProc   = WindowProcedure;
   winclass.cbClsExtra    = 0;
   winclass.cbWndExtra    = sizeof(struct winextra);
   winclass.hInstance     = glInstance;
   if (!(winclass.hIcon = LoadIcon(glInstance, MAKEINTRESOURCE(500)))) winclass.hIcon = LoadIcon(glInstance, IDI_APPLICATION);
   winclass.hCursor       = NULL; //glDefaultCursor;
   winclass.hbrBackground = NULL;
   winclass.lpszMenuName  = NULL;
   winclass.lpszClassName = "ScreenClass";
   winclass.hIconSm       = NULL;

   if (RegisterClassEx(&winclass)) {
      glScreenClassInit = 1;

      if (!glOleInit) {
         HRESULT result = OleInitialize(NULL);
         if (result IS S_OK) glOleInit = 1; // 1 = Successful initialisation
         else if (result IS S_FALSE) glOleInit = 2; // 2 = Attempted initialisation failed.
      }

      return 1;
   }
   else return 0;
}

/*********************************************************************************************************************
** Function: winCreateScreen()
**
** WS_EX_ACCEPTFILES: Specifies that a window created with this style accepts drag-drop files.
** WS_EX_APPWINDOW: Forces a top-level window onto the taskbar when the window is visible.
** WS_EX_CLIENTEDGE: Specifies that a window has a border with a sunken edge.
** WS_EX_CONTROLPARENT: The window itself contains child windows that should take part in dialog box navigation. If this style is specified, the dialog manager recurses into children of this window when performing navigation operations such as handling the TAB key, an arrow key, or a keyboard mnemonic.
** WS_EX_DLGMODALFRAME: Creates a window that has a double border; the window can, optionally, be created with a title bar by specifying the WS_CAPTION style in the dwStyle parameter.
** WS_EX_LAYERED: Windows 2000/XP: Creates a layered window. Note that this cannot be used for child windows. Also, this cannot be used if the window has a class style of either CS_OWNDC or CS_CLASSDC.
** WS_EX_NOACTIVATE: Windows 2000/XP: A top-level window created with this style does not become the foreground window when the user clicks it. The system does not bring this window to the foreground when the user minimizes or closes the foreground window.  To activate the window, use the SetActiveWindow or SetForegroundWindow function.  The window does not appear on the taskbar by default. To force the window to appear on the taskbar, use the WS_EX_APPWINDOW style.
** WS_EX_NOINHERITLAYOUT: Windows 2000/XP: A window created with this style does not pass its window layout to its child windows.
** WS_EX_NOPARENTNOTIFY: Specifies that a child window created with this style does not send the WM_PARENTNOTIFY message to its parent window when it is created or destroyed.
** WS_EX_OVERLAPPEDWINDOW: Combines the WS_EX_CLIENTEDGE and WS_EX_WINDOWEDGE styles.
** WS_EX_STATICEDGE: Creates a window with a three-dimensional border style intended to be used for items that do not accept user input.
** WS_EX_TOPMOST: Specifies that a window created with this style should be placed above all non-topmost windows and should stay above them, even when the window is deactivated. To add or remove this style, use the SetWindowPos function.
** WS_EX_TRANSPARENT: Specifies that a window created with this style should not be painted until siblings beneath the window (that were created by the same thread) have been painted. The window appears transparent because the bits of underlying sibling windows have already been painted.  To achieve transparency without these restrictions, use the SetWindowRgn function.
** WS_EX_WINDOWEDGE: Specifies that a window has a border with a raised edge.
**********************************************************************************************************************
** WS_BORDER: Creates a window that has a thin-line border.
** WS_CAPTION: Creates a window that has a title bar (includes the WS_BORDER style).
** WS_CHILD: Creates a child window. A window with this style cannot have a menu bar. This style cannot be used with the WS_POPUP style.
** WS_CLIPCHILDREN: Excludes the area occupied by child windows when drawing occurs within the parent window. This style is used when creating the parent window.
** WS_CLIPSIBLINGS: Clips child windows relative to each other; that is, when a particular child window receives a WM_PAINT message, the WS_CLIPSIBLINGS style clips all other overlapping child windows out of the region of the child window to be updated. If WS_CLIPSIBLINGS is not specified and child windows overlap, it is possible, when drawing within the client area of a child window, to draw within the client area of a neighboring child window.
** WS_DISABLED: Creates a window that is initially disabled. A disabled window cannot receive input from the user. To change this after a window has been created, use EnableWindow.
** WS_DLGFRAME: Creates a window that has a border of a style typically used with dialog boxes. A window with this style cannot have a title bar.
** WS_GROUP: Specifies the first control of a group of controls. The group consists of this first control and all controls defined after it, up to the next control with the WS_GROUP style. The first control in each group usually has the WS_TABSTOP style so that the user can move from group to group. The user can subsequently change the keyboard focus from one control in the group to the next control in the group by using the direction keys.  You can turn this style on and off to change dialog box navigation. To change this style after a window has been created, use SetWindowLong.
** WS_ICONIC: Creates a window that is initially minimized. Same as the WS_MINIMIZE style.
** WS_MAXIMIZE: Creates a window that is initially maximized.
** WS_MAXIMIZEBOX: Creates a window that has a maximize button. Cannot be combined with the WS_EX_CONTEXTHELP style. The WS_SYSMENU style must also be specified.
** WS_MINIMIZE: Creates a window that is initially minimized. Same as the WS_ICONIC style.
** WS_MINIMIZEBOX: Creates a window that has a minimize button. Cannot be combined with the WS_EX_CONTEXTHELP style. The WS_SYSMENU style must also be specified.
** WS_OVERLAPPED: Creates an overlapped window. An overlapped window has a title bar and a border. Same as the WS_TILED style.
** WS_OVERLAPPEDWINDOW: Creates an overlapped window with the WS_OVERLAPPED, WS_CAPTION, WS_SYSMENU, WS_THICKFRAME, WS_MINIMIZEBOX, and WS_MAXIMIZEBOX styles. Same as the WS_TILEDWINDOW style.
** WS_POPUP: Creates a pop-up window. This style cannot be used with the WS_CHILD style.
** WS_POPUPWINDOW: Creates a pop-up window with WS_BORDER, WS_POPUP, and WS_SYSMENU styles. The WS_CAPTION and WS_POPUPWINDOW styles must be combined to make the window menu visible.
** WS_SIZEBOX: Creates a window that has a sizing border. Same as the WS_THICKFRAME style.
** WS_SYSMENU: Creates a window that has a window menu on its title bar. The WS_CAPTION style must also be specified.
** WS_TABSTOP: Specifies a control that can receive the keyboard focus when the user presses the TAB key. Pressing the TAB key changes the keyboard focus to the next control with the WS_TABSTOP style. You can turn this style on and off to change dialog box navigation. To change this style after a window has been created, use SetWindowLong.
** WS_THICKFRAME: Creates a window that has a sizing border. Same as the WS_SIZEBOX style.
** WS_TILED: Creates an overlapped window. An overlapped window has a title bar and a border. Same as the WS_OVERLAPPED style.
** WS_TILEDWINDOW: Creates an overlapped window with the WS_OVERLAPPED, WS_CAPTION, WS_SYSMENU, WS_THICKFRAME, WS_MINIMIZEBOX, and WS_MAXIMIZEBOX styles. Same as the WS_OVERLAPPEDWINDOW style.
** WS_VISIBLE: Creates a window that is initially visible. This style can be turned on and off by using ShowWindow or SetWindowPos.
*/

HWND winCreateScreen(HWND PopOver, int *X, int *Y, int *Width, int *Height, char Maximise, char Borderless, const char *Name,
   char Composite, unsigned char Opacity, char Desktop)
{
   if (!Name) Name = "Parasol";

   char interactive;
   if ((Borderless) AND (!glTrayIcon) AND (!glTaskBar)) interactive = FALSE;
   else interactive = TRUE;

   HWND Window;
   if (Borderless) {
      if (!(Window = CreateWindowEx(
            (glTaskBar ? WS_EX_APPWINDOW : WS_EX_TOOLWINDOW) | (glStickToFront ? WS_EX_TOPMOST : 0),
            "ScreenClass", (glTaskBar ? Name : NULL),
            WS_POPUP|WS_CLIPCHILDREN|WS_CLIPSIBLINGS|(Maximise ? WS_MAXIMIZE : 0),
            *X, *Y,
            CW_USEDEFAULT, CW_USEDEFAULT,
            (HWND)PopOver,
            (HMENU)NULL,
            glInstance, NULL))) return NULL;
   }
   else if (!(Window = CreateWindowEx(
      (glTaskBar ? WS_EX_APPWINDOW : 0) | WS_EX_WINDOWEDGE | (glStickToFront ? WS_EX_TOPMOST : 0),
      "ScreenClass", Name,
      WS_SIZEBOX|WS_MINIMIZEBOX|WS_MAXIMIZEBOX|WS_CAPTION|WS_SYSMENU|WS_CLIPCHILDREN|WS_CLIPSIBLINGS|(Maximise ? WS_MAXIMIZE : 0),
      *X, *Y,
      CW_USEDEFAULT, CW_USEDEFAULT,
      (HWND)PopOver,
      (HMENU)NULL,
      glInstance, NULL))) return NULL;

   // Set the width and height of the window

   RECT winrect, client;
   GetWindowRect(Window, &winrect);
   GetClientRect(Window, &client);
   int width = *Width + (winrect.right - client.right - winrect.left);
   int height = *Height + (winrect.bottom - client.bottom - winrect.top);
   MoveWindow(Window, winrect.left, winrect.top, width, height, FALSE);

   // Return the absolute coordinates of the client region

   winGetCoords(Window, 0, 0, 0, 0, X, Y, Width, Height);

   if (glTrayIcon) {
      NOTIFYICONDATA nid;
      nid.cbSize = sizeof(NOTIFYICONDATA);
      nid.hWnd   = Window;
      nid.uID    = ID_TRAY;
      nid.uFlags = NIF_ICON | NIF_MESSAGE;
      nid.uCallbackMessage = WM_ICONNOTIFY;
      if (!(nid.hIcon = LoadIcon(glInstance, MAKEINTRESOURCE(500)))) nid.hIcon = LoadIcon(glInstance, IDI_APPLICATION);
      Shell_NotifyIcon(NIM_ADD, &nid);
   }

   if (glStickToFront > 0) glStickToFront--;

   if ((Composite) OR (Opacity < 255)) {
      SetLastError(0);
      if (!SetWindowLong(Window, GWL_EXSTYLE, GetWindowLong(Window, GWL_EXSTYLE) | WS_EX_LAYERED)) {
         if (!GetLastError()) {
            return NULL;
         }
      }

      if (!Composite) {
         if (!SetLayeredWindowAttributes(Window, 0, Opacity, LWA_ALPHA)) {
            return NULL;
         }
      }
   }

   SetWindowLong(Window, WE_INTERACTIVE, interactive);
   SetWindowLong(Window, WE_BORDERLESS, Borderless);

   if ((Desktop) AND (!glMainScreen)) glMainScreen = Window;

   AddClipboardFormatListener(Window);

   winInitDragDrop(Window);

   return Window;
}

/*********************************************************************************************************************
** This is often used for creating windowed areas inside another application, such as a web browser.
*/

HWND winCreateChild(HWND Parent, int X, int Y, int Width, int Height)
{
   HWND Window;

   if ((Window = CreateWindowEx(
      0, // WS_EX_NOPARENTNOTIFY
      "ScreenClass", "Parasol Child Window",
      WS_CHILD|WS_CLIPCHILDREN|WS_CLIPSIBLINGS,
      0, 0, Width, Height,
      Parent,
      (HMENU)0,
      glInstance, 0))) {

      glMainScreen = Window;

      return Window;
   }
   else return NULL;
}

//********************************************************************************************************************

int winHideWindow(HWND window)
{
   return ShowWindow(window, SW_HIDE);
}

//********************************************************************************************************************

void winMoveToBack(HWND Window)
{
   SetWindowPos(Window, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE|SWP_NOSIZE);
}

//********************************************************************************************************************

void winFocus(HWND Window)
{
   SetForegroundWindow(Window);
}

//********************************************************************************************************************

void winMoveToFront(HWND Window)
{
   // Note: The window will require the focus if it is to be moved in front of other MS Windows on the desktop (a window can gain the focus with SetForegroundWindow()).

   SetWindowPos(Window, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE|SWP_NOSIZE);

/* This code demonstrates a way to move a window to the front without giving
   it the focus.

  SetWindowPos(Window, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE|SWP_NOSIZE);
  SetWindowPos(Window, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE|SWP_NOSIZE);
*/
}

//********************************************************************************************************************

void winGetWindowTitle(HWND Window, char *Buffer, int Size)
{
   GetWindowText(Window, Buffer, Size);
}

//********************************************************************************************************************

void winSetWindowTitle(HWND window, const char *title)
{
   SetWindowText(window, title);
}

//********************************************************************************************************************

int winShowWindow(HWND window, int Maximise)
{
   int result;

   if (GetWindowLong(window, WE_BORDERLESS) IS TRUE) {
      // Raw surfaces (composites, borderless windows etc) do not get the focus automatically.
      // This mirrors the functionality within the Parasol desktop.

      result = ShowWindow(window, Maximise ? SW_SHOWMAXIMIZED : SW_SHOWNOACTIVATE);
   }
   else {
      if ((!Maximise) AND (IsIconic(window))) {
         // Window is minimised - restore it to its original position
         result = ShowWindow(window, SW_RESTORE);
      }
      else result = ShowWindow(window, Maximise ? SW_SHOWMAXIMIZED : SW_SHOWNOACTIVATE);
   }

   return result;
}

//********************************************************************************************************************

void winUpdateWindow(HWND hWnd)
{
   UpdateWindow(hWnd);
}

//********************************************************************************************************************

HINSTANCE winGetModuleHandle(void)
{
   return GetModuleHandle(NULL);
}

//********************************************************************************************************************

int winDestroyWindow(HWND window)
{
   NOTIFYICONDATA notify;

   if (window == glMainScreen) glMainScreen = NULL;

   ZeroMemory(&notify, sizeof(notify));
   notify.cbSize = sizeof(notify);
   notify.uID    = ID_TRAY;
   notify.hWnd   = window;
   notify.uFlags = 0;
   Shell_NotifyIcon(NIM_DELETE, &notify);

   return DestroyWindow(window);
}

//********************************************************************************************************************

void winRemoveWindowClass(const char *ClassName)
{
   UnregisterClass(ClassName, glInstance);
}

/*********************************************************************************************************************
** The coordinates are interpreted as being indicative of the client area.
*/

int winMoveWindow(HWND Window, int X, int Y)
{
   WINDOWINFO info;

   info.cbSize = sizeof(info);
   if (GetWindowInfo(Window, &info)) {
      X = X - (info.rcClient.left - info.rcWindow.left);
      Y = Y - (info.rcClient.top - info.rcClient.top);
      return MoveWindow(Window, X, Y, info.rcWindow.right - info.rcWindow.left, info.rcWindow.bottom - info.rcWindow.top, TRUE);
   }
   else return 0;
}

/*********************************************************************************************************************
** The coordinates are interpreted as being relative to the client area.
*/

int winResizeWindow(HWND Window, int X, int Y, int Width, int Height)
{
   WINDOWINFO info;

   info.cbSize = sizeof(info);
   if (!GetWindowInfo(Window, &info)) return 0;

   if (X IS 0x7fffffff) X = info.rcClient.left;
   if (Y IS 0x7fffffff) Y = info.rcClient.top;

   // Return if the current size is the same as the 'new' size

   if ((Width IS (info.rcClient.right - info.rcClient.left)) AND
       (Height IS (info.rcClient.bottom - info.rcClient.top)) AND
       (X IS info.rcClient.left) AND
       (Y IS info.rcClient.top)) return 1;

   // Convert the client coordinates to window coordinates

   X = X - (info.rcClient.left - info.rcWindow.left);
   Y = Y - (info.rcClient.top - info.rcWindow.top);
   Width = Width + (info.rcClient.left - info.rcWindow.left) + (info.rcWindow.right - info.rcClient.right);
   Height = Height + (info.rcClient.top - info.rcWindow.top) + (info.rcWindow.bottom - info.rcClient.bottom);

   // Perform the resize

   return MoveWindow(Window, X, Y, Width, Height, TRUE);
}

//********************************************************************************************************************

void winGetMargins(HWND Window, int *Left, int *Top, int *Right, int *Bottom)
{
   WINDOWINFO info;

   info.cbSize = sizeof(info);
   if (!GetWindowInfo(Window, &info)) return;

   *Left   = info.rcClient.left - info.rcWindow.left;
   *Top    = info.rcClient.top - info.rcWindow.top;
   *Right  = info.rcWindow.right - info.rcClient.right;
   *Bottom = info.rcWindow.bottom - info.rcClient.bottom;
}

//********************************************************************************************************************

int winSettings(int Flags)
{
   if (Flags & WNS_PLUGIN) {
//      KillMessageHook();
   }
   return 0;
}

//********************************************************************************************************************

void precalc_rgb(unsigned char *Data, unsigned char *Dest, int Width, int Height)
{
   unsigned char *data, *dest;
   int x, y;

   for (y=0; y < Height; y++) {
      data = Data + (Width * 4 * y);
      dest = Dest + (Width * 4 * y);
      for (x=0; x < Width; x++) {
         dest[3] = data[3];
         dest[0]= ((int)data[0] * (int)dest[3])>>8;
         dest[1]= ((int)data[1] * (int)dest[3])>>8;
         dest[2]= ((int)data[2] * (int)dest[3])>>8; // divide by 255;
         data += 4;
         dest += 4;
      }
   }
}

void win32RedrawWindow(HWND Window, HDC WindowDC, int X, int Y, int Width,
   int Height, int XDest, int YDest, int ScanWidth, int ScanHeight,
   int BPP, void *Data, int RedMask, int GreenMask, int BlueMask, int AlphaMask, int Opacity)
{
   HDC dcMemory;
   HBITMAP bmp;
   HBITMAP *pOldBitmap;
   POINT ptSrc = { 0, 0 };
   SIZE size;
   char direct_blit;
   BITMAPV4HEADER info;
   RECT rect;
   void *alpha_data;

   info.bV4Size          = sizeof(info);
   info.bV4Width         = ScanWidth;     // Width in pixels
   info.bV4Height        = -ScanHeight;   // Height in pixels
   info.bV4Planes        = 1;             // Always 1
   info.bV4BitCount      = BPP;           // Bits per pixel
   info.bV4SizeImage     = 0;
   info.bV4XPelsPerMeter = 0;
   info.bV4YPelsPerMeter = 0;
   info.bV4ClrUsed       = 0;
   info.bV4ClrImportant  = 0;
   info.bV4RedMask       = RedMask;
   info.bV4GreenMask     = GreenMask;
   info.bV4BlueMask      = BlueMask;
   info.bV4AlphaMask     = AlphaMask;
   info.bV4CSType        = 0;
   info.bV4GammaRed      = 0;
   info.bV4GammaGreen    = 0;
   info.bV4GammaBlue     = 0;

   // NB: wingdi.h sometimes defines bV4Compression as bV4V4Compression

   if (BPP IS 24) info.bV4V4Compression = BI_RGB; // Must use BI_RGB in 24bit mode, or GDI does nothing
   else info.bV4V4Compression = BI_BITFIELDS; // Must use BI_BITFIELDS and set the RGB masks in other packed modes

   if (info.bV4BitCount IS 15) info.bV4BitCount = 16;

   direct_blit = TRUE;
   if (GetWindowLong(Window, GWL_EXSTYLE) & WS_EX_LAYERED) {
      if (AlphaMask) {
         BLENDFUNCTION blend_alpha = { AC_SRC_OVER, 0, Opacity, AC_SRC_ALPHA };

         GetWindowRect(Window,&rect);
         size.cx = rect.right - rect.left;
         size.cy = rect.bottom - rect.top;

         dcMemory = NULL;
         if (!dcMemory) {
            if ((dcMemory = CreateCompatibleDC(WindowDC))) {
               if (!(bmp = CreateDIBSection(WindowDC, (BITMAPINFO *)&info, DIB_RGB_COLORS, &alpha_data, NULL, 0))) {
                  DeleteDC(dcMemory);
                  dcMemory = NULL;
               }
            }
         }

         if (dcMemory) {
            //printf("bpp %d, XD %d, YD %d, Width %d, Height %d, X %d, Y %d, ScanHeight %d\n", (int)BPP, XDest, YDest, Width, Height, X, Y, ScanHeight);

            precalc_rgb(Data, alpha_data, ScanWidth, ScanHeight);

            // SetDIBitsToDevice() defines the size of the window for UpdateLayeredWindow().  This is crazy, because
            // that means that the entire layer needs to be updated every time, making this process terribly slow.
            // However MS documentation confirms as much in the API documentation.

            X = 0;
            Y = 0;
            XDest = 0;
            YDest = 0;
            Width = size.cx;
            Height = size.cy;

            pOldBitmap = SelectObject(dcMemory, bmp);
            SetDIBitsToDevice(dcMemory, XDest, YDest, Width, Height, X, ScanHeight - (Y + Height), 0, ScanHeight, alpha_data, (BITMAPINFO *)&info, DIB_RGB_COLORS);

            UpdateLayeredWindow(Window, NULL, NULL, &size, dcMemory, &ptSrc, 0, &blend_alpha, ULW_ALPHA);

            direct_blit = FALSE;
            SelectObject(dcMemory, pOldBitmap);
            DeleteObject(bmp);
            DeleteDC(dcMemory);
         }
         //else printf("Failed to create DC and/or temporary bitmap.");
      }
      else SetLayeredWindowAttributes(Window, 0, Opacity, LWA_ALPHA);
   }

   if (direct_blit) {
      SetDIBitsToDevice(WindowDC, XDest, YDest, Width, Height, X, ScanHeight - (Y + Height), 0, ScanHeight, Data, (BITMAPINFO *)&info, DIB_RGB_COLORS);
   }
}

//********************************************************************************************************************

LONG winGetPixelFormat(int *redmask, int *greenmask, int *bluemask, int *alphamask)
{
   PIXELFORMATDESCRIPTOR  pfd;
   const unsigned char formats[] = { 0x00, 0x01, 0x03, 0x07, 0x0f, 0x1f, 0x3f, 0x7f, 0xff };
   static int mred=0, mgreen=0, mblue=0, malpha=0;

   // WARNING: Calling DescribePixelFormat() causes layered windows to flicker for some bizarre reason.  Therefore this routine has been modified so that DescribePixelFormat() is only called once.

   if (!mred) {
      if (DescribePixelFormat(GetDC(NULL), 1, sizeof(PIXELFORMATDESCRIPTOR), &pfd)) {
         if (pfd.cRedBits <= 8) mred = formats[pfd.cRedBits] << pfd.cRedShift;
         if (pfd.cGreenBits <= 8) mgreen = formats[pfd.cGreenBits] << pfd.cGreenShift;
         if (pfd.cBlueBits <= 8) mblue = formats[pfd.cBlueBits] << pfd.cBlueShift;
         if (pfd.cAlphaBits <= 8) malpha = formats[pfd.cAlphaBits] << pfd.cAlphaShift;
      }
   }

   if (mred) {
      *redmask = mred;
      *greenmask = mgreen;
      *bluemask = mblue;
      *alphamask = malpha;
      return 0;
   }
   else return -1;
}

//********************************************************************************************************************

void winGetError(LONG Error, char *Buffer, LONG BufferSize)
{
   FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, 0, Error, 0, Buffer, BufferSize, 0);
}

//********************************************************************************************************************

void winDrawLine(HDC hdc, LONG x1, LONG y1, LONG x2, LONG y2, UBYTE *rgb)
{
   HPEN pen, oldpen;

   if ((pen = CreatePen(PS_SOLID, 1, RGB(rgb[0], rgb[1], rgb[2])))) {
      if ((oldpen = SelectObject(hdc, pen))) {
         MoveToEx(hdc, x1, y1, NULL);
         LineTo(hdc, x2, y2);
         SelectObject(hdc, oldpen);
      }
      DeleteObject(pen);
   }
}

//********************************************************************************************************************

void winDrawRectangle(HDC hdc, LONG x, LONG y, LONG width, LONG height, UBYTE red, UBYTE green, UBYTE blue)
{
   RECT rect;
   HBRUSH brush;

   rect.left   = x;
   rect.top    = y;
   rect.right  = x + width;
   rect.bottom = y + height;
   brush = CreateSolidBrush(RGB(red,green,blue));
   FillRect(hdc, &rect, brush);
   DeleteObject(brush);
}

/*********************************************************************************************************************
** Sets a new clipping region for a DC.
*/

LONG winSetClipping(HDC hdc, LONG left, LONG top, LONG right, LONG bottom)
{
   HRGN region;

   if ((!right) OR (!bottom)) {
      SelectClipRgn(hdc, NULL);
      return 1;
   }

   if ((region = CreateRectRgn(left, top, right, bottom))) {
      SelectClipRgn(hdc, region);
      DeleteObject(region);
      return 1;
   }
   return 0;
}

//********************************************************************************************************************

LONG winBlit(HDC dest, LONG xdest, LONG ydest, LONG width, LONG height, HDC src, LONG x, LONG y)
{
   if (!BitBlt(dest, xdest, ydest, width, height, src, x, y, SRCCOPY)) {
      return GetLastError();
   }
   else return 0;
}

//********************************************************************************************************************

void * winCreateCompatibleDC(void)
{
   return CreateCompatibleDC(NULL);
}

//********************************************************************************************************************

void winDeleteObject(void *Object)
{
   DeleteObject(Object);
}

//********************************************************************************************************************

void winSetDIBitsToDevice(HDC hdc, LONG xdest, LONG ydest, LONG width, LONG height,
        LONG xstart, LONG ystart, LONG scanwidth, LONG scanheight, LONG bpp, void *data,
        LONG redmask, LONG greenmask, LONG bluemask)
{
   BITMAPV4HEADER info;

   info.bV4Size          = sizeof(info);
   info.bV4Width         = scanwidth;     // Width in pixels
   info.bV4Height        = -scanheight;   // Height in pixels
   info.bV4Planes        = 1;             // Always 1
   info.bV4BitCount      = bpp;           // Bits per pixel
   info.bV4SizeImage     = 0;
   info.bV4XPelsPerMeter = 0;
   info.bV4YPelsPerMeter = 0;
   info.bV4ClrUsed       = 0;
   info.bV4ClrImportant  = 0;
   info.bV4RedMask       = redmask;
   info.bV4GreenMask     = greenmask;
   info.bV4BlueMask      = bluemask;
   info.bV4AlphaMask     = 0;
   info.bV4CSType        = 0;
   info.bV4GammaRed      = 0;
   info.bV4GammaGreen    = 0;
   info.bV4GammaBlue     = 0;

   // NB: wingdi.h sometimes defines bV4Compression as bV4V4Compression

   if (bpp IS 24) info.bV4V4Compression = BI_RGB; // Must use BI_RGB in 24bit mode, or GDI does nothing
   else info.bV4V4Compression = BI_BITFIELDS; // Must use BI_BITFIELDS and set the RGB masks in other packed modes

   if (info.bV4BitCount IS 15) info.bV4BitCount = 16;

   ystart = scanheight - (ystart + height);
   SetDIBitsToDevice(hdc, xdest, ydest, width, height, xstart, ystart, 0, scanheight, data, (BITMAPINFO *)&info, DIB_RGB_COLORS);
}

//********************************************************************************************************************

void winDeleteDC(HDC hdc)
{
   DeleteDC(hdc);
}

//********************************************************************************************************************

void winGetPixel(HDC hdc, LONG x, LONG y, UBYTE *rgb)
{
   COLORREF col;
   col = GetPixel(hdc, x, y);
   rgb[0] = GetRValue(col);
   rgb[1] = GetGValue(col);
   rgb[2] = GetBValue(col);
}

//********************************************************************************************************************

HBITMAP winCreateBitmap(LONG width, LONG height, LONG bpp)
{
   return CreateBitmap(width, height, 1, bpp, NULL);
}

//********************************************************************************************************************
// This masking technique works so long as the source graphic uses a clear background after determining its original
// mask shape.

void winDrawTransparentBitmap(HDC hdcDest, HDC hdcSrc, HBITMAP hBitmap,
        LONG x, LONG y, LONG xsrc, LONG ysrc, LONG width, LONG height,
        LONG maskx, LONG masky, HDC hdcMask)
{
   if ((!hdcMask) OR (!hdcDest) OR (!hdcSrc)) return;

   BitBlt(hdcDest, x, y, width, height, hdcMask, maskx, masky, SRCAND);   // Mask out the places where the bitmap will be placed.
   BitBlt(hdcDest, x, y, width, height, hdcSrc, xsrc, ysrc, SRCPAINT);    // XOR the bitmap with the background on the destination DC.
}

//********************************************************************************************************************
// Get a pointer to our interface

static HRESULT STDMETHODCALLTYPE RKDT_QueryInterface(struct rkDropTarget *Self, REFIID iid, void ** ppvObject)
{
   MSG("rkDropTarget::QueryInterface()\n");

	if (ppvObject IS NULL) return E_POINTER;

	// Supports IID_IUnknown and IID_IDropTarget
	if (IsEqualGUID(iid, &IID_IUnknown) OR IsEqualGUID(iid, &IID_IDropTarget)) {
		*ppvObject = (void *)Self;
		RKDT_AddRef(Self);
		return S_OK;
	}

	return E_NOINTERFACE;
}

//********************************************************************************************************************

static ULONG STDMETHODCALLTYPE RKDT_AddRef(struct rkDropTarget *Self)
{
   MSG("rkDropTarget::AddRef()\n");
   return InterlockedIncrement(&Self->lRefCount);
}

//********************************************************************************************************************

static ULONG STDMETHODCALLTYPE RKDT_Release(struct rkDropTarget *Self)
{
   MSG("rkDropTarget::Release()\n");

   LONG nCount;
   if ((nCount = InterlockedDecrement(&Self->lRefCount)) == 0) {
      if (Self->ItemData) {
         free(Self->ItemData);
         Self->ItemData = NULL;
      }

      if (Self->DataItems) {
         free(Self->DataItems);
         Self->DataItems = NULL;
      }
      if (glHeap) HeapFree(glHeap, 0, Self);
   }

	return nCount;
}

//********************************************************************************************************************
// The drag action continues

static HRESULT STDMETHODCALLTYPE RKDT_DragOver(struct rkDropTarget *Self, DWORD grfKeyState, POINTL pt, DWORD * pdwEffect)
{
   MSG("rkDropTarget::DragOver()\n");
	*pdwEffect = DROPEFFECT_COPY;
	return S_OK;
}

//********************************************************************************************************************
// The drag action leaves your window - no dropping

static HRESULT STDMETHODCALLTYPE RKDT_DragLeave()
{
   MSG("rkDropTarget::DragLeave()\n");
	return S_OK;
}

//********************************************************************************************************************
// The drag action enters your window - get the item

static HRESULT STDMETHODCALLTYPE RKDT_DragEnter(struct rkDropTarget *Self, IDataObject *Data, DWORD grfKeyState, POINTL pt, DWORD *pdwEffect)
{
   MSG("rkDropTarget::DragEnter %dx%d\n", (int)pt.x, (int)pt.y);

   //HWND window = WindowFromPoint(pt);

   // Use DROPEFFECT_NONE if the datatype isn't supported, otherwise use DROPEFFECT_COPY.

	*pdwEffect = DROPEFFECT_COPY;
	return S_OK;
}

//********************************************************************************************************************
// Convert the windows datatypes to Parasol datatypes.

static int STDMETHODCALLTYPE RKDT_AssessDatatype(struct rkDropTarget *Self, IDataObject *Data, char *Result, int Length)
{
   IEnumFORMATETC *eformat;
   FORMATETC fmt;
   int i;
   char dt;

   i = 0;
   if (Data->lpVtbl->EnumFormatEtc(Data, DATADIR_GET, &eformat) IS S_OK) {
      while (eformat->lpVtbl->Next(eformat, 1, &fmt, NULL) IS S_OK) {
         TCHAR szBuf[100];
         if (GetClipboardFormatName(fmt.cfFormat, szBuf, sizeof(szBuf))) {
            MSG("Format name: %d '%s'\n", fmt.cfFormat, szBuf);
         }
         else MSG("Format: %d\n", fmt.cfFormat);

         switch (fmt.cfFormat) {
            case CF_TEXT:
            case CF_UNICODETEXT:
            case CF_OEMTEXT:
               dt = DATA_TEXT; break;

            case CF_HDROP:
               dt = DATA_FILE; break;

            case CF_BITMAP:
            case CF_DIB:
            case CF_METAFILEPICT:
            case CF_TIFF:
               dt = DATA_IMAGE; break;

            case CF_RIFF:
            case CF_WAVE:
               dt = DATA_AUDIO; break;

            //case CF_HTML:
            //   dt = DATA_XML; break;

            default:
               dt = 0; break;
         }

         if (dt) Result[i++] = dt;
         if (i >= Length-1) break;
      }
      eformat->lpVtbl->Release(eformat);
   }

   Result[i] = 0;
   return i;
}

//********************************************************************************************************************
// The data have been dropped here, so process it

static HRESULT STDMETHODCALLTYPE RKDT_Drop(struct rkDropTarget *Self, IDataObject *Data, DWORD grfKeyState, POINT pt, DWORD * pdwEffect)
{
   HWND window;
   int surface_id, total;
   char datatypes[10];

   MSG("rkDropTarget::Drop(%dx%d)\n", (int)pt.x, (int)pt.y);

	*pdwEffect = DROPEFFECT_NONE;

   window = WindowFromPoint(pt);
   surface_id = winLookupSurfaceID(window);
   if (!surface_id) {
      MSG("rkDropTarget::Drop() Unable to determine surface ID from window, aborting.\n");
      return S_OK;
   }

   if (!(total = RKDT_AssessDatatype(Self, Data, datatypes, sizeof(datatypes)))) {
      MSG("rkDropTarget::Drop() Datatype not recognised or supported.\n");
      return S_OK;
   }

   // Calling winDragDropFromHost() will send an AC_DragDrop to the underlying surface.  If an object accepts the data,
   // it will send a DATA_REQUEST to the Display that represents the surface.  At this point we can copy the
   // clipboard from the host and send it to the client.  This entire process will occur within this call, so long as
   // all the calls are direct and the messaging system isn't used.  Otherwise the data will be lost as Windows
   // cannot be expected to hold onto the data after this method returns.

   Self->CurrentDataObject = Data;
   winDragDropFromHost_Drop(surface_id, datatypes);
   Self->CurrentDataObject = NULL;

	*pdwEffect = DROPEFFECT_COPY;

   return S_OK;
}

//********************************************************************************************************************

static int get_data(struct rkDropTarget *Self, char *Preference, struct WinDT **OutData, int *OutTotal)
{
	IDataObject *data;
   STGMEDIUM stgm;
	FORMATETC fmt = { CF_TEXT, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
   int p, error;

   MSG("rkDropTarget::GetData()\n");

   if ((!Preference) OR (!OutData) OR (!OutTotal)) return ERR_NullArgs;

   if (Self->ItemData) {
      free(Self->ItemData);
      Self->ItemData = NULL;
   }

   if (Self->DataItems) {
      free(Self->DataItems);
      Self->DataItems = NULL;
   }

   data = Self->CurrentDataObject;
   for (p=0; p < 4; p++) { // Up to 4 data preferences may be specified
      switch (Preference[p]) {
         case DATA_TEXT:
         case DATA_XML: {
            int chars, u8len, i, size;
            unsigned short value;
            unsigned char *u8str;
            unsigned short *wstr;

            fmt.cfFormat = CF_UNICODETEXT;
	         if (data->lpVtbl->GetData(data, &fmt, &stgm) IS S_OK) {
               if ((wstr = (unsigned short *)GlobalLock(stgm.hGlobal))) {
                  u8len = 0;
                  for (chars=0; wstr[chars]; chars++) {
                     if (wstr[chars] < 128) u8len++;
                     else if (wstr[chars] < 0x800) u8len += 2;
                     else u8len += 3;
                  }

                  if ((Self->ItemData = malloc(u8len+1))) {
                     u8str = (unsigned char *)Self->ItemData;
                     i = 0;
                     for (chars=0; wstr[chars]; chars++) {
                        value = wstr[chars];
                        if (value < 128) {
                           u8str[i++] = (unsigned char)value;
                        }
                        else if (value < 0x800) {
                           u8str[i+1] = (value & 0x3f) | 0x80;
                           value  = value>>6;
                           u8str[i] = value | 0xc0;
                           i += 2;
                        }
                        else {
                           u8str[i+2] = (value & 0x3f)|0x80;
                           value  = value>>6;
                           u8str[i+1] = (value & 0x3f)|0x80;
                           value  = value>>6;
                           u8str[i] = value | 0xe0;
                           i += 3;
                        }
                     }
                     u8str[i++] = 0;

                     if ((Self->DataItems = (struct WinDT *)malloc(sizeof(struct WinDT)))) {
                        Self->DataItems[0].Datatype = DATA_TEXT;
                        Self->DataItems[0].Length = i;
                        Self->DataItems[0].Data = Self->ItemData;
                        *OutData  = Self->DataItems;
                        *OutTotal = 1;
                        error = ERR_Okay;
                     }
                     else error = ERR_AllocMemory;
                  }
                  else error = ERR_AllocMemory;

                  GlobalUnlock(stgm.hGlobal);
               }
               else error = ERR_Lock;

               ReleaseStgMedium(&stgm);
               return error;
            }

            fmt.cfFormat = CF_TEXT;
	         if (data->lpVtbl->GetData(data, &fmt, &stgm) IS S_OK) {
               unsigned char *str;
               if ((str = (unsigned char *)GlobalLock(stgm.hGlobal))) {
                  size = GlobalSize(stgm.hGlobal);
                  if ((Self->ItemData = malloc(size))) {
                     memcpy(Self->ItemData, str, size);

                     if ((Self->DataItems = (struct WinDT *)malloc(sizeof(struct WinDT)))) {
                        Self->DataItems[0].Datatype = DATA_TEXT;
                        Self->DataItems[0].Length   = size;
                        Self->DataItems[0].Data     = Self->ItemData;
                        *OutData  = Self->DataItems;
                        *OutTotal = 1;
                        error = ERR_Okay;
                     }
                     else error = ERR_AllocMemory;
                  }
                  else error = ERR_AllocMemory;
                  GlobalUnlock(stgm.hGlobal);
               }
               else error = ERR_Lock;

               ReleaseStgMedium(&stgm);
               return error;
            }

            break;
         }

         case DATA_IMAGE: {
            int size;

            fmt.cfFormat = CF_TIFF;
	         if (data->lpVtbl->GetData(data, &fmt, &stgm) IS S_OK) {
               unsigned char *raw;
               if ((raw = (unsigned char *)GlobalLock(stgm.hGlobal))) {
                  size = GlobalSize(stgm.hGlobal);
                  if ((Self->ItemData = malloc(size))) {
                     memcpy(Self->ItemData, raw, size);

                     if ((Self->DataItems = (struct WinDT *)malloc(sizeof(struct WinDT)))) {
                        Self->DataItems[0].Datatype = DATA_IMAGE;
                        Self->DataItems[0].Length   = size;
                        Self->DataItems[0].Data     = Self->ItemData;
                        *OutData  = Self->DataItems;
                        *OutTotal = 1;
                        error = ERR_Okay;
                     }
                     else error = ERR_AllocMemory;
                  }
                  else error = ERR_AllocMemory;
                  GlobalUnlock(stgm.hGlobal);
               }
               else error = ERR_Lock;

               ReleaseStgMedium(&stgm);
               return error;
            }

            break;
         }

         case DATA_AUDIO: {
            int size;

            fmt.cfFormat = CF_RIFF;
	         if (data->lpVtbl->GetData(data, &fmt, &stgm) IS S_OK) {
               unsigned char *raw;
               if ((raw = (unsigned char *)GlobalLock(stgm.hGlobal))) {
                  size = GlobalSize(stgm.hGlobal);
                  if ((Self->ItemData = malloc(size))) {
                     memcpy(Self->ItemData, raw, size);

                     if ((Self->DataItems = (struct WinDT *)malloc(sizeof(struct WinDT)))) {
                        Self->DataItems[0].Datatype = DATA_AUDIO;
                        Self->DataItems[0].Length   = size;
                        Self->DataItems[0].Data     = Self->ItemData;
                        *OutData  = Self->DataItems;
                        *OutTotal = 1;
                        error = ERR_Okay;
                     }
                     else error = ERR_AllocMemory;
                  }
                  else error = ERR_AllocMemory;
                  GlobalUnlock(stgm.hGlobal);
               }
               else error = ERR_Lock;

               ReleaseStgMedium(&stgm);
               return error;
            }

            break;
         }

         case DATA_FILE: {
            HDROP raw;
            int i, total, item, len, size;
            TCHAR path[MAX_PATH];
            char *str;

            fmt.cfFormat = CF_HDROP;
	         if (data->lpVtbl->GetData(data, &fmt, &stgm) IS S_OK) {
               if ((raw = (HDROP)GlobalLock(stgm.hGlobal))) {
                  total = DragQueryFile(raw, 0xffffffff, NULL, 0);

                  size = 0;
                  for (i=0; i < total; i++) {
                     size += DragQueryFile(raw, i, path, sizeof(path)) + 1;
                  }

                  if ((Self->ItemData = malloc(size))) {
                     if ((Self->DataItems = (struct WinDT *)malloc(sizeof(struct WinDT) * total))) {
                        str = (char *)Self->ItemData;
                        for (item=0; item < total; item++) {
                           len = DragQueryFile(raw, item, str, MAX_PATH) + 1;

                           Self->DataItems[item].Datatype = DATA_FILE;
                           Self->DataItems[item].Length   = len;
                           Self->DataItems[item].Data     = str;
                           str += len;
                        }
                        *OutData  = Self->DataItems;
                        *OutTotal = total;
                        error = ERR_Okay;
                     }
                     else error = ERR_AllocMemory;
                  }
                  else error = ERR_AllocMemory;
                  GlobalUnlock(stgm.hGlobal);
               }
               else error = ERR_Lock;

               ReleaseStgMedium(&stgm);
               return error;
            }

            fmt.cfFormat = fmtShellIDList;
            if (data->lpVtbl->GetData(data, &fmt, &stgm) IS S_OK) {
               LPIDA pida;
               TCHAR path[MAX_PATH], folderpath[MAX_PATH];
               LPCITEMIDLIST item, folder;
               int pos, j, index, folderlen;

               MSG("ShellIDList discovered.\n");

               error = ERR_Okay;

               if ((pida = (LPIDA)GlobalLock(stgm.hGlobal))) {
                  folder = HIDA_GetPIDLFolder(pida);
                  if (SHGetPathFromIDList(folder, folderpath)) {
                     // Calculate the size of the file strings: (FolderLength * Total) + (Lengths of each filename)

                     for (folderlen=0; folderpath[folderlen]; folderlen++);
                     size = folderlen * pida->cidl;

                     // Add lengths of each file name

                     for (index=0; index < (int)pida->cidl; index++) {
                        item = HIDA_GetPIDLItem(pida, index);
                        if (SHGetPathFromIDList(item, path)) {
                           for (j=0; path[j]; j++);
                           while ((j > 0) AND (path[j-1] != '/') AND (path[j-1] != '\\')) j--;
                           for (i=0; path[j+i]; i++);
                           size += i + 1;
                        }
                        else {
                           error = ERR_Failed;
                           break;
                        }
                     }

                     if (!error) {
                        if ((Self->ItemData = malloc(size)) AND (Self->DataItems = (struct WinDT *)malloc(sizeof(struct WinDT) * pida->cidl))) {
                           char *str = (char *)Self->ItemData;
                           pos = 0;
                           for (index=0; index < (int)pida->cidl; index++) {
                              item = HIDA_GetPIDLItem(pida, index);
                              if (SHGetPathFromIDList(item, path)) {
                                 Self->DataItems[index].Datatype = DATA_FILE;
                                 Self->DataItems[index].Data     = str + pos;

                                 // Copy the root folder path first

                                 for (j=0; folderpath[j]; j++) str[pos++] = folderpath[j];

                                 // Go to the end of the string and then find the start of the filename

                                 for (j=0; path[j]; j++);
                                 while ((j > 0) AND (path[j-1] != '/') AND (path[j-1] != '\\')) j--;

                                 while (path[j]) str[pos++] = path[j++];
                                 str[pos++] = 0;

                                 Self->DataItems[index].Length = (int)(((char *)str + pos) - (char *)Self->DataItems[index].Data);
                              }
                              else {
                                 error = ERR_Failed;
                                 break;
                              }
                           }

                           if (!error) {
                              *OutData  = Self->DataItems;
                              *OutTotal = pida->cidl;
                           }
                        }
                        else error = ERR_AllocMemory;
                     }
                  }
                  else error = ERR_Failed;

                  GlobalUnlock(stgm.hGlobal);
               }
               else error = ERR_Lock;

               ReleaseStgMedium(&stgm);
               return error;
            }
            break;
         }
      }
   }

#if 0
   // Other means of data access
	WCHAR *strFileName;
   switch (stgMedium.tymed) {
      case TYMED_FILE:
         strFileName = stgMedium.lpszFileName;
         //...
         CoTaskMemFree(stgMedium.lpszFileName);
         break;

      case TYMED_ISTREAM:
         tb_pDragFile = new COleStreamFile(stgMedium.pstm);
         DWORD siz = tb_pDragFile->GetLength();
         LPTSTR buffer = Data.GetBufferSetLength((int)siz);
         tb_pDragFile->Read(buffer, (UINT)siz); // typesafe here because GetLength >=0
         Data.ReleaseBuffer();
         break;
   }
#endif

   return ERR_Failed;
}

//********************************************************************************************************************

static RK_IDROPTARGET *glDropTarget = NULL;

static LONG winInitDragDrop(HWND Window)
{
   MSG("Initialising drag and drop.\n");

   fmtShellIDList = RegisterClipboardFormat(CFSTR_SHELLIDLIST);

   glHeap = GetProcessHeap();

   if (!glDropTarget) {
      static RK_IDROPTARGET_VTBL idt_vtbl = {
         RKDT_QueryInterface,
         RKDT_AddRef,
         RKDT_Release,
         RKDT_DragEnter,
         RKDT_DragOver,
         RKDT_DragLeave,
         RKDT_Drop
      };

      if (!(glDropTarget = HeapAlloc(glHeap, 0, sizeof(RK_IDROPTARGET)))) return ERR_Failed;
      glDropTarget->idt.lpVtbl   = (void *)&idt_vtbl;
      glDropTarget->lRefCount    = 1;
      glDropTarget->tb_pDragFile = NULL;
   }

   if (RegisterDragDrop(Window, &glDropTarget->idt) != S_OK) {
      MSG("RegisterDragDrop() failed.\n");
   }

   return ERR_Okay;
}

//********************************************************************************************************************

int winGetData(char *Preference, struct WinDT **OutData, int *OutTotal)
{
   if ((!Preference) OR (!OutData) OR (!OutTotal)) return ERR_NullArgs;
   if (!glDropTarget) return ERR_Failed;
   return get_data(glDropTarget, Preference, OutData, OutTotal);
}

//********************************************************************************************************************

void winClearClipboard(void)
{
   if (OpenClipboard(NULL)) {
      EmptyClipboard();
      CloseClipboard();
   }
}

//********************************************************************************************************************
// Called from clipAddFile(), clipAddText() etc

int winAddClip(int Datatype, const void *Data, int Size, int Cut)
{
   UINT format;
   HGLOBAL hdata;
   char * pdata;

   MSG("winAddClip()\n");

   switch(Datatype) {
      case CLIP_DATA:   return ERR_NoSupport; break;
      case CLIP_AUDIO:  format = CF_WAVE; break;
      case CLIP_IMAGE:  format = CF_BITMAP; break;
      case CLIP_FILE:   format = CF_HDROP; Size += sizeof(DROPFILES); break;
      case CLIP_OBJECT: return ERR_NoSupport; break;
      case CLIP_TEXT:   format = CF_UNICODETEXT; break;
      default:
         return ERR_NoSupport;
   }

   if (OpenClipboard(NULL)) {
      int error;

      EmptyClipboard();

      if ((hdata = GlobalAlloc(GMEM_DDESHARE, Size))) {
         if ((pdata = (char *)GlobalLock(hdata))) {
            memcpy(pdata, Data, Size);
            GlobalUnlock(hdata);

            glIgnoreClip = GetTickCount();

            SetClipboardData(format, hdata);
            error = ERR_Okay;
         }
         else error = ERR_Lock;
      }
      else error = ERR_AllocMemory;

      CloseClipboard();
      return error;
   }
   else return ERR_Failed;

   return ERR_NoSupport;
}

//********************************************************************************************************************

void winGetClip(int Datatype)
{
   UINT format;

   switch (Datatype) {
      case CLIP_DATA:   return; break;
      case CLIP_AUDIO:  format = CF_WAVE; break;
      case CLIP_IMAGE:  format = CF_BITMAP; break;
      case CLIP_FILE:   format = CF_HDROP; break;
      case CLIP_OBJECT: return; break;
      case CLIP_TEXT:   format = CF_UNICODETEXT; break;
      default:
         return;
   }

   GetClipboardData(CF_UNICODETEXT);
}

//********************************************************************************************************************
// The clipboard ID increments every time that a new item appears on the Windows clipboard.

int winCurrentClipboardID(void)
{
   return glClipboardUpdates;
}

//********************************************************************************************************************

void winCopyClipboard(void)
{
   void *pdata;
   IDataObject *pDataObj;
   IEnumFORMATETC *pEnumFmt;
   FORMATETC fmt;

   if (!glOleInit) {
      MSG("OLE not initialised.\n");
      return;
   }

   MSG("winCopyClipboard()\n");

   glIgnoreClip = GetTickCount(); // Needed to avoid automated successive calls to this function.

   HRESULT result; // Other apps can block the clipboard, so we need to be able to reattempt access.
	for (int attempt=0; attempt < 8; attempt++) {
      result = OleGetClipboard(&pDataObj);
      if (result IS S_OK) break;
      Sleep(1);
	}

   if (result IS S_OK) {
      // Enumerate the formats supported by this clip.  It is assumed that the formats
      // that are encountered first have priority.

      if (pDataObj->lpVtbl->EnumFormatEtc(pDataObj, DATADIR_GET, &pEnumFmt) IS S_OK) {
         while (pEnumFmt->lpVtbl->Next(pEnumFmt, 1, &fmt, NULL) IS S_OK) {
            if (fmt.cfFormat IS CF_UNICODETEXT) {
               FORMATETC fmt = { CF_UNICODETEXT, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
               STGMEDIUM stgm;
               if (pDataObj->lpVtbl->GetData(pDataObj, &fmt, &stgm) IS S_OK) {
                  if ((pdata = GlobalLock(stgm.hGlobal))) {
                     report_windows_clip_utf16(pdata);
                     GlobalUnlock(stgm.hGlobal);
                  }
                  ReleaseStgMedium(&stgm);
               }
               break;
            }
            else if ((fmt.cfFormat IS CF_TEXT) OR (fmt.cfFormat IS CF_OEMTEXT) OR (fmt.cfFormat IS CF_DSPTEXT)) {
               FORMATETC fmt = { CF_TEXT, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
               STGMEDIUM stgm;
               if (pDataObj->lpVtbl->GetData(pDataObj, &fmt, &stgm) IS S_OK) {
                  if ((pdata = GlobalLock(stgm.hGlobal))) {
                     report_windows_clip_utf16(pdata);
                     GlobalUnlock(stgm.hGlobal);
                  }
                  ReleaseStgMedium(&stgm);
               }
               break;
            }
            else if (fmt.cfFormat IS CF_HDROP) {
               FORMATETC fmt = { CF_HDROP, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
               STGMEDIUM stgm, effect;
               LPIDA pida;
               DWORD *effect_data;
               char cut_operation = 0;

               if (pDataObj->lpVtbl->GetData(pDataObj, &fmt, &stgm) IS S_OK) {
                  FORMATETC fmt = { fmtPreferredDropEffect, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
                  if (pDataObj->lpVtbl->GetData(pDataObj, &fmt, &effect) IS S_OK) {
                     if ((effect_data = (DWORD *)GlobalLock(effect.hGlobal))) {
                        if (*effect_data IS DROPEFFECT_MOVE) cut_operation = 1;
                        GlobalUnlock(effect.hGlobal);
                     }
                     ReleaseStgMedium(&effect);
                  }

                  if ((pida = (LPIDA)GlobalLock(stgm.hGlobal))) {
                     report_windows_hdrop(pida, cut_operation);
                     GlobalUnlock(stgm.hGlobal);
                  }
               }
               break;
            }
            else if (fmt.cfFormat IS fmtShellIDList) {
               // List of files found
               FORMATETC fmt = { fmtShellIDList, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
               STGMEDIUM stgm, effect;
               LPIDA pida;
               DWORD *effect_data;
               char cut_operation = 0;

               if (pDataObj->lpVtbl->GetData(pDataObj, &fmt, &stgm) IS S_OK) {
                  FORMATETC fmt = { fmtPreferredDropEffect, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
                  if (pDataObj->lpVtbl->GetData(pDataObj, &fmt, &effect) IS S_OK) {
                     if ((effect_data = (DWORD *)GlobalLock(effect.hGlobal))) {
                        if (*effect_data IS DROPEFFECT_MOVE) cut_operation = 1;
                        GlobalUnlock(effect.hGlobal);
                     }
                     ReleaseStgMedium(&effect);
                  }

                  if ((pida = (LPIDA)GlobalLock(stgm.hGlobal))) {
                     report_windows_files(pida, cut_operation);
                     GlobalUnlock(stgm.hGlobal);
                  }
                  ReleaseStgMedium(&stgm);
                }
                break;
            }
         }
         pEnumFmt->lpVtbl->Release(pEnumFmt);
      }
      else MSG("EnumFormatEtc() failed.\n");

      pDataObj->lpVtbl->Release(pDataObj);
   }
   else MSG("OleGetClipboard() failed.\n");
}

//********************************************************************************************************************

int winExtractFile(LPIDA pida, int Index, char *Result, int Size)
{
   TCHAR path[MAX_PATH];

   if (Index >= (int)pida->cidl) return 0;

   LPCITEMIDLIST list = HIDA_GetPIDLFolder(pida);
   if (SHGetPathFromIDList(list, path)) {
      int pos, j;
      for (pos=0; (path[pos]) AND (pos < Size-1); pos++) Result[pos] = path[pos];
      if ((Result[pos-1] != '\\') AND (pos < Size-1)) Result[pos++] = '\\';

      list = HIDA_GetPIDLItem(pida, Index);
      if (SHGetPathFromIDList(list, path)) {
         for (j=0; path[j]; j++);
         while ((j > 0) AND (path[j-1] != '/') AND (path[j-1] != '\\')) j--;

         while ((path[j]) AND (pos < Size-1)) Result[pos++] = path[j++];
         Result[pos] = 0;

         return 1;
      }
      else return 0;
   }
   else return 0;
}

//********************************************************************************************************************

void winTerminate(void)
{
   if (glDropTarget) {
      RKDT_Release(glDropTarget);
      glDropTarget = NULL;
   }

   if (glScreenClassInit) {
      UnregisterClass("ScreenClass", GetModuleHandle(NULL));
      glScreenClassInit = 0;
   }

   if (glOleInit IS 1) {
      OleUninitialize();
      glOleInit = 0;
   }
}
