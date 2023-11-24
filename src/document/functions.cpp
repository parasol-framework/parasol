
static const LONG MAXLOOP = 100000;

static const char glDefaultStyles[] =
"<template name=\"h1\"><p leading=\"2.0\"><font face=\"Open Sans\" size=\"18\" style=\"bold\"><inject/></font></p></template>\n\
<template name=\"h2\"><p leading=\"2.0\"><font face=\"Open Sans\" size=\"16\" style=\"bold\"><inject/></font></p></template>\n\
<template name=\"h3\"><p leading=\"1.5\"><font face=\"Open Sans\" size=\"14\" style=\"bold\"><inject/></font></p></template>\n\
<template name=\"h4\"><p leading=\"1.5\"><font face=\"Open Sans\" size=\"14\"><inject/></font></p></template>\n\
<template name=\"h5\"><p leading=\"1.25\"><font face=\"Open Sans\" size=\"12\"><inject/></font></p></template>\n\
<template name=\"h6\"><p leading=\"1.25\"><font face=\"Open Sans\" size=\"10\"><inject/></font></p></template>\n";

static const Field * find_field(OBJECTPTR Object, CSTRING Name, OBJECTPTR *Source) // Read-only, thread safe function.
{
   // Skip any special characters that are leading the field name (e.g. $, @).  Some symbols like / are used for XPath
   // lookups, so we only want to skip reserved symbols or we risk confusion between real fields and variable fields.

   while (Name[0]) {
      if (Name[0] IS '$') Name++;
      else if (Name[0] IS '@') Name++;
      else break;
   }

   return FindField(Object, StrHash(Name, false), Source);
}

//********************************************************************************************************************

static DOUBLE fast_hypot(DOUBLE Width, DOUBLE Height)
{
   if (Width > Height) std::swap(Width, Height);
   if ((Height / Width) <= 1.5) return 5.0 * (Width + Height) / 7.0; // Fast hypot calculation accurate to within 1% for specific use cases.
   else return std::sqrt((Width * Width) + (Height * Height));
}

//********************************************************************************************************************

static bool read_rgb8(CSTRING Value, RGB8 *RGB)
{
   FRGB rgb;
   if (!vecReadPainter(NULL, Value, &rgb, NULL, NULL, NULL)) {
      RGB->Red   = F2T(rgb.Red   * 255.0);
      RGB->Green = F2T(rgb.Green * 255.0);
      RGB->Blue  = F2T(rgb.Blue  * 255.0);
      RGB->Alpha = F2T(rgb.Alpha * 255.0);
      return true;
   }
   else return false;
}

//********************************************************************************************************************
// Extract all printable text between start and End.

static std::string stream_to_string(extDocument *Self, stream_char Start, stream_char End)
{
   if (End < Start) std::swap(Start, End);

   std::ostringstream str;
   auto cs = Start;
   for (; (cs.index <= End.index) and (cs.index < INDEX(Self->Stream.size())); cs.nextCode()) {
      if (Self->Stream[cs.index].code IS SCODE::TEXT) {
         auto &text = stream_data<bc_text>(Self, cs);
         if (cs.index < End.index) {
            str << text.text.substr(cs.offset, text.text.size() - cs.offset);
         }
         else str << text.text.substr(cs.offset, End.offset - cs.offset);
      }
   }
   return str.str();
}

/*********************************************************************************************************************

This function can be used for performing simple calculations on numeric values and strings.  It can return a result in
either a numeric format or in a string buffer if the calculation involves non-numeric characters.  Here are some
examples of valid strings:

<pre>
100/50+(12*14)
0.05 * 100 + '%'
</pre>

Currently acceptable operators are plus, minus, divide and multiply.  String references must be enclosed in single
quotes or will be ignored.  Brackets may be used to organise the order of operations during calculation.

Special operators include:

<types type="Symbol">
<type name="p">This character immediately followed with an integer allows you to change the floating-point precision of output values.</>
<type name="f">The same as the 'p' operator except the precision is always guaranteed to be fixed at that value through the use of trailing zeros (so a fixed precision of two used to print the number '7' will give a result of '7.00'.</>
</>

*********************************************************************************************************************/

static std::string write_calc(DOUBLE Value, WORD Precision)
{
   if (!Precision) return std::to_string(F2T(Value));

   LARGE wholepart = F2T(Value);
   auto out = std::to_string(wholepart);

   DOUBLE fraction = std::abs(Value) - std::abs(wholepart);
   if ((fraction > 0) or (Precision < 0)) {
      out += '.';
      fraction *= 10;
      auto px = std::abs(Precision);
      while ((fraction > 0.00001) and (px > 0)) {
         auto ival = F2T(fraction);
         out += char(ival) + '0';
         fraction = (fraction - ival) * 10;
         px--;
      }

      while (px > 0) { out += '0'; px--; }
   }

   return out;
}

//********************************************************************************************************************

static ERROR consume_input_events(objVector *Vector, const InputEvent *Events)
{
   auto Self = (extDocument *)CurrentContext();

   for (auto input=Events; input; input=input->Next) {
      if ((input->Flags & JTYPE::MOVEMENT) != JTYPE::NIL) {
         for (auto scan=input->Next; (scan) and ((scan->Flags & JTYPE::MOVEMENT) != JTYPE::NIL); scan=scan->Next) {
            input = scan;
         }

         if (input->OverID IS Self->Page->UID) Self->MouseInPage = true;
         else Self->MouseInPage = false;


         check_mouse_pos(Self, input->X, input->Y);

         // Note that this code has to 'drop through' due to the movement consolidation loop earlier in this subroutine.
      }

      if (input->Type IS JET::LMB) {
         if (input->Value > 0) {
            Self->LMB = true;
            check_mouse_click(Self, input->X, input->Y);
         }
         else {
            Self->LMB = false;
            check_mouse_release(Self, input->X, input->Y);
         }
      }
   }

   return ERR_Okay;
}

//********************************************************************************************************************
// Designed for reading unit values such as '50%' and '6px'.  The returned value is scaled to pixels.

static CSTRING read_unit(CSTRING Input, DOUBLE &Output, bool &Relative)
{
   bool isnumber = true;
   auto v = Input;
   while ((*v) and (*v <= 0x20)) v++;

   auto str = v;
   if ((*str IS '-') or (*str IS '+')) str++;

   Relative = false;
   if (((*str >= '0') and (*str <= '9')) or (*str IS '.')) {
      while ((*str >= '0') and (*str <= '9')) str++;

      if (*str IS '.') {
         str++;
         if ((*str >= '0') and (*str <= '9')) {
            while ((*str >= '0') and (*str <= '9')) str++;
         }
         else isnumber = false;
      }

      DOUBLE multiplier = 1.0;
      DOUBLE dpi = 96.0;

      if (*str IS '%') {
         Relative = true;
         multiplier = 0.01;
         str++;
      }
      else if ((str[0] IS 'p') and (str[1] IS 'x')) str += 2; // Pixel.  This is the default type
      else if ((str[0] IS 'e') and (str[1] IS 'm')) { str += 2; multiplier = 12.0 * (4.0 / 3.0); } // Multiply the current font's pixel height by the provided em value
      else if ((str[0] IS 'e') and (str[1] IS 'x')) { str += 2; multiplier = 6.0 * (4.0 / 3.0); } // As for em, but multiple by the pixel height of the 'x' character.  If no x character, revert to 0.5em
      else if ((str[0] IS 'i') and (str[1] IS 'n')) { str += 2; multiplier = dpi; } // Inches
      else if ((str[0] IS 'c') and (str[1] IS 'm')) { str += 2; multiplier = (1.0 / 2.56) * dpi; } // Centimetres
      else if ((str[0] IS 'm') and (str[1] IS 'm')) { str += 2; multiplier = (1.0 / 20.56) * dpi; } // Millimetres
      else if ((str[0] IS 'p') and (str[1] IS 't')) { str += 2; multiplier = (4.0 / 3.0); } // Points.  A point is 4/3 of a pixel
      else if ((str[0] IS 'p') and (str[1] IS 'c')) { str += 2; multiplier = (4.0 / 3.0) * 12.0; } // Pica.  1 Pica is equal to 12 Points

      Output = StrToFloat(v) * multiplier;
   }
   else Output = 0;

   return str;
}

