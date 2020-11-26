/*****************************************************************************

The source code of the Parasol project is made publicly available under the
terms described in the LICENSE.TXT file that is distributed with this package.
Please refer to it for further information on licensing.

*****************************************************************************/

//#define DEBUG
#define PRIVATE_DISPLAY
#define PRV_DISPLAY_MODULE
#define PRV_BITMAP
#define PRV_DISPLAY
#define PRV_POINTER

#define USE_XIMAGE TRUE

#define DEFAULT_WHEELSPEED 500
#define TIME_DBLCLICK      40
#define REPEAT_BUTTONS     TRUE
#define MAX_CURSOR_WIDTH   32
#define MAX_CURSOR_HEIGHT  32
#define DRAG_XOFFSET       10
#define DRAG_YOFFSET       12
#define MAX_INPUTMSG       2048             // Must be a value to the power of two
#define INPUT_MASK        (MAX_INPUTMSG-1) // All bits will be set if MAX_INPUTMSG is a power of two

#define BF_DATA     0x01
#define BF_WINVIDEO 0x02

#define SURFACE_READ      (0x0001)   // Read access
#define SURFACE_WRITE     (0x0002)   // Write access
#define SURFACE_READWRITE (SURFACE_READ|SURFACE_WRITE)

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

#include <math.h>

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

static resolution * get_resolutions(objDisplay *);
static ERROR create_bitmap_class(void);
static ERROR dither(objBitmap *, objBitmap *, ColourFormat *, LONG, LONG, LONG, LONG, LONG, LONG);

static SharedControl *glSharedControl = NULL;
static LONG glSixBitDisplay = FALSE;

static ERROR GET_HDensity(objDisplay *Self, LONG *Value);
static ERROR GET_VDensity(objDisplay *Self, LONG *Value);

//****************************************************************************

FieldDef CursorLookup[] = {
   { "None",            0 },
   { "Default",         PTR_DEFAULT },             // Values start from 1 and go up
   { "SizeBottomLeft",  PTR_SIZE_BOTTOM_LEFT },
   { "SizeBottomRight", PTR_SIZE_BOTTOM_RIGHT },
   { "SizeTopLeft",     PTR_SIZE_TOP_LEFT },
   { "SizeTopRight",    PTR_SIZE_TOP_RIGHT },
   { "SizeLeft",        PTR_SIZE_LEFT },
   { "SizeRight",       PTR_SIZE_RIGHT },
   { "SizeTop",         PTR_SIZE_TOP },
   { "SizeBottom",      PTR_SIZE_BOTTOM },
   { "Crosshair",       PTR_CROSSHAIR },
   { "Sleep",           PTR_SLEEP },
   { "Sizing",          PTR_SIZING },
   { "SplitVertical",   PTR_SPLIT_VERTICAL },
   { "SplitHorizontal", PTR_SPLIT_HORIZONTAL },
   { "Magnifier",       PTR_MAGNIFIER },
   { "Hand",            PTR_HAND },
   { "HandLeft",        PTR_HAND_LEFT },
   { "HandRight",       PTR_HAND_RIGHT },
   { "Text",            PTR_TEXT },
   { "Paintbrush",      PTR_PAINTBRUSH },
   { "Stop",            PTR_STOP },
   { "Invisible",       PTR_INVISIBLE },
   { "Custom",          PTR_CUSTOM },
   { "Dragable",        PTR_DRAGGABLE },
   { NULL, 0 }
};

//****************************************************************************

#ifdef __xwindows__

#define MAX_KEYCODES 256
#define TIME_X11DBLCLICK 600

struct XCursor {
   Cursor XCursor;
   LONG CursorID;
   LONG XCursorID;
};

static XCursor XCursors[] = {
   { 0, PTR_DEFAULT,           XC_left_ptr },
   { 0, PTR_SIZE_BOTTOM_LEFT,  XC_bottom_left_corner },
   { 0, PTR_SIZE_BOTTOM_RIGHT, XC_bottom_right_corner },
   { 0, PTR_SIZE_TOP_LEFT,     XC_top_left_corner },
   { 0, PTR_SIZE_TOP_RIGHT,    XC_top_right_corner },
   { 0, PTR_SIZE_LEFT,         XC_left_side },
   { 0, PTR_SIZE_RIGHT,        XC_right_side },
   { 0, PTR_SIZE_TOP,          XC_top_side },
   { 0, PTR_SIZE_BOTTOM,       XC_bottom_side },
   { 0, PTR_CROSSHAIR,         XC_crosshair },
   { 0, PTR_SLEEP,             XC_clock },
   { 0, PTR_SIZING,            XC_sizing },
   { 0, PTR_SPLIT_VERTICAL,    XC_sb_v_double_arrow },
   { 0, PTR_SPLIT_HORIZONTAL,  XC_sb_h_double_arrow },
   { 0, PTR_MAGNIFIER,         XC_hand2 },
   { 0, PTR_HAND,              XC_hand2 },
   { 0, PTR_HAND_LEFT,         XC_hand1 },
   { 0, PTR_HAND_RIGHT,        XC_hand1 },
   { 0, PTR_TEXT,              XC_xterm },
   { 0, PTR_PAINTBRUSH,        XC_pencil },
   { 0, PTR_STOP,              XC_left_ptr },
   { 0, PTR_INVISIBLE,         XC_dot },
   { 0, PTR_DRAGGABLE,         XC_sizing }
};

static Cursor create_blank_cursor(void);
static Cursor get_x11_cursor(LONG CursorID);
static void X11ManagerLoop(HOSTHANDLE, APTR);
static void handle_button_press(XEvent *);
static void handle_button_release(XEvent *);
static void handle_configure_notify(XConfigureEvent *);
static void handle_enter_notify(XCrossingEvent *);
static void handle_exposure(XExposeEvent *);
static void handle_key_press(XEvent *);
static void handle_key_release(XEvent *);
static void handle_motion_notify(XMotionEvent *);
static void handle_stack_change(XCirculateEvent *);

static X11Globals *glX11 = 0;
static _XDisplay *XDisplay = 0;
static XRandRBase *XRandRBase = 0;
static UBYTE glX11ShmImage = FALSE;
static UBYTE KeyHeld[K_LIST_END];
static UBYTE glTrayIcon = 0, glTaskBar = 1, glStickToFront = 0;
static LONG glKeyFlags = 0, glXFD = -1, glDGAPixelsPerLine = 0, glDGABankSize = 0;
static Atom atomSurfaceID = 0, XWADeleteWindow = 0;
static GC glXGC = 0, glClipXGC = 0;
static XWindowAttributes glRootWindow;
static Window glDisplayWindow = 0;
static Cursor C_Default;
static OBJECTPTR modXRR = NULL;
static WORD glPlugin = FALSE;
static APTR glDGAVideo = NULL;
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

HINSTANCE glInstance = 0;

static APTR GetWinCursor(LONG);

static WinCursor winCursors[] = {
   { 0, PTR_DEFAULT,           },  // NOTE: Refer to the microsoft.c file if you change anything here
   { 0, PTR_SIZE_BOTTOM_LEFT,  },
   { 0, PTR_SIZE_BOTTOM_RIGHT, },
   { 0, PTR_SIZE_TOP_LEFT,     },
   { 0, PTR_SIZE_TOP_RIGHT,    },
   { 0, PTR_SIZE_LEFT,         },
   { 0, PTR_SIZE_RIGHT,        },
   { 0, PTR_SIZE_TOP,          },
   { 0, PTR_SIZE_BOTTOM,       },
   { 0, PTR_CROSSHAIR,         },
   { 0, PTR_SLEEP,             },
   { 0, PTR_SIZING,            },
   { 0, PTR_SPLIT_VERTICAL,    },
   { 0, PTR_SPLIT_HORIZONTAL,  },
   { 0, PTR_MAGNIFIER,         },
   { 0, PTR_HAND,              },
   { 0, PTR_HAND_LEFT,         },
   { 0, PTR_HAND_RIGHT,        },
   { 0, PTR_TEXT,              },
   { 0, PTR_PAINTBRUSH,        },
   { 0, PTR_STOP,              },
   { 0, PTR_INVISIBLE,         },
   { 0, PTR_INVISIBLE,         },
   { 0, PTR_DRAGGABLE,         }
};
#endif

#ifdef __ANDROID__
OBJECTPTR modAndroid;
struct AndroidBase *AndroidBase;

static void android_init_window(LONG);
static void android_term_window(LONG);
#endif

#include "module_def.c"

//****************************************************************************
// Note: These values are used as the input masks

static const struct {
   LONG Flags;  // As many flags as necessary to describe the input type
   LONG Mask;   // Limited flags to declare the mask that must be used to receive that type
} glInputType[JET_END] = {
   { 0, 0 },                                         // UNUSED
   { JTYPE_DIGITAL|JTYPE_MOVEMENT, JTYPE_MOVEMENT }, // JET_DIGITAL_X
   { JTYPE_DIGITAL|JTYPE_MOVEMENT, JTYPE_MOVEMENT }, // JET_DIGITAL_Y
   { JTYPE_BUTTON,                 JTYPE_BUTTON },   // JET_BUTTON_1
   { JTYPE_BUTTON,                 JTYPE_BUTTON },   // JET_BUTTON_2
   { JTYPE_BUTTON,                 JTYPE_BUTTON },   // JET_BUTTON_3
   { JTYPE_BUTTON,                 JTYPE_BUTTON },   // JET_BUTTON_4
   { JTYPE_BUTTON,                 JTYPE_BUTTON },   // JET_BUTTON_5
   { JTYPE_BUTTON,                 JTYPE_BUTTON },   // JET_BUTTON_6
   { JTYPE_BUTTON,                 JTYPE_BUTTON },   // JET_BUTTON_7
   { JTYPE_BUTTON,                 JTYPE_BUTTON },   // JET_BUTTON_8
   { JTYPE_BUTTON,                 JTYPE_BUTTON },   // JET_BUTTON_9
   { JTYPE_BUTTON,                 JTYPE_BUTTON },   // JET_BUTTON_10
   { JTYPE_BUTTON,                 JTYPE_BUTTON },   // JET_TRIGGER_LEFT
   { JTYPE_BUTTON,                 JTYPE_BUTTON },   // JET_TRIGGER_RIGHT
   { JTYPE_BUTTON,                 JTYPE_BUTTON },   // JET_BUTTON_START
   { JTYPE_BUTTON,                 JTYPE_BUTTON },   // JET_BUTTON_SELECT
   { JTYPE_BUTTON,                 JTYPE_BUTTON },   // JET_LEFT_BUMPER_1
   { JTYPE_BUTTON,                 JTYPE_BUTTON },   // JET_LEFT_BUMPER_2
   { JTYPE_BUTTON,                 JTYPE_BUTTON },   // JET_RIGHT_BUMPER_1
   { JTYPE_BUTTON,                 JTYPE_BUTTON },   // JET_RIGHT_BUMPER_2
   { JTYPE_ANALOG|JTYPE_MOVEMENT,  JTYPE_MOVEMENT }, // JET_ANALOG_X
   { JTYPE_ANALOG|JTYPE_MOVEMENT,  JTYPE_MOVEMENT }, // JET_ANALOG_Y
   { JTYPE_ANALOG|JTYPE_MOVEMENT,  JTYPE_MOVEMENT }, // JET_ANALOG_Z
   { JTYPE_ANALOG|JTYPE_MOVEMENT,  JTYPE_MOVEMENT }, // JET_ANALOG2_X
   { JTYPE_ANALOG|JTYPE_MOVEMENT,  JTYPE_MOVEMENT }, // JET_ANALOG2_Y
   { JTYPE_ANALOG|JTYPE_MOVEMENT,  JTYPE_MOVEMENT }, // JET_ANALOG2_Z
   { JTYPE_EXT_MOVEMENT,           JTYPE_EXT_MOVEMENT }, // JET_WHEEL
   { JTYPE_EXT_MOVEMENT,           JTYPE_EXT_MOVEMENT }, // JET_WHEEL_TILT
   { JTYPE_EXT_MOVEMENT,           JTYPE_EXT_MOVEMENT }, // JET_PEN_TILT_VERTICAL
   { JTYPE_EXT_MOVEMENT,           JTYPE_EXT_MOVEMENT }, // JET_PEN_TILT_HORIZONTAL
   { JTYPE_MOVEMENT,               JTYPE_MOVEMENT },    // JET_ABS_X
   { JTYPE_MOVEMENT,               JTYPE_MOVEMENT },    // JET_ABS_Y
   { JTYPE_FEEDBACK,               JTYPE_FEEDBACK },    // JET_ENTER_SURFACE
   { JTYPE_FEEDBACK,               JTYPE_FEEDBACK },    // JET_LEAVE_SURFACE
   { JTYPE_EXT_MOVEMENT,           JTYPE_EXT_MOVEMENT }, // JET_PRESSURE
   { JTYPE_EXT_MOVEMENT,           JTYPE_EXT_MOVEMENT }, // JET_DEVICE_TILT_X
   { JTYPE_EXT_MOVEMENT,           JTYPE_EXT_MOVEMENT }, // JET_DEVICE_TILT_Y
   { JTYPE_EXT_MOVEMENT,           JTYPE_EXT_MOVEMENT }, // JET_DEVICE_TILT_Z
   { JTYPE_FEEDBACK,               JTYPE_FEEDBACK }     // JET_DISPLAY_EDGE
};

static const CSTRING glInputNames[JET_END] = {
   "",
   "DIGITAL_X",
   "DIGITAL_Y",
   "BUTTON_1",
   "BUTTON_2",
   "BUTTON_3",
   "BUTTON_4",
   "BUTTON_5",
   "BUTTON_6",
   "BUTTON_7",
   "BUTTON_8",
   "BUTTON_9",
   "BUTTON_10",
   "TRIGGER_LEFT",
   "TRIGGER_RIGHT",
   "BUTTON_START",
   "BUTTON_SELECT",
   "LEFT_BUMPER_1",
   "LEFT_BUMPER_2",
   "RIGHT_BUMPER_1",
   "RIGHT_BUMPER_2",
   "ANALOG_X",
   "ANALOG_Y",
   "ANALOG_Z",
   "ANALOG2_X",
   "ANALOG2_Y",
   "ANALOG2_Z",
   "WHEEL",
   "WHEEL_TILT",
   "PEN_TILT_VERTICAL",
   "PEN_TILT_HORIZONTAL",
   "ABS_X",
   "ABS_Y",
   "ENTERED_SURFACE",
   "LEFT_SURFACE",
   "PRESSURE",
   "DEVICE_TILT_X",
   "DEVICE_TILT_Y",
   "DEVICE_TILT_Z",
   "DISPLAY_EDGE"
};

static OBJECTPTR BitmapClass = NULL;
static OBJECTPTR glCompress = NULL;
struct CoreBase *CoreBase;
#if defined(_WIN32) || defined(__xwindows__)
struct KeyboardBase *KeyboardBase;
#endif
static struct SurfaceBase *SurfaceBase;
static ColourFormat glColourFormat;
static BYTE glHeadless = FALSE;

static struct {
   //LARGE TotalMsgs;     // Total number of messages that have been recorded
   LARGE  IndexCounter;   // Counter for message ID's
   InputMsg Msgs[MAX_INPUTMSG];
} *glInput = NULL;

struct InputSubscription {
   OBJECTID SubscriberID;
   OBJECTID SurfaceID;
   MEMORYID MsgPort;
   LONG     Mask;
   LARGE    LastIndex; // Index of the most recently sent message
   LONG     MsgSent:1;
};

#ifdef _GLES_ // OpenGL specific data
enum { EGL_STOPPED=0, EGL_REQUIRES_INIT, EGL_INITIALISED, EGL_TERMINATED };
static UBYTE glEGLState = 0;
static UBYTE glEGLRefreshDisplay = FALSE;
static OBJECTID glEGLPreferredDepth = 0;
static EGLContext glEGLContext = EGL_NO_CONTEXT;
static EGLSurface glEGLSurface = EGL_NO_SURFACE;
static EGLDisplay glEGLDisplay = EGL_NO_DISPLAY;
static EGLint glEGLWidth, glEGLHeight, glEGLDepth;
static pthread_mutex_t glGraphicsMutex;
static CSTRING glLastLock = NULL;
static LONG glLockCount = 0;
static OBJECTID glActiveDisplayID = 0;
#endif

static OBJECTPTR glModule = NULL;
static OBJECTPTR modSurface = NULL, modKeyboard = NULL;
static OBJECTPTR clDisplay = NULL, clPointer = NULL;
static OBJECTID glPointerID = 0;
static DISPLAYINFO *glDisplayInfo;
static APTR glDither = NULL;
static LONG glDitherSize = 0;

#define BLEND_MAX_THRESHOLD 255
#define BLEND_MIN_THRESHOLD 1

#ifndef PI
#define PI (3.141592653589793238462643383279f)
#endif

static ERROR create_pointer_class(void);
static ERROR create_display_class(void);
static ERROR GetSurfaceAbs(OBJECTID, LONG *, LONG *, LONG *, LONG *);
static ULONG ConvertRGBToPackedPixel(objBitmap *, RGB8 *) __attribute__ ((unused));
static ERROR LockSurface(objBitmap *, WORD);
static ERROR UnlockSurface(objBitmap *);

#ifdef _GLES_ // OpenGL related prototypes
static GLenum alloc_texture(LONG Width, LONG Height, GLuint *TextureID);
static void refresh_display_from_egl(objDisplay *Self);
static ERROR init_egl(void);
static void free_egl(void);
#endif

//****************************************************************************
// Alpha blending data.

static UBYTE *glAlphaLookup = NULL;

INLINE UBYTE clipByte(LONG value)
{
   value = (0 & (-(WORD)(value < 0))) | (value & (-(WORD)!(value < 0)));
   value = (255 & (-(WORD)(value > 255))) | (value & (-(WORD)!(value > 255)));
   return value;
}

//****************************************************************************
// GLES specific functions

#ifdef _GLES_
static LONG nearestPower(LONG value)
{
   int i = 1;

   if (value == 0) return value;
   if (value < 0) value = -value;

   for (;;) {
      if (value == 1) break;
      else if (value == 3) {
         i = i * 4;
         break;
      }
      value >>= 1;
      i *= 2;
   }

   return i;
}

int pthread_mutex_timedlock (pthread_mutex_t *mutex, int Timeout) __attribute__ ((unused));

int pthread_mutex_timedlock (pthread_mutex_t *mutex, int Timeout)
{
   struct timespec sleepytime;
   int retcode;

   sleepytime.tv_sec = 0;
   sleepytime.tv_nsec = 10000000; // 10ms

   LARGE start = PreciseTime();
   while ((retcode = pthread_mutex_trylock(mutex)) IS EBUSY) {
      if (PreciseTime() - start >= Timeout * 1000LL) return ETIMEDOUT;
      nanosleep(&sleepytime, NULL);
   }

   return retcode;
}
#endif

//****************************************************************************
// lock_graphics_active() is intended for functionality that MUST have access to an active OpenGL display.  If an EGL
// display is unavailable then this function will fail even if the lock could otherwise be granted.

#ifdef _GLES_
static ERROR lock_graphics_active(CSTRING Caller)
{
   parasol::Log log(__FUNCTION__);

   //log.traceBranch("%s, Count: %d, State: %d, Display: $%x, Context: $%x", Caller, glLockCount, glEGLState, (LONG)glEGLDisplay, (LONG)glEGLContext); // See unlock_graphics() for the matching step.
   if (!pthread_mutex_lock(&glGraphicsMutex)) {
   //if (!(errno = pthread_mutex_timedlock(&glGraphicsMutex, 7000))) {
      glLastLock = Caller;

      if (glEGLState IS EGL_REQUIRES_INIT) {
         init_egl();
      }

      if ((glEGLState != EGL_INITIALISED) or (glEGLDisplay IS EGL_NO_DISPLAY)) {
         pthread_mutex_unlock(&glGraphicsMutex);
         //log.trace("EGL not initialised.");
         return ERR_NotInitialised;
      }

      if ((glEGLContext != EGL_NO_CONTEXT) and (!glLockCount)) {
         // eglMakeCurrent() allows our thread to use OpenGL.
         if (eglMakeCurrent(glEGLDisplay, glEGLSurface, glEGLSurface, glEGLContext) == EGL_FALSE) { // Failure probably indicates that a power management event has occurred (requires re-initialisation).
            pthread_mutex_unlock(&glGraphicsMutex);
            return ERR_NotInitialised;
         }
      }

      glLockCount++;
      return ERR_Okay;
   }
   else {
      log.warning("Failed to get lock for %s.  Locked by %s.  Error: %s", Caller, glLastLock, strerror(errno));
      return ERR_TimeOut;
   }
}

