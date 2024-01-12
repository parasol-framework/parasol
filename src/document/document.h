
class extDocument;

typedef int INDEX;
using SEGINDEX = int;

enum class TE : char {
   NIL = 0,
   WRAP_TABLE,
   REPASS_ROW_HEIGHT,
   EXTEND_PAGE
};

enum class CB : UBYTE { // Cell border options
   NIL    = 0x00,
   TOP    = 0x01,
   BOTTOM = 0x02,
   LEFT   = 0x04,
   RIGHT  = 0x08,
   ALL    = 0X0f
};

DEFINE_ENUM_FLAG_OPERATORS(CB)

enum class CELL : UBYTE {
   NIL = 0,
   ABORT,
   WRAP_TABLE_CELL,
   REPASS_ROW_HEIGHT
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

enum class STYLE : UBYTE {
   NIL           = 0x00,
   INHERIT_STYLE = 0x01, // Inherit whatever font style applies at the insertion point.
   RESET_STYLE   = 0x02  // Current font style will be reset rather than defaulting to the most recent style at the insertion point.
};

DEFINE_ENUM_FLAG_OPERATORS(STYLE)

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

enum class SCODE : char {
   NIL = 0,
   TEXT,
   FONT,
   FONT_END,
   LINK,
   TABDEF,
   PARAGRAPH_END,
   PARAGRAPH_START,
   LINK_END,
   ADVANCE,
   LIST_START,
   LIST_END,
   TABLE_START,
   TABLE_END,
   ROW,
   CELL,
   ROW_END,
   INDEX_START,
   INDEX_END,
   XML,
   IMAGE,
   USE,
   BUTTON,
   CHECKBOX,
   COMBOBOX,
   INPUT,
   // End of list.  Functions affected by changing these codes are:
   //  BC_NAME()
   //  new_segment()
   END
};

DEFINE_ENUM_FLAG_OPERATORS(SCODE)

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
   NO_CONTENT   = 0x0001, // XML content data will be ignored
   FILTER_TABLE = 0x0008, // The tag is restricted to use within <table> sections.
   FILTER_ROW   = 0X0010, // The tag is restricted to use within <row> sections.
   FILTER_ALL   = (FILTER_TABLE|FILTER_ROW)
};

DEFINE_ENUM_FLAG_OPERATORS(IPF)

enum class TRF : ULONG {
   NIL      = 0,
   BREAK    = 0x00000001,
   CONTINUE = 0x00000002
};

DEFINE_ENUM_FLAG_OPERATORS(TRF)

class RSTREAM;

//********************************************************************************************************************
// Tab is used to represent interactive entities within the document that can be tabbed to.

struct tab {
   // The ref is a UID for the Type, so you can use it to find the tab in the document stream
   LONG  ref;        // For TT::VECTOR: VectorID; TT::LINK: LinkID
   TT    type;
   bool  active;     // true if the tabbable entity is active/visible

   tab(TT pType, LONG pReference, bool pActive) : ref(pReference), type(pType), active(pActive) { }
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
   SCODE code;  // Type
   ULONG uid; // Lookup for the Codes table

   stream_code() : code(SCODE::NIL), uid(0) { }
   stream_code(SCODE pCode, ULONG pID) : code(pCode), uid(pID) { }
};

//********************************************************************************************************************

class base_code {
public:
   ULONG uid;   // Unique identifier for lookup
   SCODE code = SCODE::NIL; // Byte code

   base_code() { uid = glByteCodeID++; }
   base_code(SCODE pCode) : code(pCode) { uid = glByteCodeID++; }
};

//********************************************************************************************************************

class docresource {
public:
   OBJECTID object_id;
   CLASSID class_id;
   RTD type;
   bool terminate = false; // If true, can be freed immediately and not on a delay

   docresource(OBJECTID pID, RTD pType, CLASSID pClassID = 0) :
      object_id(pID), class_id(pClassID), type(pType) { }

