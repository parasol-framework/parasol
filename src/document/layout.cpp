
// State machine for the layout proces

struct layout {
   struct link_marker {
      DOUBLE x;           // Starting coordinate of the link.  Can change if the link is split across multiple lines.
      DOUBLE word_width;  // Reflects the m_word_width value at the moment of a link's termination.
      INDEX index;
      ALIGN align;

      link_marker(DOUBLE pX, INDEX pIndex, ALIGN pAlign) : x(pX), word_width(0), index(pIndex), align(pAlign) { }
   };

   std::stack<bc_list *>      stack_list;
   std::stack<bc_row *>       stack_row;
   std::stack<bc_paragraph *> stack_para;
   std::stack<bc_link *>      stack_link; // Set by procLink() and remains until procLinkend()
   std::stack<link_marker>   stack_mklink; // Maintains link placement information.  Stack matches that of stack_link.

   std::vector<doc_link>    m_links;
   std::vector<doc_clip>    m_clips;
   std::vector<doc_segment> m_segments;
   std::vector<edit_cell>  m_ecells;

   extDocument *Self;
   objFont *m_font;

   INDEX idx;               // Current seek position for processing of the stream
   stream_char m_word_index; // Position of the word currently being operated on
   LONG m_align_flags;      // Current alignment settings according to the font style
   LONG m_align_width;      // Horizontal alignment will be calculated relative to this value
   DOUBLE m_cursor_x, m_cursor_y; // Insertion point of the next text character or vector object
   DOUBLE m_page_width;
   LONG m_kernchar;         // Previous character of the word being operated on
   LONG m_left_margin, m_right_margin; // Margins control whitespace for paragraphs and table cells
   LONG m_paragraph_bottom; // Bottom Y coordinate of the current paragraph; defined on paragraph end.
   LONG m_paragraph_y;      // The vertical position of the current paragraph
   LONG m_split_start;      // Set to the previous line index if the line is segmented.  Used for ensuring that all distinct entries on the line use the same line height
   LONG m_word_width;       // Pixel width of the current word
   LONG m_wrap_edge;        // Marks the boundary at which graphics and text will need to wrap.
   WORD m_space_width;      // Caches the pixel width of a single space in the current font.
   WORD m_terminate_link;   // Incremented whenever a link in stack_link requires termination.
   bool m_inline;           // Set to true when graphics (vectors, images) must be inline.
   bool m_no_wrap;          // Set to true when word-wrap is disabled.
   bool m_text_content;     // Set to true whenever text is encountered (inc. whitespace).  Resets on segment breaks.

   struct {
      stream_char index;   // Stream position for the line's content.
      DOUBLE gutter;      // Amount of vertical spacing appropriated for text.  Inclusive within the height value, not additive
      DOUBLE height;      // The complete height of the line, including inline vectors/images/tables.  Text is drawn so that the text gutter is aligned to the base line
      DOUBLE x;           // Starting horizontal position
      DOUBLE word_height; // Height of the current word (including inline graphics), utilised for word wrapping

      void reset(DOUBLE LeftMargin) {
         x      = LeftMargin;
         gutter = 0;
         height = 0;
      }

      void full_reset(DOUBLE LeftMargin) {
         reset(LeftMargin);
         word_height = 0;
      }

      void apply_word_height() {
         if (word_height > height) height = word_height;
         word_height = 0;
      }

   } m_line;

   void reset() {
      m_clips.clear();
      m_ecells.clear();
      m_segments.clear();
      m_links.clear();

      stack_list   = {};
      stack_para   = {};
      stack_row    = {};
      stack_link   = {};
      stack_mklink = {};

      m_terminate_link   = 0;
      m_align_flags      = 0;
      m_paragraph_y      = 0;
      m_paragraph_bottom = 0;
      m_word_width       = 0;
      m_kernchar         = 0;
      m_inline           = false;
      m_no_wrap          = false;
      m_text_content     = false;
   }

   // Resets the string management variables, usually done when a string
   // has been broken up on the current line due to a vector or table graphic for example.

   void reset_segment(INDEX Index, LONG X) {
      m_word_index.reset();

      m_line.index.set(Index, 0);
      m_line.x       = X;
      m_kernchar     = 0;
      m_word_width   = 0;
      m_text_content = false;
   }

   void reset_segment() { reset_segment(idx, m_cursor_x); }

   // Add a drawable segment for a single byte code at position idx.  This will not include support for text glyphs,
   // so no supplementary information such as x/y coordinates is defined.

   void add_esc_segment() {
      stream_char start(idx, 0);
      stream_char stop(idx + 1, 0);
      add_drawsegment(start, stop, m_cursor_y, 0, 0, BC_NAME(Self->Stream, idx));
      reset_segment(idx+1, m_cursor_x);
   }

   // Return true if an escape code is capable of breaking a word.

   bool breakable_word() {
      switch (Self->Stream[idx].code) {
         case SCODE::ADVANCE:
         case SCODE::TABLE_START:
            return true;

         case SCODE::VECTOR:
         case SCODE::IMAGE:
            // Graphics don't break words.  Either the graphic is floating (therefore its presence has no impact)
            // or it is inline, and therefore treated like a character.
            break;

         case SCODE::FONT:
            // Font style changes don't breakup text unless there's a face change.
            //if (m_text_content) {
            //   auto &style = stream_data<bc_font>(Self, idx);
            //   if (m_font != style.get_font()) return true;
            //}
            break;

         case SCODE::INDEX_START: {
            auto &index = stream_data<bc_index>(Self, idx);
            if (!index.visible) return true;
         }

         default: break;
      }
      return false;
   }

   // If the current font is larger or equal to the current line height, extend the line height.
   // Note that we use >= because we want to correct the base line in case there is a vector already set on the
   // line that matches the font's line spacing.

   void check_line_height() {
      if (m_font->LineSpacing >= m_line.height) {
         m_line.height = m_font->LineSpacing;
         m_line.gutter = m_font->LineSpacing - m_font->Ascent;
      }
   }

   layout(extDocument *pSelf) : Self(pSelf) { }

   INDEX do_layout(INDEX, INDEX, objFont **, LONG, LONG, DOUBLE &, DOUBLE &, ClipRectangle, bool &);

   void gen_scene_graph();

   void procSetMargins(LONG, LONG &);
   void procLink();
   void procLinkEnd();
   void procIndexStart();
   void procFont();
   WRAP procVector(LONG, DOUBLE, DOUBLE, LONG, bool &, bool &);
   void procParagraphStart();
   void procParagraphEnd();
   TE procTableEnd(bc_table *, LONG, LONG, LONG, LONG, DOUBLE &, DOUBLE &);
   void procCellEnd(bc_cell *);
   void procRowEnd(bc_table *);
   void procAdvance();
   bool procListEnd();
   WRAP procText(LONG);
   WRAP procImage(LONG);

   void terminate_link();
   void add_link(SCODE, std::variant<bc_link *, bc_cell *>, DOUBLE, DOUBLE, DOUBLE, DOUBLE, const std::string &);
   void add_drawsegment(stream_char, stream_char, DOUBLE, DOUBLE, DOUBLE, const std::string &);
   void end_line(NL, DOUBLE, stream_char, const std::string &);
   WRAP check_wordwrap(const std::string &, LONG, DOUBLE &, stream_char, DOUBLE &, DOUBLE &, LONG, LONG);
   void wrap_through_clips(stream_char, DOUBLE &, DOUBLE &, LONG, LONG);
   DOUBLE calc_page_height(DOUBLE, DOUBLE);
};

//********************************************************************************************************************

void layout::procAdvance()
{
   auto adv = &stream_data<bc_advance>(Self, idx);
   m_cursor_x += adv->x;
   m_cursor_y += adv->y;
   if (adv->x) reset_segment();
}

//********************************************************************************************************************
// Calculate the image position.  The host rectangle is modified in gen_scene_graph() as this is the most optimal
// approach (i.e. if the page width expands during layout).
//
// NOTE: If you ever see an image unexpectedly appearing at (0,0) it's because it hasn't been included in a draw
// segment.

WRAP layout::procImage(LONG AbsX)
{
   auto &image = stream_data<bc_image>(Self, idx);

   if (!image.floating()) check_line_height(); // Necessary for inline images in case they are the first 'character' on the line.

   // Calculate the final width and height.

   if (image.width_pct) {
      image.final_width = image.width * (m_page_width - m_left_margin - m_right_margin);
   }
   else if (!image.width) {
      if (image.height) {
         if (image.height_pct) {
            if (image.floating()) image.final_width = image.height * (m_page_width - m_left_margin - m_right_margin);
            else image.final_width = image.height * m_font->Ascent;
         }
         else image.final_width = image.height;
      }
      else image.final_width = m_font->Ascent;
   }
   else image.final_width = image.width;

   if (image.height_pct) {
      if (image.floating()) image.final_height = image.height * (m_page_width - m_left_margin - m_right_margin);
      else image.final_height = image.height * m_font->Ascent;
   }
   else if (!image.height) {
      if (image.floating()) image.final_height = image.final_width;
      else image.final_height = m_font->Ascent;
   }
   else image.final_height = image.height;

   if (image.final_height < 0.01) image.final_height = 0.01;
   if (image.final_width < 0.01) image.final_width = 0.01;

   if (image.padding) {
      auto hypot = fast_hypot(image.final_width, image.final_height);
      image.final_pad.left   = image.pad.left_pct ? (image.pad.left * hypot) : image.pad.left;
      image.final_pad.top    = image.pad.top_pct ? (image.pad.top * hypot) : image.pad.top;
      image.final_pad.right  = image.pad.right_pct ? (image.pad.right * hypot) : image.pad.right;
      image.final_pad.bottom = image.pad.bottom_pct ? (image.pad.bottom * hypot) : image.pad.bottom;
   }

   auto wrap_result = WRAP::DO_NOTHING;

   if (image.floating()) {
      // Calculate horizontal position

      if ((image.align & ALIGN::LEFT) != ALIGN::NIL) {
         image.x = m_left_margin;
      }
      else if ((image.align & ALIGN::CENTER) != ALIGN::NIL) {
         // We use the left margin and not the cursor for calculating the center because the image is floating.
         image.x = m_left_margin + ((m_align_width - (image.final_width + image.final_pad.left + image.final_pad.right)) * 0.5);
      }
      else if ((image.align & ALIGN::RIGHT) != ALIGN::NIL) {
         image.x = m_align_width - (image.final_width + image.final_pad.left + image.final_pad.right);
      }
      else image.x = m_cursor_x;

      add_esc_segment();

      // For a floating image we need to declare a clip region based on the final image dimensions.
      // TODO: Add support for masked clipping through SVG paths.

      m_clips.emplace_back(image.x, m_cursor_y,
         image.x + image.final_pad.left + image.final_width + image.final_pad.right,
         m_cursor_y + image.final_pad.top + image.final_height + image.final_pad.bottom,
         idx, false, "Image");
   }
   else { // Image is inline and must be treated like a text character.
      if (!m_word_width) m_word_index.set(idx, idx); // Save the index of the new word

      // Checking for wordwrap here is optimal, BUT bear in mind that if characters immediately follow the
      // image then it is also possible for word-wrapping to occur later.  Note that the line height isn't
      // adjusted in this call because if a wrap occurs then the image won't be in the former segment.

      wrap_result = check_wordwrap("Image", AbsX, m_page_width, m_word_index,
         m_cursor_x, m_cursor_y, m_word_width + image.full_width(), m_line.height);

      // The inline image will probably increase the height of the line, but due to the potential for delayed
      // word-wrapping (if we're part of an embedded word) we need to cache the value for now.

      if (image.full_height() > m_line.word_height) m_line.word_height = image.full_height();

      m_word_width += image.full_width();
      m_kernchar   = 0;
   }

   return wrap_result;
}

//********************************************************************************************************************

void layout::procFont()
{
   pf::Log log;
   auto style = &stream_data<bc_font>(Self, idx);
   m_font = style->get_font();

   if (m_font) {
      if ((style->options & FSO::ALIGN_RIGHT) != FSO::NIL) m_font->Align = ALIGN::RIGHT;
      else if ((style->options & FSO::ALIGN_CENTER) != FSO::NIL) m_font->Align = ALIGN::HORIZONTAL;
      else m_font->Align = ALIGN::NIL;

      m_inline = ((style->options & FSO::IN_LINE) != FSO::NIL);
      m_no_wrap = ((style->options & FSO::NO_WRAP) != FSO::NIL);
      //if (m_no_wrap) m_wrap_edge = 1000;

      DLAYOUT("Font Index: %d, LineSpacing: %d, Pt: %.2f, Height: %d, Ascent: %d, Cursor: %.2fx%.2f",
         style->font_index, m_font->LineSpacing, m_font->Point, m_font->Height, m_font->Ascent, m_cursor_x, m_cursor_y);
      m_space_width = fntCharWidth(m_font, ' ', 0, 0);

      // Treat the font as if it is a text character by setting the m_word_index.
      // This ensures it is included in the drawing process.

      if (!m_word_width) m_word_index.set(idx, 0);
   }
   else DLAYOUT("ESC_FONT: Unable to lookup font using style index %d.", style->font_index);
}

//********************************************************************************************************************
// NOTE: Bear in mind that the first word in a TEXT string could be a direct continuation of a previous TEXT word.
// This can occur if the font colour is changed mid-word for example.

