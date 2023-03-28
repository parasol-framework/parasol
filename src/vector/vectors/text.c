/*****************************************************************************

-CLASS-
VectorText: Extends the Vector class with support for generating text.

To create text along a path, set the #Morph field with a reference to any @Vector object that generates a path.  The
following extract illustrates the SVG equivalent of this feature:

<pre>
&lt;defs&gt;
  &lt;path id="myTextPath2" d="M75,20 l100,0 l100,30 q0,100 150,100"/&gt;
&lt;/defs&gt;

&lt;text x="10" y="100" stroke="#000000"&gt;
  &lt;textPath xlink:href="#myTextPath2"/&gt;
&lt;/text&gt;
</pre>

-END-

*****************************************************************************/

/*
textPath warping could be more accurate if the character angles were calculated on the mid-point of the character
rather than the bottom left corner.  However, this would be more computationally intensive and only useful in situations
where large glyphs were oriented around sharp corners.  The process would look something like this:

+ Compute the (x,y) of the character's middle vertex in context of the morph path.
+ Compute the angle from the middle vertex to the first vertex (start_x, start_y)
+ Compute the angle from the middle vertex to the last vertex (end_x, end_y)
+ Interpolate the two angles
+ Rotate the character around its mid point.
+ The final character position is moved to the mid-point rather than start_x,start_y
*/

#include "agg_gsv_text.h"
#include "agg_path_length.h"

static void reset_font(objVectorText *Vector);

INLINE DOUBLE int26p6_to_dbl(LONG p)
{
  return double(p) / 64.0;
}

INLINE LONG dbl_to_int26p6(DOUBLE p)
{
   return LONG(p * 64.0);
}

//****************************************************************************
// Only call this function if the font includes kerning support

INLINE void get_kerning_xy(FT_Face Face, LONG Glyph, LONG PrevGlyph, DOUBLE *X, DOUBLE *Y)
{
   FT_Vector delta;
   EFT_Get_Kerning(Face, PrevGlyph, Glyph, FT_KERNING_DEFAULT, &delta);
   *X = int26p6_to_dbl(delta.x);
   *Y = int26p6_to_dbl(delta.y);
}

//****************************************************************************
// Converts a Freetype glyph outline to an AGG path.  The size of the font must be preset in the FT_Outline object,
// with a call to FT_Set_Char_Size()

