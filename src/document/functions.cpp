
static const LONG MAXLOOP = 1000;

static const char glDefaultStyles[] =
"<template name=\"h1\"><p leading=\"1.5\" font-face=\"Open Sans\" font-size=\"18\" font-style=\"bold\"><inject/></p></template>\n\
<template name=\"h2\"><p leading=\"1.5\" font-face=\"Open Sans\" font-size=\"16\" font-style=\"bold\"><inject/></p></template>\n\
<template name=\"h3\"><p leading=\"1.25\" font-face=\"Open Sans\" font-size=\"14\" font-style=\"bold\"><inject/></p></template>\n\
<template name=\"h4\"><p leading=\"1.25\" font-face=\"Open Sans\" font-size=\"14\"><inject/></p></template>\n\
<template name=\"h5\"><p leading=\"1.0\" font-face=\"Open Sans\" font-size=\"12\"><inject/></p></template>\n\
<template name=\"h6\"><p leading=\"1.0\" font-face=\"Open Sans\" font-size=\"10\"><inject/></p></template>\n";

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

constexpr DOUBLE fast_hypot(DOUBLE Width, DOUBLE Height)
{
   if (Width > Height) std::swap(Width, Height);
   if ((Height / Width) <= 1.5) return 5.0 * (Width + Height) / 7.0; // Fast hypot calculation accurate to within 1% for specific use cases.
   else return std::sqrt((Width * Width) + (Height * Height));
}

//********************************************************************************************************************

static bool read_rgb8(CSTRING Value, RGB8 *RGB)
{
   VectorPainter painter;
   if (!vecReadPainter(NULL, Value, &painter, NULL)) {
      RGB->Red   = F2T(painter.Colour.Red   * 255.0);
      RGB->Green = F2T(painter.Colour.Green * 255.0);
      RGB->Blue  = F2T(painter.Colour.Blue  * 255.0);
      RGB->Alpha = F2T(painter.Colour.Alpha * 255.0);
      return true;
   }
   else return false;
}

//********************************************************************************************************************
// Extract all printable text between start and End.

