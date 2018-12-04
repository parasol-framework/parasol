/*****************************************************************************

The source code of the Parasol project is made publicly available under the
terms described in the LICENSE.TXT file that is distributed with this package.
Please refer to it for further information on licensing.

This code utilises the work of the FreeType Project under the FreeType
License.  For more information please refer to the FreeType home page at
www.freetype.org.

******************************************************************************

-MODULE-
Font: Provides font management functionality and hosts the Font and FontServer classes.

-END-

Switching from fonts.cfg to fonts.json or fonts.xml would be a lot easier to process, e.g.

  <fonts>
    <font name="Helvete" points="7,8,9,11" scalable="1" styles="Bold,Bold Italic,Italic,Regular">
      <fixed>
        <fixed style="Regular" src="fonts:fixed/helvete.fon"/>
        <fixed style="Bold" src="fonts:fixed/helvete.fon"/>
        <fixed style="Bold Italic" src="fonts:fixed/helvete.fon"/>
        <fixed style="Italic" src="fonts:fixed/helvete.fon"/>
      </fixed>
      <scaled>
        <scaled style="Regular" src="fonts:truetype/Helvete.ttf"/>
        <scaled style="Bold" src="fonts:truetype/HelveteBold.ttf"/>
        <scaled style="Bold Italic" src="fonts:truetype/HelveteBoldItalic.ttf"/>
        <scaled style="Italic" src="fonts:truetype/HelveteItalic.ttf"/>
      </scaled>
    </font>
  </fonts>

OR

  { "fonts":[
      { "font":"Helvete", points:"7", scalable:true, styles="Bold,Regular",
        fixed:[ { "Style":"Regular", "Src":"fonts:fixed/helvete.fon" } ],
        scaled:[ { } ]
      }
    ]
  }

*****************************************************************************/

#define PRV_FONT
#define PRV_FONT_MODULE
//#define DEBUG

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_STROKER_H

#include <parasol/main.h>
#include "font_structs.h"

#include <parasol/modules/xml.h>
#include <parasol/modules/picture.h>
#include <parasol/modules/font.h>
#include <parasol/modules/display.h>

#include <math.h>
#include <wchar.h>

static struct KeyStore *glCache = NULL;

/*****************************************************************************
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

//****************************************************************************

#define FIXED_DPI 96 // FreeType measurements are based on this DPI.

OBJECTPTR modFont = NULL;
struct CoreBase *CoreBase;
static struct DisplayBase *DisplayBase;
static OBJECTPTR clFont = NULL;
static OBJECTPTR modDisplay = NULL;
static FT_Library glFTLibrary = NULL;
static LONG glDisplayVDPI = FIXED_DPI; // Initially matches the fixed DPI value, can change if display has a high DPI setting.
static LONG glDisplayHDPI = FIXED_DPI;

static ERROR add_font_class(void);
static LONG getutf8(CSTRING, LONG *);
static LONG get_kerning(FT_Face, LONG Glyph, LONG PrevGlyph);
static struct font_glyph * get_glyph(objFont *, ULONG, UBYTE);
static void free_glyph(objFont *);
static void scan_truetype_folder(objConfig *);
static void scan_fixed_folder(objConfig *);
static ERROR analyse_bmp_font(STRING, struct winfnt_header_fields *, STRING *, UBYTE *, UBYTE);
static ERROR  fntRefreshFonts(void);

#include "font_def.c"

//****************************************************************************

INLINE LONG ReadWordLE(OBJECTPTR File)
{
   WORD result = 0;
   flReadLE2(File, &result);
   return result;
}

//****************************************************************************

INLINE LONG ReadLongLE(OBJECTPTR File)
{
   LONG result = 0;
   flReadLE4(File, &result);
   return result;
}

//****************************************************************************
// Return the first unicode value from a given string address.

static LONG getutf8(CSTRING Value, LONG *Unicode)
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

//****************************************************************************
// Returns the global point size for font scaling.  This is set to 10 by default, but the user can change the setting
// in the interface style values.

static DOUBLE glDefaultPoint = 10;
static BYTE glPointSet = FALSE;

static DOUBLE global_point_size(void)
{
   if (!glPointSet) {
      OBJECTID style_id;
      if (!FastFindObject("glStyle", ID_XML, &style_id, 1, NULL)) {
         objXML *style;
         if (!AccessObject(style_id, 3000, &style)) {
            char fontsize[20];
            glPointSet = TRUE;
            if (!acGetVar(style, "/interface/@fontsize", fontsize, sizeof(fontsize))) {
               glDefaultPoint = StrToFloat(fontsize);
               if (glDefaultPoint < 6) glDefaultPoint = 6;
               else if (glDefaultPoint > 80) glDefaultPoint = 80;
               LogMsg("Global font size is %.1f.", glDefaultPoint);
            }
            ReleaseObject(style);
         }
      }
      else LogErrorMsg("glStyle XML object is not available");
   }

   return glDefaultPoint;
}

//****************************************************************************
// For use by fntSelectFont() only.

static ERROR name_matches(CSTRING Name, CSTRING Entry)
{
   while ((*Name) AND (*Name <= 0x20)) Name++;

   UWORD e = 0;
   while ((*Name) AND (Entry[e])) {
      while (*Name IS '\'') Name++; // Ignore the use of encapsulating quotes.
      if (!*Name) break;

      if (LCASE(Entry[e]) IS LCASE(*Name)) {
         e++;
         Name++;
      }
      else break;
   }

   if (!Entry[e]) { // End of Entry reached.  Check if Name is also ended for a match.
      if ((*Name IS ',') OR (*Name IS ':') OR (!*Name)) return ERR_Okay;
   }

   return ERR_Failed;
}

/*****************************************************************************
** Attempts to update globally held DPI values with the main display's real DPI.
*/

static void update_dpi(void)
{
   static LARGE last_update = 0;
   LARGE current_time = PreciseTime();

   if (current_time - last_update > 3000000LL) {
      DISPLAYINFO *display;
      if (!gfxGetDisplayInfo(0, &display)) {
         last_update = PreciseTime();
         if ((display->VDensity >= 96) AND (display->HDensity >= 96)) {
            glDisplayVDPI = display->VDensity;
            glDisplayHDPI = display->HDensity;
            //glDisplayDPI = (glDisplayVDPI + glDisplayHDPI) / 2; // Get the average DPI in case the pixels aren't square.  Note: Average DPI beats diagonal DPI every time.
         }
      }
   }
}

//****************************************************************************
// Only call this function if the font includes kerning support (test via FTF_KERNING).

INLINE void get_kerning_xy(FT_Face Face, LONG Glyph, LONG PrevGlyph, LONG *X, LONG *Y)
{
   FT_Vector delta;
   FT_Get_Kerning(Face, PrevGlyph, Glyph, FT_KERNING_DEFAULT, &delta);
   *X = delta.x>>FT_DOWNSIZE;
   *Y = delta.y>>FT_DOWNSIZE;
}

//****************************************************************************
// Only call this function if the font includes kerning support (test via FTF_KERNING).

