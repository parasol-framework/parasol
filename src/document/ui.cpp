
//********************************************************************************************************************
// Feedback events for the combobox viewport.  Note that the viewport retains focus when the drop-down list is
// presented.

static ERROR combo_feedback(objVectorViewport *View, FM Event, OBJECTPTR EventObject, APTR Meta)
{
   auto Self = (extDocument *)CurrentContext();
   ui_widget &widget = Self->Widgets[View->UID];
   auto combo = std::get<bc_combobox *>(widget.widget);

   if (Event IS FM::LOST_FOCUS) {
      if (gfxGetUserFocus() IS combo->menu.m_surface->UID) {
         // The drop-down surface has been given the focus - so don't hide it.
      }
      else {
         combo->menu.hide();
      }
   }
   else if ((Event IS FM::HAS_FOCUS) or (Event IS FM::CHILD_HAS_FOCUS)) {
      combo->last_good_input = combo->input->get<CSTRING>(FID_String);
   }

   View->draw();
   return ERR_Okay;
}

//********************************************************************************************************************
// Callback for the user selecting an item from the drop-down list.

void bc_combobox::callback(struct doc_menu &Menu, struct dropdown_item &Item)
{
   auto combo = std::get<bc_combobox *>(Menu.m_ref);
   if (combo) {
      if (!Item.value.empty()) combo->input->setFields(fl::String(Item.value));
      else combo->input->setFields(fl::String(Item.content));
      combo->viewport->draw();
   }
}

//********************************************************************************************************************

static bool delete_selected(extDocument *Self)
{
   if ((Self->SelectIndex.valid()) and (Self->SelectIndex != Self->CursorIndex)) {
      auto start = Self->SelectIndex;
      auto end = Self->CursorIndex;
      if (start > end) std::swap(start, end);

      if (start.offset > 0) {
         if (Self->Stream[start.index].code IS SCODE::TEXT) {
            auto &text = Self->Stream.lookup<bc_text>(start);
            if (start.index IS end.index) text.text.erase(start.offset, end.offset - start.offset);
            else text.text.erase(start.offset, text.text.size() - start.offset);
         }
         start.index++;
         start.offset = 0;
      }

      if (start.index < end.index) {
         Self->Stream.data.erase(Self->Stream.data.begin() + start.index, Self->Stream.data.begin() + (end.index - start.index));
         end.index -= (end.index - start.index);

         if ((end.offset > 0) and (Self->Stream[end.index].code IS SCODE::TEXT)) {
            auto &text = Self->Stream.lookup<bc_text>(end);
            text.text.erase(0, end.offset);
         }
      }

      Self->CursorIndex = Self->SelectIndex;
      Self->SelectIndex.reset();
      return true;
   }
   return false;
}

//********************************************************************************************************************

