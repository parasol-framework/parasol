/*

The parsing code converts XML data to a serial byte stream, after which the XML data can be discarded.  A DOM
of the original XML content is *not* maintained.  After parsing, the stream will be ready for presentation via the
layout code elsewhere in this code base.

The stream consists of byte codes represented by the entity class.  Each type of code is represented by a C++
class prefixed with 'bc'.  Each code type has a specific purpose such as defining a new font style, paragraph,
hyperlink etc.  When a type is instantiated it will be assigned a UID and stored in the Codes hashmap.

*/

// State machine for the parser.  This information is discarded after parsing.

struct parser {
   struct process_table {
      struct bc_table *table;
      LONG row_col;
   };

   extDocument *Self;
   objXML *m_xml;

   RSTREAM *m_stream;                 // Generated stream content
   std::unique_ptr<RSTREAM> m_stream_alloc;
   objXML *m_inject_xml = NULL;
   objXML::TAGS *m_inject_tag = NULL, *m_header_tag = NULL, *m_footer_tag = NULL, *m_body_tag = NULL;
   objTime *m_time = NULL;
   LONG  m_loop_index  = 0;
   UWORD m_paragraph_depth = 0;     // Incremented when inside <p> tags
   char  m_in_template = 0;
   bool  m_strip_feeds = false;
   bool  m_check_else  = false;
   bool  m_default_pattern   = false;
   bool  m_button_patterns   = false;
   bool  m_checkbox_patterns = false;
   bool  m_combobox_patterns = false;
   stream_char m_index;
   bc_font m_style;
   std::stack<bc_list *> m_list_stack;
   std::stack<process_table> m_table_stack;

   parser(extDocument *pSelf, RSTREAM *pStream = NULL) : Self(pSelf) {
      if (pStream) {
         m_stream = pStream;
         m_index = stream_char(pStream->size());
      }
      else {
         m_stream_alloc = std::make_unique<RSTREAM>();
         m_stream = m_stream_alloc.get();
         m_index = 0;
      }
   }

   parser(extDocument *pSelf, objXML *pXML, RSTREAM *pStream = NULL) : Self(pSelf), m_xml(pXML) {
      if (pStream) {
         m_stream = pStream;
         m_index = stream_char(pStream->size());
      }
      else {
         m_stream_alloc = std::make_unique<RSTREAM>();
         m_stream = m_stream_alloc.get();
         m_index = 0;
      }
   }

   inline ERR  calc(const std::string &, DOUBLE *, std::string &);
   inline TRF  parse_tag(XMLTag &, IPF &);
   inline TRF  parse_tags(objXML::TAGS &, IPF = IPF::NIL);
   inline TRF  parse_tags_with_style(objXML::TAGS &, bc_font &, IPF = IPF::NIL);
   inline TRF  parse_tags_with_embedded_style(objXML::TAGS &, bc_font &, IPF = IPF::NIL);
   inline void process_page(objXML *pXML);
   inline void tag_xml_content(XMLTag &, PXF);
   inline void translate_attrib_args(pf::vector<XMLAttrib> &);
   inline void translate_args(const std::string &, std::string &);
   inline void translate_calc(std::string &, size_t);
   inline void translate_param(std::string &, size_t);
   inline void translate_reserved(std::string &, size_t, bool &);
   inline ERR  tag_xml_content_eval(std::string &);
   inline void trim_preformat(extDocument *);

   // Switching out the XML object is sometimes done for things like template injection

   inline objXML * change_xml(objXML *pXML) {
      auto old = m_xml;
      m_xml = pXML;
      return old;
   }

   inline void tag_advance(XMLTag &);
   inline void tag_body(XMLTag &);
   inline void tag_button(XMLTag &);
   inline void tag_call(XMLTag &);
   inline void tag_cell(XMLTag &);
   inline void tag_checkbox(XMLTag &);
   inline void tag_combobox(XMLTag &);
   inline void tag_debug(XMLTag &);
   inline void tag_div(XMLTag &);
   inline void tag_editdef(XMLTag &);
   inline void tag_font(XMLTag &);
   inline void tag_font_style(objXML::TAGS &, FSO);
   inline void tag_head(XMLTag &);
   inline void tag_image(XMLTag &);
   inline void tag_include(XMLTag &);
   inline void tag_index(XMLTag &);
   inline void tag_input(XMLTag &);
   inline void tag_li(XMLTag &);
   inline void tag_link(XMLTag &);
   inline void tag_list(XMLTag &);
   inline void tag_page(XMLTag &);
   inline void tag_paragraph(XMLTag &);
   inline void tag_parse(XMLTag &);
   inline void tag_pre(objXML::TAGS &);
   inline void tag_print(XMLTag &);
   inline void tag_repeat(XMLTag &);
   inline void tag_row(XMLTag &);
   inline void tag_script(XMLTag &);
   inline void tag_svg(XMLTag &);
   inline void tag_table(XMLTag &);
   inline void tag_template(XMLTag &);
   inline void tag_trigger(XMLTag &);
   inline void tag_use(XMLTag &);
   inline void tag_object(XMLTag &);
   inline bool check_para_attrib(const XMLAttrib &, bc_paragraph *, bc_font &);
   inline bool check_font_attrib(const XMLAttrib &, bc_font &);

   ~parser() {
      if (m_time) FreeResource(m_time);
   }

   void config_default_pattern() {
      if (m_default_pattern) return;
      m_default_pattern = true;

      if (auto pattern = objVectorPattern::create::global({
            fl::Name("default_pattern"),
            fl::SpreadMethod(VSPREAD::PAD)
         })) {

         objVectorRectangle::create::global({
            fl::Name("default_widget_bkgd"),
            fl::Owner(pattern->Scene->Viewport->UID),
            fl::X(0), fl::Y(0), fl::Width(SCALE(1.0)), fl::Height(SCALE(1.0)),
            fl::Stroke("rgb(255,255,255)"), fl::StrokeWidth(1.0),
            fl::RoundX(SCALE(0.03)), fl::RoundY(SCALE(0.03)),
            fl::Fill("rgb(0,0,0,128)")
         });

         Self->Viewport->Scene->addDef("/widget/default", pattern);
      }
   }
};

//********************************************************************************************************************
// Converts XML to byte code, then displays the page that is referenced by the PageName field by calling
// layout_doc().  If the PageName is unspecified, we use the first <page> that has no name, otherwise the first page
// irrespective of the name.
//
// This function does not clear existing data, so you can use it to append new content to existing document content.

void parser::process_page(objXML *pXML)
{
   pf::Log log(__FUNCTION__);

   log.branch("Page: %s", Self->PageName.c_str());

   if (!pXML) { Self->Error = ERR::NoData; return; }
   m_xml = pXML;

   // Look for the first page that matches the requested page name (if a name is specified).  Pages can be located anywhere
   // within the XML source - they don't have to be at the root.

   XMLTag *page = NULL;
   for (auto &scan : m_xml->Tags) {
      if (!iequals("page", scan.Attribs[0].Name)) continue;

      if (!page) page = &scan;

      if (Self->PageName.empty()) break;
      else if (auto name = scan.attrib("name")) {
         if (iequals(Self->PageName, *name)) page = &scan;
      }
   }

   Self->Error = ERR::Okay;
   if ((page) and (!page->Children.empty())) {
      Self->PageTag = page;

      bool no_header = page->attrib("no-header") ? true : false;
      bool no_footer = page->attrib("no-footer") ? true : false;

      // Reset values that are specific to page state

      Self->Segments.clear();
      Self->SortSegments.clear();
      Self->TemplateArgs.clear();
      Self->SelectStart.reset();
      Self->SelectEnd.reset();
      Self->Links.clear();

      Self->XPosition      = 0;
      Self->YPosition      = 0;
      Self->UpdatingLayout = true;
      Self->Error          = ERR::Okay;

      m_index = stream_char(m_stream->size());
      parse_tags(m_xml->Tags, IPF::NO_CONTENT);

      if ((m_header_tag) and (!no_header)) {
         insert_xml(Self, m_stream, m_xml, m_header_tag[0], m_stream->size(), STYLE::RESET_STYLE);
      }

      if (m_body_tag) {
         pf::Log log(__FUNCTION__);
         log.traceBranch("Processing this page through the body tag.");

         auto tags = m_inject_tag;
         m_inject_tag = &page->Children;
         m_in_template++;

         insert_xml(Self, m_stream, m_inject_xml, m_body_tag[0], m_stream->size(), STYLE::RESET_STYLE);

         m_in_template--;
         m_inject_tag = tags;
      }
      else {
         pf::Log log(__FUNCTION__);
         auto page_name = page->attrib("name");
         log.traceBranch("Processing page '%s'.", page_name ? page_name->c_str() : "");
         insert_xml(Self, m_stream, m_xml, page->Children, m_stream->size(), STYLE::RESET_STYLE);
      }

      if ((m_footer_tag) and (!no_footer)) {
         insert_xml(Self, m_stream, m_xml, m_footer_tag[0], m_stream->size(), STYLE::RESET_STYLE);
      }

      // If an error occurred then we have to kill the document as the stream may contain unsynchronised
      // byte codes (e.g. an unterminated SCODE::TABLE sequence).

      if (Self->Error != ERR::Okay) unload_doc(Self);
   }
   else {
      if (!Self->PageName.empty()) {
         auto msg = std::string("Failed to find page '") + Self->PageName + "' in document '" + Self->Path + "'.";
         error_dialog("Load Failed", msg);
         Self->Error = ERR::Search;
      }
      else {
         // If no name was specified and there is no page to process, revert to performing a standard insert

         auto orig_size = m_stream->size();
         Self->Error = insert_xml(Self, &Self->Stream, m_xml, m_xml->Tags, m_stream->size(), STYLE::NIL); //STYLE::RESET_STYLE);

         if (Self->Error != ERR::Okay) m_stream->data.resize(orig_size);
      }
   }
/*
   if ((!Self->Error) and (Self->MouseInPage)) {
      DOUBLE x, y;
      if (!gfxGetRelativeCursorPos(Self->Page->UID, &x, &y)) {
         check_mouse_pos(Self, x, y);
      }
   }
*/
   if (!Self->PageProcessed) {
      for (auto &trigger : Self->Triggers[LONG(DRT::PAGE_PROCESSED)]) {
         if (trigger.isScript()) {
            sc::Call(trigger);
         }
         else if (trigger.isC()) {
            auto routine = (void (*)(APTR, extDocument *, APTR))trigger.Routine;
            pf::SwitchContext context(trigger.Context);
            routine(trigger.Context, Self, trigger.Meta);
         }
      }
   }

   Self->PageProcessed = true;
}

//********************************************************************************************************************
// Translate all arguments found in a list of XML attributes.

void parser::translate_attrib_args(pf::vector<XMLAttrib> &Attribs)
{
   if (Attribs[0].isContent()) return;

   for (LONG attrib=1; attrib < std::ssize(Attribs); attrib++) {
      if (Attribs[attrib].Name.starts_with('$')) continue;

      std::string output;
      translate_args(Attribs[attrib].Value, output);
      Attribs[attrib].Value = output;
   }
}

//********************************************************************************************************************
// This function converts arguments such as [@arg] in a string.
//
// Calculations can also be performed, e.g. [=5+7]
//
// The escape code for brackets are &lsqr; and &rsqr; (not in the XML escape code standard and thus are unconverted up
// until this function is reached).
//
// If an attribute name is prefixed with '$' then no translation of the attribute value is attempted.
//
// URI parameters are referenced with [@param:default_val]
//
// If a major error occurs during processing, the function will abort, returning the error and also setting the Error
// field to the resulting error code.  The most common reason for an abort is a buffer overflow or memory allocation
// problems, so a complete abort of document processing is advisable.
//
// RESERVED WORDS
//    %index        Current loop index, if within a repeat loop.
//    %id           A unique ID that is regenerated on each document refresh.
//    %self         ID of the document object.
//    %platform     Windows, Linux or Native.
//    %random       Random string of 9 digits.
//    %current-page name of the current page.
//    %next-page    name of the next page.
//    %prev-page    name of the previous page.
//    %path         Current working path.
//    %author       Document author.
//    %description  Document description.
//    %copyright    Document copyright.
//    %keywords     Document keywords.
//    %title        Document title.
//    %font         Face, point size and style of the current font.
//    %font-face    Face of the current font.
//    %font-fill    Paint-fill instruction for the current font.
//    %font-size    Pixel size of the current font, scaled to 72 DPI.
//    %line-no      The current 'line' (technically segmented line) in the document.
//    %content      Inject content (same as <inject/> but usable inside tag attributes)
//    %tm-day       The current day (0 - 31)
//    %tm-month     The current month (1 - 12)
//    %tm-year      The current year (2008+)
//    %tm-hour      The current hour (0 - 23)
//    %tm-minute    The current minute (0 - 59)
//    %tm-second    The current second (0 - 59)
//    %view-height  Height of the document's available viewing area
//    %view-width   Width of the the document's available viewing area.

void parser::translate_args(const std::string &Input, std::string &Output)
{
   pf::Log log(__FUNCTION__);

   bool time_queried = false;
   Output = Input;

   // Do nothing if there are no special references being used

   {
      unsigned i;
      for (i=0; i < Input.size(); i++) {
         if (Input[i] IS '[') break;
         if ((Input[i] IS '&') and
             ((startswith("&lsqr;", std::string_view(Input.begin()+i, Input.end()))) or
              (startswith("&rsqr;", std::string_view(Input.begin()+i, Input.end()))))) break;
      }
      if (i >= Input.size()) return;
   }

   for (auto pos = std::ssize(Output)-1; pos >= 0; pos--) {
      if (Output[pos] IS '&') {
         if (startswith("&lsqr;", std::string_view(Output.begin()+pos, Output.end()))) Output.replace(pos, 6, "[");
         else if (startswith("&rsqr;", std::string_view(Output.begin()+pos, Output.end()))) Output.replace(pos, 6, "]");
      }
      else if (Output[pos] IS '[') {
         if (Output[pos+1] IS '=') { // Perform a calcuation within [= ... ]
            translate_calc(Output, pos);
         }
         else if (Output[pos+1] IS '@') {
            translate_param(Output, pos);
         }
         else if (Output[pos+1] IS '%') {
            translate_reserved(Output, pos, time_queried);
         }
         else { // Object translation, can be [object] or [object.field]
            // Make sure that there is a closing bracket

            LONG balance = 1;
            unsigned end;
            for (end=pos+1; (end < Output.size()) and (balance > 0); end++) {
               if (Output[end] IS '[') balance++;
               else if (Output[end] IS ']') balance--;
            }

            if (Output[end] != ']') {
               log.warning("Object reference missing square end bracket.");
               break;
            }
            end++;

            auto name = Output.substr(pos+1, Output.find_first_of(".]")-pos);

            // Get the object ID

            OBJECTID objectid = 0;
            if (!name.empty()) {
               if (FindObject(name.c_str(), CLASSID::NIL, FOF::SMART_NAMES, &objectid) IS ERR::Okay) {
                  if ((Self->Flags & DCF::UNRESTRICTED) IS DCF::NIL) {
                     // Only consider objects that are children of the document
                     bool valid = false;
                     for (auto parent_id = GetOwnerID(objectid); parent_id; parent_id = GetOwnerID(parent_id)) {
                        if (parent_id IS Self->UID) {
                           valid = true;
                           break;
                        }
                     }
                     if (!valid) objectid = 0;
                  }
               }

               if (objectid) {
                  if (valid_objectid(Self, objectid)) {
                     auto dot = Output.find('.');
                     if (dot != std::string::npos) { // Object makes a field reference
                        pf::ScopedObjectLock object(objectid, 2000);
                        if (object.granted()) {
                           OBJECTPTR target;
                           auto fieldname = Output.substr(dot+1, end-(dot+1));
                           if (auto classfield = FindField(object.obj, strihash(fieldname), &target)) {
                              if (classfield->Flags & FD_STRING) {
                                 CSTRING str;
                                 if (target->get(classfield->FieldID, &str) IS ERR::Okay) Output.replace(pos, end-pos, str);
                                 else Output.replace(pos, end-pos, "");
                              }
                              else {
                                 // Get field as a variable type and manage any buffer overflow (the use of variables
                                 // for extremely big strings is considered rare / poor design).
                                 std::string tbuffer;
                                 tbuffer.reserve(64 * 1024);
                                 while (tbuffer.capacity() < 8 * 1024 * 1024) {
                                    tbuffer[tbuffer.size()-1] = 0;
                                    GetFieldVariable(target, name.c_str(), tbuffer.data(), tbuffer.size());
                                    if (!tbuffer[tbuffer.size()-1]) break;
                                    tbuffer.reserve(tbuffer.capacity() * 2);
                                 }
                              }
                           }
                           else Output.replace(pos, end-pos, "");
                        }
                        else Output.replace(pos, end-pos, "");
                     }
                     else { // Convert the object reference to an ID
                        Output.replace(pos, end-pos, "#" + std::to_string(objectid));
                     }
                  }
                  else log.warning("Access denied to object '%s' #%d", name.c_str(), objectid);
               }
               else log.warning("Object '%s' does not exist.", name.c_str());
            }
         }
      }
   }
}

//********************************************************************************************************************

void parser::translate_calc(std::string &Output, size_t pos)
{
   std::string temp;
   temp.reserve(Output.size());
   unsigned j = 0;
   auto end = pos + 2;
   while ((end < Output.size()) and (Output[end] != ']')) {
      if (Output[end] IS '\'') {
         temp[j++] = '\'';
         for (++end; (Output[end]) and (Output[end] != '\''); end++) temp[j++] = Output[end];
         if (Output[end]) temp[j++] = Output[end++];
      }
      else if (Output[end] IS '"') {
         temp[j++] = '"';
         for (++end; (Output[end]) and (Output[end] != '"'); end++);
         if (Output[end]) temp[j++] = Output[end++];
      }
      else temp[j++] = Output[end++];
   }
   if (end < Output.size()) end++; // Skip ']'
   std::string calcbuffer;
   calc(temp, 0, calcbuffer);
   Output.replace(pos, end-pos, calcbuffer);
}

