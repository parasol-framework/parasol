/*****************************************************************************

The source code of the Parasol project is made publicly available under the
terms described in the LICENSE.TXT file that is distributed with this package.
Please refer to it for further information on licensing.

******************************************************************************

-CLASS-
Scintilla: Provides advanced text display and editing facilities.

The Scintilla class provides advanced text editing capabilities that are suitable for modifying text files of any kind,
as well as simple user input features for text input boxes.  The code is based on the Scintilla project at
http://scintilla.org and it may be useful to study the official Scintilla documentation for further insight into its
capabilities.
-END-

*****************************************************************************/

//#define DEBUG
#define NO_SSTRING
#define INTERNATIONAL_INPUT
#define PRV_SCINTILLA

#define DBGDRAW //FMSG

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <assert.h>
#include <limits.h>
#include <string>
#include <vector>
#include <map>

#include "Platform.h"

#include "ILexer.h"
#include "Scintilla.h"

#include "PropSetSimple.h"
#ifdef SCI_LEXER
#include "SciLexer.h"
#include "LexerModule.h"
#include "Catalogue.h"
#endif
#include "SplitVector.h"
#include "Partitioning.h"
#include "RunStyles.h"
#include "ContractionState.h"
#include "CellBuffer.h"
#include "CallTip.h"
#include "KeyMap.h"
#include "Indicator.h"
#include "XPM.h"
#include "LineMarker.h"
#include "Style.h"
#include "ViewStyle.h"
#include "AutoComplete.h"
#include "CharClassify.h"
#include "Decoration.h"
#include "Document.h"
#include "Selection.h"
#include "PositionCache.h"
#include "Editor.h"
#include "ScintillaBase.h"


#include "Platform.h"
#include "Scintilla.h"
#include "ScintillaWidget.h"
#include "Partitioning.h"
#include "RunStyles.h"
#ifdef SCI_LEXER
#include "SciLexer.h"
#include "PropSetSimple.h"
//#include "Accessor.h"
//#include "KeyWords.h"
#endif
#include "CharClassify.h"
#include "ContractionState.h"
#include "SVector.h"
#include "CellBuffer.h"
#include "CallTip.h"
#include "KeyMap.h"
#include "Indicator.h"
#include "XPM.h"
#include "LineMarker.h"
#include "Style.h"
#include "AutoComplete.h"
#include "ViewStyle.h"
#include "Document.h"
#include "Editor.h"
#include "ScintillaBase.h"
#include "UniConversion.h"

#include <parasol/main.h>
#include <parasol/modules/xml.h>
#include <parasol/modules/font.h>
#include <parasol/modules/widget.h>
#include <parasol/modules/display.h>
#include <parasol/modules/surface.h>
#include <parasol/modules/font.h>

#include "scintillapan.h"

#include <parasol/modules/scintilla.h>
#include "module_def.c"

MODULE_COREBASE;
static struct SurfaceBase *SurfaceBase;
static struct DisplayBase *DisplayBase;
static struct FontBase *FontBase;

static OBJECTPTR clScintilla = NULL;
static OBJECTPTR modSurface = NULL, modDisplay = NULL;
static OBJECTPTR modFont = NULL;
//static OBJECTID glInputID = 0;
static struct RGB8 glHighlight = { 220, 220, 255 };
extern OBJECTPTR clScintillaSearch;

struct styledef {
   UBYTE Index;
   ULONG Colour;
   ULONG FontStyle;
};

// This is bad - the fonts should be stored in the class.

static OBJECTPTR glFont = NULL, glBoldFont = NULL, glItalicFont = NULL, glBIFont = NULL;

static const struct {
   CSTRING File;
   LONG Lexer;
} glLexers[] = {
   { "*.asm|*.s",     SCLEX_ASM },
   { "*.asp",         SCLEX_ASP },
   { "*.bash",        SCLEX_BASH },
   { "*.bat|*.dos",   SCLEX_BATCH },
   { "*.c|*.cpp|*.cxx|*.h|*.hpp", SCLEX_CPP },
   { "*.css",         SCLEX_CSS },
   { "*.diff",        SCLEX_DIFF },
   { "*.errorlist",   SCLEX_ERRORLIST },
   { "*.lua|*.fluid", SCLEX_FLUID },
   { "*.dmd",         SCLEX_HTML },
   { "*.dml",         SCLEX_XML },
   { "*.html",        SCLEX_HTML },
   { "*.latex",       SCLEX_LATEX },
   { "makefile|*.make", SCLEX_MAKEFILE },
   { "*.pas",          SCLEX_PASCAL },
   { "*.perl|*.pl",    SCLEX_PERL },
   { "*.prop|*.cfg",   SCLEX_PROPERTIES },
   { "*.py",           SCLEX_PYTHON },
   { "*.ruby|*.rb",    SCLEX_RUBY },
   { "*.sql",          SCLEX_SQL },
   { "*.vb",           SCLEX_VB },
   { "*.vbscript",     SCLEX_VBSCRIPT },
   { "*.xml",          SCLEX_XML },
   { NULL, 0 }
};

#define SCICOLOUR(red,green,blue) (((UBYTE)(blue))<<16)|(((UBYTE)(green))<<8)|((UBYTE)(red))
#define SCIRED(c)   (UBYTE)(c)
#define SCIGREEN(c) (UBYTE)(c>>8)
#define SCIBLUE(c)  (UBYTE)(c>>16)
#define SCICALL     Self->SciPan->SendScintilla

//****************************************************************************
// Scintilla class definition.

static ERROR GET_AllowTabs(objScintilla *, LONG *);
static ERROR GET_AutoIndent(objScintilla *, LONG *);
static ERROR GET_FileDrop(objScintilla *, FUNCTION **);
static ERROR GET_FoldingMarkers(objScintilla *, LONG *);
static ERROR GET_LineCount(objScintilla *, LONG *);
static ERROR GET_LineNumbers(objScintilla *, LONG *);
static ERROR GET_Path(objScintilla *, CSTRING *);
static ERROR GET_ShowWhitespace(objScintilla *, LONG *);
static ERROR GET_EventCallback(objScintilla *, FUNCTION **);
static ERROR GET_String(objScintilla *, STRING *);
static ERROR GET_Symbols(objScintilla *, LONG *);
static ERROR GET_TabWidth(objScintilla *, LONG *);
static ERROR GET_Wordwrap(objScintilla *, LONG *);

static ERROR SET_AllowTabs(objScintilla *, LONG);
static ERROR SET_AutoIndent(objScintilla *, LONG);
static ERROR SET_BkgdColour(objScintilla *, struct RGB8 *);
static ERROR SET_CursorColour(objScintilla *, struct RGB8 *);
static ERROR SET_FileDrop(objScintilla *, FUNCTION *);
static ERROR SET_FoldingMarkers(objScintilla *, LONG);
static ERROR SET_HScroll(objScintilla *, OBJECTID);
static ERROR SET_LeftMargin(objScintilla *, LONG);
static ERROR SET_Lexer(objScintilla *, LONG);
static ERROR SET_LineHighlight(objScintilla *, struct RGB8 *);
static ERROR SET_LineNumbers(objScintilla *, LONG);
static ERROR SET_Path(objScintilla *, CSTRING);
static ERROR SET_Modified(objScintilla *, LONG);
static ERROR SET_Origin(objScintilla *, CSTRING);
static ERROR SET_RightMargin(objScintilla *, LONG);
static ERROR SET_ShowWhitespace(objScintilla *, LONG);
static ERROR SET_EventCallback(objScintilla *, FUNCTION *);
static ERROR SET_SelectBkgd(objScintilla *, struct RGB8 *);
static ERROR SET_SelectFore(objScintilla *, struct RGB8 *);
static ERROR SET_String(objScintilla *, CSTRING);
static ERROR SET_Symbols(objScintilla *, LONG);
static ERROR SET_TabWidth(objScintilla *, LONG);
static ERROR SET_TextColour(objScintilla *, struct RGB8 *);
static ERROR SET_VScroll(objScintilla *, OBJECTID);
static ERROR SET_Wordwrap(objScintilla *, LONG);

//****************************************************************************

static void create_styled_fonts(objScintilla *);
static ERROR create_scintilla(void);
static void draw_scintilla(objScintilla *, objSurface *, struct rkBitmap *);
static ERROR load_file(objScintilla *, CSTRING);
static void calc_longest_line(objScintilla *);
static void key_event(objScintilla *, evKey *, LONG);
static void report_event(objScintilla *, LARGE Event);
static ERROR idle_timer(objScintilla *Self, LARGE Elapsed, LARGE CurrentTime);
extern ERROR init_search(void);

//****************************************************************************

ERROR CMDInit(OBJECTPTR argModule, struct CoreBase *argCoreBase)
{
   CoreBase = argCoreBase;

   if (LoadModule((STRING)"surface", MODVERSION_SURFACE, &modSurface, &SurfaceBase) != ERR_Okay) return ERR_InitModule;
   if (LoadModule((STRING)"display", MODVERSION_DISPLAY, &modDisplay, &DisplayBase) != ERR_Okay) return ERR_InitModule;
   if (LoadModule((STRING)"font", MODVERSION_FONT, &modFont, &FontBase) != ERR_Okay) return ERR_InitModule;

   objXML *style;
   if (!FindPrivateObject("glStyle", &style)) {
      BYTE buffer[40];
      if (!acGetVar(style, "/colours/@texthighlight", buffer, sizeof(buffer))) {
         StrToColour(buffer, &glHighlight);
      }
   }

   if (!init_search()) {
      return create_scintilla();
   }
   else return ERR_AddClass;
}

//****************************************************************************

ERROR CMDExpunge(void)
{
   if (modDisplay)  { acFree(modDisplay);  modDisplay = NULL; }
   if (modFont)     { acFree(modFont);     modFont = NULL; }
   if (modSurface)  { acFree(modSurface);  modSurface  = NULL; }
   if (clScintilla) { acFree(clScintilla); clScintilla = NULL; }
   if (clScintillaSearch) { acFree(clScintillaSearch); clScintillaSearch = NULL; }
   return ERR_Okay;
}

//*****************************************************************************

