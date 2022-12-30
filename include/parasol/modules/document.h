#pragma once

// Name:      document.h
// Copyright: Paul Manias © 2005-2022
// Generator: idl-c

#include <parasol/main.h>

#define MODVERSION_DOCUMENT (1)

#include <parasol/modules/display.h>
#include <parasol/modules/xml.h>
#include <parasol/modules/font.h>

class objDocument;

// Official version number (date format).  Any changes to the handling of document content require that this number be updated.

#define RIPPLE_VERSION "20160601"

#define TT_OBJECT 1
#define TT_LINK 2
#define TT_EDIT 3

// Event flags for selectively receiving events from the Document object.

#define DEF_PATH 0x00000001
#define DEF_LINK_ACTIVATED 0x00000002

// Internal trigger codes

#define DRT_BEFORE_LAYOUT 0
#define DRT_AFTER_LAYOUT 1
#define DRT_USER_CLICK 2
#define DRT_USER_CLICK_RELEASE 3
#define DRT_USER_MOVEMENT 4
#define DRT_REFRESH 5
#define DRT_GOT_FOCUS 6
#define DRT_LOST_FOCUS 7
#define DRT_LEAVING_PAGE 8
#define DRT_PAGE_PROCESSED 9
#define DRT_MAX 10

// Document flags

#define DCF_EDIT 0x00000001
#define DCF_OVERWRITE 0x00000002
#define DCF_NO_SYS_KEYS 0x00000004
#define DCF_DISABLED 0x00000008
#define DCF_NO_SCROLLBARS 0x00000010
#define DCF_NO_LAYOUT_MSG 0x00000020
#define DCF_UNRESTRICTED 0x00000040

// Border edge flags.

#define DBE_TOP 0x00000001
#define DBE_LEFT 0x00000002
#define DBE_RIGHT 0x00000004
#define DBE_BOTTOM 0x00000008

// These are document style flags, as used in the DocStyle structure

#define FSO_BOLD 0x00000001
#define FSO_ITALIC 0x00000002
#define FSO_UNDERLINE 0x00000004
#define FSO_PREFORMAT 0x00000008
#define FSO_CAPS 0x00000010
#define FSO_STYLES 0x00000017
#define FSO_ALIGN_RIGHT 0x00000020
#define FSO_ALIGN_CENTER 0x00000040
#define FSO_ANCHOR 0x00000080
#define FSO_NO_WRAP 0x00000100

#define VER_DOCSTYLE 1

typedef struct DocStyleV1 {
   LONG      Version;            // Version of this DocStyle structure
   objDocument * Document;       // The document object that this style originates from
   objFont * Font;               // Pointer to the current font object.  Indicates face, style etc, but not simple attributes like colour
   struct RGB8 FontColour;       // Foreground colour (colour of the font)
   struct RGB8 FontUnderline;    // Underline colour for the font, if active
   LONG      StyleFlags;         // Font style flags (FSO)
} DOCSTYLE;

struct deLinkActivated {
   struct KeyStore * Parameters;    // All key-values associated with the link.
};

struct escFont {
   WORD Index;            // Font lookup
   WORD Options;          // FSO flags
   struct RGB8 Colour;    // Font colour
};

typedef struct escFont escFont;
struct SurfaceClip {
   struct SurfaceClip * Next;
   LONG Left;
   LONG Top;
   LONG Right;
   LONG Bottom;
};

struct style_status {
   struct escFont FontStyle;
   struct process_table * Table;
   struct escList * List;
   char  Face[36];
   WORD  Point;
   UBYTE FontChange:1;              // A major font change has occurred (e.g. face, point size)
   UBYTE StyleChange:1;             // A minor style change has occurred (e.g. font colour)
};

struct docdraw {
   APTR     Object;
   OBJECTID ID;
};

struct DocTrigger {
   struct DocTrigger * Next;
   struct DocTrigger * Prev;
   FUNCTION Function;
};

// Document class definition

#define VER_DOCUMENT (1.000000)

// Document methods

#define MT_docFeedParser -1
#define MT_docSelectLink -2
#define MT_docApplyFontStyle -3
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
struct docApplyFontStyle { struct DocStyleV1 * Style; objFont * Font;  };
struct docFindIndex { CSTRING Name; LONG Start; LONG End;  };
struct docInsertXML { CSTRING XML; LONG Index;  };
struct docRemoveContent { LONG Start; LONG End;  };
struct docInsertText { CSTRING Text; LONG Index; LONG Preformat;  };
struct docCallFunction { CSTRING Function; struct ScriptArg * Args; LONG TotalArgs;  };
struct docAddListener { LONG Trigger; FUNCTION * Function;  };
struct docRemoveListener { LONG Trigger; FUNCTION * Function;  };
struct docShowIndex { CSTRING Name;  };
struct docHideIndex { CSTRING Name;  };
struct docEdit { CSTRING Name; LONG Flags;  };
struct docReadContent { LONG Format; LONG Start; LONG End; STRING Result;  };

INLINE ERROR docFeedParser(APTR Ob, CSTRING String) {
   struct docFeedParser args = { String };
   return(Action(MT_docFeedParser, (OBJECTPTR)Ob, &args));
}

