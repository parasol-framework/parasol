
//********************************************************************************************************************

void StreamChar::eraseChar(extDocument *Self, RSTREAM &Stream) // Erase a character OR an escape code.
{
   if (Stream[Index].Code IS ESC::TEXT) {
      auto &text = escape_data<bcText>(Self, Index);
      if (Offset < text.Text.size()) text.Text.erase(Offset, 1);
      if (Offset > text.Text.size()) Offset = text.Text.size();
   }
   else Stream.erase(Stream.begin() + Index);
}

//********************************************************************************************************************
// Retrieve the first available character.  Assumes that the position is valid.  Does not support unicode!

UBYTE StreamChar::getChar(extDocument *Self, const RSTREAM &Stream)
{
   auto idx = Index;
   auto seek = Offset;
   while (size_t(idx) < Stream.size()) {
      if (Stream[idx].Code IS ESC::TEXT) {
         auto &text = escape_data<bcText>(Self, idx);
         if (seek < text.Text.size()) return text.Text[seek];
         else seek = 0; // The current character offset isn't valid, reset it.
      }
      idx++;
   }
   return 0;
}

//********************************************************************************************************************
// Retrieve the first character after seeking past N viable characters (forward only).  Does not support unicode!

UBYTE StreamChar::getChar(extDocument *Self, const RSTREAM &Stream, INDEX Seek)
{
   auto idx = Index;
   auto off = Offset;

   while (unsigned(idx) < Stream.size()) {
      if (Stream[idx].Code IS ESC::TEXT) {
         auto &text = escape_data<bcText>(Self, idx);
         if (off + Seek < text.Text.size()) return text.Text[off + Seek];
         Seek -= text.Text.size() - off;
         off = 0;
      }
      idx++;
   }

   return 0;
}

void StreamChar::nextChar(extDocument *Self, const RSTREAM &Stream)
{
   if (Stream[Index].Code IS ESC::TEXT) {
      auto &text = escape_data<bcText>(Self, Index);
      if (++Offset >= text.Text.size()) {
         Index++;
         Offset = 0;
      }
   }
   else if (Index < INDEX(Stream.size())) Index++;
}

//********************************************************************************************************************
// Move the cursor to the previous character OR code.

void StreamChar::prevChar(extDocument *Self, const RSTREAM &Stream)
{
   if (Stream[Index].Code IS ESC::TEXT) {
      if (Offset > 0) { Offset--; return; }
   }

   if (Index > 0) {
      Index--;
      if (Stream[Index].Code IS ESC::TEXT) {
         Offset = escape_data<bcText>(Self, Index).Text.size()-1;
      }
      else Offset = 0;
   }
   else Offset = 0;
}

//********************************************************************************************************************
// Return the previous printable character for a given position.  Does not support unicode!  Non-text codes are 
// completely ignored.

UBYTE StreamChar::getPrevChar(extDocument *Self, const RSTREAM &Stream)
{
   if ((Offset > 0) and (Stream[Index].Code IS ESC::TEXT)) {
      return escape_data<bcText>(Self, Index).Text[Offset-1];
   }

   for (auto i=Index-1; i > 0; i--) {
      if (Stream[i].Code IS ESC::TEXT) {
         return escape_data<bcText>(Self, i).Text.back();
      }
   }

   return 0;
}

//********************************************************************************************************************
// Return the previous printable character for a given position.  Inline graphics are considered characters but will
// be returned as 0xff.

UBYTE StreamChar::getPrevCharOrInline(extDocument *Self, const RSTREAM &Stream)
{
   if ((Offset > 0) and (Stream[Index].Code IS ESC::TEXT)) {
      return escape_data<bcText>(Self, Index).Text[Offset-1];
   }

   for (auto i=Index-1; i > 0; i--) {
      if (Stream[i].Code IS ESC::TEXT) {
         return escape_data<bcText>(Self, i).Text.back();
      }
      else if (Stream[i].Code IS ESC::IMAGE) {
         auto &image = escape_data<bcImage>(Self, i);
         if (!image.floating()) {
            return 0xff;
         }
      }
      else if (Stream[i].Code IS ESC::VECTOR) {
         //auto &vec = escape_data<bcVector>(Self, i);
         return 0xff; // TODO: Check for inline status
      }
      //else if (Stream[i].Code IS ESC::OBJECT) {
      //   auto &vec = escape_data<bcObject>(Self, i);
      //   return 0xff; // TODO: Check for inline status
      //}
   }

   return 0;
}
