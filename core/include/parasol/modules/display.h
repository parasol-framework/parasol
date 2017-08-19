#ifndef MODULES_DISPLAY
#define MODULES_DISPLAY 1

// Name:      display.h
// Copyright: Paul Manias 2003-2017
// Generator: idl-c

#ifndef MAIN_H
#include <parasol/main.h>
#endif

#define MODVERSION_DISPLAY (1)

#ifdef __xwindows__

#include <X11/Xlib.h>
#include <X11/extensions/XShm.h>

#undef NULL
#define NULL 0
    
#endif

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
#define BAF_BLEND 0x00000002
#define BAF_FILL 0x00000001
#define BAF_COPY 0x00000004

// Flags for CopySurface().

#define CSRF_TRANSPARENT 0x00000001
#define CSRF_ALPHA 0x00000002
#define CSRF_TRANSLUCENT 0x00000004
#define CSRF_DEFAULT_FORMAT 0x00000008
#define CSRF_CLIP 0x00000010
#define CSRF_OFFSET 0x00000020

// Flags for CopyStretch().

#define CSTF_BILINEAR 0x00000001
#define CSTF_GOOD_QUALITY 0x00000001
#define CSTF_FILTER_SOURCE 0x00000002
#define CSTF_BRESENHAM 0x00000004
#define CSTF_NEIGHBOUR 0x00000008
#define CSTF_CUBIC 0x00000010
#define CSTF_BICUBIC 0x00000010
#define CSTF_CLAMP 0x00000020

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

// Flags for the bitmap Flip method.

#define FLIP_HORIZONTAL 1
#define FLIP_VERTICAL 2

// Display flags.

#define SCR_VISIBLE 0x00000001
#define SCR_AUTO_SAVE 0x00000002
#define SCR_BUFFER 0x00000004
#define SCR_NO_ACCELERATION 0x00000008
#define SCR_BIT_6 0x00000010
#define SCR_BORDERLESS 0x00000020
#define SCR_ALPHA_BLEND 0x00000040
#define SCR_COMPOSITE 0x00000040
#define SCR_MAXIMISE 0x80000000
#define SCR_CUSTOM_WINDOW 0x40000000
#define SCR_FLIPPABLE 0x20000000
#define SCR_GTF_ENABLED 0x10000000
#define SCR_DPMS_ENABLED 0x08000000
#define SCR_POWERSAVE 0x04000000
#define SCR_HOSTED 0x02000000
#define SCR_MAXSIZE 0x00100000
#define SCR_REFRESH 0x00200000
#define SCR_READ_ONLY 0xfe300019

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

// Standard pack pixel for all formats

#define PackPixel(a,b,c,d) ((((b)>>(a)->ColourFormat->RedShift) & (a)->ColourFormat->RedMask) << (a)->ColourFormat->RedPos) | \
                           ((((c)>>(a)->ColourFormat->GreenShift) & (a)->ColourFormat->GreenMask) << (a)->ColourFormat->GreenPos) | \
                           ((((d)>>(a)->ColourFormat->BlueShift) & (a)->ColourFormat->BlueMask) << (a)->ColourFormat->BluePos) | \
                           (((255>>(a)->ColourFormat->AlphaShift) & (a)->ColourFormat->AlphaMask) << (a)->ColourFormat->AlphaPos)

#define PackPixelA(a,b,c,d,e) \
   ((((b)>>(a)->ColourFormat->RedShift) & (a)->ColourFormat->RedMask) << (a)->ColourFormat->RedPos) | \
   ((((c)>>(a)->ColourFormat->GreenShift) & (a)->ColourFormat->GreenMask) << (a)->ColourFormat->GreenPos) | \
   ((((d)>>(a)->ColourFormat->BlueShift) & (a)->ColourFormat->BlueMask) << (a)->ColourFormat->BluePos) | \
   ((((e)>>(a)->ColourFormat->AlphaShift) & (a)->ColourFormat->AlphaMask) << (a)->ColourFormat->AlphaPos)

#define PackAlpha(a,b) ((((b)>>(a)->ColourFormat->AlphaShift) & (a)->ColourFormat->AlphaMask) << (a)->ColourFormat->AlphaPos)

