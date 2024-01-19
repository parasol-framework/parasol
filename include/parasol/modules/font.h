#pragma once

// Name:      font.h
// Copyright: Paul Manias © 1998-2024
// Generator: idl-c

#include <parasol/main.h>

#define MODVERSION_FONT (1)

#include <parasol/modules/display.h>

class objFont;

// Font flags

enum class FTF : ULONG {
   NIL = 0,
   PREFER_SCALED = 0x00000001,
   PREFER_FIXED = 0x00000002,
   REQUIRE_SCALED = 0x00000004,
   REQUIRE_FIXED = 0x00000008,
   ANTIALIAS = 0x00000010,
   SMOOTH = 0x00000010,
   HEAVY_LINE = 0x00000020,
   QUICK_ALIAS = 0x00000040,
   CHAR_CLIP = 0x00000080,
   BASE_LINE = 0x00000100,
   ALLOW_SCALE = 0x00000200,
   NO_BLEND = 0x00000400,
   SCALABLE = 0x10000000,
   BOLD = 0x20000000,
   ITALIC = 0x40000000,
   KERNING = 0x80000000,
};

DEFINE_ENUM_FLAG_OPERATORS(FTF)

// Options for the StringSize() function.

#define FSS_ALL -1
#define FSS_LINE -2

