/*********************************************************************************************************************

The source code of the Parasol project is made publicly available under the terms described in the LICENSE.TXT file
that is distributed with this package.  Please refer to it for further information on licensing.

**********************************************************************************************************************

-CLASS-
Font: Draws bitmap fonts and manages font meta information.

The Font class is provided for the purpose of bitmap font rendering and querying font meta information.  It supports
styles such as bold, italic and underlined text, along with extra features such as adjustable spacing and word
alignment. Fixed-point bitmap fonts are supported through the Windows .fon file format and TrueType font files are
supported as scaled fonts.  Drawing Truetype fonts is not supported (refer to the @VectorText class for this feature).

Fonts must be stored in the `fonts:` directory in order to be recognised and either in the "fixed" or "truetype"
sub-directories as appropriate.  The process of font installation and file management is managed by functions supplied
in the Font module.

The Font class includes full support for the unicode character set through its support for UTF-8.  This gives you the
added benefit of being able to support international character sets with ease, but you must be careful not to use
character codes above #127 without being sure that they follow UTF-8 guidelines.  Find out more about UTF-8 at
this <a href="http://www.cl.cam.ac.uk/~mgk25/unicode.html">web page</a>.

Initialisation of a new font object can be as simple as declaring its #Point size and #Face name.  Font objects can
be difficult to alter post-initialisation, so all style and graphical selections must be defined on creation.  For
example, it is not possible to change styling from regular to bold format dynamically.  To support multiple styles
of the same font, create a font object for every style that requires support.  Basic settings such as colour, the
font string and text positioning are not affected by these limitations.

To draw a font string to a Bitmap object, start by setting the #Bitmap and #String fields.  The #X and #Y fields
determine string positioning and you can also use the #Align field to position a string to the right or center of the
surface area.

To clarify the terminology used in this documentation, please note the definitions for the following terms:

<list type="unsorted">
<li>'Point' determines the size of a font.  The value is relative only to other point sizes of the same font face, i.e. two faces at the same point size are not necessarily the same height.</li>
<li>'Height' represents the 'vertical bearing' or point of the font, expressed as a pixel value.  The height does not cover for any leading at the top of the font, or the gutter space used for the tails on characters like 'g' and 'y'.</li>
<li>'Gutter' is the amount of space that a character can descend below the base line.  Characters like 'g' and 'y' are examples of characters that utilise the gutter space.  The gutter is also sometimes known as the 'external leading' or 'descent' of a character.</li>
<li>'LineSpacing' is the recommended pixel distance between each line that is printed with the font.</li>
<li>'Glyph' refers to a single font character.</li>
</>

Please note that in the majority of cases the @VectorText class should be used for drawing strings because the Font
class is not integrated with the display's vector scene graph.

-END-

*********************************************************************************************************************/

static BitmapCache * check_bitmap_cache(extFont *, FTF);
static ERROR cache_truetype_font(extFont *, CSTRING);
static ERROR SET_Point(extFont *, Variable *);
static ERROR SET_Style(extFont *, CSTRING );

const char * get_ft_error(FT_Error err)
{
    #undef __FTERRORS_H__
    #define FT_ERRORDEF( e, v, s )  case e: return s;
    #define FT_ERROR_START_LIST     switch (err) {
    #define FT_ERROR_END_LIST       }
    #include FT_ERRORS_H
    return "(Unknown error)";
}

/*********************************************************************************************************************

-ACTION-
Draw: Draws a font to a Bitmap.

Draws a font to a target Bitmap, starting at the coordinates of #X and #Y, using the characters in the font #String.

-ERRORS-
Okay
FieldNotSet: The Bitmap and/or String field has not been set.
-END-

*********************************************************************************************************************/

static ERROR draw_bitmap_font(extFont *);

static ERROR FONT_Draw(extFont *Self, APTR Void)
{
   return draw_bitmap_font(Self);
}

//********************************************************************************************************************

static ERROR FONT_Free(extFont *Self, APTR Void)
{
   pf::Log log;

   CACHE_LOCK lock(glCacheMutex);

   if (Self->BmpCache) {
      // Reduce the usage count.  Use a timed delay on freeing the font in case it is used again.
      Self->BmpCache->OpenCount--;
      if (!Self->BmpCache->OpenCount) {
         if (!glCacheTimer) {
            pf::SwitchContext ctx(modFont);
            SubscribeTimer(60.0, FUNCTION(bitmap_cache_cleaner), &glCacheTimer);
         }
      }
   }

   // Manage the vector font cache

   if (Self->Cache.get()) {
      unload_glyph_cache(Self);

      if (!(--Self->Cache->Usage)) {
         log.trace("Font face usage reduced to %d.", Self->Cache->Usage);
         glCache.erase(Self->Cache->Path); // This will trigger the item's destructor
      }
   }

   if (Self->prvTempGlyph.Outline) { FreeResource(Self->prvTempGlyph.Outline); Self->prvTempGlyph.Outline = NULL; }
   if (Self->Path)    { FreeResource(Self->Path); Self->Path = NULL; }

   Self->~extFont();

   return ERR_Okay;
}

//********************************************************************************************************************

