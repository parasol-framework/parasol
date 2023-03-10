/*********************************************************************************************************************

The source code of the Parasol project is made publicly available under the
terms described in the LICENSE.TXT file that is distributed with this package.
Please refer to it for further information on licensing.

*********************************************************************************************************************/

#include "defs.h"

ERROR GET_HDensity(extDisplay *Self, LONG *Value);
ERROR GET_VDensity(extDisplay *Self, LONG *Value);

//********************************************************************************************************************

rgb_to_linear glLinearRGB;

#ifdef __xwindows__

#define MAX_KEYCODES 256
#define TIME_X11DBLCLICK 600

void X11ManagerLoop(HOSTHANDLE, APTR);
void handle_button_press(XEvent *);
void handle_button_release(XEvent *);
void handle_configure_notify(XConfigureEvent *);
void handle_enter_notify(XCrossingEvent *);
void handle_exposure(XExposeEvent *);
void handle_key_press(XEvent *);
void handle_key_release(XEvent *);
void handle_motion_notify(XMotionEvent *);
void handle_stack_change(XCirculateEvent *);

X11Globals *glX11 = 0;
_XDisplay *XDisplay = 0;
struct XRandRBase *XRandRBase = 0;
UBYTE glX11ShmImage = FALSE;
UBYTE KeyHeld[K_LIST_END];
UBYTE glTrayIcon = 0, glTaskBar = 1, glStickToFront = 0;
LONG glKeyFlags = 0, glXFD = -1, glDGAPixelsPerLine = 0, glDGABankSize = 0;
Atom atomSurfaceID = 0, XWADeleteWindow = 0;
GC glXGC = 0, glClipXGC = 0;
XWindowAttributes glRootWindow;
Window glDisplayWindow = 0;
Cursor C_Default;
OBJECTPTR modXRR = NULL;
WORD glPlugin = FALSE;
APTR glDGAVideo = NULL;
#endif

#ifdef _WIN32

HINSTANCE glInstance = 0;