#define PackPixelRGB(a,b) \
   ((((b)->Red>>(a)->ColourFormat->RedShift) & (a)->ColourFormat->RedMask) << (a)->ColourFormat->RedPos) | \
   ((((b)->Green>>(a)->ColourFormat->GreenShift) & (a)->ColourFormat->GreenMask) << (a)->ColourFormat->GreenPos) | \
   ((((b)->Blue>>(a)->ColourFormat->BlueShift) & (a)->ColourFormat->BlueMask) << (a)->ColourFormat->BluePos) | \
   (((255>>(a)->ColourFormat->AlphaShift) & (a)->ColourFormat->AlphaMask) << (a)->ColourFormat->AlphaPos)

#define PackPixelRGBA(a,b) \
   ((((b)->Red>>(a)->ColourFormat->RedShift) & (a)->ColourFormat->RedMask) << (a)->ColourFormat->RedPos) | \
   ((((b)->Green>>(a)->ColourFormat->GreenShift) & (a)->ColourFormat->GreenMask) << (a)->ColourFormat->GreenPos) | \
   ((((b)->Blue>>(a)->ColourFormat->BlueShift) & (a)->ColourFormat->BlueMask) << (a)->ColourFormat->BluePos) | \
   ((((b)->Alpha>>(a)->ColourFormat->AlphaShift) & (a)->ColourFormat->AlphaMask) << (a)->ColourFormat->AlphaPos)

// Pack pixel 'whole-byte' version, for faster 24/32 bit formats

#define PackPixelWB(a,b,c,d) ((b) << (a)->ColourFormat->RedPos) | ((c) << (a)->ColourFormat->GreenPos) | ((d) << (a)->ColourFormat->BluePos)
#define PackPixelWBA(a,b,c,d,e) ((b) << (a)->ColourFormat->RedPos) | ((c) << (a)->ColourFormat->GreenPos) | ((d) << (a)->ColourFormat->BluePos) | ((e) << (a)->ColourFormat->AlphaPos)

// Colour unpacking routines

#define UnpackRed(a,b)   ((((b) >> (a)->ColourFormat->RedPos) & (a)->ColourFormat->RedMask) << (a)->ColourFormat->RedShift)
#define UnpackGreen(a,b) ((((b) >> (a)->ColourFormat->GreenPos) & (a)->ColourFormat->GreenMask) << (a)->ColourFormat->GreenShift)
#define UnpackBlue(a,b)  ((((b) >> (a)->ColourFormat->BluePos) & (a)->ColourFormat->BlueMask) << (a)->ColourFormat->BlueShift)
#define UnpackAlpha(a,b) ((((b) >> (a)->ColourFormat->AlphaPos) & (a)->ColourFormat->AlphaMask))

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
  
// Bitmap class definition

#define VER_BITMAP (2.000000)