//********************************************************************************************************************
// Checks if the file path is safe, i.e. does not refer to an absolute file location.

static LONG safe_file_path(extDocument *Self, const std::string &Path)
{
   if ((Self->Flags & DCF::UNRESTRICTED) != DCF::NIL) return true;





   return false;
}

/*********************************************************************************************************************
** Processes an XML tag and passes it to parse_tag().
**
** IXF::HOLDSTYLE:  If set, the font style will not be cleared.
** IXF::RESETSTYLE: If set, the current font style will be completely reset, rather than defaulting to the most recent font style used at the insertion point.
** IXF::SIBLINGS:   If set, sibling tags that follow the root will be parsed.
*/

static ERROR insert_xml(extDocument *Self, objXML *XML, objXML::TAGS &Tag, INDEX TargetIndex, IXF Flags)
{
   pf::Log log(__FUNCTION__);

   if (TargetIndex < 0) TargetIndex = Self->Stream.size();

   log.traceBranch("Index: %d, Flags: $%.2x, Tag: %s", TargetIndex, LONG(Flags), Tag[0].Attribs[0].Name.c_str());

   // Retrieve the most recent font definition and use that as the style that we're going to start with.

   if ((Flags & IXF::HOLDSTYLE) != IXF::NIL) {
      // Do nothing to change the style
   }
   else {
      Self->Style = style_status();

      if ((Flags & IXF::RESETSTYLE) != IXF::NIL) {
         // Do not search for the most recent font style
      }
      else {
         for (auto i = TargetIndex - 1; i > 0; i--) {
            if (Self->Stream[i].code IS SCODE::FONT) {
               Self->Style.font_style = stream_data<bc_font>(Self, i);
               log.trace("Found existing font style, font index %d, flags $%.8x.", Self->Style.font_style.font_index, Self->Style.font_style.options);
               break;
            }
         }
      }

      // If no style is available, we need to create a default font style and insert it at the start of the stream.

      if (Self->Style.font_style.font_index IS -1) {
         if ((Self->Style.font_style.font_index = create_font(Self->FontFace, "Regular", Self->FontSize)) IS -1) {
            if ((Self->Style.font_style.font_index = create_font("Open Sans", "Regular", 10)) IS -1) {
               return ERR_Failed;
            }
         }

         Self->Style.font_style.fill = Self->FontFill;
         Self->Style.face_change = true;
      }

      if (auto font = Self->Style.font_style.get_font()) {
         Self->Style.face  = font->Face;
         Self->Style.point = font->Point;
      }
   }

   // Parse content and insert it at the end of the stream (we will move it to the insertion point afterwards).

   stream_char inserted_at(Self->Stream.size());
   stream_char insert_index(Self->Stream.size());
   if ((Flags & IXF::SIBLINGS) != IXF::NIL) { // Siblings of Tag are included
      parse_tags(Self, XML, Tag, insert_index);
   }
   else { // Siblings of Tag are not included
      auto parse_flags = IPF::NIL;
      parse_tag(Self, XML, Tag[0], insert_index, parse_flags);
   }

   if ((Flags & IXF::CLOSESTYLE) != IXF::NIL) style_check(Self, insert_index);

   if (INDEX(Self->Stream.size()) <= inserted_at.index) {
      log.trace("parse_tag() did not insert any content into the stream.");
      return ERR_NothingDone;
   }

   // Move the content from the end of the stream to the requested insertion point

   if (TargetIndex < inserted_at.index) {
      auto length = Self->Stream.size() - inserted_at.index;
      log.trace("Moving new content of %d bytes to the insertion point at index %d", TargetIndex, length);
      Self->Stream.insert(Self->Stream.begin() + TargetIndex, Self->Stream.begin() + inserted_at.index, Self->Stream.begin() + length);
      Self->Stream.resize(inserted_at.index + length);
   }

   // Check that the FocusIndex is valid (there's a slim possibility that it may not be if AC_Focus has been
   // incorrectly used).

   if (Self->FocusIndex >= LONG(Self->Tabs.size())) Self->FocusIndex = -1;

   return ERR_Okay;
}

static ERROR insert_xml(extDocument *Self, objXML *XML, XMLTag &Tag, stream_char TargetIndex, IXF Flags)
{
   pf::Log log(__FUNCTION__);

   if (TargetIndex < 0) TargetIndex = Self->Stream.size();

   log.traceBranch("Index: %d, Flags: $%.2x, Tag: %s", TargetIndex.index, LONG(Flags), Tag.Attribs[0].Name.c_str());

   // Retrieve the most recent font definition and use that as the style that we're going to start with.

   if ((Flags & IXF::HOLDSTYLE) != IXF::NIL) {
      // Do nothing to change the style
   }
   else {
      Self->Style = style_status();

      if ((Flags & IXF::RESETSTYLE) != IXF::NIL) {
         // Do not search for the most recent font style
      }
      else {
         for (auto i = TargetIndex.index - 1; i > 0; i--) {
            if (Self->Stream[i].code IS SCODE::FONT) {
               Self->Style.font_style = stream_data<bc_font>(Self, i);
               log.trace("Found existing font style, font index %d, flags $%.8x.", Self->Style.font_style.font_index, Self->Style.font_style.options);
               break;
            }
         }
      }

      // If no style is available, we need to create a default font style and insert it at the start of the stream.

      if (Self->Style.font_style.font_index IS -1) {
         if ((Self->Style.font_style.font_index = create_font(Self->FontFace, "Regular", Self->FontSize)) IS -1) {
            if ((Self->Style.font_style.font_index = create_font("Open Sans", "Regular", 10)) IS -1) {
               return ERR_Failed;
            }
         }

         Self->Style.font_style.fill = Self->FontFill;
         Self->Style.face_change = true;
      }

      if (auto font = Self->Style.font_style.get_font()) {
         Self->Style.face  = font->Face;
         Self->Style.point = font->Point;
      }
   }

   // Parse content and insert it at the end of the stream (we will move it to the insertion point afterwards).

   stream_char inserted_at(Self->Stream.size());
   stream_char insert_index(Self->Stream.size());
   auto flags = IPF::NIL;
   parse_tag(Self, XML, Tag, insert_index, flags);

   if ((Flags & IXF::CLOSESTYLE) != IXF::NIL) style_check(Self, insert_index);

   if (INDEX(Self->Stream.size()) <= inserted_at.index) {
      log.trace("parse_tag() did not insert any content into the stream.");
      return ERR_NothingDone;
   }

   // Move the content from the end of the stream to the requested insertion point

   if (TargetIndex < inserted_at) {
      auto length = INDEX(Self->Stream.size()) - inserted_at.index;
      log.trace("Moving new content of %d bytes to the insertion point at index %d", TargetIndex, length);
      Self->Stream.insert(Self->Stream.begin() + TargetIndex.index, Self->Stream.begin() + inserted_at.index, Self->Stream.begin() + length);
      Self->Stream.resize(inserted_at.index + length);
   }

   // Check that the FocusIndex is valid (there's a slim possibility that it may not be if AC_Focus has been
   // incorrectly used).

   if (Self->FocusIndex >= LONG(Self->Tabs.size())) Self->FocusIndex = -1;

   return ERR_Okay;
}

//********************************************************************************************************************
// This is the principal function for adding/inserting text into the document stream, whether that be in the parse
// phase or from user editing.
//
// Preformat must be set to true if all consecutive whitespace characters in Text are to be inserted.

