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
The document stream consists of byte code structures that indicate font style, paragraphs, hyperlinks, text etc.

PARAGRAPH MANAGEMENT
--------------------
Drawing the document starts with a layout process that reads the document stream and generates line segments
that declare the target area and content.  These segments have a dual purpose in that they are also used for user 
interaction.

GRAPHICAL OBJECT LAYOUT RULES
-----------------------------
This text clarifies the layout rules that must be observed by classes that provide support for page layouts.

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
//#define DBG_STREAM
//#define DBG_LINES // Print list of segments
//#define DBG_WORDWRAP
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

static ULONG glByteCodeID = 1;

using namespace pf;

JUMPTABLE_CORE
JUMPTABLE_FONT
JUMPTABLE_DISPLAY
JUMPTABLE_VECTOR

#include "document.h"

//********************************************************************************************************************

static std::string glHighlight = "rgb(219,219,255,255)";

static OBJECTPTR clDocument = NULL;
static OBJECTPTR modDisplay = NULL, modFont = NULL, modDocument = NULL, modVector = NULL;

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
   std::string LinkFill, VisitedLinkFill, LinkSelectFill, FontFill, Highlight;
   RSTREAM Stream;                 // Internal stream buffer
   OBJECTPTR CurrentObject;
   OBJECTPTR UserDefaultScript;  // Allows the developer to define a custom default script.
   OBJECTPTR DefaultScript;
   style_status Style;
   style_status RestoreStyle;
   std::vector<doc_segment> Segments;
   std::vector<sorted_segment> SortSegments; // Used for UI interactivity when determining who is front-most
   std::vector<doc_clip> Clips;
   std::vector<doc_link> Links;
   std::vector<mouse_over> MouseOverChain;
   std::vector<docresource> Resources; // Tracks resources that are page related.  Terminated on page unload.
   std::vector<tab> Tabs;
   std::vector<edit_cell> EditCells;
   std::vector<OBJECTPTR> LayoutResources;
   std::unordered_map<std::string, doc_edit> EditDefs;
   std::unordered_map<ULONG, std::variant<bc_text, bc_advance, bc_table, bc_table_end, bc_row, bc_row_end, bc_paragraph,
      bc_paragraph_end, bc_cell, bc_cell_end, bc_link, bc_link_end, bc_list, bc_list_end, bc_index, bc_index_end,
      bc_font, bc_vector, bc_set_margins, bc_xml, bc_image>> Codes;
   std::array<std::vector<FUNCTION>, size_t(DRT::MAX)> Triggers;
   std::vector<const XMLTag *> TemplateArgs; // If a template is called, the tag is referred here so that args can be pulled from it
   std::string FontFace;      // Default font face
   doc_edit *ActiveEditDef;    // As for ActiveEditCell, but refers to the active editing definition
   stream_char SelectStart, SelectEnd;  // Selection start & end (stream index)
   stream_char CursorIndex;    // Position of the cursor if text is selected, or edit mode is active.  It reflects the position at which entered text will be inserted.
   stream_char SelectIndex;    // The end of the selected text area, if text is selected.
   DOUBLE VPWidth, VPHeight;
   DOUBLE FontSize;
   LONG   UniqueID;          // Use for generating unique/incrementing ID's, e.g. cell ID
   objVectorScene *Scene;    // A document specific scene is required to keep our resources away from the host
   objVectorViewport *View;  // View viewport - this contains the page and serves as the page's scrolling area
   objVectorViewport *Page;  // Page viewport - this holds the graphics content
   DOUBLE MinPageWidth;      // Internal value for managing the page width, speeds up layout processing
   DOUBLE PageWidth;         // width of the widest section of the document page.  Can be pre-defined for a fixed width.
   DOUBLE LeftMargin, TopMargin, RightMargin, BottomMargin;
   LONG   LinkIndex;         // Currently selected link (mouse over)
   LONG   CalcWidth;         // Final page width calculated from the layout process
   LONG   LoopIndex;
   LONG   ElementCounter;       // Counter for element ID's
   DOUBLE XPosition, YPosition; // Scrolling offset
   FloatRect Area;              // Available space in the viewport for hosting the document
   DOUBLE ClickX, ClickY;
   LONG   ObjectCache;        // If counter > 0, data objects are persistent between document refreshes.
   LONG   TemplatesModified;  // Track modifications to Self->Templates
   LONG   BreakLoop;
   LONG   GeneratedID;        // Unique ID that is regenerated on each load/refresh
   SEGINDEX ClickSegment;     // The index of the segment that the user clicked on
   SEGINDEX MouseOverSegment; // The index of the segment that the mouse is currently positioned over
   DOUBLE SelectCharX;        // The x coordinate of the SelectIndex character
   DOUBLE CursorCharX;        // The x coordinate of the CursorIndex character
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

   template <class T = base_code> T & insertCode(stream_char &, T &);
   template <class T = base_code> T & reserveCode(stream_char &);
   std::vector<sorted_segment> & getSortedSegments();
};