static ERROR key_event(objVectorViewport *Viewport, KQ Flags, KEY Value, LONG Unicode)
{
   pf::Log log(__FUNCTION__);
   struct acScroll scroll;

   if ((Flags & KQ::PRESSED) IS KQ::NIL) return ERR_Okay;

   auto Self = (extDocument *)CurrentContext();

   log.function("Value: %d, Flags: $%.8x, ActiveEdit: %p", LONG(Value), LONG(Flags), Self->ActiveEditDef);

   if ((Self->ActiveEditDef) and ((Self->Page->Flags & VF::HAS_FOCUS) IS VF::NIL)) {
      deactivate_edit(Self, true);
   }

   if (Self->ActiveEditDef) {
      reset_cursor(Self);

      if (Unicode) {
         delete_selected(Self);

         // Output the character

         char string[12];
         UTF8WriteValue(Unicode, string, sizeof(string));
         docInsertText(Self, string, Self->CursorIndex.index, Self->CursorIndex.offset, true); // Will set UpdatingLayout to true
         Self->CursorIndex += StrLength(string); // Reposition the cursor

         layout_doc_fast(Self);

         resolve_fontx_by_index(Self, Self->CursorIndex, Self->CursorCharX);

         Self->Viewport->draw();
         return ERR_Okay;
      }

      switch(Value) {
         case KEY::TAB: {
            log.branch("Key: Tab");
            if (Self->TabFocusID) acFocus(Self->TabFocusID);
            else if ((Flags & KQ::SHIFT) != KQ::NIL) advance_tabfocus(Self, -1);
            else advance_tabfocus(Self, 1);
            break;
         }

         case KEY::ENTER: {
            delete_selected(Self);

            insert_text(Self, &Self->Stream, Self->CursorIndex, "\n", true);
            Self->CursorIndex.next_char(Self->Stream);

            layout_doc_fast(Self);
            resolve_fontx_by_index(Self, Self->CursorIndex, Self->CursorCharX);
            Self->Viewport->draw();
            break;
         }

         case KEY::LEFT: {
            Self->SelectIndex.reset();
            if (Self->Stream[Self->CursorIndex.index].code IS SCODE::CELL) {
               // Cursor cannot be moved any further left.  The cursor index should never end up here, but
               // better to be safe than sorry.

            }
            else {
               for (auto index = Self->CursorIndex; index.index > 0; ) {
                  index.prev_char(Self->Stream);

                  auto code = Self->Stream[index.index].code;
                  if (code IS SCODE::CELL) {
                     auto &cell = Self->Stream.lookup<bc_cell>(index);
                     if (cell.cell_id IS Self->ActiveEditCellID) break;
                  }
                  else if (code IS SCODE::IMAGE); // Inline images count as a character
                  else if (code != SCODE::TEXT) continue;

                  if (!resolve_fontx_by_index(Self, index, Self->CursorCharX)) {
                     Self->CursorIndex = index;
                     Self->Viewport->draw();
                     log.warning("LeftCursor: %d, X: %g", Self->CursorIndex.index, Self->CursorCharX);
                  }
                  break;
               }
            }
            break;
         }

         case KEY::RIGHT: {
            Self->SelectIndex.reset();

            auto index = Self->CursorIndex;
            while (index < INDEX(Self->Stream.size())) {
               auto code = Self->Stream[index.index].code;
               if (code IS SCODE::IMAGE); // Inline images are treated as content, so do nothing special for these and drop through to next section
               else {
                  index.next_char(Self->Stream);
                  continue;
               }

               // The current index references a content character or object.  Advance the cursor to the next index.

               index.next_char(Self->Stream);
               if (!resolve_fontx_by_index(Self, index, Self->CursorCharX)) {
                  Self->CursorIndex = index;
                  Self->Viewport->draw();
                  log.warning("RightCursor: %d, X: %g", Self->CursorIndex.index, Self->CursorCharX);
               }
               break;
            }
            break;
         }

         case KEY::HOME: {
            break;
         }

         case KEY::END: {
            break;
         }

         case KEY::UP:
            break;

         case KEY::DOWN:
            break;

         case KEY::BACKSPACE: {
            if (Self->Stream[Self->CursorIndex.index].code IS SCODE::CELL) {
               // Cursor cannot be moved any further left
            }
            else {
               auto index = Self->CursorIndex;
               index.prev_char(Self->Stream);

               if (Self->Stream[index.index].code IS SCODE::CELL);
               else {
                  if (!delete_selected(Self)) {
                     // Delete the character/escape code
                     Self->CursorIndex = index;
                     Self->CursorIndex.erase_char(Self->Stream);
                  }

                  Self->UpdatingLayout = true;
                  layout_doc_fast(Self);
                  resolve_fontx_by_index(Self, Self->CursorIndex, Self->CursorCharX);
                  Self->Viewport->draw();
               }
            }
            break;
         }

         case KEY::DELETE: {
            if (!delete_selected(Self)) {
               Self->CursorIndex.erase_char(Self->Stream);
            }
            Self->UpdatingLayout = true;
            layout_doc_fast(Self);
            resolve_fontx_by_index(Self, Self->CursorIndex, Self->CursorCharX);
            Self->Viewport->draw();
            break;
         }

         default: break; // Ignore unhandled codes
      }
   }
   else switch (Value) {
      // NB: When not in edit mode, only the navigation keys are enabled
      case KEY::TAB:
         log.branch("Key: Tab");
         if (Self->TabFocusID) acFocus(Self->TabFocusID);
         else if ((Flags & KQ::SHIFT) != KQ::NIL) advance_tabfocus(Self, -1);
         else advance_tabfocus(Self, 1);
         break;

      case KEY::ENTER: {
         auto tab = Self->FocusIndex;
         if ((tab >= 0) and (tab < std::ssize(Self->Tabs))) {
            log.branch("Key: Enter, Tab: %d/%d, Type: %d", tab, LONG(Self->Tabs.size()), LONG(Self->Tabs[tab].type));

            if ((Self->Tabs[tab].type IS TT::LINK) and (Self->Tabs[tab].active)) {
               for (auto &link : Self->Links) {
                  if (link.origin.id IS Self->Tabs[tab].ref) {
                     link.exec(Self);
                     break;
                  }
               }
            }
         }
         break;
      }

      case KEY::PAGE_DOWN:
         scroll = { 0, Self->VPHeight, 0 };
         QueueAction(AC_Scroll, Self->Viewport->UID, &scroll);
         break;

      case KEY::PAGE_UP:
         scroll = { 0, -Self->VPHeight, 0 };
         QueueAction(AC_Scroll, Self->Viewport->UID, &scroll);
         break;

      case KEY::LEFT:
         scroll = { -10, 0, 0 };
         QueueAction(AC_Scroll, Self->Viewport->UID, &scroll);
         break;

      case KEY::RIGHT:
         scroll = { 10, 0, 0 };
         QueueAction(AC_Scroll, Self->Viewport->UID, &scroll);
         break;

      case KEY::DOWN:
         scroll = { 0, 10, 0 };
         QueueAction(AC_Scroll, Self->Viewport->UID, &scroll);
         break;

      case KEY::UP:
         scroll = { 0, -10, 0 };
         QueueAction(AC_Scroll, Self->Viewport->UID, &scroll);
         break;

      default: break; // Ignore unhandled codes
   }

   return ERR_Okay;
}