   ~docresource() {
      if ((type IS RTD::PERSISTENT_SCRIPT) or (type IS RTD::PERSISTENT_OBJECT)) {
         if (terminate) FreeResource(object_id);
         else SendMessage(MSGID_FREE, MSF::NIL, &object_id, sizeof(OBJECTID));
      }
      else if (type IS RTD::OBJECT_UNLOAD_DELAY) {
         if (terminate) FreeResource(object_id);
         else SendMessage(MSGID_FREE, MSF::NIL, &object_id, sizeof(OBJECTID));
      }
      else if (type != RTD::NIL) FreeResource(object_id);
   }

   docresource(docresource &&other) noexcept { // Move constructor
      object_id = other.object_id;
      class_id  = other.class_id;
      type      = other.type;
      terminate = other.terminate;
      other.type = RTD::NIL;
   }

   docresource(const docresource &other) { // Copy constructor
      object_id = other.object_id;
      class_id  = other.class_id;
      type      = other.type;
      terminate = other.terminate;
   }

   docresource& operator=(docresource &&other) noexcept { // Move assignment
      if (this == &other) return *this;
      object_id = other.object_id;
      class_id  = other.class_id;
      type      = other.type;
      terminate = other.terminate;
      other.type = RTD::NIL;
      return *this;
   }

   docresource& operator=(const docresource& other) { // Copy assignment
      if (this == &other) return *this;
      object_id = other.object_id;
      class_id  = other.class_id;
      type      = other.type;
      terminate = other.terminate;
      return *this;
   }
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
// bc_font has a dual purpose - it can maintain current font style information during parsing as well as being embedded
// in the document stream.

struct bc_font : public base_code {
   WORD font_index;     // Font lookup (will reflect the true font face, point size, style)
   FSO  options;        // Style options, like underline and italics
   ALIGN valign;        // Vertical alignment of text within the available line height
   std::string fill;    // Font fill instruction
   std::string face;    // The font face as requested by the client.  Might not match the font we actually use.
   DOUBLE point;        // The point size as requested by the client.  Might not match the font we actually use.

   bc_font(): font_index(-1), options(FSO::NIL), valign(ALIGN::BOTTOM), fill("rgb(0,0,0)"), point(0) { code = SCODE::FONT; }

   objFont * get_font();

   bc_font(const bc_font &Other) {
      // Copy another style and reset the index to -1 so that changes can refreshed
      *this = Other;
      font_index = -1;
   }
};

struct bc_font_end : public base_code {
   bc_font_end() : base_code(SCODE::FONT_END) { }
};

//********************************************************************************************************************
// stream_char provides indexing to specific characters in the stream.  It is designed to handle positional changes so
// that text string boundaries can be crossed without incident.
//
// The index and offset are set to -1 if the stream_char is invalidated.

struct stream_char {
   INDEX index;     // Byte code position within the stream
   size_t offset;   // Specific character offset within the bc_text.text string

   stream_char() : index(-1), offset(-1) { }
   stream_char(INDEX pIndex, ULONG pOffset) : index(pIndex), offset(pOffset) { }
   stream_char(INDEX pIndex) : index(pIndex), offset(0) { }

   bool operator==(const stream_char &Other) const {
      return (this->index == Other.index) and (this->offset == Other.offset);
   }

   bool operator<(const stream_char &Other) const {
      if (this->index < Other.index) return true;
      else if ((this->index IS Other.index) and (this->offset < Other.offset)) return true;
      else return false;
   }

   bool operator>(const stream_char &Other) const {
      if (this->index > Other.index) return true;
      else if ((this->index IS Other.index) and (this->offset > Other.offset)) return true;
      else return false;
   }

   bool operator<=(const stream_char &Other) const {
      if (this->index < Other.index) return true;
      else if ((this->index IS Other.index) and (this->offset <= Other.offset)) return true;
      else return false;
   }

   bool operator>=(const stream_char &Other) const {
      if (this->index > Other.index) return true;
      else if ((this->index IS Other.index) and (this->offset >= Other.offset)) return true;
      else return false;
   }

   void operator+=(const LONG Value) {
      offset += Value;
   }