typedef struct rkBitmap {
   OBJECT_HEADER
   struct RGBPalette * Palette;                                            // Pointer to the Bitmap's palette
   struct ColourFormat * ColourFormat;                                     // Pointer to colour information
   void (*DrawUCPixel)(struct rkBitmap *, LONG, LONG, ULONG);
   void (*DrawUCRPixel)(struct rkBitmap *, LONG, LONG, struct RGB8 *);
   ULONG (*ReadUCPixel)(struct rkBitmap *, LONG, LONG);
   void (*ReadUCRPixel)(struct rkBitmap *, LONG, LONG, struct RGB8 *);
   void (*ReadUCRIndex)(struct rkBitmap *, UBYTE *, struct RGB8 *);
   void (*DrawUCRIndex)(struct rkBitmap *, UBYTE *, struct RGB8 *);
   UBYTE *  Data;                                                          // Pointer to bitmap data area
   LONG     Width;                                                         // Width
   LONG     ByteWidth;                                                     // ByteWidth (not including padding - see LineWidth)
   LONG     Height;                                                        // Height
   LONG     Type;                                                          // Bitmap type
   LONG     LineWidth;                                                     // Line differential in bytes
   LONG     PlaneMod;                                                      // Plane differential in bytes
   struct ClipRectangle Clip;                                              // Clipping rectangle
   LONG     Size;                                                          // Total size of the bitmap in bytes
   LONG     DataFlags;                                                     // Memory flags to use in allocation
   LONG     AmtColours;                                                    // Maximum amount of colours available
   LONG     Flags;                                                         // Optional flags
   LONG     TransIndex;                                                    // Colour index or packed colour value that acts as the background/transparency
   LONG     BytesPerPixel;                                                 // 1, 2, 3, 4
   LONG     BitsPerPixel;                                                  // 1, 8, 15, 16, 24, 32
   LONG     Position;                                                      // Current byte position for reading and writing
   LONG     XOffset;                                                       // Horizontal offset to apply to graphics coordinates
   LONG     YOffset;                                                       // Vertical offset to apply to graphics coordinates
   LONG     Opacity;                                                       // Opacity setting to use in drawing operations
   MEMORYID DataMID;                                                       // Memory ID of the bitmap's data, if applicable.
   struct RGB8 TransRGB;                                                   // The transparent colour of the bitmap, in RGB format.
   struct RGB8 BkgdRGB;                                                    // Background colour (for clearing, resizing)
   LONG     BkgdIndex;                                                     // Colour index or packed colour of the background.

#ifdef PRV_BITMAP
   ULONG  *Gradients;
   APTR   ResolutionChangeHandle;
   struct RGBPalette prvPaletteArray;
   struct ColourFormat prvColourFormat;
   MEMORYID prvCompressMID;
   LONG   prvAFlags;                  // Private allocation flags
   #ifdef __xwindows__
      struct {
         XImage   XImage;
         Drawable Drawable;
         XImage   *Readable;
         XShmSegmentInfo ShmInfo;
         BYTE XShmImage;
      } x11;
   #elif _WIN32
      struct {
         APTR Drawable;  // HDC for the Bitmap
      } win;
   #elif _GLES_
      ULONG prvWriteBackBuffer:1;  // For OpenGL surface locking.
      LONG prvGLPixel;
      LONG prvGLFormat;
   #endif
  
#endif
} objBitmap;

// Bitmap methods

#define MT_BmpCopyArea -1
#define MT_BmpCompress -2
#define MT_BmpDecompress -3
#define MT_BmpFlip -4
#define MT_BmpFlood -5
#define MT_BmpDrawRectangle -6
#define MT_BmpSetClipRegion -7
#define MT_BmpGetColour -8
#define MT_BmpDrawLine -9
#define MT_BmpCopyStretch -10

struct bmpCopyArea { struct rkBitmap * DestBitmap; LONG Flags; LONG X; LONG Y; LONG Width; LONG Height; LONG XDest; LONG YDest;  };
struct bmpCompress { LONG Level;  };
struct bmpDecompress { LONG RetainData;  };
struct bmpFlip { LONG Orientation;  };
struct bmpFlood { LONG X; LONG Y; ULONG Colour;  };
struct bmpDrawRectangle { LONG X; LONG Y; LONG Width; LONG Height; ULONG Colour; LONG Flags;  };
struct bmpSetClipRegion { LONG Number; LONG Left; LONG Top; LONG Right; LONG Bottom; LONG Terminate;  };
struct bmpGetColour { LONG Red; LONG Green; LONG Blue; LONG Alpha; ULONG Colour;  };
struct bmpDrawLine { LONG X; LONG Y; LONG XEnd; LONG YEnd; ULONG Colour;  };
struct bmpCopyStretch { struct rkBitmap * DestBitmap; LONG Flags; LONG X; LONG Y; LONG Width; LONG Height; LONG XDest; LONG YDest; LONG DestWidth; LONG DestHeight;  };

INLINE ERROR bmpCopyArea(APTR Ob, struct rkBitmap * DestBitmap, LONG Flags, LONG X, LONG Y, LONG Width, LONG Height, LONG XDest, LONG YDest) {
   struct bmpCopyArea args = { DestBitmap, Flags, X, Y, Width, Height, XDest, YDest };
   return(Action(MT_BmpCopyArea, Ob, &args));
}

