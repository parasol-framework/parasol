
static const LONG MAXLOOP = 100000;

static const char glDefaultStyles[] =
"<template name=\"h1\"><p leading=\"2.0\"><font face=\"Open Sans\" size=\"18\" colour=\"0,0,0\" style=\"bold\"><inject/></font></p></template>\n\
<template name=\"h2\"><p leading=\"2.0\"><font face=\"Open Sans\" size=\"16\" colour=\"0,0,0\" style=\"bold\"><inject/></font></p></template>\n\
<template name=\"h3\"><p leading=\"1.5\"><font face=\"Open Sans\" size=\"14\" colour=\"0,0,0\" style=\"bold\"><inject/></font></p></template>\n\
<template name=\"h4\"><p leading=\"1.5\"><font face=\"Open Sans\" size=\"14\" colour=\"0,0,0\"><inject/></font></p></template>\n\
<template name=\"h5\"><p leading=\"1.25\"><font face=\"Open Sans\" size=\"12\" colour=\"0,0,0\"><inject/></font></p></template>\n\
<template name=\"h6\"><p leading=\"1.25\"><font face=\"Open Sans\" size=\"10\" colour=\"0,0,0\"><inject/></font></p></template>\n";

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

#ifdef DBG_LINES

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

//static BYTE glWhitespace = true;  // Setting this to true tells the parser to ignore whitespace (prevents whitespace being used as content)

static Field * find_field(OBJECTPTR Object, CSTRING Name, OBJECTPTR *Source) // Read-only, thread safe function.
{
   // Skip any special characters that are leading the field name (e.g. $, @).  Some symbols like / are used for XPath
   // lookups, so we only want to skip reserved symbols or we risk confusion between real fields and variable fields.

   while (Name[0]) {
      if (Name[0] IS '$') Name++;
      else if (Name[0] IS '@') Name++;
      else break;
   }

   return FindField(Object, StrHash(Name, false), Source);
}

//********************************************************************************************************************

static bool read_rgb8(CSTRING Value, RGB8 *RGB)
{
   FRGB rgb;
   if (!vecReadPainter(NULL, Value, &rgb, NULL, NULL, NULL)) {
      RGB->Red   = F2T(rgb.Red   * 255.0);
      RGB->Green = F2T(rgb.Green * 255.0);
      RGB->Blue  = F2T(rgb.Blue  * 255.0);
      RGB->Alpha = F2T(rgb.Alpha * 255.0);
      return true;
   }
   else return false;
}

//********************************************************************************************************************

static STRING stream_to_string(extDocument *Self, StreamChar Start, StreamChar End)
{
   if (End < Start) std::swap(Start, End);

   // Calculate the total number of chars required.

   auto cs = Start;
   size_t size = 0;
   for (; (cs.Index <= End.Index) and (cs.Index < INDEX(Self->Stream.size())); cs.nextCode()) {
      if (Self->Stream[cs.Index].Code IS ESC::TEXT) {
         auto &text = escape_data<bcText>(Self, cs);
         if (cs.Index < End.Index) size = text.Text.size() - cs.Offset;         
         else size = (End.Offset < text.Text.size() ? End.Offset : text.Text.size()) - cs.Offset;
      }
   }

   // Generate the result string

   STRING str;
   if (!AllocMemory(size + 1, MEM::STRING|MEM::NO_CLEAR, &str)) {
      cs = Start;
      LONG pos = 0;
      for (; (cs.Index <= End.Index) and (cs.Index < INDEX(Self->Stream.size())); cs.nextCode()) {
         if (Self->Stream[cs.Index].Code IS ESC::TEXT) {
            auto &text = escape_data<bcText>(Self, cs);
            if (cs.Index < End.Index) {
               CopyMemory(text.Text.c_str() + cs.Offset, str + pos, text.Text.size() - cs.Offset);
               pos += text.Text.size() - cs.Offset;
            }
            else {
               CopyMemory(text.Text.c_str() + cs.Offset, str + pos, End.Offset - cs.Offset);
               pos += End.Offset - cs.Offset;
            }
         }
      }
      str[pos] = 0;
      return str;
   }

   return NULL;
}

/*********************************************************************************************************************

This function can be used for performing simple calculations on numeric values and strings.  It can return a result in
either a numeric format or in a string buffer if the calculation involves non-numeric characters.  Here are some
examples of valid strings:

<pre>
100/50+(12*14)
0.05 * 100 + '%'
</pre>

Currently acceptable operators are plus, minus, divide and multiply.  String references must be enclosed in single
quotes or will be ignored.  Brackets may be used to organise the order of operations during calculation.

Special operators include:

<types type="Symbol">
<type name="p">This character immediately followed with an integer allows you to change the floating-point precision of output values.</>
<type name="f">The same as the 'p' operator except the precision is always guaranteed to be fixed at that value through the use of trailing zeros (so a fixed precision of two used to print the number '7' will give a result of '7.00'.</>
</>

*********************************************************************************************************************/

static std::string write_calc(DOUBLE Value, WORD Precision)
{
   if (!Precision) return std::to_string(F2T(Value));

   LARGE wholepart = F2T(Value);
   auto out = std::to_string(wholepart);

   DOUBLE fraction = std::abs(Value) - std::abs(wholepart);
   if ((fraction > 0) or (Precision < 0)) {
      out += '.';
      fraction *= 10;
      auto px = std::abs(Precision);
      while ((fraction > 0.00001) and (px > 0)) {
         auto ival = F2T(fraction);
         out += char(ival) + '0';
         fraction = (fraction - ival) * 10;
         px--;
      }

      while (px > 0) { out += '0'; px--; }
   }

   return out;
}

ERROR calc(const std::string &String, DOUBLE *Result, std::string &Output)
{
   enum SIGN { PLUS=1, MINUS, MULTIPLY, DIVIDE, MODULO };

   if (Result) *Result = 0;

   Output.clear();

   // Search for brackets and translate them first

   std::string in(String);
   while (true) {
      // Find the last bracketed reference

      LONG last_bracket = 0;
      for (unsigned i=0; i < in.size(); i++) {
         if (in[i] IS '\'') { // Skip anything in quotes
            i++;
            while (in[i]) {
               if (in[i] IS '\\') {
                  i++; // Skip backslashes and the following character
                  if (!in[i]) break;
               }
               else if (in[i] IS '\'') break;
               i++;
            }
            if (in[i] IS '\'') i++;
         }
         else if (in[i] IS '(') last_bracket = i;
      }

      if (last_bracket > 0) { // Bracket found, translate its contents
         LONG end;
         for (end=last_bracket+1; (in[end]) and (in[end-1] != ')'); end++);
         std::string buf(in, last_bracket, end - last_bracket);

         DOUBLE calc_float;
         std::string out;
         calc(buf.c_str()+1, &calc_float, out);
         in.replace(last_bracket, end - last_bracket, out);
      }
      else break;
   }

   // Perform the calculation

   STRING end;
   WORD precision = 9;
   DOUBLE total   = 0;
   DOUBLE overall = 0;
   LONG index     = 0;
   SIGN sign      = PLUS;
   bool number    = false;
   for (unsigned s=0; in[s];) {
      if (in[s] <= 0x20); // Do nothing with whitespace
      else if (in[s] IS '\'') {
         if (number) { // Write the current floating point number to the buffer before the next calculation
            Output  += write_calc(total, precision);
            overall += total; // Reset the number
            total   = 0;
            number  = false;
         }

         s++;
         while (index < LONG(Output.size())-1) {
            if (in[s] IS '\\') s++; // Skip the \ character and continue so that we can copy the character immediately after it
            else if (in[s] IS '\'') break;

            Output += in[s++];
         }
      }
      else if (in[s] IS 'f') { // Fixed floating point precision adjustment
         s++;
         precision = -strtol(in.c_str() + s, &end, 10);
         s += end - in.c_str();
         continue;
      }
      else if (in[s] IS 'p') { // Floating point precision adjustment
         s++;
         precision = strtol(in.c_str() + s, &end, 10);
         s += end - in.c_str();
         continue;
      }
      else if ((in[s] >= '0') and (in[s] <= '9')) {
         number = true;
         DOUBLE fvalue = strtod(in.c_str() + s, &end);
         s += end - in.c_str();

         if (sign IS MINUS)         total = total - fvalue;
         else if (sign IS MULTIPLY) total = total * fvalue;
         else if (sign IS MODULO)   total = F2I(total) % F2I(fvalue);
         else if (sign IS DIVIDE) {
            if (fvalue) total = total / fvalue; // NB: Avoid division by zero errors
         }
         else total += fvalue;

         sign = PLUS; // The mathematical sign is reset whenever a number is encountered
         continue;
      }
      else if (in[s] IS '-') {
         if (sign IS MINUS) sign = PLUS; // Handle double-negatives
         else sign = MINUS;
      }
      else if (in[s] IS '+') sign = PLUS;
      else if (in[s] IS '*') sign = MULTIPLY;
      else if (in[s] IS '/') sign = DIVIDE;
      else if (in[s] IS '%') sign = MODULO;

      for (++s; (in[s] & 0xc0) IS 0x80; s++);
   }

   if (number) Output += write_calc(total, precision);
   if (Result) *Result = overall + total;
   return ERR_Okay;
}

/*********************************************************************************************************************

This function is used to translate strings that make object and field references using the standard referencing format.
References are made to objects by enclosing statements within square brackets.  As a result of calling this function,
all references within the Buffer will be translated to their relevant format.  The Buffer needs to be large enough to
accommodate these adjustments as it will be expanded during the translation.  It is recommended that the Buffer is at
least two times the actual length of the string that you are translating.

Valid references can be made to an object by name, ID or relative parameters.  Here are some examples illustrating the
different variations:

<types type="Reference">
<type name="[surface]">Name reference.</>
<type name="[#49302]">ID reference.</>
<type name="[self]">Relative reference to the object that has the current context, or the document.</>
</table>

Field references are a slightly different matter and will be converted to the value of the field that they are
referencing.  A field reference is defined using the object referencing format, but they contain a `.fieldname`
extension.  Here are some examples:

<pre>
[surface.width]
[file.location]
</pre>

A string such as `[mywindow.height] + [mywindow.width]` could be translated to `255 + 120` for instance.  References to
string based fields can expand the Buffer very quickly, which is why large buffer spaces are recommended for all-purpose
translations.

Simple calculations are possible by enclosing a statement within a `[=...]` section.  For example the aforementioned
string can be expanded to `[=[mywindow.height] + [mywindow.width]]`, which would give a result of 375.

The escape character for string translation is `$` and should be used as `[$...]`, which prevents everything within the
square brackets from being translated.  The `[$]` characters will be removed as part of this process unless the
KEEP_ESCAPE flag is used.  To escape a single right or left bracket, use `[rb]` or `[lb]` respectively.

*********************************************************************************************************************/

// Evaluate object references and calculations