//********************************************************************************************************************
// Translate argument reference.
// Valid examples: [@arg] [@arg:defaultvalue] [@arg:"value[]"] [@arg:'value[[]]]']

void parser::translate_param(std::string &Output, size_t pos)
{
   char terminator = ']';
   auto name_end = Output.find_first_of("]:", pos);
   if (name_end IS std::string::npos) return; // Invalid format

   auto argname = Output.substr(pos+2, name_end-pos-2);

   auto true_end = name_end;
   if (Output[true_end] IS ':') {
      true_end++;
      if ((Output[name_end] IS '\'') or (Output[name_end] IS '"')) {
         terminator = Output[name_end];
         for (++true_end; (true_end < Output.size()) and (Output[true_end] != terminator); true_end++);
         true_end++; // Skip terminator and the end bracket should appear immediately after.
         if ((true_end >= Output.size()) or (Output[true_end] != ']')) return;
      }
      else {
         true_end = Output.find(']', true_end);
         if (true_end IS std::string::npos) return; // Invalid format
      }
   }

   if (!Self->TemplateArgs.empty()) {
      bool processed = false;
      for (auto it=Self->TemplateArgs.rbegin(); (!processed) and (it != Self->TemplateArgs.rend()); it++) {
         auto args = *it;
         for (LONG arg=1; arg < std::ssize(args->Attribs); arg++) {
            if (!iequals(args->Attribs[arg].Name, argname)) continue;
            Output.replace(pos, true_end+1-pos, args->Attribs[arg].Value);
            processed = true;
            break;
         }
      }

      if (processed) return;
   }

   // Check against global arguments / variables

   if (Self->Vars.contains(argname)) {
      Output.replace(pos, true_end+1-pos, Self->Vars[argname]);
   }
   else if (Self->Params.contains(argname)) {
      Output.replace(pos, true_end+1-pos, Self->Params[argname]);
   }
   else if (Output[name_end] IS ':') { // Resort to the default value
      name_end++;
      if ((Output[name_end] IS '\'') or (Output[name_end] IS '"')) {
         auto default_value = Output.substr(name_end + 1, true_end);
         Output.replace(pos, true_end+1-pos, default_value);
      }
      else {
         auto default_value = Output.substr(name_end, true_end - name_end);
         Output.replace(pos, true_end+1-pos, default_value);
      }
   }
   else Output.replace(pos, true_end+1-pos, "");
}

//********************************************************************************************************************

void parser::translate_reserved(std::string &Output, size_t pos, bool &time_queried)
{
   if (!Output.compare(pos, sizeof("[%index]")-1, "[%index]")) {
      Output.replace(pos, sizeof("[%index]")-1, std::to_string(m_loop_index));
   }
   else if (!Output.compare(pos, sizeof("[%id]")-1, "[%id]")) {
      Output.replace(pos, sizeof("[%id]")-1, std::to_string(Self->UID));
   }
   else if (!Output.compare(pos, sizeof("[%self]")-1, "[%self]")) {
      Output.replace(pos, sizeof("[%self]")-1, std::to_string(Self->UID));
   }
   else if (!Output.compare(pos, sizeof("[%platform]")-1, "[%platform]")) {
      Output.replace(pos, sizeof("[%platform]")-1, GetSystemState()->Platform);
   }
   else if (!Output.compare(pos, sizeof("[%random]")-1, "[%random]")) {
      // Generate a random string of digits
      std::string random;
      random.resize(10);
      for (unsigned j=0; j < random.size(); j++) random[j] = '0' + (rand() % 10);
      Output.replace(pos, sizeof("[%random]")-1, random);
   }
   else if (!Output.compare(pos, sizeof("[%current-page]")-1, "[%current-page]")) {
      if (Self->PageTag) {
         if (auto page_name = Self->PageTag[0].attrib("name")) {
            Output.replace(pos, sizeof("[%current-page]")-1, page_name[0]);
         }
         else Output.replace(pos, sizeof("[%current-page]")-1, "");
      }
      else Output.replace(pos, sizeof("[%current-page]")-1, "");
   }
   else if (!Output.compare(pos, sizeof("[%next-page]")-1, "[%next-page]")) {
      if (Self->PageTag) {
         auto next = Self->PageTag->attrib("next-page");
         Output.replace(pos, sizeof("[%next-page]")-1, next ? *next : "");
      }
   }
   else if (!Output.compare(pos, sizeof("[%prev-page]")-1, "[%prev-page]")) {
      if (Self->PageTag) {
         auto next = Self->PageTag->attrib("prev-page");
         Output.replace(pos, sizeof("[%prev-page]")-1, next ? *next : "");
      }
   }
   else if (!Output.compare(pos, sizeof("[%path]"), "[%path]")) {
      CSTRING workingpath = "";
      Self->get(FID_WorkingPath, &workingpath);
      if (!workingpath) workingpath = "";
      Output.replace(pos, sizeof("[%path]")-1, workingpath);
   }
   else if (!Output.compare(pos, sizeof("[%author]")-1, "[%author]")) {
      Output.replace(pos, sizeof("[%author]")-1, Self->Author);
   }
   else if (!Output.compare(pos, sizeof("[%description]")-1, "[%description]")) {
      Output.replace(pos, sizeof("[%description]")-1, Self->Description);
   }
   else if (!Output.compare(pos, sizeof("[%copyright]")-1, "[%copyright]")) {
      Output.replace(pos, sizeof("[%copyright]")-1, Self->Copyright);
   }
   else if (!Output.compare(pos, sizeof("[%keywords]")-1, "[%keywords]")) {
      Output.replace(pos, sizeof("[%keywords]")-1, Self->Keywords);
   }
   else if (!Output.compare(pos, sizeof("[%title]")-1, "[%title]")) {
      Output.replace(pos, sizeof("[%title]")-1, Self->Title);
   }
   else if (!Output.compare(pos, sizeof("[%font]")-1, "[%font]")) {
      if (auto font = m_style.get_font()) {
         char buffer[60];
         snprintf(buffer, sizeof(buffer), "%s:%g:%s", font->face.c_str(), font->font_size, font->style.c_str());
         Output.replace(pos, sizeof("[%font]")-1, buffer);
      }
   }
   else if (!Output.compare(pos, sizeof("[%font-face]")-1, "[%font-face]")) {
      if (auto font = m_style.get_font()) {
         Output.replace(pos, sizeof("[%font-face]")-1, font->face);
      }
   }
   else if (!Output.compare(pos, sizeof("[%font-fill]")-1, "[%font-fill]")) {
      Output.replace(pos, sizeof("[%font-fill]")-1, m_style.fill);
   }
   else if (!Output.compare(pos, sizeof("[%font-size]")-1, "[%font-size]")) {
      if (auto font = m_style.get_font()) {
         char buffer[28];
         snprintf(buffer, sizeof(buffer), "%g", font->font_size);
         Output.replace(pos, sizeof("[%font-size]")-1, buffer);
      }
   }
   else if (!Output.compare(pos, sizeof("[%line-height]")-1, "[%line-height]")) {
      if (auto font = m_style.get_font()) {
         char buffer[28];
         snprintf(buffer, sizeof(buffer), "%d", font->metrics.Ascent);
         Output.replace(pos, sizeof("[%line-height]")-1, buffer);
      }
   }
   else if (!Output.compare(pos, sizeof("[%line-no]")-1, "[%line-no]")) {
      auto num = std::to_string(Self->Segments.size());
      Output.replace(pos, sizeof("[%line-no]")-1, num);
   }
   else if (!Output.compare(pos, sizeof("[%content]")-1, "[%content]")) {
      if ((m_in_template) and (m_inject_tag)) {
         std::string content = xml::GetContent(m_inject_tag[0][0]);
         Output.replace(pos, sizeof("[%content]")-1, content);

         //if (!xmlSerialise(m_inject_xml, m_inject_tag[0][0].ID, XMF::INCLUDE_SIBLINGS, &content)) {
         //   Output.replace(pos, sizeof("[%content]")-1, content);
         //   FreeResource(content);
         //}
      }
   }
   else if (!Output.compare(pos, sizeof("[%tm-day]")-1, "[%tm-day]")) {
      if (!m_time) m_time = objTime::create::local();
      if (!time_queried) { acQuery(m_time); time_queried = true; }
      Output.replace(pos, sizeof("[%tm-day]")-1, std::to_string(m_time->Day));
   }
   else if (!Output.compare(pos, sizeof("[%tm-month]")-1, "[%tm-month]")) {
      if (!m_time) m_time = objTime::create::local();
      if (!time_queried) { acQuery(m_time); time_queried = true; }
      Output.replace(pos, sizeof("[%tm-month]")-1, std::to_string(m_time->Month));
   }
   else if (!Output.compare(pos, sizeof("[%tm-year]")-1, "[%tm-year]")) {
      if (!m_time) m_time = objTime::create::local();
      if (!time_queried) { acQuery(m_time); time_queried = true; }
      Output.replace(pos, sizeof("[%tm-year]")-1, std::to_string(m_time->Year));
   }
   else if (!Output.compare(pos, sizeof("[%tm-hour]")-1, "[%tm-hour]")) {
      if (!m_time) m_time = objTime::create::local();
      if (!time_queried) { acQuery(m_time); time_queried = true; }
      Output.replace(pos, sizeof("[%tm-hour]")-1, std::to_string(m_time->Hour));
   }
   else if (!Output.compare(pos, sizeof("[%tm-minute]")-1, "[%tm-minute]")) {
      if (!m_time) m_time = objTime::create::local();
      if (!time_queried) { acQuery(m_time); time_queried = true; }
      Output.replace(pos, sizeof("[%tm-minute]")-1, std::to_string(m_time->Minute));
   }
   else if (!Output.compare(pos, sizeof("[%tm-second]")-1, "[%tm-second]")) {
      if (!m_time) m_time = objTime::create::local();
      if (!time_queried) { acQuery(m_time); time_queried = true; }
      Output.replace(pos, sizeof("[%tm-second]")-1, std::to_string(m_time->Second));
   }
   else if (!Output.compare(pos, sizeof("[%version]")-1, "[%version]")) {
      Output.replace(pos, sizeof("[%version]")-1, RIPL_VERSION);
   }
   else if (!Output.compare(pos, sizeof("[%view-height]")-1, "[%view-height]")) {
      char buffer[28];
      snprintf(buffer, sizeof(buffer), "%g", Self->VPHeight);
      Output.replace(pos, sizeof("[%view-height]")-1, buffer);
   }
   else if (!Output.compare(pos, sizeof("[%view-width]")-1, "[%view-width]")) {
      char buffer[28];
      snprintf(buffer, sizeof(buffer), "%g", Self->VPWidth);
      Output.replace(pos, sizeof("[%view-width]")-1, buffer);
   }
}

//********************************************************************************************************************

static BYTE datatype(std::string_view String)
{
   LONG i = 0;
   while ((String[i]) and (String[i] <= 0x20)) i++; // Skip white-space

   if ((String[i] IS '0') and (String[i+1] IS 'x')) {
      for (i+=2; String[i]; i++) {
         if (((String[i] >= '0') and (String[i] <= '9')) or
             ((String[i] >= 'A') and (String[i] <= 'F')) or
             ((String[i] >= 'a') and (String[i] <= 'f')));
         else return 's';
      }
      return 'h';
   }

   bool is_number = true;
   bool is_float  = false;

   for (; (String[i]) and (is_number); i++) {
      if (((String[i] < '0') or (String[i] > '9')) and (String[i] != '.') and (String[i] != '-')) is_number = false;
      if (String[i] IS '.') is_float = true;
   }

   if ((is_float) and (is_number)) return 'f';
   else if (is_number) return 'i';
   else return 's';
}

//********************************************************************************************************************

static bool eval_condition(const std::string &String)
{
   pf::Log log(__FUNCTION__);

   static const FieldDef table[] = {
      { "<>", COND_NOT_EQUAL },
      { "!=", COND_NOT_EQUAL },
      { "=",  COND_EQUAL },
      { "==", COND_EQUAL },
      { "<",  COND_LESS_THAN },
      { "<=", COND_LESS_EQUAL },
      { ">",  COND_GREATER_THAN },
      { ">=", COND_GREATER_EQUAL },
      { NULL, 0 }
   };

   LONG start = 0;
   while ((start < LONG(String.size())) and (unsigned(String[start]) <= 0x20)) start++;

   bool reverse = false;

   // Find the condition statement

   size_t i;
   for (i=start; i < String.size(); i++) {
      if ((String[i] IS '!') and (String[i+1] IS '=')) break;
      if (String[i] IS '>') break;
      if (String[i] IS '<') break;
      if (String[i] IS '=') break;
   }

   // If there is no condition statement, evaluate the statement as an integer

   if (i >= String.size()) {
      if (std::stoi(String)) return true;
      else return false;
   }

   auto cpos = i;

   // Extract Test value

   while ((i > 0) and (String[i-1] IS ' ')) i--;
   std::string test(String, 0, i);

   // Condition value

   LONG condition = 0;
   {
      char cond[4];
      LONG c;
      for (i=cpos,c=0; (c < 2) and ((String[i] IS '!') or (String[i] IS '=') or (String[i] IS '>') or (String[i] IS '<')); i++) {
         cond[c++] = String[i];
      }
      cond[c] = 0;

      for (unsigned j=0; table[j].Name; j++) {
         if (iequals(cond, table[j].Name)) {
            condition = table[j].Value;
            break;
         }
      }
   }

   while ((String[i]) and (unsigned(String[i]) <= 0x20)) i++; // skip white-space

   bool truth = false;
   if (!test.empty()) {
      if (condition) {
         // Convert the If->Compare to its specified type

         auto cmp_type  = datatype(std::string_view(String.begin()+i, String.end()));
         auto test_type = datatype(test);

         if (((test_type IS 'i') or (test_type IS 'f')) and ((cmp_type IS 'i') or (cmp_type IS 'f'))) {
            auto cmp_float  = strtod(String.c_str()+i, NULL);
            auto test_float = strtod(test.c_str(), NULL);
            switch (condition) {
               case COND_NOT_EQUAL:     if (test_float != cmp_float) truth = true; break;
               case COND_EQUAL:         if (test_float IS cmp_float) truth = true; break;
               case COND_LESS_THAN:     if (test_float <  cmp_float) truth = true; break;
               case COND_LESS_EQUAL:    if (test_float <= cmp_float) truth = true; break;
               case COND_GREATER_THAN:  if (test_float >  cmp_float) truth = true; break;
               case COND_GREATER_EQUAL: if (test_float >= cmp_float) truth = true; break;
               default: log.warning("Unsupported condition type %d.", condition);
            }
         }
         else if (condition IS COND_EQUAL) {
            truth = iequals(test, String.c_str()+i);
         }
         else if (condition IS COND_NOT_EQUAL) {
            truth = !iequals(test, String.c_str()+i);
         }
         else log.warning("String comparison for condition %d not possible.", condition);
      }
      else log.warning("No test condition in \"%s\".", String.c_str());
   }
   else log.warning("No test value in \"%s\".", String.c_str());

   if (reverse) return truth ^ 1;
   else return truth;
}

//********************************************************************************************************************
// Used by if, elseif, while statements to check the satisfaction of conditions.

static bool check_tag_conditions(extDocument *Self, XMLTag &Tag)
{
   pf::Log log("eval");

   bool satisfied = false;
   bool reverse = false;
   for (unsigned i=1; i < Tag.Attribs.size(); i++) {
      if (iequals("statement", Tag.Attribs[i].Name)) {
         satisfied = eval_condition(Tag.Attribs[i].Value);
         log.trace("Statement: %s", Tag.Attribs[i].Value);
         break;
      }
      else if (iequals("exists", Tag.Attribs[i].Name)) {
         OBJECTID object_id;
         if (FindObject(Tag.Attribs[i].Value.c_str(), CLASSID::NIL, FOF::SMART_NAMES, &object_id) IS ERR::Okay) {
            satisfied = valid_objectid(Self, object_id) ? true : false;
         }
         break;
      }
      else if (iequals("notnull", Tag.Attribs[i].Name)) {
         log.trace("NotNull: %s", Tag.Attribs[i].Value);
         if (Tag.Attribs[i].Value.empty()) satisfied = false;
         else if (Tag.Attribs[i].Value == "0") satisfied = false;
         else satisfied = true;
      }
      else if ((iequals("isnull", Tag.Attribs[i].Name)) or (iequals("null", Tag.Attribs[i].Name))) {
         log.trace("IsNull: %s", Tag.Attribs[i].Value);
            if (Tag.Attribs[i].Value.empty()) satisfied = true;
            else if (Tag.Attribs[i].Value == "0") satisfied = true;
            else satisfied = false;
      }
      else if (iequals("not", Tag.Attribs[i].Name)) {
         reverse = true;
      }
   }

   // Check for a not condition and invert the satisfied value if found

   if (reverse) satisfied = satisfied ^ 1;

   return satisfied;
}

//********************************************************************************************************************
// Intended for use from parse_tags(), this is the principal function for the parsing of XML tags.  Insertion into
// the stream will occur at Index, which is updated on completion.