   inline void reset() { index = -1; offset = -1; }
   inline bool valid() { return index != -1; }

   inline void set(INDEX pIndex, ULONG pOffset = 0) {
      index  = pIndex;
      offset = pOffset;
   }

   inline INDEX prev_code() {
      index--;
      if (index < 0) { index = -1; offset = -1; }
      else offset = 0;
      return index;
   }

   inline INDEX next_code() {
      offset = 0;
      index++;
      return index;
   }

   // NB: None of these support unicode.

   UBYTE get_char(RSTREAM &);
   UBYTE get_char(RSTREAM &, LONG Seek);
   UBYTE get_prev_char(RSTREAM &);
   UBYTE get_prev_char_or_inline(RSTREAM &);
   void erase_char(RSTREAM &); // Erase a character OR an escape code.
   void next_char(RSTREAM &);
   void prev_char(RSTREAM &);
};

//********************************************************************************************************************
// Refer to layout::new_segment().  A segment represents graphical content, which can be in the form of text,
// graphics or both.  A segment can consist of one line only - so if the layout process encounters a boundary causing
// wordwrap then a new segment must be created.

struct doc_segment {
   stream_char start;       // Starting index (including character if text)
   stream_char stop;        // Stop at this index/character
   stream_char trim_stop;   // The stopping point when whitespace is removed
   FloatRect area;          // Dimensions of the segment.
   DOUBLE gutter;           // The largest gutter value after taking into account all fonts used on the line.
   DOUBLE align_width;      // Full width of this segment if it were non-breaking
   RSTREAM *stream;         // The stream that this segment refers to
   bool  edit;              // true if this segment represents content that can be edited
   bool  allow_merge;       // true if this segment can be merged with siblings that have allow_merge set to true
};

struct doc_clip {
   DOUBLE left = 0, top = 0, right = 0, bottom = 0;
   INDEX index = 0; // The stream index of the object/table/item that is creating the clip.
   bool transparent = false; // If true, wrapping will not be performed around the clip region.
   std::string name;

   doc_clip() = default;

   doc_clip(DOUBLE pLeft, DOUBLE pTop, DOUBLE pRight, DOUBLE pBottom, LONG pIndex, bool pTransparent, const std::string &pName) :
      left(pLeft), top(pTop), right(pRight), bottom(pBottom), index(pIndex), transparent(pTransparent), name(pName) {

      if ((right - left > 20000) or (bottom - top > 20000)) {
         pf::Log log;
         log.warning("%s set invalid clip dimensions: %.0f,%.0f,%.0f,%.0f", name.c_str(), left, top, right, bottom);
         right = left;
         bottom = top;
      }
   }
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

struct mouse_over {
   std::string function; // name of function to call.
   DOUBLE top, left, bottom, right;
   LONG element_id;
};

struct tablecol {
   DOUBLE preset_width = 0;
   DOUBLE min_width = 0;   // For assisting layout
   DOUBLE width = 0;
   bool preset_width_rel = false;
};

//********************************************************************************************************************

struct bc_advance : public base_code {
   DOUBLE x, y;

   bc_advance() : x(0), y(0) { code = SCODE::ADVANCE; }
};

struct bc_index : public base_code {
   ULONG  name_hash = 0;          // The name of the index is held here as a hash
   LONG   id = 0;                 // UID for matching to the correct bc_index_end
   DOUBLE y = 0;                  // The cursor's vertical position of when the index was encountered during layout
   bool   visible = false;        // true if the content inside the index is visible (this is the default)
   bool   parent_visible = false; // true if the nearest parent index(es) will allow index content to be visible.  true is the default.  This allows Hide/ShowIndex() to manage themselves correctly