//********************************************************************************************************************

std::vector<sorted_segment> & extDocument::getSortedSegments()
{
   if ((!SortSegments.empty()) or (Segments.empty())) return SortSegments;

   auto sortseg_compare = [&] (sorted_segment &Left, sorted_segment &Right) {
      if (Left.y < Right.y) return 1;
      else if (Left.y > Right.y) return -1;
      else {
         if (Segments[Left.segment].area.X < Segments[Right.segment].area.X) return 1;
         else if (Segments[Left.segment].area.X > Segments[Right.segment].area.X) return -1;
         else return 0;
      }
   };

   // Build the SortSegments array (unsorted)

   SortSegments.resize(Segments.size());
   unsigned i;
   SEGINDEX seg;
   for (i=0, seg=0; seg < SEGINDEX(Segments.size()); seg++) {
      if ((Segments[seg].area.Height > 0) and (Segments[seg].area.Width > 0)) {
         SortSegments[i].segment = seg;
         SortSegments[i].y       = Segments[seg].area.Y;
         i++;
      }
   }

   // Shell sort

   unsigned j, h = 1;
   while (h < SortSegments.size() / 9) h = 3 * h + 1;

   for (; h > 0; h /= 3) {
      for (auto i=h; i < SortSegments.size(); i++) {
         sorted_segment temp = SortSegments[i];
         for (j=i; (j >= h) and (sortseg_compare(SortSegments[j - h], temp) < 0); j -= h) {
            SortSegments[j] = SortSegments[j - h];
         }
         SortSegments[j] = temp;
      }
   }

   return SortSegments;
}

//********************************************************************************************************************
// Inserts a byte code sequence into the text stream.

template <class T> T & extDocument::insertCode(stream_char &Cursor, T &Code)
{
   // All byte codes are saved to a global container.

   if (Codes.contains(Code.uid)) {
      // Sanity check - is the UID unique?  The caller probably needs to utilise glByteCodeID++
      // NB: At some point the re-use of codes should be allowed, e.g. bc_font reversions would benefit from this.
      pf::Log log(__FUNCTION__);
      log.warning("Code #%d is already registered.", Code.uid);
   }
   else Codes[Code.uid] = Code;

   if (Cursor.index IS INDEX(Stream.size())) {
      Stream.emplace_back(Code.code, Code.uid);
   }
   else {
      const stream_code insert(Code.code, Code.uid);
      Stream.insert(Stream.begin() + Cursor.index, insert);
   }
   Cursor.nextCode();
   return std::get<T>(Codes[Code.uid]);
}

template <class T> T & extDocument::reserveCode(stream_char &Cursor)
{
   auto key = glByteCodeID;
   Codes.emplace(key, T());
   auto &result = std::get<T>(Codes[key]);

   if (Cursor.index IS INDEX(Stream.size())) {
      Stream.emplace_back(result.code, result.uid);
   }
   else {
      const stream_code insert(result.code, result.uid);
      Stream.insert(Stream.begin() + Cursor.index, insert);
   }
   Cursor.nextCode();
   return result;
}

//********************************************************************************************************************

