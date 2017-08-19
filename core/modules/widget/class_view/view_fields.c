/*****************************************************************************

-FIELD-
ActiveTag: Indicates the tag index of the last item to have its state altered.

The ActiveTag field provides a method for retrieving the tag index of the last item to have its state altered.  A view
item is deemed to have had its state altered when it has been selected or de-selected.  This field can return a value
of -1 if no items have had their state altered, or if the view has been cleared.  The index can be used to lookup the
tag structure in the view's XML Tags list.

-FIELD-
BorderOffset: Sets the X, Y, XOffset and YOffset to a single value.

This field is provided for the convenience of setting the X, Y, XOffset and YOffset fields in one hit.  The value that
is specified will be written to all of the aforementioned fields as a fixed pixel-width value.

*****************************************************************************/

static ERROR SET_BorderOffset(objView *Self, LONG Value)
{
   SetFields(Self, FID_X|TLONG,  Value,
                   FID_Y|TLONG,  Value,
                   FID_XOffset|TLONG, Value,
                   FID_YOffset|TLONG, Value,
                   TAGEND);
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
ButtonBackground: The background colour to use for buttons.

This field defines the background colour to use for buttons within the view (such as for column headers).  Blending is
supported if the alpha component is used.  This field is complemented by the #ButtonHighlight,
#ButtonShadow and #ButtonThickness fields.

-FIELD-
ButtonHighlight: Defines the highlight colour of buttons.

When drawing buttons within the view (such as for column headers), the ButtonHighlight will define the colour at the
top and left edges of the border, with a pixel width of #ButtonThickness.  If highlighting is not desired, set
the alpha component of this colour to zero.

-FIELD-
ButtonShadow: Defines the shadow colour of buttons.

When drawing buttons within the view (such as for column headers), the ButtonShadow will define the colour at the
right and bottom edges of the border, with a pixel width of #ButtonThickness.  If a shadow is not desired, set
the alpha component of this colour to zero.

-FIELD-
ButtonThickness: Defines the pixel thickness of button borders.

The pixel thickness of button borders is defined here, in conjunction with the colours #ButtonHighlight and
#ButtonShadow.  If a border is not desired, this field must be set to zero.

-FIELD-
CellClick: A callback for receiving notifications of clicks in column cells.

This callback can be used when a View is using the COLUMN or COLUMNTREE style modes.  It allows the client to receive
a notification when the user clicks on a column cell.  The synopsis for the callback function is `ERROR Callback(objView *View, LONG TagIndex, LONG Column, LONG Button, LONG X, LONG Y)`

The clicked cell is converted to the relevant tag and referenced in the TagIndex parameter.  The Column is the index
of the column that was clicked; Button is the type associated with the click (e.g. JET_LMB); X and Y are the position
of the click relative to the cell's top-left position.

The tag and any associated children can be modified by the callback function.  On return, the view will check if the
tag was modified and may refresh the item with your changes.  Do not modify the XML object or any other tags, as this
is likely to result in unexpected errors.

If the clicked cell is a checkbox, the relevant tag value will be flipped between 0 and 1 to change the state of
the checkbox.  Be aware that this process occurs after the CellClick function has been called and not before.
If the tag is altered however, the value flip does not occur at all (it is assumed that the client is controlling the
tag values manually).

*****************************************************************************/

static ERROR GET_CellClick(objView *Self, FUNCTION **Value)
{
   if (Self->CellClick.Type != CALL_NONE) {
      *Value = &Self->CellClick;
      return ERR_Okay;
   }
   else return ERR_FieldNotSet;
}

static ERROR SET_CellClick(objView *Self, FUNCTION *Value)
{
   if (Value) {
      if (Self->CellClick.Type IS CALL_SCRIPT) UnsubscribeAction(Self->CellClick.Script.Script, AC_Free);
      Self->CellClick = *Value;
      if (Self->CellClick.Type IS CALL_SCRIPT) SubscribeAction(Self->CellClick.Script.Script, AC_Free);
   }
   else Self->CellClick.Type = CALL_NONE;
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
ColAltBackground: Enables alternative colour switching with #ColBackground.

This field is used when the #Style is either COLUMN, COLUMNTREE, LIST or LONG_LIST.

When a colour is defined in this field, alternative colour switching will be enabled in conjunction with
#ColBackground.  For every uneven item (line) number, ColAltBackground will be used instead of
#ColBackground for that item.

-FIELD-
ColBackground: Defines the colour of the background.  Alpha blending is supported.

-FIELD-
ColBkgdHighlight: Defines the background colour to use when an item is highlighted.

If defined, this field changes the background colour of items that are marked as highlighted.  If an item is also
marked as selected, then the final colour is interpolated with #ColSelect.

-FIELD-
ColBorder: If defined, this colour is drawn around the edge of the view.

This colour is not set by default.  If it is defined, the colour will be drawn around the edge of the view, as a
rectangle of 1 pixel width in size.

-FIELD-
ColBranch: Defines the colour of tree branch lines when the tree style is enabled.

-FIELD-
ColButtonFont: This colour is used for fonts drawn in column buttons in the top row.

-FIELD-
ColGroupShade: Applicable to the GROUP_TREE #Style only, draw this colour in the unused area of the tree.

When using the GROUP_TREE #Style, there will often be an area at the bottom of the tree that contains no items.
If a colour is defined in ColGroupShade, then the empty area will be filled with this colour.  This makes it clear
to the user that the area is unused.

-FIELD-
ColHairline: This colour will be used when drawing table and column hairlines.

This colour is used when drawing table and column hairlines.  It is strongly recommended that a light colour is used
that is barely visible against the background.

-FIELD-
ColHighlight: Defines the colour when an item is selected via mouse-over or user focus.

The ColHighlight field defines the colour that is used when the user rolls the mouse pointer over an item, or uses the
cursor keys to change the item focus.  The colour must be in hexadecimal or CSV format - for example to create a pure
red colour, a setting of "#ff0000" or "255,0,0" would be valid.

-FIELD-
ColItem: The default font colour to use for view items.

The ColItem field defines the font colour that is applied to new items.  The default is black.  Items may override the
default by using the colour attribute in their XML definition.

The colour string must be in hexadecimal or CSV format - for example to create a pure red colour, a setting of "#ff0000"
or "255,0,0" would be valid.

-FIELD-
ColSelect: Defines the background colour for selected items.

-FIELD-
ColSelectFont: The colour to use for the font in a selected item.

-FIELD-
ColSelectHairline: The colour to use for hairlines when an item is selected.

The ColSelectHairline field defines the colour that is used for hairlines when an item is selected.  The colour must be
in hexadecimal or CSV format - for example to create a pure red colour, a setting of "#ff0000" or "255,0,0" would be
valid.

-FIELD-
ColTitleFont: This colour value will apply to title bar fonts when the view is using the group-tree #Style.

-FIELD-
Columns: Active columns for 'column mode' may be set via this field.

When a view object is in column or tree mode (as defined in the #Style field), you must specify the XML tags
that should be represented as columns.  Failure to register any column names means that only the default column will
be displayed.  Consider the following XML structure:

<pre>
&lt;file&gt;
  readme.txt
  &lt;size&gt;19431&lt;/size&gt;
  &lt;date&gt;20030503 11:53:19&lt;/date&gt;
&lt;/file&gt;
</pre>

The size and date tags will need to be registered as columns in order for the view object to display them.  To show
them in the order specified, we can pass the string "size(text:'File Size', width:140, type:bytesize); date(text:Date,
width:180, type:date)" to the Columns field. The optional information that we've specified in the brackets tells the
view object the column name, the default pixel width of the column and the display type.

Currently recognised data types are as follows:

<types>
<type name="bytesize">The value is a number that can be represented in bytes, kilobytes, megabytes or gigabytes.</>
<type name="numeric">Can be used for any type of number (integer or floating point).</>
<type name="checkbox">The value must be expressed as '1' or 'Y' to draw a checkmark (on).  Any other value will draw a faded checkmark (off).  Note that if the relevant tag is not present in the item, it is not considered markable and therefore no checkmark indicator will be drawn.</>
<type name="colour">Assign a colour to the column.  The colour can be specified in CSV 'red,green,blue' or hex '#RRGGBB' format.</>
<type name="date">The date type can be used if the values are stored in the date format "YYYYMMDD HH:MM:SS".</>
<type name="rightalign">Align all text to the right of the column.</>
<type name="seconds">The value is given in seconds.  The result will be formatted depending on the size of the value - anything over 60 seconds will be formatted as digital time, split into minutes or hours as necessary.</>
<type name="showicons">Icons are to be shown in this column, to the left of each item name.  The 'icon' attribute will need to be declared in each tag assigned to this column.</>
<type name="variant">The default type - the value is treated as a string.</>
</>

If you would like to change the settings of the default column, e.g. to move it to a different position, you may refer
to it using a column name of 'default'.

By default, the width of a column will never be adjusted if it already exists in the view when you set the Columns
field.  You can override this to ensure that the width is reset by adding the 'reset' parameter to the column(s) in
question.  Alternatively, use the WIDTH_RESET flag to always reset column sizes.
-END-

*****************************************************************************/

static ERROR SET_Columns(objView *Self, CSTRING Value)
{
   struct view_col col, *newcol, *column, *prevcol, *scan;
   CSTRING str;
   CSTRING translate;
   UBYTE arg[20], buffer[120];
   LONG i, index, colindex;
   BYTE reset;

   LogBranch("%s", Value);

   // Mark all existing columns for deletion

   for (column=Self->Columns; column; column=column->Next) column->Flags |= CF_DELETE;

   index = 0;
   if (!(str = Value)) str = "";
   while (*str) {
      ClearMemory(&col, sizeof(col));

      while ((*str) AND (*str <= 0x20)) str++; // Skip whitespace

      // Extract the name

      for (i=0; (*str) AND (*str != '(') AND (*str != ';') AND (i < sizeof(col.Name)-1); i++) {
         col.Name[i] = *str;
         str++;
      }
      col.Name[i]  = 0;

      if (i < 1) break;

      reset = (Self->Flags & VWF_WIDTH_RESET) ? TRUE : FALSE;

      if (*str IS '(') {
         str++;

         while ((*str) AND (*str != ')')) {
            while ((*str) AND (*str <= 0x20)) str++; // Skip whitespace

            for (i=0; (*str) AND (i < sizeof(arg)-1) AND (*str != ':') AND (*str != ',') AND (*str != ')'); i++) arg[i] = *str++;
            arg[i] = 0;

            buffer[0] = 0;
            if (*str IS ':') {
               str++;

               if (*str IS '"') {
                  str++;
                  for (i=0; (*str) AND (*str != '"') AND (i < sizeof(buffer)-1); i++) {
                     buffer[i] = *str;
                     str++;
                  }
                  buffer[i] = 0;

                  if (*str IS '"') str++;
               }
               else if (*str IS '\'') {
                  str++;
                  for (i=0; (*str) AND (*str != '\'') AND (i < sizeof(buffer)-1); i++) {
                     buffer[i] = *str;
                     str++;
                  }
                  buffer[i] = 0;

                  if (*str IS '\'') str++;
               }
               else {
                  for (i=0; (*str) AND (*str != ')') AND (*str != ',') AND (i < sizeof(buffer)-1); i++) {
                     buffer[i] = *str;
                     str++;
                  }
                  buffer[i] = 0;
               }
            }

            while ((*str) AND (*str != ',') AND (*str != ';') AND (*str != ')')) str++;
            if ((*str IS ',') OR (*str IS ';')) str++;

            if (arg[0]) {
               if (!StrMatch("text", arg)) {
                  // Translate the column name
                  if ((translate = StrTranslateText(buffer)) AND ((APTR)translate != (APTR)buffer)) {
                     StrCopy(translate, col.Text, sizeof(col.Text));
                  }
                  else StrCopy(buffer, col.Text, sizeof(col.Text));
               }
               else if ((!StrMatch("len", arg)) OR (!StrMatch("width", arg))) {
                  col.Width = StrToInt(buffer);
               }
               else if (!StrMatch("reset", arg)) {
                  reset = TRUE;
               }
               else if (!StrMatch("type", arg)) {
                  if (!StrMatch("numeric", buffer))       col.Type = CT_NUMERIC;
                  else if (!StrMatch("number", buffer))   col.Type = CT_NUMERIC;
                  else if (!StrMatch("integer", buffer))  col.Type = CT_NUMERIC;
                  else if (!StrMatch("date", buffer))     col.Type = CT_DATE;
                  else if (!StrMatch("bytesize", buffer)) col.Type = CT_BYTESIZE;
                  else if (!StrMatch("seconds", buffer))  col.Type = CT_SECONDS;
                  else if (!StrMatch("checkbox", buffer)) col.Type = CT_CHECKBOX;
               }
               else if ((!StrMatch("colour", arg)) OR (!StrMatch("col", arg))) {
                  col.Flags |= CF_COLOUR;
                  StrToColour(buffer, &col.Colour);
               }
               else if (!StrMatch("showicons", arg)) {
                  col.Flags |= CF_SHOWICONS;
               }
               else if (!StrMatch("rightalign", arg)) {
                  col.Flags |= CF_RIGHTALIGN;
               }
               else LogErrorMsg("Unsupported column argument '%s'", arg);
            }
         }
      }

      // Scan up to the next entry

      while ((*str) AND (*str <= 0x20)) str++; // Skip whitespace
      while ((*str) AND (*str != ';')) str++; // Look for a semi-colon
      if (*str IS ';') str++;
      while ((*str) AND (*str <= 0x20)) str++; // Skip whitespace

      if (!col.Text[0]) StrCopy(col.Name, col.Text, sizeof(col.Text));
      if (!col.Width) col.Width = 100;

      if (col.Width < MIN_COLWIDTH) col.Width = MIN_COLWIDTH;

      // Check if the column already exists in our array.  If it does, update its details.  Otherwise we will create a new column and add it to the column table.

      prevcol = NULL;
      for (column=Self->Columns, colindex=0; column; column=column->Next, colindex++) {
         if (!StrMatch(column->Name, col.Name)) {
            MSG("Updating column '%s'", col.Name);

            StrCopy(col.Text, column->Text, sizeof(column->Text));
            if ((reset) OR (!(Self->Head.Flags & NF_INITIALISED))) {
               column->Width = col.Width;  // Allow the user to retain his defined width unless the reset option has been forced
            }
            if (col.Type)  column->Type  = col.Type;
            if (col.Sort)  column->Sort  = col.Sort;
            if (col.Flags) column->Flags = col.Flags;
            if (col.Flags & CF_COLOUR) column->Colour = col.Colour;
            column->Flags &= ~CF_DELETE;

            if (colindex != index) {
               // The order of the columns needs to be altered.  First of all, we will patch the list as if the column is no longer available.

               if (prevcol) prevcol->Next = column->Next;
               else Self->Columns = column->Next;

               // Now re-insert the column at the required position

               prevcol = NULL;
               for (colindex=0, scan=Self->Columns; (scan) AND (colindex < index); scan=scan->Next, colindex++) prevcol = scan;
               column->Next = scan;
               if (prevcol) prevcol->Next = column;
               else Self->Columns = column;
            }

            break;
         }
         prevcol = column;
      }

      // Add the column if an existing match wasn't found

      if (!column) {
         MSG("Adding new column '%s', type %d", col.Name, col.Type);
         if (AllocMemory(sizeof(col), MEM_DATA|MEM_NO_CLEAR, &newcol, NULL) IS ERR_Okay) {
            CopyMemory(&col, newcol, sizeof(col));
            if (!Self->Columns) Self->Columns = newcol;
            else {
               for (column=Self->Columns; column->Next; column=column->Next);
               column->Next = newcol;
            }
         }
      }

      index++;
   }

   // Remove columns marked for deletion

   scan = NULL;
   prevcol = NULL;
   column = Self->Columns;
   while (column) {
      scan = column;
      column = column->Next;
      if (scan->Flags & CF_DELETE) {
         MSG("Deleting column '%s'", scan->Name);
         if (scan IS Self->Columns) Self->Columns = scan->Next;
         else if (prevcol) prevcol->Next = scan->Next;
         FreeMemory(scan);
      }
      else prevcol = scan;
   }

   // Clear sort settings

   ClearMemory(Self->Sort, sizeof(Self->Sort));

   // Save column header settings

   if (Self->ColumnString) FreeMemory(Self->ColumnString);
   Self->ColumnString = StrClone(Value);

   // Redraw the view if we are in column mode

   if ((Self->Style IS VIEW_COLUMN) OR (Self->Style IS VIEW_COLUMN_TREE)) {
      arrange_items(Self);
      acDrawID(Self->Layout->SurfaceID);
   }

   LogBack();
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
ContextMenu: Reference to a menu that will be displayed for contextual actions.

This field can be set to a @Menu or @Surface that will be automatically displayed when the user
clicks the alternative mouse button while over the view.  The menu will be positioned to appear immediately under the
clicked coordinates.

-FIELD-
DateFormat: Sets the format to use when displaying date types.

Use the DateFormat field to define how dates should be displayed when they are used in columns.  For information on
how to set valid date formats, refer to the StrFormatDate() function in the Strings module.

If you do not set this field then the user's preferred date format will be used.

*****************************************************************************/

static ERROR GET_DateFormat(objView *Self, STRING *Value)
{
   *Value = Self->DateFormat;
   return ERR_Okay;
}

static ERROR SET_DateFormat(objView *Self, CSTRING Value)
{
   if ((Value) AND (Value[0])) StrCopy(Value, Self->DateFormat, sizeof(Self->DateFormat));
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Document: If document mode is enabled, this field must refer to a Document object for data processing and display.

If document mode has been enabled in the #Style field, a Document object must be referenced in this field for
the processing and display of data from the view object.  The Document object must be configured to use a template that
will process data from the view.  The Surface field of the document must also refer to the Surface field of the view,
or an error will be returned.

It is recommended that you do not initialise the document object as the view may need to perform its own
pre-initialisation procedures on it. The view will automatically initialise the document object when it is ready to do
so.

*****************************************************************************/

static ERROR SET_Document(objView *Self, objDocument *Value)
{
   UBYTE buffer[32];

   if (Value) {
      if (Value->Head.ClassID != ID_DOCUMENT) return PostError(ERR_InvalidObject);
      if ((Value->Head.Flags & NF_INITIALISED)) {
         LogF("@","Warning: Document should not be pre-initialised.");
      }
      if (Value->SurfaceID != Self->Layout->SurfaceID) {
         LogErrorMsg("Document surface ID %d != %d", Value->SurfaceID, Self->Layout->SurfaceID);
         return ERR_Failed;
      }

      // Pass special parameters to the document template

      IntToStr(Self->Head.UniqueID, buffer, sizeof(buffer));
      acSetVar(Value, "View", buffer);

      if (Self->ColBorder.Alpha) {
         Value->Border = Self->ColBorder;
         Value->BorderEdge = DBE_TOP|DBE_BOTTOM|DBE_RIGHT|DBE_LEFT;
      }
   }

   Self->Document = Value;
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
DragItems: Array of tag ID's representing dragged items.

When the user initiates a drag and drop operation from the view, the DragItems field will contain an array of tag ID's
that represent each item that is being dragged. The array remains valid up until the completion of the drag and drop
operation.  The #DragItemCount field reflects the number of items held in the array.

The view retains the DragItems array until the next drag and drop operation occurs.  You can manually clear the item
array by setting the DragItemCount field with a zero value.

*****************************************************************************/

static ERROR GET_DragItems(objView *Self, LONG **Value, LONG *Elements)
{
   *Value = Self->DragItems;
   *Elements = Self->DragItemCount;
   return ERR_Okay;
}

static ERROR SET_DragItems(objView *Self, LONG *Value, LONG Elements)
{
   // Clearing the dragitems array is permissible by writing this field with a NULL value.

   if (!Value) {
      if (Self->DragItems) {
         FreeMemory(Self->DragItems);
         Self->DragItems = NULL;
         Self->DragItemCount = 0;
      }
      return ERR_Okay;
   }
   else return ERR_Failed;
}

/*****************************************************************************

-FIELD-
DragItemCount: Reflects the number of items being dragged from the view.

When the user initiates a drag and drop operation from the view, the DragItemCount field will indicate the total number
of items that are being dragged.  An array of tag ID's for the dragged items can be extracted from the
#DragItems field.

The view retains the DragItems array until the next drag and drop operation occurs.  You can manually clear the item
array by setting the DragItemCount field with a zero value.

*****************************************************************************/

static ERROR SET_DragItemCount(objView *Self, LONG Value)
{
   if (Value IS 0) {
      if (Self->DragItems) {
         FreeMemory(Self->DragItems);
         Self->DragItems = NULL;
         Self->DragItemCount = 0;
      }
   }

   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
DragSource: Refers to the source object that is used during for drag and drop operations.

When initiating new drag and drop operations, the DragSource value will be used as the Source parameter when
calling ~Display.StartCursorDrag().  If this field is set to zero then the view will reference itself in
the Source parameter.

-FIELD-
ExpandCallback: A callback for receiving notifications when tree branches are expanded.

Set this field with a function reference to receive notifications when tree branches are expanded in the view.  This
will only work when in TREE and GROUP_TREE style modes.  The synopsis for the callback function is `ERROR Callback(objView *View, LONG TagIndex)`.

The index of the expanded tag is given in the TagIndex parameter, which you may use to add more items to the XML tree.
On return, the view will refresh the tree and redraw the display to reflect your changes.

*****************************************************************************/

static ERROR GET_ExpandCallback(objView *Self, FUNCTION **Value)
{
   if (Self->ExpandCallback.Type != CALL_NONE) {
      *Value = &Self->ExpandCallback;
      return ERR_Okay;
   }
   else return ERR_FieldNotSet;
}

static ERROR SET_ExpandCallback(objView *Self, FUNCTION *Value)
{
   if (Value) {
      if (Self->ExpandCallback.Type IS CALL_SCRIPT) UnsubscribeAction(Self->ExpandCallback.Script.Script, AC_Free);
      Self->ExpandCallback = *Value;
      if (Self->ExpandCallback.Type IS CALL_SCRIPT) SubscribeAction(Self->ExpandCallback.Script.Script, AC_Free);
   }
   else Self->ExpandCallback.Type = CALL_NONE;
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Flags: Optional flags.

*****************************************************************************/

static ERROR SET_Flags(objView *Self, LONG Value)
{
   if (Self->Head.Flags & NF_INITIALISED) {
      Value &= ~(VWF_NO_ICONS);
      Self->Flags = Value;

      if (Self->Flags & VWF_USER_DRAG) {
         if (glPreferDragDrop) Self->Flags |= VWF_DRAG_DROP;
         else Self->Flags &= ~VWF_DRAG_DROP;
      }
   }
   else {
      Self->Flags = Value;
   }

   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Focus: References the surface that must be used for user focus monitoring.

By default, the view will determine its user focus state by monitoring its own surface for user interaction.  If
another surface should be monitored (such as the application window) then it must be referenced here prior to
initialisation.  It is strongly recommended that the monitored surface is a parent of the view object.

Focus monitoring has a direct impact on keyboard input handling and the look and feel of the view - for example,
clicking the view area enables the arrow and enter keys for keyboard interaction.

-FIELD-
Font: Refers to the default font that renders text within the view.

The default font object that is used to draw text throughout the view is referenced via this field.  Configuring the
font prior to initialisation is encouraged, however making changes post-initialisation is likely to cause issues.
If the font settings need to be modified at run-time then consider recreating the view from scratch to eliminate the
odds of conflict.

-FIELD-
GfxFlags: Optional graphics flags.

This field provides special options that affect the look of the view when it is rendered.  It is strongly recommended
that these options are defined in the style template rather than set by the client directly.

-FIELD-
GroupFace: The font to use for group headers when the GROUP_TREE style is enabled.

The GroupFace field defines the font face that is used for group headers when the GROUP_TREE style is enabled.  If this
field is not set, the default font face is used for the group headers.

*****************************************************************************/

static ERROR SET_GroupFace(objView *Self, CSTRING Value)
{
   if (Self->GroupFace) FreeMemory(Self->GroupFace);

   if ((Self->GroupFace = StrClone(Value))) {
      return ERR_Okay;
   }
   else return ERR_AllocMemory;
}

/*****************************************************************************

-FIELD-
GroupHeight: Defines the height of group headers, in pixels.

This field applies when the view is using the GROUP_TREE #Style  It defines the pixel height of the group headers.

-FIELD-
HighlightTag: Refers to the current item that the user has highlighted.

The HighlightTag field refers to the tag index of the currently highlighted item.  An item is 'highlighted' if the
user's pointer is hovering over it.  If no item is highlighted then this field defaults to -1.

The index value can be used to lookup the item in the view's #XML Tags array.

-FIELD-
HScroll: If scrolling is required, this field must refer to a horizontal scroll bar.

If you want to attach a horizontal scrollbar to a view object, set this field to an object belonging to the Scroll
class.  So long as the Scroll object is set up to provide full scrollbar functionality, the user will be able to
scroll the text display along the horizontal axis.

*****************************************************************************/

static ERROR SET_HScroll(objView *Self, OBJECTPTR Value)
{
   if (Value) {
      if (Value->ClassID != ID_SCROLL) {
         return PostError(ERR_InvalidObject);
      }

      SetLong(Value, FID_Object, Self->Head.UniqueID);
   }

   Self->HScroll = Value;
   Self->XPos = 0;
   calc_hscroll(Self);
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
HSpacing: Specifies the minimum amount of horizontal white-space that will distance each item within the view.

Specifies the minimum amount of horizontal white-space that will distance each item within the view.  The value is
measured in pixels.

-FIELD-
IconFilter: Sets the preferred icon filter.

Setting the IconFilter will change the default graphics filter used for loading all future icons.  Existing
loaded icons are not affected by the change.

*****************************************************************************/

static ERROR GET_IconFilter(objView *Self, STRING *Value)
{
   if (Self->IconFilter[0]) *Value = Self->IconFilter;
   else *Value = NULL;
   return ERR_Okay;
}

static ERROR SET_IconFilter(objView *Self, CSTRING Value)
{
   if (!Value) Self->IconFilter[0] = 0;
   else StrCopy(Value, Self->IconFilter, sizeof(Self->IconFilter));
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
IconSize: Indicates the default icon size in each item.

The default pixel size of the icons that are rendered within items is defined here.  The default size is 16, and the
size cannot be reduced below this value for legibility reasons.  If icons are not required, please set NO_ICONS
in #Flags.

-FIELD-
IconTheme: Sets the preferred icon theme.

Setting the IconTheme will define the default theme used for loading all future icons.  Existing loaded icons are not
affected by the change.

*****************************************************************************/

static ERROR GET_IconTheme(objView *Self, STRING *Value)
{
   if (Self->IconTheme[0]) *Value = Self->IconTheme;
   else *Value = NULL;
   return ERR_Okay;
}

static ERROR SET_IconTheme(objView *Self, CSTRING Value)
{
   if (!Value) Self->IconTheme[0] = 0;
   else StrCopy(Value, Self->IconTheme, sizeof(Self->IconTheme));
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
ItemNames: Identifies the names of tags in the XML tree that must be treated as items.

Tag names that need to be recognised as items in the view's XML data must be declared in this field.  This is done by
supplying a wildcard string that OR's all possible item names together.  For example `folder|file`.

The default setting is 'item'.  Use of wildcard parameters such as the asterisk and question mark are permitted, as in
the StrCompare() function.

*****************************************************************************/

static ERROR SET_ItemNames(objView *Self, CSTRING Value)
{
   if (Self->ItemNames) { FreeMemory(Self->ItemNames); Self->ItemNames = NULL; }

   if ((!Value) OR (!*Value)) Value = "item";

   if ((Self->ItemNames = StrClone(Value))) return ERR_Okay;
   else return ERR_AllocMemory;
}

//****************************************************************************
// Internal field for supporting dynamic style changes when an object is used in a document.

static ERROR SET_LayoutStyle(objView *Self, DOCSTYLE *Value)
{
   if (!Value) return ERR_Okay;

   if (Self->Head.Flags & NF_INITIALISED) {
      docApplyFontStyle(Value->Document, Value, Self->Font);
   }
   else {
      docApplyFontStyle(Value->Document, Value, Self->Font);
   }

   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
MaxItemWidth: Limits item width when using the LIST or LONG_LIST #Style.

If this field is set when using the LIST or LONG_LIST #Style, the width of each item will not exceed the
indicated value.

-FIELD-
SelectCallback: A callback for receiving notifications when tree branches are expanded.
Lookup: SLF

Set this field with a reference to a callback function to receive notifications related to item selection.  The
synopsis for the callback function is `ERROR SelectCallback(objView *View, LONG Flags, LONG TagIndex)`.

The index of the changed item is given in the TagIndex parameter.  If the index is -1, then no tags are selected
(i.e. a deselection has occurred).  Flags provide additional information as to the type of selection that has
occurred, as in the following table:

<types lookup="SLF"/>

*****************************************************************************/

static ERROR GET_SelectCallback(objView *Self, FUNCTION **Value)
{
   if (Self->SelectCallback.Type != CALL_NONE) {
      *Value = &Self->SelectCallback;
      return ERR_Okay;
   }
   else return ERR_FieldNotSet;
}

static ERROR SET_SelectCallback(objView *Self, FUNCTION *Value)
{
   if (Value) {
      if (Self->SelectCallback.Type IS CALL_SCRIPT) UnsubscribeAction(Self->SelectCallback.Script.Script, AC_Free);
      Self->SelectCallback = *Value;
      if (Self->SelectCallback.Type IS CALL_SCRIPT) SubscribeAction(Self->SelectCallback.Script.Script, AC_Free);
   }
   else Self->SelectCallback.Type = CALL_NONE;
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
SelectedTag: Indicates the tag index of the most recently selected item.

The SelectedTag field provides a method for retrieving the tag index of the most recently selected item.  If no
selection is active then this field will return a value of -1.  The resulting index can be used to lookup the tag
structure in the view's XML Tags list.

-FIELD-
SelectedTags: Returns an array of tag indexes for all currently selected items.

A complete list of currently selected items can be retrieved by reading the SelectedTags field.  The data is returned
as an array of 32-bit tag indexes, terminated with a value of -1.  An error code will be returned if there are no
currently selected items.

The array remains valid up until the next time that the SelectedTags field is read.

*****************************************************************************/

static ERROR GET_SelectedTags(objView *Self, LONG **Array, LONG *Elements)
{
   *Array = NULL;

   if (Self->SelectedTags) { FreeMemory(Self->SelectedTags); Self->SelectedTags = NULL; }

   OBJECTPTR context = SetContext(Self);
   ERROR error = get_selected_tags(Self, &Self->SelectedTags, Elements);
   SetContext(context);

   if (!error) {
      *Array = Self->SelectedTags;
      return ERR_Okay;
   }
   else return error;
}

/*****************************************************************************

-FIELD-
Selection: Reflects the string content of the currently selected item.

The Selection field provides a method for retrieving the content of the currently selected item.  If no selection is
active then this field will return an error code and a NULL pointer.

An item can be selected manually by writing a valid item string to this field.  The string must refer to the name of an
item in the default column of the view.

*****************************************************************************/

static ERROR GET_Selection(objView *Self, STRING *Value)
{
   if (Self->SelectedTag != -1) {
      if (Self->XML->Tags) {
         *Value = get_nodestring(Self, (struct view_node *)Self->XML->Tags[Self->SelectedTag]->Private);
         return ERR_Okay;
      }
   }

   *Value = NULL;
   return ERR_NoData;
}

static ERROR SET_Selection(objView *Self, CSTRING Value)
{
   LogBranch("Selection = %s", Value);

   LONG index;
   for (index=0; Self->XML->Tags[index]; index++) {
      struct view_node *node = Self->XML->Tags[index]->Private;
      STRING str = get_nodestring(Self, node);
      if (str) {
         if (!StrMatch(Value, str)) {
            select_item(Self, Self->XML->Tags[index], SLF_MANUAL, TRUE, FALSE);
            LogBack();
            return ERR_Okay;
         }
      }
   }

   LogErrorMsg("Unable to find item \"%s\"", Value);
   LogBack();
   return ERR_Search;
}

/*****************************************************************************

-FIELD-
SelectionIndex: Indicates the index number of the currently selected item.

The SelectionIndex field provides a method for retrieving the index of the most recently selected item.  If no
selection is active then this field will return a value of -1.

You can also manually select an item by writing a valid index number to this field.  Changing the index will cause the
item to become highlighting and the view's children will be activated automatically to inform them of the change.  An
error will be returned if the index number is outside of the available range.  An index of -1 will deselect any
currently selected items.

Note that the index cannot be used as a lookup in the XML TagList array (use the #SelectedTag field instead).

*****************************************************************************/

static ERROR GET_SelectionIndex(objView *Self, LONG *Value)
{
   if (Self->SelectedTag != -1) {
      LONG index = 0;
      struct XMLTag *tag;
      for (tag=Self->XML->Tags[0]; (tag) AND (tag->Index != Self->SelectedTag); tag=tag->Next) index++;
      *Value = index;
      return ERR_Okay;
   }

   *Value = -1;
   return ERR_Okay;
}

static ERROR SET_SelectionIndex(objView *Self, LONG Value)
{
   LONG i;

   if (Value IS -1) {
      // Deselect all items
      LogMsg("SelectionIndex = %d (deselect-all)", Value);
      select_item(Self, 0, SLF_MANUAL, FALSE, FALSE);
      return ERR_Okay;
   }
   else if (Self->Head.Flags & NF_INITIALISED) {
      LogBranch("SelectionIndex = %d", Value);

         LONG index = Value;
         LONG count = 0;

         for (i=0; Self->XML->Tags[i]; i++) {
            if (((struct view_node *)Self->XML->Tags[i]->Private)->Flags & NODE_ITEM) {
               if (!index) {
                  select_item(Self, Self->XML->Tags[i], SLF_MANUAL, TRUE, FALSE);
                  acActivate(Self);
                  LogBack();
                  return ERR_Okay;
               }
               index--;
               count++;
            }
         }

         LogErrorMsg("Index %d out of range (max %d).", Value, count);

      LogBack();
      return ERR_OutOfRange;
   }
   else {
      Self->SelectionIndex = Value; // SelectionIndex field is provided purely for assisting the init process
      return ERR_Okay;
   }
}

/*****************************************************************************

-FIELD-
Style: The style of view that is displayed to the user is configured here.

The style of the view display can be configured by writing this field. You should set this field on initialisation, and
may change it at any time if the user wishes to switch to a different style.

*****************************************************************************/

static ERROR SET_Style(objView *Self, LONG Value)
{
   if (Self->Style != Value) {
      Self->Style = Value;

      if (Self->Head.Flags & NF_INITIALISED) {
         LogBranch("The view style has changed.");

         if (Self->Style IS VIEW_GROUP_TREE) {
            // Regenerate the group bitmap
            if (Self->GroupHeaderXML) {
               gen_group_bkgd(Self, Self->GroupHeaderXML, &Self->GroupBitmap, "style");
            }

            if (Self->GroupSelectXML) {
               gen_group_bkgd(Self, Self->GroupSelectXML, &Self->SelectBitmap, "style");
            }
         }

         Self->XPos = 0;
         Self->YPos = 0;
         arrange_items(Self);

         if (!Self->RedrawDue) {
            Self->RedrawDue = TRUE;
            DelayMsg(AC_Draw, Self->Layout->SurfaceID, NULL);
         }

         LogBack();
      }
   }

   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Template: Specify an alternative graphics style by name.

The Template can be set prior to initialisation to request a look and feel other than the default.  The style
name must be declared in the system installed style file, otherwise setting this field does nothing.  No error
is returned in the event that a problem occurs with the requested style.

*****************************************************************************/

static ERROR SET_Template(objView *Self, CSTRING Value)
{
   if (Value) drwApplyStyleValues(Self, Value);
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
TextAttrib: Declares the XML attribute to use when drawing the text for an item.

By default, the text drawn for each item in a view's XML tree is pulled from the content that is present within each
item's XML tag.  This condition is upheld so long as TextAttrib is set to NULL.

If the TextAttrib field is set to a string value, that string will identify the name of a tag attribute that must be
used when drawing item text.  If the named attribute is not declared in the tag for a given item, the content of that
tag will be used instead (if content is present).

*****************************************************************************/

static ERROR SET_TextAttrib(objView *Self, CSTRING Value)
{
   if (Self->TextAttrib) { FreeMemory(Self->TextAttrib); Self->TextAttrib = NULL; }

   if ((!Value) OR (!*Value)) return ERR_Okay;

   if ((Self->TextAttrib = StrClone(Value))) return ERR_Okay;
   else return ERR_AllocMemory;
}

/*****************************************************************************

-FIELD-
TotalSelected: The total number of currently selected items.

This readable field will tell you the total number of items that are currently selected by the user.

*****************************************************************************/

static ERROR GET_TotalSelected(objView *Self, LONG *Value)
{
   *Value = 0;

   LONG index;
   LONG count = 0;
   for (index=0; Self->XML->Tags[index]; index++) {
      if (((struct view_node *)Self->XML->Tags[index]->Private)->Flags & NODE_SELECTED) count++;
   }

   *Value = count;
   FMSG("Get:TotalSelected","%d", count);
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
TotalItems: The total number of items listed in the view.

This read-only field reflects the total number of items listed in the view.  It is automatically updated to remain in
sync with the #XML data at all times.

-FIELD-
VarDefault: Defines the default value to return when GetVar() fails.

VarDefault defines the default string value that will be returned when GetVar() fails to find a requested item.

*****************************************************************************/

static ERROR SET_VarDefault(objView *Self, CSTRING Value)
{
   if (Value) StrCopy(Value, Self->VarDefault, sizeof(Self->VarDefault));
   else Self->VarDefault[0] = 0;
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
VScroll: If scrolling is required, use this field to refer to a vertical scroll bar.

If you want to attach a vertical scrollbar to a view object, set this field to an object belonging to the Scroll class.
So long as the Scroll object is configured to provide full scrollbar functionality, the user will be able to scroll the
text display along the vertical axis.

*****************************************************************************/

static ERROR SET_VScroll(objView *Self, OBJECTPTR Value)
{
   if (Value) {
      if (Value->ClassID != ID_SCROLL) return PostError(ERR_InvalidObject);
      SetLong(Value, FID_Object, Self->Head.UniqueID);
   }

   Self->VScroll = Value;
   Self->YPos = 0;
   calc_vscroll(Self);
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
VSpacing: Specifies the minimum amount of vertical whitespace that will distance each item within the view.

Specifies the minimum amount of vertical whitespace that will distance each item within the view.  The value is
measured in pixels.

-FIELD-
XML: View items are stored and managed in this @XML object.

The content of the view is managed by the @XML object referenced by this field.  It is conceivable that any
element in the XML object can be displayed as an item if its name matches the #ItemNames field.  By default,
elements with a name of 'item' will be identified as such unless #ItemNames is modified.

The XML content of each matching item will be used for rendering the item name.  If this is undesirable, the
#TextAttrib field can be specified for extracting the name from one of the item's attributes.
-END-

*****************************************************************************/
