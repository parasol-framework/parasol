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
   REQUIRE_SCALED = 0x00000001,
   REQUIRE_FIXED = 0x00000002,
   HEAVY_LINE = 0x00000004,
   BASE_LINE = 0x00000008,
   VARIABLE = 0x08000000,
   SCALABLE = 0x10000000,
   BOLD = 0x20000000,
   ITALIC = 0x40000000,
   KERNING = 0x80000000,
};

DEFINE_ENUM_FLAG_OPERATORS(FTF)

// Result flags for the SelectFont() function.

enum class FMETA : ULONG {
   NIL = 0,
   SCALED = 0x00000001,
   VARIABLE = 0x00000002,
   HINT_NORMAL = 0x00000004,
   HINT_LIGHT = 0x00000008,
   HINT_INTERNAL = 0x00000010,
   HIDDEN = 0x00000020,
};

DEFINE_ENUM_FLAG_OPERATORS(FMETA)

// Force hinting options for a font.

enum class HINT : BYTE {
   NIL = 0,
   NORMAL = 1,
   INTERNAL = 2,
   LIGHT = 3,
};

// Options for the StringSize() function.

#define FSS_ALL -1
#define FSS_LINE -2

struct FontList {
   struct FontList * Next;    // Pointer to the next entry in the list.
   STRING Name;               // The name of the font face.
   STRING Alias;              // Reference to another font Name if this is an alias.
   LONG * Points;             // Pointer to an array of fixed point sizes supported by the font.
   STRING Styles;             // Supported styles are listed here in CSV format.
   BYTE   Scalable;           // TRUE if the font is scalable.
   BYTE   Variable;           // TRUE if the font has variable metrics.
   HINT   Hinting;            // Hinting options
   BYTE   Hidden;             // TRUE if the font should be hidden from user font lists.
};

// Font class definition

#define VER_FONT (1.000000)

class objFont : public BaseClass {
   public:
   static constexpr CLASSID CLASS_ID = ID_FONT;
   static constexpr CSTRING CLASS_NAME = "Font";

   using create = pf::Create<objFont>;

   DOUBLE Point;           // The point size of a font.
   DOUBLE GlyphSpacing;    // Adjusts the amount of spacing between each character.
   objBitmap * Bitmap;     // The destination Bitmap to use when drawing a font.
   STRING String;          // The string to use when drawing a Font.
   STRING Path;            // The path to a font file.
   STRING Style;           // Determines font styling.
   STRING Face;            // The name of a font face that is to be loaded on initialisation.
   struct RGB8 Outline;    // Defines the outline colour around a font.
   struct RGB8 Underline;  // Enables font underlining when set.
   struct RGB8 Colour;     // The font colour in RGB format.
   FTF    Flags;           // Optional flags.
   LONG   Gutter;          // The 'external leading' value, measured in pixels.  Applies to fixed fonts only.
   LONG   LineSpacing;     // The amount of spacing between each line.
   LONG   X;               // The starting horizontal position when drawing the font string.
   LONG   Y;               // The starting vertical position when drawing the font string.
   LONG   TabSize;         // Defines the tab size to use when drawing and manipulating a font string.
   LONG   TotalGlyphs;     // Reflects the total number of character glyphs that are available by the font object.
   LONG   WrapEdge;        // Enables word wrapping at a given boundary.
   LONG   FixedWidth;      // Forces a fixed pixel width to use for all glyphs.
   LONG   Height;          // The point size of the font, expressed in pixels.
   LONG   Leading;         // 'Internal leading' measured in pixels.  Applies to fixed fonts only.
   LONG   MaxHeight;       // The maximum possible pixel height per character.
   ALIGN  Align;           // Sets the position of a font string to an abstract alignment.
   LONG   AlignWidth;      // The width to use when aligning the font string.
   LONG   AlignHeight;     // The height to use when aligning the font string.
   LONG   Ascent;          // The total number of pixels above the baseline.
   LONG   EndX;            // Indicates the final horizontal coordinate after completing a draw operation.
   LONG   EndY;            // Indicates the final vertical coordinate after completing a draw operation.

   // Action stubs

   inline ERROR draw() noexcept { return Action(AC_Draw, this, NULL); }
   inline ERROR drawArea(LONG X, LONG Y, LONG Width, LONG Height) noexcept {
      struct acDraw args = { X, Y, Width, Height };
      return Action(AC_Draw, this, &args);
   }
   inline ERROR init() noexcept { return InitObject(this); }

   // Customised field setting

   inline ERROR setPoint(const DOUBLE Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[11];
      return field->WriteValue(target, field, FD_DOUBLE, &Value, 1);
   }

   inline ERROR setGlyphSpacing(const DOUBLE Value) noexcept {
      this->GlyphSpacing = Value;
      return ERR_Okay;
   }

   inline ERROR setBitmap(objBitmap * Value) noexcept {
      this->Bitmap = Value;
      return ERR_Okay;
   }

   template <class T> inline ERROR setString(T && Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[14];
      return field->WriteValue(target, field, 0x08800300, to_cstring(Value), 1);
   }