WRAP layout::procText(LONG AbsX)
{
   auto wrap_result = WRAP::DO_NOTHING; // Needs to to change to WRAP::EXTEND_PAGE if a word is > width

   m_align_width = m_wrap_edge; // TODO: Not sure about this following the switch to embedded TEXT structures

   auto ascent = m_font->Ascent;
   auto &text = stream_data<bc_text>(Self, idx);
   auto &str = text.text;
   for (unsigned i=0; i < str.size(); ) {
      if (str[i] IS '\n') { // The use of '\n' in a string forces a line break
#if 0
      // This link code is likely going to be needed for a case such as :
      //   <a href="">blah blah <br/> blah </a>
      // But we haven't tested it in a document yet.

      if ((!stack_link.empty()) and (m_link.open IS false)) {
         // A link is due to be closed
         add_link(SCODE::LINK, link, link_x, m_cursor_y, m_cursor_x + m_word_width - link_x, m_line.height, "<br/>");
         stack_link.pop();
      }
#endif
         check_line_height();
         wrap_result = check_wordwrap("Text", AbsX, m_page_width, m_word_index, m_cursor_x, m_cursor_y, m_word_width,
            (m_line.height < 1) ? 1 : m_line.height);
         if (wrap_result IS WRAP::EXTEND_PAGE) break;

         stream_char end(idx, i);
         end_line(NL::PARAGRAPH, 0, end, "CR");
         i++;
      }
      else if (str[i] <= 0x20) { // Whitespace encountered
         check_line_height();

         if (m_word_width) { // Existing word finished, check if it will wordwrap
            wrap_result = check_wordwrap("Text", AbsX, m_page_width, m_word_index, m_cursor_x, m_cursor_y,
               m_word_width, (m_line.height < 1) ? 1 : m_line.height);
            if (wrap_result IS WRAP::EXTEND_PAGE) break;
         }

         m_line.apply_word_height();

         if (str[i] IS '\t') {
            auto tabwidth = (m_space_width + m_font->GlyphSpacing) * m_font->TabSize;
            if (tabwidth) {
               m_cursor_x += (m_cursor_x + tabwidth) - std::fmod(m_cursor_x, tabwidth); // Round up to Alignment value, e.g. (14,8) = 16
            }
         }
         else m_cursor_x += m_word_width + m_space_width;

         // Current word state must be reset.
         m_kernchar     = 0;
         m_word_width   = 0;
         m_text_content = true;
         i++;
      }
      else {
         if (!m_word_width) {
            m_word_index.set(idx, i);   // Save the index of the new word
            check_line_height();
         }

         LONG unicode, kerning;
         i += getutf8(str.c_str()+i, &unicode);
         m_word_width  += fntCharWidth(m_font, unicode, m_kernchar, &kerning);
         m_word_width  += kerning;
         m_kernchar     = unicode;
         m_text_content = true;

         if (ascent > m_line.word_height) m_line.word_height = ascent;
      }
   }

   // Entire text string has been processed, perform one final wrapping check.

   if (m_word_width) {
      wrap_result = check_wordwrap("Text", AbsX, m_page_width, m_word_index, m_cursor_x, m_cursor_y,
         m_word_width, (m_line.height < 1) ? 1 : m_line.height);
   }

   return wrap_result;
}

//********************************************************************************************************************

void layout::terminate_link()
{
   m_terminate_link--;
   if (stack_link.empty()) return;

   add_link(SCODE::LINK, stack_link.top(), stack_mklink.top().x, m_cursor_y,
      m_cursor_x + stack_mklink.top().word_width - stack_mklink.top().x,
      m_line.height ? m_line.height : m_font->LineSpacing, "link_end");
   stack_link.pop();
   stack_mklink.pop();

   if (!stack_link.empty()) { // Nested link detected, reset the x starting point
      stack_mklink.top().x = m_cursor_x + stack_mklink.top().word_width;
   }
}

void layout::procLink()
{
   if (!stack_link.empty()) {
      // Nested link detected.  Close the current link.  Use of the stack means it will be reopened when
      // the nested link is closed.

      add_link(SCODE::LINK, stack_link.top(), stack_mklink.top().x, m_cursor_y,
         m_cursor_x + stack_mklink.top().word_width - stack_mklink.top().x,
         m_line.height ? m_line.height : m_font->LineSpacing, "link_start");
   }

   stack_link.push(&stream_data<::bc_link>(Self, idx));

   stack_mklink.emplace(m_cursor_x + m_word_width, idx, m_font->Align);
}

void layout::procLinkEnd()
{
   if (stack_link.empty()) return;

   // We can't terminate links here due to word-wrapping concerns, so instead we increment a counter to indicate that
   // a link is due for termination.  Search for m_terminate_link to see where link termination actually occurs.

   // The current m_word_width value is saved here because links can end in the middle of words.

   stack_mklink.top().word_width = m_word_width;
   m_terminate_link++;
}

//********************************************************************************************************************
// Returns true if a repass is required

bool layout::procListEnd()
{
   if (stack_list.empty()) return false;

   // If it is a custom list, a repass may be required

   if ((stack_list.top()->type IS bc_list::CUSTOM) and (stack_list.top()->repass)) {
      return true;
   }

   stack_list.pop();

   if (stack_list.empty()) {
      // At the end of a list, increase the whitespace to that of a standard paragraph.
      stream_char sc(idx, 0);
      if (!stack_para.empty()) end_line(NL::PARAGRAPH, stack_para.top()->vspacing, sc, "ListEnd");
      else end_line(NL::PARAGRAPH, 1.0, sc, "ListEnd");
   }

   return false;
}

//********************************************************************************************************************
// Indexes don't do anything, but recording the cursor's y value when they are encountered
// makes it really easy to scroll to a bookmark when requested (show_bookmark()).

void layout::procIndexStart()
{
   pf::Log log(__FUNCTION__);

   auto escindex = &stream_data<bc_index>(Self, idx);
   escindex->y = m_cursor_y;

   if (!escindex->visible) {
      // If not visible, all content within the index is not to be displayed

      auto end = idx;
      while (end < INDEX(Self->Stream.size())) {
         if (Self->Stream[end].code IS SCODE::INDEX_END) {
            bc_index_end &iend = stream_data<bc_index_end>(Self, end);
            if (iend.id IS escindex->id) break;
            end++;

            // Do some cleanup to complete the content skip.

            m_line.index.set(end, 0);
            idx = end;
            return;
         }

         end++;
      }

      log.warning("Failed to find matching index-end.  Document stream is corrupt.");
   }
}

//********************************************************************************************************************

void layout::procCellEnd(bc_cell *esccell)
{
   // CELL_END helps draw(), so set the segment to ensure that it is included in the draw stream.  Please
   // refer to SCODE::CELL to see how content is processed and how the cell dimensions are formed.

   if ((esccell) and (!esccell->onclick.empty())) {
      add_link(SCODE::CELL, esccell, esccell->abs_x, esccell->abs_y, esccell->width, esccell->height, "esc_cell_end");
   }

   if ((esccell) and (!esccell->edit_def.empty())) {
      // The area of each edit cell is logged for assisting interaction between the mouse pointer and the cells.

      m_ecells.emplace_back(esccell->cell_id, esccell->abs_x, esccell->abs_y, esccell->width, esccell->height);
   }

   add_esc_segment();
}

//********************************************************************************************************************

void layout::procRowEnd(bc_table *Table)
{
   pf::Log log;

   auto Row = stack_row.top();
   Table->row_index++;

   // Increase the table height if the row extends beyond it

   auto j = Row->y + Row->row_height + Table->cell_vspacing;
   if (j > Table->y + Table->height) {
      Table->height = j - Table->y;
   }

   // Advance the cursor by the height of this row

   m_cursor_y += Row->row_height + Table->cell_vspacing;
   m_cursor_x = Table->x;
   DLAYOUT("Row ends, advancing down by %d+%d, new height: %d, y-cursor: %.2f",
      Row->row_height, Table->cell_vspacing, Table->height, m_cursor_y);

   if (Table->row_width > Table->width) Table->width = Table->row_width;

   stack_row.pop();
   add_esc_segment();
}

//********************************************************************************************************************

void layout::procParagraphStart()
{
   if (!stack_para.empty()) {
      DOUBLE ratio;

      // If a paragraph is embedded within a paragraph, insert a newline before the new paragraph starts.

      m_left_margin = stack_para.top()->x; // Reset the margin so that the next line will be flush with the parent

      if (m_paragraph_y > 0) {
         if (stack_para.top()->leading_ratio > stack_para.top()->vspacing) ratio = stack_para.top()->leading_ratio;
         else ratio = stack_para.top()->vspacing;
      }
      else ratio = stack_para.top()->vspacing;

      stream_char sc(idx, 0);
      end_line(NL::PARAGRAPH, ratio, sc, "PS");

      stack_para.push(&stream_data<bc_paragraph>(Self, idx));
   }
   else {
      stack_para.push(&stream_data<bc_paragraph>(Self, idx));

      // Leading ratio is only used if the paragraph is preceeded by content.
      // This check ensures that the first paragraph is always flush against
      // the top of the page.

      if ((stack_para.top()->leading_ratio > 0) and (m_paragraph_y > 0)) {
         stream_char sc(idx, 0);
         end_line(NL::PARAGRAPH, stack_para.top()->leading_ratio, sc, "PS");
      }
   }

   auto escpara = stack_para.top();

   if (!stack_list.empty()) {
      // If a paragraph is inside a list then it's treated as a list item.
      // Indentation values are inherited from the list.

      auto list = stack_list.top();
      if (escpara->list_item) {
         if (stack_para.size() > 1) escpara->indent = list->block_indent;
         escpara->item_indent = list->item_indent;
         escpara->relative = false;

         if (!escpara->value.empty()) {
            auto strwidth = fntStringWidth(m_font, escpara->value.c_str(), -1) + 10;
            if (strwidth > list->item_indent) {
               list->item_indent     = strwidth;
               escpara->item_indent = strwidth;
               list->repass         = true;
            }
         }
      }
      else escpara->indent = list->item_indent;
   }

   if (escpara->indent) {
      if (escpara->relative) escpara->block_indent = escpara->indent * (0.01 * m_page_width);
      else escpara->block_indent = escpara->indent;
   }

   escpara->x = m_left_margin + escpara->block_indent;

   m_left_margin += escpara->block_indent + escpara->item_indent;
   m_cursor_x    += escpara->block_indent + escpara->item_indent;
   m_line.x      += escpara->block_indent + escpara->item_indent;

   // Paragraph management variables

   if (!stack_list.empty()) escpara->vspacing = stack_list.top()->vspacing;

   escpara->y = m_cursor_y;
   escpara->height = 0;
}

//********************************************************************************************************************

void layout::procParagraphEnd()
{
   stream_char sc(idx + 1, 0);
   if (!stack_para.empty()) {
      // The paragraph height reflects the true size of the paragraph after we take into account
      // any vectors and tables within the paragraph.

      auto para = stack_para.top();
      m_paragraph_bottom = para->y + para->height;

      end_line(NL::PARAGRAPH, para->vspacing, sc, "PE");

      m_left_margin = para->x - para->block_indent;
      m_cursor_x    = para->x - para->block_indent;
      m_line.x      = para->x - para->block_indent;
      stack_para.pop();
   }
   else end_line(NL::PARAGRAPH, 0, sc, "PE-NP"); // Technically an error when there's no matching PS code.
}

//********************************************************************************************************************

