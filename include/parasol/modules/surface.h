#ifndef MODULES_SURFACE
#define MODULES_SURFACE 1

// Name:      surface.h
// Copyright: Paul Manias © 2000-2022
// Generator: idl-c

#ifndef MAIN_H
#include <parasol/main.h>
#endif

#define MODVERSION_SURFACE (1)

#ifndef MODULES_DISPLAY_H
#include <parasol/modules/display.h>
#endif

#define DRAG_NONE 0
#define DRAG_ANCHOR 1
#define DRAG_NORMAL 2

// Optional flags for the ExposeSurface() function.

#define EXF_CHILDREN 0x00000001
#define EXF_REDRAW_VOLATILE 0x00000002
#define EXF_REDRAW_VOLATILE_OVERLAP 0x00000004
#define EXF_ABSOLUTE_COORDS 0x00000008
#define EXF_ABSOLUTE 0x00000008
#define EXF_CURSOR_SPLIT 0x00000010

#define RT_ROOT 0x00000001

// drwLockBitmap() result flags

#define LVF_EXPOSE_CHANGES 0x00000001

// Flags for RedrawSurface().

#define IRF_IGNORE_NV_CHILDREN 0x00000001
#define IRF_IGNORE_CHILDREN 0x00000002
#define IRF_SINGLE_BITMAP 0x00000004
#define IRF_RELATIVE 0x00000008
#define IRF_FORCE_DRAW 0x00000010

// AccessSurfaceList() flags

#define ARF_READ 0x00000001
#define ARF_WRITE 0x00000002
#define ARF_UPDATE 0x00000004
#define ARF_NO_DELAY 0x00000008

// CopySurface() flags

#define BDF_SYNC 0x00000001
#define BDF_REDRAW 0x00000002
#define BDF_DITHER 0x00000004

#define DSF_NO_DRAW 0x00000001
#define DSF_NO_EXPOSE 0x00000002

// Options for the Surface WindowType field.

#define SWIN_HOST 0
#define SWIN_TASKBAR 1
#define SWIN_ICON_TRAY 2
#define SWIN_NONE 3

#define RNF_TRANSPARENT 0x00000001
#define RNF_STICK_TO_BACK 0x00000002
#define RNF_STICK_TO_FRONT 0x00000004
#define RNF_VISIBLE 0x00000008
#define RNF_STICKY 0x00000010
#define RNF_GRAB_FOCUS 0x00000020
#define RNF_HAS_FOCUS 0x00000040
#define RNF_FAST_RESIZE 0x00000080
#define RNF_DISABLED 0x00000100
#define RNF_REGION 0x00000200
#define RNF_AUTO_QUIT 0x00000400
#define RNF_HOST 0x00000800
#define RNF_PRECOPY 0x00001000
#define RNF_WRITE_ONLY 0x00002000
#define RNF_VIDEO 0x00002000
#define RNF_NO_HORIZONTAL 0x00004000
#define RNF_NO_VERTICAL 0x00008000
#define RNF_POINTER 0x00010000
#define RNF_CURSOR 0x00010000
#define RNF_SCROLL_CONTENT 0x00020000
#define RNF_AFTER_COPY 0x00040000
#define RNF_READ_ONLY 0x00050240
#define RNF_VOLATILE 0x00051000
#define RNF_FIXED_BUFFER 0x00080000
#define RNF_PERVASIVE_COPY 0x00100000
#define RNF_NO_FOCUS 0x00200000
#define RNF_FIXED_DEPTH 0x00400000
#define RNF_TOTAL_REDRAW 0x00800000
#define RNF_POST_COMPOSITE 0x01000000
#define RNF_COMPOSITE 0x01000000
#define RNF_NO_PRECOMPOSITE 0x01000000
#define RNF_FULL_SCREEN 0x02000000
#define RNF_IGNORE_FOCUS 0x04000000
#define RNF_INIT_ONLY 0x06583981
#define RNF_ASPECT_RATIO 0x08000000