   bc_index(ULONG pName, LONG pID, LONG pY, bool pVisible, bool pParentVisible) :
      name_hash(pName), id(pID), y(pY), visible(pVisible), parent_visible(pParentVisible) {
      code = SCODE::INDEX_START;
   }
};

struct bc_index_end : public base_code {
   LONG id; // UID matching to the correct bc_index
   bc_index_end(LONG pID) : id(pID) { code = SCODE::INDEX_END; }
};

struct bc_link : public base_code {
   GuardedObject<objVectorPath> path;
   LINK  type;                    // Link type (either a function or hyperlink)
   UWORD id;
   std::string pointer_motion;    // Function to call for pointer motion events
   std::string ref;               // Function name or a path, depending on the Type
   std::vector<std::pair<std::string,std::string>> args;
   std::string fill;              // Fill instruction from the client
   bc_font font;                  // Font style from the parser

   bc_link() : type(LINK::NIL), id(0)
      { code = SCODE::LINK; }
};

struct bc_link_end : public base_code {
   bc_link_end() { code = SCODE::LINK_END; }
};

struct bc_list : public base_code {
   enum {
      ORDERED=0,
      BULLET,
      CUSTOM
   };

   std::string fill;                   // Fill to use for bullet points (valid for BULLET only).
   std::vector<std::string> buffer;    // Temp buffer, used for ordered lists
   LONG   start        = 1;            // Starting value for ordered lists (default: 1)
   LONG   item_indent  = BULLET_INDENT; // Minimum indentation for text printed for each item
   LONG   block_indent = BULLET_INDENT; // Indentation for each set of items
   LONG   item_num     = 0;
   LONG   order_insert = 0;
   DOUBLE v_spacing    = 0.5;           // Spacing between list items, expressed as a ratio
   UBYTE  type         = BULLET;
   bool   repass       = false;

   bc_list() { code = SCODE::LIST_START; }
};

struct bc_list_end : public base_code {
   bc_list_end() { code = SCODE::LIST_END; }
};

struct bc_table : public base_code {
   GuardedObject<objVectorPath> path;
   GuardedObject<objVectorViewport> viewport;
   std::vector<PathCommand> seq;
   std::vector<tablecol> columns;        // Table column management
   std::string fill, stroke;             // SVG stroke and fill instructions
   DOUBLE cell_v_spacing = 0, cell_h_spacing = 0; // Spacing between each cell
   DOUBLE cell_padding = 0;              // Spacing inside each cell (margins)
   DOUBLE row_width = 0;                 // Assists in the computation of row width
   DOUBLE x = 0, y = 0, width = 0, height = 0; // Run-time dimensions calculated during layout
   DOUBLE min_width = 0, min_height = 0; // User-determined minimum table width/height
   DOUBLE cursor_x = 0, cursor_y = 0;    // Cursor coordinates
   DOUBLE stroke_width = 0;              // Stroke width
   size_t total_clips = 0;               // Temporary record of Document->Clips.size()
   LONG   rows = 0;                      // Total number of rows in table
   LONG   row_index = 0;                 // Current row being processed, generally for debugging
   UBYTE  compute_columns = 0;
   ALIGN  align = ALIGN::NIL;            // Horizontal alignment.  If defined, the table will be floating.
   bool   width_pct = false;             // true if width is a percentage
   bool   height_pct = false;            // true if height is a percentage
   bool   cells_expanded = false;        // false if the table cells have not been expanded to match the inside table width
   bool   reset_row_height = false;      // true if the height of all rows needs to be reset in the current pass
   bool   wrap = false;
   bool   collapsed = false;             // Equivalent to HTML collapsing, eliminates whitespace between rows and cells

   // Entry followed by the minimum width of each column

   bc_table() { code = SCODE::TABLE_START; }

   inline bool floating_x() {
      return (align & (ALIGN::LEFT|ALIGN::RIGHT|ALIGN::HORIZONTAL)) != ALIGN::NIL;
   }

