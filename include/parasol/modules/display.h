#ifndef MODULES_DISPLAY
#define MODULES_DISPLAY 1

// Name:      display.h
// Copyright: Paul Manias 2003-2022
// Generator: idl-c

#ifndef MAIN_H
#include <parasol/main.h>
#endif

#define MODVERSION_DISPLAY (1)

#ifdef __xwindows__

#include <X11/Xlib.h>
#include <X11/extensions/XShm.h>

#endif

class objBitmap;
class objDisplay;
class objClipboard;
class objPointer;
class objSurface;

#ifdef __xwindows__

#undef NULL
#define NULL 0
    
#endif

#define DRAG_NONE 0
#define DRAG_ANCHOR 1
#define DRAG_NORMAL 2

// Events for WindowHook()

#define WH_CLOSE 1

// Colour space options.

#define CS_SRGB 1
#define CS_LINEAR_RGB 2
#define CS_CIE_LAB 3
#define CS_CIE_LCH 4

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
#define RNF_AUTO_QUIT 0x00000200
#define RNF_HOST 0x00000400
#define RNF_PRECOPY 0x00000800
#define RNF_WRITE_ONLY 0x00001000
#define RNF_VIDEO 0x00001000
#define RNF_NO_HORIZONTAL 0x00002000
#define RNF_NO_VERTICAL 0x00004000
#define RNF_POINTER 0x00008000
#define RNF_CURSOR 0x00008000
#define RNF_SCROLL_CONTENT 0x00010000
#define RNF_AFTER_COPY 0x00020000
#define RNF_READ_ONLY 0x00028040
#define RNF_VOLATILE 0x00028800
#define RNF_FIXED_BUFFER 0x00040000
#define RNF_PERVASIVE_COPY 0x00080000
#define RNF_NO_FOCUS 0x00100000
#define RNF_FIXED_DEPTH 0x00200000
#define RNF_TOTAL_REDRAW 0x00400000
#define RNF_POST_COMPOSITE 0x00800000
#define RNF_COMPOSITE 0x00800000
#define RNF_NO_PRECOMPOSITE 0x00800000
#define RNF_FULL_SCREEN 0x01000000
#define RNF_IGNORE_FOCUS 0x02000000
#define RNF_INIT_ONLY 0x032c1d81
#define RNF_ASPECT_RATIO 0x04000000

#define HOST_TRAY_ICON 1
#define HOST_TASKBAR 2
#define HOST_STICK_TO_FRONT 3
#define HOST_TRANSLUCENCE 4
#define HOST_TRANSPARENT 5

// Flags for the Pointer class.

#define PF_UNUSED 0x00000001
#define PF_VISIBLE 0x00000002
#define PF_ANCHOR 0x00000004

// Acceleration flags for GetDisplayInfo().

#define ACF_VIDEO_BLIT 0x00000001
#define ACF_SOFTWARE_BLIT 0x00000002

// Flags for the SetCursor() function.

#define CRF_LMB 0x00000001
#define CRF_MMB 0x00000002
#define CRF_RMB 0x00000004
#define CRF_RESTRICT 0x00000008
#define CRF_BUFFER 0x00000010
#define CRF_NO_BUTTONS 0x00000020

// Instructions for basic graphics operations.

#define BAF_DITHER 0x00000001
#define BAF_FILL 0x00000001
#define BAF_BLEND 0x00000002
#define BAF_COPY 0x00000004
#define BAF_LINEAR 0x00000008

// Flags for CopySurface().

#define CSRF_TRANSPARENT 0x00000001
#define CSRF_ALPHA 0x00000002
#define CSRF_TRANSLUCENT 0x00000004
#define CSRF_DEFAULT_FORMAT 0x00000008
#define CSRF_CLIP 0x00000010
#define CSRF_OFFSET 0x00000020

// Bitmap types

#define BMP_PLANAR 2
#define BMP_CHUNKY 3

// Bitmap flags

#define BMF_BLANK_PALETTE 0x00000001
#define BMF_COMPRESSED 0x00000002
#define BMF_NO_DATA 0x00000004
#define BMF_TRANSPARENT 0x00000008
#define BMF_MASK 0x00000010
#define BMF_INVERSE_ALPHA 0x00000020
#define BMF_QUERIED 0x00000040
#define BMF_CLEAR 0x00000080
#define BMF_USER 0x00000100
#define BMF_ACCELERATED_2D 0x00000200
#define BMF_ACCELERATED_3D 0x00000400
#define BMF_ALPHA_CHANNEL 0x00000800
#define BMF_NEVER_SHRINK 0x00001000
#define BMF_X11_DGA 0x00002000
#define BMF_FIXED_DEPTH 0x00004000
#define BMF_NO_BLEND 0x00008000
#define BMF_PREMUL 0x00010000

// Flags for the bitmap Flip method.

#define FLIP_HORIZONTAL 1
#define FLIP_VERTICAL 2

// Display flags.

#define SCR_READ_ONLY 0xfe300019
#define SCR_VISIBLE 0x00000001
#define SCR_AUTO_SAVE 0x00000002
#define SCR_BUFFER 0x00000004
#define SCR_NO_ACCELERATION 0x00000008
#define SCR_BIT_6 0x00000010
#define SCR_BORDERLESS 0x00000020
#define SCR_COMPOSITE 0x00000040
#define SCR_ALPHA_BLEND 0x00000040
#define SCR_MAXSIZE 0x00100000
#define SCR_REFRESH 0x00200000
#define SCR_HOSTED 0x02000000
#define SCR_POWERSAVE 0x04000000
#define SCR_DPMS_ENABLED 0x08000000
#define SCR_GTF_ENABLED 0x10000000
#define SCR_FLIPPABLE 0x20000000
#define SCR_CUSTOM_WINDOW 0x40000000
#define SCR_MAXIMISE 0x80000000

// Flags for the Display class SetMonitor() method.

#define SMF_AUTO_DETECT 0x00000001
#define SMF_BIT_6 0x00000002

// Flags for gamma operations.

#define GMF_SAVE 0x00000001

// Flags for GetDisplayType().

#define DT_NATIVE 1
#define DT_X11 2
#define DT_WINDOWS 3
#define DT_GLES 4

// Possible modes for the Display class' DPMS field.

#define DPMS_DEFAULT 0
#define DPMS_OFF 1
#define DPMS_SUSPEND 2
#define DPMS_STANDBY 3

#define CT_DATA 0
#define CT_AUDIO 1
#define CT_IMAGE 2
#define CT_FILE 3
#define CT_OBJECT 4
#define CT_TEXT 5
#define CT_END 6

// Clipboard types

#define CLIPTYPE_DATA 0x00000001
#define CLIPTYPE_AUDIO 0x00000002
#define CLIPTYPE_IMAGE 0x00000004
#define CLIPTYPE_FILE 0x00000008
#define CLIPTYPE_OBJECT 0x00000010
#define CLIPTYPE_TEXT 0x00000020

// Clipboard flags

#define CLF_DRAG_DROP 0x00000001
#define CLF_HOST 0x00000002

#define CEF_DELETE 0x00000001
#define CEF_EXTEND 0x00000002

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
   LONG     Top;         // Absolute Y
   LONG     Right;       // Absolute right coordinate
   LONG     Bottom;      // Absolute bottom coordinate
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