//********************************************************************************************************************

static void error_dialog(const std::string Title, const std::string Message)
{
   pf::Log log(__FUNCTION__);

   log.warning("%s", Message.c_str());

#if !(defined(DBG_LAYOUT) || defined(DBG_STREAM) || defined(DBG_SEGMENTS))
   static bool detect_recursive_dialog = false;
   static OBJECTID dialog_id = 0;
   if ((dialog_id) and (CheckObjectExists(dialog_id) IS ERR_True)) return;
   if (detect_recursive_dialog) return;
   detect_recursive_dialog = true;

   OBJECTPTR dialog;
   if (!NewObject(ID_SCRIPT, &dialog)) {
      dialog->setFields(fl::Name("scDialog"), fl::Owner(CurrentTaskID()), fl::Path("scripts:gui/dialog.fluid"));

      acSetVar(dialog, "modal", "1");
      acSetVar(dialog, "title", Title.c_str());
      acSetVar(dialog, "options", "okay");
      acSetVar(dialog, "type", "error");
      acSetVar(dialog, "message", Message.c_str());

      if ((!InitObject(dialog)) and (!acActivate(dialog))) {
         CSTRING *results;
         LONG size;
         if ((!GetFieldArray(dialog, FID_Results, (APTR *)&results, &size)) and (size > 0)) {
            dialog_id = StrToInt(results[0]);
         }
      }
   }

   detect_recursive_dialog = false;
#endif
}

static void error_dialog(const std::string Title, ERROR Error)
{
   if (auto errstr = GetErrorMsg(Error)) {
      std::string buffer("Error: ");
      buffer.append(errstr);
      error_dialog(Title, buffer);
   }
}

//********************************************************************************************************************

static ERROR activate_cell_edit(extDocument *Self, INDEX CellIndex, stream_char CursorIndex)
{
   pf::Log log(__FUNCTION__);

   if ((CellIndex < 0) or (CellIndex >= INDEX(Self->Stream.size()))) return log.warning(ERR_OutOfRange);

   log.branch("Cell Index: %d, Cursor Index: %d", CellIndex, CursorIndex.index);

   if (Self->Stream[CellIndex].code != SCODE::CELL) { // Sanity check
      return log.warning(ERR_Failed);
   }

   auto &cell = Self->Stream.lookup<bc_cell>(CellIndex);
   if (CursorIndex.index <= CellIndex) { // Go to the start of the cell content
      CursorIndex.set(CellIndex + 1);
   }

   auto &stream = cell.stream;

   if (stream->data[CursorIndex.index].code != SCODE::TEXT) {
      // Skip ahead to the first relevant control code - it's always best to place the cursor ahead of things like
      // font styles, paragraph formatting etc.

      CursorIndex.offset = 0;
      while (CursorIndex.index < INDEX(Self->Stream.size())) {
         std::array<SCODE, 5> content = {
            SCODE::TABLE_START, SCODE::LINK_END, SCODE::IMAGE, SCODE::PARAGRAPH_END, SCODE::TEXT
         };
         if (std::find(std::begin(content), std::end(content), stream->data[CursorIndex.index].code) != std::end(content)) break;
         CursorIndex.next_code();
      }
   }

   auto it = Self->EditDefs.find(cell.edit_def);
   if (it IS Self->EditDefs.end()) return log.warning(ERR_Search);

   deactivate_edit(Self, false);

   auto &edit = it->second;
   cell.modified = false;

   Self->ActiveEditCellID = cell.cell_id;
   Self->ActiveEditDef    = &edit;
   Self->CursorIndex      = CursorIndex;
   Self->SelectIndex.reset();

   log.msg("Activated cell %d, cursor index %d, EditDef: %p, CRC: $%.8x",
      Self->ActiveEditCellID, Self->CursorIndex.index, Self->ActiveEditDef, Self->ActiveEditCRC);

   // Set the focus index to the relevant TT::EDIT entry

   for (unsigned tab=0; tab < Self->Tabs.size(); tab++) {
      if ((Self->Tabs[tab].type IS TT::EDIT) and (Self->Tabs[tab].ref IS cell.cell_id)) {
         Self->FocusIndex = tab;
         break;
      }
   }

   resolve_fontx_by_index(Self, Self->CursorIndex, Self->CursorCharX);

   reset_cursor(Self); // Reset cursor flashing

   // User callbacks

   if (!edit.on_enter.empty()) {
      objScript *script;
      std::string function_name, argstring;

      log.msg("Calling onenter callback function.");

      if (!extract_script(Self, edit.on_enter, &script, function_name, argstring)) {
         ScriptArg args[] = { { "ID", edit.name } };
         scExec(script, function_name.c_str(), args, ARRAYSIZE(args));
      }
   }

   Self->Viewport->draw();
   return ERR_Okay;
}