TE layout::procTableEnd(bc_table *esctable, LONG Offset, LONG AbsX, LONG TopMargin, LONG BottomMargin, DOUBLE &Height, DOUBLE &Width)
{
   pf::Log log(__FUNCTION__);

   ClipRectangle clip;
   LONG minheight;

   if (esctable->cells_expanded IS false) {
      LONG unfixed, colwidth;

      // Table cells need to match the available width inside the table.  This routine checks for that - if the cells
      // are short then the table processing is restarted.

      DLAYOUT("Checking table @ index %d for cell/table widening.  Table width: %d", idx, esctable->width);

      esctable->cells_expanded = true;

      if (!esctable->columns.empty()) {
         colwidth = (esctable->thickness * 2) + esctable->cell_hspacing;
         for (auto &col : esctable->columns) {
            colwidth += col.width + esctable->cell_hspacing;
         }
         if (esctable->thin) colwidth -= esctable->cell_hspacing * 2; // Thin tables have no spacing allocated on the sides

         if (colwidth < esctable->width) { // Cell layout is less than the pre-determined table width
            // Calculate the amount of additional space that is available for cells to expand into

            LONG avail_width = esctable->width - (esctable->thickness * 2) -
               (esctable->cell_hspacing * (esctable->columns.size() - 1));

            if (!esctable->thin) avail_width -= (esctable->cell_hspacing * 2);

            // Count the number of columns that do not have a fixed size

            unfixed = 0;
            for (unsigned j=0; j < esctable->columns.size(); j++) {
               if (esctable->columns[j].preset_width) avail_width -= esctable->columns[j].width;
               else unfixed++;
            }

            // Adjust for expandable columns that we know have exceeded the pre-calculated cell width
            // on previous passes (we want to treat them the same as the preset_width columns)  Such cells
            // will often exist that contain large graphics for example.

            if (unfixed > 0) {
               DOUBLE cell_width = avail_width / unfixed;
               for (unsigned j=0; j < esctable->columns.size(); j++) {
                  if ((esctable->columns[j].min_width) and (esctable->columns[j].min_width > cell_width)) {
                     avail_width -= esctable->columns[j].min_width;
                     unfixed--;
                  }
               }

               if (unfixed > 0) {
                  cell_width = avail_width / unfixed;
                  bool expanded = false;

                  //total = 0;
                  for (unsigned j=0; j < esctable->columns.size(); j++) {
                     if (esctable->columns[j].preset_width) continue; // Columns with preset-widths are never auto-expanded
                     if (esctable->columns[j].min_width > cell_width) continue;

                     if (esctable->columns[j].width < cell_width) {
                        DLAYOUT("Expanding column %d from width %d to %.2f", j, esctable->columns[j].width, cell_width);
                        esctable->columns[j].width = cell_width;
                        //if (total - (DOUBLE)F2I(total) >= 0.5) esctable->Columns[j].width++; // Fractional correction

                        expanded = true;
                     }
                     //total += cell_width;
                  }

                  if (expanded) {
                     DLAYOUT("At least one cell was widened - will repass table layout.");
                     return TE::WRAP_TABLE;
                  }
               }
            }
         }
      }
      else DLAYOUT("Table is missing its columns array.");
   }
   else DLAYOUT("Cells already widened - keeping table width of %d.", esctable->width);

   // Cater for the minimum height requested

   if (esctable->height_pct) {
      // If the table height is expressed as a percentage, it is calculated with
      // respect to the height of the display port.

      if (!Offset) {
         minheight = ((Self->Area.Height - BottomMargin - esctable->y) * esctable->min_height) / 100;
      }
      else minheight = ((Height - BottomMargin - TopMargin) * esctable->min_height) / 100;

      if (minheight < 0) minheight = 0;
   }
   else minheight = esctable->min_height;

   if (minheight > esctable->height + esctable->cell_vspacing + esctable->thickness) {
      // The last row in the table needs its height increased
      if (!stack_row.empty()) {
         auto j = minheight - (esctable->height + esctable->cell_vspacing + esctable->thickness);
         DLAYOUT("Extending table height to %d (row %d+%d) due to a minimum height of %d at coord %.2f",
            minheight, stack_row.top()->row_height, j, esctable->min_height, esctable->y);
         stack_row.top()->row_height += j;
         return TE::REPASS_ROW_HEIGHT;
      }
      else log.warning("No last row defined for table height extension.");
   }

   // Adjust for cellspacing at the bottom

   esctable->height += esctable->cell_vspacing + esctable->thickness;

   // Restart if the width of the table will force an extension of the page.

   LONG j = esctable->x + esctable->width - AbsX + m_right_margin;
   if ((j > Width) and (Width < WIDTH_LIMIT)) {
      DLAYOUT("Table width (%.2f+%d) increases page width to %d, layout restart forced.",
         esctable->x, esctable->width, j);
      Width = j;
      return TE::EXTEND_PAGE;
   }

   // Extend the height of the current line to the height of the table if the table is to be inline
   // (like an image).  We also extend the line height if the table covers the
   // entire width of the page (this is a valuable optimisation for the layout routine).

   if ((m_inline) or ((esctable->x <= m_left_margin) and (esctable->x + esctable->width >= m_wrap_edge))) {
      if (esctable->height > m_line.height) {
         m_line.height = esctable->height;
      }
   }

   if (!stack_para.empty()) {
      j = (esctable->y + esctable->height) - stack_para.top()->y;
      if (j > stack_para.top()->height) stack_para.top()->height = j;
   }

   // Check if the table collides with clipping boundaries and adjust its position accordingly.
   // Such a check is performed in SCODE::TABLE_START - this second check is required only if the width
   // of the table has been extended.
   //
   // Note that the total number of clips is adjusted so that only clips up to the TABLE_START are
   // considered (otherwise, clips inside the table cells will cause collisions against the parent
   // table).

   DLAYOUT("Checking table collisions (%.2fx%.2f).", esctable->x, esctable->y);

   std::vector<doc_clip> saved_clips(m_clips.begin() + esctable->total_clips, m_clips.end() + m_clips.size());
   m_clips.resize(esctable->total_clips);
   auto ww = check_wordwrap("Table", AbsX, Width, idx, esctable->x, esctable->y, esctable->width, esctable->height);
   m_clips.insert(m_clips.end(), saved_clips.begin(), saved_clips.end());

   if (ww IS WRAP::EXTEND_PAGE) {
      DLAYOUT("Table wrapped - expanding page width due to table size/position.");
      return TE::EXTEND_PAGE;
   }
   else if (ww IS WRAP::WRAPPED) {
      // A repass is necessary as everything in the table will need to be rearranged
      DLAYOUT("Table wrapped - rearrangement necessary.");
      return TE::WRAP_TABLE;
   }

   //DLAYOUT("new table pos: %dx%d", esctable->x, esctable->y);

   // The table sets a clipping region in order to state its placement (the surrounds of a table are
   // effectively treated as a graphics vector, since it's not text).

   //if (clip.left IS m_left_margin) clip.left = 0; // Extending the clipping to the left doesn't hurt

   m_clips.emplace_back(
      esctable->x, esctable->y, clip.Left + esctable->width, clip.Top + esctable->height,
      idx, false, "Table");

   m_cursor_x = esctable->x + esctable->width;
   m_cursor_y = esctable->y;

   DLAYOUT("Final Table Size: %.2fx%.2f,%dx%d", esctable->x, esctable->y, esctable->width, esctable->height);

   esctable = esctable->stack;

   add_esc_segment();
   return TE::NIL;
}

//********************************************************************************************************************
// Embedded vectors are always contained by a VectorViewport irrespective of whether or not the client asked for one.