static ERROR insert_text(extDocument *Self, stream_char &Index, const std::string &Text, bool Preformat)
{
   // Check if there is content to be processed

   if ((!Preformat) and (Self->NoWhitespace)) {
      unsigned i;
      for (i=0; i < Text.size(); i++) if (Text[i] > 0x20) break;
      if (i IS Text.size()) return ERR_Okay;
   }

   style_check(Self, Index);

   if (Preformat) {
      bc_text et(Text, true);
      Self->insert_code(Index, et);
   }
   else {
      bc_text et;
      et.text.reserve(Text.size());
      auto ws = Self->NoWhitespace; // Retrieve previous whitespace state
      for (unsigned i=0; i < Text.size(); ) {
         if (Text[i] <= 0x20) { // Whitespace encountered
            for (++i; (Text[i] <= 0x20) and (i < Text.size()); i++);
            if (!ws) et.text += ' ';
            ws = true;
         }
         else {
            et.text += Text[i++];
            ws = false;
         }
      }
      Self->NoWhitespace = ws;
      Self->insert_code(Index, et);
   }

   return ERR_Okay;
}

//********************************************************************************************************************

static ERROR load_doc(extDocument *Self, std::string Path, bool Unload, ULD UnloadFlags)
{
   pf::Log log(__FUNCTION__);

   log.branch("Loading file '%s', page '%s'", Path.c_str(), Self->PageName.c_str());

   if (Unload) unload_doc(Self, UnloadFlags);

   process_parameters(Self, Path);

   // Generate a path without parameter values.

   auto i = Path.find_first_of("&#?");
   if (i != std::string::npos) Path.erase(i);

   if (!AnalysePath(Path.c_str(), NULL)) {
      auto task = CurrentTask();
      task->setPath(Path);

      if (auto xml = objXML::create::integral(
         fl::Flags(XMF::ALL_CONTENT|XMF::PARSE_HTML|XMF::STRIP_HEADERS|XMF::WELL_FORMED),
         fl::Path(Path), fl::ReadOnly(true))) {

         if (Self->XML) FreeResource(Self->XML);
         Self->XML = xml;

         pf::LogLevel level(3);
         Self->Error = process_page(Self, xml);

         return Self->Error;
      }
      else {
         error_dialog("Document Load Error", std::string("Failed to load document file '") + Path + "'");
         return log.warning(ERR_OpenFile);
      }
   }
   else return log.warning(ERR_FileNotFound);
}

//********************************************************************************************************************
// Converts XML into RIPPLE bytecode, then displays the page that is referenced by the PageName field by calling
// layout_doc().  If the PageName is unspecified, we use the first <page> that has no name, otherwise the first page
// irrespective of the name.
//
// This function does not clear existing data, so you can use it to append new content to existing document content.

static ERROR process_page(extDocument *Self, objXML *xml)
{
   if (!xml) return ERR_NoData;

   pf::Log log(__FUNCTION__);

   log.branch("Page: %s, XML: %d", Self->PageName.c_str(), xml->UID);

   // Look for the first page that matches the requested page name (if a name is specified).  Pages can be located anywhere
   // within the XML source - they don't have to be at the root.

   XMLTag *page = NULL;
   for (auto &scan : xml->Tags) {
      if (StrMatch("page", scan.Attribs[0].Name)) continue;

      if (!page) page = &scan;

      if (Self->PageName.empty()) break;
      else if (auto name = scan.attrib("name")) {
         if (!StrMatch(Self->PageName, *name)) page = &scan;
      }
   }

   Self->Error = ERR_Okay;
   if ((page) and (!page->Children.empty())) {
      Self->PageTag = page;

      bool noheader = false;
      bool nofooter = false;
      if (page->attrib("noheader")) noheader = true;
      if (page->attrib("nofooter")) nofooter = true;

      Self->Segments.clear();
      Self->SortSegments.clear();
      Self->TemplateArgs.clear();

      Self->SelectStart.reset();
      Self->SelectEnd.reset();
      Self->XPosition    = 0;
      Self->YPosition    = 0;
      Self->UpdatingLayout = true;
      Self->Error        = ERR_Okay;

      // Process tags at the root level, but only those that we allow up to the first <page> entry.

      pf::Log log(__FUNCTION__);

      log.traceBranch("Processing root level tags.");

      Self->BodyTag   = NULL;
      Self->HeaderTag = NULL;
      Self->FooterTag = NULL;
      for (auto &tag : xml->Tags) {
         if (tag.isContent()) continue;

         switch (StrHash(tag.Attribs[0].Name)) {
            case HASH_body:
               // If a <body> tag contains any children, it is treated as a template and must contain an <inject/> tag so
               // that the XML insertion point is known.

               insert_xml(Self, xml, tag); // Process the body attributes in tag_body() and set BodyTag
               break;
            case HASH_page:       break;
            case HASH_background: insert_xml(Self, xml, tag); break;
            case HASH_editdef:    insert_xml(Self, xml, tag); break;
            case HASH_template:   insert_xml(Self, xml, tag); break;
            case HASH_head:       insert_xml(Self, xml, tag); break;
            case HASH_info:       insert_xml(Self, xml, tag); break;
            case HASH_include:    insert_xml(Self, xml, tag); break;
            case HASH_parse:      insert_xml(Self, xml, tag); break;
            case HASH_script:     insert_xml(Self, xml, tag); break;
            case HASH_header:     Self->HeaderTag = &tag.Children; break;
            case HASH_footer:     Self->FooterTag = &tag.Children; break;
            default: log.warning("Tag '%s' Not supported at the root level.", tag.Attribs[0].Name.c_str());
         }
      }

      if ((Self->HeaderTag) and (!noheader)) {
         pf::Log log(__FUNCTION__);
         log.traceBranch("Processing header.");
         insert_xml(Self, xml, Self->HeaderTag[0][0], Self->Stream.size(), IXF::SIBLINGS|IXF::RESETSTYLE);
      }

      if (Self->BodyTag) {
         pf::Log log(__FUNCTION__);
         log.traceBranch("Processing this page through the body tag.");

         init_template block(Self, page->Children, xml);
         insert_xml(Self, xml, Self->BodyTag[0][0], Self->Stream.size(), IXF::SIBLINGS|IXF::RESETSTYLE);
      }
      else {
         pf::Log log(__FUNCTION__);
         auto page_name = page->attrib("name");
         log.traceBranch("Processing page '%s'.", page_name ? page_name->c_str() : "");
         insert_xml(Self, xml, page->Children, Self->Stream.size(), IXF::SIBLINGS|IXF::RESETSTYLE);
      }

      if ((Self->FooterTag) and (!nofooter)) {
         pf::Log log(__FUNCTION__);
         log.traceBranch("Processing footer.");
         insert_xml(Self, xml, Self->FooterTag[0][0], Self->Stream.size(), IXF::SIBLINGS|IXF::RESETSTYLE);
      }

      #ifdef DBG_STREAM
         print_stream(Self, Self->Stream);
      #endif

      // If an error occurred then we have to kill the document as the stream may contain disconnected escape
      // sequences (e.g. an unterminated SCODE::TABLE sequence).

      if (Self->Error) unload_doc(Self, ULD::NIL);

      Self->UpdatingLayout = true;
      if (Self->initialised()) redraw(Self, true);

      #ifdef RAW_OUTPUT
         objFile::create file = { fl::Path("drive1:doc-stream.bin"), fl::Flags(FL::NEW|FL::WRITE) };
         file->write(Self->Stream, Self->Stream.size());
      #endif
   }
   else {
      if (!Self->PageName.empty()) {
         auto msg = std::string("Failed to find page '") + Self->PageName + "' in document '" + Self->Path + "'.";
         error_dialog("Load Failed", msg);
      }
      else {
         auto msg = std::string("Failed to find a valid page in document '") + Self->Path > "'.";
         error_dialog("Load Failed", msg);
      }
      Self->Error = ERR_Search;
   }

   if ((!Self->Error) and (Self->MouseInPage)) {
      DOUBLE x, y;
      if (!gfxGetRelativeCursorPos(Self->Page->UID, &x, &y)) {
         check_mouse_pos(Self, x, y);
      }
   }

   if (!Self->PageProcessed) {
      for (auto &trigger : Self->Triggers[LONG(DRT::PAGE_PROCESSED)]) {
         if (trigger.Type IS CALL_SCRIPT) {
            scCallback(trigger.Script.Script, trigger.Script.ProcedureID, NULL, 0, NULL);
         }
         else if (trigger.Type IS CALL_STDC) {
            auto routine = (void (*)(APTR, extDocument *))trigger.StdC.Routine;
            pf::SwitchContext context(trigger.StdC.Context);
            routine(trigger.StdC.Context, Self);
         }
      }
   }

   Self->PageProcessed = true;
   return Self->Error;
}

