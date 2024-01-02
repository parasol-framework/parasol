
//********************************************************************************************************************

static std::string printable(RSTREAM &, stream_char, ULONG = 60) __attribute__ ((unused));

static std::string printable(RSTREAM &Stream, stream_char Start, ULONG Length)
{
   std::string result;
   result.reserve(Length);
   stream_char i = Start;
   while ((i.index < INDEX(Stream.size())) and (result.size() < Length)) {
      if (Stream[i.index].code IS SCODE::TEXT) {
         auto &text = Stream.lookup<bc_text>(i);
         result += text.text.substr(i.offset, result.capacity() - result.size());
      }
      else result += '%';
      i.next_code();
   }
   return result;
}

//********************************************************************************************************************

#ifdef DBG_STREAM

static void print_stream(RSTREAM &Stream)
{
   if (Stream.data.empty()) return;

   pf::Log log;
   std::ostringstream out;
   out << "\nSTREAM: " << Stream.size() << " codes\n";
   out << "-------------------------------------------------------------------------------\n";

   bool printpos = false;
   for (INDEX i=0; i < INDEX(Stream.size()); i++) {
      auto code = Stream[i].code;
      if (code IS SCODE::FONT) {
         auto &style = Stream.lookup<bc_font>(i);
         out << "[Font";
         out << ":#" << style.font_index;
         if ((style.options & FSO::ALIGN_RIGHT) != FSO::NIL) out << ":A/R";
         if ((style.options & FSO::ALIGN_CENTER) != FSO::NIL) out << ":A/C";
         if ((style.options & FSO::BOLD) != FSO::NIL) out << ":Bold";
         out << ":" << style.fill << "]";
      }
      else if (code IS SCODE::PARAGRAPH_START) {
         auto &para =  Stream.lookup<bc_paragraph>(i);
         if (para.list_item) out << "[PS:LI]";
         else out << "[PS]";
      }
      else if (code IS SCODE::PARAGRAPH_END) {
         out << "[PE]\n";
      }
      else out << "[" << byte_code(code) << "]";

      printpos = true;
   }

   log.msg("%s", out.str().c_str());
}

#endif

static void print_segments(extDocument *Self)
{
#ifdef DBG_SEGMENTS
   pf::Log log;
   std::ostringstream out;

   out << "\nSEGMENTS\n--------\n";

   for (unsigned si=0; si < Self->Segments.size(); si++) {
      auto &seg = Self->Segments[si];
      auto i = seg.start;

      out << std::setw(3) << si << ": Span: " << seg.start.index << ':' << seg.start.offset << " - " << seg.stop.index << ':' << seg.stop.offset << ": ";
      out << "(" << seg.area.X << "x" << seg.area.Y << ", " << seg.area.Width << "x" << seg.area.Height << ") ";
      if (seg.edit) out << "{ ";
      out << "\"";
      while (i < seg.stop) {
         auto code = seg.stream[0][i.index].code;
         if (code IS SCODE::FONT) {
            auto &style = seg.stream->lookup<bc_font>(i.index);
            out << "[E:Font:#" << style.font_index << "]";
         }
         else if (code IS SCODE::PARAGRAPH_START) {
            auto &para =  seg.stream->lookup<bc_paragraph>(i.index);
            if (para.list_item) out << "[E:LI]";
            else out << "[E:PS]";
         }
         else if (code IS SCODE::PARAGRAPH_END) {
            out << "[E:PE]\n";
         }
         else out << "[E:" <<  byte_code(code) << "]";
         i.next_code();
      }

      out << "\"";
      if (seg.edit) out << " }";
      out << "\n";
   }

   log.msg("%s", out.str().c_str());
#endif
}