static ERROR SCINTILLA_ActionNotify(objScintilla *Self, struct acActionNotify *Args)
{
   if (!Args) return ERR_NullArgs;

   MSG("Action: %d, ErrorCode: %d", Args->ActionID, Args->Error);

   if (Args->Error != ERR_Okay) {
      if (Args->ActionID IS AC_Write) {
         if (Self->FileStream) { acFree(Self->FileStream); Self->FileStream = NULL; }
      }
      return ERR_Okay;
   }

   if (Args->ActionID IS AC_DragDrop) {
      // There are two drag-drop cases - DATA_TEXT and DATA_FILE.  DATA_TEXT is something that we can handle ourselves,
      // while DATA_FILE is handled via an external function provided by the user.  Refer to the DataFeed action for
      // further code.

      struct acDragDrop *drag = (struct acDragDrop *)Args->Args;
      if (!drag) return PostError(ERR_NullArgs);

      // Send the source an item request

      struct dcRequest request;
      request.Item          = drag->Item;
      request.Preference[0] = DATA_FILE;
      request.Preference[1] = DATA_TEXT;
      request.Preference[2] = 0;

      struct acDataFeed dc;
      dc.ObjectID = Self->Head.UniqueID;
      dc.Datatype = DATA_REQUEST;
      dc.Buffer   = &request;
      dc.Size     = sizeof(request);
      if (!ActionMsg(AC_DataFeed, drag->SourceID, &dc)) {
         // The source will return a DATA_RECEIPT for the items that we've asked for (see the DataFeed action).
      }
   }
   else if (Args->ActionID IS AC_Focus) {
      if (!Self->prvKeyEvent) {
         FUNCTION callback;
         SET_FUNCTION_STDC(callback, (APTR)&key_event);
         SubscribeEvent(EVID_IO_KEYBOARD_KEYPRESS, &callback, Self, &Self->prvKeyEvent);
      }

      if ((Self->Visible) AND (!(Self->Flags & SCF_DISABLED))) {
         Self->SciPan->panGotFocus();
      }
      else LogMsg("(Focus) Cannot receive focus, surface not visible or disabled.");
   }
   else if (Args->ActionID IS AC_Free) {
      if ((Self->EventCallback.Type IS CALL_SCRIPT) AND (Self->EventCallback.Script.Script->UniqueID IS Args->ObjectID)) {
         Self->EventCallback.Type = CALL_NONE;
      }
   }
   else if (Args->ActionID IS AC_Hide) {
      // Parent surface has been hidden
      acHide(Self);
   }
   else if (Args->ActionID IS AC_LostFocus) {
      LogBranch("LostFocus");

      if (Self->prvKeyEvent) { UnsubscribeEvent(Self->prvKeyEvent); Self->prvKeyEvent = NULL; }

      Self->SciPan->panLostFocus();

      LogBack();
   }
   else if (Args->ActionID IS AC_Show) {
      // Parent surface now visible
      acShow(Self);
   }
   else if (Args->ActionID IS AC_Redimension) {
      struct acRedimension *resize;
      BYTE resized;

      if (!(resize = (struct acRedimension *)Args->Args)) return ERR_Okay;

      if ((Self->Surface.Width != F2T(resize->Width)) OR (Self->Surface.Height != F2T(resize->Height))) resized = TRUE;
      else resized = FALSE;

      Self->Surface.X = F2T(resize->X);
      Self->Surface.Y = F2T(resize->Y);
      Self->Surface.Width  = F2T(resize->Width);
      Self->Surface.Height = F2T(resize->Height);

      if (resized) Self->SciPan->panResized();
   }
   else if (Args->ActionID IS AC_Write) {
      struct acWrite *write;

      if (!(write = (struct acWrite *)Args->Args)) return ERR_Okay;

      LogMsg("%d bytes incoming from file stream.", write->Result);

      Self->HoldModify = TRUE; // Prevent the file from being marked as modified due to incoming data

      SCICALL(SCI_SETUNDOCOLLECTION, 0UL); // Turn off undo

      if (write->Buffer) {
         acDataFeed(Self, Self->Head.UniqueID, DATA_TEXT, write->Buffer, write->Result);
      }
      else { // We have to read the data from the file stream
      }

      SCICALL(SCI_SETUNDOCOLLECTION, 1UL); // Turn on undo

      Self->HoldModify = FALSE;
   }

   return ERR_Okay;
}

/*****************************************************************************
-ACTION-
Clear: Clears all content from the editor.

******************************************************************************/

static ERROR SCINTILLA_Clear(objScintilla *Self, APTR Void)
{
   LogBranch(NULL);

   SCICALL(SCI_BEGINUNDOACTION);
   SCICALL(SCI_CLEARALL);
   SCICALL(SCI_ENDUNDOACTION);

   LogBack();
   return ERR_Okay;
}

/*****************************************************************************
-ACTION-
Clipboard: Full support for clipboard activity is provided through this action.

*****************************************************************************/

static ERROR SCINTILLA_Clipboard(objScintilla *Self, struct acClipboard *Args)
{
   if ((!Args) OR (!Args->Mode)) {
      return PostError(ERR_NullArgs);
   }

   if (Args->Mode IS CLIPMODE_CUT) {
      Self->SciPan->Cut();
      return ERR_Okay;
   }
   else if (Args->Mode IS CLIPMODE_COPY) {
      Self->SciPan->Copy();
      return ERR_Okay;
   }
   else if (Args->Mode IS CLIPMODE_PASTE) {
      Self->SciPan->Paste();
      return ERR_Okay;
   }
   else return PostError(ERR_Args);
}

//*****************************************************************************

static ERROR SCINTILLA_DataFeed(objScintilla *Self, struct acDataFeed *Args)
{
   LONG i, keyval, total;

   if (!Args) return PostError(ERR_NullArgs);

   if (Args->DataType IS DATA_TEXT) {
      STRING str;

      // Incoming text is appended to the end of the document

      if (!Args->Buffer) str = "";
      else str = (STRING)Args->Buffer;

      SCICALL(SCI_APPENDTEXT, StrLength(str), str);
   }
   else if (Args->DataType IS DATA_INPUT_READY) {
      struct InputMsg *input;

      while (!gfxGetInputMsg((struct dcInputReady *)Args->Buffer, 0, &input)) {
         if (Self->Flags & SCF_DISABLED) continue;

         if (input->Flags & JTYPE_BUTTON) {
            if (input->Value > 0) {
               Self->SciPan->panMousePress(input->Type, input->X, input->Y);
            }
            else Self->SciPan->panMouseRelease(input->Type, input->X, input->Y);
         }
         else if (input->Flags & JTYPE_MOVEMENT) {
            Self->SciPan->panMouseMove(input->X, input->Y);
         }
      }
   }
   else if (Args->Datatype IS DATA_RECEIPT) {
      LONG i, count;
      objXML *xml;
      struct XMLTag *tag;
      STRING path;

      LogMsg("Received item receipt from object %d.", Args->ObjectID);

      if (!CreateObject(ID_XML, NF_INTEGRAL, &xml,
            FID_Statement|TSTRING, Args->Buffer,
            TAGEND)) {

         count = 0;
         for (i=0; i < xml->TagCount; i++) {
            tag = xml->Tags[i];
            if (!StrMatch("file", tag->Attrib->Name)) {
               // If the file is being dragged within the same device, it will be moved instead of copied.

               if ((path = XMLATTRIB(tag, "path"))) {
                  if (Self->FileDrop.Type) {
                     if (Self->FileDrop.Type IS CALL_STDC) {
                        void (*routine)(objScintilla *, STRING);
                        OBJECTPTR context;

                        context = SetContext(Self->FileDrop.StdC.Context);
                           routine = (void (*)(objScintilla *, STRING))Self->FileDrop.StdC.Routine;
                           routine(Self, path);
                        SetContext(context);
                     }
                     else if (Self->FileDrop.Type IS CALL_SCRIPT) {
                        struct scCallback exec;
                        OBJECTPTR script;
                        struct ScriptArg args[] = {
                           { "Scintilla", FD_OBJECTPTR },
                           { "Path",      FD_STR }
                        };

                        args[0].Address = Self;
                        args[1].Address = path;

                        exec.ProcedureID = Self->FileDrop.Script.ProcedureID;
                        exec.Args      = args;
                        exec.TotalArgs = ARRAYSIZE(args);
                        if ((script = Self->FileDrop.Script.Script)) {
                           Action(MT_ScCallback, script, &exec);
                        }
                     }
                  }
               }
            }
            else if (!StrMatch("text", tag->Attrib->Name)) {
               struct sciInsertText insert;

               if ((tag->Child) AND (!tag->Child->Attrib->Name)) {
                  insert.String = tag->Child->Attrib->Value;
                  insert.Pos    = -1;
                  Action(MT_SciInsertText, Self, &insert);
               }
            }
         }

         acFree(xml);

         return ERR_Okay;
      }
      else return PostError(ERR_CreateObject);
   }

   return ERR_Okay;
}

/*****************************************************************************

-METHOD-
DeleteLine: Deletes a line.

This method will delete a single line at the position indicated by the Line parameter.  If no parameters are provided
or the Line parameter is less than zero, the line at the current cursor position will be deleted.  If the index
exceeds the total number of available lines, the last available line will be targeted.

-INPUT-
int Line: The number of the line to delete.  Indexing starts from 0.

-RESULT-
Okay

*****************************************************************************/

static ERROR SCINTILLA_DeleteLine(objScintilla *Self, struct sciDeleteLine *Args)
{
   LONG line, pos, start, end, linecount;

   linecount = SCICALL(SCI_GETLINECOUNT);

   if ((!Args) OR (Args->Line < 0)) {
      pos = SCICALL(SCI_GETCURRENTPOS);
      line = SCICALL(SCI_LINEFROMPOSITION, pos);
   }
   else line = Args->Line;

   FMSG("~","Line: %d", line);

   // Set the start and end markers.  Some adjustments may be necessary if this is the last line in the document.

   start = SCICALL(SCI_POSITIONFROMLINE, line);
   end = start + SCICALL(SCI_LINELENGTH, line);

   if (line+1 IS linecount) {
      if (line > 0) {
         start = SCICALL(SCI_POSITIONFROMLINE, line-1) + SCICALL(SCI_LINELENGTH, line-1) - 1;
      }

      SCICALL(SCI_GOTOLINE, line-1);
   }

   SCICALL(SCI_SETTARGETSTART, start);
   SCICALL(SCI_SETTARGETEND, end);

   // Delete the targeted text

   SCICALL(SCI_REPLACETARGET, 0UL, "");

   STEP();
   return ERR_Okay;
}

/*****************************************************************************
-ACTION-
Disable: Disables the target #Surface.

*****************************************************************************/

static ERROR SCINTILLA_Disable(objScintilla *Self, APTR Void)
{
   Self->Flags |= SCF_DISABLED;
   DelayMsg(AC_Draw, Self->SurfaceID, NULL);
   return ERR_Okay;
}

/*****************************************************************************
-ACTION-
Draw: Draws the Scintilla object's graphics.

*****************************************************************************/

static ERROR SCINTILLA_Draw(objScintilla *Self, struct acDraw *Args)
{
   ActionMsg(AC_Draw, Self->SurfaceID, Args);
   return ERR_Okay;
}

/*****************************************************************************
-ACTION-
Enable: Enables the target #Surface.

*****************************************************************************/

static ERROR SCINTILLA_Enable(objScintilla *Self, APTR Void)
{
   Self->Flags &= ~SCF_DISABLED;
   DelayMsg(AC_Draw, Self->SurfaceID, NULL);
   return ERR_Okay;
}

/*****************************************************************************
-ACTION-
Focus: Focus on the Scintilla surface.
-END-
*****************************************************************************/

static ERROR SCINTILLA_Focus(objScintilla *Self, APTR Void)
{
   return acFocusID(Self->SurfaceID);
}

//*****************************************************************************

static ERROR SCINTILLA_Free(objScintilla *Self, APTR)
{
   OBJECTPTR object;

   delete Self->SciPan;
   Self->SciPan = NULL;

   if (Self->TimerID) { UpdateTimer(Self->TimerID, 0); Self->TimerID = 0; }

   if ((Self->FocusID) AND (Self->FocusID != Self->SurfaceID)) {
      if (!AccessObject(Self->FocusID, 500, &object)) {
         UnsubscribeAction(object, NULL);
         UnsubscribeFeed(object);
         ReleaseObject(object);
      }
   }

   if (Self->SurfaceID) {
      if (!AccessObject(Self->SurfaceID, 500, &object)) {
         drwRemoveCallback(object, (APTR)&draw_scintilla);
         UnsubscribeAction(object, NULL);
         UnsubscribeFeed(object);
         ReleaseObject(object);
      }
   }

   /*if (Self->PointerLocked) {
      RestoreCursor(PTR_DEFAULT, Self->Head.UniqueID);
      Self->PointerLocked = FALSE;
   }*/

   if (Self->prvKeyEvent)  { UnsubscribeEvent(Self->prvKeyEvent); Self->prvKeyEvent = NULL; }
   if (Self->FileStream)   { acFree(Self->FileStream); Self->FileStream = NULL; }
   if (Self->Path)         { FreeMemory(Self->Path);  Self->Path = NULL; }
   if (Self->StringBuffer) { FreeMemory(Self->StringBuffer); Self->StringBuffer = NULL; }
   if (Self->Font)         { acFree(Self->Font);       Self->Font = NULL; }
   if (Self->BoldFont)     { acFree(Self->BoldFont);   Self->BoldFont = NULL; }
   if (Self->ItalicFont)   { acFree(Self->ItalicFont); Self->ItalicFont = NULL; }
   if (Self->BIFont)       { acFree(Self->BIFont);     Self->BIFont = NULL; }

   gfxUnsubscribeInput(0);

   return ERR_Okay;
}

