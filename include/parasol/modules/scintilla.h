#ifndef MODULES_SCINTILLA
#define MODULES_SCINTILLA 1

// Name:      scintilla.h
// Copyright: Paul Manias © 2005-2020
// Generator: idl-c

#ifndef MAIN_H
#include <parasol/main.h>
#endif

#define MODVERSION_SCINTILLA (1)

#ifndef MODULES_DISPLAY_H
#include <parasol/modules/display.h>
#endif

#ifndef MODULES_FONT_H
#include <parasol/modules/font.h>
#endif

#ifndef MODULES_SURFACE_H
#include <parasol/modules/surface.h>
#endif

#ifndef MODULES_WIDGET_H
#include <parasol/modules/widget.h>
#endif

// Scintilla Lexers.  These codes originate from the Scintilla library.

#define SCLEX_PERL 6
#define SCLEX_ASSEMBLER 34
#define SCLEX_ASP 29
#define SCLEX_PYTHON 2
#define SCLEX_CPP 3
#define SCLEX_FLUID 15
#define SCLEX_VBSCRIPT 28
#define SCLEX_RUBY 22
#define SCLEX_SQL 7
#define SCLEX_PROPERTIES 9
#define SCLEX_BATCH 12
#define SCLEX_MAKEFILE 11
#define SCLEX_REBOL 71
#define SCLEX_PASCAL 18
#define SCLEX_HTML 4
#define SCLEX_PHPSCRIPT 69
#define SCLEX_BASH 62
#define SCLEX_CSS 38
#define SCLEX_XML 5
#define SCLEX_ERRORLIST 10
#define SCLEX_VB 8
#define SCLEX_DIFF 16

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

typedef struct rkScintilla {
   OBJECT_HEADER
   LARGE    EventFlags;          // SEF flags.  Indicates the events that will be reported.
   struct rkFont * Font;         // Font to use for the text
   CSTRING  Path;                // Source text file
   OBJECTID SurfaceID;           // The object that should be rendered to
   LONG     Flags;               // Optional flags
   OBJECTID FocusID;
   LONG     Visible;             // TRUE if the text is visible
   LONG     LeftMargin;          // Size of the left margin
   LONG     RightMargin;         // Size of the right margin
   struct RGB8 LineHighlight;    // Background colour for the current line of the cursor
   struct RGB8 SelectFore;       // Default colour for text highlighting (foreground)
   struct RGB8 SelectBkgd;       // Default colour for text highlighting (background)
   struct RGB8 BkgdColour;       // Colour for text background
   struct RGB8 CursorColour;     // The colour of the cursor
   struct RGB8 TextColour;       // The colour of foreground text
   LONG     CursorRow;           // Current cursor row
   LONG     CursorCol;           // Current cursor column
   LONG     Lexer;               // Chosen lexer
   LONG     Modified;            // TRUE if the document has been modified since last save.

#ifdef PRV_SCINTILLA
   struct  SurfaceCoords Surface;
   FUNCTION FileDrop;
   FUNCTION EventCallback;
   objFile *FileStream;
   objFont *BoldFont;        // Bold version of the current font
   objFont *ItalicFont;      // Italic version of the current font
   objFont *BIFont;          // Bold-Italic version of the current font
   ScintillaParasol *API;
   APTR   prvKeyEvent;
   STRING StringBuffer;
   LONG   LongestLine;         // Longest line in the document
   LONG   LongestWidth;        // Pixel width of the longest line
   LONG   TabWidth;
   LONG   InputHandle;
   TIMER  TimerID;
   LARGE  ReportEventFlags;    // For delayed event reporting.
   UWORD  KeyAlt:1;
   UWORD  KeyCtrl:1;
   UWORD  KeyShift:1;
   UWORD  LineNumbers:1;
   UWORD  Wordwrap:1;
   UWORD  Symbols:1;
   UWORD  FoldingMarkers:1;
   UWORD  ShowWhitespace:1;
   UWORD  AutoIndent:1;
   UWORD  HoldModify:1;
   UWORD  AllowTabs:1;
   UBYTE  ScrollLocked;
  
#endif
} objScintilla;

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


// ScintillaSearch class definition

#define VER_SCINTILLASEARCH (1.000000)

typedef struct rkScintillaSearch {
   OBJECT_HEADER
   struct rkScintilla * Scintilla;    // The targeted Scintilla object.
   CSTRING Text;                      // The text being searched for.
   LONG    Flags;                     // Optional flags affecting the search.
   LONG    Start;                     // Start of the current/most recent selection
   LONG    End;                       // End of the current/most recent selection
} objScintillaSearch;

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


#endif