static void unlock_graphics(void)
{
   glLockCount--;
   if (!glLockCount) {
      glLastLock = NULL;
      if (glEGLContext != EGL_NO_CONTEXT) { // Turn off eglMakeCurrent() so that other threads can use OpenGL
         eglMakeCurrent(glEGLDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
      }
   }
   pthread_mutex_unlock(&glGraphicsMutex);
}

#endif

//****************************************************************************

static ERROR GetSurfaceAbs(OBJECTID SurfaceID, LONG *AbsX, LONG *AbsY, LONG *Width, LONG *Height)
{
   SurfaceControl *ctl;

   if (!AccessMemory(glSharedControl->SurfacesMID, MEM_READ, 500, &ctl)) {
      auto list = (SurfaceList *)((BYTE *)ctl + ctl->ArrayIndex);
      LONG i;
      for (i=0; (list[i].SurfaceID) and (list[i].SurfaceID != SurfaceID); i++);

      if (!list[i].SurfaceID) {
         ReleaseMemory(ctl);
         return ERR_Search;
      }
      if (AbsX) *AbsX = list[i].Left;
      if (AbsY) *AbsY = list[i].Top;
      if (Width)  *Width  = list[i].Width;
      if (Height) *Height = list[i].Height;
      ReleaseMemory(ctl);
      return ERR_Okay;
   }
   else return ERR_AccessMemory;
}

//****************************************************************************

#ifdef __xwindows__

static WORD glDGAAvailable = -1; // -1 indicates that we have not tried the setup process yet
static APTR glDGAMemory = NULL;

static LONG x11DGAAvailable(APTR *VideoAddress, LONG *PixelsPerLine, LONG *BankSize)
{
   parasol::Log log(__FUNCTION__);
   STRING displayname;

   glX11->DGACount++;
   *VideoAddress = NULL;

   if ((glX11->Manager IS FALSE) and (glX11->DGAInitialised IS FALSE)) {
      //if (glX11->DGACount <= 1) printf("Fast video access is not enabled (must be in full screen mode)\n");
      return FALSE;
   }

   if (glDGAAvailable IS -1) {
      // Check for the DGA driver.  This will only work if the extension is version 2.0+ and we have permissions
      // to map memory.

      glDGAAvailable = FALSE;

      displayname = XDisplayName(NULL);
      if ((!StrCompare(displayname, ":", 1, NULL)) or
          (!StrCompare(displayname, "unix:", 5, NULL)) ) {
         LONG events, errors, major, minor, screen;

         if (XDGAQueryExtension(XDisplay, &events, &errors) and XDGAQueryVersion(XDisplay, &major, &minor)) {
            screen = DefaultScreen(XDisplay);

            // This part will map the video buffer memory into our process.  Access to /dev/mem is required in order
            // for this to work.  After doing this, superuser privileges are dropped immediately.

            if (!SetResource(RES_PRIVILEGED_USER, TRUE)) {
               if ((major >= 2) and (XDGAOpenFramebuffer(XDisplay, screen))) { // Success, DGA is enabled
                  LONG ram;

                  // Get RAM address, pixels-per-line, bank-size and total amount of video memory

                  XF86DGAGetVideo(XDisplay, DefaultScreen(XDisplay), (char **)&glDGAMemory, &glX11->PixelsPerLine, &glX11->BankSize, &ram);

                  XDGACloseFramebuffer(XDisplay, screen);
                  glDGAAvailable = TRUE;
                  glX11->DGAInitialised = TRUE;
               }
               else if (glX11->DGACount <= 1) printf("\033[1mFast video access is not available (driver needs root access)\033[0m\n");

               SetResource(RES_PRIVILEGED_USER, FALSE);

               // Now we permanently drop root capabilities.  The exception to the rule is the desktop executable,
               // which always runs with privileges (indicated via RES_PRIVILEGED).

               if (GetResource(RES_PRIVILEGED) IS FALSE) {
                  setuid(getuid());
               }
            }
            else if (glX11->DGACount <= 1) printf("\033[1mFast video access is not available (driver needs root access)\033[0m\n");
         }
         else if (glX11->DGACount <= 1) printf("Fast video access is not available (DGA extension failure).\n");
      }
      else log.warning("DGA is not available (display %s).", displayname);
   }

   if (VideoAddress)  *VideoAddress = glDGAMemory;
   if (PixelsPerLine) *PixelsPerLine = glX11->PixelsPerLine;
   if (BankSize)      *BankSize = glX11->BankSize;
   return glDGAAvailable;
}

//**********************************************************************
// This routine is called if there is another window manager running.

static XErrorHandler CatchRedirectError(Display *XDisplay, XErrorEvent *event)
{
   parasol::Log log("X11");
   log.msg("A window manager has been detected on this X11 server.");
   glX11->Manager = FALSE;
   return 0;
}

//****************************************************************************

const CSTRING glXProtoList[] = { NULL,
"CreateWindow","ChangeWindowAttributes","GetWindowAttributes","DestroyWindow","DestroySubwindows","ChangeSaveSet","ReparentWindow","MapWindow","MapSubwindows",
"UnmapWindow","UnmapSubwindows","ConfigureWindow","CirculateWindow","GetGeometry","QueryTree","InternAtom","GetAtomName",
"ChangeProperty","DeleteProperty","GetProperty","ListProperties","SetSelectionOwner","GetSelectionOwner","ConvertSelection","SendEvent",
"GrabPointer","UngrabPointer","GrabButton","UngrabButton","ChangeActivePointerGrab","GrabKeyboard","UngrabKeyboard","GrabKey",
"UngrabKey","AllowEvents","GrabServer","UngrabServer","QueryPointer","GetMotionEvents","TranslateCoords","WarpPointer",
"SetInputFocus","GetInputFocus","QueryKeymap","OpenFont","CloseFont","QueryFont","QueryTextExtents","ListFonts",
"ListFontsWithInfo","SetFontPath","GetFontPath","CreatePixmap","FreePixmap","CreateGC","ChangeGC","CopyGC",
"SetDashes","SetClipRectangles","FreeGC","ClearArea","CopyArea","CopyPlane","PolyVertex","PolyLine",
"PolySegment","PolyRectangle","PolyArc","FillPoly","PolyFillRectangle","PolyFillArc","PutImage","GetImage",
"PolyText8","PolyText16","ImageText8","ImageText16","CreateColormap","FreeColormap","CopyColormapAndFree","InstallColormap",
"UninstallColormap","ListInstalledColormaps","AllocColor","AllocNamedColor","AllocColorCells","AllocColorPlanes","FreeColors","StoreColors",
"StoreNamedColor","QueryColors","LookupColor","CreateCursor","CreateGlyphCursor","FreeCursor","RecolorCursor","QueryBestSize",
"QueryExtension","ListExtensions","ChangeKeyboardMapping","GetKeyboardMapping","ChangeKeyboardControl","GetKeyboardControl","Bell","ChangePointerControl",
"GetPointerControl","SetScreenSaver","GetScreenSaver","ChangeHosts","ListHosts","SetAccessControl","SetCloseDownMode","KillClient",
"RotateProperties","ForceScreenSaver","SetPointerMapping","GetPointerMapping","SetModifierMapping","GetModifierMapping","NoOperation"
};

static XErrorHandler CatchXError(Display *XDisplay, XErrorEvent *XEvent)
{
   parasol::Log log("X11");
   char buffer[80];

   if (XDisplay) {
      XGetErrorText(XDisplay, XEvent->error_code, buffer, sizeof(buffer)-1);
      if ((XEvent->request_code > 0) and (XEvent->request_code < ARRAYSIZE(glXProtoList))) {
         log.warning("Function: %s, XError: %s", glXProtoList[XEvent->request_code], buffer);
      }
      else log.warning("Function: Unknown, XError: %s", buffer);
   }
   return 0;
}

//****************************************************************************

static int CatchXIOError(Display *XDisplay)
{
   parasol::Log log("X11");
   log.error("A fatal XIO error occurred in relation to display \"%s\".", XDisplayName(NULL));
   return 0;
}

/*****************************************************************************
** Returns TRUE if we are the window manager for the display.
*/

static LONG x11WindowManager(void)
{
   if (glX11) return glX11->Manager;
   else return FALSE;
}

#endif

//****************************************************************************

static ERROR get_display_info(OBJECTID DisplayID, DISPLAYINFO *Info, LONG InfoSize)
{
   parasol::Log log(__FUNCTION__);

  //log.traceBranch("Display: %d, Info: %p, Size: %d", DisplayID, Info, InfoSize);

   if (!Info) return log.warning(ERR_NullArgs);

   if (InfoSize != sizeof(DisplayInfoV3)) {
      log.error("Invalid InfoSize of %d (V3: %d)", InfoSize, (LONG)sizeof(DisplayInfoV3));
      return log.warning(ERR_Args);
   }

   objDisplay *display;
   if (DisplayID) {
      if (glDisplayInfo->DisplayID IS DisplayID) {
         CopyMemory(glDisplayInfo, Info, InfoSize);
         return ERR_Okay;
      }
      else if (!AccessObject(DisplayID, 5000, &display)) {
         Info->DisplayID     = DisplayID;
         Info->Flags         = display->Flags;
         Info->Width         = display->Width;
         Info->Height        = display->Height;
         Info->BitsPerPixel  = display->Bitmap->BitsPerPixel;
         Info->BytesPerPixel = display->Bitmap->BytesPerPixel;
         Info->AmtColours    = display->Bitmap->AmtColours;
         GET_HDensity(display, &Info->HDensity);
         GET_VDensity(display, &Info->VDensity);

         #ifdef __xwindows__
            Info->AccelFlags = -1;
            if (glDGAAvailable IS TRUE) {
               Info->AccelFlags &= ~ACF_VIDEO_BLIT; // Turn off video blitting when X11DGA is active (it does not provide blitter syncing)
            }
         #else
            Info->AccelFlags = -1;
         #endif

         Info->PixelFormat.RedShift   = display->Bitmap->ColourFormat->RedShift;
         Info->PixelFormat.GreenShift = display->Bitmap->ColourFormat->GreenShift;
         Info->PixelFormat.BlueShift  = display->Bitmap->ColourFormat->BlueShift;
         Info->PixelFormat.AlphaShift = display->Bitmap->ColourFormat->AlphaShift;
         Info->PixelFormat.RedMask    = display->Bitmap->ColourFormat->RedMask;
         Info->PixelFormat.GreenMask  = display->Bitmap->ColourFormat->GreenMask;
         Info->PixelFormat.BlueMask   = display->Bitmap->ColourFormat->BlueMask;
         Info->PixelFormat.AlphaMask  = display->Bitmap->ColourFormat->AlphaMask;
         Info->PixelFormat.RedPos     = display->Bitmap->ColourFormat->RedPos;
         Info->PixelFormat.GreenPos   = display->Bitmap->ColourFormat->GreenPos;
         Info->PixelFormat.BluePos    = display->Bitmap->ColourFormat->BluePos;
         Info->PixelFormat.AlphaPos   = display->Bitmap->ColourFormat->AlphaPos;

         ReleaseObject(display);
         return ERR_Okay;
      }
      else return log.warning(ERR_AccessObject);
   }
   else {
      // If no display is specified, return default display settings for the main monitor and availability flags.

      Info->Flags = 0;

#ifdef __xwindows__
      XPixmapFormatValues *list;
      LONG count, i;

      Info->Width  = glRootWindow.width;
      Info->Height = glRootWindow.height;
      Info->AccelFlags = -1;
      #warning TODO: Get display density
      Info->VDensity = 96;
      Info->HDensity = 96;

      if (glDGAAvailable IS TRUE) {
         Info->AccelFlags &= ~ACF_VIDEO_BLIT; // Turn off video blitting when DGA is active
      }

      Info->BitsPerPixel = DefaultDepth(XDisplay, DefaultScreen(XDisplay));

      if (Info->BitsPerPixel <= 8) Info->BytesPerPixel = 1;
      else if (Info->BitsPerPixel <= 16) Info->BytesPerPixel = 2;
      else if (Info->BitsPerPixel <= 24) Info->BytesPerPixel = 3;
      else Info->BytesPerPixel = 4;

      if ((list = XListPixmapFormats(XDisplay, &count))) {
         for (i=0; i < count; i++) {
            if (list[i].depth IS Info->BitsPerPixel) {
               Info->BytesPerPixel = list[i].bits_per_pixel;
               if (list[i].bits_per_pixel <= 8) Info->BytesPerPixel = 1;
               else if (list[i].bits_per_pixel <= 16) Info->BytesPerPixel = 2;
               else if (list[i].bits_per_pixel <= 24) Info->BytesPerPixel = 3;
               else {
                  Info->BytesPerPixel = 4;
                  Info->BitsPerPixel  = 32;
               }
            }
         }
         XFree(list);
      }

#elif _WIN32
      LONG width, height, bits, bytes, colours, hdpi, vdpi;

      #warning TODO: Allow the user to set a custom DPI via style values.

      winGetDesktopSize(&width, &height);
      winGetDisplaySettings(&bits, &bytes, &colours);
      winGetDPI(&hdpi, &vdpi);

      Info->Width         = width;
      Info->Height        = height;
      Info->BitsPerPixel  = bits;
      Info->BytesPerPixel = bytes;
      Info->AccelFlags    = 0xffffffffffffffffLL;
      Info->HDensity      = hdpi;
      Info->VDensity      = vdpi;
      if (Info->HDensity < 96) Info->HDensity = 96;
      if (Info->VDensity < 96) Info->VDensity = 96;

#elif __ANDROID__
      // On Android the current display information is always returned.

      log.trace("Refresh");
      if (!adLockAndroid(3000)) {
         ANativeWindow *window;
         if (!adGetWindow(&window)) {
            // TODO: The recommended pixel depth should be determined by analysing the device's CPU capability, the
            // graphics chip and available memory.

            glDisplayInfo->DisplayID     = 0;
            glDisplayInfo->Width         = ANativeWindow_getWidth(window);
            glDisplayInfo->Height        = ANativeWindow_getHeight(window);
            glDisplayInfo->BitsPerPixel  = 16;
            glDisplayInfo->BytesPerPixel = 2;
            glDisplayInfo->AccelFlags    = ACF_VIDEO_BLIT;
            glDisplayInfo->Flags         = SCR_MAXSIZE;  // Indicates that the width and height are the display's maximum.

            AConfiguration *config;
            if (!adGetConfig(&config)) {
               glDisplayInfo->HDensity = AConfiguration_getDensity(config);
               if (glDisplayInfo->HDensity < 60) glDisplayInfo->HDensity = 160;
            }
            else glDisplayInfo->HDensity = 160;

            glDisplayInfo->VDensity = glDisplayInfo->HDensity;

            LONG pixel_format = ANativeWindow_getFormat(window);
            if ((pixel_format IS WINDOW_FORMAT_RGBA_8888) or (pixel_format IS WINDOW_FORMAT_RGBX_8888)) {
               glDisplayInfo->BytesPerPixel = 32;
               if (pixel_format IS WINDOW_FORMAT_RGBA_8888) glDisplayInfo->BitsPerPixel = 32;
               else glDisplayInfo->BitsPerPixel = 24;
            }

            CopyMemory(&glColourFormat, &glDisplayInfo->PixelFormat, sizeof(glDisplayInfo->PixelFormat));

            if ((glDisplayInfo->BitsPerPixel < 8) or (glDisplayInfo->BitsPerPixel > 32)) {
               if (glDisplayInfo->BitsPerPixel > 32) glDisplayInfo->BitsPerPixel = 32;
               else if (glDisplayInfo->BitsPerPixel < 15) glDisplayInfo->BitsPerPixel = 16;
            }

            if (glDisplayInfo->BitsPerPixel > 24) glDisplayInfo->AmtColours = 1<<24;
            else glDisplayInfo->AmtColours = 1<<glDisplayInfo->BitsPerPixel;

            log.trace("%dx%dx%d", glDisplayInfo->Width, glDisplayInfo->Height, glDisplayInfo->BitsPerPixel);
         }
         else {
            adUnlockAndroid();
            return log.warning(ERR_SystemCall);
         }

         adUnlockAndroid();
      }
      else return log.warning(ERR_TimeOut);

      CopyMemory(glDisplayInfo, Info, InfoSize);
      return ERR_Okay;
#else

      if (glDisplayInfo->DisplayID) {
         CopyMemory(glDisplayInfo, Info, InfoSize);
         return ERR_Okay;
      }
      else {
         Info->Width         = 1024;
         Info->Height        = 768;
         Info->BitsPerPixel  = 32;
         Info->BytesPerPixel = 4;
         Info->AccelFlags = ACF_SOFTWARE_BLIT;
         Info->HDensity = 96;
         Info->VDensity = 96;
      }
#endif

      Info->PixelFormat.RedShift   = glColourFormat.RedShift;
      Info->PixelFormat.GreenShift = glColourFormat.GreenShift;
      Info->PixelFormat.BlueShift  = glColourFormat.BlueShift;
      Info->PixelFormat.AlphaShift = glColourFormat.AlphaShift;
      Info->PixelFormat.RedMask    = glColourFormat.RedMask;
      Info->PixelFormat.GreenMask  = glColourFormat.GreenMask;
      Info->PixelFormat.BlueMask   = glColourFormat.BlueMask;
      Info->PixelFormat.AlphaMask  = glColourFormat.AlphaMask;
      Info->PixelFormat.RedPos     = glColourFormat.RedPos;
      Info->PixelFormat.GreenPos   = glColourFormat.GreenPos;
      Info->PixelFormat.BluePos    = glColourFormat.BluePos;
      Info->PixelFormat.AlphaPos   = glColourFormat.AlphaPos;

      if ((Info->BitsPerPixel < 8) or (Info->BitsPerPixel > 32)) {
         log.warning("Invalid bpp of %d.", Info->BitsPerPixel);
         if (Info->BitsPerPixel > 32) Info->BitsPerPixel = 32;
         else if (Info->BitsPerPixel < 8) Info->BitsPerPixel = 8;
      }

      if (Info->BitsPerPixel > 24) Info->AmtColours = 1<<24;
      else Info->AmtColours = 1<<Info->BitsPerPixel;

      log.trace("%dx%dx%d", Info->Width, Info->Height, Info->BitsPerPixel);
      return ERR_Okay;
   }
}

/*****************************************************************************
** Command: Init()
*/

static ERROR CMDInit(OBJECTPTR argModule, struct CoreBase *argCoreBase)
{
   parasol::Log log(__FUNCTION__);
   DOUBLE fAlpha;
   WORD iValue, iAlpha;
   LONG i;
   ERROR error;
   #ifdef __xwindows__
      XGCValues gcv;
      LONG shmmajor, shmminor, pixmaps;
   #endif

   CoreBase = argCoreBase;

   GetPointer(argModule, FID_Master, &glModule);

   CSTRING driver_name;
   if ((driver_name = (CSTRING)GetResourcePtr(RES_DISPLAY_DRIVER))) {
      log.msg("User requested display driver '%s'", driver_name);
      if ((!StrMatch(driver_name, "none")) or (!StrMatch(driver_name, "headless"))) {
         glHeadless = TRUE;
      }
   }

   // NB: The display module cannot load the Surface module during initialisation due to recursive dependency.

   glSharedControl = (SharedControl *)GetResourcePtr(RES_SHARED_CONTROL);

   #ifdef _GLES_
      pthread_mutexattr_t attr;
      pthread_mutexattr_init(&attr);
      pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE); // Allow recursive use of lock_graphics()

      pthread_mutex_init(&glGraphicsMutex, &attr);
   #endif

   #ifdef __ANDROID__
      if (GetResource(RES_SYSTEM_STATE) >= 0) {
         if (LoadModule("android", MODVERSION_ANDROID, (OBJECTPTR *)&modAndroid, &AndroidBase) != ERR_Okay) return ERR_InitModule;

         FUNCTION fInitWindow, fTermWindow;
         SET_CALLBACK_STDC(fInitWindow, &android_init_window); // Sets EGL for re-initialisation and draws the display.
         SET_CALLBACK_STDC(fTermWindow, &android_term_window); // Frees EGL

         if (adAddCallbacks(ACB_INIT_WINDOW, &fInitWindow,
                            ACB_TERM_WINDOW, &fTermWindow,
                            TAGEND) != ERR_Okay) {
            return ERR_SystemCall;
         }
      }
   #endif

   MEMORYID memoryid = RPM_DisplayInfo;
   error = AllocMemory(sizeof(DISPLAYINFO), MEM_UNTRACKED|MEM_PUBLIC|MEM_RESERVED|MEM_NO_BLOCKING, &glDisplayInfo, &memoryid);
   if (error IS ERR_ResourceExists) {
      if (!glDisplayInfo) {
         if (AccessMemory(RPM_DisplayInfo, MEM_READ_WRITE|MEM_NO_BLOCKING, 1000, &glDisplayInfo) != ERR_Okay) {
            return log.warning(ERR_AccessMemory);
         }
      }
   }
   else if (error) return ERR_AllocMemory;
   else glDisplayInfo->DisplayID = 0xffffffff; // Indicate a refresh of the cache is required.

   // Allocate the input message cyclic array

   memoryid = RPM_InputMsgs;
   error = AllocMemory(sizeof(glInput[0]), MEM_UNTRACKED|MEM_PUBLIC|MEM_RESERVED|MEM_NO_BLOCKING, &glInput, &memoryid);
   if (error IS ERR_ResourceExists) {
      if (!glInput) {
         if (AccessMemory(RPM_InputMsgs, MEM_READ_WRITE|MEM_NO_BLOCKING, 1000, &glInput) != ERR_Okay) {
            return log.warning(ERR_AccessMemory);
         }
      }
   }
   else if (error) return ERR_AllocMemory;

#ifdef __xwindows__
   if (!glHeadless) {
      log.trace("Allocating global memory structure.");

      memoryid = RPM_X11;
      if (!(error = AllocMemory(sizeof(X11Globals), MEM_UNTRACKED|MEM_PUBLIC|MEM_RESERVED|MEM_NO_BLOCKING, (APTR)&glX11, &memoryid))) {
         glX11->Manager = TRUE; // Assume that we are the window manager
      }
      else if (error IS ERR_ResourceExists) {
         if (!glX11) {
            if (AccessMemory(RPM_X11, MEM_READ_WRITE, 1000, &glX11) != ERR_Okay) {
               return log.warning(ERR_AccessMemory);
            }
         }
      }
      else return log.warning(ERR_AllocMemory);

      // Attempt to open X11.  Use PARASOL_XDISPLAY if set, otherwise use the DISPLAY variable.

      log.msg("Attempting to open X11...");

      CSTRING strdisplay = getenv("PARASOL_XDISPLAY");
      if (!strdisplay) strdisplay = getenv("DISPLAY");

      if ((XDisplay = XOpenDisplay(strdisplay))) {
         // Select the X messages that we want to receive from the root window.  This will also tell us if an X11 manager
         // is currently running or not (refer to the CatchRedirectError() exception routine).

         if (glX11->InitCount < 1) {
            XSetErrorHandler((XErrorHandler)CatchRedirectError);

            XSelectInput(XDisplay, RootWindow(XDisplay, DefaultScreen(XDisplay)),
               LeaveWindowMask|EnterWindowMask|PointerMotionMask|
               PropertyChangeMask|SubstructureRedirectMask| // SubstructureNotifyMask |
               KeyPressMask|ButtonPressMask|ButtonReleaseMask);

            if (!getenv("PARASOL_XDISPLAY")) setenv("PARASOL_XDISPLAY", strdisplay, FALSE);

            XSync(XDisplay, False);
         }

         XSetErrorHandler((XErrorHandler)CatchXError);
         XSetIOErrorHandler(CatchXIOError);
      }
      else return ERR_Failed;

      glX11->InitCount++;

      // Try to load XRandR, but it's okay if it's not available

      if (!NewObject(ID_MODULE, NULL, &modXRR)) {
         char buffer[32];
         IntToStr((MAXINT)XDisplay, buffer, sizeof(buffer));
         acSetVar(modXRR, "XDisplay", buffer);
         SetString(modXRR, FID_Name, "xrandr");
         if (!acInit(modXRR)) {
            if (GetPointer(modXRR, FID_ModBase, &XRandRBase) != ERR_Okay) XRandRBase = NULL;
         }
      }
      else XRandRBase = NULL;

      // Get the X11 file descriptor (for incoming events) and tell the Core to listen to it when the task is sleeping.

      glXFD = XConnectionNumber(XDisplay);
      fcntl(glXFD, F_SETFD, 1); // FD does not duplicate across exec()
      SetResource(RES_X11_FD, glXFD);
      RegisterFD(glXFD, RFD_READ, X11ManagerLoop, NULL);

      // This function checks for DGA and also maps the video memory for us

      glDGAAvailable = x11DGAAvailable(&glDGAVideo, &glDGAPixelsPerLine, &glDGABankSize);

      log.msg("DGA Enabled: %d", glDGAAvailable);

      // Create the graphics contexts for drawing directly to X11 windows

      gcv.function = GXcopy;
      gcv.graphics_exposures = False;
      glXGC = XCreateGC(XDisplay, DefaultRootWindow(XDisplay), GCGraphicsExposures|GCFunction, &gcv);

      gcv.function = GXcopy;
      gcv.graphics_exposures = False;
      glClipXGC = XCreateGC(XDisplay, DefaultRootWindow(XDisplay), GCGraphicsExposures|GCFunction, &gcv);

      #ifdef USE_XIMAGE
         if (XShmQueryVersion(XDisplay, &shmmajor, &shmminor, &pixmaps)) {
            log.msg("X11 shared image extension is active.");
            glX11ShmImage = TRUE;
         }
      #endif

      if (x11WindowManager() IS FALSE) {
         // We are an X11 client
//         XSelectInput(XDisplay, RootWindow(XDisplay, DefaultScreen(XDisplay)), NULL);
      }

      C_Default = XCreateFontCursor(XDisplay, XC_left_ptr);

      XWADeleteWindow = XInternAtom(XDisplay, "WM_DELETE_WINDOW", False);
      atomSurfaceID   = XInternAtom(XDisplay, "PARASOL_SCREENID", False);

      // Get root window attributes

      XGetWindowAttributes(XDisplay, DefaultRootWindow(XDisplay), &glRootWindow);

      ClearMemory(KeyHeld, sizeof(KeyHeld));

      // Drop superuser privileges following X11 initialisation (we only need suid for DGA).

      seteuid(getuid());

      log.trace("Loading X11 cursor graphics.");

      for (i=0; i < ARRAYSIZE(XCursors); i++) {
         if (XCursors[i].CursorID IS PTR_INVISIBLE) XCursors[i].XCursor = create_blank_cursor();
         else XCursors[i].XCursor = XCreateFontCursor(XDisplay, XCursors[i].XCursorID);
      }

      // Set the DISPLAY variable for clients to :10, which is the default X11 display for the rootless X Server.

      if (x11WindowManager() IS TRUE) {
         setenv("DISPLAY", ":10", TRUE);
      }
   }
#elif _WIN32

   // Load cursor graphics

   log.msg("Loading cursor graphics.");

   if ((glInstance = winGetModuleHandle())) {
      if (!winCreateScreenClass()) return log.warning(ERR_SystemCall);
   }
   else return log.warning(ERR_SystemCall);

   winDisableBatching();

   winInitCursors(winCursors, ARRAYSIZE(winCursors));

#endif

   // Initialise our classes

   if (create_pointer_class() != ERR_Okay) {
      log.warning("Failed to create Pointer class.");
      return ERR_AddClass;
   }

   if (create_display_class() != ERR_Okay) {
      log.warning("Failed to create Display class.");
      return ERR_AddClass;
   }

   if (create_bitmap_class() != ERR_Okay) {
      log.warning("Failed to create Bitmap class.");
      return ERR_AddClass;
   }

   // Initialise 64K alpha blending table, for cutting down on multiplications.  This memory block is shared, so one
   // table serves all processes.

   log.msg("Initialise blending table.");

   memoryid = RPM_AlphaBlend;
   if (!(error = AllocMemory(256 * 256, MEM_UNTRACKED|MEM_PUBLIC|MEM_RESERVED|MEM_NO_BLOCKING, &glAlphaLookup, &memoryid))) {
      i = 0;
      for (iAlpha=0; iAlpha < 256; iAlpha++) {
         fAlpha = (DOUBLE)iAlpha * (1.0 / 255.0);
         for (iValue=0; iValue < 256; iValue++) {
            glAlphaLookup[i++] = clipByte(F2I((DOUBLE)iValue * fAlpha));
         }
      }
   }
   else if (error IS ERR_ResourceExists) {
      if (!glAlphaLookup) {
         if (AccessMemory(RPM_AlphaBlend, MEM_READ_WRITE|MEM_NO_BLOCKING, 500, &glAlphaLookup) != ERR_Okay) {
            return ERR_AccessMemory;
         }
      }
   }
   else return ERR_AllocMemory;

   return ERR_Okay;
}

//****************************************************************************

static ERROR CMDOpen(OBJECTPTR Module)
{
   SetPointer(Module, FID_FunctionList, glFunctions);
   return ERR_Okay;
}

//****************************************************************************

static ERROR CMDExpunge(void)
{
   if (glCompress)    { acFree(glCompress); glCompress = NULL; }
   if (glAlphaLookup) { ReleaseMemory(glAlphaLookup); glAlphaLookup = NULL; }
   if (glDither)      { FreeResource(glDither); glDither = NULL; }

#ifdef __xwindows__

   WORD i;
   ERROR error;

   error = ERR_Okay;

   if (!glHeadless) {
      if (modXRR) { acFree(modXRR); modXRR = NULL; }

      if (glXFD != -1) { DeregisterFD(glXFD); glXFD = -1; }
      SetResource(RES_X11_FD, -1);

      XSetErrorHandler(NULL);
      XSetIOErrorHandler(NULL);

      if (XDisplay) {
         for (i=0; i < ARRAYSIZE(XCursors); i++) {
            if (XCursors[i].XCursor) XFreeCursor(XDisplay, XCursors[i].XCursor);
         }

         if (glXGC) { XFreeGC(XDisplay, glXGC); glXGC = 0; }
         if (glClipXGC) { XFreeGC(XDisplay, glClipXGC); glClipXGC = 0; }

         // Closing the display causes a crash, so we're not doing it anymore ...
         /*
         xtmp = XDisplay;
         XDisplay = NULL;
         XCloseDisplay(xtmp);
         */
      }

      if (glX11) {
         // Note: In full-screen mode, expunging of the X11 module causes segfaults right at the end of program
         // termination.  In order to resolve this problem, we return DoNotExpunge to prevent the removal of X11 module
         // code.  The reason why this problem occurs is because something at program termination relies on our module
         // code being present in the system.

         if (glX11->Manager) error = ERR_DoNotExpunge;

         ReleaseMemory(glX11);
         glX11 = NULL;
      }
   }

#elif __ANDROID__

   if (modAndroid) {
      FUNCTION fInitWindow, fTermWindow;
      SET_CALLBACK_STDC(fInitWindow, &android_init_window);
      SET_CALLBACK_STDC(fTermWindow, &android_term_window);

      adRemoveCallbacks(ACB_INIT_WINDOW, &fInitWindow,
                        ACB_TERM_WINDOW, &fTermWindow,
                        TAGEND);

      acFree(modAndroid);
      modAndroid = NULL;
   }

#elif _WIN32

   winRemoveWindowClass("ScreenClass");
   winFreeDragDrop();

#endif

   if (glInput)       { ReleaseMemory(glInput); glInput = NULL; }
   if (glDisplayInfo) { ReleaseMemory(glDisplayInfo); glDisplayInfo = NULL; }
   if (clPointer)     { acFree(clPointer);   clPointer   = NULL; }
   if (clDisplay)     { acFree(clDisplay);   clDisplay   = NULL; }
   if (BitmapClass)   { acFree(BitmapClass); BitmapClass = NULL; }
   if (modKeyboard)   { acFree(modKeyboard); modKeyboard = NULL; }

   #ifdef _GLES_
      free_egl();
      pthread_mutex_destroy(&glGraphicsMutex);
   #endif

#ifdef __xwindows__
   return error;
#else
   return ERR_Okay;
#endif
}

/*****************************************************************************

-FUNCTION-
StartCursorDrag: Attaches an item to the cursor for the purpose of drag and drop.

This function starts a drag and drop operation with the mouse cursor.  The user must be holding the primary mouse
button to initiate the drag and drop operation.

A Source object ID is required that indicates the origin of the item being dragged and will be used to retrieve the
data on completion of the drag and drop operation. An Item number, which is optional, identifies the item being dragged
from the Source object.

The type of data represented by the source item and all other supportable data types are specified in the Datatypes
parameter as a null terminated array.  The array is arranged in order of preference, starting with the item's native
data type.  Acceptable data type values are listed in the documentation for the DataFeed action.

The Surface argument allows for a composite surface to be dragged by the mouse cursor as a graphical representation of
the source item.  It is recommended that the graphic be 32x32 pixels in size and no bigger than 64x64 pixels.  The
Surface will be hidden on completion of the drag and drop operation.

If the call to StartCursorDrag() is successful, the mouse cursor will operate in drag and drop mode.  The UserMovement
and UserClickRelease actions normally reported from the SystemPointer will now include the JD_DRAGITEM flag in the
ButtonFlags parameter.  When the user releases the primary mouse button, the drag and drop operation will stop and the
DragDrop action will be passed to the surface immediately underneath the mouse cursor.  Objects that are monitoring for
the DragDrop action on that surface can then contact the Source object with a DataFeed DragDropRequest.  The
resulting data is then passed to the requesting object with a DragDropResult on the DataFeed.

-INPUT-
oid Source:     Refers to an object that is managing the source data.
int Item:       A custom number that represents the item being dragged from the source.
cstr Datatypes: A null terminated byte array that lists the datatypes supported by the source item, in order of conversion preference.
oid Surface:    A 32-bit composite surface that represents the item being dragged.

-ERRORS-
Okay:
NullArgs:
AccessObject:
Failed: The left mouse button is not held by the user.
InUse: A drag and drop operation has already been started.

*****************************************************************************/

static ERROR gfxStartCursorDrag(OBJECTID Source, LONG Item, CSTRING Datatypes, OBJECTID Surface)
{
   parasol::Log log(__FUNCTION__);

   log.branch("Source: %d, Item: %d, Surface: %d", Source, Item, Surface);

   if (!Source) return log.warning(ERR_NullArgs);

   objPointer *pointer;
   if ((pointer = gfxAccessPointer())) {
      if (!pointer->Buttons[0].LastClicked) {
         gfxReleasePointer(pointer);
         return log.warning(ERR_Failed);
      }

      if (pointer->DragSourceID) {
         gfxReleasePointer(pointer);
         return ERR_InUse;
      }

      pointer->DragSurface = Surface;
      pointer->DragItem    = Item;
      pointer->DragSourceID = Source;
      StrCopy(Datatypes, pointer->DragData, sizeof(pointer->DragData));

      // Refer to PTR_Init() on why the surface module is opened dynamically

      if (!modSurface) {
         parasol::SwitchContext ctx(CurrentTask());
         if (LoadModule("surface", MODVERSION_SURFACE, &modSurface, &SurfaceBase) != ERR_Okay) {
            return ERR_InitModule;
         }
      }

      SURFACEINFO *info;
      if (!drwGetSurfaceInfo(Surface, &info)) {
         pointer->DragParent = info->ParentID;
      }

      if (Surface) {
         log.trace("Moving draggable surface %d to %dx%d", Surface, pointer->X, pointer->Y);
         acMoveToPointID(Surface, pointer->X+DRAG_XOFFSET, pointer->Y+DRAG_YOFFSET, 0, MTF_X|MTF_Y);
         acShowID(Surface);
         acMoveToFrontID(Surface);
      }

      gfxReleasePointer(pointer);
      return ERR_Okay;
   }
   else return log.warning(ERR_AccessObject);
}

/*****************************************************************************

-FUNCTION-
GetDisplayInfo: Retrieves display information.

The GetDisplayInfo() function returns information about a display, which includes information such as its size and bit
depth.  If the system is running on a hosted display (e.g. Windows or X11) then GetDisplayInfo() can also be used to
retrieve information about the default monitor by using a Display of zero.

The resulting DISPLAYINFO structure values remain good until the next call to this function, at which point they will
be overwritten.

-INPUT-
oid Display: Object ID of the display to be analysed.
&struct(*DisplayInfo) Info: This reference will receive a pointer to a DISPLAYINFO structure.

-ERRORS-
Okay:
NullArgs:
AllocMemory:

*****************************************************************************/

static ERROR gfxGetDisplayInfo(OBJECTID DisplayID, DISPLAYINFO **Result)
{
   static THREADVAR DISPLAYINFO *t_info = NULL;

   if (!Result) return ERR_NullArgs;

   if (!t_info) {
      // Each thread gets an allocation that can't be resource tracked, so MEM_HIDDEN is used in this case.
      // Note that this could conceivably cause memory leaks if temporary threads were to use this function.
      if (AllocMemory(sizeof(DISPLAYINFO), MEM_NO_CLEAR|MEM_HIDDEN, &t_info, NULL)) {
         return ERR_AllocMemory;
      }
   }

   ERROR error;
   if (!(error = get_display_info(DisplayID, t_info, sizeof(DISPLAYINFO)))) {
      *Result = t_info;
      return ERR_Okay;
   }
   else return error;
}

/*****************************************************************************

-FUNCTION-
GetDisplayType: Returns the type of display supported.

This function returns the type of display supported by the loaded Display module.  Current return values are:

<types lookup="DT"/>

-RESULT-
int(DT): Returns an integer indicating the display type.

*****************************************************************************/

static LONG gfxGetDisplayType(void)
{
#ifdef _WIN32
   return DT_WINDOWS;
#elif __xwindows__
   return DT_X11;
#elif _GLES_
   return DT_GLES;
#else
   return DT_NATIVE;
#endif
}

/*****************************************************************************

-FUNCTION-
AccessPointer: Returns a lock on the default pointer object.

Use AccessPointer() to grab a lock on the default pointer object that is active in the system.  This is typically the
first object created from the Pointer class with a name of "SystemPointer".

Call ~Core.ReleaseObject() to free the lock once it is no longer required.

-RESULT-
obj(Pointer): Returns the address of the default pointer object.

******************************************************************************/

static objPointer * gfxAccessPointer(void)
{
   objPointer *pointer;

   pointer = NULL;

   if (!glPointerID) {
      if (FastFindObject("SystemPointer", ID_POINTER, &glPointerID, 1, NULL) IS ERR_Okay) {
         AccessObject(glPointerID, 2000, &pointer);
      }
      return pointer;
   }

   if (AccessObject(glPointerID, 2000, &pointer) IS ERR_NoMatchingObject) {
      if (FastFindObject("SystemPointer", ID_POINTER, &glPointerID, 1, NULL) IS ERR_Okay) {
         AccessObject(glPointerID, 2000, &pointer);
      }
   }

   return pointer;
}

/******************************************************************************

-FUNCTION-
GetCursorInfo: Retrieves graphics information from the active mouse cursor.

The GetCursorInfo() function is used to retrieve useful information on the graphics structure of the mouse cursor.  It
will return the maximum possible dimensions for custom cursor graphics and indicates the optimal bits-per-pixel setting
for the hardware cursor.

If there is no cursor (e.g. this is likely on touch-screen devices) then all field values will be set to zero.

Note: If the hardware cursor is monochrome, the bits-per-pixel setting will be set to 2 on return.  This does not
indicate a 4 colour cursor image; rather colour 0 is the mask, 1 is the foreground colour (black), 2 is the background
colour (white) and 3 is an XOR pixel.  When creating the bitmap, always set the palette to the RGB values that are
wanted.  The mask colour for the bitmap must refer to colour index 0.

-INPUT-
struct(*CursorInfo) Info: Pointer to a CursorInfo structure.
structsize Size: The byte-size of the Info structure.

-ERRORS-
Okay:
NullArgs:
NoSupport: The device does not support a cursor (common for touch screen displays).

******************************************************************************/

static ERROR gfxGetCursorInfo(CursorInfo *Info, LONG Size)
{
   if (!Info) return ERR_NullArgs;

#ifdef __ANDROID__
   // TODO: Some Android devices probably do support a mouse or similar input device.
   ClearMemory(Info, sizeof(CursorInfo));
   return ERR_NoSupport;
#else
   Info->Width  = 32;
   Info->Height = 32;
   Info->BitsPerPixel = 1;
   Info->Flags = 0;
   return ERR_Okay;
#endif
}

/******************************************************************************

-FUNCTION-
GetCursorPos: Returns the coordinates of the UI pointer.

This function is used to retrieve the current coordinates of the user interface pointer.  If the device is touch-screen
based then the coordinates will reflect the last position that a touch event occurred.

-INPUT-
&int X: 32-bit variable that will store the pointer's horizontal coordinate.
&int Y: 32-bit variable that will store the pointer's vertical coordinate.

-ERRORS-
Okay
AccessObject: Failed to access the SystemPointer object.

******************************************************************************/

ERROR gfxGetCursorPos(LONG *X, LONG *Y)
{
   objPointer *pointer;

   if ((pointer = gfxAccessPointer())) {
      if (X) *X = pointer->X;
      if (Y) *Y = pointer->Y;
      ReleaseObject(pointer);
      return ERR_Okay;
   }
   else {
      parasol::Log log(__FUNCTION__);
      log.warning("Failed to grab the mouse pointer.");
      return ERR_Failed;
   }
}

/*****************************************************************************

-FUNCTION-
GetInputMsg: Read the next message delivered from the user input message queue.

This function reads messages from the input message queue.  It is designed to be used in conjunction with the
~Core.SubscribeInput() function and is called when InputReady messages are received through the subscriber's data
feed.  This function should be called using a loop to parse all the messages waiting on the input queue (refer to
the ~Core.SubscribeInput() function for an example).

All receivable messages are held in an InputMsg structure that is returned in the Msg parameter.  The structure format
is as follows:

<fields>
<fld type="UWORD" name="Type">This value is set to a JET constant that describes the input event.</>
<fld type="UWORD" name="Flags">Flags provide a broad description of the event type and can also provide more specific information relevant to the event (see JTYPE flags).</>
<fld type="DOUBLE" name="Value">The value associated with the Type</>
<fld type="OBJECTID" name="RecipientID">The surface that the input message is being conveyed to.</>
<fld type="OBJECTID" name="OverID">The surface that was directly under the mouse pointer at the time of the event.</>
<fld type="LONG" name="AbsX">Absolute horizontal coordinate of the mouse pointer (relative to the top left of the display).</>
<fld type="LONG" name="AbsY">Absolute vertical coordinate of the mouse pointer (relative to the top left of the display).</>
<fld type="LONG" name="OverX">Horizontal pointer coordinate, usually relative to the surface that the pointer is positioned over.  If a mouse button is held or the pointer is anchored, the coordinates are relative to the Recipient surface.</>
<fld type="LONG" name="OverY">Vertical pointer coordinate.</>
<fld type="LARGE" name="Timestamp">Millisecond counter at which the input was recorded, or as close to it as possible.</>
<fld type="OBJECTID" name="DeviceID">Reference to the hardware device that this event originated from.  There is no guarantee that the DeviceID is a reference to a publicly accessible object.</>
</>

JET constants are as follows, taking special note of ENTERED_SURFACE and LEFT_SURFACE, which are not driven by device input:

<types lookup="JET"/>

The JTYPE flags that can be set in the Flags field are as follows.  Note that these flags serve as input masks for the
~Core.SubscribeInput() function, so to receive a message of the given type the appropriate JTYPE flag must have
been set in the original subscription call.

<types lookup="JTYPE"/>

-INPUT-
struct(*dcInputReady) Input: Points to an input ready message as received through the DC_INPUTREADY data channel.
int(JTYPE) Flags: Input mask flags.
&struct(*InputMsg) Msg: Pointer to a InputMsg structural reference that will store the result.  GetInputMsg() will not modify the existing value stored in this parameter in the event of an error.

-ERRORS-
Okay: A message has been retrieved from the queue.
NullArgs:
OutOfRange:
AccessMemory: Failed to access the queue data.
Finished: There are no input messages left to read from the queue.

*****************************************************************************/

