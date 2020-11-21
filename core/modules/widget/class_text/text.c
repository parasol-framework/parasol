/*****************************************************************************

The source code of the Parasol project is made publicly available under the
terms described in the LICENSE.TXT file that is distributed with this package.
Please refer to it for further information on licensing.

******************************************************************************

-CLASS-
Text: Provides text display and editing functionality.

The Text class provides a complete text display and editing service that is suitable for almost any situation that
requires effective text management.  The class is most effective when applied to general text display, text editing
services, command-lines and input boxes.

The Text class is closely linked to the Font class, which provides all of the code necessary for font management.  If
you require information on how to set font definitions such as the face and colour of the font, please refer to the
documentation for the Font class.  All fields in the @Font class are inherited, and you will find that the Face, Colour,
Bold, Point and Align fields are particularly helpful for setting font attributes.

The graphical area of a text object is defined using the standard dimension conventions (x, y, width and height).
Margins (left, right, top and bottom) also allow you to offset the text from the edges of the surface area.  The Text
class draws its graphics to the foreground only, so you have the choice of defining your own background to be placed
behind the text.  If you wish to forgo that in favour of a clear background, set the Background field to your preferred
colour.

The following example shows how to create a simple string display within a @Surface:

<pre>surface.new('text', { string='Hello World', x=5, y=10, colour='#303030' })</pre>

If you intend to create a text object that accepts user input, there are a number of flags available to you that decide
how the object will behave as the user enters text information.  You may also attach child objects that can be
activated when the user presses the enter key after typing in some information.  This can be useful for creating a
custom-built reaction to user input.  Here is an example of a text object that runs a script when the enter key is
pressed:

<pre>
surface.new('text', x=3, y=4,
   activated = function(Text)
      print(Text.string)
   end
})
</pre>

For long text lists, scrollbars can be attached via the HScroll and VScroll fields.  For hints on how to use a text
object to build a full featured text editing application, refer to the script file located at
`programs:apps/textviewer/main.dml`.

By default the Text class supports text highlighting for cut, copy and paste operations.  This support is backed by
system keypresses such as CTRL-C, CTRL-V and CTRL-X.

-END-

*****************************************************************************/

//#define DEBUG

#define PRV_TEXT
#include <parasol/main.h>
#include <parasol/modules/xml.h>
#include <parasol/modules/document.h>
#include <parasol/modules/font.h>
#include <parasol/modules/display.h>
#include <parasol/modules/surface.h>
#include <parasol/modules/widget.h>
#include "../defs.h"

//#define DBG_TEXT TRUE

static OBJECTPTR clText = NULL;
static struct RGB8 glHighlight = { 220, 220, 255, 255 };

#define COLOUR_LENGTH 16
#define CURSOR_RATE 1400

struct TextHistory {
   LONG  Number;
   UBYTE Buffer[120];
};

struct TextLine {
   STRING String;
   LONG   Length;
   LONG   PixelLength;
};

enum {
   STATE_ENTERED=1,
   STATE_INSIDE,
   STATE_EXITED
};

static const struct FieldArray clFields[];
static const struct ActionArray clTextActions[];
static const struct MethodArray clTextMethods[];

static void  add_history(objText *, CSTRING);
static ERROR add_line(objText *, CSTRING, LONG, LONG, LONG);
static ERROR add_xml(objText *, struct XMLTag *, WORD, LONG);
static ERROR calc_hscroll(objText *);
static ERROR calc_vscroll(objText *);
static LONG  calc_width(objText *, CSTRING, LONG);
static LONG  column_coord(objText *, LONG, LONG);
static ERROR cursor_timer(objText *, LARGE, LARGE);
static void  DeleteSelectedArea(objText *);
static void  draw_lines(objText *, LONG, LONG);
static void  draw_text(objText *, objSurface *, objBitmap *);
static void  feedback_activated(objText *);
static void  feedback_validate_input(objText *);
static void  GetSelectedArea(objText *, LONG *, LONG *, LONG *, LONG *);
static void  insert_char(objText *Self, LONG Unicode, LONG Column);
static void  key_event(objText *, evKey *, LONG);
static ERROR load_file(objText *, CSTRING);
static void  move_cursor(objText *, LONG, LONG);
static void  Redraw(objText *);
static void  redraw_cursor(objText *, LONG);
static void  redraw_line(objText *, LONG);
static ERROR replace_line(objText *, CSTRING, LONG, LONG);
static LONG  row_coord(objText *, LONG);
static void  stretch_text(objText *);
static void  validate_cursorpos(objText *, LONG);
static LONG  view_cursor(objText *);
static LONG  view_selection(objText *);
static LONG  xml_content_len(struct XMLTag *);
static void  xml_extract_content(struct XMLTag *, UBYTE *, LONG *, WORD);

#define AXF_NEWLINE   0x0002

#define Remove_cursor(a) redraw_cursor((a),FALSE)

//****************************************************************************

INLINE void set_point(objText *Self, DOUBLE Value)
{
   SetDouble(Self->Font, FID_Point, Value);

   // String widths need to be recalculated after resetting the point size.
   if (Self->Font->Head.Flags & NF_INITIALISED) {
      LONG i;
      for (i=0; i < Self->AmtLines; i++) {
         Self->Array[i].PixelLength = calc_width(Self, Self->Array[i].String, Self->Array[i].Length);
      }
   }
}

//****************************************************************************

ERROR init_text(void)
{
   OBJECTPTR style;
   if (!FindPrivateObject("glStyle", &style)) {
      UBYTE buffer[32];
      if (!acGetVar(style, "/colours/@texthighlight", buffer, sizeof(buffer))) {
         StrToColour(buffer, &glHighlight);
      }
   }

   return(CreateObject(ID_METACLASS, 0, &clText,
      FID_ClassVersion|TFLOAT, VER_TEXT,
      FID_Name|TSTRING,   "Text",
      FID_Category|TLONG, CCF_GUI,
      FID_Flags|TLONG,    CLF_PROMOTE_INTEGRAL|CLF_PRIVATE_ONLY,
      FID_Actions|TPTR,   clTextActions,
      FID_Methods|TARRAY, clTextMethods,
      FID_Fields|TARRAY,  clFields,
      FID_Size|TLONG,     sizeof(objText),
      FID_Path|TSTR,      MOD_PATH,
      TAGEND));
}

void free_text(void)
{
   if (clText) { acFree(clText); clText = NULL; }
}

//****************************************************************************

static void resize_text(objText *Self)
{
   if (Self->Flags & TXF_STRETCH) stretch_text(Self);
   if (Self->Flags & TXF_WORDWRAP) calc_vscroll(Self);

   if (Self->RelSize > 0) {
      DOUBLE point = Self->Layout->BoundHeight * Self->RelSize / 100.0;
      SetDouble(Self->Font, FID_Point, point);
   }
}

//****************************************************************************