INLINE ERROR bmpCompress(APTR Ob, LONG Level) {
   struct bmpCompress args = { Level };
   return(Action(MT_BmpCompress, Ob, &args));
}

INLINE ERROR bmpDecompress(APTR Ob, LONG RetainData) {
   struct bmpDecompress args = { RetainData };
   return(Action(MT_BmpDecompress, Ob, &args));
}

INLINE ERROR bmpFlip(APTR Ob, LONG Orientation) {
   struct bmpFlip args = { Orientation };
   return(Action(MT_BmpFlip, Ob, &args));
}

INLINE ERROR bmpFlood(APTR Ob, LONG X, LONG Y, ULONG Colour) {
   struct bmpFlood args = { X, Y, Colour };
   return(Action(MT_BmpFlood, Ob, &args));
}

INLINE ERROR bmpDrawRectangle(APTR Ob, LONG X, LONG Y, LONG Width, LONG Height, ULONG Colour, LONG Flags) {
   struct bmpDrawRectangle args = { X, Y, Width, Height, Colour, Flags };
   return(Action(MT_BmpDrawRectangle, Ob, &args));
}

INLINE ERROR bmpSetClipRegion(APTR Ob, LONG Number, LONG Left, LONG Top, LONG Right, LONG Bottom, LONG Terminate) {
   struct bmpSetClipRegion args = { Number, Left, Top, Right, Bottom, Terminate };
   return(Action(MT_BmpSetClipRegion, Ob, &args));
}

INLINE ERROR bmpDrawLine(APTR Ob, LONG X, LONG Y, LONG XEnd, LONG YEnd, ULONG Colour) {
   struct bmpDrawLine args = { X, Y, XEnd, YEnd, Colour };
   return(Action(MT_BmpDrawLine, Ob, &args));
}

INLINE ERROR bmpCopyStretch(APTR Ob, struct rkBitmap * DestBitmap, LONG Flags, LONG X, LONG Y, LONG Width, LONG Height, LONG XDest, LONG YDest, LONG DestWidth, LONG DestHeight) {
   struct bmpCopyStretch args = { DestBitmap, Flags, X, Y, Width, Height, XDest, YDest, DestWidth, DestHeight };
   return(Action(MT_BmpCopyStretch, Ob, &args));
}


INLINE ULONG bmpGetColour(APTR Bitmap, UBYTE Red, UBYTE Green, UBYTE Blue, UBYTE Alpha) {
   if (((objBitmap *)Bitmap)->BitsPerPixel > 8) {
      return PackPixelA((objBitmap *)Bitmap, Red, Green, Blue, Alpha);
   }
   else {
      struct bmpGetColour args = { Red, Green, Blue, Alpha };
      if (!Action(MT_BmpGetColour, Bitmap, &args)) return args.Colour;
      return 0;
   }
}

INLINE ULONG bmpGetColourRGB(APTR Bitmap, struct RGB8 *RGB) {
   if (((objBitmap *)Bitmap)->BitsPerPixel > 8) {
      return PackPixelA((objBitmap *)Bitmap, RGB->Red, RGB->Green, RGB->Blue, RGB->Alpha);
   }
   else {
      struct bmpGetColour args = { RGB->Red, RGB->Green, RGB->Blue, RGB->Alpha };
      if (!Action(MT_BmpGetColour, Bitmap, &args)) return args.Colour;
      return 0;
   }
}
  
// Display class definition

#define VER_DISPLAY (1.000000)