WRAP layout::procVector(LONG Offset, DOUBLE AbsX, DOUBLE AbsY, LONG PageHeight,
   bool &VerticalRepass, bool &CheckWrap)
{
   pf::Log log;
   DOUBLE cx, cy, cr, cb;
   OBJECTID vector_id;

   // Tell the vector our cursor_x and cursor_y positions so that it can position itself within the stream
   // layout.  The vector will tell us its clipping boundary when it returns (if it has a clipping boundary).

   auto &vec = stream_data<::bc_vector>(Self, idx);
   if (!(vector_id = vec.object_id)) return WRAP::DO_NOTHING;
   if (vec.owned) return WRAP::DO_NOTHING; // Do not manipulate vectors that have owners

   // cell: Reflects the page/cell coordinates and width/height of the page/cell.

wrap_vector:
   cx = AbsX;
   cy = AbsY;
   cr  = cx + m_page_width;
   if ((!Offset) and (PageHeight < Self->Area.Height)) {
      cb = AbsY + Self->Area.Height; // The reported page height cannot be shorter than the document's viewport area
   }
   else cb = AbsY + PageHeight;

   if (m_line.height) {
      if (cb < m_cursor_y + m_line.height) cb = AbsY + m_line.height;
   }
   else if (cb < m_cursor_y + 1) cb = m_cursor_y + 1;

   LONG dimensions = 0;
   ALIGN align;
   DOUBLE cell_width, cell_height, left_margin, line_height, zone_height;
   ERROR error;

   pf::ScopedObjectLock<objVectorViewport> vector(vector_id, 5000);
   if (!vector.granted()) {
      if (vector.error IS ERR_DoesNotExist) vec.object_id = 0;
      return WRAP::DO_NOTHING;
   }

   DLAYOUT("[Idx:%d] The %s's available page area is (%.2fx%.2f, %.2fx%.2f), cursor %.2fx%.2f",
      idx, vector->Class->ClassName, cx, cr, cy, cb, m_cursor_x, m_cursor_y);

#if true
   DOUBLE new_y, new_width, new_height, calc_x;
   vector->get(FID_Dimensions, &dimensions);

   left_margin = m_left_margin - AbsX;
   line_height = m_line.height ? (m_line.height - m_line.gutter) : m_font->Ascent;

   cell_width  = cr - cx;
   cell_height = cb - cy;
   align = m_font->Align;

   // Relative dimensions can use the full size of the page/cell only when text-wrapping is disabled.

   zone_height = line_height;
   cx  += left_margin;
   cell_width  = cell_width - m_right_margin - left_margin; // Remove margins from the cell_width because we're only interested in the space available to the vector
   DOUBLE new_x = m_cursor_x;

   // WIDTH

   if (dimensions & DMF_RELATIVE_WIDTH) {
      DOUBLE wp;
      vector->getPercentage(FID_Width, &wp);
      new_width = cell_width * wp;
      if (new_width < 1) new_width = 1;
      else if (new_width > cell_width) new_width = cell_width;
   }
   else if (dimensions & DMF_FIXED_WIDTH) vector->get(FID_Width, &new_width);
   else if ((dimensions & DMF_X) and (dimensions & DMF_X_OFFSET)) {
      calc_x = new_x;

      if (dimensions & DMF_FIXED_X);
      else {
         // Relative x, such as 10% would mean 'NewX must be at least 10% beyond 'cell.left + leftmargin'
         DOUBLE xp;
         vector->getPercentage(FID_X, &xp);
         DOUBLE minx = cx + cell_width * xp;
         if (minx > calc_x) calc_x = minx;
      }

      // Calculate width

      if (dimensions & DMF_FIXED_X_OFFSET) {
         DOUBLE xo;
         vector->get(FID_XOffset, &xo);
         new_width = cell_width - xo - (calc_x - cx);
      }
      else {
         DOUBLE xop;
         vector->getPercentage(FID_XOffset, &xop);
         new_width = cell_width - (calc_x - cx) - (cell_width * xop);
      }

      if (new_width < 1) new_width = 1;
      else if (new_width > cell_width) new_width = cell_width;
   }
   else {
      DLAYOUT("No width specified for %s #%d (dimensions $%x), defaulting to 1 pixel.",
         vector->Class->ClassName, vector->UID, dimensions);
      new_width = 1;
   }

   // X COORD

   if (((align & ALIGN::HORIZONTAL) != ALIGN::NIL) and (dimensions & DMF_WIDTH)) {
      new_x = new_x + ((cell_width - new_width) * 0.5);
   }
   else if (((align & ALIGN::RIGHT) != ALIGN::NIL) and (dimensions & DMF_WIDTH)) {
      new_x = (AbsX + m_page_width) - m_right_margin - new_width;
   }
   else if (dimensions & DMF_RELATIVE_X) {
      DOUBLE xp;
      vector->getPercentage(FID_X, &xp);
      new_x = m_cursor_x + xp * cell_width;
   }
   else if ((dimensions & DMF_WIDTH) and (dimensions & DMF_X_OFFSET)) {
      if (dimensions & DMF_FIXED_X_OFFSET) {
         DOUBLE xo;
         vector->get(FID_XOffset, &xo);
         new_x = m_cursor_x + (cell_width - new_width - xo);
      }
      else {
         DOUBLE xop;
         vector->getPercentage(FID_XOffset, &xop);
         new_x = m_cursor_x + cell_width - new_width - (cell_width * xop);
      }
   }
   else if (dimensions & DMF_FIXED_X) {
      vector->get(FID_X, &new_x);
      new_x += m_cursor_x;
   }

   // HEIGHT

   if (dimensions & DMF_RELATIVE_HEIGHT) {
      // If the vector is inside a paragraph <p> section, the height will be calculated based
      // on the current line height.  Otherwise, the height is calculated based on the cell/page
      // height.

      DOUBLE hp;
      vector->getPercentage(FID_Height, &hp);
      new_height = zone_height * hp;
      if (new_height > zone_height) new_height = zone_height;
   }
   else if (dimensions & DMF_FIXED_HEIGHT) {
      vector->get(FID_Height, &new_height);
   }
   else if ((dimensions & DMF_Y) and (dimensions & DMF_Y_OFFSET)) {
      DOUBLE yp, yo, yop;
      if (dimensions & DMF_FIXED_Y) vector->get(FID_Y, &new_y);
      else {
         vector->getPercentage(FID_Y, &yp);
         new_y = zone_height * yp;
      }

      if (dimensions & DMF_FIXED_Y_OFFSET) {
         vector->get(FID_YOffset, &yo);
         new_height = zone_height - yo;
      }
      else {
         vector->getPercentage(FID_YOffset, &yop);
         new_height = zone_height - zone_height * yop;
      }

      if (new_height > zone_height) new_height = zone_height;
   }
   else new_height = line_height;

   if (new_height < 1) new_height = 1;

   // Y COORD

   const DOUBLE top = vec.ignore_cursor ? cy : m_cursor_y;

   if (dimensions & DMF_RELATIVE_Y) {
      DOUBLE yp;
      vector->getPercentage(FID_Y, &yp);
      new_y = top + yp * line_height;
   }
   else if ((dimensions & DMF_HEIGHT) and (dimensions & DMF_Y_OFFSET)) {
      DOUBLE height, yo, yop;
      vector->get(FID_Height, &height);
      if (dimensions & DMF_FIXED_Y_OFFSET) {
         vector->get(FID_YOffset, &yo);
         new_y = cy + zone_height - height - yo;
      }
      else {
         vector->getPercentage(FID_YOffset, &yop);
         new_y = top + zone_height - height - (zone_height * yop);
      }
   }
   else if (dimensions & DMF_Y_OFFSET) {
      // This section resolves situations where no explicit Y coordinate has been defined,
      // but the Y offset has been defined.  This means we leave the existing Y coordinate as-is and
      // adjust the height.

      if (dimensions & DMF_FIXED_Y_OFFSET) {
         DOUBLE yo;
         vector->get(FID_YOffset, &yo);
         new_height = zone_height - yo;
      }
      else {
         DOUBLE yop;
         vector->getPercentage(FID_YOffset, &yop);
         new_height = zone_height - zone_height * yop;
      }

      if (new_height < 1) new_height = 1;
      if (new_height > zone_height) new_height = zone_height;
      new_y = top;
   }
   else if (dimensions & DMF_FIXED_Y) {
      DOUBLE y;
      vector->get(FID_Y, &y);
      new_y = top + y;
   }

   // Set the clipping

   DLAYOUT("Clip region is being restricted to the bounds: %.2fx%.2f,%.2fx%.2f", new_x, new_y, new_width, new_height);

   cx   = new_x;
   cy    = new_y;
   cr  = new_x + new_width;
   cb = new_y + new_height;

   // If BlockRight is true, no text may be printed to the right of the vector.

   if (vec.block_right) {
      DLAYOUT("Block Right: Expanding clip.right boundary from %.2f to %.2f.",
         cr, AbsX + m_page_width - m_right_margin);
      cr = (AbsX + m_page_width) - m_right_margin; //cell_width;
   }

   // If BlockLeft is true, no text may be printed to the left of the vector (but not
   // including text that has already been printed).

   if (vec.block_left) {
      DLAYOUT("Block Left: Expanding clip.left boundary from %.2f to %.2f.", cx, AbsX);
      cx  = AbsX; //left_margin;
   }

   DOUBLE width_check = vec.ignore_cursor ? cr - AbsX : cr + m_right_margin;

   DLAYOUT("#%d, Pos: %.2fx%.2f,%.2fx%.2f, Align: $%.8x, WidthCheck: %.2f/%.0f",
      vector->UID, new_x, new_y, new_width, new_height, LONG(align), width_check, m_page_width);
   DLAYOUT("Clip Size: %.2fx%.2f,%.2fx%.2f, LineHeight: %.2f",
      cx, cy, cell_width, cell_height, line_height);

   dimensions = dimensions;
   error = ERR_Okay;

   acRedimension(vector.obj, new_x, new_y, 0.0, new_width, new_height, 0.0);

#else
   left_margin = m_left_margin - AbsX;
   line_height = m_line.height ? (m_line.height - m_line.gutter) : m_font->Ascent;

   cell_width  = cr - cx;
   cell_height = cb - cy;
   align = m_font->Align;

   if (!vec.Embedded) {
      // In background mode, the bounds are adjusted to match the size of the cell
      // if the vector supports GraphicWidth and GraphicHeight.  For all other vectors,
      // it is assumed that the bounds have been preset.
      //
      // Positioning within the cell bounds is managed by the GraphicX/y/width/height and
      // Align fields.
      //
      // Gaps are automatically worked into the calculated x/y value.

      if ((vector->GraphicWidth) and (vector->GraphicHeight) and (!(layoutflags & LAYOUT_TILE))) {
         vector->BoundX = cx;
         vector->BoundY = cy;

         if ((align & ALIGN::HORIZONTAL) != ALIGN::NIL) {
            vector->GraphicX = cx + vec.margins.left + ((cell_width - vector->GraphicWidth)>>1);
         }
         else if ((align & ALIGN::RIGHT) != ALIGN::NIL) vector->GraphicX = cx + cell_width - vec.margins.right - vector->GraphicWidth;
         else if (!vector->PresetX) {
            if (!vector->preset_width) {
               vector->GraphicX = cx + vec.margins.left;
            }
            else vector->GraphicX = m_cursor_x + vec.margins.left;
         }

         if ((align & ALIGN::VERTICAL) != ALIGN::NIL) vector->GraphicY = cy + ((cell_height - vector->TopMargin - vector->BottomMargin - vector->GraphicHeight) * 0.5);
         else if ((align & ALIGN::BOTTOM) != ALIGN::NIL) vector->GraphicY = cy + cell_height - vector->BottomMargin - vector->GraphicHeight;
         else if (!vector->PresetY) {
            if (!vector->PresetHeight) {
               vector->GraphicY = cy + vec.margins.top;
            }
            else vector->GraphicY = m_cursor_y + vec.margins.top;
         }

         // The vector bounds are set to the GraphicWidth/Height.  When the vector is drawn,
         // the bounds will be automatically restricted to the size of the cell (or page) so
         // that there is no over-draw.

         vector->BoundWidth = vector->GraphicWidth;
         vector->BoundHeight = vector->GraphicHeight;

         DLAYOUT("X/Y: %dx%d, W/H: %dx%d (Width/Height are preset)", vector->BoundX, vector->BoundY, vector->BoundWidth, vector->BoundHeight);
      }
      else {
         // If the vector does not use preset GraphicWidth and GraphicHeight, then
         // we just want to use the preset x/y/width/height in relation to the available
         // space within the cell.

         vector->ParentSurface.width = cr - cx;
         vector->ParentSurface.height = cb - cy;

         vector->get(FID_X, &vector->BoundX);
         vector->get(FID_Y, &vector->BoundY);
         vector->get(FID_Width, &vector->BoundWidth);
         vector->get(FID_Height, &vector->BoundHeight);

         vector->BoundX += cx;
         vector->BoundY += cy;


         DLAYOUT("X/Y: %dx%d, W/H: %dx%d, Parent W/H: %dx%d (Width/Height not preset), Dimensions: $%.8x",
            vector->BoundX, vector->BoundY, vector->BoundWidth, vector->BoundHeight, vector->ParentSurface.width, vector->ParentSurface.height, dimensions);
      }

      dimensions = dimensions;
      error = ERR_NothingDone; // Do not add a clipping region because the graphic is in the background
   }
   else {
      // The vector can extend the line's height if the GraphicHeight is larger than the line.
      //
      // Alignment calculations are restricted to this area, which forms the initial Bounds*:
      //
      //   X: CursorX
      //   Y: CursorY
      //   Right: PageWidth - RightMargin
      //   Bottom: LineHeight - GraphicHeight
      //
      // If IgnoreCursor is defined then the user has set fixed values for both X and Y.
      // The cursor is completely ignored and the existing Bound* fields will be used without alteration.
      // Use of IgnoreCursor also means that the left, right, top and bottom margins are all ignored.  Text
      // will still be wrapped around the boundaries as long as LAYOUT_BACKGROUND isn't set.

      LONG extclip_left = 0;
      LONG extclip_right = 0;

      if (vec.IgnoreCursor);
      else {
         // In cursor-relative (normal) layout mode, the graphic will be restricted by
         // the margins, so we adjust cell.left and the cell_width accordingly.
         //
         // The BoundWidth and BoundHeight can be expanded if the GraphicWidth/GraphicHeight exceed the bound values.
         //
         // Relative width/height values are allowed.
         //
         // A relative XOffset is allowed, this will be computed against the cell_width.
         //
         // Y Coordinates are allowed, these are computed from the top of the line.
         //
         // Relative height and vertical offsets are allowed, these are computed from the line_height.
         //
         // Vertical alignment is managed within the bounds of the vector when
         // it is drawn, so we do not cater for vertical alignment when positioning
         // the vector in this code.

         cx += left_margin;
         cell_width = cell_width - right_margin - left_margin; // Remove margins from the cell_width because we're only interested in the space available to the vector
      }

      // Adjust the bounds to reflect special dimension settings.  The minimum
      // width and height is 1, and the bounds may not exceed the size of the
      // available cell space (because a width of 110% would cause infinite recursion).

      if (vec.IgnoreCursor) vector->BoundX = cx;
      else vector->BoundX = m_cursor_x;

      vector->BoundWidth = 1; // Just a default in case no width in the Dimension flags is defined

      if ((align & ALIGN::HORIZONTAL) and (vector->GraphicWidth)) {
         // In horizontal mode where a GraphicWidth is preset, we force the BoundX and BoundWidth to their
         // exact settings and override any attempt by the user to have preset the X and Width fields.
         // The vector will attempt a horizontal alignment within the bounds, this will be to no effect as the
         // GraphicWidth is equivalent to the BoundWidth.  Text can still appear to the left and right of the vector,
         // if the author does not like this then the BlockLeft and BlockRight options can be used to extend
         // the clipping region.


         LONG new_x = vector->BoundX + ((cell_width - (vector->GraphicWidth + vec.margins.left + vec.margins.right))/2);
         if (new_x > vector->BoundX) vector->BoundX = new_x;
         vector->BoundX += vec.margins.left;
         vector->BoundWidth = vector->GraphicWidth;
         extclip_left = vec.margins.left;
         extclip_right = vec.margins.right;
      }
      else if ((align & ALIGN::RIGHT) and (vector->GraphicWidth)) {
         LONG new_x = ((AbsX + width) - right_margin) - (vector->GraphicWidth + vec.margins.right);
         if (new_x > vector->BoundX) vector->BoundX = new_x;
         vector->BoundWidth = vector->GraphicWidth;
         extclip_left = vec.margins.left;
         extclip_right = vec.margins.right;
      }
      else {
         LONG xoffset;

         if (dimensions & DMF_FIXED_X_OFFSET) xoffset = vector->XOffset;
         else if (dimensions & DMF_RELATIVE_X_OFFSET) xoffset = (DOUBLE)cell_width * (DOUBLE)vector->XOffset;
         else xoffset = 0;

         if (dimensions & DMF_RELATIVE_X) {
            LONG new_x = vector->BoundX + vec.margins.left + vector->x * cell_width;
            if (new_x > vector->BoundX) vector->BoundX = new_x;
            extclip_left = 0;
            extclip_right = 0;
         }
         else if (dimensions & DMF_FIXED_X) {
            LONG new_x = vector->BoundX + vector->x + vec.margins.left;
            if (new_x > vector->BoundX) vector->BoundX = new_x;
            extclip_left = 0;
            extclip_right = 0;
         }

         // WIDTH

         if (dimensions & DMF_RELATIVE_WIDTH) {
            vector->BoundWidth = (DOUBLE)(cell_width - (vector->BoundX - cx)) * (DOUBLE)vector->width;
            if (vector->BoundWidth < 1) vector->BoundWidth = 1;
            else if (vector->BoundWidth > cell_width) vector->BoundWidth = cell_width;
         }
         else if (dimensions & DMF_FIXED_WIDTH) vector->BoundWidth = vector->width;

         // GraphicWidth and GraphicHeight settings will expand the width and height
         // bounds automatically unless the width and height fields in the Layout have been preset
         // by the user.
         //
         // NOTE: If the vector supports GraphicWidth and GraphicHeight, it must keep
         // them up to date if they are based on relative values.

         if ((vector->GraphicWidth > 0) and (!(dimensions & DMF_WIDTH))) {
            DLAYOUT("Setting BoundWidth from %d to preset GraphicWidth of %d", vector->BoundWidth, vector->GraphicWidth);
            vector->BoundWidth = vector->GraphicWidth;
         }
         else if ((dimensions & DMF_X) and (dimensions & DMF_X_OFFSET)) {
            if (dimensions & DMF_FIXED_X_OFFSET) vector->BoundWidth = cell_width - xoffset - (vector->BoundX - cx);
            else vector->BoundWidth = cell_width - (vector->BoundX - cx) - xoffset;

            if (vector->BoundWidth < 1) vector->BoundWidth = 1;
            else if (vector->BoundWidth > cell_width) vector->BoundWidth = cell_width;
         }
         else if ((dimensions & DMF_WIDTH) and
                  (dimensions & DMF_X_OFFSET)) {
            if (dimensions & DMF_FIXED_X_OFFSET) {
               LONG new_x = vector->BoundX + cell_width - vector->BoundWidth - xoffset - vec.margins.right;
               if (new_x > vector->BoundX) vector->BoundX = new_x;
               extclip_left = vec.margins.left;
            }
            else {
               LONG new_x = vector->BoundX + cell_width - vector->BoundWidth - xoffset;
               if (new_x > vector->BoundX) vector->BoundX = new_x;
            }
         }
         else {
            if ((align & ALIGN::HORIZONTAL) and (dimensions & DMF_WIDTH)) {
               LONG new_x = vector->BoundX + ((cell_width - (vector->BoundWidth + vec.margins.left + vec.margins.right))/2);
               if (new_x > vector->BoundX) vector->BoundX = new_x;
               vector->BoundX += vec.margins.left;
               extclip_left = vec.margins.left;
               extclip_right = vec.margins.right;
            }
            else if ((align & ALIGN::RIGHT) and (dimensions & DMF_WIDTH)) {
               // Note that it is possible the BoundX may end up behind the cursor, or the cell's left margin.
               // A check for this is made later, so don't worry about it here.

               LONG new_x = ((AbsX + width) - m_right_margin) - (vector->BoundWidth + vec.margins.right);
               if (new_x > vector->BoundX) vector->BoundX = new_x;
               extclip_left = vec.margins.left;
               extclip_right = vec.margins.right;
            }
         }
      }

      // VERTICAL SUPPORT

      LONG obj_y;

      if (vec.IgnoreCursor) vector->BoundY = cy;
      else vector->BoundY = m_cursor_y;

      obj_y = 0;
      if (dimensions & DMF_RELATIVE_Y)   obj_y = vector->y * line_height;
      else if (dimensions & DMF_FIXED_Y) obj_y = vector->y;
      obj_y += vector->TopMargin;
      vector->BoundY += obj_y;
      vector->BoundHeight = line_height - obj_y; // This is merely a default

      LONG zone_height;

      if ((Paragraph) or (PageHeight < 1)) zone_height = line_height;
      else zone_height = PageHeight;

      // HEIGHT

      if (dimensions & DMF_RELATIVE_HEIGHT) {
         // If the vector is inside a paragraph <p> section, the height will be calculated based
         // on the current line height.  Otherwise, the height is calculated based on the cell/page
         // height.

         vector->BoundHeight = (DOUBLE)(zone_height - obj_y) * (DOUBLE)vector->height;
         if (vector->BoundHeight > zone_height - obj_y) vector->BoundHeight = line_height - obj_y;
      }
      else if (dimensions & DMF_FIXED_HEIGHT) {
         vector->BoundHeight = vector->height;
      }

      if ((vector->GraphicHeight > vector->BoundHeight) and (!(dimensions & DMF_HEIGHT))) {
         DLAYOUT("Expanding BoundHeight from %d to preset GraphicHeight of %d", vector->BoundHeight, vector->GraphicHeight);
         vector->BoundHeight = vector->GraphicHeight;
      }
      else {
         if (vector->BoundHeight < 1) vector->BoundHeight = 1;

         // This code deals with vertical offsets

         if (dimensions & DMF_Y_OFFSET) {
            if (dimensions & DMF_Y) {
               if (dimensions & DMF_FIXED_Y_OFFSET) vector->BoundHeight = zone_height - vector->YOffset;
               else vector->BoundHeight = zone_height - (DOUBLE)zone_height * (DOUBLE)vector->YOffset;

               if (vector->BoundHeight > zone_height) vector->BoundHeight = zone_height;
            }
            else if (dimensions & DMF_HEIGHT) {
               if (dimensions & DMF_FIXED_Y_OFFSET) vector->BoundY = cy + zone_height - vector->height - vector->YOffset;
               else vector->BoundY += (DOUBLE)zone_height - (DOUBLE)vector->height - ((DOUBLE)zone_height * (DOUBLE)vector->YOffset);
            }
         }
      }

      if (layoutflags & (LAYOUT_BACKGROUND|LAYOUT_TILE)) {
         error = ERR_NothingDone; // No text wrapping for background and tile layouts
      }
      else {
         // Set the clipping

         DLAYOUT("Clip region is being restricted to the bounds: %dx%d,%dx%d", vector->BoundX, vector->BoundY, vector->BoundWidth, vector->BoundHeight);

         cx  = vector->BoundX - extclip_left;
         cy   = vector->BoundY - vector->TopMargin;
         cr = vector->BoundX + vector->BoundWidth + extclip_right;
         cb = vector->BoundY + vector->BoundHeight + vector->BottomMargin;

         // If BlockRight is set, no text may be printed to the right of the vector.  This has no impact
         // on the vector's bounds.

         if (vec.BlockRight) {
            DLAYOUT("BlockRight: Expanding clip.right boundary from %d to %d.", cr, AbsX + width - l.right_margin);
            LONG new_right = (AbsX + width) - right_margin; //cell_width;
            if (new_right > cr) cr = new_right;
         }

         // If BlockLeft is set, no text may be printed to the left of the vector (but not
         // including text that has already been printed).  This has no impact on the vector's
         // bounds.

         if (vec.BlockLeft) {
            DLAYOUT("BlockLeft: Expanding clip.left boundary from %d to %d.", cx, AbsX);

            if (IgnoreCursor) cx = AbsX;
            else cx  = m_left_margin;
         }

         if (IgnoreCursor) width_check = cr - AbsX;
         else width_check = cr + m_right_margin;

         DLAYOUT("#%d, Pos: %dx%d,%dx%d, Align: $%.8x, WidthCheck: %d/%d", vector->UID, vector->BoundX, vector->BoundY, vector->BoundWidth, vector->BoundHeight, align, vector->x), width_check, width);
         DLAYOUT("Clip Size: %dx%d,%dx%d, LineHeight: %d, GfxSize: %dx%d, LayoutFlags: $%.8x", cx, cy, cell_width, cell_height, line_height, vector->GraphicWidth, vector->GraphicHeight, layoutflags);

         dimensions = dimensions;
         error = ERR_Okay;
      }
   }
