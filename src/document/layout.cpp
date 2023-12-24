/*

The layout process involves reading the serialised document stream and generating line segments that declare
regions for graphics content.  These segments have a dual purpose in that they can also be used for user
interaction.

The trickiest parts of the layout process are state management, word wrapping and page width extension.

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
Padding:      Padding inside each cell (margins)
Spacing:      Spacing between cells.

For complex tables with different coloured borders between cells, allocate single-pixel sized cells with the background
colour set to the desired value in order to create the illusion of multi-coloured cell borders.

The page area owned by a table is given a clipping zone by the page layout engine, in the same way that objects are
given clipping zones.  This allows text to be laid out around the table with no effort on the part of the developer.

*/

// State machine for the layout process.  This information is discarded post-layout.

struct layout {
   extDocument *Self;
   LONG  m_break_loop = MAXLOOP;
   UWORD m_depth = 0;     // Section depth - increases when do_layout() recurses, e.g. into tables

   std::vector<doc_segment> m_segments;
   std::vector<edit_cell>   m_ecells;

private:
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
   std::stack<bc_font *>      stack_font;

   std::vector<doc_clip>    m_clips;

   objFont *m_font = NULL;

   DOUBLE m_cursor_x = 0, m_cursor_y = 0; // Insertion point of the next text character or vector object
   DOUBLE m_page_width = 0;
   INDEX idx = 0;               // Current seek position for processing of the stream
   stream_char m_word_index;    // Position of the word currently being operated on
   LONG m_align_flags = 0;      // Current alignment settings according to the font style
   LONG m_align_width = 0;      // Horizontal alignment will be calculated relative to this value
   LONG m_kernchar    = 0;      // Previous character of the word being operated on
   LONG m_left_margin = 0, m_right_margin = 0; // Margins control whitespace for paragraphs and table cells
   LONG m_paragraph_bottom = 0; // Bottom Y coordinate of the current paragraph; defined on paragraph end.
   LONG m_paragraph_y  = 0;     // The vertical position of the current paragraph
   SEGINDEX m_line_seg_start = 0; // Set to the starting segment of a new line.  Resets on end_line() or wordwrap.  Used for ensuring that all distinct entries on the line use the same line height
   LONG m_word_width   = 0;     // Pixel width of the current word
   LONG m_wrap_edge    = 0;     // Marks the boundary at which graphics and text will need to wrap.
   WORD m_space_width  = 0;      // Caches the pixel width of a single space in the current font.
   bool m_inline       = false; // Set to true when graphics (vectors, images) must be inline.
   bool m_no_wrap      = false; // Set to true when word-wrap is disabled.
   bool m_cursor_drawn = false; // Set to true when the cursor has been drawn during scene graph creation.
   bool m_edit_mode    = false; // Set to true when inside an area that allows user editing of the content.

   struct {
      stream_char index;   // Stream position for the line's content.
      DOUBLE gutter;       // Amount of vertical spacing appropriated for text.  Inclusive within the height value, not additive
      DOUBLE height;       // The complete height of the line, including inline vectors/images/tables.  Text is drawn so that the text gutter is aligned to the base line
      DOUBLE x;            // Starting horizontal position
      DOUBLE word_height;  // Height of the current word (including inline graphics), utilised for word wrapping

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

   inline void reset() {
      m_clips.clear();
      m_ecells.clear();
      m_segments.clear();

      stack_list = {};
      stack_para = {};
      stack_row  = {};
      stack_font = {};

      m_align_flags      = 0;
      m_paragraph_y      = 0;
      m_paragraph_bottom = 0;
      m_word_width       = 0;
      m_kernchar         = 0;
      m_inline           = false;
      m_no_wrap          = false;
   }

   // Resets the string management variables, usually done when a string
   // has been broken up on the current line due to a vector or table graphic for example.

   inline void reset_segment(INDEX Index, LONG X) {
      m_word_index.reset();

      m_line.index.set(Index);
      m_line.x       = X;
      m_kernchar     = 0;
      m_word_width   = 0;
   }

   inline void reset_segment() { reset_segment(idx, m_cursor_x); }

   // Add a segment for a single byte code at position idx.  This will not include support for text glyphs,
   // so extended information is not required.

   inline void add_graphics_segment() {
      new_segment(stream_char(idx, 0), stream_char(idx + 1, 0), m_cursor_y, 0, 0, BC_NAME(Self->Stream, idx));
      reset_segment(idx+1, m_cursor_x);
   }

   // Add a segment for a single byte code at position idx.  For use by non-graphical codes only (i.e. no
   // graphics region).

   inline void add_code_segment() {
      new_code_segment(stream_char(idx, 0), stream_char(idx + 1, 0), BC_NAME(Self->Stream, idx));
   }

   // When lines are segmented, the last segment will store the final height of the line whilst the earlier segments
   // will have the wrong height.  This function ensures that all segments for a line have the same height and gutter
   // values.

   inline void sanitise_line_height() {
      auto end = SEGINDEX(m_segments.size());
      if (end > m_line_seg_start) {
         auto final_height = m_segments[end-1].area.Height;
         auto final_gutter = m_segments[end-1].gutter;

         if (final_height) {
            for (auto i=m_line_seg_start; i < end; i++) {
               if (m_segments[i].depth != m_depth) continue;
               m_segments[i].area.Height = final_height;
               m_segments[i].gutter      = final_gutter;
            }
         }
      }
   }

   // If the current font is larger or equal to the current line height, extend the line height.
   // Note that we use >= because we want to correct the base line in case there is a vector already set on the
   // line that matches the font's line spacing.

   inline void check_line_height() {
      if (m_font->LineSpacing >= m_line.height) {
         m_line.height = m_font->LineSpacing;
         m_line.gutter = m_font->LineSpacing - m_font->Ascent;
      }
   }

   CELL lay_cell(bc_table *);
   void lay_cell_end();
   void lay_font();
   void lay_font_end();
   WRAP lay_image();
   void lay_index();
   bool lay_list_end();
   void lay_paragraph_end();
   void lay_paragraph();
   void lay_row_end(bc_table *);
   void lay_set_margins(LONG &);
   TE   lay_table_end(bc_table *, LONG, DOUBLE, DOUBLE, DOUBLE &, DOUBLE &);
   WRAP lay_text();

   DOUBLE calc_page_height(DOUBLE);
   WRAP check_wordwrap(const std::string &, DOUBLE &, stream_char, DOUBLE &, DOUBLE &, DOUBLE, DOUBLE);
   void end_line(NL, DOUBLE, stream_char, const std::string &);
   void new_code_segment(const stream_char, const stream_char, const std::string &);
   void new_segment(const stream_char, const stream_char, DOUBLE, DOUBLE, DOUBLE, const std::string &);
   void wrap_through_clips(stream_char, DOUBLE &, DOUBLE &, DOUBLE, DOUBLE);

public:
   layout(extDocument *pSelf) : Self(pSelf) { }

