/*********************************************************************************************************************

The source code of the Parasol project is made publicly available under the terms described in the LICENSE.TXT file
that is distributed with this package.  Please refer to it for further information on licensing.

This code utilises the work of the FreeType Project under the FreeType License.  For more information please refer to
the FreeType home page at www.freetype.org.

**********************************************************************************************************************

-MODULE-
Font: Provides font management functionality and hosts the Font class.

-END-

*********************************************************************************************************************/

#define PRV_FONT
#define PRV_FONT_MODULE
//#define DEBUG

#include <ft2build.h>
#include <freetype/ftsizes.h>
#include FT_FREETYPE_H
#include FT_STROKER_H

#include <parasol/main.h>

#include <parasol/modules/xml.h>
#include <parasol/modules/picture.h>
#include <parasol/modules/font.h>
#include <parasol/modules/display.h>

#include <math.h>
#include <wchar.h>
#include <parasol/strings.hpp>

/*********************************************************************************************************************
** This table determines what ASCII characters are treated as white-space for word-wrapping purposes.  You'll need to
** refer to an ASCII table to see what is going on here.
*/

static const UBYTE glWrapBreaks[256] = {
   1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 0x0f
   1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 0x1f
   1, 0, 0, 1, 1, 1, 1, 0, 0, 1, 1, 1, 1, 1, 1, 1, // 0x2f
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, // 0x3f
   1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0x4f
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, // 0x5f
   1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0x6f
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, // 0x7f
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0x8f
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0x9f
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0xaf
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0xbf
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0xcf
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0xdf
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0xef
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0  // 0xff
};

//********************************************************************************************************************

OBJECTPTR modFont = NULL;
struct CoreBase *CoreBase;
struct DisplayBase *DisplayBase;
static OBJECTPTR clFont = NULL;
static OBJECTPTR modDisplay = NULL;
static FT_Library glFTLibrary = NULL;

#include "font_structs.h"

static LONG glDisplayVDPI = FIXED_DPI; // Initially matches the fixed DPI value, can change if display has a high DPI setting.
static LONG glDisplayHDPI = FIXED_DPI;

class extFont : public objFont {
   public:
   WORD *prvTabs;                // Array of tab stops
   UBYTE *prvData;
   std::shared_ptr<font_cache> Cache;     // Reference to the Truetype font that is in use
   struct FontCharacter *prvChar;
   struct BitmapCache   *BmpCache;
   struct font_glyph    prvTempGlyph;
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
};

#include "font_def.c"

#include "font_bitmap.cpp"

static ERROR add_font_class(void);
static LONG getutf8(CSTRING, ULONG *);
static LONG get_kerning(FT_Face, LONG Glyph, LONG PrevGlyph);
static font_glyph * get_glyph(extFont *, ULONG, bool);
static void unload_glyph_cache(extFont *);
static void scan_truetype_folder(objConfig *);
static void scan_fixed_folder(objConfig *);
static ERROR analyse_bmp_font(CSTRING, winfnt_header_fields *, STRING *, UBYTE *, UBYTE);
static ERROR  fntRefreshFonts(void);

//********************************************************************************************************************
// Return the first unicode value from a given string address.

static LONG getutf8(CSTRING Value, ULONG *Unicode)
{
   LONG i, len, code;

   if ((*Value & 0x80) != 0x80) {
      if (Unicode) *Unicode = *Value;
      return 1;
   }
   else if ((*Value & 0xe0) IS 0xc0) {
      len  = 2;
      code = *Value & 0x1f;
   }
   else if ((*Value & 0xf0) IS 0xe0) {
      len  = 3;
      code = *Value & 0x0f;
   }
   else if ((*Value & 0xf8) IS 0xf0) {
      len  = 4;
      code = *Value & 0x07;
   }
   else if ((*Value & 0xfc) IS 0xf8) {
      len  = 5;
      code = *Value & 0x03;
   }
   else if ((*Value & 0xfc) IS 0xfc) {
      len  = 6;
      code = *Value & 0x01;
   }
   else {
      // Unprintable character
      if (Unicode) *Unicode = 0;
      return 1;
   }

   for (i=1; i < len; ++i) {
      if ((Value[i] & 0xc0) != 0x80) code = -1;
      code <<= 6;
      code |= Value[i] & 0x3f;
   }

   if (code IS -1) {
      if (Unicode) *Unicode = 0;
      return 1;
   }
   else {
      if (Unicode) *Unicode = code;
      return len;
   }
}

//********************************************************************************************************************
// Returns the global point size for font scaling.  This is set to 10 by default, but the user can change the setting
// in the interface style values.

static DOUBLE glDefaultPoint = 10;
static bool glPointSet = false;

static DOUBLE global_point_size(void)
{
   if (!glPointSet) {
      pf::Log log(__FUNCTION__);
      OBJECTID style_id;
      if (!FindObject("glStyle", ID_XML, FOF::NIL, &style_id)) {
         pf::ScopedObjectLock<objXML> style(style_id, 3000);
         if (style.granted()) {
            char fontsize[20];
            glPointSet = true;
            if (!acGetVar(style.obj, "/interface/@fontsize", fontsize, sizeof(fontsize))) {
               glDefaultPoint = StrToFloat(fontsize);
               if (glDefaultPoint < 6) glDefaultPoint = 6;
               else if (glDefaultPoint > 80) glDefaultPoint = 80;
               log.msg("Global font size is %.1f.", glDefaultPoint);
            }
         }
      }
      else log.warning("glStyle XML object is not available");
   }

   return glDefaultPoint;
}

//********************************************************************************************************************
// Attempts to update globally held DPI values with the main display's real DPI.
//

static void update_dpi(void)
{
   static LARGE last_update = 0;
   LARGE current_time = PreciseTime();

   if (current_time - last_update > 3000000LL) {
      DISPLAYINFO *display;
      if (!gfxGetDisplayInfo(0, &display)) {
         last_update = PreciseTime();
         if ((display->VDensity >= 96) and (display->HDensity >= 96)) {
            glDisplayVDPI = display->VDensity;
            glDisplayHDPI = display->HDensity;
            //glDisplayDPI = (glDisplayVDPI + glDisplayHDPI) / 2; // Get the average DPI in case the pixels aren't square.  Note: Average DPI beats diagonal DPI every time.
         }
      }
   }
}

//********************************************************************************************************************
// Only call this function if the font includes kerning support (test via FTF_KERNING).

INLINE void get_kerning_xy(FT_Face Face, LONG Glyph, LONG PrevGlyph, LONG *X, LONG *Y)
{
   FT_Vector delta;
   FT_Get_Kerning(Face, PrevGlyph, Glyph, FT_KERNING_DEFAULT, &delta);
   *X = delta.x>>FT_DOWNSIZE;
   *Y = delta.y>>FT_DOWNSIZE;
}

