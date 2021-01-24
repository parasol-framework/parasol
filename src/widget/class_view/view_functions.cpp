
//****************************************************************************

static void vwUserClick(objView *Self, const InputMsg *Input)
{
   parasol::Log log(__FUNCTION__);
   view_col *col;
   LONG i;
   LONG x;

   log.traceBranch("Style: %d, %dx%d, Selected: %d, Type: %d, Flags: $%.8x", Self->Style, Input->X, Input->Y, Self->SelectedTag, Input->Type, Input->Flags);

   Self->ClickX      = Input->X;
   Self->ClickY      = Input->Y;
   Self->ClickIndex  = -1;
   Self->ActiveTag   = -1;
   Self->SelectingItems = FALSE;
   XMLTag *tag = NULL;
   BYTE active = FALSE;

   // Reset the drag and drop state

   if (Self->DragItems) {
      FreeResource(Self->DragItems);
      Self->DragItems = NULL;
      Self->DragItemCount = 0;
   }

   if (((Self->Style IS VIEW_COLUMN) or (Self->Style IS VIEW_COLUMN_TREE)) and
       (Input->Type IS JET_LMB) and
       (Input->Y >= Self->Layout->BoundY) and (Input->Y < Self->Layout->BoundY + Self->Layout->BoundHeight)) {

      if (Input->Y < Self->Layout->BoundY + Self->ColumnHeight) {
         // Check if the click was over one of the column buttons.  This will affect sorting, or it can indicate a column resize.

         x = Self->Layout->BoundX + Self->XPos;
         for (col=Self->Columns, i=0; col; col=col->Next, i++) {
            if ((x+col->Width >= Self->ClickX-4) and (x+col->Width < Self->ClickX+4)) {
               // The user has opted to resize the column

               Self->ColumnResize = col;
               if (gfxSetCursor(0, CRF_LMB, PTR_SPLIT_HORIZONTAL, 0, Self->Head.UniqueID) IS ERR_Okay) {
                  Self->PointerLocked = PTR_SPLIT_HORIZONTAL;
               }

               return;
            }
            else if ((Self->ClickX >= x) and (Self->ClickX < x + col->Width)) {
               if (Self->Style IS VIEW_COLUMN) {
                  if (!(Self->Flags & VWF_NO_SORTING)) {
                     if (Self->Sort[0] IS i+1) viewSortColumnIndex(Self, i, TRUE);
                     else viewSortColumnIndex(Self, i, FALSE);
                  }
               }

               return;
            }
            x += col->Width;
         }
      }
   }

   view_node *node;

   if (Self->Flags & VWF_NO_SELECT) return;

   if (Input->Type IS JET_RMB) {
      if (Self->ContextMenu) {
         LONG x, y;

         // NOTE: The object placed in the context menu can be anything, e.g. it can be a menu or a surface reference for example.

         gfxGetCursorPos(&x, &y);
         acMoveToPoint(Self->ContextMenu, x-6, y-6, 0, MTF_X|MTF_Y);
         acShow(Self->ContextMenu);
      }
   }
   else if (Input->Type IS JET_LMB) {
      if ((tag = get_item_xy(Self, Self->XML->Tags,
            Input->X - Self->Layout->BoundX - Self->XPos,
            Input->Y - Self->Layout->BoundY - Self->YPos))) {
         node = (view_node *)tag->Private;
         Self->ClickHeld = TRUE;
         if ((Self->Style IS VIEW_TREE) or (Self->Style IS VIEW_COLUMN_TREE) or (Self->Style IS VIEW_GROUP_TREE)) {
            if (((Self->Style IS VIEW_GROUP_TREE) and (!node->Indent)) or
                (Input->X < node->X + Self->XPos + SWITCH_SIZE + Self->Layout->LeftMargin)) {
               // The user is expanding or collapsing a tree branch

               if (open_branch_callback(Self, tag) IS FALSE) {
                  if (node->Flags & NODE_CHILDREN) {
                     // Reverse the state of the branch if it has child items
                     node->Flags ^= NODE_OPEN;
                     Self->Deselect = FALSE;

                     arrange_items(Self);

                     if (!Self->RedrawDue) {
                        Self->RedrawDue = TRUE;
                        DelayMsg(AC_Draw, Self->Layout->SurfaceID, NULL);
                     }
                  }
               }

               return;
            }
         }

         if ((Self->Style IS VIEW_COLUMN_TREE) or (Self->Style IS VIEW_COLUMN)) {
            // Check if a cell has been clicked, e.g. checkbox
            LONG index;

            x = Self->Layout->BoundX + Self->XPos;
            for (col=Self->Columns, i=0; col; col=col->Next, i++) {
               if ((Self->ClickX >= x) and (Self->ClickX < x + col->Width)) {

                  index = tag->Index; // Save the current index in case it gets modified
                  LONG modstamp = Self->XML->Modified;
                  if (!report_cellclick(Self, tag->Index, i, Input->Type, Input->X - x, Input->Y - node->Y)) {
                     // If the subscriber alters the XML, redraw the item and return with no further processing.

                     if (Self->XML->Modified != modstamp) {
                        tag = Self->XML->Tags[index];
                        draw_item(Self, tag);
                        return;
                     }
                  }

                  if (Self->XML->Modified IS modstamp) {
                     if (col->Type IS CT_CHECKBOX) {
                        // Get the checkmark value and flip it

                        char buffer[32];
                        XMLTag *vtag;

                        get_col_value(Self, tag, col, buffer, sizeof(buffer), &vtag);

                        if (vtag) {
                           LONG checked;
                           if (buffer[0]) {
                              checked = StrToInt(buffer);
                              if ((!checked) and ((buffer[0] IS 'y') or (buffer[0] IS 'Y'))) {
                                 checked = 1;
                              }
                           }
                           else checked = 0;

                           checked ^= 1;

                           if (vtag->Child) {
                              LONG tagindex = tag->Index;
                              xmlSetAttrib(Self->XML, vtag->Child->Index, 0, 0, checked ? "1" : "0");
                              tag = Self->XML->Tags[tagindex];
                           }
                           else {
                              LONG tagindex = tag->Index;
                              xmlInsertContent(Self->XML, vtag->Index, XMI_CHILD, checked ? "1" : "0", 0);
                              tag = Self->XML->Tags[tagindex];
                           }

                           draw_item(Self, tag);
                        }

                        return;
                     }
                  }
                  break;
               }
               x += col->Width;
            }
         }

         Self->SelectingItems = TRUE; // TRUE, indicates that the user has clicked on an item to initiate selection or deselection

         if (node->Flags & NODE_SELECTED) node->Flags |= NODE_CAN_DESELECT;
         else node->Flags &= ~NODE_CAN_DESELECT;

         if ((node->Flags & NODE_SELECTED) and (!(Self->Flags & VWF_DRAG_DROP))) {
            // In multi-select mode, when the item is already selected we will deselect it.

            if (!(Input->Flags & JTYPE_DBL_CLICK)) {
               if (Self->Flags & VWF_MULTI_SELECT) {
                  log.trace("Deselecting clicked node.");
                  node->Flags &= ~NODE_SELECTED;
                  draw_item(Self, tag);
                  Self->Deselect = TRUE;
               }
            }

            Self->ActiveTag = tag->Index; // The ActiveTag refers to the most recently selected or -deselected- item
            report_selection(Self, SLF_ACTIVE|SLF_CLICK, tag->Index);
         }
         else {
            active = select_item(Self, tag, SLF_CLICK, TRUE, TRUE);
            Self->Deselect = FALSE; // We are not in deselect mode for click-dragging
         }
      }

      // Check the validity of the most current selected item

      check_selected_items(Self, Self->XML->Tags[0]);
   }

   // Double clicking will activate an item, if not already activated by select_item().

   if ((Self->ActiveTag != -1) and (Input->Type IS JET_LMB) and (Input->Flags & JTYPE_DBL_CLICK) and (active IS FALSE)) {
      // Turn off LMB, this is required to prevent highlight-dragging if the user accidentally moves the mouse shortly
      // before the button is released after a double-click.

      parasol::Log log(__FUNCTION__);
      log.traceBranch("Activating...");
      Self->ClickHeld = FALSE;
      acActivate(Self);
   }

   return;
}

//*****************************************************************************

static void vwUserClickRelease(objView *Self, const InputMsg *Input)
{
   Self->ClickIndex = -1;
   if (Input->Type IS JET_LMB) {
      Self->ColumnResize = NULL;
      Self->SelectingItems = FALSE;
      Self->ClickHeld = FALSE;

      // The cursor image must be checked following a click-release, e.g. if resizing a column.

      check_pointer_cursor(Self, Input->X, Input->Y);

      // If the user clicks and releases the mouse on a selected item when in drag drop mode, we will deselect it.  The
      // mouse must not have moved between the click and the release, otherwise it counts as an item drag.

      if (Self->Flags & VWF_DRAG_DROP) {
         if ((Self->ActiveTag != -1) and (Self->ActiveTag < Self->XML->TagCount)) {
            if ((ABS(Input->X - Self->ClickX) <= 1) and (ABS(Input->Y - Self->ClickY) <= 1)) {
               auto tag = Self->XML->Tags[Self->ActiveTag];
               auto node = (view_node *)tag->Private;
               if ((node->Flags & NODE_SELECTED) and (node->Flags & NODE_CAN_DESELECT)) {
                  node->Flags &= ~NODE_SELECTED;
                  draw_item(Self, tag);
                  report_selection(Self, SLF_ACTIVE, Self->ActiveTag); // The status of the tag has changed, so we need to report it even though the index being the same value.
               }
            }
         }
      }

      if (Self->Flags & VWF_AUTO_DESELECT) deselect_item(Self);
   }
}

//*****************************************************************************

static void vwUserMovement(objView *Self, const InputMsg *Input)
{
   parasol::Log log(__FUNCTION__);
   XMLTag *tag;
   view_col *col;
   LONG cx, width, index, checktag;
   LONG i, lastindex, pagey;
   BYTE highlighted, drag, highlighting;

   log.traceBranch("X: %d, Y: %d", Input->X, Input->Y);

   // Check the ClickIndex field to make sure it's not lying outside of the XML tag array

   if (Self->ClickIndex > Self->XML->TagCount) Self->ClickIndex = -1;

   check_pointer_cursor(Self, Input->X, Input->Y);

   // Handle column resizing

   LONG x = Input->X;
   LONG y = Input->Y;

   if (((Self->Style IS VIEW_COLUMN) or (Self->Style IS VIEW_COLUMN_TREE)) and (Self->ColumnResize)) {
      // Calculate the horizontal position of the column being resized.

      cx = 0;
      for (col=Self->Columns; col != Self->ColumnResize; col=col->Next) cx += col->Width;

      if (col) {
         width = col->Width;
         col->Width = x - Self->XPos - cx;
         if (col->Width < MIN_COLWIDTH) col->Width = MIN_COLWIDTH;
         if (col->Width != width) {
            // Recalculate the width of the scrollable page and reset the widths of each item to match the page width.

            for (col=Self->Columns, Self->PageWidth=0; col; col=col->Next) Self->PageWidth += col->Width;

            arrange_items(Self); // Rearrange items to match the new column/page width

            // Recalculate the horizontal scrollbar, then issue a redraw

            calc_hscroll(Self);

            acDrawID(Self->Layout->SurfaceID);
         }
         return;
      }
      else Self->ColumnResize = NULL;
   }

   // Adjust for scrolling

   x -= Self->XPos;
   y -= Self->YPos;

   // If the mouse coordinates fall outside of the view area, restrict them.  This is useful when the user drags the
   // pointer while the mouse button is held down.

   if (x < Self->Layout->BoundX) x = Self->Layout->BoundX;
   if (y < Self->Layout->BoundY + Self->ColumnHeight) y = Self->Layout->BoundY + Self->ColumnHeight + (Self->LineHeight / 2);

   if (x > Self->PageWidth) x = Self->PageWidth;
   pagey = y;
   if (pagey > Self->Layout->BoundY + Self->PageHeight) pagey = Self->Layout->BoundY + Self->PageHeight - (Self->LineHeight / 2);

   highlighted = FALSE;
   checktag = -1;

   if (Input->OverID IS Self->Layout->SurfaceID) highlighting = TRUE;
   else highlighting = FALSE;

   drag = Self->ActiveDrag;
   Self->ActiveDrag = FALSE;

   if ((tag = get_item_xy(Self, Self->XML->Tags, x-Self->Layout->BoundX, pagey-Self->Layout->BoundY))) {
      auto node = (view_node *)tag->Private;
      checktag = tag->Index;
      if ((Self->ClickHeld) and (Self->SelectingItems)) {
         // Click-dragging, multi-select support etc

         if (Self->ClickIndex IS -1) Self->ClickIndex = tag->Index;

         if (Self->DragItems) {
            if (Input->OverID IS Self->Layout->SurfaceID) highlighting = TRUE;
         }
         else if (Self->Flags & VWF_DRAG_DROP) {
            if (drag) {
               LONG absx, absy;
               // Dragging starts if the pointer moves at least 4 pixels from the click origin.
               absx = Input->X - Self->ClickX;
               absy = Input->Y - Self->ClickY;
               if (absx < 0) absx = -absx;
               if (absy < 0) absy = -absy;
               if ((absx > 4) or (absy > 4)) {
                  drag_items(Self);
               }
               else Self->ActiveDrag = TRUE; // Keep the ActiveDrag set to TRUE
            }
         }
         else if ((Self->Flags & VWF_SENSITIVE) or (!(Self->Flags & VWF_MULTI_SELECT))) {
            if (!(node->Flags & NODE_SELECTED)) {
               // Scan for any existing selections and turn them off.

               log.trace("Selecting tag %d.", tag->Index);

               for (index=0; Self->XML->Tags[index]; index++) {
                  auto node = (view_node *)Self->XML->Tags[index]->Private;
                  if (node->Flags & NODE_SELECTED) {
                     node->Flags &= ~NODE_SELECTED;
                     draw_item(Self, Self->XML->Tags[index]);
                  }
               }
               node = (view_node *)tag->Private;

               node->Flags |= NODE_SELECTED;
               draw_item(Self, tag);

               // Change the currently active tag

               LONG flags = SLF_SELECTED;
               Self->SelectedTag = tag->Index;
               if (!(Self->Flags & VWF_MULTI_SELECT)) {
                  Self->ActiveTag = tag->Index;
                  flags |= SLF_ACTIVE;
               }
               report_selection(Self, flags, tag->Index);

               // Activate if sensitive mode is enabled

               if (Self->Flags & VWF_SENSITIVE) {
                  // Activate so long as the 'insensitive' attribute has not been set against the tag

                  for (i=0; i < tag->TotalAttrib; i++) {
                     if (!StrMatch(tag->Attrib[i].Name, "insensitive")) break;
                  }

                  if (i >= tag->TotalAttrib) acActivate(Self);
               }
            }
         }
         else {
            // Select or deselect everything between the item at which the LMB was held and the item where we are at now.

            log.trace("Single-select for tag %d", tag->Index);

            i = Self->ClickIndex; // ClickIndex is the item at which the LMB was held
            lastindex = tag->Index; // LastIndex is the item at which we are now

            for (auto tag=Self->XML->Tags[Self->ClickIndex]; tag; ) {
               auto node = (view_node *)tag->Private;
               if (node->Flags & NODE_SELECTED) { // The node is currently selected
                  if (Self->Deselect IS TRUE) {
                     node->Flags &= ~NODE_SELECTED;
                     draw_item(Self, tag);
                  }
               }
               else { // The node is not yet selected
                  if (Self->Deselect IS FALSE) {
                     node->Flags |= NODE_SELECTED;
                     draw_item(Self, tag);
                  }
               }

               if (tag->Index IS lastindex) break;

               if (tag->Index < lastindex) tag = tag->Next; // Scan forwards
               else tag = tag->Prev; // Scan backwards
            }

            if (lastindex != Self->SelectedTag) { Self->SelectedTag = lastindex; report_selection(Self, SLF_SELECTED, lastindex); }
            if (lastindex != Self->ActiveTag) { Self->ActiveTag = lastindex; report_selection(Self, SLF_ACTIVE, lastindex); }
         }
      }

      if (highlighting) {
         // Highlight the underlying item due to mouse-over (do not select it)

         auto node = (view_node *)Self->XML->Tags[checktag]->Private;
         if (y <= node->Y + node->Height) {
            if (Self->HighlightTag != Self->XML->Tags[checktag]->Index) {
               if ((i = Self->HighlightTag) != -1) {
                  Self->HighlightTag = -1;
                  draw_item(Self, Self->XML->Tags[i]);
               }

               Self->HighlightTag = Self->XML->Tags[checktag]->Index;
               draw_item(Self, Self->XML->Tags[checktag]);
            }
            highlighted = TRUE;
         }
      }
   }

   // If no item is to be highlighted, check if there is a current highlighted item and deselect it.

   if ((highlighted IS FALSE) and (Self->HighlightTag != -1)) {
      auto tag = Self->XML->Tags[Self->HighlightTag];
      Self->HighlightTag = -1;
      draw_item(Self, tag);
   }

   if (Self->ClickHeld) {
      if ((x > Self->Layout->BoundX) and (x < Self->Layout->BoundX + Self->Layout->BoundWidth)) {
         //check_selected_items(Self, Self->XML->Tags[0]); Seems like overkill to call this since our routine doesn't change or use the SelectedTag.

         if (checktag != -1) check_item_visible(Self, Self->XML->Tags[checktag]);
      }
   }
}

