
//********************************************************************************************************************
// If the layout needs to be recalculated, set the UpdatingLayout field before calling this function.

static void redraw(extDocument *Self, bool Focus)
{
   pf::Log log(__FUNCTION__);

   log.traceBranch("");

   {
      pf::LogLevel level(3);
      layout_doc(Self);
   }

   Self->Viewport->draw();

   if ((Focus) and (Self->FocusIndex != -1)) set_focus(Self, -1, "redraw()");
}

//********************************************************************************************************************
// Convert the layout information to a vector scene.

void layout::draw()
{
   pf::Log log(__FUNCTION__);
   escVector *escvector;

   if (Self->UpdatingLayout) return; // Drawing is disabled if the layout is being updated

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

   for (auto obj : Self->LayoutResources) {
      FreeResource(obj);
   }
   Self->LayoutResources.clear();

   std::stack<escList *> stack_list;
   std::stack<escRow *> stack_row;
   std::stack<escParagraph *> stack_para;
   std::stack<escTable *> stack_table;
   std::string link_save_rgb;
   bool tabfocus = false;
   bool m_cursor_drawn = false;

   #ifdef GUIDELINES

      // Page boundary is marked in blue
      gfxDrawRectangle(Bitmap, Self->LeftMargin-1, Self->TopMargin-1,
         Self->CalcWidth - Self->RightMargin - Self->LeftMargin + 2, Self->PageHeight - Self->TopMargin - Self->BottomMargin + 2,
         Bitmap->packPixel(0, 0, 255), 0);

      // Special clip regions are marked in grey
      for (unsigned i=0; i < m_clips.size(); i++) {
         gfxDrawRectangle(Bitmap, Self->Clips[i].Clip.Left, Self->Clips[i].Clip.Top,
            Self->Clips[i].Clip.Right - Self->Clips[i].Clip.Left, Self->Clips[i].Clip.Bottom - Self->Clips[i].Clip.Top,
            Bitmap->packPixel(255, 200, 200), 0);
      }
   #endif

   StreamChar select_start;
   StreamChar select_end;
   LONG select_startx;
   LONG select_endx;

   if ((Self->ActiveEditDef) and (!Self->SelectIndex.valid())) {
      select_start  = Self->CursorIndex;
      select_end    = Self->CursorIndex;
      select_startx = Self->CursorCharX;
      select_endx   = Self->CursorCharX;
   }
   else if ((Self->CursorIndex.valid()) and (Self->SelectIndex.valid())) {
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

   ClipRectangle clip(0, 0, Self->VPWidth, Self->VPHeight);

   DOUBLE alpha = 1.0;
   for (SEGINDEX seg=0; seg < SEGINDEX(m_segments.size()); seg++) {
      auto &segment = m_segments[seg];

      // Don't process segments that are out of bounds.  Be mindful of floating vectors as they can be placed at any coordinate.

      bool oob = false;
      if (!segment.FloatingVectors) {
         if (segment.Y >= clip.Bottom) oob = true;
         if (segment.Y + segment.Height < clip.Top) oob = true;
         if (segment.X + segment.Width < clip.Left) oob = true;
         if (segment.X >= clip.Right) oob = true;
      }

      // Highlighting of selected text
      /*
      if ((select_start <= segment.Stop) and (select_end > segment.Start)) {
         if (select_start != select_end) {
            alpha = 80.0/255.0;
            if ((select_start > segment.Start) and (select_start < segment.Stop)) {
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
            alpha = 1.0;
         }
      }
      */
      if ((Self->ActiveEditDef) and (Self->CursorState) and (!m_cursor_drawn)) {
         if ((Self->CursorIndex >= segment.Start) and (Self->CursorIndex <= segment.Stop)) {
            if ((Self->CursorIndex IS segment.Stop) and
                (Self->CursorIndex.getPrevChar(Self, Self->Stream) IS '\n'));
            else if ((Self->Page->Flags & VF::HAS_FOCUS) != VF::NIL) { // Standard text cursor
               auto rect = objVectorRectangle::create::global({
                  fl::Owner(Self->Page->UID),
                  fl::X(segment.X + Self->CursorCharX), fl::Y(segment.Y),
                  fl::Width(2), fl::Height(segment.BaseLine),
                  fl::Fill("rgb(255,0,0,255)") });
               Self->LayoutResources.push_back(rect);
               m_cursor_drawn = true;
            }
         }
      }

      #ifdef GUIDELINES_CONTENT
         if (segment.TextContent) {
            gfxDrawRectangle(Bitmap, segment.X, segment.Y,
               (segment.Width > 0) ? segment.Width : 5, segment.Height,
               Bitmap->packPixel(0, 255, 0), 0);
         }
      #endif

      auto fx = segment.X;
      std::string font_fill = "rgb(0,0,0,255)";
      ALIGN font_align = ALIGN::NIL;
      for (auto i = segment.Start; i < segment.TrimStop; i.nextCode()) {
         switch (ESCAPE_CODE(Self->Stream, i)) {
            case ESC::FONT: {
               auto &style = escape_data<escFont>(Self, i);
               if (auto font = lookup_font(style.Index, "draw_document")) {
                  if (tabfocus IS false) font_fill = style.Fill;
                  else font_fill = Self->LinkSelectFill;

                  if ((style.Options & FSO::ALIGN_RIGHT) != FSO::NIL) font_align = ALIGN::RIGHT;
                  else if ((style.Options & FSO::ALIGN_CENTER) != FSO::NIL) font_align = ALIGN::HORIZONTAL;
                  else font_align = ALIGN::NIL;

                  if ((style.Options & FSO::UNDERLINE) != FSO::NIL) font->Underline = font->Colour;
                  else font->Underline.Alpha = 0;
               }
               break;
            }

            case ESC::LIST_START:
               stack_list.push(&escape_data<escList>(Self, i));
               break;

            case ESC::LIST_END:
               stack_list.pop();
               break;

            case ESC::PARAGRAPH_START:
               stack_para.push(&escape_data<escParagraph>(Self, i));

               if ((!stack_list.empty()) and (stack_para.top()->ListItem)) {
                  // Handling for paragraphs that form part of a list

                  if ((stack_list.top()->Type IS escList::CUSTOM) or
                      (stack_list.top()->Type IS escList::ORDERED)) {
                     if (!stack_para.top()->Value.empty()) {
                        font->X = fx - stack_para.top()->ItemIndent;
                        font->Y = segment.Y + font->Leading + (segment.BaseLine - font->Ascent);
                        font->AlignWidth = segment.AlignWidth;
                        font->setString(stack_para.top()->Value);
                        font->draw();
                     }
                  }
                  else if (stack_list.top()->Type IS escList::BULLET) {
                     static const LONG SIZE_BULLET = 5;
                     // TODO: Requires conversion to vector
                     //gfxDrawEllipse(Bitmap,
                     //   fx - stack_para.top()->ItemIndent, segment.Y + ((segment.BaseLine - SIZE_BULLET)/2),
                     //   SIZE_BULLET, SIZE_BULLET, Bitmap->packPixel(esclist->Colour), true);
                  }
               }
               break;

            case ESC::PARAGRAPH_END:
               stack_para.pop();
               break;

            case ESC::TABLE_START: {
               stack_table.push(&escape_data<escTable>(Self, i));
               auto table = stack_table.top();

               //log.trace("Draw Table: %dx%d,%dx%d", esctable->X, esctable->Y, esctable->Width, esctable->Height);

               if ((!table->Fill.empty()) or (!table->Stroke.empty())) {
                  auto rect = objVectorRectangle::create::global({
                     fl::Owner(Self->Page->UID),
                     fl::X(table->X), fl::Y(table->Y),
                     fl::Width(table->Width), fl::Height(table->Height)
                  });

                  Self->LayoutResources.push_back(rect);

                  if (!table->Fill.empty()) {
                     rect->set(FID_Fill, table->Fill);
                  }

                  if (!table->Stroke.empty()) {
                     rect->set(FID_Stroke, table->Stroke);
                     rect->set(FID_StrokeWidth, table->Thickness);
                  }
               }
               break;
            }

            case ESC::TABLE_END:
               stack_table.pop();
               break;

            case ESC::ROW: {
               stack_row.push(&escape_data<escRow>(Self, i));
               auto row = stack_row.top();
               if (!row->Fill.empty()) {
                  auto rect = objVectorRectangle::create::global({
                     fl::Owner(Self->Page->UID),
                     fl::X(stack_table.top()->X), fl::Y(row->Y),
                     fl::Width(stack_table.top()->Width),
                     fl::Height(row->RowHeight),
                     fl::Fill(row->Fill)
                  });
                  Self->LayoutResources.push_back(rect);
               }
               break;
            }

            case ESC::ROW_END:
               stack_row.pop();
               break;

            case ESC::CELL: {
               auto &cell = escape_data<escCell>(Self, i);

               if ((!cell.Fill.empty()) or (!cell.Stroke.empty())) {
                  auto rect = objVectorRectangle::create::global({
                     fl::Owner(Self->Page->UID),
                     fl::X(cell.AbsX), fl::Y(cell.AbsY),
                     fl::Width(stack_table.top()->Columns[cell.Column].Width),
                     fl::Height(stack_row.top()->RowHeight)
                  });

                  if (!cell.Stroke.empty()) {
                     rect->set(FID_Stroke, cell.Stroke);
                     rect->set(FID_StrokeWidth, 1);
                  }

                  if (!cell.Fill.empty()) {
                     rect->set(FID_Fill, cell.Fill);
                  }

                  Self->LayoutResources.push_back(rect);
               }
               break;
            }

            case ESC::LINK: {
               auto esclink = &escape_data<escLink>(Self, i);
               if (Self->HasFocus) {
                  if ((Self->Tabs[Self->FocusIndex].Type IS TT_LINK) and (Self->Tabs[Self->FocusIndex].Ref IS esclink->ID) and (Self->Tabs[Self->FocusIndex].Active)) {
                     link_save_rgb = font_fill;
                     font_fill = Self->LinkSelectFill;
                     tabfocus = true;
                  }
               }

               break;
            }

            case ESC::LINK_END:
               if (tabfocus) {
                  font_fill = link_save_rgb;
                  tabfocus = false;
               }
               break;

            case ESC::TEXT: {
               auto &et = escape_data<escText>(Self, i);
               if (!oob) {
                  auto text = objVectorText::create::global({
                     fl::Owner(Self->Page->UID),
                     fl::X(fx), fl::Y(segment.Y + font->Leading + (segment.BaseLine - font->Ascent)),
                     //fl::AlignWidth(segment.AlignWidth),
                     fl::String(et.Text),
                     fl::Fill("rgb(255,0,0)")
                  });
                  Self->LayoutResources.push_back(text);
                  fx = font->EndX;
               }
               break;
            }

            default: break;
         }
      }
   } // for loop
}