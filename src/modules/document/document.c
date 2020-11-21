/*****************************************************************************

The source code of the Parasol project is made publicly available under the
terms described in the LICENSE.TXT file that is distributed with this package.
Please refer to it for further information on licensing.

******************************************************************************

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

*****************************************************************************/

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

#define DRAW_PAGE(a) DelayMsg(MT_DrwInvalidateRegion, (a)->SurfaceID, NULL);

#define ULD_TERMINATE       0x01
#define ULD_KEEP_PARAMETERS 0x02
#define ULD_REFRESH         0x04
#define ULD_REDRAW          0x08

#include <parasol/main.h>
#include <parasol/modules/xml.h>
#include <parasol/modules/document.h>
#include <parasol/modules/widget.h>
#include <parasol/modules/font.h>
#include <parasol/modules/display.h>
#include <parasol/modules/surface.h>

#include "hashes.h"

struct CoreBase  *CoreBase;
static struct SurfaceBase *SurfaceBase;
static struct FontBase    *FontBase;
static struct DisplayBase *DisplayBase;
static OBJECTPTR clDocument = NULL;
static OBJECTPTR modDisplay = NULL, modSurface = NULL, modFont = NULL, modDocument = NULL;
static struct RGB8 glHighlight = { 220, 220, 255, 255 };
static LONG glTranslateBufferSize = 0;
static STRING glTranslateBuffer = NULL;

#include "module_def.c"

//****************************************************************************

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
   void (*Routine)(objDocument *, objXML *, struct XMLTag *, struct XMLTag *, LONG *, LONG);
   LONG Flags;
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

// These flag values are in the upper word so that we can OR them with IPF and TAG constants.

#define FILTER_TABLE 0x80000000 // FILTER: Table
#define FILTER_ROW   0x40000000 // FILTER: Row
#define FILTER_ALL   (FILTER_TABLE|FILTER_ROW)

#define IXF_SIBLINGS   0x01
#define IXF_HOLDSTYLE  0x02
#define IXF_RESETSTYLE 0x04
#define IXF_CLOSESTYLE 0x08

#define TRF_BREAK    0x00000001
#define TRF_CONTINUE 0x00000002

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
   struct RGB8 Colour;    // Colour to use for bullet points (valid for LT_BULLET only).
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
   struct RGB8 Colour;       // Background colour
   struct RGB8 Highlight;    // Border highlight
   struct RGB8 Shadow;       // Border shadow
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
   struct RGB8 Highlight;
   struct RGB8 Shadow;
   struct RGB8 Colour;
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
   struct RGB8 Highlight;
   struct RGB8 Shadow;
   struct RGB8 Colour;
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

/*****************************************************************************
** Function prototypes.
*/

struct layout; // Pre-def