static ERROR FONT_Init(extFont *Self, APTR Void)
{
   pf::Log log;
   LONG diff;
   FTF style;
   ERROR error;

   if ((!Self->prvFace[0]) and (!Self->Path)) {
      log.warning("Face not defined.");
      return ERR_FieldNotSet;
   }

   if (!Self->Point) Self->Point = global_point_size();

   if (!Self->Path) {
      CSTRING path = NULL;
      if (!fntSelectFont(Self->prvFace, Self->prvStyle, Self->Point, Self->Flags & (FTF::PREFER_SCALED|FTF::PREFER_FIXED|FTF::ALLOW_SCALE), &path)) {
         Self->set(FID_Path, path);
         FreeResource(path);
      }
      else {
         log.warning("Font \"%s\" (point %.2f, style %s) is not recognised.", Self->prvFace, Self->Point, Self->prvStyle);
         return ERR_Failed;
      }
   }

   // Check the bitmap cache to see if we have already loaded this font

   if (!StrMatch("Bold", Self->prvStyle)) style = FTF::BOLD;
   else if (!StrMatch("Italic", Self->prvStyle)) style = FTF::ITALIC;
   else if (!StrMatch("Bold Italic", Self->prvStyle)) style = FTF::BOLD|FTF::ITALIC;
   else style = FTF::NIL;

   CACHE_LOCK lock(glCacheMutex);

   BitmapCache *cache = check_bitmap_cache(Self, style);

   if (cache); // The font exists in the cache
   else if (!StrCompare("*.ttf", Self->Path, 0, STR::WILDCARD)); // The font is truetype
   else {
      objFile::create file = { fl::Path(Self->Path), fl::Flags(FL::READ|FL::APPROXIMATE) };
      if (file.ok()) {
         // Check if the file is a Windows Bitmap Font

         winmz_header_fields mz_header;
         file->read(&mz_header, sizeof(mz_header));

         if (mz_header.magic IS ID_WINMZ) {
            file->seek(mz_header.lfanew, SEEK::START);

            winne_header_fields ne_header;
            if ((!file->read(&ne_header, sizeof(ne_header))) and (ne_header.magic IS ID_WINNE)) {
               ULONG res_offset = mz_header.lfanew + ne_header.resource_tab_offset;
               file->seek(res_offset, SEEK::START);

               // Count the number of fonts in the file

               WORD size_shift = 0;
               UWORD font_count = 0;
               LONG font_offset = 0;
               flReadLE(*file, &size_shift);

               WORD type_id;
               for ((error = flReadLE(*file, &type_id)); (!error) and (type_id); error = flReadLE(*file, &type_id)) {
                  WORD count = 0;
                  flReadLE(*file, &count);

                  if ((UWORD)type_id IS 0x8008) {
                     font_count  = count;
                     file->get(FID_Position, &font_offset);
                     font_offset += 4;
                     break;
                  }

                  file->seek(4 + (count * 12), SEEK::CURRENT);
               }

               if ((!font_count) or (!font_offset)) {
                  log.warning("There are no fonts in the file \"%s\"", Self->Path);
                  return ERR_Failed;
               }

               file->seek(font_offset, SEEK::START);

               // Scan the list of available fonts to find the closest point size for our font

               auto fonts = std::make_unique<winFont[]>(font_count);

               for (LONG i=0; i < font_count; i++) {
                  UWORD offset, size;
                  flReadLE(*file, &offset);
                  flReadLE(*file, &size);
                  fonts[i].Offset = offset<<size_shift;
                  fonts[i].Size   = size<<size_shift;
                  file->seek(8, SEEK::CURRENT);
               }

               LONG abs = 0x7fff;
               LONG wfi = 0;
               winfnt_header_fields face;
               for (LONG i=0; i < font_count; i++) {
                  file->seek((DOUBLE)fonts[i].Offset, SEEK::START);

                  winfnt_header_fields header;
                  if (!file->read(&header, sizeof(header))) {
                     if ((header.version != 0x200) and (header.version != 0x300)) {
                        log.warning("Font \"%s\" is written in unsupported version %d.", Self->prvFace, header.version);
                        return ERR_Failed;
                     }

                     if (header.file_type & 1) {
                        log.warning("Font \"%s\" is in the non-supported vector font format.", Self->prvFace);
                        return ERR_Failed;
                     }

                     if (header.pixel_width <= 0) header.pixel_width = header.pixel_height;

                     if ((diff = Self->Point - header.nominal_point_size) < 0) diff = -diff;

                     if (diff < abs) {
                        face = header;
                        abs  = diff;
                        wfi  = i;
                     }
                  }
                  else return log.warning(ERR_Read);
               }

               // Check the bitmap cache again to ensure that the discovered font is not already loaded.  This is important
               // if the cached font wasn't originally found due to variation in point size.

               Self->Point = face.nominal_point_size;
               cache = check_bitmap_cache(Self, style);
               if (!cache) { // Load the font into the cache
                  auto it = glBitmapCache.emplace(glBitmapCache.end(), face, Self->prvStyle, Self->Path, *file, fonts[wfi]);

                  if (!it->Result) cache = &(*it);
                  else {
                     ERROR error = it->Result;
                     glBitmapCache.erase(it);
                     return error;
                  }
               }
            } // File is not a windows fixed font (but could be truetype)
         } // File is not a windows fixed font (but could be truetype)
      }
      else return log.warning(ERR_OpenFile);
   }

   if (cache) {
      Self->prvData     = cache->mData;
      Self->Ascent      = cache->Header.ascent;
      Self->Point       = cache->Header.nominal_point_size;
      Self->Height      = cache->Header.ascent - cache->Header.internal_leading + cache->Header.external_leading;
      Self->Leading     = cache->Header.internal_leading;
      Self->Gutter      = cache->Header.external_leading;
      if (!Self->Gutter) Self->Gutter = cache->Header.pixel_height - Self->Height - cache->Header.internal_leading;
      Self->LineSpacing += cache->Header.pixel_height; // Add to any preset linespacing rather than over-riding
      Self->MaxHeight   = cache->Header.pixel_height; // Supposedly the pixel_height includes internal and external leading values (?)
      Self->prvBitmapHeight = cache->Header.pixel_height;
      Self->prvDefaultChar  = cache->Header.first_char + cache->Header.default_char;
      Self->TotalGlyphs     = cache->Header.last_char - cache->Header.first_char + 1;

      // If this is a monospaced font, set the FixedWidth field

      if (cache->Header.avg_width IS cache->Header.max_width) {
         Self->FixedWidth = cache->Header.avg_width;
      }

      if (Self->FixedWidth > 0) Self->prvSpaceWidth = Self->FixedWidth;
      else if (cache->Chars[' '].Advance) Self->prvSpaceWidth = cache->Chars[' '].Advance;
      else Self->prvSpaceWidth = cache->Chars[cache->Header.first_char + cache->Header.break_char].Advance;

      log.trace("Cache Count: %d, Style: %s", cache->OpenCount, Self->prvStyle);

      Self->prvChar = cache->Chars;
      Self->Flags |= cache->StyleFlags;

      cache->OpenCount++;

      Self->BmpCache = cache;
   }
   else {
      if ((error = cache_truetype_font(Self, Self->Path))) return error;

      if (FT_HAS_KERNING(Self->Cache->Face)) Self->Flags |= FTF::KERNING;
      Self->Flags |= FTF::SCALABLE;
   }

   // Remove the location string to reduce resource usage

   if (Self->Path) { FreeResource(Self->Path); Self->Path = NULL; }

   log.extmsg("Family: %s, Style: %s, Glyphs: %d, Point: %.2f, Height: %d", Self->prvFace, Self->prvStyle, Self->TotalGlyphs, Self->Point, Self->Height);
   log.trace("LineSpacing: %d, Leading: %d, Gutter: %d", Self->LineSpacing, Self->Leading, Self->Gutter);

   return ERR_Okay;
}

//********************************************************************************************************************

static ERROR FONT_NewObject(extFont *Self, APTR Void)
{
   new (Self) extFont;

   update_dpi(); // A good time to check the DPI is whenever a new font is created.

   Self->TabSize         = 8;
   Self->prvDefaultChar  = '.';
   Self->prvLineCountCR  = 1;
   Self->Style           = Self->prvStyle;
   Self->Face            = Self->prvFace;
   Self->Colour.Alpha    = 255;
   Self->GlyphSpacing    = 1.0;
   StrCopy("Regular", Self->prvStyle, sizeof(Self->prvStyle));
   return ERR_Okay;
}

/*********************************************************************************************************************

-FIELD-
Align: Sets the position of a font string to an abstract alignment.

Use this field to set the alignment of a font string within its surface area.  This is an abstract means of positioning
in comparison to setting the X and Y fields directly.

-FIELD-
AlignHeight: The height to use when aligning the font string.

If the `VERTICAL` or `TOP` alignment options are used in the #Align field, the AlignHeight should be set so
that the alignment of the font string can be correctly calculated.  If the AlignHeight is not defined, the target
#Bitmap's height will be used when computing alignment.

-FIELD-
AlignWidth: The width to use when aligning the font string.

If the `HORIZONTAL` or `RIGHT` alignment options are used in the #Align field, the AlignWidth should be set so
that the alignment of the font string can be correctly calculated.  If the AlignWidth is not defined, the target
#Bitmap's width will be used when computing alignment.

-FIELD-
Ascent: The total number of pixels above the baseline.

The Ascent value reflects the total number of pixels above the baseline, including the #Leading value.

-FIELD-
Bitmap: The destination Bitmap to use when drawing a font.

-FIELD-
Bold: Set to TRUE to enable bold styling.

Setting the Bold field to TRUE prior to initialisation will enable bold styling.  This field is provided only for
convenience - we recommend that you set the Style field for determining font styling where possible.

*********************************************************************************************************************/