//********************************************************************************************************************

static void deactivate_edit(extDocument *Self, bool Redraw)
{
   pf::Log log(__FUNCTION__);

   if (!Self->ActiveEditDef) return;

   log.branch("Redraw: %d, CellID: %d", Redraw, Self->ActiveEditCellID);

   if (Self->FlashTimer) {
      UpdateTimer(Self->FlashTimer, 0); // Turn off the timer
      Self->FlashTimer = 0;
   }

   // The edit tag needs to be found so that we can determine if on_exit needs to be called or not.

   auto edit = Self->ActiveEditDef;
   LONG cell_index = Self->Stream.find_cell(Self->ActiveEditCellID);

   Self->ActiveEditCellID = 0;
   Self->ActiveEditDef = NULL;
   Self->CursorIndex.reset();
   Self->SelectIndex.reset();

   if (Redraw) Self->Viewport->draw();

   if (cell_index >= 0) {
      if (!edit->on_change.empty()) {
         bc_cell &cell = Self->Stream.lookup<bc_cell>(cell_index);

         if (cell.modified) {
            log.trace("Change detected in editable cell %d", cell.cell_id);

            objScript *script;
            std::string function_name, argstring;
            if (!extract_script(Self, edit->on_change, &script, function_name, argstring)) {
               auto cell_content = cell_index;
               cell_content++;

               std::vector<ScriptArg> args = {
                  ScriptArg("CellID", edit->name)
                  //ScriptArg("Start", cell_content),
                  //ScriptArg("End", i)
               };

               for (auto &cell_arg : cell.args) args.emplace_back("", cell_arg.second);

               scExec(script, function_name.c_str(), args.data(), args.size());
            }
         }
      }

      if (!edit->on_exit.empty()) {



      }
   }
   else log.warning("Failed to find cell ID %d", Self->ActiveEditCellID);
}

//********************************************************************************************************************
// Sends motion events for zones that the mouse pointer has departed.

static void check_pointer_exit(extDocument *Self, LONG X, LONG Y)
{
   for (auto it = Self->MouseOverChain.begin(); it != Self->MouseOverChain.end(); ) {
      if ((X < it->left) or (Y < it->top) or (X >= it->right) or (Y >= it->bottom)) {
         // Pointer has left this zone

         std::string function_name, argstring;
         objScript *script;
         if (!extract_script(Self, it->function, &script, function_name, argstring)) {
            const ScriptArg args[] = {
               { "Element", it->element_id },
               { "Status",  0 },
               { "Args",    argstring }
            };

            scExec(script, function_name.c_str(), args, ARRAYSIZE(args));
         }

         it = Self->MouseOverChain.erase(it);
      }
      else it++;
   }
}

//********************************************************************************************************************
// TODO: This code needs to utilise cell viewports for managing UI interactivity.

