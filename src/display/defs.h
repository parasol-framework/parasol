
#define __system__
//#define DEBUG
#define PRIVATE_DISPLAY
#define PRV_DISPLAY_MODULE
#define PRV_BITMAP
#define PRV_DISPLAY
#define PRV_POINTER
#define PRV_CLIPBOARD
#define PRV_SURFACE
//#define DBG_DRAW_ROUTINES // Use this if you want to debug any external code that is subscribed to surface drawing routines
//#define FASTACCESS
//#define DBG_LAYERS
#define FOCUSMSG(...) //LogF(NULL, __VA_ARGS__)

#ifdef DBG_LAYERS
#include <stdio.h>
#endif

#include <unordered_set>

#include <math.h>

#ifdef __linux__
 #include <dlfcn.h>
 #include <stdlib.h>
 #include <signal.h>
 #include <fcntl.h>
 #include <unistd.h>
 #include <sys/types.h>
 #include <sys/utsname.h>
 #include <sys/wait.h>
 #include <sys/mman.h>
 #include <errno.h>
#endif

#ifdef __xwindows__
 #include <X11/Xlib.h>
 #include <X11/Xos.h>
 #include <X11/keysym.h>
 #include <X11/XKBlib.h>
 #include <X11/keysymdef.h>
 #include <X11/Xproto.h>
 #include <X11/extensions/Xrandr.h> // Requires libxrandr-dev
 #include <X11/extensions/Xxf86dga.h> // Requires libxxf86dga-dev
 #include <X11/extensions/XShm.h>
 #include <X11/cursorfont.h>
 #include <stdlib.h>
 #include <X11/Xlib.h>
 #include <X11/Xos.h>
 #include <X11/Xutil.h>
 #include <sys/shm.h>
 #include <stdio.h>
 #include <parasol/modules/xrandr.h>
#endif

#ifdef _GLES_
#define GL_GLEXT_PROTOTYPES 1
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <EGL/eglplatform.h>
#include <GLES/gl.h>
#include <GLES/glext.h>
#endif

#ifdef __ANDROID__
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <android/configuration.h>
#endif

#define USE_XIMAGE         TRUE
#define SIZE_FOCUSLIST     30
#define DEFAULT_WHEELSPEED 500
#define TIME_DBLCLICK      40
#define REPEAT_BUTTONS     TRUE
#define MAX_CURSOR_WIDTH   32
#define MAX_CURSOR_HEIGHT  32
#define DRAG_XOFFSET       10
#define DRAG_YOFFSET       12

#define BF_DATA     0x01
#define BF_WINVIDEO 0x02

#define BLEND_MAX_THRESHOLD 255
#define BLEND_MIN_THRESHOLD 1

#ifndef PI
#define PI (3.141592653589793238462643383279f)
#endif

#define SURFACE_READ      (0x0001)   // Read access
#define SURFACE_WRITE     (0x0002)   // Write access
#define SURFACE_READWRITE (SURFACE_READ|SURFACE_WRITE)

#define MAX_CLIPS 10     // Maximum number of clips stored in the historical buffer

#define MAX_INPUTMSG 2048 // Must be a value to the power of two
#define INPUT_MASK        (MAX_INPUTMSG-1) // All bits will be set if MAX_INPUTMSG is a power of two

#include <parasol/modules/display.h>
#include <parasol/modules/xml.h>
#include <parasol/linear_rgb.h>

#define URF_REDRAWS_CHILDREN     0x00000001

#define UpdateSurfaceField(a,b) { \
   SurfaceList *list; SurfaceControl *ctl; WORD i; \
   if (Self->initialised()) { \
   if ((ctl = gfxAccessList(ARF_UPDATE))) { \
      list = (SurfaceList *)((BYTE *)ctl + ctl->ArrayIndex); \
      for (i=0; i < ctl->Total; i++) { \
         if (list[i].SurfaceID IS (a)->UID) { \
            list[i].b = (a)->b; \
            break; \
         } \
      } \
      gfxReleaseList(ARF_UPDATE); \
   } \
   } \
}

