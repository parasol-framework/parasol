#pragma once

// Name:      document.h
// Copyright: Paul Manias © 2005-2023
// Generator: idl-c

#include <parasol/main.h>

#define MODVERSION_DOCUMENT (1)

#include <parasol/modules/display.h>
#include <parasol/modules/xml.h>
#include <parasol/modules/font.h>
#include <parasol/modules/vector.h>

class objDocument;

// Official version number (date format).  Any changes to the handling of document content require that this number be updated.

#define RIPPLE_VERSION "20231021"

#define TT_OBJECT 1
#define TT_LINK 2
#define TT_EDIT 3

// Event flags for selectively receiving events from the Document object.

enum class DEF : ULONG {
   NIL = 0,
   PATH = 0x00000001,
   LINK_ACTIVATED = 0x00000002,
};

DEFINE_ENUM_FLAG_OPERATORS(DEF)

// Internal trigger codes

enum class DRT : LONG {
   NIL = 0,
   BEFORE_LAYOUT = 0,
   AFTER_LAYOUT = 1,
   USER_CLICK = 2,
   USER_CLICK_RELEASE = 3,
   USER_MOVEMENT = 4,
   REFRESH = 5,
   GOT_FOCUS = 6,
   LOST_FOCUS = 7,
   LEAVING_PAGE = 8,
   PAGE_PROCESSED = 9,
   MAX = 10,
};

// Document flags

enum class DCF : ULONG {
   NIL = 0,
   EDIT = 0x00000001,
   OVERWRITE = 0x00000002,
   NO_SYS_KEYS = 0x00000004,
   DISABLED = 0x00000008,
   NO_SCROLLBARS = 0x00000010,
   NO_LAYOUT_MSG = 0x00000020,
   UNRESTRICTED = 0x00000040,
};

DEFINE_ENUM_FLAG_OPERATORS(DCF)

// Border edge flags.

enum class DBE : ULONG {
   NIL = 0,
   TOP = 0x00000001,
   LEFT = 0x00000002,
   RIGHT = 0x00000004,
   BOTTOM = 0x00000008,
};

DEFINE_ENUM_FLAG_OPERATORS(DBE)

// These are document style flags, as used in the DocStyle structure

enum class FSO : ULONG {
   NIL = 0,
   BOLD = 0x00000001,
   ITALIC = 0x00000002,
   UNDERLINE = 0x00000004,
   PREFORMAT = 0x00000008,
   CAPS = 0x00000010,
   STYLES = 0x00000017,
   ALIGN_RIGHT = 0x00000020,
   ALIGN_CENTER = 0x00000040,
   IN_LINE = 0x00000080,
   NO_WRAP = 0x00000100,
};

DEFINE_ENUM_FLAG_OPERATORS(FSO)

// Document class definition

#define VER_DOCUMENT (1.000000)

// Document methods

#define MT_docFeedParser -1
#define MT_docSelectLink -2
#define MT_docFindIndex -4
#define MT_docInsertXML -5
#define MT_docRemoveContent -6
#define MT_docInsertText -7
#define MT_docCallFunction -8
#define MT_docAddListener -9
#define MT_docRemoveListener -10
#define MT_docShowIndex -11
#define MT_docHideIndex -12
#define MT_docEdit -13
#define MT_docReadContent -14

struct docFeedParser { CSTRING String;  };
struct docSelectLink { LONG Index; CSTRING Name;  };
struct docFindIndex { CSTRING Name; LONG Start; LONG End;  };
struct docInsertXML { CSTRING XML; LONG Index;  };
struct docRemoveContent { LONG Start; LONG End;  };
struct docInsertText { CSTRING Text; LONG Index; LONG Char; LONG Preformat;  };
struct docCallFunction { CSTRING Function; struct ScriptArg * Args; LONG TotalArgs;  };
struct docAddListener { DRT Trigger; FUNCTION * Function;  };
struct docRemoveListener { LONG Trigger; FUNCTION * Function;  };
struct docShowIndex { CSTRING Name;  };
struct docHideIndex { CSTRING Name;  };
struct docEdit { CSTRING Name; LONG Flags;  };
struct docReadContent { DATA Format; LONG Start; LONG End; STRING Result;  };