static const std::string & byteCode(SCODE Code) {
   static const std::string strCodes[] = {
      "?", "Text", "Font", "FontCol", "Uline", "Bkgd", "Inv", "Vector", "Link", "TabDef", "PE",
      "P", "EndLink", "Advance", "List", "ListEnd", "Table", "TableEnd", "Row", "Cell",
      "CellEnd", "RowEnd", "SetMargins", "Index", "IndexEnd", "XML", "Image"
   };

   if (LONG(Code) < ARRAYSIZE(strCodes)) return strCodes[LONG(Code)];
   return strCodes[0];
}

//********************************************************************************************************************
// Function prototypes.

#include "module_def.c"

struct layout; // Pre-def

static ERROR activate_cell_edit(extDocument *, LONG, stream_char);
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
static const Field * find_field(OBJECTPTR Object, CSTRING Name, OBJECTPTR *Source);
static ERROR tag_xml_content_eval(extDocument *, std::string &);
static SEGINDEX find_segment(extDocument *, stream_char, bool);
static LONG  find_tabfocus(extDocument *, UBYTE, LONG);
static ERROR flash_cursor(extDocument *, LARGE, LARGE);
static std::string get_font_style(FSO);
//static LONG   get_line_from_index(extDocument *, INDEX index);
static LONG  getutf8(CSTRING, LONG *);
static ERROR insert_text(extDocument *, stream_char &, const std::string &, bool);
static ERROR insert_xml(extDocument *, objXML *, objXML::TAGS &, LONG, IXF);
static ERROR insert_xml(extDocument *, objXML *, XMLTag &, stream_char TargetIndex = stream_char(), IXF Flags = IXF::NIL);
static ERROR key_event(objVectorViewport *, KQ, KEY, LONG);
static void  layout_doc(extDocument *);
static ERROR load_doc(extDocument *, std::string, bool, ULD);
static void  notify_disable_viewport(OBJECTPTR, ACTIONID, ERROR, APTR);
static void  notify_enable_viewport(OBJECTPTR, ACTIONID, ERROR, APTR);
static void  notify_focus_viewport(OBJECTPTR, ACTIONID, ERROR, APTR);
static void  notify_free_event(OBJECTPTR, ACTIONID, ERROR, APTR);
static void  notify_lostfocus_viewport(OBJECTPTR, ACTIONID, ERROR, APTR);
static void  notify_redimension_viewport(objVectorViewport *, objVector *, DOUBLE X, DOUBLE Y, DOUBLE Width, DOUBLE Height);
static TRF   parse_tag(extDocument *, objXML *, XMLTag &, stream_char &, IPF &);
static TRF   parse_tags(extDocument *, objXML *, objXML::TAGS &, stream_char &, IPF = IPF::NIL);
static void  print_xmltree(objXML::TAGS &, LONG &);
static ERROR process_page(extDocument *, objXML *);
static void  process_parameters(extDocument *, const std::string &);
static bool  read_rgb8(CSTRING, RGB8 *);
static CSTRING read_unit(CSTRING Input, DOUBLE &Output, bool &Relative);
static void  redraw(extDocument *, bool);
static ERROR report_event(extDocument *, DEF, APTR, CSTRING);
static void  reset_cursor(extDocument *);
static ERROR resolve_fontx_by_index(extDocument *, stream_char, DOUBLE &);
static ERROR resolve_font_pos(extDocument *, doc_segment &, DOUBLE, DOUBLE &, stream_char &);
static LONG  safe_file_path(extDocument *, const std::string &);
static void  set_focus(extDocument *, LONG, CSTRING);
static void  show_bookmark(extDocument *, const std::string &);
static std::string stream_to_string(extDocument *, stream_char, stream_char);
static void  style_check(extDocument *, stream_char &);
static void  tag_xml_content(extDocument *, objXML *, XMLTag &, PXF);
static ERROR unload_doc(extDocument *, ULD);
static bool  valid_object(extDocument *, OBJECTPTR);
static bool  valid_objectid(extDocument *, OBJECTID);
static BYTE  view_area(extDocument *, LONG Left, LONG Top, LONG Right, LONG Bottom);
static std::string write_calc(DOUBLE Value, WORD Precision);

inline void print_xmltree(objXML::TAGS &Tags) {
   LONG indent = 0;
   print_xmltree(Tags, indent);
}