static std::string stream_to_string(RSTREAM &Stream, stream_char Start, stream_char End)
{
   if (End < Start) std::swap(Start, End);

   std::ostringstream str;
   auto cs = Start;
   for (; (cs.index <= End.index) and (cs.index < INDEX(Stream.size())); cs.next_code()) {
      if (Stream[cs.index].code IS SCODE::TEXT) {
         auto &text = Stream.lookup<bc_text>(cs);
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
// Designed for reading unit values such as '50%' and '6px'.  The returned value is scaled to pixels.

static CSTRING read_unit(CSTRING Input, DOUBLE &Output, bool &Scaled)
{
   bool isnumber = true;
   auto v = Input;
   while ((*v) and (unsigned(*v) <= 0x20)) v++;

   auto str = v;
   if ((*str IS '-') or (*str IS '+')) str++;

   Scaled = false;
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
         Scaled = true;
         multiplier = 0.01;
         str++;
      }
      else if ((str[0] IS 'p') and (str[1] IS 'x')) str += 2; // Pixel.  This is the default type
      else if ((str[0] IS 'e') and (str[1] IS 'm')) { str += 2; multiplier = 12.0 * (4.0 / 3.0); } // Current font-size
      else if ((str[0] IS 'e') and (str[1] IS 'x')) { str += 2; multiplier = 6.0 * (4.0 / 3.0); } // Current font-size, reduced to the height of the 'x' character.
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

//********************************************************************************************************************
// Process an XML tree by setting correct style information and then calling parse_tags().

static ERROR insert_xml(extDocument *Self, RSTREAM *Stream, objXML *XML, objXML::TAGS &Tag, INDEX TargetIndex,
   STYLE StyleFlags, IPF Options)
{
   pf::Log log(__FUNCTION__);

   if (TargetIndex < 0) TargetIndex = Stream->size();

   log.traceBranch("Index: %d, Flags: $%.2x, Tag: %s", TargetIndex, LONG(StyleFlags), Tag[0].Attribs[0].Name.c_str());

   if ((StyleFlags & STYLE::INHERIT_STYLE) != STYLE::NIL) { // Do nothing to change the style
      parser parse(Self, XML, Stream);
      parse.m_index = stream_char(TargetIndex);

      if (Stream->data.empty()) {
         parse.parse_tags(Tag, Options);
      }
      else {
         // Override the paragraph-content sanity check when inserting content in an existing document
         parse.m_paragraph_depth++;
         parse.parse_tags(Tag, Options);
         parse.m_paragraph_depth--;
      }
   }
   else {
      bc_font style;
      style.fill = Self->FontFill;

      if ((StyleFlags & STYLE::RESET_STYLE) != STYLE::NIL) {
         // Do not search for the most recent font style (force a reset)
      }
      else {
         for (auto i = TargetIndex - 1; i >= 0; i--) {
            if (Stream[0][i].code IS SCODE::FONT) {
               style = Stream->lookup<bc_font>(i);
               break;
            }
            else if (Stream[0][i].code IS SCODE::PARAGRAPH_START) {
               style = Stream->lookup<bc_paragraph>(i).font;
               break;
            }
            else if (Stream[0][i].code IS SCODE::LINK) {
               style = Stream->lookup<bc_link>(i).font;
               break;
            }
         }
      }

      // Revert to the default style if none is available.

      if (auto font = style.get_font()) {
         style.face  = font->Face;
         style.point = font->Point;
      }

      parser parse(Self, XML, Stream);
      parse.m_index = stream_char(TargetIndex);

      if (Stream->data.empty()) {
         parse.parse_tags_with_style(Tag, style, Options);
      }
      else {
         parse.m_paragraph_depth++;
         parse.parse_tags_with_style(Tag, style, Options);
         parse.m_paragraph_depth--;
      }
   }

   // Check that the FocusIndex is valid (there's a slim possibility that it may not be if AC_Focus has been
   // incorrectly used).

   if (Self->FocusIndex >= std::ssize(Self->Tabs)) Self->FocusIndex = -1;

   return ERR_Okay;
}

//********************************************************************************************************************
// This is the principal function for adding/inserting text into the document stream, whether that be in the parse
// phase or from user editing.
//
// Preformat must be set to true if all consecutive whitespace characters in Text are to be inserted.

static ERROR insert_text(extDocument *Self, RSTREAM *Stream, stream_char &Index, const std::string &Text, bool Preformat)
{
   // Check if there is content to be processed

   if ((!Preformat) and (Self->NoWhitespace)) {
      unsigned i;
      for (i=0; i < Text.size(); i++) if (unsigned(Text[i]) > 0x20) break;
      if (i IS Text.size()) return ERR_Okay;
   }

   if (Preformat) {
      bc_text et(Text, true);
      Stream->emplace<bc_text>(Index, et);
   }
   else {
      bc_text et;
      et.text.reserve(Text.size());
      auto ws = Self->NoWhitespace; // Retrieve previous whitespace state
      for (unsigned i=0; i < Text.size(); ) {
         if (unsigned(Text[i]) <= 0x20) { // Whitespace encountered
            for (++i; (unsigned(Text[i]) <= 0x20) and (i < Text.size()); i++);
            if (!ws) et.text += ' ';
            ws = true;
         }
         else {
            et.text += Text[i++];
            ws = false;
         }
      }
      Self->NoWhitespace = ws;
      Stream->emplace(Index, et);
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

      objXML::create xml = {
         fl::Flags(XMF::ALL_CONTENT|XMF::PARSE_HTML|XMF::STRIP_HEADERS|XMF::WELL_FORMED),
         fl::Path(Path),
         fl::ReadOnly(true)
      };

      if (xml.ok()) {
         #ifndef RETAIN_LOG_LEVEL
         pf::LogLevel level(3);
         #endif
         parser parse(Self, &Self->Stream);

         if (Self->PretextXML) {
            parse.process_page(Self->PretextXML);
         }

         parse.process_page(*xml);

         if (Self->initialised()) {
            Self->UpdatingLayout = true;
            redraw(Self, true);
         }

         #ifdef DBG_STREAM
            print_stream(Self->Stream);
         #endif
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
// This function removes all allocations that were made in displaying the current page, and resets a number of
// variables that they are at the default settings for the next page.
//
// Set ULD::TERMINATE if the document object is being destroyed.
//
// The PageName is not freed because the desired page must not be dropped during refresh of manually loaded XML for
// example.

static ERROR unload_doc(extDocument *Self, ULD Flags)
{
   pf::Log log(__FUNCTION__);

   log.branch("Flags: $%.2x", LONG(Flags));

   #ifdef DBG_STREAM
      print_stream(Self->Stream);
   #endif

   Self->Highlight = glHighlight;

   Self->CursorStroke   = "rgb(102,102,204,255)";
   Self->FontFill       = "rgb(0,0,0)";
   Self->LinkFill       = "rgb(0,0,255)";
   Self->LinkSelectFill = "rgb(255,0,0)";
   Self->Background     = "rgb(255,255,255,255)";

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
   Self->RelPageWidth  = false;
   Self->MinPageWidth  = MIN_PAGE_WIDTH;
   Self->DefaultScript = NULL;
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

   if (Self->SVG)         { FreeResource(Self->SVG); Self->SVG = NULL; }
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

   if (Self->Page) {
      Self->Page->setMask(NULL); // Reset the clipping mask if it was defined by <body>

      pf::vector<ChildEntry> list;
      if (!ListChildren(Self->Page->UID, &list)) {
         for (auto it=list.rbegin(); it != list.rend(); it++) FreeResource(it->ObjectID);
      }
   }

   if ((Self->View) and (Self->Page)) {
      // Client generated objects can appear in the view if <svg placement="background"/> was used.
      pf::vector<ChildEntry> list;
      if (!ListChildren(Self->View->UID, &list)) {
         for (auto it=list.rbegin(); it != list.rend(); it++) {
            if (it->ObjectID != Self->Page->UID) FreeResource(it->ObjectID);
         }
      }
   }

   Self->UIObjects.clear();

   if (Self->Page) acMoveToPoint(Self->Page, 0, 0, 0, MTF::X|MTF::Y);

   Self->NoWhitespace   = true;
   Self->UpdatingLayout = true;

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

inline std::string get_font_style(const FSO Options)
{
   if ((Options & (FSO::BOLD|FSO::ITALIC)) IS (FSO::BOLD|FSO::ITALIC)) return "Bold Italic";
   else if ((Options & FSO::BOLD) != FSO::NIL) return "Bold";
   else if ((Options & FSO::ITALIC) != FSO::NIL) return "Italic";
   else return "Regular";
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

static bc_font * find_style(RSTREAM &Stream, stream_char &Char)
{
   bc_font *style = NULL;

   for (INDEX fi = Char.index; fi < Char.index; fi++) {
      if (Stream[fi].code IS SCODE::FONT) style = &Stream.lookup<bc_font>(fi);
      else if (Stream[fi].code IS SCODE::PARAGRAPH_START) style = &Stream.lookup<bc_paragraph>(fi).font;
      else if (Stream[fi].code IS SCODE::LINK) style = &Stream.lookup<bc_link>(fi).font;
      else if (Stream[fi].code IS SCODE::TEXT) break;
   }

   // Didn't work?  Try going backwards

   if (!style) {
      for (INDEX fi = Char.index; fi >= 0; fi--) {
         if (Stream[fi].code IS SCODE::FONT) {
            style = &Stream.lookup<bc_font>(fi);
            break;
         }
         else if (Stream[fi].code IS SCODE::PARAGRAPH_START) {
            style = &Stream.lookup<bc_paragraph>(fi).font;
            break;
         }
         else if (Stream[fi].code IS SCODE::LINK) {
            style = &Stream.lookup<bc_link>(fi).font;
            break;
         }
      }
   }

   return style;
}

//********************************************************************************************************************
// For a given line segment, convert a horizontal coordinate to the corresponding character index and its coordinate.
/*
static ERROR resolve_font_pos(doc_segment &Segment, DOUBLE X, DOUBLE &CharX, stream_char &Char)
{
   pf::Log log(__FUNCTION__);

   bc_font *style = find_style(Segment.stream[0], Char);
   auto font = style ? style->get_font() : glFonts[0].font;
   if (!font) return ERR_Search;

   for (INDEX i = Segment.start.index; i < Segment.stop.index; i++) {
      if (Segment.stream[0][i].code IS SCODE::TEXT) {
         auto &str = Segment.stream->lookup<bc_text>(i).text;
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
*/
//********************************************************************************************************************
// For a given character in the stream, find its representative line segment.

static SEGINDEX find_segment(std::vector<doc_segment> &Segments, stream_char Char, bool InclusiveStop)
{
   if (InclusiveStop) {
      for (SEGINDEX segment=0; segment < SEGINDEX(Segments.size()); segment++) {
         if ((Char >= Segments[segment].start) and (Char <= Segments[segment].stop)) {
            if ((Char IS Segments[segment].stop) and (Char.get_prev_char(Segments[segment].stream[0]) IS '\n'));
            else return segment;
         }
      }
   }
   else {
      for (SEGINDEX segment=0; segment < SEGINDEX(Segments.size()); segment++) {
         if ((Char >= Segments[segment].start) and (Char < Segments[segment].stop)) {
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
               Self->Params.emplace(arg, value);
            }
            else Self->Params.emplace(arg, "1");

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

static ERROR extract_script(extDocument *Self, const std::string &Link, objScript **Script, std::string &Function, std::string &Args)
{
   pf::Log log(__FUNCTION__);

   if (Script) {
      if (!(*Script = Self->DefaultScript)) {
         if (!(*Script = Self->ClientScript)) {
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
            *Script = (objScript *)GetObjectPtr(id);
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

void ui_link::exec(extDocument *Self)
{
   objScript *script;
   CLASSID class_id, subclass_id;

   pf::Log log(__FUNCTION__);

   log.branch("");

   Self->Processing++;

   if ((Self->EventMask & DEF::LINK_ACTIVATED) != DEF::NIL) {
      KEYVALUE params;

      if (origin.type IS LINK::FUNCTION) {
         std::string function_name, args;
         if (!extract_script(Self, origin.ref, NULL, function_name, args)) {
            params.emplace("onclick", function_name);
         }
      }
      else if (origin.type IS LINK::HREF) {
         params.emplace("href", origin.ref);
      }

      for (size_t i=0; i < origin.args.size(); i++) {
         if ((origin.args[i].first[0] IS '@') or (origin.args[i].first[0] IS '$')) {
            params.emplace(origin.args[i].first.c_str()+1, origin.args[i].second);
         }
         else params.emplace(origin.args[i].first, origin.args[i].second);
      }

      ERROR result = report_event(Self, DEF::LINK_ACTIVATED, &origin, &params);
      if (result IS ERR_Skip) goto end;
   }

   if (origin.type IS LINK::FUNCTION) { // function is in the format 'function()' or 'script.function()'
      std::string function_name, fargs;
      if (!extract_script(Self, origin.ref, &script, function_name, fargs)) {
         std::vector<ScriptArg> sa;

         if (!origin.args.empty()) {
            for (auto &arg : origin.args) {
               if (arg.first.starts_with('_')) { // Global variable setting
                  acSetVar(script, arg.first.c_str()+1, arg.second.c_str());
               }
               else sa.emplace_back("", arg.second.data());
            }
         }

         scExec(script, function_name.c_str(), sa.data(), sa.size());
      }
   }
   else if (origin.type IS LINK::HREF) {
      if (origin.ref[0] IS ':') {
         Self->Bookmark = origin.ref.substr(1);
         show_bookmark(Self, Self->Bookmark);
      }
      else {
         if ((origin.ref[0] IS '#') or (origin.ref[0] IS '?')) {
            log.trace("Switching to page '%s'", origin.ref.c_str());

            if (!Self->Path.empty()) {
               LONG end;
               for (end=0; Self->Path[end]; end++) {
                  if ((Self->Path[end] IS '&') or (Self->Path[end] IS '#') or (Self->Path[end] IS '?')) break;
               }
               auto path = std::string(Self->Path, end) + origin.ref;
               Self->set(FID_Path, path);
            }
            else Self->set(FID_Path, origin.ref);

            if (!Self->Bookmark.empty()) show_bookmark(Self, Self->Bookmark);
         }
         else {
            log.trace("Link is a file reference.");

            std::string path;

            if (!Self->Path.empty()) {
               auto j = origin.ref.find_first_of("/\\:");
               if ((j IS std::string::npos) or (origin.ref[j] != ':')) { // Path is relative
                  auto end = Self->Path.find_first_of("&#?");
                  if (end IS std::string::npos) {
                     path.assign(Self->Path, 0, Self->Path.find_last_of("/\\") + 1);
                  }
                  else path.assign(Self->Path, 0, Self->Path.find_last_of("/\\", end) + 1);
               }
            }

            auto lk = path + origin.ref;
            auto end = lk.find_first_of("?#&");
            if (!IdentifyFile(lk.substr(0, end).c_str(), &class_id, &subclass_id)) {
               if (class_id IS ID_DOCUMENT) {
                  Self->set(FID_Path, lk);

                  if (!Self->Bookmark.empty()) show_bookmark(Self, Self->Bookmark);
                  else log.msg("No bookmark was preset.");
               }
            }
            else {
               auto msg = std::string("It is not possible to follow this link because the type of file is not recognised.  The referenced link is:\n\n") + lk;
               error_dialog("Action Cancelled", msg);
            }
         }
      }
   }

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

      auto &esc_index = Self->Stream.lookup<bc_index>(start);
      Self->scrollToPoint(0, esc_index.y - 4, 0, STP::Y);
   }
   else log.warning("Failed to find bookmark '%s'", Bookmark.c_str());
}

//********************************************************************************************************************
// Generic function for reporting events that relate to entities.

static ERROR report_event(extDocument *Self, DEF Event, entity *Entity, KEYVALUE *EventData)
{
   pf::Log log(__FUNCTION__);
   ERROR result = ERR_Okay;

   if ((Event & Self->EventMask) != DEF::NIL) {
      log.traceBranch("Event $%x -> Entity %d", LONG(Event), Entity->uid);

      if (Self->EventCallback.isC()) {
         auto routine = (ERROR (*)(extDocument *, DEF, KEYVALUE *, entity *, APTR))Self->EventCallback.StdC.Routine;
         pf::SwitchContext context(Self->EventCallback.StdC.Context);
         result = routine(Self, Event, EventData, Entity, Self->EventCallback.StdC.Meta);
      }
      else if (Self->EventCallback.isScript()) {
         if (auto script = Self->EventCallback.Script.Script) {
            if (EventData) {
               ScriptArg args[] = {
                  { "Document", Self, FD_OBJECTPTR },
                  { "EventMask", LONG(Event) },
                  { "KeyValue:Parameters", EventData, FD_STRUCT },
                  { "Entity", Entity->uid }
               };
               scCallback(script, Self->EventCallback.Script.ProcedureID, args, std::ssize(args), &result);
            }
            else {
               ScriptArg args[] = {
                  { "Document", Self, FD_OBJECTPTR },
                  { "EventMask", LONG(Event) },
                  { "KeyValue", LONG(0) },
                  { "Entity", Entity->uid }
               };
               scCallback(script, Self->EventCallback.Script.ProcedureID, args, std::ssize(args), &result);
            }
         }
      }
   }
   else log.trace("No subscriber for event $%.8x", (LONG)Event);

   return result;
}

//********************************************************************************************************************
// Set padding values in clockwise order.  For percentages, the final value is calculated from the diagonal of the
// parent.

void padding::parse(const std::string &Value)
{
   auto str = Value.c_str();
   str = read_unit(str, left, left_pct);

   if (*str) str = read_unit(str, top, top_pct);
   else { top = left; top_pct = left_pct; }

   if (*str) str = read_unit(str, right, right_pct);
   else { right = top; right_pct = top_pct; }

   if (*str) str = read_unit(str, bottom, bottom_pct);
   else { bottom = right; bottom_pct = right_pct; }

   configured = true;
}
