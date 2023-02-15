/*********************************************************************************************************************

The source code of the Parasol project is made publicly available under the
terms described in the LICENSE.TXT file that is distributed with this package.
Please refer to it for further information on licensing.

**********************************************************************************************************************

-MODULE-
Document: Provides document display and editing facilities.

The Document module exports a small number of functions in support of the @Document class.
-END-

PARAGRAPH MANAGEMENT
--------------------
Text is managed as a stream of text interspersed with escaped areas that contain byte codes.  When the document text is
drawn, we maintain a 'line list' where the index of each line is recorded (see font_wrap()).  This allows us to do
things like Ctrl-K to delete a 'line'. It also allows us to remember the pixel width and height of each line, which is
important for highlighting selected text.

THE BYTE CODE
-------------
The text stream is a sequence of UTF-8 text with special escape codes inserted at points where we want to perform
certain actions, such as changing the font style, indicating hyperlinks etc.  The escape code value is the standard
0x1b.  Following that is a 16-bit number that indicates the length of the data (starting from 0x1b to the end of the
escape sequence).  A single byte indicates the instruction, e.g. ESC_FONT, ESC_OBJECT, ESC_HYPERLINK.  Following that
is the data related to that instruction.  Another escape code is placed at the end to terminate the sequence (often
useful when a routine needs to backtrack through the text stream).

GRAPHICAL OBJECT LAYOUT RULES
-----------------------------
RIPPLE allows for extremely complex document layouts.  This chapter clarifies the layout rules that must be observed by
classes that provide support for RIPPLE page layouts.

LAYOUT INTERPRETATION: Information about the available layout space will be passed in the Clip argument of the Layout
action.  Note that if the object is inside a table cell, the amount of space available will be smaller than the actual
page size.  Multiple iterations of the page layout will typically result in expanded coordinates in the Clip argument
each time the page layout is recalculated.

FIXED PLACEMENT: If the class accepts dimension values for X, Y, Width and/or height, fixed placement is enabled if any
of those values are set by the user.  Fixed placement can occur on the horizontal axis, vertical axis or both depending
on the number of dimension values that have been set.  When fixed placement occurs, positioning relative to the
document cursor is disabled and the values supplied by the user are used for placement of the graphical object.  Where
fixed placement is enabled, the object should still return a clipping region unless it is in background mode.  Document
margins are honoured when in fixed placement mode.

LAYOUT OPTIONS: All classes should support layout options by declaring a Layout field that supports the following
flags: SQUARE, WIDE, RIGHT, LEFT, BOTTOM, BACKGROUND, FOREGROUND, FIXED, VFIXED, HFIXED

BACKGROUND MODE: The user can place graphical objects in the background by specifying the BACKGROUND layout option.
All text will be overlayed on top of the graphics and no text clipping will be performed against the object.  The
layout support routine must return ERR_NothingDone to indicate that no clipping zone is defined.

FOREGROUND MODE: The user can force an object into the foreground so that it will be drawn over the document's text
stream.  This is achieved by setting the FOREGROUND layout option.

EXTENDED CLIPPING: By default, clipping is to the graphical area occupied by an object.  In some cases, the user may
wish to extend the clipping to the edges of the available layout space.  This can be achieved by requesting an object
layout of RIGHT (extend clip to the right), LEFT (extend clip to the left), WIDE (extend both left and right). The
default layout clipping is SQUARE, which does not extend the clipping region.

ALIGNMENT: Graphics Alignment can be requested by the document when calling the layout support action.  The class can
also support alignment by providing an Align field.  The formula that is used for alignment depends on whether or not
the dimensions are fixed in place. Alignment options will always override dimension settings where appropriate.  Thus
if horizontal alignment is selected, any predefined X value set by the user can be ignored in favour of calculating the
alignment from the left-most side of the cell.  The alignment formula must honour the margins of the available cell
space.  When an object is not in background mode, all alignment values are calculated with respect to the height of the
current line and not the height of the cell space that is occupied.  If horizontal centering is opted, the left-most
side used in the calculation must be taken from the current CursorX position.

MARGINS: In standard layout mode, cell margins must be honoured.  In fixed placement mode, cell margins are honoured
when calculating offsets, relative values and alignment.  In background mode, cell margins are ignored.

WHITESPACE: Gaps of whitespace at the top, left, right or bottom sides of a graphics object may be supported by some
class types, usually to prevent text from getting too close to the sides of an object.  This feature can only be
applied to objects that are are not in fixed placement or background mode.

TIGHT CLIPPING: Tight clipping is used where a complex clip region is required that is smaller than the rectangular
region occupied by a graphical object. A graphic with a circular or triangular shape could be an example of a graphic
that could use tight clipping.  Support for this feature is currently undefined in the RIPPLE standard.  In future it
is likely that it will be possible for the user to create customised tight-clipping zones by declaring polygonal areas
that should be avoided.  There are no plans to implement this feature at the level of object layouts.

TABLES
------
Internally, the layout of tables is managed as follows:

Border-Thickness, Cell-Spacing, Cell-Padding, Content, Cell-Padding, Cell-Spacing, ..., Border-Thickness

Table attributes are:

Columns:      The minimum width of each column in the table.
Width/Height: Minimum width and height of the table.
Colour:       Background colour for the table.
Border:       Border colour for the table (see thickness).
Thickness:    Thickness of the border colour.
Highlight     Highlight colour for border.
Shadow:       Shadow colour for border.
Padding:      Padding inside each cell (syn. Margins)
Spacing:      Spacing between cells.

For complex tables with different coloured borders between cells, allocate single-pixel sized cells with the background
colour set to the desired value in order to create the illusion of multi-coloured cell borders.

The page area owned by a table is given a clipping zone by the page layout engine, in the same way that objects are
given clipping zones.  This allows text to be laid out around the table with no effort on the part of the developer.


CELLS
-----
Borders: Borders are drawn within the cell, so the cell-padding value need to at least be the same value as the border
thickness, or text inside the cell will mix with the border.

*********************************************************************************************************************/