INLINE LONG get_kerning(FT_Face Face, LONG Glyph, LONG PrevGlyph)
{
   if ((!Glyph) or (!PrevGlyph)) return 0;

   FT_Vector delta;
   FT_Get_Kerning(Face, PrevGlyph, Glyph, FT_KERNING_DEFAULT, &delta);
   return delta.x>>FT_DOWNSIZE;
}

//********************************************************************************************************************

INLINE void calc_lines(extFont *Self)
{
   if (Self->String) {
      if (Self->Flags & FTF_CHAR_CLIP) {
         fntStringSize(Self, Self->String, -1, 0, NULL, &Self->prvLineCount);
      }
      else if (Self->WrapEdge > 0) {
         fntStringSize(Self, Self->String, -1, Self->WrapEdge - Self->X, NULL, &Self->prvLineCount);
      }
      else Self->prvLineCount = Self->prvLineCountCR;
   }
   else Self->prvLineCount = 1;
}

//********************************************************************************************************************

static objConfig *glConfig = NULL;

static ERROR CMDInit(OBJECTPTR argModule, struct CoreBase *argCoreBase)
{
   pf::Log log;

   CoreBase = argCoreBase;

   argModule->getPtr(FID_Root, &modFont);

   if (objModule::load("display", MODVERSION_DISPLAY, &modDisplay, &DisplayBase) != ERR_Okay) return ERR_InitModule;

   if (FT_Init_FreeType(&glFTLibrary)) {
      log.warning("Failed to initialise the FreeType font library.");
      return ERR_Failed;
   }

   LOC type;
   bool refresh = (AnalysePath("fonts:fonts.cfg", &type) != ERR_Okay) or (type != LOC::FILE);

   if ((glConfig = objConfig::create::global(fl::Name("cfgSystemFonts"), fl::Path("fonts:fonts.cfg")))) {
      if (refresh) fntRefreshFonts();

      ConfigGroups *groups;
      if (not ((!glConfig->getPtr(FID_Data, &groups)) and (groups->size() > 0))) {
         log.error("No system fonts are available for use.");
         return ERR_Failed;
      }
   }
   else {
      log.error("Failed to load or prepare the font configuration file.");
      return ERR_Failed;
   }

   return add_font_class();
}

static ERROR CMDOpen(OBJECTPTR Module)
{
   Module->set(FID_FunctionList, glFunctions);
   return ERR_Okay;
}

static ERROR CMDExpunge(void)
{
   if (glCacheTimer) { UpdateTimer(glCacheTimer, 0); glCacheTimer = NULL; }

   if (glFTLibrary) { FT_Done_FreeType(glFTLibrary); glFTLibrary = NULL; }
   if (glConfig)    { FreeResource(glConfig); glConfig = NULL; }

   if (clFont)     { FreeResource(clFont);     clFont = NULL; }
   if (modDisplay) { FreeResource(modDisplay); modDisplay = NULL; }

   glBitmapCache.clear();

   return ERR_Okay;
}

/*********************************************************************************************************************

-FUNCTION-
CharWidth: Returns the width of a character.

This function will return the pixel width of a font character.  The character is specified as a unicode value in the
Char parameter. Kerning values can also be returned, which affect the position of the character along the horizontal.
The previous character in the word is set in KChar and the kerning value will be returned in the Kerning parameter.  If
kerning information is not required, set the KChar and Kerning parameters to zero.

-INPUT-
ext(Font) Font: The font to use for calculating the character width.
uint Char: A unicode character.
uint KChar: A unicode character to use for calculating the font kerning (optional).
&int Kerning: The resulting kerning value (optional).

-RESULT-
int: The pixel width of the character will be returned.

*********************************************************************************************************************/

static LONG fntCharWidth(extFont *Font, ULONG Char, ULONG KChar, LONG *Kerning)
{
   if (Kerning) *Kerning = 0;

   if (Font->FixedWidth > 0) return Font->FixedWidth;
   else if (Font->Flags & FTF_SCALABLE) {
      font_glyph *cache;
      if ((cache = get_glyph(Font, Char, false))) {
         if ((Font->Flags & FTF_KERNING) and (KChar) and (Kerning)) {
            LONG kglyph = FT_Get_Char_Index(Font->Cache->Face, KChar);
            *Kerning = get_kerning(Font->Cache->Face, cache->GlyphIndex, kglyph);
         }
         return cache->AdvanceX + Font->GlyphSpacing;
      }
      else {
         pf::Log log(__FUNCTION__);
         log.trace("No glyph for character %u", Char);
         if (Font->prvChar) return Font->prvChar[(LONG)Font->prvDefaultChar].Advance;
         else return 0;
      }
   }
   else if (Char < 256) return Font->prvChar[Char].Advance;
   else {
      pf::Log log(__FUNCTION__);
      log.traceWarning("Character %u out of range.", Char);
      return Font->prvChar[(LONG)Font->prvDefaultChar].Advance;
   }
}

/*********************************************************************************************************************

-FUNCTION-
GetList: Returns a list of all available system fonts.

This function returns a linked list of all available system fonts.

-INPUT-
&!struct(FontList) Result: The font list is returned here.

-ERRORS-
Okay
NullArgs
AccessObject: Font configuration information could not be accessed.

*********************************************************************************************************************/

static ERROR fntGetList(FontList **Result)
{
   pf::Log log(__FUNCTION__);

   if (!Result) return log.warning(ERR_NullArgs);

   log.branch();

   *Result = NULL;

   pf::ScopedObjectLock<objConfig> config(glConfig, 3000);
   if (!config.granted()) return log.warning(ERR_AccessObject);

   size_t size = 0;
   ConfigGroups *groups;
   if (!glConfig->getPtr(FID_Data, &groups)) {
      for (auto & [group, keys] : groups[0]) {
         size += sizeof(FontList) + keys["Name"].size() + 1 + keys["Styles"].size() + 1 + (keys["Points"].size()*4) + 1;
      }

      FontList *list, *last_list = NULL;
      if (!AllocMemory(size, MEM_DATA, &list)) {
         STRING buffer = (STRING)(list + groups->size());
         *Result = list;

         for (auto & [group, keys] : groups[0]) {
            last_list = list;
            list->Next = list + 1;

            if (keys.contains("Name")) {
               list->Name = buffer;
               buffer += StrCopy(keys["Name"].c_str(), buffer) + 1;
            }

            if (keys.contains("Styles")) {
               list->Styles = buffer;
               buffer += StrCopy(keys["Styles"].c_str(), buffer) + 1;
            }

            if (keys.contains("Scalable")) {
               if (!StrCompare("Yes", keys["Scalable"].c_str(), 0, STR::MATCH_LEN)) list->Scalable = TRUE;
            }

            list->Points = NULL;
            if (keys.contains("Points")) {
               CSTRING fontpoints = keys["Points"].c_str();
               if (*fontpoints) {
                  list->Points = (LONG *)buffer;
                  for (WORD j=0; *fontpoints; j++) {
                     ((LONG *)buffer)[0] = StrToInt(fontpoints);
                     buffer += sizeof(LONG);
                     while ((*fontpoints) and (*fontpoints != ',')) fontpoints++;
                     if (*fontpoints IS ',') fontpoints++;
                  }
                  ((LONG *)buffer)[0] = 0;
                  buffer += sizeof(LONG);
               }
            }

            list++;
         }

         if (last_list) last_list->Next = NULL;

         return ERR_Okay;
      }
      else return ERR_AllocMemory;
   }
   else return ERR_NoData;
}