INLINE ERROR docFeedParser(APTR Ob, CSTRING String) {
   struct docFeedParser args = { String };
   return(Action(MT_docFeedParser, (OBJECTPTR)Ob, &args));
}

INLINE ERROR docSelectLink(APTR Ob, LONG Index, CSTRING Name) {
   struct docSelectLink args = { Index, Name };
   return(Action(MT_docSelectLink, (OBJECTPTR)Ob, &args));
}

INLINE ERROR docFindIndex(APTR Ob, CSTRING Name, LONG * Start, LONG * End) {
   struct docFindIndex args = { Name, (LONG)0, (LONG)0 };
   ERROR error = Action(MT_docFindIndex, (OBJECTPTR)Ob, &args);
   if (Start) *Start = args.Start;
   if (End) *End = args.End;
   return(error);
}

INLINE ERROR docInsertXML(APTR Ob, CSTRING XML, LONG Index) {
   struct docInsertXML args = { XML, Index };
   return(Action(MT_docInsertXML, (OBJECTPTR)Ob, &args));
}

INLINE ERROR docRemoveContent(APTR Ob, LONG Start, LONG End) {
   struct docRemoveContent args = { Start, End };
   return(Action(MT_docRemoveContent, (OBJECTPTR)Ob, &args));
}

INLINE ERROR docInsertText(APTR Ob, CSTRING Text, LONG Index, LONG Char, LONG Preformat) {
   struct docInsertText args = { Text, Index, Char, Preformat };
   return(Action(MT_docInsertText, (OBJECTPTR)Ob, &args));
}

INLINE ERROR docCallFunction(APTR Ob, CSTRING Function, struct ScriptArg * Args, LONG TotalArgs) {
   struct docCallFunction args = { Function, Args, TotalArgs };
   return(Action(MT_docCallFunction, (OBJECTPTR)Ob, &args));
}

INLINE ERROR docAddListener(APTR Ob, DRT Trigger, FUNCTION * Function) {
   struct docAddListener args = { Trigger, Function };
   return(Action(MT_docAddListener, (OBJECTPTR)Ob, &args));
}

INLINE ERROR docRemoveListener(APTR Ob, LONG Trigger, FUNCTION * Function) {
   struct docRemoveListener args = { Trigger, Function };
   return(Action(MT_docRemoveListener, (OBJECTPTR)Ob, &args));
}

INLINE ERROR docShowIndex(APTR Ob, CSTRING Name) {
   struct docShowIndex args = { Name };
   return(Action(MT_docShowIndex, (OBJECTPTR)Ob, &args));
}

INLINE ERROR docHideIndex(APTR Ob, CSTRING Name) {
   struct docHideIndex args = { Name };
   return(Action(MT_docHideIndex, (OBJECTPTR)Ob, &args));
}

INLINE ERROR docEdit(APTR Ob, CSTRING Name, LONG Flags) {
   struct docEdit args = { Name, Flags };
   return(Action(MT_docEdit, (OBJECTPTR)Ob, &args));
}

INLINE ERROR docReadContent(APTR Ob, DATA Format, LONG Start, LONG End, STRING * Result) {
   struct docReadContent args = { Format, Start, End, (STRING)0 };
   ERROR error = Action(MT_docReadContent, (OBJECTPTR)Ob, &args);
   if (Result) *Result = args.Result;
   return(error);
}


class objDocument : public BaseClass {
   public:
   static constexpr CLASSID CLASS_ID = ID_DOCUMENT;
   static constexpr CSTRING CLASS_NAME = "Document";

   using create = pf::Create<objDocument>;

   STRING   Description;            // A description of the document, provided by its author.
   STRING   FontFace;               // Defines the default font face.
   STRING   Title;                  // The title of the document.
   STRING   Author;                 // The author(s) of the document.
   STRING   Copyright;              // Copyright information for the document.
   STRING   Keywords;               // Includes keywords declared by the source document.
   STRING   Highlight;              // Defines the fill used to highlight the document.
   STRING   Background;             // Optional background colour for the document.
   STRING   CursorStroke;           // The colour used for the document cursor.
   STRING   BorderStroke;           // The stroke to use for drawing a border around the document window.
   objVectorViewport * Viewport;    // A target viewport that will host the document graphics.
   objVectorViewport * Focus;       // Refers to the object that will be monitored for user focusing.
   OBJECTID TabFocusID;             // Allows the user to hit the tab key to focus on other GUI objects.
   DEF      EventMask;              // Specifies events that need to be reported from the Document object.
   DCF      Flags;                  // Optional flags that affect object behaviour.
   LONG     FontSize;               // The point-size of the default font.
   LONG     PageHeight;             // Measures the page height of the document, in pixels.
   DBE      BorderEdge;             // Border edge flags.
   LONG     LineHeight;             // Default line height (taken as an average) for all text on the page.
   ERROR    Error;                  // The most recently generated error code.