   ERROR do_layout(INDEX, INDEX, objFont **, DOUBLE &, DOUBLE &, ClipRectangle, bool &);
   void gen_scene_graph(objVectorViewport *, RSTREAM &, SEGINDEX, SEGINDEX);
   void gen_scene_init(objVectorViewport *);
};

//********************************************************************************************************************
// In the first pass, the size of each cell is calculated with respect to its content.  When SCODE::TABLE_END is
// reached, the max height and width for each row/column will be calculated and a subsequent pass will be made to
// fill out the cells.
//
// If the width of a cell increases, there is a chance that the height of all cells in that column will decrease,
// subsequently lowering the row height of all rows in the table, not just the current row.  Therefore on the second
// pass the row heights need to be recalculated from scratch.

CELL layout::lay_cell(bc_table *Table)
{
   pf::Log log(__FUNCTION__);

   bool vertical_repass = false;

   auto &cell = stream_data<bc_cell>(Self, idx);

   if (!Table) {
      log.warning("Table not defined for cell @ index %d - document byte code is corrupt.", idx);
      return CELL::ABORT;
   }

   if (cell.column >= LONG(Table->columns.size())) {
      DLAYOUT("Cell %d exceeds total table column limit of %d.", cell.column, LONG(Table->columns.size()));
      return CELL::NIL;
   }

   // Adding a segment ensures that the table graphics will be accounted for when drawing.

   new_segment(stream_char(idx, 0), stream_char(idx + 1, 0), m_cursor_y, 0, 0, "Cell");

   // Set the absolute coordinates of the cell within the viewport.

   cell.x = m_cursor_x;
   cell.y = m_cursor_y;

   if (Table->collapsed) {
      //if (cell.column IS 0);
      //else cell.AbsX += Table->cell_hspacing;
   }
   else cell.x += Table->cell_hspacing;

   if (cell.column IS 0) cell.x += Table->strokeWidth;

   cell.width  = Table->columns[cell.column].width; // Minimum width for the cell's column
   cell.height = stack_row.top()->row_height;
   //DLAYOUT("%d / %d", escrow->min_height, escrow->row_height);

   DLAYOUT("Index %d, Processing cell at (%g,%gy), size (%g,%g), column %d",
      idx, m_cursor_x, m_cursor_y, cell.width, cell.height, cell.column);

   // Find the matching CELL_END

   auto cell_end_idx = cell.find_cell_end(Self, Self->Stream, idx);

   if (cell_end_idx IS -1) {
      log.warning("Failed to find matching cell-end.  Document stream is corrupt.");
      return CELL::ABORT;
   }

   auto cell_end = &stream_data<bc_cell_end>(Self, cell_end_idx);
   cell_end->cell_start = &cell;

   idx++; // Go to start of cell content

   if (idx < cell_end_idx) {
      m_edit_mode = (!cell.edit_def.empty()) ? true : false;

      layout sl(Self);
      sl.m_depth = m_depth;
      sl.do_layout(idx, cell_end_idx, &m_font, cell.width, cell.height,
         ClipRectangle(Table->cell_padding), vertical_repass);

      // The main product of do_layout() are the produced segments, so append them here.

      m_segments.insert(m_segments.end(), sl.m_segments.begin(), sl.m_segments.end());

      idx = cell_end_idx - 1; // -1 to make up for the loop's ++

      if (!cell.edit_def.empty()) {
         m_edit_mode = false;

         // Edit cells have a minimum width/height so that the user can still interact with them when empty.

         if (sl.m_segments.empty()) {
            // No content segments were created, which means that there's nothing for the editing cursor to attach
            // itself too.

            // TODO: Maybe ceate an SCODE::NOP type that gets placed at the start of the edit cell.  If there's no genuine
            // content, then we at least have the SCODE::NOP type for the cursor to be attached to?
         }

         if (cell.width < 16) cell.width = 16;
         if (cell.height < m_font->LineSpacing) cell.height = m_font->LineSpacing;
      }
   }

   DLAYOUT("Cell (%d:%d) is size (%gx%g, %gx%g); min. width %g", Table->row_index, cell.column,
      cell.x, cell.y, cell.width, cell.height, Table->columns[cell.column].width);

   // Increase the overall width for the entire column if this cell has increased the column width.
   // This will affect the entire table, so a restart from TABLE_START is required.

   if (Table->columns[cell.column].width < cell.width) {
      DLAYOUT("Increasing column width of cell (%d:%d) from %g to %g (table_start repass required).", Table->row_index, cell.column, Table->columns[cell.column].width, cell.width);
      Table->columns[cell.column].width = cell.width; // This has the effect of increasing the minimum column width for all cells in the column

      // Percentage based and zero columns need to be recalculated.  The easiest thing to do
      // would be for a complete recompute (ComputeColumns = true) with the new minwidth.  The
      // problem with ComputeColumns is that it does it all from scratch - we need to adjust it
      // so that it can operate in a second style of mode where it recognises temporary width values.

      Table->columns[cell.column].min_width = cell.width; // Column must be at least this size
      Table->compute_columns = 2;
      Table->reset_row_height = true; // Row heights need to be reset due to the width increase
      return CELL::WRAP_TABLE_CELL;
   }

   // Advance the width of the entire row and adjust the row height

   Table->row_width += Table->columns[cell.column].width;

   if (!Table->collapsed) Table->row_width += Table->cell_hspacing;
   else if ((cell.column + cell.col_span) < LONG(Table->columns.size())-1) Table->row_width += Table->cell_hspacing;

   if ((cell.height > stack_row.top()->row_height) or (stack_row.top()->vertical_repass)) {
      // A repass will be required if the row height has increased and vectors or tables have been used
      // in earlier cells, because vectors need to know the final dimensions of their table cell (viewport).

      if (cell.column IS LONG(Table->columns.size())-1) {
         DLAYOUT("Extending row height from %g to %g (row repass required)", stack_row.top()->row_height, cell.height);
      }

      stack_row.top()->row_height = cell.height;
      if ((cell.column + cell.col_span) >= LONG(Table->columns.size())) {
         return CELL::REPASS_ROW_HEIGHT;
      }
      else stack_row.top()->vertical_repass = true; // Make a note to do a vertical repass once all columns on this row have been processed
   }

   m_cursor_x += Table->columns[cell.column].width;

   if (!Table->collapsed) m_cursor_x += Table->cell_hspacing;
   else if ((cell.column + cell.col_span) < LONG(Table->columns.size())) m_cursor_x += Table->cell_hspacing;

   if (cell.column IS 0) m_cursor_x += Table->strokeWidth;

   return CELL::NIL;
}

//********************************************************************************************************************

void layout::lay_cell_end()
{
   auto cell_end = &stream_data<bc_cell_end>(Self, idx);

   if ((cell_end->cell_start) and (!cell_end->cell_start->edit_def.empty())) {
      // The area of each edit cell is logged for assisting interaction between the mouse pointer and the cells.

      auto cs = cell_end->cell_start;
      m_ecells.emplace_back(cell_end->cell_id, cs->x, cs->y, cs->width, cs->height);
   }

   // CELL_END helps draw(), so set the segment to ensure that it is included in the draw stream.  Please
   // refer to SCODE::CELL to see how content is processed and how the cell dimensions are formed.

   add_graphics_segment();
}

//********************************************************************************************************************
// Calculate the image position.  The host rectangle is modified in gen_scene_graph() as this is the most optimal
// approach (i.e. if the page width expands during layout).
//
// NOTE: If you ever see an image unexpectedly appearing at (0,0) it's because it hasn't been included in a draw
// segment.

WRAP layout::lay_image()
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