//****************************************************************************

static ERROR calc_hscroll(objView *Self)
{
   parasol::Log log(__FUNCTION__);

   log.traceBranch("calc_hscroll: Page: %d, View: %d", Self->Layout->BoundX + Self->PageWidth, Self->Layout->BoundWidth);

   if (!Self->HScroll) return ERR_Okay;

   if ((Self->GroupBitmap) and (Self->GroupBitmap->Width != Self->PageWidth)) {
      log.msg("GroupBitmap->Width %d != Self->PageWidth %d", Self->GroupBitmap->Width, Self->PageWidth);
      if (Self->GroupHeaderXML) {
         gen_group_bkgd(Self, Self->GroupHeaderXML, &Self->GroupBitmap, "calc_hscroll");
      }

      if (Self->GroupSelectXML) {
         gen_group_bkgd(Self, Self->GroupSelectXML, &Self->SelectBitmap, "calc_hscroll");
      }
   }

   struct scUpdateScroll scroll;
   scroll.ViewSize = -1;
   if (Self->Document) scroll.PageSize = 1;
   else scroll.PageSize = Self->Layout->BoundX + Self->PageWidth;
   scroll.Position = -Self->XPos;
   scroll.Unit     = 16;
   return Action(MT_ScUpdateScroll, Self->HScroll, &scroll);
}

//****************************************************************************

static ERROR calc_vscroll(objView *Self)
{
   parasol::Log log(__FUNCTION__);

   log.trace("calc_vscroll: Page: %d, View: %d", Self->Layout->BoundY + Self->PageHeight, Self->Layout->BoundHeight);

   if (!Self->VScroll) return ERR_Okay;
   if (Self->PageHeight < 0) Self->PageHeight = 0;

   struct scUpdateScroll scroll;
   scroll.ViewSize = -1;
   if (Self->Document) scroll.PageSize = 1;
   else scroll.PageSize = Self->Layout->BoundY + Self->PageHeight;
   scroll.Position = -Self->YPos;
   scroll.Unit     = Self->Font->MaxHeight;
   return Action(MT_ScUpdateScroll, Self->VScroll, &scroll);
}

//****************************************************************************

static void check_pointer_cursor(objView *Self, LONG X, LONG Y)
{
   if ((Self->Style IS VIEW_COLUMN) or (Self->Style IS VIEW_COLUMN_TREE)) {
      if ((Y >= Self->Layout->BoundY) and (Y < Self->Layout->BoundY + Self->ColumnHeight)) {
         LONG cx = 0;
         view_col *col;
         for (col=Self->Columns; col; col=col->Next) {
            cx += col->Width;

            if ((X - Self->XPos >= cx-3) and (X - Self->XPos < cx+3)) {
               if (!Self->PointerLocked) {
                  if (gfxSetCursor(Self->Layout->SurfaceID, 0, PTR_SPLIT_HORIZONTAL, 0, Self->Head.UniqueID) IS ERR_Okay) {
                     Self->PointerLocked = PTR_SPLIT_HORIZONTAL;
                  }
               }
               return;
            }
         }
      }
   }

   if ((Self->PointerLocked) and (!Self->ColumnResize)) {
      // We have the pointer locked and the cursor is out of bounds.  Assuming that the user is not performing a column
      // resize, restore the cursor back to its normal state.
      gfxRestoreCursor(PTR_DEFAULT, Self->Head.UniqueID);
      Self->PointerLocked = 0;
   }
}

//****************************************************************************

static LONG arrange_tree(objView *Self, XMLTag *Root, LONG X)
{
   parasol::Log log(__FUNCTION__);

   log.traceBranch("Index %d, X %d, Y %d", Self->TreeIndex, X, Self->PageHeight);

   Self->TreeIndex++;
   objBitmap *expand, *collapse;
   if (!(expand = get_expand_bitmap(Self, 0))) return 0;
   if (!(collapse = get_collapse_bitmap(Self, 0))) return 0;
   LONG itemcount = 0;

   XMLTag *child;
   for (auto tag=Root; tag; tag=tag->Next) {
      view_node *node;
      if (!(node = (view_node *)tag->Private)) continue;
      if (!(node->Flags & NODE_ITEM)) continue;

      if (((Self->TreeIndex IS 1) and (Self->Style IS VIEW_TREE)) or (node->Flags & NODE_CHILDREN)) {
         node->Flags |= NODE_TREEBOX;
      }
      else node->Flags &= ~NODE_TREEBOX;

      node->X = X;
      node->Y = Self->PageHeight;
      node->Height = Self->LineHeight;
      node->Indent = Self->TreeIndex;
      Self->PageHeight += Self->LineHeight;

      if (Self->Style IS VIEW_COLUMN_TREE) {
         if (Self->Layout->BoundWidth > Self->PageWidth) node->Width = Self->Layout->BoundWidth;
         else node->Width = Self->PageWidth; // In column mode the entire breadth of the line is used for each item
      }
      else {
         node->Width = expand->Width + 4 + Self->IconWidth + 4;

         CSTRING str = get_nodestring(Self, node);
         if (str) node->Width += fntStringWidth(Self->Font, str, -1);

         if (node->X + node->Width + 4 > Self->PageWidth) Self->PageWidth = node->X + node->Width + 4;
      }

      if (node->Flags & NODE_OPEN) {
         if ((child = tag->Child)) {
            LONG childcount = arrange_tree(Self, tag->Child, node->X + Self->IconWidth);
            if (childcount) node->Flags |= NODE_CHILDREN;
            else node->Flags &= ~NODE_CHILDREN;
         }
      }

      Self->TotalItems++;
      itemcount++;
   }

   Self->TreeIndex--;
   return itemcount;
}

//****************************************************************************

#define GAP_ICON_TEXT 4

static void arrange_items(objView *Self)
{
   parasol::Log log(__FUNCTION__);

   log.traceBranch("");

   Self->PageWidth  = Self->Layout->BoundWidth;
   Self->PageHeight = 0;
   Self->IconWidth  = 0;
   Self->LineHeight = 0;
   Self->TotalItems = 0;

   if ((Self->Style IS VIEW_COLUMN) or (Self->Style IS VIEW_COLUMN_TREE)) Self->ColumnHeight = Self->Font->MaxHeight + 6;
   else Self->ColumnHeight = 0;

   if (!(Self->Flags & VWF_NO_ICONS)) {
      Self->IconWidth  = Self->IconSize + GAP_ICON_TEXT;

      if ((Self->Style IS VIEW_COLUMN) or (Self->Style IS VIEW_COLUMN_TREE)) {
         Self->LineHeight = Self->IconSize + 1;
      }
      else if (Self->Style IS VIEW_LIST) {
         Self->LineHeight = Self->IconSize + 1;
      }
      else if (Self->Style IS VIEW_LONG_LIST) {
         Self->LineHeight = Self->IconSize + 2;
      }
      else Self->LineHeight = Self->IconSize + 5;  // Line height must meet the icon size, at a minimum
   }

   // Determine the number of pixels assigned to each line

   if (Self->LineHeight < Self->Font->MaxHeight) {
      Self->LineHeight = Self->Font->MaxHeight;
      if (Self->VSpacing) Self->LineHeight += Self->VSpacing;
      else Self->LineHeight += 4;
   }
   else {
      if (Self->VSpacing) Self->LineHeight += Self->VSpacing;
      else Self->LineHeight += 1;
   }

   if (Self->XML->TagCount < 1) goto exit;

   XMLTag *tag;
   view_node *node, *scan_node;
   view_col *col;
   LONG x, y, strwidth;
   LONG index, columngap, columncount, hbar;

   if ((Self->Style IS VIEW_DOCUMENT) or (Self->Document)) {
      // Note with respect to the above - setting the Document field overrides the default view style.  If the
      // developer wishes to switch back to a standard style then the Document field needs to be set to NULL.

      if (Self->Document) {
         // Get an item count

         auto tag = Self->XML->Tags[0];
         for (index=0; tag; index++) {
            if (!(node = (view_node *)tag->Private)) break;
            Self->TotalItems++;
            tag = tag->Next;
         }

         if (!(Self->Document->Head.Flags & NF_INITIALISED)) {
            if (acInit(Self->Document) != ERR_Okay) {
               Self->Document = NULL;
               goto exit;
            }
         }

         // Reprocess the document

         acRefresh(Self->Document);

         GetFields(Self->Document,
            FID_PageHeight|TLONG, &Self->PageHeight,
            FID_PageWidth|TLONG,  &Self->PageWidth,
            TAGEND);
      }
   }
   else if (Self->Style IS VIEW_GROUP_TREE) {
      for (auto tag=Self->XML->Tags[0]; tag; tag=tag->Next) {
         if (!(node = (view_node *)tag->Private)) continue;
         if (!(node->Flags & NODE_ITEM)) continue;

         node->X = 0;
         node->Y = Self->PageHeight;
         node->Width  = Self->Layout->BoundWidth;
         if (Self->GroupBitmap) node->Height = Self->GroupBitmap->Height;
         else node->Height = Self->LineHeight;
         node->Indent = 0;

         Self->TreeIndex = 0;
         if ((node->Flags & NODE_OPEN) and (tag->Child)) {
            Self->PageHeight += node->Height;
            arrange_tree(Self, tag->Child, 0);
         }
         else Self->PageHeight += node->Height;
      }
   }
   else if (Self->Style IS VIEW_TREE) {
      if (Self->XML->Tags[0]) {
         Self->TreeIndex = 0;
         arrange_tree(Self, Self->XML->Tags[0], 0);
      }
   }
   else if ((Self->Style IS VIEW_LIST) or (Self->Style IS VIEW_LONG_LIST)) {
      LONG linewidth;

      columncount = 0;
      columngap = 20;

      if ((Self->Style IS VIEW_LIST) and (Self->HScroll)) {
         // Horizontal scrollbar height compensation, helps avoid the vertical scrollbar from having to be used in list mode.
         hbar = 20;
      }
      else hbar = 0;

      x = 0;
      y = Self->Layout->TopMargin;
      linewidth = 0;
      auto tag = Self->XML->Tags[0];
      for (index=0; tag; index++) {
         if (!(node = (view_node *)tag->Private)) break;

         CSTRING text = get_nodestring(Self, node);

         strwidth = fntStringWidth(Self->Font, text, -1);
         if (Self->IconWidth + strwidth + columngap > linewidth) {
            linewidth = Self->IconWidth + strwidth + columngap;
         }

         if (linewidth > Self->MaxItemWidth) linewidth = Self->MaxItemWidth;

         node->X = x;
         node->Y = y;
         node->Width  = 0; // We'll set this later
         node->Height = Self->LineHeight;

         if (y IS Self->Layout->BoundY) node->Flags |= NODE_NEWCOLUMN;
         else node->Flags &= ~NODE_NEWCOLUMN;

         y += Self->LineHeight;

         if ((!tag->Next) or ((Self->Style IS VIEW_LIST) and (y > Self->Layout->BoundY) and (y + Self->LineHeight > Self->Layout->BoundHeight - hbar))) {
            // We are about to go to a new column, or this is the end of all columns

            if ((!tag->Next) and (columncount < 1)) {
               // If there are no more tags and we know there is only one column, all nodes will use the entire width of view.

               for (auto scan=tag; scan; scan=scan->Prev) {
                  scan_node = (view_node *)scan->Private;
                  if (scan_node->Width) break;
                  scan_node->Width = Self->Layout->BoundWidth;
               }
            }
            else {
               for (auto scan=tag; scan; scan=scan->Prev) {
                  scan_node = (view_node *)scan->Private;
                  if (scan_node->Width) break;
                  scan_node->Width = linewidth;
               }
            }

            y = Self->Layout->TopMargin;
            x += linewidth;
            linewidth = 0;
            columncount++;
         }

         if (Self->Style IS VIEW_LONG_LIST) {
            if (linewidth > Self->PageWidth) Self->PageWidth = linewidth + Self->Layout->RightMargin;
         }
         else if (node->X + linewidth > Self->PageWidth) {
            Self->PageWidth = node->X + linewidth + Self->Layout->RightMargin;
         }

         if (node->Y + node->Height + Self->Layout->BottomMargin > Self->PageHeight) {
            Self->PageHeight = node->Y + node->Height + Self->Layout->BottomMargin;
         }

         Self->TotalItems++;
         tag = tag->Next;
      }
   }
   else if (Self->Style IS VIEW_ICON) {

   }
   else if (Self->Style IS VIEW_COLUMN_TREE) {

      // Calculate the width of the scrollable page

      for (Self->PageWidth=0, col=Self->Columns; col; col=col->Next) {
         Self->PageWidth += col->Width;
      }

      if (Self->XML->Tags[0]) {
         Self->TreeIndex = 0;
         Self->PageHeight = Self->ColumnHeight;
         arrange_tree(Self, Self->XML->Tags[0], 0);
      }
   }
   else if (Self->Style IS VIEW_COLUMN) {
      y = Self->ColumnHeight;
      columncount = 0;
      columngap = 20;

      // Calculate the width of the scrollable page

      for (Self->PageWidth=0, col=Self->Columns; col; col=col->Next) {
         Self->PageWidth += col->Width;
      }

      node = 0;
      tag = Self->XML->Tags[0];
      for (index=0; tag; index++) {
         if (!(node = (view_node *)tag->Private)) break;

         CSTRING text = get_nodestring(Self, node);

         if ((text) and (text[0])) {
            strwidth = fntStringWidth(Self->Font, text, -1);

            node->X = 0;
            node->Y = y;
            if (Self->Layout->BoundWidth > Self->PageWidth) node->Width = Self->Layout->BoundWidth;
            else node->Width = Self->PageWidth; // In column mode the entire breadth of the line is used for each item
            node->Height = Self->LineHeight;

            y += Self->LineHeight;
         }
         else {
            log.warning("Empty item found in XML tags, index %d", index);
            node->Width  = 0;
         }

         Self->TotalItems++;
         tag = tag->Next;
      }

      // Calculate the height of the scrollable page (does not include the column buttons at the top, as they are not included in the scroll proces).

      if (node) Self->PageHeight = node->Y + node->Height + Self->Layout->BottomMargin;
      else Self->PageHeight = 0;
   }
   else log.warning("No style specified.");

   // Adjust node coordinates if a border is enabled
/*
   if (Self->ColBorder.Alpha) {
      if (!(Self->GfxFlags & VGF_DRAW_TABLE)) {
         for (tag = Self->XML->Tags[0]; tag; tag=tag->Next) {
            if (!(node = (view_node *)tag->Private)) continue;
            node->X++;
            node->Y++;
         }
      }
   }
*/
   // Recalculate scroll bars, based on the new PageWidth and PageHeight values

exit:
   if (Self->PageHeight <= Self->Layout->BoundHeight) Self->YPos = 0;
   if (Self->PageWidth <= Self->Layout->BoundWidth)   Self->XPos = 0;

   calc_vscroll(Self);
   calc_hscroll(Self);
}