#endif

   if (width_check) {
      // The cursor must advance past the clipping region so that the segment positions will be
      // correct when set.

      CheckWrap = true;

      // Check if the clipping region is invalid.  Invalid clipping regions are not added to the clip
      // region list (i.e. layout of document text will ignore the presence of the vector).

      if ((cb <= cy) or (cr <= cx)) {
         if (auto name = vector->Name) log.warning("%s %s returned an invalid clip region of %.2fx%.2f,%.2fx%.2f", vector->Class->ClassName, name, cx, cy, cr, cb);
         else log.warning("%s #%d returned an invalid clip region of %.2fx%.2f,%.2fx%.2f", vector->Class->ClassName, vector->UID, cx, cy, cr, cb);
         return WRAP::DO_NOTHING;
      }

      // If the right-side of the vector extends past the page width, increase the width.

      LONG left_check;

      if (vec.ignore_cursor) left_check = AbsX;
      else if (vec.block_left) left_check = m_left_margin;
      else left_check = m_left_margin; //m_cursor_x;

      if (m_page_width >= WIDTH_LIMIT);
      else if ((cx < left_check) or (vec.ignore_cursor)) {
         // The vector is < left-hand side of the page/cell, this means that we may have to force a page/cell width
         // increase.
         //
         // Note: Vectors with IgnoreCursor are always checked here, because they aren't subject
         // to wrapping due to the x/y being fixed.  Such vectors are limited to width increases only.

         LONG cmp_width;

         if (vec.ignore_cursor) cmp_width = AbsX + (cr - cx);
         else cmp_width = m_left_margin + (cr - cx) + m_right_margin;

         if (m_page_width < cmp_width) {
            DLAYOUT("Restarting as %s clip.left %.2f < %d and extends past the page width (%.2f > %.0f).", vector->Class->ClassName, cx, left_check, width_check, m_page_width);
            m_page_width = cmp_width;
            return WRAP::EXTEND_PAGE;
         }
      }
      else if (width_check > m_page_width) {
         // Perform a wrapping check if the vector possibly extends past the width of the page/cell.

         DLAYOUT("Wrapping %s vector #%d as it extends past the page width (%.2f > %.0f).  Pos: %.2fx%.2f", vector->Class->ClassName, vector->UID, width_check, m_page_width, cx, cy);

         auto ww = check_wordwrap("Vector", AbsX, m_page_width, idx, cx, cy, cr - cx, cb - cy);

         if (ww IS WRAP::EXTEND_PAGE) {
            DLAYOUT("Expanding page width due to vector size.");
            return WRAP::EXTEND_PAGE;
         }
         else if (ww IS WRAP::WRAPPED) {
            DLAYOUT("Vector coordinates wrapped to %.2fx%.2f", cx, cy);
            // The check_wordwrap() function will have reset m_cursor_x and m_cursor_y, so
            // on our repass, the cell.left and cell.top will reflect this new cursor position.

            goto wrap_vector;
         }
      }

      DLAYOUT("Adding %s clip to the list: (%.2fx%.2f, %.2fx%.2f)", vector->Class->ClassName, cx, cy, cr-cx, cb-cy);

      m_clips.emplace_back(cx, cy, cr, cb, idx, !vec.in_line, "Vector");

      if (vec.in_line) {
         if (cb > m_cursor_y) {
            auto objheight = cb - m_cursor_y;
            if ((m_inline) or (vec.in_line)) { // Inline graphics affect the line height.
               if (objheight > m_line.word_height) m_line.word_height = objheight;
            }
         }

         //if (cr > m_cursor_x) m_word_width += cr - m_cursor_x;

         if (!stack_para.empty()) {
            auto j = cb - stack_para.top()->y;
            if (j > stack_para.top()->height) stack_para.top()->height = j;
         }
      }
   }

   // If the vector uses a relative height or vertical offset, a repass will be required if the page height
   // increases.

   if ((dimensions & (DMF_RELATIVE_HEIGHT|DMF_FIXED_Y_OFFSET|DMF_RELATIVE_Y_OFFSET)) and
       ((!vec.in_line) or (vec.ignore_cursor))) {
      DLAYOUT("Vertical repass may be required.");
      VerticalRepass = true;
   }

   add_esc_segment();

   return WRAP::DO_NOTHING;
}

//********************************************************************************************************************

void layout::procSetMargins(LONG AbsY, LONG &BottomMargin)
{
   auto &escmargins = stream_data<::bc_set_margins>(Self, idx);

   if (escmargins.left != 0x7fff) {
      m_cursor_x    += escmargins.left;
      m_line.x      += escmargins.left;
      m_left_margin += escmargins.left;
   }

   if (escmargins.right != 0x7fff) {
      m_right_margin += escmargins.right;
      m_align_width  -= escmargins.right;
      m_wrap_edge    -= escmargins.right;
   }

   if (escmargins.top != 0x7fff) {
      if (m_cursor_y < AbsY + escmargins.top) m_cursor_y = AbsY + escmargins.top;
   }

   if (escmargins.bottom != 0x7fff) {
      BottomMargin += escmargins.bottom;
      if (BottomMargin < 0) BottomMargin = 0;
   }
}

//********************************************************************************************************************
// This function creates segments, which are used during the drawing process as well as user interactivity, e.g. to
// determine the character that the mouse is positioned over.

void layout::add_drawsegment(stream_char Start, stream_char Stop, DOUBLE Y, DOUBLE Width, DOUBLE AlignWidth, const std::string &Debug)
{
   pf::Log log(__FUNCTION__);

   // Process trailing whitespace at the end of the line.  This helps to prevent situations such as underlining
   // occurring in whitespace at the end of the line during word-wrapping.

   auto trim_stop = Stop;
   while ((trim_stop.get_prev_char_or_inline(Self, Self->Stream) <= 0x20) and (trim_stop > Start)) {
      if (!trim_stop.get_prev_char_or_inline(Self, Self->Stream)) break;
      trim_stop.prev_char(Self, Self->Stream);
   }

   if (Start >= Stop) {
      DLAYOUT("Cancelling addition, no content in line to add (bytes %d-%d) \"%.20s\" (%s)",
         Start.index, Stop.index, printable(Self, Start).c_str(), Debug.c_str());
      return;
   }

   // The content of the segment affects some factors such as line height.

   bool text_content     = false;
   bool floating_vectors = false;
   bool allow_merge      = true; // If true, this segment can be merged with prior segment(s) on the line

   for (auto i=Start; i < Stop; i.nextCode()) {
      switch (Self->Stream[i.index].code) {
         case SCODE::VECTOR:
            floating_vectors = true;
            // Fall through
         case SCODE::IMAGE:
         case SCODE::TABLE_START:
         case SCODE::TABLE_END:
         case SCODE::FONT:
            allow_merge = false;
            break;

         case SCODE::TEXT:
            text_content = true;
            allow_merge = false;
            break;

         default:
            break;
      }
   }

   auto line_height = m_line.height;
   auto gutter      = m_line.gutter;
   if (text_content) {
      if (line_height <= 0) {
         // No line-height given and there is text content - use the most recent font to determine the line height
         line_height = m_font->LineSpacing;
         gutter      = m_font->LineSpacing - m_font->Ascent;
      }
      else if (!gutter) { // If gutter is missing for some reason, define it
         gutter = m_font->LineSpacing - m_font->Ascent;
      }
   }
   else if (line_height < 0) line_height = 0;

#ifdef DBG_STREAM
   log.branch("#%d %d:%d - %d:%d, Area: %.0fx%.0f,%.0f:%.0fx%.0f, WordWidth: %d [%.20s]...[%.20s] (%s)",
      LONG(m_segments.size()), start.index, LONG(start.offset), Stop.index, LONG(Stop.offset), m_line.x, y, width,
      AlignWidth, line_height, m_word_width, printable(Self, start).c_str(),
      printable(Self, Stop).c_str(), Debug.c_str());
#endif

   auto x = m_line.x;

   if ((!m_segments.empty()) and (Start < m_segments.back().stop)) {
      // Patching: If the start of the new segment is < the end of the previous segment,
      // adjust the previous segment so that it stops at the beginning of our new segment.
      // This prevents overlapping between segments and the two segments will be patched
      // together in the next section of this routine.

      if (Start <= m_segments.back().start) {
         // If the start of the new segment retraces to an index that has already been configured,
         // then we have actually encountered a coding flaw and the caller should be investigated.

         log.warning("(%s) New segment #%d retraces to index %d, which has been configured by previous segments.", Debug.c_str(), m_segments.back().start.index, Start.index);
         return;
      }
      else {
         DLAYOUT("New segment #%d start index is less than (%d < %d) the end of previous segment - will patch up.",
            m_segments.back().start.index, Start.index, m_segments.back().stop.index);
         m_segments.back().stop = Start;
      }
   }

   doc_segment segment;

   // Is the new segment a continuation of the previous one, and does the previous segment contain content?
   if ((allow_merge) and (!m_segments.empty()) and (m_segments.back().stop IS Start) and
       (m_segments.back().allow_merge)) {
      // We are going to extend the previous segment rather than add a new one, as the two
      // segments only contain control codes.

      segment = m_segments.back();
      m_segments.pop_back();

      Start  = segment.start;
      x      = segment.area.X;
      Width += segment.area.Width;
      AlignWidth += segment.align_width;
      if (segment.area.Height > line_height) {
         line_height = segment.area.Height;
         gutter      = segment.gutter;
      }
   }

#ifdef _DEBUG
   // If this is a segmented line, check if any previous entries have greater
   // heights.  If so, this is considered an internal programming error.

   if ((m_split_start != NOTSPLIT) and (height > 0)) {
      for (i=m_split_start; i < offset; i++) {
         if (m_segments[i].depth != Self->Depth) continue;
         if (m_segments[i].height > height) {
            log.warning("A previous entry in segment %d has a height larger than the new one (%d > %d)", i, m_segments[i].height, height);
            BaseLine = m_segments[i].BaseLine;
            height = m_segments[i].height;
         }
      }
   }
#endif

   segment.start           = Start;
   segment.stop            = Stop;
   segment.trim_stop        = trim_stop;
   segment.area            = { x, Y, Width, line_height };
   segment.gutter          = gutter;
   segment.depth           = Self->Depth;
   segment.align_width      = AlignWidth;
   segment.text_content     = text_content;
   segment.floating_vectors = floating_vectors;
   segment.allow_merge      = allow_merge;
   segment.edit            = Self->EditMode;

   // If a line is segmented, we need to check for earlier line segments and ensure that their height and gutter
   // is matched to that of the last line (which always contains the maximum height and gutter values).

   if ((m_split_start != NOTSPLIT) and (line_height)) {
      if (LONG(m_segments.size()) != m_split_start) {
         DLAYOUT("Resetting height (%.0f) & gutter (%.0f) of segments index %d-%d.", line_height, gutter, segment.start.index, m_split_start);
         for (unsigned i=m_split_start; i < m_segments.size(); i++) {
            if (m_segments[i].depth != Self->Depth) continue;
            m_segments[i].area.Height = line_height;
            m_segments[i].gutter      = gutter;
         }
      }
   }

   m_segments.emplace_back(segment);
}