/*****************************************************************************

-METHOD-
GetLine: Copies the text content of any line to a user-supplied buffer.

This method will retrieve the string for a line at a given index.  The string is copied to a user supplied buffer of
the indicated length (in bytes).

-INPUT-
int Line: The index of the line to retrieve.
buf(str) Buffer: The destination buffer.
bufsize Length: The byte size of the buffer.

-RESULT-
Okay:
NullArgs:
OutOfRange: At least one of the parameters is out of range.
BufferOverflow: The supplied buffer is not large enough to contain the

*****************************************************************************/

static ERROR SCINTILLA_GetLine(objScintilla *Self, struct sciGetLine *Args)
{
   if ((!Args) OR (!Args->Buffer)) return PostError(ERR_NullArgs);
   if ((Args->Line < 0) OR (Args->Length < 1)) return PostError(ERR_OutOfRange);

   LONG len = SCICALL(SCI_LINELENGTH, Args->Line); // Returns the length of the line (in bytes) including line-end characters (NB: there could be more than one line-end character!)
   if (Args->Length > len) {
      SCICALL(SCI_GETLINE, Args->Line, Args->Buffer);
      Args->Buffer[len] = 0;
      return ERR_Okay;
   }
   else return ERR_BufferOverflow;
}

/*****************************************************************************

-METHOD-
GetPos: Returns the byte position of a given line and column number.

This method converts a line and column index to the equivalent byte position within the text document.

-INPUT-
int Line: Line index
int Column: Column index
&int Pos: The byte position is returned in this parameter.

-RESULT-
Okay:
NullArgs:

*****************************************************************************/

static ERROR SCINTILLA_GetPos(objScintilla *Self, struct sciGetPos *Args)
{
   if (!Args) return PostError(ERR_NullArgs);

   Args->Pos = SCICALL(SCI_FINDCOLUMN, Args->Line, Args->Column);
   return ERR_Okay;
}

/*****************************************************************************

-METHOD-
GotoLine: Moves the cursor to any line in the document.

This method moves the cursor to a given line index.  If the index is greater than the total number of available lines,
the cursor is moved to the last line in the document.  A line index of less than zero is invalid.

-INPUT-
int Line: The line index to move to.

-RESULT-
Okay:
NullArgs:
OutOfRange: The Line is less than zero.

*****************************************************************************/

static ERROR SCINTILLA_GotoLine(objScintilla *Self, struct sciGotoLine *Args)
{
   if (!Args) return PostError(ERR_NullArgs);
   if (Args->Line < 0) return ERR_OutOfRange;

   LogBranch("Line: %d", Args->Line);
   SCICALL(SCI_GOTOLINE, Args->Line);
   LogBack();
   return ERR_Okay;
}

//*****************************************************************************

static ERROR SCINTILLA_Hide(objScintilla *Self, APTR Void)
{
   if (Self->Visible) {
      LogBranch(NULL);

         if (Self->VScrollbar) {
            SetLong(Self->VScrollbar, FID_Hide, TRUE);
            acHide(Self->VScrollbar);
         }

         if (Self->HScrollbar) {
            SetLong(Self->HScrollbar, FID_Hide, TRUE);
            acHide(Self->HScrollbar);
         }

         Self->Visible = FALSE;
         acDraw(Self);

      LogBack();
   }

   return ERR_Okay;
}

//*****************************************************************************

static ERROR SCINTILLA_Init(objScintilla *Self, APTR)
{
   if (!Self->SurfaceID) return PostError(ERR_UnsupportedOwner);

   if (!Self->FocusID) Self->FocusID = Self->SurfaceID;

   // Subscribe to the object responsible for the user focus

   OBJECTPTR object;
   if (!AccessObject(Self->FocusID, 5000, &object)) {
      SubscribeActionTags(object,
         AC_Focus,
         AC_LostFocus,
         TAGEND);
      ReleaseObject(object);
   }

   // Set up the target surface

   MSG("Configure target surface #%d", Self->SurfaceID);

   objSurface *surface;
   if (!AccessObject(Self->SurfaceID, 3000, &surface)) {
      SetLong(surface, FID_Flags, surface->Flags|RNF_GRAB_FOCUS);

      Self->Surface.X = surface->X;
      Self->Surface.Y = surface->Y;
      Self->Surface.Width  = surface->Width;
      Self->Surface.Height = surface->Height;

      drwAddCallback(surface, (APTR)&draw_scintilla);

      SubscribeFeed(surface);

      SubscribeActionTags(surface,
         AC_DataFeed,
         AC_DragDrop,
         AC_Disable,
         AC_Enable,
         AC_Hide,
         AC_Redimension,
         AC_Show,
         TAGEND);

      if (surface->Flags & RNF_HAS_FOCUS) {
         FUNCTION callback;
         SET_FUNCTION_STDC(callback, (APTR)&key_event);
         SubscribeEvent(EVID_IO_KEYBOARD_KEYPRESS, &callback, Self, &Self->prvKeyEvent);
      }

      ReleaseObject(surface);
   }
   else return PostError(ERR_AccessObject);

   gfxSubscribeInput(Self->SurfaceID, JTYPE_MOVEMENT|JTYPE_BUTTON, 0);

   // Generate scrollbars if they haven't been provided

   AdjustLogLevel(2);

   if (!Self->VScrollID) {
      if (!CreateObject(ID_SCROLLBAR, 0, &Self->VScrollbar,
            FID_Name|TSTRING,      "page_vscroll",
            FID_Surface|TLONG,     (Self->ScrollTargetID) ? Self->ScrollTargetID : Self->SurfaceID,
            FID_Direction|TSTRING, "VERTICAL",
            FID_Monitor|TLONG,     Self->SurfaceID,
            TAGEND)) {
         SET_VScroll(Self, Self->VScrollbar->Head.UniqueID);
      }
   }

   if (!Self->HScrollID) {
      if (!CreateObject(ID_SCROLLBAR, 0, &Self->HScrollbar,
            FID_Name|TSTRING,      "page_hscroll",
            FID_Surface|TLONG,     (Self->ScrollTargetID) ? Self->ScrollTargetID : Self->SurfaceID,
            FID_Direction|TSTRING, "HORIZONTAL",
            FID_Monitor|TLONG,     Self->SurfaceID,
            FID_Intersect|TLONG,   Self->VScrollID,
            TAGEND)) {
         SET_HScroll(Self, Self->HScrollbar->Head.UniqueID);
      }
   }

   AdjustLogLevel(-2);

   if (acInit(Self->Font) != ERR_Okay) return ERR_Init;

   create_styled_fonts(Self);

   // Create a Scintilla class object, passing it the target surface and a pointer to our own structure to link us
   // together.

   if (!(Self->SciPan = new ScintillaPan(Self->SurfaceID, Self))) {
      return ERR_Failed;
   }

   Self->SciPan->panFontChanged(Self->Font, Self->BoldFont, Self->ItalicFont, Self->BIFont);

   // Load a text file if required

   if (Self->Path) {
      if (load_file(Self, Self->Path) != ERR_Okay) {
         return ERR_File;
      }
   }
   else calc_longest_line(Self);

   FUNCTION callback;
   SET_FUNCTION_STDC(callback, (APTR)&idle_timer);
   SubscribeTimer(0.03, &callback, &Self->TimerID);

   if (Self->Visible IS -1) Self->Visible = TRUE;

   if ((!(Self->Flags & SCF_DETECT_LEXER)) AND (Self->Lexer)) {
      Self->SciPan->SetLexer(Self->Lexer);
   }

   DelayMsg(AC_Draw, Self->SurfaceID, NULL);

   if (Self->LongestWidth) SCICALL(SCI_SETSCROLLWIDTH, Self->LongestWidth);
   else SCICALL(SCI_SETSCROLLWIDTH, 1UL);

   if (Self->Flags & SCF_EXT_PAGE) {
      LogMsg("Extended page mode.");
      SCICALL(SCI_SETENDATLASTLINE, 0UL); // Allow scrolling by an extra page at the end of the document
   }
   else SCICALL(SCI_SETENDATLASTLINE, 1UL);

   SCICALL(SCI_SETMARGINLEFT, 0, Self->LeftMargin);
   SCICALL(SCI_SETMARGINRIGHT, 0, 0L);

   SCICALL(SCI_SETTABWIDTH, Self->TabWidth);

   // Selected text will be inversed with these colours

   SCICALL(SCI_SETSELFORE, (unsigned long int)true, (long int)SCICOLOUR(Self->SelectFore.Red, Self->SelectFore.Green, Self->SelectFore.Blue));
   SCICALL(SCI_SETSELBACK, (unsigned long int)true, (long int)SCICOLOUR(Self->SelectBkgd.Red, Self->SelectBkgd.Green, Self->SelectBkgd.Blue));
   //SCICALL(SCI_SETSELALPHA, Self->SelectBkgd.Alpha); // Currently doesn't work as expected

   // Enable line colour for the line that contains the text cursor

   SCICALL(SCI_SETCARETLINEBACK, SCICOLOUR(Self->LineHighlight.Red, Self->LineHighlight.Green, Self->LineHighlight.Blue));
   if (Self->LineHighlight.Alpha > 0) {
      SCICALL(SCI_SETCARETLINEVISIBLE, 1UL);
      //SCICALL(SCI_SETCARETLINEBACKALPHA, Self->LineHighlight.Alpha); // Not working currently - maybe a drawing issue?
   }
   else SCICALL(SCI_SETCARETLINEVISIBLE, 0UL);

   SCICALL(SCI_SETCARETFORE, SCICOLOUR(Self->CursorColour.Red, Self->CursorColour.Green, Self->CursorColour.Blue));
   SCICALL(SCI_SETCARETWIDTH, 2);

#ifdef DEBUG
   SCICALL(SCI_SETCARETPERIOD, 0UL);
#endif

   // Show whitespace characters like tabs

   if (Self->ShowWhitespace) SCICALL(SCI_SETVIEWWS, (long unsigned int)SCWS_VISIBLEALWAYS);
   else SCICALL(SCI_SETVIEWWS, (long unsigned int)SCWS_INVISIBLE);

   // Turn off buffered drawing because Parasol surfaces already include buffer support

   SCICALL(SCI_SETBUFFEREDDRAW, 0UL);

   // Caret visibility policy

   SCICALL(SCI_SETYCARETPOLICY, CARET_SLOP|CARET_STRICT|CARET_EVEN, 3);
   SCICALL(SCI_SETXCARETPOLICY, CARET_SLOP|CARET_STRICT|CARET_EVEN, Self->RightMargin);

   // Caret visibility policy (folding margins)

   SCICALL(SCI_SETVISIBLEPOLICY, VISIBLE_STRICT|VISIBLE_SLOP, 4);

   // Miscellaneous options

   SCICALL(SCI_SETEOLMODE, SC_EOL_LF);
   SCICALL(SCI_SETPROPERTY, "fold","0");

   if (Self->AllowTabs) SCICALL(SCI_SETUSETABS, 1UL);
   else  SCICALL(SCI_SETUSETABS, 0UL);

   // Set all special margins to invisible (note that the values indicate the pixel width of the margin)

   if (Self->LineNumbers) SCICALL(SCI_SETMARGINWIDTHN, 0, 50L);
   else SCICALL(SCI_SETMARGINWIDTHN, 0, 0L);

   if (Self->Symbols) SCICALL(SCI_SETMARGINWIDTHN, 1, 20L);
   else SCICALL(SCI_SETMARGINWIDTHN, 1, 0L);

   if (Self->FoldingMarkers) SCICALL(SCI_SETMARGINWIDTHN, 2, 20L);
   else SCICALL(SCI_SETMARGINWIDTHN, 2, 0L);

   if (Self->Wordwrap) SCICALL(SCI_SETWRAPMODE, 1UL);
   else SCICALL(SCI_SETWRAPMODE, 0UL);

   return ERR_Okay;
}

