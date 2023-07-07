//********************************************************************************************************************
// This function lays out the document so that it is ready to be drawn.  It calculates the position, pixel length and
// height of each line and rearranges any objects that are present in the document.

static void layout_doc(extDocument *Self)
{
   pf::Log log(__FUNCTION__);
   objFont *font;
   LONG pagewidth, hscroll_offset;
   bool vertical_repass;

   if (Self->UpdateLayout IS false) return;

   for (auto &obj : Self->LayoutResources) FreeResource(obj);
   Self->LayoutResources.clear();

   if (Self->Stream.empty()) return;

   // Initial height is 1, not the surface height because we want to accurately report the final height of the page.

   LONG pageheight = 1;

   DLAYOUT("Area: %dx%d,%dx%d Visible: %d ----------", Self->AreaX, Self->AreaY, Self->AreaWidth, Self->AreaHeight, Self->VScrollVisible);

   Self->BreakLoop = MAXLOOP;

restart:
   Self->BreakLoop--;

   hscroll_offset = 0;

   if (Self->PageWidth <= 0) {
      // If no preferred page width is set, maximise the page width to the available viewing area
      pagewidth = Self->AreaWidth - hscroll_offset;
   }
   else {
      if (!Self->RelPageWidth) { // Page width is fixed
         pagewidth = Self->PageWidth;
      }
      else { // Page width is relative
         pagewidth = (Self->PageWidth * (Self->AreaWidth - hscroll_offset)) / 100;
      }
   }

   if (pagewidth < Self->MinPageWidth) pagewidth = Self->MinPageWidth;

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

   layout_section(Self, 0, &font, 0, 0, &pagewidth, &pageheight, Self->LeftMargin, Self->TopMargin, Self->RightMargin,
      Self->BottomMargin, &vertical_repass);

   DLAYOUT("Section layout complete.");

   // If the resulting page width has increased beyond the available area, increase the MinPageWidth value to reduce
   // the number of passes required for the next time we do a layout.


   if ((pagewidth > Self->AreaWidth) and (Self->MinPageWidth < pagewidth)) Self->MinPageWidth = pagewidth;

   Self->PageHeight = pageheight;
//   if (Self->PageHeight < Self->AreaHeight) Self->PageHeight = Self->AreaHeight;
   Self->CalcWidth = pagewidth;

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

                     if ((esclist->Type IS LT_CUSTOM) or (esclist->Type IS LT_ORDERED)) {
                        if (!escpara->Value.empty()) {
                           font->X = fx - escpara->ItemIndent;
                           font->Y = segment.Y + font->Leading + (segment.BaseLine - font->Ascent);
                           font->AlignWidth = segment.AlignWidth;
                           font->setString(escpara->Value);
                           font->draw();
                        }
                     }
                     else if (esclist->Type IS LT_BULLET) {
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