static ERROR gfxGetInputMsg(struct dcInputReady *Input, LONG Flags, struct InputMsg **Msg)
{
   parasol::Log log(__FUNCTION__);
   InputSubscription *list;
   LONG i, subindex;
   BYTE msgfound;

   if ((!Input) or (!Msg)) return ERR_NullArgs;
/*
   if (Flags & IMF_CONSOLIDATE) {
      ERROR error;
      error = gfxGetInputMsg(Input, 0, Msg);
      if (Msg[0].Flags &
      return(
   }
*/

   if (!glSharedControl->InputMID) return ERR_Finished;

   auto in = (struct dcDisplayInputReady *)Input;
   subindex = in->SubIndex;
   if ((subindex < 0) or (subindex >= glSharedControl->InputTotal)) return log.warning(ERR_OutOfRange);

   if (!AccessMemory(glSharedControl->InputMID, MEM_READ, 2000, &list)) {
      list[subindex].MsgSent = FALSE;

      if (in->NextIndex >= glInput->IndexCounter) {
         ReleaseMemory(list);
         return ERR_Finished;
      }

      //log.traceBranch("ID: " PF64() "/" PF64() ", Subscriber: %d", in->NextIndex, glInput->IndexCounter, list[subindex].SubscriberID);

      if (in->NextIndex < glInput->IndexCounter - MAX_INPUTMSG) {
         log.msg("Input messages have wrapped (subscriber %d unresponsive).", list[subindex].SubscriberID);
         in->NextIndex = glInput->IndexCounter - MAX_INPUTMSG + 1;
      }

      // Scan the message list until we either get a match or we run out of messages.


      msgfound = FALSE;
      while (in->NextIndex < glInput->IndexCounter) {
         i = in->NextIndex & INPUT_MASK;
         if ((list[subindex].Mask & glInput->Msgs[i].Mask) IS glInput->Msgs[i].Mask) {
            if ((!list[subindex].SurfaceID) or (list[subindex].SurfaceID IS glInput->Msgs[i].RecipientID)) {
               msgfound = TRUE;
               break;
            }
         }

         in->NextIndex++;
      }

      if (!msgfound) {
         // Subscriber is up to date with its messages
         in->NextIndex = glInput->IndexCounter;
         ReleaseMemory(list);
         return ERR_Finished;
      }

      if (in->NextIndex >= list[subindex].LastIndex) {
         // This is the last message in the queue for this subscriber.  Set the NextIndex
         // to IndexCounter and the next call to gfxGetInputMsg() will return ERR_Finished.

         in->NextIndex = glInput->IndexCounter;
      }
      else if (in->NextIndex < glInput->IndexCounter) in->NextIndex++;

      *Msg = glInput->Msgs + i;

      ReleaseMemory(list);
      return ERR_Okay;
   }
   else return log.warning(ERR_AccessMemory);
}

/******************************************************************************

-FUNCTION-
GetRelativeCursorPos: Returns the coordinates of the pointer cursor, relative to a surface object.

This function is used to retrieve the current coordinates of the pointer cursor. The coordinates are relative to the
surface object that is specified in the Surface argument.

The X and Y parameters will not be set if a failure occurs.

-INPUT-
oid Surface: Unique ID of the surface that the coordinates need to be relative to.
&int X: 32-bit variable that will store the pointer's horizontal coordinate.
&int Y: 32-bit variable that will store the pointer's vertical coordinate.

-ERRORS-
Okay:
AccessObject: Failed to access the SystemPointer object.

******************************************************************************/

static ERROR gfxGetRelativeCursorPos(OBJECTID SurfaceID, LONG *X, LONG *Y)
{
   parasol::Log log(__FUNCTION__);
   objPointer *pointer;
   LONG absx, absy;

   if (GetSurfaceAbs(SurfaceID, &absx, &absy, 0, 0) != ERR_Okay) {
      log.warning("Failed to get info for surface #%d.", SurfaceID);
      return ERR_Failed;
   }

   if ((pointer = gfxAccessPointer())) {
      if (X) *X = pointer->X - absx;
      if (Y) *Y = pointer->Y - absy;

      ReleaseObject(pointer);
      return ERR_Okay;
   }
   else {
      log.warning("Failed to grab the mouse pointer.");
      return ERR_AccessObject;
   }
}

/******************************************************************************

-FUNCTION-
LockCursor: Anchors the cursor so that it cannot move without explicit movement signals.

The LockCursor() function will lock the current pointer position and pass UserMovement signals to the surface
referenced in the Surface parameter.  The pointer will not move unless the ~SetCursorPos() function is called.
The anchor is granted on a time-limited basis.  It is necessary to reissue the anchor every time that a UserMovement
signal is intercepted.  Failure to reissue the anchor will return the pointer to its normal state, typically within 200
microseconds.

The anchor can be released at any time by calling the ~UnlockCursor() function.

-INPUT-
oid Surface: Refers to the surface object that the pointer should send movement signals to.

-ERRORS-
Okay
NullArgs
NoSupport: The pointer cannot be locked due to system limitations.
AccessObject: Failed to access the pointer object.

******************************************************************************/

static ERROR gfxLockCursor(OBJECTID SurfaceID)
{
#ifdef __snap__
   parasol::Log log(__FUNCTION__);
   objPointer *pointer;

   if (!SurfaceID) return log.warning(ERR_NullArgs);

   if ((pointer = gfxAccessPointer())) {
      // Return if the cursor is currently locked by someone else

      if ((pointer->AnchorID) and (pointer->AnchorID != SurfaceID)) {
         if (CheckObjectExists(pointer->AnchorID, NULL) != ERR_True);
         else if ((pointer->AnchorMsgQueue < 0) and (CheckMemoryExists(pointer->AnchorMsgQueue) != ERR_True));
         else {
            ReleaseObject(pointer);
            return ERR_LockFailed; // The pointer is locked by someone else
         }
      }

      pointer->AnchorID       = SurfaceID;
      pointer->AnchorMsgQueue = GetResource(RES_MESSAGE_QUEUE);
      pointer->AnchorTime     = PreciseTime() / 1000LL;

      ReleaseObject(pointer);
      return ERR_Okay;
   }
   else {
      log.warning("Failed to access the mouse pointer.");
      return ERR_AccessObject;
   }
#else
   return ERR_NoSupport;
#endif
}

/******************************************************************************

-FUNCTION-
RestoreCursor: Returns the pointer image to its original state.

Use the RestoreCursor() function to undo an earlier call to ~SetCursor().  It is necessary to provide the same OwnerID
that was used in the original call to ~SetCursor().

To release ownership of the cursor without changing the current cursor image, use a Cursor setting of PTR_NOCHANGE.

-INPUT-
int(PTR) Cursor: The cursor image that the pointer will be restored to (0 for the default).
oid Owner: The ownership ID that was given in the initial call to SetCursor().

-ERRORS-
Okay
Args
-END-

******************************************************************************/

static ERROR gfxRestoreCursor(LONG Cursor, OBJECTID OwnerID)
{
   parasol::Log log(__FUNCTION__);
   objPointer *pointer;

   if ((pointer = gfxAccessPointer())) {
/*
      OBJECTPTR caller;
      caller = CurrentContext();
      log.function("Cursor: %d, Owner: %d, Current-Owner: %d (Caller: %d / Class %d)", Cursor, OwnerID, pointer->CursorOwnerID, caller->UniqueID, caller->ClassID);
*/
      if ((!OwnerID) or (OwnerID IS pointer->CursorOwnerID)) {
         // Restore the pointer to the given cursor image
         if (!OwnerID) gfxSetCursor(NULL, CRF_RESTRICT, Cursor, NULL, pointer->CursorOwnerID);
         else gfxSetCursor(NULL, CRF_RESTRICT, Cursor, NULL, OwnerID);

         pointer->CursorOwnerID   = NULL;
         pointer->CursorRelease   = NULL;
         pointer->CursorReleaseID = NULL;
      }

      // If a cursor change has been buffered, enable it

      if (pointer->BufferOwner) {
         if (OwnerID != pointer->BufferOwner) {
            gfxSetCursor(pointer->BufferObject, pointer->BufferFlags, pointer->BufferCursor, NULL, pointer->BufferOwner);
         }
         else pointer->BufferOwner = NULL; // Owner and Buffer are identical, so clear due to restored pointer
      }

      ReleaseObject(pointer);
      return ERR_Okay;
   }
   else {
      log.warning("Failed to access the mouse pointer.");
      return ERR_AccessObject;
   }
}

/*****************************************************************************

-FUNCTION-
ScanDisplayModes: Private. Returns formatted resolution information from the display database.

For internal use only.

<pre>
DISPLAYINFO info;
ClearMemory(&info, sizeof(info));
while (!scrScanDisplayModes("depth=32", &info, sizeof(info))) {
   ...
}
</pre>

-INPUT-
cstr Filter: The filter to apply to the resolution database.  May be NULL for no filter.
struct(*DisplayInfo) Info: A pointer to a screenINFO structure must be referenced here.  The structure will be filled with information when the function returns.
structsize Size: Size of the screenINFO structure.

-ERRORS-
Okay: The resolution information was retrieved.
Args:
NoSupport: Native graphics system not available (e.g. hosted on Windows or X11).
Search: There are no more display modes to return that are a match for the Filter.

*****************************************************************************/

static ERROR gfxScanDisplayModes(CSTRING Filter, DISPLAYINFO *Info, LONG Size)
{
#ifdef __snap__

   GA_modeInfo modeinfo;
   UWORD *modes;
   LONG colours, bytes, i, j, minrefresh, maxrefresh, refresh, display;
   WORD f_depth, c_depth; // f = filter, c = condition (0 = equal; -1 <, -2 <=; +1 >, +2 >=)
   WORD f_bytes, c_bytes;
   WORD f_width, c_width, f_height, c_height;
   WORD f_refresh, c_refresh;
   WORD f_minrefresh, c_minrefresh;
   WORD f_maxrefresh, c_maxrefresh;
   BYTE interlace, matched;

   if ((!Info) or (Size < sizeof(DisplayInfoV3))) return ERR_Args;

   // Reset all filters

   f_depth   = c_depth   = 0;
   f_bytes   = c_bytes   = 0;
   f_width   = c_width   = 0;
   f_height  = c_height  = 0;
   f_refresh = c_refresh = 0;
   f_minrefresh = c_minrefresh = 0;
   f_maxrefresh = c_maxrefresh = 0;

   if (Filter) {
      while (*Filter) {
         while ((*Filter) and (*Filter <= 0x20)) Filter++;
         while (*Filter IS ',') Filter++;
         while ((*Filter) and (*Filter <= 0x20)) Filter++;

         if (!StrCompare("depth", Filter, 5, 0))   extract_value(Filter, &f_depth, &c_depth);
         if (!StrCompare("bytes", Filter, 5, 0))   extract_value(Filter, &f_bytes, &c_bytes);
         if (!StrCompare("width", Filter, 5, 0))   extract_value(Filter, &f_width, &c_width);
         if (!StrCompare("height", Filter, 6, 0))  extract_value(Filter, &f_height, &c_height);
         if (!StrCompare("refresh", Filter, 7, 0)) extract_value(Filter, &f_refresh, &c_refresh);
         if (!StrCompare("minrefresh", Filter, 10, 0)) extract_value(Filter, &f_minrefresh, &c_minrefresh);
         if (!StrCompare("maxrefresh", Filter, 10, 0)) extract_value(Filter, &f_maxrefresh, &c_maxrefresh);

         while ((*Filter) and (*Filter != ',')) Filter++;
      }
   }

   modes = glSNAPDevice->AvailableModes;
   display = glSNAP->Init.GetDisplayOutput() & gaOUTPUT_SELECTMASK;
   for (i=Info->Index; modes[i] != 0xffff; i++) {
      modeinfo.dwSize = sizeof(modeinfo);
      if (glSNAP->Init.GetVideoModeInfoExt(modes[i], &modeinfo, display, glSNAP->Init.GetActiveHead())) continue;

      if (modeinfo.AttributesExt & gaIsPanningMode) continue;
      if (modeinfo.Attributes & gaIsTextMode) continue;
      if (modeinfo.BitsPerPixel < 8) continue;

      if (modeinfo.BitsPerPixel <= 8) bytes = 1;
      else if (modeinfo.BitsPerPixel <= 16) bytes = 2;
      else if (modeinfo.BitsPerPixel <= 24) bytes = 3;
      else bytes = 4;

      if (modeinfo.BitsPerPixel <= 24) colours = 1<<modeinfo.BitsPerPixel;
      else colours = 1<<24;

      minrefresh = 0x7fffffff;
      maxrefresh = 0;
      for (j=0; modeinfo.RefreshRateList[j] != -1; j++) {
         if ((refresh = modeinfo.RefreshRateList[j]) < 0) refresh = -refresh;
         if (refresh > maxrefresh) maxrefresh = refresh;
         if (refresh < minrefresh) minrefresh = refresh;
      }

      if (minrefresh IS 0x7fffffff) minrefresh = 0;

      refresh = modeinfo.DefaultRefreshRate;
      if (refresh < 0) {
         refresh = -refresh;
         interlace = TRUE;
      }
      else interlace = TRUE;

      // Check if this mode meets the filters, if so then reduce the index

      if (Filter) {
         matched = TRUE;
         if ((f_depth)   and (!compare_values(f_depth,   c_depth,   modeinfo.BitsPerPixel))) matched = FALSE;
         if ((f_bytes)   and (!compare_values(f_bytes,   c_bytes,   bytes))) matched = FALSE;
         if ((f_width)   and (!compare_values(f_width,   c_width,   modeinfo.XResolution)))  matched = FALSE;
         if ((f_height)  and (!compare_values(f_height,  c_height,  modeinfo.YResolution)))  matched = FALSE;
         if ((f_refresh) and (!compare_values(f_refresh, c_refresh, modeinfo.BitsPerPixel))) matched = FALSE;
         if ((f_minrefresh) and (!compare_values(f_minrefresh, c_minrefresh, minrefresh)))   matched = FALSE;
         if ((f_maxrefresh) and (!compare_values(f_maxrefresh, c_maxrefresh, maxrefresh)))   matched = FALSE;

         if (matched IS FALSE) continue;
      }

      // Return information for this mode

      Info->Width         = modeinfo.XResolution;
      Info->Height        = modeinfo.YResolution;
      Info->Depth         = modeinfo.BitsPerPixel;
      Info->BytesPerPixel = bytes;
      Info->AmtColours    = colours;
      Info->MinRefresh    = minrefresh;
      Info->MaxRefresh    = maxrefresh;
      Info->RefreshRate   = refresh;
      Info->Index         = i + 1;
      return ERR_Okay;
   }

   return ERR_Search;

#else

   return ERR_NoSupport;

#endif
}

/******************************************************************************

-FUNCTION-
SetCursor: Sets the cursor image and can anchor the pointer to any surface.

Use the SetCursor() function to change the pointer image and/or restrict the movement of the pointer to a surface area.

To change the cursor image, set the Cursor or Name parameters to define the new image.  Valid cursor ID's and their
equivalent names are listed in the documentation for the Cursor field.  If the ObjectID field is set to a valid surface,
then the cursor image will switch back to the default setting once the pointer moves outside of its region.  If both
the Cursor and Name parameters are NULL, the cursor image will remain unchanged from its current image.

The SetCursor() function accepts the following flags in the Flags parameter:

<types lookup="CRF"/>

The Owner parameter is used as a locking mechanism to prevent the cursor from being changed whilst it is locked.  We
recommend that it is set to an object ID such as the program's task ID.  As the owner, the cursor remains under your
program's control until ~RestoreCursor() is called.

-INPUT-
oid Surface: Refers to the surface object that the pointer should anchor itself to, if the CRF_RESTRICT flag is used.  Otherwise, this parameter can be set to a surface that the new cursor image should be limited to.  The object referred to here must be publicly accessible to all tasks.
int(CRF) Flags:  Optional flags that affect the cursor.
int(PTR) Cursor: The ID of the cursor image that is to be set.
cstr Name: The name of the cursor image that is to be set (if Cursor is zero).
oid Owner: The object nominated as the owner of the anchor, and/or owner of the cursor image setting.

-ERRORS-
Okay
Args
NoSupport: The pointer cannot be set due to system limitations.
OutOfRange: The cursor ID is outside of acceptable range.
AccessObject: Failed to access the internally maintained image object.

******************************************************************************/

static ERROR gfxSetCursor(OBJECTID ObjectID, LONG Flags, LONG CursorID, CSTRING Name, OBJECTID OwnerID)
{
   parasol::Log log(__FUNCTION__);
   objPointer *pointer;
   LONG flags;

/*
   if (!OwnerID) {
      log.warning("An Owner must be provided to this function.");
      return ERR_Args;
   }
*/
   // Validate the cursor ID

   if ((CursorID < 0) or (CursorID >= PTR_END)) return log.warning(ERR_OutOfRange);

   if (!(pointer = gfxAccessPointer())) {
      log.warning("Failed to access the mouse pointer.");
      return ERR_AccessObject;
   }

   if (Name) log.traceBranch("Object: %d, Flags: $%.8x, Owner: %d (Current %d), Cursor: %s", ObjectID, Flags, OwnerID, pointer->CursorOwnerID, Name);
   else log.traceBranch("Object: %d, Flags: $%.8x, Owner: %d (Current %d), Cursor: %s", ObjectID, Flags, OwnerID, pointer->CursorOwnerID, CursorLookup[CursorID].Name);

   // Extract the cursor ID from the cursor name if no ID was given

   if (!CursorID) {
      if (Name) {
         for (LONG i=0; CursorLookup[i].Name; i++) {
            if (!StrMatch(CursorLookup[i].Name, Name)) {
               CursorID = CursorLookup[i].Value;
               break;
            }
         }
      }
      else CursorID = pointer->CursorID;
   }

   // Return if the cursor is currently pwn3d by someone

   if ((pointer->CursorOwnerID) and (pointer->CursorOwnerID != OwnerID)) {
      if ((pointer->CursorOwnerID < 0) and (CheckObjectExists(pointer->CursorOwnerID, NULL) != ERR_True)) pointer->CursorOwnerID = NULL;
      else if ((pointer->MessageQueue < 0) and (CheckMemoryExists(pointer->MessageQueue) != ERR_True)) pointer->CursorOwnerID = NULL;
      else if (Flags & CRF_BUFFER) {
         // If the BUFFER option is used, then we can buffer the change so that it
         // will be activated as soon as the current holder releases the cursor.

         log.extmsg("Request buffered, pointer owned by #%d.", pointer->CursorOwnerID);

         pointer->BufferCursor = CursorID;
         pointer->BufferOwner  = OwnerID;
         pointer->BufferFlags  = Flags;
         pointer->BufferObject = ObjectID;
         pointer->BufferQueue  = GetResource(RES_MESSAGE_QUEUE);
         ReleaseObject(pointer);
         return ERR_Okay;
      }
      else {
         ReleaseObject(pointer);
         return ERR_LockFailed; // The pointer is locked by someone else
      }
   }

   log.trace("Anchor: %d, Owner: %d, Release: $%x, Cursor: %d", ObjectID, OwnerID, Flags, CursorID);

   // If CRF_NOBUTTONS is used, the cursor can only be set if no mouse buttons are held down at the current time.

   if (Flags & CRF_NO_BUTTONS) {
      if ((pointer->Buttons[0].LastClicked) or (pointer->Buttons[1].LastClicked) or (pointer->Buttons[2].LastClicked)) {
         ReleaseObject(pointer);
         return ERR_NothingDone;
      }
   }

   // Reset restrictions/anchoring if the correct flags are set, or if the cursor is having a change of ownership.

   if ((Flags & CRF_RESTRICT) or (OwnerID != pointer->CursorOwnerID)) pointer->RestrictID = NULL;

   if (OwnerID IS pointer->BufferOwner) pointer->BufferOwner = NULL;

   pointer->CursorReleaseID = 0;
   pointer->CursorOwnerID   = 0;
   pointer->CursorRelease   = NULL;
   pointer->MessageQueue    = NULL;

   if (CursorID) {
      if ((CursorID IS pointer->CursorID) and (CursorID != PTR_CUSTOM)) {
         // Do nothing
      }
      else {
         // Use this routine if our cursor is hardware based

         log.trace("Adjusting hardware/hosted cursor image.");

         #ifdef __xwindows__

            APTR xwin;
            objSurface *surface;
            objDisplay *display;
            Cursor xcursor;

            if ((pointer->SurfaceID) and (!AccessObject(pointer->SurfaceID, 1000, &surface))) {
               if ((surface->DisplayID) and (!AccessObject(surface->DisplayID, 1000, &display))) {
                  if ((GetPointer(display, FID_WindowHandle, &xwin) IS ERR_Okay) and (xwin)) {
                     xcursor = get_x11_cursor(CursorID);
                     XDefineCursor(XDisplay, (Window)xwin, xcursor);
                     XFlush(XDisplay);
                     pointer->CursorID = CursorID;
                  }
                  else log.warning("Failed to acquire window handle for surface #%d.", pointer->SurfaceID);
                  ReleaseObject(display);
               }
               else log.warning("Display of surface #%d undefined or inaccessible.", pointer->SurfaceID);
               ReleaseObject(surface);
            }
            else log.warning("Pointer surface undefined or inaccessible.");

         #elif _WIN32

            if (pointer->Head.TaskID IS CurrentTask()->UniqueID) {
               winSetCursor(GetWinCursor(CursorID));
               pointer->CursorID = CursorID;
            }
            else {
               struct ptrSetWinCursor set;
               set.Cursor = CursorID;
               DelayMsg(MT_PtrSetWinCursor, pointer->Head.UniqueID, &set);
            }

         #endif
      }

      if ((ObjectID < 0) and (GetClassID(ObjectID) IS ID_SURFACE) and (!(Flags & CRF_RESTRICT))) {
         pointer->CursorReleaseID = ObjectID; // Release the cursor image if it goes outside of the given surface object
      }
   }

   pointer->CursorOwnerID = OwnerID;

   // Manage button release flag options (useful when the RESTRICT or ANCHOR options are used).

   flags = Flags;
   if (flags & (CRF_LMB|CRF_MMB|CRF_RMB)) {
      if (flags & CRF_LMB) {
         if (pointer->Buttons[0].LastClicked) pointer->CursorRelease |= 0x01;
         else flags &= ~(CRF_RESTRICT); // The LMB has already been released by the user, so do not allow restrict/anchoring
      }
      else if (flags & CRF_RMB) {
         if (pointer->Buttons[1].LastClicked) pointer->CursorRelease |= 0x02;
         else flags &= ~(CRF_RESTRICT); // The MMB has already been released by the user, so do not allow restrict/anchoring
      }
      else if (flags & CRF_MMB) {
         if (pointer->Buttons[2].LastClicked) pointer->CursorRelease |= 0x04;
         else flags &= ~(CRF_RESTRICT); // The MMB has already been released by the user, so do not allow restrict/anchoring
      }
   }

   if ((flags & CRF_RESTRICT) and (ObjectID)) {
      if ((ObjectID < 0) and (GetClassID(ObjectID) IS ID_SURFACE)) { // Must be a public surface object
         // Restrict the pointer to the specified surface
         pointer->RestrictID = ObjectID;

         #ifdef __xwindows__
            // Pointer grabbing has been turned off for X11 because LBreakout2 was not receiving
            // movement events when run from the desktop.  The reason for this
            // is that only the desktop (which does the X11 input handling) is allowed
            // to grab the pointer.

            //DelayMsg(MT_GrabX11Pointer, pointer->Head.UniqueID, NULL);
         #endif
      }
      else log.warning("The pointer may only be restricted to public surfaces.");
   }

   pointer->MessageQueue = GetResource(RES_MESSAGE_QUEUE);

   ReleaseObject(pointer);
   return ERR_Okay;
}

/******************************************************************************

-FUNCTION-
SetCustomCursor: Sets the cursor to a customised bitmap image.

Use the SetCustomCursor() function to change the pointer image and/or anchor the position of the pointer so that it
cannot move without permission.  The functionality provided is identical to that of the SetCursor() function with some
minor adjustments to allow custom images to be set.

The Bitmap that is provided should be within the width, height and bits-per-pixel settings that are returned by the
GetCursorInfo() function.  If the basic settings are outside the allowable parameters, the Bitmap will be trimmed or
resampled appropriately when the cursor is downloaded to the video card.

It may be possible to speed up the creation of custom cursors by drawing directly to the pointer's internal bitmap
buffer rather than supplying a fresh bitmap.  To do this, the Bitmap parameter must be NULL and it is necessary to draw
to the pointer's bitmap before calling SetCustomCursor().  Note that the bitmap is always returned as a 32-bit,
alpha-enabled graphics area.  The following code illustrates this process:

<pre>
objPointer *pointer;
objBitmap *bitmap;

if ((pointer = gfxAccessPointer())) {
   if (!AccessObject(pointer->BitmapID, 3000, &bitmap)) {
      // Adjust clipping to match the cursor size.
      buffer->Clip.Right  = CursorWidth;
      buffer->Clip.Bottom = CursorHeight;
      if (buffer->Clip.Right > buffer->Width) buffer->Clip.Right = buffer->Width;
      if (buffer->Clip.Bottom > buffer->Height) buffer->Clip.Bottom = buffer->Height;

      // Draw to the bitmap here.
      ...

      gfxSetCustomCursor(ObjectID, NULL, NULL, 1, 1, glTaskID, NULL);
      ReleaseObject(bitmap);
   }
   gfxReleasePointer(pointer);
}
</pre>

-INPUT-
oid Surface: Refers to the surface object that the pointer should restrict itself to, if the CRF_RESTRICT flag is used.  Otherwise, this parameter can be set to a surface that the new cursor image should be limited to.  The object referred to here must be publicly accessible to all tasks.
int(CRF) Flags: Optional flags affecting the cursor are set here.
obj(Bitmap) Bitmap: The bitmap to set for the mouse cursor.
int HotX: The horizontal position of the cursor hot-spot.
int HotY: The vertical position of the cursor hot-spot.
oid Owner: The object nominated as the owner of the anchor.

-ERRORS-
Okay:
Args:
NoSupport:
AccessObject: Failed to access the internally maintained image object.

******************************************************************************/

static ERROR gfxSetCustomCursor(OBJECTID ObjectID, LONG Flags, objBitmap *Bitmap, LONG HotX, LONG HotY, OBJECTID OwnerID)
{
#ifdef __snap__
   parasol::Log log(__FUNCTION__);
   objPointer *pointer;
   objBitmap *buffer;
   ERROR error;

   if (Bitmap) log.extmsg("Object: %d, Bitmap: %p, Size: %dx%d, BPP: %d", ObjectID, Bitmap, Bitmap->Width, Bitmap->Height, Bitmap->BitsPerPixel);
   else log.extmsg("Object: %d, Bitmap Preset", ObjectID);

   if ((pointer = gfxAccessPointer())) {
      if (!AccessObject(pointer->BitmapID, 0, &buffer)) {
         if (Bitmap) {
            // Adjust the clipping area of our custom bitmap to match the incoming dimensions of the new cursor image.

            buffer->Clip.Right = Bitmap->Width;
            buffer->Clip.Bottom = Bitmap->Height;
            if (buffer->Clip.Right > buffer->Width) buffer->Clip.Right = buffer->Width;
            if (buffer->Clip.Bottom > buffer->Height) buffer->Clip.Bottom = buffer->Height;

            if (Bitmap->BitsPerPixel IS 2) {
               ULONG mask;

               // Monochrome: 0 = mask, 1 = black (fg), 2 = white (bg), 3 = XOR

               if (buffer->Flags & BMF_INVERSEALPHA) mask = PackPixelA(buffer, 0, 0, 0, 255);
               else mask = PackPixelA(buffer, 0, 0, 0, 0);

               ULONG foreground = PackPixel(buffer, Bitmap->Palette->Col[1].Red, Bitmap->Palette->Col[1].Green, Bitmap->Palette->Col[1].Blue);
               ULONG background = PackPixel(buffer, Bitmap->Palette->Col[2].Red, Bitmap->Palette->Col[2].Green, Bitmap->Palette->Col[2].Blue);
               for (LONG y=0; y < Bitmap->Clip.Bottom; y++) {
                  for (LONG x=0; x < Bitmap->Clip.Right; x++) {
                     switch (Bitmap->ReadUCPixel(Bitmap, x, y)) {
                        case 0: buffer->DrawUCPixel(buffer, x, y, mask); break;
                        case 1: buffer->DrawUCPixel(buffer, x, y, foreground); break;
                        case 2: buffer->DrawUCPixel(buffer, x, y, background); break;
                        case 3: buffer->DrawUCPixel(buffer, x, y, foreground); break;
                     }
                  }
               }
            }
            else mtCopyArea(Bitmap, buffer, NULL, 0, 0, Bitmap->Width, Bitmap->Height, 0, 0);
         }

         pointer->Cursors[PTR_CUSTOM].HotX = HotX;
         pointer->Cursors[PTR_CUSTOM].HotY = HotY;
         error = gfxSetCursor(ObjectID, Flags, PTR_CUSTOM, NULL, OwnerID);
         ReleaseObject(buffer);
      }
      else error = ERR_AccessObject;

      ReleaseObject(pointer);
      return error;
   }
   else {
      log.warning("Failed to access the mouse pointer.");
      return ERR_AccessObject;
   }
#else
   return gfxSetCursor(ObjectID, Flags, PTR_DEFAULT, NULL, OwnerID);
#endif
}

/******************************************************************************

-FUNCTION-
SetCursorPos: Changes the position of the pointer cursor.

Changes the position of the pointer cursor using coordinates relative to the entire display.

-INPUT-
int X: The new horizontal coordinate for the pointer.
int Y: The new vertical coordinate for the pointer.

-ERRORS-
Okay:
AccessObject: Failed to access the SystemPointer object.

******************************************************************************/

static ERROR gfxSetCursorPos(LONG X, LONG Y)
{
   objPointer *pointer;

   struct acMoveToPoint move = { (DOUBLE)X, (DOUBLE)Y, 0, MTF_X|MTF_Y };
   if ((pointer = gfxAccessPointer())) {
      Action(AC_MoveToPoint, pointer, &move);
      ReleaseObject(pointer);
   }
   else ActionMsg(AC_MoveToPoint, glPointerID, &move);

   return ERR_Okay;
}

/*****************************************************************************

-FUNCTION-
SetHostOption: Alter options associated with the host display system.

For internal usage only.

-INPUT-
int(HOST) Option: One of HOST_TRAY_ICON, HOST_TASKBAR or HOST_STICK_TO_FRONT.
large Value: The value to be applied to the option.

-ERRORS-
Okay

*****************************************************************************/

static ERROR gfxSetHostOption(LONG Option, LARGE Value)
{
#if defined(_WIN32) || defined(__xwindows__)
   parasol::Log log(__FUNCTION__);

   switch (Option) {
      case HOST_TRAY_ICON:
         glTrayIcon += Value;
         if (glTrayIcon) glTaskBar = 0;
         break;

      case HOST_TASKBAR:
         glTaskBar = Value;
         if (glTaskBar) glTrayIcon = 0;
         break;

      case HOST_STICK_TO_FRONT:
         glStickToFront += Value;
         break;

      default:
         log.warning("Invalid option %d, Data " PF64(), Option, Value);
   }
#endif

   return ERR_Okay;
}

/******************************************************************************

-FUNCTION-
UnlockCursor: Undoes an earlier call to LockCursor()

Call this function to undo any earlier calls to LockCursor() and return the mouse pointer to its regular state.

-INPUT-
oid Surface: Refers to the surface object used for calling LockCursor().

-ERRORS-
Okay:
NullArgs:
AccessObject: Failed to access the pointer object.
NotLocked: A lock is not present, or the lock belongs to another surface.
-END-

******************************************************************************/

static ERROR gfxUnlockCursor(OBJECTID SurfaceID)
{
   parasol::Log log(__FUNCTION__);

   if (!SurfaceID) return log.warning(ERR_NullArgs);

   objPointer *pointer;
   if ((pointer = gfxAccessPointer())) {
      if (pointer->AnchorID IS SurfaceID) {
         pointer->AnchorID = NULL;
         pointer->AnchorMsgQueue = NULL;
         ReleaseObject(pointer);
         return ERR_Okay;
      }
      else {
         ReleaseObject(pointer);
         return ERR_NotLocked;
      }
   }
   else {
      log.warning("Failed to access the mouse pointer.");
      return ERR_AccessObject;
   }
}

//****************************************************************************

#ifdef __xwindows__
static Cursor create_blank_cursor(void)
{
   parasol::Log log(__FUNCTION__);
   Pixmap data_pixmap, mask_pixmap;
   XColor black = { 0, 0, 0, 0 };
   Window rootwindow;
   Cursor cursor;

   log.function("Creating blank cursor for X11.");

   rootwindow = DefaultRootWindow(XDisplay);

   data_pixmap = XCreatePixmap(XDisplay, rootwindow, 1, 1, 1);
   mask_pixmap = XCreatePixmap(XDisplay, rootwindow, 1, 1, 1);

   //XSetWindowBackground(XDisplay, data_pixmap, 0);
   //XSetWindowBackground(XDisplay, mask_pixmap, 0);
   //XClearArea(XDisplay, data_pixmap, 0, 0, 1, 1, False);
   //XClearArea(XDisplay, mask_pixmap, 0, 0, 1, 1, False);

   cursor = XCreatePixmapCursor(XDisplay, data_pixmap, mask_pixmap, &black, &black, 0, 0);

   XFreePixmap(XDisplay, data_pixmap); // According to XFree documentation, it is OK to free the pixmaps
   XFreePixmap(XDisplay, mask_pixmap);

   XSync(XDisplay, False);
   return cursor;
}
#endif