      add_graphics_segment();

      // For a floating image we need to declare a clip region based on the final image dimensions.
      // TODO: Add support for masked clipping through SVG paths.

      m_clips.emplace_back(image.x, m_cursor_y,
         image.x + image.final_pad.left + image.final_width + image.final_pad.right,
         m_cursor_y + image.final_pad.top + image.final_height + image.final_pad.bottom,
         idx, false, "Image");
   }
   else { // Image is inline and must be treated like a text character.
      if (!m_word_width) m_word_index.set(idx); // Save the index of the new word

      // Checking for wordwrap here is optimal, BUT bear in mind that if characters immediately follow the
      // image then it is also possible for word-wrapping to occur later.  Note that the line height isn't
      // adjusted in this call because if a wrap occurs then the image won't be in the former segment.

      wrap_result = check_wordwrap("Image", m_page_width, m_word_index,
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

void layout::lay_font()
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

      DLAYOUT("Font Index: %d, LineSpacing: %d, Pt: %g, Height: %d, Ascent: %d, Cursor: %gx%g",
         style->font_index, m_font->LineSpacing, m_font->Point, m_font->Height, m_font->Ascent, m_cursor_x, m_cursor_y);
      m_space_width = fntCharWidth(m_font, ' ', 0, 0);

      // Treat the font as if it is a text character by setting the m_word_index.
      // This ensures it is included in the drawing process.

      if (!m_word_width) m_word_index.set(idx);
   }
   else DLAYOUT("ESC_FONT: Unable to lookup font using style index %d.", style->font_index);

   stack_font.push(&stream_data<bc_font>(Self, idx));
}

void layout::lay_font_end()
{
   stack_font.pop();
   if (!stack_font.empty()) m_font = stack_font.top()->get_font();
}

//********************************************************************************************************************
// NOTE: Bear in mind that the first word in a TEXT string could be a direct continuation of a previous TEXT word.
// This can occur if the font colour is changed mid-word for example.

WRAP layout::lay_text()
{
   auto wrap_result = WRAP::DO_NOTHING; // Needs to to change to WRAP::EXTEND_PAGE if a word is > width

   m_align_width = m_wrap_edge; // TODO: Not sure about this following the switch to embedded TEXT structures

   auto ascent = m_font->Ascent;
   auto &text = stream_data<bc_text>(Self, idx);
   auto &str = text.text;
   text.vector_text.clear();
   for (unsigned i=0; i < str.size(); ) {
      if (str[i] IS '\n') { // The use of '\n' in a string forces a line break
         check_line_height();
         wrap_result = check_wordwrap("Text", m_page_width, m_word_index, m_cursor_x, m_cursor_y, m_word_width,
            (m_line.height < 1) ? 1 : m_line.height);
         if (wrap_result IS WRAP::EXTEND_PAGE) break;

         stream_char end(idx, i);
         end_line(NL::PARAGRAPH, 0, end, "CR");
         i++;
      }
      else if (str[i] <= 0x20) { // Whitespace encountered
         check_line_height();

         if (m_word_width) { // Existing word finished, check if it will wordwrap
            wrap_result = check_wordwrap("Text", m_page_width, m_word_index, m_cursor_x, m_cursor_y,
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
         m_kernchar   = 0;
         m_word_width = 0;
         i++;
      }
      else {
         if (!m_word_width) {
            m_word_index.set(idx, i);   // Save the index of the new word
            check_line_height();
         }

         LONG unicode, kerning;
         i += getutf8(str.c_str()+i, &unicode);
         m_word_width += fntCharWidth(m_font, unicode, m_kernchar, &kerning);
         m_word_width += kerning;
         m_kernchar    = unicode;

         if (ascent > m_line.word_height) m_line.word_height = ascent;
      }
   }

   // Entire text string has been processed, perform one final wrapping check.

   if (m_word_width) {
      wrap_result = check_wordwrap("Text", m_page_width, m_word_index, m_cursor_x, m_cursor_y,
         m_word_width, (m_line.height < 1) ? 1 : m_line.height);
   }

   return wrap_result;
}

//********************************************************************************************************************
// Returns true if a repass is required

bool layout::lay_list_end()
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

void layout::lay_index()
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

            m_line.index.set(end);
            idx = end;
            return;
         }

         end++;
      }

      log.warning("Failed to find matching index-end.  Document stream is corrupt.");
   }
}

//********************************************************************************************************************

void layout::lay_row_end(bc_table *Table)
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
   DLAYOUT("Row ends, advancing down by %g+%g, new height: %g, y-cursor: %g",
      Row->row_height, Table->cell_vspacing, Table->height, m_cursor_y);

   if (Table->row_width > Table->width) Table->width = Table->row_width;

   stack_row.pop();
   add_graphics_segment();
}

//********************************************************************************************************************

void layout::lay_paragraph()
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

void layout::lay_paragraph_end()
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