static ERROR tag_xml_content_eval(extDocument *Self, std::string &Buffer)
{
   pf::Log log(__FUNCTION__);
   LONG i;

   // Quick check for translation symbols

   if (Buffer.find('[') IS std::string::npos) return ERR_EmptyString;

   log.traceBranch("%.80s", Buffer.c_str());

   ERROR error = ERR_Okay;
   ERROR majorerror = ERR_Okay;

   // Skip to the end of the buffer (translation occurs 'backwards')

   auto pos = LONG(Buffer.size() - 1);
   while (pos >= 0) {
      if ((Buffer[pos] IS '[') and ((Buffer[pos+1] IS '@') or (Buffer[pos+1] IS '%'))) {
         // Ignore arguments, e.g. [@id] or [%id].  It's also useful for ignoring [@attrib] in xpath.
         pos--;
      }
      else if (Buffer[pos] IS '[') {
         // Make sure that there is a balanced closing bracket

         LONG end;
         LONG balance = 0;
         for (end=pos; Buffer[end]; end++) {
            if (Buffer[end] IS '[') balance++;
            else if (Buffer[end] IS ']') {
               balance--;
               if (!balance) break;
            }
         }

         if (Buffer[end] != ']') {
            log.warning("Unbalanced string: %.90s ...", Buffer.c_str());
            return ERR_InvalidData;
         }

         if (Buffer[pos+1] IS '=') { // Perform a calculation
            std::string num;
            num.assign(Buffer, pos+2, end-(pos+2));

            std::string calcbuffer;
            DOUBLE value;
            calc(num, &value, calcbuffer);
            Buffer.insert(end-pos+1, calcbuffer);
         }
         else if (Buffer[pos+1] IS '$') { // Escape sequence - e.g. translates [$ABC] to ABC.  Note: Use [rb] and [lb] instead for brackets.
            Buffer.erase(end, 1); // ']'
            Buffer.erase(pos, 2); // '[$'
            pos--;
            continue;
         }
         else {
            std::string name;
            name.reserve(64);

            for (i=pos+1; (Buffer[i] != '.') and (i < end); i++) {
               name += std::tolower(Buffer[i]);
            }

            // Check for [lb] and [rb] escape codes

            char code = 0;
            if (name == "rb") code = ']';
            else if (name == "lb") code = '[';

            if (code) {
               Buffer[pos] = code;
               Buffer.erase(pos+1, 3);
               pos--;
               continue;
            }
            else {
               OBJECTID objectid = 0;
               if (!StrMatch(name, "self")) objectid = CurrentContext()->UID;
               else FindObject(name.c_str(), 0, FOF::SMART_NAMES, &objectid);

               if (objectid) {
                  OBJECTPTR object = NULL;
                  if (Buffer[i] IS '.') {
                     // Get the field from the object
                     i++;

                     std::string field(Buffer, i, end);
                     if (!AccessObject(objectid, 2000, &object)) {
                        OBJECTPTR target;
                        Field *classfield;
                        if (((classfield = find_field(object, field.c_str(), &target))) and (classfield->Flags & FD_STRING)) {
                           CSTRING str;
                           if (!GetField(object, (FIELD)classfield->FieldID|TSTR, &str)) {
                              Buffer.insert(end-pos+1, str);
                           }
                        }
                        else { // Get field as an unlisted type and manage any buffer overflow
                           std::string tbuffer;
                           tbuffer.reserve(4096);
repeat:
                           tbuffer[tbuffer.capacity()-1] = 0;
                           if (!GetFieldVariable(object, field.c_str(), tbuffer.data(), tbuffer.capacity())) {
                              if (tbuffer[tbuffer.capacity()-1]) {
                                 tbuffer.reserve(tbuffer.capacity() * 2);
                                 goto repeat;
                              }
                              Buffer.insert(end-pos+1, tbuffer);
                           }
                        }
                        // NB: For fields, error code is always Okay so that the reference evaluates to NULL

                        ReleaseObject(object);
                     }
                     else error = ERR_AccessObject;
                  }
                  else { // Convert the object reference to an ID
                     Buffer.insert(end-pos+1, std::string("#") + std::to_string(objectid));
                  }
               }
               else {
                  error = ERR_NoMatchingObject;
                  log.traceWarning("Failed to find object '%s'", name.c_str());
               }
            }
         }

         if (error != ERR_Okay) {
            pos--;
            majorerror = error;
            error = ERR_Okay;
         }
      }
      else pos--;
   }

   log.trace("Result: %s", Buffer.c_str());

   return majorerror;
}

//********************************************************************************************************************

static bool eval_condition(const std::string &String)
{
   pf::Log log(__FUNCTION__);

   static const FieldDef table[] = {
      { "<>", COND_NOT_EQUAL },
      { "!=", COND_NOT_EQUAL },
      { "=",  COND_EQUAL },
      { "==", COND_EQUAL },
      { "<",  COND_LESS_THAN },
      { "<=", COND_LESS_EQUAL },
      { ">",  COND_GREATER_THAN },
      { ">=", COND_GREATER_EQUAL },
      { NULL, 0 }
   };

   LONG start = 0;
   while ((start < LONG(String.size())) and (String[start] <= 0x20)) start++;

   bool reverse = false;

   // Find the condition statement

   LONG i;
   for (i=start; i < LONG(String.size()); i++) {
      if ((String[i] IS '!') and (String[i+1] IS '=')) break;
      if (String[i] IS '>') break;
      if (String[i] IS '<') break;
      if (String[i] IS '=') break;
   }

   // If there is no condition statement, evaluate the statement as an integer

   if (i >= LONG(String.size())) {
      if (StrToInt(String)) return true;
      else return false;
   }

   LONG cpos = i;

   // Extract Test value

   while ((i > 0) and (String[i-1] IS ' ')) i--;
   std::string test(String, i);

   // Condition value

   LONG condition = 0;
   {
      std::string cond;
      cond.reserve(3);
      char c;
      for (i=cpos,c=0; (c < 2) and ((String[i] IS '!') or (String[i] IS '=') or (String[i] IS '>') or (String[i] IS '<')); i++) {
         cond[c++] = String[i];
      }

      for (unsigned j=0; table[j].Name; j++) {
         if (!StrMatch(cond, table[j].Name)) {
            condition = table[j].Value;
            break;
         }
      }
   }

   while ((String[i]) and (String[i] <= 0x20)) i++; // skip white-space

   bool truth = false;
   if (!test.empty()) {
      if (condition) {
         // Convert the If->Compare to its specified type

         auto cmp_type  = StrDatatype(String.c_str()+i);
         auto test_type = StrDatatype(test.c_str());

         if (((test_type IS STT::NUMBER) or (test_type IS STT::FLOAT)) and ((cmp_type IS STT::NUMBER) or (cmp_type IS STT::FLOAT))) {
            auto cmp_float  = StrToFloat(String.c_str()+i);
            auto test_float = StrToFloat(test);
            switch (condition) {
               case COND_NOT_EQUAL:     if (test_float != cmp_float) truth = true; break;
               case COND_EQUAL:         if (test_float IS cmp_float) truth = true; break;
               case COND_LESS_THAN:     if (test_float <  cmp_float) truth = true; break;
               case COND_LESS_EQUAL:    if (test_float <= cmp_float) truth = true; break;
               case COND_GREATER_THAN:  if (test_float >  cmp_float) truth = true; break;
               case COND_GREATER_EQUAL: if (test_float >= cmp_float) truth = true; break;
               default: log.warning("Unsupported condition type %d.", condition);
            }
         }
         else if (condition IS COND_EQUAL) {
            if (!StrMatch(test, String.c_str()+i)) truth = true;
         }
         else if (condition IS COND_NOT_EQUAL) {
            if (StrMatch(test, String.c_str()+i) != ERR_Okay) truth = true;
         }
         else log.warning("String comparison for condition %d not possible.", condition);
      }
      else log.warning("No test condition in \"%s\".", String.c_str());
   }
   else log.warning("No test value in \"%s\".", String.c_str());

   if (reverse) return truth ^ 1;
   else return truth;
}

//********************************************************************************************************************

static ERROR consume_input_events(objVector *Vector, const InputEvent *Events)
{
   auto Self = (extDocument *)CurrentContext();

   for (auto input=Events; input; input=input->Next) {
      if ((input->Flags & JTYPE::MOVEMENT) != JTYPE::NIL) {
         for (auto scan=input->Next; (scan) and ((scan->Flags & JTYPE::MOVEMENT) != JTYPE::NIL); scan=scan->Next) {
            input = scan;
         }

         if (input->OverID IS Self->Page->UID) Self->MouseInPage = true;
         else Self->MouseInPage = false;


         check_mouse_pos(Self, input->X, input->Y);

         // Note that this code has to 'drop through' due to the movement consolidation loop earlier in this subroutine.
      }

      if (input->Type IS JET::LMB) {
         if (input->Value > 0) {
            Self->LMB = true;
            check_mouse_click(Self, input->X, input->Y);
         }
         else {
            Self->LMB = false;
            check_mouse_release(Self, input->X, input->Y);
         }
      }
   }

   return ERR_Okay;
}

//********************************************************************************************************************
// Checks if the file path is safe, i.e. does not refer to an absolute file location.

static LONG safe_file_path(extDocument *Self, const std::string &Path)
{
   if ((Self->Flags & DCF::UNRESTRICTED) != DCF::NIL) return true;





   return false;
}

//********************************************************************************************************************
// Used by if, elseif, while statements to check the satisfaction of conditions.

static bool check_tag_conditions(extDocument *Self, XMLTag &Tag)
{
   pf::Log log("eval");

   bool satisfied = false;
   bool reverse = false;
   for (unsigned i=1; i < Tag.Attribs.size(); i++) {
      if (!StrMatch("statement", Tag.Attribs[i].Name)) {
         satisfied = eval_condition(Tag.Attribs[i].Value);
         log.trace("Statement: %s", Tag.Attribs[i].Value);
         break;
      }
      else if (!StrMatch("exists", Tag.Attribs[i].Name)) {
         OBJECTID object_id;
         if (!FindObject(Tag.Attribs[i].Value.c_str(), 0, FOF::SMART_NAMES, &object_id)) {
            if (valid_objectid(Self, object_id)) {
               satisfied = true;
            }
         }
         break;
      }
      else if (!StrMatch("notnull", Tag.Attribs[i].Name)) {
         log.trace("NotNull: %s", Tag.Attribs[i].Value);
         if (Tag.Attribs[i].Value.empty()) satisfied = false;
         else if (Tag.Attribs[i].Value == "0") satisfied = false;
         else satisfied = true;
      }
      else if ((!StrMatch("isnull", Tag.Attribs[i].Name)) or (!StrMatch("null", Tag.Attribs[i].Name))) {
         log.trace("IsNull: %s", Tag.Attribs[i].Value);
            if (Tag.Attribs[i].Value.empty()) satisfied = true;
            else if (Tag.Attribs[i].Value == "0") satisfied = true;
            else satisfied = false;
      }
      else if (!StrMatch("not", Tag.Attribs[i].Name)) {
         reverse = true;
      }
   }

   // Check for a not condition and invert the satisfied value if found

   if (reverse) satisfied = satisfied ^ 1;

   return satisfied;
}

/*********************************************************************************************************************
** Processes an XML tag and passes it to parse_tag().
**
** IXF_HOLDSTYLE:  If set, the font style will not be cleared.
** IXF_RESETSTYLE: If set, the current font style will be completely reset, rather than defaulting to the most recent font style used at the insertion point.
** IXF_SIBLINGS:   If set, sibling tags that follow the root will be parsed.
*/

