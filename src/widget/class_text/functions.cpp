//****************************************************************************

static void feedback_validate_input(objText *Self)
{
   parasol::Log log("validate_input");

   log.branch("");

   if (Self->ValidateInput.Type IS CALL_STDC) {
      auto routine = (void (*)(objText *))Self->ValidateInput.StdC.Routine;
      if (routine) {
         parasol::SwitchContext ctx(Self->ValidateInput.StdC.Context);
         routine(Self);
      }
   }
   else if (Self->ValidateInput.Type IS CALL_SCRIPT) {
      OBJECTPTR script;
      if ((script = Self->ValidateInput.Script.Script)) {
         const ScriptArg args[] = { { "Text", FD_OBJECTPTR, { .Address = Self } } };
         scCallback(script, Self->ValidateInput.Script.ProcedureID, args, ARRAYSIZE(args));
      }
   }
}

//****************************************************************************

static void feedback_activated(objText *Self)
{
   parasol::Log log(__FUNCTION__);

   log.branch("");

   if (Self->Activated.Type IS CALL_STDC) {
      auto routine = (void (*)(objText *))Self->Activated.StdC.Routine;
      if (routine) {
         parasol::SwitchContext ctx(Self->Activated.StdC.Context);
         routine(Self);
      }
   }
   else if (Self->Activated.Type IS CALL_SCRIPT) {
      OBJECTPTR script;
      if ((script = Self->Activated.Script.Script)) {
         const ScriptArg args[] = { { "Text", FD_OBJECTPTR, { .Address = Self } } };
         scCallback(script, Self->Activated.Script.ProcedureID, args, ARRAYSIZE(args));
      }
   }
}

//****************************************************************************

static void add_history(objText *Self, CSTRING History)
{
   // Increment all history indexes
   for (LONG i=0; i < Self->HistorySize; i++) {
      if (Self->History[i].Number > 0) Self->History[i].Number++;
      if (Self->History[i].Number > Self->HistorySize) Self->History[i].Number = 0;
   }

   // Find an empty buffer and add the history text to it
   for (LONG i=0; i < Self->HistorySize; i++) {
      if (Self->History[i].Number IS 0) {
         Self->History[i].Number = 1; // New entries have an index of 1
         StrCopy(History, Self->History[i].Buffer, sizeof(Self->History[0].Buffer));
         return;
      }
   }
}

//****************************************************************************

static ERROR add_line(objText *Self, CSTRING String, LONG Line, LONG Length, LONG Allocated)
{
   STRING str;
   LONG len, i, line, unicodelen;

   // If a line number was not given then we will insert the line at the end of the list (note that the first line starts at #1).

   if (Line < 0) line = Self->AmtLines;
   else line = Line;

   // Get the length of the text

   if ((String) AND (*String)) {
      if (Length >= 0) len = Length;
      else for (len=0; (String[len]) AND (String[len] != '\n') AND (String[len] != '\r'); len++);
   }
   else len = 0;

   // Stop the string from exceeding the acceptable character limit

   if (len >= Self->CharLimit) {
      for (unicodelen=0, i=0; (i < len) AND (unicodelen < Self->CharLimit); unicodelen++) {
         for (++i; (String[i] & 0xc0) IS 0x80; i++);
      }
      len = i;
   }

   // If the line array is at capacity, expand it

   if (Self->AmtLines >= Self->MaxLines) {
      TextLine *newlines;
      if (!AllocMemory(sizeof(TextLine) * (Self->MaxLines + 100), MEM_DATA, &newlines, NULL)) {
         Self->MaxLines += 100;
         if (Self->Array) {
            CopyMemory(Self->Array, newlines, sizeof(Self->Array[0]) * Self->AmtLines);
            FreeResource(Self->Array);
         }
         Self->Array = newlines;
      }
      else return ERR_AllocMemory;
   }

   // Expand the end of the array to allow for our new entry

   if (line < Self->AmtLines) {
      CopyMemory(Self->Array + line, Self->Array + line + 1, sizeof(Self->Array[0]) * (Self->AmtLines - line));
   }

   Self->AmtLines++;

   // Insert the new line into the array

   if (len > 0) {
      if (Allocated) {
         Self->Array[line].String      = (STRING)String;
         Self->Array[line].Length      = len;
         Self->Array[line].PixelLength = calc_width(Self, String, len);
      }
      else if (!AllocMemory(len + 1, MEM_STRING|MEM_NO_CLEAR, &str, NULL)) {
         for (i=0; (i < len) AND (String[i]); i++) str[i] = String[i];
         str[i] = 0;
         Self->Array[line].String      = str;
         Self->Array[line].Length      = len;
         Self->Array[line].PixelLength = calc_width(Self, str, len);
      }
      else {
         ClearMemory(Self->Array + line, sizeof(Self->Array[line]));
         return ERR_AllocMemory;
      }
   }
   else ClearMemory(Self->Array + line, sizeof(Self->Array[line]));

   if (!Self->NoUpdate) {
      calc_hscroll(Self);
      calc_vscroll(Self);

      draw_lines(Self, line, Self->AmtLines - line);
   }

   return ERR_Okay;
}

//****************************************************************************

static ERROR add_xml(objText *Self, XMLTag *XMLList, WORD Flags, LONG Line)
{
   LONG j, i;

   if (!XMLList) return ERR_Okay;
   if (Line < 0) Line = Self->AmtLines;

   // Count the amount of bytes in the XML statement's content

   LONG len = 0;
   XMLTag *tag = XMLList->Child;
   while (tag) {
      len += xml_content_len(tag);
      tag = tag->Next;
   }

   if (len > 0) {
      char str[len+1];

      // Copy the content into a string buffer

      len = 0;
      tag = XMLList->Child;
      while (tag) {
         xml_extract_content(tag, str, &len, Flags);
         tag = tag->Next;
      }

         // Replace all white-space with real spaces (code 0x20)

         for (j=0; j < len; j++) if (str[j] < 0x20) str[j] = ' ';

         // Shrink the string in areas where white-space is doubled-up

         for (j=0; j < len; j++) {
            if ((str[j] IS '.') AND (str[j+1] IS 0x20) AND (str[j+2] IS 0x20)) {
               j += 2;  // The end of sentences are allowed double-spaces
            }
            else if ((str[j] IS 0x20) AND (str[j+1] IS 0x20)) {
               for (i=j; str[i] IS 0x20; i++);
               StrCopy(str+i, str+j, COPY_ALL);
            }
         }

         // Get the length of the text

         for (len=0; str[len]; len++);

         // If the line array is at capacity, expand it

         if (Self->AmtLines >= Self->MaxLines) {
            TextLine *newlines;
            if (!AllocMemory(sizeof(TextLine) * (Self->MaxLines + 100), MEM_DATA, &newlines, NULL)) {
               Self->MaxLines += 100;
               if (Self->Array) {
                  CopyMemory(Self->Array, newlines, sizeof(Self->Array[0]) * Self->AmtLines);
                  FreeResource(Self->Array);
               }
               Self->Array = newlines;
            }
            else return ERR_AllocMemory;
         }

         // Expand the end of the array to allow for our new entry

         if (Line < Self->AmtLines) {
            CopyMemory(Self->Array + Line, Self->Array + Line + 1, sizeof(Self->Array[0]) * (Self->AmtLines - Line));
         }

         Self->AmtLines++;

         // Insert the new line into the array

         if (len > 0) {
            if (!AllocMemory(len + 1, MEM_STRING|MEM_NO_CLEAR, &Self->Array[Line].String, NULL)) {
               Self->Array[Line].String[CharCopy(str, Self->Array[Line].String, len)] = 0;
               Self->Array[Line].Length      = len;
               Self->Array[Line].PixelLength = calc_width(Self, str, len);
            }
            else {
               ClearMemory(Self->Array + Line, sizeof(Self->Array[Line]));
               return ERR_AllocMemory;
            }
         }
         else ClearMemory(Self->Array + Line, sizeof(Self->Array[Line]));

         if (!Self->NoUpdate) {
            calc_hscroll(Self);
            calc_vscroll(Self);
            redraw_line(Self, Line);
         }

      if (Flags & AXF_NEWLINE) {
         add_line(Self, "", -1, -1, FALSE);
      }
   }
   else add_line(Self, "", -1, -1, FALSE);

   return ERR_Okay;
}

//****************************************************************************

