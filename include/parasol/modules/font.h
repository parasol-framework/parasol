#pragma once

// Name:      font.h
// Copyright: Paul Manias © 1998-2023
// Generator: idl-c

#include <parasol/main.h>

#define MODVERSION_FONT (1)

#include <parasol/modules/display.h>

class objFont;

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

class objFont : public BaseClass {
   public:
   static constexpr CLASSID CLASS_ID = ID_FONT;
   static constexpr CSTRING CLASS_NAME = "Font";

   using create = pf::Create<objFont>;

   DOUBLE Angle;                                         // A rotation angle to use when drawing scalable fonts.
   DOUBLE Point;                                         // The point size of a font.
   DOUBLE StrokeSize;                                    // The strength of stroked outlines is defined here.
   objBitmap * Bitmap;                                   // The destination Bitmap to use when drawing a font.
   STRING String;                                        // The string to use when drawing a Font.
   STRING Path;                                          // The path to a font file.
   STRING Style;                                         // Determines font styling.
   STRING Face;                                          // The name of a font face that is to be loaded on initialisation.
   ERROR (*WrapCallback)(objFont *, LONG *, LONG *);     // The routine defined here will be called when the wordwrap boundary is encountered.
   APTR   EscapeCallback;                                // The routine defined here will be called when escape characters are encountered.
   APTR   UserData;                                      // Optional storage variable for user data; ignored by the Font class.
   struct RGB8 Outline;                                  // Defines the outline colour around a font.
   struct RGB8 Underline;                                // Enables font underlining when set.
   struct RGB8 Colour;                                   // The font colour in RGB format.
   LONG   Flags;                                         // Optional flags.
   LONG   Gutter;                                        // The 'external leading' value, measured in pixels.  Applies to fixed fonts only.
   LONG   GlyphSpacing;                                  // The amount of spacing between each character.
   LONG   LineSpacing;                                   // The amount of spacing between each line.
   LONG   X;                                             // The starting horizontal position when drawing the font string.
   LONG   Y;                                             // The starting vertical position when drawing the font string.
   LONG   TabSize;                                       // Defines the tab size to use when drawing and manipulating a font string.
   LONG   TotalChars;                                    // Reflects the total number of character glyphs that are available by the font object.
   LONG   WrapEdge;                                      // Enables word wrapping at a given boundary.
   LONG   FixedWidth;                                    // Forces a fixed pixel width to use for all glyphs.
   LONG   Height;                                        // The point size of the font, expressed in pixels.
   LONG   Leading;                                       // 'Internal leading' measured in pixels.  Applies to fixed fonts only.
   LONG   MaxHeight;                                     // The maximum possible pixel height per character.
   LONG   Align;                                         // Sets the position of a font string to an abstract alignment.
   LONG   AlignWidth;                                    // The width to use when aligning the font string.
   LONG   AlignHeight;                                   // The height to use when aligning the font string.
   LONG   Ascent;                                        // The total number of pixels above the baseline.
   LONG   EndX;                                          // Indicates the final horizontal coordinate after completing a draw operation.
   LONG   EndY;                                          // Indicates the final vertical coordinate after completing a draw operation.
   LONG   VDPI;                                          // Defines the vertical dots-per-inch of the target device.
   LONG   HDPI;                                          // Defines the horizontal dots-per-inch of the target device.

   // Action stubs

   inline ERROR draw() { return Action(AC_Draw, this, NULL); }
   inline ERROR drawArea(LONG X, LONG Y, LONG Width, LONG Height) {
      struct acDraw args = { X, Y, Width, Height };
      return Action(AC_Draw, this, &args);
   }
   inline ERROR init() { return Action(AC_Init, this, NULL); }
};

extern struct FontBase *FontBase;
struct FontBase {
   ERROR (*_GetList)(struct FontList ** Result);
   LONG (*_StringWidth)(objFont * Font, CSTRING String, LONG Chars);
   void (*_StringSize)(objFont * Font, CSTRING String, LONG Chars, LONG Wrap, LONG * Width, LONG * Rows);
   ERROR (*_ConvertCoords)(objFont * Font, CSTRING String, LONG X, LONG Y, LONG * Column, LONG * Row, LONG * ByteColumn, LONG * BytePos, LONG * CharX);
   LONG (*_CharWidth)(objFont * Font, ULONG Char, ULONG KChar, LONG * Kerning);
   DOUBLE (*_SetDefaultSize)(DOUBLE Size);
   APTR (*_FreetypeHandle)(void);
   ERROR (*_InstallFont)(CSTRING Files);
   ERROR (*_RemoveFont)(CSTRING Name);
   ERROR (*_SelectFont)(CSTRING Name, CSTRING Style, LONG Point, LONG Flags, CSTRING * Path);
};

#ifndef PRV_FONT_MODULE
inline ERROR fntGetList(struct FontList ** Result) { return FontBase->_GetList(Result); }
inline LONG fntStringWidth(objFont * Font, CSTRING String, LONG Chars) { return FontBase->_StringWidth(Font,String,Chars); }
inline void fntStringSize(objFont * Font, CSTRING String, LONG Chars, LONG Wrap, LONG * Width, LONG * Rows) { return FontBase->_StringSize(Font,String,Chars,Wrap,Width,Rows); }
inline ERROR fntConvertCoords(objFont * Font, CSTRING String, LONG X, LONG Y, LONG * Column, LONG * Row, LONG * ByteColumn, LONG * BytePos, LONG * CharX) { return FontBase->_ConvertCoords(Font,String,X,Y,Column,Row,ByteColumn,BytePos,CharX); }
inline LONG fntCharWidth(objFont * Font, ULONG Char, ULONG KChar, LONG * Kerning) { return FontBase->_CharWidth(Font,Char,KChar,Kerning); }
inline DOUBLE fntSetDefaultSize(DOUBLE Size) { return FontBase->_SetDefaultSize(Size); }
inline APTR fntFreetypeHandle(void) { return FontBase->_FreetypeHandle(); }
inline ERROR fntInstallFont(CSTRING Files) { return FontBase->_InstallFont(Files); }
inline ERROR fntRemoveFont(CSTRING Name) { return FontBase->_RemoveFont(Name); }
inline ERROR fntSelectFont(CSTRING Name, CSTRING Style, LONG Point, LONG Flags, CSTRING * Path) { return FontBase->_SelectFont(Name,Style,Point,Flags,Path); }
#endif