typedef struct PixelFormat {
   UBYTE RedShift;    // Right shift value
   UBYTE GreenShift;
   UBYTE BlueShift;
   UBYTE AlphaShift;
   UBYTE RedMask;     // The unshifted mask value (ranges from 0x00 to 0xff)
   UBYTE GreenMask;
   UBYTE BlueMask;
   UBYTE AlphaMask;
   UBYTE RedPos;      // Left shift/positional value
   UBYTE GreenPos;
   UBYTE BluePos;
   UBYTE AlphaPos;
} PIXELFORMAT;

#define VER_DISPLAYINFO 3

typedef struct DisplayInfoV3 {
   OBJECTID DisplayID;                // Object ID related to the display
   LONG     Flags;                    // Display flags
   WORD     Width;                    // Pixel width of the display
   WORD     Height;                   // Pixel height of the display
   WORD     BitsPerPixel;             // Bits per pixel
   WORD     BytesPerPixel;            // Bytes per pixel
   LARGE    AccelFlags;               // Flags describing supported hardware features.
   LONG     AmtColours;               // Total number of supported colours.
   struct PixelFormat PixelFormat;    // The colour format to use for each pixel.
   FLOAT    MinRefresh;               // Minimum refresh rate
   FLOAT    MaxRefresh;               // Maximum refresh rate
   FLOAT    RefreshRate;              // Recommended refresh rate
   LONG     Index;                    // Display mode ID (internal)
   LONG     HDensity;                 // Horizontal pixel density per inch.
   LONG     VDensity;                 // Vertical pixel density per inch.
} DISPLAYINFO;

struct CursorInfo {
   LONG Width;           // Maximum cursor width for custom cursors
   LONG Height;          // Maximum cursor height for custom cursors
   LONG Flags;           // Currently unused
   WORD BitsPerPixel;    // Preferred bits-per-pixel setting for custom cursors
};

#define VER_BITMAPSURFACE 2

typedef struct BitmapSurfaceV2 {
   APTR  Data;                   // Pointer to the bitmap graphics data.
   WORD  Width;                  // Pixel width of the bitmap.
   WORD  Height;                 // Pixel height of the bitmap.
   LONG  LineWidth;              // The distance between bitmap lines, measured in bytes.
   UBYTE BitsPerPixel;           // The number of bits per pixel (8, 15, 16, 24, 32).
   UBYTE BytesPerPixel;          // The number of bytes per pixel (1, 2, 3, 4).
   UBYTE Opacity;                // Opacity level of the source if CSRF_TRANSLUCENT is used.
   UBYTE Version;                // Version of this structure.
   LONG  Colour;                 // Colour index to use if CSRF_TRANSPARENT is used.
   struct ClipRectangle Clip;    // A clipping rectangle will restrict drawing operations to this region if CSRF_CLIP is used.
   WORD  XOffset;                // Offset all X coordinate references by the given value.
   WORD  YOffset;                // Offset all Y coordinate references by the given value.
   struct ColourFormat Format;   // The colour format of this bitmap's pixels, or alternatively use CSRF_DEFAULT_FORMAT.
   APTR  Private;                // A private pointer reserved for internal usage
} BITMAPSURFACE;

// Bitmap class definition

#define VER_BITMAP (2.000000)

// Bitmap methods

#define MT_BmpCopyArea -1
#define MT_BmpCompress -2
#define MT_BmpDecompress -3
#define MT_BmpFlip -4
#define MT_BmpDrawRectangle -6
#define MT_BmpSetClipRegion -7
#define MT_BmpGetColour -8
#define MT_BmpDrawLine -9
#define MT_BmpPremultiply -10
#define MT_BmpDemultiply -11
#define MT_BmpConvertToLinear -12
#define MT_BmpConvertToRGB -13

struct bmpCopyArea { objBitmap * DestBitmap; LONG Flags; LONG X; LONG Y; LONG Width; LONG Height; LONG XDest; LONG YDest;  };
struct bmpCompress { LONG Level;  };
struct bmpDecompress { LONG RetainData;  };
struct bmpFlip { LONG Orientation;  };
struct bmpDrawRectangle { LONG X; LONG Y; LONG Width; LONG Height; ULONG Colour; LONG Flags;  };
struct bmpSetClipRegion { LONG Number; LONG Left; LONG Top; LONG Right; LONG Bottom; LONG Terminate;  };
struct bmpGetColour { LONG Red; LONG Green; LONG Blue; LONG Alpha; ULONG Colour;  };
struct bmpDrawLine { LONG X; LONG Y; LONG XEnd; LONG YEnd; ULONG Colour;  };

INLINE ERROR bmpCopyArea(APTR Ob, objBitmap * DestBitmap, LONG Flags, LONG X, LONG Y, LONG Width, LONG Height, LONG XDest, LONG YDest) {
   struct bmpCopyArea args = { DestBitmap, Flags, X, Y, Width, Height, XDest, YDest };
   return(Action(MT_BmpCopyArea, (OBJECTPTR)Ob, &args));
}

INLINE ERROR bmpCompress(APTR Ob, LONG Level) {
   struct bmpCompress args = { Level };
   return(Action(MT_BmpCompress, (OBJECTPTR)Ob, &args));
}

INLINE ERROR bmpDecompress(APTR Ob, LONG RetainData) {
   struct bmpDecompress args = { RetainData };
   return(Action(MT_BmpDecompress, (OBJECTPTR)Ob, &args));
}

INLINE ERROR bmpFlip(APTR Ob, LONG Orientation) {
   struct bmpFlip args = { Orientation };
   return(Action(MT_BmpFlip, (OBJECTPTR)Ob, &args));
}

INLINE ERROR bmpDrawRectangle(APTR Ob, LONG X, LONG Y, LONG Width, LONG Height, ULONG Colour, LONG Flags) {
   struct bmpDrawRectangle args = { X, Y, Width, Height, Colour, Flags };
   return(Action(MT_BmpDrawRectangle, (OBJECTPTR)Ob, &args));
}

INLINE ERROR bmpSetClipRegion(APTR Ob, LONG Number, LONG Left, LONG Top, LONG Right, LONG Bottom, LONG Terminate) {
   struct bmpSetClipRegion args = { Number, Left, Top, Right, Bottom, Terminate };
   return(Action(MT_BmpSetClipRegion, (OBJECTPTR)Ob, &args));
}

INLINE ERROR bmpDrawLine(APTR Ob, LONG X, LONG Y, LONG XEnd, LONG YEnd, ULONG Colour) {
   struct bmpDrawLine args = { X, Y, XEnd, YEnd, Colour };
   return(Action(MT_BmpDrawLine, (OBJECTPTR)Ob, &args));
}

#define bmpPremultiply(obj) Action(MT_BmpPremultiply,(obj),0)

#define bmpDemultiply(obj) Action(MT_BmpDemultiply,(obj),0)

#define bmpConvertToLinear(obj) Action(MT_BmpConvertToLinear,(obj),0)

#define bmpConvertToRGB(obj) Action(MT_BmpConvertToRGB,(obj),0)