static void draw_text(objText *Self, objSurface *Surface, objBitmap *Bitmap)
{
   objFont *font, *currentfont;
   ClipRectangle clipsave;
   RGB8 basergb;
   STRING str;
   LONG i, x, sx, row, column, endrow, endcolumn, width;
   LONG selectrow, selectcolumn, textheight;
   LONG amtlines;
   WORD valign;

   if ((Self->Layout->Visible IS FALSE) OR (Self->Tag)) return;

   if (!(font = Self->Font)) return;

   // Frame testing

   if ((Self->Frame) AND (Surface->Frame != Self->Frame)) return;

   // In EDIT mode, there must always be at least 1 line so that we can print the cursor

   amtlines = Self->AmtLines;
   if ((amtlines < 1) AND (Self->Flags & TXF_EDIT)) amtlines = 1;

   valign = (font->LineSpacing - font->MaxHeight)>>1; // VAlign is used to keep strings vertically centered within each line

   // Set font dimensions

   font->X = Self->Layout->BoundX + (Self->Layout->Document ? 0 : Self->Layout->LeftMargin);
   font->Y = Self->Layout->BoundY + (Self->Layout->Document ? 0 : Self->Layout->TopMargin) + font->Leading;
   font->WrapCallback = NULL;
   font->X += Self->XPosition;
   font->Y += Self->YPosition + valign;
   font->Bitmap = Bitmap;

   if ((Self->Flags & TXF_WORDWRAP) OR (font->Flags & FTF_CHAR_CLIP)) {
      font->WrapEdge = Self->Layout->BoundX + Self->Layout->BoundWidth - (Self->Layout->Document ? 0 : Self->Layout->RightMargin);
   }
   else font->WrapEdge = 0;

   font->Align = Self->Layout->Align & (~(ALIGN_VERTICAL|ALIGN_BOTTOM|ALIGN_TOP)); // We'll use our own vertical alignment calculations
   font->AlignWidth  = Self->Layout->BoundWidth  - (Self->Layout->Document ? 0 : (Self->Layout->LeftMargin + Self->Layout->RightMargin));
   font->AlignHeight = Self->Layout->BoundHeight - (Self->Layout->Document ? 0 : (Self->Layout->TopMargin  + Self->Layout->BottomMargin));

   if (Self->Layout->Align & (ALIGN_VERTICAL|ALIGN_BOTTOM)) {
      // If in wordwrap mode, calculate the height of all the text lines so that we can get a correct alignment

      if ((Self->Flags & TXF_WORDWRAP) AND (Self->AmtLines > 0)) {
         LONG wrapheight;
         textheight = 0;
         for (row=0; row < Self->AmtLines; row++) {
            fntStringSize(font, Self->Array[row].String, -1, Self->Layout->BoundWidth  - (Self->Layout->Document ? 0 : (Self->Layout->LeftMargin + Self->Layout->RightMargin)), 0, &wrapheight);
            textheight += wrapheight * font->LineSpacing;
         }
      }
      else textheight = amtlines * font->LineSpacing;

      if (Self->Layout->Align & ALIGN_VERTICAL) {
         font->Y = Self->Layout->BoundY + (Self->Layout->Document ? 0 : Self->Layout->TopMargin) + ((Self->Layout->BoundHeight - (Self->Layout->Document ? 0 : Self->Layout->BottomMargin) - textheight)>>1) + font->Leading;
      }
      else if (Self->Layout->Align & ALIGN_BOTTOM) {
         font->Y = Self->Layout->BoundY + Self->Layout->BoundHeight - textheight - (Self->Layout->Document ? 0 : Self->Layout->BottomMargin) + font->Leading;
      }
   }

   // Set clipping area to match the text object

   clipsave = Bitmap->Clip;

   if (Self->Layout->BoundX > Bitmap->Clip.Left) Bitmap->Clip.Left = Self->Layout->BoundX;
   if (Self->Layout->BoundY > Bitmap->Clip.Top)  Bitmap->Clip.Top  = Self->Layout->BoundY;
   if (Self->Layout->BoundX + Self->Layout->BoundWidth < Bitmap->Clip.Right) Bitmap->Clip.Right = Self->Layout->BoundX + Self->Layout->BoundWidth;
   if (Self->Layout->BoundY + Self->Layout->BoundHeight < Bitmap->Clip.Bottom) Bitmap->Clip.Bottom = Self->Layout->BoundY + Self->Layout->BoundHeight;

   // Clear the background if requested.  Note that any use of alpha-blending will mean that fast scrolling
   // is disabled.

   if (Self->Background.Alpha > 0) {
      ULONG bkgd = PackPixelRGBA(Bitmap, &Self->Background);
      gfxDrawRectangle(Bitmap, Self->Layout->BoundX, Self->Layout->BoundY, Self->Layout->BoundWidth, Self->Layout->BoundHeight, bkgd, BAF_FILL|BAF_BLEND);
   }

   // If an area has been selected, highlight it

   selectrow    = -1;
   selectcolumn = -1;
   endrow       = -1;
   endcolumn    = -1;

   if (Self->Flags & TXF_AREA_SELECTED) {
      if ((Self->SelectRow != Self->CursorRow) OR (Self->SelectColumn != Self->CursorColumn)) {
         GetSelectedArea(Self, &selectrow, &selectcolumn, &endrow, &endcolumn);
      }
   }

   basergb = font->Colour;

   // Skip lines that are outside of the viewable area

   row = 0;
   if (!(Self->Flags & TXF_WORDWRAP)) {
      for (; (font->Y - valign + font->LineSpacing) <= Bitmap->Clip.Top; row++) {
         font->Y += font->LineSpacing;
      }
   }

   sx = Self->Layout->BoundX + (Self->Layout->Document ? 0 : Self->Layout->LeftMargin);

   for (; (row < amtlines) AND (font->Y - valign - font->Leading < Bitmap->Clip.Bottom); row++) {
      // Do style management if there are tags listed against this line

      currentfont = Self->Font;

      if (currentfont != font) {
         currentfont->X = font->X;
         currentfont->Y = font->Y;
         currentfont->Bitmap = font->Bitmap;
         currentfont->LineSpacing  = font->LineSpacing;
         currentfont->WrapEdge     = font->WrapEdge;
         currentfont->WrapCallback = font->WrapCallback;
      }

      currentfont->Colour.Red   = basergb.Red;
      currentfont->Colour.Green = basergb.Green;
      currentfont->Colour.Blue  = basergb.Blue;

      // Set the font string

      if (Self->Flags & TXF_SECRET) {
         char buffer[Self->Array[row].Length+1];
         for (i=0; i < Self->Array[row].Length; i++) buffer[i] = '*';
         buffer[i] = 0;
         SetString(currentfont, FID_String, buffer);
      }
      else if (Self->Flags & TXF_VARIABLE) {
         char buffer[Self->Array[row].Length+100];
         for (i=0; i < Self->Array[row].Length; i++) buffer[i] = Self->Array[row].String[i];
         buffer[i] = 0;
         StrEvaluate(buffer, Self->Array[row].Length+100, 0, 0);
         SetString(currentfont, FID_String, buffer);
      }
      else SetString(currentfont, FID_String, Self->Array[row].String);

      // Draw any highlighting on this line

      if (Self->Flags & TXF_SECRET) {
         // Highlighting is not allowed in secret mode
      }
      else if ((row >= selectrow) AND (row <= endrow)) {
         width = Self->Layout->BoundWidth;
         if (row IS selectrow) {
            // First row
            x = column_coord(Self, row, selectcolumn);
            if (Self->Array[row].Length > 0) {
               if (row IS endrow) {
                  if ((width = fntStringWidth(currentfont, Self->Array[row].String, endcolumn)) > 3) {
                     width = width - x + sx + Self->XPosition;
                  }
                  else width = 3;
               }
               else width = Self->Array[row].PixelLength - x + sx + Self->XPosition;
            }
            else width = 3;
         }
         else if (row < endrow) {
            // Middle row
            x = currentfont->X;
            if (Self->Array[row].Length > 0) width = Self->Array[row].PixelLength;
            else width = 3;
         }
         else {
            // End row
            x = currentfont->X;
            if ((Self->Array[row].Length > 0) AND (endcolumn > 0)) {
               x = sx + Self->XPosition;
               width = fntStringWidth(currentfont, Self->Array[row].String, endcolumn) - x + sx + Self->XPosition;
            }
            else width = 3;
         }

         gfxDrawRectangle(Bitmap, x, currentfont->Y - currentfont->Leading - valign, width, currentfont->LineSpacing,
            PackPixelRGBA(Bitmap, &Self->Highlight), BAF_FILL);
      }

      // Draw the cursor if the object is in edit mode

      if ((row IS Self->CursorRow) AND (Surface->Flags & RNF_HAS_FOCUS) AND ((Self->CursorFlash % CURSOR_RATE) < (CURSOR_RATE>>1)) AND
          (Self->Flags & TXF_EDIT) AND (!Self->NoCursor)) {

         x = Self->Layout->BoundX + (Self->Layout->Document ? 0 : Self->Layout->LeftMargin) + Self->XPosition;

         if (Self->Layout->Align & ALIGN_HORIZONTAL) {
            width = fntStringWidth(currentfont, currentfont->String, -1);
            x += (Self->Layout->BoundWidth - width)>>1;
         }

         if ((str = currentfont->String)) {
            i = UTF8Length(str);
            column = Self->CursorColumn;
            if (column >= i) column = i;

            if (column > 0) {
               x += fntStringWidth(currentfont, str, column);
            }
         }

         gfxDrawRectangle(Bitmap, x, currentfont->Y - currentfont->Leading - valign, Self->CursorWidth,
            currentfont->LineSpacing, PackPixelRGBA(Bitmap, &Self->CursorColour), BAF_FILL|BAF_BLEND);
      }

      // Draw the font now

      if ((Self->Array[row].Length < 1) OR (!Self->Array[row].String)) {
         currentfont->EndX = currentfont->X;
         currentfont->EndY = currentfont->Y;
         font->Y += currentfont->LineSpacing;
      }
      else if (Self->Flags & TXF_WORDWRAP) {
         //GetLong(currentfont, FID_LineCount, &i);
         //if ((currentfont->Y - valign + (currentfont->LineSpacing * i)) >= Bitmap->Clip.Top) {
            acDraw(currentfont);
         //}

         font->Y = currentfont->EndY + currentfont->LineSpacing;
      }
      else {
         acDraw(currentfont);
         font->Y += currentfont->LineSpacing;
      }
   }

   font->Colour.Red   = basergb.Red;
   font->Colour.Green = basergb.Green;
   font->Colour.Blue  = basergb.Blue;

   Bitmap->Clip = clipsave;
}