ERROR decompose_ft_outline(const FT_Outline &outline, bool flip_y, agg::path_storage &path)
{
   FT_Vector v_last, v_control, v_start;
   DOUBLE x1, y1, x2, y2, x3, y3;
   FT_Vector *point, *limit;
   char *tags;
   LONG n;         // index of contour in outline
   char tag;       // current point's state

   LONG first = 0; // index of first point in contour
   for (n=0; n < outline.n_contours; n++) {
      LONG last = outline.contours[n];  // index of last point in contour
      limit = outline.points + last;

      v_start = outline.points[first];
      v_last  = outline.points[last];
      v_control = v_start;
      point = outline.points + first;
      tags  = outline.tags  + first;
      tag   = FT_CURVE_TAG(tags[0]);

      if (tag == FT_CURVE_TAG_CUBIC) return ERR_Failed; // A contour cannot start with a cubic control point!

      // check first point to determine origin
      if (tag == FT_CURVE_TAG_CONIC) { // first point is conic control.  Yes, this happens.
         if (FT_CURVE_TAG(outline.tags[last]) == FT_CURVE_TAG_ON) { // start at last point if it is on the curve
            v_start = v_last;
            limit--;
         }
         else { // if both first and last points are conic, start at their middle and record its position for closure
            v_start.x = (v_start.x + v_last.x) / 2;
            v_start.y = (v_start.y + v_last.y) / 2;
            v_last = v_start;
         }
         point--;
         tags--;
      }

      x1 = int26p6_to_dbl(v_start.x);
      y1 = int26p6_to_dbl(v_start.y);
      if (flip_y) y1 = -y1;
      path.move_to(x1, y1);

      while (point < limit) {
         point++;
         tags++;

         tag = FT_CURVE_TAG(tags[0]);
         switch(tag) {
            case FT_CURVE_TAG_ON: {  // emit a single line_to
               x1 = int26p6_to_dbl(point->x);
               y1 = int26p6_to_dbl(point->y);
               if (flip_y) y1 = -y1;
               path.line_to(x1, y1);
               continue;
            }

            case FT_CURVE_TAG_CONIC: { // consume conic arcs
               v_control.x = point->x;
               v_control.y = point->y;

               Do_Conic:
                  if (point < limit) {
                      FT_Vector vec, v_middle;

                      point++;
                      tags++;
                      tag = FT_CURVE_TAG(tags[0]);

                      vec.x = point->x;
                      vec.y = point->y;

                      if (tag == FT_CURVE_TAG_ON) {
                          x1 = int26p6_to_dbl(v_control.x);
                          y1 = int26p6_to_dbl(v_control.y);
                          x2 = int26p6_to_dbl(vec.x);
                          y2 = int26p6_to_dbl(vec.y);
                          if (flip_y) { y1 = -y1; y2 = -y2; }
                          path.curve3(x1, y1, x2, y2);
                          continue;
                      }

                      if (tag != FT_CURVE_TAG_CONIC) return ERR_Failed;

                      v_middle.x = (v_control.x + vec.x) / 2;
                      v_middle.y = (v_control.y + vec.y) / 2;

                      x1 = int26p6_to_dbl(v_control.x);
                      y1 = int26p6_to_dbl(v_control.y);
                      x2 = int26p6_to_dbl(v_middle.x);
                      y2 = int26p6_to_dbl(v_middle.y);
                      if (flip_y) { y1 = -y1; y2 = -y2; }
                      path.curve3(x1, y1, x2, y2);
                      v_control = vec;
                      goto Do_Conic;
                  }

                  x1 = int26p6_to_dbl(v_control.x);
                  y1 = int26p6_to_dbl(v_control.y);
                  x2 = int26p6_to_dbl(v_start.x);
                  y2 = int26p6_to_dbl(v_start.y);
                  if (flip_y) { y1 = -y1; y2 = -y2; }
                  path.curve3(x1, y1, x2, y2);
                  goto Close;
            }

            default: { // FT_CURVE_TAG_CUBIC
               FT_Vector vec1, vec2;

               if (point + 1 > limit || FT_CURVE_TAG(tags[1]) != FT_CURVE_TAG_CUBIC) return ERR_Failed;

               vec1.x = point[0].x;
               vec1.y = point[0].y;
               vec2.x = point[1].x;
               vec2.y = point[1].y;

               point += 2;
               tags  += 2;

               if (point <= limit) {
                  FT_Vector vec;
                  vec.x = point->x;
                  vec.y = point->y;
                  x1 = int26p6_to_dbl(vec1.x);
                  y1 = int26p6_to_dbl(vec1.y);
                  x2 = int26p6_to_dbl(vec2.x);
                  y2 = int26p6_to_dbl(vec2.y);
                  x3 = int26p6_to_dbl(vec.x);
                  y3 = int26p6_to_dbl(vec.y);
                  if (flip_y) { y1 = -y1; y2 = -y2; y3 = -y3; }
                  path.curve4(x1, y1, x2, y2, x3, y3);
                  continue;
               }

               x1 = int26p6_to_dbl(vec1.x);
               y1 = int26p6_to_dbl(vec1.y);
               x2 = int26p6_to_dbl(vec2.x);
               y2 = int26p6_to_dbl(vec2.y);
               x3 = int26p6_to_dbl(v_start.x);
               y3 = int26p6_to_dbl(v_start.y);
               if (flip_y) { y1 = -y1; y2 = -y2; y3 = -y3; }
               path.curve4(x1, y1, x2, y2, x3, y3);
               goto Close;
            }
         } // switch
      } // while

      path.close_polygon();

Close:
      first = last + 1;
   }

   return ERR_Okay;
}

//****************************************************************************
// This path generator creates text as a single path, by concatenating the paths of all individual characters in the
// string.