TRF parser::parse_tag(XMLTag &Tag, IPF &Flags)
{
   pf::Log log(__FUNCTION__);

   if (Self->Error != ERR::Okay) {
      log.traceWarning("Error field is set, returning immediately.");
      return TRF::NIL;
   }

   XMLTag *object_template = NULL;

   auto saved_attribs = Tag.Attribs;
   translate_attrib_args(Tag.Attribs);

   auto tagname = Tag.Attribs[0].Name;
   if (tagname.starts_with('$')) tagname.erase(0, 1);
   auto tag_hash = strihash(tagname);
   object_template = NULL;

   auto result = TRF::NIL;
   if (Tag.isContent()) {
      if ((Flags & IPF::NO_CONTENT) IS IPF::NIL) {
         if (m_strip_feeds) {
            if (m_paragraph_depth) { // We must be in a paragraph to accept content as text
               unsigned i = 0;
               while ((Tag.Attribs[0].Value[i] IS '\n') or (Tag.Attribs[0].Value[i] IS '\r')) i++;
               if (i > 0) {
                  auto content = Tag.Attribs[0].Value.substr(i);
                  insert_text(Self, m_stream, m_index, content, ((m_style.options & FSO::PREFORMAT) != FSO::NIL));
               }
               else insert_text(Self, m_stream, m_index, Tag.Attribs[0].Value, ((m_style.options & FSO::PREFORMAT) != FSO::NIL));
            }
            m_strip_feeds = false;
         }
         else if (m_paragraph_depth) { // We must be in a paragraph to accept content as text
            insert_text(Self, m_stream, m_index, Tag.Attribs[0].Value, ((m_style.options & FSO::PREFORMAT) != FSO::NIL));
         }
      }
      Tag.Attribs = saved_attribs;
      return result;
   }

   if (Self->Templates) { // Check for templates first, as they can be used to override the default tag names.
      if (Self->RefreshTemplates) {
         Self->TemplateIndex.clear();

         for (XMLTag &scan : Self->Templates->Tags) {
            for (unsigned i=0; i < scan.Attribs.size(); i++) {
               if (iequals("name", scan.Attribs[i].Name)) {
                  Self->TemplateIndex[strihash(scan.Attribs[i].Value)] = &scan;
               }
            }
         }

         Self->RefreshTemplates = false;
      }

      if (Self->TemplateIndex.contains(tag_hash)) {
         // Process the template by jumping into it.  Arguments in the tag are added to a sequential
         // list that will be processed in reverse by translate_attrib_args().

         auto xml  = m_inject_xml;
         auto tags = m_inject_tag;
         m_inject_xml = xml;
         m_inject_tag = &Tag.Children;
         m_in_template++;

         pf::Log log(__FUNCTION__);
         log.traceBranch("Executing template '%s'.", tagname.c_str());

         Self->TemplateArgs.push_back(&Tag);
         auto old_xml = change_xml(Self->Templates);

         parse_tags(Self->TemplateIndex[tag_hash]->Children, Flags);

         change_xml(old_xml);
         Self->TemplateArgs.pop_back();

         Tag.Attribs = saved_attribs;

         m_in_template--;
         m_inject_tag = tags;
         m_inject_xml = xml;

         return result;
      }
   }

   if ((Flags & IPF::NO_CONTENT) != IPF::NIL) { // Do nothing when content tags are not allowed
      switch (tag_hash) {
         case HASH_a:
         case HASH_link:
         case HASH_b:
         case HASH_div:
         case HASH_p:
         case HASH_font:
         case HASH_i:
         case HASH_li:
         case HASH_pre:
         case HASH_u:
         case HASH_list:
            log.trace("Content disabled on '%s', tag not processed.", tagname.c_str());
            Tag.Attribs = saved_attribs;
            return result;
         default:
            break;
      }
   }

   switch (tag_hash) {
      // Content tags (tags that affect text, the page layout etc)
      // The content is compulsory, otherwise tag has no effect
      case HASH_a:
      case HASH_link:
         if (!Tag.Children.empty()) tag_link(Tag);
         else log.trace("No content found in tag '%s'", tagname.c_str());
         break;

      case HASH_b:
         if (!Tag.Children.empty()) tag_font_style(Tag.Children, FSO::BOLD);
         break;

      case HASH_div:
         if (!Tag.Children.empty()) tag_div(Tag);
         break;

      case HASH_p: tag_paragraph(Tag); break;

      case HASH_font:
         if (!Tag.Children.empty()) tag_font(Tag);
         break;

      case HASH_i:
         if (!Tag.Children.empty()) tag_font_style(Tag.Children, FSO::ITALIC);
         break;

      case HASH_li:
         if (!Tag.Children.empty()) tag_li(Tag);
         break;

      case HASH_pre:
         if (!Tag.Children.empty()) tag_pre(Tag.Children);
         break;

      case HASH_u: if (!Tag.Children.empty()) tag_font_style(Tag.Children, FSO::UNDERLINE); break;

      case HASH_list: if (!Tag.Children.empty()) tag_list(Tag); break;

      case HASH_advance: tag_advance(Tag); break;

      case HASH_br:
         insert_text(Self, m_stream, m_index, "\n", true);
         Self->NoWhitespace = true;
         break;

      case HASH_button: tag_button(Tag); break;

      case HASH_checkbox: tag_checkbox(Tag); break;

      case HASH_combobox: tag_combobox(Tag); break;

      case HASH_input: tag_input(Tag); break;

      case HASH_image: tag_image(Tag); break;

      // Conditional command tags

      case HASH_repeat: if (!Tag.Children.empty()) tag_repeat(Tag); break;

      case HASH_break:
         // Breaking stops executing all tags (within this section) beyond the breakpoint.  If in a loop, the loop
         // will stop executing.

         result = TRF::BREAK;
         break;

      case HASH_continue:
         // Continuing - does the same thing as a break but the loop continues.
         // If used when not in a loop, then all sibling tags are skipped.
         result = TRF::CONTINUE;
         break;

      case HASH_if:
         if (check_tag_conditions(Self, Tag)) { // Statement is true
            m_check_else = false;
            result = parse_tags(Tag.Children, Flags);
         }
         else m_check_else = true;
         break;

      case HASH_elseif:
         if (m_check_else) {
            if (check_tag_conditions(Self, Tag)) { // Statement is true
               m_check_else = false;
               result = parse_tags(Tag.Children, Flags);
            }
         }
         break;

      case HASH_else:
         if (m_check_else) {
            m_check_else = false;
            result = parse_tags(Tag.Children, Flags);
         }
         break;

      case HASH_while: {
         auto saveindex = m_loop_index;
         m_loop_index = 0;

         if ((!Tag.Children.empty()) and (check_tag_conditions(Self, Tag))) {
            // Save/restore the statement string on each cycle to fully evaluate the condition each time.

            bool state = true;
            while (state) {
               state = check_tag_conditions(Self, Tag);
               Tag.Attribs = saved_attribs;
               translate_attrib_args(Tag.Attribs);

               if ((state) and ((parse_tags(Tag.Children, Flags) & TRF::BREAK) != TRF::NIL)) break;

               m_loop_index++;
            }
         }

         m_loop_index = saveindex;
         break;
      }

      // Special instructions

      case HASH_call: tag_call(Tag); break;

      case HASH_debug: tag_debug(Tag); break;

      case HASH_focus: Self->FocusIndex = Self->Tabs.size(); break;

      case HASH_include: tag_include(Tag); break;

      case HASH_print: tag_print(Tag); break;

      case HASH_parse: tag_parse(Tag); break;

      case HASH_trigger: tag_trigger(Tag); break;

      // Root level instructions

      case HASH_page: if (!Tag.Children.empty()) tag_page(Tag); break;

      case HASH_svg: if (!Tag.Children.empty()) tag_svg(Tag); break;

      // Table layout instructions

      case HASH_row:
         if ((Flags & IPF::FILTER_TABLE) IS IPF::NIL) {
            log.warning("Invalid use of <row> - Applied to invalid parent tag.");
            Self->Error = ERR::InvalidData;
         }
         else if (!Tag.Children.empty()) tag_row(Tag);
         break;

      case HASH_td: // HTML compatibility
      case HASH_cell:
         if ((Flags & IPF::FILTER_ROW) IS IPF::NIL) {
            log.warning("Invalid use of <cell> - Applied to invalid parent tag.");
            Self->Error = ERR::InvalidData;
         }
         else if (!Tag.Children.empty()) tag_cell(Tag);
         break;

      case HASH_table: if (!Tag.Children.empty()) tag_table(Tag); break;

      case HASH_tr: if (!Tag.Children.empty()) tag_row(Tag); break;

      // Others

      case HASH_data: break; // Intentionally does nothing

      case HASH_edit_def: tag_editdef(Tag); break;

      case HASH_footer:
         if (!Tag.Children.empty()) m_footer_tag = &Tag.Children;
         break;

      case HASH_header:
         if (!Tag.Children.empty()) m_header_tag = &Tag.Children;
         break;

      case HASH_info: tag_head(Tag); break;

      case HASH_inject: // This instruction can only be used from within a template.
         if (m_in_template) {
            if (m_inject_tag) {
               auto old_xml = change_xml(m_inject_xml);
               parse_tags(m_inject_tag[0], Flags);
               change_xml(old_xml);
            }
         }
         else log.warning("<inject/> request detected but not used inside a template.");
         break;

      case HASH_use: tag_use(Tag); break;

      case HASH_body: tag_body(Tag); break;

      case HASH_index: tag_index(Tag); break;

      case HASH_script: tag_script(Tag); break;

      case HASH_template: tag_template(Tag); break;

      default:
         if ((Flags & IPF::NO_CONTENT) IS IPF::NIL) {
            log.warning("Tag '%s' unsupported as an instruction or template.", tagname.c_str());
         }
         else log.warning("Unrecognised tag '%s' used in a content-restricted area.", tagname.c_str());
         break;
   } // switch

   Tag.Attribs = saved_attribs;
   return result;
}

//********************************************************************************************************************
// See also process_page(), insert_xml()

TRF parser::parse_tags(objXML::TAGS &Tags, IPF Flags)
{
   TRF result = TRF::NIL;

   for (auto &tag : Tags) {
      // Note that Flags will carry state between multiple calls to parse_tag().  This allows if/else to work correctly.
      result = parse_tag(tag, Flags);
      if ((Self->Error != ERR::Okay) or ((result & (TRF::CONTINUE|TRF::BREAK)) != TRF::NIL)) break;
   }

   return result;
}

//********************************************************************************************************************

TRF parser::parse_tags_with_style(objXML::TAGS &Tags, bc_font &Style, IPF Flags)
{
   bool font_change = false;

   if ((Style.options & (FSO::BOLD|FSO::ITALIC)) != (m_style.options & (FSO::BOLD|FSO::ITALIC))) {
      font_change = true;
   }
   else if ((Style.options & (FSO::NO_WRAP|FSO::ALIGN_CENTER|FSO::ALIGN_RIGHT|FSO::PREFORMAT|FSO::UNDERLINE)) !=
            (m_style.options & (FSO::NO_WRAP|FSO::ALIGN_CENTER|FSO::ALIGN_RIGHT|FSO::PREFORMAT|FSO::UNDERLINE))) {
      font_change = true;
   }
   else if ((Style.valign & (ALIGN::TOP|ALIGN::VERTICAL|ALIGN::BOTTOM)) != (m_style.valign & (ALIGN::TOP|ALIGN::VERTICAL|ALIGN::BOTTOM))) {
      font_change = true;
   }
   else if ((Style.face != m_style.face) or (Style.font_size != m_style.font_size)) {
      font_change = true;
   }
   else if ((Style.fill != m_style.fill)) {
      font_change = true;
   }

   auto result = TRF::NIL;
   if (font_change) {
      Style.uid = glByteCodeID++;

      auto save_status = m_style;
      m_style = Style;
      m_stream->insert(m_index, m_style);

      for (auto &tag : Tags) {
         result = parse_tag(tag, Flags);
         if ((Self->Error != ERR::Okay) or ((result & (TRF::CONTINUE|TRF::BREAK)) != TRF::NIL)) break;
      }

      m_style = save_status;
      m_stream->emplace<bc_font_end>(m_index);
   }
   else {
      for (auto &tag : Tags) {
         // Note that Flags will carry state between multiple calls to parse_tag().  This allows if/else to work correctly.
         result = parse_tag(tag, Flags);
         if ((Self->Error != ERR::Okay) or ((result & (TRF::CONTINUE|TRF::BREAK)) != TRF::NIL)) break;
      }
   }

   return result;
}

//********************************************************************************************************************

TRF parser::parse_tags_with_embedded_style(objXML::TAGS &Tags, bc_font &Style, IPF Flags)
{
   if (Tags.empty()) return TRF::NIL;

   Style.uid = glByteCodeID++;

   auto save_style = m_style;
   m_style = Style;

   TRF result = TRF::NIL;
   for (auto &tag : Tags) {
      // Note that Flags will carry state between multiple calls to parse_tag().  This allows if/else to work correctly.
      result = parse_tag(tag, Flags);
      if ((Self->Error != ERR::Okay) or ((result & (TRF::CONTINUE|TRF::BREAK)) != TRF::NIL)) break;
   }

   m_style = save_style;
   return result;
}

//********************************************************************************************************************

bool parser::check_para_attrib(const XMLAttrib &Attrib, bc_paragraph *Para, bc_font &Style)
{
   switch (strihash(Attrib.Name)) {
      case HASH_no_wrap:
         Style.options |= FSO::NO_WRAP;
         return true;

      case HASH_v_align: {
         // Vertical alignment defines the vertical position for text in cases where the line height is greater than
         // the text itself (e.g. if an image is anchored in the line).
         ALIGN align = ALIGN::NIL;
         if (iequals("top", Attrib.Value)) align = ALIGN::TOP;
         else if (iequals("center", Attrib.Value)) align = ALIGN::VERTICAL;
         else if (iequals("middle", Attrib.Value)) align = ALIGN::VERTICAL; // synonym
         else if (iequals("bottom", Attrib.Value)) align = ALIGN::BOTTOM;

         if (align != ALIGN::NIL) {
            Style.valign = (Style.valign & (ALIGN::TOP|ALIGN::VERTICAL|ALIGN::BOTTOM)) | align;
         }
         return true;
      }

      case HASH_kerning:  // REQUIRES CODE and DOCUMENTATION
         return true;

      case HASH_line_height:
         // Line height affects the advance of m_cursor_y whenever a word-wrap occurs.  It is expressed as a multiplier
         // that is applied to m_line.height.

         if (Para) Para->line_height = DUNIT(Attrib.Value, DU::LINE_HEIGHT, DBL_MIN);
         return true;

      case HASH_trim:
         if (Para) Para->trim = true;
         return true;

      case HASH_indent:
         if (Para) {
            if (Attrib.Value.empty()) Para->indent = DUNIT(3.0, DU::LINE_HEIGHT);
            else Para->indent = DUNIT(Attrib.Value, DU::PIXEL, 0);
         }
         return true;
   }

   return false;
}

//********************************************************************************************************************

bool parser::check_font_attrib(const XMLAttrib &Attrib, bc_font &Style)
{
   pf::Log log;

   switch (strihash(Attrib.Name)) {
      case HASH_colour:
         log.warning("Font 'colour' attrib is deprecated, use 'fill'");
         [[fallthrough]];
      case HASH_font_fill:
         [[fallthrough]];
      case HASH_fill:
         Style.fill = Attrib.Value;
         return true;

      case HASH_font_face:
         [[fallthrough]];
      case HASH_face: {
         auto j = Attrib.Value.find(':');
         if (j != std::string::npos) { // Font size follows
            auto str = Attrib.Value.c_str();
            j++;
            Style.font_size = DUNIT(str+j).value;
            j = Attrib.Value.find(':', j);
            if (j != std::string::npos) { // Style follows
               j++;
               if (iequals("bold", str+j)) Style.options |= FSO::BOLD;
               else if (iequals("italic", str+j)) Style.options |= FSO::ITALIC;
               else if (iequals("bold italic", str+j)) Style.options |= FSO::BOLD|FSO::ITALIC;
            }
         }

         Style.face = Attrib.Value.substr(0, j);
         return true;
      }

      case HASH_font_size:
         [[fallthrough]];
      case HASH_size:
         Style.font_size = DUNIT(Attrib.Value).value;
         return true;

      case HASH_font_style:
         [[fallthrough]];
      case HASH_style:
         if (iequals("bold", Attrib.Value)) Style.options |= FSO::BOLD;
         else if (iequals("italic", Attrib.Value)) Style.options |= FSO::ITALIC;
         else if (iequals("bold italic", Attrib.Value)) Style.options |= FSO::BOLD|FSO::ITALIC;
         return true;
   }

   return false;
}

//********************************************************************************************************************

void parser::trim_preformat(extDocument *Self)
{
   auto i = m_index.index - 1;
   for (; i > 0; i--) {
      if (m_stream[0][i].code IS SCODE::TEXT) {
         auto &text = m_stream->lookup<bc_text>(i);

         static const std::string ws(" \t\f\v\n\r");
         auto found = text.text.find_last_not_of(ws);
         if (found != std::string::npos) {
            text.text.erase(found + 1);
            break;
         }
         else text.text.clear();
      }
      else break;
   }
}

//********************************************************************************************************************
// Advances the cursor.  It is only possible to advance positively on either axis.

void parser::tag_advance(XMLTag &Tag)
{
   auto &adv = m_stream->emplace<bc_advance>(m_index);

   for (LONG i=1; i < std::ssize(Tag.Attribs); i++) {
      switch (strihash(Tag.Attribs[i].Name)) {
         case HASH_x: adv.x = DUNIT(Tag.Attribs[i].Value, DU::PIXEL); break;
         case HASH_y: adv.y = DUNIT(Tag.Attribs[i].Value, DU::PIXEL); break;
      }

      adv.x.value = std::abs(adv.x.value);
      adv.y.value = std::abs(adv.y.value);
   }
}

//********************************************************************************************************************
// NB: If a <body> tag contains any children, it is treated as a template and must contain an <inject/> tag so that
// the XML insertion point is known.