//****************************************************************************

static LONG xml_content_len(XMLTag *XMLTag)
{
   LONG len;

   if (!XMLTag->Attrib[0].Name) {
      for (len=0; XMLTag->Attrib[0].Value[len]; len++);
   }
   else if ((XMLTag = XMLTag->Child)) {
      len = 0;
      while (XMLTag) {
         len += xml_content_len(XMLTag);
         XMLTag = XMLTag->Next;
      }
   }
   else return 0;

   return len;
}

//****************************************************************************

static void xml_extract_content(XMLTag *XMLTag, char *Buffer, LONG *Index, WORD Flags)
{
   if (!XMLTag->Attrib[0].Name) {
      STRING content;
      LONG pos = *Index;
      if ((content = XMLTag->Attrib[0].Value)) {
         LONG i;
         for (i=0; content[i]; i++) {
            // Skip whitespace
            if (content[i] <= 0x20) while ((content[i+1]) AND (content[i+1] <= 0x20)) i++;
            Buffer[pos++] = content[i];
         }
      }
      Buffer[pos] = 0;
      *Index = pos;
   }
   else if ((XMLTag = XMLTag->Child)) {
      while (XMLTag) {
         xml_extract_content(XMLTag, Buffer, Index, Flags);
         XMLTag = XMLTag->Next;
      }
   }
}

//****************************************************************************

static ERROR calc_hscroll(objText *Self)
{
   struct scUpdateScroll scroll;
   LONG width;

   if (!Self->HScrollID) return ERR_Okay;
   if (Self->NoUpdate) return ERR_Okay;

   // If wordwrap is enabled then the horizontal scrollbar is pointless

   if (Self->Flags & TXF_WORDWRAP) return ERR_Okay;

   GET_TextWidth(Self, &width);
   scroll.ViewSize = -1;
   scroll.PageSize = width + (Self->Layout->Document ? 0 : (Self->Layout->LeftMargin + Self->Layout->RightMargin));
   scroll.Position = -Self->XPosition;
   scroll.Unit     = Self->Font->MaxHeight;
   return ActionMsg(MT_ScUpdateScroll, Self->HScrollID, &scroll);
}

//****************************************************************************

static ERROR calc_vscroll(objText *Self)
{
   struct scUpdateScroll scroll;
   LONG lines, row, pagewidth;

   if (!Self->VScrollID) return ERR_Okay;
   if (Self->NoUpdate) return ERR_Okay;

   if ((Self->Flags & TXF_WORDWRAP) AND (Self->AmtLines > 0) AND (Self->Layout->ParentSurface.Width > 0)) {
      pagewidth = Self->Layout->BoundWidth;

      lines = 0;
      pagewidth = pagewidth - (Self->Layout->Document ? 0 : (Self->Layout->LeftMargin + Self->Layout->RightMargin));
      for (row=0; row < Self->AmtLines; row++) {
         if (Self->Array[row].PixelLength > pagewidth) {
            lines += ((Self->Array[row].PixelLength + pagewidth - 1) / pagewidth);
         }
         else lines++;
      }
   }
   else lines = Self->AmtLines;

   scroll.ViewSize = -1;
   scroll.PageSize = (lines * Self->Font->LineSpacing) + Self->Layout->BoundY + (Self->Layout->Document ? 0 : (Self->Layout->TopMargin + Self->Layout->BottomMargin));
   scroll.Position = -Self->YPosition;
   scroll.Unit     = Self->Font->LineSpacing;
   return ActionMsg(MT_ScUpdateScroll, Self->VScrollID, &scroll);
}

//****************************************************************************

static LONG calc_width(objText *Self, CSTRING String, LONG Length)
{
   if (Self->Flags & TXF_SECRET) {
      WORD i;
      if (!String) return 0;
      if (!Length) for (Length=0; String[Length]; Length++);

      BYTE buffer[Length+1];

      for (i=0; i < Length; i++) buffer[i] = '*';
      buffer[i] = 0;

      return fntStringWidth(Self->Font, buffer, Length);
   }
   else return fntStringWidth(Self->Font, String, (Length <= 0) ? -1 : Length);
}

/*****************************************************************************
** This function returns the exact horizontal coordinate for a specific column.  The coordinate is absolute and
** relative to the text object's surface container.
*/

static LONG column_coord(objText *Self, LONG Row, LONG Column)
{
   STRING str;
   BYTE buffer[Self->Array[Row].Length+1];

   LONG alignx = 0;

   if (Self->Flags & TXF_SECRET) {
      str = buffer;
      WORD i;
      for (i=0; i < Self->Array[Row].Length; i++) buffer[i] = '*';
      buffer[i] = 0;
   }
   else str = Self->Array[Row].String;

   if (Self->Layout->Align & ALIGN_HORIZONTAL) {
      alignx = (Self->Layout->BoundWidth - fntStringWidth(Self->Font, str, -1)) / 2;
   }

   if (Column <= 0) return Self->Layout->BoundX + alignx + (Self->Layout->Document ? 0 : Self->Layout->LeftMargin) + Self->XPosition;
   if (Column >= Self->Array[Row].Length) return Self->Layout->BoundX + alignx + (Self->Layout->Document ? 0 : Self->Layout->LeftMargin) + Self->XPosition + Self->Array[Row].PixelLength;

   if (Row >= Self->AmtLines) Row = Self->AmtLines - 1;

   return Self->Layout->BoundX + alignx + (Self->Layout->Document ? 0 : Self->Layout->LeftMargin) + Self->XPosition + fntStringWidth(Self->Font, str, Column);
}

//****************************************************************************