static ERROR GET_Bold(extFont *Self, LONG *Value)
{
   if ((Self->Flags & FTF::BOLD) != FTF::NIL) *Value = TRUE;
   else if (StrSearch("bold", Self->prvStyle) != -1) *Value = TRUE;
   else *Value = FALSE;
   return ERR_Okay;
}

static ERROR SET_Bold(extFont *Self, LONG Value)
{
   if (Self->initialised()) {
      // If the font is initialised, setting the bold style is implicit
      return SET_Style(Self, "Bold");
   }
   else if ((Self->Flags & FTF::ITALIC) != FTF::NIL) {
      return SET_Style(Self, "Bold Italic");
   }
   else return SET_Style(Self, "Bold");
}

/*********************************************************************************************************************

-FIELD-
Colour: The font colour in RGB format.

-FIELD-
EndX: Indicates the final horizontal coordinate after completing a draw operation.

The EndX and EndY fields reflect the final coordinate of the font #String, following the most recent call to
the #Draw() action.

-FIELD-
EndY: Indicates the final vertical coordinate after completing a draw operation.

The EndX and EndY fields reflect the final coordinate of the font #String, following the most recent call to
the #Draw() action.

-FIELD-
Face: The name of a font face that is to be loaded on initialisation.

The name of an installed font face must be specified here for initialisation.  If this field is not set then the
initialisation process will use the user's preferred face.  A list of available faces can be obtained from the Font
module's ~Font.GetList() function.

For convenience, the face string can also be extended with extra parameters so that the point size and style are
defined at the same time.  Extra parameters are delimited with the colon character and must follow a set order
defined as `face:pointsize:style:colour`.

Here are some examples:

<pre>
Open Sans:12:Bold Italic:#ff0000
Courier:10.6
Charter:120%::255,128,255
</pre>

To load a font file that is not installed by default, replace the face parameter with the SRC command, followed by the
font location: `SRC:volumename:data/images/shine:14:Italic`

Multiple font faces can be specified in CSV format, e.g. `Sans Serif,Open Sans`, which allows the closest matching font to
be selected if the first face is unavailable or unable to match the requested point size.  This feature can be very
useful for pairing bitmap fonts with a scalable equivalent.

*********************************************************************************************************************/

static ERROR SET_Face(extFont *Self, CSTRING Value)
{
   LONG i, j;

   if ((Value) and (Value[0])) {
      if (!StrCompare("SRC:", Value, 4)) {
         std::ostringstream path;
         LONG coloncount = 0;
         for (i=4; Value[i]; i++) {
            if (Value[i] IS ':') {
               coloncount++;
               if (coloncount > 1) break;
            }
            path << Value[i];
         }
         Self->Path = StrClone(path.str().c_str());
         Self->prvFace[0] = 0;
      }
      else {
         for (i=0; (Value[i]) and (Value[i] != ':') and (i < std::ssize(Self->prvFace)-1); i++) Self->prvFace[i] = Value[i];
         Self->prvFace[i] = 0;
      }

      if (Value[i] != ':') return ERR_Okay;

      // Extract the point size

      i++;
      Variable var(StrToFloat(Value+i));
      while ((Value[i] >= '0') and (Value[i] <= '9')) i++;
      if (Value[i] IS '.') {
         Value++;
         while ((Value[i] >= '0') and (Value[i] <= '9')) i++;
      }
      if (Value[i] IS '%') { var.Type |= FD_SCALED; i++; }
      SET_Point(Self, &var);

      if (Value[i] != ':') return ERR_Okay;

      // Extract the style string

      i++;
      for (j=0; (Value[i]) and (Value[i] != ':') and (j < std::ssize(Self->prvStyle)-1); j++) Self->prvStyle[j] = Value[i++];
      Self->prvStyle[j] = 0;

      if (Value[i] != ':') return ERR_Okay;

      // Extract the colour string

      i++;
      Self->set(FID_Colour, Value + i);
   }
   else Self->prvFace[0] = 0;

   return ERR_Okay;
}

/*********************************************************************************************************************

-FIELD-
FixedWidth: Forces a fixed pixel width to use for all glyphs.

The FixedWidth value imposes a preset pixel width for all glyphs in the font.  It is important to note that if the
fixed width value is less than the widest glyph, the glyphs will overlap each other.

-FIELD-
Flags:  Optional flags.

*********************************************************************************************************************/

static ERROR SET_Flags(extFont *Self, FTF Value)
{
   Self->Flags = (Self->Flags & FTF(0xff000000)) | (Value & FTF(0x00ffffff));
   return ERR_Okay;
}

/*********************************************************************************************************************

-FIELD-
Gutter: The 'external leading' value, measured in pixels.  Applies to fixed fonts only.

This field reflects the 'external leading' value (also known as the 'gutter'), measured in pixels.  It is typically
defined by fixed bitmap fonts.  For truetype fonts the gutter is derived from the calculation `LineSpacing - Ascent`.

-FIELD-
Height: The point size of the font, expressed in pixels.

The point size of the font is expressed in this field as a pixel measurement.  It does not not include the leading value
(refer to #Ascent if leading is required).

The height is calculated on initialisation and can be read at any time.

-FIELD-
GlyphSpacing: Adjusts the amount of spacing between each character.

This field adjusts the horizontal spacing between each glyph, technically known as kerning between each font
character.  The value is expressed as a multiplier of the width of each character, and defaults to `1.0`.

Using negative values is valid, and can lead to text being printed backwards.

-FIELD-
Italic: Set to TRUE to enable italic styling.

Setting the Italic field to TRUE prior to initialisation will enable italic styling.  This field is provided for
convenience only - we recommend that you set the Style field for determining font styling where possible.

*********************************************************************************************************************/

static ERROR GET_Italic(extFont *Self, LONG *Value)
{
   if ((Self->Flags & FTF::ITALIC) != FTF::NIL) *Value = TRUE;
   else if (StrSearch("italic", Self->prvStyle) != -1) *Value = TRUE;
   else *Value = FALSE;
   return ERR_Okay;
}

static ERROR SET_Italic(extFont *Self, LONG Value)
{
   if (Self->initialised()) {
      // If the font is initialised, setting the italic style is implicit
      return SET_Style(Self, "Italic");
   }
   else if ((Self->Flags & FTF::BOLD) != FTF::NIL) {
      return SET_Style(Self, "Bold Italic");
   }
   else return SET_Style(Self, "Italic");
}

/*********************************************************************************************************************

-FIELD-
Leading: 'Internal leading' measured in pixels.  Applies to fixed fonts only.

-FIELD-
LineCount: The total number of lines in a font string.

This field indicates the number of lines that are present in a font's String field.  If word wrapping is enabled, this
will be taken into account in the resulting figure.

*********************************************************************************************************************/

static ERROR GET_LineCount(extFont *Self, LONG *Value)
{
   if (!Self->prvLineCount) calc_lines(Self);
   *Value = Self->prvLineCount;
   return ERR_Okay;
}