void parser::tag_body(XMLTag &Tag)
{
   pf::Log log(__FUNCTION__);

   static const LONG MAX_BODY_MARGIN = 500;

   // Body tag needs to be placed before any content

   for (LONG i=1; i < std::ssize(Tag.Attribs); i++) {
      switch (strihash(Tag.Attribs[i].Name)) {
         case HASH_clip_path: {
            OBJECTPTR clip;
            if (Self->Scene->findDef(Tag.Attribs[i].Value.c_str(), &clip) IS ERR::Okay) {
               Self->Page->set(FID_Mask, clip);
            }
            break;
         }

         case HASH_cursor_stroke:
            Self->CursorStroke = Tag.Attribs[i].Value;
            break;

         case HASH_link:
            Self->LinkFill = Tag.Attribs[i].Value;
            break;

         // This subroutine supports "N" for setting all margins to "N" and "L T R B" for setting individual
         // margins clockwise

         case HASH_margins: {
            bool rel;
            auto str = Tag.Attribs[i].Value.c_str();

            str = read_unit(str, Self->LeftMargin, rel);

            if (*str) str = read_unit(str, Self->TopMargin, rel);
            else Self->TopMargin = Self->LeftMargin;

            if (*str) str = read_unit(str, Self->RightMargin, rel);
            else Self->RightMargin = Self->TopMargin;

            if (*str) str = read_unit(str, Self->BottomMargin, rel);
            else Self->BottomMargin = Self->RightMargin;

            if (Self->LeftMargin < 0) Self->LeftMargin = 0;
            else if (Self->LeftMargin > MAX_BODY_MARGIN) Self->LeftMargin = MAX_BODY_MARGIN;

            if (Self->TopMargin < 0) Self->TopMargin = 0;
            else if (Self->TopMargin > MAX_BODY_MARGIN) Self->TopMargin = MAX_BODY_MARGIN;

            if (Self->RightMargin < 0) Self->RightMargin = 0;
            else if (Self->RightMargin > MAX_BODY_MARGIN) Self->RightMargin = MAX_BODY_MARGIN;

            if (Self->BottomMargin < 0) Self->BottomMargin = 0;
            else if (Self->BottomMargin > MAX_BODY_MARGIN) Self->BottomMargin = MAX_BODY_MARGIN;

            break;
         }

         case HASH_select_fill: // Fill to use when a link is selected (using the tab key to get to a link will select it)
            Self->LinkSelectFill = Tag.Attribs[i].Value;
            break;

         case HASH_fill:
            Self->Background = Tag.Attribs[i].Value;
            break;

         case HASH_face:
            [[fallthrough]];
         case HASH_font_face:
            Self->FontFace = Tag.Attribs[i].Value;
            break;

         case HASH_font_size: // Default font point size
            Self->FontSize = DUNIT(Tag.Attribs[i].Value).value;
            break;

         case HASH_font_colour: // DEPRECATED, use font fill
            log.warning("The fontcolour attrib is deprecated, use font-fill.");
            [[fallthrough]];
         case HASH_font_fill: // Default font fill
            Self->FontFill = Tag.Attribs[i].Value;
            break;

         case HASH_v_link:
            Self->VisitedLinkFill = Tag.Attribs[i].Value;
            break;

         case HASH_page_width:
            [[fallthrough]];
         case HASH_width:
            Self->PageWidth = std::clamp(strtod(Tag.Attribs[i].Value.c_str(), NULL), 1.0, 6000.0);

            if (Tag.Attribs[i].Value.find('%') != std::string::npos) Self->RelPageWidth = true;
            else Self->RelPageWidth = false;
            log.msg("Page width forced to %g%s.", Self->PageWidth, Self->RelPageWidth ? "%%" : "");
            break;

         default:
            log.warning("Body attribute %s=%s not supported.", Tag.Attribs[i].Name.c_str(), Tag.Attribs[i].Value.c_str());
            break;
      }
   }

   // Overwrite the default Style attributes with the client's choices

   m_style.options   = FSO::NIL;
   m_style.fill      = Self->FontFill;
   m_style.face      = Self->FontFace;
   m_style.font_size = Self->FontSize;

   if (!Tag.Children.empty()) m_body_tag = &Tag.Children;
}

//********************************************************************************************************************
// Use this instruction to call a function during the parsing of the document.
//
// The only argument required by this tag is 'function'.  All following attributes are treated as arguments that are
// passed to the called procedure (note that arguments are passed in the order in which they appear).
//
// Global arguments can be set against the script object itself if the argument is prefixed with an underscore.
//
// To call a function that isn't in the default script, simply specify the name of the target script before the
// function name, split with a dot, e.g. "script.function".
//
// <call function="[script].function" arg1="" arg2="" _global=""/>

void parser::tag_call(XMLTag &Tag)
{
   pf::Log log(__FUNCTION__);
   objScript *script = Self->DefaultScript;

   std::string function;
   if (std::ssize(Tag.Attribs) > 1) {
      if (iequals("function", Tag.Attribs[1].Name)) {
         if (auto i = Tag.Attribs[1].Value.find('.');  i != std::string::npos) {
            auto script_name = Tag.Attribs[1].Value.substr(0, i);

            OBJECTID id;
            if (FindObject(script_name.c_str(), CLASSID::NIL, FOF::NIL, &id) IS ERR::Okay) script = (objScript *)GetObjectPtr(id);

            function.assign(Tag.Attribs[1].Value, i + 1);
         }
         else function = Tag.Attribs[1].Value;
      }
   }

   if (function.empty()) {
      log.warning("The first attribute to <call/> must be a function reference.");
      Self->Error = ERR::Syntax;
      return;
   }

   if (!script) {
      log.warning("No script in this document for a requested <call/>.");
      Self->Error = ERR::Failed;
      return;
   }

   {
      pf::Log log(__FUNCTION__);
      log.traceBranch("Calling script #%d function '%s'", script->UID, function.c_str());

      if (Tag.Attribs.size() > 2) {
         std::vector<ScriptArg> args;

         unsigned index = 0;
         for (unsigned i=2; i < Tag.Attribs.size(); i++) {
            if (Tag.Attribs[i].Name[0] IS '_') { // Global variable setting
               acSetKey(script, Tag.Attribs[i].Name.c_str()+1, Tag.Attribs[i].Value.c_str());
            }
            else if (args[index].Name[0] IS '@') {
               args.emplace_back(Tag.Attribs[i].Name.c_str() + 1, Tag.Attribs[i].Value);
            }
            else args.emplace_back(Tag.Attribs[i].Name.c_str(), Tag.Attribs[i].Value);
         }

         script->exec(function.c_str(), args.data(), args.size());
      }
      else script->exec(function.c_str(), NULL, 0);
   }

   // Check for a result and print it

   CSTRING *results;
   LONG size;
   if ((GetFieldArray(script, FID_Results, (APTR *)&results, &size) IS ERR::Okay) and (size > 0)) {
      auto xmlinc = objXML::create::global(fl::Statement(results[0]), fl::Flags(XMF::PARSE_HTML|XMF::STRIP_HEADERS));
      if (xmlinc) {
         auto old_xml = change_xml(xmlinc);
         parse_tags(xmlinc->Tags);
         change_xml(old_xml);

         // Add the created XML object to the document rather than destroying it

         Self->Resources.emplace_back(xmlinc->UID, RTD::OBJECT_TEMP);
      }
      FreeResource(results);
   }
}

//********************************************************************************************************************
// A button can have both an on and off pattern, but for our purposes we'll have one pattern and rely on the click
// action to provide feedback that the button has been pressed.

const char glButtonSVG[] = R"-(
<svg width="100%" height="100%">
  <defs>
    <linearGradient id="darkEdge" x1="0" y1="1" x2="0" y2="0" gradientUnits="objectBoundingBox">
      <stop stop-color="#000000" stop-opacity="1" offset="0"/>
      <stop stop-color="#050505" stop-opacity="1" offset="0.84"/>
      <stop stop-color="#afafaf" stop-opacity="1" offset="1"/>
    </linearGradient>

    <filter id="dropShadow" color-interpolation-filters="sRGB" primitiveUnits="objectBoundingBox">
      <feGaussianBlur stdDeviation="0.013"/>
    </filter>

    <linearGradient id="shading" gradientUnits="objectBoundingBox" x1="0" y1="0" x2="0" y2="1.04">
      <stop stop-color="#ffffff" stop-opacity="0.50" offset="0"/>
      <stop stop-color="#7f7f7f" stop-opacity="0" offset="0.5"/>
      <stop stop-color="#000000" stop-opacity="0.54" offset="1"/>
    </linearGradient>
  </defs>

  <rect opacity="0.6" fill="rgb(0,0,0)" filter="url(#dropShadow)" width="95%" height="93%"
    x="2.5%" y="4%" ry="20" rx="20"/>
  <rect fill="#555d6d" width="95%" height="93%" x="2.5%" y="2.5%" ry="20" rx="20"/>
  <rect rx="20" ry="20" width="95%" height="93%" x="2.5%" y="2.5%" fill="none" stroke="url(#darkEdge)"
    stroke-width="0.5%" stroke-linecap="round" stroke-opacity="0.7" stroke-linejoin="round" stroke-miterlimit="4"/>
  <rect rx="20" ry="20" width="95%" height="93%" x="2.5%" y="2.5%" fill="url(#shading)"/>
</svg>)-";

void parser::tag_button(XMLTag &Tag)
{
   pf::Log log(__FUNCTION__);

   bc_button &widget = m_stream->emplace<bc_button>(m_index);

   for (LONG i=1; i < std::ssize(Tag.Attribs); i++) {
      auto hash = strihash(Tag.Attribs[i].Name);
      auto &value = Tag.Attribs[i].Value;
      if (hash IS HASH_fill)          widget.fill   = value;
      else if (hash IS HASH_alt_fill) widget.alt_fill = value;
      else if (hash IS HASH_name)     widget.name   = value;
      else if (hash IS HASH_width)    widget.width  = DUNIT(value);
      else if (hash IS HASH_height)   widget.height = DUNIT(value);
      else if (hash IS HASH_padding)  widget.pad.parse(value); // Outer padding
      else if (hash IS HASH_cell_padding) widget.inner_padding.parse(value); // Inner padding
      else log.warning("<button> unsupported attribute '%s'", Tag.Attribs[i].Name.c_str());
   }

   widget.internal_page = true;

   if (!m_button_patterns) {
      // Load up the default pattern values for buttons (irrespective of this button utilising them or not)
      m_button_patterns = true;

      if (auto pattern_active = objVectorPattern::create::global({
            fl::Name("button_active"),
            fl::SpreadMethod(VSPREAD::CLIP)
         })) {

         auto svg = objSVG::create { fl::Target(pattern_active->Scene), fl::Statement(glButtonSVG) };

         if (svg.ok()) {
            FreeResource(*svg);
         }
         else { // Revert to a basic rectangle if the SVG didn't process
            objVectorRectangle::create::global({
               fl::Owner(pattern_active->Scene->Viewport->UID),
               fl::Width(SCALE(1.0)), fl::Height(SCALE(1.0)),
               fl::Stroke("rgb(64,64,64,128)"), fl::StrokeWidth(2.0),
               fl::RoundX(SCALE(0.1)),
               fl::Fill("rgb(0,0,0,32)")
            });
         }

         Self->Viewport->Scene->addDef("/widget/button/active", pattern_active);
      }

      if (auto pattern_inactive = objVectorPattern::create::global({
            fl::Name("button_inactive"),
            fl::SpreadMethod(VSPREAD::CLIP)
         })) {

         auto svg = objSVG::create { fl::Target(pattern_inactive->Scene), fl::Statement(glButtonSVG) };

         if (svg.ok()) {
            FreeResource(*svg);
         }
         else {
            objVectorRectangle::create::global({
               fl::Owner(pattern_inactive->Scene->Viewport->UID),
               fl::Width(SCALE(1.0)), fl::Height(SCALE(1.0)),
               fl::Stroke("rgb(0,0,0,64)"), fl::StrokeWidth(2.0),
               fl::RoundX(SCALE(0.1)),
               fl::Fill("rgb(255,255,255,96)")
            });
         }

         Self->Viewport->Scene->addDef("/widget/button/inactive", pattern_inactive);
      }
   }

   if (widget.fill.empty())      widget.fill      = "url(#/widget/button/inactive)";
   if (widget.font_fill.empty()) widget.font_fill = "rgb(255,255,255,220)";

   widget.def_size = DUNIT(1.7, DU::FONT_SIZE);

   if (!Tag.Children.empty()) {
      Self->NoWhitespace = true; // Reset whitespace flag: false allows whitespace at the start of the cell, true prevents whitespace

      parser parse(Self, widget.stream);

      auto new_style = m_style;
      new_style.options = FSO::ALIGN_CENTER;
      new_style.valign  = ALIGN::CENTER;
      new_style.fill    = widget.font_fill;

      parse.m_paragraph_depth++;
      parse.parse_tags_with_style(Tag.Children, new_style);
      parse.m_paragraph_depth--;
   }

   Self->NoWhitespace = false; // Widgets are treated as inline characters
}

//********************************************************************************************************************

void parser::tag_checkbox(XMLTag &Tag)
{
   pf::Log log(__FUNCTION__);

   bc_checkbox &widget = m_stream->emplace<bc_checkbox>(m_index);

   for (LONG i=1; i < std::ssize(Tag.Attribs); i++) {
      auto hash = strihash(Tag.Attribs[i].Name);
      auto &value = Tag.Attribs[i].Value;

      if (hash IS HASH_label)      widget.label = value;
      else if (hash IS HASH_name)  widget.name  = value;
      else if (hash IS HASH_fill)  widget.fill  = value;
      else if (hash IS HASH_width) widget.width = DUNIT(value);
      else if (hash IS HASH_label_pos) {
         if (iequals("left", value)) widget.label_pos = 0;
         else if (iequals("right", value)) widget.label_pos = 1;
      }
      else if (hash IS HASH_value) {
         widget.alt_state = (value == "1") or (value == "true");
      }
      else log.warning("<checkbox> unsupported attribute '%s'", Tag.Attribs[i].Name.c_str());
   }

   if (widget.fill.empty()) widget.fill = "url(#/widget/checkbox/off)";

   if (widget.alt_fill.empty()) widget.alt_fill = "url(#/widget/checkbox/on)";

   if (!m_checkbox_patterns) {
      m_checkbox_patterns = true;

      if (auto pattern_on = objVectorPattern::create::global({
            fl::Name("checkbox_on"),
            fl::SpreadMethod(VSPREAD::CLIP)
         })) {

         auto vp = pattern_on->Scene->Viewport;
         objVectorRectangle::create::global({
            fl::Owner(vp->UID),
            fl::X(-8), fl::Y(-8), fl::Width(54), fl::Height(54),
            fl::Stroke("rgb(255,255,255)"), fl::StrokeWidth(2.0),
            fl::RoundX(6), fl::RoundY(6),
            fl::Fill("rgb(0,0,0,128)")
         });

         objVectorPath::create::global({
            fl::Owner(vp->UID),
            fl::Sequence("M4.75 15.0832 15.8333 26.1665 33.2498 4 38 8.75 15.8333 35.6665 0 19.8332 4.75 15.0832Z"),
            fl::Fill("rgb(255,255,255)")
         });

         vp->setFields(fl::AspectRatio(ARF::X_MIN|ARF::Y_MIN|ARF::MEET),
            fl::ViewX(-8), fl::ViewY(-8), fl::ViewWidth(54), fl::ViewHeight(54));

         Self->Viewport->Scene->addDef("/widget/checkbox/on", pattern_on);
      }

      if (auto pattern_off = objVectorPattern::create::global({
            fl::Name("checkbox_off"),
            fl::SpreadMethod(VSPREAD::CLIP)
         })) {

         auto vp = pattern_off->Scene->Viewport;
         objVectorRectangle::create::global({
            fl::Owner(vp->UID),
            fl::X(-8), fl::Y(-8), fl::Width(54), fl::Height(54),
            fl::Stroke("rgb(255,255,255)"), fl::StrokeWidth(2.0),
            fl::RoundX(6), fl::RoundY(6),
            fl::Fill("rgb(0,0,0,128)")
         });

         objVectorPath::create::global({
            fl::Owner(vp->UID),
            fl::Sequence("M4.75 15.0832 15.8333 26.1665 33.2498 4 38 8.75 15.8333 35.6665 0 19.8332 4.75 15.0832Z"),
            fl::Fill("rgb(255,255,255,64)")
         });

         vp->setFields(fl::AspectRatio(ARF::X_MIN|ARF::Y_MIN|ARF::MEET),
            fl::ViewX(-8), fl::ViewY(-8), fl::ViewWidth(54), fl::ViewHeight(54));

         Self->Viewport->Scene->addDef("/widget/checkbox/off", pattern_off);
      }
   }

   widget.def_size = DUNIT(1.4, DU::FONT_SIZE);

   if (!widget.label.empty()) widget.label_pad = m_style.get_font()->metrics.Ascent * 0.5;

   Self->NoWhitespace = false; // Widgets are treated as inline characters
}

//********************************************************************************************************************