/*****************************************************************************
** Internal: sort_items()
*/

static ERROR sort_items(objView *Self)
{
   parasol::Log log(__FUNCTION__);
   view_col *col;
   LONG colindex, i;
   LONG flags;

   flags = XSF_CHECK_SORT|XSF_REPORT_SORTING;
   colindex = Self->Sort[0];
   if (colindex < 0) { colindex = -colindex; flags |= XSF_DESC; }
   colindex--;

   for (i=0, col=Self->Columns; (col) and (i < colindex); col=col->Next, i++);

   if (!col) {
      col = Self->Columns;
      Self->Sort[0] = 1;
   }

   log.extmsg("Column: %d (%s)", Self->Sort[0], col->Name);

   // Ask the XML object to re-sort the XML.  This will sort on tag content by default, although the developer can override this by passing the 'sort' attribute amongst the tags to affect/improve sorting behaviour.

   return xmlSort(Self->XML, 0, col->Name, flags);
}

//****************************************************************************
// Create the box icon for expanding.

static objBitmap * get_expand_bitmap(objView *Self, LONG BPP)
{
   if (!Self->ExpandBitmap) {
      objBitmap *box;
      if (!CreateObject(ID_BITMAP, NF_INTEGRAL, &box,
            FID_Width|TLONG,        SWITCH_SIZE,
            FID_Height|TLONG,       SWITCH_SIZE,
            FID_BitsPerPixel|TLONG, BPP,
            TAGEND)) {
         Self->ExpandBitmap = box;
         gfxDrawRectangle(box, 0, 0, box->Width, box->Height, PackPixel(box,250,250,250), BAF_FILL); // Fill colour
         gfxDrawRectangle(box, 0, 0, box->Width, box->Height, PackPixel(box,130,130,130), 0);     // Border
         gfxDrawRectangle(box, 1, 1, box->Width-2, box->Height-2, PackPixel(box,230,230,230), 0); // Border softening
         gfxDrawRectangle(box, 3, box->Height/2, box->Width-6, 1, PackPixel(box,80,80,80), BAF_FILL); // Horizontal slash (-)
         gfxDrawRectangle(box, box->Width/2, 3, 1, box->Height-6, PackPixel(box,80,80,80), BAF_FILL); // Vertical slash (+)
      }
   }

   return Self->ExpandBitmap;
}

//****************************************************************************
// Create the box icon for collapsing.

static objBitmap * get_collapse_bitmap(objView *Self, LONG BPP)
{
   if (!Self->CollapseBitmap) {
      objBitmap *box;
      if (!CreateObject(ID_BITMAP, NF_INTEGRAL, &box,
            FID_Width|TLONG,        SWITCH_SIZE,
            FID_Height|TLONG,       SWITCH_SIZE,
            FID_BitsPerPixel|TLONG, BPP,
            TAGEND)) {
         Self->CollapseBitmap = box;
         gfxDrawRectangle(box, 0, 0, box->Width, box->Height, PackPixel(box,250,250,250), BAF_FILL);  // Fill colour
         gfxDrawRectangle(box, 0, 0, box->Width, box->Height, PackPixel(box,130,130,130), 0);         // Border
         gfxDrawRectangle(box, 1, 1, box->Width-2, box->Height-2, PackPixel(box,230,230,230), 0);     // Border softening
         gfxDrawRectangle(box, 3, box->Height/2, box->Width-6, 1, PackPixel(box,80,80,80), BAF_FILL); // Horizontal slash (-)
      }
   }

   return Self->CollapseBitmap;
}

//****************************************************************************

static void format_value(objView *Self, STRING buffer, LONG Size, LONG Type)
{
   if (Type IS CT_DATE) {
      DateTime time;
      char str[6];

      LONG j = 0;
      LONG i = 0;
      for (i=0; (buffer[j]) and (i < 4); i++) str[i] = buffer[j++];
      str[i] = 0;
      if ((time.Year = StrToInt(str))) {
         for (i=0; (buffer[j]) and (i < 2); i++) str[i] = buffer[j++];
         str[i] = 0;
         time.Month = StrToInt(str);
         for (i=0; (buffer[j]) and (i < 2); i++) str[i] = buffer[j++];
         str[i] = 0;
         time.Day = StrToInt(str);
         while ((buffer[j]) and (buffer[j] <= 0x20)) j++;
         for (i=0; (buffer[j]) and (i < 2); i++) str[i] = buffer[j++];
         str[i] = 0;
         time.Hour = StrToInt(str);
         if (buffer[j] IS ':') j++;
         for (i=0; (buffer[j]) and (i < 2); i++) str[i] = buffer[j++];
         str[i] = 0;
         time.Minute = StrToInt(str);
         if (buffer[j] IS ':') j++;
         for (i=0; (buffer[j]) and (i < 2); i++) str[i] = buffer[j++];
         str[i] = 0;
         time.Second = StrToInt(str);

         StrFormatDate(buffer, Size, Self->DateFormat, &time);
      }
      else buffer[0] = 0;
   }
   else if (Type IS CT_BYTESIZE) {
      DOUBLE number;
      number = StrToFloat(buffer);
      if (number < 1024.0) StrFormat(buffer, Size, "%.0f", number);
      else if (number < 1048576.0) StrFormat(buffer, Size, "%.0f KB", number / 1024.0);
      else if (number < 1073741824.0) {
         number /= 1048576.0;
         if (number >= 10.0) StrFormat(buffer, Size, "%.0f MB", number);
         else StrFormat(buffer, Size, "%.1f MB", number);
      }
      else StrFormat(buffer, Size, "%.1f GB", number / 1073741824.0);
   }
   else if (Type IS CT_SECONDS) {
      LONG min, sec;
      sec = StrToInt(buffer);
      min = sec / 60;
      sec = sec % 60;
      StrFormat(buffer, Size, "%d:%.2d", min, sec);
   }
}

//****************************************************************************

static LONG glSaveClipRight = 0;

static LONG draw_tree(objView *Self, objSurface *Surface, objBitmap *Bitmap, XMLTag *Root, LONG *Y)
{
   objFont *font;
   XMLTag *child;
   objBitmap *expand, *collapse;
   LONG nx, ny, strwidth, col_branch, col_selectbar, col_select, linebreak, clipright;
   BYTE clip;

   if (!Root) return 0;

   col_branch    = bmpGetColour(Bitmap, Self->ColBranch.Red, Self->ColBranch.Green, Self->ColBranch.Blue, Self->ColBranch.Alpha);
   col_selectbar = bmpGetColour(Bitmap, 0, 0, 0, 255);
   col_select    = bmpGetColour(Bitmap, 0, 0, 255, 255);
   if (!(expand = get_expand_bitmap(Self, Bitmap->BitsPerPixel))) return 0;
   if (!(collapse = get_collapse_bitmap(Self, Bitmap->BitsPerPixel))) return 0;

   view_node *node = NULL;
   view_node *firstnode = NULL;
   LONG itemcount = 0;
   font = Self->Font;
   font->Bitmap = Bitmap;
   font->AlignHeight = Self->LineHeight;
   font->Align = ALIGN_VERTICAL;
   font->WrapEdge = 8192;

   clipright = Bitmap->Clip.Right;

   //log.trace("draw_tree() Area: %dx%d,%dx%d Index: %d, Highlight: %d", Self->Layout->BoundX, Self->Layout->BoundY, Self->Layout->BoundWidth, Self->Layout->BoundHeight, Self->TreeIndex, Self->HighlightTag);

   linebreak = 0;
   for (auto tag=Root; tag; tag=tag->Next) {
      if ((!tag->Private) or (!(((view_node *)tag->Private)->Flags & NODE_ITEM))) continue;

      auto node = (view_node *)tag->Private;
      if (!firstnode) firstnode = node;

      if (Self->Style != VIEW_COLUMN_TREE) {
         if ((linebreak) and (Self->GfxFlags & VGF_LINE_BREAKS)) {
            Bitmap->Opacity = 255;
            gfxDrawRectangle(Bitmap, Self->Layout->BoundX, linebreak+Self->LineHeight-1, Self->Layout->BoundWidth, 1,
               PackPixel(Bitmap,240,240,240), TRUE);
         }
      }

      nx = node->X + Self->XPos + Self->Layout->BoundX + Self->Layout->LeftMargin;
      ny = node->Y + Self->YPos + Self->Layout->BoundY;

      clip = (ny + Self->LineHeight > Bitmap->Clip.Top) and (ny < Bitmap->Clip.Bottom);
      linebreak = ny;

      if (clip) {
         RGB8 rgbBkgd;

         rgbBkgd.Alpha = 0;
         font->Colour = node->FontRGB;

         if ((tag->Index IS Self->HighlightTag) and (!(Surface->Flags & RNF_DISABLED))) {
            if (Self->ColBkgdHighlight.Alpha) {
               rgbBkgd = Self->ColBkgdHighlight;

               if (node->Flags & NODE_SELECTED) {
                  WORD red, green, blue, alpha;
                  alpha = Self->ColBkgdHighlight.Alpha + ((Self->ColSelect.Alpha - Self->ColBkgdHighlight.Alpha)>>1);
                  red   = Self->ColBkgdHighlight.Red + ((Self->ColSelect.Red - Self->ColBkgdHighlight.Red)>>1);
                  green = Self->ColBkgdHighlight.Green + ((Self->ColSelect.Green - Self->ColBkgdHighlight.Green)>>1);
                  blue  = Self->ColBkgdHighlight.Blue + ((Self->ColSelect.Blue - Self->ColBkgdHighlight.Blue)>>1);
                  rgbBkgd.Alpha = alpha;
                  rgbBkgd.Red   = red;
                  rgbBkgd.Green = green;
                  rgbBkgd.Blue  = blue;
               }
               else rgbBkgd = Self->ColBkgdHighlight;
            }
         }
         else if (node->Flags & NODE_SELECTED) { // Draw a background for the item to indicate that it is selected
            rgbBkgd = Self->ColSelect;
//            linebreak = FALSE;
         }
#if 0
         else if ((Self->Style IS VIEW_GROUP_TREE) and (Self->TreeIndex IS 1)) { // In group-tree mode, top-level tree folders have slightly darker backgrounds which vary according to whether they are open or closed.
            if (node->Flags & NODE_CHILDREN) {
               if ((Self->Style IS VIEW_GROUP_TREE) and (node->Flags & NODE_OPEN)) {
                  rgbBkgd.Alpha = 10;
               }
               else rgbBkgd.Alpha = 5;
               linebreak = 0;
            }
         }
#endif

         if (rgbBkgd.Alpha) {
            LONG save;
            save = Bitmap->Clip.Right;
            Bitmap->Clip.Right = glSaveClipRight;

            Bitmap->Opacity = rgbBkgd.Alpha;
            gfxDrawRectangle(Bitmap, Self->Layout->BoundX, ny, Self->Layout->BoundWidth, Self->LineHeight, PackPixelRGB(Bitmap,&rgbBkgd), TRUE);
            Bitmap->Opacity = 255;

            Bitmap->Clip.Right = save;
         }

         // Draw horizontal tree branches

         if (Self->GfxFlags & VGF_BRANCHES) {
            if (node->Flags & NODE_TREEBOX) {
               Bitmap->Opacity = Self->ColBranch.Alpha;
               gfxDrawRectangle(Bitmap, nx+(expand->Width/2), ny+(Self->LineHeight/2), 11+(Self->IconWidth/2), 1, col_branch, TRUE);
               Bitmap->Opacity = 255;
            }
            else if (node->Indent > 1) {
               Bitmap->Opacity = Self->ColBranch.Alpha;
               gfxDrawRectangle(Bitmap, nx+(expand->Width/2), ny+(Self->LineHeight/2), 11+(Self->IconWidth/2), 1, col_branch, TRUE);
               Bitmap->Opacity = 255;
            }
         }

         // Draw the text

         char buffer[400];
         XMLTag *vtag;

         get_col_value(Self, tag, Self->Columns, buffer, sizeof(buffer), &vtag);

         if (vtag) {
            format_value(Self, buffer, sizeof(buffer), Self->Columns->Type);

            font->X = nx + expand->Width + 4 + Self->IconWidth + GAP_ICON_TEXT;
            font->Y = ny;

            if (!(Surface->Flags & RNF_DISABLED)) {
               if ((node->Flags & NODE_SELECTED)) {
                  if (Self->ColSelectFont.Alpha) font->Colour = Self->ColSelectFont;
               }
               else if (tag->Index IS Self->HighlightTag) {
                  if (Self->ColHighlight.Alpha) font->Colour = Self->ColHighlight;
               }
            }
#if 0
            if ((tag->Index IS Self->HighlightTag) and (!Self->ClickHeld) and (!(Surface->Flags & RNF_DISABLED))) {
               font->Colour = Self->ColHighlight;
            }
            else font->Colour = node->FontRGB;
#endif
            SetString(font, FID_String, buffer);
            GetLong(font, FID_Width, &strwidth);
            acDraw(font);
         }
      }

      // Draw children if the item is open

      if (node->Flags & NODE_OPEN) {
         if ((child = tag->Child)) {
            LONG childcount = draw_tree(Self, Surface, Bitmap, tag->Child, Y);
            if (childcount) node->Flags |= NODE_CHILDREN;
            else node->Flags &= ~NODE_CHILDREN;
         }
      }

      // Draw the icon.  If no icon is available, draw a dummy icon

      if (clip) {
         if (Self->Flags & VWF_NO_ICONS) {

         }
         else if (node->Icon) {
            objBitmap *iconbmp;
            if ((node->Flags & (NODE_OPEN|NODE_SELECTED)) and (node->IconOpen)) iconbmp = node->IconOpen;
            else iconbmp = node->Icon;

            if (Surface->Flags & RNF_DISABLED) iconbmp->Opacity = 128;

            gfxCopyArea(iconbmp, Bitmap, BAF_BLEND, 0, 0, iconbmp->Width, iconbmp->Height,
               nx + expand->Width + 4 + ((Self->IconWidth - iconbmp->Width)>>1),
               ny + ((Self->LineHeight - iconbmp->Height)>>1));

            if (Surface->Flags & RNF_DISABLED) iconbmp->Opacity = 255;
         }
         else {
            Bitmap->Opacity = Self->ColBranch.Alpha;
            gfxDrawEllipse(Bitmap,
               nx + expand->Width + 4 + ((Self->IconWidth - 10)>>1),
               ny + ((Self->LineHeight - 10)>>1),
               10, 10, col_branch, TRUE);
            Bitmap->Opacity = 255;
         }
      }

      itemcount++;
   }

   // Draw a vertical branch from the top to the bottom of the --child-- items

   if ((Self->GfxFlags & VGF_BRANCHES) and (firstnode)) {
      if ((node->Indent > 1) /*OR (Self->Style IS VIEW_GROUP_TREE)*/) {
         ny = firstnode->Y - (Self->LineHeight>>1) + Self->Layout->BoundY + Self->YPos;
         Bitmap->Opacity = Self->ColBranch.Alpha;
         gfxDrawRectangle(Bitmap, firstnode->X + (expand->Height>>1) + Self->Layout->BoundX + Self->XPos + Self->Layout->LeftMargin, ny,
            1, node->Y + (Self->LineHeight>>1) + Self->Layout->BoundY + Self->YPos - ny, col_branch, TRUE);
         Bitmap->Opacity = 255;
      }
   }

   // Draw the open/close boxes for each item

   for (auto tag=Root; tag; tag=tag->Next) {
      if (!(node = (view_node *)tag->Private)) continue;
      if (!(node->Flags & NODE_ITEM)) continue;

      nx = node->X + Self->XPos + Self->Layout->BoundX + Self->Layout->LeftMargin;
      ny = node->Y + Self->YPos + Self->Layout->BoundY;

      if (node->Flags & NODE_TREEBOX) {
         if (node->Flags & NODE_CHILDREN) {
            if (node->Flags & NODE_OPEN) {
               gfxCopyArea(collapse, Bitmap, 0, 0, 0, collapse->Width, collapse->Height,
                  nx, ny+((Self->LineHeight-collapse->Height)/2));
            }
            else {
               gfxCopyArea(expand, Bitmap, 0, 0, 0, expand->Width, expand->Height,
                  nx, ny+((Self->LineHeight-expand->Height)/2));
            }
         }
         else {
            // Empty box
            Bitmap->Opacity = 255;
            gfxDrawRectangle(Bitmap, nx, ny+((Self->LineHeight-expand->Height)/2), expand->Width, expand->Height, PackPixel(Bitmap,250,250,250), TRUE);      // Fill colour
            gfxDrawRectangle(Bitmap, nx, ny+((Self->LineHeight-expand->Height)/2), expand->Width, expand->Height, PackPixel(Bitmap,130,130,130), FALSE);     // Border
            gfxDrawRectangle(Bitmap, nx+1, ny+((Self->LineHeight-expand->Height)/2)+1, expand->Width-2, expand->Height-2, PackPixel(Bitmap,230,230,230), FALSE); // Border softening
            Bitmap->Opacity = 255;
         }
      }
   }

   if ((node) and (Y)) *Y = node->Y + node->Height;

   return itemcount;
}

