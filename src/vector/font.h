#pragma once

#include <parasol/main.h>

#include <ft2build.h>
#include FT_SIZES_H
#include FT_MULTIPLE_MASTERS_H
#include FT_FREETYPE_H
#include FT_SFNT_NAMES_H

#define CF_BITMAP 0
#define CF_FREETYPE 1

class common_font {
public:
   LONG type;
   common_font(LONG pType) : type(pType) { }
};

//********************************************************************************************************************

struct CaseInsensitiveMap {
   bool operator() (const std::string &lhs, const std::string &rhs) const {
      return ::strcasecmp(lhs.c_str(), rhs.c_str()) < 0;
   }
};

//********************************************************************************************************************
// Fonts are stored independently of VectorText objects so that they can be permanently cached.

class bmp_font : public common_font {
public:
   objFont *font = nullptr;

   bmp_font() : common_font(CF_BITMAP) { }
   bmp_font(objFont *pFont) : font(pFont), common_font(CF_BITMAP) { }

   ~bmp_font() {
      if (font) { FreeResource(font); font = nullptr; }
   }
};

//********************************************************************************************************************
// Fonts are stored independently of VectorText objects so that they can be permanently cached.

class freetype_font {
   public:
      struct glyph {
         agg::path_storage path; // AGG vector path generated from the freetype glyph
         double adv_x, adv_y;    // Pixel advances, these values should not be rounded
         LONG   glyph_index;     // Freetype glyph index; saves having to call a function for conversion
      };

      using METRIC_GROUP = std::vector<FT_Fixed>;
      using GLYPH_TABLE = ankerl::unordered_dense::map<ULONG, glyph>; // Unicode to glyph lookup

      class ft_point : public common_font {
         public:
            GLYPH_TABLE glyphs;
            freetype_font *font = nullptr;
            FT_Size ft_size = nullptr;

            // These values are measured as pixels in 72 DPI.
            //
            // It is widely acknowledged that the metrics declared by font creators or their tools may not be
            // the precise glyph metrics in reality...

            double height;  // Full height from the baseline - including accents
            double ascent;  // Ascent from the baseline - not including accents.  Typically matches the font-size in pixels
            double descent; // Number of pixels allocated below the baseline, not including vertical whitespace
            double line_spacing;
            METRIC_GROUP axis;

            glyph & get_glyph(ULONG);

            ft_point() : common_font(CF_FREETYPE) { }

            ft_point(freetype_font &pFont, METRIC_GROUP &pMetrics, LONG pSize) : common_font(CF_FREETYPE) {
               font = &pFont;
               set_axis(pMetrics);
               FT_Set_Var_Design_Coordinates(pFont.face, axis.size(), axis.data());
               pFont.active_size = this;
               set_size(pSize);
            }

            ft_point(freetype_font &pFont, LONG pSize) : common_font(CF_FREETYPE) {
               font = &pFont;
               set_size(pSize);
            }

            ~ft_point() {
               // FT_Done_Face() will remove all FT_New_Size() allocations, interfering with
               // manual calls to FT_Done_Size()
               //
               //if (ft_size) { FT_Done_Size(ft_size); ft_size = NULL; }
            }

            void set_size(LONG Size) {
               if (!FT_New_Size(font->face, &ft_size)) {
                  FT_Activate_Size(ft_size);
                  FT_Set_Char_Size(font->face, 0, Size<<6, 72, 72);

                  if FT_HAS_VERTICAL(ft_size->face) {
                     line_spacing = std::trunc(int26p6_to_dbl(ft_size->face->max_advance_height) * (72.0 / glDisplayVDPI));
                  }
                  else line_spacing = std::trunc(int26p6_to_dbl(ft_size->metrics.height + std::abs(ft_size->metrics.descender)) * 72.0 / glDisplayVDPI * 1.15);

                  // Check if the client has applied a line spacing modifier for this font.

                  if (glFontConfig) {
                     pf::ScopedObjectLock<objConfig> config(glFontConfig, 500);
                     if (config.granted()) {
                        ConfigGroups *groups;
                        if (glFontConfig->get(FID_Data, groups) IS ERR::Okay) {
                           for (auto & [group, keys] : groups[0]) {
                              if (pf::iequals(group, font->face->family_name)) {
                                 if (auto it = keys.find("LineSpacing"); it != keys.end()) {
                                    line_spacing *= std::stod(it->second);
                                 }
                                 break;
                              }
                              else if (auto it = keys.find("Name"); it != keys.end()) {
                                 if (pf::iequals(it->second, font->face->family_name)) {
                                    if (auto it = keys.find("LineSpacing"); it != keys.end()) {
                                       line_spacing *= std::stod(it->second);
                                    }
                                    break;
                                 }
                              }
                           }
                        }
                     }
                  }

                  height  = int26p6_to_dbl(ft_size->metrics.height) * (72.0 / glDisplayVDPI);
                  ascent  = int26p6_to_dbl(ft_size->metrics.ascender) * (72.0 / glDisplayVDPI);
                  descent = std::abs(int26p6_to_dbl(ft_size->metrics.descender)) * (72.0 / glDisplayVDPI);
               }
            }

            void set_axis(METRIC_GROUP &metrics) {
               axis = metrics;
            }
      };

      using SIZE_CACHE = std::map<LONG, ft_point>; // font-size = glyph cache
      using STYLE_CACHE = std::map<std::string, SIZE_CACHE, CaseInsensitiveMap>;
      using METRIC_TABLE = std::map<std::string, METRIC_GROUP, CaseInsensitiveMap>;

   public:
      FT_Face face = nullptr;
      STYLE_CACHE style_cache; // Lists all known styles and contains the glyph cache for each style
      METRIC_TABLE metrics; // For variable fonts, these are pre-defined metrics with style names
      FMETA meta = FMETA::NIL;
      LONG glyph_flags = 0;
      ft_point *active_size = nullptr;

      freetype_font()  { }
      freetype_font(FT_Face pFace, STYLE_CACHE &pStyles, METRIC_TABLE &pMetrics, FMETA pMeta = FMETA::NIL)
         : face(pFace), style_cache(pStyles), metrics(pMetrics), meta(pMeta) {
         if ((pMeta & FMETA::HINT_INTERNAL) != FMETA::NIL) glyph_flags = FT_LOAD_TARGET_NORMAL|FT_LOAD_FORCE_AUTOHINT;
         else if ((pMeta & FMETA::HINT_LIGHT) != FMETA::NIL) glyph_flags = FT_LOAD_TARGET_LIGHT;
         else if ((pMeta & FMETA::HINT_NORMAL) != FMETA::NIL) glyph_flags = FT_LOAD_TARGET_NORMAL; // Use the font's hinting information
         else glyph_flags = FT_LOAD_DEFAULT; // Default, typically matches FT_LOAD_TARGET_NORMAL
      }

      ~freetype_font();
};

extern ERR get_font(pf::Log &Log, CSTRING, CSTRING, LONG, LONG, common_font **);

// Caching note: Although it is policy for cached fonts to be permanently retained, it is not necessary for the
// glyphs themselves to be permanently cached.  Future resource management should therefore actively remove
// glyphs that have gone stale.

extern std::recursive_mutex glFontMutex;
extern ankerl::unordered_dense::map<ULONG, std::unique_ptr<bmp_font>> glBitmapFonts;
extern ankerl::unordered_dense::map<ULONG, std::unique_ptr<freetype_font>> glFreetypeFonts;

extern FT_Library glFTLibrary;