class objBitmap : public BaseClass {
   public:
   struct RGBPalette * Palette;                                      // Points to a bitmap's colour palette.
   struct ColourFormat * ColourFormat;                               // Describes the colour format used to construct each bitmap pixel.
   void (*DrawUCPixel)(objBitmap *, LONG, LONG, ULONG);              // Points to a C function that draws pixels to the bitmap using colour indexes.
   void (*DrawUCRPixel)(objBitmap *, LONG, LONG, struct RGB8 *);     // Points to a C function that draws pixels to the bitmap in RGB format.
   ULONG (*ReadUCPixel)(objBitmap *, LONG, LONG);                    // Points to a C function that reads pixels from the bitmap in colour index format.
   void (*ReadUCRPixel)(objBitmap *, LONG, LONG, struct RGB8 *);     // Points to a C function that reads pixels from the bitmap in RGB format.
   void (*ReadUCRIndex)(objBitmap *, UBYTE *, struct RGB8 *);        // Points to a C function that reads pixels from the bitmap in RGB format.
   void (*DrawUCRIndex)(objBitmap *, UBYTE *, struct RGB8 *);        // Points to a C function that draws pixels to the bitmap in RGB format.
   UBYTE *  Data;                                                    // Pointer to a bitmap's data area.
   LONG     Width;                                                   // The width of the bitmap, in pixels.
   LONG     ByteWidth;                                               // The width of the bitmap, in bytes.
   LONG     Height;                                                  // The height of the bitmap, in pixels.
   LONG     Type;                                                    // Defines the data type of the bitmap.
   LONG     LineWidth;                                               // Line differential in bytes
   LONG     PlaneMod;                                                // The differential between each bitmap plane.
   struct ClipRectangle Clip;                                        // Defines the bitmap's clipping region.
   LONG     Size;                                                    // The total size of the bitmap, in bytes.
   LONG     DataFlags;                                               // Defines the memory flags to use in allocating a bitmap's data area.
   LONG     AmtColours;                                              // The maximum number of displayable colours.
   LONG     Flags;                                                   // Optional flags.
   LONG     TransIndex;                                              // The transparent colour of the bitmap, represented as an index.
   LONG     BytesPerPixel;                                           // The number of bytes per pixel.
   LONG     BitsPerPixel;                                            // The number of bits per pixel
   LONG     Position;                                                // The current read/write data position.
   LONG     XOffset;                                                 // Private. Provided for surface/video drawing purposes - considered too advanced for standard use.
   LONG     YOffset;                                                 // Private. Provided for surface/video drawing purposes - considered too advanced for standard use.
   LONG     Opacity;                                                 // Determines the translucency setting to use in drawing operations.
   MEMORYID DataMID;                                                 // Memory ID of the bitmap's data, if applicable.
   struct RGB8 TransRGB;                                             // The transparent colour of the bitmap, in RGB format.
   struct RGB8 BkgdRGB;                                              // Background colour (for clearing, resizing)
   LONG     BkgdIndex;                                               // The bitmap's background colour is defined here as a colour index.
   LONG     ColourSpace;                                             // Defines the colour space for RGB values.
   public:
   inline ULONG getColour(UBYTE Red, UBYTE Green, UBYTE Blue, UBYTE Alpha) {
      if (BitsPerPixel > 8) return packPixel(Red, Green, Blue, Alpha);
      else {
         struct bmpGetColour args = { Red, Green, Blue, Alpha };
         if (!Action(MT_BmpGetColour, this, &args)) return args.Colour;
         return 0;
      }
   }

   inline ULONG getColour(struct RGB8 &RGB) {
      if (BitsPerPixel > 8) return packPixel(RGB);
      else {
         struct bmpGetColour args = { RGB.Red, RGB.Green, RGB.Blue, RGB.Alpha };
         if (!Action(MT_BmpGetColour, this, &args)) return args.Colour;
         return 0;
      }
   }

   inline ULONG packPixel(UBYTE R, UBYTE G, UBYTE B) {
      return
         (((R>>ColourFormat->RedShift) & ColourFormat->RedMask) << ColourFormat->RedPos) |
         (((G>>ColourFormat->GreenShift) & ColourFormat->GreenMask) << ColourFormat->GreenPos) |
         (((B>>ColourFormat->BlueShift) & ColourFormat->BlueMask) << ColourFormat->BluePos) |
         (((255>>ColourFormat->AlphaShift) & ColourFormat->AlphaMask) << ColourFormat->AlphaPos);
   }

   inline ULONG packPixel(UBYTE R, UBYTE G, UBYTE B, UBYTE A) {
      return
         (((R>>ColourFormat->RedShift) & ColourFormat->RedMask) << ColourFormat->RedPos) |
         (((G>>ColourFormat->GreenShift) & ColourFormat->GreenMask) << ColourFormat->GreenPos) |
         (((B>>ColourFormat->BlueShift) & ColourFormat->BlueMask) << ColourFormat->BluePos) |
         (((A>>ColourFormat->AlphaShift) & ColourFormat->AlphaMask) << ColourFormat->AlphaPos);
   }

   inline ULONG packPixel(struct RGB8 &RGB, UBYTE Alpha) {
      return
         (((RGB.Red>>ColourFormat->RedShift) & ColourFormat->RedMask) << ColourFormat->RedPos) |
         (((RGB.Green>>ColourFormat->GreenShift) & ColourFormat->GreenMask) << ColourFormat->GreenPos) |
         (((RGB.Blue>>ColourFormat->BlueShift) & ColourFormat->BlueMask) << ColourFormat->BluePos) |
         (((Alpha>>ColourFormat->AlphaShift) & ColourFormat->AlphaMask) << ColourFormat->AlphaPos);
   }

   inline ULONG packPixel(struct RGB8 &RGB) {
      return
         (((RGB.Red>>ColourFormat->RedShift) & ColourFormat->RedMask) << ColourFormat->RedPos) |
         (((RGB.Green>>ColourFormat->GreenShift) & ColourFormat->GreenMask) << ColourFormat->GreenPos) |
         (((RGB.Blue>>ColourFormat->BlueShift) & ColourFormat->BlueMask) << ColourFormat->BluePos) |
         (((RGB.Alpha>>ColourFormat->AlphaShift) & ColourFormat->AlphaMask) << ColourFormat->AlphaPos);
   }

   // Pack pixel 'whole-byte' version, for faster 24/32 bit formats

   inline ULONG packPixelWB(UBYTE R, UBYTE G, UBYTE B, UBYTE A = 255) {
      return (R << ColourFormat->RedPos) | (G << ColourFormat->GreenPos) | (B << ColourFormat->BluePos) | (A << ColourFormat->AlphaPos);
   }

   inline ULONG packPixelWB(struct RGB8 &RGB) {
      return (RGB.Red << ColourFormat->RedPos) | (RGB.Green << ColourFormat->GreenPos) | (RGB.Blue << ColourFormat->BluePos) | (RGB.Alpha << ColourFormat->AlphaPos);
   }

   // Colour unpacking routines