//****************************************************************************
// Draws column buttons.

static void draw_button(objView *Self, objBitmap *Bitmap, LONG X, LONG Y, LONG Width, LONG Height)
{
   Height--;

   if (Self->ButtonThickness IS 0) {
      gfxDrawRectangle(Bitmap, X+Width-1, Y, 1, Height, PackPixelRGBA(Bitmap, &Self->ButtonHighlight), BAF_FILL|BAF_BLEND);
      return;
   }
   else if (Self->ButtonThickness < 0) return;

   gfxDrawRectangle(Bitmap, X, Y,        Width, 1,     PackPixelRGBA(Bitmap, &Self->ButtonHighlight), BAF_FILL|BAF_BLEND); // Highlight (Top)
   gfxDrawRectangle(Bitmap, X, Y,        1, Height,    PackPixelRGBA(Bitmap, &Self->ButtonHighlight), BAF_FILL|BAF_BLEND);   // Highlight (Left)
   gfxDrawRectangle(Bitmap, X+Width-1,   Y, 1, Height, PackPixelRGBA(Bitmap, &Self->ButtonShadow), BAF_FILL|BAF_BLEND); // Shadow (Right)
   gfxDrawRectangle(Bitmap, X, Y+Height, Width, 1,     PackPixelRGBA(Bitmap, &Self->ButtonShadow), BAF_FILL|BAF_BLEND); // Shadow (Bottom)

   if (Self->ButtonThickness > 1) {
      X++;
      Y++;
      Width  -= 2;
      Height -= 2;
      RGB8 bkgd = Self->ButtonHighlight;
      bkgd.Alpha = bkgd.Alpha / 2;
      gfxDrawRectangle(Bitmap, X, Y, Width-2, 1, PackPixelRGBA(Bitmap, &bkgd), BAF_FILL|BAF_BLEND); // Highlight (Inner-Top)
      gfxDrawRectangle(Bitmap, X, Y, 1, Height, PackPixelRGBA(Bitmap, &bkgd), BAF_FILL|BAF_BLEND); // Highlight (Inner-Left)

      bkgd = Self->ButtonShadow;
      bkgd.Alpha = bkgd.Alpha / 2;
      //gfxDrawRectangle(Bitmap, Width-1, Y, 1, Height, r, g, b, BAF_FILL|BAF_BLEND); // Shadow (Inner-Right)
      gfxDrawRectangle(Bitmap, X, Y+Height, Width, 1, PackPixelRGBA(Bitmap, &bkgd), BAF_FILL|BAF_BLEND);  // Shadow (Inner-Bottom)
   }
}

//****************************************************************************

static void draw_column_header(objView *Self, objBitmap *Bitmap, ClipRectangle *Clip, LONG AreaX, LONG AreaY,
   LONG AreaWidth, LONG AreaHeight)
{
   view_col *col;
   RGB8 *rgb;
   LONG cx, cy, ah, i;

   objFont *font;
   if (!Self->GroupFace) {
      font = Self->Font;
   }
   else if (!Self->GroupFont) {
      CreateObject(ID_FONT, NF_INTEGRAL, &Self->GroupFont,
         FID_Owner|TLONG,   Self->Head.UniqueID,
         FID_Face|TSTRING,  Self->GroupFace,
         TAGEND);

      if (!(font = Self->GroupFont)) font = Self->Font;
   }
   else font = Self->GroupFont;

   if (Self->GfxFlags & VGF_OUTLINE_TITLE) {
      font->Outline.Red   = 60;
      font->Outline.Green = 60;
      font->Outline.Blue  = 60;
      font->Outline.Alpha = 255;
   }
   else font->Outline.Alpha = 0;

   font->Bitmap = Bitmap;

   // Draw column buttons at the top

   LONG x = AreaX + Self->XPos;

   // Draw complete button background in one shot

   gfxDrawRectangle(Bitmap, AreaX, AreaY, AreaWidth, Self->ColumnHeight,
      PackPixelRGBA(Bitmap, &Self->ButtonBackground), BAF_FILL|BAF_BLEND);

   LONG colindex = 0;
   for (col=Self->Columns; col; col=col->Next, colindex++) {
      // Adjust clipping to match that of the current column

      if (x > Clip->Left) Bitmap->Clip.Left = x;
      if ((x + col->Width) < Clip->Right) Bitmap->Clip.Right = x + col->Width;

      // Draw column headers

      if (Self->GfxFlags & VGF_DRAW_TABLE) {
         if (Self->GfxFlags & VGF_NO_BORDER) rgb = &Self->ColHairline;
         else rgb = &Self->ButtonShadow;
         gfxDrawRectangle(Bitmap, x+col->Width-1, AreaY, 1, Self->ColumnHeight, PackPixelRGBA(Bitmap, rgb), BAF_FILL|BAF_BLEND);
      }
      else draw_button(Self, Bitmap, x, AreaY, col->Width, Self->ColumnHeight);

      // Draw sort arrow

      font->X = x + 4;
      font->Y = AreaY;
      font->AlignWidth = col->Width - 10;
      font->AlignHeight = Self->ColumnHeight;
      font->WrapEdge = x + col->Width;
      font->Colour = Self->ColButtonFont;

      if (col->Width >= 20) {
         if (Self->Sort[0] IS -(colindex+1)) {
            // Up arrow (descending sort)
            ah = 5;
            cx = x + col->Width - (ah*2) - 2;
            cy = font->Y + ((Self->ColumnHeight - ah)>>1);
            for (i=0; i < ah; i++) {
               gfxDrawRectangle(Bitmap, cx, cy, (i * 2), 1, PackPixelRGBA(Bitmap, &Self->ButtonHighlight), BAF_FILL|BAF_BLEND);
               cx--;
               cy++;
            }
            font->AlignWidth -= (ah * 2) + 6;
         }
         else if (Self->Sort[0] IS (colindex+1)) {
            // Down arrow (ascending sort)
            ah = 5;
            cx = x + col->Width - (ah*2) - 2;
            cy = font->Y + ((Self->ColumnHeight - ah)>>1) + ah;
            for (i=0; i < ah; i++) {
               gfxDrawRectangle(Bitmap, cx, cy, (i * 2), 1, PackPixelRGBA(Bitmap, &Self->ButtonHighlight), BAF_FILL|BAF_BLEND);
               cx--;
               cy--;
            }

            font->AlignWidth -= (ah * 2) + 6;
         }
      }

      // Draw text inside the button

      font->Align = ALIGN_VERTICAL;
      if (col->Flags & CF_RIGHTALIGN) font->Align |= ALIGN_RIGHT;

      SetString(font, FID_String, col->Text);
      acDraw(font);

      // Draw background for entire vertical column, if required

      if (col->Flags & CF_COLOUR) {
         gfxDrawRectangle(Bitmap, x, AreaY + Self->ColumnHeight, col->Width, AreaHeight - AreaY - Self->ColumnHeight,
            PackPixelRGBA(Bitmap, &col->Colour), BAF_BLEND|BAF_FILL);
      }

      // Draw hairlines between each column if this option is on

      if (Self->GfxFlags & (VGF_HAIRLINES|VGF_DRAW_TABLE)) {
         if ((col->Next) or (x+col->Width < AreaWidth)) {
            gfxDrawRectangle(Bitmap, x+col->Width-1, AreaY + Self->ColumnHeight, 1, AreaHeight - Self->ColumnHeight,
               PackPixelRGBA(Bitmap, &Self->ColHairline), BAF_BLEND|BAF_FILL);
         }
      }

      Bitmap->Clip.Left  = Clip->Left;
      Bitmap->Clip.Right = Clip->Right;

      x += col->Width;
   }

   font->Align = 0;

   // Draw an empty button to fill any left-over space to the right of the columns

   if ((x < AreaWidth) and (!(Self->GfxFlags & VGF_DRAW_TABLE))) {
      draw_button(Self, Bitmap, x, AreaY, 16000, Self->ColumnHeight);
   }
}

//****************************************************************************