#define UpdateSurfaceField2(a,b,c) { \
   SurfaceList *list; SurfaceControl *ctl; WORD i; \
   if (Self->initialised()) { \
      if ((ctl = gfxAccessList(ARF_UPDATE))) { \
         list = (SurfaceList *)((BYTE *)ctl + ctl->ArrayIndex); \
         for (i=0; i < ctl->Total; i++) { \
            if (list[i].SurfaceID IS (a)->UID) { \
               list[i].b = (a)->c; \
               break; \
            } \
         } \
         gfxReleaseList(ARF_UPDATE); \
      } \
   } \
}

#define UpdateSurfaceList(a) update_surface_copy((a), 0)

struct dcDisplayInputReady { // This is an internal structure used by the display module to replace dcInputReady
   LARGE NextIndex;    // Next message index for the subscriber to look at
   LONG  SubIndex;     // Index into the InputSubscription list
};

class WindowHook {
public:
   OBJECTID SurfaceID;
   UBYTE Event;

   WindowHook(OBJECTID aSurfaceID, UBYTE aEvent) : SurfaceID(aSurfaceID), Event(aEvent) { };

   bool operator== (const WindowHook &rhs) const {
      return (SurfaceID == rhs.SurfaceID) and (Event == rhs.Event);
   }

   bool operator() (const WindowHook &lhs, const WindowHook &rhs) const {
       if (lhs.SurfaceID == rhs.SurfaceID) return lhs.Event < rhs.Event;
       else return lhs.SurfaceID < rhs.SurfaceID;
   }
};

namespace std {
   template <> struct hash<WindowHook> {
      std::size_t operator()(const WindowHook &k) const {
         return ((std::hash<OBJECTID>()(k.SurfaceID)
            ^ (std::hash<UBYTE>()(k.Event) << 1)) >> 1);
      }
   };
}

enum {
   STAGE_PRECOPY=1,
   STAGE_AFTERCOPY,
   STAGE_COMPOSITE
};

#define MT_PtrSetWinCursor -1
#define MT_PtrGrabX11Pointer -2
#define MT_PtrUngrabX11Pointer -3

struct ptrSetWinCursor { LONG Cursor;  };
struct ptrGrabX11Pointer { OBJECTID SurfaceID;  };

INLINE ERROR ptrSetWinCursor(OBJECTPTR Ob, LONG Cursor) {
   struct ptrSetWinCursor args = { Cursor };
   return Action(MT_PtrSetWinCursor, Ob, &args);
}

#define ptrUngrabX11Pointer(obj) Action(MT_PtrUngrabX11Pointer,(obj),0)

INLINE ERROR ptrGrabX11Pointer(OBJECTPTR Ob, OBJECTID SurfaceID) {
   struct ptrGrabX11Pointer args = { SurfaceID };
   return Action(MT_PtrGrabX11Pointer, Ob, &args);
}

#include "idl.h"

#ifdef __ANDROID__
#include <parasol/modules/android.h>
#endif

#undef NULL
#define NULL 0

struct resolution {
   WORD width;
   WORD height;
   WORD bpp;
};

// glInputEvents is allocated in shared memory for all processes consuming input events.

struct InputEventMgr {
   ULONG  IndexCounter;   // Counter for message ID's
   InputEvent Msgs[MAX_INPUTMSG];
};

extern InputEventMgr *glInputEvents;

// InputSubscription is allocated as an array of items for glSharedControl->InputMID

struct InputSubscription {
   LONG     Handle;        // Identifier needed for removing the subscription.
   LONG     ProcessID;     // The process to be woken when an input event occurs.
   OBJECTID SurfaceFilter; // Optional.  Wake the process only if the event occurs within this surface.
   WORD     InputMask;     // Process events that match this filter only.
   ULONG    LastAlerted;   // The IndexCounter value when this subscription was last alerted.
};

// Each input event subscription is registered as an InputCallback

class InputCallback {
public:
   OBJECTID SurfaceFilter;
   WORD     InputMask; // JTYPE flags
   FUNCTION Callback;

   bool operator==(const InputCallback &other) const {
      return (SurfaceFilter == other.SurfaceFilter);
   }
};

