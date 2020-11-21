
static ERROR GET_TextHeight(objText *Self, LONG *Value);

/*****************************************************************************

-FIELD-
Activated: Callback function for validating user input.

The Activated callback informs the client that the user wishes to activate the text widget, having pressed the enter
key or its functional equivalent.  The function prototype is `Function(*Text)`.

If the callback rejects the current #String, it is the client's choice as to how the user is informed.  This
could involve resetting the string to its former value; displaying a passive warning; or using a dialog box.
Where possible, it is recommended that passive warnings are displayed and more intrusive errors are only imposed after
submission of the content.

*****************************************************************************/

static ERROR GET_Activated(objText *Self, FUNCTION **Value)
{
   if (Self->Activated.Type != CALL_NONE) {
      *Value = &Self->Activated;
      return ERR_Okay;
   }
   else return ERR_FieldNotSet;
}

static ERROR SET_Activated(objText *Self, FUNCTION *Value)
{
   if (Value) {
      if (Self->Activated.Type IS CALL_SCRIPT) UnsubscribeAction(Self->Activated.Script.Script, AC_Free);
      Self->Activated = *Value;
      if (Self->Activated.Type IS CALL_SCRIPT) SubscribeAction(Self->Activated.Script.Script, AC_Free);
   }
   else Self->Activated.Type = CALL_NONE;
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
AmtLines: The total number of lines stored in the object.

-FIELD-
Background: Optional background colour for text.  Set to NULL for no background.

-FIELD-
CharLimit: Limits the amount of characters allowed in a text object's string.

Set the CharLimit field if you wish to limit the amount of characters that can appear in a text object's string.  The
minimum possible value is 0 for no characters.

The CharLimit field is most useful for restricting the amount of characters that a user can enter in an editable text
object.

*****************************************************************************/

static ERROR SET_CharLimit(objText *Self, LONG Value)
{
   if (Value < 0) return ERR_OutOfRange;

   Self->CharLimit = Value;

   // Check that the existing string is within the character limit


   return ERR_Okay;
}

/****************************************************************************

-FIELD-
CursorColour: The colour used for the text cursor.

-FIELD-
CursorColumn: The current column position of the cursor.

****************************************************************************/

static ERROR SET_CursorColumn(objText *Self, LONG Value)
{
   if (Value >= 0) {
      Self->CursorColumn = Value;
      Redraw(Self);
      return ERR_Okay;
   }

   return ERR_Failed;
}

/****************************************************************************

-FIELD-
CursorRow: The current line position of the cursor.

****************************************************************************/

static ERROR SET_CursorRow(objText *Self, LONG Value)
{
   if (Value >= 0) {
      if (Value < Self->AmtLines) Self->CursorRow = Value;
      else Self->CursorRow = Self->AmtLines - 1;
      Redraw(Self);
      return ERR_Okay;
   }
   else return ERR_Failed;
}

/*****************************************************************************

-FIELD-
Flags: Special flags that affect object behaviour.

-FIELD-
Focus: Refers to the object that will be monitored for user focussing.

By default, a text object will become active (i.e. capable of receiving keyboard input) when its surface container
receives the focus.  If you would like to change this so that a Text becomes active when some other object receives the
focus, refer to that object by writing its ID to this field.

-FIELD-
Font: Points to a @Font object that controls the drawing of text.

To set the face, colour and other attributes of a text object's graphics, you need to read the Font field and write
your settings to the font object prior to initialisation.  For a list of all the fields that can be set, please refer
to the documentation for the @Font class.

-FIELD-
Frame: Forces a text object's graphic to be drawn to a specific frame.

If this field is set to a valid frame number, the text graphic will only be drawn when the frame of the container
matches the Frame number in this field.  When set to 0 (the default), the text graphic will be drawn regardless of the
container's frame number.

-FIELD-
Height: Private. Mirrors the Layout Height.

*****************************************************************************/

static ERROR GET_Height(objText *Self, struct Variable *Value)
{
   if (!(Self->Layout->Dimensions & DMF_VERTICAL_FLAGS)) {
      LONG lval;
      GET_TextHeight(Self, &lval);

      if (Value->Type & FD_DOUBLE) Value->Double = lval;
      else if (Value->Type & FD_LARGE) Value->Large = lval;
      else return PostError(ERR_FieldTypeMismatch);
      return ERR_Okay;
   }
   else return GetField(Self->Layout, FID_Height|TVAR, Value);
}

static ERROR SET_Height(objText *Self, struct Variable *Value)
{
   return SetVariable(Self->Layout, FID_Height, Value);
}

/*****************************************************************************

-FIELD-
Highlight: Defines the colour used to highlight text.

-FIELD-
HistorySize: Defines the maximum number of records stored in the history buffer.

If the history buffer is enabled, the HistorySize will indicate the maximum number of string records that can be stored
in the text object for retrieval by the user.

The history buffer is enabled with the HISTORY option in the #Flags field.

-FIELD-
HScroll: If scrolling is required, use this field to refer to a horizontal scroll bar.

To attach a horizontal scrollbar to a text object, set the HScroll field to an object belonging to the @Scroll
class.  If the Scroll object is configured to provide full scrollbar functionality, the user will be able to scroll the
text display along the horizontal axis.

*****************************************************************************/

static ERROR SET_HScroll(objText *Self, OBJECTID Value)
{
   OBJECTID objectid = Value;

   if (GetClassID(objectid) IS ID_SCROLLBAR) {
      OBJECTPTR object;
      if (!AccessObject(objectid, 3000, &object)) {
         GetLong(object, FID_Scroll, &objectid);
         ReleaseObject(object);
      }
   }

   if (GetClassID(objectid) IS ID_SCROLL) {
      OBJECTPTR object;
      if (!AccessObject(objectid, 3000, &object)) {
         SetLong(object, FID_Object, Self->Head.UniqueID);
         Self->HScrollID = objectid;
         Self->XPosition = 0;
         if (Self->Head.Flags & NF_INITIALISED) calc_hscroll(Self);
         ReleaseObject(object);
         return ERR_Okay;
      }
      else return PostError(ERR_AccessObject);
   }
   else {
      LogErrorMsg("Attempt to set the HScroll field with an invalid object.");
      return ERR_Failed;
   }
}

/*****************************************************************************

-FIELD-
LayoutStyle: Private.  Internal field for supporting dynamic style changes when a GUI object is used in a document.

*****************************************************************************/

static ERROR SET_LayoutStyle(objText *Self, DOCSTYLE *Value)
{
   if (!Value) return ERR_Okay;

   if (Self->Head.Flags & NF_INITIALISED) docApplyFontStyle(Value->Document, Value, Self->Font);
   else docApplyFontStyle(Value->Document, Value, Self->Font);
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
LineLimit: Restricts the total number of lines allowed in a text object.

Set the LineLimit field to restrict the maximum number of lines permitted in a text object.  It is common to set this
field to a value of 1 for input boxes that have a limited amount of space available.

-FIELD-
Location: Identifies the location of a text file to load.

To load a text file into a text object, set the Location field.  If this field is set after initialisation, the object
will automatically clear its content and reload data from the location that you specify.

Viable alternatives to setting the Location involve loading the data manually and then setting the String field with a
data pointer, or using the DataFeed action.

*****************************************************************************/

static ERROR GET_Location(objText *Self, STRING *Value)
{
   if (Self->Location) {
      *Value = Self->Location;
      return ERR_Okay;
   }
   else {
      *Value = NULL;
      return ERR_FieldNotSet;
   }
}

static ERROR SET_Location(objText *Self, CSTRING Value)
{
   if (Self->Location) { FreeResource(Self->Location); Self->Location = NULL; }
   if ((Value) AND (*Value)) {
      if (!(Self->Location = StrClone(Value))) return PostError(ERR_AllocMemory);
      if (Self->Head.Flags & NF_INITIALISED) {
         load_file(Self, Self->Location);
      }
   }

   return ERR_Okay;
}

/****************************************************************************

-FIELD-
Origin: Similar to the Location field, but does not automatically load content if set.

This field is identical to the Location field, with the exception that it does not update the content of a text object
if it is set after initialisation.  This may be useful if the origin of the text data needs to be changed without
causing a load operation.

****************************************************************************/

static ERROR SET_Origin(objText *Self, CSTRING Value)
{
   if (Self->Location) { FreeResource(Self->Location); Self->Location = NULL; }

   if ((Value) AND (*Value)) {
      if (!(Self->Location = StrClone(Value))) return PostError(ERR_AllocMemory);
   }

   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Point: Private.  This is a proxy for the Font Point field, because changing the point size requires recalculating the line widths.

*****************************************************************************/

static ERROR GET_Point(objText *Self, struct Variable *Value)
{
   if (Value->Type & FD_DOUBLE) Value->Double = Self->Font->Point;
   else Value->Large  = F2T(Self->Font->Point);
   return ERR_Okay;
}

static ERROR SET_Point(objText *Self, struct Variable *Value)
{
   SetField(Self->Font, FID_Point|TVAR, Value);

   // String widths need to be recalculated.

   if (Self->Font->Head.Flags & NF_INITIALISED) {
      LONG i;
      for (i=0; i < Self->AmtLines; i++) {
         Self->Array[i].PixelLength = calc_width(Self, Self->Array[i].String, Self->Array[i].Length);
      }
   }
   return ERR_Okay;
}

/****************************************************************************

-FIELD-
SelectColumn: Indicates the column position of a selection's beginning.

If the user has selected an area of text, the starting column of that area will be indicated by this field.  If an area
has not been selected, the value of the SelectColumn field is undefined.

To check whether or not an area has been selected, test the AREASELECTED bit in the #Flags field.

-FIELD-
SelectRow: Indicates the line position of a selection's beginning.

If the user has selected an area of text, the starting row of that area will be indicated by this field.  If an area
has not been selected, the value of the SelectRow field is undefined.

To check whether or not an area has been selected, test the AREASELECTED bit in the #Flags field.

-FIELD-
String: Text information can be written directly to a text object through this field.

To write a string to a text object, set this field.  Updating a text object in this fashion causes it to analyse the
string information for return codes, which means the string data can be split into lines.  Any data that is in the text
object when you set this field will be deleted automatically.  The graphics will also be redrawn and any attached
Scroll objects will be recalculated.

*****************************************************************************/

static ERROR GET_String(objText *Self, STRING *Value)
{

   *Value = NULL;

   if (Self->AmtLines IS 1) {
      *Value = Self->Array[0].String;
      return ERR_Okay;
   }
   else if (Self->AmtLines > 1) {
      if (Self->StringBuffer) { FreeResource(Self->StringBuffer); Self->StringBuffer = NULL; }

      LONG size = 1, i;
      for (i=0; i < Self->AmtLines; i++) {
         size += Self->Array[i].Length + 1;
      }

      if (!AllocMemory(size, MEM_STRING, &Self->StringBuffer, NULL)) {
         STRING str = Self->StringBuffer;
         for (i=0; i < Self->AmtLines; i++) {
            str += StrCopy(Self->Array[i].String, str, Self->Array[i].Length);
            if (i < Self->AmtLines + 1) *str++ = '\n';
         }
         *str = 0;

         *Value = Self->StringBuffer;
         return ERR_Okay;
      }
      else return ERR_AllocMemory;
   }
   return ERR_NoData;
}

static ERROR SET_String(objText *Self, CSTRING String)
{
   Self->NoUpdate++; // Turn off graphical updates

   acClear(Self);

   if (Self->Flags & TXF_STR_TRANSLATE) String = StrTranslateText(String);

   if (String) { // Add the string information
      CSTRING line = String;
      while ((line) AND (*line)) {
         LONG len = StrLineLength(line);
         txtAddLine(Self, -1, line, len);
         line += len;
         if (*line) {
            line++; // Skip the return code
            if (!*line) txtAddLine(Self, -1, "", 0); // There was a return code at the end of the line
         }
      }
   }

   if ((Self->Head.Flags & NF_INITIALISED) AND (Self->Flags & TXF_STRETCH)) {
      stretch_text(Self);
   }

   // Update the entire text area

   Self->NoUpdate--;

   if (Self->Head.Flags & NF_INITIALISED) {
      calc_hscroll(Self);
      calc_vscroll(Self);
      Redraw(Self);
   }

   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
TabFocus: Allows the user to hit the tab key to focus on other GUI objects.

If this field points to another GUI object, the user will be able to use the tab key to move to that object when
entering information into the text object. (Technically this causes the focus to be set to that object, and the text
object will thus lose the focus.)

When a series of objects are 'chained' via tab focussing, the user will have an easier time moving between objects
through use of the keyboard.

-FIELD-
TextHeight: Indicator for the pixel height of all lines in a text object.

The total height of all lines in a text object can be measured by reading the TextHeight.  The returned value
compensates for vertical and gutter spacing.  Wordwrap will be taken into account if the WORDWRAP bit has been set in
the Flags field.

*****************************************************************************/

static ERROR GET_TextHeight(objText *Self, LONG *Value)
{
   LONG lines, pagewidth, row, count;

   if ((Self->Flags & TXF_WORDWRAP) AND (Self->AmtLines > 0) AND (Self->Layout->ParentSurface.Width > 0)) {
      pagewidth = Self->Layout->BoundWidth - Self->Layout->LeftMargin - Self->Layout->RightMargin;
      if (Self->AmtLines < 20) {
         // This routine is slow, but gives a precise indication of the height

         lines = 0;
         for (row=0; row < Self->AmtLines; row++) {
            if (Self->Array[row].PixelLength >= pagewidth) {
               fntStringSize(Self->Font, Self->Array[row].String, -1, pagewidth, NULL, &count);
               lines += count;
            }
            else lines++;
         }
      }
      else {
         // This routine is fast, but gives only a rough indication of the height

         lines = 0;
         for (row=0; row < Self->AmtLines; row++) {
            if (Self->Array[row].PixelLength >= pagewidth) {
               lines += ((Self->Array[row].PixelLength + pagewidth) / pagewidth);
            }
            else lines++;
         }
      }
   }
   else lines = Self->AmtLines;

   // In edit mode, there is always at least 1 active line (so that you can enter text)

   if ((lines < 1) AND (Self->Flags & TXF_EDIT)) lines = 1;

   //*Value = (Self->Font->MaxHeight + (Self->Font->LineSpacing * (lines-1)));
   *Value = Self->Font->LineSpacing * lines;

   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
TextWidth: Measures the pixel width of the text string.

The width of the longest text line can be retrieved from this field. The result includes the LeftMargin and RightMargin
field values if they have been defined.

*****************************************************************************/

static ERROR GET_TextWidth(objText *Self, LONG *Value)
{
   LONG row, width;

   LONG longest = 0;
   if ((Self->Flags & TXF_WORDWRAP) AND (Self->Layout->ParentSurface.Width > 0)) {
      LONG pagewidth = Self->Layout->BoundWidth - Self->Layout->LeftMargin - Self->Layout->RightMargin;

      if (Self->AmtLines < 50) {
         // Calculate an accurate value for the text width by asking the font object to return the pixel width of each
         // line when word-wrapping is taken into consideration.

         for (row=0; row < Self->AmtLines; row++) {
            if (Self->Array[row].String) {
               fntStringSize(Self->Font, Self->Array[row].String, -1, Self->Layout->BoundX + Self->Layout->BoundWidth - Self->Layout->RightMargin, &width, NULL);
               if (width > longest) longest = width;
            }
         }
      }
      else {
         // Calculate an approximate value for the text width
         for (row=0; row < Self->AmtLines; row++) {
            if (Self->Array[row].PixelLength > pagewidth) {
               longest = pagewidth;
            }
            else if (Self->Array[row].PixelLength > longest) {
               longest = Self->Array[row].PixelLength;
            }
         }
      }
   }
   else {
      for (row=0; row < Self->AmtLines; row++) {
         if (Self->Array[row].PixelLength > longest) {
            longest = Self->Array[row].PixelLength;
         }
      }
   }

   *Value = longest + Self->Layout->LeftMargin + Self->Layout->RightMargin;
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
TextX: The horizontal position for all text strings.

*****************************************************************************/

static ERROR GET_TextX(objText *Self, LONG *Value)
{
   *Value = Self->Layout->LeftMargin;
   return ERR_Okay;
}

static ERROR SET_TextX(objText *Self, LONG Value)
{
   Self->Layout->LeftMargin = Value;
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
TextY: The vertical position of the first text string.

*****************************************************************************/

static ERROR GET_TextY(objText *Self, LONG *Value)
{
   *Value = Self->Layout->TopMargin;
   return ERR_Okay;
}

static ERROR SET_TextY(objText *Self, LONG Value)
{
   Self->Layout->TopMargin = Value;
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
ValidateInput: Callback function for validating user input.

The ValidateInput callback allows the client to check that the current text string is valid.  It is called when the
#Activate() action is used, which will typically occur when the enter key being pressed, or the text object
loses the focus.

The function prototype is `Function(*Text)`.

If the callback rejects the current #String, it is the client's choice as to how the user is informed.  This
could involve resetting the string to its former value; displaying a passive warning; or using a dialog box.
Where possible, it is recommended that passive warnings are displayed and more intrusive errors are only imposed after
submission of the content.

*****************************************************************************/

static ERROR GET_ValidateInput(objText *Self, FUNCTION **Value)
{
   if (Self->ValidateInput.Type != CALL_NONE) {
      *Value = &Self->ValidateInput;
      return ERR_Okay;
   }
   else return ERR_FieldNotSet;
}

static ERROR SET_ValidateInput(objText *Self, FUNCTION *Value)
{
   if (Value) {
      if (Self->ValidateInput.Type IS CALL_SCRIPT) UnsubscribeAction(Self->ValidateInput.Script.Script, AC_Free);
      Self->ValidateInput = *Value;
      if (Self->ValidateInput.Type IS CALL_SCRIPT) SubscribeAction(Self->ValidateInput.Script.Script, AC_Free);
   }
   else Self->ValidateInput.Type = CALL_NONE;
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
VScroll: If scrolling is required, use this field to refer to a vertical scroll bar.

To attach a vertical scrollbar to a text object, set the VScroll field to an object belonging to the @Scroll class.  If
the Scroll object is configured to provide full scrollbar functionality, the user will be able to scroll the text
display along the vertical axis.

*****************************************************************************/

static ERROR SET_VScroll(objText *Self, OBJECTID Value)
{
   OBJECTID objectid = Value;

   if (GetClassID(objectid) IS ID_SCROLLBAR) {
      OBJECTPTR object;
      if (!AccessObject(objectid, 3000, &object)) {
         GetLong(object, FID_Scroll, &objectid);
         ReleaseObject(object);
      }
   }

   if (GetClassID(objectid) IS ID_SCROLL) {
      OBJECTPTR object;
      if (!AccessObject(objectid, 3000, &object)) {
         SetLong(object, FID_Object, Self->Head.UniqueID);
         Self->VScrollID = objectid;
         Self->YPosition = 0;
         if (Self->Head.Flags & NF_INITIALISED) calc_vscroll(Self);
         ReleaseObject(object);
         return ERR_Okay;
      }
      else return PostError(ERR_AccessObject);
   }
   else {
      LogErrorMsg("Attempt to set the VScroll field with an invalid object.");
      return ERR_Failed;
   }
}

/*****************************************************************************

-FIELD-
Width: Private. Mirrors the Layout Width.

*****************************************************************************/

static ERROR GET_Width(objText *Self, struct Variable *Value)
{
   if (!(Self->Layout->Dimensions & DMF_HORIZONTAL_FLAGS)) {
      LONG width;
      GET_TextWidth(Self, &width);
      if (Value->Type & FD_DOUBLE) Value->Double = width;
      else if (Value->Type & FD_LARGE) Value->Large = width;
      else return PostError(ERR_FieldTypeMismatch);
      return ERR_Okay;
   }
   else return GetField(Self->Layout, FID_Width|TVAR, Value);
}

static ERROR SET_Width(objText *Self, struct Variable *Value)
{
   return SetVariable(Self->Layout, FID_Width, Value);
}
