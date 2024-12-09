#pragma once

// Name:      scintilla.h
// Copyright: Paul Manias © 2005-2024
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

namespace sci {
struct SetFont { CSTRING Face; static const AC id = AC(-1); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };
struct ReplaceText { CSTRING Find; CSTRING Replace; STF Flags; LONG Start; LONG End; static const AC id = AC(-2); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };
struct DeleteLine { LONG Line; static const AC id = AC(-3); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };
struct SelectRange { LONG Start; LONG End; static const AC id = AC(-4); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };
struct InsertText { CSTRING String; LONG Pos; static const AC id = AC(-5); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };
struct GetLine { LONG Line; STRING Buffer; LONG Length; static const AC id = AC(-6); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };
struct ReplaceLine { LONG Line; CSTRING String; LONG Length; static const AC id = AC(-7); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };
struct GotoLine { LONG Line; static const AC id = AC(-8); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };
struct TrimWhitespace { static const AC id = AC(-9); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };
struct GetPos { LONG Line; LONG Column; LONG Pos; static const AC id = AC(-10); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };
struct ReportEvent { static const AC id = AC(-11); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };
struct ScrollToPoint { LONG X; LONG Y; static const AC id = AC(-12); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };

} // namespace

class objScintilla : public Object {
   public:
   static constexpr CLASSID CLASS_ID = CLASSID::SCINTILLA;
   static constexpr CSTRING CLASS_NAME = "Scintilla";

   using create = pf::Create<objScintilla>;

   objFont * Font;               // Refers to the font that is used for drawing text in the document.
   CSTRING   Path;               // Identifies the location of a text file to load.
   SEF       EventFlags;         // Specifies events that need to be reported from the Scintilla object.
   OBJECTID  SurfaceID;          // Refers to the Surface targeted by the Scintilla object.
   SCIF      Flags;              // Optional flags.
   OBJECTID  FocusID;            // Defines the object that is monitored for user focus changes.
   LONG      Visible;            // If TRUE, indicates the Scintilla object is visible in the target Surface.
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
   LONG      Modified;           // Returns true if the document has been modified and not saved.

   // Action stubs

