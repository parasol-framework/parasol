#pragma once

// Name:      display.h
// Copyright: Paul Manias 2003-2023
// Generator: idl-c

#include <parasol/main.h>

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

enum class DRAG : LONG {
   NIL = 0,
   NONE = 0,
   ANCHOR = 1,
   NORMAL = 2,
};

// Events for WindowHook()

enum class WH : LONG {
   NIL = 0,
   CLOSE = 1,
};

// Colour space options.

enum class CS : LONG {
   NIL = 0,
   SRGB = 1,
   LINEAR_RGB = 2,
   CIE_LAB = 3,
   CIE_LCH = 4,
};

// Optional flags for the ExposeSurface() function.

enum class EXF : ULONG {
   NIL = 0,
   CHILDREN = 0x00000001,
   REDRAW_VOLATILE = 0x00000002,
   REDRAW_VOLATILE_OVERLAP = 0x00000004,
   ABSOLUTE_COORDS = 0x00000008,
   ABSOLUTE = 0x00000008,
   CURSOR_SPLIT = 0x00000010,
};

DEFINE_ENUM_FLAG_OPERATORS(EXF)

enum class RT : ULONG {
   NIL = 0,
   ROOT = 0x00000001,
};

DEFINE_ENUM_FLAG_OPERATORS(RT)

// drwLockBitmap() result flags

enum class LVF : ULONG {
   NIL = 0,
   EXPOSE_CHANGES = 0x00000001,
};

DEFINE_ENUM_FLAG_OPERATORS(LVF)

// Flags for RedrawSurface().

enum class IRF : ULONG {
   NIL = 0,
   IGNORE_NV_CHILDREN = 0x00000001,
   IGNORE_CHILDREN = 0x00000002,
   SINGLE_BITMAP = 0x00000004,
   RELATIVE = 0x00000008,
   FORCE_DRAW = 0x00000010,
   REDRAWS_CHILDREN = 0x00000020,
};

DEFINE_ENUM_FLAG_OPERATORS(IRF)

// CopySurface() flags

enum class BDF : ULONG {
   NIL = 0,
   REDRAW = 0x00000001,
   DITHER = 0x00000002,
};

DEFINE_ENUM_FLAG_OPERATORS(BDF)

enum class DSF : ULONG {
   NIL = 0,
   NO_DRAW = 0x00000001,
   NO_EXPOSE = 0x00000002,
};

DEFINE_ENUM_FLAG_OPERATORS(DSF)

// Options for the Surface WindowType field.

enum class SWIN : LONG {
   NIL = 0,
   HOST = 0,
   TASKBAR = 1,
   ICON_TRAY = 2,
   NONE = 3,
};

// Switches for the Surface class' Flags field.

enum class RNF : ULONG {
   NIL = 0,
   TRANSPARENT = 0x00000001,
   STICK_TO_BACK = 0x00000002,
   STICK_TO_FRONT = 0x00000004,
   VISIBLE = 0x00000008,
   STICKY = 0x00000010,
   GRAB_FOCUS = 0x00000020,
   HAS_FOCUS = 0x00000040,
   DISABLED = 0x00000080,
   AUTO_QUIT = 0x00000100,
   HOST = 0x00000200,
   PRECOPY = 0x00000400,
   WRITE_ONLY = 0x00000800,
   VIDEO = 0x00000800,
   NO_HORIZONTAL = 0x00001000,
   NO_VERTICAL = 0x00002000,
   CURSOR = 0x00004000,
   POINTER = 0x00004000,
   SCROLL_CONTENT = 0x00008000,
   AFTER_COPY = 0x00010000,
   READ_ONLY = 0x00014040,
   VOLATILE = 0x00014400,
   FIXED_BUFFER = 0x00020000,
   PERVASIVE_COPY = 0x00040000,
   NO_FOCUS = 0x00080000,
   FIXED_DEPTH = 0x00100000,
   TOTAL_REDRAW = 0x00200000,
   POST_COMPOSITE = 0x00400000,
   COMPOSITE = 0x00400000,
   NO_PRECOMPOSITE = 0x00400000,
   FULL_SCREEN = 0x00800000,
   IGNORE_FOCUS = 0x01000000,
   INIT_ONLY = 0x01960e81,
   ASPECT_RATIO = 0x02000000,
};

DEFINE_ENUM_FLAG_OPERATORS(RNF)

enum class HOST : LONG {
   NIL = 0,
   TRAY_ICON = 1,
   TASKBAR = 2,
   STICK_TO_FRONT = 3,
   TRANSLUCENCE = 4,
   TRANSPARENT = 5,
};

// Flags for the Pointer class.

enum class PF : ULONG {
   NIL = 0,
   UNUSED = 0x00000001,
   VISIBLE = 0x00000002,
   ANCHOR = 0x00000004,
};

DEFINE_ENUM_FLAG_OPERATORS(PF)

// Acceleration flags for GetDisplayInfo().

enum class ACF : ULONG {
   NIL = 0,
   VIDEO_BLIT = 0x00000001,
   SOFTWARE_BLIT = 0x00000002,
};

DEFINE_ENUM_FLAG_OPERATORS(ACF)

// Flags for the SetCursor() function.

enum class CRF : ULONG {
   NIL = 0,
   LMB = 0x00000001,
   MMB = 0x00000002,
   RMB = 0x00000004,
   RESTRICT = 0x00000008,
   BUFFER = 0x00000010,
   NO_BUTTONS = 0x00000020,
};

DEFINE_ENUM_FLAG_OPERATORS(CRF)

// Instructions for basic graphics operations.

enum class BAF : ULONG {
   NIL = 0,
   DITHER = 0x00000001,
   FILL = 0x00000001,
   BLEND = 0x00000002,
   COPY = 0x00000004,
   LINEAR = 0x00000008,
};

DEFINE_ENUM_FLAG_OPERATORS(BAF)

// Flags for CopySurface().

enum class CSRF : ULONG {
   NIL = 0,
   TRANSPARENT = 0x00000001,
   ALPHA = 0x00000002,
   TRANSLUCENT = 0x00000004,
   DEFAULT_FORMAT = 0x00000008,
   CLIP = 0x00000010,
   OFFSET = 0x00000020,
};

DEFINE_ENUM_FLAG_OPERATORS(CSRF)

// Bitmap types

enum class BMP : LONG {
   NIL = 0,
   PLANAR = 2,
   CHUNKY = 3,
};

// Bitmap flags

enum class BMF : ULONG {
   NIL = 0,
   BLANK_PALETTE = 0x00000001,
   COMPRESSED = 0x00000002,
   NO_DATA = 0x00000004,
   TRANSPARENT = 0x00000008,
   MASK = 0x00000010,
   INVERSE_ALPHA = 0x00000020,
   QUERIED = 0x00000040,
   CLEAR = 0x00000080,
   USER = 0x00000100,
   ACCELERATED_2D = 0x00000200,
   ACCELERATED_3D = 0x00000400,
   ALPHA_CHANNEL = 0x00000800,
   NEVER_SHRINK = 0x00001000,
   X11_DGA = 0x00002000,
   FIXED_DEPTH = 0x00004000,
   NO_BLEND = 0x00008000,
   PREMUL = 0x00010000,
};

DEFINE_ENUM_FLAG_OPERATORS(BMF)

// Flags for the bitmap Flip method.

enum class FLIP : LONG {
   NIL = 0,
   HORIZONTAL = 1,
   VERTICAL = 2,
};

// Display flags.

enum class SCR : ULONG {
   NIL = 0,
   READ_ONLY = 0xfe300019,
   VISIBLE = 0x00000001,
   AUTO_SAVE = 0x00000002,
   BUFFER = 0x00000004,
   NO_ACCELERATION = 0x00000008,
   BIT_6 = 0x00000010,
   BORDERLESS = 0x00000020,
   COMPOSITE = 0x00000040,
   ALPHA_BLEND = 0x00000040,
   MAXSIZE = 0x00100000,
   REFRESH = 0x00200000,
   HOSTED = 0x02000000,
   POWERSAVE = 0x04000000,
   DPMS_ENABLED = 0x08000000,
   GTF_ENABLED = 0x10000000,
   FLIPPABLE = 0x20000000,
   CUSTOM_WINDOW = 0x40000000,
   MAXIMISE = 0x80000000,
};

DEFINE_ENUM_FLAG_OPERATORS(SCR)

// Flags for the Display class SetMonitor() method.

enum class MON : ULONG {
   NIL = 0,
   AUTO_DETECT = 0x00000001,
   BIT_6 = 0x00000002,
};

DEFINE_ENUM_FLAG_OPERATORS(MON)

// Flags for gamma operations.

enum class GMF : ULONG {
   NIL = 0,
   SAVE = 0x00000001,
};

DEFINE_ENUM_FLAG_OPERATORS(GMF)

// Flags for GetDisplayType().