typedef struct rkDisplay {
   OBJECT_HEADER
   DOUBLE   RefreshRate;        // The active refresh rate
   struct rkBitmap * Bitmap;    // Reference to the display's bitmap information.
   LONG     Flags;              // Optional flags
   LONG     Width;              // The width of the visible display
   LONG     Height;             // The height of the visible display
   LONG     X;                  // Hardware co-ordinate for vertical offset, or position of host window
   LONG     Y;                  // Hardware co-ordinate for horizontal offset, or position of host window
   LONG     BmpX;               // Current offset of the horizontal axis
   LONG     BmpY;               // Current offset of the vertical axis
   OBJECTID BufferID;           // Double buffer bitmap
   LONG     TotalMemory;        // The total amount of accessible RAM installed on the video card.
   LONG     MinHScan;           // The minimum horizontal scan rate of the display output device.
   LONG     MaxHScan;           // The maximum horizontal scan rate of the display output device.
   LONG     MinVScan;           // The minimum vertical scan rate of the display output device.
   LONG     MaxVScan;           // The maximum vertical scan rate of the display output device.
   LONG     DisplayType;        // Indicates the display type.
   LONG     DPMS;               // Optional display power modes.
   OBJECTID PopOverID;          // For hosted displays, PopOver can refer to another display that should always remain behind us
   LONG     LeftMargin;         // Left window margin, if hosted
   LONG     RightMargin;        // Right window margin, if hosted
   LONG     TopMargin;          // Top window margin, if hosted
   LONG     BottomMargin;       // Bottom window margin, if hosted

#ifdef PRV_DISPLAY
   DOUBLE Gamma[3];          // Red, green, blue gamma radioactivity indicator
   struct resolution *Resolutions;
   FUNCTION  ResizeFeedback;
   MEMORYID  ResolutionsMID;
   WORD      TotalResolutions;
   OBJECTID  BitmapID;
   LONG      BmpXOffset;     // X offset for scrolling
   LONG      BmpYOffset;     // Y offset for scrolling
   #ifdef __xwindows__
   union {
      APTR   WindowHandle;
      Window XWindowHandle;
   };
   #elif __ANDROID__
      ANativeWindow *WindowHandle;
   #else
      APTR   WindowHandle;
   #endif
   APTR      UserLoginHandle;
   WORD      Opacity;
   LONG      VDensity;          // Cached DPI value, if calculable.
   LONG      HDensity;
   UBYTE     DriverVendor[60];
   UBYTE     DriverCopyright[80];
   UBYTE     Manufacturer[60];
   UBYTE     Chipset[40];
   UBYTE     DAC[32];
   UBYTE     Clock[32];
   UBYTE     DriverVersion[16];
   UBYTE     CertificationDate[20];
   UBYTE     Display[32];
   UBYTE     DisplayManufacturer[60];
   #ifdef _WIN32
      APTR OldProcedure;
   #endif
  
#endif
} objDisplay;

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
struct gfxUpdateDisplay { struct rkBitmap * Bitmap; LONG X; LONG Y; LONG Width; LONG Height; LONG XDest; LONG YDest;  };

#define gfxWaitVBL(obj) Action(MT_GfxWaitVBL,(obj),0)

INLINE ERROR gfxUpdatePalette(APTR Ob, struct RGBPalette * NewPalette) {
   struct gfxUpdatePalette args = { NewPalette };
   return(Action(MT_GfxUpdatePalette, Ob, &args));
}

INLINE ERROR gfxSetDisplay(APTR Ob, LONG X, LONG Y, LONG Width, LONG Height, LONG InsideWidth, LONG InsideHeight, LONG BitsPerPixel, DOUBLE RefreshRate, LONG Flags) {
   struct gfxSetDisplay args = { X, Y, Width, Height, InsideWidth, InsideHeight, BitsPerPixel, RefreshRate, Flags };
   return(Action(MT_GfxSetDisplay, Ob, &args));
}

INLINE ERROR gfxSizeHints(APTR Ob, LONG MinWidth, LONG MinHeight, LONG MaxWidth, LONG MaxHeight) {
   struct gfxSizeHints args = { MinWidth, MinHeight, MaxWidth, MaxHeight };
   return(Action(MT_GfxSizeHints, Ob, &args));
}

INLINE ERROR gfxSetGamma(APTR Ob, DOUBLE Red, DOUBLE Green, DOUBLE Blue, LONG Flags) {
   struct gfxSetGamma args = { Red, Green, Blue, Flags };
   return(Action(MT_GfxSetGamma, Ob, &args));
}

INLINE ERROR gfxSetGammaLinear(APTR Ob, DOUBLE Red, DOUBLE Green, DOUBLE Blue, LONG Flags) {
   struct gfxSetGammaLinear args = { Red, Green, Blue, Flags };
   return(Action(MT_GfxSetGammaLinear, Ob, &args));
}