   void computeColumns() { // Compute the default column widths
      if (!compute_columns) return;

      compute_columns = 0;
      cells_expanded = false;

      if (!columns.empty()) {
         for (unsigned j=0; j < columns.size(); j++) {
            //if (ComputeColumns IS 1) {
            //   Columns[j].width = 0;
            //   Columns[j].min_width = 0;
            //}

            if (columns[j].preset_width_rel) { // Percentage width value
               columns[j].width = columns[j].preset_width * width;
            }
            else if (columns[j].preset_width) { // Fixed width value
               columns[j].width = columns[j].preset_width;
            }
            else columns[j].width = 0;

            if (columns[j].min_width > columns[j].width) columns[j].width = columns[j].min_width;
         }
      }
      else columns.clear();
   }
};

struct bc_table_end : public base_code {
   bc_table_end() { code = SCODE::TABLE_END; }
};

// It is recommended that font styling for paragraphs take advantage of the embedded font object.  Using a separate
// FONT code raises the chance of confusion for the user, because features like leading are calculated using the
// style registered in the paragraph.

class bc_paragraph : public base_code {
   public:
   GuardedObject<objVector> icon; // Icon representation if this is an item
   bc_font font;         // Default font that applies to this paragraph.  Embedding the font style in this way ensures that vertical placement can be computed immediately without looking for a FONT code.
   std::string value = "";
   DOUBLE x, y, height;  // Layout dimensions, manipulated at run-time
   DOUBLE block_indent;  // Indentation; also equivalent to setting a left margin value
   DOUBLE item_indent;   // For list items only.  This value is carried directly from bc_list.item_indent
   DOUBLE indent;        // Client specified indent value
   DOUBLE line_height;   // Spacing between paragraph lines on word-wrap, affects the cursor's vertical advance.  Expressed as a ratio of the m_line.line_height
   DOUBLE leading;       // Leading whitespace (minimum amount of space from the end of the last paragraph).  Expressed as a ratio of the default line height
   //DOUBLE trailing;    // Not implemented: Trailing whitespace
   // Options
   bool relative;        // True if the indent value is relative
   bool list_item;       // True if this paragraph represents a list item
   bool trim;
   bool aggregate;

   bc_paragraph() : base_code(SCODE::PARAGRAPH_START), x(0), y(0), height(0),
      block_indent(0), item_indent(0), indent(0), line_height(1.0), leading(1.0),
      relative(false), list_item(false), trim(false), aggregate(false) { }

   bc_paragraph(const bc_font &Style) : bc_paragraph() {
      font = Style;
      font.font_index = -1;
   }
};

struct bc_paragraph_end : public base_code {
   bc_paragraph_end() : base_code(SCODE::PARAGRAPH_END) { }
};

struct bc_row : public base_code {
   GuardedObject<objVectorRectangle> rect_fill;
   DOUBLE y = 0;
   DOUBLE row_height = 0; // height of all cells on this row, used when drawing the cells
   DOUBLE min_height = 0;
   std::string stroke, fill;
   bool vertical_repass = false;

   bc_row() : base_code(SCODE::ROW) { }
};

struct bc_row_end : public base_code {
   bc_row_end() : base_code(SCODE::ROW_END) { }
};

struct bc_cell : public base_code {
   GuardedObject<objVectorViewport> viewport;
   GuardedObject<objVectorRectangle> rect_fill;
   RSTREAM *stream = NULL;        // Internally managed byte code content for the cell
   LONG cell_id = 0;              // UID for the cell
   LONG column = 0;               // Column number that the cell starts in
   LONG col_span = 0;             // Number of columns spanned by this cell (normally set to 1)
   LONG row_span = 0;             // Number of rows spanned by this cell
   CB border = CB::NIL;           // Border options
   DOUBLE x = 0, y = 0;           // Cell coordinates, relative to their container
   DOUBLE width = 0, height = 0;  // Width and height of the cell
   DOUBLE stroke_width = 0;
   std::string onclick;           // name of an onclick function
   std::string edit_def;          // The edit definition that this cell is linked to (if any)
   std::vector<std::pair<std::string, std::string>> args;
   std::vector<doc_segment> segments;
   std::string stroke;
   std::string fill;
   bool modified = false;         // Set to true when content in the cell has been modified

   bc_cell(LONG pCellID, LONG pColumn) :
      base_code(SCODE::CELL), cell_id(pCellID), column(pColumn),
      col_span(1), row_span(1), x(0), y(0), width(0), height(0)
      { }

