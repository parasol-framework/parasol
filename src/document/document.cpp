/*********************************************************************************************************************

The source code of the Parasol project is made publicly available under the terms described in the LICENSE.TXT file
that is distributed with this package.  Please refer to it for further information on licensing.

*********************************************************************************************************************/

//#define _DEBUG
//#define DBG_LAYOUT
//#define DBG_STREAM
//#define DBG_SEGMENTS // Print list of segments
//#define DBG_WORDWRAP
//#define GUIDELINES // Clipping guidelines
//#define GUIDELINES_CONTENT // Segment guidelines

#if (defined(_DEBUG) || defined(DBG_LAYOUT) || defined(DBG_STREAM) || defined(DBG_SEGMENTS))
 #define RETAIN_LOG_LEVEL TRUE
#endif

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

static const LONG MAX_PAGE_WIDTH   = 30000;
static const LONG MAX_PAGE_HEIGHT  = 100000;
static const LONG MIN_PAGE_WIDTH   = 20;
static const LONG MAX_DEPTH        = 50;    // Limits the number of tables-within-tables
static const LONG BULLET_INDENT    = 14;    // Minimum indentation for bullet point lists
static const LONG WIDTH_LIMIT      = 4000;
static const LONG DEFAULT_FONTSIZE = 10;
static const DOUBLE MAX_VSPACING   = 6.0;
static const DOUBLE MAX_LEADING    = 6.0;
static const DOUBLE MIN_LINE_HEIGHT = 0.001;
static const DOUBLE MAX_LINE_HEIGHT = 10.0;
static const DOUBLE MIN_LEADING    = 1.0;

static ULONG glByteCodeID = 1;
static ULONG glUID = 1000; // Use for generating unique/incrementing ID's, e.g. cell ID
static UWORD glLinkID = 1; // Unique counter for links

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

std::vector<sorted_segment> & extDocument::get_sorted_segments()
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

template <class T> T & RSTREAM::insert_code(stream_char &Cursor, T &Code)
{
   // All byte codes are saved to a global container.

   if (codes.contains(Code.uid)) {
      // Sanity check - is the UID unique?  The caller probably needs to utilise glByteCodeID++
      // NB: At some point the re-use of codes should be allowed, e.g. bc_font reversions would benefit from this.
      pf::Log log(__FUNCTION__);
      log.warning("Code #%d is already registered.", Code.uid);
   }
   else codes[Code.uid] = Code;

   if (Cursor.index IS INDEX(data.size())) {
      data.emplace_back(Code.code, Code.uid);
   }
   else {
      const stream_code insert(Code.code, Code.uid);
      data.insert(data.begin() + Cursor.index, insert);
   }
   Cursor.next_code();
   return std::get<T>(codes[Code.uid]);
}

template <class T> T & RSTREAM::reserve_code(stream_char &Cursor)
{
   auto key = glByteCodeID;
   codes.emplace(key, T());
   auto &result = std::get<T>(codes[key]);

   if (Cursor.index IS INDEX(data.size())) {
      data.emplace_back(result.code, result.uid);
   }
   else {
      const stream_code insert(result.code, result.uid);
      data.insert(data.begin() + Cursor.index, insert);
   }
   Cursor.next_code();
   return result;
}

//********************************************************************************************************************

