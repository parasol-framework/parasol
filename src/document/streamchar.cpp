
//********************************************************************************************************************

void stream_char::eraseChar(extDocument *Self, RSTREAM &Stream) // Erase a character OR an escape code.
{
   if (Stream[Index].code IS ESC::TEXT) {
      auto &text = escape_data<bc_text>(Self, Index);
      if (Offset < text.text.size()) text.text.erase(Offset, 1);
      if (Offset > text.text.size()) Offset = text.text.size();
   }
   else Stream.erase(Stream.begin() + Index);
}

//********************************************************************************************************************
// Retrieve the first available character.  Assumes that the position is valid.  Does not support unicode!

UBYTE stream_char::getChar(extDocument *Self, const RSTREAM &Stream)
{
   auto idx = Index;
   auto seek = Offset;
   while (size_t(idx) < Stream.size()) {
      if (Stream[idx].code IS ESC::TEXT) {
         auto &text = escape_data<bc_text>(Self, idx);
         if (seek < text.text.size()) return text.text[seek];
         else seek = 0; // The current character offset isn't valid, reset it.
      }
      idx++;
   }
   return 0;
}

//********************************************************************************************************************
// Retrieve the first character after seeking past N viable characters (forward only).  Does not support unicode!

UBYTE stream_char::getChar(extDocument *Self, const RSTREAM &Stream, INDEX Seek)
{
   auto idx = Index;
   auto off = Offset;

   while (unsigned(idx) < Stream.size()) {
      if (Stream[idx].code IS ESC::TEXT) {
         auto &text = escape_data<bc_text>(Self, idx);
         if (off + Seek < text.text.size()) return text.text[off + Seek];
         Seek -= text.text.size() - off;
         off = 0;
      }
      idx++;
   }

   return 0;
}

void stream_char::nextChar(extDocument *Self, const RSTREAM &Stream)
{
   if (Stream[Index].code IS ESC::TEXT) {
      auto &text = escape_data<bc_text>(Self, Index);
      if (++Offset >= text.text.size()) {
         Index++;
         Offset = 0;
      }
   }
   else if (Index < INDEX(Stream.size())) Index++;
}

//********************************************************************************************************************
// Move the cursor to the previous character OR code.

void stream_char::prevChar(extDocument *Self, const RSTREAM &Stream)
{
   if (Stream[Index].code IS ESC::TEXT) {
      if (Offset > 0) { Offset--; return; }
   }

   if (Index > 0) {
      Index--;
      if (Stream[Index].code IS ESC::TEXT) {
         Offset = escape_data<bc_text>(Self, Index).text.size()-1;
      }
      else Offset = 0;
   }
   else Offset = 0;
}

//********************************************************************************************************************
// Return the previous printable character for a given position.  Does not support unicode!  Non-text codes are 
// completely ignored.

UBYTE stream_char::getPrevChar(extDocument *Self, const RSTREAM &Stream)
{
   if ((Offset > 0) and (Stream[Index].code IS ESC::TEXT)) {
      return escape_data<bc_text>(Self, Index).text[Offset-1];
   }

   for (auto i=Index-1; i > 0; i--) {
      if (Stream[i].code IS ESC::TEXT) {
         return escape_data<bc_text>(Self, i).text.back();
      }
   }

   return 0;
}

//********************************************************************************************************************
// Return the previous printable character for a given position.  Inline graphics are considered characters but will
// be returned as 0xff.

UBYTE stream_char::getPrevCharOrInline(extDocument *Self, const RSTREAM &Stream)
{
   if ((Offset > 0) and (Stream[Index].code IS ESC::TEXT)) {
      return escape_data<bc_text>(Self, Index).text[Offset-1];
   }

   for (auto i=Index-1; i > 0; i--) {
      if (Stream[i].code IS ESC::TEXT) {
         return escape_data<bc_text>(Self, i).text.back();
      }
      else if (Stream[i].code IS ESC::IMAGE) {
         auto &image = escape_data<bc_image>(Self, i);
         if (!image.floating()) {
            return 0xff;
         }
      }
      else if (Stream[i].code IS ESC::VECTOR) {
         //auto &vec = escape_data<bc_vector>(Self, i);
         return 0xff; // TODO: Check for inline status
      }
      //else if (Stream[i].code IS ESC::OBJECT) {
      //   auto &vec = escape_data<bcObject>(Self, i);
      //   return 0xff; // TODO: Check for inline status
      //}
   }

   return 0;
}
