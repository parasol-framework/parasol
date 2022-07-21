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

Support for bitmap fonts is included.  This feature is implemented by drawing bitmap characters to an internal @VectorImage
and then rendering that to the viewport as a rectangular path.  It is recommended that this feature is only used for
fast 1:1 rendering without transforms.  The user is otherwise better served through the use of scalable fonts.

-END-

TODO: decompose_ft_outline() should cache the generated paths
      ShapeInside and ShapeSubtract require implementation

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

#define DEFAULT_WEIGHT 400

static void add_line(objVectorText *, std::string, LONG Offset, LONG Length, LONG Line = -1);
static ERROR cursor_timer(objVectorText *, LARGE, LARGE);
static ERROR decompose_ft_outline(const FT_Outline &, bool, agg::path_storage &);
static void delete_selection(objVectorText *);
static void insert_char(objVectorText *, LONG, LONG);
static void generate_text(objVectorText *);
static void generate_text_bitmap(objVectorText *);
static void key_event(objVectorText *, evKey *, LONG);
static void reset_font(objVectorText *);

enum { WS_NO_WORD=0, WS_NEW_WORD, WS_IN_WORD };

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

static ERROR text_focus_event(objVector *Vector, LONG Event)
{
   objVectorText *Self = (objVectorText *)CurrentContext();

   if (Event & FM_HAS_FOCUS) {
      if ((Self->txFlags & VTXF_EDITABLE) and (Self->txCursor.vector)) {
         acMoveToFront(Self->txCursor.vector);

         if (Self->txCursor.timer) UpdateTimer(Self->txCursor.timer, 1.0);
         else {
            auto callback = make_function_stdc(cursor_timer);
            SubscribeTimer(0.8, &callback, &Self->txCursor.timer);

            if (!Self->txKeyEvent) {
               auto callback = make_function_stdc(key_event);
               SubscribeEvent(EVID_IO_KEYBOARD_KEYPRESS, &callback, Self, &Self->txKeyEvent);
            }
         }

         Self->txCursor.resetFlash();
         SetLong(Self->txCursor.vector, FID_Visibility, VIS_VISIBLE);
         acDraw(Self);
      }
   }
   else if (Event & (FM_LOST_FOCUS|FM_CHILD_HAS_FOCUS)) {
      if (Self->txCursor.vector) SetLong(Self->txCursor.vector, FID_Visibility, VIS_HIDDEN);
      if (Self->txCursor.timer)  { UpdateTimer(Self->txCursor.timer, 0); Self->txCursor.timer = 0; }
      if (Self->txKeyEvent)      { UnsubscribeEvent(Self->txKeyEvent); Self->txKeyEvent = NULL; }

      // When a simple input line loses the focus, all selections are deselected

      if (Self->txLineLimit IS 1) {
         if (Self->txFlags & VTXF_AREA_SELECTED) Self->txFlags &= ~VTXF_AREA_SELECTED;
      }

      acDraw(Self);
   }

   return ERR_Okay;
}

/*****************************************************************************

-METHOD-
DeleteLine: Deletes any line number.

This method deletes lines from a text object.  You only need to specify the line number to have it deleted.  If the
line number does not exist, then the call will fail.  The text graphic will be updated as a result of calling this
method.

-INPUT-
int Line: The line number that you want to delete.  If negative, the last line will be deleted.

-ERRORS-
Okay: The line was deleted.
Args: The Line value was out of the valid range.
-END-

*****************************************************************************/

static ERROR VECTORTEXT_DeleteLine(objVectorText *Self, struct vtDeleteLine *Args)
{
   if (Self->txLines.empty()) return ERR_Okay;

   if ((!Args) or (Args->Line < 0)) Self->txLines.pop_back();
   else if ((size_t)Args->Line < Self->txLines.size()) Self->txLines.erase(Self->txLines.begin() + Args->Line);
   else return ERR_Args;

   mark_dirty(Self, RC_BASE_PATH);

   if (Self->txCursor.row() IS Args->Line) {
      Self->txCursor.move(Self, Self->txCursor.row(), 0);
   }
   else if ((size_t)Self->txCursor.row() >= Self->txLines.size()) {
      Self->txCursor.move(Self, Self->txLines.size()-1, Self->txCursor.column());
   }

   Self->txFlags &= ~VTXF_AREA_SELECTED;

   return ERR_Okay;
}

//****************************************************************************

static ERROR VECTORTEXT_Free(objVectorText *Self, APTR Void)
{
   Self->txLines.~vector<TextLine>();
   Self->txCursor.~TextCursor();

   if (Self->txBitmapImage) { acFree(Self->txBitmapImage); Self->txBitmapImage = NULL; }
   if (Self->txAlphaBitmap) { acFree(Self->txAlphaBitmap); Self->txAlphaBitmap = NULL; }
   if (Self->txFamily)      { FreeResource(Self->txFamily); Self->txFamily = NULL; }
   if (Self->txFont)        { acFree(Self->txFont); Self->txFont = NULL; }
   if (Self->txDX)          { FreeResource(Self->txDX); Self->txDX = NULL; }
   if (Self->txDY)          { FreeResource(Self->txDY); Self->txDY = NULL; }
   if (Self->txKeyEvent)    { UnsubscribeEvent(Self->txKeyEvent); Self->txKeyEvent = NULL; }

   if (Self->txFocusID) {
      OBJECTPTR focus;
      if (!AccessObject(Self->txFocusID, 5000, &focus)) {
         auto callback = make_function_stdc(text_focus_event);
         vecSubscribeFeedback(focus, 0, &callback);
         ReleaseObject(focus);
      }
   }

   return ERR_Okay;
}

//****************************************************************************

static ERROR VECTORTEXT_Init(objVectorText *Self, APTR Void)
{
   if (Self->txFlags & VTXF_EDITABLE) {
      if (!Self->txFocusID) {
         for (auto parent=Self->Parent; parent; parent=((objVector *)parent)->Parent) {
            if (parent->SubID IS ID_VECTORVIEWPORT) {
               Self->txFocusID = parent->UID;
               break;
            }
         }
      }

      OBJECTPTR focus;
      if (!AccessObject(Self->txFocusID, 5000, &focus)) {
         auto callback = make_function_stdc(text_focus_event);
         vecSubscribeFeedback(focus, FM_HAS_FOCUS|FM_CHILD_HAS_FOCUS|FM_LOST_FOCUS, &callback);
         ReleaseObject(focus);
      }

      // The editing cursor will inherit transforms from the VectorText as long as it is a direct child.

      if (!CreateObject(ID_VECTORPOLYGON, 0, &Self->txCursor.vector,
            FID_X1|TLONG,     0,
            FID_Y1|TLONG,     0,
            FID_X2|TLONG,     1,
            FID_Y2|TLONG,     1,
            FID_Closed|TLONG, FALSE,
            FID_Stroke|TSTR,  "rgb(255,0,0,255)",
            FID_StrokeWidth|TDOUBLE, 1.25,
            FID_Visibility|TLONG,    VIS_HIDDEN,
            TAGEND)) {

      }
      else return ERR_CreateObject;

      if (Self->txLines.empty()) Self->txLines.emplace_back(std::string(""));
   }

   return ERR_Okay;
}