static const std::string & byte_code(SCODE Code) {
   static const std::string strCodes[] = {
      "?", "Text", "Font", "FontEnd", "Link", "TabDef", "PE",
      "P", "EndLink", "Advance", "List", "ListEnd", "Table", "TableEnd", "Row", "Cell",
      "RowEnd", "SetMargins", "Index", "IndexEnd", "XML", "Image", "Use", "Button", "Checkbox", "Combobox", "Input"
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
static void  advance_tabfocus(extDocument *, BYTE);
static void  check_mouse_click(extDocument *, DOUBLE X, DOUBLE Y);
static void  check_mouse_pos(extDocument *, DOUBLE, DOUBLE);
static void  check_mouse_release(extDocument *, DOUBLE X, DOUBLE Y);
static ERROR consume_input_events(objVector *, const InputEvent *);
static LONG  create_font(const std::string &, const std::string &, LONG);
static void  deactivate_edit(extDocument *, bool);
static void  deselect_text(extDocument *);
static ERROR extract_script(extDocument *, const std::string &, OBJECTPTR *, std::string &, std::string &);
static void  error_dialog(const std::string, const std::string);
static void  error_dialog(const std::string, ERROR);
static const Field * find_field(OBJECTPTR, CSTRING, OBJECTPTR *);
static SEGINDEX find_segment(std::vector<doc_segment> &, stream_char, bool);
static LONG  find_tabfocus(extDocument *, UBYTE, LONG);
static ERROR flash_cursor(extDocument *, LARGE, LARGE);
static std::string get_font_style(FSO);
static LONG  getutf8(CSTRING, LONG *);
static ERROR insert_text(extDocument *, RSTREAM &, stream_char &, const std::string &, bool);
static ERROR insert_xml(extDocument *, RSTREAM &, objXML *, objXML::TAGS &, LONG, STYLE = STYLE::NIL, IPF = IPF::NIL);
static ERROR key_event(objVectorViewport *, KQ, KEY, LONG);
static void  layout_doc(extDocument *);
static ERROR load_doc(extDocument *, std::string, bool, ULD = ULD::NIL);
static void  notify_disable_viewport(OBJECTPTR, ACTIONID, ERROR, APTR);
static void  notify_enable_viewport(OBJECTPTR, ACTIONID, ERROR, APTR);
static void  notify_focus_viewport(OBJECTPTR, ACTIONID, ERROR, APTR);
static void  notify_free_event(OBJECTPTR, ACTIONID, ERROR, APTR);
static void  notify_lostfocus_viewport(OBJECTPTR, ACTIONID, ERROR, APTR);
static ERROR feedback_view(objVectorViewport *, FM);
static void  process_parameters(extDocument *, const std::string &);
static bool  read_rgb8(CSTRING, RGB8 *);
static CSTRING read_unit(CSTRING, DOUBLE &, bool &);
static void  redraw(extDocument *, bool);
static ERROR report_event(extDocument *, DEF, KEYVALUE *);
static void  reset_cursor(extDocument *);
static ERROR resolve_fontx_by_index(extDocument *, stream_char, DOUBLE &);
static ERROR resolve_font_pos(doc_segment &, DOUBLE, DOUBLE &, stream_char &);
static LONG  safe_file_path(extDocument *, const std::string &);
static void  set_focus(extDocument *, LONG, CSTRING);
static void  show_bookmark(extDocument *, const std::string &);
static std::string stream_to_string(RSTREAM &, stream_char, stream_char);
static ERROR unload_doc(extDocument *, ULD = ULD::NIL);
static bool  valid_objectid(extDocument *, OBJECTID);
static BYTE  view_area(extDocument *, LONG, LONG, LONG, LONG);
static std::string write_calc(DOUBLE, WORD);

static ERROR GET_WorkingPath(extDocument *, CSTRING *);

inline bool read_rgb8(const std::string Value, RGB8 *RGB) {
   return read_rgb8(Value.c_str(), RGB);
}

#ifdef DBG_STREAM
static void print_stream(RSTREAM &);
#endif

//********************************************************************************************************************

static std::vector<font_entry> glFonts;

objFont * bc_font::get_font()
{
   if (font_index != -2) {
      if ((font_index < INDEX(glFonts.size())) and (font_index >= 0)) return glFonts[font_index].font;
      else if (font_index IS -1) {
         if (face.empty()) {
            if ((font_index = create_font(((extDocument *)CurrentContext())->FontFace, "Regular", ((extDocument *)CurrentContext())->FontSize)) IS -1) {
               font_index = create_font("Open Sans", "Regular", 10);
            }
         }
         else font_index = create_font(face, get_font_style(options), point);

         if (font_index != -1) return glFonts[font_index].font;
         else font_index = -2;
      }
   }

   pf::Log log(__FUNCTION__);
   log.warning("Failed to create font %s:%g", face.c_str(), point);

   if (!glFonts.empty()) return glFonts[0].font; // Always try to return a font rather than NULL
   else return NULL;
}

//********************************************************************************************************************
// For a given index in the stream, return the element code.  Index MUST be a valid reference to a byte code sequence.

template <class T> T & RSTREAM::lookup(const stream_char Index) {
   return std::get<T>(codes[data[Index.index].uid]);
}

template <class T> T & RSTREAM::lookup(const INDEX Index) {
   return std::get<T>(codes[data[Index].uid]);
}

template <class T> inline void remove_cursor(T a) { draw_cursor(a, false); }

template <class T> inline const std::string & BC_NAME(RSTREAM &Stream, T Index) {
   return byte_code(Stream[Index].code);
}

//********************************************************************************************************************

static ERROR CMDInit(OBJECTPTR argModule, struct CoreBase *argCoreBase)
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

static ERROR CMDExpunge(void)
{
   glFonts.clear();

   if (modVector)  { FreeResource(modVector);  modVector  = NULL; }
   if (modDisplay) { FreeResource(modDisplay); modDisplay = NULL; }
   if (modFont)    { FreeResource(modFont);    modFont    = NULL; }

   if (clDocument) { FreeResource(clDocument); clDocument = NULL; }
   return ERR_Okay;
}

//********************************************************************************************************************

inline INDEX RSTREAM::find_cell(LONG ID)
{
   for (INDEX i=0; i < INDEX(data.size()); i++) {
      if (data[i].code IS SCODE::CELL) {
         auto &cell = std::get<bc_cell>(codes[data[i].uid]);
         if ((ID) and (ID IS cell.cell_id)) return i;
      }
   }

   return -1;
}

inline INDEX RSTREAM::find_editable_cell(const std::string &EditDef)
{
   for (INDEX i=0; i < INDEX(data.size()); i++) {
      if (data[i].code IS SCODE::CELL) {
         auto &cell = lookup<bc_cell>(i);
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
// Layout the document with a lower log level to cut back on noise.

inline void layout_doc_fast(extDocument *Self)
{
#ifndef RETAIN_LOG_LEVEL
   pf::LogLevel level(2);
#endif

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

PARASOL_MOD(CMDInit, NULL, NULL, CMDExpunge, MOD_IDL, NULL)
extern "C" struct ModHeader * register_document_module() { return &ModHeader; }
