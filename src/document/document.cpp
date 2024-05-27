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

#include <float.h>
#include <iomanip>
#include <algorithm>
#include <array>
#include <variant>
#include <stack>
#include <cmath>
#include <mutex>
#include <charconv>
#include "defs/hashes.h"

static const LONG MAX_PAGE_WIDTH    = 30000;
static const LONG MAX_PAGE_HEIGHT   = 100000;
static const LONG MIN_PAGE_WIDTH    = 20;
static const LONG MAX_DEPTH         = 40;    // Limits recursion from tables-within-tables
static const LONG WIDTH_LIMIT       = 4000;
static const LONG DEFAULT_FONTSIZE  = 16;    // 72DPI pixel size

using BYTECODE = ULONG;
using CELL_ID = ULONG;

static BYTECODE glByteCodeID = 1;
static ULONG glUID = 1000; // Use for generating unique/incrementing ID's, e.g. cell ID

using namespace pf;

JUMPTABLE_CORE
JUMPTABLE_FONT
JUMPTABLE_DISPLAY
JUMPTABLE_VECTOR

#include "defs/document.h"

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
// Function prototypes.

#include "defs/module_def.c"

struct layout; // Pre-def

static ERR activate_cell_edit(extDocument *, LONG, stream_char);
static ERR add_document_class(void);
static LONG  add_tabfocus(extDocument *, TT, BYTECODE);
static void  advance_tabfocus(extDocument *, BYTE);
static void  deactivate_edit(extDocument *, bool);
static ERR extract_script(extDocument *, const std::string &, objScript **, std::string &, std::string &);
static void  error_dialog(const std::string, const std::string);
static void  error_dialog(const std::string, ERR);
static const Field * find_field(OBJECTPTR, CSTRING, OBJECTPTR *);
static SEGINDEX find_segment(std::vector<doc_segment> &, stream_char, bool);
static LONG  find_tabfocus(extDocument *, TT, BYTECODE);
static ERR flash_cursor(extDocument *, LARGE, LARGE);
inline std::string get_font_style(const FSO);
static LONG getutf8(CSTRING, LONG *);
static ERR  insert_text(extDocument *, RSTREAM *, stream_char &, const std::string_view, bool);
static ERR  insert_xml(extDocument *, RSTREAM *, objXML *, objXML::TAGS &, LONG, STYLE = STYLE::NIL, IPF = IPF::NIL);
static ERR  key_event(objVectorViewport *, KQ, KEY, LONG);
static void layout_doc(extDocument *);
static ERR  load_doc(extDocument *, std::string, bool, ULD = ULD::NIL);
static void notify_disable_viewport(OBJECTPTR, ACTIONID, ERR, APTR);
static void notify_enable_viewport(OBJECTPTR, ACTIONID, ERR, APTR);
static void notify_focus_viewport(OBJECTPTR, ACTIONID, ERR, APTR);
static void notify_free_event(OBJECTPTR, ACTIONID, ERR, APTR);
static void notify_lostfocus_viewport(OBJECTPTR, ACTIONID, ERR, APTR);
static ERR  feedback_view(objVectorViewport *, FM);
static void process_parameters(extDocument *, const std::string_view);
static bool read_rgb8(CSTRING, RGB8 *);
static CSTRING read_unit(CSTRING, DOUBLE &, bool &);
static void redraw(extDocument *, bool);
static ERR  report_event(extDocument *, DEF, entity *, KEYVALUE *);
static void reset_cursor(extDocument *);
static ERR  resolve_fontx_by_index(extDocument *, stream_char, DOUBLE &);
static LONG safe_file_path(extDocument *, const std::string &);
static void set_focus(extDocument *, LONG, CSTRING);
static void show_bookmark(extDocument *, const std::string &);
static std::string stream_to_string(RSTREAM &, stream_char, stream_char);
static ERR  unload_doc(extDocument *, ULD = ULD::NIL);
static bool valid_objectid(extDocument *, OBJECTID);
static bool view_area(extDocument *, DOUBLE, DOUBLE, DOUBLE, DOUBLE);
static std::string write_calc(DOUBLE, WORD);

static ERR GET_WorkingPath(extDocument *, CSTRING *);

inline bool read_rgb8(const std::string Value, RGB8 *RGB) {
   return read_rgb8(Value.c_str(), RGB);
}

#ifdef DBG_STREAM
static void print_stream(RSTREAM &);
#endif

//********************************************************************************************************************
// Fonts are shared in a global cache (multiple documents can have access to the cache)

static std::deque<font_entry> glFonts; // font_entry pointers must be kept stable
static std::mutex glFontsMutex;

