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

enum class SCLEX : LONG {
   NIL = 0,
   ERRORLIST = 10,
   MAKEFILE = 11,
   BATCH = 12,
   FLUID = 15,
   DIFF = 16,
   PASCAL = 18,
   RUBY = 22,
   VBSCRIPT = 28,
   ASP = 29,
   PYTHON = 2,
   ASSEMBLER = 34,
   CSS = 38,
   CPP = 3,
   HTML = 4,
   XML = 5,
   BASH = 62,
   PHPSCRIPT = 69,
   PERL = 6,
   REBOL = 71,
   SQL = 7,
   VB = 8,
   PROPERTIES = 9,
};

// Optional flags.

enum class SCIF : ULONG {
   NIL = 0,
   DISABLED = 0x00000001,
   DETECT_LEXER = 0x00000002,
   EDIT = 0x00000004,
   EXT_PAGE = 0x00000008,
};

DEFINE_ENUM_FLAG_OPERATORS(SCIF)

// Flags for EventCallback and EventFlags

enum class SEF : ULONG {
   NIL = 0,
   MODIFIED = 0x00000001,
   CURSOR_POS = 0x00000002,
   FAIL_RO = 0x00000004,
   NEW_CHAR = 0x00000008,
};

DEFINE_ENUM_FLAG_OPERATORS(SEF)

// Scintilla search flags.

enum class STF : ULONG {
   NIL = 0,
   CASE = 0x00000001,
   MOVE_CURSOR = 0x00000002,
   SCAN_SELECTION = 0x00000004,
   BACKWARDS = 0x00000008,
   EXPRESSION = 0x00000010,
   WRAP = 0x00000020,
};

DEFINE_ENUM_FLAG_OPERATORS(STF)

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
struct sciReplaceText { CSTRING Find; CSTRING Replace; STF Flags; LONG Start; LONG End;  };
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