//*****************************************************************************

#ifdef __xwindows__

static Cursor get_x11_cursor(LONG CursorID)
{
   parasol::Log log(__FUNCTION__);

   for (WORD i=0; i < ARRAYSIZE(XCursors); i++) {
      if (XCursors[i].CursorID IS CursorID) return XCursors[i].XCursor;
   }

   log.warning("Cursor #%d is not a recognised cursor ID.", CursorID);
   return XCursors[0].XCursor;
}
#endif

#ifdef _WIN32

static APTR GetWinCursor(LONG CursorID)
{
   for (WORD i=0; i < ARRAYSIZE(winCursors); i++) {
      if (winCursors[i].CursorID IS CursorID) return winCursors[i].WinCursor;
   }

   parasol::Log log;
   log.warning("Cursor #%d is not a recognised cursor ID.", CursorID);
   return winCursors[0].WinCursor;
}
#endif

//*****************************************************************************

static void update_displayinfo(objDisplay *Self)
{
   if (StrMatch("SystemDisplay", GetName(Self)) != ERR_Okay) return;

   glDisplayInfo->DisplayID = 0;
   get_display_info(Self->Head.UniqueID, glDisplayInfo, sizeof(DISPLAYINFO));
}

/*****************************************************************************
** Surface locking routines.  These should only be called on occasions where you need to use the CPU to access graphics
** memory.  These functions are internal, if the user wants to lock a bitmap surface then the Lock() action must be
** called on the bitmap.
**
** Please note: Regarding SURFACE_READ, using this flag will cause the video content to be copied to the bitmap buffer.
** If you do not need this overhead because the bitmap content is going to be refreshed, then specify SURFACE_WRITE
** only.  You will still be able to read the bitmap content with the CPU, it just avoids the copy overhead.
*/

#ifdef _WIN32

static ERROR LockSurface(objBitmap *Bitmap, WORD Access)
{
   if (!Bitmap->Data) {
      parasol::Log log(__FUNCTION__);
      log.warning("[Bitmap:%d] Bitmap is missing the Data field.", Bitmap->Head.UniqueID);
      return ERR_FieldNotSet;
   }

   return ERR_Okay;
}

static ERROR UnlockSurface(objBitmap *Bitmap)
{
   return ERR_Okay;
}

#elif __xwindows__

static ERROR LockSurface(objBitmap *Bitmap, WORD Access)
{
   LONG size;
   WORD alignment;

   if ((Bitmap->Flags & BMF_X11_DGA) and (glDGAAvailable)) {
      return ERR_Okay;
   }
   else if ((Bitmap->x11.drawable) and (Access & SURFACE_READ)) {
      // If there is an existing readable area, try to reuse it if possible
      if (Bitmap->x11.readable) {
         if ((Bitmap->x11.readable->width >= Bitmap->Width) and (Bitmap->x11.readable->height >= Bitmap->Height)) {
            if (Access & SURFACE_READ) {
               XGetSubImage(XDisplay, Bitmap->x11.drawable, Bitmap->XOffset + Bitmap->Clip.Left,
                  Bitmap->YOffset + Bitmap->Clip.Top, Bitmap->Clip.Right - Bitmap->Clip.Left,
                  Bitmap->Clip.Bottom - Bitmap->Clip.Top, 0xffffffff, ZPixmap, Bitmap->x11.readable,
                  Bitmap->XOffset + Bitmap->Clip.Left, Bitmap->YOffset + Bitmap->Clip.Top);
            }
            return ERR_Okay;
         }
         else XDestroyImage(Bitmap->x11.readable);
      }

      // Generate a fresh XImage from the current drawable

      if (Bitmap->LineWidth & 0x0001) alignment = 8;
      else if (Bitmap->LineWidth & 0x0002) alignment = 16;
      else alignment = 32;

      if (Bitmap->Type IS BMP_PLANAR) {
         size = Bitmap->LineWidth * Bitmap->Height * Bitmap->BitsPerPixel;
      }
      else size = Bitmap->LineWidth * Bitmap->Height;

      Bitmap->Data = (UBYTE *)malloc(size);

      if ((Bitmap->x11.readable = XCreateImage(XDisplay, CopyFromParent, Bitmap->BitsPerPixel,
           ZPixmap, 0, (char *)Bitmap->Data, Bitmap->Width, Bitmap->Height, alignment, Bitmap->LineWidth))) {
         if (Access & SURFACE_READ) {
            XGetSubImage(XDisplay, Bitmap->x11.drawable, Bitmap->XOffset + Bitmap->Clip.Left,
               Bitmap->YOffset + Bitmap->Clip.Top, Bitmap->Clip.Right - Bitmap->Clip.Left,
               Bitmap->Clip.Bottom - Bitmap->Clip.Top, 0xffffffff, ZPixmap, Bitmap->x11.readable,
               Bitmap->XOffset + Bitmap->Clip.Left, Bitmap->YOffset + Bitmap->Clip.Top);
         }
         return ERR_Okay;
      }
      else return ERR_Failed;
   }
   return ERR_Okay;
}

ERROR UnlockSurface(objBitmap *Bitmap)
{
   return ERR_Okay;
}

#elif _GLES_

static ERROR LockSurface(objBitmap *Bitmap, WORD Access)
{
   parasol::Log log(__FUNCTION__);

   if (Bitmap->DataFlags & MEM_VIDEO) {
      // MEM_VIDEO represents the video display in OpenGL.  Read/write CPU access is not available to this area but
      // we can use glReadPixels() to get a copy of the framebuffer and then write changes back.  Because this is
      // extremely bad practice (slow), a debug message is printed to warn the developer to use a different code path.
      //
      // Practically the only reason why we allow this is for unusual measures like taking screenshots, grabbing the display for debugging, development testing etc.

      log.warning("Warning: Locking of OpenGL video surfaces for CPU access is bad practice (bitmap: #%d, mem: $%.8x)", Bitmap->Head.UniqueID, Bitmap->DataFlags);

      if (!Bitmap->Data) {
         if (AllocMemory(Bitmap->Size, MEM_NO_BLOCKING|MEM_NO_POOL|MEM_NO_CLEAR|Bitmap->Head.MemFlags|Bitmap->DataFlags, &Bitmap->Data, &Bitmap->DataMID) != ERR_Okay) {
            return log.warning(ERR_AllocMemory);
         }
         Bitmap->prvAFlags |= BF_DATA;
      }

      if (!lock_graphics_active(__func__)) {
         if (Access & SURFACE_READ) {
            //glPixelStorei(GL_PACK_ALIGNMENT, 1); Might be required if width is not 32-bit aligned (i.e. 16 bit uneven width?)
            glReadPixels(0, 0, Bitmap->Width, Bitmap->Height, Bitmap->prvGLPixel, Bitmap->prvGLFormat, Bitmap->Data);
         }

         if (Access & SURFACE_WRITE) Bitmap->prvWriteBackBuffer = TRUE;
         else Bitmap->prvWriteBackBuffer = FALSE;

         unlock_graphics();
      }

      return ERR_Okay;
   }
   else if (Bitmap->DataFlags & MEM_TEXTURE) {
      // Using the CPU on BLIT bitmaps is banned - it is considered to be poor programming.  Instead,
      // MEM_DATA bitmaps should be used when R/W CPU access is desired to a bitmap.

      return log.warning(ERR_NoSupport);
   }

   if (!Bitmap->Data) {
      log.warning("[Bitmap:%d] Bitmap is missing the Data field.  Memory flags: $%.8x", Bitmap->Head.UniqueID, Bitmap->DataFlags);
      return ERR_FieldNotSet;
   }

   return ERR_Okay;
}

static ERROR UnlockSurface(objBitmap *Bitmap)
{
   if ((Bitmap->DataFlags & MEM_VIDEO) and (Bitmap->prvWriteBackBuffer)) {
      if (!lock_graphics_active(__func__)) {
         #ifdef GL_DRAW_PIXELS
            glDrawPixels(Bitmap->Width, Bitmap->Height, pixel_type, format, Bitmap->Data);
         #else
            GLenum glerror;
            GLuint texture_id;
            if ((glerror = alloc_texture(Bitmap->Width, Bitmap->Height, &texture_id)) IS GL_NO_ERROR) { // Create a new texture space and bind it.
               //(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid *pixels);
               glTexImage2D(GL_TEXTURE_2D, 0, Bitmap->prvGLPixel, Bitmap->Width, Bitmap->Height, 0, Bitmap->prvGLPixel, Bitmap->prvGLFormat, Bitmap->Data); // Copy the bitmap content to the texture. (Target, Level, Bitmap, Border)
               if ((glerror = glGetError()) IS GL_NO_ERROR) {
                  // Copy graphics to the frame buffer.

                  glClearColor(0, 0, 0, 1.0);
                  glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
                  glColor4f(1.0f, 1.0f, 1.0f, 1.0f);    // Ensure colour is reset.
                  glDrawTexiOES(0, 0, 1, Bitmap->Width, Bitmap->Height);
                  glBindTexture(GL_TEXTURE_2D, 0);
                  eglSwapBuffers(glEGLDisplay, glEGLSurface);
               }
               else log.warning(ERR_OpenGL);

               glDeleteTextures(1, &texture_id);
            }
            else log.warning(ERR_OpenGL);
         #endif

         unlock_graphics();
      }

      Bitmap->prvWriteBackBuffer = FALSE;
   }

   return ERR_Okay;
}

#else

#error Platform not supported.

#define LockSurface(a,b)
#define UnlockSurface(a)

#endif

/*****************************************************************************
** Use this function to allocate simple 2D OpenGL textures.  It configures the texture so that it is suitable for basic
** rendering operations.  Note that the texture will still be bound on returning.
*/

#ifdef _GLES_
static GLenum alloc_texture(LONG Width, LONG Height, GLuint *TextureID)
{
   GLenum glerror;

   glGenTextures(1, TextureID); // Generate a new texture ID
   glBindTexture(GL_TEXTURE_2D, TextureID[0]); // Target the new texture bank

   glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); // Filter for minification, GL_LINEAR is smoother than GL_NEAREST
   glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR); // Filter for magnification
   glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); // Texture wrap behaviour
   glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE); // Texture wrap behaviour

   glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

   if ((glerror = glGetError()) IS GL_NO_ERROR) {
      GLint crop[4] = { 0, Height, Width, -Height };
      glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_CROP_RECT_OES, crop); // This is for glDrawTex*OES

      glerror = glGetError();
      if (glerror != GL_NO_ERROR) log.warning("glTexParameteriv() error: %d", glerror);
   }
   else log.warning("glTexEnvf() error: %d", glerror);

   return glerror;
}
#endif

/*****************************************************************************

-FUNCTION-
CopyArea: Copies a rectangular area from one bitmap to another.

This function copies rectangular areas from one bitmap to another.  It performs a straight region-copy only, using the
fastest method available.  Bitmaps may be of a different type (e.g. bit depth), however this will result in performance
penalties.  The copy process will respect the clipping region defined in both the source and destination bitmap
objects.

If the TRANSPARENT flag is set in the source object, all colours that match the ColourIndex field will be ignored in
the copy operation.

To enable dithering, pass BAF_DITHER in the Flags argument.  The drawing algorithm will use dithering if the source
needs to be down-sampled to the target bitmap's bit depth.  To enable alpha blending, set BAF_BLEND (the source bitmap
will also need to have the BMF_ALPHA_CHANNEL flag set to indicate that an alpha channel is available).

-INPUT-
obj(Bitmap) Bitmap: The source bitmap.
obj(Bitmap) Dest: Pointer to the destination bitmap.
int(BAF) Flags: Special flags.
int X:      The horizontal position of the area to be copied.
int Y:      The vertical position of the area to be copied.
int Width:  The width of the area.
int Height: The height of the area.
int XDest:  The horizontal position to copy the area to.
int YDest:  The vertical position to copy the area to.

-ERRORS-
Okay:
NullArgs: The DestBitmap argument was not specified.
Mismatch: The destination bitmap is not a close enough match to the source bitmap in order to perform the blit.

*****************************************************************************/

static UBYTE validate_clip(CSTRING Header, CSTRING Name, objBitmap *Bitmap)
{
   parasol::Log log(Header);

#ifdef DEBUG // Force crash if clipping is wrong (use gdb)
   if ((Bitmap->XOffset + Bitmap->Clip.Right) > Bitmap->Width) ((UBYTE *)0)[0] = 0;
   if ((Bitmap->YOffset + Bitmap->Clip.Bottom) > Bitmap->Height) ((UBYTE *)0)[0] = 0;
   if ((Bitmap->XOffset + Bitmap->Clip.Left) < 0) ((UBYTE *)0)[0] = 0;
   if (Bitmap->Clip.Left >= Bitmap->Clip.Right) ((UBYTE *)0)[0] = 0;
   if (Bitmap->Clip.Top >= Bitmap->Clip.Bottom) ((UBYTE *)0)[0] = 0;
#else
   if ((Bitmap->XOffset + Bitmap->Clip.Right) > Bitmap->Width) {
      log.warning("#%d %s: Invalid right-clip of %d (offset %d), limited to width of %d.", Bitmap->Head.UniqueID, Name, Bitmap->Clip.Right, Bitmap->XOffset, Bitmap->Width);
      Bitmap->Clip.Right = Bitmap->Width - Bitmap->XOffset;
   }

   if ((Bitmap->YOffset + Bitmap->Clip.Bottom) > Bitmap->Height) {
      log.warning("#%d %s: Invalid bottom-clip of %d (offset %d), limited to height of %d.", Bitmap->Head.UniqueID, Name, Bitmap->Clip.Bottom, Bitmap->YOffset, Bitmap->Height);
      Bitmap->Clip.Bottom = Bitmap->Height - Bitmap->YOffset;
   }

   if ((Bitmap->XOffset + Bitmap->Clip.Left) < 0) {
      log.warning("#%d %s: Invalid left-clip of %d (offset %d).", Bitmap->Head.UniqueID, Name, Bitmap->Clip.Left, Bitmap->XOffset);
      Bitmap->XOffset = 0;
      Bitmap->Clip.Left = 0;
   }

   if ((Bitmap->YOffset + Bitmap->Clip.Top) < 0) {
      log.warning("#%d %s: Invalid top-clip of %d (offset %d).", Bitmap->Head.UniqueID, Name, Bitmap->Clip.Top, Bitmap->YOffset);
      Bitmap->YOffset = 0;
      Bitmap->Clip.Top = 0;
   }

   if (Bitmap->Clip.Left >= Bitmap->Clip.Right) {
      log.warning("#%d %s: Left clip >= Right clip (%d >= %d)", Bitmap->Head.UniqueID, Name, Bitmap->Clip.Left, Bitmap->Clip.Right);
      return 1;
   }

   if (Bitmap->Clip.Top >= Bitmap->Clip.Bottom) {
      log.warning("#%d %s: Top clip >= Bottom clip (%d >= %d)", Bitmap->Head.UniqueID, Name, Bitmap->Clip.Top, Bitmap->Clip.Bottom);
      return 1;
   }
#endif

   return 0;
}