//#define DEBUG
//#define DBG_LAYOUT
//#define DBG_LAYOUT_ESCAPE // Do not use unless you want a lot of detail
//#define DBG_WORDWRAP
//#define DBG_STREAM
//#define DBG_LINES // Print list of lines (segments)
//#define GUIDELINES // Clipping guidelines
//#define GUIDELINES_CONTENT // Segment guidelines

#ifdef DBG_LAYOUT
 #define LAYOUT(...)   LogF(__VA_ARGS__)
 #define LAYOUT_LOGRETURN() LogReturn()
#else
 #define LAYOUT(...)
 #define LAYOUT_LOGRETURN()
#endif

#ifdef DBG_WORDWRAP
 #define WRAP(...)   LogF(__VA_ARGS__)
 #define WRAP_LOGRETURN() LogReturn()
#else
 #define WRAP(...)
 #define WRAP_LOGRETURN()
#endif

#define PRV_DOCUMENT
#define PRV_DOCUMENT_MODULE
#define PRV_MODDOCUMENT
#define PRV_SURFACE
#define COLOUR_LENGTH  16
#define CURSOR_RATE    1400
#define MAX_PAGEWIDTH  200000
#define MAX_PAGEHEIGHT 200000
#define MIN_PAGE_WIDTH 20
#define MAX_ARGS       80
#define MAX_DEPTH      1000  // Limits the number of tables-within-tables
#define MAX_DRAWBKGD   30
#define BULLET_WIDTH   14    // Minimum column width for bullet point lists
#define BORDER_SIZE    1
#define WIDTH_LIMIT    4000
#define LINE_HEIGHT    16    // Default line height (measured as an average) for the page
#define DEFAULT_INDENT 30
#define DEFAULT_FONTSIZE 10
#define MIN_LINEHEIGHT 0.001
#define MIN_VSPACING   0.001
#define MAX_VSPACING   20
#define MIN_LEADING    0.001
#define MAX_LEADING    20
#define NOTSPLIT       -1
#define BUFFER_BLOCK   8192
#define CTRL_CODE      '\E' // The escape code, 0x1b.  NOTE: This must be between 1 and 0x20 so that it can be treated as whitespace for certain routines and also to avoid UTF8 interference
#define CLIP_BLOCK     30

#define DRAW_PAGE(a) QueueAction(MT_DrwInvalidateRegion, (a)->SurfaceID);

#define ULD_TERMINATE       0x01
#define ULD_KEEP_PARAMETERS 0x02
#define ULD_REFRESH         0x04
#define ULD_REDRAW          0x08

#define SEF_STRICT        0x00000001
#define SEF_IGNORE_QUOTES 0x00000002
#define SEF_KEEP_ESCAPE   0x00000004
#define SEF_NO_SCRIPT     0x00000008

enum {
   COND_NOT_EQUAL=1,
   COND_EQUAL,
   COND_LESS_THAN,
   COND_LESS_EQUAL,
   COND_GREATER_THAN,
   COND_GREATER_EQUAL
};