font_entry * bc_font::get_font()
{
   pf::Log log(__FUNCTION__);

   if (font_index IS -2) {
      if (!glFonts.empty()) return &glFonts[0]; // Always try to return a font rather than NULL
      return NULL;
   }

   if ((font_index < std::ssize(glFonts)) and (font_index >= 0)) return &glFonts[font_index];

   // Sanity check the face and point values

   if (face.empty()) face = ((extDocument *)CurrentContext())->FontFace;

   if (font_size < 3) {
      font_size = ((extDocument *)CurrentContext())->FontSize;
      if (font_size < 3) font_size = DEFAULT_FONTSIZE;
   }

   // Check the cache for this font

   auto style_name = get_font_style(options);
   APTR new_handle;
   CSTRING resolved_face;
   if (fntResolveFamilyName(face.c_str(), &resolved_face) IS ERR::Okay) {
      face.assign(resolved_face);

      if (vecGetFontHandle(face.c_str(), style_name.c_str(), 400, font_size, &new_handle) IS ERR::Okay) {
         for (unsigned i=0; i < glFonts.size(); i++) {
            if (new_handle IS glFonts[i].handle) {
               font_index = i;
               break;
            }
         }
      }

      if (font_index IS -1) { // Font not in cache
         std::lock_guard lk(glFontsMutex);

         log.branch("Index: %d, %s, %s, %g", LONG(std::ssize(glFonts)), face.c_str(), style_name.c_str(), font_size);

         if (font_index IS -1) { // Add the font to the cache
            font_index = std::ssize(glFonts);
            glFonts.emplace_back(new_handle, face, style_name, font_size);
         }
      }

      if (font_index >= 0) return &glFonts[font_index];
   }

   log.warning("Failed to create font %s:%g", face.c_str(), font_size);

   if (!glFonts.empty()) return &glFonts[0]; // Always try to return a font rather than NULL
   else return NULL;
}

//********************************************************************************************************************

template <class T> inline void remove_cursor(T a) { draw_cursor(a, false); }

//********************************************************************************************************************

static const std::array<std::string_view, LONG(SCODE::END)> strCodes = {
   "?", "Text", "Font", "FontEnd", "Link", "TabDef", "PE", "P", "EndLink", "Advance", "List", "ListEnd",
   "Table", "TableEnd", "Row", "Cell", "RowEnd", "Index", "IndexEnd", "XML", "Image", "Use", "Button", "Checkbox",
   "Combobox", "Input"
};

template <class T> inline const std::string_view & BC_NAME(RSTREAM &Stream, T Index) {
   return strCodes[LONG(Stream[Index].code)];
}

//********************************************************************************************************************

static ERR MODInit(OBJECTPTR argModule, struct CoreBase *argCoreBase)
{
   CoreBase = argCoreBase;

   argModule->getPtr(FID_Root, &modDocument);

   if (objModule::load("display", &modDisplay, &DisplayBase) != ERR::Okay) return ERR::InitModule;
   if (objModule::load("font", &modFont, &FontBase) != ERR::Okay) return ERR::InitModule;
   if (objModule::load("vector", &modVector, &VectorBase) != ERR::Okay) return ERR::InitModule;

   OBJECTID style_id;
   if (FindObject("glStyle", CLASSID::XML, FOF::NIL, &style_id) IS ERR::Okay) {
      char buffer[32];
      if (acGetKey(GetObjectPtr(style_id), "/colours/@DocumentHighlight", buffer, sizeof(buffer)) IS ERR::Okay) {
         glHighlight.assign(buffer);
      }
   }

   return add_document_class();
}

static ERR MODExpunge(void)
{
   glFonts.clear();

   if (modVector)  { FreeResource(modVector);  modVector  = NULL; }
   if (modDisplay) { FreeResource(modDisplay); modDisplay = NULL; }
   if (modFont)    { FreeResource(modFont);    modFont    = NULL; }

   if (clDocument) { FreeResource(clDocument); clDocument = NULL; }
   return ERR::Okay;
}

//********************************************************************************************************************

inline INDEX RSTREAM::find_cell(CELL_ID ID)
{
   for (INDEX i=0; i < std::ssize(data); i++) {
      if (data[i].code IS SCODE::CELL) {
         auto &cell = std::get<bc_cell>(codes[data[i].uid]);
         if ((ID) and (ID IS cell.cell_id)) return i;
      }
   }

   return -1;
}

inline INDEX RSTREAM::find_editable_cell(std::string_view EditDef)
{
   for (INDEX i=0; i < std::ssize(data); i++) {
      if (data[i].code IS SCODE::CELL) {
         auto &cell = lookup<bc_cell>(i);
         if (EditDef == cell.edit_def) return i;
      }
   }

   return -1;
}

//********************************************************************************************************************

inline doc_edit * find_editdef(extDocument *Self, std::string_view Name)
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

#include "scrollbar.cpp"
#include "streamchar.cpp"
#include "parsing.cpp"
#include "class/fields.cpp"
#include "class/document_class.cpp"
#include "debug.cpp"
#include "functions.cpp"
#include "ui.cpp"
#include "layout.cpp"
#include "menu.cpp"
#include "draw.cpp"
#include "entities.cpp"
#include "dunit.cpp"

//********************************************************************************************************************

static ERR add_document_class(void)
{
   clDocument = objMetaClass::create::global(
      fl::BaseClassID(CLASSID::DOCUMENT),
      fl::ClassVersion(VER_DOCUMENT),
      fl::Name("Document"),
      fl::Category(CCF::GUI),
      fl::Flags(CLF::INHERIT_LOCAL),
      fl::Actions(clDocumentActions),
      fl::Methods(clDocumentMethods),
      fl::Fields(clFields),
      fl::Size(sizeof(extDocument)),
      fl::Path(MOD_PATH),
      fl::FileExtension("*.rpl|*.ripple|*.ripl"));

   return clDocument ? ERR::Okay : ERR::AddClass;
}

//********************************************************************************************************************

PARASOL_MOD(MODInit, NULL, NULL, MODExpunge, MOD_IDL, NULL)
extern "C" struct ModHeader * register_document_module() { return &ModHeader; }