void parser::tag_combobox(XMLTag &Tag)
{
   pf::Log log(__FUNCTION__);

   bc_combobox &widget = m_stream->emplace<bc_combobox>(m_index);

   for (LONG i=1; i < std::ssize(Tag.Attribs); i++) {
      auto hash = strihash(Tag.Attribs[i].Name);
      auto &value = Tag.Attribs[i].Value;
      if (hash IS HASH_label)          widget.label = value;
      else if (hash IS HASH_value)     widget.value = value;
      else if (hash IS HASH_fill)      widget.fill = value;
      else if (hash IS HASH_font_fill) widget.font_fill = value;
      else if (hash IS HASH_name)      widget.name = value;
      else if (hash IS HASH_label_pos) {
         if (iequals("left", value)) widget.label_pos = 0;
         else if (iequals("right", value)) widget.label_pos = 1;
      }
      else if (hash IS HASH_width) widget.width = DUNIT(value);
      else log.warning("<combobox> unsupported attribute '%s'", Tag.Attribs[i].Name.c_str());
   }

   // Process <option/> tags for the drop-down menu.
   // The content within each option is used as presentation in the drop-down list.
   // The 'value' attrib, if declared, will appear in the combobox for the selected item.
   // The 'id' attrib, if declared, is a UID hidden from the user.

   if (!Tag.Children.empty()) {
      for (auto &scan : Tag.Children) {
         if (iequals("style", scan.name())) {
            // Client is overriding the decorator: A custom SVG background is expected, defs and body
            // adjustments may also be provided.
            if (scan.hasContent()) {
               STRING xml_ser;
               if (m_xml->serialise(scan.Children[0].ID, XMF::INCLUDE_SIBLINGS, &xml_ser) IS ERR::Okay) {
                  widget.style = xml_ser;
                  FreeResource(xml_ser);
               }
            }
         }
         else if (iequals("option", scan.name())) {
            std::string value;

            if (!scan.Children.empty()) {
               STRING xml_ser;
               if (m_xml->serialise(scan.Children[0].ID, XMF::INCLUDE_SIBLINGS, &xml_ser) IS ERR::Okay) {
                  value = xml_ser;
                  FreeResource(xml_ser);
               }
            }

            if (!value.empty()) {
               auto &option = widget.menu.m_items.emplace_back(value);

               auto id = scan.attrib("id");
               if ((id) and (!id->empty())) option.id = *id;

               auto val = scan.attrib("value");
               if ((val) and (!val->empty())) option.value = *val;

               auto icon = scan.attrib("icon");
               if (icon) option.icon = *icon;
            }
         }
      }
   }

   if (!m_combobox_patterns) {
      // The combobox uses the default fill pattern with an arrow button overlayed on the right.
      m_combobox_patterns = true;

      if (auto pattern_cb = objVectorPattern::create::global({
            fl::Name("combobox"),
            fl::SpreadMethod(VSPREAD::CLIP)
         })) {

         const LONG PAD = 8;
         auto vp = pattern_cb->Scene->Viewport;
         auto rect = objVectorRectangle::create::global({ // Button background
            fl::Owner(vp->UID),
            fl::X(-(PAD-1)), fl::Y(-(PAD-1)), fl::Width(29+((PAD-1)*2)), fl::Height(29+((PAD-1)*2)),
            fl::Fill("rgb(0,0,0,128)")
         });

         std::array<DOUBLE, 8> round = { 0, 0, 6, 6, 6, 6, 0, 0 };
         SetArray(rect, FID_Rounding|TDOUBLE, round);

         objVectorPath::create::global({ // Down arrow
            fl::Owner(vp->UID),
            fl::Sequence("M14.5 16.1 26.1 4.5 26.1 12.9 14.5 24.5 2.9 12.9 2.9 4.5 14.5 16.1Z"), // 80% size
            //fl::Sequence("M14.5 16.5 29 2 29 12.5 14.5 27 0 12.5 0 2 14.5 16.5Z"), // Original size; 29x29
            fl::Fill("rgb(255,255,255,220)")
         });

         vp->setFields(fl::AspectRatio(ARF::X_MAX|ARF::Y_MIN|ARF::MEET),
            fl::ViewX(-PAD), fl::ViewY(-PAD),
            fl::ViewWidth(29+(PAD*2)), fl::ViewHeight(29+(PAD*2)));

         Self->Viewport->Scene->addDef("/widget/combobox", pattern_cb);
      }
   }

   if (widget.fill.empty()) {
      config_default_pattern();
      widget.fill = "url(#/widget/default);url(#/widget/combobox)";
   }

   if (widget.font_fill.empty()) widget.font_fill = "rgb(255,255,255)";

   widget.def_size  = DUNIT(1.7, DU::FONT_SIZE);
   widget.label_pad = m_style.get_font()->metrics.Ascent * 0.5;

   Self->NoWhitespace = false; // Widgets are treated as inline characters
}

//********************************************************************************************************************

void parser::tag_input(XMLTag &Tag)
{
   pf::Log log(__FUNCTION__);

   bc_input &widget = m_stream->emplace<bc_input>(m_index);

   for (LONG i=1; i < std::ssize(Tag.Attribs); i++) {
      auto &value = Tag.Attribs[i].Value;
      switch (strihash(Tag.Attribs[i].Name)) {
         case HASH_label:     widget.label = value; break;
         case HASH_value:     widget.value = value; break;
         case HASH_fill:      widget.fill  = value; break;
         case HASH_width:     widget.width = DUNIT(value); break;
         case HASH_font_fill: widget.font_fill = value; break;
         case HASH_name:      widget.name = value; break;
         case HASH_label_pos:
            if (iequals("left", value)) widget.label_pos = 0;
            else if (iequals("right", value)) widget.label_pos = 1;
            break;
         default:
            log.warning("<input> unsupported attribute '%s'", Tag.Attribs[i].Name.c_str());
            break;
      }
   }

   if (widget.fill.empty()) {
      config_default_pattern();
      widget.fill = "url(#/widget/default)";
   }

   if (widget.font_fill.empty()) widget.font_fill = "rgb(255,255,255)";

   widget.def_size  = DUNIT(1.7, DU::FONT_SIZE);
   widget.label_pad = m_style.get_font()->metrics.Ascent * 0.5;

   Self->NoWhitespace = false; // Widgets are treated as inline characters
}

//********************************************************************************************************************

void parser::tag_debug(XMLTag &Tag)
{
   pf::Log log("DocMsg");
   for (LONG i=1; i < std::ssize(Tag.Attribs); i++) {
      if (iequals("msg", Tag.Attribs[i].Name)) log.warning("%s", Tag.Attribs[i].Value.c_str());
   }
}

//********************************************************************************************************************
// Declaring <svg> anywhere can execute an SVG statement of any kind, with the caveat that it will target the
// Page viewport.  This feature should only be used for the creation of resources that can then be referred to in the
// document as named patterns, or via the 'use' option for symbols.
//
// This tag can only be used ONCE per document.  Potentially we could improve this by appending to the existing
// SVG object via data feeds.

void parser::tag_svg(XMLTag &Tag)
{
   pf::Log log(__FUNCTION__);

   if (Self->SVG) {
      Self->Error = ERR::AlreadyDefined;
      log.warning("Illegal attempt to declare <svg/> more than once.");
      return;
   }

   objVectorViewport *target = Self->Page;
   for (LONG i=1; i < std::ssize(Tag.Attribs); i++) {
      if (iequals("placement", Tag.Attribs[i].Name)) {
         if (iequals("foreground", Tag.Attribs[i].Value)) target = Self->Page;
         else if (iequals("background", Tag.Attribs[i].Value)) target = Self->View;
         Tag.Attribs.erase(Tag.Attribs.begin() + i);
         i--;
      }
   }

   STRING xml_svg;
   if (auto err = m_xml->serialise(Tag.ID, XMF::NIL, &xml_svg); err IS ERR::Okay) {
      if ((Self->SVG = objSVG::create::local({ fl::Statement(xml_svg), fl::Target(target) }))) {
         if (target IS Self->View) { // Put the page back in front of the background objects
            acMoveToFront(Self->Page);
         }
      }
      else Self->Error = ERR::CreateObject;
   }
}

//********************************************************************************************************************
// The <use> tag allows SVG <symbol> declarations to be injected into the parent viewport (e.g. table cells).
// SVG objects that are created in this way are treated as dynamically rendered background graphics.  All text will
// be laid on top with no clipping considerations.
//
// If more sophisticated inline or float embedding is required, the <image> tag is probably more applicable to the
// client.

void parser::tag_use(XMLTag &Tag)
{
   std::string id;
   for (LONG i = 1; i < std::ssize(Tag.Attribs); i++) {
      if (iequals("href", Tag.Attribs[i].Name)) {
         id = Tag.Attribs[i].Value;
      }
   }

   if (!id.empty()) {
      auto &use = m_stream->emplace<bc_use>(m_index);
      use.id = id;
   }
}

//********************************************************************************************************************
// Use div to structure the document in a similar way to paragraphs.  The main difference is that it impacts on style
// attributes only, avoiding the declaration of paragraph start and end points and won't cause line breaks.

void parser::tag_div(XMLTag &Tag)
{
   pf::Log log(__FUNCTION__);

   auto new_style = m_style;
   for (LONG i=1; i < std::ssize(Tag.Attribs); i++) {
      if (iequals("align", Tag.Attribs[i].Name)) {
         if ((iequals(Tag.Attribs[i].Value, "center")) or
             (iequals(Tag.Attribs[i].Value, "middle"))) {
            new_style.options |= FSO::ALIGN_CENTER;
         }
         else if (iequals(Tag.Attribs[i].Value, "right")) {
            new_style.options |= FSO::ALIGN_RIGHT;
         }
         else log.warning("Alignment type '%s' not supported.", Tag.Attribs[i].Value.c_str());
      }
      else check_para_attrib(Tag.Attribs[i], 0, new_style);
   }

   parse_tags_with_style(Tag.Children, new_style);
}

//********************************************************************************************************************
// Creates a new edit definition.  These are stored in a linked list.  Edit definitions are used by referring to them
// by name in table cells.

void parser::tag_editdef(XMLTag &Tag)
{
   pf::Log log(__FUNCTION__);

   doc_edit edit;
   std::string name;

   for (LONG i=1; i < std::ssize(Tag.Attribs); i++) {
      switch (strihash(Tag.Attribs[i].Name)) {
         case HASH_max_chars:
            edit.max_chars = std::stoi(Tag.Attribs[i].Value);
            if (edit.max_chars < 0) edit.max_chars = -1;
            break;

         case HASH_name: name = Tag.Attribs[i].Value; break;

         case HASH_select_fill: break;

         case HASH_line_breaks: edit.line_breaks = std::stoi(Tag.Attribs[i].Value); break;

         case HASH_edit_fonts:
         case HASH_edit_images:
         case HASH_edit_tables:
         case HASH_edit_all:
            break;

         case HASH_on_change:
            if (!Tag.Attribs[i].Value.empty()) edit.on_change = Tag.Attribs[i].Value;
            break;

         case HASH_on_exit:
            if (!Tag.Attribs[i].Value.empty()) edit.on_exit = Tag.Attribs[i].Value;
            break;

         case HASH_on_enter:
            if (!Tag.Attribs[i].Value.empty()) edit.on_enter = Tag.Attribs[i].Value;
            break;

         default:
            if (Tag.Attribs[i].Name[0] IS '@') {
               edit.args.emplace_back(make_pair(Tag.Attribs[i].Name, Tag.Attribs[i].Value));
            }
            else if (Tag.Attribs[i].Name[0] IS '_') {
               edit.args.emplace_back(make_pair(Tag.Attribs[i].Name, Tag.Attribs[i].Value));
            }
      }
   }

   if (!name.empty()) Self->EditDefs[name] = std::move(edit);
}

//********************************************************************************************************************
// Use of <meta> for custom information is allowed and is ignored by the parser.

void parser::tag_head(XMLTag &Tag)
{
   // The head contains information about the document

   for (auto &scan : Tag.Children) {
      // Anything allocated here needs to be freed in unload_doc()
      if (iequals("title", scan.name())) {
         if (scan.hasContent()) {
            if (Self->Title) FreeResource(Self->Title);
            Self->Title = pf::strclone(scan.Children[0].Attribs[0].Value);
         }
      }
      else if (iequals("author", scan.name())) {
         if (scan.hasContent()) {
            if (Self->Author) FreeResource(Self->Author);
            Self->Author = pf::strclone(scan.Children[0].Attribs[0].Value);
         }
      }
      else if (iequals("copyright", scan.name())) {
         if (scan.hasContent()) {
            if (Self->Copyright) FreeResource(Self->Copyright);
            Self->Copyright = pf::strclone(scan.Children[0].Attribs[0].Value);
         }
      }
      else if (iequals("keywords", scan.name())) {
         if (scan.hasContent()) {
            if (Self->Keywords) FreeResource(Self->Keywords);
            Self->Keywords = pf::strclone(scan.Children[0].Attribs[0].Value);
         }
      }
      else if (iequals("description", scan.name())) {
         if (scan.hasContent()) {
            if (Self->Description) FreeResource(Self->Description);
            Self->Description = pf::strclone(scan.Children[0].Attribs[0].Value);
         }
      }
   }
}

//********************************************************************************************************************
// Include XML from another RIPL file.

void parser::tag_include(XMLTag &Tag)
{
   pf::Log log(__FUNCTION__);

   for (LONG i=1; i < std::ssize(Tag.Attribs); i++) {
      if (iequals("src", Tag.Attribs[i].Name)) {
         if (auto xmlinc = objXML::create::local(fl::Path(Tag.Attribs[i].Value), fl::Flags(XMF::PARSE_HTML|XMF::STRIP_HEADERS))) {
            auto old_xml = change_xml(xmlinc);
            parse_tags(xmlinc->Tags);
            Self->Resources.emplace_back(xmlinc->UID, RTD::OBJECT_TEMP);
            change_xml(old_xml);
         }
         else log.warning("Failed to include '%s'", Tag.Attribs[i].Value.c_str());
      }
      else if (iequals("volatile", Tag.Attribs[i].Name)) {
         // Instruct the cache manager that it should always check if the source requires reloading, irrespective of the
         // amount of time that has passed since the last load.
      }
   }

   log.warning("<include> directive missing required 'src' element.");
}

//********************************************************************************************************************
// Parse a string value as XML

void parser::tag_parse(XMLTag &Tag)
{
   pf::Log log(__FUNCTION__);

   // The value attribute will contain XML.  We will parse the XML as if it were part of the document source.  This feature
   // is typically used when pulling XML information out of an object field.

   if (std::ssize(Tag.Attribs) > 1) {
      if ((iequals("value", Tag.Attribs[1].Name)) or
          (iequals("$value", Tag.Attribs[1].Name))) {
         log.traceBranch("Parsing string value as XML...");

         if (auto xmlinc = objXML::create::local(fl::Statement(Tag.Attribs[1].Value), fl::Flags(XMF::PARSE_HTML|XMF::STRIP_HEADERS))) {
            auto old_xml = change_xml(xmlinc);
            parse_tags(xmlinc->Tags);
            change_xml(old_xml);

            // Add the created XML object to the document rather than destroying it

            Self->Resources.emplace_back(xmlinc->UID, RTD::OBJECT_TEMP);
         }
      }
   }
}

//********************************************************************************************************************
// Bitmap and vector images are supported as vector rectangles that reference a pattern name.  Images need to be
// loaded as resources in an <svg> tag and can then be referenced by name.  Technically any pattern type can be
// referenced as an image - so if the client wants to refer to a gradient for example, that is perfectly legal.
//
// Images are inline by default.  Whitespace on either side is never blocked, whether inline or floating.
// Blocking whitespace can be achieved by embedding the image within <p> tags.
//
// A benefit to rendering SVG images in the <defs> area is that they are converted to cached bitmap textures ahead of
// time.  This provides a considerable speed boost when drawing them, at a potential cost to image quality.

void parser::tag_image(XMLTag &Tag)
{
   pf::Log log(__FUNCTION__);

   bc_image img;
   img.def_size = DUNIT(0.9, DU::FONT_SIZE);

   for (LONG i=1; i < std::ssize(Tag.Attribs); i++) {
      auto &value = Tag.Attribs[i].Value;

      switch (strihash(Tag.Attribs[i].Name)) {
         case HASH_float:
         case HASH_align:
            // Setting the horizontal alignment of an image will cause it to float above the text.
            // If the image is declared inside a paragraph, it will be completely de-anchored as a result.
            switch (strihash(value)) {
               case HASH_left:   img.align = ALIGN::LEFT; break;
               case HASH_right:  img.align = ALIGN::RIGHT; break;
               case HASH_center: img.align = ALIGN::CENTER; break;
               case HASH_middle: img.align = ALIGN::CENTER; break; // synonym
               default:
                  log.warning("Invalid alignment value '%s'", value.c_str());
                  break;
            }
            break;

         case HASH_v_align:
            // If the image is anchored and the line is taller than the image, the image can be vertically aligned.
            switch(strihash(value)) {
               case HASH_top:    img.align = ALIGN::TOP; break;
               case HASH_center: img.align = ALIGN::VERTICAL; break;
               case HASH_middle: img.align = ALIGN::VERTICAL; break; // synonym
               case HASH_bottom: img.align = ALIGN::BOTTOM; break;
               default:
                  log.warning("Invalid valign value '%s'", value.c_str());
                  break;
            }
            break;

         case HASH_padding: img.pad.parse(value); break;
         case HASH_fill:    img.fill = value; break;
         case HASH_src:     img.fill = value; break;
         case HASH_width:   img.width  = DUNIT(value); break;
         case HASH_height:  img.height = DUNIT(value); break;

         default:
            log.warning("<image> unsupported attribute '%s'", Tag.Attribs[i].Name.c_str());
      }
   }

   if (!img.fill.empty()) {
      if (img.width.value <= 0) img.width.clear(); // Zero is equivalent to 'auto', meaning on-the-fly computation
      if (img.height.value <= 0) img.height.clear();

      if (!img.floating_x()) Self->NoWhitespace = false; // Images count as characters when inline.
      m_stream->emplace(m_index, img);
   }
   else {
      log.warning("No src defined for <image> tag.");
      return;
   }
}

//********************************************************************************************************************
// Indexes set bookmarks that can be used for quick-scrolling to document sections.  They can also be used to mark
// sections of content that may require run-time modification.
//
// <index name="News">
//   <p>Something in here.</p>
// </index>
//
// If the name attribute is not specified, an attempt will be made to derive the name from the first immediate string
// of the index' content, e.g:
//
//   <index>News</>
//
// The developer can use indexes to bookmark areas of code that are of interest.  The FindIndex() method is used for
// this purpose.