struct SurfaceControl {
   LONG ListIndex;    // Byte offset of the ordered list
   LONG ArrayIndex;   // Byte offset of the list array
   LONG EntrySize;    // Byte size of each entry in the array
   LONG Total;        // Total number of entries currently in the list array
   LONG ArraySize;    // Max limit of entries in the list array
};

#define VER_SURFACEINFO 2

typedef struct SurfaceInfoV2 {
   OBJECTID ParentID;    // Object that contains the surface area
   OBJECTID BitmapID;    // Surface bitmap buffer
   OBJECTID DataMID;     // Bitmap data memory ID
   OBJECTID DisplayID;   // If the surface object is root, its display is reflected here
   LONG     Flags;       // Surface flags (RNF_VISIBLE etc)
   LONG     X;           // Horizontal coordinate
   LONG     Y;           // Vertical coordinate
   LONG     Width;       // Width of the surface area
   LONG     Height;      // Height of the surface area
   LONG     AbsX;        // Absolute X coordinate
   LONG     AbsY;        // Absolute Y coordinate
   WORD     Level;       // Branch level within the tree
   BYTE     BitsPerPixel; // Bits per pixel of the bitmap
   BYTE     BytesPerPixel; // Bytes per pixel of the bitmap
   LONG     LineWidth;   // Line width of the bitmap, in bytes
} SURFACEINFO;

struct SurfaceList {
   OBJECTID ParentID;    // Object that owns the surface area
   OBJECTID SurfaceID;   // ID of the surface area
   OBJECTID BitmapID;    // Shared bitmap buffer, if available
   OBJECTID DisplayID;   // Display
   OBJECTID DataMID;     // For drwCopySurface()
   OBJECTID TaskID;      // Task that owns the surface
   OBJECTID RootID;      // RootLayer
   OBJECTID PopOverID;
   LONG     Flags;       // Surface flags (RNF_VISIBLE etc)
   LONG     X;           // Horizontal coordinate
   LONG     Y;           // Vertical coordinate
   LONG     Width;       // Width
   LONG     Height;      // Height
   LONG     Left;        // Absolute X
   LONG     Right;       // Absolute right coordinate
   LONG     Bottom;      // Absolute bottom coordinate
   LONG     Top;         // Absolute Y
   WORD     Level;       // Level number within the hierarchy
   WORD     LineWidth;   // [applies to the bitmap owner]
   BYTE     BytesPerPixel; // [applies to the bitmap owner]
   BYTE     BitsPerPixel; // [applies to the bitmap owner]
   BYTE     Cursor;      // Preferred cursor image ID
   UBYTE    Opacity;     // Current opacity setting 0 - 255
};

struct PrecopyRegion {
   LONG X;             // X Coordinate
   LONG Y;             // Y Coordinate
   LONG Width;         // Width
   LONG Height;        // Height
   LONG XOffset;       // X offset
   LONG YOffset;       // Y offset
   WORD Dimensions;    // Dimension flags
   WORD Flags;
};

struct SurfaceCallback {
   OBJECTPTR Object;    // Context to use for the function.
   FUNCTION  Function;  // void (*Routine)(OBJECTPTR, struct Surface *, objBitmap *);
};

struct SurfaceCoords {
   LONG X;        // Horizontal coordinate
   LONG Y;        // Vertical coordinate
   LONG Width;    // Width
   LONG Height;   // Height
   LONG AbsX;     // Absolute X
   LONG AbsY;     // Absolute Y
};

// Surface class definition

#define VER_SURFACE (1.000000)