namespace std {
   template <>
   struct hash<InputCallback>
   {
      std::size_t operator()(const InputCallback& k) const {
         return (k.SurfaceFilter);
      }
   };
}

struct ClipHeader {
   LONG Counter;
#ifdef _WIN32
   LONG LastID;
   UBYTE Init:1;
#endif
};

struct ClipEntry {
   LONG     Datatype;    // The type of data clipped
   LONG     Flags;       // CEF_DELETE may be set for the 'cut' operation
   CLASSID  ClassID;     // Class ID that is capable of managing the clip data, if it originated from an object
   MEMORYID Files;       // List of file locations, separated with semi-colons, referencing all the data in this clip entry
   LONG     FilesLen;    // Complete byte-length of the Files string
   UWORD    ID;          // Unique identifier for the clipboard entry
   WORD     TotalItems;  // Total number of items in the clip-set
};

class extPointer : public objPointer {
   public:
   struct {
      LARGE LastClickTime;      // Timestamp
      OBJECTID LastClicked;     // Most recently clicked object
      UBYTE DblClick:1;         // TRUE if last click was a double-click
   } Buttons[10];
   LARGE    ClickTime;
   LARGE    AnchorTime;
   DOUBLE   LastClickX, LastClickY;
   DOUBLE   LastReleaseX, LastReleaseY;
   APTR     UserLoginHandle;
   OBJECTID LastSurfaceID;      // Last object that the pointer was positioned over
   OBJECTID CursorReleaseID;
   OBJECTID DragSurface;        // Draggable surface anchored to the pointer position
   OBJECTID DragParent;         // Parent of the draggable surface
   MEMORYID MessageQueue;       // Message port of the task that holds the cursor
   MEMORYID AnchorMsgQueue;     // Message port of the task that holds the cursor anchor
   LONG     CursorRelease;
   LONG     BufferCursor;
   LONG     BufferFlags;
   MEMORYID BufferQueue;
   OBJECTID BufferOwner;
   OBJECTID BufferObject;
   char     DragData[8];          // Data preferences for current drag & drop item
   char     Device[32];
   char     ButtonOrder[12];      // The order of the first 11 buttons can be changed here
   WORD     ButtonOrderFlags[12]; // Button order represented as JD flags
   BYTE     PostComposite;        // Enable post-composite drawing (default)
   UBYTE    prvOverCursorID;
   struct {
      WORD HotX;
      WORD HotY;
   } Cursors[PTR_END];
};

class extSurface : public objSurface {
   public:
   LARGE    LastRedimension;      // Timestamp of the last redimension call
   objBitmap *Bitmap;
   struct SurfaceCallback *Callback;
   APTR      UserLoginHandle;
   APTR      TaskRemovedHandle;
   WINHANDLE DisplayWindow;       // Reference to the platform dependent window representing the Surface object
   OBJECTID PrevModalID;          // Previous surface to have been modal
   OBJECTID BitmapOwnerID;        // The surface object that owns the root bitmap
   OBJECTID RevertFocusID;
   LONG     LineWidth;            // Bitmap line width, in bytes
   LONG     ScrollToX, ScrollToY;
   LONG     ScrollFromX, ScrollFromY;
   LONG     ListIndex;            // Last known list index
   LONG     InputHandle;          // Input handler for dragging of surfaces
   TIMER    RedrawTimer;          // For ScheduleRedraw()
   TIMER    ScrollTimer;
   MEMORYID DataMID;              // Bitmap memory reference
   MEMORYID PrecopyMID;           // Precopy region information
   struct SurfaceCallback CallbackCache[4];
   WORD     ScrollProgress;
   WORD     Opacity;
   UWORD    InheritedRoot:1;      // TRUE if the user set the RootLayer manually
   UWORD    ParentDefined:1;      // TRUE if the parent field was set manually
   UWORD    SkipPopOver:1;
   UWORD    FixedX:1;
   UWORD    FixedY:1;
   UWORD    Document:1;
   UWORD    RedrawScheduled:1;
   UWORD    RedrawCountdown;      // Unsubscribe from the timer when this value reaches zero.
   BYTE     BitsPerPixel;         // Bitmap bits per pixel
   BYTE     BytesPerPixel;        // Bitmap bytes per pixel
   UBYTE    CallbackCount;
   UBYTE    CallbackSize;         // Current size of the callback array.
   BYTE     WindowType;           // See SWIN constants
   BYTE     PrecopyTotal;
   BYTE     Anchored;
};