//****************************************************************************

static ERROR VECTORTEXT_NewObject(objVectorText *Self, APTR Void)
{
   new (&Self->txLines) std::vector<TextLine>;
   new (&Self->txCursor) TextCursor;

   StrCopy("Regular", Self->txFontStyle, sizeof(Self->txFontStyle));
   Self->GeneratePath = (void (*)(rkVector *))&generate_text;
   Self->StrokeWidth  = 0.0;
   Self->txWeight     = DEFAULT_WEIGHT;
   Self->txFontSize   = 10 * 4.0 / 3.0;
   Self->txCharLimit  = 0x7fffffff;
   Self->txFamily     = StrClone("Open Sans");
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
form of `ALIGN_LEFT`, `ALIGN_HORIZONTAL` and `ALIGN_RIGHT`.

In addition, the SVG equivalent values of `start`, `middle` and `end` are supported and map directly to the formerly
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
CharLimit: Limits the total characters allowed in the string.

Set the CharLimit field to limit the number of characters that can appear in the string.  The minimum possible value
is 0 for no characters.  If the object is in edit mode then the user will be unable to extend the string beyond the
limit.

Note that it is valid for the #String length to exceed the limit if set manually.  Only the display of the string
characters will be affected by the CharLimit value.

*****************************************************************************/

static ERROR TEXT_GET_CharLimit(objVectorText *Self, LONG *Value)
{
   *Value = Self->txCharLimit;
   return ERR_Okay;
}

static ERROR TEXT_SET_CharLimit(objVectorText *Self, LONG Value)
{
   if (Value < 0) return ERR_OutOfRange;

   Self->txCharLimit = Value;
   return ERR_Okay;
}

/*****************************************************************************
-FIELD-
CursorColumn: The current column position of the cursor.

****************************************************************************/

static ERROR TEXT_GET_CursorColumn(objVectorText *Self, LONG *Value)
{
   *Value = Self->txCursor.column();
   return ERR_Okay;
}

static ERROR TEXT_SET_CursorColumn(objVectorText *Self, LONG Value)
{
   if (Value >= 0) {
      Self->txCursor.move(Self, Self->txCursor.row(), Value);
      return ERR_Okay;
   }
   else return ERR_InvalidValue;
}

/****************************************************************************
-FIELD-
CursorRow: The current line position of the cursor.

****************************************************************************/

static ERROR TEXT_GET_CursorRow(objVectorText *Self, LONG *Value)
{
   *Value = Self->txCursor.row();
   return ERR_Okay;
}

static ERROR TEXT_SET_CursorRow(objVectorText *Self, LONG Value)
{
   if (Value >= 0) {
      if (Value < Self->txTotalLines) Self->txCursor.move(Self, Value, Self->txCursor.column());
      else Self->txCursor.move(Self, Self->txTotalLines - 1, Self->txCursor.column());
      return ERR_Okay;
   }
   else return ERR_InvalidValue;
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
in CSV format in case the first-choice font is unavailable.  For instance, `Arial,Open Sans` would load the Open Sans
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
TextFlags: Optional flags.

-END-
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
Focus: Refers to the object that will be monitored for user focussing.

A VectorText object in edit mode will become active when its nearest viewport receives the focus.  Setting the Focus
field to a different vector in the scene graph will redirect monitoring to it.

Changing this value post-initialisation has no effect.

*****************************************************************************/

static ERROR TEXT_GET_Focus(objVectorText *Self, OBJECTID *Value)
{
   *Value = Self->txFocusID;
   return ERR_Okay;
}

static ERROR TEXT_SET_Focus(objVectorText *Self, OBJECTID Value)
{
   Self->txFocusID = Value;
   return ERR_Okay;
}

/*****************************************************************************
-FIELD-
Font: The primary Font object that is used to source glyphs for the text string.

Returns the @Font object that is used for drawing the text.  The object may be queried but must remain unmodified.
Any modification by the client that happens to work in the present code release may fail in future releases.

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

The FontSize refers to the height of the font from baseline to baseline.  By default, the value corresponds to the
current user coordinate system in pixels.  To define the point size, append 'pt' to the number.

If retrieving the font size, the string must be freed by the client when no longer in use.

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
-FIELD-
FontStyle: Determines font styling.

Unique styles for a font can be selected through the FontStyle field.  Conventional font styles are `Bold`,
`Bold Italic`, `Italic` and `Regular` (the default).  Because TrueType fonts can use any style name that the
designer chooses such as `Thin`, `Narrow` or `Wide`, use ~Font.GetList() for a definitive list of available
style names.

Errors are not returned if the style name is invalid or unavailable.

*****************************************************************************/

static ERROR TEXT_GET_FontStyle(objVectorText *Self, CSTRING *Value)
{
   *Value = Self->txFontStyle;
   return ERR_Okay;
}

static ERROR TEXT_SET_FontStyle(objVectorText *Self, CSTRING Value)
{
   if ((!Value) or (!Value[0])) StrCopy("Regular", Self->txFontStyle, sizeof(Self->txFontStyle));
   else StrCopy(Value, Self->txFontStyle, sizeof(Self->txFontStyle));
   return ERR_Okay;
}

/*****************************************************************************
-FIELD-
InlineSize: Enables word-wrapping at a fixed area size.

The inline-size property allows one to set the wrapping area to a rectangular shape. The computed value of the
property sets the width of the rectangle for horizontal text and the height of the rectangle for vertical text.
The other dimension (height for horizontal text, width for vertical text) is of infinite length. A value of zero
(the default) disables the creation of a wrapping area.

*****************************************************************************/

static ERROR TEXT_GET_InlineSize(objVectorText *Self, DOUBLE *Value)
{
   *Value = Self->txInlineSize;
   return ERR_Okay;
}

static ERROR TEXT_SET_InlineSize(objVectorText *Self, DOUBLE Value)
{
   Self->txInlineSize = Value;
   reset_path((objVector *)Self);
   return ERR_Okay;
}

/*****************************************************************************
-FIELD-
LineLimit: Restricts the total number of lines allowed in a text object.

Set the LineLimit field to restrict the maximum number of lines permitted in a text object.  It is common to set this
field to a value of 1 for input boxes that have a limited amount of space available.

*****************************************************************************/

static ERROR TEXT_GET_LineLimit(objVectorText *Self, LONG *Value)
{
   *Value = Self->txLineLimit;
   return ERR_Okay;
}

static ERROR TEXT_SET_LineLimit(objVectorText *Self, LONG Value)
{
   Self->txLineLimit = Value;
   return ERR_Okay;
}

/****************************************************************************
-FIELD-
SelectColumn: Indicates the column position of a selection's beginning.

If the user has selected an area of text, the starting column of that area will be indicated by this field.  If an area
has not been selected, the value of the SelectColumn field is undefined.

To check whether or not an area has been selected, test the `AREA_SELECTED` bit in the #Flags field.

*****************************************************************************/

static ERROR TEXT_GET_SelectColumn(objVectorText *Self, LONG *Value)
{
   *Value = Self->txCursor.selectColumn;
   return ERR_Okay;
}

/****************************************************************************
-FIELD-
SelectRow: Indicates the line position of a selection's beginning.

If the user has selected an area of text, the starting row of that area will be indicated by this field.  If an area
has not been selected, the value of the SelectRow field is undefined.

To check whether or not an area has been selected, test the `AREA_SELECTED` bit in the #Flags field.

*****************************************************************************/

static ERROR TEXT_GET_SelectRow(objVectorText *Self, LONG *Value)
{
   *Value = Self->txCursor.selectRow;
   return ERR_Okay;
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
ShapeInside: Reference a vector shape to define a content area that enables word-wrapping.

This property enables word-wrapping in which the text will conform to the path of a @Vector shape.  Internally this is
achieved by rendering the vector path as a mask and then fitting the text within the mask without crossing its
boundaries.

This feature is computationally expensive and the use of #InlineSize is preferred if the text can be wrapped to
a rectangular area.

*****************************************************************************/

static ERROR TEXT_GET_ShapeInside(objVectorText *Self, OBJECTID *Value)
{
   *Value = Self->txShapeInsideID;
   return ERR_Okay;
}

static ERROR TEXT_SET_ShapeInside(objVectorText *Self, OBJECTID Value)
{
   Self->txShapeInsideID = Value;
   return ERR_Okay;
}

/*****************************************************************************
-FIELD-
ShapeSubtract: Excludes a portion of the content area from the wrapping area.

This property can be used in conjunction with #ShapeInside to further restrict the content area that is available
for word-wrapping.  It has no effect if #ShapeInside is undefined.

*****************************************************************************/

static ERROR TEXT_GET_ShapeSubtract(objVectorText *Self, OBJECTID *Value)
{
   *Value = Self->txShapeSubtractID;
   return ERR_Okay;
}

static ERROR TEXT_SET_ShapeSubtract(objVectorText *Self, OBJECTID Value)
{
   Self->txShapeSubtractID = Value;
   return ERR_Okay;
}

/*****************************************************************************
-FIELD-
String: The string to use for drawing the glyphs is defined here.

The string for drawing the glyphs is defined here in UTF-8 format.

When retrieving a string that contains return codes, only the first line of text is returned.

*****************************************************************************/

static ERROR TEXT_GET_String(objVectorText *Self, CSTRING *Value)
{
   if (Self->txLines.size() > 0) {
      *Value = Self->txLines[0].c_str();
      return ERR_Okay;
   }
   else return ERR_FieldNotSet;
}

static ERROR TEXT_SET_String(objVectorText *Self, CSTRING Value)
{
   Self->txLines.clear();

   if (Value) {
      while (*Value) {
         size_t total;
         for (total=0; (Value[total]) and (Value[total] != '\n'); total++);
         Self->txLines.emplace_back(std::string(Value, total));
         Value += total;
         if (*Value IS '\n') Value++;
      }
   }

   reset_path((objVector *)Self);
   if (Self->txCursor.vector) Self->txCursor.validatePosition(Self);
   return ERR_Okay;
}

/*****************************************************************************
-FIELD-
TextLength: The expected length of the text after all computations have been taken into account.

The purpose of this attribute is to allow exact alignment of the text graphic in the computed result.  If the
#Width that is initially computed does not match this value, then the text will be scaled to match the
TextLength.

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
TextWidth: The raw pixel width of the widest line in the @String value..

*****************************************************************************/

static ERROR TEXT_GET_TextWidth(objVectorText *Self, LONG *Value)
{
   if (!(Self->Head.Flags & NF_INITIALISED)) return ERR_NotInitialised;

   if (!Self->txFont) reset_font(Self);

   LONG width = 0;
   for (auto &line : Self->txLines) {
      LONG w = fntStringWidth(Self->txFont, line.c_str(), -1);
      if (w > width) width = w;
   }
   *Value = width;
   return ERR_Okay;
}

/*****************************************************************************
-FIELD-
TotalLines: The total number of lines stored in the object.

*****************************************************************************/

static ERROR TEXT_GET_TotalLines(objVectorText *Self, LONG *Value)
{
   *Value = Self->txLines.size();
   return ERR_Okay;
}

/*****************************************************************************
-FIELD-
Weight: Defines the level of boldness in the text.

The weight value determines the level of boldness in the text.  A default value of 400 will render the text in its
normal state.  Lower values between 100 to 300 render the text in a light format, while high values in the range of
400 - 900 result in boldness.

Please note that setting the Weight will give it priority over the #FontStyle value.
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
// Calculate the cursor that would be displayed at this character position and save it to the
// line's chars array.

static void calc_cursor_position(TextLine &Line, agg::trans_affine &transform, DOUBLE PointSize, DOUBLE PathScale)
{
   agg::path_storage cursor_path;
   DOUBLE cx1, cy1, cx2, cy2;

   cursor_path.move_to(0, PointSize * 0.1);
   cursor_path.line_to(0, -(PointSize * PathScale) - (PointSize * 0.2));
   agg::conv_transform<agg::path_storage, agg::trans_affine> trans_cursor(cursor_path, transform);

   trans_cursor.vertex(&cx1, &cy1);
   trans_cursor.vertex(&cx2, &cy2);
   Line.chars.emplace_back(cx1, cy1, cx2, cy2);
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

   auto &lines = Vector->txLines;
   if (lines.empty()) return;

   if (!(Vector->txFont->Flags & FTF_SCALABLE)) {
      generate_text_bitmap(Vector);
      return;
   }

   FT_Face ftface;
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

         if (!morph->BasePath.total_vertices()) morph = NULL;
         else {
            morph->BasePath.rewind(0);
            morph->BasePath.vertex(0, &start_x, &start_y);
            end_vx = start_x;
            end_vy = start_y;
            if (morph->PathLength > 0) {
               path_scale = morph->PathLength / agg::path_length(morph->BasePath);
               morph->BasePath.rewind(0);
            }
         }
      }
   }

   // Compute the string length in characters if a transition is being applied

   LONG total_chars = 0;
   if (Vector->Transition) {
      for (auto const &line : Vector->txLines) {
         for (auto lstr=line.c_str(); *lstr; ) {
            LONG char_len;
            LONG unicode = UTF8ReadValue(lstr, &char_len);
            lstr += char_len;
            if (unicode <= 0x20) continue;
            else if (!unicode) break;
            total_chars++;
         }
      }
   }

   // Upscaling is used to get the Freetype engine to generate accurate vertices and advance coordinates, which is
   // important if the characters are being transformed.  There is a limit to the upscale value.  100 seems to be
   // reasonable; anything 1000+ results in issues.

   DOUBLE upscale = 1;
   if ((Vector->Transition) or (morph)) upscale = 100;

   // The scale_char transform is applied to each character to ensure that it is scaled to the path correctly.
   // The '3/4' conversion makes sense if you refer to read_unit() and understand that a point is 3/4 of a pixel.

   agg::trans_affine scale_char;
   const DOUBLE point_size = Vector->txFontSize * (3.0 / 4.0) * upscale;

   if (path_scale != 1.0) {
      scale_char.translate(0, point_size);
      scale_char.scale(path_scale);
      scale_char.translate(0, -point_size * path_scale);
   }
   scale_char.scale(1.0 / upscale); // Downscale the generated character to the correct size.

   if (!Vector->FreetypeSize) EFT_New_Size(ftface, &Vector->FreetypeSize);
   if (Vector->FreetypeSize != ftface->size) EFT_Activate_Size(Vector->FreetypeSize);

   if (ftface->size->metrics.height != dbl_to_int26p6(point_size)) {
      EFT_Set_Char_Size(ftface, 0, dbl_to_int26p6(point_size), FIXED_DPI, FIXED_DPI);
   }

   agg::path_storage char_path;
   agg::path_storage cursor_path;
   LONG prev_glyph = 0;
   LONG cmd = -1;
   LONG char_index = 0;
   LONG current_row = 0;
   DOUBLE dist = 0; // Distance to next vertex
   DOUBLE angle = 0;
   DOUBLE longest_line_width = 0;

   if (morph) {
      for (auto &line : Vector->txLines) {
         LONG current_col = 0;
         line.chars.clear();
         auto wrap_state = WS_NO_WORD;
         for (auto str=line.c_str(); *str; ) {
            LONG char_len;
            LONG unicode = UTF8ReadValue(str, &char_len);
            str += char_len;

            if (unicode <= 0x20) wrap_state = WS_NO_WORD;
            else if (wrap_state IS WS_NEW_WORD) wrap_state = WS_IN_WORD;
            else wrap_state = WS_NEW_WORD;

            if (!unicode) break;

            char_index++; // Character index only increases if a glyph is being drawn

            agg::trans_affine transform(scale_char); // The initial transform scales the char to the path.

            if (Vector->Transition) { // Apply any special transitions early.
               apply_transition(Vector->Transition, DOUBLE(char_index) / DOUBLE(total_chars), transform);
            }

            LONG glyph = EFT_Get_Char_Index(ftface, unicode);

            if (!EFT_Load_Glyph(ftface, glyph, FT_LOAD_LINEAR_DESIGN)) {
               char_path.free_all();
               if (!decompose_ft_outline(ftface->glyph->outline, true, char_path)) {
                  DOUBLE kx, ky;
                  get_kerning_xy(ftface, glyph, prev_glyph, &kx, &ky);

                  DOUBLE char_width = int26p6_to_dbl(ftface->glyph->advance.x) + kx;

                  char_width = char_width * ABS(transform.sx);
                  //char_width = char_width * transform.scale();

                  // Compute end_vx,end_vy (the last vertex to use for angle computation) and store the distance from start_x,start_y to end_vx,end_vy in dist.
                  if (char_width > dist) {
                     while (cmd != agg::path_cmd_stop) {
                        DOUBLE current_x, current_y;
                        cmd = morph->BasePath.vertex(&current_x, &current_y);
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
                     agg::conv_transform<agg::path_storage, agg::trans_affine> trans_char(char_path, transform);
                     Vector->BasePath.concat_path(trans_char);
                  }

                  //dx += char_width;
                  //dy += int26p6_to_dbl(ftface->glyph->advance.y) + ky;
               }
               else log.trace("Failed to get outline of character.");
            }
            prev_glyph = glyph;
            current_col++;
         }

         current_row++;
      }
      Vector->txWidth = start_x;
   }
   else {
      DOUBLE dx = 0, dy = 0; // Text coordinate tracking from (0,0), not transformed
      for (auto &line : Vector->txLines) {
         LONG current_col = 0;
         line.chars.clear();
         auto wrap_state = WS_NO_WORD;
         if ((line.empty()) and (Vector->txCursor.vector)) {
            agg::trans_affine transform(scale_char);
            calc_cursor_position(line, transform, point_size, path_scale);
         }
         else for (auto str=line.c_str(); *str; ) {
            LONG char_len;
            LONG unicode = UTF8ReadValue(str, &char_len);

            if (unicode <= 0x20) { wrap_state = WS_NO_WORD; prev_glyph = 0; }
            else if (wrap_state IS WS_NEW_WORD) wrap_state = WS_IN_WORD;
            else if (wrap_state IS WS_NO_WORD)  wrap_state = WS_NEW_WORD;

            if (!unicode) break;

            char_index++; // Character index only increases if a glyph is being drawn

            agg::trans_affine transform(scale_char); // The initial transform scales the char to the path.

            if (Vector->Transition) { // Apply any special transitions early.
               apply_transition(Vector->Transition, DOUBLE(char_index) / DOUBLE(total_chars), transform);
            }

            // Determine if this is a new word that would wrap if drawn.
            // TODO: Wrapping information should be cached for speeding up subsequent redraws.

            if ((wrap_state IS WS_NEW_WORD) and (Vector->txInlineSize)) {
               LONG word_length = 0;
               for (LONG c=0; (str[c]) and (str[c] > 0x20); ) {
                  for (++c; ((str[c] & 0xc0) IS 0x80); c++);
                  word_length++;
               }

               LONG word_width = fntStringWidth(Vector->txFont, str, word_length);

               if ((dx + word_width) * ABS(transform.sx) >= Vector->txInlineSize) {
                  dx = 0;
                  dy += Vector->txFont->LineSpacing;
               }
            }

            str += char_len;

            LONG glyph = EFT_Get_Char_Index(ftface, unicode);

            if (!EFT_Load_Glyph(ftface, glyph, FT_LOAD_LINEAR_DESIGN)) {
               char_path.free_all();
               if (!decompose_ft_outline(ftface->glyph->outline, true, char_path)) {
                  DOUBLE kx, ky;
                  get_kerning_xy(ftface, glyph, prev_glyph, &kx, &ky);

                  DOUBLE char_width = int26p6_to_dbl(ftface->glyph->advance.x) + kx;

                  char_width = char_width * ABS(transform.sx);
                  //char_width = char_width * transform.scale();

                  transform.translate(dx, dy);
                  agg::conv_transform<agg::path_storage, agg::trans_affine> trans_char(char_path, transform);
                  Vector->BasePath.concat_path(trans_char);

                  if (Vector->txCursor.vector) {
                     calc_cursor_position(line, transform, point_size, path_scale);

                     if (!*str) { // Last character reached, add a final cursor entry past the character position.
                        transform.translate(char_width, 0);
                        calc_cursor_position(line, transform, point_size, path_scale);
                     }
                  }

                  dx += char_width;
                  dy += int26p6_to_dbl(ftface->glyph->advance.y) + ky;
               }
               else log.trace("Failed to get outline of character.");
            }

            prev_glyph = glyph;
            current_col++;
         }

         if (dx > longest_line_width) longest_line_width = dx;
         dx = 0;
         dy += Vector->txFont->LineSpacing;
         current_row++;
      }

      Vector->txWidth = longest_line_width;
   }

   if (Vector->txCursor.vector) {
      Vector->txCursor.resetVector(Vector);
   }
}

//****************************************************************************
// Bitmap fonts are drawn as a rectangular block referencing a VectorImage texture that contains the rendered font.

static void generate_text_bitmap(objVectorText *Vector)
{
   parasol::Log log(__FUNCTION__);

   if (!Vector->txFont) {
      reset_font(Vector);
      if (!Vector->txFont) return;
   }

   auto &lines = Vector->txLines;
   if (!lines.size()) return;

   LONG dx = 0, dy = Vector->txFont->Leading;
   LONG longest_line_width = 0;

   if ((!Vector->txInlineSize) and (!(Vector->txCursor.vector))) { // Fast calculation if no wrapping or active cursor
      for (auto &line : Vector->txLines) {
         line.chars.clear();
         LONG line_width = fntStringWidth(Vector->txFont, line.c_str(), -1);
         if (line_width > longest_line_width) longest_line_width = line_width;
         dy += Vector->txFont->LineSpacing;
      }
   }
   else {
      agg::path_storage cursor_path;
      LONG prev_glyph = 0;

      for (auto &line : Vector->txLines) {
         line.chars.clear();
         auto wrap_state = WS_NO_WORD;
         for (auto str=line.c_str(); *str; ) {
            LONG char_len;
            auto unicode = UTF8ReadValue(str, &char_len);

            LONG kerning;
            auto char_width = fntCharWidth(Vector->txFont, unicode, prev_glyph, &kerning);

            if (unicode <= 0x20) { wrap_state = WS_NO_WORD; prev_glyph = 0; }
            else if (wrap_state IS WS_NEW_WORD) wrap_state = WS_IN_WORD;
            else if (wrap_state IS WS_NO_WORD)  wrap_state = WS_NEW_WORD;

            if (!unicode) break;

            // Determine if this is a new word that would wrap if drawn.
            // TODO: Wrapping information should be cached for speeding up subsequent redraws.

            if ((wrap_state IS WS_NEW_WORD) and (Vector->txInlineSize)) {
               LONG word_length = 0;
               for (LONG c=0; (str[c]) and (str[c] > 0x20); ) {
                  for (++c; ((str[c] & 0xc0) IS 0x80); c++);
                  word_length++;
               }

               LONG word_width = fntStringWidth(Vector->txFont, str, word_length);

               if (dx + word_width >= Vector->txInlineSize) {
                  if (dx > longest_line_width) longest_line_width = dx;
                  dx = 0;
                  dy += Vector->txFont->LineSpacing;
               }
            }

            str += char_len;

            if (Vector->txCursor.vector) {
               line.chars.emplace_back(dx, dx, dy + 1, dy - ((DOUBLE)Vector->txFont->Height * 1.2));
               if (!*str) { // Last character reached, add a final cursor entry past the character position.
                  line.chars.emplace_back(dx + kerning + char_width, dy, dx + kerning + char_width, dy - ((DOUBLE)Vector->txFont->Height * 1.2));
               }
            }

            dx += kerning + char_width;
            prev_glyph = unicode;
         }

         if (dx > longest_line_width) longest_line_width = dx;
         dx = 0;
         dy += Vector->txFont->LineSpacing;
      }
   }

   if (dy < Vector->txFont->LineSpacing) dy = Vector->txFont->LineSpacing; // Enforce min. height

   // Standard rectangle to host the text image.

   Vector->BasePath.move_to(0, 0);
   Vector->BasePath.line_to(longest_line_width, 0);
   Vector->BasePath.line_to(longest_line_width, dy);
   Vector->BasePath.line_to(0, dy);
   Vector->BasePath.close_polygon();

   Vector->txWidth = longest_line_width;

   if (Vector->txCursor.vector) {
      Vector->txCursor.resetVector(Vector);
   }

   if (!Vector->txBitmapImage) {
      if (CreateObject(ID_BITMAP, NF_INTEGRAL, &Vector->txAlphaBitmap,
            FID_Width|TLONG,        longest_line_width,
            FID_Height|TLONG,       dy,
            FID_BitsPerPixel|TLONG, 32,
            FID_Flags|TLONG,        BMF_ALPHA_CHANNEL,
            TAGEND) != ERR_Okay) return;

      if (CreateObject(ID_VECTORIMAGE, NF_INTEGRAL, &Vector->txBitmapImage,
            FID_Bitmap|TPTR,        Vector->txAlphaBitmap,
            FID_SpreadMethod|TLONG, VSPREAD_CLIP,
            FID_Units|TLONG,        VUNIT_BOUNDING_BOX,
            FID_AspectRatio|TLONG,  ARF_X_MIN|ARF_Y_MIN,
            TAGEND) != ERR_Okay) return;
   }
   else acResize(Vector->txAlphaBitmap, longest_line_width, dy, 0);

   Vector->FillImage = Vector->txBitmapImage;
   Vector->DisableFillColour = true;

   Vector->txFont->Bitmap = Vector->txAlphaBitmap;

   gfxDrawRectangle(Vector->txAlphaBitmap, 0, 0, Vector->txAlphaBitmap->Width, Vector->txAlphaBitmap->Height, 0x000000ff, BAF_FILL);

   if (Vector->txInlineSize) Vector->txFont->WrapEdge = Vector->txInlineSize;

   LONG y = Vector->txFont->Leading;
   for (auto &line : Vector->txLines) {
      auto str = line.c_str();
      if (!str[0]) y += Vector->txFont->LineSpacing;
      else {
         SetString(Vector->txFont, FID_String, str);
         Vector->txFont->X = 0;
         Vector->txFont->Y = y;
         acDraw(Vector->txFont);

         if (Vector->txInlineSize) y = Vector->txFont->EndY + Vector->txFont->LineSpacing;
         else y += Vector->txFont->LineSpacing;
      }
   }
}

//****************************************************************************
// Converts a Freetype glyph outline to an AGG path.  The size of the font must be preset in the FT_Outline object,
// with a call to FT_Set_Char_Size()

static ERROR decompose_ft_outline(const FT_Outline &outline, bool flip_y, agg::path_storage &path)
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

   if (!(Vector->txFont->Flags & FTF_SCALABLE)) {
      // Bitmap fonts need an adjustment because the Y coordinate corresponds to the base-line.
      y -= Vector->txFont->Height + Vector->txFont->Leading;
   }

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
   log.branch("Style: %s, Weight: %d", Vector->txFontStyle, Vector->txWeight);
   parasol::SwitchContext context(Vector);

   objFont *font;
   if (!NewObject(ID_FONT, NF_INTEGRAL, &font)) {
      // Note that we don't configure too much of the font, as AGG uses the Freetype functions directly.  The
      // use of the Font object is really as a place-holder to take advantage of the Parasol font cache.

      if (Vector->txFamily) {
         std::string family(Vector->txFamily);
         family.append(",Open Sans");

         std::string style(Vector->txFontStyle);

         if (Vector->txWeight != DEFAULT_WEIGHT) {
            style = "Regular";
            if (Vector->txWeight >= 700) style = "Extra Bold";
            else if (Vector->txWeight >= 500) style = "Bold";
            else if (Vector->txWeight <= 200) style = "Extra Light";
            else if (Vector->txWeight <= 300) style = "Light";

            if (!StrMatch("Italic", Vector->txFontStyle)) {
               if (style IS "Regular") style = "Italic";
               else style.append(" Italic");
            }
            SetString(font, FID_Style, style.c_str());
         }
         else SetString(font, FID_Style, Vector->txFontStyle);

         CSTRING location;
         if (!fntSelectFont(family.c_str(), style.c_str(), Vector->txFontSize, FTF_PREFER_SCALED, &location)) {
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

static ERROR cursor_timer(objVectorText *Self, LARGE Elapsed, LARGE CurrentTime)
{
   if ((Self->txFlags & VTXF_EDITABLE) and (Self->txCursor.vector)) {
      parasol::Log log(__FUNCTION__);
      Self->txCursor.flash ^= 1;
      SetLong(Self->txCursor.vector, FID_Visibility, Self->txCursor.flash ? VIS_VISIBLE : VIS_HIDDEN);
      acDraw(Self);
      return ERR_Okay;
   }
   else {
      Self->txCursor.timer = 0;
      return ERR_Terminate;
   }
}

//****************************************************************************

static void add_line(objVectorText *Self, std::string String, LONG Offset, LONG Length, LONG Line)
{
   if (Length < 0) Length = String.length();

   // Stop the string from exceeding the acceptable character limit

   if (Length >= Self->txCharLimit) {
      LONG i;
      for (LONG unicodelen=0, i=0; (i < Length) and (unicodelen < Self->txCharLimit); unicodelen++) {
         for (++i; (String[i] & 0xc0) IS 0x80; i++);
      }
      Length = i;
   }

   if (Line < 0) {
      Self->txLines.emplace_back(String.substr(Offset, Length));
   }
   else {
      TextLine tl(String.substr(Offset, Length));
      Self->txLines.insert(Self->txLines.begin() + Line, 1, tl);
   }

   acDraw(Self);
}

//****************************************************************************

static void key_event(objVectorText *Self, evKey *Event, LONG Size)
{
   if (!(Event->Qualifiers & KQ_PRESSED)) return;

   parasol::Log log(__FUNCTION__);

   log.trace("$%.8x, Value: %d", Event->Qualifiers, Event->Code);

   Self->txCursor.resetFlash(); // Reset the flashing cursor to make it visible
   SetLong(Self->txCursor.vector, FID_Visibility, VIS_VISIBLE);

   if ((!(Self->txFlags & VTXF_NO_SYS_KEYS)) and (Event->Qualifiers & KQ_CTRL)) {
      switch(Event->Code) {
         case K_C: // Copy
            acClipboard(Self, CLIPMODE_COPY);
            return;

         case K_X: // Cut
            if (!(Self->txFlags & VTXF_EDITABLE)) return;
            acClipboard(Self, CLIPMODE_CUT);
            return;

         case K_V: // Paste
            if (!(Self->txFlags & VTXF_EDITABLE)) return;
            acClipboard(Self, CLIPMODE_PASTE);
            return;

         case K_K: // Delete line
            if (!(Self->txFlags & VTXF_EDITABLE)) return;
            vtDeleteLine(Self, Self->txCursor.row());
            return;

         case K_Z: // Undo
            if (!(Self->txFlags & VTXF_EDITABLE)) return;
            return;

         case K_Y: // Redo
            if (!(Self->txFlags & VTXF_EDITABLE)) return;
            return;
      }
   }

   if (!(Event->Qualifiers & KQ_NOT_PRINTABLE)) { // and (!(Flags & KQ_INSTRUCTIONKEYS))
      // Printable character handling

      if (!(Self->txFlags & VTXF_EDITABLE)) {
         log.trace("Object does not have the EDIT flag set.");
         return;
      }

      if (Self->txFlags & VTXF_AREA_SELECTED) delete_selection(Self);

      insert_char(Self, Event->Unicode, Self->txCursor.column());
      return;
   }

   switch(Event->Code) {
   case K_BACKSPACE:
      if (Self->txFlags & VTXF_AREA_SELECTED) delete_selection(Self);
      else if (Self->txCursor.column() > 0) {
         if ((size_t)Self->txCursor.column() > Self->txLines[Self->txCursor.row()].length()) {
            Self->txCursor.move(Self, Self->txCursor.row(), Self->txLines[Self->txCursor.row()].lastChar()-1);
         }
         else Self->txCursor.move(Self, Self->txCursor.row(), Self->txCursor.column()-1);

         auto offset = Self->txLines[Self->txCursor.row()].utf8CharOffset(Self->txCursor.column());
         Self->txLines[Self->txCursor.row()].replace(offset, Self->txLines[Self->txCursor.row()].charLength(offset), "");
      }
      else if (Self->txCursor.row() > 0) { // The current line will be shifted up into the line above it
         if ((!Self->txLines[Self->txCursor.row()-1].empty()) or (!Self->txLines[Self->txCursor.row()].empty())) {
            Self->txLines[Self->txCursor.row()-1] += Self->txLines[Self->txCursor.row()];
            Self->txLines.erase(Self->txLines.begin() + Self->txCursor.row());
            Self->txCursor.move(Self, Self->txCursor.row()-1, Self->txLines[Self->txCursor.row()].utf8Length());
         }
         else {
            Self->txLines.erase(Self->txLines.begin() + Self->txCursor.row());
            Self->txCursor.move(Self, Self->txCursor.row()-1, Self->txLines[Self->txCursor.row()-1].length());
         }
      }
      else; // Do nothing, cursor at (0,0)

      mark_dirty(Self, RC_BASE_PATH);
      acDraw(Self);
      break;

   case K_CLEAR:
      if (Self->txFlags & VTXF_AREA_SELECTED) delete_selection(Self);
      else {
         Self->txCursor.move(Self, Self->txCursor.row(), 0);
         vtDeleteLine(Self, Self->txCursor.row());
      }
      break;

   case K_DELETE:
      if (Self->txFlags & VTXF_AREA_SELECTED) delete_selection(Self);
      else if ((size_t)Self->txCursor.column() < Self->txLines[Self->txCursor.row()].length()) {
         auto offset = Self->txLines[Self->txCursor.row()].utf8CharOffset(Self->txCursor.column());
         Self->txLines[Self->txCursor.row()].replace(offset, Self->txLines[Self->txCursor.row()].charLength(offset), "");
      }
      else if ((size_t)Self->txCursor.row() < Self->txLines.size() - 1) { // The next line is going to be pulled up into the current line
         Self->txLines[Self->txCursor.row()] += Self->txLines[Self->txCursor.row()+1];
         Self->txLines.erase(Self->txLines.begin() + Self->txCursor.row() + 1);
      }
      mark_dirty(Self, RC_BASE_PATH);
      acDraw(Self);
      break;

   case K_END:
      Self->txCursor.move(Self, Self->txCursor.row(), Self->txLines[Self->txCursor.row()].length());
      break;

   case K_TAB:
      break;

   case K_ENTER:
   case K_NP_ENTER: {
      if ((Self->txLineLimit) and (Self->txLines.size() >= (size_t)Self->txLineLimit)) break;

      if (Self->txFlags & VTXF_AREA_SELECTED) delete_selection(Self);
      if (Self->txLines.empty()) Self->txLines.resize(1);

      auto row    = Self->txCursor.row();
      auto offset = Self->txLines[row].utf8CharOffset(Self->txCursor.column());
      Self->txCursor.move(Self, row + 1, 0);
      add_line(Self, Self->txLines[row], offset, Self->txLines[row].length() - offset, row+1);

      if (!offset) Self->txLines[row].clear();
      else Self->txLines[row].replace(offset, Self->txLines[row].length() - offset, "");
      break;
   }

   case K_HOME:
      Self->txCursor.move(Self, Self->txCursor.row(), 0);
      break;

   case K_INSERT:
      if (Self->txFlags & VTXF_OVERWRITE) Self->txFlags &= ~VTXF_OVERWRITE;
      else Self->txFlags |= VTXF_OVERWRITE;
      break;

   case K_LEFT:
      Self->txCursor.resetFlash();
      SetLong(Self->txCursor.vector, FID_Visibility, VIS_VISIBLE);
      if (Self->txCursor.column() > 0) Self->txCursor.move(Self, Self->txCursor.row(), Self->txCursor.column()-1);
      else if (Self->txCursor.row() > 0) Self->txCursor.move(Self, Self->txCursor.row()-1, Self->txLines[Self->txCursor.row()-1].utf8Length());
      break;

   case K_RIGHT:
      Self->txCursor.resetFlash();
      SetLong(Self->txCursor.vector, FID_Visibility, VIS_VISIBLE);
      if (!Self->txLines.empty()) {
         if (Self->txCursor.column() < Self->txLines[Self->txCursor.row()].utf8Length()) {
            Self->txCursor.move(Self, Self->txCursor.row(), Self->txCursor.column()+1);
         }
         else if ((size_t)Self->txCursor.row() < Self->txLines.size() - 1) {
            Self->txCursor.move(Self, Self->txCursor.row()+1, 0);
         }
      }
      break;

   case K_DOWN:
   case K_UP:
      Self->txCursor.resetFlash();
      SetLong(Self->txCursor.vector, FID_Visibility, VIS_VISIBLE);
      if (((Event->Code IS K_UP) and (Self->txCursor.row() > 0)) or
          ((Event->Code IS K_DOWN) and ((size_t)Self->txCursor.row() < Self->txLines.size()-1))) {
         LONG end_column;

         // Determine the current true position of the current cursor column, in UTF-8.  Then determine the cursor
         // character that we are going to be at when we end up at the row above us.

         if (((Self->txCursor.row() << 16) | Self->txCursor.column()) IS Self->txCursor.savePos) {
            end_column = Self->txCursor.endColumn;
         }
         else {
            Self->txCursor.endColumn = 0;
            end_column = Self->txCursor.column();
         }

         LONG colchar = 0;
         LONG col = 0;
         LONG i = 0;
         while (((size_t)i < Self->txLines[Self->txCursor.row()].length()) and (colchar < end_column)) {
            col++;
            colchar++;
            for (++i; (Self->txLines[Self->txCursor.row()][i] & 0xc0) == 0x80; i++);
         }

         Self->txFlags &= ~VTXF_AREA_SELECTED;

         LONG new_row;
         if (Event->Code IS K_UP) new_row = Self->txCursor.row() - 1;
         else new_row = Self->txCursor.row() + 1;

         LONG new_column;
         for (new_column=0, i=0; (col > 0) and ((size_t)i < Self->txLines[new_row].length());) {
            col--;
            new_column++;
            for (++i; (Self->txLines[new_row][i] & 0xc0) IS 0x80; i++);
         }

         if (new_column > Self->txCursor.endColumn) Self->txCursor.endColumn = new_column;
         Self->txCursor.savePos = ((LONG)new_row << 16) | new_column;
         Self->txCursor.move(Self, new_row, new_column);
         acDraw(Self);
      }
      break;
   }
}

//****************************************************************************

static void delete_selection(objVectorText *Self)
{
   Self->txFlags &= ~VTXF_AREA_SELECTED;

   LONG row, column, end_row, end_column;
   Self->txCursor.selectedArea(Self, &row, &column, &end_row, &end_column);
   column = Self->txLines[row].utf8CharOffset(column);
   end_column = Self->txLines[end_row].utf8CharOffset(end_column);

   if (row IS end_row) { // Selection limited to one line
      Self->txLines[row].replace(column, end_column - column, "");
      Self->txCursor.move(Self, row, column);
   }
   else {
      Self->txLines[row].resize(column); // Truncate to end of the first line
      Self->txCursor.move(Self, row, column);

      row++;
      if (row < end_row) Self->txLines.erase(Self->txLines.begin() + row, Self->txLines.begin() + end_row);

      if (end_column > 0) Self->txLines[row].replace(0, end_column, "");
   }

   mark_dirty(Self, RC_BASE_PATH);
}

//****************************************************************************
// Note: This function validates boundaries except for the column going beyond the string length.

void TextCursor::move(objVectorText *Vector, LONG Row, LONG Column, bool ValidateWidth)
{
   Vector->txFlags &= ~VTXF_AREA_SELECTED;

   if (Row < 0) Row = 0;
   else if ((size_t)Row >= Vector->txLines.size()) Row = (LONG)Vector->txLines.size() - 1;

   if (Column < 0) Column = 0;
   else if (ValidateWidth) {
      LONG max_col = Vector->txLines[mRow].utf8Length();
      if (Column > max_col) Column = max_col;
   }

   mRow = Row;
   mColumn = Column;

   resetVector(Vector);

   acDraw(Vector);
}

//****************************************************************************

void TextCursor::resetVector(objVectorText *Vector)
{
   if (Vector->txCursor.vector) {
      auto &line = Vector->txLines[mRow];

      if (!line.chars.empty()) {
         auto col = mColumn;
         if ((size_t)col > line.chars.size()) col = line.chars.size();
         Vector->txCursor.vector->Points[0].X = line.chars[col].x1 + 0.5;
         Vector->txCursor.vector->Points[0].Y = line.chars[col].y1;
         Vector->txCursor.vector->Points[1].X = line.chars[col].x2 + 0.5;
         Vector->txCursor.vector->Points[1].Y = line.chars[col].y2;
         reset_path(Vector->txCursor.vector);
      }
   }
}

//****************************************************************************
// If the cursor is out of the current line's boundaries, this function will move it to a safe position.

void TextCursor::validatePosition(objVectorText *Self)
{
   if (Self->txLines[mRow].empty()) {
      if (mColumn != 0) Self->txCursor.move(Self, mRow, 0);
   }
   else {
      LONG max_col = Self->txLines[mRow].utf8Length();
      if (mColumn > max_col) Self->txCursor.move(Self, mRow, max_col);
   }
}

//****************************************************************************

static void insert_char(objVectorText *Self, LONG Unicode, LONG Column)
{
   if ((!Self) or (!Unicode)) return;

   mark_dirty(Self, RC_BASE_PATH);

   char buffer[6];
   LONG charlen = UTF8WriteValue(Unicode, buffer, 6);

   if (Self->txLines.empty()) {
      Self->txLines.emplace_back(std::string(buffer, charlen));
      Self->txCursor.move(Self, 0, 1);
   }
   else if (Self->txLines[Self->txCursor.row()].empty()) {
      if (Self->txCharLimit < 1) return;
      Self->txLines[Self->txCursor.row()].append(buffer, charlen);
      Self->txCursor.move(Self, Self->txCursor.row(), 1);
   }
   else {
      auto offset = Self->txLines[Self->txCursor.row()].utf8CharOffset(Column);

      if ((Self->txFlags & VTXF_OVERWRITE) and ((size_t)offset < Self->txLines[Self->txCursor.row()].length())) {
         Self->txLines[Self->txCursor.row()].replace(offset, Self->txLines[Self->txCursor.row()].charLength(offset), "");
      }

      Self->txLines[Self->txCursor.row()].insert(offset, buffer, charlen);

      // Enforce character limits (delete the character at the end of this line to keep it within limits)

      if (Self->txLines[Self->txCursor.row()].utf8Length() > Self->txCharLimit) {
         Self->txLines[Self->txCursor.row()].resize(Self->txLines[Self->txCursor.row()].lastChar());
      }

      Self->txCursor.move(Self, Self->txCursor.row(), Self->txCursor.column() + 1, true);
   }
}

//****************************************************************************

#include "text_def.cpp"

static const FieldDef clTextFlags[] = {
   { "Underline",   VTXF_UNDERLINE },
   { "Overline",    VTXF_OVERLINE },
   { "LineThrough", VTXF_LINE_THROUGH },
   { "Blink",       VTXF_BLINK },
   { "Editable",    VTXF_EDITABLE },
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
   { "FontStyle",     FDF_VIRTUAL|FDF_STRING|FDF_RI,           0, (APTR)TEXT_GET_FontStyle, (APTR)TEXT_SET_FontStyle },
   { "DX",            FDF_VIRTUAL|FDF_ARRAY|FDF_DOUBLE|FDF_RW, 0, (APTR)TEXT_GET_DX, (APTR)TEXT_SET_DX },
   { "DY",            FDF_VIRTUAL|FDF_ARRAY|FDF_DOUBLE|FDF_RW, 0, (APTR)TEXT_GET_DY, (APTR)TEXT_SET_DY },
   { "InlineSize",    FDF_VIRTUAL|FDF_DOUBLE|FDF_RW,           0, (APTR)TEXT_GET_InlineSize, (APTR)TEXT_SET_InlineSize },
   { "LetterSpacing", FDF_VIRTUAL|FDF_DOUBLE|FDF_RW,           0, (APTR)TEXT_GET_LetterSpacing, (APTR)TEXT_SET_LetterSpacing },
   { "Rotate",        FDF_VIRTUAL|FDF_ARRAY|FDF_DOUBLE|FDF_RW, 0, (APTR)TEXT_GET_Rotate, (APTR)TEXT_SET_Rotate },
   { "ShapeInside",   FDF_VIRTUAL|FDF_OBJECTID|FDF_RW,         ID_VECTOR, (APTR)TEXT_GET_ShapeInside, (APTR)TEXT_SET_ShapeInside },
   { "ShapeSubtract", FDF_VIRTUAL|FDF_OBJECTID|FDF_RW,         ID_VECTOR, (APTR)TEXT_GET_ShapeSubtract, (APTR)TEXT_SET_ShapeSubtract },
   { "TextLength",    FDF_VIRTUAL|FDF_DOUBLE|FDF_RW,           0, (APTR)TEXT_GET_TextLength, (APTR)TEXT_SET_TextLength },
   { "TextFlags",     FDF_VIRTUAL|FDF_LONGFLAGS|FDF_RW,        (MAXINT)&clTextFlags, (APTR)TEXT_GET_Flags, (APTR)TEXT_SET_Flags },
   { "TextWidth",     FDF_VIRTUAL|FDF_LONG|FDF_R,              0, (APTR)TEXT_GET_TextWidth, NULL },
   { "StartOffset",   FDF_VIRTUAL|FDF_DOUBLE|FDF_RW,           0, (APTR)TEXT_GET_StartOffset, (APTR)TEXT_SET_StartOffset },
   { "Spacing",       FDF_VIRTUAL|FDF_DOUBLE|FDF_RW,           0, (APTR)TEXT_GET_Spacing, (APTR)TEXT_SET_Spacing },
   { "Font",          FDF_VIRTUAL|FDF_OBJECT|FDF_R,            0, (APTR)TEXT_GET_Font, NULL },
   // Non-SVG fields related to real-time text editing
   { "Focus",         FDF_VIRTUAL|FDF_OBJECTID|FDF_RI,    0, (APTR)TEXT_GET_Focus, (APTR)TEXT_SET_Focus },
   { "CursorColumn",  FDF_VIRTUAL|FDF_LONG|FDF_RW,        0, (APTR)TEXT_GET_CursorColumn, (APTR)TEXT_SET_CursorColumn },
   { "CursorRow",     FDF_VIRTUAL|FDF_LONG|FDF_RW,        0, (APTR)TEXT_GET_CursorRow, (APTR)TEXT_SET_CursorRow },
   { "TotalLines",    FDF_VIRTUAL|FDF_LONG|FDF_R,         0, (APTR)TEXT_GET_TotalLines, NULL },
   { "SelectRow",     FDF_VIRTUAL|FDF_LONG|FDF_R,         0, (APTR)TEXT_GET_SelectRow, NULL },
   { "SelectColumn",  FDF_VIRTUAL|FDF_LONG|FDF_R,         0, (APTR)TEXT_GET_SelectColumn, NULL },
   { "LineLimit",     FDF_VIRTUAL|FDF_LONG|FDF_RW,        0, (APTR)TEXT_GET_LineLimit, (APTR)TEXT_SET_LineLimit },
   { "CharLimit",     FDF_VIRTUAL|FDF_LONG|FDF_RW,        0, (APTR)TEXT_GET_CharLimit, (APTR)TEXT_SET_CharLimit },
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
      FID_Actions|TPTR,      clVectorTextActions,
      FID_Methods|TARRAY,    clVectorTextMethods,
      FID_Fields|TARRAY,     clTextFields,
      FID_Size|TLONG,        sizeof(objVectorText),
      FID_Path|TSTR,         MOD_PATH,
      TAGEND));
}