#include <parasol/main.h>
#include <parasol/modules/xml.h>
#include <parasol/modules/document.h>
#include <parasol/modules/font.h>
#include <parasol/modules/display.h>
#include <parasol/modules/vector.h>

#include "hashes.h"

struct CoreBase  *CoreBase;
struct FontBase    *FontBase;
struct DisplayBase *DisplayBase;
struct VectorBase  *VectorBase;
static OBJECTPTR clDocument = NULL;
static RGB8 glHighlight = { 220, 220, 255, 255 };
static OBJECTPTR modDisplay = NULL, modFont = NULL, modDocument = NULL, modVector = NULL;

//********************************************************************************************************************

class extDocument : public objDocument {
   public:
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
   STRING TBuffer;           // Translation buffer
   LONG   TBufferSize;
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
};

enum {
   RT_OBJECT_TEMP=1,
   RT_OBJECT_UNLOAD,
   RT_OBJECT_UNLOAD_DELAY,
   RT_MEMORY,
   RT_PERSISTENT_SCRIPT,
   RT_PERSISTENT_OBJECT
};

struct docresource {
   struct docresource *Next;
   struct docresource *Prev;
   union {
      APTR Memory;
      APTR Address;
      OBJECTID ObjectID;
   };
   LONG ClassID;
   BYTE Type;
};

struct tagroutine {
   ULONG TagHash;
   void (*Routine)(extDocument *, objXML *, XMLTag *, XMLTag *, LONG *, LONG);
   ULONG Flags;
};

struct DocSegment {
   LONG  Index;      // Line's byte index within the document text stream
   LONG  Stop;       // The stopping index for the line
   LONG  TrimStop;   // The stopping index for the line, with any whitespace removed
   LONG X;           // Horizontal coordinate of this line on the display
   LONG Y;           // Vertical coordinate of this line on the display
   UWORD Height;     // Pixel height of the line, including all anchored objects.  When drawing, this is used for vertical alignment of graphics within the line
   UWORD BaseLine;   // Base-line - this is the height of the largest font down to the base line
   UWORD Width;      // Width of the characters in this line segment
   UWORD AlignWidth; // Full-width of this line-segment if it were non-breaking
   UWORD Depth;      // Section depth that this segment belongs to - helps to differentiate between inner and outer tables
   UBYTE Edit:1;           // TRUE if this segment represents content that can be edited
   UBYTE TextContent:1;    // TRUE if there are text characters in this segment
   UBYTE ControlContent:1; // TRUE if there are control codes in this segment
   UBYTE ObjectContent:1;  // TRUE if there are objects in this segment
   UBYTE AllowMerge:1;     // TRUE if this segment can be merged with sibling segments that have AllowMerge set to TRUE
};

struct SortSegment {
   LONG Segment;
   LONG Y;
};

struct MouseOver {
   struct MouseOver *Next;
   LONG Top, Left, Bottom, Right;
   LONG ElementID;
};

struct DocLink {
   union {
      struct escLink *Link;
      struct escCell *Cell;
      APTR Escape;
   };
   LONG X, Y;
   UWORD Width, Height;
   LONG Segment;
   UBYTE EscapeCode;
};

struct DocEdit {
   struct DocEdit *Next;
   ULONG NameHash;      // The name of the edit area is held here as a hash.  Zero if the area has no name.
   LONG MaxChars;
   LONG OnEnter;        // Offset to the name of the OnEnter function
   LONG OnExit;         // Offset to the name of the OnExit function
   LONG OnChange;       // Offset to the name of the OnChange function
   LONG Args;
   LONG TotalArgs;
   UBYTE LineBreaks:1;
};

struct DocClip {
   struct SurfaceClip Clip;
   LONG Index;
   BYTE Transparent:1;
#ifdef DBG_WORDWRAP
   UBYTE Name[32];
#endif
};

enum {
   STATE_OUTSIDE=0,
   STATE_ENTERED,
   STATE_INSIDE
};

#define IPF_NOCONTENT  0x0001
#define IPF_STRIPFEEDS 0x0002

#define TAG_CHILDREN     0x00000001 // Children are compulsory for the tag to have an effect
#define TAG_CONTENT      0x00000002 // Tag has a direct impact on text content or the page layout
#define TAG_CONDITIONAL  0x00000004 // Tag is a conditional statement
#define TAG_INSTRUCTION  0x00000008 // Tag is an executable instruction
#define TAG_ROOT         0x00000010 // Tag is limited to use at the root of the document
#define TAG_PARAGRAPH    0x00000020 // Tag results in paragraph formatting (will force some type of line break)
#define TAG_OBJECTOK     0x00000040 // It is OK for this tag to be used within any object