inline bool read_rgb8(const std::string Value, RGB8 *RGB) {
   return read_rgb8(Value.c_str(), RGB);
}

static TAGROUTINE tag_advance, tag_background, tag_body, tag_bold, tag_br, tag_cache, tag_call, tag_cell;
static TAGROUTINE tag_debug, tag_div, tag_editdef, tag_focus, tag_font, tag_footer, tag_head, tag_header, tag_image;
static TAGROUTINE tag_include, tag_index, tag_inject, tag_italic, tag_li, tag_link, tag_list, tag_page;
static TAGROUTINE tag_paragraph, tag_parse, tag_pre, tag_print, tag_repeat, tag_restorestyle, tag_row, tag_savestyle;
static TAGROUTINE tag_script, tag_set, tag_setfont, tag_setmargins, tag_table, tag_template, tag_trigger;
static TAGROUTINE tag_underline, tag_xml, tag_xmlraw, tag_xmltranslate;

//********************************************************************************************************************
// TAG::OBJECTOK: Indicates that the tag can be used inside an object element, e.g. <image>.<this_tag_ok/>..</image>
// FILTER_TABLE: The tag is restricted to use within <table> sections.
// FILTER_ROW:   The tag is restricted to use within <row> sections.

static std::map<ULONG, tagroutine> glTags = {
   // Content tags (tags that affect text, the page layout etc)
   { HASH_a,             { tag_link,         TAG::CHILDREN|TAG::CONTENT } },
   { HASH_link,          { tag_link,         TAG::CHILDREN|TAG::CONTENT } },
   { HASH_b,             { tag_bold,         TAG::CHILDREN|TAG::CONTENT } },
   { HASH_div,           { tag_div,          TAG::CHILDREN|TAG::CONTENT|TAG::PARAGRAPH } },
   { HASH_p,             { tag_paragraph,    TAG::CHILDREN|TAG::CONTENT|TAG::PARAGRAPH } },
   { HASH_font,          { tag_font,         TAG::CHILDREN|TAG::CONTENT } },
   { HASH_i,             { tag_italic,       TAG::CHILDREN|TAG::CONTENT } },
   { HASH_li,            { tag_li,           TAG::CHILDREN|TAG::CONTENT } },
   { HASH_pre,           { tag_pre,          TAG::CHILDREN|TAG::CONTENT } },
   { HASH_u,             { tag_underline,    TAG::CHILDREN|TAG::CONTENT } },
   { HASH_list,          { tag_list,         TAG::CHILDREN|TAG::CONTENT|TAG::PARAGRAPH } },
   { HASH_advance,       { tag_advance,      TAG::CONTENT } },
   { HASH_br,            { tag_br,           TAG::CONTENT } },
   { HASH_image,         { tag_image,        TAG::CONTENT } },
   // Conditional command tags
   { HASH_else,          { NULL,             TAG::CONDITIONAL } },
   { HASH_elseif,        { NULL,             TAG::CONDITIONAL } },
   { HASH_repeat,        { tag_repeat,       TAG::CHILDREN|TAG::CONDITIONAL } },
   // Special instructions
   { HASH_cache,         { tag_cache,        TAG::INSTRUCTION } },
   { HASH_call,          { tag_call,         TAG::INSTRUCTION } },
   { HASH_debug,         { tag_debug,        TAG::INSTRUCTION } },
   { HASH_focus,         { tag_focus,        TAG::INSTRUCTION|TAG::OBJECTOK } },
   { HASH_include,       { tag_include,      TAG::INSTRUCTION|TAG::OBJECTOK } },
   { HASH_print,         { tag_print,        TAG::INSTRUCTION|TAG::OBJECTOK } },
   { HASH_parse,         { tag_parse,        TAG::INSTRUCTION|TAG::OBJECTOK } },
   { HASH_set,           { tag_set,          TAG::INSTRUCTION|TAG::OBJECTOK } },
   { HASH_trigger,       { tag_trigger,      TAG::INSTRUCTION } },
   // Root level tags
   { HASH_page,          { tag_page,         TAG::CHILDREN | TAG::ROOT } },
   // Others
   { HASH_background,    { tag_background,   TAG::NIL } },
   { HASH_data,          { NULL,             TAG::NIL } },
   { HASH_editdef,       { tag_editdef,      TAG::NIL } },
   { HASH_footer,        { tag_footer,       TAG::NIL } },
   { HASH_head,          { tag_head,         TAG::NIL } }, // Synonym for info
   { HASH_header,        { tag_header,       TAG::NIL } },
   { HASH_info,          { tag_head,         TAG::NIL } },
   { HASH_inject,        { tag_inject,       TAG::OBJECTOK } },
   { HASH_row,           { tag_row,          TAG::CHILDREN|TAG::FILTER_TABLE } },
   { HASH_cell,          { tag_cell,         TAG::PARAGRAPH|TAG::FILTER_ROW } },
   { HASH_table,         { tag_table,        TAG::CHILDREN } },
   { HASH_td,            { tag_cell,         TAG::CHILDREN|TAG::FILTER_ROW } },
   { HASH_tr,            { tag_row,          TAG::CHILDREN } },
   { HASH_body,          { tag_body,         TAG::NIL } },
   { HASH_index,         { tag_index,        TAG::NIL } },
   { HASH_setmargins,    { tag_setmargins,   TAG::OBJECTOK } },
   { HASH_setfont,       { tag_setfont,      TAG::OBJECTOK } },
   { HASH_restorestyle,  { tag_restorestyle, TAG::OBJECTOK } },
   { HASH_savestyle,     { tag_savestyle,    TAG::OBJECTOK } },
   { HASH_script,        { tag_script,       TAG::NIL } },
   { HASH_template,      { tag_template,     TAG::NIL } },
   { HASH_xml,           { tag_xml,          TAG::OBJECTOK } },
   { HASH_xml_raw,       { tag_xmlraw,       TAG::OBJECTOK } },
   { HASH_xml_translate, { tag_xmltranslate, TAG::OBJECTOK } }
};