   template <class T> inline UBYTE unpackRed(T Packed)   { return (((Packed >> ColourFormat->RedPos) & ColourFormat->RedMask) << ColourFormat->RedShift); }
   template <class T> inline UBYTE unpackGreen(T Packed) { return (((Packed >> ColourFormat->GreenPos) & ColourFormat->GreenMask) << ColourFormat->GreenShift); }
   template <class T> inline UBYTE unpackBlue(T Packed)  { return (((Packed >> ColourFormat->BluePos) & ColourFormat->BlueMask) << ColourFormat->BlueShift); }
   template <class T> inline UBYTE unpackAlpha(T Packed) { return (((Packed >> ColourFormat->AlphaPos) & ColourFormat->AlphaMask)); }
  
   // Action stubs

   inline ERROR clear() { return Action(AC_Clear, this, NULL); }
   inline ERROR copyData(OBJECTID DestID) {
      struct acCopyData args = { .DestID = DestID };
      return Action(AC_CopyData, this, &args);
   }
   inline ERROR draw() { return Action(AC_Draw, this, NULL); }
   inline ERROR drawArea(LONG X, LONG Y, LONG Width, LONG Height) {
      struct acDraw args = { X, Y, Width, Height };
      return Action(AC_Draw, this, &args);
   }
   inline ERROR flush() { return Action(AC_Flush, this, NULL); }
   inline ERROR init() { return Action(AC_Init, this, NULL); }
   inline ERROR lock() { return Action(AC_Lock, this, NULL); }
   inline ERROR query() { return Action(AC_Query, this, NULL); }
   inline ERROR read(APTR Buffer, LONG Bytes, LONG *Result) {
      ERROR error;
      struct acRead read = { (BYTE *)Buffer, Bytes };
      if (!(error = Action(AC_Read, this, &read))) {
         if (Result) *Result = read.Result;
         return ERR_Okay;
      }
      else {
         if (Result) *Result = 0;
         return error;
      }
   }
   inline ERROR resize(DOUBLE Width, DOUBLE Height, DOUBLE Depth) {
      struct acResize args = { Width, Height, Depth };
      return Action(AC_Resize, this, &args);
   }
   inline ERROR saveImage(OBJECTID DestID, CLASSID ClassID) {
      struct acSaveImage args = { { DestID }, { ClassID } };
      return Action(AC_SaveImage, this, &args);
   }
   inline ERROR seek(DOUBLE Offset, LONG Position) {
      struct acSeek args = { Offset, Position };
      return Action(AC_Seek, this, &args);
   }
   inline ERROR seekStart(DOUBLE Offset)   { return seek(Offset, SEEK_START); }
   inline ERROR seekEnd(DOUBLE Offset)     { return seek(Offset, SEEK_END); }
   inline ERROR seekCurrent(DOUBLE Offset) { return seek(Offset, SEEK_CURRENT); }
   inline ERROR unlock() { return Action(AC_Unlock, this, NULL); }
   inline ERROR write(CPTR Buffer, LONG Bytes, LONG *Result) {
      ERROR error;
      struct acWrite write = { (BYTE *)Buffer, Bytes };
      if (!(error = Action(AC_Write, this, &write))) {
         if (Result) *Result = write.Result;
      }
      else if (Result) *Result = 0;
      return error;
   }
   inline LONG writeResult(CPTR Buffer, LONG Bytes) {
      struct acWrite write = { (BYTE *)Buffer, Bytes };
      if (!Action(AC_Write, this, &write)) return write.Result;
      else return 0;
   }
};

// Display class definition

#define VER_DISPLAY (1.000000)

// Display methods

#define MT_GfxWaitVBL -1
#define MT_GfxUpdatePalette -2
#define MT_GfxSetDisplay -3
#define MT_GfxSizeHints -4
#define MT_GfxSetGamma -5
#define MT_GfxSetGammaLinear -6
#define MT_GfxSetMonitor -7
#define MT_GfxMinimise -8
#define MT_GfxUpdateDisplay -9
#define MT_GfxCheckXWindow -10

struct gfxUpdatePalette { struct RGBPalette * NewPalette;  };
struct gfxSetDisplay { LONG X; LONG Y; LONG Width; LONG Height; LONG InsideWidth; LONG InsideHeight; LONG BitsPerPixel; DOUBLE RefreshRate; LONG Flags;  };
struct gfxSizeHints { LONG MinWidth; LONG MinHeight; LONG MaxWidth; LONG MaxHeight;  };
struct gfxSetGamma { DOUBLE Red; DOUBLE Green; DOUBLE Blue; LONG Flags;  };
struct gfxSetGammaLinear { DOUBLE Red; DOUBLE Green; DOUBLE Blue; LONG Flags;  };
struct gfxSetMonitor { CSTRING Name; LONG MinH; LONG MaxH; LONG MinV; LONG MaxV; LONG Flags;  };
struct gfxUpdateDisplay { objBitmap * Bitmap; LONG X; LONG Y; LONG Width; LONG Height; LONG XDest; LONG YDest;  };

#define gfxWaitVBL(obj) Action(MT_GfxWaitVBL,(obj),0)

INLINE ERROR gfxUpdatePalette(APTR Ob, struct RGBPalette * NewPalette) {
   struct gfxUpdatePalette args = { NewPalette };
   return(Action(MT_GfxUpdatePalette, (OBJECTPTR)Ob, &args));
}

INLINE ERROR gfxSetDisplay(APTR Ob, LONG X, LONG Y, LONG Width, LONG Height, LONG InsideWidth, LONG InsideHeight, LONG BitsPerPixel, DOUBLE RefreshRate, LONG Flags) {
   struct gfxSetDisplay args = { X, Y, Width, Height, InsideWidth, InsideHeight, BitsPerPixel, RefreshRate, Flags };
   return(Action(MT_GfxSetDisplay, (OBJECTPTR)Ob, &args));
}

INLINE ERROR gfxSizeHints(APTR Ob, LONG MinWidth, LONG MinHeight, LONG MaxWidth, LONG MaxHeight) {
   struct gfxSizeHints args = { MinWidth, MinHeight, MaxWidth, MaxHeight };
   return(Action(MT_GfxSizeHints, (OBJECTPTR)Ob, &args));
}

INLINE ERROR gfxSetGamma(APTR Ob, DOUBLE Red, DOUBLE Green, DOUBLE Blue, LONG Flags) {
   struct gfxSetGamma args = { Red, Green, Blue, Flags };
   return(Action(MT_GfxSetGamma, (OBJECTPTR)Ob, &args));
}

INLINE ERROR gfxSetGammaLinear(APTR Ob, DOUBLE Red, DOUBLE Green, DOUBLE Blue, LONG Flags) {
   struct gfxSetGammaLinear args = { Red, Green, Blue, Flags };
   return(Action(MT_GfxSetGammaLinear, (OBJECTPTR)Ob, &args));
}

INLINE ERROR gfxSetMonitor(APTR Ob, CSTRING Name, LONG MinH, LONG MaxH, LONG MinV, LONG MaxV, LONG Flags) {
   struct gfxSetMonitor args = { Name, MinH, MaxH, MinV, MaxV, Flags };
   return(Action(MT_GfxSetMonitor, (OBJECTPTR)Ob, &args));
}

#define gfxMinimise(obj) Action(MT_GfxMinimise,(obj),0)

