
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
   NIL         = 0x00,
   SIBLINGS    = 0x01,
   HOLD_STYLE  = 0x02,
   RESET_STYLE = 0x04,
   CLOSE_STYLE = 0x08
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

enum class SCODE : char {
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
   // End of list - NB: PLEASE UPDATE byte_code() IF YOU ADD NEW CODES
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
   SCODE code;  // Type
   ULONG uid; // Lookup for the Codes table

   stream_code() : code(SCODE::NIL), uid(0) { }
   stream_code(SCODE pCode, ULONG pID) : code(pCode), uid(pID) { }
};

using RSTREAM = std::vector<stream_code>;

//********************************************************************************************************************

class base_code {
public:
   ULONG uid;   // Unique identifier for lookup
   SCODE code = SCODE::NIL; // Byte code

   base_code() { uid = glByteCodeID++; }
   base_code(SCODE pCode) : code(pCode) { uid = glByteCodeID++; }
};

//********************************************************************************************************************
// stream_char provides indexing to specific characters in the stream.  It is designed to handle positional changes so
// that text string boundaries can be crossed without incident.
//
// The index and offset are set to -1 if the stream_char is invalidated.

struct stream_char {
   INDEX index;     // A TEXT code position within the stream
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
   inline bool valid(const RSTREAM &Stream) { return (index != -1) and (index < INDEX(Stream.size())); }

   inline void set(INDEX pIndex, ULONG pOffset) {
      index  = pIndex;
      offset = pOffset;
   }

   inline INDEX prevCode() {
      index--;
      if (index < 0) { index = -1; offset = -1; }
      else offset = 0;
      return index;
   }

   inline INDEX nextCode() {
      offset = 0;
      index++;
      return index;
   }

   // NB: None of these support unicode.

   UBYTE get_char(extDocument *, const RSTREAM &);
   UBYTE get_char(extDocument *, const RSTREAM &, LONG Seek);
   UBYTE get_prev_char(extDocument *, const RSTREAM &);
   UBYTE get_prev_char_or_inline(extDocument *, const RSTREAM &);
   void erase_char(extDocument *, RSTREAM &); // Erase a character OR an escape code.
   void next_char(extDocument *, const RSTREAM &);
   void prev_char(extDocument *, const RSTREAM &);
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
      object_id  = other.object_id;
      class_id   = other.class_id;
      type      = other.type;
      terminate = other.terminate;
      other.type = RTD::NIL;
   }

   docresource(const docresource &other) { // Copy constructor
      object_id  = other.object_id;
      class_id   = other.class_id;
      type      = other.type;
      terminate = other.terminate;
   }

   docresource& operator=(docresource &&other) noexcept { // Move assignment
      if (this == &other) return *this;
      object_id  = other.object_id;
      class_id   = other.class_id;
      type      = other.type;
      terminate = other.terminate;
      other.type = RTD::NIL;
      return *this;
   }

   docresource& operator=(const docresource& other) { // Copy assignment
      if (this == &other) return *this;
      object_id  = other.object_id;
      class_id   = other.class_id;
      type      = other.type;
      terminate = other.terminate;
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
   WORD font_index;      // font lookup
   FSO  options;
   std::string fill;    // font fill
   ALIGN valign;        // Vertical alignment of text within the available line height

   bc_font(): font_index(-1), options(FSO::NIL), fill("rgb(0,0,0)"), valign(ALIGN::BOTTOM) { code = SCODE::FONT; }

   objFont * get_font();
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
// Refer to layout::new_segment().  A segment represents graphical content, which can be in the form of text,
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
   SCODE  base_code;

   doc_link(SCODE pCode, std::variant<bc_link *, bc_cell *> pRef, SEGINDEX pSegment, LONG pX, LONG pY, LONG pWidth, LONG pHeight) :
       ref(pRef), x(pX), y(pY), width(pWidth), height(pHeight), segment(pSegment), base_code(pCode) { }

   doc_link() : x(0), y(0), width(0), height(0), segment(0), base_code(SCODE::NIL) { }

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
   bool formatted = false;
   SEGINDEX segment = -1; // Reference to the first segment that manages this text string.

   bc_text() { code = SCODE::TEXT; }
   bc_text(std::string pText) : text(pText) { code = SCODE::TEXT; }
   bc_text(std::string pText, bool pFormatted) : text(pText), formatted(pFormatted) { code = SCODE::TEXT; }
};

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
   LINK  type;    // Link type (either a function or hyperlink)
   UWORD id;
   FSO   align;
   std::string pointer_motion; // function to call for pointer motion events
   std::string ref; // function name or a path, depending on Type
   std::vector<std::pair<std::string,std::string>> args;

   bc_link() : type(LINK::NIL), id(0), align(FSO::NIL) { code = SCODE::LINK; }
};