class extDisplay : public objDisplay {
   public:
   DOUBLE Gamma[3];          // Red, green, blue gamma radioactivity indicator
   struct resolution *Resolutions;
   FUNCTION  ResizeFeedback;
   MEMORYID  ResolutionsMID;
   WORD      TotalResolutions;
   OBJECTID  BitmapID;
   LONG      BmpXOffset;     // X offset for scrolling
   LONG      BmpYOffset;     // Y offset for scrolling
   #ifdef __xwindows__
   union {
      APTR   WindowHandle;
      Window XWindowHandle;
   };
   #elif __ANDROID__
      ANativeWindow *WindowHandle;
   #else
      APTR   WindowHandle;
   #endif
   APTR  UserLoginHandle;
   WORD  Opacity;
   LONG  VDensity;          // Cached DPI value, if calculable.
   LONG  HDensity;
   char  DriverVendor[60];
   char  DriverCopyright[80];
   char  Manufacturer[60];
   char  Chipset[40];
   char  DAC[32];
   char  Clock[32];
   char  DriverVersion[16];
   char  CertificationDate[20];
   char  Display[32];
   char  DisplayManufacturer[60];
   #ifdef _WIN32
      APTR OldProcedure;
   #endif
};

class extBitmap : public objBitmap {
   public:
   ULONG  *Gradients;
   APTR   ResolutionChangeHandle;
   struct RGBPalette prvPaletteArray;
   struct ColourFormat prvColourFormat;
   MEMORYID prvCompressMID;
   LONG   prvAFlags;                  // Private allocation flags
   #ifdef __xwindows__
      struct {
         XImage   ximage;
         Drawable drawable;
         XImage   *readable;
         XShmSegmentInfo ShmInfo;
         BYTE XShmImage;
      } x11;
   #elif _WIN32
      struct {
         APTR Drawable;  // HDC for the Bitmap
      } win;
   #elif _GLES_
      ULONG prvWriteBackBuffer:1;  // For OpenGL surface locking.
      LONG prvGLPixel;
      LONG prvGLFormat;
   #endif
};

extern ERROR create_bitmap_class(void);
extern ERROR create_clipboard_class(void);
extern ERROR create_display_class(void);
extern ERROR create_pointer_class(void);
extern ERROR create_surface_class(void);
extern ERROR get_surface_abs(OBJECTID, LONG *, LONG *, LONG *, LONG *);
extern void  input_event_loop(HOSTHANDLE, APTR);
extern ERROR lock_surface(extBitmap *, WORD);
extern ERROR unlock_surface(extBitmap *);
extern ERROR get_display_info(OBJECTID, DISPLAYINFO *, LONG);
extern void  resize_feedback(FUNCTION *, OBJECTID, LONG X, LONG Y, LONG Width, LONG Height);
extern void  forbidDrawing(void);
extern void  forbidExpose(void);
extern void  permitDrawing(void);
extern void  permitExpose(void);
extern ERROR apply_style(OBJECTPTR, OBJECTPTR, CSTRING);
extern ERROR load_styles(void);
extern WORD  find_bitmap_owner(SurfaceList *, WORD);
extern void  move_layer(extSurface *, LONG, LONG);
extern void  move_layer_pos(SurfaceControl *, LONG, LONG);
extern void  prepare_background(extSurface *, SurfaceList *, WORD, WORD, extBitmap *, ClipRectangle *, BYTE);
extern void  process_surface_callbacks(extSurface *, extBitmap *);
extern void  refresh_pointer(extSurface *Self);
extern ERROR track_layer(extSurface *);
extern void  untrack_layer(OBJECTID);
extern BYTE  restrict_region_to_parents(SurfaceList *, LONG, ClipRectangle *, BYTE);
extern ERROR load_style_values(void);
extern ERROR resize_layer(extSurface *, LONG X, LONG Y, LONG, LONG, LONG, LONG, LONG BPP, DOUBLE, LONG);
extern void  redraw_nonintersect(OBJECTID, SurfaceList *, WORD, WORD, ClipRectangle *, ClipRectangle *, LONG, LONG);
extern ERROR _expose_surface(OBJECTID, SurfaceList *, WORD, WORD, LONG, LONG, LONG, LONG, LONG);
extern ERROR _redraw_surface(OBJECTID, SurfaceList *, WORD, WORD, LONG, LONG, LONG, LONG, LONG);
extern void  _redraw_surface_do(extSurface *, SurfaceList *, WORD, WORD, LONG, LONG, LONG, LONG, extBitmap *, LONG);
extern void  check_styles(STRING Path, OBJECTPTR *Script) __attribute__((unused));
extern ERROR update_surface_copy(extSurface *, SurfaceList *);
extern LONG  find_surface_list(SurfaceList *, LONG, OBJECTID);
extern LONG  find_parent_list(SurfaceList *, WORD, extSurface *);