static ERROR insert_xml(extDocument *Self, objXML *XML, objXML::TAGS &Tag, INDEX TargetIndex, UBYTE Flags)
{
   pf::Log log(__FUNCTION__);

   if (TargetIndex < 0) TargetIndex = Self->Stream.size();

   log.traceBranch("Index: %d, Flags: $%.2x, Tag: %s", TargetIndex, Flags, Tag[0].Attribs[0].Name.c_str());

   // Retrieve the most recent font definition and use that as the style that we're going to start with.

   if (Flags & IXF_HOLDSTYLE) {
      // Do nothing to change the style
   }
   else {
      Self->Style = style_status();

      if (Flags & IXF_RESETSTYLE) {
         // Do not search for the most recent font style
      }
      else {
         for (auto i = TargetIndex - 1; i > 0; i--) {
            if (Self->Stream[i].Code IS ESC::FONT) {
               Self->Style.FontStyle = escape_data<bcFont>(Self, i);
               log.trace("Found existing font style, font index %d, flags $%.8x.", Self->Style.FontStyle.FontIndex, Self->Style.FontStyle.Options);
               break;
            }
         }
      }

      // If no style is available, we need to create a default font style and insert it at the start of the stream.

      if (Self->Style.FontStyle.FontIndex IS -1) {
         if ((Self->Style.FontStyle.FontIndex = create_font(Self->FontFace, "Regular", Self->FontSize)) IS -1) {
            if ((Self->Style.FontStyle.FontIndex = create_font("Open Sans", "Regular", 10)) IS -1) {
               return ERR_Failed;
            }
         }

         Self->Style.FontStyle.Fill = Self->FontFill;
         Self->Style.FontChange = true;
      }

      if (auto font = Self->Style.FontStyle.getFont()) {
         Self->Style.Face  = font->Face;
         Self->Style.Point = font->Point;
      }
   }

   // Parse content and insert it at the end of the stream (we will move it to the insertion point afterwards).

   StreamChar inserted_at(Self->Stream.size());
   StreamChar insert_index(Self->Stream.size());
   if (Flags & IXF_SIBLINGS) { // Siblings of Tag are included
      parse_tags(Self, XML, Tag, insert_index);
   }
   else { // Siblings of Tag are not included
      auto parse_flags = IPF::NIL;
      parse_tag(Self, XML, Tag[0], insert_index, parse_flags);
   }

   if (Flags & IXF_CLOSESTYLE) style_check(Self, insert_index);

   if (INDEX(Self->Stream.size()) <= inserted_at.Index) {
      log.trace("parse_tag() did not insert any content into the stream.");
      return ERR_NothingDone;
   }

   // Move the content from the end of the stream to the requested insertion point

   if (TargetIndex < inserted_at.Index) {
      auto length = Self->Stream.size() - inserted_at.Index;
      log.trace("Moving new content of %d bytes to the insertion point at index %d", TargetIndex, length);
      Self->Stream.insert(Self->Stream.begin() + TargetIndex, Self->Stream.begin() + inserted_at.Index, Self->Stream.begin() + length);
      Self->Stream.resize(inserted_at.Index + length);
   }

   // Check that the FocusIndex is valid (there's a slim possibility that it may not be if AC_Focus has been
   // incorrectly used).

   if (Self->FocusIndex >= LONG(Self->Tabs.size())) Self->FocusIndex = -1;

   return ERR_Okay;
}

static ERROR insert_xml(extDocument *Self, objXML *XML, XMLTag &Tag, StreamChar TargetIndex, UBYTE Flags)
{
   pf::Log log(__FUNCTION__);

   if (TargetIndex < 0) TargetIndex = Self->Stream.size();

   log.traceBranch("Index: %d, Flags: $%.2x, Tag: %s", TargetIndex.Index, Flags, Tag.Attribs[0].Name.c_str());

   // Retrieve the most recent font definition and use that as the style that we're going to start with.

   if (Flags & IXF_HOLDSTYLE) {
      // Do nothing to change the style
   }
   else {
      Self->Style = style_status();

      if (Flags & IXF_RESETSTYLE) {
         // Do not search for the most recent font style
      }
      else {
         for (auto i = TargetIndex.Index - 1; i > 0; i--) {
            if (Self->Stream[i].Code IS ESC::FONT) {
               Self->Style.FontStyle = escape_data<bcFont>(Self, i);
               log.trace("Found existing font style, font index %d, flags $%.8x.", Self->Style.FontStyle.FontIndex, Self->Style.FontStyle.Options);
               break;
            }
         }
      }

      // If no style is available, we need to create a default font style and insert it at the start of the stream.

      if (Self->Style.FontStyle.FontIndex IS -1) {
         if ((Self->Style.FontStyle.FontIndex = create_font(Self->FontFace, "Regular", Self->FontSize)) IS -1) {
            if ((Self->Style.FontStyle.FontIndex = create_font("Open Sans", "Regular", 10)) IS -1) {
               return ERR_Failed;
            }
         }

         Self->Style.FontStyle.Fill = Self->FontFill;
         Self->Style.FontChange = true;
      }

      if (auto font = Self->Style.FontStyle.getFont()) {
         Self->Style.Face  = font->Face;
         Self->Style.Point = font->Point;
      }
   }

   // Parse content and insert it at the end of the stream (we will move it to the insertion point afterwards).

   StreamChar inserted_at(Self->Stream.size());
   StreamChar insert_index(Self->Stream.size());
   auto flags = IPF::NIL;
   parse_tag(Self, XML, Tag, insert_index, flags);

   if (Flags & IXF_CLOSESTYLE) style_check(Self, insert_index);

   if (INDEX(Self->Stream.size()) <= inserted_at.Index) {
      log.trace("parse_tag() did not insert any content into the stream.");
      return ERR_NothingDone;
   }

   // Move the content from the end of the stream to the requested insertion point

   if (TargetIndex < inserted_at) {
      auto length = INDEX(Self->Stream.size()) - inserted_at.Index;
      log.trace("Moving new content of %d bytes to the insertion point at index %d", TargetIndex, length);
      Self->Stream.insert(Self->Stream.begin() + TargetIndex.Index, Self->Stream.begin() + inserted_at.Index, Self->Stream.begin() + length);
      Self->Stream.resize(inserted_at.Index + length);
   }

   // Check that the FocusIndex is valid (there's a slim possibility that it may not be if AC_Focus has been
   // incorrectly used).

   if (Self->FocusIndex >= LONG(Self->Tabs.size())) Self->FocusIndex = -1;

   return ERR_Okay;
}

//********************************************************************************************************************
// This is the principal function for the parsing of XML tags.  Insertion into the stream will occur at Index, which
// is updated on completion.
//
// Supported Flags:
//   IPF::NO_CONTENT:
//   IPF::STRIP_FEEDS:

static TRF parse_tag(extDocument *Self, objXML *XML, XMLTag &Tag, StreamChar &Index, IPF &Flags)
{
   pf::Log log(__FUNCTION__);

   if (Self->Error) {
      log.traceWarning("Error field is set, returning immediately.");
      return TRF::NIL;
   }

   IPF filter = Flags & IPF::FILTER_ALL;
   XMLTag *object_template = NULL;

   auto saved_attribs = Tag.Attribs;
   translate_attrib_args(Self, Tag.Attribs);

   auto tagname = Tag.Attribs[0].Name;
   if (tagname.starts_with('$')) tagname.erase(0, 1);
   object_template = NULL;
  
   TRF result = TRF::NIL;
   if (Tag.isContent()) {
      if ((Flags & IPF::NO_CONTENT) IS IPF::NIL) {
         if ((Flags & IPF::STRIP_FEEDS) != IPF::NIL) {
            if (Self->CurrentObject) {
               // Objects do not normally accept document content (user should use <xml>)
               // An exception is made for content that is injected within an object tag.

               if (XML IS Self->InjectXML) {
                  unsigned i = 0;
                  while ((Tag.Attribs[0].Value[i] IS '\n') or (Tag.Attribs[0].Value[i] IS '\r')) i++;
                  acDataContent(Self->CurrentObject, Tag.Attribs[0].Value.c_str() + i);
               }
            }
            else if (Self->ParagraphDepth) { // We must be in a paragraph to accept content as text
               unsigned i = 0;
               while ((Tag.Attribs[0].Value[i] IS '\n') or (Tag.Attribs[0].Value[i] IS '\r')) i++;
               if (i > 0) {
                  auto content = Tag.Attribs[0].Value.substr(i);
                  insert_text(Self, Index, content, ((Self->Style.FontStyle.Options & FSO::PREFORMAT) != FSO::NIL));
               }
               else insert_text(Self, Index, Tag.Attribs[0].Value, ((Self->Style.FontStyle.Options & FSO::PREFORMAT) != FSO::NIL));
            }
            Flags &= ~IPF::STRIP_FEEDS;
         }
         else if (Self->CurrentObject) {
            if (XML IS Self->InjectXML) acDataContent(Self->CurrentObject, Tag.Attribs[0].Value.c_str());
         }
         else if (Self->ParagraphDepth) { // We must be in a paragraph to accept content as text
            insert_text(Self, Index, Tag.Attribs[0].Value, ((Self->Style.FontStyle.Options & FSO::PREFORMAT) != FSO::NIL));
         }
      }
      Tag.Attribs = saved_attribs;
      return result;
   }

   if (Self->Templates) { // Check for templates first, as they can be used to override the default RPL tag names.
      bool template_match = false;
      for (auto &scan : Self->Templates->Tags) {
         for (unsigned i=0; i < scan.Attribs.size(); i++) {
            if ((!StrMatch("name", scan.Attribs[i].Name)) and (!StrMatch(tagname, scan.Attribs[i].Value))) {
               template_match = true;
            }
         }

         if (template_match) {
            // Process the template by jumping into it.  Arguments in the tag are added to a sequential
            // list that will be processed in reverse by translate_attrib_args().

            pf::Log log(__FUNCTION__);

            initTemplate block(Self, Tag.Children, XML); // Required for the <inject/> feature to work inside the template

            log.traceBranch("Executing template '%s'.", tagname.c_str());

            Self->TemplateArgs.push_back(&Tag);
            parse_tags(Self, Self->Templates, scan.Children, Index, Flags);
            Self->TemplateArgs.pop_back();

            Tag.Attribs = saved_attribs;
            return result;
         }
      }
   }

   if (auto tag = glTags.find(tagname); tag != glTags.end()) {
      auto &tr = tag->second;
      if (((tr.Flags & TAG::FILTER_ALL) != TAG::NIL) and ((tr.Flags & TAG(filter)) IS TAG::NIL)) {
         // A filter applies to this tag and the filter flags do not match
         log.warning("Invalid use of tag '%s' - Not applied to the correct tag parent.", tagname.c_str());
         Self->Error = ERR_InvalidData;
      }
      else if (tr.Routine) {
         //log.traceBranch("%s", tagname);

         if ((Self->CurrentObject) and ((tr.Flags & (TAG::OBJECTOK|TAG::CONDITIONAL)) IS TAG::NIL)) {
            log.warning("Illegal use of tag %s within object of class '%s'.", tagname.c_str(), Self->CurrentObject->Class->ClassName);
            result = TRF::BREAK;
         }
         else {

            if (((Flags & IPF::NO_CONTENT) != IPF::NIL) and ((tr.Flags & TAG::CONTENT) != TAG::NIL)) {
               // Do nothing when content is not allowed
               log.trace("Content disabled on '%s', tag not processed.", tagname.c_str());
            }
            else if ((tr.Flags & TAG::CHILDREN) != TAG::NIL) {
               // Child content is compulsory or tag has no effect
               if (!Tag.Children.empty()) tr.Routine(Self, XML, Tag, Tag.Children, Index, Flags);
               else log.trace("No content found in tag '%s'", tagname.c_str());
            }
            else tr.Routine(Self, XML, Tag, Tag.Children, Index, Flags);

         }
      }
   }
   else if (!StrMatch("break", tagname)) {
      // Breaking stops executing all tags (within this section) beyond the breakpoint.  If in a loop, the loop
      // will stop executing.

      result = TRF::BREAK;
   }
   else if (!StrMatch("continue", tagname)) {
      // Continuing - does the same thing as a break but the loop continues.
      // If used when not in a loop, then all sibling tags are skipped.

      result = TRF::CONTINUE;
   }
   else if (!StrMatch("if", tagname)) {
      if (check_tag_conditions(Self, Tag)) { // Statement is true
         Flags &= ~IPF::CHECK_ELSE;
         result = parse_tags(Self, XML, Tag.Children, Index, Flags);
      }
      else Flags |= IPF::CHECK_ELSE;
   }
   else if (!StrMatch("elseif", tagname)) {
      if ((Flags & IPF::CHECK_ELSE) != IPF::NIL) {
         if (check_tag_conditions(Self, Tag)) { // Statement is true
            Flags &= ~IPF::CHECK_ELSE;
            result = parse_tags(Self, XML, Tag.Children, Index, Flags);
         }
      }
   }
   else if (!StrMatch("else", tagname)) {
      if ((Flags & IPF::CHECK_ELSE) != IPF::NIL) {
         Flags &= ~IPF::CHECK_ELSE;
         result = parse_tags(Self, XML, Tag.Children, Index, Flags);
      }
   }
   else if (!StrMatch("while", tagname)) {
      if ((!Tag.Children.empty()) and (check_tag_conditions(Self, Tag))) {
         // Save/restore the statement string on each cycle to fully evaluate the condition each time.

         auto saveindex = Self->LoopIndex;
         Self->LoopIndex = 0;

         bool state = true;
         while (state) {
            state = check_tag_conditions(Self, Tag);
            Tag.Attribs = saved_attribs;
            translate_attrib_args(Self, Tag.Attribs);

            if ((state) and ((parse_tags(Self, XML, Tag.Children, Index, Flags) & TRF::BREAK) != TRF::NIL)) break;

            Self->LoopIndex++;
         }

         Self->LoopIndex = saveindex;
      }
   }
   else if ((Flags & IPF::NO_CONTENT) IS IPF::NIL) {
      log.warning("Tag '%s' unsupported as an instruction or template.", tagname.c_str());
   }
   else log.warning("Unrecognised tag '%s' used in a content-restricted area.", tagname.c_str());

   Tag.Attribs = saved_attribs;
   return result;
}