enum class DT : LONG {
   NIL = 0,
   NATIVE = 1,
   X11 = 2,
   WINGDI = 3,
   GLES = 4,
};

// Possible modes for the Display class' PowerMode field.

enum class DPMS : LONG {
   NIL = 0,
   DEFAULT = 0,
   OFF = 1,
   SUSPEND = 2,
   STANDBY = 3,
};

enum class CT : LONG {
   NIL = 0,
   DATA = 0,
   AUDIO = 1,
   IMAGE = 2,
   FILE = 3,
   OBJECT = 4,
   TEXT = 5,
   END = 6,
};

// Clipboard types

enum class CLIPTYPE : ULONG {
   NIL = 0,
   DATA = 0x00000001,
   AUDIO = 0x00000002,
   IMAGE = 0x00000004,
   FILE = 0x00000008,
   OBJECT = 0x00000010,
   TEXT = 0x00000020,
};

DEFINE_ENUM_FLAG_OPERATORS(CLIPTYPE)

// Clipboard flags

enum class CPF : ULONG {
   NIL = 0,
   DRAG_DROP = 0x00000001,
   HOST = 0x00000002,
   HISTORY_BUFFER = 0x00000004,
};

DEFINE_ENUM_FLAG_OPERATORS(CPF)

enum class CEF : ULONG {
   NIL = 0,
   DELETE = 0x00000001,
   EXTEND = 0x00000002,
};

DEFINE_ENUM_FLAG_OPERATORS(CEF)

#define VER_SURFACEINFO 2

typedef struct SurfaceInfoV2 {
   APTR     Data;        // Bitmap data memory ID
   OBJECTID ParentID;    // Object that contains the surface area
   OBJECTID BitmapID;    // Surface bitmap buffer
   OBJECTID DisplayID;   // Refers to the display if this object is at root level
   RNF      Flags;       // Surface flags
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
   inline bool visible() const { return (Flags & RNF::VISIBLE) != RNF::NIL; }
   inline bool invisible() const { return (Flags & RNF::VISIBLE) IS RNF::NIL; }
   inline bool hasFocus() const { return (Flags & RNF::HAS_FOCUS) != RNF::NIL; }
   inline bool transparent() const { return (Flags & RNF::TRANSPARENT) != RNF::NIL; }
} SURFACEINFO;

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
   UBYTE GreenShift;  // Green shift value
   UBYTE BlueShift;   // Blue shift value
   UBYTE AlphaShift;  // Alpha shift value
   UBYTE RedMask;     // The unshifted red mask value (ranges from 0x00 to 0xff)
   UBYTE GreenMask;   // The unshifted green mask value (ranges from 0x00 to 0xff)
   UBYTE BlueMask;    // The unshifted blue mask value (ranges from 0x00 to 0xff)
   UBYTE AlphaMask;   // The unshifted alpha mask value (ranges from 0x00 to 0xff)
   UBYTE RedPos;      // Left shift/positional value for red
   UBYTE GreenPos;    // Left shift/positional value for green
   UBYTE BluePos;     // Left shift/positional value for blue
   UBYTE AlphaPos;    // Left shift/positional value for alpha
} PIXELFORMAT;

#define VER_DISPLAYINFO 3