INLINE ERROR docSelectLink(APTR Ob, LONG Index, CSTRING Name) {
   struct docSelectLink args = { Index, Name };
   return(Action(MT_docSelectLink, (OBJECTPTR)Ob, &args));
}

INLINE ERROR docApplyFontStyle(APTR Ob, struct DocStyleV1 * Style, objFont * Font) {
   struct docApplyFontStyle args = { Style, Font };
   return(Action(MT_docApplyFontStyle, (OBJECTPTR)Ob, &args));
}

INLINE ERROR docFindIndex(APTR Ob, CSTRING Name, LONG * Start, LONG * End) {
   struct docFindIndex args = { Name, 0, 0 };
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

INLINE ERROR docInsertText(APTR Ob, CSTRING Text, LONG Index, LONG Preformat) {
   struct docInsertText args = { Text, Index, Preformat };
   return(Action(MT_docInsertText, (OBJECTPTR)Ob, &args));
}

INLINE ERROR docCallFunction(APTR Ob, CSTRING Function, struct ScriptArg * Args, LONG TotalArgs) {
   struct docCallFunction args = { Function, Args, TotalArgs };
   return(Action(MT_docCallFunction, (OBJECTPTR)Ob, &args));
}

INLINE ERROR docAddListener(APTR Ob, LONG Trigger, FUNCTION * Function) {
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

INLINE ERROR docReadContent(APTR Ob, LONG Format, LONG Start, LONG End, STRING * Result) {
   struct docReadContent args = { Format, Start, End, 0 };
   ERROR error = Action(MT_docReadContent, (OBJECTPTR)Ob, &args);
   if (Result) *Result = args.Result;
   return(error);
}


class objDocument : public BaseClass {
   public:
   static constexpr CLASSID CLASS_ID = ID_DOCUMENT;
   static constexpr CSTRING CLASS_NAME = "Document";

   using create = parasol::Create<objDocument>;

   LARGE    EventMask;        // Specifies events that need to be reported from the Document object.
   STRING   Description;      // A description of the document, provided by its author.
   STRING   FontFace;         // Defines the default font face.
   STRING   Title;            // The title of the document.
   STRING   Author;           // The author(s) of the document.
   STRING   Copyright;        // Copyright information for the document.
   STRING   Keywords;         // Includes keywords declared by the source document.
   OBJECTID TabFocusID;       // Allows the user to hit the tab key to focus on other GUI objects.
   OBJECTID SurfaceID;        // Defines the surface area for document graphics.
   OBJECTID FocusID;          // Refers to the object that will be monitored for user focusing.
   LONG     Flags;            // Optional flags that affect object behaviour.
   LONG     LeftMargin;       // Defines the amount of whitespace to leave at the left of the page.
   LONG     TopMargin;        // Defines the amount of white-space to leave at the top of the document page.
   LONG     RightMargin;      // Defines the amount of white-space to leave at the right side of the document page.
   LONG     BottomMargin;     // Defines the amount of whitespace to leave at the bottom of the document page.
   LONG     FontSize;         // The point-size of the default font.
   LONG     PageHeight;       // Measures the page height of the document, in pixels.
   LONG     BorderEdge;       // Border edge flags.
   LONG     LineHeight;       // Default line height (taken as an average) for all text on the page.
   ERROR    Error;            // The most recently generated error code.
   struct RGB8 FontColour;    // Default font colour.
   struct RGB8 Highlight;     // Defines the colour used to highlight document.
   struct RGB8 Background;    // Optional background colour for the document.
   struct RGB8 CursorColour;  // The colour used for the document cursor.
   struct RGB8 LinkColour;    // Default font colour for hyperlinks.
   struct RGB8 VLinkColour;   // Default font colour for visited hyperlinks.
   struct RGB8 SelectColour;  // Default font colour to use when hyperlinks are selected.
   struct RGB8 Border;        // Border colour around the document's surface.

   // Action stubs

   inline ERROR activate() { return Action(AC_Activate, this, NULL); }
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
   inline ERROR getVar(CSTRING FieldName, STRING Buffer, LONG Size) {
      struct acGetVar args = { FieldName, Buffer, Size };
      ERROR error = Action(AC_GetVar, this, &args);
      if ((error) AND (Buffer)) Buffer[0] = 0;
      return error;
   }
   inline ERROR init() { return Action(AC_Init, this, NULL); }
   inline ERROR refresh() { return Action(AC_Refresh, this, NULL); }
   inline ERROR saveToObject(OBJECTID DestID, CLASSID ClassID) {
      struct acSaveToObject args = { { DestID }, { ClassID } };
      return Action(AC_SaveToObject, this, &args);
   }
   inline ERROR scrollToPoint(DOUBLE X, DOUBLE Y, DOUBLE Z, LONG Flags) {
      struct acScrollToPoint args = { X, Y, Z, Flags };
      return Action(AC_ScrollToPoint, this, &args);
   }
   inline ERROR acSetVar(CSTRING FieldName, CSTRING Value) {
      struct acSetVar args = { FieldName, Value };
      return Action(AC_SetVar, this, &args);
   }
};

struct DocumentBase {
   LONG (*_CharLength)(objDocument *, LONG);
};

#ifndef PRV_DOCUMENT_MODULE
#define docCharLength(...) (DocumentBase->_CharLength)(__VA_ARGS__)
#endif

