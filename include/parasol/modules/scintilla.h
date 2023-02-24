#pragma once

// Name:      scintilla.h
// Copyright: Paul Manias © 2005-2023
// Generator: idl-c

#include <parasol/main.h>

#define MODVERSION_SCINTILLA (1)

#include <parasol/modules/display.h>
#include <parasol/modules/font.h>

class objScintilla;
class objScintillaSearch;

// Scintilla Lexers.  These codes originate from the Scintilla library.

#define SCLEX_ERRORLIST 10
#define SCLEX_MAKEFILE 11
#define SCLEX_BATCH 12
#define SCLEX_FLUID 15
#define SCLEX_DIFF 16
#define SCLEX_PASCAL 18
#define SCLEX_RUBY 22
#define SCLEX_VBSCRIPT 28
#define SCLEX_ASP 29
#define SCLEX_PYTHON 2
#define SCLEX_ASSEMBLER 34
#define SCLEX_CSS 38
#define SCLEX_CPP 3
#define SCLEX_HTML 4
#define SCLEX_XML 5
#define SCLEX_BASH 62
#define SCLEX_PHPSCRIPT 69
#define SCLEX_PERL 6
#define SCLEX_REBOL 71
#define SCLEX_SQL 7
#define SCLEX_VB 8
#define SCLEX_PROPERTIES 9

// Optional flags.

#define SCF_DISABLED 0x00000001
#define SCF_DETECT_LEXER 0x00000002
#define SCF_EDIT 0x00000004
#define SCF_EXT_PAGE 0x00000008

// Flags for EventCallback and EventFlags

#define SEF_MODIFIED 0x00000001
#define SEF_CURSOR_POS 0x00000002
#define SEF_FAIL_RO 0x00000004
#define SEF_NEW_CHAR 0x00000008

// Scintilla search flags.

#define STF_CASE 0x00000001
#define STF_MOVE_CURSOR 0x00000002
#define STF_SCAN_SELECTION 0x00000004
#define STF_BACKWARDS 0x00000008
#define STF_EXPRESSION 0x00000010
#define STF_WRAP 0x00000020

// Scintilla class definition

#define VER_SCINTILLA (1.000000)

// Scintilla methods

#define MT_SciSetFont -1
#define MT_SciReplaceText -2
#define MT_SciDeleteLine -3
#define MT_SciSelectRange -4
#define MT_SciInsertText -5
#define MT_SciGetLine -6
#define MT_SciReplaceLine -7
#define MT_SciGotoLine -8
#define MT_SciTrimWhitespace -9
#define MT_SciGetPos -10
#define MT_SciReportEvent -11

struct sciSetFont { CSTRING Face;  };
struct sciReplaceText { CSTRING Find; CSTRING Replace; LONG Flags; LONG Start; LONG End;  };
struct sciDeleteLine { LONG Line;  };
struct sciSelectRange { LONG Start; LONG End;  };
struct sciInsertText { CSTRING String; LONG Pos;  };
struct sciGetLine { LONG Line; STRING Buffer; LONG Length;  };
struct sciReplaceLine { LONG Line; CSTRING String; LONG Length;  };
struct sciGotoLine { LONG Line;  };
struct sciGetPos { LONG Line; LONG Column; LONG Pos;  };

INLINE ERROR sciSetFont(APTR Ob, CSTRING Face) {
   struct sciSetFont args = { Face };
   return(Action(MT_SciSetFont, (OBJECTPTR)Ob, &args));
}

INLINE ERROR sciReplaceText(APTR Ob, CSTRING Find, CSTRING Replace, LONG Flags, LONG Start, LONG End) {
   struct sciReplaceText args = { Find, Replace, Flags, Start, End };
   return(Action(MT_SciReplaceText, (OBJECTPTR)Ob, &args));
}

INLINE ERROR sciDeleteLine(APTR Ob, LONG Line) {
   struct sciDeleteLine args = { Line };
   return(Action(MT_SciDeleteLine, (OBJECTPTR)Ob, &args));
}

INLINE ERROR sciSelectRange(APTR Ob, LONG Start, LONG End) {
   struct sciSelectRange args = { Start, End };
   return(Action(MT_SciSelectRange, (OBJECTPTR)Ob, &args));
}

INLINE ERROR sciInsertText(APTR Ob, CSTRING String, LONG Pos) {
   struct sciInsertText args = { String, Pos };
   return(Action(MT_SciInsertText, (OBJECTPTR)Ob, &args));
}

INLINE ERROR sciGetLine(APTR Ob, LONG Line, STRING Buffer, LONG Length) {
   struct sciGetLine args = { Line, Buffer, Length };
   return(Action(MT_SciGetLine, (OBJECTPTR)Ob, &args));
}

INLINE ERROR sciReplaceLine(APTR Ob, LONG Line, CSTRING String, LONG Length) {
   struct sciReplaceLine args = { Line, String, Length };
   return(Action(MT_SciReplaceLine, (OBJECTPTR)Ob, &args));
}

INLINE ERROR sciGotoLine(APTR Ob, LONG Line) {
   struct sciGotoLine args = { Line };
   return(Action(MT_SciGotoLine, (OBJECTPTR)Ob, &args));
}

#define sciTrimWhitespace(obj) Action(MT_SciTrimWhitespace,(obj),0)

INLINE ERROR sciGetPos(APTR Ob, LONG Line, LONG Column, LONG * Pos) {
   struct sciGetPos args = { Line, Column, 0 };
   ERROR error = Action(MT_SciGetPos, (OBJECTPTR)Ob, &args);
   if (Pos) *Pos = args.Pos;
   return(error);
}