static TRF parse_tags(extDocument *Self, objXML *XML, objXML::TAGS &Tags, StreamChar &Index, IPF Flags)
{
   TRF result = TRF::NIL;

   for (auto &tag : Tags) {
      // Note that Flags will carry state between multiple calls to parse_tag().  This allows if/else to work correctly.
      result = parse_tag(Self, XML, tag, Index, Flags);
      if ((Self->Error) or ((result & (TRF::CONTINUE|TRF::BREAK)) != TRF::NIL)) break;
   }

   return result;
}

//********************************************************************************************************************
// Check for a pending font and/or style change and respond appropriately.

static void style_check(extDocument *Self, StreamChar &Cursor)
{
   if (Self->Style.FontChange) {
      // Create a new font object for the current style

      auto style_name = get_font_style(Self->Style.FontStyle.Options);
      Self->Style.FontStyle.FontIndex = create_font(Self->Style.Face, style_name, Self->Style.Point);
      //log.trace("Changed font to index %d, face %s, style %s, size %d.", Self->Style.FontStyle.Index, Self->Style.Face, style_name, Self->Style.Point);
      Self->Style.FontChange  = false;
      Self->Style.StyleChange = true;
   }

   if (Self->Style.StyleChange) { // Insert a font change into the text stream
      // NB: Assigning a new UID is suboptimal in cases where we are reverting to a previously registered state
      // (i.e. anywhere where saved_style_check() has been used).  We could allow insertCode() to lookup formerly
      // allocated UID's and save some memory usage if we improved the management of saved styles.
      Self->Style.FontStyle.UID = glByteCodeID++;
      Self->insertCode(Cursor, Self->Style.FontStyle);
      Self->Style.StyleChange = false;
   }
}

//********************************************************************************************************************
// Preformat must be set to true if all consecutive whitespace characters in Text are to be inserted.

static ERROR insert_text(extDocument *Self, StreamChar &Index, const std::string &Text, bool Preformat)
{
   // Check if there is content to be processed

   if ((!Preformat) and (Self->NoWhitespace)) {
      unsigned i;
      for (i=0; i < Text.size(); i++) if (Text[i] > 0x20) break;
      if (i IS Text.size()) return ERR_Okay;
   }

   style_check(Self, Index);

   if (Preformat) {
      bcText et(Text, true);
      Self->insertCode(Index, et);
   }
   else {
      bcText et;
      et.Text.reserve(Text.size());
      auto ws = Self->NoWhitespace;
      for (unsigned i=0; i < Text.size(); ) {
         if (Text[i] <= 0x20) { // Whitespace eliminator
            for (++i; (Text[i] <= 0x20) and (i < Text.size()); i++);
            if (!ws) et.Text += ' ';
            ws = true;
         }
         else {
            et.Text += Text[i++];
            ws = false;
         }
      }
      Self->NoWhitespace = ws;
      Self->insertCode(Index, et);
   }

   return ERR_Okay;
}

//********************************************************************************************************************

static ERROR load_doc(extDocument *Self, std::string Path, bool Unload, ULD UnloadFlags)
{
   pf::Log log(__FUNCTION__);

   log.branch("Loading file '%s', page '%s'", Path.c_str(), Self->PageName.c_str());

   if (Unload) unload_doc(Self, UnloadFlags);

   process_parameters(Self, Path);

   // Generate a path without parameter values.

   auto i = Path.find_first_of("&#?");
   if (i != std::string::npos) Path.erase(i);

   if (!AnalysePath(Path.c_str(), NULL)) {
      auto task = CurrentTask();
      task->setPath(Path);

      if (auto xml = objXML::create::integral(
         fl::Flags(XMF::ALL_CONTENT|XMF::PARSE_HTML|XMF::STRIP_HEADERS|XMF::WELL_FORMED),
         fl::Path(Path), fl::ReadOnly(true))) {

         if (Self->XML) FreeResource(Self->XML);
         Self->XML = xml;

         pf::LogLevel level(3);
         Self->Error = process_page(Self, xml);

         return Self->Error;
      }
      else {
         error_dialog("Document Load Error", std::string("Failed to load document file '") + Path + "'");
         return log.warning(ERR_OpenFile);
      }
   }
   else return log.warning(ERR_FileNotFound);
}

//********************************************************************************************************************
// Converts XML into RIPPLE bytecode, then displays the page that is referenced by the PageName field by calling
// layout_doc().  If the PageName is unspecified, we use the first <page> that has no name, otherwise the first page
// irrespective of the name.
//
// This function does not clear existing data, so you can use it to append new content to existing document content.

static ERROR process_page(extDocument *Self, objXML *xml)
{
   if (!xml) return ERR_NoData;

   pf::Log log(__FUNCTION__);

   log.branch("Page: %s, XML: %d", Self->PageName.c_str(), xml->UID);

   // Look for the first page that matches the requested page name (if a name is specified).  Pages can be located anywhere
   // within the XML source - they don't have to be at the root.

   XMLTag *page = NULL;
   for (auto &scan : xml->Tags) {
      if (StrMatch("page", scan.Attribs[0].Name)) continue;

      if (!page) page = &scan;

      if (Self->PageName.empty()) break;
      else if (auto name = scan.attrib("name")) {
         if (!StrMatch(Self->PageName, *name)) page = &scan;
      }
   }

   Self->Error = ERR_Okay;
   if ((page) and (!page->Children.empty())) {
      Self->PageTag = page;

      bool noheader = false;
      bool nofooter = false;
      if (page->attrib("noheader")) noheader = true;
      if (page->attrib("nofooter")) nofooter = true;

      Self->Segments.clear();
      Self->SortSegments.clear();
      Self->TemplateArgs.clear();
      
      Self->SelectStart.reset();
      Self->SelectEnd.reset();
      Self->XPosition    = 0;
      Self->YPosition    = 0;
      Self->UpdatingLayout = true;
      Self->Error        = ERR_Okay;

      // Process tags at the root level, but only those that we allow up to the first <page> entry.

      pf::Log log(__FUNCTION__);

      log.traceBranch("Processing root level tags.");

      Self->BodyTag   = NULL;
      Self->HeaderTag = NULL;
      Self->FooterTag = NULL;
      for (auto &tag : xml->Tags) {
         if (tag.isContent()) continue;

         switch (StrHash(tag.Attribs[0].Name)) {
            case HASH_body:
               // If a <body> tag contains any children, it is treated as a template and must contain an <inject/> tag so
               // that the XML insertion point is known.

               insert_xml(Self, xml, tag); // Process the body attributes in tag_body() and set BodyTag
               break;
            case HASH_page:       break;
            case HASH_background: insert_xml(Self, xml, tag); break;
            case HASH_editdef:    insert_xml(Self, xml, tag); break;
            case HASH_template:   insert_xml(Self, xml, tag); break;
            case HASH_head:       insert_xml(Self, xml, tag); break;
            case HASH_info:       insert_xml(Self, xml, tag); break;
            case HASH_include:    insert_xml(Self, xml, tag); break;
            case HASH_parse:      insert_xml(Self, xml, tag); break;
            case HASH_script:     insert_xml(Self, xml, tag); break;
            case HASH_header:     Self->HeaderTag = &tag.Children; break;
            case HASH_footer:     Self->FooterTag = &tag.Children; break;
            default: log.warning("Tag '%s' Not supported at the root level.", tag.Attribs[0].Name.c_str());
         }
      }

      if ((Self->HeaderTag) and (!noheader)) {
         pf::Log log(__FUNCTION__);
         log.traceBranch("Processing header.");
         insert_xml(Self, xml, Self->HeaderTag[0][0], Self->Stream.size(), IXF_SIBLINGS|IXF_RESETSTYLE);
      }

      if (Self->BodyTag) {
         pf::Log log(__FUNCTION__);
         log.traceBranch("Processing this page through the body tag.");

         initTemplate block(Self, page->Children, xml);
         insert_xml(Self, xml, Self->BodyTag[0][0], Self->Stream.size(), IXF_SIBLINGS|IXF_RESETSTYLE);
      }
      else {
         pf::Log log(__FUNCTION__);
         auto page_name = page->attrib("name");
         log.traceBranch("Processing page '%s'.", page_name ? page_name->c_str() : "");
         insert_xml(Self, xml, page->Children, Self->Stream.size(), IXF_SIBLINGS|IXF_RESETSTYLE);
      }

      if ((Self->FooterTag) and (!nofooter)) {
         pf::Log log(__FUNCTION__);
         log.traceBranch("Processing footer.");
         insert_xml(Self, xml, Self->FooterTag[0][0], Self->Stream.size(), IXF_SIBLINGS|IXF_RESETSTYLE);
      }

      #ifdef DBG_STREAM
         print_stream(Self, Self->Stream);
      #endif

      // If an error occurred then we have to kill the document as the stream may contain disconnected escape
      // sequences (e.g. an unterminated ESC::TABLE sequence).

      if (Self->Error) unload_doc(Self, ULD::NIL);

      Self->UpdatingLayout = true;
      if (Self->initialised()) redraw(Self, true);

      #ifdef RAW_OUTPUT
         objFile::create file = { fl::Path("drive1:doc-stream.bin"), fl::Flags(FL::NEW|FL::WRITE) };
         file->write(Self->Stream, Self->Stream.size());
      #endif
   }
   else {
      if (!Self->PageName.empty()) {
         auto msg = std::string("Failed to find page '") + Self->PageName + "' in document '" + Self->Path + "'.";
         error_dialog("Load Failed", msg);
      }
      else {
         auto msg = std::string("Failed to find a valid page in document '") + Self->Path > "'.";
         error_dialog("Load Failed", msg);
      }
      Self->Error = ERR_Search;
   }

   if ((!Self->Error) and (Self->MouseInPage)) {
      DOUBLE x, y;
      if (!gfxGetRelativeCursorPos(Self->Page->UID, &x, &y)) {
         check_mouse_pos(Self, x, y);
      }
   }

   if (!Self->PageProcessed) {
      for (auto &trigger : Self->Triggers[LONG(DRT::PAGE_PROCESSED)]) {
         if (trigger.Type IS CALL_SCRIPT) {
            scCallback(trigger.Script.Script, trigger.Script.ProcedureID, NULL, 0, NULL);
         }
         else if (trigger.Type IS CALL_STDC) {
            auto routine = (void (*)(APTR, extDocument *))trigger.StdC.Routine;
            pf::SwitchContext context(trigger.StdC.Context);
            routine(trigger.StdC.Context, Self);
         }
      }
   }

   Self->PageProcessed = true;
   return Self->Error;
}