/*********************************************************************************************************************

-FUNCTION-
StringSize: Calculates the exact dimensions of a font string, giving respect to word wrapping.

This function calculates the width and height of a String (in pixels and rows respectively).  It takes into account the
font object's current settings and accepts a boundary in the Wrap argument for calculating word wrapping.  The routine
takes into account any line feeds that may already exist in the String.

A character limit can be specified in the Chars argument.  If this argument is set to FSS_ALL, all characters in String
will be used in the calculation.  If set to FSS_LINE, the routine will terminate when the first line feed or word-wrap
is encountered and the Rows value will reflect the byte position of the word at which the wrapping boundary was
encountered.

-INPUT-
ext(Font) Font: An initialised font object.
cstr String: The string to be analysed.
int(FSS) Chars:  The number of characters (not bytes, so consider UTF-8 serialisation) to be used in calculating the string length.  FSS constants can also be used here.
int Wrap:   The pixel position at which word wrapping occurs.  If zero or less, wordwrap is disabled.
&int Width: The width of the longest line will be returned in this parameter.
&int Rows:  The number of calculated rows will be returned in this parameter.

*********************************************************************************************************************/

static void fntStringSize(extFont *Font, CSTRING String, LONG Chars, LONG Wrap, LONG *Width, LONG *Rows)
{
   font_glyph *cache;
   ULONG unicode;
   WORD rowcount, wordwidth, lastword, tabwidth, charwidth;
   UBYTE line_abort, pchar;

   if ((!Font) or (!String)) return;
   if (!Font->initialised()) return;

   if (Chars IS FSS_LINE) {
      Chars = 0x7fffffff;
      line_abort = 1;
   }
   else {
      line_abort = 0;
      if (Chars < 0) Chars = 0x7fffffff;
   }

   if ((Wrap <= 0) or (Font->Flags & FTF_CHAR_CLIP)) Wrap = 0x7fffffff;

   //log.msg("StringSize: %.10s, Wrap %d, Chars %d, Abort: %d", String, Wrap, Chars, line_abort);

   CSTRING start  = String;
   LONG x         = 0;
   LONG prevglyph = 0;
   if (line_abort) rowcount = 0;
   else rowcount  = 1;
   LONG longest   = 0;
   LONG charcount = 0;
   LONG wordindex = 0;
   while ((*String) and (charcount < Chars)) {
      lastword = x;

      // Skip whitespace

      while ((*String) and (*String <= 0x20)) {
         if (*String IS ' ') x += Font->prvChar[' '].Advance + Font->GlyphSpacing;
         else if (*String IS '\t') {
            tabwidth = (Font->prvChar[' '].Advance + Font->GlyphSpacing) * Font->TabSize;
            if (tabwidth) x += ROUNDUP(x, tabwidth);
         }
         else if (*String IS '\n') {
            if (lastword > longest) longest = lastword;
            x = 0;
            if (line_abort) {
               line_abort = 2;
               String++;
               break;
            }
            rowcount++;
         }
         String++;
         charcount++;
         prevglyph = 0;
      }

      if ((!*String) or (line_abort IS 2)) break;

      // Calculate the width of the discovered word

      wordindex = (LONG)(String - start);
      wordwidth = 0;
      charwidth = 0;

      while (charcount < Chars) {
         LONG charlen = getutf8(String, &unicode);

         if (Font->FixedWidth > 0) charwidth = Font->FixedWidth;
         else if (Font->Flags & FTF_SCALABLE) {
            if (unicode IS ' ') {
               charwidth += Font->prvChar[' '].Advance + Font->GlyphSpacing;
            }
            else if ((cache = get_glyph(Font, unicode, false))) {
               charwidth = cache->AdvanceX + Font->GlyphSpacing;
               if (Font->Flags & FTF_KERNING) charwidth += get_kerning(Font->Cache->Face, cache->GlyphIndex, prevglyph); // Kerning adjustment
               prevglyph = cache->GlyphIndex;
            }
         }
         else if (unicode < 256) charwidth = Font->prvChar[unicode].Advance + Font->GlyphSpacing;
         else charwidth = Font->prvChar[(LONG)Font->prvDefaultChar].Advance + Font->GlyphSpacing;

         if ((!x) and (!(Font->Flags & FTF_CHAR_CLIP)) and (x+wordwidth+charwidth >= Wrap)) {
            // This is the first word of the line and it exceeds the boundary, so we have to split it.

            lastword = wordwidth;
            wordwidth += charwidth; // This is just to ensure that a break occurs
            wordindex = (LONG)(String - start);
            break;
         }
         else {
            pchar = glWrapBreaks[(UBYTE)(*String)];
            wordwidth += charwidth;
            String += charlen;
            charcount++;

            // Break if the previous char was a wrap character or current char is whitespace.

            if ((pchar) or (*String <= 0x20)) break;
         }
      }

      // Check the width of the word against the wrap boundary

      if (x + wordwidth >= Wrap) {
         prevglyph = 0;
         if (lastword > longest) longest = lastword;
         rowcount++;
         if (line_abort) {
            x = 0;
            String = start + wordindex;
            break;
         }
         else x = wordwidth;
      }
      else x += wordwidth;
   }

   if (x > longest) longest = x;

   if (Rows) {
      if (line_abort) *Rows = (LONG)(String - start);
      else *Rows = rowcount;
   }

   if (Width) *Width = longest;
}

/*********************************************************************************************************************

-FUNCTION-
FreetypeHandle: Returns a handle to the FreeType library.

This function returns a direct handle to the internal FreeType library.  It is intended that this handle should only be
used by existing projects that are based on FreeType and need access to its functionality.  References to FreeType
functions can be obtained by loading the Font module and then calling the ResolveSymbol method to retrieve function
names, e.g. "FT_Open_Face".

-RESULT-
ptr: A handle to the FreeType library will be returned.

*********************************************************************************************************************/

static APTR fntFreetypeHandle(void)
{
   return glFTLibrary;
}