struct bc_link_end : public base_code {
   bc_link_end() { code = SCODE::LINK_END; }
};

struct bc_image : public base_code {
   std::string src;                  // Fill instruction
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

   bc_image() { code = SCODE::IMAGE; }

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

   std::string fill;                   // Fill to use for bullet points (valid for BULLET only).
   std::vector<std::string> buffer;    // Temp buffer, used for ordered lists
   LONG   start        = 1;            // Starting value for ordered lists (default: 1)
   LONG   item_indent  = BULLET_INDENT; // Minimum indentation for text printed for each item
   LONG   block_indent = BULLET_INDENT; // Indentation for each set of items
   LONG   item_num     = 0;
   LONG   order_insert = 0;
   DOUBLE vspacing     = 0.5;           // Spacing between list items, expressed as a ratio
   UBYTE  type         = BULLET;
   bool   repass       = false;

   bc_list() { code = SCODE::LIST_START; }
};

struct bc_list_end : public base_code {
   bc_list_end() { code = SCODE::LIST_END; }
};

struct bc_set_margins : public base_code {
   WORD left = 0x7fff; WORD top = 0x7fff; WORD bottom = 0x7fff; WORD right = 0x7fff;
   bc_set_margins() { code = SCODE::SET_MARGINS; }
};

struct bc_vector : public base_code {
   OBJECTID object_id = 0;     // Reference to the vector
   CLASSID class_id = 0;       // Precise class that the object belongs to, mostly for informative/debugging purposes
   ClipRectangle margins = { 0, 0, 0, 0 };
   bool in_line       = false; // true if object is embedded as part of the text stream (treated as if it were a character)
   bool owned         = false; // true if the object is owned by a parent (not subject to normal document layout)
   bool ignore_cursor = false; // true if the client has set fixed values for both x and y
   bool block_right   = false; // true if no text may be printed to the right of the object
   bool block_left    = false; // true if no text may be printed to the left of the object
   bc_vector() { code = SCODE::VECTOR; }
};

struct bc_xml : public base_code {
   OBJECTID object_id = 0;   // Reference to the object
   bool owned = false;      // true if the object is owned by a parent (not subject to normal document layout)
   bc_xml() { code = SCODE::XML; }
};

struct bc_table : public base_code {
   struct bc_table *stack = NULL;
   objVectorPath *path = NULL;
   std::vector<PathCommand> seq;
   std::vector<tablecol> columns;        // Table column management
   std::string fill, stroke;             // SVG stroke and fill instructions
   DOUBLE cell_vspacing = 0, cell_hspacing = 0; // Spacing between each cell
   DOUBLE cell_padding = 0;              // Spacing inside each cell (margins)
   DOUBLE row_width = 0;                 // Assists in the computation of row width
   DOUBLE x = 0, y = 0, width = 0, height = 0; // Dimensions
   DOUBLE min_width = 0, min_height = 0; // User-determined minimum table width/height
   DOUBLE cursor_x = 0, cursor_y = 0;    // Cursor coordinates
   DOUBLE strokeWidth = 0;               // Stroke width
   size_t total_clips = 0;               // Temporary record of Document->Clips.size()
   LONG   rows = 0;                      // Total number of rows in table
   LONG   row_index = 0;                 // Current row being processed, generally for debugging
   UBYTE  compute_columns = 0;
   bool   width_pct = false;             // true if width is a percentage
   bool   height_pct = false;            // true if height is a percentage
   bool   cells_expanded = false;        // false if the table cells have not been expanded to match the inside table width
   bool   reset_row_height = false;      // true if the height of all rows needs to be reset in the current pass
   bool   wrap = false;
   bool   collapsed = false;             // Equivalent to HTML collapsing, eliminates whitespace between rows and cells
   // Entry followed by the minimum width of each column
   bc_table() { code = SCODE::TABLE_START; }

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

   bc_paragraph() : base_code(SCODE::PARAGRAPH_START), x(0), y(0), height(0),
      block_indent(0), item_indent(0), indent(0), vspacing(1.0), leading_ratio(1.0),
      relative(false), list_item(false), trim(false), aggregate(false) { }

   void applyStyle(const style_status &Style) {
      vspacing     = Style.list->vspacing;
      block_indent = Style.list->block_indent;
      item_indent  = Style.list->item_indent;
   }
};

struct bc_paragraph_end : public base_code {
   bc_paragraph_end() : base_code(SCODE::PARAGRAPH_END) { }
};