//********************************************************************************************************************
// This function removes all allocations that were made in displaying the current page, and resets a number of
// variables that they are at the default settings for the next page.
//
// Set Terminate to true only if the document object is being destroyed.
//
// The PageName is not freed because the desired page must not be dropped during refresh of manually loaded XML for
// example.

static ERROR unload_doc(extDocument *Self, ULD Flags)
{
   pf::Log log(__FUNCTION__);

   log.branch("Flags: $%.2x", LONG(Flags));

   #ifdef DBG_STREAM
      print_stream(Self);
   #endif

   log.trace("Resetting variables.");

   if (Self->FontFill) FreeResource(Self->FontFill);
   Self->FontFill     = StrClone("rgb(0,0,0)");

   if (Self->Highlight) FreeResource(Self->Highlight);
   Self->Highlight    = StrClone(glHighlight.c_str());

   if (Self->CursorStroke) FreeResource(Self->CursorStroke);
   Self->CursorStroke = StrClone("rgb(102,102,204,255)");

   if (Self->LinkFill) FreeResource(Self->LinkFill);
   Self->LinkFill     = StrClone("rgb(0,0,255,255)");

   if (Self->Background) FreeResource(Self->Background);
   Self->Background   = StrClone("rgb(255,255,255,255)");

   if (Self->LinkSelectFill) FreeResource(Self->LinkSelectFill);
   Self->LinkSelectFill = StrClone("rgb(255,0,0,255)");

   Self->LeftMargin    = 10;
   Self->RightMargin   = 10;
   Self->TopMargin     = 10;
   Self->BottomMargin  = 10;
   Self->XPosition     = 0;
   Self->YPosition     = 0;
//   Self->ScrollVisible = false;
   Self->PageHeight    = 0;
   Self->Invisible     = 0;
   Self->PageWidth     = 0;
   Self->CalcWidth     = 0;
   Self->LineHeight    = LINE_HEIGHT; // Default line height for measurements concerning the page (can be derived from a font)
   Self->RelPageWidth  = false;
   Self->MinPageWidth  = MIN_PAGE_WIDTH;
   Self->DefaultScript = NULL;
   Self->BkgdGfx       = 0;
   Self->FontSize      = DEFAULT_FONTSIZE;
   Self->FocusIndex    = -1;
   Self->PageProcessed = false;
   Self->MouseOverSegment = -1;
   Self->ActiveEditCellID = 0;
   Self->ActiveEditDef    = NULL;
   Self->SelectIndex.reset();
   Self->CursorIndex.reset();

   if (Self->ActiveEditDef) deactivate_edit(Self, false);

   Self->Links.clear();

   if (Self->LinkIndex != -1) {
      Self->LinkIndex = -1;
      gfxRestoreCursor(PTC::DEFAULT, Self->UID);
   }

   if (Self->FontFace) FreeResource(Self->FontFace);
   if ((Flags & ULD::TERMINATE) != ULD::NIL) Self->FontFace = NULL;
   else Self->FontFace = StrClone("Open Sans");

   Self->PageTag = NULL;

   Self->EditCells.clear();
   Self->Stream.clear();
   Self->SortSegments.clear();
   Self->Segments.clear();
   Self->Params.clear();
   Self->MouseOverChain.clear();
   Self->Tabs.clear();

   for (auto &t : Self->Triggers) t.clear();

   if ((Flags & ULD::TERMINATE) != ULD::NIL) Self->Vars.clear();

   if (Self->Keywords)    { FreeResource(Self->Keywords); Self->Keywords = NULL; }
   if (Self->Author)      { FreeResource(Self->Author); Self->Author = NULL; }
   if (Self->Copyright)   { FreeResource(Self->Copyright); Self->Copyright = NULL; }
   if (Self->Description) { FreeResource(Self->Description); Self->Description = NULL; }
   if (Self->Title)       { FreeResource(Self->Title); Self->Title = NULL; }

   // Free templates only if they have been modified (no longer at the default settings).

   if (Self->Templates) {
      if (Self->TemplatesModified != Self->Templates->Modified) {
         FreeResource(Self->Templates);
         Self->Templates = NULL;
      }
   }

   // Remove all page related resources

   {
      pf::Log log(__FUNCTION__);
      log.traceBranch("Freeing page-allocated resources.");

      for (auto it = Self->Resources.begin(); it != Self->Resources.end(); ) {
         if ((ULD::TERMINATE) != ULD::NIL) it->Terminate = true;

         if ((it->Type IS RT_PERSISTENT_SCRIPT) or (it->Type IS RT_PERSISTENT_OBJECT)) {
            // Persistent objects and scripts will survive refreshes
            if ((Flags & ULD::REFRESH) != ULD::NIL);
            else { it = Self->Resources.erase(it); continue; }
         }
         else { it = Self->Resources.erase(it); continue; }

         it++;
      }
   }

   if (!Self->Templates) {
      if (!(Self->Templates = objXML::create::integral(fl::Name("xmlTemplates"), fl::Statement(glDefaultStyles),
         fl::Flags(XMF::PARSE_HTML|XMF::STRIP_HEADERS)))) return ERR_CreateObject;

      Self->TemplatesModified = Self->Templates->Modified;
   }

   Self->NoWhitespace = true; // Reset whitespace flag

   if (Self->Page) acMoveToPoint(Self->Page, 0, 0, 0, MTF::X|MTF::Y);

   Self->UpdatingLayout = true;
   Self->GeneratedID = AllocateID(IDTYPE::GLOBAL);

   if ((Flags & ULD::REDRAW) != ULD::NIL) {
      Self->Viewport->draw();
   }

   return ERR_Okay;
}

//********************************************************************************************************************

#if 0
static LONG get_line_from_index(extDocument *Self, INDEX Index)
{
   LONG line;
   for (line=1; line < Self->SegCount; line++) {
      if (Index < Self->Segments[line].Index) {
         return line-1;
      }
   }
   return 0;
}
#endif

//********************************************************************************************************************

static void add_template(extDocument *Self, objXML *XML, XMLTag &Tag)
{
   pf::Log log(__FUNCTION__);
   unsigned i;

   // Validate the template (must have a name)

   for (i=1; i < Tag.Attribs.size(); i++) {
      if ((!StrMatch("name", Tag.Attribs[i].Name)) and (!Tag.Attribs[i].Value.empty())) break;
      if ((!StrMatch("class", Tag.Attribs[i].Name)) and (!Tag.Attribs[i].Value.empty())) break;
   }

   if (i >= Tag.Attribs.size()) {
      log.warning("A <template> is missing a name or class attribute.");
      return;
   }

   // Note: It would be nice if we scanned the existing templates and
   // replaced them correctly, however we're going to be lazy and override
   // styles by placing updated definitions at the end of the style list.

   STRING strxml;
   if (!xmlGetString(XML, Tag.ID, XMF::NIL, &strxml)) {
      xmlInsertXML(Self->Templates, 0, XMI::PREV, strxml, 0);
      FreeResource(strxml);
   }
   else log.warning("Failed to convert template %d to an XML string.", Tag.ID);
}

//********************************************************************************************************************

static std::string get_font_style(FSO Options)
{
   if ((Options & (FSO::BOLD|FSO::ITALIC)) IS (FSO::BOLD|FSO::ITALIC)) return "Bold Italic";
   else if ((Options & FSO::BOLD) != FSO::NIL) return "Bold";
   else if ((Options & FSO::ITALIC) != FSO::NIL) return "Italic";
   else return "Regular";
}

//********************************************************************************************************************
// Creates a font (if it doesn't already exist) and returns an index.
//
// Created fonts belong to the Document module rather than the current object, so they can be reused between multiple
// open documents.

static LONG create_font(const std::string &Face, const std::string &Style, LONG Point)
{
   pf::Log log(__FUNCTION__);

   if (Point < 3) Point = DEFAULT_FONTSIZE;

   // Check the cache for this font

   for (unsigned i=0; i < glFonts.size(); i++) {
      if ((!StrMatch(Face, glFonts[i].Font->Face)) and
          (!StrMatch(Style, glFonts[i].Font->Style)) and
          (Point IS glFonts[i].Point)) {
         log.trace("Match %d = %s(%s,%d)", i, Face.c_str(), Style.c_str(), Point);
         return i;
      }
   }

   log.branch("Index: %d, %s, %s, %d", LONG(glFonts.size()), Face.c_str(), Style.c_str(), Point);

   pf::LogLevel level(2);

   objFont *font = objFont::create::integral(
      fl::Owner(modDocument->UID), fl::Face(Face), fl::Style(Style), fl::Point(Point), fl::Flags(FTF::ALLOW_SCALE));

   if (font) {
      // Perform a second check in case the font we ended up with is in our cache.  This can occur if the font we have acquired
      // is a little different to what we requested (e.g. scalable instead of fixed, or a different face).

      for (unsigned i=0; i < glFonts.size(); i++) {
         if ((!StrMatch(font->Face, glFonts[i].Font->Face)) and
            (!StrMatch(font->Style, glFonts[i].Font->Style)) and
            (font->Point IS glFonts[i].Point)) {
            log.trace("Match %d = %s(%s,%d)", i, Face.c_str(), Style.c_str(), Point);
            FreeResource(font);
            return i;
         }
      }

      auto index = glFonts.size();
      glFonts.emplace_back(font, Point);
      return index;
   }
   else {
      return -1;
   }
}