/*********************************************************************************************************************

-FUNCTION-
StringWidth: Returns the pixel width of any given string in relation to a font's settings.

This function calculates the pixel width of any string in relation to a font's object definition.  The routine takes
into account any line feeds that might be specified in the String, so if the String contains 8 lines, then the width
of the longest line will be returned.

Word wrapping will not be taken into account, even if it has been enabled in the font object.

-INPUT-
ext(Font) Font: An initialised font object.
cstr String: The string to be calculated.
int Chars: The number of characters (not bytes, so consider UTF-8 serialisation) to be used in calculating the string length, or -1 to use the entire string.

-RESULT-
int: The pixel width of the string is returned - this will be zero if there was an error or the string is empty.

*********************************************************************************************************************/

static LONG fntStringWidth(extFont *Font, CSTRING String, LONG Chars)
{
   if ((!Font) or (!String)) return 0;
   if (!Font->initialised()) return 0;

   font_glyph *cache;

   CSTRING str = String;
   if (Chars < 0) Chars = 0x7fffffff;

   LONG len     = 0;
   LONG lastlen = 0;
   LONG prevglyph = 0;
   while ((*str) and (Chars > 0)) {
      if (*str IS '\n') {
         if (lastlen < len) lastlen = len; // Compare lengths
         len  = 0; // Reset
         str++;
         Chars--;
      }
      else if (*str IS '\t') {
         WORD tabwidth = (Font->prvChar[' '].Advance + Font->GlyphSpacing) * Font->TabSize;
         if (tabwidth) len = ROUNDUP(len, tabwidth);
         str++;
         Chars--;
      }
      else {
         ULONG unicode;
         str += getutf8(str, &unicode);
         Chars--;

         if (Font->FixedWidth > 0) len += Font->FixedWidth + Font->GlyphSpacing;
         else if (Font->Flags & FTF_SCALABLE) {
            if ((unicode < 256) and (Font->prvChar[unicode].Advance) and (!(Font->Flags & FTF_KERNING))) {
               len += Font->prvChar[unicode].Advance + Font->GlyphSpacing;
            }
            else if (unicode IS ' ') {
               len += Font->prvChar[' '].Advance + Font->GlyphSpacing;
            }
            else if ((cache = get_glyph(Font, unicode, false))) {
               len += cache->AdvanceX + Font->GlyphSpacing;
               if (Font->Flags & FTF_KERNING) len += get_kerning(Font->Cache->Face, cache->GlyphIndex, prevglyph);
               prevglyph = cache->GlyphIndex;
            }
         }
         else if ((unicode < 256) and (Font->prvChar[unicode].Advance)) {
            len += Font->prvChar[unicode].Advance + Font->GlyphSpacing;
         }
         else len += Font->prvChar[(LONG)Font->prvDefaultChar].Advance + Font->GlyphSpacing;
      }
   }

   if (lastlen > len) return lastlen - Font->GlyphSpacing;
   else if (len > 0) return len - Font->GlyphSpacing;
   else return 0;
}

/*********************************************************************************************************************

-FUNCTION-
ConvertCoords: Converts pixel coordinates into equivalent column and row positions in font strings.

This function is used to convert pixel coordinates within a font String into the equivalent Row and Column character
positions.  If the coordinate values that are supplied are in excess of the String dimensions, the Column and Row results
will be automatically restricted to their maximum value.  For instance, if the Y argument is set to 280 and the
String consists of 15 rows amounting to 150 pixels in height, the Row value will be returned as 15.

Negative coordinate values are not permitted.

-INPUT-
ext(Font) Font: An initialised font object.
cstr String: Either point to a string for inspection or set to NULL to inspect the string currently in the font's String field.
int X:       The horizontal coordinate to translate into a column position.
int Y:       The vertical coordinate to translate into a row position.
&int Column: This result parameter will be updated to reflect the calculated character position, with consideration to the UTF-8 standard.  May be NULL if not required.
&int Row:    This result parameter will be updated to reflect the calculated row position.  May be NULL if not required.
&int ByteColumn: This result parameter will be updated to reflect the absolute column byte position within the given row.  May be NULL if not required.
&int BytePos:    This result parameter will be updated to reflect the absolute byte position with the font String.  May be NULL if not required.
&int CharX:      This result parameter will be reflect the X coordinate of the character to which the (X, Y) coordinates are resolved.  May be NULL if not required.

-ERRORS-
Okay: The character position was calculated.
Args:
FieldNotSet: The String field has not been set.

*********************************************************************************************************************/

static ERROR fntConvertCoords(extFont *Font, CSTRING String, LONG X, LONG Y, LONG *Column, LONG *Row,
   LONG *ByteColumn, LONG *BytePos, LONG *CharX)
{
   font_glyph *cache;
   LONG i;

   LONG row     = 0;
   LONG column  = 0;
   LONG bytecol = 0;
   LONG bytepos = 0;

   CSTRING str;
   if (!(str = String)) {
      if (!(str = Font->String)) return ERR_NullArgs;
   }

   if (X < 0) X = 0;
   if (Y < 0) Y = 0;

   // Calculate the row

   LONG y = Y;
   while (y > Font->LineSpacing) {
      // Search for line feeds
      i = 0;
      while ((str[i]) and (str[i] != '\n')) for (++i; ((str[i] & 0xc0) IS 0x80); i++);
      if (str[i] IS '\n') {
         y -= Font->LineSpacing;
         Row++;
         str += i + 1;
         BytePos += i + 1;
      }
      else break;
   }

   // Calculate the column

   LONG xpos = 0;
   LONG width = 0;
   ULONG prevglyph = 0;
   while ((*str) and (*str != '\n')) {
      if (Font->FixedWidth > 0) {
         str += getutf8(str, NULL);
         width = Font->FixedWidth + Font->GlyphSpacing;
      }
      else if (*str IS '\t') {
         WORD tabwidth = (Font->prvChar[' '].Advance + Font->GlyphSpacing) * Font->TabSize;
         width = ROUNDUP(xpos, tabwidth) - xpos;
         str++;
      }
      else {
         ULONG unicode;
         str += getutf8(str, &unicode);

         if (Font->Flags & FTF_SCALABLE) {
            if (unicode IS ' ') {
               width = Font->prvChar[' '].Advance + Font->GlyphSpacing;
            }
            else if ((!(Font->Flags & FTF_KERNING)) and (unicode < 256) and (Font->prvChar[unicode].Advance)) {
               width = Font->prvChar[unicode].Advance + Font->GlyphSpacing;
            }
            else if ((cache = get_glyph(Font, unicode, false))) {
               width = cache->AdvanceX + Font->GlyphSpacing;
               if (Font->Flags & FTF_KERNING) xpos += get_kerning(Font->Cache->Face, cache->GlyphIndex, prevglyph);
               prevglyph = cache->GlyphIndex;
            }
         }
         else if ((unicode < 256) and (Font->prvChar[unicode].Advance)) width = Font->prvChar[unicode].Advance + Font->GlyphSpacing;
         else width = Font->prvChar[(LONG)Font->prvDefaultChar].Advance + Font->GlyphSpacing;
      }

      // Subtract the width of the current character and keep processing.  Note that the purpose of dividing the width
      // by 2 is to allow for rounding up if the point is closer to the right hand side of the character.

      if (xpos + (width>>1) >= X) break;
      xpos += width;

      column  += 1;
      bytecol += 1;
      bytepos += 1;
   }

   //log.msg("fntConvertCoords:","Row: %d, Col: %d, BCol: %d, BPos: %d", row, column, bytecol, bytepos);

   if (Row)        *Row        = row;
   if (Column)     *Column     = column;
   if (ByteColumn) *ByteColumn = bytecol;
   if (BytePos)    *BytePos    = bytepos;
   if (CharX)      *CharX      = xpos;

   return ERR_Okay;
}