   ~bc_cell();
   bc_cell (const bc_cell &Other);
};

struct bc_text : public base_code {
   std::string text;
   std::vector<objVectorText *> vector_text;
   bool formatted = false;
   SEGINDEX segment = -1; // Reference to the first segment that manages this text string.

   bc_text() { code = SCODE::TEXT; }
   bc_text(std::string pText) : text(pText) { code = SCODE::TEXT; }
   bc_text(std::string pText, bool pFormatted) : text(pText), formatted(pFormatted) { code = SCODE::TEXT; }
};

struct bc_use : public base_code {
   std::string id; // Reference to a symbol registered in the Document's SVG object
   bool processed = false;

   bc_use() { code = SCODE::USE; }
   bc_use(std::string pID) : id(pID) { code = SCODE::USE; }
};

struct bc_xml : public base_code {
   OBJECTID object_id = 0;   // Reference to the object
   bool owned = false;      // true if the object is owned by a parent (not subject to normal document layout)
   bc_xml() { code = SCODE::XML; }
};

//********************************************************************************************************************
// Common widget management structure

struct widget_mgr {
   std::string name;                   // Client provided name identifier
   std::string label;
   std::string fill;                   // Default fill instruction
   std::string alt_fill;               // Alternative fill instruction for state changes
   std::string font_fill;              // Default fill instruction for user input text
   GuardedObject<objVectorViewport> viewport;
   GuardedObject<objVectorRectangle> rect;    // A vector will host the widget and define a clipping mask for it
   DOUBLE width = 0, height = 0;       // Client can define a fixed width/height, or leave at 0 for auto-sizing
   DOUBLE final_width, final_height;   // Final dimensions computed during layout
   DOUBLE label_width = 0, label_pad = 0;  // If a label is specified, the label_width & pad is in addition to final_width
   DOUBLE x = 0;                       // For floating widgets only, horizontal position calculated during layout
   ALIGN align = ALIGN::NIL;           // NB: If horizontal alignment is defined then the widget is treated as floating.
   bool width_pct = false, height_pct = false, padding = false;
   bool alt_state = false, internal_label = false;
   UBYTE label_pos = 1;                // 0 = left, 1 = right

   struct {
      DOUBLE left = 0, right = 0, top = 0, bottom = 0;
      bool left_pct = false, right_pct = false, top_pct = false, bottom_pct = false;
   } pad, final_pad; // Padding defines whitespace around the widget

   inline bool floating_y() {
      return false;
   }

   inline bool floating_x() {
      return (align & (ALIGN::LEFT|ALIGN::RIGHT|ALIGN::HORIZONTAL)) != ALIGN::NIL;
   }

   constexpr DOUBLE full_width() const {
      if (internal_label) return final_width + final_pad.left + final_pad.right;
      else return final_width + label_width + label_pad + final_pad.left + final_pad.right;
   }

   constexpr DOUBLE full_height() const { return final_height + final_pad.top + final_pad.bottom; }
};

//********************************************************************************************************************

struct bc_button : public base_code, widget_mgr {
   bc_button() { code = SCODE::BUTTON; }
   GuardedObject<objVectorText> label_text;
   bool processed = false;
};

struct bc_checkbox : public base_code, widget_mgr {
   bc_checkbox() { code = SCODE::CHECKBOX; }
   GuardedObject<objVectorText> label_text;
   bool processed = false;
};

struct bc_combobox : public base_code, widget_mgr {
   std::string value;
   GuardedObject<objVectorText> label_text;
   GuardedObject<objVectorViewport> clip_vp;

   bc_combobox() { code = SCODE::COMBOBOX; }
};

struct bc_input : public base_code, widget_mgr {
   std::string value;
   GuardedObject<objVectorText> label_text;
   GuardedObject<objVectorViewport> clip_vp;
   bool secret = false;