INLINE ERROR gfxUpdateDisplay(APTR Ob, objBitmap * Bitmap, LONG X, LONG Y, LONG Width, LONG Height, LONG XDest, LONG YDest) {
   struct gfxUpdateDisplay args = { Bitmap, X, Y, Width, Height, XDest, YDest };
   return(Action(MT_GfxUpdateDisplay, (OBJECTPTR)Ob, &args));
}

#define gfxCheckXWindow(obj) Action(MT_GfxCheckXWindow,(obj),0)


class objDisplay : public BaseClass {
   public:
   DOUBLE   RefreshRate;  // This field manages the display refresh rate.
   objBitmap * Bitmap;    // Reference to the display's bitmap information.
   LONG     Flags;        // Optional flag settings.
   LONG     Width;        // Defines the width of the display.
   LONG     Height;       // Defines the height of the display.
   LONG     X;            // Defines the horizontal coordinate of the display.
   LONG     Y;            // Defines the vertical coordinate of the display.
   LONG     BmpX;         // The horizontal coordinate of the bitmap within a display.
   LONG     BmpY;         // The vertical coordinate of the Bitmap within a display.
   OBJECTID BufferID;     // Double buffer bitmap
   LONG     TotalMemory;  // The total amount of user accessible RAM installed on the video card, or zero if unknown.
   LONG     MinHScan;     // The minimum horizontal scan rate of the display output device.
   LONG     MaxHScan;     // The maximum horizontal scan rate of the display output device.
   LONG     MinVScan;     // The minimum vertical scan rate of the display output device.
   LONG     MaxVScan;     // The maximum vertical scan rate of the display output device.
   LONG     DisplayType;  // In hosted mode, indicates the bottom margin of the client window.
   LONG     DPMS;         // Holds the default display power management method.
   OBJECTID PopOverID;    // Enables pop-over support for hosted display windows.
   LONG     LeftMargin;   // In hosted mode, indicates the left-hand margin of the client window.
   LONG     RightMargin;  // In hosted mode, indicates the pixel margin between the client window and right window edge.
   LONG     TopMargin;    // In hosted mode, indicates the pixel margin between the client window and top window edge.
   LONG     BottomMargin; // In hosted mode, indicates the bottom margin of the client window.
   // Action stubs

   inline ERROR activate() { return Action(AC_Activate, this, NULL); }
   inline ERROR clear() { return Action(AC_Clear, this, NULL); }
   inline ERROR dataFeed(OBJECTID ObjectID, LONG Datatype, const void *Buffer, LONG Size) {
      struct acDataFeed args = { { ObjectID }, { Datatype }, Buffer, Size };
      return Action(AC_DataFeed, this, &args);
   }
   inline ERROR disable() { return Action(AC_Disable, this, NULL); }
   inline ERROR draw() { return Action(AC_Draw, this, NULL); }
   inline ERROR drawArea(LONG X, LONG Y, LONG Width, LONG Height) {
      struct acDraw args = { X, Y, Width, Height };
      return Action(AC_Draw, this, &args);
   }
   inline ERROR enable() { return Action(AC_Enable, this, NULL); }
   inline ERROR flush() { return Action(AC_Flush, this, NULL); }
   inline ERROR focus() { return Action(AC_Focus, this, NULL); }
   inline ERROR getVar(CSTRING FieldName, STRING Buffer, LONG Size) {
      struct acGetVar args = { FieldName, Buffer, Size };
      ERROR error = Action(AC_GetVar, this, &args);
      if ((error) AND (Buffer)) Buffer[0] = 0;
      return error;
   }
   inline ERROR hide() { return Action(AC_Hide, this, NULL); }
   inline ERROR init() { return Action(AC_Init, this, NULL); }
   inline ERROR move(DOUBLE X, DOUBLE Y, DOUBLE Z) {
      struct acMove args = { X, Y, Z };
      return Action(AC_Move, this, &args);
   }
   inline ERROR moveToBack() { return Action(AC_MoveToBack, this, NULL); }
   inline ERROR moveToFront() { return Action(AC_MoveToFront, this, NULL); }
   inline ERROR moveToPoint(DOUBLE X, DOUBLE Y, DOUBLE Z, LONG Flags) {
      struct acMoveToPoint moveto = { X, Y, Z, Flags };
      return Action(AC_MoveToPoint, this, &moveto);
   }
   inline ERROR redimension(DOUBLE X, DOUBLE Y, DOUBLE Z, DOUBLE Width, DOUBLE Height, DOUBLE Depth) {
      struct acRedimension args = { X, Y, Z, Width, Height, Depth };
      return Action(AC_Redimension, this, &args);
   }
   inline ERROR resize(DOUBLE Width, DOUBLE Height, DOUBLE Depth) {
      struct acResize args = { Width, Height, Depth };
      return Action(AC_Resize, this, &args);
   }
   inline ERROR saveImage(OBJECTID DestID, CLASSID ClassID) {
      struct acSaveImage args = { { DestID }, { ClassID } };
      return Action(AC_SaveImage, this, &args);
   }
   inline ERROR saveSettings() { return Action(AC_SaveSettings, this, NULL); }
   inline ERROR show() { return Action(AC_Show, this, NULL); }
};

// Clipboard class definition

#define VER_CLIPBOARD (1.000000)

// Clipboard methods

#define MT_ClipAddFile -1
#define MT_ClipAddObject -2
#define MT_ClipAddObjects -3
#define MT_ClipGetFiles -4
#define MT_ClipAddText -5
#define MT_ClipRemove -6

struct clipAddFile { LONG Datatype; CSTRING Path; LONG Flags;  };
struct clipAddObject { LONG Datatype; OBJECTID ObjectID; LONG Flags;  };
struct clipAddObjects { LONG Datatype; OBJECTID * Objects; LONG Flags;  };
struct clipGetFiles { LONG Datatype; LONG Index; CSTRING * Files; LONG Flags;  };
struct clipAddText { CSTRING String;  };
struct clipRemove { LONG Datatype;  };

INLINE ERROR clipAddFile(APTR Ob, LONG Datatype, CSTRING Path, LONG Flags) {
   struct clipAddFile args = { Datatype, Path, Flags };
   return(Action(MT_ClipAddFile, (OBJECTPTR)Ob, &args));
}

INLINE ERROR clipAddObject(APTR Ob, LONG Datatype, OBJECTID ObjectID, LONG Flags) {
   struct clipAddObject args = { Datatype, ObjectID, Flags };
   return(Action(MT_ClipAddObject, (OBJECTPTR)Ob, &args));
}

INLINE ERROR clipAddObjects(APTR Ob, LONG Datatype, OBJECTID * Objects, LONG Flags) {
   struct clipAddObjects args = { Datatype, Objects, Flags };
   return(Action(MT_ClipAddObjects, (OBJECTPTR)Ob, &args));
}

INLINE ERROR clipGetFiles(APTR Ob, LONG * Datatype, LONG Index, CSTRING ** Files, LONG * Flags) {
   struct clipGetFiles args = { 0, Index, 0, 0 };
   ERROR error = Action(MT_ClipGetFiles, (OBJECTPTR)Ob, &args);
   if (Datatype) *Datatype = args.Datatype;
   if (Files) *Files = args.Files;
   if (Flags) *Flags = args.Flags;
   return(error);
}

