/*********************************************************************************************************************

The source code of the Parasol project is made publicly available under the terms described in the LICENSE.TXT file
that is distributed with this package.  Please refer to it for further information on licensing.

**********************************************************************************************************************

-MODULE-
Document: Provides document display and editing facilities.

The Document module exports a small number of functions in support of the @Document class.
-END-

THE BYTE CODE
-------------
The text stream is an array of escape code structures that indicate font style, paragraphs, hyperlinks, text etc.

PARAGRAPH MANAGEMENT
--------------------
When document text is drawn, we maintain a 'line list' where the index of each line is recorded (see font_wrap()).
This allows us to do things like Ctrl-K to delete a 'line'. It also allows us to remember the pixel width and height
of each line, which is important for highlighting selected text.

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
Fill:         Background fill for the table.
Thickness:    Size of the Stroke pattern.
Stroke        Stroke pattern for border.
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

//#define _DEBUG
//#define DBG_LAYOUT
//#define DBG_LAYOUT_ESCAPE // Do not use unless you want a lot of detail
//#define DBG_WORDWRAP
//#define DBG_STREAM
//#define DBG_LINES // Print list of lines (segments)
//#define GUIDELINES // Clipping guidelines
//#define GUIDELINES_CONTENT // Segment guidelines

#ifdef DBG_LAYOUT
 #define DLAYOUT(...)   log.msg(__VA_ARGS__)
#else
 #define DLAYOUT(...)
#endif

#ifdef DBG_WORDWRAP
 #define DWRAP(...)   log.msg(__VA_ARGS__)
#else
 #define DWRAP(...)
#endif

#define PRV_DOCUMENT_MODULE
#define PRV_SURFACE

#include <parasol/main.h>
#include <parasol/modules/xml.h>
#include <parasol/modules/document.h>
#include <parasol/modules/font.h>
#include <parasol/modules/display.h>
#include <parasol/modules/svg.h>
#include <parasol/modules/vector.h>
#include <parasol/strings.hpp>

#include <iomanip>
#include <algorithm>
#include <array>
#include <variant>
#include <stack>
#include <cmath>
#include "hashes.h"

typedef int INDEX;
using SEGINDEX = int;

static const LONG MAX_PAGEWIDTH    = 200000;
static const LONG MAX_PAGEHEIGHT   = 200000;
static const LONG MIN_PAGE_WIDTH   = 20;
static const LONG MAX_DEPTH        = 1000;  // Limits the number of tables-within-tables
static const LONG BULLET_WIDTH     = 14;    // Minimum column width for bullet point lists
static const LONG BORDER_SIZE      = 1;
static const LONG WIDTH_LIMIT      = 4000;
static const LONG LINE_HEIGHT      = 16;    // Default line height (measured as an average) for the page
static const LONG DEFAULT_INDENT   = 30;
static const LONG DEFAULT_FONTSIZE = 10;
static const LONG MAX_VSPACING     = 20;
static const LONG MAX_LEADING      = 20;
static const LONG NOTSPLIT         = -1;
static const DOUBLE MIN_LINEHEIGHT = 0.001;
static const DOUBLE MIN_VSPACING   = 0.001;
static const DOUBLE MIN_LEADING    = 0.001;

enum {
   COND_NOT_EQUAL=1,
   COND_EQUAL,
   COND_LESS_THAN,
   COND_LESS_EQUAL,
   COND_GREATER_THAN,
   COND_GREATER_EQUAL
};

using namespace pf;

JUMPTABLE_CORE
JUMPTABLE_FONT
JUMPTABLE_DISPLAY
JUMPTABLE_VECTOR

enum class ESC : char {
   NIL = 0,
   TEXT,
   FONT,
   FONTCOLOUR,
   UNDERLINE,
   BACKGROUND,
   INVERSE,
   VECTOR,
   LINK,
   TABDEF,
   PARAGRAPH_END,   // 10
   PARAGRAPH_START,
   LINK_END,
   ADVANCE,
   LIST_START,
   LIST_END,        // 15
   TABLE_START,
   TABLE_END,
   ROW,
   CELL,
   CELL_END,        // 20
   ROW_END,
   SET_MARGINS,
   INDEX_START,
   INDEX_END,
   XML,
   IMAGE,
   // End of list - NB: PLEASE UPDATE escCode() IF YOU ADD NEW CODES
   END
};

DEFINE_ENUM_FLAG_OPERATORS(ESC)

enum class ULD : UBYTE {
   NIL             = 0,
   TERMINATE       = 0x01,
   KEEP_PARAMETERS = 0x02,
   REFRESH         = 0x04,
   REDRAW          = 0x08
};

DEFINE_ENUM_FLAG_OPERATORS(ULD)

enum class NL : char {
   NONE = 0,
   PARAGRAPH
};

DEFINE_ENUM_FLAG_OPERATORS(NL)

enum class WRAP : char {
   DO_NOTHING = 0,
   EXTEND_PAGE,
   WRAPPED
};

DEFINE_ENUM_FLAG_OPERATORS(WRAP)

enum {
   STATE_OUTSIDE=0,
   STATE_ENTERED,
   STATE_INSIDE
};

enum class IPF : ULONG {
   NIL = 0,
   NO_CONTENT  = 0x0001,
   STRIP_FEEDS = 0x0002,
   CHECK_ELSE  = 0x0004,
   // These flag values are in the upper word so that we can or them with IPF and TAG constants.
   FILTER_TABLE = 0x80000000, // The tag is restricted to use within <table> sections.
   FILTER_ROW   = 0X40000000, // The tag is restricted to use within <row> sections.
   FILTER_ALL   = 0xffff0000
};

DEFINE_ENUM_FLAG_OPERATORS(IPF)

enum class TAG : ULONG {
   NIL = 0,
   CHILDREN     = 0x00000001, // Children are compulsory for the tag to have an effect
   CONTENT      = 0x00000002, // Tag has a direct impact on text content or the page layout
   CONDITIONAL  = 0x00000004, // Tag is a conditional statement
   INSTRUCTION  = 0x00000008, // Tag is an executable instruction
   ROOT         = 0x00000010, // Tag is limited to use at the root of the document
   PARAGRAPH    = 0x00000020, // Tag results in paragraph formatting (will force some type of line break)
   OBJECTOK     = 0x00000040, // It is OK for this tag to be used within any object
   // These flag values are in the upper word so that we can or them with IPF and TAG constants.
   FILTER_TABLE = 0x80000000, // The tag is restricted to use within <table> sections.
   FILTER_ROW   = 0X40000000, // The tag is restricted to use within <row> sections.
   FILTER_ALL   = 0xffff0000
};

DEFINE_ENUM_FLAG_OPERATORS(TAG)

enum class TRF : ULONG {
   NIL      = 0,
   BREAK    = 0x00000001,
   CONTINUE = 0x00000002
};

DEFINE_ENUM_FLAG_OPERATORS(TRF)

//********************************************************************************************************************

static std::string glHighlight = "rgb(219,219,255,255)";

static OBJECTPTR clDocument = NULL;
static OBJECTPTR modDisplay = NULL, modFont = NULL, modDocument = NULL, modVector = NULL;

class extDocument;

//********************************************************************************************************************

struct deLinkActivated {
   std::map<std::string, std::string> Values;  // All key-values associated with the link.
};

//********************************************************************************************************************
// Every instruction in the document stream is represented by a StreamCode entity.  The Code refers to what the thing
// is, while the UID hash refers to further information in the Codes table.

struct StreamCode {
   ESC Code;  // Type
   ULONG UID; // Lookup for the Codes table

   StreamCode() : Code(ESC::NIL), UID(0) { }
   StreamCode(ESC pCode, ULONG pID) : Code(pCode), UID(pID) { }
};

using RSTREAM = std::vector<StreamCode>;

//********************************************************************************************************************
// StreamChar provides indexing to specific characters in the stream.  It is designed to handle positional changes so
// that text string boundaries can be crossed without incident.
//
// The Index and Offset are set to -1 if the StreamChar is invalidated.

struct StreamChar {
   INDEX Index;     // A TEXT code position within the stream
   size_t Offset;   // Specific character offset within the escText.Text string

   StreamChar() : Index(-1), Offset(-1) { }
   StreamChar(INDEX pIndex, ULONG pOffset) : Index(pIndex), Offset(pOffset) { }
   StreamChar(INDEX pIndex) : Index(pIndex), Offset(0) { }

   bool operator==(const StreamChar &Other) const {
      return (this->Index == Other.Index) and (this->Offset == Other.Offset);
   }

   bool operator<(const StreamChar &Other) const {
      if (this->Index < Other.Index) return true;
      else if ((this->Index IS Other.Index) and (this->Offset < Other.Offset)) return true;
      else return false;
   }
   
   bool operator>(const StreamChar &Other) const {
      if (this->Index > Other.Index) return true;
      else if ((this->Index IS Other.Index) and (this->Offset > Other.Offset)) return true;
      else return false;
   }
   
   bool operator<=(const StreamChar &Other) const {
      if (this->Index < Other.Index) return true;
      else if ((this->Index IS Other.Index) and (this->Offset <= Other.Offset)) return true;
      else return false;
   }
   
   bool operator>=(const StreamChar &Other) const {
      if (this->Index > Other.Index) return true;
      else if ((this->Index IS Other.Index) and (this->Offset >= Other.Offset)) return true;
      else return false;
   }

   void operator+=(const LONG Value) {
      Offset += Value;
   }
   
   inline void reset() { Index = -1; Offset = -1; }
   inline bool valid() { return Index != -1; }
   inline bool valid(const RSTREAM &Stream) { return (Index != -1) and (Index < INDEX(Stream.size())); }

   inline void set(INDEX pIndex, ULONG pOffset) {
      Index  = pIndex;
      Offset = pOffset;
   }    

   inline INDEX prevCode() {
      Index--;
      if (Index < 0) { Index = -1; Offset = -1; }
      else Offset = 0;
      return Index;
   }

   inline INDEX nextCode() {
      Offset = 0;
      Index++;
      return Index;
   }

   char getChar(extDocument *, const RSTREAM &);
   char getChar(extDocument *, const RSTREAM &, LONG Seek);
   char getPrevChar(extDocument *, const RSTREAM &);
   void eraseChar(extDocument *, RSTREAM &); // Erase a character OR an escape code.
   void nextChar(extDocument *, const RSTREAM &);
   void prevChar(extDocument *, const RSTREAM &);
};


//********************************************************************************************************************
// Tab is used to represent interactive entities within the document that can be tabbed to.

struct Tab {
   // The Ref is a UID for the Type, so you can use it to find the tab in the document stream
   LONG  Ref;        // For TT_OBJECT: ObjectID; TT_LINK: LinkID
   LONG  XRef;       // For TT_OBJECT: SurfaceID (if found)
   UBYTE Type;       // TT_LINK, TT_OBJECT
   bool  Active;     // true if the tabbable entity is active/visible
};

//********************************************************************************************************************

static ULONG glEscapeCodeID = 1;

class BaseCode {
public:
   ULONG UID;   // Unique identifier for lookup
   ESC Code = ESC::NIL; // Escape code
   
   BaseCode() { UID = glEscapeCodeID++; }
   BaseCode(ESC pCode) : Code(pCode) { UID = glEscapeCodeID++; }
};

//********************************************************************************************************************

struct escFont : public BaseCode {
   WORD FontIndex;      // Font lookup
   FSO  Options;
   std::string Fill;    // Font fill

   escFont(): FontIndex(-1), Options(FSO::NIL), Fill("rgb(0,0,0)") { Code = ESC::FONT; }

   objFont * getFont();
};

struct style_status {
   struct escFont FontStyle;
   struct process_table * Table;
   struct escList * List;
   std::string Face;
   WORD Point;
   bool FontChange;      // A major font change has occurred (e.g. face, point size)
   bool StyleChange;     // A minor style change has occurred (e.g. font colour)

   style_status() : Table(NULL), List(NULL), Face(""), Point(0), FontChange(false), StyleChange(false) { }
};

//********************************************************************************************************************
// Refer to layout::add_drawsegment().  A segment represents graphical content, which can be in the form of text,
// graphics or both.  A segment can consist of one line only - so if the layout process encounters a boundary causing 
// wordwrap then a new segment must be created.

struct DocSegment {
   StreamChar Start;      // Starting index (including character if text)
   StreamChar Stop;       // Stop at this index/character
   StreamChar TrimStop;   // The stopping point when whitespace is removed
   FloatRect Area;        // Dimensions of the segment.
   UWORD BaseLine;        // Base-line - this is the height of the largest font down to the base line
   UWORD AlignWidth;      // Full width of this segment if it were non-breaking
   UWORD Depth;           // Branch depth associated with the segment - helps to differentiate between inner and outer tables
   bool  Edit;            // true if this segment represents content that can be edited
   bool  TextContent;     // true if there is TEXT in this segment
   bool  FloatingVectors; // true if there are user defined vectors in this segment with independent X,Y coordinates
   bool  AllowMerge;      // true if this segment can be merged with siblings that have AllowMerge set to true
};

struct DocClip {
   DOUBLE Left, Top, Right, Bottom;
   INDEX Index; // The stream index of the object/table/item that is creating the clip.
   bool Transparent; // If true, wrapping will not be performed around the clip region.
   std::string Name;

   DocClip() = default;

   DocClip(DOUBLE pLeft, DOUBLE pTop, DOUBLE pRight, DOUBLE pBottom, LONG pIndex, bool pTransparent, const std::string &pName) :
      Left(pLeft), Top(pTop), Right(pRight), Bottom(pBottom), Index(pIndex), Transparent(pTransparent), Name(pName) { }
};

struct DocEdit {
   LONG MaxChars;
   std::string Name;
   std::string OnEnter, OnExit, OnChange;
   std::vector<std::pair<std::string, std::string>> Args;
   bool LineBreaks;

   DocEdit() : MaxChars(-1), Args(0), LineBreaks(false) { }
};

struct escLink;
struct escCell;

// DocLink describes an area on the page that can be interacted with as a link or table cell.
// The link will be associated with a segment and an originating stream code.
// 
// TODO: We'll need to swap the X/Y/Width/Height variables for a vector path that represents the link
// area.  This will allow us to support transforms correctly, as well as links that need to accurately
// map to vector shapes.

struct DocLink {
   std::variant<escLink *, escCell *> Ref;
   DOUBLE X, Y, Width, Height;
   SEGINDEX Segment;
   ESC  BaseCode;

   DocLink(ESC pCode, std::variant<escLink *, escCell *> pRef, SEGINDEX pSegment, LONG pX, LONG pY, LONG pWidth, LONG pHeight) :
       Ref(pRef), X(pX), Y(pY), Width(pWidth), Height(pHeight), Segment(pSegment), BaseCode(pCode) { }

   DocLink() : X(0), Y(0), Width(0), Height(0), Segment(0), BaseCode(ESC::NIL) { }

   escLink * asLink() { return std::get<escLink *>(Ref); }
   escCell * asCell() { return std::get<escCell *>(Ref); }
   void exec(extDocument *);
};

struct DocMouseOver {
   std::string Function; // Name of function to call.
   LONG Top, Left, Bottom, Right;
   LONG ElementID;
};

class SortedSegment { // Efficient lookup to the DocSegment array, sorted by vertical position
public:
   SEGINDEX Segment;
   DOUBLE Y;
};

static const char IXF_SIBLINGS   = 0x01;
static const char IXF_HOLDSTYLE  = 0x02;
static const char IXF_RESETSTYLE = 0x04;
static const char IXF_CLOSESTYLE = 0x08;

static const WORD PXF_ARGS      = 0x0001;
static const WORD PXF_TRANSLATE = 0x0002;

enum class LINK : UBYTE {
   NIL = 0,
   HREF,
   FUNCTION
};

DEFINE_ENUM_FLAG_OPERATORS(LINK)

struct tablecol {
   UWORD PresetWidth;
   UWORD MinWidth;   // For assisting layout
   UWORD Width;
};

//********************************************************************************************************************

struct escText : public BaseCode {
   std::string Text;
   bool Formatted = false;
   SEGINDEX Segment = -1; // Reference to the first segment that manages this text string.

   escText() { Code = ESC::TEXT; }
   escText(std::string pText) : Text(pText) { Code = ESC::TEXT; }
   escText(std::string pText, bool pFormatted) : Text(pText), Formatted(pFormatted) { Code = ESC::TEXT; }
};

struct escAdvance : public BaseCode {
   LONG X, Y;

   escAdvance() : X(0), Y(0) { Code = ESC::ADVANCE; }
};

struct escIndex : public BaseCode {
   ULONG NameHash;     // The name of the index is held here as a hash
   LONG  ID;           // UID for matching to the correct escIndexEnd
   LONG  Y;            // The cursor's vertical position of when the index was encountered during layout
   bool Visible;       // true if the content inside the index is visible (this is the default)
   bool ParentVisible; // true if the nearest parent index(es) will allow index content to be visible.  true is the default.  This allows Hide/ShowIndex() to manage themselves correctly

   escIndex(ULONG pName, LONG pID, LONG pY, bool pVisible, bool pParentVisible) :
      NameHash(pName), ID(pID), Y(pY), Visible(pVisible), ParentVisible(pParentVisible) {
      Code = ESC::INDEX_START;
   }
};

struct escIndexEnd : public BaseCode {
   LONG ID; // UID matching to the correct escIndex
   escIndexEnd(LONG pID) : ID(pID) { Code = ESC::INDEX_END; }
};

struct escLink : public BaseCode {
   LINK Type;    // Link type (either a function or hyperlink)
   UWORD ID;
   FSO   Align;
   std::string PointerMotion; // Function to call for pointer motion events
   std::string Ref;
   std::vector<std::pair<std::string,std::string>> Args;

   escLink() : Type(LINK::NIL), ID(0), Align(FSO::NIL) {
      Code = ESC::LINK;
   }
};

struct escLinkEnd : public BaseCode {
   escLinkEnd() { Code = ESC::LINK_END; }
};

struct escImage : public BaseCode {
   LONG Width = 0, Height = 0; // Client can define fixed a width/height 
   objVectorRectangle *Rect = NULL;
   escImage() { Code = ESC::IMAGE; }
};

struct escList : public BaseCode {
   enum {
      ORDERED=0,
      BULLET,
      CUSTOM
   };

   std::string Fill;                 // Fill to use for bullet points (valid for BULLET only).
   std::vector<std::string> Buffer;  // Temp buffer, used for ordered lists
   LONG  Start = 1;                  // Starting value for ordered lists (default: 1)
   LONG  ItemIndent = BULLET_WIDTH;  // Minimum indentation for text printed for each item
   LONG  BlockIndent = BULLET_WIDTH; // Indentation for each set of items
   LONG  ItemNum = 0;
   LONG  OrderInsert = 0;
   FLOAT VSpacing = 0.5;        // Spacing between list items, expressed as a ratio
   UBYTE Type = BULLET;
   bool  Repass = false;

   escList() { Code = ESC::LIST_START; }
};

struct escListEnd : public BaseCode {
   escListEnd() { Code = ESC::LIST_END; }
};

struct escSetMargins : public BaseCode {
   WORD Left = 0x7fff; WORD Top = 0x7fff; WORD Bottom = 0x7fff; WORD Right = 0x7fff;
   escSetMargins() { Code = ESC::SET_MARGINS; }
};

struct escVector : public BaseCode {
   OBJECTID ObjectID = 0;     // Reference to the vector
   CLASSID ClassID = 0;       // Precise class that the object belongs to, mostly for informative/debugging purposes
   ClipRectangle Margins;
   bool Embedded = false;     // true if object is embedded as part of the text stream (treated as if it were a character)
   bool Owned = false;        // true if the object is owned by a parent (not subject to normal document layout)
   bool IgnoreCursor = false; // true if the client has set fixed values for both X and Y
   bool BlockRight = false;   // true if no text may be printed to the right of the object
   bool BlockLeft = false;    // true if no text may be printed to the left of the object
   escVector() { Code = ESC::VECTOR; }
};

struct escXML : public BaseCode {
   OBJECTID ObjectID = 0;   // Reference to the object
   bool Owned = false;      // true if the object is owned by a parent (not subject to normal document layout)
   escXML() { Code = ESC::XML; }
};

struct escTable : public BaseCode {
   struct escTable *Stack = NULL;
   std::vector<tablecol> Columns; // Table column management
   std::string Fill, Stroke;
   WORD  CellVSpacing = 0, CellHSpacing = 0; // Spacing between each cell
   WORD  CellPadding = 0;             // Spacing inside each cell (margins)
   LONG  RowWidth = 0;                // Assists in the computation of row width
   DOUBLE  X = 0, Y = 0;              // Calculated X/Y coordinate of the table
   LONG  Width = 0, Height = 0;       // Calculated table width/height
   LONG  MinWidth = 0, MinHeight = 0; // User-determined minimum table width/height
   LONG  Rows = 0;                    // Total number of rows in table
   LONG  RowIndex = 0;                // Current row being processed, generally for debugging
   LONG  CursorX = 0, CursorY = 0;    // Cursor coordinates
   LONG  TotalClips = 0;              // Temporary record of Document->Clips.size()
   UWORD Thickness  = 0;              // Border thickness
   UBYTE ComputeColumns = 0;
   bool  WidthPercent   = false;   // true if width is a percentage
   bool  HeightPercent  = false;   // true if height is a percentage
   bool  CellsExpanded  = false;   // false if the table cells have not been expanded to match the inside table width
   bool  ResetRowHeight = false;   // true if the height of all rows needs to be reset in the current pass
   bool  Wrap = false;
   bool  Thin = false;
   // Entry followed by the minimum width of each column
   escTable() { Code = ESC::TABLE_START; }

   void computeColumns() { // Compute the default column widths
      if (!ComputeColumns) return;

      ComputeColumns = 0;
      CellsExpanded = false;

      if (!Columns.empty()) {
         for (unsigned j=0; j < Columns.size(); j++) {
            //if (ComputeColumns IS 1) {
            //   Columns[j].Width = 0;
            //   Columns[j].MinWidth = 0;
            //}

            if (Columns[j].PresetWidth & 0x8000) { // Percentage width value
               Columns[j].Width = (DOUBLE)((Columns[j].PresetWidth & 0x7fff) * Width) * 0.01;
            }
            else if (Columns[j].PresetWidth) { // Fixed width value
               Columns[j].Width = Columns[j].PresetWidth;
            }
            else Columns[j].Width = 0;

            if (Columns[j].MinWidth > Columns[j].Width) Columns[j].Width = Columns[j].MinWidth;
         }
      }
      else Columns.clear();
   }
};

struct escTableEnd : public BaseCode {
   escTableEnd() { Code = ESC::TABLE_END; }
};

class escParagraph : public BaseCode {
   public:
   std::string Value = "";
   LONG   X, Y, Height;
   LONG   BlockIndent;
   LONG   ItemIndent;
   DOUBLE Indent;
   DOUBLE VSpacing;     // Trailing whitespace, expressed as a ratio of the default line height
   DOUBLE LeadingRatio; // Leading whitespace (minimum amount of space from the end of the last paragraph).  Expressed as a ratio of the default line height
   // Options
   bool Relative;
   bool ListItem;
   bool Trim;

   escParagraph() : BaseCode(ESC::PARAGRAPH_START), X(0), Y(0), Height(0),
      BlockIndent(0), ItemIndent(0), Indent(0), VSpacing(1.0), LeadingRatio(1.0),
      Relative(false), ListItem(false), Trim(false) { }

   void applyStyle(const style_status &Style) {
      VSpacing     = Style.List->VSpacing;
      BlockIndent  = Style.List->BlockIndent;
      ItemIndent   = Style.List->ItemIndent;
   }
};

struct escParagraphEnd : public BaseCode {
   escParagraphEnd() : BaseCode(ESC::PARAGRAPH_END) { }
};

struct escRow : public BaseCode {
   LONG  Y = 0;
   LONG  RowHeight = 0; // Height of all cells on this row, used when drawing the cells
   LONG  MinHeight = 0;
   std::string Stroke, Fill;
   bool  VerticalRepass = false;

   escRow() : BaseCode(ESC::ROW) { }
};

struct escRowEnd : public BaseCode {
   escRowEnd() : BaseCode(ESC::ROW_END) { }
};

struct escCell : public BaseCode {
   LONG CellID;         // Identifier for the matching escCellEnd
   LONG Column;         // Column number that the cell starts in
   LONG ColSpan;        // Number of columns spanned by this cell (normally set to 1)
   LONG RowSpan;        // Number of rows spanned by this cell
   LONG AbsX, AbsY;     // Cell coordinates, these are absolute
   LONG Width, Height;  // Width and height of the cell
   std::string OnClick; // Name of an onclick function
   std::string EditDef; // The edit definition that this cell is linked to (if any)
   std::vector<std::pair<std::string, std::string>> Args;
   std::string Stroke;
   std::string Fill;

   escCell(LONG pCellID, LONG pColumn) :
      BaseCode(ESC::CELL), CellID(pCellID), Column(pColumn),
      ColSpan(1), RowSpan(1), AbsX(0), AbsY(0), Width(0), Height(0)
      { }
};

struct escCellEnd : public BaseCode {
   LONG CellID = 0;    // Matching identifier from escCell
   escCellEnd() : BaseCode(ESC::CELL_END) { }
};

//********************************************************************************************************************

class extDocument : public objDocument {
   public:
   FUNCTION EventCallback;
   objXML *XML;             // Source XML document
   objXML *InsertXML;       // For temporary XML parsing by the InsertXML method
   objXML *Templates;       // All templates for the current document are stored here
   objXML *InjectXML;
   objXML::TAGS *InjectTag;
   objXML::TAGS *HeaderTag;
   objXML::TAGS *FooterTag;
   objXML::TAGS *BodyTag; // Reference to the children of the body tag, if any
   XMLTag *PageTag;
   std::unordered_map<std::string, std::string> Vars;
   std::unordered_map<std::string, std::string> Params;
   std::string Path;               // Optional file to load on Init()
   std::string PageName;           // Page name to load from the Path
   std::string Bookmark;           // Bookmark name processed from the Path
   std::string WorkingPath;        // String storage for the WorkingPath field
   RSTREAM Stream;                 // Internal stream buffer
   OBJECTPTR CurrentObject;
   OBJECTPTR UserDefaultScript;  // Allows the developer to define a custom default script.
   OBJECTPTR DefaultScript;
   struct style_status Style;
   struct style_status RestoreStyle;
   std::vector<DocSegment> Segments;
   std::vector<SortedSegment> SortSegments; // Used for UI interactivity when determining who is front-most
   std::vector<DocClip> Clips;
   std::vector<DocLink> Links;
   std::vector<DocMouseOver> MouseOverChain;
   std::vector<struct docresource> Resources;
   std::vector<Tab> Tabs;
   std::vector<struct EditCell> EditCells;
   std::vector<OBJECTPTR> LayoutResources;
   std::unordered_map<std::string, struct DocEdit> EditDefs;
   std::unordered_map<ULONG, std::variant<escText, escAdvance, escTable, escTableEnd, escRow, escRowEnd, escParagraph,
      escParagraphEnd, escCell, escCellEnd, escLink, escLinkEnd, escList, escListEnd, escIndex, escIndexEnd,
      escFont, escVector, escSetMargins, escXML, escImage>> Codes;
   std::array<std::vector<FUNCTION>, size_t(DRT::MAX)> Triggers;
   std::vector<const XMLTag *> TemplateArgs; // If a template is called, the tag is referred here so that args can be pulled from it
   struct DocEdit *ActiveEditDef; // As for ActiveEditCell, but refers to the active editing definition
   StreamChar SelectStart;    // Selection start (stream index)
   StreamChar SelectEnd;      // Selection end (stream index)
   StreamChar CursorIndex;    // Position of the cursor if text is selected, or edit mode is active.  It reflects the position at which entered text will be inserted.
   StreamChar SelectIndex;    // The end of the selected text area, if text is selected.
   DOUBLE VPWidth, VPHeight;
   LONG   UniqueID;          // Use for generating unique/incrementing ID's, e.g. cell ID
   objVectorScene *Scene;    // A document specific scene is required to keep our resources away from the host
   objVectorViewport *View;  // View viewport - this contains the page and serves as the page's scrolling area
   objVectorViewport *Page;  // Page viewport - this holds the graphics content
   LONG   MinPageWidth;      // Internal value for managing the page width, speeds up layout processing
   FLOAT  PageWidth;         // Width of the widest section of the document page.  Can be pre-defined for a fixed width.
   LONG   LinkIndex;         // Currently selected link (mouse over)
   LONG   CalcWidth;         // Final page width calculated from the layout process
   LONG   LoopIndex;
   LONG   ElementCounter;       // Counter for element ID's
   LONG   XPosition, YPosition; // Scrolling offset
   LONG   AreaX, AreaY, AreaWidth, AreaHeight;
   LONG   ClickX, ClickY;
   LONG   ObjectCache;        // If counter > 0, data objects are persistent between document refreshes.   
   LONG   TemplatesModified;  // Track modifications to Self->Templates
   LONG   BreakLoop;
   LONG   GeneratedID;        // Unique ID that is regenerated on each load/refresh
   SEGINDEX ClickSegment;     // The index of the segment that the user clicked on
   SEGINDEX MouseOverSegment; // The index of the segment that the mouse is currently positioned over
   LONG   SelectCharX;        // The X coordinate of the SelectIndex character
   LONG   CursorCharX;        // The X coordinate of the CursorIndex character
   DOUBLE PointerX, PointerY; // Current pointer coordinates on the document surface
   TIMER  FlashTimer;         // For flashing the cursor
   LONG   ActiveEditCellID;   // If editing is active, this refers to the ID of the cell being edited
   ULONG  ActiveEditCRC;      // CRC for cell editing area, used for managing onchange notifications
   UWORD  Depth;              // Section depth - increases when do_layout() recurses, e.g. into tables
   UWORD  ParagraphDepth;     // Incremented when inside <p> tags
   UWORD  LinkID;             // Unique counter for links
   WORD   FocusIndex;         // Tab focus index
   WORD   Invisible;          // Incremented for sections within a hidden index
   UBYTE  Processing;         // If > 0, the page layout is being altered
   UBYTE  InTemplate;
   UBYTE  BkgdGfx;
   UBYTE  State:3;
   bool   RelPageWidth;     // Relative page width
   bool   UpdatingLayout;   // True if the page layout is in the process of being updated
   bool   VScrollVisible;   // True if the vertical scrollbar is visible to the user
   bool   MouseInPage;      // True if the mouse cursor is in the page area
   bool   PageProcessed;    // True if the parsing of page content has been completed
   bool   NoWhitespace;     // True if the parser should stop injecting whitespace characters
   bool   HasFocus;         // True if the main viewport has the focus
   bool   CursorSet;        // True if the mouse cursor image has been altered from the default
   bool   LMB;              // True if the LMB is depressed.
   bool   EditMode;
   bool   CursorState;      // True if the edit cursor is on, false if off.  Used for flashing of the cursor

   template <class T = BaseCode> T & insertCode(StreamChar &, T &);
   template <class T = BaseCode> T & reserveCode(StreamChar &);
   std::vector<SortedSegment> & getSortedSegments();
};

//********************************************************************************************************************

std::vector<SortedSegment> & extDocument::getSortedSegments() 
{
   if ((!SortSegments.empty()) or (Segments.empty())) return SortSegments;
   
   auto sortseg_compare = [&] (SortedSegment &Left, SortedSegment &Right) {
      if (Left.Y < Right.Y) return 1;
      else if (Left.Y > Right.Y) return -1;
      else {
         if (Segments[Left.Segment].Area.X < Segments[Right.Segment].Area.X) return 1;
         else if (Segments[Left.Segment].Area.X > Segments[Right.Segment].Area.X) return -1;
         else return 0;
      }
   };

   // Build the SortSegments array (unsorted)

   SortSegments.resize(Segments.size());     
   unsigned i;
   SEGINDEX seg;
   for (i=0, seg=0; seg < SEGINDEX(Segments.size()); seg++) {
      if ((Segments[seg].Area.Height > 0) and (Segments[seg].Area.Width > 0)) {
         SortSegments[i].Segment = seg;
         SortSegments[i].Y       = Segments[seg].Area.Y;
         i++;
      }
   }

   // Shell sort

   unsigned j, h = 1;
   while (h < SortSegments.size() / 9) h = 3 * h + 1;

   for (; h > 0; h /= 3) {
      for (auto i=h; i < SortSegments.size(); i++) {
         SortedSegment temp = SortSegments[i];
         for (j=i; (j >= h) and (sortseg_compare(SortSegments[j - h], temp) < 0); j -= h) {
            SortSegments[j] = SortSegments[j - h];
         }
         SortSegments[j] = temp;
      }
   }

   return SortSegments;
}

//********************************************************************************************************************
// Inserts an escape sequence into the text stream.

template <class T> T & extDocument::insertCode(StreamChar &Cursor, T &Code)
{
   // All escape codes are saved to a global container.

   if (Codes.contains(Code.UID)) { 
      // Sanity check - is the UID unique?  The caller probably needs to utilise glEscapeCodeID++
      // NB: At some point the re-use of codes should be allowed, e.g. escFont reversions would benefit from this.
      pf::Log log(__FUNCTION__);
      log.warning("Code #%d is already registered.", Code.UID);
   }
   else Codes[Code.UID] = Code;

   if (Cursor.Index IS INDEX(Stream.size())) {
      Stream.emplace_back(Code.Code, Code.UID);
   }
   else {
      const StreamCode insert(Code.Code, Code.UID);
      Stream.insert(Stream.begin() + Cursor.Index, insert);
   }
   Cursor.nextCode();
   return std::get<T>(Codes[Code.UID]);
}

template <class T> T & extDocument::reserveCode(StreamChar &Cursor)
{
   auto key = glEscapeCodeID;
   Codes.emplace(key, T());
   auto &result = std::get<T>(Codes[key]);

   if (Cursor.Index IS INDEX(Stream.size())) {
      Stream.emplace_back(result.Code, result.UID);
   }
   else {
      const StreamCode insert(result.Code, result.UID);
      Stream.insert(Stream.begin() + Cursor.Index, insert);
   }
   Cursor.nextCode();
   return result;
}

//********************************************************************************************************************

enum {
   RT_OBJECT_TEMP=1,
   RT_OBJECT_UNLOAD,
   RT_OBJECT_UNLOAD_DELAY,
   RT_MEMORY,
   RT_PERSISTENT_SCRIPT, // Survives refreshes
   RT_PERSISTENT_OBJECT  // Survives refreshes
};

struct docresource {
   union {
      APTR Address;
      OBJECTID ObjectID;
   };
   CLASSID ClassID;
   BYTE Type;
   bool Terminate = false; // If true, can be freed immediately and not on a delay

   docresource(OBJECTID pID, BYTE pType) : ObjectID(pID), Type(pType) { }

   ~docresource() {
      if (Type IS RT_MEMORY) {
         FreeResource(Address);
      }
      else if ((Type IS RT_PERSISTENT_SCRIPT) or (Type IS RT_PERSISTENT_OBJECT)) {
         if (Terminate) FreeResource(ObjectID);
         else SendMessage(MSGID_FREE, MSF::NIL, &ObjectID, sizeof(OBJECTID));
      }
      else if (Type IS RT_OBJECT_UNLOAD_DELAY) {
         if (Terminate) FreeResource(ObjectID);
         else SendMessage(MSGID_FREE, MSF::NIL, &ObjectID, sizeof(OBJECTID));
      }
      else FreeResource(ObjectID);
   }
};

class tagroutine {
public:
   void (*Routine)(extDocument *, objXML *, XMLTag &, objXML::TAGS &, StreamChar &, IPF);
   TAG Flags;
};

struct process_table {
   struct escTable *escTable;
   LONG RowCol;
};

struct EditCell {
   LONG CellID;
   LONG X, Y, Width, Height;
};

static const std::string & escCode(ESC Code) {
   static const std::string strCodes[] = {
      "?", "Text", "Font", "FontCol", "Uline", "Bkgd", "Inv", "Vector", "Link", "TabDef", "PE",
      "P", "EndLink", "Advance", "List", "ListEnd", "Table", "TableEnd", "Row", "Cell",
      "CellEnd", "RowEnd", "SetMargins", "Index", "IndexEnd", "XML", "Image"
   };

   if (LONG(Code) < ARRAYSIZE(strCodes)) return strCodes[LONG(Code)];
   return strCodes[0];
}

//********************************************************************************************************************

struct CaseInsensitiveMap {
   bool operator() (const std::string &lhs, const std::string &rhs) const {
      return ::strcasecmp(lhs.c_str(), rhs.c_str()) < 0;
   }
};

//********************************************************************************************************************
// Function prototypes.

#include "module_def.c"

struct layout; // Pre-def

static ERROR activate_cell_edit(extDocument *, LONG, StreamChar);
static ERROR add_document_class(void);
static LONG  add_tabfocus(extDocument *, UBYTE, LONG);
static void  add_template(extDocument *, objXML *, XMLTag &);
static void  advance_tabfocus(extDocument *, BYTE);
static void  check_mouse_click(extDocument *, DOUBLE X, DOUBLE Y);
static void  check_mouse_pos(extDocument *, DOUBLE, DOUBLE);
static void  check_mouse_release(extDocument *, DOUBLE X, DOUBLE Y);
static ERROR consume_input_events(objVector *, const InputEvent *);
static void  translate_args(extDocument *, const std::string &, std::string &);
static void  translate_attrib_args(extDocument *, pf::vector<XMLAttrib> &);
static LONG  create_font(const std::string &, const std::string &, LONG);
static void  deactivate_edit(extDocument *, bool);
static void  deselect_text(extDocument *);
static ERROR extract_script(extDocument *, const std::string &, OBJECTPTR *, std::string &, std::string &);
static void  error_dialog(const std::string, const std::string);
static void  error_dialog(const std::string, ERROR);
static ERROR tag_xml_content_eval(extDocument *, std::string &);
static SEGINDEX find_segment(extDocument *, StreamChar, bool);
static LONG  find_tabfocus(extDocument *, UBYTE, LONG);
static ERROR flash_cursor(extDocument *, LARGE, LARGE);
static std::string get_font_style(FSO);
//static LONG   get_line_from_index(extDocument *, INDEX Index);
static LONG  getutf8(CSTRING, LONG *);
static ERROR insert_text(extDocument *, StreamChar &, const std::string &, bool);
static ERROR insert_xml(extDocument *, objXML *, objXML::TAGS &, LONG, UBYTE);
static ERROR insert_xml(extDocument *, objXML *, XMLTag &, StreamChar TargetIndex = StreamChar(), UBYTE Flags = 0);
static ERROR key_event(objVectorViewport *, KQ, KEY, LONG);
static void  layout_doc(extDocument *);
static ERROR load_doc(extDocument *, std::string, bool, ULD);
static void  notify_disable_viewport(OBJECTPTR, ACTIONID, ERROR, APTR);
static void  notify_enable_viewport(OBJECTPTR, ACTIONID, ERROR, APTR);
static void  notify_focus_viewport(OBJECTPTR, ACTIONID, ERROR, APTR);
static void  notify_free_event(OBJECTPTR, ACTIONID, ERROR, APTR);
static void  notify_lostfocus_viewport(OBJECTPTR, ACTIONID, ERROR, APTR);
static void  notify_redimension_viewport(objVectorViewport *, objVector *, DOUBLE X, DOUBLE Y, DOUBLE Width, DOUBLE Height);
static TRF   parse_tag(extDocument *, objXML *, XMLTag &, StreamChar &, IPF &);
static TRF   parse_tags(extDocument *, objXML *, objXML::TAGS &, StreamChar &, IPF = IPF::NIL);
static void  print_xmltree(objXML::TAGS &, LONG &);
static ERROR process_page(extDocument *, objXML *);
static void  process_parameters(extDocument *, const std::string &);
static bool  read_rgb8(CSTRING, RGB8 *);
static void  redraw(extDocument *, bool);
static ERROR report_event(extDocument *, DEF, APTR, CSTRING);
static void  reset_cursor(extDocument *);
static ERROR resolve_fontx_by_index(extDocument *, StreamChar, LONG *);
static ERROR resolve_font_pos(extDocument *, DocSegment &, LONG X, LONG *, StreamChar &);
static LONG  safe_file_path(extDocument *, const std::string &);
static void  set_focus(extDocument *, LONG, CSTRING);
static void  show_bookmark(extDocument *, const std::string &);
static STRING stream_to_string(extDocument *, StreamChar, StreamChar);
static void  style_check(extDocument *, StreamChar &);
static void  tag_xml_content(extDocument *, objXML *, XMLTag &, WORD);
static ERROR unload_doc(extDocument *, ULD);
static bool  valid_object(extDocument *, OBJECTPTR);
static bool  valid_objectid(extDocument *, OBJECTID);
static BYTE  view_area(extDocument *, LONG Left, LONG Top, LONG Right, LONG Bottom);

inline void print_xmltree(objXML::TAGS &Tags) {
   LONG indent = 0;
   print_xmltree(Tags, indent);
}

inline bool read_rgb8(const std::string Value, RGB8 *RGB) {
   return read_rgb8(Value.c_str(), RGB);
}

#ifdef DBG_STREAM
static void print_stream(extDocument *, const RSTREAM &);
static void print_stream(extDocument *Self) { print_stream(Self, Self->Stream); }
#endif

//********************************************************************************************************************
// Basic font caching on an index basis.

struct FontEntry {
   objFont *Font;
   LONG Point;

   FontEntry(objFont *pFont, LONG pPoint) : Font(pFont), Point(pPoint) { }

   ~FontEntry() {
      if (Font) {
         pf::Log log(__FUNCTION__);
         log.msg("Removing cached font %s:%.2f.", Font->Face, Font->Point);
         FreeResource(Font); 
         Font = NULL; 
      }
   }
    
   FontEntry(FontEntry &&other) noexcept { // Move constructor
      Font  = other.Font;
      Point = other.Point;
      other.Font = NULL;
   }

   FontEntry(const FontEntry &other) { // Copy constructor
      Font  = other.Font;
      Point = other.Point;
   }

   FontEntry& operator=(FontEntry &&other) noexcept { // Move assignment
      if (this == &other) return *this;
      Font  = other.Font;
      Point = other.Point;
      other.Font = NULL;
      return *this;
   }

   FontEntry& operator=(const FontEntry& other) { // Copy assignment
      if (this == &other) return *this;
      Font  = other.Font;
      Point = other.Point;
      return *this;
   }
};

static std::vector<FontEntry> glFonts;

objFont * escFont::getFont() 
{
   if ((FontIndex < INDEX(glFonts.size())) and (FontIndex >= 0)) return glFonts[FontIndex].Font;
   else {
      pf::Log log(__FUNCTION__);
      log.warning("Bad font index %d.  Max: %d", FontIndex, LONG(glFonts.size()));
      if (!glFonts.empty()) return glFonts[0].Font; // Always try to return a font rather than NULL
      else return NULL;
   }
}

//********************************************************************************************************************
// For a given index in the stream, return the element code.  Index MUST be a valid reference to an escape sequence.

template <class T> T & escape_data(extDocument *Self, StreamChar Index) {
   auto &sv = Self->Codes[Self->Stream[Index.Index].UID];
   return std::get<T>(sv);
}

template <class T> T & escape_data(extDocument *Self, INDEX Index) {
   auto &sv = Self->Codes[Self->Stream[Index].UID];
   return std::get<T>(sv);
}

template <class T> inline void remove_cursor(T a) { draw_cursor(a, false); }

template <class T> inline const std::string & ESCAPE_NAME(RSTREAM &Stream, T Index) {
   return escCode(Stream[Index].Code);
}

//********************************************************************************************************************
// Convenience class for entering and leaving a template region.  This is achieved by setting InjectXML and InjectTag
// with references to the content that will be injected to the template.  Injection typically occurs when the client
// uses the <inject/> tag.

class initTemplate {
   extDocument  *Self;
   objXML::TAGS *Tags;
   objXML       *XML;

   public:
   initTemplate(extDocument *pSelf, objXML::TAGS &pTag, objXML *pXML) {
      Self = pSelf;
      Tags = Self->InjectTag;
      XML  = Self->InjectXML;
      Self->InjectTag = &pTag;
      Self->InjectXML = pXML;
      Self->InTemplate++;
   }

   ~initTemplate() {
      Self->InTemplate--;
      Self->InjectTag = Tags;
      Self->InjectXML = XML;
   }
};

//********************************************************************************************************************

ERROR CMDInit(OBJECTPTR argModule, struct CoreBase *argCoreBase)
{
   CoreBase = argCoreBase;

   argModule->getPtr(FID_Root, &modDocument);

   if (objModule::load("display", &modDisplay, &DisplayBase) != ERR_Okay) return ERR_InitModule;
   if (objModule::load("font", &modFont, &FontBase) != ERR_Okay) return ERR_InitModule;
   if (objModule::load("vector", &modVector, &VectorBase) != ERR_Okay) return ERR_InitModule;

   OBJECTID style_id;
   if (!FindObject("glStyle", ID_XML, FOF::NIL, &style_id)) {
      char buffer[32];
      if (!acGetVar(GetObjectPtr(style_id), "/colours/@DocumentHighlight", buffer, sizeof(buffer))) {
         glHighlight.assign(buffer);
      }
   }

   return add_document_class();
}

ERROR CMDExpunge(void)
{
   glFonts.clear();

   if (modVector)  { FreeResource(modVector);  modVector  = NULL; }
   if (modDisplay) { FreeResource(modDisplay); modDisplay = NULL; }
   if (modFont)    { FreeResource(modFont);    modFont    = NULL; }

   if (clDocument) { FreeResource(clDocument); clDocument = NULL; }
   return ERR_Okay;
}

static ERROR CMDOpen(OBJECTPTR Module)
{
   return ERR_Okay;
}

//********************************************************************************************************************

inline INDEX find_cell(extDocument *Self, LONG ID)
{
   for (INDEX i=0; i < INDEX(Self->Stream.size()); i++) {
      if (Self->Stream[i].Code IS ESC::CELL) {
         auto &cell = std::get<escCell>(Self->Codes[Self->Stream[i].UID]);
         if ((ID) and (ID IS cell.CellID)) return i;
      }
   }

   return -1;
}

inline INDEX find_editable_cell(extDocument *Self, const std::string &EditDef)
{
   for (INDEX i=0; i < INDEX(Self->Stream.size()); i++) {
      if (Self->Stream[i].Code IS ESC::CELL) {
         auto &cell = escape_data<escCell>(Self, i);
         if (EditDef == cell.EditDef) return i;
      }
   }

   return -1;
}

//********************************************************************************************************************

inline DocEdit * find_editdef(extDocument *Self, const std::string Name)
{
   auto it = Self->EditDefs.find(Name);
   if (it != Self->EditDefs.end()) return &it->second;
   else return NULL;
}

//********************************************************************************************************************

inline void layout_doc_fast(extDocument *Self)
{
   pf::LogLevel level(2);
   layout_doc(Self);
}

//********************************************************************************************************************

void StreamChar::eraseChar(extDocument *Self, RSTREAM &Stream) // Erase a character OR an escape code.
{
   if (Stream[Index].Code IS ESC::TEXT) {
      auto &text = escape_data<escText>(Self, Index);
      if (Offset < text.Text.size()) text.Text.erase(Offset, 1);
      if (Offset > text.Text.size()) Offset = text.Text.size();
   }
   else Stream.erase(Stream.begin() + Index);
}

//********************************************************************************************************************
// Retrieve the first available character.  Assumes that the position is valid.

char StreamChar::getChar(extDocument *Self, const RSTREAM &Stream) 
{
   auto idx = Index;
   auto seek = Offset;
   while (size_t(idx) < Stream.size()) {
      if (Stream[idx].Code IS ESC::TEXT) {
         auto &text = escape_data<escText>(Self, idx);
         if (seek < text.Text.size()) return text.Text[seek];
         else seek = 0; // The current character offset isn't valid, reset it.
      }
      idx++;
   }
   return 0;
}

//********************************************************************************************************************
// Retrieve the first character after seeking past N viable characters (forward only).

char StreamChar::getChar(extDocument *Self, const RSTREAM &Stream, INDEX Seek) 
{     
   auto idx = Index;
   auto off = Offset;
   
   while (unsigned(idx) < Stream.size()) {
      if (Stream[idx].Code IS ESC::TEXT) {
         auto &text = escape_data<escText>(Self, idx);
         if (off + Seek < text.Text.size()) return text.Text[off + Seek];   
         Seek -= text.Text.size() - off;
         off = 0;
      }
      idx++;
   }
   
   return 0;
}

void StreamChar::nextChar(extDocument *Self, const RSTREAM &Stream)
{
   if (Stream[Index].Code IS ESC::TEXT) {
      auto &text = escape_data<escText>(Self, Index);
      if (++Offset >= text.Text.size()) {
         Index++;
         Offset = 0;
      }
   }
   else if (Index < INDEX(Stream.size())) Index++;
}

//********************************************************************************************************************
// Move the cursor to the previous character OR code.

void StreamChar::prevChar(extDocument *Self, const RSTREAM &Stream)
{
   if (Stream[Index].Code IS ESC::TEXT) {
      if (Offset > 0) { Offset--; return; }
   }
   
   if (Index > 0) {
      Index--;
      if (Stream[Index].Code IS ESC::TEXT) {
         Offset = escape_data<escText>(Self, Index).Text.size()-1;
      }
      else Offset =0;
   }
   else Offset = 0;
}

//********************************************************************************************************************
// Return the previous printable character.  Codes do not count.

char StreamChar::getPrevChar(extDocument *Self, const RSTREAM &Stream)
{
   if ((Offset > 0) and (Stream[Index].Code IS ESC::TEXT)) {
      return escape_data<escText>(Self, Index).Text[Offset-1];
   }
   
   for (auto i=Index; i > 0; i--) {
      if (Stream[i].Code IS ESC::TEXT) {
         return escape_data<escText>(Self, i).Text.back();
      }   
   }

   return 0;
}
//********************************************************************************************************************

#include "parsing.cpp"
#include "class/fields.cpp"
#include "class/document_class.cpp"
#include "functions.cpp"
#include "ui.cpp"
#include "layout.cpp"
#include "draw.cpp"

//********************************************************************************************************************

static ERROR add_document_class(void)
{
   clDocument = objMetaClass::create::global(
      fl::BaseClassID(ID_DOCUMENT),
      fl::ClassVersion(VER_DOCUMENT),
      fl::Name("Document"),
      fl::Category(CCF::GUI),
      fl::Flags(CLF::PROMOTE_INTEGRAL),
      fl::Actions(clDocumentActions),
      fl::Methods(clDocumentMethods),
      fl::Fields(clFields),
      fl::Size(sizeof(extDocument)),
      fl::Path(MOD_PATH),
      fl::FileExtension("*.rpl|*.ripple|*.ripl"));

   return clDocument ? ERR_Okay : ERR_AddClass;
}

//********************************************************************************************************************

PARASOL_MOD(CMDInit, NULL, CMDOpen, CMDExpunge, MOD_IDL, NULL)
extern "C" struct ModHeader * register_document_module() { return &ModHeader; }