static void DeleteSelectedArea(objText *Self)
{
   STRING str;
   LONG row, column, endrow, endcolumn, i, j;

   Self->Flags &= ~TXF_AREA_SELECTED;

   GetSelectedArea(Self, &row, &column, &endrow, &endcolumn);
   column = UTF8CharOffset(Self->Array[row].String, column);
   endcolumn = UTF8CharOffset(Self->Array[endrow].String, endcolumn);

   if (row IS endrow) {
      str = Self->Array[row].String;
      for (i=column; endcolumn < Self->Array[row].Length; i++) {
         str[i] = str[endcolumn++];
      }
      Self->Array[row].Length = i;
      Self->Array[row].String[i] = 0;
      Self->Array[row].PixelLength = calc_width(Self, str, i);
      move_cursor(Self, row, column);
      redraw_line(Self, row);
      calc_hscroll(Self);
   }
   else {
      i = column + (Self->Array[endrow].Length - endcolumn) + 1;
      if (AllocMemory(i, MEM_STRING|MEM_NO_CLEAR, &str, NULL) IS ERR_Okay) {

         if (Self->Array[row].String) for (i=0; i < column; i++) str[i] = Self->Array[row].String[i];
         else i = 0;
         if (Self->Array[endrow].String) for (j=endcolumn; j < Self->Array[endrow].Length; j++) str[i++] = Self->Array[endrow].String[j];
         str[i] = 0;

         if (Self->Array[row].String) FreeResource(Self->Array[row].String);
         Self->Array[row].String = str;
         Self->Array[row].Length = i;
         Self->Array[row].PixelLength = calc_width(Self, str, i);
         move_cursor(Self, row, column);

         // Delete following strings

         row++;
         endrow++;
         for (i=row; i < endrow; i++) {
            if (Self->Array[i].String) FreeResource(Self->Array[i].String);
         }

         for (i=row; endrow < Self->AmtLines; i++) {
            Self->Array[i].String      = Self->Array[endrow].String;
            Self->Array[i].PixelLength = Self->Array[endrow].PixelLength;
            Self->Array[i].Length      = Self->Array[endrow].Length;
            endrow++;
         }

         Self->AmtLines = i;

         draw_lines(Self, row-1, 30000);
         calc_hscroll(Self);
         calc_vscroll(Self);
      }
   }
}

//****************************************************************************

static void draw_lines(objText *Self, LONG Row, LONG Total)
{
   LONG lines;

   if (Self->NoUpdate) return;

   if (Total < 1) return;

   if ((Self->Flags & TXF_WORDWRAP) AND (Row < Self->AmtLines)) {
      if (Row IS Self->AmtLines-1) {
         // Draw only the last word-wrapped line for speed
         if (Self->Array[Row].PixelLength > 0) {
            lines = (Self->Array[Row].PixelLength + Self->Layout->BoundWidth - 1) / Self->Layout->BoundWidth;
            if (lines < 1) lines = 1;
         }
         else lines = Total;
      }
      else lines = 1000; // Draw everything past the row to be redrawn
   }
   else lines = Total;

   acDrawAreaID(Self->Layout->SurfaceID, 0, row_coord(Self, Row), 30000, lines * Self->Font->LineSpacing);
}

//****************************************************************************

static void redraw_line(objText *Self, LONG Line)
{
   if (Self->NoUpdate) return;
   if (Line < 0) return;

   LONG lines;
   if ((Self->Flags & TXF_WORDWRAP) AND (Line < Self->AmtLines)) {
      if (Self->Array[Line].PixelLength > 0) {
         lines = (Self->Array[Line].PixelLength + Self->Layout->BoundWidth - 1) / Self->Layout->BoundWidth;
         if (lines < 1) lines = 1;
      }
      else lines = 1;
   }
   else lines = 1;

   acDrawAreaID(Self->Layout->SurfaceID, 0, row_coord(Self, Line), 30000, Self->Font->LineSpacing * lines);
}

//****************************************************************************

static void GetSelectedArea(objText *Self, LONG *Row, LONG *Column, LONG *EndRow, LONG *EndColumn)
{
   if (Self->SelectRow < Self->CursorRow) {
      *Row       = Self->SelectRow;
      *EndRow    = Self->CursorRow;
      *Column    = Self->SelectColumn;
      *EndColumn = Self->CursorColumn;
   }
   else if (Self->SelectRow IS Self->CursorRow) {
      *Row       = Self->SelectRow;
      *EndRow    = Self->CursorRow;
      if (Self->SelectColumn < Self->CursorColumn) {
         *Column    = Self->SelectColumn;
         *EndColumn = Self->CursorColumn;
      }
      else {
         *Column    = Self->CursorColumn;
         *EndColumn = Self->SelectColumn;
      }
   }
   else {
      *Row       = Self->CursorRow;
      *EndRow    = Self->SelectRow;
      *Column    = Self->CursorColumn;
      *EndColumn = Self->SelectColumn;
   }
}

//****************************************************************************