extern ERROR gfxRedrawSurface(OBJECTID, LONG, LONG, LONG, LONG, LONG);

#ifdef DBG_LAYERS
extern void print_layer_list(STRING Function, SurfaceControl *Ctl, LONG POI)
#endif

extern std::unordered_map<LONG, InputCallback> glInputCallbacks;
extern SharedControl *glSharedControl;
extern bool glSixBitDisplay;
extern OBJECTPTR glModule;
extern OBJECTPTR clDisplay, clPointer, clBitmap, clClipboard, clSurface;
extern OBJECTID glPointerID;
extern DISPLAYINFO *glDisplayInfo;
extern APTR glDither;
extern OBJECTPTR glCompress;
extern struct CoreBase *CoreBase;
extern ColourFormat glColourFormat;
extern bool glHeadless;
extern FieldDef CursorLookup[];
extern UBYTE *glAlphaLookup;
extern TIMER glRefreshPointerTimer;
extern extBitmap *glComposite;
extern DOUBLE glpRefreshRate, glpGammaRed, glpGammaGreen, glpGammaBlue;
extern LONG glpDisplayWidth, glpDisplayHeight, glpDisplayX, glpDisplayY;
extern LONG glpDisplayDepth; // If zero, the display depth will be based on the hosted desktop's bit depth.
extern LONG glpMaximise, glpFullScreen;
extern LONG glpWindowType;
extern char glpDPMS[20];
extern std::unordered_map<WindowHook, FUNCTION> glWindowHooks;

// Thread-specific variables.

extern THREADVAR APTR glSurfaceMutex;
extern THREADVAR WORD tlNoDrawing, tlNoExpose, tlVolatileIndex;
extern THREADVAR UBYTE tlListCount; // For drwAccesslist()
extern THREADVAR OBJECTID tlFreeExpose;
extern THREADVAR SurfaceControl *tlSurfaceList;
extern THREADVAR LONG glRecentSurfaceIndex;

struct InputType {
   LONG Flags;  // As many flags as necessary to describe the input type
   LONG Mask;   // Limited flags to declare the mask that must be used to receive that type
};

extern const std::array<struct InputType, JET_END> glInputType;
extern const std::array<std::string, JET_END> glInputNames;

#define find_surface_index(a,b) find_surface_list( (SurfaceList *)((BYTE *)(a) + (a)->ArrayIndex), (a)->Total, (b))
#define find_own_index(a,b)     find_surface_list( (SurfaceList *)((BYTE *)(a) + (a)->ArrayIndex), (a)->Total, (b)->UID)
#define find_parent_index(a,b)  find_parent_list( (SurfaceList *)((BYTE *)(a) + (a)->ArrayIndex), (a)->Total, (b))

//****************************************************************************