static void draw_view(objView *Self, objSurface *Surface, objBitmap *Bitmap)
{
   view_node *node;
   objBitmap *expand, *collapse, *groupbmp;
   ClipRectangle clip;
   CSTRING str;

   Self->RedrawDue = FALSE;

   if (!Self->Layout->Visible) return;

   auto font = Self->Font;

   ClipRectangle save = Bitmap->Clip;

   LONG ax = Self->Layout->BoundX;
   LONG ay = Self->Layout->BoundY;
   LONG awidth = Self->Layout->BoundWidth;
   LONG aheight = Self->Layout->BoundHeight;
   LONG offset = 0; // Border offset

   if (Self->ColBorder.Alpha) {
      if (!(Self->GfxFlags & VGF_DRAW_TABLE)) {
         gfxDrawRectangle(Bitmap, ax, ay, awidth, aheight, PackPixelRGBA(Bitmap, &Self->ColBorder), BAF_BLEND);
         offset = 1;
      }
   }

   ax += offset;
   ay += offset;
   awidth -= offset<<1;
   aheight -= offset<<1;
   if (ax > Bitmap->Clip.Left) Bitmap->Clip.Left = ax;
   if (ay > Bitmap->Clip.Top)  Bitmap->Clip.Top = ay;
   if (ax + awidth < Bitmap->Clip.Right)   Bitmap->Clip.Right = ax + awidth;
   if (ay + aheight < Bitmap->Clip.Bottom) Bitmap->Clip.Bottom = ay + aheight;
   ax -= offset;
   ay -= offset;
   awidth += offset<<1;
   aheight += offset<<1;

   clip = Bitmap->Clip;
   glSaveClipRight = Bitmap->Clip.Right;

   if (Self->Style IS VIEW_GROUP_TREE) { // In group-tree mode, all the items at the root of the tree get their own title-bar and their children are displayed as trees.
      objBitmap *iconbmp;
      LONG ny, ix, ey;
      BYTE alt;

      if (Self->ColBackground.Alpha) {
         gfxDrawRectangle(Bitmap, ax, ay, awidth, aheight, PackPixelRGBA(Bitmap, &Self->ColBackground), BAF_BLEND|BAF_FILL);
      }

      if (Self->XML->TagCount < 1) goto exit;

      auto font = Self->Font;

      if (!Self->GroupFace) {
         font = Self->Font;
      }
      else if (!Self->GroupFont) {
         CreateObject(ID_FONT, NF_INTEGRAL, &Self->GroupFont,
            FID_Owner|TLONG,   Self->Head.UniqueID,
            FID_Face|TSTRING,  Self->GroupFace,
            TAGEND);

         if (!(font = Self->GroupFont)) font = Self->Font;
      }
      else font = Self->GroupFont;

      if (Self->GfxFlags & VGF_OUTLINE_TITLE) {
         font->Outline.Red   = 60;
         font->Outline.Green = 60;
         font->Outline.Blue  = 60;
         font->Outline.Alpha = 255;
      }
      else font->Outline.Alpha = 0;

      font->Bitmap = Bitmap;
      font->Align  = ALIGN_VERTICAL;

      if (!(expand = get_expand_bitmap(Self, Bitmap->BitsPerPixel))) goto exit;
      if (!(collapse = get_collapse_bitmap(Self, Bitmap->BitsPerPixel))) goto exit;

      ny = ay;
      ey = ay;
      alt = 0;
      for (auto tag=Self->XML->Tags[0]; tag; tag=tag->Next) {
         if ((!tag->Private) or (!(((view_node *)tag->Private)->Flags & NODE_ITEM))) continue;

         node = (view_node *)tag->Private;
         ny = ay + node->Y + Self->YPos;
         ey = node->Y + node->Height;

         if (node->Flags & NODE_OPEN) {
            draw_tree(Self, Surface, Bitmap, tag->Child, &ey);
            if ((node->Flags & NODE_OPEN) and (Self->GfxFlags & VGF_GROUP_SHADOW)) {
               draw_shadow(Self, Bitmap, ny+node->Height);
            }
         }

         if (Self->GfxFlags & VGF_ALT_GROUP) { // Group graphic - button style background
            alt ^= 1;
            if (alt) groupbmp = Self->GroupBitmap;
            else groupbmp = Self->SelectBitmap;
         }
         else {
            if (node->Flags & NODE_OPEN) groupbmp = Self->SelectBitmap ? Self->SelectBitmap : Self->GroupBitmap;
            else groupbmp = Self->GroupBitmap ? Self->GroupBitmap : Self->SelectBitmap;
         }

         if (groupbmp) {
            for (ix=ax; ix < ax+awidth; ix += groupbmp->Width) {
               gfxCopyArea(groupbmp, Bitmap, 0, 0, 0, groupbmp->Width, node->Height, ix + Self->XPos, ny);
            }
         }
         else if (node->Flags & NODE_OPEN) {
            gfxDrawRectangle(Bitmap, ax, ny, awidth, node->Height, PackPixel(Bitmap,180,180,200), BAF_FILL);
            gfxDrawRectangle(Bitmap, ax, ny, awidth, 1, PackPixel(Bitmap,200,200,220), BAF_FILL);
            gfxDrawRectangle(Bitmap, ax, ny+node->Height-1, awidth, 1, PackPixel(Bitmap,120,120,140), BAF_FILL);
         }
         else {
            gfxDrawRectangle(Bitmap, ax, ny, awidth, node->Height, PackPixel(Bitmap,160,160,180), BAF_FILL);
            gfxDrawRectangle(Bitmap, ax, ny, awidth, 1, PackPixel(Bitmap,180,180,200), BAF_FILL);
            gfxDrawRectangle(Bitmap, ax, ny+node->Height-1, awidth, 1, PackPixel(Bitmap,140,140,150), BAF_FILL);
         }

         // Draw group icon and title

         ix = ax + 3 + Self->XPos;
#if 0
         if (node->Flags & NODE_CHILDREN) {
            // Square background box
            #if 0
               gfxCopyArea(expand, Bitmap, 0, 0, 0, expand->Width, expand->Height, ix, y+((node->Height-expand->Height)/2) );
               ix += expand->Width + 2;
            #else
               // Draw a + or - as appropriate
               gfxDrawRectangle(Bitmap, ix+3, y+((node->Height-expand->Height)/2)+expand->Height/2, expand->Width-6, 1, PackPixel(Bitmap,100,100,100), BAF_FILL);
               if (!(node->Flags & NODE_OPEN)) {
                  gfxDrawRectangle(Bitmap, ix + expand->Width/2, y+((node->Height-expand->Height)/2)+3, 1, expand->Height-6, PackPixel(Bitmap,100,100,100), BAF_FILL);
               }
               ix += expand->Width + 2;
            #endif
         }
         else {
            // If no +/-, darken the box
/*
            gfxCopyArea(box, Bitmap, 0, 0, 0, box->Width, box->Height, ix, y+((node->Height-box->Height)/2) );
            gfxDrawRectangle(Bitmap, ix, y+((node->Height-box->Height)/2), box->Width, box->Height, PackPixelA(Bitmap,0,0,0,20), BAF_FILL|BAF_BLEND);
*/
            ix += expand->Width + 2;
         }
#endif
         if (node->Icon) {
            if ((node->Flags & (NODE_OPEN|NODE_SELECTED)) and (node->IconOpen)) iconbmp = node->IconOpen;
            else iconbmp = node->Icon;

            if (Surface->Flags & RNF_DISABLED) iconbmp->Opacity = 128;

            gfxCopyArea(iconbmp, Bitmap, BAF_BLEND, 0, 0, iconbmp->Width, iconbmp->Height,
               ix + ((node->Height - iconbmp->Width)/2),
               ny + ((node->Height - iconbmp->Height)/2));
            ix += node->Height + 4;

            if (Surface->Flags & RNF_DISABLED) iconbmp->Opacity = 255;
         }
         else ix += 4;

         str = get_nodestring(Self, node);
         if (str) {
            font->X = ix;
            font->Y = ny;
            font->Colour = Self->ColTitleFont;
            font->AlignHeight = groupbmp->Height;
            SetString(font, FID_String, str);
            acDraw(font);
         }
      }

      // Darken any unused area at the bottom of the group view

      if (Self->ColGroupShade.Alpha) {
         ey += ay + Self->YPos;
         gfxDrawRectangle(Bitmap, ax, ey, awidth, ay + aheight - ey, PackPixelRGBA(Bitmap, &Self->ColGroupShade), BAF_BLEND|BAF_FILL);
      }
   }
   else if (Self->Style IS VIEW_TREE) {
      if (Self->ColBackground.Alpha) { // Background
         gfxDrawRectangle(Bitmap, ax, ay, awidth, aheight, PackPixelRGBA(Bitmap, &Self->ColBackground), BAF_BLEND|BAF_FILL);
      }

      if (Self->XML->TagCount < 1) goto exit;

      draw_tree(Self, Surface, Bitmap, Self->XML->Tags[0], NULL);
   }
   else if ((Self->Style IS VIEW_LIST) or (Self->Style IS VIEW_LONG_LIST)) {
      RGB8 rgbBkgd;
      LONG x, y, index, end_y;
      BYTE alt = FALSE;
      if ((Self->Style IS VIEW_LONG_LIST) and (Self->ColAltBackground.Alpha)) alt = TRUE;

      if ((alt IS FALSE) and (Self->ColBackground.Alpha)) {
         gfxDrawRectangle(Bitmap, ax, ay, awidth, aheight, PackPixelRGBA(Bitmap, &Self->ColBackground), BAF_BLEND|BAF_FILL);
      }

      if (Self->XML->TagCount < 1) goto exit;

      font->Bitmap = Bitmap;

      end_y = ay;
      index = 0;
      for (auto tag=Self->XML->Tags[0]; tag; tag = tag->Next) {
         if (!(node = (view_node *)tag->Private)) continue;

         index++;

         x = ax + node->X + Self->XPos;
         y = ay + node->Y + Self->YPos;
         end_y = y + Self->LineHeight;

         if (x + node->Width <= Bitmap->Clip.Left) continue;
         if (y + node->Height <= Bitmap->Clip.Top) continue;
         if (x >= Bitmap->Clip.Right) break;
         if (y >= Bitmap->Clip.Bottom) break;

         // If the item is selected, draw a highlight for it

         rgbBkgd.Red = 0;
         rgbBkgd.Green = 0;
         rgbBkgd.Blue = 0;
         rgbBkgd.Alpha = 0;
         font->Colour = node->FontRGB;
         if (tag->Index IS Self->HighlightTag) {
            if (Self->ColBkgdHighlight.Alpha) {
               rgbBkgd = Self->ColBkgdHighlight;

               if (node->Flags & NODE_SELECTED) {
                  WORD red, green, blue, alpha;
                  alpha = Self->ColBkgdHighlight.Alpha + ((Self->ColSelect.Alpha - Self->ColBkgdHighlight.Alpha)>>1);
                  red   = Self->ColBkgdHighlight.Red + ((Self->ColSelect.Red - Self->ColBkgdHighlight.Red)>>1);
                  green = Self->ColBkgdHighlight.Green + ((Self->ColSelect.Green - Self->ColBkgdHighlight.Green)>>1);
                  blue  = Self->ColBkgdHighlight.Blue + ((Self->ColSelect.Blue - Self->ColBkgdHighlight.Blue)>>1);
                  rgbBkgd.Alpha = alpha;
                  rgbBkgd.Red   = red;
                  rgbBkgd.Green = green;
                  rgbBkgd.Blue  = blue;
               }
               else rgbBkgd = Self->ColBkgdHighlight;
            }
            else if (node->Flags & NODE_SELECTED) rgbBkgd = Self->ColSelect;

            if (Self->ColHighlight.Alpha) font->Colour = Self->ColHighlight;
         }
         else if (node->Flags & NODE_SELECTED) {
            if (Self->ColSelect.Alpha) rgbBkgd = Self->ColSelect;
            if (Self->ColSelectFont.Alpha) font->Colour = Self->ColSelectFont;
         }

         if ((alt) and (rgbBkgd.Alpha < 255)) {  // Draw line background if alternate line colours are enabled
            if (index & 1) {
               gfxDrawRectangle(Bitmap, x, y, node->Width, Self->LineHeight, PackPixelRGBA(Bitmap, &Self->ColAltBackground), BAF_BLEND|BAF_FILL);
            }
            else gfxDrawRectangle(Bitmap, x, y, node->Width, Self->LineHeight, PackPixelRGBA(Bitmap, &Self->ColBackground), BAF_BLEND|BAF_FILL);
         }

         if (rgbBkgd.Alpha) {  // Draw the highlight
            gfxDrawRectangle(Bitmap, x, y, node->Width, Self->LineHeight, PackPixelRGBA(Bitmap, &rgbBkgd), BAF_BLEND|BAF_FILL);
         }

         if (node->Icon) { // Draw the icon on the left
            if (Surface->Flags & RNF_DISABLED) node->Icon->Opacity = 128;
            gfxCopyArea(node->Icon, Bitmap, BAF_BLEND, 0, 0, node->Icon->Width, node->Icon->Height,
               x + Self->Layout->LeftMargin + ((Self->IconWidth - node->Icon->Width)/2),
               y + ((Self->LineHeight - node->Icon->Height)/2));
            if (Surface->Flags & RNF_DISABLED) node->Icon->Opacity = 255;
         }

         str = get_nodestring(Self, node); // Draw the text alongside the icon
         if (str) {
            font->X = x + Self->Layout->LeftMargin + Self->IconWidth + 2;
            font->Y = y + ((Self->LineHeight - font->Height)/2);
            font->WrapEdge = x + node->Width;

            if (!(Surface->Flags & RNF_DISABLED)) {
               if ((node->Flags & NODE_SELECTED)) {
                  if (Self->ColSelectFont.Alpha) font->Colour = Self->ColSelectFont;
               }
               else if (tag->Index IS Self->HighlightTag) {
                  if (Self->ColHighlight.Alpha) font->Colour = Self->ColHighlight;
               }
            }

            SetString(font, FID_String, str);

            if (Surface->Flags & RNF_DISABLED) font->Colour.Alpha >>= 1;

            acDraw(font);

            font->Colour.Alpha = 255;
         }
      }

      if ((alt) and (Self->ColBackground.Alpha)) {
         gfxDrawRectangle(Bitmap, ax, end_y, awidth, aheight - end_y, PackPixelRGBA(Bitmap, &Self->ColBackground), BAF_BLEND|BAF_FILL);
      }
   }
   else if (Self->Style IS VIEW_ICON) {
      if (Self->ColBackground.Alpha) {
         gfxDrawRectangle(Bitmap, ax, ay, awidth, aheight, PackPixelRGBA(Bitmap, &Self->ColBackground), BAF_BLEND|BAF_FILL);
      }

      if (Self->XML->TagCount < 1) goto exit;
   }
   else if (Self->Style IS VIEW_COLUMN_TREE) {
      draw_column_header(Self, Bitmap, &clip, ax, ay, awidth, aheight);

      if (ay + Self->ColumnHeight > clip.Top) { // Background
         Bitmap->Clip.Top = ay + Self->ColumnHeight;
         if (Bitmap->Clip.Top >= Bitmap->Clip.Bottom) goto exit;
      }

      if ((Self->ColBackground.Alpha) and (!Self->ColAltBackground.Alpha)) {
         gfxDrawRectangle(Bitmap, ax, ay+Self->ColumnHeight, awidth, aheight - Self->ColumnHeight,
            PackPixelRGBA(Bitmap, &Self->ColBackground), BAF_BLEND|BAF_FILL);
      }

      if (Self->XML->TagCount < 1) goto exit;

      // Tree on the left

      if (ax + Self->Columns->Width < Bitmap->Clip.Right) {
         Bitmap->Clip.Right = ax + Self->Columns->Width;
      }

      draw_tree(Self, Surface, Bitmap, Self->XML->Tags[0], NULL);

      Bitmap->Clip = clip;

      draw_columns(Self, Surface, Bitmap, &clip, ax, ay, awidth, aheight);  // Draw the column items
   }
   else if (Self->Style IS VIEW_COLUMN) {
      draw_column_header(Self, Bitmap, &clip, ax, ay, awidth, aheight);

      if ((Self->ColBackground.Alpha) and (!Self->ColAltBackground.Alpha)) { // Background
         gfxDrawRectangle(Bitmap, ax, ay+Self->ColumnHeight, awidth, aheight - Self->ColumnHeight,
            PackPixelRGBA(Bitmap, &Self->ColBackground), BAF_BLEND|BAF_FILL);
      }

      draw_columns(Self, Surface, Bitmap, &clip, ax, ay, awidth, aheight); // Draw the items
   }

exit:
   Bitmap->Clip = save;
}

//****************************************************************************

static LONG glRowIndex = 0, glRowEnd = 0;