INLINE ERROR gfxSetMonitor(APTR Ob, CSTRING Name, LONG MinH, LONG MaxH, LONG MinV, LONG MaxV, LONG Flags) {
   struct gfxSetMonitor args = { Name, MinH, MaxH, MinV, MaxV, Flags };
   return(Action(MT_GfxSetMonitor, Ob, &args));
}

#define gfxMinimise(obj) Action(MT_GfxMinimise,(obj),0)

INLINE ERROR gfxUpdateDisplay(APTR Ob, struct rkBitmap * Bitmap, LONG X, LONG Y, LONG Width, LONG Height, LONG XDest, LONG YDest) {
   struct gfxUpdateDisplay args = { Bitmap, X, Y, Width, Height, XDest, YDest };
   return(Action(MT_GfxUpdateDisplay, Ob, &args));
}

#define gfxCheckXWindow(obj) Action(MT_GfxCheckXWindow,(obj),0)


// Pointer class definition

#define VER_POINTER (1.000000)

typedef struct rkPointer {
   OBJECT_HEADER
   DOUBLE   Speed;         // Speed multiplier (%)
   DOUBLE   Acceleration;  // Acceleration level - keep between 0.0 and 3.0
   DOUBLE   DoubleClick;   // Double click speed
   DOUBLE   WheelSpeed;    // Mouse-wheel speed
   LONG     X;             // Current x coordinate
   LONG     Y;             // Current y coordinate
   LONG     MaxSpeed;      // Maximum speed
   LONG     OverX;         // The X coord relative to the closest object
   LONG     OverY;         // The Y coord relative to the closest object
   LONG     OverZ;         // The Z distance relative to the closest object
   OBJECTID InputID;       // Indicates where pointer input comes from (usually SystemMouse)
   OBJECTID SurfaceID;     // Top-most surface that contains the pointer
   OBJECTID AnchorID;      // Surface object that the pointer is anchored to
   LONG     CursorID;      // Current cursor image
   OBJECTID CursorOwnerID; // Who owns the currently active cursor status
   LONG     Flags;         // Optional flags
   OBJECTID RestrictID;    // Surface object that the pointer is restricted to
   LONG     HostX;         // Indicates the current position of the host cursor on Windows or X11
   LONG     HostY;         // Indicates the current position of the host cursor on Windows or X11
   OBJECTID BitmapID;      // Bitmap to use for custom cursor images
   OBJECTID DragSourceID;  // If drag & drop is active, refers to the object managing the draggable item
   LONG     DragItem;      // If drag & drop is active, refers to the custom ID of the item being dragged
   OBJECTID OverObjectID;  // Object positioned under the pointer
   LONG     ClickSlop;     // Leniency value for determining whether the user intended to make a click or drag.

#ifdef PRV_POINTER
   struct {
      LARGE LastClickTime;      // Timestamp
      OBJECTID LastClicked;     // Most recently clicked object
      UBYTE DblClick:1;         // TRUE if last click was a double-click
   } Buttons[10];
   LARGE    ClickTime;
   LARGE    AnchorTime;
   struct   Time *prvTime;
   APTR     UserLoginHandle;
   OBJECTID LastSurfaceID;      // Last object that the pointer was positioned over
   OBJECTID CursorReleaseID;
   OBJECTID DragSurface;        // Draggable surface anchored to the pointer position
   OBJECTID DragParent;         // Parent of the draggable surface
   MEMORYID MessageQueue;       // Message port of the task that holds the cursor
   MEMORYID AnchorMsgQueue;     // Message port of the task that holds the cursor anchor
   LONG     LastClickX, LastClickY;
   LONG     LastReleaseX, LastReleaseY;
   LONG     CursorRelease;
   LONG     BufferCursor;
   LONG     BufferFlags;
   MEMORYID BufferQueue;
   OBJECTID BufferOwner;
   OBJECTID BufferObject;
   UBYTE    DragData[8];          // Data preferences for current drag & drop item
   UBYTE    Device[32];
   UBYTE    ButtonOrder[12];      // The order of the first 11 buttons can be changed here
   WORD     ButtonOrderFlags[12]; // Button order represented as JD flags
   BYTE     PostComposite;        // Enable post-composite drawing (default)
   UBYTE    prvOverCursorID;
   struct {
      WORD HotX;
      WORD HotY;
   } Cursors[PTR_END];
  
#endif
} objPointer;