/*********************************************************************************************************************

-FIELD-
LineSpacing: The amount of spacing between each line.

This field defines the amount of space between each line that is printed with a font object.  It is set automatically
during initialisation to reflect the recommended distance between each line.   The client can increase or decrease
this value to make finer adjustments to the line spacing.  If negative, the text will be printed in a reverse vertical
direction with each new line.

If set prior to initialisation, the value will be added to the font's normal line-spacing instead of over-riding it.
For instance, setting the LineSpacing to 2 will result in an extra 2 pixels being added to the font's spacing.

-FIELD-
Path: The path to a font file.

This field can be defined prior to initialisation.  It can be used to refer to the exact location of a font data file,
in opposition to the normal practice of loading fonts that are installed on the host system.

This feature is ideal for use when distributing custom fonts with an application.

*********************************************************************************************************************/

static ERROR SET_Path(extFont *Self, CSTRING Value)
{
   if (!Self->initialised()) {
      if (Self->Path) { FreeResource(Self->Path); Self->Path = NULL; }
      if (Value) Self->Path = StrClone(Value);
      return ERR_Okay;
   }
   else return ERR_Failed;
}

/*********************************************************************************************************************

-FIELD-
MaxHeight: The maximum possible pixel height per character.

This field reflects the maximum possible pixel height per character, covering the entire character set at the current
point size.

-FIELD-
Opacity: Determines the level of translucency applied to a font.

This field determines the translucency level of a font graphic.  The default setting is 100%, which means that the font
will not be translucent.  Any other value set here will alter the impact of a font's graphic over the destination
Bitmap.  High values will retain the boldness of the font, while low values can render it close to invisible.

Please note that the use of translucency will always have an impact on the time it normally takes to draw a font.

*********************************************************************************************************************/

static ERROR GET_Opacity(extFont *Self, DOUBLE *Value)
{
   *Value = (Self->Colour.Alpha * 100)>>8;
   return ERR_Okay;
}

static ERROR SET_Opacity(extFont *Self, DOUBLE Value)
{
   if (Value >= 100) Self->Colour.Alpha = 255;
   else if (Value <= 0) Self->Colour.Alpha = 0;
   else Self->Colour.Alpha = F2T(Value * (255.0 / 100.0));
   return ERR_Okay;
}

/*********************************************************************************************************************

-FIELD-
Outline: Defines the outline colour around a font.

An outline can be drawn around a font by setting the Outline field to an RGB colour.  The outline can be turned off by
writing this field with a NULL value or setting the alpha component to zero.  Outlining is currently supported for
bitmap fonts only.

-FIELD-
Point: The point size of a font.

The point size of a font defines the size of a font, relative to other point sizes for a particular font face.  For
example, Arial point 8 is half the size of Arial point 16.  The point size between font families cannot be compared
accurately due to designer discretion when it comes to determining font size.  For accurate point size in terms of
pixels, please refer to the Height field.

The unit of measure for point size is dependent on the target display.  For video displays, the point size translates
directly into pixels.  When printing however, point size will translate to a certain number of dots on the page (the
exact number of dots will depend on the printer device and final DPI).

The Point field also supports proportional sizing based on the default value set by the system or user.  For instance
if a Point value of 1.5 is specified and the default font size is 10, the final point size for the font will be 15.
This feature is very important in order to support multiple devices at varying DPI's - i.e. mobile devices.  You can
change the global point size for your application by calling ~Font.SetDefaultSize() in the Font module.

When setting the point size of a bitmap font, the system will try and find the closest matching value for the requested
point size.  For instance, if you request a fixed font at point 11 and the closest size is point 8, the system will
drop the font to point 8.  This does not impact upon scalable fonts, which can be measured to any point size.

*********************************************************************************************************************/

static ERROR GET_Point(extFont *Self, Variable *Value)
{
   if (Value->Type & FD_SCALED) return ERR_NoSupport;

   if (Value->Type & FD_DOUBLE) Value->Double = Self->Point;
   else if (Value->Type & FD_LARGE) Value->Large = Self->Point;
   else return ERR_FieldTypeMismatch;
   return ERR_Okay;
}

static ERROR SET_Point(extFont *Self, Variable *Value)
{
   pf::Log log;
   DOUBLE value;

   if (Value->Type & FD_DOUBLE) value = Value->Double;
   else if (Value->Type & FD_LARGE) value = Value->Large;
   else if (Value->Type & FD_STRING) value = strtod((CSTRING)Value->Pointer, NULL);
   else return ERR_FieldTypeMismatch;

   if (Value->Type & FD_SCALED) {
      // Default point size is scaled relative to display DPI, then re-scaled to the % value that was passed in.
      DOUBLE global_point = global_point_size();
      DOUBLE pct = value;
      value = (global_point * (DOUBLE)glDisplayHDPI / 96.0) * pct;
      log.msg("Calculated point size: %.2f, from global point %.2f * %.0f%%, DPI %d", value, global_point, pct, glDisplayHDPI);
   }

   if (value < 1) value = 1;

   if (Self->initialised()) {
      if (Self->Cache.get()) {
         unload_glyph_cache(Self); // Remove any existing glyph reference
         Self->Point = value;
         cache_truetype_font(Self, NULL); // Updates meta information like the Leading value
      }
   }
   else Self->Point = value;

   return ERR_Okay;
}

/*********************************************************************************************************************

-FIELD-
String: The string to use when drawing a Font.

The String field must be defined in order to draw text with a font object.  A string must consist of a valid sequence
of UTF-8 characters.  Line feeds are allowed (whenever a line feed is reached, the Draw action will start printing on
the next line).  Drawing will stop when the null termination character is reached.

If a string contains characters that are not supported by a font, those characters will be printed using a default
character from the font.

*********************************************************************************************************************/

static ERROR SET_String(extFont *Self, CSTRING Value)
{
   if (!StrCompare(Value, Self->String, 0, STR::MATCH_CASE|STR::MATCH_LEN)) return ERR_Okay;

   Self->prvLineCount = 0;
   Self->prvStrWidth  = 0; // Reset the string width for GET_Width
   Self->prvLineCountCR = 1; // Line count (carriage returns only)

   if ((Value) and (*Value)) {
      LONG i;
      for (i=0; Value[i]; i++) if (Value[i] IS '\n') Self->prvLineCountCR++;

      Self->prvBuffer.assign(Value);
      Self->String = (STRING)Self->prvBuffer.c_str();
   }

   return ERR_Okay;
}

/*********************************************************************************************************************

-FIELD-
Style: Determines font styling.

The style of a font can be selected by setting the Style field.  This comes into effect only if the font actually
supports the specified style as part of its graphics set.  If the style is unsupported, the regular styling of the
face will be used on initialisation.

Bitmap fonts are a special case if a bold or italic style is selected.  In this situation the system can automatically
convert the font to that style even if the correct graphics set does not exist.

Conventional font styles are `Bold`, `Bold Italic`, `Italic` and `Regular` (the default).  TrueType fonts can consist
of any style that the designer chooses, such as `Narrow` or `Wide`, so use ~Font.GetList() to retrieve available style
names.

*********************************************************************************************************************/

static ERROR SET_Style(extFont *Self, CSTRING Value)
{
   if ((!Value) or (!Value[0])) StrCopy("Regular", Self->prvStyle, sizeof(Self->prvStyle));
   else StrCopy(Value, Self->prvStyle, sizeof(Self->prvStyle));
   return ERR_Okay;
}