/*********************************************************************************************************************

-FUNCTION-
SetDefaultSize: Sets the default font size for the application.

This function is used to set the default font size for the application.  This will affect fonts that you create with
proportional sizes (e.g. a point size of 150% and a default point of 10 would result in a 15 point font).  Also, Font
objects with no preset size will be set to the default size.

Please note that the default size is defined by the global style value on the xpath `/interface/@fontsize`.  This can
also be overridden by the user's style preference.  We recommend against an application calling SetDefaultSize() unless
the interface design makes it a necessity (for instance if the user has poor eyesight, restricting the font size may
have usability implications).

-INPUT-
double Size: The new default point size.

-RESULT-
double: The previous font size is returned.

*********************************************************************************************************************/

static DOUBLE fntSetDefaultSize(DOUBLE Size)
{
   DOUBLE previous;
   if ((Size < 6) or (Size > 100)) return glDefaultPoint;

   previous = glDefaultPoint;
   glDefaultPoint = Size;
   glPointSet = true;
   return previous;
}

/*********************************************************************************************************************

-FUNCTION-
InstallFont: Installs a new font.

The InstallFont() function is used to install new fonts on a system running Parasol.  While it is possible
for users to download new font files and install them by hand, this is a process that is too difficult for novices and
is open to mistakes on the part of the user.  By using a program that uses the InstallFont() function, the installation
process can take place automatically.

To install a new font, you only need to know the location of the font file(s).  The rest of the information about the
font will be derived after an analysis of the data.

Once this function is called, the data files will be copied into the correct sub-directory and the font registration
files will be updated to reflect the presence of the new font.  The font will be available immediately thereafter, so
there is no need to reset the system to acknowledge the presence of the font.

-INPUT-
cstr Files: A list of the font files that are to be installed must be specified here.  If there is more than one data file, separate each file name with a semi-colon.

-ERRORS-
Okay: The font information was successfully installed.
NullArgs:
NoSupport: One of the font files is in an unsupported file format.

*********************************************************************************************************************/

static ERROR fntInstallFont(CSTRING Files)
{
   pf::Log log(__FUNCTION__);

   if (!Files) return log.warning(ERR_NullArgs);

   log.branch("Files: %s", Files);

   // Copy all files to the destination directory

   char buffer[512];
   LONG i = 0;
   while (Files[i]) {
      LONG j;
      for (j=0; (Files[i]) and (Files[i] != ';'); j++) buffer[j] = Files[i++];
      buffer[j++] = 0;

      // Read the file header to figure out whether the file belongs in the fixed or truetype directory.

      objFile::create file = { fl::Flags(FL_READ), fl::Path(buffer) };
      if (file.ok()) {
         if (!file->read(buffer, 256)) {
            CSTRING directory = ((buffer[0] IS 'M') and (buffer[1] IS 'Z')) ? "fixed" : "truetype";
            snprintf(buffer, sizeof(buffer), "fonts:%s/", directory);
            flCopy(*file, buffer, NULL);
         }
      }

      if (Files[i]) {
         i++;
         while ((Files[i]) and (Files[i] <= 0x20)) i++;
      }
   }

   // Refresh the font server so that the installed files will show up in the font list

   fntRefreshFonts();
   return ERR_Okay;
}


/*********************************************************************************************************************

-FUNCTION-
RemoveFont: Removes an installed font from the system.

RemoveFont() will remove any font that is installed in the system.  Once a font has been removed, the data is
permanently destroyed and it cannot be recovered.  To remove a font, specify its family name only.
All associated styles for that font will be deleted.

This function may fail if attempting to remove a font that is currently in use.

-INPUT-
cstr Name: The name of the font face (family name) that is to be removed.

-ERRORS-
Okay: The font was successfully removed.
Args: Invalid arguments were specified.
DeleteFile: Removal aborted due to a file deletion failure.

*********************************************************************************************************************/

static ERROR fntRemoveFont(CSTRING Name)
{
   pf::Log log(__FUNCTION__);

   if (!Name) return log.warning(ERR_NullArgs);
   if (!Name[0]) return log.warning(ERR_EmptyString);

   log.branch("%s", Name);

   pf::ScopedObjectLock<objConfig> config(glConfig);
   if (!config.granted()) return log.warning(ERR_AccessObject);

   // Delete all files related to this font

   ConfigGroups *groups;
   if (!glConfig->getPtr(FID_Data, &groups)) {
      for (auto & [group, keys] : *groups) {
         if (StrMatch(Name, keys["Name"].c_str())) continue;

         ERROR error = ERR_Okay;
         if (keys.contains("Styles")) {
            auto styles = keys["Styles"];
            log.trace("Scanning styles: %s", styles.c_str());

            for (ULONG i=0; styles[i]; ) {
               auto n = styles.find(",", i);
               if (n IS std::string::npos) n = styles.size();

               auto fixed_style = std::string("Fixed:") + styles.substr(i, n);
               if (keys.contains(fixed_style)) {
                  if (DeleteFile(keys[fixed_style].c_str(), NULL)) error = ERR_DeleteFile;
               }

               auto scale_style = std::string("Scale:") + styles.substr(i, n);
               if (keys.contains(scale_style)) {
                  if (DeleteFile(keys[scale_style].c_str(), NULL)) error = ERR_DeleteFile;
               }

               i += n + 1;
            }
         }
         else log.warning("There is no Styles entry for the %s font.", Name);

         cfgDeleteGroup(glConfig, group.c_str());
         return error;
      }
   }

   return log.warning(ERR_Search);
}