typedef struct rkSurface {
   OBJECT_HEADER
   OBJECTID DragID;     // Drag the object that this field points to
   OBJECTID BufferID;   // Buffer bitmap (backing store)
   OBJECTID ParentID;   // Graphical container of the Surface object, if any
   OBJECTID PopOverID;  // Keeps a surface in front of another surface in the Z order.
   LONG     TopMargin;  // Top movement limit
   LONG     BottomMargin; // Bottom movement limit
   LONG     LeftMargin; // Left movement limit
   LONG     RightMargin; // Right movement limit
   LONG     MinWidth;   // Minimum width setting
   LONG     MinHeight;  // Minimum height setting
   LONG     MaxWidth;   // Maximum width setting
   LONG     MaxHeight;  // Maximum height setting
   LONG     LeftLimit;  // Limits the surface area from moving too far left
   LONG     RightLimit; // Limits the surface area from moving too far right
   LONG     TopLimit;   // Limits the surface area from moving too far up
   LONG     BottomLimit; // Limits the surface area from moving too far down
   OBJECTID DisplayID;  // Refers to the Display object that is managing the surface's graphics.
   LONG     Flags;      // Special flags
   LONG     X;          // Fixed horizontal coordinate
   LONG     Y;          // Fixed vertical coordinate
   LONG     Width;      // Fixed width
   LONG     Height;     // Fixed height
   OBJECTID RootID;     // Surface that is acting as a root for many surface children (useful when applying translucency)
   OBJECTID ProgramID;  // The task that is represented by the surface object (important for linking desktop windows to foreign tasks)
   LONG     Align;      // Alignment flags
   LONG     Dimensions; // Dimension flags
   LONG     DragStatus; // Indicates the draggable state when dragging is enabled.
   LONG     Cursor;     // The preferred cursor image to use when the pointer is over the surface
   struct RGB8 Colour;  // Background colour specification
   LONG     Type;       // Internal surface type flags
   LONG     Modal;      // Set to 1 to enable modal mode

#ifdef PRV_SURFACE
   // These coordinate fields are private but may be accessed by some internal classes, like Document

   LONG     XOffset, YOffset;     // Fixed horizontal and vertical offset
   DOUBLE   XOffsetPercent;       // Relative horizontal offset
   DOUBLE   YOffsetPercent;       // Relative vertical offset
   DOUBLE   WidthPercent, HeightPercent; // Relative width and height
   DOUBLE   XPercent, YPercent;   // Relative coordinate

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
  
#endif
} objSurface;

// Surface methods

#define MT_DrwInheritedFocus -1
#define MT_DrwExpose -2
#define MT_DrwInvalidateRegion -3
#define MT_DrwSetDisplay -4
#define MT_DrwSetOpacity -5
#define MT_DrwAddCallback -6
#define MT_DrwMinimise -7
#define MT_DrwResetDimensions -8
#define MT_DrwRemoveCallback -9
#define MT_DrwScheduleRedraw -10

struct drwInheritedFocus { OBJECTID FocusID; LONG Flags;  };
struct drwExpose { LONG X; LONG Y; LONG Width; LONG Height; LONG Flags;  };
struct drwInvalidateRegion { LONG X; LONG Y; LONG Width; LONG Height;  };
struct drwSetDisplay { LONG X; LONG Y; LONG Width; LONG Height; LONG InsideWidth; LONG InsideHeight; LONG BitsPerPixel; DOUBLE RefreshRate; LONG Flags;  };
struct drwSetOpacity { DOUBLE Value; DOUBLE Adjustment;  };
struct drwAddCallback { FUNCTION * Callback;  };
struct drwResetDimensions { DOUBLE X; DOUBLE Y; DOUBLE XOffset; DOUBLE YOffset; DOUBLE Width; DOUBLE Height; LONG Dimensions;  };
struct drwRemoveCallback { FUNCTION * Callback;  };

INLINE ERROR drwInheritedFocus(APTR Ob, OBJECTID FocusID, LONG Flags) {
   struct drwInheritedFocus args = { FocusID, Flags };
   return(Action(MT_DrwInheritedFocus, (OBJECTPTR)Ob, &args));
}

INLINE ERROR drwExpose(APTR Ob, LONG X, LONG Y, LONG Width, LONG Height, LONG Flags) {
   struct drwExpose args = { X, Y, Width, Height, Flags };
   return(Action(MT_DrwExpose, (OBJECTPTR)Ob, &args));
}