INLINE ERROR sciReplaceText(APTR Ob, CSTRING Find, CSTRING Replace, STF Flags, LONG Start, LONG End) {
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
   struct sciGetPos args = { Line, Column, (LONG)0 };
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

   SEF       EventFlags;         // Specifies events that need to be reported from the Scintilla object.
   objFont * Font;               // Refers to the font that is used for drawing text in the document.
   CSTRING   Path;               // Identifies the location of a text file to load.
   OBJECTID  SurfaceID;          // Refers to the @Surface targeted by the Scintilla object.
   SCIF      Flags;              // Optional flags.
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
   SCLEX     Lexer;              // The lexer for document styling is defined here.
   LONG      Modified;           // Returns TRUE if the document has been modified and not saved.

   // Action stubs

   inline ERROR clear() { return Action(AC_Clear, this, NULL); }
   inline ERROR clipboard(CLIPMODE Mode) {
      struct acClipboard args = { Mode };
      return Action(AC_Clipboard, this, &args);
   }
   inline ERROR dataFeed(OBJECTPTR Object, DATA Datatype, const void *Buffer, LONG Size) {
      struct acDataFeed args = { Object, Datatype, Buffer, Size };
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
   inline ERROR init() { return InitObject(this); }
   inline ERROR redo(LONG Steps) {
      struct acRedo args = { Steps };
      return Action(AC_Redo, this, &args);
   }
   inline ERROR saveToObject(OBJECTPTR Dest, CLASSID ClassID = 0) {
      struct acSaveToObject args = { Dest, { ClassID } };
      return Action(AC_SaveToObject, this, &args);
   }
   inline ERROR scrollToPoint(DOUBLE X, DOUBLE Y, DOUBLE Z, STP Flags) {
      struct acScrollToPoint args = { X, Y, Z, Flags };
      return Action(AC_ScrollToPoint, this, &args);
   }
   inline ERROR show() { return Action(AC_Show, this, NULL); }
   inline ERROR undo(LONG Steps) {
      struct acUndo args = { Steps };
      return Action(AC_Undo, this, &args);
   }

   // Customised field setting

   inline ERROR setEventFlags(const SEF Value) {
      this->EventFlags = Value;
      return ERR_Okay;
   }

   template <class T> inline ERROR setPath(T && Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[22];
      return field->WriteValue(target, field, 0x08800300, to_cstring(Value), 1);
   }

   inline ERROR setSurface(const OBJECTID Value) {
      if (this->initialised()) return ERR_NoFieldAccess;
      this->SurfaceID = Value;
      return ERR_Okay;
   }

   inline ERROR setFlags(const SCIF Value) {
      if (this->initialised()) return ERR_NoFieldAccess;
      this->Flags = Value;
      return ERR_Okay;
   }

   inline ERROR setFocus(const OBJECTID Value) {
      if (this->initialised()) return ERR_NoFieldAccess;
      this->FocusID = Value;
      return ERR_Okay;
   }

   inline ERROR setVisible(const LONG Value) {
      if (this->initialised()) return ERR_NoFieldAccess;
      this->Visible = Value;
      return ERR_Okay;
   }

   inline ERROR setLeftMargin(const LONG Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[32];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERROR setRightMargin(const LONG Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[27];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERROR setLineHighlight(const struct RGB8 * Value, LONG Elements) {
      auto target = this;
      auto field = &this->Class->Dictionary[5];
      return field->WriteValue(target, field, 0x01081300, Value, Elements);
   }

   inline ERROR setSelectFore(const struct RGB8 * Value, LONG Elements) {
      auto target = this;
      auto field = &this->Class->Dictionary[34];
      return field->WriteValue(target, field, 0x01081500, Value, Elements);
   }

   inline ERROR setSelectBkgd(const struct RGB8 * Value, LONG Elements) {
      auto target = this;
      auto field = &this->Class->Dictionary[33];
      return field->WriteValue(target, field, 0x01081500, Value, Elements);
   }

   inline ERROR setBkgdColour(const struct RGB8 * Value, LONG Elements) {
      auto target = this;
      auto field = &this->Class->Dictionary[23];
      return field->WriteValue(target, field, 0x01081300, Value, Elements);
   }

   inline ERROR setCursorColour(const struct RGB8 * Value, LONG Elements) {
      auto target = this;
      auto field = &this->Class->Dictionary[0];
      return field->WriteValue(target, field, 0x01081300, Value, Elements);
   }

   inline ERROR setTextColour(const struct RGB8 * Value, LONG Elements) {
      auto target = this;
      auto field = &this->Class->Dictionary[24];
      return field->WriteValue(target, field, 0x01081300, Value, Elements);
   }

   inline ERROR setCursorRow(const LONG Value) {
      this->CursorRow = Value;
      return ERR_Okay;
   }

   inline ERROR setCursorCol(const LONG Value) {
      this->CursorCol = Value;
      return ERR_Okay;
   }

   inline ERROR setLexer(const SCLEX Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[6];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERROR setModified(const LONG Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[17];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERROR setAllowTabs(const LONG Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[12];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERROR setAutoIndent(const LONG Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[18];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERROR setFileDrop(FUNCTION Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[11];
      return field->WriteValue(target, field, FD_FUNCTION, &Value, 1);
   }

   inline ERROR setFoldingMarkers(const LONG Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[13];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERROR setLineNumbers(const LONG Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[14];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   template <class T> inline ERROR setOrigin(T && Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[9];
      return field->WriteValue(target, field, 0x08800300, to_cstring(Value), 1);
   }

   inline ERROR setShowWhitespace(const LONG Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[8];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERROR setEventCallback(FUNCTION Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[35];
      return field->WriteValue(target, field, FD_FUNCTION, &Value, 1);
   }

   template <class T> inline ERROR setString(T && Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[10];
      return field->WriteValue(target, field, 0x08800300, to_cstring(Value), 1);
   }

   inline ERROR setSymbols(const LONG Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[28];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERROR setTabWidth(const LONG Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[25];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERROR setWordwrap(const LONG Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[29];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
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
struct ssFind { LONG Pos; STF Flags;  };

INLINE ERROR ssNext(APTR Ob, LONG * Pos) {
   struct ssNext args = { (LONG)0 };
   ERROR error = Action(MT_SsNext, (OBJECTPTR)Ob, &args);
   if (Pos) *Pos = args.Pos;
   return(error);
}

INLINE ERROR ssPrev(APTR Ob, LONG * Pos) {
   struct ssPrev args = { (LONG)0 };
   ERROR error = Action(MT_SsPrev, (OBJECTPTR)Ob, &args);
   if (Pos) *Pos = args.Pos;
   return(error);
}

INLINE ERROR ssFind(APTR Ob, LONG * Pos, STF Flags) {
   struct ssFind args = { (LONG)0, Flags };
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
   STF     Flags;               // Optional flags.
   LONG    Start;               // Start of the current/most recent selection
   LONG    End;                 // End of the current/most recent selection

   // Customised field setting

   inline ERROR setScintilla(objScintilla * Value) {
      if (this->initialised()) return ERR_NoFieldAccess;
      this->Scintilla = Value;
      return ERR_Okay;
   }

   template <class T> inline ERROR setText(T && Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[5];
      return field->WriteValue(target, field, 0x08800300, to_cstring(Value), 1);
   }

   inline ERROR setFlags(const STF Value) {
      this->Flags = Value;
      return ERR_Okay;
   }

};

