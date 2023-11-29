
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
// Convert the layout information to a vector scene.  This is the final step in the layout process. The advantage in
// performing this step separately to the layout process is that the graphics resources are managed last, which is
// sensible for keeping them out of the layout loop.

void layout::gen_scene_init(objVectorViewport *Viewport)
{
   pf::vector<ChildEntry> list;
   if (!ListChildren(Viewport->UID, &list)) {
      for (auto it=list.rbegin(); it != list.rend(); it++) FreeResource(it->ObjectID);
   }

   m_cursor_drawn = false;
}

void layout::gen_scene_graph(objVectorViewport *Viewport, RSTREAM &Stream, SEGINDEX Start, SEGINDEX Stop)
{
   pf::Log log(__FUNCTION__);

   if (Self->UpdatingLayout) return; // Drawing is disabled if the layout is being updated

   if (glFonts.empty()) {
      log.traceWarning("No default font defined.");
      return;
   }

   auto font = glFonts[0].font;

   #ifdef _DEBUG
   if (Stream.empty()) {
      log.traceWarning("No content in stream or no segments.");
      return;
   }
   #endif

   std::stack<bc_list *> stack_list;
   std::stack<bc_row *> stack_row;
   std::stack<bc_paragraph *> stack_para;
   std::stack<bc_table *> stack_table;
   std::stack<objVectorViewport *> stack_vp;
   std::string link_save_rgb;
   bool tabfocus = false;

   #ifdef GUIDELINES // Make clip regions visible
      for (unsigned i=0; i < m_clips.size(); i++) {
         auto rect = objVectorRectangle::create::global({
               fl::Owner(Viewport->UID),
               fl::X(m_clips[i].Clip.left), fl::Y(m_clips[i].Clip.top),
               fl::Width(m_clips[i].Clip.right - m_clips[i].Clip.left),
               fl::Height(m_clips[i].Clip.bottom - m_clips[i].Clip.top),
               fl::Fill("rgb(255,200,200,64)") });
      }
   #endif

   stream_char select_start, select_end;
   DOUBLE select_start_x, select_end_x;

   if ((Self->ActiveEditDef) and (!Self->SelectIndex.valid())) {
      select_start   = Self->CursorIndex;
      select_end     = Self->CursorIndex;
      select_start_x = Self->CursorCharX;
      select_end_x   = Self->CursorCharX;
   }
   else if ((Self->CursorIndex.valid()) and (Self->SelectIndex.valid())) {
      if (Self->SelectIndex < Self->CursorIndex) {
         select_start   = Self->SelectIndex;
         select_end     = Self->CursorIndex;
         select_start_x = Self->SelectCharX;
         select_end_x   = Self->CursorCharX;
      }
      else {
         select_start   = Self->CursorIndex;
         select_end     = Self->SelectIndex;
         select_start_x = Self->CursorCharX;
         select_end_x   = Self->SelectCharX;
      }
   }

   FloatRect clip(0, 0, Self->VPWidth, Self->VPHeight);

   std::string font_fill = "rgb(0,0,0,255)";
   auto font_align = ALIGN::NIL;
   //DOUBLE alpha = 1.0;
   for (SEGINDEX seg=Start; seg < Stop; seg++) {
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
                  gfxDrawRectangle(Bitmap, segment.x + select_start_x, segment.y,
                     select_end_x - select_start_x, segment.height, Bitmap->packPixel(0, 128, 0), BAF::FILL);
               }
               else {
                  gfxDrawRectangle(Bitmap, segment.x + select_start_x, segment.y,
                     segment.width - select_start_x, segment.height, Bitmap->packPixel(0, 128, 0), BAF::FILL);
               }
            }
            else if (select_end < segment.stop) {
               gfxDrawRectangle(Bitmap, segment.x, segment.y, select_end_x, segment.height,
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
                (Self->CursorIndex.get_prev_char_or_inline(Self, Stream) IS '\n'));
            else if ((Self->Page->Flags & VF::HAS_FOCUS) != VF::NIL) { // Standard text cursor
               objVectorRectangle::create::global({
                  fl::Owner(Viewport->UID),
                  fl::X(segment.area.X + Self->CursorCharX), fl::Y(segment.area.Y),
                  fl::Width(2), fl::Height(segment.area.Height - segment.gutter),
                  fl::Fill("rgb(255,0,0,255)") });
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

      auto x_offset = segment.area.X;
      for (auto cursor = segment.start; cursor < segment.stop; cursor.nextCode()) {
         switch (Stream[cursor.index].code) {
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

                        objVectorText::create::global({
                           fl::Owner(Viewport->UID),
                           fl::X(ix), fl::Y(iy),
                           fl::String(stack_para.top()->value),
                           fl::Font(font),
                           fl::Fill(stack_list.top()->fill)
                        });
                     }
                  }
                  else if (stack_list.top()->type IS bc_list::BULLET) {
                     DOUBLE ix = segment.area.X - stack_para.top()->item_indent + (m_font->Height * 0.5);
                     DOUBLE iy = segment.area.Y + (segment.area.Height - segment.gutter) - (m_font->Height * 0.5);

                     objVectorEllipse::create::global({
                        fl::Owner(Viewport->UID),
                        fl::CenterX(ix), fl::CenterY(iy),
                        fl::Radius(m_font->Height * 0.25),
                        fl::Fill(stack_list.top()->fill)
                     });
                  }
               }
               break;

            case SCODE::PARAGRAPH_END:
               stack_para.pop();
               break;

            case SCODE::TABLE_START: {
               stack_table.push(&stream_data<bc_table>(Self, cursor));
               auto table = stack_table.top();

               stack_vp.push(Viewport);

               Viewport = objVectorViewport::create::global({
                  fl::Owner(Viewport->UID),
                  fl::X(table->x), fl::Y(table->y),
                  fl::Width(table->width), fl::Height(table->height)
               });

               // To build sophisticated table grids, we allocate a single VectorPath that
               // the table, rows and cells can all add to.  This ensures efficiency and consistency
               // in the final result.

               table->path = objVectorPath::create::global({ fl::Owner(Viewport->UID) });

               if ((!table->fill.empty()) or (!table->stroke.empty())) {
                  char path[120];
                  snprintf(path, sizeof(path), "M0,0 H%g V%g H0 Z", table->width, table->height);

                  table->path->set(FID_Sequence, path);

                  if (!table->fill.empty()) table->path->set(FID_Fill, table->fill);

                  if (!table->stroke.empty()) {
                     table->path->set(FID_Stroke, table->stroke);
                     table->path->set(FID_StrokeWidth, table->strokeWidth);
                  }
               }
               break;
            }

            case SCODE::TABLE_END:
               Viewport = stack_vp.top();
               stack_vp.pop();

               stack_table.pop();
               break;

            case SCODE::ROW: {
               stack_row.push(&stream_data<bc_row>(Self, cursor));
               auto row = stack_row.top();
               if ((!row->fill.empty()) and (row->row_height > 0)) {
                  objVectorRectangle::create::global({
                     fl::Owner(Viewport->UID),
                     fl::X(0), fl::Y(row->y - stack_table.top()->y),
                     fl::Width(stack_table.top()->width),
                     fl::Height(row->row_height),
                     fl::Fill(row->fill)
                  });
               }
               break;
            }

            case SCODE::ROW_END:
               stack_row.pop();
               break;

            case SCODE::CELL: {
               auto &cell = stream_data<bc_cell>(Self, cursor);

               stack_vp.push(Viewport);

               //DOUBLE cell_width  = stack_table.top()->columns[cell.column].width;
               //DOUBLE cell_height = stack_row.top()->row_height;

               if ((cell.width >= 1) and (cell.height >= 1)) {
                  Viewport = objVectorViewport::create::global({
                     fl::Owner(Viewport->UID),
                     fl::X(cell.x - stack_table.top()->x), fl::Y(cell.y - stack_table.top()->y),
                     fl::Width(cell.width), fl::Height(cell.height)
                  });

                  auto rect = objVectorRectangle::create::global({
                     fl::Owner(Viewport->UID),
                     fl::X(0), fl::Y(0), fl::Width("100%"), fl::Height("100%")
                  });

                  if (!cell.stroke.empty()) {
                     rect->set(FID_Stroke, cell.stroke);
                     rect->set(FID_StrokeWidth, 1);
                  }

                  if (!cell.fill.empty()) rect->set(FID_Fill, cell.fill);
               }

               break;
            }

            case SCODE::CELL_END: {
               Viewport = stack_vp.top();
               stack_vp.pop();
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
               if (oob) break;

               auto &img = stream_data<bc_image>(Self, cursor);

               // Apply the rectangle dimensions as defined during layout.  If the image is inline then we utilise
               // x_offset for managing the horizontal position amongst the text.

               DOUBLE x;
               if (img.floating()) x = img.x + img.final_pad.left;
               else {
                  if ((font_align & ALIGN::HORIZONTAL) != ALIGN::NIL) x = x_offset + ((segment.align_width - segment.area.Width) * 0.5);
                  else if ((font_align & ALIGN::RIGHT) != ALIGN::NIL) x = x_offset + segment.align_width - segment.area.Width;
                  else x = x_offset;
               }
               DOUBLE y = segment.area.Y + img.final_pad.top;

               if ((img.rect = objVectorRectangle::create::global({
                     fl::Name("rect_image"),
                     fl::Owner(Viewport->UID),
                     fl::X(x), fl::Y(y), fl::Width(img.final_width), fl::Height(img.final_height),
                     fl::Fill(img.src)
                  }))) {
               }

               if (!img.floating()) x_offset += img.final_width + img.final_pad.left + img.final_pad.right;
               break;
            }

            case SCODE::TEXT: {
               if (oob) break;

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

               DOUBLE x;
               if ((font_align & ALIGN::HORIZONTAL) != ALIGN::NIL) x = x_offset + ((segment.align_width - segment.area.Width) * 0.5);
               else if ((font_align & ALIGN::RIGHT) != ALIGN::NIL) x = x_offset + segment.align_width - segment.area.Width;
               else x = x_offset;

               if (!str.empty()) {
                  auto text = objVectorText::create::global({
                     fl::Owner(Viewport->UID),
                     fl::X(x), fl::Y(y),
                     fl::String(str),
                     fl::Font(font),
                     fl::Fill(font_fill)
                  });

                  DOUBLE twidth;
                  text->get(FID_TextWidth, &twidth);
                  x_offset += twidth;
               }
               break;
            }

            default: break;
         }
      }
   } // for loop
}