INLINE ERROR clipAddText(APTR Ob, CSTRING String) {
   struct clipAddText args = { String };
   return(Action(MT_ClipAddText, (OBJECTPTR)Ob, &args));
}

INLINE ERROR clipRemove(APTR Ob, LONG Datatype) {
   struct clipRemove args = { Datatype };
   return(Action(MT_ClipRemove, (OBJECTPTR)Ob, &args));
}


class objClipboard : public BaseClass {
   public:
   LONG     Flags;      // Optional flags.
   MEMORYID ClusterID;  // Identifies a unique cluster of items targeted by a clipboard object.

#ifdef PRV_CLIPBOARD
   FUNCTION RequestHandler;
   BYTE     ClusterAllocated:1;
  
#endif
   // Action stubs

   inline ERROR clear() { return Action(AC_Clear, this, NULL); }
   inline ERROR dataFeed(OBJECTID ObjectID, LONG Datatype, const void *Buffer, LONG Size) {
      struct acDataFeed args = { { ObjectID }, { Datatype }, Buffer, Size };
      return Action(AC_DataFeed, this, &args);
   }
   inline ERROR getVar(CSTRING FieldName, STRING Buffer, LONG Size) {
      struct acGetVar args = { FieldName, Buffer, Size };
      ERROR error = Action(AC_GetVar, this, &args);
      if ((error) AND (Buffer)) Buffer[0] = 0;
      return error;
   }
   inline ERROR init() { return Action(AC_Init, this, NULL); }
};

// Pointer class definition

#define VER_POINTER (1.000000)

class objPointer : public BaseClass {
   public:
   DOUBLE   Speed;         // Speed multiplier for Pointer movement.
   DOUBLE   Acceleration;  // The rate of acceleration for relative pointer movement.
   DOUBLE   DoubleClick;   // The maximum interval between two clicks for a double click to be recognised.
   DOUBLE   WheelSpeed;    // Defines a multiplier to be applied to the mouse wheel.
   DOUBLE   X;             // The horizontal position of the pointer within its parent display.
   DOUBLE   Y;             // The vertical position of the pointer within its parent display.
   DOUBLE   OverX;         // The horizontal position of the Pointer with respect to the object underneath the hot-spot.
   DOUBLE   OverY;         // The vertical position of the Pointer with respect to the object underneath the hot-spot.
   DOUBLE   OverZ;         // The position of the Pointer within an object.
   LONG     MaxSpeed;      // Restricts the maximum speed of a pointer's movement.
   OBJECTID InputID;       // Declares the I/O object to read movement from.
   OBJECTID SurfaceID;     // The top-most surface that is under the pointer's hot spot.
   OBJECTID AnchorID;      // Can refer to a surface that the pointer has been anchored to.
   LONG     CursorID;      // Sets the user's cursor image, selected from the pre-defined graphics bank.
   OBJECTID CursorOwnerID; // The current owner of the cursor, as defined by SetCursor().
   LONG     Flags;         // Optional flags.
   OBJECTID RestrictID;    // Refers to a surface when the pointer is restricted.
   LONG     HostX;         // Indicates the current position of the host cursor on Windows or X11
   LONG     HostY;         // Indicates the current position of the host cursor on Windows or X11
   OBJECTID BitmapID;      // Refers to bitmap in which custom cursor images can be drawn.
   OBJECTID DragSourceID;  // The object managing the current drag operation, as defined by StartCursorDrag().
   LONG     DragItem;      // The currently dragged item, as defined by StartCursorDrag().
   OBJECTID OverObjectID;  // Readable field that gives the ID of the object under the pointer.
   LONG     ClickSlop;     // A leniency value that assists in determining if the user intended to click or drag.
};

// Surface class definition

#define VER_SURFACE (1.000000)

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


class objSurface : public BaseClass {
   public:
   OBJECTID DragID;     // This object-based field is used to control the dragging of objects around the display.
   OBJECTID BufferID;   // The ID of the bitmap that manages the surface's graphics.
   OBJECTID ParentID;   // The parent for a surface is defined here.
   OBJECTID PopOverID;  // Keeps a surface in front of another surface in the Z order.
   LONG     TopMargin;  // Manipulates the top margin of a surface object.
   LONG     BottomMargin; // Manipulates the bottom margin of a surface object.
   LONG     LeftMargin; // Manipulates the left margin of a surface object.
   LONG     RightMargin; // Manipulates the right margin of a surface object.
   LONG     MinWidth;   // Prevents the width of a surface object from shrinking beyond a certain value.
   LONG     MinHeight;  // Prevents the height of a surface object from shrinking beyond a certain value.
   LONG     MaxWidth;   // Prevents the width of a surface object from exceeding a certain value.
   LONG     MaxHeight;  // Prevents the height of a surface object from exceeding a certain value.
   LONG     LeftLimit;  // Prevents a surface object from moving beyond a given point on the left-hand side.
   LONG     RightLimit; // Prevents a surface object from moving beyond a given point on the right-hand side.
   LONG     TopLimit;   // Prevents a surface object from moving beyond a given point at the top of its container.
   LONG     BottomLimit; // Prevents a surface object from moving beyond a given point at the bottom of its container.
   OBJECTID DisplayID;  // Refers to the @Display object that is managing the surface's graphics.
   LONG     Flags;      // Optional flags.
   LONG     X;          // Determines the horizontal position of a surface object.
   LONG     Y;          // Determines the vertical position of a surface object.
   LONG     Width;      // Defines the width of a surface object.
   LONG     Height;     // Defines the height of a surface object.
   OBJECTID RootID;     // Surface that is acting as a root for many surface children (useful when applying translucency)
   OBJECTID ProgramID;  // The task that is represented by the surface object (important for linking desktop windows to foreign tasks)
   LONG     Align;      // This field allows you to align a surface area within its owner.
   LONG     Dimensions; // Indicates currently active dimension settings.
   LONG     DragStatus; // Indicates the draggable state when dragging is enabled.
   LONG     Cursor;     // A default cursor image can be set here for changing the mouse pointer.
   struct RGB8 Colour;  // String-based field for setting the background colour.
   LONG     Type;       // Internal surface type flags
   LONG     Modal;      // Sets the surface as modal (prevents user interaction with other surfaces).

#ifdef PRV_SURFACE
   // These coordinate fields are considered private but may be accessed by some internal classes, like Document
   LONG     XOffset, YOffset;     // Fixed horizontal and vertical offset
   DOUBLE   XOffsetPercent;       // Relative horizontal offset
   DOUBLE   YOffsetPercent;       // Relative vertical offset
   DOUBLE   WidthPercent, HeightPercent; // Relative width and height
   DOUBLE   XPercent, YPercent;   // Relative coordinate
  
#endif
   // Action stubs