void parser::tag_index(XMLTag &Tag)
{
   pf::Log log(__FUNCTION__);

   ULONG name = 0;
   bool visible = true;
   for (LONG i=1; i < std::ssize(Tag.Attribs); i++) {
      if (iequals("name", Tag.Attribs[i].Name)) {
         name = strihash(Tag.Attribs[i].Value);
      }
      else if (iequals("hide", Tag.Attribs[i].Name)) {
         visible = false;
      }
      else log.warning("<index> unsupported attribute '%s'", Tag.Attribs[i].Name.c_str());
   }

   if ((!name) and (!Tag.Children.empty())) {
      if (Tag.Children[0].isContent()) name = strihash(Tag.Children[0].Attribs[0].Value);
   }

   if (name) {
      bc_index index(name, glUID++, 0, visible, Self->Invisible ? false : true);

      auto &stream_index = m_stream->emplace(m_index, index);

      if (!Tag.Children.empty()) {
         if (!visible) Self->Invisible++;
         parse_tags(Tag.Children);
         if (!visible) Self->Invisible--;
      }

      bc_index_end end(stream_index.id);
      m_stream->emplace(m_index, end);
   }
   else if (!Tag.Children.empty()) parse_tags(Tag.Children);
}

//********************************************************************************************************************
// If calling a function with 'onclick', all arguments must be identified with the @ prefix.  Parameters will be
// passed to the function in the order in which they are given.  Global values can be set against the document
// object itself, if a parameter is prefixed with an underscore.
//
// Script objects can be specifically referenced when calling a function, e.g. "myscript.function".  If no script
// object is referenced, then it is assumed that the default script contains the function.
//
// <a href="http://" onclick="function" fill="rgb" @arg1="" @arg2="" _global=""/>
//
// Dummy links that specify neither an href or onclick value can be useful in embedded documents if the
// EventCallback feature is used.

void parser::tag_link(XMLTag &Tag)
{
   pf::Log log(__FUNCTION__);

   bc_link link;
   bool select = false;
   link.fill = Self->LinkFill;

   for (LONG i=1; i < std::ssize(Tag.Attribs); i++) {
      switch (strihash(Tag.Attribs[i].Name)) {
         case HASH_href:
            if (link.type IS LINK::NIL) {
               link.ref = Tag.Attribs[i].Value;
               link.type = LINK::HREF;
            }
            break;

         case HASH_title: // 'title' is the http equivalent of our 'hint'
            [[fallthrough]];
         case HASH_hint:
            link.hint = Tag.Attribs[i].Value;
            break;

         case HASH_on_click:
            if (link.type IS LINK::NIL) { // Function to execute on click
               link.ref = Tag.Attribs[i].Value;
               link.type = LINK::FUNCTION;
            }
            break;

         case HASH_on_motion: // Function to execute on cursor motion
            link.hooks.on_motion = Tag.Attribs[i].Value;
            break;

         case HASH_on_crossing: // Function to execute on cursor crossing in/out
            link.hooks.on_crossing = Tag.Attribs[i].Value;
            break;

         case HASH_fill: link.fill = Tag.Attribs[i].Value; break;

         case HASH_select: select = true; break;

         default:
            if (Tag.Attribs[i].Name.starts_with('@')) link.args.push_back(make_pair(Tag.Attribs[i].Name, Tag.Attribs[i].Value));
            else if (Tag.Attribs[i].Name.starts_with('_')) link.args.push_back(make_pair(Tag.Attribs[i].Name, Tag.Attribs[i].Value));
            else log.warning("<a|link> unsupported attribute '%s'", Tag.Attribs[i].Name.c_str());
      }
   }

   if ((link.type != LINK::NIL) or (!Tag.Children.empty())) {
      // Font modifications are saved with the link as opposed to inserting a new bc_font as it's a lot cleaner
      // this way - especially for run-time modifications.

      link.font = bc_font(m_style);
      link.font.options |= FSO::UNDERLINE;
      link.font.fill = link.fill;

      auto &stream_link = m_stream->emplace(m_index, link);

      parse_tags_with_embedded_style(Tag.Children, stream_link.font);

      m_stream->emplace<bc_link_end>(m_index);

      // Links are added to the list of tab locations

      auto i = add_tabfocus(Self, TT::LINK, stream_link.uid);
      if (select) Self->FocusIndex = i;
   }
   else parse_tags(Tag.Children);
}

//********************************************************************************************************************

void parser::tag_list(XMLTag &Tag)
{
   pf::Log log(__FUNCTION__);
   bc_list list;

   list.fill     = m_style.fill; // Default fill matches the current font colour
   list.item_num = list.start;

   for (LONG i=1; i < std::ssize(Tag.Attribs); i++) {
      auto &name  = Tag.Attribs[i].Name;
      auto &value = Tag.Attribs[i].Value;
      if (iequals("fill", name)) {
         list.fill = value;
      }
      else if (iequals("indent", name)) {
         // Affects the indenting to apply to child items.
         list.block_indent = DUNIT(value, DU::PIXEL);
      }
      else if (iequals("v-spacing", name)) {
         // Affects the vertical advance from one list-item paragraph to the next.
         // Equivalent to paragraph leading, not v-spacing, which affects each line
         list.v_spacing = DUNIT(value, DU::LINE_HEIGHT);
         if (list.v_spacing.value < 0) list.v_spacing.clear();
      }
      else if (iequals("type", name)) {
         if (iequals("bullet", value)) {
            list.type = bc_list::BULLET;
         }
         else if (iequals("ordered", value)) {
            list.type = bc_list::ORDERED;
            list.item_indent.clear();
         }
         else if (iequals("custom", value)) {
            list.type = bc_list::CUSTOM;
            list.item_indent.clear();
         }
      }
      else log.msg("Unknown list attribute '%s'", name.c_str());
   }

   auto &stream_list = m_stream->emplace(m_index, list);
   m_list_stack.push(&stream_list);

      // Refer to tag_li() to see how list items are managed

      if (!Tag.Children.empty()) parse_tags(Tag.Children);

   m_list_stack.pop();
   m_stream->emplace<bc_list_end>(m_index);

   Self->NoWhitespace = true;
}

//********************************************************************************************************************
// Also see check_para_attrib() for paragraph attributes.

void parser::tag_paragraph(XMLTag &Tag)
{
   pf::Log log(__FUNCTION__);

   m_paragraph_depth++;

   bc_paragraph para(m_style);

   for (LONG i=1; i < std::ssize(Tag.Attribs); i++) {
      if (iequals("align", Tag.Attribs[i].Name)) {
         if ((iequals(Tag.Attribs[i].Value, "center")) or
             (iequals(Tag.Attribs[i].Value, "middle"))) {
            para.font.options |= FSO::ALIGN_CENTER;
         }
         else if (iequals(Tag.Attribs[i].Value, "right")) {
            para.font.options |= FSO::ALIGN_RIGHT;
         }
         else log.warning("Alignment type '%s' not supported.", Tag.Attribs[i].Value.c_str());
      }
      else if (iequals("leading", Tag.Attribs[i].Name)) {
         // The leading is a line height multiplier that applies to the first line in the paragraph only.
         // It is typically used for things like headers.

         para.leading = DUNIT(Tag.Attribs[i].Value, DU::LINE_HEIGHT, DBL_MIN);
      }
      else if (check_para_attrib(Tag.Attribs[i], &para, para.font));
      else check_font_attrib(Tag.Attribs[i], para.font);
   }

   auto &stream_para = m_stream->emplace(m_index, para);

   Self->NoWhitespace = stream_para.trim;

   parse_tags_with_embedded_style(Tag.Children, stream_para.font);

   bc_paragraph_end end;
   m_stream->emplace(m_index, end);
   Self->NoWhitespace = true;
   m_paragraph_depth--;
}

//********************************************************************************************************************

void parser::tag_print(XMLTag &Tag)
{
   pf::Log log(__FUNCTION__);

   // Copy the content from the value attribute into the document stream.  If used inside an object, the data is sent
   // to that object as XML.

   if (Tag.Attribs.size() > 1) {
      auto tagname = Tag.Attribs[1].Name.c_str();
      if (*tagname IS '$') tagname++;

      if (iequals("value", tagname)) {
         insert_text(Self, m_stream, m_index, Tag.Attribs[1].Value, (m_style.options & FSO::PREFORMAT) != FSO::NIL);
      }
      else if (iequals("src", Tag.Attribs[1].Name)) {
         // This option is only supported in unrestricted mode
         if ((Self->Flags & DCF::UNRESTRICTED) != DCF::NIL) {
            CacheFile *cache;
            if (LoadFile(Tag.Attribs[1].Value.c_str(), LDF::NIL, &cache) IS ERR::Okay) {
               insert_text(Self, m_stream, m_index, std::string((CSTRING)cache->Data), (m_style.options & FSO::PREFORMAT) != FSO::NIL);
               UnloadFile(cache);
            }
         }
         else log.warning("Cannot <print src.../> unless in unrestricted mode.");
      }
   }
}

//********************************************************************************************************************
// Templates can be used to create custom tags.

void parser::tag_template(XMLTag &Tag)
{
   pf::Log log(__FUNCTION__);

   if (m_in_template) return;

   // Validate the template (must have a name)

   LONG n;
   for (n=1; n < std::ssize(Tag.Attribs); n++) {
      if ((iequals("name", Tag.Attribs[n].Name)) and (!Tag.Attribs[n].Value.empty())) break;
   }

   if (n >= std::ssize(Tag.Attribs)) {
      log.warning("A <template> is missing a name attribute.");
      return;
   }

   STRING strxml;
   if (m_xml->serialise(Tag.ID, XMF::NIL, &strxml) IS ERR::Okay) {
      // Remove any existing tag that uses the same name.
      if (Self->TemplateIndex.contains(strihash(Tag.Attribs[n].Value))) {
         Self->Templates->removeTag(Tag.ID, 1);
      }

      Self->Templates->insertXML(Self->Templates->Tags[0].ID, XMI::END, strxml, 0);
      FreeResource(strxml);

      Self->RefreshTemplates = true; // Force a refresh of the TemplateIndex because the pointers will be changed
   }
   else log.warning("Failed to convert template %d to an XML string.", Tag.ID);
}

//********************************************************************************************************************

ERR parser::calc(const std::string &String, DOUBLE *Result, std::string &Output)
{
   enum SIGN { PLUS=1, MINUS, MULTIPLY, DIVIDE, MODULO };

   if (Result) *Result = 0;

   Output.clear();

   // Search for brackets and translate them first

   std::string in(String);
   while (true) {
      // Find the last bracketed reference

      LONG last_bracket = 0;
      for (unsigned i=0; i < in.size(); i++) {
         if (in[i] IS '\'') { // Skip anything in quotes
            i++;
            while (in[i]) {
               if (in[i] IS '\\') {
                  i++; // Skip backslashes and the following character
                  if (!in[i]) break;
               }
               else if (in[i] IS '\'') break;
               i++;
            }
            if (in[i] IS '\'') i++;
         }
         else if (in[i] IS '(') last_bracket = i;
      }

      if (last_bracket > 0) { // Bracket found, translate its contents
         LONG end;
         for (end=last_bracket+1; (in[end]) and (in[end-1] != ')'); end++);
         std::string buf(in, last_bracket, end - last_bracket);

         DOUBLE calc_float;
         std::string out;
         calc(buf.c_str()+1, &calc_float, out);
         in.replace(last_bracket, end - last_bracket, out);
      }
      else break;
   }

   // Perform the calculation

   STRING end;
   WORD precision = 9;
   DOUBLE total   = 0;
   DOUBLE overall = 0;
   LONG index     = 0;
   SIGN sign      = PLUS;
   bool number    = false;
   for (unsigned s=0; in[s];) {
      if (unsigned(in[s]) <= 0x20); // Do nothing with whitespace
      else if (in[s] IS '\'') {
         if (number) { // Write the current floating point number to the buffer before the next calculation
            Output  += write_calc(total, precision);
            overall += total; // Reset the number
            total   = 0;
            number  = false;
         }

         s++;
         while (index < LONG(Output.size())-1) {
            if (in[s] IS '\\') s++; // Skip the \ character and continue so that we can copy the character immediately after it
            else if (in[s] IS '\'') break;

            Output += in[s++];
         }
      }
      else if (in[s] IS 'f') { // Fixed floating point precision adjustment
         s++;
         precision = -strtol(in.c_str() + s, &end, 10);
         s += end - in.c_str();
         continue;
      }
      else if (in[s] IS 'p') { // Floating point precision adjustment
         s++;
         precision = strtol(in.c_str() + s, &end, 10);
         s += end - in.c_str();
         continue;
      }
      else if ((in[s] >= '0') and (in[s] <= '9')) {
         number = true;
         DOUBLE fvalue = strtod(in.c_str() + s, &end);
         s += end - in.c_str();

         if (sign IS MINUS)         total = total - fvalue;
         else if (sign IS MULTIPLY) total = total * fvalue;
         else if (sign IS MODULO)   total = F2I(total) % F2I(fvalue);
         else if (sign IS DIVIDE) {
            if (fvalue) total = total / fvalue; // NB: Avoid division by zero errors
         }
         else total += fvalue;

         sign = PLUS; // The mathematical sign is reset whenever a number is encountered
         continue;
      }
      else if (in[s] IS '-') {
         if (sign IS MINUS) sign = PLUS; // Handle double-negatives
         else sign = MINUS;
      }
      else if (in[s] IS '+') sign = PLUS;
      else if (in[s] IS '*') sign = MULTIPLY;
      else if (in[s] IS '/') sign = DIVIDE;
      else if (in[s] IS '%') sign = MODULO;

      for (++s; (in[s] & 0xc0) IS 0x80; s++);
   }

   if (number) Output += write_calc(total, precision);
   if (Result) *Result = overall + total;
   return ERR::Okay;
}

/*********************************************************************************************************************

This function is used to translate strings that make object and field references using the standard referencing format.
References are made to objects by enclosing statements within square brackets.  As a result of calling this function,
all references within the Buffer will be translated to their relevant format.  The Buffer needs to be large enough to
accommodate these adjustments as it will be expanded during the translation.  It is recommended that the Buffer is at
least two times the actual length of the string that you are translating.

Valid references can be made to an object by name, ID or relative parameters.  Here are some examples illustrating the
different variations:

<types type="Reference">
<type name="[surface]">Name reference.</>
<type name="[#49302]">ID reference.</>
<type name="[self]">Relative reference to the object that has the current context, or the document.</>
</table>

Field references are a slightly different matter and will be converted to the value of the field that they are
referencing.  A field reference is defined using the object referencing format, but they contain a `.fieldname`
extension.  Here are some examples:

<pre>
[surface.width]
[file.location]
</pre>

A string such as `[mywindow.height] + [mywindow.width]` could be translated to `255 + 120` for instance.

Simple calculations are possible by enclosing a statement within a `[=...]` section.  For example the aforementioned
string can be expanded to `[=[mywindow.height] + [mywindow.width]]`, which would give a result of 375.

The escape character for string translation is `$` and should be used as `[$...]`, which prevents everything within the
square brackets from being translated.  The `[$]` characters will be removed as part of this process unless the
KEEP_ESCAPE flag is used.  To escape a single right or left bracket, use `[rb]` or `[lb]` respectively.

*********************************************************************************************************************/

// Evaluate object references and calculations

ERR parser::tag_xml_content_eval(std::string &Buffer)
{
   pf::Log log(__FUNCTION__);
   LONG i;

   // Quick check for translation symbols

   if (Buffer.find('[') IS std::string::npos) return ERR::EmptyString;

   log.traceBranch("%.80s", Buffer.c_str());

   ERR error = ERR::Okay;
   ERR majorerror = ERR::Okay;

   // Skip to the end of the buffer (translation occurs 'backwards')

   auto pos = std::ssize(Buffer) - 1;
   while (pos >= 0) {
      if ((Buffer[pos] IS '[') and ((Buffer[pos+1] IS '@') or (Buffer[pos+1] IS '%'))) {
         // Ignore arguments, e.g. [@id] or [%id].  It's also useful for ignoring [@attrib] in xpath.
         pos--;
      }
      else if (Buffer[pos] IS '[') {
         // Make sure that there is a balanced closing bracket

         LONG end;
         LONG balance = 0;
         for (end=pos; Buffer[end]; end++) {
            if (Buffer[end] IS '[') balance++;
            else if (Buffer[end] IS ']') {
               balance--;
               if (!balance) break;
            }
         }

         if (Buffer[end] != ']') {
            log.warning("Unbalanced string: %.90s ...", Buffer.c_str());
            return ERR::InvalidData;
         }

         if (Buffer[pos+1] IS '=') { // Perform a calculation
            std::string num;
            num.assign(Buffer, pos+2, end-(pos+2));

            std::string calcbuffer;
            DOUBLE value;
            calc(num, &value, calcbuffer);
            Buffer.insert(end-pos+1, calcbuffer);
         }
         else if (Buffer[pos+1] IS '$') { // Escape sequence - e.g. translates [$ABC] to ABC.  Note: Use [rb] and [lb] instead for brackets.
            Buffer.erase(end, 1); // ']'
            Buffer.erase(pos, 2); // '[$'
            pos--;
            continue;
         }
         else {
            std::string name;
            name.reserve(64);

            for (i=pos+1; (Buffer[i] != '.') and (i < end); i++) {
               name += std::tolower(Buffer[i]);
            }

            // Check for [lb] and [rb] escape codes

            char code = 0;
            if (name == "rb") code = ']';
            else if (name == "lb") code = '[';

            if (code) {
               Buffer[pos] = code;
               Buffer.erase(pos+1, 3);
               pos--;
               continue;
            }
            else {
               OBJECTID objectid = 0;
               if (iequals(name, "self")) objectid = CurrentContext()->UID;
               else FindObject(name.c_str(), CLASSID::NIL, FOF::SMART_NAMES, &objectid);

               if (objectid) {
                  OBJECTPTR object = NULL;
                  if (Buffer[i] IS '.') {
                     // Get the field from the object
                     i++;

                     std::string field(Buffer, i, end);
                     if (AccessObject(objectid, 2000, &object) IS ERR::Okay) {
                        OBJECTPTR target;
                        const Field *classfield;
                        if (((classfield = find_field(object, field.c_str(), &target))) and (classfield->Flags & FD_STRING)) {
                           CSTRING str;
                           if (GetField(object, (FIELD)classfield->FieldID|TSTR, &str) IS ERR::Okay) {
                              Buffer.insert(end-pos+1, str);
                           }
                        }
                        else { // Get field as an unlisted type and manage any buffer overflow
                           std::string tbuffer;
                           tbuffer.reserve(4096);
repeat:
                           tbuffer[tbuffer.capacity()-1] = 0;
                           if (GetFieldVariable(object, field.c_str(), tbuffer.data(), tbuffer.capacity()) IS ERR::Okay) {
                              if (tbuffer[tbuffer.capacity()-1]) {
                                 tbuffer.reserve(tbuffer.capacity() * 2);
                                 goto repeat;
                              }
                              Buffer.insert(end-pos+1, tbuffer);
                           }
                        }
                        // NB: For fields, error code is always Okay so that the reference evaluates to NULL

                        ReleaseObject(object);
                     }
                     else error = ERR::AccessObject;
                  }
                  else { // Convert the object reference to an ID
                     Buffer.insert(end-pos+1, std::move(std::string("#") + std::to_string(objectid)));
                  }
               }
               else {
                  error = ERR::NoMatchingObject;
                  log.traceWarning("Failed to find object '%s'", name.c_str());
               }
            }
         }

         if (error != ERR::Okay) {
            pos--;
            majorerror = error;
            error = ERR::Okay;
         }
      }
      else pos--;
   }

   log.trace("Result: %s", Buffer.c_str());

   return majorerror;
}