// These flag values are in the upper word so that we can or them with IPF and TAG constants.

#define FILTER_TABLE 0x80000000 // FILTER: Table
#define FILTER_ROW   0x40000000 // FILTER: Row
#define FILTER_ALL   (FILTER_TABLE|FILTER_ROW)

#define IXF_SIBLINGS   0x01
#define IXF_HOLDSTYLE  0x02
#define IXF_RESETSTYLE 0x04
#define IXF_CLOSESTYLE 0x08

#define TRF_BREAK    0x00000001
#define TRF_CONTINUE 0x00000002

#define PXF_ARGS      0x0001
#define PXF_TRANSLATE 0x0002

// This PTR macros are used in tags.cpp

#define PTR_SAVE_ARGS(tag) \
   *b_revert = Self->BufferIndex; \
   *s_revert = Self->ArgIndex; \
   *e_revert = 0; \
   if (convert_xml_args(Self, (tag)->Attrib, (tag)->TotalAttrib) != ERR_Okay) goto next; \
   *e_revert = Self->ArgIndex;

#define PTR_RESTORE_ARGS() \
   if (*e_revert > *s_revert) { \
      while (*e_revert > *s_revert) { \
         *e_revert -= 1; \
         Self->VArg[*e_revert].Attrib[0] = Self->VArg[*e_revert].String; \
      } \
   } \
   Self->BufferIndex = *b_revert; \
   Self->ArgIndex = *s_revert;

enum {
   LINK_HREF=1,
   LINK_FUNCTION
};

struct tablecol {
   UWORD PresetWidth;
   UWORD MinWidth;   // For assisting layout
   UWORD Width;
};

typedef struct escAdvance { LONG X, Y; } escAdvance;

typedef struct escIndex {
   ULONG NameHash;   // The name of the index is held here as a hash
   LONG  ID;         // Unique ID for matching to the correct escIndexEnd
   LONG  Y;          // The cursor's vertical position of when the index was encountered during layout
   UBYTE Visible:1;  // TRUE if the content inside the index is visible (this is the default)
   UBYTE ParentVisible:1; // TRUE if the nearest parent index(es) will allow index content to be visible.  TRUE is the default.  This allows Hide/ShowIndex() to manage themselves correctly
} escIndex;

typedef struct escIndexEnd {
   LONG ID;     // Unique ID matching to the correct escIndex
} escIndexEnd;

typedef struct escLink {
   UBYTE Type;    // Link type (either a function or hyperlink)
   UBYTE Args;    // Total number of args being sent, if a function
   UWORD ID;
   LONG Align;
   LONG PointerMotion;
   // Please update tag_link() if you add fields to this structure
} escLink;

typedef struct escList {
   struct escList *Stack; // Stack management pointer during layout
   RGB8 Colour;    // Colour to use for bullet points (valid for LT_BULLET only).
   STRING Buffer;         // Temp buffer, used for ordered lists
   LONG  Start;           // Starting value for ordered lists (default: 1)
   LONG  ItemIndent;      // Minimum indentation for text printed for each item
   LONG  BlockIndent;     // Indentation for each set of items
   LONG  ItemNum;
   LONG  OrderInsert;
   FLOAT VSpacing;        // Spacing between list items, expressed as a ratio
   UBYTE Type;
   UBYTE Repass:1;
} escList;

typedef struct escSetMargins { WORD Left; WORD Top; WORD Bottom; WORD Right; } escSetMargins;

typedef struct escObject {
   OBJECTID ObjectID;   // Reference to the object
   LONG  ClassID;       // Class that the object belongs to, mostly for informative/debugging purposes
   UBYTE Embedded:1;    // TRUE if object is embedded as part of the text stream (treated as if it were a character)
   UBYTE Owned:1;       // TRUE if the object is owned by a parent (not subject to normal document layout)
   UBYTE Graphical:1;   // TRUE if the object has graphical representation or contains graphical objects
} escObject;