   inline ERROR activate() { return Action(AC_Activate, this, NULL); }
   inline ERROR disable() { return Action(AC_Disable, this, NULL); }
   inline ERROR draw() { return Action(AC_Draw, this, NULL); }
   inline ERROR drawArea(LONG X, LONG Y, LONG Width, LONG Height) {
      struct acDraw args = { X, Y, Width, Height };
      return Action(AC_Draw, this, &args);
   }
   inline ERROR enable() { return Action(AC_Enable, this, NULL); }
   inline ERROR focus() { return Action(AC_Focus, this, NULL); }
   inline ERROR hide() { return Action(AC_Hide, this, NULL); }
   inline ERROR init() { return Action(AC_Init, this, NULL); }
   inline ERROR lostFocus() { return Action(AC_LostFocus, this, NULL); }
   inline ERROR move(DOUBLE X, DOUBLE Y, DOUBLE Z) {
      struct acMove args = { X, Y, Z };
      return Action(AC_Move, this, &args);
   }
   inline ERROR moveToBack() { return Action(AC_MoveToBack, this, NULL); }
   inline ERROR moveToFront() { return Action(AC_MoveToFront, this, NULL); }
   inline ERROR moveToPoint(DOUBLE X, DOUBLE Y, DOUBLE Z, LONG Flags) {
      struct acMoveToPoint moveto = { X, Y, Z, Flags };
      return Action(AC_MoveToPoint, this, &moveto);
   }
   inline ERROR redimension(DOUBLE X, DOUBLE Y, DOUBLE Z, DOUBLE Width, DOUBLE Height, DOUBLE Depth) {
      struct acRedimension args = { X, Y, Z, Width, Height, Depth };
      return Action(AC_Redimension, this, &args);
   }
   inline ERROR resize(DOUBLE Width, DOUBLE Height, DOUBLE Depth) {
      struct acResize args = { Width, Height, Depth };
      return Action(AC_Resize, this, &args);
   }
   inline ERROR saveImage(OBJECTID DestID, CLASSID ClassID) {
      struct acSaveImage args = { { DestID }, { ClassID } };
      return Action(AC_SaveImage, this, &args);
   }
   inline ERROR scroll(DOUBLE X, DOUBLE Y, DOUBLE Z) {
      struct acScroll args = { X, Y, Z };
      return Action(AC_Scroll, this, &args);
   }
   inline ERROR scrollToPoint(DOUBLE X, DOUBLE Y, DOUBLE Z, LONG Flags) {
      struct acScrollToPoint args = { X, Y, Z, Flags };
      return Action(AC_ScrollToPoint, this, &args);
   }
   inline ERROR show() { return Action(AC_Show, this, NULL); }
};

struct DisplayBase {
   struct SurfaceControl * (*_AccessList)(LONG);
   objPointer * (*_AccessPointer)(void);
   ERROR (*_CheckIfChild)(OBJECTID, OBJECTID);
   ERROR (*_Compress)(objBitmap *, LONG);
   ERROR (*_CopyArea)(objBitmap *, objBitmap *, LONG, LONG, LONG, LONG, LONG, LONG, LONG);
   ERROR (*_CopyRawBitmap)(struct BitmapSurfaceV2 *, objBitmap *, LONG, LONG, LONG, LONG, LONG, LONG, LONG);
   ERROR (*_CopySurface)(OBJECTID, objBitmap *, LONG, LONG, LONG, LONG, LONG, LONG, LONG);
   ERROR (*_Decompress)(objBitmap *, LONG);
   void (*_DrawLine)(objBitmap *, LONG, LONG, LONG, LONG, ULONG);
   void (*_DrawPixel)(objBitmap *, LONG, LONG, ULONG);
   void (*_DrawRGBPixel)(objBitmap *, LONG, LONG, struct RGB8 *);
   void (*_DrawRectangle)(objBitmap *, LONG, LONG, LONG, LONG, ULONG, LONG);
   ERROR (*_ExposeSurface)(OBJECTID, LONG, LONG, LONG, LONG, LONG);
   void (*_FlipBitmap)(objBitmap *, LONG);
   void (*_GetColourFormat)(struct ColourFormat *, LONG, LONG, LONG, LONG, LONG);
   ERROR (*_GetCursorInfo)(struct CursorInfo *, LONG);
   ERROR (*_GetCursorPos)(DOUBLE *, DOUBLE *);
   ERROR (*_GetDisplayInfo)(OBJECTID, struct DisplayInfoV3 **);
   LONG (*_GetDisplayType)(void);
   CSTRING (*_GetInputTypeName)(LONG);
   OBJECTID (*_GetModalSurface)(OBJECTID);
   ERROR (*_GetRelativeCursorPos)(OBJECTID, DOUBLE *, DOUBLE *);
   ERROR (*_GetSurfaceCoords)(OBJECTID, LONG *, LONG *, LONG *, LONG *, LONG *, LONG *);
   ERROR (*_GetSurfaceFlags)(OBJECTID, LONG *);
   ERROR (*_GetSurfaceInfo)(OBJECTID, struct SurfaceInfoV2 **);
   OBJECTID (*_GetUserFocus)(void);
   ERROR (*_GetVisibleArea)(OBJECTID, LONG *, LONG *, LONG *, LONG *, LONG *, LONG *);
   ERROR (*_LockBitmap)(OBJECTID, objBitmap **, LONG *);
   ERROR (*_LockCursor)(OBJECTID);
   ULONG (*_ReadPixel)(objBitmap *, LONG, LONG);
   void (*_ReadRGBPixel)(objBitmap *, LONG, LONG, struct RGB8 **);
   void (*_ReleaseList)(LONG);
   ERROR (*_Resample)(objBitmap *, struct ColourFormat *);
   ERROR (*_RestoreCursor)(LONG, OBJECTID);
   DOUBLE (*_ScaleToDPI)(DOUBLE);
   ERROR (*_ScanDisplayModes)(CSTRING, struct DisplayInfoV3 *, LONG);
   void (*_SetClipRegion)(objBitmap *, LONG, LONG, LONG, LONG, LONG, LONG);
   ERROR (*_SetCursor)(OBJECTID, LONG, LONG, CSTRING, OBJECTID);
   ERROR (*_SetCursorPos)(DOUBLE, DOUBLE);
   ERROR (*_SetCustomCursor)(OBJECTID, LONG, objBitmap *, LONG, LONG, OBJECTID);
   ERROR (*_SetHostOption)(LONG, LARGE);
   OBJECTID (*_SetModalSurface)(OBJECTID);
   ERROR (*_StartCursorDrag)(OBJECTID, LONG, CSTRING, OBJECTID);
   ERROR (*_SubscribeInput)(FUNCTION *, OBJECTID, LONG, OBJECTID, LONG *);
   void (*_Sync)(objBitmap *);
   ERROR (*_UnlockBitmap)(OBJECTID, objBitmap *);
   ERROR (*_UnlockCursor)(OBJECTID);
   ERROR (*_UnsubscribeInput)(LONG);
   ERROR (*_WindowHook)(OBJECTID, LONG, FUNCTION *);
};

