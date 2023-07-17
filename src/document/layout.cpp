
enum class TE : char {
   NIL = 0,
   WRAP_TABLE,
   REPASS_ROW_HEIGHT, 
   EXTEND_PAGE       
};

// State machine for the layout proces

struct layout {
   std::vector<escList *> stack_list;
   std::vector<escRow *> stack_row;
   extDocument *Self;
   objFont *m_font;
   escLink *m_current_link;
   LONG m_align_flags;
   LONG m_align_width;
   LONG m_cursor_x, m_cursor_y;
   LONG m_kernchar;
   LONG m_left_margin;
   LONG m_paragraph_bottom; // Bottom Y coordinate of the current paragraph; defined on paragraph end.
   LONG m_paragraph_y;
   LONG m_right_margin;
   LONG m_split_start;
   LONG m_start_clips;
   INDEX m_word_index;
   LONG m_word_width;
   LONG m_wrap_edge;
   WORD m_space_width;
   bool m_anchor;
   bool m_no_wrap;
   bool m_set_segment;
   bool m_text_content;
  
   struct {
      LONG full_height;  // The complete height of the line, covers the height of all vectors and tables anchored to the line.  Text is drawn so that the text gutter is aligned to the base line
      LONG height;       // Height of the line with respect to the text
      LONG increase;
      LONG index;
      LONG x;
   } m_line;

   struct {
      LONG x;
      INDEX index;
      ALIGN align;
      bool open;
   } m_link;
   
   // Resets the string management variables, usually done when a string
   // has been broken up on the current line due to a vector or table graphic for example.

   void reset_segment(INDEX Index, LONG X) {
      m_line.index   = Index; 
      m_line.x       = X; 
      m_kernchar     = 0; 
      m_word_index   = -1; 
      m_word_width   = 0; 
      m_text_content = 0;
   }

   layout(extDocument *pSelf) : Self(pSelf) { }

   LONG do_layout(INDEX, INDEX, objFont **, LONG, LONG, LONG *, LONG *, ClipRectangle, bool *);

   void procSetMargins(LONG, LONG, LONG &);
   void procLink(LONG);
   void procLinkEnd(LONG);
   void procIndexStart(LONG &);
   void procFont(LONG);
   WRAP procVector(LONG, LONG, DOUBLE, DOUBLE, LONG &, LONG, bool &, bool &, escParagraph *);
   escParagraph * procParagraphStart(LONG, escParagraph *, LONG);
   escParagraph * procParagraphEnd(LONG, escParagraph *);
   TE procTableEnd(LONG, escTable *, escParagraph *, LONG, LONG, LONG, LONG, LONG *, LONG *);
   void procCellEnd(escCell *);
   void procRowEnd(escTable *);

   void add_drawsegment(LONG, LONG, LONG, LONG, LONG, const std::string &);
   void end_line(LONG NewLine, INDEX Index, DOUBLE Spacing, INDEX RestartIndex, const std::string &);
   WRAP check_wordwrap(const std::string &, INDEX, LONG X, LONG *Width,
      INDEX, LONG &GraphicX, LONG &GraphicY, LONG GraphicWidth, LONG GraphicHeight);
   void check_clips(INDEX Index, INDEX VectorIndex, LONG &GraphicX, LONG &GraphicY, LONG GraphicWidth, LONG GraphicHeight);
};


//********************************************************************************************************************

void layout::procFont(INDEX Index)
{
   pf::Log log;
   auto style = &escape_data<escFont>(Self, Index);
   m_font = lookup_font(style->Index, "ESC::FONT");

   if (m_font) {
      if ((style->Options & FSO::ALIGN_RIGHT) != FSO::NIL) m_font->Align = ALIGN::RIGHT;
      else if ((style->Options & FSO::ALIGN_CENTER) != FSO::NIL) m_font->Align = ALIGN::HORIZONTAL;
      else m_font->Align = ALIGN::NIL;

      m_anchor = ((style->Options & FSO::ANCHOR) != FSO::NIL);
      m_no_wrap = ((style->Options & FSO::NO_WRAP) != FSO::NIL);
      //if (m_no_wrap) m_wrap_edge = 1000;

      DLAYOUT("Font Index: %d, LineSpacing: %d, Height: %d, Ascent: %d, Cursor: %dx%d", style->Index, m_font->LineSpacing, m_font->Height, m_font->Ascent, m_cursor_x, m_cursor_y);
      m_space_width = fntCharWidth(m_font, ' ', 0, 0);

      // Treat the font as if it is a text character by setting the m_word_index.  This ensures it is included in the drawing process

      if (!m_word_width) m_word_index = Index;
   }
   else DLAYOUT("ESC_FONT: Unable to lookup font using style index %d.", style->Index);
}

//********************************************************************************************************************

void layout::procLink(INDEX Index)
{
   if (m_current_link) {
      // Close the currently open link because it's illegal to have a link embedded within a link.

      if (m_font) {
         add_link(Self, ESC::LINK, m_current_link, m_link.x, m_cursor_y, m_cursor_x + m_word_width - m_link.x, m_line.height ? m_line.height : m_font->LineSpacing, "esc_link");
      }
   }

   m_current_link = &escape_data<::escLink>(Self, Index);
   m_link.x     = m_cursor_x + m_word_width;
   m_link.index = Index;
   m_link.open  = true;
   m_link.align = m_font->Align;
}

void layout::procLinkEnd(INDEX Index)
{
   // We don't call add_link() unless the entire word that contains the link has
   // been processed.  This is necessary due to the potential for a word-wrap.

   if (m_current_link) {
      m_link.open = false;

      if (m_word_width < 1) {
         add_link(Self, ESC::LINK, m_current_link, m_link.x, m_cursor_y, m_cursor_x - m_link.x, m_line.height ? m_line.height : m_font->LineSpacing, "esc_link_end");
         m_current_link = NULL;
      }
   }   
}

//********************************************************************************************************************

void layout::procIndexStart(INDEX &Index)
{
   pf::Log log(__FUNCTION__);

   // Indexes don't do anything, but recording the cursor's Y value when they are encountered
   // makes it really easy to scroll to a bookmark when requested (show_bookmark()).

   auto escindex = &escape_data<escIndex>(Self, Index);
   escindex->Y = m_cursor_y;

   if (!escindex->Visible) {
      // If Visible is false, then all content within the index is not to be displayed

      auto end = Index;
      while (end < INDEX(Self->Stream.size())) {
         if (Self->Stream[end] IS CTRL_CODE) {
            if (ESCAPE_CODE(Self->Stream, end) IS ESC::INDEX_END) {
               escIndexEnd &iend = escape_data<escIndexEnd>(Self, end);
               if (iend.ID IS escindex->ID) break;
            }
         }

         NEXT_CHAR(Self->Stream, end);
      }

      if (end >= INDEX(Self->Stream.size())) {
         log.warning("Failed to find matching index-end.  Document stream is corrupt.");
      }

      NEXT_CHAR(Self->Stream, end);

      // Do some cleanup work to complete the content skip.  NB: There is some code associated with this at
      // the top of this routine, with break_segment = 1.

      m_line.index = end;
      Index = end;
   }
}

//********************************************************************************************************************

void layout::procCellEnd(escCell *esccell)
{
   // CELL_END helps draw_document(), so set the segment to ensure that it is
   // included in the draw stream.  Please refer to ESC::CELL to see how content is
   // processed and how the cell dimensions are formed.

   m_set_segment = true;

   if ((esccell) and (!esccell->OnClick.empty())) {
      add_link(Self, ESC::CELL, esccell, esccell->AbsX, esccell->AbsY, esccell->Width, esccell->Height, "esc_cell_end");
   }

   if ((esccell) and (!esccell->EditDef.empty())) {
      // The area of each edit cell is logged for assisting interaction between the mouse pointer and the cells.

      Self->EditCells.emplace_back(esccell->CellID, esccell->AbsX, esccell->AbsY, esccell->Width, esccell->Height);
   }
}

//********************************************************************************************************************

void layout::procRowEnd(escTable *Table)
{
   pf::Log log;

   auto Row = stack_row.back();
   Table->RowIndex++;

   // Increase the table height if the row extends beyond it

   auto j = Row->Y + Row->RowHeight + Table->CellVSpacing;
   if (j > Table->Y + Table->Height) {
      Table->Height = j - Table->Y;
   }

   // Advance the cursor by the height of this row

   m_cursor_y += Row->RowHeight + Table->CellVSpacing;
   m_cursor_x = Table->X;
   DLAYOUT("Row ends, advancing down by %d+%d, new height: %d, y-cursor: %d",
      Row->RowHeight, Table->CellVSpacing, Table->Height, m_cursor_y);

   if (Table->RowWidth > Table->Width) Table->Width = Table->RowWidth;

   stack_row.pop_back();
   m_set_segment = true;
}

//********************************************************************************************************************

escParagraph * layout::procParagraphStart(INDEX Index, escParagraph *Parent, LONG Width)
{
   escParagraph *escpara;

   if (Parent) {
      DOUBLE ratio;
   
      // If a paragraph is embedded within a paragraph, insert a newline before the new paragraph starts.
   
      m_left_margin = Parent->X; // Reset the margin so that the next line will be flush with the parent
   
      if (m_paragraph_y > 0) {
         if (Parent->LeadingRatio > Parent->VSpacing) ratio = Parent->LeadingRatio;
         else ratio = Parent->VSpacing;
      }
      else ratio = Parent->VSpacing;
   
      end_line(NL_PARAGRAPH, Index, ratio, Index, "Esc:PStart");
   
      escpara = &escape_data<escParagraph>(Self, Index);
      escpara->Stack = Parent;
   }
   else {
      escpara = &escape_data<escParagraph>(Self, Index);
      escpara->Stack = NULL;
   
      // Leading ratio is only used if the paragraph is preceeded by content.
      // This check ensures that the first paragraph is always flush against
      // the top of the page.
   
      if ((escpara->LeadingRatio > 0) and (m_paragraph_y > 0)) {
         end_line(NL_PARAGRAPH, Index, escpara->LeadingRatio, Index, "Esc:PStart");
      }
   }
   
   // Indentation support
   
   if (!stack_list.empty()) {
      // For list items, indentation is managed by the list that this paragraph is contained within.
   
      auto list = stack_list.back();
      if (escpara->ListItem) {
         if (Parent) escpara->Indent = list->BlockIndent;
         escpara->ItemIndent = list->ItemIndent;
         escpara->Relative = false;
   
         if (!escpara->Value.empty()) {
            auto strwidth = fntStringWidth(m_font, escpara->Value.c_str(), -1) + 10;
            if (strwidth > list->ItemIndent) {
               list->ItemIndent    = strwidth;
               escpara->ItemIndent = strwidth;
               list->Repass        = true;
            }
         }
      }
      else escpara->Indent = list->ItemIndent;
   }
   
   if (escpara->Indent) {
      if (escpara->Relative) escpara->BlockIndent = escpara->Indent * 100 / Width;
      else escpara->BlockIndent = escpara->Indent;
   }
   
   escpara->X = m_left_margin + escpara->BlockIndent;
   
   m_left_margin += escpara->BlockIndent + escpara->ItemIndent;
   m_cursor_x    += escpara->BlockIndent + escpara->ItemIndent;
   m_line.x      += escpara->BlockIndent + escpara->ItemIndent;
   
   // Paragraph management variables
   
   if (!stack_list.empty()) escpara->VSpacing = stack_list.back()->VSpacing;   
   
   escpara->Y = m_cursor_y;
   escpara->Height = 0;

   return escpara;
}

escParagraph * layout::procParagraphEnd(INDEX Index, escParagraph *Current)
{
   if (Current) {
      // The paragraph height reflects the true size of the paragraph after we take into account
      // any vectors and tables within the paragraph.

      m_paragraph_bottom = Current->Y + Current->Height;

      end_line(NL_PARAGRAPH, Index, Current->VSpacing, Index + ESCAPE_LEN, "Esc:PEnd");

      m_left_margin = Current->X - Current->BlockIndent;
      m_cursor_x    = Current->X - Current->BlockIndent;
      m_line.x      = Current->X - Current->BlockIndent;

      return Current->Stack;
   }
   else {
      end_line(NL_PARAGRAPH, Index, 0, Index + ESCAPE_LEN, "Esc:PEnd-NP");
      return NULL;
   }
}

//********************************************************************************************************************