//********************************************************************************************************************
// This function removes all allocations that were made in displaying the current page, and resets a number of
// variables that they are at the default settings for the next page.
//
// Set Terminate to true only if the document object is being destroyed.
//
// The PageName is not freed because the desired page must not be dropped during refresh of manually loaded XML for
// example.

static ERROR unload_doc(extDocument *Self, ULD Flags)
{
   pf::Log log(__FUNCTION__);

   log.branch("Flags: $%.2x", LONG(Flags));

   #ifdef DBG_STREAM
      print_stream(Self);
   #endif

   log.trace("Resetting variables.");

   Self->Highlight = glHighlight;

   if (Self->CursorStroke) FreeResource(Self->CursorStroke);
   Self->CursorStroke = StrClone("rgb(102,102,204,255)");

   Self->FontFill       = "rgb(0,0,0)";
   Self->LinkFill       = "rgb(0,0,255,255)";
   Self->LinkSelectFill = "rgb(255,0,0,255)";

   if (Self->Background) FreeResource(Self->Background);
   Self->Background   = StrClone("rgb(255,255,255,255)");

   Self->LeftMargin    = 10;
   Self->RightMargin   = 10;
   Self->TopMargin     = 10;
   Self->BottomMargin  = 10;
   Self->XPosition     = 0;
   Self->YPosition     = 0;
   //Self->ScrollVisible = false;
   Self->PageHeight    = 0;
   Self->Invisible     = 0;
   Self->PageWidth     = 0;
   Self->CalcWidth     = 0;
   Self->LineHeight    = LINE_HEIGHT; // Default line height for measurements concerning the page (can be derived from a font)
   Self->RelPageWidth  = false;
   Self->MinPageWidth  = MIN_PAGE_WIDTH;
   Self->DefaultScript = NULL;
   Self->BkgdGfx       = 0;
   Self->FontSize      = DEFAULT_FONTSIZE;
   Self->FocusIndex    = -1;
   Self->PageProcessed = false;
   Self->RefreshTemplates = true;
   Self->MouseOverSegment = -1;
   Self->ActiveEditCellID = 0;
   Self->ActiveEditDef    = NULL;
   Self->SelectIndex.reset();
   Self->CursorIndex.reset();

   if (Self->ActiveEditDef) deactivate_edit(Self, false);

   Self->Links.clear();

   if (Self->LinkIndex != -1) {
      Self->LinkIndex = -1;
      gfxRestoreCursor(PTC::DEFAULT, Self->UID);
   }

   Self->FontFace = "Open Sans";
   Self->PageTag = NULL;

   Self->EditCells.clear();
   Self->Stream.clear();
   Self->SortSegments.clear();
   Self->Segments.clear();
   Self->Params.clear();
   Self->MouseOverChain.clear();
   Self->Tabs.clear();

   for (auto &t : Self->Triggers) t.clear();

   if ((Flags & ULD::TERMINATE) != ULD::NIL) Self->Vars.clear();

   if (Self->Keywords)    { FreeResource(Self->Keywords); Self->Keywords = NULL; }
   if (Self->Author)      { FreeResource(Self->Author); Self->Author = NULL; }
   if (Self->Copyright)   { FreeResource(Self->Copyright); Self->Copyright = NULL; }
   if (Self->Description) { FreeResource(Self->Description); Self->Description = NULL; }
   if (Self->Title)       { FreeResource(Self->Title); Self->Title = NULL; }

   // Free templates only if they have been modified (no longer at the default settings).

   if (Self->Templates) {
      if (Self->TemplatesModified != Self->Templates->Modified) {
         FreeResource(Self->Templates);
         Self->Templates = NULL;
      }
   }

   // Remove all page related resources

   if (!Self->Resources.empty()) {
      pf::Log log(__FUNCTION__);
      log.branch("Freeing page-allocated resources.");

      for (auto it = Self->Resources.begin(); it != Self->Resources.end(); ) {
         if ((ULD::TERMINATE) != ULD::NIL) it->terminate = true;

         if ((it->type IS RTD::PERSISTENT_SCRIPT) or (it->type IS RTD::PERSISTENT_OBJECT)) {
            // Persistent objects and scripts will survive refreshes
            if ((Flags & ULD::REFRESH) != ULD::NIL);
            else { it = Self->Resources.erase(it); continue; }
         }
         else { it = Self->Resources.erase(it); continue; }

         it++;
      }
   }

   if (!Self->Templates) {
      if (!(Self->Templates = objXML::create::integral(fl::Name("xmlTemplates"), fl::Statement(glDefaultStyles),
         fl::Flags(XMF::PARSE_HTML|XMF::STRIP_HEADERS)))) return ERR_CreateObject;

      Self->TemplatesModified = Self->Templates->Modified;
      Self->RefreshTemplates = true;
   }

   if (Self->Page) acMoveToPoint(Self->Page, 0, 0, 0, MTF::X|MTF::Y);

   Self->NoWhitespace   = true;
   Self->UpdatingLayout = true;
   Self->GeneratedID    = AllocateID(IDTYPE::GLOBAL);

   if ((Flags & ULD::REDRAW) != ULD::NIL) {
      Self->Viewport->draw();
   }

   return ERR_Okay;
}

//********************************************************************************************************************

#if 0
static LONG get_line_from_index(extDocument *Self, INDEX index)
{
   LONG line;
   for (line=1; line < Self->SegCount; line++) {
      if (index < Self->Segments[line].index) {
         return line-1;
      }
   }
   return 0;
}
#endif

//********************************************************************************************************************

static std::string get_font_style(FSO Options)
{
   if ((Options & (FSO::BOLD|FSO::ITALIC)) IS (FSO::BOLD|FSO::ITALIC)) return "Bold Italic";
   else if ((Options & FSO::BOLD) != FSO::NIL) return "Bold";
   else if ((Options & FSO::ITALIC) != FSO::NIL) return "Italic";
   else return "Regular";
}

//********************************************************************************************************************
// Creates a font (if it doesn't already exist) and returns an index.
//
// Created fonts belong to the Document module rather than the current object, so they can be reused between multiple
// open documents.