static void key_event(objText *Self, evKey *Event, LONG Size)
{
   if (!(Event->Qualifiers & KQ_PRESSED)) return;

   parasol::Log log(__FUNCTION__);
   STRING str;
   LONG i, j, row, len, offset;

   log.trace("$%.8x, Value: %d", Event->Qualifiers, Event->Code);

   Self->CursorFlash = 0; // Reset the flashing cursor to make it visible

   if ((!(Self->Flags & TXF_NO_SYS_KEYS)) AND (Event->Qualifiers & KQ_CTRL)) {
      switch(Event->Code) {
         case K_C: // Copy
            acClipboard(Self, CLIPMODE_COPY);
            return;

         case K_X: // Cut
            if (!(Self->Flags & TXF_EDIT)) return;
            acClipboard(Self, CLIPMODE_CUT);
            return;

         case K_V: // Paste
            if (!(Self->Flags & TXF_EDIT)) return;
            acClipboard(Self, CLIPMODE_PASTE);
            return;

         case K_K: // Delete line
            if (!(Self->Flags & TXF_EDIT)) return;
            txtDeleteLine(Self, Self->CursorRow);
            return;

         case K_Z: // Undo
            if (!(Self->Flags & TXF_EDIT)) return;
            return;

         case K_Y: // Redo
            if (!(Self->Flags & TXF_EDIT)) return;
            return;
      }
   }

   if (!(Event->Qualifiers & KQ_NOT_PRINTABLE)) { // AND (!(Flags & KQ_INSTRUCTIONKEYS))
      // Printable character handling

      if (!(Self->Flags & TXF_EDIT)) {
         log.trace("Object does not have the EDIT flag set.");
         return;
      }

      if (Self->Flags & TXF_AREA_SELECTED) DeleteSelectedArea(Self);
      insert_char(Self, Event->Unicode, Self->CursorColumn);
      return;
   }

   if (!(Self->Flags & TXF_EDIT)) {
      struct acScroll scroll;
      // When not in edit mode, only the navigation keys are enabled
      switch (Event->Code) {
         case K_PAGE_DOWN:
            if (Self->LineLimit IS 1) break;

            scroll.XChange = 0;
            scroll.YChange = Self->Layout->BoundHeight;
            scroll.ZChange = 0;
            DelayMsg(AC_Scroll, Self->Layout->SurfaceID, &scroll);
            break;

         case K_PAGE_UP:
            if (Self->LineLimit IS 1) break;

            scroll.XChange = 0;
            scroll.YChange = -Self->Layout->BoundHeight;
            scroll.ZChange = 0;
            DelayMsg(AC_Scroll, Self->Layout->SurfaceID, &scroll);
            break;

         case K_LEFT:
            scroll.XChange = -Self->Font->MaxHeight;
            scroll.YChange = 0;
            scroll.ZChange = 0;
            DelayMsg(AC_Scroll, Self->Layout->SurfaceID, &scroll);
            break;

         case K_RIGHT:
            scroll.XChange = Self->Font->MaxHeight;
            scroll.YChange = 0;
            scroll.ZChange = 0;
            DelayMsg(AC_Scroll, Self->Layout->SurfaceID, &scroll);
            break;

         case K_DOWN:
            scroll.XChange = 0;
            scroll.YChange = Self->Font->MaxHeight;
            scroll.ZChange = 0;
            DelayMsg(AC_Scroll, Self->Layout->SurfaceID, &scroll);
            break;

         case K_UP:
            scroll.XChange = 0;
            scroll.YChange = -Self->Font->MaxHeight;
            scroll.ZChange = 0;
            DelayMsg(AC_Scroll, Self->Layout->SurfaceID, &scroll);
            break;
      }
      return;
   }

   switch(Event->Code) {
   case K_BACKSPACE:
      if (Self->Flags & TXF_AREA_SELECTED) DeleteSelectedArea(Self);
      else if (Self->CursorColumn > 0) {
         if (Self->CursorColumn >= Self->Array[Self->CursorRow].Length) {
            Self->CursorColumn = Self->Array[Self->CursorRow].Length - 1;
         }
         else Self->CursorColumn--;

         if ((str = Self->Array[Self->CursorRow].String)) {
            i = UTF8CharOffset(str, Self->CursorColumn);
            for (len=1; (str[i+len] & 0xc0) IS 0x80; len++);
            for (; str[i+len]; i++) str[i] = str[i+len];
            str[i] = 0;
            Self->Array[Self->CursorRow].Length--;
            Self->Array[Self->CursorRow].PixelLength = calc_width(Self, str, Self->Array[Self->CursorRow].Length);
            redraw_line(Self, Self->CursorRow);
            calc_hscroll(Self);
         }
      }
      else if (Self->CursorRow > 0) {
         // This routine is used if the current line will be shifted up into the line above it
         len = Self->Array[Self->CursorRow-1].Length + Self->Array[Self->CursorRow].Length;
         if (len > 0) {
            if (AllocMemory(len+1, MEM_STRING|MEM_NO_CLEAR, &str, NULL) IS ERR_Okay) {
               for (i=0; i < Self->Array[Self->CursorRow-1].Length; i++) str[i] = Self->Array[Self->CursorRow-1].String[i];
               for (j=0; j < Self->Array[Self->CursorRow].Length; j++) str[i++] = Self->Array[Self->CursorRow].String[j];
               str[i] = 0;

               Self->CursorRow--;
               Self->CursorColumn = UTF8Length(Self->Array[Self->CursorRow].String);
               if (Self->Array[Self->CursorRow].String) FreeResource(Self->Array[Self->CursorRow].String);
               Self->Array[Self->CursorRow].String = str;
               Self->Array[Self->CursorRow].Length = len;
               Self->Array[Self->CursorRow].PixelLength = calc_width(Self, str, len);
               redraw_line(Self, Self->CursorRow);
               txtDeleteLine(Self, Self->CursorRow+1);
               view_cursor(Self);
            }
         }
         else {
            txtDeleteLine(Self, Self->CursorRow);
            move_cursor(Self, Self->CursorRow-1, Self->Array[Self->CursorRow-1].Length);
         }
      }
      break;

   case K_CLEAR:
      if (Self->Flags & TXF_AREA_SELECTED) DeleteSelectedArea(Self);
      else {
         Self->CursorColumn = 0;
         txtDeleteLine(Self, Self->CursorRow);
      }
      break;

   case K_DELETE:
      if (Self->Flags & TXF_AREA_SELECTED) DeleteSelectedArea(Self);
      else if (Self->CursorColumn < Self->Array[Self->CursorRow].Length) {
         if ((str = Self->Array[Self->CursorRow].String)) {
            offset = UTF8CharOffset(Self->Array[Self->CursorRow].String, Self->CursorColumn);
            len = UTF8CharLength(Self->Array[Self->CursorRow].String+offset);
            for (i=offset; str[i+len]; i++) str[i] = str[i+len];
            str[i] = 0;
            Self->Array[Self->CursorRow].Length -= len;
            Self->Array[Self->CursorRow].PixelLength = calc_width(Self, str, Self->Array[Self->CursorRow].Length);
            redraw_line(Self, Self->CursorRow);
            calc_hscroll(Self);
         }
      }
      else if (Self->CursorRow < Self->AmtLines - 1) {
         // This code is used if the next line is going to be pulled up into the current line
         len = Self->Array[Self->CursorRow+1].Length + Self->Array[Self->CursorRow].Length;
         if (len > 0) {
            if (AllocMemory(len+1, MEM_STRING|MEM_NO_CLEAR, &str, NULL) IS ERR_Okay) {
               for (i=0; i < Self->Array[Self->CursorRow].Length; i++) str[i] = Self->Array[Self->CursorRow].String[i];
               for (j=0; j < Self->Array[Self->CursorRow+1].Length; j++) str[i++] = Self->Array[Self->CursorRow+1].String[j];
               str[i] = 0;

               if (Self->Array[Self->CursorRow].String) FreeResource(Self->Array[Self->CursorRow].String);
               Self->Array[Self->CursorRow].String = str;
               Self->Array[Self->CursorRow].Length = len;
               Self->Array[Self->CursorRow].PixelLength = calc_width(Self, str, len);
               redraw_line(Self, Self->CursorRow);
               txtDeleteLine(Self, Self->CursorRow+1);
            }
         }
         else txtDeleteLine(Self, Self->CursorRow);
      }
      break;

   case K_END:
      move_cursor(Self, Self->CursorRow, Self->Array[Self->CursorRow].Length);
      break;

   case K_ENTER:
   case K_NP_ENTER:
      Self->HistoryPos = 0; // Reset the history position

      if (Self->Flags & TXF_ENTER_TAB) {
         // Match the enter-key with an emulated tab-key press (this is useful for things like input boxes).

         evKey key = {
            .EventID    = EVID_IO_KEYBOARD_KEYPRESS,
            .Qualifiers = KQ_NOT_PRINTABLE|KQ_PRESSED,
            .Code       = K_TAB,
            .Unicode    = '\t'
         };
         BroadcastEvent(&key, sizeof(key));
      }

      if (Self->Activated.Type) {
         feedback_validate_input(Self);
         feedback_activated(Self);

         if (Self->TabFocusID) {
            acLostFocus(Self);
            acFocusID(Self->TabFocusID);
         }

         if (Self->Flags & TXF_AUTO_CLEAR) {
            if (Self->Flags & TXF_HISTORY) add_history(Self, Self->Array[0].String);
            SetString(Self, FID_String, "");
            Self->CursorColumn = 0;
         }
      }
      else {
         if ((Self->LineLimit) AND (Self->AmtLines >= Self->LineLimit)) break;

         if (Self->Flags & TXF_AREA_SELECTED) DeleteSelectedArea(Self);

         if (!Self->AmtLines) {
            Self->AmtLines = 1;
            Self->Array[0].String = NULL;
            Self->Array[0].Length = 0;
            Self->Array[0].PixelLength = 0;
         }

         row    = Self->CursorRow;
         offset = UTF8CharOffset(Self->Array[row].String, Self->CursorColumn);
         Self->CursorRow++;
         Self->CursorColumn = 0;
         add_line(Self, Self->Array[row].String + offset, row+1, Self->Array[row].Length - offset, FALSE);

         if (!offset) txtReplaceLine(Self, row, 0, 0);
         else txtReplaceLine(Self, row, Self->Array[row].String, offset);
         view_cursor(Self);
      }
      break;

   case K_HOME:
      move_cursor(Self, Self->CursorRow, 0);
      break;

   case K_INSERT:
      if (Self->Flags & TXF_OVERWRITE) Self->Flags &= ~TXF_OVERWRITE;
      else Self->Flags |= TXF_OVERWRITE;
      break;

   case K_LEFT:
      validate_cursorpos(Self, FALSE);
      if (Self->CursorColumn > 0) move_cursor(Self, Self->CursorRow, Self->CursorColumn-1);
      else if (Self->CursorRow > 0) move_cursor(Self, Self->CursorRow-1, UTF8Length(Self->Array[Self->CursorRow-1].String));
      break;

   case K_PAGE_DOWN:
      if (Self->LineLimit IS 1) break;
      move_cursor(Self, Self->CursorRow + (Self->Layout->ParentSurface.Height/(Self->Font->LineSpacing)), Self->CursorColumn);
      break;

   case K_PAGE_UP:
      if (Self->LineLimit IS 1) break;
      move_cursor(Self, Self->CursorRow - (Self->Layout->ParentSurface.Height/(Self->Font->LineSpacing)), Self->CursorColumn);
      break;

   case K_RIGHT:
      validate_cursorpos(Self, FALSE);
      if (Self->CursorColumn < UTF8Length(Self->Array[Self->CursorRow].String)) {
         move_cursor(Self, Self->CursorRow, Self->CursorColumn+1);
      }
      else if (Self->CursorRow < Self->AmtLines - 1) {
         move_cursor(Self, Self->CursorRow+1, 0);
      }
      break;

   case K_TAB:
      if (Self->TabFocusID) {
         acLostFocus(Self);
         acFocusID(Self->TabFocusID);
      }
      else {
         if (Self->LineLimit IS 1) break;
         else if ((Self->Flags & TXF_TAB_KEY) OR (Event->Qualifiers & KQ_SHIFT)) {
            if (Self->Flags & TXF_AREA_SELECTED) DeleteSelectedArea(Self);
            insert_char(Self, '\t', Self->CursorColumn);
         }
      }
      break;

   case K_DOWN:
   case K_UP:
      if (Self->Flags & TXF_HISTORY) {
         if (Event->Code IS K_UP) {
            // Return if we area already at the maximum historical position

            if (Self->HistoryPos >= Self->HistorySize) return;
            Self->HistoryPos++;
            for (i=0; i < Self->HistorySize; i++) {
               if (Self->History[i].Number IS Self->HistoryPos) {
                  SetString(Self, FID_String, Self->History[i].Buffer);
                  for (Self->CursorColumn=0; Self->History[i].Buffer[Self->CursorColumn]; Self->CursorColumn++);
                  Redraw(Self);
                  return;
               }
            }

            // If we couldn't find a string to match the higher history number, revert to what it was.

            Self->HistoryPos--;
         }
         else {
            if (Self->HistoryPos <= 0) {
               SetString(Self, FID_String, "");
               Self->CursorColumn = 0;
               return;
            }
            Self->HistoryPos--;
            for (i=0; i < Self->HistorySize; i++) {
               if (Self->History[i].Number IS Self->HistoryPos) {
                  SetString(Self, FID_String, Self->History[i].Buffer);
                  for (Self->CursorColumn=0; Self->History[i].Buffer[Self->CursorColumn]; Self->CursorColumn++);
                  Redraw(Self);
                  return;
               }
            }
         }
      }
      else if (((Event->Code IS K_UP) AND (Self->CursorRow > 0)) OR
               ((Event->Code IS K_DOWN) AND (Self->CursorRow < Self->AmtLines-1))) {
         WORD endcolumn, col, colchar;

         // Determine the current true position of the current cursor column, in UTF-8, with respect to tabs.  Then determine the cursor
         // character that we are going to be at when we end up at the row above us.

         if (((Self->CursorRow << 16) | Self->CursorColumn) IS Self->CursorSavePos) endcolumn = Self->CursorEndColumn;
         else {
            Self->CursorEndColumn = 0;
            endcolumn = Self->CursorColumn;
         }

         colchar = 0;
         col = 0;
         i = 0;
         while ((i < Self->Array[Self->CursorRow].Length) AND (colchar < endcolumn)) {
            if (Self->Array[Self->CursorRow].String[i] IS '\t') {
               col += ROUNDUP(col, Self->Font->TabSize);
            }
            else col++;

            colchar++;

            for (++i; (Self->Array[Self->CursorRow].String[i] & 0xc0) IS 0x80; i++);
         }

         // If an area is currently selected, turn off the selection and redraw the graphics area.

         if (Self->Flags & TXF_AREA_SELECTED) {
            Self->Flags &= ~TXF_AREA_SELECTED;
            Redraw(Self);
         }

         // Remove the current cursor first

         Remove_cursor(Self);

         if (Event->Code IS K_UP) Self->CursorRow--;
         else Self->CursorRow++;

         for (Self->CursorColumn=0, i=0; (col > 0) AND (i < Self->Array[Self->CursorRow].Length);) {
            if (Self->Array[Self->CursorRow].String[i] IS '\t') {
               col -= ROUNDUP(Self->CursorColumn, Self->Font->TabSize);
               Self->CursorColumn++;
            }
            else {
               col--;
               Self->CursorColumn++;
            }

            for (++i; (Self->Array[Self->CursorRow].String[i] & 0xc0) IS 0x80; i++);
         }

         if (Self->CursorColumn > Self->CursorEndColumn) Self->CursorEndColumn = Self->CursorColumn;

         Self->CursorSavePos = ((LONG)Self->CursorRow << 16) | Self->CursorColumn;

         // Make sure that the cursor is going to be viewable at its new position

         view_cursor(Self);
         redraw_cursor(Self, TRUE);
      }
      break;
   }
}