struct FontList {
   struct FontList * Next;    // Pointer to the next entry in the list.
   STRING Name;               // The name of the font face.
   LONG * Points;             // Pointer to an array of fixed point sizes supported by the font.
   STRING Styles;             // Supported styles are listed here in CSV format.
   BYTE   Scalable;           // TRUE if the font is scalable.
   BYTE   Reserved1;          // Do not use.
   WORD   Reserved2;          // Do not use.
};

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
   FTF    Flags;                                         // Optional flags.
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
   ALIGN  Align;                                         // Sets the position of a font string to an abstract alignment.
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
   inline ERROR init() { return InitObject(this); }

   // Customised field setting

   inline ERROR setAngle(const DOUBLE Value) {
      this->Angle = Value;
      return ERR_Okay;
   }

   inline ERROR setPoint(const DOUBLE Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[12];
      Variable var(Value);
      return field->WriteValue(target, field, FD_VARIABLE, &var, 1);
   }

   inline ERROR setStrokeSize(const DOUBLE Value) {
      this->StrokeSize = Value;
      return ERR_Okay;
   }

   inline ERROR setBitmap(objBitmap * Value) {
      this->Bitmap = Value;
      return ERR_Okay;
   }

   template <class T> inline ERROR setString(T && Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[15];
      return field->WriteValue(target, field, 0x08800300, to_cstring(Value), 1);
   }

   template <class T> inline ERROR setPath(T && Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[30];
      return field->WriteValue(target, field, 0x08800300, to_cstring(Value), 1);
   }

   template <class T> inline ERROR setStyle(T && Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[13];
      return field->WriteValue(target, field, 0x08800500, to_cstring(Value), 1);
   }

   template <class T> inline ERROR setFace(T && Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[27];
      return field->WriteValue(target, field, 0x08800500, to_cstring(Value), 1);
   }

   inline ERROR setEscapeCallback(APTR Value) {
      this->EscapeCallback = Value;
      return ERR_Okay;
   }

   inline ERROR setUserData(APTR Value) {
      this->UserData = Value;
      return ERR_Okay;
   }

   inline ERROR setOutline(const struct RGB8 Value) {
      this->Outline = Value;
      return ERR_Okay;
   }

   inline ERROR setUnderline(const struct RGB8 Value) {
      this->Underline = Value;
      return ERR_Okay;
   }

   inline ERROR setColour(const struct RGB8 Value) {
      this->Colour = Value;
      return ERR_Okay;
   }

   inline ERROR setFlags(const FTF Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[10];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERROR setGutter(const LONG Value) {
      if (this->initialised()) return ERR_NoFieldAccess;
      this->Gutter = Value;
      return ERR_Okay;
   }

   inline ERROR setGlyphSpacing(const LONG Value) {
      this->GlyphSpacing = Value;
      return ERR_Okay;
   }

   inline ERROR setLineSpacing(const LONG Value) {
      this->LineSpacing = Value;
      return ERR_Okay;
   }

   inline ERROR setX(const LONG Value) {
      this->X = Value;
      return ERR_Okay;
   }

   inline ERROR setY(const LONG Value) {
      this->Y = Value;
      return ERR_Okay;
   }

   inline ERROR setTabSize(const LONG Value) {
      this->TabSize = Value;
      return ERR_Okay;
   }

   inline ERROR setWrapEdge(const LONG Value) {
      this->WrapEdge = Value;
      return ERR_Okay;
   }

   inline ERROR setFixedWidth(const LONG Value) {
      this->FixedWidth = Value;
      return ERR_Okay;
   }

   inline ERROR setHeight(const LONG Value) {
      if (this->initialised()) return ERR_NoFieldAccess;
      this->Height = Value;
      return ERR_Okay;
   }

   inline ERROR setMaxHeight(const LONG Value) {
      if (this->initialised()) return ERR_NoFieldAccess;
      this->MaxHeight = Value;
      return ERR_Okay;
   }

   inline ERROR setAlign(const ALIGN Value) {
      this->Align = Value;
      return ERR_Okay;
   }

   inline ERROR setAlignWidth(const LONG Value) {
      this->AlignWidth = Value;
      return ERR_Okay;
   }

   inline ERROR setAlignHeight(const LONG Value) {
      this->AlignHeight = Value;
      return ERR_Okay;
   }

   inline ERROR setEndX(const LONG Value) {
      this->EndX = Value;
      return ERR_Okay;
   }

   inline ERROR setEndY(const LONG Value) {
      this->EndY = Value;
      return ERR_Okay;
   }

   inline ERROR setVDPI(const LONG Value) {
      if (this->initialised()) return ERR_NoFieldAccess;
      this->VDPI = Value;
      return ERR_Okay;
   }

   inline ERROR setHDPI(const LONG Value) {
      if (this->initialised()) return ERR_NoFieldAccess;
      this->HDPI = Value;
      return ERR_Okay;
   }

   inline ERROR setBold(const LONG Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[24];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   template <class T> inline ERROR setEscapeChar(T && Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[38];
      return field->WriteValue(target, field, 0x08800308, to_cstring(Value), 1);
   }

   inline ERROR setItalic(const LONG Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[5];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERROR setOpacity(const DOUBLE Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[22];
      return field->WriteValue(target, field, FD_DOUBLE, &Value, 1);
   }

   inline ERROR setTabs(const WORD * Value, LONG Elements) {
      auto target = this;
      auto field = &this->Class->Dictionary[31];
      return field->WriteValue(target, field, 0x00401308, Value, Elements);
   }

};

#ifdef PARASOL_STATIC
#define JUMPTABLE_FONT static struct FontBase *FontBase;
#else
#define JUMPTABLE_FONT struct FontBase *FontBase;
#endif

struct FontBase {
#ifndef PARASOL_STATIC
   ERROR (*_GetList)(struct FontList ** Result);
   LONG (*_StringWidth)(objFont * Font, CSTRING String, LONG Chars);
   void (*_StringSize)(objFont * Font, CSTRING String, LONG Chars, LONG Wrap, LONG * Width, LONG * Rows);
   ERROR (*_ConvertCoords)(objFont * Font, CSTRING String, LONG X, LONG Y, LONG * Column, LONG * Row, LONG * ByteColumn, LONG * BytePos, LONG * CharX);
   LONG (*_CharWidth)(objFont * Font, ULONG Char, ULONG KChar, LONG * Kerning);
   DOUBLE (*_SetDefaultSize)(DOUBLE Size);
   APTR (*_FreetypeHandle)(void);
   ERROR (*_InstallFont)(CSTRING Files);
   ERROR (*_RemoveFont)(CSTRING Name);
   ERROR (*_SelectFont)(CSTRING Name, CSTRING Style, LONG Point, FTF Flags, CSTRING * Path);
#endif // PARASOL_STATIC
};

#ifndef PRV_FONT_MODULE
#ifndef PARASOL_STATIC
extern struct FontBase *FontBase;
inline ERROR fntGetList(struct FontList ** Result) { return FontBase->_GetList(Result); }
inline LONG fntStringWidth(objFont * Font, CSTRING String, LONG Chars) { return FontBase->_StringWidth(Font,String,Chars); }
inline void fntStringSize(objFont * Font, CSTRING String, LONG Chars, LONG Wrap, LONG * Width, LONG * Rows) { return FontBase->_StringSize(Font,String,Chars,Wrap,Width,Rows); }
inline ERROR fntConvertCoords(objFont * Font, CSTRING String, LONG X, LONG Y, LONG * Column, LONG * Row, LONG * ByteColumn, LONG * BytePos, LONG * CharX) { return FontBase->_ConvertCoords(Font,String,X,Y,Column,Row,ByteColumn,BytePos,CharX); }
inline LONG fntCharWidth(objFont * Font, ULONG Char, ULONG KChar, LONG * Kerning) { return FontBase->_CharWidth(Font,Char,KChar,Kerning); }
inline DOUBLE fntSetDefaultSize(DOUBLE Size) { return FontBase->_SetDefaultSize(Size); }
inline APTR fntFreetypeHandle(void) { return FontBase->_FreetypeHandle(); }
inline ERROR fntInstallFont(CSTRING Files) { return FontBase->_InstallFont(Files); }
inline ERROR fntRemoveFont(CSTRING Name) { return FontBase->_RemoveFont(Name); }
inline ERROR fntSelectFont(CSTRING Name, CSTRING Style, LONG Point, FTF Flags, CSTRING * Path) { return FontBase->_SelectFont(Name,Style,Point,Flags,Path); }
#else
extern "C" {
extern ERROR fntGetList(struct FontList ** Result);
extern LONG fntStringWidth(objFont * Font, CSTRING String, LONG Chars);
extern void fntStringSize(objFont * Font, CSTRING String, LONG Chars, LONG Wrap, LONG * Width, LONG * Rows);
extern ERROR fntConvertCoords(objFont * Font, CSTRING String, LONG X, LONG Y, LONG * Column, LONG * Row, LONG * ByteColumn, LONG * BytePos, LONG * CharX);
extern LONG fntCharWidth(objFont * Font, ULONG Char, ULONG KChar, LONG * Kerning);
extern DOUBLE fntSetDefaultSize(DOUBLE Size);
extern APTR fntFreetypeHandle(void);
extern ERROR fntInstallFont(CSTRING Files);
extern ERROR fntRemoveFont(CSTRING Name);
extern ERROR fntSelectFont(CSTRING Name, CSTRING Style, LONG Point, FTF Flags, CSTRING * Path);
}
#endif // PARASOL_STATIC
#endif

