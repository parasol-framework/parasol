#pragma once

#define __system__
//#define DEBUG
#define PRIVATE_DISPLAY
#define PRV_DISPLAY_MODULE
#define PRV_BITMAP
#define PRV_DISPLAY
#define PRV_POINTER
#define PRV_CLIPBOARD
#define PRV_SURFACE
#define PRV_CONTROLLER
//#define DBG_DRAW_ROUTINES // Use this if you want to debug any external code that is subscribed to surface drawing routines
//#define FASTACCESS
//#define DBG_LAYERS
#define FOCUSMSG(...) //LogF(NULL, __VA_ARGS__)

#include <parasol/system/errors.h>

#ifdef DBG_LAYERS
#include <stdio.h>
#endif

#include <unordered_set>
#include <mutex>
#include <queue>
#include <sstream>
#include <array>
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

#define ALIGN32(a) (((a) + 3) & (~3))

#define SURFACE_READ      (0x0001)   // Read access
#define SURFACE_WRITE     (0x0002)   // Write access
#define SURFACE_READWRITE (SURFACE_READ|SURFACE_WRITE)

#include <parasol/modules/display.h>
#include <parasol/modules/xml.h>
#include <parasol/strings.hpp>
#include "../link/linear_rgb.h"
#include "../link/unicode.h"

using namespace pf;
class extBitmap;

#define UpdateSurfaceRecord(a) update_surface_copy(a)

struct SurfaceRecord {
   APTR     Data;          // For drwCopySurface()
   OBJECTID ParentID;      // Object that owns the surface area
   OBJECTID SurfaceID;     // ID of the surface area
   OBJECTID BitmapID;      // Shared bitmap buffer, if available
   OBJECTID DisplayID;     // Display
   OBJECTID RootID;        // RootLayer
   OBJECTID PopOverID;
   RNF      Flags;         // Surface flags (RNF::VISIBLE etc)
   LONG     X;             // Horizontal coordinate
   LONG     Y;             // Vertical coordinate
   LONG     Width;         // Width
   LONG     Height;        // Height
   LONG     Left;          // Absolute X
   LONG     Top;           // Absolute Y
   LONG     Right;         // Absolute right coordinate
   LONG     Bottom;        // Absolute bottom coordinate
   int16_t     Level;         // Level number within the hierarchy
   int16_t     LineWidth;     // [applies to the bitmap owner]
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
      return ClipRectangle(Left, Top, Right, Bottom);
   }

   inline bool hasFocus() const { return (Flags & RNF::HAS_FOCUS) != RNF::NIL; }
   inline void dropFocus() { Flags &= RNF::HAS_FOCUS; }
   inline bool transparent() const { return (Flags & RNF::TRANSPARENT) != RNF::NIL; }
   inline bool visible() const { return (Flags & RNF::VISIBLE) != RNF::NIL; }
   inline bool invisible() const { return (Flags & RNF::VISIBLE) IS RNF::NIL; }
   inline bool isVolatile() const { return (Flags & RNF::VOLATILE) != RNF::NIL; }
   inline bool isCursor() const { return (Flags & RNF::CURSOR) != RNF::NIL; }
};

typedef std::vector<SurfaceRecord> SURFACELIST;
extern std::recursive_mutex glSurfaceLock;

class WinHook {
public:
   OBJECTID SurfaceID;
   WH Event;

   WinHook(OBJECTID aSurfaceID, WH aEvent) : SurfaceID(aSurfaceID), Event(aEvent) { };

   bool operator== (const WinHook &rhs) const {
      return (SurfaceID == rhs.SurfaceID) and (Event == rhs.Event);
   }

   bool operator() (const WinHook &lhs, const WinHook &rhs) const {
       if (lhs.SurfaceID == rhs.SurfaceID) return UBYTE(lhs.Event) < UBYTE(rhs.Event);
       else return lhs.SurfaceID < rhs.SurfaceID;
   }
};

namespace std {
   template <> struct hash<WinHook> {
      std::size_t operator()(const WinHook &k) const {
         return ((std::hash<OBJECTID>()(k.SurfaceID)
            ^ (std::hash<UBYTE>()(UBYTE(k.Event)) << 1)) >> 1);
      }
   };
}

enum {
   STAGE_PRECOPY=1,
   STAGE_AFTERCOPY,
   STAGE_COMPOSITE
};