static ERROR gfxCopyArea(objBitmap *Bitmap, objBitmap *dest, LONG Flags, LONG X, LONG Y, LONG Width, LONG Height, LONG DestX, LONG DestY)
{
   parasol::Log log(__FUNCTION__);
   RGB8 pixel, src;
   UBYTE *srctable, *desttable;
   LONG i;
   ULONG colour;
   UBYTE *data, *srcdata;

   if (!dest) return ERR_NullArgs;
   if (dest->Head.ClassID != ID_BITMAP) {
      log.warning("Destination #%d is not a Bitmap.", dest->Head.UniqueID);
      return ERR_InvalidObject;
   }

   if (!(Bitmap->Head.Flags & NF_INITIALISED)) return log.warning(ERR_NotInitialised);

   //log.trace("%dx%d,%dx%d to %dx%d, Offset: %dx%d to %dx%d", X, Y, Width, Height, DestX, DestY, Bitmap->XOffset, Bitmap->YOffset, dest->XOffset, dest->YOffset);

   if (validate_clip(__FUNCTION__, "Source", Bitmap)) return ERR_Okay;

   if (Bitmap != dest) { // Validate the clipping region of the destination
      if (validate_clip(__FUNCTION__, "Dest", dest)) return ERR_Okay;
   }

   if (Bitmap IS dest) { // Use this clipping routine only if we are copying within the same bitmap
      if (X < Bitmap->Clip.Left) {
         Width = Width - (Bitmap->Clip.Left - X);
         DestX = DestX + (Bitmap->Clip.Left - X);
         X = Bitmap->Clip.Left;
      }
      else if (X >= Bitmap->Clip.Right) {
         log.trace("Clipped: X >= Bitmap->ClipRight (%d >= %d)", X, Bitmap->Clip.Right);
         return ERR_Okay;
      }

      if (Y < Bitmap->Clip.Top) {
         Height = Height - (Bitmap->Clip.Top - Y);
         DestY  = DestY + (Bitmap->Clip.Top - Y);
         Y = Bitmap->Clip.Top;
      }
      else if (Y >= Bitmap->Clip.Bottom) {
         log.trace("Clipped: Y >= Bitmap->ClipBottom (%d >= %d)", Y, Bitmap->Clip.Bottom);
         return ERR_Okay;
      }

      // Clip the destination coordinates

      if ((DestX < dest->Clip.Left)) {
         Width = Width - (dest->Clip.Left - DestX);
         if (Width < 1) return ERR_Okay;
         X = X + (dest->Clip.Left - DestX);
         DestX = dest->Clip.Left;
      }
      else if (DestX >= dest->Clip.Right) {
         log.trace("Clipped: DestX >= RightClip (%d >= %d)", DestX, dest->Clip.Right);
         return ERR_Okay;
      }

      if ((DestY < dest->Clip.Top)) {
         Height = Height - (dest->Clip.Top - DestY);
         if (Height < 1) return ERR_Okay;
         Y = Y + (dest->Clip.Top - DestY);
         DestY = dest->Clip.Top;
      }
      else if (DestY >= dest->Clip.Bottom) {
         log.trace("Clipped: DestY >= BottomClip (%d >= %d)", DestY, dest->Clip.Bottom);
         return ERR_Okay;
      }

      // Clip the Width and Height

      if ((DestX + Width)   >= Bitmap->Clip.Right)  Width  = Bitmap->Clip.Right - DestX;
      if ((DestY + Height)  >= Bitmap->Clip.Bottom) Height = Bitmap->Clip.Bottom - DestY;

      if ((X + Width)  >= Bitmap->Clip.Right)  Width  = Bitmap->Clip.Right - X;
      if ((Y + Height) >= Bitmap->Clip.Bottom) Height = Bitmap->Clip.Bottom - Y;
   }
   else {
      // Check if the destination that we are copying to is within the drawable area.

      if (DestX < dest->Clip.Left) {
         Width = Width - (dest->Clip.Left - DestX);
         if (Width < 1) return ERR_Okay;
         X = X + (dest->Clip.Left - DestX);
         DestX = dest->Clip.Left;
      }
      else if (DestX >= dest->Clip.Right) return ERR_Okay;

      if (DestY < dest->Clip.Top) {
         Height = Height - (dest->Clip.Top - DestY);
         if (Height < 1) return ERR_Okay;
         Y = Y + (dest->Clip.Top - DestY);
         DestY = dest->Clip.Top;
      }
      else if (DestY >= dest->Clip.Bottom) return ERR_Okay;

      // Check if the source that we are copying from is within its own drawable area.

      if (X < Bitmap->Clip.Left) {
         DestX += (Bitmap->Clip.Left - X);
         Width = Width - (Bitmap->Clip.Left - X);
         if (Width < 1) return ERR_Okay;
         X = Bitmap->Clip.Left;
      }
      else if (X >= Bitmap->Clip.Right) return ERR_Okay;

      if (Y < Bitmap->Clip.Top) {
         DestY += (Bitmap->Clip.Top - Y);
         Height = Height - (Bitmap->Clip.Top - Y);
         if (Height < 1) return ERR_Okay;
         Y = Bitmap->Clip.Top;
      }
      else if (Y >= Bitmap->Clip.Bottom) return ERR_Okay;

      // Clip the Width and Height of the source area, based on the imposed clip region.

      if ((DestX + Width)  >= dest->Clip.Right) Width   = dest->Clip.Right - DestX;
      if ((DestY + Height) >= dest->Clip.Bottom) Height = dest->Clip.Bottom - DestY;
      if ((X + Width)  >= Bitmap->Clip.Right)  Width  = Bitmap->Clip.Right - X;
      if ((Y + Height) >= Bitmap->Clip.Bottom) Height = Bitmap->Clip.Bottom - Y;
   }

   if (Width < 1) return ERR_Okay;
   if (Height < 1) return ERR_Okay;

   // Adjust coordinates by offset values

   X += Bitmap->XOffset;
   Y += Bitmap->YOffset;
   DestX  += dest->XOffset;
   DestY  += dest->YOffset;

#ifdef _WIN32
   if (dest->win.Drawable) { // Destination is a window

      if (Bitmap->win.Drawable) { // Both the source and destination are window areas
         LONG error;
         if ((error = winBlit(dest->win.Drawable, DestX, DestY, Width, Height, Bitmap->win.Drawable, X, Y))) {
            char buffer[80];
            buffer[0] = 0;
            winGetError(error, buffer, sizeof(buffer));
            log.warning("BitBlt(): %s", buffer);
         }
      }
      else { // The source is a software image
         if ((Flags & BAF_BLEND) and (Bitmap->BitsPerPixel IS 32) and (Bitmap->Flags & BMF_ALPHA_CHANNEL)) {
            ULONG *srcdata;
            UBYTE destred, destgreen, destblue, red, green, blue, alpha;

            // 32-bit alpha blending is enabled

            srcdata = (ULONG *)(Bitmap->Data + (Y * Bitmap->LineWidth) + (X<<2));

            while (Height > 0) {
               for (i=0; i < Width; i++) {
                  alpha = 255 - CFUnpackAlpha(&Bitmap->prvColourFormat, srcdata[i]);

                  if (alpha >= BLEND_MAX_THRESHOLD) {
                     red   = srcdata[i] >> Bitmap->prvColourFormat.RedPos;
                     green = srcdata[i] >> Bitmap->prvColourFormat.GreenPos;
                     blue  = srcdata[i] >> Bitmap->prvColourFormat.BluePos;
                     SetPixelV(dest->win.Drawable, DestX+i, DestY, (blue<<16) | (green<<8) | red);
                  }
                  else if (alpha >= BLEND_MIN_THRESHOLD) {
                     colour = GetPixel(dest->win.Drawable, DestX+i, DestY);
                     destred   = colour & 0xff;
                     destgreen = (colour>>8) & 0xff;
                     destblue  = (colour>>16) & 0xff;
                     red   = srcdata[i] >> Bitmap->prvColourFormat.RedPos;
                     green = srcdata[i] >> Bitmap->prvColourFormat.GreenPos;
                     blue  = srcdata[i] >> Bitmap->prvColourFormat.BluePos;
                     red   = destred   + (((red   - destred)   * alpha)>>8);
                     green = destgreen + (((green - destgreen) * alpha)>>8);
                     blue  = destblue  + (((blue  - destblue)  * alpha)>>8);
                     SetPixelV(dest->win.Drawable, DestX+i, DestY, (blue<<16) | (green<<8) | red);
                  }
               }
               srcdata = (ULONG *)(((UBYTE *)srcdata) + Bitmap->LineWidth);
               DestY++;
               Height--;
            }
         }
         else if (Bitmap->Flags & BMF_TRANSPARENT) {
            ULONG wincolour;
            while (Height > 0) {
               for (i=0; i < Width; i++) {
                  colour = Bitmap->ReadUCPixel(Bitmap, X + i, Y);
                  if (colour != (ULONG)Bitmap->TransIndex) {
                     wincolour = UnpackRed(Bitmap, colour);
                     wincolour |= UnpackGreen(Bitmap, colour)<<8;
                     wincolour |= UnpackBlue(Bitmap, colour)<<16;
                     SetPixelV(dest->win.Drawable, DestX + i, DestY, wincolour);
                  }
               }
               Y++; DestY++;
               Height--;
            }
         }
         else  {
            winSetDIBitsToDevice(dest->win.Drawable, DestX, DestY, Width, Height, X, Y,
               Bitmap->Width, Bitmap->Height, Bitmap->BitsPerPixel, Bitmap->Data,
               Bitmap->ColourFormat->RedMask   << Bitmap->ColourFormat->RedPos,
               Bitmap->ColourFormat->GreenMask << Bitmap->ColourFormat->GreenPos,
               Bitmap->ColourFormat->BlueMask  << Bitmap->ColourFormat->BluePos);
         }
      }

      return ERR_Okay;
   }

#elif __xwindows__

   // Use this routine if the destination is a pixmap (write only memory).  X11 windows are always represented as pixmaps.

   if ((dest->Flags & BMF_X11_DGA) and (glDGAAvailable) and (dest != Bitmap)) {
      // We have direct access to the graphics address, so drop through to the software routine
      dest->Data = (UBYTE *)glDGAVideo;
   }
   else if (dest->x11.drawable) {
      if (!Bitmap->x11.drawable) {
         if ((Flags & BAF_BLEND) and (Bitmap->BitsPerPixel IS 32) and (Bitmap->Flags & BMF_ALPHA_CHANNEL)) {
            ULONG *srcdata;
            UBYTE alpha;
            WORD cl, cr, ct, cb;

            cl = dest->Clip.Left;
            cr = dest->Clip.Right;
            ct = dest->Clip.Top;
            cb = dest->Clip.Bottom;
            dest->Clip.Left   = DestX - dest->XOffset;
            dest->Clip.Right  = DestX + Width - dest->XOffset;
            dest->Clip.Top    = DestY - dest->YOffset;
            dest->Clip.Bottom = DestY + Height - dest->YOffset;
            if (!LockSurface(dest, SURFACE_READ|SURFACE_WRITE)) {
               srcdata = (ULONG *)(Bitmap->Data + (Y * Bitmap->LineWidth) + (X<<2));

               while (Height > 0) {
                  for (i=0; i < Width; i++) {
                     alpha = 255 - UnpackAlpha(Bitmap, srcdata[i]);

                     if (alpha >= BLEND_MAX_THRESHOLD) {
                        pixel.Red   = (UBYTE)(srcdata[i] >> Bitmap->prvColourFormat.RedPos);
                        pixel.Green = (UBYTE)(srcdata[i] >> Bitmap->prvColourFormat.GreenPos);
                        pixel.Blue  = (UBYTE)(srcdata[i] >> Bitmap->prvColourFormat.BluePos);
                        dest->DrawUCRPixel(dest, DestX+i, DestY, &pixel);
                     }
                     else if (alpha >= BLEND_MIN_THRESHOLD) {
                        dest->ReadUCRPixel(dest, DestX+i, DestY, &pixel);
                        pixel.Red   += ((((UBYTE)(srcdata[i] >> Bitmap->prvColourFormat.RedPos)   - pixel.Red)   * alpha)>>8);
                        pixel.Green += ((((UBYTE)(srcdata[i] >> Bitmap->prvColourFormat.GreenPos) - pixel.Green) * alpha)>>8);
                        pixel.Blue  += ((((UBYTE)(srcdata[i] >> Bitmap->prvColourFormat.BluePos)  - pixel.Blue)  * alpha)>>8);
                        dest->DrawUCRPixel(dest, DestX+i, DestY, &pixel);
                     }
                  }
                  srcdata = (ULONG *)(((UBYTE *)srcdata) + Bitmap->LineWidth);
                  DestY++;
                  Height--;
               }
               UnlockSurface(dest);
            }
            dest->Clip.Left   = cl;
            dest->Clip.Right  = cr;
            dest->Clip.Top    = ct;
            dest->Clip.Bottom = cb;
         }
         else if (Bitmap->Flags & BMF_TRANSPARENT) {
            while (Height > 0) {
               for (i = 0; i < Width; i++) {
                  colour = Bitmap->ReadUCPixel(Bitmap, X + i, Y);
                  if (colour != (ULONG)Bitmap->TransIndex) dest->DrawUCPixel(dest, DestX + i, DestY, colour);
               }
               Y++; DestY++;
               Height--;
            }
         }
         else {
            // Source is an ximage, destination is a pixmap

            if (Bitmap->x11.XShmImage IS TRUE)  {
               if (XShmPutImage(XDisplay, dest->x11.drawable, glXGC, &Bitmap->x11.ximage, X, Y, DestX, DestY, Width, Height, False)) {

               }
               else log.warning("XShmPutImage() failed.");
            }
            else {
               XPutImage(XDisplay, dest->x11.drawable, glXGC,
                  &Bitmap->x11.ximage, X, Y, DestX, DestY, Width, Height);
            }
         }
      }
      else {
         // Both the source and the destination are pixmaps

         XCopyArea(XDisplay, Bitmap->x11.drawable, dest->x11.drawable,
            glXGC, X, Y, Width, Height, DestX, DestY);
      }

      return ERR_Okay;
   }

#elif _GLES_

   if (dest->DataFlags & MEM_VIDEO) { // Destination is the video display.
      if (Bitmap->DataFlags & MEM_VIDEO) { // Source is the video display.
         // No simple way to support this in OpenGL - we have to copy the display into a texture buffer, then copy the texture back to the display.

         ERROR error;
         if (!lock_graphics_active(__func__)) {
            GLuint texture;
            if (alloc_texture(Bitmap->Width, Bitmap->Height, &texture) IS GL_NO_ERROR) {
               //glViewport(0, 0, Bitmap->Width, Bitmap->Height);  // Set viewport so it matches texture size of ^2
               glCopyTexImage2D(GL_TEXTURE_2D, 0, Bitmap->prvGLPixel, 0, 0, Bitmap->Width, Bitmap->Height, 0); // Copy screen to texture
               //glViewport(0, 0, Bitmap->Width, Bitmap->Height);  // Restore viewport to display size
               glDrawTexiOES(DestX, -DestY, 1, Bitmap->Width, Bitmap->Height);
               glBindTexture(GL_TEXTURE_2D, 0);
               eglSwapBuffers(glEGLDisplay, glEGLSurface);
               glDeleteTextures(1, &texture);
               error = ERR_Okay;
            }
            else error = log.warning(ERR_OpenGL);

            unlock_graphics();
         }
         else error = ERR_LockFailed;

         return error;
      }
      else if (Bitmap->DataFlags & MEM_TEXTURE) {
         // Texture-to-video blitting (


      }
      else {
         // RAM-to-video blitting.  We have to allocate a temporary texture, copy the data to it and then blit that to the display.

         ERROR error;
         if (!lock_graphics_active(__func__)) {
            GLuint texture;
            if (alloc_texture(Bitmap->Width, Bitmap->Height, &texture) IS GL_NO_ERROR) {
               glTexImage2D(GL_TEXTURE_2D, 0, Bitmap->prvGLPixel, Bitmap->Width, Bitmap->Height, 0, Bitmap->prvGLPixel, Bitmap->prvGLFormat, Bitmap->Data); // Copy the bitmap content to the texture.
               if (glGetError() IS GL_NO_ERROR) {
                  glDrawTexiOES(0, 0, 1, Bitmap->Width, Bitmap->Height);
                  glBindTexture(GL_TEXTURE_2D, 0);
                  eglSwapBuffers(glEGLDisplay, glEGLSurface);
               }
               else error = ERR_OpenGL;

               glDeleteTextures(1, &texture);
               error = ERR_Okay;
            }
            else error = log.warning(ERR_OpenGL);

            unlock_graphics();
         }
         else error = ERR_LockFailed;

         return error;
      }
   }

#endif

   // GENERIC SOFTWARE BLITTING ROUTINES

   if ((Flags & BAF_BLEND) and (Bitmap->BitsPerPixel IS 32) and (Bitmap->Flags & BMF_ALPHA_CHANNEL)) {
      // 32-bit alpha blending support

      if (!LockSurface(Bitmap, SURFACE_READ)) {
         if (!LockSurface(dest, SURFACE_WRITE)) {
            UBYTE red, green, blue, *dest_lookup;
            UWORD alpha;

            dest_lookup = glAlphaLookup + (255<<8);

            if (dest->BitsPerPixel IS 32) { // Both bitmaps are 32 bit
               const UBYTE sA = Bitmap->ColourFormat->AlphaPos>>3;
               const UBYTE sR = Bitmap->ColourFormat->RedPos>>3;
               const UBYTE sG = Bitmap->ColourFormat->GreenPos>>3;
               const UBYTE sB = Bitmap->ColourFormat->BluePos>>3;
               const UBYTE dA = dest->ColourFormat->AlphaPos>>3;
               const UBYTE dR = dest->ColourFormat->RedPos>>3;
               const UBYTE dG = dest->ColourFormat->GreenPos>>3;
               const UBYTE dB = dest->ColourFormat->BluePos>>3;

               UBYTE *sdata = Bitmap->Data + (Y * Bitmap->LineWidth) + (X<<2);
               UBYTE *ddata = dest->Data + (DestY * dest->LineWidth) + (DestX<<2);

               if (Flags & BAF_COPY) { // Avoids blending in cases where the destination pixel is empty.
                  for (LONG y=0; y < Height; y++) {
                     UBYTE *sp = sdata, *dp = ddata;
                     for (LONG x=0; x < Width; x++) {
                        if (dp[dA]) {
                           if (sp[sA] IS 0xff) ((ULONG *)dp)[0] = ((ULONG *)sp)[0];
                           else if (sp[sA]) {
                              dp[dR] = dp[dR] + (((sp[sR] - dp[dR]) * sp[sA])>>8);
                              dp[dG] = dp[dG] + (((sp[sG] - dp[dG]) * sp[sA])>>8);
                              dp[dB] = dp[dB] + (((sp[sB] - dp[dB]) * sp[sA])>>8);
                              dp[dA] = dp[dA] + ((sp[sA] * (0xff-dp[dA]))>>8);
                           }
                        }
                        else ((ULONG *)dp)[0] = ((ULONG *)sp)[0];

                        sp += 4;
                        dp += 4;
                     }
                     sdata += Bitmap->LineWidth;
                     ddata += dest->LineWidth;
                  }
               }
               else {
                  while (Height > 0) {
                     UBYTE *sp = sdata, *dp = ddata;
                     if (Bitmap->Opacity IS 0xff) {
                        for (i=0; i < Width; i++) {
                           if (sp[sA] IS 0xff) ((ULONG *)dp)[0] = ((ULONG *)sp)[0];
                           else if (sp[sA]) {
                              UBYTE alpha = sp[sA];
                              dp[dR] = dp[dR] + (((sp[sR] - dp[dR]) * alpha)>>8);
                              dp[dG] = dp[dG] + (((sp[sG] - dp[dG]) * alpha)>>8);
                              dp[dB] = dp[dB] + (((sp[sB] - dp[dB]) * alpha)>>8);
                              dp[dA] = dp[dA] + ((sp[sA] * (0xff-dp[dA]))>>8);
                           }

                           sp += 4;
                           dp += 4;
                        }
                     }
                     else {
                        for (i=0; i < Width; i++) {
                           if (sp[sA]) {
                              UBYTE alpha = (sp[sA] * Bitmap->Opacity)>>8;
                              dp[dR] = dp[dR] + (((sp[sR] - dp[dR]) * alpha)>>8);
                              dp[dG] = dp[dG] + (((sp[sG] - dp[dG]) * alpha)>>8);
                              dp[dB] = dp[dB] + (((sp[sB] - dp[dB]) * alpha)>>8);
                              dp[dA] = dp[dA] + ((sp[sA] * (0xff-dp[dA]))>>8);
                           }

                           sp += 4;
                           dp += 4;
                        }
                     }
                     sdata += Bitmap->LineWidth;
                     ddata += dest->LineWidth;
                     Height--;
                  }
               }
            }
            else if (dest->BytesPerPixel IS 2) {
               UWORD *ddata;
               ULONG *sdata = (ULONG *)(Bitmap->Data + (Y * Bitmap->LineWidth) + (X<<2));
               ddata = (UWORD *)(dest->Data + (DestY * dest->LineWidth) + (DestX<<1));
               while (Height > 0) {
                  for (i=0; i < Width; i++) {
                     colour = sdata[i];
                     alpha = ((UBYTE)(colour >> Bitmap->prvColourFormat.AlphaPos));
                     alpha = (glAlphaLookup + (alpha<<8))[Bitmap->Opacity]<<8; // Multiply the source pixel by overall translucency level

                     if (alpha >= BLEND_MAX_THRESHOLD<<8) {
                        ddata[i] = PackPixel(dest, (UBYTE)(colour >> Bitmap->prvColourFormat.RedPos),
                                                   (UBYTE)(colour >> Bitmap->prvColourFormat.GreenPos),
                                                   (UBYTE)(colour >> Bitmap->prvColourFormat.BluePos));
                     }
                     else if (alpha >= BLEND_MIN_THRESHOLD<<8) {
                        red   = colour >> Bitmap->prvColourFormat.RedPos;
                        green = colour >> Bitmap->prvColourFormat.GreenPos;
                        blue  = colour >> Bitmap->prvColourFormat.BluePos;
                        srctable  = glAlphaLookup + (alpha);
                        desttable = dest_lookup - (alpha);
                        ddata[i] = PackPixel(dest, (UBYTE)(srctable[red]   + desttable[UnpackRed(dest, ddata[i])]),
                                                   (UBYTE)(srctable[green] + desttable[UnpackGreen(dest, ddata[i])]),
                                                   (UBYTE)(srctable[blue]  + desttable[UnpackBlue(dest, ddata[i])]));
                     }
                  }
                  sdata = (ULONG *)(((UBYTE *)sdata) + Bitmap->LineWidth);
                  ddata = (UWORD *)(((UBYTE *)ddata) + dest->LineWidth);
                  Height--;
               }
            }
            else {
               ULONG *sdata = (ULONG *)(Bitmap->Data + (Y * Bitmap->LineWidth) + (X<<2));
               while (Height > 0) {
                  for (i=0; i < Width; i++) {
                     colour = sdata[i];
                     alpha = ((UBYTE)(colour >> Bitmap->prvColourFormat.AlphaPos));
                     alpha = (glAlphaLookup + (alpha<<8))[Bitmap->Opacity]; // Multiply the source pixel by overall translucency level

                     if (alpha >= BLEND_MAX_THRESHOLD) {
                        pixel.Red   = colour >> Bitmap->prvColourFormat.RedPos;
                        pixel.Green = colour >> Bitmap->prvColourFormat.GreenPos;
                        pixel.Blue  = colour >> Bitmap->prvColourFormat.BluePos;
                        dest->DrawUCRPixel(dest, DestX + i, DestY, &pixel);
                     }
                     else if (alpha >= BLEND_MIN_THRESHOLD) {
                        red   = colour >> Bitmap->prvColourFormat.RedPos;
                        green = colour >> Bitmap->prvColourFormat.GreenPos;
                        blue  = colour >> Bitmap->prvColourFormat.BluePos;

                        srctable  = glAlphaLookup + (alpha<<8);
                        desttable = glAlphaLookup + ((255-alpha)<<8);

                        dest->ReadUCRPixel(dest, DestX + i, DestY, &pixel);
                        pixel.Red   = srctable[red]   + desttable[pixel.Red];
                        pixel.Green = srctable[green] + desttable[pixel.Green];
                        pixel.Blue  = srctable[blue]  + desttable[pixel.Blue];
                        dest->DrawUCRPixel(dest, DestX + i, DestY, &pixel);
                     }
                  }
                  sdata = (ULONG *)(((UBYTE *)sdata) + Bitmap->LineWidth);
                  DestY++;
                  Height--;
               }
            }

            UnlockSurface(dest);
         }
         UnlockSurface(Bitmap);
      }

      return ERR_Okay;
   }
   else if (Bitmap->Flags & BMF_TRANSPARENT) {
      // Transparent colour copying.  In this mode, the alpha component of individual source pixels is ignored

      if (!LockSurface(Bitmap, SURFACE_READ)) {
         if (!LockSurface(dest, SURFACE_WRITE)) {
            if (Bitmap->Opacity < 255) { // Transparent mask with translucent pixels (consistent blend level)
               srctable  = glAlphaLookup + (Bitmap->Opacity<<8);
               desttable = glAlphaLookup + ((255-Bitmap->Opacity)<<8);
               while (Height > 0) {
                  for (i=0; i < Width; i++) {
                     colour = Bitmap->ReadUCPixel(Bitmap, X + i, Y);
                     if (colour != (ULONG)Bitmap->TransIndex) {
                        dest->ReadUCRPixel(dest, DestX + i, DestY, &pixel);

                        pixel.Red   = srctable[UnpackRed(Bitmap, colour)]   + desttable[pixel.Red];
                        pixel.Green = srctable[UnpackGreen(Bitmap, colour)] + desttable[pixel.Green];
                        pixel.Blue  = srctable[UnpackBlue(Bitmap, colour)]  + desttable[pixel.Blue];

                        dest->DrawUCRPixel(dest, DestX + i, DestY, &pixel);
                     }
                  }
                  Y++; DestY++;
                  Height--;
               }
            }
            else if (Bitmap->BitsPerPixel IS dest->BitsPerPixel) {
               if (Bitmap->BytesPerPixel IS 4) {
                  ULONG *ddata, *sdata;

                  sdata = (ULONG *)(Bitmap->Data + (Y * Bitmap->LineWidth) + (X<<2));
                  ddata = (ULONG *)(dest->Data + (DestY * dest->LineWidth) + (DestX<<2));
                  colour = Bitmap->TransIndex;
                  while (Height > 0) {
                     for (i=0; i < Width; i++) if (sdata[i] != colour) ddata[i] = sdata[i];
                     ddata = (ULONG *)(((BYTE *)ddata) + dest->LineWidth);
                     sdata = (ULONG *)(((BYTE *)sdata) + Bitmap->LineWidth);
                     Height--;
                  }
               }
               else if (Bitmap->BytesPerPixel IS 2) {
                  UWORD *ddata, *sdata;

                  sdata = (UWORD *)(Bitmap->Data + (Y * Bitmap->LineWidth) + (X<<1));
                  ddata = (UWORD *)(dest->Data + (DestY * dest->LineWidth) + (DestX<<1));
                  colour = Bitmap->TransIndex;
                  while (Height > 0) {
                     for (i=0; i < Width; i++) if (sdata[i] != colour) ddata[i] = sdata[i];
                     ddata = (UWORD *)(((BYTE *)ddata) + dest->LineWidth);
                     sdata = (UWORD *)(((BYTE *)sdata) + Bitmap->LineWidth);
                     Height--;
                  }
               }
               else {
                  while (Height > 0) {
                     for (LONG i=0; i < Width; i++) {
                        colour = Bitmap->ReadUCPixel(Bitmap, X + i, Y);
                        if (colour != (ULONG)Bitmap->TransIndex) dest->DrawUCPixel(dest, DestX + i, DestY, colour);
                     }
                     Y++; DestY++;
                     Height--;
                  }
               }
            }
            else if (Bitmap->BitsPerPixel IS 8) {
               while (Height > 0) {
                  for (LONG i=0; i < Width; i++) {
                     colour = Bitmap->ReadUCPixel(Bitmap, X + i, Y);
                     if (colour != (ULONG)Bitmap->TransIndex) {
                        dest->DrawUCRPixel(dest, DestX + i, DestY, &Bitmap->Palette->Col[colour]);
                     }
                  }
                  Y++; DestY++;
                  Height--;
               }
            }
            else while (Height > 0) {
               for (LONG i=0; i < Width; i++) {
                  Bitmap->ReadUCRPixel(Bitmap, X + i, Y, &pixel);
                  if ((pixel.Red != Bitmap->TransRGB.Red) or (pixel.Green != Bitmap->TransRGB.Green) or (pixel.Blue != Bitmap->TransRGB.Blue)) {
                     dest->DrawUCRPixel(dest, DestX + i, DestY, &pixel);
                  }
               }
               Y++; DestY++;
               Height--;
            }

            UnlockSurface(dest);
         }
         UnlockSurface(Bitmap);
      }

      return ERR_Okay;
   }
   else { // Straight copy operation
      if (!LockSurface(Bitmap, SURFACE_READ)) {
         if (!LockSurface(dest, SURFACE_WRITE)) {
            if (Bitmap->Opacity < 255) { // Translucent draw
               srctable  = glAlphaLookup + (Bitmap->Opacity<<8);
               desttable = glAlphaLookup + ((255-Bitmap->Opacity)<<8);

               if ((Bitmap->BytesPerPixel IS 4) and (dest->BytesPerPixel IS 4)) {
                  ULONG *ddata, *sdata;
                  ULONG cmp_alpha;

                  sdata = (ULONG *)(Bitmap->Data + (Y * Bitmap->LineWidth) + (X<<2));
                  ddata = (ULONG *)(dest->Data + (DestY * dest->LineWidth) + (DestX<<2));
                  cmp_alpha = 255 << Bitmap->prvColourFormat.AlphaPos;
                  while (Height > 0) {
                     for (i=0; i < Width; i++) {
                        ddata[i] = ((srctable[(UBYTE)(sdata[i]>>Bitmap->prvColourFormat.RedPos)]   + desttable[(UBYTE)(ddata[i]>>dest->prvColourFormat.RedPos)]) << dest->prvColourFormat.RedPos) |
                                   ((srctable[(UBYTE)(sdata[i]>>Bitmap->prvColourFormat.GreenPos)] + desttable[(UBYTE)(ddata[i]>>dest->prvColourFormat.GreenPos)]) << dest->prvColourFormat.GreenPos) |
                                   ((srctable[(UBYTE)(sdata[i]>>Bitmap->prvColourFormat.BluePos)]  + desttable[(UBYTE)(ddata[i]>>dest->prvColourFormat.BluePos)]) << dest->prvColourFormat.BluePos) |
                                   cmp_alpha;
                     }
                     ddata = (ULONG *)(((BYTE *)ddata) + dest->LineWidth);
                     sdata = (ULONG *)(((BYTE *)sdata) + Bitmap->LineWidth);
                     Height--;
                  }
               }
               else if ((Bitmap->BytesPerPixel IS 2) and (dest->BytesPerPixel IS 2)) {
                  UWORD *ddata, *sdata;

                  sdata = (UWORD *)(Bitmap->Data + (Y * Bitmap->LineWidth) + (X<<1));
                  ddata = (UWORD *)(dest->Data + (DestY * dest->LineWidth) + (DestX<<1));
                  while (Height > 0) {
                     for (i=0; i < Width; i++) {
                        ddata[i] = PackPixel(dest, srctable[UnpackRed(Bitmap, sdata[i])]   + desttable[UnpackRed(dest, ddata[i])],
                                                   srctable[UnpackGreen(Bitmap, sdata[i])] + desttable[UnpackGreen(dest, ddata[i])],
                                                   srctable[UnpackBlue(Bitmap, sdata[i])]  + desttable[UnpackBlue(dest, ddata[i])]);
                     }
                     ddata = (UWORD *)(((BYTE *)ddata) + dest->LineWidth);
                     sdata = (UWORD *)(((BYTE *)sdata) + Bitmap->LineWidth);
                     Height--;
                  }
               }
               else while (Height > 0) {
                  for (i=0; i < Width; i++) {
                     Bitmap->ReadUCRPixel(Bitmap, X + i, Y, &src);
                     dest->ReadUCRPixel(dest, DestX + i, DestY, &pixel);

                     pixel.Red   = srctable[src.Red]   + desttable[pixel.Red];
                     pixel.Green = srctable[src.Green] + desttable[pixel.Green];
                     pixel.Blue  = srctable[src.Blue]  + desttable[pixel.Blue];

                     dest->DrawUCRPixel(dest, DestX + i, DestY, &pixel);
                  }
                  Y++; DestY++;
                  Height--;
               }
            }
            else if (Bitmap->BitsPerPixel IS dest->BitsPerPixel) {
               // Use this fast routine for identical bitmaps

               srcdata = Bitmap->Data + (X * Bitmap->BytesPerPixel) + (Y * Bitmap->LineWidth);
               data    = dest->Data + (DestX  * dest->BytesPerPixel) + (DestY * dest->LineWidth);
               Width   = Width * Bitmap->BytesPerPixel;

               if ((Bitmap IS dest) and (DestY >= Y) and (DestY < Y+Height)) {
                  // Copy backwards when we are copying within the same bitmap and there is an overlap.

                  srcdata += Bitmap->LineWidth * (Height-1);
                  data    += dest->LineWidth * (Height-1);

                  while (Height > 0) {
                     for (i=Width-1; i >= 0; i--) data[i] = srcdata[i];
                     srcdata -= Bitmap->LineWidth;
                     data    -= dest->LineWidth;
                     Height--;
                  }
               }
               else {
                  while (Height > 0) {
                     for (i=0; (size_t)i > sizeof(LONG); i += sizeof(LONG)) {
                        ((LONG *)(data+i))[0] = ((LONG *)(srcdata+i))[0];
                     }
                     while (i < Width) { data[i] = srcdata[i]; i++; }
                     srcdata += Bitmap->LineWidth;
                     data    += dest->LineWidth;
                     Height--;
                  }
               }
            }
            else {
               // If the bitmaps do not match then we need to use this slower RGB translation subroutine.

               bool dithered = FALSE;
               if (Flags & BAF_DITHER) {
                  if ((dest->BitsPerPixel < 24) and
                      ((Bitmap->BitsPerPixel > dest->BitsPerPixel) or
                       ((Bitmap->BitsPerPixel <= 8) and (dest->BitsPerPixel > 8)))) {
                     if (Bitmap->Flags & BMF_TRANSPARENT);
                     else {
                        dither(Bitmap, dest, NULL, Width, Height, X, Y, DestX, DestY);
                        dithered = TRUE;
                     }
                  }
               }

               if (dithered IS FALSE) {
                  if ((Bitmap IS dest) and (DestY >= Y) and (DestY < Y+Height)) {
                     while (Height > 0) {
                        Y += Height - 1;
                        DestY  += Height - 1;
                        for (i=0; i < Width; i++) {
                           Bitmap->ReadUCRPixel(Bitmap, X + i, Y, &pixel);
                           dest->DrawUCRPixel(dest, DestX + i, DestY, &pixel);
                        }
                        Y--; DestY--;
                        Height--;
                     }
                  }
                  else {
                     while (Height > 0) {
                        for (i=0; i < Width; i++) {
                           Bitmap->ReadUCRPixel(Bitmap, X + i, Y, &pixel);
                           dest->DrawUCRPixel(dest, DestX + i, DestY, &pixel);
                        }
                        Y++; DestY++;
                        Height--;
                     }
                  }
               }
            }

            UnlockSurface(dest);
         }
         UnlockSurface(Bitmap);
      }

      return ERR_Okay;
   }
}

/*****************************************************************************

-FUNCTION-
CopyStretch: Copies a rectangular area from one bitmap to another, with stretching.

This function copies rectangular images between bitmaps.  It performs a straight region-copy with the added capability
of stretching the source to match the destination area.  The source and destination bitmaps may be of a different type,
however this will result in a much slower copy operation.  The copy will honour the clip region set in both the source
and destination bitmap objects.

Special flags affecting the stretch operation are accepted by this routine.  The following table illustrates the
available options.

<types lookup="CSTF"/>

Special operations like transparency and alpha blending are not currently supported by this routine.

-INPUT-
obj(Bitmap) Bitmap: The source bitmap.
obj(Bitmap) Dest: Pointer to the destination bitmap.
int(CSTF) Flags: Special flags.
int X:          The horizontal position of the area to be copied.
int Y:          The vertical position of the area to be copied.
int Width:      The width of the source area.
int Height:     The height of the source area.
int XDest:      The horizontal position to copy the area to.
int YDest:      The vertical position to copy the area to.
int DestWidth:  The width to use for the destination area.
int DestHeight: The height to use for the destination area.

-ERRORS-
Okay:
NullArgs:
Mismatch: The destination bitmap is not a close enough match to the source bitmap in order to perform the blit.

*****************************************************************************/

#define FILTER_THRESHOLD   255
#define BILINEAR_THRESHOLD 255

static ERROR gfxCopyStretch(objBitmap *Bitmap, objBitmap *Dest, LONG Flags, LONG X,
   LONG Y, LONG Width, LONG Height, LONG DestX, LONG DestY, LONG DestWidth, LONG DestHeight)
{
   parasol::Log log(__FUNCTION__);
   UBYTE *diffytable, *ytable, *diffxtable, *xtable, *destdata, *srcdata;
   LONG x, y, isrcx, isrcy, endx, endy, bytex;

   if (!Dest) return ERR_NullArgs;
   if (Dest->Head.ClassID != ID_BITMAP) return log.warning(ERR_InvalidObject);

   if (Bitmap IS Dest) return log.warning(ERR_Args);

   if ((Width IS DestWidth) and (Height IS DestHeight)) {
      gfxCopyArea(Bitmap, Dest, 0, X, Y, Width, Height, DestX, DestY);
   }

   if ((Width < 1) or (Height < 1) or (DestWidth < 1) or (DestHeight < 1)) return ERR_Okay;

   // Check the clipping regions before committing to the blit

   if ((Dest->Clip.Right <= DestX) or (Dest->Clip.Top >= DestY+DestHeight) or
       (Dest->Clip.Bottom <= DestY) or (Dest->Clip.Left >= DestX+DestWidth)) return ERR_Okay;

   if ((Bitmap->Clip.Right <= X) or (Bitmap->Clip.Top >= Y+Height) or
       (Bitmap->Clip.Bottom <= Y) or (Bitmap->Clip.Left >= X+Width)) return ERR_Okay;

   // Figure out a good resampling routine if none is specified.  At a minimum, bresenham is fast and better than nearest-neighbour.

   if (!(Flags & (CSTF_BILINEAR|CSTF_BRESENHAM|CSTF_NEIGHBOUR))) {
      Flags |= CSTF_BRESENHAM;
   }

   log.traceBranch("#%d (%dx%d,%dx%d) TO #%d (%dx%d)", Bitmap->Head.UniqueID,  X, Y, Width, Height, Dest->Head.UniqueID, DestWidth, DestHeight);

   if (!LockSurface(Bitmap, SURFACE_READ)) {
      if (!LockSurface(Dest, SURFACE_WRITE)) {

         // If the FILTERSOURCE flag is set, this means that we are allowed to filter the source image for the purposes of attaining better quality.
         // This will give the developer a highly modified source image after this function returns.
         //
         // The first part of the process that cuts the image in half, this helps us retain graphics quality when reducing large images to very small areas.

         log.trace("%dx%d TO %dx%d", Width, Height, DestWidth, DestHeight);

         if (Flags & CSTF_FILTER_SOURCE) {
            RGB8 rgb1, rgb2, rgb3, rgb4, rgb;

            #define CUTHALF      0.30  // The ratio required for the image to be cut in half (cannot be more than 0.5)
            #define FILTER_RATIO 0.60  // The ratio required for the image to be filtered for the last pass

            // Cut the height of the source in half

            while ((DOUBLE)DestHeight / (DOUBLE)Height < CUTHALF) {
              endy = Bitmap->Clip.Bottom>>1;
              for (y=Bitmap->Clip.Top; y < endy; y++) {
                  srcdata  = Bitmap->Data + (((y<<1)+Bitmap->YOffset) * Bitmap->LineWidth) + ((Bitmap->Clip.Left+Bitmap->XOffset) * Bitmap->BytesPerPixel);
                  destdata = Bitmap->Data + ((y+Bitmap->YOffset) * Bitmap->LineWidth) + ((Bitmap->Clip.Left+Bitmap->XOffset) * Bitmap->BytesPerPixel);
                  for (x=Bitmap->Clip.Left; x < Bitmap->Clip.Right; x++) {
                     Bitmap->ReadUCRIndex(Bitmap, srcdata, &rgb1);

                     if (y < endy-1) {
                        Bitmap->ReadUCRIndex(Bitmap, srcdata + Bitmap->LineWidth, &rgb2);
                     }
                     else {
                        //Bitmap->ReadUCRIndex(Bitmap, srcdata - Bitmap->LineWidth, &rgb2);
                        rgb2 = rgb1;
                        rgb2.Alpha = 0;
                     }

                     #if (FILTER_THRESHOLD < 255)
                        if (rgb2.Alpha > FILTER_THRESHOLD) {
                           Bitmap->DrawUCRIndex(Bitmap, destdata, &rgb1);
                        }
                        else {
                           rgb.Red    = (rgb1.Red   + rgb2.Red)>>1;
                           rgb.Green  = (rgb1.Green + rgb2.Green)>>1;
                           rgb.Blue   = (rgb1.Blue  + rgb2.Blue)>>1;
                           rgb.Alpha  = (rgb1.Alpha + rgb2.Alpha)>>1;
                           Bitmap->DrawUCRIndex(Bitmap, destdata, &rgb);
                        }
                     #else
                        rgb.Red    = (rgb1.Red   + rgb2.Red)>>1;
                        rgb.Green  = (rgb1.Green + rgb2.Green)>>1;
                        rgb.Blue   = (rgb1.Blue  + rgb2.Blue)>>1;
                        rgb.Alpha  = (rgb1.Alpha + rgb2.Alpha)>>1;
                        Bitmap->DrawUCRIndex(Bitmap, destdata, &rgb);
                     #endif

                     srcdata += Bitmap->BytesPerPixel;
                     destdata += Bitmap->BytesPerPixel;
                  }
               }

               Bitmap->Clip.Bottom -= (Height>>1);
               Height = Height>>1;
            }

            // Cut the width of the source in half

            while ((DOUBLE)DestWidth / (DOUBLE)Width < CUTHALF) {
              endx = Bitmap->Clip.Right>>1;
              for (y=Bitmap->Clip.Top; y < Bitmap->Clip.Bottom; y++) {
                  srcdata  = Bitmap->Data + ((y+Bitmap->YOffset) * Bitmap->LineWidth) + ((Bitmap->Clip.Left+Bitmap->XOffset) * Bitmap->BytesPerPixel);
                  destdata = srcdata;
                  for (x=Bitmap->Clip.Left; x < endx; x++) {
                     Bitmap->ReadUCRIndex(Bitmap, srcdata, &rgb1);

                     if (x < endx-1) {
                        Bitmap->ReadUCRIndex(Bitmap, srcdata + Bitmap->BytesPerPixel, &rgb2);
                     }
                     else {
                        //Bitmap->ReadUCRIndex(Bitmap, srcdata - Bitmap->BytesPerPixel, &rgb2);
                        rgb2 = rgb1;
                        rgb2.Alpha = 0;
                     }

                     #if (FILTER_THRESHOLD < 255)
                        if (rgb2.Alpha > FILTER_THRESHOLD) {
                           Bitmap->DrawUCRIndex(Bitmap, destdata, &rgb1);
                        }
                        else {
                           rgb.Red    = (rgb1.Red   + rgb2.Red)>>1;
                           rgb.Green  = (rgb1.Green + rgb2.Green)>>1;
                           rgb.Blue   = (rgb1.Blue  + rgb2.Blue)>>1;
                           rgb.Alpha  = (rgb1.Alpha + rgb2.Alpha)>>1;
                           Bitmap->DrawUCRIndex(Bitmap, destdata, &rgb);
                        }
                     #else
                        rgb.Red    = (rgb1.Red   + rgb2.Red)>>1;
                        rgb.Green  = (rgb1.Green + rgb2.Green)>>1;
                        rgb.Blue   = (rgb1.Blue  + rgb2.Blue)>>1;
                        rgb.Alpha  = (rgb1.Alpha + rgb2.Alpha)>>1;
                        Bitmap->DrawUCRIndex(Bitmap, destdata, &rgb);
                     #endif

                     srcdata += Bitmap->BytesPerPixel + Bitmap->BytesPerPixel;
                     destdata += Bitmap->BytesPerPixel;
                  }
               }

               Bitmap->Clip.Right -= (Width>>1);
               Width = Width>>1;
            }

            if ((((DOUBLE)DestHeight / (DOUBLE)Height >= CUTHALF) and ((DOUBLE)DestHeight / (DOUBLE)Height <= FILTER_RATIO)) or
                (((DOUBLE)DestWidth / (DOUBLE)Width >= CUTHALF) and ((DOUBLE)DestWidth / (DOUBLE)Width <= FILTER_RATIO))) {

               log.trace("Image will be filtered for last step (%dx%d TO %dx%d)", Width, Height, DestWidth, DestHeight);

               for (y=Bitmap->Clip.Top; y < Bitmap->Clip.Bottom-1; y++) {
                  destdata = Bitmap->Data + ((y+Bitmap->YOffset) * Bitmap->LineWidth) + ((Bitmap->Clip.Left+Bitmap->XOffset) * Bitmap->BytesPerPixel);

                  for (x=Bitmap->Clip.Left; x < Bitmap->Clip.Right; x++) {
                     // Read 2x2 pixels, extended from the source point (rgb1)

                     if (x < Bitmap->Clip.Right-1) {
                        Bitmap->ReadUCRIndex(Bitmap, destdata, &rgb1);
                        Bitmap->ReadUCRIndex(Bitmap, destdata + Bitmap->BytesPerPixel, &rgb2);
                        Bitmap->ReadUCRIndex(Bitmap, destdata + Bitmap->LineWidth, &rgb3);
                        Bitmap->ReadUCRIndex(Bitmap, destdata + Bitmap->LineWidth + Bitmap->BytesPerPixel, &rgb4);
                     }
                     else {
                        Bitmap->ReadUCRIndex(Bitmap, destdata, &rgb1);
                        Bitmap->ReadUCRIndex(Bitmap, destdata - Bitmap->BytesPerPixel, &rgb2);
                        Bitmap->ReadUCRIndex(Bitmap, destdata + Bitmap->LineWidth, &rgb3);
                        Bitmap->ReadUCRIndex(Bitmap, destdata + Bitmap->LineWidth - Bitmap->BytesPerPixel, &rgb4);
                     }

                     #if (FILTER_THRESHOLD < 255)
                        if (rgb2.Alpha > FILTER_THRESHOLD) rgb2 = rgb1;
                        if (rgb3.Alpha > FILTER_THRESHOLD) rgb3 = rgb1;
                        if (rgb4.Alpha > FILTER_THRESHOLD) rgb4 = rgb1;
                     #endif

                     // Calculate the averages and draw the pixel

                     rgb.Red    = (rgb1.Red   + rgb2.Red   + rgb3.Red   + rgb4.Red)>>2;
                     rgb.Green  = (rgb1.Green + rgb2.Green + rgb3.Green + rgb4.Green)>>2;
                     rgb.Blue   = (rgb1.Blue  + rgb2.Blue  + rgb3.Blue  + rgb4.Blue)>>2;
                     rgb.Alpha = (rgb1.Alpha + rgb2.Alpha + rgb3.Alpha + rgb4.Alpha)>>2;

                     Bitmap->DrawUCRIndex(Bitmap, destdata, &rgb);
                     destdata += Bitmap->BytesPerPixel;
                  }
               }
            }
         }

         if (Flags & CSTF_BRESENHAM) {
            // Fast bresenham scaling from Dr Dobbs Journal by Thiadmer Riemersma.  Provides fast resampling of OK quality between 0.67 and 2.0 scaling factors.

            ClipRectangle clip;
            DOUBLE fx, fy, xScale, yScale;
            LONG numpixels, mid, e;
            RGB8 p, p2;

            xScale = (DOUBLE)Width / (DOUBLE)DestWidth;
            yScale = (DOUBLE)Height / (DOUBLE)DestHeight;

            clip = Bitmap->Clip;
            clip.Left   += Bitmap->XOffset;
            clip.Right  += Bitmap->XOffset;
            clip.Top    += Bitmap->YOffset;
            clip.Bottom += Bitmap->YOffset;

            fy = Y;
            for (y=DestY; y < DestY + DestHeight; y++, fy += yScale) {
               if (y < Dest->Clip.Top) continue;
               if (y >= Dest->Clip.Bottom) break;

               isrcy = Bitmap->YOffset + F2I(fy);
               mid = DestWidth>>1;
               e = 0;
               fx = X;
               numpixels = DestWidth;
               //if (DestWidth > Width) numpixels--;

               for (x=DestX; (x < DestX+DestWidth) and (numpixels-- > 0); x++, fx += xScale) {
                  if (x < Dest->Clip.Left) continue;
                  if (x >= Dest->Clip.Right) break;

                  isrcx = F2I(fx) + Bitmap->XOffset;
                  if ((isrcx >= clip.Left) and (isrcy >= clip.Top) and
                      (isrcx < clip.Right) and (isrcy < clip.Bottom)) {

                     Bitmap->ReadUCRPixel(Bitmap, isrcx, isrcy, &p);

                     if ((e >= mid) and (isrcx+1 < clip.Left)) {
                        Bitmap->ReadUCRPixel(Bitmap, isrcx + 1, isrcy, &p2);
                        p.Red   = (p.Red + p2.Red)>>1;
                        p.Green = (p.Green + p2.Green)>>1;
                        p.Blue  = (p.Blue + p2.Blue)>>1;
                     }
                     Dest->DrawUCRPixel(Dest, Dest->XOffset + x, Dest->YOffset + y, &p);
                  }

                  e += Width;
                  if (e >= DestWidth) {
                     e -= DestWidth;
                     isrcx++;
                  }
               }
            }
         }
         else if (Flags & CSTF_BILINEAR) {
            RGB8  background, rgb[4];
            ClipRectangle srcclip;
            LONG xScale, startsrcx, calcx, calcy, srcx, dx, isrcy2;
            DOUBLE yScale, srcy;

            // Bilinear resample, good for expanding images, okay for shrinking

            xScale = (Width<<8) / DestWidth;
            yScale = (DOUBLE)Height / (DOUBLE)DestHeight;
            background.Red   = Bitmap->BkgdRGB.Red;
            background.Green = Bitmap->BkgdRGB.Green;
            background.Blue  = Bitmap->BkgdRGB.Blue;
            if (Flags & CSTF_CLAMP) background.Alpha = 255;
            else background.Alpha = 0;

            srcclip = Bitmap->Clip;
            srcclip.Left   += Bitmap->XOffset;
            srcclip.Right  += Bitmap->XOffset;
            srcclip.Top    += Bitmap->YOffset;
            srcclip.Bottom += Bitmap->YOffset;

            // Setup horizontal parameters

            endx = DestX + DestWidth;
            if (endx > Dest->Clip.Right) endx = Dest->Clip.Right;

            startsrcx = X<<8;
            dx = DestX;
            if (dx < Dest->Clip.Left) {
               startsrcx += xScale * (Dest->Clip.Left - dx);
               dx = Dest->Clip.Left;
            }

            // Setup vertical parameters

            srcy = Y;
            endy = DestY + DestHeight;
            if (endy > Dest->Clip.Bottom) endy = Dest->Clip.Bottom;
            y = DestY;

            if (y < Dest->Clip.Top) {
               srcy += yScale * (Dest->Clip.Top - DestY);
               y = Dest->Clip.Top;
            }

            for (; y < endy; y++, srcy += yScale) {
               calcy = (srcy - (LONG)srcy) * 255; // Calculate the fractional part and upscale it to an integer between 0 and 255
               diffytable = glAlphaLookup + ((255 - calcy)<<8); // Reverse the fractional value, convert to lookup table
               ytable = glAlphaLookup + (calcy<<8);

               isrcy = srcy + Bitmap->YOffset;
               if (y < endy-(DestHeight>>1)) isrcy2 = isrcy-1;
               else isrcy2 = isrcy+1;

               srcdata  = Bitmap->Data + (Bitmap->LineWidth * isrcy);
               destdata = Dest->Data + (Dest->LineWidth * (Dest->YOffset + y)) +
                          (Dest->BytesPerPixel * (dx + Dest->XOffset));

               for (x=dx, srcx=startsrcx; x < endx; x++, srcx += xScale) {
                  calcx = srcx & 0xff; // Get the fractional part 0.XX, keep it upscaled to an integer between 0 and 255
                  diffxtable  = glAlphaLookup + ((255 - calcx)<<8); // Reverse the fractional value, use lookup table for speed
                  xtable = glAlphaLookup + (calcx<<8);

                  // Get 2x2 pixels from the source point

                  isrcx = (srcx>>8) + Bitmap->XOffset;
                  bytex = isrcx * Bitmap->BytesPerPixel;

                  if ((isrcy >= srcclip.Top) and (isrcy < srcclip.Bottom)) {
                     if ((isrcx >= srcclip.Left) and (isrcx < srcclip.Right)) Bitmap->ReadUCRIndex(Bitmap, srcdata + bytex, &rgb[0]);
                     else rgb[0] = background;

                     if (x < endx-(DestWidth)) {
                        if ((isrcx-1 >= srcclip.Left) and (isrcx-1 < srcclip.Right)) {
                           Bitmap->ReadUCRIndex(Bitmap, srcdata + bytex - Bitmap->BytesPerPixel, &rgb[1]);
                        }
                        else { rgb[1] = rgb[0]; if (!(Flags & CSTF_CLAMP)) rgb[1].Alpha = 0; }
                     }
                     else {
                        if ((isrcx+1 >= srcclip.Left) and (isrcx+1 < srcclip.Right)) {
                           Bitmap->ReadUCRIndex(Bitmap, srcdata + bytex + Bitmap->BytesPerPixel, &rgb[1]);
                        }
                        else { rgb[1] = rgb[0]; if (!(Flags & CSTF_CLAMP)) rgb[1].Alpha = 0; }
                     }
                  }
                  else rgb[0] = rgb[1] = background;

                  if ((isrcy2 >= srcclip.Top) and (isrcy2 < srcclip.Bottom)) {
                     if ((isrcx >= srcclip.Left) and (isrcx < srcclip.Right)) {
                        Bitmap->ReadUCRIndex(Bitmap, srcdata + Bitmap->LineWidth + bytex, &rgb[2]);
                     }
                     else rgb[2] = background;

                     if (x < endx-(DestWidth)) {
                        if ((isrcx-1 >= srcclip.Left) and (isrcx-1 < srcclip.Right)) {
                           Bitmap->ReadUCRIndex(Bitmap, srcdata + Bitmap->LineWidth + bytex - Bitmap->BytesPerPixel, &rgb[3]);
                        }
                        else { rgb[3] = rgb[2]; if (!(Flags & CSTF_CLAMP)) rgb[3].Alpha = 0; }
                     }
                     else {
                        if ((isrcx+1 >= srcclip.Left) and (isrcx+1 < srcclip.Right)) {
                           Bitmap->ReadUCRIndex(Bitmap, srcdata + Bitmap->LineWidth + bytex + Bitmap->BytesPerPixel, &rgb[3]);
                        }
                        else { rgb[3] = rgb[2]; if (!(Flags & CSTF_CLAMP)) rgb[3].Alpha = 0; }
                     }
                  }
                  else if (Bitmap->BkgdRGB.Alpha) {
                     rgb[2] = rgb[3] = background;
                  }
                  else {
                     rgb[2] = rgb[0];
                     rgb[3] = rgb[1];
                  }

                  // Interpolate the 4 values using our lookup tables (to avoid slow multiplications)

                  RGB8 drgb;
                  drgb.Red   = diffxtable[diffytable[rgb[0].Red]   + ytable[rgb[2].Red]]   + xtable[diffytable[rgb[1].Red]   + ytable[rgb[3].Red]];
                  drgb.Green = diffxtable[diffytable[rgb[0].Green] + ytable[rgb[2].Green]] + xtable[diffytable[rgb[1].Green] + ytable[rgb[3].Green]];
                  drgb.Blue  = diffxtable[diffytable[rgb[0].Blue]  + ytable[rgb[2].Blue]]  + xtable[diffytable[rgb[1].Blue]  + ytable[rgb[3].Blue]];
                  drgb.Alpha = diffxtable[diffytable[rgb[0].Alpha] + ytable[rgb[2].Alpha]] + xtable[diffytable[rgb[1].Alpha] + ytable[rgb[3].Alpha]];

                  Dest->DrawUCRIndex(Dest, destdata, &drgb);
                  destdata += Dest->BytesPerPixel;
               }
            }
         }
         else {
            DOUBLE fx, fy, xScale, yScale;
            ClipRectangle clip;
            ULONG pixel;

            // Nearest neighbour: Fast resize, poor quality

            xScale = (DOUBLE)Width / (DOUBLE)DestWidth;
            yScale = (DOUBLE)Height / (DOUBLE)DestHeight;
            clip = Bitmap->Clip;
            clip.Left   += Bitmap->XOffset;
            clip.Right  += Bitmap->XOffset;
            clip.Top    += Bitmap->YOffset;
            clip.Bottom += Bitmap->YOffset;

            fy = Y;
            for (y=DestY; y < DestY + DestHeight; y++, fy += yScale) {
               if (y < Dest->Clip.Top) continue;
               if (y >= Dest->Clip.Bottom) break;

               fx = X;
               isrcy = Bitmap->YOffset + F2I(fy);
               for (x=DestX; x < DestX + DestWidth; x++, fx += xScale) {
                  if (x < Dest->Clip.Left) continue;
                  if (x >= Dest->Clip.Right) break;

                  isrcx = Bitmap->XOffset + F2I(fx);
                  if ((isrcx >= clip.Left) and (isrcy >= clip.Top) and
                      (isrcx < clip.Right) and (isrcy < clip.Bottom)) {
                     pixel = Bitmap->ReadUCPixel(Bitmap, isrcx, isrcy);
                     Dest->DrawUCPixel(Dest, Dest->XOffset + x, Dest->YOffset + y, pixel);
                  }
                  else {
                     //Dest->DrawUCPixel(Dest, Dest->XOffset + x, Dest->YOffset + y, colour);
                  }
               }
            }
         }

         UnlockSurface(Dest);
      }
      UnlockSurface(Bitmap);
   }

   return ERR_Okay;
}

