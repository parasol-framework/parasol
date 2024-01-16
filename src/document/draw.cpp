
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
         //Self->Viewport->draw();
      }
      else if (Event->Type IS JET::LEFT_AREA) {
         //Widget.hover = false;
         //Self->Viewport->draw();
      }
   }
}

//********************************************************************************************************************

static ERROR inputevent_button(objVectorViewport *Viewport, const InputEvent *Event)
{
   auto Self = (extDocument *)CurrentContext();

   if (!Self->Widgets.contains(Viewport->UID)) return ERR_Terminate;

   ui_widget &widget = Self->Widgets[Viewport->UID];
   auto button = std::get<bc_button *>(widget.widget);

   handle_widget_event(Self, *button, Event);

   for (; Event; Event = Event->Next) {
      if ((Event->Flags & JTYPE::BUTTON) != JTYPE::NIL) {
         if (Event->Type IS JET::LMB) {
            if (Event->Value IS 1) button->alt_state = true;
            else button->alt_state = false;
         }

         if (button->alt_state) button->viewport->setFill(button->alt_fill);
         else button->viewport->setFill(button->fill);

         Self->Viewport->draw();
      }
   }

   return ERR_Okay;
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

ERROR layout::build_widget(widget_mgr &Widget, doc_segment &Segment, objVectorViewport *Viewport, bc_font *Style,
   DOUBLE &XAdvance, DOUBLE ExtWidth, bool CreateViewport, DOUBLE &X, DOUBLE &Y)
{
   if (Widget.floating_x()) {
      // If the widget is floating then the X coordinate will be pre-calculated during layout
      X = Widget.x + Widget.final_pad.left;
   }
   else {
      // For inline widgets, alignment is calculated from the active style.
      if ((Style->options & FSO::ALIGN_CENTER) != FSO::NIL) X = XAdvance + ((Segment.align_width - Segment.area.Width) * 0.5);
      else if ((Style->options & FSO::ALIGN_RIGHT) != FSO::NIL) X = XAdvance + Segment.align_width - Segment.area.Width;
      else X = XAdvance;
   }

   if (Widget.floating_x()) Y = Segment.area.Y + Widget.final_pad.top;
   else {
      if ((Style->valign & ALIGN::TOP) != ALIGN::NIL) Y = Segment.area.Y + Widget.final_pad.top;
      else if ((Style->valign & ALIGN::VERTICAL) != ALIGN::NIL) {
         DOUBLE avail_space = Segment.area.Height - Segment.gutter;
         Y = Segment.area.Y + ((avail_space - (Widget.final_height + Widget.final_pad.top + Widget.final_pad.bottom)) * 0.5);
      }
      else {
         // Bottom alignment.  Aligning to the gutter produces better results compared to base line alignment.
         Y = Segment.area.Y + Segment.area.Height - Widget.final_height - Widget.final_pad.bottom;
      }
   }

   const DOUBLE width = Widget.final_width + ExtWidth;

   if (CreateViewport) {
      if (Widget.viewport.empty()) {
         auto vp = objVectorViewport::create::global({
            fl::Name("vp_widget"),
            fl::Owner(Viewport->UID),
            fl::Fill(Widget.alt_state ? Widget.alt_fill : Widget.fill)
         });

         if (!vp) return ERR_CreateObject;
         else Widget.viewport.set(vp);
      }

      Widget.viewport->setFields(fl::X(X), fl::Y(Y), fl::Width(width), fl::Height(Widget.final_height));
   }
   else {
      if (Widget.rect.empty()) {
         auto rect = objVectorRectangle::create::global({
            fl::Name("rect_widget"),
            fl::Owner(Viewport->UID),
            fl::Fill(Widget.alt_state ? Widget.alt_fill : Widget.fill)
         });

         if (!rect) return ERR_CreateObject;
         else Widget.rect = rect;
      }

      Widget.rect->setFields(fl::X(X), fl::Y(Y), fl::Width(width), fl::Height(Widget.final_height));
   }

   if (!Widget.floating_x()) XAdvance += Widget.final_pad.left + Widget.final_pad.right + width;

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

   log.branch();

   // Remove former objects from the viewport

   for (auto it=Self->UIObjects.rbegin(); it != Self->UIObjects.rend(); it++) {
      FreeResource(*it);
   }
   Self->UIObjects.clear();

   m_cursor_drawn = false;

   Self->Links.clear();
   Self->Widgets.clear(); // NB: Widgets are cleared and re-added because they use direct pointers to the std::vector stream

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
               std::vector<PathCommand> seq = {
                  { .Type = PE::Move, .X = segment.area.X + Self->CursorCharX, .Y = segment.area.Y },
                  { .Type = PE::VLineRel, .Y = segment.area.Height - segment.gutter }
               };

               auto vp = objVectorPath::create::global({
                  fl::Owner(Viewport->UID), fl::Stroke("rgb(255,0,0,255)"), fl::StrokeWidth(2) 
               });

               vpSetCommand(vp, seq.size(), seq.data(), seq.size() * sizeof(PathCommand));
               m_cursor_drawn = true;
            }
         }
      }

      auto x_advance = segment.area.X;
      for (auto cursor = segment.start; cursor < segment.stop; cursor.next_code()) {
         switch (segment.stream[0][cursor.index].code) {
            case SCODE::FONT: {
               stack_style.push(&segment.stream->lookup<bc_font>(cursor));
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

            case SCODE::PARAGRAPH_START: {
               auto &para = segment.stream->lookup<bc_paragraph>(cursor);
               stack_para.push(&para);
               stack_style.push(&segment.stream->lookup<bc_paragraph>(cursor).font);

               if ((!stack_list.empty()) and (para.list_item)) {
                  // Handling for paragraphs that form part of a list

                  if ((stack_list.top()->type IS bc_list::CUSTOM) or
                      (stack_list.top()->type IS bc_list::ORDERED)) {
                     if (!para.icon.empty()) {
                        para.icon->setFields(
                           fl::X(segment.area.X - para.item_indent), 
                           fl::Y(segment.area.Y + segment.area.Height - segment.gutter)
                        );
                     }
                  }
                  else if (stack_list.top()->type IS bc_list::BULLET) {
                     if (!para.icon.empty()) {
                        const DOUBLE radius = segment.area.Height * 0.2;
                        para.icon->setFields(
                           fl::CenterX(segment.area.X - para.item_indent + radius), 
                           fl::CenterY(segment.area.Y + (segment.area.Height * 0.5)),
                           fl::Radius(radius));
                     }
                  }
               }
               break;
            }

            case SCODE::PARAGRAPH_END:
               stack_style.pop();
               stack_para.pop();
               break;

            case SCODE::TABLE_START: {
               stack_table.push(&segment.stream->lookup<bc_table>(cursor));
               auto table = stack_table.top();

               if (table->floating_x()); // If floating, X coordinate is calculated during layout
               else {
                  // Otherwise the X coordinate is dependent on the style's alignment
                  // NB: Currently the TABLE code is defined as non-graphical and positioning is declared in the
                  // table structure, not the segment.area.
                  if ((stack_style.top()->options & FSO::ALIGN_CENTER) != FSO::NIL) table->x = table->x + ((segment.align_width - segment.area.Width) * 0.5);
                  else if ((stack_style.top()->options & FSO::ALIGN_RIGHT) != FSO::NIL) table->x = table->x + segment.align_width - segment.area.Width;
               }

               stack_vp.push(Viewport);
               if (!(Viewport = *table->viewport)) return;

               Viewport->setFields(fl::X(table->x), fl::Y(table->y), fl::Width(table->width), fl::Height(table->height));

               // To build sophisticated table grids, we allocate a single VectorPath that
               // the table, rows and cells contribute to.  This ensures efficiency and consistency
               // in the final result.

               if ((!table->fill.empty()) or (!table->stroke.empty())) {
                  table->seq.push_back({ .Type = PE::Move, .X = 0, .Y = 0 });
                  table->seq.push_back({ .Type = PE::HLineRel, .X = table->width, });
                  table->seq.push_back({ .Type = PE::VLineRel, .Y = table->height });
                  table->seq.push_back({ .Type = PE::HLineRel, .X = -table->width, });
                  table->seq.push_back({ .Type = PE::ClosePath });
               }
               break;
            }

            case SCODE::TABLE_END: {
               auto &table = stack_table.top();
               vpSetCommand(*table->path, table->seq.size(), table->seq.data(),
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
               if (!row->rect_fill.empty()) {
                  row->rect_fill->setFields(fl::X(0), 
                     fl::Y(row->y - stack_table.top()->y),
                     fl::Width(stack_table.top()->width),    
                     fl::Height(row->row_height));
               }
               break;
            }

            case SCODE::ROW_END:
               stack_row.pop();
               break;

            case SCODE::CELL: {
               // If a cell defines fill/stroke values then it gets an independent rectangle to achieve that.
               //
               // If it defines a border then it can instead make use of the table's VectorPath object, which is
               // more efficient and creates consistent looking output.

               auto &cell = segment.stream->lookup<bc_cell>(cursor);
               auto table = stack_table.top();
               
               if ((!cell.fill.empty()) or (!cell.stroke.empty())) {
                  if (!cell.stroke.empty()) {
                     cell.rect_fill->setFields(fl::Stroke(cell.stroke), fl::StrokeWidth(cell.stroke_width));
                  }
                  if (!cell.fill.empty()) cell.rect_fill->setFields(fl::Fill(cell.fill));
               }
               else if (!cell.rect_fill.empty()) cell.rect_fill->setFields(fl::Fill(NULL), fl::Stroke(NULL));

               if ((cell.width >= 1) and (cell.height >= 1)) {
                  cell.viewport->setFields(fl::X(cell.x), fl::Y(cell.y),
                     fl::Width(cell.width), fl::Height(cell.height));

                  if ((cell.border != CB::NIL) and (cell.stroke.empty())) {
                     // When a cell defines a border value, it piggy-backs the table's stroke definition
                     if (cell.border IS CB::ALL) {
                        table->seq.push_back({ .Type = PE::Move, .X = cell.x, .Y = cell.y });
                        table->seq.push_back({ .Type = PE::HLineRel, .X = cell.width });
                        table->seq.push_back({ .Type = PE::VLineRel, .Y = cell.height });
                        table->seq.push_back({ .Type = PE::HLineRel, .X = -cell.width });
                        table->seq.push_back({ .Type = PE::ClosePath });
                     }
                     else {
                        if ((cell.border & CB::LEFT) != CB::NIL) {
                           table->seq.push_back({ .Type = PE::Move, .X = cell.x, .Y = cell.y });
                           table->seq.push_back({ .Type = PE::VLineRel, .Y = cell.height });
                           table->seq.push_back({ .Type = PE::ClosePath });
                        }

                        if ((cell.border & CB::TOP) != CB::NIL) {
                           table->seq.push_back({ .Type = PE::Move, .X = cell.x, .Y = cell.y });
                           table->seq.push_back({ .Type = PE::HLineRel, .X = cell.width });
                           table->seq.push_back({ .Type = PE::ClosePath });
                        }

                        if ((cell.border & CB::RIGHT) != CB::NIL) {
                           table->seq.push_back({ .Type = PE::Move, .X = cell.x + cell.width, .Y = cell.y });
                           table->seq.push_back({ .Type = PE::VLineRel, .Y = cell.height });
                           table->seq.push_back({ .Type = PE::ClosePath });
                        }

                        if ((cell.border & CB::BOTTOM) != CB::NIL) {
                           table->seq.push_back({ .Type = PE::Move, .X = cell.x, .Y = cell.y + cell.height });
                           table->seq.push_back({ .Type = PE::HLineRel, .X = cell.width });
                           table->seq.push_back({ .Type = PE::ClosePath });
                        }
                     }
                  }
                  else cell.viewport->setFields(fl::Width(0), fl::Height(0));

                  gen_scene_graph(*cell.viewport, cell.segments);
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
                  if ((Self->Tabs[Self->FocusIndex].type IS TT::LINK) and
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

               // Define the path that represents the clickable area.

               vpSetCommand(*ui_link.origin.path, ui_link.path.size(), ui_link.path.data(),
                  ui_link.path.size() * sizeof(PathCommand));

               acMoveToFront(*ui_link.origin.path);

               ui_link.path.clear(); // Return the memory

               Self->Links.emplace_back(ui_link);

               stack_ui_link.pop();
               stack_style.pop();
               break;
            }

            case SCODE::BUTTON: {
               DOUBLE wx, wy;
               auto &button = segment.stream->lookup<bc_button>(cursor);
               auto font = stack_style.top()->get_font();

               if (!build_widget(button, segment, Viewport, stack_style.top(), x_advance, 0, true, wx, wy)) {
                  const DOUBLE avail_space = button.final_height - font->Gutter;
                  const DOUBLE x = (button.final_width - button.label_width) * 0.5;
                  const DOUBLE y = avail_space - ((avail_space - font->Ascent) * 0.5);

                  if (!button.processed) {
                     button.processed = true;

                     button.label_text.set(objVectorText::create::global({
                        fl::Owner(button.viewport->UID),
                        fl::String(button.label),
                        fl::Font(font),
                        fl::Fill(button.font_fill)
                     }));

                     if (button.viewport->Scene->SurfaceID) {
                        auto call = make_function_stdc(inputevent_button);
                        vecSubscribeInput(*button.viewport, JTYPE::BUTTON|JTYPE::FEEDBACK, &call);
                     }
                  }

                  Self->Widgets.emplace(button.viewport->UID, ui_widget { &button });
                  button.label_text->setFields(fl::X(x), fl::Y(F2T(y)));
               }
               break;
            }

            case SCODE::CHECKBOX: {
               auto &checkbox = segment.stream->lookup<bc_checkbox>(cursor);

               DOUBLE wx, wy;
               if (!checkbox.label.empty()) {
                  if (checkbox.label_pos) {
                     // Right-sided labels can be integrated with the widget so that clicking affects state.
                     if (!build_widget(checkbox, segment, Viewport, stack_style.top(), x_advance, checkbox.label_width + checkbox.label_pad, true, wx, wy)) {
                        DOUBLE x, y;
                        auto font = stack_style.top()->get_font();
                        const DOUBLE avail_space = checkbox.final_height - font->Gutter;
                        y = avail_space - ((avail_space - font->Ascent) * 0.5);

                        x = checkbox.final_width + checkbox.label_pad;

                        if (checkbox.label_text.empty()) {
                           checkbox.label_text.set(objVectorText::create::global({
                              fl::Owner(checkbox.viewport->UID),
                              fl::String(checkbox.label),
                              fl::Font(font),
                              fl::Fill(stack_style.top()->fill)
                           }));
                        }

                        checkbox.label_text->setFields(fl::X(F2T(x)), fl::Y(F2T(y)));
                     }
                  }
                  else {
                     // Left-sided labels aren't included in the scope of the widget's viewport
                     // TODO: Interactivity is feasible but we'll need to add an input feedback mechanism for that
                     auto font = stack_style.top()->get_font();
                     auto x_label = x_advance;
                     x_advance += checkbox.label_width + checkbox.label_pos;
                     if (!build_widget(checkbox, segment, Viewport, stack_style.top(), x_advance, 0, true, wx, wy)) {
                        const DOUBLE avail_space = checkbox.final_height - font->Gutter;
                        DOUBLE y = wy + avail_space - ((avail_space - font->Ascent) * 0.5);

                        if (checkbox.label_text.empty()) {
                           checkbox.label_text.set(objVectorText::create::global({
                              fl::Owner(Viewport->UID),
                              fl::String(checkbox.label),
                              fl::Font(font),
                              fl::Fill(stack_style.top()->fill)
                           }));
                        }

                        checkbox.label_text->setFields(fl::X(F2T(x_label)), fl::Y(F2T(y)));
                     }
                  }
               }
               else build_widget(checkbox, segment, Viewport, stack_style.top(), x_advance, 0, true, wx, wy);

               if (!checkbox.processed) {
                  checkbox.processed = true;
                  if (checkbox.viewport->Scene->SurfaceID) {
                     auto call = make_function_stdc(inputevent_checkbox);
                     vecSubscribeInput(*checkbox.viewport, JTYPE::BUTTON|JTYPE::FEEDBACK, &call);
                  }
               }
               Self->Widgets.emplace(checkbox.viewport->UID, ui_widget { &checkbox });
               break;
            }

            case SCODE::COMBOBOX: {
               auto &combo = segment.stream->lookup<bc_combobox>(cursor);
               auto font = stack_style.top()->get_font();

               DOUBLE wx, wy;
               const DOUBLE avail_space = combo.final_height - font->Gutter;

               if (!combo.label.empty()) {
                  if (combo.label_pos) {
                     build_widget(combo, segment, Viewport, stack_style.top(), x_advance, 0, true, wx, wy);

                     DOUBLE y = wy + avail_space - ((avail_space - font->Ascent) * 0.5);

                     if (combo.label_text.empty()) {
                        combo.label_text = objVectorText::create::global({
                           fl::Owner(Viewport->UID),
                           fl::String(combo.label),
                           fl::Font(font),
                           fl::Fill(stack_style.top()->fill)
                        });
                     }
                     
                     combo.label_text->setFields(fl::X(F2T(x_advance + combo.label_pad)), fl::Y(F2T(y)));

                     x_advance += combo.label_width + combo.label_pad;
                  }
                  else {
                     auto x_label = x_advance;
                     x_advance += combo.label_pad + combo.label_width;

                     build_widget(combo, segment, Viewport, stack_style.top(), x_advance, 0, true, wx, wy);

                     DOUBLE y = wy + avail_space - ((avail_space - font->Ascent) * 0.5);

                     if (combo.label_text.empty()) {
                        combo.label_text = objVectorText::create::global({
                           fl::Owner(Viewport->UID),
                           fl::String(combo.label),
                           fl::Font(font),
                           fl::Fill(stack_style.top()->fill)
                        });
                     }

                     combo.label_text->setFields(fl::X(F2T(x_label)), fl::Y(F2T(y)));
                  }
               }
               else build_widget(combo, segment, Viewport, stack_style.top(), x_advance, 0, true, wx, wy);

               if (combo.clip_vp.empty()) {
                  combo.clip_vp = objVectorViewport::create::global({
                     fl::Name("vp_clip_combo"),
                     fl::Owner(combo.viewport->UID),
                     fl::Overflow(VOF::HIDDEN)
                  });
                  
                  DOUBLE y = avail_space - ((avail_space - font->Ascent) * 0.5);

                  objVectorText::create::global({
                     fl::Owner(combo.clip_vp->UID),
                     fl::X(0), fl::Y(F2T(y)),
                     fl::String(combo.value),
                     fl::Cursor(PTC::TEXT),
                     fl::Font(font),
                     fl::Fill(combo.font_fill),
                     fl::LineLimit(1),
                     fl::TextFlags(VTXF::EDITABLE)
                  });
               }

               if (!combo.clip_vp.empty()) {
                  combo.clip_vp->setFields(fl::X(combo.label_pad * 0.75), fl::Y(0), 
                     fl::XOffset(combo.label_pad + (combo.height * 0.75)), fl::YOffset(0));
               }

               break;
            }

            case SCODE::IMAGE: {
               auto &img = segment.stream->lookup<bc_image>(cursor);
               DOUBLE wx, wy;
               build_widget(img, segment, Viewport, stack_style.top(), x_advance, 0, false, wx, wy);
               break;
            }

            case SCODE::INPUT: {
               auto &input = segment.stream->lookup<bc_input>(cursor);
               auto font = stack_style.top()->get_font();

               DOUBLE wx, wy;
               const DOUBLE avail_space = input.final_height - font->Gutter;

               if (!input.label.empty()) {
                  if (input.label_pos) {
                     build_widget(input, segment, Viewport, stack_style.top(), x_advance, 0, true, wx, wy);

                     DOUBLE y = wy + avail_space - ((avail_space - font->Ascent) * 0.5);

                     if (input.label_text.empty()) {
                        input.label_text = objVectorText::create::global({
                           fl::Owner(Viewport->UID),
                           fl::String(input.label),
                           fl::Font(font),
                           fl::Fill(stack_style.top()->fill)
                        });
                     }
                     
                     input.label_text->setFields(fl::X(F2T(x_advance + input.label_pad)), fl::Y(F2T(y)));

                     x_advance += input.label_width + input.label_pad;
                  }
                  else {
                     auto x_label = x_advance;
                     x_advance += input.label_pad + input.label_width;

                     build_widget(input, segment, Viewport, stack_style.top(), x_advance, 0, true, wx, wy);

                     DOUBLE y = wy + avail_space - ((avail_space - font->Ascent) * 0.5);

                     if (input.label_text.empty()) {
                        input.label_text = objVectorText::create::global({
                           fl::Owner(Viewport->UID),
                           fl::String(input.label),
                           fl::Font(font),
                           fl::Fill(stack_style.top()->fill)
                        });
                     }

                     input.label_text->setFields(fl::X(F2T(x_label)), fl::Y(F2T(y)));
                  }
               }
               else build_widget(input, segment, Viewport, stack_style.top(), x_advance, 0, true, wx, wy);

               if (input.clip_vp.empty()) {
                  input.clip_vp = objVectorViewport::create::global({
                     fl::Name("vp_clip_input"),
                     fl::Owner(input.viewport->UID),
                     fl::Overflow(VOF::HIDDEN)
                  });
                  
                  auto flags = VTXF::EDITABLE;
                  if (input.secret) flags |= VTXF::SECRET;

                  DOUBLE y = avail_space - ((avail_space - font->Ascent) * 0.5);

                  objVectorText::create::global({
                     fl::Owner(input.clip_vp->UID),
                     fl::X(0), fl::Y(F2T(y)),
                     fl::String(input.value),
                     fl::Cursor(PTC::TEXT),
                     fl::Font(font),
                     fl::Fill(input.font_fill),
                     fl::LineLimit(1),
                     fl::TextFlags(flags)
                  });
               }

               if (!input.clip_vp.empty()) {
                  input.clip_vp->setFields(fl::X(input.label_pad), fl::Y(0), fl::XOffset(input.label_pad), fl::YOffset(0));
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
                     fl::Name("doc_text"),
                     fl::Owner(Viewport->UID),
                     fl::X(x), fl::Y(F2T(y)),
                     fl::String(str),
                     fl::Cursor(PTC::TEXT),
                     fl::Font(font),
                     fl::Fill(stack_style.top()->fill),
                     fl::TextFlags(((stack_style.top()->options & FSO::UNDERLINE) != FSO::NIL) ? VTXF::UNDERLINE : VTXF::NIL)
                  });

                  Self->UIObjects.push_back(vt->UID);

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