//********************************************************************************************************************
// This function converts arguments such as [@arg] in a string.
//
// Calculations can also be performed, e.g. [=5+7]
//
// The escape code for brackets are &lsqr; and &rsqr; (not in the XML escape code standard and thus are unconverted up
// until this function is reached).
//
// If an attribute name is prefixed with '$' then no translation of the attribute value is attempted.
//
// If a major error occurs during processing, the function will abort, returning the error and also setting the Error
// field to the resulting error code.  The most common reason for an abort is a buffer overflow or memory allocation
// problems, so a complete abort of document processing is advisable.
//
// RESERVED WORDS
//    %index       Current loop index, if within a repeat loop.
//    %id          A unique ID that is regenerated on each document refresh.
//    %self        ID of the document object.
//    %platform    Windows, Linux or Native.
//    %random      Random string of 9 digits.
//    %currentpage Name of the current page.
//    %nextpage    Name of the next page.
//    %prevpage    Name of the previous page.
//    %path        Current working path.
//    %author      Document author.
//    %description Document description.
//    %copyright   Document copyright.
//    %keywords    Document keywords.
//    %title       Document title.
//    %font        Face, point size and style of the current font.
//    %fontface    Face of the current font.
//    %fontcolour  Colour of the current font.
//    %fontsize    Point size of the current font.
//    %lineno      The current 'line' (technically segmented line) in the document.
//    %content     Inject content (same as <inject/> but usable inside tag attributes)
//    %tm-day      The current day (0 - 31)
//    %tm-month    The current month (1 - 12)
//    %tm-year     The current year (2008+)
//    %tm-hour     The current hour (0 - 23)
//    %tm-minute   The current minute (0 - 59)
//    %tm-second   The current second (0 - 59)
//    %viewheight  Height of the document's available viewing area
//    %viewwidth   Width of the the document's available viewing area.

static void translate_args(extDocument *Self, const std::string &Input, std::string &Output)
{
   pf::Log log(__FUNCTION__);

   Output = Input;

   // Do nothing if there are no special references being used

   {
      unsigned i;
      for (i=0; i < Input.size(); i++) {
         if (Input[i] IS '[') break;
         if ((Input[i] IS '&') and ((!StrCompare("&lsqr;", Input.c_str()+i)) or (!StrCompare("&rsqr;", Input.c_str()+i)))) break;
      }
      if (i >= Input.size()) return;
   }

   for (auto pos = signed(Output.size()); pos >= 0; pos--) {
      if (Output[pos] IS '&') {
         if (!StrCompare("&lsqr;", Output.c_str()+pos)) Output.replace(pos, 6, "[");
         else if (!StrCompare("&rsqr;", Output.c_str()+pos)) Output.replace(pos, 6, "]");
      }
      else if (Output[pos] IS '[') {
         if (Output[pos+1] IS '=') { // Perform a calcuation within [= ... ]
            std::string temp;
            temp.reserve(Output.size());
            unsigned j = 0;
            auto end = pos+2;
            while ((end < LONG(Output.size())) and (Output[end] != ']')) {
               if (Output[end] IS '\'') {
                  temp[j++] = '\'';
                  for (++end; (Output[end]) and (Output[end] != '\''); end++) temp[j++] = Output[end];
                  if (Output[end]) temp[j++] = Output[end++];
               }
               else if (Output[end] IS '"') {
                  temp[j++] = '"';
                  for (++end; (Output[end]) and (Output[end] != '"'); end++);
                  if (Output[end]) temp[j++] = Output[end++];
               }
               else temp[j++] = Output[end++];
            }
            if (end < LONG(Output.size())) end++; // Skip ']'
            std::string calcbuffer;
            calc(temp, 0, calcbuffer);
            Output.replace(pos, end-pos, calcbuffer);
         }
         else if (Output[pos+1] IS '%') {
            // Check against reserved keywords

            if (!Output.compare(pos, std::string::npos, "[%index]")) {
               Output.replace(pos, sizeof("[%index]")-1, std::to_string(Self->LoopIndex));
            }
            else if (!Output.compare(pos, std::string::npos, "[%id]")) {
               Output.replace(pos, sizeof("[%id]")-1, std::to_string(Self->GeneratedID));
            }
            else if (!Output.compare(pos, std::string::npos, "[%self]")) {
               Output.replace(pos, sizeof("[%self]")-1, std::to_string(Self->UID));
            }
            else if (!Output.compare(pos, std::string::npos, "[%platform]")) {
               Output.replace(pos, sizeof("[%platform]")-1, GetSystemState()->Platform);
            }
            else if (!Output.compare(pos, std::string::npos, "[%random]")) {
               // Generate a random string of digits
               std::string random;
               random.reserve(10);
               for (unsigned j=0; j < random.size(); j++) random[j] = '0' + (rand() % 10);
               Output.replace(pos, sizeof("[%random]")-1, random);
            }
            else if (!Output.compare(pos, std::string::npos, "[%currentpage]")) {
               if (Self->PageTag) {
                  if (auto page_name = Self->PageTag[0].attrib("name")) {
                     Output.replace(pos, sizeof("[%currentpage]")-1, page_name[0]);
                  }
                  else Output.replace(pos, sizeof("[%currentpage]")-1, "");
               }
               else Output.replace(pos, sizeof("[%currentpage]")-1, "");
            }
            else if (!Output.compare(pos, std::string::npos, "[%nextpage]")) {
               if (Self->PageTag) {
                  auto next = Self->PageTag->attrib("nextpage");
                  Output.replace(pos, sizeof("[%nextpage]")-1, next ? *next : "");
               }
            }
            else if (!Output.compare(pos, std::string::npos, "[%prevpage]")) {
               if (Self->PageTag) {
                  auto next = Self->PageTag->attrib("prevpage");
                  Output.replace(pos, sizeof("[%prevpage]")-1, next ? *next : "");
               }
            }
            else if (!Output.compare(pos, std::string::npos, "[%path]")) {
               CSTRING workingpath = "";
               GET_WorkingPath(Self, &workingpath);
               if (!workingpath) workingpath = "";
               Output.replace(pos, sizeof("[%path]")-1, workingpath);
            }
            else if (!Output.compare(pos, std::string::npos, "[%author]")) {
               Output.replace(pos, sizeof("[%author]")-1, Self->Author);
            }
            else if (!Output.compare(pos, std::string::npos, "[%description]")) {
               Output.replace(pos, sizeof("[%description]")-1, Self->Description);
            }
            else if (!Output.compare(pos, std::string::npos, "[%copyright]")) {
               Output.replace(pos, sizeof("[%copyright]")-1, Self->Copyright);
            }
            else if (!Output.compare(pos, std::string::npos, "[%keywords]")) {
               Output.replace(pos, sizeof("[%keywords]")-1, Self->Keywords);
            }
            else if (!Output.compare(pos, std::string::npos, "[%title]")) {
               Output.replace(pos, sizeof("[%title]")-1, Self->Title);
            }
            else if (!Output.compare(pos, std::string::npos, "[%font]")) {
               if (auto font = Self->Style.FontStyle.getFont()) {
                  Output.replace(pos, sizeof("[%font]")-1, std::string(font->Face) + ":" + std::to_string(font->Point) + ":" + font->Style);
               }
            }
            else if (!Output.compare(pos, std::string::npos, "[%fontface]")) {
               if (auto font = Self->Style.FontStyle.getFont()) {
                  Output.replace(pos, sizeof("[%fontface]")-1, font->Face);
               }
            }
            else if (!Output.compare(pos, std::string::npos, "[%fontcolour]")) {
               if (auto font = Self->Style.FontStyle.getFont()) {
                  char colour[28];
                  snprintf(colour, sizeof(colour), "#%.2x%.2x%.2x%.2x", font->Colour.Red, font->Colour.Green, font->Colour.Blue, font->Colour.Alpha);
                  Output.replace(pos, sizeof("[%fontcolour]")-1, colour);
               }
            }
            else if (!Output.compare(pos, std::string::npos, "[%fontsize]")) {
               if (auto font = Self->Style.FontStyle.getFont()) {
                  auto num = std::to_string(font->Point);
                  Output.replace(pos, sizeof("[%fontsize]")-1, num);
               }
            }
            else if (!Output.compare(pos, std::string::npos, "[%lineno]")) {
               auto num = std::to_string(Self->Segments.size());
               Output.replace(pos, sizeof("[%lineno]")-1, num);
            }
            else if (!Output.compare(pos, std::string::npos, "[%content]")) {
               if ((Self->InTemplate) and (Self->InjectTag)) {
                  std::string content = xmlGetContent(Self->InjectTag[0][0]);
                  Output.replace(pos, sizeof("[%content]")-1, content);

                  //if (!xmlGetString(Self->InjectXML, Self->InjectTag[0][0].ID, XMF::INCLUDE_SIBLINGS, &content)) {
                  //   Output.replace(pos, sizeof("[%content]")-1, content);
                  //   FreeResource(content);
                  //}
               }
            }
            else if (!Output.compare(pos, std::string::npos, "[%tm-day]")) {

            }
            else if (!Output.compare(pos, std::string::npos, "[%tm-month]")) {

            }
            else if (!Output.compare(pos, std::string::npos, "[%tm-year]")) {

            }
            else if (!Output.compare(pos, std::string::npos, "[%tm-hour]")) {

            }
            else if (!Output.compare(pos, std::string::npos, "[%tm-minute]")) {

            }
            else if (!Output.compare(pos, std::string::npos, "[%tm-second]")) {

            }
            else if (!Output.compare(pos, std::string::npos, "[%version]")) {
               Output.replace(pos, sizeof("[%version]")-1, RIPPLE_VERSION);
            }
            else if (!Output.compare(pos, std::string::npos, "[%viewheight]")) {
               Output.replace(pos, sizeof("[%viewheight]")-1, std::to_string(Self->AreaHeight));
            }
            else if (!Output.compare(pos, std::string::npos, "[%viewwidth]")) {
               Output.replace(pos, sizeof("[%viewwidth]")-1, std::to_string(Self->AreaWidth));
            }
         }
         else if (Output[pos+1] IS '@') {
            // Translate argument reference.
            // Valid examples: [@arg] [@arg:defaultvalue] [@arg:"value[]"] [@arg:'value[[]]]']

            char terminator = ']';
            auto end = Output.find_first_of("]:", pos);
            if (end IS std::string::npos) continue; // Not a valid reference

            auto argname = Output.substr(pos, end-pos);

            auto true_end = end;
            if ((Output[end] IS '\'') or (Output[end] IS '"')) {
               terminator = Output[end];
               for (++true_end; (true_end < Output.size()) and (Output[true_end] != '\''); true_end++);
               while ((true_end < Output.size()) and (Output[true_end] != ']')) true_end++;
            }

            bool processed = false;
            for (auto it=Self->TemplateArgs.rbegin(); (!processed) and (it != Self->TemplateArgs.rend()); it++) {
               auto args = *it;
               for (unsigned arg=1; arg < args->Attribs.size(); arg++) {
                  if (StrCompare(args->Attribs[arg].Name, argname)) continue;
                  Output.replace(pos, true_end-pos, args->Attribs[arg].Value);
                  processed = true;
                  break;
               }
            }

            if (processed) continue;

            // Check against global arguments / variables

            if (Self->Vars.contains(argname)) {
               Output.replace(pos, true_end-pos, Self->Vars[argname]);
            }
            else if (Self->Params.contains(argname)) {
               Output.replace(pos, true_end-pos, Self->Params[argname]);
            }
            else if (Output[end] IS ':') { // Resort to the default value
               end++;
               if ((Output[end] IS '\'') or (Output[end] IS '"')) {
                  end++;
                  auto start = end;
                  while ((end < Output.size()) and (Output[end] != terminator)) end++;
                  Output.replace(pos, true_end-pos, Output.substr(start, end));
               }
               else Output.replace(pos, true_end-pos, Output.substr(end, true_end));
            }
            else Output.replace(pos, true_end+1-pos, "");
         }
         else { // Object translation, can be [object] or [object.field]
            // Make sure that there is a closing bracket

            LONG balance = 1;
            unsigned end;
            for (end=pos+1; (end < Output.size()) and (balance > 0); end++) {
               if (Output[end] IS '[') balance++;
               else if (Output[end] IS ']') balance--;
            }

            if (Output[end] != ']') {
               log.warning("Object reference missing square end bracket.");
               break;
            }
            end++;

            auto name = Output.substr(pos+1, Output.find_first_of(".]")-pos);

            // Get the object ID

            OBJECTID objectid = 0;
            if (!name.empty()) {
               if (name == "self") {
                  // [self] can't be used in RIPPLE, because arguments are parsed prior to object
                  // creation.  We print a message to remind the developer of this rather than
                  // failing quietly.

                  log.warning("Self references are not permitted in RIPPLE.");
               }
               else if (name == "owner") {
                  if (Self->CurrentObject) objectid = Self->CurrentObject->UID;
               }
               else if (!FindObject(name.c_str(), 0, FOF::SMART_NAMES, &objectid)) {
                  if ((Self->Flags & DCF::UNRESTRICTED) IS DCF::NIL) {
                     // Only consider objects that are children of the document
                     bool valid = false;
                     for (auto parent_id = GetOwnerID(objectid); parent_id; parent_id = GetOwnerID(parent_id)) {
                        if (parent_id IS Self->UID) {
                           valid = true;
                           break;
                        }
                     }
                     if (!valid) objectid = 0;
                  }
               }

               if (objectid) {
                  if (valid_objectid(Self, objectid)) {
                     auto dot = Output.find('.');
                     if (dot != std::string::npos) { // Object makes a field reference
                        pf::ScopedObjectLock object(objectid, 2000);
                        if (object.granted()) {
                           OBJECTPTR target;
                           auto fieldname = Output.substr(dot+1, end-(dot+1));
                           if (auto classfield = FindField(object.obj, StrHash(fieldname), &target)) {
                              if (classfield->Flags & FD_STRING) {
                                 CSTRING str;
                                 if (!target->get(classfield->FieldID, &str)) Output.replace(pos, end-pos, str);
                                 else Output.replace(pos, end-pos, "");
                              }
                              else {
                                 // Get field as a variable type and manage any buffer overflow (the use of variables
                                 // for extremely big strings is considered rare / poor design).
                                 std::string tbuffer;
                                 tbuffer.reserve(64 * 1024);
                                 while (tbuffer.capacity() < 8 * 1024 * 1024) {
                                    tbuffer[tbuffer.size()-1] = 0;
                                    GetFieldVariable(target, name.c_str(), tbuffer.data(), tbuffer.size());
                                    if (!tbuffer[tbuffer.size()-1]) break;
                                    tbuffer.reserve(tbuffer.capacity() * 2);
                                 }
                              }
                           }
                           else Output.replace(pos, end-pos, "");
                        }
                        else Output.replace(pos, end-pos, "");
                     }
                     else { // Convert the object reference to an ID
                        Output.replace(pos, end-pos, "#" + std::to_string(objectid));
                     }
                  }
                  else log.warning("Access denied to object '%s' #%d", name.c_str(), objectid);
               }
               else log.warning("Object '%s' does not exist.", name.c_str());
            }
         }
      }
   }
}