//********************************************************************************************************************
// This function lays out the document so that it is ready to be drawn.  It calculates the position, pixel length and
// height of each line and rearranges any vectors that are present in the document.

static void layout_doc(extDocument *Self)
{
   pf::Log log(__FUNCTION__);

   if (!Self->UpdatingLayout) return;

   // Remove any resources from the previous layout process.

   for (auto &obj : Self->LayoutResources) FreeResource(obj);
   Self->LayoutResources.clear();

   if (Self->Stream.empty()) return;

   // Initial height is 1 and not set to the viewport height because we want to accurately report the final height
   // of the page.

   #ifdef DBG_LAYOUT
      log.branch("Area: %dx%d,%dx%d Visible: %d ----------", Self->Area.x, Self->Area.y, Self->Area.width, Self->Area.height, Self->VScrollVisible);
   #endif

   Self->BreakLoop = MAXLOOP;

   layout l(Self);
   bool restart;
   do {
      restart = false;
      Self->BreakLoop--;

      DOUBLE page_width;

      if (Self->PageWidth <= 0) {
         // No preferred page width; maximise the page width to the available viewing area
         page_width = Self->Area.Width;
      }
      else if (!Self->RelPageWidth) page_width = Self->PageWidth;
      else page_width = (Self->PageWidth * Self->Area.Width) * 0.01;

      if (page_width < Self->MinPageWidth) page_width = Self->MinPageWidth;

      Self->SortSegments.clear();

      Self->PageProcessed = false;
      Self->Error = ERR_Okay;
      Self->Depth = 0;

      if (glFonts.empty()) return;
      auto font = glFonts[0].font;

      DOUBLE page_height = 1;
      l = layout(Self);
      bool vertical_repass = false;
      l.do_layout(0, Self->Stream.size(), &font, 0, 0, page_width, page_height,
         ClipRectangle(Self->LeftMargin, Self->TopMargin, Self->RightMargin, Self->BottomMargin),
         vertical_repass);

      // If the resulting page width has increased beyond the available area, increase the MinPageWidth value to reduce
      // the number of passes required for the next time we do a layout.

      if ((page_width > Self->Area.Width) and (Self->MinPageWidth < page_width)) Self->MinPageWidth = page_width;

      Self->PageHeight = page_height;
      //if (Self->PageHeight < Self->AreaHeight) Self->PageHeight = Self->AreaHeight;
      Self->CalcWidth = page_width;

      // Recalculation may be required if visibility of the scrollbar needs to change.

      if ((Self->BreakLoop > 0) and (!Self->Error)) {
         if (Self->PageHeight > Self->Area.Height) {
            // Page height is bigger than the viewport, so the scrollbar needs to be visible.

            if (!Self->VScrollVisible) {
               DLAYOUT("Vertical scrollbar visibility needs to be enabled, restarting...");
               Self->VScrollVisible = true;
               Self->BreakLoop = MAXLOOP;
               restart = true;
            }
         }
         else { // Page height is smaller than the viewport, so the scrollbar needs to be invisible.
            if (Self->VScrollVisible) {
               DLAYOUT("Vertical scrollbar needs to be invisible, restarting...");
               Self->VScrollVisible = false;
               Self->BreakLoop = MAXLOOP;
               restart = true;
            }
         }
      }
   } while (restart);

   // Look for clickable links that need to be aligned and adjust them (links cannot be aligned until the entire
   // width of their line is known, hence it's easier to make a final adjustment for all links post-layout).

   if (!Self->Error) {
      Self->Links = l.m_links;
      for (auto &link : Self->Links) {
         if (link.base_code != SCODE::LINK) continue;

         auto esclink = std::get<bc_link *>(link.ref);
         if ((esclink->align & (FSO::ALIGN_RIGHT|FSO::ALIGN_CENTER)) != FSO::NIL) {
            auto &segment = l.m_segments[link.segment];
            if ((esclink->align & FSO::ALIGN_RIGHT) != FSO::NIL) {
               link.x = segment.area.X + segment.align_width - link.width;
            }
            else if ((esclink->align & FSO::ALIGN_CENTER) != FSO::NIL) {
               link.x = link.x + ((segment.align_width - link.width) / 2);
            }
         }
      }
   }
   else Self->Links.clear();

   if (!Self->Error) {
      Self->Clips = l.m_clips;
      Self->EditCells = l.m_ecells;
   }
   else {
      Self->Clips.clear();
      Self->EditCells.clear();
   }

   if ((!Self->Error) and (!l.m_segments.empty())) Self->Segments = l.m_segments;
   else Self->Segments.clear();

   Self->UpdatingLayout = false;

#ifdef DBG_SEGMENTS
   print_segments(Self, Self->Stream);
   print_tabfocus(Self);
#endif

   // If an error occurred during layout processing, unload the document and display an error dialog.  (NB: While it is
   // possible to display a document up to the point at which the error occurred, we want to maintain a strict approach
   // so that human error is considered excusable in document formatting).

   if (Self->Error) {
      unload_doc(Self, ULD::REDRAW);

      std::string msg = "A failure occurred during the layout of this document - it cannot be displayed.\n\nDetails: ";
      if (Self->Error IS ERR_Loop) msg.append("This page cannot be rendered correctly in its current form.");
      else msg.append(GetErrorMsg(Self->Error));

      error_dialog("Document Layout Error", msg);
   }
   else {
      acResize(Self->Page, Self->CalcWidth, Self->PageHeight, 0);

      l.gen_scene_graph();

      for (auto &trigger : Self->Triggers[LONG(DRT::AFTER_LAYOUT)]) {
         if (trigger.Type IS CALL_SCRIPT) {
            const ScriptArg args[] = {
               { "ViewWidth",  Self->Area.Width },
               { "ViewHeight", Self->Area.Height },
               { "PageWidth",  Self->CalcWidth },
               { "PageHeight", Self->PageHeight }
            };
            scCallback(trigger.Script.Script, trigger.Script.ProcedureID, args, ARRAYSIZE(args), NULL);
         }
         else if (trigger.Type IS CALL_STDC) {
            auto routine = (void (*)(APTR, extDocument *, LONG, LONG, LONG, LONG))trigger.StdC.Routine;
            pf::SwitchContext context(trigger.StdC.Context);
            routine(trigger.StdC.Context, Self, Self->Area.Width, Self->Area.Height, Self->CalcWidth, Self->PageHeight);
         }
      }
   }
}

//********************************************************************************************************************
// Calculate the position, pixel length and height of each element on the page.  Routine will loop if the size of the
// page is too small and requires expansion.  Individual table cells are treated as miniature pages, resulting in a
// recursive call.
//
// TODO: Consider prioritising the layout of table cells first, possibly using concurrent threads.
//
// Offset/End: start and end points within the stream for layout processing.
// X/Y:      Section coordinates, starts at 0,0 for the main page, subsequent sections (table cells) can be at any location, measured as absolute to the top left corner of the page.
// Width:    Minimum width of the page/section.  Can be increased if insufficient space is available.  Includes the left and right margins in the resulting calculation.
// Height:   Minimum height of the page/section.  Will be increased to match the number of lines in the layout.
// Margins:  Margins within the page area.  These are inclusive to the resulting page width/height.  If in a cell, margins reflect cell padding values.