static void check_mouse_click(extDocument *Self, DOUBLE X, DOUBLE Y)
{
   pf::Log log(__FUNCTION__);

   Self->ClickX = X;
   Self->ClickY = Y;
   Self->ClickSegment = Self->MouseOverSegment;

   auto segment = Self->MouseOverSegment;

   if (segment IS -1) {
      // The mouse is not positioned over a segment.  Check if the mosue is positioned within
      // an editing cell.  If it is, we need to find the segment nearest to the mouse pointer
      // and position the cursor at the end of that segment.

      unsigned i;
      for (i=0; i < Self->EditCells.size(); i++) {
         if ((X >= Self->EditCells[i].x) and (X < Self->EditCells[i].x + Self->EditCells[i].width) and
             (Y >= Self->EditCells[i].y) and (Y < Self->EditCells[i].y + Self->EditCells[i].height)) {
            break;
         }
      }
      /*
      if (i < Self->EditCells.size()) {
         // Mouse is within an editable segment.

         INDEX cell_start = Self->Stream.find_cell(Self->EditCells[i].cell_id);

         SEGINDEX last_segment = -1;
         auto &ss = Self->get_sorted_segments();
         for (unsigned sortseg=0; sortseg < ss.size(); sortseg++) {
            SEGINDEX seg = ss[sortseg].segment;
            if ((Self->Segments[seg].start.index >= cell_start) and (Self->Segments[seg].stop.index <= cell_end)) {
               last_segment = seg;
               // Segment found.  Break if the segment's vertical position is past the mouse pointer
               if (Y < Self->Segments[seg].area.Y) break;
               if ((Y >= Self->Segments[seg].area.Y) and (X < Self->Segments[seg].area.X)) break;
            }
         }

         if (last_segment != -1) {
            // Set the cursor to the end of the nearest segment
            log.warning("Last seg: %d", last_segment);
            Self->CursorCharX = Self->Segments[last_segment].area.X + Self->Segments[last_segment].area.Width;
            Self->SelectCharX = Self->CursorCharX;

            // A click results in the deselection of existing text

            if (Self->CursorIndex.valid()) deselect_text(Self);

            Self->CursorIndex = Self->Segments[last_segment].stop;
            Self->SelectIndex.reset(); //Self->Segments[last_segment].stop;

            activate_cell_edit(Self, cell_start, Self->CursorIndex);
         }

         return;
      }
      else log.warning("Mouse not within an editable cell.");
      */
   }

   if (segment != -1) {
      stream_char sc;
      if (!resolve_font_pos(Self->Segments[segment], X, Self->CursorCharX, sc)) {
         if (Self->CursorIndex.valid()) deselect_text(Self); // A click results in the deselection of existing text

         if (!Self->Segments[segment].edit) deactivate_edit(Self, true);

         // Set the new cursor information

         Self->CursorIndex = sc;
         Self->SelectIndex.reset(); //sc; // SelectIndex is for text selections where the user holds the LMB and drags the mouse
         Self->SelectCharX = Self->CursorCharX;

         log.msg("User clicked on point %gx%g in segment %d, cursor index: %d, char x: %g", X, Y, segment, Self->CursorIndex.index, Self->CursorCharX);

         if (Self->Segments[segment].edit) {
            // If the segment is editable, we'll have to turn on edit mode so
            // that the cursor flashes.  Work backwards to find the edit cell.

            for (auto cellindex = Self->Segments[segment].start; cellindex.valid(); cellindex.prev_code()) {
               if (Self->Stream[cellindex.index].code IS SCODE::CELL) {
                  auto &cell = Self->Stream.lookup<bc_cell>(cellindex);
                  if (!cell.edit_def.empty()) {
                     activate_cell_edit(Self, cellindex.index, Self->CursorIndex);
                     break;
                  }
               }
            }
         }
      }
   }
   else if (Self->CursorIndex.valid()) {
      deselect_text(Self);
      deactivate_edit(Self, true);
   }
}

//********************************************************************************************************************

static void check_mouse_release(extDocument *Self, DOUBLE X, DOUBLE Y)
{
   if ((std::abs(X - Self->ClickX) > 3) or (std::abs(Y - Self->ClickY) > 3)) {
      pf::Log log(__FUNCTION__);
      log.trace("User click cancelled due to mouse shift.");
      return;
   }
}

//********************************************************************************************************************

static void check_mouse_pos(extDocument *Self, DOUBLE X, DOUBLE Y)
{
   Self->MouseOverSegment = -1;
   Self->PointerX = X;
   Self->PointerY = Y;

   check_pointer_exit(Self, X, Y); // For function callbacks

   if (Self->MouseInPage) {
      unsigned row;
      auto &ss = Self->get_sorted_segments();

      for (row=0; (row < ss.size()) and (Y < ss[row].y); row++);

      for (; row < ss.size(); row++) {
         if ((Y >= ss[row].y) and (Y < ss[row].y + Self->Segments[ss[row].segment].area.Height)) {
            if ((X >= Self->Segments[ss[row].segment].area.X) and (X < Self->Segments[ss[row].segment].area.X + Self->Segments[ss[row].segment].area.Width)) {
               Self->MouseOverSegment = ss[row].segment;
               break;
            }
         }
      }
   }

   // If the user is holding the mouse button and moving it around, we need to highlight the selected text.

   if ((Self->LMB) and (Self->CursorIndex.valid())) {
      if (!Self->SelectIndex.valid()) Self->SelectIndex = Self->CursorIndex;

      if (Self->MouseOverSegment != -1) {
         DOUBLE cursor_x;
         stream_char cursor_index;
         if (!resolve_font_pos(Self->Segments[Self->MouseOverSegment], X, cursor_x, cursor_index)) {
            if (Self->ActiveEditDef) {
               // For select-dragging, we must check that the selection is within the bounds of the editing area.

               if (INDEX cell_index = Self->Stream.find_cell(Self->ActiveEditCellID); cell_index >= 0) {
                  INDEX i = cell_index++;
                  if (cursor_index.index < i) {
                     // If the cursor index precedes the start of the editing area, reset it

                     cursor_index.set(i);
                     if (!resolve_fontx_by_index(Self, cursor_index, cursor_x)) {

                     }
                  }
                  else {
                     // If the cursor index is past the end of the editing area, reset it
                     /*
                     while (i < INDEX(Self->Stream.size())) {
                        if (Self->Stream[i].code IS SCODE::CELL_END) {
                           auto &cell_end = Self->Stream.lookup<bc_cell_end>(i);
                           if (cell_end.cell_id IS Self->ActiveEditCellID) {
                              stream_char sc(i, 0);
                              if (auto seg = find_segment(Self->Segments, sc, false); seg > 0) {
                                 seg--;
                                 sc = Self->Segments[seg].stop;
                                 if (cursor_index > sc) {
                                    if (!resolve_fontx_by_index(Self, sc, cursor_x)) {
                                       cursor_index = sc;
                                    }
                                 }
                              }
                              break;
                           }
                        }
                        i++;
                     }
                     */
                  }

                  Self->CursorIndex = cursor_index;
                  Self->CursorCharX = cursor_x;
               }
               else deactivate_edit(Self, false);
            }
            else {
               Self->CursorIndex = cursor_index;
               Self->CursorCharX = cursor_x;
            }

            Self->Viewport->draw();
         }
      }
   }
}

