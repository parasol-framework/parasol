#ifndef MODULES_DOCUMENT
#define MODULES_DOCUMENT 1

// Name:      document.h
// Copyright: Paul Manias © 2005-2022
// Generator: idl-c

#ifndef MAIN_H
#include <parasol/main.h>
#endif

#define MODVERSION_DOCUMENT (1)

#ifndef MODULES_DISPLAY_H
#include <parasol/modules/display.h>
#endif

#ifndef MODULES_XML_H
#include <parasol/modules/xml.h>
#endif

#ifndef MODULES_FONT_H
#include <parasol/modules/font.h>
#endif

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
   LONG Version;                    // Version of this DocStyle structure
   struct rkDocument * Document;    // The document object that this style originates from
   struct rkFont * Font;            // Pointer to the current font object.  Indicates face, style etc, but not simple attributes like colour
   struct RGB8 FontColour;          // Foreground colour (colour of the font)
   struct RGB8 FontUnderline;       // Underline colour for the font, if active
   LONG StyleFlags;                 // Font style flags (FSO)
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

typedef struct rkDocument {
   OBJECT_HEADER
   LARGE    EventMask;        // Event mask for selectively receiving events from the Document object.
   STRING   Description;      // A description assigned by the author of the document
   STRING   FontFace;         // The user's default font face
   STRING   Title;            // The title of the document
   STRING   Author;           // The author of the document
   STRING   Copyright;        // Copyright information for the document
   STRING   Keywords;         // Keywords for the document
   OBJECTID TabFocusID;       // If the tab key is pressed, focus can be changed to this object
   OBJECTID SurfaceID;        // The surface that the document will be rendered to
   OBJECTID FocusID;
   LONG     Flags;            // Optional flags
   LONG     LeftMargin;       // Size of the left margin
   LONG     TopMargin;        // Size of the top margin
   LONG     RightMargin;      // Size of the right margin
   LONG     BottomMargin;     // Size of the bottom margin
   LONG     FontSize;         // The user's default font size
   LONG     PageHeight;       // Height of the document page
   LONG     BorderEdge;
   LONG     LineHeight;       // Default line height (assumed to be an average) for all text the loaded page
   ERROR    Error;            // Processing error code
   struct RGB8 FontColour;    // Default font colour
   struct RGB8 Highlight;     // Default colour for document highlighting
   struct RGB8 Background;    // Colour for document background
   struct RGB8 CursorColour;  // The colour of the cursor
   struct RGB8 LinkColour;    // Colour to use for hyperlinks
   struct RGB8 VLinkColour;   // Colour to use for visited hyperlinks
   struct RGB8 SelectColour;  // Default colour to use when links are selected (e.g. when the user tabs to a link)
   struct RGB8 Border;

#ifdef PRV_DOCUMENT
   FUNCTION EventCallback;
   objXML *XML;             // Source XML document
   objXML *InsertXML;       // For temporary XML parsing by the InsertXML method
   objXML *Templates;       // All templates for the current document are stored here
   objXML *InjectXML;
   struct KeyStore *Vars;
   struct KeyStore *Params;
   struct escCell *CurrentCell;      // Used to assist drawing, reflects the cell we are currently drawing within (if any)
   STRING ParamBuffer;
   STRING Temp;
   STRING Buffer;
   STRING Path;              // Optional file to load on Init()
   STRING PageName;          // Page name to load from the Path
   STRING Bookmark;          // Bookmark name processed from the Path
   UBYTE  *Stream;           // Internal stream buffer
   STRING WorkingPath;       // String storage for the WorkingPath field
   APTR   prvKeyEvent;
   OBJECTPTR CurrentObject;
   OBJECTPTR UserDefaultScript;  // Allows the developer to define a custom default script.
   OBJECTPTR DefaultScript;
   struct DocSegment *Segments; // Pointer to an array of segments
   struct SortSegment *SortSegments;
   struct style_status Style;
   struct style_status RestoreStyle;
   struct XMLTag *InjectTag;
   struct XMLTag *HeaderTag;
   struct XMLTag *FooterTag;
   struct XMLTag *PageTag;
   struct XMLTag *BodyTag;
   struct DocClip *Clips;
   LONG SurfaceWidth, SurfaceHeight;
   struct DocLink *Links;
   struct DocEdit *EditDefs;
   struct MouseOver *MouseOverChain;
   struct docresource *Resources;
   struct DocEdit *ActiveEditDef; // As for ActiveEditCell, but refers to the active editing definition
   struct {
      STRING *Attrib;
      STRING String;
   } *VArg;
   struct {
      // The Ref is a unique ID for the Type, so you can use it to find the tab in the document stream
      LONG  Ref;             // For TT_OBJECT: ObjectID; TT_LINK: LinkID
      LONG  XRef;            // For TT_OBJECT: SurfaceID (if found)
      UBYTE Type;            // TT_LINK, TT_OBJECT
      UBYTE Active:1;        // TRUE if the tabbable link is active/visible
   } *Tabs;
   struct DocTrigger * Triggers[DRT_MAX];
   struct XMLTag * ArgNest[64];
   struct EditCell *EditCells;
   LONG   ArgNestIndex;
   LONG   TabIndex;
   LONG   MaxTabs;
   LONG   UniqueID;          // Use for generating unique/incrementing ID's, e.g. cell ID
   OBJECTID UserFocusID;
   OBJECTID ViewID;          // View surface - this contains the page and serves as the page's scrolling area
   OBJECTID PageID;          // Page surface - this holds the graphics content
   LONG   MinPageWidth;      // Internal value for managing the page width, speeds up layout processing
   FLOAT  PageWidth;         // Width of the widest section of the document page.  Can be pre-defined for a fixed width.
   LONG   InputHandle;
   LONG   MaxClips;
   LONG   TotalClips;
   LONG   TotalLinks;       // Current total of assigned links
   LONG   MaxLinks;         // Current size limit of the link array
   LONG   LinkIndex;        // Currently selected link (mouse over)
   LONG   ScrollWidth;
   LONG   StreamLen;        // The length of the stream string (up to the termination character)
   LONG   StreamSize;       // Allocated size of the stream buffer
   LONG   SelectStart;      // Selection start (stream index)
   LONG   SelectEnd;        // Selection end (stream index)
   LONG   MaxSegments;      // Total number of segments available in the line array
   LONG   SegCount;         // Total number of entries in the segments array
   LONG   SortCount;        // Total number of entries in the sorted segments array
   LONG   XPosition;        // Horizontal scrolling offset
   LONG   YPosition;        // Vertical scrolling offset
   LONG   AreaX, AreaY;
   LONG   AreaWidth, AreaHeight;
   LONG   TempSize;
   LONG   BufferSize;
   LONG   BufferIndex;
   LONG   CalcWidth;
   LONG   LoopIndex;
   LONG   ElementCounter;     // Counter for element ID's
   LONG   ClickX, ClickY;
   LONG   ECIndex;            // EditCells table index
   LONG   ECMax;              // Maximum number of entries currently available in the EditCells array
   LONG   ObjectCache;
   LONG   TemplatesModified;  // Track modifications to Self->Templates
   LONG   BreakLoop;
   LONG   GeneratedID;       // Unique ID that is regenerated on each load/refresh
   LONG   ClickSegment;      // The index of the segment that the user clicked on
   LONG   MouseOverSegment;  // The index of the segment that the mouse is currently positioned over
   LONG   CursorIndex;       // Position of the cursor if text is selected, or edit mode is active.  It reflects the position at which entered text will be inserted.  -1 if not in use
   LONG   SelectIndex;       // The end of the selected text area, if text is selected.  Otherwise -1
   LONG   SelectCharX;       // The X coordinate of the SelectIndex character
   LONG   CursorCharX;       // The X coordinate of the CursorIndex character
   LONG   PointerX;          // Current pointer X coordinate on the document surface
   LONG   PointerY;
   TIMER  FlashTimer;
   LONG   ActiveEditCellID;  // If editing is active, this refers to the ID of the cell being edited
   ULONG  ActiveEditCRC;     // CRC for cell editing area, used for managing onchange notifications
   UWORD  Depth;             // Section depth - increases when layout_section() recurses, e.g. into tables
   UWORD  ParagraphDepth;
   UWORD  LinkID;
   WORD   ArgIndex;
   WORD   FocusIndex;        // Tab focus index
   WORD   Invisible;         // This variable is incremented for sections within a hidden index
   ULONG  RelPageWidth:1;    // Relative page width
   ULONG  PointerLocked:1;
   ULONG  ClickHeld:1;
   ULONG  UpdateLayout:1;
   ULONG  State:3;
   ULONG  VScrollVisible:1;
   ULONG  MouseOver:1;
   ULONG  PageProcessed:1;
   ULONG  NoWhitespace:1;
   ULONG  HasFocus:1;
   ULONG  CursorSet:1;
   ULONG  LMB:1;
   ULONG  EditMode:1;
   ULONG  CursorState:1;   // TRUE if the edit cursor is on, FALSE if off.  Used for flashing of the cursor
   UBYTE  Processing;
   UBYTE  DrawIntercept;
   UBYTE  InTemplate;
   UBYTE  BkgdGfx;

#endif
} objDocument;

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
struct docApplyFontStyle { struct DocStyleV1 * Style; struct rkFont * Font;  };
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

INLINE ERROR docApplyFontStyle(APTR Ob, struct DocStyleV1 * Style, struct rkFont * Font) {
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


struct DocumentBase {
   LONG (*_CharLength)(struct rkDocument *, LONG);
};

#ifndef PRV_DOCUMENT_MODULE
#define docCharLength(...) (DocumentBase->_CharLength)(__VA_ARGS__)
#endif

#endif