WinCursor winCursors[24] = {
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

//********************************************************************************************************************
// Note: These values are used as the input masks

const InputType glInputType[JET_END] = {
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

const CSTRING glInputNames[JET_END] = {
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

std::recursive_mutex glInputLock;

OBJECTPTR glCompress = NULL;
static objCompression *glIconArchive = NULL;
struct CoreBase *CoreBase;
ColourFormat glColourFormat;
bool glHeadless = false;
OBJECTPTR glModule = NULL;
OBJECTPTR clDisplay = NULL, clPointer = NULL, clBitmap = NULL, clClipboard = NULL, clSurface = NULL;
OBJECTID glPointerID = 0;
DISPLAYINFO glDisplayInfo;
APTR glDither = NULL;
SharedControl *glSharedControl = NULL;
bool glSixBitDisplay = false;
static MsgHandler *glExposeHandler = NULL;
TIMER glRefreshPointerTimer = 0;
extBitmap *glComposite = NULL;
static BYTE glDisplayType = DT_NATIVE;
DOUBLE glpRefreshRate = -1, glpGammaRed = 1, glpGammaGreen = 1, glpGammaBlue = 1;
LONG glpDisplayWidth = 1024, glpDisplayHeight = 768, glpDisplayX = 0, glpDisplayY = 0;
LONG glpDisplayDepth = 0; // If zero, the display depth will be based on the hosted desktop's bit depth.
LONG glpMaximise = FALSE, glpFullScreen = FALSE;
LONG glpWindowType = SWIN_HOST;
char glpDPMS[20] = "Standby";
UBYTE *glDemultiply = NULL;

std::vector<OBJECTID> glFocusList;
std::mutex glFocusLock;
std::recursive_mutex glSurfaceLock;

THREADVAR WORD tlNoDrawing = 0, tlNoExpose = 0, tlVolatileIndex = 0;
THREADVAR OBJECTID tlFreeExpose = 0;

//********************************************************************************************************************
// Alpha blending data.

UBYTE *glAlphaLookup = NULL;

INLINE UBYTE clipByte(LONG value)
{
   value = (0 & (-(WORD)(value < 0))) | (value & (-(WORD)!(value < 0)));
   value = (255 & (-(WORD)(value > 255))) | (value & (-(WORD)!(value > 255)));
   return value;
}

//********************************************************************************************************************
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

//********************************************************************************************************************
// lock_graphics_active() is intended for functionality that MUST have access to an active OpenGL display.  If an EGL
// display is unavailable then this function will fail even if the lock could otherwise be granted.

#ifdef _GLES_
ERROR lock_graphics_active(CSTRING Caller)
{
   pf::Log log(__FUNCTION__);

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

void unlock_graphics(void)
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

//********************************************************************************************************************
// Handles incoming interface messages.

static ERROR msg_handler(APTR Custom, LONG UniqueID, LONG Type, APTR Data, LONG Size)
{
   if ((Data) and ((size_t)Size >= sizeof(ExposeMessage))) {
      auto expose = (ExposeMessage *)Data;
      gfxExposeSurface(expose->ObjectID, expose->X, expose->Y, expose->Width, expose->Height, expose->Flags);
   }

   return ERR_Okay;
}

//********************************************************************************************************************

#ifdef __xwindows__

WORD glDGAAvailable = -1; // -1 indicates that we have not tried the setup process yet

#ifdef XDGA_AVAILABLE
APTR glDGAMemory = NULL;

LONG x11DGAAvailable(APTR *VideoAddress, LONG *PixelsPerLine, LONG *BankSize)
{
   pf::Log log(__FUNCTION__);
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
#else
LONG x11DGAAvailable(APTR *VideoAddress, LONG *PixelsPerLine, LONG *BankSize)
{
   glDGAAvailable = FALSE;
   return glDGAAvailable;
}
#endif

//**********************************************************************
// This routine is called if there is another window manager running.

XErrorHandler CatchRedirectError(Display *XDisplay, XErrorEvent *event)
{
   pf::Log log("X11");
   log.msg("A window manager has been detected on this X11 server.");
   glX11->Manager = FALSE;
   return 0;
}

//********************************************************************************************************************

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

XErrorHandler CatchXError(Display *XDisplay, XErrorEvent *XEvent)
{
   pf::Log log("X11");
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

//********************************************************************************************************************

int CatchXIOError(Display *XDisplay)
{
   pf::Log log("X11");
   log.error("A fatal XIO error occurred in relation to display \"%s\".", XDisplayName(NULL));
   return 0;
}

/*********************************************************************************************************************
** Returns TRUE if we are the window manager for the display.
*/

LONG x11WindowManager(void)
{
   if (glX11) return glX11->Manager;
   else return FALSE;
}

#endif

//********************************************************************************************************************

ERROR get_display_info(OBJECTID DisplayID, DISPLAYINFO *Info, LONG InfoSize)
{
   pf::Log log(__FUNCTION__);

  //log.traceBranch("Display: %d, Info: %p, Size: %d", DisplayID, Info, InfoSize);

   if (!Info) return log.warning(ERR_NullArgs);

   if (InfoSize != sizeof(DisplayInfoV3)) {
      log.error("Invalid InfoSize of %d (V3: %d)", InfoSize, (LONG)sizeof(DisplayInfoV3));
      return log.warning(ERR_Args);
   }

   extDisplay *display;
   if (DisplayID) {
      if (glDisplayInfo.DisplayID IS DisplayID) {
         CopyMemory(&glDisplayInfo, Info, InfoSize);
         return ERR_Okay;
      }
      else if (!AccessObjectID(DisplayID, 5000, &display)) {
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
      if ((glHeadless) or (!XDisplay)) {
         Info->Width         = 1024;
         Info->Height        = 768;
         Info->AccelFlags    = 0;
         Info->VDensity      = 96;
         Info->HDensity      = 96;
         Info->BitsPerPixel  = 32;
         Info->BytesPerPixel = 4;
      }
      else {
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

            glDisplayInfo.DisplayID     = 0;
            glDisplayInfo.Width         = ANativeWindow_getWidth(window);
            glDisplayInfo.Height        = ANativeWindow_getHeight(window);
            glDisplayInfo.BitsPerPixel  = 16;
            glDisplayInfo.BytesPerPixel = 2;
            glDisplayInfo.AccelFlags    = ACF_VIDEO_BLIT;
            glDisplayInfo.Flags         = SCR_MAXSIZE;  // Indicates that the width and height are the display's maximum.

            AConfiguration *config;
            if (!adGetConfig(&config)) {
               glDisplayInfo.HDensity = AConfiguration_getDensity(config);
               if (glDisplayInfo.HDensity < 60) glDisplayInfo.HDensity = 160;
            }
            else glDisplayInfo.HDensity = 160;

            glDisplayInfo.VDensity = glDisplayInfo.HDensity;

            LONG pixel_format = ANativeWindow_getFormat(window);
            if ((pixel_format IS WINDOW_FORMAT_RGBA_8888) or (pixel_format IS WINDOW_FORMAT_RGBX_8888)) {
               glDisplayInfo.BytesPerPixel = 32;
               if (pixel_format IS WINDOW_FORMAT_RGBA_8888) glDisplayInfo.BitsPerPixel = 32;
               else glDisplayInfo.BitsPerPixel = 24;
            }

            CopyMemory(&glColourFormat, &glDisplayInfo.PixelFormat, sizeof(glDisplayInfo.PixelFormat));

            if ((glDisplayInfo.BitsPerPixel < 8) or (glDisplayInfo.BitsPerPixel > 32)) {
               if (glDisplayInfo.BitsPerPixel > 32) glDisplayInfo.BitsPerPixel = 32;
               else if (glDisplayInfo.BitsPerPixel < 15) glDisplayInfo.BitsPerPixel = 16;
            }

            if (glDisplayInfo.BitsPerPixel > 24) glDisplayInfo.AmtColours = 1<<24;
            else glDisplayInfo.AmtColours = 1<<glDisplayInfo.BitsPerPixel;

            log.trace("%dx%dx%d", glDisplayInfo.Width, glDisplayInfo.Height, glDisplayInfo.BitsPerPixel);
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

      if (glDisplayInfo.DisplayID) {
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

//********************************************************************************************************************

static ERROR CMDInit(OBJECTPTR argModule, struct CoreBase *argCoreBase)
{
   pf::Log log(__FUNCTION__);
   ERROR error;
   #ifdef __xwindows__
      XGCValues gcv;
      LONG shmmajor, shmminor, pixmaps;
   #endif

   CoreBase = argCoreBase;

   argModule->getPtr(FID_Root, &glModule);

   if (GetSystemState()->Stage < 0) { // An early load indicates that classes are being probed, so just return them.
      create_pointer_class();
      create_display_class();
      create_bitmap_class();
      create_clipboard_class();
      create_surface_class();
      return ERR_Okay;
   }

   CSTRING driver_name;
   if ((driver_name = (CSTRING)GetResourcePtr(RES_DISPLAY_DRIVER))) {
      log.msg("User requested display driver '%s'", driver_name);
      if ((!StrMatch(driver_name, "none")) or (!StrMatch(driver_name, "headless"))) {
         glHeadless = TRUE;
      }
   }

   // NB: The display module cannot load the Surface module during initialisation due to recursive dependency.

   glSharedControl = (SharedControl *)GetResourcePtr(RES_SHARED_CONTROL);

   // Register a fake FD as input_event_loop() so that we can process input events on every ProcessMessages() cycle.

   RegisterFD((HOSTHANDLE)-2, RFD_ALWAYS_CALL, input_event_loop, NULL);

   // Add a message handler to the system for responding to interface messages

   auto call = make_function_stdc(msg_handler);
   if (AddMsgHandler(NULL, MSGID_EXPOSE, &call, &glExposeHandler) != ERR_Okay) {
      return log.warning(ERR_Failed);
   }

   #ifdef _GLES_
      pthread_mutexattr_t attr;
      pthread_mutexattr_init(&attr);
      pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE); // Allow recursive use of lock_graphics()

      pthread_mutex_init(&glGraphicsMutex, &attr);
   #endif

   #ifdef __ANDROID__
      if (GetResource(RES_SYSTEM_STATE) >= 0) {
         if (objModule::load("android", MODVERSION_ANDROID, (OBJECTPTR *)&modAndroid, &AndroidBase) != ERR_Okay) return ERR_InitModule;

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

   glDisplayInfo.DisplayID = 0xffffffff; // Indicate a refresh of the cache is required.

#ifdef __xwindows__
   if (!glHeadless) {
      log.trace("Allocating global memory structure.");

      MEMORYID memoryid = RPM_X11;
      if (!(error = AllocMemory(sizeof(X11Globals), MEM_UNTRACKED|MEM_PUBLIC|MEM_RESERVED|MEM_NO_BLOCKING, (APTR)&glX11, &memoryid))) {
         glX11->Manager = TRUE; // Assume that we are the window manager
      }
      else if (error IS ERR_ResourceExists) {
         if (!glX11) {
            if (AccessMemoryID(RPM_X11, MEM_READ_WRITE, 1000, &glX11) != ERR_Okay) {
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

      if (!NewObject(ID_MODULE, &modXRR)) {
         char buffer[32];
         IntToStr((MAXINT)XDisplay, buffer, sizeof(buffer));
         acSetVar(modXRR, "XDisplay", buffer);
         modXRR->set(FID_Name, "xrandr");
         if (!acInit(modXRR)) {
            if (modXRR->getPtr(FID_ModBase, &XRandRBase) != ERR_Okay) XRandRBase = NULL;
         }
      }
      else XRandRBase = NULL;

      // Get the X11 file descriptor (for incoming events) and tell the Core to listen to it when the task is sleeping.
      // The FD is currently marked as a dummy because processes aren't being woken from select() if the X11 FD already
      // contains input events.  Dummy FD routines are always called manually prior to select().

      glXFD = XConnectionNumber(XDisplay);
      fcntl(glXFD, F_SETFD, 1); // FD does not duplicate across exec()
      RegisterFD(glXFD, RFD_READ|RFD_ALWAYS_CALL, X11ManagerLoop, NULL);

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

      if (x11WindowManager() IS FALSE) { // We are a client of X11
//         XSelectInput(XDisplay, RootWindow(XDisplay, DefaultScreen(XDisplay)), NULL);
      }

      C_Default = XCreateFontCursor(XDisplay, XC_left_ptr);

      XWADeleteWindow = XInternAtom(XDisplay, "WM_DELETE_WINDOW", False);
      atomSurfaceID   = XInternAtom(XDisplay, "PARASOL_SCREENID", False);

      XGetWindowAttributes(XDisplay, DefaultRootWindow(XDisplay), &glRootWindow);

      ClearMemory(KeyHeld, sizeof(KeyHeld));

      // Drop superuser privileges following X11 initialisation (we only need suid for DGA).

      seteuid(getuid());

      init_xcursors();

      // Set the DISPLAY variable for clients to :10, which is the default X11 display for the rootless X Server.

      if (x11WindowManager() IS TRUE) {
         setenv("DISPLAY", ":10", TRUE);
      }
   }
#elif _WIN32

   {
      if ((glInstance = winGetModuleHandle())) {
         if (!winCreateScreenClass()) return log.warning(ERR_SystemCall);
      }
      else return log.warning(ERR_SystemCall);

      winDisableBatching();

      winInitCursors(winCursors, ARRAYSIZE(winCursors));

      MEMORYID memoryid = RPM_Clipboard;
      AllocMemory(sizeof(ClipHeader) + (MAX_CLIPS * sizeof(ClipEntry)), MEM_UNTRACKED|MEM_PUBLIC|MEM_RESERVED|MEM_NO_BLOCKING, NULL, &memoryid);
   }

#endif

   // Initialise our classes

   if (create_pointer_class() != ERR_Okay) return log.warning(ERR_AddClass);
   if (create_display_class() != ERR_Okay) return log.warning(ERR_AddClass);
   if (create_bitmap_class() != ERR_Okay) return log.warning(ERR_AddClass);
   if (create_clipboard_class() != ERR_Okay) return log.warning(ERR_AddClass);
   if (create_surface_class() != ERR_Okay) return log.warning(ERR_AddClass);

   // Initialise 64K alpha blending table, for cutting down on multiplications.  This memory block is shared, so one
   // table serves all processes.

   MEMORYID memoryid = RPM_AlphaBlend;
   if (!(error = AllocMemory(256 * 256, MEM_UNTRACKED|MEM_PUBLIC|MEM_RESERVED|MEM_NO_BLOCKING, &glAlphaLookup, &memoryid))) {
      LONG i = 0;
      for (WORD iAlpha=0; iAlpha < 256; iAlpha++) {
         DOUBLE fAlpha = (DOUBLE)iAlpha * (1.0 / 255.0);
         for (WORD iValue=0; iValue < 256; iValue++) {
            glAlphaLookup[i++] = clipByte(F2I((DOUBLE)iValue * fAlpha));
         }
      }
   }
   else if (error IS ERR_ResourceExists) {
      if (!glAlphaLookup) {
         if (AccessMemoryID(RPM_AlphaBlend, MEM_READ_WRITE|MEM_NO_BLOCKING, 500, &glAlphaLookup) != ERR_Okay) {
            return ERR_AccessMemory;
         }
      }
   }
   else return ERR_AllocMemory;

   glDisplayType = gfxGetDisplayType();

#ifdef __ANDROID__
      glpFullScreen = TRUE;
      glpDisplayDepth = 16;

      DISPLAYINFO *info;
      if (!gfxGetDisplayInfo(0, &info)) {
         glpDisplayWidth  = info.Width;
         glpDisplayHeight = info.Height;
         glpDisplayDepth  = info.BitsPerPixel;
      }
#else
   objConfig::create config = { fl::Path("user:config/display.cfg") };

   if (config.ok()) {
      cfgRead(*config, "DISPLAY", "Maximise", &glpMaximise);

      if ((glDisplayType IS DT_X11) or (glDisplayType IS DT_WINDOWS)) {
         log.msg("Using hosted window dimensions: %dx%d,%dx%d", glpDisplayX, glpDisplayY, glpDisplayWidth, glpDisplayHeight);
         if ((cfgRead(*config, "DISPLAY", "WindowWidth", &glpDisplayWidth) != ERR_Okay) or (!glpDisplayWidth)) {
            cfgRead(*config, "DISPLAY", "Width", &glpDisplayWidth);
         }

         if ((cfgRead(*config, "DISPLAY", "WindowHeight", &glpDisplayHeight) != ERR_Okay) or (!glpDisplayHeight)) {
            cfgRead(*config, "DISPLAY", "Height", &glpDisplayHeight);
         }

         cfgRead(*config, "DISPLAY", "WindowX", &glpDisplayX);
         cfgRead(*config, "DISPLAY", "WindowY", &glpDisplayY);
         cfgRead(*config, "DISPLAY", "FullScreen", &glpFullScreen);
      }
      else {
         cfgRead(*config, "DISPLAY", "Width", &glpDisplayWidth);
         cfgRead(*config, "DISPLAY", "Height", &glpDisplayHeight);
         cfgRead(*config, "DISPLAY", "XCoord", &glpDisplayX);
         cfgRead(*config, "DISPLAY", "YCoord", &glpDisplayY);
         cfgRead(*config, "DISPLAY", "Depth", &glpDisplayDepth);
         log.msg("Using default display dimensions: %dx%d,%dx%d", glpDisplayX, glpDisplayY, glpDisplayWidth, glpDisplayHeight);
      }

      cfgRead(*config, "DISPLAY", "RefreshRate", &glpRefreshRate);
      cfgRead(*config, "DISPLAY", "GammaRed", &glpGammaRed);
      cfgRead(*config, "DISPLAY", "GammaGreen", &glpGammaGreen);
      cfgRead(*config, "DISPLAY", "GammaBlue", &glpGammaBlue);
      CSTRING dpms;
      if (!cfgReadValue(*config, "DISPLAY", "DPMS", &dpms)) {
         StrCopy(dpms, glpDPMS, sizeof(glpDPMS));
      }
   }
#endif

   STRING icon_path;
   if (ResolvePath("iconsource:", 0, &icon_path) != ERR_Okay) { // The client can set iconsource: to redefine the icon origins
      icon_path = StrClone("styles:icons/");
   }

   // Icons are stored in compressed archives, accessible via "archive:icons/<category>/<icon>.svg"

   auto src = std::string(icon_path) + "Default.zip";
   if (!(glIconArchive = objCompression::create::integral(fl::Path(src), fl::ArchiveName("icons"), fl::Flags(CMF_READ_ONLY)))) {
      return ERR_CreateObject;
   }

   FreeResource(icon_path);

   // The icons: special volume is a simple reference to the archive path.

   if (SetVolume("icons", "archive:icons/", "misc/picture", NULL, NULL, VOLUME_REPLACE|VOLUME_HIDDEN)) return ERR_SetVolume;

#ifdef _WIN32 // Get any existing Windows clipboard content

   ClipHeader *clipboard;
   if (!AccessMemoryID(RPM_Clipboard, MEM_READ_WRITE, 3000, &clipboard)) {
      if (!clipboard->Init) {
         log.branch("Populating clipboard for the first time from the Windows host.");
         winCopyClipboard();
         clipboard->Init = TRUE;
         log.debranch();
      }
      else log.msg("Clipboard already initialised by other process.");
      ReleaseMemory(clipboard);
   }
   else log.warning(ERR_AccessMemory);

#endif

   return ERR_Okay;
}

//********************************************************************************************************************

static ERROR CMDOpen(OBJECTPTR Module)
{
   Module->set(FID_FunctionList, glFunctions);
   return ERR_Okay;
}

//********************************************************************************************************************

static ERROR CMDExpunge(void)
{
   pf::Log log(__FUNCTION__);
   ERROR error = ERR_Okay;

   if (glAlphaLookup)         { ReleaseMemory(glAlphaLookup); glAlphaLookup = NULL; }
   if (glDither)              { FreeResource(glDither); glDither = NULL; }
   if (glExposeHandler)       { FreeResource(glExposeHandler); glExposeHandler = NULL; }
   if (glRefreshPointerTimer) { UpdateTimer(glRefreshPointerTimer, 0); glRefreshPointerTimer = 0; }
   if (glComposite)           { acFree(glComposite); glComposite = NULL; }
   if (glCompress)            { acFree(glCompress); glCompress = NULL; }
   if (glDemultiply)          { FreeResource(glDemultiply); glDemultiply = NULL; }

   DeregisterFD((HOSTHANDLE)-2); // Disable input_event_loop()

#ifdef __xwindows__

   if (!glHeadless) {
      if (modXRR) { acFree(modXRR); modXRR = NULL; }

      if (glXFD != -1) { DeregisterFD(glXFD); glXFD = -1; }

      XSetErrorHandler(NULL);
      XSetIOErrorHandler(NULL);

      if (XDisplay) {
         free_xcursors();

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
   winTerminate();

#endif

   if (glIconArchive) { acFree(glIconArchive); glIconArchive = NULL; }
   if (clPointer)     { acFree(clPointer);     clPointer     = NULL; }
   if (clDisplay)     { acFree(clDisplay);     clDisplay     = NULL; }
   if (clBitmap)      { acFree(clBitmap);      clBitmap      = NULL; }
   if (clClipboard)   { acFree(clClipboard);   clClipboard   = NULL; }
   if (clSurface)     { acFree(clSurface);     clSurface     = NULL; }

   #ifdef _GLES_
      free_egl();
      pthread_mutex_destroy(&glGraphicsMutex);
   #endif

   return error;
}

/*********************************************************************************************************************
** Use this function to allocate simple 2D OpenGL textures.  It configures the texture so that it is suitable for basic
** rendering operations.  Note that the texture will still be bound on returning.
*/

#ifdef _GLES_
GLenum alloc_texture(LONG Width, LONG Height, GLuint *TextureID)
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

#ifdef _GLES_
/*********************************************************************************************************************
** This function is designed so that it can be re-called in case the OpenGL display needs to be reset.  THIS FUNCTION
** REQUIRES THAT THE GRAPHICS MUTEX IS LOCKED.
**
** PLEASE NOTE: EGL's design for embedded devices means that only one Display object can be active at any time.
*/

ERROR init_egl(void)
{
   pf::Log log(__FUNCTION__);
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

   glDisplayInfo.DisplayID = 0xffffffff; // Force refresh of display info cache.

   if (!glPointerID) {
      FindObject("SystemPointer", 0, 0, &glPointerID);
   }

   if (glPointerID) {
      AConfiguration *config;
      objPointer *pointer;
      if (!adGetConfig(&config)) {
         DOUBLE dp_factor = 160.0 / AConfiguration_getDensity(config);
         if (!AccessObjectID(glPointerID, 3000, &pointer)) {
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

//********************************************************************************************************************

void refresh_display_from_egl(extDisplay *Self)
{
   pf::Log log(__FUNCTION__);

   log.traceBranch("%dx%dx%d", glEGLWidth, glEGLHeight, glEGLDepth);

   glEGLRefreshDisplay = FALSE;

   Self->Width = glEGLWidth;
   Self->Height = glEGLHeight;

   ANativeWindow *window;
   if (!adGetWindow(&window)) {
      Self->WindowHandle = window;
   }

   // If the display's bitmap depth / size needs to change, resize it here.

   if (Self->Bitmap->Head.Flags & NF::INITIALISED) {
      if ((Self->Width != Self->Bitmap->Width) or (Self->Height != Self->Bitmap->Height)) {
         log.trace("Resizing OpenGL representative bitmap to match new dimensions.");
         acResize(Self->Bitmap, glEGLWidth, glEGLHeight, glEGLDepth);
      }
   }
}

/*********************************************************************************************************************
** Free EGL resources.  This does not relate to hiding or switch off of the display - in fact the display can remain
** active as it normally does.  For this reason, we just focus on resource deallocation.
*/

void free_egl(void)
{
   pf::Log log(__FUNCTION__);

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

//********************************************************************************************************************

#ifdef __xwindows__
#include "x11/handlers.cpp"
#endif

#ifdef _WIN32
#include "win32/handlers.cpp"
#endif

#ifdef __ANDROID__
#include "android/android.cpp"
#endif

//********************************************************************************************************************

PARASOL_MOD(CMDInit, NULL, CMDOpen, CMDExpunge, MODVERSION_DISPLAY, MOD_IDL, NULL)