//********************************************************************************************************************
// Translate all arguments found in a list of XML attributes.

static void translate_attrib_args(extDocument *Self, pf::vector<XMLAttrib> &Attribs)
{
   if (Attribs[0].isContent()) return;

   for (unsigned attrib=1; (attrib < Attribs.size()); attrib++) {
      if (Attribs[attrib].Name.starts_with('$')) continue;

      std::string output;
      translate_args(Self, Attribs[attrib].Value, output);
      Attribs[attrib].Value = output;
   }
}

//********************************************************************************************************************
// Checks if an object reference is a valid member of the document.

static bool valid_object(extDocument *Self, OBJECTPTR Object)
{
   if ((Self->Flags & DCF::UNRESTRICTED) != DCF::NIL) return true;

   auto obj = Object;
   while (obj) {
      if (!obj->OwnerID) return false;
      if (obj->OwnerID < 0) return valid_objectid(Self, obj->UID); // Switch to scanning public objects
      obj = GetObjectPtr(obj->OwnerID);
      if (obj IS Self) return true;
   }
   return false;
}

//********************************************************************************************************************
//Checks if an object reference is a valid member of the document.

static bool valid_objectid(extDocument *Self, OBJECTID ObjectID)
{
   if ((Self->Flags & DCF::UNRESTRICTED) != DCF::NIL) return true;

   while (ObjectID) {
      ObjectID = GetOwnerID(ObjectID);
      if (ObjectID IS Self->UID) return true;
   }
   return false;
}

//********************************************************************************************************************

static LONG getutf8(CSTRING Value, LONG *Unicode)
{
   LONG i, len, code;

   if ((*Value & 0x80) != 0x80) {
      if (Unicode) *Unicode = *Value;
      return 1;
   }
   else if ((*Value & 0xe0) IS 0xc0) {
      len  = 2;
      code = *Value & 0x1f;
   }
   else if ((*Value & 0xf0) IS 0xe0) {
      len  = 3;
      code = *Value & 0x0f;
   }
   else if ((*Value & 0xf8) IS 0xf0) {
      len  = 4;
      code = *Value & 0x07;
   }
   else if ((*Value & 0xfc) IS 0xf8) {
      len  = 5;
      code = *Value & 0x03;
   }
   else if ((*Value & 0xfc) IS 0xfc) {
      len  = 6;
      code = *Value & 0x01;
   }
   else {
      // Unprintable character
      if (Unicode) *Unicode = 0;
      return 1;
   }

   for (i=1; i < len; ++i) {
      if ((Value[i] & 0xc0) != 0x80) code = -1;
      code <<= 6;
      code |= Value[i] & 0x3f;
   }

   if (code IS -1) {
      if (Unicode) *Unicode = 0;
      return 1;
   }
   else {
      if (Unicode) *Unicode = code;
      return len;
   }
}

//********************************************************************************************************************
// Find the nearest font style that will represent Char

static bcFont * find_style(extDocument *Self, const RSTREAM &Stream, StreamChar &Char)
{
   bcFont *style = NULL;

   for (INDEX fi = Char.Index; fi < Char.Index; fi++) {
      if (Stream[fi].Code IS ESC::FONT) style = &escape_data<bcFont>(Self, fi);
      else if (Stream[fi].Code IS ESC::TEXT) break;
   }

   // Didn't work?  Try going backwards

   if (!style) {
      for (INDEX fi = Char.Index; fi >= 0; fi--) {
         if (Stream[fi].Code IS ESC::FONT) {
            style = &escape_data<bcFont>(Self, fi);
            break;
         }
      }
   }

   return style;
}

//********************************************************************************************************************
// For a given line segment, convert a horizontal coordinate to the corresponding character index and its coordinate.

static ERROR resolve_font_pos(extDocument *Self, DocSegment &Segment, LONG X, LONG *CharX, StreamChar &Char)
{
   pf::Log log(__FUNCTION__);

   bcFont *style = find_style(Self, Self->Stream, Char);
   auto font = style ? style->getFont() : glFonts[0].Font;
   if (!font) return ERR_Search;

   for (INDEX i = Segment.Start.Index; i < Segment.Stop.Index; i++) {
      if (Self->Stream[i].Code IS ESC::TEXT) {
         auto &str = escape_data<bcText>(Self, i).Text;
         LONG offset;
         if (!fntConvertCoords(font, str.c_str(), X - Segment.Area.X, 0, NULL, NULL, NULL, &offset, CharX)) {
            Char.set(Segment.Start.Index, offset);
            return ERR_Okay;
         }
         else break;
      }
   }

  log.trace("Failed to convert coordinate %d to a font-relative cursor position.", X);
   return ERR_Failed;
}

//********************************************************************************************************************
// Using only a stream index, this function will determine the X coordinate of the character at that index.  This is
// slower than resolve_font_pos(), because the segment has to be resolved by this function.

static ERROR resolve_fontx_by_index(extDocument *Self, StreamChar Char, LONG *CharX)
{
   pf::Log log("resolve_fontx");

   log.branch("Index: %d", Char.Index);

   bcFont *style = find_style(Self, Self->Stream, Char);   
   auto font = style ? style->getFont() : glFonts[0].Font;
   if (!font) return log.warning(ERR_Search);

   // Find the segment linked to this character.  This is so that we can derive an X coordinate for the character
   // string.

   if (SEGINDEX segment = find_segment(Self, Char, true); segment >= 0) {
      auto i = Self->Segments[segment].Start;
      while ((i <= Self->Segments[segment].Stop) and (i < Char)) {
         if (Self->Stream[i.Index].Code IS ESC::TEXT) {
            *CharX = fntStringWidth(font, escape_data<bcText>(Self, i).Text.c_str(), -1);
            return ERR_Okay;
         }
         i.nextCode();
      }
   }

   log.warning("Failed to find a segment for index %d.", Char.Index);
   return ERR_Search;
}

//********************************************************************************************************************
// For a given character in the stream, find its representative line segment.

static SEGINDEX find_segment(extDocument *Self, StreamChar Char, bool InclusiveStop)
{
   if (InclusiveStop) {
      for (SEGINDEX segment=0; segment < SEGINDEX(Self->Segments.size()); segment++) {
         if ((Char >= Self->Segments[segment].Start) and (Char <= Self->Segments[segment].Stop)) {
            if ((Char IS Self->Segments[segment].Stop) and (Char.getPrevChar(Self, Self->Stream) IS '\n'));
            else return segment;
         }
      }
   }
   else {
      for (SEGINDEX segment=0; segment < SEGINDEX(Self->Segments.size()); segment++) {
         if ((Char >= Self->Segments[segment].Start) and (Char < Self->Segments[segment].Stop)) {
            return segment;
         }
      }
   }

   return -1;
}

//********************************************************************************************************************
// scheme://domain.com/path?param1=value&param2=value#fragment:bookmark