   bc_input() { code = SCODE::INPUT; }
};

struct bc_image : public base_code, widget_mgr {
   // Images inherit from widget graphics management since the rules are identical
   bc_image() { code = SCODE::IMAGE; }
};

//********************************************************************************************************************

struct ui_widget {
   std::variant<bc_checkbox *, bc_image *, bc_input *, bc_combobox *, bc_button *> widget;
   bool hover = false;                   // True if the mouse pointer is hovering over the widget
};

struct ui_link {
   bc_link origin;                       // A copy of the original link information (stable pointers are unavailable)
   FloatRect area;                       // Occupied area in the UI
   stream_char cursor_start, cursor_end; // Starting position and end of the link's segment
   std::vector<PathCommand> path;
   RSTREAM *stream;
   bool hover = false;                   // True if the mouse pointer is hovering over the link

   void exec(extDocument *);

   void append_link() {
      path.push_back({ .Type = PE::Move, .X = area.X, .Y = area.Y });
      path.push_back({ .Type = PE::HLineRel, .X = area.Width, });
      path.push_back({ .Type = PE::VLineRel, .Y = area.Height });
      path.push_back({ .Type = PE::HLineRel, .X = -area.Width, });
      path.push_back({ .Type = PE::ClosePath });
   }
};

//********************************************************************************************************************

using CODEMAP = std::unordered_map<ULONG, std::variant<bc_text, bc_advance, bc_table, bc_table_end, bc_row, bc_row_end, bc_paragraph,
      bc_paragraph_end, bc_cell, bc_link, bc_link_end, bc_list, bc_list_end, bc_index, bc_index_end,
      bc_font, bc_font_end, bc_xml, bc_image, bc_use, bc_button, bc_checkbox, bc_combobox, bc_input>>;

class RSTREAM {
public:
   std::vector<stream_code> data;
   CODEMAP codes;

   RSTREAM() { data.reserve(8 * 1024); }

   RSTREAM(RSTREAM &Other) {
      data = Other.data;
      codes = Other.codes;
   }

   void clear() {
      data.clear();
      codes.clear();
   }

   std::size_t size() const { return data.size(); }

   // Overloading [] operator to access elements in array style

   stream_code& operator[](const int Index) { return data[Index]; }

   const stream_code& operator[](const int Index) const { return data[Index]; }

   template <class T> T & lookup(const stream_char Index);
   template <class T> T & lookup(const INDEX Index);

   template <class T = base_code> T & insert(stream_char &, T &);
   template <class T = base_code> T & emplace(stream_char &, T &);
   template <class T = base_code> T & emplace(stream_char &);