/*********************************************************************************************************************

-FIELD-
TabSize: Defines the tab size to use when drawing and manipulating a font string.

The TabSize value controls the interval between tabs, measured in characters.  If the font is scalable, the character
width of 'o' is used for character measurement.

The default tab size is 8 and the TabSize only comes into effect when tab characters are used in the font
#String.

-FIELD-
TotalGlyphs: Reflects the total number of character glyphs that are available by the font object.

The total number of character glyphs that are available is reflected in this field.  The font must have been
initialised before the count is known.

-FIELD-
Underline: Enables font underlining when set.

To underline a font string, set the Underline field to the colour that should be used to draw the underline.
Underlining can be turned off by writing this field with a NULL value or setting the alpha component to zero.

-FIELD-
Width: Returns the pixel width of a string.

Read this virtual field to obtain the pixel width of a font string.  You must have already set a string in the font for
this to work, or a width of zero will be returned.

*********************************************************************************************************************/

static ERROR GET_Width(extFont *Self, LONG *Value)
{
   if (!Self->String) {
      *Value = 0;
      return ERR_Okay;
   }

   if ((!Self->prvStrWidth) or ((Self->Align & (ALIGN::HORIZONTAL|ALIGN::RIGHT)) != ALIGN::NIL) or (Self->WrapEdge)){
      if (Self->WrapEdge > 0) {
         string_size(Self, Self->String, FSS_ALL, Self->WrapEdge - Self->X, &Self->prvStrWidth, NULL);
      }
      else string_size(Self, Self->String, FSS_ALL, 0, &Self->prvStrWidth, NULL);
   }

   *Value = Self->prvStrWidth;
   return ERR_Okay;
}

/*********************************************************************************************************************

-FIELD-
WrapEdge: Enables word wrapping at a given boundary.

Word wrapping is enabled by setting the WrapEdge field to a value greater than zero.  Wrapping occurs whenever the
right-most edge of any word in the font string extends past the coordinate indicated by the WrapEdge.

-FIELD-
X: The starting horizontal position when drawing the font string.

When drawing font strings, the X and Y fields define the position that the string will be drawn to in the target
surface.  The default coordinates are (0,0).

-FIELD-
Y: The starting vertical position when drawing the font string.

When drawing font strings, the X and Y fields define the position that the string will be drawn to in the target
surface.  The default coordinates are (0,0).

-FIELD-
YOffset: Additional offset value that is added to vertically aligned fonts.

Fonts that are aligned vertically (either in the center or bottom edge of the drawing area) will have a vertical offset
value.  Reading that value from this field and adding it to the Y field will give you an accurate reading of where
the string will be drawn.
-END-

*********************************************************************************************************************/

static ERROR GET_YOffset(extFont *Self, LONG *Value)
{
   if (Self->prvLineCount < 1) calc_lines(Self);

   if ((Self->Align & ALIGN::VERTICAL) != ALIGN::NIL) {
      LONG offset = (Self->AlignHeight - (Self->Height + (Self->LineSpacing * (Self->prvLineCount-1))))>>1;
      offset += (Self->LineSpacing - Self->MaxHeight)>>1; // Adjust for spacing between each individual line
      *Value = offset;
   }
   else if ((Self->Align & ALIGN::BOTTOM) != ALIGN::NIL) {
      *Value = Self->AlignHeight - (Self->MaxHeight + (Self->LineSpacing * (Self->prvLineCount-1)));
   }
   else *Value = 0;

   return ERR_Okay;
}

//********************************************************************************************************************
// All resources that are allocated in this routine must be untracked.
// Assumes a cache lock is held on being called.

static ERROR cache_truetype_font(extFont *Self, CSTRING Path)
{
   pf::Log log(__FUNCTION__);
   FT_Open_Args openargs;
   ERROR error;

   if (Path) { // Check the cache.
      CSTRING location;
      if (!ResolvePath(Path, RSF::NIL, (STRING *)&location)) {
         GuardedResource res(location);

         std::string sp(location);
         if (glCache.contains(sp)) Self->Cache = glCache[sp];
         else {
            log.branch("Creating new cache for font '%s'", location);

            FT_Face face;
            openargs.flags    = FT_OPEN_PATHNAME;
            openargs.pathname = (STRING)location;
            if ((error = FT_Open_Face(glFTLibrary, &openargs, 0, &face))) {
               if (error IS FT_Err_Unknown_File_Format) return ERR_NoSupport;
               log.warning("Fatal error in attempting to load font \"%s\".", location);
               return ERR_Failed;
            }

            if (!FT_IS_SCALABLE(face)) { // Only scalable fonts are supported by this routine
               FT_Done_Face(face);
               return log.warning(ERR_InvalidData);
            }

            Self->Cache = glCache[sp] = std::make_shared<font_cache>(sp, face);
         }
      }
   }
   else { // If no path is provided, the font is already cached and requires a new point size.
      log.trace("Recalculating size of currently loaded font.");
   }

   font_cache *fc = Self->Cache.get();

   if ((Self->Height) and (!Self->Point)) {
      // If the user has defined the font size in pixels, we need to convert it to a point size.
      // This conversion does not have to be 100% accurate - within 5% is good enough.
      Self->Point = (((DOUBLE)Self->Height * glDisplayHDPI) + (glDisplayHDPI * 0.5)) / glDisplayHDPI;
   }

   // Note that the point size is relative to the DPI of the target display

   if (Self->Point <= 0) Self->Point = global_point_size();

   Self->Height = F2T(Self->Point);

   fc->Glyphs.try_emplace(Self->Point, fc->Face, Self->Point, Self->prvDefaultChar);

   auto &glyph = fc->Glyphs.at(Self->Point);
   Self->TotalGlyphs = fc->Face->num_glyphs;

   // Determine the line distance of the font, which describes the amount of distance between each font line that is printed.

   if (!Path) Self->LineSpacing = Self->Height * 1.33;
   else Self->LineSpacing += Self->Height * 1.33;
   Self->MaxHeight = Self->Height * 1.33;

   // Leading adjustments for the top part of the font

   Self->Leading      = Self->MaxHeight - Self->Height; // Make the leading the same size as the gutter
   Self->MaxHeight   += Self->Leading; // Increase the max-height by the leading amount
   Self->LineSpacing += Self->Leading; // Increase the line-spacing by the leading amount
   Self->Ascent       = Self->Height + Self->Leading;
   Self->prvChar      = glyph.Chars;
   Self->Gutter       = Self->LineSpacing - Self->Ascent;

   if (Self->FixedWidth > 0) Self->prvSpaceWidth = Self->FixedWidth;
   else if (!FT_Load_Glyph(fc->Face, FT_Get_Char_Index(fc->Face, CHAR_SPACE), FT_LOAD_DEFAULT)) {
      Self->prvSpaceWidth = (fc->Face->glyph->advance.x>>FT_DOWNSIZE);
      if (Self->prvSpaceWidth < 3) Self->prvSpaceWidth = Self->Height>>1;
   }
   else Self->prvSpaceWidth = Self->Height>>1;

   fc->Usage++;
   glyph.Usage++;
   return ERR_Okay;
}

//********************************************************************************************************************
// The bias table is based on the most frequently used letters in the alphabet in the following order:
// e t a o i n s r h l d c u m f p g w y b v k x j q z

