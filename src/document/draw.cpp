
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
// Generic input handler for all widgets

static void handle_widget_event(extDocument *Self, widget_mgr &Widget, const InputEvent *Event)
{
   for (; Event; Event = Event->Next) {
      if (Event->Type IS JET::ENTERED_AREA) {
         //Widget.hover = true;
         Self->Viewport->draw();
      }
      else if (Event->Type IS JET::LEFT_AREA) {
         //Widget.hover = false;
         Self->Viewport->draw();
      }
   }
}

//********************************************************************************************************************

static ERROR inputevent_checkbox(objVectorViewport *Viewport, const InputEvent *Event)
{
   auto Self = (extDocument *)CurrentContext();

   if (!Self->Widgets.contains(Viewport->UID)) return ERR_Terminate;

   ui_widget &widget = Self->Widgets[Viewport->UID];
   auto checkbox = std::get<bc_checkbox *>(widget.widget);

   handle_widget_event(Self, *checkbox, Event);

   for (; Event; Event = Event->Next) {
      if ((Event->Flags & JTYPE::BUTTON) != JTYPE::NIL) {
         if (Event->Type IS JET::LMB) {
            if (Event->Value IS 1) checkbox->alt_state ^= 1;
         }

         if (checkbox->alt_state) checkbox->viewport->setFill(checkbox->alt_fill);
         else checkbox->viewport->setFill(checkbox->fill);

         Self->Viewport->draw();
      }
   }

   return ERR_Okay;
}

//********************************************************************************************************************

ERROR build_widget(widget_mgr &Widget, doc_segment &Segment, objVectorViewport *Viewport, bc_font *Style,
   DOUBLE &XAdvance, DOUBLE ExtWidth, bool CreateViewport)
{
   DOUBLE x, y;
   if (Widget.floating()) x = Widget.x + Widget.final_pad.left;
   else {
      if ((Style->options & FSO::ALIGN_CENTER) != FSO::NIL) x = XAdvance + ((Segment.align_width - Segment.area.Width) * 0.5);
      else if ((Style->options & FSO::ALIGN_RIGHT) != FSO::NIL) x = XAdvance + Segment.align_width - Segment.area.Width;
      else x = XAdvance;
   }
   y = Segment.area.Y + Widget.final_pad.top;

   const DOUBLE width = Widget.final_width + ExtWidth;

   if (CreateViewport) {
      if (!(Widget.viewport = objVectorViewport::create::global({
            fl::Name("vp_widget"),
            fl::Owner(Viewport->UID),
            fl::X(x), fl::Y(y),
            fl::Width(width), fl::Height(Widget.final_height),
            fl::Fill(Widget.alt_state ? Widget.alt_fill : Widget.fill)
         }))) {
         return ERR_CreateObject;
      }
   }
   else if (!(Widget.rect = objVectorRectangle::create::global({
        fl::Name("rect_widget"),
        fl::Owner(Viewport->UID),
        fl::X(x), fl::Y(y),
        fl::Width(width), fl::Height(Widget.final_height),
        fl::Fill(Widget.alt_state ? Widget.alt_fill : Widget.fill)
      }))) {

      return ERR_CreateObject;
   }

   if (!Widget.floating()) XAdvance += Widget.final_pad.left + Widget.final_pad.right + width;

   return ERR_Okay;
}

//********************************************************************************************************************
// Convert the layout information to a vector scene.  This is the final step in the layout process. The advantage in
// performing this step separately to the layout process is that the graphics resources are managed last, which is
// sensible for keeping them out of the layout loop.
//
// It is intended that the layout process generates the document's entire scene graph every time.  Optimisations
// relating to things like obscuration of graphics elements are considered to be the job of the VectorScene's drawing
// functionality.