INDEX layout::do_layout(INDEX Offset, INDEX End, objFont **Font, LONG AbsX, LONG AbsY, DOUBLE &Width, DOUBLE &Height,
   ClipRectangle Margins, bool &VerticalRepass)
{
   pf::Log log(__FUNCTION__);

   bc_cell *esccell;
   bc_table *esctable;

   layout tablestate(Self), rowstate(Self), liststate(Self);
   LONG last_height, edit_segment;
   bool check_wrap;

   if ((Self->Stream.empty()) or (Offset >= End) or (!Font) or (!Font[0])) {
      log.traceBranch("No document stream to be processed.");
      return 0;
   }

   if (Self->Depth >= MAX_DEPTH) {
      log.traceBranch("Depth limit exceeded (too many tables-within-tables).");
      return 0;
   }

   auto page_height = Height;
   m_page_width = Width;

   #ifdef DBG_LAYOUT
   log.branch("Dimensions: %dx%d,%.0fx%.0f (edge %.0f), LM %d RM %d TM %d BM %d",
      AbsX, AbsY, m_page_width, page_height, AbsX + m_page_width - margins.right,
      margins.left, margins.right, margins.top, margins.bottom);
   #endif

   Self->Depth++;

extend_page:
   if (m_page_width > WIDTH_LIMIT) {
      DLAYOUT("Restricting page width from %.0f to %d", m_page_width, WIDTH_LIMIT);
      m_page_width = WIDTH_LIMIT;
      if (Self->BreakLoop > 4) Self->BreakLoop = 4; // Very large page widths normally means that there's a parsing problem
   }

   if (Self->Error) {
      Self->Depth--;
      return 0;
   }
   else if (!Self->BreakLoop) {
      Self->Error = ERR_Loop;
      Self->Depth--;
      return 0;
   }
   Self->BreakLoop--;

   reset();

   last_height  = page_height;
   esctable     = NULL;
   esccell      = NULL;
   edit_segment = 0;
   check_wrap   = false;  // true if a wordwrap or collision check is required

   m_left_margin  = AbsX + Margins.Left;
   m_right_margin = Margins.Right;   // Retain the right margin in an adjustable variable, in case we adjust the margin
   m_wrap_edge    = AbsX + m_page_width - Margins.Right;
   m_align_width  = m_wrap_edge;
   m_cursor_x     = AbsX + Margins.Left;
   m_cursor_y     = AbsY + Margins.Top;
   m_split_start  = m_segments.size();
   m_font         = *Font;
   m_space_width  = fntCharWidth(m_font, ' ', 0, NULL);
   m_word_index.reset();

   m_line.index.set(Offset, 0);    // The starting index of the line we are operating on
   m_line.full_reset(AbsX + Margins.Left);

   for (idx = Offset; idx < End; idx++) {
      if (m_line.index.index < idx) {
         if (breakable_word()) {
            DLAYOUT("Setting line at code '%s', index %d, line.x: %.0f, m_word_width: %d",
               BC_NAME(Self->Stream,idx).c_str(), m_line.index.index, m_line.x, m_word_width);
            m_cursor_x += m_word_width;
            stream_char sc(idx,0);
            add_drawsegment(m_line.index, sc, m_cursor_y, m_cursor_x - m_line.x, m_align_width - m_line.x, "WordBreak");
            reset_segment();
            m_align_width = m_wrap_edge;
         }
      }

      // Any escape code for an inline element that forces a word-break will initiate a wrapping check.

      if (esctable) m_align_width = m_wrap_edge;
      else switch (Self->Stream[idx].code) {
         case SCODE::TABLE_END:
         case SCODE::ADVANCE: {
            auto wrap_result = check_wordwrap("EscCode", AbsX, m_page_width, m_word_index.index, m_cursor_x, m_cursor_y, m_word_width, (m_line.height < 1) ? 1 : m_line.height);
            if (wrap_result IS WRAP::EXTEND_PAGE) {
               DLAYOUT("Expanding page width on wordwrap request.");
               goto extend_page;
            }
            break;
         }

         default:
            m_align_width = m_wrap_edge;
            break;
      }

      if (idx >= End) break;

#ifdef DBG_LAYOUT_ESCAPE
      DLAYOUT("ESC_%s Indexes: %d-%d-%d, WordWidth: %d",
         BC_NAME(Self->Stream, idx).c_str(), m_line.index.index, idx, m_word_index.index, m_word_width);
#endif

      switch (Self->Stream[idx].code) {
         case SCODE::TEXT: {
            auto wrap_result = procText(AbsX);
            if (wrap_result IS WRAP::EXTEND_PAGE) { // A word in the text string is too big for the available space.
               DLAYOUT("Expanding page width on wordwrap request.");
               goto extend_page;
            }
            else if (wrap_result IS WRAP::WRAPPED) { // A wrap occurred during text processing.
               // The presence of the line-break must be ignored, due to word-wrap having already made the new line for us
               auto &text = stream_data<bc_text>(Self, idx);
               if (text.text[0] IS '\n') {
                  if (text.text.size() > 0) m_line.index.offset = 1;
               }
            }
            break;
         }
         case SCODE::ADVANCE:         procAdvance(); break;
         case SCODE::FONT:            procFont(); break;
         case SCODE::INDEX_START:     procIndexStart(); break;
         case SCODE::SET_MARGINS:     procSetMargins(AbsY, Margins.Bottom); break;
         case SCODE::LINK:            procLink(); break;
         case SCODE::LINK_END:        procLinkEnd(); break;
         case SCODE::CELL_END:        procCellEnd(esccell); break;

         case SCODE::PARAGRAPH_START: procParagraphStart(); break;
         case SCODE::PARAGRAPH_END:   procParagraphEnd(); break;

         case SCODE::LIST_START:
            // This is the start of a list.  Each item in the list will be identified by SCODE::PARAGRAPH codes.  The
            // cursor position is advanced by the size of the item graphics element.

            liststate = *this;
            stack_list.push(&stream_data<bc_list>(Self, idx));
            stack_list.top()->repass = false;
            break;

         case SCODE::LIST_END:
            if (procListEnd()) {
               *this = liststate;
            }
            break;

         case SCODE::IMAGE: {
            auto ww = procImage(AbsX);
            if (ww IS WRAP::EXTEND_PAGE) goto extend_page;
            break;
         }

         case SCODE::VECTOR: {
            auto ww = procVector(Offset, AbsX, AbsY, page_height, VerticalRepass, check_wrap);
            if (ww IS WRAP::EXTEND_PAGE) goto extend_page;
            break;
         }

         case SCODE::TABLE_START: {
            // TODO: Recent changes to page layouts will mean that each cell will need to be processed as a page with
            // a dedicated layout class instant.

            // Table layout steps are as follows:
            //
            // 1. Copy prefixed/default widths and heights to all cells in the table.
            // 2. Calculate the size of each cell with respect to its content.  This can be left-to-right or
            //    top-to-bottom, it makes no difference.
            // 3. During the cell-layout process, keep track of the maximum width/height for the relevant
            //    row/column.  If either increases, make a second pass so that relevant cells are resized
            //    correctly.
            // 4. If the width of the rows is less than the requested table width (e.g. table width = 100%) then
            //    expand the cells to meet the requested width.
            // 5. Restart the page layout using the correct width and height settings for the cells.

            tablestate = *this;

            if (esctable) {
               auto ptr = esctable;
               esctable = &stream_data<bc_table>(Self, idx);
               esctable->stack = ptr;
            }
            else {
               esctable = &stream_data<bc_table>(Self, idx);
               esctable->stack = NULL;
            }

            esctable->reset_row_height = true; // All rows start with a height of min_height up until TABLE_END in the first pass
            esctable->compute_columns = 1;
            esctable->width = -1;

            for (unsigned j=0; j < esctable->columns.size(); j++) esctable->columns[j].min_width = 0;

            LONG width;
wrap_table_start:
            // Calculate starting table width, ensuring that the table meets the minimum width according to the cell
            // spacing and padding values.

            if (esctable->width_pct) {
               width = ((Width - (m_cursor_x - AbsX) - m_right_margin) * esctable->min_width) / 100;
            }
            else width = esctable->min_width;

            if (width < 0) width = 0;

            {
               DOUBLE min = (esctable->thickness * 2) + (esctable->cell_hspacing * (esctable->columns.size()-1)) + (esctable->cell_padding * 2 * esctable->columns.size());
               if (esctable->thin) min -= esctable->cell_hspacing * 2; // Thin tables do not have spacing on the left and right borders
               if (width < min) width = min;
            }

            if (width > WIDTH_LIMIT - m_cursor_x - m_right_margin) {
               log.traceWarning("Table width in excess of allowable limits.");
               width = WIDTH_LIMIT - m_cursor_x - m_right_margin;
               if (Self->BreakLoop > 4) Self->BreakLoop = 4;
            }

            if ((esctable->compute_columns) and (esctable->width >= width)) esctable->compute_columns = 0;

            esctable->width = width;

wrap_table_end:
wrap_table_cell:
            esctable->cursor_x    = m_cursor_x;
            esctable->cursor_y    = m_cursor_y;
            esctable->x           = m_cursor_x;
            esctable->y           = m_cursor_y;
            esctable->row_index   = 0;
            esctable->total_clips = m_clips.size();
            esctable->height      = esctable->thickness;

            DLAYOUT("(i%d) Laying out table of %dx%d, coords %.2fx%.2f,%dx%d%s, page width %.0f.",
               idx, LONG(esctable->columns.size()), esctable->rows, esctable->x, esctable->y, esctable->width, esctable->min_height, esctable->height_pct ? "%" : "", Width);

            esctable->computeColumns();

            DLAYOUT("Checking for table collisions before layout (%.2fx%.2f).  reset_row_height: %d", esctable->x, esctable->y, esctable->reset_row_height);

            auto ww = check_wordwrap("Table", AbsX, Width, idx, esctable->x, esctable->y, esctable->width, esctable->height);
            if (ww IS WRAP::EXTEND_PAGE) {
               DLAYOUT("Expanding page width due to table size.");
               goto extend_page;
            }
            else if (ww IS WRAP::WRAPPED) {
               // The width of the table and positioning information needs to be recalculated in the event of a
               // table wrap.

               DLAYOUT("Restarting table calculation due to page wrap to position %.2fx%.2f.", m_cursor_x, m_cursor_y);
               esctable->compute_columns = 1;
               goto wrap_table_start;
            }

            m_cursor_x = esctable->x;
            m_cursor_y = esctable->y + esctable->thickness + esctable->cell_vspacing;
            add_esc_segment();
            break;
         }

         case SCODE::TABLE_END: {
            auto action = procTableEnd(esctable, Offset, AbsX, Margins.Top, Margins.Bottom, Height, Width);
            if (action != TE::NIL) {
               *this = tablestate;
               if (action IS TE::WRAP_TABLE) goto wrap_table_end;
               else if (action IS TE::REPASS_ROW_HEIGHT) goto repass_row_height_ext;
               else if (action IS TE::EXTEND_PAGE) goto extend_page;
            }
            break;
         }

         case SCODE::ROW:
            stack_row.push(&stream_data<bc_row>(Self, idx));
            rowstate = *this;

            if (esctable->reset_row_height) stack_row.top()->row_height = stack_row.top()->min_height;

repass_row_height_ext:
            stack_row.top()->vertical_repass = false;
            stack_row.top()->y = m_cursor_y;
            esctable->row_width = (esctable->thickness<<1) + esctable->cell_hspacing;

            add_esc_segment();
            break;

         case SCODE::ROW_END: procRowEnd(esctable); break;

         case SCODE::CELL: {
            // In the first pass, the size of each cell is calculated with respect to its content.  When
            // SCODE::TABLE_END is reached, the max height and width for each row/column will be calculated
            // and a subsequent pass will be made to fill out the cells.
            //
            // If the width of a cell increases, there is a chance that the height of all cells in that
            // column will decrease, subsequently lowering the row height of all rows in the table, not just the
            // current row.  Therefore on the second pass the row heights need to be recalculated from scratch.

            bool vertical_repass = false;

            esccell = &stream_data<bc_cell>(Self, idx);

            if (!esctable) {
               log.warning("bc_table variable not defined for cell @ index %d - document byte code is corrupt.", idx);
               goto exit;
            }

            if (esccell->column >= LONG(esctable->columns.size())) {
               DLAYOUT("Cell %d exceeds total table column limit of %d.", esccell->column, LONG(esctable->columns.size()));
               break;
            }

            // Setting the line is the only way to ensure that the table graphics will be accounted for when drawing.

            stream_char start(idx, 0);
            stream_char stop(idx + 1, 0);
            add_drawsegment(start, stop, m_cursor_y, 0, 0, "Cell");

            // Set the AbsX location of the cel  AbsX determines the true location of the cell for do_layout()

            esccell->abs_x = m_cursor_x;
            esccell->abs_y = m_cursor_y;

            if (esctable->thin) {
               //if (esccell->column IS 0);
               //else esccell->AbsX += esctable->cell_hspacing;
            }
            else esccell->abs_x += esctable->cell_hspacing;

            if (esccell->column IS 0) esccell->abs_x += esctable->thickness;

            esccell->width  = esctable->columns[esccell->column].width; // Minimum width for the cell's column
            esccell->height = stack_row.top()->row_height;
            //DLAYOUT("%d / %d", escrow->min_height, escrow->row_height);

            DLAYOUT("Index %d, Processing cell at (%.2f,%.2fy), size (%.0f,%.0f), column %d",
               idx, m_cursor_x, m_cursor_y, esccell->width, esccell->height, esccell->column);

            // Find the matching CELL_END

            auto cell_end = idx;
            while (cell_end < INDEX(Self->Stream.size())) {
               if (Self->Stream[cell_end].code IS SCODE::CELL_END) {
                  auto &end = stream_data<bc_cell_end>(Self, cell_end);
                  if (end.cell_id IS esccell->cell_id) break;
               }

               cell_end++;
            }

            if (cell_end >= INDEX(Self->Stream.size())) {
               log.warning("Failed to find matching cell-end.  Document stream is corrupt.");
               goto exit;
            }

            idx++; // Go to start of cell content

            if (idx < cell_end) {
               auto segcount = m_segments.size();

               Self->EditMode = (!esccell->edit_def.empty()) ? true : false;

               layout sl(Self);
               idx = sl.do_layout(idx, cell_end, &m_font, esccell->abs_x, esccell->abs_y,
                  esccell->width, esccell->height, ClipRectangle(esctable->cell_padding), vertical_repass);

               if (!esccell->edit_def.empty()) Self->EditMode = false;

               if (!esccell->edit_def.empty()) {
                  // Edit cells have a minimum width/height so that the user can still interact with them when empty.

                  if (m_segments.size() IS segcount) {
                     // No content segments were created, which means that there's nothing for the cursor to attach
                     // itself too.

                     // Do we really want to do something here?
                     // I'd suggest that we instead break up the segments a bit more???  Another possibility - create an SCODE::NULL
                     // type that gets placed at the start of the edit cell.  If there's no genuine content, then we at least have the SCODE::NULL
                     // type for the cursor to be attached to?  SCODE::NULL does absolutely nothing except act as faux content.


                     // TODO Work on this problem next
                  }

                  if (esccell->width < 16) esccell->width = 16;
                  if (esccell->height < m_font->LineSpacing) esccell->height = m_font->LineSpacing;
               }
            }

            if (!idx) goto exit;

            DLAYOUT("Cell (%d:%d) is size %.0fx%.0f (min width %d)", esctable->row_index, esccell->column, esccell->width, esccell->height, esctable->columns[esccell->column].width);

            // Increase the overall width for the entire column if this cell has increased the column width.
            // This will affect the entire table, so a restart from TABLE_START is required.

            if (esctable->columns[esccell->column].width < esccell->width) {
               DLAYOUT("Increasing column width of cell (%d:%d) from %d to %.0f (table_start repass required).", esctable->row_index, esccell->column, esctable->columns[esccell->column].width, esccell->width);
               esctable->columns[esccell->column].width = esccell->width; // This has the effect of increasing the minimum column width for all cells in the column

               // Percentage based and zero columns need to be recalculated.  The easiest thing to do
               // would be for a complete recompute (ComputeColumns = true) with the new minwidth.  The
               // problem with ComputeColumns is that it does it all from scratch - we need to adjust it
               // so that it can operate in a second style of mode where it recognises temporary width values.

               esctable->columns[esccell->column].min_width = esccell->width; // Column must be at least this size
               esctable->compute_columns = 2;

               esctable->reset_row_height = true; // Row heights need to be reset due to the width increase
               *this = tablestate;
               goto wrap_table_cell;
            }

            // Advance the width of the entire row and adjust the row height

            esctable->row_width += esctable->columns[esccell->column].width;

            if (!esctable->thin) esctable->row_width += esctable->cell_hspacing;
            else if ((esccell->column + esccell->col_span) < LONG(esctable->columns.size())-1) esctable->row_width += esctable->cell_hspacing;

            if ((esccell->height > stack_row.top()->row_height) or (stack_row.top()->vertical_repass)) {
               // A repass will be required if the row height has increased and vectors or tables have been used
               // in earlier cells, because vectors need to know the final dimensions of their table cell.

               if (esccell->column IS LONG(esctable->columns.size())-1) {
                  DLAYOUT("Extending row height from %d to %.0f (row repass required)", stack_row.top()->row_height, esccell->height);
               }

               stack_row.top()->row_height = esccell->height;
               if ((esccell->column + esccell->col_span) >= LONG(esctable->columns.size())) {
                  *this = rowstate;
                  goto repass_row_height_ext;
               }
               else stack_row.top()->vertical_repass = true; // Make a note to do a vertical repass once all columns on this row have been processed
            }

            m_cursor_x += esctable->columns[esccell->column].width;

            if (!esctable->thin) m_cursor_x += esctable->cell_hspacing;
            else if ((esccell->column + esccell->col_span) < LONG(esctable->columns.size())) m_cursor_x += esctable->cell_hspacing;

            if (esccell->column IS 0) m_cursor_x += esctable->thickness;
            break;
         }

         default: break;
      }
   }

   // Check if the cursor + any remaining text requires closure

   if ((m_cursor_x + m_word_width > m_left_margin) or (m_word_index.valid())) {
      stream_char sc(idx, 0);
      end_line(NL::NONE, 0, sc, "SectionEnd");
   }

exit:

   page_height = calc_page_height(AbsY, Margins.Bottom);

   // Force a second pass if the page height has increased and there are vectors in the page (the vectors may need
   // to know the page height - e.g. if there is a gradient filling the background).
   //
   // This requirement is also handled in SCODE::CELL, so we only perform it here if processing is occurring within the
   // root page area (Offset of 0).

   if ((!Offset) and (VerticalRepass) and (last_height < page_height)) {
      DLAYOUT("============================================================");
      DLAYOUT("SECOND PASS [%d]: Root page height increased from %d to %.0f", Offset, last_height, page_height);
      goto extend_page;
   }

   *Font = m_font;
   if (page_height > Height) Height = page_height;

   Self->Depth--;

   if (!stack_link.empty()) log.warning("Sanity check for stack_link failed at end of layout.");
   if (!stack_mklink.empty()) log.warning("Sanity check for stack_mklink failed at end of layout.");

   return idx;
}

