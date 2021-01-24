
//#define DEBUG
#define PRIVATE_DISPLAY
#define PRV_DISPLAY_MODULE
#define PRV_BITMAP
#define PRV_DISPLAY
#define PRV_POINTER

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

#define DEFAULT_WHEELSPEED 500
#define TIME_DBLCLICK      40
#define REPEAT_BUTTONS     TRUE
#define MAX_CURSOR_WIDTH   32
#define MAX_CURSOR_HEIGHT  32
#define DRAG_XOFFSET       10
#define DRAG_YOFFSET       12

#define BF_DATA     0x01
#define BF_WINVIDEO 0x02

#define SURFACE_READ      (0x0001)   // Read access
#define SURFACE_WRITE     (0x0002)   // Write access
#define SURFACE_READWRITE (SURFACE_READ|SURFACE_WRITE)

#define __system__
#include <parasol/modules/display.h>
#include <parasol/modules/window.h>
#include <parasol/modules/xml.h>
#include <parasol/modules/surface.h>

#include "display.h"
#include "idl.h"

#ifdef __ANDROID__
#include <parasol/modules/android.h>
#endif

#ifdef __xwindows__
 #include <parasol/modules/xrandr.h>
#endif

#undef NULL
#define NULL 0

struct resolution {
   WORD width;
   WORD height;
   WORD bpp;
};

#define MAX_INPUTMSG 2048 // Must be a value to the power of two

// glInputEvents is allocated in shared memory for all processes consuming input events.

static struct {
   ULONG  IndexCounter;   // Counter for message ID's
   InputMsg Msgs[MAX_INPUTMSG];
} *glInputEvents = NULL;

static resolution * get_resolutions(objDisplay *);
static ERROR create_bitmap_class(void);
static ERROR dither(objBitmap *, objBitmap *, ColourFormat *, LONG, LONG, LONG, LONG, LONG, LONG);

static SharedControl *glSharedControl = NULL;
static LONG glSixBitDisplay = FALSE;

#define INPUT_MASK        (MAX_INPUTMSG-1) // All bits will be set if MAX_INPUTMSG is a power of two

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

static std::unordered_map<LONG, InputCallback> glInputCallbacks;

static void input_event_loop(HOSTHANDLE, APTR);

static ERROR create_pointer_class(void);
static ERROR create_display_class(void);
static ERROR GetSurfaceAbs(OBJECTID, LONG *, LONG *, LONG *, LONG *);
static ULONG ConvertRGBToPackedPixel(objBitmap *, RGB8 *) __attribute__ ((unused));
static void input_event_loop(HOSTHANDLE, APTR);
static ERROR LockSurface(objBitmap *, WORD);
static ERROR UnlockSurface(objBitmap *);

//****************************************************************************

#define BLEND_MAX_THRESHOLD 255
#define BLEND_MIN_THRESHOLD 1

#ifndef PI
#define PI (3.141592653589793238462643383279f)
#endif

#ifdef _GLES_ // OpenGL related prototypes
static GLenum alloc_texture(LONG Width, LONG Height, GLuint *TextureID);
static void refresh_display_from_egl(objDisplay *Self);
static ERROR init_egl(void);
static void free_egl(void);
#endif

#ifdef _WIN32

#define DLLCALL // __declspec(dllimport)
#define WINAPI  __stdcall

extern BYTE glTrayIcon, glTaskBar, glStickToFront;

extern "C" {
DLLCALL LONG WINAPI SetPixelV(APTR, LONG, LONG, LONG);
DLLCALL LONG WINAPI SetPixel(APTR, LONG, LONG, LONG);
DLLCALL LONG WINAPI GetPixel(APTR, LONG, LONG);

LONG winBlit(APTR, LONG, LONG, LONG, LONG, APTR, LONG, LONG);
void winGetError(LONG, STRING, LONG);
APTR winCreateCompatibleDC(void);
APTR winCreateBitmap(LONG, LONG, LONG);
void winDeleteDC(APTR);
void winDeleteObject(void *);
void winDrawEllipse(APTR, LONG, LONG, LONG, LONG, LONG, UBYTE *);
void winDrawLine(APTR, LONG, LONG, LONG, LONG, UBYTE *);
void winDrawRectangle(APTR, LONG, LONG, LONG, LONG, UBYTE, UBYTE, UBYTE);
void winGetPixel(APTR, LONG, LONG, UBYTE *);
LONG winGetPixelFormat(LONG *, LONG *, LONG *, LONG *);
APTR winSelectObject(APTR, APTR);
APTR winSetClipping(APTR, LONG, LONG, LONG, LONG);
void winSetDIBitsToDevice(APTR, LONG, LONG, LONG, LONG, LONG, LONG, LONG, LONG, LONG, APTR, LONG, LONG, LONG);
}

#include "win32/windows.h"

#endif
