#ifndef MODULES_FONT
#define MODULES_FONT 1

// Name:      font.h
// Copyright: Paul Manias © 1998-2022
// Generator: idl-c

#ifndef MAIN_H
#include <parasol/main.h>
#endif

#define MODVERSION_FONT (1)

#ifndef MODULES_DISPLAY_H
#include <parasol/modules/display.h>
#endif

// Font flags

#define FTF_PREFER_SCALED 0x00000001
#define FTF_PREFER_FIXED 0x00000002
#define FTF_REQUIRE_SCALED 0x00000004
#define FTF_REQUIRE_FIXED 0x00000008
#define FTF_ANTIALIAS 0x00000010
#define FTF_SMOOTH 0x00000010
#define FTF_HEAVY_LINE 0x00000020
#define FTF_QUICK_ALIAS 0x00000040
#define FTF_CHAR_CLIP 0x00000080
#define FTF_BASE_LINE 0x00000100
#define FTF_ALLOW_SCALE 0x00000200
#define FTF_SCALABLE 0x10000000
#define FTF_BOLD 0x20000000
#define FTF_ITALIC 0x40000000
#define FTF_KERNING 0x80000000

struct FontList {
   struct FontList * Next;    // Pointer to the next entry in the list.
   STRING Name;               // The name of the font face.
   LONG * Points;             // Pointer to an array of fixed point sizes supported by the font.
   STRING Styles;             // Supported styles are listed here in CSV format.
   BYTE   Scalable;           // TRUE if the font is scalable.
   BYTE   Reserved1;          // Do not use.
   WORD   Reserved2;          // Do not use.
};

// Options for the StringSize() function.

#define FSS_ALL -1
#define FSS_LINE -2

// Font class definition

#define VER_FONT (1.000000)

typedef struct rkFont {
   OBJECT_HEADER
   DOUBLE Angle;                                               // Rotation angle to use when drawing the font.
   DOUBLE Point;                                               // The Point/Height of the font (arbitrary size, non-exact)
   DOUBLE StrokeSize;                                          // For scalable fonts only.  Indicates the strength of the stroked outline if Outline.Alpha > 0
   struct rkBitmap * Bitmap;                                   // Pointer to destination Bitmap
   STRING String;                                              // String for drawing etc
   STRING Path;                                                // The location of the font file as derived from the name of the face.  Field may be set directly if the font source is unregistered
   STRING Style;                                               // The font style (Regular, Bold, Italic, Condensed, Light etc)
   STRING Face;                                                // The face of the font
   ERROR (*WrapCallback)(struct rkFont *, LONG *, LONG *);     // Callback on encountering wordwrap
   APTR   EscapeCallback;                                      // Callback on encountering an escape character
   APTR   UserData;                                            // User relevant data
   struct RGB8 Outline;                                        // Outline colour
   struct RGB8 Underline;                                      // Underline colour
   struct RGB8 Colour;                                         // Colour
   LONG   Flags;                                               // Optional flags
   LONG   Gutter;                                              // "External leading" in pixels (fixed fonts only)
   LONG   GlyphSpacing;                                        // Amount of spacing between characters (additional to normal horizontal spacing)
   LONG   LineSpacing;                                         // Vertical spacing between lines
   LONG   X;                                                   // X Coordinate
   LONG   Y;                                                   // Y Coordinate
   LONG   TabSize;                                             // Tab differential
   LONG   TotalChars;                                          // Total number of characters / glyphs supported by the font
   LONG   WrapEdge;                                            // Point at which word-wrap is encountered
   LONG   FixedWidth;                                          // Force fixed width
   LONG   Height;                                              // The point size of the font (vertical bearing), expressed in pixels.  Does not include leading (see Ascent if you want leading included)
   LONG   Leading;                                             // "Internal leading" in pixels (fixed fonts only)
   LONG   MaxHeight;                                           // Maximum possible pixel height per character, covering the entire character set at the chosen point size
   LONG   Align;                                               // Alignment options when drawing the font.
   LONG   AlignWidth;                                          // Width to use for right alignment
   LONG   AlignHeight;                                         // Height to use for bottom alignment
   LONG   Ascent;                                              // The total number of pixels above the baseline, including leading
   LONG   EndX;                                                // Finished horizontal coordinate after drawing
   LONG   EndY;                                                // Finished vertical coordinate after drawing
   LONG   VDPI;                                                // Target DPI - this might be used if targeting DPI on paper rather than the display.
   LONG   HDPI;

#ifdef PRV_FONT
   WORD *prvTabs;                // Array of tab stops
   UBYTE *prvData;
   class font_cache *Cache;     // Reference to the Truetype font that is in use
   struct FontCharacter *prvChar;
   struct BitmapCache *BmpCache;
   struct font_glyph prvTempGlyph;
   LONG prvLineCount;
   LONG prvStrWidth;
   WORD prvSpaceWidth;          // Pixel width of word breaks
   WORD prvBitmapHeight;
   WORD prvLineCountCR;
   char prvEscape[2];
   char prvFace[32];
   char prvBuffer[80];
   char prvStyle[20];
   char prvDefaultChar;
   UBYTE prvTotalTabs;
  
#endif
} objFont;

struct FontBase {
   ERROR (*_GetList)(struct FontList **);
   LONG (*_StringWidth)(struct rkFont *, CSTRING, LONG);
   void (*_StringSize)(struct rkFont *, CSTRING, LONG, LONG, LONG *, LONG *);
   ERROR (*_ConvertCoords)(struct rkFont *, CSTRING, LONG, LONG, LONG *, LONG *, LONG *, LONG *, LONG *);
   LONG (*_CharWidth)(struct rkFont *, ULONG, ULONG, LONG *);
   DOUBLE (*_SetDefaultSize)(DOUBLE);
   APTR (*_FreetypeHandle)(void);
   ERROR (*_InstallFont)(CSTRING);
   ERROR (*_RemoveFont)(CSTRING);
   ERROR (*_SelectFont)(CSTRING, CSTRING, LONG, LONG, CSTRING *);
};

#ifndef PRV_FONT_MODULE
#define fntGetList(...) (FontBase->_GetList)(__VA_ARGS__)
#define fntStringWidth(...) (FontBase->_StringWidth)(__VA_ARGS__)
#define fntStringSize(...) (FontBase->_StringSize)(__VA_ARGS__)
#define fntConvertCoords(...) (FontBase->_ConvertCoords)(__VA_ARGS__)
#define fntCharWidth(...) (FontBase->_CharWidth)(__VA_ARGS__)
#define fntSetDefaultSize(...) (FontBase->_SetDefaultSize)(__VA_ARGS__)
#define fntFreetypeHandle(...) (FontBase->_FreetypeHandle)(__VA_ARGS__)
#define fntInstallFont(...) (FontBase->_InstallFont)(__VA_ARGS__)
#define fntRemoveFont(...) (FontBase->_RemoveFont)(__VA_ARGS__)
#define fntSelectFont(...) (FontBase->_SelectFont)(__VA_ARGS__)
#endif

#endif