static const auto MT_PtrSetWinCursor  = AC(-1);
static const auto MT_PtrGrabX11Pointer = AC(-2);
static const auto MT_PtrUngrabX11Pointer = AC(-3);

struct ptrSetWinCursor { PTC Cursor;  };
struct ptrGrabX11Pointer { OBJECTID SurfaceID;  };

INLINE ERR ptrSetWinCursor(OBJECTPTR Ob, PTC Cursor) {
   struct ptrSetWinCursor args = { Cursor };
   return Action(MT_PtrSetWinCursor, Ob, &args);
}

#define ptrUngrabX11Pointer(obj) Action(MT_PtrUngrabX11Pointer,(obj),0)

INLINE ERR ptrGrabX11Pointer(OBJECTPTR Ob, OBJECTID SurfaceID) {
   struct ptrGrabX11Pointer args = { SurfaceID };
   return Action(MT_PtrGrabX11Pointer, Ob, &args);
}

#include "idl.h"

#ifdef __ANDROID__
#include <parasol/modules/android.h>
#endif

struct resolution {
   int16_t width;
   int16_t height;
   int16_t bpp;
};

extern std::vector<InputEvent> glInputEvents;

// Each input event subscription is registered as an InputCallback

class InputCallback {
public:
   OBJECTID SurfaceFilter;
   JTYPE    InputMask; // JTYPE flags
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
   CLIPTYPE Datatype; // The type of data clipped
   CEF Flags;         // CEF::DELETE may be set for the 'cut' operation
   std::vector<ClipItem> Items;  // List of file locations referencing all the data in this clip entry

   ~ClipRecord();
   ClipRecord(CLIPTYPE pDatatype, CEF pFlags, const std::vector<ClipItem> pItems) :
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
   double   LastClickX, LastClickY;
   double   LastReleaseX, LastReleaseY;
   OBJECTID LastSurfaceID;      // Last object that the pointer was positioned over
   OBJECTID CursorReleaseID;
   OBJECTID DragSurface;        // Draggable surface anchored to the pointer position
   OBJECTID DragParent;         // Parent of the draggable surface
   LONG     CursorRelease;
   PTC      BufferCursor;
   CRF      BufferFlags;
   OBJECTID BufferOwner;
   OBJECTID BufferObject;
   char     DragData[8];          // Data preferences for current drag & drop item
   char     Device[32];
   char     ButtonOrder[12];      // The order of the first 11 buttons can be changed here
   int16_t     ButtonOrderFlags[12]; // Button order represented as JD flags
   BYTE     PostComposite;        // Enable post-composite drawing (default)
   UBYTE    prvOverCursorID;
   struct {
      int16_t HotX;
      int16_t HotY;
   } Cursors[LONG(PTC::END)];
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
   LONG     ListIndex;            // Last known list index
   LONG     InputHandle;          // Input handler for dragging of surfaces
   SWIN     WindowType;           // See SWIN constants
   TIMER    RedrawTimer;          // For ScheduleRedraw()
   SurfaceCallback CallbackCache[4];
   int16_t     ScrollProgress;
   int16_t     Opacity;
   uint16_t InheritedRoot:1;      // TRUE if the user set the RootLayer manually
   uint16_t ParentDefined:1;      // TRUE if the parent field was set manually
   uint16_t SkipPopOver:1;
   uint16_t FixedX:1;
   uint16_t FixedY:1;
   uint16_t Document:1;
   uint16_t RedrawScheduled:1;
   uint16_t RedrawCountdown;      // Unsubscribe from the timer when this value reaches zero.
   BYTE     BitsPerPixel;         // Bitmap bits per pixel
   BYTE     BytesPerPixel;        // Bitmap bytes per pixel
   UBYTE    CallbackCount;
   UBYTE    CallbackSize;         // Current size of the callback array.
   BYTE     Anchored;
};

class extDisplay : public objDisplay {
   public:
   using create = pf::Create<extDisplay>;