//********************************************************************************************************************

void parser::tag_font(XMLTag &Tag)
{
   pf::Log log(__FUNCTION__);

   auto new_style = m_style;
   bool preformat = false;

   for (LONG i=1; i < std::ssize(Tag.Attribs); i++) {
      if (iequals("preformat", Tag.Attribs[i].Name)) {
         new_style.options |= FSO::PREFORMAT;
         preformat = true;
         m_strip_feeds = true;
      }
      else check_font_attrib(Tag.Attribs[i], new_style);
   }

   parse_tags_with_style(Tag.Children, new_style);

   if (preformat) trim_preformat(Self);
}

//********************************************************************************************************************

void parser::tag_object(XMLTag &Tag)
{
   /*
   pf::Log log(__FUNCTION__);

   // NF::LOCAL is only set when the object is owned by the document

   OBJECTPTR object;
   if (NewObject(class_id, (Self->CurrentObject) ? NF::NIL : NF::LOCAL, (OBJECTPTR *)&object)) {
      log.warning("Failed to create object of class #%d.", class_id);
      return;
   }

   log.branch("Processing %s object from document tag, owner #%d.", object->Class->ClassName, Self->CurrentObject ? Self->CurrentObject->UID : -1);

   // Setup the callback interception so that we can control the order in which objects draw their graphics to the surface.

   if (Self->CurrentObject) {
      SetOwner(object, Self->CurrentObject);
   }
   else if (!pagetarget.empty()) {
      auto field_id = strihash(pagetarget);
      if (Self->BkgdGfx) object->set(field_id, Self->View);
      else object->set(field_id, Self->Page);
   }

   for (unsigned i=1; i < Tag.Attribs.size(); i++) {
      auto argname = Tag.Attribs[i].Name.c_str();
      while (*argname IS '$') argname++;
      if (Tag.Attribs[i].Value.empty()) object->set(strihash(argname), "1");
      else object->set(strihash(argname), Tag.Attribs[i].Value);
   }

   // Check for the 'data' tag which can be used to send data feed information prior to initialisation.
   //
   // <data type="text">Content</data>
   // <data type="xml" template="TemplateName"/>
   // <data type="xml" object="[xmlobj]"/>
   // <data type="xml">Content</data>

   bool customised = false;

   if (!Tag.Children.empty()) {
      STRING src;

      for (auto &scan : Tag.Children) {
         if (!iequals("data", scan.Attribs[0].name)) continue;

         if (*e_revert > *s_revert) {
            while (*e_revert > *s_revert) {
               *e_revert -= 1;
               Self->RestoreAttrib[*e_revert].Attrib[0] = Self->RestoreAttrib[*e_revert].String;
            }
         }
         Self->RestoreAttrib.resize(*s_revert);

         *s_revert = Self->RestoreAttrib.size();
         *e_revert = 0;
         translate_attrib_args(scan.Attribs);
         *e_revert = Self->RestoreAttrib.size();

         const std::string *type = scan.attrib("type");

         if ((!type) or (iequals("text", type))) {
            if (scan.Children.empty()) continue;
            std::string buffer;
            xmlGetContent(scan, buffer);
            if (!buffer.empty()) acDataText(object, buffer.c_str());
         }
         else if (iequals("xml", type)) {
            customised = true;

            if (auto t = scan.attrib("template")) {
               for (auto &tmp : Self->Templates->Tags) {
                  for (unsigned i=1; i < tmp.Attribs.size(); i++) {
                     if ((iequals("name", tmp.Attribs[i].name)) and (iequals(t, tmp.Attribs[i].Value))) {
                        if (!xmlSerialise(Self->Templates, tmp.Children[0].ID, XMF::INCLUDE_SIBLINGS|XMF::STRIP_CDATA, &content)) {
                           acDataXML(object, content.c_str());
                        }

                        break;
                     }
                  }
               }
            }
            else if (auto src = scan.attrib("object")) {
               OBJECTID objectid;
               if (!FindObject(src.c_str(), 0, FOF::SMART_NAMES, &objectid)) {
                  if ((objectid) and (valid_objectid(Self, objectid))) {
                     objXML *objxml;
                     if (!AccessObject(objectid, 3000, &objxml)) {
                        if (objxml->classID() IS CLASSID::XML) {
                           if (!xmlSerialise(objxml, 0, XMF::INCLUDE_SIBLINGS|XMF::STRIP_CDATA, &content)) {
                              acDataXML(object, content.c_str());
                           }
                        }
                        else log.warning("Cannot extract XML data from a non-XML object.");
                        ReleaseObject(objxml);
                     }
                  }
                  else log.warning("Invalid object reference '%s'", src);
               }
               else log.warning("Unable to find object '%s'", src);
            }
            else {
               if (scan.Children.empty()) continue;
               if (!xmlSerialise(XML, scan.Children.ID, XMF::INCLUDE_SIBLINGS|XMF::STRIP_CDATA, &content)) {
                  acDataXML(object, content.c_str());
               }
            }
         }
         else log.warning("Unsupported data type '%s'", type);
      }
   }

   // Feeds are applied to invoked objects, whereby the object's class name matches a feed.

   if ((!customised) and (Template)) {
      STRING content;
      if (!xmlSerialise(Self->Templates, Template->Children[0].ID, XMF::INCLUDE_SIBLINGS|XMF::STRIP_CDATA, &content)) {
         acDataXML(object, content);
         FreeResource(content);
      }
   }

   if (!InitObject(object)) {
      bc_vector escobj;

      if (Self->Invisible) acHide(object); // Hide the object if it's in an invisible section

      // Child tags are processed as normal, but are applied with respect to the object.  Any tags that reflect
      // document content are passed to the object as XML.

      if (!Tag.Children.empty()) {
         pf::Log log(__FUNCTION__);
         log.traceBranch("Processing child tags for object #%d.", object->UID);
         auto prevobject = Self->CurrentObject;
         Self->CurrentObject = object;
         parse_tags(Tag.Children, Flags & (~IPF::FILTER_ALL));
         Self->CurrentObject = prevobject;
      }

      if (&Children != &Tag.Children) {
         pf::Log log(__FUNCTION__);
         log.traceBranch("Processing further child tags for object #%d.", object->UID);
         auto prevobject = Self->CurrentObject;
         Self->CurrentObject = object;
         parse_tags(Children, Flags & (~IPF::FILTER_ALL));
         Self->CurrentObject = prevobject;
      }

      // The object can self-destruct in ClosingTag(), so check that it still exists before inserting it into the text stream.

      if (!CheckObjectExists(object->UID)) {
         if (Self->BkgdGfx) {
            auto &resource = Self->Resources.emplace_back(object->UID, RTD::OBJECT_UNLOAD);
            resource.class_id = class_id;
         }
         else {
            escobj.object_id = object->UID;
            escobj.class_id  = object->classID();
            escobj.in_line = false;
            if (Self->CurrentObject) escobj.owned = true;

            // By default objects are assumed to be in the background (thus not embedded as part of the text stream).
            // This section is intended to confirm the graphical state of the object.

            if (object->classID() IS CLASSID::VECTOR) {
               //if (layout->Layout & (LAYOUT_BACKGROUND|LAYOUT_FOREGROUND));
               //else if (layout->Layout & LAYOUT_EMBEDDED) escobj.Inline = true;
            }
            else escobj.in_line = true; // If the layout object is not present, the object is managing its own graphics and likely is embedded (button, combobox, checkbox etc are like this)

            m_stream->emplace(Index, escobj);

            if (Self->ObjectCache) {
               switch (object->classID()) {
                  // The following class types can be cached
                  case CLASSID::XML:
                  case CLASSID::FILE:
                  case CLASSID::CONFIG:
                  case CLASSID::COMPRESSION:
                  case CLASSID::SCRIPT: {
                     Self->Resources.emplace_back(object->UID, RTD::PERSISTENT_OBJECT);
                     break;
                  }

                  // The following class types use their own internal caching system

                  default:
                     log.warning("Cannot cache object of class type '%s'", object->Class->ClassName);
                  //case CLASSID::IMAGE:
                  //   auto &res = Self->Resources.emplace_back(object->UID, RTD::OBJECT_UNLOAD);
                     break;
               }
            }
            else {
               auto &res = Self->Resources.emplace_back(object->UID, RTD::OBJECT_UNLOAD);
               res.class_id = class_id;
            }

            // If the object is inline, we will allow whitespace to immediately follow the object.

            if (escobj.in_line) Self->NoWhitespace = false;

            // Add the object to the tab-list if it is in our list of classes that support keyboard input.

            static const CLASSID classes[] = { CLASSID::VECTOR };

            for (unsigned i=0; i < std::ssize(classes); i++) {
               if (classes[i] IS class_id) {
                  add_tabfocus(Self, TT::OBJECT, object->UID);
                  break;
               }
            }
         }
      }
      else log.trace("Object %d self-destructed.", object->UID);
   }
   else {
      FreeResource(object);
      log.warning("Failed to initialise object of class $%.8x", class_id);
   }
      */
}

//********************************************************************************************************************
// The use of pre will turn off the automated whitespace management so that all whitespace is parsed as-is.  It does
// not switch to a monospaced font.

void parser::tag_pre(objXML::TAGS &Children)
{
   auto save = m_strip_feeds;
   m_strip_feeds = true;

   if ((m_style.options & FSO::PREFORMAT) IS FSO::NIL) {
      auto new_style = m_style;
      new_style.options |= FSO::PREFORMAT;
      parse_tags_with_style(Children, new_style);
   }
   else parse_tags(Children);

   m_strip_feeds = save;

   trim_preformat(Self);
}

//********************************************************************************************************************
// By default, a script will be activated when the parser encounters it in the document.  If the script returns a
// result string, that result is assumed to be valid XML and is processed by the parser as such.
//
// If the script contains functions, those functions can be called at any time, either during the parsing process or
// when the document is displayed.
//
// The first script encountered by the parser will serve as the default source for all function calls.  If you need to
// call functions in other scripts then you need to access them by name - e.g. 'myscript.function()'.
//
// Only the first section of content enclosed within the <script> tag (CDATA) is accepted by the script parser.

void parser::tag_script(XMLTag &Tag)
{
   pf::Log log(__FUNCTION__);
   objScript *script;
   ERR error;

   std::string type = "fluid";
   std::string src, cachefile, name;
   bool defaultscript = false;
   bool persistent = false;

   for (LONG i=1; i < std::ssize(Tag.Attribs); i++) {
      auto tagname = Tag.Attribs[i].Name.c_str();
      if (*tagname IS '$') tagname++;
      if (*tagname IS '@') continue; // Variables are set later

      if (iequals("type", tagname)) {
         type = Tag.Attribs[i].Value;
      }
      else if (iequals("persistent", tagname)) {
         // A script that is marked as persistent will survive refreshes
         persistent = true;
      }
      else if (iequals("src", tagname)) {
         if (safe_file_path(Self, Tag.Attribs[i].Value)) {
            src = Tag.Attribs[i].Value;
         }
         else {
            log.warning("Security violation - cannot set script src to: %s", Tag.Attribs[i].Value.c_str());
            return;
         }
      }
      else if (iequals("cache-file", tagname)) {
         // Currently the security risk of specifying a cache file is that you could overwrite files on the user's PC,
         // so for the time being this requires unrestricted mode.

         if ((Self->Flags & DCF::UNRESTRICTED) != DCF::NIL) {
            cachefile = Tag.Attribs[i].Value;
         }
         else {
            log.warning("Security violation - cannot set script cachefile to: %s", Tag.Attribs[i].Value.c_str());
            return;
         }
      }
      else if (iequals("name", tagname)) {
         name = Tag.Attribs[i].Value;
      }
      else if (iequals("default", tagname)) {
         defaultscript = true;
      }
      else if (iequals("external", tagname)) {
         // Reference an external script as the default for function calls
         if ((Self->Flags & DCF::UNRESTRICTED) != DCF::NIL) {
            OBJECTID id;
            if (FindObject(Tag.Attribs[i].Value.c_str(), CLASSID::NIL, FOF::NIL, &id) IS ERR::Okay) {
               Self->DefaultScript = (objScript *)GetObjectPtr(id);
               return;
            }
            else {
               log.warning("Failed to find external script '%s'", Tag.Attribs[i].Value.c_str());
               return;
            }
         }
         else {
            log.warning("Security violation - cannot reference external script '%s'", Tag.Attribs[i].Value.c_str());
            return;
         }
      }
   }

   if ((persistent) and (name.empty())) name = "mainscript";

   if (src.empty()) {
      if ((Tag.Children.empty()) or (!Tag.Children[0].Attribs[0].Name.empty()) or (Tag.Children[0].Attribs[0].Value.empty())) {
         // Ignore if script holds no content
         log.warning("<script/> tag does not contain content.");
         return;
      }
   }

   // If the script is persistent and already exists in the resource cache, do nothing further.

   if (persistent) {
      for (auto &resource : Self->Resources) {
         if (resource.type IS RTD::PERSISTENT_SCRIPT) {
            script = (objScript *)GetObjectPtr(resource.object_id);
            if (iequals(name, script->Name)) {
               log.msg("Persistent script discovered.");
               if ((!Self->DefaultScript) or (defaultscript)) Self->DefaultScript = script;
               return;
            }
         }
      }
   }

   if (iequals("fluid", type)) {
      error = NewLocalObject(CLASSID::FLUID, &script);
   }
   else {
      error = ERR::NoSupport;
      log.warning("Unsupported script type '%s'", type.c_str());
   }

   if (error IS ERR::Okay) {
      if (!name.empty()) SetName(script, name.c_str());

      if (!src.empty()) script->setPath(src);
      else {
         std::string content = xml::GetContent(Tag);
         if (!content.empty()) script->setStatement(content);
      }

      if (!cachefile.empty()) script->setCacheFile(cachefile);

      // Object references are to be limited in scope to the Document object

      //script->setObjectScope(Self->Head.UID);

      // Pass custom arguments in the script tag

      for (unsigned i=1; i < Tag.Attribs.size(); i++) {
         auto tagname = Tag.Attribs[i].Name.c_str();
         if (*tagname IS '$') tagname++;
         if (*tagname IS '@') acSetKey(script, tagname+1, Tag.Attribs[i].Value.c_str());
      }

      if (InitObject(script) IS ERR::Okay) {
         // Pass document arguments to the script

         std::unordered_map<std::string, std::string> *vs;
         if (script->getPtr(FID_Variables, &vs) IS ERR::Okay) {
            Self->Vars   = *vs;
            Self->Params = *vs;
         }

         if (acActivate(script) IS ERR::Okay) { // Persistent scripts survive refreshes.
            Self->Resources.emplace_back(script->UID, persistent ? RTD::PERSISTENT_SCRIPT : RTD::OBJECT_UNLOAD_DELAY);

            if ((!Self->DefaultScript) or (defaultscript)) {
               log.msg("Script #%d is the default script for this document.", script->UID);
               Self->DefaultScript = script;
            }

            // Any results returned from the script are processed as XML

            CSTRING *results;
            LONG size;
            if ((GetFieldArray(script, FID_Results, (APTR *)&results, &size) IS ERR::Okay) and (size > 0)) {
               auto xmlinc = objXML::create::global(fl::Statement(results[0]), fl::Flags(XMF::PARSE_HTML|XMF::STRIP_HEADERS));
               if (xmlinc) {
                  auto old_xml = change_xml(xmlinc);
                  parse_tags(xmlinc->Tags);
                  change_xml(old_xml);

                  // Add the created XML object to the document rather than destroying it

                  Self->Resources.emplace_back(xmlinc->UID, RTD::OBJECT_TEMP);
               }
            }
         }
         else FreeResource(script);
      }
      else FreeResource(script);
   }
}

//********************************************************************************************************************
// Supports FSO::BOLD, FSO::ITALIC, FSO::UNDERLINE

void parser::tag_font_style(objXML::TAGS &Children, FSO StyleFlag)
{
   if ((m_style.options & StyleFlag) IS FSO::NIL) {
      auto new_status = m_style;
      new_status.options |= StyleFlag;
      parse_tags_with_style(Children, new_status);
   }
   else parse_tags(Children);
}

//********************************************************************************************************************
// List item parser.  List items are essentially paragraphs with automated indentation management.

