
#define MAXLOOP 100000

// This is a list of the default class types that may be used in document pages.  Its purpose is to restrict the types
// of objects that can be used so that we don't run into major security problems.  Basically, if an instantiated
// object could have the potential to run any program that the user has access to, or if it could gain access to local
// information and use it for nefarious purposes, then it's not secure enough for document usage.
//
// TODO: NEEDS TO BE REPLACED WITH AN XML DEFINITION and PARSED INTO A KEY VALUE STORE.

struct DocClass {
   std::string ClassName;
   CLASSID ClassID;
   std::string PageTarget;
   std::string Fields;

   DocClass(const std::string pName, CLASSID pClassID, const std::string pTarget, const std::string pFields) :
      ClassName(pName), ClassID(pClassID), PageTarget(pTarget), Fields(pFields) { }
};

static const DocClass glDocClasses[] = {
   { "vector",     ID_VECTOR,    "surface", "" },
   { "document",   ID_DOCUMENT,  "surface", "" },
   { "scintilla",  ID_SCINTILLA, "", "" },
   { "http",       ID_HTTP,      "", "" },
   { "config",     ID_CONFIG,    "", "" },
   { "xml",        ID_XML,       "", "" }
};

static const char glDefaultStyles[] =
"<template name=\"h1\"><p leading=\"2.0\"><font face=\"Open Sans\" size=\"18\" colour=\"0,0,0\" style=\"bold\"><inject/></font></p></template>\n\
<template name=\"h2\"><p leading=\"2.0\"><font face=\"Open Sans\" size=\"16\" colour=\"0,0,0\" style=\"bold\"><inject/></font></p></template>\n\
<template name=\"h3\"><p leading=\"1.5\"><font face=\"Open Sans\" size=\"14\" colour=\"0,0,0\" style=\"bold\"><inject/></font></p></template>\n\
<template name=\"h4\"><p leading=\"1.5\"><font face=\"Open Sans\" size=\"14\" colour=\"0,0,0\"><inject/></font></p></template>\n\
<template name=\"h5\"><p leading=\"1.25\"><font face=\"Open Sans\" size=\"12\" colour=\"0,0,0\"><inject/></font></p></template>\n\
<template name=\"h6\"><p leading=\"1.25\"><font face=\"Open Sans\" size=\"10\" colour=\"0,0,0\"><inject/></font></p></template>\n";

//********************************************************************************************************************

static std::string printable(extDocument *, LONG, LONG = 60) __attribute__ ((unused));

static std::string printable(extDocument *Self, LONG Offset, LONG Length)
{
   std::string result;
   result.reserve(80);
   unsigned i = Offset, j = 0;
   while ((i < Self->Stream.size()) and (i < Offset+Length) and (j < result.capacity())) {
      if (Self->Stream[i] IS CTRL_CODE) {
         result[j++] = '%';
         i += ESCAPE_LEN;
      }
      else if (Self->Stream[i] < 0x20) {
         result[j++] = '?';
         i++;
      }
      else result[j++] = Self->Stream[i++];
   }
   result.resize(j);
   return result;
}

//********************************************************************************************************************

static void print_xmltree(objXML::TAGS &Tags, LONG &Indent)
{
   pf::Log log(__FUNCTION__);
   std::ostringstream buffer;

   for (auto &tag : Tags) {
      for (LONG i=0; i < Indent; i++) buffer << ' ';

      if (!tag.isContent()) buffer << tag.Attribs[0].Name;      
      else buffer << '[' << tag.Attribs[0].Value << ']';      

      log.msg("%s", buffer.str().c_str());

      Indent++;
      print_xmltree(tag.Children, Indent);
      Indent--;
   }
}

//********************************************************************************************************************

#ifdef DBG_STREAM

#include <stdio.h>
#include <stdarg.h>
#undef NULL
#define NULL 0

static void print_stream(extDocument *Self, STRING Stream)
{
   STRING str;
   escFont *style;
   LONG i, len, code;
   BYTE printpos;

   if ((!(str = Stream)) or (!str[0])) return;

   fprintf(stderr, "\nSTREAM: %d bytes\n------\n", Self->Stream.size());
   i = 0;
   printpos = false;
   while (str[i]) {
      if (str[i] IS CTRL_CODE) {
         code = ESCAPE_CODE(str, i);
         fprintf(stderr, "(%d)", i);
         if (code IS ESC_FONT) {
            style = escape_data<escFont>(str, i);
            fprintf(stderr, "[E:Font:%d", style->Index);
            if ((style->Options & FSO::ALIGN_RIGHT) != FSO::NIL) fprintf(stderr, ":A/R");
            if ((style->Options & FSO::ALIGN_CENTER) != FSO::NIL) fprintf(stderr, ":A/C");
            if ((style->Options & FSO::BOLD) != FSO::NIL) fprintf(stderr, ":Bold");
            fprintf(stderr, ":#%.2x%.2x%.2x%.2x", style->Colour.Red, style->Colour.Green, style->Colour.Blue, style->Colour.Alpha);
            fprintf(stderr, "]");
         }
         else if (code IS ESC_PARAGRAPH_START) {
            auto para = escape_data<escParagraph>(str, i);
            if (para->ListItem) fprintf(stderr, "[E:LI]");
            else fprintf(stderr, "[E:PS]");
         }
         else if (code IS ESC_PARAGRAPH_END) {
            fprintf(stderr, "[E:PE]\n");
         }
/*
         else if (ESCAPE_CODE(str, i) IS ESC_LIST_ITEM) {
            auto item = escape_data<escItem>(str, i);
            fprintf(stderr, "[I:X(%d):Width(%d):Custom(%d)]", item->X, item->Width, item->CustomWidth);
         }
*/
         else if (code < ARRAYSIZE(strCodes)) {
            fprintf(stderr, "[E:%s]", strCodes[code]);
         }
         else fprintf(stderr, "[E:%d]", code);
         i += ESCAPE_LEN;
         printpos = true;
      }
      else {
         if (printpos) {
            printpos = false;
            fprintf(stderr, "(%d)", i);
         }
         if ((str[i] <= 0x20) or (str[i] > 127)) fprintf(stderr, ".");
         else fprintf(stderr, "%c", str[i]);
         i++;
      }
   }

   fprintf(stderr, "\nActive Edit: %d, Cursor Index: %d / X: %d, Select Index: %d\n", Self->ActiveEditCellID, Self->CursorIndex, Self->CursorCharX, Self->SelectIndex);
}

#endif

#ifdef DBG_LINES

#include <stdio.h>
#include <stdarg.h>
#undef NULL
#define NULL 0

static void print_lines(extDocument *Self)
{
   fprintf(stderr, "\nSEGMENTS\n--------\n");

   STRING str = Self->Stream;
   for (LONG row=0; row < Self->Segments.size(); row++) {
      DocSegment *line = Self->Segments + row;
      LONG i = line->Index;

      fprintf(stderr, "Seg %d, Bytes %d-%d: %dx%d,%dx%d: ", row, line->Index, line->Stop, line->X, line->Y, line->Width, line->Height);
      if (line->Edit) fprintf(stderr, "{ ");
      fprintf(stderr, "\"");
      while (i < line->Stop) {
         if (str[i] IS CTRL_CODE) {
            if (ESCAPE_CODE(str, i) IS ESC_FONT) {
               auto style = escape_data<escFont>(str, i);
               fprintf(stderr, "[E:Font:%d:$%.2x%.2x%.2x", style->Index, style->Colour.Red, style->Colour.Green, style->Colour.Blue);
               fprintf(stderr, "]");
            }
            else if (ESCAPE_CODE(str, i) IS ESC_PARAGRAPH_START) {
               para = escape_data<escParagraph>(str, i);
               if (para->ListItem) fprintf(stderr, "[E:LI]");
               else fprintf(stderr, "[E:PS]");
            }
            else if (ESCAPE_CODE(str, i) IS ESC_PARAGRAPH_END) {
               fprintf(stderr, "[E:PE]\n");
            }
            else if (ESCAPE_CODE(str, i) IS ESC_OBJECT) {
               auto obj = escape_data<escObject>(str, i);
               fprintf(stderr, "[E:OBJ:%d]", obj->ObjectID);
            }
            else if (ESCAPE_CODE(str, i) < ARRAYSIZE(strCodes)) {
               fprintf(stderr, "[E:%s]", strCodes[(UBYTE)ESCAPE_CODE(str, i)]);
            }
            else fprintf(stderr, "[E:%d]", ESCAPE_CODE(str, i));
            i += ESCAPE_LEN;
         }
         else {
            if ((str[i] <= 0x20) or (str[i] > 127)) fprintf(stderr, ".");
            else fprintf(stderr, "%c", str[i]);
            i++;
         }
      }

      fprintf(stderr, "\"");
      if (line->Edit) fprintf(stderr, " }");
      fprintf(stderr, "\n");
   }
}

static void print_sorted_lines(extDocument *Self)
{
   fprintf(stderr, "\nSORTED SEGMENTS\n---------------\n");

   STRING str = Self->Stream;
   for (LONG row=0; row < Self->SortSegments.size(); row++) {
      DocSegment *line = Self->Segments + Self->SortSegments[row].Segment;
      fprintf(stderr, "%d: Y: %d-%d, Seg: %d \"", row, Self->SortSegments[row].Y, Self->Segments[Self->SortSegments[row].Segment].X, Self->SortSegments[row].Segment);

      LONG i = line->Index;
      while (i < line->Stop) {
         if (str[i] IS CTRL_CODE) {
            if (ESCAPE_CODE(str, i) IS ESC_FONT) {
               auto style = escape_data<escFont>(str, i);
               fprintf(stderr, "[E:Font:%d:$%.2x%.2x%.2x", style->Index, style->Colour.Red, style->Colour.Green, style->Colour.Blue);
               fprintf(stderr, "]");
            }
            else if (ESCAPE_CODE(str, i) IS ESC_PARAGRAPH_START) {
               para = escape_data<escParagraph>(str, i);
               if (para->ListItem) fprintf(stderr, "[E:LI]");
               else fprintf(stderr, "[E:PS]");
            }
            else if (ESCAPE_CODE(str, i) IS ESC_PARAGRAPH_END) {
               fprintf(stderr, "[E:PE]\n");
            }
            else if (ESCAPE_CODE(str, i) IS ESC_OBJECT) {
               obj = escape_data<escObject>(str, i);
               fprintf(stderr, "[E:OBJ:%d]", obj->ObjectID);
            }
            else if (ESCAPE_CODE(str, i) < ARRAYSIZE(strCodes)) {
               fprintf(stderr, "[E:%s]", strCodes[(UBYTE)ESCAPE_CODE(str, i)]);
            }
            else fprintf(stderr, "[E:%d]", ESCAPE_CODE(str, i));
            i += ESCAPE_LEN;
         }
         else {
            if ((str[i] <= 0x20) or (str[i] > 127)) fprintf(stderr, ".");
            else fprintf(stderr, "%c", str[i]);
            i++;
         }
      }

      fprintf(stderr, "\"\n");
   }
}

static void print_tabfocus(extDocument *Self)
{
   if (!Self->Tabs.empty()) {
      fprintf(stderr, "\nTAB FOCUSLIST\n-------------\n");

      for (LONG i=0; i < Self->Tabs.size(); i++) {
         fprintf(stderr, "%d: Type: %d, Ref: %d, XRef: %d\n", i, Self->Tabs[i].Type, Self->Tabs[i].Ref, Self->Tabs[i].XRef);
      }
   }
}

#endif

//static BYTE glWhitespace = true;  // Setting this to true tells the parser to ignore whitespace (prevents whitespace being used as content)

// RESET_SEGMENT: Resets the string management variables, usually done when a string
// has been broken up on the current line due to an object or table graphic for example.

#define RESET_SEGMENT(index,x,l) \
   (l)->line_index   = (index); \
   (l)->line_x       = (x); \
   (l)->kernchar     = 0; \
   (l)->textcontent  = 0;

#define RESET_SEGMENT_WORD(index,x,l) \
   (l)->line_index   = (index); \
   (l)->line_x       = (x); \
   (l)->kernchar     = 0; \
   (l)->wordindex    = -1; \
   (l)->wordwidth    = 0; \
   (l)->textcontent  = 0;

struct layout {
   objFont *font;
   escLink *link;
   LONG alignwidth;
   LONG base_line;         // The complete height of the line, covers the height of all objects and tables anchored to the line.  Text is drawn so that the text gutter is aligned to the base line
   LONG line_height;       // Height of the line with respect to the text
   LONG paragraph_end;
   LONG cursorx, cursory;
   LONG line_index;
   LONG line_x;
   LONG left_margin;
   LONG link_x;
   LONG link_index;
   ALIGN link_align;
   LONG kernchar;
   LONG right_margin;
   LONG split_start;
   LONG start_clips;
   LONG wrapedge;
   LONG wordindex;
   LONG wordwidth;
   LONG line_increase;
   LONG paragraph_y;
   LONG alignflags;
   WORD spacewidth;
   WORD len;
   UBYTE anchor;
   bool nowrap;
   bool link_open;
   bool setsegment;
   bool textcontent;
};

enum {
   WRAP_DONOTHING=0,
   WRAP_EXTENDPAGE,
   WRAP_WRAPPED
};

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

static void check_clips(extDocument *Self, LONG Index, layout *l,
   LONG ObjectIndex, LONG *ObjectX, LONG *ObjectY, LONG ObjectWidth, LONG ObjectHeight);

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