static ERROR TEXT_ActionNotify(objText *Self, struct acActionNotify *Args)
{
   if (!Args) return ERR_NullArgs;

   if (Args->Error != ERR_Okay) {
      if (Args->ActionID IS AC_Write) {
         if (Self->FileStream) { acFree(Self->FileStream); Self->FileStream = NULL; }
      }
      return ERR_Okay;
   }

   if (Args->ActionID IS AC_Disable) {
      acDisable(Self);
   }
   else if (Args->ActionID IS AC_Enable) {
      acEnable(Self);
   }
   else if (Args->ActionID IS AC_Focus) {
      Self->CursorFlash = 0;
      redraw_cursor(Self, TRUE);
      if (Self->CursorTimer) UpdateTimer(Self->CursorTimer, 0.1);
      else {
         FUNCTION callback;
         SET_FUNCTION_STDC(callback, &cursor_timer);
         SubscribeTimer(0.1, &callback, &Self->CursorTimer);

         if (!Self->prvKeyEvent) {
            SET_FUNCTION_STDC(callback, &key_event);
            SubscribeEvent(EVID_IO_KEYBOARD_KEYPRESS, &callback, Self, &Self->prvKeyEvent);
         }
      }
   }
   else if (Args->ActionID IS AC_Free) {
      if ((Self->ValidateInput.Type IS CALL_SCRIPT) AND (Self->ValidateInput.Script.Script->UniqueID IS Args->ObjectID)) {
         Self->ValidateInput.Type = CALL_NONE;
      }
      else if ((Self->Activated.Type IS CALL_SCRIPT) AND (Self->Activated.Script.Script->UniqueID IS Args->ObjectID)) {
         Self->Activated.Type = CALL_NONE;
      }
   }
   else if (Args->ActionID IS AC_LostFocus) { // Flash the cursor via the timer
      if (Self->CursorTimer) { UpdateTimer(Self->CursorTimer, 0); Self->CursorTimer = 0; }
      if (Self->prvKeyEvent) { UnsubscribeEvent(Self->prvKeyEvent); Self->prvKeyEvent = NULL; }

      // When a simple input line loses the focus, all selections are deselected

      if (Self->LineLimit IS 1) {
         if (Self->Flags & TXF_AREA_SELECTED) { Self->Flags &= ~TXF_AREA_SELECTED; }
         if (Self->XPosition) { Self->XPosition = 0; }
      }

      Redraw(Self);

      // Optional feedback mechanism - note that this can trigger even if nothing has changed (ideally we need to
      // modify this so that there's no trigger if there are no changes).

      feedback_validate_input(Self);
   }
   else if (Args->ActionID IS AC_Write) {
      struct acWrite *write;
      if (!(write = (struct acWrite *)Args->Args)) return ERR_Okay;

      LogMsg("%d bytes incoming from file stream.", write->Result);

      if (write->Buffer) {
         acDataFeed(Self, Self->Head.UniqueID, DATA_TEXT, write->Buffer, write->Result);
      }
   }

   return ERR_Okay;
}

/*****************************************************************************

-METHOD-
AddLine: Adds a new line to any row position in a text object.

Lines can be added or inserted into a text object by using the AddLine() method.  You need to provide the text string
that you wish to use, the line number that the text will be inserted into, and the length of the text string.

If you set the Text argument to NULL, then an empty string will be inserted into the line number.  If the Line argument
is less than zero, then the string will be added to the end of the Text.  If the Length is set to -1, then the length
will be calculated by counting the amount of characters in the Text argument.

If the new line is visible within the text object's associated surface, that region of the surface will be redrawn so
that the new line is displayed.

-INPUT-
int Line: The number of the line at which the text should be inserted.
buf(cstr) String: The text that you want to add.
bufsize Length: The length of the string in bytes.

-ERRORS-
Okay
NullArgs
AllocMemory

*****************************************************************************/

static ERROR TEXT_AddLine(objText *Self, struct txtAddLine *Args)
{
   if (!Args) return PostError(ERR_NullArgs);

   return add_line(Self, Args->String, Args->Line, Args->Length, FALSE);
}

/*****************************************************************************
-ACTION-
Clear: Clears all content from the object.

You can delete all of the text information from a text object by calling the Clear action.  All of the text data will
be deleted from the object and the graphics will be automatically updated as a result of calling this action.

*****************************************************************************/

static ERROR TEXT_Clear(objText *Self, APTR Void)
{
   LONG i;
   APTR newarray;

   // Reallocate the line array

   Self->MaxLines = 50;
   if (AllocMemory(sizeof(struct TextLine) * Self->MaxLines, MEM_DATA, &newarray, NULL) != ERR_Okay) {
      return PostError(ERR_AllocMemory);
   }

   if (Self->Array) {
      for (i=0; i < Self->AmtLines; i++) {
         if (Self->Array[i].String) FreeResource(Self->Array[i].String);
      }
      FreeResource(Self->Array);
   }

   Self->Array        = newarray;
   Self->AmtLines     = 0;
   Self->CursorRow    = 0;
   Self->CursorColumn = 0;
   Self->YPosition    = 0;
   Self->XPosition    = 0;
   Self->ClickHeld    = FALSE;
   Self->SelectRow    = 0;
   Self->SelectColumn = 0;
   Self->Flags       &= ~TXF_AREA_SELECTED;

   if (!Self->NoUpdate) {
      Redraw(Self);
      calc_hscroll(Self);
      calc_vscroll(Self);
   }

   return ERR_Okay;
}

/*****************************************************************************
-ACTION-
Clipboard: Full support for clipboard activity is provided through this action.
-END-
*****************************************************************************/

static ERROR TEXT_Clipboard(objText *Self, struct acClipboard *Args)
{
   STRING buffer;
   LONG size, i, row, column, endrow, endcolumn, pos, start;

   if ((!Args) OR (!Args->Mode)) return PostError(ERR_NullArgs);

   if ((Args->Mode IS CLIPMODE_CUT) OR (Args->Mode IS CLIPMODE_COPY)) {
      if (Args->Mode IS CLIPMODE_CUT) LogBranch("Operation: Cut");
      else LogBranch("Operation: Copy");

      // Calculate the length of the highlighted text

      if ((Self->Flags & TXF_AREA_SELECTED) AND ((Self->SelectRow != Self->CursorRow) OR
          (Self->SelectColumn != Self->CursorColumn))) {

         GetSelectedArea(Self, &row, &column, &endrow, &endcolumn);
         column = UTF8CharOffset(Self->Array[row].String, column);
         endcolumn = UTF8CharOffset(Self->Array[endrow].String, endcolumn);

         size = 0;
         for (i=row; i <= endrow; i++) size += Self->Array[i].Length + 1;
         if (AllocMemory(size+1, MEM_STRING, &buffer, NULL) IS ERR_Okay) {
            pos = 0;
            start = row;

            // Copy the selected area into the buffer

            if (Self->Array[row].String) {
               if (row IS endrow) {
                  while (column < endcolumn) buffer[pos++] = Self->Array[row].String[column++];
               }
               else while (column < Self->Array[row].Length) buffer[pos++] = Self->Array[row].String[column++];
            }

            if (++row <= endrow) {
               for (; row < endrow; row++) {
                  buffer[pos++] = '\n';
                  if (Self->Array[row].String) {
                     for (i=0; i < Self->Array[row].Length; i++) buffer[pos++] = Self->Array[row].String[i];
                  }
               }
               buffer[pos++] = '\n';
               if (Self->Array[row].String) {
                  for (i=0; i < endcolumn; i++) buffer[pos++] = Self->Array[row].String[i];
               }
            }

            buffer[pos] = 0;

            // Send the text to the clipboard object

            objClipboard *clipboard;
            if (!CreateObject(ID_CLIPBOARD, 0, &clipboard, TAGEND)) {
               if (!ActionTags(MT_ClipAddText, clipboard, buffer)) {
                  // Delete the highlighted text if the CUT mode was used

                  if (Args->Mode IS CLIPMODE_CUT) DeleteSelectedArea(Self);
                  else {
                     //Self->Flags &= ~TXF_AREA_SELECTED;
                     //draw_lines(Self, start, endrow - start + 1);
                  }
               }
               else LogErrorMsg("Failed to add text to the system clipboard.");
               acFree(clipboard);
            }

            FreeResource(buffer);
         }
        else {
            PostError(ERR_AllocMemory);
            LogReturn();
            return ERR_AllocMemory;
         }
      }

      LogReturn();
      return ERR_Okay;
   }
   else if (Args->Mode IS CLIPMODE_PASTE) {
      LogBranch("Operation: Paste");

      if (!(Self->Flags & TXF_EDIT)) {
         LogErrorMsg("Edit mode is not enabled, paste operation aborted.");
         return ERR_Failed;
      }

      objClipboard *clipboard;
      if (!CreateObject(ID_CLIPBOARD, 0, &clipboard, TAGEND)) {
         struct clipGetFiles get;
         get.Datatype = CLIPTYPE_TEXT;
         get.Index = 0;
         if (!Action(MT_ClipGetFiles, clipboard, &get)) {
            OBJECTPTR file;
            if (!CreateObject(ID_FILE, 0, &file,
                  FID_Path|TSTR,   get.Files[0],
                  FID_Flags|TLONG, FL_READ,
                  TAGEND)) {

               if ((!GetLong(file, FID_Size, &size)) AND (size > 0)) {
                  if (!AllocMemory(size+1, MEM_STRING, &buffer, NULL)) {
                     LONG result;
                     if (!acRead(file, buffer, size, &result)) {
                        buffer[result] = 0;
                        acDataText(Self, buffer);
                     }
                     else LogErrorMsg("Failed to read data from the clipboard file.");
                     FreeResource(buffer);
                  }
                  else LogErrorMsg("Out of memory.");
               }

               acFree(file);
            }
            else LogF("@", "Failed to load clipboard file \"%s\"", get.Files[0]);
         }
         acFree(clipboard);
      }

      LogReturn();
      return ERR_Okay;
   }
   else return PostError(ERR_Args);
}