static void draw_column_branch(objView *Self, objSurface *Surface, objBitmap *Bitmap,
   ClipRectangle *Clip, XMLTag *Tag, LONG ax, LONG ay, LONG awidth, LONG aheight)
{
   parasol::Log log(__FUNCTION__);
   view_node *node, *tagnode;
   XMLTag *vtag;
   LONG x, y;
   char buffer[400];

   if (!Tag) return;

   //log.traceBranch("Tag: %d", Tag->Index);

   auto font = Self->Font;
   while (Tag) {
      if (!(node = (view_node *)Tag->Private)) {
         log.warning("No private node for tag #%d", Tag->Index);
         break;
      }

      if (!node->Width) goto next;

      glRowIndex++;

      x = ax + Self->XPos; // node->X is not used as it is always 0 (for COLUMN mode) or only has meaning when drawing a tree
      y = ay + node->Y + Self->YPos;
      glRowEnd = y + Self->LineHeight;

      if (x + node->Width  <= Clip->Left) break;
      if (y + node->Height <= Clip->Top) goto next;
      if (x >= Clip->Right) break;
      if (y >= Clip->Bottom) break;

      Bitmap->Clip.Left  = Clip->Left;
      Bitmap->Clip.Right = Clip->Right;

      font->Colour = node->FontRGB;

      // If the item is selected, draw a highlight for it

      {
         RGB8 rgbBkgd = { 0, 0, 0, 0 };
         if ((Self->Style != VIEW_COLUMN_TREE) AND (!(Surface->Flags & RNF_DISABLED))) {
            if (Tag->Index IS Self->HighlightTag) {
               if (Self->ColBkgdHighlight.Alpha) {
                  if (node->Flags & NODE_SELECTED) {
                     rgbBkgd.Alpha = Self->ColBkgdHighlight.Alpha + ((Self->ColSelect.Alpha - Self->ColBkgdHighlight.Alpha)>>1);
                     rgbBkgd.Red   = Self->ColBkgdHighlight.Red + ((Self->ColSelect.Red - Self->ColBkgdHighlight.Red)>>1);
                     rgbBkgd.Green = Self->ColBkgdHighlight.Green + ((Self->ColSelect.Green - Self->ColBkgdHighlight.Green)>>1);
                     rgbBkgd.Blue  = Self->ColBkgdHighlight.Blue + ((Self->ColSelect.Blue - Self->ColBkgdHighlight.Blue)>>1);
                  }
                  else rgbBkgd = Self->ColBkgdHighlight;
               }
               else if (node->Flags & NODE_SELECTED) rgbBkgd = Self->ColSelect;
               if (Self->ColHighlight.Alpha) font->Colour = Self->ColHighlight;
            }
            else if (node->Flags & NODE_SELECTED) {
               if (Self->ColSelect.Alpha) rgbBkgd = Self->ColSelect;
               if (Self->ColSelectFont.Alpha) font->Colour = Self->ColSelectFont;
            }
         }

         // Draw line background if alternate line colours are enabled

         if ((Self->ColAltBackground.Alpha > 0) and (rgbBkgd.Alpha < 255)) {
            if (glRowIndex & 1) {
               gfxDrawRectangle(Bitmap, x, y, node->Width, Self->LineHeight, PackPixelRGBA(Bitmap, &Self->ColAltBackground), BAF_BLEND|BAF_FILL);
            }
            else gfxDrawRectangle(Bitmap, x, y, node->Width, Self->LineHeight, PackPixelRGBA(Bitmap, &Self->ColBackground), BAF_BLEND|BAF_FILL);
         }

         if (rgbBkgd.Alpha) {
            gfxDrawRectangle(Bitmap, x, y, node->Width, Self->LineHeight, PackPixelRGBA(Bitmap, &rgbBkgd), BAF_BLEND|BAF_FILL);
         }
      }

      if (FALSE) { // Draw hairlines for selected items
         if (Self->GfxFlags & (VGF_HAIRLINES|VGF_DRAW_TABLE)) {
            RGB8 *rgb;
            if (((LONG *)&Self->ColSelectHairline)[0]) rgb = &Self->ColSelectHairline;
            else rgb = &Self->ColHairline;

            LONG hx = x;
            for (auto col=Self->Columns; col; col=col->Next) {
               if ((col->Next) or (hx+col->Width < awidth)) {
                  gfxDrawRectangle(Bitmap, hx+col->Width-1, y, 1, Self->LineHeight, PackPixelRGBA(Bitmap, rgb), BAF_BLEND|BAF_FILL);
               }
               hx += col->Width;
            }
         }
      }

      for (auto col=Self->Columns; col; x += col->Width, col=col->Next) {
         if ((Self->Style IS VIEW_COLUMN_TREE) and (col IS Self->Columns)) {
            // In COLUMNTREE mode, the first column is ignored because the tree is drawn in column 1.
            continue;
         }

         // Adjust clipping to match that of the current column

         if (x > Clip->Left) Bitmap->Clip.Left = x;
         else Bitmap->Clip.Left = Clip->Left;

         if ((x + col->Width) < Clip->Right) Bitmap->Clip.Right = x + col->Width;
         else Bitmap->Clip.Right = Clip->Right;

         get_col_value(Self, Tag, col, buffer, sizeof(buffer), &vtag);
         if (!vtag) continue;

         tagnode = (view_node *)vtag->Private;
         if (!tagnode) continue;

         if (col->Type IS CT_CHECKBOX) {
            static BYTE tick_error = ERR_Okay;
            LONG checked;

            if (buffer[0]) {
               checked = StrToInt(buffer);
               if ((!checked) and ((buffer[0] IS 'y') or (buffer[0] IS 'Y'))) checked = 1;
            }
            else checked = 0;

            if ((!glTick) and (!tick_error)) {
               objPicture **ptr;
               if (KeyGet(glCache, KEY_TICK, &ptr, NULL) != ERR_Okay) {
                  if (!(tick_error = CreateObject(ID_PICTURE, 0, &glTick,
                        FID_Path|TSTR,   "templates:images/tick",
                        FID_Flags|TLONG, PCF_FORCE_ALPHA_32,
                        TAGEND))) {
                     SetOwner(glTick, modWidget);
                     KeySet(glCache, KEY_TICK, &glTick, sizeof(objPicture **));
                  }
               }
               else glTick = ptr[0];
            }

            if ((!glTick) or (Self->LineHeight < glTick->Bitmap->Height+2)) {
               LONG csize;
               if ((csize = Self->LineHeight - 2) >= 6) {
                  ULONG colour;
                  if (checked) colour = bmpGetColour(Bitmap, 0, 0, 0, 255);
                  else  colour = bmpGetColour(Bitmap, 0, 0, 0, 128);

                  LONG tx = 0;
                  LONG ty = csize * 0.75;
                  LONG tx2 = csize * 0.25;
                  LONG ty2 = csize;
                  gfxDrawLine(Bitmap, tx, ty, tx2, ty2, colour);
                  gfxDrawLine(Bitmap, tx, ty-1, tx2, ty2-1, colour);

                  tx = tx2;
                  ty = ty2;
                  tx2 = csize;
                  ty2 = csize * 0.25;
                  gfxDrawLine(Bitmap, tx, ty, tx2, ty2, colour);
                  gfxDrawLine(Bitmap, tx, ty-1, tx2, ty2-1, colour);
               }
            }
            else {
               if (checked) glTick->Bitmap->Opacity = 255;
               else glTick->Bitmap->Opacity = 40;

               LONG cx = x + ((col->Width - glTick->Bitmap->Width)/2);
               LONG cy = y + ((Self->LineHeight - glTick->Bitmap->Height)/2);
               gfxCopyArea(glTick->Bitmap, Bitmap, BAF_BLEND, 0, 0, glTick->Bitmap->Width, glTick->Bitmap->Height, cx, cy);
            }

            continue;
         }

         // Draw the icon for this column

         if ((col->Flags & CF_SHOWICONS) and (tagnode->Icon)) {
            objBitmap *iconbmp = tagnode->Icon;
            if (Surface->Flags & RNF_DISABLED) iconbmp->Opacity = 128;

            gfxCopyArea(iconbmp, Bitmap, BAF_BLEND, 0, 0, iconbmp->Width, iconbmp->Height,
               x + 2 + ((Self->IconWidth - iconbmp->Width)/2), y + ((Self->LineHeight - iconbmp->Height)/2));

            if (Surface->Flags & RNF_DISABLED) iconbmp->Opacity = 255;
         }

         if (buffer[0]) {
            format_value(Self, buffer, sizeof(buffer), col->Type);

            font->Align = ALIGN_VERTICAL;
            font->AlignHeight = Self->LineHeight;
            if (col->Flags & CF_RIGHTALIGN) {
               font->Align |= ALIGN_RIGHT;
               font->AlignWidth = col->Width - 8;
            }

            if ((tagnode->Icon) and (col->Flags & CF_SHOWICONS)) font->X = x + Self->IconWidth + 4;
            else font->X = x + 4;

            font->Y = ay + Self->YPos + ((view_node *)(Tag->Private))->Y;
            font->WrapEdge = x + col->Width;

            SetString(font, FID_String, buffer);
            if (Surface->Flags & RNF_DISABLED) font->Colour.Alpha >>= 1;
            acDraw(font);
            font->Colour.Alpha = 255;
         } // if (buffer[0])
      } // for (col=Self->Columns; col; x += col->Width, col=col->Next)

next:
      if (Self->Style IS VIEW_COLUMN_TREE) {
         if ((node->Flags & NODE_OPEN) and (node->Flags & NODE_CHILDREN)) {
            draw_column_branch(Self, Surface, Bitmap, Clip, Tag->Child, ax, ay, awidth, aheight);
         }
      }

      Tag = Tag->Next;
   }
}

//****************************************************************************

static void draw_columns(objView *Self, objSurface *Surface, objBitmap *Bitmap, ClipRectangle *Clip,
   LONG ax, LONG ay, LONG awidth, LONG aheight)
{
   if (ay + Self->ColumnHeight > Clip->Top) {
      Bitmap->Clip.Top = ay + Self->ColumnHeight;
      if (Bitmap->Clip.Top >= Bitmap->Clip.Bottom) return;
   }

   objFont *font = Self->Font;
   font->Bitmap = Bitmap;

   glRowEnd = ay + Self->ColumnHeight;
   glRowIndex = 0;

   draw_column_branch(Self, Surface, Bitmap, Clip, Self->XML->Tags[0], ax, ay, awidth, aheight);

   font->Align = 0;

   Bitmap->Clip = *Clip;

   // Clear the end of the list if alternate line colours are being used

   if ((Self->ColAltBackground.Alpha > 0) and (Self->ColBackground.Alpha)) {
      gfxDrawRectangle(Bitmap, ax, glRowEnd, awidth, aheight, PackPixelRGBA(Bitmap, &Self->ColBackground), BAF_FILL|BAF_BLEND);
   }

   if (Self->GfxFlags & VGF_DRAW_TABLE) {
      // Draw a border around the button background if in table mode.  Can be avoided with the NOBORDER option.

      if (!(Self->GfxFlags & VGF_NO_BORDER)) {
         gfxDrawRectangle(Bitmap, ax, ay, awidth, Self->ColumnHeight, PackPixelRGBA(Bitmap, &Self->ButtonShadow), 0);
         gfxDrawRectangle(Bitmap, ax, ay, awidth, aheight, PackPixelRGBA(Bitmap, &Self->ButtonShadow), 0);
      }
      else gfxDrawRectangle(Bitmap, ax, ay + Self->ColumnHeight - 1, awidth, 1, PackPixelRGBA(Bitmap, &Self->ColHairline), BAF_FILL);
   }
}

//****************************************************************************
// Scrolls any given area of the document into view.

static BYTE check_item_visible(objView *Self, XMLTag *Tag)
{
   parasol::Log log(__FUNCTION__);

   if (Self->Flags & VWF_NO_SELECT_JMP) return FALSE;

   if (!Tag) return FALSE;
   auto node = (view_node *)Tag->Private;

   LONG left   = node->X;
   LONG top    = node->Y - Self->ColumnHeight;
   LONG bottom = top + node->Height;
   LONG right  = left + node->Width;

   LONG view_x = -Self->XPos;
   LONG view_y = -Self->YPos;
   LONG view_height = Self->Layout->BoundHeight - Self->ColumnHeight;
   if (Self->HBarVisible) view_height -= Self->HBarHeight;
   LONG view_width  = Self->Layout->BoundWidth;

   log.traceBranch("View: %dx%d, Item: %dx%d,%dx%d, Area: %dx%d,%dx%d", view_x, view_y, left, top, right, bottom, Self->Layout->BoundX, Self->Layout->BoundY, Self->Layout->BoundWidth, Self->Layout->BoundHeight);

   // Vertical

   if (top < view_y) {
      view_y = top;
      if (view_y < 0) view_y = 0;
   }
   else if (bottom > view_y + view_height) {
      view_y = bottom - view_height;
      if (view_y > Self->PageHeight - view_height) view_y = Self->PageHeight - view_height;
   }

   // Horizontal

   if ((Self->Style != VIEW_TREE) and (Self->Style != VIEW_GROUP_TREE) and (Self->Style != VIEW_COLUMN) and (Self->Style != VIEW_COLUMN_TREE)) {
      if (left < view_x) {
         view_x = left;
         if (view_x < 0) view_x = 0;
      }
      else if (right > view_x + view_width) {
         view_x = right - view_width;
         if (view_x > Self->PageWidth - view_width) view_x = Self->PageWidth - view_width;
      }
   }

   if ((-view_x != Self->XPos) or (-view_y != Self->YPos)) {
      acScrollToPoint(Self, view_x, view_y, 0, STP_X|STP_Y);
      calc_hscroll(Self);
      calc_vscroll(Self);
      return TRUE;
   }
   else return FALSE;
}

/******************************************************************************
** This function checks that the SelectedTag refers to a valid, selected item.  If it doesn't, the SelectedTag field is
** recalculated.
*/

static void check_selected_items(objView *Self, XMLTag *Tags)
{
   parasol::Log log(__FUNCTION__);

   log.traceBranch("check_selected_items(SelectedTag:%d/%d)", Self->SelectedTag, Self->XML->TagCount);

   if (Self->SelectedTag IS -1) return;

   if (Self->SelectedTag < Self->XML->TagCount) {
      view_node *node;
      if (!(node = (view_node *)Self->XML->Tags[Self->SelectedTag]->Private)) {
         log.trace("Private node is missing.");
         return;
      }

      if (node->Flags & NODE_SELECTED) {
         log.trace("Tag is already selected.");
         return;
      }
   }
   else log.traceWarning("SelectedTag is invalid.");

   for (auto scan=Tags; scan; scan=scan->Next) {
      auto node = (view_node *)scan->Private;
      if (node->Flags & NODE_SELECTED) {
         Self->SelectedTag = scan->Index;
         log.trace("Selected tag reset to %d", scan->Index);
         report_selection(Self, SLF_SELECTED, scan->Index);
         return;
      }
   }

   log.trace("Selected tag reset to nothing.");
   Self->SelectedTag = -1;
   report_selection(Self, SLF_SELECTED, -1);
}

//*****************************************************************************

static void draw_item(objView *Self, XMLTag *Tag)
{
   if (!Tag) return;

   auto node = (view_node *)Tag->Private;

   //log.trace("draw_item: %dx%d,%dx%d", node->X, node->Y, node->Width, node->Height);

   if ((Self->Style IS VIEW_TREE) or (Self->Style IS VIEW_GROUP_TREE) or (Self->Style IS VIEW_COLUMN_TREE)) {
      // Draw using the full width of the view
      acDrawAreaID(Self->Layout->SurfaceID, Self->Layout->BoundX,
         Self->Layout->BoundY + node->Y + Self->YPos, Self->Layout->BoundWidth, node->Height);
   }
   else {
      acDrawAreaID(Self->Layout->SurfaceID, Self->Layout->BoundX + node->X + Self->XPos,
         Self->Layout->BoundY + node->Y + Self->YPos, node->Width, node->Height);
   }
}

/*****************************************************************************
** Marks an item as selected and then partially redraws the view in order to show the selection.
**
** You can also use this function to deselect all tags, by passing a NULL pointer in the Tag argument.
*/