struct DisplayBase {
   ERROR (*_GetDisplayInfo)(OBJECTID, struct DisplayInfoV3 **);
   LONG (*_GetDisplayType)(void);
   ERROR (*_SetCursor)(OBJECTID, LONG, LONG, CSTRING, OBJECTID);
   ERROR (*_RestoreCursor)(LONG, OBJECTID);
   ERROR (*_GetCursorPos)(LONG *, LONG *);
   ERROR (*_SetCursorPos)(LONG, LONG);
   ERROR (*_GetRelativeCursorPos)(OBJECTID, LONG *, LONG *);
   ERROR (*_GetCursorInfo)(struct CursorInfo *, LONG);
   ERROR (*_SetCustomCursor)(OBJECTID, LONG, struct rkBitmap *, LONG, LONG, OBJECTID);
   struct rkPointer * (*_AccessPointer)(void);
   ERROR (*_ScanDisplayModes)(CSTRING, struct DisplayInfoV3 *, LONG);
   ERROR (*_LockCursor)(OBJECTID);
   ERROR (*_UnlockCursor)(OBJECTID);
   ERROR (*_SetHostOption)(LONG, LARGE);
   ERROR (*_StartCursorDrag)(OBJECTID, LONG, CSTRING, OBJECTID);
   ERROR (*_CopySurface)(struct BitmapSurfaceV2 *, struct rkBitmap *, LONG, LONG, LONG, LONG, LONG, LONG, LONG);
   void (*_Sync)(struct rkBitmap *);
   ERROR (*_Resample)(struct rkBitmap *, struct ColourFormat *);
   void (*_GetColourFormat)(struct ColourFormat *, LONG, LONG, LONG, LONG, LONG);
   ERROR (*_CopyArea)(struct rkBitmap *, struct rkBitmap *, LONG, LONG, LONG, LONG, LONG, LONG, LONG);
   ERROR (*_CopyStretch)(struct rkBitmap *, struct rkBitmap *, LONG, LONG, LONG, LONG, LONG, LONG, LONG, LONG, LONG);
   void (*_ReadRGBPixel)(struct rkBitmap *, LONG, LONG, struct RGB8 **);
   ULONG (*_ReadPixel)(struct rkBitmap *, LONG, LONG);
   void (*_DrawRGBPixel)(struct rkBitmap *, LONG, LONG, struct RGB8 *);
   void (*_DrawPixel)(struct rkBitmap *, LONG, LONG, ULONG);
   void (*_DrawLine)(struct rkBitmap *, LONG, LONG, LONG, LONG, ULONG);
   void (*_DrawRectangle)(struct rkBitmap *, LONG, LONG, LONG, LONG, ULONG, LONG);
   void (*_FlipBitmap)(struct rkBitmap *, LONG);
   void (*_SetClipRegion)(struct rkBitmap *, LONG, LONG, LONG, LONG, LONG, LONG);
   ERROR (*_Compress)(struct rkBitmap *, LONG);
   ERROR (*_Decompress)(struct rkBitmap *, LONG);
   ERROR (*_Flood)(struct rkBitmap *, LONG, LONG, ULONG);
   void (*_DrawEllipse)(struct rkBitmap *, LONG, LONG, LONG, LONG, ULONG, LONG);
   ERROR (*_SubscribeInput)(OBJECTID, LONG, OBJECTID);
   ERROR (*_UnsubscribeInput)(OBJECTID);
   ERROR (*_GetInputMsg)(struct dcInputReady *, LONG, struct InputMsg **);
   CSTRING (*_GetInputTypeName)(LONG);
   DOUBLE (*_ScaleToDPI)(DOUBLE);
};