static void generate_text(objVectorText *Vector)
{
   parasol::Log log(__FUNCTION__);

   if (!Vector->txFont) {
      reset_font(Vector);
      if (!Vector->txFont) return;
   }

   CSTRING str;
   FT_Face ftface;
   LONG unicode;

   if (!(str = Vector->txString)) return;

   if ((GetPointer(Vector->txFont, FID_FreetypeFace, &ftface)) or (!ftface)) return;

   objVector *morph = Vector->Morph;
   DOUBLE start_x, start_y, end_vx, end_vy;
   DOUBLE path_scale = 1.0;
   if (morph) {
      if (Vector->MorphFlags & VMF_STRETCH) {
         // In stretch mode, the standard morphing algorithm is used (see gen_vector_path())
         morph = NULL;
      }
      else {
         if (morph->Dirty) { // Regenerate the target path if necessary
            gen_vector_path((objVector *)morph);
            morph->Dirty = 0;
         }

         if (!morph->BasePath) morph = NULL;
         else {
            morph->BasePath->rewind(0);
            morph->BasePath->vertex(0, &start_x, &start_y);
            end_vx = start_x;
            end_vy = start_y;
            if (morph->PathLength > 0) {
               path_scale = morph->PathLength / agg::path_length(*morph->BasePath);
               morph->BasePath->rewind(0);
            }
         }
      }
   }

   // Compute the string length in characters

   LONG str_length = 0;
   for (CSTRING lstr=Vector->txString; *lstr; ) {
      if (*lstr IS '\n') lstr++;
      else if (*lstr IS '\t') lstr++;
      else {
         LONG char_len = UTF8CharLength(lstr);
         if (!char_len) continue;
         lstr += char_len;
         str_length++;
      }
   }

   // The scale_char transform is applied to each character to ensure that it is scaled to the path correctly.

   DOUBLE dx = 0, dy = 0;
   LONG prevglyph = 0;
   DOUBLE dist = 0; // Distance to next vertex
   LONG cmd = -1;
   LONG char_index = 0;
   DOUBLE angle = 0;

   // Upscaling is used to get the Freetype engine to generate accurate vertices and advance coordinates.
   // There is a limit to the upscale value.  100 seems to be reasonable; anything 1000+ results in issues.

   DOUBLE upscale = 1;
   if ((Vector->Transition) or (morph)) upscale = 100;

   agg::path_storage char_path;
   agg::trans_affine scale_char;

   // The '3/4' conversion makes sense if you refer to read_unit() and understand that a point is 3/4 of a pixel.

   const DOUBLE point_size = Vector->txFontSize * (3.0 / 4.0) * upscale;
   if (path_scale != 1.0) {
      scale_char.translate(0, point_size);
      scale_char.scale(path_scale);
      scale_char.translate(0, -point_size * path_scale);
   }
   scale_char.scale(1.0 / upscale); // Downscale the generated character to the correct size.

   while (*str) {
      if (*str IS '\n') str++;
      else if (*str IS '\t') str++;
      else {
         LONG charlen;
         unicode = UTF8ReadValue(str, &charlen);
         str += charlen;

         if (!unicode) continue;
         char_index++;

         DOUBLE kx, ky;

         agg::trans_affine transform(scale_char); // The initial transform scales the char to the path.

         if (Vector->Transition) { // Apply any special transitions early.
            apply_transition(Vector->Transition, DOUBLE(char_index) / DOUBLE(str_length), transform);
         }

         EFT_Set_Char_Size(ftface, 0, dbl_to_int26p6(point_size), FIXED_DPI, FIXED_DPI); // Note that the font will be upscaled if necessary.

         LONG glyph = EFT_Get_Char_Index(ftface, unicode);

         if (!EFT_Load_Glyph(ftface, glyph, FT_LOAD_LINEAR_DESIGN)) {
            char_path.free_all();
            if (!decompose_ft_outline(ftface->glyph->outline, true, char_path)) {
               get_kerning_xy(ftface, glyph, prevglyph, &kx, &ky);

               DOUBLE char_width = int26p6_to_dbl(ftface->glyph->advance.x) + kx;

               char_width = char_width * ABS(transform.sx);
               //char_width = char_width * transform.scale();

               if (morph) {
                  // Compute end_vx,end_vy (the last vertex to use for angle computation) and store the distance from start_x,start_y to end_vx,end_vy in dist.
                  if (char_width > dist) {
                     while (cmd != agg::path_cmd_stop) {
                        DOUBLE current_x, current_y;
                        cmd = morph->BasePath->vertex(&current_x, &current_y);
                        if (agg::is_vertex(cmd)) {
                           const DOUBLE x = (current_x - end_vx), y = (current_y - end_vy);
                           const DOUBLE vertex_dist = sqrt((x * x) + (y * y));
                           dist += vertex_dist;

                           //log.trace("%c char_width: %.2f, VXY: %.2f %.2f, next: %.2f %.2f, dist: %.2f", unicode, char_width, start_x, start_y, end_vx, end_vy, dist);

                           end_vx = current_x;
                           end_vy = current_y;

                           if (char_width <= dist) break; // Stop processing vertices if dist exceeds the character width.
                        }
                     }
                  }

                  // At this stage we can say that start_x,start_y is the bottom left corner of the character and end_vx,end_vy is
                  // the bottom right corner.

                  DOUBLE tx = start_x, ty = start_y;

                  if (cmd != agg::path_cmd_stop) { // Advance (start_x,start_y) to the next point on the morph path.
                     angle = atan2(end_vy - start_y, end_vx - start_x);

                     DOUBLE x = end_vx - start_x, y = end_vy - start_y;
                     DOUBLE d = sqrt(x * x + y * y);
                     start_x += x / d * (char_width);
                     start_y += y / d * (char_width);

                     dist -= char_width; // The distance to the next vertex is reduced by the width of the char.
                  }
                  else { // No more path to use - advance start_x,start_y by the last known angle.
                     start_x += (char_width) * cos(angle);
                     start_y += (char_width) * sin(angle);
                  }

                  //log.trace("Char '%c' is at (%.2f %.2f) to (%.2f %.2f), angle: %.2f, remaining dist: %.2f", unicode, tx, ty, start_x, start_y, angle / DEG2RAD, dist);

                  if (unicode > 0x20) {
                     transform.rotate(angle); // Rotate the character in accordance with its position on the path angle.
                     transform.translate(tx, ty); // Move the character to its correct position on the path.
                     agg::conv_transform<agg::path_storage, agg::trans_affine> trans_path(char_path, transform);
                     Vector->BasePath->concat_path(trans_path);
                  }
                  dx += char_width;
               }
               else {
                  transform.translate(dx, dy);
                  agg::conv_transform<agg::path_storage, agg::trans_affine> trans_path(char_path, transform);
                  Vector->BasePath->concat_path(trans_path);
                  // Advance to next character coordinate
                  dx += char_width;
                  dy += int26p6_to_dbl(ftface->glyph->advance.y) + ky;
               }

            }
            else log.trace("Failed to get outline of character.");
         }
         prevglyph = glyph;
      }
   }

   Vector->txWidth = dx;
}

