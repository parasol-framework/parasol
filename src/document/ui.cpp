
//********************************************************************************************************************

static bool delete_selected(extDocument *Self)
{
   if ((Self->SelectIndex.valid()) and (Self->SelectIndex != Self->CursorIndex)) {
      auto start = Self->SelectIndex;
      auto end = Self->CursorIndex;
      if (start > end) std::swap(start, end);

      if (start.Offset > 0) {
         if (Self->Stream[start.Index].Code IS ESC::TEXT) {
            auto &text = escape_data<bcText>(Self, start);
            if (start.Index IS end.Index) text.Text.erase(start.Offset, end.Offset - start.Offset);
            else text.Text.erase(start.Offset, text.Text.size() - start.Offset);
         }
         start.Index++;
         start.Offset = 0;
      }

      if (start.Index < end.Index) {
         Self->Stream.erase(Self->Stream.begin() + start.Index, Self->Stream.begin() + (end.Index - start.Index));
         end.Index -= (end.Index - start.Index);

         if ((end.Offset > 0) and (Self->Stream[end.Index].Code IS ESC::TEXT)) {
            auto &text = escape_data<bcText>(Self, end);
            text.Text.erase(0, end.Offset);
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
         docInsertText(Self, string, Self->CursorIndex.Index, Self->CursorIndex.Offset, true); // Will set UpdatingLayout to true
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

            insert_text(Self, Self->CursorIndex, "\n", true);
            Self->CursorIndex.nextChar(Self, Self->Stream);

            layout_doc_fast(Self);
            resolve_fontx_by_index(Self, Self->CursorIndex, Self->CursorCharX);
            Self->Viewport->draw();
            break;
         }

         case KEY::LEFT: {
            Self->SelectIndex.reset();
            if (Self->Stream[Self->CursorIndex.Index].Code IS ESC::CELL) {
               // Cursor cannot be moved any further left.  The cursor index should never end up here, but
               // better to be safe than sorry.

            }
            else {
               for (auto index = Self->CursorIndex; index.Index > 0; ) {
                  index.prevChar(Self, Self->Stream);

                  auto code = Self->Stream[index.Index].Code;
                  if (code IS ESC::CELL) {
                     auto &cell = escape_data<bcCell>(Self, index);
                     if (cell.CellID IS Self->ActiveEditCellID) break;
                  }
                  else if (code IS ESC::VECTOR); // Vectors count as a character
                  else if (code != ESC::TEXT) continue;

                  if (!resolve_fontx_by_index(Self, index, Self->CursorCharX)) {
                     Self->CursorIndex = index;
                     Self->Viewport->draw();
                     log.warning("LeftCursor: %d, X: %d", Self->CursorIndex.Index, Self->CursorCharX);
                  }
                  break;
               }
            }
            break;
         }

         case KEY::RIGHT: {
            Self->SelectIndex.reset();

            auto index = Self->CursorIndex;
            while (index.valid(Self->Stream)) {
               auto code = Self->Stream[index.Index].Code;
               if (code IS ESC::CELL_END) {
                  auto &cell_end = escape_data<bcCellEnd>(Self, index);
                  if (cell_end.CellID IS Self->ActiveEditCellID) {
                     // End of editing zone - cursor cannot be moved any further right
                     break;
                  }
               }
               else if (code IS ESC::VECTOR); // Objects are treated as content, so do nothing special for these and drop through to next section
               else {
                  index.nextChar(Self, Self->Stream);
                  continue;
               }

               // The current index references a content character or object.  Advance the cursor to the next index.

               index.nextChar(Self, Self->Stream);
               if (!resolve_fontx_by_index(Self, index, Self->CursorCharX)) {
                  Self->CursorIndex = index;
                  Self->Viewport->draw();
                  log.warning("RightCursor: %d, X: %d", Self->CursorIndex.Index, Self->CursorCharX);
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
            if (Self->Stream[Self->CursorIndex.Index].Code IS ESC::CELL) {
               // Cursor cannot be moved any further left
            }
            else {
               auto index = Self->CursorIndex;
               index.prevChar(Self, Self->Stream);

               if (Self->Stream[index.Index].Code IS ESC::CELL);
               else {
                  if (!delete_selected(Self)) {
                     // Delete the character/escape code
                     Self->CursorIndex = index;
                     Self->CursorIndex.eraseChar(Self, Self->Stream);
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
            if (Self->Stream[Self->CursorIndex.Index].Code IS ESC::CELL_END) {
               // Not allowed to delete the end point
            }
            else {
               if (!delete_selected(Self)) {
                  Self->CursorIndex.eraseChar(Self, Self->Stream);
               }
               Self->UpdatingLayout = true;
               layout_doc_fast(Self);
               resolve_fontx_by_index(Self, Self->CursorIndex, Self->CursorCharX);
               Self->Viewport->draw();
            }

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
         if ((tab >= 0) and (unsigned(tab) < Self->Tabs.size())) {
            log.branch("Key: Enter, Tab: %d/%d, Type: %d", tab, LONG(Self->Tabs.size()), Self->Tabs[tab].Type);

            if ((Self->Tabs[tab].Type IS TT_LINK) and (Self->Tabs[tab].Active)) {
               for (auto &link : Self->Links) {
                  if ((link.BaseCode IS ESC::LINK) and (link.asLink()->ID IS Self->Tabs[tab].Ref)) {
                     link.exec(Self);
                     break;
                  }
               }
            }
         }
         break;
      }

      case KEY::PAGE_DOWN:
         scroll.DeltaX = 0;
         scroll.DeltaY = Self->Area.Height;
         scroll.DeltaZ = 0;
         QueueAction(AC_Scroll, Self->Viewport->UID, &scroll);
         break;

      case KEY::PAGE_UP:
         scroll.DeltaX = 0;
         scroll.DeltaY = -Self->Area.Height;
         scroll.DeltaZ = 0;
         QueueAction(AC_Scroll, Self->Viewport->UID, &scroll);
         break;

      case KEY::LEFT:
         scroll.DeltaX = -10;
         scroll.DeltaY = 0;
         scroll.DeltaZ = 0;
         QueueAction(AC_Scroll, Self->Viewport->UID, &scroll);
         break;

      case KEY::RIGHT:
         scroll.DeltaX = 10;
         scroll.DeltaY = 0;
         scroll.DeltaZ = 0;
         QueueAction(AC_Scroll, Self->Viewport->UID, &scroll);
         break;

      case KEY::DOWN:
         scroll.DeltaX = 0;
         scroll.DeltaY = 10;
         scroll.DeltaZ = 0;
         QueueAction(AC_Scroll, Self->Viewport->UID, &scroll);
         break;

      case KEY::UP:
         scroll.DeltaX = 0;
         scroll.DeltaY = -10;
         scroll.DeltaZ = 0;
         QueueAction(AC_Scroll, Self->Viewport->UID, &scroll);
         break;

      default: break; // Ignore unhandled codes
   }

   return ERR_Okay;
}

//********************************************************************************************************************

static bool detect_recursive_dialog = false;

static void error_dialog(const std::string Title, const std::string Message)
{
   pf::Log log(__FUNCTION__);
   static OBJECTID dialog_id = 0;

   log.warning("%s", Message.c_str());
#if !(defined(DBG_LAYOUT) || defined(DBG_STREAM) || defined(DBG_SEGMENTS))
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

static ERROR activate_cell_edit(extDocument *Self, INDEX CellIndex, StreamChar CursorIndex)
{
   pf::Log log(__FUNCTION__);
   auto &stream = Self->Stream;

   if ((CellIndex < 0) or (CellIndex >= INDEX(Self->Stream.size()))) return log.warning(ERR_OutOfRange);

   log.branch("Cell Index: %d, Cursor Index: %d", CellIndex, CursorIndex.Index);

   if (stream[CellIndex].Code != ESC::CELL) { // Sanity check
      return log.warning(ERR_Failed);
   }

   auto &cell = escape_data<bcCell>(Self, CellIndex);
   if (CursorIndex.Index <= CellIndex) { // Go to the start of the cell content
      CursorIndex.set(CellIndex + 1, 0);
   }

   if (stream[CursorIndex.Index].Code != ESC::TEXT) {
      // Skip ahead to the first relevant control code - it's always best to place the cursor ahead of things like
      // font styles, paragraph formatting etc.

      CursorIndex.Offset = 0;
      while (CursorIndex.Index < INDEX(Self->Stream.size())) {
         std::array<ESC, 6> content = {
            ESC::CELL_END, ESC::TABLE_START, ESC::VECTOR, ESC::LINK_END, ESC::PARAGRAPH_END, ESC::TEXT
         };
         if (std::find(std::begin(content), std::end(content), stream[CursorIndex.Index].Code) != std::end(content)) break;
         CursorIndex.nextCode();
      }
   }

   auto it = Self->EditDefs.find(cell.EditDef);
   if (it IS Self->EditDefs.end()) return log.warning(ERR_Search);

   deactivate_edit(Self, false);

   auto &edit = it->second;
   if (!edit.OnChange.empty()) { // Calculate a CRC for the cell content
      for (INDEX i = CellIndex; i < INDEX(Self->Stream.size()); i++) {
         if (stream[i].Code IS ESC::CELL_END) {
            auto &end = escape_data<bcCellEnd>(Self, i);
            if (end.CellID IS cell.CellID) {
               Self->ActiveEditCRC = GenCRC32(0, stream.data() + CellIndex, i - CellIndex);
               break;
            }
         }
      }
   }

   Self->ActiveEditCellID = cell.CellID;
   Self->ActiveEditDef    = &edit;
   Self->CursorIndex      = CursorIndex;
   Self->SelectIndex.reset();

   log.msg("Activated cell %d, cursor index %d, EditDef: %p, CRC: $%.8x",
      Self->ActiveEditCellID, Self->CursorIndex.Index, Self->ActiveEditDef, Self->ActiveEditCRC);

   // Set the focus index to the relevant TT_EDIT entry

   for (unsigned tab=0; tab < Self->Tabs.size(); tab++) {
      if ((Self->Tabs[tab].Type IS TT_EDIT) and (Self->Tabs[tab].Ref IS cell.CellID)) {
         Self->FocusIndex = tab;
         break;
      }
   }

   resolve_fontx_by_index(Self, Self->CursorIndex, Self->CursorCharX);

   reset_cursor(Self); // Reset cursor flashing

   // User callbacks

   if (!edit.OnEnter.empty()) {
      OBJECTPTR script;
      std::string function_name, argstring;

      log.msg("Calling onenter callback function.");

      if (!extract_script(Self, edit.OnEnter, &script, function_name, argstring)) {
         ScriptArg args[] = { { "ID", edit.Name } };
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

   // The edit tag needs to be found so that we can determine if OnExit needs to be called or not.

   auto edit = Self->ActiveEditDef;
   LONG cell_index = find_cell(Self, Self->ActiveEditCellID);

   Self->ActiveEditCellID = 0;
   Self->ActiveEditDef = NULL;
   Self->CursorIndex.reset();
   Self->SelectIndex.reset();

   if (Redraw) Self->Viewport->draw();

   if (cell_index >= 0) {
      if (!edit->OnChange.empty()) {
         bcCell &cell = escape_data<bcCell>(Self, cell_index);

         // CRC comparison - has the cell content changed?

         for (INDEX i = cell_index; i < INDEX(Self->Stream.size()); i++) {
            if (Self->Stream[i].Code IS ESC::CELL_END) {
               auto &end = escape_data<bcCellEnd>(Self, i);
               if (end.CellID IS cell.CellID) {
                  auto crc = GenCRC32(0, Self->Stream.data() + cell_index, i - cell_index);
                  if (crc != Self->ActiveEditCRC) {
                     log.trace("Change detected in editable cell %d", cell.CellID);

                     OBJECTPTR script;
                     std::string function_name, argstring;
                     if (!extract_script(Self, edit->OnChange, &script, function_name, argstring)) {
                        auto cell_content = cell_index;
                        cell_content++;

                        std::vector<ScriptArg> args = {
                           ScriptArg("CellID", edit->Name),
                           ScriptArg("Start", cell_content),
                           ScriptArg("End", i)
                        };

                        for (auto &cell_arg : cell.Args) args.emplace_back("", cell_arg.second);

                        scExec(script, function_name.c_str(), args.data(), args.size());
                     }
                  }

                  break;
               }
            }
         }
      }

      if (!edit->OnExit.empty()) {



      }
   }
   else log.warning("Failed to find cell ID %d", Self->ActiveEditCellID);
}

//********************************************************************************************************************
// Sends motion events for zones that the mouse pointer has departed.

static void check_pointer_exit(extDocument *Self, LONG X, LONG Y)
{
   for (auto it = Self->MouseOverChain.begin(); it != Self->MouseOverChain.end(); ) {
      if ((X < it->Left) or (Y < it->Top) or (X >= it->Right) or (Y >= it->Bottom)) {
         // Pointer has left this zone

         std::string function_name, argstring;
         OBJECTPTR script;
         if (!extract_script(Self, it->Function, &script, function_name, argstring)) {
            const ScriptArg args[] = {
               { "Element", it->ElementID },
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
         if ((X >= Self->EditCells[i].X) and (X < Self->EditCells[i].X + Self->EditCells[i].Width) and
             (Y >= Self->EditCells[i].Y) and (Y < Self->EditCells[i].Y + Self->EditCells[i].Height)) {
            break;
         }
      }

      if (i < Self->EditCells.size()) {
         // Mouse is within an editable segment.  Find the start and ending indexes of the editable area

         INDEX cell_start = find_cell(Self, Self->EditCells[i].CellID);
         INDEX cell_end  = cell_start;
         while (cell_end < INDEX(Self->Stream.size())) {
            if (Self->Stream[cell_end].Code IS ESC::CELL_END) {
               auto &end = escape_data<bcCellEnd>(Self, cell_end);
               if (end.CellID IS Self->EditCells[i].CellID) break;
            }

            cell_end++;
         }

         if (cell_end >= INDEX(Self->Stream.size())) return; // No matching cell end - document stream is corrupt

         log.warning("Analysing cell area %d - %d", cell_start, cell_end);

         SEGINDEX last_segment = -1;
         auto &ss = Self->getSortedSegments();
         for (unsigned sortseg=0; sortseg < ss.size(); sortseg++) {
            SEGINDEX seg = ss[sortseg].Segment;
            if ((Self->Segments[seg].Start.Index >= cell_start) and (Self->Segments[seg].Stop.Index <= cell_end)) {
               last_segment = seg;
               // Segment found.  Break if the segment's vertical position is past the mouse pointer
               if (Y < Self->Segments[seg].Area.Y) break;
               if ((Y >= Self->Segments[seg].Area.Y) and (X < Self->Segments[seg].Area.X)) break;
            }
         }

         if (last_segment != -1) {
            // Set the cursor to the end of the nearest segment
            log.warning("Last seg: %d", last_segment);
            Self->CursorCharX = Self->Segments[last_segment].Area.X + Self->Segments[last_segment].Area.Width;
            Self->SelectCharX = Self->CursorCharX;

            // A click results in the deselection of existing text

            if (Self->CursorIndex.valid()) deselect_text(Self);

            Self->CursorIndex = Self->Segments[last_segment].Stop;
            Self->SelectIndex.reset(); //Self->Segments[last_segment].Stop;

            activate_cell_edit(Self, cell_start, Self->CursorIndex);
         }

         return;
      }
      else log.warning("Mouse not within an editable cell.");
   }

   if (segment != -1) {
      StreamChar sc;
      if (!resolve_font_pos(Self, Self->Segments[segment], X, Self->CursorCharX, sc)) {
         if (Self->CursorIndex.valid()) deselect_text(Self); // A click results in the deselection of existing text

         if (!Self->Segments[segment].Edit) deactivate_edit(Self, true);

         // Set the new cursor information

         Self->CursorIndex = sc;
         Self->SelectIndex.reset(); //sc; // SelectIndex is for text selections where the user holds the LMB and drags the mouse
         Self->SelectCharX = Self->CursorCharX;

         log.msg("User clicked on point %.2fx%.2f in segment %d, cursor index: %d, char x: %d", X, Y, segment, Self->CursorIndex.Index, Self->CursorCharX);

         if (Self->Segments[segment].Edit) {
            // If the segment is editable, we'll have to turn on edit mode so
            // that the cursor flashes.  Work backwards to find the edit cell.

            for (auto cellindex = Self->Segments[segment].Start; cellindex.valid(); cellindex.prevCode()) {
               if (Self->Stream[cellindex.Index].Code IS ESC::CELL) {
                  auto &cell = escape_data<bcCell>(Self, cellindex);
                  if (!cell.EditDef.empty()) {
                     activate_cell_edit(Self, cellindex.Index, Self->CursorIndex);
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

   if ((Self->LinkIndex >= 0) and (Self->LinkIndex < LONG(Self->Links.size()))) {
      Self->Links[Self->LinkIndex].exec(Self);
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
      auto &ss = Self->getSortedSegments();

      for (row=0; (row < ss.size()) and (Y < ss[row].Y); row++);

      for (; row < ss.size(); row++) {
         if ((Y >= ss[row].Y) and (Y < ss[row].Y + Self->Segments[ss[row].Segment].Area.Height)) {
            if ((X >= Self->Segments[ss[row].Segment].Area.X) and (X < Self->Segments[ss[row].Segment].Area.X + Self->Segments[ss[row].Segment].Area.Width)) {
               Self->MouseOverSegment = ss[row].Segment;
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
         StreamChar cursor_index;
         if (!resolve_font_pos(Self, Self->Segments[Self->MouseOverSegment], X, cursor_x, cursor_index)) {
            if (Self->ActiveEditDef) {
               // For select-dragging, we must check that the selection is within the bounds of the editing area.

               if (INDEX cell_index = find_cell(Self, Self->ActiveEditCellID); cell_index >= 0) {
                  INDEX i = cell_index++;
                  if (cursor_index.Index < i) {
                     // If the cursor index precedes the start of the editing area, reset it

                     cursor_index.set(i, 0);
                     if (!resolve_fontx_by_index(Self, cursor_index, cursor_x)) {

                     }
                  }
                  else {
                     // If the cursor index is past the end of the editing area, reset it

                     while (i < INDEX(Self->Stream.size())) {
                        if (Self->Stream[i].Code IS ESC::CELL_END) {
                           auto &cell_end = escape_data<bcCellEnd>(Self, i);
                           if (cell_end.CellID IS Self->ActiveEditCellID) {
                              StreamChar sc(i, 0);
                              if (auto seg = find_segment(Self, sc, false); seg > 0) {
                                 seg--;
                                 sc = Self->Segments[seg].Stop;
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

   // Check if the user moved onto a link

   if ((Self->MouseInPage) and (!Self->LMB)) {
      for (auto i = LONG(Self->Links.size())-1; i >= 0; i--) { // Search from front to back
         if ((X >= Self->Links[i].X) and (Y >= Self->Links[i].Y) and
             (X < Self->Links[i].X + Self->Links[i].Width) and
             (Y < Self->Links[i].Y + Self->Links[i].Height)) {

            // The mouse pointer is inside a link or table cell.
            if (Self->LinkIndex IS -1) {
               gfxSetCursor(0, CRF::BUFFER, PTC::HAND, 0, Self->UID);
               Self->CursorSet = true;
            }

            if ((Self->Links[i].BaseCode IS ESC::LINK) and (!Self->Links[i].asLink()->PointerMotion.empty())) {
               auto mo = Self->MouseOverChain.emplace(Self->MouseOverChain.begin(),
                  Self->Links[i].asLink()->PointerMotion,
                  Self->Links[i].Y,
                  Self->Links[i].X,
                  Self->Links[i].Y + Self->Links[i].Height,
                  Self->Links[i].X + Self->Links[i].Width,
                  Self->Links[i].asLink()->ID);

               OBJECTPTR script;
               std::string argstring, func_name;
               if (!extract_script(Self, Self->Links[i].asLink()->PointerMotion, &script, func_name, argstring)) {
                  const ScriptArg args[] = { { "Element", mo->ElementID }, { "Status", 1 }, { "Args", argstring } };
                  scExec(script, func_name.c_str(), args, ARRAYSIZE(args));
               }
            }

            Self->LinkIndex = i;
            return;
         }
      }
   }

   // The mouse pointer is not inside a link

   if (Self->LinkIndex != -1) Self->LinkIndex = -1;

   // Check if the user moved onto text content

   if (Self->MouseOverSegment != -1) {
      if ((Self->Segments[Self->MouseOverSegment].TextContent) or (Self->Segments[Self->MouseOverSegment].Edit)) {
         gfxSetCursor(0, CRF::BUFFER, PTC::TEXT, 0, Self->UID);
         Self->CursorSet = true;
      }
      return;
   }

   for (unsigned i=0; i < Self->EditCells.size(); i++) {
      if ((X >= Self->EditCells[i].X) and (X < Self->EditCells[i].X + Self->EditCells[i].Width) and
          (Y >= Self->EditCells[i].Y) and (Y < Self->EditCells[i].Y + Self->EditCells[i].Height)) {
         gfxSetCursor(0, CRF::BUFFER, PTC::TEXT, 0, Self->UID);
         Self->CursorSet = true;
         return;
      }
   }

   // Reset the cursor to the default

   if (Self->CursorSet) {
      Self->CursorSet = false;
      gfxRestoreCursor(PTC::DEFAULT, Self->UID);
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

static LONG find_tabfocus(extDocument *Self, UBYTE Type, LONG Reference)
{
   for (unsigned i=0; i < Self->Tabs.size(); i++) {
      if ((Self->Tabs[i].Type IS Type) and (Reference IS Self->Tabs[i].Ref)) return i;
   }
   return -1;
}

//********************************************************************************************************************
// This function is used in tags.c by the link and object insertion code.

static LONG add_tabfocus(extDocument *Self, UBYTE Type, LONG Reference)
{
   pf::Log log(__FUNCTION__);

   //log.function("Type: %d, Ref: %d", Type, Reference);

   if (Type IS TT_LINK) { // For TT_LINK types, check that the link isn't already registered
      for (unsigned i=0; i < Self->Tabs.size(); i++) {
         if ((Self->Tabs[i].Type IS TT_LINK) and (Self->Tabs[i].Ref IS Reference)) {
            return i;
         }
      }
   }

   auto index = Self->Tabs.size();
   Self->Tabs.emplace_back(Type, Reference, Type, Self->Invisible ^ 1);

   if (Type IS TT_OBJECT) {
      // Find out if the object has a surface and if so, place it in the XRef field.

      if (GetClassID(Reference) != ID_SURFACE) {
         pf::ScopedObjectLock object(Reference, 3000);
         if (object.granted()) {
            OBJECTID regionid = 0;
            if (FindField(*object, FID_Region, NULL)) {
               if (!object->get(FID_Region, &regionid)) {
                  if (GetClassID(regionid) != ID_SURFACE) regionid = 0;
               }
            }

            if (!regionid) {
               if (FindField(*object, FID_Surface, NULL)) {
                  if (!object->get(FID_Surface, &regionid)) {
                     if (GetClassID(regionid) != ID_SURFACE) regionid = 0;
                  }
               }
            }

            Self->Tabs.back().XRef = regionid;
         }
      }
      else Self->Tabs.back().XRef = Reference;
   }

   return index;
}

//********************************************************************************************************************
// Changes the focus to an object or link in the document.  The new index is stored in the FocusIndex field.  If the
// Index is set to -1, set_focus() will focus on the first element, but only if it is an object.

static void set_focus(extDocument *Self, INDEX Index, CSTRING Caller)
{
   pf::Log log(__FUNCTION__);

   if (Self->Tabs.empty()) return;

   if ((Index < -1) or (unsigned(Index) >= Self->Tabs.size())) {
      log.traceWarning("Index %d out of bounds.", Index);
      return;
   }

   log.branch("Index: %d/%d, Type: %d, Ref: %d, HaveFocus: %d, Caller: %s", Index, LONG(Self->Tabs.size()), Index != -1 ? Self->Tabs[Index].Type : -1, Index != -1 ? Self->Tabs[Index].Ref : -1, Self->HasFocus, Caller);

   if (Self->ActiveEditDef) deactivate_edit(Self, true);

   if (Index IS -1) {
      Index = 0;
      Self->FocusIndex = 0;
      if (Self->Tabs[0].Type IS TT_LINK) {
         log.msg("First focusable element is a link - focus unchanged.");
         return;
      }
   }

   if (!Self->Tabs[Index].Active) {
      log.warning("Tab marker %d is not active.", Index);
      return;
   }

   Self->FocusIndex = Index;

   if (Self->Tabs[Index].Type IS TT_EDIT) {
      acFocus(Self->Page);

      if (auto cell_index = find_cell(Self, Self->Tabs[Self->FocusIndex].Ref); cell_index >= 0) {
         activate_cell_edit(Self, cell_index, StreamChar());
      }
   }
   else if (Self->Tabs[Index].Type IS TT_OBJECT) {
      if (Self->HasFocus) {
         CLASSID class_id = GetClassID(Self->Tabs[Index].Ref);
         OBJECTPTR input;
         if (class_id IS ID_VECTORTEXT) {
            if (!AccessObject(Self->Tabs[Index].Ref, 1000, &input)) {
               acFocus(input);
               //if ((input->getPtr(FID_UserInput, &text) IS ERR_Okay) and (text)) {
               //   txtSelectArea(text, 0,0, 200000, 200000);
               //}
               ReleaseObject(input);
            }
         }
         else if (acFocus(Self->Tabs[Index].Ref) != ERR_Okay) {
            acFocus(Self->Tabs[Index].XRef);
            // Causes an InheritedFocus callback in ActionNotify
         }
      }
   }
   else if (Self->Tabs[Index].Type IS TT_LINK) {
      if (Self->HasFocus) { // Scroll to the link if it is out of view, or redraw the display if it is not.
         for (unsigned i=0; i < Self->Links.size(); i++) {
            if ((Self->Links[i].BaseCode IS ESC::LINK) and (Self->Links[i].asLink()->ID IS Self->Tabs[Index].Ref)) {
               auto link_x = Self->Links[i].X;
               auto link_y = Self->Links[i].Y;
               auto link_bottom = link_y + Self->Links[i].Height;
               auto link_right  = link_x + Self->Links[i].Width;

               for (++i; i < Self->Links.size(); i++) {
                  if (Self->Links[i].asLink()->ID IS Self->Tabs[Index].Ref) {
                     if (Self->Links[i].Y + Self->Links[i].Height > link_bottom) link_bottom = Self->Links[i].Y + Self->Links[i].Height;
                     if (Self->Links[i].X + Self->Links[i].Width > link_right) link_right = Self->Links[i].X + Self->Links[i].Width;
                  }
               }

               view_area(Self, link_x, link_y, link_right, link_bottom);
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

static BYTE view_area(extDocument *Self, LONG Left, LONG Top, LONG Right, LONG Bottom)
{
   pf::Log log(__FUNCTION__);

   DOUBLE hgap = Self->Area.Width * 0.1, vgap = Self->Area.Height * 0.1;
   DOUBLE view_x = -Self->XPosition, view_y = -Self->YPosition;
   DOUBLE view_height = Self->Area.Height, view_width  = Self->Area.Width;

   log.trace("View: %dx%d,%dx%d Link: %dx%d,%dx%d", view_x, view_y, view_width, view_height, Left, Top, Right, Bottom);

   // Vertical

   if (Self->PageHeight > Self->Area.Height) {
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

   if (Self->CalcWidth > Self->Area.Width) {
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
      if (Self->Tabs[i].XRef IS currentfocus) {
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

   auto i = signed(Self->Tabs.size()); // This while loop is designed to stop if no tab indexes are found to be active
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

      if (!Self->Tabs[Self->FocusIndex].Active) continue;

      if ((Self->Tabs[Self->FocusIndex].Type IS TT_OBJECT) and (Self->Tabs[Self->FocusIndex].XRef)) {
         SURFACEINFO *info;
         if (!gfxGetSurfaceInfo(Self->Tabs[Self->FocusIndex].XRef, &info)) {
            if ((info->Flags & RNF::DISABLED) != RNF::NIL) continue;
         }
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

   log.traceBranch("PageHeight: %d/%d, PageWidth: %d/%d, XPos: %d, YPos: %d", Self->PageHeight, Self->Area.Height, Self->CalcWidth, Self->Area.Width, Self->XPosition, Self->YPosition);
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
   else {
      auto call = make_function_stdc(flash_cursor);
      SubscribeTimer(0.5, &call, &Self->FlashTimer);
   }
}