/*****************************************************************************

-FUNCTION-
CopySurface: Copies graphics data from an arbitrary surface to a bitmap.

This function will copy data from a described surface to a destination bitmap object.  You are required to provide the
function with a full description of the source in a BitmapSurface structure.

The X, Y, Width and Height parameters define the area from the source that you wish to copy.  The XDest and
YDest parameters define the top left corner that you will blit the graphics to in the destination.

-INPUT-
struct(*BitmapSurface) Surface: Description of the surface source.
obj(Bitmap) Bitmap: Destination bitmap.
int(CSRF) Flags:  Optional flags.
int X:      Horizontal source coordinate.
int Y:      Vertical source coordinate.
int Width:  Source width.
int Height: Source height.
int XDest:  Horizontal destination coordinate.
int YDest:  Vertical destination coordinate.

-ERRORS-
Okay:
Args:
NullArgs:

*****************************************************************************/

#define UnpackSRed(a,b)   ((((b) >> (a)->Format.RedPos)   & (a)->Format.RedMask) << (a)->Format.RedShift)
#define UnpackSGreen(a,b) ((((b) >> (a)->Format.GreenPos) & (a)->Format.GreenMask) << (a)->Format.GreenShift)
#define UnpackSBlue(a,b)  ((((b) >> (a)->Format.BluePos)  & (a)->Format.BlueMask) << (a)->Format.BlueShift)
#define UnpackSAlpha(a,b) ((((b) >> (a)->Format.AlphaPos) & (a)->Format.AlphaMask))

static ULONG read_surface8(BITMAPSURFACE *Surface, WORD X, WORD Y)
{
   return ((UBYTE *)Surface->Data)[(Surface->LineWidth * Y) + X];
}

static ULONG read_surface16(BITMAPSURFACE *Surface, WORD X, WORD Y)
{
   return ((UWORD *)((BYTE *)Surface->Data + (Y * Surface->LineWidth) + X + X))[0];
}

static ULONG read_surface_lsb24(BITMAPSURFACE *Surface, WORD X, WORD Y)
{
   UBYTE *data;
   data = (UBYTE *)Surface->Data + (Surface->LineWidth * Y) + (X + X + X);
   return (data[2]<<16) | (data[1]<<8) | data[0];
}

static ULONG read_surface_msb24(BITMAPSURFACE *Surface, WORD X, WORD Y)
{
   UBYTE *data;
   data = (UBYTE *)Surface->Data + (Surface->LineWidth * Y) + (X + X + X);
   return (data[0]<<16) | (data[1]<<8) | data[2];
}

static ULONG read_surface32(BITMAPSURFACE *Surface, WORD X, WORD Y)
{
   return ((ULONG *)((UBYTE *)Surface->Data + (Surface->LineWidth * Y) + (X<<2)))[0];
}

static ERROR gfxCopySurface(BITMAPSURFACE *Surface, objBitmap *Bitmap,
          LONG Flags, LONG X, LONG Y, LONG Width, LONG Height,
          LONG XDest, LONG YDest)
{
   parasol::Log log(__FUNCTION__);
   RGB8 pixel, src;
   UBYTE *srctable, *desttable;
   LONG i;
   WORD srcwidth;
   ULONG colour;
   UBYTE *data, *srcdata;
   ULONG (*read_surface)(BITMAPSURFACE *, WORD, WORD);

   if ((!Surface) or (!Bitmap)) return log.warning(ERR_NullArgs);

   if ((!Surface->Data) or (Surface->LineWidth < 1) or (!Surface->BitsPerPixel)) {
      return log.warning(ERR_Args);
   }

   srcwidth = Surface->LineWidth / Surface->BytesPerPixel;

   // Check if the destination that we are copying to is within the drawable area.

   if ((XDest < Bitmap->Clip.Left)) {
      Width = Width - (Bitmap->Clip.Left - X);
      if (Width < 1) return ERR_Okay;
      X = X + (Bitmap->Clip.Left - X);
      XDest = Bitmap->Clip.Left;
   }
   else if (XDest >= Bitmap->Clip.Right) return ERR_Okay;

   if ((YDest < Bitmap->Clip.Top)) {
      Height = Height - (Bitmap->Clip.Top - YDest);
      if (Height < 1) return ERR_Okay;
      Y = Y + (Bitmap->Clip.Top - YDest);
      YDest = Bitmap->Clip.Top;
   }
   else if (YDest >= Bitmap->Clip.Bottom) return ERR_Okay;

   // Check if the source that we are blitting from is within its own drawable area.

   if (Flags & CSRF_CLIP) {
      if (X < 0) {
         if ((Width += X) < 1) return ERR_Okay;
         X = 0;
      }
      else if (X >= srcwidth) return ERR_Okay;

      if (Y < 0) {
         if ((Height += Y) < 1) return ERR_Okay;
         Y = 0;
      }
      else if (Y >= Surface->Height) return ERR_Okay;
   }

   // Clip the width and height

   if ((XDest + Width)  >= Bitmap->Clip.Right)  Width  = Bitmap->Clip.Right - XDest;
   if ((YDest + Height) >= Bitmap->Clip.Bottom) Height = Bitmap->Clip.Bottom - YDest;

   if (Flags & CSRF_CLIP) {
      if ((X + Width)  >= Surface->Clip.Right)  Width  = Surface->Clip.Right - X;
      if ((Y + Height) >= Surface->Clip.Bottom) Height = Surface->Clip.Bottom - Y;
   }

   if (Width < 1) return ERR_Okay;
   if (Height < 1) return ERR_Okay;

   // Adjust coordinates by offset values

   if (Flags & CSRF_OFFSET) {
      X += Surface->XOffset;
      Y += Surface->YOffset;
   }

   XDest += Bitmap->XOffset;
   YDest += Bitmap->YOffset;

   if (Flags & CSRF_DEFAULT_FORMAT) gfxGetColourFormat(&Surface->Format, Surface->BitsPerPixel, 0, 0, 0, 0);;

   switch(Surface->BytesPerPixel) {
      case 1: read_surface = read_surface8; break;
      case 2: read_surface = read_surface16; break;
      case 3: if (Surface->Format.RedPos IS 16) read_surface = read_surface_lsb24;
              else read_surface = read_surface_msb24;
              break;
      case 4: read_surface = read_surface32; break;
      default: return log.warning(ERR_Args);
   }

#ifdef __xwindows__

   // Use this routine if the destination is a pixmap (write only memory).  X11 windows are always represented as pixmaps.

   if (Bitmap->x11.drawable) {
      // Source is an ximage, destination is a pixmap.  NB: If DGA is enabled, we will avoid using these routines because mem-copying from software
      // straight to video RAM is a lot faster.

      XImage ximage;
      WORD alignment;

      if (Bitmap->LineWidth & 0x0001) alignment = 8;
      else if (Bitmap->LineWidth & 0x0002) alignment = 16;
      else alignment = 32;

      ximage.width            = Surface->LineWidth / Surface->BytesPerPixel;  // Image width
      ximage.height           = Surface->Height; // Image height
      ximage.xoffset          = 0;               // Number of pixels offset in X direction
      ximage.format           = ZPixmap;         // XYBitmap, XYPixmap, ZPixmap
      ximage.data             = (char *)Surface->Data;   // Pointer to image data
      ximage.byte_order       = 0;               // LSBFirst / MSBFirst
      ximage.bitmap_unit      = alignment;       // Quant. of scanline - 8, 16, 32
      ximage.bitmap_bit_order = 0;               // LSBFirst / MSBFirst
      ximage.bitmap_pad       = alignment;       // 8, 16, 32, either XY or Zpixmap
      if (Surface->BitsPerPixel IS 32) ximage.depth = 24;
      else ximage.depth = Surface->BitsPerPixel;            // Actual bits per pixel
      ximage.bytes_per_line   = Surface->LineWidth;         // Accelerator to next line
      ximage.bits_per_pixel   = Surface->BytesPerPixel * 8; // Bits per pixel-group
      ximage.red_mask         = 0;
      ximage.green_mask       = 0;
      ximage.blue_mask        = 0;
      XInitImage(&ximage);

      XPutImage(XDisplay, Bitmap->x11.drawable, glXGC,
         &ximage, X, Y, XDest, YDest, Width, Height);

      return ERR_Okay;
   }

#endif // __xwindows__

   if (LockSurface(Bitmap, SURFACE_WRITE) IS ERR_Okay) {
      if ((Flags & CSRF_ALPHA) and (Surface->BitsPerPixel IS 32)) { // 32-bit alpha blending support
         ULONG *sdata = (ULONG *)((BYTE *)Surface->Data + (Y * Surface->LineWidth) + (X<<2));

         if (Bitmap->BitsPerPixel IS 32) {
            ULONG *ddata = (ULONG *)(Bitmap->Data + (YDest * Bitmap->LineWidth) + (XDest<<2));
            while (Height > 0) {
               for (LONG i=0; i < Width; i++) {
                  colour = sdata[i];

                  UBYTE alpha = ((UBYTE)(colour >> Surface->Format.AlphaPos));
                  alpha = (glAlphaLookup + (alpha<<8))[Surface->Opacity]; // Multiply the source pixel by overall translucency level

                  if (alpha >= BLEND_MAX_THRESHOLD) ddata[i] = colour;
                  else if (alpha >= BLEND_MIN_THRESHOLD) {

                     UBYTE red   = colour >> Surface->Format.RedPos;
                     UBYTE green = colour >> Surface->Format.GreenPos;
                     UBYTE blue  = colour >> Surface->Format.BluePos;

                     colour = ddata[i];
                     UBYTE destred   = colour >> Bitmap->prvColourFormat.RedPos;
                     UBYTE destgreen = colour >> Bitmap->prvColourFormat.GreenPos;
                     UBYTE destblue  = colour >> Bitmap->prvColourFormat.BluePos;

                     srctable  = glAlphaLookup + (alpha<<8);
                     desttable = glAlphaLookup + ((255-alpha)<<8);
                     ddata[i] = PackPixelWBA(Bitmap, srctable[red] + desttable[destred],
                                                  srctable[green] + desttable[destgreen],
                                                  srctable[blue] + desttable[destblue],
                                                  255);
                  }
               }
               sdata = (ULONG *)(((UBYTE *)sdata) + Surface->LineWidth);
               ddata = (ULONG *)(((UBYTE *)ddata) + Bitmap->LineWidth);
               Height--;
            }
         }
         else while (Height > 0) {
            for (LONG i=0; i < Width; i++) {
               colour = sdata[i];
               UBYTE alpha = ((UBYTE)(colour >> Surface->Format.AlphaPos));
               alpha = (glAlphaLookup + (alpha<<8))[Surface->Opacity]; // Multiply the source pixel by overall translucency level

               if (alpha >= BLEND_MAX_THRESHOLD) {
                  pixel.Red   = colour >> Surface->Format.RedPos;
                  pixel.Green = colour >> Surface->Format.GreenPos;
                  pixel.Blue  = colour >> Surface->Format.BluePos;
                  Bitmap->DrawUCRPixel(Bitmap, XDest + i, YDest, &pixel);
               }
               else if (alpha >= BLEND_MIN_THRESHOLD) {
                  UBYTE red   = colour >> Surface->Format.RedPos;
                  UBYTE green = colour >> Surface->Format.GreenPos;
                  UBYTE blue  = colour >> Surface->Format.BluePos;

                  srctable  = glAlphaLookup + (alpha<<8);
                  desttable = glAlphaLookup + ((255-alpha)<<8);

                  Bitmap->ReadUCRPixel(Bitmap, XDest + i, YDest, &pixel);
                  pixel.Red   = srctable[red]   + desttable[pixel.Red];
                  pixel.Green = srctable[green] + desttable[pixel.Green];
                  pixel.Blue  = srctable[blue]  + desttable[pixel.Blue];
                  Bitmap->DrawUCRPixel(Bitmap, XDest + i, YDest, &pixel);
               }
            }
            sdata = (ULONG *)(((UBYTE *)sdata) + Surface->LineWidth);
            YDest++;
            Height--;
         }
      }
      else if (Flags & CSRF_TRANSPARENT) {
         // Transparent colour blitting

         if ((Flags & CSRF_TRANSLUCENT) and (Surface->Opacity < 255)) {
            // Transparent mask with translucent pixels

            srctable  = glAlphaLookup + (Surface->Opacity<<8);
            desttable = glAlphaLookup + ((255-Surface->Opacity)<<8);

            while (Height > 0) {
               for (LONG i=0; i < Width; i++) {
                  colour = read_surface(Surface, X + i, Y);
                  if (colour != (ULONG)Surface->Colour) {
                     Bitmap->ReadUCRPixel(Bitmap, XDest + i, YDest, &pixel);

                     pixel.Red   = srctable[UnpackSRed(Surface, colour)]   + desttable[pixel.Red];
                     pixel.Green = srctable[UnpackSGreen(Surface, colour)] + desttable[pixel.Green];
                     pixel.Blue  = srctable[UnpackSBlue(Surface, colour)]  + desttable[pixel.Blue];

                     Bitmap->DrawUCRPixel(Bitmap, XDest + i, YDest, &pixel);
                  }
               }
               Y++; YDest++;
               Height--;
            }
         }
         else if (Surface->BitsPerPixel IS Bitmap->BitsPerPixel) {
            if (Surface->BytesPerPixel IS 4) {
               ULONG *sdata = (ULONG *)((BYTE *)Surface->Data + (Y * Surface->LineWidth) + (X<<2));
               ULONG *ddata = (ULONG *)(Bitmap->Data + (YDest * Bitmap->LineWidth) + (XDest<<2));
               colour = Surface->Colour;
               while (Height > 0) {
                  for (i=0; i < Width; i++) if (sdata[i] != colour) ddata[i] = sdata[i];
                  ddata = (ULONG *)(((BYTE *)ddata) + Bitmap->LineWidth);
                  sdata = (ULONG *)(((BYTE *)sdata) + Surface->LineWidth);
                  Height--;
               }
            }
            else if (Surface->BytesPerPixel IS 2) {
               UWORD *ddata, *sdata;

               sdata = (UWORD *)((BYTE *)Surface->Data + (Y * Surface->LineWidth) + (X<<1));
               ddata = (UWORD *)(Bitmap->Data + (YDest * Bitmap->LineWidth) + (XDest<<1));
               colour = Surface->Colour;
               while (Height > 0) {
                  for (i=0; i < Width; i++) if (sdata[i] != colour) ddata[i] = sdata[i];
                  ddata = (UWORD *)(((BYTE *)ddata) + Bitmap->LineWidth);
                  sdata = (UWORD *)(((BYTE *)sdata) + Surface->LineWidth);
                  Height--;
               }
            }
            else {
               while (Height > 0) {
                  for (i=0; i < Width; i++) {
                     colour = read_surface(Surface, X + i, Y);
                     if (colour != (ULONG)Surface->Colour) Bitmap->DrawUCPixel(Bitmap, XDest + i, YDest, colour);
                  }
                  Y++; YDest++;
                  Height--;
               }
            }
         }
         else {
            while (Height > 0) {
               for (i=0; i < Width; i++) {
                  colour = read_surface(Surface, X + i, Y);
                  if (colour != (ULONG)Surface->Colour) {
                     pixel.Red   = UnpackSRed(Surface, colour);
                     pixel.Green = UnpackSGreen(Surface, colour);
                     pixel.Blue  = UnpackSBlue(Surface, colour);
                     Bitmap->DrawUCRPixel(Bitmap, XDest + i, YDest, &pixel);
                  }
               }
               Y++; YDest++;
               Height--;
            }
         }
      }
      else { // Straight copy operation
         if ((Flags & CSRF_TRANSLUCENT) and (Surface->Opacity < 255)) { // Straight translucent blit
            srctable  = glAlphaLookup + (Surface->Opacity<<8);
            desttable = glAlphaLookup + ((255-Surface->Opacity)<<8);

            if ((Surface->BytesPerPixel IS 4) and (Bitmap->BytesPerPixel IS 4)) {
               ULONG *ddata, *sdata;

               sdata = (ULONG *)((BYTE *)Surface->Data + (Y * Surface->LineWidth) + (X<<2));
               ddata = (ULONG *)(Bitmap->Data + (YDest * Bitmap->LineWidth) + (XDest<<2));
               while (Height > 0) {
                  for (LONG i=0; i < Width; i++) {
                     ddata[i] = ((srctable[(UBYTE)(sdata[i]>>Surface->Format.RedPos)]   + desttable[(UBYTE)(ddata[i]>>Bitmap->prvColourFormat.RedPos)]) << Bitmap->prvColourFormat.RedPos) |
                                ((srctable[(UBYTE)(sdata[i]>>Surface->Format.GreenPos)] + desttable[(UBYTE)(ddata[i]>>Bitmap->prvColourFormat.GreenPos)]) << Bitmap->prvColourFormat.GreenPos) |
                                ((srctable[(UBYTE)(sdata[i]>>Surface->Format.BluePos)]  + desttable[(UBYTE)(ddata[i]>>Bitmap->prvColourFormat.BluePos)]) << Bitmap->prvColourFormat.BluePos);
                  }
                  ddata = (ULONG *)(((BYTE *)ddata) + Bitmap->LineWidth);
                  sdata = (ULONG *)(((BYTE *)sdata) + Surface->LineWidth);
                  Height--;
               }
            }
            else if ((Surface->BytesPerPixel IS 2) and (Bitmap->BytesPerPixel IS 2)) {
               UWORD *ddata, *sdata;

               sdata = (UWORD *)((BYTE *)Surface->Data + (Y * Surface->LineWidth) + (X<<1));
               ddata = (UWORD *)(Bitmap->Data + (YDest * Bitmap->LineWidth) + (XDest<<1));
               while (Height > 0) {
                  for (i=0; i < Width; i++) {
                     ddata[i] = PackPixel(Bitmap, srctable[UnpackSRed(Surface, sdata[i])] + desttable[UnpackRed(Bitmap, ddata[i])],
                                                  srctable[UnpackSGreen(Surface, sdata[i])] + desttable[UnpackGreen(Bitmap, ddata[i])],
                                                  srctable[UnpackSBlue(Surface, sdata[i])] + desttable[UnpackBlue(Bitmap, ddata[i])]);
                  }
                  ddata = (UWORD *)(((BYTE *)ddata) + Bitmap->LineWidth);
                  sdata = (UWORD *)(((BYTE *)sdata) + Surface->LineWidth);
                  Height--;
               }
            }
            else while (Height > 0) {
               for (LONG i=0; i < Width; i++) {
                  colour = read_surface(Surface, X + i, Y);
                  src.Red   = UnpackSRed(Surface, colour);
                  src.Green = UnpackSGreen(Surface, colour);
                  src.Blue  = UnpackSBlue(Surface, colour);

                  Bitmap->ReadUCRPixel(Bitmap, XDest + i, YDest, &pixel);

                  pixel.Red   = srctable[src.Red]   + desttable[pixel.Red];
                  pixel.Green = srctable[src.Green] + desttable[pixel.Green];
                  pixel.Blue  = srctable[src.Blue]  + desttable[pixel.Blue];

                  Bitmap->DrawUCRPixel(Bitmap, XDest + i, YDest, &pixel);
               }
               Y++; YDest++;
               Height--;
            }
         }
         else if (Surface->BitsPerPixel IS Bitmap->BitsPerPixel) {
            // Use this fast routine for identical bitmaps

            srcdata = (UBYTE *)Surface->Data + (X * Surface->BytesPerPixel) + (Y * Surface->LineWidth);
            data    = Bitmap->Data + (XDest  * Bitmap->BytesPerPixel) + (YDest * Bitmap->LineWidth);
            Width   = Width * Surface->BytesPerPixel;

            while (Height > 0) {
               for (i=0; (size_t)i > sizeof(LONG); i += sizeof(LONG)) {
                  ((LONG *)(data+i))[0] = ((LONG *)(srcdata+i))[0];
               }
               while (i < Width) { data[i] = srcdata[i]; i++; }
               srcdata += Surface->LineWidth;
               data    += Bitmap->LineWidth;
               Height--;
            }
         }
         else {
            // If the bitmaps do not match then we need to use this slower RGB translation subroutine.

            while (Height > 0) {
               for (LONG i=0; i < Width; i++) {
                  colour = read_surface(Surface, X + i, Y);
                  src.Red   = UnpackSRed(Surface, colour);
                  src.Green = UnpackSGreen(Surface, colour);
                  src.Blue  = UnpackSBlue(Surface, colour);
                  Bitmap->DrawUCRPixel(Bitmap, XDest + i, YDest, &src);
               }
               Y++; YDest++;
               Height--;
            }
         }
      }

      UnlockSurface(Bitmap);
   }

   return ERR_Okay;
}

/*****************************************************************************

-FUNCTION-
Compress: Compresses bitmap data to save memory.

A bitmap can be compressed with the Compress() function to save memory when the bitmap is not in use.  This is useful
if a large bitmap needs to be stored in memory and it is anticipated that the bitmap will be used infrequently.

Once a bitmap is compressed, its image data is invalid.  Any attempt to access the bitmap image data will likely
result in a memory access fault.  The image data will remain invalid until the ~Decompress() function is
called to restore the bitmap to its original state.

The BMF_COMPRESSED bit will be set in the Flags field after a successful call to this function to indicate that the
bitmap is compressed.

-INPUT-
obj(Bitmap) Bitmap: Pointer to the @Bitmap that will be compressed.
int Level: Level of compression.  Zero uses a default setting (recommended), the maximum is 10.

-ERRORS-
Okay
Args
AllocMemory
ReallocMemory
CreateObject: A Compression object could not be created.

*****************************************************************************/

static ERROR gfxCompress(objBitmap *Bitmap, LONG Level)
{
   return ActionTags(MT_BmpCompress, Bitmap, Level);
}

/*****************************************************************************

-FUNCTION-
Decompress: Decompresses a compressed bitmap.

The Decompress() function is used to restore a compressed bitmap to its original state.  If the bitmap is not
compressed, this function does nothing.

By default the original compression data will be terminated, however it can be retained by setting the RetainData
argument to TRUE.  Retaining the data will allow it to be decompressed on consecutive occasions.  Because both the raw
and compressed image data will be held in memory, it is recommended that CompressBitmap is called as soon as possible
with the Altered argument set to FALSE.  This will remove the raw image data from memory while retaining the original
compressed data without starting a recompression process.

-INPUT-
obj(Bitmap) Bitmap: Pointer to the @Bitmap that will be decompressed.
int RetainData:     Retains the compression data if TRUE.

-ERRORS-
Okay
AllocMemory

*****************************************************************************/

static ERROR gfxDecompress(objBitmap *Bitmap, LONG RetainData)
{
   return ActionTags(MT_BmpDecompress, Bitmap, RetainData);
}

/*****************************************************************************

-FUNCTION-
DrawEllipse: Draws an ellipse within a bounding box.

This function will draw an ellipse within the bounding box defined by (X, Y, Width, Height).  If Fill is TRUE then the
ellipse will be filled with Colour.  If FALSE then the border of the ellipse will be drawn at a size of 1 pixel.

-INPUT-
obj(Bitmap) Bitmap: Pointer to the target @Bitmap.
int X:       The left-side of the bounding box.
int Y:       The top-side of the bounding box.
int Width:   The width of the bounding box.
int Height:  The height of the bounding box.
uint Colour: The colour of the ellipse, expressed in packed RGB format.
int Fill:    Set to TRUE to fill the ellipse.

*****************************************************************************/

#define DRAWPIXEL(bmp,x,y,col) \
   if ((x >= clipleft) and (y >= cliptop) and (x < clipright) and (y < clipbottom)) { \
      if (bmp->Opacity < 255) { \
         bmp->ReadUCRPixel(bmp, x, y, &rgb); \
         bmp->DrawUCPixel(bmp, x, y, PackPixel(bmp, srctable[red] + desttable[rgb.Red], srctable[green] + desttable[rgb.Green], srctable[blue]  + desttable[rgb.Blue])); \
      } \
      else bmp->DrawUCPixel(bmp,x,y,col); \
   }