typedef struct escTable {
   struct escTable *Stack;
   struct tablecol *Columns; // Table column management, allocated as an independent memory array
   RGB8 Colour;       // Background colour
   RGB8 Highlight;    // Border highlight
   RGB8 Shadow;       // Border shadow
   WORD CellVSpacing;        // Spacing between each cell, vertically
   WORD CellHSpacing;        // Spacing between each cell, horizontally
   WORD CellPadding;         // Spacing inside each cell (margins)
   LONG RowWidth;            // Assists in the computation of row width
   LONG X, Y;                // Calculated X/Y coordinate of the table
   LONG Width, Height;       // Calculated table width/height
   LONG MinWidth, MinHeight; // User-determined minimum table width/height
   LONG TotalColumns;        // Total number of columns in table
   LONG Rows;                // Total number of rows in table
   LONG RowIndex;            // Current row being processed, generally for debugging
   LONG CursorX, CursorY;    // Cursor coordinates
   LONG TotalClips;
   UWORD Thickness;          // Border thickness
   UBYTE ComputeColumns;
   UBYTE WidthPercent:1;     // TRUE if width is a percentage
   UBYTE HeightPercent:1;    // TRUE if height is a percentage
   UBYTE CellsExpanded:1;    // FALSE if the table cells have not been expanded to match the inside table width
   UBYTE ResetRowHeight:1;   // TRUE if the height of all rows needs to be reset in the current pass
   UBYTE Wrap:1;
   UBYTE Thin:1;
   // Entry followed by the minimum width of each column
} escTable;

typedef struct escParagraph {
   struct escParagraph *Stack;
   LONG   X, Y;
   LONG   Height;
   LONG   BlockIndent;
   LONG   ItemIndent;
   DOUBLE Indent;
   DOUBLE VSpacing;     // Trailing whitespace, expressed as a ratio of the default line height
   DOUBLE LeadingRatio; // Leading whitespace (minimum amount of space from the end of the last paragraph).  Expressed as a ratio of the default line height
   // Options
   UBYTE Relative:1;
   UBYTE CustomString:1;
   UBYTE ListItem:1;
   UBYTE Trim:1;
} escParagraph;

typedef struct escRow {
   struct escRow *Stack;
   LONG  Y;
   LONG  RowHeight; // Height of all cells on this row, used when drawing the cells
   LONG  MinHeight;
   RGB8 Highlight;
   RGB8 Shadow;
   RGB8 Colour;
   UBYTE  VerticalRepass:1;
} escRow;

typedef struct escCell {
   // PLEASE REFER TO THE DEFAULTS IN TAG_CELL() IN TAGS.C IF YOU CHANGE THIS STRUCTURE
   struct escCell *Stack;
   LONG CellID;         // Identifier for the matching escCellEnd
   LONG Column;         // Column number that the cell starts in
   LONG ColSpan;        // Number of columns spanned by this cell (normally set to 1)
   LONG RowSpan;        // Number of rows spanned by this cell
   LONG AbsX, AbsY;     // Cell coordinates, these are absolute
   LONG Width, Height;  // Width and height of the cell
   LONG OnClick;        // Offset to the name of an onclick function
   LONG Args;           // Offset to the argument list, if any are specified.  Otherwise zero
   ULONG EditHash;      // Hash-name of the edit definition that this cell is linked to (if any, otherwise zero)
   WORD TotalArgs;      // Total number of arguments for function execution
   RGB8 Highlight;
   RGB8 Shadow;
   RGB8 Colour;
} escCell;

typedef struct escCellEnd {
   LONG CellID;    // Matching identifier from escCell
} escCellEnd;

struct process_table {
   struct escTable *escTable;
   LONG RowCol;
};

struct EditCell {
   LONG CellID;
   LONG X, Y, Width, Height;
};

enum {
   ESC_FONT=1,
   ESC_FONTCOLOUR,
   ESC_UNDERLINE,
   ESC_BACKGROUND,
   ESC_INVERSE,
   ESC_OBJECT,
   ESC_LINK,
   ESC_TABDEF,
   ESC_PARAGRAPH_END,
   ESC_PARAGRAPH_START, // 10
   ESC_LINK_END,
   ESC_ADVANCE,
   ESC_SHRINK,          // Deprecated
   ESC_LIST_START,
   ESC_LIST_END,        // 15
   ESC_TABLE_START,
   ESC_TABLE_END,
   ESC_ROW,
   ESC_CELL,
   ESC_CELL_END,        // 20
   ESC_ROW_END,
   ESC_SETMARGINS,
   ESC_INDEX_START,
   ESC_INDEX_END,
   // End of list - NB: PLEASE UPDATE strCodes[] IF YOU ADD NEW CODES
   ESC_END
};

enum {
   LT_ORDERED=0,
   LT_BULLET,
   LT_CUSTOM
};

enum {
   NL_NONE=0,
   NL_PARAGRAPH
};

static const CSTRING strCodes[] = {
   "-",
   "Font",
   "FontCol",
   "Uline",
   "Bkgd",
   "Inv",
   "Obj",
   "Link",
   "TabDef",
   "PE",
   "P",
   "EndLnk",
   "Advance",
   "Shrink",
   "List",
   "ListEnd",
   "Table",
   "TableEnd",
   "Row",
   "Cell",
   "CellEnd",
   "RowEnd",
   "SetMargins",
   "Index",
   "IndexEnd"
};