/*****************************************************************************

-METHOD-
InsertText: Inserts text into a document.

Use InsertText to insert a string at any point in the document (if adding text to the end of the document, we recommend
using data channels instead).

You will need to specify the character position at which the provided String should be inserted.  Two special character
positions are also supported as an alternative - a value of -1 inserts the text at the current cursor position and a
value of -2 replaces currently selected text.

-INPUT-
cstr String: A text string to add.
int Pos: -1 inserts at the current cursor position, -2 replaces currently selected text, zero or above inserts at the character index indicated.

-RESULT-
Okay
NullArgs
OutOfRange

*****************************************************************************/

static ERROR SCINTILLA_InsertText(objScintilla *Self, struct sciInsertText *Args)
{
   LONG pos;

   if ((!Args) OR (!Args->String)) return PostError(ERR_NullArgs);

   LogBranch("Pos: %d, Text: %.10s", Args->Pos, Args->String);

   pos = Args->Pos;
   if (pos IS -1) {
      // Get the current cursor position
      pos = SCICALL(SCI_GETCURRENTPOS);
   }
   else if (pos IS -2) {
      // Replace currently selected text

      SCICALL(SCI_BEGINUNDOACTION);
      SCICALL(SCI_REPLACESEL, 0UL, Args->String);
      SCICALL(SCI_ENDUNDOACTION);

      LogBack();
      return ERR_Okay;
   }
   else if (pos < -1) {
      LogBack();
      return PostError(ERR_OutOfRange);
   }

   SCICALL(SCI_BEGINUNDOACTION);
   SCICALL(SCI_INSERTTEXT, pos, Args->String);
   SCICALL(SCI_ENDUNDOACTION);

   LogBack();
   return ERR_Okay;
}

//*****************************************************************************

static ERROR SCINTILLA_NewObject(objScintilla *Self, APTR)
{
   if (!NewObject(ID_FONT, NF_INTEGRAL, &Self->Font)) {
      SetString(Self->Font, FID_Face, "courier:10");
      Self->LeftMargin  = 4;
      Self->RightMargin = 30;
      Self->AutoIndent  = TRUE;
      Self->TabWidth    = 8;
      Self->AllowTabs   = FALSE;

      Self->BkgdColour.Red   = 255;
      Self->BkgdColour.Green = 255;
      Self->BkgdColour.Blue  = 255;
      Self->BkgdColour.Alpha = 255;

      Self->LineHighlight.Red   = 240;
      Self->LineHighlight.Green = 240;
      Self->LineHighlight.Blue  = 255;
      Self->LineHighlight.Alpha = 255;

      Self->CursorColour.Red   = 0;
      Self->CursorColour.Green = 0;
      Self->CursorColour.Blue  = 0;
      Self->CursorColour.Alpha = 255;

      Self->SelectFore.Red   = 255;
      Self->SelectFore.Green = 255;
      Self->SelectFore.Blue  = 255;
      Self->SelectFore.Alpha = 255;

      Self->SelectBkgd.Red   = 0;
      Self->SelectBkgd.Green = 0;
      Self->SelectBkgd.Blue  = 180;
      Self->SelectBkgd.Alpha = 255;
   }
   else return ERR_NewObject;

   return ERR_Okay;
}

//*****************************************************************************

static ERROR SCINTILLA_NewOwner(objScintilla *Self, struct acNewOwner *Args)
{
   if (!(Self->Head.Flags & NF_INITIALISED)) {
      OBJECTID owner_id = Args->NewOwnerID;
      while ((owner_id) AND (GetClassID(owner_id) != ID_SURFACE)) {
         owner_id = GetOwnerID(owner_id);
      }
      if (owner_id) Self->SurfaceID = owner_id;
   }

   return ERR_Okay;
}

/*****************************************************************************
-ACTION-
Redo: Redo the most recently undone activity.

*****************************************************************************/

static ERROR SCINTILLA_Redo(objScintilla *Self, struct acRedo *Args)
{
   LogBranch(NULL);

   SCICALL(SCI_REDO);

   LogBack();
   return ERR_Okay;
}

/*****************************************************************************

-METHOD-
ReplaceLine: Replaces a line with new text content.

This method will replace an entire line with a new text string.  If the new string contains line feeds, then multiple
lines will be inserted at the target position.  It is possible to limit the number of characters inserted from the
source string by setting the Length parameter.  To insert all characters from the source string, set a Length of -1.

-INPUT-
int Line: Index of the line being targeted.
cstr String: The new string that will replace the line.
int Length: The number of characters to replace the target with, or -1 for the entire source string.

-RESULT-
Okay:
NullArgs:
OutOfRange: The line index is less than zero or greater than the available number of lines.

*****************************************************************************/

static ERROR SCINTILLA_ReplaceLine(objScintilla *Self, struct sciReplaceLine *Args)
{
   if (!Args) return ERR_NullArgs;
   if (Args->Line < 0) return PostError(ERR_OutOfRange);

   // Select the line, then replace the text

   LONG start, end;
   if ((start = SCICALL(SCI_POSITIONFROMLINE, Args->Line)) < 0) return PostError(ERR_OutOfRange);
   if ((end = SCICALL(SCI_GETLINEENDPOSITION, Args->Line)) < 0) return PostError(ERR_OutOfRange);
   SCICALL(SCI_SETTARGETSTART, start);
   SCICALL(SCI_SETTARGETEND, end);

   // Replace the targeted text

   SCICALL(SCI_REPLACETARGET, Args->Length, Args->String);

   return ERR_Okay;
}

/*****************************************************************************

-METHOD-
ReplaceText: Replaces all text within an entire document or limited range.

The ReplaceText method will replace all instances of the Find string with the content of the Replace string, between a
given Start and End point.  The STF_CASE, STF_SCAN_SELECTION and STF_EXPRESSION are valid flag options for this method
(see FindText for details).

-INPUT-
cstr Find: The keyword string to find.
cstr Replace: The string that will replace the keyword.
int(STF) Flags: Optional flags.
int Start: The start of the search - set to zero if covering the entire document.  If -1, starts from the current cursor position.
int End: The end of the search - set to -1 if covering the entire document.

-RESULT-
Okay: At least one keyword was successfully replaced.
Args:
Search: The keyword could not be found.

*****************************************************************************/

static ERROR SCINTILLA_ReplaceText(objScintilla *Self, struct sciReplaceText *Args)
{
   LONG start, end, flags, pos;
   LONG len, targstart, targend, findlen, replacelen;

   if ((!Args) OR (!Args->Find) OR (!*Args->Find)) return PostError(ERR_NullArgs);

   LogBranch("Text: '%.10s'... Between: %d - %d, Flags: $%.8x", Args->Find, Args->Start, Args->End, Args->Flags);

   // Calculate the start and end positions

   if (Args->Flags & STF_SCAN_SELECTION) {
      start = SCICALL(SCI_GETSELECTIONSTART);
      end   = SCICALL(SCI_GETSELECTIONEND);
   }
   else {
      if (Args->Start < 0) start = SCICALL(SCI_GETCURRENTPOS);
      else start = Args->Start;

      if (Args->End < 0) end = SCICALL(SCI_GETLENGTH);
      else end = Args->End;

      if (start IS end) {
         LogBack();
         return ERR_Search;
      }
   }

   CSTRING replace;
   if (!Args->Replace) replace = "";
   else replace = Args->Replace;

   SCICALL(SCI_SETTARGETSTART, start);
   SCICALL(SCI_SETTARGETEND, end);

   findlen = StrLength(Args->Find);
   replacelen = StrLength(replace);

   flags = ((Args->Flags & STF_CASE) ? SCFIND_MATCHCASE : 0) |
           ((Args->Flags & STF_EXPRESSION) ? SCFIND_REGEXP : 0);

   SCICALL(SCI_SETSEARCHFLAGS, flags);

   SCICALL(SCI_BEGINUNDOACTION);

   pos = 0;
   while (pos != -1) {
      MSG("Search between %d - %d", start, end);

      SCICALL(SCI_SETTARGETSTART, start);
      SCICALL(SCI_SETTARGETEND, end);

      pos = SCICALL(SCI_SEARCHINTARGET, findlen, (char *)Args->Find);

      if (pos != -1) {
         MSG("Found keyword at %d", pos);
         //targstart  = SCICALL(SCI_GETTARGETSTART);
         //targend    = SCICALL(SCI_GETTARGETEND);

         // Do the replace

         if (Args->Flags & STF_EXPRESSION) {
            len = SCICALL(SCI_REPLACETARGETRE, (long unsigned int)-1, replace);
            end = end + (len - findlen);
         }
         else {
            SCICALL(SCI_REPLACETARGET, (ULONG)-1, replace);
            end = end + (replacelen - findlen);
         }
      }
      else MSG("Keyword not found.");
   }

   SCICALL(SCI_ENDUNDOACTION);

   LogBack();
   return ERR_Okay;
}

/*****************************************************************************
-METHOD-
ReportEvent: Private.  For internal use only.

Private

*****************************************************************************/

static ERROR SCINTILLA_ReportEvent(objScintilla *Self, APTR Void)
{
   if (!Self->ReportEventFlags) return ERR_Okay;

   LARGE flags = Self->ReportEventFlags;
   Self->ReportEventFlags = 0;
   report_event(Self, flags);
   return ERR_Okay;
}

/*****************************************************************************
-ACTION-
SaveToObject: Save content as a text stream to another object.

*****************************************************************************/

static ERROR SCINTILLA_SaveToObject(objScintilla *Self, struct acSaveToObject *Args)
{
   if ((!Args) OR (!Args->DestID)) return PostError(ERR_NullArgs);

   LONG len = SCICALL(SCI_GETLENGTH);

   LogBranch("To: %d, Size: %d", Args->DestID, len);

   OBJECTPTR object;
   if (!AccessObject(Args->DestID, 5000, &object)) {
      ERROR error;
      APTR buffer;
      if (!(AllocMemory(len+1, MEM_STRING|MEM_NO_CLEAR, &buffer, NULL))) {
         SCICALL(SCI_GETTEXT, len+1, (const char *)buffer);
         error = acWrite(object, buffer, len, NULL);
         FreeMemory(buffer);
      }
      else error = ERR_AllocMemory;

      ReleaseObject(object);
      LogBack();
      return error;
   }
   else {
      PostError(ERR_AccessObject);
      LogBack();
      return ERR_AccessObject;
   }
}

