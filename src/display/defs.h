
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

#define USE_XIMAGE TRUE

#define SIZE_FOCUSLIST   30
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
#include <parasol/modules/window.h>
#include <parasol/modules/xml.h>

#define URF_REDRAWS_CHILDREN     0x00000001

#define UpdateSurfaceField(a,b) { \
   SurfaceList *list; SurfaceControl *ctl; WORD i; \
   if (Self->Head.Flags & NF_INITIALISED) { \
   if ((ctl = gfxAccessList(ARF_UPDATE))) { \
      list = (SurfaceList *)((BYTE *)ctl + ctl->ArrayIndex); \
      for (i=0; i < ctl->Total; i++) { \
         if (list[i].SurfaceID IS (a)->Head.UID) { \
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
   if (Self->Head.Flags & NF_INITIALISED) { \
      if ((ctl = gfxAccessList(ARF_UPDATE))) { \
         list = (SurfaceList *)((BYTE *)ctl + ctl->ArrayIndex); \
         for (i=0; i < ctl->Total; i++) { \
            if (list[i].SurfaceID IS (a)->Head.UID) { \
               list[i].b = (a)->c; \
               break; \
            } \
         } \
         gfxReleaseList(ARF_UPDATE); \
      } \
   } \
}

#define UpdateSurfaceList(a) UpdateSurfaceCopy((a), 0)

struct dcDisplayInputReady { // This is an internal structure used by the display module to replace dcInputReady
   LARGE NextIndex;    // Next message index for the subscriber to look at
   LONG  SubIndex;     // Index into the InputSubscription list
};

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

INLINE ERROR ptrSetWinCursor(APTR Ob, LONG Cursor) {
   struct ptrSetWinCursor args = { Cursor };
   return Action(MT_PtrSetWinCursor, Ob, &args);
}

#define ptrUngrabX11Pointer(obj) Action(MT_PtrUngrabX11Pointer,(obj),0)

INLINE ERROR ptrGrabX11Pointer(APTR Ob, OBJECTID SurfaceID) {
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

extern ERROR create_clipboard_class(void);
extern ERROR create_pointer_class(void);
extern ERROR create_display_class(void);
extern ERROR GetSurfaceAbs(OBJECTID, LONG *, LONG *, LONG *, LONG *);
extern ULONG ConvertRGBToPackedPixel(objBitmap *, RGB8 *) __attribute__ ((unused));
extern void  input_event_loop(HOSTHANDLE, APTR);
extern ERROR LockSurface(objBitmap *, WORD);
extern ERROR UnlockSurface(objBitmap *);
extern resolution * get_resolutions(objDisplay *);
extern ERROR create_bitmap_class(void);
extern ERROR dither(objBitmap *, objBitmap *, ColourFormat *, LONG, LONG, LONG, LONG, LONG, LONG);
extern ERROR get_display_info(OBJECTID, DISPLAYINFO *, LONG);
extern void  update_displayinfo(objDisplay *);
extern void  resize_feedback(FUNCTION *, OBJECTID, LONG X, LONG Y, LONG Width, LONG Height);
extern void  forbidDrawing(void);
extern void  forbidExpose(void);
extern void  permitDrawing(void);
extern void  permitExpose(void);
extern ERROR access_video(OBJECTID DisplayID, objDisplay **, objBitmap **);
extern ERROR apply_style(OBJECTPTR, OBJECTPTR, CSTRING);
extern ERROR load_styles(void);
extern BYTE  check_surface_list(void);
extern UBYTE CheckVisibility(SurfaceList *, WORD);
extern UBYTE check_volatile(SurfaceList *, WORD);
extern ERROR create_surface_class(void);
extern void  expose_buffer(SurfaceList *, WORD Total, WORD Index, WORD ScanIndex, LONG Left, LONG Top, LONG Right, LONG Bottom, OBJECTID VideoID, objBitmap *);
extern WORD  FindBitmapOwner(SurfaceList *, WORD);
extern ERROR gfxRedrawSurface(OBJECTID, LONG, LONG, LONG, LONG, LONG);
extern void  invalidate_overlap(objSurface *, SurfaceList *, WORD, LONG, LONG, LONG, LONG, LONG, LONG, objBitmap *);
extern void  move_layer(objSurface *, LONG, LONG);
extern void  move_layer_pos(SurfaceControl *, LONG, LONG);
extern LONG  msg_handler(APTR, LONG, LONG, APTR, LONG);
extern void  prepare_background(objSurface *, SurfaceList *, WORD, WORD, objBitmap *, ClipRectangle *, BYTE);
extern void  process_surface_callbacks(objSurface *, objBitmap *);
extern void  redraw_nonintersect(OBJECTID, SurfaceList *, WORD, WORD, ClipRectangle *, ClipRectangle *, LONG, LONG);
extern void  release_video(objDisplay *);
extern ERROR track_layer(objSurface *);
extern void  untrack_layer(OBJECTID);
extern void  check_bmp_buffer_depth(objSurface *, objBitmap *);
extern BYTE  restrict_region_to_parents(SurfaceList *, LONG, ClipRectangle *, BYTE);
extern ERROR load_style_values(void);
extern ERROR _expose_surface(OBJECTID SurfaceID, SurfaceList *list, WORD index, WORD total, LONG X, LONG Y, LONG Width, LONG Height, LONG Flags);
extern ERROR _redraw_surface(OBJECTID SurfaceID, SurfaceList *list, WORD Index, WORD Total, LONG Left, LONG Top, LONG Right, LONG Bottom, LONG Flags);
extern void  _redraw_surface_do(objSurface *, SurfaceList *, WORD, WORD, LONG, LONG, LONG, LONG, objBitmap *, LONG);
extern void  check_styles(STRING Path, OBJECTPTR *Script) __attribute__((unused));
extern ERROR resize_layer(objSurface *, LONG, LONG, LONG, LONG, LONG, LONG, LONG, DOUBLE, LONG);
extern ERROR UpdateSurfaceCopy(objSurface *Self, SurfaceList *Copy);
extern LONG  find_surface_list(SurfaceList *list, LONG Total, OBJECTID SurfaceID);
extern LONG  find_parent_list(SurfaceList *list, WORD Total, objSurface *Self);

#ifdef DBG_LAYERS
extern void print_layer_list(STRING Function, SurfaceControl *Ctl, LONG POI)
#endif

extern std::unordered_map<LONG, InputCallback> glInputCallbacks;
extern SharedControl *glSharedControl;
extern LONG glSixBitDisplay;
extern OBJECTPTR glModule;
extern OBJECTPTR clDisplay, clPointer, clBitmap, clClipboard, clSurface;
extern OBJECTID glPointerID;
extern DISPLAYINFO *glDisplayInfo;
extern APTR glDither;
extern LONG glDitherSize;
extern OBJECTPTR glCompress;
extern objCompression *glIconArchive;
extern struct CoreBase *CoreBase;
extern ColourFormat glColourFormat;
extern BYTE glHeadless;
extern FieldDef CursorLookup[];
extern UBYTE *glAlphaLookup;
extern TIMER glRefreshPointerTimer;
extern objBitmap *glComposite;
extern BYTE glDisplayType;
extern DOUBLE glpRefreshRate, glpGammaRed, glpGammaGreen, glpGammaBlue;
extern LONG glpDisplayWidth, glpDisplayHeight, glpDisplayX, glpDisplayY;
extern LONG glpDisplayDepth; // If zero, the display depth will be based on the hosted desktop's bit depth.
extern LONG glpMaximise, glpFullScreen;
extern LONG glpWindowType;
extern char glpDPMS[20];
extern LONG glClassFlags; // Set on CMDInit()
extern objXML *glStyle;
extern OBJECTPTR glAppStyle;
extern OBJECTPTR glDesktopStyleScript;
extern OBJECTPTR glDefaultStyleScript;
extern MsgHandler *glExposeHandler;

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
#define find_own_index(a,b)     find_surface_list( (SurfaceList *)((BYTE *)(a) + (a)->ArrayIndex), (a)->Total, (b)->Head.UID)
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

extern Cursor create_blank_cursor(void);
extern Cursor get_x11_cursor(LONG CursorID);
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
