
//********************************************************************************************************************
// If the layout needs to be recalculated, set the UpdatingLayout field before calling this function.

static void redraw(extDocument *Self, bool Focus)
{
   pf::Log log(__FUNCTION__);

   log.traceBranch("");

   {
      pf::LogLevel level(3);
      layout_doc(Self); // Does nothing if UpdatingLayout is false
   }

   Self->Viewport->draw();

   if ((Focus) and (Self->FocusIndex != -1)) set_focus(Self, -1, "redraw()");
}

//********************************************************************************************************************
// Convert the layout information to a vector scene.  This is the final step in the layout process.

void layout::gen_scene_graph()
{
   pf::Log log(__FUNCTION__);
   //bc_vector *escvector;

   if (Self->UpdatingLayout) return; // Drawing is disabled if the layout is being updated

   if (glFonts.empty()) {
      log.traceWarning("No default font defined.");
      return;
   }

   auto font = glFonts[0].font;

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

   std::stack<bc_list *> stack_list;
   std::stack<bc_row *> stack_row;
   std::stack<bc_paragraph *> stack_para;
   std::stack<bc_table *> stack_table;
   std::string link_save_rgb;
   bool tabfocus = false;
   bool m_cursor_drawn = false;

   #ifdef GUIDELINES
      // Special clip regions are marked in grey
/*
      for (unsigned i=0; i < m_clips.size(); i++) {
         gfxDrawRectangle(Bitmap, Self->Clips[i].Clip.left, Self->Clips[i].Clip.top,
            Self->Clips[i].Clip.right - Self->Clips[i].Clip.left, Self->Clips[i].Clip.bottom - Self->Clips[i].Clip.top,
            Bitmap->packPixel(255, 200, 200), 0);
      }
*/
   #endif

   stream_char select_start, select_end;
   LONG select_startx, select_endx;

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

   FloatRect clip(0, 0, Self->VPWidth, Self->VPHeight);

   std::string font_fill = "rgb(0,0,0,255)";
   auto font_align = ALIGN::NIL;
   //DOUBLE alpha = 1.0;
   for (SEGINDEX seg=0; seg < SEGINDEX(m_segments.size()); seg++) {
      auto &segment = m_segments[seg];

      // Don't process segments that are out of bounds.  Be mindful of floating vectors as they can be placed at any coordinate.

      bool oob = false;
      if (!segment.floating_vectors) {
         if (segment.area.Y >= clip.Height) oob = true;
         else if (segment.area.Y + segment.area.Height < clip.Y) oob = true;
         else if (segment.area.X + segment.area.Width < clip.X) oob = true;
         else if (segment.area.X >= clip.Width) oob = true;
      }

      // Highlighting of selected text
      /*
      if ((select_start <= segment.stop) and (select_end > segment.start)) {
         if (select_start != select_end) {
            alpha = 80.0/255.0;
            if ((select_start > segment.start) and (select_start < segment.stop)) {
               if (select_end < segment.stop) {
                  gfxDrawRectangle(Bitmap, segment.x + select_startx, segment.y,
                     select_endx - select_startx, segment.height, Bitmap->packPixel(0, 128, 0), BAF::FILL);
               }
               else {
                  gfxDrawRectangle(Bitmap, segment.x + select_startx, segment.y,
                     segment.width - select_startx, segment.height, Bitmap->packPixel(0, 128, 0), BAF::FILL);
               }
            }
            else if (select_end < segment.stop) {
               gfxDrawRectangle(Bitmap, segment.x, segment.y, select_endx, segment.height,
                  Bitmap->packPixel(0, 128, 0), BAF::FILL);
            }
            else {
               gfxDrawRectangle(Bitmap, segment.x, segment.y, segment.width, segment.height,
                  Bitmap->packPixel(0, 128, 0), BAF::FILL);
            }
            alpha = 1.0;
         }
      }
      */
      if ((Self->ActiveEditDef) and (Self->CursorState) and (!m_cursor_drawn)) {
         if ((Self->CursorIndex >= segment.start) and (Self->CursorIndex <= segment.stop)) {
            if ((Self->CursorIndex IS segment.stop) and
                (Self->CursorIndex.get_prev_char_or_inline(Self, Self->Stream) IS '\n'));
            else if ((Self->Page->Flags & VF::HAS_FOCUS) != VF::NIL) { // Standard text cursor
               auto rect = objVectorRectangle::create::global({
                  fl::Owner(Self->Page->UID),
                  fl::X(segment.area.X + Self->CursorCharX), fl::Y(segment.area.Y),
                  fl::Width(2), fl::Height(segment.area.Height - segment.gutter),
                  fl::Fill("rgb(255,0,0,255)") });
               Self->LayoutResources.push_back(rect);
               m_cursor_drawn = true;
            }
         }
      }

      #ifdef GUIDELINES_CONTENT
         if (segment.text_content) {
            gfxDrawRectangle(Bitmap, segment.x, segment.y,
               (segment.width > 0) ? segment.width : 5, segment.height,
               Bitmap->packPixel(0, 255, 0), 0);
         }
      #endif

      auto fx = segment.area.X;
      for (auto cursor = segment.start; cursor < segment.stop; cursor.nextCode()) {
         switch (Self->Stream[cursor.index].code) {
            case SCODE::FONT: {
               auto &style = stream_data<bc_font>(Self, cursor);
               if (auto new_font = style.get_font()) {
                  if (tabfocus IS false) font_fill = style.fill;
                  else font_fill = Self->LinkSelectFill;

                  if ((style.options & FSO::ALIGN_RIGHT) != FSO::NIL) font_align = ALIGN::RIGHT;
                  else if ((style.options & FSO::ALIGN_CENTER) != FSO::NIL) font_align = ALIGN::HORIZONTAL;
                  else font_align = ALIGN::NIL;

                  if (style.valign != ALIGN::NIL) {
                     if ((style.valign & ALIGN::TOP) != ALIGN::NIL) font_align |= ALIGN::TOP;
                     else if ((style.valign & ALIGN::VERTICAL) != ALIGN::NIL) font_align |= ALIGN::VERTICAL;
                     else font_align |= ALIGN::BOTTOM;
                  }

                  if ((style.options & FSO::UNDERLINE) != FSO::NIL) new_font->Underline = new_font->Colour;
                  else new_font->Underline.Alpha = 0;

                  font = new_font;
               }
               break;
            }

            case SCODE::LIST_START:
               stack_list.push(&stream_data<bc_list>(Self, cursor));
               break;

            case SCODE::LIST_END:
               stack_list.pop();
               break;

            case SCODE::PARAGRAPH_START:
               stack_para.push(&stream_data<bc_paragraph>(Self, cursor));

               if ((!stack_list.empty()) and (stack_para.top()->list_item)) {
                  // Handling for paragraphs that form part of a list

                  if ((stack_list.top()->type IS bc_list::CUSTOM) or
                      (stack_list.top()->type IS bc_list::ORDERED)) {
                     if (!stack_para.top()->value.empty()) {
                        DOUBLE ix = segment.area.X - stack_para.top()->item_indent;
                        DOUBLE iy = segment.area.Y + segment.area.Height - segment.gutter;

                        auto text = objVectorText::create::global({
                           fl::Owner(Self->Page->UID),
                           fl::X(ix), fl::Y(iy),
                           fl::String(stack_para.top()->value),
                           fl::Font(font),
                           fl::Fill(stack_list.top()->fill)
                           //fl::AlignWidth(segment.AlignWidth),
                        });
                        Self->LayoutResources.push_back(text);
                     }
                  }
                  else if (stack_list.top()->type IS bc_list::BULLET) {                     
                     DOUBLE ix = segment.area.X - stack_para.top()->item_indent + (m_font->Height * 0.5);
                     DOUBLE iy = segment.area.Y + (segment.area.Height - segment.gutter) - (m_font->Height * 0.5);

                     auto bullet = objVectorEllipse::create::global({
                        fl::Owner(Self->Page->UID),
                        fl::CenterX(ix), fl::CenterY(iy),
                        fl::Radius(m_font->Height * 0.25),
                        fl::Fill(stack_list.top()->fill)
                     });
                     Self->LayoutResources.push_back(bullet);
                  }
               }
               break;

            case SCODE::PARAGRAPH_END:
               stack_para.pop();
               break;

            case SCODE::TABLE_START: {
               stack_table.push(&stream_data<bc_table>(Self, cursor));
               auto table = stack_table.top();

               //log.trace("Draw Table: %dx%d,%dx%d", esctable->x, esctable->y, esctable->width, esctable->height);

               if ((!table->fill.empty()) or (!table->stroke.empty())) {
                  auto rect = objVectorRectangle::create::global({
                     fl::Owner(Self->Page->UID),
                     fl::X(table->x), fl::Y(table->y),
                     fl::Width(table->width), fl::Height(table->height)
                  });

                  Self->LayoutResources.push_back(rect);

                  if (!table->fill.empty()) {
                     rect->set(FID_Fill, table->fill);
                  }

                  if (!table->stroke.empty()) {
                     rect->set(FID_Stroke, table->stroke);
                     rect->set(FID_StrokeWidth, table->thickness);
                  }
               }
               break;
            }

            case SCODE::TABLE_END:
               stack_table.pop();
               break;

            case SCODE::ROW: {
               stack_row.push(&stream_data<bc_row>(Self, cursor));
               auto row = stack_row.top();
               if (!row->fill.empty()) {
                  auto rect = objVectorRectangle::create::global({
                     fl::Owner(Self->Page->UID),
                     fl::X(stack_table.top()->x), fl::Y(row->y),
                     fl::Width(stack_table.top()->width),
                     fl::Height(row->row_height),
                     fl::Fill(row->fill)
                  });
                  Self->LayoutResources.push_back(rect);
               }
               break;
            }

            case SCODE::ROW_END:
               stack_row.pop();
               break;

            case SCODE::CELL: {
               auto &cell = stream_data<bc_cell>(Self, cursor);

               #ifdef DBG_LAYOUT
                  cell.stroke = "rgb(255,0,0)";
               #endif

               if ((!cell.fill.empty()) or (!cell.stroke.empty())) {
                  auto rect = objVectorRectangle::create::global({
                     fl::Owner(Self->Page->UID),
                     fl::X(cell.abs_x), fl::Y(cell.abs_y),
                     fl::Width(stack_table.top()->columns[cell.column].width),
                     fl::Height(stack_row.top()->row_height)
                  });

                  if (!cell.stroke.empty()) {
                     rect->set(FID_Stroke, cell.stroke);
                     rect->set(FID_StrokeWidth, 1);
                  }

                  if (!cell.fill.empty()) {
                     rect->set(FID_Fill, cell.fill);
                  }

                  Self->LayoutResources.push_back(rect);
               }
               break;
            }

            case SCODE::LINK: {
               auto esclink = &stream_data<bc_link>(Self, cursor);
               if (Self->HasFocus) {
                  // Override the default link colour if the link has the tab key's focus
                  if ((Self->Tabs[Self->FocusIndex].type IS TT_LINK) and 
                      (Self->Tabs[Self->FocusIndex].ref IS esclink->id) and 
                      (Self->Tabs[Self->FocusIndex].active)) {
                     link_save_rgb = font_fill;
                     font_fill = Self->LinkSelectFill;
                     tabfocus = true;
                  }
               }

               break;
            }

            case SCODE::LINK_END:
               if (tabfocus) {
                  font_fill = link_save_rgb;
                  tabfocus = false;
               }
               break;

            case SCODE::IMAGE: {
               auto &img = stream_data<bc_image>(Self, cursor);
               // Apply the rectangle dimensions as defined during layout.  If the image is inline then we utilise
               // fx for managing the horizontal position amongst the text.

               DOUBLE x = (img.floating() ? img.x : fx) + img.final_pad.left;
               DOUBLE y = segment.area.Y + img.final_pad.top;

               acMoveToPoint(img.rect, x, y, 0, MTF::X|MTF::Y);
               acResize(img.rect, img.final_width, img.final_height, 0);

               if (!img.floating()) fx += img.final_width + img.final_pad.left + img.final_pad.right;
               break;
            }

            case SCODE::TEXT: { // cursor = segment.start; cursor < segment.trim_stop; cursor.nextCode()
               if (!oob) {
                  auto &txt = stream_data<bc_text>(Self, cursor);

                  std::string str;
                  if (cursor.index < segment.trim_stop.index) str.append(txt.text, cursor.offset, std::string::npos);
                  else str.append(txt.text, cursor.offset, segment.trim_stop.offset - cursor.offset);

                  DOUBLE y = segment.area.Y;
                  if ((font_align & ALIGN::TOP) != ALIGN::NIL) y += font->Ascent;
                  else if ((font_align & ALIGN::VERTICAL) != ALIGN::NIL) {
                     DOUBLE avail_space = segment.area.Height - segment.gutter;
                     y += avail_space - ((avail_space - font->Ascent) * 0.5);
                  }
                  else y += segment.area.Height - segment.gutter;

                  if (!str.empty()) {
                     auto text = objVectorText::create::global({
                        fl::Owner(Self->Page->UID),
                        fl::X(fx), fl::Y(y),
                        fl::String(str),
                        fl::Font(font),
                        fl::Fill(font_fill)
                        //fl::AlignWidth(segment.AlignWidth),
                     });
                     Self->LayoutResources.push_back(text);

                     DOUBLE twidth;
                     text->get(FID_TextWidth, &twidth);
                     fx += twidth;
                  }
               }
               break;
            }

            default: break;
         }
      }
   } // for loop
}