static ERROR  activate_edit(objDocument *, LONG CellIndex, LONG CursorIndex);
static ERROR  add_clip(objDocument *, struct SurfaceClip *, LONG, CSTRING Name, BYTE);
static LONG   add_drawsegment(objDocument *, LONG, LONG Stop, struct layout *, LONG, LONG, LONG, CSTRING);
static void   add_link(objDocument *, UBYTE EscapeCode, APTR, LONG, LONG, LONG, LONG, CSTRING);
static struct docresource * add_resource_id(objDocument *, LONG, LONG);
static LONG   add_tabfocus(objDocument *, UBYTE, LONG);
static void   add_template(objDocument *, objXML *, struct XMLTag *);
static void   advance_tabfocus(objDocument *, BYTE);
static LONG   calc_page_height(objDocument *, LONG, LONG, LONG);
static void   calc_scroll(objDocument *);
static void   check_mouse_click(objDocument *, LONG X, LONG Y);
static void   check_mouse_pos(objDocument *, LONG, LONG);
static void   check_mouse_release(objDocument *, LONG X, LONG Y);
static ERROR  convert_xml_args(objDocument *, struct XMLAttrib *, LONG);
static LONG   create_font(CSTRING, CSTRING, LONG);
static void   deactivate_edit(objDocument *, BYTE);
static void   deselect_text(objDocument *);
static void   draw_background(objDocument *, objSurface *, objBitmap *);
static void   draw_document(objDocument *, objSurface *, objBitmap *);
static void   draw_border(objDocument *, objSurface *, objBitmap *);
static void   exec_link(objDocument *, LONG Index);
static ERROR  extract_script(objDocument *, CSTRING Link, OBJECTPTR *Script, CSTRING *Function, CSTRING *);
static void   error_dialog(CSTRING, CSTRING, ERROR);
static LONG   find_segment(objDocument *, LONG Index, LONG);
static LONG   find_tabfocus(objDocument *, UBYTE Type, LONG Reference);
static void   fix_command(STRING, STRING *);
static ERROR  flash_cursor(objDocument *, LARGE, LARGE);
static void   free_links(objDocument *);
static STRING get_font_style(LONG);
//static LONG   get_line_from_index(objDocument *, LONG Index);
static LONG   getutf8(CSTRING, LONG *);
static ERROR  insert_escape(objDocument *, LONG *, WORD, APTR, LONG);
static void   insert_paragraph_end(objDocument *, LONG *);
static void   insert_paragraph_start(objDocument *, LONG *, escParagraph *);
static ERROR  insert_string(CSTRING, STRING, LONG, LONG, LONG);
static ERROR  insert_text(objDocument *, LONG *, CSTRING, LONG, BYTE);
static ERROR  insert_xml(objDocument *, objXML *, struct XMLTag *, LONG, UBYTE);
static void   key_event(objDocument *, evKey *, LONG);
static ERROR  keypress(objDocument *, LONG, LONG, LONG);
static void   layout_doc(objDocument *);
static LONG   layout_section(objDocument *, LONG, objFont **, LONG, LONG, LONG *, LONG *, LONG, LONG, LONG, LONG, BYTE *);
static ERROR  load_doc(objDocument *, CSTRING, BYTE, BYTE);
static objFont * lookup_font(LONG, CSTRING);
static LONG   parse_tag(objDocument *, objXML *, struct XMLTag *, LONG *, LONG);
#ifdef DEBUG
static void   print_xmltree(struct XMLTag *, LONG *) __attribute__ ((unused));
#endif
#ifdef DBG_LINES
static void print_sorted_lines(objDocument *) __attribute__ ((unused));
#endif
static ERROR  process_page(objDocument *, objXML *);
static void   process_parameters(objDocument *, CSTRING);
static void   redraw(objDocument *, BYTE);
static ERROR  report_event(objDocument *, LARGE Event, APTR EventData, CSTRING StructName);
static void   reset_cursor(objDocument *);
static ERROR  resolve_fontx_by_index(objDocument *, LONG Index, LONG *CharX);
static ERROR  resolve_font_pos(objDocument *, LONG Segment, LONG X, LONG *, LONG *BytePos);
static ERROR  safe_translate(STRING, LONG, LONG);
static void   set_focus(objDocument *, LONG, STRING);
static void   show_bookmark(objDocument *, STRING Bookmark);
static void   style_check(objDocument *, LONG *Index);
static void   tag_object(objDocument *, CSTRING, LONG class_id, struct XMLTag *, objXML *XML, struct XMLTag *Tag, struct XMLTag *Child, LONG *Index, LONG Flags, UBYTE *, UBYTE *, LONG *);
static void   tag_xml_content(objDocument *, objXML *, struct XMLTag *, WORD);
static ERROR  unload_doc(objDocument *, BYTE);
static BYTE   valid_object(objDocument *, OBJECTPTR);
static BYTE   valid_objectid(objDocument *, OBJECTID);
static BYTE   view_area(objDocument *, LONG Left, LONG Top, LONG Right, LONG Bottom);
static LONG   xml_content_len(struct XMLTag *) __attribute__ ((unused));
static void   xml_extract_content(struct XMLTag *, UBYTE *, LONG *, BYTE) __attribute__ ((unused));

#ifdef DBG_STREAM
static void print_stream(objDocument *Self, STRING Stream) __attribute__ ((unused));
#endif

struct FontEntry {
   objFont *Font;
   LONG Point;
};

static STRING exsbuffer = NULL;
static LONG exsbuffer_size = 0;
static struct FontEntry *glFonts = NULL;
static LONG glTotalFonts = 0;
static LONG glMaxFonts = 0;
static struct tagroutine glTags[];

// Control code format: ESC,Code,Length[2],ElementID[4]...Data...,Length[2],ESC

#define ESC_LEN_START 8
#define ESC_LEN_END   3
#define ESC_LEN (ESC_LEN_START + ESC_LEN_END)

#define remove_cursor(a)           draw_cursor((a),FALSE)
// Calculate the length of an escape sequence
#define ESC_ELEMENTID(a)        (((LONG *)(a))[1])
#define ESCAPE_CODE(stream, index) ((stream)[(index)+1]) // Escape codes are only 1 byte long
#define ESCAPE_LEN(a)              ((((a)[2])<<8) | ((a)[3]))
#define ESCAPE_DATA(stream, index) ((APTR)(stream) + (index) + ESC_LEN_START)
// Move to the next character - handles UTF8 only, no escape sequence handling
#define NEXT_CHAR(s,i) { if ((s)[(i)] IS CTRL_CODE) i += ESCAPE_LEN(s+i); else { i++; while (((s)[(i)] & 0xc0) IS 0x80) (i)++; } }
#define PREV_CHAR(s,i) { if ((s)[(i)-1] IS CTRL_CODE) (i) -= ((s)[(i)-3]<<8) | ((s)[(i)-2]); else i--; }

#define START_TEMPLATE(TAG,XML,ARGS) \
   struct XMLTag *savetag; \
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

static const struct FieldArray clFields[];
static const struct ActionArray clDocumentActions[];
static const struct MethodArray clDocumentMethods[];

static FIELD FID_LayoutSurface;

//****************************************************************************