//****************************************************************************

static void move_cursor(objText *Self, LONG Row, LONG Column)
{
   // If an area is currently selected, turn off the selection and redraw the graphics area.

   if (Self->Flags & TXF_AREA_SELECTED) {
      Self->Flags &= ~TXF_AREA_SELECTED;
      Redraw(Self);
   }

   if (Row < 0) Row = 0;
   else if (Row >= Self->AmtLines) Row = Self->AmtLines - 1;

   if (Column < 0) Column = 0;

   // Remove the current cursor first

   Remove_cursor(Self);

   // Make sure that the cursor is going to be viewable at its new position

   Self->CursorRow = Row;
   Self->CursorColumn = Column;
   view_cursor(Self);

   // Redraw the cursor at its new position

   redraw_cursor(Self, TRUE);
}

//****************************************************************************
// Redraws the cursor area.

static void redraw_cursor(objText *Self, LONG Visible)
{
   if (Self->NoUpdate) return;
   if (!(Self->Flags & TXF_EDIT)) return;

   if (Visible IS FALSE) Self->NoCursor++;

   LONG x = column_coord(Self, Self->CursorRow, Self->CursorColumn);
   LONG y = row_coord(Self, Self->CursorRow);

   acDrawAreaID(Self->Layout->SurfaceID, x, y,
      Self->CursorWidth + 1,
      Self->Font->LineSpacing+1);

   if (Visible IS FALSE) Self->NoCursor--;
}

//****************************************************************************

static void insert_char(objText *Self, LONG Unicode, LONG Column)
{
   STRING str;
   LONG i, j, k, charlen, offset, unicodelen;
   char buffer[6];

   if ((!Self) OR (!Unicode)) return;

   // If FORCECAPS is used, convert lower case letters to upper case

   if (Self->Flags & TXF_FORCE_CAPS) {
      if ((Unicode >= 'a') AND (Unicode <= 'z')) Unicode = Unicode - 'a' + 'A';
   }

   // Convert the character into a UTF-8 sequence

   charlen = UTF8WriteValue(Unicode, buffer, 6);

   if ((!Self->Array[Self->CursorRow].String) OR (Self->Array[Self->CursorRow].Length < 1)) {
      if (Self->CharLimit < 1) return;

      if (Self->Array[Self->CursorRow].String) FreeResource(Self->Array[Self->CursorRow].String);

      if (!AllocMemory(charlen+1, MEM_STRING|MEM_NO_CLEAR, &Self->Array[Self->CursorRow].String, NULL)) {
         for (i=0; i < charlen; i++) Self->Array[Self->CursorRow].String[i] = buffer[i];
         Self->Array[Self->CursorRow].String[i]   = 0;
         Self->Array[Self->CursorRow].Length = charlen;
         Self->Array[Self->CursorRow].PixelLength = calc_width(Self, Self->Array[Self->CursorRow].String, 1);

         Self->CursorColumn = 1;

         if (Self->AmtLines <= Self->CursorRow) Self->AmtLines = Self->CursorRow + 1;

         redraw_line(Self, Self->CursorRow);
         calc_hscroll(Self);
      }
   }
   else {
      offset = UTF8CharOffset(Self->Array[Self->CursorRow].String, Column);

      if (offset > Self->Array[Self->CursorRow].Length) return;

      if (!AllocMemory(Self->Array[Self->CursorRow].Length + charlen + 1, MEM_STRING|MEM_NO_CLEAR, &str, NULL)) {
         j = 0;
         i = 0;
         while (i < offset) str[j++] = Self->Array[Self->CursorRow].String[i++]; // Copy existing characters
         for (k=0; k < charlen; k++) str[j++] = buffer[k]; // Insert new character

         // If overwrite mode is set, skip over the character bytes at the current cursor position.

         if ((Self->Flags & TXF_OVERWRITE) AND (i < Self->Array[Self->CursorRow].Length)) {
            charlen--;
            for (++i; (Self->Array[Self->CursorRow].String[i] & 0xc0) IS 0x80; i++) charlen--;
         }

         while (i < Self->Array[Self->CursorRow].Length) str[j++] = Self->Array[Self->CursorRow].String[i++]; // Copy remaining characters
         str[j] = 0;

         FreeResource(Self->Array[Self->CursorRow].String);
         Self->Array[Self->CursorRow].String = str;
         Self->Array[Self->CursorRow].Length += charlen;
         Self->CursorColumn++;

         // Get the UTF-8 length of this string so that we can enforce character limits

         for (unicodelen=0, i=0; i < Self->Array[Self->CursorRow].Length; unicodelen++) {
            for (++i; (str[i] & 0xc0) IS 0x80; i++);
         }

         if (unicodelen > Self->CharLimit) {
            // Delete the character that is at the end of this line to keep it within limits
            Self->Array[Self->CursorRow].Length -= UTF8PrevLength(str, Self->Array[Self->CursorRow].Length);
            str[Self->Array[Self->CursorRow].Length] = 0;
         }

         // Ensure that the cursor column does not exceed the length of the line

         if (Self->CursorColumn > unicodelen) Self->CursorColumn = unicodelen;

         Self->Array[Self->CursorRow].PixelLength = calc_width(Self, str, Self->Array[Self->CursorRow].Length);

         redraw_line(Self, Self->CursorRow);

         if (view_cursor(Self) IS FALSE) calc_hscroll(Self);
      }
   }
}