   double Gamma[3];          // Red, green, blue gamma radioactivity indicator
   std::vector<struct resolution> Resolutions;
   FUNCTION  ResizeFeedback;
   LONG  ControllerPorts;
   LONG  VDensity;          // Cached DPI value, if calculable.
   LONG  HDensity;
   #ifdef __xwindows__
   union {
      APTR   WindowHandle;
      Window XWindowHandle;
   };
   Pixmap XPixmap;
   #elif __ANDROID__
      ANativeWindow *WindowHandle;
   #else
      APTR   WindowHandle;
   #endif
   int16_t  Opacity;
   char  Manufacturer[60];
   char  Chipset[40];
   char  Display[32];
   char  DisplayManufacturer[60];
};

extern void clean_clipboard(void);
extern ERR  create_bitmap_class(void);
extern ERR  create_clipboard_class(void);
extern ERR  create_controller_class(void);
extern ERR  create_display_class(void);
extern ERR  create_pointer_class(void);
extern ERR  create_surface_class(void);
extern ERR  get_surface_abs(OBJECTID, LONG *, LONG *, LONG *, LONG *);
extern void input_event_loop(HOSTHANDLE, APTR);
extern ERR  lock_surface(extBitmap *, int16_t);
extern ERR  unlock_surface(extBitmap *);
extern ERR  get_display_info(OBJECTID, DISPLAYINFO *, LONG);
extern void resize_feedback(FUNCTION *, OBJECTID, LONG X, LONG Y, LONG Width, LONG Height);
extern void forbidDrawing(void);
extern void forbidExpose(void);
extern void permitDrawing(void);
extern void permitExpose(void);
extern ERR  apply_style(OBJECTPTR, OBJECTPTR, CSTRING);
extern ERR  load_styles(void);
extern LONG find_bitmap_owner(const SURFACELIST &, LONG);
extern void move_layer(extSurface *, LONG, LONG);
extern void move_layer_pos(SURFACELIST &, LONG, LONG);
extern void prepare_background(extSurface *, const SURFACELIST &, LONG, extBitmap *, const ClipRectangle &, BYTE);
extern void process_surface_callbacks(extSurface *, extBitmap *);
extern void refresh_pointer(extSurface *Self);
extern ERR  track_layer(extSurface *);
extern void untrack_layer(OBJECTID);
extern BYTE restrict_region_to_parents(const SURFACELIST &, LONG, ClipRectangle &, bool);
extern ERR  load_style_values(void);
extern ERR  resize_layer(extSurface *, LONG X, LONG Y, LONG, LONG, LONG, LONG, LONG BPP, double, LONG);
extern void redraw_nonintersect(OBJECTID, const SURFACELIST &, LONG, const ClipRectangle &, const ClipRectangle &, IRF, EXF);
extern ERR  _expose_surface(OBJECTID, const SURFACELIST &, LONG, LONG, LONG, LONG, LONG, EXF);
extern ERR  _redraw_surface(OBJECTID, const SURFACELIST &, LONG, LONG, LONG, LONG, LONG, IRF);
extern void _redraw_surface_do(extSurface *, const SURFACELIST &, LONG, ClipRectangle &, extBitmap *, IRF);
extern void check_styles(STRING Path, OBJECTPTR *Script) __attribute__((unused));
extern ERR  update_surface_copy(extSurface *);
extern ERR  update_display(extDisplay *, extBitmap *, LONG X, LONG Y, LONG Width, LONG Height, LONG XDest, LONG YDest);
extern void get_resolutions(extDisplay *);

extern ERR RedrawSurface(OBJECTID, LONG, LONG, LONG, LONG, IRF);

#ifdef DBG_LAYERS
extern void print_layer_list(STRING Function, SurfaceControl *Ctl, LONG POI)
#endif

extern bool glSixBitDisplay;
extern OBJECTPTR glModule;
extern OBJECTPTR clDisplay, clPointer, clBitmap, clClipboard, clSurface, clController;
extern OBJECTID glPointerID;
extern DISPLAYINFO glDisplayInfo;
extern objCompression *glCompress;
extern struct CoreBase *CoreBase;
extern ColourFormat glColourFormat;
extern bool glHeadless;
extern FieldDef CursorLookup[];
extern TIMER glRefreshPointerTimer;
extern extBitmap *glComposite;
extern double glpRefreshRate, glpGammaRed, glpGammaGreen, glpGammaBlue;
extern LONG glpDisplayWidth, glpDisplayHeight, glpDisplayX, glpDisplayY;
extern LONG glpDisplayDepth; // If zero, the display depth will be based on the hosted desktop's bit depth.
extern LONG glpMaximise, glpFullScreen;
extern SWIN glpWindowType;
extern char glpDPMS[20];
extern UBYTE *glDemultiply;
extern std::array<UBYTE, 256 * 256> glAlphaLookup;
extern std::list<ClipRecord> glClips;
extern int glLastPort;