//********************************************************************************************************************
// This function is called only when a paragraph or explicit line-break (\n) is encountered.

void layout::end_line(NL NewLine, DOUBLE Spacing, stream_char Next, const std::string &Caller)
{
   pf::Log log(__FUNCTION__);

   if ((!m_line.height) and (m_word_width)) {
      // If this is a one-word line, the line height will not have been defined yet
      m_line.height = m_font->LineSpacing;
      m_line.gutter = m_font->LineSpacing - m_font->Ascent;
   }

   if (m_terminate_link) terminate_link();
   else if ((!stack_link.empty()) and (m_cursor_x + m_word_width > stack_mklink.top().x)) {
      // A link is active and will continue to the next line.

      add_link(SCODE::LINK, stack_link.top(), stack_mklink.top().x, m_cursor_y,
         m_cursor_x + m_word_width - stack_mklink.top().x,
         m_line.height ? m_line.height : m_font->LineSpacing, "link_end");
      stack_mklink.top().x = m_left_margin;
   }

#ifdef DBG_LAYOUT
   log.branch("%s: CursorX/Y: %.2f/%.2f, ParaY: %d, ParaEnd: %d, Line Height: %.0f * %.2f, Span: %d:%d - %d:%d",
      Caller.c_str(), m_cursor_x, m_cursor_y, m_paragraph_y, m_paragraph_bottom, m_line.height, Spacing,
      m_line.index.index, LONG(m_line.index.offset), Next.index, LONG(Next.offset));
#endif

   for (auto &clip : m_clips) {
      if (clip.transparent) continue;
      if ((m_cursor_y + m_line.height >= clip.top) and (m_cursor_y < clip.bottom)) {
         if (m_cursor_x + m_word_width < clip.left) {
            if (clip.left < m_align_width) m_align_width = clip.left;
         }
      }
   }

   if (idx > m_line.index.index) {
      stream_char sc(idx, 0);
      add_drawsegment(m_line.index, sc, m_cursor_y, m_cursor_x + m_word_width - m_line.x, m_align_width - m_line.x, "Esc:EndLine");
   }

   if (NewLine != NL::NONE) {
      // Determine the new vertical position of the cursor.  This subroutine takes into account multiple line-breaks, so that
      // the overall amount of whitespace is no more than the biggest line-break specified in a line-break sequence.

      auto bottom_line = m_cursor_y + m_line.height;
      if (m_paragraph_bottom > bottom_line) bottom_line = m_paragraph_bottom;

      m_paragraph_y = bottom_line;
      if (!m_line.height) {
         // The line is devoid of content, e.g. in the case of "<p>...<p>...</p></p>" the "</p></p>" is empty.
         // The m_cursor_y position will not be advanced in this case.
      }
      else {
         // Paragraph gap measured as default line height * spacing ratio

         auto advance_to = bottom_line + F2I(Self->LineHeight * Spacing);
         if (advance_to > m_cursor_y) m_cursor_y = advance_to;
      }
   }

   // Reset line management variables for a new line starting from the left margin.

   m_line.full_reset(m_left_margin);
   m_line.index       = Next;
   m_cursor_x         = m_left_margin;
   m_split_start      = m_segments.size();
   m_word_index       = m_line.index;
   m_kernchar         = 0;
   m_word_width       = 0;
   m_paragraph_bottom = 0;
}

//********************************************************************************************************************
// This function will check the need for word wrapping of an element marked by the area (x, y, width, height).  The
// (x, y) position will be updated if the element is wrapped.  If clipping boundaries are present on the page,
// horizontal advancement across the line may occur.  Some layout state variables are also updated if a wrap occurs,
// e.g. the cursor position.
//
// Wrapping can be checked even if there is no 'active word' because we need to be able to wrap empty lines (e.g.
// solo <br/> tags).

WRAP layout::check_wordwrap(const std::string &Type, LONG AbsX, DOUBLE &PageWidth, stream_char Cursor,
   DOUBLE &X, DOUBLE &Y, LONG Width, LONG Height)
{
   pf::Log log(__FUNCTION__);

   if (!Self->BreakLoop) return WRAP::DO_NOTHING;
   if (Width < 1) Width = 1;

#ifdef DBG_WORDWRAP
   log.branch("Index: %d/%d, %s: %dx%d,%dx%d, LineHeight: %d, Cursor: %.2fx%.2f, PageWidth: %d, Edge: %d",
      idx, Cursor.index, type.c_str(), x, y, width, height, m_line.height, m_cursor_x, m_cursor_y, width, m_wrap_edge);
#endif

   auto result = WRAP::DO_NOTHING;
   LONG breakloop = MAXLOOP; // Employ safety measures to prevent a loop trap.

restart:
   m_align_width = m_wrap_edge;

   if (!m_clips.empty()) { // If clips are registered then we need to check them for collisions.
      wrap_through_clips(Cursor, X, Y, Width, Height);
   }

   if (X + Width > m_wrap_edge) {
      if ((PageWidth < WIDTH_LIMIT) and ((X IS m_left_margin) or (m_no_wrap))) {
         // Force an extension of the page width and recalculate from scratch
         auto min_width = X + Width + m_right_margin - AbsX;
         if (min_width > PageWidth) {
            PageWidth = min_width;
            DWRAP("Forcing an extension of the page width to %d", min_width);
         }
         else PageWidth += 1;
         return WRAP::EXTEND_PAGE;
      }

      if (!m_line.height) {
         m_line.height = 1;
         m_line.gutter = 0;
      }

      if ((!stack_link.empty()) and (stack_mklink.top().x != X)) {
         add_link(SCODE::LINK, stack_link.top(), stack_mklink.top().x, Y, X - stack_mklink.top().x, m_line.height, "check_wrap");
      }

      // Set the line segment up to the cursor.  The line.index is updated so that this process only occurs
      // in the first iteration.

      if (m_line.index < Cursor) {
         add_drawsegment(m_line.index, Cursor, Y, X - m_line.x, m_align_width - m_line.x, "DoWrap");
         m_line.index = Cursor;
      }

      // Reset the line management variables so that the next line starts at the left margin.

      X  = m_left_margin;
      Y += m_line.height;

      m_cursor_x    = X;
      m_cursor_y    = Y;
      m_split_start = m_segments.size();
      m_kernchar    = 0;

      m_line.reset(m_left_margin);

      if (!stack_mklink.empty()) stack_mklink.top().x = m_left_margin;

      result = WRAP::WRAPPED;
      if (--breakloop > 0) goto restart; // Go back and check the clip boundaries again
      else {
         log.traceWarning("Breaking out of continuous loop.");
         Self->Error = ERR_Loop;
      }
   }

   if (m_terminate_link) { // Check if a link termination is pending for this word
      terminate_link();
   }

   #ifdef DBG_WORDWRAP
      if (result IS WRAP::WRAPPED) log.msg("A wrap to Y coordinate %.2f has occurred.", m_cursor_y);
   #endif

   return result;
}

//********************************************************************************************************************
// Compare a given area against clip regions and move the x,y position when there's an intersection.

void layout::wrap_through_clips(stream_char WordIndex, DOUBLE &X, DOUBLE &Y, LONG Width, LONG Height)
{
   pf::Log log(__FUNCTION__);

#ifdef DBG_WORDWRAP
   log.branch("Index: %d-%d, WordIndex: %d, Graphic: %dx%d,%dx%d, TotalClips: %d",
      m_line.index.index, idx, WordIndex, x, y, width, height, LONG(m_clips.size()));
#endif

restart:
   for (auto &clip : m_clips) {
      if (clip.transparent) continue;
      if ((Y + Height < clip.top) or (Y >= clip.bottom)) continue;
      if ((X >= clip.right) or (X + Width < clip.left)) continue;

      if (clip.left < m_align_width) m_align_width = clip.left;

      DWRAP("Word: \"%.20s\" (%dx%d,%dx%d) advances over clip %d-%d",
         printable(Self, WordIndex).c_str(), X, Y, Width, Height, clip.left, clip.right);

      // Set the line segment up to the encountered boundary and continue checking the vector position against the
      // clipping boundaries.

      bool reset_link;
      if ((!stack_link.empty()) and (clip.index < stack_mklink.top().index)) {
         // An open link intersects with a clipping region that was created prior to the opening of the link.  We do
         // not want to include this vector as a clickable part of the link - we will wrap over or around it, so
         // set a partial link now and ensure the link is reopened after the clipping region.

         DWRAP("Setting hyperlink now to cross a clipping boundary.");

         auto height = m_line.height ? m_line.height : m_font->LineSpacing;
         add_link(SCODE::LINK, stack_link.top(), stack_mklink.top().x, Y, X + Width - stack_mklink.top().x, height, "clip_intersect");

         reset_link = true;
      }
      else reset_link = false;

      // Advance the position.  We break if a wordwrap is required - the code outside of this loop will detect
      // the need for a wordwrap and then restart the wordwrapping process.

      if (X IS m_line.x) m_line.x = clip.right;
      X = clip.right; // Go past the clip boundary

      if (X + Width > m_wrap_edge) {
         DWRAP("Wrapping-Break: X(%d)+Width(%d) > Edge(%d) at clip '%s' %dx%d,%dx%d",
            X, Width, m_wrap_edge, clip.name.c_str(), clip.left, clip.top, clip.right, clip.bottom);
         break;
      }

      if (m_line.index < WordIndex) {
         if (!m_line.height) add_drawsegment(m_line.index, WordIndex, Y, X - m_line.x, X - m_line.x, "Wrap:EmptyLine");
         else add_drawsegment(m_line.index, WordIndex, Y, X + Width - m_line.x, m_align_width - m_line.x, "Wrap");
      }

      DWRAP("Line index reset to %d, previously %d", WordIndex.index, m_line.index.index);

      m_line.index = WordIndex;
      m_line.x = X;
      if ((reset_link) and (!stack_link.empty())) stack_mklink.top().x = X;

      goto restart; // Check all the clips from the beginning
   }
}

//********************************************************************************************************************
// Record a clickable link, cell, or other form of clickable area.

void layout::add_link(SCODE base_code, std::variant<bc_link *, bc_cell *> Escape,
   DOUBLE X, DOUBLE Y, DOUBLE Width, DOUBLE Height, const std::string &Caller)
{
   pf::Log log(__FUNCTION__);

   if ((Width < 0.01) or (Height < 0.01)) {
      log.traceWarning("Illegal link dimensions of (%.0fx%.0f, %.0fx%.0f) [%s]", X, Y, Width, Height, Caller.c_str());
      return;
   }

   DLAYOUT("#%d (%.0fx%.0f, %.0fx%.0f), %s", LONG(m_links.size()), X, Y, Width, Height, Caller.c_str());

   m_links.emplace_back(base_code, Escape, m_segments.size(), X, Y, Width, Height);
}

//********************************************************************************************************************
// Calculate the page height, which is either going to be the coordinate of the bottom-most line, or one of the
// clipping regions if one of them extends further than the bottom-most line.

DOUBLE layout::calc_page_height(DOUBLE Y, DOUBLE BottomMargin)
{
   pf::Log log(__FUNCTION__);

   if (Self->Segments.empty()) return 0;

   // Find the last segment that had text and use that to determine the bottom of the page

   DOUBLE height = 0;
   DOUBLE y = 0;
   SEGINDEX last = Self->Segments.size() - 1;
   while ((last > 0) and (!height) and (!y)) {
      if (Self->Segments[last].text_content) {
         height = Self->Segments[last].area.Height;
         y = Self->Segments[last].area.Y;
         break;
      }
      last--;
   }

   auto page_height = y + height;

   // Extend the height if a clipping region passes the last line of text.

   for (auto &clip : Self->Clips) {
      if (clip.transparent) continue;
      if (clip.bottom > page_height) page_height = clip.bottom;
   }

   // Add the bottom margin and subtract the y offset so that we have the true height of the page/cell.

   page_height = page_height + BottomMargin - Y;

   log.trace("Page Height: %.2f + %.2f -> %.2f, Bottom: %.2f, Y: %.2f",
      Self->Segments.back().area.Y, Self->Segments.back().area.Height, page_height, BottomMargin, Y);

   return page_height;
}