   // Action stubs

   inline ERROR activate() { return Action(AC_Activate, this, NULL); }
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
   inline ERROR getVar(CSTRING FieldName, STRING Buffer, LONG Size) {
      struct acGetVar args = { FieldName, Buffer, Size };
      ERROR error = Action(AC_GetVar, this, &args);
      if ((error) and (Buffer)) Buffer[0] = 0;
      return error;
   }
   inline ERROR init() { return InitObject(this); }
   inline ERROR refresh() { return Action(AC_Refresh, this, NULL); }
   inline ERROR saveToObject(OBJECTPTR Dest, CLASSID ClassID = 0) {
      struct acSaveToObject args = { Dest, { ClassID } };
      return Action(AC_SaveToObject, this, &args);
   }
   inline ERROR scrollToPoint(DOUBLE X, DOUBLE Y, DOUBLE Z, STP Flags) {
      struct acScrollToPoint args = { X, Y, Z, Flags };
      return Action(AC_ScrollToPoint, this, &args);
   }
   inline ERROR acSetVar(CSTRING FieldName, CSTRING Value) {
      struct acSetVar args = { FieldName, Value };
      return Action(AC_SetVar, this, &args);
   }

   // Customised field setting

   template <class T> inline ERROR setFontFace(T && Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[0];
      return field->WriteValue(target, field, 0x08800300, to_cstring(Value), 1);
   }

   template <class T> inline ERROR setHighlight(T && Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[10];
      return field->WriteValue(target, field, 0x08800300, to_cstring(Value), 1);
   }

   template <class T> inline ERROR setBackground(T && Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[16];
      return field->WriteValue(target, field, 0x08800300, to_cstring(Value), 1);
   }

   template <class T> inline ERROR setCursorStroke(T && Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[11];
      return field->WriteValue(target, field, 0x08800300, to_cstring(Value), 1);
   }

   template <class T> inline ERROR setBorderStroke(T && Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[25];
      return field->WriteValue(target, field, 0x08800300, to_cstring(Value), 1);
   }

   inline ERROR setViewport(objVectorViewport * Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[28];
      return field->WriteValue(target, field, 0x08000301, Value, 1);
   }

   inline ERROR setFocus(objVectorViewport * Value) {
      if (this->initialised()) return ERR_NoFieldAccess;
      this->Focus = Value;
      return ERR_Okay;
   }

   inline ERROR setTabFocus(OBJECTID Value) {
      this->TabFocusID = Value;
      return ERR_Okay;
   }

   inline ERROR setEventMask(const DEF Value) {
      this->EventMask = Value;
      return ERR_Okay;
   }

   inline ERROR setFlags(const DCF Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[5];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERROR setFontSize(const LONG Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[1];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERROR setBorderEdge(const DBE Value) {
      if (this->initialised()) return ERR_NoFieldAccess;
      this->BorderEdge = Value;
      return ERR_Okay;
   }

   inline ERROR setDefaultScript(OBJECTPTR Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[14];
      return field->WriteValue(target, field, 0x08000401, Value, 1);
   }

   inline ERROR setEventCallback(FUNCTION Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[30];
      return field->WriteValue(target, field, FD_FUNCTION, &Value, 1);
   }

   template <class T> inline ERROR setPath(T && Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[18];
      return field->WriteValue(target, field, 0x08800300, to_cstring(Value), 1);
   }

   template <class T> inline ERROR setOrigin(T && Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[9];
      return field->WriteValue(target, field, 0x08800300, to_cstring(Value), 1);
   }

   inline ERROR setPageWidth(const LONG Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[15];
      Variable var(Value);
      return field->WriteValue(target, field, FD_VARIABLE, &var, 1);
   }

};