/*****************************************************************************

-METHOD-
SetFont: Changes the font that is used for text display.

Call SetFont() to change the font face that is used for displaying text.  The string format follows the standard for
font requests, e.g. `Helvete:12:Bold Italic:#ff0000`.  Refer to the Face field in the @Font class for more
details.

If the new face is invalid or fails to load, the current font will remain unchanged.

-INPUT-
cstr Face: The name of the new font face.

-RESULT-
Okay:
NullArgs:
CreateObject: Failed to create a Font object.
-END-

*****************************************************************************/

static ERROR SCINTILLA_SetFont(objScintilla *Self, struct sciSetFont *Args)
{
   if ((!Args) OR (!Args->Face)) return PostError(ERR_NullArgs);

   LogBranch("%s", Args->Face);

   objFont *font;
   if (!CreateObject(ID_FONT, NF_INTEGRAL, &font,
         FID_Face|TSTR, Args->Face,
         TAGEND)) {

      Self->Font = font;
      Self->Flags &= ~FTF_KERNING;

      create_styled_fonts(Self);

      Self->SciPan->panFontChanged(Self->Font, Self->BoldFont, Self->ItalicFont, Self->BIFont);

      calc_longest_line(Self);

      LogBack();
      return ERR_Okay;
   }
   else {
      LogBack();
      return ERR_CreateObject;
   }
}

//****************************************************************************
// Scintilla: ScrollToPoint

static ERROR SCINTILLA_ScrollToPoint(objScintilla *Self, struct acScrollToPoint *Args)
{
   FMSG("~","Sending Scroll requests to Scintilla: %dx%d.", (Args->Flags & STP_X) ? (LONG)Args->X : 0, (Args->Flags & STP_Y) ? (LONG)Args->Y : 0);

   Self->ScrollLocked++;

   if (Args->Flags & STP_X) Self->SciPan->panScrollToX(Args->X);
   if (Args->Flags & STP_Y) Self->SciPan->panScrollToY(Args->Y);

   Self->ScrollLocked--;

   STEP();
   return ERR_Okay;
}

/*****************************************************************************

-METHOD-
SelectRange: Selects a range of text, can also deselect all text.

This method will select an area of text between a start and end point, measured in characters.  It can also deselect
all text if no arguments are provided.

-INPUT-
int Start: The character at which the selection will start.
int End: The character at which the selection will end.  If negative, the last character in the document will be targeted.

-RESULT-
Okay:

*****************************************************************************/

static ERROR SCINTILLA_SelectRange(objScintilla *Self, struct sciSelectRange *Args)
{
   if ((!Args) OR ((!Args->Start) AND (!Args->End))) { // Deselect all text
      LONG pos = SCICALL(SCI_GETCURRENTPOS);
      SCICALL(SCI_SETANCHOR, pos);
      return ERR_Okay;
   }

   LogBranch("Selecting area %d to %d", Args->Start, Args->End);

   if (Args->End < 0) {
      LONG linecount = SCICALL(SCI_GETLINECOUNT);
      LONG end = SCICALL(SCI_FINDCOLUMN, linecount, 0L);
      SCICALL(SCI_SETSEL, Args->Start, end);
      SCICALL(SCI_SCROLLCARET);
   }
   else {
      SCICALL(SCI_SETSEL, Args->Start, Args->End);
      SCICALL(SCI_SCROLLCARET);
   }

   LogBack();
   return ERR_Okay;
}

//****************************************************************************

static ERROR SCINTILLA_Show(objScintilla *Self, APTR Void)
{
   if (!Self->Visible) {
      LogBranch(NULL);

         Self->Visible = TRUE;

         if (Self->VScrollbar) {
            SetLong(Self->VScrollbar, FID_Hide, FALSE);
            acShow(Self->VScrollbar);
         }

         if (Self->HScrollbar) {
            SetLong(Self->HScrollbar, FID_Hide, FALSE);
            acShow(Self->HScrollbar);
         }

         acDraw(Self);

      LogBack();
      return ERR_Okay;
   }
   else return ERR_Okay|ERF_Notified;
}

/*****************************************************************************

-METHOD-
TrimWhitespace: Strips trailing white-space from the document.

The TrimWhitespace method will remove trailing white-space from every line in the document.  Both tabs and spaces are
considered white-space - all other characters shall be treated as content.

The position of the cursor is reset to the left margin as a result of calling this method.

*****************************************************************************/

static ERROR SCINTILLA_TrimWhitespace(objScintilla *Self, APTR Void)
{
   LONG line, lineStart, lineEnd, i;
   UBYTE ch;

   FMSG("~","");

   LONG cursorpos = SCICALL(SCI_GETCURRENTPOS);
   LONG cursorline = SCICALL(SCI_LINEFROMPOSITION, cursorpos);

   SCICALL(SCI_BEGINUNDOACTION);

   LONG maxLines = SCICALL(SCI_GETLINECOUNT);
   for (line = 0; line < maxLines; line++) {
      lineStart = SCICALL(SCI_POSITIONFROMLINE, line);
      lineEnd = SCICALL(SCI_GETLINEENDPOSITION, line);
      i = lineEnd - 1;
      ch = SCICALL(SCI_GETCHARAT, i);
      while ((i >= lineStart) AND ((ch IS ' ') OR (ch IS '\t'))) {
         i--;
         ch = SCICALL(SCI_GETCHARAT, i);
      }

      if (i < (lineEnd-1)) {
         SCICALL(SCI_SETTARGETSTART, i + 1);
         SCICALL(SCI_SETTARGETEND, lineEnd);
         SCICALL(SCI_REPLACETARGET, 0UL, "");
      }
   }

   SCICALL(SCI_ENDUNDOACTION);

   SCICALL(SCI_GOTOLINE, cursorline);

   STEP();
   return ERR_Okay;
}

/*****************************************************************************
-ACTION-
Undo: Undo the last user action.

*****************************************************************************/