   template <class T> inline ERROR setPath(T && Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[25];
      return field->WriteValue(target, field, 0x08800300, to_cstring(Value), 1);
   }

   template <class T> inline ERROR setStyle(T && Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[12];
      return field->WriteValue(target, field, 0x08800500, to_cstring(Value), 1);
   }

   template <class T> inline ERROR setFace(T && Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[23];
      return field->WriteValue(target, field, 0x08800500, to_cstring(Value), 1);
   }

   inline ERROR setOutline(const struct RGB8 Value) noexcept {
      this->Outline = Value;
      return ERR_Okay;
   }

   inline ERROR setUnderline(const struct RGB8 Value) noexcept {
      this->Underline = Value;
      return ERR_Okay;
   }

   inline ERROR setColour(const struct RGB8 Value) noexcept {
      this->Colour = Value;
      return ERR_Okay;
   }

   inline ERROR setFlags(const FTF Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[9];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERROR setGutter(const LONG Value) noexcept {
      if (this->initialised()) return ERR_NoFieldAccess;
      this->Gutter = Value;
      return ERR_Okay;
   }

   inline ERROR setLineSpacing(const LONG Value) noexcept {
      this->LineSpacing = Value;
      return ERR_Okay;
   }

   inline ERROR setX(const LONG Value) noexcept {
      this->X = Value;
      return ERR_Okay;
   }

   inline ERROR setY(const LONG Value) noexcept {
      this->Y = Value;
      return ERR_Okay;
   }

   inline ERROR setTabSize(const LONG Value) noexcept {
      this->TabSize = Value;
      return ERR_Okay;
   }

   inline ERROR setWrapEdge(const LONG Value) noexcept {
      this->WrapEdge = Value;
      return ERR_Okay;
   }

   inline ERROR setFixedWidth(const LONG Value) noexcept {
      this->FixedWidth = Value;
      return ERR_Okay;
   }

   inline ERROR setHeight(const LONG Value) noexcept {
      if (this->initialised()) return ERR_NoFieldAccess;
      this->Height = Value;
      return ERR_Okay;
   }

   inline ERROR setMaxHeight(const LONG Value) noexcept {
      if (this->initialised()) return ERR_NoFieldAccess;
      this->MaxHeight = Value;
      return ERR_Okay;
   }

   inline ERROR setAlign(const ALIGN Value) noexcept {
      this->Align = Value;
      return ERR_Okay;
   }

   inline ERROR setAlignWidth(const LONG Value) noexcept {
      this->AlignWidth = Value;
      return ERR_Okay;
   }

   inline ERROR setAlignHeight(const LONG Value) noexcept {
      this->AlignHeight = Value;
      return ERR_Okay;
   }

   inline ERROR setEndX(const LONG Value) noexcept {
      this->EndX = Value;
      return ERR_Okay;
   }

   inline ERROR setEndY(const LONG Value) noexcept {
      this->EndY = Value;
      return ERR_Okay;
   }

   inline ERROR setBold(const LONG Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[20];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERROR setItalic(const LONG Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[5];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERROR setOpacity(const DOUBLE Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[18];
      return field->WriteValue(target, field, FD_DOUBLE, &Value, 1);
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
   LONG (*_CharWidth)(objFont * Font, ULONG Char, ULONG KChar, LONG * Kerning);
   ERROR (*_RefreshFonts)(void);
   ERROR (*_SelectFont)(CSTRING Name, CSTRING Style, CSTRING * Path, FMETA * Meta);
   ERROR (*_ResolveFamilyName)(CSTRING String, CSTRING * Result);
#endif // PARASOL_STATIC
};

#ifndef PRV_FONT_MODULE
#ifndef PARASOL_STATIC
extern struct FontBase *FontBase;
inline ERROR fntGetList(struct FontList ** Result) { return FontBase->_GetList(Result); }
inline LONG fntStringWidth(objFont * Font, CSTRING String, LONG Chars) { return FontBase->_StringWidth(Font,String,Chars); }
inline LONG fntCharWidth(objFont * Font, ULONG Char, ULONG KChar, LONG * Kerning) { return FontBase->_CharWidth(Font,Char,KChar,Kerning); }
inline ERROR fntRefreshFonts(void) { return FontBase->_RefreshFonts(); }
inline ERROR fntSelectFont(CSTRING Name, CSTRING Style, CSTRING * Path, FMETA * Meta) { return FontBase->_SelectFont(Name,Style,Path,Meta); }
inline ERROR fntResolveFamilyName(CSTRING String, CSTRING * Result) { return FontBase->_ResolveFamilyName(String,Result); }
#else
extern "C" {
extern ERROR fntGetList(struct FontList ** Result);
extern LONG fntStringWidth(objFont * Font, CSTRING String, LONG Chars);
extern LONG fntCharWidth(objFont * Font, ULONG Char, ULONG KChar, LONG * Kerning);
extern ERROR fntRefreshFonts(void);
extern ERROR fntSelectFont(CSTRING Name, CSTRING Style, CSTRING * Path, FMETA * Meta);
extern ERROR fntResolveFamilyName(CSTRING String, CSTRING * Result);
}
#endif // PARASOL_STATIC
#endif