static const UBYTE bias[26] = { 9,3,6,6,9,6,3,6,9,1,1,6,6,9,9,3,1,9,9,9,6,3,3,1,3,1 };

static font_glyph * get_glyph(extFont *Self, ULONG Unicode)
{
   pf::Log log(__FUNCTION__);

   glyph_cache &cache = Self->Cache->Glyphs.at(Self->Point);
   auto &face = Self->Cache->Face;

   if (face->size != cache.Size) FT_Activate_Size(cache.Size);

   LONG glyph_index;

   if (cache.Glyphs.contains(Unicode)) {
      font_glyph *glyph = &cache.Glyphs[Unicode];
      return glyph;
   }

   if (!(glyph_index = FT_Get_Char_Index(face, Unicode))) {
      if (!(glyph_index = FT_Get_Char_Index(face, Self->prvDefaultChar))) {
         glyph_index = 1; // Take the first glyph as the default
      }
   }

   FT_Error fterr;
   if ((fterr = FT_Load_Glyph(face, glyph_index, FT_LOAD_DEFAULT))) {
      log.warning("Failed to load glyph %d '%lc', FT error: %s", glyph_index, (wint_t)Unicode, get_ft_error(fterr));
      return NULL;
   }

   if (cache.Glyphs.size() < MAX_GLYPHS) { // Cache this glyph if possible
      log.traceBranch("Creating new cache entry for unicode value %d, advance %d", Unicode, (LONG)face->glyph->advance.x>>FT_DOWNSIZE);

      font_glyph glyph;
      ClearMemory(&glyph, sizeof(glyph));

      glyph.Top        = face->glyph->bitmap_top;
      glyph.Left       = face->glyph->bitmap_left;
      glyph.Width      = face->glyph->bitmap.width;
      glyph.Height     = face->glyph->bitmap.rows;
      glyph.AdvanceX   = face->glyph->advance.x>>FT_DOWNSIZE;
      glyph.AdvanceY   = face->glyph->advance.y>>FT_DOWNSIZE;
      glyph.GlyphIndex = glyph_index;

      if ((Unicode >= 'a') and (Unicode <= 'z')) glyph.Count = bias[Unicode-'a'];
      else if ((Unicode >= 'A') and (Unicode <= 'Z')) glyph.Count = bias[Unicode-'A'];
      else glyph.Count = 1;

      cache.Glyphs.emplace(Unicode, glyph);
      font_glyph *key_glyph = &cache.Glyphs[Unicode];
      return key_glyph;
   }
   else {
      // Cache is full.  Return a temporary glyph with graphics data if requested.

      if (Self->prvTempGlyph.Outline) {
         FreeResource(Self->prvTempGlyph.Outline);
         Self->prvTempGlyph.Outline = NULL;
      }

      Self->prvTempGlyph.Data      = NULL;
      Self->prvTempGlyph.Outline   = NULL;
      Self->prvTempGlyph.AdvanceX   = face->glyph->advance.x>>FT_DOWNSIZE;
      Self->prvTempGlyph.AdvanceY   = face->glyph->advance.y>>FT_DOWNSIZE;
      Self->prvTempGlyph.GlyphIndex = glyph_index;
      return &Self->prvTempGlyph;
   }
}

//********************************************************************************************************************