static void process_parameters(extDocument *Self, const std::string &String)
{
   pf::Log log(__FUNCTION__);

   log.branch("%s", String.c_str());

   Self->Params.clear();
   Self->PageName.clear();
   Self->Bookmark.clear();

   std::string arg, value;
   arg.reserve(64);
   value.reserve(0xffff);

   bool pagename_processed = false;
   for (unsigned pos=0; pos < String.size(); pos++) {
      if ((String[pos] IS '#') and (!pagename_processed)) {
         // Reference is '#fragment:bookmark' where 'fragment' refers to a page in the loaded XML file and 'bookmark'
         // is an optional bookmark reference within that page.

         pagename_processed = true;

         if (auto ind = String.find(":", pos+1); ind != std::string::npos) {
            ind -= pos;
            Self->PageName.assign(String, pos + 1);
            if (Self->PageName[ind] IS ':') { // Check for bookmark separator
               Self->PageName.resize(ind);
               ind++;

               Self->Bookmark.assign(Self->PageName, ind);
               if (ind = Self->Bookmark.find('?'); ind != std::string::npos) {
                  Self->Bookmark.resize(ind);
               }
            }
         }
         else Self->PageName.assign(String, pos + 1);

         break;
      }
      else if (String[pos] IS '?') {
         // Arguments follow, separated by & characters for separation
         // Please note that it is okay to set zero-length parameter values

         pos++;

         auto uri_char = [&](std::string &Output) {
            if ((String[pos] IS '%') and
                (((String[pos+1] >= '0') and (String[pos+1] <= '9')) or
                 ((String[pos+1] >= 'A') and (String[pos+1] <= 'F')) or
                 ((String[pos+1] >= 'a') and (String[pos+1] <= 'f'))) and
                (((String[pos+2] >= '0') and (String[pos+2] <= '9')) or
                 ((String[pos+2] >= 'A') and (String[pos+2] <= 'F')) or
                 ((String[pos+2] >= 'a') and (String[pos+2] <= 'f')))) {
               Output += std::stoi(String.substr(pos+1, 2), nullptr, 16);
               pos += 3;
            }
            else Output += String[pos++];
         };

         while (pos < String.size()) {
            arg.clear();

            // Extract the parameter name

            while ((pos < String.size()) and (String[pos] != '#') and (String[pos] != '&') and (String[pos] != ';') and (String[pos] != '=')) {
               uri_char(arg);
            }

            if (String[pos] IS '=') { // Extract the parameter value
               value.clear();
               pos++;
               while ((pos < String.size()) and (String[pos] != '#') and (String[pos] != '&') and (String[pos] != ';')) {
                  uri_char(value);
               }
               Self->Params[arg] = value;
            }
            else Self->Params[arg] = "1";

            while ((String[pos]) and (String[pos] != '#') and (String[pos] != '&') and (String[pos] != ';')) pos++;
            if ((String[pos] != '&') and (String[pos] != ';')) break;
            pos++;
         }
      }
   }

   log.msg("Reset page name to '%s', bookmark '%s'", Self->PageName.c_str(), Self->Bookmark.c_str());
}

//********************************************************************************************************************
// Resolves function references.
// E.g. "script.function(Args...)"; "function(Args...)"; "function()", "function", "script.function"

static ERROR extract_script(extDocument *Self, const std::string &Link, OBJECTPTR *Script, std::string &Function, std::string &Args)
{
   pf::Log log(__FUNCTION__);

   if (Script) {
      if (!(*Script = Self->DefaultScript)) {
         if (!(*Script = Self->UserDefaultScript)) {
            log.warning("Cannot call function '%s', no default script in document.", Link.c_str());
            return ERR_Search;
         }
      }
   }

   auto pos = std::string::npos;
   auto dot = Link.find('.');
   auto open_bracket = Link.find('(');

   if (dot != std::string::npos) { // A script name is referenced
      pos = dot + 1;
      if (Script) {
         std::string script_name;
         script_name.assign(Link, 0, dot);
         OBJECTID id;
         if (!FindObject(script_name.c_str(), ID_SCRIPT, FOF::NIL, &id)) {
            // Security checks
            *Script = GetObjectPtr(id);
            if ((Script[0]->OwnerID != Self->UID) and ((Self->Flags & DCF::UNRESTRICTED) IS DCF::NIL)) {
               log.warning("Script '%s' does not belong to this document.  Request ignored due to security restrictions.", script_name.c_str());
               return ERR_NoPermission;
            }
         }
         else {
            log.warning("Unable to find '%s'", script_name.c_str());
            return ERR_Search;
         }
      }
   }
   else pos = 0;

   if ((open_bracket != std::string::npos) and (open_bracket < dot)) {
      log.warning("Malformed function reference: %s", Link.c_str());
      return ERR_InvalidData;
   }

   if (open_bracket != std::string::npos) {
      Function.assign(Link, pos, open_bracket-pos);
      if (auto end_bracket = Link.find_last_of(')'); end_bracket != std::string::npos) {
         Args.assign(Link, open_bracket + 1, end_bracket-1);
      }
      else log.warning("Malformed function args: %s", Link.c_str());
   }
   else Function.assign(Link, pos);

   return ERR_Okay;
}

//********************************************************************************************************************

void DocLink::exec(extDocument *Self)
{
   pf::Log log(__FUNCTION__);

   log.branch("");

   Self->Processing++;

   if ((BaseCode IS ESC::LINK) and ((Self->EventMask & DEF::LINK_ACTIVATED) != DEF::NIL)) {
      deLinkActivated params;
      auto link = asLink();

      if (link->Type IS LINK::FUNCTION) {
         std::string function_name, args;
         if (!extract_script(Self, link->Ref, NULL, function_name, args)) {
            params.Values["onclick"] = function_name;
         }
      }
      else if (link->Type IS LINK::HREF) {
         params.Values["href"] = link->Ref;
      }

      if (!link->Args.empty()) {
         for (unsigned i=0; i < link->Args.size(); i++) {
            params.Values[link->Args[i].first] = link->Args[i].second;
         }
      }

      ERROR result = report_event(Self, DEF::LINK_ACTIVATED, &params, "deLinkActivated:Parameters");
      if (result IS ERR_Skip) goto end;
   }

   if (BaseCode IS ESC::LINK) {
      OBJECTPTR script;
      std::string function_name, fargs;
      CLASSID class_id, subclass_id;

      auto link = asLink();
      if (link->Type IS LINK::FUNCTION) { // Function is in the format 'function()' or 'script.function()'
         if (!extract_script(Self, link->Ref, &script, function_name, fargs)) {
            std::vector<ScriptArg> args;

            if (!link->Args.empty()) {
               for (auto &arg : link->Args) {
                  if (arg.first.starts_with('_')) { // Global variable setting
                     acSetVar(script, arg.first.c_str()+1, arg.second.c_str());
                  }
                  else args.emplace_back("", arg.second.data());
               }
            }

            scExec(script, function_name.c_str(), args.data(), args.size());
         }
      }
      else if (link->Type IS LINK::HREF) {
         if (link->Ref[0] IS ':') {
            Self->Bookmark = link->Ref.substr(1);
            show_bookmark(Self, Self->Bookmark);
         }
         else {
            if ((link->Ref[0] IS '#') or (link->Ref[0] IS '?')) {
               log.trace("Switching to page '%s'", link->Ref.c_str());

               if (!Self->Path.empty()) {
                  LONG end;
                  for (end=0; Self->Path[end]; end++) {
                     if ((Self->Path[end] IS '&') or (Self->Path[end] IS '#') or (Self->Path[end] IS '?')) break;
                  }
                  auto path = std::string(Self->Path, end) + link->Ref;
                  Self->set(FID_Path, path);
               }
               else Self->set(FID_Path, link->Ref);

               if (!Self->Bookmark.empty()) show_bookmark(Self, Self->Bookmark);
            }
            else {
               log.trace("Link is a file reference.");

               std::string path;

               if (!Self->Path.empty()) {
                  auto j = link->Ref.find_first_of("/\\:");
                  if ((j IS std::string::npos) or (link->Ref[j] != ':')) {
                     auto end = Self->Path.find_first_of("&#?");
                     if (end IS std::string::npos) path.assign(Self->Path);
                     else path.assign(Self->Path, 0, Self->Path.find_last_of("/\\", end) + 1);
                  }
               }

               auto lk = path + link->Ref;
               auto end = lk.find_first_of("?#&");
               if (!IdentifyFile(lk.substr(0, end).c_str(), &class_id, &subclass_id)) {
                  if (class_id IS ID_DOCUMENT) {
                     Self->set(FID_Path, lk);

                     if (!Self->Bookmark.empty()) show_bookmark(Self, Self->Bookmark);
                     else log.msg("No bookmark was preset.");
                  }
               }
               else {
                  auto msg = std::string("It is not possible to follow this link as the type of file is not recognised.  The referenced link is:\n\n") + lk;
                  error_dialog("Action Cancelled", msg);
               }
            }
         }
      }
   }
   else if (BaseCode IS ESC::CELL) {
      OBJECTPTR script;
      std::string function_name, script_args;

      auto cell = asCell();
      if (!extract_script(Self, cell->OnClick, &script, function_name, script_args)) {
         std::vector<ScriptArg> args;
         if (!cell->Args.empty()) {
            for (auto &cell_arg : cell->Args) {
               if (cell_arg.first.starts_with('_')) { // Global variable setting
                  acSetVar(script, cell_arg.first.c_str()+1, cell_arg.second.c_str());
               }
               else args.emplace_back("", cell_arg.second);
            }
         }

         scExec(script, function_name.c_str(), args.data(), args.size());
      }
   }
   else log.trace("Link index does not refer to a supported link type.");

end:
   Self->Processing--;
}

//********************************************************************************************************************

static void show_bookmark(extDocument *Self, const std::string &Bookmark)
{
   pf::Log log(__FUNCTION__);

   log.branch("%s", Bookmark.c_str());

   // Find the indexes for the bookmark name

   LONG start, end;
   if (!docFindIndex(Self, Bookmark.c_str(), &start, &end)) {
      // Get the vertical position of the index and scroll to it

      auto &esc_index = escape_data<bcIndex>(Self, start);
      Self->scrollToPoint(0, esc_index.Y - 4, 0, STP::Y);
   }
   else log.warning("Failed to find bookmark '%s'", Bookmark.c_str());
}

//********************************************************************************************************************

static ERROR report_event(extDocument *Self, DEF Event, APTR EventData, CSTRING StructName)
{
   pf::Log log(__FUNCTION__);
   ERROR result = ERR_Okay;

   if ((Event & Self->EventMask) != DEF::NIL) {
      log.branch("Reporting event $%.8x", LONG(Event));

      if (Self->EventCallback.Type IS CALL_STDC) {
         auto routine = (ERROR (*)(extDocument *, LARGE, APTR))Self->EventCallback.StdC.Routine;
         pf::SwitchContext context(Self->EventCallback.StdC.Context);
         result = routine(Self, LARGE(Event), EventData);
      }
      else if (Self->EventCallback.Type IS CALL_SCRIPT) {
         if (auto script = Self->EventCallback.Script.Script) {
            if ((EventData) and (StructName)) {
               ScriptArg args[3] = {
                  ScriptArg("Document", Self, FD_OBJECTPTR),
                  ScriptArg("EventMask", LARGE(Event)),
                  ScriptArg(StructName, EventData, FD_STRUCT)
               };
               scCallback(script, Self->EventCallback.Script.ProcedureID, args, 3, &result);
            }
            else {
               ScriptArg args[2] = {
                  ScriptArg("Document", Self, FD_OBJECTPTR),
                  ScriptArg("EventMask", LARGE(Event))
               };
               scCallback(script, Self->EventCallback.Script.ProcedureID, args, 2, &result);
            }
         }
      }
   }
   else log.trace("No subscriber for event $%.8x", (LONG)Event);

   return result;
}