TE layout::lay_table_end(bc_table *esctable, LONG Offset, DOUBLE TopMargin, DOUBLE BottomMargin, DOUBLE &Height, DOUBLE &Width)
{
   pf::Log log(__FUNCTION__);

   DOUBLE minheight;

   if (esctable->cells_expanded IS false) {
      LONG unfixed, colwidth;

      // Table cells need to match the available width inside the table.  This routine checks for that - if the cells
      // are short then the table processing is restarted.

      DLAYOUT("Checking table @ index %d for cell/table widening.  Table width: %g", idx, esctable->width);

      esctable->cells_expanded = true;

      if (!esctable->columns.empty()) {
         colwidth = (esctable->strokeWidth * 2) + esctable->cell_hspacing;
         for (auto &col : esctable->columns) {
            colwidth += col.width + esctable->cell_hspacing;
         }
         if (esctable->collapsed) colwidth -= esctable->cell_hspacing * 2; // Collapsed tables have no spacing allocated on the sides

         if (colwidth < esctable->width) { // Cell layout is less than the pre-determined table width
            // Calculate the amount of additional space that is available for cells to expand into

            LONG avail_width = esctable->width - (esctable->strokeWidth * 2) -
               (esctable->cell_hspacing * (esctable->columns.size() - 1));

            if (!esctable->collapsed) avail_width -= (esctable->cell_hspacing * 2);

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
                        DLAYOUT("Expanding column %d from width %g to %g", j, esctable->columns[j].width, cell_width);
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
   else DLAYOUT("Cells already widened - keeping table width of %g.", esctable->width);

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

   if (minheight > esctable->height + esctable->cell_vspacing + esctable->strokeWidth) {
      // The last row in the table needs its height increased
      if (!stack_row.empty()) {
         auto h = minheight - (esctable->height + esctable->cell_vspacing + esctable->strokeWidth);
         DLAYOUT("Extending table height to %g (row %g+%g) due to a minimum height of %g at coord %g",
            minheight, stack_row.top()->row_height, h, esctable->min_height, esctable->y);
         stack_row.top()->row_height += h;
         return TE::REPASS_ROW_HEIGHT;
      }
      else log.warning("No last row defined for table height extension.");
   }

   // Adjust for cellspacing at the bottom

   esctable->height += esctable->cell_vspacing + esctable->strokeWidth;

   // Restart if the width of the table will force an extension of the page.

   LONG j = esctable->x + esctable->width + m_right_margin;
   if ((j > Width) and (Width < WIDTH_LIMIT)) {
      DLAYOUT("Table width (%g+%g) increases page width to %d, layout restart forced.",
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

   DLAYOUT("Checking table collisions (%gx%g, %gx%g).", esctable->x, esctable->y, esctable->width, esctable->height);

   WRAP ww;
   if (esctable->total_clips > m_clips.size()) {
      std::vector<doc_clip> saved_clips(m_clips.begin() + esctable->total_clips, m_clips.end() + m_clips.size());
      m_clips.resize(esctable->total_clips);
      ww = check_wordwrap("Table", m_page_width, idx, esctable->x, esctable->y, esctable->width, esctable->height);
      m_clips.insert(m_clips.end(), saved_clips.begin(), saved_clips.end());
   }
   else ww = check_wordwrap("Table", m_page_width, idx, esctable->x, esctable->y, esctable->width, esctable->height);

   if (ww IS WRAP::EXTEND_PAGE) {
      DLAYOUT("Table wrapped - expanding page width due to table size/position.");
      return TE::EXTEND_PAGE;
   }
   else if (ww IS WRAP::WRAPPED) {
      // A repass is necessary as everything in the table will need to be rearranged
      DLAYOUT("Table wrapped - rearrangement necessary.");
      return TE::WRAP_TABLE;
   }

   // The table sets a clipping region because its surrounds are available as whitespace for
   // other content.

   m_clips.emplace_back(
      esctable->x, esctable->y, esctable->x + esctable->width, esctable->y + esctable->height,
      idx, false, "Table");

   m_cursor_x = esctable->x + esctable->width;
   m_cursor_y = esctable->y;

   DLAYOUT("Final Table Size: %gx%g,%gx%g", esctable->x, esctable->y, esctable->width, esctable->height);

   esctable = esctable->stack;

   add_graphics_segment(); // Technically table-end is a graphics segment because tables can be inline
   return TE::NIL;
}

//********************************************************************************************************************
// Embedded vectors are always contained by a VectorViewport irrespective of whether or not the client asked for one.

#if 0
WRAP layout::lay_vector(LONG Offset, LONG PageHeight, bool &VerticalRepass, bool &CheckWrap)
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
   cx = 0;
   cy = 0;
   cr  = cx + m_page_width;
   if ((!Offset) and (PageHeight < Self->Area.Height)) {
      cb = Self->Area.Height; // The reported page height cannot be shorter than the document's viewport area
   }
   else cb = PageHeight;

   if (m_line.height) {
      if (cb < m_cursor_y + m_line.height) cb = m_line.height;
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

   DLAYOUT("[Idx:%d] The %s's available page area is (%gx%g, %gx%g), cursor %gx%g",
      idx, vector->Class->ClassName, cx, cr, cy, cb, m_cursor_x, m_cursor_y);

#if true
   DOUBLE new_y, new_width, new_height, calc_x;
   vector->get(FID_Dimensions, &dimensions);

   left_margin = m_left_margin;
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
      new_x = m_page_width - m_right_margin - new_width;
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

   DLAYOUT("Clip region is being restricted to the bounds: %gx%g,%gx%g", new_x, new_y, new_width, new_height);

   cx = new_x;
   cy = new_y;
   cr = new_x + new_width;
   cb = new_y + new_height;

   // If BlockRight is true, no text may be printed to the right of the vector.

   if (vec.block_right) {
      DLAYOUT("Block Right: Expanding clip.right boundary from %g to %g.",
         cr, m_page_width - m_right_margin);
      cr = m_page_width - m_right_margin; //cell_width;
   }

   // If BlockLeft is true, no text may be printed to the left of the vector (but not
   // including text that has already been printed).

   if (vec.block_left) cx  = 0; // left_margin

   DOUBLE width_check = vec.ignore_cursor ? cr : cr + m_right_margin;

   DLAYOUT("#%d, Pos: %gx%g,%gx%g, Align: $%.8x, WidthCheck: %g/%g",
      vector->UID, new_x, new_y, new_width, new_height, LONG(align), width_check, m_page_width);
   DLAYOUT("Clip Size: %gx%g,%gx%g, LineHeight: %g",
      cx, cy, cell_width, cell_height, line_height);

   dimensions = dimensions;
   error = ERR_Okay;

   acRedimension(vector.obj, new_x, new_y, 0.0, new_width, new_height, 0.0);

#else
   left_margin = m_left_margin;
   line_height = m_line.height ? (m_line.height - m_line.gutter) : m_font->Ascent;

   cell_width  = cr - cx;
   cell_height = cb - cy;
   align = m_font->Align;

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
      LONG new_x = (width - right_margin) - (vector->GraphicWidth + vec.margins.right);
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

            LONG new_x = (width - m_right_margin) - (vector->BoundWidth + vec.margins.right);
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
         DLAYOUT("BlockRight: Expanding clip.right boundary from %d to %d.", cr, width - l.right_margin);
         LONG new_right = width - right_margin; //cell_width;
         if (new_right > cr) cr = new_right;
      }

      // If BlockLeft is set, no text may be printed to the left of the vector (but not
      // including text that has already been printed).  This has no impact on the vector's
      // bounds.

      if (vec.BlockLeft) cx = IgnoreCursor ? 0 : m_left_margin;
      width_check = IgnoreCursor ? cr : cr + m_right_margin;

      DLAYOUT("#%d, Pos: %dx%d,%dx%d, Align: $%.8x, WidthCheck: %d/%d", vector->UID, vector->BoundX, vector->BoundY, vector->BoundWidth, vector->BoundHeight, align, vector->x), width_check, width);
      DLAYOUT("Clip Size: %dx%d,%dx%d, LineHeight: %d, GfxSize: %dx%d, LayoutFlags: $%.8x", cx, cy, cell_width, cell_height, line_height, vector->GraphicWidth, vector->GraphicHeight, layoutflags);

      dimensions = dimensions;
      error = ERR_Okay;
   }
#endif

   if (width_check) {
      // The cursor must advance past the clipping region so that the segment positions will be
      // correct when set.

      CheckWrap = true;

      // Check if the clipping region is invalid.  Invalid clipping regions are not added to the clip
      // region list (i.e. layout of document text will ignore the presence of the vector).

      if ((cb <= cy) or (cr <= cx)) {
         if (auto name = vector->Name) log.warning("%s %s returned an invalid clip region of %gx%g,%gx%g", vector->Class->ClassName, name, cx, cy, cr, cb);
         else log.warning("%s #%d returned an invalid clip region of %gx%g,%gx%g", vector->Class->ClassName, vector->UID, cx, cy, cr, cb);
         return WRAP::DO_NOTHING;
      }

      // If the right-side of the vector extends past the page width, increase the width.

      LONG left_check;

      if (vec.ignore_cursor) left_check = 0;
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

         if (vec.ignore_cursor) cmp_width = (cr - cx);
         else cmp_width = m_left_margin + (cr - cx) + m_right_margin;

         if (m_page_width < cmp_width) {
            DLAYOUT("Restarting as %s clip.left %g < %d and extends past the page width (%g > %g).", vector->Class->ClassName, cx, left_check, width_check, m_page_width);
            m_page_width = cmp_width;
            return WRAP::EXTEND_PAGE;
         }
      }
      else if (width_check > m_page_width) {
         // Perform a wrapping check if the vector possibly extends past the width of the page/cell.

         DLAYOUT("Wrapping %s vector #%d as it extends past the page width (%g > %g).  Pos: %gx%g", vector->Class->ClassName, vector->UID, width_check, m_page_width, cx, cy);

         auto ww = check_wordwrap("Vector", m_page_width, idx, cx, cy, cr - cx, cb - cy);

         if (ww IS WRAP::EXTEND_PAGE) {
            DLAYOUT("Expanding page width due to vector size.");
            return WRAP::EXTEND_PAGE;
         }
         else if (ww IS WRAP::WRAPPED) {
            DLAYOUT("Vector coordinates wrapped to %gx%g", cx, cy);
            // The check_wordwrap() function will have reset m_cursor_x and m_cursor_y, so
            // on our repass, the cell.left and cell.top will reflect this new cursor position.

            goto wrap_vector;
         }
      }

      DLAYOUT("Adding %s clip to the list: (%gx%g, %gx%g)", vector->Class->ClassName, cx, cy, cr-cx, cb-cy);

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

   add_graphics_segment();

   return WRAP::DO_NOTHING;
}
#endif

//********************************************************************************************************************

void layout::lay_set_margins(LONG &BottomMargin)
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
      if (m_cursor_y < escmargins.top) m_cursor_y = escmargins.top;
   }

   if (escmargins.bottom != 0x7fff) {
      BottomMargin += escmargins.bottom;
      if (BottomMargin < 0) BottomMargin = 0;
   }
}

