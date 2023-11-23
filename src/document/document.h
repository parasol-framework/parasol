
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

struct tab {
   // The ref is a UID for the Type, so you can use it to find the tab in the document stream
   LONG  ref;        // For TT_OBJECT: ObjectID; TT_LINK: LinkID
   LONG  xref;       // For TT_OBJECT: SurfaceID (if found)
   UBYTE type;       // TT_LINK, TT_OBJECT
   bool  active;     // true if the tabbable entity is active/visible
};

//********************************************************************************************************************

struct edit_cell {
   LONG cell_id;
   DOUBLE x, y, width, height;
};

//********************************************************************************************************************

struct link_activated {
   std::map<std::string, std::string> Values;  // All key-values associated with the link.
};

//********************************************************************************************************************
// Every instruction in the document stream is represented by a stream_code entity.  The code refers to what the thing
// is, while the UID hash refers to further information in the Codes table.

struct stream_code {
   ESC code;  // Type
   ULONG uid; // Lookup for the Codes table

   stream_code() : code(ESC::NIL), uid(0) { }
   stream_code(ESC pCode, ULONG pID) : code(pCode), uid(pID) { }
};

using RSTREAM = std::vector<stream_code>;

//********************************************************************************************************************

class base_code {
public:
   ULONG uid;   // Unique identifier for lookup
   ESC code = ESC::NIL; // Byte code

   base_code() { uid = glByteCodeID++; }
   base_code(ESC pCode) : code(pCode) { uid = glByteCodeID++; }
};

//********************************************************************************************************************
// stream_char provides indexing to specific characters in the stream.  It is designed to handle positional changes so
// that text string boundaries can be crossed without incident.
//
// The index and Offset are set to -1 if the stream_char is invalidated.

struct stream_char {
   INDEX Index;     // A TEXT code position within the stream
   size_t Offset;   // Specific character offset within the bc_text.text string

   stream_char() : Index(-1), Offset(-1) { }
   stream_char(INDEX pIndex, ULONG pOffset) : Index(pIndex), Offset(pOffset) { }
   stream_char(INDEX pIndex) : Index(pIndex), Offset(0) { }

   bool operator==(const stream_char &Other) const {
      return (this->Index == Other.Index) and (this->Offset == Other.Offset);
   }

   bool operator<(const stream_char &Other) const {
      if (this->Index < Other.Index) return true;
      else if ((this->Index IS Other.Index) and (this->Offset < Other.Offset)) return true;
      else return false;
   }

   bool operator>(const stream_char &Other) const {
      if (this->Index > Other.Index) return true;
      else if ((this->Index IS Other.Index) and (this->Offset > Other.Offset)) return true;
      else return false;
   }

   bool operator<=(const stream_char &Other) const {
      if (this->Index < Other.Index) return true;
      else if ((this->Index IS Other.Index) and (this->Offset <= Other.Offset)) return true;
      else return false;
   }