/*********************************************************************************************************************

-FUNCTION-
SelectFont: Searches for a 'best fitting' font file based on select criteria.

This function searches for the closest matching font based on the details provided by the client.  The details that can
be searched for include the name, point size and style of the desired font.

It is possible to list multiple faces in order of their preference in the Name parameter.  For instance
`Sans Serif,Source Sans,*` will give preference to `Sans Serif` and will look for `Source Sans` if the first choice
font is unavailable.  The use of the `*` wildcard indicates that the default system font should be used in the event
that neither of the other choices are available.  Note that omitting this wildcard will raise the likelihood of
`ERR_Search` being returned in the event that neither of the preferred choices are available.

Flags that alter the search behaviour are `FTF_PREFER_SCALED`, `FTF_PREFER_FIXED` and `FTF_ALLOW_SCALE`.

-INPUT-
cstr Name:  The name of a font face, or multiple faces in CSV format.  Using camel-case for each word is compulsory.
cstr Style: The required style, e.g. Bold or Italic.  Using camel-case for each word is compulsory.
int Point:  Preferred point size.
int(FTF) Flags:  Optional flags.
&!cstr Path: The location of the best-matching font file is returned in this parameter.

-ERRORS-
Okay
NullArgs
AccessObject: Unable to access the internal font configuration object.
Search: Unable to find a suitable font.
-END-

*********************************************************************************************************************/

static std::optional<std::string> get_font_path(ConfigKeys &Keys, const std::string& Type, const std::string& Style)
{
   pf::Log log(__FUNCTION__);

   std::string cfg_style(Type + ":" + Style);
   log.trace("Looking for font style %s", cfg_style.c_str());
   if (Keys.contains(cfg_style)) return make_optional(Keys[cfg_style]);
   else if (StrMatch("Regular", Style.c_str()) != ERR_Okay) {
      log.trace("Looking for regular version of the font...");
      if (Keys.contains("Fixed:Regular")) return make_optional(Keys["Fixed:Regular"]);
   }
   return std::nullopt;
}

static ERROR fntSelectFont(CSTRING Name, CSTRING Style, LONG Point, LONG Flags, CSTRING *Path)
{
   pf::Log log(__FUNCTION__);

   log.branch("%s:%d:%s, Flags: $%.8x", Name, Point, Style, Flags);

   if (!Name) return log.warning(ERR_NullArgs);

   pf::ScopedObjectLock<objConfig> config(glConfig, 5000);
   if (!config.granted()) return log.warning(ERR_AccessObject);

   ConfigGroups *groups;
   if (glConfig->getPtr(FID_Data, &groups)) return ERR_Search;

   bool multi = (Flags & FTF_ALLOW_SCALE) ? true : false; // ALLOW_SCALE is equivalent to '*' for fixed fonts

   std::string style_name(Style);
   pf::camelcase(style_name);

   std::string fixed_group_name, scale_group_name;
   ConfigKeys *fixed_group = NULL, *scale_group = NULL;

   std::vector<std::string> names;
   pf::split(std::string(Name), std::back_inserter(names));

   for (auto &name : names) {
      if (!name.compare("*")) {
         // Use of the '*' wildcard indicates that the default scalable font can be used.  This feature is usually
         // combined with a fixed font, e.g. "Sans Serif,*"
         multi = true;
         break;
      }

      pf::ltrim(name, "'\"");
      pf::rtrim(name, "'\"");

      for (auto & [group, keys] : groups[0]) {
         if (!StrCompare(name.c_str(), keys["Name"].c_str(), 0, STR::WILDCARD)) {
            // Determine if this is a fixed and/or scalable font.  Note that if the font supports
            // both fixed and scalable, fixed_group and scale_group will point to the same font.

            for (auto & [k, v] : keys) {
               if ((!fixed_group) and (!k.compare(0, 6, "Fixed:"))) {
                  fixed_group_name = group;
                  fixed_group = &keys;
                  if (scale_group) break;
               }
               else if ((!scale_group) and (!k.compare(0, 6, "Scale:"))) {
                  scale_group_name = group;
                  scale_group = &keys;
                  if (fixed_group) break;
               }
            }

            break;
         }
      }

      if ((fixed_group) or (scale_group)) break; // Break now if suitable fixed and/or scalable font settings have been discovered.

      multi = true;
   }

   if ((!scale_group) and (!fixed_group)) log.warning("The font \"%s\" is not installed on this system.", Name);
   if (Flags & FTF_REQUIRE_FIXED) scale_group = NULL;
   if (Flags & FTF_REQUIRE_SCALED) fixed_group = NULL;

   if ((!scale_group) and (!(FTF_REQUIRE_FIXED))) {
      // Select a default scalable font if multi-face font search was enabled.  Otherwise we presume that
      // auto-upgrading of the fixed font is undesirable.
      if ((fixed_group) and (multi)) {
         static char default_font[60] = "";
         if (!default_font[0]) { // Static value only needs to be calculated once
            OBJECTID style_id;
            if (!FindObject("glStyle", ID_XML, FOF::NIL, &style_id)) {
               pf::ScopedObjectLock<objXML> style(style_id, 3000);
               if (style.granted()) {
                  if (acGetVar(style.obj, "/fonts/font(@name='scalable')/@face", default_font, sizeof(default_font))) {
                     StrCopy("Hera Sans", default_font, sizeof(default_font));
                  }
               }
            }
         }

         for (auto & [group, keys] : *groups) {
            if ((keys.contains("Name")) and (!keys["Name"].compare(default_font))) {
               scale_group_name = group;
               scale_group = &keys;
               break;
            }
         }
      }

      if (!fixed_group) { // Sans Serif is a good default for a fixed font.
         for (auto & [group, keys] : *groups) {
            if ((keys.contains("Name")) and (!keys["Name"].compare("Sans Serif"))) {
               fixed_group_name = group;
               fixed_group = &keys;
               break;
            }
         }
      }
   }

   if ((!fixed_group) and (!scale_group)) return ERR_Search;

   // Read the point sizes for the fixed group and determine if the requested point size is within 2 units of one
   // of those values.  If not, we'll have to use the scaled font option.

   if ((fixed_group) and (scale_group) and (Point)) {
      bool acceptable = false;
      std::vector<std::string> points;
      pf::split(fixed_group[0]["Points"], std::back_inserter(points));

      for (auto &point : points) {
         auto diff = StrToInt(point.c_str()) - Point;
         if ((diff >= -1) and (diff <= 1)) { acceptable = true; break; }
      }

      if (!acceptable) {
         log.extmsg("Fixed point font is not a good match, will use scalable font.");
         fixed_group = NULL;
      }
   }

   ConfigKeys *preferred_group, *alt_group = NULL;
   std::string preferred_type, alt_type;

   if (not ((Point < 12) or (Flags & (FTF_PREFER_FIXED|FTF_REQUIRE_FIXED)))) {
      preferred_group = fixed_group;
      preferred_type = "Fixed";
      if (!(Flags & FTF_REQUIRE_FIXED)) {
         alt_group = scale_group;
         alt_type = "Scale";
      }
   }
   else {
      preferred_group = scale_group;
      preferred_type = "Scale";
      if (!(Flags & FTF_REQUIRE_SCALED)) {
         alt_group = fixed_group;
         alt_type = "Fixed";
      }
   }

   if (preferred_group) {
      auto path = get_font_path(*preferred_group, preferred_type, style_name);
      if (path.has_value()) {
         if ((*Path = StrClone(path.value().c_str()))) return ERR_Okay;
         else return ERR_AllocMemory;
      }
   }

   if (alt_group) {
      auto path = get_font_path(*alt_group, alt_type, style_name);
      if (path.has_value()) {
         if ((*Path = StrClone(path.value().c_str()))) return ERR_Okay;
         else return ERR_AllocMemory;
      }
   }

   // A regular font style in either format does not exist, so choose the first style that is listed

   log.warning("Requested style '%s' not supported, choosing first style.", style_name.c_str());

   std::string styles = (preferred_group) ? preferred_group[0]["Styles"] : alt_group[0]["Styles"];
   auto end = styles.find(",");
   if (end IS std::string::npos) end = styles.size();
   std::string first_style = styles.substr(0, end);

   if ((preferred_group) and (preferred_group->contains(preferred_type + ":" + first_style))) {
      *Path = StrClone(preferred_group[0][preferred_type + ":" + first_style].c_str());
      return ERR_Okay;
   }
   else if ((alt_group) and (alt_group->contains(alt_type + ":" + first_style))) {
      *Path = StrClone(alt_group[0][alt_type + ":" + first_style].c_str());
      return ERR_Okay;
   }

   return ERR_Search;
}