static void gfxDrawEllipse(objBitmap *Bitmap, LONG X, LONG Y, LONG Width, LONG Height, ULONG Colour, LONG Fill)
{
   UBYTE *srctable, *desttable;
   RGB8 rgb;
   LONG rx, ry, cx, cy, clipleft, clipright, cliptop, clipbottom;
   UBYTE red, green, blue;

   if (!Bitmap) return;

   if ((Width < 1) or (Height < 1)) return;

   if ((Bitmap->Clip.Right <= X) or (Bitmap->Clip.Top >= Y+Height) or
       (Bitmap->Clip.Bottom <= Y) or (Bitmap->Clip.Left >= X+Width)) return;

   if (!LockSurface(Bitmap, SURFACE_WRITE)) {

      rx = Width>>1;
      ry = Height>>1;
      cx = X + rx;
      cy = Y + ry;

      if (rx < 1) rx = 1;
      if (ry < 1) ry = 1;

      if (!Fill) {
         LONG t1 = rx * rx, t2 = t1<<1, t3 = t2<<1;
         LONG t4 = ry * ry, t5 = t4<<1, t6 = t5<<1;
         LONG t7 = rx * t5, t8 = t7<<1, t9 = 0;
         LONG d1 = t2 - t7 + (t4>>1);
         LONG d2 = (t1>>1) - t8 + t5;
         WORD x = rx, y = 0;

         clipleft   = Bitmap->Clip.Left  + Bitmap->XOffset;
         clipright  = Bitmap->Clip.Right + Bitmap->XOffset;
         cliptop    = Bitmap->Clip.Top + Bitmap->YOffset;
         clipbottom = Bitmap->Clip.Bottom + Bitmap->YOffset;

         cx += Bitmap->XOffset;
         cy += Bitmap->YOffset;

         red   = UnpackRed(Bitmap, Colour);
         green = UnpackGreen(Bitmap, Colour);
         blue  = UnpackBlue(Bitmap, Colour);

         srctable  = glAlphaLookup + ((Bitmap->Opacity)<<8);
         desttable = glAlphaLookup + ((255 - Bitmap->Opacity)<<8);

         while (d2 < 0) {
            DRAWPIXEL(Bitmap, cx + x, cy + y, Colour);
            DRAWPIXEL(Bitmap, cx + x, cy - y, Colour);
            DRAWPIXEL(Bitmap, cx - x, cy + y, Colour);
            DRAWPIXEL(Bitmap, cx - x, cy - y, Colour);
            y++;
            t9 += t3;
            if (d1 < 0) {
               d1 += t9 + t2;
               d2 += t9;
            }
            else {
               x--;
               t8 -= t6;
               d1 += t9 + t2 - t8;
               d2 += t9 + t5 - t8;
            }
         }

         do {
            DRAWPIXEL(Bitmap, cx + x, cy + y, Colour);
            DRAWPIXEL(Bitmap, cx + x, cy - y, Colour);
            DRAWPIXEL(Bitmap, cx - x, cy + y, Colour);
            DRAWPIXEL(Bitmap, cx - x, cy - y, Colour);
            x--;
            t8 -= t6;
            if (d2 < 0) {
               y++;
               t9 += t3;
               d2 += t9 + t5 - t8;
            }
            else d2 += t5 - t8;
         } while (x>=0);
      }
      else {
         LONG t1 = rx*rx, t2 = t1<<1, t3 = t2<<1;
         LONG t4 = ry*ry, t5 = t4<<1, t6 = t5<<1;
         LONG t7 = rx*t5, t8 = t7<<1, t9 = 0;
         LONG d1 = t2 - t7 + (t4>>1);
         LONG d2 = (t1>>1) - t8 + t5;
         WORD x = rx, y = 0;

         while (d2 < 0) {
            gfxDrawRectangle(Bitmap, cx,   cy+y, x+1, 1, Colour, BAF_FILL);
            gfxDrawRectangle(Bitmap, cx-x, cy+y, x+1, 1, Colour, BAF_FILL);
            gfxDrawRectangle(Bitmap, cx,   cy-y, x+1, 1, Colour, BAF_FILL);
            gfxDrawRectangle(Bitmap, cx-x, cy-y, x+1, 1, Colour, BAF_FILL);
            y++;
            t9 += t3;
            if (d1 < 0) {
               d1 += t9 + t2;
               d2 += t9;
            }
            else {
               x--;
               t8 -= t6;
               d1 += t9 + t2 - t8;
               d2 += t9 + t5 - t8;
            }
         }

         do {
            gfxDrawRectangle(Bitmap, cx+x, cy,   1, y+1, Colour, BAF_FILL);
            gfxDrawRectangle(Bitmap, cx+x, cy-y, 1, y+1, Colour, BAF_FILL);
            gfxDrawRectangle(Bitmap, cx-x, cy,   1, y+1, Colour, BAF_FILL);
            gfxDrawRectangle(Bitmap, cx-x, cy-y, 1, y+1, Colour, BAF_FILL);
            x--;
            t8 -= t6;
            if (d2 < 0) {
               y++;
               t9 += t3;
               d2 += t9 + t5 - t8;
            }
            else d2 += t5 - t8;
         } while (x>=0);
      }

      UnlockSurface(Bitmap);
   }
}

/*****************************************************************************

-FUNCTION-
GetColourFormat: Generates the values for a ColourFormat structure for a given bit depth.

This function will generate the values for a ColourFormat structure, for either a given bit depth or
customised colour bit values.  The ColourFormat structure is used by internal bitmap routines to pack and unpack bit
values to and from bitmap memory.

<pre>
struct ColourFormat {
   UBYTE  RedShift;    // Right shift value (applies only to 15/16 bit formats for eliminating redundant bits)
   UBYTE  BlueShift;
   UBYTE  GreenShift;
   UBYTE  AlphaShift;
   UBYTE  RedMask;     // The unshifted mask value (ranges from 0x00 to 0xff)
   UBYTE  GreenMask;
   UBYTE  BlueMask;
   UBYTE  AlphaMask;
   UBYTE  RedPos;      // Left shift/positional value
   UBYTE  GreenPos;
   UBYTE  BluePos;
   UBYTE  AlphaPos;
};
</pre>

The ColourFormat structure is supported by the following macros for packing and unpacking colour bit values:

<pre>
Colour = CFPackPixel(Format,Red,Green,Blue)
Colour = CFPackPixelA(Format,Red,Green,Blue,Alpha)
Colour = CFPackAlpha(Format,Alpha)
Red    = CFUnpackRed(Format,Colour)
Green  = CFUnpackGreen(Format,Colour)
Blue   = CFUnpackBlue(Format,Colour)
Alpha  = CFUnpackAlpha(Format,Colour)
</pre>

-INPUT-
struct(*ColourFormat) Format: Pointer to an empty ColourFormat structure.
int BitsPerPixel: The depth that you would like to generate colour values for.  Ignored if mask values are set.
int RedMask:      Red component bit mask value.  Set this value to zero if the BitsPerPixel argument is used.
int GreenMask:    Green component bit mask value.
int BlueMask:     Blue component bit mask value.
int AlphaMask:    Alpha component bit mask value.

*****************************************************************************/

static void gfxGetColourFormat(ColourFormat *Format, LONG BPP, LONG RedMask, LONG GreenMask, LONG BlueMask, LONG AlphaMask)
{
   LONG mask;

   //log.function("R: $%.8x G: $%.8x, B: $%.8x, A: $%.8x", RedMask, GreenMask, BlueMask, AlphaMask);

   if (!RedMask) {
      if (BPP IS 15) {
         RedMask   = 0x7c00;
         GreenMask = 0x03e0;
         BlueMask  = 0x001f;
         AlphaMask = 0x0000;
      }
      else if (BPP IS 16) {
         RedMask   = 0xf800;
         GreenMask = 0x07e0;
         BlueMask  = 0x001f;
         AlphaMask = 0x0000;
      }
      else {
         BPP = 32;
         AlphaMask = 0xff000000;
         RedMask   = 0x00ff0000;
         GreenMask = 0x0000ff00;
         BlueMask  = 0x000000ff;
      }
   }

   // Calculate the lower byte mask and the position (left shift) of the colour

   mask = RedMask;
   Format->RedPos = 0;
   Format->RedShift = 0;
   while ((mask) and (!(mask & 1))) { mask = mask>>1; Format->RedPos++; }
   Format->RedMask = mask;
   for (mask=0x80; (mask) and (!(mask & Format->RedMask)); mask=mask>>1) Format->RedShift++;

   mask = BlueMask;
   Format->BluePos = 0;
   Format->BlueShift = 0;
   while ((mask) and (!(mask & 1))) { mask = mask>>1; Format->BluePos++; }
   Format->BlueMask = mask;
   for (mask=0x80; (mask) and (!(mask & Format->BlueMask)); mask=mask>>1) Format->BlueShift++;

   mask = GreenMask;
   Format->GreenPos = 0;
   Format->GreenShift = 0;
   while ((mask) and (!(mask & 1))) { mask = mask>>1; Format->GreenPos++; }
   Format->GreenMask = mask;
   for (mask=0x80; (mask) and (!(mask & Format->GreenMask)); mask=mask>>1) Format->GreenShift++;

   mask = AlphaMask;
   Format->AlphaPos = 0;
   Format->AlphaShift = 0;
   while ((mask) and (!(mask & 1))) { mask = mask>>1; Format->AlphaPos++; }
   Format->AlphaMask = mask;
   for (mask=0x80; (mask) and (!(mask & Format->AlphaMask)); mask=mask>>1) Format->AlphaShift++;

   Format->BitsPerPixel = BPP;
}

/*****************************************************************************

-FUNCTION-
DrawLine: Draws a line to a bitmap.

This function will draw a line using a bitmap colour value.  The line will start from the position determined by
(X, Y) and end at (EndX, EndY) inclusive.  Hardware acceleration will be used to draw the line if available.

The opacity of the line is determined by the value in the Opacity field of the target bitmap.

-INPUT-
obj(Bitmap) Bitmap: The target bitmap.
int X: X-axis starting position.
int Y: Y-axis starting position.
int XEnd: X-axis end position.
int YEnd: Y-axis end position.
uint Colour: The pixel colour for drawing the line.

*****************************************************************************/

static void gfxDrawLine(objBitmap *Bitmap, LONG X, LONG Y, LONG EndX, LONG EndY, ULONG Colour)
{
   RGB8 pixel, rgb;
   LONG i, dx, dy, l, m, x_inc, y_inc;
   LONG err_1, dx2, dy2;
   LONG drawx, drawy, clipleft, clipright, clipbottom, cliptop;
   ULONG colour;

   if (Bitmap->Opacity < 1) return;

   #ifdef __xwindows__
      if ((Bitmap->DataFlags & (MEM_VIDEO|MEM_TEXTURE)) and (Bitmap->Opacity >= 255)) {
         XRectangle rectangles;
         rectangles.x      = Bitmap->Clip.Left + Bitmap->XOffset;
         rectangles.y      = Bitmap->Clip.Top + Bitmap->YOffset;
         rectangles.width  = Bitmap->Clip.Right + Bitmap->XOffset - rectangles.x;
         rectangles.height = Bitmap->Clip.Bottom + Bitmap->YOffset - rectangles.y;
         XSetClipRectangles(XDisplay, glClipXGC, 0, 0, &rectangles, 1, YXSorted);

         XSetForeground(XDisplay, glClipXGC, Colour);
         XDrawLine(XDisplay, Bitmap->x11.drawable, glClipXGC, X + Bitmap->XOffset, Y + Bitmap->YOffset, EndX + Bitmap->XOffset, EndY + Bitmap->YOffset);
         return;
      }
   #endif

   rgb.Red   = UnpackRed(Bitmap, Colour);
   rgb.Green = UnpackGreen(Bitmap, Colour);
   rgb.Blue  = UnpackBlue(Bitmap, Colour);

   #ifdef _WIN32

      if ((Bitmap->prvAFlags & BF_WINVIDEO) and (Bitmap->Opacity >= 255)) {
         winSetClipping(Bitmap->win.Drawable, Bitmap->Clip.Left + Bitmap->XOffset, Bitmap->Clip.Top + Bitmap->YOffset,
            Bitmap->Clip.Right + Bitmap->XOffset, Bitmap->Clip.Bottom + Bitmap->YOffset);
         winDrawLine(Bitmap->win.Drawable, X + Bitmap->XOffset, Y + Bitmap->YOffset,
            EndX + Bitmap->XOffset, EndY + Bitmap->YOffset, &rgb.Red);
         winSetClipping(Bitmap->win.Drawable, 0, 0, 0, 0);

         return;
      }

   #endif

   if (LockSurface(Bitmap, SURFACE_READWRITE) != ERR_Okay) return;

   drawx = X + Bitmap->XOffset;
   drawy = Y + Bitmap->YOffset;
   dx    = ((EndX + Bitmap->XOffset) - (X + Bitmap->XOffset));
   dy    = ((EndY + Bitmap->YOffset) - (Y + Bitmap->YOffset));
   x_inc = (dx < 0) ? -1 : 1;
   if (dx < 0) l = -dx;
   else l = dx;
   y_inc = (dy < 0) ? -1 : 1;
   if (dy < 0) m = -dy;
   else m = dy;
   dx2   = l << 1;
   dy2   = m << 1;

   cliptop    = Bitmap->Clip.Top    + Bitmap->YOffset;
   clipbottom = Bitmap->Clip.Bottom + Bitmap->YOffset;
   clipleft   = Bitmap->Clip.Left   + Bitmap->XOffset;
   clipright  = Bitmap->Clip.Right  + Bitmap->XOffset;

   if (Bitmap->Opacity < 255) {
      // Translucent routine

      if ((l >= m)) {
         err_1 = dy2 - l;
         for (i = 0; i < l; i++) {
            if ((drawx >= clipleft) and (drawx < clipright) and (drawy >= cliptop) and (drawy < clipbottom))  {
               Bitmap->ReadUCRPixel(Bitmap, drawx, drawy, &pixel);
               pixel.Red   = rgb.Red   + (((pixel.Red   - rgb.Red)   * (255 - Bitmap->Opacity))>>8);
               pixel.Green = rgb.Green + (((pixel.Green - rgb.Green) * (255 - Bitmap->Opacity))>>8);
               pixel.Blue  = rgb.Blue  + (((pixel.Blue  - rgb.Blue)  * (255 - Bitmap->Opacity))>>8);
               pixel.Alpha = 255;
               Bitmap->DrawUCRPixel(Bitmap, drawx, drawy, &pixel);
            }
            if (err_1 > 0) { drawy += y_inc; err_1 -= dx2; }
            err_1 += dy2;
            drawx += x_inc;
         }
      }
      else {
         err_1 = dx2 - m;
         for (i = 0; i < m; i++) {
            if ((drawx >= clipleft) and (drawx < clipright) and (drawy >= cliptop) and (drawy < clipbottom))  {
               Bitmap->ReadUCRPixel(Bitmap, drawx, drawy, &pixel);
               pixel.Red   = rgb.Red   + (((pixel.Red   - rgb.Red)   * (255 - Bitmap->Opacity))>>8);
               pixel.Green = rgb.Green + (((pixel.Green - rgb.Green) * (255 - Bitmap->Opacity))>>8);
               pixel.Blue  = rgb.Blue  + (((pixel.Blue  - rgb.Blue)  * (255 - Bitmap->Opacity))>>8);
               pixel.Alpha = 255;
               Bitmap->DrawUCRPixel(Bitmap, drawx, drawy, &pixel);
            }
            if (err_1 > 0) { drawx += x_inc; err_1 -= dy2; }
            err_1 += dx2;
            drawy += y_inc;
         }
      }

      if ((drawx >= clipleft) and (drawx < clipright) and (drawy >= cliptop) and (drawy < clipbottom))  {
         Bitmap->ReadUCRPixel(Bitmap, drawx, drawy, &pixel);
         pixel.Red   = rgb.Red   + (((pixel.Red   - rgb.Red) * (255 - Bitmap->Opacity))>>8);
         pixel.Green = rgb.Green + (((pixel.Green - rgb.Green) * (255 - Bitmap->Opacity))>>8);
         pixel.Blue  = rgb.Blue  + (((pixel.Blue  - rgb.Blue) * (255 - Bitmap->Opacity))>>8);
         pixel.Alpha = 255;
         Bitmap->DrawUCRPixel(Bitmap, drawx, drawy, &pixel);
      }
   }
   else {
      colour = Colour;

      if (l >= m) {
         err_1 = dy2 - l;
         for (i = 0; i < l; i++) {
            if ((drawx >= clipleft) and (drawx < clipright) and (drawy >= cliptop) and (drawy < clipbottom))  {
               Bitmap->DrawUCPixel(Bitmap, drawx, drawy, colour);
            }
            if (err_1 > 0) { drawy += y_inc; err_1 -= dx2; }
            err_1 += dy2;
            drawx += x_inc;
         }
      }
      else {
         err_1 = dx2 - m;
         for (i = 0; i < m; i++) {
            if ((drawx >= clipleft) and (drawx < clipright) and (drawy >= cliptop) and (drawy < clipbottom))  {
               Bitmap->DrawUCPixel(Bitmap, drawx, drawy, colour);
            }
            if (err_1 > 0) { drawx += x_inc; err_1 -= dy2; }
            err_1 += dx2;
            drawy += y_inc;
         }
      }

      if ((drawx >= clipleft) and (drawx < clipright) and (drawy >= cliptop) and (drawy < clipbottom))  {
         Bitmap->DrawUCPixel(Bitmap, drawx, drawy, colour);
      }
   }

   UnlockSurface(Bitmap);
}

/*****************************************************************************

-FUNCTION-
DrawRGBPixel: Draws a 24 bit pixel to a bitmap.

This function draws an RGB colour to the (X, Y) position of a target bitmap.  The function will check the given
coordinates to ensure that the pixel is inside the bitmap's clipping area.

-INPUT-
obj(Bitmap) Bitmap: The target bitmap object.
int X: Horizontal coordinate of the pixel.
int Y: Vertical coordinate of the pixel.
struct(*RGB8) RGB: The colour to be drawn, in RGB format.

*****************************************************************************/

void gfxDrawRGBPixel(objBitmap *Bitmap, LONG X, LONG Y, RGB8 *Pixel)
{
   if ((X >= Bitmap->Clip.Right) or (X < Bitmap->Clip.Left)) return;
   if ((Y >= Bitmap->Clip.Bottom) or (Y < Bitmap->Clip.Top)) return;
   Bitmap->DrawUCRPixel(Bitmap, X + Bitmap->XOffset, Y + Bitmap->YOffset, Pixel);
}

/*****************************************************************************

-FUNCTION-
DrawPixel: Draws a single pixel to a bitmap.

This function draws a pixel to the coordinates X, Y on a bitmap with a colour determined by the Colour index.
This function will check the given coordinates to make sure that the pixel is inside the bitmap's clipping area.

-INPUT-
obj(Bitmap) Bitmap: The target bitmap object.
int X: The horizontal coordinate of the pixel.
int Y: The vertical coordinate of the pixel.
uint Colour: The colour value to use for the pixel.

*****************************************************************************/

static void gfxDrawPixel(objBitmap *Bitmap, LONG X, LONG Y, ULONG Colour)
{
   if ((X >= Bitmap->Clip.Right) or (X < Bitmap->Clip.Left)) return;
   if ((Y >= Bitmap->Clip.Bottom) or (Y < Bitmap->Clip.Top)) return;
   Bitmap->DrawUCPixel(Bitmap, X + Bitmap->XOffset, Y + Bitmap->YOffset, Colour);
}

/*****************************************************************************

-FUNCTION-
DrawRectangle: Draws rectangles, both filled and unfilled.

This function draws both filled and unfilled rectangles.  The rectangle is drawn to the target bitmap at position
(X, Y) with dimensions determined by the specified Width and Height.  If the Flags parameter defines BAF_FILL then
the rectangle will be filled, otherwise only the outline will be drawn.  The colour of the rectangle is determined by
the pixel value in the Colour argument.  Blending is not enabled unless the BAF_BLEND flag is defined and an alpha
value is present in the Colour.

-INPUT-
obj(Bitmap) Bitmap: Pointer to the target @Bitmap.
int X:       The left-most coordinate of the rectangle.
int Y:       The top-most coordinate of the rectangle.
int Width:   The width of the rectangle.
int Height:  The height of the rectangle.
uint Colour: The colour value to use for the rectangle.
int(BAF) Flags: Use BAF_FILL to fill the rectangle.  Use of BAF_BLEND will enable blending.

*****************************************************************************/

static void gfxDrawRectangle(objBitmap *Bitmap, LONG X, LONG Y, LONG Width, LONG Height, ULONG Colour, LONG Flags)
{
   parasol::Log log(__FUNCTION__);
   RGB8 pixel;
   UBYTE *data;
   UWORD *word;
   ULONG *longdata;
   LONG xend, x, EX, EY, i;

   if (!Bitmap) return;

   // If we are not going to fill the rectangle, use this routine to draw an outline.

   if ((!(Flags & BAF_FILL)) and (Width > 1) and (Height > 1)) {
      EX = X + Width - 1;
      EY = Y + Height - 1;
      if (X >= Bitmap->Clip.Left) gfxDrawRectangle(Bitmap, X, Y, 1, Height, Colour, Flags|BAF_FILL); // Left
      if (Y >= Bitmap->Clip.Top)  gfxDrawRectangle(Bitmap, X, Y, Width, 1, Colour, Flags|BAF_FILL); // Top
      if (Y + Height <= Bitmap->Clip.Bottom) gfxDrawRectangle(Bitmap, X, EY, Width, 1, Colour, Flags|BAF_FILL); // Bottom
      if (X + Width <= Bitmap->Clip.Right)   gfxDrawRectangle(Bitmap, X+Width-1, Y, 1, Height, Colour, Flags|BAF_FILL);
      return;
   }

   if (!(Bitmap->Head.Flags & NF_INITIALISED)) { log.warning(ERR_NotInitialised); return; }

   X += Bitmap->XOffset;
   Y += Bitmap->YOffset;

   if (X >= Bitmap->Clip.Right + Bitmap->XOffset) return;
   if (Y >= Bitmap->Clip.Bottom + Bitmap->YOffset) return;
   if (X + Width <= Bitmap->Clip.Left + Bitmap->XOffset) return;
   if (Y + Height <= Bitmap->Clip.Top + Bitmap->YOffset) return;

   if (X < Bitmap->Clip.Left + Bitmap->XOffset) {
      Width -= Bitmap->Clip.Left + Bitmap->XOffset - X;
      X = Bitmap->Clip.Left + Bitmap->XOffset;
   }

   if (Y < Bitmap->Clip.Top + Bitmap->YOffset) {
      Height -= Bitmap->Clip.Top + Bitmap->YOffset - Y;
      Y = Bitmap->Clip.Top + Bitmap->YOffset;
   }

   if ((X + Width) >= Bitmap->Clip.Right + Bitmap->XOffset)   Width = Bitmap->Clip.Right + Bitmap->XOffset - X;
   if ((Y + Height) >= Bitmap->Clip.Bottom + Bitmap->YOffset) Height = Bitmap->Clip.Bottom + Bitmap->YOffset - Y;

   UWORD red   = UnpackRed(Bitmap, Colour);
   UWORD green = UnpackGreen(Bitmap, Colour);
   UWORD blue  = UnpackBlue(Bitmap, Colour);

   // Translucent rectangle support

   UBYTE opacity = 255;
   if (Flags & BAF_BLEND) {
      opacity = UnpackAlpha(Bitmap, Colour);
   }
   else opacity = Bitmap->Opacity; // Pulling the opacity from the bitmap is deprecated, used BAF_BLEND instead.

   if (opacity < 255) {
      if (!LockSurface(Bitmap, SURFACE_READWRITE)) {
         UWORD wordpixel;

         if (Bitmap->BitsPerPixel IS 32) {
            ULONG cmb_alpha;
            longdata = (ULONG *)(Bitmap->Data + (Bitmap->LineWidth * Y));
            xend = X + Width;
            cmb_alpha = 255 << Bitmap->prvColourFormat.AlphaPos;
            while (Height > 0) {
               i = X;
               while (i < xend) {
                  UBYTE sr = longdata[i]>>Bitmap->prvColourFormat.RedPos;
                  UBYTE sg = longdata[i]>>Bitmap->prvColourFormat.GreenPos;
                  UBYTE sb = longdata[i]>>Bitmap->prvColourFormat.BluePos;

                  longdata[i] = (((((red   - sr)*opacity)>>8)+sr) << Bitmap->prvColourFormat.RedPos) |
                                (((((green - sg)*opacity)>>8)+sg) << Bitmap->prvColourFormat.GreenPos) |
                                (((((blue  - sb)*opacity)>>8)+sb) << Bitmap->prvColourFormat.BluePos) |
                                cmb_alpha;
                  i++;
               }
               longdata = (ULONG *)(((BYTE *)longdata) + Bitmap->LineWidth);
               Height--;
            }
         }
         else if (Bitmap->BitsPerPixel IS 24) {
            data = Bitmap->Data + (Bitmap->LineWidth * Y);
            X    = X * Bitmap->BytesPerPixel;
            xend = X + (Width * Bitmap->BytesPerPixel);
            while (Height > 0) {
               i = X;
               while (i < xend) {
                  data[i] = (((blue - data[i])*opacity)>>8)+data[i]; i++;
                  data[i] = (((green - data[i])*opacity)>>8)+data[i]; i++;
                  data[i] = (((red - data[i])*opacity)>>8)+data[i]; i++;
               }
               data += Bitmap->LineWidth;
               Height--;
            }
         }
         else if (Bitmap->BitsPerPixel IS 16) {
            word = (UWORD *)(Bitmap->Data + (Bitmap->LineWidth * Y));
            xend = X + Width;
            while (Height > 0) {
               i = X;
               while (i < xend) {
                  UBYTE sr = (word[i] & 0x001f)<<3;
                  UBYTE sg = (word[i] & 0x07e0)>>3;
                  UBYTE sb = (word[i] & 0xf800)>>8;
                  sr = (((red   - sr)*opacity)>>8) + sr;
                  sg = (((green - sg)*opacity)>>8) + sg;
                  sb = (((blue  - sb)*opacity)>>8) + sb;
                  wordpixel =  (sb>>3) & 0x001f;
                  wordpixel |= (sg<<3) & 0x07e0;
                  wordpixel |= (sr<<8) & 0xf800;
                  word[i] = wordpixel;
                  i++;
               }
               word = (UWORD *)(((UBYTE *)word) + Bitmap->LineWidth);
               Height--;
            }
         }
         else if (Bitmap->BitsPerPixel IS 15) {
            word = (UWORD *)(Bitmap->Data + (Bitmap->LineWidth * Y));
            xend = X + Width;
            while (Height > 0) {
               i = X;
               while (i < xend) {
                  UBYTE sr = (word[i] & 0x001f)<<3;
                  UBYTE sg = (word[i] & 0x03e0)>>2;
                  UBYTE sb = (word[i] & 0x7c00)>>7;
                  sr = (((red   - sr)*opacity)>>8) + sr;
                  sg = (((green - sg)*opacity)>>8) + sg;
                  sb = (((blue  - sb)*opacity)>>8) + sb;
                  wordpixel =  (sb>>3) & 0x001f;
                  wordpixel |= (sg<<2) & 0x03e0;
                  wordpixel |= (sr<<7) & 0x7c00;
                  word[i] = wordpixel;
                  i++;
               }
               word = (UWORD *)(((UBYTE *)word) + Bitmap->LineWidth);
               Height--;
            }
         }
         else {
            while (Height > 0) {
               for (i=X; i < X + Width; i++) {
                  Bitmap->ReadUCRPixel(Bitmap, i, Y, &pixel);
                  pixel.Red   = (((red - pixel.Red)*opacity)>>8) + pixel.Red;
                  pixel.Green = (((green - pixel.Green)*opacity)>>8) + pixel.Green;
                  pixel.Blue  = (((blue - pixel.Blue)*opacity)>>8) + pixel.Blue;
                  pixel.Alpha = 255;
                  Bitmap->DrawUCRPixel(Bitmap, i, Y, &pixel);
               }
               Y++;
               Height--;
            }
         }

         UnlockSurface(Bitmap);
      }

      return;
   }

   // Standard rectangle (no translucency) video support

   #ifdef _GLES_
      if (Bitmap->DataFlags & MEM_VIDEO) {
      log.warning("TODO: Draw rectangles to opengl");
         glClearColor(0.5, 0.5, 0.5, 1.0);
         glClear(GL_COLOR_BUFFER_BIT);
         return;
      }
   #endif

   #ifdef _WIN32
      if (Bitmap->win.Drawable) {
         winDrawRectangle(Bitmap->win.Drawable, X, Y, Width, Height, red, green, blue);
         return;
      }
   #endif

   #ifdef __xwindows__
      if (Bitmap->DataFlags & (MEM_VIDEO|MEM_TEXTURE)) {
         XSetForeground(XDisplay, glXGC, Colour);
         XFillRectangle(XDisplay, Bitmap->x11.drawable, glXGC, X, Y, Width, Height);
         return;
      }
   #endif

   // Standard rectangle data support

   if (!LockSurface(Bitmap, SURFACE_WRITE)) {
      if (!Bitmap->Data) {
         UnlockSurface(Bitmap);
         return;
      }

      if (Bitmap->Type IS BMP_CHUNKY) {
         if (Bitmap->BitsPerPixel IS 32) {
            longdata = (ULONG *)(Bitmap->Data + (Bitmap->LineWidth * Y));
            while (Height > 0) {
               for (x=X; x < (X+Width); x++) longdata[x] = Colour;
               longdata = (ULONG *)(((UBYTE *)longdata) + Bitmap->LineWidth);
               Height--;
            }
         }
         else if (Bitmap->BitsPerPixel IS 24) {
            data = Bitmap->Data + (Bitmap->LineWidth * Y);
            X = X + X + X;
            xend = X + Width + Width + Width;
            while (Height > 0) {
               for (x=X; x < xend;) {
                  data[x++] = blue; data[x++] = green; data[x++] = red;
               }
               data += Bitmap->LineWidth;
               Height--;
            }
         }
         else if ((Bitmap->BitsPerPixel IS 16) or (Bitmap->BitsPerPixel IS 15)) {
            word = (UWORD *)(Bitmap->Data + (Bitmap->LineWidth * Y));
            xend = X + Width;
            while (Height > 0) {
               for (x=X; x < xend; x++) word[x] = (UWORD)Colour;
               word = (UWORD *)(((BYTE *)word) + Bitmap->LineWidth);
               Height--;
            }
         }
         else if (Bitmap->BitsPerPixel IS 8) {
            data = Bitmap->Data + (Bitmap->LineWidth * Y);
            xend = X + Width;
            while (Height > 0) {
               for (x=X; x < xend;) data[x++] = Colour;
               data += Bitmap->LineWidth;
               Height--;
            }
         }
         else while (Height > 0) {
            for (i=X; i < X + Width; i++) Bitmap->DrawUCPixel(Bitmap, i, Y, Colour);
            Y++;
            Height--;
         }
      }
      else while (Height > 0) {
         for (i=X; i < X + Width; i++) Bitmap->DrawUCPixel(Bitmap, i, Y, Colour);
         Y++;
         Height--;
      }

      UnlockSurface(Bitmap);
   }

   return;
}

/*****************************************************************************

-FUNCTION-
FlipBitmap: Flips a bitmap around its horizontal or vertical axis.

The FlipBitmap() function is used to flip bitmap images on their horizontal or vertical axis.  The amount of time
required to flip a bitmap is dependent on the area of the bitmap you are trying to flip over and its total number of
colours.

-INPUT-
obj(Bitmap) Bitmap: Pointer to a bitmap object.
int(FLIP) Orientation: Set to either FLIP_HORIZONTAL or FLIP_VERTICAL.  If set to neither, the function does nothing.

*****************************************************************************/

static void gfxFlipBitmap(objBitmap *Bitmap, LONG Orientation)
{
   ActionTags(MT_BmpFlip, Bitmap, Orientation);
}

/*****************************************************************************

-FUNCTION-
Flood: Perform a flood-fill operation on a pixel specified at (X, Y).

This function performs a flood-fill operation on a bitmap.  It requires an X and Y value that will target a pixel to
initiate the flood-fill operation.  The colour value indicated in RGB will be used to change the targeted pixel and all
adjacent pixels that share the targeted pixel's colour.

-INPUT-
obj(Bitmap) Bitmap: Pointer to a bitmap object.
int X: The horizontal coordinate of the pixel to target for the flood operation.
int Y: The vertical coordinate of the pixel to target for the flood operation.
uint Colour: The colour value of the fill in packed-pixel format.

-ERRORS-
Okay

*****************************************************************************/

