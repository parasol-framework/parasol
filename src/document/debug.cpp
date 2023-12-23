
//********************************************************************************************************************

static std::string printable(extDocument *, stream_char, ULONG = 60) __attribute__ ((unused));

static std::string printable(extDocument *Self, stream_char Start, ULONG Length)
{
   std::string result;
   result.reserve(Length);
   stream_char i = Start;
   while ((i.index < INDEX(Self->Stream.size())) and (result.size() < Length)) {
      if (Self->Stream[i.index].code IS SCODE::TEXT) {
         auto &text = stream_data<bc_text>(Self, i);
         result += text.text.substr(i.offset, result.capacity() - result.size());
      }
      else result += '%';
      i.next_code();
   }
   return result;
}

//********************************************************************************************************************

#ifdef DBG_STREAM

static void print_stream(extDocument *Self, const RSTREAM &Stream)
{
   if (Stream.empty()) return;

   pf::Log log;
   std::ostringstream out;
   out << "\nSTREAM: " << Stream.size() << " codes\n";
   out << "-------------------------------------------------------------------------------\n";

   bool printpos = false;
   for (INDEX i=0; i < INDEX(Stream.size()); i++) {
      auto code = Stream[i].code;
      if (code IS SCODE::FONT) {
         auto style = &stream_data<bc_font>(Self, i);
         out << "[Font";
         out << ":#" << style->font_index;
         if ((style->options & FSO::ALIGN_RIGHT) != FSO::NIL) out << ":A/R";
         if ((style->options & FSO::ALIGN_CENTER) != FSO::NIL) out << ":A/C";
         if ((style->options & FSO::BOLD) != FSO::NIL) out << ":Bold";
         out << ":" << style->fill << "]";
      }
      else if (code IS SCODE::PARAGRAPH_START) {
         auto para = &stream_data<bc_paragraph>(Self, i);
         if (para->list_item) out << "[PS:LI]";
         else out << "[PS]";
      }
      else if (code IS SCODE::PARAGRAPH_END) {
         out << "[PE]\n";
      }
      else out << "[" << byte_code(code) << "]";

      printpos = true;
   }

   out << "\nActive Edit: " << Self->ActiveEditCellID << ", Cursor Index: " << Self->CursorIndex.index << " / X: " << Self->CursorCharX << ", Select Index: " << Self->SelectIndex.index << "\n";

   log.msg("%s", out.str().c_str());
}

#endif

static void print_segments(extDocument *Self, const RSTREAM &Stream)
{
#ifdef DBG_SEGMENTS
   pf::Log log;
   std::ostringstream out;

   out << "\nSEGMENTS\n--------\n";

   for (unsigned row=0; row < Self->Segments.size(); row++) {
      auto &line = Self->Segments[row];
      auto i = line.start;

      out << std::setw(3) << row << ": Span: " << std::setw(3) << line.start.index << " - " << std::setw(3) << line.stop.index << ": ";
      out << "(" << line.area.X << "x" << line.area.Y << ", " << line.area.Width << "x" << line.area.Height << ") ";
      if (line.edit) out << "{ ";
      out << "\"";
      while (i < line.stop) {
         auto code = Stream[i.index].code;
         if (code IS SCODE::FONT) {
            auto style = &stream_data<bc_font>(Self, i.index);
            out << "[E:Font:#" << style->font_index << "]";
         }
         else if (code IS SCODE::PARAGRAPH_START) {
            auto para = &stream_data<bc_paragraph>(Self, i.index);
            if (para->list_item) out << "[E:LI]";
            else out << "[E:PS]";
         }
         else if (code IS SCODE::PARAGRAPH_END) {
            out << "[E:PE]\n";
         }
         else out << "[E:" <<  byte_code(code) << "]";
         i.next_code();
      }

      out << "\"";
      if (line.edit) out << " }";
      out << "\n";
   }

   log.msg("%s", out.str().c_str());
#endif
}
