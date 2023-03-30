
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
#include <mutex>
#include <queue>
#include <sstream>
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
 #include <X11/extensions/XShm.h>
 #include <X11/cursorfont.h>
 #include <stdlib.h>
 #include <X11/Xlib.h>
 #include <X11/Xos.h>
 #include <X11/Xutil.h>
 #include <sys/shm.h>
 #include <stdio.h>

 #ifdef XDGA_ENABLED
  #include <X11/extensions/Xxf86dga.h> // Requires libxxf86dga-dev
 #endif

 #ifdef XRANDR_ENABLED
  #include <X11/extensions/Xrandr.h> // Requires libxrandr-dev
 #endif

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

#include <parasol/modules/display.h>
#include <parasol/modules/xml.h>
#include <parasol/linear_rgb.h>

#define URF_REDRAWS_CHILDREN     0x00000001

#define UpdateSurfaceRecord(a) update_surface_copy(a)

struct SurfaceRecord {
   APTR     Data;          // For drwCopySurface()
   OBJECTID ParentID;      // Object that owns the surface area
   OBJECTID SurfaceID;     // ID of the surface area
   OBJECTID BitmapID;      // Shared bitmap buffer, if available
   OBJECTID DisplayID;     // Display
   OBJECTID RootID;        // RootLayer
   OBJECTID PopOverID;
   LONG     Flags;         // Surface flags (RNF_VISIBLE etc)
   LONG     X;             // Horizontal coordinate
   LONG     Y;             // Vertical coordinate
   LONG     Width;         // Width
   LONG     Height;        // Height
   LONG     Left;          // Absolute X
   LONG     Top;           // Absolute Y
   LONG     Right;         // Absolute right coordinate
   LONG     Bottom;        // Absolute bottom coordinate
   WORD     Level;         // Level number within the hierarchy
   WORD     LineWidth;     // [applies to the bitmap owner]
   BYTE     BytesPerPixel; // [applies to the bitmap owner]
   BYTE     BitsPerPixel;  // [applies to the bitmap owner]
   BYTE     Cursor;        // Preferred cursor image ID
   UBYTE    Opacity;       // Current opacity setting 0 - 255

   inline void setArea(LONG pLeft, LONG pTop, LONG pRight, LONG pBottom) {
      Left   = pLeft;
      Top    = pTop;
      Right  = pRight;
      Bottom = pBottom;
   }

   inline ClipRectangle area() const {
      return std::move(ClipRectangle(Left, Top, Right, Bottom));
   }

   inline bool hasFocus() const { return Flags & RNF_HAS_FOCUS; }
   inline void dropFocus() { Flags &= RNF_HAS_FOCUS; }
};

typedef std::vector<SurfaceRecord> SURFACELIST;
extern std::recursive_mutex glSurfaceLock;

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

struct resolution {
   WORD width;
   WORD height;
   WORD bpp;
};

// Double-buffered input event system; helps to maintain pointer stability when messages are incoming.

class EventBuffer {
   public:
   std::vector<InputEvent> *primary = &buffer_a;
   bool processing = false;

   // Change the primary pointer for new incoming messages.  Return the current stack of messages for
   // processing.

   std::vector<InputEvent> & retarget() {
      if (primary IS &buffer_a) {
         primary = &buffer_b;
         primary->clear();
         return buffer_a;
      }
      else {
         primary = &buffer_a;
         primary->clear();
         return buffer_b;
      }
   }

   inline bool empty() {
      return primary->empty();
   }

   inline void push_back(InputEvent &Event) {
      primary->push_back(Event);
   }

   private:
   std::vector<InputEvent> buffer_a;
   std::vector<InputEvent> buffer_b;
};

extern EventBuffer glInputEvents;

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

//********************************************************************************************************************

struct ClipItem {
   std::string Path; // Path to a file containing the data.
   std::vector<char> Data; // Vector containing raw data.

   ClipItem(std::string pPath) : Path(pPath) { }
};

struct ClipRecord {
   LONG  Datatype;    // The type of data clipped
   LONG  Flags;       // CEF_DELETE may be set for the 'cut' operation
   std::vector<ClipItem> Items;  // List of file locations referencing all the data in this clip entry

   ~ClipRecord();
   ClipRecord(LONG pDatatype, LONG pFlags, const std::vector<ClipItem> pItems) :
      Datatype(pDatatype), Flags(pFlags), Items(pItems) { }
};