/*********************************************************************************************************************
** Function prototypes.
*/

#include "module_def.c"

struct layout; // Pre-def

static ERROR  activate_edit(extDocument *, LONG, LONG);
static ERROR  add_clip(extDocument *, SurfaceClip *, LONG, CSTRING, BYTE);
static ERROR  add_document_class(void);
static LONG   add_drawsegment(extDocument *, LONG, LONG Stop, layout *, LONG, LONG, LONG, CSTRING);
static void   add_link(extDocument *, UBYTE, APTR, LONG, LONG, LONG, LONG, CSTRING);
static docresource * add_resource_id(extDocument *, LONG, LONG);
static docresource * add_resource_ptr(extDocument *, APTR, LONG);
static LONG   add_tabfocus(extDocument *, UBYTE, LONG);
static void   add_template(extDocument *, objXML *, XMLTag *);
static void   advance_tabfocus(extDocument *, BYTE);
static LONG   calc_page_height(extDocument *, LONG, LONG, LONG);
static void   check_mouse_click(extDocument *, DOUBLE X, DOUBLE Y);
static void   check_mouse_pos(extDocument *, DOUBLE, DOUBLE);
static void   check_mouse_release(extDocument *, DOUBLE X, DOUBLE Y);
static ERROR  consume_input_events(const InputEvent *, LONG);
static ERROR  convert_xml_args(extDocument *, XMLAttrib *, LONG);
static LONG   create_font(CSTRING, CSTRING, LONG);
static void   deactivate_edit(extDocument *, BYTE);
static void   deselect_text(extDocument *);
static void   draw_background(extDocument *, objSurface *, objBitmap *);
static void   draw_document(extDocument *, objSurface *, objBitmap *);
static void   draw_border(extDocument *, objSurface *, objBitmap *);
static void   exec_link(extDocument *, LONG);
static ERROR  extract_script(extDocument *, CSTRING, OBJECTPTR *, CSTRING *, CSTRING *);
static void   error_dialog(CSTRING, CSTRING, ERROR);
static ERROR  eval(extDocument *, STRING, LONG, LONG);
static LONG   find_segment(extDocument *, LONG, LONG);
static LONG   find_tabfocus(extDocument *, UBYTE, LONG);
static ERROR  flash_cursor(extDocument *, LARGE, LARGE);
static void   free_links(extDocument *);
static CSTRING get_font_style(LONG);
//static LONG   get_line_from_index(extDocument *, LONG Index);
static LONG   getutf8(CSTRING, LONG *);
static ERROR  insert_escape(extDocument *, LONG *, WORD, APTR, LONG);
static void   insert_paragraph_end(extDocument *, LONG *);
static void   insert_paragraph_start(extDocument *, LONG *, escParagraph *);
static ERROR  insert_string(CSTRING, STRING, LONG, LONG, LONG);
static ERROR  insert_text(extDocument *, LONG *, CSTRING, LONG, BYTE);
static ERROR  insert_xml(extDocument *, objXML *, XMLTag *, LONG, UBYTE);
static void   key_event(extDocument *, evKey *, LONG);
static ERROR  keypress(extDocument *, LONG, LONG, LONG);
static void   layout_doc(extDocument *);
static LONG   layout_section(extDocument *, LONG, objFont **, LONG, LONG, LONG *, LONG *, LONG, LONG, LONG, LONG, BYTE *);
static ERROR  load_doc(extDocument *, CSTRING, BYTE, BYTE);
static objFont * lookup_font(LONG, CSTRING);
static void   notify_disable_surface(OBJECTPTR Object, ACTIONID ActionID, ERROR Result, APTR Args);
static void   notify_enable_surface(OBJECTPTR Object, ACTIONID ActionID, ERROR Result, APTR Args);
static void   notify_focus_surface(OBJECTPTR Object, ACTIONID ActionID, ERROR Result, APTR Args);
static void   notify_free_event(OBJECTPTR Object, ACTIONID ActionID, ERROR Result, APTR Args);
static void   notify_lostfocus_surface(OBJECTPTR Object, ACTIONID ActionID, ERROR Result, APTR Args);
static void   notify_redimension_surface(OBJECTPTR Object, ACTIONID ActionID, ERROR Result, struct acRedimension *Args);
static LONG   parse_tag(extDocument *, objXML *, XMLTag *, LONG *, LONG);
#ifdef DEBUG
static void   print_xmltree(XMLTag *, LONG *) __attribute__ ((unused));
#endif
#ifdef DBG_LINES
static void print_sorted_lines(extDocument *) __attribute__ ((unused));
#endif
static ERROR  process_page(extDocument *, objXML *);
static void   process_parameters(extDocument *, CSTRING);
static bool read_rgb8(CSTRING, RGB8 *);
static void   redraw(extDocument *, BYTE);
static ERROR  report_event(extDocument *, LARGE, APTR, CSTRING);
static void   reset_cursor(extDocument *);
static ERROR  resolve_fontx_by_index(extDocument *, LONG, LONG *);
static ERROR  resolve_font_pos(extDocument *, LONG, LONG X, LONG *, LONG *);
static LONG   safe_file_path(extDocument *, CSTRING);
static void   set_focus(extDocument *, LONG, CSTRING);
static void   set_object_style(extDocument *, OBJECTPTR);
static void   show_bookmark(extDocument *, CSTRING);
static void   style_check(extDocument *, LONG *);
static void   tag_object(extDocument *, CSTRING, CLASSID, XMLTag *, objXML *, XMLTag *, XMLTag *, LONG *, LONG, UBYTE *, UBYTE *, LONG *);
static void   tag_xml_content(extDocument *, objXML *, XMLTag *, WORD);
static ERROR  unload_doc(extDocument *, BYTE);
static BYTE   valid_object(extDocument *, OBJECTPTR);
static BYTE   valid_objectid(extDocument *, OBJECTID);
static BYTE   view_area(extDocument *, LONG Left, LONG Top, LONG Right, LONG Bottom);
static LONG   xml_content_len(XMLTag *) __attribute__ ((unused));
static void   xml_extract_content(XMLTag *, char *, LONG *, BYTE) __attribute__ ((unused));

