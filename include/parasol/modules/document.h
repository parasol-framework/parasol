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

namespace doc {
struct FeedParser { CSTRING String; static const ACTIONID id = -1; ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };
struct SelectLink { LONG Index; CSTRING Name; static const ACTIONID id = -2; ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };
struct FindIndex { CSTRING Name; LONG Start; LONG End; static const ACTIONID id = -4; ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };
struct InsertXML { CSTRING XML; LONG Index; static const ACTIONID id = -5; ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };
struct RemoveContent { LONG Start; LONG End; static const ACTIONID id = -6; ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };
struct InsertText { CSTRING Text; LONG Index; LONG Char; LONG Preformat; static const ACTIONID id = -7; ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };
struct CallFunction { CSTRING Function; struct ScriptArg * Args; LONG TotalArgs; static const ACTIONID id = -8; ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };
struct AddListener { DRT Trigger; FUNCTION * Function; static const ACTIONID id = -9; ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };
struct RemoveListener { LONG Trigger; FUNCTION * Function; static const ACTIONID id = -10; ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };
struct ShowIndex { CSTRING Name; static const ACTIONID id = -11; ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };
struct HideIndex { CSTRING Name; static const ACTIONID id = -12; ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };
struct Edit { CSTRING Name; LONG Flags; static const ACTIONID id = -13; ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };
struct ReadContent { DATA Format; LONG Start; LONG End; STRING Result; static const ACTIONID id = -14; ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };

} // namespace

class objDocument : public Object {
   public:
   static constexpr CLASSID CLASS_ID = CLASSID::DOCUMENT;
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
   inline ERR saveToObject(OBJECTPTR Dest, CLASSID ClassID = CLASSID::NIL) noexcept {
      struct acSaveToObject args = { Dest, { ClassID } };
      return Action(AC_SaveToObject, this, &args);
   }
   inline ERR acSetKey(CSTRING FieldName, CSTRING Value) noexcept {
      struct acSetKey args = { FieldName, Value };
      return Action(AC_SetKey, this, &args);
   }
   inline ERR feedParser(CSTRING String) noexcept {
      struct doc::FeedParser args = { String };
      return(Action(-1, this, &args));
   }
   inline ERR selectLink(LONG Index, CSTRING Name) noexcept {
      struct doc::SelectLink args = { Index, Name };
      return(Action(-2, this, &args));
   }
   inline ERR findIndex(CSTRING Name, LONG * Start, LONG * End) noexcept {
      struct doc::FindIndex args = { Name, (LONG)0, (LONG)0 };
      ERR error = Action(-4, this, &args);
      if (Start) *Start = args.Start;
      if (End) *End = args.End;
      return(error);
   }
   inline ERR insertXML(CSTRING XML, LONG Index) noexcept {
      struct doc::InsertXML args = { XML, Index };
      return(Action(-5, this, &args));
   }
   inline ERR removeContent(LONG Start, LONG End) noexcept {
      struct doc::RemoveContent args = { Start, End };
      return(Action(-6, this, &args));
   }
   inline ERR insertText(CSTRING Text, LONG Index, LONG Char, LONG Preformat) noexcept {
      struct doc::InsertText args = { Text, Index, Char, Preformat };
      return(Action(-7, this, &args));
   }
   inline ERR callFunction(CSTRING Function, struct ScriptArg * Args, LONG TotalArgs) noexcept {
      struct doc::CallFunction args = { Function, Args, TotalArgs };
      return(Action(-8, this, &args));
   }
   inline ERR addListener(DRT Trigger, FUNCTION * Function) noexcept {
      struct doc::AddListener args = { Trigger, Function };
      return(Action(-9, this, &args));
   }
   inline ERR removeListener(LONG Trigger, FUNCTION * Function) noexcept {
      struct doc::RemoveListener args = { Trigger, Function };
      return(Action(-10, this, &args));
   }
   inline ERR showIndex(CSTRING Name) noexcept {
      struct doc::ShowIndex args = { Name };
      return(Action(-11, this, &args));
   }
   inline ERR hideIndex(CSTRING Name) noexcept {
      struct doc::HideIndex args = { Name };
      return(Action(-12, this, &args));
   }
   inline ERR edit(CSTRING Name, LONG Flags) noexcept {
      struct doc::Edit args = { Name, Flags };
      return(Action(-13, this, &args));
   }
   inline ERR readContent(DATA Format, LONG Start, LONG End, STRING * Result) noexcept {
      struct doc::ReadContent args = { Format, Start, End, (STRING)0 };
      ERR error = Action(-14, this, &args);
      if (Result) *Result = args.Result;
      return(error);
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

