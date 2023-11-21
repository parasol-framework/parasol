
class extDocument;

typedef int INDEX;
using SEGINDEX = int;

enum class TE : char {
   NIL = 0,
   WRAP_TABLE,
   REPASS_ROW_HEIGHT,
   EXTEND_PAGE
};

enum class RTD : UBYTE {
   NIL=0,
   OBJECT_TEMP,         // The object can be removed after parsing has finished
   OBJECT_UNLOAD,       // Default choice for object termination, terminates immediately
   OBJECT_UNLOAD_DELAY, // Use SendMessage() to terminate the object
   PERSISTENT_SCRIPT,   // The script can survive refreshes
   PERSISTENT_OBJECT    // The object can survive refreshes
};

DEFINE_ENUM_FLAG_OPERATORS(RTD)

enum class IXF : UBYTE {
   NIL        = 0x00,
   SIBLINGS   = 0x01,
   HOLDSTYLE  = 0x02,
   RESETSTYLE = 0x04,
   CLOSESTYLE = 0x08
};

DEFINE_ENUM_FLAG_OPERATORS(IXF)

enum class PXF : WORD {
   NIL       = 0,
   ARGS      = 0x0001,
   TRANSLATE = 0x0002
};

DEFINE_ENUM_FLAG_OPERATORS(PXF)

enum class LINK : UBYTE {
   NIL = 0,
   HREF,
   FUNCTION
};

DEFINE_ENUM_FLAG_OPERATORS(LINK)

enum {
   COND_NOT_EQUAL=1,
   COND_EQUAL,
   COND_LESS_THAN,
   COND_LESS_EQUAL,
   COND_GREATER_THAN,
   COND_GREATER_EQUAL
};

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
   // End of list - NB: PLEASE UPDATE byteCode() IF YOU ADD NEW CODES
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
// Tab is used to represent interactive entities within the document that can be tabbed to.

struct Tab {
   // The Ref is a UID for the Type, so you can use it to find the tab in the document stream
   LONG  Ref;        // For TT_OBJECT: ObjectID; TT_LINK: LinkID
   LONG  XRef;       // For TT_OBJECT: SurfaceID (if found)
   UBYTE Type;       // TT_LINK, TT_OBJECT
   bool  Active;     // true if the tabbable entity is active/visible
};

//********************************************************************************************************************

struct EditCell {
   LONG CellID;
   LONG X, Y, Width, Height;
};

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

class BaseCode {
public:
   ULONG UID;   // Unique identifier for lookup
   ESC Code = ESC::NIL; // Byte code

   BaseCode() { UID = glByteCodeID++; }
   BaseCode(ESC pCode) : Code(pCode) { UID = glByteCodeID++; }
};

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

   // NB: None of these support unicode.

   UBYTE getChar(extDocument *, const RSTREAM &);
   UBYTE getChar(extDocument *, const RSTREAM &, LONG Seek);
   UBYTE getPrevChar(extDocument *, const RSTREAM &);
   UBYTE getPrevCharOrInline(extDocument *, const RSTREAM &);
   void eraseChar(extDocument *, RSTREAM &); // Erase a character OR an escape code.
   void nextChar(extDocument *, const RSTREAM &);
   void prevChar(extDocument *, const RSTREAM &);
};

//********************************************************************************************************************

class docresource {
public:
   OBJECTID ObjectID;
   CLASSID ClassID;
   RTD Type;
   bool Terminate = false; // If true, can be freed immediately and not on a delay

   docresource(OBJECTID pID, RTD pType, CLASSID pClassID = 0) :
      ObjectID(pID), ClassID(pClassID), Type(pType) { }

   ~docresource() {
      if ((Type IS RTD::PERSISTENT_SCRIPT) or (Type IS RTD::PERSISTENT_OBJECT)) {
         if (Terminate) FreeResource(ObjectID);
         else SendMessage(MSGID_FREE, MSF::NIL, &ObjectID, sizeof(OBJECTID));
      }
      else if (Type IS RTD::OBJECT_UNLOAD_DELAY) {
         if (Terminate) FreeResource(ObjectID);
         else SendMessage(MSGID_FREE, MSF::NIL, &ObjectID, sizeof(OBJECTID));
      }
      else if (Type != RTD::NIL) FreeResource(ObjectID);
   }