INLINE LONG get_kerning(FT_Face Face, LONG Glyph, LONG PrevGlyph)
{
   if ((!Glyph) OR (!PrevGlyph)) return 0;

   FT_Vector delta;
   FT_Get_Kerning(Face, PrevGlyph, Glyph, FT_KERNING_DEFAULT, &delta);
   return delta.x>>FT_DOWNSIZE;
}

//****************************************************************************

INLINE void calc_lines(objFont *Self)
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

//****************************************************************************

static OBJECTID glConfigID = 0;

static ERROR CMDInit(OBJECTPTR argModule, struct CoreBase *argCoreBase)
{
   CoreBase = argCoreBase;

   GetPointer(argModule, FID_Master, &modFont);

   if (LoadModule("display", MODVERSION_DISPLAY, &modDisplay, &DisplayBase) != ERR_Okay) return ERR_InitModule;

   if (!(glCache = VarNew(0, KSF_THREAD_SAFE))) return ERR_AllocMemory;

   // Initialise the FreeType library

   if (FT_Init_FreeType(&glFTLibrary)) {
      LogErrorMsg("Failed to initialise the FreeType font library.");
      return ERR_Failed;
   }

   LONG type;
   UBYTE refresh;

   if ((AnalysePath("config:fonts.cfg", &type) != ERR_Okay) OR (type != LOC_FILE)) refresh = TRUE;
   else refresh = FALSE;

   OBJECTPTR config;
   if (!NewLockedObject(ID_CONFIG, 0, &config, &glConfigID)) {
      SetFields(config,
         FID_Name|TSTR, "cfgSystemFonts",
         FID_Path|TSTR, "config:fonts.cfg",
         TAGEND);
      if (!acInit(config)) {
         if (refresh) fntRefreshFonts();
      }
      else { acFree(config); glConfigID = 0; }
      ReleaseObject(config);
   }

   return add_font_class();
}

static ERROR CMDOpen(OBJECTPTR Module)
{
   SetPointer(Module, FID_FunctionList, glFunctions);
   return ERR_Okay;
}

static ERROR CMDExpunge(void)
{
   if (glFTLibrary) { FT_Done_FreeType(glFTLibrary); glFTLibrary = NULL; }
   if (glConfigID) { acFreeID(glConfigID); glConfigID = 0; }
   if (glCache) { VarFree(glCache); glCache = NULL; }

   // Free allocated class and modules

   if (clFont)     { acFree(clFont);     clFont = NULL; }
   if (modDisplay) { acFree(modDisplay); modDisplay = NULL; }

   // NB: Cached font files are not removed during expunge, because the task's shutdown procedure will have automatically destroy any cached fonts our CMDExpunge() routine is called.

   if (glBitmapCache) {
      struct BitmapCache *scan = glBitmapCache;
      while (scan) {
         struct BitmapCache *next = scan->Next;
         if (scan->Data) FreeMemory(scan->Data);
         if (scan->Outline) FreeMemory(scan->Outline);
         FreeMemory(scan);
         scan = next;
      }
      glBitmapCache = NULL;
   }

   return ERR_Okay;
}

/*****************************************************************************

-FUNCTION-
CharWidth: Returns the width of a character.

This function will return the pixel width of a font character.  The character is specified as a unicode value in the
Char parameter. Kerning values can also be returned, which affect the position of the character along the horizontal.
The previous character in the word is set in KChar and the kerning value will be returned in the Kerning parameter.  If
kerning information is not required, set the KChar and Kerning parameters to zero.

-INPUT-
obj(Font) Font: The font to use for calculating the character width.
uint Char: A unicode character.
uint KChar: A unicode character to use for calculating the font kerning (optional).
&int Kerning: The resulting kerning value (optional).

-RESULT-
int: The pixel width of the character will be returned.

*****************************************************************************/

static LONG fntCharWidth(objFont *Font, ULONG Char, ULONG KChar, LONG *Kerning)
{
   if (Kerning) *Kerning = 0;

   if (Font->FixedWidth > 0) return Font->FixedWidth;
   else if (Font->Flags & FTF_SCALABLE) {
      struct font_glyph *cache;
      if ((cache = get_glyph(Font, Char, FALSE))) {
         if ((Font->Flags & FTF_KERNING) AND (KChar) AND (Kerning)) {
            LONG kglyph = FT_Get_Char_Index(Font->FTFace, KChar);
            *Kerning = get_kerning(Font->FTFace, cache->GlyphIndex, kglyph);
         }
         return cache->Char.AdvanceX + Font->GlyphSpacing;
      }
      else {
         FMSG("fntCharWidth()","No glyph for character %u", Char);
         if (Font->prvChar) return Font->prvChar[Font->prvDefaultChar].Advance;
         else return 0;
      }
   }
   else if (Char < 256) return Font->prvChar[Char].Advance;
   else {
      FMSG("@fntCharWidth:","Character %u out of range.", Char);
      return Font->prvChar[Font->prvDefaultChar].Advance;
   }
}

/*****************************************************************************

-FUNCTION-
GetList: Returns a list of all available system fonts.

This function returns a linked list of all available system fonts.

-INPUT-
&!struct(FontList) Result: The font list is returned here.

-ERRORS-
Okay
NullArgs
AccessObject: The SystemFonts object could not be accessed.

*****************************************************************************/

static ERROR fntGetList(struct FontList **Result)
{
   if (!Result) return ERR_NullArgs;

   *Result = NULL;

   objConfig *config;
   CSTRING section;
   ERROR error = ERR_Okay;
   if (!AccessObject(glConfigID, 3000, &config)) {
      LONG size = 0, totalfonts, i;

      if ((!GetLong(config, FID_TotalSections, &totalfonts)) AND (totalfonts > 0)) {
         for (i=0; i < totalfonts; i++) {
            if (cfgGetSectionFromIndex(config, i, &section) != ERR_Okay) break;

            CSTRING fontname = NULL;
            CSTRING fontstyles = NULL;
            CSTRING fontpoints = NULL;
            cfgReadValue(config, section, "Name", &fontname);
            cfgReadValue(config, section, "Styles", &fontstyles);
            cfgReadValue(config, section, "Points", &fontpoints);

            size += sizeof(struct FontList);
            size += StrLength(fontname) + 1;
            size += StrLength(fontstyles) + 1;
            size += StrLength(fontpoints) + 1;
         }

         struct FontList *list;
         if (!AllocMemory(size, MEM_DATA, &list, NULL)) {
            STRING buffer = (STRING)(list + totalfonts);
            *Result = list;

            for (i=0; i < totalfonts; i++) {
               if (i < totalfonts-1) list->Next = list + 1;
               else list->Next = NULL;

               if (cfgGetSectionFromIndex(config, i, &section) != ERR_Okay) break;

               CSTRING fontname;
               if (!cfgReadValue(config, section, "Name", &fontname)) {
                  list->Name = buffer;
                  buffer += StrCopy(fontname, buffer, COPY_ALL) + 1;
               }

               CSTRING fontstyles;
               if (!cfgReadValue(config, section, "Styles", &fontstyles)) {
                  list->Styles = buffer;
                  buffer += StrCopy(fontstyles, buffer, COPY_ALL) + 1;
               }

               CSTRING scalable;
               if (!cfgReadValue(config, section, "Scalable", &scalable)) {
                  if (!StrCompare("Yes", scalable, 0, STR_MATCH_LEN)) list->Scalable = TRUE;
               }

               CSTRING fontpoints;
               if (!cfgReadValue(config, section, "Points", &fontpoints)) {
                  list->Points = buffer;

                  WORD j;
                  for (j=0; *fontpoints; j++) {
                      *buffer++ = StrToInt(fontpoints);
                     while ((*fontpoints) AND (*fontpoints != ',')) fontpoints++;
                     if (*fontpoints IS ',') fontpoints++;
                  }
                  *buffer++ = 0;
               }

               list++;
            }
         }
         else error = ERR_AllocMemory;
      }
      else error = ERR_NoData;

      ReleaseObject(config);
   }
   else error = ERR_AccessObject;

   return error;
}