static ERROR draw_bitmap_font(extFont *Self)
{
   pf::Log log(__FUNCTION__);
   objBitmap *bitmap;
   RGB8 rgb;
   static const UBYTE table[] = { 0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01 };
   UBYTE *xdata, *data;
   LONG linewidth, offset, charclip, wrapindex, charlen;
   ULONG unicode, ocolour;
   WORD startx, xpos, ex, ey, sx, sy, xinc;
   WORD bytewidth, alpha, charwidth;
   bool draw_line;
   #define CHECK_LINE_CLIP(font,y,bmp) if (((y)-1 < (bmp)->Clip.Bottom) and ((y) + (font)->prvBitmapHeight + 1 > (bmp)->Clip.Top)) draw_line = true; else draw_line = false;

   // Validate settings for fixed font type

   if (!(bitmap = Self->Bitmap)) { log.warning("The Bitmap field is not set."); return ERR_FieldNotSet; }
   if (!Self->String) return ERR_FieldNotSet;
   if (!Self->String[0]) return ERR_Okay;

   ERROR error = ERR_Okay;
   STRING str = Self->String;
   LONG dxcoord = Self->X;
   LONG dycoord = Self->Y;

   if (!Self->AlignWidth)  Self->AlignWidth  = bitmap->Width;
   if (!Self->AlignHeight) Self->AlignHeight = bitmap->Height;

   GET_YOffset(Self, &offset);
   dycoord = dycoord + offset - Self->Leading;

   if ((Self->Flags & FTF::BASE_LINE) != FTF::NIL) {
      dycoord -= (Self->Ascent - Self->Leading); // - 1;
   }

   string_size(Self, str, FSS_LINE, (Self->WrapEdge > 0) ? (Self->WrapEdge - Self->X) : 0, &linewidth, &wrapindex);
   CSTRING wrapstr = str + wrapindex;

   // If horizontal centering is required, calculate the correct horizontal starting coordinate.

   if ((Self->Align & (ALIGN::HORIZONTAL|ALIGN::RIGHT)) != ALIGN::NIL) {
      if ((Self->Align & ALIGN::HORIZONTAL) != ALIGN::NIL) {
         dxcoord = Self->X + ((Self->AlignWidth - linewidth)>>1);
      }
      else dxcoord = Self->X + Self->AlignWidth - linewidth;
   }

   ULONG colour  = bitmap->getColour(Self->Colour);
   ULONG ucolour = bitmap->getColour(Self->Underline);

   if (Self->Outline.Alpha > 0) {
      Self->BmpCache->get_outline();
      ocolour = bitmap->getColour(Self->Outline);
   }
   else ocolour = 0;

   charclip = Self->WrapEdge - 8; //(Self->prvChar['.'].Advance<<2); //8

   if (acLock(bitmap) != ERR_Okay) return log.warning(ERR_Lock);

   WORD dx = 0, dy = 0;
   startx = dxcoord;
   CHECK_LINE_CLIP(Self, dycoord, bitmap);
   while (*str) {
      if (*str IS '\n') { // Reset the font to a new line
         if (Self->Underline.Alpha > 0) {
            gfxDrawRectangle(bitmap, startx, dycoord + Self->Height + 1, dxcoord-startx, ((Self->Flags & FTF::HEAVY_LINE) != FTF::NIL) ? 2 : 1, ucolour, BAF::FILL);
         }

         str++;

         while ((*str) and (*str <= 0x20)) { if (*str IS '\n') dycoord += Self->LineSpacing; str++; }
         string_size(Self, str, FSS_LINE, (Self->WrapEdge > 0) ? (Self->WrapEdge - Self->X) : 0, &linewidth, &wrapindex);
         wrapstr = str + wrapindex;

         if ((Self->Align & (ALIGN::HORIZONTAL|ALIGN::RIGHT)) != ALIGN::NIL) {
            if ((Self->Align & ALIGN::HORIZONTAL) != ALIGN::NIL) dxcoord = Self->X + ((Self->AlignWidth - linewidth)>>1);
            else dxcoord = Self->X + Self->AlignWidth - linewidth;
         }
         else dxcoord = Self->X;

         startx = dxcoord;
         dycoord += Self->LineSpacing;
         CHECK_LINE_CLIP(Self, dycoord, bitmap);
      }
      else if (*str IS '\t') {
         WORD tabwidth = (Self->prvChar['o'].Advance * Self->GlyphSpacing) * Self->TabSize;
         dxcoord = Self->X + pf::roundup(dxcoord - Self->X, tabwidth);
         str++;
      }
      else {
         charlen = getutf8(str, &unicode);

         if ((unicode > 255) or (!Self->prvChar[unicode].Advance)) unicode = Self->prvDefaultChar;

         if (Self->FixedWidth > 0) charwidth = Self->FixedWidth;
         else charwidth = Self->prvChar[unicode].Advance;

         // Wordwrap management

         if (str >= wrapstr) {
            dxcoord = Self->X;
            dycoord += Self->LineSpacing;

            while ((*str) and (*str <= 0x20)) { if (*str IS '\n') dycoord += Self->LineSpacing; str++; }
            string_size(Self, str, FSS_LINE, Self->WrapEdge - dxcoord, &linewidth, &wrapindex);
            wrapstr = str + wrapindex;

            if ((Self->Align & (ALIGN::HORIZONTAL|ALIGN::RIGHT)) != ALIGN::NIL) {
               if ((Self->Align & ALIGN::HORIZONTAL) != ALIGN::NIL) dxcoord = Self->X + ((Self->AlignWidth - linewidth)>>1);
               else dxcoord = Self->X + Self->AlignWidth - linewidth;
            }
            CHECK_LINE_CLIP(Self, dycoord, bitmap);
         }

         str += charlen;

         if ((unicode > 0x20) and (draw_line)) {
            if (Self->Outline.Alpha > 0) { // Outline support
               auto outline = Self->BmpCache->get_outline();
               if (outline) {
                  auto data = outline + Self->prvChar[unicode].OutlineOffset;
                  bytewidth = (Self->prvChar[unicode].Width + 9)>>3;

                  sx = dxcoord - 1;
                  ex = sx + Self->prvChar[unicode].Width + 2;

                  if (ex > bitmap->Clip.Right) ex = bitmap->Clip.Right;

                  xinc = 0;
                  if (sx < bitmap->Clip.Left) {
                     xinc = bitmap->Clip.Left - sx;
                     sx = bitmap->Clip.Left;
                  }

                  sy = dycoord - 1;

                  ey = sy + Self->prvBitmapHeight + 2;
                  if (ey > bitmap->Clip.Bottom) ey = bitmap->Clip.Bottom;

                  if (sy < bitmap->Clip.Top) {
                     data += bytewidth * (bitmap->Clip.Top - sy);
                     sy = bitmap->Clip.Top;
                  }

                  sx += bitmap->XOffset;
                  sy += bitmap->YOffset;
                  dx += bitmap->XOffset;
                  dy += bitmap->YOffset;
                  ex += bitmap->XOffset;
                  ey += bitmap->YOffset;

                  if (Self->Outline.Alpha < 255) {
                     alpha = 255 - Self->Outline.Alpha;
                     for (dy=sy; dy < ey; dy++) {
                        xpos = xinc;
                        for (dx=sx; dx < ex; dx++) {
                           if (data[xpos>>3] & (0x80>>(xpos & 0x7))) {
                              bitmap->ReadUCRPixel(bitmap, dx, dy, &rgb);
                              rgb.Red   = Self->Outline.Red   + (((rgb.Red   - Self->Outline.Red) * alpha)>>8);
                              rgb.Green = Self->Outline.Green + (((rgb.Green - Self->Outline.Green) * alpha)>>8);
                              rgb.Blue  = Self->Outline.Blue  + (((rgb.Blue  - Self->Outline.Blue) * alpha)>>8);
                              bitmap->DrawUCRPixel(bitmap, dx, dy, &rgb);
                           }
                           xpos++;
                        }
                        data += bytewidth;
                     }
                  }
                  else {
                     for (dy=sy; dy < ey; dy++) {
                        xpos = xinc;
                        for (dx=sx; dx < ex; dx++) {
                           if (data[xpos>>3] & (0x80>>(xpos & 0x7))) {
                              bitmap->DrawUCPixel(bitmap, dx, dy, ocolour);
                           }
                           xpos++;
                        }
                        data += bytewidth;
                     }
                  }
               }
            }

            data = Self->prvData + Self->prvChar[unicode].Offset;
            bytewidth = (Self->prvChar[unicode].Width + 7)>>3;

            // Horizontal coordinates

            sx = dxcoord;

            ex = sx + Self->prvChar[unicode].Width;
            if (ex > bitmap->Clip.Right) ex = bitmap->Clip.Right;

            xinc = 0;
            if (sx < bitmap->Clip.Left) {
               xinc = bitmap->Clip.Left - sx;
               sx = bitmap->Clip.Left;
            }

            // Vertical coordinates

            sy = dycoord;

            ey = sy + Self->prvBitmapHeight;
            if (ey > bitmap->Clip.Bottom) ey = bitmap->Clip.Bottom;

            if (sy < bitmap->Clip.Top) {
               data += bytewidth * (bitmap->Clip.Top - sy);
               sy = bitmap->Clip.Top;
            }

            sx += bitmap->XOffset; // Add offsets only after clipping adjustments
            sy += bitmap->YOffset;
            dx += bitmap->XOffset;
            dy += bitmap->YOffset;
            ex += bitmap->XOffset;
            ey += bitmap->YOffset;

            if (Self->Colour.Alpha < 255) {
               alpha = 255 - Self->Colour.Alpha;
               for (dy=sy; dy < ey; dy++) {
                  xpos = xinc;
                  for (dx=sx; dx < ex; dx++) {
                     if (data[xpos>>3] & (0x80>>(xpos & 0x7))) {
                        bitmap->ReadUCRPixel(bitmap, dx, dy, &rgb);
                        rgb.Red   = Self->Colour.Red   + (((rgb.Red   - Self->Colour.Red) * alpha)>>8);
                        rgb.Green = Self->Colour.Green + (((rgb.Green - Self->Colour.Green) * alpha)>>8);
                        rgb.Blue  = Self->Colour.Blue  + (((rgb.Blue  - Self->Colour.Blue) * alpha)>>8);
                        bitmap->DrawUCRPixel(bitmap, dx, dy, &rgb);
                     }
                     xpos++;
                  }
                  data += bytewidth;
               }
            }
            else {
               if (bitmap->BytesPerPixel IS 4) {
                  auto dest = (ULONG *)(bitmap->Data + (sx<<2) + (sy * bitmap->LineWidth));
                  for (dy=sy; dy < ey; dy++) {
                     xpos = xinc & 0x07;
                     xdata = data + (xinc>>3);
                     for (dx=0; dx < ex-sx; dx++) {
                        if (*xdata & table[xpos++]) dest[dx] = colour;
                        if (xpos > 7) {
                           xpos = 0;
                           xdata++;
                        }
                     }
                     dest = (ULONG *)(((UBYTE *)dest) + bitmap->LineWidth);
                     data += bytewidth;
                  }
               }
               else if (bitmap->BytesPerPixel IS 2) {
                  auto dest = (UWORD *)(bitmap->Data + (sx<<1) + (sy * bitmap->LineWidth));
                  for (dy=sy; dy < ey; dy++) {
                     xpos = xinc & 0x07;
                     xdata = data + (xinc>>3);
                     for (dx=0; dx < ex-sx; dx++) {
                        if (*xdata & table[xpos++]) dest[dx] = colour;
                        if (xpos > 7) {
                           xpos = 0;
                           xdata++;
                        }
                     }
                     dest = (UWORD *)(((UBYTE *)dest) + bitmap->LineWidth);
                     data += bytewidth;
                  }
               }
               else if (bitmap->BitsPerPixel IS 8) {
                  if ((bitmap->Flags & BMF::MASK) != BMF::NIL) {
                     if ((bitmap->Flags & BMF::INVERSE_ALPHA) != BMF::NIL) colour = 0;
                     else colour = 255;
                  }

                  auto dest = (UBYTE *)(bitmap->Data + sx + (sy * bitmap->LineWidth));
                  for (dy=sy; dy < ey; dy++) {
                     xpos = xinc & 0x07;
                     xdata = data + (xinc>>3);
                     for (dx=0; dx < ex-sx; dx++) {
                        if (*xdata & table[xpos++]) dest[dx] = (UBYTE)colour;
                        if (xpos > 7) {
                           xpos = 0;
                           xdata++;
                        }
                     }
                     dest = (UBYTE *)(((UBYTE *)dest) + bitmap->LineWidth);
                     data += bytewidth;
                  }
               }
               else {
                  for (dy=sy; dy < ey; dy++) {
                     xpos = xinc & 0x07;
                     xdata = data + (xinc>>3);
                     for (dx=sx; dx < ex; dx++) {
                        if (*xdata & table[xpos++]) bitmap->DrawUCPixel(bitmap, dx, dy, colour);
                        if (xpos > 7) {
                           xpos = 0;
                           xdata++;
                        }
                     }
                     data += bytewidth;
                  }
               }
            }
         }

         dxcoord += charwidth * Self->GlyphSpacing;
      }
   } // while (*str)

   // Draw an underline for the current line if underlining is turned on

   if (Self->Underline.Alpha > 0) {
      if ((Self->Flags & FTF::BASE_LINE) != FTF::NIL) sy = dycoord;
      else sy = dycoord + Self->Height + Self->Leading + 1;
      gfxDrawRectangle(bitmap, startx, sy, dxcoord-startx, ((Self->Flags & FTF::HEAVY_LINE) != FTF::NIL) ? 2 : 1, ucolour, BAF::FILL);
   }

   Self->EndX = dxcoord;
   Self->EndY = dycoord + Self->Leading;

   acUnlock(bitmap);

   return error;
}

