#pragma once

#include <parasol/main.h>

#include <ft2build.h>
#include <freetype/ftsizes.h>
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
            FT_Size ft_size = NULL;

            glyph & get_glyph(FT_Face, ULONG);

            // These values are measured in 72 DPI.
            // 
            // It is widely acknowledged that the metrics declared by font creators or their tools may not be 
            // the glyph metrics in reality...
            // 
            // Generally, descent() + ascent() == height() + 1
            // 
            // The FT_Face max_advance_height is completely unreliable in practice

            inline DOUBLE line_spacing() { 
               if FT_HAS_VERTICAL(ft_size->face) {
                  return std::trunc(int26p6_to_dbl(ft_size->face->max_advance_height) * (72.0 / DISPLAY_DPI)); 
               }
               else return std::trunc(int26p6_to_dbl(ft_size->metrics.height + std::abs(ft_size->metrics.descender)) * 72.0 / DISPLAY_DPI * 1.15);
            }

            // Full height from the baseline - INCLUDES ACCENTS
            inline DOUBLE height() { return int26p6_to_dbl(ft_size->metrics.height) * (72.0 / DISPLAY_DPI); }

            // Ascent from the baseline - DOES NOT INCLUDE ACCENTS.  Should match the font-size in pixels
            inline DOUBLE ascent() { return int26p6_to_dbl(ft_size->metrics.ascender) * 72.0 / DISPLAY_DPI; }

            inline DOUBLE descent() { return std::abs(int26p6_to_dbl(ft_size->metrics.descender)) * 72.0 / DISPLAY_DPI; }

            ft_point() : common_font(CF_FREETYPE) { }

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
   
      freetype_font()  { }
      freetype_font(FT_Face pFace) : face(pFace) { }

      ~freetype_font();
};

extern std::recursive_mutex glFontMutex;
extern std::unordered_map<ULONG, bmp_font> glBitmapFonts;
extern std::unordered_map<ULONG, freetype_font> glFreetypeFonts;

extern FT_Library glFTLibrary;