//****************************************************************************

static void get_text_xy(objVectorText *Vector)
{
   DOUBLE x = Vector->txX, y = Vector->txY;

   if (Vector->txXRelative) {
      if (Vector->ParentView->vpDimensions & DMF_WIDTH) x *= Vector->ParentView->vpFixedWidth;
      else if (Vector->ParentView->vpViewWidth > 0) x *= Vector->ParentView->vpViewWidth;
      else x *= Vector->Scene->PageWidth;
   }

   if (Vector->txYRelative) {
      if (Vector->ParentView->vpDimensions & DMF_HEIGHT) y *= Vector->ParentView->vpFixedHeight;
      else if (Vector->ParentView->vpViewHeight > 0) y *= Vector->ParentView->vpViewHeight;
      else y *= Vector->Scene->PageHeight;
   }

   if (Vector->txAlignFlags & ALIGN_RIGHT) x -= Vector->txWidth;
   else if (Vector->txAlignFlags & ALIGN_HORIZONTAL) x -= Vector->txWidth * 0.5;

   Vector->FinalX = x;
   Vector->FinalY = y;
}

//****************************************************************************
// (Re)loads the font for a text object.  This is a resource intensive exercise that should be avoided until the object
// is ready to initialise.

static void reset_font(objVectorText *Vector)
{
   if (!(Vector->Head.Flags & NF_INITIALISED)) return;

   parasol::Log log(__FUNCTION__);
   log.branch();
   parasol::SwitchContext context(Vector);

   objFont *font;
   if (!NewObject(ID_FONT, NF_INTEGRAL, &font)) {
      // Note that we don't configure too much of the font, as AGG uses the Freetype functions directly.  The
      // use of the Font object is really as a place-holder to take advantage of the Parasol font cache.

      if (Vector->txFamily) {
         CSTRING location;
         char family[120];
         UWORD i = StrCopy(Vector->txFamily, family, sizeof(family));
         StrCopy(",Open Sans", family+i, sizeof(family)-i);
         CSTRING weight = "Regular";
         if (Vector->txWeight >= 700) weight = "Extra Bold";
         else if (Vector->txWeight >= 500) weight = "Bold";
         else if (Vector->txWeight <= 200) weight = "Extra Light";
         else if (Vector->txWeight <= 300) weight = "Light";
         if (!fntSelectFont(family, weight, Vector->txFontSize, FTF_PREFER_SCALED, &location)) {
            SetString(font, FID_Path, location);
            FreeResource(location);
         }
         else SetString(font, FID_Face, "*");
      }
      else SetString(font, FID_Face, "*");

      // Set the correct point size, which is really for the benefit of the client e.g. if the Font object
      // is used to determine the source font's attributes.

      DOUBLE point_size = Vector->txFontSize * (3.0 / 4.0);
      SetDouble(font, FID_Point, point_size);

      if (!acInit(font)) {
         if (Vector->txFont) acFree(Vector->txFont);
         Vector->txFont = font;
      }
   }
}

//****************************************************************************

static ERROR TEXT_Free(objVectorText *Self, APTR Void)
{
   if (Self->txString) { FreeResource(Self->txString); Self->txString = NULL; }
   if (Self->txFamily) { FreeResource(Self->txFamily); Self->txFamily = NULL; }
   if (Self->txFont)   { acFree(Self->txFont); Self->txFont = NULL; }
   if (Self->txDX)     { FreeResource(Self->txDX); Self->txDX = NULL; }
   if (Self->txDY)     { FreeResource(Self->txDY); Self->txDY = NULL; }
   return ERR_Okay;
}

//****************************************************************************

static ERROR TEXT_NewObject(objVectorText *Self, APTR Void)
{
   Self->GeneratePath = (void (*)(rkVector *))&generate_text;
   Self->StrokeWidth = 0.0;
   Self->txWeight   = 400;
   Self->txFontSize = 10 * 4.0 / 3.0;
   Self->txFamily   = StrClone("Open Sans");
   Self->FillColour.Red   = 1;
   Self->FillColour.Green = 1;
   Self->FillColour.Blue  = 1;
   Self->FillColour.Alpha = 1;
   return ERR_Okay;
}

/*****************************************************************************
-FIELD-
Align: Defines the alignment of the text string.

This field specifies the horizontal alignment of the text string.  The standard alignment flags are supported in the
form of ALIGN_LEFT, ALIGN_HORIZONTAL and ALIGN_RIGHT.

In addition, the SVG equivalent values of 'start', 'middle' and 'end' are supported and map directly to the formerly
mentioned align flags.

*****************************************************************************/

