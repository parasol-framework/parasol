#pragma once

// Name:      document.h
// Copyright: Paul Manias © 2005-2024
// Generator: idl-c

#include <parasol/main.h>

#define MODVERSION_DOCUMENT (1)

#include <parasol/modules/display.h>
#include <parasol/modules/xml.h>
#include <parasol/modules/font.h>
#include <parasol/modules/vector.h>

class objDocument;

// Official version number (date format).  Any changes to the handling of document content require that this number be updated.

#define RIPL_VERSION "20240126"

enum class TT : BYTE {
   NIL = 0,
   VECTOR = 1,
   LINK = 2,
   EDIT = 3,
};

// Event flags for selectively receiving events from the Document object.

enum class DEF : ULONG {
   NIL = 0,
   PATH = 0x00000001,
   ON_CLICK = 0x00000002,
   ON_MOTION = 0x00000004,
   ON_CROSSING_IN = 0x00000008,
   ON_CROSSING_OUT = 0x00000010,
   ON_CROSSING = 0x00000018,
   LINK_ACTIVATED = 0x00000020,
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
   NO_LAYOUT_MSG = 0x00000010,
   UNRESTRICTED = 0x00000020,
};

DEFINE_ENUM_FLAG_OPERATORS(DCF)

// These are document style flags, as used in the DocStyle structure

enum class FSO : ULONG {
   NIL = 0,
   BOLD = 0x00000001,
   ITALIC = 0x00000002,
   UNDERLINE = 0x00000004,
   STYLES = 0x00000007,
   PREFORMAT = 0x00000008,
   ALIGN_RIGHT = 0x00000010,
   ALIGN_CENTER = 0x00000020,
   NO_WRAP = 0x00000040,
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

inline ERR docFeedParser(APTR Ob, CSTRING String) noexcept {
   struct docFeedParser args = { String };
   return(Action(MT_docFeedParser, (OBJECTPTR)Ob, &args));
}

inline ERR docSelectLink(APTR Ob, LONG Index, CSTRING Name) noexcept {
   struct docSelectLink args = { Index, Name };
   return(Action(MT_docSelectLink, (OBJECTPTR)Ob, &args));
}

inline ERR docFindIndex(APTR Ob, CSTRING Name, LONG * Start, LONG * End) noexcept {
   struct docFindIndex args = { Name, (LONG)0, (LONG)0 };
   ERR error = Action(MT_docFindIndex, (OBJECTPTR)Ob, &args);
   if (Start) *Start = args.Start;
   if (End) *End = args.End;
   return(error);
}

inline ERR docInsertXML(APTR Ob, CSTRING XML, LONG Index) noexcept {
   struct docInsertXML args = { XML, Index };
   return(Action(MT_docInsertXML, (OBJECTPTR)Ob, &args));
}

inline ERR docRemoveContent(APTR Ob, LONG Start, LONG End) noexcept {
   struct docRemoveContent args = { Start, End };
   return(Action(MT_docRemoveContent, (OBJECTPTR)Ob, &args));
}

inline ERR docInsertText(APTR Ob, CSTRING Text, LONG Index, LONG Char, LONG Preformat) noexcept {
   struct docInsertText args = { Text, Index, Char, Preformat };
   return(Action(MT_docInsertText, (OBJECTPTR)Ob, &args));
}

inline ERR docCallFunction(APTR Ob, CSTRING Function, struct ScriptArg * Args, LONG TotalArgs) noexcept {
   struct docCallFunction args = { Function, Args, TotalArgs };
   return(Action(MT_docCallFunction, (OBJECTPTR)Ob, &args));
}

inline ERR docAddListener(APTR Ob, DRT Trigger, FUNCTION * Function) noexcept {
   struct docAddListener args = { Trigger, Function };
   return(Action(MT_docAddListener, (OBJECTPTR)Ob, &args));
}

inline ERR docRemoveListener(APTR Ob, LONG Trigger, FUNCTION * Function) noexcept {
   struct docRemoveListener args = { Trigger, Function };
   return(Action(MT_docRemoveListener, (OBJECTPTR)Ob, &args));
}

inline ERR docShowIndex(APTR Ob, CSTRING Name) noexcept {
   struct docShowIndex args = { Name };
   return(Action(MT_docShowIndex, (OBJECTPTR)Ob, &args));
}

inline ERR docHideIndex(APTR Ob, CSTRING Name) noexcept {
   struct docHideIndex args = { Name };
   return(Action(MT_docHideIndex, (OBJECTPTR)Ob, &args));
}

inline ERR docEdit(APTR Ob, CSTRING Name, LONG Flags) noexcept {
   struct docEdit args = { Name, Flags };
   return(Action(MT_docEdit, (OBJECTPTR)Ob, &args));
}

inline ERR docReadContent(APTR Ob, DATA Format, LONG Start, LONG End, STRING * Result) noexcept {
   struct docReadContent args = { Format, Start, End, (STRING)0 };
   ERR error = Action(MT_docReadContent, (OBJECTPTR)Ob, &args);
   if (Result) *Result = args.Result;
   return(error);
}


class objDocument : public Object {
   public:
   static constexpr CLASSID CLASS_ID = ID_DOCUMENT;
   static constexpr CSTRING CLASS_NAME = "Document";