//********************************************************************************************************************
// The text will be deselected, but the cursor and editing area will remain active.

static void deselect_text(extDocument *Self)
{
   if (Self->CursorIndex IS Self->SelectIndex) return; // Nothing to deselect
   Self->SelectIndex.reset();
   Self->Viewport->draw();
}

//********************************************************************************************************************

static LONG find_tabfocus(extDocument *Self, TT Type, LONG Reference)
{
   for (unsigned i=0; i < Self->Tabs.size(); i++) {
      if ((Self->Tabs[i].type IS Type) and (Reference IS Self->Tabs[i].ref)) return i;
   }
   return -1;
}

//********************************************************************************************************************
// This function is used in tags.c by the link and object insertion code.

static LONG add_tabfocus(extDocument *Self, TT Type, LONG Reference)
{
   pf::Log log(__FUNCTION__);

   //log.branch("Type: %d, Ref: %d", LONG(Type), Reference);

   if (Type IS TT::LINK) { // For TT::LINK types, check that the link isn't already registered
      for (unsigned i=0; i < Self->Tabs.size(); i++) {
         if ((Self->Tabs[i].type IS TT::LINK) and (Self->Tabs[i].ref IS Reference)) {
            return i;
         }
      }
   }

   auto index = std::ssize(Self->Tabs);
   Self->Tabs.emplace_back(Type, Reference, Self->Invisible ^ 1);
   return index;
}

//********************************************************************************************************************
// Input events received for hyperlinks.

static ERROR link_callback(objVector *Vector, InputEvent *Event)
{
   pf::Log log(__FUNCTION__);

   auto Self = (extDocument *)CurrentContext();

   ui_link *link = NULL;
   for (unsigned i=0; i < Self->Links.size(); i++) {
      if (*Self->Links[i].origin.path IS Vector) {
         link = &Self->Links[i];
         break;
      }
   }

   if (!link) {
      log.warning("Failed to relate vector #%d to a hyperlink.", Vector->UID);
      return ERR_Okay;
   }

   objScript *script;
   std::string argstring, func_name;

   if ((Event->Flags & JTYPE::MOVEMENT) != JTYPE::NIL) {
      if (!link->origin.on_motion.empty()) {
         if (!extract_script(Self, link->origin.on_motion, &script, func_name, argstring)) {
            const ScriptArg args[] = { { "Element", link->origin.id }, { "Status", 1 }, { "Args", argstring } };
            scExec(script, func_name.c_str(), args, ARRAYSIZE(args));
         }
      }
   }
   else if (Event->Type IS JET::ENTERED_AREA) {
      link->hover = true;
      if (!link->origin.on_motion.empty()) {
         if (!extract_script(Self, link->origin.on_motion, &script, func_name, argstring)) {
            const ScriptArg args[] = { { "Element", link->origin.id }, { "Status", 1 }, { "Args", argstring } };
            scExec(script, func_name.c_str(), args, ARRAYSIZE(args));
         }
      }

      for (auto cursor=link->cursor_start; cursor < link->cursor_end; cursor.next_code()) {
         if (link->stream[0][cursor.index].code IS SCODE::TEXT) {
            auto &txt = link->stream[0].lookup<bc_text>(cursor);
            for (auto vt : txt.vector_text) vt->setFill(Self->LinkSelectFill);
         }
      }

      Self->Viewport->draw();
   }
   else if (Event->Type IS JET::LEFT_AREA) {
      link->hover = false;
      if (!link->origin.on_motion.empty()) {
         if (!extract_script(Self, link->origin.on_motion, &script, func_name, argstring)) {
            const ScriptArg args[] = { { "Element", link->origin.id }, { "Status", 1 }, { "Args", argstring } };
            scExec(script, func_name.c_str(), args, ARRAYSIZE(args));
         }
      }

      for (auto cursor=link->cursor_start; cursor < link->cursor_end; cursor.next_code()) {
         if (link->stream[0][cursor.index].code IS SCODE::TEXT) {
            auto &txt = link->stream[0].lookup<bc_text>(cursor);
            for (auto vt : txt.vector_text) vt->setFill(link->origin.fill);
         }
      }

      Self->Viewport->draw();
   }
   else if ((Event->Flags & JTYPE::BUTTON) != JTYPE::NIL) {
      if (Event->Value IS 0) link->exec(Self);
   }
   else log.warning("Unknown event type %d for input vector %d", LONG(Event->Type), Vector->UID);

   return ERR_Okay;
}

