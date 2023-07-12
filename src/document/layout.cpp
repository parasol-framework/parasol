
#define TE_WRAP_TABLE        1
#define TE_REPASS_ROW_HEIGHT 2
#define TE_EXTEND_PAGE       3

// State machine for the layout process

struct layout {
   extDocument *Self;
   objFont *font;
   escLink *current_link;
   LONG alignflags;
   LONG alignwidth;
   LONG cursor_x, cursor_y;
   LONG kernchar;
   LONG left_margin;
   LONG paragraph_end;
   LONG paragraph_y;
   LONG right_margin;
   LONG split_start;
   LONG start_clips;
   INDEX wordindex;
   LONG wordwidth;
   LONG wrapedge;
   WORD spacewidth;
   UBYTE anchor;
   bool nowrap;
   bool setsegment;
   bool textcontent;
  
   struct {
      LONG full_height;  // The complete height of the line, covers the height of all objects and tables anchored to the line.  Text is drawn so that the text gutter is aligned to the base line
      LONG height;       // Height of the line with respect to the text
      LONG increase;
      LONG index;
      LONG x;
   } line;

   struct {
      LONG x;
      INDEX index;
      ALIGN align;
      bool open;
   } link;
   
   // Resets the string management variables, usually done when a string
   // has been broken up on the current line due to an object or table graphic for example.

   void reset_segment(INDEX Index, LONG X) {
      line.index  = Index; 
      line.x      = X; 
      kernchar    = 0; 
      wordindex   = -1; 
      wordwidth   = 0; 
      textcontent = 0;
   }

   layout(extDocument *pSelf) : Self(pSelf) { }

   void injectSetMargins(LONG, LONG, LONG &);
   void injectLink(LONG);
   void injectLinkEnd(LONG);
   void injectIndexStart(LONG &);
   void injectObject(LONG, LONG, LONG, LONG, LONG, LONG);
   escParagraph * injectParagraphStart(LONG, escParagraph *, escList *, LONG);
   escParagraph * injectParagraphEnd(LONG, escParagraph *);
   LONG injectTableEnd(LONG, escTable *, escRow *, escParagraph *, LONG, LONG, LONG, LONG, LONG *, LONG *);
   void add_drawsegment(LONG, LONG, LONG, LONG, LONG, const std::string &);

   void end_line(LONG NewLine, INDEX Index, DOUBLE Spacing, LONG RestartIndex, const std::string &);
   UBYTE check_wordwrap(const std::string &, INDEX Index, LONG X, LONG *Width,
      LONG ObjectIndex, LONG &GraphicX, LONG &GraphicY, LONG GraphicWidth, LONG GraphicHeight);
   void check_clips(INDEX Index, LONG ObjectIndex, LONG &GraphicX, LONG &GraphicY, LONG GraphicWidth, LONG GraphicHeight);
};

//********************************************************************************************************************

void layout::injectLink(INDEX Index)
{
   if (current_link) {
      // Close the currently open link because it's illegal to have a link embedded within a link.

      if (font) {
         add_link(Self, ESC::LINK, current_link, link.x, cursor_y, cursor_x + wordwidth - link.x, line.height ? line.height : font->LineSpacing, "esc_link");
      }
   }

   current_link = &escape_data<::escLink>(Self, Index);
   link.x     = cursor_x + wordwidth;
   link.index = Index;
   link.open  = true;
   link.align = font->Align;
}

void layout::injectLinkEnd(INDEX Index)
{
   // We don't call add_link() unless the entire word that contains the link has
   // been processed.  This is necessary due to the potential for a word-wrap.

   if (current_link) {
      link.open = false;

      if (wordwidth < 1) {
         add_link(Self, ESC::LINK, current_link, link.x, cursor_y, cursor_x - link.x, line.height ? line.height : font->LineSpacing, "esc_link_end");
         current_link = NULL;
      }
   }   
}

//********************************************************************************************************************

void layout::injectIndexStart(INDEX &Index)
{
   pf::Log log(__FUNCTION__);

   // Indexes don't do anything, but recording the cursor's Y value when they are encountered
   // makes it really easy to scroll to a bookmark when requested (show_bookmark()).

   auto escindex = &escape_data<escIndex>(Self, Index);
   escindex->Y = cursor_y;

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

      line.index = end;
      Index = end;
   }
}

//********************************************************************************************************************