//****************************************************************************

static ERROR load_file(objText *Self, CSTRING Location)
{
   parasol::Log log(__FUNCTION__);

   log.branch("Loading file '%s'", Location);

   objFile *file;
   if (!CreateObject(ID_FILE, NF_INTEGRAL, &file,
         FID_Flags|TLONG, FL_READ,
         FID_Path|TSTR,   Location,
         TAGEND)) {

      LARGE size;
      if (file->Flags & FL_STREAM) {
         log.msg("File is streamed.");

         if (!flStartStream(file, Self->Head.UniqueID, FL_READ, 0)) {
            acClear(Self);
            SubscribeAction(file, AC_Write);
            Self->FileStream = file;
            file = NULL;
         }
         else {
            acFree(file);
            return ERR_Read;
         }
      }
      else if ((!GetLarge(file, FID_Size, &size)) AND (size > 0)) {
         STRING line;
         if (!AllocMemory(size+1, MEM_STRING|MEM_NO_CLEAR, &line, NULL)) {
            LONG result;
            if (!acRead(file, line, size, &result)) {
               line[result] = 0;

               Self->NoUpdate++;
               acClear(Self);
               acDataText(Self, line);
               Self->NoUpdate--;
            }
            FreeResource(line);
         }
         else {
            acFree(file);
            return ERR_AllocMemory;
         }
      }
      else acClear(Self);

      if (Self->Head.Flags & NF_INITIALISED) {
         Redraw(Self);
         calc_hscroll(Self);
         calc_vscroll(Self);
      }

      if (file) acFree(file);
      return ERR_Okay;
   }
   else return ERR_OpenFile;
}

//****************************************************************************

static void Redraw(objText *Self)
{
   if (Self->NoUpdate) return;
   acDrawAreaID(Self->Layout->SurfaceID, Self->Layout->BoundX, Self->Layout->BoundY, Self->Layout->BoundWidth, Self->Layout->BoundHeight);
}

//****************************************************************************

static ERROR replace_line(objText *Self, CSTRING String, LONG Line, LONG ByteLength)
{
   STRING str;
   LONG i, unicodelen;

   if ((Line < 0) OR (Line >= Self->AmtLines)) return ERR_Args;

   // Calculate the length of the text if necessary

   LONG len = 0;
   if ((String) AND (*String)) {
      if (ByteLength >= 0) len = ByteLength;
      else for (len=0; (String[len]) AND (String[len] != '\n') AND (String[len] != '\r'); len++);
   }

   // Stop the string from exceeding the acceptable character limit

   if (len >= Self->CharLimit) {
      for (unicodelen=0, i=0; (i < len) AND (unicodelen < Self->CharLimit); unicodelen++) {
         for (++i; (String[i] & 0xc0) IS 0x80; i++);
      }
      len = i;
   }

   if (len < 1) {
      // If the length is zero, clear the line
      if (Self->Array[Line].String) FreeResource(Self->Array[Line].String);
      Self->Array[Line].String      = NULL;
      Self->Array[Line].Length      = 0;
      Self->Array[Line].PixelLength = 0;
   }
   else if (len <= Self->Array[Line].Length) {
      // If the new string is smaller than the available space, copy the new string straight over the old one.

      for (i=0; (i < len) AND (String[i]); i++) {
         Self->Array[Line].String[i] = String[i];
      }
      Self->Array[Line].String[i]   = 0;
      Self->Array[Line].Length      = i;
      Self->Array[Line].PixelLength = calc_width(Self, String, i);
   }
   else if (!AllocMemory(len+1, MEM_STRING|MEM_NO_CLEAR, &str, NULL)) {
      if (Self->Array[Line].String) FreeResource(Self->Array[Line].String);
      for (i=0; (i < len) AND (String[i]); i++) str[i] = String[i];
      str[i] = 0;
      Self->Array[Line].String = str;
      Self->Array[Line].Length = i;
      Self->Array[Line].PixelLength = calc_width(Self, String, i);
   }
   else return ERR_AllocMemory;

   if (!Self->NoUpdate) {
      calc_hscroll(Self);
      redraw_line(Self, Line);
   }

   return ERR_Okay;
}

//****************************************************************************
// Returns the coordinate of the row - note that this is not the coordinate of the font on that row.

static LONG row_coord(objText *Self, LONG Row)
{
   LONG y, height, line, pagewidth, i;

   if (Self->Flags & TXF_WORDWRAP) {
      line = 0;
      pagewidth = Self->Layout->BoundWidth - (Self->Layout->Document ? 0 : (Self->Layout->LeftMargin + Self->Layout->RightMargin));
      for (i=0; i < Row; i++) {
         if (Self->Array[i].PixelLength > pagewidth) {
            line += ((Self->Array[i].PixelLength + pagewidth - 1) / pagewidth);
         }
         else line++;
      }
   }
   else line = Row;

   if (Self->Layout->Align & ALIGN_VERTICAL) {
      GET_TextHeight(Self, &height);

      y = Self->Layout->BoundY + Self->Layout->TopMargin + ((Self->Layout->BoundHeight - (Self->Layout->Document ? 0 : (Self->Layout->TopMargin + Self->Layout->BottomMargin)) - height)>>1);
   }
   else if (Self->Layout->Align & ALIGN_BOTTOM) {
      GET_TextHeight(Self, &height);
      y = Self->Layout->BoundY + Self->Layout->BoundHeight - height - (Self->Layout->Document ? 0 : Self->Layout->BottomMargin);
   }
   else y = Self->Layout->BoundY + Self->Layout->TopMargin;

   y += (line * Self->Font->LineSpacing) + Self->YPosition;

   return y;
}

//****************************************************************************

static void stretch_text(objText *Self)
{
   parasol::Log log(__FUNCTION__);
   Variable targetwidth, targetheight;
   LONG textwidth, textheight;

   if (!(Self->Font->Flags & FTF_SCALABLE)) {
      log.msg("Cannot stretch non-scalable text.");
      return;
   }

   set_point(Self, 10.0); // Reset the point size so that resizing is consistent.

   targetwidth.Type = FD_DOUBLE;
   targetheight.Type = FD_DOUBLE;
   GetField(Self->Layout, FID_Width|TVAR, &targetwidth);
   GetField(Self->Layout, FID_Height|TVAR, &targetheight);

   // Note: The -0.5 is to prevent overrun, because scaling by point size is only going to be 98% accurate at best on the horizontal.

   // Shrink by width

   if (!GET_TextWidth(Self, &textwidth)) {
      if (textwidth > targetwidth.Double) {
         if (GET_TextHeight(Self, &textheight)) return;
         // Use the smaller of the two point sizes.
         DOUBLE hpoint = Self->Font->Point * ((DOUBLE)targetwidth.Double / (DOUBLE)textwidth);
         if (textheight > targetheight.Double) {
            DOUBLE vpoint = Self->Font->Point * ((DOUBLE)targetheight.Double / (DOUBLE)textheight);
            if (hpoint < vpoint) set_point(Self, hpoint-0.5);
            else set_point(Self, vpoint-0.5);
         }
         else set_point(Self, hpoint-0.5);
         return;
      }
   }

   // Shrink by height.

   if (!GET_TextHeight(Self, &textheight)) {
      if (textheight > targetheight.Double) {
         set_point(Self, (Self->Font->Point * ((DOUBLE)targetheight.Double / (DOUBLE)textheight))-0.5);
         return;
      }
   }

   // Enlarge by width

   if (textwidth < targetwidth.Double) {
      DOUBLE hpoint = Self->Font->Point * ((DOUBLE)targetwidth.Double / (DOUBLE)textwidth);
      if (textheight < targetheight.Double) {
         // Use the smaller of the two point sizes.
         DOUBLE vpoint = Self->Font->Point * ((DOUBLE)targetheight.Double / (DOUBLE)textheight);
         if (hpoint < vpoint) set_point(Self, hpoint-0.5);
         else set_point(Self, vpoint-0.5);
      }
      else set_point(Self, hpoint-0.5);
      return;
   }

   if (textheight < targetheight.Double) {
      DOUBLE vpoint = Self->Font->Point * ((DOUBLE)targetheight.Double / (DOUBLE)textheight);
      set_point(Self, vpoint-0.5);
   }
}