ERROR layout::gen_scene_init(objVectorViewport *Viewport)
{
   pf::Log log(__FUNCTION__);

   // Remove former objects from the viewport

   pf::vector<ChildEntry> list;
   if (!ListChildren(Viewport->UID, &list)) {
      for (auto it=list.rbegin(); it != list.rend(); it++) FreeResource(it->ObjectID);
   }

   m_cursor_drawn = false;

   Self->Links.clear();
   Self->Widgets.clear();

   if (Self->UpdatingLayout) return ERR_NothingDone; // Drawing is disabled if the layout is being updated

   if (glFonts.empty()) { // Sanity check
      log.traceWarning("Failed to load a default font.");
      return ERR_Failed;
   }

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

   return ERR_Okay;
}

void layout::gen_scene_graph(objVectorViewport *Viewport, std::vector<doc_segment> &Segments)
{
   pf::Log log(__FUNCTION__);

   std::stack<bc_list *>      stack_list;
   std::stack<bc_row *>       stack_row;
   std::stack<bc_paragraph *> stack_para;
   std::stack<bc_table *>     stack_table;
   std::stack<ui_link>        stack_ui_link;
   std::stack<bc_font *>      stack_style;
   std::stack<objVectorViewport *> stack_vp;

#ifndef RETAIN_LOG_LEVEL
   pf::LogLevel level(2);
#endif

/*
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
*/
   for (SEGINDEX seg=0; seg < SEGINDEX(Segments.size()); seg++) {
      auto &segment = Segments[seg];

      if (!stack_ui_link.empty()) {
         stack_ui_link.top().area = segment.area;
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
                (Self->CursorIndex.get_prev_char_or_inline(segment.stream[0]) IS '\n'));
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

      auto x_advance = segment.area.X;
      for (auto cursor = segment.start; cursor < segment.stop; cursor.next_code()) {
         switch (segment.stream[0][cursor.index].code) {
            case SCODE::FONT: {
               auto &style = segment.stream->lookup<bc_font>(cursor);
               stack_style.push(&style);
               break;
            }

            case SCODE::FONT_END:
               stack_style.pop();
               break;

            case SCODE::LIST_START:
               stack_list.push(&segment.stream->lookup<bc_list>(cursor));
               break;

            case SCODE::LIST_END:
               stack_list.pop();
               break;

            case SCODE::PARAGRAPH_START:
               stack_para.push(&segment.stream->lookup<bc_paragraph>(cursor));

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
               auto &use = segment.stream->lookup<bc_use>(cursor);
               svgParseSymbol(Self->SVG, use.id.c_str(), Viewport);
               break;
            }

            case SCODE::TABLE_START: {
               stack_table.push(&segment.stream->lookup<bc_table>(cursor));
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
                     table->path->set(FID_StrokeWidth, table->stroke_width);
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
               stack_row.push(&segment.stream->lookup<bc_row>(cursor));
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
               auto &cell = segment.stream->lookup<bc_cell>(cursor);

               //DOUBLE cell_width  = stack_table.top()->columns[cell.column].width;
               //DOUBLE cell_height = stack_row.top()->row_height;

               if ((cell.width >= 1) and (cell.height >= 1)) {
                  objVectorViewport *cell_vp = objVectorViewport::create::global({
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
                        fl::Owner(cell_vp->UID),
                        fl::X(0), fl::Y(0), fl::Width("100%"), fl::Height("100%")
                     });

                     if (!cell.stroke.empty()) {
                        rect->set(FID_Stroke, cell.stroke);
                        rect->set(FID_StrokeWidth, cell.stroke_width);
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

                  gen_scene_graph(cell_vp, cell.segments);
               }

               break;
            }

            case SCODE::LINK: {
               auto link = &segment.stream->lookup<bc_link>(cursor);

               stack_ui_link.push(ui_link {
                  .origin = *link,
                  .area   = { x_advance, segment.area.Y, segment.area.Width - x_advance, segment.area.Height },
                  .cursor_start = cursor,
                  .stream = segment.stream
               });

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
                  else if (stack_ui_link.top().hover) {
                     link->font.fill = Self->LinkSelectFill;
                  }
               }

               break;
            }

            case SCODE::LINK_END: {
               auto &ui_link = stack_ui_link.top();
               ui_link.cursor_end = cursor;
               ui_link.area.Width = x_advance - ui_link.area.X;
               if (ui_link.area.Width >= 1) ui_link.append_link();

               // Create a VectorPath that represents the clickable area.  Doing this at the end of the
               // link ensures that our path has input priority over existing text.

               if ((ui_link.vector_path = objVectorPath::create::global({
                     fl::Owner(Viewport->UID), fl::Name("link_vp"), fl::Cursor(PTC::HAND)
                     #ifdef GUIDELINES
                     , fl::Stroke("rgb(255,0,0)"), fl::StrokeWidth(1)
                     #endif
                  }))) {

                  vpSetCommand(ui_link.vector_path, ui_link.path.size(), ui_link.path.data(),
                     ui_link.path.size() * sizeof(PathCommand));

                  if (ui_link.vector_path->Scene->SurfaceID) {
                     auto callback = make_function_stdc(link_callback);
                     vecSubscribeInput(ui_link.vector_path, JTYPE::BUTTON|JTYPE::FEEDBACK, &callback);
                  }
               }

               ui_link.path.clear();

               Self->Links.emplace_back(ui_link);

               stack_ui_link.pop();
               stack_style.pop();
               break;
            }

            case SCODE::BUTTON: {
               auto &button = segment.stream->lookup<bc_button>(cursor);
               build_widget(button, segment, Viewport, stack_style.top(), x_advance, 0, true);
               break;
            }

            case SCODE::CHECKBOX: {
               auto &checkbox = segment.stream->lookup<bc_checkbox>(cursor);

               if (!checkbox.label.empty()) {
                  if (checkbox.label_pos) { 
                     // Right-sided labels can be integrated with the widget so that clicking affects state.
                     if (!build_widget(checkbox, segment, Viewport, stack_style.top(), x_advance, checkbox.label_width + checkbox.label_pad, true)) {
                        DOUBLE x, y;
                        auto font = stack_style.top()->get_font();
                        const DOUBLE avail_space = checkbox.final_height - font->Gutter;
                        y = checkbox.final_pad.top + avail_space - ((avail_space - font->Ascent) * 0.5);

                        x = checkbox.final_width + checkbox.label_pad;                        

                        objVectorText::create::global({
                           fl::Owner(checkbox.viewport->UID),
                           fl::X(std::trunc(x)), fl::Y(std::trunc(y)),
                           fl::String(checkbox.label),
                           fl::Font(font),
                           fl::Fill(stack_style.top()->fill)
                        });
                     }
                  }
                  else { 
                     // Left-sided labels aren't included in the scope of the widget's viewport
                     // TODO: Interactivity is feasible but we'll need to add an input feedback mechanism for that
                     auto font = stack_style.top()->get_font();
                     const DOUBLE avail_space = checkbox.final_height - font->Gutter;
                     DOUBLE y = segment.area.Y + checkbox.final_pad.top + avail_space - ((avail_space - font->Ascent) * 0.5);

                     objVectorText::create::global({
                        fl::Owner(Viewport->UID),
                        fl::X(std::trunc(x_advance)), fl::Y(std::trunc(y)),
                        fl::String(checkbox.label),
                        fl::Font(font),
                        fl::Fill(stack_style.top()->fill)
                     });

                     x_advance += checkbox.label_width + checkbox.label_pos;

                     build_widget(checkbox, segment, Viewport, stack_style.top(), x_advance, 0, true);                     
                  }
               }
               else build_widget(checkbox, segment, Viewport, stack_style.top(), x_advance, 0, true);

               if (checkbox.viewport) {
                  auto call = make_function_stdc(inputevent_checkbox);
                  vecSubscribeInput(checkbox.viewport, JTYPE::BUTTON|JTYPE::FEEDBACK, &call);

                  Self->Widgets.emplace(checkbox.viewport->UID, ui_widget { &checkbox });
               }
               break;
            }

            case SCODE::IMAGE: {
               auto &img = segment.stream->lookup<bc_image>(cursor);
               build_widget(img, segment, Viewport, stack_style.top(), x_advance, 0, false);
               break;
            }

            case SCODE::COMBOBOX: {
               auto &combo = segment.stream->lookup<bc_combobox>(cursor);
               build_widget(combo, segment, Viewport, stack_style.top(), x_advance, 0, true);
               break;
            }

            case SCODE::INPUT: {
               auto &input = segment.stream->lookup<bc_input>(cursor);
               auto font = stack_style.top()->get_font();

               const DOUBLE avail_space = input.final_height - font->Gutter;

               if (!input.label.empty()) {
                  if (input.label_pos) {
                     build_widget(input, segment, Viewport, stack_style.top(), x_advance, 0, true);

                     DOUBLE y = segment.area.Y + input.final_pad.top + avail_space - ((avail_space - font->Ascent) * 0.5);

                     objVectorText::create::global({
                        fl::Owner(Viewport->UID),
                        fl::X(std::trunc(x_advance + input.label_pad)), fl::Y(std::trunc(y)),
                        fl::String(input.label),
                        fl::Font(font),
                        fl::Fill(stack_style.top()->fill)
                     });

                     x_advance += input.label_width + input.label_pad;
                  }
                  else {
                     DOUBLE y = segment.area.Y + input.final_pad.top + avail_space - ((avail_space - font->Ascent) * 0.5);

                     objVectorText::create::global({
                        fl::Owner(Viewport->UID),
                        fl::X(std::trunc(x_advance)), fl::Y(std::trunc(y)),
                        fl::String(input.label),
                        fl::Font(font),
                        fl::Fill(stack_style.top()->fill)
                     });

                     x_advance += input.label_pad + input.label_width;

                     build_widget(input, segment, Viewport, stack_style.top(), x_advance, 0, true);
                  }
               }
               else build_widget(input, segment, Viewport, stack_style.top(), x_advance, 0, true);

               DOUBLE y = avail_space - ((avail_space - font->Ascent) * 0.5);

               if ((input.clip_vp = objVectorViewport::create::global({
                     fl::Name("vp_clip_input"),
                     fl::Owner(input.viewport->UID),
                     fl::X(input.label_pad), fl::Y(0),
                     fl::XOffset(input.label_pad), fl::YOffset(0),
                     fl::Overflow(VOF::HIDDEN)
                  }))) {

                  auto flags = VTXF::EDITABLE;
                  if (input.secret) flags |= VTXF::SECRET;

                  objVectorText::create::global({
                     fl::Owner(input.clip_vp->UID),
                     fl::X(0), fl::Y(std::trunc(y)),
                     fl::String(input.value),
                     fl::Cursor(PTC::TEXT),
                     fl::Font(font),
                     fl::Fill(input.font_fill),
                     fl::LineLimit(1),
                     fl::TextFlags(flags)
                  });
               }

               break;
            }

            case SCODE::TEXT: {
               auto &txt = segment.stream->lookup<bc_text>(cursor);
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
                  if ((stack_style.top()->options & FSO::ALIGN_CENTER) != FSO::NIL) x = x_advance + ((segment.align_width - segment.area.Width) * 0.5);
                  else if ((stack_style.top()->options & FSO::ALIGN_RIGHT) != FSO::NIL) x = x_advance + segment.align_width - segment.area.Width;
                  else x = x_advance;

                  auto vt = objVectorText::create::global({
                     fl::Owner(Viewport->UID),
                     fl::X(x), fl::Y(std::trunc(y)),
                     fl::String(str),
                     fl::Cursor(PTC::TEXT),
                     fl::Font(font),
                     fl::Fill(stack_style.top()->fill),
                     fl::TextFlags(((stack_style.top()->options & FSO::UNDERLINE) != FSO::NIL) ? VTXF::UNDERLINE : VTXF::NIL)
                  });

                  txt.vector_text.push_back(vt);

                  DOUBLE twidth;
                  vt->get(FID_TextWidth, &twidth);
                  x_advance += twidth;
               }
               break;
            }

            default: break;
         } // switch()
      } // for cursor

      if (!stack_ui_link.empty()) {
         auto &link = stack_ui_link.top();
         link.area.Width = x_advance - link.area.X;
         if (link.area.Width >= 1) link.append_link();
      }

   } // for segment
}
