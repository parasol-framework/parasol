
//********************************************************************************************************************
// Rastered fonts are drawn as a rectangular block referencing a VectorImage texture that contains the rendered font.

static void raster_text_to_bitmap(extVectorText *Vector)
{
   pf::Log log(__FUNCTION__);

   if (!Vector->txBitmapFont) {
      reset_font(Vector);
      if (!Vector->txBitmapFont) return;
   }

   auto &lines = Vector->txLines;
   if (!lines.size()) return;

   LONG dx = 0, dy = Vector->txBitmapFont->Leading;
   LONG longest_line_width = 0;

   if ((!Vector->txInlineSize) and (!(Vector->txCursor.vector))) { // Fast calculation if no wrapping or active cursor
      for (auto &line : Vector->txLines) {
         line.chars.clear();
         LONG line_width = fntStringWidth(Vector->txBitmapFont, line.c_str(), -1);
         if (line_width > longest_line_width) longest_line_width = line_width;
         dy += Vector->txBitmapFont->LineSpacing;
      }
   }
   else {
      agg::path_storage cursor_path;
      LONG prev_glyph = 0;

      for (auto &line : Vector->txLines) {
         line.chars.clear();
         auto wrap_state = WS_NO_WORD;
         for (auto str=line.c_str(); *str; ) {
            LONG char_len;
            auto unicode = UTF8ReadValue(str, &char_len);

            LONG kerning;
            auto char_width = fntCharWidth(Vector->txBitmapFont, unicode, prev_glyph, &kerning);

            if (unicode <= 0x20) { wrap_state = WS_NO_WORD; prev_glyph = 0; }
            else if (wrap_state IS WS_NEW_WORD) wrap_state = WS_IN_WORD;
            else if (wrap_state IS WS_NO_WORD)  wrap_state = WS_NEW_WORD;

            if (!unicode) break;

            // Determine if this is a new word that would wrap if drawn.
            // TODO: Wrapping information should be cached for speeding up subsequent redraws.

            if ((wrap_state IS WS_NEW_WORD) and (Vector->txInlineSize)) {
               LONG word_length = 0;
               for (LONG c=0; (str[c]) and (str[c] > 0x20); ) {
                  for (++c; ((str[c] & 0xc0) IS 0x80); c++);
                  word_length++;
               }

               LONG word_width = fntStringWidth(Vector->txBitmapFont, str, word_length);

               if (dx + word_width >= Vector->txInlineSize) {
                  if (dx > longest_line_width) longest_line_width = dx;
                  dx = 0;
                  dy += Vector->txBitmapFont->LineSpacing;
               }
            }

            str += char_len;

            if (Vector->txCursor.vector) {
               line.chars.emplace_back(dx, dx, dy + 1, dy - ((DOUBLE)Vector->txBitmapFont->Height * 1.2));
               if (!*str) { // Last character reached, add a final cursor entry past the character position.
                  line.chars.emplace_back(dx + kerning + char_width, dy, dx + kerning + char_width, dy - ((DOUBLE)Vector->txBitmapFont->Height * 1.2));
               }
            }

            dx += kerning + char_width;
            prev_glyph = unicode;
         }

         if (dx > longest_line_width) longest_line_width = dx;
         dx = 0;
         dy += Vector->txBitmapFont->LineSpacing;
      }
   }

   if (dy < Vector->txBitmapFont->LineSpacing) dy = Vector->txBitmapFont->LineSpacing; // Enforce min. height

   // Standard rectangle to host the text image.

   Vector->BasePath.move_to(0, 0);
   Vector->BasePath.line_to(longest_line_width, 0);
   Vector->BasePath.line_to(longest_line_width, dy);
   Vector->BasePath.line_to(0, dy);
   Vector->BasePath.close_polygon();

   Vector->txWidth = longest_line_width;

   if (Vector->txCursor.vector) {
      Vector->txCursor.reset_vector(Vector);
   }

   if (!Vector->txBitmapImage) {
      if (!(Vector->txAlphaBitmap = objBitmap::create::integral(
            fl::Width(longest_line_width),
            fl::Height(dy),
            fl::BitsPerPixel(32),
            fl::Flags(BMF::ALPHA_CHANNEL)))) return;

      if (!(Vector->txBitmapImage = objVectorImage::create::integral(
            fl::Bitmap(Vector->txAlphaBitmap),
            fl::SpreadMethod(VSPREAD::CLIP),
            fl::Units(VUNIT::BOUNDING_BOX),
            fl::AspectRatio(ARF::X_MIN|ARF::Y_MIN)))) return;
   }
   else acResize(Vector->txAlphaBitmap, longest_line_width, dy, 0);

   Vector->Fill[0].Image = Vector->txBitmapImage;
   Vector->DisableFillColour = true;

   Vector->txBitmapFont->Bitmap = Vector->txAlphaBitmap;

   gfxDrawRectangle(Vector->txAlphaBitmap, 0, 0, Vector->txAlphaBitmap->Width, Vector->txAlphaBitmap->Height, 0x00000000, BAF::FILL);

   if (Vector->txInlineSize) Vector->txBitmapFont->WrapEdge = Vector->txInlineSize;

   LONG y = Vector->txBitmapFont->Leading;
   for (auto &line : Vector->txLines) {
      auto str = line.c_str();
      if (!str[0]) y += Vector->txBitmapFont->LineSpacing;
      else {
         Vector->txBitmapFont->setString(str);
         Vector->txBitmapFont->X = 0;
         Vector->txBitmapFont->Y = y;
         Vector->txBitmapFont->Colour.Red   = F2T(Vector->Fill[0].Colour.Red * 255.0);
         Vector->txBitmapFont->Colour.Green = F2T(Vector->Fill[0].Colour.Green * 255.0);
         Vector->txBitmapFont->Colour.Blue  = F2T(Vector->Fill[0].Colour.Blue * 255.0);
         Vector->txBitmapFont->Colour.Alpha = F2T(Vector->Fill[0].Colour.Alpha * 255.0);
         acDraw(Vector->txBitmapFont);

         if (Vector->txInlineSize) y = Vector->txBitmapFont->EndY + Vector->txBitmapFont->LineSpacing;
         else y += Vector->txBitmapFont->LineSpacing;
      }
   }

   // Text paths are always oriented around (0,0) and are transformed later

   Vector->Bounds = { 0, 0, Vector->txWidth, DOUBLE(dy) };
}