/*****************************************************************************

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
obj(Font) Font: An initialised font object.
cstr String: The string to be analysed.
int(FSS) Chars:  The number of characters (not bytes, so consider UTF-8 formatting) to be used in calculating the string length.  FSS constants can also be used here.
int Wrap:   The pixel position at which word wrapping occurs.  If zero or less, wordwrap is disabled.
&int Width: The width of the longest line will be returned in this parameter.
&int Rows:  The number of calculated rows will be returned in this parameter.

*****************************************************************************/

static void fntStringSize(objFont *Font, CSTRING String, LONG Chars, LONG Wrap, LONG *Width, LONG *Rows)
{
   struct font_glyph *cache;
   LONG unicode;
   WORD rowcount, wordwidth, lastword, tabwidth, charwidth, charlen;
   UBYTE line_abort, pchar;

   if ((!Font) OR (!String)) return;
   if (!(Font->Head.Flags & NF_INITIALISED)) return;

   if (Chars IS FSS_LINE) {
      Chars = 0x7fffffff;
      line_abort = 1;
   }
   else {
      line_abort = 0;
      if (Chars < 0) Chars = 0x7fffffff;
   }

   if ((Wrap <= 0) OR (Font->Flags & FTF_CHAR_CLIP)) Wrap = 0x7fffffff;

   //LogMsg("StringSize: %.10s, Wrap %d, Chars %d, Abort: %d", String, Wrap, Chars, line_abort);

   CSTRING start  = String;
   LONG x         = 0;
   LONG prevglyph = 0;
   if (line_abort) rowcount = 0;
   else rowcount  = 1;
   LONG longest   = 0;
   LONG charcount = 0;
   LONG wordindex = 0;
   while ((*String) AND (charcount < Chars)) {
      lastword = x;

      // Skip whitespace

      while ((*String) AND (*String <= 0x20)) {
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

      if ((!*String) OR (line_abort IS 2)) break;

      // Calculate the width of the discovered word

      wordindex = (LONG)(String - start);
      wordwidth = 0;
      charwidth = 0;

      while (charcount < Chars) {
         charlen = getutf8(String, &unicode);

         if (Font->FixedWidth > 0) charwidth = Font->FixedWidth;
         else if (Font->Flags & FTF_SCALABLE) {
            if (unicode IS ' ') {
               charwidth += Font->prvChar[' '].Advance + Font->GlyphSpacing;
            }
            else if ((cache = get_glyph(Font, unicode, FALSE))) {
               charwidth = cache->Char.AdvanceX + Font->GlyphSpacing;
               if (Font->Flags & FTF_KERNING) charwidth += get_kerning(Font->FTFace, cache->GlyphIndex, prevglyph); // Kerning adjustment
               prevglyph = cache->GlyphIndex;
            }
         }
         else if (unicode < 256) charwidth = Font->prvChar[unicode].Advance + Font->GlyphSpacing;
         else charwidth = Font->prvChar[Font->prvDefaultChar].Advance + Font->GlyphSpacing;

         if ((!x) AND (!(Font->Flags & FTF_CHAR_CLIP)) AND (x+wordwidth+charwidth >= Wrap)) {
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

            if ((pchar) OR (*String <= 0x20)) break;
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

/*****************************************************************************

-FUNCTION-
FreetypeHandle: Returns a handle to the FreeType library.

This function returns a direct handle to the internal FreeType library.  It is intended that this handle should only be
used by existing projects that are based on FreeType and need access to its functionality.  References to FreeType
functions can be obtained by loading the Font module and then calling the ResolveSymbol method to retrieve function
names, e.g. "FT_Open_Face".

-RESULT-
ptr: A handle to the FreeType library will be returned.

*****************************************************************************/

static APTR fntFreetypeHandle(void)
{
   return glFTLibrary;
}

/*****************************************************************************

-FUNCTION-
StringWidth: Returns the pixel width of any given string in relation to a font's settings.

This function calculates the pixel width of any string in relation to a font's object definition.  The routine takes
into account any line feeds that might be specified in the String, so if the String contains 8 lines, then the width
of the longest line will be returned.

Word wrapping will not be taken into account, even if it has been enabled in the font object.

-INPUT-
obj(Font) Font: An initialised font object.
cstr String: The string to be calculated.
int Chars: The number of characters (not bytes, so consider UTF-8 formatting) to be used in calculating the string length, or -1 if you want the entire string to be used.

-RESULT-
int: The pixel width of the string is returned - this will be zero if there was an error or the string is empty.

*****************************************************************************/

static LONG fntStringWidth(objFont *Font, CSTRING String, LONG Chars)
{
   if ((!Font) OR (!String)) return 0;

   if (!(Font->Head.Flags & NF_INITIALISED)) return 0;

   struct font_glyph *cache;

   CSTRING str = String;
   if (Chars < 0) Chars = 0x7fffffff;

   LONG len     = 0;
   LONG lastlen = 0;
   LONG prevglyph = 0;
   while ((*str) AND (Chars > 0)) {
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
            if ((unicode < 256) AND (Font->prvChar[unicode].Advance) AND (!(Font->Flags & FTF_KERNING))) {
               len += Font->prvChar[unicode].Advance + Font->GlyphSpacing;
            }
            else if (unicode IS ' ') {
               len += Font->prvChar[' '].Advance + Font->GlyphSpacing;
            }
            else if ((cache = get_glyph(Font, unicode, FALSE))) {
               len += cache->Char.AdvanceX + Font->GlyphSpacing;
               if (Font->Flags & FTF_KERNING) len += get_kerning(Font->FTFace, cache->GlyphIndex, prevglyph);
               prevglyph = cache->GlyphIndex;
            }
         }
         else if ((unicode < 256) AND (Font->prvChar[unicode].Advance)) {
            len += Font->prvChar[unicode].Advance + Font->GlyphSpacing;
         }
         else len += Font->prvChar[Font->prvDefaultChar].Advance + Font->GlyphSpacing;
      }
   }

   if (lastlen > len) return lastlen - Font->GlyphSpacing;
   else if (len > 0) return len - Font->GlyphSpacing;
   else return 0;
}

/*****************************************************************************

-FUNCTION-
ConvertCoords: Converts pixel coordinates into equivalent column and row positions in font strings.

This function is used to convert pixel coordinates within a font String into the equivalent Row and Column character
positions.  If the coordinate values that you supply are in excess of the String dimensions, the Column and Row results
will be automatically restricted to their maximum value.  For instance, if the Y argument is set to 280 and the
String consists of 15 rows amounting to 150 pixels in height, the Row value will be returned as 15.

Negative coordinate values are not permitted.

-INPUT-
obj(Font) Font: An initialised font object.
cstr String: Must point to the string that you will be inspecting, or NULL if you want to inspect the string currently in the font's String field.
int X:       The horizontal coordinate that you want to translate into a column position.
int Y:       The vertical coordinate that you want to translate into a row position.
&int Column: This result parameter will be updated to reflect the calculated character position, with consideration to the UTF-8 standard.  May be NULL if not required.
&int Row:    This result parameter will be updated to reflect the calculated row position.  May be NULL if not required.
&int ByteColumn: This result parameter will be updated to reflect the absolute column byte position within the given row.  May be NULL if not required.
&int BytePos:    This result parameter will be updated to reflect the absolute byte position with the font String.  May be NULL if not required.
&int CharX:      This result parameter will be reflect the X coordinate of the character to which the (X, Y) coordinates are resolved.  May be NULL if not required.

-ERRORS-
Okay: The character position was calculated.
Args:
FieldNotSet: The String field has not been set.

*****************************************************************************/

static ERROR fntConvertCoords(objFont *Font, CSTRING String, LONG X, LONG Y, LONG *Column, LONG *Row,
   LONG *ByteColumn, LONG *BytePos, LONG *CharX)
{
   struct font_glyph *cache;
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
      while ((str[i]) AND (str[i] != '\n')) for (++i; ((str[i] & 0xc0) IS 0x80); i++);
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
   while ((*str) AND (*str != '\n')) {
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
            else if ((!(Font->Flags & FTF_KERNING)) AND (unicode < 256) AND (Font->prvChar[unicode].Advance)) {
               width = Font->prvChar[unicode].Advance + Font->GlyphSpacing;
            }
            else if ((cache = get_glyph(Font, unicode, FALSE))) {
               width = cache->Char.AdvanceX + Font->GlyphSpacing;
               if (Font->Flags & FTF_KERNING) xpos += get_kerning(Font->FTFace, cache->GlyphIndex, prevglyph);
               prevglyph = cache->GlyphIndex;
            }
         }
         else if ((unicode < 256) AND (Font->prvChar[unicode].Advance)) width = Font->prvChar[unicode].Advance + Font->GlyphSpacing;
         else width = Font->prvChar[Font->prvDefaultChar].Advance + Font->GlyphSpacing;
      }

      // Subtract the width of the current character and keep processing.  Note that the purpose of dividing the width
      // by 2 is to allow for rounding up if the point is closer to the right hand side of the character.

      if (xpos + (width>>1) >= X) break;
      xpos += width;

      column  += 1;
      bytecol += 1;
      bytepos += 1;
   }

   //LogF("fntConvertCoords:","Row: %d, Col: %d, BCol: %d, BPos: %d", row, column, bytecol, bytepos);

   if (Row)        *Row        = row;
   if (Column)     *Column     = column;
   if (ByteColumn) *ByteColumn = bytecol;
   if (BytePos)    *BytePos    = bytepos;
   if (CharX)      *CharX      = xpos;

   return ERR_Okay;
}

/*****************************************************************************

-FUNCTION-
SetDefaultSize: Sets the default font size for the application.

This function is used to set the default font size for the application.  This will affect fonts that you create with
proportional sizes (e.g. a point size of 150% and a default point of 10 would result in a 15 point font).  Also, Font
objects with no preset size will be set to the default size.

Please note that the default size is defined by the global style value on the xpath "/interface/@fontsize".  This can
also be overridden by the user's style preference.  For this reason, we recommend against your application using
SetDefaultSize() unless your interface design makes it a necessity (e.g. the user may have poor eyesight, so
restricting the font size may have usability implications).

-INPUT-
double Size: The new default point size.

-RESULT-
double: The previous font size is returned.

*****************************************************************************/

static DOUBLE fntSetDefaultSize(DOUBLE Size)
{
   DOUBLE previous;
   if ((Size < 6) OR (Size > 100)) return glDefaultPoint;

   previous = glDefaultPoint;
   glDefaultPoint = Size;
   glPointSet = TRUE;
   return previous;
}

/*****************************************************************************

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
ExclusiveDenied: Access to the SystemFonts object was denied.
NullArgs:
NoSupport: One of the font files is in an unsupported file format.

*****************************************************************************/

static ERROR fntInstallFont(CSTRING Files)
{
   if (!Files) return PostError(ERR_NullArgs);

   LogBranch("Files: %s", Files);

   // Copy all files to the destination directory

   BYTE buffer[512];
   LONG i = 0;
   while (Files[i]) {
      LONG j;
      for (j=0; (Files[i]) AND (Files[i] != ';'); j++) buffer[j] = Files[i++];
      buffer[j++] = 0;

      // Read the file header to figure out whether the file belongs in the fixed or truetype directory.

      OBJECTPTR file;
      if (CreateObject(ID_FILE, 0, &file,
            FID_Flags|TLONG, FL_READ,
            FID_Path|TSTR,   buffer,
            TAGEND) IS ERR_Okay) {

         CSTRING directory = "fixed";
         if (!acRead(file, buffer, 256, NULL)) {
            if ((buffer[0] IS 'M') AND (buffer[1] IS 'Z')) directory = "fixed";
            else directory = "truetype";

            StrFormat(buffer, sizeof(buffer), "fonts:%s/", directory);
            flCopy(file, buffer, NULL);
         }

         acFree(file);
      }

      if (Files[i]) {
         i++;
         while ((Files[i]) AND (Files[i] <= 0x20)) i++;
      }
   }

   // Refresh the font server so that the installed files will show up in the font list

   fntRefreshFonts();

   LogBack();
   return ERR_Okay;
}


/*****************************************************************************

-FUNCTION-
RemoveFont: Removes an installed font from the system.

RemoveFont() will remove any font that is installed in the system.  Once a font has been removed, the data is
permanently destroyed and it cannot be recovered.  To remove a font, you are required to specify its family name only.
All associated styles for that font will be deleted.

This function may fail if attempting to remove a font that is currently in use.

-INPUT-
cstr Name: The name of the font face (family name) that is to be removed.

-ERRORS-
Okay: The font was successfully removed.
Args: Invalid arguments were specified.
DeleteFile: Removal aborted due to a file deletion failure.

*****************************************************************************/

static ERROR fntRemoveFont(CSTRING Name)
{
   objConfig *config;
   LONG i, n;
   UBYTE buffer[200], style[200];

   if (!Name) return PostError(ERR_NullArgs);
   if (!Name[0]) return PostError(ERR_EmptyString);

   LogBranch("%s", Name);

   if (AccessObject(glConfigID, 3000, &config) != ERR_Okay) {
      return LogBackError(0, ERR_AccessObject);
   }

   // Delete all files related to this font

   LONG amtentries = config->AmtEntries;
   struct ConfigEntry *entries = config->Entries;

   for (i=0; i < amtentries; i++) {
      if (!StrMatch("Name", entries[i].Key)) {
         if (!StrMatch(Name, entries[i].Data)) {
            break;
         }
      }
   }

   if (i >= amtentries) {
      ReleaseObject(config);
      return LogBackError(0, ERR_Search);
   }

   CSTRING str;
   ERROR error = ERR_Okay;
   if (!cfgReadValue(config, entries[i].Section, "Styles", &str)) {
      MSG("Scanning styles: %s", str);

      while (*str) {
         for (n=0; ((*str) AND (*str != ',')); n++) style[n] = *str++;
         style[n] = 0;

         if (*str IS ',') str++;

         StrFormat(buffer, sizeof(buffer), "Fixed:%s", style);
         CSTRING value;
         if (!cfgReadValue(config, entries[i].Section, buffer, &value)) {
            if (DeleteFile(value, NULL)) error = ERR_DeleteFile;
         }

         StrFormat(buffer, sizeof(buffer), "Scale:%s", style);
         if (!cfgReadValue(config, entries[i].Section, buffer, &value)) {
            if (DeleteFile(value, NULL)) error = ERR_DeleteFile;
         }
      }
   }
   else LogErrorMsg("There is no Styles entry for the %s font.", Name);

   StrCopy(entries[i].Section, buffer, sizeof(buffer));
   cfgDeleteSection(config, buffer);

   ReleaseObject(config);

   MSG("Font removed successfully.");
   LogBack();
   return error;
}

/*****************************************************************************

-FUNCTION-
SelectFont: Searches for a 'best fitting' font file based on select criteria.

This function searches for the closest matching font based on the details provided by the client.  The details that can
be searched for include the name, point size and style of the desired font.

It is possible to list multiple faces in order of their preference in the Name parameter.  For instance
"Sans Serif,Source Sans,*" will give preference to 'Sans Serif' and will look for 'Source Sans' if the first choice
font is unavailable.  The use of the '*' wildcard indicates that the default system font should be used in the event
that neither of the other choices are available.  Note that omitting this wildcard will raise the likelihood of
ERR_Search being returned in the event that neither of the preferred choices are available.

Flags that alter the search behaviour are FTF_PREFER_SCALED, FTF_PREFER_FIXED and FTF_ALLOW_SCALE.

-INPUT-
cstr Name:  The name of a font face, or multiple faces in CSV format.
cstr Style: The required style, e.g. bold or italic.
int Point:  Preferred point size.
int(FTF) Flags:  Optional flags.
&!cstr Path: The location of the best-matching font file is returned in this parameter.

-ERRORS-
Okay
NullArgs
AccessObject: Unable to access the internal font configuration object.
Search: Unable to find a suitable font.
-END-

*****************************************************************************/

static ERROR fntSelectFont(CSTRING Name, CSTRING Style, LONG Point, LONG Flags, CSTRING *Path)
{
   CSTRING str;
   LONG i, j;
   UBYTE buffer[120], style[60];

   LogBranch("%s:%d:%s, Flags: $%.8x", Name, Point, Style, Flags);

   objConfig *config;
   if (AccessObject(glConfigID, 5000, &config) != ERR_Okay) {
      return LogBackError(0, ERR_AccessObject);
   }

   struct ConfigEntry *entries;
   entries = config->Entries;

   // Find the config section that we should be interested in.  If multiple faces are specified, then
   // up to two fonts can be detected - a fixed bitmap font and a scalable font.

   BYTE multi = FALSE;
   if (Flags & FTF_ALLOW_SCALE) multi = TRUE;
   CSTRING fixed_section = NULL;
   CSTRING scale_section = NULL;
   CSTRING name = Name;
   while ((name) AND (*name)) {
      if (name[0] IS '*') {
         // Use of the '*' wildcard indicates that the default scalable font can be used.  This is is usually
         // accompanied with a fixed font, e.g. "Sans Serif,*"
         multi = TRUE;
         break;
      }

      UWORD pos;
      for (pos=0; pos < config->AmtEntries; pos++) {
         if (!StrMatch(entries[pos].Key, "Name")) {
            if (!name_matches(name, entries[pos].Data)) {
               // Determine if this is a fixed and/or scalable font.  Note that if the font supports
               // both fixed and scalable, fixed_section and scale_section will point to the same font.

               CSTRING section = entries[pos++].Section;
               while ((pos < config->AmtEntries) AND (!StrMatch(section, entries[pos].Section))) {
                  if (!StrCompare("Fixed:", entries[pos].Key, 6, 0)) {
                     if (!fixed_section) {
                        fixed_section = entries[pos].Section;
                        if (scale_section) break;
                     }
                  }
                  else if (!StrCompare("Scale:", entries[pos].Key, 6, 0)) {
                     if (!scale_section) {
                        scale_section = entries[pos].Section;
                        if (fixed_section) break;
                     }
                  }
                  pos++;
               }

               break; // Desired font processed.  Break and process the next font if more than one was specified in the Name.
            }
            else {
               // Not the font that we're looking for.  Skip to the next section.
               while ((pos < config->AmtEntries-1) AND (!StrMatch(entries[pos].Section, entries[pos+1].Section))) pos++;
            }
         }
      }

      if ((fixed_section) OR (scale_section)) break; // Break now if suitable fixed and scalable font settings have been discovered.

      while (*name) { // Try the next name, if any
         if (*name IS ',') {
            multi = TRUE;
            name++;
            while ((*name) AND (*name <= 0x20)) name++;
            break;
         }
         name++;
      }
   }

   if ((!scale_section) AND (!fixed_section)) LogErrorMsg("The font \"%s\" is not installed on this system.", Name);

   //LogErrorMsg("Name: %s, Multi: %d, Fixed: %s, Scale: %s", Name, multi, fixed_section, scale_section);

   if (!scale_section) {
      // Allow use of scalable Source Sans Pro only if multi-face font search was enabled.  Otherwise we presume that
      // auto-upgrading the fixed font is undesirable.
      if ((fixed_section) AND (multi)) {
         static char default_font[60] = "";
         if (!default_font[0]) {
            StrCopy("[glStyle./fonts/font(@name='scalable')/@face]", default_font, sizeof(default_font));
            if (StrEvaluate(default_font, sizeof(default_font), SEF_STRICT, 0) != ERR_Okay) {
               StrCopy("Hera Sans", default_font, sizeof(default_font));
            }
         }

         UWORD pos;
         for (pos=0; pos < config->AmtEntries; pos++) {
            if ((!StrMatch(entries[pos].Key, "Name")) AND (!StrMatch(entries[pos].Data, default_font))) {
               scale_section = entries[pos].Section;
               break;
            }
         }
      }

      if (!fixed_section) { // Sans Serif is a good default for a fixed font.
         UWORD pos;
         for (pos=0; pos < config->AmtEntries; pos++) {
            if ((!StrMatch(entries[pos].Key, "Name")) AND (!StrMatch(entries[pos].Data, "Sans Serif"))) {
               fixed_section = entries[pos].Section;
               break;
            }
         }
      }

      if ((!fixed_section) AND (!scale_section)) {
         ReleaseObject(config);
         LogBack();
         return ERR_Search;
      }
   }

   if ((fixed_section) AND (scale_section) AND (Point)) {
      // Read the point sizes for the fixed section and determine if the requested point size is within 2 units of one
      // of those values.  If not, we'll have to use the scaled font option.

      if (!cfgReadValue(config, fixed_section, "Points", &str)) {
         i = 0;
         BYTE acceptable = FALSE;
         while (str[i]) {
            LONG point = StrToInt(str+i);
            point -= Point;
            if ((point >= -1) AND (point <= 1)) { acceptable = TRUE; break; }
            while (str[i]) {
               if (str[i] IS ',') { i++; break; }
               else i++;
            }
         }

         if (!acceptable) {
            LogMsg("Fixed point font is not a good match, will use scalable font.");
            fixed_section = NULL;
         }
      }
   }

   if (((Point < 12) OR (Flags & FTF_PREFER_FIXED)) AND (!(Flags & FTF_PREFER_SCALED))) {
      if (fixed_section) {
         // Try to find a fixed font first.

         StrFormat(buffer, sizeof(buffer), "Fixed:%s", Style);
         MSG("Looking for a fixed font (%s)...", buffer);
         if (!cfgReadValue(config, fixed_section, buffer, &str)) {
            *Path = StrClone(str);
            ReleaseObject(config);
            LogBack();
            return ERR_Okay;
         }

         // If a stylized version of the font was requested, look for the regular version.

         if (StrMatch("Regular", Style) != ERR_Okay) {
            MSG("Looking for regular version of the font...");
            if (!cfgReadValue(config, fixed_section, "Fixed:Regular", &str)) {
               *Path = StrClone(str);
               ReleaseObject(config);
               LogBack();
               return ERR_Okay;
            }
         }
      }

      // Try for a scaled font

      if ((scale_section) AND (!(Flags & FTF_PREFER_FIXED))) {
         MSG("Looking for a scalable version of the font...");
         StrFormat(buffer, sizeof(buffer), "Scale:%s", Style);
         if (!cfgReadValue(config, scale_section, buffer, &str)) {
            *Path = StrClone(str);
            ReleaseObject(config);
            LogBack();
            return ERR_Okay;
         }

         if (StrMatch("Regular", Style) != ERR_Okay) {
            if (!cfgReadValue(config, scale_section, "Scale:Regular", &str)) {
               *Path = StrClone(str);
               ReleaseObject(config);
               LogBack();
               return ERR_Okay;
            }
         }
      }

      // A regular font style in either format does not exist, so choose the first style that is listed

      MSG("Requested style not supported, choosing first style.");

      if (!cfgReadValue(config, (fixed_section != NULL) ? fixed_section : scale_section, "Styles", &str)) {
         j = 0;
         for (i=0; (str[i]) AND (str[i] != ',') AND (j < sizeof(style)-1); i++) style[j++] = str[i];
         style[j] = 0;

         StrFormat(buffer, sizeof(buffer), "Fixed:%s", style);
         if (!cfgReadValue(config, fixed_section, buffer, &str)) {
            *Path = StrClone(str);
            ReleaseObject(config);
            LogBack();
            return ERR_Okay;
         }

         if (!(Flags & FTF_PREFER_FIXED)) {
            StrFormat(buffer, sizeof(buffer), "Scale:%s", style);
            MSG("Checking for scalable version (%s)", buffer);
            if (!cfgReadValue(config, scale_section, buffer, &str)) {
               *Path = StrClone(str);
               ReleaseObject(config);
               LogBack();
               return ERR_Okay;
            }
         }
      }
   }
   else {
      // Try to find a scalable font first

      MSG("Looking for a scalable font at size %d...", Point);

      if (scale_section) {
         StrFormat(buffer, sizeof(buffer), "Scale:%s", Style);
         if (!cfgReadValue(config, scale_section, buffer, &str)) {
            *Path = StrClone(str);
            ReleaseObject(config);
            LogBack();
            return ERR_Okay;
         }

         if (StrMatch("Regular", Style) != ERR_Okay) {
            if (!cfgReadValue(config, scale_section, "Scale:Regular", &str)) {
               *Path = StrClone(str);
               ReleaseObject(config);
               LogBack();
               return ERR_Okay;
            }
         }
      }

      if ((fixed_section) AND (!(Flags & FTF_PREFER_SCALED))) {
         StrFormat(buffer, sizeof(buffer), "Fixed:%s", Style);
         MSG("Checking for a fixed version of the font '%s'.", buffer);
         if (!cfgReadValue(config, fixed_section, buffer, &str)) {
            *Path = StrClone(str);
            ReleaseObject(config);
            LogBack();
            return ERR_Okay;
         }

         if (StrMatch("Regular", Style) != ERR_Okay) {
            MSG("Checking for a regular style fixed font.");
            if (!cfgReadValue(config, fixed_section, "Fixed:Regular", &str)) {
               *Path = StrClone(str);
               ReleaseObject(config);
               LogBack();
               return ERR_Okay;
            }
         }
      }
      else MSG("User prefers scaled fonts only.");

      // A regular font style in either format does not exist, so choose the first style that is listed

      if (!cfgReadValue(config, (scale_section != NULL) ? scale_section : fixed_section, "Styles", &str)) {
         j = 0;
         for (i=0; (str[i]) AND (str[i] != ',') AND (j < sizeof(style)-1); i++) style[j++] = str[i];
         style[j] = 0;

         MSG("Requested style not supported, using style '%s'", style);

         StrFormat(buffer, sizeof(buffer), "Scale:%s", style);
         if (!cfgReadValue(config, scale_section, buffer, &str)) {
            *Path = StrClone(str);
            ReleaseObject(config);
            LogBack();
            return ERR_Okay;
         }

         if (!(Flags & FTF_PREFER_SCALED)) {
            StrFormat(buffer, sizeof(buffer), "Fixed:%s", style);
            if (!cfgReadValue(config, fixed_section, buffer, &str)) {
               *Path = StrClone(str);
               ReleaseObject(config);
               LogBack();
               return ERR_Okay;
            }
         }
      }
      else MSG("Styles not listed for font '%s'", Name);
   }

   ReleaseObject(config);
   LogBack();
   return ERR_Search;
}

/*****************************************************************************

-INTERNAL-
RefreshFonts: Refreshes the system font list with up-to-date font information.

The Refresh() action scans for fonts that are installed in the system.  Ideally this action should not be necessary
when the InstallFont() and RemoveFont() methods are used correctly, however it can be useful when font files have been
manually deleted or added to the system.

Refreshing fonts can take an extensive amount of time as each font file needs to be completely analysed for information.
Once the analysis is complete, the "SystemFonts" object will be updated and the "fonts.cfg" file will reflect current
font settings.

-ERRORS-
Okay: Fonts were successfully refreshed.
AccessObject: Access to the SytsemFonts object was denied, or the object does not exist.
-END-

*****************************************************************************/

static ERROR fntRefreshFonts(void)
{
   #define MAX_STYLES 20
   STRING section, styles[MAX_STYLES];
   LONG i, pos, stylecount, j;

   LogBranch("Refreshing the fonts: directory.");

   objConfig *config;
   if (AccessObject(glConfigID, 3000, &config) != ERR_Okay) {
      return LogBackError(0, ERR_AccessObject);
   }

   acClear(config); // Clear out existing font information

   scan_fixed_folder(config);
   scan_truetype_folder(config);

   MSG("Sorting the font names.");

   cfgSortByKey(config, NULL, FALSE); // Sort the font names into alphabetical order

   // Create a style list for each font

   MSG("Generating style lists for each font.");

   struct ConfigEntry *entries;
   if ((!GetPointer(config, FID_Entries, &entries)) AND (entries)) {
      section = entries[0].Section;
      stylecount = 0;
      i = 0;
      while (i <= config->AmtEntries) { // Use of <= is important in order to write out the style for the last font
         if ((i < config->AmtEntries) AND (!StrCompare(entries[i].Section, section, 0, STR_MATCH_LEN|STR_CASE))) {
            // If this is a style item, add it to our style list
            if (!StrCompare("Fixed:", entries[i].Key, 6, 0)) {
               if (stylecount < MAX_STYLES-1) styles[stylecount++] = entries[i].Key + 6;
            }
            else if (!StrCompare("Scale:", entries[i].Key, 6, 0)) {
               if (stylecount < MAX_STYLES-1) styles[stylecount++] = entries[i].Key + 6;
            }
            i++;
         }
         else if (stylecount > 0) {
               UBYTE buffer[300];
               UBYTE sectionstr[80];

               // Write the style list to the font configuration

               styles[stylecount] = NULL;
               StrSort(styles, SBF_NO_DUPLICATES);

               pos = 0;
               for (j=0; styles[j]; j++) {
                  if ((pos > 0) AND (pos < sizeof(buffer)-1)) buffer[pos++] = ',';
                  pos += StrCopy(styles[j], buffer+pos, sizeof(buffer)-pos);
               }

               StrCopy(section, sectionstr, sizeof(sectionstr));

               cfgWriteValue(config, sectionstr, "Styles", buffer);

               // Reset the config index since we added a new entry to the object

               if (GetPointer(config, FID_Entries, &entries)) break;

               for (i=0; i < config->AmtEntries; i++) {
                  if (StrCompare(entries[i].Section, sectionstr, 0, STR_MATCH_LEN|STR_CASE) IS ERR_Okay) {
                     while ((i < config->AmtEntries) AND (StrCompare(entries[i].Section, sectionstr, 0, STR_MATCH_LEN|STR_CASE) IS ERR_Okay)) i++;
                     if (i < config->AmtEntries) section = entries[i].Section;
                     break;
                  }
               }

               stylecount = 0;
         }
         else if (i < config->AmtEntries) {
            LogErrorMsg("No styles listed for font %s", section);
            section = entries[i].Section;
            i++;
         }
         else i++;
      }
   }

   // Save the font configuration file

   MSG("Saving the font configuration file.");

   OBJECTPTR file;
   if (!CreateObject(ID_FILE, 0, &file,
         FID_Path|TSTR,   "config:fonts.cfg",
         FID_Flags|TLONG, FL_NEW|FL_WRITE,
         TAGEND)) {
      acSaveToObject(config, file->UniqueID, 0);
      acFree(file);
   }

   ReleaseObject(config);
   LogBack();
   return ERR_Okay;
}

//****************************************************************************

static void scan_truetype_folder(objConfig *Config)
{
   struct DirInfo *dir;
   LONG j;
   char location[100];
   char section[200];
   FT_Face ftface;
   FT_Open_Args open;

   LogBranch("Scanning for truetype fonts.");

   if (!OpenDir("fonts:truetype/", RDF_FILE, &dir)) {
      while (!ScanDir(dir)) {
         StrFormat(location, sizeof(location), "fonts:truetype/%s", dir->Info->Name);

         for (j=0; location[j]; j++);
         while ((j > 0) AND (location[j-1] != '.') AND (location[j-1] != ':') AND (location[j-1] != '/') AND (location[j-1] != '\\')) j--;

         ResolvePath(location, 0, (STRING *)&open.pathname);
         open.flags    = FT_OPEN_PATHNAME;
         if (!FT_Open_Face(glFTLibrary, &open, 0, &ftface)) {
            FreeMemory(open.pathname);

            LogMsg("Detected font file \"%s\", name: %s, style: %s", location, ftface->family_name, ftface->style_name);

            LONG n;
            if (ftface->family_name) n = StrCopy(ftface->family_name, section, sizeof(section));
            else {
               n = 0;
               while ((j > 0) AND (location[j-1] != ':') AND (location[j-1] != '/') AND (location[j-1] != '\\')) j--;
               while ((location[j]) AND (location[j] != '.')) section[n++] = location[j++];
            }
            section[n] = 0;

            // Strip any style references out of the font name and keep them as style flags

            LONG style = 0;
            if (ftface->style_name) {
               if ((n = StrSearch(" Bold", section, STR_MATCH_CASE)) != -1) {
                  for (j=0; " Bold"[j]; j++) section[n++] = ' ';
                  style |= FTF_BOLD;
               }

               if ((n = StrSearch(" Italic", section, STR_MATCH_CASE)) != -1) {
                  for (j=0; " Italic"[j]; j++) section[n++] = ' ';
                  style |= FTF_ITALIC;
               }
            }

            for (n=0; section[n]; n++);
            while ((n > 0) AND (section[n-1] <= 0x20)) n--;
            section[n] = 0;

            cfgWriteValue(Config, section, "Name", section);

            if (FT_IS_SCALABLE(ftface)) cfgWriteValue(Config, section, "Scalable", "Yes");

            // Add the style with a link to the font file location

            if (FT_IS_SCALABLE(ftface)) {
               if ((ftface->style_name) AND (StrMatch("regular", ftface->style_name) != ERR_Okay)) {
                  char buffer[200];
                  CharCopy("Scale:", buffer, sizeof(buffer));
                  StrCopy(ftface->style_name, buffer+6, sizeof(buffer)-6);
                  cfgWriteValue(Config, section, buffer, location);
               }
               else {
                  if (style IS FTF_BOLD) cfgWriteValue(Config, section, "Scale:Bold", location);
                  else if (style IS FTF_ITALIC) cfgWriteValue(Config, section, "Scale:Italic", location);
                  else if (style IS (FTF_BOLD|FTF_ITALIC)) cfgWriteValue(Config, section, "Scale:Bold Italic", location);
                  else cfgWriteValue(Config, section, "Scale:Regular", location);
               }
            }

            FT_Done_Face(ftface);
         }
         else {
            FreeMemory(open.pathname);
            LogErrorMsg("Failed to analyse scalable font file \"%s\".", location);
         }
      }

      CloseDir(dir);
   }
   else LogErrorMsg("Failed to open the fonts:truetype/ directory.");

   LogBack();
}

//****************************************************************************

static void scan_fixed_folder(objConfig *Config)
{
   LONG j, n, style;
   WORD i;
   UBYTE location[100], section[200], points[20], pntbuffer[80];
   STRING facename;

   LogBranch("Scanning for fixed fonts.");

   UBYTE bold = FALSE;
   UBYTE bolditalic = FALSE;
   UBYTE italic = FALSE;

   struct DirInfo *dir;
   if (!OpenDir("fonts:fixed/", RDF_FILE, &dir)) {
      while (!ScanDir(dir)) {
         StrFormat(location, sizeof(location), "fonts:fixed/%s", dir->Info->Name);

         struct winfnt_header_fields header;
         if (!analyse_bmp_font(location, &header, &facename, points, ARRAYSIZE(points))) {
            LogF("6Font:","Detected font file \"%s\", name: %s", location, facename);

            if (!facename) continue;
            StrCopy(facename, section, sizeof(section));

            // Strip any style references out of the font name and keep them as style flags

            style = 0;
            if ((n = StrSearch(" Bold", section, STR_MATCH_CASE)) != -1) {
               for (j=0; " Bold"[j]; j++) section[n++] = ' ';
               style |= FTF_BOLD;
            }

            if ((n = StrSearch(" Italic", section, STR_MATCH_CASE)) != -1) {
               for (j=0; " Italic"[j]; j++) section[n++] = ' ';
               style |= FTF_ITALIC;
            }

            if (header.italic) style |= FTF_ITALIC;
            if (header.weight >= 600) style |= FTF_BOLD;

            for (n=0; section[n]; n++);
            while ((n > 0) AND (section[n-1] <= 0x20)) n--;
            section[n] = 0;

            cfgWriteValue(Config, section, "Name", section);

            // Add the style with a link to the font file location

            if (style IS FTF_BOLD) {
               cfgWriteValue(Config, section, "Fixed:Bold", location);
               bold = TRUE;
            }
            else if (style IS FTF_ITALIC) {
               cfgWriteValue(Config, section, "Fixed:Italic", location);
               italic = TRUE;
            }
            else if (style IS (FTF_BOLD|FTF_ITALIC)) {
               cfgWriteValue(Config, section, "Fixed:Bold Italic", location);
               bolditalic = TRUE;
            }
            else {
               cfgWriteValue(Config, section, "Fixed:Regular", location);
               if (bold IS FALSE)       cfgWriteValue(Config, section, "Fixed:Bold", location);
               if (bolditalic IS FALSE) cfgWriteValue(Config, section, "Fixed:Bold Italic", location);
               if (italic IS FALSE)     cfgWriteValue(Config, section, "Fixed:Italic", location);
            }

            j = 0;
            for (i=0; points[i]; i++) {
               if (i > 0) pntbuffer[j++] = ',';
               j += IntToStr(points[i], pntbuffer+j, sizeof(pntbuffer)-j-2);
            }

            pntbuffer[j] = 0;
            cfgWriteValue(Config, section, "Points", pntbuffer);

            FreeMemory(facename);
         }
         else LogErrorMsg("Failed to analyse %s", location);
      }
      CloseDir(dir);
   }
   else LogErrorMsg("Failed to scan directory fonts:fixed/");

   LogBack();
}

//****************************************************************************

static ERROR analyse_bmp_font(STRING Path, struct winfnt_header_fields *Header, STRING *FaceName, UBYTE *Points, UBYTE MaxPoints)
{
   struct winmz_header_fields mz_header;
   struct winne_header_fields ne_header;
   OBJECTPTR file;
   LONG i, res_offset, font_offset;
   UWORD size_shift, font_count, type_id, count;
   UBYTE face[50];

   if ((!Path) OR (!Header) OR (!FaceName)) return ERR_NullArgs;

   *FaceName = NULL;
   if (!CreateObject(ID_FILE, 0, &file,
         FID_Path|TSTR,   Path,
         FID_Flags|TLONG, FL_READ,
         TAGEND)) {

      acRead(file, &mz_header, sizeof(mz_header), NULL);

      if (mz_header.magic IS ID_WINMZ) {
         acSeekStart(file, mz_header.lfanew);

         if ((!acRead(file, &ne_header, sizeof(ne_header), NULL)) AND (ne_header.magic IS ID_WINNE)) {
            res_offset = mz_header.lfanew + ne_header.resource_tab_offset;
            acSeekStart(file, res_offset);

            font_count  = 0;
            font_offset = 0;
            size_shift  = ReadWordLE(file);

            for (type_id=ReadWordLE(file); type_id; type_id=ReadWordLE(file)) {
               count = ReadWordLE(file);

               if (type_id IS 0x8008) {
                  font_count  = count;
                  GetLong(file, FID_Position, &font_offset);
                  font_offset = font_offset + 4;
                  break;
               }

               acSeekCurrent(file, 4 + count * 12);
            }

            if ((!font_count) OR (!font_offset)) {
               LogErrorMsg("There are no fonts in file \"%s\"", Path);
               acFree(file);
               return ERR_Failed;
            }

            acSeekStart(file, font_offset);

            {
               struct winFontList fonts[font_count];

               // Get the offset and size of each font entry

               for (i=0; i < font_count; i++) {
                  fonts[i].Offset = ReadWordLE(file)<<size_shift;
                  fonts[i].Size   = ReadWordLE(file)<<size_shift;
                  acSeekCurrent(file, 8);
               }

               // Read font point sizes

               for (i=0; (i < font_count) AND (i < MaxPoints-1); i++) {
                  acSeekStart(file, fonts[i].Offset);
                  if (acRead(file, Header, sizeof(struct winfnt_header_fields), NULL) IS ERR_Okay) {
                     Points[i] = Header->nominal_point_size;
                  }
               }
               Points[i] = 0;

               // Go to the first font in the file and read the font header

               acSeekStart(file, fonts[0].Offset);

               if (acRead(file, Header, sizeof(struct winfnt_header_fields), NULL) != ERR_Okay) {
                  acFree(file);
                  return ERR_Read;
               }

                // NOTE: 0x100 indicates the Microsoft vector font format, which we do not support.

               if ((Header->version != 0x200) AND (Header->version != 0x300)) {
                  LogErrorMsg("Font \"%s\" is written in unsupported version %d / $%x.", Path, Header->version, Header->version);
                  acFree(file);
                  return ERR_NoSupport;
               }

               if (Header->file_type & 1) {
                  LogErrorMsg("Font \"%s\" is in the non-supported vector font format.", Path);
                  acFree(file);
                  return ERR_NoSupport;
               }

               // Extract the name of the font

               acSeekStart(file, fonts[0].Offset + Header->face_name_offset);

               for (i=0; i < sizeof(face)-1; i++) {
                  if ((acRead(file, face+i, 1, NULL) != ERR_Okay) OR (!face[i])) {
                     break;
                  }
               }
               face[i] = 0;
               *FaceName = StrClone(face);
            }

            acFree(file);
            return ERR_Okay;
         }

         acFree(file);
         return ERR_NoSupport;
      }

      acFree(file); // File is not a windows fixed font
      return ERR_NoSupport;
   }
   else return ERR_File;
}

//****************************************************************************

#include "class_font.c"

//****************************************************************************

PARASOL_MOD(CMDInit, NULL, CMDOpen, CMDExpunge, MODVERSION_FONT)