static ERROR gfxFlood(objBitmap *Bitmap, LONG X, LONG Y, ULONG Colour)
{
   return ActionTags(MT_BmpFlood, Bitmap, X, Y, Colour);
}

/*****************************************************************************

-FUNCTION-
ReadRGBPixel: Reads a pixel's colour from the target bitmap.

This function reads a pixel from a bitmap surface and returns the value in an RGB structure that remains good up until
the next call to this function.  Zero is returned in the alpha component if the pixel is out of bounds.

This function is thread-safe if the target Bitmap is locked.

-INPUT-
obj(Bitmap) Bitmap: Pointer to a bitmap object.
int X: The horizontal coordinate of the pixel.
int Y: The vertical coordinate of the pixel.
&struct(RGB8) RGB: The colour values will be stored in this RGB structure.

*****************************************************************************/

static void gfxReadRGBPixel(objBitmap *Bitmap, LONG X, LONG Y, RGB8 **Pixel)
{
   static THREADVAR RGB8 pixel;
   if ((X >= Bitmap->Clip.Right) or (X < Bitmap->Clip.Left) or
       (Y >= Bitmap->Clip.Bottom) or (Y < Bitmap->Clip.Top)) {
      pixel.Red = 0; pixel.Green = 0; pixel.Blue = 0; pixel.Alpha = 0;
   }
   else {
      pixel.Alpha = 255;
      Bitmap->ReadUCRPixel(Bitmap, X + Bitmap->XOffset, Y + Bitmap->YOffset, &pixel);
   }
   *Pixel = &pixel;
}

/*****************************************************************************

-FUNCTION-
ReadPixel: Reads a pixel's colour from the target bitmap.

This function reads a pixel from a bitmap area and returns its colour index (if the Bitmap is indexed with a palette)
or its packed pixel value.  Zero is returned if the pixel is out of bounds.

-INPUT-
obj(Bitmap) Bitmap: Pointer to a bitmap object.
int X: The horizontal coordinate of the pixel.
int Y: The vertical coordinate of the pixel.

-RESULT-
uint: The colour value of the pixel will be returned.  Zero is returned if the pixel is out of bounds.

*****************************************************************************/

static ULONG gfxReadPixel(objBitmap *Bitmap, LONG X, LONG Y)
{
   if ((X >= Bitmap->Clip.Right) or (X < Bitmap->Clip.Left) or
       (Y >= Bitmap->Clip.Bottom) or (Y < Bitmap->Clip.Top)) return 0;
   else return Bitmap->ReadUCPixel(Bitmap, X, Y);
}

/*****************************************************************************

-FUNCTION-
ScaleToDPI: Scales a value to the active display's DPI.

ScaleToDPI() is a convenience function for scaling any value to the active display's current DPI setting.  The value
that you provide must be fixed in relation to the system wide default of 96 DPI.  If the display's DPI varies differs
to that, your value will be scaled to match.  For instance, an 8 point font at 96 DPI would be scaled to 20 points if
the display was 240 DPI.

If the DPI of the display is unknown, your value will be returned unscaled.

-INPUT-
double Value: The number to be scaled.

-RESULT-
double: The scaled value is returned.

*****************************************************************************/

static DOUBLE gfxScaleToDPI(DOUBLE Value)
{
   if ((!glDisplayInfo->HDensity) or (!glDisplayInfo->VDensity)) return Value;
   else return 96.0 / (((DOUBLE)glDisplayInfo->HDensity + (DOUBLE)glDisplayInfo->VDensity) * 0.5) * Value;
}

/*****************************************************************************

-FUNCTION-
Resample: Resamples a bitmap by dithering it to a new set of colour masks.

The Resample() function provides a means for resampling a bitmap to a new colour format without changing the actual
bit depth of the image. It uses dithering so as to retain the quality of the image when down-sampling.  This function
is generally used to 'pre-dither' true colour bitmaps in preparation for copying to bitmaps with lower colour quality.

You are required to supply a ColourFormat structure that describes the colour format that you would like to apply to
the bitmap's image data.

-INPUT-
obj(Bitmap) Bitmap: The bitmap object to be resampled.
struct(*ColourFormat) ColourFormat: The new colour format to be applied to the bitmap.

-ERRORS-
Okay
NullArgs

*****************************************************************************/

static ERROR gfxResample(objBitmap *Bitmap, ColourFormat *Format)
{
   if ((!Bitmap) or (!Format)) return ERR_NullArgs;

   dither(Bitmap, Bitmap, Format, Bitmap->Width, Bitmap->Height, 0, 0, 0, 0);
   return ERR_Okay;
}

/*****************************************************************************

-FUNCTION-
SetClipRegion: Sets a clipping region for a bitmap object.

The SetClipRegion() method is used to manage the clipping regions assigned to a bitmap object.  Each new bitmap that is
created has at least one clip region assigned to it, but by using SetClipRegion() you can also define multiple clipping
areas, which is useful for complex graphics management.

Each clipping region that you set is assigned a Number, starting from zero which is the default.  Each time that you
set a new clip region you must specify the number of the region that you wish to set.  If you attempt to 'skip'
regions - for instance, if you set regions 0, 1, 2 and 3, then skip 4 and set 5, the routine will set region 4 instead.
If you have specified multiple clip regions and want to lower the count or reset the list, set the number of the last
region that you want in your list and set the Terminate argument to TRUE to kill the regions specified beyond it.

The ClipLeft, ClipTop, ClipRight and ClipBottom fields in the target Bitmap will be updated to reflect the overall
area that is covered by the clipping regions that have been set.

-INPUT-
obj(Bitmap) Bitmap: The target bitmap.
int Number:    The number of the clip region to set.
int Left:      The horizontal start of the clip region.
int Top:       The vertical start of the clip region.
int Right:     The right-most edge of the clip region.
int Bottom:    The bottom-most edge of the clip region.
int Terminate: Set to TRUE if this is the last clip region in the list, otherwise FALSE.

*****************************************************************************/

static void gfxSetClipRegion(objBitmap *Bitmap, LONG Number, LONG Left, LONG Top, LONG Right, LONG Bottom,
   LONG Terminate)
{
   Bitmap->Clip.Left   = Left;
   Bitmap->Clip.Top    = Top;
   Bitmap->Clip.Right  = Right;
   Bitmap->Clip.Bottom = Bottom;

   if (Bitmap->Clip.Left < 0) Bitmap->Clip.Left = 0;
   if (Bitmap->Clip.Top  < 0) Bitmap->Clip.Top = 0;
   if (Bitmap->Clip.Right  > Bitmap->Width)  Bitmap->Clip.Right = Bitmap->Width;
   if (Bitmap->Clip.Bottom > Bitmap->Height) Bitmap->Clip.Bottom = Bitmap->Height;
}

/*****************************************************************************

-FUNCTION-
Sync: Waits for the completion of all active bitmap operations.

The Sync() function will wait for all current video operations to complete before it returns.  This ensures that it is
safe to write to video memory with the CPU, preventing any possibility of clashes with the onboard graphics chip.

-INPUT-
obj(Bitmap) Bitmap: Pointer to the bitmap that you want to synchronise or NULL to sleep on the graphics accelerator.
-END-

*****************************************************************************/

static void gfxSync(objBitmap *Bitmap)
{

}

/*****************************************************************************
** NOTE: Please ensure that the Width and Height are already clipped to meet the restrictions of BOTH the source and
** destination bitmaps.
*/

#define DITHER_ERROR(c)                  /* Dither one colour component */ \
   dif = (buf1[x].c>>3) - (brgb.c<<3);   /* An eighth of the error */ \
   if (dif) {                            \
      val3 = buf2[x+1].c + (dif<<1);     /* 1/4 down & right */ \
      dif = dif + dif + dif;             \
      val1 = buf1[x+1].c + dif;          /* 3/8 to the right */ \
      val2 = buf2[x].c + dif;            /* 3/8 to the next row */ \
      if (dif > 0) {                     /* Check for overflow */ \
         buf1[x+1].c = MIN(16383, val1); \
         buf2[x].c   = MIN(16383, val2); \
         buf2[x+1].c = MIN(16383, val3); \
      }                                  \
      else if (dif < 0) {                \
         buf1[x+1].c = MAX(0, val1);     \
         buf2[x].c   = MAX(0, val2);     \
         buf2[x+1].c = MAX(0, val3);     \
      }                                  \
   }

static ERROR dither(objBitmap *Bitmap, objBitmap *Dest, ColourFormat *Format, LONG Width, LONG Height,
   LONG SrcX, LONG SrcY, LONG DestX, LONG DestY)
{
   parasol::Log log(__FUNCTION__);
   RGB16 *buf1, *buf2, *buffer;
   RGB8 brgb;
   UBYTE *srcdata, *destdata, *data;
   LONG dif, val1, val2, val3;
   LONG x, y;
   ULONG colour;
   WORD index;
   UBYTE rmask, gmask, bmask;

   if ((Width < 1) or (Height < 1)) return ERR_Okay;

   // Punch the developer for making stupid mistakes

   if ((Dest->BitsPerPixel >= 24) and (!Format)) {
      log.warning("Dithering attempted to a %dbpp bitmap.", Dest->BitsPerPixel);
      return ERR_Failed;
   }

   // Do a straight copy if the bitmap is too small for dithering

   if ((Height < 2) or (Width < 2)) {
      for (y=SrcY; y < SrcY+Height; y++) {
         for (x=SrcX; x < SrcX+Width; x++) {
            Bitmap->ReadUCRPixel(Bitmap, x, y, &brgb);
            Dest->DrawUCRPixel(Dest, x, y, &brgb);
         }
      }
      return ERR_Okay;
   }

   // Allocate buffer for dithering

   if (Width * (LONG)sizeof(RGB16) * 2 > glDitherSize) {
      if (glDither) { FreeResource(glDither); glDither = NULL; }

      if (AllocMemory(Width * sizeof(RGB16) * 2, MEM_NO_CLEAR|MEM_UNTRACKED, &glDither, NULL) != ERR_Okay) {
         return ERR_AllocMemory;
      }
   }

   buf1 = (RGB16 *)glDither;
   buf2 = buf1 + Width;

   // Prime buf2, which will be copied to buf1 at the start of the loop.  We work with six binary "decimal places" to reduce roundoff errors.

   for (x=0,index=0; x < Width; x++,index+=Bitmap->BytesPerPixel) {
      Bitmap->ReadUCRIndex(Bitmap, Bitmap->Data + index, &brgb);
      buf2[x].Red   = brgb.Red<<6;
      buf2[x].Green = brgb.Green<<6;
      buf2[x].Blue  = brgb.Blue<<6;
      buf2[x].Alpha = brgb.Alpha;
   }

   if (!Format) Format = &Dest->prvColourFormat;

   srcdata = Bitmap->Data + ((SrcY+1) * Bitmap->LineWidth);
   destdata = Dest->Data + (DestY * Dest->LineWidth);
   rmask = Format->RedMask   << Format->RedShift;
   gmask = Format->GreenMask << Format->GreenShift;
   bmask = Format->BlueMask  << Format->BlueShift;

   for (y=0; y < Height - 1; y++) {
      // Move line 2 to line 1, line 2 then is empty for reading the next row

      buffer = buf2;
      buf2 = buf1;
      buf1 = buffer;

      // Read the next source line

      if (Bitmap->BytesPerPixel IS 4) {
         buffer = buf2;
         data = srcdata+(SrcX<<2);
         for (x=0; x < Width; x++, data+=4, buffer++) {
            colour = ((ULONG *)data)[0];
            buffer->Red   = ((UBYTE)(colour >> Bitmap->prvColourFormat.RedPos))<<6;
            buffer->Green = ((UBYTE)(colour >> Bitmap->prvColourFormat.GreenPos))<<6;
            buffer->Blue  = ((UBYTE)(colour >> Bitmap->prvColourFormat.BluePos))<<6;
            buffer->Alpha = ((UBYTE)(colour >> Bitmap->prvColourFormat.AlphaPos));
         }
      }
      else if (Bitmap->BytesPerPixel IS 2) {
         buffer = buf2;
         data = srcdata+(SrcX<<1);
         for (x=0; x < Width; x++, data+=2, buffer++) {
            colour = ((UWORD *)data)[0];
            buffer->Red   = UnpackRed(Bitmap, colour)<<6;
            buffer->Green = UnpackGreen(Bitmap, colour)<<6;
            buffer->Blue  = UnpackBlue(Bitmap, colour)<<6;
         }
      }
      else {
         buffer = buf2;
         data = srcdata + (SrcX * Bitmap->BytesPerPixel);
         for (x=0; x < Width; x++, data+=Bitmap->BytesPerPixel, buffer++) {
            Bitmap->ReadUCRIndex(Bitmap, data, &brgb);
            buffer->Red   = brgb.Red<<6;
            buffer->Green = brgb.Green<<6;
            buffer->Blue  = brgb.Blue<<6;
         }
      }

      // Dither

      buffer = buf1;
      data = destdata + (DestX * Dest->BytesPerPixel);
      if (Dest->BytesPerPixel IS 2) {
         for (x=0; x < Width - 1; x++,data+=2,buffer++) {
            brgb.Red   = (buffer->Red>>6) & rmask;
            brgb.Green = (buffer->Green>>6) & gmask;
            brgb.Blue  = (buffer->Blue>>6) & bmask;
            ((UWORD *)data)[0] = ((brgb.Red>>Dest->prvColourFormat.RedShift) << Dest->prvColourFormat.RedPos) |
                                 ((brgb.Green>>Dest->prvColourFormat.GreenShift) << Dest->prvColourFormat.GreenPos) |
                                 ((brgb.Blue>>Dest->prvColourFormat.BlueShift) << Dest->prvColourFormat.BluePos);
            DITHER_ERROR(Red);
            DITHER_ERROR(Green);
            DITHER_ERROR(Blue);
         }
      }
      else if (Dest->BytesPerPixel IS 4) {
         for (x=0; x < Width-1; x++,data+=4,buffer++) {
            brgb.Red   = (buffer->Red>>6) & rmask;
            brgb.Green = (buffer->Green>>6) & gmask;
            brgb.Blue  = (buffer->Blue>>6) & bmask;
            ((ULONG *)data)[0] = PackPixelWBA(Dest, brgb.Red, brgb.Green, brgb.Blue, buffer->Alpha);
            DITHER_ERROR(Red);
            DITHER_ERROR(Green);
            DITHER_ERROR(Blue);
         }
      }
      else {
         for (x=0; x < Width - 1; x++,data+=Dest->BytesPerPixel,buffer++) {
            brgb.Red   = (buffer->Red>>6) & rmask;
            brgb.Green = (buffer->Green>>6) & gmask;
            brgb.Blue  = (buffer->Blue>>6) & bmask;
            Dest->DrawUCRIndex(Dest, data, &brgb);
            DITHER_ERROR(Red);
            DITHER_ERROR(Green);
            DITHER_ERROR(Blue);
         }
      }

      // Draw the last pixel in the row - no downward propagation

      brgb.Red   = buf1[Width-1].Red>>6;
      brgb.Green = buf1[Width-1].Green>>6;
      brgb.Blue  = buf1[Width-1].Blue>>6;
      brgb.Alpha = buf1[Width-1].Alpha;
      Dest->DrawUCRIndex(Dest, destdata + ((Width - 1) * Dest->BytesPerPixel), &brgb);

      srcdata += Bitmap->LineWidth;
      destdata += Dest->LineWidth;
   }

   // Draw the last row of pixels - no leftward propagation

   if (Bitmap != Dest) {
      for (x=0,index=0; x < Width; x++,index+=Dest->BytesPerPixel) {
         brgb.Red   = buf2[x].Red>>6;
         brgb.Green = buf2[x].Green>>6;
         brgb.Blue  = buf2[x].Blue>>6;
         brgb.Alpha = buf2[x].Alpha;
         Dest->DrawUCRIndex(Dest, destdata+index, &brgb);
      }
   }

   return ERR_Okay;
}

/******************************************************************************

-FUNCTION-
SubscribeInput: Subscribe to incoming input messages for any active surface object.

The SubscribeInput() function provides a method for receiving input messages.  Input messages can be filtered so that
they are received in relation to surfaces and devices.  An input mask can also be applied so that only certain types of
messages are received.  If no filters are applied, then all user input messages can be received.

The input system is limited to managing messages that are related to the display (such as track pads, mouse pointers,
graphics tablets and touch screens). Keyboard devices are not included in the input management system as they are
specially supported by the <module>Keyboard</> module.

To reduce the number of messages being passed through the system, input messages are placed on a global queue that
is accessible to all tasks.  When a new message appears that matches a client's filtering criteria, an InputReady data
feed message will be sent to it.  The ~Core.GetInputMsg() function can then be used to process the available
messages in the queue.  The following code segment illustrates an example of this, and would be used in the DataFeed
action:

<pre>
if (Args->DataType IS DATA_INPUT_READY) {
   struct InputMsg *input;

   while (!scrGetInputMsg(Args->Buffer, 0, &input)) {
      if (input->Flags & JTYPE_MOVEMENT) {

      }
      else if (input->Type IS JET_LMB) {

      }
   }
}
</pre>

Further information on the processing of input messages is available in the documentation for the ~Core.GetInputMsg()
function.

You are required to remove the subscription with ~Core.UnsubscribeInput() once it is no longer required.

-INPUT-
oid Surface: If set, only the input messages that match the given surface ID will be received.
int(JTYPE) Mask: Combine JTYPE flags to define the input messages that you wish to received.  Set to 0xffffffff if all messages are desirable.
oid Device: If set, only the input messages that match the given device ID will be received.  NOTE - Support not yet implemented, set to zero.

-ERRORS-
Okay:
NullArgs:

******************************************************************************/

static ERROR gfxSubscribeInput(OBJECTID SurfaceID, LONG Mask, OBJECTID DeviceID)
{
   parasol::Log log(__FUNCTION__);

   #define CHUNK_INPUT 20

   auto sub = CurrentContext();

   log.traceBranch("Subscriber: #%d, Surface: #%d, MsgPort: " PF64() ", Mask: $%.8x, InputMID: %d", sub->UniqueID, SurfaceID, GetResource(RES_MESSAGE_QUEUE), Mask, glSharedControl->InputMID);

   // Allocate the subscription array if it does not exist.  NB: The memory is untracked and will be removed by the
   // last task that cleans up the memory resource pool.

   if (!glSharedControl->InputMID) {
      if (AllocMemory(sizeof(InputSubscription) * CHUNK_INPUT, MEM_PUBLIC|MEM_UNTRACKED, NULL, &glSharedControl->InputMID)) {
         return log.warning(ERR_AllocMemory);
      }
      glSharedControl->InputSize = CHUNK_INPUT;
   }

   // Add the subscription to the list.  Note that granted access to InputMID acts as a lock for variables like InputTotal.

   InputSubscription *list, *newlist;
   if (!AccessMemory(glSharedControl->InputMID, MEM_READ_WRITE, 2000, &list)) {
      // If there is no space left in the subscription array, expand it

      if (glSharedControl->InputTotal >= glSharedControl->InputSize) {
         log.msg("Input array needs to be expanded from %d entries.", glSharedControl->InputSize);

         MEMORYID newlistid;
         if (AllocMemory(sizeof(InputSubscription) * (glSharedControl->InputSize+CHUNK_INPUT), MEM_PUBLIC|MEM_UNTRACKED, (APTR *)&newlist, &newlistid)) {
            ReleaseMemory(list);
            return ERR_AllocMemory;
         }

         CopyMemory(list, newlist, sizeof(InputSubscription) * glSharedControl->InputSize);

         ReleaseMemory(list);

         FreeResourceID(glSharedControl->InputMID);
         glSharedControl->InputMID = newlistid;
         glSharedControl->InputSize += CHUNK_INPUT;
         list = newlist;
      }

      LONG i = glSharedControl->InputTotal;

      list[i].SurfaceID    = SurfaceID;
      list[i].SubscriberID = sub->UniqueID;
      list[i].MsgPort      = GetResource(RES_MESSAGE_QUEUE);
      if (!Mask) list[i].Mask = 0xffffffff;
      else list[i].Mask = Mask;

      glSharedControl->InputTotal++;

      ReleaseMemory(list);
      return ERR_Okay;
   }
   else return log.warning(ERR_AccessMemory);
}

/******************************************************************************

-FUNCTION-
GetInputTypeName: Returns the string name for an input type.

This function converts JET integer constants to their string equivalent.  Refer to ~Core.SubscribeInput() for a
list of JET constants.

-INPUT-
int(JET) Type: JET type integer.

-RESULT-
cstr: A string describing the input type is returned or NULL if the Type is invalid.

******************************************************************************/

static CSTRING gfxGetInputTypeName(LONG Type)
{
   if ((Type < 1) or (Type >= JET_END)) return NULL;
   return glInputNames[Type];
}

/******************************************************************************

-FUNCTION-
UnsubscribeInput: Removes an input subscription.

This function removes an input subscription that has been configured using ~Core.SubscribeInput().  If a Surface
filter was specified in the original subscription, this can also be defined so that any other active subscriptions
acquired by the Subscriber are unaffected.

-INPUT-
oid Surface: The surface to target for unsubscription.  If zero, all subscriptions are removed for the active object.

-ERRORS-
Okay
NullArgs
NotFound
-END-

******************************************************************************/

static ERROR gfxUnsubscribeInput(OBJECTID SurfaceID)
{
   parasol::Log log(__FUNCTION__);

   OBJECTPTR sub = CurrentContext();

   log.traceBranch("Subscriber: %d, Surface: %d", sub->UniqueID, SurfaceID);

   if (!glSharedControl->InputMID) return ERR_NotFound;

   InputSubscription *list;
   if (!AccessMemory(glSharedControl->InputMID, MEM_READ_WRITE, 2000, &list)) {
      bool removed = FALSE;
      for (LONG i=0; i < glSharedControl->InputTotal; i++) {
         if ((list[i].SubscriberID IS sub->UniqueID) and ((!SurfaceID) or (SurfaceID IS list[i].SurfaceID))) {
            removed = TRUE;
            if (i+1 < glSharedControl->InputTotal) {
               // Compact the list
               CopyMemory(list+i+1, list+i, sizeof(InputSubscription) * (glSharedControl->InputTotal - i - 1));
            }
            else ClearMemory(list+i, sizeof(list[i]));
            i--; // Offset the subsequent i++ of the for loop
            glSharedControl->InputTotal--;
         }
      }

      if (!glSharedControl->InputTotal) {
         log.trace("Freeing subscriber memory (last subscription removed)");
         ReleaseMemory(list);
         FreeResourceID(glSharedControl->InputMID);
         glSharedControl->InputMID   = 0;
         glSharedControl->InputSize  = 0;
         glSharedControl->InputTotal = 0;
      }
      else ReleaseMemory(list);

      if (!removed) return ERR_NotFound;
      else return ERR_Okay;
   }
   else return log.warning(ERR_AccessMemory);
}

/******************************************************************************
** Called when windows has an item to be dropped on our display area.
*/

#ifdef _WIN32
void winDragDropFromHost_Drop(int SurfaceID, char *Datatypes)
{
#ifdef WIN_DRAGDROP
   parasol::Log log(__FUNCTION__);
   objPointer *pointer;
   OBJECTID modal_id;
   extern OBJECTID glOverTaskID;

   log.branch("Surface: %d", SurfaceID);

   if ((pointer = gfxAccessPointer())) {
      // Pass AC_DragDrop to the surface underneath the mouse cursor.  If a surface subscriber accepts the data, it
      // will send a DATA_REQUEST to the relevant display object.  See DISPLAY_DataFeed() and winGetData().

      modal_id = drwGetModalSurface(glOverTaskID);
      if (modal_id IS SurfaceID) modal_id = 0;

      if (!modal_id) {
         SURFACEINFO *info;
         if (!drwGetSurfaceInfo(pointer->OverObjectID, &info)) {
            acDragDropID(pointer->OverObjectID, info->DisplayID, -1, Datatypes);
         }
         else log.warning(ERR_GetSurfaceInfo);
      }
      else log.msg("Program is modal - drag/drop cancelled.");

      gfxReleasePointer(pointer);
   }
#endif
}
#endif

#ifdef _GLES_
/*****************************************************************************
** This function is designed so that it can be re-called in case the OpenGL display needs to be reset.  THIS FUNCTION
** REQUIRES THAT THE GRAPHICS MUTEX IS LOCKED.
**
** PLEASE NOTE: EGL's design for embedded devices means that only one Display object can be active at any time.
*/

static ERROR init_egl(void)
{
   parasol::Log log(__FUNCTION__);
   EGLint format;
   LONG depth;

   log.branch("Requested Depth: %d", glEGLPreferredDepth);

   if (glEGLDisplay != EGL_NO_DISPLAY) {
      log.msg("EGL display is already initialised.");
      return ERR_Okay;
   }

   depth = glEGLPreferredDepth;
   if (depth < 16) depth = 16;

   glEGLRefreshDisplay = TRUE; // The active Display will need to refresh itself because the width/height/depth that EGL provides may differ from that desired.
   glEGLDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);

   eglInitialize(glEGLDisplay, 0, 0);

   // Here, the application chooses the configuration it desires. In this sample, we have a very simplified selection
   // process, where we pick the first EGLConfig that matches our criteria

   EGLint attribs[20];
   LONG a = 0;
   attribs[a++] = EGL_SURFACE_TYPE; attribs[a++] = EGL_WINDOW_BIT;
   attribs[a++] = EGL_BLUE_SIZE;    attribs[a++] = (depth IS 16) ? 5 : 8;
   attribs[a++] = EGL_GREEN_SIZE;   attribs[a++] = (depth IS 16) ? 6 : 8;
   attribs[a++] = EGL_RED_SIZE;     attribs[a++] = (depth IS 16) ? 5 : 8;
   attribs[a++] = EGL_DEPTH_SIZE;   attribs[a++] = 0; // Turns off 3D depth buffer if zero
   attribs[a++] = EGL_NONE;

   EGLConfig config;
   EGLint numConfigs;
   eglChooseConfig(glEGLDisplay, attribs, &config, 1, &numConfigs);

   // EGL_NATIVE_VISUAL_ID is an attribute of the EGLConfig that is guaranteed to be accepted by ANativeWindow_setBuffersGeometry().
   // As soon as we picked a EGLConfig, we can safely reconfigure the ANativeWindow buffers to match, using EGL_NATIVE_VISUAL_ID.

   LONG redsize, greensize, bluesize, alphasize, bufsize;
   eglGetConfigAttrib(glEGLDisplay, config, EGL_NATIVE_VISUAL_ID, &format);
   eglGetConfigAttrib(glEGLDisplay, config, EGL_RED_SIZE, &redsize);
   eglGetConfigAttrib(glEGLDisplay, config, EGL_GREEN_SIZE, &greensize);
   eglGetConfigAttrib(glEGLDisplay, config, EGL_BLUE_SIZE, &bluesize);
   eglGetConfigAttrib(glEGLDisplay, config, EGL_ALPHA_SIZE, &alphasize);
   eglGetConfigAttrib(glEGLDisplay, config, EGL_BUFFER_SIZE, &bufsize);
   glEGLDepth = bufsize; //redsize + greensize + bluesize + alphasize;

   ANativeWindow *window;
   if (!adGetWindow(&window)) {
      ANativeWindow_setBuffersGeometry(window, 0, 0, format);
      glEGLSurface = eglCreateWindowSurface(glEGLDisplay, config, window, NULL);
      glEGLContext = eglCreateContext(glEGLDisplay, config, NULL, NULL);
   }
   else {
      log.warning(ERR_SystemCall);
      return ERR_SystemCall;
   }

   if (eglMakeCurrent(glEGLDisplay, glEGLSurface, glEGLSurface, glEGLContext) == EGL_FALSE) {
      log.warning(ERR_SystemCall);
      return ERR_SystemCall;
   }

   eglQuerySurface(glEGLDisplay, glEGLSurface, EGL_WIDTH, &glEGLWidth);
   eglQuerySurface(glEGLDisplay, glEGLSurface, EGL_HEIGHT, &glEGLHeight);

   log.trace("Actual width and height set by EGL: %dx%dx%d", glEGLWidth, glEGLHeight, glEGLDepth);

   glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);
   //glEnable(GL_CULL_FACE);
   glClearColorx(0, 0, 0, 0xffff); // Default background colour.
   glShadeModel(GL_SMOOTH); // Switching from GL_SMOOTH to GL_FLAT gives more performance and 2D pixel accuracy
   glEnable(GL_BLEND); // Enable alpha blending.
   glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
   glDisable(GL_DEPTH_TEST); // Disabling depth test is good for 2D only
   glEnable(GL_TEXTURE_2D);
   //glDisable(GL_DITHER); // Dithering affects performance slightly if converting 24/32-bit to 16-bit, but quality is then an issue
   glDisable(GL_LIGHTING); // Improves performance for 2D
   glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

   glDisplayInfo->DisplayID = 0xffffffff; // Force refresh of display info cache.

   if (!glPointerID) {
      FastFindObject("SystemPointer", NULL, &glPointerID, 1, NULL);
   }

   if (glPointerID) {
      AConfiguration *config;
      objPointer *pointer;
      if (!adGetConfig(&config)) {
         DOUBLE dp_factor = 160.0 / AConfiguration_getDensity(config);
         if (!AccessObject(glPointerID, 3000, &pointer)) {
            pointer->ClickSlop = F2I(8.0 * dp_factor);
            log.msg("Click-slop calculated as %d.", pointer->ClickSlop);
            ReleaseObject(pointer);
         }
         else log.warning(ERR_AccessObject);
      }
      else log.warning("Failed to get Android Config object.");
   }

   glEGLState = EGL_INITIALISED;
   return ERR_Okay;
}

//****************************************************************************

static void refresh_display_from_egl(objDisplay *Self)
{
   parasol::Log log(__FUNCTION__);

   log.traceBranch("%dx%dx%d", glEGLWidth, glEGLHeight, glEGLDepth);

   glEGLRefreshDisplay = FALSE;

   Self->Width = glEGLWidth;
   Self->Height = glEGLHeight;

   ANativeWindow *window;
   if (!adGetWindow(&window)) {
      Self->WindowHandle = window;
   }

   // If the display's bitmap depth / size needs to change, resize it here.

   if (Self->Bitmap->Head.Flags & NF_INITIALISED) {
      if ((Self->Width != Self->Bitmap->Width) or (Self->Height != Self->Bitmap->Height)) {
         log.trace("Resizing OpenGL representative bitmap to match new dimensions.");
         acResize(Self->Bitmap, glEGLWidth, glEGLHeight, glEGLDepth);
      }
   }
}

/*****************************************************************************
** Free EGL resources.  This does not relate to hiding or switch off of the display - in fact the display can remain
** active as it normally does.  For this reason, we just focus on resource deallocation.
*/

static void free_egl(void)
{
   parasol::Log log(__FUNCTION__);

   log.branch("Current Display: $%x", (LONG)glEGLDisplay);

   glEGLState = EGL_TERMINATED; // The sooner we set this, the better.  It stops other threads from thinking that it's OK to keep using OpenGL.

   if (!pthread_mutex_lock(&glGraphicsMutex)) {
      log.msg("Lock granted - terminating EGL resources.");

      if (glEGLDisplay != EGL_NO_DISPLAY) {
         eglMakeCurrent(glEGLDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
         if (glEGLContext != EGL_NO_CONTEXT) eglDestroyContext(glEGLDisplay, glEGLContext);
         if (glEGLSurface != EGL_NO_SURFACE) eglDestroySurface(glEGLDisplay, glEGLSurface);
         eglTerminate(glEGLDisplay);
      }

      glEGLDisplay = EGL_NO_DISPLAY;
      glEGLContext = EGL_NO_CONTEXT;
      glEGLSurface = EGL_NO_SURFACE;

      pthread_mutex_unlock(&glGraphicsMutex);
   }
   else log.warning(ERR_LockFailed);

   log.msg("EGL successfully terminated.");
}
#endif

//****************************************************************************

#include "class_pointer.cpp"
#include "class_display.cpp"
#include "class_bitmap.cpp"

#ifdef __xwindows__
#include "x11/handlers.cpp"
#endif

#ifdef _WIN32
#include "win32/handlers.cpp"
#endif

#ifdef __ANDROID__
#include "android/android.cpp"
#endif

//****************************************************************************

PARASOL_MOD(CMDInit, NULL, CMDOpen, CMDExpunge, MODVERSION_DISPLAY)