//********************************************************************************************************************

extern std::vector<SurfaceRecord> glSurfaces;

//********************************************************************************************************************

class extPointer : public objPointer {
   public:
   using create = pf::Create<extPointer>;

   struct {
      LARGE LastClickTime;      // Timestamp
      OBJECTID LastClicked;     // Most recently clicked object
      UBYTE DblClick:1;         // TRUE if last click was a double-click
   } Buttons[10];
   LARGE    ClickTime;
   LARGE    AnchorTime;
   DOUBLE   LastClickX, LastClickY;
   DOUBLE   LastReleaseX, LastReleaseY;
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
   using create = pf::Create<extSurface>;

   LARGE    LastRedimension;      // Timestamp of the last redimension call
   objBitmap *Bitmap;
   SurfaceCallback *Callback;
   APTR      Data;
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
   SurfaceCallback CallbackCache[4];
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
   BYTE     Anchored;
};

class extDisplay : public objDisplay {
   public:
   using create = pf::Create<extDisplay>;

   DOUBLE Gamma[3];          // Red, green, blue gamma radioactivity indicator
   std::vector<struct resolution> Resolutions;
   FUNCTION  ResizeFeedback;
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
   using create = pf::Create<extBitmap>;

   ULONG  *Gradients;
   APTR   ResolutionChangeHandle;
   RGBPalette prvPaletteArray;
   struct ColourFormat prvColourFormat;
   UBYTE *prvCompress;
   LONG   prvAFlags;                  // Private allocation flags
   #ifdef __xwindows__
      struct {
         XImage   ximage;
         Drawable drawable;
         XImage   *readable;
         XShmSegmentInfo ShmInfo;
         bool XShmImage;
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
extern LONG  find_bitmap_owner(const SURFACELIST &, LONG);
extern void  move_layer(extSurface *, LONG, LONG);
extern void  move_layer_pos(SURFACELIST &, LONG, LONG);
extern void  prepare_background(extSurface *, const SURFACELIST &, LONG, extBitmap *, const ClipRectangle &, BYTE);
extern void  process_surface_callbacks(extSurface *, extBitmap *);
extern void  refresh_pointer(extSurface *Self);
extern ERROR track_layer(extSurface *);
extern void  untrack_layer(OBJECTID);
extern BYTE  restrict_region_to_parents(const SURFACELIST &, LONG, ClipRectangle &, bool);
extern ERROR load_style_values(void);
extern ERROR resize_layer(extSurface *, LONG X, LONG Y, LONG, LONG, LONG, LONG, LONG BPP, DOUBLE, LONG);
extern void  redraw_nonintersect(OBJECTID, const SURFACELIST &, LONG, const ClipRectangle &, const ClipRectangle &, LONG, LONG);
extern ERROR _expose_surface(OBJECTID, const SURFACELIST &, LONG, LONG, LONG, LONG, LONG, LONG);
extern ERROR _redraw_surface(OBJECTID, const SURFACELIST &, LONG, LONG, LONG, LONG, LONG, LONG);
extern void  _redraw_surface_do(extSurface *, const SURFACELIST &, LONG, ClipRectangle &, extBitmap *, LONG);
extern void  check_styles(STRING Path, OBJECTPTR *Script) __attribute__((unused));
extern ERROR update_surface_copy(extSurface *);

extern ERROR gfxRedrawSurface(OBJECTID, LONG, LONG, LONG, LONG, LONG);

#ifdef DBG_LAYERS
extern void print_layer_list(STRING Function, SurfaceControl *Ctl, LONG POI)
#endif

extern SharedControl *glSharedControl;
extern bool glSixBitDisplay;
extern OBJECTPTR glModule;
extern OBJECTPTR clDisplay, clPointer, clBitmap, clClipboard, clSurface;
extern OBJECTID glPointerID;
extern DISPLAYINFO glDisplayInfo;
extern APTR glDither;
extern OBJECTPTR glCompress;
extern struct CoreBase *CoreBase;
extern ColourFormat glColourFormat;
extern bool glHeadless;
extern FieldDef CursorLookup[];
extern TIMER glRefreshPointerTimer;
extern extBitmap *glComposite;
extern DOUBLE glpRefreshRate, glpGammaRed, glpGammaGreen, glpGammaBlue;
extern LONG glpDisplayWidth, glpDisplayHeight, glpDisplayX, glpDisplayY;
extern LONG glpDisplayDepth; // If zero, the display depth will be based on the hosted desktop's bit depth.
extern LONG glpMaximise, glpFullScreen;
extern LONG glpWindowType;
extern char glpDPMS[20];
extern UBYTE *glDemultiply;
extern std::array<UBYTE, 256 * 256> glAlphaLookup;
extern std::list<ClipRecord> glClips;

extern std::unordered_map<WindowHook, FUNCTION> glWindowHooks;
extern std::vector<OBJECTID> glFocusList;
extern std::mutex glFocusLock;
extern std::recursive_mutex glSurfaceLock;
extern std::recursive_mutex glInputLock;

// Thread-specific variables.

extern THREADVAR WORD tlNoDrawing, tlNoExpose, tlVolatileIndex;
extern THREADVAR OBJECTID tlFreeExpose;

struct InputType {
   LONG Flags;  // As many flags as necessary to describe the input type
   LONG Mask;   // Limited flags to declare the mask that must be used to receive that type
};

extern const InputType glInputType[JET_END];
extern const CSTRING glInputNames[JET_END];

//********************************************************************************************************************

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

int winAddClip(int Datatype, const void *, int, int);
void winClearClipboard(void);
void winCopyClipboard(void);
int winExtractFile(void *, int, char *, int);
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

struct X11Globals {
   bool Manager;
   LONG PixelsPerLine; // Defined by DGA
   LONG BankSize; // Definfed by DGA
};

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
extern void init_xcursors(void);
extern void free_xcursors(void);

extern WORD glDGAAvailable;
extern APTR glDGAMemory;
extern X11Globals glX11;
extern _XDisplay *XDisplay;
extern struct XRandRBase *XRandRBase;
extern bool glX11ShmImage;
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

template <typename T>
void UpdateSurfaceField(objSurface *Self, T SurfaceRecord::*LValue, T Value)
{
   if (Self->initialised()) {
      for (auto &record : glSurfaces) {
         if (record.SurfaceID IS Self->UID) {
            record.*LValue = Value;
            return;
         }
      }
   }
}

inline void clip_rectangle(ClipRectangle &rect, ClipRectangle &clip)
{
   if (rect.Left   < clip.Left)   rect.Left   = clip.Left;
   if (rect.Top    < clip.Top)    rect.Top    = clip.Top;
   if (rect.Right  > clip.Right)  rect.Right  = clip.Right;
   if (rect.Bottom > clip.Bottom) rect.Bottom = clip.Bottom;
}

inline LONG find_bitmap_owner(LONG Index)
{
   return find_bitmap_owner(glSurfaces, Index);
}

//********************************************************************************************************************
// Surface list lookup routines.

inline LONG find_surface_list(extSurface *Surface, LONG Limit = -1)
{
   if (Limit IS -1) Limit = glSurfaces.size();
   else if (Limit > LONG(glSurfaces.size())) {
      pf::Log log(__FUNCTION__);
      log.warning("Invalid Limit parameter of %d (max %d)", Limit, LONG(glSurfaces.size()));
      Limit = glSurfaces.size();
   }

   for (LONG i=0; i < Limit; i++) {
      if (glSurfaces[i].SurfaceID IS Surface->UID) return i;
   }

   return -1;
}

inline LONG find_surface_list(OBJECTID SurfaceID, LONG Limit = -1)
{
   if (Limit IS -1) Limit = glSurfaces.size();
   else if (Limit > LONG(glSurfaces.size())) {
      pf::Log log(__FUNCTION__);
      log.warning("Invalid Limit parameter of %d (max %d)", Limit, LONG(glSurfaces.size()));
      Limit = glSurfaces.size();
   }

   for (LONG i=0; i < Limit; i++) {
      if (glSurfaces[i].SurfaceID IS SurfaceID) return i;
   }

   return -1;
}

inline LONG find_parent_list(const SURFACELIST &list, extSurface *Self)
{
   if ((Self->ListIndex < LONG(list.size())) and (list[Self->ListIndex].SurfaceID IS Self->UID)) {
      for (LONG i=Self->ListIndex-1; i >= 0; i--) {
         if (list[i].SurfaceID IS Self->ParentID) return i;
      }
   }

   return find_surface_list(Self->ParentID);
}