static LONG create_font(const std::string &Face, const std::string &Style, LONG Point)
{
   pf::Log log(__FUNCTION__);

   if (Point < 3) Point = DEFAULT_FONTSIZE;

   // Check the cache for this font

   for (unsigned i=0; i < glFonts.size(); i++) {
      if ((!StrMatch(Face, glFonts[i].font->Face)) and
          (!StrMatch(Style, glFonts[i].font->Style)) and
          (Point IS glFonts[i].point)) {
         log.trace("Match %d = %s(%s,%d)", i, Face.c_str(), Style.c_str(), Point);
         return i;
      }
   }

   log.branch("Index: %d, %s, %s, %d", LONG(glFonts.size()), Face.c_str(), Style.c_str(), Point);

   pf::LogLevel level(2);

   objFont *font = objFont::create::integral(
      fl::Owner(modDocument->UID), fl::Face(Face), fl::Style(Style), fl::Point(Point), fl::Flags(FTF::ALLOW_SCALE));

   if (font) {
      // Perform a second check in case the font we ended up with is in our cache.  This can occur if the font we have acquired
      // is a little different to what we requested (e.g. scalable instead of fixed, or a different face).

      for (unsigned i=0; i < glFonts.size(); i++) {
         if ((!StrMatch(font->Face, glFonts[i].font->Face)) and
            (!StrMatch(font->Style, glFonts[i].font->Style)) and
            (font->Point IS glFonts[i].point)) {
            log.trace("Match %d = %s(%s,%d)", i, Face.c_str(), Style.c_str(), Point);
            FreeResource(font);
            return i;
         }
      }

      auto index = glFonts.size();
      glFonts.emplace_back(font, Point);
      return index;
   }
   else {
      return -1;
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
// If a major error occurs during processing, the function will abort, returning the error and also setting the Error
// field to the resulting error code.  The most common reason for an abort is a buffer overflow or memory allocation
// problems, so a complete abort of document processing is advisable.
//
// RESERVED WORDS
//    %index       Current loop index, if within a repeat loop.
//    %id          A unique ID that is regenerated on each document refresh.
//    %self        ID of the document object.
//    %platform    Windows, Linux or Native.
//    %random      Random string of 9 digits.
//    %currentpage name of the current page.
//    %nextpage    name of the next page.
//    %prevpage    name of the previous page.
//    %path        Current working path.
//    %author      Document author.
//    %description Document description.
//    %copyright   Document copyright.
//    %keywords    Document keywords.
//    %title       Document title.
//    %font        Face, point size and style of the current font.
//    %fontface    Face of the current font.
//    %fontcolour  Colour of the current font.
//    %fontsize    Point size of the current font.
//    %lineno      The current 'line' (technically segmented line) in the document.
//    %content     Inject content (same as <inject/> but usable inside tag attributes)
//    %tm-day      The current day (0 - 31)
//    %tm-month    The current month (1 - 12)
//    %tm-year     The current year (2008+)
//    %tm-hour     The current hour (0 - 23)
//    %tm-minute   The current minute (0 - 59)
//    %tm-second   The current second (0 - 59)
//    %viewheight  Height of the document's available viewing area
//    %viewwidth   Width of the the document's available viewing area.

static void translate_args(extDocument *Self, const std::string &Input, std::string &Output)
{
   pf::Log log(__FUNCTION__);

   bool time_queried = false;
   Output = Input;

   // Do nothing if there are no special references being used

   {
      unsigned i;
      for (i=0; i < Input.size(); i++) {
         if (Input[i] IS '[') break;
         if ((Input[i] IS '&') and ((!StrCompare("&lsqr;", Input.c_str()+i)) or (!StrCompare("&rsqr;", Input.c_str()+i)))) break;
      }
      if (i >= Input.size()) return;
   }

   for (auto pos = signed(Output.size())-1; pos >= 0; pos--) {
      if (Output[pos] IS '&') {
         if (!StrCompare("&lsqr;", Output.c_str()+pos)) Output.replace(pos, 6, "[");
         else if (!StrCompare("&rsqr;", Output.c_str()+pos)) Output.replace(pos, 6, "]");
      }
      else if (Output[pos] IS '[') {
         if (Output[pos+1] IS '=') { // Perform a calcuation within [= ... ]
            std::string temp;
            temp.reserve(Output.size());
            unsigned j = 0;
            auto end = pos+2;
            while ((end < LONG(Output.size())) and (Output[end] != ']')) {
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
            if (end < LONG(Output.size())) end++; // Skip ']'
            std::string calcbuffer;
            calc(temp, 0, calcbuffer);
            Output.replace(pos, end-pos, calcbuffer);
         }
         else if (Output[pos+1] IS '%') {
            // Check against reserved keywords

            if (!Output.compare(pos, sizeof("[%index]")-1, "[%index]")) {
               Output.replace(pos, sizeof("[%index]")-1, std::to_string(Self->LoopIndex));
            }
            else if (!Output.compare(pos, sizeof("[%id]")-1, "[%id]")) {
               Output.replace(pos, sizeof("[%id]")-1, std::to_string(Self->GeneratedID));
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
            else if (!Output.compare(pos, sizeof("[%currentpage]")-1, "[%currentpage]")) {
               if (Self->PageTag) {
                  if (auto page_name = Self->PageTag[0].attrib("name")) {
                     Output.replace(pos, sizeof("[%currentpage]")-1, page_name[0]);
                  }
                  else Output.replace(pos, sizeof("[%currentpage]")-1, "");
               }
               else Output.replace(pos, sizeof("[%currentpage]")-1, "");
            }
            else if (!Output.compare(pos, sizeof("[%nextpage]")-1, "[%nextpage]")) {
               if (Self->PageTag) {
                  auto next = Self->PageTag->attrib("nextpage");
                  Output.replace(pos, sizeof("[%nextpage]")-1, next ? *next : "");
               }
            }
            else if (!Output.compare(pos, sizeof("[%prevpage]")-1, "[%prevpage]")) {
               if (Self->PageTag) {
                  auto next = Self->PageTag->attrib("prevpage");
                  Output.replace(pos, sizeof("[%prevpage]")-1, next ? *next : "");
               }
            }
            else if (!Output.compare(pos, sizeof("[%path]"), "[%path]")) {
               CSTRING workingpath = "";
               GET_WorkingPath(Self, &workingpath);
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
               if (auto font = Self->Style.font_style.get_font()) {
                  char buffer[60];
                  snprintf(buffer, sizeof(buffer), "%s:%.0f:%s", font->Face, font->Point, font->Style);
                  Output.replace(pos, sizeof("[%font]")-1, buffer);
               }
            }
            else if (!Output.compare(pos, sizeof("[%fontface]")-1, "[%fontface]")) {
               if (auto font = Self->Style.font_style.get_font()) {
                  Output.replace(pos, sizeof("[%fontface]")-1, font->Face);
               }
            }
            else if (!Output.compare(pos, sizeof("[%fontcolour]")-1, "[%fontcolour]")) {
               if (auto font = Self->Style.font_style.get_font()) {
                  char colour[28];
                  snprintf(colour, sizeof(colour), "#%.2x%.2x%.2x%.2x", font->Colour.Red, font->Colour.Green, font->Colour.Blue, font->Colour.Alpha);
                  Output.replace(pos, sizeof("[%fontcolour]")-1, colour);
               }
            }
            else if (!Output.compare(pos, sizeof("[%fontsize]")-1, "[%fontsize]")) {
               if (auto font = Self->Style.font_style.get_font()) {
                  char buffer[28];
                  snprintf(buffer, sizeof(buffer), "%.2f", font->Point);
                  Output.replace(pos, sizeof("[%fontsize]")-1, buffer);
               }
            }
            else if (!Output.compare(pos, sizeof("[%lineno]")-1, "[%lineno]")) {
               auto num = std::to_string(Self->Segments.size());
               Output.replace(pos, sizeof("[%lineno]")-1, num);
            }
            else if (!Output.compare(pos, sizeof("[%content]")-1, "[%content]")) {
               if ((Self->InTemplate) and (Self->InjectTag)) {
                  std::string content = xmlGetContent(Self->InjectTag[0][0]);
                  Output.replace(pos, sizeof("[%content]")-1, content);

                  //if (!xmlGetString(Self->InjectXML, Self->InjectTag[0][0].ID, XMF::INCLUDE_SIBLINGS, &content)) {
                  //   Output.replace(pos, sizeof("[%content]")-1, content);
                  //   FreeResource(content);
                  //}
               }
            }
            else if (!Output.compare(pos, sizeof("[%tm-day]")-1, "[%tm-day]")) {
               if (!Self->Time) Self->Time = objTime::create::integral();
               if (!time_queried) { acQuery(Self->Time); time_queried = true; }
               Output.replace(pos, sizeof("[%tm-day]")-1, std::to_string(Self->Time->Day));
            }
            else if (!Output.compare(pos, sizeof("[%tm-month]")-1, "[%tm-month]")) {
               if (!Self->Time) Self->Time = objTime::create::integral();
               if (!time_queried) { acQuery(Self->Time); time_queried = true; }
               Output.replace(pos, sizeof("[%tm-month]")-1, std::to_string(Self->Time->Month));
            }
            else if (!Output.compare(pos, sizeof("[%tm-year]")-1, "[%tm-year]")) {
               if (!Self->Time) Self->Time = objTime::create::integral();
               if (!time_queried) { acQuery(Self->Time); time_queried = true; }
               Output.replace(pos, sizeof("[%tm-year]")-1, std::to_string(Self->Time->Year));
            }
            else if (!Output.compare(pos, sizeof("[%tm-hour]")-1, "[%tm-hour]")) {
               if (!Self->Time) Self->Time = objTime::create::integral();
               if (!time_queried) { acQuery(Self->Time); time_queried = true; }
               Output.replace(pos, sizeof("[%tm-hour]")-1, std::to_string(Self->Time->Hour));
            }
            else if (!Output.compare(pos, sizeof("[%tm-minute]")-1, "[%tm-minute]")) {
               if (!Self->Time) Self->Time = objTime::create::integral();
               if (!time_queried) { acQuery(Self->Time); time_queried = true; }
               Output.replace(pos, sizeof("[%tm-minute]")-1, std::to_string(Self->Time->Minute));
            }
            else if (!Output.compare(pos, sizeof("[%tm-second]")-1, "[%tm-second]")) {
               if (!Self->Time) Self->Time = objTime::create::integral();
               if (!time_queried) { acQuery(Self->Time); time_queried = true; }
               Output.replace(pos, sizeof("[%tm-second]")-1, std::to_string(Self->Time->Second));
            }
            else if (!Output.compare(pos, sizeof("[%version]")-1, "[%version]")) {
               Output.replace(pos, sizeof("[%version]")-1, RIPPLE_VERSION);
            }
            else if (!Output.compare(pos, sizeof("[%viewheight]")-1, "[%viewheight]")) {
               char buffer[28];
               snprintf(buffer, sizeof(buffer), "%.0f", Self->Area.Height);
               Output.replace(pos, sizeof("[%viewheight]")-1, buffer);
            }
            else if (!Output.compare(pos, sizeof("[%viewwidth]")-1, "[%viewwidth]")) {
               char buffer[28];
               snprintf(buffer, sizeof(buffer), "%.0f", Self->Area.Width);
               Output.replace(pos, sizeof("[%viewwidth]")-1, buffer);
            }
         }
         else if (Output[pos+1] IS '@') {
            // Translate argument reference.
            // Valid examples: [@arg] [@arg:defaultvalue] [@arg:"value[]"] [@arg:'value[[]]]']

            char terminator = ']';
            auto end = Output.find_first_of("]:", pos);
            if (end IS std::string::npos) continue; // Not a valid reference

            auto argname = Output.substr(pos, end-pos);

            auto true_end = end;
            if ((Output[end] IS '\'') or (Output[end] IS '"')) {
               terminator = Output[end];
               for (++true_end; (true_end < Output.size()) and (Output[true_end] != '\''); true_end++);
               while ((true_end < Output.size()) and (Output[true_end] != ']')) true_end++;
            }

            bool processed = false;
            for (auto it=Self->TemplateArgs.rbegin(); (!processed) and (it != Self->TemplateArgs.rend()); it++) {
               auto args = *it;
               for (unsigned arg=1; arg < args->Attribs.size(); arg++) {
                  if (StrCompare(args->Attribs[arg].Name, argname)) continue;
                  Output.replace(pos, true_end-pos, args->Attribs[arg].Value);
                  processed = true;
                  break;
               }
            }

            if (processed) continue;

            // Check against global arguments / variables

            if (Self->Vars.contains(argname)) {
               Output.replace(pos, true_end-pos, Self->Vars[argname]);
            }
            else if (Self->Params.contains(argname)) {
               Output.replace(pos, true_end-pos, Self->Params[argname]);
            }
            else if (Output[end] IS ':') { // Resort to the default value
               end++;
               if ((Output[end] IS '\'') or (Output[end] IS '"')) {
                  end++;
                  auto start = end;
                  while ((end < Output.size()) and (Output[end] != terminator)) end++;
                  Output.replace(pos, true_end-pos, Output.substr(start, end));
               }
               else Output.replace(pos, true_end-pos, Output.substr(end, true_end));
            }
            else Output.replace(pos, true_end+1-pos, "");
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
               if (name == "self") {
                  // [self] can't be used in RIPPLE, because arguments are parsed prior to object
                  // creation.  We print a message to remind the developer of this rather than
                  // failing quietly.

                  log.warning("Self references are not permitted in RIPPLE.");
               }
               else if (name == "owner") {
                  if (Self->CurrentObject) objectid = Self->CurrentObject->UID;
               }
               else if (!FindObject(name.c_str(), 0, FOF::SMART_NAMES, &objectid)) {
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
                           if (auto classfield = FindField(object.obj, StrHash(fieldname), &target)) {
                              if (classfield->Flags & FD_STRING) {
                                 CSTRING str;
                                 if (!target->get(classfield->FieldID, &str)) Output.replace(pos, end-pos, str);
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
// Translate all arguments found in a list of XML attributes.

static void translate_attrib_args(extDocument *Self, pf::vector<XMLAttrib> &Attribs)
{
   if (Attribs[0].isContent()) return;

   for (unsigned attrib=1; (attrib < Attribs.size()); attrib++) {
      if (Attribs[attrib].Name.starts_with('$')) continue;

      std::string output;
      translate_args(Self, Attribs[attrib].Value, output);
      Attribs[attrib].Value = output;
   }
}

//********************************************************************************************************************
// Checks if an object reference is a valid member of the document.

static bool valid_object(extDocument *Self, OBJECTPTR Object)
{
   if ((Self->Flags & DCF::UNRESTRICTED) != DCF::NIL) return true;

   auto obj = Object;
   while (obj) {
      if (!obj->OwnerID) return false;
      if (obj->OwnerID < 0) return valid_objectid(Self, obj->UID); // Switch to scanning public objects
      obj = GetObjectPtr(obj->OwnerID);
      if (obj IS Self) return true;
   }
   return false;
}

//********************************************************************************************************************
//Checks if an object reference is a valid member of the document.

static bool valid_objectid(extDocument *Self, OBJECTID ObjectID)
{
   if ((Self->Flags & DCF::UNRESTRICTED) != DCF::NIL) return true;

   while (ObjectID) {
      ObjectID = GetOwnerID(ObjectID);
      if (ObjectID IS Self->UID) return true;
   }
   return false;
}

//********************************************************************************************************************

static LONG getutf8(CSTRING Value, LONG *Unicode)
{
   LONG i, len, code;

   if ((*Value & 0x80) != 0x80) {
      if (Unicode) *Unicode = *Value;
      return 1;
   }
   else if ((*Value & 0xe0) IS 0xc0) {
      len  = 2;
      code = *Value & 0x1f;
   }
   else if ((*Value & 0xf0) IS 0xe0) {
      len  = 3;
      code = *Value & 0x0f;
   }
   else if ((*Value & 0xf8) IS 0xf0) {
      len  = 4;
      code = *Value & 0x07;
   }
   else if ((*Value & 0xfc) IS 0xf8) {
      len  = 5;
      code = *Value & 0x03;
   }
   else if ((*Value & 0xfc) IS 0xfc) {
      len  = 6;
      code = *Value & 0x01;
   }
   else {
      // Unprintable character
      if (Unicode) *Unicode = 0;
      return 1;
   }

   for (i=1; i < len; ++i) {
      if ((Value[i] & 0xc0) != 0x80) code = -1;
      code <<= 6;
      code |= Value[i] & 0x3f;
   }

   if (code IS -1) {
      if (Unicode) *Unicode = 0;
      return 1;
   }
   else {
      if (Unicode) *Unicode = code;
      return len;
   }
}

//********************************************************************************************************************
// Find the nearest font style that will represent Char

static bc_font * find_style(extDocument *Self, const RSTREAM &Stream, stream_char &Char)
{
   bc_font *style = NULL;

   for (INDEX fi = Char.index; fi < Char.index; fi++) {
      if (Stream[fi].code IS SCODE::FONT) style = &stream_data<bc_font>(Self, fi);
      else if (Stream[fi].code IS SCODE::TEXT) break;
   }

   // Didn't work?  Try going backwards

   if (!style) {
      for (INDEX fi = Char.index; fi >= 0; fi--) {
         if (Stream[fi].code IS SCODE::FONT) {
            style = &stream_data<bc_font>(Self, fi);
            break;
         }
      }
   }

   return style;
}

//********************************************************************************************************************
// For a given line segment, convert a horizontal coordinate to the corresponding character index and its coordinate.

static ERROR resolve_font_pos(extDocument *Self, doc_segment &Segment, DOUBLE X, DOUBLE &CharX, stream_char &Char)
{
   pf::Log log(__FUNCTION__);

   bc_font *style = find_style(Self, Self->Stream, Char);
   auto font = style ? style->get_font() : glFonts[0].font;
   if (!font) return ERR_Search;

   for (INDEX i = Segment.start.index; i < Segment.stop.index; i++) {
      if (Self->Stream[i].code IS SCODE::TEXT) {
         auto &str = stream_data<bc_text>(Self, i).text;
         LONG offset, cx;
         if (!fntConvertCoords(font, str.c_str(), X - Segment.area.X, 0, NULL, NULL, NULL, &offset, &cx)) {
            CharX = cx;
            Char.set(Segment.start.index, offset);
            return ERR_Okay;
         }
         else break;
      }
   }

  log.trace("Failed to convert coordinate %d to a font-relative cursor position.", X);
   return ERR_Failed;
}

//********************************************************************************************************************
// Using only a stream index, this function will determine the x coordinate of the character at that index.  This is
// slower than resolve_font_pos(), because the segment has to be resolved by this function.

static ERROR resolve_fontx_by_index(extDocument *Self, stream_char Char, DOUBLE &CharX)
{
   pf::Log log("resolve_fontx");

   log.branch("Index: %d", Char.index);

   bc_font *style = find_style(Self, Self->Stream, Char);
   auto font = style ? style->get_font() : glFonts[0].font;
   if (!font) return log.warning(ERR_Search);

   // Find the segment linked to this character.  This is so that we can derive an x coordinate for the character
   // string.

   if (SEGINDEX segment = find_segment(Self, Char, true); segment >= 0) {
      auto i = Self->Segments[segment].start;
      while ((i <= Self->Segments[segment].stop) and (i < Char)) {
         if (Self->Stream[i.index].code IS SCODE::TEXT) {
            CharX = fntStringWidth(font, stream_data<bc_text>(Self, i).text.c_str(), -1);
            return ERR_Okay;
         }
         i.nextCode();
      }
   }

   log.warning("Failed to find a segment for index %d.", Char.index);
   return ERR_Search;
}

//********************************************************************************************************************
// For a given character in the stream, find its representative line segment.

static SEGINDEX find_segment(extDocument *Self, stream_char Char, bool InclusiveStop)
{
   if (InclusiveStop) {
      for (SEGINDEX segment=0; segment < SEGINDEX(Self->Segments.size()); segment++) {
         if ((Char >= Self->Segments[segment].start) and (Char <= Self->Segments[segment].stop)) {
            if ((Char IS Self->Segments[segment].stop) and (Char.get_prev_char(Self, Self->Stream) IS '\n'));
            else return segment;
         }
      }
   }
   else {
      for (SEGINDEX segment=0; segment < SEGINDEX(Self->Segments.size()); segment++) {
         if ((Char >= Self->Segments[segment].start) and (Char < Self->Segments[segment].stop)) {
            return segment;
         }
      }
   }

   return -1;
}

//********************************************************************************************************************
// scheme://domain.com/path?param1=value&param2=value#fragment:bookmark

static void process_parameters(extDocument *Self, const std::string &String)
{
   pf::Log log(__FUNCTION__);

   log.branch("%s", String.c_str());

   Self->Params.clear();
   Self->PageName.clear();
   Self->Bookmark.clear();

   std::string arg, value;
   arg.reserve(64);
   value.reserve(0xffff);

   bool pagename_processed = false;
   for (unsigned pos=0; pos < String.size(); pos++) {
      if ((String[pos] IS '#') and (!pagename_processed)) {
         // Reference is '#fragment:bookmark' where 'fragment' refers to a page in the loaded XML file and 'bookmark'
         // is an optional bookmark reference within that page.

         pagename_processed = true;

         if (auto ind = String.find(":", pos+1); ind != std::string::npos) {
            ind -= pos;
            Self->PageName.assign(String, pos + 1);
            if (Self->PageName[ind] IS ':') { // Check for bookmark separator
               Self->PageName.resize(ind);
               ind++;

               Self->Bookmark.assign(Self->PageName, ind);
               if (ind = Self->Bookmark.find('?'); ind != std::string::npos) {
                  Self->Bookmark.resize(ind);
               }
            }
         }
         else Self->PageName.assign(String, pos + 1);

         break;
      }
      else if (String[pos] IS '?') {
         // Arguments follow, separated by & characters for separation
         // Please note that it is okay to set zero-length parameter values

         pos++;

         auto uri_char = [&](std::string &Output) {
            if ((String[pos] IS '%') and
                (((String[pos+1] >= '0') and (String[pos+1] <= '9')) or
                 ((String[pos+1] >= 'A') and (String[pos+1] <= 'F')) or
                 ((String[pos+1] >= 'a') and (String[pos+1] <= 'f'))) and
                (((String[pos+2] >= '0') and (String[pos+2] <= '9')) or
                 ((String[pos+2] >= 'A') and (String[pos+2] <= 'F')) or
                 ((String[pos+2] >= 'a') and (String[pos+2] <= 'f')))) {
               Output += std::stoi(String.substr(pos+1, 2), nullptr, 16);
               pos += 3;
            }
            else Output += String[pos++];
         };

         while (pos < String.size()) {
            arg.clear();

            // Extract the parameter name

            while ((pos < String.size()) and (String[pos] != '#') and (String[pos] != '&') and (String[pos] != ';') and (String[pos] != '=')) {
               uri_char(arg);
            }

            if (String[pos] IS '=') { // Extract the parameter value
               value.clear();
               pos++;
               while ((pos < String.size()) and (String[pos] != '#') and (String[pos] != '&') and (String[pos] != ';')) {
                  uri_char(value);
               }
               Self->Params[arg] = value;
            }
            else Self->Params[arg] = "1";

            while ((String[pos]) and (String[pos] != '#') and (String[pos] != '&') and (String[pos] != ';')) pos++;
            if ((String[pos] != '&') and (String[pos] != ';')) break;
            pos++;
         }
      }
   }

   log.msg("Reset page name to '%s', bookmark '%s'", Self->PageName.c_str(), Self->Bookmark.c_str());
}

//********************************************************************************************************************
// Resolves function references.
// E.g. "script.function(Args...)"; "function(Args...)"; "function()", "function", "script.function"

static ERROR extract_script(extDocument *Self, const std::string &Link, OBJECTPTR *Script, std::string &Function, std::string &Args)
{
   pf::Log log(__FUNCTION__);

   if (Script) {
      if (!(*Script = Self->DefaultScript)) {
         if (!(*Script = Self->UserDefaultScript)) {
            log.warning("Cannot call function '%s', no default script in document.", Link.c_str());
            return ERR_Search;
         }
      }
   }

   auto pos = std::string::npos;
   auto dot = Link.find('.');
   auto open_bracket = Link.find('(');

   if (dot != std::string::npos) { // A script name is referenced
      pos = dot + 1;
      if (Script) {
         std::string script_name;
         script_name.assign(Link, 0, dot);
         OBJECTID id;
         if (!FindObject(script_name.c_str(), ID_SCRIPT, FOF::NIL, &id)) {
            // Security checks
            *Script = GetObjectPtr(id);
            if ((Script[0]->OwnerID != Self->UID) and ((Self->Flags & DCF::UNRESTRICTED) IS DCF::NIL)) {
               log.warning("Script '%s' does not belong to this document.  Request ignored due to security restrictions.", script_name.c_str());
               return ERR_NoPermission;
            }
         }
         else {
            log.warning("Unable to find '%s'", script_name.c_str());
            return ERR_Search;
         }
      }
   }
   else pos = 0;

   if ((open_bracket != std::string::npos) and (open_bracket < dot)) {
      log.warning("Malformed function reference: %s", Link.c_str());
      return ERR_InvalidData;
   }

   if (open_bracket != std::string::npos) {
      Function.assign(Link, pos, open_bracket-pos);
      if (auto end_bracket = Link.find_last_of(')'); end_bracket != std::string::npos) {
         Args.assign(Link, open_bracket + 1, end_bracket-1);
      }
      else log.warning("Malformed function args: %s", Link.c_str());
   }
   else Function.assign(Link, pos);

   return ERR_Okay;
}

//********************************************************************************************************************

void doc_link::exec(extDocument *Self)
{
   pf::Log log(__FUNCTION__);

   log.branch("");

   Self->Processing++;

   if ((base_code IS SCODE::LINK) and ((Self->EventMask & DEF::LINK_ACTIVATED) != DEF::NIL)) {
      link_activated params;
      auto link = as_link();

      if (link->type IS LINK::FUNCTION) {
         std::string function_name, args;
         if (!extract_script(Self, link->ref, NULL, function_name, args)) {
            params.Values["onclick"] = function_name;
         }
      }
      else if (link->type IS LINK::HREF) {
         params.Values["href"] = link->ref;
      }

      if (!link->args.empty()) {
         for (unsigned i=0; i < link->args.size(); i++) {
            params.Values[link->args[i].first] = link->args[i].second;
         }
      }

      ERROR result = report_event(Self, DEF::LINK_ACTIVATED, &params, "link_activated:Parameters");
      if (result IS ERR_Skip) goto end;
   }

   if (base_code IS SCODE::LINK) {
      OBJECTPTR script;
      std::string function_name, fargs;
      CLASSID class_id, subclass_id;

      auto link = as_link();
      if (link->type IS LINK::FUNCTION) { // function is in the format 'function()' or 'script.function()'
         if (!extract_script(Self, link->ref, &script, function_name, fargs)) {
            std::vector<ScriptArg> args;

            if (!link->args.empty()) {
               for (auto &arg : link->args) {
                  if (arg.first.starts_with('_')) { // Global variable setting
                     acSetVar(script, arg.first.c_str()+1, arg.second.c_str());
                  }
                  else args.emplace_back("", arg.second.data());
               }
            }

            scExec(script, function_name.c_str(), args.data(), args.size());
         }
      }
      else if (link->type IS LINK::HREF) {
         if (link->ref[0] IS ':') {
            Self->Bookmark = link->ref.substr(1);
            show_bookmark(Self, Self->Bookmark);
         }
         else {
            if ((link->ref[0] IS '#') or (link->ref[0] IS '?')) {
               log.trace("Switching to page '%s'", link->ref.c_str());

               if (!Self->Path.empty()) {
                  LONG end;
                  for (end=0; Self->Path[end]; end++) {
                     if ((Self->Path[end] IS '&') or (Self->Path[end] IS '#') or (Self->Path[end] IS '?')) break;
                  }
                  auto path = std::string(Self->Path, end) + link->ref;
                  Self->set(FID_Path, path);
               }
               else Self->set(FID_Path, link->ref);

               if (!Self->Bookmark.empty()) show_bookmark(Self, Self->Bookmark);
            }
            else {
               log.trace("Link is a file reference.");

               std::string path;

               if (!Self->Path.empty()) {
                  auto j = link->ref.find_first_of("/\\:");
                  if ((j IS std::string::npos) or (link->ref[j] != ':')) {
                     auto end = Self->Path.find_first_of("&#?");
                     if (end IS std::string::npos) path.assign(Self->Path);
                     else path.assign(Self->Path, 0, Self->Path.find_last_of("/\\", end) + 1);
                  }
               }

               auto lk = path + link->ref;
               auto end = lk.find_first_of("?#&");
               if (!IdentifyFile(lk.substr(0, end).c_str(), &class_id, &subclass_id)) {
                  if (class_id IS ID_DOCUMENT) {
                     Self->set(FID_Path, lk);

                     if (!Self->Bookmark.empty()) show_bookmark(Self, Self->Bookmark);
                     else log.msg("No bookmark was preset.");
                  }
               }
               else {
                  auto msg = std::string("It is not possible to follow this link as the type of file is not recognised.  The referenced link is:\n\n") + lk;
                  error_dialog("Action Cancelled", msg);
               }
            }
         }
      }
   }
   else if (base_code IS SCODE::CELL) {
      OBJECTPTR script;
      std::string function_name, script_args;

      auto cell = as_cell();
      if (!extract_script(Self, cell->onclick, &script, function_name, script_args)) {
         std::vector<ScriptArg> args;
         if (!cell->args.empty()) {
            for (auto &cell_arg : cell->args) {
               if (cell_arg.first.starts_with('_')) { // Global variable setting
                  acSetVar(script, cell_arg.first.c_str()+1, cell_arg.second.c_str());
               }
               else args.emplace_back("", cell_arg.second);
            }
         }

         scExec(script, function_name.c_str(), args.data(), args.size());
      }
   }
   else log.trace("Link index does not refer to a supported link type.");

end:
   Self->Processing--;
}

//********************************************************************************************************************

static void show_bookmark(extDocument *Self, const std::string &Bookmark)
{
   pf::Log log(__FUNCTION__);

   log.branch("%s", Bookmark.c_str());

   // Find the indexes for the bookmark name

   LONG start, end;
   if (!docFindIndex(Self, Bookmark.c_str(), &start, &end)) {
      // Get the vertical position of the index and scroll to it

      auto &esc_index = stream_data<bc_index>(Self, start);
      Self->scrollToPoint(0, esc_index.y - 4, 0, STP::Y);
   }
   else log.warning("Failed to find bookmark '%s'", Bookmark.c_str());
}

//********************************************************************************************************************

static ERROR report_event(extDocument *Self, DEF Event, APTR EventData, CSTRING StructName)
{
   pf::Log log(__FUNCTION__);
   ERROR result = ERR_Okay;

   if ((Event & Self->EventMask) != DEF::NIL) {
      log.branch("Reporting event $%.8x", LONG(Event));

      if (Self->EventCallback.Type IS CALL_STDC) {
         auto routine = (ERROR (*)(extDocument *, LARGE, APTR))Self->EventCallback.StdC.Routine;
         pf::SwitchContext context(Self->EventCallback.StdC.Context);
         result = routine(Self, LARGE(Event), EventData);
      }
      else if (Self->EventCallback.Type IS CALL_SCRIPT) {
         if (auto script = Self->EventCallback.Script.Script) {
            if ((EventData) and (StructName)) {
               ScriptArg args[3] = {
                  ScriptArg("Document", Self, FD_OBJECTPTR),
                  ScriptArg("EventMask", LARGE(Event)),
                  ScriptArg(StructName, EventData, FD_STRUCT)
               };
               scCallback(script, Self->EventCallback.Script.ProcedureID, args, 3, &result);
            }
            else {
               ScriptArg args[2] = {
                  ScriptArg("Document", Self, FD_OBJECTPTR),
                  ScriptArg("EventMask", LARGE(Event))
               };
               scCallback(script, Self->EventCallback.Script.ProcedureID, args, 2, &result);
            }
         }
      }
   }
   else log.trace("No subscriber for event $%.8x", (LONG)Event);

   return result;
}