   using create = pf::Create<objDocument>;

   STRING   Description;            // A description of the document, provided by its author.
   STRING   Title;                  // The title of the document.
   STRING   Author;                 // The author(s) of the document.
   STRING   Copyright;              // Copyright information for the document.
   STRING   Keywords;               // Includes keywords declared by the source document.
   objVectorViewport * Viewport;    // A client-specific viewport that will host the document graphics.
   objVectorViewport * Focus;       // Refers to the object that will be monitored for user focusing.
   objVectorViewport * View;        // An internally created viewport that hosts the Page
   objVectorViewport * Page;        // The Page contains the document content and is hosted by the View
   OBJECTID TabFocusID;             // Allows the user to hit the tab key to focus on other GUI objects.
   DEF      EventMask;              // Specifies events that need to be reported from the Document object.
   DCF      Flags;                  // Optional flags that affect object behaviour.
   LONG     PageHeight;             // Measures the page height of the document, in pixels.
   ERR      Error;                  // The most recently generated error code.

   // Action stubs

   inline ERR activate() noexcept { return Action(AC_Activate, this, NULL); }
   inline ERR clear() noexcept { return Action(AC_Clear, this, NULL); }
   inline ERR clipboard(CLIPMODE Mode) noexcept {
      struct acClipboard args = { Mode };
      return Action(AC_Clipboard, this, &args);
   }
   inline ERR dataFeed(OBJECTPTR Object, DATA Datatype, const void *Buffer, LONG Size) noexcept {
      struct acDataFeed args = { Object, Datatype, Buffer, Size };
      return Action(AC_DataFeed, this, &args);
   }
   inline ERR disable() noexcept { return Action(AC_Disable, this, NULL); }
   inline ERR draw() noexcept { return Action(AC_Draw, this, NULL); }
   inline ERR drawArea(LONG X, LONG Y, LONG Width, LONG Height) noexcept {
      struct acDraw args = { X, Y, Width, Height };
      return Action(AC_Draw, this, &args);
   }
   inline ERR enable() noexcept { return Action(AC_Enable, this, NULL); }
   inline ERR focus() noexcept { return Action(AC_Focus, this, NULL); }
   inline ERR getKey(CSTRING Key, STRING Value, LONG Size) noexcept {
      struct acGetKey args = { Key, Value, Size };
      auto error = Action(AC_GetKey, this, &args);
      if ((error != ERR::Okay) and (Value)) Value[0] = 0;
      return error;
   }
   inline ERR init() noexcept { return InitObject(this); }
   inline ERR refresh() noexcept { return Action(AC_Refresh, this, NULL); }
   inline ERR saveToObject(OBJECTPTR Dest, CLASSID ClassID = 0) noexcept {
      struct acSaveToObject args = { Dest, { ClassID } };
      return Action(AC_SaveToObject, this, &args);
   }
   inline ERR acSetKey(CSTRING FieldName, CSTRING Value) noexcept {
      struct acSetKey args = { FieldName, Value };
      return Action(AC_SetKey, this, &args);
   }

   // Customised field setting

   inline ERR setViewport(objVectorViewport * Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[23];
      return field->WriteValue(target, field, 0x08000301, Value, 1);
   }

   inline ERR setFocus(objVectorViewport * Value) noexcept {
      if (this->initialised()) return ERR::NoFieldAccess;
      this->Focus = Value;
      return ERR::Okay;
   }

   inline ERR setTabFocus(OBJECTID Value) noexcept {
      this->TabFocusID = Value;
      return ERR::Okay;
   }

   inline ERR setEventMask(const DEF Value) noexcept {
      this->EventMask = Value;
      return ERR::Okay;
   }

   inline ERR setFlags(const DCF Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[3];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERR setClientScript(OBJECTPTR Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[18];
      return field->WriteValue(target, field, 0x08000401, Value, 1);
   }

   inline ERR setEventCallback(FUNCTION Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[25];
      return field->WriteValue(target, field, FD_FUNCTION, &Value, 1);
   }

   template <class T> inline ERR setPath(T && Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[13];
      return field->WriteValue(target, field, 0x08800300, to_cstring(Value), 1);
   }

   template <class T> inline ERR setOrigin(T && Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[7];
      return field->WriteValue(target, field, 0x08800300, to_cstring(Value), 1);
   }

   inline ERR setPageWidth(const LONG Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[10];
      Variable var(Value);
      return field->WriteValue(target, field, FD_VARIABLE, &var, 1);
   }

   template <class T> inline ERR setPretext(T && Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[20];
      return field->WriteValue(target, field, 0x08800200, to_cstring(Value), 1);
   }

};

namespace fl {
   using namespace pf;

constexpr FieldValue EventCallback(const FUNCTION &Value) { return FieldValue(FID_EventCallback, &Value); }
constexpr FieldValue EventCallback(APTR Value) { return FieldValue(FID_EventCallback, Value); }
constexpr FieldValue EventMask(DEF Value) { return FieldValue(FID_EventMask, LONG(Value)); }
constexpr FieldValue Flags(DCF Value) { return FieldValue(FID_Flags, LONG(Value)); }

}

