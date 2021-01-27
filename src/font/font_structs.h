
#include <unordered_set>
#include <map>

#define CHAR_TAB     (0x09)
#define CHAR_ENTER   (10)
#define CHAR_SPACE   '.'     // Character to use for determining the size of a space
#define FT_DOWNSIZE  6
#define MAX_GLYPHS 256 // Maximum number of glyph bitmaps to cache
#define FIXED_DPI 96 // FreeType measurements are based on this DPI.

#ifndef PI
#define PI 3.1415926535897932384626433832795
#endif

INLINE FT_F26Dot6 DBL_TO_FT(DOUBLE Value)
{
  return F2T(Value * 64.0);
}

struct FontCharacter {
   WORD Width;
   WORD Advance;
   UWORD Offset;
   UWORD OutlineOffset;
};

/******************************************************************************
** Win32 font structures
*/

struct winFontList {
   LONG Offset, Size, Point;
};

struct winmz_header_fields {
   UWORD magic;
   UBYTE data[29 * 2];
   ULONG lfanew;
};

struct winne_header_fields {
   UWORD magic;
   UBYTE data[34];
   UWORD resource_tab_offset;
   UWORD rname_tab_offset;
};

struct winfnt_header_fields {
   UWORD version;
   ULONG file_size;
   char copyright[60];
   UWORD file_type;
   UWORD nominal_point_size;     // Point size
   UWORD vertical_resolution;
   UWORD horizontal_resolution;
   UWORD ascent;                 // The amount of pixels above the base-line
   UWORD internal_leading;       // top leading pixels
   UWORD external_leading;       // gutter
   BYTE  italic;                 // TRUE if font is italic
   BYTE  underline;              // TRUE if font is underlined
   BYTE  strike_out;             // TRUE if font is striked-out
   UWORD weight;                 // Indicates font boldness
   BYTE  charset;
   UWORD pixel_width;
   UWORD pixel_height;
   BYTE  pitch_and_family;
   UWORD avg_width;
   UWORD max_width;
   UBYTE first_char;
   UBYTE last_char;
   UBYTE default_char;
   UBYTE break_char;
   UWORD bytes_per_row;
   ULONG device_offset;
   ULONG face_name_offset;
   ULONG bits_pointer;
   ULONG bits_offset;
   BYTE  reserved;
   ULONG flags;
   UWORD A_space;
   UWORD B_space;
   UWORD C_space;
   UWORD color_table_offset;
   BYTE  reservedend[4];
} __attribute__((packed));

#define ID_WINMZ  0x5A4D
#define ID_WINNE  0x454E

/******************************************************************************
** Structure definition for cached bitmap fonts.
*/

struct BitmapCache {
   struct BitmapCache *Next;
   UBYTE *Data;
   UBYTE *Outline;
   struct winfnt_header_fields Header;
   struct FontCharacter Chars[256];
   BYTE Location[200];
   WORD OpenCount;
   LONG StyleFlags;
};

static struct BitmapCache *glBitmapCache = NULL;

//****************************************************************************
// Truetype rendered font cache

class font_glyph {
public:
   ULONG Count;          // Number of times that the glyph has been used
   ULONG GlyphIndex;     // Freetype glyph index
   UBYTE *Data;
   UBYTE *Outline;
   UWORD Width, Height;
   WORD  Top, Left;
   WORD  AdvanceX, AdvanceY;
   UWORD OutlineWidth, OutlineHeight, OutlineTop, OutlineLeft;

   font_glyph() {
      Data       = NULL;
      Outline    = NULL;
      Count      = 0;
      GlyphIndex = 0;
   }
};

class glyph_cache { // Represents a set of glyphs at a set point-size for a font face.
public:
   LONG Usage;    // Counter for usage of the typeface at this specific point size
   DOUBLE Point;
   FT_Size Size;  // Freetype size structure
   struct FontCharacter Chars[256]; // Pre-calculated glyph widths and advances for most Latin characters.
   std::unordered_map<ULONG, font_glyph> Glyphs; // Size limited by MAX_GLYPHS

   glyph_cache(FT_Face &pFace, DOUBLE pPoint, unsigned char pDefaultChar) {
      Usage = 0;
      Point = pPoint;

      // Once the FT_Size reference is configured, all one has to do is call FT_Activate_Size() to switch to it.

      FT_New_Size(pFace, &Size);
      FT_Activate_Size(Size);
      FT_Set_Char_Size(pFace, 0, DBL_TO_FT(pPoint), FIXED_DPI, FIXED_DPI); // The Point is pre-scaled, so we use FIXED_DPI here.

      // Pre-calculate the width of each character in the range of 0x20 - 0xff

      if (!FT_Load_Glyph(pFace, FT_Get_Char_Index(pFace, pDefaultChar), FT_LOAD_DEFAULT)) {
         Chars[(LONG)pDefaultChar].Width   = pFace->glyph->advance.x>>FT_DOWNSIZE;
         Chars[(LONG)pDefaultChar].Advance = pFace->glyph->advance.x>>FT_DOWNSIZE;
      }

      for (LONG i=' '; i < ARRAYSIZE(Chars); i++) {
         LONG j;
         if ((j = FT_Get_Char_Index(pFace, i)) and (!FT_Load_Glyph(pFace, j, FT_LOAD_DEFAULT))) {
            Chars[i].Width   = pFace->glyph->advance.x>>FT_DOWNSIZE;
            Chars[i].Advance = pFace->glyph->advance.x>>FT_DOWNSIZE;
         }
         else {
            Chars[i].Width   = Chars[(LONG)pDefaultChar].Width;
            Chars[i].Advance = Chars[(LONG)pDefaultChar].Advance;
         }
      }
   }

   ~glyph_cache() {
      for (const auto & [ unicode, fg ] : Glyphs) {
         if (fg.Data) FreeResource(fg.Data);
         if (fg.Outline) FreeResource(fg.Outline);
      }
      FT_Done_Size(Size);
   }
};

class font_cache { // Represents a font face.  Stored in glCache
public:
   std::unordered_map<DOUBLE, glyph_cache> Glyphs; // <Size, glyph_cache>
   std::string Path;       // Path to the font source
   FT_Face Face;           // Truetype font face
   LONG    Usage;          // Counter for usage of the typeface

   font_cache(const std::string &pPath, FT_Face &pFace) {
      Path = pPath;
      Face = pFace;
      Usage = 0;
   }
};