INLINE ERROR drwInvalidateRegion(APTR Ob, LONG X, LONG Y, LONG Width, LONG Height) {
   struct drwInvalidateRegion args = { X, Y, Width, Height };
   return(Action(MT_DrwInvalidateRegion, (OBJECTPTR)Ob, &args));
}

INLINE ERROR drwSetDisplay(APTR Ob, LONG X, LONG Y, LONG Width, LONG Height, LONG InsideWidth, LONG InsideHeight, LONG BitsPerPixel, DOUBLE RefreshRate, LONG Flags) {
   struct drwSetDisplay args = { X, Y, Width, Height, InsideWidth, InsideHeight, BitsPerPixel, RefreshRate, Flags };
   return(Action(MT_DrwSetDisplay, (OBJECTPTR)Ob, &args));
}

INLINE ERROR drwSetOpacity(APTR Ob, DOUBLE Value, DOUBLE Adjustment) {
   struct drwSetOpacity args = { Value, Adjustment };
   return(Action(MT_DrwSetOpacity, (OBJECTPTR)Ob, &args));
}

#define drwMinimise(obj) Action(MT_DrwMinimise,(obj),0)

INLINE ERROR drwResetDimensions(APTR Ob, DOUBLE X, DOUBLE Y, DOUBLE XOffset, DOUBLE YOffset, DOUBLE Width, DOUBLE Height, LONG Dimensions) {
   struct drwResetDimensions args = { X, Y, XOffset, YOffset, Width, Height, Dimensions };
   return(Action(MT_DrwResetDimensions, (OBJECTPTR)Ob, &args));
}

#define drwScheduleRedraw(obj) Action(MT_DrwScheduleRedraw,(obj),0)


struct SurfaceBase {
   ERROR (*_GetSurfaceInfo)(OBJECTID, struct SurfaceInfoV2 **);
   ERROR (*_LockBitmap)(OBJECTID, struct rkBitmap **, LONG *);
   ERROR (*_UnlockBitmap)(OBJECTID, struct rkBitmap *);
   ERROR (*_ExposeSurface)(OBJECTID, LONG, LONG, LONG, LONG, LONG);
   ERROR (*_CopySurface)(OBJECTID, struct rkBitmap *, LONG, LONG, LONG, LONG, LONG, LONG, LONG);
   struct SurfaceControl * (*_AccessList)(LONG);
   void (*_ReleaseList)(LONG);
   OBJECTID (*_SetModalSurface)(OBJECTID);
   OBJECTID (*_GetUserFocus)(void);
   void (*_ForbidExpose)(void);
   void (*_PermitExpose)(void);
   void (*_ForbidDrawing)(void);
   void (*_PermitDrawing)(void);
   ERROR (*_GetSurfaceCoords)(OBJECTID, LONG *, LONG *, LONG *, LONG *, LONG *, LONG *);
   OBJECTID (*_GetModalSurface)(OBJECTID);
   ERROR (*_GetSurfaceFlags)(OBJECTID, LONG *);
   ERROR (*_GetVisibleArea)(OBJECTID, LONG *, LONG *, LONG *, LONG *, LONG *, LONG *);
   ERROR (*_CheckIfChild)(OBJECTID, OBJECTID);
   ERROR (*_ApplyStyleValues)(APTR, CSTRING);
   ERROR (*_ApplyStyleGraphics)(APTR, OBJECTID, CSTRING, CSTRING);
   ERROR (*_SetCurrentStyle)(CSTRING);
};