   inline INDEX find_cell(LONG);
   inline INDEX find_editable_cell(std::string_view);
};

//********************************************************************************************************************

class sorted_segment { // Efficient lookup to the doc_segment array, sorted by vertical position
public:
   SEGINDEX segment;
   DOUBLE y;
};

//********************************************************************************************************************

class extDocument : public objDocument {
   public:
   FUNCTION EventCallback;
   std::unordered_map<std::string, std::string> Vars;
   std::unordered_map<std::string, std::string> Params;
   std::string Path;               // Optional file to load on Init()
   std::string PageName;           // Page name to load from the Path
   std::string Bookmark;           // Bookmark name processed from the Path
   std::string WorkingPath;        // String storage for the WorkingPath field
   std::string LinkFill, VisitedLinkFill, LinkSelectFill, FontFill, Highlight;
   RSTREAM Stream;                 // Internal stream buffer
   std::map<ULONG, XMLTag *>   TemplateIndex;
   std::vector<OBJECTID>       UIObjects;    // List of temporary objects in the UI
   std::vector<doc_segment>    Segments;
   std::vector<sorted_segment> SortSegments; // Used for UI interactivity when determining who is front-most
   std::vector<ui_link>        Links;
   std::unordered_map<OBJECTID, ui_widget> Widgets; // For UI matching viewport interactivity to widgets.
   std::vector<mouse_over>     MouseOverChain;
   std::vector<docresource>    Resources; // Tracks resources that are page related.  Terminated on page unload.
   std::vector<tab>            Tabs;
   std::vector<edit_cell>      EditCells;
   std::unordered_map<std::string_view, doc_edit> EditDefs;
   std::array<std::vector<FUNCTION>, size_t(DRT::MAX)> Triggers;
   std::vector<const XMLTag *> TemplateArgs; // If a template is called, the tag is referred here so that args can be pulled from it
   std::string FontFace;       // Default font face
   stream_char SelectStart, SelectEnd;  // Selection start & end (stream index)
   stream_char CursorIndex;    // Position of the cursor if text is selected, or edit mode is active.  It reflects the position at which entered text will be inserted.
   stream_char SelectIndex;    // The end of the selected text area, if text is selected.
   objXML *Templates;          // All templates for the current document are stored here
   objSVG *SVG;                // Allocated by the <svg> tag
   XMLTag *PageTag;            // Refers to a specific page that is being processed for the layout
   OBJECTPTR UserDefaultScript;    // Allows the developer to define a custom default script.
   OBJECTPTR DefaultScript;
   doc_edit *ActiveEditDef;  // As for ActiveEditCell, but refers to the active editing definition
   objVectorScene    *Scene; // A document specific scene is required to keep our resources away from the host
   DOUBLE VPWidth, VPHeight; // Dimensions of the host Viewport
   DOUBLE FontSize;
   DOUBLE MinPageWidth;      // Internal value for managing the page width, speeds up layout processing
   DOUBLE PageWidth;         // width of the widest section of the document page.  Can be pre-defined for a fixed width.
   DOUBLE LeftMargin, TopMargin, RightMargin, BottomMargin;
   DOUBLE CalcWidth;         // Final page width calculated from the layout process
   DOUBLE XPosition, YPosition; // Scrolling offset
   DOUBLE ClickX, ClickY;
   DOUBLE SelectCharX;        // The x coordinate of the SelectIndex character
   DOUBLE CursorCharX;        // The x coordinate of the CursorIndex character
   DOUBLE PointerX, PointerY; // Current pointer coordinates on the document surface
   LONG   TemplatesModified;  // For tracking modifications to Self->Templates (compared to Self->Templates->Modified)
   SEGINDEX ClickSegment;     // The index of the segment that the user clicked on
   SEGINDEX MouseOverSegment; // The index of the segment that the mouse is currently positioned over
   TIMER  FlashTimer;         // For flashing the cursor
   LONG   ActiveEditCellID;   // If editing is active, this refers to the ID of the cell being edited
   ULONG  ActiveEditCRC;      // CRC for cell editing area, used for managing onchange notifications
   WORD   FocusIndex;         // Tab focus index
   WORD   Invisible;          // Incremented for sections within a hidden index
   UBYTE  Processing;         // If > 0, the page layout is being altered
   UBYTE  State:3;
   bool   RefreshTemplates; // True if the template index requires refreshing.
   bool   RelPageWidth;     // Relative page width
   bool   UpdatingLayout;   // True if the page layout is in the process of being updated
   bool   VScrollVisible;   // True if the vertical scrollbar is visible to the user
   bool   MouseInPage;      // True if the mouse cursor is in the page area
   bool   PageProcessed;    // True if the parsing of page content has been completed
   bool   NoWhitespace;     // True if the parser should stop injecting whitespace characters
   bool   HasFocus;         // True if the main viewport has the focus
   bool   LMB;              // True if the LMB is depressed.
   bool   CursorState;      // True if the edit cursor is on, false if off.  Used for flashing of the cursor

   std::vector<sorted_segment> & get_sorted_segments();
};

bc_cell::~bc_cell() {
   delete stream;
}

bc_cell::bc_cell(const bc_cell &Other) {
   if (Other.stream) stream = new RSTREAM(Other.stream[0]);
   cell_id = Other.cell_id;
   column = Other.column;
   col_span = Other.col_span;
   row_span = Other.row_span;
   border = Other.border;
   x = Other.x, y = Other.y;
   width = Other.width, height = Other.height;
   stroke_width = Other.stroke_width;
   onclick = Other.onclick;
   edit_def = Other.edit_def;
   args = Other.args;
   segments = Other.segments;
   stroke = Other.stroke;
   fill = Other.fill;
   modified = Other.modified;
}