static BYTE select_item(objView *Self, XMLTag *Tag, LONG Flags, BYTE MultiSelect, BYTE Draggable)
{
   parasol::Log log(__FUNCTION__);
   LONG i;
   BYTE new_selection;

   log.traceBranch("Index: %d, MultiSelect: %d, Draggable: %d", (Tag) ? Tag->Index : -1, MultiSelect, Draggable);

   BYTE shiftkey = FALSE;
   BYTE ctrlkey = FALSE;
   BYTE deselect_all = -1;
   if ((Self->Flags & VWF_MULTI_SELECT) and (Self->Flags & VWF_DRAG_DROP)) {
      LONG lastindex, firstindex;

      LONG keystate = GetResource(RES_KEY_STATE);
      log.trace("Key state: $%.8x", keystate);
      if (keystate & KQ_SHIFT) shiftkey = TRUE;
      else if (keystate & KQ_CTRL) ctrlkey = TRUE;

      MultiSelect = FALSE;
      if (shiftkey) {
         // Highlight everything between the selectedtag and the new tag

         if (Self->SelectedTag < Tag->Index) {
            firstindex = Self->SelectedTag;
            lastindex  = Tag->Index;
         }
         else {
            firstindex = Tag->Index;
            lastindex  = Self->SelectedTag;
         }

         log.trace("The shift key is held, highlight from tag %d to %d", Self->SelectedTag, Tag->Index);

         XMLTag *scan;
         for (scan=Self->XML->Tags[0]; scan; scan=scan->Next) {
            if (scan->Index IS firstindex) break;
            auto node = (view_node *)scan->Private;
            if (node->Flags & NODE_SELECTED) {
               node->Flags &= ~NODE_SELECTED;
               draw_item(Self, scan);
            }
         }

         while ((scan) and (scan->Index <= lastindex)) {
            auto node = (view_node *)scan->Private;
            if (!(node->Flags & NODE_SELECTED)) {
               node->Flags |= NODE_SELECTED;
               draw_item(Self, scan);
            }
            scan = scan->Next;
         }

         while (scan) {
            auto node = (view_node *)scan->Private;
            if (node->Flags & NODE_SELECTED) {
               node->Flags &= ~NODE_SELECTED;
               draw_item(Self, scan);
            }
            scan = scan->Next;
         }

         check_item_visible(Self, Tag);
         report_selection(Self, SLF_ACTIVE|SLF_SELECTED|SLF_MULTIPLE, Tag->Index);
         return FALSE;
      }
      else if (ctrlkey) {
         log.trace("The ctrl key is held.");
         if (Self->Flags & VWF_MULTI_SELECT) MultiSelect = TRUE;
      }
      else {
         // No key is held.  If the tag is already selected, do nothing
         if ((Tag) and (((view_node *)Tag->Private)->Flags & NODE_SELECTED)) {
            log.trace("No key is held and the tag is already marked as selected.  DragActive: %d", Self->ActiveDrag);
            Self->ActiveDrag  = (Self->Flags & VWF_DRAG_DROP) ? Draggable : FALSE;
            Self->ActiveTag   = Tag->Index;
            Self->SelectedTag = Tag->Index;
            report_selection(Self, SLF_ACTIVE|SLF_SELECTED|SLF_MULTIPLE|Flags, Tag->Index);
            return FALSE;
         }
      }
   }

   if (deselect_all IS -1) {
      if ((!Tag) or (MultiSelect IS FALSE) or (!(Self->Flags & VWF_MULTI_SELECT))) {
         deselect_all = TRUE;
      }
      else deselect_all = FALSE;
   }

   // If we're in single-select mode or we are deselecting everything, scan for any existing selections and turn them off.

   if (deselect_all) {
      for (LONG index=0; Self->XML->Tags[index]; index++) {
         auto scan = Self->XML->Tags[index];
         auto node = (view_node *)scan->Private;
         if ((node->Flags & NODE_ITEM) and (node->Flags & NODE_SELECTED)) {
            node->Flags &= ~NODE_SELECTED;
            draw_item(Self, scan);
         }
      }
   }

   if (Tag) { // Select the new item
      auto node = (view_node *)Tag->Private;

      if ((!node->Width) and (Self->Style != VIEW_TREE)) {
         // Redundant nodes cannot be selected
         return FALSE;
      }

      if (node->Flags & NODE_SELECTED) {
         node->Flags &= ~NODE_SELECTED;
         draw_item(Self, Tag);
         new_selection = FALSE;
      }
      else {
         UBYTE redraw_tree;

         node->Flags |= NODE_SELECTED;

         redraw_tree = FALSE;
         if ((Self->Style IS VIEW_TREE) or (Self->Style IS VIEW_COLUMN_TREE) or (Self->Style IS VIEW_GROUP_TREE)) {
            // Open up parent nodes in the tree
            i = Tag->Index - 1;
            auto scan = Tag;
            while (scan->Prev) scan = scan->Prev;

            while (i >= 0) {
               if (Self->XML->Tags[i]->Child IS scan) {
                  node = (view_node *)Self->XML->Tags[i]->Private;
                  if (node->Flags & NODE_CHILDREN) {
                     if (!(node->Flags & NODE_OPEN)) {
                        node->Flags |= NODE_OPEN;
                        redraw_tree = TRUE;
                     }
                  }
                  scan = Self->XML->Tags[i];
                  while (scan->Prev) scan = scan->Prev;
               }
               i--;
            }
         }

         if (redraw_tree) {
            arrange_items(Self);

            if (!Self->RedrawDue) {
               Self->RedrawDue = TRUE;
               DelayMsg(AC_Draw, Self->Layout->SurfaceID, NULL);
            }
         }
         else draw_item(Self, Tag);

         new_selection = TRUE;
      }

      Self->ActiveDrag  = (Self->Flags & VWF_DRAG_DROP) ? Draggable : FALSE;
      Self->ActiveTag   = Tag->Index;
      Self->SelectedTag = Tag->Index;
      report_selection(Self, SLF_ACTIVE|SLF_SELECTED|Flags, Tag->Index);

      // Ensure that the newly selected item is visible

      check_item_visible(Self, Self->XML->Tags[Self->ActiveTag]);

      // Respond to the selection

      if ((new_selection) and (Self->Flags & VWF_SENSITIVE)) {
         // Sensitive mode means that we have to activate whenver a new item is selected
         for (i=0; i < Tag->TotalAttrib; i++) {
            if (!StrMatch(Tag->Attrib[i].Name, "insensitive"))  break;
         }
         if (i >= Tag->TotalAttrib) {
            acActivate(Self);
            return TRUE;
         }
      }
   }
   else {
      log.trace("No tag will be selected.");
      Self->ActiveDrag  = FALSE;
      Self->ActiveTag   = -1;
      Self->SelectedTag = -1;
      report_selection(Self, SLF_ACTIVE|SLF_SELECTED|Flags, -1);
   }

   return FALSE;
}

//****************************************************************************

static void key_event(objView *Self, evKey *Event, LONG Size)
{
   if (!(Event->Qualifiers & KQ_PRESSED)) return;

   if (Event->Qualifiers & KQ_CTRL) {

   }
   else switch(Event->Code) {
      case K_ENTER:
         acActivate(Self);
         if (Self->Flags & VWF_AUTO_DESELECT) deselect_item(Self);
         break;

      case K_DOWN:
         switch(Self->Style) {
            case VIEW_COLUMN:
            case VIEW_LIST:
            case VIEW_LONG_LIST:
            case VIEW_COLUMN_TREE:
               if (Self->ActiveTag IS -1) select_item(Self, Self->XML->Tags[0], SLF_KEYPRESS, FALSE, FALSE);
               else if (Self->XML->Tags[Self->ActiveTag]->Next) select_item(Self, Self->XML->Tags[Self->ActiveTag]->Next, SLF_KEYPRESS, FALSE, FALSE);
         }
         break;

      case K_UP:
         switch(Self->Style) {
            case VIEW_COLUMN:
            case VIEW_LIST:
            case VIEW_LONG_LIST:
            case VIEW_COLUMN_TREE:
               if (Self->ActiveTag IS -1) select_item(Self, Self->XML->Tags[0], SLF_KEYPRESS, FALSE, FALSE);
               else if (Self->XML->Tags[Self->ActiveTag]->Prev) select_item(Self, Self->XML->Tags[Self->ActiveTag]->Prev, SLF_KEYPRESS, FALSE, FALSE);
         }
         break;
   }
}

//****************************************************************************

static ERROR unload_icon(objView *Self, ULONG *Key)
{
   parasol::Log log(__FUNCTION__);
   if ((Key) and (Key[0])) {
      CachedIcon *ci;
      ERROR error;
      if (!(error = KeyGet(glCache, Key[0], &ci, NULL))) {
         ci->Counter--;
         if (ci->Counter IS 0) {
            log.trace("Key: $%x, Counter: %d, Removing bitmap %p", Key[0], ci->Counter, ci->Icon);
            acFree(ci->Icon);
            KeySet(glCache, Key[0], NULL, 0); // Remove the key
         }
         else log.trace("Key: $%x, Counter: %d", Key[0], ci->Counter);
      }
      else log.warning("Failed to find key $%x", Key[0]);
      Key[0] = 0;
      return error;
   }
   else return ERR_Args;
}

//****************************************************************************

static ERROR load_icon(objView *Self, CSTRING IconFile, objBitmap **Icon, ULONG *Key)
{
   parasol::Log log(__FUNCTION__);

   if (!StrCompare("icons:", IconFile, 6, 0)) IconFile += 6;

   log.traceBranch("%s", IconFile);

   CachedIcon *ci;
   ULONG key_hash = StrHash(IconFile, FALSE);
   *Icon = NULL;
   if (KeyGet(glCache, key_hash, &ci, NULL) != ERR_Okay) {
      SURFACEINFO *info;
      if (drwGetSurfaceInfo(Self->Layout->SurfaceID, &info) != ERR_Okay) info->BitsPerPixel = 32;

      if (!widgetCreateIcon(IconFile, "View", Self->IconFilter, Self->IconSize, Icon)) {
         log.msg("Caching new icon: '%s', Object: #%d", IconFile, Icon[0]->Head.UniqueID);
         SetOwner(Icon[0], modWidget);

         CachedIcon ci = { &Icon[0]->Head, 1 };
         KeySet(glCache, key_hash, &ci, sizeof(ci));
      }
   }
   else {
      ci->Counter++;
      Icon[0] = (objBitmap *)ci->Icon;
   }

   if (!Icon[0]) {
      log.warning("load_icon() failed to load '%s'", IconFile);
      return ERR_Failed;
   }
   else {
      Key[0] = key_hash;
      return ERR_Okay;
   }
}

//****************************************************************************

static BYTE deselect_item(objView *Self)
{
   parasol::Log log(__FUNCTION__);

   log.trace("deselect_item(%d)", Self->SelectedTag);

   if (Self->SelectedTag IS -1) return FALSE;

   if (Self->SelectedTag < Self->XML->TagCount) {
      auto node = (view_node *)Self->XML->Tags[Self->SelectedTag]->Private;
      if (node->Flags & NODE_SELECTED) {
         node->Flags &= ~NODE_SELECTED;
         draw_item(Self, Self->XML->Tags[Self->SelectedTag]);
      }
   }

   Self->SelectedTag = -1;
   report_selection(Self, SLF_SELECTED, -1);

   return TRUE;
}

//****************************************************************************

static void draw_shadow(objView *Self, objBitmap *Bitmap, LONG Y)
{
   if (!Self->Shadow) {
      GradientStop stops[2];

      stops[0].RGB.Red   = 0;
      stops[0].RGB.Green = 0;
      stops[0].RGB.Blue  = 0;
      stops[0].RGB.Alpha = 80.0 / 255.0;
      stops[0].Offset = 0;

      stops[1].RGB.Red   = 0;
      stops[1].RGB.Green = 0;
      stops[1].RGB.Blue  = 0;
      stops[1].RGB.Alpha = 0;
      stops[1].Offset = 1;

      if (!NewObject(ID_VECTORGRADIENT, NF_INTEGRAL, &Self->Shadow)) {
         SetArray(&Self->Shadow->Head, FID_Stops, stops, 2);
         if (acInit(Self->Shadow)) { acFree(Self->Shadow); Self->Shadow = NULL; }
      }
   }

   APTR path;
   if (!vecGenerateRectangle(Self->Layout->BoundX, Y, Self->Layout->BoundWidth, 4, &path)) {
      vecDrawPath(Bitmap, path, 0, NULL, &Self->Shadow->Head);
      vecFreePath(path);
   }
}

/*****************************************************************************
** This function prepares (or updates) XML tags so that they can be used in the view, by configuring the node
** information etc.
*/

static LONG prepare_xml(objView *Self, XMLTag *Root, CSTRING ItemName, LONG Limit)
{
   parasol::Log log(__FUNCTION__);
   LONG j;
   STRING str;

   //log.traceBranch("");

   LONG count = 0;
   if (Limit <= 0) Limit = 0x7fffffff;

   for (auto tag=Root; (tag) and (count < Limit); tag=tag->Next) {
      auto node = (view_node *)tag->Private;

      if (node->Flags & NODE_SELECTED) {
         if (Self->SelectedTag IS -1) {
            // NOTE: These are set directly, when prepare_xml() returns, the code that called this function should do the field notification.

            Self->SelectedTag = tag->Index;
            Self->ActiveTag = tag->Index;
         }
      }

      if (node->Flags & NODE_STRIPPED) continue;

      if (!(node->Flags & NODE_ITEM)) {
         // Strip-out return codes and trailing whitespace from content tags.

         if (!tag->Attrib->Name) {
            if ((str = tag->Attrib->Value)) {
               for (j=0; str[j]; j++) {
                  if (str[j] IS '\n') str[j] = ' ';
               }
               while ((j > 0) and (str[j-1] <= 0x20)) j--;
               str[j] = 0;
            }
            node->Flags |= NODE_STRIPPED;
            continue;
         }

         // Determine whether this is an actual item or just a column value

         if ((!ItemName) or (!StrMatch(tag->Attrib->Name, ItemName)) or
             (!StrCompare(Self->ItemNames, tag->Attrib->Name, 0, STR_WILDCARD))) {
            node->Flags |= NODE_ITEM;
         }
         else continue;

         // Set default colour for new items

         node->FontRGB.Red   = Self->ColItem.Red;
         node->FontRGB.Green = Self->ColItem.Green;
         node->FontRGB.Blue  = Self->ColItem.Blue;
         node->FontRGB.Alpha = Self->ColItem.Alpha;

         // Load newly referenced icons.  Icons must be referenced in the format "group/iconname".

         if ((!(Self->Flags & VWF_NO_ICONS)) and ((!node->Icon) or (!node->IconOpen))) {
            CSTRING iconfile;
            if ((iconfile = XMLATTRIB(tag, "icon"))) {
               load_icon(Self, iconfile, &node->Icon, &node->IconKey);
            }
            if ((iconfile = XMLATTRIB(tag, "iconopen"))) {
               load_icon(Self, iconfile, &node->IconOpen, &node->IconOpenKey);
            }
         }
      }

      node->ChildString = FALSE;
      if ((Self->TextAttrib) and ((str = XMLATTRIB(tag, Self->TextAttrib)))) {
         set_nodestring(Self, node, str);
      }
      else if ((tag->Child) and (!tag->Child->Attrib->Name)) {
         node->ChildString = TRUE;
         set_nodestring(Self, node, tag->Child->Attrib->Value);
      }
      else set_nodestring(Self, node, 0);

      // Check if the item has at least 1 child or if the 'custom' attribute has been used.

      node->Flags &= ~NODE_CHILDREN;
      if (tag->Child) {
         if (prepare_xml(Self, tag->Child, tag->Attrib->Name, 0) > 0) {
            node->Flags |= NODE_CHILDREN;
         }
      }

      if (!(node->Flags & NODE_CHILDREN)) node->Flags &= ~NODE_OPEN; // Ensure that the open flag is off if there are no children

      if (XMLATTRIB(tag, "custom")) node->Flags |= NODE_CHILDREN;

      if ((str = XMLATTRIB(tag, "datatype"))) {
         for (j=0; ((size_t)j < sizeof(node->Datatype)-1) and (str[j]); j++) {
            node->Datatype[j] = str[j];
         }
         node->Datatype[j] = 0;
      }
      else node->Datatype[0] = 0;

      count++;
   }

   return count;
}

//****************************************************************************