#ifdef DBG_STREAM
static void print_stream(extDocument *Self, STRING Stream) __attribute__ ((unused));
#endif

struct FontEntry {
   objFont *Font;
   LONG Point;
};

static STRING exsbuffer = NULL;
static LONG exsbuffer_size = 0;
static FontEntry *glFonts = NULL;
static LONG glTotalFonts = 0;
static LONG glMaxFonts = 0;

// Control code format: ESC,Code,Length[2],ElementID[4]...Data...,Length[2],ESC

#define ESC_LEN_START 8
#define ESC_LEN_END   3
#define ESC_LEN (ESC_LEN_START + ESC_LEN_END)

template <class T>
T * escape_data(BYTE *Stream, LONG Index) {
   return (T *)(Stream + Index + ESC_LEN_START);
}

template <class T>
T * escape_data(UBYTE *Stream, LONG Index) {
   return (T *)(Stream + Index + ESC_LEN_START);
}

#define remove_cursor(a)           draw_cursor((a),FALSE)
// Calculate the length of an escape sequence
#define ESC_ELEMENTID(a)           (((LONG *)(a))[1])
#define ESCAPE_CODE(stream, index) ((stream)[(index)+1]) // Escape codes are only 1 byte long
#define ESCAPE_LEN(a)              ((((a)[2])<<8) | ((a)[3]))
// Move to the next character - handles UTF8 only, no escape sequence handling
#define NEXT_CHAR(s,i) { if ((s)[(i)] IS CTRL_CODE) i += ESCAPE_LEN(s+i); else { i++; while (((s)[(i)] & 0xc0) IS 0x80) (i)++; } }
#define PREV_CHAR(s,i) { if ((s)[(i)-1] IS CTRL_CODE) (i) -= ((s)[(i)-3]<<8) | ((s)[(i)-2]); else i--; }

#define START_TEMPLATE(TAG,XML,ARGS) \
   XMLTag *savetag; \
   objXML *savexml; \
   savetag = Self->InjectTag; \
   savexml = Self->InjectXML; \
   Self->InjectTag = (TAG); \
   Self->InjectXML = (XML); \
   Self->InTemplate++;

#define END_TEMPLATE() \
   Self->InTemplate--; \
   Self->InjectTag = savetag; \
   Self->InjectXML = savexml;

static FIELD FID_LayoutSurface;

//********************************************************************************************************************

