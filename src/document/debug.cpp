
//********************************************************************************************************************

static std::string printable(extDocument *, StreamChar, ULONG = 60) __attribute__ ((unused));

static std::string printable(extDocument *Self, StreamChar Start, ULONG Length)
{
   std::string result;
   result.reserve(Length);
   StreamChar i = Start;
   while ((i.Index < INDEX(Self->Stream.size())) and (result.size() < Length)) {
      if (Self->Stream[i.Index].Code IS ESC::TEXT) {
         auto &text = escape_data<bcText>(Self, i);
         result += text.Text.substr(i.Offset, result.capacity() - result.size());
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
      auto code = Stream[i].Code;
      if (code IS ESC::FONT) {
         auto style = &escape_data<bcFont>(Self, i);
         out << "[Font";
         out << ":#" << style->FontIndex;
         if ((style->Options & FSO::ALIGN_RIGHT) != FSO::NIL) out << ":A/R";
         if ((style->Options & FSO::ALIGN_CENTER) != FSO::NIL) out << ":A/C";
         if ((style->Options & FSO::BOLD) != FSO::NIL) out << ":Bold";
         out << ":" << style->Fill << "]";
      }
      else if (code IS ESC::PARAGRAPH_START) {
         auto para = &escape_data<bcParagraph>(Self, i);
         if (para->ListItem) out << "[LI]";
         else out << "[PS]";
      }
      else if (code IS ESC::PARAGRAPH_END) {
         out << "[PE]\n";
      }
/*
      else if (code IS ESC::LIST_ITEM) {
         auto item = escape_data<escItem>(str, i);
         out << "[I:X(%d):Width(%d):Custom(%d)]", item->X, item->Width, item->CustomWidth);
      }
*/
      else out << "[" << byteCode(code) << "]";

      printpos = true;
   }

   out << "\nActive Edit: " << Self->ActiveEditCellID << ", Cursor Index: " << Self->CursorIndex.Index << " / X: " << Self->CursorCharX << ", Select Index: " << Self->SelectIndex.Index << "\n";

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
      auto i = line.Start;

      out << std::setw(3) << row << ": Span: " << line.Start.Index << "-" << line.Stop.Index << ": ";
      out << "(" << line.Area.X << "x" << line.Area.Y << ", " << line.Area.Width << "x" << line.Area.Height << ") ";
      if (line.Edit) out << "{ ";
      out << "\"";
      while (i < line.Stop) {
         auto code = Stream[i.Index].Code;
         if (code IS ESC::FONT) {
            auto style = &escape_data<bcFont>(Self, i.Index);
            out << "[E:Font:#" << style->FontIndex << "]";
         }
         else if (code IS ESC::PARAGRAPH_START) {
            auto para = &escape_data<bcParagraph>(Self, i.Index);
            if (para->ListItem) out << "[E:LI]";
            else out << "[E:PS]";
         }
         else if (code IS ESC::PARAGRAPH_END) {
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
         out << i << ": Type: " << LONG(Self->Tabs[i].Type) << ", Ref: " << Self->Tabs[i].Ref << ", XRef: " << Self->Tabs[i].XRef << "\n";
      }
   }

   log.msg("%s", out.str().c_str());
}

#endif