   inline ERR clear() noexcept { return Action(AC::Clear, this, NULL); }
   inline ERR clipboard(CLIPMODE Mode) noexcept {
      struct acClipboard args = { Mode };
      return Action(AC::Clipboard, this, &args);
   }
   inline ERR dataFeed(OBJECTPTR Object, DATA Datatype, const void *Buffer, LONG Size) noexcept {
      struct acDataFeed args = { Object, Datatype, Buffer, Size };
      return Action(AC::DataFeed, this, &args);
   }
   inline ERR disable() noexcept { return Action(AC::Disable, this, NULL); }
   inline ERR draw() noexcept { return Action(AC::Draw, this, NULL); }
   inline ERR drawArea(LONG X, LONG Y, LONG Width, LONG Height) noexcept {
      struct acDraw args = { X, Y, Width, Height };
      return Action(AC::Draw, this, &args);
   }
   inline ERR enable() noexcept { return Action(AC::Enable, this, NULL); }
   inline ERR focus() noexcept { return Action(AC::Focus, this, NULL); }
   inline ERR hide() noexcept { return Action(AC::Hide, this, NULL); }
   inline ERR init() noexcept { return InitObject(this); }
   inline ERR redo(LONG Steps) noexcept {
      struct acRedo args = { Steps };
      return Action(AC::Redo, this, &args);
   }
   inline ERR saveToObject(OBJECTPTR Dest, CLASSID ClassID = CLASSID::NIL) noexcept {
      struct acSaveToObject args = { Dest, { ClassID } };
      return Action(AC::SaveToObject, this, &args);
   }
   inline ERR show() noexcept { return Action(AC::Show, this, NULL); }
   inline ERR undo(LONG Steps) noexcept {
      struct acUndo args = { Steps };
      return Action(AC::Undo, this, &args);
   }
   inline ERR setFont(CSTRING Face) noexcept {
      struct sci::SetFont args = { Face };
      return(Action(AC(-1), this, &args));
   }
   inline ERR replaceText(CSTRING Find, CSTRING Replace, STF Flags, LONG Start, LONG End) noexcept {
      struct sci::ReplaceText args = { Find, Replace, Flags, Start, End };
      return(Action(AC(-2), this, &args));
   }
   inline ERR deleteLine(LONG Line) noexcept {
      struct sci::DeleteLine args = { Line };
      return(Action(AC(-3), this, &args));
   }
   inline ERR selectRange(LONG Start, LONG End) noexcept {
      struct sci::SelectRange args = { Start, End };
      return(Action(AC(-4), this, &args));
   }
   inline ERR insertText(CSTRING String, LONG Pos) noexcept {
      struct sci::InsertText args = { String, Pos };
      return(Action(AC(-5), this, &args));
   }
   inline ERR getLine(LONG Line, STRING Buffer, LONG Length) noexcept {
      struct sci::GetLine args = { Line, Buffer, Length };
      return(Action(AC(-6), this, &args));
   }
   inline ERR replaceLine(LONG Line, CSTRING String, LONG Length) noexcept {
      struct sci::ReplaceLine args = { Line, String, Length };
      return(Action(AC(-7), this, &args));
   }
   inline ERR gotoLine(LONG Line) noexcept {
      struct sci::GotoLine args = { Line };
      return(Action(AC(-8), this, &args));
   }
   inline ERR trimWhitespace() noexcept {
      return(Action(AC(-9), this, NULL));
   }
   inline ERR getPos(LONG Line, LONG Column, LONG * Pos) noexcept {
      struct sci::GetPos args = { Line, Column, (LONG)0 };
      ERR error = Action(AC(-10), this, &args);
      if (Pos) *Pos = args.Pos;
      return(error);
   }
   inline ERR reportEvent() noexcept {
      return(Action(AC(-11), this, NULL));
   }
   inline ERR scrollToPoint(LONG X, LONG Y) noexcept {
      struct sci::ScrollToPoint args = { X, Y };
      return(Action(AC(-12), this, &args));
   }

   // Customised field setting

   template <class T> inline ERR setPath(T && Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[22];
      return field->WriteValue(target, field, 0x08800300, to_cstring(Value), 1);
   }

   inline ERR setEventFlags(const SEF Value) noexcept {
      this->EventFlags = Value;
      return ERR::Okay;
   }

   inline ERR setSurface(OBJECTID Value) noexcept {
      if (this->initialised()) return ERR::NoFieldAccess;
      this->SurfaceID = Value;
      return ERR::Okay;
   }

   inline ERR setFlags(const SCIF Value) noexcept {
      if (this->initialised()) return ERR::NoFieldAccess;
      this->Flags = Value;
      return ERR::Okay;
   }

   inline ERR setFocus(OBJECTID Value) noexcept {
      if (this->initialised()) return ERR::NoFieldAccess;
      this->FocusID = Value;
      return ERR::Okay;
   }

   inline ERR setVisible(const LONG Value) noexcept {
      if (this->initialised()) return ERR::NoFieldAccess;
      this->Visible = Value;
      return ERR::Okay;
   }

   inline ERR setLeftMargin(const LONG Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[32];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERR setRightMargin(const LONG Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[27];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERR setLineHighlight(const struct RGB8 * Value, LONG Elements) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[5];
      return field->WriteValue(target, field, 0x01081300, Value, Elements);
   }

   inline ERR setSelectFore(const struct RGB8 * Value, LONG Elements) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[34];
      return field->WriteValue(target, field, 0x01081500, Value, Elements);
   }

