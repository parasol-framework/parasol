
//********************************************************************************************************************

void stream_char::erase_char(extDocument *Self, RSTREAM &Stream) // Erase a character OR an escape code.
{
   if (Stream[index].code IS SCODE::TEXT) {
      auto &text = Self->stream_data<bc_text>(index);
      if (offset < text.text.size()) text.text.erase(offset, 1);
      if (offset > text.text.size()) offset = text.text.size();
   }
   else Stream.erase(Stream.begin() + index);
}

//********************************************************************************************************************
// Retrieve the first available character.  Assumes that the position is valid.  Does not support unicode!

UBYTE stream_char::get_char(extDocument *Self, const RSTREAM &Stream)
{
   auto idx = index;
   auto seek = offset;
   while (size_t(idx) < Stream.size()) {
      if (Stream[idx].code IS SCODE::TEXT) {
         auto &text = Self->stream_data<bc_text>(idx);
         if (seek < text.text.size()) return text.text[seek];
         else seek = 0; // The current character offset isn't valid, reset it.
      }
      idx++;
   }
   return 0;
}

//********************************************************************************************************************
// Retrieve the first character after seeking past N viable characters (forward only).  Does not support unicode!

UBYTE stream_char::get_char(extDocument *Self, const RSTREAM &Stream, INDEX Seek)
{
   auto idx = index;
   auto off = offset;

   while (unsigned(idx) < Stream.size()) {
      if (Stream[idx].code IS SCODE::TEXT) {
         auto &text = Self->stream_data<bc_text>(idx);
         if (off + Seek < text.text.size()) return text.text[off + Seek];
         Seek -= text.text.size() - off;
         off = 0;
      }
      idx++;
   }

   return 0;
}

void stream_char::next_char(extDocument *Self, const RSTREAM &Stream)
{
   if (Stream[index].code IS SCODE::TEXT) {
      auto &text = Self->stream_data<bc_text>(index);
      if (++offset >= text.text.size()) {
         index++;
         offset = 0;
      }
   }
   else if (index < INDEX(Stream.size())) index++;
}

//********************************************************************************************************************
// Move the cursor to the previous character OR code.

void stream_char::prev_char(extDocument *Self, const RSTREAM &Stream)
{
   if (Stream[index].code IS SCODE::TEXT) {
      if (offset > 0) { offset--; return; }
   }

   if (index > 0) {
      index--;
      if (Stream[index].code IS SCODE::TEXT) {
         offset = Self->stream_data<bc_text>(index).text.size()-1;
      }
      else offset = 0;
   }
   else offset = 0;
}

//********************************************************************************************************************
// Return the previous printable character for a given position.  Does not support unicode!  Non-text codes are 
// completely ignored.

UBYTE stream_char::get_prev_char(extDocument *Self, const RSTREAM &Stream)
{
   if ((offset > 0) and (Stream[index].code IS SCODE::TEXT)) {
      return Self->stream_data<bc_text>(index).text[offset-1];
   }

   for (auto i=index-1; i > 0; i--) {
      if (Stream[i].code IS SCODE::TEXT) {
         return Self->stream_data<bc_text>(i).text.back();
      }
   }

   return 0;
}

//********************************************************************************************************************
// Return the previous printable character for a given position.  Inline graphics are considered characters but will
// be returned as 0xff.

UBYTE stream_char::get_prev_char_or_inline(extDocument *Self, const RSTREAM &Stream)
{
   if ((offset > 0) and (Stream[index].code IS SCODE::TEXT)) {
      return Self->stream_data<bc_text>(index).text[offset-1];
   }

   for (auto i=index-1; i > 0; i--) {
      if (Stream[i].code IS SCODE::TEXT) {
         return Self->stream_data<bc_text>(i).text.back();
      }
      else if (Stream[i].code IS SCODE::IMAGE) {
         auto &image = Self->stream_data<bc_image>(i);
         if (!image.floating()) {
            return 0xff;
         }
      }
      //else if (Stream[i].code IS SCODE::OBJECT) {
      //   auto &vec = Self->stream_data<bcObject>(i);
      //   return 0xff; // TODO: Check for inline status
      //}
   }

   return 0;
}