static STRING stream_to_string(extDocument *Self, LONG Start, LONG End)
{
   if (End < Start) std::swap(Start, End);

   STRING str;
   if (!AllocMemory(End - Start + 1, MEM::STRING|MEM::NO_CLEAR, &str)) {
      LONG i = Start;
      LONG pos = 0;
      while (i < LONG(Self->Stream.size())) {
         if (Self->Stream[i] != CTRL_CODE) str[pos++] = Self->Stream[i];
         NEXT_CHAR(Self->Stream, i);
      }            
      str[pos] = 0;
      return str;
   }
   else return NULL;
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

inline BYTE sortseg_compare(extDocument *Self, SortSegment &Left, SortSegment &Right)
{
   if (Left.Y < Right.Y) return 1;
   else if (Left.Y > Right.Y) return -1;
   else {
      if (Self->Segments[Left.Segment].X < Self->Segments[Right.Segment].X) return 1;
      else if (Self->Segments[Left.Segment].X > Self->Segments[Right.Segment].X) return -1;
      else return 0;
   }
}

//********************************************************************************************************************

static ERROR consume_input_events(const InputEvent *Events, LONG Handle)
{
   auto Self = (extDocument *)CurrentContext();

   for (auto input=Events; input; input=input->Next) {
      if ((input->Flags & JTYPE::MOVEMENT) != JTYPE::NIL) {
         for (auto scan=input->Next; (scan) and ((scan->Flags & JTYPE::MOVEMENT) != JTYPE::NIL); scan=scan->Next) {
            input = scan;
         }

         if (input->OverID IS Self->PageID) Self->MouseOver = true;
         else Self->MouseOver = false;

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

static ERROR insert_xml(extDocument *Self, objXML *XML, objXML::TAGS &Tag, LONG TargetIndex, UBYTE Flags)
{
   pf::Log log(__FUNCTION__);

   if (TargetIndex < 0) TargetIndex = Self->Stream.size();

   log.traceBranch("Index: %d, Flags: $%.2x, Tag: %s", TargetIndex, Flags, Tag[0].Attribs[0].Name.c_str());

   // Retrieve the most recent font definition and use that as the style that we're going to start with.

   if (Flags & IXF_HOLDSTYLE) {
      // Do nothing to change the style
   }
   else {
      Self->Style.clear();

      if (Flags & IXF_RESETSTYLE) {
         // Do not search for the most recent font style
      }
      else {
         auto str = Self->Stream;
         auto i   = TargetIndex;
         PREV_CHAR(str, i);
         while (i > 0) {
            if ((str[i] IS CTRL_CODE) and (ESCAPE_CODE(str, i) IS ESC_FONT)) {
               Self->Style.FontStyle = escape_data<escFont>(Self, i);
               log.trace("Found existing font style, font index %d, flags $%.8x.", Self->Style.FontStyle.Index, Self->Style.FontStyle.Options);
               break;
            }
            PREV_CHAR(str, i);
         }
      }

      // If no style is available, we need to create a default font style and insert it at the start of the stream.

      if (Self->Style.FontStyle.Index IS -1) {
         if ((Self->Style.FontStyle.Index = create_font(Self->FontFace, "Regular", Self->FontSize)) IS -1) {
            if ((Self->Style.FontStyle.Index = create_font("Open Sans", "Regular", 10)) IS -1) {
               return ERR_Failed;
            }
         }

         Self->Style.FontStyle.Colour = Self->FontColour;
         Self->Style.FontChange = true;
      }

      if (auto font = lookup_font(Self->Style.FontStyle.Index, "insert_xml")) {
         Self->Style.Face  = font->Face;
         Self->Style.Point = font->Point;
      }
   }

   // Parse content and insert it at the end of the stream (we will move it to the insertion point afterwards).

   LONG inserted_at = Self->Stream.size();
   LONG insert_index = Self->Stream.size();
   if (Flags & IXF_SIBLINGS) { // Siblings of Tag are included
      parse_tags(Self, XML, Tag, insert_index);
   }
   else { // Siblings of Tag are not included
      auto parse_flags = IPF::NIL;
      parse_tag(Self, XML, Tag[0], insert_index, parse_flags);
   }

   if (Flags & IXF_CLOSESTYLE) style_check(Self, insert_index);

   if (LONG(Self->Stream.size()) <= inserted_at) {
      log.trace("parse_tag() did not insert any content into the stream.");
      return ERR_NothingDone;
   }

   // Move the content from the end of the stream to the requested insertion point

   if (TargetIndex < inserted_at) {
      auto length = Self->Stream.size() - inserted_at;
      log.trace("Moving new content of %d bytes to the insertion point at index %d", TargetIndex, length);
      Self->Stream.insert(TargetIndex, Self->Stream.substr(inserted_at, length));
      Self->Stream.resize(inserted_at + length);
   }

   // Check that the FocusIndex is valid (there's a slim possibility that it may not be if AC_Focus has been
   // incorrectly used).

   if (Self->FocusIndex >= LONG(Self->Tabs.size())) Self->FocusIndex = -1;

   return ERR_Okay;
}

static ERROR insert_xml(extDocument *Self, objXML *XML, XMLTag &Tag, LONG TargetIndex, UBYTE Flags)
{
   pf::Log log(__FUNCTION__);

   if (TargetIndex < 0) TargetIndex = Self->Stream.size();

   log.traceBranch("Index: %d, Flags: $%.2x, Tag: %s", TargetIndex, Flags, Tag.Attribs[0].Name.c_str());

   // Retrieve the most recent font definition and use that as the style that we're going to start with.

   if (Flags & IXF_HOLDSTYLE) {
      // Do nothing to change the style
   }
   else {
      Self->Style.clear();

      if (Flags & IXF_RESETSTYLE) {
         // Do not search for the most recent font style
      }
      else {
         auto &str = Self->Stream;
         auto i   = TargetIndex;
         PREV_CHAR(str, i);
         while (i > 0) {
            if ((str[i] IS CTRL_CODE) and (ESCAPE_CODE(str, i) IS ESC_FONT)) {
               Self->Style.FontStyle = escape_data<escFont>(Self, i);
               log.trace("Found existing font style, font index %d, flags $%.8x.", Self->Style.FontStyle.Index, Self->Style.FontStyle.Options);
               break;
            }
            PREV_CHAR(str, i);
         }
      }

      // If no style is available, we need to create a default font style and insert it at the start of the stream.

      if (Self->Style.FontStyle.Index IS -1) {
         if ((Self->Style.FontStyle.Index = create_font(Self->FontFace, "Regular", Self->FontSize)) IS -1) {
            if ((Self->Style.FontStyle.Index = create_font("Open Sans", "Regular", 10)) IS -1) {
               return ERR_Failed;
            }
         }

         Self->Style.FontStyle.Colour = Self->FontColour;
         Self->Style.FontChange = true;
      }

      if (auto font = lookup_font(Self->Style.FontStyle.Index, "insert_xml")) {
         Self->Style.Face  = font->Face;
         Self->Style.Point = font->Point;
      }
   }

   // Parse content and insert it at the end of the stream (we will move it to the insertion point afterwards).

   LONG inserted_at = Self->Stream.size();
   LONG insert_index = Self->Stream.size();
   auto flags = IPF::NIL;
   parse_tag(Self, XML, Tag, insert_index, flags);

   if (Flags & IXF_CLOSESTYLE) style_check(Self, insert_index);

   if (LONG(Self->Stream.size()) <= inserted_at) {
      log.trace("parse_tag() did not insert any content into the stream.");
      return ERR_NothingDone;
   }

   // Move the content from the end of the stream to the requested insertion point

   if (TargetIndex < inserted_at) {
      auto length = LONG(Self->Stream.size()) - inserted_at;
      log.trace("Moving new content of %d bytes to the insertion point at index %d", TargetIndex, length);
      Self->Stream.insert(TargetIndex, Self->Stream.substr(inserted_at, length));
      Self->Stream.resize(inserted_at + length);
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

static LONG parse_tag(extDocument *Self, objXML *XML, XMLTag &Tag, LONG &Index, IPF &Flags)
{
   pf::Log log(__FUNCTION__);

   if (Self->Error) {
      log.traceWarning("Error field is set, returning immediately.");
      return 0;
   }

   IPF filter = Flags & IPF::FILTER_ALL;
   XMLTag *object_template = NULL;
   
   auto process_object = [&](XMLTag &Tag, std::string &tagname) {
      if ((Flags & IPF::NO_CONTENT) IS IPF::NIL) {
         // Check if the tagname refers to a class.  For security reasons, we limit the classes that can be embedded
         // in functional pages.

         if (tagname.starts_with("obj:")) tagname.erase(0, 4);

         std::string pagetarget;
         CLASSID class_id = 0;
         unsigned i;
         for (i=0; i < ARRAYSIZE(glDocClasses); i++) {
            if (!StrMatch(tagname, glDocClasses[i].ClassName)) {
               pagetarget = glDocClasses[i].PageTarget;
               class_id = glDocClasses[i].ClassID;
               break;
            }
         }

         if ((i >= ARRAYSIZE(glDocClasses)) and ((Self->Flags & DCF::UNRESTRICTED) != DCF::NIL)) {
            class_id = ResolveClassName(tagname.c_str());
         }

         if (class_id) {
            auto parse_flags = IPF::NIL;
            tag_object(Self, pagetarget, class_id, object_template, XML, Tag, Tag.Children, Index, parse_flags);
         }
         else log.warning("Tag '%s' unsupported as an instruction, template or class.", tagname.c_str());
      }
      else log.warning("Unrecognised tag '%s' used in a content-restricted area.", tagname.c_str());
   };

   auto saved_attribs = Tag.Attribs;
   translate_attrib_args(Self, Tag.Attribs);

   auto tagname = Tag.Attribs[0].Name;
   if (tagname.starts_with('$')) tagname.erase(0, 1);
   object_template = NULL;

   LONG result = 0;
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
            if ((!StrMatch("class", scan.Attribs[i].Name)) and (!StrMatch(tagname, scan.Attribs[i].Value))) {
               object_template = &scan;
               template_match = true;
            }
            else if ((!StrMatch("name", scan.Attribs[i].Name)) and (!StrMatch(tagname, scan.Attribs[i].Value))) {
               template_match = true;
            }
         }

         if (template_match) {
            if (object_template) process_object(Tag, tagname);               
            else {
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
            result = TRF_BREAK;
         }
         else {
            if ((tr.Flags & TAG::PARAGRAPH) != TAG::NIL) Self->ParagraphDepth++;

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

            if ((tr.Flags & TAG::PARAGRAPH) != TAG::NIL) Self->ParagraphDepth--;
         }
      }
   }
   else if (!StrMatch("break", tagname)) {
      // Breaking stops executing all tags (within this section) beyond the breakpoint.  If in a loop, the loop
      // will stop executing.

      result = TRF_BREAK;
   }
   else if (!StrMatch("continue", tagname)) {
      // Continuing - does the same thing as a break but the loop continues.
      // If used when not in a loop, then all sibling tags are skipped.

      result = TRF_CONTINUE;
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

            if ((state) and (parse_tags(Self, XML, Tag.Children, Index, Flags) & TRF_BREAK)) break;

            Self->LoopIndex++;
         }

         Self->LoopIndex = saveindex;
      }
   }
   else process_object(Tag, tagname);      

   Tag.Attribs = saved_attribs;
   return result;
}

static LONG parse_tags(extDocument *Self, objXML *XML, objXML::TAGS &Tags, LONG &Index, IPF Flags)
{
   LONG result = 0;
   
   for (auto &tag : Tags) {
      // Note that Flags will carry state between multiple calls to parse_tag().  This allows if/else to work correctly.
      result = parse_tag(Self, XML, tag, Index, Flags); 
      if ((Self->Error) or (result & (TRF_CONTINUE|TRF_BREAK))) break;
   }

   return result;
}

//********************************************************************************************************************

static void style_check(extDocument *Self, LONG &Index)
{
   if (Self->Style.FontChange) {
      // Create a new font object for the current style

      auto style_name = get_font_style(Self->Style.FontStyle.Options);
      Self->Style.FontStyle.Index = create_font(Self->Style.Face, style_name, Self->Style.Point);
      //log.trace("Changed font to index %d, face %s, style %s, size %d.", Self->Style.FontStyle.Index, Self->Style.Face, style_name, Self->Style.Point);
      Self->Style.FontChange  = false;
      Self->Style.StyleChange = true;
   }

   if (Self->Style.StyleChange) {
      // Insert a font change into the text stream
      //log.trace("Style change detected.");
      Self->insertEscape(Index, Self->Style.FontStyle);
      Self->Style.StyleChange = false;
   }
}

//********************************************************************************************************************
// Inserts plain UTF8 text into the document stream.  Insertion can be at any byte index, indicated by the Index
// parameter.  The Index value will be increased by the number of bytes to insert, indicated by Length.
//
// Preformat must be set to true if all consecutive whitespace characters in Text are to be inserted.

static ERROR insert_text(extDocument *Self, LONG &Index, const std::string &Text, bool Preformat)
{
#ifdef DBG_STREAM
   pf::Log log(__FUNCTION__);
   log.trace("Index: %d, WSpace: %d", Index, Self->NoWhitespace);
#endif

   // Check if there is content to be processed

   if ((!Preformat) and (Self->NoWhitespace)) {
      unsigned i;
      for (i=0; i < Text.size(); i++) if (Text[i] > 0x20) break;
      if (i IS Text.size()) return ERR_Okay;
   }

   style_check(Self, Index);

   if (Preformat) {
      if (Text.find(CTRL_CODE) IS std::string::npos) {
         Self->Stream.insert(Index, Text);
         Index += Text.size();
      }
      else {
         std::string new_text(Text);
         std::replace(new_text.begin(), new_text.end(), CTRL_CODE, ' ');
         Self->Stream.insert(Index, new_text);
         Index += new_text.size();
      }
   }
   else {
      std::string new_text;
      new_text.reserve(Text.size());
      for (unsigned i=0; i < Text.size(); ) {
         if (Text[i] <= 0x20) { // Whitespace eliminator, also handles any unwanted presence of ESC_CODE which is < 0x20
            while ((Text[i] <= 0x20) and (i < Text.size())) i++;
            if (!Self->NoWhitespace) new_text += ' ';
            Self->NoWhitespace = true;
         }
         else {
            new_text += Text[i++];
            Self->NoWhitespace = false;
         }
      }
      Self->Stream.insert(Index, new_text);
      Index += new_text.size();
   }

   return ERR_Okay;
}

//********************************************************************************************************************
// Inserts an escape sequence into the text stream.
//
//
// [0x1b][Code][0xNNNNNNNN][0x1b]

template <class T> ERROR extDocument::insertEscape(LONG &Index, T &Code)
{
   // All escape codes are saved to a global container.
   //auto record = std::make_pair(ULONG(Code.ID), dynamic_cast<EscapeCode &>(Code));
   //Codes.insert(ULONG(Code.ID), Code);
   Codes[Code.ID] = Code;


   return ERR_Okay;
}
/*
static ERROR insert_escape(extDocument *Self, LONG &Index, WORD EscapeCode, APTR Data, LONG Length)
{
   pf::Log log(__FUNCTION__);

#ifdef DBG_STREAM
   log.trace("Index: %d, Code: %s (%d), Length: %d", Index, strCodes[EscapeCode], EscapeCode, Length);
#else
   //log.trace("Index: %d, Code: %d, Length: %d", Index, EscapeCode, Length);
#endif

   if (Length > 0xffff - ESC_LEN) {
      log.warning("Escape code length of %d exceeds %d bytes.", Length, 0xffff - ESC_LEN);
      return ERR_BufferOverflow;
   }

   auto &stream = Self->Stream;
   LONG element_id = ++Self->ElementCounter;
   LONG size = Self->Stream.size() + Length + ESC_LEN + 1;
   LONG total_length = Length + ESC_LEN;

      if (Self->Stream[Index]) {
         CopyMemory(Self->Stream + Index,
            Self->Stream + Index + Length + ESC_LEN,
            Self->Stream.size() - Index);
      }

      stream = Self->Stream;
      LONG pos = Index;
      stream[pos++] = CTRL_CODE;
      stream[pos++] = EscapeCode;
      stream[pos++] = total_length>>8;
      stream[pos++] = total_length & 0xff;
      ((LONG *)(stream + pos))[0] = element_id;
      pos += sizeof(LONG);
      if ((Data) and (Length > 0)) {
         CopyMemory(Data, stream + pos, Length);
         pos += Length;
      }
      stream[pos++] = total_length>>8;
      stream[pos++] = total_length & 0xff;
      stream[pos++] = CTRL_CODE;

   Index += Length + ESC_LEN;
   return ERR_Okay;
}
*/
//********************************************************************************************************************
// This function is called only when a paragraph or explicit line-break (\n) is encountered.

static void end_line(extDocument *Self, layout *l, LONG NewLine, LONG Index, DOUBLE Spacing, LONG RestartIndex, CSTRING Caller)
{
   pf::Log log(__FUNCTION__);
   LONG bottomline, new_y;

   if ((!l->line_height) and (l->wordwidth)) {
      // If this is a one-word line, the line height will not have been defined yet
      //log.trace("Line Height being set to font (currently %d/%d)", l->line_height, l->base_line);
      l->line_height = l->font->LineSpacing;
      l->base_line   = l->font->Ascent;
   }

   DLAYOUT("%s: CursorY: %d, ParaY: %d, ParaEnd: %d, Line Height: %d * %.2f, Index: %d/%d, Restart: %d", Caller, l->cursory, l->paragraph_y, l->paragraph_end, l->line_height, Spacing, l->line_index, Index, RestartIndex);

   for (unsigned i=l->start_clips; i < Self->Clips.size(); i++) {
      if (Self->Clips[i].Transparent) continue;
      if ((l->cursory + l->line_height >= Self->Clips[i].Clip.Top) and (l->cursory < Self->Clips[i].Clip.Bottom)) {
         if (l->cursorx + l->wordwidth < Self->Clips[i].Clip.Left) {
            if (Self->Clips[i].Clip.Left < l->alignwidth) l->alignwidth = Self->Clips[i].Clip.Left;
         }
      }
   }

   if (Index > l->line_index) {
      add_drawsegment(Self, l->line_index, Index, l, l->cursory, l->cursorx + l->wordwidth - l->line_x, l->alignwidth - l->line_x, "Esc:EndLine");
   }

   // Determine the new vertical position of the cursor.  This routine takes into account multiple line-breaks, so that
   // the overall amount of whitespace is no more than the biggest line-break specified in
   // a line-break sequence.

   if (NewLine) {
      bottomline = l->cursory + l->line_height;
      if (l->paragraph_end > bottomline) bottomline = l->paragraph_end;

      // Check for a previous paragraph escape sequence.  This resolves cases such as "<p>...<p>...</p></p>"

      if (auto i = Index; i > 0) {
         PREV_CHAR(Self->Stream, i);
         while (i > 0) {
            if (Self->Stream[i] IS CTRL_CODE) {
               if ((ESCAPE_CODE(Self->Stream, i) IS ESC_PARAGRAPH_END) or
                   (ESCAPE_CODE(Self->Stream, i) IS ESC_PARAGRAPH_START)) {

                  if (ESCAPE_CODE(Self->Stream, i) IS ESC_PARAGRAPH_START) {
                     // Check if a custom string is specified in the paragraph, in which case the paragraph counts
                     // as content.

                     auto &para = escape_data<escParagraph>(Self, i);
                     if (!para.Value.empty()) break;
                  }

                  bottomline = l->paragraph_y;
                  break;
               }
               else if ((ESCAPE_CODE(Self->Stream, i) IS ESC_OBJECT) or (ESCAPE_CODE(Self->Stream, i) IS ESC_TABLE_END)) break; // Content encountered

               PREV_CHAR(Self->Stream, i);
            }
            else break; // Content encountered
         }
      }

      l->paragraph_y = bottomline;

      // Paragraph gap measured as default line height * spacing ratio

      new_y = bottomline + F2I(Self->LineHeight * Spacing);
      if (new_y > l->cursory) l->cursory = new_y;
   }

   // Reset line management variables for a new line starting from the left margin.

   l->line_x      = l->left_margin;
   l->cursorx     = l->left_margin;
   l->line_height = 0;
   l->base_line   = 0;
   l->split_start = Self->Segments.size();
   l->line_index  = RestartIndex;
   l->wordindex   = l->line_index;
   l->kernchar    = 0;
   l->wordwidth   = 0;
   l->paragraph_end = 0;   
}

//********************************************************************************************************************
// Word-wrapping is checked whenever whitespace is encountered or certain escape codes are found in the text stream,
// e.g. paragraphs and objects will mark an end to the current word.
//
// Wrapping is always checked even if there is no 'active word' because we need to be able to wrap empty lines (e.g.
// solo <br/> tags).
//
// Index - The current index value.
// ObjectIndex - The index that indicates the start of the word.

static UBYTE check_wordwrap(CSTRING Type, extDocument *Self, LONG Index, layout *l, LONG X, LONG *Width,
   LONG ObjectIndex, LONG *GraphicX, LONG *GraphicY, LONG GraphicWidth, LONG GraphicHeight)
{
   pf::Log log(__FUNCTION__);
   LONG minwidth;
   UBYTE result;
   LONG breakloop;

   if (!Self->BreakLoop) return WRAP_DONOTHING;

   // If the width of the object is larger than the available page width, extend the size of the page.

/*
   if (GraphicWidth > *Width - l->left_margin - l->right_margin) {
      *Width = GraphicWidth + l->left_margin + l->right_margin;
      return WRAP_EXTENDPAGE;
   }
*/

   // This code pushes the object along to the next available space when a boundary is encountered on the current line.

   WRAP("~check_wrap()","Index: %d/%d, %s: %dx%d,%dx%d, LineHeight: %d, Cursor: %dx%d, PageWidth: %d, Edge: %d", Index, ObjectIndex, Type, *GraphicX, *GraphicY, GraphicWidth, GraphicHeight, l->line_height, l->cursorx, l->cursory, *Width, l->wrapedge);

   result = WRAP_DONOTHING;
   breakloop = MAXLOOP;

restart:
   l->alignwidth = l->wrapedge;

   if (!Self->Clips.empty()) check_clips(Self, Index, l, ObjectIndex, GraphicX, GraphicY, GraphicWidth, GraphicHeight);

   if (*GraphicX + GraphicWidth > l->wrapedge) {
      if ((*Width < WIDTH_LIMIT) and ((*GraphicX IS l->left_margin) or (l->nowrap))) {
         // Force an extension of the page width and recalculate from scratch
         minwidth = *GraphicX + GraphicWidth + l->right_margin - X;
         if (minwidth > *Width) {
            *Width = minwidth;
            WRAP("check_wrap:","Forcing an extension of the page width to %d", minwidth);
         }
         else *Width += 1;
         return WRAP_EXTENDPAGE;
      }
      else {
         if (!l->line_height) {
            l->line_height = 1; //l->font->LineSpacing;
            l->base_line   = 1; //l->font->Ascent;
         }

         if (l->link) {
            if (l->link_x IS *GraphicX) {
               // If the link starts with the object, the link itself is going to be wrapped with it
            }
            else {
               add_link(Self, ESC_LINK, l->link, l->link_x, *GraphicY, *GraphicX - l->link_x, l->line_height, "check_wrap");
            }
         }

         // Set the line segment up to the object index.  The line_index is
         // updated so that this process only occurs in the first iteration.

         if (l->line_index < ObjectIndex) {
            add_drawsegment(Self, l->line_index, ObjectIndex, l, *GraphicY, *GraphicX - l->line_x, l->alignwidth - l->line_x, "DoWrap");
            l->line_index = ObjectIndex;
         }

         // Reset the line management variables so that the next line starts at the left margin.

         *GraphicX        = l->left_margin;
         *GraphicY       += l->line_height;
         l->cursorx      = *GraphicX;
         l->cursory      = *GraphicY;
         l->split_start  = Self->Segments.size();
         l->line_x       = l->left_margin;
         l->link_x       = l->left_margin; // Only matters if a link is defined
         l->kernchar     = 0;
         l->base_line    = 0;
         l->line_height  = 0;

         result = WRAP_WRAPPED;
         if (--breakloop > 0) goto restart; // Go back and check the clip boundaries again
         else {
            log.traceWarning("Breaking out of continuous loop.");
            Self->Error = ERR_Loop;
         }
      }
   }

   // No wrap has occurred

   if ((l->link) and (l->link_open IS false)) {
      // A link is due to be closed
      add_link(Self, ESC_LINK, l->link, l->link_x, *GraphicY, *GraphicX + GraphicWidth - l->link_x, l->line_height ? l->line_height : l->font->LineSpacing, "check_wrap");
      l->link = NULL;
   }

   #ifdef DBG_WORDWRAP
      if (result IS WRAP_WRAPPED) WRAP("check_wrap","A wrap to Y coordinate %d has occurred.", l->cursory);
   #endif

   return result;
}

static void check_clips(extDocument *Self, LONG Index, layout *l,
   LONG ObjectIndex, LONG *GraphicX, LONG *GraphicY, LONG GraphicWidth, LONG GraphicHeight)
{
   LONG clip, i, height;
   UBYTE reset_link;

   WRAP("~check_clips()","Index: %d-%d, ObjectIndex: %d, Graphic: %dx%d,%dx%d, TotalClips: %d", l->line_index, Index, ObjectIndex, *GraphicX, *GraphicY, GraphicWidth, GraphicHeight, Self->Clips.size());

   for (clip=l->start_clips; clip < LONG(Self->Clips.size()); clip++) {
      if (Self->Clips[clip].Transparent) continue;
      if (*GraphicY + GraphicHeight < Self->Clips[clip].Clip.Top) continue;
      if (*GraphicY >= Self->Clips[clip].Clip.Bottom) continue;
      if (*GraphicX >= Self->Clips[clip].Clip.Right) continue;
      if (*GraphicX + GraphicWidth < Self->Clips[clip].Clip.Left) continue;

      if (Self->Clips[clip].Clip.Left < l->alignwidth) l->alignwidth = Self->Clips[clip].Clip.Left;

      WRAP("check_clips:","Word: \"%.20s\" (%dx%d,%dx%d) advances over clip %d-%d",
         printable(Self, ObjectIndex).c_str(), *GraphicX, *GraphicY, GraphicWidth, GraphicHeight,
         Self->Clips[clip].Clip.Left, Self->Clips[clip].Clip.Right);

      // Set the line segment up to the encountered boundary and continue checking the object position against the
      // clipping boundaries.

      if ((l->link) and (Self->Clips[clip].Index < l->link_index)) {
         // An open link intersects with a clipping region that was created prior to the opening of the link.  We do
         // not want to include this object as a clickable part of the link - we will wrap over or around it, so
         // set a partial link now and ensure the link is reopened after the clipping region.

         WRAP("~check_clips","Setting hyperlink now to cross a clipping boundary.");

         if (!l->line_height) height = l->font->LineSpacing;
         else height = l->line_height;
         add_link(Self, ESC_LINK, l->link, l->link_x, *GraphicY, *GraphicX + GraphicWidth - l->link_x, height, "clip_intersect");

         reset_link = true;
      }
      else reset_link = false;

      // Advance the object position.  We break if a wordwrap is required - the code outside of this loop will detect
      // the need for a wordwrap and then restart the wordwrapping process.

      if (*GraphicX IS l->line_x) l->line_x = Self->Clips[clip].Clip.Right;
      *GraphicX = Self->Clips[clip].Clip.Right; // Push the object over the clip boundary

      if (*GraphicX + GraphicWidth > l->wrapedge) {
         WRAP("check_clips:","Wrapping-Break: X(%d)+Width(%d) > Edge(%d) at clip '%s' %dx%d,%dx%d", *GraphicX, GraphicWidth, l->wrapedge, Self->Clips[clip].Name, Self->Clips[clip].Clip.Left, Self->Clips[clip].Clip.Top, Self->Clips[clip].Clip.Right, Self->Clips[clip].Clip.Bottom);
         break;
      }

      if ((GraphicWidth) and (ObjectIndex >= 0)) i = ObjectIndex;
      else i = Index;

      if (l->line_index < i) {
         if (!l->line_height) {
            add_drawsegment(Self, l->line_index, i, l, *GraphicY, *GraphicX - l->line_x, *GraphicX - l->line_x, "Wrap:EmptyLine");
         }
         else add_drawsegment(Self, l->line_index, i, l, *GraphicY, *GraphicX + GraphicWidth - l->line_x, l->alignwidth - l->line_x, "Wrap");
      }

      WRAP("check_clips","Line index reset to %d, previously %d", i, l->line_index);

      l->line_index = i;
      l->line_x = *GraphicX;
      if ((reset_link) and (l->link)) l->link_x = *GraphicX;

      clip = l->start_clips-1; // Check all the clips from the beginning
   }
}

//********************************************************************************************************************
// Calculate the position, pixel length and height of each line for the entire page.  This function does not recurse,
// but it reiterates if the size of the page section is expanded.  It is also called for individual table cells
// which are treated as miniature pages.
//
// Offset:   The byte offset within the document stream to start layout processing.
// X/Y:      Section coordinates, starts at 0,0 for the main page, subsequent sections (table cells) can be at any location, measured as absolute to the top left corner of the page.
// Width:    Minimum width of the page/section.  Can be increased if insufficient space is available.  Includes the left and right margins in the resulting calculation.
// Height:   Minimum height of the page/section.  Will be increased to match the number of lines in the layout.
// Margins:  Margins within the page area.  These are inclusive to the resulting page width/height.  If in a cell, margins reflect cell padding values.
     
struct LAYOUT_STATE {
   // Records the current layout, index and state information.
   layout Layout;
   LONG Index      = 0;
   LONG TotalClips = 0;
   LONG TotalLinks = 0;
   LONG SegCount   = 0;
   LONG ECIndex    = 0;

   LAYOUT_STATE() = default;

   LAYOUT_STATE(extDocument *pSelf, LONG pIndex, layout &pLayout) {
      Layout     = pLayout; 
      Index      = pIndex; 
      TotalClips = pSelf->Clips.size(); 
      TotalLinks = pSelf->Links.size(); 
      ECIndex    = pSelf->EditCells.size(); 
      SegCount   = pSelf->Segments.size();
   }

   void restore(extDocument *pSelf) {
      pf::Log log(__FUNCTION__);
      DLAYOUT("Restoring earlier layout state to index %d", Index);
      pSelf->Clips.resize(TotalClips);
      pSelf->Links.resize(TotalLinks);
      pSelf->Segments.resize(SegCount);
      pSelf->EditCells.resize(ECIndex);
   }
};

static LONG layout_section(extDocument *Self, LONG Offset, objFont **Font,
   LONG AbsX, LONG AbsY, LONG *Width, LONG *Height,
   LONG LeftMargin, LONG TopMargin, LONG RightMargin, LONG BottomMargin, bool *VerticalRepass)
{
   pf::Log log(__FUNCTION__);
   layout l;
   escFont    *style;
   escAdvance *advance;
   escObject  *escobj;
   escList    *esclist;
   escCell    *esccell;
   escRow     *escrow, *lastrow;
   DocEdit    *edit;
   escTable   *esctable;
   escParagraph *escpara;
   LAYOUT_STATE tablestate, rowstate, liststate;
   LONG start_ecindex, unicode, i, j, page_height, lastheight, lastwidth, edit_segment;
   bool checkwrap, object_vertical_repass;

   if ((Self->Stream.empty()) or (!Self->Stream[Offset]) or (!Font)) {
      log.trace("No document stream to be processed.");
      return 0;
   }

   if (Self->Depth >= MAX_DEPTH) {
      log.trace("Depth limit exceeded (too many tables-within-tables).");
      return 0;
   }
   
   // You must execute a goto to the point at which SAVE_STATE() was used after calling this macro
   
   auto RESTORE_STATE = [&](LAYOUT_STATE &s) {
      s.restore(Self); 
      l = s.Layout; 
      i = s.Index;
   };

   auto start_links    = Self->Links.size();
   auto start_segments = Self->Segments.size();
   l.start_clips  = Self->Clips.size();
   start_ecindex  = Self->EditCells.size();
   page_height    = *Height;
   object_vertical_repass = false;

   *VerticalRepass = false;

   #ifdef DBG_LAYOUT
   log.branch("Dimensions: %dx%d,%dx%d (edge %d), LM %d RM %d TM %d BM %d",
      AbsX, AbsY, *Width, *Height, AbsX + *Width - RightMargin,
      LeftMargin, RightMargin, TopMargin, BottomMargin);
   #endif

   Self->Depth++;

extend_page:
   if (*Width > WIDTH_LIMIT) {
      DLAYOUT("Restricting page width from %d to %d", *Width, WIDTH_LIMIT);
      *Width = WIDTH_LIMIT;
      if (Self->BreakLoop > 4) Self->BreakLoop = 4; // Very large page widths normally means that there's a parsing problem
   }

   if (Self->Error) {
      Self->Depth--;      
      return 0;
   }
   else if (!Self->BreakLoop) {
      Self->Error = ERR_Loop;
      Self->Depth--;      
      return 0;
   }
   Self->BreakLoop--;

   Self->Links.resize(start_links);     // Also refer to SAVE_STATE() and restore_state()
   Self->Segments.resize(start_segments);
   Self->Clips.resize(l.start_clips);

   lastrow         = NULL; // For table management
   lastwidth       = *Width;
   lastheight      = page_height;
   esclist         = NULL;
   escrow          = NULL;
   esctable        = NULL;
   escpara         = NULL;
   edit            = NULL;
   esccell         = NULL;
   style           = NULL;
   edit_segment    = 0;
   checkwrap       = false;  // true if a wordwrap or collision check is required
   l.anchor        = false;  // true if in an anchored section (objects are anchored to the line)
   l.alignflags    = 0;      // Current alignment settings according to the font style
   l.link          = NULL;
   l.paragraph_y   = 0;
   l.paragraph_end = 0;
   l.line_increase = 0;
   l.len           = 0;
   l.left_margin   = AbsX + LeftMargin;
   l.right_margin  = RightMargin;   // Retain the right margin in an adjustable variable, in case we adjust the margin
   l.wrapedge      = AbsX + *Width - l.right_margin;
   l.alignwidth    = l.wrapedge;
   l.cursorx       = AbsX + LeftMargin;  // The absolute position of the cursor
   l.cursory       = AbsY + TopMargin;
   l.wordwidth     = 0;         // The pixel width of the current word.  Zero if no word is being worked on
   l.wordindex     = -1;        // A byte index in the stream, for the word currently being operated on
   l.line_index    = Offset;    // The starting index of the line we are operating on
   l.line_x        = AbsX + LeftMargin;
   l.line_height   = 0;
   l.base_line     = 0;
   l.kernchar      = 0;      // Previous character of the word being operated on
   l.link_x        = 0;
   l.link_index    = 0;
   l.split_start   = Self->Segments.size();  // Set to the previous line index if line is segmented.  Used for ensuring that all distinct entries on the line use the same line height
   l.font          = *Font;
   l.nowrap        = false; // true if word wrapping is to be turned off
   l.link_open     = false;
   l.setsegment    = false;
   l.textcontent   = false;
   l.spacewidth    = fntCharWidth(l.font, ' ', 0, NULL);

   i = Offset;

   while (1) {
      // For certain graphics-related escape codes, set the line segment up to the encountered escape code if the text
      // string will be affected (e.g. if the string will be broken up due to a clipping region etc).

      if (Self->Stream[i] IS CTRL_CODE) {
         // Any escape code that sets l.setsegment to true in its case routine, must set breaksegment to true now so
         // that any textual content can be handled immediately.
         //
         // This is done particular for escape codes that can be treated as wordwrap breaks.

         if (l.line_index < i) {
            BYTE breaksegment;
            breaksegment = 0;
            switch (ESCAPE_CODE(Self->Stream, i)) {
               case ESC_ADVANCE:
               case ESC_TABLE_START:
                  breaksegment = 1;
                  break;

               case ESC_FONT:
                  if (l.textcontent) {
                     style = &escape_data<escFont>(Self, i);
                     objFont *font = lookup_font(style->Index, "ESC_FONT");
                     if (l.font != font) breaksegment = 1;
                  }
                  break;

               case ESC_OBJECT:
                  escobj = &escape_data<escObject>(Self, i);
                  if (escobj->Graphical) breaksegment = 1;
                  break;

               case ESC_INDEX_START: {
                  auto index = &escape_data<escIndex>(Self, i);
                  if (!index->Visible) breaksegment = 1;
                  break;
               }
            }

            if (breaksegment) {
               DLAYOUT("Setting line at escape '%s', index %d, line_x: %d, wordwidth: %d", strCodes[(UBYTE)ESCAPE_CODE(Self->Stream,i)].c_str(), l.line_index, l.line_x, l.wordwidth);
                  l.cursorx += l.wordwidth;
                  add_drawsegment(Self, l.line_index, i, &l, l.cursory, l.cursorx - l.line_x, l.alignwidth - l.line_x, "Esc:Object");
                  RESET_SEGMENT_WORD(i, l.cursorx, &l);
                  l.alignwidth = l.wrapedge;               
            }
         }
      }

      // Wordwrap checking.  Any escape code that results in a word-break for the current word will initiate a wrapping
      // check.  Encountering whitespace also results in a wrapping check.

      if (esctable) {
         l.alignwidth = l.wrapedge;
      }
      else {
         if (Self->Stream[i] IS CTRL_CODE) {
            switch (ESCAPE_CODE(Self->Stream, i)) {
               // These escape codes cause wrapping because they can break up words

               case ESC_PARAGRAPH_START:
               case ESC_PARAGRAPH_END:
               case ESC_TABLE_END:
               case ESC_OBJECT:
               case ESC_ADVANCE:
               case ESC_LINK_END:
                  checkwrap = true;
                  break;

               default:
                  l.alignwidth = l.wrapedge;
                  break;
            }
         }
         else if (Self->Stream[i] > 0x20) {
            // Non-whitespace characters do not result in a wordwrap check
            l.alignwidth = l.wrapedge;
         }
         else checkwrap = true;

         if (checkwrap) {
            LONG wrap_result;

            checkwrap = false;

            wrap_result = check_wordwrap("Text", Self, i, &l, AbsX, Width, l.wordindex, &l.cursorx, &l.cursory, (l.wordwidth < 1) ? 1 : l.wordwidth, (l.line_height < 1) ? 1 : l.line_height);

            if (wrap_result IS WRAP_EXTENDPAGE) {
               DLAYOUT("Expanding page width on wordwrap request.");
               goto extend_page;
            }
            else if ((Self->Stream[i] IS '\n') and (wrap_result IS WRAP_WRAPPED)) {
               // The presence of the line-break must be ignored, due to word-wrap having already made the new line for us
               i++;
               l.line_index = i;
               continue;
            }
         }
      }

      // Break the loop if there are no more characters to process

      if (!Self->Stream[i]) break;

      if (Self->Stream[i] IS CTRL_CODE) {
         // Escape code encountered.  The escape code format is:
         //   ESC,Code,Length,Data,Length,ESC

#ifdef DBG_LAYOUT_ESCAPE
         DLAYOUT("ESC_%s: %p, Index: %d-%d-%d, WordWidth: %d", strCodes[ESCAPE_CODE(Self->Stream, i)], esctable, l.line_index, i, l.wordindex, l.wordwidth);
#endif
         l.setsegment = false; // Escape codes that draw something in draw_document() (e.g. object, table) should set this flag to true in their case statement
         l.len = ESCAPE_LEN;
         switch (ESCAPE_CODE(Self->Stream, i)) {
            case ESC_ADVANCE:
               advance = &escape_data<escAdvance>(Self, i);
               l.cursorx += advance->X;
               l.cursory += advance->Y;
               if (advance->X) {
                  RESET_SEGMENT_WORD(i, l.cursorx, &l);
               }
               break;

            case ESC_FONT:
               style = &escape_data<escFont>(Self, i);
               l.font = lookup_font(style->Index, "ESC_FONT");

               if (l.font) {
                  if ((style->Options & FSO::ALIGN_RIGHT) != FSO::NIL) l.font->Align = ALIGN::RIGHT;
                  else if ((style->Options & FSO::ALIGN_CENTER) != FSO::NIL) l.font->Align = ALIGN::HORIZONTAL;
                  else l.font->Align = ALIGN::NIL;

                  if ((style->Options & FSO::ANCHOR) != FSO::NIL) l.anchor = true;
                  else l.anchor = false;

                  if ((style->Options & FSO::NO_WRAP) != FSO::NIL) {
                     l.nowrap = true;
                     //wrapedge = 1000;
                  }
                  else l.nowrap = false;

                  DLAYOUT("Font Index: %d, LineSpacing: %d, Height: %d, Ascent: %d, Cursor: %dx%d", style->Index, l.font->LineSpacing, l.font->Height, l.font->Ascent, l.cursorx, l.cursory);
                  l.spacewidth = fntCharWidth(l.font, ' ', 0, 0);

                  // Treat the font as if it is a text character by setting the wordindex.  This ensures it is included in the drawing process

                  if (!l.wordwidth) l.wordindex = i;
               }
               else DLAYOUT("ESC_FONT: Unable to lookup font using style index %d.", style->Index);

               break;

            case ESC_INDEX_START: {
               // Indexes don't do anything, but recording the cursor's Y value when they are encountered
               // makes it really easy to scroll to a bookmark when requested (show_bookmark()).

               LONG end;
               auto escindex = &escape_data<escIndex>(Self, i);
               escindex->Y = l.cursory;

               if (!escindex->Visible) {
                  // If Visible is false, then all content within the index is not to be displayed

                  end = i;
                  while (Self->Stream[end]) {
                     if (Self->Stream[end] IS CTRL_CODE) {
                        if (ESCAPE_CODE(Self->Stream, end) IS ESC_INDEX_END) {
                           escIndexEnd &iend = escape_data<escIndexEnd>(Self, end);
                           if (iend.ID IS escindex->ID) break;
                        }
                     }

                     NEXT_CHAR(Self->Stream, end);
                  }

                  if (Self->Stream[end] IS 0) {
                     log.warning("Failed to find matching index-end.  Document stream is corrupt.");
                     goto exit;
                  }

                  NEXT_CHAR(Self->Stream, end);

                  // Do some cleanup work to complete the content skip.  NB: There is some code associated with this at
                  // the top of this routine, with breaksegment = 1.

                  l.line_index = end;
                  i = end;
                  l.len = 0;
               }

               break;
            }

            case ESC_SET_MARGINS: {
               auto &escmargins = escape_data<escSetMargins>(Self, i);

               if (escmargins.Left != 0x7fff) {
                  l.cursorx     += escmargins.Left;
                  l.line_x      += escmargins.Left;
                  l.left_margin += escmargins.Left;
               }

               if (escmargins.Right != 0x7fff) {
                  l.right_margin += escmargins.Right;
                  l.alignwidth -= escmargins.Right;
                  l.wrapedge   -= escmargins.Right;
               }

               if (escmargins.Top != 0x7fff) {
                  if (l.cursory < AbsY + escmargins.Top) l.cursory = AbsY + escmargins.Top;
               }

               if (escmargins.Bottom != 0x7fff) {
                  BottomMargin += escmargins.Bottom;
                  if (BottomMargin < 0) BottomMargin = 0;
               }
               break;
            }

            // LINK MANAGEMENT

            case ESC_LINK: {
               if (l.link) {
                  // Close the currently open link because it's illegal to have a link embedded within a link.

                  if (l.font) {
                     add_link(Self, ESC_LINK, l.link, l.link_x, l.cursory, l.cursorx + l.wordwidth - l.link_x, l.line_height ? l.line_height : l.font->LineSpacing, "esc_link");
                  }
               }

               l.link       = &escape_data<escLink>(Self, i);
               l.link_x     = l.cursorx + l.wordwidth;
               l.link_index = i;
               l.link_open  = true;
               l.link_align = l.font->Align;
               break;
            }

            case ESC_LINK_END: {
               // We don't call add_link() unless the entire word that contains the link has
               // been processed.  This is necessary due to the potential for a word-wrap.

               if (l.link) {
                  l.link_open = false;

                  if (l.wordwidth < 1) {
                     add_link(Self, ESC_LINK, l.link, l.link_x, l.cursory, l.cursorx - l.link_x, l.line_height ? l.line_height : l.font->LineSpacing, "esc_link_end");
                     l.link = NULL;
                  }
               }

               break;
            }

            // LIST MANAGEMENT

            case ESC_LIST_START:
               // This is the start of a list.  Each item in the list will be identified by ESC_PARAGRAPH codes.  The
               // cursor position is advanced by the size of the item graphics element.

               liststate = LAYOUT_STATE(Self, i, l);

               if (esclist) {
                  auto ptr = esclist;
                  esclist = &escape_data<escList>(Self, i);
                  esclist->Stack = ptr;
               }
               else {
                  esclist = &escape_data<escList>(Self, i);
                  esclist->Stack = NULL;
               }
list_repass:
               esclist->Repass = false;
               break;

            case ESC_LIST_END:
               // If it is a custom list, a repass is required

               if ((esclist) and (esclist->Type IS LT_CUSTOM) and (esclist->Repass)) {
                  DLAYOUT("Repass for list required, commencing...");
                  RESTORE_STATE(liststate);
                  goto list_repass;
               }

               if (esclist) {
                  esclist = esclist->Stack;
               }

               // At the end of a list, increase the whitespace to that of a standard paragraph.

               if (!esclist) {
                  if (escpara) end_line(Self, &l, NL_PARAGRAPH, i, escpara->VSpacing, i, "Esc:ListEnd");
                  else end_line(Self, &l, NL_PARAGRAPH, i, 1.0, i, "Esc:ListEnd");
               }

               break;

            // EMBEDDED OBJECT MANAGEMENT

            case ESC_OBJECT: {
               ClipRectangle cell;
               OBJECTID object_id;

               // Tell the object our CursorX and CursorY positions so that it can position itself within the stream
               // layout.  The object will tell us its clipping boundary when it returns (if it has a clipping boundary).

               auto &escobj = escape_data<escObject>(Self, i);
               if (!(object_id = escobj.ObjectID)) break;
               if (!escobj.Graphical) break; // Do not bother with objects that do not draw anything
               if (escobj.Owned) break; // Do not manipulate objects that have owners

               // cell: Reflects the page/cell coordinates and width/height of the page/cell.

//wrap_object:
               cell.Left   = AbsX;
               cell.Top    = AbsY;
               cell.Right  = cell.Left + *Width;
               if ((!Offset) and (page_height < Self->AreaHeight)) cell.Bottom = AbsY + Self->AreaHeight; // The reported page height cannot be shorter than the document's surface area
               else cell.Bottom = AbsY + page_height;

               if (l.line_height) {
                  if (cell.Bottom < l.cursory + l.line_height) cell.Bottom = AbsY + l.line_height;
               }
               else if (cell.Bottom < l.cursory + 1) cell.Bottom = l.cursory + 1;

/*
               LONG width_check = 0;
               LONG dimensions = 0;
               LONG layoutflags = 0;
               if (!(error = AccessObject(object_id, 5000, &object))) {
                  DLAYOUT("[Idx:%d] The %s's available page area is %d-%d,%d-%d, margins %dx%d,%d, cursor %dx%d", i, object->Class->ClassName, cell.Left, cell.Right, cell.Top, cell.Bottom, l.left_margin-AbsX, l.right_margin, TopMargin, l.cursorx, l.cursory);

                  LONG cellwidth, cellheight, align, leftmargin, lineheight, zone_height;
                  OBJECTID layout_surface_id;

                  if ((FindField(object, FID_LayoutSurface, NULL)) and (!object->get(FID_LayoutSurface, &layout_surface_id))) {
                     objSurface *surface;
                     LONG new_x, new_y, new_width, new_height, calc_x;

                     // This layout method is used for objects that do not have a Layout object for graphics management and
                     // simply rely on a Surface object instead.

                     if (!(error = AccessObject(layout_surface_id, 3000, &surface))) {
                        leftmargin    = l.left_margin - AbsX;
                        lineheight    = (l.base_line) ? l.base_line : l.font->Ascent;

                        cellwidth  = cell.Right - cell.Left;
                        cellheight = cell.Bottom - cell.Top;
                        align = l.font->Align | surface->Align;

                        // Relative dimensions can use the full size of the page/cell only when text-wrapping is disabled.

                        zone_height = lineheight;
                        cell.Left += leftmargin;
                        cellwidth = cellwidth - l.right_margin - leftmargin; // Remove margins from the cellwidth because we're only interested in the space available to the object
                        new_x = l.cursorx;

                        // WIDTH

                        if (surface->Dimensions & DMF_RELATIVE_WIDTH) {
                           new_width = (DOUBLE)cellwidth * (DOUBLE)surface->WidthPercent * 0.01;
                           if (new_width < 1) new_width = 1;
                           else if (new_width > cellwidth) new_width = cellwidth;
                        }
                        else if (surface->Dimensions & DMF_FIXED_WIDTH) new_width = surface->Width;
                        else if ((surface->Dimensions & DMF_X) and (surface->Dimensions & DMF_X_OFFSET)) {
                           // Calculate X boundary

                           calc_x = new_x;

                           if (surface->Dimensions & DMF_FIXED_X);
                           else if (surface->Dimensions & DMF_RELATIVE_X) {
                              // Relative X, such as 10% would mean 'NewX must be at least 10% beyond 'cell.left + leftmargin'
                              LONG minx;
                              minx = cell.Left + F2T((DOUBLE)cellwidth * (DOUBLE)surface->XPercent * 0.01);
                              if (minx > calc_x) calc_x = minx;
                           }
                           else calc_x = l.cursorx;

                           // Calculate width

                           if (surface->Dimensions & DMF_FIXED_X_OFFSET) new_width = cellwidth - surface->XOffset - (calc_x - cell.Left);
                           else new_width = cellwidth - (calc_x - cell.Left) - (cellwidth * surface->XOffsetPercent * 0.01);

                           if (new_width < 1) new_width = 1;
                           else if (new_width > cellwidth) new_width = cellwidth;
                        }
                        else {
                           DLAYOUT("No width specified for %s #%d (dimensions $%x), defaulting to 1 pixel.", object->Class->ClassName, object->UID, surface->Dimensions);
                           new_width = 1;
                        }

                        // X COORD

                        if ((align & ALIGN::HORIZONTAL) and (surface->Dimensions & DMF_WIDTH)) {
                           new_x = new_x + ((cellwidth - new_width)/2);
                        }
                        else if ((align & ALIGN::RIGHT) and (surface->Dimensions & DMF_WIDTH)) {
                           new_x = (AbsX + *Width) - l.right_margin - new_width;
                        }
                        else if (surface->Dimensions & DMF_RELATIVE_X) {
                           new_x += F2T(surface->XPercent * cellwidth * 0.01);
                        }
                        else if ((surface->Dimensions & DMF_WIDTH) and (surface->Dimensions & DMF_X_OFFSET)) {
                           if (surface->Dimensions & DMF_FIXED_X_OFFSET) {
                              new_x += (cellwidth - new_width - surface->XOffset);
                           }
                           else new_x += F2T((DOUBLE)cellwidth - (DOUBLE)new_width - ((DOUBLE)cellwidth * (DOUBLE)surface->XOffsetPercent * 0.01));
                        }
                        else if (surface->Dimensions & DMF_FIXED_X) {
                           new_x += surface->X;
                        }

                        // HEIGHT

                        if (surface->Dimensions & DMF_RELATIVE_HEIGHT) {
                           // If the object is inside a paragraph <p> section, the height will be calculated based
                           // on the current line height.  Otherwise, the height is calculated based on the cell/page
                           // height.

                           new_height = (DOUBLE)zone_height * (DOUBLE)surface->HeightPercent * 0.01;
                           if (new_height > zone_height) new_height = zone_height;
                        }
                        else if (surface->Dimensions & DMF_FIXED_HEIGHT) {
                           new_height = surface->Height;
                        }
                        else if ((surface->Dimensions & DMF_Y) and
                                 (surface->Dimensions & DMF_Y_OFFSET)) {
                           if (surface->Dimensions & DMF_FIXED_Y) new_y = surface->Y;
                           else if (surface->Dimensions & DMF_RELATIVE_Y) new_y = F2T((DOUBLE)zone_height * (DOUBLE)surface->YPercent * 0.01);
                           else new_y = l.cursory;

                           if (surface->Dimensions & DMF_FIXED_Y_OFFSET) new_height = zone_height - surface->YOffset;
                           else new_height = zone_height - F2T((DOUBLE)zone_height * (DOUBLE)surface->YOffsetPercent * 0.01);

                           if (new_height > zone_height) new_height = zone_height;
                        }
                        else new_height = lineheight;

                        if (new_height < 1) new_height = 1;

                        // Y COORD

                        if (layoutflags & LAYOUT_IGNORE_CURSOR) new_y = cell.Top;
                        else new_y = l.cursory;

                        if (surface->Dimensions & DMF_RELATIVE_Y) {
                           new_y += F2T(surface->YPercent * lineheight * 0.01);
                        }
                        else if ((surface->Dimensions & DMF_HEIGHT) and
                                 (surface->Dimensions & DMF_Y_OFFSET)) {
                           if (surface->Dimensions & DMF_FIXED_Y_OFFSET) new_y = cell.Top + F2T(zone_height - surface->Height - surface->YOffset);
                           else new_y += F2T((DOUBLE)zone_height - (DOUBLE)surface->Height - ((DOUBLE)zone_height * (DOUBLE)surface->YOffsetPercent * 0.01));
                        }
                        else if (surface->Dimensions & DMF_Y_OFFSET) {
                           // This section resolves situations where no explicit Y coordinate has been defined,
                           // but the Y offset has been defined.  This means we leave the existing Y coordinate as-is and
                           // adjust the height.

                           if (surface->Dimensions & DMF_FIXED_Y_OFFSET) new_height = zone_height - surface->YOffset;
                           else new_height = zone_height - F2T((DOUBLE)zone_height * (DOUBLE)surface->YOffsetPercent * 0.01);

                           if (new_height < 1) new_height = 1;
                           if (new_height > zone_height) new_height = zone_height;
                        }
                        else if (surface->Dimensions & DMF_FIXED_Y) {
                           new_y += surface->Y;
                        }

                        // Set the clipping

                        DLAYOUT("Clip region is being restricted to the bounds: %dx%d,%dx%d", new_x, new_y, new_width, new_height);

                        cell.Left  = new_x;
                        cell.Top   = new_y;
                        cell.Right = new_x + new_width;
                        cell.Bottom = new_y + new_height;

                        // If LAYOUT_RIGHT is set, no text may be printed to the right of the object.  This has no impact
                        // on the object's bounds.

                        if (layoutflags & LAYOUT_RIGHT) {
                           DLAYOUT("LAYOUT_RIGHT: Expanding clip.right boundary from %d to %d.", cell.Right, AbsX + *Width - l.right_margin);
                           cell.Right = (AbsX + *Width) - l.right_margin; //cellwidth;
                        }

                        // If LAYOUT_LEFT is set, no text may be printed to the left of the object (but not
                        // including text that has already been printed).  This has no impact on the object's
                        // bounds.

                        if (layoutflags & LAYOUT_LEFT) {
                           DLAYOUT("LAYOUT_LEFT: Expanding clip.left boundary from %d to %d.", cell.Left, AbsX);
                           cell.Left  = AbsX; //leftmargin;
                        }

                        if (layoutflags & LAYOUT_IGNORE_CURSOR) width_check = cell.Right - AbsX;
                        else width_check = cell.Right + l.right_margin;

                        DLAYOUT("#%d, Pos: %dx%d,%dx%d, Align: $%.8x, From: %dx%d,%dx%d,%dx%d, WidthCheck: %d/%d", object->UID, new_x, new_y, new_width, new_height, align, F2T(surface->X), F2T(surface->Y), F2T(surface->Width), F2T(surface->Height), F2T(surface->XOffset), F2T(surface->YOffset), width_check, *Width);
                        DLAYOUT("Clip Size: %dx%d,%dx%d, LineHeight: %d, LayoutFlags: $%.8x", cell.Left, cell.Top, cellwidth, cellheight, lineheight, layoutflags);

                        dimensions = surface->Dimensions;
                        error = ERR_Okay;

                        acRedimension(surface, new_x, new_y, 0, new_width, new_height, 0);

                        ReleaseObject(surface);
                     }
                     else {
                        dimensions = 0;
                     }
                  }
                  else if ((FindField(object, FID_Layout, NULL)) and (!object->getPtr(FID_Layout, &layout))) {
                     leftmargin = l.left_margin - AbsX;
                     lineheight = (l.base_line) ? l.base_line : l.font->Ascent;

                     cellwidth  = cell.Right - cell.Left;
                     cellheight = cell.Bottom - cell.Top;
                     align = l.font->Align | layout->Align;

                     layoutflags = layout->Layout;

                     if (layoutflags & (LAYOUT_BACKGROUND|LAYOUT_TILE)) {
                        // In background mode, the bounds are adjusted to match the size of the cell
                        // if the object supports GraphicWidth and GraphicHeight.  For all other objects,
                        // it is assumed that the bounds have been preset.
                        //
                        // Positioning within the cell bounds is managed by the GraphicX/Y/Width/Height and
                        // Align fields.
                        //
                        // Gaps are automatically worked into the calculated X/Y value.

                        if ((layout->GraphicWidth) and (layout->GraphicHeight) and (!(layoutflags & LAYOUT_TILE))) {
                           layout->BoundX = cell.Left;
                           layout->BoundY = cell.Top;

                           if (align & ALIGN::HORIZONTAL) {
                              layout->GraphicX = cell.Left + layout->LeftMargin + ((cellwidth - layout->GraphicWidth)>>1);
                           }
                           else if (align & ALIGN::RIGHT) layout->GraphicX = cell.Left + cellwidth - layout->RightMargin - layout->GraphicWidth;
                           else if (!layout->PresetX) {
                              if (!layout->PresetWidth) {
                                 layout->GraphicX = cell.Left + layout->LeftMargin;
                              }
                              else layout->GraphicX = l.cursorx + layout->LeftMargin;
                           }

                           if (align & ALIGN::VERTICAL) layout->GraphicY = cell.Top + ((cellheight - layout->TopMargin - layout->BottomMargin - F2T(layout->GraphicHeight))>>1);
                           else if (align & ALIGN::BOTTOM) layout->GraphicY = cell.Top + cellheight - layout->BottomMargin - layout->GraphicHeight;
                           else if (!layout->PresetY) {
                              if (!layout->PresetHeight) {
                                 layout->GraphicY = cell.Top + layout->TopMargin;
                              }
                              else layout->GraphicY = l.cursory + layout->TopMargin;
                           }

                           // The object bounds are set to the GraphicWidth/Height.  When the object is drawn,
                           // the bounds will be automatically restricted to the size of the cell (or page) so
                           // that there is no over-draw.

                           layout->BoundWidth = layout->GraphicWidth;
                           layout->BoundHeight = layout->GraphicHeight;

                           DLAYOUT("X/Y: %dx%d, W/H: %dx%d (Width/Height are preset)", layout->BoundX, layout->BoundY, layout->BoundWidth, layout->BoundHeight);
                        }
                        else {
                           // If the object does not use preset GraphicWidth and GraphicHeight, then
                           // we just want to use the preset X/Y/Width/Height in relation to the available
                           // space within the cell.

                           layout->ParentSurface.Width = cell.Right - cell.Left;
                           layout->ParentSurface.Height = cell.Bottom - cell.Top;

                           object->get(FID_X, &layout->BoundX);
                           object->get(FID_Y, &layout->BoundY);
                           object->get(FID_Width, &layout->BoundWidth);
                           object->get(FID_Height, &layout->BoundHeight);

                           layout->BoundX += cell.Left;
                           layout->BoundY += cell.Top;


                           DLAYOUT("X/Y: %dx%d, W/H: %dx%d, Parent W/H: %dx%d (Width/Height not preset), Dimensions: $%.8x", layout->BoundX, layout->BoundY, layout->BoundWidth, layout->BoundHeight, layout->ParentSurface.Width, layout->ParentSurface.Height, layout->Dimensions);
                        }

                        dimensions = layout->Dimensions;
                        error = ERR_NothingDone; // Do not add a clipping region because the graphic is in the background
                     }
                     else {
                        // The object can extend the line's height if the GraphicHeight is larger than the line.
                        //
                        // Alignment calculations are restricted to this area, which forms the initial Bounds*:
                        //
                        //   X: CursorX
                        //   Y: CursorY
                        //   Right: PageWidth - RightMargin
                        //   Bottom: LineHeight - GraphicHeight
                        //
                        // If LAYOUT_IGNORE_CURSOR is set, then the user has set fixed values for both X and Y.
                        // The cursor is completely ignored and the existing Bound* fields will be used without alteration.
                        // Use of IGNORECURSOR also means that the left, right, top and bottom margins are all ignored.  Text
                        // will still be wrapped around the boundaries as long as LAYOUT_BACKGROUND isn't set.

                        LONG extclip_left, extclip_right;

                        extclip_left = 0;
                        extclip_right = 0;

                        if (layoutflags & LAYOUT_IGNORE_CURSOR);
                        else {
                           // In cursor-relative (normal) layout mode, the graphic will be restricted by
                           // the margins, so we adjust cell.left and the cellwidth accordingly.
                           //
                           // The BoundWidth and BoundHeight can be expanded if the GraphicWidth/GraphicHeight exceed the bound values.
                           //
                           // Relative width/height values are allowed.
                           //
                           // A relative XOffset is allowed, this will be computed against the cellwidth.
                           //
                           // Y Coordinates are allowed, these are computed from the top of the line.
                           //
                           // Relative height and vertical offsets are allowed, these are computed from the lineheight.
                           //
                           // Vertical alignment is managed within the bounds of the object when
                           // it is drawn, so we do not cater for vertical alignment when positioning
                           // the object in this code.

                           cell.Left += leftmargin;
                           cellwidth = cellwidth - l.right_margin - leftmargin; // Remove margins from the cellwidth because we're only interested in the space available to the object
                        }

                        // Adjust the bounds to reflect special dimension settings.  The minimum
                        // width and height is 1, and the bounds may not exceed the size of the
                        // available cell space (because a width of 110% would cause infinite recursion).

                        if (layoutflags & LAYOUT_IGNORE_CURSOR) layout->BoundX = cell.Left;
                        else layout->BoundX = l.cursorx;

                        layout->BoundWidth = 1; // Just a default in case no width in the Dimension flags is defined

                        if ((align & ALIGN::HORIZONTAL) and (layout->GraphicWidth)) {
                           // In horizontal mode where a GraphicWidth is preset, we force the BoundX and BoundWidth to their
                           // exact settings and override any attempt by the user to have preset the X and Width fields.
                           // The object will attempt a horizontal alignment within the bounds, this will be to no effect as the
                           // GraphicWidth is equivalent to the BoundWidth.  Text can still appear to the left and right of the object,
                           // if the author does not like this then the LAYOUT_LEFT and LAYOUT_RIGHT flags can be used to extend
                           // the clipping region.


                           LONG new_x = layout->BoundX + ((cellwidth - (layout->GraphicWidth + layout->LeftMargin + layout->RightMargin))/2);
                           if (new_x > layout->BoundX) layout->BoundX = new_x;
                           layout->BoundX += layout->LeftMargin;
                           layout->BoundWidth = layout->GraphicWidth;
                           extclip_left = layout->LeftMargin;
                           extclip_right = layout->RightMargin;
                        }
                        else if ((align & ALIGN::RIGHT) and (layout->GraphicWidth)) {
                           LONG new_x = ((AbsX + *Width) - l.right_margin) - (layout->GraphicWidth + layout->RightMargin);
                           if (new_x > layout->BoundX) layout->BoundX = new_x;
                           layout->BoundWidth = layout->GraphicWidth;
                           extclip_left = layout->LeftMargin;
                           extclip_right = layout->RightMargin;
                        }
                        else {
                           LONG xoffset;

                           if (layout->Dimensions & DMF_FIXED_X_OFFSET) xoffset = layout->XOffset;
                           else if (layout->Dimensions & DMF_RELATIVE_X_OFFSET) xoffset = (DOUBLE)cellwidth * (DOUBLE)layout->XOffset * 0.01;
                           else xoffset = 0;

                           if (layout->Dimensions & DMF_RELATIVE_X) {
                              LONG new_x = layout->BoundX + layout->LeftMargin + F2T(layout->X * cellwidth * 0.01);
                              if (new_x > layout->BoundX) layout->BoundX = new_x;
                              extclip_left = layout->LeftMargin;
                              extclip_right = layout->RightMargin;
                           }
                           else if (layout->Dimensions & DMF_FIXED_X) {
                              LONG new_x = layout->BoundX + layout->X + layout->LeftMargin;
                              if (new_x > layout->BoundX) layout->BoundX = new_x;
                              extclip_left = layout->LeftMargin;
                              extclip_right = layout->RightMargin;
                           }

                           // WIDTH

                           if (layout->Dimensions & DMF_RELATIVE_WIDTH) {
                              layout->BoundWidth = (DOUBLE)(cellwidth - (layout->BoundX - cell.Left)) * (DOUBLE)layout->Width * 0.01;
                              if (layout->BoundWidth < 1) layout->BoundWidth = 1;
                              else if (layout->BoundWidth > cellwidth) layout->BoundWidth = cellwidth;
                           }
                           else if (layout->Dimensions & DMF_FIXED_WIDTH) layout->BoundWidth = layout->Width;

                           // GraphicWidth and GraphicHeight settings will expand the width and height
                           // bounds automatically unless the Width and Height fields in the Layout have been preset
                           // by the user.
                           //
                           // NOTE: If the object supports GraphicWidth and GraphicHeight, it must keep
                           // them up to date if they are based on relative values.

                           if ((layout->GraphicWidth > 0) and (!(layout->Dimensions & DMF_WIDTH))) {
                              DLAYOUT("Setting BoundWidth from %d to preset GraphicWidth of %d", layout->BoundWidth, layout->GraphicWidth);
                              layout->BoundWidth = layout->GraphicWidth;
                           }
                           else if ((layout->Dimensions & DMF_X) and (layout->Dimensions & DMF_X_OFFSET)) {
                              if (layout->Dimensions & DMF_FIXED_X_OFFSET) layout->BoundWidth = cellwidth - xoffset - (layout->BoundX - cell.Left);
                              else layout->BoundWidth = cellwidth - (layout->BoundX - cell.Left) - xoffset;

                              if (layout->BoundWidth < 1) layout->BoundWidth = 1;
                              else if (layout->BoundWidth > cellwidth) layout->BoundWidth = cellwidth;
                           }
                           else if ((layout->Dimensions & DMF_WIDTH) and
                                    (layout->Dimensions & DMF_X_OFFSET)) {
                              if (layout->Dimensions & DMF_FIXED_X_OFFSET) {
                                 LONG new_x = layout->BoundX + cellwidth - layout->BoundWidth - xoffset - layout->RightMargin;
                                 if (new_x > layout->BoundX) layout->BoundX = new_x;
                                 extclip_left = layout->LeftMargin;
                              }
                              else {
                                 LONG new_x = layout->BoundX + F2T((DOUBLE)cellwidth - (DOUBLE)layout->BoundWidth - xoffset);
                                 if (new_x > layout->BoundX) layout->BoundX = new_x;
                              }
                           }
                           else {
                              if ((align & ALIGN::HORIZONTAL) and (layout->Dimensions & DMF_WIDTH)) {
                                 LONG new_x = layout->BoundX + ((cellwidth - (layout->BoundWidth + layout->LeftMargin + layout->RightMargin))/2);
                                 if (new_x > layout->BoundX) layout->BoundX = new_x;
                                 layout->BoundX += layout->LeftMargin;
                                 extclip_left = layout->LeftMargin;
                                 extclip_right = layout->RightMargin;
                              }
                              else if ((align & ALIGN::RIGHT) and (layout->Dimensions & DMF_WIDTH)) {
                                 // Note that it is possible the BoundX may end up behind the cursor, or the cell's left margin.
                                 // A check for this is made later, so don't worry about it here.

                                 LONG new_x = ((AbsX + *Width) - l.right_margin) - (layout->BoundWidth + layout->RightMargin);
                                 if (new_x > layout->BoundX) layout->BoundX = new_x;
                                 extclip_left = layout->LeftMargin;
                                 extclip_right = layout->RightMargin;
                              }
                           }
                        }

                        // VERTICAL SUPPORT

                        LONG obj_y;

                        if (layoutflags & LAYOUT_IGNORE_CURSOR) layout->BoundY = cell.Top;
                        else layout->BoundY = l.cursory;

                        obj_y = 0;
                        if (layout->Dimensions & DMF_RELATIVE_Y)   obj_y = F2T((DOUBLE)layout->Y * (DOUBLE)lineheight * 0.01);
                        else if (layout->Dimensions & DMF_FIXED_Y) obj_y = layout->Y;
                        obj_y += layout->TopMargin;
                        layout->BoundY += obj_y;
                        layout->BoundHeight = lineheight - obj_y; // This is merely a default

                        LONG zone_height;

                        if ((escpara) or (page_height < 1)) zone_height = lineheight;
                        else zone_height = page_height;

                        // HEIGHT

                        if (layout->Dimensions & DMF_RELATIVE_HEIGHT) {
                           // If the object is inside a paragraph <p> section, the height will be calculated based
                           // on the current line height.  Otherwise, the height is calculated based on the cell/page
                           // height.

                           layout->BoundHeight = (DOUBLE)(zone_height - obj_y) * (DOUBLE)layout->Height * 0.01;
                           if (layout->BoundHeight > zone_height - obj_y) layout->BoundHeight = lineheight - obj_y;
                        }
                        else if (layout->Dimensions & DMF_FIXED_HEIGHT) {
                           layout->BoundHeight = layout->Height;
                        }

                        if ((layout->GraphicHeight > layout->BoundHeight) and (!(layout->Dimensions & DMF_HEIGHT))) {
                           DLAYOUT("Expanding BoundHeight from %d to preset GraphicHeight of %d", layout->BoundHeight, layout->GraphicHeight);
                           layout->BoundHeight = layout->GraphicHeight;
                        }
                        else {
                           if (layout->BoundHeight < 1) layout->BoundHeight = 1;

                           // This code deals with vertical offsets

                           if (layout->Dimensions & DMF_Y_OFFSET) {
                              if (layout->Dimensions & DMF_Y) {
                                 if (layout->Dimensions & DMF_FIXED_Y_OFFSET) layout->BoundHeight = zone_height - layout->YOffset;
                                 else layout->BoundHeight = zone_height - F2T((DOUBLE)zone_height * (DOUBLE)layout->YOffset * 0.01);

                                 if (layout->BoundHeight > zone_height) layout->BoundHeight = zone_height;
                              }
                              else if (layout->Dimensions & DMF_HEIGHT) {
                                 if (layout->Dimensions & DMF_FIXED_Y_OFFSET) layout->BoundY = cell.Top + F2T(zone_height - layout->Height - layout->YOffset);
                                 else layout->BoundY += F2T((DOUBLE)zone_height - (DOUBLE)layout->Height - ((DOUBLE)zone_height * (DOUBLE)layout->YOffset * 0.01));
                              }
                           }
                        }

                        if (layoutflags & (LAYOUT_BACKGROUND|LAYOUT_TILE)) {
                           error = ERR_NothingDone; // No text wrapping for background and tile layouts
                        }
                        else {
                           // Set the clipping

                           DLAYOUT("Clip region is being restricted to the bounds: %dx%d,%dx%d", layout->BoundX, layout->BoundY, layout->BoundWidth, layout->BoundHeight);

                           cell.Left  = layout->BoundX - extclip_left;
                           cell.Top   = layout->BoundY - layout->TopMargin;
                           cell.Right = layout->BoundX + layout->BoundWidth + extclip_right;
                           cell.Bottom = layout->BoundY + layout->BoundHeight + layout->BottomMargin;

                           // If LAYOUT_RIGHT is set, no text may be printed to the right of the object.  This has no impact
                           // on the object's bounds.

                           if (layoutflags & LAYOUT_RIGHT) {
                              DLAYOUT("LAYOUT_RIGHT: Expanding clip.right boundary from %d to %d.", cell.Right, AbsX + *Width - l.right_margin);
                              LONG new_right = (AbsX + *Width) - l.right_margin; //cellwidth;
                              if (new_right > cell.Right) cell.Right = new_right;
                           }

                           // If LAYOUT_LEFT is set, no text may be printed to the left of the object (but not
                           // including text that has already been printed).  This has no impact on the object's
                           // bounds.

                           if (layoutflags & LAYOUT_LEFT) {
                              DLAYOUT("LAYOUT_LEFT: Expanding clip.left boundary from %d to %d.", cell.Left, AbsX);

                              if (layoutflags & LAYOUT_IGNORE_CURSOR) cell.Left = AbsX;
                              else cell.Left  = l.left_margin;
                           }

                           if (layoutflags & LAYOUT_IGNORE_CURSOR) width_check = cell.Right - AbsX;
                           else width_check = cell.Right + l.right_margin;

                           DLAYOUT("#%d, Pos: %dx%d,%dx%d, Align: $%.8x, From: %dx%d,%dx%d,%dx%d, WidthCheck: %d/%d", object->UID, layout->BoundX, layout->BoundY, layout->BoundWidth, layout->BoundHeight, align, F2T(layout->X), F2T(layout->Y), F2T(layout->Width), F2T(layout->Height), F2T(layout->XOffset), F2T(layout->YOffset), width_check, *Width);
                           DLAYOUT("Clip Size: %dx%d,%dx%d, LineHeight: %d, GfxSize: %dx%d, LayoutFlags: $%.8x", cell.Left, cell.Top, cellwidth, cellheight, lineheight, layout->GraphicWidth, layout->GraphicHeight, layoutflags);

                           dimensions = layout->Dimensions;
                           error = ERR_Okay;
                        }
                     }
                  }
                  else error = ERR_NoSupport;

                  ReleaseObject(object);
               }
               else {
                  if (error IS ERR_DoesNotExist) escobj->ObjectID = 0;
               }

               if ((!error) and (width_check)) {
                  // The cursor must advance past the clipping region so that the segment positions will be
                  // correct when set.

                  checkwrap = true;

                  // Check if the clipping region is invalid.  Invalid clipping regions are not added to the clip
                  // region list (i.e. layout of document text will ignore the presence of the object).

                  if ((cell.Bottom <= cell.Top) or (cell.Right <= cell.Left)) {
                     CSTRING name;
                     if ((name = object->Name)) log.warning("%s object %s returned an invalid clip region of %dx%d,%dx%d", object->Class->ClassName, name, cell.Left, cell.Top, cell.Right, cell.Bottom);
                     else log.warning("%s object #%d returned an invalid clip region of %dx%d,%dx%d", object->Class->ClassName, object->UID, cell.Left, cell.Top, cell.Right, cell.Bottom);
                     break;
                  }

                  // If the right-side of the object extends past the page width, increase the width.

                  LONG left_check;

                  if (layoutflags & LAYOUT_IGNORE_CURSOR) left_check = AbsX;
                  else if (layoutflags & LAYOUT_LEFT) left_check = l.left_margin;
                  else left_check = l.left_margin; //l.cursorx;

                  if (*Width >= WIDTH_LIMIT);
                  else if ((cell.Left < left_check) or (layoutflags & LAYOUT_IGNORE_CURSOR)) {
                     // The object is < left-hand side of the page/cell, this means
                     // that we may have to force a page/cell width increase.
                     //
                     // Note: Objects with IGNORECURSOR are always checked here, because they aren't subject
                     // to wrapping due to the X/Y being fixed.  Such objects are limited to width increases only.

                     LONG cmp_width;

                     if (layoutflags & LAYOUT_IGNORE_CURSOR) cmp_width = AbsX + (cell.Right - cell.Left);
                     else cmp_width = l.left_margin + (cell.Right - cell.Left) + l.right_margin;

                     if (*Width < cmp_width) {
                        DLAYOUT("Restarting as %s clip.left %d < %d and extends past the page width (%d > %d).", object->Class->ClassName, cell.Left, left_check, width_check, *Width);
                        *Width = cmp_width;
                        goto extend_page;
                     }
                  }
                  else if (width_check > *Width) {
                     // Perform a wrapping check if the object possibly extends past the width of the page/cell.

                     DLAYOUT("Wrapping %s object #%d as it extends past the page width (%d > %d).  Pos: %dx%d", object->Class->ClassName, object->UID, width_check, *Width, cell.Left, cell.Top);

                     j = check_wordwrap("Object", Self, i, &l, AbsX, Width, i, &cell.Left, &cell.Top, cell.Right - cell.Left, cell.Bottom - cell.Top);

                     if (j IS WRAP_EXTENDPAGE) {
                        DLAYOUT("Expanding page width due to object size.");
                        goto extend_page;
                     }
                     else if (j IS WRAP_WRAPPED) {
                        DLAYOUT("Object coordinates wrapped to %dx%d", cell.Left, cell.Top);
                        // The check_wordwrap() function will have reset l.cursorx and l.cursory, so
                        // on our repass, the cell.left and cell.top will reflect this new cursor position.

                        goto wrap_object;
                     }
                  }

                  DLAYOUT("Adding %s clip to the list: %dx%d,%dx%d", object->Class->ClassName, cell.Left, cell.Top, cell.Right-cell.Left, cell.Bottom-cell.Top);

                  Self->Clips.emplace_back(cell, i, layoutflags & (LAYOUT_BACKGROUND|LAYOUT_FOREGROUND) ? true : false, "Object");

                  if (!(layoutflags & (LAYOUT_BACKGROUND|LAYOUT_FOREGROUND))) {
                     if (cell.Bottom > l.cursory) {
                        objheight = cell.Bottom - l.cursory;
                        if ((l.anchor) or (escobj->Embedded)) {
                           // If all objects in the current section need to be anchored to the text, each
                           // object becomes part of the current line (e.g. treat the object as if it were
                           // a text character).  This requires us to adjust the line height.

                           if (objheight > l.line_height) {
                              l.line_height = objheight;
                              l.base_line   = l.font->Ascent;
                           }
                        }
                        else {
                           // If anchoring is not set, the height of the object will still define the height
                           // of the line, but cannot exceed the height of the font for that line.

                           if (objheight < l.font->LineSpacing) {
                              l.line_height = objheight;
                              l.base_line   = objheight;
                           }
                        }
                     }

                     //if (cell.Right > l.cursorx) l.wordwidth += cell.Right - l.cursorx;

                     if (escpara) {
                        j = cell.Bottom - escpara->Y;
                        if (j > escpara->Height) escpara->Height = j;
                     }
                  }
               }
               else if ((error != ERR_NothingDone) and (error != ERR_NoAction)) {
                  DLAYOUT("Error code #%d during object layout: %s", error, GetErrorMsg(error));
               }

               l.setsegment = true;

               // If the object uses a relative height or vertical offset, a repass will be required if the page height
               // increases.

               if ((dimensions & (DMF_RELATIVE_HEIGHT|DMF_FIXED_Y_OFFSET|DMF_RELATIVE_Y_OFFSET)) and (layoutflags & (LAYOUT_BACKGROUND|LAYOUT_IGNORE_CURSOR))) {
                  DLAYOUT("Vertical repass may be required.");
                  object_vertical_repass = true;
               }
*/
               break;
            }

            case ESC_TABLE_START: {
               // Table layout steps are as follows:
               //
               // 1. Copy prefixed/default widths and heights to all cells in the table.
               // 2. Calculate the size of each cell with respect to its content.  This can
               //    be left-to-right or top-to-bottom, it makes no difference.
               // 3. During the cell-layout process, keep track of the maximum width/height
               //    for the relevant row/column.  If either increases, make a second pass
               //    so that relevant cells are resized correctly.
               // 4. If the width of the rows is less than the requested table width (e.g.
               //    table width = 100%) then expand the cells to meet the requested width.
               // 5. Restart the page layout using the correct width and height settings
               //    for the cells.

               tablestate = LAYOUT_STATE(Self, i, l);

               if (esctable) {
                  auto ptr = esctable;
                  esctable = &escape_data<escTable>(Self, i);
                  esctable->Stack = ptr;
               }
               else {
                  esctable = &escape_data<escTable>(Self, i);
                  esctable->Stack = NULL;
               }

               esctable->ResetRowHeight = true; // All rows start with a height of MinHeight up until TABLE_END in the first pass
               esctable->ComputeColumns = 1;
               esctable->Width = -1;

               for (unsigned j=0; j < esctable->Columns.size(); j++) esctable->Columns[j].MinWidth = 0;

               LONG width;
wrap_table_start:
               // Calculate starting table width, ensuring that the table meets the minimum width according to the cell
               // spacing and padding values.

               if (esctable->WidthPercent) {
                  width = ((*Width - (l.cursorx - AbsX) - l.right_margin) * esctable->MinWidth) / 100;
               }
               else width = esctable->MinWidth;

               if (width < 0) width = 0;

               {
                  LONG min = (esctable->Thickness * 2) + (esctable->CellHSpacing * (esctable->Columns.size()-1)) + (esctable->CellPadding * 2 * esctable->Columns.size());
                  if (esctable->Thin) min -= esctable->CellHSpacing * 2; // Thin tables do not have spacing on the left and right borders
                  if (width < min) width = min;
               }

               if (width > WIDTH_LIMIT - l.cursorx - l.right_margin) {
                  log.traceWarning("Table width in excess of allowable limits.");
                  width = WIDTH_LIMIT - l.cursorx - l.right_margin;
                  if (Self->BreakLoop > 4) Self->BreakLoop = 4;
               }

               if (esctable->ComputeColumns) {
                  if (esctable->Width >= width) esctable->ComputeColumns = 0;
               }

               esctable->Width = width;

wrap_table_end:
wrap_table_cell:
               esctable->CursorX    = l.cursorx;
               esctable->CursorY    = l.cursory;
               esctable->X          = l.cursorx;
               esctable->Y          = l.cursory;
               esctable->RowIndex   = 0;
               esctable->TotalClips = Self->Clips.size();
               esctable->Height     = esctable->Thickness;

               DLAYOUT("(i%d) Laying out table of %dx%d, coords %dx%d,%dx%d%s, page width %d.", i, LONG(esctable->Columns.size()), esctable->Rows, esctable->X, esctable->Y, esctable->Width, esctable->MinHeight, esctable->HeightPercent ? "%" : "", *Width);
               // NB: LOGRETURN() is matched in ESC_TABLE_END

               if (esctable->ComputeColumns) {
                  // Compute the default column widths

                  esctable->ComputeColumns = 0;
                  esctable->CellsExpanded = false;

                  if (!esctable->Columns.empty()) {
                     for (unsigned j=0; j < esctable->Columns.size(); j++) {
                        //if (esctable->ComputeColumns IS 1) {
                        //   esctable->Columns[j].Width = 0;
                        //   esctable->Columns[j].MinWidth = 0;
                        //}

                        if (esctable->Columns[j].PresetWidth & 0x8000) { // Percentage width value
                           esctable->Columns[j].Width = (DOUBLE)((esctable->Columns[j].PresetWidth & 0x7fff) * esctable->Width) * 0.01;
                        }
                        else if (esctable->Columns[j].PresetWidth) { // Fixed width value
                           esctable->Columns[j].Width = esctable->Columns[j].PresetWidth;
                        }
                        else esctable->Columns[j].Width = 0;                        

                        if (esctable->Columns[j].MinWidth > esctable->Columns[j].Width) esctable->Columns[j].Width = esctable->Columns[j].MinWidth;
                     }
                  }
                  else {
                     log.warning("No columns array defined for table.");
                     esctable->Columns.clear();
                  }
               }

               DLAYOUT("Checking for table collisions before layout (%dx%d).  ResetRowHeight: %d", esctable->X, esctable->Y, esctable->ResetRowHeight);

               j = check_wordwrap("Table", Self, i, &l, AbsX, Width, i, &esctable->X, &esctable->Y, (esctable->Width < 1) ? 1 : esctable->Width, esctable->Height);
               if (j IS WRAP_EXTENDPAGE) {
                  DLAYOUT("Expanding page width due to table size.");                  
                  goto extend_page;
               }
               else if (j IS WRAP_WRAPPED) {
                  // The width of the table and positioning information needs
                  // to be recalculated in the event of a table wrap.

                  DLAYOUT("Restarting table calculation due to page wrap to position %dx%d.", l.cursorx, l.cursory);
                  esctable->ComputeColumns = 1;                  
                  goto wrap_table_start;
               }
               l.cursorx = esctable->X;
               l.cursory = esctable->Y;

               l.setsegment = true;

               l.cursory += esctable->Thickness + esctable->CellVSpacing;
               lastrow = NULL;

               break;
            }

            case ESC_TABLE_END: {
               ClipRectangle clip;
               LONG minheight;

               if (esctable->CellsExpanded IS false) {
                  DOUBLE cellwidth;
                  LONG unfixed, colwidth;

                  // Table cells need to match the available width inside the table.
                  // This routine checks for that - if the cells are short then the
                  // table processing is restarted.

                  DLAYOUT("Checking table @ index %d for cell/table widening.  Table width: %d", i, esctable->Width);

                  esctable->CellsExpanded = true;

                  if (!esctable->Columns.empty()) {
                     colwidth = (esctable->Thickness * 2) + esctable->CellHSpacing;
                     for (auto &col : esctable->Columns) {
                        colwidth += col.Width + esctable->CellHSpacing;
                     }
                     if (esctable->Thin) colwidth -= esctable->CellHSpacing * 2; // Thin tables have no spacing allocated on the sides

                     if (colwidth < esctable->Width) { // Cell layout is less than the pre-determined table width
                        // Calculate the amount of additional space that is available for cells to expand into

                        LONG avail_width = esctable->Width - (esctable->Thickness * 2) -
                           (esctable->CellHSpacing * (esctable->Columns.size() - 1));

                        if (!esctable->Thin) avail_width -= (esctable->CellHSpacing * 2);

                        // Count the number of columns that do not have a fixed size

                        unfixed = 0;
                        for (unsigned j=0; j < esctable->Columns.size(); j++) {
                           if (esctable->Columns[j].PresetWidth) avail_width -= esctable->Columns[j].Width;
                           else unfixed++;
                        }

                        // Adjust for expandable columns that we know have exceeded the pre-calculated cell width
                        // on previous passes (we want to treat them the same as the PresetWidth columns)  Such cells
                        // will often exist that contain large graphics for example.

                        if (unfixed > 0) {
                           cellwidth = avail_width / unfixed;
                           for (unsigned j=0; j < esctable->Columns.size(); j++) {
                              if ((esctable->Columns[j].MinWidth) and (esctable->Columns[j].MinWidth > cellwidth)) {
                                 avail_width -= esctable->Columns[j].MinWidth;
                                 unfixed--;
                              }
                           }

                           if (unfixed > 0) {
                              cellwidth = avail_width / unfixed;
                              bool expanded = false;

                              //total = 0;
                              for (unsigned j=0; j < esctable->Columns.size(); j++) {
                                 if (esctable->Columns[j].PresetWidth) continue; // Columns with preset-widths are never auto-expanded
                                 if (esctable->Columns[j].MinWidth > cellwidth) continue;

                                 if (esctable->Columns[j].Width < cellwidth) {
                                    DLAYOUT("Expanding column %d from width %d to %.2f", j, esctable->Columns[j].Width, cellwidth);
                                    esctable->Columns[j].Width = cellwidth;
                                    //if (total - (DOUBLE)F2I(total) >= 0.5) esctable->Columns[j].Width++; // Fractional correction

                                    expanded = true;
                                 }
                                 //total += cellwidth;
                              }

                              if (expanded) {
                                 DLAYOUT("At least one cell was widened - will repass table layout.");
                                 RESTORE_STATE(tablestate);                                 
                                 goto wrap_table_end;
                              }
                           }
                        }
                     }
                  }
                  else DLAYOUT("Table is missing its columns array.");
               }
               else DLAYOUT("Cells already widened - keeping table width of %d.", esctable->Width);

               // Cater for the minimum height requested

               if (esctable->HeightPercent) {
                  // If the table height is expressed as a percentage, it is calculated with
                  // respect to the height of the display port.

                  if (!Offset) {
                     minheight = ((Self->AreaHeight - BottomMargin - esctable->Y) * esctable->MinHeight) / 100;
                  }
                  else minheight = ((*Height - BottomMargin - TopMargin) * esctable->MinHeight) / 100;

                  if (minheight < 0) minheight = 0;
               }
               else minheight = esctable->MinHeight;

               if (minheight > esctable->Height + esctable->CellVSpacing + esctable->Thickness) {
                  // The last row in the table needs its height increased
                  if (lastrow) {
                     j = minheight - (esctable->Height + esctable->CellVSpacing + esctable->Thickness);
                     DLAYOUT("Extending table height to %d (row %d+%d) due to a minimum height of %d at coord %d", minheight, lastrow->RowHeight, j, esctable->MinHeight, esctable->Y);
                     lastrow->RowHeight += j;
                     RESTORE_STATE(rowstate);                     
                     escrow = lastrow;
                     goto repass_row_height_ext;
                  }
                  else log.warning("No last row defined for table height extension.");
               }

               // Adjust for cellspacing at the bottom

               esctable->Height += esctable->CellVSpacing + esctable->Thickness;

               // Restart if the width of the table will force an extension of the page.

               j = esctable->X + esctable->Width - AbsX + l.right_margin;
               if ((j > *Width) and (*Width < WIDTH_LIMIT)) {
                  DLAYOUT("Table width (%d+%d) increases page width to %d, layout restart forced.", esctable->X, esctable->Width, j);
                  *Width = j;                  
                  goto extend_page;
               }

               // Extend the height of the current line to the height of the table if the table is to be anchored (a
               // technique typically applied to objects).  We also extend the line height if the table covers the
               // entire width of the page (this is a valuable optimisation for the layout routine).

               if ((l.anchor) or ((esctable->X <= l.left_margin) and (esctable->X + esctable->Width >= l.wrapedge))) {
                  if (esctable->Height > l.line_height) {
                     l.line_height = esctable->Height;
                     l.base_line   = l.font->Ascent;
                  }
               }

               if (escpara) {
                  j = (esctable->Y + esctable->Height) - escpara->Y;
                  if (j > escpara->Height) escpara->Height = j;
               }

               // Check if the table collides with clipping boundaries and adjust its position accordingly.
               // Such a check is performed in ESC_TABLE_START - this second check is required only if the width
               // of the table has been extended.
               //
               // Note that the total number of clips is adjusted so that only clips up to the TABLE_START are 
               // considered (otherwise, clips inside the table cells will cause collisions against the parent
               // table).

               DLAYOUT("Checking table collisions (%dx%d).", esctable->X, esctable->Y);

               std::vector<DocClip> saved_clips(Self->Clips.begin() + esctable->TotalClips, Self->Clips.end() + Self->Clips.size());
               Self->Clips.resize(esctable->TotalClips);
               j = check_wordwrap("Table", Self, i, &l, AbsX, Width, i, &esctable->X, &esctable->Y, esctable->Width, esctable->Height);
               Self->Clips.insert(Self->Clips.end(), saved_clips.begin(), saved_clips.end());

               if (j IS WRAP_EXTENDPAGE) {
                  DLAYOUT("Table wrapped - expanding page width due to table size/position.");                  
                  goto extend_page;
               }
               else if (j IS WRAP_WRAPPED) {
                  // A repass is necessary as everything in the table will need to be rearranged
                  DLAYOUT("Table wrapped - rearrangement necessary.");

                  RESTORE_STATE(tablestate);                  
                  goto wrap_table_end;
               }

               //DLAYOUT("new table pos: %dx%d", esctable->X, esctable->Y);

               // The table sets a clipping region in order to state its placement (the surrounds of a table are
               // effectively treated as a graphical object, since it's not text).

               //if (clip.Left IS l.left_margin) clip.Left = 0; // Extending the clipping to the left doesn't hurt
                             
               Self->Clips.emplace_back(
                  ClipRectangle(esctable->X, esctable->Y, clip.Left + esctable->Width, clip.Top + esctable->Height),
                  i, false, "Table");   

               l.cursorx = esctable->X + esctable->Width;
               l.cursory = esctable->Y;

               DLAYOUT("Final Table Size: %dx%d,%dx%d", esctable->X, esctable->Y, esctable->Width, esctable->Height);

               esctable = esctable->Stack;

               l.setsegment = true;               
               break;
            }

            case ESC_ROW:
               if (escrow) {
                  auto ptr = escrow;
                  escrow = &escape_data<escRow>(Self, i);
                  escrow->Stack = ptr;
               }
               else {
                  escrow = &escape_data<escRow>(Self, i);
                  escrow->Stack = NULL;
               }

               rowstate = LAYOUT_STATE(Self, i, l);

               if (esctable->ResetRowHeight) escrow->RowHeight = escrow->MinHeight;

repass_row_height_ext:
               escrow->VerticalRepass = false;
               escrow->Y = l.cursory;
               esctable->RowWidth = (esctable->Thickness<<1) + esctable->CellHSpacing;

               l.setsegment = true;
               break;

            case ESC_ROW_END:
               esctable->RowIndex++;

                  // Increase the table height if the row extends beyond it

                  j = escrow->Y + escrow->RowHeight + esctable->CellVSpacing;
                  if (j > esctable->Y + esctable->Height) {
                     esctable->Height = j - esctable->Y;
                  }

                  // Advance the cursor by the height of this row

                  l.cursory += escrow->RowHeight + esctable->CellVSpacing;
                  l.cursorx = esctable->X;
                  DLAYOUT("Row ends, advancing down by %d+%d, new height: %d, y-cursor: %d",
                     escrow->RowHeight, esctable->CellVSpacing, esctable->Height, l.cursory);

               if (esctable->RowWidth > esctable->Width) esctable->Width = esctable->RowWidth;

               lastrow = escrow;
               escrow  = escrow->Stack;
               l.setsegment = true;
               break;

            case ESC_CELL: {
               // In the first pass, the size of each cell is calculated with
               // respect to its content.  When ESC_TABLE_END is reached, the
               // max height and width for each row/column will be calculated
               // and a subsequent pass will be made to fill out the cells.
               //
               // If the width of a cell increases, there is a chance that the height of all
               // cells in that column will decrease, subsequently lowering the row height
               // of all rows in the table, not just the current row.  Therefore on the second
               // pass the row heights need to be recalculated from scratch.

               bool vertical_repass;

               esccell = &escape_data<escCell>(Self, i);

               if (!esctable) {
                  log.warning("escTable variable not defined for cell @ index %d - document byte code is corrupt.", i);
                  goto exit;
               }

               if (esccell->Column >= LONG(esctable->Columns.size())) {
                  DLAYOUT("Cell %d exceeds total table column limit of %d.", esccell->Column, LONG(esctable->Columns.size()));
                  break;
               }

               // Setting the line is the only way to ensure that the table graphics will be accounted for when drawing.

               add_drawsegment(Self, i, i+l.len, &l, l.cursory, 0, 0, "Esc:Cell");

               // Set the AbsX location of the cell.  AbsX determines the true location of the cell for layout_section()

               esccell->AbsX = l.cursorx;
               esccell->AbsY = l.cursory;

               if (esctable->Thin) {
                  //if (esccell->Column IS 0);
                  //else esccell->AbsX += esctable->CellHSpacing;
               }
               else {
                  esccell->AbsX += esctable->CellHSpacing;
               }

               if (esccell->Column IS 0) esccell->AbsX += esctable->Thickness;

               esccell->Width  = esctable->Columns[esccell->Column].Width; // Minimum width for the cell's column
               esccell->Height = escrow->RowHeight;
               //DLAYOUT("%d / %d", escrow->MinHeight, escrow->RowHeight);

               DLAYOUT("Index %d, Processing cell at %dx %dy, size %dx%d, column %d", i, l.cursorx, l.cursory, esccell->Width, esccell->Height, esccell->Column);

               // Find the matching CELL_END

               LONG cell_end = i;
               while (Self->Stream[cell_end]) {
                  if (Self->Stream[cell_end] IS CTRL_CODE) {
                     if (ESCAPE_CODE(Self->Stream, cell_end) IS ESC_CELL_END) {
                        auto &end = escape_data<escCellEnd>(Self, cell_end);
                        if (end.CellID IS esccell->CellID) break;
                     }
                  }

                  NEXT_CHAR(Self->Stream, cell_end);
               }

               if (Self->Stream[cell_end] IS 0) {
                  log.warning("Failed to find matching cell-end.  Document stream is corrupt.");
                  goto exit;
               }

               i += l.len; // Go to start of cell content
               l.len = 0;

               if (i < cell_end) {
                  LONG segcount = Self->Segments.size();
                  auto savechar = Self->Stream[cell_end];
                  Self->Stream[cell_end] = 0;

                  if (!esccell->EditDef.empty()) Self->EditMode = true;
                  else Self->EditMode = false;

                     i = layout_section(Self, i, &l.font,
                            esccell->AbsX, esccell->AbsY,
                            &esccell->Width, &esccell->Height,
                            esctable->CellPadding, esctable->CellPadding, esctable->CellPadding, esctable->CellPadding, &vertical_repass);

                  if (!esccell->EditDef.empty()) Self->EditMode = false;

                  Self->Stream[cell_end] = savechar;

                  if (!esccell->EditDef.empty()) {
                     // Edit cells have a minimum width/height so that the user can still interact with them when empty.

                     if (LONG(Self->Segments.size()) IS segcount) {
                        // No content segments were created, which means that there's nothing for the cursor to attach
                        // itself too.

                        //do we really want to do something here?
                        //I'd suggest that we instead break up the segments a bit more???  ANother possibility - create an ESC_NULL
                        //type that gets placed at the start of the edit cell.  If there's no genuine content, then we at least have the ESC_NULL
                        //type for the cursor to be attached to?  ESC_NULL does absolutely nothing except act as faux content.


// TODO Work on this next




                     }

                     if (esccell->Width < 16) esccell->Width = 16;
                     if (esccell->Height < l.font->LineSpacing) {
                        esccell->Height = l.font->LineSpacing;
                     }
                  }
               }               

               if (!i) goto exit;

               DLAYOUT("Cell (%d:%d) is size %dx%d (min width %d)", esctable->RowIndex, esccell->Column, esccell->Width, esccell->Height, esctable->Columns[esccell->Column].Width);

               // Increase the overall width for the entire column if this cell has increased the column width.
               // This will affect the entire table, so a restart from TABLE_START is required.

               if (esctable->Columns[esccell->Column].Width < esccell->Width) {
                  DLAYOUT("Increasing column width of cell (%d:%d) from %d to %d (table_start repass required).", esctable->RowIndex, esccell->Column, esctable->Columns[esccell->Column].Width, esccell->Width);
                  esctable->Columns[esccell->Column].Width = esccell->Width; // This has the effect of increasing the minimum column width for all cells in the column

                  // Percentage based and zero columns need to be recalculated.  The easiest thing to do
                  // would be for a complete recompute (ComputeColumns = true) with the new minwidth.  The
                  // problem with ComputeColumns is that it does it all from scratch - we need to adjust it
                  // so that it can operate in a second style of mode where it recognises temporary width values.

                  esctable->Columns[esccell->Column].MinWidth = esccell->Width; // Column must be at least this size
                  esctable->ComputeColumns = 2;

                  esctable->ResetRowHeight = true; // Row heights need to be reset due to the width increase
                  RESTORE_STATE(tablestate);                  
                  goto wrap_table_cell;
               }

               // Advance the width of the entire row and adjust the row height

               esctable->RowWidth += esctable->Columns[esccell->Column].Width;

               if (!esctable->Thin) esctable->RowWidth += esctable->CellHSpacing;
               else if ((esccell->Column + esccell->ColSpan) < LONG(esctable->Columns.size())-1) esctable->RowWidth += esctable->CellHSpacing;

               if ((esccell->Height > escrow->RowHeight) or (escrow->VerticalRepass)) {
                  // A repass will be required if the row height has increased
                  // and objects or tables have been used in earlier cells, because
                  // objects need to know the final dimensions of their table cell.

                  if (esccell->Column IS LONG(esctable->Columns.size())-1) {
                     DLAYOUT("Extending row height from %d to %d (row repass required)", escrow->RowHeight, esccell->Height);
                  }

                  escrow->RowHeight = esccell->Height;
                  if ((esccell->Column + esccell->ColSpan) >= LONG(esctable->Columns.size())) {
                     RESTORE_STATE(rowstate);
                     goto repass_row_height_ext;
                  }
                  else escrow->VerticalRepass = true; // Make a note to do a vertical repass once all columns on this row have been processed
               }

               l.cursorx += esctable->Columns[esccell->Column].Width;

               if (!esctable->Thin) l.cursorx += esctable->CellHSpacing;
               else if ((esccell->Column + esccell->ColSpan) < LONG(esctable->Columns.size())) l.cursorx += esctable->CellHSpacing;

               if (esccell->Column IS 0) l.cursorx += esctable->Thickness;               
               break;
            }

            case ESC_CELL_END: {
               // CELL_END helps draw_document(), so set the segment to ensure that it is
               // included in the draw stream.  Please refer to ESC_CELL to see how content is
               // processed and how the cell dimensions are formed.

               l.setsegment = true;

               if ((esccell) and (!esccell->OnClick.empty())) {
                  add_link(Self, ESC_CELL, esccell, esccell->AbsX, esccell->AbsY, esccell->Width, esccell->Height, "esc_cell_end");
               }

               if ((esccell) and (!esccell->EditDef.empty())) {
                  // The area of each edit cell is logged for assisting interaction between
                  // the mouse pointer and the cells.

                  Self->EditCells.emplace_back(esccell->CellID, esccell->AbsX, esccell->AbsY, esccell->Width, esccell->Height);
               }

               break;
            }

            case ESC_PARAGRAPH_START: {
               escParagraph *parent;

               if ((parent = escpara)) {
                  DOUBLE ratio;

                  // If a paragraph is embedded within a paragraph, insert a newline before the new paragraph starts.

                  l.left_margin = parent->X; // Reset the margin so that the next line will be flush with the parent

                  if (l.paragraph_y > 0) {
                     if (escpara->LeadingRatio > escpara->VSpacing) ratio = escpara->LeadingRatio;
                     else ratio = escpara->VSpacing;
                  }
                  else ratio = escpara->VSpacing;

                  end_line(Self, &l, NL_PARAGRAPH, i, ratio, i, "Esc:PStart");

                  auto ptr = escpara;
                  escpara = &escape_data<escParagraph>(Self, i);
                  escpara->Stack = ptr;
               }
               else {
                  escpara = &escape_data<escParagraph>(Self, i);
                  escpara->Stack = NULL;

                  // Leading ratio is only used if the paragraph is preceeded by content.
                  // This check ensures that the first paragraph is always flush against
                  // the top of the page.

                  if ((escpara->LeadingRatio > 0) and (l.paragraph_y > 0)) {
                     end_line(Self, &l, NL_PARAGRAPH, i, escpara->LeadingRatio, i, "Esc:PStart");
                  }
               }

               // Indentation support

               if (esclist) {
                  // For list items, indentation is managed by the list that this paragraph is contained within.

                  if (escpara->ListItem) {
                     if (parent) escpara->Indent = esclist->BlockIndent;
                     escpara->ItemIndent = esclist->ItemIndent;
                     escpara->Relative = false;

                     if (!escpara->Value.empty()) {
                        LONG strwidth;
                        strwidth = fntStringWidth(l.font, escpara->Value.c_str(), -1) + 10;
                        if (strwidth > esclist->ItemIndent) {
                           esclist->ItemIndent = strwidth;
                           escpara->ItemIndent = strwidth;
                           esclist->Repass     = true;
                        }
                     }
                  }
                  else escpara->Indent = esclist->ItemIndent;
               }

               if (escpara->Indent) {
                  if (escpara->Relative) escpara->BlockIndent = escpara->Indent * 100 / *Width;
                  else escpara->BlockIndent = escpara->Indent;
               }

               escpara->X = l.left_margin + escpara->BlockIndent;

               l.left_margin += escpara->BlockIndent + escpara->ItemIndent;
               l.cursorx     += escpara->BlockIndent + escpara->ItemIndent;
               l.line_x      += escpara->BlockIndent + escpara->ItemIndent;

               // Paragraph management variables

               if (esclist) {
                  escpara->VSpacing = esclist->VSpacing;
               }

               escpara->Y = l.cursory;
               escpara->Height = 0;
               break;
            }

            case ESC_PARAGRAPH_END: {
               if (escpara) {
                  // The paragraph height reflects the true size of the paragraph after we take into account
                  // any objects and tables within the paragraph.

                  l.paragraph_end = escpara->Y + escpara->Height;

                  end_line(Self, &l, NL_PARAGRAPH, i, escpara->VSpacing, i + l.len, "Esc:PEnd");

                  l.left_margin = escpara->X - escpara->BlockIndent;
                  l.cursorx     = escpara->X - escpara->BlockIndent;
                  l.line_x      = escpara->X - escpara->BlockIndent;

                  escpara = escpara->Stack;
               }
               else end_line(Self, &l, NL_PARAGRAPH, i, escpara->VSpacing, i + l.len, "Esc:PEnd-NP");

               break;
            }
         }

         if (l.setsegment) {
            // Notice that this version of our call to add_drawsegment() does not define content position information (i.e. X/Y coordinates)
            // because we only expect to add an escape code to the drawing sequence, with the intention that the escape code carries
            // information relevant to the drawing process.  It is vital therefore that all content has been set with an earlier call
            // to add_drawsegment() before processing of the escape code.  See earlier in this routine.

            add_drawsegment(Self, i, i+l.len, &l, l.cursory, 0, 0, strCodes[(UBYTE)ESCAPE_CODE(Self->Stream, i)]); //"Esc:SetSegment");
            RESET_SEGMENT_WORD(i+l.len, l.cursorx, &l);
         }

         i += l.len;
      }
      else {
         // If the font character is larger or equal to the current line height, extend
         // the height for the current line.  Note that we go for >= because we want to
         // correct the base line in case there is an object already set on the line that
         // matches the font's line spacing.

         if (l.font->LineSpacing >= l.line_height) {
            l.line_height = l.font->LineSpacing;
            l.base_line   = l.font->Ascent;
         }

         if (Self->Stream[i] IS '\n') {
#if 0
            // This link code is likely going to be needed for a case such as :
            //   <a href="">blah blah <br/> blah </a>
            // But we haven't tested it in a rpl document yet.

            if ((l.link) and (l.link_open IS false)) {
               // A link is due to be closed
               add_link(Self, ESC_LINK, l.link, l.link_x, l.cursory, l.cursorx + l.wordwidth - l.link_x, l.line_height, "<br/>");
               l.link = NULL;
            }
#endif
            end_line(Self, &l, NL_PARAGRAPH, i+1 /* index */, 0 /* spacing */, i+1 /* restart-index */, "CarriageReturn");
           i++;
         }
         else if (Self->Stream[i] <= 0x20) {
            if (Self->Stream[i] IS '\t') {
               LONG tabwidth = (l.spacewidth + l.font->GlyphSpacing) * l.font->TabSize;
               if (tabwidth) l.cursorx += pf::roundup(l.cursorx, tabwidth);
               i++;
            }
            else {
               l.cursorx += l.wordwidth + l.spacewidth;
               i++;
            }

            l.kernchar  = 0;
            l.wordwidth = 0;
            l.textcontent = true;
         }
         else {
            LONG kerning;

            if (!l.wordwidth) l.wordindex = i;   // Record the index of the new word (if this is one)

            i += getutf8(Self->Stream.c_str()+i, &unicode);
            l.wordwidth += fntCharWidth(l.font, unicode, l.kernchar, &kerning);
            l.wordwidth += kerning;
            l.kernchar = unicode;
            l.textcontent = true;
         }
      }
   } // while(1)

   // Check if the cursor + any remaining text requires closure

   if ((l.cursorx + l.wordwidth > l.left_margin) or (l.wordindex != -1)) {
      end_line(Self, &l, NL_NONE, i, 0, i, "SectionEnd");
   }

exit:

   page_height = calc_page_height(Self, l.start_clips, AbsY, BottomMargin);

   // Force a second pass if the page height has increased and there are objects
   // on the page (the objects may need to know the page height - e.g. if there
   // is a gradient filling the background).
   //
   // This feature is also handled in ESC_CELL, so we only perform it here
   // if processing is occurring within the root page area (Offset of 0).

   if ((!Offset) and (object_vertical_repass) and (lastheight < page_height)) {
      DLAYOUT("============================================================");
      DLAYOUT("SECOND PASS [%d]: Root page height increased from %d to %d", Offset, lastheight, page_height);
      goto extend_page;
   }

   *Font = l.font;
   if (page_height > *Height) *Height = page_height;

   *VerticalRepass = object_vertical_repass;

   Self->Depth--;   
   return i;
}

//********************************************************************************************************************
// Calculate the page height, which is either going to be the coordinate of
// the bottom-most line, or one of the clipping regions if one of them
// extends further than the bottom-most line.

static LONG calc_page_height(extDocument *Self, LONG FirstClip, LONG Y, LONG BottomMargin)
{
   // Find the last segment that had text and use that to determine the bottom of the page

   LONG height = 0;
   LONG y = 0;
   LONG last = Self->Segments.size()-1;
   while ((last > 0) and (!height) and (!y)) {
      if (Self->Segments[last].TextContent) {
         height = Self->Segments[last].Height;
         y = Self->Segments[last].Y;
         break;
      }
      last--;
   }

   LONG page_height = (y + height);

   // Check clipping regions to see if they extend past the last line of text - if so, we extend the height.

   for (unsigned j=FirstClip; j < Self->Clips.size(); j++) {
      if (Self->Clips[j].Transparent) continue;
      if (Self->Clips[j].Clip.Bottom > page_height) page_height = Self->Clips[j].Clip.Bottom;
   }

   // Add the bottom margin and subtract the Y offset so that we have the true height of the page/cell.

   page_height = page_height + BottomMargin - Y;

/*
   log.trace("Page Height: %d + %d -> %d, Bottom: %d, Y: %d",
      Self->Segments[last].Y, Self->Segments[last].Height, page_height, BottomMargin, Y);
*/
   return page_height;
}

//********************************************************************************************************************

static void free_links(extDocument *Self)
{
   Self->Links.clear();
}

//********************************************************************************************************************
// Record a clickable link, cell, or other form of clickable area.

static void add_link(extDocument *Self, UBYTE EscapeCode, APTR Escape, LONG X, LONG Y, LONG Width, LONG Height, CSTRING Caller)
{
   pf::Log log(__FUNCTION__);

   if ((!Self) or (!Escape)) return;

   if ((Width < 1) or (Height < 1)) {
      log.traceWarning("Illegal width/height for link @ %dx%d, W/H %dx%d [%s]", X, Y, Width, Height, Caller);
      return;
   }

   DLAYOUT("%dx%d,%dx%d, %s", X, Y, Width, Height, Caller);

   Self->Links.emplace_back(EscapeCode, Escape, Self->Segments.size(), X, Y, Width, Height);
}

//********************************************************************************************************************

static void draw_background(extDocument *Self, objSurface *Surface, objBitmap *Bitmap)
{
   gfxDrawRectangle(Bitmap, 0, 0, Surface->Width, Surface->Height, Bitmap->packPixel(Self->Background), BAF::FILL);
}

//********************************************************************************************************************
// Note that this function also controls the drawing of objects that have loaded into the document (see the
// subscription hook in the layout process).

static void draw_document(extDocument *Self, objSurface *Surface, objBitmap *Bitmap)
{
   pf::Log log(__FUNCTION__);
   escList *esclist;
   escLink *esclink;
   escParagraph *escpara;
   escTable *esctable;
   escCell *esccell;
   escRow *escrow;
   escObject *escobject;
   RGB8 link_save_rgb;
   UBYTE tabfocus, oob, cursor_drawn;

   if (Self->UpdateLayout) {
      // Drawing is disabled if the layout needs to be updated (this likely indicates that the document stream has been
      // modified and has yet to be recalculated - drawing while in this state is liable to lead to a crash)

      return;
   }
   
   auto font = lookup_font(0, "draw_document");

   if (!font) {
      log.traceWarning("No default font defined.");
      return;
   }

   #ifdef _DEBUG
   if (Self->Stream.empty()) {
      log.traceWarning("No content in stream or no segments.");
      return;
   }
   #endif

   Self->CurrentCell = NULL;
   font->Bitmap = Bitmap;
   esclist  = NULL;
   escpara  = NULL;
   esctable = NULL;
   escrow   = NULL;
   esccell  = NULL;
   tabfocus = false;
   cursor_drawn = false;

   #ifdef GUIDELINES

      // Page boundary is marked in blue
      gfxDrawRectangle(Bitmap, Self->LeftMargin-1, Self->TopMargin-1,
         Self->CalcWidth - Self->RightMargin - Self->LeftMargin + 2, Self->PageHeight - Self->TopMargin - Self->BottomMargin + 2,
         Bitmap->packPixel(0, 0, 255), 0);

      // Special clip regions are marked in grey
      for (unsigned i=0; i < Self->Clips.size(); i++) {
         gfxDrawRectangle(Bitmap, Self->Clips[i].Clip.Left, Self->Clips[i].Clip.Top,
            Self->Clips[i].Clip.Right - Self->Clips[i].Clip.Left, Self->Clips[i].Clip.Bottom - Self->Clips[i].Clip.Top,
            Bitmap->packPixel(255, 200, 200), 0);
      }
   #endif

   LONG select_start  = -1;
   LONG select_end    = -1;
   LONG select_startx = 0;
   LONG select_endx   = 0;

   if ((Self->ActiveEditDef) and (Self->SelectIndex IS -1)) {
      select_start  = Self->CursorIndex;
      select_end    = Self->CursorIndex;
      select_startx = Self->CursorCharX;
      select_endx   = Self->CursorCharX;
   }
   else if ((Self->CursorIndex != -1) and (Self->SelectIndex != -1)) {
      if (Self->SelectIndex < Self->CursorIndex) {
         select_start  = Self->SelectIndex;
         select_end    = Self->CursorIndex;
         select_startx = Self->SelectCharX;
         select_endx   = Self->CursorCharX;
      }
      else {
         select_start  = Self->CursorIndex;
         select_end    = Self->SelectIndex;
         select_startx = Self->CursorCharX;
         select_endx   = Self->SelectCharX;
      }
   }

   auto alpha = Bitmap->Opacity;
   for (unsigned seg=0; seg < Self->Segments.size(); seg++) {
      auto &segment = Self->Segments[seg];

      // Don't process segments that are out of bounds.  This can't be applied to objects, as they can draw anywhere.

      oob = false;
      if (!segment.ObjectContent) {
         if (segment.Y >= Bitmap->Clip.Bottom) oob = true;
         if (segment.Y + segment.Height < Bitmap->Clip.Top) oob = true;
         if (segment.X + segment.Width < Bitmap->Clip.Left) oob = true;
         if (segment.X >= Bitmap->Clip.Right) oob = true;
      }

      // Highlighting of selected text

      if ((select_start <= segment.Stop) and (select_end > segment.Index)) {
         if (select_start != select_end) {
            Bitmap->Opacity = 80;
            if ((select_start > segment.Index) and (select_start < segment.Stop)) {
               if (select_end < segment.Stop) {
                  gfxDrawRectangle(Bitmap, segment.X + select_startx, segment.Y,
                     select_endx - select_startx, segment.Height, Bitmap->packPixel(0, 128, 0), BAF::FILL);
               }
               else {
                  gfxDrawRectangle(Bitmap, segment.X + select_startx, segment.Y,
                     segment.Width - select_startx, segment.Height, Bitmap->packPixel(0, 128, 0), BAF::FILL);
               }
            }
            else if (select_end < segment.Stop) {
               gfxDrawRectangle(Bitmap, segment.X, segment.Y, select_endx, segment.Height,
                  Bitmap->packPixel(0, 128, 0), BAF::FILL);
            }
            else {
               gfxDrawRectangle(Bitmap, segment.X, segment.Y, segment.Width, segment.Height,
                  Bitmap->packPixel(0, 128, 0), BAF::FILL);
            }
            Bitmap->Opacity = 255;
         }
      }

      if ((Self->ActiveEditDef) and (Self->CursorState) and (!cursor_drawn)) {
         if ((Self->CursorIndex >= segment.Index) and (Self->CursorIndex <= segment.Stop)) {
            if ((Self->CursorIndex IS segment.Stop) and (Self->Stream[Self->CursorIndex-1] IS '\n')); // The -1 looks naughty, but it works as CTRL_CODE != \n, so use of PREV_CHAR() is unnecessary
            else {
               if (gfxGetUserFocus() IS Self->PageID) { // Standard text cursor
                  gfxDrawRectangle(Bitmap, segment.X + Self->CursorCharX, segment.Y, 2, segment.BaseLine,
                     Bitmap->packPixel(255, 0, 0), BAF::FILL);
                  cursor_drawn = true;
               }
            }
         }
      }

      #ifdef GUIDELINES_CONTENT
         if (segment.TextContent) {
            gfxDrawRectangle(Bitmap,
               segment.X, segment.Y,
               (segment.Width > 0) ? segment.Width : 5, segment.Height,
               Bitmap->packPixel(0, 255, 0), 0);
         }
      #endif

      std::string strbuffer;
      strbuffer.reserve(segment.Stop - segment.Index + 1);

      auto fx = segment.X;
      auto i  = segment.Index;
      auto si = 0;

      while (i < segment.TrimStop) {
         if (Self->Stream[i] IS CTRL_CODE) {
            switch (ESCAPE_CODE(Self->Stream, i)) {
               case ESC_OBJECT: {
                  OBJECTPTR object;

                  escobject = &escape_data<escObject>(Self, i);

                  if ((escobject->Graphical) and (!escobject->Owned)) {
                     if (escobject->ObjectID < 0) {
                        object = NULL;
                        AccessObject(escobject->ObjectID, 3000, &object);
                     }
                     else object = GetObjectPtr(escobject->ObjectID);
/*
                     if (object) {
                        objLayout *layout;

                        if ((FindField(object, FID_Layout, NULL)) and (!object->getPtr(FID_Layout, &layout))) {
                           if (layout->DrawCallback.Type) {
                              // If the graphic is within a cell, ensure that the graphic does not exceed
                              // the dimensions of the cell.

                              if (Self->CurrentCell) {
                                 if (layout->BoundX + layout->BoundWidth > Self->CurrentCell->AbsX + Self->CurrentCell->Width) {
                                    layout->BoundWidth  = Self->CurrentCell->AbsX + Self->CurrentCell->Width - layout->BoundX;
                                 }

                                 if (layout->BoundY + layout->BoundHeight > Self->CurrentCell->AbsY + Self->CurrentCell->Height) {
                                    layout->BoundHeight = Self->CurrentCell->AbsY + Self->CurrentCell->Height - layout->BoundY;
                                 }
                              }

                              auto opacity = Bitmap->Opacity;
                              Bitmap->Opacity = 255;
                              auto routine = (void (*)(OBJECTPTR, rkSurface *, objBitmap *))layout->DrawCallback.StdC.Routine;
                              routine(object, Surface, Bitmap);
                              Bitmap->Opacity = opacity;
                           }
                        }

                        if (escobject->ObjectID < 0) ReleaseObject(object);
                     }
*/
                  }

                  break;
               }

               case ESC_FONT: {
                  auto &style = escape_data<escFont>(Self, i);
                  if (auto font = lookup_font(style.Index, "draw_document")) {
                     font->Bitmap = Bitmap;
                     if (tabfocus IS false) font->Colour = style.Colour;
                     else font->Colour = Self->SelectColour;

                     if ((style.Options & FSO::ALIGN_RIGHT) != FSO::NIL) font->Align = ALIGN::RIGHT;
                     else if ((style.Options & FSO::ALIGN_CENTER) != FSO::NIL) font->Align = ALIGN::HORIZONTAL;
                     else font->Align = ALIGN::NIL;

                     if ((style.Options & FSO::UNDERLINE) != FSO::NIL) font->Underline = font->Colour;
                     else font->Underline.Alpha = 0;
                  }
                  break;
               }

               case ESC_LIST_START:
                  if (esclist) {
                     auto ptr = esclist;
                     esclist = &escape_data<escList>(Self, i);
                     esclist->Stack = ptr;
                  }
                  else esclist = &escape_data<escList>(Self, i);
                  break;

               case ESC_LIST_END:
                  if (esclist) esclist = esclist->Stack;
                  break;

               case ESC_PARAGRAPH_START:
                  if (escpara) {
                     auto ptr = escpara;
                     escpara = &escape_data<escParagraph>(Self, i);
                     escpara->Stack = ptr;
                  }
                  else escpara = &escape_data<escParagraph>(Self, i);

                  if ((esclist) and (escpara->ListItem)) {
                     // Handling for paragraphs that form part of a list

                     if ((esclist->Type IS LT_CUSTOM) or (esclist->Type IS LT_ORDERED)) {
                        if (!escpara->Value.empty()) {
                           font->X = fx - escpara->ItemIndent;
                           font->Y = segment.Y + font->Leading + (segment.BaseLine - font->Ascent);
                           font->AlignWidth = segment.AlignWidth;
                           font->setString(escpara->Value);
                           font->draw();
                        }
                     }
                     else if (esclist->Type IS LT_BULLET) {
                        #define SIZE_BULLET 5
                        // TODO: Requires conversion to vector
                        //gfxDrawEllipse(Bitmap,
                        //   fx - escpara->ItemIndent, segment.Y + ((segment.BaseLine - SIZE_BULLET)/2),
                        //   SIZE_BULLET, SIZE_BULLET, Bitmap->packPixel(esclist->Colour), true);
                     }
                  }
                  break;

               case ESC_PARAGRAPH_END:
                  if (escpara) escpara = escpara->Stack;
                  break;

               case ESC_TABLE_START: {
                  if (esctable) {
                     auto ptr = esctable;
                     esctable = &escape_data<escTable>(Self, i);
                     esctable->Stack = ptr;
                  }
                  else esctable = &escape_data<escTable>(Self, i);

                  //log.trace("Draw Table: %dx%d,%dx%d", esctable->X, esctable->Y, esctable->Width, esctable->Height);

                  if (esctable->Colour.Alpha > 0) {
                     gfxDrawRectangle(Bitmap,
                        esctable->X+esctable->Thickness, esctable->Y+esctable->Thickness,
                        esctable->Width-(esctable->Thickness<<1), esctable->Height-(esctable->Thickness<<1),
                        Bitmap->packPixel(esctable->Colour), BAF::FILL|BAF::BLEND);
                  }

                  if (esctable->Shadow.Alpha > 0) {
                     Bitmap->Opacity = esctable->Shadow.Alpha;
                     for (LONG j=0; j < esctable->Thickness; j++) {
                        gfxDrawRectangle(Bitmap,
                           esctable->X+j, esctable->Y+j,
                           esctable->Width-(j<<1), esctable->Height-(j<<1),
                           Bitmap->packPixel(esctable->Shadow), BAF::NIL);
                     }
                     Bitmap->Opacity = alpha;
                  }
                  break;
               }

               case ESC_TABLE_END:
                  if (esctable) esctable = esctable->Stack;
                  break;

               case ESC_ROW: {
                  if (escrow) {
                     auto ptr = escrow;
                     escrow = &escape_data<escRow>(Self, i);
                     escrow->Stack = ptr;
                  }
                  else escrow = &escape_data<escRow>(Self, i);

                  if (escrow->Colour.Alpha) {
                     gfxDrawRectangle(Bitmap, esctable->X, escrow->Y, esctable->Width, escrow->RowHeight,
                        Bitmap->packPixel(escrow->Colour), BAF::FILL|BAF::BLEND);
                  }
                  break;
               }

               case ESC_ROW_END:
                  if (escrow) escrow = escrow->Stack;
                  break;

               case ESC_CELL: {
                  if (esccell) {
                     auto ptr = esccell;
                     esccell = &escape_data<escCell>(Self, i);
                     esccell->Stack = ptr;
                  }
                  else esccell = &escape_data<escCell>(Self, i);

                  Self->CurrentCell = esccell;

                  if (esccell->Colour.Alpha > 0) { // Fill colour
                     WORD border;
                     if (esccell->Shadow.Alpha > 0) border = 1;
                     else border = 0;

                     gfxDrawRectangle(Bitmap, esccell->AbsX+border, esccell->AbsY+border,
                        esctable->Columns[esccell->Column].Width-border, escrow->RowHeight-border,
                        Bitmap->packPixel(esccell->Colour), BAF::FILL|BAF::BLEND);
                  }

                  if (esccell->Shadow.Alpha > 0) { // Border colour
                     gfxDrawRectangle(Bitmap, esccell->AbsX, esccell->AbsY, esctable->Columns[esccell->Column].Width,
                        escrow->RowHeight, Bitmap->packPixel(esccell->Shadow), BAF::NIL);
                  }
                  break;
               }

               case ESC_CELL_END:
                  if (esccell) esccell = esccell->Stack;
                  Self->CurrentCell = esccell;
                  break;

               case ESC_LINK: {
                  esclink = &escape_data<escLink>(Self, i);
                  if (Self->HasFocus) {
                     if ((Self->Tabs[Self->FocusIndex].Type IS TT_LINK) and (Self->Tabs[Self->FocusIndex].Ref IS esclink->ID) and (Self->Tabs[Self->FocusIndex].Active)) {
                        link_save_rgb = font->Colour;
                        font->Colour = Self->SelectColour;
                        tabfocus = true;
                     }
                  }

                  break;
               }

               case ESC_LINK_END:
                  if (tabfocus) {
                     font->Colour = link_save_rgb;
                     tabfocus = false;
                  }
                  break;
            }

            i += ESCAPE_LEN;
         }
         else if (!oob) {
            if (Self->Stream[i] <= 0x20) { strbuffer[si++] = ' '; i++; }
            else strbuffer[si++] = Self->Stream[i++];

            // Print the string buffer content if the next string character is an escape code.

            if (Self->Stream[i] IS CTRL_CODE) {
               strbuffer[si] = 0;
               font->X = fx;
               font->Y = segment.Y + font->Leading + (segment.BaseLine - font->Ascent);
               font->AlignWidth = segment.AlignWidth;
               font->setString(strbuffer);
               font->draw();
               fx = font->EndX;
               si = 0;
            }
         }
         else i++;
      }

      strbuffer[si] = 0;

      if ((si > 0) and (!oob)) {
         font->X = fx;
         font->Y = segment.Y + font->Leading + (segment.BaseLine - font->Ascent);
         font->AlignWidth = segment.AlignWidth;
         font->setString(strbuffer);
         font->draw();
         fx = font->EndX;
      }
   } // for loop
}

//********************************************************************************************************************

static void draw_border(extDocument *Self, objSurface *Surface, objBitmap *Bitmap)
{
   if ((Self->BorderEdge IS DBE::NIL) or (Self->BorderEdge IS (DBE::TOP|DBE::BOTTOM|DBE::LEFT|DBE::RIGHT))) {
      gfxDrawRectangle(Bitmap, 0, 0, Surface->Width, Surface->Height, Bitmap->packPixel(Self->Border), BAF::NIL);
   }
   else {
      if ((Self->BorderEdge & DBE::TOP) != DBE::NIL) {
         gfxDrawRectangle(Bitmap, 0, 0, Surface->Width, 1, Bitmap->packPixel(Self->Border), BAF::NIL);
      }
      if ((Self->BorderEdge & DBE::LEFT) != DBE::NIL) {
         gfxDrawRectangle(Bitmap, 0, 0, 1, Surface->Height, Bitmap->packPixel(Self->Border), BAF::NIL);
      }
      if ((Self->BorderEdge & DBE::RIGHT) != DBE::NIL) {
         gfxDrawRectangle(Bitmap, Surface->Width-1, 0, 1, Surface->Height, Bitmap->packPixel(Self->Border), BAF::NIL);
      }
      if ((Self->BorderEdge & DBE::BOTTOM) != DBE::NIL) {
         gfxDrawRectangle(Bitmap, 0, Surface->Height-1, Surface->Width, 1, Bitmap->packPixel(Self->Border), BAF::NIL);
      }
   }
}

//********************************************************************************************************************

static ERROR keypress(extDocument *Self, KQ Flags, KEY Value, LONG Unicode)
{
   pf::Log log(__FUNCTION__);
   struct acScroll scroll;

   log.function("Value: %d, Flags: $%.8x, ActiveEdit: %p", LONG(Value), LONG(Flags), Self->ActiveEditDef);

   if ((Self->ActiveEditDef) and (gfxGetUserFocus() != Self->PageID)) {
      deactivate_edit(Self, true);
   }

   if (Self->ActiveEditDef) {
      reset_cursor(Self);

      if (Unicode) {
         // Delete any text that is selected

         if ((Self->SelectIndex != -1) and (Self->SelectIndex != Self->CursorIndex)) {
            if (Self->SelectIndex < Self->CursorIndex) {
               Self->Stream.erase(Self->SelectIndex, Self->CursorIndex - Self->SelectIndex);
               Self->CursorIndex = Self->SelectIndex;
            }
            else Self->Stream.erase(Self->CursorIndex, Self->SelectIndex - Self->CursorIndex);
            Self->SelectIndex = -1;
         }

         // Output the character

         char string[12];
         UTF8WriteValue(Unicode, string, sizeof(string));
         docInsertText(Self, string, Self->CursorIndex, true); // Will set UpdateLayout to true
         Self->CursorIndex += StrLength(string); // Reposition the cursor

         layout_doc_fast(Self);

         resolve_fontx_by_index(Self, Self->CursorIndex, &Self->CursorCharX);

         DRAW_PAGE(Self);
         return ERR_Okay;
      }

      switch(Value) {
         case KEY::TAB: {
            log.branch("Key: Tab");
            if (Self->TabFocusID) acFocus(Self->TabFocusID);
            else {
               if ((Flags & KQ::SHIFT) != KQ::NIL) advance_tabfocus(Self, -1);
               else advance_tabfocus(Self, 1);
            }
            break;
         }

         case KEY::ENTER: {
            // Delete any text that is selected

            if ((Self->SelectIndex != -1) and (Self->SelectIndex != Self->CursorIndex)) {
               if (Self->SelectIndex < Self->CursorIndex) {
                  Self->Stream.erase(Self->SelectIndex, Self->CursorIndex - Self->SelectIndex);
                  Self->CursorIndex = Self->SelectIndex;
               }
               else Self->Stream.erase(Self->CursorIndex, Self->SelectIndex - Self->CursorIndex);               

               Self->SelectIndex = -1;
            }

            docInsertXML(Self, "<br/>", Self->CursorIndex);
            NEXT_CHAR(Self->Stream, Self->CursorIndex);

            layout_doc_fast(Self);
            resolve_fontx_by_index(Self, Self->CursorIndex, &Self->CursorCharX);
            DRAW_PAGE(Self);
            break;
         }

         case KEY::LEFT: {
            Self->SelectIndex = -1;

            LONG index = Self->CursorIndex;
            if ((Self->Stream[index] IS CTRL_CODE) and (ESCAPE_CODE(Self->Stream, index) IS ESC_CELL)) {
               // Cursor cannot be moved any further left.  The cursor index should never end up here, but
               // better to be safe than sorry.

            }
            else {
               while (index > 0) {
                  PREV_CHAR(Self->Stream, index);
                  if (Self->Stream[index] IS CTRL_CODE) {
                     if (ESCAPE_CODE(Self->Stream, index) IS ESC_CELL) {
                        auto &cell = escape_data<escCell>(Self, index);
                        if (cell.CellID IS Self->ActiveEditCellID) break;
                     }
                     else if (ESCAPE_CODE(Self->Stream, index) IS ESC_OBJECT);
                     else continue;
                  }

                  if (!resolve_fontx_by_index(Self, index, &Self->CursorCharX)) {
                     Self->CursorIndex = index;
                     DRAW_PAGE(Self);
                     log.warning("LeftCursor: %d, X: %d", Self->CursorIndex, Self->CursorCharX);
                  }
                  break;
               }
            }
            break;
         }

         case KEY::RIGHT: {
            LONG code;

            Self->SelectIndex = -1;

            LONG index = Self->CursorIndex;
            while (Self->Stream[index]) {
               if (Self->Stream[index] IS CTRL_CODE) {
                  code = ESCAPE_CODE(Self->Stream, index);
                  if (code IS ESC_CELL_END) {
                     auto &cell_end = escape_data<escCellEnd>(Self, index);
                     if (cell_end.CellID IS Self->ActiveEditCellID) {
                        // End of editing zone - cursor cannot be moved any further right
                        break;
                     }
                  }
                  else if (code IS ESC_OBJECT); // Objects are treated as content, so do nothing special for these and drop through to next section
                  else {
                     NEXT_CHAR(Self->Stream, index);
                     continue;
                  }
               }

               // The current index references a content character or object.  Advance the cursor to the next index.

               NEXT_CHAR(Self->Stream, index);
               if (!resolve_fontx_by_index(Self, index, &Self->CursorCharX)) {
                  Self->CursorIndex = index;
                  DRAW_PAGE(Self);
                  log.warning("RightCursor: %d, X: %d", Self->CursorIndex, Self->CursorCharX);
               }
               break;
            }
            break;
         }

         case KEY::HOME: {
            break;
         }

         case KEY::END: {
            break;
         }

         case KEY::UP:
            break;

         case KEY::DOWN:
            break;

         case KEY::BACKSPACE: {
            LONG index = Self->CursorIndex;
            if ((Self->Stream[index] IS CTRL_CODE) and (ESCAPE_CODE(Self->Stream, index) IS ESC_CELL)) {
               // Cursor cannot be moved any further left
            }
            else {
               PREV_CHAR(Self->Stream, index);

               if ((Self->Stream[index] IS CTRL_CODE) and (ESCAPE_CODE(Self->Stream, index) IS ESC_CELL));
               else { // Delete the character/escape code
                  if ((Self->SelectIndex != -1) and (Self->SelectIndex != Self->CursorIndex)) {
                     if (Self->SelectIndex < Self->CursorIndex) { 
                        Self->Stream.erase(Self->SelectIndex, Self->CursorIndex - Self->SelectIndex);
                        Self->CursorIndex = Self->SelectIndex;
                     }
                     else {
                        Self->Stream.erase(index, Self->SelectIndex - index);
                        Self->CursorIndex = index;
                     }
                  }
                  else {
                     Self->Stream.erase(index, Self->CursorIndex - index);
                     Self->CursorIndex = index;
                  }

                  Self->SelectIndex = -1;
                  Self->UpdateLayout = true;
                  layout_doc_fast(Self);
                  resolve_fontx_by_index(Self, Self->CursorIndex, &Self->CursorCharX);
                  DRAW_PAGE(Self);

                  #ifdef DBG_STREAM
                     print_stream(Self, stream);
                  #endif
               }
            }
            break;
         }

         case KEY::DELETE: {
            LONG index = Self->CursorIndex;
            if ((Self->Stream[index] IS CTRL_CODE) and (ESCAPE_CODE(Self->Stream, index) IS ESC_CELL_END)) {
               // Not allowed to delete the end point
            }
            else {
               if ((Self->SelectIndex != -1) and (Self->SelectIndex != Self->CursorIndex)) {
                  if (Self->SelectIndex < Self->CursorIndex) { 
                     Self->Stream.erase(Self->SelectIndex, Self->CursorIndex - Self->SelectIndex); 
                     Self->CursorIndex = Self->SelectIndex;
                  }
                  else Self->Stream.erase(Self->CursorIndex, Self->SelectIndex - Self->CursorIndex);              
                  Self->SelectIndex = -1;
               }
               else {
                  auto end = index;
                  NEXT_CHAR(Self->Stream, end);
                  Self->Stream.erase(Self->CursorIndex, end - Self->CursorIndex);
               }

               Self->UpdateLayout = true;
               layout_doc_fast(Self);
               resolve_fontx_by_index(Self, Self->CursorIndex, &Self->CursorCharX);
               DRAW_PAGE(Self);

               #ifdef DBG_STREAM
                  print_stream(Self, stream);
               #endif
            }

            break;
         }

         default: break; // Ignore unhandled codes
      }
   }
   else switch (Value) {
      // NB: When not in edit mode, only the navigation keys are enabled
      case KEY::TAB:
         log.branch("Key: Tab");
         if (Self->TabFocusID) acFocus(Self->TabFocusID);
         else if ((Flags & KQ::SHIFT) != KQ::NIL) advance_tabfocus(Self, -1);
         else advance_tabfocus(Self, 1);
         break;

      case KEY::ENTER: {
         auto tab = Self->FocusIndex;
         if ((tab >= 0) and (unsigned(tab) < Self->Tabs.size())) {
            log.branch("Key: Enter, Tab: %d/%d, Type: %d", tab, LONG(Self->Tabs.size()), Self->Tabs[tab].Type);

            if ((Self->Tabs[tab].Type IS TT_LINK) and (Self->Tabs[tab].Active)) {
               for (auto &link : Self->Links) {
                  if ((link.EscapeCode IS ESC_LINK) and (link.Link->ID IS Self->Tabs[tab].Ref)) {
                     exec_link(Self, link);
                     break;
                  }
               }
            }
         }
         break;
      }

      case KEY::PAGE_DOWN:
         scroll.DeltaX = 0;
         scroll.DeltaY = Self->AreaHeight;
         scroll.DeltaZ = 0;
         QueueAction(AC_Scroll, Self->SurfaceID, &scroll);
         break;

      case KEY::PAGE_UP:
         scroll.DeltaX = 0;
         scroll.DeltaY = -Self->AreaHeight;
         scroll.DeltaZ = 0;
         QueueAction(AC_Scroll, Self->SurfaceID, &scroll);
         break;

      case KEY::LEFT:
         scroll.DeltaX = -10;
         scroll.DeltaY = 0;
         scroll.DeltaZ = 0;
         QueueAction(AC_Scroll, Self->SurfaceID, &scroll);
         break;

      case KEY::RIGHT:
         scroll.DeltaX = 10;
         scroll.DeltaY = 0;
         scroll.DeltaZ = 0;
         QueueAction(AC_Scroll, Self->SurfaceID, &scroll);
         break;

      case KEY::DOWN:
         scroll.DeltaX = 0;
         scroll.DeltaY = 10;
         scroll.DeltaZ = 0;
         QueueAction(AC_Scroll, Self->SurfaceID, &scroll);
         break;

      case KEY::UP:
         scroll.DeltaX = 0;
         scroll.DeltaY = -10;
         scroll.DeltaZ = 0;
         QueueAction(AC_Scroll, Self->SurfaceID, &scroll);
         break;

      default: break; // Ignore unhandled codes
   }

   return ERR_Okay;
}

//********************************************************************************************************************

static ERROR load_doc(extDocument *Self, std::string Path, bool Unload, BYTE UnloadFlags)
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

         AdjustLogLevel(3);
         Self->Error = process_page(Self, xml);
         AdjustLogLevel(-3);

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
// This function lays out the document so that it is ready to be drawn.  It calculates the position, pixel length and
// height of each line and rearranges any objects that are present in the document.

static void layout_doc(extDocument *Self)
{
   pf::Log log(__FUNCTION__);
   objFont *font;
   LONG pagewidth, hscroll_offset;
   bool vertical_repass;

   if (Self->UpdateLayout IS false) return;
   if (Self->Stream.empty()) return;

   // Initial height is 1, not the surface height because we want to accurately report the final height of the page.

   LONG pageheight = 1;

   DLAYOUT("Area: %dx%d,%dx%d Visible: %d ----------", Self->AreaX, Self->AreaY, Self->AreaWidth, Self->AreaHeight, Self->VScrollVisible);

   Self->BreakLoop = MAXLOOP;

restart:
   Self->BreakLoop--;

   hscroll_offset = 0;

   if (Self->PageWidth <= 0) {
      // If no preferred page width is set, maximise the page width to the available viewing area
      pagewidth = Self->AreaWidth - hscroll_offset;
   }
   else {
      if (!Self->RelPageWidth) { // Page width is fixed
         pagewidth = Self->PageWidth;
      }
      else { // Page width is relative
         pagewidth = (Self->PageWidth * (Self->AreaWidth - hscroll_offset)) / 100;
      }
   }

   if (pagewidth < Self->MinPageWidth) pagewidth = Self->MinPageWidth;

   Self->Segments.clear();
   Self->SortSegments.clear();
   Self->Clips.clear();
   Self->Links.clear();
   Self->EditCells.clear();
   Self->PageProcessed = false;
   Self->Error = ERR_Okay;
   Self->Depth = 0;

   if (!(font = lookup_font(0, "layout_doc"))) { // There is no content loaded for display      
      return;
   }

   layout_section(Self, 0, &font, 0, 0, &pagewidth, &pageheight, Self->LeftMargin, Self->TopMargin, Self->RightMargin,
      Self->BottomMargin, &vertical_repass);

   DLAYOUT("Section layout complete.");

   // If the resulting page width has increased beyond the available area, increase the MinPageWidth value to reduce
   // the number of passes required for the next time we do a layout.


   if ((pagewidth > Self->AreaWidth) and (Self->MinPageWidth < pagewidth)) Self->MinPageWidth = pagewidth;

   Self->PageHeight = pageheight;
//   if (Self->PageHeight < Self->AreaHeight) Self->PageHeight = Self->AreaHeight;
   Self->CalcWidth = pagewidth;

   // Recalculation may be required if visibility of the scrollbar needs to change.

   if ((Self->BreakLoop > 0) and (!Self->Error)) {
      if (Self->PageHeight > Self->AreaHeight) {
         // Page height is bigger than the surface, so the scrollbar needs to be visible.

         if (!Self->VScrollVisible) {
            DLAYOUT("Vertical scrollbar visibility needs to be enabled, restarting...");
            Self->VScrollVisible = true;
            Self->BreakLoop = MAXLOOP;
            goto restart;
         }
      }
      else {
         // Page height is smaller than the surface, so the scrollbar needs to be invisible.

         if (Self->VScrollVisible) {
            DLAYOUT("Vertical scrollbar needs to be invisible, restarting...");
            Self->VScrollVisible = false;
            Self->BreakLoop = MAXLOOP;
            goto restart;
         }
      }
   }

   // Look for clickable links that need to be aligned and adjust them (links cannot be aligned until the entire
   // width of their line is known, hence it's easier to make a final adjustment for all links post-layout).

   if (!Self->Error) {
      for (unsigned i=0; i < Self->Links.size(); i++) {
         if (Self->Links[i].EscapeCode != ESC_LINK) continue;

         auto link = &Self->Links[i];
         auto esclink = link->Link;
         if ((esclink->Align & (FSO::ALIGN_RIGHT|FSO::ALIGN_CENTER)) != FSO::NIL) {
            auto &segment = Self->Segments[link->Segment];
            if ((esclink->Align & FSO::ALIGN_RIGHT) != FSO::NIL) {
               link->X = segment.X + segment.AlignWidth - link->Width;
            }
            else if ((esclink->Align & FSO::ALIGN_CENTER) != FSO::NIL) {
               link->X = link->X + ((segment.AlignWidth - link->Width) / 2);
            }
         }
      }
   }

   // Build the sorted segment array

   if ((!Self->Error) and (!Self->Segments.empty())) {
      Self->SortSegments.resize(Self->Segments.size());
      unsigned seg, i, j;

      for (i=0, seg=0; seg < Self->Segments.size(); seg++) {
         if ((Self->Segments[seg].Height > 0) and (Self->Segments[seg].Width > 0)) {
            Self->SortSegments[i].Segment = seg;
            Self->SortSegments[i].Y       = Self->Segments[seg].Y;
            i++;
         }
      }

      // Shell sort

      unsigned h = 1;
      while (h < Self->SortSegments.size() / 9) h = 3 * h + 1;

      for (; h > 0; h /= 3) {
         for (i=h; i < Self->SortSegments.size(); i++) {
            SortSegment temp = Self->SortSegments[i];
            for (j=i; (j >= h) and (sortseg_compare(Self, Self->SortSegments[j - h], temp) < 0); j -= h) {
               Self->SortSegments[j] = Self->SortSegments[j - h];
            }
            Self->SortSegments[j] = temp;
         }
      }
   }

   Self->UpdateLayout = false;

#ifdef DBG_LINES
   print_lines(Self);
   //print_sorted_lines(Self);
   print_tabfocus(Self);
#endif

   // If an error occurred during layout processing, unload the document and display an error dialog.  (NB: While it is
   // possible to display a document up to the point at which the error occurred, we want to maintain a strict approach
   // so that human error is considered excusable in document formatting).

   if (Self->Error) {
      unload_doc(Self, ULD_REDRAW);

      std::string msg = "A failure occurred during the layout of this document - it cannot be displayed.\n\nDetails: ";
      if (Self->Error IS ERR_Loop) msg.append("This page cannot be rendered correctly due to its design.");
      else msg.append(GetErrorMsg(Self->Error));

      error_dialog("Document Layout Error", msg);
   }
   else {
      for (auto &trigger : Self->Triggers[DRT_AFTER_LAYOUT]) {
         if (trigger.Type IS CALL_SCRIPT) {
            const ScriptArg args[] = {
               { "ViewWidth",  Self->AreaWidth },
               { "ViewHeight", Self->AreaHeight },
               { "PageWidth",  Self->CalcWidth },
               { "PageHeight", Self->PageHeight }
            };
            scCallback(trigger.Script.Script, trigger.Script.ProcedureID, args, ARRAYSIZE(args), NULL);
         }
         else if (trigger.Type IS CALL_STDC) {
            auto routine = (void (*)(APTR, extDocument *, LONG, LONG, LONG, LONG))trigger.StdC.Routine;
            pf::SwitchContext context(trigger.StdC.Context);
            routine(trigger.StdC.Context, Self, Self->AreaWidth, Self->AreaHeight, Self->CalcWidth, Self->PageHeight);            
         }
      }
   }   
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
      Self->RestoreAttrib.clear();
      Self->TemplateArgs.clear();

      Self->XPosition    = 0;
      Self->YPosition    = 0;
      Self->ClickHeld    = false;
      Self->SelectStart  = 0;
      Self->SelectEnd    = 0;
      Self->UpdateLayout = true;
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
         insert_xml(Self, xml, page->Children[0], Self->Stream.size(), IXF_SIBLINGS|IXF_RESETSTYLE);
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
      // sequences (e.g. an unterminated ESC_TABLE sequence).

      if (Self->Error) unload_doc(Self, 0);

      Self->UpdateLayout = true;
      if (Self->initialised()) redraw(Self, true);

      Self->RestoreAttrib.clear();

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

   if ((!Self->Error) and (Self->MouseOver)) {
      DOUBLE x, y;
      if (!gfxGetRelativeCursorPos(Self->PageID, &x, &y)) {
         check_mouse_pos(Self, x, y);
      }
   }

   if (!Self->PageProcessed) {
      for (auto &trigger : Self->Triggers[DRT_PAGE_PROCESSED]) {
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

static ERROR unload_doc(extDocument *Self, BYTE Flags)
{
   pf::Log log(__FUNCTION__);

   log.branch("Flags: $%.2x", Flags);

   #ifdef DBG_STREAM
      print_stream(Self, Self->Stream);
   #endif

   log.trace("Resetting variables.");

   Self->FontColour   = { 0, 0, 0, 255 };
   Self->Highlight    = glHighlight;
   Self->CursorColour = { UBYTE(0.4  * 255), UBYTE(0.4 * 255), UBYTE (0.8 * 255), 255 };
   Self->LinkColour   = { 0, 0, 255, 255 };
   Self->Background   = { 255, 255, 255, 255 };
   Self->SelectColour = { 255, 0, 0, 255 };

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
   Self->DrawIntercept = 0;
   Self->FontSize      = DEFAULT_FONTSIZE;
   Self->FocusIndex    = -1;
   Self->PageProcessed = false;
   Self->MouseOverSegment = -1;
   Self->SelectIndex      = -1;
   Self->CursorIndex      = -1;
   Self->ActiveEditCellID = 0;
   Self->ActiveEditDef    = NULL;

   if (Self->ActiveEditDef) deactivate_edit(Self, false);   

   free_links(Self);

   if (Self->LinkIndex != -1) {
      Self->LinkIndex = -1;
      gfxRestoreCursor(PTC::DEFAULT, Self->UID);
   }

   if (Self->FontFace) FreeResource(Self->FontFace);
   if (Flags & ULD_TERMINATE) Self->FontFace = NULL;
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

   if (Flags & ULD_TERMINATE) Self->Vars.clear();   

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

      for (auto it = Self->Resources.begin(); it != Self->Resources.end(); it++) {
         if (ULD_TERMINATE) it->Terminate = true;
         if ((it->Type IS RT_PERSISTENT_SCRIPT) or (it->Type IS RT_PERSISTENT_OBJECT)) {
            // Persistent objects and scripts will survive refreshes
            if (Flags & ULD_REFRESH) { it++; continue; }
            else it = Self->Resources.erase(it);
         }
         else it = Self->Resources.erase(it);
      }
   }

   if (!Self->Templates) {
      if (!(Self->Templates = objXML::create::integral(fl::Name("xmlTemplates"), fl::Statement(glDefaultStyles),
         fl::Flags(XMF::PARSE_HTML|XMF::STRIP_HEADERS)))) return ERR_CreateObject;

      Self->TemplatesModified = Self->Templates->Modified;
   }

   Self->NoWhitespace = true; // Reset whitespace flag

   if (Self->PageID) acMoveToPoint(Self->PageID, 0, 0, 0, MTF::X|MTF::Y);

   Self->UpdateLayout = true;
   Self->GeneratedID = AllocateID(IDTYPE::GLOBAL);

   if (Flags & ULD_REDRAW) {
      DRAW_PAGE(Self);
   }

   return ERR_Okay;
}

//********************************************************************************************************************
// If the layout needs to be recalculated, set the UpdateLayout field before calling this function.

static void redraw(extDocument *Self, BYTE Focus)
{
   pf::Log log(__FUNCTION__);

   log.traceBranch("");

   //drwForbidDrawing();

      AdjustLogLevel(3);
      layout_doc(Self);
      AdjustLogLevel(-3);

   //drwPermitDrawing();

   DRAW_PAGE(Self);

   if ((Focus) and (Self->FocusIndex != -1)) set_focus(Self, -1, "redraw()");
}

//********************************************************************************************************************

#if 0
static LONG get_line_from_index(extDocument *Self, LONG Index)
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

static bool detect_recursive_dialog = false;

static void error_dialog(const std::string Title, const std::string Message)
{
   pf::Log log(__FUNCTION__);
   static OBJECTID dialog_id = 0;

   log.warning("%s", Message.c_str());

   if ((dialog_id) and (CheckObjectExists(dialog_id) IS ERR_True)) return;
   if (detect_recursive_dialog) return;
   detect_recursive_dialog = true;

   OBJECTPTR dialog;
   if (!NewObject(ID_SCRIPT, &dialog)) {
      dialog->setFields(fl::Name("scDialog"), fl::Owner(CurrentTaskID()), fl::Path("scripts:gui/dialog.fluid"));

      acSetVar(dialog, "modal", "1");
      acSetVar(dialog, "title", Title.c_str());
      acSetVar(dialog, "options", "okay");
      acSetVar(dialog, "type", "error");
      acSetVar(dialog, "message", Message.c_str());

      if ((!InitObject(dialog)) and (!acActivate(dialog))) {
         CSTRING *results;
         LONG size;
         if ((!GetFieldArray(dialog, FID_Results, (APTR *)&results, &size)) and (size > 0)) {
            dialog_id = StrToInt(results[0]);
         }
      }
   }

   detect_recursive_dialog = false;
}

static void error_dialog(const std::string Title, ERROR Error)
{
   pf::Log log(__FUNCTION__);
   static OBJECTID dialog_id = 0;

   log.warning("%s", GetErrorMsg(Error));
   
   if ((dialog_id) and (CheckObjectExists(dialog_id) IS ERR_True)) return;
   if (detect_recursive_dialog) return;
   detect_recursive_dialog = true;

   OBJECTPTR dialog;
   if (!NewObject(ID_SCRIPT, &dialog)) {
      dialog->setFields(fl::Name("scDialog"), fl::Owner(CurrentTaskID()), fl::Path("scripts:gui/dialog.fluid"));

      acSetVar(dialog, "modal", "1");
      acSetVar(dialog, "title", Title.c_str());
      acSetVar(dialog, "options", "okay");
      acSetVar(dialog, "type", "error");

      if (auto errstr = GetErrorMsg(Error)) {
         std::string buffer("Error: ");
         buffer.append(errstr);
         acSetVar(dialog, "message", buffer.c_str());
      }

      if ((!InitObject(dialog)) and (!acActivate(dialog))) {
         CSTRING *results;
         LONG size;
         if ((!GetFieldArray(dialog, FID_Results, (APTR *)&results, &size)) and (size > 0)) {
            dialog_id = StrToInt(results[0]);
         }
      }
   }

   detect_recursive_dialog = false;
}

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
// Converts a font index into a font structure.

static objFont * lookup_font(LONG Index, const std::string &Caller)
{
   if ((unsigned(Index) < glFonts.size()) and (Index >= 0)) return glFonts[Index].Font;
   else {
      pf::Log log(__FUNCTION__);
      log.warning("Bad font index %d.  Max: %d.  Caller: %s", Index, LONG(glFonts.size()), Caller.c_str());
      if (!glFonts.empty()) return glFonts[0].Font; // Always try to return a font rather than NULL
      else return NULL;
   }
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

   // If we already have loaded this font, return it.

   for (unsigned i=0; i < glFonts.size(); i++) {
      if ((!StrMatch(Face, glFonts[i].Font->Face)) and
          (!StrMatch(Style, glFonts[i].Font->Style)) and
          (Point IS glFonts[i].Point)) {
         log.trace("Match %d = %s(%s,%d)", i, Face.c_str(), Style.c_str(), Point);
         return i;
      }
   }

   log.branch("Index: %d, %s, %s, %d", LONG(glFonts.size()), Face.c_str(), Style.c_str(), Point);

   AdjustLogLevel(2);

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
            AdjustLogLevel(-2);
            return i;
         }
      }

      auto index = glFonts.size();
      glFonts.emplace_back(font, Point);
      AdjustLogLevel(-2);
      return index;
   }
   else {
      AdjustLogLevel(-2);
      return -1;
   }
}

//********************************************************************************************************************
// This function creates segments, which are used during the drawing process as well as user interactivity, e.g. to
// determine the character that the mouse is positioned over.  A segment will usually consist of a sequence of
// text characters or escape sequences.
//
// Offset: The start of the line within the stream.
// Stop:   The stream index at which the line stops.

static void add_drawsegment(extDocument *Self, LONG Offset, LONG Stop, layout *Layout,
   LONG Y, LONG Width, LONG AlignWidth, const std::string &Debug)
{
   pf::Log log(__FUNCTION__);
   LONG i;

   // Determine trailing whitespace at the end of the line.  This helps
   // to prevent situations such as underlining occurring in whitespace
   // at the end of the line during word-wrapping.

   auto trimstop = Stop;
   while ((Self->Stream[trimstop-1] <= 0x20) and (trimstop > Offset)) {
      if (Self->Stream[trimstop-1] IS CTRL_CODE) break;
      trimstop--;
   }

   if (Offset >= Stop) {
      DLAYOUT("Cancelling addition, no content in line to add (bytes %d-%d) \"%.20s\" (%s)", Offset, Stop, printable(Self, Offset).c_str(), Debug.c_str());
      return;
   }

   // Check the new segment to see if there are any text characters or escape codes relevant to drawing

   bool text_content    = false;
   bool control_content = false;
   bool object_content  = false;
   bool allow_merge     = true;
   for (i=Offset; i < Stop;) {
      if (Self->Stream[i] IS CTRL_CODE) {
         auto code = ESCAPE_CODE(Self->Stream, i);
         control_content = true;
         if (code IS ESC_OBJECT) object_content = true;
         if ((code IS ESC_OBJECT) or (code IS ESC_TABLE_START) or (code IS ESC_TABLE_END) or (code IS ESC_FONT)) {
            allow_merge = false;
         }
      }
      else {
         text_content = true;
         allow_merge = false;
      }

      NEXT_CHAR(Self->Stream, i);
   }

   auto Height   = Layout->line_height;
   auto BaseLine = Layout->base_line;
   if (text_content) {
      if (Height <= 0) {
         // No line-height given and there is text content - use the most recent font to determine the line height
         Height   = Layout->font->LineSpacing;
         BaseLine = Layout->font->Ascent;
      }
      else if (!BaseLine) { // If base-line is missing for some reason, define it
         BaseLine = Layout->font->Ascent;
      }
   }
   else {
      if (Height <= 0) Height = 0;
      if (BaseLine <= 0) BaseLine = 0;
   }

#ifdef DBG_STREAM
   DLAYOUT("#%d, Bytes: %d-%d, Area: %dx%d,%d:%dx%d, WordWidth: %d, CursorY: %d, [%.20s]...[%.20s] (%s)",
      Self->SegCount, Offset, Stop, Layout->line_x, Y, Width, AlignWidth, Height, Layout->wordwidth,
      Layout->cursory, printable(Self, Offset, Stop-Offset).c_str(), printable(Self, Stop).c_str(), Debug);
#endif

   DocSegment segment;
   auto x = Layout->line_x;

   if ((!Self->Segments.empty()) and (Offset < Self->Segments.back().Stop)) {
      // Patching: If the start of the new segment is < the end of the previous segment,
      // adjust the previous segment so that it stops at the beginning of our new segment.
      // This prevents overlapping between segments and the two segments will be patched
      // together in the next section of this routine.

      if (Offset <= Self->Segments.back().Index) {
         // If the start of the new segment retraces to an index that has already been configured,
         // then we have actually encountered a coding flaw and the caller should be investigated.

         log.warning("(%s) New segment #%d retraces to index %d, which has been configured by previous segments.", Debug.c_str(), Self->Segments.back().Index, Offset);
         return;
      }
      else {
         DLAYOUT("New segment #%d start index is less than (%d < %d) the end of previous segment - will patch up.", Self->Segments.back().Index, Offset, Self->Segments.back().Stop);
         Self->Segments.back().Stop = Offset;
      }
   }

   // Is the new segment a continuation of the previous one, and does the previous segment contain content?
   if ((allow_merge) and (!Self->Segments.empty()) and (Self->Segments.back().Stop IS Offset) and
       (Self->Segments.back().AllowMerge)) {
      // We are going to extend the previous line rather than add a new one, as the two
      // segments only contain control codes.

      segment = Self->Segments.back();
      Self->Segments.pop_back();

      Offset = segment.Index;
      x      = segment.X;
      Width += segment.Width;
      AlignWidth += segment.AlignWidth;
      if (segment.Height > Height) {
         Height = segment.Height;
         BaseLine = segment.BaseLine;
      }
   }

#ifdef _DEBUG
   // If this is a segmented line, check if any previous entries have greater
   // heights.  If so, this is considered an internal programming error.

   if ((Layout->split_start != NOTSPLIT) and (Height > 0)) {
      for (i=Layout->split_start; i < Offset; i++) {
         if (Self->Segments[i].Depth != Self->Depth) continue;
         if (Self->Segments[i].Height > Height) {
            log.warning("A previous entry in segment %d has a height larger than the new one (%d > %d)", i, Self->Segments[i].Height, Height);
            BaseLine = Self->Segments[i].BaseLine;
            Height = Self->Segments[i].Height;
         }
      }
   }
#endif

   segment.Index    = Offset;
   segment.Stop     = Stop;
   segment.TrimStop = trimstop;
   segment.X        = x;
   segment.Y        = Y;
   segment.Height   = Height;
   segment.BaseLine = BaseLine;
   segment.Width    = Width;
   segment.Depth    = Self->Depth;
   segment.AlignWidth     = AlignWidth;
   segment.TextContent    = text_content;
   segment.ControlContent = control_content;
   segment.ObjectContent  = object_content;
   segment.AllowMerge     = allow_merge;
   segment.Edit           = Self->EditMode;

   // If a line is segmented, we need to backtrack for earlier line segments and ensure that their height and baseline
   // is matched to that of the last line (which always contains the maximum height and baseline values).

   if ((Layout->split_start != NOTSPLIT) and (Height)) {
      if (LONG(Self->Segments.size()) != Layout->split_start) {
         DLAYOUT("Resetting height (%d) & base (%d) of segments index %d-%d.", Height, BaseLine, segment.Index, Layout->split_start);
         for (unsigned i=Layout->split_start; i < Self->Segments.size(); i++) {
            if (Self->Segments[i].Depth != Self->Depth) continue;
            Self->Segments[i].Height = Height;
            Self->Segments[i].BaseLine = BaseLine;
         }
      }
   }

   Self->Segments.emplace_back(segment);
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
               if (auto font = lookup_font(Self->Style.FontStyle.Index, "convert_xml")) {
                  Output.replace(pos, sizeof("[%font]")-1, std::string(font->Face) + ":" + std::to_string(font->Point) + ":" + font->Style);
               }
            }
            else if (!Output.compare(pos, std::string::npos, "[%fontface]")) {
               if (auto font = lookup_font(Self->Style.FontStyle.Index, "convert_xml")) {
                  Output.replace(pos, sizeof("[%fontface]")-1, font->Face);
               }
            }
            else if (!Output.compare(pos, std::string::npos, "[%fontcolour]")) {
               if (auto font = lookup_font(Self->Style.FontStyle.Index, "convert_xml")) {
                  char colour[28];
                  snprintf(colour, sizeof(colour), "#%.2x%.2x%.2x%.2x", font->Colour.Red, font->Colour.Green, font->Colour.Blue, font->Colour.Alpha);
                  Output.replace(pos, sizeof("[%fontcolour]")-1, colour);
               }
            }
            else if (!Output.compare(pos, std::string::npos, "[%fontsize]")) {
               if (auto font = lookup_font(Self->Style.FontStyle.Index, "convert_xml")) {
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

static ERROR activate_edit(extDocument *Self, LONG CellIndex, LONG CursorIndex)
{
   pf::Log log(__FUNCTION__);
   auto &stream = Self->Stream;

   if ((CellIndex < 0) or (CellIndex >= LONG(Self->Stream.size()))) return log.warning(ERR_OutOfRange);

   log.branch("Cell Index: %d, Cursor Index: %d", CellIndex, CursorIndex);

   // Check the validity of the index

   if ((stream[CellIndex] != CTRL_CODE) or (ESCAPE_CODE(stream, CellIndex) != ESC_CELL)) {
      return log.warning(ERR_Failed);
   }

   auto &cell = escape_data<escCell>(Self, CellIndex);
   if (CursorIndex <= 0) { // Go to the start of the cell content
      CursorIndex = CellIndex;
      NEXT_CHAR(stream, CursorIndex);
   }

   // Skip any non-content control codes - it's always best to place the cursor ahead of things like
   // font styles, paragraph formatting etc.

   while (CursorIndex < LONG(Self->Stream.size())) {
      if (stream[CursorIndex] IS CTRL_CODE) {
         if (ESCAPE_CODE(stream, CursorIndex) IS ESC_CELL_END) break;
         else if (ESCAPE_CODE(stream, CursorIndex) IS ESC_TABLE_START) break;
         else if (ESCAPE_CODE(stream, CursorIndex) IS ESC_OBJECT) break;
         else if (ESCAPE_CODE(stream, CursorIndex) IS ESC_LINK_END) break;
         else if (ESCAPE_CODE(stream, CursorIndex) IS ESC_PARAGRAPH_END) break;
      }
      else break;

      NEXT_CHAR(stream, CursorIndex);
   }

   auto it = Self->EditDefs.find(cell.EditDef);
   if (it IS Self->EditDefs.end()) return log.warning(ERR_Search);

   deactivate_edit(Self, false);

   auto &edit = it->second;
   if (!edit.OnChange.empty()) { // Calculate a CRC for the cell content
      unsigned i = CellIndex;
      while (i < Self->Stream.size()) {
         if ((stream[i] IS CTRL_CODE) and (ESCAPE_CODE(stream, i) IS ESC_CELL_END)) {
            auto &end = escape_data<escCellEnd>(Self, i);
            if (end.CellID IS cell.CellID) {
               Self->ActiveEditCRC = GenCRC32(0, stream.data() + CellIndex, i - CellIndex);
               break;
            }
         }
         NEXT_CHAR(stream, i);
      }
   }

   Self->ActiveEditCellID = cell.CellID;
   Self->ActiveEditDef = &edit;
   Self->CursorIndex   = CursorIndex;
   Self->SelectIndex   = -1;

   log.msg("Activated cell %d, cursor index %d, EditDef: %p, CRC: $%.8x", Self->ActiveEditCellID, Self->CursorIndex, Self->ActiveEditDef, Self->ActiveEditCRC);

   // Set the focus index to the relevant TT_EDIT entry

   for (unsigned tab=0; tab < Self->Tabs.size(); tab++) {
      if ((Self->Tabs[tab].Type IS TT_EDIT) and (Self->Tabs[tab].Ref IS cell.CellID)) {
         Self->FocusIndex = tab;
         break;
      }
   }

   resolve_fontx_by_index(Self, Self->CursorIndex, &Self->CursorCharX);

   reset_cursor(Self); // Reset cursor flashing

   // User callbacks

   if (!edit.OnEnter.empty()) {
      OBJECTPTR script;
      std::string function_name, argstring;

      log.msg("Calling onenter callback function.");

      if (!extract_script(Self, edit.OnEnter, &script, function_name, argstring)) {
         ScriptArg args[] = { { "ID", edit.Name } };
         scExec(script, function_name.c_str(), args, ARRAYSIZE(args));
      }
   }

   DRAW_PAGE(Self);
   return ERR_Okay;
}

//********************************************************************************************************************

static void deactivate_edit(extDocument *Self, BYTE Redraw)
{
   pf::Log log(__FUNCTION__);

   if (!Self->ActiveEditDef) return;

   log.branch("Redraw: %d, CellID: %d", Redraw, Self->ActiveEditCellID);

   if (Self->FlashTimer) {
      UpdateTimer(Self->FlashTimer, 0); // Turn off the timer
      Self->FlashTimer = 0;
   }

   // The edit tag needs to be found so that we can determine if OnExit needs to be called or not.

   auto edit = Self->ActiveEditDef;
   LONG cell_index = find_cell(Self, Self->ActiveEditCellID);

   Self->ActiveEditCellID = 0;
   Self->ActiveEditDef = NULL;
   Self->CursorIndex = -1;
   Self->SelectIndex = -1;

   if (Redraw) DRAW_PAGE(Self);

   if (cell_index >= 0) {
      if (!edit->OnChange.empty()) {
         escCell &cell = escape_data<escCell>(Self, cell_index);

         // CRC comparison - has the cell content changed?

         auto i = unsigned(cell_index);
         while (i < Self->Stream.size()) {
            if ((Self->Stream[i] IS CTRL_CODE) and (ESCAPE_CODE(Self->Stream, i) IS ESC_CELL_END)) {
               auto &end = escape_data<escCellEnd>(Self, i);
               if (end.CellID IS cell.CellID) {
                  auto crc = GenCRC32(0, Self->Stream.data() + cell_index, i - cell_index);
                  if (crc != Self->ActiveEditCRC) {
                     log.trace("Change detected in editable cell %d", cell.CellID);

                     OBJECTPTR script;
                     std::string function_name, argstring;
                     if (!extract_script(Self, edit->OnChange, &script, function_name, argstring)) {
                        auto cell_content = cell_index;
                        NEXT_CHAR(Self->Stream, cell_content);

                        std::vector<ScriptArg> args = {
                           ScriptArg("CellID", edit->Name),
                           ScriptArg("Start", cell_content),
                           ScriptArg("End", i)
                        };

                        for (auto &cell_arg : cell.Args) args.emplace_back("", cell_arg.second);

                        scExec(script, function_name.c_str(), args.data(), args.size());
                     }
                  }

                  break;
               }
            }
            NEXT_CHAR(Self->Stream, i);
         }
      }

      if (!edit->OnExit.empty()) {



      }
   }
   else log.warning("Failed to find cell ID %d", Self->ActiveEditCellID);
}

//********************************************************************************************************************
// Sends motion events for zones that the mouse pointer has departed.

static void check_pointer_exit(extDocument *Self, LONG X, LONG Y)
{
   for (auto it = Self->MouseOverChain.begin(); it != Self->MouseOverChain.end(); ) {
      if ((X < it->Left) or (Y < it->Top) or (X >= it->Right) or (Y >= it->Bottom)) {
         // Pointer has left this zone

         std::string function_name, argstring;
         OBJECTPTR script;
         if (!extract_script(Self, it->Function, &script, function_name, argstring)) {
            const ScriptArg args[] = {
               { "Element", it->ElementID },
               { "Status",  0 },
               { "Args",    argstring }
            };

            scExec(script, function_name.c_str(), args, ARRAYSIZE(args));
         }

         it = Self->MouseOverChain.erase(it);
      }
      else it++;
   }
}

//********************************************************************************************************************

static void check_mouse_click(extDocument *Self, DOUBLE X, DOUBLE Y)
{
   pf::Log log(__FUNCTION__);

   Self->ClickX = X;
   Self->ClickY = Y;
   Self->ClickSegment = Self->MouseOverSegment;

   auto segment = Self->MouseOverSegment;

   if (segment IS -1) {
      // The mouse is not positioned over a segment.  Check if the mosue is positioned within
      // an editing cell.  If it is, we need to find the segment nearest to the mouse pointer
      // and position the cursor at the end of that segment.

      unsigned i, sortseg;

      for (i=0; i < Self->EditCells.size(); i++) {
         if ((X >= Self->EditCells[i].X) and (X < Self->EditCells[i].X + Self->EditCells[i].Width) and
             (Y >= Self->EditCells[i].Y) and (Y < Self->EditCells[i].Y + Self->EditCells[i].Height)) {
            break;
         }
      }

      if (i < Self->EditCells.size()) {
         // Mouse is within an editable segment.  Find the start and ending indexes of the editable area

         LONG cell_start, cell_end, last_segment;

         cell_start = find_cell(Self, Self->EditCells[i].CellID);
         cell_end = cell_start;
         while (Self->Stream[cell_end]) {
            if (Self->Stream[cell_end] IS CTRL_CODE) {
               if (ESCAPE_CODE(Self->Stream, cell_end) IS ESC_CELL_END) {
                  auto end = escape_data<escCellEnd>(Self, cell_end);
                  if (end.CellID IS Self->EditCells[i].CellID) break;
               }
            }

            NEXT_CHAR(Self->Stream, cell_end);
         }

         if (!Self->Stream[cell_end]) return; // No matching cell end - document stream is corrupt

         log.warning("Analysing cell area %d - %d", cell_start, cell_end);

         last_segment = -1;
         for (sortseg=0; sortseg < Self->SortSegments.size(); sortseg++) {
            LONG seg = Self->SortSegments[sortseg].Segment;
            if ((Self->Segments[seg].Index >= cell_start) and (Self->Segments[seg].Stop <= cell_end)) {
               last_segment = seg;
               // Segment found.  Break if the segment's vertical position is past the mouse pointer
               if (Y < Self->Segments[seg].Y) break;
               if ((Y >= Self->Segments[seg].Y) and (X < Self->Segments[seg].X)) break;
            }
         }

         if (last_segment != -1) {
            // Set the cursor to the end of the nearest segment
            log.warning("Last seg: %d", last_segment);
            Self->CursorCharX = Self->Segments[last_segment].X + Self->Segments[last_segment].Width;
            Self->SelectCharX = Self->CursorCharX;

            // A click results in the deselection of existing text

            if (Self->CursorIndex != -1) deselect_text(Self);

            Self->CursorIndex = Self->Segments[last_segment].Stop;
            Self->SelectIndex = -1; //Self->Segments[last_segment].Stop;

            activate_edit(Self, cell_start, Self->CursorIndex);
         }

         return;
      }
      else log.warning("Mouse not within an editable cell.");
   }

   if (segment != -1) {
      LONG bytepos;
      if (!resolve_font_pos(Self, segment, X, &Self->CursorCharX, &bytepos)) {
         if (Self->CursorIndex != -1) deselect_text(Self); // A click results in the deselection of existing text

         if (!Self->Segments[segment].Edit) deactivate_edit(Self, true);

         // Set the new cursor information

         Self->CursorIndex = Self->Segments[segment].Index + bytepos;
         Self->SelectIndex = -1; //Self->Segments[segment].Index + bytepos; // SelectIndex is for text selections where the user holds the LMB and drags the mouse
         Self->SelectCharX = Self->CursorCharX;

         log.msg("User clicked on point %.2fx%.2f in segment %d, cursor index: %d, char x: %d", X, Y, segment, Self->CursorIndex, Self->CursorCharX);

         if (Self->Segments[segment].Edit) {
            // If the segment is editable, we'll have to turn on edit mode so
            // that the cursor flashes.  Work backwards to find the edit cell.

            auto cellindex = Self->Segments[segment].Index;
            while (cellindex > 0) {
               if ((Self->Stream[cellindex] IS CTRL_CODE) and (ESCAPE_CODE(Self->Stream, cellindex) IS ESC_CELL)) {
                  auto &cell = escape_data<escCell>(Self, cellindex);
                  if (!cell.EditDef.empty()) {
                     activate_edit(Self, cellindex, Self->CursorIndex);
                     break;
                  }
               }
               PREV_CHAR(Self->Stream, cellindex);
            }
         }
      }
   }
   else {
      if (Self->CursorIndex != -1) {
         deselect_text(Self);
         deactivate_edit(Self, true);
      }
   }
}

//********************************************************************************************************************

static void check_mouse_release(extDocument *Self, DOUBLE X, DOUBLE Y)
{
   if ((std::abs(X - Self->ClickX) > 3) or (std::abs(Y - Self->ClickY) > 3)) {
      pf::Log log(__FUNCTION__);
      log.trace("User click cancelled due to mouse shift.");
      return;
   }

   if (Self->LinkIndex != -1) exec_link(Self, Self->LinkIndex);
}

//********************************************************************************************************************

static void check_mouse_pos(extDocument *Self, DOUBLE X, DOUBLE Y)
{
   DocEdit *edit;

   Self->MouseOverSegment = -1;
   Self->PointerX = X;
   Self->PointerY = Y;

   check_pointer_exit(Self, X, Y); // For function callbacks

   if (Self->MouseOver) {
      unsigned row;
      for (row=0; (row < Self->SortSegments.size()) and (Y < Self->SortSegments[row].Y); row++);

      for (; row < Self->SortSegments.size(); row++) {
         if ((Y >= Self->SortSegments[row].Y) and (Y < Self->SortSegments[row].Y + Self->Segments[Self->SortSegments[row].Segment].Height)) {
            if ((X >= Self->Segments[Self->SortSegments[row].Segment].X) and (X < Self->Segments[Self->SortSegments[row].Segment].X + Self->Segments[Self->SortSegments[row].Segment].Width)) {
               Self->MouseOverSegment = Self->SortSegments[row].Segment;
               break;
            }
         }
      }
   }

   // If the user is holding the mouse button and moving it around, we need to highlight the selected text.

   if ((Self->LMB) and (Self->CursorIndex != -1)) {
      if (Self->SelectIndex IS -1) Self->SelectIndex = Self->CursorIndex;

      if (Self->MouseOverSegment != -1) {
         LONG bytepos, cursor_x, cursor_index;
         if (!resolve_font_pos(Self, Self->MouseOverSegment, X, &cursor_x, &bytepos)) {
            cursor_index = Self->Segments[Self->MouseOverSegment].Index + bytepos;

            if ((edit = Self->ActiveEditDef)) {
               // For select-dragging, we must check that the selection is within the bounds of the editing area.

               if (auto i = find_cell(Self, Self->ActiveEditCellID); i >= 0) {
                  NEXT_CHAR(Self->Stream, i);
                  if (cursor_index < i) {
                     // If the cursor index precedes the start of the editing area, reset it

                     if (!resolve_fontx_by_index(Self, i, &cursor_x)) {
                        cursor_index = i;
                     }
                  }
                  else {
                     // If the cursor index exceeds the end of the editing area, reset it

                     while (i < LONG(Self->Stream.size())) {
                        if ((Self->Stream[i] IS CTRL_CODE) and (ESCAPE_CODE(Self->Stream, i) IS ESC_CELL_END)) {
                           auto &cell_end = escape_data<escCellEnd>(Self, i);
                           if (cell_end.CellID IS Self->ActiveEditCellID) {                             
                              if (auto seg = find_segment(Self, i, false); seg > 0) {
                                 seg--;
                                 i = Self->Segments[seg].Stop;
                                 if (cursor_index > i) {
                                    if (!resolve_fontx_by_index(Self, i, &cursor_x)) {
                                       cursor_index = i;
                                    }
                                 }
                              }
                              break;
                           }
                        }
                        NEXT_CHAR(Self->Stream, i);
                     }
                  }

                  Self->CursorIndex = cursor_index;
                  Self->CursorCharX = cursor_x;
               }
               else deactivate_edit(Self, false);
            }
            else {
               Self->CursorIndex = cursor_index;
               Self->CursorCharX = cursor_x;
            }

            DRAW_PAGE(Self);
         }
      }
   }

   // Check if the user moved onto a link

   if ((Self->MouseOver) and (!Self->LMB)) {
      for (auto i = LONG(Self->Links.size())-1; i >= 0; i--) { // Search from front to back
         if ((X >= Self->Links[i].X) and (Y >= Self->Links[i].Y) and
             (X < Self->Links[i].X + Self->Links[i].Width) and
             (Y < Self->Links[i].Y + Self->Links[i].Height)) {
            // The mouse pointer is inside a link

            if (Self->LinkIndex IS -1) {
               gfxSetCursor(0, CRF::BUFFER, PTC::HAND, 0, Self->UID);
               Self->CursorSet = true;
            }

            if ((Self->Links[i].EscapeCode IS ESC_LINK) and (!Self->Links[i].Link->PointerMotion.empty())) {                          
               auto mo = Self->MouseOverChain.emplace(Self->MouseOverChain.begin(), 
                  Self->Links[i].Link->PointerMotion, 
                  Self->Links[i].Y, 
                  Self->Links[i].X, 
                  Self->Links[i].Y + Self->Links[i].Height, 
                  Self->Links[i].X + Self->Links[i].Width, 
                  Self->Links[i].Link->ID);

               OBJECTPTR script;
               std::string argstring, func_name;
               if (!extract_script(Self, Self->Links[i].Link->PointerMotion, &script, func_name, argstring)) {
                  const ScriptArg args[] = { { "Element", mo->ElementID }, { "Status", 1 }, { "Args", argstring } };
                  scExec(script, func_name.c_str(), args, ARRAYSIZE(args));
               }
            }

            Self->LinkIndex = i;
            return;
         }
      }
   }

   // The mouse pointer is not inside a link

   if (Self->LinkIndex != -1) Self->LinkIndex = -1;

   // Check if the user moved onto text content

   if (Self->MouseOverSegment != -1) {
      if ((Self->Segments[Self->MouseOverSegment].TextContent) or (Self->Segments[Self->MouseOverSegment].Edit)) {
         gfxSetCursor(0, CRF::BUFFER, PTC::TEXT, 0, Self->UID);
         Self->CursorSet = true;
      }
      return;
   }

   for (unsigned i=0; i < Self->EditCells.size(); i++) {
      if ((X >= Self->EditCells[i].X) and (X < Self->EditCells[i].X + Self->EditCells[i].Width) and
          (Y >= Self->EditCells[i].Y) and (Y < Self->EditCells[i].Y + Self->EditCells[i].Height)) {
         gfxSetCursor(0, CRF::BUFFER, PTC::TEXT, 0, Self->UID);
         Self->CursorSet = true;
         return;
      }
   }

   // Reset the cursor to the default

   if (Self->CursorSet) {
      Self->CursorSet = false;
      gfxRestoreCursor(PTC::DEFAULT, Self->UID);
   }
}

//********************************************************************************************************************

static ERROR resolve_font_pos(extDocument *Self, LONG Segment, LONG X, LONG *CharX, LONG *BytePos)
{
   pf::Log log(__FUNCTION__);
   LONG i, index;

   if ((Segment >= 0) and (Segment < LONG(Self->Segments.size()))) {
      // Find the font that represents the start of the stream

      auto &str = Self->Stream;
      escFont *style = NULL;

      // First, go forwards to try and find the correct font

      auto fi = Self->Segments[Segment].Index;
      while (fi < Self->Segments[Segment].Stop) {
         if ((str[fi] IS CTRL_CODE) and (ESCAPE_CODE(str, fi) IS ESC_FONT)) {
            style = &escape_data<escFont>(Self, fi);
         }
         else if (str[fi] != CTRL_CODE) break;
         NEXT_CHAR(str, fi);
      }

      // Didn't work?  Try going backwards

      if (!style) {
         fi = Self->Segments[Segment].Index;
         while (fi >= 0) {
            if ((str[fi] IS CTRL_CODE) and (ESCAPE_CODE(str, fi) IS ESC_FONT)) {
               style = &escape_data<escFont>(Self, fi);
               break;
            }
            PREV_CHAR(str, fi);
         }
      }

      objFont *font;
      if (!style) font = lookup_font(0, "check_mouse_click");
      else font = lookup_font(style->Index, "check_mouse_click");

      if (!font) return ERR_Search;

      // Determine the index of the character in the stream that has been clicked.
      // Note: style changes will cause a segment to end a new one to be created.
      // This is also true of objects that appear in the stream.  We can rely on this
      // fact to focus entirely on pulling the string out of the segment and not worry about
      // interpreting any fancy escape codes.

      // Normalise the segment into a plain character string so that we can translate the coordinates

      std::string buffer;
      buffer.reserve(Self->Segments[Segment].Stop - Self->Segments[Segment].Index + 1);
      LONG pos = 0;
      i = Self->Segments[Segment].Index;
      while (i < Self->Segments[Segment].Stop) {
         if (Self->Stream[i] != CTRL_CODE) buffer[pos++] = Self->Stream[i++];
         else i += ESCAPE_LEN;
      }
      buffer[pos] = 0;

      if (!fntConvertCoords(font, buffer.c_str(), X - Self->Segments[Segment].X, 0, NULL, NULL, NULL, &index, CharX)) {
         // Convert the character position to the correct byte position - i.e. take control codes into account.

         for (i=Self->Segments[Segment].Index; (index > 0); ) {
            if (Self->Stream[i] IS CTRL_CODE) i += ESCAPE_LEN;
            else { index--; i++; }
         }

         *BytePos = i - Self->Segments[Segment].Index;
         return ERR_Okay;
      }
      else {
         log.trace("Failed to convert coordinate %d to a font-relative cursor position.", X);
         return ERR_Failed;
      }
   }
   else {
      log.trace("Current segment value is invalid.");
      return ERR_OutOfRange;
   }
}

//********************************************************************************************************************
// Using only a stream index, this function will determine the X coordinate of the character at that index.  This is
// slower than resolve_font_pos(), because the segment has to be resolved by this function.

static ERROR resolve_fontx_by_index(extDocument *Self, LONG Index, LONG *CharX)
{
   pf::Log log("resolve_fontx");
   LONG segment;

   log.branch("Index: %d", Index);

   escFont *style = NULL;

   // First, go forwards to try and find the correct font

   LONG fi = Index;
   while ((Self->Stream[fi] != CTRL_CODE) and (fi < LONG(Self->Stream.size()))) {
      if ((Self->Stream[fi] IS CTRL_CODE) and (ESCAPE_CODE(Self->Stream, fi) IS ESC_FONT)) {
         style = &escape_data<escFont>(Self, fi);
      }
      else if (Self->Stream[fi] != CTRL_CODE) break;
      NEXT_CHAR(Self->Stream, fi);
   }

   // Didn't work?  Try going backwards

   if (!style) {
      fi = Index;
      while (fi >= 0) {
         if ((Self->Stream[fi] IS CTRL_CODE) and (ESCAPE_CODE(Self->Stream, fi) IS ESC_FONT)) {
            style = &escape_data<escFont>(Self, fi);
            break;
         }
         PREV_CHAR(Self->Stream, fi);
      }
   }

   auto font = lookup_font(style ? style->Index : 0, "check_mouse_click");

   if (!font) return log.warning(ERR_Search);

   // Find the segment associated with this index.  This is so that we can derive an X coordinate for the character
   // string.

   if ((segment = find_segment(Self, Index, true)) >= 0) {
      // Normalise the segment into a plain character string

      std::string buffer;
      buffer.reserve((Self->Segments[segment].Stop+1) - Self->Segments[segment].Index + 1);
      LONG pos = 0;
      LONG i = Self->Segments[segment].Index;
      while ((i <= Self->Segments[segment].Stop) and (i < Index)) {
         if (Self->Stream[i] != CTRL_CODE) buffer[pos++] = Self->Stream[i++];
         else i += ESCAPE_LEN;
      }
      buffer[pos] = 0;

      if (pos > 0) *CharX = fntStringWidth(font, buffer.c_str(), -1);
      else *CharX = 0;

      return ERR_Okay;
   }
   else log.warning("Failed to find a segment for index %d.", Index);

   return ERR_Search;
}

//********************************************************************************************************************

static LONG find_segment(extDocument *Self, LONG Index, LONG InclusiveStop)
{
   if (InclusiveStop) {
      for (unsigned segment=0; segment < Self->Segments.size(); segment++) {
         if ((Index >= Self->Segments[segment].Index) and (Index <= Self->Segments[segment].Stop)) {
            if ((Index IS Self->Segments[segment].Stop) and (Self->Stream[Index-1] IS '\n'));
            else return segment;
         }
      }
   }
   else {
      for (unsigned segment=0; segment < Self->Segments.size(); segment++) {
         if ((Index >= Self->Segments[segment].Index) and (Index < Self->Segments[segment].Stop)) {
            return segment;
         }
      }
   }

   return -1;
}

//********************************************************************************************************************
// The text will be deselected, but the cursor and editing area will remain active.

static void deselect_text(extDocument *Self)
{
   pf::Log log(__FUNCTION__);

   // Return immediately if there is nothing to deselect

   if (Self->CursorIndex IS Self->SelectIndex) return;

   log.traceBranch("");

   LONG start = Self->CursorIndex;
   LONG end = Self->SelectIndex;

   if (end < start) {
      auto tmp = start;
      start = end;
      end = tmp;
   }

   // Redrawing is really simple, we're not going to care about the X coordinates and
   // just redraw everything from the Y coordinate of start to Y+Height of end.

   // Find the start

   LONG mid;
   LONG top    = 0;
   LONG bottom = Self->Segments.size()-1;
   LONG last   = -1;
   while ((mid = (bottom-top)>>1) != last) {
      last = mid;
      mid += top;
      if (start >= Self->Segments[mid].Index) top = mid;
      else if (end < Self->Segments[mid].Stop) bottom = mid;
      else break;
   }

   LONG startseg = mid; // Start is now set to the segment rather than stream index

   // Find the end
    
   top    = startseg;
   bottom = Self->Segments.size()-1;
   last   = -1;
   while ((mid = (bottom-top)>>1) != last) {
      last = mid;
      mid += top;
      if (start >= Self->Segments[mid].Index) top = mid;
      else if (end < Self->Segments[mid].Stop) bottom = mid;
      else break;
   }

   //LONG endseg = mid; // End is now set to the segment rather than stream index

   Self->SelectIndex = -1;

   DRAW_PAGE(Self);  // TODO: Draw only the area that we've identified as relevant.
}

//********************************************************************************************************************

static LONG find_tabfocus(extDocument *Self, UBYTE Type, LONG Reference)
{
   for (unsigned i=0; i < Self->Tabs.size(); i++) {
      if ((Self->Tabs[i].Type IS Type) and (Reference IS Self->Tabs[i].Ref)) return i;
   }
   return -1;
}

//********************************************************************************************************************
// This function is used in tags.c by the link and object insertion code.

static LONG add_tabfocus(extDocument *Self, UBYTE Type, LONG Reference)
{
   pf::Log log(__FUNCTION__);

   //log.function("Type: %d, Ref: %d", Type, Reference);

   if (Type IS TT_LINK) { // For TT_LINK types, check that the link isn't already registered
      for (unsigned i=0; i < Self->Tabs.size(); i++) {
         if ((Self->Tabs[i].Type IS TT_LINK) and (Self->Tabs[i].Ref IS Reference)) {
            return i;
         }
      }
   }

   auto index = Self->Tabs.size();
   Self->Tabs.emplace_back(Type, Reference, Type, Self->Invisible ^ 1);

   if (Type IS TT_OBJECT) {
      // Find out if the object has a surface and if so, place it in the XRef field.

      if (GetClassID(Reference) != ID_SURFACE) {
         OBJECTPTR object;
         if (!AccessObject(Reference, 3000, &object)) {
            OBJECTID regionid = 0;
            if (FindField(object, FID_Region, NULL)) {
               if (!object->get(FID_Region, &regionid)) {
                  if (GetClassID(regionid) != ID_SURFACE) regionid = 0;
               }
            }

            if (!regionid) {
               if (FindField(object, FID_Surface, NULL)) {
                  if (!object->get(FID_Surface, &regionid)) {
                     if (GetClassID(regionid) != ID_SURFACE) regionid = 0;
                  }
               }
            }

            Self->Tabs.back().XRef = regionid;

            ReleaseObject(object);
         }
      }
      else Self->Tabs.back().XRef = Reference;
   }

   return index;
}

//********************************************************************************************************************
// Changes the focus to an object or link in the document.  The new index is stored in the FocusIndex field.  If the
// Index is set to -1, set_focus() will focus on the first element, but only if it is an object.

static void set_focus(extDocument *Self, LONG Index, CSTRING Caller)
{
   pf::Log log(__FUNCTION__);

   if (Self->Tabs.empty()) return;   

   if ((Index < -1) or (unsigned(Index) >= Self->Tabs.size())) {
      log.traceWarning("Index %d out of bounds.", Index);
      return;
   }

   log.branch("Index: %d/%d, Type: %d, Ref: %d, HaveFocus: %d, Caller: %s", Index, LONG(Self->Tabs.size()), Index != -1 ? Self->Tabs[Index].Type : -1, Index != -1 ? Self->Tabs[Index].Ref : -1, Self->HasFocus, Caller);

   if (Self->ActiveEditDef) deactivate_edit(Self, true);

   if (Index IS -1) {
      Index = 0;
      Self->FocusIndex = 0;
      if (Self->Tabs[0].Type IS TT_LINK) {
         log.msg("First focusable element is a link - focus unchanged.");
         return;
      }
   }

   if (!Self->Tabs[Index].Active) {
      log.warning("Tab marker %d is not active.", Index);
      return;
   }

   Self->FocusIndex = Index;

   if (Self->Tabs[Index].Type IS TT_EDIT) {
      acFocus(Self->PageID);

      LONG cell_index;
      if ((cell_index = find_cell(Self, Self->Tabs[Self->FocusIndex].Ref)) >= 0) {
         activate_edit(Self, cell_index, -1);
      }
   }
   else if (Self->Tabs[Index].Type IS TT_OBJECT) {
      if (Self->HasFocus) {
         CLASSID class_id = GetClassID(Self->Tabs[Index].Ref);
         OBJECTPTR input;
         if (class_id IS ID_VECTORTEXT) {
            if (!AccessObject(Self->Tabs[Index].Ref, 1000, &input)) {
               acFocus(input);
               //if ((input->getPtr(FID_UserInput, &text) IS ERR_Okay) and (text)) {
               //   txtSelectArea(text, 0,0, 200000, 200000);
               //}
               ReleaseObject(input);
            }
         }
         else if (acFocus(Self->Tabs[Index].Ref) != ERR_Okay) {
            acFocus(Self->Tabs[Index].XRef);
            // Causes an InheritedFocus callback in ActionNotify
         }
      }
   }
   else if (Self->Tabs[Index].Type IS TT_LINK) {
      if (Self->HasFocus) { // Scroll to the link if it is out of view, or redraw the display if it is not.
         unsigned i;
         for (i=0; i < Self->Links.size(); i++) {
            if ((Self->Links[i].EscapeCode IS ESC_LINK) and (Self->Links[i].Link->ID IS Self->Tabs[Index].Ref)) break;
         }

         if (i < Self->Links.size()) {
            LONG link_x = Self->Links[i].X;
            LONG link_y = Self->Links[i].Y;
            LONG link_bottom = link_y + Self->Links[i].Height;
            LONG link_right = link_x + Self->Links[i].Width;

            for (++i; i < Self->Links.size(); i++) {
               if (Self->Links[i].Link->ID IS Self->Tabs[Index].Ref) {
                  if (Self->Links[i].Y + Self->Links[i].Height > link_bottom) link_bottom = Self->Links[i].Y + Self->Links[i].Height;
                  if (Self->Links[i].X + Self->Links[i].Width > link_right) link_right = Self->Links[i].X + Self->Links[i].Width;
               }
            }

            if (!view_area(Self, link_x, link_y, link_right, link_bottom)) {
               DRAW_PAGE(Self);
            }
         }
         else DRAW_PAGE(Self);

         acFocus(Self->PageID);
      }
   }
}

//********************************************************************************************************************
// Scrolls any given area of the document into view.

static BYTE view_area(extDocument *Self, LONG Left, LONG Top, LONG Right, LONG Bottom)
{
   pf::Log log(__FUNCTION__);

   LONG hgap = Self->AreaWidth * 0.1;
   LONG vgap = Self->AreaHeight * 0.1;
   LONG view_x = -Self->XPosition;
   LONG view_y = -Self->YPosition;
   LONG view_height = Self->AreaHeight;
   LONG view_width  = Self->AreaWidth;

   log.trace("View: %dx%d,%dx%d Link: %dx%d,%dx%d", view_x, view_y, view_width, view_height, Left, Top, Right, Bottom);

   // Vertical

   if (Self->PageHeight > Self->AreaHeight) {
      if (Top < view_y + vgap) {
         view_y = Top - vgap;
         if (view_y < view_height>>2) view_y = 0;

         if ((Bottom < view_height - vgap) and (-Self->YPosition > view_height)) {
            view_y = 0;
         }
      }
      else if (Bottom > view_y + view_height - vgap) {
         view_y = Bottom + vgap - view_height;
         if (view_y > Self->PageHeight - view_height - (view_height>>2)) view_y = Self->PageHeight - view_height;
      }
   }
   else view_y = 0;

   // Horizontal

   if (Self->CalcWidth > Self->AreaWidth) {
      if (Left < view_x + hgap) {
         view_x = Left - hgap;
         if (view_x < 0) view_x = 0;
      }
      else if (Right > view_x + view_width - hgap) {
         view_x = Right + hgap - view_width;
         if (view_x > Self->CalcWidth - view_width) view_x = Self->CalcWidth - view_width;
      }
   }
   else view_x = 0;

   if ((-view_x != Self->XPosition) or (-view_y != Self->YPosition)) {
      acScrollToPoint(Self, view_x, view_y, 0, STP::X|STP::Y);
      return true;
   }
   else return false;
}

//********************************************************************************************************************

static void advance_tabfocus(extDocument *Self, BYTE Direction)
{
   pf::Log log(__FUNCTION__);

   if (Self->Tabs.empty()) return;

   // Check that the FocusIndex is accurate (it may have changed if the user clicked on a gadget).

   OBJECTID currentfocus = gfxGetUserFocus();
   for (unsigned i=0; i < Self->Tabs.size(); i++) {
      if (Self->Tabs[i].XRef IS currentfocus) {
         Self->FocusIndex = i;
         break;
      }
   }

   log.function("Direction: %d, Current Surface: %d, Current Index: %d", Direction, currentfocus, Self->FocusIndex);

   if (Self->FocusIndex < 0) {
      // FocusIndex may be -1 to indicate nothing is selected, so we'll have to start from the first focusable index in that case.

      if (Direction IS -1) Self->FocusIndex = 1; // Future --
      else Self->FocusIndex = -1; // Future ++
   }

   // Advance the focus index.  Operates as a loop so that disabled surfaces can be skipped.

   auto i = signed(Self->Tabs.size()); // This while loop is designed to stop if no tab indexes are found to be active
   while (i > 0) {
      i--;

      if (Direction IS -1) {
         Self->FocusIndex--;
         if (Self->FocusIndex < 0) Self->FocusIndex = Self->Tabs.size() - 1;
      }
      else {
         Self->FocusIndex++;
         if (Self->FocusIndex >= LONG(Self->Tabs.size())) Self->FocusIndex = 0;
      }

      if (!Self->Tabs[Self->FocusIndex].Active) continue;

      if ((Self->Tabs[Self->FocusIndex].Type IS TT_OBJECT) and (Self->Tabs[Self->FocusIndex].XRef)) {
         SURFACEINFO *info;
         if (!gfxGetSurfaceInfo(Self->Tabs[Self->FocusIndex].XRef, &info)) {
            if ((info->Flags & RNF::DISABLED) != RNF::NIL) continue;
         }
      }
      break;
   }

   if (i >= 0) set_focus(Self, Self->FocusIndex, "adv_tabfocus");
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
// Obsoletion of the old scrollbar code means that we should be adjusting page size only and let the scrollbars
// automatically adjust in the background.

static void calc_scroll(extDocument *Self) __attribute__((unused));
static void calc_scroll(extDocument *Self)
{
   pf::Log log(__FUNCTION__);

   log.traceBranch("PageHeight: %d/%d, PageWidth: %d/%d, XPos: %d, YPos: %d", Self->PageHeight, Self->AreaHeight, Self->CalcWidth, Self->AreaWidth, Self->XPosition, Self->YPosition);
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

static void exec_link(extDocument *Self, DocLink &Link)
{
   pf::Log log(__FUNCTION__);

   log.branch("");

   Self->Processing++;

   if ((Link.EscapeCode IS ESC_LINK) and ((Self->EventMask & DEF::LINK_ACTIVATED) != DEF::NIL)) {
      deLinkActivated params;
      auto link = Link.Link;

      if (link->Type IS LINK_FUNCTION) {
         std::string function_name, args;
         if (!extract_script(Self, link->Ref, NULL, function_name, args)) {
            params.Values["onclick"] = function_name;
         }
      }
      else if (link->Type IS LINK_HREF) {
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

   if (Link.EscapeCode IS ESC_LINK) {
      OBJECTPTR script;
      std::string function_name, fargs;
      CLASSID class_id, subclass_id;

      auto link = Link.Link;
      if (link->Type IS LINK_FUNCTION) { // Function is in the format 'function()' or 'script.function()'
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
      else if (link->Type IS LINK_HREF) {
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

               std::string lk;

               if (!Self->Path.empty()) {
                  bool abspath = false; // Is the link an absolute path indicated by a volume name?
                  for (LONG j=0; link->Ref[j]; j++) {
                     if ((link->Ref[j] IS '/') or (link->Ref[j] IS '\\')) break;
                     if (link->Ref[j] IS ':') { abspath = true; break; }
                  }

                  if (!abspath) {
                     LONG end;
                     for (end=0; Self->Path[end]; end++) {
                        if ((Self->Path[end] IS '&') or (Self->Path[end] IS '#') or (Self->Path[end] IS '?')) break;
                     }
                     while ((end > 0) and (Self->Path[end-1] != '/') and (Self->Path[end-1] != '\\') and (Self->Path[end-1] != ':')) end--;
                     lk.assign(Self->Path, end);
                  }
               }

               lk += link->Ref;

               LONG end;
               for (end=0; lk[end]; end++) {
                  if ((lk[end] IS '?') or (lk[end] IS '#') or (lk[end] IS '&')) break;
               }

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
   else if (Link.EscapeCode IS ESC_CELL) {
      OBJECTPTR script;
      std::string function_name, script_args;

      escCell *cell = Link.Cell;

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

static void exec_link(extDocument *Self, LONG Index)
{   
   if ((Index IS -1) or (unsigned(Index) >= Self->Links.size())) return;

   exec_link(Self, Self->Links[Index]);
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

      auto &esc_index = escape_data<escIndex>(Self, start);
      Self->scrollToPoint(0, esc_index.Y - 4, 0, STP::Y);
   }
   else log.warning("Failed to find bookmark '%s'", Bookmark.c_str());
}

//********************************************************************************************************************

static void key_event(extDocument *Self, evKey *Event, LONG Size)
{
   if ((Event->Qualifiers & KQ::PRESSED) != KQ::NIL) {
      keypress(Self, Event->Qualifiers, Event->Code, Event->Unicode);
   }
}

//********************************************************************************************************************

static ERROR flash_cursor(extDocument *Self, LARGE TimeElapsed, LARGE CurrentTime)
{
   Self->CursorState ^= 1;

   DRAW_PAGE(Self);
   return ERR_Okay;
}

//********************************************************************************************************************

static void reset_cursor(extDocument *Self)
{
   pf::Log log(__FUNCTION__);

   log.function("");

   Self->CursorState = 1;
   if (Self->FlashTimer) UpdateTimer(Self->FlashTimer, 0.5);
   else {
      auto call = make_function_stdc(flash_cursor);
      SubscribeTimer(0.5, &call, &Self->FlashTimer);
   }
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