static XMLTag * get_item_xy(objView *Self, XMLTag **Array, LONG X, LONG Y)
{
   view_node *node;

   if ((Self->Style IS VIEW_TREE) or (Self->Style IS VIEW_GROUP_TREE) or (Self->Style IS VIEW_COLUMN_TREE)) {
      for (auto tag=*Array; tag; tag=tag->Next) {
         if (!(node = (view_node *)tag->Private)) continue;
         if (!(node->Flags & NODE_ITEM)) continue;

         if ((X >= Self->Layout->BoundX) and (X < Self->Layout->BoundX+Self->Layout->BoundWidth) and
             (Y >= node->Y) and (Y < node->Y + node->Height)) {
            return tag;
         }

         if ((node->Flags & NODE_CHILDREN) and (node->Flags & NODE_OPEN)) {
            XMLTag *child;
            if ((child = get_item_xy(Self, &tag->Child, X, Y))) {
               return child;
            }
         }
      }
   }
   else if (Self->Style IS VIEW_COLUMN) {
      for (LONG index=0; Array[index]; index++) {
         if (!(node = (view_node *)Array[index]->Private)) continue;
         if (!(node->Flags & NODE_ITEM)) continue;
         if ((X >= node->X) and (X < node->X + node->Width) and
             (Y >= node->Y) and (Y < node->Y + node->Height)) {
             return Array[index];
         }
      }
   }
   else for (LONG index=0; Array[index]; index++) {
      if (!(node = (view_node *)Array[index]->Private)) continue;
      if (!(node->Flags & NODE_ITEM)) continue;
      if ((X >= node->X) and (X < node->X + node->Width) and
          (Y >= node->Y) and (Y < node->Y + node->Height)) {
          return Array[index];
      }
   }
   return NULL;
}

/*****************************************************************************
** Column mode: Drawn as an icon (if available) and the default column text.
*/

static void draw_dragitem(objView *Self, objSurface *Surface, objBitmap *Bitmap)
{
   auto font = Self->Font;
   font->Bitmap = Bitmap;
   font->Align = 0;
   font->WrapEdge = Surface->Width - 3;

   gfxDrawRectangle(Bitmap, 0, 0, Surface->Width, Surface->Height, bmpGetColour(Bitmap, 255, 255, 255, 160), TRUE);
   gfxDrawRectangle(Bitmap, 0, 0, Bitmap->Width, Bitmap->Height, bmpGetColour(Bitmap, 80, 80, 180, 60), FALSE);

   LONG x = 0;
   LONG y = 0;
   LONG lineheight = Self->LineHeight + 4;
   for (LONG i=0; i < Self->DragItemCount; i++) {
      view_node *node;
      auto tag = Self->XML->Tags[Self->DragItems[i]];
      if (!(node = (view_node *)tag->Private)) continue;

      if ((i IS MAX_DRAGITEMS-1) and (Self->DragItemCount - i - 1 > 0)) {
         font->X   = x + 2;
         font->Y   = y + ((lineheight - font->Height)/2);

         font->Align = ALIGN_RIGHT;
         font->AlignWidth = Surface->Width - 6;
         font->Colour.Red   = 0;
         font->Colour.Green = 0;
         font->Colour.Blue  = 0;
         font->Colour.Alpha = 32;
         font->X++;
         font->Y++;
         char buffer[80];
         StrFormat(buffer, sizeof(buffer), "[ +%d ]", Self->DragItemCount - i - 1);
         SetString(font, FID_String, buffer);
         acDraw(font);

         font->Colour.Alpha = 255;
         font->X--;
         font->Y--;
         acDraw(font);
         font->Align = 0;
         LONG width;
         GetLong(font, FID_Width, &width);
         font->WrapEdge -= width + 3;
      }

      // Draw the icon on the left

      if (node->Icon) {
         objBitmap *iconbmp = node->Icon;
         gfxCopyArea(iconbmp, Bitmap, BAF_BLEND, 0, 0, iconbmp->Width, iconbmp->Height,
            2 + ((Self->IconWidth - iconbmp->Width)/2),
            y + ((lineheight - iconbmp->Height)/2));
      }

      // Draw the text alongside the icon

      auto str = get_nodestring(Self, node);
      if ((tag) and (str)) {
         font->X = x + Self->IconWidth + 2;
         font->Y = y + ((lineheight - font->Height)/2);

         font->Colour.Red   = 0;
         font->Colour.Green = 0;
         font->Colour.Blue  = 0;
         font->Colour.Alpha = 32;
         font->X++;
         font->Y++;
         SetString(font, FID_String, str);
         acDraw(font);

         font->Colour.Alpha = 255;
         font->X--;
         font->Y--;
         acDraw(font);
      }
      y += lineheight;
   }
}

//****************************************************************************

void drag_items(objView *Self)
{
   // Record the items that have been selected for the drag

   if (Self->DragItems) {
      FreeResource(Self->DragItems);
      Self->DragItems = NULL;
      Self->DragItemCount = 0;
   }

   if (!get_selected_tags(Self, &Self->DragItems, &Self->DragItemCount)) {
      // Create a draggable surface at the correct size

      LONG itemcount = Self->DragItemCount;
      if (itemcount > MAX_DRAGITEMS) itemcount = MAX_DRAGITEMS;

      LONG width  = 128;
      LONG height = (Self->LineHeight + 4) * itemcount;
      if (!Self->DragSurface) {
         objSurface *surface;
         ERROR error;
         if (!NewLockedObject(ID_SURFACE, NF_INTEGRAL, &surface, &Self->DragSurface)) {
            SetFields(surface,
               FID_Parent|TLONG,    0,
               FID_Width|TLONG,     width,
               FID_Height|TLONG,    height,
               FID_WindowType|TSTR, "NONE",
               FID_Flags|TLONG,     RNF_COMPOSITE|RNF_STICK_TO_FRONT,
               TAGEND);
            if (!acInit(surface)) {
               drwAddCallback(surface, (APTR)&draw_dragitem);
               error = ERR_Okay;
            }
            else error = ERR_Init;

            if (error) { acFree(surface); Self->DragSurface = 0; }

            ReleaseObject(surface);
         }
         else error = ERR_NewObject;

         if (error) return;
      }
      else acResizeID(Self->DragSurface, width, height, 0);

      char *datatype;
      if (Self->DragItemCount IS 1) {
         XMLTag *tag = Self->XML->Tags[Self->DragItems[0]];
         auto node = (view_node *)tag->Private;
         datatype = node->Datatype;
      }
      else datatype = NULL;

      gfxStartCursorDrag((Self->DragSourceID) ? Self->DragSourceID : Self->Head.UniqueID, 0, datatype, Self->DragSurface);
   }
}

//****************************************************************************

static ERROR get_selected_tags(objView *Self, LONG **Result, LONG *Count)
{
   if (Count) *Count = 0;

   // Count the total number of selected items

   LONG count = 0;
   for (LONG index=0; Self->XML->Tags[index]; index++) {
      auto node = (view_node *)Self->XML->Tags[index]->Private;
      if (node->Flags & NODE_SELECTED) count++;
   }

   if (count < 1) return ERR_NoData;

   ERROR error;
   LONG *array;
   if (!(error = AllocMemory(sizeof(LONG) * (count + 1), MEM_DATA|MEM_NO_CLEAR, &array, NULL))) {
      LONG i = 0;
      for (LONG index=0; Self->XML->Tags[index]; index++) {
         auto node = (view_node *)Self->XML->Tags[index]->Private;
         if (node->Flags & NODE_SELECTED) {
            array[i] = index;
            i++;
         }
      }
      array[i] = -1;

      *Result = array;
      if (Count) *Count = count;
      return ERR_Okay;
   }
   else return ERR_AllocMemory;
}

//****************************************************************************

static void get_col_value(objView *Self, XMLTag *Tag, view_col *col, STRING buffer, LONG BufferSize, XMLTag **Value)
{
   STRING str;

   if (Value) *Value = NULL;
   if (buffer) buffer[0] = 0;

   if (!StrMatch("Default", col->Name)) {
      if (buffer) {
         if ((Self->TextAttrib) and ((str = XMLATTRIB(Tag, Self->TextAttrib)))) {
            StrCopy(str, buffer, BufferSize);
         }
         else xmlGetContent(Self->XML, Tag->Index, buffer, BufferSize);
      }
      if (Value) *Value = Tag;
   }
   else {
      // Scan for the tag that matches that set against the column.  If it doesn't exist then we'll print nothing in this column.
      // Column data can either exist in a child tag first, or a tag attribute if no matching child tags are available.

      for (auto child=Tag->Child; child; child=child->Next) {
         if (!StrMatch(child->Attrib->Name, col->Name)) {
            if (buffer) {
               if ((Self->TextAttrib) and ((str = XMLATTRIB(child, Self->TextAttrib)))) {
                  StrCopy(str, buffer, BufferSize);
               }
               else xmlGetContent(Self->XML, child->Index, buffer, BufferSize);
            }
            if (Value) *Value = child;
            return;
         }
      }

      for (LONG i=0; i < Tag->TotalAttrib; i++) {
         if (!StrMatch(Tag->Attrib[i].Name, col->Name)) {
            if (buffer) StrCopy(Tag->Attrib[i].Value, buffer, BufferSize);
            if (Value) *Value = Tag;
            return;
         }
      }
   }
}

//****************************************************************************

static ERROR report_cellclick(objView *Self, LONG TagIndex, LONG Column, LONG Input, LONG X, LONG Y)
{
   if (Self->CellClick.Type) {
      parasol::Log log(__FUNCTION__);
      log.traceBranch("Tag: %d, Column: %d, XY: (%d,%d)", TagIndex, Column, X, Y);
      if (Self->CellClick.Type IS CALL_STDC) {
         auto routine = (void (*)(objView *, LONG, LONG, LONG, LONG, LONG))Self->CellClick.StdC.Routine;
         parasol::SwitchContext ctx(Self->CellClick.StdC.Context);
         routine(Self, TagIndex, Column, Input, X, Y);
      }
      else if (Self->CellClick.Type IS CALL_SCRIPT) {
         OBJECTPTR script;
         if ((script = Self->CellClick.Script.Script)) {
            const ScriptArg args[] = {
               { "View",     FD_OBJECTPTR, { .Address = Self } },
               { "Tag",      FD_LONG, { .Long = TagIndex } },
               { "Column",   FD_LONG, { .Long = Column } },
               { "Input",    FD_LONG, { .Long = Input } },
               { "X",        FD_LONG, { .Long = X } },
               { "Y",        FD_LONG, { .Long = Y } }
            };
            scCallback(script, Self->CellClick.Script.ProcedureID, args, ARRAYSIZE(args), NULL);
         }
      }

      return ERR_Okay;
   }
   else return ERR_NothingDone;
}

//****************************************************************************

static void report_selection(objView *Self, LONG Flags, LONG TagIndex)
{
   if (Self->SelectCallback.Type) {
      parasol::Log log(__FUNCTION__);
      log.traceBranch("Flags: $%.8x, Tag: %d", Flags, TagIndex);
      if (Self->SelectCallback.Type IS CALL_STDC) {
         auto routine = (void (*)(objView *, LONG, LONG))Self->SelectCallback.StdC.Routine;
         parasol::SwitchContext ctx(Self->SelectCallback.StdC.Context);
         routine(Self, Flags, TagIndex);
      }
      else if (Self->SelectCallback.Type IS CALL_SCRIPT) {
         OBJECTPTR script;
         if ((script = Self->SelectCallback.Script.Script)) {
            const ScriptArg args[] = {
               { "View",  FD_OBJECTPTR, { .Address = Self } },
               { "Flags", FD_LONG, { .Long = Flags } },
               { "Tag",   FD_LONG, { .Long = TagIndex } }
            };
            scCallback(script, Self->SelectCallback.Script.ProcedureID, args, ARRAYSIZE(args), NULL);
         }
      }
   }
}

//****************************************************************************

static void process_style(objView *Self, objXML *XML, XMLTag *tag)
{
   for (tag=tag->Child; tag; tag=tag->Next) {
      if (!StrMatch("defaults", tag->Attrib->Name)) {
         for (auto defaults=tag->Child; defaults; defaults=defaults->Next) {
            if (!StrMatch("values", defaults->Attrib->Name)) {
               for (LONG a=1; a < defaults->TotalAttrib; a++) {
                  char value[300];
                  StrCopy(defaults->Attrib[a].Value, value, sizeof(value));
                  StrEvaluate(value, sizeof(value), 0, 0);
                  SetFieldEval(Self, defaults->Attrib[a].Name, value);
               }
            }
         }
      }
      else if (!StrMatch("graphics", tag->Attrib->Name)) {
         CSTRING name = XMLATTRIB(tag, "name");
         STRING str;
         if (!StrMatch("groupheader", name)) {
            if (!xmlGetString(XML, tag->Child->Index, XMF_INCLUDE_SIBLINGS, &str)) {
               if (Self->GroupHeaderXML) FreeResource(Self->GroupHeaderXML);
               Self->GroupHeaderXML = str;
            }
         }
         else if (!StrMatch("groupselect", name)) {
            if (!xmlGetString(XML, tag->Child->Index, XMF_INCLUDE_SIBLINGS, &str)) {
               if (Self->GroupSelectXML) FreeResource(Self->GroupSelectXML);
               Self->GroupSelectXML = str;
            }
         }
         else if (!StrMatch("background", name)) {
            if (!xmlGetString(XML, tag->Child->Index, XMF_INCLUDE_SIBLINGS, &str)) {
               if (Self->BkgdXML) FreeResource(Self->BkgdXML);
               Self->BkgdXML = str;
            }
         }
      }
   }
}

//****************************************************************************

static BYTE open_branch_callback(objView *Self, XMLTag *Tag)
{
   parasol::Log log("open_branch");

   log.branch("Index: %d", Tag->Index);

   auto node = (view_node *)Tag->Private;
   if (!(node->Flags & NODE_OPEN)) {
      // Whenever a branch is opened, we call ExpandCallback to update the XML tag's children.

      auto i = Tag->Index;
      auto modstamp = Self->XML->Modified;

      if (Self->ExpandCallback.Type) {
          if (Self->ExpandCallback.Type IS CALL_STDC) {
            auto routine = (void (*)(objView *, LONG))Self->ExpandCallback.StdC.Routine;
            parasol::SwitchContext ctx(Self->ExpandCallback.StdC.Context);
            routine(Self, Tag->Index);
         }
         else if (Self->ExpandCallback.Type IS CALL_SCRIPT) {
            OBJECTPTR script;
            if ((script = Self->ExpandCallback.Script.Script)) {
               const ScriptArg args[] = {
                  { "View",     FD_OBJECTPTR, { .Address = Self } },
                  { "TagIndex", FD_LONG, { .Long = Tag->Index } }
               };
               scCallback(script, Self->ExpandCallback.Script.ProcedureID, args, ARRAYSIZE(args), NULL);
            }
         }
      }

      if (Self->XML->Modified != modstamp) {
         log.trace("A subscriber modified the XML tree structure.");

         Tag = Self->XML->Tags[i]; // Re-grab the tag pointer if the tree structure was modified
         node = (view_node *)Tag->Private;
         node->Flags |= NODE_OPEN;
         Self->Deselect = FALSE;
         acRefresh(Self);
         return TRUE;
      }
      else log.trace("No modifications were made to the view XML (%d == %d).", Self->XML->Modified, modstamp);
   }

   return FALSE;
}
