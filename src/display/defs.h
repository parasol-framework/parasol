
//#define DEBUG
#define PRIVATE_DISPLAY
#define PRV_DISPLAY_MODULE
#define PRV_BITMAP
#define PRV_DISPLAY
#define PRV_POINTER
#define PRV_CLIPBOARD

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

struct dcDisplayInputReady { // This is an internal structure used by the display module to replace dcInputReady
   LARGE NextIndex;    // Next message index for the subscriber to look at
   LONG  SubIndex;     // Index into the InputSubscription list
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

struct InputEventMgr {
   ULONG  IndexCounter;   // Counter for message ID's
   InputEvent Msgs[MAX_INPUTMSG];
};

extern InputEventMgr *glInputEvents;

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

#define MAX_CLIPS 10     // Maximum number of clips stored in the historical buffer

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
extern void input_event_loop(HOSTHANDLE, APTR);
extern ERROR LockSurface(objBitmap *, WORD);
extern ERROR UnlockSurface(objBitmap *);
extern resolution * get_resolutions(objDisplay *);
extern ERROR create_bitmap_class(void);
extern ERROR dither(objBitmap *, objBitmap *, ColourFormat *, LONG, LONG, LONG, LONG, LONG, LONG);
extern ERROR get_display_info(OBJECTID, DISPLAYINFO *, LONG);
extern void update_displayinfo(objDisplay *);
extern void resize_feedback(FUNCTION *, OBJECTID, LONG X, LONG Y, LONG Width, LONG Height);

extern std::unordered_map<LONG, InputCallback> glInputCallbacks;
extern SharedControl *glSharedControl;
extern LONG glSixBitDisplay;
extern OBJECTPTR glModule;
extern OBJECTPTR modSurface;
extern OBJECTPTR clDisplay, clPointer, clBitmap, clClipboard;
extern OBJECTID glPointerID;
extern DISPLAYINFO *glDisplayInfo;
extern APTR glDither;
extern LONG glDitherSize;
extern OBJECTPTR glCompress;
extern objCompression *glIconArchive;
extern struct CoreBase *CoreBase;
extern struct SurfaceBase *SurfaceBase;
extern ColourFormat glColourFormat;
extern BYTE glHeadless;
extern FieldDef CursorLookup[];

struct InputType {
   LONG Flags;  // As many flags as necessary to describe the input type
   LONG Mask;   // Limited flags to declare the mask that must be used to receive that type
};

extern const std::array<struct InputType, JET_END> glInputType;
extern const std::array<std::string, JET_END> glInputNames;

//****************************************************************************

#define BLEND_MAX_THRESHOLD 255
#define BLEND_MIN_THRESHOLD 1

#ifndef PI
#define PI (3.141592653589793238462643383279f)
#endif

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

extern int winAddClip(int Datatype, void * Data, int Size, int Cut);
extern void winClearClipboard(void);
extern void winCopyClipboard(void);
extern int winExtractFile(void *pida, int Index, char *Result, int Size);
extern void winGetClip(int Datatype);
extern void winTerminate(void);

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

#include "prototypes.h"
#include "win32/windows.h"

#endif