TE layout::procTableEnd(INDEX Index, escTable *esctable, escParagraph *escpara, LONG Offset, LONG AbsX, LONG TopMargin, LONG BottomMargin, LONG *Height, LONG *Width)
{
   pf::Log log(__FUNCTION__);

   ClipRectangle clip;
   LONG minheight;

   if (esctable->CellsExpanded IS false) {
      LONG unfixed, colwidth;

      // Table cells need to match the available width inside the table.  This routine checks for that - if the cells 
      // are short then the table processing is restarted.

      DLAYOUT("Checking table @ index %d for cell/table widening.  Table width: %d", Index, esctable->Width);

      esctable->CellsExpanded = true;

      if (!esctable->Columns.empty()) {
         colwidth = (esctable->Thickness * 2) + esctable->CellHSpacing;
         for (auto &col : esctable->Columns) {
            colwidth += col.Width + esctable->CellHSpacing;
         }
         if (esctable->Thin) colwidth -= esctable->CellHSpacing * 2; // Thin tables have no spacing allocated on the sides

         if (colwidth < esctable->Width) { // Cell layout is less than the pre-determined table width
            // Calculate the amount of additional space that is available for cells to expand into

            LONG avail_width = esctable->Width - (esctable->Thickness * 2) -
               (esctable->CellHSpacing * (esctable->Columns.size() - 1));

            if (!esctable->Thin) avail_width -= (esctable->CellHSpacing * 2);

            // Count the number of columns that do not have a fixed size

            unfixed = 0;
            for (unsigned j=0; j < esctable->Columns.size(); j++) {
               if (esctable->Columns[j].PresetWidth) avail_width -= esctable->Columns[j].Width;
               else unfixed++;
            }

            // Adjust for expandable columns that we know have exceeded the pre-calculated cell width
            // on previous passes (we want to treat them the same as the PresetWidth columns)  Such cells
            // will often exist that contain large graphics for example.

            if (unfixed > 0) {
               DOUBLE cell_width = avail_width / unfixed;
               for (unsigned j=0; j < esctable->Columns.size(); j++) {
                  if ((esctable->Columns[j].MinWidth) and (esctable->Columns[j].MinWidth > cell_width)) {
                     avail_width -= esctable->Columns[j].MinWidth;
                     unfixed--;
                  }
               }

               if (unfixed > 0) {
                  cell_width = avail_width / unfixed;
                  bool expanded = false;

                  //total = 0;
                  for (unsigned j=0; j < esctable->Columns.size(); j++) {
                     if (esctable->Columns[j].PresetWidth) continue; // Columns with preset-widths are never auto-expanded
                     if (esctable->Columns[j].MinWidth > cell_width) continue;

                     if (esctable->Columns[j].Width < cell_width) {
                        DLAYOUT("Expanding column %d from width %d to %.2f", j, esctable->Columns[j].Width, cell_width);
                        esctable->Columns[j].Width = cell_width;
                        //if (total - (DOUBLE)F2I(total) >= 0.5) esctable->Columns[j].Width++; // Fractional correction

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
   else DLAYOUT("Cells already widened - keeping table width of %d.", esctable->Width);

   // Cater for the minimum height requested

   if (esctable->HeightPercent) {
      // If the table height is expressed as a percentage, it is calculated with
      // respect to the height of the display port.

      if (!Offset) {
         minheight = ((Self->AreaHeight - BottomMargin - esctable->Y) * esctable->MinHeight) / 100;
      }
      else minheight = ((*Height - BottomMargin - TopMargin) * esctable->MinHeight) / 100;

      if (minheight < 0) minheight = 0;
   }
   else minheight = esctable->MinHeight;

   if (minheight > esctable->Height + esctable->CellVSpacing + esctable->Thickness) {
      // The last row in the table needs its height increased
      if (!stack_row.empty()) {
         auto j = minheight - (esctable->Height + esctable->CellVSpacing + esctable->Thickness);
         DLAYOUT("Extending table height to %d (row %d+%d) due to a minimum height of %d at coord %d", minheight, stack_row.back()->RowHeight, j, esctable->MinHeight, esctable->Y);
         stack_row.back()->RowHeight += j;
         return TE::REPASS_ROW_HEIGHT;
      }
      else log.warning("No last row defined for table height extension.");
   }

   // Adjust for cellspacing at the bottom

   esctable->Height += esctable->CellVSpacing + esctable->Thickness;

   // Restart if the width of the table will force an extension of the page.

   LONG j = esctable->X + esctable->Width - AbsX + m_right_margin;
   if ((j > *Width) and (*Width < WIDTH_LIMIT)) {
      DLAYOUT("Table width (%d+%d) increases page width to %d, layout restart forced.", esctable->X, esctable->Width, j);
      *Width = j;                  
      return TE::EXTEND_PAGE;
   }

   // Extend the height of the current line to the height of the table if the table is to be anchored (a
   // technique typically applied to vectors).  We also extend the line height if the table covers the
   // entire width of the page (this is a valuable optimisation for the layout routine).

   if ((m_anchor) or ((esctable->X <= m_left_margin) and (esctable->X + esctable->Width >= m_wrap_edge))) {
      if (esctable->Height > m_line.height) {
         m_line.height = esctable->Height;
         m_line.full_height = m_font->Ascent;
      }
   }

   if (escpara) {
      j = (esctable->Y + esctable->Height) - escpara->Y;
      if (j > escpara->Height) escpara->Height = j;
   }

   // Check if the table collides with clipping boundaries and adjust its position accordingly.
   // Such a check is performed in ESC::TABLE_START - this second check is required only if the width
   // of the table has been extended.
   //
   // Note that the total number of clips is adjusted so that only clips up to the TABLE_START are 
   // considered (otherwise, clips inside the table cells will cause collisions against the parent
   // table).

   DLAYOUT("Checking table collisions (%dx%d).", esctable->X, esctable->Y);

   std::vector<DocClip> saved_clips(Self->Clips.begin() + esctable->TotalClips, Self->Clips.end() + Self->Clips.size());
   Self->Clips.resize(esctable->TotalClips);
   auto ww = check_wordwrap("Table", Index, AbsX, Width, Index, esctable->X, esctable->Y, esctable->Width, esctable->Height);
   Self->Clips.insert(Self->Clips.end(), saved_clips.begin(), saved_clips.end());

   if (ww IS WRAP::EXTEND_PAGE) {
      DLAYOUT("Table wrapped - expanding page width due to table size/position.");                  
      return TE::EXTEND_PAGE;
   }
   else if (ww IS WRAP::WRAPPED) {
      // A repass is necessary as everything in the table will need to be rearranged
      DLAYOUT("Table wrapped - rearrangement necessary.");
      return TE::WRAP_TABLE;
   }

   //DLAYOUT("new table pos: %dx%d", esctable->X, esctable->Y);

   // The table sets a clipping region in order to state its placement (the surrounds of a table are
   // effectively treated as a graphics vector, since it's not text).

   //if (clip.Left IS m_left_margin) clip.Left = 0; // Extending the clipping to the left doesn't hurt
                             
   Self->Clips.emplace_back(
      ClipRectangle(esctable->X, esctable->Y, clip.Left + esctable->Width, clip.Top + esctable->Height),
      Index, false, "Table");   

   m_cursor_x = esctable->X + esctable->Width;
   m_cursor_y = esctable->Y;

   DLAYOUT("Final Table Size: %dx%d,%dx%d", esctable->X, esctable->Y, esctable->Width, esctable->Height);

   esctable = esctable->Stack;

   m_set_segment = true;
   return TE::NIL;
}

//********************************************************************************************************************
// Embedded vectors are always contained by a VectorViewport irrespective of whether or not the client asked for one.

WRAP layout::procVector(INDEX Index, LONG Offset, DOUBLE AbsX, DOUBLE AbsY, LONG &Width, LONG PageHeight, 
   bool &VerticalRepass, bool &CheckWrap, escParagraph *Paragraph)
{
   pf::Log log;
   ClipRectangle cell;
   OBJECTID vector_id;
   
   // Tell the vector our CursorX and CursorY positions so that it can position itself within the stream
   // layout.  The vector will tell us its clipping boundary when it returns (if it has a clipping boundary).
   
   auto &vec = escape_data<::escVector>(Self, Index);
   if (!(vector_id = vec.ObjectID)) return WRAP::DO_NOTHING;
   if (vec.Owned) return WRAP::DO_NOTHING; // Do not manipulate vectors that have owners
   
   // cell: Reflects the page/cell coordinates and width/height of the page/cell.

wrap_vector:
   cell.Left   = AbsX;
   cell.Top    = AbsY;
   cell.Right  = cell.Left + Width;
   if ((!Offset) and (PageHeight < Self->AreaHeight)) {
      cell.Bottom = AbsY + Self->AreaHeight; // The reported page height cannot be shorter than the document's surface area
   }
   else cell.Bottom = AbsY + PageHeight;
   
   if (m_line.height) {
      if (cell.Bottom < m_cursor_y + m_line.height) cell.Bottom = AbsY + m_line.height;
   }
   else if (cell.Bottom < m_cursor_y + 1) cell.Bottom = m_cursor_y + 1;

   LONG dimensions = 0;
   ALIGN align;
   DOUBLE cell_width, cell_height, left_margin, line_height, zone_height;
   OBJECTID layout_surface_id;  
   ERROR error;

   pf::ScopedObjectLock<objVectorViewport> vector(vector_id, 5000);
   if (!vector.granted()) {
      if (vector.error IS ERR_DoesNotExist) vec.ObjectID = 0;   
      return WRAP::DO_NOTHING;
   }

   DLAYOUT("[Idx:%d] The %s's available page area is %d-%d,%d-%d, cursor %dx%d", 
      Index, vector->Class->ClassName, cell.Left, cell.Right, cell.Top, cell.Bottom, m_cursor_x, m_cursor_y);
   
#if true
   DOUBLE new_y, new_width, new_height, calc_x;
   vector->get(FID_Dimensions, &dimensions);
   
   left_margin = m_left_margin - AbsX;
   line_height = (m_line.full_height) ? m_line.full_height : m_font->Ascent;
   
   cell_width  = cell.Right - cell.Left;
   cell_height = cell.Bottom - cell.Top;
   align = m_font->Align;
   
   // Relative dimensions can use the full size of the page/cell only when text-wrapping is disabled.
   
   zone_height = line_height;
   cell.Left  += left_margin;
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
         // Relative X, such as 10% would mean 'NewX must be at least 10% beyond 'cell.left + leftmargin'
         DOUBLE xp;
         vector->getPercentage(FID_X, &xp);
         DOUBLE minx = cell.Left + cell_width * xp;
         if (minx > calc_x) calc_x = minx;
      }
   
      // Calculate width
   
      if (dimensions & DMF_FIXED_X_OFFSET) {
         DOUBLE xo;
         vector->get(FID_XOffset, &xo);
         new_width = cell_width - xo - (calc_x - cell.Left);
      }
      else {
         DOUBLE xop;
         vector->getPercentage(FID_XOffset, &xop);
         new_width = cell_width - (calc_x - cell.Left) - (cell_width * xop);
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
      new_x = (AbsX + Width) - m_right_margin - new_width;
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
      DOUBLE y, yp, yo, yop;
      if (dimensions & DMF_FIXED_Y) vector->get(FID_Y, &new_y);
      else new_y = zone_height * yp;
   
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
   
   const DOUBLE top = vec.IgnoreCursor ? cell.Top : m_cursor_y;
   
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
         new_y = cell.Top + zone_height - height - yo;
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
   
   DLAYOUT("Clip region is being restricted to the bounds: %dx%d,%dx%d", new_x, new_y, new_width, new_height);
   
   cell.Left   = new_x;
   cell.Top    = new_y;
   cell.Right  = new_x + new_width;
   cell.Bottom = new_y + new_height;
   
   // If BlockRight is true, no text may be printed to the right of the vector.
   
   if (vec.BlockRight) {
      DLAYOUT("Block Right: Expanding clip.right boundary from %d to %d.", 
         cell.Right, AbsX + Width - m_right_margin);
      cell.Right = (AbsX + Width) - m_right_margin; //cell_width;
   }
   
   // If BlockLeft is true, no text may be printed to the left of the vector (but not
   // including text that has already been printed).
   
   if (vec.BlockLeft) {
      DLAYOUT("Block Left: Expanding clip.left boundary from %d to %d.", cell.Left, AbsX);
      cell.Left  = AbsX; //left_margin;
   }
   
   DOUBLE width_check = vec.IgnoreCursor ? cell.Right - AbsX : cell.Right + m_right_margin;
   
   DLAYOUT("#%d, Pos: %dx%d,%dx%d, Align: $%.8x, WidthCheck: %d/%d", 
      vector->UID, new_x, new_y, new_width, new_height, LONG(align), width_check, Width);
   DLAYOUT("Clip Size: %dx%d,%dx%d, LineHeight: %d", 
      cell.Left, cell.Top, cell_width, cell_height, line_height);
   
   dimensions = dimensions;
   error = ERR_Okay;
   
   acRedimension(vector.obj, new_x, new_y, 0.0, new_width, new_height, 0.0);  
      
#else
   left_margin = m_left_margin - AbsX;
   line_height = (m_line.full_height) ? m_line.full_height : m_font->Ascent;
   
   cell_width  = cell.Right - cell.Left;
   cell_height = cell.Bottom - cell.Top;
   align = m_font->Align;
   
   if (!vec.Embedded) {
      // In background mode, the bounds are adjusted to match the size of the cell
      // if the vector supports GraphicWidth and GraphicHeight.  For all other vectors,
      // it is assumed that the bounds have been preset.
      //
      // Positioning within the cell bounds is managed by the GraphicX/Y/Width/Height and
      // Align fields.
      //
      // Gaps are automatically worked into the calculated X/Y value.
   
      if ((vector->GraphicWidth) and (vector->GraphicHeight) and (!(layoutflags & LAYOUT_TILE))) {
         vector->BoundX = cell.Left;
         vector->BoundY = cell.Top;
   
         if ((align & ALIGN::HORIZONTAL) != ALIGN::NIL) {
            vector->GraphicX = cell.Left + vec.Margins.Left + ((cell_width - vector->GraphicWidth)>>1);
         }
         else if ((align & ALIGN::RIGHT) != ALIGN::NIL) vector->GraphicX = cell.Left + cell_width - vec.Margins.Right - vector->GraphicWidth;
         else if (!vector->PresetX) {
            if (!vector->PresetWidth) {
               vector->GraphicX = cell.Left + vec.Margins.Left;
            }
            else vector->GraphicX = m_cursor_x + vec.Margins.Left;
         }
   
         if ((align & ALIGN::VERTICAL) != ALIGN::NIL) vector->GraphicY = cell.Top + ((cell_height - vector->TopMargin - vector->BottomMargin - vector->GraphicHeight) * 0.5);
         else if ((align & ALIGN::BOTTOM) != ALIGN::NIL) vector->GraphicY = cell.Top + cell_height - vector->BottomMargin - vector->GraphicHeight;
         else if (!vector->PresetY) {
            if (!vector->PresetHeight) {
               vector->GraphicY = cell.Top + vec.Margins.Top;
            }
            else vector->GraphicY = m_cursor_y + vec.Margins.Top;
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
         // we just want to use the preset X/Y/Width/Height in relation to the available
         // space within the cell.
   
         vector->ParentSurface.Width = cell.Right - cell.Left;
         vector->ParentSurface.Height = cell.Bottom - cell.Top;
   
         vector->get(FID_X, &vector->BoundX);
         vector->get(FID_Y, &vector->BoundY);
         vector->get(FID_Width, &vector->BoundWidth);
         vector->get(FID_Height, &vector->BoundHeight);
   
         vector->BoundX += cell.Left;
         vector->BoundY += cell.Top;
   
   
         DLAYOUT("X/Y: %dx%d, W/H: %dx%d, Parent W/H: %dx%d (Width/Height not preset), Dimensions: $%.8x", 
            vector->BoundX, vector->BoundY, vector->BoundWidth, vector->BoundHeight, vector->ParentSurface.Width, vector->ParentSurface.Height, dimensions);
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
   
         cell.Left += left_margin;
         cell_width = cell_width - right_margin - left_margin; // Remove margins from the cell_width because we're only interested in the space available to the vector
      }
   
      // Adjust the bounds to reflect special dimension settings.  The minimum
      // width and height is 1, and the bounds may not exceed the size of the
      // available cell space (because a width of 110% would cause infinite recursion).
   
      if (vec.IgnoreCursor) vector->BoundX = cell.Left;
      else vector->BoundX = m_cursor_x;
   
      vector->BoundWidth = 1; // Just a default in case no width in the Dimension flags is defined
   
      if ((align & ALIGN::HORIZONTAL) and (vector->GraphicWidth)) {
         // In horizontal mode where a GraphicWidth is preset, we force the BoundX and BoundWidth to their
         // exact settings and override any attempt by the user to have preset the X and Width fields.
         // The vector will attempt a horizontal alignment within the bounds, this will be to no effect as the
         // GraphicWidth is equivalent to the BoundWidth.  Text can still appear to the left and right of the vector,
         // if the author does not like this then the BlockLeft and BlockRight options can be used to extend
         // the clipping region.
   
   
         LONG new_x = vector->BoundX + ((cell_width - (vector->GraphicWidth + vec.Margins.Left + vec.Margins.Right))/2);
         if (new_x > vector->BoundX) vector->BoundX = new_x;
         vector->BoundX += vec.Margins.Left;
         vector->BoundWidth = vector->GraphicWidth;
         extclip_left = vec.Margins.Left;
         extclip_right = vec.Margins.Right;
      }
      else if ((align & ALIGN::RIGHT) and (vector->GraphicWidth)) {
         LONG new_x = ((AbsX + Width) - right_margin) - (vector->GraphicWidth + vec.Margins.Right);
         if (new_x > vector->BoundX) vector->BoundX = new_x;
         vector->BoundWidth = vector->GraphicWidth;
         extclip_left = vec.Margins.Left;
         extclip_right = vec.Margins.Right;
      }
      else {
         LONG xoffset;
   
         if (dimensions & DMF_FIXED_X_OFFSET) xoffset = vector->XOffset;
         else if (dimensions & DMF_RELATIVE_X_OFFSET) xoffset = (DOUBLE)cell_width * (DOUBLE)vector->XOffset;
         else xoffset = 0;
   
         if (dimensions & DMF_RELATIVE_X) {
            LONG new_x = vector->BoundX + vec.Margins.Left + vector->X * cell_width;
            if (new_x > vector->BoundX) vector->BoundX = new_x;
            extclip_left = 0;
            extclip_right = 0;
         }
         else if (dimensions & DMF_FIXED_X) {
            LONG new_x = vector->BoundX + vector->X + vec.Margins.Left;
            if (new_x > vector->BoundX) vector->BoundX = new_x;
            extclip_left = 0;
            extclip_right = 0;
         }
   
         // WIDTH
   
         if (dimensions & DMF_RELATIVE_WIDTH) {
            vector->BoundWidth = (DOUBLE)(cell_width - (vector->BoundX - cell.Left)) * (DOUBLE)vector->Width;
            if (vector->BoundWidth < 1) vector->BoundWidth = 1;
            else if (vector->BoundWidth > cell_width) vector->BoundWidth = cell_width;
         }
         else if (dimensions & DMF_FIXED_WIDTH) vector->BoundWidth = vector->Width;
   
         // GraphicWidth and GraphicHeight settings will expand the width and height
         // bounds automatically unless the Width and Height fields in the Layout have been preset
         // by the user.
         //
         // NOTE: If the vector supports GraphicWidth and GraphicHeight, it must keep
         // them up to date if they are based on relative values.
   
         if ((vector->GraphicWidth > 0) and (!(dimensions & DMF_WIDTH))) {
            DLAYOUT("Setting BoundWidth from %d to preset GraphicWidth of %d", vector->BoundWidth, vector->GraphicWidth);
            vector->BoundWidth = vector->GraphicWidth;
         }
         else if ((dimensions & DMF_X) and (dimensions & DMF_X_OFFSET)) {
            if (dimensions & DMF_FIXED_X_OFFSET) vector->BoundWidth = cell_width - xoffset - (vector->BoundX - cell.Left);
            else vector->BoundWidth = cell_width - (vector->BoundX - cell.Left) - xoffset;
   
            if (vector->BoundWidth < 1) vector->BoundWidth = 1;
            else if (vector->BoundWidth > cell_width) vector->BoundWidth = cell_width;
         }
         else if ((dimensions & DMF_WIDTH) and
                  (dimensions & DMF_X_OFFSET)) {
            if (dimensions & DMF_FIXED_X_OFFSET) {
               LONG new_x = vector->BoundX + cell_width - vector->BoundWidth - xoffset - vec.Margins.Right;
               if (new_x > vector->BoundX) vector->BoundX = new_x;
               extclip_left = vec.Margins.Left;
            }
            else {
               LONG new_x = vector->BoundX + cell_width - vector->BoundWidth - xoffset;
               if (new_x > vector->BoundX) vector->BoundX = new_x;
            }
         }
         else {
            if ((align & ALIGN::HORIZONTAL) and (dimensions & DMF_WIDTH)) {
               LONG new_x = vector->BoundX + ((cell_width - (vector->BoundWidth + vec.Margins.Left + vec.Margins.Right))/2);
               if (new_x > vector->BoundX) vector->BoundX = new_x;
               vector->BoundX += vec.Margins.Left;
               extclip_left = vec.Margins.Left;
               extclip_right = vec.Margins.Right;
            }
            else if ((align & ALIGN::RIGHT) and (dimensions & DMF_WIDTH)) {
               // Note that it is possible the BoundX may end up behind the cursor, or the cell's left margin.
               // A check for this is made later, so don't worry about it here.
   
               LONG new_x = ((AbsX + Width) - m_right_margin) - (vector->BoundWidth + vec.Margins.Right);
               if (new_x > vector->BoundX) vector->BoundX = new_x;
               extclip_left = vec.Margins.Left;
               extclip_right = vec.Margins.Right;
            }
         }
      }
   
      // VERTICAL SUPPORT
   
      LONG obj_y;
   
      if (vec.IgnoreCursor) vector->BoundY = cell.Top;
      else vector->BoundY = m_cursor_y;
   
      obj_y = 0;
      if (dimensions & DMF_RELATIVE_Y)   obj_y = vector->Y * line_height;
      else if (dimensions & DMF_FIXED_Y) obj_y = vector->Y;
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
   
         vector->BoundHeight = (DOUBLE)(zone_height - obj_y) * (DOUBLE)vector->Height;
         if (vector->BoundHeight > zone_height - obj_y) vector->BoundHeight = line_height - obj_y;
      }
      else if (dimensions & DMF_FIXED_HEIGHT) {
         vector->BoundHeight = vector->Height;
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
               if (dimensions & DMF_FIXED_Y_OFFSET) vector->BoundY = cell.Top + zone_height - vector->Height - vector->YOffset;
               else vector->BoundY += (DOUBLE)zone_height - (DOUBLE)vector->Height - ((DOUBLE)zone_height * (DOUBLE)vector->YOffset);
            }
         }
      }
   
      if (layoutflags & (LAYOUT_BACKGROUND|LAYOUT_TILE)) {
         error = ERR_NothingDone; // No text wrapping for background and tile layouts
      }
      else {
         // Set the clipping
   
         DLAYOUT("Clip region is being restricted to the bounds: %dx%d,%dx%d", vector->BoundX, vector->BoundY, vector->BoundWidth, vector->BoundHeight);
   
         cell.Left  = vector->BoundX - extclip_left;
         cell.Top   = vector->BoundY - vector->TopMargin;
         cell.Right = vector->BoundX + vector->BoundWidth + extclip_right;
         cell.Bottom = vector->BoundY + vector->BoundHeight + vector->BottomMargin;
   
         // If BlockRight is set, no text may be printed to the right of the vector.  This has no impact
         // on the vector's bounds.
   
         if (vec.BlockRight) {
            DLAYOUT("BlockRight: Expanding clip.right boundary from %d to %d.", cell.Right, AbsX + Width - l.right_margin);
            LONG new_right = (AbsX + Width) - right_margin; //cell_width;
            if (new_right > cell.Right) cell.Right = new_right;
         }
   
         // If BlockLeft is set, no text may be printed to the left of the vector (but not
         // including text that has already been printed).  This has no impact on the vector's
         // bounds.
   
         if (vec.BlockLeft) {
            DLAYOUT("BlockLeft: Expanding clip.left boundary from %d to %d.", cell.Left, AbsX);
   
            if (IgnoreCursor) cell.Left = AbsX;
            else cell.Left  = m_left_margin;
         }
   
         if (IgnoreCursor) width_check = cell.Right - AbsX;
         else width_check = cell.Right + m_right_margin;
   
         DLAYOUT("#%d, Pos: %dx%d,%dx%d, Align: $%.8x, WidthCheck: %d/%d", vector->UID, vector->BoundX, vector->BoundY, vector->BoundWidth, vector->BoundHeight, align, vector->X), width_check, Width);
         DLAYOUT("Clip Size: %dx%d,%dx%d, LineHeight: %d, GfxSize: %dx%d, LayoutFlags: $%.8x", cell.Left, cell.Top, cell_width, cell_height, line_height, vector->GraphicWidth, vector->GraphicHeight, layoutflags);
   
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
   
      if ((cell.Bottom <= cell.Top) or (cell.Right <= cell.Left)) {
         if (auto name = vector->Name) log.warning("%s %s returned an invalid clip region of %dx%d,%dx%d", vector->Class->ClassName, name, cell.Left, cell.Top, cell.Right, cell.Bottom);
         else log.warning("%s #%d returned an invalid clip region of %dx%d,%dx%d", vector->Class->ClassName, vector->UID, cell.Left, cell.Top, cell.Right, cell.Bottom);
         return WRAP::DO_NOTHING;
      }
   
      // If the right-side of the vector extends past the page width, increase the width.
   
      LONG left_check;
   
      if (vec.IgnoreCursor) left_check = AbsX;
      else if (vec.BlockLeft) left_check = m_left_margin;
      else left_check = m_left_margin; //m_cursor_x;
   
      if (Width >= WIDTH_LIMIT);
      else if ((cell.Left < left_check) or (vec.IgnoreCursor)) {
         // The vector is < left-hand side of the page/cell, this means that we may have to force a page/cell width 
         // increase.
         //
         // Note: Vectors with IgnoreCursor are always checked here, because they aren't subject
         // to wrapping due to the X/Y being fixed.  Such vectors are limited to width increases only.
   
         LONG cmp_width;
   
         if (vec.IgnoreCursor) cmp_width = AbsX + (cell.Right - cell.Left);
         else cmp_width = m_left_margin + (cell.Right - cell.Left) + m_right_margin;
   
         if (Width < cmp_width) {
            DLAYOUT("Restarting as %s clip.left %d < %d and extends past the page width (%d > %d).", vector->Class->ClassName, cell.Left, left_check, width_check, Width);
            Width = cmp_width;
            return WRAP::EXTEND_PAGE;
         }
      }
      else if (width_check > Width) {
         // Perform a wrapping check if the vector possibly extends past the width of the page/cell.
   
         DLAYOUT("Wrapping %s vector #%d as it extends past the page width (%d > %d).  Pos: %dx%d", vector->Class->ClassName, vector->UID, width_check, Width, cell.Left, cell.Top);
   
         auto ww = check_wordwrap("Vector", Index, AbsX, &Width, Index, cell.Left, cell.Top, cell.Right - cell.Left, cell.Bottom - cell.Top);
   
         if (ww IS WRAP::EXTEND_PAGE) {
            DLAYOUT("Expanding page width due to vector size.");
            return WRAP::EXTEND_PAGE;
         }
         else if (ww IS WRAP::WRAPPED) {
            DLAYOUT("Vector coordinates wrapped to %dx%d", cell.Left, cell.Top);
            // The check_wordwrap() function will have reset m_cursor_x and m_cursor_y, so
            // on our repass, the cell.left and cell.top will reflect this new cursor position.
   
            goto wrap_vector;
         }
      }
   
      DLAYOUT("Adding %s clip to the list: %dx%d,%dx%d", vector->Class->ClassName, cell.Left, cell.Top, cell.Right-cell.Left, cell.Bottom-cell.Top);
   
      Self->Clips.emplace_back(cell, Index, !vec.Embedded, "Vector");
   
      if (vec.Embedded) {
         if (cell.Bottom > m_cursor_y) {
            auto objheight = cell.Bottom - m_cursor_y;
            if ((m_anchor) or (vec.Embedded)) {
               // If all vectors in the current section need to be anchored to the text, each
               // vector becomes part of the current line (e.g. treat the vector as if it were
               // a text character).  This requires us to adjust the line height.
   
               if (objheight > m_line.height) {
                  m_line.height = objheight;
                  m_line.full_height = m_font->Ascent;
               }
            }
            else {
               // If anchoring is not set, the height of the vector will still define the height
               // of the line, but cannot exceed the height of the font for that line.
   
               if (objheight < m_font->LineSpacing) {
                  m_line.height = objheight;
                  m_line.full_height = objheight;
               }
            }
         }
   
         //if (cell.Right > m_cursor_x) m_word_width += cell.Right - m_cursor_x;
   
         if (Paragraph) {
            auto j = cell.Bottom - Paragraph->Y;
            if (j > Paragraph->Height) Paragraph->Height = j;
         }
      }
   }
   
   m_set_segment = true;
   
   // If the vector uses a relative height or vertical offset, a repass will be required if the page height
   // increases.
   
   if ((dimensions & (DMF_RELATIVE_HEIGHT|DMF_FIXED_Y_OFFSET|DMF_RELATIVE_Y_OFFSET)) and 
       ((!vec.Embedded) or (vec.IgnoreCursor))) {
      DLAYOUT("Vertical repass may be required.");
      VerticalRepass = true;
   }

   return WRAP::DO_NOTHING;
}

//********************************************************************************************************************

void layout::procSetMargins(INDEX Index, LONG AbsY, LONG &BottomMargin)
{
   auto &escmargins = escape_data<::escSetMargins>(Self, Index);

   if (escmargins.Left != 0x7fff) {
      m_cursor_x     += escmargins.Left;
      m_line.x      += escmargins.Left;
      m_left_margin += escmargins.Left;
   }

   if (escmargins.Right != 0x7fff) {
      m_right_margin += escmargins.Right;
      m_align_width -= escmargins.Right;
      m_wrap_edge      -= escmargins.Right;
   }

   if (escmargins.Top != 0x7fff) {
      if (m_cursor_y < AbsY + escmargins.Top) m_cursor_y = AbsY + escmargins.Top;
   }

   if (escmargins.Bottom != 0x7fff) {
      BottomMargin += escmargins.Bottom;
      if (BottomMargin < 0) BottomMargin = 0;
   }
}

//********************************************************************************************************************
// This function lays out the document so that it is ready to be drawn.  It calculates the position, pixel length and
// height of each line and rearranges any vectors that are present in the document.

static void layout_doc(extDocument *Self)
{
   pf::Log log(__FUNCTION__);
   objFont *m_font;
   LONG page_width, hscroll_offset;
   bool vertical_repass;

   if (Self->UpdateLayout IS false) return;

   // Remove any resources from the previous layout process.

   for (auto &obj : Self->LayoutResources) FreeResource(obj);
   Self->LayoutResources.clear();

   if (Self->Stream.empty()) return;

   // Initial height is 1 and not set to the surface height because we want to accurately report the final height 
   // of the page.

   LONG page_height = 1;

   DLAYOUT("Area: %dx%d,%dx%d Visible: %d ----------", Self->AreaX, Self->AreaY, Self->AreaWidth, Self->AreaHeight, Self->VScrollVisible);

   Self->BreakLoop = MAXLOOP;

restart:
   Self->BreakLoop--;

   hscroll_offset = 0;

   if (Self->PageWidth <= 0) {
      // If no preferred page width is set, maximise the page width to the available viewing area
      page_width = Self->AreaWidth - hscroll_offset;
   }
   else {
      if (!Self->RelPageWidth) { // Page width is fixed
         page_width = Self->PageWidth;
      }
      else { // Page width is relative
         page_width = (Self->PageWidth * (Self->AreaWidth - hscroll_offset)) / 100;
      }
   }

   if (page_width < Self->MinPageWidth) page_width = Self->MinPageWidth;

   Self->Segments.clear();
   Self->SortSegments.clear();
   Self->Clips.clear();
   Self->Links.clear();
   Self->EditCells.clear();

   Self->PageProcessed = false;
   Self->Error = ERR_Okay;
   Self->Depth = 0;

   if (!(m_font = lookup_font(0, "layout_doc"))) { // There is no content loaded for display      
      return;
   }

   layout l(Self);
   l.do_layout(0, Self->Stream.size(), &m_font, 0, 0, &page_width, &page_height, 
      ClipRectangle(Self->LeftMargin, Self->TopMargin, Self->RightMargin, Self->BottomMargin), 
      &vertical_repass);

   DLAYOUT("Section layout complete.");

   // If the resulting page width has increased beyond the available area, increase the MinPageWidth value to reduce
   // the number of passes required for the next time we do a layout.

   if ((page_width > Self->AreaWidth) and (Self->MinPageWidth < page_width)) Self->MinPageWidth = page_width;

   Self->PageHeight = page_height;
//   if (Self->PageHeight < Self->AreaHeight) Self->PageHeight = Self->AreaHeight;
   Self->CalcWidth = page_width;

   // Recalculation may be required if visibility of the scrollbar needs to change.

   if ((Self->BreakLoop > 0) and (!Self->Error)) {
      if (Self->PageHeight > Self->AreaHeight) {
         // Page height is bigger than the surface, so the scrollbar needs to be visible.

         if (!Self->VScrollVisible) {
            DLAYOUT("Vertical scrollbar visibility needs to be enabled, restarting...");
            Self->VScrollVisible = true;
            Self->BreakLoop = MAXLOOP;
            goto restart;
         }
      }
      else {
         // Page height is smaller than the surface, so the scrollbar needs to be invisible.

         if (Self->VScrollVisible) {
            DLAYOUT("Vertical scrollbar needs to be invisible, restarting...");
            Self->VScrollVisible = false;
            Self->BreakLoop = MAXLOOP;
            goto restart;
         }
      }
   }

   // Look for clickable links that need to be aligned and adjust them (links cannot be aligned until the entire
   // width of their line is known, hence it's easier to make a final adjustment for all links post-layout).

   if (!Self->Error) {
      for (auto &link : Self->Links) {
         if (link.EscapeCode != ESC::LINK) continue;

         auto esclink = link.Link;
         if ((esclink->Align & (FSO::ALIGN_RIGHT|FSO::ALIGN_CENTER)) != FSO::NIL) {
            auto &segment = Self->Segments[link.Segment];
            if ((esclink->Align & FSO::ALIGN_RIGHT) != FSO::NIL) {
               link.X = segment.X + segment.AlignWidth - link.Width;
            }
            else if ((esclink->Align & FSO::ALIGN_CENTER) != FSO::NIL) {
               link.X = link.X + ((segment.AlignWidth - link.Width) / 2);
            }
         }
      }
   }

   // Build the sorted segment array

   if ((!Self->Error) and (!Self->Segments.empty())) {
      Self->SortSegments.resize(Self->Segments.size());
      unsigned seg, i, j;

      for (i=0, seg=0; seg < Self->Segments.size(); seg++) {
         if ((Self->Segments[seg].Height > 0) and (Self->Segments[seg].Width > 0)) {
            Self->SortSegments[i].Segment = seg;
            Self->SortSegments[i].Y       = Self->Segments[seg].Y;
            i++;
         }
      }

      // Shell sort

      unsigned h = 1;
      while (h < Self->SortSegments.size() / 9) h = 3 * h + 1;

      for (; h > 0; h /= 3) {
         for (auto i=h; i < Self->SortSegments.size(); i++) {
            SortSegment temp = Self->SortSegments[i];
            for (j=i; (j >= h) and (sortseg_compare(Self, Self->SortSegments[j - h], temp) < 0); j -= h) {
               Self->SortSegments[j] = Self->SortSegments[j - h];
            }
            Self->SortSegments[j] = temp;
         }
      }
   }

   Self->UpdateLayout = false;

#ifdef DBG_LINES
   print_lines(Self);
   //print_sorted_lines(Self);
   print_tabfocus(Self);
#endif

   // If an error occurred during layout processing, unload the document and display an error dialog.  (NB: While it is
   // possible to display a document up to the point at which the error occurred, we want to maintain a strict approach
   // so that human error is considered excusable in document formatting).

   if (Self->Error) {
      unload_doc(Self, ULD_REDRAW);

      std::string msg = "A failure occurred during the layout of this document - it cannot be displayed.\n\nDetails: ";
      if (Self->Error IS ERR_Loop) msg.append("This page cannot be rendered correctly due to its design.");
      else msg.append(GetErrorMsg(Self->Error));

      error_dialog("Document Layout Error", msg);
   }
   else {
      for (auto &trigger : Self->Triggers[DRT_AFTER_LAYOUT]) {
         if (trigger.Type IS CALL_SCRIPT) {
            const ScriptArg args[] = {
               { "ViewWidth",  Self->AreaWidth },
               { "ViewHeight", Self->AreaHeight },
               { "PageWidth",  Self->CalcWidth },
               { "PageHeight", Self->PageHeight }
            };
            scCallback(trigger.Script.Script, trigger.Script.ProcedureID, args, ARRAYSIZE(args), NULL);
         }
         else if (trigger.Type IS CALL_STDC) {
            auto routine = (void (*)(APTR, extDocument *, LONG, LONG, LONG, LONG))trigger.StdC.Routine;
            pf::SwitchContext context(trigger.StdC.Context);
            routine(trigger.StdC.Context, Self, Self->AreaWidth, Self->AreaHeight, Self->CalcWidth, Self->PageHeight);            
         }
      }
   }   
}

//********************************************************************************************************************
// This function creates segments, which are used during the drawing process as well as user interactivity, e.g. to
// determine the character that the mouse is positioned over.  A segment will usually consist of a sequence of
// text characters or escape sequences.
//
// Offset: The start of the line within the stream.
// Stop:   The stream index at which the line stops.

void layout::add_drawsegment(INDEX Offset, INDEX Stop, LONG Y, LONG Width, LONG AlignWidth, const std::string &Debug)
{
   pf::Log log(__FUNCTION__);

   // Determine trailing whitespace at the end of the line.  This helps to prevent situations such as underlining 
   // occurring in whitespace at the end of the line during word-wrapping.

   auto trim_stop = Stop;
   while ((Self->Stream[trim_stop-1] <= 0x20) and (trim_stop > Offset)) {
      if (Self->Stream[trim_stop-1] IS CTRL_CODE) break;
      trim_stop--;
   }

   if (Offset >= Stop) {
      DLAYOUT("Cancelling addition, no content in line to add (bytes %d-%d) \"%.20s\" (%s)", Offset, Stop, printable(Self, Offset).c_str(), Debug.c_str());
      return;
   }

   // Check the new segment to see if there are any text characters or escape codes relevant to drawing

   bool text_content    = false;
   bool control_content = false;
   bool vector_content  = false;
   bool allow_merge     = true;

   INDEX i;
   for (i=Offset; i < Stop;) {
      if (Self->Stream[i] IS CTRL_CODE) {
         auto code = ESCAPE_CODE(Self->Stream, i);
         control_content = true;
         if (code IS ESC::VECTOR) vector_content = true;
         if ((code IS ESC::VECTOR) or (code IS ESC::TABLE_START) or (code IS ESC::TABLE_END) or (code IS ESC::FONT)) {
            allow_merge = false;
         }
      }
      else {
         text_content = true;
         allow_merge = false;
      }

      NEXT_CHAR(Self->Stream, i);
   }

   auto Height   = m_line.height;
   auto BaseLine = m_line.full_height;
   if (text_content) {
      if (Height <= 0) {
         // No line-height given and there is text content - use the most recent font to determine the line height
         Height   = m_font->LineSpacing;
         BaseLine = m_font->Ascent;
      }
      else if (!BaseLine) { // If base-line is missing for some reason, define it
         BaseLine = m_font->Ascent;
      }
   }
   else {
      if (Height <= 0) Height = 0;
      if (BaseLine <= 0) BaseLine = 0;
   }

#ifdef DBG_STREAM
   DLAYOUT("#%d, Bytes: %d-%d, Area: %dx%d,%d:%dx%d, WordWidth: %d, CursorY: %d, [%.20s]...[%.20s] (%s)",
      LONG(Self->Segments.size()), Offset, Stop, m_line.x, Y, Width, AlignWidth, Height, m_word_width,
      m_cursor_y, printable(Self, Offset, Stop-Offset).c_str(), printable(Self, Stop).c_str(), Debug.c_str());
#endif

   DocSegment segment;
   auto x = m_line.x;

   if ((!Self->Segments.empty()) and (Offset < Self->Segments.back().Stop)) {
      // Patching: If the start of the new segment is < the end of the previous segment,
      // adjust the previous segment so that it stops at the beginning of our new segment.
      // This prevents overlapping between segments and the two segments will be patched
      // together in the next section of this routine.

      if (Offset <= Self->Segments.back().Index) {
         // If the start of the new segment retraces to an index that has already been configured,
         // then we have actually encountered a coding flaw and the caller should be investigated.

         log.warning("(%s) New segment #%d retraces to index %d, which has been configured by previous segments.", Debug.c_str(), Self->Segments.back().Index, Offset);
         return;
      }
      else {
         DLAYOUT("New segment #%d start index is less than (%d < %d) the end of previous segment - will patch up.", Self->Segments.back().Index, Offset, Self->Segments.back().Stop);
         Self->Segments.back().Stop = Offset;
      }
   }

   // Is the new segment a continuation of the previous one, and does the previous segment contain content?
   if ((allow_merge) and (!Self->Segments.empty()) and (Self->Segments.back().Stop IS Offset) and
       (Self->Segments.back().AllowMerge)) {
      // We are going to extend the previous line rather than add a new one, as the two
      // segments only contain control codes.

      segment = Self->Segments.back();
      Self->Segments.pop_back();

      Offset = segment.Index;
      x      = segment.X;
      Width += segment.Width;
      AlignWidth += segment.AlignWidth;
      if (segment.Height > Height) {
         Height   = segment.Height;
         BaseLine = segment.BaseLine;
      }
   }

#ifdef _DEBUG
   // If this is a segmented line, check if any previous entries have greater
   // heights.  If so, this is considered an internal programming error.

   if ((m_split_start != NOTSPLIT) and (Height > 0)) {
      for (i=m_split_start; i < Offset; i++) {
         if (Self->Segments[i].Depth != Self->Depth) continue;
         if (Self->Segments[i].Height > Height) {
            log.warning("A previous entry in segment %d has a height larger than the new one (%d > %d)", i, Self->Segments[i].Height, Height);
            BaseLine = Self->Segments[i].BaseLine;
            Height = Self->Segments[i].Height;
         }
      }
   }
#endif

   segment.Index          = Offset;
   segment.Stop           = Stop;
   segment.TrimStop       = trim_stop;
   segment.X              = x;
   segment.Y              = Y;
   segment.Height         = Height;
   segment.BaseLine       = BaseLine;
   segment.Width          = Width;
   segment.Depth          = Self->Depth;
   segment.AlignWidth     = AlignWidth;
   segment.TextContent    = text_content;
   segment.ControlContent = control_content;
   segment.VectorContent  = vector_content;
   segment.AllowMerge     = allow_merge;
   segment.Edit           = Self->EditMode;

   // If a line is segmented, we need to backtrack for earlier line segments and ensure that their height and full_height
   // is matched to that of the last line (which always contains the maximum height and full_height values).

   if ((m_split_start != NOTSPLIT) and (Height)) {
      if (LONG(Self->Segments.size()) != m_split_start) {
         DLAYOUT("Resetting height (%d) & base (%d) of segments index %d-%d.", Height, BaseLine, segment.Index, m_split_start);
         for (unsigned i=m_split_start; i < Self->Segments.size(); i++) {
            if (Self->Segments[i].Depth != Self->Depth) continue;
            Self->Segments[i].Height = Height;
            Self->Segments[i].BaseLine = BaseLine;
         }
      }
   }

   Self->Segments.emplace_back(segment);
}

//********************************************************************************************************************
// Calculate the position, pixel length and height of each line for the entire page.  This function does not recurse,
// but does iterate if the size of the page section is expanded.  It is also called for individual table cells
// which are treated as miniature pages.
//
// Offset:   The byte offset within the document stream to start layout processing.
// X/Y:      Section coordinates, starts at 0,0 for the main page, subsequent sections (table cells) can be at any location, measured as absolute to the top left corner of the page.
// Width:    Minimum width of the page/section.  Can be increased if insufficient space is available.  Includes the left and right margins in the resulting calculation.
// Height:   Minimum height of the page/section.  Will be increased to match the number of lines in the layout.
// Margins:  Margins within the page area.  These are inclusive to the resulting page width/height.  If in a cell, margins reflect cell padding values.
     
struct LAYOUT_STATE {
   // Records the current layout, index and state information.
   layout Layout;
   INDEX Index     = 0;
   LONG TotalClips = 0;
   LONG TotalLinks = 0;
   LONG SegCount   = 0;
   LONG ECIndex    = 0;

   LAYOUT_STATE(extDocument *pSelf) : Layout(pSelf) { }

   LAYOUT_STATE(extDocument *pSelf, LONG pIndex, layout &pLayout) : Layout(pLayout) {
      Index      = pIndex; 
      TotalClips = pSelf->Clips.size(); 
      TotalLinks = pSelf->Links.size(); 
      ECIndex    = pSelf->EditCells.size(); 
      SegCount   = pSelf->Segments.size();
   }

   void restore(extDocument *pSelf) {
      pf::Log log(__FUNCTION__);
      DLAYOUT("Restoring earlier layout state to index %d", Index);
      pSelf->Clips.resize(TotalClips);
      pSelf->Links.resize(TotalLinks);
      pSelf->Segments.resize(SegCount);
      pSelf->EditCells.resize(ECIndex);
   }
};

INDEX layout::do_layout(INDEX Offset, INDEX End, objFont **Font, LONG AbsX, LONG AbsY, 
   LONG *Width, LONG *Height, ClipRectangle Margins, bool *VerticalRepass)
{
   pf::Log log(__FUNCTION__);

   escCell    *esccell;
   escTable   *esctable;
   escParagraph *escpara;

   LAYOUT_STATE tablestate(Self), rowstate(Self), liststate(Self);
   LONG start_ecindex, unicode, j, lastheight, lastwidth, edit_segment;
   INDEX i;
   bool checkwrap;

   if ((Self->Stream.empty()) or (Offset >= End) or (!Font) or (!Font[0])) {
      log.trace("No document stream to be processed.");
      return 0;
   }

   if (Self->Depth >= MAX_DEPTH) {
      log.trace("Depth limit exceeded (too many tables-within-tables).");
      return 0;
   }
   
   // You must execute a goto to the point at which SAVE_STATE() was used after calling this macro
   
   auto RESTORE_STATE = [&](LAYOUT_STATE &s) {
      s.restore(Self); 
      *this = s.Layout; 
      return s.Index;
   };

   auto start_links    = Self->Links.size();
   auto start_segments = Self->Segments.size();
   m_start_clips       = Self->Clips.size();
   start_ecindex       = Self->EditCells.size();
   LONG page_height    = *Height;
   bool vector_vertical_repass = false;

   *VerticalRepass = false;

   #ifdef DBG_LAYOUT
   log.branch("Dimensions: %dx%d,%dx%d (edge %d), LM %d RM %d TM %d BM %d",
      AbsX, AbsY, *Width, *Height, AbsX + *Width - Margins.Right,
      Margins.Left, Margins.Right, Margins.Top, Margins.Bottom);
   #endif

   Self->Depth++;

extend_page:
   if (*Width > WIDTH_LIMIT) {
      DLAYOUT("Restricting page width from %d to %d", *Width, WIDTH_LIMIT);
      *Width = WIDTH_LIMIT;
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

   Self->Links.resize(start_links);     // Also refer to SAVE_STATE() and restore_state()
   Self->Segments.resize(start_segments);
   Self->Clips.resize(m_start_clips);

   lastwidth       = *Width;
   lastheight      = page_height;
   esctable        = NULL;
   escpara         = NULL;
   esccell         = NULL;
   edit_segment    = 0;
   checkwrap       = false;  // true if a wordwrap or collision check is required

   m_anchor           = false;  // true if in an anchored section (vectors are anchored to the line)
   m_align_flags      = 0;      // Current alignment settings according to the font style
   m_paragraph_y      = 0;
   m_paragraph_bottom = 0;
   m_line.increase    = 0;
   m_left_margin      = AbsX + Margins.Left;
   m_right_margin     = Margins.Right;   // Retain the right margin in an adjustable variable, in case we adjust the margin
   m_wrap_edge        = AbsX + *Width - Margins.Right;
   m_align_width      = m_wrap_edge;
   m_cursor_x         = AbsX + Margins.Left;  // The absolute position of the cursor
   m_cursor_y         = AbsY + Margins.Top;
   m_word_width       = 0;         // The pixel width of the current word.  Zero if no word is being worked on
   m_word_index       = -1;        // A byte index in the stream, for the word currently being operated on
   m_line.index       = Offset;    // The starting index of the line we are operating on
   m_line.x           = AbsX + Margins.Left;
   m_line.height      = 0;
   m_line.full_height = 0;
   m_kernchar         = 0;      // Previous character of the word being operated on
   m_link.x           = 0;
   m_link.index       = 0;
   m_split_start      = Self->Segments.size();  // Set to the previous line index if line is segmented.  Used for ensuring that all distinct entries on the line use the same line height
   m_font             = *Font;
   m_no_wrap          = false; // true if word wrapping is to be turned off
   m_link.open        = false;
   m_set_segment      = false;
   m_text_content     = false;
   m_space_width      = fntCharWidth(m_font, ' ', 0, NULL);

   i = Offset;

   while (i < End) {
      // For certain graphics-related escape codes, set the line segment up to the encountered escape code if the text
      // string will be affected (e.g. if the string will be broken up due to a clipping region etc).

      if (Self->Stream[i] IS CTRL_CODE) {
         // Any escape code that sets m_set_segment to true in its case routine, must set break_segment to true now so
         // that any textual content can be handled immediately.
         //
         // This is done particular for escape codes that can be treated as wordwrap breaks.

         if (m_line.index < i) {
            BYTE break_segment = 0;
            switch (ESCAPE_CODE(Self->Stream, i)) {
               case ESC::ADVANCE:
               case ESC::TABLE_START:
                  break_segment = 1;
                  break;

               case ESC::FONT:
                  if (m_text_content) {
                     auto style = &escape_data<escFont>(Self, i);
                     objFont *m_font = lookup_font(style->Index, "ESC::FONT");
                     if (m_font != m_font) break_segment = 1;
                  }
                  break;

               case ESC::VECTOR: {
                  auto vec = &escape_data<escVector>(Self, i);
                  break_segment = 1;
                  break;
               }

               case ESC::INDEX_START: {
                  auto index = &escape_data<escIndex>(Self, i);
                  if (!index->Visible) break_segment = 1;
                  break;
               }

               default: break;
            }

            if (break_segment) {
               DLAYOUT("Setting line at escape '%s', index %d, line.x: %d, m_word_width: %d", ESCAPE_NAME(Self->Stream,i).c_str(), m_line.index, m_line.x, m_word_width);
                  m_cursor_x += m_word_width;
                  add_drawsegment(m_line.index, i, m_cursor_y, m_cursor_x - m_line.x, m_align_width - m_line.x, "Esc:Vector");
                  reset_segment(i, m_cursor_x);
                  m_align_width = m_wrap_edge;               
            }
         }
      }

      // Wordwrap checking.  Any escape code that results in a word-break for the current word will initiate a wrapping
      // check.  Encountering whitespace also results in a wrapping check.

      if (esctable) {
         m_align_width = m_wrap_edge;
      }
      else {
         if (Self->Stream[i] IS CTRL_CODE) {
            switch (ESCAPE_CODE(Self->Stream, i)) {
               // These escape codes cause wrapping because they can break up words

               case ESC::PARAGRAPH_START:
               case ESC::PARAGRAPH_END:
               case ESC::TABLE_END:
               case ESC::VECTOR:
               case ESC::ADVANCE:
               case ESC::LINK_END:
                  checkwrap = true;
                  break;

               default:
                  m_align_width = m_wrap_edge;
                  break;
            }
         }
         else if (Self->Stream[i] > 0x20) { // Non-whitespace characters do not result in a wordwrap check
            m_align_width = m_wrap_edge;
         }
         else checkwrap = true;

         if (checkwrap) {
            checkwrap = false;
            auto wrap_result = check_wordwrap("Text", i, AbsX, Width, m_word_index, m_cursor_x, m_cursor_y, (m_word_width < 1) ? 1 : m_word_width, (m_line.height < 1) ? 1 : m_line.height);

            if (wrap_result IS WRAP::EXTEND_PAGE) {
               DLAYOUT("Expanding page width on wordwrap request.");
               goto extend_page;
            }
            else if ((Self->Stream[i] IS '\n') and (wrap_result IS WRAP::WRAPPED)) {
               // The presence of the line-break must be ignored, due to word-wrap having already made the new line for us
               i++;
               m_line.index = i;
               continue;
            }
         }
      }

      if (i >= End) break;

      if (Self->Stream[i] IS CTRL_CODE) { // Escape code encountered.
#ifdef DBG_LAYOUT_ESCAPE
         DLAYOUT("ESC_%s Indexes: %d-%d-%d, WordWidth: %d", 
            ESCAPE_NAME(Self->Stream, i).c_str(), m_line.index, i, m_word_index, m_word_width);
#endif
         m_set_segment = false; // Escape codes that draw something in draw_document() (e.g. vector, table) should set this flag to true in their case statement
         switch (ESCAPE_CODE(Self->Stream, i)) {
            case ESC::ADVANCE: {
               auto advance = &escape_data<escAdvance>(Self, i);
               m_cursor_x += advance->X;
               m_cursor_y += advance->Y;
               if (advance->X) reset_segment(i, m_cursor_x);               
               break;
            }

            case ESC::FONT:
               procFont(i);
               break;

            case ESC::INDEX_START: 
               procIndexStart(i);
               break;

            case ESC::SET_MARGINS: 
               procSetMargins(i, AbsY, Margins.Bottom); 
               break;            

            // LINK MANAGEMENT
             case ESC::LINK: 
               procLink(i); 
               break;

            case ESC::LINK_END: 
               procLinkEnd(i); 
               break;

            // LIST MANAGEMENT

            case ESC::LIST_START:
               // This is the start of a list.  Each item in the list will be identified by ESC::PARAGRAPH codes.  The
               // cursor position is advanced by the size of the item graphics element.

               liststate = LAYOUT_STATE(Self, i, *this);

               stack_list.push_back(&escape_data<escList>(Self, i));
list_repass:
               stack_list.back()->Repass = false;
               break;

            case ESC::LIST_END:
               // If it is a custom list, a repass is required

               if ((!stack_list.empty()) and (stack_list.back()->Type IS escList::CUSTOM) and (stack_list.back()->Repass)) {
                  DLAYOUT("Repass for list required, commencing...");
                  i = RESTORE_STATE(liststate);
                  goto list_repass;
               }

               stack_list.pop_back();               

               if (stack_list.empty()) {
                  // At the end of a list, increase the whitespace to that of a standard paragraph.
                  if (escpara) end_line(NL_PARAGRAPH, i, escpara->VSpacing, i, "Esc:ListEnd");
                  else end_line(NL_PARAGRAPH, i, 1.0, i, "Esc:ListEnd");
               }

               break;

            case ESC::VECTOR: {
               auto ww = procVector(i, Offset, AbsX, AbsY, *Width, page_height, vector_vertical_repass, checkwrap, escpara); 
               if (ww IS WRAP::EXTEND_PAGE) {
                  goto extend_page;
               }
               break;
            }

            case ESC::TABLE_START: {
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

               tablestate = LAYOUT_STATE(Self, i, *this);

               if (esctable) {
                  auto ptr = esctable;
                  esctable = &escape_data<escTable>(Self, i);
                  esctable->Stack = ptr;
               }
               else {
                  esctable = &escape_data<escTable>(Self, i);
                  esctable->Stack = NULL;
               }

               esctable->ResetRowHeight = true; // All rows start with a height of MinHeight up until TABLE_END in the first pass
               esctable->ComputeColumns = 1;
               esctable->Width = -1;

               for (unsigned j=0; j < esctable->Columns.size(); j++) esctable->Columns[j].MinWidth = 0;

               LONG width;
wrap_table_start:
               // Calculate starting table width, ensuring that the table meets the minimum width according to the cell
               // spacing and padding values.

               if (esctable->WidthPercent) {
                  width = ((*Width - (m_cursor_x - AbsX) - m_right_margin) * esctable->MinWidth) / 100;
               }
               else width = esctable->MinWidth;

               if (width < 0) width = 0;

               {
                  LONG min = (esctable->Thickness * 2) + (esctable->CellHSpacing * (esctable->Columns.size()-1)) + (esctable->CellPadding * 2 * esctable->Columns.size());
                  if (esctable->Thin) min -= esctable->CellHSpacing * 2; // Thin tables do not have spacing on the left and right borders
                  if (width < min) width = min;
               }

               if (width > WIDTH_LIMIT - m_cursor_x - m_right_margin) {
                  log.traceWarning("Table width in excess of allowable limits.");
                  width = WIDTH_LIMIT - m_cursor_x - m_right_margin;
                  if (Self->BreakLoop > 4) Self->BreakLoop = 4;
               }

               if (esctable->ComputeColumns) {
                  if (esctable->Width >= width) esctable->ComputeColumns = 0;
               }

               esctable->Width = width;

wrap_table_end:
wrap_table_cell:
               esctable->CursorX    = m_cursor_x;
               esctable->CursorY    = m_cursor_y;
               esctable->X          = m_cursor_x;
               esctable->Y          = m_cursor_y;
               esctable->RowIndex   = 0;
               esctable->TotalClips = Self->Clips.size();
               esctable->Height     = esctable->Thickness;

               DLAYOUT("(i%d) Laying out table of %dx%d, coords %dx%d,%dx%d%s, page width %d.", 
                  i, LONG(esctable->Columns.size()), esctable->Rows, esctable->X, esctable->Y, esctable->Width, esctable->MinHeight, esctable->HeightPercent ? "%" : "", *Width);

               esctable->computeColumns();

               DLAYOUT("Checking for table collisions before layout (%dx%d).  ResetRowHeight: %d", esctable->X, esctable->Y, esctable->ResetRowHeight);

               auto ww = check_wordwrap("Table", i, AbsX, Width, i, esctable->X, esctable->Y, (esctable->Width < 1) ? 1 : esctable->Width, esctable->Height);
               if (ww IS WRAP::EXTEND_PAGE) {
                  DLAYOUT("Expanding page width due to table size.");                  
                  goto extend_page;
               }
               else if (ww IS WRAP::WRAPPED) {
                  // The width of the table and positioning information needs to be recalculated in the event of a 
                  // table wrap.

                  DLAYOUT("Restarting table calculation due to page wrap to position %dx%d.", m_cursor_x, m_cursor_y);
                  esctable->ComputeColumns = 1;                  
                  goto wrap_table_start;
               }

               m_cursor_x = esctable->X;
               m_cursor_y = esctable->Y + esctable->Thickness + esctable->CellVSpacing;
               m_set_segment = true;
               break;
            }

            case ESC::TABLE_END: {
               auto action = procTableEnd(i, esctable, escpara, Offset, AbsX, Margins.Top, Margins.Bottom, Height, Width);
               if (action != TE::NIL) {
                  i = RESTORE_STATE(tablestate);
                  if (action IS TE::WRAP_TABLE) goto wrap_table_end;
                  else if (action IS TE::REPASS_ROW_HEIGHT) {
                     goto repass_row_height_ext;
                  }
                  else if (action IS TE::EXTEND_PAGE) goto extend_page;
               }
               break;
            }

            case ESC::ROW:
               stack_row.push_back(&escape_data<escRow>(Self, i));
               rowstate = LAYOUT_STATE(Self, i, *this);

               if (esctable->ResetRowHeight) stack_row.back()->RowHeight = stack_row.back()->MinHeight;

repass_row_height_ext:
               stack_row.back()->VerticalRepass = false;
               stack_row.back()->Y = m_cursor_y;
               esctable->RowWidth = (esctable->Thickness<<1) + esctable->CellHSpacing;

               m_set_segment = true;
               break;

            case ESC::ROW_END:
               procRowEnd(esctable);
               break;

            case ESC::CELL: {
               // In the first pass, the size of each cell is calculated with
               // respect to its content.  When ESC::TABLE_END is reached, the
               // max height and width for each row/column will be calculated
               // and a subsequent pass will be made to fill out the cells.
               //
               // If the width of a cell increases, there is a chance that the height of all
               // cells in that column will decrease, subsequently lowering the row height
               // of all rows in the table, not just the current row.  Therefore on the second
               // pass the row heights need to be recalculated from scratch.

               bool vertical_repass;

               esccell = &escape_data<escCell>(Self, i);

               if (!esctable) {
                  log.warning("escTable variable not defined for cell @ index %d - document byte code is corrupt.", i);
                  goto exit;
               }

               if (esccell->Column >= LONG(esctable->Columns.size())) {
                  DLAYOUT("Cell %d exceeds total table column limit of %d.", esccell->Column, LONG(esctable->Columns.size()));
                  break;
               }

               // Setting the line is the only way to ensure that the table graphics will be accounted for when drawing.

               add_drawsegment(i, i+ESCAPE_LEN, m_cursor_y, 0, 0, "Esc:Cell");

               // Set the AbsX location of the cel  AbsX determines the true location of the cell for do_layout()

               esccell->AbsX = m_cursor_x;
               esccell->AbsY = m_cursor_y;

               if (esctable->Thin) {
                  //if (esccell->Column IS 0);
                  //else esccell->AbsX += esctable->CellHSpacing;
               }
               else esccell->AbsX += esctable->CellHSpacing;               

               if (esccell->Column IS 0) esccell->AbsX += esctable->Thickness;

               esccell->Width  = esctable->Columns[esccell->Column].Width; // Minimum width for the cell's column
               esccell->Height = stack_row.back()->RowHeight;
               //DLAYOUT("%d / %d", escrow->MinHeight, escrow->RowHeight);

               DLAYOUT("Index %d, Processing cell at %dx %dy, size %dx%d, column %d", i, m_cursor_x, m_cursor_y, esccell->Width, esccell->Height, esccell->Column);

               // Find the matching CELL_END

               auto cell_end = i;
               while (cell_end < Self->Stream.size()) {
                  if (Self->Stream[cell_end] IS CTRL_CODE) {
                     if (ESCAPE_CODE(Self->Stream, cell_end) IS ESC::CELL_END) {
                        auto &end = escape_data<escCellEnd>(Self, cell_end);
                        if (end.CellID IS esccell->CellID) break;
                     }
                  }

                  NEXT_CHAR(Self->Stream, cell_end);
               }

               if (cell_end >= Self->Stream.size()) {
                  log.warning("Failed to find matching cell-end.  Document stream is corrupt.");
                  goto exit;
               }

               i += ESCAPE_LEN; // Go to start of cell content

               if (i < cell_end) {
                  auto segcount = Self->Segments.size();

                  if (!esccell->EditDef.empty()) Self->EditMode = true;
                  else Self->EditMode = false;

                     layout sl(Self);
                     i = sl.do_layout(i, cell_end, &m_font, esccell->AbsX, esccell->AbsY,
                            &esccell->Width, &esccell->Height,
                            ClipRectangle(esctable->CellPadding), &vertical_repass);

                  if (!esccell->EditDef.empty()) Self->EditMode = false;

                  if (!esccell->EditDef.empty()) {
                     // Edit cells have a minimum width/height so that the user can still interact with them when empty.

                     if (Self->Segments.size() IS segcount) {
                        // No content segments were created, which means that there's nothing for the cursor to attach
                        // itself too.

                        // Do we really want to do something here?
                        // I'd suggest that we instead break up the segments a bit more???  Another possibility - create an ESC::NULL
                        // type that gets placed at the start of the edit cell.  If there's no genuine content, then we at least have the ESC::NULL
                        // type for the cursor to be attached to?  ESC::NULL does absolutely nothing except act as faux content.


                        // TODO Work on this problem next
                     }

                     if (esccell->Width < 16) esccell->Width = 16;
                     if (esccell->Height < m_font->LineSpacing) {
                        esccell->Height = m_font->LineSpacing;
                     }
                  }
               }               

               if (!i) goto exit;

               DLAYOUT("Cell (%d:%d) is size %dx%d (min width %d)", esctable->RowIndex, esccell->Column, esccell->Width, esccell->Height, esctable->Columns[esccell->Column].Width);

               // Increase the overall width for the entire column if this cell has increased the column width.
               // This will affect the entire table, so a restart from TABLE_START is required.

               if (esctable->Columns[esccell->Column].Width < esccell->Width) {
                  DLAYOUT("Increasing column width of cell (%d:%d) from %d to %d (table_start repass required).", esctable->RowIndex, esccell->Column, esctable->Columns[esccell->Column].Width, esccell->Width);
                  esctable->Columns[esccell->Column].Width = esccell->Width; // This has the effect of increasing the minimum column width for all cells in the column

                  // Percentage based and zero columns need to be recalculated.  The easiest thing to do
                  // would be for a complete recompute (ComputeColumns = true) with the new minwidth.  The
                  // problem with ComputeColumns is that it does it all from scratch - we need to adjust it
                  // so that it can operate in a second style of mode where it recognises temporary width values.

                  esctable->Columns[esccell->Column].MinWidth = esccell->Width; // Column must be at least this size
                  esctable->ComputeColumns = 2;

                  esctable->ResetRowHeight = true; // Row heights need to be reset due to the width increase
                  i = RESTORE_STATE(tablestate);                  
                  goto wrap_table_cell;
               }

               // Advance the width of the entire row and adjust the row height

               esctable->RowWidth += esctable->Columns[esccell->Column].Width;

               if (!esctable->Thin) esctable->RowWidth += esctable->CellHSpacing;
               else if ((esccell->Column + esccell->ColSpan) < LONG(esctable->Columns.size())-1) esctable->RowWidth += esctable->CellHSpacing;

               if ((esccell->Height > stack_row.back()->RowHeight) or (stack_row.back()->VerticalRepass)) {
                  // A repass will be required if the row height has increased
                  // and vectors or tables have been used in earlier cells, because
                  // vectors need to know the final dimensions of their table cell.

                  if (esccell->Column IS LONG(esctable->Columns.size())-1) {
                     DLAYOUT("Extending row height from %d to %d (row repass required)", stack_row.back()->RowHeight, esccell->Height);
                  }

                  stack_row.back()->RowHeight = esccell->Height;
                  if ((esccell->Column + esccell->ColSpan) >= LONG(esctable->Columns.size())) {
                     i = RESTORE_STATE(rowstate);
                     goto repass_row_height_ext;
                  }
                  else stack_row.back()->VerticalRepass = true; // Make a note to do a vertical repass once all columns on this row have been processed
               }

               m_cursor_x += esctable->Columns[esccell->Column].Width;

               if (!esctable->Thin) m_cursor_x += esctable->CellHSpacing;
               else if ((esccell->Column + esccell->ColSpan) < LONG(esctable->Columns.size())) m_cursor_x += esctable->CellHSpacing;

               if (esccell->Column IS 0) m_cursor_x += esctable->Thickness;               
               break;
            }

            case ESC::CELL_END:
               procCellEnd(esccell);
               break;

            case ESC::PARAGRAPH_START: 
               escpara = procParagraphStart(i, escpara, *Width); 
               break;

            case ESC::PARAGRAPH_END: 
               escpara = procParagraphEnd(i, escpara); 
               break;

            default: break;
         }

         if (m_set_segment) {
            // Notice that this version of our call to add_drawsegment() does not define content position information (i.e. X/Y coordinates)
            // because we only expect to add an escape code to the drawing sequence, with the intention that the escape code carries
            // information relevant to the drawing process.  It is vital therefore that all content has been set with an earlier call
            // to add_drawsegment() before processing of the escape code.  See earlier in this routine.

            add_drawsegment(i, i+ESCAPE_LEN, m_cursor_y, 0, 0, ESCAPE_NAME(Self->Stream, i)); //"Esc:SetSegment");
            reset_segment(i+ESCAPE_LEN, m_cursor_x);
         }

         i += ESCAPE_LEN;
      }
      else {
         // If the font character is larger or equal to the current line height, extend
         // the height for the current line.  Note that we go for >= because we want to
         // correct the base line in case there is a vector already set on the line that
         // matches the font's line spacing.

         if (m_font->LineSpacing >= m_line.height) {
            m_line.height = m_font->LineSpacing;
            m_line.full_height = m_font->Ascent;
         }

         if (Self->Stream[i] IS '\n') {
#if 0
            // This link code is likely going to be needed for a case such as :
            //   <a href="">blah blah <br/> blah </a>
            // But we haven't tested it in a rpl document yet.

            if ((link) and (link_open IS false)) {
               // A link is due to be closed
               add_link(Self, ESC::LINK, link, link_x, m_cursor_y, m_cursor_x + m_word_width - link_x, m_line.height, "<br/>");
               link = NULL;
            }
#endif
            end_line(NL_PARAGRAPH, i+1 /* index */, 0 /* spacing */, i+1 /* restart-index */, "CarriageReturn");
           i++;
         }
         else if (Self->Stream[i] <= 0x20) {
            if (Self->Stream[i] IS '\t') {
               LONG tabwidth = (m_space_width + m_font->GlyphSpacing) * m_font->TabSize;
               if (tabwidth) m_cursor_x += pf::roundup(m_cursor_x, tabwidth);
               i++;
            }
            else {
               m_cursor_x += m_word_width + m_space_width;
               i++;
            }

            m_kernchar  = 0;
            m_word_width = 0;
            m_text_content = true;
         }
         else {
            LONG kerning;

            if (!m_word_width) m_word_index = i;   // Record the index of the new word (if this is one)

            i += getutf8(Self->Stream.c_str()+i, &unicode);
            m_word_width  += fntCharWidth(m_font, unicode, m_kernchar, &kerning);
            m_word_width  += kerning;
            m_kernchar     = unicode;
            m_text_content = true;
         }
      }
   } // while(1)

   // Check if the cursor + any remaining text requires closure

   if ((m_cursor_x + m_word_width > m_left_margin) or (m_word_index != -1)) {
      end_line(NL_NONE, i, 0, i, "SectionEnd");
   }

exit:

   page_height = calc_page_height(Self, m_start_clips, AbsY, Margins.Bottom);

   // Force a second pass if the page height has increased and there are vectors
   // on the page (the vectors may need to know the page height - e.g. if there
   // is a gradient filling the background).
   //
   // This feature is also handled in ESC::CELL, so we only perform it here
   // if processing is occurring within the root page area (Offset of 0).

   if ((!Offset) and (vector_vertical_repass) and (lastheight < page_height)) {
      DLAYOUT("============================================================");
      DLAYOUT("SECOND PASS [%d]: Root page height increased from %d to %d", Offset, lastheight, page_height);
      goto extend_page;
   }

   *Font = m_font;
   if (page_height > *Height) *Height = page_height;

   *VerticalRepass = vector_vertical_repass;

   Self->Depth--;   
   return i;
}

//********************************************************************************************************************
// Note that this function also controls the drawing of vectors that have loaded into the document (see the
// subscription hook in the layout process).
/*
static void draw_document(extDocument *Self, objSurface *Surface, objBitmap *Bitmap)
{
   pf::Log log(__FUNCTION__);
   escList *esclist;
   escLink *esclink;
   escParagraph *escpara;
   escTable *esctable;
   escCell *esccell;
   escRow *escrow;
   escVector *escvector;
   RGB8 link_save_rgb;
   UBYTE tabfocus, oob, m_cursor_drawn;

   if (Self->UpdateLayout) {
      // Drawing is disabled if the layout needs to be updated (this likely indicates that the document stream has been
      // modified and has yet to be recalculated - drawing while in this state is liable to lead to a crash)

      return;
   }
   
   auto font = lookup_font(0, "draw_document");

   if (!font) {
      log.traceWarning("No default font defined.");
      return;
   }

   #ifdef _DEBUG
   if (Self->Stream.empty()) {
      log.traceWarning("No content in stream or no segments.");
      return;
   }
   #endif

   Self->CurrentCell = NULL;
   font->Bitmap = Bitmap;
   esclist  = NULL;
   escpara  = NULL;
   esctable = NULL;
   escrow   = NULL;
   esccell  = NULL;
   tabfocus = false;
   m_cursor_drawn = false;

   #ifdef GUIDELINES

      // Page boundary is marked in blue
      gfxDrawRectangle(Bitmap, Self->LeftMargin-1, Self->TopMargin-1,
         Self->CalcWidth - Self->RightMargin - Self->LeftMargin + 2, Self->PageHeight - Self->TopMargin - Self->BottomMargin + 2,
         Bitmap->packPixel(0, 0, 255), 0);

      // Special clip regions are marked in grey
      for (unsigned i=0; i < Self->Clips.size(); i++) {
         gfxDrawRectangle(Bitmap, Self->Clips[i].Clip.Left, Self->Clips[i].Clip.Top,
            Self->Clips[i].Clip.Right - Self->Clips[i].Clip.Left, Self->Clips[i].Clip.Bottom - Self->Clips[i].Clip.Top,
            Bitmap->packPixel(255, 200, 200), 0);
      }
   #endif

   LONG select_start  = -1;
   LONG select_end    = -1;
   LONG select_startx = 0;
   LONG select_endx   = 0;

   if ((Self->ActiveEditDef) and (Self->SelectIndex IS -1)) {
      select_start  = Self->CursorIndex;
      select_end    = Self->CursorIndex;
      select_startx = Self->CursorCharX;
      select_endx   = Self->CursorCharX;
   }
   else if ((Self->CursorIndex != -1) and (Self->SelectIndex != -1)) {
      if (Self->SelectIndex < Self->CursorIndex) {
         select_start  = Self->SelectIndex;
         select_end    = Self->CursorIndex;
         select_startx = Self->SelectCharX;
         select_endx   = Self->CursorCharX;
      }
      else {
         select_start  = Self->CursorIndex;
         select_end    = Self->SelectIndex;
         select_startx = Self->CursorCharX;
         select_endx   = Self->SelectCharX;
      }
   }

   auto alpha = Bitmap->Opacity;
   for (unsigned seg=0; seg < Self->Segments.size(); seg++) {
      auto &segment = Self->Segments[seg];

      // Don't process segments that are out of bounds.  This can't be applied to vectors, as they can draw anywhere.

      oob = false;
      if (!segment.VectorContent) {
         if (segment.Y >= Bitmap->Clip.Bottom) oob = true;
         if (segment.Y + segment.Height < Bitmap->Clip.Top) oob = true;
         if (segment.X + segment.Width < Bitmap->Clip.Left) oob = true;
         if (segment.X >= Bitmap->Clip.Right) oob = true;
      }

      // Highlighting of selected text

      if ((select_start <= segment.Stop) and (select_end > segment.Index)) {
         if (select_start != select_end) {
            Bitmap->Opacity = 80;
            if ((select_start > segment.Index) and (select_start < segment.Stop)) {
               if (select_end < segment.Stop) {
                  gfxDrawRectangle(Bitmap, segment.X + select_startx, segment.Y,
                     select_endx - select_startx, segment.Height, Bitmap->packPixel(0, 128, 0), BAF::FILL);
               }
               else {
                  gfxDrawRectangle(Bitmap, segment.X + select_startx, segment.Y,
                     segment.Width - select_startx, segment.Height, Bitmap->packPixel(0, 128, 0), BAF::FILL);
               }
            }
            else if (select_end < segment.Stop) {
               gfxDrawRectangle(Bitmap, segment.X, segment.Y, select_endx, segment.Height,
                  Bitmap->packPixel(0, 128, 0), BAF::FILL);
            }
            else {
               gfxDrawRectangle(Bitmap, segment.X, segment.Y, segment.Width, segment.Height,
                  Bitmap->packPixel(0, 128, 0), BAF::FILL);
            }
            Bitmap->Opacity = 255;
         }
      }

      if ((Self->ActiveEditDef) and (Self->CursorState) and (!m_cursor_drawn)) {
         if ((Self->CursorIndex >= segment.Index) and (Self->CursorIndex <= segment.Stop)) {
            if ((Self->CursorIndex IS segment.Stop) and (Self->Stream[Self->CursorIndex-1] IS '\n')); // The -1 looks naughty, but it works as CTRL_CODE != \n, so use of PREV_CHAR() is unnecessary
            else if ((Self->Page->Flags & VF::HAS_FOCUS) != VF::NIL) { // Standard text cursor
               gfxDrawRectangle(Bitmap, segment.X + Self->CursorCharX, segment.Y, 2, segment.BaseLine,
                  Bitmap->packPixel(255, 0, 0), BAF::FILL);
               m_cursor_drawn = true;               
            }
         }
      }

      #ifdef GUIDELINES_CONTENT
         if (segment.TextContent) {
            gfxDrawRectangle(Bitmap,
               segment.X, segment.Y,
               (segment.Width > 0) ? segment.Width : 5, segment.Height,
               Bitmap->packPixel(0, 255, 0), 0);
         }
      #endif

      std::string strbuffer;
      strbuffer.reserve(segment.Stop - segment.Index + 1);

      auto fx = segment.X;
      auto i  = segment.Index;
      auto si = 0;

      while (i < segment.TrimStop) {
         if (Self->Stream[i] IS CTRL_CODE) {
            switch (ESCAPE_CODE(Self->Stream, i)) {
               case ESC::VECTOR: {
                  OBJECTPTR object;

                  escvector = &escape_data<escVector>(Self, i);

                  if ((escvector->Graphical) and (!escvector->Owned)) {
                     if (escvector->ObjectID < 0) {
                        object = NULL;
                        AccessObject(escvector->ObjectID, 3000, &object);
                     }
                     else object = GetObjectPtr(escvector->ObjectID);
*
                     if (object) {
                        objLayout *layout;

                        if ((FindField(object, FID_Layout, NULL)) and (!object->getPtr(FID_Layout, &layout))) {
                           if (vector->DrawCallback.Type) {
                              // If the graphic is within a cell, ensure that the graphic does not exceed
                              // the dimensions of the cell.

                              if (Self->CurrentCell) {
                                 if (vector->BoundX + vector->BoundWidth > Self->CurrentCell->AbsX + Self->CurrentCell->Width) {
                                    vector->BoundWidth  = Self->CurrentCell->AbsX + Self->CurrentCell->Width - vector->BoundX;
                                 }

                                 if (vector->BoundY + vector->BoundHeight > Self->CurrentCell->AbsY + Self->CurrentCell->Height) {
                                    vector->BoundHeight = Self->CurrentCell->AbsY + Self->CurrentCell->Height - vector->BoundY;
                                 }
                              }

                              auto opacity = Bitmap->Opacity;
                              Bitmap->Opacity = 255;
                              auto routine = (void (*)(OBJECTPTR, rkSurface *, objBitmap *))vector->DrawCallback.StdC.Routine;
                              routine(object, Surface, Bitmap);
                              Bitmap->Opacity = opacity;
                           }
                        }

                        if (escvector->ObjectID < 0) ReleaseObject(object);
                     }
*
                  }

                  break;
               }

               case ESC::FONT: {
                  auto &style = escape_data<escFont>(Self, i);
                  if (auto font = lookup_font(style.Index, "draw_document")) {
                     font->Bitmap = Bitmap;
                     if (tabfocus IS false) font->Colour = style.Fill;
                     else font->Fill = Self->LinkSelectFill;

                     if ((style.Options & FSO::ALIGN_RIGHT) != FSO::NIL) font->Align = ALIGN::RIGHT;
                     else if ((style.Options & FSO::ALIGN_CENTER) != FSO::NIL) font->Align = ALIGN::HORIZONTAL;
                     else font->Align = ALIGN::NIL;

                     if ((style.Options & FSO::UNDERLINE) != FSO::NIL) font->Underline = font->Colour;
                     else font->Underline.Alpha = 0;
                  }
                  break;
               }

               case ESC::LIST_START:
                  if (esclist) {
                     auto ptr = esclist;
                     esclist = &escape_data<escList>(Self, i);
                     esclist->Stack = ptr;
                  }
                  else esclist = &escape_data<escList>(Self, i);
                  break;

               case ESC::LIST_END:
                  if (esclist) esclist = esclist->Stack;
                  break;

               case ESC::PARAGRAPH_START:
                  if (escpara) {
                     auto ptr = escpara;
                     escpara = &escape_data<escParagraph>(Self, i);
                     escpara->Stack = ptr;
                  }
                  else escpara = &escape_data<escParagraph>(Self, i);

                  if ((esclist) and (escpara->ListItem)) {
                     // Handling for paragraphs that form part of a list

                     if ((esclist->Type IS escList::CUSTOM) or (esclist->Type IS escList::ORDERED)) {
                        if (!escpara->Value.empty()) {
                           font->X = fx - escpara->ItemIndent;
                           font->Y = segment.Y + font->Leading + (segment.BaseLine - font->Ascent);
                           font->AlignWidth = segment.AlignWidth;
                           font->setString(escpara->Value);
                           font->draw();
                        }
                     }
                     else if (esclist->Type IS escList::BULLET) {
                        #define SIZE_BULLET 5
                        // TODO: Requires conversion to vector
                        //gfxDrawEllipse(Bitmap,
                        //   fx - escpara->ItemIndent, segment.Y + ((segment.BaseLine - SIZE_BULLET)/2),
                        //   SIZE_BULLET, SIZE_BULLET, Bitmap->packPixel(esclist->Colour), true);
                     }
                  }
                  break;

               case ESC::PARAGRAPH_END:
                  if (escpara) escpara = escpara->Stack;
                  break;

               case ESC::TABLE_START: {
                  if (esctable) {
                     auto ptr = esctable;
                     esctable = &escape_data<escTable>(Self, i);
                     esctable->Stack = ptr;
                  }
                  else esctable = &escape_data<escTable>(Self, i);

                  //log.trace("Draw Table: %dx%d,%dx%d", esctable->X, esctable->Y, esctable->Width, esctable->Height);

                  if (esctable->Colour.Alpha > 0) {
                     gfxDrawRectangle(Bitmap,
                        esctable->X+esctable->Thickness, esctable->Y+esctable->Thickness,
                        esctable->Width-(esctable->Thickness<<1), esctable->Height-(esctable->Thickness<<1),
                        Bitmap->packPixel(esctable->Colour), BAF::FILL|BAF::BLEND);
                  }

                  if (esctable->Shadow.Alpha > 0) {
                     Bitmap->Opacity = esctable->Shadow.Alpha;
                     for (LONG j=0; j < esctable->Thickness; j++) {
                        gfxDrawRectangle(Bitmap,
                           esctable->X+j, esctable->Y+j,
                           esctable->Width-(j<<1), esctable->Height-(j<<1),
                           Bitmap->packPixel(esctable->Shadow), BAF::NIL);
                     }
                     Bitmap->Opacity = alpha;
                  }
                  break;
               }

               case ESC::TABLE_END:
                  if (esctable) esctable = esctable->Stack;
                  break;

               case ESC::ROW: {
                  if (escrow) {
                     auto ptr = escrow;
                     escrow = &escape_data<escRow>(Self, i);
                     escrow->Stack = ptr;
                  }
                  else escrow = &escape_data<escRow>(Self, i);

                  if (escrow->Colour.Alpha) {
                     gfxDrawRectangle(Bitmap, esctable->X, escrow->Y, esctable->Width, escrow->RowHeight,
                        Bitmap->packPixel(escrow->Colour), BAF::FILL|BAF::BLEND);
                  }
                  break;
               }

               case ESC::ROW_END:
                  if (escrow) escrow = escrow->Stack;
                  break;

               case ESC::CELL: {
                  if (esccell) {
                     auto ptr = esccell;
                     esccell = &escape_data<escCell>(Self, i);
                     esccell->Stack = ptr;
                  }
                  else esccell = &escape_data<escCell>(Self, i);

                  Self->CurrentCell = esccell;

                  if (esccell->Colour.Alpha > 0) { // Fill colour
                     WORD border;
                     if (esccell->Shadow.Alpha > 0) border = 1;
                     else border = 0;

                     gfxDrawRectangle(Bitmap, esccell->AbsX+border, esccell->AbsY+border,
                        esctable->Columns[esccell->Column].Width-border, escrow->RowHeight-border,
                        Bitmap->packPixel(esccell->Colour), BAF::FILL|BAF::BLEND);
                  }

                  if (esccell->Shadow.Alpha > 0) { // Border colour
                     gfxDrawRectangle(Bitmap, esccell->AbsX, esccell->AbsY, esctable->Columns[esccell->Column].Width,
                        escrow->RowHeight, Bitmap->packPixel(esccell->Shadow), BAF::NIL);
                  }
                  break;
               }

               case ESC::CELL_END:
                  if (esccell) esccell = esccell->Stack;
                  Self->CurrentCell = esccell;
                  break;

               case ESC::LINK: {
                  esclink = &escape_data<escLink>(Self, i);
                  if (Self->HasFocus) {
                     if ((Self->Tabs[Self->FocusIndex].Type IS TT_LINK) and (Self->Tabs[Self->FocusIndex].Ref IS esclink->ID) and (Self->Tabs[Self->FocusIndex].Active)) {
                        link_save_rgb = font->Colour;
                        font->Colour = Self->LinkSelectFill;
                        tabfocus = true;
                     }
                  }

                  break;
               }

               case ESC::LINK_END:
                  if (tabfocus) {
                     font->Colour = link_save_rgb;
                     tabfocus = false;
                  }
                  break;

               default: break;
            }

            i += ESCAPE_LEN;
         }
         else if (!oob) {
            if (Self->Stream[i] <= 0x20) { strbuffer[si++] = ' '; i++; }
            else strbuffer[si++] = Self->Stream[i++];

            // Print the string buffer content if the next string character is an escape code.

            if (Self->Stream[i] IS CTRL_CODE) {
               strbuffer[si] = 0;
               font->X = fx;
               font->Y = segment.Y + font->Leading + (segment.BaseLine - font->Ascent);
               font->AlignWidth = segment.AlignWidth;
               font->setString(strbuffer);
               font->draw();
               fx = font->EndX;
               si = 0;
            }
         }
         else i++;
      }

      strbuffer[si] = 0;

      if ((si > 0) and (!oob)) {
         font->X = fx;
         font->Y = segment.Y + font->Leading + (segment.BaseLine - font->Ascent);
         font->AlignWidth = segment.AlignWidth;
         font->setString(strbuffer);
         font->draw();
         fx = font->EndX;
      }
   } // for loop
}
*/

//********************************************************************************************************************
// This function is called only when a paragraph or explicit line-break (\n) is encountered.

void layout::end_line(LONG NewLine, INDEX Index, DOUBLE Spacing, LONG RestartIndex, const std::string &Caller)
{
   pf::Log log(__FUNCTION__);

   if ((!m_line.height) and (m_word_width)) {
      // If this is a one-word line, the line height will not have been defined yet
      //log.trace("Line Height being set to font (currently %d/%d)", m_line.height, m_line.full_height);
      m_line.height = m_font->LineSpacing;
      m_line.full_height = m_font->Ascent;
   }

   DLAYOUT("%s: CursorY: %d, ParaY: %d, ParaEnd: %d, Line Height: %d * %.2f, Index: %d/%d, Restart: %d", 
      Caller.c_str(), m_cursor_y, m_paragraph_y, m_paragraph_bottom, m_line.height, Spacing, m_line.index, Index, RestartIndex);

   for (unsigned i=m_start_clips; i < Self->Clips.size(); i++) {
      if (Self->Clips[i].Transparent) continue;
      if ((m_cursor_y + m_line.height >= Self->Clips[i].Clip.Top) and (m_cursor_y < Self->Clips[i].Clip.Bottom)) {
         if (m_cursor_x + m_word_width < Self->Clips[i].Clip.Left) {
            if (Self->Clips[i].Clip.Left < m_align_width) m_align_width = Self->Clips[i].Clip.Left;
         }
      }
   }

   if (Index > m_line.index) {
      add_drawsegment(m_line.index, Index, m_cursor_y, m_cursor_x + m_word_width - m_line.x, m_align_width - m_line.x, "Esc:EndLine");
   }

   // Determine the new vertical position of the cursor.  This routine takes into account multiple line-breaks, so that
   // the overall amount of whitespace is no more than the biggest line-break specified in
   // a line-break sequence.

   if (NewLine) {
      auto bottom_line = m_cursor_y + m_line.height;
      if (m_paragraph_bottom > bottom_line) bottom_line = m_paragraph_bottom;

      // Check for a previous paragraph escape sequence.  This resolves cases such as "<p>...<p>...</p></p>"

      if (auto i = Index; i > 0) {
         PREV_CHAR(Self->Stream, i);
         while (i > 0) {
            if (Self->Stream[i] IS CTRL_CODE) {
               if ((ESCAPE_CODE(Self->Stream, i) IS ESC::PARAGRAPH_END) or
                   (ESCAPE_CODE(Self->Stream, i) IS ESC::PARAGRAPH_START)) {

                  if (ESCAPE_CODE(Self->Stream, i) IS ESC::PARAGRAPH_START) {
                     // Check if a custom string is specified in the paragraph, in which case the paragraph counts
                     // as content.

                     auto &para = escape_data<escParagraph>(Self, i);
                     if (!para.Value.empty()) break;
                  }

                  bottom_line = m_paragraph_y;
                  break;
               }
               else if ((ESCAPE_CODE(Self->Stream, i) IS ESC::VECTOR) or (ESCAPE_CODE(Self->Stream, i) IS ESC::TABLE_END)) break; // Content encountered

               PREV_CHAR(Self->Stream, i);
            }
            else break; // Content encountered
         }
      }

      m_paragraph_y = bottom_line;

      // Paragraph gap measured as default line height * spacing ratio

      auto new_y = bottom_line + F2I(Self->LineHeight * Spacing);
      if (new_y > m_cursor_y) m_cursor_y = new_y;
   }

   // Reset line management variables for a new line starting from the left margin.

   m_cursor_x         = m_left_margin;
   m_line.x           = m_left_margin;
   m_line.height      = 0;
   m_line.full_height = 0;
   m_split_start      = Self->Segments.size();
   m_line.index       = RestartIndex;
   m_word_index       = m_line.index;
   m_kernchar         = 0;
   m_word_width       = 0;
   m_paragraph_bottom = 0;   
}

//********************************************************************************************************************
// Word-wrapping is checked whenever whitespace is encountered or certain escape codes are found in the text stream,
// e.g. paragraphs and vectors will mark an end to the current word.
//
// Wrapping is always checked even if there is no 'active word' because we need to be able to wrap empty lines (e.g.
// solo <br/> tags).
//
// Index - The current index value.
// VectorIndex - The index that indicates the start of the word.

WRAP layout::check_wordwrap(const std::string &Type, INDEX Index, LONG X, LONG *Width,
   INDEX VectorIndex, LONG &GraphicX, LONG &GraphicY, LONG GraphicWidth, LONG GraphicHeight)
{
   pf::Log log(__FUNCTION__);

   if (!Self->BreakLoop) return WRAP::DO_NOTHING;

   // If the width of the vector is larger than the available page width, extend the size of the page.

/*
   if (GraphicWidth > *Width - m_left_margin - m_right_margin) {
      *Width = GraphicWidth + m_left_margin + m_right_margin;
      return WRAP_EXTENDPAGE;
   }
*/

   // This code pushes the vector along to the next available space when a boundary is encountered on the current line.

#ifdef DBG_WORDWRAP
   log.branch("Index: %d/%d, %s: %dx%d,%dx%d, LineHeight: %d, Cursor: %dx%d, PageWidth: %d, Edge: %d", 
      Index, VectorIndex, Type.c_str(), GraphicX, GraphicY, GraphicWidth, GraphicHeight, m_line.height, m_cursor_x, m_cursor_y, *Width, m_wrap_edge);
#endif

   auto result = WRAP::DO_NOTHING;
   LONG breakloop = MAXLOOP;

restart:
   m_align_width = m_wrap_edge;

   if (!Self->Clips.empty()) check_clips(Index, VectorIndex, GraphicX, GraphicY, GraphicWidth, GraphicHeight);

   if (GraphicX + GraphicWidth > m_wrap_edge) {
      if ((*Width < WIDTH_LIMIT) and ((GraphicX IS m_left_margin) or (m_no_wrap))) {
         // Force an extension of the page width and recalculate from scratch
         auto minwidth = GraphicX + GraphicWidth + m_right_margin - X;
         if (minwidth > *Width) {
            *Width = minwidth;
            DWRAP("Forcing an extension of the page width to %d", minwidth);
         }
         else *Width += 1;
         return WRAP::EXTEND_PAGE;
      }
      else {
         if (!m_line.height) {
            m_line.height = 1; //font->LineSpacing;
            m_line.full_height = 1; //font->Ascent;
         }

         if (m_current_link) {
            if (m_link.x IS GraphicX) {
               // If the link starts with the vector, the link itself is going to be wrapped with it
            }
            else {
               add_link(Self, ESC::LINK, m_current_link, m_link.x, GraphicY, GraphicX - m_link.x, m_line.height, "check_wrap");
            }
         }

         // Set the line segment up to the vector index.  The line.index is
         // updated so that this process only occurs in the first iteration.

         if (m_line.index < VectorIndex) {
            add_drawsegment(m_line.index, VectorIndex, GraphicY, GraphicX - m_line.x, m_align_width - m_line.x, "DoWrap");
            m_line.index = VectorIndex;
         }

         // Reset the line management variables so that the next line starts at the left margin.

         GraphicX       = m_left_margin;
         GraphicY      += m_line.height;
         m_cursor_x     = GraphicX;
         m_cursor_y     = GraphicY;
         m_split_start  = Self->Segments.size();
         m_line.x       = m_left_margin;
         m_link.x         = m_left_margin; // Only matters if a link is defined
         m_kernchar     = 0;
         m_line.full_height = 0;
         m_line.height      = 0;

         result = WRAP::WRAPPED;
         if (--breakloop > 0) goto restart; // Go back and check the clip boundaries again
         else {
            log.traceWarning("Breaking out of continuous loop.");
            Self->Error = ERR_Loop;
         }
      }
   }

   // No wrap has occurred

   if ((m_current_link) and (!m_link.open)) { // A link is due to be closed
      add_link(Self, ESC::LINK, m_current_link, m_link.x, GraphicY, GraphicX + GraphicWidth - m_link.x, m_line.height ? m_line.height : m_font->LineSpacing, "check_wrap");
      m_current_link = NULL;
   }

   #ifdef DBG_WORDWRAP
      if (result IS WRAP::WRAPPED) DWRAP("A wrap to Y coordinate %d has occurred.", m_cursor_y);
   #endif

   return result;
}

void layout::check_clips(INDEX Index, INDEX VectorIndex, LONG &GraphicX, LONG &GraphicY, LONG GraphicWidth, LONG GraphicHeight)
{
   pf::Log log(__FUNCTION__);

#ifdef DBG_WORDWRAP
   log.branch("Index: %d-%d, VectorIndex: %d, Graphic: %dx%d,%dx%d, TotalClips: %d", 
      m_line.index, Index, VectorIndex, GraphicX, GraphicY, GraphicWidth, GraphicHeight, LONG(Self->Clips.size()));
#endif

   for (auto clip=m_start_clips; clip < LONG(Self->Clips.size()); clip++) {
      if (Self->Clips[clip].Transparent) continue;
      if (GraphicY + GraphicHeight < Self->Clips[clip].Clip.Top) continue;
      if (GraphicY >= Self->Clips[clip].Clip.Bottom) continue;
      if (GraphicX >= Self->Clips[clip].Clip.Right) continue;
      if (GraphicX + GraphicWidth < Self->Clips[clip].Clip.Left) continue;

      if (Self->Clips[clip].Clip.Left < m_align_width) m_align_width = Self->Clips[clip].Clip.Left;

      DWRAP("Word: \"%.20s\" (%dx%d,%dx%d) advances over clip %d-%d",
         printable(Self, VectorIndex).c_str(), GraphicX, GraphicY, GraphicWidth, GraphicHeight,
         Self->Clips[clip].Clip.Left, Self->Clips[clip].Clip.Right);

      // Set the line segment up to the encountered boundary and continue checking the vector position against the
      // clipping boundaries.

      bool reset_link;
      if ((m_current_link) and (Self->Clips[clip].Index < m_link.index)) {
         // An open link intersects with a clipping region that was created prior to the opening of the link.  We do
         // not want to include this vector as a clickable part of the link - we will wrap over or around it, so
         // set a partial link now and ensure the link is reopened after the clipping region.

         DWRAP("Setting hyperlink now to cross a clipping boundary.");

         auto height = m_line.height ? m_line.height : m_font->LineSpacing;
         add_link(Self, ESC::LINK, m_current_link, m_link.x, GraphicY, GraphicX + GraphicWidth - m_link.x, height, "clip_intersect");

         reset_link = true;
      }
      else reset_link = false;

      // Advance the vector position.  We break if a wordwrap is required - the code outside of this loop will detect
      // the need for a wordwrap and then restart the wordwrapping process.

      if (GraphicX IS m_line.x) m_line.x = Self->Clips[clip].Clip.Right;
      GraphicX = Self->Clips[clip].Clip.Right; // Push the vector over the clip boundary

      if (GraphicX + GraphicWidth > m_wrap_edge) {
         DWRAP("Wrapping-Break: X(%d)+Width(%d) > Edge(%d) at clip '%s' %dx%d,%dx%d", 
            GraphicX, GraphicWidth, m_wrap_edge, Self->Clips[clip].Name.c_str(), Self->Clips[clip].Clip.Left, Self->Clips[clip].Clip.Top, Self->Clips[clip].Clip.Right, Self->Clips[clip].Clip.Bottom);
         break;
      }

      INDEX i = ((GraphicWidth) and (VectorIndex >= 0)) ? VectorIndex : Index;

      if (m_line.index < i) {
         if (!m_line.height) {
            add_drawsegment(m_line.index, i, GraphicY, GraphicX - m_line.x, GraphicX - m_line.x, "Wrap:EmptyLine");
         }
         else add_drawsegment(m_line.index, i, GraphicY, GraphicX + GraphicWidth - m_line.x, m_align_width - m_line.x, "Wrap");
      }

      DWRAP("Line index reset to %d, previously %d", i, m_line.index);

      m_line.index = i;
      m_line.x = GraphicX;
      if ((reset_link) and (m_current_link)) m_link.x = GraphicX;

      clip = m_start_clips-1; // Check all the clips from the beginning
   }
}

//********************************************************************************************************************
// Record a clickable link, cell, or other form of clickable area.

static void add_link(extDocument *Self, ESC EscapeCode, APTR Escape, LONG X, LONG Y, LONG Width, LONG Height, CSTRING Caller)
{
   pf::Log log(__FUNCTION__);

   if ((!Self) or (!Escape)) return;

   if ((Width < 1) or (Height < 1)) {
      log.traceWarning("Illegal width/height for link @ %dx%d, W/H %dx%d [%s]", X, Y, Width, Height, Caller);
      return;
   }

   DLAYOUT("%dx%d,%dx%d, %s", X, Y, Width, Height, Caller);

   Self->Links.emplace_back(EscapeCode, Escape, Self->Segments.size(), X, Y, Width, Height);
}