   docresource(docresource &&other) noexcept { // Move constructor
      ObjectID  = other.ObjectID;
      ClassID   = other.ClassID;
      Type      = other.Type;
      Terminate = other.Terminate;
      other.Type = RTD::NIL;
   }

   docresource(const docresource &other) { // Copy constructor
      ObjectID  = other.ObjectID;
      ClassID   = other.ClassID;
      Type      = other.Type;
      Terminate = other.Terminate;
   }

   docresource& operator=(docresource &&other) noexcept { // Move assignment
      if (this == &other) return *this;
      ObjectID  = other.ObjectID;
      ClassID   = other.ClassID;
      Type      = other.Type;
      Terminate = other.Terminate;
      other.Type = RTD::NIL;
      return *this;
   }

   docresource& operator=(const docresource& other) { // Copy assignment
      if (this == &other) return *this;
      ObjectID  = other.ObjectID;
      ClassID   = other.ClassID;
      Type      = other.Type;
      Terminate = other.Terminate;
      return *this;
   }
};

//********************************************************************************************************************

typedef void TAGROUTINE (extDocument *, objXML *, XMLTag &, objXML::TAGS &, StreamChar &, IPF);

class tagroutine {
public:
   TAGROUTINE *Routine;
   TAG Flags;
};

struct process_table {
   struct bcTable *bcTable;
   LONG RowCol;
};

//********************************************************************************************************************

struct CaseInsensitiveMap {
   bool operator() (const std::string &lhs, const std::string &rhs) const {
      return ::strcasecmp(lhs.c_str(), rhs.c_str()) < 0;
   }
};

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

//********************************************************************************************************************

struct bcFont : public BaseCode {
   WORD FontIndex;      // Font lookup
   FSO  Options;
   std::string Fill;    // Font fill
   ALIGN VAlign;        // Vertical alignment of text within the available line height

   bcFont(): FontIndex(-1), Options(FSO::NIL), Fill("rgb(0,0,0)"), VAlign(ALIGN::BOTTOM) { Code = ESC::FONT; }

   objFont * getFont();
};

struct style_status {
   struct bcFont FontStyle;
   struct process_table * Table;
   struct bcList * List;
   std::string Face;
   DOUBLE Point;
   bool FaceChange;      // A major font change has occurred (e.g. face, point size)
   bool StyleChange;     // A minor style change has occurred (e.g. font colour)

   style_status() : Table(NULL), List(NULL), Face(""), Point(0), FaceChange(false), StyleChange(false) { }
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

struct bcLink;
struct bcCell;

// DocLink describes an area on the page that can be interacted with as a link or table cell.
// The link will be associated with a segment and an originating stream code.
//
// TODO: We'll need to swap the X/Y/Width/Height variables for a vector path that represents the link
// area.  This will allow us to support transforms correctly, as well as links that need to accurately
// map to vector shapes.

struct DocLink {
   std::variant<bcLink *, bcCell *> Ref;
   DOUBLE X, Y, Width, Height;
   SEGINDEX Segment;
   ESC  BaseCode;

   DocLink(ESC pCode, std::variant<bcLink *, bcCell *> pRef, SEGINDEX pSegment, LONG pX, LONG pY, LONG pWidth, LONG pHeight) :
       Ref(pRef), X(pX), Y(pY), Width(pWidth), Height(pHeight), Segment(pSegment), BaseCode(pCode) { }

   DocLink() : X(0), Y(0), Width(0), Height(0), Segment(0), BaseCode(ESC::NIL) { }

   bcLink * asLink() { return std::get<bcLink *>(Ref); }
   bcCell * asCell() { return std::get<bcCell *>(Ref); }
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

struct tablecol {
   UWORD PresetWidth;
   UWORD MinWidth;   // For assisting layout
   UWORD Width;
};

//********************************************************************************************************************

struct bcText : public BaseCode {
   std::string Text;
   bool Formatted = false;
   SEGINDEX Segment = -1; // Reference to the first segment that manages this text string.