typedef struct DisplayInfoV3 {
   OBJECTID DisplayID;                // Object ID related to the display
   SCR      Flags;                    // Display flags
   WORD     Width;                    // Pixel width of the display
   WORD     Height;                   // Pixel height of the display
   WORD     BitsPerPixel;             // Bits per pixel
   WORD     BytesPerPixel;            // Bytes per pixel
   ACF      AccelFlags;               // Flags describing supported hardware features.
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
   UBYTE Opacity;                // Opacity level of the source if CSRF::TRANSLUCENT is used.
   UBYTE Version;                // Version of this structure.
   LONG  Colour;                 // Colour index to use if CSRF::TRANSPARENT is used.
   struct ClipRectangle Clip;    // A clipping rectangle will restrict drawing operations to this region if CSRF::CLIP is used.
   WORD  XOffset;                // Offset all X coordinate references by the given value.
   WORD  YOffset;                // Offset all Y coordinate references by the given value.
   struct ColourFormat Format;   // The colour format of this bitmap's pixels, or alternatively use CSRF::DEFAULT_FORMAT.
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
#define MT_BmpPremultiply -10
#define MT_BmpDemultiply -11
#define MT_BmpConvertToLinear -12
#define MT_BmpConvertToRGB -13

struct bmpCopyArea { objBitmap * DestBitmap; BAF Flags; LONG X; LONG Y; LONG Width; LONG Height; LONG XDest; LONG YDest;  };
struct bmpCompress { LONG Level;  };
struct bmpDecompress { LONG RetainData;  };
struct bmpFlip { FLIP Orientation;  };
struct bmpDrawRectangle { LONG X; LONG Y; LONG Width; LONG Height; ULONG Colour; BAF Flags;  };
struct bmpSetClipRegion { LONG Number; LONG Left; LONG Top; LONG Right; LONG Bottom; LONG Terminate;  };
struct bmpGetColour { LONG Red; LONG Green; LONG Blue; LONG Alpha; ULONG Colour;  };

INLINE ERROR bmpCopyArea(APTR Ob, objBitmap * DestBitmap, BAF Flags, LONG X, LONG Y, LONG Width, LONG Height, LONG XDest, LONG YDest) {
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

INLINE ERROR bmpFlip(APTR Ob, FLIP Orientation) {
   struct bmpFlip args = { Orientation };
   return(Action(MT_BmpFlip, (OBJECTPTR)Ob, &args));
}

INLINE ERROR bmpDrawRectangle(APTR Ob, LONG X, LONG Y, LONG Width, LONG Height, ULONG Colour, BAF Flags) {
   struct bmpDrawRectangle args = { X, Y, Width, Height, Colour, Flags };
   return(Action(MT_BmpDrawRectangle, (OBJECTPTR)Ob, &args));
}

INLINE ERROR bmpSetClipRegion(APTR Ob, LONG Number, LONG Left, LONG Top, LONG Right, LONG Bottom, LONG Terminate) {
   struct bmpSetClipRegion args = { Number, Left, Top, Right, Bottom, Terminate };
   return(Action(MT_BmpSetClipRegion, (OBJECTPTR)Ob, &args));
}

#define bmpPremultiply(obj) Action(MT_BmpPremultiply,(obj),0)

#define bmpDemultiply(obj) Action(MT_BmpDemultiply,(obj),0)

#define bmpConvertToLinear(obj) Action(MT_BmpConvertToLinear,(obj),0)

#define bmpConvertToRGB(obj) Action(MT_BmpConvertToRGB,(obj),0)


class objBitmap : public BaseClass {
   public:
   static constexpr CLASSID CLASS_ID = ID_BITMAP;
   static constexpr CSTRING CLASS_NAME = "Bitmap";

   using create = pf::Create<objBitmap>;

   struct RGBPalette * Palette;                                      // Points to a bitmap's colour palette.
   struct ColourFormat * ColourFormat;                               // Describes the colour format used to construct each bitmap pixel.
   void (*DrawUCPixel)(objBitmap *, LONG, LONG, ULONG);              // Points to a C function that draws pixels to the bitmap using colour indexes.
   void (*DrawUCRPixel)(objBitmap *, LONG, LONG, struct RGB8 *);     // Points to a C function that draws pixels to the bitmap in RGB format.
   ULONG (*ReadUCPixel)(objBitmap *, LONG, LONG);                    // Points to a C function that reads pixels from the bitmap in colour index format.
   void (*ReadUCRPixel)(objBitmap *, LONG, LONG, struct RGB8 *);     // Points to a C function that reads pixels from the bitmap in RGB format.
   void (*ReadUCRIndex)(objBitmap *, UBYTE *, struct RGB8 *);        // Points to a C function that reads pixels from the bitmap in RGB format.
   void (*DrawUCRIndex)(objBitmap *, UBYTE *, struct RGB8 *);        // Points to a C function that draws pixels to the bitmap in RGB format.
   UBYTE * Data;                                                     // Pointer to a bitmap's data area.
   LONG    Width;                                                    // The width of the bitmap, in pixels.
   LONG    ByteWidth;                                                // The width of the bitmap, in bytes.
   LONG    Height;                                                   // The height of the bitmap, in pixels.
   BMP     Type;                                                     // Defines the data type of the bitmap.
   LONG    LineWidth;                                                // Line differential in bytes
   LONG    PlaneMod;                                                 // The differential between each bitmap plane.
   struct ClipRectangle Clip;                                        // Defines the bitmap's clipping region.
   LONG    Size;                                                     // The total size of the bitmap, in bytes.
   MEM     DataFlags;                                                // Defines the memory flags to use in allocating a bitmap's data area.
   LONG    AmtColours;                                               // The maximum number of displayable colours.
   BMF     Flags;                                                    // Optional flags.
   LONG    TransIndex;                                               // The transparent colour of the bitmap, represented as an index.
   LONG    BytesPerPixel;                                            // The number of bytes per pixel.
   LONG    BitsPerPixel;                                             // The number of bits per pixel
   LONG    Position;                                                 // The current read/write data position.
   LONG    XOffset;                                                  // Private. Provided for surface/video drawing purposes - considered too advanced for standard use.
   LONG    YOffset;                                                  // Private. Provided for surface/video drawing purposes - considered too advanced for standard use.
   LONG    Opacity;                                                  // Determines the translucency setting to use in drawing operations.
   struct RGB8 TransRGB;                                             // The transparent colour of the bitmap, in RGB format.
   struct RGB8 BkgdRGB;                                              // Background colour (for clearing, resizing)
   LONG    BkgdIndex;                                                // The bitmap's background colour is defined here as a colour index.
   CS      ColourSpace;                                              // Defines the colour space for RGB values.
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
   
   inline ULONG packPixelWB(struct RGB8 &RGB, UBYTE Alpha) {
      return (RGB.Red << ColourFormat->RedPos) | (RGB.Green << ColourFormat->GreenPos) | (RGB.Blue << ColourFormat->BluePos) | (Alpha << ColourFormat->AlphaPos);
   }

   // Colour unpacking routines

   template <class T> inline UBYTE unpackRed(T Packed)   { return (((Packed >> ColourFormat->RedPos) & ColourFormat->RedMask) << ColourFormat->RedShift); }
   template <class T> inline UBYTE unpackGreen(T Packed) { return (((Packed >> ColourFormat->GreenPos) & ColourFormat->GreenMask) << ColourFormat->GreenShift); }
   template <class T> inline UBYTE unpackBlue(T Packed)  { return (((Packed >> ColourFormat->BluePos) & ColourFormat->BlueMask) << ColourFormat->BlueShift); }
   template <class T> inline UBYTE unpackAlpha(T Packed) { return (((Packed >> ColourFormat->AlphaPos) & ColourFormat->AlphaMask)); }

   // Action stubs

   inline ERROR clear() { return Action(AC_Clear, this, NULL); }
   inline ERROR copyData(OBJECTPTR Dest) {
      struct acCopyData args = { .Dest = Dest };
      return Action(AC_CopyData, this, &args);
   }
   inline ERROR draw() { return Action(AC_Draw, this, NULL); }
   inline ERROR drawArea(LONG X, LONG Y, LONG Width, LONG Height) {
      struct acDraw args = { X, Y, Width, Height };
      return Action(AC_Draw, this, &args);
   }
   inline ERROR flush() { return Action(AC_Flush, this, NULL); }
   inline ERROR init() { return InitObject(this); }
   inline ERROR lock() { return Action(AC_Lock, this, NULL); }
   inline ERROR query() { return Action(AC_Query, this, NULL); }
   template <class T, class U> ERROR read(APTR Buffer, T Size, U *Result) {
      static_assert(std::is_integral<U>::value, "Result value must be an integer type");
      static_assert(std::is_integral<T>::value, "Size value must be an integer type");
      ERROR error;
      const LONG bytes = (Size > 0x7fffffff) ? 0x7fffffff : Size;
      struct acRead read = { (BYTE *)Buffer, bytes };
      if (!(error = Action(AC_Read, this, &read))) *Result = static_cast<U>(read.Result);
      else *Result = 0;
      return error;
   }
   template <class T> ERROR read(APTR Buffer, T Size) {
      static_assert(std::is_integral<T>::value, "Size value must be an integer type");
      const LONG bytes = (Size > 0x7fffffff) ? 0x7fffffff : Size;
      struct acRead read = { (BYTE *)Buffer, bytes };
      return Action(AC_Read, this, &read);
   }
   inline ERROR resize(DOUBLE Width, DOUBLE Height, DOUBLE Depth = 0) {
      struct acResize args = { Width, Height, Depth };
      return Action(AC_Resize, this, &args);
   }
   inline ERROR saveImage(OBJECTPTR Dest, CLASSID ClassID = 0) {
      struct acSaveImage args = { Dest, { ClassID } };
      return Action(AC_SaveImage, this, &args);
   }
   inline ERROR seek(DOUBLE Offset, SEEK Position = SEEK::CURRENT) {
      struct acSeek args = { Offset, Position };
      return Action(AC_Seek, this, &args);
   }
   inline ERROR seekStart(DOUBLE Offset)   { return seek(Offset, SEEK::START); }
   inline ERROR seekEnd(DOUBLE Offset)     { return seek(Offset, SEEK::END); }
   inline ERROR seekCurrent(DOUBLE Offset) { return seek(Offset, SEEK::CURRENT); }
   inline ERROR unlock() { return Action(AC_Unlock, this, NULL); }
   inline ERROR write(CPTR Buffer, LONG Size, LONG *Result = NULL) {
      ERROR error;
      struct acWrite write = { (BYTE *)Buffer, Size };
      if (!(error = Action(AC_Write, this, &write))) {
         if (Result) *Result = write.Result;
      }
      else if (Result) *Result = 0;
      return error;
   }
   inline ERROR write(std::string Buffer, LONG *Result = NULL) {
      ERROR error;
      struct acWrite write = { (BYTE *)Buffer.c_str(), LONG(Buffer.size()) };
      if (!(error = Action(AC_Write, this, &write))) {
         if (Result) *Result = write.Result;
      }
      else if (Result) *Result = 0;
      return error;
   }
   inline LONG writeResult(CPTR Buffer, LONG Size) {
      struct acWrite write = { (BYTE *)Buffer, Size };
      if (!Action(AC_Write, this, &write)) return write.Result;
      else return 0;
   }

   // Customised field setting

   inline ERROR setPalette(struct RGBPalette * Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[31];
      return field->WriteValue(target, field, 0x08000300, Value, 1);
   }

   inline ERROR setData(UBYTE * Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[25];
      return field->WriteValue(target, field, 0x08000500, Value, 1);
   }

   inline ERROR setWidth(const LONG Value) {
      if (this->initialised()) return ERR_NoFieldAccess;
      this->Width = Value;
      return ERR_Okay;
   }

   inline ERROR setHeight(const LONG Value) {
      if (this->initialised()) return ERR_NoFieldAccess;
      this->Height = Value;
      return ERR_Okay;
   }

   inline ERROR setType(const BMP Value) {
      if (this->initialised()) return ERR_NoFieldAccess;
      this->Type = Value;
      return ERR_Okay;
   }

   inline ERROR setClip(struct ClipRectangle * Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[24];
      return field->WriteValue(target, field, 0x08000310, Value, 1);
   }

   inline ERROR setDataFlags(const MEM Value) {
      if (this->initialised()) return ERR_NoFieldAccess;
      this->DataFlags = Value;
      return ERR_Okay;
   }

   inline ERROR setAmtColours(const LONG Value) {
      if (this->initialised()) return ERR_NoFieldAccess;
      this->AmtColours = Value;
      return ERR_Okay;
   }

   inline ERROR setFlags(const BMF Value) {
      if (this->initialised()) return ERR_NoFieldAccess;
      this->Flags = Value;
      return ERR_Okay;
   }

   inline ERROR setTransIndex(const LONG Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[30];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERROR setBytesPerPixel(const LONG Value) {
      if (this->initialised()) return ERR_NoFieldAccess;
      this->BytesPerPixel = Value;
      return ERR_Okay;
   }

   inline ERROR setBitsPerPixel(const LONG Value) {
      if (this->initialised()) return ERR_NoFieldAccess;
      this->BitsPerPixel = Value;
      return ERR_Okay;
   }

   inline ERROR setXOffset(const LONG Value) {
      this->XOffset = Value;
      return ERR_Okay;
   }

   inline ERROR setYOffset(const LONG Value) {
      this->YOffset = Value;
      return ERR_Okay;
   }

   inline ERROR setOpacity(const LONG Value) {
      this->Opacity = Value;
      return ERR_Okay;
   }

   inline ERROR setTransRGB(const struct RGB8 * Value, LONG Elements) {
      auto target = this;
      auto field = &this->Class->Dictionary[34];
      return field->WriteValue(target, field, 0x01081300, Value, Elements);
   }

   inline ERROR setBkgdIndex(const LONG Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[10];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERROR setColourSpace(const CS Value) {
      this->ColourSpace = Value;
      return ERR_Okay;
   }

   inline ERROR setClipLeft(const LONG Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[15];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERROR setClipRight(const LONG Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[13];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERROR setClipBottom(const LONG Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[18];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERROR setClipTop(const LONG Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[38];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERROR setBkgd(const BYTE * Value, LONG Elements) {
      auto target = this;
      auto field = &this->Class->Dictionary[23];
      return field->WriteValue(target, field, 0x01081300, Value, Elements);
   }

   inline ERROR setHandle(APTR Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[0];
      return field->WriteValue(target, field, 0x08010300, Value, 1);
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
#define MT_GfxCheckXWindow -9

struct gfxUpdatePalette { struct RGBPalette * NewPalette;  };
struct gfxSetDisplay { LONG X; LONG Y; LONG Width; LONG Height; LONG InsideWidth; LONG InsideHeight; LONG BitsPerPixel; DOUBLE RefreshRate; LONG Flags;  };
struct gfxSizeHints { LONG MinWidth; LONG MinHeight; LONG MaxWidth; LONG MaxHeight; LONG EnforceAspect;  };
struct gfxSetGamma { DOUBLE Red; DOUBLE Green; DOUBLE Blue; GMF Flags;  };
struct gfxSetGammaLinear { DOUBLE Red; DOUBLE Green; DOUBLE Blue; GMF Flags;  };
struct gfxSetMonitor { CSTRING Name; LONG MinH; LONG MaxH; LONG MinV; LONG MaxV; MON Flags;  };

#define gfxWaitVBL(obj) Action(MT_GfxWaitVBL,(obj),0)

INLINE ERROR gfxUpdatePalette(APTR Ob, struct RGBPalette * NewPalette) {
   struct gfxUpdatePalette args = { NewPalette };
   return(Action(MT_GfxUpdatePalette, (OBJECTPTR)Ob, &args));
}

INLINE ERROR gfxSetDisplay(APTR Ob, LONG X, LONG Y, LONG Width, LONG Height, LONG InsideWidth, LONG InsideHeight, LONG BitsPerPixel, DOUBLE RefreshRate, LONG Flags) {
   struct gfxSetDisplay args = { X, Y, Width, Height, InsideWidth, InsideHeight, BitsPerPixel, RefreshRate, Flags };
   return(Action(MT_GfxSetDisplay, (OBJECTPTR)Ob, &args));
}

INLINE ERROR gfxSizeHints(APTR Ob, LONG MinWidth, LONG MinHeight, LONG MaxWidth, LONG MaxHeight, LONG EnforceAspect) {
   struct gfxSizeHints args = { MinWidth, MinHeight, MaxWidth, MaxHeight, EnforceAspect };
   return(Action(MT_GfxSizeHints, (OBJECTPTR)Ob, &args));
}

INLINE ERROR gfxSetGamma(APTR Ob, DOUBLE Red, DOUBLE Green, DOUBLE Blue, GMF Flags) {
   struct gfxSetGamma args = { Red, Green, Blue, Flags };
   return(Action(MT_GfxSetGamma, (OBJECTPTR)Ob, &args));
}

INLINE ERROR gfxSetGammaLinear(APTR Ob, DOUBLE Red, DOUBLE Green, DOUBLE Blue, GMF Flags) {
   struct gfxSetGammaLinear args = { Red, Green, Blue, Flags };
   return(Action(MT_GfxSetGammaLinear, (OBJECTPTR)Ob, &args));
}

INLINE ERROR gfxSetMonitor(APTR Ob, CSTRING Name, LONG MinH, LONG MaxH, LONG MinV, LONG MaxV, MON Flags) {
   struct gfxSetMonitor args = { Name, MinH, MaxH, MinV, MaxV, Flags };
   return(Action(MT_GfxSetMonitor, (OBJECTPTR)Ob, &args));
}

#define gfxMinimise(obj) Action(MT_GfxMinimise,(obj),0)

#define gfxCheckXWindow(obj) Action(MT_GfxCheckXWindow,(obj),0)


class objDisplay : public BaseClass {
   public:
   static constexpr CLASSID CLASS_ID = ID_DISPLAY;
   static constexpr CSTRING CLASS_NAME = "Display";

   using create = pf::Create<objDisplay>;

   DOUBLE   RefreshRate;  // This field manages the display refresh rate.
   objBitmap * Bitmap;    // Reference to the display's bitmap information.
   SCR      Flags;        // Optional flag settings.
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
   DT       DisplayType;  // In hosted mode, indicates the bottom margin of the client window.
   DPMS     PowerMode;    // The display's power management method.
   OBJECTID PopOverID;    // Enables pop-over support for hosted display windows.
   LONG     LeftMargin;   // In hosted mode, indicates the left-hand margin of the client window.
   LONG     RightMargin;  // In hosted mode, indicates the pixel margin between the client window and right window edge.
   LONG     TopMargin;    // In hosted mode, indicates the pixel margin between the client window and top window edge.
   LONG     BottomMargin; // In hosted mode, indicates the bottom margin of the client window.

   // Action stubs

   inline ERROR activate() { return Action(AC_Activate, this, NULL); }
   inline ERROR clear() { return Action(AC_Clear, this, NULL); }
   inline ERROR dataFeed(OBJECTPTR Object, DATA Datatype, const void *Buffer, LONG Size) {
      struct acDataFeed args = { Object, Datatype, Buffer, Size };
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
      if ((error) and (Buffer)) Buffer[0] = 0;
      return error;
   }
   inline ERROR hide() { return Action(AC_Hide, this, NULL); }
   inline ERROR init() { return InitObject(this); }
   inline ERROR move(DOUBLE X, DOUBLE Y, DOUBLE Z) {
      struct acMove args = { X, Y, Z };
      return Action(AC_Move, this, &args);
   }
   inline ERROR moveToBack() { return Action(AC_MoveToBack, this, NULL); }
   inline ERROR moveToFront() { return Action(AC_MoveToFront, this, NULL); }
   inline ERROR moveToPoint(DOUBLE X, DOUBLE Y, DOUBLE Z, MTF Flags) {
      struct acMoveToPoint moveto = { X, Y, Z, Flags };
      return Action(AC_MoveToPoint, this, &moveto);
   }
   inline ERROR redimension(DOUBLE X, DOUBLE Y, DOUBLE Z, DOUBLE Width, DOUBLE Height, DOUBLE Depth) {
      struct acRedimension args = { X, Y, Z, Width, Height, Depth };
      return Action(AC_Redimension, this, &args);
   }
   inline ERROR redimension(DOUBLE X, DOUBLE Y, DOUBLE Width, DOUBLE Height) {
      struct acRedimension args = { X, Y, 0, Width, Height, 0 };
      return Action(AC_Redimension, this, &args);
   }
   inline ERROR resize(DOUBLE Width, DOUBLE Height, DOUBLE Depth = 0) {
      struct acResize args = { Width, Height, Depth };
      return Action(AC_Resize, this, &args);
   }
   inline ERROR saveImage(OBJECTPTR Dest, CLASSID ClassID = 0) {
      struct acSaveImage args = { Dest, { ClassID } };
      return Action(AC_SaveImage, this, &args);
   }
   inline ERROR saveSettings() { return Action(AC_SaveSettings, this, NULL); }
   inline ERROR show() { return Action(AC_Show, this, NULL); }

   // Customised field setting

   inline ERROR setRefreshRate(const DOUBLE Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[43];
      return field->WriteValue(target, field, FD_DOUBLE, &Value, 1);
   }

   inline ERROR setFlags(const SCR Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[4];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERROR setWidth(const LONG Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[8];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERROR setHeight(const LONG Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[2];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERROR setX(const LONG Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[0];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERROR setY(const LONG Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[1];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERROR setBmpX(const LONG Value) {
      this->BmpX = Value;
      return ERR_Okay;
   }

   inline ERROR setBmpY(const LONG Value) {
      this->BmpY = Value;
      return ERR_Okay;
   }

   inline ERROR setPowerMode(const DPMS Value) {
      this->PowerMode = Value;
      return ERR_Okay;
   }

   inline ERROR setPopOver(const OBJECTID Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[27];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERROR setGamma(const DOUBLE * Value, LONG Elements) {
      auto target = this;
      auto field = &this->Class->Dictionary[5];
      return field->WriteValue(target, field, 0x80001508, Value, Elements);
   }

   inline ERROR setHDensity(const LONG Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[17];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERROR setVDensity(const LONG Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[15];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERROR setOpacity(const DOUBLE Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[16];
      return field->WriteValue(target, field, FD_DOUBLE, &Value, 1);
   }

   inline ERROR setResizeFeedback(const FUNCTION Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[32];
      return field->WriteValue(target, field, FD_FUNCTION, &Value, 1);
   }

   inline ERROR setWindowHandle(APTR Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[11];
      return field->WriteValue(target, field, 0x08000308, Value, 1);
   }

   template <class T> inline ERROR setTitle(T && Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[7];
      return field->WriteValue(target, field, 0x08800308, to_cstring(Value), 1);
   }

};

// Clipboard class definition

#define VER_CLIPBOARD (1.000000)

// Clipboard methods

#define MT_ClipAddFile -1
#define MT_ClipAddObjects -2
#define MT_ClipGetFiles -3
#define MT_ClipAddText -4
#define MT_ClipRemove -5

struct clipAddFile { CLIPTYPE Datatype; CSTRING Path; CEF Flags;  };
struct clipAddObjects { CLIPTYPE Datatype; OBJECTID * Objects; CEF Flags;  };
struct clipGetFiles { CLIPTYPE Datatype; LONG Index; CSTRING * Files; CEF Flags;  };
struct clipAddText { CSTRING String;  };
struct clipRemove { CLIPTYPE Datatype;  };

INLINE ERROR clipAddFile(APTR Ob, CLIPTYPE Datatype, CSTRING Path, CEF Flags) {
   struct clipAddFile args = { Datatype, Path, Flags };
   return(Action(MT_ClipAddFile, (OBJECTPTR)Ob, &args));
}

INLINE ERROR clipAddObjects(APTR Ob, CLIPTYPE Datatype, OBJECTID * Objects, CEF Flags) {
   struct clipAddObjects args = { Datatype, Objects, Flags };
   return(Action(MT_ClipAddObjects, (OBJECTPTR)Ob, &args));
}

INLINE ERROR clipGetFiles(APTR Ob, CLIPTYPE * Datatype, LONG Index, CSTRING ** Files, CEF * Flags) {
   struct clipGetFiles args = { (CLIPTYPE)0, Index, (CSTRING *)0, (CEF)0 };
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

INLINE ERROR clipRemove(APTR Ob, CLIPTYPE Datatype) {
   struct clipRemove args = { Datatype };
   return(Action(MT_ClipRemove, (OBJECTPTR)Ob, &args));
}


class objClipboard : public BaseClass {
   public:
   static constexpr CLASSID CLASS_ID = ID_CLIPBOARD;
   static constexpr CSTRING CLASS_NAME = "Clipboard";

   using create = pf::Create<objClipboard>;

   CPF Flags;    // Optional flags.

#ifdef PRV_CLIPBOARD
   FUNCTION RequestHandler;
#endif

   // Action stubs

   inline ERROR clear() { return Action(AC_Clear, this, NULL); }
   inline ERROR dataFeed(OBJECTPTR Object, DATA Datatype, const void *Buffer, LONG Size) {
      struct acDataFeed args = { Object, Datatype, Buffer, Size };
      return Action(AC_DataFeed, this, &args);
   }
   inline ERROR init() { return InitObject(this); }

   // Customised field setting

   inline ERROR setFlags(const CPF Value) {
      if (this->initialised()) return ERR_NoFieldAccess;
      this->Flags = Value;
      return ERR_Okay;
   }

   inline ERROR setRequestHandler(FUNCTION Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[3];
      return field->WriteValue(target, field, FD_FUNCTION, &Value, 1);
   }

};

// Pointer class definition

#define VER_POINTER (1.000000)

class objPointer : public BaseClass {
   public:
   static constexpr CLASSID CLASS_ID = ID_POINTER;
   static constexpr CSTRING CLASS_NAME = "Pointer";

   using create = pf::Create<objPointer>;

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
   PTC      CursorID;      // Sets the user's cursor image, selected from the pre-defined graphics bank.
   OBJECTID CursorOwnerID; // The current owner of the cursor, as defined by SetCursor().
   PF       Flags;         // Optional flags.
   OBJECTID RestrictID;    // Refers to a surface when the pointer is restricted.
   LONG     HostX;         // Indicates the current position of the host cursor on Windows or X11
   LONG     HostY;         // Indicates the current position of the host cursor on Windows or X11
   objBitmap * Bitmap;     // Refers to bitmap in which custom cursor images can be drawn.
   OBJECTID DragSourceID;  // The object managing the current drag operation, as defined by StartCursorDrag().
   LONG     DragItem;      // The currently dragged item, as defined by StartCursorDrag().
   OBJECTID OverObjectID;  // Readable field that gives the ID of the object under the pointer.
   LONG     ClickSlop;     // A leniency value that assists in determining if the user intended to click or drag.

   // Customised field setting

   inline ERROR setSpeed(const DOUBLE Value) {
      this->Speed = Value;
      return ERR_Okay;
   }

   inline ERROR setAcceleration(const DOUBLE Value) {
      this->Acceleration = Value;
      return ERR_Okay;
   }

   inline ERROR setDoubleClick(const DOUBLE Value) {
      this->DoubleClick = Value;
      return ERR_Okay;
   }

   inline ERROR setWheelSpeed(const DOUBLE Value) {
      this->WheelSpeed = Value;
      return ERR_Okay;
   }

   inline ERROR setX(const DOUBLE Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[0];
      return field->WriteValue(target, field, FD_DOUBLE, &Value, 1);
   }

   inline ERROR setY(const DOUBLE Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[1];
      return field->WriteValue(target, field, FD_DOUBLE, &Value, 1);
   }

   inline ERROR setMaxSpeed(const LONG Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[21];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERROR setInput(const OBJECTID Value) {
      this->InputID = Value;
      return ERR_Okay;
   }

   inline ERROR setSurface(const OBJECTID Value) {
      this->SurfaceID = Value;
      return ERR_Okay;
   }

   inline ERROR setCursor(const PTC Value) {
      if (this->initialised()) return ERR_NoFieldAccess;
      this->CursorID = Value;
      return ERR_Okay;
   }

   inline ERROR setCursorOwner(const OBJECTID Value) {
      this->CursorOwnerID = Value;
      return ERR_Okay;
   }

   inline ERROR setFlags(const PF Value) {
      if (this->initialised()) return ERR_NoFieldAccess;
      this->Flags = Value;
      return ERR_Okay;
   }

   inline ERROR setClickSlop(const LONG Value) {
      this->ClickSlop = Value;
      return ERR_Okay;
   }

   template <class T> inline ERROR setButtonOrder(T && Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[13];
      return field->WriteValue(target, field, 0x08800300, to_cstring(Value), 1);
   }

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

struct drwInheritedFocus { OBJECTID FocusID; RNF Flags;  };
struct drwExpose { LONG X; LONG Y; LONG Width; LONG Height; EXF Flags;  };
struct drwInvalidateRegion { LONG X; LONG Y; LONG Width; LONG Height;  };
struct drwSetDisplay { LONG X; LONG Y; LONG Width; LONG Height; LONG InsideWidth; LONG InsideHeight; LONG BitsPerPixel; DOUBLE RefreshRate; LONG Flags;  };
struct drwSetOpacity { DOUBLE Value; DOUBLE Adjustment;  };
struct drwAddCallback { FUNCTION * Callback;  };
struct drwResetDimensions { DOUBLE X; DOUBLE Y; DOUBLE XOffset; DOUBLE YOffset; DOUBLE Width; DOUBLE Height; LONG Dimensions;  };
struct drwRemoveCallback { FUNCTION * Callback;  };

INLINE ERROR drwInheritedFocus(APTR Ob, OBJECTID FocusID, RNF Flags) {
   struct drwInheritedFocus args = { FocusID, Flags };
   return(Action(MT_DrwInheritedFocus, (OBJECTPTR)Ob, &args));
}

INLINE ERROR drwExpose(APTR Ob, LONG X, LONG Y, LONG Width, LONG Height, EXF Flags) {
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
   static constexpr CLASSID CLASS_ID = ID_SURFACE;
   static constexpr CSTRING CLASS_NAME = "Surface";

   using create = pf::Create<objSurface>;

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
   RNF      Flags;      // Optional flags.
   LONG     X;          // Determines the horizontal position of a surface object.
   LONG     Y;          // Determines the vertical position of a surface object.
   LONG     Width;      // Defines the width of a surface object.
   LONG     Height;     // Defines the height of a surface object.
   OBJECTID RootID;     // Surface that is acting as a root for many surface children (useful when applying translucency)
   ALIGN    Align;      // This field allows you to align a surface area within its owner.
   LONG     Dimensions; // Indicates currently active dimension settings.
   DRAG     DragStatus; // Indicates the draggable state when dragging is enabled.
   PTC      Cursor;     // A default cursor image can be set here for changing the mouse pointer.
   struct RGB8 Colour;  // String-based field for setting the background colour.
   RT       Type;       // Internal surface type flags
   LONG     Modal;      // Sets the surface as modal (prevents user interaction with other surfaces).

#ifdef PRV_SURFACE
   // These coordinate fields are considered private but may be accessed by some internal classes, like Document
   LONG     XOffset, YOffset;     // Fixed horizontal and vertical offset
   DOUBLE   XOffsetPercent;       // Relative horizontal offset
   DOUBLE   YOffsetPercent;       // Relative vertical offset
   DOUBLE   WidthPercent, HeightPercent; // Relative width and height
   DOUBLE   XPercent, YPercent;   // Relative coordinate
#endif
   public:
   inline bool visible() const { return (Flags & RNF::VISIBLE) != RNF::NIL; }
   inline bool invisible() const { return (Flags & RNF::VISIBLE) IS RNF::NIL; }
   inline bool hasFocus() const { return (Flags & RNF::HAS_FOCUS) != RNF::NIL; }
   inline bool transparent() const { return (Flags & RNF::TRANSPARENT) != RNF::NIL; }
   inline bool disabled() const { return (Flags & RNF::DISABLED) != RNF::NIL; }
   inline bool isCursor() const { return (Flags & RNF::CURSOR) != RNF::NIL; }

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
   inline ERROR init() { return InitObject(this); }
   inline ERROR lostFocus() { return Action(AC_LostFocus, this, NULL); }
   inline ERROR move(DOUBLE X, DOUBLE Y, DOUBLE Z) {
      struct acMove args = { X, Y, Z };
      return Action(AC_Move, this, &args);
   }
   inline ERROR moveToBack() { return Action(AC_MoveToBack, this, NULL); }
   inline ERROR moveToFront() { return Action(AC_MoveToFront, this, NULL); }
   inline ERROR moveToPoint(DOUBLE X, DOUBLE Y, DOUBLE Z, MTF Flags) {
      struct acMoveToPoint moveto = { X, Y, Z, Flags };
      return Action(AC_MoveToPoint, this, &moveto);
   }
   inline ERROR redimension(DOUBLE X, DOUBLE Y, DOUBLE Z, DOUBLE Width, DOUBLE Height, DOUBLE Depth) {
      struct acRedimension args = { X, Y, Z, Width, Height, Depth };
      return Action(AC_Redimension, this, &args);
   }
   inline ERROR redimension(DOUBLE X, DOUBLE Y, DOUBLE Width, DOUBLE Height) {
      struct acRedimension args = { X, Y, 0, Width, Height, 0 };
      return Action(AC_Redimension, this, &args);
   }
   inline ERROR resize(DOUBLE Width, DOUBLE Height, DOUBLE Depth = 0) {
      struct acResize args = { Width, Height, Depth };
      return Action(AC_Resize, this, &args);
   }
   inline ERROR saveImage(OBJECTPTR Dest, CLASSID ClassID = 0) {
      struct acSaveImage args = { Dest, { ClassID } };
      return Action(AC_SaveImage, this, &args);
   }
   inline ERROR scroll(DOUBLE X, DOUBLE Y, DOUBLE Z = 0) {
      struct acScroll args = { X, Y, Z };
      return Action(AC_Scroll, this, &args);
   }
   inline ERROR scrollToPoint(DOUBLE X, DOUBLE Y, DOUBLE Z, STP Flags) {
      struct acScrollToPoint args = { X, Y, Z, Flags };
      return Action(AC_ScrollToPoint, this, &args);
   }
   inline ERROR show() { return Action(AC_Show, this, NULL); }

   // Customised field setting

   inline ERROR setDrag(const OBJECTID Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[29];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERROR setParent(const OBJECTID Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[15];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERROR setPopOver(const OBJECTID Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[40];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERROR setTopMargin(const LONG Value) {
      this->TopMargin = Value;
      return ERR_Okay;
   }

   inline ERROR setBottomMargin(const LONG Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[43];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERROR setLeftMargin(const LONG Value) {
      this->LeftMargin = Value;
      return ERR_Okay;
   }

   inline ERROR setRightMargin(const LONG Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[38];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERROR setMinWidth(const LONG Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[37];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERROR setMinHeight(const LONG Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[33];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERROR setMaxWidth(const LONG Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[23];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERROR setMaxHeight(const LONG Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[16];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERROR setLeftLimit(const LONG Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[5];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERROR setRightLimit(const LONG Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[19];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERROR setTopLimit(const LONG Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[52];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERROR setBottomLimit(const LONG Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[50];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERROR setFlags(const RNF Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[8];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERROR setX(const LONG Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[0];
      Variable var(Value);
      return field->WriteValue(target, field, FD_VARIABLE, &var, 1);
   }

   inline ERROR setY(const LONG Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[1];
      Variable var(Value);
      return field->WriteValue(target, field, FD_VARIABLE, &var, 1);
   }

   inline ERROR setWidth(const LONG Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[12];
      Variable var(Value);
      return field->WriteValue(target, field, FD_VARIABLE, &var, 1);
   }

   inline ERROR setHeight(const LONG Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[2];
      Variable var(Value);
      return field->WriteValue(target, field, FD_VARIABLE, &var, 1);
   }

   inline ERROR setAlign(const ALIGN Value) {
      this->Align = Value;
      return ERR_Okay;
   }

   inline ERROR setDimensions(const LONG Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[35];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERROR setCursor(const PTC Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[53];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERROR setColour(const struct RGB8 Value) {
      this->Colour = Value;
      return ERR_Okay;
   }

   inline ERROR setType(const RT Value) {
      if (this->initialised()) return ERR_NoFieldAccess;
      this->Type = Value;
      return ERR_Okay;
   }

   inline ERROR setModal(const LONG Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[9];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERROR setRootLayer(const OBJECTID Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[39];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERROR setAbsX(const LONG Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[27];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERROR setAbsY(const LONG Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[28];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERROR setBitsPerPixel(const LONG Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[41];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERROR setInsideHeight(const LONG Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[47];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERROR setInsideWidth(const LONG Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[36];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERROR setMovement(const LONG Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[34];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERROR setOpacity(const DOUBLE Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[25];
      return field->WriteValue(target, field, FD_DOUBLE, &Value, 1);
   }

   inline ERROR setRevertFocus(const OBJECTID Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[18];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERROR setVisible(const LONG Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[26];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERROR setWindowType(const LONG Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[32];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERROR setWindowHandle(APTR Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[21];
      return field->WriteValue(target, field, 0x08000308, Value, 1);
   }

   inline ERROR setXOffset(const LONG Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[17];
      Variable var(Value);
      return field->WriteValue(target, field, FD_VARIABLE, &var, 1);
   }

   inline ERROR setYOffset(const LONG Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[24];
      Variable var(Value);
      return field->WriteValue(target, field, FD_VARIABLE, &var, 1);
   }

};

#ifdef PARASOL_STATIC
#define JUMPTABLE_DISPLAY static struct DisplayBase *DisplayBase;
#else
#define JUMPTABLE_DISPLAY struct DisplayBase *DisplayBase;
#endif

struct DisplayBase {
#ifndef PARASOL_STATIC
   objPointer * (*_AccessPointer)(void);
   ERROR (*_CheckIfChild)(OBJECTID Parent, OBJECTID Child);
   ERROR (*_CopyArea)(objBitmap * Bitmap, objBitmap * Dest, BAF Flags, LONG X, LONG Y, LONG Width, LONG Height, LONG XDest, LONG YDest);
   ERROR (*_CopyRawBitmap)(struct BitmapSurfaceV2 * Surface, objBitmap * Bitmap, CSRF Flags, LONG X, LONG Y, LONG Width, LONG Height, LONG XDest, LONG YDest);
   ERROR (*_CopySurface)(OBJECTID Surface, objBitmap * Bitmap, BDF Flags, LONG X, LONG Y, LONG Width, LONG Height, LONG XDest, LONG YDest);
   void (*_DrawPixel)(objBitmap * Bitmap, LONG X, LONG Y, ULONG Colour);
   void (*_DrawRGBPixel)(objBitmap * Bitmap, LONG X, LONG Y, struct RGB8 * RGB);
   void (*_DrawRectangle)(objBitmap * Bitmap, LONG X, LONG Y, LONG Width, LONG Height, ULONG Colour, BAF Flags);
   ERROR (*_ExposeSurface)(OBJECTID Surface, LONG X, LONG Y, LONG Width, LONG Height, EXF Flags);
   void (*_FlipBitmap)(objBitmap * Bitmap, FLIP Orientation);
   void (*_GetColourFormat)(struct ColourFormat * Format, LONG BitsPerPixel, LONG RedMask, LONG GreenMask, LONG BlueMask, LONG AlphaMask);
   ERROR (*_GetCursorInfo)(struct CursorInfo * Info, LONG Size);
   ERROR (*_GetCursorPos)(DOUBLE * X, DOUBLE * Y);
   ERROR (*_GetDisplayInfo)(OBJECTID Display, struct DisplayInfoV3 ** Info);
   DT (*_GetDisplayType)(void);
   CSTRING (*_GetInputTypeName)(JET Type);
   OBJECTID (*_GetModalSurface)(void);
   ERROR (*_GetRelativeCursorPos)(OBJECTID Surface, DOUBLE * X, DOUBLE * Y);
   ERROR (*_GetSurfaceCoords)(OBJECTID Surface, LONG * X, LONG * Y, LONG * AbsX, LONG * AbsY, LONG * Width, LONG * Height);
   ERROR (*_GetSurfaceFlags)(OBJECTID Surface, RNF * Flags);
   ERROR (*_GetSurfaceInfo)(OBJECTID Surface, struct SurfaceInfoV2 ** Info);
   OBJECTID (*_GetUserFocus)(void);
   ERROR (*_GetVisibleArea)(OBJECTID Surface, LONG * X, LONG * Y, LONG * AbsX, LONG * AbsY, LONG * Width, LONG * Height);
   ERROR (*_LockBitmap)(OBJECTID Surface, objBitmap ** Bitmap, LVF * Info);
   ERROR (*_LockCursor)(OBJECTID Surface);
   ULONG (*_ReadPixel)(objBitmap * Bitmap, LONG X, LONG Y);
   void (*_ReadRGBPixel)(objBitmap * Bitmap, LONG X, LONG Y, struct RGB8 ** RGB);
   ERROR (*_Resample)(objBitmap * Bitmap, struct ColourFormat * ColourFormat);
   ERROR (*_RestoreCursor)(PTC Cursor, OBJECTID Owner);
   DOUBLE (*_ScaleToDPI)(DOUBLE Value);
   ERROR (*_ScanDisplayModes)(CSTRING Filter, struct DisplayInfoV3 * Info, LONG Size);
   void (*_SetClipRegion)(objBitmap * Bitmap, LONG Number, LONG Left, LONG Top, LONG Right, LONG Bottom, LONG Terminate);
   ERROR (*_SetCursor)(OBJECTID Surface, CRF Flags, PTC Cursor, CSTRING Name, OBJECTID Owner);
   ERROR (*_SetCursorPos)(DOUBLE X, DOUBLE Y);
   ERROR (*_SetCustomCursor)(OBJECTID Surface, CRF Flags, objBitmap * Bitmap, LONG HotX, LONG HotY, OBJECTID Owner);
   ERROR (*_SetHostOption)(HOST Option, LARGE Value);
   OBJECTID (*_SetModalSurface)(OBJECTID Surface);
   ERROR (*_StartCursorDrag)(OBJECTID Source, LONG Item, CSTRING Datatypes, OBJECTID Surface);
   ERROR (*_SubscribeInput)(FUNCTION * Callback, OBJECTID SurfaceFilter, JTYPE Mask, OBJECTID DeviceFilter, LONG * Handle);
   void (*_Sync)(objBitmap * Bitmap);
   ERROR (*_UnlockBitmap)(OBJECTID Surface, objBitmap * Bitmap);
   ERROR (*_UnlockCursor)(OBJECTID Surface);
   ERROR (*_UnsubscribeInput)(LONG Handle);
   ERROR (*_WindowHook)(OBJECTID SurfaceID, WH Event, FUNCTION * Callback);
#endif // PARASOL_STATIC
};

#ifndef PRV_DISPLAY_MODULE
#ifndef PARASOL_STATIC
extern struct DisplayBase *DisplayBase;
inline objPointer * gfxAccessPointer(void) { return DisplayBase->_AccessPointer(); }
inline ERROR gfxCheckIfChild(OBJECTID Parent, OBJECTID Child) { return DisplayBase->_CheckIfChild(Parent,Child); }
inline ERROR gfxCopyArea(objBitmap * Bitmap, objBitmap * Dest, BAF Flags, LONG X, LONG Y, LONG Width, LONG Height, LONG XDest, LONG YDest) { return DisplayBase->_CopyArea(Bitmap,Dest,Flags,X,Y,Width,Height,XDest,YDest); }
inline ERROR gfxCopyRawBitmap(struct BitmapSurfaceV2 * Surface, objBitmap * Bitmap, CSRF Flags, LONG X, LONG Y, LONG Width, LONG Height, LONG XDest, LONG YDest) { return DisplayBase->_CopyRawBitmap(Surface,Bitmap,Flags,X,Y,Width,Height,XDest,YDest); }
inline ERROR gfxCopySurface(OBJECTID Surface, objBitmap * Bitmap, BDF Flags, LONG X, LONG Y, LONG Width, LONG Height, LONG XDest, LONG YDest) { return DisplayBase->_CopySurface(Surface,Bitmap,Flags,X,Y,Width,Height,XDest,YDest); }
inline void gfxDrawPixel(objBitmap * Bitmap, LONG X, LONG Y, ULONG Colour) { return DisplayBase->_DrawPixel(Bitmap,X,Y,Colour); }
inline void gfxDrawRGBPixel(objBitmap * Bitmap, LONG X, LONG Y, struct RGB8 * RGB) { return DisplayBase->_DrawRGBPixel(Bitmap,X,Y,RGB); }
inline void gfxDrawRectangle(objBitmap * Bitmap, LONG X, LONG Y, LONG Width, LONG Height, ULONG Colour, BAF Flags) { return DisplayBase->_DrawRectangle(Bitmap,X,Y,Width,Height,Colour,Flags); }
inline ERROR gfxExposeSurface(OBJECTID Surface, LONG X, LONG Y, LONG Width, LONG Height, EXF Flags) { return DisplayBase->_ExposeSurface(Surface,X,Y,Width,Height,Flags); }
inline void gfxFlipBitmap(objBitmap * Bitmap, FLIP Orientation) { return DisplayBase->_FlipBitmap(Bitmap,Orientation); }
inline void gfxGetColourFormat(struct ColourFormat * Format, LONG BitsPerPixel, LONG RedMask, LONG GreenMask, LONG BlueMask, LONG AlphaMask) { return DisplayBase->_GetColourFormat(Format,BitsPerPixel,RedMask,GreenMask,BlueMask,AlphaMask); }
inline ERROR gfxGetCursorInfo(struct CursorInfo * Info, LONG Size) { return DisplayBase->_GetCursorInfo(Info,Size); }
inline ERROR gfxGetCursorPos(DOUBLE * X, DOUBLE * Y) { return DisplayBase->_GetCursorPos(X,Y); }
inline ERROR gfxGetDisplayInfo(OBJECTID Display, struct DisplayInfoV3 ** Info) { return DisplayBase->_GetDisplayInfo(Display,Info); }
inline DT gfxGetDisplayType(void) { return DisplayBase->_GetDisplayType(); }
inline CSTRING gfxGetInputTypeName(JET Type) { return DisplayBase->_GetInputTypeName(Type); }
inline OBJECTID gfxGetModalSurface(void) { return DisplayBase->_GetModalSurface(); }
inline ERROR gfxGetRelativeCursorPos(OBJECTID Surface, DOUBLE * X, DOUBLE * Y) { return DisplayBase->_GetRelativeCursorPos(Surface,X,Y); }
inline ERROR gfxGetSurfaceCoords(OBJECTID Surface, LONG * X, LONG * Y, LONG * AbsX, LONG * AbsY, LONG * Width, LONG * Height) { return DisplayBase->_GetSurfaceCoords(Surface,X,Y,AbsX,AbsY,Width,Height); }
inline ERROR gfxGetSurfaceFlags(OBJECTID Surface, RNF * Flags) { return DisplayBase->_GetSurfaceFlags(Surface,Flags); }
inline ERROR gfxGetSurfaceInfo(OBJECTID Surface, struct SurfaceInfoV2 ** Info) { return DisplayBase->_GetSurfaceInfo(Surface,Info); }
inline OBJECTID gfxGetUserFocus(void) { return DisplayBase->_GetUserFocus(); }
inline ERROR gfxGetVisibleArea(OBJECTID Surface, LONG * X, LONG * Y, LONG * AbsX, LONG * AbsY, LONG * Width, LONG * Height) { return DisplayBase->_GetVisibleArea(Surface,X,Y,AbsX,AbsY,Width,Height); }
inline ERROR gfxLockBitmap(OBJECTID Surface, objBitmap ** Bitmap, LVF * Info) { return DisplayBase->_LockBitmap(Surface,Bitmap,Info); }
inline ERROR gfxLockCursor(OBJECTID Surface) { return DisplayBase->_LockCursor(Surface); }
inline ULONG gfxReadPixel(objBitmap * Bitmap, LONG X, LONG Y) { return DisplayBase->_ReadPixel(Bitmap,X,Y); }
inline void gfxReadRGBPixel(objBitmap * Bitmap, LONG X, LONG Y, struct RGB8 ** RGB) { return DisplayBase->_ReadRGBPixel(Bitmap,X,Y,RGB); }
inline ERROR gfxResample(objBitmap * Bitmap, struct ColourFormat * ColourFormat) { return DisplayBase->_Resample(Bitmap,ColourFormat); }
inline ERROR gfxRestoreCursor(PTC Cursor, OBJECTID Owner) { return DisplayBase->_RestoreCursor(Cursor,Owner); }
inline DOUBLE gfxScaleToDPI(DOUBLE Value) { return DisplayBase->_ScaleToDPI(Value); }
inline ERROR gfxScanDisplayModes(CSTRING Filter, struct DisplayInfoV3 * Info, LONG Size) { return DisplayBase->_ScanDisplayModes(Filter,Info,Size); }
inline void gfxSetClipRegion(objBitmap * Bitmap, LONG Number, LONG Left, LONG Top, LONG Right, LONG Bottom, LONG Terminate) { return DisplayBase->_SetClipRegion(Bitmap,Number,Left,Top,Right,Bottom,Terminate); }
inline ERROR gfxSetCursor(OBJECTID Surface, CRF Flags, PTC Cursor, CSTRING Name, OBJECTID Owner) { return DisplayBase->_SetCursor(Surface,Flags,Cursor,Name,Owner); }
inline ERROR gfxSetCursorPos(DOUBLE X, DOUBLE Y) { return DisplayBase->_SetCursorPos(X,Y); }
inline ERROR gfxSetCustomCursor(OBJECTID Surface, CRF Flags, objBitmap * Bitmap, LONG HotX, LONG HotY, OBJECTID Owner) { return DisplayBase->_SetCustomCursor(Surface,Flags,Bitmap,HotX,HotY,Owner); }
inline ERROR gfxSetHostOption(HOST Option, LARGE Value) { return DisplayBase->_SetHostOption(Option,Value); }
inline OBJECTID gfxSetModalSurface(OBJECTID Surface) { return DisplayBase->_SetModalSurface(Surface); }
inline ERROR gfxStartCursorDrag(OBJECTID Source, LONG Item, CSTRING Datatypes, OBJECTID Surface) { return DisplayBase->_StartCursorDrag(Source,Item,Datatypes,Surface); }
inline ERROR gfxSubscribeInput(FUNCTION * Callback, OBJECTID SurfaceFilter, JTYPE Mask, OBJECTID DeviceFilter, LONG * Handle) { return DisplayBase->_SubscribeInput(Callback,SurfaceFilter,Mask,DeviceFilter,Handle); }
inline void gfxSync(objBitmap * Bitmap) { return DisplayBase->_Sync(Bitmap); }
inline ERROR gfxUnlockBitmap(OBJECTID Surface, objBitmap * Bitmap) { return DisplayBase->_UnlockBitmap(Surface,Bitmap); }
inline ERROR gfxUnlockCursor(OBJECTID Surface) { return DisplayBase->_UnlockCursor(Surface); }
inline ERROR gfxUnsubscribeInput(LONG Handle) { return DisplayBase->_UnsubscribeInput(Handle); }
inline ERROR gfxWindowHook(OBJECTID SurfaceID, WH Event, FUNCTION * Callback) { return DisplayBase->_WindowHook(SurfaceID,Event,Callback); }
#else
extern "C" {
extern objPointer * gfxAccessPointer(void);
extern ERROR gfxCheckIfChild(OBJECTID Parent, OBJECTID Child);
extern ERROR gfxCopyArea(objBitmap * Bitmap, objBitmap * Dest, BAF Flags, LONG X, LONG Y, LONG Width, LONG Height, LONG XDest, LONG YDest);
extern ERROR gfxCopyRawBitmap(struct BitmapSurfaceV2 * Surface, objBitmap * Bitmap, CSRF Flags, LONG X, LONG Y, LONG Width, LONG Height, LONG XDest, LONG YDest);
extern ERROR gfxCopySurface(OBJECTID Surface, objBitmap * Bitmap, BDF Flags, LONG X, LONG Y, LONG Width, LONG Height, LONG XDest, LONG YDest);
extern void gfxDrawPixel(objBitmap * Bitmap, LONG X, LONG Y, ULONG Colour);
extern void gfxDrawRGBPixel(objBitmap * Bitmap, LONG X, LONG Y, struct RGB8 * RGB);
extern void gfxDrawRectangle(objBitmap * Bitmap, LONG X, LONG Y, LONG Width, LONG Height, ULONG Colour, BAF Flags);
extern ERROR gfxExposeSurface(OBJECTID Surface, LONG X, LONG Y, LONG Width, LONG Height, EXF Flags);
extern void gfxFlipBitmap(objBitmap * Bitmap, FLIP Orientation);
extern void gfxGetColourFormat(struct ColourFormat * Format, LONG BitsPerPixel, LONG RedMask, LONG GreenMask, LONG BlueMask, LONG AlphaMask);
extern ERROR gfxGetCursorInfo(struct CursorInfo * Info, LONG Size);
extern ERROR gfxGetCursorPos(DOUBLE * X, DOUBLE * Y);
extern ERROR gfxGetDisplayInfo(OBJECTID Display, struct DisplayInfoV3 ** Info);
extern DT gfxGetDisplayType(void);
extern CSTRING gfxGetInputTypeName(JET Type);
extern OBJECTID gfxGetModalSurface(void);
extern ERROR gfxGetRelativeCursorPos(OBJECTID Surface, DOUBLE * X, DOUBLE * Y);
extern ERROR gfxGetSurfaceCoords(OBJECTID Surface, LONG * X, LONG * Y, LONG * AbsX, LONG * AbsY, LONG * Width, LONG * Height);
extern ERROR gfxGetSurfaceFlags(OBJECTID Surface, RNF * Flags);
extern ERROR gfxGetSurfaceInfo(OBJECTID Surface, struct SurfaceInfoV2 ** Info);
extern OBJECTID gfxGetUserFocus(void);
extern ERROR gfxGetVisibleArea(OBJECTID Surface, LONG * X, LONG * Y, LONG * AbsX, LONG * AbsY, LONG * Width, LONG * Height);
extern ERROR gfxLockBitmap(OBJECTID Surface, objBitmap ** Bitmap, LVF * Info);
extern ERROR gfxLockCursor(OBJECTID Surface);
extern ULONG gfxReadPixel(objBitmap * Bitmap, LONG X, LONG Y);
extern void gfxReadRGBPixel(objBitmap * Bitmap, LONG X, LONG Y, struct RGB8 ** RGB);
extern ERROR gfxResample(objBitmap * Bitmap, struct ColourFormat * ColourFormat);
extern ERROR gfxRestoreCursor(PTC Cursor, OBJECTID Owner);
extern DOUBLE gfxScaleToDPI(DOUBLE Value);
extern ERROR gfxScanDisplayModes(CSTRING Filter, struct DisplayInfoV3 * Info, LONG Size);
extern void gfxSetClipRegion(objBitmap * Bitmap, LONG Number, LONG Left, LONG Top, LONG Right, LONG Bottom, LONG Terminate);
extern ERROR gfxSetCursor(OBJECTID Surface, CRF Flags, PTC Cursor, CSTRING Name, OBJECTID Owner);
extern ERROR gfxSetCursorPos(DOUBLE X, DOUBLE Y);
extern ERROR gfxSetCustomCursor(OBJECTID Surface, CRF Flags, objBitmap * Bitmap, LONG HotX, LONG HotY, OBJECTID Owner);
extern ERROR gfxSetHostOption(HOST Option, LARGE Value);
extern OBJECTID gfxSetModalSurface(OBJECTID Surface);
extern ERROR gfxStartCursorDrag(OBJECTID Source, LONG Item, CSTRING Datatypes, OBJECTID Surface);
extern ERROR gfxSubscribeInput(FUNCTION * Callback, OBJECTID SurfaceFilter, JTYPE Mask, OBJECTID DeviceFilter, LONG * Handle);
extern void gfxSync(objBitmap * Bitmap);
extern ERROR gfxUnlockBitmap(OBJECTID Surface, objBitmap * Bitmap);
extern ERROR gfxUnlockCursor(OBJECTID Surface);
extern ERROR gfxUnsubscribeInput(LONG Handle);
extern ERROR gfxWindowHook(OBJECTID SurfaceID, WH Event, FUNCTION * Callback);
}
#endif // PARASOL_STATIC
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

// Stubs

INLINE ERROR drwInvalidateRegionID(OBJECTID ObjectID, LONG X, LONG Y, LONG Width, LONG Height) {
   struct drwInvalidateRegion args = { X, Y, Width, Height };
   return ActionMsg(MT_DrwInvalidateRegion, ObjectID, &args);
}

INLINE ERROR drwExposeID(OBJECTID ObjectID, LONG X, LONG Y, LONG Width, LONG Height, EXF Flags) {
   struct drwExpose args = { X, Y, Width, Height, Flags };
   return ActionMsg(MT_DrwExpose, ObjectID, &args);
}

INLINE ERROR drwSetOpacityID(OBJECTID ObjectID, DOUBLE Value, DOUBLE Adjustment) {
   struct drwSetOpacity args = { Value, Adjustment};
   return ActionMsg(MT_DrwSetOpacity, ObjectID, &args);
}

INLINE ERROR drwAddCallback(OBJECTPTR Surface, APTR Callback) {
   if (Callback) {
      auto call = make_function_stdc(Callback);
      struct drwAddCallback args = { &call };
      return Action(MT_DrwAddCallback, Surface, &args);
   }
   else {
      struct drwAddCallback args = { NULL };
      return Action(MT_DrwAddCallback, Surface, &args);
   }
}

INLINE ERROR drwRemoveCallback(OBJECTPTR Surface, APTR Callback) {
   if (Callback) {
      auto call = make_function_stdc(Callback);
      struct drwRemoveCallback args = { &call };
      return Action(MT_DrwRemoveCallback, Surface, &args);
   }
   else {
      struct drwRemoveCallback args = { NULL };
      return Action(MT_DrwRemoveCallback, Surface, &args);
   }
}