//********************************************************************************************************************
// Changes the focus to an object or link in the document.  The new index is stored in the FocusIndex field.  If the
// Index is set to -1, set_focus() will focus on the first element, but only if it is an object.

static void set_focus(extDocument *Self, INDEX Index, CSTRING Caller)
{
   pf::Log log(__FUNCTION__);

   if (Self->Tabs.empty()) return;

   if ((Index < -1) or (Index >= std::ssize(Self->Tabs))) {
      log.traceWarning("Index %d out of bounds.", Index);
      return;
   }

   log.branch("Index: %d/%d, Type: %d, Ref: %d, HaveFocus: %d, Caller: %s", Index, LONG(Self->Tabs.size()),
      Index != -1 ? LONG(Self->Tabs[Index].type) : -1, Index != -1 ? Self->Tabs[Index].ref : -1, Self->HasFocus, Caller);

   if (Self->ActiveEditDef) deactivate_edit(Self, true);

   if (Index IS -1) {
      Index = 0;
      Self->FocusIndex = 0;
      if (Self->Tabs[0].type IS TT::LINK) {
         log.msg("First focusable element is a link - focus unchanged.");
         return;
      }
   }

   if (!Self->Tabs[Index].active) {
      log.warning("Tab marker %d is not active.", Index);
      return;
   }

   Self->FocusIndex = Index;

   if (Self->Tabs[Index].type IS TT::EDIT) {
      acFocus(Self->Page);

      if (auto cell_index = Self->Stream.find_cell(Self->Tabs[Self->FocusIndex].ref); cell_index >= 0) {
         activate_cell_edit(Self, cell_index, stream_char());
      }
   }
   else if (Self->Tabs[Index].type IS TT::VECTOR) {
      if (Self->HasFocus) {
         CLASSID class_id = GetClassID(Self->Tabs[Index].ref);
         OBJECTPTR input;
         if (class_id IS ID_VECTORTEXT) {
            if (!AccessObject(Self->Tabs[Index].ref, 1000, &input)) {
               acFocus(input);
               //if ((input->getPtr(FID_UserInput, &text) IS ERR_Okay) and (text)) {
               //   txtSelectArea(text, 0,0, 200000, 200000);
               //}
               ReleaseObject(input);
            }
         }
         else acFocus(Self->Tabs[Index].ref);
      }
   }
   else if (Self->Tabs[Index].type IS TT::LINK) {
      if (Self->HasFocus) { // Scroll to the link if it is out of view, or redraw the display if it is not.
         for (unsigned i=0; i < Self->Links.size(); i++) {
            auto &link = Self->Links[i];
            if (link.origin.id IS Self->Tabs[Index].ref) {
               DOUBLE link_x = 0, link_y = 0, link_width = 0, link_height = 0;
               for (++i; i < Self->Links.size(); i++) {
                  if (link.origin.id IS Self->Tabs[Index].ref) {
                     vecGetBoundary(*link.origin.path, VBF::NIL, &link_x, &link_y, &link_width, &link_height);
                  }
               }

               view_area(Self, link_x, link_y, link_x + link_width, link_y + link_height);
               break;
            }
         }

         Self->Viewport->draw();
         acFocus(Self->Page);
      }
   }
}

//********************************************************************************************************************
// Scrolls any given area of the document into view.

static bool view_area(extDocument *Self, DOUBLE Left, DOUBLE Top, DOUBLE Right, DOUBLE Bottom)
{
   pf::Log log(__FUNCTION__);

   DOUBLE hgap = Self->VPWidth * 0.1, vgap = Self->VPHeight * 0.1;
   DOUBLE view_x = -Self->XPosition, view_y = -Self->YPosition;
   DOUBLE view_height = Self->VPHeight, view_width  = Self->VPWidth;

   log.trace("View: %dx%d,%dx%d Link: %gx%g,%gx%g", view_x, view_y, view_width, view_height, Left, Top, Right, Bottom);

   // Vertical

   if (Self->PageHeight > Self->VPHeight) {
      if (Top < view_y + vgap) {
         view_y = Top - vgap;
         if (view_y < view_height * 0.25) view_y = 0;

         if ((Bottom < view_height - vgap) and (-Self->YPosition > view_height)) {
            view_y = 0;
         }
      }
      else if (Bottom > view_y + view_height - vgap) {
         view_y = Bottom + vgap - view_height;
         if (view_y > Self->PageHeight - view_height - (view_height * 0.25)) view_y = Self->PageHeight - view_height;
      }
   }
   else view_y = 0;

   // Horizontal

   if (Self->CalcWidth > Self->VPWidth) {
      if (Left < view_x + hgap) {
         view_x = Left - hgap;
         if (view_x < 0) view_x = 0;
      }
      else if (Right > view_x + view_width - hgap) {
         view_x = Right + hgap - view_width;
         if (view_x > Self->CalcWidth - view_width) view_x = Self->CalcWidth - view_width;
      }
   }
   else view_x = 0;

   if ((-view_x != Self->XPosition) or (-view_y != Self->YPosition)) {
      acScrollToPoint(Self, view_x, view_y, 0, STP::X|STP::Y);
      return true;
   }
   else return false;
}