   bcText() { Code = ESC::TEXT; }
   bcText(std::string pText) : Text(pText) { Code = ESC::TEXT; }
   bcText(std::string pText, bool pFormatted) : Text(pText), Formatted(pFormatted) { Code = ESC::TEXT; }
};

struct bcAdvance : public BaseCode {
   DOUBLE X, Y;

   bcAdvance() : X(0), Y(0) { Code = ESC::ADVANCE; }
};

struct bcIndex : public BaseCode {
   ULONG NameHash;     // The name of the index is held here as a hash
   LONG  ID;           // UID for matching to the correct bcIndexEnd
   LONG  Y;            // The cursor's vertical position of when the index was encountered during layout
   bool Visible;       // true if the content inside the index is visible (this is the default)
   bool ParentVisible; // true if the nearest parent index(es) will allow index content to be visible.  true is the default.  This allows Hide/ShowIndex() to manage themselves correctly

   bcIndex(ULONG pName, LONG pID, LONG pY, bool pVisible, bool pParentVisible) :
      NameHash(pName), ID(pID), Y(pY), Visible(pVisible), ParentVisible(pParentVisible) {
      Code = ESC::INDEX_START;
   }
};

struct bcIndexEnd : public BaseCode {
   LONG ID; // UID matching to the correct bcIndex
   bcIndexEnd(LONG pID) : ID(pID) { Code = ESC::INDEX_END; }
};

struct bcLink : public BaseCode {
   LINK Type;    // Link type (either a function or hyperlink)
   UWORD ID;
   FSO   Align;
   std::string PointerMotion; // Function to call for pointer motion events
   std::string Ref; // Function name or a path, depending on Type
   std::vector<std::pair<std::string,std::string>> Args;

   bcLink() : Type(LINK::NIL), ID(0), Align(FSO::NIL) {
      Code = ESC::LINK;
   }
};

struct bcLinkEnd : public BaseCode {
   bcLinkEnd() { Code = ESC::LINK_END; }
};

struct bcImage : public BaseCode {
   DOUBLE width = 0, height = 0;     // Client can define a fixed width/height, or leave at 0 for auto-sizing
   DOUBLE final_width, final_height; // Final dimensions computed during layout
   objVectorRectangle *rect = NULL;  // A vector will host the image and define a clipping mask for it
   DOUBLE x = 0;                     // For floating images only, horizontal position calculated during layout
   ALIGN align = ALIGN::NIL;         // NB: If horizontal alignment is defined then the image is treated as floating.
   bool width_pct = false, height_pct = false, padding = false;

   struct {
      DOUBLE left = 0, right = 0, top = 0, bottom = 0;
      bool left_pct = false, right_pct = false, top_pct = false, bottom_pct = false;
   } pad, final_pad;

   bcImage() { Code = ESC::IMAGE; }

   inline bool floating() {
      return (align & (ALIGN::LEFT|ALIGN::RIGHT|ALIGN::HORIZONTAL)) != ALIGN::NIL;
   }

   constexpr DOUBLE full_width() { return final_width + final_pad.left + final_pad.right; }
   constexpr DOUBLE full_height() { return final_height + final_pad.top + final_pad.bottom; }
};

struct bcList : public BaseCode {
   enum {
      ORDERED=0,
      BULLET,
      CUSTOM
   };

   std::string Fill;                 // Fill to use for bullet points (valid for BULLET only).
   std::vector<std::string> Buffer;  // Temp buffer, used for ordered lists
   LONG  Start       = 1;            // Starting value for ordered lists (default: 1)
   LONG  ItemIndent  = BULLET_WIDTH; // Minimum indentation for text printed for each item
   LONG  BlockIndent = BULLET_WIDTH; // Indentation for each set of items
   LONG  ItemNum     = 0;
   LONG  OrderInsert = 0;
   DOUBLE VSpacing   = 0.5;          // Spacing between list items, expressed as a ratio
   UBYTE Type        = BULLET;
   bool  Repass      = false;