//********************************************************************************************************************
// This function creates segments, that will be used in the final stage of the layout process to draw the graphics.
// They can also assist with user interactivity, e.g. to determine the character that the mouse is positioned over.

void layout::new_segment(const stream_char Start, const stream_char Stop, DOUBLE Y, DOUBLE Width, DOUBLE AlignWidth,
   const std::string &Debug)
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
      DLAYOUT("Cancelling new segment, no content in range %d-%d  \"%.20s\" (%s)",
         Start.index, Stop.index, printable(Self, Start).c_str(), Debug.c_str());
      return;
   }

   bool inline_content   = false;
   bool floating_vectors = false;
   bool allow_merge      = true; // If true, this segment can be merged with prior segment(s) on the line

   for (auto i=Start; i < Stop; i.next_code()) {
      switch (Self->Stream[i.index].code) {
         case SCODE::IMAGE: {
            auto &img =  stream_data<bc_image>(Self, i.index);
            if (img.floating()) floating_vectors = true;
            else inline_content = true;
            allow_merge = false;
            break;
         }

         case SCODE::TABLE_START:
         case SCODE::TABLE_END:
            allow_merge = false;
            break;

         case SCODE::TEXT:
            // Disable merging because a single text code can be referenced by multiple segments (due to word wrapping)
            inline_content = true;
            allow_merge = false;
            break;

         default:
            break;
      }
   }

   auto line_height = m_line.height;
   auto gutter      = m_line.gutter;
   if (inline_content) {
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
   log.branch("#%d %d:%d - %d:%d, Area: %gx%g,%g:%gx%g, WordWidth: %d [%.20s]...[%.20s] (%s)",
      LONG(m_segments.size()), Start.index, LONG(Start.offset), Stop.index, LONG(Stop.offset), m_line.x, Y, Width,
      AlignWidth, line_height, m_word_width, printable(Self, Start).c_str(),
      printable(Self, Stop).c_str(), Debug.c_str());
#endif

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

      if (line_height > segment.area.Height) {
         segment.area.Height = line_height;
         segment.gutter = gutter;
      }

      segment.area.Width  += Width;
      segment.align_width += AlignWidth;
   }
   else {
      segment.start       = Start;
      segment.area        = { m_line.x, Y, Width, line_height };
      segment.align_width = AlignWidth;
      segment.gutter      = gutter;
   }

   segment.stop             = Stop;
   segment.trim_stop        = trim_stop;
   segment.depth            = m_depth;
   segment.inline_content   = inline_content;
   segment.floating_vectors = floating_vectors;
   segment.allow_merge      = allow_merge;
   segment.edit             = m_edit_mode;

   m_segments.emplace_back(segment);
}

// For the addition of non-graphical segments only (no area defined)

void layout::new_code_segment(const stream_char Start, const stream_char Stop, const std::string &Debug)
{
   pf::Log log(__FUNCTION__);

#ifdef DBG_STREAM
   log.branch("#%d %d:%d - %d:%d, [%.20s]...[%.20s] (%s)",
      LONG(m_segments.size()), Start.index, LONG(Start.offset), Stop.index, LONG(Stop.offset),
      printable(Self, Start).c_str(), printable(Self, Stop).c_str(), Debug.c_str());
#endif

   if ((!m_segments.empty()) and (Start < m_segments.back().stop)) {
      // TODO: Verify if this is still necessary?

      // Patching: If the start of the new segment is < the end of the previous segment,
      // adjust the previous segment so that it stops at the beginning of our new segment.
      // This prevents overlapping between segments and the two segments can be patched
      // together in the next section of this routine.

      DLAYOUT("New segment #%d start index is less than (%d < %d) the end of previous segment - will patch up.",
         m_segments.back().start.index, Start.index, m_segments.back().stop.index);
      m_segments.back().stop = Start;
   }

   if ((!m_segments.empty()) and (m_segments.back().stop IS Start) and (m_segments.back().allow_merge)) {
      // We can extend the previous segment.

      auto &current_seg = m_segments.back();
      current_seg.stop      = Stop;
      current_seg.trim_stop = Stop;
   }
   else {
      doc_segment segment;
      segment.start       = Start;
      segment.stop        = Stop;
      segment.trim_stop   = Stop;
      segment.area        = { 0, 0, 0, 0 };
      segment.align_width = 0;
      segment.gutter      = 0;
      segment.inline_content   = false;
      segment.floating_vectors = false;
      segment.depth            = m_depth;
      segment.allow_merge = true;
      segment.edit        = m_edit_mode;

      m_segments.emplace_back(segment);
   }
}

