
//********************************************************************************************************************
// If the layout needs to be recalculated, set the UpdatingLayout field before calling this function.

static void redraw(extDocument *Self, bool Focus)
{
   pf::Log log(__FUNCTION__);

   log.traceBranch("");

   {
      #ifndef RETAIN_LOG_LEVEL
      pf::LogLevel level(3);
      #endif

      layout_doc(Self); // Does nothing if UpdatingLayout is false
   }

   if (Self->Viewport->Scene->SurfaceID) Self->Viewport->draw();

   if ((Focus) and (Self->FocusIndex != -1)) set_focus(Self, -1, "redraw()");
}

//********************************************************************************************************************
// Convert the layout information to a vector scene.  This is the final step in the layout process. The advantage in
// performing this step separately to the layout process is that the graphics resources are managed last, which is
// sensible for keeping them out of the layout loop.

void layout::gen_scene_init(objVectorViewport *Viewport)
{
   // Remove former objects from the viewport

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

   if (glFonts.empty()) { // Sanity check
      log.traceWarning("Failed to load a default font.");
      return;
   }

   std::stack<bc_list *> stack_list;
   std::stack<bc_row *> stack_row;
   std::stack<bc_paragraph *> stack_para;
   std::stack<bc_table *> stack_table;
   std::stack<bc_link *> stack_link;
   std::stack<bc_font *> stack_style;
   std::stack<objVectorViewport *> stack_vp;

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

   for (SEGINDEX seg=Start; seg < Stop; seg++) {
      auto &segment = m_segments[seg];

      // Don't process codes that are out of bounds.  Be mindful of floating vectors as they can be placed at any coordinate.

      bool oob = segment.oob(0, 0, Self->VPWidth, Self->VPHeight);

      if (!stack_link.empty()) {
         stack_link.top()->area = segment.area;
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

      auto x_offset = segment.area.X;
      for (auto cursor = segment.start; cursor < segment.stop; cursor.next_code()) {
         switch (Stream[cursor.index].code) {
            case SCODE::FONT: {
               auto &style = Self->stream_data<bc_font>(cursor);
               stack_style.push(&style);
               break;
            }

            case SCODE::FONT_END: {
               stack_style.pop();
               break;
            }

            case SCODE::LIST_START:
               stack_list.push(&Self->stream_data<bc_list>(cursor));
               break;

            case SCODE::LIST_END:
               stack_list.pop();
               break;

            case SCODE::PARAGRAPH_START:
               stack_para.push(&Self->stream_data<bc_paragraph>(cursor));

               if (stack_style.empty()) { // Sanity check - there must always be at least one font style on the stack.
                  log.warning("The byte stream is missing a font style code.");
                  return;
               }

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
                           fl::Font(stack_style.top()->get_font()),
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

            // TODO: It would be preferable to pre-process 'use' instructions in advance.
            case SCODE::USE: {
               auto &use = Self->stream_data<bc_use>(cursor);
               svgParseSymbol(Self->SVG, use.id.c_str(), Viewport);
               break;
            }

            case SCODE::TABLE_START: {
               stack_table.push(&Self->stream_data<bc_table>(cursor));
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
                  table->seq.push_back({ .Type = PE::Move, .X = 0, .Y = 0 });
                  table->seq.push_back({ .Type = PE::HLineRel, .X = table->width, });
                  table->seq.push_back({ .Type = PE::VLineRel, .Y = table->height });
                  table->seq.push_back({ .Type = PE::HLineRel, .X = -table->width, });
                  table->seq.push_back({ .Type = PE::ClosePath });

                  if (!table->fill.empty()) table->path->set(FID_Fill, table->fill);

                  if (!table->stroke.empty()) {
                     table->path->set(FID_Stroke, table->stroke);
                     table->path->set(FID_StrokeWidth, table->strokeWidth);
                  }
               }
               break;
            }

            case SCODE::TABLE_END: {
               auto &table = stack_table.top();
               vpSetCommand(table->path, table->seq.size(), table->seq.data(),
                  table->seq.size() * sizeof(PathCommand));
               table->seq.clear();

               Viewport = stack_vp.top();
               stack_vp.pop();

               stack_table.pop();
               break;
            }

            case SCODE::ROW: {
               stack_row.push(&Self->stream_data<bc_row>(cursor));
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
               auto &cell = Self->stream_data<bc_cell>(cursor);

               stack_vp.push(Viewport);

               //DOUBLE cell_width  = stack_table.top()->columns[cell.column].width;
               //DOUBLE cell_height = stack_row.top()->row_height;

               if ((cell.width >= 1) and (cell.height >= 1)) {
                  Viewport = objVectorViewport::create::global({
                     fl::Owner(Viewport->UID),
                     fl::X(cell.x - stack_table.top()->x), fl::Y(cell.y - stack_table.top()->y),
                     fl::Width(cell.width), fl::Height(cell.height)
                  });

                  // If a cell defines fill/stroke values then it gets an independent rectangle to achieve that.
                  //
                  // If it defines a border then it can instead make use of the table's VectorPath object, which is
                  // more efficient and creates consistent looking output.

                  if ((!cell.stroke.empty()) or (!cell.fill.empty())) {
                     auto rect = objVectorRectangle::create::global({
                        fl::Owner(Viewport->UID),
                        fl::X(0), fl::Y(0), fl::Width("100%"), fl::Height("100%")
                     });

                     if (!cell.stroke.empty()) {
                        rect->set(FID_Stroke, cell.stroke);
                        rect->set(FID_StrokeWidth, cell.strokeWidth);
                     }

                     if (!cell.fill.empty()) rect->set(FID_Fill, cell.fill);
                  }

                  if ((cell.border != CB::NIL) and (cell.stroke.empty())) {
                     // When a cell defines a border value, it piggy-backs the table's stroke definition
                     auto &table = stack_table.top();
                     if (cell.border IS CB::ALL) {
                        table->seq.push_back({ .Type = PE::Move, .X = cell.x - table->x, .Y = cell.y - table->y });
                        table->seq.push_back({ .Type = PE::HLineRel, .X = cell.width });
                        table->seq.push_back({ .Type = PE::VLineRel, .Y = cell.height });
                        table->seq.push_back({ .Type = PE::HLineRel, .X = -cell.width });
                        table->seq.push_back({ .Type = PE::ClosePath });
                     }
                     else {
                        if ((cell.border & CB::LEFT) != CB::NIL) {
                           table->seq.push_back({ .Type = PE::Move, .X = cell.x - table->x, .Y = cell.y - table->y });
                           table->seq.push_back({ .Type = PE::VLineRel, .Y = cell.height });
                           table->seq.push_back({ .Type = PE::ClosePath });
                        }

                        if ((cell.border & CB::TOP) != CB::NIL) {
                           table->seq.push_back({ .Type = PE::Move, .X = cell.x - table->x, .Y = cell.y - table->y });
                           table->seq.push_back({ .Type = PE::HLineRel, .X = cell.width });
                           table->seq.push_back({ .Type = PE::ClosePath });
                        }

                        if ((cell.border & CB::RIGHT) != CB::NIL) {
                           table->seq.push_back({ .Type = PE::Move, .X = cell.x - table->x + cell.width, .Y = cell.y - table->y });
                           table->seq.push_back({ .Type = PE::VLineRel, .Y = cell.height });
                           table->seq.push_back({ .Type = PE::ClosePath });
                        }

                        if ((cell.border & CB::BOTTOM) != CB::NIL) {
                           table->seq.push_back({ .Type = PE::Move, .X = cell.x - table->x, .Y = cell.y - table->y + cell.height });
                           table->seq.push_back({ .Type = PE::HLineRel, .X = cell.width });
                           table->seq.push_back({ .Type = PE::ClosePath });
                        }
                     }
                  }
               }

               break;
            }

            case SCODE::CELL_END: {
               Viewport = stack_vp.top();
               stack_vp.pop();
               break;
            }

            case SCODE::LINK: {
               auto link = &Self->stream_data<bc_link>(cursor);

               link->cursor_start = cursor;
               link->area = { x_offset, segment.area.Y, segment.area.Width - x_offset, segment.area.Height };
               stack_link.push(link);

               // Font management

               link->font.fill = link->fill; // Reset the fill instruction to the default
               stack_style.push(&link->font);
               if (Self->HasFocus) {
                  // Override the default link colour if the link has the tab key's focus
                  if ((Self->Tabs[Self->FocusIndex].type IS TT_LINK) and
                        (Self->Tabs[Self->FocusIndex].ref IS link->id) and
                        (Self->Tabs[Self->FocusIndex].active)) {
                     link->font.fill = Self->LinkSelectFill;
                  }
                  else if (link->hover) {
                     link->font.fill = Self->LinkSelectFill;
                  }
               }

               break;
            }

            case SCODE::LINK_END: {
               auto &link = stack_link.top();
               link->cursor_end = cursor;
               link->area.Width = x_offset - link->area.X;
               if (link->area.Width >= 1) link->append_link();

               // Create a VectorPath that represents the clickable area.  Doing this at the end of the
               // link ensures that our path has input priority over existing text.

               if ((link->vector_path = objVectorPath::create::global({
                     fl::Owner(Viewport->UID), fl::Name("link_vp"), fl::Cursor(PTC::HAND)
                     #ifdef GUIDELINES
                     , fl::Stroke("rgb(255,0,0)"), fl::StrokeWidth(1)
                     #endif
                  }))) {

                  vpSetCommand(link->vector_path, link->path.size(), link->path.data(),
                     link->path.size() * sizeof(PathCommand));

                  if (link->vector_path->Scene->SurfaceID) {
                     auto callback = make_function_stdc(link_callback);
                     vecSubscribeInput(link->vector_path, JTYPE::BUTTON|JTYPE::FEEDBACK, &callback);
                  }
               }

               link->path.clear();

               stack_link.pop();
               stack_style.pop();
               break;
            }

            case SCODE::IMAGE: {
               if (oob) break;

               auto &img = Self->stream_data<bc_image>(cursor);

               // Apply the rectangle dimensions as defined during layout.  If the image is inline then we utilise
               // x_offset for managing the horizontal position amongst the text.

               DOUBLE x;
               if (img.floating()) x = img.x + img.final_pad.left;
               else {
                  if ((stack_style.top()->options & FSO::ALIGN_CENTER) != FSO::NIL) x = x_offset + ((segment.align_width - segment.area.Width) * 0.5);
                  else if ((stack_style.top()->options & FSO::ALIGN_RIGHT) != FSO::NIL) x = x_offset + segment.align_width - segment.area.Width;
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

               auto &txt = Self->stream_data<bc_text>(cursor);
               auto font = stack_style.top()->get_font();

               std::string str;
               if (cursor.index < segment.trim_stop.index) str.append(txt.text, cursor.offset, std::string::npos);
               else str.append(txt.text, cursor.offset, segment.trim_stop.offset - cursor.offset);

               if (!str.empty()) {
                  DOUBLE y = segment.area.Y;
                  if ((stack_style.top()->valign & ALIGN::TOP) != ALIGN::NIL) y += font->Ascent;
                  else if ((stack_style.top()->valign & ALIGN::VERTICAL) != ALIGN::NIL) {
                     DOUBLE avail_space = segment.area.Height - segment.gutter;
                     y += avail_space - ((avail_space - font->Ascent) * 0.5);
                  }
                  else y += segment.area.Height - segment.gutter;

                  DOUBLE x;
                  if ((stack_style.top()->options & FSO::ALIGN_CENTER) != FSO::NIL) x = x_offset + ((segment.align_width - segment.area.Width) * 0.5);
                  else if ((stack_style.top()->options & FSO::ALIGN_RIGHT) != FSO::NIL) x = x_offset + segment.align_width - segment.area.Width;
                  else x = x_offset;

                  auto vt  = objVectorText::create::global({
                     fl::Owner(Viewport->UID),
                     fl::X(x), fl::Y(y),
                     fl::String(str),
                     fl::Cursor(PTC::TEXT),
                     fl::Font(font),
                     fl::Fill(stack_style.top()->fill),
                     fl::TextFlags(((stack_style.top()->options & FSO::UNDERLINE) != FSO::NIL) ? VTXF::UNDERLINE : VTXF::NIL)
                  });

                  txt.vector_text.push_back(vt);

                  DOUBLE twidth;
                  vt->get(FID_TextWidth, &twidth);
                  x_offset += twidth;
               }
               break;
            }

            default: break;
         } // switch()
      } // for cursor

      if (!stack_link.empty()) {
         auto &link = stack_link.top();
         link->area.Width = x_offset - link->area.X;
         if (link->area.Width >= 1) link->append_link();
      }

   } // for segment
}