//********************************************************************************************************************

static void unload_glyph_cache(extFont *Font)
{
   pf::Log log(__FUNCTION__);

   CACHE_LOCK lock(glCacheMutex);

   if ((Font->Cache.get()) and (Font->Cache->Glyphs.contains(Font->Point))) {
      auto &glyphs = Font->Cache->Glyphs.at(Font->Point);
      glyphs.Usage--;
      if (!glyphs.Usage) {
         Font->Cache->Glyphs.erase(Font->Point);
      }
   }
}

//********************************************************************************************************************

#include "class_font_def.c"

static const FieldDef AlignFlags[] = {
   { "Right",      ALIGN::RIGHT      }, { "Left",     ALIGN::LEFT },
   { "Bottom",     ALIGN::BOTTOM     }, { "Top",      ALIGN::TOP },
   { "Horizontal", ALIGN::HORIZONTAL }, { "Vertical", ALIGN::VERTICAL },
   { "Center",     ALIGN::CENTER     }, { "Middle",   ALIGN::MIDDLE },
   { NULL, 0 }
};

static const FieldArray clFontFields[] = {
   { "Point",        FDF_DOUBLE|FDF_VARIABLE|FDF_SCALED|FDF_RW, GET_Point, SET_Point },
   { "GlyphSpacing", FDF_DOUBLE|FDF_RW },
   { "Bitmap",       FDF_OBJECT|FDF_RW, NULL, NULL, ID_BITMAP },
   { "String",       FDF_STRING|FDF_RW, NULL, SET_String },
   { "Path",         FDF_STRING|FDF_RW, NULL, SET_Path },
   { "Style",        FDF_STRING|FDF_RI, NULL, SET_Style },
   { "Face",         FDF_STRING|FDF_RI, NULL, SET_Face },
   { "Outline",      FDF_RGB|FDF_RW },
   { "Underline",    FDF_RGB|FDF_RW },
   { "Colour",       FDF_RGB|FDF_RW },
   { "Flags",        FDF_LONGFLAGS|FDF_RW, NULL, SET_Flags, clFontFlags },
   { "Gutter",       FDF_LONG|FDF_RI },
   { "LineSpacing",  FDF_LONG|FDF_RW },
   { "X",            FDF_LONG|FDF_RW },
   { "Y",            FDF_LONG|FDF_RW },
   { "TabSize",      FDF_LONG|FDF_RW },
   { "TotalGlyphs",  FDF_LONG|FDF_R },
   { "WrapEdge",     FDF_LONG|FDF_RW },
   { "FixedWidth",   FDF_LONG|FDF_RW },
   { "Height",       FDF_LONG|FDF_RI },
   { "Leading",      FDF_LONG|FDF_R },
   { "MaxHeight",    FDF_LONG|FDF_RI },
   { "Align",        FDF_LONGFLAGS|FDF_RW, NULL, NULL, AlignFlags },
   { "AlignWidth",   FDF_LONG|FDF_RW },
   { "AlignHeight",  FDF_LONG|FDF_RW },
   { "Ascent",       FDF_LONG|FDF_R },
   { "EndX",         FDF_LONG|FDF_RW },
   { "EndY",         FDF_LONG|FDF_RW },
   // Virtual fields
   { "Bold",         FDF_VIRTUAL|FDF_LONG|FDF_RW, GET_Bold, SET_Bold },
   { "Italic",       FDF_VIRTUAL|FDF_LONG|FDF_RW, GET_Italic, SET_Italic },
   { "LineCount",    FDF_VIRTUAL|FDF_LONG|FDF_R, GET_LineCount },
   { "Location",     FDF_VIRTUAL|FDF_STRING|FDF_SYNONYM|FDF_RW, NULL, SET_Path },
   { "Opacity",      FDF_VIRTUAL|FDF_DOUBLE|FDF_RW, GET_Opacity, SET_Opacity },
   { "Width",        FDF_VIRTUAL|FDF_LONG|FDF_R, GET_Width },
   { "YOffset",      FDF_VIRTUAL|FDF_LONG|FDF_R, GET_YOffset },
   END_FIELD
};

//********************************************************************************************************************

static ERROR add_font_class(void)
{
   clFont = objMetaClass::create::global(
      fl::BaseClassID(ID_FONT),
      fl::ClassVersion(VER_FONT),
      fl::Name("Font"),
      fl::Category(CCF::GRAPHICS),
      fl::FileExtension("*.font|*.fnt|*.tty|*.fon"),
      fl::FileDescription("Font"),
      fl::Actions(clFontActions),
      fl::Fields(clFontFields),
      fl::Size(sizeof(extFont)),
      fl::Path(MOD_PATH));

   return clFont ? ERR_Okay : ERR_AddClass;
}