   inline ERR setSelectBkgd(const struct RGB8 * Value, LONG Elements) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[33];
      return field->WriteValue(target, field, 0x01081500, Value, Elements);
   }

   inline ERR setBkgdColour(const struct RGB8 * Value, LONG Elements) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[23];
      return field->WriteValue(target, field, 0x01081300, Value, Elements);
   }

   inline ERR setCursorColour(const struct RGB8 * Value, LONG Elements) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[0];
      return field->WriteValue(target, field, 0x01081300, Value, Elements);
   }

   inline ERR setTextColour(const struct RGB8 * Value, LONG Elements) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[24];
      return field->WriteValue(target, field, 0x01081300, Value, Elements);
   }

   inline ERR setCursorRow(const LONG Value) noexcept {
      this->CursorRow = Value;
      return ERR::Okay;
   }

   inline ERR setCursorCol(const LONG Value) noexcept {
      this->CursorCol = Value;
      return ERR::Okay;
   }

   inline ERR setLexer(const SCLEX Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[6];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERR setModified(const LONG Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[17];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERR setAllowTabs(const LONG Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[12];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERR setAutoIndent(const LONG Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[18];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERR setFileDrop(FUNCTION Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[11];
      return field->WriteValue(target, field, FD_FUNCTION, &Value, 1);
   }

   inline ERR setFoldingMarkers(const LONG Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[13];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERR setLineNumbers(const LONG Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[14];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   template <class T> inline ERR setOrigin(T && Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[9];
      return field->WriteValue(target, field, 0x08800300, to_cstring(Value), 1);
   }

   inline ERR setShowWhitespace(const LONG Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[8];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERR setEventCallback(FUNCTION Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[35];
      return field->WriteValue(target, field, FD_FUNCTION, &Value, 1);
   }

   template <class T> inline ERR setString(T && Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[10];
      return field->WriteValue(target, field, 0x08800300, to_cstring(Value), 1);
   }

   inline ERR setSymbols(const LONG Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[28];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERR setTabWidth(const LONG Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[25];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERR setWordwrap(const LONG Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[29];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

};

// ScintillaSearch class definition

#define VER_SCINTILLASEARCH (1.000000)

// ScintillaSearch methods

namespace ss {
struct Next { LONG Pos; static const AC id = AC(-1); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };
struct Prev { LONG Pos; static const AC id = AC(-2); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };
struct Find { LONG Pos; STF Flags; static const AC id = AC(-3); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };

} // namespace

class objScintillaSearch : public Object {
   public:
   static constexpr CLASSID CLASS_ID = CLASSID::SCINTILLASEARCH;
   static constexpr CSTRING CLASS_NAME = "ScintillaSearch";

   using create = pf::Create<objScintillaSearch>;

   objScintilla * Scintilla;    // Targets a Scintilla object for searching.
   CSTRING Text;                // The string sequence to search for.
   STF     Flags;               // Optional flags.
   LONG    Start;               // Start of the current/most recent selection
   LONG    End;                 // End of the current/most recent selection
   inline ERR next(LONG * Pos) noexcept {
      struct ss::Next args = { (LONG)0 };
      ERR error = Action(AC(-1), this, &args);
      if (Pos) *Pos = args.Pos;
      return(error);
   }
   inline ERR prev(LONG * Pos) noexcept {
      struct ss::Prev args = { (LONG)0 };
      ERR error = Action(AC(-2), this, &args);
      if (Pos) *Pos = args.Pos;
      return(error);
   }
   inline ERR find(LONG * Pos, STF Flags) noexcept {
      struct ss::Find args = { (LONG)0, Flags };
      ERR error = Action(AC(-3), this, &args);
      if (Pos) *Pos = args.Pos;
      return(error);
   }

   // Customised field setting

   inline ERR setScintilla(objScintilla * Value) noexcept {
      if (this->initialised()) return ERR::NoFieldAccess;
      this->Scintilla = Value;
      return ERR::Okay;
   }

   template <class T> inline ERR setText(T && Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[5];
      return field->WriteValue(target, field, 0x08800300, to_cstring(Value), 1);
   }

   inline ERR setFlags(const STF Value) noexcept {
      this->Flags = Value;
      return ERR::Okay;
   }

};