//****************************************************************************
// If the cursor is out of the current line's boundaries, this function will move it to a safe position.

static void validate_cursorpos(objText *Self, LONG Redraw)
{
   LONG column;

   if ((!Self->Array[Self->CursorRow].String) OR (Self->Array[Self->CursorRow].Length < 1)) {
      column = 0;
   }
   else {
      column = UTF8Length(Self->Array[Self->CursorRow].String);
      if (Self->CursorColumn < column) column = Self->CursorColumn;
   }

   if (column != Self->CursorColumn) {
      if (Redraw IS TRUE) move_cursor(Self, Self->CursorRow, column);
      else Self->CursorColumn = column;
   }
}

//****************************************************************************
// Returns TRUE if the cursor was out of sight and needed to be scrolled into view.
//
// Note: If the cursor is not giving enough space at the bottom or right areas of the text view, simply increase the
// bottom and right margin values so that the cursor appears before it reaches the edge.

static LONG view_cursor(objText *Self)
{
   LONG ycoord, height, xcoord, width, scrolly, scrollx, xpos, ypos;
   BYTE scroll;

   //log.function("[%d] Row: %d, Column: %d", Self->Head.UniqueID, Self->CursorRow, Self->CursorColumn);

   if (!(Self->Flags & (TXF_EDIT|TXF_SINGLE_SELECT|TXF_MULTI_SELECT|TXF_AREA_SELECTED))) return FALSE;

   if ((Self->Layout->ParentSurface.Height < 1) OR (Self->Layout->ParentSurface.Width < 1)) return FALSE;

   scroll = FALSE;
   xpos = Self->XPosition;
   ypos = Self->YPosition;

   // Vertical positioning for the cursor

   ycoord  = row_coord(Self, Self->CursorRow);
   scrolly = 0;

   if (ycoord < Self->Layout->BoundY) {
      if (!Self->CursorRow) scrolly = -ypos;
      else scrolly = -ycoord + Self->Layout->BoundY;
   }
   else {
      height = Self->Layout->ParentSurface.Height;
      if (!Self->Layout->Document) {
         if (height > Self->Layout->BottomMargin) height -= Self->Layout->BottomMargin; // This compensates for any obscuring scrollbar and keeps the cursor inside the view
      }
      if ((ycoord + Self->Font->LineSpacing) > height) {
         scrolly = -((ycoord + Self->Font->LineSpacing) - height);
      }
   }

   // Horizontal positioning for the cursor

   xcoord  = column_coord(Self, Self->CursorRow, Self->CursorColumn);
   scrollx = 0;

   if (xcoord < Self->Layout->BoundX) {
      if (!Self->CursorColumn) scrollx = -xpos; // Scroll to position zero
      else scrollx = -xcoord + Self->Layout->BoundX;
   }
   else {
      width = Self->Layout->ParentSurface.Width;
      if (!Self->Layout->Document) {
         if (width > Self->Layout->RightMargin) width -= Self->Layout->RightMargin; // This compensates for any obscuring scrollbar and keeps the cursor inside the view
      }
      if (xcoord > width) scrollx = -(xcoord - width);
   }

   // Do the scroll action

   if ((scrollx) OR (scrolly)) {
      ActionTags(AC_ScrollToPoint, Self, (DOUBLE)((-xpos) - scrollx), (DOUBLE)((-ypos) - scrolly), 0.0, STP_X|STP_Y);

      if (!Self->NoUpdate) {
         if (scrollx) calc_hscroll(Self);
         if (scrolly) calc_vscroll(Self);
      }
      scroll = TRUE;
   }

   return scroll;
}

//****************************************************************************
// Returns TRUE if the cursor was out of sight and needed to be scrolled into view.
//
// Similar to view_cursor() but includes the selected area.  Intended for use by text highlighting functions, e.g. for
// finding text.

static LONG view_selection(objText *Self)
{
   LONG ycoord, height, xcoord, width, scrolly, scrollx, selectx, selecty, xpos, ypos;
   BYTE scroll;

   if ((Self->Layout->ParentSurface.Height < 1) OR (Self->Layout->ParentSurface.Width < 1)) return FALSE;

   scroll = FALSE;
   xpos = Self->XPosition;
   ypos = Self->YPosition;

   // Vertical positioning for the start of the select area

   ycoord  = row_coord(Self, Self->SelectRow);
   selecty = 0;

   if (ycoord < Self->Layout->BoundY) {
      if (!Self->SelectRow) selecty = -ypos;
      else selecty = -ycoord + Self->Layout->BoundY;
   }
   else {
      height = Self->Layout->ParentSurface.Height;
      if (!Self->Layout->Document) {
         if (height > Self->Layout->BottomMargin) height -= Self->Layout->BottomMargin; // This compensates for any obscuring scrollbar and keeps the cursor inside the view
      }
      if ((ycoord + Self->Font->LineSpacing) > height) {
         selecty = -((ycoord + Self->Font->LineSpacing) - height);
      }
   }

   // Horizontal positioning for the start of the select area

   xcoord  = column_coord(Self, Self->SelectRow, Self->SelectColumn);
   selectx = 0;

   if (xcoord < Self->Layout->BoundX) {
      if (!Self->SelectColumn) selectx = -xpos; // Scroll to position zero
      else selectx = -xcoord + Self->Layout->BoundX;
   }
   else {
      width = Self->Layout->ParentSurface.Width;
      if (!Self->Layout->Document) {
         if (width > Self->Layout->RightMargin) width -= Self->Layout->RightMargin; // This compensates for any obscuring scrollbar and keeps the cursor inside the view
      }
      if (xcoord > width) selectx = -(xcoord - width);
   }

   // Vertical positioning for the cursor

   ycoord  = row_coord(Self, Self->CursorRow);
   scrolly = 0;

   if (ycoord < Self->Layout->BoundY) {
      if (!Self->CursorRow) scrolly = -ypos;
      else scrolly = -ycoord + Self->Layout->BoundY;
   }
   else {
      height = Self->Layout->ParentSurface.Height;
      if (Self->Layout->Document) {
         if (height > Self->Layout->BottomMargin) height -= Self->Layout->BottomMargin; // This compensates for any obscuring scrollbar and keeps the cursor inside the view
      }
      if ((ycoord + Self->Font->LineSpacing) > height) {
         scrolly = -((ycoord + Self->Font->LineSpacing) - height);
      }
   }

   // Horizontal positioning for the cursor

   xcoord  = column_coord(Self, Self->CursorRow, Self->CursorColumn);
   scrollx = 0;

   if (xcoord < Self->Layout->BoundX) {
      if (!Self->CursorColumn) scrollx = -xpos; // Scroll to position zero
      else scrollx = -xcoord + Self->Layout->BoundX;
   }
   else {
      width = Self->Layout->ParentSurface.Width;
      if (!Self->Layout->Document) {
         if (width > Self->Layout->RightMargin) width -= Self->Layout->RightMargin; // This compensates for any obscuring scrollbar and keeps the cursor inside the view
      }
      if (xcoord > width) scrollx = -(xcoord - width);
   }

   // Do the scroll action

   if ((selectx) AND (!scrollx)) scrollx = selectx;
   if ((selecty) AND (!scrolly)) scrolly = selecty;

   if ((scrollx) OR (scrolly)) {
      acScrollToPoint(Self, (DOUBLE)((-xpos) - scrollx), (DOUBLE)((-ypos) - scrolly), 0.0, STP_X|STP_Y);

      if (!Self->NoUpdate) {
         if (scrollx) calc_hscroll(Self);
         if (scrolly) calc_vscroll(Self);
      }
      scroll = TRUE;
   }

   return scroll;
}
