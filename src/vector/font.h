#pragma once

#include <parasol/main.h>

#include <ft2build.h>
#include <freetype/ftsizes.h>
#include <freetype/ftmm.h>
#include FT_FREETYPE_H

#define CF_BITMAP 0
#define CF_FREETYPE 1

class common_font {
public:
   LONG type;
   common_font(LONG pType) : type(pType) { }
};

//********************************************************************************************************************
// Fonts are stored independently of VectorText objects so that they can be permanently cached.

class bmp_font : public common_font {
public:
   objFont *font = NULL;

   bmp_font() : common_font(CF_BITMAP) { }
   bmp_font(objFont *pFont) : font(pFont), common_font(CF_BITMAP) { }

   ~bmp_font() {
      if (font) { FreeResource(font); font = NULL; }
   }
};

//********************************************************************************************************************
// Fonts are stored independently of VectorText objects so that they can be permanently cached.

class freetype_font {
   public:
      class glyph {
         public:
            agg::path_storage path;
            DOUBLE advance_x;
            DOUBLE advance_y;
            LONG   glyph_index;
      };

      using GLYPH_TABLE = std::unordered_map<ULONG, glyph>;

      class ft_point : public common_font {
         public:
            GLYPH_TABLE glyphs;
            freetype_font *font = NULL;
            FT_Size ft_size = NULL;

            // These values are measured as pixels in 72 DPI.
            // 
            // It is widely acknowledged that the metrics declared by font creators or their tools may not be 
            // the precise glyph metrics in reality...

            DOUBLE height;  // Full height from the baseline - including accents
            DOUBLE ascent;  // Ascent from the baseline - not including accents.  Typically matches the font-size in pixels
            DOUBLE descent; // Number of pixels allocated below the baseline, not including vertical whitespace
            DOUBLE line_spacing;

            glyph & get_glyph(ULONG);

            ft_point() : common_font(CF_FREETYPE) { }

            ft_point(freetype_font &pFont, LONG Size) : common_font(CF_FREETYPE) {
               font = &pFont;
               if (!FT_New_Size(pFont.face, &ft_size)) {
                  FT_Activate_Size(ft_size);
                  FT_Set_Char_Size(pFont.face, 0, Size<<6, 72, 72);

                  if FT_HAS_VERTICAL(ft_size->face) {
                     line_spacing = std::trunc(int26p6_to_dbl(ft_size->face->max_advance_height) * (72.0 / DISPLAY_DPI));
                  }
                  else line_spacing = std::trunc(int26p6_to_dbl(ft_size->metrics.height + std::abs(ft_size->metrics.descender)) * 72.0 / DISPLAY_DPI * 1.15);
                  
                  height  = int26p6_to_dbl(ft_size->metrics.height) * (72.0 / DISPLAY_DPI);
                  ascent  = int26p6_to_dbl(ft_size->metrics.ascender) * (72.0 / DISPLAY_DPI);
                  descent = std::abs(int26p6_to_dbl(ft_size->metrics.descender)) * (72.0 / DISPLAY_DPI);
               }
            }

            ~ft_point() {
               // FT_Done_Face() will remove all FT_New_Size() allocations, interfering with
               // manual calls to FT_Done_Size()
               // 
               //if (ft_size) { FT_Done_Size(ft_size); ft_size = NULL; }
            }
      };

   public:
      FT_Face face = NULL;
      std::map<LONG, ft_point> points;
      FMETA meta = FMETA::NIL;
      LONG glyph_flags = 0;
   
      freetype_font()  { }
      freetype_font(FT_Face pFace, FMETA pMeta = FMETA::NIL) : face(pFace), meta(pMeta) { 
         if ((pMeta & FMETA::HINT_INTERNAL) != FMETA::NIL) glyph_flags = FT_LOAD_TARGET_NORMAL|FT_LOAD_FORCE_AUTOHINT;
         else if ((pMeta & FMETA::HINT_LIGHT) != FMETA::NIL) glyph_flags = FT_LOAD_TARGET_LIGHT;
         else if ((pMeta & FMETA::HINT_NORMAL) != FMETA::NIL) glyph_flags = FT_LOAD_TARGET_NORMAL; // Use the font's hinting information
         else glyph_flags = FT_LOAD_DEFAULT; // Default, typically matches FT_LOAD_TARGET_NORMAL
      }

      ~freetype_font();
};

// Caching note: Although it is policy for cached fonts to be permanently retained, it is not necessary for the
// glyphs themselves to be permanently cached.  Future resource management should therefore actively remove 
// glyphs that have gone stale.

extern std::recursive_mutex glFontMutex;
extern std::unordered_map<ULONG, bmp_font> glBitmapFonts;
extern std::unordered_map<ULONG, freetype_font> glFreetypeFonts;

extern FT_Library glFTLibrary;