   bool operator>=(const stream_char &Other) const {
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

typedef void TAGROUTINE (extDocument *, objXML *, XMLTag &, objXML::TAGS &, stream_char &, IPF);

class tagroutine {
public:
   TAGROUTINE *routine;
   TAG flags;
};

struct process_table {
   struct bc_table *table;
   LONG row_col;
};

//********************************************************************************************************************

struct case_insensitive_map {
   bool operator() (const std::string &lhs, const std::string &rhs) const {
      return ::strcasecmp(lhs.c_str(), rhs.c_str()) < 0;
   }
};

//********************************************************************************************************************
// Basic font caching on an index basis.

struct font_entry {
   objFont *font;
   DOUBLE point;

   font_entry(objFont *pFont, DOUBLE pPoint) : font(pFont), point(pPoint) { }

   ~font_entry() {
      if (font) {
         pf::Log log(__FUNCTION__);
         log.msg("Removing cached font %s:%.2f.", font->Face, font->Point);
         FreeResource(font);
         font = NULL;
      }
   }

   font_entry(font_entry &&other) noexcept { // Move constructor
      font  = other.font;
      point = other.point;
      other.font = NULL;
   }

   font_entry(const font_entry &other) { // Copy constructor
      font  = other.font;
      point = other.point;
   }

   font_entry& operator=(font_entry &&other) noexcept { // Move assignment
      if (this == &other) return *this;
      font  = other.font;
      point = other.point;
      other.font = NULL;
      return *this;
   }

   font_entry& operator=(const font_entry& other) { // Copy assignment
      if (this == &other) return *this;
      font  = other.font;
      point = other.point;
      return *this;
   }
};

//********************************************************************************************************************

struct bc_font : public base_code {
   WORD FontIndex;      // font lookup
   FSO  Options;
   std::string Fill;    // font fill
   ALIGN VAlign;        // Vertical alignment of text within the available line height

   bc_font(): FontIndex(-1), Options(FSO::NIL), Fill("rgb(0,0,0)"), VAlign(ALIGN::BOTTOM) { code = ESC::FONT; }

   objFont * getFont();
};

struct style_status {
   struct bc_font font_style;
   struct process_table *table;
   struct bc_list *list;
   std::string face;
   DOUBLE point;
   bool face_change;      // A major font change has occurred (e.g. face, point size)
   bool style_change;     // A minor style change has occurred (e.g. font colour)

   style_status() : table(NULL), list(NULL), face(""), point(0), face_change(false), style_change(false) { }
};

//********************************************************************************************************************
// Refer to layout::add_drawsegment().  A segment represents graphical content, which can be in the form of text,
// graphics or both.  A segment can consist of one line only - so if the layout process encounters a boundary causing
// wordwrap then a new segment must be created.

struct doc_segment {
   stream_char start;       // Starting index (including character if text)
   stream_char stop;        // stop at this index/character
   stream_char trim_stop;   // The stopping point when whitespace is removed
   FloatRect area;          // Dimensions of the segment.
   DOUBLE gutter;           // The largest gutter value after taking into account all fonts used on the line.
   DOUBLE align_width;      // Full width of this segment if it were non-breaking
   UWORD depth;             // Branch depth associated with the segment - helps to differentiate between inner and outer tables
   bool  edit;              // true if this segment represents content that can be edited
   bool  text_content;      // true if there is TEXT in this segment
   bool  floating_vectors;  // true if there are user defined vectors in this segment with independent x,y coordinates
   bool  allow_merge;       // true if this segment can be merged with siblings that have allow_merge set to true
};

struct doc_clip {
   DOUBLE left, top, right, bottom;
   INDEX index; // The stream index of the object/table/item that is creating the clip.
   bool transparent; // If true, wrapping will not be performed around the clip region.
   std::string name;

   doc_clip() = default;

   doc_clip(DOUBLE pLeft, DOUBLE pTop, DOUBLE pRight, DOUBLE pBottom, LONG pIndex, bool pTransparent, const std::string &pName) :
      left(pLeft), top(pTop), right(pRight), bottom(pBottom), index(pIndex), transparent(pTransparent), name(pName) { }
};

struct doc_edit {
   LONG max_chars;
   std::string name;
   std::string on_enter, on_exit, on_change;
   std::vector<std::pair<std::string, std::string>> args;
   bool line_breaks;

   doc_edit() : max_chars(-1), args(0), line_breaks(false) { }
};

struct bc_link;
struct bc_cell;

// doc_link describes an area on the page that can be interacted with as a link or table cell.
// The link will be associated with a segment and an originating stream code.
//
// TODO: We'll need to swap the x/y/width/height variables for a vector path that represents the link
// area.  This will allow us to support transforms correctly, as well as links that need to accurately
// map to vector shapes.

struct doc_link {
   std::variant<bc_link *, bc_cell *> ref;
   DOUBLE x, y, width, height;
   SEGINDEX segment;
   ESC  base_code;

   doc_link(ESC pCode, std::variant<bc_link *, bc_cell *> pRef, SEGINDEX pSegment, LONG pX, LONG pY, LONG pWidth, LONG pHeight) :
       ref(pRef), x(pX), y(pY), width(pWidth), height(pHeight), segment(pSegment), base_code(pCode) { }

   doc_link() : x(0), y(0), width(0), height(0), segment(0), base_code(ESC::NIL) { }

   bc_link * as_link() { return std::get<bc_link *>(ref); }
   bc_cell * as_cell() { return std::get<bc_cell *>(ref); }
   void exec(extDocument *);
};

struct mouse_over {
   std::string function; // name of function to call.
   DOUBLE top, left, bottom, right;
   LONG element_id;
};

class sorted_segment { // Efficient lookup to the doc_segment array, sorted by vertical position
public:
   SEGINDEX segment;
   DOUBLE y;
};

struct tablecol {
   DOUBLE preset_width = 0;
   DOUBLE min_width = 0;   // For assisting layout
   DOUBLE width = 0;
   bool preset_width_rel = false;
};

//********************************************************************************************************************

struct bc_text : public base_code {
   std::string text;
   bool Formatted = false;
   SEGINDEX Segment = -1; // Reference to the first segment that manages this text string.

   bc_text() { code = ESC::TEXT; }
   bc_text(std::string pText) : text(pText) { code = ESC::TEXT; }
   bc_text(std::string pText, bool pFormatted) : text(pText), Formatted(pFormatted) { code = ESC::TEXT; }
};

struct bc_advance : public base_code {
   DOUBLE x, y;

   bc_advance() : x(0), y(0) { code = ESC::ADVANCE; }
};

struct bc_index : public base_code {
   ULONG NameHash;     // The name of the index is held here as a hash
   LONG  ID;           // UID for matching to the correct bc_index_end
   DOUBLE Y;           // The cursor's vertical position of when the index was encountered during layout
   bool Visible;       // true if the content inside the index is visible (this is the default)
   bool ParentVisible; // true if the nearest parent index(es) will allow index content to be visible.  true is the default.  This allows Hide/ShowIndex() to manage themselves correctly

   bc_index(ULONG pName, LONG pID, LONG pY, bool pVisible, bool pParentVisible) :
      NameHash(pName), ID(pID), Y(pY), Visible(pVisible), ParentVisible(pParentVisible) {
      code = ESC::INDEX_START;
   }
};

struct bc_index_end : public base_code {
   LONG ID; // UID matching to the correct bc_index
   bc_index_end(LONG pID) : ID(pID) { code = ESC::INDEX_END; }
};

struct bc_link : public base_code {
   LINK  Type;    // Link type (either a function or hyperlink)
   UWORD ID;
   FSO   Align;
   std::string PointerMotion; // function to call for pointer motion events
   std::string Ref; // function name or a path, depending on Type
   std::vector<std::pair<std::string,std::string>> Args;

   bc_link() : Type(LINK::NIL), ID(0), Align(FSO::NIL) { code = ESC::LINK; }
};

struct bc_link_end : public base_code {
   bc_link_end() { code = ESC::LINK_END; }
};

struct bc_image : public base_code {
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

   bc_image() { code = ESC::IMAGE; }

   inline bool floating() {
      return (align & (ALIGN::LEFT|ALIGN::RIGHT|ALIGN::HORIZONTAL)) != ALIGN::NIL;
   }

   constexpr DOUBLE full_width() { return final_width + final_pad.left + final_pad.right; }
   constexpr DOUBLE full_height() { return final_height + final_pad.top + final_pad.bottom; }
};

struct bc_list : public base_code {
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
   DOUBLE vspacing   = 0.5;          // Spacing between list items, expressed as a ratio
   UBYTE Type        = BULLET;
   bool  Repass      = false;

   bc_list() { code = ESC::LIST_START; }
};

struct bc_list_end : public base_code {
   bc_list_end() { code = ESC::LIST_END; }
};

struct bc_set_margins : public base_code {
   WORD Left = 0x7fff; WORD Top = 0x7fff; WORD Bottom = 0x7fff; WORD Right = 0x7fff;
   bc_set_margins() { code = ESC::SET_MARGINS; }
};

struct bc_vector : public base_code {
   OBJECTID ObjectID = 0;     // Reference to the vector
   CLASSID ClassID = 0;       // Precise class that the object belongs to, mostly for informative/debugging purposes
   ClipRectangle Margins;
   bool Inline       = false; // true if object is embedded as part of the text stream (treated as if it were a character)
   bool Owned        = false; // true if the object is owned by a parent (not subject to normal document layout)
   bool IgnoreCursor = false; // true if the client has set fixed values for both x and y
   bool BlockRight   = false; // true if no text may be printed to the right of the object
   bool BlockLeft    = false; // true if no text may be printed to the left of the object
   bc_vector() { code = ESC::VECTOR; }
};

struct bc_xml : public base_code {
   OBJECTID ObjectID = 0;   // Reference to the object
   bool Owned = false;      // true if the object is owned by a parent (not subject to normal document layout)
   bc_xml() { code = ESC::XML; }
};

struct bc_table : public base_code {
   struct bc_table *Stack = NULL;
   std::vector<tablecol> Columns; // Table column management
   std::string Fill, Stroke;
   DOUBLE CellVSpacing = 0, CellHSpacing = 0; // Spacing between each cell
   DOUBLE CellPadding = 0;               // Spacing inside each cell (margins)
   DOUBLE RowWidth = 0;                  // Assists in the computation of row width
   DOUBLE X = 0, Y = 0;                  // Calculated x/y coordinate of the table
   DOUBLE Width = 0, Height = 0;         // Calculated table width/height
   DOUBLE MinWidth = 0, MinHeight = 0;   // User-determined minimum table width/height
   LONG   Rows = 0;                      // Total number of rows in table
   LONG   RowIndex = 0;                  // Current row being processed, generally for debugging
   DOUBLE CursorX = 0, CursorY = 0;      // Cursor coordinates
   LONG   TotalClips = 0;                // Temporary record of Document->Clips.size()
   UWORD  Thickness  = 0;                // Border thickness
   UBYTE  ComputeColumns = 0;
   bool   WidthPercent   = false;   // true if width is a percentage
   bool   HeightPercent  = false;   // true if height is a percentage
   bool   CellsExpanded  = false;   // false if the table cells have not been expanded to match the inside table width
   bool   ResetRowHeight = false;   // true if the height of all rows needs to be reset in the current pass
   bool   Wrap = false;
   bool   Thin = false;
   // Entry followed by the minimum width of each column
   bc_table() { code = ESC::TABLE_START; }

   void computeColumns() { // Compute the default column widths
      if (!ComputeColumns) return;

      ComputeColumns = 0;
      CellsExpanded = false;

      if (!Columns.empty()) {
         for (unsigned j=0; j < Columns.size(); j++) {
            //if (ComputeColumns IS 1) {
            //   Columns[j].width = 0;
            //   Columns[j].min_width = 0;
            //}

            if (Columns[j].preset_width_rel) { // Percentage width value
               Columns[j].width = Columns[j].preset_width * Width;
            }
            else if (Columns[j].preset_width) { // Fixed width value
               Columns[j].width = Columns[j].preset_width;
            }
            else Columns[j].width = 0;

            if (Columns[j].min_width > Columns[j].width) Columns[j].width = Columns[j].min_width;
         }
      }
      else Columns.clear();
   }
};

struct bc_table_end : public base_code {
   bc_table_end() { code = ESC::TABLE_END; }
};

class bc_paragraph : public base_code {
   public:
   std::string value = "";
   DOUBLE x, y, height;
   DOUBLE block_indent;
   DOUBLE item_indent;
   DOUBLE indent;
   DOUBLE vspacing;      // Trailing whitespace, expressed as a ratio of the default line height
   DOUBLE leading_ratio; // Leading whitespace (minimum amount of space from the end of the last paragraph).  Expressed as a ratio of the default line height
   // Options
   bool relative;
   bool list_item;
   bool trim;
   bool aggregate;

   bc_paragraph() : base_code(ESC::PARAGRAPH_START), x(0), y(0), height(0),
      block_indent(0), item_indent(0), indent(0), vspacing(1.0), leading_ratio(1.0),
      relative(false), list_item(false), trim(false), aggregate(false) { }

   void applyStyle(const style_status &Style) {
      vspacing     = Style.list->vspacing;
      block_indent = Style.list->BlockIndent;
      item_indent  = Style.list->ItemIndent;
   }
};

struct bc_paragraph_end : public base_code {
   bc_paragraph_end() : base_code(ESC::PARAGRAPH_END) { }
};

struct bc_row : public base_code {
   DOUBLE Y = 0;
   DOUBLE RowHeight = 0; // height of all cells on this row, used when drawing the cells
   DOUBLE MinHeight = 0;
   std::string Stroke, Fill;
   bool  VerticalRepass = false;

   bc_row() : base_code(ESC::ROW) { }
};

struct bc_row_end : public base_code {
   bc_row_end() : base_code(ESC::ROW_END) { }
};

struct bc_cell : public base_code {
   LONG CellID;          // Identifier for the matching bc_cell_end
   LONG Column;          // Column number that the cell starts in
   LONG ColSpan;         // Number of columns spanned by this cell (normally set to 1)
   LONG RowSpan;         // Number of rows spanned by this cell
   DOUBLE AbsX, AbsY;    // Cell coordinates, these are absolute
   DOUBLE Width, Height; // width and height of the cell
   std::string OnClick;  // name of an onclick function
   std::string EditDef;  // The edit definition that this cell is linked to (if any)
   std::vector<std::pair<std::string, std::string>> Args;
   std::string Stroke;
   std::string Fill;

   bc_cell(LONG pCellID, LONG pColumn) :
      base_code(ESC::CELL), CellID(pCellID), Column(pColumn),
      ColSpan(1), RowSpan(1), AbsX(0), AbsY(0), Width(0), Height(0)
      { }
};

struct bc_cell_end : public base_code {
   LONG CellID = 0;    // Matching identifier from bc_cell
   bc_cell_end() : base_code(ESC::CELL_END) { }
};