#ifdef DBG_STREAM
static void print_stream(extDocument *, const RSTREAM &);
static void print_stream(extDocument *Self) { print_stream(Self, Self->Stream); }
#endif

//********************************************************************************************************************

static std::vector<font_entry> glFonts;

objFont * bc_font::get_font()
{
   if ((font_index < INDEX(glFonts.size())) and (font_index >= 0)) return glFonts[font_index].font;
   else {
      pf::Log log(__FUNCTION__);
      log.warning("Bad font index %d.  Max: %d", font_index, LONG(glFonts.size()));
      if (!glFonts.empty()) return glFonts[0].font; // Always try to return a font rather than NULL
      else return NULL;
   }
}

//********************************************************************************************************************
// For a given index in the stream, return the element code.  Index MUST be a valid reference to a byte code sequence.

template <class T> T & stream_data(extDocument *Self, stream_char Index) {
   auto &sv = Self->Codes[Self->Stream[Index.index].uid];
   return std::get<T>(sv);
}

template <class T> T & stream_data(extDocument *Self, INDEX Index) {
   auto &sv = Self->Codes[Self->Stream[Index].uid];
   return std::get<T>(sv);
}

template <class T> inline void remove_cursor(T a) { draw_cursor(a, false); }

template <class T> inline const std::string & BC_NAME(RSTREAM &Stream, T Index) {
   return byteCode(Stream[Index].code);
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
      if (Self->Stream[i].code IS SCODE::CELL) {
         auto &cell = std::get<bc_cell>(Self->Codes[Self->Stream[i].uid]);
         if ((ID) and (ID IS cell.cell_id)) return i;
      }
   }

   return -1;
}

inline INDEX find_editable_cell(extDocument *Self, const std::string &EditDef)
{
   for (INDEX i=0; i < INDEX(Self->Stream.size()); i++) {
      if (Self->Stream[i].code IS SCODE::CELL) {
         auto &cell = stream_data<bc_cell>(Self, i);
         if (EditDef == cell.edit_def) return i;
      }
   }

   return -1;
}

//********************************************************************************************************************

inline doc_edit * find_editdef(extDocument *Self, const std::string Name)
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

#include "streamchar.cpp"
#include "parsing.cpp"
#include "class/fields.cpp"
#include "class/document_class.cpp"
#include "debug.cpp"
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