#ifndef PRV_DISPLAY_MODULE
#define gfxAccessList(...) (DisplayBase->_AccessList)(__VA_ARGS__)
#define gfxAccessPointer(...) (DisplayBase->_AccessPointer)(__VA_ARGS__)
#define gfxCheckIfChild(...) (DisplayBase->_CheckIfChild)(__VA_ARGS__)
#define gfxCompress(...) (DisplayBase->_Compress)(__VA_ARGS__)
#define gfxCopyArea(...) (DisplayBase->_CopyArea)(__VA_ARGS__)
#define gfxCopyRawBitmap(...) (DisplayBase->_CopyRawBitmap)(__VA_ARGS__)
#define gfxCopySurface(...) (DisplayBase->_CopySurface)(__VA_ARGS__)
#define gfxDecompress(...) (DisplayBase->_Decompress)(__VA_ARGS__)
#define gfxDrawLine(...) (DisplayBase->_DrawLine)(__VA_ARGS__)
#define gfxDrawPixel(...) (DisplayBase->_DrawPixel)(__VA_ARGS__)
#define gfxDrawRGBPixel(...) (DisplayBase->_DrawRGBPixel)(__VA_ARGS__)
#define gfxDrawRectangle(...) (DisplayBase->_DrawRectangle)(__VA_ARGS__)
#define gfxExposeSurface(...) (DisplayBase->_ExposeSurface)(__VA_ARGS__)
#define gfxFlipBitmap(...) (DisplayBase->_FlipBitmap)(__VA_ARGS__)
#define gfxGetColourFormat(...) (DisplayBase->_GetColourFormat)(__VA_ARGS__)
#define gfxGetCursorInfo(a) (DisplayBase->_GetCursorInfo)(a,sizeof(*a))
#define gfxGetCursorPos(...) (DisplayBase->_GetCursorPos)(__VA_ARGS__)
#define gfxGetDisplayInfo(...) (DisplayBase->_GetDisplayInfo)(__VA_ARGS__)
#define gfxGetDisplayType(...) (DisplayBase->_GetDisplayType)(__VA_ARGS__)
#define gfxGetInputTypeName(...) (DisplayBase->_GetInputTypeName)(__VA_ARGS__)
#define gfxGetModalSurface(...) (DisplayBase->_GetModalSurface)(__VA_ARGS__)
#define gfxGetRelativeCursorPos(...) (DisplayBase->_GetRelativeCursorPos)(__VA_ARGS__)
#define gfxGetSurfaceCoords(...) (DisplayBase->_GetSurfaceCoords)(__VA_ARGS__)
#define gfxGetSurfaceFlags(...) (DisplayBase->_GetSurfaceFlags)(__VA_ARGS__)
#define gfxGetSurfaceInfo(...) (DisplayBase->_GetSurfaceInfo)(__VA_ARGS__)
#define gfxGetUserFocus(...) (DisplayBase->_GetUserFocus)(__VA_ARGS__)
#define gfxGetVisibleArea(...) (DisplayBase->_GetVisibleArea)(__VA_ARGS__)
#define gfxLockBitmap(...) (DisplayBase->_LockBitmap)(__VA_ARGS__)
#define gfxLockCursor(...) (DisplayBase->_LockCursor)(__VA_ARGS__)
#define gfxReadPixel(...) (DisplayBase->_ReadPixel)(__VA_ARGS__)
#define gfxReadRGBPixel(...) (DisplayBase->_ReadRGBPixel)(__VA_ARGS__)
#define gfxReleaseList(...) (DisplayBase->_ReleaseList)(__VA_ARGS__)
#define gfxResample(...) (DisplayBase->_Resample)(__VA_ARGS__)
#define gfxRestoreCursor(...) (DisplayBase->_RestoreCursor)(__VA_ARGS__)
#define gfxScaleToDPI(...) (DisplayBase->_ScaleToDPI)(__VA_ARGS__)
#define gfxScanDisplayModes(a,b) (DisplayBase->_ScanDisplayModes)(a,b,sizeof(*b))
#define gfxSetClipRegion(...) (DisplayBase->_SetClipRegion)(__VA_ARGS__)
#define gfxSetCursor(...) (DisplayBase->_SetCursor)(__VA_ARGS__)
#define gfxSetCursorPos(...) (DisplayBase->_SetCursorPos)(__VA_ARGS__)
#define gfxSetCustomCursor(...) (DisplayBase->_SetCustomCursor)(__VA_ARGS__)
#define gfxSetHostOption(...) (DisplayBase->_SetHostOption)(__VA_ARGS__)
#define gfxSetModalSurface(...) (DisplayBase->_SetModalSurface)(__VA_ARGS__)
#define gfxStartCursorDrag(...) (DisplayBase->_StartCursorDrag)(__VA_ARGS__)
#define gfxSubscribeInput(...) (DisplayBase->_SubscribeInput)(__VA_ARGS__)
#define gfxSync(...) (DisplayBase->_Sync)(__VA_ARGS__)
#define gfxUnlockBitmap(...) (DisplayBase->_UnlockBitmap)(__VA_ARGS__)
#define gfxUnlockCursor(...) (DisplayBase->_UnlockCursor)(__VA_ARGS__)
#define gfxUnsubscribeInput(...) (DisplayBase->_UnsubscribeInput)(__VA_ARGS__)
#define gfxWindowHook(...) (DisplayBase->_WindowHook)(__VA_ARGS__)
#endif

// Direct ColourFormat versions

#define CFPackPixel(a,b,c,d)      ((((b)>>(a)->RedShift) & (a)->RedMask) << (a)->RedPos) | ((((c)>>(a)->GreenShift) & (a)->GreenMask) << (a)->GreenPos) | ((((d)>>(a)->BlueShift) & (a)->BlueMask) << (a)->BluePos)
#define CFPackPixelA(a,b,c,d,e)   ((((b)>>(a)->RedShift) & (a)->RedMask) << (a)->RedPos) | ((((c)>>(a)->GreenShift) & (a)->GreenMask) << (a)->GreenPos) | ((((d)>>(a)->BlueShift) & (a)->BlueMask) << (a)->BluePos) | ((((e)>>(a)->AlphaShift) & (a)->AlphaMask) << (a)->AlphaPos)
#define CFPackAlpha(a,b)          ((((b)>>(a)->AlphaShift) & (a)->AlphaMask) << (a)->AlphaPos)
#define CFPackPixelWB(a,b,c,d)    ((b) << (a)->RedPos) | ((c) << (a)->GreenPos) | ((d) << (a)->BluePos)
#define CFPackPixelWBA(a,b,c,d,e) ((b) << (a)->RedPos) | ((c) << (a)->GreenPos) | ((d) << (a)->BluePos) | ((e) << (a)->AlphaPos)
#define CFUnpackRed(a,b)          ((((b) >> (a)->RedPos) & (a)->RedMask) << (a)->RedShift)
#define CFUnpackGreen(a,b)        ((((b) >> (a)->GreenPos) & (a)->GreenMask) << (a)->GreenShift)
#define CFUnpackBlue(a,b)         ((((b) >> (a)->BluePos) & (a)->BlueMask) << (a)->BlueShift)
#define CFUnpackAlpha(a,b)        ((((b) >> (a)->AlphaPos) & (a)->AlphaMask))

#define gfxReleasePointer(a)    (ReleaseObject(a))
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

INLINE ERROR drwAddCallback(OBJECTPTR Surface, APTR Callback) {
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

INLINE ERROR drwRemoveCallback(OBJECTPTR Surface, APTR Callback) {
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