struct bc_row : public base_code {
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

struct bc_cell_end : public base_code {
   LONG cell_id = 0;    // Matching identifier from bc_cell
   bc_cell_end() : base_code(SCODE::CELL_END) { }
};

struct bc_cell : public base_code {
   LONG cell_id = 0;              // Identifier for the matching bc_cell_end
   LONG column = 0;               // Column number that the cell starts in
   LONG col_span = 0;             // Number of columns spanned by this cell (normally set to 1)
   LONG row_span = 0;             // Number of rows spanned by this cell
   CB border = CB::NIL;           // Border options
   DOUBLE x = 0, y = 0;           // Cell coordinates, relative to their container
   DOUBLE width = 0, height = 0;  // width and height of the cell
   DOUBLE strokeWidth = 0;
   std::string onclick;           // name of an onclick function
   std::string edit_def;          // The edit definition that this cell is linked to (if any)
   std::vector<std::pair<std::string, std::string>> args;
   std::string stroke;
   std::string fill;

   bc_cell(LONG pCellID, LONG pColumn) :
      base_code(SCODE::CELL), cell_id(pCellID), column(pColumn),
      col_span(1), row_span(1), x(0), y(0), width(0), height(0)
      { }

   INDEX find_cell_end(extDocument *, RSTREAM &, INDEX);
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
   style_status Style;
   style_status RestoreStyle;
   std::map<ULONG, XMLTag *> TemplateIndex;
   std::vector<doc_segment>    Segments;
   std::vector<sorted_segment> SortSegments; // Used for UI interactivity when determining who is front-most
   std::vector<doc_link>       Links;
   std::vector<mouse_over>     MouseOverChain;
   std::vector<docresource>    Resources; // Tracks resources that are page related.  Terminated on page unload.
   std::vector<tab>            Tabs;
   std::vector<edit_cell>      EditCells;
   std::unordered_map<std::string, doc_edit> EditDefs;
   std::unordered_map<ULONG, std::variant<bc_text, bc_advance, bc_table, bc_table_end, bc_row, bc_row_end, bc_paragraph,
      bc_paragraph_end, bc_cell, bc_cell_end, bc_link, bc_link_end, bc_list, bc_list_end, bc_index, bc_index_end,
      bc_font, bc_vector, bc_set_margins, bc_xml, bc_image>> Codes;
   std::array<std::vector<FUNCTION>, size_t(DRT::MAX)> Triggers;
   std::vector<const XMLTag *> TemplateArgs; // If a template is called, the tag is referred here so that args can be pulled from it
   std::string FontFace;       // Default font face
   FloatRect Area;             // Available space in the viewport for hosting the document
   stream_char SelectStart, SelectEnd;  // Selection start & end (stream index)
   stream_char CursorIndex;    // Position of the cursor if text is selected, or edit mode is active.  It reflects the position at which entered text will be inserted.
   stream_char SelectIndex;    // The end of the selected text area, if text is selected.
   objXML *XML;                // Source XML document
   objXML *InsertXML;          // For temporary XML parsing by the InsertXML method
   objXML *Templates;          // All templates for the current document are stored here
   objXML *InjectXML;
   objXML::TAGS *InjectTag, *HeaderTag, *FooterTag, *BodyTag;
   XMLTag *PageTag;            // Refers to a specific page that is being processed for the layout
   objTime *Time;
   OBJECTPTR CurrentObject;
   OBJECTPTR UserDefaultScript;    // Allows the developer to define a custom default script.
   OBJECTPTR DefaultScript;
   doc_edit *ActiveEditDef;  // As for ActiveEditCell, but refers to the active editing definition
   objVectorScene    *Scene; // A document specific scene is required to keep our resources away from the host
   objVectorViewport *View;  // View viewport - this contains the page and serves as the page's scrolling area
   objVectorViewport *Page;  // Page viewport - this holds the graphics content
   DOUBLE VPWidth, VPHeight;
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
   LONG   UniqueID;           // Use for generating unique/incrementing ID's, e.g. cell ID
   LONG   LinkIndex;          // Currently selected link (mouse over)
   LONG   LoopIndex;
   LONG   ElementCounter;     // Counter for element ID's
   LONG   ObjectCache;        // If counter > 0, data objects are persistent between document refreshes.
   LONG   TemplatesModified;  // For tracking modifications to Self->Templates (compared to Self->Templates->Modified)
   LONG   BreakLoop;
   LONG   GeneratedID;        // Unique ID that is regenerated on each load/refresh
   SEGINDEX ClickSegment;     // The index of the segment that the user clicked on
   SEGINDEX MouseOverSegment; // The index of the segment that the mouse is currently positioned over
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
   bool   RefreshTemplates; // True if the template index requires refreshing.
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

   template <class T = base_code> T & insert_code(stream_char &, T &);
   template <class T = base_code> T & reserve_code(stream_char &);
   std::vector<sorted_segment> & get_sorted_segments();
};