ERROR CMDInit(OBJECTPTR argModule, struct CoreBase *argCoreBase)
{
   CoreBase = argCoreBase;

   GetPointer(argModule, FID_Master, &modDocument);

   if (LoadModule("surface", MODVERSION_SURFACE, &modSurface, &SurfaceBase) != ERR_Okay) return ERR_InitModule;
   if (LoadModule("display", MODVERSION_DISPLAY, &modDisplay, &DisplayBase) != ERR_Okay) return ERR_InitModule;
   if (LoadModule("font", MODVERSION_FONT, &modFont, &FontBase) != ERR_Okay) return ERR_InitModule;

   FID_LayoutSurface = StrHash("LayoutSurface", 0);

   OBJECTPTR style;
   if (!FindPrivateObject("glStyle", &style)) {
      char buffer[32];
      if (!acGetVar(style, "/colours/@DocumentHighlight", buffer, sizeof(buffer))) {
         StrToColour(buffer, &glHighlight);
      }
   }

   return CreateObject(ID_METACLASS, 0, &clDocument,
      FID_BaseClassID|TLONG,   ID_DOCUMENT,
      FID_ClassVersion|TFLOAT, VER_DOCUMENT,
      FID_Name|TSTR,           "Document",
      FID_Category|TLONG,      CCF_GUI,
      FID_Flags|TLONG,         CLF_PROMOTE_INTEGRAL|CLF_PRIVATE_ONLY,
      FID_Actions|TPTR,        clDocumentActions,
      FID_Methods|TARRAY,      clDocumentMethods,
      FID_Fields|TARRAY,       clFields,
      FID_Size|TLONG,          sizeof(objDocument),
      FID_Path|TSTR,           MOD_PATH,
      FID_FileExtension|TSTR,  "*.rpl|*.ripple|*.rple",
      TAGEND);
}

ERROR CMDExpunge(void)
{
   {
      LogMsg("Freeing %d internally allocated fonts.", glTotalFonts);
      WORD i;
      for (i=0; i < glTotalFonts; i++) acFree(glFonts[i].Font);
   }

   if (exsbuffer)         { FreeResource(exsbuffer); exsbuffer = NULL; }
   if (glFonts)           { FreeResource(glFonts); glFonts = NULL; }
   if (glTranslateBuffer) { FreeResource(glTranslateBuffer); glTranslateBuffer = NULL; }

   if (modDisplay) { acFree(modDisplay);  modDisplay = NULL; }
   if (modSurface) { acFree(modSurface);  modSurface = NULL; }
   if (modFont)    { acFree(modFont);     modFont = NULL; }

   if (clDocument) { acFree(clDocument);  clDocument = NULL; }
   return ERR_Okay;
}

static ERROR CMDOpen(OBJECTPTR Module)
{
   SetPointer(Module, FID_FunctionList, glFunctions);
   return ERR_Okay;
}

/*****************************************************************************

-FUNCTION-
CharLength: Returns the length of any character or escape code in a document data stream.

This function will compute the byte-length of any UTF-8 character sequence or escape code in a document's data stream.

-INPUT-
obj(Document) Document: The document to query.
int Index: The byte index of the character to inspect.

-RESULT-
int: The length of the character is returned, or 0 if an error occurs.

-END-

*****************************************************************************/

static LONG docCharLength(objDocument *Self, LONG Index)
{
   if (!Self) return 0;

   if (Self->Stream[Index] IS CTRL_CODE) {
      return ESCAPE_LEN(Self->Stream + Index);
   }
   else {
      LONG len;
      len = 1;
      while (((Self->Stream)[len] & 0xc0) IS 0x80) len++;
      return len;
   }
}

//****************************************************************************

INLINE LONG find_cell(objDocument *Self, LONG ID, ULONG EditHash)
{
   UBYTE *stream;
   if (!(stream = Self->Stream)) return -1;

   LONG i = 0;
   while (stream[i]) {
      if (stream[i] IS CTRL_CODE) {
         if (ESCAPE_CODE(stream, i) IS ESC_CELL) {
            escCell *cell = ESCAPE_DATA(stream, i);
            if ((ID) AND (ID IS cell->CellID)) {
               return i;
            }
            else if ((EditHash) AND (EditHash IS cell->EditHash)) {
               return i;
            }
         }
      }
      NEXT_CHAR(stream, i);
   }

   return -1;
}

//****************************************************************************

INLINE struct DocEdit * find_editdef(objDocument *Self, ULONG Hash)
{
   struct DocEdit *edit;

   for (edit=Self->EditDefs; edit; edit=edit->Next) {
      if (edit->NameHash IS Hash) return edit;
   }

   return NULL;
}

//****************************************************************************

INLINE void layout_doc_fast(objDocument *Self)
{
   drwForbidDrawing();
      AdjustLogLevel(2);
      layout_doc(Self);
      AdjustLogLevel(-2);
   drwPermitDrawing();
}

#include "class/fields.c"
#include "class/document_class.c"
#include "functions.c"
#include "tags.c"

//****************************************************************************

PARASOL_MOD(CMDInit, NULL, CMDOpen, CMDExpunge, MODVERSION_DOCUMENT)