ERROR CMDInit(OBJECTPTR argModule, struct CoreBase *argCoreBase)
{
   CoreBase = argCoreBase;

   argModule->getPtr(FID_Master, &modDocument);

   if (objModule::load("display", MODVERSION_DISPLAY, &modDisplay, &DisplayBase) != ERR_Okay) return ERR_InitModule;
   if (objModule::load("font", MODVERSION_FONT, &modFont, &FontBase) != ERR_Okay) return ERR_InitModule;
   if (objModule::load("vector", MODVERSION_VECTOR, &modVector, &VectorBase) != ERR_Okay) return ERR_InitModule;

   FID_LayoutSurface = StrHash("LayoutSurface", 0);

   OBJECTID style_id;
   if (!FindObject("glStyle", ID_XML, 0, &style_id)) {
      char buffer[32];
      if (!acGetVar(GetObjectPtr(style_id), "/colours/@DocumentHighlight", buffer, sizeof(buffer))) {
         read_rgb8(buffer, &glHighlight);
      }
   }

   return add_document_class();
}

ERROR CMDExpunge(void)
{
   {
      parasol::Log log;
      log.msg("Freeing %d internally allocated fonts.", glTotalFonts);
      for (LONG i=0; i < glTotalFonts; i++) acFree(glFonts[i].Font);
   }

   if (exsbuffer) { FreeResource(exsbuffer); exsbuffer = NULL; }
   if (glFonts)   { FreeResource(glFonts);   glFonts   = NULL; }

   if (modVector)  { acFree(modVector);   modVector  = NULL; }
   if (modDisplay) { acFree(modDisplay);  modDisplay = NULL; }
   if (modFont)    { acFree(modFont);     modFont    = NULL; }

   if (clDocument) { acFree(clDocument);  clDocument = NULL; }
   return ERR_Okay;
}

static ERROR CMDOpen(OBJECTPTR Module)
{
   Module->set(FID_FunctionList, glFunctions);
   return ERR_Okay;
}

/*********************************************************************************************************************

-FUNCTION-
CharLength: Returns the length of any character or escape code in a document data stream.

This function will compute the byte-length of any UTF-8 character sequence or escape code in a document's data stream.

-INPUT-
ext(Document) Document: The document to query.
int Index: The byte index of the character to inspect.

-RESULT-
int: The length of the character is returned, or 0 if an error occurs.

-END-

*********************************************************************************************************************/

static LONG docCharLength(extDocument *Self, LONG Index)
{
   if (!Self) return 0;

   if (Self->Stream[Index] IS CTRL_CODE) {
      return ESCAPE_LEN(Self->Stream + Index);
   }
   else {
      LONG len = 1;
      while (((Self->Stream)[len] & 0xc0) IS 0x80) len++;
      return len;
   }
}

//********************************************************************************************************************

INLINE LONG find_cell(extDocument *Self, LONG ID, ULONG EditHash)
{
   UBYTE *stream;
   if (!(stream = (UBYTE *)Self->Stream)) return -1;

   LONG i = 0;
   while (stream[i]) {
      if (stream[i] IS CTRL_CODE) {
         if (ESCAPE_CODE(stream, i) IS ESC_CELL) {
            auto cell = escape_data<escCell>(stream, i);
            if ((ID) and (ID IS cell->CellID)) return i;
            else if ((EditHash) and (EditHash IS cell->EditHash)) return i;
         }
      }
      NEXT_CHAR(stream, i);
   }

   return -1;
}

//********************************************************************************************************************

INLINE DocEdit * find_editdef(extDocument *Self, ULONG Hash)
{
   for (auto edit=Self->EditDefs; edit; edit=edit->Next) {
      if (edit->NameHash IS Hash) return edit;
   }

   return NULL;
}

//********************************************************************************************************************

INLINE void layout_doc_fast(extDocument *Self)
{
   AdjustLogLevel(2);
   layout_doc(Self);
   AdjustLogLevel(-2);
}

#include "tags.cpp"
#include "class/fields.cpp"
#include "class/document_class.cpp"
#include "functions.cpp"

//********************************************************************************************************************

static ERROR add_document_class(void)
{
   clDocument = objMetaClass::create::global(
      fl::BaseClassID(ID_DOCUMENT),
      fl::ClassVersion(VER_DOCUMENT),
      fl::Name("Document"),
      fl::Category(CCF_GUI),
      fl::Flags(CLF_PROMOTE_INTEGRAL),
      fl::Actions(clDocumentActions),
      fl::Methods(clDocumentMethods),
      fl::Fields(clFields),
      fl::Size(sizeof(extDocument)),
      fl::Path(MOD_PATH),
      fl::FileExtension("*.rpl|*.ripple|*.rple"));

   return clDocument ? ERR_Okay : ERR_AddClass;
}

//********************************************************************************************************************

PARASOL_MOD(CMDInit, NULL, CMDOpen, CMDExpunge, MODVERSION_DOCUMENT)