/*********************************************************************************************************************

-INTERNAL-
RefreshFonts: Refreshes the system font list with up-to-date font information.

The Refresh() action scans for fonts that are installed in the system.  Ideally this action should not be necessary
when the InstallFont() and RemoveFont() methods are used correctly, however it can be useful when font files have been
manually deleted or added to the system.

Refreshing fonts can take an extensive amount of time as each font file needs to be completely analysed for information.
Once the analysis is complete, the `cfgSystemFonts` object will be updated and the `fonts:fonts.cfg` file will reflect current
font settings.

-ERRORS-
Okay: Fonts were successfully refreshed.
AccessObject: Access to the SytsemFonts object was denied, or the object does not exist.
-END-

*********************************************************************************************************************/

static ERROR fntRefreshFonts(void)
{
   pf::Log log(__FUNCTION__);

   log.branch("Refreshing the fonts: directory.");

   pf::ScopedObjectLock<objConfig> config(glConfig, 3000);
   if (!config.granted()) return log.warning(ERR_AccessObject);

   acClear(glConfig); // Clear out existing font information

   scan_fixed_folder(glConfig);
   scan_truetype_folder(glConfig);

   log.trace("Sorting the font names.");

   cfgSortByKey(glConfig, NULL, FALSE); // Sort the font names into alphabetical order

   // Create a style list for each font, e.g.
   //
   //    Fixed:Bold Italic = fonts:fixed/courier.fon
   //    Scale:Bold = fonts:truetype/Courier Prime Bold.ttf
   //    Styles = Bold,Bold Italic,Italic,Regular

   log.trace("Generating style lists for each font.");

   ConfigGroups *groups;
   if (!glConfig->getPtr(FID_Data, &groups)) {
      for (auto & [group, keys] : *groups) {
         std::list <std::string> styles;
         for (auto & [k, v] : keys) {
            if (!k.compare(0, 6, "Fixed:")) styles.push_front(k.substr(6, std::string::npos));
            else if (!k.compare(0, 6, "Scale:")) styles.push_front(k.substr(6, std::string::npos));
         }

         styles.sort();
         std::string style_list;
         for (LONG i=0; not styles.empty(); i++) {
            if (i) style_list.append(",");
            style_list.append(styles.front());
            styles.pop_front();
         }

         keys["Styles"] = style_list.c_str();
      }
   }

   // Save the font configuration file

   log.trace("Saving the font configuration file.");

   objFile::create file = { fl::Path("fonts:fonts.cfg"), fl::Flags(FL_NEW|FL_WRITE) };
   if (file.ok()) glConfig->saveToObject(*file);

   return ERR_Okay;
}

//********************************************************************************************************************

static void scan_truetype_folder(objConfig *Config)
{
   pf::Log log(__FUNCTION__);
   DirInfo *dir;
   LONG j;
   char location[100];
   char group[200];
   FT_Face ftface;
   FT_Open_Args open;

   log.branch("Scanning for truetype fonts.");

   if (!OpenDir("fonts:truetype/", RDF::FILE, &dir)) {
      while (!ScanDir(dir)) {
         snprintf(location, sizeof(location), "fonts:truetype/%s", dir->Info->Name);

         for (j=0; location[j]; j++);
         while ((j > 0) and (location[j-1] != '.') and (location[j-1] != ':') and (location[j-1] != '/') and (location[j-1] != '\\')) j--;

         ResolvePath(location, RSF::NIL, (STRING *)&open.pathname);
         open.flags = FT_OPEN_PATHNAME;
         if (!FT_Open_Face(glFTLibrary, &open, 0, &ftface)) {
            FreeResource(open.pathname);

            log.msg("Detected font file \"%s\", name: %s, style: %s", location, ftface->family_name, ftface->style_name);

            LONG n;
            if (ftface->family_name) n = StrCopy(ftface->family_name, group, sizeof(group));
            else {
               n = 0;
               while ((j > 0) and (location[j-1] != ':') and (location[j-1] != '/') and (location[j-1] != '\\')) j--;
               while ((location[j]) and (location[j] != '.')) group[n++] = location[j++];
            }
            group[n] = 0;

            // Strip any style references out of the font name and keep them as style flags

            LONG style = 0;
            if (ftface->style_name) {
               if ((n = StrSearchCase(" Bold", group)) != -1) {
                  for (j=0; " Bold"[j]; j++) group[n++] = ' ';
                  style |= FTF_BOLD;
               }

               if ((n = StrSearchCase(" Italic", group)) != -1) {
                  for (j=0; " Italic"[j]; j++) group[n++] = ' ';
                  style |= FTF_ITALIC;
               }
            }

            for (n=0; group[n]; n++);
            while ((n > 0) and (group[n-1] <= 0x20)) n--;
            group[n] = 0;

            Config->write(group, "Name", group);

            if (FT_IS_SCALABLE(ftface)) Config->write(group, "Scalable", "Yes");

            // Add the style with a link to the font file location

            if (FT_IS_SCALABLE(ftface)) {
               if ((ftface->style_name) and (StrMatch("regular", ftface->style_name) != ERR_Okay)) {
                  std::string buffer("Scale:");
                  buffer.append(ftface->style_name);
                  Config->write(group, buffer.c_str(), location);
               }
               else {
                  if (style IS FTF_BOLD) Config->write(group, "Scale:Bold", location);
                  else if (style IS FTF_ITALIC) Config->write(group, "Scale:Italic", location);
                  else if (style IS (FTF_BOLD|FTF_ITALIC)) Config->write(group, "Scale:Bold Italic", location);
                  else Config->write(group, "Scale:Regular", location);
               }
            }

            FT_Done_Face(ftface);
         }
         else {
            FreeResource(open.pathname);
            log.warning("Failed to analyse scalable font file \"%s\".", location);
         }
      }

      FreeResource(dir);
   }
   else log.warning("Failed to open the fonts:truetype/ directory.");
}