static ERROR TEXT_GET_Align(objVectorText *Self, LONG *Value)
{
   *Value = Self->txAlignFlags;
   return ERR_Okay;
}

static ERROR TEXT_SET_Align(objVectorText *Self, LONG Value)
{
   Self->txAlignFlags = Value;
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
DX: Adjusts horizontal spacing on a per-character basis.

If a single value is provided, it represents the new relative X coordinate for the current text position for
rendering the glyphs corresponding to the first character within this element or any of its descendants.  The current
text position is shifted along the x-axis of the current user coordinate system by the provided value before the first
character's glyphs are rendered.

If a series of values is provided, then the values represent incremental shifts along the x-axis for the current text
position before rendering the glyphs corresponding to the first n characters within this element or any of its
descendants. Thus, before the glyphs are rendered corresponding to each character, the current text position resulting
from drawing the glyphs for the previous character within the current ‘text’ element is shifted along the X axis of the
current user coordinate system by length.

If more characters exist than values, then for each of these extra characters: (a) if an ancestor Text object
specifies a relative X coordinate for the given character via a #DX field, then the current text position
is shifted along the x-axis of the current user coordinate system by that amount (nearest ancestor has precedence),
else (b) no extra shift along the x-axis occurs.

*****************************************************************************/

static ERROR TEXT_GET_DX(objVectorText *Self, DOUBLE **Values, LONG *Elements)
{
   *Values = Self->txDX;
   *Elements = Self->txTotalDX;
   return ERR_Okay;
}

static ERROR TEXT_SET_DX(objVectorText *Self, DOUBLE *Values, LONG Elements)
{
   if (Self->txDX) { FreeResource(Self->txDX); Self->txDX = NULL; Self->txTotalDX = 0; }

   if (!AllocMemory(sizeof(DOUBLE) * Elements, MEM_DATA, &Self->txDX, NULL)) {
      CopyMemory(Values, Self->txDX, Elements * sizeof(DOUBLE));
      Self->txTotalDX = Elements;
      reset_path((objVector *)Self);
      return ERR_Okay;
   }
   else return ERR_AllocMemory;
}

/*****************************************************************************
-FIELD-
DY: Adjusts vertical spacing on a per-character basis.

This field follows the same rules described in #DX.

*****************************************************************************/

static ERROR TEXT_GET_DY(objVectorText *Self, DOUBLE **Values, LONG *Elements)
{
   *Values   = Self->txDY;
   *Elements = Self->txTotalDY;
   return ERR_Okay;
}

static ERROR TEXT_SET_DY(objVectorText *Self, DOUBLE *Values, LONG Elements)
{
   if (Self->txDY) { FreeResource(Self->txDY); Self->txDY = NULL; Self->txTotalDY = 0; }

   if (!AllocMemory(sizeof(DOUBLE) * Elements, MEM_DATA, &Self->txDY, NULL)) {
      CopyMemory(Values, Self->txDY, Elements * sizeof(DOUBLE));
      Self->txTotalDY = Elements;
      reset_path((objVector *)Self);
      return ERR_Okay;
   }
   else return ERR_AllocMemory;
}

/*****************************************************************************
-FIELD-
Face: Defines the font face/family to use in rendering the text string.

The face/family of the desired font for rendering the text is specified here.  It is possible to list multiple fonts
in CSV format in case the first-choice font is unavailable.  For instance, 'Arial,Open Sans' would load the Open Sans
font if Arial was unavailable.

If none of the listed fonts are available, the default system font will be used.

Please note that referencing bitmap fonts is unsupported and they will be ignored by the font loader.

*****************************************************************************/

static ERROR TEXT_GET_Face(objVectorText *Self, CSTRING *Value)
{
   *Value = Self->txFamily;
   return ERR_Okay;
}

static ERROR TEXT_SET_Face(objVectorText *Self, CSTRING Value)
{
   if (Self->txFamily) { FreeResource(Self->txFamily); Self->txFamily = NULL; }
   if (Value) Self->txFamily = StrClone(Value);
   reset_font(Self);
   return ERR_Okay;
}

/*****************************************************************************
-PRIVATE-
Flags: Optional flags.

No flags are currently supported.

*****************************************************************************/

static ERROR TEXT_GET_Flags(objVectorText *Self, LONG *Value)
{
   *Value = Self->txFlags;
   return ERR_Okay;
}

static ERROR TEXT_SET_Flags(objVectorText *Self, LONG Value)
{
   Self->txFlags = Value;
   return ERR_Okay;
}

/*****************************************************************************
-FIELD-
Font: The primary Font object that is used to source glyphs for the text string.

Returns the Font object that is used for drawing the text.  The object may be queried but must remain unmodified.
Any programmed modification that works in the present code base may fail in future releases.

*****************************************************************************/

static ERROR TEXT_GET_Font(objVectorText *Self, OBJECTPTR *Value)
{
   if (!Self->txFont) reset_font(Self);

   if (Self->txFont) {
      *Value = &Self->txFont->Head;
      return ERR_Okay;
   }
   else return ERR_FieldNotSet;
}

/*****************************************************************************
-PRIVATE-
LetterSpacing: Currently unsupported.
-END-
*****************************************************************************/

// SVG standard, presuming this inserts space as opposed to acting as a multiplier

static ERROR TEXT_GET_LetterSpacing(objVectorText *Self, DOUBLE *Value)
{
   *Value = Self->txLetterSpacing;
   return ERR_Okay;
}

static ERROR TEXT_SET_LetterSpacing(objVectorText *Self, DOUBLE Value)
{
   Self->txLetterSpacing = Value;
   reset_path((objVector *)Self);
   return ERR_Okay;
}

/*****************************************************************************
-FIELD-
FontSize: Defines the vertical size of the font.

The FontSize refers to the height of the font from baseline to baseline.  Without an identifier, the height value
corresponds to the current user coordinate system (pixels by default).  If you intend to set the font's point size,
please ensure that 'pt' is appended to the number.

*****************************************************************************/

static ERROR TEXT_GET_FontSize(objVectorText *Self, CSTRING *Value)
{
   char buffer[32];
   IntToStr(Self->txFontSize, buffer, sizeof(buffer));
   *Value = StrClone(buffer);
   return ERR_Okay;
}

static ERROR TEXT_SET_FontSize(objVectorText *Self, CSTRING Value)
{
   if (Value > 0) {
      Self->txFontSize = read_unit(Value, &Self->txRelativeFontSize);
      reset_font(Self);
      return ERR_Okay;
   }
   else return ERR_OutOfRange;
}

/*****************************************************************************
-PRIVATE-
Spacing: Not currently implemented.

*****************************************************************************/

static ERROR TEXT_GET_Spacing(objVectorText *Self, DOUBLE *Value)
{
   *Value = Self->txSpacing;
   return ERR_Okay;
}

static ERROR TEXT_SET_Spacing(objVectorText *Self, DOUBLE Value)
{
   Self->txSpacing = Value;
   reset_path((objVector *)Self);
   return ERR_Okay;
}

/*****************************************************************************
-PRIVATE-
StartOffset: Not currently implemented.

*****************************************************************************/

static ERROR TEXT_GET_StartOffset(objVectorText *Self, DOUBLE *Value)
{
   *Value = Self->txStartOffset;
   return ERR_Okay;
}

static ERROR TEXT_SET_StartOffset(objVectorText *Self, DOUBLE Value)
{
   Self->txStartOffset = Value;
   reset_path((objVector *)Self);
   return ERR_Okay;
}

/*****************************************************************************
-FIELD-
X: The x coordinate of the text.

The x-axis coordinate of the text is specified here as a fixed value.  Relative coordinates are not supported.

*****************************************************************************/

static ERROR TEXT_GET_X(objVectorText *Self, Variable *Value)
{
   DOUBLE val = Self->txX;
   if (Value->Type & FD_PERCENTAGE) val = val * 100.0;
   if (Value->Type & FD_DOUBLE) Value->Double = val;
   else if (Value->Type & FD_LARGE) Value->Large = F2T(val);
   return ERR_Okay;
}

static ERROR TEXT_SET_X(objVectorText *Self, Variable *Value)
{
   if (Value->Type & FD_DOUBLE) Self->txX = Value->Double;
   else if (Value->Type & FD_LARGE) Self->txX = Value->Large;
   else return ERR_FieldTypeMismatch;
   mark_dirty(Self, RC_TRANSFORM);
   return ERR_Okay;
}

/*****************************************************************************
-FIELD-
Y: The base-line y coordinate of the text.

The Y-axis coordinate of the text is specified here as a fixed value.  Relative coordinates are not supported.

Unlike other vector shapes, the Y coordinate positions the text from its base line rather than the top of the shape.

*****************************************************************************/

static ERROR TEXT_GET_Y(objVectorText *Self, Variable *Value)
{
   DOUBLE val = Self->txY;
   if (Value->Type & FD_PERCENTAGE) val = val * 100.0;
   if (Value->Type & FD_DOUBLE) Value->Double = val;
   else if (Value->Type & FD_LARGE) Value->Large = F2T(val);
   return ERR_Okay;
}

static ERROR TEXT_SET_Y(objVectorText *Self, Variable *Value)
{
   if (Value->Type & FD_DOUBLE) Self->txY = Value->Double;
   else if (Value->Type & FD_LARGE) Self->txY = Value->Large;
   else return ERR_FieldTypeMismatch;
   mark_dirty(Self, RC_TRANSFORM);
   return ERR_Okay;
}

/*****************************************************************************
-FIELD-
Rotate: Applies vertical spacing on a per-character basis.

Applies supplemental rotation about the current text position for all of the glyphs in the text string.

If multiple values are provided, then the first number represents the supplemental rotation for the glyphs
corresponding to the first character within this element or any of its descendants, the second number represents the
supplemental rotation for the glyphs that correspond to the second character, and so on.

If more numbers are provided than there are characters, then the extra numbers will be ignored.

If more characters are provided than numbers, then for each of these extra characters the rotation value specified by
the last number must be used.

If the attribute is not specified and if an ancestor 'text' or 'tspan' element specifies a supplemental rotation for a
given character via a 'rotate' attribute, then the given supplemental rotation is applied to the given character
(nearest ancestor has precedence). If there are more characters than numbers specified in the ancestor's 'rotate'
attribute, then for each of these extra characters the rotation value specified by the last number must be used.

This supplemental rotation has no impact on the rules by which current text position is modified as glyphs get rendered
and is supplemental to any rotation due to text on a path and to 'glyph-orientation-horizontal' or
'glyph-orientation-vertical'.

*****************************************************************************/

static ERROR TEXT_GET_Rotate(objVectorText *Self, DOUBLE **Values, LONG *Elements)
{
   *Values = Self->txRotate;
   *Elements = Self->txTotalRotate;
   return ERR_Okay;
}

static ERROR TEXT_SET_Rotate(objVectorText *Self, DOUBLE *Values, LONG Elements)
{
   if (Self->txRotate) { FreeResource(Self->txRotate); Self->txRotate = NULL; Self->txTotalRotate = 0; }

   if (!AllocMemory(sizeof(DOUBLE) * Elements, MEM_DATA, &Self->txRotate, NULL)) {
      CopyMemory(Values, Self->txRotate, Elements * sizeof(DOUBLE));
      Self->txTotalRotate = Elements;
      reset_path((objVector *)Self);
      return ERR_Okay;
   }
   else return ERR_AllocMemory;
}

/*****************************************************************************
-FIELD-
String: The string to use for drawing the glyphs is defined here.

The string for drawing the glyphs is defined here in UTF-8 format.

*****************************************************************************/

static ERROR TEXT_GET_String(objVectorText *Self, CSTRING *Value)
{
   *Value = Self->txString;
   return ERR_Okay;
}

static ERROR TEXT_SET_String(objVectorText *Self, CSTRING Value)
{
   if (Self->txString) { FreeResource(Self->txString); Self->txString = NULL; }
   if (Value) Self->txString = StrClone(Value);
   reset_path((objVector *)Self);
   return ERR_Okay;
}

/*****************************************************************************
-FIELD-
TextLength: Reflects the expected length of the text after all computations have been taken into account.

The purpose of this attribute is to allow exact alignment of the text graphic in the computed result.  If the
#Width that is initially computed does not match this value, then the text will be scaled to match the
TextLength.
-END-
*****************************************************************************/

// NB: Internally we can fulfil TextLength requirements simply by checking the width of the text path boundary
// and if they don't match, apply a rescale transformation just prior to drawing (Width * (TextLength / Width))

static ERROR TEXT_GET_TextLength(objVectorText *Self, DOUBLE *Value)
{
   *Value = Self->txTextLength;
   return ERR_Okay;
}

static ERROR TEXT_SET_TextLength(objVectorText *Self, DOUBLE Value)
{
   Self->txTextLength = Value;
   return ERR_Okay;
}

/*****************************************************************************
-FIELD-
Weight: Defines the level of boldness in the text.

The weight value determines the level of boldness in the text.  A default value of 400 will render the text in its
normal state.  Lower values between 100 to 300 render the text in a light format, while high values in the range of
400 - 900 result in boldness.
-END-
*****************************************************************************/

static ERROR TEXT_GET_Weight(objVectorText *Self, LONG *Value)
{
   *Value = Self->txWeight;
   return ERR_Okay;
}

static ERROR TEXT_SET_Weight(objVectorText *Self, LONG Value)
{
   if ((Value >= 100) and (Value <= 900)) {
      Self->txWeight = Value;
      reset_path((objVector *)Self);
      return ERR_Okay;
   }
   else return ERR_OutOfRange;
}

//****************************************************************************

static const ActionArray clTextActions[] = {
   { AC_Free,      (APTR)TEXT_Free },
   { AC_NewObject, (APTR)TEXT_NewObject },
   //{ AC_Move,        (APTR)TEXT_Move },
   //{ AC_MoveToPoint, (APTR)TEXT_MoveToPoint },
   //{ AC_Redimension, (APTR)TEXT_Redimension },
   //{ AC_Resize,      (APTR)TEXT_Resize },
   { 0, NULL }
};

//****************************************************************************

static const FieldDef clTextFlags[] = {
   { "Underline",   VTXF_UNDERLINE },
   { "Overline",    VTXF_OVERLINE },
   { "LineThrough", VTXF_LINE_THROUGH },
   { "Blink",       VTXF_BLINK },
   { NULL, 0 }
};

static const FieldDef clTextAlign[] = {
   { "Left",       ALIGN_LEFT },
   { "Horizontal", ALIGN_HORIZONTAL },
   { "Right",      ALIGN_RIGHT },
   // SVG synonyms
   { "Start",      ALIGN_LEFT },
   { "Middle",     ALIGN_HORIZONTAL },
   { "End",        ALIGN_RIGHT },
   { NULL, 0 }
};

static const FieldDef clTextStretch[] = {
   { "Normal",         VTS_NORMAL },
   { "Wider",          VTS_WIDER },
   { "Narrower",       VTS_NARROWER },
   { "UltraCondensed", VTS_ULTRA_CONDENSED },
   { "ExtraCondensed", VTS_EXTRA_CONDENSED },
   { "Condensed",      VTS_CONDENSED },
   { "SemiCondensed",  VTS_SEMI_CONDENSED },
   { "Expanded",       VTS_EXPANDED },
   { "SemiExpanded",   VTS_SEMI_EXPANDED },
   { "ExtraExpanded",  VTS_EXTRA_EXPANDED },
   { "UltraExpanded",  VTS_ULTRA_EXPANDED },
   { NULL, 0 }
};

static const FieldArray clTextFields[] = {
   { "X",             FDF_VIRTUAL|FDF_VARIABLE|FDF_DOUBLE|FDF_PERCENTAGE|FDF_RW, 0, (APTR)TEXT_GET_X, (APTR)TEXT_SET_X },
   { "Y",             FDF_VIRTUAL|FDF_VARIABLE|FDF_DOUBLE|FDF_PERCENTAGE|FDF_RW, 0, (APTR)TEXT_GET_Y, (APTR)TEXT_SET_Y },
   { "Weight",        FDF_VIRTUAL|FDF_LONG|FDF_RW,             0, (APTR)TEXT_GET_Weight, (APTR)TEXT_SET_Weight },
   { "String",        FDF_VIRTUAL|FDF_STRING|FDF_RW,           0, (APTR)TEXT_GET_String, (APTR)TEXT_SET_String },
   { "Align",         FDF_VIRTUAL|FDF_LONGFLAGS|FDF_RW,        (MAXINT)&clTextAlign, (APTR)TEXT_GET_Align, (APTR)TEXT_SET_Align },
   { "Face",          FDF_VIRTUAL|FDF_STRING|FDF_RW,           0, (APTR)TEXT_GET_Face, (APTR)TEXT_SET_Face },
   { "FontSize",      FDF_VIRTUAL|FDF_ALLOC|FDF_STRING|FDF_RW, 0, (APTR)TEXT_GET_FontSize, (APTR)TEXT_SET_FontSize },
   { "DX",            FDF_VIRTUAL|FDF_ARRAY|FDF_DOUBLE|FDF_RW, 0, (APTR)TEXT_GET_DX, (APTR)TEXT_SET_DX },
   { "DY",            FDF_VIRTUAL|FDF_ARRAY|FDF_DOUBLE|FDF_RW, 0, (APTR)TEXT_GET_DY, (APTR)TEXT_SET_DY },
   { "LetterSpacing", FDF_VIRTUAL|FDF_DOUBLE|FDF_RW,           0, (APTR)TEXT_GET_LetterSpacing, (APTR)TEXT_SET_LetterSpacing },
   { "Rotate",        FDF_VIRTUAL|FDF_ARRAY|FDF_DOUBLE|FDF_RW, 0, (APTR)TEXT_GET_Rotate, (APTR)TEXT_SET_Rotate },
   { "TextLength",    FDF_VIRTUAL|FDF_DOUBLE|FDF_RW,           0, (APTR)TEXT_GET_TextLength, (APTR)TEXT_SET_TextLength },
   { "Flags",         FDF_VIRTUAL|FDF_LONGFLAGS|FDF_RW,        (MAXINT)&clTextFlags, (APTR)TEXT_GET_Flags, (APTR)TEXT_SET_Flags },
   { "StartOffset",   FDF_VIRTUAL|FDF_DOUBLE|FDF_RW,           0, (APTR)TEXT_GET_StartOffset, (APTR)TEXT_SET_StartOffset },
   { "Spacing",       FDF_VIRTUAL|FDF_DOUBLE|FDF_RW,           0, (APTR)TEXT_GET_Spacing, (APTR)TEXT_SET_Spacing },
   { "Font",          FDF_VIRTUAL|FDF_OBJECT|FDF_R,            0, (APTR)TEXT_GET_Font, NULL },
   END_FIELD
};

//****************************************************************************

static ERROR init_text(void)
{
   return(CreateObject(ID_METACLASS, 0, &clVectorText,
      FID_BaseClassID|TLONG, ID_VECTOR,
      FID_SubClassID|TLONG,  ID_VECTORTEXT,
      FID_Name|TSTRING,      "VectorText",
      FID_Category|TLONG,    CCF_GRAPHICS,
      FID_Actions|TPTR,      clTextActions,
      FID_Fields|TARRAY,     clTextFields,
      FID_Size|TLONG,        sizeof(objVectorText),
      FID_Path|TSTR,         MOD_PATH,
      TAGEND));
}