//********************************************************************************************************************
// This function lays out the document so that it is ready to be drawn.  It calculates the position, pixel length and
// height of each line and rearranges any vectors that are present in the document.

static void layout_doc(extDocument *Self)
{
   pf::Log log(__FUNCTION__);

   if (!Self->UpdatingLayout) return;

   if (Self->Stream.empty()) return;

   // Initial height is 1 and not set to the viewport height because we want to accurately report the final height
   // of the page.

   #ifdef DBG_LAYOUT
      log.branch("Area: %gx%g,%gx%g Visible: %d ----------", Self->Area.X, Self->Area.Y, Self->Area.Width, Self->Area.Height, Self->VScrollVisible);
   #endif

   layout l(Self);
   bool repeat = true;
   while (repeat) {
      repeat = false;
      l.m_break_loop--;

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
      l.m_depth = 0;

      if (glFonts.empty()) return;
      auto font = glFonts[0].font;

      DOUBLE page_height = 1;
      l = layout(Self);
      bool vertical_repass = false;
      if (l.do_layout(0, Self->Stream.size(), &font, page_width, page_height,
         ClipRectangle(Self->LeftMargin, Self->TopMargin, Self->RightMargin, Self->BottomMargin),
         vertical_repass) != ERR_Okay) break;

      // If the resulting page width has increased beyond the available area, increase the MinPageWidth value to reduce
      // the number of passes required for the next time we do a layout.

      if ((page_width > Self->Area.Width) and (Self->MinPageWidth < page_width)) Self->MinPageWidth = page_width;

      Self->PageHeight = page_height;
      //if (Self->PageHeight < Self->AreaHeight) Self->PageHeight = Self->AreaHeight;
      Self->CalcWidth = page_width;

      // Recalculation may be required if visibility of the scrollbar needs to change.

      if ((l.m_break_loop > 0) and (!Self->Error)) {
         if (Self->PageHeight > Self->Area.Height) {
            // Page height is bigger than the viewport, so the scrollbar needs to be visible.

            if (!Self->VScrollVisible) {
               DLAYOUT("Vertical scrollbar visibility needs to be enabled, restarting...");
               Self->VScrollVisible = true;
               l.m_break_loop = MAXLOOP;
               repeat = true;
            }
         }
         else { // Page height is smaller than the viewport, so the scrollbar needs to be invisible.
            if (Self->VScrollVisible) {
               DLAYOUT("Vertical scrollbar needs to be invisible, restarting...");
               Self->VScrollVisible = false;
               l.m_break_loop = MAXLOOP;
               repeat = true;
            }
         }
      }
   };

   if (!Self->Error) Self->EditCells = l.m_ecells;
   else Self->EditCells.clear();

   if ((!Self->Error) and (!l.m_segments.empty())) Self->Segments = l.m_segments;
   else Self->Segments.clear();

   Self->UpdatingLayout = false;

   print_segments(Self, Self->Stream);

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

      l.gen_scene_init(Self->Page);
      l.gen_scene_graph(Self->Page, Self->Stream, 0, SEGINDEX(l.m_segments.size()));

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
// Offset/End: Start and end points within the stream for layout processing.
// Width:      Minimum width of the page/section.  Can be increased if insufficient space is available.  Includes the
//             left and right margins in the resulting calculation.
// Height:     Minimum height of the page/section.  Will be increased to match the number of lines in the layout.
// Margins:    Margins within the page area.  These are inclusive to the resulting page width/height.  If in a cell,
//             margins reflect cell padding values.

ERROR layout::do_layout(INDEX Offset, INDEX End, objFont **Font, DOUBLE &Width, DOUBLE &Height,
   ClipRectangle Margins, bool &VerticalRepass)
{
   pf::Log log(__FUNCTION__);

   if ((Self->Stream.empty()) or (Offset >= End) or (!Font) or (!Font[0])) {
      log.traceBranch("No document stream to be processed.");
      return ERR_NothingDone;
   }

   if (m_depth >= MAX_DEPTH) {
      log.traceBranch("Depth limit exceeded (too many tables-within-tables).");
      return ERR_Recursion;
   }

   if (Self->Error) return Self->Error;

   auto page_height = Height;
   m_page_width = Width;

   if (Margins.Left + Margins.Right > m_page_width) {
      m_page_width = Margins.Left + Margins.Right;
   }

   #ifdef DBG_LAYOUT
   log.branch("Dimensions: %gx%g (edge %g), LM %d RM %d TM %d BM %d",
      m_page_width, page_height, m_page_width - Margins.Right,
      Margins.Left, Margins.Right, Margins.Top, Margins.Bottom);
   #endif

   m_depth++;

   layout tablestate(Self), rowstate(Self), liststate(Self);
   bc_table *esctable;
   DOUBLE last_height;
   LONG edit_segment;
   bool check_wrap;

extend_page:
   if (m_page_width > WIDTH_LIMIT) {
      DLAYOUT("Restricting page width from %g to %d", m_page_width, WIDTH_LIMIT);
      m_page_width = WIDTH_LIMIT;
      if (m_break_loop > 4) m_break_loop = 4; // Very large page widths normally means that there's a parsing problem
   }

   if (Self->Error) {
      m_depth--;
      return Self->Error;
   }
   else if (!m_break_loop) {
      Self->Error = ERR_Loop;
      m_depth--;
      return Self->Error;
   }
   m_break_loop--;

   reset();

   last_height  = page_height;
   esctable     = NULL;
   edit_segment = 0;
   check_wrap   = false;  // true if a wordwrap or collision check is required

   m_left_margin  = Margins.Left;
   m_right_margin = Margins.Right;   // Retain the right margin in an adjustable variable, in case we adjust the margin
   m_wrap_edge    = m_page_width - Margins.Right;
   m_align_width  = m_wrap_edge;
   m_cursor_x     = Margins.Left;
   m_cursor_y     = Margins.Top;
   m_line_seg_start = m_segments.size();
   m_font         = *Font;
   m_space_width  = fntCharWidth(m_font, ' ', 0, NULL);

   m_word_index.reset();
   m_line.index.set(Offset);    // The starting index of the line we are operating on
   m_line.full_reset(Margins.Left);

   for (idx = Offset; idx < End; idx++) {
      if ((m_cursor_x >= MAX_PAGE_WIDTH) or (m_cursor_y >= MAX_PAGE_HEIGHT)) {
         log.warning("Invalid cursor position reached @ %gx%g", m_cursor_x, m_cursor_y);
         Self->Error = ERR_InvalidDimension;
         break;
      }

      if (m_line.index.index < idx) {
         // Some byte codes can force a segment definition to be defined now, e.g. because they might otherwise
         // mess up the region size.
         bool set_segment_now = false;

         switch (Self->Stream[idx].code) {
            case SCODE::ADVANCE:
            case SCODE::TABLE_START:
               set_segment_now = true;
               break;

            case SCODE::INDEX_START: {
               auto &index = stream_data<bc_index>(Self, idx);
               if (!index.visible) set_segment_now = true;
               break;
            }

            default: break;
         }

         if (set_segment_now) {
            DLAYOUT("Setting line at code '%s', index %d, line.x: %g, m_word_width: %d",
               BC_NAME(Self->Stream,idx).c_str(), m_line.index.index, m_line.x, m_word_width);
            m_cursor_x += m_word_width;
            new_segment(m_line.index, stream_char(idx, 0), m_cursor_y, m_cursor_x - m_line.x, m_align_width - m_line.x, "WordBreak");
            reset_segment();
            m_align_width = m_wrap_edge;
         }
      }

      // Any escape code for an inline element that forces a word-break will initiate a wrapping check.

      if (esctable) m_align_width = m_wrap_edge;
      else switch (Self->Stream[idx].code) {
         case SCODE::TABLE_END:
         case SCODE::ADVANCE: {
            auto wrap_result = check_wordwrap("EscCode", m_page_width, m_word_index.index, m_cursor_x, m_cursor_y, m_word_width, (m_line.height < 1) ? 1 : m_line.height);
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
            auto wrap_result = lay_text();
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

         case SCODE::ADVANCE: {
            auto adv = &stream_data<bc_advance>(Self, idx);
            m_cursor_x += adv->x;
            m_cursor_y += adv->y;
            if (adv->x) reset_segment();
            break;
         }

         case SCODE::FONT:            lay_font(); break;
         case SCODE::FONT_END:        lay_font_end(); break;
         case SCODE::INDEX_START:     lay_index(); break;
         case SCODE::SET_MARGINS:     lay_set_margins(Margins.Bottom); break;
         case SCODE::LINK:            break;
         case SCODE::LINK_END:        break;
         case SCODE::PARAGRAPH_START: lay_paragraph(); break;
         case SCODE::PARAGRAPH_END:   lay_paragraph_end(); break;

         case SCODE::LIST_START:
            // This is the start of a list.  Each item in the list will be identified by SCODE::PARAGRAPH codes.  The
            // cursor position is advanced by the size of the item graphics element.

            liststate = *this;
            stack_list.push(&stream_data<bc_list>(Self, idx));
            stack_list.top()->repass = false;
            break;

         case SCODE::LIST_END:
            if (lay_list_end()) {
               *this = liststate;
            }
            break;

         case SCODE::IMAGE: {
            auto ww = lay_image();
            if (ww IS WRAP::EXTEND_PAGE) goto extend_page;
            break;
         }

         case SCODE::TABLE_START: {
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
               auto parent_table = esctable;
               esctable = &stream_data<bc_table>(Self, idx);
               esctable->stack = parent_table;
            }
            else {
               esctable = &stream_data<bc_table>(Self, idx);
               esctable->stack = NULL;
            }

            esctable->reset_row_height = true; // All rows start with a height of min_height up until TABLE_END in the first pass
            esctable->compute_columns = 1;
            esctable->width = -1;

            for (unsigned j=0; j < esctable->columns.size(); j++) esctable->columns[j].min_width = 0;

            DOUBLE width;
wrap_table_start:
            // Calculate starting table width, ensuring that the table meets the minimum width according to the cell
            // spacing and padding values.

            if (esctable->width_pct) {
               width = ((Width - m_cursor_x - m_right_margin) * esctable->min_width) / 100;
            }
            else width = esctable->min_width;

            if (width < 0) width = 0;

            {
               DOUBLE min = (esctable->strokeWidth * 2) + (esctable->cell_hspacing * (esctable->columns.size()-1)) + (esctable->cell_padding * 2 * esctable->columns.size());
               if (esctable->collapsed) min -= esctable->cell_hspacing * 2; // Thin tables do not have spacing on the left and right borders
               if (width < min) width = min;
            }

            if (width > WIDTH_LIMIT - m_cursor_x - m_right_margin) {
               log.traceWarning("Table width in excess of allowable limits.");
               width = WIDTH_LIMIT - m_cursor_x - m_right_margin;
               if (m_break_loop > 4) m_break_loop = 4;
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
            esctable->height      = esctable->strokeWidth;

            DLAYOUT("(i%d) Laying out table of %dx%d, coords %gx%g,%gx%g%s, page width %g.",
               idx, LONG(esctable->columns.size()), esctable->rows, esctable->x, esctable->y,
               esctable->width, esctable->min_height, esctable->height_pct ? "%" : "", Width);

            esctable->computeColumns();

            DLAYOUT("Checking for table collisions before layout (%gx%g).  reset_row_height: %d",
               esctable->x, esctable->y, esctable->reset_row_height);

            auto ww = check_wordwrap("Table", m_page_width, idx, esctable->x, esctable->y, esctable->width, esctable->height);
            if (ww IS WRAP::EXTEND_PAGE) {
               DLAYOUT("Expanding page width due to table size.");
               goto extend_page;
            }
            else if (ww IS WRAP::WRAPPED) {
               // The width of the table and positioning information needs to be recalculated in the event of a
               // table wrap.

               DLAYOUT("Restarting table calculation due to page wrap to position %gx%g.", m_cursor_x, m_cursor_y);
               esctable->compute_columns = 1;
               goto wrap_table_start;
            }

            m_cursor_x = esctable->x;
            m_cursor_y = esctable->y + esctable->strokeWidth + esctable->cell_vspacing;
            add_graphics_segment();
            break;
         }

         case SCODE::TABLE_END: {
            auto action = lay_table_end(esctable, Offset, Margins.Top, Margins.Bottom, Height, Width);
            if (action != TE::NIL) {
               auto req_width = m_page_width;
               *this = tablestate;
               m_page_width = req_width;
               if (action IS TE::WRAP_TABLE) goto wrap_table_end;
               else if (action IS TE::REPASS_ROW_HEIGHT) goto repass_row_height;
               else if (action IS TE::EXTEND_PAGE) goto extend_page;
            }
            break;
         }

         case SCODE::ROW:
            stack_row.push(&stream_data<bc_row>(Self, idx));
            rowstate = *this;

            if (esctable->reset_row_height) stack_row.top()->row_height = stack_row.top()->min_height;

repass_row_height:
            stack_row.top()->vertical_repass = false;
            stack_row.top()->y = m_cursor_y;
            esctable->row_width = (esctable->strokeWidth * 2) + esctable->cell_hspacing;

            add_graphics_segment();
            break;

         case SCODE::ROW_END:
            lay_row_end(esctable);
            break;

         case SCODE::CELL: {
            switch(lay_cell(esctable)) {
               case CELL::ABORT:
                  goto exit;

               case CELL::WRAP_TABLE_CELL:
                  *this = tablestate;
                  goto wrap_table_cell;

               case CELL::REPASS_ROW_HEIGHT:
                  *this = rowstate;
                  goto repass_row_height;

               default:
                  break;
            }
            break;
         }

         case SCODE::CELL_END:
            lay_cell_end();
            break;

         default: break;
      }
   }

   // Check if the cursor + any remaining text requires closure

   if ((m_cursor_x + m_word_width > m_left_margin) or (m_word_index.valid())) {
      stream_char sc(idx, 0);
      m_line.apply_word_height();
      end_line(NL::NONE, 0, sc, "LayoutEnd");
   }

exit:

   page_height = calc_page_height(Margins.Bottom);

   if (page_height > MAX_PAGE_HEIGHT) {
      log.warning("Calculated page_height of %g is invalid.", page_height);
      Self->Error = ERR_InvalidDimension;
      return Self->Error;
   }

   // Force a second pass if the page height has increased and there are vectors in the page (the vectors may need
   // to know the page height - e.g. if there is a gradient filling the background).
   //
   // This requirement is also handled in SCODE::CELL, so we only perform it here if processing is occurring within the
   // root page area (Offset of 0).

   if ((!Offset) and (VerticalRepass) and (last_height < page_height)) {
      DLAYOUT("============================================================");
      DLAYOUT("SECOND PASS [%d]: Root page height increased from %g to %g", Offset, last_height, page_height);
      goto extend_page;
   }

   *Font = m_font;

   if (m_page_width > Width) Width = m_page_width;
   if (page_height > Height) Height = page_height;

   m_depth--;

   return Self->Error;
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

#ifdef DBG_LAYOUT
   log.branch("%s: CursorX/Y: %g/%g, ParaY: %d, ParaEnd: %d, Line Height: %g * %g, Span: %d:%d - %d:%d",
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
      new_segment(m_line.index, stream_char(idx, 0), m_cursor_y, m_cursor_x + m_word_width - m_line.x, m_align_width - m_line.x, "Esc:EndLine");
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

   sanitise_line_height();

   m_line.full_reset(m_left_margin);
   m_line.index       = Next;
   m_cursor_x         = m_left_margin;
   m_line_seg_start   = m_segments.size();
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

WRAP layout::check_wordwrap(const std::string &Type, DOUBLE &PageWidth, stream_char Cursor,
   DOUBLE &X, DOUBLE &Y, DOUBLE Width, DOUBLE Height)
{
   pf::Log log(__FUNCTION__);

   if (!m_break_loop) return WRAP::DO_NOTHING;
   if (Width < 1) Width = 1;

   if ((X > MAX_PAGE_WIDTH) or (Y > MAX_PAGE_HEIGHT) or (PageWidth > MAX_PAGE_WIDTH)) {
      log.warning("Invalid element position of %gx%g in page of %g", X, Y, PageWidth);
      Self->Error = ERR_InvalidDimension;
      return WRAP::DO_NOTHING;
   }

#ifdef DBG_WORDWRAP
   log.branch("Index: %d/%d, %s: %dx%d,%dx%d, LineHeight: %d, Cursor: %gx%g, PageWidth: %d, Edge: %d",
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
         auto min_width = X + Width + m_right_margin;
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

      // Set the line segment up to the cursor.  The line.index is updated so that this process only occurs
      // in the first iteration.

      if (m_line.index < Cursor) {
         new_segment(m_line.index, Cursor, Y, X - m_line.x, m_align_width - m_line.x, "DoWrap");
         m_line.index = Cursor;
      }

      // Reset the line management variables so that the next line starts at the left margin.

      sanitise_line_height();

      X  = m_left_margin;
      Y += m_line.height;

      m_cursor_x = X;
      m_cursor_y = Y;
      m_kernchar = 0;
      m_line_seg_start = m_segments.size();

      m_line.reset(m_left_margin);

      result = WRAP::WRAPPED;
      if (--breakloop > 0) goto restart; // Go back and check the clip boundaries again
      else {
         log.traceWarning("Breaking out of continuous loop.");
         Self->Error = ERR_Loop;
      }
   }

   #ifdef DBG_WORDWRAP
      if (result IS WRAP::WRAPPED) log.msg("A wrap to Y coordinate %g has occurred.", m_cursor_y);
   #endif

   return result;
}

//********************************************************************************************************************
// Compare a given area against clip regions and move the x,y position when there's an intersection.

void layout::wrap_through_clips(stream_char WordIndex, DOUBLE &X, DOUBLE &Y, DOUBLE Width, DOUBLE Height)
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

      DWRAP("Word: \"%.20s\" (%gx%g,%gx%g) advances over clip %d-%d",
         printable(Self, WordIndex).c_str(), X, Y, Width, Height, clip.left, clip.right);

      // Set the line segment up to the encountered boundary and continue checking the vector position against the
      // clipping boundaries.

      // Advance the position.  We break if a wordwrap is required - the code outside of this loop will detect
      // the need for a wordwrap and then restart the wordwrapping process.

      if (X IS m_line.x) m_line.x = clip.right;
      X = clip.right; // Go past the clip boundary

      if (X + Width > m_wrap_edge) {
         DWRAP("Wrapping-Break: X(%g)+Width(%g) > Edge(%d) at clip '%s' %dx%d,%dx%d",
            X, Width, m_wrap_edge, clip.name.c_str(), clip.left, clip.top, clip.right, clip.bottom);
         break;
      }

      if (m_line.index < WordIndex) {
         if (!m_line.height) new_segment(m_line.index, WordIndex, Y, X - m_line.x, X - m_line.x, "Wrap:EmptyLine");
         else new_segment(m_line.index, WordIndex, Y, X + Width - m_line.x, m_align_width - m_line.x, "Wrap");
      }

      DWRAP("Line index reset to %d, previously %d", WordIndex.index, m_line.index.index);

      m_line.index = WordIndex;
      m_line.x = X;

      goto restart; // Check all the clips from the beginning
   }
}

//********************************************************************************************************************
// Calculate the page height, which is either going to be the coordinate of the bottom-most line, or one of the
// clipping regions if one of them extends further than the bottom-most line.

DOUBLE layout::calc_page_height(DOUBLE BottomMargin)
{
   pf::Log log(__FUNCTION__);

   if (m_segments.empty()) return 0;

   // Find the last segment that had text or inline content and use that to determine the bottom of the page

   DOUBLE page_height = 0;
   for (SEGINDEX last = m_segments.size() - 1; last >= 0; last--) {
      if (m_segments[last].inline_content) {
         page_height = m_segments[last].area.Height + m_segments[last].area.Y;
         break;
      }
   }

   // Extend the height if a clipping region passes the last line of text.

   for (auto &clip : m_clips) {
      if (clip.transparent) continue;
      if (clip.bottom > page_height) page_height = clip.bottom;
   }

   // Add the bottom margin and subtract the y offset so that we have the true height of the page/cell.

   page_height = page_height + BottomMargin;

   log.trace("Page Height: %g + %g -> %g, Bottom: %g",
      m_segments.back().area.Y, m_segments.back().area.Height, page_height, BottomMargin);

   return page_height;
}
