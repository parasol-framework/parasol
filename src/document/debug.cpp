
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
      i.nextCode();
   }
   return result;
}

//********************************************************************************************************************

static void print_xmltree(objXML::TAGS &Tags, LONG &Indent)
{
   pf::Log log(__FUNCTION__);

   for (auto &tag : Tags) {
      std::ostringstream buffer;
      for (LONG i=0; i < Indent; i++) buffer << ' ';

      if (tag.isContent()) {
         auto output = tag.Attribs[0].Value;
         auto pos = output.find("\n", 0);
         while (pos != std::string::npos) {
            output.replace(pos, 1, "_");
            pos = output.find("\n", pos+1);
         }

         buffer << '[' << output << ']';
      }
      else {
         buffer << '<' << tag.Attribs[0].Name;
         for (unsigned a=1; a < tag.Attribs.size(); a++) {
            buffer << " " << tag.Attribs[a].Name << "=\"" << tag.Attribs[a].Value << "\"";
         }
         buffer << '>';
      }

      log.msg("%s", buffer.str().c_str());

      Indent++;
      print_xmltree(tag.Children, Indent);
      Indent--;
   }
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

#ifdef DBG_SEGMENTS

#include <stdio.h>
#include <stdarg.h>
#undef NULL
#define NULL 0

static void print_segments(extDocument *Self, const RSTREAM &Stream)
{
   pf::Log log;
   std::ostringstream out;

   out << "\nSEGMENTS\n--------\n";

   for (unsigned row=0; row < Self->Segments.size(); row++) {
      auto &line = Self->Segments[row];
      auto i = line.start;

      out << std::setw(3) << row << ": Span: " << line.start.index << "-" << line.Stop.index << ": ";
      out << "(" << line.Area.x << "x" << line.Area.y << ", " << line.Area.width << "x" << line.Area.height << ") ";
      if (line.Edit) out << "{ ";
      out << "\"";
      while (i < line.Stop) {
         auto code = Stream[i.index].Code;
         if (code IS SCODE::FONT) {
            auto style = &stream_data<bc_font>(Self, i.index);
            out << "[E:Font:#" << style->font_index << "]";
         }
         else if (code IS SCODE::PARAGRAPH_START) {
            auto para = &stream_data<bc_paragraph>(Self, i.index);
            if (para->ListItem) out << "[E:LI]";
            else out << "[E:PS]";
         }
         else if (code IS SCODE::PARAGRAPH_END) {
            out << "[E:PE]\n";
         }
         else out << "[E:" <<  byteCode(code) << "]";
         i.nextCode();
      }

      out << "\"";
      if (line.Edit) out << " }";
      out << "\n";
   }

   log.msg("%s", out.str().c_str());
}

static void print_tabfocus(extDocument *Self)
{
   pf::Log log;
   std::ostringstream out;

   if (!Self->Tabs.empty()) {
      out << "\nTAB FOCUSLIST\n-------------\n";

      for (unsigned i=0; i < Self->Tabs.size(); i++) {
         out << i << ": Type: " << LONG(Self->Tabs[i].type) << ", Ref: " << Self->Tabs[i].ref << ", XRef: " << Self->Tabs[i].XRef << "\n";
      }
   }

   log.msg("%s", out.str().c_str());
}

#endif
