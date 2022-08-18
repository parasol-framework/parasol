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
#endif

struct WinCursor {
   void * WinCursor;
   int CursorID;
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
HWND winCreateScreen(HWND, int *, int *, int *, int *, char, char, char *, char, unsigned char, char);
int winCreateScreenClass(void);
void winDisableBatching(void);
void winRemoveWindowClass(const char *);
int winProcessMessage(void *);
int winDestroyWindow(HWND);
int winDetermineWindow(int, int, int *, int *, int *);
void winFindClose(HANDLE);
HANDLE winFindWindow(char *, char *);
void winFocus(HWND);
void winFreeDragDrop(void);
void winGetCoords(HWND Window, int *WinX, int *WinY, int *WinWidth, int *WinHeight, int *ClientX, int *ClientY, int *ClientWidth, int *ClientHeight);
int winGetDesktopSize(int *, int *);
int winGetDisplaySettings(int *, int *, int *);
void winGetMargins(HWND, int *Left, int *Top, int *Right, int *Bottom);
HINSTANCE winGetModuleHandle(void);
int winGetWindowInfo(HWND, int *, int *, int *, int *, int *);
void winGetWindowTitle(HWND, char *, int);
int winHideWindow(HWND);
void winInitCursors(struct WinCursor *Cursor, int);
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

void winDragDropFromHost_Drop(int SurfaceID, char *Datatypes);
int winGetData(char *Preference, struct WinDT **OutData, int *OutTotal);

#ifdef __cplusplus
}
#endif