void parser::tag_li(XMLTag &Tag)
{
   pf::Log log(__FUNCTION__);

   if (m_list_stack.empty()) {
      log.warning("<li> not used inside a <list> tag.");
      return;
   }

   auto &list = m_list_stack.top();

   bc_paragraph para(m_style);
   para.list_item   = true;
   para.item_indent = list->item_indent;

   for (LONG i=1; i < std::ssize(Tag.Attribs); i++) {
      auto tagname = Tag.Attribs[i].Name.c_str();
      if (*tagname IS '$') tagname++;

      if (iequals("value", tagname)) {
         para.value = Tag.Attribs[i].Value;
      }
      else if (iequals("aggregate", tagname)) {
         if (Tag.Attribs[i].Value == "true") para.aggregate = true;
         else if (Tag.Attribs[i].Value == "1") para.aggregate = true;
      }
      else check_para_attrib(Tag.Attribs[i], &para, para.font);
   }

   m_paragraph_depth++;

   if ((list->type IS bc_list::CUSTOM) and (!para.value.empty())) {
      auto &stream_para = m_stream->emplace(m_index, para);

         parse_tags_with_embedded_style(Tag.Children, stream_para.font);

      m_stream->emplace<bc_paragraph_end>(m_index);
   }
   else if (list->type IS bc_list::ORDERED) {
      auto list_size = list->buffer.size();
      list->buffer.push_back(std::to_string(list->item_num) + ".");

      // ItemNum is reset because a child list could be created

      auto save_item = list->item_num;
      list->item_num = 1;

      if (para.aggregate) for (auto &p : list->buffer) para.value += p;
      else para.value = list->buffer.back();

      auto &stream_para = m_stream->emplace(m_index, para);

         parse_tags_with_embedded_style(Tag.Children, stream_para.font);

      m_stream->emplace<bc_paragraph_end>(m_index);

      list->item_num = save_item;
      list->buffer.resize(list_size);

      list->item_num++;
   }
   else { // BULLET
      auto &stream_para = m_stream->emplace(m_index, para);

         parse_tags_with_embedded_style(Tag.Children, stream_para.font);

      m_stream->emplace<bc_paragraph_end>(m_index);
      Self->NoWhitespace = true;
   }

   m_paragraph_depth--;
}

//********************************************************************************************************************

void parser::tag_repeat(XMLTag &Tag)
{
   pf::Log log(__FUNCTION__);

   std::string index_name;
   LONG loop_start = 0, loop_end = 0, count = 0, step  = 0;

   for (LONG i=1; i < std::ssize(Tag.Attribs); i++) {
      if (iequals("start", Tag.Attribs[i].Name)) {
         loop_start = std::stoi(Tag.Attribs[i].Value);
         if (loop_start < 0) loop_start = 0;
      }
      else if (iequals("count", Tag.Attribs[i].Name)) {
         count = std::stoi(Tag.Attribs[i].Value);
         if (count < 0) {
            log.warning("Invalid count value of %d", count);
            return;
         }
      }
      else if (iequals("end", Tag.Attribs[i].Name)) {
         loop_end = std::stoi(Tag.Attribs[i].Value) + 1;
      }
      else if (iequals("step", Tag.Attribs[i].Name)) {
         step = std::stoi(Tag.Attribs[i].Value);
      }
      else if (iequals("index", Tag.Attribs[i].Name)) {
         // If an index name is specified, the programmer will need to refer to it as [@indexname] and [%index] will
         // remain unchanged from any parent repeat loop.

         index_name = Tag.Attribs[i].Value;
      }
   }

   if (!step) {
      if (loop_end < loop_start) step = -1;
      else step = 1;
   }

   // Validation - ensure that it will be possible for the repeat loop to execute correctly without the chance of
   // infinite looping.
   //
   // If the user set both count and end attributes, the count attribute will be given the priority here.

   if (count > 0) loop_end = loop_start + (count * step);

   if (step > 0) {
      if (loop_end < loop_start) step = -step;
   }
   else if (loop_end > loop_start) step = -step;

   log.traceBranch("Performing a repeat loop (start: %d, end: %d, step: %d).", loop_start, loop_end, step);

   auto save_index = m_loop_index;

   while (loop_start < loop_end) {
      if (index_name.empty()) m_loop_index = loop_start;
      else acSetKey(Self, index_name.c_str(), std::to_string(loop_start).c_str());

      parse_tags(Tag.Children);
      loop_start += step;
   }

   if (index_name.empty()) m_loop_index = save_index;

   log.trace("insert_child:","Repeat loop ends.");
}

//********************************************************************************************************************
// <table columns="10%,90%" width="100" height="100" fill="rgb(128,128,128)">
//  <row><cell>Activate<brk/>This activates the object.</cell></row>
//  <row><cell span="2">Reset</cell></row>
// </table>
//
// <table width="100" height="100" fill="rgb(128,128,128)">
//  <cell>Activate</cell><cell>This activates the object.</cell>
//  <cell colspan="2">Reset</cell>
// </table>
//
// The only acceptable child tags inside a <table> section are row, brk and cell tags.  Command tags are acceptable
// (repeat, if statements, etc).  The table byte code is typically generated as SCODE::TABLE_START, SCODE::ROW,
// SCODE::CELL..., SCODE::ROW_END, SCODE::TABLE_END.

void parser::tag_table(XMLTag &Tag)
{
   pf::Log log(__FUNCTION__);

   auto &table = m_stream->emplace<bc_table>(m_index);
   table.min_width  = DUNIT(1, DU::PIXEL);
   table.min_height = DUNIT(1, DU::PIXEL);

   std::string columns;
   for (LONG i=1; i < std::ssize(Tag.Attribs); i++) {
      auto &value = Tag.Attribs[i].Value;
      switch (strihash(Tag.Attribs[i].Name)) {
         case HASH_columns:
            // Column preferences are processed only when the end of the table marker has been reached.
            columns = value;
            break;

         case HASH_width:
            table.min_width = DUNIT(value, DU::PIXEL, DBL_MIN);
            break;

         case HASH_height:
            table.min_height = DUNIT(value, DU::PIXEL, DBL_MIN);
            break;

         case HASH_fill:
            table.fill = value;
            break;

         case HASH_stroke:
            table.stroke = value;
            if (table.stroke_width.empty()) table.stroke_width = DUNIT(1.0, DU::PIXEL);
            break;

         case HASH_collapsed: // Collapsed tables do not have spacing (defined by 'spacing' or 'h-spacing') on the edges
            table.collapsed = true;
            break;

         case HASH_spacing: // Spacing between the cells (H & V)
            table.cell_v_spacing = DUNIT(value, DU::PIXEL, 0);
            table.cell_h_spacing = table.cell_v_spacing;
            break;

         case HASH_v_spacing: // Spacing between the cells (V)
            table.cell_v_spacing = DUNIT(value, DU::PIXEL, 0);
            break;

         case HASH_h_spacing: // Spacing between the cells (H)
            table.cell_h_spacing = DUNIT(value, DU::PIXEL, 0);
            break;

         case HASH_align:
            switch(strihash(value)) {
               case HASH_left:   table.align = ALIGN::LEFT; break;
               case HASH_right:  table.align = ALIGN::RIGHT; break;
               case HASH_center: table.align = ALIGN::CENTER; break;
               case HASH_middle: table.align = ALIGN::CENTER; break; // synonym
               default: log.warning("Invalid alignment value '%s'", value.c_str()); break;
            }
            break;

         case HASH_margins: // synonym
         case HASH_cell_padding: // Equivalent to CSS cell-padding
            table.cell_padding.parse(value);
            break;

         case HASH_stroke_width:
            table.stroke_width = std::clamp(strtod(value.c_str(), NULL), 0.0, 255.0);
            break;
      }
   }

   m_table_stack.push(process_table { &table, 0 });

      parse_tags(Tag.Children, IPF::NO_CONTENT|IPF::FILTER_TABLE);

   m_table_stack.pop();

   if (!columns.empty()) { // The columns value, if supplied is arranged as a CSV list of column widths
      std::vector<std::string> list;
      for (unsigned i=0; i < columns.size(); ) {
         auto end = columns.find(',', i);
         if (end IS std::string::npos) end = columns.size();
         auto val = columns.substr(i, end-i);
         trim(val);
         list.push_back(val);
         i = end + 1;
      }

      unsigned i;
      for (i=0; (i < table.columns.size()) and (i < list.size()); i++) {
         table.columns[i].preset_width = strtod(list[i].c_str(), NULL);
         if (list[i].find_first_of('%') != std::string::npos) {
            table.columns[i].preset_width *= 0.01;
            table.columns[i].preset_width_rel = true;
            if ((table.columns[i].preset_width < 0.0000001) or (table.columns[i].preset_width > 1.0)) {
               log.warning("A <table> column value is invalid.");
               Self->Error = ERR::InvalidDimension;
            }
         }
      }

      if (i < table.columns.size()) log.warning("Warning - columns attribute '%s' did not define %d columns.", columns.c_str(), LONG(table.columns.size()));
   }

   bc_table_end end;
   m_stream->emplace(m_index, end);

   Self->NoWhitespace = true; // Setting this to true will prevent the possibility of blank spaces immediately following the table.
}

//********************************************************************************************************************

void parser::tag_row(XMLTag &Tag)
{
   pf::Log log(__FUNCTION__);

   if (m_table_stack.empty()) {
      log.warning("<row> not defined inside <table> section.");
      Self->Error = ERR::InvalidData;
      return;
   }

   bc_row escrow;

   for (LONG i=1; i < std::ssize(Tag.Attribs); i++) {
      if (iequals("height", Tag.Attribs[i].Name)) {
         escrow.min_height = std::clamp(strtod(Tag.Attribs[i].Value.c_str(), NULL), 0.0, 4000.0);
      }
      else if (iequals("fill", Tag.Attribs[i].Name))   escrow.fill   = Tag.Attribs[i].Value;
      else if (iequals("stroke", Tag.Attribs[i].Name)) escrow.stroke = Tag.Attribs[i].Value;
   }

   auto &table = m_table_stack.top();

   m_stream->emplace(m_index, escrow);

   table.table->rows++;
   table.row_col = 0;

   if (!Tag.Children.empty()) {
      parse_tags(Tag.Children, IPF::NO_CONTENT|IPF::FILTER_ROW);
   }

   bc_row_end end;
   m_stream->emplace(m_index, end);

   if (table.row_col > std::ssize(table.table->columns)) {
      table.table->columns.resize(table.row_col);
   }
}

//********************************************************************************************************************

void parser::tag_cell(XMLTag &Tag)
{
   pf::Log log(__FUNCTION__);
   auto new_style = m_style;
   static UBYTE edit_recurse = 0;

   if (m_table_stack.empty()) {
      log.warning("<cell> not defined inside <table> section.");
      Self->Error = ERR::InvalidData;
      return;
   }

   bc_cell cell(glUID++, m_table_stack.top().row_col);
   bool select = false;
   for (LONG i=1; i < std::ssize(Tag.Attribs); i++) {
      switch (strihash(Tag.Attribs[i].Name)) {
         case HASH_border: {
            std::vector<std::string> list;
            pf::split(Tag.Attribs[i].Value, std::back_inserter(list));

            for (auto &v : list) {
               if (iequals("all", v))         cell.border = CB::ALL;
               else if (iequals("top", v))    cell.border |= CB::TOP;
               else if (iequals("left", v))   cell.border |= CB::LEFT;
               else if (iequals("bottom", v)) cell.border |= CB::BOTTOM;
               else if (iequals("right", v))  cell.border |= CB::RIGHT;
            }

            break;
         }

         case HASH_col_span:
            cell.col_span = std::clamp(LONG(std::stoi(Tag.Attribs[i].Value)), 1, 1000);
            break;

         case HASH_row_span:
            cell.row_span = std::clamp(LONG(std::stoi(Tag.Attribs[i].Value)), 1, 1000);
            break;

         case HASH_edit:
            if (edit_recurse) {
               log.warning("Edit cells cannot be embedded recursively.");
               Self->Error = ERR::Recursion;
               return;
            }
            cell.edit_def = Tag.Attribs[i].Value;

            if (!Self->EditDefs.contains(Tag.Attribs[i].Value)) {
               log.warning("Edit definition '%s' does not exist.", Tag.Attribs[i].Value.c_str());
               cell.edit_def.clear();
            }
            break;

         case HASH_stroke:
            cell.stroke = Tag.Attribs[i].Value;
            if (cell.stroke_width.empty()) {
               cell.stroke_width = m_table_stack.top().table->stroke_width;
               if (cell.stroke_width.empty()) cell.stroke_width = DUNIT(1.0, DU::PIXEL);
            }
            break;

         case HASH_select: select = true; break;

         case HASH_fill: cell.fill = Tag.Attribs[i].Value; break;

         case HASH_stroke_width: cell.stroke_width = DUNIT(Tag.Attribs[i].Value, DU::PIXEL); break;

         case HASH_no_wrap: new_style.options |= FSO::NO_WRAP; break;

         // NOTE: For the following events, if the client is embedding a document with the intention of using
         // event hooks, they can opt to define an empty string so that the relevant input_events flag is set.

         case HASH_on_click:
            cell.hooks.events |= JTYPE::BUTTON;
            cell.hooks.on_click = Tag.Attribs[i].Value;
            break;

         case HASH_on_motion:
            cell.hooks.events |= JTYPE::MOVEMENT;
            cell.hooks.on_motion = Tag.Attribs[i].Value;
            break;

         case HASH_on_crossing:
            cell.hooks.events |= JTYPE::CROSSING;
            cell.hooks.on_crossing = Tag.Attribs[i].Value;
            break;

         default:
            if (Tag.Attribs[i].Name.starts_with('@')) {
               cell.args[Tag.Attribs[i].Name.substr(1)] = Tag.Attribs[i].Value;
            }
            else cell.args[Tag.Attribs[i].Name] = Tag.Attribs[i].Value;
      }
   }

   if (!cell.edit_def.empty()) edit_recurse++;

   // Edit sections enforce preformatting, which means that all whitespace entered by the user will be taken
   // into account.  The following check sets FSO::PREFORMAT if it hasn't been set already.

   auto cell_index = m_index;

   if (!Tag.Children.empty()) {
      Self->NoWhitespace = true; // Reset whitespace flag: false allows whitespace at the start of the cell, true prevents whitespace

      if ((!cell.edit_def.empty()) and ((m_style.options & FSO::PREFORMAT) IS FSO::NIL)) {
         new_style.options |= FSO::PREFORMAT;
      }

      // Cell content is managed in an internal stream

      parser parse(Self, cell.stream);

      parse.m_paragraph_depth++;
      parse.parse_tags_with_style(Tag.Children, new_style);
      parse.m_paragraph_depth--;
   }

   auto &stream_cell = m_stream->emplace(m_index, cell);

   m_table_stack.top().row_col += stream_cell.col_span;

   if (!stream_cell.edit_def.empty()) {
      // Links are added to the list of tabbable points

      LONG tab = add_tabfocus(Self, TT::EDIT, stream_cell.cell_id);
      if (select) Self->FocusIndex = tab;
   }

   if (!stream_cell.edit_def.empty()) edit_recurse--;
}

//********************************************************************************************************************
// No response is required for page tags, but we can check for validity.

void parser::tag_page(XMLTag &Tag)
{
   pf::Log log(__FUNCTION__);
   if (auto name = Tag.attrib("name")) {
      auto str = name->c_str();
      while (*str) {
         if (((*str >= 'A') and (*str <= 'Z')) or
             ((*str >= 'a') and (*str <= 'z')) or
             ((*str >= '0') and (*str <= '9'))) {
            // Character is valid
         }
         else {
            log.warning("Page has an invalid name of '%s'.  Character support is limited to [A-Z,a-z,0-9].", name->c_str());
            break;
         }
         str++;
      }
   }
}

//********************************************************************************************************************
// Usage: <trigger event="resize" function="script.function"/>

void parser::tag_trigger(XMLTag &Tag)
{
   pf::Log log(__FUNCTION__);
   DRT trigger_code;
   objScript *script;
   LARGE function_id;

   std::string event, function_name;
   for (LONG i=1; i < std::ssize(Tag.Attribs); i++) {
      switch (strihash(Tag.Attribs[i].Name)) {
         case HASH_event: event = Tag.Attribs[i].Value; break;
         case HASH_function: function_name = Tag.Attribs[i].Value; break;
      }
   }

   if ((!event.empty()) and (!function_name.empty())) {
      // These are described in the documentation for the AddListener method

      switch(strihash(event)) {
         case HASH_after_layout:    trigger_code = DRT::AFTER_LAYOUT; break;
         case HASH_before_layout:   trigger_code = DRT::BEFORE_LAYOUT; break;
         case HASH_on_click:        trigger_code = DRT::USER_CLICK; break;
         case HASH_on_release:      trigger_code = DRT::USER_CLICK_RELEASE; break;
         case HASH_on_motion:       trigger_code = DRT::USER_MOVEMENT; break;
         case HASH_refresh:         trigger_code = DRT::REFRESH; break;
         case HASH_focus:           trigger_code = DRT::GOT_FOCUS; break;
         case HASH_focus_lost:      trigger_code = DRT::LOST_FOCUS; break;
         case HASH_leaving_page:    trigger_code = DRT::LEAVING_PAGE; break;
         case HASH_page_processed:  trigger_code = DRT::PAGE_PROCESSED; break;
         default:
            log.warning("Trigger event '%s' for function '%s' is not recognised.", event.c_str(), function_name.c_str());
            return;
      }

      // Get the script

      std::string args;
      if (extract_script(Self, function_name.c_str(), &script, function_name, args) IS ERR::Okay) {
         if (script->getProcedureID(function_name.c_str(), &function_id) IS ERR::Okay) {
            Self->Triggers[LONG(trigger_code)].emplace_back(FUNCTION(script, function_id));
         }
         else log.warning("Unable to resolve '%s' in script #%d to a function ID (the procedure may not exist)", function_name.c_str(), script->UID);
      }
      else log.warning("The script for '%s' is not available - check if it is declared prior to the trigger tag.", function_name.c_str());
   }
}