#ifdef _GLES_ // OpenGL related prototypes
GLenum alloc_texture(LONG Width, LONG Height, GLuint *TextureID);
void refresh_display_from_egl(objDisplay *Self);
ERROR init_egl(void);
void free_egl(void);
#endif

#ifdef _WIN32

#define DLLCALL // __declspec(dllimport)
#define WINAPI  __stdcall

extern BYTE glTrayIcon, glTaskBar, glStickToFront;

extern "C" {
DLLCALL LONG WINAPI SetPixelV(APTR, LONG, LONG, LONG);
DLLCALL LONG WINAPI SetPixel(APTR, LONG, LONG, LONG);
DLLCALL LONG WINAPI GetPixel(APTR, LONG, LONG);

int winAddClip(int Datatype, void * Data, int Size, int Cut);
void winClearClipboard(void);
void winCopyClipboard(void);
int winExtractFile(void *pida, int Index, char *Result, int Size);
void winGetClip(int Datatype);
void winTerminate(void);
APTR winGetDC(APTR);
void winReleaseDC(APTR, APTR);
void winSetSurfaceID(APTR, LONG);
APTR GetWinCursor(LONG);
LONG winBlit(APTR, LONG, LONG, LONG, LONG, APTR, LONG, LONG);
void winGetError(LONG, STRING, LONG);
APTR winCreateCompatibleDC(void);
APTR winCreateBitmap(LONG, LONG, LONG);
void winDeleteDC(APTR);
void winDeleteObject(void *);
void winDrawLine(APTR, LONG, LONG, LONG, LONG, UBYTE *);
void winDrawRectangle(APTR, LONG, LONG, LONG, LONG, UBYTE, UBYTE, UBYTE);
void winGetPixel(APTR, LONG, LONG, UBYTE *);
LONG winGetPixelFormat(LONG *, LONG *, LONG *, LONG *);
APTR winSelectObject(APTR, APTR);
APTR winSetClipping(APTR, LONG, LONG, LONG, LONG);
void winSetDIBitsToDevice(APTR, LONG, LONG, LONG, LONG, LONG, LONG, LONG, LONG, LONG, APTR, LONG, LONG, LONG);
}

#include "win32/windows.h"

extern WinCursor winCursors[24];

#endif // _WIN32

#ifdef __xwindows__

extern void X11ManagerLoop(HOSTHANDLE, APTR);
extern void handle_button_press(XEvent *);
extern void handle_button_release(XEvent *);
extern void handle_configure_notify(XConfigureEvent *);
extern void handle_enter_notify(XCrossingEvent *);
extern void handle_exposure(XExposeEvent *);
extern void handle_key_press(XEvent *);
extern void handle_key_release(XEvent *);
extern void handle_motion_notify(XMotionEvent *);
extern void handle_stack_change(XCirculateEvent *);
extern LONG x11WindowManager(void);
extern void init_xcursors(void);
extern void free_xcursors(void);

extern WORD glDGAAvailable;
extern APTR glDGAMemory;
extern X11Globals *glX11;
extern _XDisplay *XDisplay;
extern struct XRandRBase *XRandRBase;
extern UBYTE glX11ShmImage;
extern UBYTE KeyHeld[K_LIST_END];
extern UBYTE glTrayIcon, glTaskBar, glStickToFront;
extern LONG glKeyFlags, glXFD, glDGAPixelsPerLine, glDGABankSize;
extern Atom atomSurfaceID, XWADeleteWindow;
extern GC glXGC, glClipXGC;
extern XWindowAttributes glRootWindow;
extern Window glDisplayWindow;
extern Cursor C_Default;
extern OBJECTPTR modXRR;
extern WORD glPlugin;
extern APTR glDGAVideo;

#endif

#include "prototypes.h"

INLINE void clip_rectangle(ClipRectangle *rect, ClipRectangle *clip)
{
   if (rect->Left   < clip->Left)   rect->Left   = clip->Left;
   if (rect->Top    < clip->Top)    rect->Top    = clip->Top;
   if (rect->Right  > clip->Right)  rect->Right  = clip->Right;
   if (rect->Bottom > clip->Bottom) rect->Bottom = clip->Bottom;
}
