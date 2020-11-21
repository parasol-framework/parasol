#ifndef MODULES_WINDOW
#define MODULES_WINDOW 1

// Name:      window.h
// Copyright: Paul Manias © 2003-2020
// Generator: idl-c

#ifndef MAIN_H
#include <parasol/main.h>
#endif

#define MODVERSION_WINDOW (1)

#ifndef MODULES_SURFACE_H
#include <parasol/modules/surface.h>
#endif

// Window flags.

#define WNF_DISABLED 0x00000001
#define WNF_SMART_LIMITS 0x00000002
#define WNF_BACKGROUND 0x00000004
#define WNF_VIDEO 0x00000008
#define WNF_NO_MARGINS 0x00000010
#define WNF_BORDERLESS 0x00000020
#define WNF_FORCE_POS 0x00000040

// The orientation to use for the display when the window is maximised.

#define WOR_ANY 0
#define WOR_PORTRAIT 1
#define WOR_LANDSCAPE 2

// Window class definition

#define VER_WINDOW (1.000000)

typedef struct rkWindow {
   OBJECT_HEADER
   struct rkSurface * Surface;    // The window surface
   LONG     Flags;                // Special options
   LONG     InsideBorder;         // Set to TRUE to draw a border at the edges of the window
   LONG     Center;               // Set to TRUE if the window position should be centered
   LONG     Minimise;             // Set to TRUE to enable the minimise gadget
   LONG     Maximise;             // Set to TRUE to enable the maximise gadget
   LONG     MoveToBack;           // Set to TRUE to enable the movetoback gadget
   LONG     Close;                // Set to TRUE to enable the close gadget
   LONG     Quit;                 // If TRUE, a quit message will be sent when the window is closed
   LONG     RestoreX;             // X coordinate to restore when reversing the maximise operation
   LONG     RestoreY;             // Y coordinate to restore when reversing the maximise operation
   LONG     RestoreWidth;         // Width to restore when reversing the maximise operation
   LONG     RestoreHeight;        // Height to restore when reversing the maximise operation
   LONG     Focus;                // Set to TRUE if the window should get the focus whenever it is shown
   OBJECTID TitleID;              // Refers to the text object that controls the window title
   OBJECTID MinimiseID;           // The surface that represents the minimise gadget
   OBJECTID MaximiseID;           // The surface that represents the maximise gadget
   OBJECTID MoveToBackID;         // The surface that represents the move-to-back gadget
   OBJECTID CloseID;              // The surface that represents the close gadget
   LONG     ResizeFlags;
   LONG     ResizeBorder;         // Pixel width of the resize border
   OBJECTID CanvasID;
   OBJECTID UserFocusID;          // Child surface that currently holds the user's focus
   LONG     Orientation;          // The WOR graphics orientation to use when this window owns the display.
   LONG     ClientLeft;           // Pixels dedicated to the client area (window border)
   LONG     ClientRight;
   LONG     ClientTop;
   LONG     ClientBottom;

#ifdef PRV_WINDOW
 PRV_WINDOW_FIELDS 
#endif
} objWindow;

// Window methods

#define MT_WinMaximise -1
#define MT_WinMinimise -2
#define MT_WinClose -3

struct winMaximise { LONG Toggle;  };

INLINE ERROR winMaximise(APTR Ob, LONG Toggle) {
   struct winMaximise args = { Toggle };
   return(Action(MT_WinMaximise, (OBJECTPTR)Ob, &args));
}

#define winMinimise(obj) Action(MT_WinMinimise,(obj),0)

#define winClose(obj) Action(MT_WinClose,(obj),0)


#endif
