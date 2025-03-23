#pragma once

// Name:      display.h
// Copyright: Paul Manias 2003-2025
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
class objController;
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
   POINTER = 0x00004000,
   CURSOR = 0x00004000,
   AFTER_COPY = 0x00008000,
   READ_ONLY = 0x0000c040,
   VOLATILE = 0x0000c400,
   FIXED_BUFFER = 0x00010000,
   PERVASIVE_COPY = 0x00020000,
   NO_FOCUS = 0x00040000,
   FIXED_DEPTH = 0x00080000,
   TOTAL_REDRAW = 0x00100000,
   POST_COMPOSITE = 0x00200000,
   NO_PRECOMPOSITE = 0x00200000,
   COMPOSITE = 0x00200000,
   FULL_SCREEN = 0x00400000,
   IGNORE_FOCUS = 0x00800000,
   INIT_ONLY = 0x00cb0e81,
   ASPECT_RATIO = 0x01000000,
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
   GRAB_CONTROLLERS = 0x00000080,
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

struct xrMode {
   LONG Width;    // Horizontal
   LONG Height;   // Vertical
   LONG Depth;    // bit depth
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

namespace bmp {
struct CopyArea { objBitmap * DestBitmap; BAF Flags; LONG X; LONG Y; LONG Width; LONG Height; LONG XDest; LONG YDest; static const AC id = AC(-1); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };
struct Compress { LONG Level; static const AC id = AC(-2); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };
struct Decompress { LONG RetainData; static const AC id = AC(-3); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };
struct DrawRectangle { LONG X; LONG Y; LONG Width; LONG Height; ULONG Colour; BAF Flags; static const AC id = AC(-4); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };
struct SetClipRegion { LONG Number; LONG Left; LONG Top; LONG Right; LONG Bottom; LONG Terminate; static const AC id = AC(-5); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };
struct GetColour { LONG Red; LONG Green; LONG Blue; LONG Alpha; ULONG Colour; static const AC id = AC(-6); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };
struct Premultiply { static const AC id = AC(-7); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };
struct Demultiply { static const AC id = AC(-8); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };
struct ConvertToLinear { static const AC id = AC(-9); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };
struct ConvertToRGB { static const AC id = AC(-10); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };

} // namespace

class objBitmap : public Object {
   public:
   static constexpr CLASSID CLASS_ID = CLASSID::BITMAP;
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
   LONG    LineWidth;                                                // The length of each bitmap line in bytes, including alignment.
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
   LONG    Opacity;                                                  // Determines the translucency setting to use in drawing operations.
   struct RGB8 TransColour;                                          // The transparent colour of the bitmap, in RGB format.
   struct RGB8 Bkgd;                                                 // The bitmap's background colour is defined here in RGB format.
   LONG    BkgdIndex;                                                // The bitmap's background colour is defined here as a colour index.
   CS      ColourSpace;                                              // Defines the colour space for RGB values.
   public:
   inline ULONG getColour(struct RGB8 &RGB) {
      if (BitsPerPixel > 8) return packPixel(RGB);
      else {
         ULONG result;
         if (getColour(RGB.Red, RGB.Green, RGB.Blue, RGB.Alpha, &result) IS ERR::Okay) {
            return result;
         }
         else return 0;
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

   inline UBYTE * offset(LONG X, LONG Y) {
      auto r_data = Data;
      Data += (X * BytesPerPixel) + (Y * LineWidth);
      return r_data;
   }

   // Colour unpacking routines

   template <class T> inline UBYTE unpackRed(T Packed)   { return (((Packed >> ColourFormat->RedPos) & ColourFormat->RedMask) << ColourFormat->RedShift); }
   template <class T> inline UBYTE unpackGreen(T Packed) { return (((Packed >> ColourFormat->GreenPos) & ColourFormat->GreenMask) << ColourFormat->GreenShift); }
   template <class T> inline UBYTE unpackBlue(T Packed)  { return (((Packed >> ColourFormat->BluePos) & ColourFormat->BlueMask) << ColourFormat->BlueShift); }
   template <class T> inline UBYTE unpackAlpha(T Packed) { return (((Packed >> ColourFormat->AlphaPos) & ColourFormat->AlphaMask)); }

   // Action stubs

   inline ERR clear() noexcept { return Action(AC::Clear, this, NULL); }
   inline ERR copyData(OBJECTPTR Dest) noexcept {
      struct acCopyData args = { .Dest = Dest };
      return Action(AC::CopyData, this, &args);
   }
   inline ERR draw() noexcept { return Action(AC::Draw, this, NULL); }
   inline ERR drawArea(LONG X, LONG Y, LONG Width, LONG Height) noexcept {
      struct acDraw args = { X, Y, Width, Height };
      return Action(AC::Draw, this, &args);
   }
   inline ERR flush() noexcept { return Action(AC::Flush, this, NULL); }
   inline ERR init() noexcept { return InitObject(this); }
   inline ERR lock() noexcept { return Action(AC::Lock, this, NULL); }
   inline ERR query() noexcept { return Action(AC::Query, this, NULL); }
   template <class T, class U> ERR read(APTR Buffer, T Size, U *Result) noexcept {
      static_assert(std::is_integral<U>::value, "Result value must be an integer type");
      static_assert(std::is_integral<T>::value, "Size value must be an integer type");
      const LONG bytes = (Size > 0x7fffffff) ? 0x7fffffff : Size;
      struct acRead read = { (BYTE *)Buffer, bytes };
      if (auto error = Action(AC::Read, this, &read); error IS ERR::Okay) {
         *Result = static_cast<U>(read.Result);
         return ERR::Okay;
      }
      else { *Result = 0; return error; }
   }
   template <class T> ERR read(APTR Buffer, T Size) noexcept {
      static_assert(std::is_integral<T>::value, "Size value must be an integer type");
      const LONG bytes = (Size > 0x7fffffff) ? 0x7fffffff : Size;
      struct acRead read = { (BYTE *)Buffer, bytes };
      return Action(AC::Read, this, &read);
   }
   inline ERR resize(DOUBLE Width, DOUBLE Height, DOUBLE Depth = 0) noexcept {
      struct acResize args = { Width, Height, Depth };
      return Action(AC::Resize, this, &args);
   }
   inline ERR saveImage(OBJECTPTR Dest, CLASSID ClassID = CLASSID::NIL) noexcept {
      struct acSaveImage args = { Dest, { ClassID } };
      return Action(AC::SaveImage, this, &args);
   }
   inline ERR seek(DOUBLE Offset, SEEK Position = SEEK::CURRENT) noexcept {
      struct acSeek args = { Offset, Position };
      return Action(AC::Seek, this, &args);
   }
   inline ERR seekStart(DOUBLE Offset) noexcept { return seek(Offset, SEEK::START); }
   inline ERR seekEnd(DOUBLE Offset) noexcept { return seek(Offset, SEEK::END); }
   inline ERR seekCurrent(DOUBLE Offset) noexcept { return seek(Offset, SEEK::CURRENT); }
   inline ERR unlock() noexcept { return Action(AC::Unlock, this, NULL); }
   inline ERR write(CPTR Buffer, LONG Size, LONG *Result = NULL) noexcept {
      struct acWrite write = { (BYTE *)Buffer, Size };
      if (auto error = Action(AC::Write, this, &write); error IS ERR::Okay) {
         if (Result) *Result = write.Result;
         return ERR::Okay;
      }
      else {
         if (Result) *Result = 0;
         return error;
      }
   }
   inline ERR write(std::string Buffer, LONG *Result = NULL) noexcept {
      struct acWrite write = { (BYTE *)Buffer.c_str(), LONG(Buffer.size()) };
      if (auto error = Action(AC::Write, this, &write); error IS ERR::Okay) {
         if (Result) *Result = write.Result;
         return ERR::Okay;
      }
      else {
         if (Result) *Result = 0;
         return error;
      }
   }
   inline LONG writeResult(CPTR Buffer, LONG Size) noexcept {
      struct acWrite write = { (BYTE *)Buffer, Size };
      if (Action(AC::Write, this, &write) IS ERR::Okay) return write.Result;
      else return 0;
   }
   inline ERR copyArea(objBitmap * DestBitmap, BAF Flags, LONG X, LONG Y, LONG Width, LONG Height, LONG XDest, LONG YDest) noexcept {
      struct bmp::CopyArea args = { DestBitmap, Flags, X, Y, Width, Height, XDest, YDest };
      return(Action(AC(-1), this, &args));
   }
   inline ERR compress(LONG Level) noexcept {
      struct bmp::Compress args = { Level };
      return(Action(AC(-2), this, &args));
   }
   inline ERR decompress(LONG RetainData) noexcept {
      struct bmp::Decompress args = { RetainData };
      return(Action(AC(-3), this, &args));
   }
   inline ERR drawRectangle(LONG X, LONG Y, LONG Width, LONG Height, ULONG Colour, BAF Flags) noexcept {
      struct bmp::DrawRectangle args = { X, Y, Width, Height, Colour, Flags };
      return(Action(AC(-4), this, &args));
   }
   inline ERR setClipRegion(LONG Number, LONG Left, LONG Top, LONG Right, LONG Bottom, LONG Terminate) noexcept {
      struct bmp::SetClipRegion args = { Number, Left, Top, Right, Bottom, Terminate };
      return(Action(AC(-5), this, &args));
   }
   inline ERR getColour(LONG Red, LONG Green, LONG Blue, LONG Alpha, ULONG * Colour) noexcept {
      struct bmp::GetColour args = { Red, Green, Blue, Alpha, (ULONG)0 };
      ERR error = Action(AC(-6), this, &args);
      if (Colour) *Colour = args.Colour;
      return(error);
   }
   inline ERR premultiply() noexcept {
      return(Action(AC(-7), this, NULL));
   }
   inline ERR demultiply() noexcept {
      return(Action(AC(-8), this, NULL));
   }
   inline ERR convertToLinear() noexcept {
      return(Action(AC(-9), this, NULL));
   }
   inline ERR convertToRGB() noexcept {
      return(Action(AC(-10), this, NULL));
   }

   // Customised field setting

   inline ERR setPalette(struct RGBPalette * Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[30];
      return field->WriteValue(target, field, 0x08000300, Value, 1);
   }

   inline ERR setData(UBYTE * Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[24];
      return field->WriteValue(target, field, 0x08000500, Value, 1);
   }

   inline ERR setWidth(const LONG Value) noexcept {
      if (this->initialised()) return ERR::NoFieldAccess;
      this->Width = Value;
      return ERR::Okay;
   }

   inline ERR setHeight(const LONG Value) noexcept {
      if (this->initialised()) return ERR::NoFieldAccess;
      this->Height = Value;
      return ERR::Okay;
   }

   inline ERR setType(const BMP Value) noexcept {
      if (this->initialised()) return ERR::NoFieldAccess;
      this->Type = Value;
      return ERR::Okay;
   }

   inline ERR setClip(struct ClipRectangle * Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[23];
      return field->WriteValue(target, field, 0x08000310, Value, 1);
   }

   inline ERR setDataFlags(const MEM Value) noexcept {
      if (this->initialised()) return ERR::NoFieldAccess;
      this->DataFlags = Value;
      return ERR::Okay;
   }

   inline ERR setAmtColours(const LONG Value) noexcept {
      if (this->initialised()) return ERR::NoFieldAccess;
      this->AmtColours = Value;
      return ERR::Okay;
   }

   inline ERR setFlags(const BMF Value) noexcept {
      if (this->initialised()) return ERR::NoFieldAccess;
      this->Flags = Value;
      return ERR::Okay;
   }

   inline ERR setTransIndex(const LONG Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[29];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERR setBytesPerPixel(const LONG Value) noexcept {
      if (this->initialised()) return ERR::NoFieldAccess;
      this->BytesPerPixel = Value;
      return ERR::Okay;
   }

   inline ERR setBitsPerPixel(const LONG Value) noexcept {
      if (this->initialised()) return ERR::NoFieldAccess;
      this->BitsPerPixel = Value;
      return ERR::Okay;
   }

   inline ERR setOpacity(const LONG Value) noexcept {
      this->Opacity = Value;
      return ERR::Okay;
   }

   inline ERR setTransColour(const struct RGB8 * Value, LONG Elements) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[16];
      return field->WriteValue(target, field, 0x01081300, Value, Elements);
   }

   inline ERR setBkgd(const struct RGB8 * Value, LONG Elements) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[22];
      return field->WriteValue(target, field, 0x01081300, Value, Elements);
   }

   inline ERR setBkgdIndex(const LONG Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[10];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERR setColourSpace(const CS Value) noexcept {
      this->ColourSpace = Value;
      return ERR::Okay;
   }

   inline ERR setClipLeft(const LONG Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[15];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERR setClipRight(const LONG Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[13];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERR setClipBottom(const LONG Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[19];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERR setClipTop(const LONG Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[36];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERR setHandle(APTR Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[0];
      return field->WriteValue(target, field, 0x08010300, Value, 1);
   }

};

// Display class definition

#define VER_DISPLAY (1.000000)

// Display methods

namespace gfx {
struct WaitVBL { static const AC id = AC(-1); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };
struct UpdatePalette { struct RGBPalette * NewPalette; static const AC id = AC(-2); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };
struct SetDisplay { LONG X; LONG Y; LONG Width; LONG Height; LONG InsideWidth; LONG InsideHeight; LONG BitsPerPixel; DOUBLE RefreshRate; LONG Flags; static const AC id = AC(-3); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };
struct SizeHints { LONG MinWidth; LONG MinHeight; LONG MaxWidth; LONG MaxHeight; LONG EnforceAspect; static const AC id = AC(-4); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };
struct SetGamma { DOUBLE Red; DOUBLE Green; DOUBLE Blue; GMF Flags; static const AC id = AC(-5); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };
struct SetGammaLinear { DOUBLE Red; DOUBLE Green; DOUBLE Blue; GMF Flags; static const AC id = AC(-6); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };
struct SetMonitor { CSTRING Name; LONG MinH; LONG MaxH; LONG MinV; LONG MaxV; MON Flags; static const AC id = AC(-7); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };
struct Minimise { static const AC id = AC(-8); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };
struct CheckXWindow { static const AC id = AC(-9); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };

} // namespace

class objDisplay : public Object {
   public:
   static constexpr CLASSID CLASS_ID = CLASSID::DISPLAY;
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

   inline ERR activate() noexcept { return Action(AC::Activate, this, NULL); }
   inline ERR clear() noexcept { return Action(AC::Clear, this, NULL); }
   inline ERR dataFeed(OBJECTPTR Object, DATA Datatype, const void *Buffer, LONG Size) noexcept {
      struct acDataFeed args = { Object, Datatype, Buffer, Size };
      return Action(AC::DataFeed, this, &args);
   }
   inline ERR disable() noexcept { return Action(AC::Disable, this, NULL); }
   inline ERR draw() noexcept { return Action(AC::Draw, this, NULL); }
   inline ERR drawArea(LONG X, LONG Y, LONG Width, LONG Height) noexcept {
      struct acDraw args = { X, Y, Width, Height };
      return Action(AC::Draw, this, &args);
   }
   inline ERR enable() noexcept { return Action(AC::Enable, this, NULL); }
   inline ERR flush() noexcept { return Action(AC::Flush, this, NULL); }
   inline ERR focus() noexcept { return Action(AC::Focus, this, NULL); }
   inline ERR getKey(CSTRING Key, STRING Value, LONG Size) noexcept {
      struct acGetKey args = { Key, Value, Size };
      auto error = Action(AC::GetKey, this, &args);
      if ((error != ERR::Okay) and (Value)) Value[0] = 0;
      return error;
   }
   inline ERR hide() noexcept { return Action(AC::Hide, this, NULL); }
   inline ERR init() noexcept { return InitObject(this); }
   inline ERR move(DOUBLE X, DOUBLE Y, DOUBLE Z) noexcept {
      struct acMove args = { X, Y, Z };
      return Action(AC::Move, this, &args);
   }
   inline ERR moveToBack() noexcept { return Action(AC::MoveToBack, this, NULL); }
   inline ERR moveToFront() noexcept { return Action(AC::MoveToFront, this, NULL); }
   inline ERR moveToPoint(DOUBLE X, DOUBLE Y, DOUBLE Z, MTF Flags) noexcept {
      struct acMoveToPoint moveto = { X, Y, Z, Flags };
      return Action(AC::MoveToPoint, this, &moveto);
   }
   inline ERR redimension(DOUBLE X, DOUBLE Y, DOUBLE Z, DOUBLE Width, DOUBLE Height, DOUBLE Depth) noexcept {
      struct acRedimension args = { X, Y, Z, Width, Height, Depth };
      return Action(AC::Redimension, this, &args);
   }
   inline ERR redimension(DOUBLE X, DOUBLE Y, DOUBLE Width, DOUBLE Height) noexcept {
      struct acRedimension args = { X, Y, 0, Width, Height, 0 };
      return Action(AC::Redimension, this, &args);
   }
   inline ERR resize(DOUBLE Width, DOUBLE Height, DOUBLE Depth = 0) noexcept {
      struct acResize args = { Width, Height, Depth };
      return Action(AC::Resize, this, &args);
   }
   inline ERR saveImage(OBJECTPTR Dest, CLASSID ClassID = CLASSID::NIL) noexcept {
      struct acSaveImage args = { Dest, { ClassID } };
      return Action(AC::SaveImage, this, &args);
   }
   inline ERR saveSettings() noexcept { return Action(AC::SaveSettings, this, NULL); }
   inline ERR show() noexcept { return Action(AC::Show, this, NULL); }
   inline ERR waitVBL() noexcept {
      return(Action(AC(-1), this, NULL));
   }
   inline ERR updatePalette(struct RGBPalette * NewPalette) noexcept {
      struct gfx::UpdatePalette args = { NewPalette };
      return(Action(AC(-2), this, &args));
   }
   inline ERR setDisplay(LONG X, LONG Y, LONG Width, LONG Height, LONG InsideWidth, LONG InsideHeight, LONG BitsPerPixel, DOUBLE RefreshRate, LONG Flags) noexcept {
      struct gfx::SetDisplay args = { X, Y, Width, Height, InsideWidth, InsideHeight, BitsPerPixel, RefreshRate, Flags };
      return(Action(AC(-3), this, &args));
   }
   inline ERR sizeHints(LONG MinWidth, LONG MinHeight, LONG MaxWidth, LONG MaxHeight, LONG EnforceAspect) noexcept {
      struct gfx::SizeHints args = { MinWidth, MinHeight, MaxWidth, MaxHeight, EnforceAspect };
      return(Action(AC(-4), this, &args));
   }
   inline ERR setGamma(DOUBLE Red, DOUBLE Green, DOUBLE Blue, GMF Flags) noexcept {
      struct gfx::SetGamma args = { Red, Green, Blue, Flags };
      return(Action(AC(-5), this, &args));
   }
   inline ERR setGammaLinear(DOUBLE Red, DOUBLE Green, DOUBLE Blue, GMF Flags) noexcept {
      struct gfx::SetGammaLinear args = { Red, Green, Blue, Flags };
      return(Action(AC(-6), this, &args));
   }
   inline ERR setMonitor(CSTRING Name, LONG MinH, LONG MaxH, LONG MinV, LONG MaxV, MON Flags) noexcept {
      struct gfx::SetMonitor args = { Name, MinH, MaxH, MinV, MaxV, Flags };
      return(Action(AC(-7), this, &args));
   }
   inline ERR minimise() noexcept {
      return(Action(AC(-8), this, NULL));
   }
   inline ERR checkXWindow() noexcept {
      return(Action(AC(-9), this, NULL));
   }

   // Customised field setting

   inline ERR setRefreshRate(const DOUBLE Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[39];
      return field->WriteValue(target, field, FD_DOUBLE, &Value, 1);
   }

   inline ERR setFlags(const SCR Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[4];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERR setWidth(const LONG Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[8];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERR setHeight(const LONG Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[2];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERR setX(const LONG Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[0];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERR setY(const LONG Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[1];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERR setBmpX(const LONG Value) noexcept {
      this->BmpX = Value;
      return ERR::Okay;
   }

   inline ERR setBmpY(const LONG Value) noexcept {
      this->BmpY = Value;
      return ERR::Okay;
   }

   inline ERR setPowerMode(const DPMS Value) noexcept {
      this->PowerMode = Value;
      return ERR::Okay;
   }

   inline ERR setPopOver(OBJECTID Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[25];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERR setGamma(const DOUBLE * Value, LONG Elements) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[5];
      return field->WriteValue(target, field, 0x80001508, Value, Elements);
   }

   inline ERR setHDensity(const LONG Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[16];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERR setVDensity(const LONG Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[14];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERR setOpacity(const DOUBLE Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[15];
      return field->WriteValue(target, field, FD_DOUBLE, &Value, 1);
   }

   inline ERR setResizeFeedback(const FUNCTION Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[29];
      return field->WriteValue(target, field, FD_FUNCTION, &Value, 1);
   }

   inline ERR setWindowHandle(APTR Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[11];
      return field->WriteValue(target, field, 0x08000308, Value, 1);
   }

   template <class T> inline ERR setTitle(T && Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[7];
      return field->WriteValue(target, field, 0x08800308, to_cstring(Value), 1);
   }

};

// Clipboard class definition

#define VER_CLIPBOARD (1.000000)

// Clipboard methods

namespace clip {
struct AddFile { CLIPTYPE Datatype; CSTRING Path; CEF Flags; static const AC id = AC(-1); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };
struct AddObjects { CLIPTYPE Datatype; OBJECTID * Objects; CEF Flags; static const AC id = AC(-2); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };
struct GetFiles { CLIPTYPE Filter; LONG Index; CLIPTYPE Datatype; CSTRING * Files; CEF Flags; static const AC id = AC(-3); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };
struct AddText { CSTRING String; static const AC id = AC(-4); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };
struct Remove { CLIPTYPE Datatype; static const AC id = AC(-5); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };

} // namespace

class objClipboard : public Object {
   public:
   static constexpr CLASSID CLASS_ID = CLASSID::CLIPBOARD;
   static constexpr CSTRING CLASS_NAME = "Clipboard";

   using create = pf::Create<objClipboard>;

   CPF Flags;    // Optional flags.

#ifdef PRV_CLIPBOARD
   FUNCTION RequestHandler;
#endif

   // Action stubs

   inline ERR clear() noexcept { return Action(AC::Clear, this, NULL); }
   inline ERR dataFeed(OBJECTPTR Object, DATA Datatype, const void *Buffer, LONG Size) noexcept {
      struct acDataFeed args = { Object, Datatype, Buffer, Size };
      return Action(AC::DataFeed, this, &args);
   }
   inline ERR init() noexcept { return InitObject(this); }
   inline ERR addFile(CLIPTYPE Datatype, CSTRING Path, CEF Flags) noexcept {
      struct clip::AddFile args = { Datatype, Path, Flags };
      return(Action(AC(-1), this, &args));
   }
   inline ERR addObjects(CLIPTYPE Datatype, OBJECTID * Objects, CEF Flags) noexcept {
      struct clip::AddObjects args = { Datatype, Objects, Flags };
      return(Action(AC(-2), this, &args));
   }
   inline ERR getFiles(CLIPTYPE Filter, LONG Index, CLIPTYPE * Datatype, CSTRING ** Files, CEF * Flags) noexcept {
      struct clip::GetFiles args = { Filter, Index, (CLIPTYPE)0, (CSTRING *)0, (CEF)0 };
      ERR error = Action(AC(-3), this, &args);
      if (Datatype) *Datatype = args.Datatype;
      if (Files) *Files = args.Files;
      if (Flags) *Flags = args.Flags;
      return(error);
   }
   inline ERR addText(CSTRING String) noexcept {
      struct clip::AddText args = { String };
      return(Action(AC(-4), this, &args));
   }
   inline ERR remove(CLIPTYPE Datatype) noexcept {
      struct clip::Remove args = { Datatype };
      return(Action(AC(-5), this, &args));
   }

   // Customised field setting

   inline ERR setFlags(const CPF Value) noexcept {
      if (this->initialised()) return ERR::NoFieldAccess;
      this->Flags = Value;
      return ERR::Okay;
   }

   inline ERR setRequestHandler(FUNCTION Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[3];
      return field->WriteValue(target, field, FD_FUNCTION, &Value, 1);
   }

};

// Controller class definition

#define VER_CONTROLLER (1.000000)

class objController : public Object {
   public:
   static constexpr CLASSID CLASS_ID = CLASSID::CONTROLLER;
   static constexpr CSTRING CLASS_NAME = "Controller";

   using create = pf::Create<objController>;

   DOUBLE LeftTrigger;    // Left trigger value between 0.0 and 1.0.
   DOUBLE RightTrigger;   // Right trigger value between 0.0 and 1.0.
   DOUBLE LeftStickX;     // Left analog stick value for X axis, between -1.0 and 1.0.
   DOUBLE LeftStickY;     // Left analog stick value for Y axis, between -1.0 and 1.0.
   DOUBLE RightStickX;    // Right analog stick value for X axis, between -1.0 and 1.0.
   DOUBLE RightStickY;    // Right analog stick value for Y axis, between -1.0 and 1.0.
   CON    Buttons;        // JET button values expressed as bit-fields.
   LONG   Port;           // The port number assigned to the controller.

   // Action stubs

   inline ERR query() noexcept { return Action(AC::Query, this, NULL); }
   inline ERR init() noexcept { return InitObject(this); }

   // Customised field setting

   inline ERR setPort(const LONG Value) noexcept {
      if (this->initialised()) return ERR::NoFieldAccess;
      this->Port = Value;
      return ERR::Okay;
   }

};

// Pointer class definition

#define VER_POINTER (1.000000)

class objPointer : public Object {
   public:
   static constexpr CLASSID CLASS_ID = CLASSID::POINTER;
   static constexpr CSTRING CLASS_NAME = "Pointer";

   using create = pf::Create<objPointer>;

   DOUBLE   Speed;         // Speed multiplier for pointer movement.
   DOUBLE   Acceleration;  // The rate of acceleration for relative pointer movement.
   DOUBLE   DoubleClick;   // The maximum interval between two clicks for a double click to be recognised.
   DOUBLE   WheelSpeed;    // Defines a multiplier to be applied to the mouse wheel.
   DOUBLE   X;             // The horizontal position of the pointer within its parent display.
   DOUBLE   Y;             // The vertical position of the pointer within its parent display.
   DOUBLE   OverX;         // The horizontal position of the pointer with respect to the object underneath the hot-spot.
   DOUBLE   OverY;         // The vertical position of the pointer with respect to the object underneath the hot-spot.
   DOUBLE   OverZ;         // The position of the Pointer within an object.
   LONG     MaxSpeed;      // Restricts the maximum speed of a pointer's movement.
   OBJECTID InputID;       // Declares the I/O object to read movement from.
   OBJECTID SurfaceID;     // The top-most surface that is under the pointer's hot spot.
   OBJECTID AnchorID;      // Can refer to a surface that the pointer has been anchored to.
   PTC      CursorID;      // Sets the user's cursor image, selected from the pre-defined graphics bank.
   OBJECTID CursorOwnerID; // The current owner of the cursor, as defined by ~Display.SetCursor().
   PF       Flags;         // Optional flags.
   OBJECTID RestrictID;    // Refers to a surface when the pointer is restricted.
   LONG     HostX;         // Indicates the current position of the host cursor on Windows or X11
   LONG     HostY;         // Indicates the current position of the host cursor on Windows or X11
   objBitmap * Bitmap;     // Refers to bitmap in which custom cursor images can be drawn.
   OBJECTID DragSourceID;  // The object managing the current drag operation, as defined by ~Display.StartCursorDrag().
   LONG     DragItem;      // The currently dragged item, as defined by ~Display.StartCursorDrag().
   OBJECTID OverObjectID;  // Readable field that gives the ID of the object under the pointer.
   LONG     ClickSlop;     // A leniency value that assists in determining if the user intended to click or drag.

   // Customised field setting

   inline ERR setSpeed(const DOUBLE Value) noexcept {
      this->Speed = Value;
      return ERR::Okay;
   }

   inline ERR setAcceleration(const DOUBLE Value) noexcept {
      this->Acceleration = Value;
      return ERR::Okay;
   }

   inline ERR setDoubleClick(const DOUBLE Value) noexcept {
      this->DoubleClick = Value;
      return ERR::Okay;
   }

   inline ERR setWheelSpeed(const DOUBLE Value) noexcept {
      this->WheelSpeed = Value;
      return ERR::Okay;
   }

   inline ERR setX(const DOUBLE Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[0];
      return field->WriteValue(target, field, FD_DOUBLE, &Value, 1);
   }

   inline ERR setY(const DOUBLE Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[1];
      return field->WriteValue(target, field, FD_DOUBLE, &Value, 1);
   }

   inline ERR setMaxSpeed(const LONG Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[21];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERR setInput(OBJECTID Value) noexcept {
      this->InputID = Value;
      return ERR::Okay;
   }

   inline ERR setSurface(OBJECTID Value) noexcept {
      this->SurfaceID = Value;
      return ERR::Okay;
   }

   inline ERR setCursor(const PTC Value) noexcept {
      if (this->initialised()) return ERR::NoFieldAccess;
      this->CursorID = Value;
      return ERR::Okay;
   }

   inline ERR setCursorOwner(OBJECTID Value) noexcept {
      this->CursorOwnerID = Value;
      return ERR::Okay;
   }

   inline ERR setFlags(const PF Value) noexcept {
      if (this->initialised()) return ERR::NoFieldAccess;
      this->Flags = Value;
      return ERR::Okay;
   }

   inline ERR setClickSlop(const LONG Value) noexcept {
      this->ClickSlop = Value;
      return ERR::Okay;
   }

   template <class T> inline ERR setButtonOrder(T && Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[13];
      return field->WriteValue(target, field, 0x08800300, to_cstring(Value), 1);
   }

};

// Surface class definition

#define VER_SURFACE (1.000000)

// Surface methods

namespace drw {
struct InheritedFocus { OBJECTID FocusID; RNF Flags; static const AC id = AC(-1); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };
struct ExposeToDisplay { LONG X; LONG Y; LONG Width; LONG Height; EXF Flags; static const AC id = AC(-2); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };
struct InvalidateRegion { LONG X; LONG Y; LONG Width; LONG Height; static const AC id = AC(-3); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };
struct SetDisplay { LONG X; LONG Y; LONG Width; LONG Height; LONG InsideWidth; LONG InsideHeight; LONG BitsPerPixel; DOUBLE RefreshRate; LONG Flags; static const AC id = AC(-4); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };
struct SetOpacity { DOUBLE Value; DOUBLE Adjustment; static const AC id = AC(-5); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };
struct AddCallback { FUNCTION * Callback; static const AC id = AC(-6); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };
struct Minimise { static const AC id = AC(-7); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };
struct ResetDimensions { DOUBLE X; DOUBLE Y; DOUBLE XOffset; DOUBLE YOffset; DOUBLE Width; DOUBLE Height; DMF Dimensions; static const AC id = AC(-8); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };
struct RemoveCallback { FUNCTION * Callback; static const AC id = AC(-9); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };
struct ScheduleRedraw { static const AC id = AC(-10); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };

} // namespace

class objSurface : public Object {
   public:
   static constexpr CLASSID CLASS_ID = CLASSID::SURFACE;
   static constexpr CSTRING CLASS_NAME = "Surface";

   using create = pf::Create<objSurface>;

   OBJECTID DragID;     // This object-based field is used to control the dragging of objects around the display.
   OBJECTID BufferID;   // The ID of the bitmap that manages the surface's graphics.
   OBJECTID ParentID;   // The parent for a surface is defined here.
   OBJECTID PopOverID;  // Keeps a surface in front of another surface in the Z order.
   LONG     MinWidth;   // Prevents the width of a surface object from shrinking beyond a certain value.
   LONG     MinHeight;  // Prevents the height of a surface object from shrinking beyond a certain value.
   LONG     MaxWidth;   // Prevents the width of a surface object from exceeding a certain value.
   LONG     MaxHeight;  // Prevents the height of a surface object from exceeding a certain value.
   LONG     LeftLimit;  // Prevents a surface object from moving beyond a given point on the left-hand side.
   LONG     RightLimit; // Prevents a surface object from moving beyond a given point on the right-hand side.
   LONG     TopLimit;   // Prevents a surface object from moving beyond a given point at the top of its container.
   LONG     BottomLimit; // Prevents a surface object from moving beyond a given point at the bottom of its container.
   OBJECTID DisplayID;  // Refers to the Display object that is managing the surface's graphics.
   RNF      Flags;      // Optional flags.
   LONG     X;          // Determines the horizontal position of a surface object.
   LONG     Y;          // Determines the vertical position of a surface object.
   LONG     Width;      // Defines the width of a surface object.
   LONG     Height;     // Defines the height of a surface object.
   OBJECTID RootID;     // Surface that is acting as a root for many surface children (useful when applying translucency)
   ALIGN    Align;      // This field allows you to align a surface area within its owner.
   DMF      Dimensions; // Indicates currently active dimension settings.
   DRAG     DragStatus; // Indicates the draggable state when dragging is enabled.
   PTC      Cursor;     // A default cursor image can be set here for changing the mouse pointer.
   struct RGB8 Colour;  // String-based field for setting the background colour.
   RT       Type;       // Internal surface type flags
   LONG     Modal;      // Sets the surface as modal (prevents user interaction with other surfaces).

#ifdef PRV_SURFACE
   // These coordinate fields are considered private but may be accessed by some internal classes, like Document
   LONG     XOffset, YOffset;     // Fixed horizontal and vertical offset
   DOUBLE   XOffsetPercent;       // Scaled horizontal offset
   DOUBLE   YOffsetPercent;       // Scaled vertical offset
   DOUBLE   WidthPercent, HeightPercent; // Scaled width and height
   DOUBLE   XPercent, YPercent;   // Scaled coordinate
#endif
   public:
   inline bool visible() const { return (Flags & RNF::VISIBLE) != RNF::NIL; }
   inline bool invisible() const { return (Flags & RNF::VISIBLE) IS RNF::NIL; }
   inline bool hasFocus() const { return (Flags & RNF::HAS_FOCUS) != RNF::NIL; }
   inline bool transparent() const { return (Flags & RNF::TRANSPARENT) != RNF::NIL; }
   inline bool disabled() const { return (Flags & RNF::DISABLED) != RNF::NIL; }
   inline bool isCursor() const { return (Flags & RNF::CURSOR) != RNF::NIL; }

   // Action stubs

   inline ERR activate() noexcept { return Action(AC::Activate, this, NULL); }
   inline ERR disable() noexcept { return Action(AC::Disable, this, NULL); }
   inline ERR draw() noexcept { return Action(AC::Draw, this, NULL); }
   inline ERR drawArea(LONG X, LONG Y, LONG Width, LONG Height) noexcept {
      struct acDraw args = { X, Y, Width, Height };
      return Action(AC::Draw, this, &args);
   }
   inline ERR enable() noexcept { return Action(AC::Enable, this, NULL); }
   inline ERR focus() noexcept { return Action(AC::Focus, this, NULL); }
   inline ERR hide() noexcept { return Action(AC::Hide, this, NULL); }
   inline ERR init() noexcept { return InitObject(this); }
   inline ERR lostFocus() noexcept { return Action(AC::LostFocus, this, NULL); }
   inline ERR move(DOUBLE X, DOUBLE Y, DOUBLE Z) noexcept {
      struct acMove args = { X, Y, Z };
      return Action(AC::Move, this, &args);
   }
   inline ERR moveToBack() noexcept { return Action(AC::MoveToBack, this, NULL); }
   inline ERR moveToFront() noexcept { return Action(AC::MoveToFront, this, NULL); }
   inline ERR moveToPoint(DOUBLE X, DOUBLE Y, DOUBLE Z, MTF Flags) noexcept {
      struct acMoveToPoint moveto = { X, Y, Z, Flags };
      return Action(AC::MoveToPoint, this, &moveto);
   }
   inline ERR redimension(DOUBLE X, DOUBLE Y, DOUBLE Z, DOUBLE Width, DOUBLE Height, DOUBLE Depth) noexcept {
      struct acRedimension args = { X, Y, Z, Width, Height, Depth };
      return Action(AC::Redimension, this, &args);
   }
   inline ERR redimension(DOUBLE X, DOUBLE Y, DOUBLE Width, DOUBLE Height) noexcept {
      struct acRedimension args = { X, Y, 0, Width, Height, 0 };
      return Action(AC::Redimension, this, &args);
   }
   inline ERR resize(DOUBLE Width, DOUBLE Height, DOUBLE Depth = 0) noexcept {
      struct acResize args = { Width, Height, Depth };
      return Action(AC::Resize, this, &args);
   }
   inline ERR saveImage(OBJECTPTR Dest, CLASSID ClassID = CLASSID::NIL) noexcept {
      struct acSaveImage args = { Dest, { ClassID } };
      return Action(AC::SaveImage, this, &args);
   }
   inline ERR show() noexcept { return Action(AC::Show, this, NULL); }
   inline ERR inheritedFocus(OBJECTID FocusID, RNF Flags) noexcept {
      struct drw::InheritedFocus args = { FocusID, Flags };
      return(Action(AC(-1), this, &args));
   }
   inline ERR exposeToDisplay(LONG X, LONG Y, LONG Width, LONG Height, EXF Flags) noexcept {
      struct drw::ExposeToDisplay args = { X, Y, Width, Height, Flags };
      return(Action(AC(-2), this, &args));
   }
   inline ERR invalidateRegion(LONG X, LONG Y, LONG Width, LONG Height) noexcept {
      struct drw::InvalidateRegion args = { X, Y, Width, Height };
      return(Action(AC(-3), this, &args));
   }
   inline ERR setDisplay(LONG X, LONG Y, LONG Width, LONG Height, LONG InsideWidth, LONG InsideHeight, LONG BitsPerPixel, DOUBLE RefreshRate, LONG Flags) noexcept {
      struct drw::SetDisplay args = { X, Y, Width, Height, InsideWidth, InsideHeight, BitsPerPixel, RefreshRate, Flags };
      return(Action(AC(-4), this, &args));
   }
   inline ERR setOpacity(DOUBLE Value, DOUBLE Adjustment) noexcept {
      struct drw::SetOpacity args = { Value, Adjustment };
      return(Action(AC(-5), this, &args));
   }
   inline ERR addCallback(FUNCTION Callback) noexcept {
      struct drw::AddCallback args = { &Callback };
      return(Action(AC(-6), this, &args));
   }
   inline ERR minimise() noexcept {
      return(Action(AC(-7), this, NULL));
   }
   inline ERR resetDimensions(DOUBLE X, DOUBLE Y, DOUBLE XOffset, DOUBLE YOffset, DOUBLE Width, DOUBLE Height, DMF Dimensions) noexcept {
      struct drw::ResetDimensions args = { X, Y, XOffset, YOffset, Width, Height, Dimensions };
      return(Action(AC(-8), this, &args));
   }
   inline ERR removeCallback(FUNCTION Callback) noexcept {
      struct drw::RemoveCallback args = { &Callback };
      return(Action(AC(-9), this, &args));
   }
   inline ERR scheduleRedraw() noexcept {
      return(Action(AC(-10), this, NULL));
   }

   // Customised field setting

   inline ERR setDrag(OBJECTID Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[29];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERR setParent(OBJECTID Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[15];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERR setPopOver(OBJECTID Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[38];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERR setMinWidth(const LONG Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[36];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERR setMinHeight(const LONG Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[33];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERR setMaxWidth(const LONG Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[23];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERR setMaxHeight(const LONG Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[16];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERR setLeftLimit(const LONG Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[5];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERR setRightLimit(const LONG Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[19];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERR setTopLimit(const LONG Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[46];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERR setBottomLimit(const LONG Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[44];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERR setFlags(const RNF Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[8];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERR setX(const LONG Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[0];
      Unit var(Value);
      return field->WriteValue(target, field, FD_UNIT, &var, 1);
   }

   inline ERR setY(const LONG Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[1];
      Unit var(Value);
      return field->WriteValue(target, field, FD_UNIT, &var, 1);
   }

   inline ERR setWidth(const LONG Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[12];
      Unit var(Value);
      return field->WriteValue(target, field, FD_UNIT, &var, 1);
   }

   inline ERR setHeight(const LONG Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[2];
      Unit var(Value);
      return field->WriteValue(target, field, FD_UNIT, &var, 1);
   }

   inline ERR setAlign(const ALIGN Value) noexcept {
      this->Align = Value;
      return ERR::Okay;
   }

   inline ERR setDimensions(const DMF Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[35];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERR setCursor(const PTC Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[47];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERR setColour(const struct RGB8 Value) noexcept {
      this->Colour = Value;
      return ERR::Okay;
   }

   inline ERR setType(const RT Value) noexcept {
      if (this->initialised()) return ERR::NoFieldAccess;
      this->Type = Value;
      return ERR::Okay;
   }

   inline ERR setModal(const LONG Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[9];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERR setRootLayer(OBJECTID Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[37];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERR setAbsX(const LONG Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[27];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERR setAbsY(const LONG Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[28];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERR setBitsPerPixel(const LONG Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[39];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERR setMovement(const LONG Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[34];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERR setOpacity(const DOUBLE Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[25];
      return field->WriteValue(target, field, FD_DOUBLE, &Value, 1);
   }

   inline ERR setRevertFocus(OBJECTID Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[18];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERR setVisible(const LONG Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[26];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERR setWindowType(const LONG Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[32];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERR setWindowHandle(APTR Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[21];
      return field->WriteValue(target, field, 0x08000308, Value, 1);
   }

   inline ERR setXOffset(const LONG Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[17];
      Unit var(Value);
      return field->WriteValue(target, field, FD_UNIT, &var, 1);
   }

   inline ERR setYOffset(const LONG Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[24];
      Unit var(Value);
      return field->WriteValue(target, field, FD_UNIT, &var, 1);
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
   ERR (*_CheckIfChild)(OBJECTID Parent, OBJECTID Child);
   ERR (*_CopyArea)(objBitmap *Bitmap, objBitmap *Dest, BAF Flags, LONG X, LONG Y, LONG Width, LONG Height, LONG XDest, LONG YDest);
   ERR (*_CopyRawBitmap)(struct BitmapSurfaceV2 *Surface, objBitmap *Dest, CSRF Flags, LONG X, LONG Y, LONG Width, LONG Height, LONG XDest, LONG YDest);
   ERR (*_CopySurface)(OBJECTID Surface, objBitmap *Bitmap, BDF Flags, LONG X, LONG Y, LONG Width, LONG Height, LONG XDest, LONG YDest);
   void (*_DrawPixel)(objBitmap *Bitmap, LONG X, LONG Y, ULONG Colour);
   void (*_DrawRGBPixel)(objBitmap *Bitmap, LONG X, LONG Y, struct RGB8 *RGB);
   void (*_DrawRectangle)(objBitmap *Bitmap, LONG X, LONG Y, LONG Width, LONG Height, ULONG Colour, BAF Flags);
   ERR (*_ExposeSurface)(OBJECTID Surface, LONG X, LONG Y, LONG Width, LONG Height, EXF Flags);
   void (*_GetColourFormat)(struct ColourFormat *Format, LONG BitsPerPixel, LONG RedMask, LONG GreenMask, LONG BlueMask, LONG AlphaMask);
   ERR (*_GetCursorInfo)(struct CursorInfo *Info, LONG Size);
   ERR (*_GetCursorPos)(DOUBLE *X, DOUBLE *Y);
   ERR (*_GetDisplayInfo)(OBJECTID Display, struct DisplayInfoV3 **Info);
   DT (*_GetDisplayType)(void);
   CSTRING (*_GetInputTypeName)(JET Type);
   OBJECTID (*_GetModalSurface)(void);
   ERR (*_GetRelativeCursorPos)(OBJECTID Surface, DOUBLE *X, DOUBLE *Y);
   ERR (*_GetSurfaceCoords)(OBJECTID Surface, LONG *X, LONG *Y, LONG *AbsX, LONG *AbsY, LONG *Width, LONG *Height);
   ERR (*_GetSurfaceFlags)(OBJECTID Surface, RNF *Flags);
   ERR (*_GetSurfaceInfo)(OBJECTID Surface, struct SurfaceInfoV2 **Info);
   OBJECTID (*_GetUserFocus)(void);
   ERR (*_GetVisibleArea)(OBJECTID Surface, LONG *X, LONG *Y, LONG *AbsX, LONG *AbsY, LONG *Width, LONG *Height);
   ERR (*_LockCursor)(OBJECTID Surface);
   ULONG (*_ReadPixel)(objBitmap *Bitmap, LONG X, LONG Y);
   void (*_ReadRGBPixel)(objBitmap *Bitmap, LONG X, LONG Y, struct RGB8 **RGB);
   ERR (*_Resample)(objBitmap *Bitmap, struct ColourFormat *ColourFormat);
   ERR (*_RestoreCursor)(PTC Cursor, OBJECTID Owner);
   DOUBLE (*_ScaleToDPI)(DOUBLE Value);
   ERR (*_ScanDisplayModes)(CSTRING Filter, struct DisplayInfoV3 *Info, LONG Size);
   void (*_SetClipRegion)(objBitmap *Bitmap, LONG Number, LONG Left, LONG Top, LONG Right, LONG Bottom, LONG Terminate);
   ERR (*_SetCursor)(OBJECTID Surface, CRF Flags, PTC Cursor, CSTRING Name, OBJECTID Owner);
   ERR (*_SetCursorPos)(DOUBLE X, DOUBLE Y);
   ERR (*_SetCustomCursor)(OBJECTID Surface, CRF Flags, objBitmap *Bitmap, LONG HotX, LONG HotY, OBJECTID Owner);
   ERR (*_SetHostOption)(HOST Option, LARGE Value);
   OBJECTID (*_SetModalSurface)(OBJECTID Surface);
   ERR (*_StartCursorDrag)(OBJECTID Source, LONG Item, CSTRING Datatypes, OBJECTID Surface);
   ERR (*_SubscribeInput)(FUNCTION *Callback, OBJECTID SurfaceFilter, JTYPE Mask, OBJECTID DeviceFilter, LONG *Handle);
   void (*_Sync)(objBitmap *Bitmap);
   ERR (*_UnlockCursor)(OBJECTID Surface);
   ERR (*_UnsubscribeInput)(LONG Handle);
   ERR (*_WindowHook)(OBJECTID SurfaceID, WH Event, FUNCTION *Callback);
#endif // PARASOL_STATIC
};

#ifndef PRV_DISPLAY_MODULE
#ifndef PARASOL_STATIC
extern struct DisplayBase *DisplayBase;
namespace gfx {
inline objPointer * AccessPointer(void) { return DisplayBase->_AccessPointer(); }
inline ERR CheckIfChild(OBJECTID Parent, OBJECTID Child) { return DisplayBase->_CheckIfChild(Parent,Child); }
inline ERR CopyArea(objBitmap *Bitmap, objBitmap *Dest, BAF Flags, LONG X, LONG Y, LONG Width, LONG Height, LONG XDest, LONG YDest) { return DisplayBase->_CopyArea(Bitmap,Dest,Flags,X,Y,Width,Height,XDest,YDest); }
inline ERR CopyRawBitmap(struct BitmapSurfaceV2 *Surface, objBitmap *Dest, CSRF Flags, LONG X, LONG Y, LONG Width, LONG Height, LONG XDest, LONG YDest) { return DisplayBase->_CopyRawBitmap(Surface,Dest,Flags,X,Y,Width,Height,XDest,YDest); }
inline ERR CopySurface(OBJECTID Surface, objBitmap *Bitmap, BDF Flags, LONG X, LONG Y, LONG Width, LONG Height, LONG XDest, LONG YDest) { return DisplayBase->_CopySurface(Surface,Bitmap,Flags,X,Y,Width,Height,XDest,YDest); }
inline void DrawPixel(objBitmap *Bitmap, LONG X, LONG Y, ULONG Colour) { return DisplayBase->_DrawPixel(Bitmap,X,Y,Colour); }
inline void DrawRGBPixel(objBitmap *Bitmap, LONG X, LONG Y, struct RGB8 *RGB) { return DisplayBase->_DrawRGBPixel(Bitmap,X,Y,RGB); }
inline void DrawRectangle(objBitmap *Bitmap, LONG X, LONG Y, LONG Width, LONG Height, ULONG Colour, BAF Flags) { return DisplayBase->_DrawRectangle(Bitmap,X,Y,Width,Height,Colour,Flags); }
inline ERR ExposeSurface(OBJECTID Surface, LONG X, LONG Y, LONG Width, LONG Height, EXF Flags) { return DisplayBase->_ExposeSurface(Surface,X,Y,Width,Height,Flags); }
inline void GetColourFormat(struct ColourFormat *Format, LONG BitsPerPixel, LONG RedMask, LONG GreenMask, LONG BlueMask, LONG AlphaMask) { return DisplayBase->_GetColourFormat(Format,BitsPerPixel,RedMask,GreenMask,BlueMask,AlphaMask); }
inline ERR GetCursorInfo(struct CursorInfo *Info, LONG Size) { return DisplayBase->_GetCursorInfo(Info,Size); }
inline ERR GetCursorPos(DOUBLE *X, DOUBLE *Y) { return DisplayBase->_GetCursorPos(X,Y); }
inline ERR GetDisplayInfo(OBJECTID Display, struct DisplayInfoV3 **Info) { return DisplayBase->_GetDisplayInfo(Display,Info); }
inline DT GetDisplayType(void) { return DisplayBase->_GetDisplayType(); }
inline CSTRING GetInputTypeName(JET Type) { return DisplayBase->_GetInputTypeName(Type); }
inline OBJECTID GetModalSurface(void) { return DisplayBase->_GetModalSurface(); }
inline ERR GetRelativeCursorPos(OBJECTID Surface, DOUBLE *X, DOUBLE *Y) { return DisplayBase->_GetRelativeCursorPos(Surface,X,Y); }
inline ERR GetSurfaceCoords(OBJECTID Surface, LONG *X, LONG *Y, LONG *AbsX, LONG *AbsY, LONG *Width, LONG *Height) { return DisplayBase->_GetSurfaceCoords(Surface,X,Y,AbsX,AbsY,Width,Height); }
inline ERR GetSurfaceFlags(OBJECTID Surface, RNF *Flags) { return DisplayBase->_GetSurfaceFlags(Surface,Flags); }
inline ERR GetSurfaceInfo(OBJECTID Surface, struct SurfaceInfoV2 **Info) { return DisplayBase->_GetSurfaceInfo(Surface,Info); }
inline OBJECTID GetUserFocus(void) { return DisplayBase->_GetUserFocus(); }
inline ERR GetVisibleArea(OBJECTID Surface, LONG *X, LONG *Y, LONG *AbsX, LONG *AbsY, LONG *Width, LONG *Height) { return DisplayBase->_GetVisibleArea(Surface,X,Y,AbsX,AbsY,Width,Height); }
inline ERR LockCursor(OBJECTID Surface) { return DisplayBase->_LockCursor(Surface); }
inline ULONG ReadPixel(objBitmap *Bitmap, LONG X, LONG Y) { return DisplayBase->_ReadPixel(Bitmap,X,Y); }
inline void ReadRGBPixel(objBitmap *Bitmap, LONG X, LONG Y, struct RGB8 **RGB) { return DisplayBase->_ReadRGBPixel(Bitmap,X,Y,RGB); }
inline ERR Resample(objBitmap *Bitmap, struct ColourFormat *ColourFormat) { return DisplayBase->_Resample(Bitmap,ColourFormat); }
inline ERR RestoreCursor(PTC Cursor, OBJECTID Owner) { return DisplayBase->_RestoreCursor(Cursor,Owner); }
inline DOUBLE ScaleToDPI(DOUBLE Value) { return DisplayBase->_ScaleToDPI(Value); }
inline ERR ScanDisplayModes(CSTRING Filter, struct DisplayInfoV3 *Info, LONG Size) { return DisplayBase->_ScanDisplayModes(Filter,Info,Size); }
inline void SetClipRegion(objBitmap *Bitmap, LONG Number, LONG Left, LONG Top, LONG Right, LONG Bottom, LONG Terminate) { return DisplayBase->_SetClipRegion(Bitmap,Number,Left,Top,Right,Bottom,Terminate); }
inline ERR SetCursor(OBJECTID Surface, CRF Flags, PTC Cursor, CSTRING Name, OBJECTID Owner) { return DisplayBase->_SetCursor(Surface,Flags,Cursor,Name,Owner); }
inline ERR SetCursorPos(DOUBLE X, DOUBLE Y) { return DisplayBase->_SetCursorPos(X,Y); }
inline ERR SetCustomCursor(OBJECTID Surface, CRF Flags, objBitmap *Bitmap, LONG HotX, LONG HotY, OBJECTID Owner) { return DisplayBase->_SetCustomCursor(Surface,Flags,Bitmap,HotX,HotY,Owner); }
inline ERR SetHostOption(HOST Option, LARGE Value) { return DisplayBase->_SetHostOption(Option,Value); }
inline OBJECTID SetModalSurface(OBJECTID Surface) { return DisplayBase->_SetModalSurface(Surface); }
inline ERR StartCursorDrag(OBJECTID Source, LONG Item, CSTRING Datatypes, OBJECTID Surface) { return DisplayBase->_StartCursorDrag(Source,Item,Datatypes,Surface); }
inline ERR SubscribeInput(FUNCTION *Callback, OBJECTID SurfaceFilter, JTYPE Mask, OBJECTID DeviceFilter, LONG *Handle) { return DisplayBase->_SubscribeInput(Callback,SurfaceFilter,Mask,DeviceFilter,Handle); }
inline void Sync(objBitmap *Bitmap) { return DisplayBase->_Sync(Bitmap); }
inline ERR UnlockCursor(OBJECTID Surface) { return DisplayBase->_UnlockCursor(Surface); }
inline ERR UnsubscribeInput(LONG Handle) { return DisplayBase->_UnsubscribeInput(Handle); }
inline ERR WindowHook(OBJECTID SurfaceID, WH Event, FUNCTION *Callback) { return DisplayBase->_WindowHook(SurfaceID,Event,Callback); }
} // namespace
#else
namespace gfx {
extern objPointer * AccessPointer(void);
extern ERR CheckIfChild(OBJECTID Parent, OBJECTID Child);
extern ERR CopyArea(objBitmap *Bitmap, objBitmap *Dest, BAF Flags, LONG X, LONG Y, LONG Width, LONG Height, LONG XDest, LONG YDest);
extern ERR CopyRawBitmap(struct BitmapSurfaceV2 *Surface, objBitmap *Dest, CSRF Flags, LONG X, LONG Y, LONG Width, LONG Height, LONG XDest, LONG YDest);
extern ERR CopySurface(OBJECTID Surface, objBitmap *Bitmap, BDF Flags, LONG X, LONG Y, LONG Width, LONG Height, LONG XDest, LONG YDest);
extern void DrawPixel(objBitmap *Bitmap, LONG X, LONG Y, ULONG Colour);
extern void DrawRGBPixel(objBitmap *Bitmap, LONG X, LONG Y, struct RGB8 *RGB);
extern void DrawRectangle(objBitmap *Bitmap, LONG X, LONG Y, LONG Width, LONG Height, ULONG Colour, BAF Flags);
extern ERR ExposeSurface(OBJECTID Surface, LONG X, LONG Y, LONG Width, LONG Height, EXF Flags);
extern void GetColourFormat(struct ColourFormat *Format, LONG BitsPerPixel, LONG RedMask, LONG GreenMask, LONG BlueMask, LONG AlphaMask);
extern ERR GetCursorInfo(struct CursorInfo *Info, LONG Size);
extern ERR GetCursorPos(DOUBLE *X, DOUBLE *Y);
extern ERR GetDisplayInfo(OBJECTID Display, struct DisplayInfoV3 **Info);
extern DT GetDisplayType(void);
extern CSTRING GetInputTypeName(JET Type);
extern OBJECTID GetModalSurface(void);
extern ERR GetRelativeCursorPos(OBJECTID Surface, DOUBLE *X, DOUBLE *Y);
extern ERR GetSurfaceCoords(OBJECTID Surface, LONG *X, LONG *Y, LONG *AbsX, LONG *AbsY, LONG *Width, LONG *Height);
extern ERR GetSurfaceFlags(OBJECTID Surface, RNF *Flags);
extern ERR GetSurfaceInfo(OBJECTID Surface, struct SurfaceInfoV2 **Info);
extern OBJECTID GetUserFocus(void);
extern ERR GetVisibleArea(OBJECTID Surface, LONG *X, LONG *Y, LONG *AbsX, LONG *AbsY, LONG *Width, LONG *Height);
extern ERR LockCursor(OBJECTID Surface);
extern ULONG ReadPixel(objBitmap *Bitmap, LONG X, LONG Y);
extern void ReadRGBPixel(objBitmap *Bitmap, LONG X, LONG Y, struct RGB8 **RGB);
extern ERR Resample(objBitmap *Bitmap, struct ColourFormat *ColourFormat);
extern ERR RestoreCursor(PTC Cursor, OBJECTID Owner);
extern DOUBLE ScaleToDPI(DOUBLE Value);
extern ERR ScanDisplayModes(CSTRING Filter, struct DisplayInfoV3 *Info, LONG Size);
extern void SetClipRegion(objBitmap *Bitmap, LONG Number, LONG Left, LONG Top, LONG Right, LONG Bottom, LONG Terminate);
extern ERR SetCursor(OBJECTID Surface, CRF Flags, PTC Cursor, CSTRING Name, OBJECTID Owner);
extern ERR SetCursorPos(DOUBLE X, DOUBLE Y);
extern ERR SetCustomCursor(OBJECTID Surface, CRF Flags, objBitmap *Bitmap, LONG HotX, LONG HotY, OBJECTID Owner);
extern ERR SetHostOption(HOST Option, LARGE Value);
extern OBJECTID SetModalSurface(OBJECTID Surface);
extern ERR StartCursorDrag(OBJECTID Source, LONG Item, CSTRING Datatypes, OBJECTID Surface);
extern ERR SubscribeInput(FUNCTION *Callback, OBJECTID SurfaceFilter, JTYPE Mask, OBJECTID DeviceFilter, LONG *Handle);
extern void Sync(objBitmap *Bitmap);
extern ERR UnlockCursor(OBJECTID Surface);
extern ERR UnsubscribeInput(LONG Handle);
extern ERR WindowHook(OBJECTID SurfaceID, WH Event, FUNCTION *Callback);
} // namespace
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

namespace fl {
   using namespace pf;

constexpr FieldValue WindowType(SWIN Value) { return FieldValue(FID_WindowType, LONG(Value)); }

} // namespace