#ifndef PRV_SURFACE_MODULE
#define drwGetSurfaceInfo(...) (SurfaceBase->_GetSurfaceInfo)(__VA_ARGS__)
#define drwLockBitmap(...) (SurfaceBase->_LockBitmap)(__VA_ARGS__)
#define drwUnlockBitmap(...) (SurfaceBase->_UnlockBitmap)(__VA_ARGS__)
#define drwExposeSurface(...) (SurfaceBase->_ExposeSurface)(__VA_ARGS__)
#define drwCopySurface(...) (SurfaceBase->_CopySurface)(__VA_ARGS__)
#define drwAccessList(...) (SurfaceBase->_AccessList)(__VA_ARGS__)
#define drwReleaseList(...) (SurfaceBase->_ReleaseList)(__VA_ARGS__)
#define drwSetModalSurface(...) (SurfaceBase->_SetModalSurface)(__VA_ARGS__)
#define drwGetUserFocus(...) (SurfaceBase->_GetUserFocus)(__VA_ARGS__)
#define drwForbidExpose(...) (SurfaceBase->_ForbidExpose)(__VA_ARGS__)
#define drwPermitExpose(...) (SurfaceBase->_PermitExpose)(__VA_ARGS__)
#define drwForbidDrawing(...) (SurfaceBase->_ForbidDrawing)(__VA_ARGS__)
#define drwPermitDrawing(...) (SurfaceBase->_PermitDrawing)(__VA_ARGS__)
#define drwGetSurfaceCoords(...) (SurfaceBase->_GetSurfaceCoords)(__VA_ARGS__)
#define drwGetModalSurface(...) (SurfaceBase->_GetModalSurface)(__VA_ARGS__)
#define drwGetSurfaceFlags(...) (SurfaceBase->_GetSurfaceFlags)(__VA_ARGS__)
#define drwGetVisibleArea(...) (SurfaceBase->_GetVisibleArea)(__VA_ARGS__)
#define drwCheckIfChild(...) (SurfaceBase->_CheckIfChild)(__VA_ARGS__)
#define drwApplyStyleValues(...) (SurfaceBase->_ApplyStyleValues)(__VA_ARGS__)
#define drwApplyStyleGraphics(...) (SurfaceBase->_ApplyStyleGraphics)(__VA_ARGS__)
#define drwSetCurrentStyle(...) (SurfaceBase->_SetCurrentStyle)(__VA_ARGS__)
#endif

// Helper function for surface lookups.

INLINE LONG FIND_SURFACE_INDEX(SurfaceControl *Ctl, OBJECTID SurfaceID) {
   auto *list = (SurfaceList *)((char *)Ctl + Ctl->ArrayIndex);
   for (LONG j=0; j < Ctl->Total; j++) {
      if (list->SurfaceID IS SurfaceID) return j;
      list = (SurfaceList *)((char *)list + Ctl->EntrySize);
   }
   return -1;
}

// Stubs

INLINE ERROR drwInvalidateRegionID(OBJECTID ObjectID, LONG X, LONG Y, LONG Width, LONG Height) {
   struct drwInvalidateRegion args = { X, Y, Width, Height };
   return ActionMsg(MT_DrwInvalidateRegion, ObjectID, &args);
}

INLINE ERROR drwExposeID(OBJECTID ObjectID, LONG X, LONG Y, LONG Width, LONG Height, LONG Flags) {
   struct drwExpose args = { X, Y, Width, Height, Flags };
   return ActionMsg(MT_DrwExpose, ObjectID, &args);
}

INLINE ERROR drwSetOpacityID(OBJECTID ObjectID, DOUBLE Value, DOUBLE Adjustment) {
   struct drwSetOpacity args = { Value, Adjustment};
   return ActionMsg(MT_DrwSetOpacity, ObjectID, &args);
}

INLINE ERROR drwAddCallback(APTR Surface, APTR Callback) {
   if (Callback) {
      FUNCTION func;
      SET_FUNCTION_STDC(func, Callback);
      struct drwAddCallback args = { &func };
      return Action(MT_DrwAddCallback, Surface, &args);
   }
   else {
      struct drwAddCallback args = { NULL };
      return Action(MT_DrwAddCallback, Surface, &args);
   }
}

INLINE ERROR drwRemoveCallback(APTR Surface, APTR Callback) {
   if (Callback) {
      FUNCTION func;
      SET_FUNCTION_STDC(func, Callback);
      struct drwRemoveCallback args = { &func };
      return Action(MT_DrwRemoveCallback, Surface, &args);
   }
   else {
      struct drwRemoveCallback args = { NULL };
      return Action(MT_DrwRemoveCallback, Surface, &args);
   }
}
  
#endif