static ERROR SCINTILLA_Undo(objScintilla *Self, struct acUndo *Args)
{
   LogBranch(NULL);

   SCICALL(SCI_UNDO);

   LogBack();
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
AllowTabs: If enabled, use of the tab key produces real tabs and not spaces.

*****************************************************************************/

static ERROR GET_AllowTabs(objScintilla *Self, LONG *Value)
{
   *Value = Self->AllowTabs;
   return ERR_Okay;
}

static ERROR SET_AllowTabs(objScintilla *Self, LONG Value)
{
   if (Value) {
      Self->AllowTabs = TRUE;
      if (Self->Head.Flags & NF_INITIALISED) SCICALL(SCI_SETUSETABS, 1UL);
   }
   else {
      Self->AllowTabs = FALSE;
      if (Self->Head.Flags & NF_INITIALISED) SCICALL(SCI_SETUSETABS, 0UL);
   }
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
AutoIndent: If TRUE, enables auto-indenting when the user presses the enter key.

*****************************************************************************/

static ERROR GET_AutoIndent(objScintilla *Self, LONG *Value)
{
   *Value = Self->AutoIndent;
   return ERR_Okay;
}

static ERROR SET_AutoIndent(objScintilla *Self, LONG Value)
{
   if (Value) Self->AutoIndent = 1;
   else Self->AutoIndent = 0;
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
BkgdColour: Defines the background colour.  Alpha blending is not supported.

*****************************************************************************/

static ERROR SET_BkgdColour(objScintilla *Self, RGB8 *Value)
{
   Self->BkgdColour = *Value;

   if (Self->Head.Flags & NF_INITIALISED) {
      SCICALL(SCI_STYLESETBACK, STYLE_DEFAULT, (long int)SCICOLOUR(Self->BkgdColour.Red, Self->BkgdColour.Green, Self->BkgdColour.Blue));
   }

   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
CursorCol: The current column of the text cursor.

The CursorCol and #CursorRow fields reflect the current row and column of the user's text cursor.  The values
are updated every time that the cursor is moved.  Use #EventCallback and listen for the event SEF_CURSOR_POS to
receive updates on changes to CursorCol and #CursorRow.

-FIELD-
CursorRow: The current row of the text cursor.

The #CursorCol and CursorRow fields reflect the current row and column of the user's text cursor.  The values
are updated every time that the cursor is moved.  Use #EventCallback and listen for the event SEF_CURSOR_POS to
receive updates on changes to #CursorCol and CursorRow.

-FIELD-
CursorColour: Defines the colour of the text cursor.  Alpha blending is not supported.

*****************************************************************************/

static ERROR SET_CursorColour(objScintilla *Self, RGB8 *Value)
{
   Self->CursorColour = *Value;

   if (Self->Head.Flags & NF_INITIALISED) {
      SCICALL(SCI_SETCARETFORE, STYLE_DEFAULT, (long int)SCICOLOUR(Self->CursorColour.Red, Self->CursorColour.Green, Self->CursorColour.Blue));
   }

   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
FileDrop: A callback for receiving drag and drop file notifications.

Set this field with a reference to a callback function to receive notifications when the user drops a file onto the
Scintilla object's surface.  The synopsis for the callback function is `ERROR Function(*Scintilla, CSTRING Path)`

If multiple files are dropped, the callback will be repeatedly called until all of the file paths have been reported.

*****************************************************************************/

static ERROR GET_FileDrop(objScintilla *Self, FUNCTION **Value)
{
   if (Self->FileDrop.Type != CALL_NONE) {
      *Value = &Self->FileDrop;
      return ERR_Okay;
   }
   else return ERR_FieldNotSet;
}

static ERROR SET_FileDrop(objScintilla *Self, FUNCTION *Value)
{
   if (Value) Self->FileDrop = *Value;
   else Self->FileDrop.Type = CALL_NONE;
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Flags: Optional flags.

-FIELD-
Focus: Defines the object that is monitored for user focus changes.

By default, the user focus is managed by monitoring the target #Surface for changes (for instance, clicking
on or away from the surface will result in a focus state change).  If another object should be monitored for focus
state changes, it can be defined here prior to initialisation.

-FIELD-
FoldingMarkers: Folding markers in the left margin will be visible when this value is TRUE.

*****************************************************************************/

static ERROR GET_FoldingMarkers(objScintilla *Self, LONG *Value)
{
   *Value = Self->FoldingMarkers;
   return ERR_Okay;
}

static ERROR SET_FoldingMarkers(objScintilla *Self, LONG Value)
{
   if (Value) {
      Self->FoldingMarkers = TRUE;
      if (Self->Head.Flags & NF_INITIALISED) SCICALL(SCI_SETMARGINWIDTHN, 2, 20L);
   }
   else {
      Self->FoldingMarkers = FALSE;
      if (Self->Head.Flags & NF_INITIALISED) SCICALL(SCI_SETMARGINWIDTHN, 2, 0L);
   }
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Font: Refers to the font that is used for drawing text in the document.

This field refers to the font object that is used for drawing text in the document.  It is recommended that all
font customisation takes place prior to initialisation of the Scintilla object.  Directly altering the font object
after initialisation may result in clashes with the Scintilla class that produce unpredictable results.

To change the font post-initialisation, please use the #SetFont() method.

-FIELD-
HScroll: Refers to a scroll object that is managing horizontal scrolling.

This field refers to the scroll object that is managing horizontal scrolling of the document page.  It is possible
to set this field with either a @Scroll or @ScrollBar object, although in the latter case the
Scroll object will be extracted automatically and referenced in this field.

*****************************************************************************/

static ERROR SET_HScroll(objScintilla *Self, OBJECTID Value)
{
   if (GetClassID(Value) IS ID_SCROLLBAR) {
      OBJECTPTR object;
      if (!AccessObject(Value, 3000, &object)) {
         GetLong(object, FID_Scroll, &Value);
         ReleaseObject(object);
      }
   }

   if (GetClassID(Value) IS ID_SCROLL) {
      OBJECTPTR object;
      if (!AccessObject(Value, 3000, &object)) {
         SetLong(object, FID_Object, Self->Head.UniqueID);
         Self->HScrollID = Value;
         //Self->XPosition = 0;
         //if (Self->Head.Flags & NF_INITIALISED) CalculateHScroll(Self);
         ReleaseObject(object);
         return ERR_Okay;
      }
      else return PostError(ERR_AccessObject);
   }
   else return PostError(ERR_WrongObjectType);

   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
LeftMargin: The amount of white-space at the left side of the page.

*****************************************************************************/

static ERROR SET_LeftMargin(objScintilla *Self, LONG Value)
{
   if ((Value >= 0) AND (Value <= 100)) {
      Self->LeftMargin = Value;
      if (Self->Head.Flags & NF_INITIALISED) {
         SCICALL(SCI_SETMARGINLEFT, 0, Self->LeftMargin);
      }
      return ERR_Okay;
   }
   else return PostError(ERR_OutOfRange);
}

/*****************************************************************************

-FIELD-
Lexer: The lexer for document styling is defined here.

The lexer for document styling is defined here.

*****************************************************************************/

static ERROR SET_Lexer(objScintilla *Self, LONG Value)
{
   Self->Lexer = Value;
   if (Self->Head.Flags & NF_INITIALISED) {
      LogBranch("Changing lexer to %d", Value);
      Self->SciPan->SetLexer(Self->Lexer);
      LogBack();
   }
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
LineCount: The total number of lines in the document.

*****************************************************************************/

static ERROR GET_LineCount(objScintilla *Self, LONG *Value)
{
   if (Self->Head.Flags & NF_INITIALISED) {
      *Value = SCICALL(SCI_GETLINECOUNT);
      return ERR_Okay;
   }
   else return ERR_NotInitialised;
}

/*****************************************************************************

-FIELD-
LineHighlight: The colour to use when highlighting the line that contains the user's cursor.

*****************************************************************************/

static ERROR SET_LineHighlight(objScintilla *Self, RGB8 *Value)
{
   Self->LineHighlight = *Value;

   if (Self->Head.Flags & NF_INITIALISED) {
      SCICALL(SCI_SETCARETLINEBACK, SCICOLOUR(Self->LineHighlight.Red, Self->LineHighlight.Green, Self->LineHighlight.Blue));
      if (Self->LineHighlight.Alpha > 0) {
         SCICALL(SCI_SETCARETLINEVISIBLE, 1UL);
         //SCICALL(SCI_SETCARETLINEBACKALPHA, Self->LineHighlight.Alpha);
      }
      else SCICALL(SCI_SETCARETLINEVISIBLE, 0UL);
   }

   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
LineNumbers: Line numbers will appear on the left when this value is TRUE.

*****************************************************************************/

static ERROR GET_LineNumbers(objScintilla *Self, LONG *Value)
{
   *Value = Self->LineNumbers;
   return ERR_Okay;
}

static ERROR SET_LineNumbers(objScintilla *Self, LONG Value)
{
   if (Value) {
      Self->LineNumbers = TRUE;
      if (Self->Head.Flags & NF_INITIALISED) SCICALL(SCI_SETMARGINWIDTHN, 0, 50L);
   }
   else {
      Self->LineNumbers = FALSE;
      if (Self->Head.Flags & NF_INITIALISED) SCICALL(SCI_SETMARGINWIDTHN, 0, 0L);
   }
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Path: Identifies the location of a text file to load.

To load data from a text file into a scintilla object, set the Path field.

If the Path is set after initialisation, the existing content will be cleared and data loaded from the location
that you specify.  To change the path without automatically loading from the source file, set the
#Origin field instead.

*****************************************************************************/

static ERROR GET_Path(objScintilla *Self, CSTRING *Value)
{
   *Value = Self->Path;
   return ERR_Okay;
}

static ERROR SET_Path(objScintilla *Self, CSTRING Value)
{
   LogBranch((const char *)Value);

   if (Self->Path) { FreeMemory(Self->Path); Self->Path = NULL; }

   if ((Value) AND (*Value)) {
      if ((Self->Path = StrClone(Value))) {
         if (Self->Head.Flags & NF_INITIALISED) {
            if (load_file(Self, Self->Path) != ERR_Okay) {
               LogBack();
               return ERR_File;
            }
         }
      }
      else { LogBack(); return ERR_AllocMemory; }
   }

   LogBack();
   return ERR_Okay;
}

/****************************************************************************

-FIELD-
Origin: Similar to the Path field, but does not automatically load content if set.

This field is identical to the #Path field, with the exception that it does not update the content of a
scintilla object if it is set after initialisation.  This may be useful if the origin of the currently loaded content
needs to be changed without causing a load operation.

****************************************************************************/

static ERROR SET_Origin(objScintilla *Self, CSTRING Value)
{
   if (Self->Path) { FreeMemory(Self->Path); Self->Path = NULL; }

   if ((Value) AND (*Value)) {
      if (!(Self->Path = StrClone(Value))) {
         return PostError(ERR_AllocMemory);
      }
   }

   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Modified:  Returns TRUE if the document has been modified and not saved.

The Modified field controls the modification state of the document.  It is automatically changed to a value of TRUE
when the user edits the document.  To receive notification of changes to the document state, you should subscribe to
the Modified field.

It is recommended that you manually set this field to FALSE if the document is saved to disk.  The Scintilla class will
not make this change for you automatically.

*****************************************************************************/

static ERROR SET_Modified(objScintilla *Self, LONG Value)
{
   if (Self->Head.Flags & NF_INITIALISED) {
      if (Value) {
         Self->Modified = TRUE;
      }
      else {
         Self->Modified = FALSE;
         SCICALL(SCI_SETSAVEPOINT); // Tell Scintilla that the document is unmodified
      }

      report_event(Self, SEF_MODIFIED);
   }

   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
RightMargin: Defines the amount of white-space at the right side of the page.

*****************************************************************************/

static ERROR SET_RightMargin(objScintilla *Self, LONG Value)
{
   if ((Value >= 0) AND (Value <= 100)) {
      Self->RightMargin = Value;
      if (Self->Head.Flags & NF_INITIALISED) {
         SCICALL(SCI_SETMARGINRIGHT, 0, Self->RightMargin);
      }
      return ERR_Okay;
   }
   else return PostError(ERR_OutOfRange);
}

/*****************************************************************************

-FIELD-
ShowWhitespace: White-space characters will be visible to the user when this field is TRUE.

*****************************************************************************/

static ERROR GET_ShowWhitespace(objScintilla *Self, LONG *Value)
{
   *Value = Self->ShowWhitespace;
   return ERR_Okay;
}

static ERROR SET_ShowWhitespace(objScintilla *Self, LONG Value)
{
   if (Value) {
      Self->ShowWhitespace = 1;
      if (Self->Head.Flags & NF_INITIALISED) SCICALL(SCI_SETVIEWWS, (long unsigned int)SCWS_VISIBLEALWAYS);
   }
   else {
      Self->ShowWhitespace = 0;
      if (Self->Head.Flags & NF_INITIALISED) SCICALL(SCI_SETVIEWWS, (long unsigned int)SCWS_INVISIBLE);
   }
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
EventCallback: Provides callbacks for global state changes.

Set this field with a function reference to receive event notifications.  It must be set in conjunction with
#EventFlags so that you can select the type of notifications that will be received.

The callback function must be in the format `Function(*Scintilla, LARGE EventFlag)`.

The EventFlag value will indicate the event that occurred.  Please see the #EventFlags field for a list of
supported events and additional details.

*****************************************************************************/

static ERROR GET_EventCallback(objScintilla *Self, FUNCTION **Value)
{
   if (Self->EventCallback.Type != CALL_NONE) {
      *Value = &Self->EventCallback;
      return ERR_Okay;
   }
   else return ERR_FieldNotSet;
}

static ERROR SET_EventCallback(objScintilla *Self, FUNCTION *Value)
{
   if (Value) {
      if (Self->EventCallback.Type IS CALL_SCRIPT) UnsubscribeAction(Self->EventCallback.Script.Script, AC_Free);
      Self->EventCallback = *Value;
      if (Self->EventCallback.Type IS CALL_SCRIPT) SubscribeAction(Self->EventCallback.Script.Script, AC_Free);
   }
   else Self->EventCallback.Type = CALL_NONE;
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
EventFlags: Specifies events that need to be reported from the Scintilla object.

To receive event notifications, set #EventCallback with a function reference and the EventFlags field with a mask that
indicates the events that need to be received.

-FIELD-
ScrollTarget: Sets an alternative target surface for scrollbar creation.

During initialisation the Scintilla class will create scrollbars within the target #Surface.  If this
behaviour is undesirable, an alternative target surface can be defined by setting the ScrollTarget field.

-FIELD-
SelectBkgd: Defines the background colour of selected text.  Supports alpha blending.

*****************************************************************************/

static ERROR SET_SelectBkgd(objScintilla *Self, RGB8 *Value)
{
   if ((Value) AND (Value->Alpha)) {
      Self->SelectBkgd = *Value;
      SCICALL(SCI_SETSELBACK, (unsigned long int)true, (long int)SCICOLOUR(Self->SelectBkgd.Red, Self->SelectBkgd.Green, Self->SelectBkgd.Blue));
      //SCICALL(SCI_SETSELALPHA, Self->SelectBkgd.Alpha);
   }
   else {
      Self->SelectBkgd.Alpha = 0;
      SCICALL(SCI_SETSELBACK, (unsigned long int)false, 0L);
      //SCICALL(SCI_SETSELALPHA, 0UL);
   }
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
SelectFore: Defines the colour of selected text.  Supports alpha blending.

*****************************************************************************/

static ERROR SET_SelectFore(objScintilla *Self, RGB8 *Value)
{
   LogMsg("New SelectFore colour: %d,%d,%d,%d", Value->Red, Value->Green, Value->Blue, Value->Alpha);
   if ((Value) AND (Value->Alpha)) {
      Self->SelectFore = *Value;
      SCICALL(SCI_SETSELFORE, (unsigned long int)true, (long int)SCICOLOUR(Self->SelectFore.Red, Self->SelectFore.Green, Self->SelectFore.Blue));
   }
   else {
      Self->SelectFore.Alpha = 0;
      SCICALL(SCI_SETSELFORE, (unsigned long int)false, 0L);
   }
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
String: Manages the text data as a complete string.

A Scintilla document can be completely updated by setting the String field.  Equally, the entire document can be
retrieved by getting the String field.  Please be aware that retrieving the document in string format can be very
inefficient, as the document text is normally stored on a per-line basis.  Consider using the #GetLine()
method as the preferred alternative, as it is much more efficient with memory usage.

*****************************************************************************/

static ERROR GET_String(objScintilla *Self, STRING *Value)
{
   LONG len = SCICALL(SCI_GETLENGTH);

   if (Self->StringBuffer) { FreeMemory(Self->StringBuffer); Self->StringBuffer = NULL; }

   if (!AllocMemory(len+1, MEM_STRING|MEM_NO_CLEAR, &Self->StringBuffer, NULL)) {
      SCICALL(SCI_GETTEXT, len+1, (const char *)Self->StringBuffer);
      *Value = Self->StringBuffer;
      return ERR_Okay;
   }
   else return ERR_AllocMemory;
}

static ERROR SET_String(objScintilla *Self, CSTRING Value)
{
   if (Self->Head.Flags & NF_INITIALISED) {
      if ((Value) AND (*Value)) SCICALL(SCI_SETTEXT, 0UL, (const char *)Value);
      else acClear(Self);
   }
   else return ERR_NotInitialised;

   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Surface: Refers to the @Surface targeted by the Scintilla object.

This compulsory field refers to the @Surface that the Scintilla object is targeting for graphics operations.  If not
set prior to initialisation, the Scintilla object will search for the nearest @Surface object based on its ownership
hierarchy.

-FIELD-
Symbols: Symbols can be displayed in the left margin when this value is TRUE.

*****************************************************************************/

static ERROR GET_Symbols(objScintilla *Self, LONG *Value)
{
   *Value = Self->Symbols;
   return ERR_Okay;
}

static ERROR SET_Symbols(objScintilla *Self, LONG Value)
{
   if (Value) {
      Self->Symbols = TRUE;
      if (Self->Head.Flags & NF_INITIALISED) SCICALL(SCI_SETMARGINWIDTHN, 1, 20L);
   }
   else {
      Self->Symbols = FALSE;
      if (Self->Head.Flags & NF_INITIALISED) SCICALL(SCI_SETMARGINWIDTHN, 1, 0L);
   }
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
TabWidth: The width of tab stops in the document, measured as fixed-width characters.

*****************************************************************************/

static ERROR GET_TabWidth(objScintilla *Self, LONG *Value)
{
   *Value = Self->TabWidth;
   return ERR_Okay;
}

static ERROR SET_TabWidth(objScintilla *Self, LONG Value)
{
   if (Value > 0) {
      if (Value > 200) Value = 200;
      Self->TabWidth = Value;
      if (Self->Head.Flags & NF_INITIALISED) SCICALL(SCI_SETTABWIDTH, Value);
      return ERR_Okay;
   }
   else return PostError(ERR_OutOfRange);
}

/*****************************************************************************

-FIELD-
VScroll: Refers to a scroll object that is managing horizontal scrolling.

This field refers to the scroll object that is managing horizontal scrolling of the document page.  It is possible
to set this field with either a @Scroll or @ScrollBar object, although in the latter case the
Scroll object will be extracted automatically and referenced in this field.

*****************************************************************************/

static ERROR SET_VScroll(objScintilla *Self, OBJECTID Value)
{
   OBJECTPTR object;

   // If we've been given a scrollbar, extract the scroll object

   if (GetClassID(Value) IS ID_SCROLLBAR) {
      if (!AccessObject(Value, 3000, &object)) {
         GetLong(object, FID_Scroll, &Value);
         ReleaseObject(object);
      }
      else return PostError(ERR_AccessObject);
   }

   // Use the scroll object for issuing commands

   if (GetClassID(Value) IS ID_SCROLL) {
      if (!AccessObject(Value, 3000, &object)) {
         SetLong(object, FID_Object, Self->Head.UniqueID);
         Self->VScrollID = Value;
         ReleaseObject(object);
         return ERR_Okay;
      }
      else return PostError(ERR_AccessObject);
   }
   else return PostError(ERR_WrongObjectType);
}

/*****************************************************************************

-FIELD-
TextColour: Defines the default colour of foreground text.  Supports alpha blending.

*****************************************************************************/

static ERROR SET_TextColour(objScintilla *Self, RGB8 *Value)
{
   Self->TextColour = *Value;

   if (Self->Head.Flags & NF_INITIALISED) {
      SCICALL(SCI_STYLESETFORE, STYLE_DEFAULT, (long int)SCICOLOUR(Self->TextColour.Red, Self->TextColour.Green, Self->TextColour.Blue));
   }

   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Visible: If TRUE, indicates the Scintilla object is visible in the target #Surface.

This field is set to TRUE if the Scintilla object is visible in the target #Surface.  To control visibility, please use
the #Show() and #Hide() actions appropriately.  This field can be set to FALSE prior to initialisation if the Scintilla
object should start in a hidden state.

-FIELD-
Wordwrap: Enables automatic word wrapping when TRUE.
-END-

*****************************************************************************/

static ERROR GET_Wordwrap(objScintilla *Self, LONG *Value)
{
   *Value = Self->Wordwrap;
   return ERR_Okay;
}

static ERROR SET_Wordwrap(objScintilla *Self, LONG Value)
{
   if (Value) Self->Wordwrap = TRUE;
   else Self->Wordwrap = FALSE;

   if (Self->Head.Flags & NF_INITIALISED) {
      Self->SciPan->panWordwrap(Self->Wordwrap);
   }
   return ERR_Okay;
}

//*****************************************************************************

static void create_styled_fonts(objScintilla *Self)
{
   LogMsg("create_styled_fonts(%s,%.2f,$%.8x)", Self->Font->Face, Self->Font->Point, Self->Font->Flags);

   if (!Self->Font) return;

   if (Self->BoldFont)   { acFree(Self->BoldFont); Self->BoldFont = NULL; }
   if (Self->ItalicFont) { acFree(Self->ItalicFont); Self->ItalicFont = NULL; }
   if (Self->BIFont)     { acFree(Self->BIFont); Self->BIFont = NULL; }

   if (!CreateObject(ID_FONT, NF_INTEGRAL, &Self->BoldFont,
         FID_Face|TSTR,     Self->Font->Face,
         FID_Point|TDOUBLE, Self->Font->Point,
         FID_Flags|TLONG,   Self->Font->Flags,
         FID_Style|TSTR,    "bold",
         TAGEND)) {
      if (!(Self->Font->Flags & FTF_KERNING)) Self->BoldFont->Flags &= ~FTF_KERNING;
   }

   if (!CreateObject(ID_FONT, NF_INTEGRAL, &Self->ItalicFont,
         FID_Face|TSTR,     Self->Font->Face,
         FID_Point|TDOUBLE, Self->Font->Point,
         FID_Flags|TLONG,   Self->Font->Flags,
         FID_Style|TSTR,    "italics",
         TAGEND)) {
      if (!(Self->Font->Flags & FTF_KERNING)) Self->BoldFont->Flags &= ~FTF_KERNING;
   }

   if (!CreateObject(ID_FONT, NF_INTEGRAL, &Self->BIFont,
         FID_Face|TSTR,     Self->Font->Face,
         FID_Point|TDOUBLE, Self->Font->Point,
         FID_Flags|TLONG,   Self->Font->Flags,
         FID_Style|TSTR,    "bold italics",
         TAGEND)) {
      if (!(Self->Font->Flags & FTF_KERNING)) Self->BoldFont->Flags &= ~FTF_KERNING;
   }
}

//****************************************************************************
// Scintilla initiates drawing instructions through window::InvalidateRectangle()

static THREADVAR objBitmap *glBitmap = NULL;

static void draw_scintilla(objScintilla *Self, objSurface *Surface, struct rkBitmap *Bitmap)
{
   if (!Self->Visible) return;
   if (!(Self->Head.Flags & NF_INITIALISED)) return;

   FMSG("~draw_scintilla()","Surface: %d, Bitmap: %d. Clip: %dx%d,%dx%d, Offset: %dx%d", Surface->Head.UniqueID, Bitmap->Head.UniqueID, Bitmap->Clip.Left, Bitmap->Clip.Top, Bitmap->Clip.Right - Bitmap->Clip.Left, Bitmap->Clip.Bottom - Bitmap->Clip.Top, Bitmap->XOffset, Bitmap->YOffset);

   glBitmap = Bitmap;

   glFont       = (OBJECTPTR)Self->Font;
   glBoldFont   = (OBJECTPTR)Self->BoldFont;
   glItalicFont = (OBJECTPTR)Self->ItalicFont;
   glBIFont     = (OBJECTPTR)Self->BIFont;

   Self->SciPan->panDraw(Surface, Bitmap);

   glBitmap = NULL;

   if (Self->Flags & SCF_DISABLED) {
      gfxDrawRectangle(Bitmap, 0, 0, Bitmap->Width, Bitmap->Height, PackPixelA(Bitmap, 0, 0, 0, 64), BAF_FILL|BAF_BLEND);
   }

   STEP();
}

//*****************************************************************************

void user_error(CSTRING Title, CSTRING Message)
{
   OBJECTPTR dialog;

   if (!CreateObject(ID_DIALOG, NF_INTEGRAL, &dialog,
         FID_Image|TSTR,    "icons:items/error(48)",
         FID_Options|TSTR,  "okay",
         FID_Title|TSTR,    Title,
         FID_String|TSTR,   Message,
         TAGEND)) {
      acShow(dialog);
   }
}

//*****************************************************************************

static ERROR load_file(objScintilla *Self, CSTRING Path)
{
   objFile *file;
   STRING str;
   LONG size, len;
   ERROR error;

   if (!CreateObject(ID_FILE, NF_INTEGRAL, &file,
         FID_Flags|TLONG, FL_READ,
         FID_Path|TSTR,   Path,
         TAGEND)) {

      if (file->Flags & FL_STREAM) {
         if (!flStartStream(file, Self->Head.UniqueID, FL_READ, 0)) {
            acClear(Self);

            SubscribeActionTags(file, AC_Write, TAGEND);
            Self->FileStream = file;
            file = NULL;
         }
         else error = ERR_Failed;
      }
      else if (!GetLong(file, FID_Size, &size)) {
         if (size > 0) {
            if (size < 1024 * 1024 * 10) {
               if (!AllocMemory(size+1, MEM_STRING|MEM_NO_CLEAR, &str, NULL)) {
                  if (!acRead(file, str, size, &len)) {
                     str[len] = 0;
                     SCICALL(SCI_SETTEXT, str);
                     SCICALL(SCI_EMPTYUNDOBUFFER);
                     error = ERR_Okay;

                     calc_longest_line(Self);
                  }
                  else error = ERR_Read;

                  FreeMemory(str);
               }
               else error = ERR_AllocMemory;
            }
            else error = ERR_BufferOverflow;
         }
         else error = ERR_Okay; // File is empty
      }
      else error = ERR_File;

      if (file) acFree(file);
   }
   else error = ERR_File;

   if ((!error) AND (Self->Flags & SCF_DETECT_LEXER)) {
      LONG i;
      for (i=0; Path[i]; i++);
      while ((i > 0) AND (Path[i-1] != '/') AND (Path[i-1] != '\\') AND (Path[i-1] != ':')) i--;
      Path = Path + i;

      for (i=0; i < ARRAYSIZE(glLexers); i++) {
         if (!StrCompare(glLexers[i].File, Path, 0, STR_WILDCARD)) {
            Self->Lexer = glLexers[i].Lexer;
            LogBranch("Lexer for the loaded file is %d.", Self->Lexer);
            Self->SciPan->SetLexer(Self->Lexer);
            LogBack();
            break;
         }
      }
      if (i >= ARRAYSIZE(glLexers)) LogMsg("Failed to choose a lexer for %s", Path);
   }

   return error;
}

//*****************************************************************************

static void key_event(objScintilla *Self, evKey *Event, LONG Size)
{
   LONG i;

   if (Self->Flags & SCF_DISABLED) return;
   if (!(Self->Flags & SCF_EDIT)) return;

   if (Event->Qualifiers & KQ_PRESSED) {
      if ((Event->Code IS K_L_SHIFT) OR (Event->Code IS K_R_SHIFT)) Self->KeyShift = TRUE;
      else if ((Event->Code IS K_L_ALT) OR (Event->Code IS K_R_ALT)) Self->KeyAlt = TRUE;
      else if ((Event->Code IS K_L_CONTROL) OR (Event->Code IS K_R_CONTROL)) Self->KeyCtrl = TRUE;

      LONG keyval = Event->Code;

      char string[8];
      string[0] = 0;

      if (!(Event->Qualifiers & KQ_NOT_PRINTABLE)) {
         WORD out = UTF8WriteValue(Event->Unicode, string, sizeof(string)-1);
         if (out >= 0) string[out] = 0;
      }

      StrCopy(string, (STRING)Self->SciPan->lastkeytrans, sizeof(Self->SciPan->lastkeytrans));

      switch (keyval) {
         // Handle known non-printable character keys first
         case K_TAB:       keyval = SCK_TAB; break;
         case K_DOWN:      keyval = SCK_DOWN; break;
         case K_UP:        keyval = SCK_UP; break;
         case K_LEFT:      keyval = SCK_LEFT; break;
         case K_RIGHT:     keyval = SCK_RIGHT; break;
         case K_HOME:      keyval = SCK_HOME; break;
         case K_END:       keyval = SCK_END; break;
         case K_PAGE_UP:   keyval = SCK_PRIOR; break;
         case K_PAGE_DOWN: keyval = SCK_NEXT; break;
         case K_DELETE:    keyval = SCK_DELETE; break;
         case K_INSERT:    keyval = SCK_INSERT; break;
         case K_ENTER:
         case K_NP_ENTER:  keyval = SCK_RETURN; break;
         case K_ESCAPE:    keyval = SCK_ESCAPE; break;
         case K_BACKSPACE: keyval = SCK_BACK; break;
         default:
            if (Event->Qualifiers & KQ_NOT_PRINTABLE) {
               // Unhandled non-printable characters are ignored
               keyval = 0;
            }
            else if ((keyval >= K_A) AND (keyval <= K_Z)) {
               keyval = keyval - K_A + (LONG)'a';
            }
            else if((keyval >= K_ZERO) AND (keyval <= K_NINE)) {
               keyval = keyval - K_ZERO + (LONG)'0';
            }
            else {
               // Call KeyDefault(), which will pull the key value from the lastkeytrans buffer
               if (string[0]) Self->SciPan->KeyDefault(0,0);
               keyval = 0;
            }
            break;
      }

      if (keyval) {
         FMSG("~","Keypress: %d", keyval);
         Self->SciPan->panKeyDown(keyval, Event->Qualifiers);
         STEP();
      }
   }
   else if (Event->Qualifiers & KQ_RELEASED) {
      if ((Event->Code IS K_L_SHIFT) OR (Event->Code IS K_R_SHIFT)) Self->KeyShift = FALSE;
      else if ((Event->Code IS K_L_ALT) OR (Event->Code IS K_R_ALT)) Self->KeyAlt = FALSE;
      else if ((Event->Code IS K_L_CONTROL) OR (Event->Code IS K_R_CONTROL)) Self->KeyCtrl = FALSE;
   }
}

//*****************************************************************************

static void report_event(objScintilla *Self, LARGE Event)
{
   if (Event & Self->EventFlags) {
      if (Self->EventCallback.Type) {
          if (Self->EventCallback.Type IS CALL_STDC) {
            void (*routine)(objScintilla *, LARGE);
            OBJECTPTR context;

            context = SetContext(Self->EventCallback.StdC.Context);
               routine = (void (*)(objScintilla *, LARGE)) Self->EventCallback.StdC.Routine;
               routine(Self, Event);
            SetContext(context);
         }
         else if (Self->EventCallback.Type IS CALL_SCRIPT) {
            struct scCallback exec;
            OBJECTPTR script;
            struct ScriptArg args[2];
            args[0].Name = "Scintilla";
            args[0].Type = FD_OBJECTPTR;
            args[0].Address = Self;

            args[1].Name = "EventFlags";
            args[1].Type = FD_LARGE;
            args[1].Large = Event;

            exec.ProcedureID = Self->EventCallback.Script.ProcedureID;
            exec.Args      = args;
            exec.TotalArgs = ARRAYSIZE(args);
            if ((script = Self->EventCallback.Script.Script)) {
               Action(MT_ScCallback, script, &exec);
            }
         }
      }
   }
}

//*****************************************************************************

static void calc_longest_line(objScintilla *Self)
{
   LONG i, linewidth, col;
   #define LINE_COUNT_LIMIT 2000

   if (!Self->Font) return;

   FMSG("~calc_longest()","Wrap: %d", Self->Wordwrap);

   LONG lines = SCICALL(SCI_GETLINECOUNT);
   if (lines > LINE_COUNT_LIMIT) {
      lines = LINE_COUNT_LIMIT;
   }

   LONG cwidth = 0;
   LONG cline  = 0;
   LONG cpos   = 0;

   if (Self->Wordwrap) {
      Self->LongestLine = 0;
      Self->LongestWidth = 0;
      goto calc_scroll;
   }

   // Find the line with the longest width

   for (i=0; i < lines; i++) {
      LONG end = SCICALL(SCI_GETLINEENDPOSITION, i);
      if (Self->Font->FixedWidth) {
         LONG col = SCICALL(SCI_GETCOLUMN, end);
         linewidth = col * Self->Font->FixedWidth;
      }
      else linewidth = SCICALL(SCI_POINTXFROMPOSITION, 0, end);

      if (linewidth > cwidth) {
         cline = i;
         cwidth = linewidth;
      }
   }

   if (lines IS LINE_COUNT_LIMIT) {
      Self->LongestWidth += 1024; // Add lots of extra space in case there are much longer lines further on in the document
   }
   else Self->LongestWidth += 30;

   Self->LongestLine  = cline;
   Self->LongestWidth = cwidth;

calc_scroll:
   FMSG("calc_longest:","Longest line: %d", Self->LongestWidth);

   if (Self->Head.Flags & NF_INITIALISED) {
      if (Self->LongestWidth >= 60) {
         SCICALL(SCI_SETSCROLLWIDTH, Self->LongestWidth);
      }
      else SCICALL(SCI_SETSCROLLWIDTH, 1UL);
   }

   STEP();
}

//****************************************************************************

static ERROR idle_timer(objScintilla *Self, LARGE Elapsed, LARGE CurrentTime)
{
   AdjustLogLevel(3);
   Self->SciPan->panIdleEvent();
   AdjustLogLevel(-3);
   return ERR_Okay;
}

//****************************************************************************

#include "class_scintilla_ext.cxx"
#include "pan_classes.cxx"
#include "pan_window.cxx"
#include "pan_menu.cxx"
#include "pan_surface.cxx"
#include "pan_listbox.cxx"

//****************************************************************************

#include "class_scintilla_def.cxx"

static const struct FieldArray clFields[] = {
   { "EventFlags",     FDF_LARGE|FDF_FLAGS|FDF_RW, (MAXINT)&clScintillaEventFlags, NULL, NULL },
   { "Font",           FDF_INTEGRAL|FDF_R,   ID_FONT,    NULL, NULL },
   { "Path",           FDF_STRING|FDF_RW,    0,          NULL, (APTR)SET_Path },
   { "VScroll",        FDF_OBJECTID|FDF_RI,  ID_SCROLL,  NULL, (APTR)SET_VScroll },
   { "HScroll",        FDF_OBJECTID|FDF_RI,  ID_SCROLL,  NULL, (APTR)SET_HScroll },
   { "Surface",        FDF_OBJECTID|FDF_RI,  ID_SURFACE, NULL, NULL },
   { "Flags",          FDF_LONGFLAGS|FDF_RI, (MAXINT)&clScintillaFlags, NULL, NULL },
   { "Focus",          FDF_OBJECTID|FDF_RI,  0,          NULL, NULL },
   { "Visible",        FDF_LONG|FDF_RI,      0,          NULL, NULL },
   { "LeftMargin",     FDF_LONG|FDF_RW,      0,          NULL, (APTR)SET_LeftMargin },
   { "RightMargin",    FDF_LONG|FDF_RW,      0,          NULL, (APTR)SET_RightMargin },
   { "LineHighlight",  FDF_RGB|FDF_RW,       0,          NULL, (APTR)SET_LineHighlight },
   { "SelectFore",     FDF_RGB|FDF_RI,       0,          NULL, (APTR)SET_SelectFore },
   { "SelectBkgd",     FDF_RGB|FDF_RI,       0,          NULL, (APTR)SET_SelectBkgd },
   { "BkgdColour",     FDF_RGB|FDF_RW,       0,          NULL, (APTR)SET_BkgdColour },
   { "CursorColour",   FDF_RGB|FDF_RW,       0,          NULL, (APTR)SET_CursorColour },
   { "TextColour",     FDF_RGB|FDF_RW,       0,          NULL, (APTR)SET_TextColour },
   { "ScrollTarget",   FDF_OBJECTID|FDF_RI,  0,          NULL, NULL },
   { "CursorRow",      FDF_LONG|FDF_RW,      0,          NULL, NULL },
   { "CursorCol",      FDF_LONG|FDF_RW,      0,          NULL, NULL },
   { "Lexer",          FDF_LONG|FDF_LOOKUP|FDF_RI, (MAXINT)&clScintillaLexer, NULL, (APTR)SET_Lexer },
   { "Modified",       FDF_LONG|FDF_RW,      0,          NULL, (APTR)SET_Modified },

   // Virtual fields
   { "AllowTabs",      FDF_LONG|FDF_RW,    0, (APTR)GET_AllowTabs,      (APTR)SET_AllowTabs },
   { "AutoIndent",     FDF_LONG|FDF_RW,    0, (APTR)GET_AutoIndent,     (APTR)SET_AutoIndent },
   { "FileDrop",       FDF_FUNCTIONPTR|FDF_RW, 0, (APTR)GET_FileDrop,   (APTR)SET_FileDrop },
   { "FoldingMarkers", FDF_LONG|FDF_RW,    0, (APTR)GET_FoldingMarkers, (APTR)SET_FoldingMarkers },
   { "LineCount",      FDF_LONG|FDF_R,     0, (APTR)GET_LineCount,      NULL },
   { "LineNumbers",    FDF_LONG|FDF_RW,    0, (APTR)GET_LineNumbers,    (APTR)SET_LineNumbers },
   { "Origin",         FDF_STRING|FDF_RW,  0, (APTR)GET_Path,           (APTR)SET_Origin },
   { "ShowWhitespace", FDF_LONG|FDF_RW,    0, (APTR)GET_ShowWhitespace, (APTR)SET_ShowWhitespace },
   { "EventCallback",  FDF_FUNCTIONPTR|FDF_RW, 0, (APTR)GET_EventCallback, (APTR)SET_EventCallback },
   { "String",         FDF_STRING|FDF_RW,  0, (APTR)GET_String,         (APTR)SET_String },
   { "Symbols",        FDF_LONG|FDF_RW,    0, (APTR)GET_Symbols,        (APTR)SET_Symbols },
   { "TabWidth",       FDF_LONG|FDF_RW,    0, (APTR)GET_TabWidth,       (APTR)SET_TabWidth },
   { "Wordwrap",       FDF_LONG|FDF_RW,    0, (APTR)GET_Wordwrap,       (APTR)SET_Wordwrap },
   END_FIELD
};

//****************************************************************************

static ERROR create_scintilla(void)
{
   return CreateObject(ID_METACLASS, 0, &clScintilla,
      FID_ClassVersion|TFLOAT, VER_SCINTILLA,
      FID_Name|TSTR,      "Scintilla",
      FID_Category|TLONG, CCF_TOOL,
      FID_Flags|TLONG,    CLF_PROMOTE_INTEGRAL,
      FID_Actions|TPTR,   clScintillaActions,
      FID_Methods|TARRAY, clScintillaMethods,
      FID_Fields|TARRAY,  clFields,
      FID_Size|TLONG,     sizeof(objScintilla),
      FID_Path|TSTR,      "modules:scintilla",
      TAGEND);
}

//****************************************************************************

PARASOL_MOD(CMDInit, NULL, NULL, CMDExpunge, 1.0)