/*****************************************************************************

-ACTION-
DataFeed: Text data can be sent to a text object via data feeds.

A convenient method for appending data to a text object is via data feeds.  The Text class currently supports the
DATA_TEXT and DATA_XML types for this purpose.  If the text contains return codes, the data will be split into multiple
lines.

The surface that is associated with the Text object will be redrawn as a result of calling this action.

-ERRORS-
Okay
Args
AllocMemory
Mismatch:    The data type that was passed to the action is not supported by the Text class.
-END-

*****************************************************************************/

static ERROR TEXT_DataFeed(objText *Self, struct acDataFeed *Args)
{
   CSTRING line;
   STRING str, buffer;
   LONG i, j, len, linestart, pos, end, size, bufsize;

   if ((!Args) OR (!Args->Buffer)) return PostError(ERR_NullArgs);

   if ((Args->DataType IS DATA_TEXT) OR (Args->DataType IS DATA_CONTENT)) {
      if ((bufsize = Args->Size) <= 0) {
         bufsize = StrLength(Args->Buffer);
         if (!bufsize) return ERR_Okay;
      }

      LogF("~6DataFeed()","Inserting text data of size %d.", bufsize);

      Self->NoUpdate++;
      linestart = Self->CursorRow;
      line = Args->Buffer;

      if ((Self->Flags & TXF_EDIT) AND (Self->Flags & TXF_AREA_SELECTED)) {
         DeleteSelectedArea(Self);
      }

      if ((Self->Flags & TXF_EDIT) AND (Self->AmtLines > 0)) {
         if ((linestart = Self->CursorRow) < 0) linestart = 0;
         for (len=0; (line[len] != '\n') AND (line[len] != '\r') AND (len < bufsize); len++); // Length of the first line

         if (AllocMemory(Self->Array[Self->CursorRow].Length + len + 1, MEM_STRING|MEM_NO_CLEAR, &str, NULL) IS ERR_Okay) {

            if ((len >= bufsize) OR (Self->LineLimit IS 1)) {
               j = 0;
               for (i=0; (i < Self->CursorColumn) AND (i < Self->Array[Self->CursorRow].Length); i++) str[i] = Self->Array[Self->CursorRow].String[j++];
               for (pos=0; pos < len; pos++) str[i++] = line[pos];
               Self->CursorColumn = i;
               while (j < Self->Array[Self->CursorRow].Length) str[i++] = Self->Array[Self->CursorRow].String[j++];
               txtReplaceLine(Self, Self->CursorRow, str, i);
            }
            else {
               // Replace the first line

               j = 0;
               for (i=0; (i < Self->CursorColumn) AND (i < Self->Array[Self->CursorRow].Length); i++) str[i] = Self->Array[Self->CursorRow].String[j++];
               for (pos=0; pos < len; pos++) str[i++] = line[pos];
               end = i;
               while (j < Self->Array[Self->CursorRow].Length) str[i++] = Self->Array[Self->CursorRow].String[j++];
               str[i] = 0;
               txtReplaceLine(Self, Self->CursorRow, str, Self->CursorColumn + len);

               // Add further lines

               Self->CursorRow++;
               pos++;
               if ((pos < bufsize) AND (line[pos] IS '\r')) pos++;

               if (pos < bufsize) {
                  while (pos < bufsize) {
                     for (len=0; (line[pos+len] != '\n') AND (line[pos+len] != '\r') AND (pos+len < bufsize); len++);
                     add_line(Self, line+pos, Self->CursorRow++, len, FALSE);

                     if ((pos+len < bufsize) AND (line[pos+len] IS '\r')) len++;

                     if (pos+len < bufsize) {
                        len++;
                        if (pos+len >= bufsize) add_line(Self, 0, Self->CursorRow++, 0, FALSE); // Blank line
                     }
                     pos += len;
                  }
               }
               else add_line(Self, 0, Self->CursorRow++, 0, FALSE); // Blank line

               Self->CursorRow--;

               // Replace the last line

               for (len=0; str[end+len]; len++);
               if (Self->Array[Self->CursorRow].Length + len) {
                  if (AllocMemory(Self->Array[Self->CursorRow].Length + len, MEM_STRING|MEM_NO_CLEAR, &buffer, NULL) IS ERR_Okay) {
                     for (i=0; i < Self->Array[Self->CursorRow].Length; i++) buffer[i] = Self->Array[Self->CursorRow].String[i];
                     Self->CursorColumn = i;
                     for (j=0; j < len; j++) buffer[i++] = str[end+j];
                     txtReplaceLine(Self, Self->CursorRow, buffer, i);
                     FreeResource(buffer);
                  }
               }
            }

            FreeResource(str);
         }
      }
      else {
         STRING str;
         WORD linefeed, trailing_line;

         if ((linestart = Self->AmtLines - 1) < 1) linestart = 0;

         trailing_line = FALSE;
         for (pos=0; pos < bufsize; ) {
            // If we have run out of lines, expand the line list

            line = Args->Buffer + pos;
            size = bufsize - pos;
            linefeed = 1; // Normally a linefeed will consist of just the return character

            if (Self->Tag) {
               // NOTE: When text is encapsulated inside <text>...</text> tags, ALL whitespace is converted into spaces in this processing routine.  If the user wishes to
               // force return codes, he should use the \n character to do this, or a double-return if WORDWRAP is enabled.

               for (len=0; len < size; len++) {
                  if (Self->Flags & TXF_WORDWRAP) {
                     // Two returns indicate a line break when wordwrap is enabled
                     if ((line[len] IS '\n') AND (len < size-1) AND (line[len+1] IS '\n')) break;
                  }

                  if ((line[len] IS '\\') AND (len+1 < size) AND (line[len+1] IS 'n')) {
                     linefeed = 2;
                     break;
                  }
               }
            }
            else {
               for (len=0; (line[len] != '\n') AND (len < size); len++);
               // Check for a trailing line (a return code at the very end of the data feed)

               if ((len IS size-1) AND (line[len] IS '\n')) trailing_line = TRUE;
            }

            if (len > 0) {
               if (AllocMemory(len+1, MEM_STRING|MEM_NO_CLEAR, &str, NULL) IS ERR_Okay) {
                  j = 0;
                  if (Self->Tag) {
                     for (i=0; i < len; i++) {
                        if ((line[i] IS '\\') AND (i+1 < len)) {
                           if (line[i+1] IS '\\') { str[j++] = '\\'; i++; }
                           else if (line[i+1] IS 'n') i++; // Skip "\n" character strings
                        }
                        else if (line[i] IS '\r'); // Ignore carriage returns
                        else if (line[i] IS '\t') str[j++] = '\t'; // Accept tabs, don't convert to ' '
                        else if (line[i] <= 0x20) {
                           if ((j > 0) AND (str[j-1] IS ' ')); // Do nothing if the last character was a space
                           else str[j++] = ' ';  // Turn all other whitespace into spaces
                        }
                        else str[j++] = line[i]; // Accept standard character
                     }
                  }
                  else for (i=0; i < len; i++) {
                     if (line[i] IS '\r') continue;
                     str[j++] = line[i];
                  }
                  str[j] = 0;

                  add_line(Self, str, -1, j, TRUE);
               }
            }
            else {
               // Add a blank line
               add_line(Self, "", -1, 0, FALSE);
            }

            if (trailing_line) {
               add_line(Self, "", -1, 0, FALSE);
               break;
            }

            pos += len + linefeed;
         }
      }

      Self->NoUpdate--;

      if (!Self->NoUpdate) {
         calc_hscroll(Self);
         calc_vscroll(Self);

         draw_lines(Self, linestart, Self->AmtLines - linestart);
         view_cursor(Self);
      }

      LogReturn();
   }
   else if (Args->DataType IS DATA_XML) {
      struct XMLTag *tag;
      LONG linestart, itemcount;

      LogF("6","Received an XML statement of %d bytes.", Args->Size);

      // Accepted XML tags are:
      //
      // ITEM: Lines can be grouped under separate item tags.
      // P:    Paragraph with line-break.
      //
      // Please note that XML tags currently affect the entire line - a tag cannot affect a selected portion of text :-/

      if ((linestart = Self->AmtLines - 1) < 1) linestart = 0;  //linestart = Self->AmtLines;

      if (!Self->XML) {
         if (CreateObject(ID_XML, NF_INTEGRAL, &Self->XML,
               FID_Statement|TSTR, Args->Buffer,
               TAGEND) != ERR_Okay) {
            return PostError(ERR_CreateObject);
         }
      }
      else if (SetString(Self->XML, FID_Statement, Args->Buffer) != ERR_Okay) {
         return ERR_SetField;
      }

      // Search for <item> tags and add them as individual lines

      itemcount = 0;
      for (tag=Self->XML->Tags[0]; tag; tag=tag->Next) {
         if (!StrMatch("item", tag->Attrib[0].Name)) {
            add_xml(Self, tag, 0, -1);
            itemcount++;
         }
         else if (!StrMatch("p", tag->Attrib[0].Name)) {
            add_xml(Self, tag, AXF_NEWLINE, -1);
            itemcount++;
         }
      }

      // If there were no <item> tags in the XML statement, assume that the XML statement counts as one single item.

      if (!itemcount) add_xml(Self, Self->XML->Tags[0], FALSE, -1);
   }
   else if (Args->DataType IS DATA_INPUT_READY) {
      struct InputMsg *input;

      while (!gfxGetInputMsg((struct dcInputReady *)Args->Buffer, 0, &input)) {
         if (input->Type IS JET_LMB) {
            if (input->Value > 0) {
               struct acDraw draw;
               STRING str;
               LONG rowcount, i, clickrow, clickcol;
               UBYTE outofbounds;

               if (!(Self->Flags & (TXF_EDIT|TXF_SINGLE_SELECT|TXF_MULTI_SELECT))) continue;

               LogBranch(NULL);

               Self->CursorFlash = 0;
               outofbounds = FALSE;
               clickrow = 0;
               clickcol = 0;

               // Determine the row that was clicked

               if (Self->AmtLines > 0) {
                  clickrow = (input->Y - (Self->Layout->Document ? 0 : Self->Layout->TopMargin) - Self->YPosition) / Self->Font->LineSpacing;
                  if (clickrow >= Self->AmtLines) {
                     clickrow = Self->AmtLines - 1;
                     outofbounds = TRUE;
                  }
               }

               // Determine the column that was clicked

               if (clickrow < Self->AmtLines) {
                  if (Self->Array[clickrow].String) {
                     if (Self->Flags & TXF_SECRET) {
                        BYTE buffer[Self->Array[clickrow].Length+1];
                        for (i=0; i < Self->Array[clickrow].Length; i++) buffer[i] = '*';
                        buffer[i+1] = 0;

                        fntConvertCoords(Self->Font, buffer, input->X - Self->Layout->BoundX - (Self->Layout->Document ? 0 : Self->Layout->LeftMargin) - Self->XPosition, 0,
                           0, 0, &clickcol, 0, 0);
                     }
                     else {
                        fntConvertCoords(Self->Font, Self->Array[clickrow].String,
                           input->X - Self->Layout->BoundX - (Self->Layout->Document ? 0 : Self->Layout->LeftMargin) - Self->XPosition, 0,
                           0, 0, &clickcol, 0, 0);
                     }
                  }
               }

               // If there is an old area selection, clear it

               if (Self->Flags & TXF_AREA_SELECTED) {
                  Self->Flags &= ~TXF_AREA_SELECTED;

                  draw.X = Self->Layout->BoundX;
                  draw.Width  = Self->Layout->BoundWidth;
                  if (Self->CursorRow < Self->SelectRow) {
                     draw.Y = row_coord(Self, Self->CursorRow);
                     rowcount = Self->SelectRow - Self->CursorRow + 1;
                  }
                  else {
                     draw.Y = row_coord(Self, Self->SelectRow);
                     rowcount = Self->CursorRow - Self->SelectRow + 1;
                  }
                  draw.Height = rowcount * Self->Font->LineSpacing;

                  Self->NoCursor++;
                  ActionMsg(AC_Draw, Self->Layout->SurfaceID, &draw);
                  Self->NoCursor--;
               }
               else if (Self->Flags & TXF_EDIT) {
                  Remove_cursor(Self);
               }

               Self->SelectRow    = clickrow;
               Self->SelectColumn = clickcol;
               Self->ClickHeld    = TRUE;

               // Return if we are NOT in edit mode and the position of the click was out of bounds

               if ((outofbounds) AND (!(Self->Flags & TXF_EDIT))) {
                  LogReturn();
                  continue;
               }

               // Return if the row and column values will remain unchanged

               if (((clickcol IS Self->CursorColumn) AND (clickrow IS Self->CursorRow)) OR
                   (Self->AmtLines < 1)) {
                  if (!(input->Flags & JTYPE_DBL_CLICK)) {
                     redraw_cursor(Self, TRUE);
                     LogReturn();
                     continue;
                  }
               }

               Self->CursorRow    = clickrow;
               Self->CursorColumn = clickcol;

               // For double-clicks, highlight the word next to the cursor

               if ((input->Flags & JTYPE_DBL_CLICK) AND (!(Self->Flags & TXF_SECRET))) {
                  // Scan back to find the start of the word

                  str = Self->Array[Self->CursorRow].String;
                  for (i=Self->CursorColumn; i > 0; i--) {
                     if (str[i-1] <= 47) break;
                     if ((str[i-1] >= 58) AND (str[i-1] <= 64)) break;
                     if ((str[i-1] >= 91) AND (str[i-1] <= 96)) break;
                     if ((str[i-1] >= 123) AND (str[i-1] <= 127)) break;
                  }

                  Self->SelectColumn = i;

                  // Scan forward to find the end of the word

                  for (i=Self->CursorColumn; i < Self->Array[Self->CursorRow].Length; i++) {
                     if (str[i] <= 47) break;
                     if ((str[i] >= 58) AND (str[i] <= 64)) break;
                     if ((str[i] >= 91) AND (str[i] <= 96)) break;
                     if ((str[i] >= 123) AND (str[i] <= 127)) break;
                  }

                  Self->CursorColumn = i;

                  Self->SelectRow = Self->CursorRow;

                  if (Self->SelectColumn != Self->CursorColumn) {
                     Self->Flags |= TXF_AREA_SELECTED;
                     redraw_line(Self, Self->CursorRow);
                  }
                  else redraw_cursor(Self, TRUE);
               }
               else redraw_cursor(Self, TRUE);

               view_cursor(Self);

               LogReturn();
            }
            else {
               if (!(Self->Flags & (TXF_EDIT|TXF_SINGLE_SELECT|TXF_MULTI_SELECT))) continue;

               Self->ClickHeld = FALSE;
               if ((Self->SelectRow != Self->CursorRow) OR (Self->SelectColumn != Self->CursorColumn)) {
                  Self->Flags |= TXF_AREA_SELECTED;
               }
            }
         }
         else if (input->Flags & JTYPE_MOVEMENT) {
            LONG oldrow, oldcolumn, x;
            UBYTE inside;

            // Determine the current movement state (exit, enter, inside)

            if (Self->Flags & TXF_EDIT) {
               inside = TRUE;
               if (input->OverID IS Self->Layout->SurfaceID) {
                  if ((input->X < Self->Layout->BoundX) OR (input->Y < Self->Layout->BoundY) OR
                      (input->X >= Self->Layout->BoundX + Self->Layout->BoundWidth) OR (input->Y >= Self->Layout->BoundY + Self->Layout->BoundHeight)) {
                     inside = FALSE;
                  }
               }
               else inside = FALSE;

               if (inside) {
                  if (Self->State IS STATE_ENTERED) Self->State = STATE_INSIDE;
                  else if (Self->State != STATE_INSIDE) {
                     Self->State = STATE_ENTERED;

                     gfxSetCursor(0, CRF_BUFFER, PTR_TEXT, 0, Self->Head.UniqueID);
                     Self->PointerLocked = TRUE;
                  }
               }
               else {
                  if (Self->State != STATE_EXITED) {
                     Self->State = STATE_EXITED;
                     gfxRestoreCursor(PTR_DEFAULT, Self->Head.UniqueID);
                     Self->PointerLocked = FALSE;
                  }
               }
            }

            if (Self->ClickHeld IS FALSE) continue;

            if (Self->AmtLines < 1) continue;

            if (!(Self->Flags & (TXF_EDIT|TXF_SINGLE_SELECT|TXF_MULTI_SELECT))) continue;

            if (Self->Flags & TXF_SECRET) continue;

            oldrow = Self->CursorRow;
            oldcolumn = Self->CursorColumn;

            // Calculate the cursor row

            Self->CursorRow = (input->Y - (Self->Layout->Document ? 0 : Self->Layout->TopMargin) - Self->YPosition) / Self->Font->LineSpacing;

            if (Self->CursorRow < 0) Self->CursorRow = 0;
            if (Self->CursorRow >= Self->AmtLines) Self->CursorRow = Self->AmtLines - 1;

            // Calculate the cursor column

            Self->CursorColumn = 0;
            if (Self->Array[Self->CursorRow].String) {
               x = input->X - Self->Layout->BoundX - (Self->Layout->Document ? 0 : Self->Layout->LeftMargin) - Self->XPosition;
               fntConvertCoords(Self->Font, Self->Array[Self->CursorRow].String, x, 0,
                  0, 0, &Self->CursorColumn, 0, 0);
            }

            if ((Self->CursorRow != oldrow) OR (Self->CursorColumn != oldcolumn)) {
               // Set the AREASELECTED flag if an area has been highlighted by the user

               if ((Self->SelectRow != Self->CursorRow) OR (Self->SelectColumn != Self->CursorColumn)) {
                  Self->Flags |= TXF_AREA_SELECTED;
               }

               if (Self->CursorRow < oldrow) draw_lines(Self, Self->CursorRow, oldrow - Self->CursorRow + 1);
               else draw_lines(Self, oldrow, Self->CursorRow - oldrow + 1);
            }

            view_cursor(Self);
         }
      }
   }
   else {
      LogMsg("Datatype %d not supported.", Args->DataType);
      return ERR_Mismatch;
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

static ERROR TEXT_DeleteLine(objText *Self, struct txtDeleteLine *Args)
{
   if (Self->AmtLines < 1) return ERR_Okay;

   if (Self->CursorRow IS Args->Line) move_cursor(Self, Self->CursorRow, 0);

   if ((!Args) OR (Args->Line < 0)) {
      // Delete the line at the very end of the list
      if (Self->Array[Self->AmtLines-1].String) FreeResource(Self->Array[Self->AmtLines-1].String);
      ClearMemory(Self->Array + Self->AmtLines - 1, sizeof(Self->Array[0]));

      if (Self->Flags & TXF_AREA_SELECTED) {
         Self->Flags &= ~TXF_AREA_SELECTED;
         Redraw(Self);
      }
      else redraw_line(Self, Self->AmtLines-1);

      Self->AmtLines--;
      if (Self->CursorRow >= Self->AmtLines) move_cursor(Self, Self->AmtLines-1, Self->CursorColumn);
   }
   else {
      if (Args->Line >= Self->AmtLines) return PostError(ERR_Args);

      if (Self->Array[Args->Line].String) FreeResource(Self->Array[Args->Line].String);
      ClearMemory(Self->Array + Args->Line, sizeof(Self->Array[0]));

      if (Args->Line < Self->AmtLines - 1) {
         CopyMemory(Self->Array + Args->Line + 1, Self->Array + Args->Line, sizeof(Self->Array[0]) * (Self->AmtLines - Args->Line - 1));
      }

      Self->AmtLines--;

      if (Self->CursorRow >= Self->AmtLines) move_cursor(Self, Self->AmtLines-1, Self->CursorColumn);

      if (Self->Flags & TXF_AREA_SELECTED) {
         Self->Flags &= ~TXF_AREA_SELECTED;
         Redraw(Self);
      }
      else draw_lines(Self, Args->Line, Self->AmtLines - Args->Line + 1);
   }

   calc_hscroll(Self);
   calc_vscroll(Self);
   return ERR_Okay;
}

/*****************************************************************************
-ACTION-
Disable: Disables object functionality.
-END-
*****************************************************************************/

static ERROR TEXT_Disable(objText *Self, APTR Void)
{
   Self->Flags |= TXF_DISABLED;
   return ERR_Okay;
}

//****************************************************************************

static ERROR TEXT_Draw(objText *Self, APTR Void)
{
   if (Self->Layout->SurfaceID) {
      Redraw(Self);
      return ERR_Okay;
   }
   else return ERR_FieldNotSet;
}

/*****************************************************************************
-ACTION-
Enable: Enables object functionality.
-END-
*****************************************************************************/

static ERROR TEXT_Enable(objText *Self, APTR Void)
{
   Self->Flags &= ~TXF_DISABLED;
   return ERR_Okay;
}

/*****************************************************************************
-ACTION-
Focus: Calling this action will activate keyboard input.
-END-
*****************************************************************************/

static ERROR TEXT_Focus(objText *Self, APTR Void)
{
   return acFocusID(Self->Layout->SurfaceID);
}

//****************************************************************************

static ERROR TEXT_Free(objText *Self, APTR Void)
{
   if (Self->CursorTimer) { UpdateTimer(Self->CursorTimer, 0); Self->CursorTimer = 0; }
   if (Self->prvKeyEvent) { UnsubscribeEvent(Self->prvKeyEvent); Self->prvKeyEvent = NULL; }

   if ((Self->FocusID) AND (Self->FocusID != Self->Layout->SurfaceID)) {
      OBJECTPTR object;
      if (!AccessObject(Self->FocusID, 5000, &object)) {
         UnsubscribeAction(object, 0);
         ReleaseObject(object);
      }
   }

   if (Self->Layout) { acFree(Self->Layout); Self->Layout = NULL; }

   if (Self->PointerLocked) {
      gfxRestoreCursor(PTR_DEFAULT, Self->Head.UniqueID);
      Self->PointerLocked = FALSE;
   }

   if (Self->Array) {
      LONG i;
      for (i=0; i < Self->AmtLines; i++) {
         if (Self->Array[i].String) FreeResource(Self->Array[i].String);
      }
      FreeResource(Self->Array);
      Self->Array = NULL;
   }
   Self->AmtLines = 0;
   Self->MaxLines = 0;

   if (Self->FileStream)   { acFree(Self->FileStream); Self->FileStream = NULL; }
   if (Self->StringBuffer) { FreeResource(Self->StringBuffer); Self->StringBuffer = NULL; }
   if (Self->Location) { FreeResource(Self->Location); Self->Location = NULL; }
   if (Self->History)  { FreeResource(Self->History); Self->History = NULL; }
   if (Self->XML)      { acFree(Self->XML);  Self->XML = NULL; }
   if (Self->Font)     { acFree(Self->Font); Self->Font = NULL; }

   gfxUnsubscribeInput(0);

   return ERR_Okay;
}

/*****************************************************************************

-METHOD-
GetLine: Returns the string content of any given line.

This method can be used to get the string associated with any given line number.  You may choose to provide a buffer
space for the method to output the string data to, or you may set the Buffer argument to NULL to have the method
allocate a memory block containing the string.  If you are providing a buffer, make sure that the Length argument is
set to the correct buffer size.  In the case of allocated buffers, the Length argument will be updated to reflect the
length of the allocation (including the NULL byte).

-INPUT-
int Line: The line number that you want to retrieve.  Must be zero or greater.
buf(str) Buffer: Point this argument to a buffer space for the string result, or set to NULL if a buffer should be allocated by the method.
bufsize Length: Set this argument to the length of the buffer that you have provided, or set to NULL if a buffer is to be allocated.

-ERRORS-
Okay
NullArgs
OutOfRange: The line number that you specified was outside of the valid range of line numbers.
AllocMemory: The necessary amount of buffer space could not be allocated.
-END-

*****************************************************************************/

static ERROR TEXT_GetLine(objText *Self, struct txtGetLine *Args)
{
   if (!Args) return PostError(ERR_NullArgs);

   if (Args->Line >= Self->AmtLines) {
      LogErrorMsg("Cannot retrieve line %d (%d lines available).", Args->Line, Self->AmtLines);
      return ERR_OutOfRange;
   }

   if (!Args->Buffer) {
      Args->Length = Self->Array[Args->Line].Length + 1;
      if (AllocMemory(Args->Length, MEM_STRING|MEM_NO_CLEAR|MEM_TASK, (APTR *)&Args->Buffer, NULL) != ERR_Okay) {
         return ERR_AllocMemory;
      }
   }

   StrCopy(Self->Array[Args->Line].String, Args->Buffer, Args->Length);
   return ERR_Okay;
}

//****************************************************************************

static ERROR TEXT_Hide(objText *Self, APTR Void)
{
   return acHide(Self->Layout);
}

//****************************************************************************

static ERROR TEXT_Init(objText *Self, APTR Void)
{
   objSurface *surface;
   OBJECTPTR object;
   ERROR error;
   LONG i;

   SetFunctionPtr(Self->Layout, FID_DrawCallback, &draw_text);
   SetFunctionPtr(Self->Layout, FID_ResizeCallback, &resize_text);
   if (acInit(Self->Layout) != ERR_Okay) return ERR_Init;

   if (!Self->FocusID) Self->FocusID = Self->Layout->SurfaceID;

   // Subscribe to the surface

   if (!AccessObject(Self->Layout->SurfaceID, 5000, &surface)) {
      SubscribeActionTags(surface,
         AC_Disable,
         AC_Enable,
         TAGEND);
      ReleaseObject(surface);
   }
   else return PostError(ERR_AccessObject);

   if (Self->Flags & (TXF_EDIT|TXF_SINGLE_SELECT|TXF_MULTI_SELECT)) {
      gfxSubscribeInput(Self->Layout->SurfaceID, JTYPE_MOVEMENT|JTYPE_BUTTON, 0);
   }

   // Initialise the Font

   if (Self->RelSize > 0) {
      Self->Font->Point = (DOUBLE)Self->Layout->ParentSurface.Height * Self->RelSize / 100.0;
      Self->Font->Flags |= FTF_PREFER_SCALED;
      MSG("Font Size = %.2f (%d * %.2f%% / 100.0)", Self->Font->Point, Self->Layout->ParentSurface.Height, Self->RelSize);
   }
   else if (Self->Flags & TXF_STRETCH) Self->Font->Flags |= FTF_PREFER_SCALED;

   if (acInit(Self->Font) != ERR_Okay) return PostError(ERR_Init);

   // Now that we have a font, we can calculate the pixel widths of each existing text line

   for (i=0; i < Self->AmtLines; i++) {
      Self->Array[i].PixelLength = calc_width(Self, Self->Array[i].String, Self->Array[i].Length);
   }

   if (Self->Flags & TXF_STRETCH) stretch_text(Self);

   // Load a text file into the line array if required

   if (Self->Location) {
      if ((error = load_file(Self, Self->Location))) return PostError(error);
   }

   // Allocate a history buffer if TXF_HISTORY is enabled

   if (Self->Flags & TXF_HISTORY) {
      if (Self->HistorySize < 1) return PostError(ERR_InvalidValue);
      if (AllocMemory(Self->HistorySize * sizeof(struct TextHistory), MEM_DATA, &Self->History, NULL) != ERR_Okay) {
         Self->Flags &= ~TXF_HISTORY;
      }
   }

   if (Self->Flags & TXF_GLOBAL_EDITING) {
      LogMsg("Using global editing mode.");

      struct acActionNotify notify;
      notify.ActionID = AC_Focus;
      notify.Error    = ERR_Okay;
      Action(AC_ActionNotify, Self, &notify);
   }
   else if (!AccessObject(Self->FocusID, 5000, &object)) {
      SubscribeActionTags(object,
         AC_Focus,
         AC_LostFocus,
         TAGEND);

      if ((Self->Flags & TXF_EDIT) AND (((objSurface *)object)->Flags & RNF_HAS_FOCUS)) {
         FUNCTION callback;
         SET_FUNCTION_STDC(callback, &cursor_timer);
         SubscribeTimer(0.1, &callback, &Self->CursorTimer); // Flash the cursor via the timer

         SET_FUNCTION_STDC(callback, &key_event);
         SubscribeEvent(EVID_IO_KEYBOARD_KEYPRESS, &callback, Self, &Self->prvKeyEvent);
      }

      ReleaseObject(object);
   }

   // In command line mode, the cursor is placed at the end of any existing text on initialisation.

   if (Self->LineLimit IS 1) {
      if (Self->Array[0].String) Self->CursorColumn = Self->Array[0].Length;
   }

   calc_hscroll(Self);
   calc_vscroll(Self);

   return ERR_Okay;
}

//****************************************************************************

static ERROR TEXT_NewObject(objText *Self, APTR Void)
{
   if (!NewObject(ID_FONT, NF_INTEGRAL, &Self->Font)) {
      SetString(Self->Font, FID_Face, glDefaultFace);

      Self->Highlight.Red   = glHighlight.Red;
      Self->Highlight.Green = glHighlight.Green;
      Self->Highlight.Blue  = glHighlight.Blue;
      Self->Highlight.Alpha = 255;
      Self->CursorColour.Red   = 100;
      Self->CursorColour.Green = 100;
      Self->CursorColour.Blue  = 200;
      Self->CursorColour.Alpha = 255;
      Self->MaxLines        = 50;
      Self->HistorySize     = 20;
      Self->CursorWidth     = 1;
      Self->CharLimit       = 4096; // Maximum number of characters per line
      Self->LineLimit       = 0x7fffffff;
      if (!AllocMemory(sizeof(struct TextLine) * Self->MaxLines, MEM_DATA, &Self->Array, NULL)) {
         if (!NewObject(ID_LAYOUT, NF_INTEGRAL, &Self->Layout)) {
            return ERR_Okay;
         }
         else return ERR_NewObject;
      }
      else return PostError(ERR_AllocMemory);
   }
   else return PostError(ERR_NewObject);
}

/*****************************************************************************

-METHOD-
ReplaceLine: Replaces the content of any text line.

Any line within a text object can be replaced with new information by using the ReplaceLine() method.  You need to
provide the text string that you wish to use, the number of the line that will be replaced, and the length of the text
string.

If you set the String argument to NULL, then an empty string will replace the line number.  If the Length is set to -1,
then the length of the new string will be calculated by counting the amount of characters in the String argument.

If the new line content is visible within the text object's associated surface, that region of the surface will be
redrawn so that the new line content is displayed.

-INPUT-
int Line:   The line number that will be replaced.
buf(cstr) String: The text data that you want to use in replacing the line.
bufsize Length: The length of the String in bytes.

-ERRORS-
Okay
Args
AllocMemory: The memory required to add the text string to the list was unavailable.
-END-

*****************************************************************************/

static ERROR TEXT_ReplaceLine(objText *Self, struct txtReplaceLine *Args)
{
   if (!Args) return PostError(ERR_NullArgs);
   return replace_line(Self, Args->String, Args->Line, Args->Length);
}

/*****************************************************************************
-ACTION-
SaveToObject: Use this action to save edited information as a text file.
-END-
*****************************************************************************/

static ERROR TEXT_SaveToObject(objText *Self, struct acSaveToObject *Args)
{
   if ((!Args) OR (!Args->DestID)) return PostError(ERR_NullArgs);

   LogAction("Destination: %d, Lines: %d", Args->DestID, Self->AmtLines);

   if ((Self->Array) AND (Self->AmtLines > 0)) {
      if ((Self->AmtLines IS 1) AND (Self->Array[0].Length < 1)) return ERR_Okay;

      OBJECTPTR Object;
      LONG i;

      if (!AccessObject(Args->DestID, 5000, &Object)) {
         for (i=0; i < Self->AmtLines; i++) {
            // Output line
            if (Self->Array[i].Length > 0) {
               acWrite(Object, Self->Array[i].String, Self->Array[i].Length, NULL);
            }
            // Output return code
            if (i < Self->AmtLines-1) {
               acWrite(Object, "\n", 1, NULL);
            }
         }
         ReleaseObject(Object);
      }
   }

   return ERR_Okay;
}

/*****************************************************************************
-ACTION-
ScrollToPoint: Scrolls a text object's graphical content.
-END-
*****************************************************************************/

static ERROR TEXT_ScrollToPoint(objText *Self, struct acScrollToPoint *Args)
{
   if (!Args) return PostError(ERR_NullArgs);

   if ((Args->X IS Self->XPosition) AND (Args->Y IS Self->YPosition)) return ERR_Okay;

   OBJECTPTR surface;
   if (!AccessObject(Self->Layout->SurfaceID, 5000, &surface)) {
      LONG x, y;
      if (Args->Flags & STP_X) x = -Args->X;
      else x = Self->XPosition;

      if (Args->Flags & STP_Y) y = -Args->Y;
      else y = Self->YPosition;

      Self->XPosition = x;
      Self->YPosition = y;

      Redraw(Self);

      ReleaseObject(surface);
   }

   return ERR_Okay;
}

/*****************************************************************************

-METHOD-
SelectArea: Forces a user selection over a specific text area.

If you would like to force a user selection over a specific text area, use this method.  Normally, user selections
occur when the user moves a pointing device over a Text object to highlight an area of text.  By using this method, you
will bypass that procedure by highlighting an area manually.

The area that you specify will be highlighted as if the user had selected that area him or herself.  The selection can
be cancelled if the user performs an activity that causes the selection to be removed.

-INPUT-
int Row: The row from which the selection will start.
int Column: The column from which the selection will start.
int EndRow: The number of the row that will terminate the selection.
int EndColumn: The number of the column that will terminate the selection.

-ERRORS-
Okay
Args
-END-

*****************************************************************************/

static ERROR TEXT_SelectAreaText(objText *Self, struct txtSelectArea *Args)
{
   if ((!Args) OR (Args->Row < 0) OR (Args->Column < 0) OR
       (Args->EndRow < 0) OR (Args->EndColumn < 0)) {
      return PostError(ERR_Args);
   }

   LogMethod("%dx%d TO %dx%d", Args->Column, Args->Row, Args->EndColumn, Args->EndRow);

   if (Self->AmtLines < 1) {
      LogMsg("There is no selectable data present.");
      return ERR_Okay;
   }

   if (Args->Row < Self->AmtLines) Self->SelectRow = Args->Row;
   else Self->SelectRow = Self->AmtLines - 1;

   if (Args->Column < Self->Array[Self->SelectRow].Length) Self->SelectColumn = Args->Column;
   else Self->SelectColumn = Self->Array[Self->SelectRow].Length;

   if (Args->EndRow < Self->AmtLines) Self->CursorRow = Args->EndRow;
   else Self->CursorRow = Self->AmtLines - 1;

   if (Args->EndColumn < Self->Array[Self->CursorRow].Length) Self->CursorColumn = Args->EndColumn;
   else Self->CursorColumn = Self->Array[Self->CursorRow].Length;

   if ((Self->SelectRow != Self->CursorRow) OR (Self->SelectColumn != Self->CursorColumn)) {
      Self->Flags |= TXF_AREA_SELECTED;
   }
   else {
      LogMsg("No text was selected.");
      Self->Flags &= ~TXF_AREA_SELECTED;
   }

   draw_lines(Self, Self->SelectRow, Self->CursorRow - Self->SelectRow + 1);
   view_selection(Self);

   return ERR_Okay;
}

/*****************************************************************************

-METHOD-
SetFont: Makes changes to the font face, style and size after initialisation.

Call SetFont() to change the font face that is used for displaying text.  The string format follows the standard for
font requests, e.g. `Open Sans:12:Bold Italic:#ff0000`.  Refer to the @Font.Face field for more details.

If the new face is invalid or fails to load, the current font will remain unchanged.

-INPUT-
cstr Face: The name and specifications for the font face.

-ERRORS-
Okay
NullArgs
CreateObject
-END-

*****************************************************************************/

static ERROR TEXT_SetFont(objText *Self, struct txtSetFont *Args)
{
   if ((!Args) OR (!Args->Face)) return PostError(ERR_NullArgs);

   objFont *font;
   if (!CreateObject(ID_FONT, NF_INTEGRAL, &font,
         FID_Face|TSTR, Args->Face,
         TAGEND)) {

      if (Self->Font) acFree(Self->Font);

      Self->Font = font;

      // Recalculate the pixel width of each line

      LONG i;
      for (i=0; i < Self->AmtLines; i++) {
         Self->Array[i].PixelLength = calc_width(Self, Self->Array[i].String, Self->Array[i].Length);
      }

      Self->CursorRow    = 0;
      Self->CursorColumn = 0;
      Self->SelectRow    = 0;
      Self->SelectColumn = 0;
      Self->XPosition    = 0;
      Self->YPosition    = 0;
      Self->Flags &= ~TXF_AREA_SELECTED;

      Redraw(Self);
      calc_hscroll(Self);
      calc_vscroll(Self);

      return ERR_Okay;
   }
   else return ERR_CreateObject;
}

//****************************************************************************

static ERROR TEXT_Show(objText *Self, APTR Void)
{
   return acShow(Self->Layout);
}

//****************************************************************************

static ERROR cursor_timer(objText *Self, LARGE Elapsed, LARGE CurrentTime)
{
   if (Self->Flags & TXF_EDIT) {
      UBYTE one = (Self->CursorFlash % CURSOR_RATE) < (CURSOR_RATE>>1);
      Self->CursorFlash += Elapsed / 1000;
      UBYTE two = (Self->CursorFlash % CURSOR_RATE) < (CURSOR_RATE>>1);

      if (Self->LineLimit IS 1) view_cursor(Self);
      if (one != two) redraw_cursor(Self, TRUE);
   }

   return ERR_Okay;
}

//****************************************************************************

#include "fields.c"
#include "functions.c"
#include "def.c"

static const struct FieldArray clFields[] = {
   { "Layout",          FDF_INTEGRAL|FDF_SYSTEM|FDF_R, 0,   NULL, NULL },
   { "Font",            FDF_INTEGRAL|FDF_R,   ID_FONT,   NULL, NULL },
   { "VScroll",         FDF_OBJECTID|FDF_RW,  ID_SCROLL, NULL, SET_VScroll },
   { "HScroll",         FDF_OBJECTID|FDF_RW,  ID_SCROLL, NULL, SET_HScroll },
   { "TabFocus",        FDF_OBJECTID|FDF_RW,  0,         NULL, NULL },
   { "Focus",           FDF_OBJECTID|FDF_RI,  0,         NULL, NULL },
   { "CursorColumn",    FDF_LONG|FDF_RW,      0,         NULL, SET_CursorColumn },
   { "CursorRow",       FDF_LONG|FDF_RW,      0,         NULL, SET_CursorRow },
   { "Flags",           FDF_LONGFLAGS|FDF_RI, (MAXINT)&clTextFlags, NULL, NULL },
   { "AmtLines",        FDF_LONG|FDF_R,       0,         NULL, NULL },
   { "SelectRow",       FDF_LONG|FDF_R,       0,         NULL, NULL },
   { "SelectColumn",    FDF_LONG|FDF_R,       0,         NULL, NULL },
   { "Frame",           FDF_LONG|FDF_RW,      0,         NULL, NULL },
   { "HistorySize",     FDF_LONG|FDF_RI,      0,         NULL, NULL },
   { "LineLimit",       FDF_LONG|FDF_RW,      0,         NULL, NULL },
   { "CharLimit",       FDF_LONG|FDF_RW,      0,         NULL, SET_CharLimit },
   { "Highlight",       FDF_RGB|FDF_RW,       0,         NULL, NULL },
   { "Background",      FDF_RGB|FDF_RW,       0,         NULL, NULL },
   { "CursorColour",    FDF_RGB|FDF_RW,       0,         NULL, NULL },
   // Virtual fields
   { "Activated",       FDF_FUNCTIONPTR|FDF_RW, 0, GET_Activated, SET_Activated },
   { "LayoutStyle",     FDF_POINTER|FDF_SYSTEM|FDF_W, 0, NULL, SET_LayoutStyle },
   { "Location",        FDF_STRING|FDF_RW,  0, GET_Location,   SET_Location },
   { "Origin",          FDF_STRING|FDF_RW,  0, GET_Location,   SET_Origin },
   { "Src",             FDF_SYNONYM|FDF_STRING|FDF_RW,  0, GET_Location,   SET_Location },
   { "String",          FDF_STRING|FDF_RW,  0, GET_String,     SET_String },
   { "TextHeight",      FDF_LONG|FDF_R,     0, GET_TextHeight, NULL },
   { "TextWidth",       FDF_LONG|FDF_R,     0, GET_TextWidth,  NULL },
   { "TextX",           FDF_LONG|FDF_RW,    0, GET_TextX,      SET_TextX },
   { "TextY",           FDF_LONG|FDF_RW,    0, GET_TextY,      SET_TextY },
   { "ValidateInput",   FDF_FUNCTIONPTR|FDF_RW,  0, GET_ValidateInput, SET_ValidateInput },
   { "Height",          FDF_VARIABLE|FDF_DOUBLE|FDF_PERCENTAGE|FDF_RW, 0, GET_Height,  SET_Height },
   { "Point",           FDF_VARIABLE|FDF_DOUBLE|FDF_PERCENTAGE|FDF_RW, 0, GET_Point,   SET_Point },
   { "Width",           FDF_VARIABLE|FDF_DOUBLE|FDF_PERCENTAGE|FDF_RW, 0, GET_Width,   SET_Width },
   END_FIELD
};