   bcList() { Code = ESC::LIST_START; }
};

struct bcListEnd : public BaseCode {
   bcListEnd() { Code = ESC::LIST_END; }
};

struct bcSetMargins : public BaseCode {
   WORD Left = 0x7fff; WORD Top = 0x7fff; WORD Bottom = 0x7fff; WORD Right = 0x7fff;
   bcSetMargins() { Code = ESC::SET_MARGINS; }
};

struct bcVector : public BaseCode {
   OBJECTID ObjectID = 0;     // Reference to the vector
   CLASSID ClassID = 0;       // Precise class that the object belongs to, mostly for informative/debugging purposes
   ClipRectangle Margins;
   bool Inline       = false; // true if object is embedded as part of the text stream (treated as if it were a character)
   bool Owned        = false; // true if the object is owned by a parent (not subject to normal document layout)
   bool IgnoreCursor = false; // true if the client has set fixed values for both X and Y
   bool BlockRight   = false; // true if no text may be printed to the right of the object
   bool BlockLeft    = false; // true if no text may be printed to the left of the object
   bcVector() { Code = ESC::VECTOR; }
};

struct bcXML : public BaseCode {
   OBJECTID ObjectID = 0;   // Reference to the object
   bool Owned = false;      // true if the object is owned by a parent (not subject to normal document layout)
   bcXML() { Code = ESC::XML; }
};

struct bcTable : public BaseCode {
   struct bcTable *Stack = NULL;
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
   bcTable() { Code = ESC::TABLE_START; }

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

struct bcTableEnd : public BaseCode {
   bcTableEnd() { Code = ESC::TABLE_END; }
};

class bcParagraph : public BaseCode {
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

   bcParagraph() : BaseCode(ESC::PARAGRAPH_START), X(0), Y(0), Height(0),
      BlockIndent(0), ItemIndent(0), Indent(0), VSpacing(1.0), LeadingRatio(1.0),
      Relative(false), ListItem(false), Trim(false) { }

   void applyStyle(const style_status &Style) {
      VSpacing     = Style.List->VSpacing;
      BlockIndent  = Style.List->BlockIndent;
      ItemIndent   = Style.List->ItemIndent;
   }
};

struct bcParagraphEnd : public BaseCode {
   bcParagraphEnd() : BaseCode(ESC::PARAGRAPH_END) { }
};

struct bcRow : public BaseCode {
   LONG  Y = 0;
   LONG  RowHeight = 0; // Height of all cells on this row, used when drawing the cells
   LONG  MinHeight = 0;
   std::string Stroke, Fill;
   bool  VerticalRepass = false;

   bcRow() : BaseCode(ESC::ROW) { }
};

struct bcRowEnd : public BaseCode {
   bcRowEnd() : BaseCode(ESC::ROW_END) { }
};

struct bcCell : public BaseCode {
   LONG CellID;         // Identifier for the matching bcCellEnd
   LONG Column;         // Column number that the cell starts in
   LONG ColSpan;        // Number of columns spanned by this cell (normally set to 1)
   LONG RowSpan;        // Number of rows spanned by this cell
   LONG AbsX, AbsY;     // Cell coordinates, these are absolute
   DOUBLE Width, Height;  // Width and height of the cell
   std::string OnClick; // Name of an onclick function
   std::string EditDef; // The edit definition that this cell is linked to (if any)
   std::vector<std::pair<std::string, std::string>> Args;
   std::string Stroke;
   std::string Fill;

   bcCell(LONG pCellID, LONG pColumn) :
      BaseCode(ESC::CELL), CellID(pCellID), Column(pColumn),
      ColSpan(1), RowSpan(1), AbsX(0), AbsY(0), Width(0), Height(0)
      { }
};

struct bcCellEnd : public BaseCode {
   LONG CellID = 0;    // Matching identifier from bcCell
   bcCellEnd() : BaseCode(ESC::CELL_END) { }
};