//********************************************************************************************************************

static void advance_tabfocus(extDocument *Self, BYTE Direction)
{
   pf::Log log(__FUNCTION__);

   if (Self->Tabs.empty()) return;

   // Check that the FocusIndex is accurate (it may have changed if the user clicked on a gadget).

   OBJECTID currentfocus = gfxGetUserFocus();
   for (unsigned i=0; i < Self->Tabs.size(); i++) {
      if (Self->Tabs[i].ref IS currentfocus) {
         Self->FocusIndex = i;
         break;
      }
   }

   log.function("Direction: %d, Current Index: %d", Direction, Self->FocusIndex);

   if (Self->FocusIndex < 0) {
      // FocusIndex may be -1 to indicate nothing is selected, so we'll have to start from the first focusable index in that case.

      if (Direction IS -1) Self->FocusIndex = 1; // Future --
      else Self->FocusIndex = -1; // Future ++
   }

   // Advance the focus index.  Operates as a loop so that disabled surfaces can be skipped.

   auto i = std::ssize(Self->Tabs); // This while loop is designed to stop if no tab indexes are found to be active
   while (i > 0) {
      i--;

      if (Direction IS -1) {
         Self->FocusIndex--;
         if (Self->FocusIndex < 0) Self->FocusIndex = Self->Tabs.size() - 1;
      }
      else {
         Self->FocusIndex++;
         if (Self->FocusIndex >= LONG(Self->Tabs.size())) Self->FocusIndex = 0;
      }

      if (!Self->Tabs[Self->FocusIndex].active) continue;

      if (Self->Tabs[Self->FocusIndex].type IS TT::VECTOR) {
         /*
         SURFACEINFO *info;
         if (!gfxGetSurfaceInfo(Self->Tabs[Self->FocusIndex].ref, &info)) {
            if ((info->Flags & RNF::DISABLED) != RNF::NIL) continue;
         }
         */
      }
      break;
   }

   if (i >= 0) set_focus(Self, Self->FocusIndex, "adv_tabfocus");
}

//********************************************************************************************************************
// Obsoletion of the old scrollbar code means that we should be adjusting page size only and let the scrollbars
// automatically adjust in the background.

static void calc_scroll(extDocument *Self) __attribute__((unused));
static void calc_scroll(extDocument *Self)
{
   pf::Log log(__FUNCTION__);

   log.traceBranch("PageHeight: %d/%g, PageWidth: %g/%g, XPos: %g, YPos: %g",
      Self->PageHeight, Self->VPHeight, Self->CalcWidth, Self->VPWidth, Self->XPosition, Self->YPosition);
}

//********************************************************************************************************************

static ERROR flash_cursor(extDocument *Self, LARGE TimeElapsed, LARGE CurrentTime)
{
   Self->CursorState ^= 1;

   Self->Viewport->draw();
   return ERR_Okay;
}

//********************************************************************************************************************

static void reset_cursor(extDocument *Self)
{
   pf::Log log(__FUNCTION__);

   log.function("");

   Self->CursorState = 1;
   if (Self->FlashTimer) UpdateTimer(Self->FlashTimer, 0.5);
   else SubscribeTimer(0.5, FUNCTION(flash_cursor), &Self->FlashTimer);
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
// Incoming events from the button viewport

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

static ERROR inputevent_dropdown(objVectorViewport *Viewport, const InputEvent *Event)
{
   pf::Log log;
   auto Self = (extDocument *)CurrentContext();

   if (!Self->Widgets.contains(Viewport->UID)) return ERR_Terminate;

   ui_widget &widget = Self->Widgets[Viewport->UID];
   auto &combo = *std::get<bc_combobox *>(widget.widget);

   handle_widget_event(Self, combo, Event);

   for (; Event; Event = Event->Next) {
      if ((Event->Flags & JTYPE::BUTTON) != JTYPE::NIL) {
         combo.menu.create(combo.width);

         if (Event->Type IS JET::LMB) {

            if (Event->Value IS 1) {
               combo.menu.toggle(*combo.viewport);
            }
         }


         Self->Viewport->draw();
      }
   }

   return ERR_Okay;
}

//********************************************************************************************************************
// Incoming events from the checkbox viewport

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