extern ankerl::unordered_dense::map<WinHook, FUNCTION> glWindowHooks;
extern std::vector<OBJECTID> glFocusList;
extern std::recursive_mutex glFocusLock;
extern std::recursive_mutex glSurfaceLock;
extern std::recursive_mutex glInputLock;

// Thread-specific variables.

extern THREADVAR int16_t tlNoDrawing, tlNoExpose, tlVolatileIndex;
extern THREADVAR OBJECTID tlFreeExpose;

struct InputType {
   JTYPE Flags;  // As many flags as necessary to describe the input type
   JTYPE Mask;   // Limited flags to declare the mask that must be used to receive that type
};

extern const InputType glInputType[LONG(JET::END)];
extern const CSTRING glInputNames[LONG(JET::END)];

//********************************************************************************************************************

#ifdef _GLES_ // OpenGL related prototypes
GLenum alloc_texture(LONG Width, LONG Height, GLuint *TextureID);
void refresh_display_from_egl(objDisplay *Self);
ERR init_egl(void);
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
}

#include "win32/windows.h"

HCURSOR GetWinCursor(PTC CursorID);

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
extern ERR resize_pixmap(extDisplay *, LONG, LONG);
extern ERR xr_set_display_mode(LONG *, LONG *);

extern int16_t glDGAAvailable;
extern APTR glDGAMemory;
extern XVisualInfo glXInfoAlpha;
extern X11Globals glX11;
extern _XDisplay *XDisplay;
extern bool glX11ShmImage;
extern bool glXCompositeSupported;
extern UBYTE KeyHeld[LONG(KEY::LIST_END)];
extern UBYTE glTrayIcon, glTaskBar, glStickToFront;
extern KQ glKeyFlags;
extern LONG glXFD, glDGAPixelsPerLine, glDGABankSize;
extern Atom atomSurfaceID, XWADeleteWindow;
extern GC glXGC, glClipXGC;
extern XWindowAttributes glRootWindow;
extern Window glDisplayWindow;
extern Cursor C_Default;
extern OBJECTPTR modXRR;
extern int16_t glPlugin;
extern APTR glDGAVideo;
extern bool glXRRAvailable;

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
   if (Limit IS -1) Limit = LONG(glSurfaces.size());
   else if (Limit > LONG(glSurfaces.size())) {
      pf::Log log(__FUNCTION__);
      log.warning("Invalid Limit parameter of %d (max %d)", Limit, LONG(glSurfaces.size()));
      Limit = LONG(glSurfaces.size());
   }

   for (LONG i=0; i < Limit; i++) {
      if (glSurfaces[i].SurfaceID IS Surface->UID) return i;
   }

   return -1;
}

inline LONG find_surface_list(OBJECTID SurfaceID, LONG Limit = -1)
{
   if (Limit IS -1) Limit = LONG(glSurfaces.size());
   else if (Limit > LONG(glSurfaces.size())) {
      pf::Log log(__FUNCTION__);
      log.warning("Invalid Limit parameter of %d (max %d)", Limit, LONG(glSurfaces.size()));
      Limit = LONG(glSurfaces.size());
   }

   for (LONG i=0; i < Limit; i++) {
      if (glSurfaces[i].SurfaceID IS SurfaceID) return i;
   }

   return -1;
}

inline LONG find_parent_list(const SURFACELIST &list, extSurface *Self)
{
   if ((Self->ListIndex < std::ssize(list)) and (list[Self->ListIndex].SurfaceID IS Self->UID)) {
      for (LONG i=Self->ListIndex-1; i >= 0; i--) {
         if (list[i].SurfaceID IS Self->ParentID) return i;
      }
   }

   return find_surface_list(Self->ParentID);
}

//********************************************************************************************************************

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
         Window window;
         XImage   ximage;
         Drawable drawable;
         XImage   *readable;
         XShmSegmentInfo ShmInfo;
         GC gc;
         LONG pix_width, pix_height;
         bool XShmImage;
      } x11;

      inline GC getGC() {
         if (x11.gc) return x11.gc;
         else return glXGC;
      }

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