//********************************************************************************************************************

static void scan_fixed_folder(objConfig *Config)
{
   pf::Log log(__FUNCTION__);

   log.branch("Scanning for fixed fonts.");

   DirInfo *dir;
   if (!OpenDir("fonts:fixed/", RDF::FILE, &dir)) {
      while (!ScanDir(dir)) {
         bool bold = false;
         bool bolditalic = false;
         bool italic = false;

         std::string location("fonts:fixed/");
         location.append(dir->Info->Name);
         auto src = location.c_str();

         winfnt_header_fields header;
         UBYTE points[20];
         STRING facename;
         if (!analyse_bmp_font(src, &header, &facename, points, ARRAYSIZE(points))) {
            log.extmsg("Detected font file \"%s\", name: %s", src, facename);

            if (!facename) continue;
            std::string group(facename);

            // Strip any style references out of the font name and keep them as style flags

            LONG style = 0;

            {
               auto n = group.find(" Bold");
               if (n != std::string::npos) {
                  group.erase(n, 5);
                  style |= FTF_BOLD;
               }
            }

            {
               auto n = group.find(" Italic");
               if (n != std::string::npos) {
                  group.erase(n, 7);
                  style |= FTF_ITALIC;
               }
            }

            if (header.italic) style |= FTF_ITALIC;
            if (header.weight >= 600) style |= FTF_BOLD;

            {
               auto n = group.length();
               while ((n > 0) and (group[n-1] <= 0x20)) n--;
               if (n != group.length()) group.resize(n);
            }

            auto gs = group.c_str();
            Config->write(gs, "Name", gs);

            // Add the style with a link to the font file location

            if (style IS FTF_BOLD) {
               Config->write(gs, "Fixed:Bold", src);
               bold = true;
            }
            else if (style IS FTF_ITALIC) {
               Config->write(gs, "Fixed:Italic", src);
               italic = true;
            }
            else if (style IS (FTF_BOLD|FTF_ITALIC)) {
               Config->write(gs, "Fixed:Bold Italic", src);
               bolditalic = true;
            }
            else {
               Config->write(gs, "Fixed:Regular", src);
               if (!bold)       Config->write(gs, "Fixed:Bold", src);
               if (!bolditalic) Config->write(gs, "Fixed:Bold Italic", src);
               if (!italic)     Config->write(gs, "Fixed:Italic", src);
            }

            std::ostringstream out;
            for (LONG i=0; points[i]; i++) {
               if (i > 0) out << ',';
               out << points[i];
            }

            Config->write(gs, "Points", out.str());

            FreeResource(facename);
         }
         else log.warning("Failed to analyse %s", src);
      }
      FreeResource(dir);
   }
   else log.warning("Failed to scan directory fonts:fixed/");
}

//********************************************************************************************************************

static ERROR analyse_bmp_font(CSTRING Path, winfnt_header_fields *Header, STRING *FaceName, UBYTE *Points, UBYTE MaxPoints)
{
   pf::Log log(__FUNCTION__);
   winmz_header_fields mz_header;
   winne_header_fields ne_header;
   LONG i, res_offset, font_offset;
   UWORD size_shift, font_count, count;
   char face[50];

   if ((!Path) or (!Header) or (!FaceName)) return ERR_NullArgs;

   *FaceName = NULL;
   objFile::create file = { fl::Path(Path), fl::Flags(FL_READ) };
   if (file.ok()) {
      file->read(&mz_header, sizeof(mz_header));

      if (mz_header.magic IS ID_WINMZ) {
         file->seekStart(mz_header.lfanew);

         if ((!file->read(&ne_header, sizeof(ne_header))) and (ne_header.magic IS ID_WINNE)) {
            res_offset = mz_header.lfanew + ne_header.resource_tab_offset;
            file->seekStart(res_offset);

            font_count  = 0;
            font_offset = 0;
            size_shift  = 0;
            flReadLE(*file, &size_shift);

            UWORD type_id = 0;
            for (flReadLE(*file, &type_id); type_id; flReadLE(*file, &type_id)) {
               if (!flReadLE(*file, &count)) {
                  if (type_id IS 0x8008) {
                     font_count  = count;
                     file->get(FID_Position, &font_offset);
                     font_offset = font_offset + 4;
                     break;
                  }

                  file->seekCurrent(4 + count * 12);
               }
            }

            if ((!font_count) or (!font_offset)) {
               log.warning("There are no fonts in file \"%s\"", Path);
               return ERR_Failed;
            }

            file->seekStart(font_offset);

            {
               winFont fonts[font_count];

               // Get the offset and size of each font entry

               for (LONG i=0; i < font_count; i++) {
                  UWORD offset = 0, size = 0;
                  flReadLE(*file, &offset);
                  flReadLE(*file, &size);
                  fonts[i].Offset = offset<<size_shift;
                  fonts[i].Size   = size<<size_shift;
                  file->seekCurrent(8);
               }

               // Read font point sizes

               for (i=0; (i < font_count) and (i < MaxPoints-1); i++) {
                  file->seekStart(fonts[i].Offset);
                  if (!file->read(Header, sizeof(winfnt_header_fields))) {
                     Points[i] = Header->nominal_point_size;
                  }
               }
               Points[i] = 0;

               // Go to the first font in the file and read the font header

               file->seekStart(fonts[0].Offset);

               if (file->read(Header, sizeof(winfnt_header_fields))) {
                  return ERR_Read;
               }

                // NOTE: 0x100 indicates the Microsoft vector font format, which we do not support.

               if ((Header->version != 0x200) and (Header->version != 0x300)) {
                  log.warning("Font \"%s\" is written in unsupported version %d / $%x.", Path, Header->version, Header->version);
                  return ERR_NoSupport;
               }

               if (Header->file_type & 1) {
                  log.warning("Font \"%s\" is in the non-supported vector font format.", Path);
                  return ERR_NoSupport;
               }

               // Extract the name of the font

               file->seekStart(fonts[0].Offset + Header->face_name_offset);

               for (i=0; (size_t)i < sizeof(face)-1; i++) {
                  if ((file->read(face+i, 1)) or (!face[i])) break;
               }
               face[i] = 0;
               *FaceName = StrClone(face);
            }

            return ERR_Okay;
         }
         else return ERR_NoSupport;
      }
      else return ERR_NoSupport;
   }
   else return ERR_File;
}

//********************************************************************************************************************

#include "class_font.cpp"

//********************************************************************************************************************

static STRUCTS glStructures = {
   { "FontList", sizeof(FontList) }
};

PARASOL_MOD(CMDInit, NULL, CMDOpen, CMDExpunge, MODVERSION_FONT, MOD_IDL, &glStructures)