escParagraph * layout::injectParagraphStart(INDEX Index, escParagraph *Parent, escList *List, LONG Width)
{
   escParagraph *escpara;

   if (Parent) {
      DOUBLE ratio;
   
      // If a paragraph is embedded within a paragraph, insert a newline before the new paragraph starts.
   
      left_margin = Parent->X; // Reset the margin so that the next line will be flush with the parent
   
      if (paragraph_y > 0) {
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
   
      if ((escpara->LeadingRatio > 0) and (paragraph_y > 0)) {
         end_line(NL_PARAGRAPH, Index, escpara->LeadingRatio, Index, "Esc:PStart");
      }
   }
   
   // Indentation support
   
   if (List) {
      // For list items, indentation is managed by the list that this paragraph is contained within.
   
      if (escpara->ListItem) {
         if (Parent) escpara->Indent = List->BlockIndent;
         escpara->ItemIndent = List->ItemIndent;
         escpara->Relative = false;
   
         if (!escpara->Value.empty()) {
            auto strwidth = fntStringWidth(font, escpara->Value.c_str(), -1) + 10;
            if (strwidth > List->ItemIndent) {
               List->ItemIndent = strwidth;
               escpara->ItemIndent = strwidth;
               List->Repass     = true;
            }
         }
      }
      else escpara->Indent = List->ItemIndent;
   }
   
   if (escpara->Indent) {
      if (escpara->Relative) escpara->BlockIndent = escpara->Indent * 100 / Width;
      else escpara->BlockIndent = escpara->Indent;
   }
   
   escpara->X = left_margin + escpara->BlockIndent;
   
   left_margin += escpara->BlockIndent + escpara->ItemIndent;
   cursor_x    += escpara->BlockIndent + escpara->ItemIndent;
   line.x      += escpara->BlockIndent + escpara->ItemIndent;
   
   // Paragraph management variables
   
   if (List) escpara->VSpacing = List->VSpacing;   
   
   escpara->Y = cursor_y;
   escpara->Height = 0;

   return escpara;
}

escParagraph * layout::injectParagraphEnd(INDEX Index, escParagraph *Current)
{
   if (Current) {
      // The paragraph height reflects the true size of the paragraph after we take into account
      // any objects and tables within the paragraph.

      paragraph_end = Current->Y + Current->Height;

      end_line(NL_PARAGRAPH, Index, Current->VSpacing, Index + ESCAPE_LEN, "Esc:PEnd");

      left_margin = Current->X - Current->BlockIndent;
      cursor_x     = Current->X - Current->BlockIndent;
      line.x      = Current->X - Current->BlockIndent;

      return Current->Stack;
   }
   else {
      end_line(NL_PARAGRAPH, Index, Current->VSpacing, Index + ESCAPE_LEN, "Esc:PEnd-NP");
      return NULL;
   }
}

//********************************************************************************************************************

LONG layout::injectTableEnd(INDEX Index, escTable *esctable, escRow *LastRow, escParagraph *escpara, LONG Offset, LONG AbsX, LONG TopMargin, LONG BottomMargin, LONG *Height, LONG *Width)
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
               DOUBLE cellwidth = avail_width / unfixed;
               for (unsigned j=0; j < esctable->Columns.size(); j++) {
                  if ((esctable->Columns[j].MinWidth) and (esctable->Columns[j].MinWidth > cellwidth)) {
                     avail_width -= esctable->Columns[j].MinWidth;
                     unfixed--;
                  }
               }

               if (unfixed > 0) {
                  cellwidth = avail_width / unfixed;
                  bool expanded = false;

                  //total = 0;
                  for (unsigned j=0; j < esctable->Columns.size(); j++) {
                     if (esctable->Columns[j].PresetWidth) continue; // Columns with preset-widths are never auto-expanded
                     if (esctable->Columns[j].MinWidth > cellwidth) continue;

                     if (esctable->Columns[j].Width < cellwidth) {
                        DLAYOUT("Expanding column %d from width %d to %.2f", j, esctable->Columns[j].Width, cellwidth);
                        esctable->Columns[j].Width = cellwidth;
                        //if (total - (DOUBLE)F2I(total) >= 0.5) esctable->Columns[j].Width++; // Fractional correction

                        expanded = true;
                     }
                     //total += cellwidth;
                  }

                  if (expanded) {
                     DLAYOUT("At least one cell was widened - will repass table layout.");
                     return TE_WRAP_TABLE;
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
      if (LastRow) {
         auto j = minheight - (esctable->Height + esctable->CellVSpacing + esctable->Thickness);
         DLAYOUT("Extending table height to %d (row %d+%d) due to a minimum height of %d at coord %d", minheight, LastRow->RowHeight, j, esctable->MinHeight, esctable->Y);
         LastRow->RowHeight += j;
         return TE_REPASS_ROW_HEIGHT;
      }
      else log.warning("No last row defined for table height extension.");
   }

   // Adjust for cellspacing at the bottom

   esctable->Height += esctable->CellVSpacing + esctable->Thickness;

   // Restart if the width of the table will force an extension of the page.

   LONG j = esctable->X + esctable->Width - AbsX + right_margin;
   if ((j > *Width) and (*Width < WIDTH_LIMIT)) {
      DLAYOUT("Table width (%d+%d) increases page width to %d, layout restart forced.", esctable->X, esctable->Width, j);
      *Width = j;                  
      return TE_EXTEND_PAGE;
   }

   // Extend the height of the current line to the height of the table if the table is to be anchored (a
   // technique typically applied to objects).  We also extend the line height if the table covers the
   // entire width of the page (this is a valuable optimisation for the layout routine).

   if ((anchor) or ((esctable->X <= left_margin) and (esctable->X + esctable->Width >= wrapedge))) {
      if (esctable->Height > line.height) {
         line.height = esctable->Height;
         line.full_height = font->Ascent;
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
   j = check_wordwrap("Table", Index, AbsX, Width, Index, esctable->X, esctable->Y, esctable->Width, esctable->Height);
   Self->Clips.insert(Self->Clips.end(), saved_clips.begin(), saved_clips.end());

   if (j IS WRAP_EXTENDPAGE) {
      DLAYOUT("Table wrapped - expanding page width due to table size/position.");                  
      return TE_EXTEND_PAGE;
   }
   else if (j IS WRAP_WRAPPED) {
      // A repass is necessary as everything in the table will need to be rearranged
      DLAYOUT("Table wrapped - rearrangement necessary.");
      return TE_WRAP_TABLE;
   }

   //DLAYOUT("new table pos: %dx%d", esctable->X, esctable->Y);

   // The table sets a clipping region in order to state its placement (the surrounds of a table are
   // effectively treated as a graphical object, since it's not text).

   //if (clip.Left IS left_margin) clip.Left = 0; // Extending the clipping to the left doesn't hurt
                             
   Self->Clips.emplace_back(
      ClipRectangle(esctable->X, esctable->Y, clip.Left + esctable->Width, clip.Top + esctable->Height),
      Index, false, "Table");   

   cursor_x = esctable->X + esctable->Width;
   cursor_y = esctable->Y;

   DLAYOUT("Final Table Size: %dx%d,%dx%d", esctable->X, esctable->Y, esctable->Width, esctable->Height);

   esctable = esctable->Stack;

   setsegment = true;
   return 0;
}

//********************************************************************************************************************

void layout::injectObject(INDEX Index, LONG Offset, LONG AbsX, LONG AbsY, LONG Width, LONG PageHeight)
{
   ClipRectangle cell;
   OBJECTID object_id;
   
   // Tell the object our CursorX and CursorY positions so that it can position itself within the stream
   // layout.  The object will tell us its clipping boundary when it returns (if it has a clipping boundary).
   
   auto &escobj = escape_data<::escObject>(Self, Index);
   if (!(object_id = escobj.ObjectID)) return;
   if (!escobj.Graphical) return; // Do not bother with objects that do not draw anything
   if (escobj.Owned) return; // Do not manipulate objects that have owners
   
   // cell: Reflects the page/cell coordinates and width/height of the page/cell.

//wrap_object:
   cell.Left   = AbsX;
   cell.Top    = AbsY;
   cell.Right  = cell.Left + Width;
   if ((!Offset) and (PageHeight < Self->AreaHeight)) {
      cell.Bottom = AbsY + Self->AreaHeight; // The reported page height cannot be shorter than the document's surface area
   }
   else cell.Bottom = AbsY + PageHeight;
   
   if (line.height) {
      if (cell.Bottom < cursor_y + line.height) cell.Bottom = AbsY + line.height;
   }
   else if (cell.Bottom < cursor_y + 1) cell.Bottom = cursor_y + 1;

/*
   LONG width_check = 0;
   LONG dimensions = 0;
   LONG layoutflags = 0;
   if (!(error = AccessObject(object_id, 5000, &object))) {
      DLAYOUT("[Idx:%d] The %s's available page area is %d-%d,%d-%d, margins %dx%d,%d, cursor %dx%d", i, object->Class->ClassName, cell.Left, cell.Right, cell.Top, cell.Bottom, l.left_margin-AbsX, l.right_margin, TopMargin, l.cursor_x, l.cursor_y);
   
      LONG cellwidth, cellheight, align, leftmargin, lineheight, zone_height;
      OBJECTID layout_surface_id;
   
      if ((FindField(object, FID_LayoutSurface, NULL)) and (!object->get(FID_LayoutSurface, &layout_surface_id))) {
         objSurface *surface;
         LONG new_x, new_y, new_width, new_height, calc_x;
   
         // This layout method is used for objects that do not have a Layout object for graphics management and
         // simply rely on a Surface object instead.
   
         if (!(error = AccessObject(layout_surface_id, 3000, &surface))) {
            leftmargin    = l.left_margin - AbsX;
            lineheight    = (l.line.full_height) ? l.line.full_height : l.font->Ascent;
   
            cellwidth  = cell.Right - cell.Left;
            cellheight = cell.Bottom - cell.Top;
            align = l.font->Align | surface->Align;
   
            // Relative dimensions can use the full size of the page/cell only when text-wrapping is disabled.
   
            zone_height = lineheight;
            cell.Left += leftmargin;
            cellwidth = cellwidth - l.right_margin - leftmargin; // Remove margins from the cellwidth because we're only interested in the space available to the object
            new_x = l.cursor_x;
   
            // WIDTH
   
            if (surface->Dimensions & DMF_RELATIVE_WIDTH) {
               new_width = (DOUBLE)cellwidth * (DOUBLE)surface->WidthPercent * 0.01;
               if (new_width < 1) new_width = 1;
               else if (new_width > cellwidth) new_width = cellwidth;
            }
            else if (surface->Dimensions & DMF_FIXED_WIDTH) new_width = surface->Width;
            else if ((surface->Dimensions & DMF_X) and (surface->Dimensions & DMF_X_OFFSET)) {
               // Calculate X boundary
   
               calc_x = new_x;
   
               if (surface->Dimensions & DMF_FIXED_X);
               else if (surface->Dimensions & DMF_RELATIVE_X) {
                  // Relative X, such as 10% would mean 'NewX must be at least 10% beyond 'cell.left + leftmargin'
                  LONG minx;
                  minx = cell.Left + F2T((DOUBLE)cellwidth * (DOUBLE)surface->XPercent * 0.01);
                  if (minx > calc_x) calc_x = minx;
               }
               else calc_x = l.cursor_x;
   
               // Calculate width
   
               if (surface->Dimensions & DMF_FIXED_X_OFFSET) new_width = cellwidth - surface->XOffset - (calc_x - cell.Left);
               else new_width = cellwidth - (calc_x - cell.Left) - (cellwidth * surface->XOffsetPercent * 0.01);
   
               if (new_width < 1) new_width = 1;
               else if (new_width > cellwidth) new_width = cellwidth;
            }
            else {
               DLAYOUT("No width specified for %s #%d (dimensions $%x), defaulting to 1 pixel.", object->Class->ClassName, object->UID, surface->Dimensions);
               new_width = 1;
            }
   
            // X COORD
   
            if ((align & ALIGN::HORIZONTAL) and (surface->Dimensions & DMF_WIDTH)) {
               new_x = new_x + ((cellwidth - new_width)/2);
            }
            else if ((align & ALIGN::RIGHT) and (surface->Dimensions & DMF_WIDTH)) {
               new_x = (AbsX + *Width) - l.right_margin - new_width;
            }
            else if (surface->Dimensions & DMF_RELATIVE_X) {
               new_x += F2T(surface->XPercent * cellwidth * 0.01);
            }
            else if ((surface->Dimensions & DMF_WIDTH) and (surface->Dimensions & DMF_X_OFFSET)) {
               if (surface->Dimensions & DMF_FIXED_X_OFFSET) {
                  new_x += (cellwidth - new_width - surface->XOffset);
               }
               else new_x += F2T((DOUBLE)cellwidth - (DOUBLE)new_width - ((DOUBLE)cellwidth * (DOUBLE)surface->XOffsetPercent * 0.01));
            }
            else if (surface->Dimensions & DMF_FIXED_X) {
               new_x += surface->X;
            }
   
            // HEIGHT
   
            if (surface->Dimensions & DMF_RELATIVE_HEIGHT) {
               // If the object is inside a paragraph <p> section, the height will be calculated based
               // on the current line height.  Otherwise, the height is calculated based on the cell/page
               // height.
   
               new_height = (DOUBLE)zone_height * (DOUBLE)surface->HeightPercent * 0.01;
               if (new_height > zone_height) new_height = zone_height;
            }
            else if (surface->Dimensions & DMF_FIXED_HEIGHT) {
               new_height = surface->Height;
            }
            else if ((surface->Dimensions & DMF_Y) and
                     (surface->Dimensions & DMF_Y_OFFSET)) {
               if (surface->Dimensions & DMF_FIXED_Y) new_y = surface->Y;
               else if (surface->Dimensions & DMF_RELATIVE_Y) new_y = F2T((DOUBLE)zone_height * (DOUBLE)surface->YPercent * 0.01);
               else new_y = l.cursor_y;
   
               if (surface->Dimensions & DMF_FIXED_Y_OFFSET) new_height = zone_height - surface->YOffset;
               else new_height = zone_height - F2T((DOUBLE)zone_height * (DOUBLE)surface->YOffsetPercent * 0.01);
   
               if (new_height > zone_height) new_height = zone_height;
            }
            else new_height = lineheight;
   
            if (new_height < 1) new_height = 1;
   
            // Y COORD
   
            if (layoutflags & LAYOUT_IGNORE_CURSOR) new_y = cell.Top;
            else new_y = l.cursor_y;
   
            if (surface->Dimensions & DMF_RELATIVE_Y) {
               new_y += F2T(surface->YPercent * lineheight * 0.01);
            }
            else if ((surface->Dimensions & DMF_HEIGHT) and
                     (surface->Dimensions & DMF_Y_OFFSET)) {
               if (surface->Dimensions & DMF_FIXED_Y_OFFSET) new_y = cell.Top + F2T(zone_height - surface->Height - surface->YOffset);
               else new_y += F2T((DOUBLE)zone_height - (DOUBLE)surface->Height - ((DOUBLE)zone_height * (DOUBLE)surface->YOffsetPercent * 0.01));
            }
            else if (surface->Dimensions & DMF_Y_OFFSET) {
               // This section resolves situations where no explicit Y coordinate has been defined,
               // but the Y offset has been defined.  This means we leave the existing Y coordinate as-is and
               // adjust the height.
   
               if (surface->Dimensions & DMF_FIXED_Y_OFFSET) new_height = zone_height - surface->YOffset;
               else new_height = zone_height - F2T((DOUBLE)zone_height * (DOUBLE)surface->YOffsetPercent * 0.01);
   
               if (new_height < 1) new_height = 1;
               if (new_height > zone_height) new_height = zone_height;
            }
            else if (surface->Dimensions & DMF_FIXED_Y) {
               new_y += surface->Y;
            }
   
            // Set the clipping
   
            DLAYOUT("Clip region is being restricted to the bounds: %dx%d,%dx%d", new_x, new_y, new_width, new_height);
   
            cell.Left  = new_x;
            cell.Top   = new_y;
            cell.Right = new_x + new_width;
            cell.Bottom = new_y + new_height;
   
            // If LAYOUT_RIGHT is set, no text may be printed to the right of the object.  This has no impact
            // on the object's bounds.
   
            if (layoutflags & LAYOUT_RIGHT) {
               DLAYOUT("LAYOUT_RIGHT: Expanding clip.right boundary from %d to %d.", cell.Right, AbsX + *Width - l.right_margin);
               cell.Right = (AbsX + *Width) - l.right_margin; //cellwidth;
            }
   
            // If LAYOUT_LEFT is set, no text may be printed to the left of the object (but not
            // including text that has already been printed).  This has no impact on the object's
            // bounds.
   
            if (layoutflags & LAYOUT_LEFT) {
               DLAYOUT("LAYOUT_LEFT: Expanding clip.left boundary from %d to %d.", cell.Left, AbsX);
               cell.Left  = AbsX; //leftmargin;
            }
   
            if (layoutflags & LAYOUT_IGNORE_CURSOR) width_check = cell.Right - AbsX;
            else width_check = cell.Right + l.right_margin;
   
            DLAYOUT("#%d, Pos: %dx%d,%dx%d, Align: $%.8x, From: %dx%d,%dx%d,%dx%d, WidthCheck: %d/%d", object->UID, new_x, new_y, new_width, new_height, align, F2T(surface->X), F2T(surface->Y), F2T(surface->Width), F2T(surface->Height), F2T(surface->XOffset), F2T(surface->YOffset), width_check, *Width);
            DLAYOUT("Clip Size: %dx%d,%dx%d, LineHeight: %d, LayoutFlags: $%.8x", cell.Left, cell.Top, cellwidth, cellheight, lineheight, layoutflags);
   
            dimensions = surface->Dimensions;
            error = ERR_Okay;
   
            acRedimension(surface, new_x, new_y, 0, new_width, new_height, 0);
   
            ReleaseObject(surface);
         }
         else {
            dimensions = 0;
         }
      }
      else if ((FindField(object, FID_Layout, NULL)) and (!object->getPtr(FID_Layout, &layout))) {
         leftmargin = l.left_margin - AbsX;
         lineheight = (l.line.full_height) ? l.line.full_height : l.font->Ascent;
   
         cellwidth  = cell.Right - cell.Left;
         cellheight = cell.Bottom - cell.Top;
         align = l.font->Align | layout->Align;
   
         layoutflags = layout->Layout;
   
         if (layoutflags & (LAYOUT_BACKGROUND|LAYOUT_TILE)) {
            // In background mode, the bounds are adjusted to match the size of the cell
            // if the object supports GraphicWidth and GraphicHeight.  For all other objects,
            // it is assumed that the bounds have been preset.
            //
            // Positioning within the cell bounds is managed by the GraphicX/Y/Width/Height and
            // Align fields.
            //
            // Gaps are automatically worked into the calculated X/Y value.
   
            if ((layout->GraphicWidth) and (layout->GraphicHeight) and (!(layoutflags & LAYOUT_TILE))) {
               layout->BoundX = cell.Left;
               layout->BoundY = cell.Top;
   
               if (align & ALIGN::HORIZONTAL) {
                  layout->GraphicX = cell.Left + layout->LeftMargin + ((cellwidth - layout->GraphicWidth)>>1);
               }
               else if (align & ALIGN::RIGHT) layout->GraphicX = cell.Left + cellwidth - layout->RightMargin - layout->GraphicWidth;
               else if (!layout->PresetX) {
                  if (!layout->PresetWidth) {
                     layout->GraphicX = cell.Left + layout->LeftMargin;
                  }
                  else layout->GraphicX = l.cursor_x + layout->LeftMargin;
               }
   
               if (align & ALIGN::VERTICAL) layout->GraphicY = cell.Top + ((cellheight - layout->TopMargin - layout->BottomMargin - F2T(layout->GraphicHeight))>>1);
               else if (align & ALIGN::BOTTOM) layout->GraphicY = cell.Top + cellheight - layout->BottomMargin - layout->GraphicHeight;
               else if (!layout->PresetY) {
                  if (!layout->PresetHeight) {
                     layout->GraphicY = cell.Top + layout->TopMargin;
                  }
                  else layout->GraphicY = l.cursor_y + layout->TopMargin;
               }
   
               // The object bounds are set to the GraphicWidth/Height.  When the object is drawn,
               // the bounds will be automatically restricted to the size of the cell (or page) so
               // that there is no over-draw.
   
               layout->BoundWidth = layout->GraphicWidth;
               layout->BoundHeight = layout->GraphicHeight;
   
               DLAYOUT("X/Y: %dx%d, W/H: %dx%d (Width/Height are preset)", layout->BoundX, layout->BoundY, layout->BoundWidth, layout->BoundHeight);
            }
            else {
               // If the object does not use preset GraphicWidth and GraphicHeight, then
               // we just want to use the preset X/Y/Width/Height in relation to the available
               // space within the cell.
   
               layout->ParentSurface.Width = cell.Right - cell.Left;
               layout->ParentSurface.Height = cell.Bottom - cell.Top;
   
               object->get(FID_X, &layout->BoundX);
               object->get(FID_Y, &layout->BoundY);
               object->get(FID_Width, &layout->BoundWidth);
               object->get(FID_Height, &layout->BoundHeight);
   
               layout->BoundX += cell.Left;
               layout->BoundY += cell.Top;
   
   
               DLAYOUT("X/Y: %dx%d, W/H: %dx%d, Parent W/H: %dx%d (Width/Height not preset), Dimensions: $%.8x", layout->BoundX, layout->BoundY, layout->BoundWidth, layout->BoundHeight, layout->ParentSurface.Width, layout->ParentSurface.Height, layout->Dimensions);
            }
   
            dimensions = layout->Dimensions;
            error = ERR_NothingDone; // Do not add a clipping region because the graphic is in the background
         }
         else {
            // The object can extend the line's height if the GraphicHeight is larger than the line.
            //
            // Alignment calculations are restricted to this area, which forms the initial Bounds*:
            //
            //   X: CursorX
            //   Y: CursorY
            //   Right: PageWidth - RightMargin
            //   Bottom: LineHeight - GraphicHeight
            //
            // If LAYOUT_IGNORE_CURSOR is set, then the user has set fixed values for both X and Y.
            // The cursor is completely ignored and the existing Bound* fields will be used without alteration.
            // Use of IGNORECURSOR also means that the left, right, top and bottom margins are all ignored.  Text
            // will still be wrapped around the boundaries as long as LAYOUT_BACKGROUND isn't set.
   
            LONG extclip_left, extclip_right;
   
            extclip_left = 0;
            extclip_right = 0;
   
            if (layoutflags & LAYOUT_IGNORE_CURSOR);
            else {
               // In cursor-relative (normal) layout mode, the graphic will be restricted by
               // the margins, so we adjust cell.left and the cellwidth accordingly.
               //
               // The BoundWidth and BoundHeight can be expanded if the GraphicWidth/GraphicHeight exceed the bound values.
               //
               // Relative width/height values are allowed.
               //
               // A relative XOffset is allowed, this will be computed against the cellwidth.
               //
               // Y Coordinates are allowed, these are computed from the top of the line.
               //
               // Relative height and vertical offsets are allowed, these are computed from the lineheight.
               //
               // Vertical alignment is managed within the bounds of the object when
               // it is drawn, so we do not cater for vertical alignment when positioning
               // the object in this code.
   
               cell.Left += leftmargin;
               cellwidth = cellwidth - l.right_margin - leftmargin; // Remove margins from the cellwidth because we're only interested in the space available to the object
            }
   
            // Adjust the bounds to reflect special dimension settings.  The minimum
            // width and height is 1, and the bounds may not exceed the size of the
            // available cell space (because a width of 110% would cause infinite recursion).
   
            if (layoutflags & LAYOUT_IGNORE_CURSOR) layout->BoundX = cell.Left;
            else layout->BoundX = l.cursor_x;
   
            layout->BoundWidth = 1; // Just a default in case no width in the Dimension flags is defined
   
            if ((align & ALIGN::HORIZONTAL) and (layout->GraphicWidth)) {
               // In horizontal mode where a GraphicWidth is preset, we force the BoundX and BoundWidth to their
               // exact settings and override any attempt by the user to have preset the X and Width fields.
               // The object will attempt a horizontal alignment within the bounds, this will be to no effect as the
               // GraphicWidth is equivalent to the BoundWidth.  Text can still appear to the left and right of the object,
               // if the author does not like this then the LAYOUT_LEFT and LAYOUT_RIGHT flags can be used to extend
               // the clipping region.
   
   
               LONG new_x = layout->BoundX + ((cellwidth - (layout->GraphicWidth + layout->LeftMargin + layout->RightMargin))/2);
               if (new_x > layout->BoundX) layout->BoundX = new_x;
               layout->BoundX += layout->LeftMargin;
               layout->BoundWidth = layout->GraphicWidth;
               extclip_left = layout->LeftMargin;
               extclip_right = layout->RightMargin;
            }
            else if ((align & ALIGN::RIGHT) and (layout->GraphicWidth)) {
               LONG new_x = ((AbsX + *Width) - l.right_margin) - (layout->GraphicWidth + layout->RightMargin);
               if (new_x > layout->BoundX) layout->BoundX = new_x;
               layout->BoundWidth = layout->GraphicWidth;
               extclip_left = layout->LeftMargin;
               extclip_right = layout->RightMargin;
            }
            else {
               LONG xoffset;
   
               if (layout->Dimensions & DMF_FIXED_X_OFFSET) xoffset = layout->XOffset;
               else if (layout->Dimensions & DMF_RELATIVE_X_OFFSET) xoffset = (DOUBLE)cellwidth * (DOUBLE)layout->XOffset * 0.01;
               else xoffset = 0;
   
               if (layout->Dimensions & DMF_RELATIVE_X) {
                  LONG new_x = layout->BoundX + layout->LeftMargin + F2T(layout->X * cellwidth * 0.01);
                  if (new_x > layout->BoundX) layout->BoundX = new_x;
                  extclip_left = layout->LeftMargin;
                  extclip_right = layout->RightMargin;
               }
               else if (layout->Dimensions & DMF_FIXED_X) {
                  LONG new_x = layout->BoundX + layout->X + layout->LeftMargin;
                  if (new_x > layout->BoundX) layout->BoundX = new_x;
                  extclip_left = layout->LeftMargin;
                  extclip_right = layout->RightMargin;
               }
   
               // WIDTH
   
               if (layout->Dimensions & DMF_RELATIVE_WIDTH) {
                  layout->BoundWidth = (DOUBLE)(cellwidth - (layout->BoundX - cell.Left)) * (DOUBLE)layout->Width * 0.01;
                  if (layout->BoundWidth < 1) layout->BoundWidth = 1;
                  else if (layout->BoundWidth > cellwidth) layout->BoundWidth = cellwidth;
               }
               else if (layout->Dimensions & DMF_FIXED_WIDTH) layout->BoundWidth = layout->Width;
   
               // GraphicWidth and GraphicHeight settings will expand the width and height
               // bounds automatically unless the Width and Height fields in the Layout have been preset
               // by the user.
               //
               // NOTE: If the object supports GraphicWidth and GraphicHeight, it must keep
               // them up to date if they are based on relative values.
   
               if ((layout->GraphicWidth > 0) and (!(layout->Dimensions & DMF_WIDTH))) {
                  DLAYOUT("Setting BoundWidth from %d to preset GraphicWidth of %d", layout->BoundWidth, layout->GraphicWidth);
                  layout->BoundWidth = layout->GraphicWidth;
               }
               else if ((layout->Dimensions & DMF_X) and (layout->Dimensions & DMF_X_OFFSET)) {
                  if (layout->Dimensions & DMF_FIXED_X_OFFSET) layout->BoundWidth = cellwidth - xoffset - (layout->BoundX - cell.Left);
                  else layout->BoundWidth = cellwidth - (layout->BoundX - cell.Left) - xoffset;
   
                  if (layout->BoundWidth < 1) layout->BoundWidth = 1;
                  else if (layout->BoundWidth > cellwidth) layout->BoundWidth = cellwidth;
               }
               else if ((layout->Dimensions & DMF_WIDTH) and
                        (layout->Dimensions & DMF_X_OFFSET)) {
                  if (layout->Dimensions & DMF_FIXED_X_OFFSET) {
                     LONG new_x = layout->BoundX + cellwidth - layout->BoundWidth - xoffset - layout->RightMargin;
                     if (new_x > layout->BoundX) layout->BoundX = new_x;
                     extclip_left = layout->LeftMargin;
                  }
                  else {
                     LONG new_x = layout->BoundX + F2T((DOUBLE)cellwidth - (DOUBLE)layout->BoundWidth - xoffset);
                     if (new_x > layout->BoundX) layout->BoundX = new_x;
                  }
               }
               else {
                  if ((align & ALIGN::HORIZONTAL) and (layout->Dimensions & DMF_WIDTH)) {
                     LONG new_x = layout->BoundX + ((cellwidth - (layout->BoundWidth + layout->LeftMargin + layout->RightMargin))/2);
                     if (new_x > layout->BoundX) layout->BoundX = new_x;
                     layout->BoundX += layout->LeftMargin;
                     extclip_left = layout->LeftMargin;
                     extclip_right = layout->RightMargin;
                  }
                  else if ((align & ALIGN::RIGHT) and (layout->Dimensions & DMF_WIDTH)) {
                     // Note that it is possible the BoundX may end up behind the cursor, or the cell's left margin.
                     // A check for this is made later, so don't worry about it here.
   
                     LONG new_x = ((AbsX + *Width) - l.right_margin) - (layout->BoundWidth + layout->RightMargin);
                     if (new_x > layout->BoundX) layout->BoundX = new_x;
                     extclip_left = layout->LeftMargin;
                     extclip_right = layout->RightMargin;
                  }
               }
            }
   
            // VERTICAL SUPPORT
   
            LONG obj_y;
   
            if (layoutflags & LAYOUT_IGNORE_CURSOR) layout->BoundY = cell.Top;
            else layout->BoundY = l.cursor_y;
   
            obj_y = 0;
            if (layout->Dimensions & DMF_RELATIVE_Y)   obj_y = F2T((DOUBLE)layout->Y * (DOUBLE)lineheight * 0.01);
            else if (layout->Dimensions & DMF_FIXED_Y) obj_y = layout->Y;
            obj_y += layout->TopMargin;
            layout->BoundY += obj_y;
            layout->BoundHeight = lineheight - obj_y; // This is merely a default
   
            LONG zone_height;
   
            if ((escpara) or (page_height < 1)) zone_height = lineheight;
            else zone_height = page_height;
   
            // HEIGHT
   
            if (layout->Dimensions & DMF_RELATIVE_HEIGHT) {
               // If the object is inside a paragraph <p> section, the height will be calculated based
               // on the current line height.  Otherwise, the height is calculated based on the cell/page
               // height.
   
               layout->BoundHeight = (DOUBLE)(zone_height - obj_y) * (DOUBLE)layout->Height * 0.01;
               if (layout->BoundHeight > zone_height - obj_y) layout->BoundHeight = lineheight - obj_y;
            }
            else if (layout->Dimensions & DMF_FIXED_HEIGHT) {
               layout->BoundHeight = layout->Height;
            }
   
            if ((layout->GraphicHeight > layout->BoundHeight) and (!(layout->Dimensions & DMF_HEIGHT))) {
               DLAYOUT("Expanding BoundHeight from %d to preset GraphicHeight of %d", layout->BoundHeight, layout->GraphicHeight);
               layout->BoundHeight = layout->GraphicHeight;
            }
            else {
               if (layout->BoundHeight < 1) layout->BoundHeight = 1;
   
               // This code deals with vertical offsets
   
               if (layout->Dimensions & DMF_Y_OFFSET) {
                  if (layout->Dimensions & DMF_Y) {
                     if (layout->Dimensions & DMF_FIXED_Y_OFFSET) layout->BoundHeight = zone_height - layout->YOffset;
                     else layout->BoundHeight = zone_height - F2T((DOUBLE)zone_height * (DOUBLE)layout->YOffset * 0.01);
   
                     if (layout->BoundHeight > zone_height) layout->BoundHeight = zone_height;
                  }
                  else if (layout->Dimensions & DMF_HEIGHT) {
                     if (layout->Dimensions & DMF_FIXED_Y_OFFSET) layout->BoundY = cell.Top + F2T(zone_height - layout->Height - layout->YOffset);
                     else layout->BoundY += F2T((DOUBLE)zone_height - (DOUBLE)layout->Height - ((DOUBLE)zone_height * (DOUBLE)layout->YOffset * 0.01));
                  }
               }
            }
   
            if (layoutflags & (LAYOUT_BACKGROUND|LAYOUT_TILE)) {
               error = ERR_NothingDone; // No text wrapping for background and tile layouts
            }
            else {
               // Set the clipping
   
               DLAYOUT("Clip region is being restricted to the bounds: %dx%d,%dx%d", layout->BoundX, layout->BoundY, layout->BoundWidth, layout->BoundHeight);
   
               cell.Left  = layout->BoundX - extclip_left;
               cell.Top   = layout->BoundY - layout->TopMargin;
               cell.Right = layout->BoundX + layout->BoundWidth + extclip_right;
               cell.Bottom = layout->BoundY + layout->BoundHeight + layout->BottomMargin;
   
               // If LAYOUT_RIGHT is set, no text may be printed to the right of the object.  This has no impact
               // on the object's bounds.
   
               if (layoutflags & LAYOUT_RIGHT) {
                  DLAYOUT("LAYOUT_RIGHT: Expanding clip.right boundary from %d to %d.", cell.Right, AbsX + *Width - l.right_margin);
                  LONG new_right = (AbsX + *Width) - l.right_margin; //cellwidth;
                  if (new_right > cell.Right) cell.Right = new_right;
               }
   
               // If LAYOUT_LEFT is set, no text may be printed to the left of the object (but not
               // including text that has already been printed).  This has no impact on the object's
               // bounds.
   
               if (layoutflags & LAYOUT_LEFT) {
                  DLAYOUT("LAYOUT_LEFT: Expanding clip.left boundary from %d to %d.", cell.Left, AbsX);
   
                  if (layoutflags & LAYOUT_IGNORE_CURSOR) cell.Left = AbsX;
                  else cell.Left  = l.left_margin;
               }
   
               if (layoutflags & LAYOUT_IGNORE_CURSOR) width_check = cell.Right - AbsX;
               else width_check = cell.Right + l.right_margin;
   
               DLAYOUT("#%d, Pos: %dx%d,%dx%d, Align: $%.8x, From: %dx%d,%dx%d,%dx%d, WidthCheck: %d/%d", object->UID, layout->BoundX, layout->BoundY, layout->BoundWidth, layout->BoundHeight, align, F2T(layout->X), F2T(layout->Y), F2T(layout->Width), F2T(layout->Height), F2T(layout->XOffset), F2T(layout->YOffset), width_check, *Width);
               DLAYOUT("Clip Size: %dx%d,%dx%d, LineHeight: %d, GfxSize: %dx%d, LayoutFlags: $%.8x", cell.Left, cell.Top, cellwidth, cellheight, lineheight, layout->GraphicWidth, layout->GraphicHeight, layoutflags);
   
               dimensions = layout->Dimensions;
               error = ERR_Okay;
            }
         }
      }
      else error = ERR_NoSupport;
   
      ReleaseObject(object);
   }
   else {
      if (error IS ERR_DoesNotExist) escobj->ObjectID = 0;
   }

   if ((!error) and (width_check)) {
      // The cursor must advance past the clipping region so that the segment positions will be
      // correct when set.
   
      checkwrap = true;
   
      // Check if the clipping region is invalid.  Invalid clipping regions are not added to the clip
      // region list (i.e. layout of document text will ignore the presence of the object).
   
      if ((cell.Bottom <= cell.Top) or (cell.Right <= cell.Left)) {
         CSTRING name;
         if ((name = object->Name)) log.warning("%s object %s returned an invalid clip region of %dx%d,%dx%d", object->Class->ClassName, name, cell.Left, cell.Top, cell.Right, cell.Bottom);
         else log.warning("%s object #%d returned an invalid clip region of %dx%d,%dx%d", object->Class->ClassName, object->UID, cell.Left, cell.Top, cell.Right, cell.Bottom);
         break;
      }
   
      // If the right-side of the object extends past the page width, increase the width.
   
      LONG left_check;
   
      if (layoutflags & LAYOUT_IGNORE_CURSOR) left_check = AbsX;
      else if (layoutflags & LAYOUT_LEFT) left_check = l.left_margin;
      else left_check = l.left_margin; //l.cursor_x;
   
      if (*Width >= WIDTH_LIMIT);
      else if ((cell.Left < left_check) or (layoutflags & LAYOUT_IGNORE_CURSOR)) {
         // The object is < left-hand side of the page/cell, this means
         // that we may have to force a page/cell width increase.
         //
         // Note: Objects with IGNORECURSOR are always checked here, because they aren't subject
         // to wrapping due to the X/Y being fixed.  Such objects are limited to width increases only.
   
         LONG cmp_width;
   
         if (layoutflags & LAYOUT_IGNORE_CURSOR) cmp_width = AbsX + (cell.Right - cell.Left);
         else cmp_width = l.left_margin + (cell.Right - cell.Left) + l.right_margin;
   
         if (*Width < cmp_width) {
            DLAYOUT("Restarting as %s clip.left %d < %d and extends past the page width (%d > %d).", object->Class->ClassName, cell.Left, left_check, width_check, *Width);
            *Width = cmp_width;
            goto extend_page;
         }
      }
      else if (width_check > *Width) {
         // Perform a wrapping check if the object possibly extends past the width of the page/cell.
   
         DLAYOUT("Wrapping %s object #%d as it extends past the page width (%d > %d).  Pos: %dx%d", object->Class->ClassName, object->UID, width_check, *Width, cell.Left, cell.Top);
   
         j = check_wordwrap("Object", Self, i, AbsX, Width, i, cell.Left, cell.Top, cell.Right - cell.Left, cell.Bottom - cell.Top);
   
         if (j IS WRAP_EXTENDPAGE) {
            DLAYOUT("Expanding page width due to object size.");
            goto extend_page;
         }
         else if (j IS WRAP_WRAPPED) {
            DLAYOUT("Object coordinates wrapped to %dx%d", cell.Left, cell.Top);
            // The check_wordwrap() function will have reset l.cursor_x and l.cursor_y, so
            // on our repass, the cell.left and cell.top will reflect this new cursor position.
   
            goto wrap_object;
         }
      }
   
      DLAYOUT("Adding %s clip to the list: %dx%d,%dx%d", object->Class->ClassName, cell.Left, cell.Top, cell.Right-cell.Left, cell.Bottom-cell.Top);
   
      Self->Clips.emplace_back(cell, i, layoutflags & (LAYOUT_BACKGROUND|LAYOUT_FOREGROUND) ? true : false, "Object");
   
      if (!(layoutflags & (LAYOUT_BACKGROUND|LAYOUT_FOREGROUND))) {
         if (cell.Bottom > l.cursor_y) {
            objheight = cell.Bottom - l.cursor_y;
            if ((l.anchor) or (escobj->Embedded)) {
               // If all objects in the current section need to be anchored to the text, each
               // object becomes part of the current line (e.g. treat the object as if it were
               // a text character).  This requires us to adjust the line height.
   
               if (objheight > l.line.height) {
                  l.line.height = objheight;
                  l.line.full_height = l.font->Ascent;
               }
            }
            else {
               // If anchoring is not set, the height of the object will still define the height
               // of the line, but cannot exceed the height of the font for that line.
   
               if (objheight < l.font->LineSpacing) {
                  l.line.height = objheight;
                  l.line.full_height = objheight;
               }
            }
         }
   
         //if (cell.Right > l.cursor_x) l.wordwidth += cell.Right - l.cursor_x;
   
         if (escpara) {
            j = cell.Bottom - escpara->Y;
            if (j > escpara->Height) escpara->Height = j;
         }
      }
   }
   else if ((error != ERR_NothingDone) and (error != ERR_NoAction)) {
      DLAYOUT("Error code #%d during object layout: %s", error, GetErrorMsg(error));
   }
   
   l.setsegment = true;
   
   // If the object uses a relative height or vertical offset, a repass will be required if the page height
   // increases.
   
   if ((dimensions & (DMF_RELATIVE_HEIGHT|DMF_FIXED_Y_OFFSET|DMF_RELATIVE_Y_OFFSET)) and (layoutflags & (LAYOUT_BACKGROUND|LAYOUT_IGNORE_CURSOR))) {
      DLAYOUT("Vertical repass may be required.");
      object_vertical_repass = true;
   }
*/
}

//********************************************************************************************************************

void layout::injectSetMargins(INDEX Index, LONG AbsY, LONG &BottomMargin)
{
   auto &escmargins = escape_data<::escSetMargins>(Self, Index);

   if (escmargins.Left != 0x7fff) {
      cursor_x     += escmargins.Left;
      line.x      += escmargins.Left;
      left_margin += escmargins.Left;
   }

   if (escmargins.Right != 0x7fff) {
      right_margin += escmargins.Right;
      alignwidth -= escmargins.Right;
      wrapedge   -= escmargins.Right;
   }

   if (escmargins.Top != 0x7fff) {
      if (cursor_y < AbsY + escmargins.Top) cursor_y = AbsY + escmargins.Top;
   }

   if (escmargins.Bottom != 0x7fff) {
      BottomMargin += escmargins.Bottom;
      if (BottomMargin < 0) BottomMargin = 0;
   }
}

//********************************************************************************************************************
// This function lays out the document so that it is ready to be drawn.  It calculates the position, pixel length and
// height of each line and rearranges any objects that are present in the document.

static void layout_doc(extDocument *Self)
{
   pf::Log log(__FUNCTION__);
   objFont *font;
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

   if (!(font = lookup_font(0, "layout_doc"))) { // There is no content loaded for display      
      return;
   }

   layout_section(Self, 0, &font, 0, 0, &page_width, &page_height, 
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

void layout::add_drawsegment(LONG Offset, LONG Stop, LONG Y, LONG Width, LONG AlignWidth, const std::string &Debug)
{
   pf::Log log(__FUNCTION__);
   LONG i;

   // Determine trailing whitespace at the end of the line.  This helps
   // to prevent situations such as underlining occurring in whitespace
   // at the end of the line during word-wrapping.

   auto trimstop = Stop;
   while ((Self->Stream[trimstop-1] <= 0x20) and (trimstop > Offset)) {
      if (Self->Stream[trimstop-1] IS CTRL_CODE) break;
      trimstop--;
   }

   if (Offset >= Stop) {
      DLAYOUT("Cancelling addition, no content in line to add (bytes %d-%d) \"%.20s\" (%s)", Offset, Stop, printable(Self, Offset).c_str(), Debug.c_str());
      return;
   }

   // Check the new segment to see if there are any text characters or escape codes relevant to drawing

   bool text_content    = false;
   bool control_content = false;
   bool object_content  = false;
   bool allow_merge     = true;
   for (i=Offset; i < Stop;) {
      if (Self->Stream[i] IS CTRL_CODE) {
         auto code = ESCAPE_CODE(Self->Stream, i);
         control_content = true;
         if (code IS ESC::OBJECT) object_content = true;
         if ((code IS ESC::OBJECT) or (code IS ESC::TABLE_START) or (code IS ESC::TABLE_END) or (code IS ESC::FONT)) {
            allow_merge = false;
         }
      }
      else {
         text_content = true;
         allow_merge = false;
      }

      NEXT_CHAR(Self->Stream, i);
   }

   auto Height   = line.height;
   auto BaseLine = line.full_height;
   if (text_content) {
      if (Height <= 0) {
         // No line-height given and there is text content - use the most recent font to determine the line height
         Height   = font->LineSpacing;
         BaseLine = font->Ascent;
      }
      else if (!BaseLine) { // If base-line is missing for some reason, define it
         BaseLine = font->Ascent;
      }
   }
   else {
      if (Height <= 0) Height = 0;
      if (BaseLine <= 0) BaseLine = 0;
   }

#ifdef DBG_STREAM
   DLAYOUT("#%d, Bytes: %d-%d, Area: %dx%d,%d:%dx%d, WordWidth: %d, CursorY: %d, [%.20s]...[%.20s] (%s)",
      LONG(Self->Segments.size()), Offset, Stop, line.x, Y, Width, AlignWidth, Height, wordwidth,
      cursor_y, printable(Self, Offset, Stop-Offset).c_str(), printable(Self, Stop).c_str(), Debug.c_str());
#endif

   DocSegment segment;
   auto x = line.x;

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
         Height = segment.Height;
         BaseLine = segment.BaseLine;
      }
   }

#ifdef _DEBUG
   // If this is a segmented line, check if any previous entries have greater
   // heights.  If so, this is considered an internal programming error.

   if ((split_start != NOTSPLIT) and (Height > 0)) {
      for (i=split_start; i < Offset; i++) {
         if (Self->Segments[i].Depth != Self->Depth) continue;
         if (Self->Segments[i].Height > Height) {
            log.warning("A previous entry in segment %d has a height larger than the new one (%d > %d)", i, Self->Segments[i].Height, Height);
            BaseLine = Self->Segments[i].BaseLine;
            Height = Self->Segments[i].Height;
         }
      }
   }
#endif

   segment.Index    = Offset;
   segment.Stop     = Stop;
   segment.TrimStop = trimstop;
   segment.X        = x;
   segment.Y        = Y;
   segment.Height   = Height;
   segment.BaseLine = BaseLine;
   segment.Width    = Width;
   segment.Depth    = Self->Depth;
   segment.AlignWidth     = AlignWidth;
   segment.TextContent    = text_content;
   segment.ControlContent = control_content;
   segment.ObjectContent  = object_content;
   segment.AllowMerge     = allow_merge;
   segment.Edit           = Self->EditMode;

   // If a line is segmented, we need to backtrack for earlier line segments and ensure that their height and full_height
   // is matched to that of the last line (which always contains the maximum height and full_height values).

   if ((split_start != NOTSPLIT) and (Height)) {
      if (LONG(Self->Segments.size()) != split_start) {
         DLAYOUT("Resetting height (%d) & base (%d) of segments index %d-%d.", Height, BaseLine, segment.Index, split_start);
         for (unsigned i=split_start; i < Self->Segments.size(); i++) {
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

   LAYOUT_STATE(extDocument *pSelf, LONG pIndex, layout &pLayout) : Layout(pSelf) {
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

static LONG layout_section(extDocument *Self, INDEX Offset, objFont **Font,
   LONG AbsX, LONG AbsY, LONG *Width, LONG *Height, ClipRectangle Margins, bool *VerticalRepass)
{
   pf::Log log(__FUNCTION__);
   layout l(Self);
   escFont    *style;
   escAdvance *advance;
   escObject  *escobj;
   escList    *esclist;
   escCell    *esccell;
   escLink    *esclink;
   escRow     *escrow, *lastrow;
   DocEdit    *edit;
   escTable   *esctable;
   escParagraph *escpara;
   LAYOUT_STATE tablestate(Self), rowstate(Self), liststate(Self);
   LONG start_ecindex, unicode, j, lastheight, lastwidth, edit_segment;
   INDEX i;
   bool checkwrap;

   if ((Self->Stream.empty()) or (!Self->Stream[Offset]) or (!Font)) {
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
      l = s.Layout; 
      i = s.Index;
   };

   auto start_links    = Self->Links.size();
   auto start_segments = Self->Segments.size();
   l.start_clips       = Self->Clips.size();
   start_ecindex       = Self->EditCells.size();
   LONG page_height    = *Height;
   bool object_vertical_repass = false;

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
   Self->Clips.resize(l.start_clips);

   lastrow         = NULL; // For table management
   lastwidth       = *Width;
   lastheight      = page_height;
   esclist         = NULL;
   escrow          = NULL;
   esctable        = NULL;
   escpara         = NULL;
   esclink         = NULL;
   edit            = NULL;
   esccell         = NULL;
   style           = NULL;
   edit_segment    = 0;
   checkwrap       = false;  // true if a wordwrap or collision check is required

   l.anchor        = false;  // true if in an anchored section (objects are anchored to the line)
   l.alignflags    = 0;      // Current alignment settings according to the font style
   l.paragraph_y   = 0;
   l.paragraph_end = 0;
   l.line.increase = 0;
   l.left_margin   = AbsX + Margins.Left;
   l.right_margin  = Margins.Right;   // Retain the right margin in an adjustable variable, in case we adjust the margin
   l.wrapedge      = AbsX + *Width - l.right_margin;
   l.alignwidth    = l.wrapedge;
   l.cursor_x      = AbsX + Margins.Left;  // The absolute position of the cursor
   l.cursor_y      = AbsY + Margins.Top;
   l.wordwidth     = 0;         // The pixel width of the current word.  Zero if no word is being worked on
   l.wordindex     = -1;        // A byte index in the stream, for the word currently being operated on
   l.line.index    = Offset;    // The starting index of the line we are operating on
   l.line.x        = AbsX + Margins.Left;
   l.line.height   = 0;
   l.line.full_height = 0;
   l.kernchar      = 0;      // Previous character of the word being operated on
   l.link.x        = 0;
   l.link.index    = 0;
   l.split_start   = Self->Segments.size();  // Set to the previous line index if line is segmented.  Used for ensuring that all distinct entries on the line use the same line height
   l.font          = *Font;
   l.nowrap        = false; // true if word wrapping is to be turned off
   l.link.open     = false;
   l.setsegment    = false;
   l.textcontent   = false;
   l.spacewidth    = fntCharWidth(l.font, ' ', 0, NULL);

   i = Offset;

   while (true) {
      // For certain graphics-related escape codes, set the line segment up to the encountered escape code if the text
      // string will be affected (e.g. if the string will be broken up due to a clipping region etc).

      if (Self->Stream[i] IS CTRL_CODE) {
         // Any escape code that sets l.setsegment to true in its case routine, must set break_segment to true now so
         // that any textual content can be handled immediately.
         //
         // This is done particular for escape codes that can be treated as wordwrap breaks.

         if (l.line.index < i) {
            BYTE break_segment = 0;
            switch (ESCAPE_CODE(Self->Stream, i)) {
               case ESC::ADVANCE:
               case ESC::TABLE_START:
                  break_segment = 1;
                  break;

               case ESC::FONT:
                  if (l.textcontent) {
                     style = &escape_data<escFont>(Self, i);
                     objFont *font = lookup_font(style->Index, "ESC::FONT");
                     if (l.font != font) break_segment = 1;
                  }
                  break;

               case ESC::OBJECT:
                  escobj = &escape_data<escObject>(Self, i);
                  if (escobj->Graphical) break_segment = 1;
                  break;

               case ESC::INDEX_START: {
                  auto index = &escape_data<escIndex>(Self, i);
                  if (!index->Visible) break_segment = 1;
                  break;
               }

               default: break;
            }

            if (break_segment) {
               DLAYOUT("Setting line at escape '%s', index %d, line.x: %d, wordwidth: %d", ESCAPE_NAME(Self->Stream,i).c_str(), l.line.index, l.line.x, l.wordwidth);
                  l.cursor_x += l.wordwidth;
                  l.add_drawsegment(l.line.index, i, l.cursor_y, l.cursor_x - l.line.x, l.alignwidth - l.line.x, "Esc:Object");
                  l.reset_segment(i, l.cursor_x);
                  l.alignwidth = l.wrapedge;               
            }
         }
      }

      // Wordwrap checking.  Any escape code that results in a word-break for the current word will initiate a wrapping
      // check.  Encountering whitespace also results in a wrapping check.

      if (esctable) {
         l.alignwidth = l.wrapedge;
      }
      else {
         if (Self->Stream[i] IS CTRL_CODE) {
            switch (ESCAPE_CODE(Self->Stream, i)) {
               // These escape codes cause wrapping because they can break up words

               case ESC::PARAGRAPH_START:
               case ESC::PARAGRAPH_END:
               case ESC::TABLE_END:
               case ESC::OBJECT:
               case ESC::ADVANCE:
               case ESC::LINK_END:
                  checkwrap = true;
                  break;

               default:
                  l.alignwidth = l.wrapedge;
                  break;
            }
         }
         else if (Self->Stream[i] > 0x20) {
            // Non-whitespace characters do not result in a wordwrap check
            l.alignwidth = l.wrapedge;
         }
         else checkwrap = true;

         if (checkwrap) {
            checkwrap = false;
            auto wrap_result = l.check_wordwrap("Text", i, AbsX, Width, l.wordindex, l.cursor_x, l.cursor_y, (l.wordwidth < 1) ? 1 : l.wordwidth, (l.line.height < 1) ? 1 : l.line.height);

            if (wrap_result IS WRAP_EXTENDPAGE) {
               DLAYOUT("Expanding page width on wordwrap request.");
               goto extend_page;
            }
            else if ((Self->Stream[i] IS '\n') and (wrap_result IS WRAP_WRAPPED)) {
               // The presence of the line-break must be ignored, due to word-wrap having already made the new line for us
               i++;
               l.line.index = i;
               continue;
            }
         }
      }

      // Break the loop if there are no more characters to process

      if (i >= LONG(Self->Stream.size())) break;

      if (Self->Stream[i] IS CTRL_CODE) { // Escape code encountered.
#ifdef DBG_LAYOUT_ESCAPE
         DLAYOUT("ESC_%s Indexes: %d-%d-%d, WordWidth: %d", 
            ESCAPE_NAME(Self->Stream, i).c_str(), l.line.index, i, l.wordindex, l.wordwidth);
#endif
         l.setsegment = false; // Escape codes that draw something in draw_document() (e.g. object, table) should set this flag to true in their case statement
         switch (ESCAPE_CODE(Self->Stream, i)) {
            case ESC::ADVANCE:
               advance = &escape_data<escAdvance>(Self, i);
               l.cursor_x += advance->X;
               l.cursor_y += advance->Y;
               if (advance->X) l.reset_segment(i, l.cursor_x);               
               break;

            case ESC::FONT:
               style = &escape_data<escFont>(Self, i);
               l.font = lookup_font(style->Index, "ESC::FONT");

               if (l.font) {
                  if ((style->Options & FSO::ALIGN_RIGHT) != FSO::NIL) l.font->Align = ALIGN::RIGHT;
                  else if ((style->Options & FSO::ALIGN_CENTER) != FSO::NIL) l.font->Align = ALIGN::HORIZONTAL;
                  else l.font->Align = ALIGN::NIL;

                  if ((style->Options & FSO::ANCHOR) != FSO::NIL) l.anchor = true;
                  else l.anchor = false;

                  if ((style->Options & FSO::NO_WRAP) != FSO::NIL) {
                     l.nowrap = true;
                     //wrapedge = 1000;
                  }
                  else l.nowrap = false;

                  DLAYOUT("Font Index: %d, LineSpacing: %d, Height: %d, Ascent: %d, Cursor: %dx%d", style->Index, l.font->LineSpacing, l.font->Height, l.font->Ascent, l.cursor_x, l.cursor_y);
                  l.spacewidth = fntCharWidth(l.font, ' ', 0, 0);

                  // Treat the font as if it is a text character by setting the wordindex.  This ensures it is included in the drawing process

                  if (!l.wordwidth) l.wordindex = i;
               }
               else DLAYOUT("ESC_FONT: Unable to lookup font using style index %d.", style->Index);

               break;

            case ESC::INDEX_START: 
               l.injectIndexStart(i);
               break;

            case ESC::SET_MARGINS: 
               l.injectSetMargins(i, AbsY, Margins.Bottom); 
               break;            

            // LINK MANAGEMENT
             case ESC::LINK: 
               l.injectLink(i); 
               break;

            case ESC::LINK_END: 
               l.injectLinkEnd(i); 
               break;

            // LIST MANAGEMENT

            case ESC::LIST_START:
               // This is the start of a list.  Each item in the list will be identified by ESC::PARAGRAPH codes.  The
               // cursor position is advanced by the size of the item graphics element.

               liststate = LAYOUT_STATE(Self, i, l);

               if (esclist) {
                  auto ptr = esclist;
                  esclist = &escape_data<escList>(Self, i);
                  esclist->Stack = ptr;
               }
               else {
                  esclist = &escape_data<escList>(Self, i);
                  esclist->Stack = NULL;
               }
list_repass:
               esclist->Repass = false;
               break;

            case ESC::LIST_END:
               // If it is a custom list, a repass is required

               if ((esclist) and (esclist->Type IS escList::CUSTOM) and (esclist->Repass)) {
                  DLAYOUT("Repass for list required, commencing...");
                  RESTORE_STATE(liststate);
                  goto list_repass;
               }

               if (esclist) esclist = esclist->Stack;               

               // At the end of a list, increase the whitespace to that of a standard paragraph.

               if (!esclist) {
                  if (escpara) l.end_line(NL_PARAGRAPH, i, escpara->VSpacing, i, "Esc:ListEnd");
                  else l.end_line(NL_PARAGRAPH, i, 1.0, i, "Esc:ListEnd");
               }

               break;

            // EMBEDDED OBJECT MANAGEMENT

            case ESC::OBJECT: 
               l.injectObject(i, Offset, AbsX, AbsY, *Width, page_height); 
               break;

            case ESC::TABLE_START: {
               // Table layout steps are as follows:
               //
               // 1. Copy prefixed/default widths and heights to all cells in the table.
               // 2. Calculate the size of each cell with respect to its content.  This can
               //    be left-to-right or top-to-bottom, it makes no difference.
               // 3. During the cell-layout process, keep track of the maximum width/height
               //    for the relevant row/column.  If either increases, make a second pass
               //    so that relevant cells are resized correctly.
               // 4. If the width of the rows is less than the requested table width (e.g.
               //    table width = 100%) then expand the cells to meet the requested width.
               // 5. Restart the page layout using the correct width and height settings
               //    for the cells.

               tablestate = LAYOUT_STATE(Self, i, l);

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
                  width = ((*Width - (l.cursor_x - AbsX) - l.right_margin) * esctable->MinWidth) / 100;
               }
               else width = esctable->MinWidth;

               if (width < 0) width = 0;

               {
                  LONG min = (esctable->Thickness * 2) + (esctable->CellHSpacing * (esctable->Columns.size()-1)) + (esctable->CellPadding * 2 * esctable->Columns.size());
                  if (esctable->Thin) min -= esctable->CellHSpacing * 2; // Thin tables do not have spacing on the left and right borders
                  if (width < min) width = min;
               }

               if (width > WIDTH_LIMIT - l.cursor_x - l.right_margin) {
                  log.traceWarning("Table width in excess of allowable limits.");
                  width = WIDTH_LIMIT - l.cursor_x - l.right_margin;
                  if (Self->BreakLoop > 4) Self->BreakLoop = 4;
               }

               if (esctable->ComputeColumns) {
                  if (esctable->Width >= width) esctable->ComputeColumns = 0;
               }

               esctable->Width = width;

wrap_table_end:
wrap_table_cell:
               esctable->CursorX    = l.cursor_x;
               esctable->CursorY    = l.cursor_y;
               esctable->X          = l.cursor_x;
               esctable->Y          = l.cursor_y;
               esctable->RowIndex   = 0;
               esctable->TotalClips = Self->Clips.size();
               esctable->Height     = esctable->Thickness;

               DLAYOUT("(i%d) Laying out table of %dx%d, coords %dx%d,%dx%d%s, page width %d.", i, LONG(esctable->Columns.size()), esctable->Rows, esctable->X, esctable->Y, esctable->Width, esctable->MinHeight, esctable->HeightPercent ? "%" : "", *Width);
               // NB: LOGRETURN() is matched in ESC::TABLE_END

               if (esctable->ComputeColumns) {
                  // Compute the default column widths

                  esctable->ComputeColumns = 0;
                  esctable->CellsExpanded = false;

                  if (!esctable->Columns.empty()) {
                     for (unsigned j=0; j < esctable->Columns.size(); j++) {
                        //if (esctable->ComputeColumns IS 1) {
                        //   esctable->Columns[j].Width = 0;
                        //   esctable->Columns[j].MinWidth = 0;
                        //}

                        if (esctable->Columns[j].PresetWidth & 0x8000) { // Percentage width value
                           esctable->Columns[j].Width = (DOUBLE)((esctable->Columns[j].PresetWidth & 0x7fff) * esctable->Width) * 0.01;
                        }
                        else if (esctable->Columns[j].PresetWidth) { // Fixed width value
                           esctable->Columns[j].Width = esctable->Columns[j].PresetWidth;
                        }
                        else esctable->Columns[j].Width = 0;                        

                        if (esctable->Columns[j].MinWidth > esctable->Columns[j].Width) esctable->Columns[j].Width = esctable->Columns[j].MinWidth;
                     }
                  }
                  else {
                     log.warning("No columns array defined for table.");
                     esctable->Columns.clear();
                  }
               }

               DLAYOUT("Checking for table collisions before layout (%dx%d).  ResetRowHeight: %d", esctable->X, esctable->Y, esctable->ResetRowHeight);

               j = l.check_wordwrap("Table", i, AbsX, Width, i, esctable->X, esctable->Y, (esctable->Width < 1) ? 1 : esctable->Width, esctable->Height);
               if (j IS WRAP_EXTENDPAGE) {
                  DLAYOUT("Expanding page width due to table size.");                  
                  goto extend_page;
               }
               else if (j IS WRAP_WRAPPED) {
                  // The width of the table and positioning information needs
                  // to be recalculated in the event of a table wrap.

                  DLAYOUT("Restarting table calculation due to page wrap to position %dx%d.", l.cursor_x, l.cursor_y);
                  esctable->ComputeColumns = 1;                  
                  goto wrap_table_start;
               }
               l.cursor_x = esctable->X;
               l.cursor_y = esctable->Y;

               l.setsegment = true;

               l.cursor_y += esctable->Thickness + esctable->CellVSpacing;
               lastrow = NULL;

               break;
            }

            case ESC::TABLE_END: {
               auto action = l.injectTableEnd(i, esctable, lastrow, escpara, Offset, AbsX, Margins.Top, Margins.Bottom, Height, Width);
               if (action) {
                  RESTORE_STATE(tablestate);
                  if (action IS TE_WRAP_TABLE) goto wrap_table_end;
                  else if (action IS TE_REPASS_ROW_HEIGHT) {
                     escrow = lastrow;
                     goto repass_row_height_ext;
                  }
                  else if (action IS TE_EXTEND_PAGE) goto extend_page;
               }
               break;
            }

            case ESC::ROW:
               if (escrow) {
                  auto ptr = escrow;
                  escrow = &escape_data<escRow>(Self, i);
                  escrow->Stack = ptr;
               }
               else {
                  escrow = &escape_data<escRow>(Self, i);
                  escrow->Stack = NULL;
               }

               rowstate = LAYOUT_STATE(Self, i, l);

               if (esctable->ResetRowHeight) escrow->RowHeight = escrow->MinHeight;

repass_row_height_ext:
               escrow->VerticalRepass = false;
               escrow->Y = l.cursor_y;
               esctable->RowWidth = (esctable->Thickness<<1) + esctable->CellHSpacing;

               l.setsegment = true;
               break;

            case ESC::ROW_END:
               esctable->RowIndex++;

                  // Increase the table height if the row extends beyond it

                  j = escrow->Y + escrow->RowHeight + esctable->CellVSpacing;
                  if (j > esctable->Y + esctable->Height) {
                     esctable->Height = j - esctable->Y;
                  }

                  // Advance the cursor by the height of this row

                  l.cursor_y += escrow->RowHeight + esctable->CellVSpacing;
                  l.cursor_x = esctable->X;
                  DLAYOUT("Row ends, advancing down by %d+%d, new height: %d, y-cursor: %d",
                     escrow->RowHeight, esctable->CellVSpacing, esctable->Height, l.cursor_y);

               if (esctable->RowWidth > esctable->Width) esctable->Width = esctable->RowWidth;

               lastrow = escrow;
               escrow  = escrow->Stack;
               l.setsegment = true;
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

               l.add_drawsegment(i, i+ESCAPE_LEN, l.cursor_y, 0, 0, "Esc:Cell");

               // Set the AbsX location of the cell.  AbsX determines the true location of the cell for layout_section()

               esccell->AbsX = l.cursor_x;
               esccell->AbsY = l.cursor_y;

               if (esctable->Thin) {
                  //if (esccell->Column IS 0);
                  //else esccell->AbsX += esctable->CellHSpacing;
               }
               else {
                  esccell->AbsX += esctable->CellHSpacing;
               }

               if (esccell->Column IS 0) esccell->AbsX += esctable->Thickness;

               esccell->Width  = esctable->Columns[esccell->Column].Width; // Minimum width for the cell's column
               esccell->Height = escrow->RowHeight;
               //DLAYOUT("%d / %d", escrow->MinHeight, escrow->RowHeight);

               DLAYOUT("Index %d, Processing cell at %dx %dy, size %dx%d, column %d", i, l.cursor_x, l.cursor_y, esccell->Width, esccell->Height, esccell->Column);

               // Find the matching CELL_END

               LONG cell_end = i;
               while (Self->Stream[cell_end]) {
                  if (Self->Stream[cell_end] IS CTRL_CODE) {
                     if (ESCAPE_CODE(Self->Stream, cell_end) IS ESC::CELL_END) {
                        auto &end = escape_data<escCellEnd>(Self, cell_end);
                        if (end.CellID IS esccell->CellID) break;
                     }
                  }

                  NEXT_CHAR(Self->Stream, cell_end);
               }

               if (Self->Stream[cell_end] IS 0) {
                  log.warning("Failed to find matching cell-end.  Document stream is corrupt.");
                  goto exit;
               }

               i += ESCAPE_LEN; // Go to start of cell content

               if (i < cell_end) {
                  LONG segcount = Self->Segments.size();
                  auto savechar = Self->Stream[cell_end];
                  Self->Stream[cell_end] = 0;

                  if (!esccell->EditDef.empty()) Self->EditMode = true;
                  else Self->EditMode = false;

                     i = layout_section(Self, i, &l.font,
                            esccell->AbsX, esccell->AbsY,
                            &esccell->Width, &esccell->Height,
                            ClipRectangle(esctable->CellPadding), &vertical_repass);

                  if (!esccell->EditDef.empty()) Self->EditMode = false;

                  Self->Stream[cell_end] = savechar;

                  if (!esccell->EditDef.empty()) {
                     // Edit cells have a minimum width/height so that the user can still interact with them when empty.

                     if (LONG(Self->Segments.size()) IS segcount) {
                        // No content segments were created, which means that there's nothing for the cursor to attach
                        // itself too.

                        //do we really want to do something here?
                        //I'd suggest that we instead break up the segments a bit more???  ANother possibility - create an ESC::NULL
                        //type that gets placed at the start of the edit cell.  If there's no genuine content, then we at least have the ESC::NULL
                        //type for the cursor to be attached to?  ESC::NULL does absolutely nothing except act as faux content.


// TODO Work on this next




                     }

                     if (esccell->Width < 16) esccell->Width = 16;
                     if (esccell->Height < l.font->LineSpacing) {
                        esccell->Height = l.font->LineSpacing;
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
                  RESTORE_STATE(tablestate);                  
                  goto wrap_table_cell;
               }

               // Advance the width of the entire row and adjust the row height

               esctable->RowWidth += esctable->Columns[esccell->Column].Width;

               if (!esctable->Thin) esctable->RowWidth += esctable->CellHSpacing;
               else if ((esccell->Column + esccell->ColSpan) < LONG(esctable->Columns.size())-1) esctable->RowWidth += esctable->CellHSpacing;

               if ((esccell->Height > escrow->RowHeight) or (escrow->VerticalRepass)) {
                  // A repass will be required if the row height has increased
                  // and objects or tables have been used in earlier cells, because
                  // objects need to know the final dimensions of their table cell.

                  if (esccell->Column IS LONG(esctable->Columns.size())-1) {
                     DLAYOUT("Extending row height from %d to %d (row repass required)", escrow->RowHeight, esccell->Height);
                  }

                  escrow->RowHeight = esccell->Height;
                  if ((esccell->Column + esccell->ColSpan) >= LONG(esctable->Columns.size())) {
                     RESTORE_STATE(rowstate);
                     goto repass_row_height_ext;
                  }
                  else escrow->VerticalRepass = true; // Make a note to do a vertical repass once all columns on this row have been processed
               }

               l.cursor_x += esctable->Columns[esccell->Column].Width;

               if (!esctable->Thin) l.cursor_x += esctable->CellHSpacing;
               else if ((esccell->Column + esccell->ColSpan) < LONG(esctable->Columns.size())) l.cursor_x += esctable->CellHSpacing;

               if (esccell->Column IS 0) l.cursor_x += esctable->Thickness;               
               break;
            }

            case ESC::CELL_END: {
               // CELL_END helps draw_document(), so set the segment to ensure that it is
               // included in the draw stream.  Please refer to ESC::CELL to see how content is
               // processed and how the cell dimensions are formed.

               l.setsegment = true;

               if ((esccell) and (!esccell->OnClick.empty())) {
                  add_link(Self, ESC::CELL, esccell, esccell->AbsX, esccell->AbsY, esccell->Width, esccell->Height, "esc_cell_end");
               }

               if ((esccell) and (!esccell->EditDef.empty())) {
                  // The area of each edit cell is logged for assisting interaction between
                  // the mouse pointer and the cells.

                  Self->EditCells.emplace_back(esccell->CellID, esccell->AbsX, esccell->AbsY, esccell->Width, esccell->Height);
               }

               break;
            }

            case ESC::PARAGRAPH_START: 
               escpara = l.injectParagraphStart(i, escpara, esclist, *Width); 
               break;

            case ESC::PARAGRAPH_END: 
               escpara = l.injectParagraphEnd(i, escpara); 
               break;

            default: break;
         }

         if (l.setsegment) {
            // Notice that this version of our call to add_drawsegment() does not define content position information (i.e. X/Y coordinates)
            // because we only expect to add an escape code to the drawing sequence, with the intention that the escape code carries
            // information relevant to the drawing process.  It is vital therefore that all content has been set with an earlier call
            // to add_drawsegment() before processing of the escape code.  See earlier in this routine.

            l.add_drawsegment(i, i+ESCAPE_LEN, l.cursor_y, 0, 0, ESCAPE_NAME(Self->Stream, i)); //"Esc:SetSegment");
            l.reset_segment(i+ESCAPE_LEN, l.cursor_x);
         }

         i += ESCAPE_LEN;
      }
      else {
         // If the font character is larger or equal to the current line height, extend
         // the height for the current line.  Note that we go for >= because we want to
         // correct the base line in case there is an object already set on the line that
         // matches the font's line spacing.

         if (l.font->LineSpacing >= l.line.height) {
            l.line.height = l.font->LineSpacing;
            l.line.full_height = l.font->Ascent;
         }

         if (Self->Stream[i] IS '\n') {
#if 0
            // This link code is likely going to be needed for a case such as :
            //   <a href="">blah blah <br/> blah </a>
            // But we haven't tested it in a rpl document yet.

            if ((l.link) and (l.link_open IS false)) {
               // A link is due to be closed
               add_link(Self, ESC::LINK, l.link, l.link_x, l.cursor_y, l.cursor_x + l.wordwidth - l.link_x, l.line.height, "<br/>");
               l.link = NULL;
            }
#endif
            l.end_line(NL_PARAGRAPH, i+1 /* index */, 0 /* spacing */, i+1 /* restart-index */, "CarriageReturn");
           i++;
         }
         else if (Self->Stream[i] <= 0x20) {
            if (Self->Stream[i] IS '\t') {
               LONG tabwidth = (l.spacewidth + l.font->GlyphSpacing) * l.font->TabSize;
               if (tabwidth) l.cursor_x += pf::roundup(l.cursor_x, tabwidth);
               i++;
            }
            else {
               l.cursor_x += l.wordwidth + l.spacewidth;
               i++;
            }

            l.kernchar  = 0;
            l.wordwidth = 0;
            l.textcontent = true;
         }
         else {
            LONG kerning;

            if (!l.wordwidth) l.wordindex = i;   // Record the index of the new word (if this is one)

            i += getutf8(Self->Stream.c_str()+i, &unicode);
            l.wordwidth += fntCharWidth(l.font, unicode, l.kernchar, &kerning);
            l.wordwidth += kerning;
            l.kernchar = unicode;
            l.textcontent = true;
         }
      }
   } // while(1)

   // Check if the cursor + any remaining text requires closure

   if ((l.cursor_x + l.wordwidth > l.left_margin) or (l.wordindex != -1)) {
      l.end_line(NL_NONE, i, 0, i, "SectionEnd");
   }

exit:

   page_height = calc_page_height(Self, l.start_clips, AbsY, Margins.Bottom);

   // Force a second pass if the page height has increased and there are objects
   // on the page (the objects may need to know the page height - e.g. if there
   // is a gradient filling the background).
   //
   // This feature is also handled in ESC::CELL, so we only perform it here
   // if processing is occurring within the root page area (Offset of 0).

   if ((!Offset) and (object_vertical_repass) and (lastheight < page_height)) {
      DLAYOUT("============================================================");
      DLAYOUT("SECOND PASS [%d]: Root page height increased from %d to %d", Offset, lastheight, page_height);
      goto extend_page;
   }

   *Font = l.font;
   if (page_height > *Height) *Height = page_height;

   *VerticalRepass = object_vertical_repass;

   Self->Depth--;   
   return i;
}

//********************************************************************************************************************
// Note that this function also controls the drawing of objects that have loaded into the document (see the
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
   escObject *escobject;
   RGB8 link_save_rgb;
   UBYTE tabfocus, oob, cursor_drawn;

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
   cursor_drawn = false;

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

      // Don't process segments that are out of bounds.  This can't be applied to objects, as they can draw anywhere.

      oob = false;
      if (!segment.ObjectContent) {
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

      if ((Self->ActiveEditDef) and (Self->CursorState) and (!cursor_drawn)) {
         if ((Self->CursorIndex >= segment.Index) and (Self->CursorIndex <= segment.Stop)) {
            if ((Self->CursorIndex IS segment.Stop) and (Self->Stream[Self->CursorIndex-1] IS '\n')); // The -1 looks naughty, but it works as CTRL_CODE != \n, so use of PREV_CHAR() is unnecessary
            else if ((Self->Page->Flags & VF::HAS_FOCUS) != VF::NIL) { // Standard text cursor
               gfxDrawRectangle(Bitmap, segment.X + Self->CursorCharX, segment.Y, 2, segment.BaseLine,
                  Bitmap->packPixel(255, 0, 0), BAF::FILL);
               cursor_drawn = true;               
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
               case ESC::OBJECT: {
                  OBJECTPTR object;

                  escobject = &escape_data<escObject>(Self, i);

                  if ((escobject->Graphical) and (!escobject->Owned)) {
                     if (escobject->ObjectID < 0) {
                        object = NULL;
                        AccessObject(escobject->ObjectID, 3000, &object);
                     }
                     else object = GetObjectPtr(escobject->ObjectID);
*
                     if (object) {
                        objLayout *layout;

                        if ((FindField(object, FID_Layout, NULL)) and (!object->getPtr(FID_Layout, &layout))) {
                           if (layout->DrawCallback.Type) {
                              // If the graphic is within a cell, ensure that the graphic does not exceed
                              // the dimensions of the cell.

                              if (Self->CurrentCell) {
                                 if (layout->BoundX + layout->BoundWidth > Self->CurrentCell->AbsX + Self->CurrentCell->Width) {
                                    layout->BoundWidth  = Self->CurrentCell->AbsX + Self->CurrentCell->Width - layout->BoundX;
                                 }

                                 if (layout->BoundY + layout->BoundHeight > Self->CurrentCell->AbsY + Self->CurrentCell->Height) {
                                    layout->BoundHeight = Self->CurrentCell->AbsY + Self->CurrentCell->Height - layout->BoundY;
                                 }
                              }

                              auto opacity = Bitmap->Opacity;
                              Bitmap->Opacity = 255;
                              auto routine = (void (*)(OBJECTPTR, rkSurface *, objBitmap *))layout->DrawCallback.StdC.Routine;
                              routine(object, Surface, Bitmap);
                              Bitmap->Opacity = opacity;
                           }
                        }

                        if (escobject->ObjectID < 0) ReleaseObject(object);
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

   if ((!line.height) and (wordwidth)) {
      // If this is a one-word line, the line height will not have been defined yet
      //log.trace("Line Height being set to font (currently %d/%d)", line.height, line.full_height);
      line.height = font->LineSpacing;
      line.full_height = font->Ascent;
   }

   DLAYOUT("%s: CursorY: %d, ParaY: %d, ParaEnd: %d, Line Height: %d * %.2f, Index: %d/%d, Restart: %d", 
      Caller.c_str(), cursor_y, paragraph_y, paragraph_end, line.height, Spacing, line.index, Index, RestartIndex);

   for (unsigned i=start_clips; i < Self->Clips.size(); i++) {
      if (Self->Clips[i].Transparent) continue;
      if ((cursor_y + line.height >= Self->Clips[i].Clip.Top) and (cursor_y < Self->Clips[i].Clip.Bottom)) {
         if (cursor_x + wordwidth < Self->Clips[i].Clip.Left) {
            if (Self->Clips[i].Clip.Left < alignwidth) alignwidth = Self->Clips[i].Clip.Left;
         }
      }
   }

   if (Index > line.index) {
      add_drawsegment(line.index, Index, cursor_y, cursor_x + wordwidth - line.x, alignwidth - line.x, "Esc:EndLine");
   }

   // Determine the new vertical position of the cursor.  This routine takes into account multiple line-breaks, so that
   // the overall amount of whitespace is no more than the biggest line-break specified in
   // a line-break sequence.

   if (NewLine) {
      auto bottom_line = cursor_y + line.height;
      if (paragraph_end > bottom_line) bottom_line = paragraph_end;

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

                  bottom_line = paragraph_y;
                  break;
               }
               else if ((ESCAPE_CODE(Self->Stream, i) IS ESC::OBJECT) or (ESCAPE_CODE(Self->Stream, i) IS ESC::TABLE_END)) break; // Content encountered

               PREV_CHAR(Self->Stream, i);
            }
            else break; // Content encountered
         }
      }

      paragraph_y = bottom_line;

      // Paragraph gap measured as default line height * spacing ratio

      auto new_y = bottom_line + F2I(Self->LineHeight * Spacing);
      if (new_y > cursor_y) cursor_y = new_y;
   }

   // Reset line management variables for a new line starting from the left margin.

   cursor_x      = left_margin;
   line.x        = left_margin;
   line.height   = 0;
   line.full_height = 0;
   split_start   = Self->Segments.size();
   line.index    = RestartIndex;
   wordindex     = line.index;
   kernchar      = 0;
   wordwidth     = 0;
   paragraph_end = 0;   
}

//********************************************************************************************************************
// Word-wrapping is checked whenever whitespace is encountered or certain escape codes are found in the text stream,
// e.g. paragraphs and objects will mark an end to the current word.
//
// Wrapping is always checked even if there is no 'active word' because we need to be able to wrap empty lines (e.g.
// solo <br/> tags).
//
// Index - The current index value.
// ObjectIndex - The index that indicates the start of the word.

UBYTE layout::check_wordwrap(const std::string &Type, INDEX Index, LONG X, LONG *Width,
   LONG ObjectIndex, LONG &GraphicX, LONG &GraphicY, LONG GraphicWidth, LONG GraphicHeight)
{
   pf::Log log(__FUNCTION__);

   if (!Self->BreakLoop) return WRAP_DONOTHING;

   // If the width of the object is larger than the available page width, extend the size of the page.

/*
   if (GraphicWidth > *Width - left_margin - right_margin) {
      *Width = GraphicWidth + left_margin + right_margin;
      return WRAP_EXTENDPAGE;
   }
*/

   // This code pushes the object along to the next available space when a boundary is encountered on the current line.

#ifdef DBG_WORDWRAP
   log.branch("Index: %d/%d, %s: %dx%d,%dx%d, LineHeight: %d, Cursor: %dx%d, PageWidth: %d, Edge: %d", 
      Index, ObjectIndex, Type.c_str(), GraphicX, GraphicY, GraphicWidth, GraphicHeight, line.height, cursor_x, cursor_y, *Width, wrapedge);
#endif

   UBYTE result = WRAP_DONOTHING;
   LONG breakloop = MAXLOOP;

restart:
   alignwidth = wrapedge;

   if (!Self->Clips.empty()) check_clips(Index, ObjectIndex, GraphicX, GraphicY, GraphicWidth, GraphicHeight);

   if (GraphicX + GraphicWidth > wrapedge) {
      if ((*Width < WIDTH_LIMIT) and ((GraphicX IS left_margin) or (nowrap))) {
         // Force an extension of the page width and recalculate from scratch
         auto minwidth = GraphicX + GraphicWidth + right_margin - X;
         if (minwidth > *Width) {
            *Width = minwidth;
            WRAP("Forcing an extension of the page width to %d", minwidth);
         }
         else *Width += 1;
         return WRAP_EXTENDPAGE;
      }
      else {
         if (!line.height) {
            line.height = 1; //font->LineSpacing;
            line.full_height = 1; //font->Ascent;
         }

         if (current_link) {
            if (link.x IS GraphicX) {
               // If the link starts with the object, the link itself is going to be wrapped with it
            }
            else {
               add_link(Self, ESC::LINK, current_link, link.x, GraphicY, GraphicX - link.x, line.height, "check_wrap");
            }
         }

         // Set the line segment up to the object index.  The line.index is
         // updated so that this process only occurs in the first iteration.

         if (line.index < ObjectIndex) {
            add_drawsegment(line.index, ObjectIndex, GraphicY, GraphicX - line.x, alignwidth - line.x, "DoWrap");
            line.index = ObjectIndex;
         }

         // Reset the line management variables so that the next line starts at the left margin.

         GraphicX     = left_margin;
         GraphicY    += line.height;
         cursor_x     = GraphicX;
         cursor_y     = GraphicY;
         split_start  = Self->Segments.size();
         line.x       = left_margin;
         link.x       = left_margin; // Only matters if a link is defined
         kernchar     = 0;
         line.full_height = 0;
         line.height  = 0;

         result = WRAP_WRAPPED;
         if (--breakloop > 0) goto restart; // Go back and check the clip boundaries again
         else {
            log.traceWarning("Breaking out of continuous loop.");
            Self->Error = ERR_Loop;
         }
      }
   }

   // No wrap has occurred

   if ((current_link) and (!link.open)) {
      // A link is due to be closed
      add_link(Self, ESC::LINK, current_link, link.x, GraphicY, GraphicX + GraphicWidth - link.x, line.height ? line.height : font->LineSpacing, "check_wrap");
      current_link = NULL;
   }

   #ifdef DBG_WORDWRAP
      if (result IS WRAP_WRAPPED) WRAP("A wrap to Y coordinate %d has occurred.", cursor_y);
   #endif

   return result;
}

void layout::check_clips(INDEX Index, LONG ObjectIndex, LONG &GraphicX, LONG &GraphicY, LONG GraphicWidth, LONG GraphicHeight)
{
   pf::Log log(__FUNCTION__);
   LONG i;
   bool reset_link;

#ifdef DBG_WORDWRAP
   log.branch("Index: %d-%d, ObjectIndex: %d, Graphic: %dx%d,%dx%d, TotalClips: %d", 
      line.index, Index, ObjectIndex, GraphicX, GraphicY, GraphicWidth, GraphicHeight, LONG(Self->Clips.size()));
#endif

   for (auto clip=start_clips; clip < LONG(Self->Clips.size()); clip++) {
      if (Self->Clips[clip].Transparent) continue;
      if (GraphicY + GraphicHeight < Self->Clips[clip].Clip.Top) continue;
      if (GraphicY >= Self->Clips[clip].Clip.Bottom) continue;
      if (GraphicX >= Self->Clips[clip].Clip.Right) continue;
      if (GraphicX + GraphicWidth < Self->Clips[clip].Clip.Left) continue;

      if (Self->Clips[clip].Clip.Left < alignwidth) alignwidth = Self->Clips[clip].Clip.Left;

      WRAP("Word: \"%.20s\" (%dx%d,%dx%d) advances over clip %d-%d",
         printable(Self, ObjectIndex).c_str(), GraphicX, GraphicY, GraphicWidth, GraphicHeight,
         Self->Clips[clip].Clip.Left, Self->Clips[clip].Clip.Right);

      // Set the line segment up to the encountered boundary and continue checking the object position against the
      // clipping boundaries.

      if ((current_link) and (Self->Clips[clip].Index < link.index)) {
         // An open link intersects with a clipping region that was created prior to the opening of the link.  We do
         // not want to include this object as a clickable part of the link - we will wrap over or around it, so
         // set a partial link now and ensure the link is reopened after the clipping region.

         WRAP("Setting hyperlink now to cross a clipping boundary.");

         auto height = line.height ? line.height : font->LineSpacing;
         add_link(Self, ESC::LINK, current_link, link.x, GraphicY, GraphicX + GraphicWidth - link.x, height, "clip_intersect");

         reset_link = true;
      }
      else reset_link = false;

      // Advance the object position.  We break if a wordwrap is required - the code outside of this loop will detect
      // the need for a wordwrap and then restart the wordwrapping process.

      if (GraphicX IS line.x) line.x = Self->Clips[clip].Clip.Right;
      GraphicX = Self->Clips[clip].Clip.Right; // Push the object over the clip boundary

      if (GraphicX + GraphicWidth > wrapedge) {
         WRAP("Wrapping-Break: X(%d)+Width(%d) > Edge(%d) at clip '%s' %dx%d,%dx%d", 
            GraphicX, GraphicWidth, wrapedge, Self->Clips[clip].Name.c_str(), Self->Clips[clip].Clip.Left, Self->Clips[clip].Clip.Top, Self->Clips[clip].Clip.Right, Self->Clips[clip].Clip.Bottom);
         break;
      }

      if ((GraphicWidth) and (ObjectIndex >= 0)) i = ObjectIndex;
      else i = Index;

      if (line.index < i) {
         if (!line.height) {
            add_drawsegment(line.index, i, GraphicY, GraphicX - line.x, GraphicX - line.x, "Wrap:EmptyLine");
         }
         else add_drawsegment(line.index, i, GraphicY, GraphicX + GraphicWidth - line.x, alignwidth - line.x, "Wrap");
      }

      WRAP("Line index reset to %d, previously %d", i, line.index);

      line.index = i;
      line.x = GraphicX;
      if ((reset_link) and (current_link)) link.x = GraphicX;

      clip = start_clips-1; // Check all the clips from the beginning
   }
}
