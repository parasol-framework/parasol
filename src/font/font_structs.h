
#define CHAR_TAB     (0x09)
#define CHAR_ENTER   (10)
#define CHAR_SPACE   '.'     // Character to use for determining the size of a space
#define FT_DOWNSIZE  6

#ifndef PI
#define PI 3.1415926535897932384626433832795
#endif

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

/*****************************************************************************
** Vector font cache
*/

struct glyph_bitmap {
   UBYTE *Data;
   UWORD Width, Height;
   WORD  Top, Left;
   WORD  AdvanceX, AdvanceY;

   UBYTE *Outline;
   UWORD OutlineWidth, OutlineHeight, OutlineTop, OutlineLeft;
};

struct font_glyph { // See glyph_cache.Glyphs
   ULONG Unicode;        // Unicode character represented by the glyph
   ULONG Count;          // Number of times that the glyph has been used
   ULONG GlyphIndex;     // Freetype glyph index
   struct glyph_bitmap Char;
};

struct glyph_cache { // Represents a set of glyphs (at different point sizes) for a font face.
   struct glyph_cache *Next;   // Next glyph array in the cache
   struct glyph_cache *Prev;   // Previous glyph array in the cache
   struct KeyStore *Glyphs;    // Glyph storage - see font_glyph
   LONG FontSize;              // Nominal size for the glyphs in this cache
   LONG Usage;                 // Counter for usage
   struct FontCharacter Chars[256]; // Pre-calculated glyph widths and advances for most Latin characters.
};

struct font_cache { // Represents a font face.
   struct glyph_cache *Glyphs;    // List of glyphs for this font, at various sizes
   struct glyph_cache *LastGlyph;
   STRING Path;                   // Path to the font source
   FT_Face Face;                  // Truetype font face
   LONG Usage;                    // Counter for usage
   LONG CurrentSize;
};
