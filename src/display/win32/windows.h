#ifdef __cplusplus
extern "C" {
#endif

#ifdef PARASOL_MAIN_H
typedef void * HWND;
typedef void * HDC;
typedef void * HANDLE;
typedef void * HINSTANCE;
typedef void * WNDPROC;
typedef void * HCURSOR;
enum class PTC : LONG;
#endif

struct WinCursor {
   #ifdef _WIN32
   HCURSOR WinCursor;
   #else
   void * WinCursor;
   #endif
   #ifdef PARASOL_MAIN_H
   PTC CursorID;
   #else
   int CursorID;
   #endif
};

struct WinDT {
   int Datatype;
   int Length;
   void *Data;
};

void win32RedrawWindow(HWND, HDC, int X, int Y, int Width,
   int Height, int XDest, int YDest, int ScanWidth, int ScanHeight,
   int BPP, void *Data, int RedMask, int GreenMask, int BlueMask, int AlphaMask, int Opacity);
void MsgKeyPress(int, int, int);
void MsgKeyRelease(int, int);
void MsgMovement(int, double, double, int, int);
void MsgWheelMovement(int, float);
void MsgButtonPress(int, int);
void MsgFocusState(int SurfaceID, int State);
void MsgResizedWindow(int, int, int, int, int, int, int, int, int);
void MsgSetFocus(int SurfaceID);
void MsgSwitchWindowType(HWND, int);
void MsgTimer(void);
void MsgWindowClose(int SurfaceID);
void MsgWindowDestroyed(int SurfaceID);

void CheckWindowSize(int, int *, int *);

void Win32ManagerLoop(void);

void winGetDPI(LONG *, LONG *);
int winLookupSurfaceID(HWND);
HWND winCreateChild(HWND, int, int, int, int);
HWND winCreateScreen(HWND, int *, int *, int *, int *, char, char, const char *, char, unsigned char, char);
int winCreateScreenClass(void);
int winCurrentClipboardID(void);
void winDisableBatching(void);
void winRemoveWindowClass(const char *);
int winProcessMessage(void *);
int winDestroyWindow(HWND);
int winDetermineWindow(int, int, int *, int *, int *);
void winFindClose(HANDLE);
HANDLE winFindWindow(char *, char *);
void winFocus(HWND);
void winFreeDragDrop(void);
void winGetCoords(HWND, int *, int *, int *, int *, int *, int *, int *, int *);
int winGetDesktopSize(int *, int *);
int winGetDisplaySettings(int *, int *, int *);
void winGetMargins(HWND, int *, int *, int *, int *);
HINSTANCE winGetModuleHandle(void);
int winGetWindowInfo(HWND, int *, int *, int *, int *, int *);
void winGetWindowTitle(HWND, char *, int);
int winHideWindow(HWND);
void winInitCursors(struct WinCursor *, int);
void winMinimiseWindow(HWND);
int winMoveWindow(HWND, int, int);
void winMoveToBack(HWND);
void winMoveToFront(HWND);
int winReadKey(char *, char *, char *, int);
int winResizeWindow(HWND, int, int, int, int);
void winSetCursorPos(double X, double Y);
void winShowCursor(int);
void winSetCursor(HCURSOR);
void winSetSurfaceID(HWND, int);
int winSettings(int);
void winSetWindowTitle(HWND, const char *);
int winShowWindow(HWND, int);
void winUpdateWindow(HWND);
void winTerminate(void);
HDC winGetDC(HWND);
void winReleaseDC(HWND, HDC);
void winDragDropFromHost_Drop(int, char *);
int winGetData(char *, struct WinDT **, int *);

#ifdef __cplusplus
}
#endif