#define sciReportEvent(obj) Action(MT_SciReportEvent,(obj),0)


class objScintilla : public BaseClass {
   public:
   static constexpr CLASSID CLASS_ID = ID_SCINTILLA;
   static constexpr CSTRING CLASS_NAME = "Scintilla";

   using create = pf::Create<objScintilla>;

   LARGE     EventFlags;         // Specifies events that need to be reported from the Scintilla object.
   objFont * Font;               // Refers to the font that is used for drawing text in the document.
   CSTRING   Path;               // Identifies the location of a text file to load.
   OBJECTID  SurfaceID;          // Refers to the @Surface targeted by the Scintilla object.
   LONG      Flags;              // Optional flags.
   OBJECTID  FocusID;            // Defines the object that is monitored for user focus changes.
   LONG      Visible;            // If TRUE, indicates the Scintilla object is visible in the target #Surface.
   LONG      LeftMargin;         // The amount of white-space at the left side of the page.
   LONG      RightMargin;        // Defines the amount of white-space at the right side of the page.
   struct RGB8 LineHighlight;    // The colour to use when highlighting the line that contains the user's cursor.
   struct RGB8 SelectFore;       // Defines the colour of selected text.  Supports alpha blending.
   struct RGB8 SelectBkgd;       // Defines the background colour of selected text.  Supports alpha blending.
   struct RGB8 BkgdColour;       // Defines the background colour.  Alpha blending is not supported.
   struct RGB8 CursorColour;     // Defines the colour of the text cursor.  Alpha blending is not supported.
   struct RGB8 TextColour;       // Defines the default colour of foreground text.  Supports alpha blending.
   LONG      CursorRow;          // The current row of the text cursor.
   LONG      CursorCol;          // The current column of the text cursor.
   LONG      Lexer;              // The lexer for document styling is defined here.
   LONG      Modified;           // Returns TRUE if the document has been modified and not saved.

   // Action stubs

   inline ERROR clear() { return Action(AC_Clear, this, NULL); }
   inline ERROR clipboard(LONG Mode) {
      struct acClipboard args = { Mode };
      return Action(AC_Clipboard, this, &args);
   }
   inline ERROR dataFeed(OBJECTID ObjectID, LONG Datatype, const void *Buffer, LONG Size) {
      struct acDataFeed args = { { ObjectID }, { Datatype }, Buffer, Size };
      return Action(AC_DataFeed, this, &args);
   }
   inline ERROR disable() { return Action(AC_Disable, this, NULL); }
   inline ERROR draw() { return Action(AC_Draw, this, NULL); }
   inline ERROR drawArea(LONG X, LONG Y, LONG Width, LONG Height) {
      struct acDraw args = { X, Y, Width, Height };
      return Action(AC_Draw, this, &args);
   }
   inline ERROR enable() { return Action(AC_Enable, this, NULL); }
   inline ERROR focus() { return Action(AC_Focus, this, NULL); }
   inline ERROR hide() { return Action(AC_Hide, this, NULL); }
   inline ERROR init() { return Action(AC_Init, this, NULL); }
   inline ERROR redo(LONG Steps) {
      struct acRedo args = { Steps };
      return Action(AC_Redo, this, &args);
   }
   inline ERROR saveToObject(OBJECTID DestID, CLASSID ClassID) {
      struct acSaveToObject args = { { DestID }, { ClassID } };
      return Action(AC_SaveToObject, this, &args);
   }
   inline ERROR scrollToPoint(DOUBLE X, DOUBLE Y, DOUBLE Z, LONG Flags) {
      struct acScrollToPoint args = { X, Y, Z, Flags };
      return Action(AC_ScrollToPoint, this, &args);
   }
   inline ERROR show() { return Action(AC_Show, this, NULL); }
   inline ERROR undo(LONG Steps) {
      struct acUndo args = { Steps };
      return Action(AC_Undo, this, &args);
   }
};

// ScintillaSearch class definition

#define VER_SCINTILLASEARCH (1.000000)

// ScintillaSearch methods

#define MT_SsNext -1
#define MT_SsPrev -2
#define MT_SsFind -3

struct ssNext { LONG Pos;  };
struct ssPrev { LONG Pos;  };
struct ssFind { LONG Pos; LONG Flags;  };

INLINE ERROR ssNext(APTR Ob, LONG * Pos) {
   struct ssNext args = { 0 };
   ERROR error = Action(MT_SsNext, (OBJECTPTR)Ob, &args);
   if (Pos) *Pos = args.Pos;
   return(error);
}

INLINE ERROR ssPrev(APTR Ob, LONG * Pos) {
   struct ssPrev args = { 0 };
   ERROR error = Action(MT_SsPrev, (OBJECTPTR)Ob, &args);
   if (Pos) *Pos = args.Pos;
   return(error);
}

INLINE ERROR ssFind(APTR Ob, LONG * Pos, LONG Flags) {
   struct ssFind args = { 0, Flags };
   ERROR error = Action(MT_SsFind, (OBJECTPTR)Ob, &args);
   if (Pos) *Pos = args.Pos;
   return(error);
}


class objScintillaSearch : public BaseClass {
   public:
   static constexpr CLASSID CLASS_ID = ID_SCINTILLASEARCH;
   static constexpr CSTRING CLASS_NAME = "ScintillaSearch";

   using create = pf::Create<objScintillaSearch>;

   objScintilla * Scintilla;    // Targets a Scintilla object for searching.
   CSTRING Text;                // The string sequence to search for.
   LONG    Flags;               // Optional flags.
   LONG    Start;               // Start of the current/most recent selection
   LONG    End;                 // End of the current/most recent selection
};