#ifndef PRV_DISPLAY_MODULE
#define gfxGetDisplayInfo(...) (DisplayBase->_GetDisplayInfo)(__VA_ARGS__)
#define gfxGetDisplayType(...) (DisplayBase->_GetDisplayType)(__VA_ARGS__)
#define gfxSetCursor(...) (DisplayBase->_SetCursor)(__VA_ARGS__)
#define gfxRestoreCursor(...) (DisplayBase->_RestoreCursor)(__VA_ARGS__)
#define gfxGetCursorPos(...) (DisplayBase->_GetCursorPos)(__VA_ARGS__)
#define gfxSetCursorPos(...) (DisplayBase->_SetCursorPos)(__VA_ARGS__)
#define gfxGetRelativeCursorPos(...) (DisplayBase->_GetRelativeCursorPos)(__VA_ARGS__)
#define gfxGetCursorInfo(a) (DisplayBase->_GetCursorInfo)(a,sizeof(*a))
#define gfxSetCustomCursor(...) (DisplayBase->_SetCustomCursor)(__VA_ARGS__)
#define gfxAccessPointer(...) (DisplayBase->_AccessPointer)(__VA_ARGS__)
#define gfxScanDisplayModes(a,b) (DisplayBase->_ScanDisplayModes)(a,b,sizeof(*b))
#define gfxLockCursor(...) (DisplayBase->_LockCursor)(__VA_ARGS__)
#define gfxUnlockCursor(...) (DisplayBase->_UnlockCursor)(__VA_ARGS__)
#define gfxSetHostOption(...) (DisplayBase->_SetHostOption)(__VA_ARGS__)
#define gfxStartCursorDrag(...) (DisplayBase->_StartCursorDrag)(__VA_ARGS__)
#define gfxCopySurface(...) (DisplayBase->_CopySurface)(__VA_ARGS__)
#define gfxSync(...) (DisplayBase->_Sync)(__VA_ARGS__)
#define gfxResample(...) (DisplayBase->_Resample)(__VA_ARGS__)
#define gfxGetColourFormat(...) (DisplayBase->_GetColourFormat)(__VA_ARGS__)
#define gfxCopyArea(...) (DisplayBase->_CopyArea)(__VA_ARGS__)
#define gfxCopyStretch(...) (DisplayBase->_CopyStretch)(__VA_ARGS__)
#define gfxReadRGBPixel(...) (DisplayBase->_ReadRGBPixel)(__VA_ARGS__)
#define gfxReadPixel(...) (DisplayBase->_ReadPixel)(__VA_ARGS__)
#define gfxDrawRGBPixel(...) (DisplayBase->_DrawRGBPixel)(__VA_ARGS__)
#define gfxDrawPixel(...) (DisplayBase->_DrawPixel)(__VA_ARGS__)
#define gfxDrawLine(...) (DisplayBase->_DrawLine)(__VA_ARGS__)
#define gfxDrawRectangle(...) (DisplayBase->_DrawRectangle)(__VA_ARGS__)
#define gfxFlipBitmap(...) (DisplayBase->_FlipBitmap)(__VA_ARGS__)
#define gfxSetClipRegion(...) (DisplayBase->_SetClipRegion)(__VA_ARGS__)
#define gfxCompress(...) (DisplayBase->_Compress)(__VA_ARGS__)
#define gfxDecompress(...) (DisplayBase->_Decompress)(__VA_ARGS__)
#define gfxFlood(...) (DisplayBase->_Flood)(__VA_ARGS__)
#define gfxDrawEllipse(...) (DisplayBase->_DrawEllipse)(__VA_ARGS__)
#define gfxSubscribeInput(...) (DisplayBase->_SubscribeInput)(__VA_ARGS__)
#define gfxUnsubscribeInput(...) (DisplayBase->_UnsubscribeInput)(__VA_ARGS__)
#define gfxGetInputMsg(...) (DisplayBase->_GetInputMsg)(__VA_ARGS__)
#define gfxGetInputTypeName(...) (DisplayBase->_GetInputTypeName)(__VA_ARGS__)
#define gfxScaleToDPI(...) (DisplayBase->_ScaleToDPI)(__VA_ARGS__)
#endif

#define gfxReleasePointer(a)    (ReleaseObject(a))
  
#endif
