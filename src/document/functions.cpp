
#define MAXLOOP 100000

// This is a list of the default class types that may be used in document pages.  Its purpose is to restrict the types
// of objects that can be used so that we don't run into major security problems.  Basically, if an instantiated
// object could have the potential to run any program that the user has access to, or if it could gain access to local
// information and use it for nefarious purposes, then it's not secure enough for document usage.
//
// TODO: NEEDS TO BE REPLACED WITH AN XML DEFINITION and PARSED INTO A KEY VALUE STORE.

static const struct {
   CSTRING ClassName;
   CLASSID ClassID;
   CSTRING PageTarget;
   CSTRING Fields;
} glDocClasses[] = {
   // GUI
   { "vector",      ID_VECTOR,    "surface", "" },
   { "document",    ID_DOCUMENT,  "surface", "" },
   // TOOLS
   { "scintilla",    ID_SCINTILLA,    NULL, "" },
   // NETWORK
   { "http",         ID_HTTP,         NULL, "" },
   // DATA
   { "config",       ID_CONFIG,       NULL, "" },
   { "xml",          ID_XML,          NULL, "" }
};

static const char glDefaultStyles[] =
"<template name=\"h1\"><p leading=\"2.0\"><font face=\"Open Sans\" size=\"18\" colour=\"0,0,0\" style=\"bold\"><inject/></font></p></template>\n\
<template name=\"h2\"><p leading=\"2.0\"><font face=\"Open Sans\" size=\"16\" colour=\"0,0,0\" style=\"bold\"><inject/></font></p></template>\n\
<template name=\"h3\"><p leading=\"1.5\"><font face=\"Open Sans\" size=\"14\" colour=\"0,0,0\" style=\"bold\"><inject/></font></p></template>\n\
<template name=\"h4\"><p leading=\"1.5\"><font face=\"Open Sans\" size=\"14\" colour=\"0,0,0\"><inject/></font></p></template>\n\
<template name=\"h5\"><p leading=\"1.25\"><font face=\"Open Sans\" size=\"12\" colour=\"0,0,0\"><inject/></font></p></template>\n\
<template name=\"h6\"><p leading=\"1.25\"><font face=\"Open Sans\" size=\"10\" colour=\"0,0,0\"><inject/></font></p></template>\n";

#if defined(DEBUG) || defined(DBG_LAYOUT)
static char glPrintable[80];

static STRING printable(CSTRING String, LONG Length) __attribute__ ((unused));

static STRING printable(CSTRING String, LONG Length)
{
   LONG i = 0, j = 0;
   while ((String[i]) and (i < Length) and (j < ARRAYSIZE(glPrintable)-1)) {
      if (String[i] IS CTRL_CODE) {
         glPrintable[j++] = '%';
         i += ESCAPE_LEN(String+i);
      }
      else if (String[i] < 0x20) {
         glPrintable[j++] = '?';
         i++;
      }
      else glPrintable[j++] = String[i++];
   }
   glPrintable[j] = 0;
   return glPrintable;
}

static char glPrintable2[80];

static STRING printable2(CSTRING String, LONG Length) __attribute__ ((unused));

static STRING printable2(CSTRING String, LONG Length)
{
   LONG i = 0, j = 0;
   while ((String[i]) and (i < Length) and (j < ARRAYSIZE(glPrintable2)-1)) {
      if (String[i] IS CTRL_CODE) {
         glPrintable2[j++] = '%';
         i += ESCAPE_LEN(String+i);
      }
      else if (String[i] < 0x20) {
         glPrintable[j++] = '?';
         i++;
      }
      else glPrintable2[j++] = String[i++];
   }
   glPrintable2[j] = 0;
   return glPrintable2;
}

static void print_xmltree(XMLTag *Tag, LONG *Indent)
{
   parasol::Log log(__FUNCTION__);
   XMLTag *child;
   LONG i, j;
   char buffer[1000];

   if (!Indent) {
      i = 0;
      while (Tag) {
         print_xmltree(Tag, &i);
         Tag=Tag->Next;
      }
      return;
   }

   if (!Tag->Attrib) {
      log.warning("Error - no Attrib in tag %d", Tag->Index);
      return;
   }

   for (i=0; i < *Indent; i++) buffer[i] = ' ';

   if (Tag->Attrib->Name) {
      for (j=0; Tag->Attrib->Name[j]; j++) buffer[i++] = Tag->Attrib->Name[j];
   }
   else {
      // Extract up to 20 characters of content
      buffer[i++] = '[';
      for (j=0; (Tag->Attrib->Value[j]) and (j < 20); j++) buffer[i++] = Tag->Attrib->Value[j];
      buffer[i++] = ']';
   }
   buffer[i] = 0;

   log.msg("%s", buffer);

   Indent[0]++;
   for (child=Tag->Child; child; child=child->Next) {
      print_xmltree(child, Indent);
   }
   Indent[0]--;
}
#endif

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

   fprintf(stderr, "\nSTREAM: %d bytes\n------\n", Self->StreamLen);
   i = 0;
   printpos = FALSE;
   while (str[i]) {
      if (str[i] IS CTRL_CODE) {
         code = ESCAPE_CODE(str, i);
         fprintf(stderr, "(%d)", i);
         len = ESCAPE_LEN(str+i);
         if (len < ESC_LEN) {
            fprintf(stderr, "\nInvalid escape code length of %d at index %d.  Code: %d\n", len, i, code);
            break;
         }
         if (code IS ESC_FONT) {
            style = escape_data<escFont>(str, i);
            fprintf(stderr, "[E:Font:%d:%d", len, style->Index);
            if (style->Options & FSO_ALIGN_RIGHT) fprintf(stderr, ":A/R");
            if (style->Options & FSO_ALIGN_CENTER) fprintf(stderr, ":A/C");
            if (style->Options & FSO_BOLD) fprintf(stderr, ":Bold");
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
            fprintf(stderr, "[E:%s:%d]", strCodes[code], len);
         }
         else fprintf(stderr, "[E:%d:%d]", code, len);
         i += len;
         printpos = TRUE;
      }
      else {
         if (printpos) {
            printpos = FALSE;
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
   for (LONG row=0; row < Self->SegCount; row++) {
      DocSegment *line = Self->Segments + row;
      LONG i = line->Index;

      fprintf(stderr, "Seg %d, Bytes %d-%d: %dx%d,%dx%d: ", row, line->Index, line->Stop, line->X, line->Y, line->Width, line->Height);
      if (line->Edit) fprintf(stderr, "{ ");
      fprintf(stderr, "\"");
      while (i < line->Stop) {
         if (str[i] IS CTRL_CODE) {
            if (ESCAPE_CODE(str, i) IS ESC_FONT) {
               auto style = escape_data<escFont>(str, i);
               fprintf(stderr, "[E:Font:%d:%d:$%.2x%.2x%.2x", ESCAPE_LEN(str+i), style->Index, style->Colour.Red, style->Colour.Green, style->Colour.Blue);
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
               fprintf(stderr, "[E:%s:%d]", strCodes[(UBYTE)ESCAPE_CODE(str, i)], ESCAPE_LEN(str+i));
            }
            else fprintf(stderr, "[E:%d:%d]", ESCAPE_CODE(str, i), ESCAPE_LEN(str+i));
            i += ESCAPE_LEN(str+i);
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
   for (LONG row=0; row < Self->SortCount; row++) {
      DocSegment *line = Self->Segments + Self->SortSegments[row].Segment;
      fprintf(stderr, "%d: Y: %d-%d, Seg: %d \"", row, Self->SortSegments[row].Y, Self->Segments[Self->SortSegments[row].Segment].X, Self->SortSegments[row].Segment);

      LONG i = line->Index;
      while (i < line->Stop) {
         if (str[i] IS CTRL_CODE) {
            if (ESCAPE_CODE(str, i) IS ESC_FONT) {
               auto style = escape_data<escFont>(str, i);
               fprintf(stderr, "[E:Font:%d:%d:$%.2x%.2x%.2x", ESCAPE_LEN(str+i), style->Index, style->Colour.Red, style->Colour.Green, style->Colour.Blue);
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
               fprintf(stderr, "[E:%s:%d]", strCodes[(UBYTE)ESCAPE_CODE(str, i)], ESCAPE_LEN(str+i));
            }
            else fprintf(stderr, "[E:%d:%d]", ESCAPE_CODE(str, i), ESCAPE_LEN(str+i));
            i += ESCAPE_LEN(str+i);
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
   if (Self->TabIndex) {
      fprintf(stderr, "\nTAB FOCUSLIST\n-------------\n");

      for (LONG i=0; i < Self->TabIndex; i++) {
         fprintf(stderr, "%d: Type: %d, Ref: %d, XRef: %d\n", i, Self->Tabs[i].Type, Self->Tabs[i].Ref, Self->Tabs[i].XRef);
      }
   }
}

#endif

//static BYTE glWhitespace = TRUE;  // Setting this to TRUE tells the parser to ignore whitespace (prevents whitespace being used as content)

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
   LONG link_align;
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
   UBYTE nowrap:1;
   UBYTE link_open:1;
   UBYTE setsegment:1;
   UBYTE textcontent:1;
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

   return FindField(Object, StrHash(Name, FALSE), Source);
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

static bool test_statement(CSTRING TestString, CSTRING CompareString, LONG Condition)
{
   parasol::Log log(__FUNCTION__);

   //log.msg("\"%s\" %d \"%s\"", TestString, Condition, CompareString);

   // Convert the If->Compare to its specified type

   LONG cmp_type  = StrDatatype(CompareString);
   LONG test_type = StrDatatype(TestString);

   bool result = false;
   if (((test_type IS STT_NUMBER) or (test_type IS STT_FLOAT)) and ((cmp_type IS STT_NUMBER) or (cmp_type IS STT_FLOAT))) {
      DOUBLE cmp_float  = StrToFloat(CompareString);
      DOUBLE test_float = StrToFloat(TestString);
      switch(Condition) {
         case COND_NOT_EQUAL:     if (test_float != cmp_float) result = true; break;
         case COND_EQUAL:         if (test_float IS cmp_float) result = true; break;
         case COND_LESS_THAN:     if (test_float <  cmp_float) result = true; break;
         case COND_LESS_EQUAL:    if (test_float <= cmp_float) result = true; break;
         case COND_GREATER_THAN:  if (test_float >  cmp_float) result = true; break;
         case COND_GREATER_EQUAL: if (test_float >= cmp_float) result = true; break;
         default: log.warning("Unsupported condition type %d.", Condition);
      }
   }
   else {
      if (Condition IS COND_EQUAL) {
         if (StrMatch(TestString, CompareString) IS ERR_Okay) result = true;
      }
      else if (Condition IS COND_NOT_EQUAL) {
         if (StrMatch(TestString, CompareString) != ERR_Okay) result = true;
      }
      else log.warning("String comparison for condition %d not possible.", Condition);
   }

   return result;
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

*****************************************************************************/

static WORD write_calc(STRING Buffer, LONG BufferSize, DOUBLE Value, WORD Precision)
{
   LONG index = 0;
   LARGE wholepart = F2T(Value);
   if (wholepart < 0) wholepart = -wholepart;

   // Sign the value if it is less than 0

   if ((Value < 0) and (index < BufferSize - 1)) Buffer[index++] = '-';

   if (!Precision) {
      index += IntToStr(wholepart, Buffer+index, BufferSize);
      return index;
   }

   DOUBLE fraction = (Value - wholepart);
   if (fraction < 0) fraction = -fraction;

   index += IntToStr(wholepart, Buffer+index, BufferSize);

   if ((index < BufferSize-1) and ((fraction > 0) or (Precision < 0))) {
      Buffer[index++] = '.';
      fraction = fraction * 10;
      auto px = Precision;
      if (px < 0) px = -px;
      while ((fraction > 0.00001) and (index < BufferSize-1) and (px > 0)) {
         LONG ival = F2T(fraction);
         Buffer[index++] = ival + '0';
         fraction = (fraction - ival) * 10;
         px--;
      }

      if (Precision < 0) {
         while (px > 0) { Buffer[index++] = '0'; px--; }
      }
   }

   return index;
}

ERROR calc(CSTRING String, DOUBLE *Result, STRING Output, LONG OutputSize)
{
   enum SIGN {
      PLUS=1,
      MINUS,
      MULTIPLY,
      DIVIDE,
      MODULO
   };

   if (Result) *Result = 0;

   if (Output) {
      if (OutputSize < 1) return ERR_BufferOverflow;
      Output[0] = 0;
   }

   // Search for brackets and translate them first

   std::string in(String);
   while (1) {
      // Find the last bracketed reference

      LONG last_bracket = 0;
      for (LONG i=0; in[i]; i++) {
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
         calc(buf.c_str()+1, &calc_float, NULL, 0);
         char num[30];
         snprintf(num, sizeof(num), "%f", calc_float);

         in.replace(last_bracket, end - last_bracket, num);
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
   for (LONG s=0; in[s];) {
      if (in[s] <= 0x20); // Do nothing with whitespace
      else if (in[s] IS '\'') {
         if (Output) {
            if (number) { // Write the current floating point number to the buffer before the next calculation
               index   += write_calc(Output+index, OutputSize - index, total, precision);
               overall += total; // Reset the number
               total   = 0;
               number  = false;
            }

            s++;
            while (index < OutputSize-1) {
               if (in[s] IS '\\') s++; // Skip the \ character and continue so that we can copy the character immediately after it
               else if (in[s] IS '\'') break;

               Output[index++] = in[s++];
            }
         }
         else { // Skip string content if there is no string buffer
            s++;
            while (in[s] != '\'') s++;
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

   if (Output) {
      if (number) index += write_calc(Output+index, OutputSize - index, total, precision);
      Output[index] = 0;
   }

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

*****************************************************************************/

static ERROR eval(extDocument *Self, STRING Buffer, LONG BufferLength, LONG Flags)
{
   parasol::Log log(__FUNCTION__);
   LONG pos, i, j;

   if ((!Buffer) or (BufferLength < 3)) return log.warning(ERR_Args);

   // Quick check for translation symbols

   for (pos=0; Buffer[pos] != '['; pos++) {
      if (!Buffer[pos]) return ERR_EmptyString;
   }

   log.traceBranch("Size: %d, %s", BufferLength, Buffer);

   Field *classfield;

   ERROR error = ERR_Okay;
   ERROR majorerror = ERR_Okay;
   STRING calcbuffer = NULL;

   // Skip to the end of the buffer (translation occurs 'backwards')

   for (; Buffer[pos]; pos++);
   pos--;
   while (pos >= 0) {
      // Do not translate quoted areas

      if ((Buffer[pos] IS '"') and (!(Flags & SEF_IGNORE_QUOTES))) {
         pos--;
         while ((pos >= 0) and (Buffer[pos] != '"')) pos--;
         if (pos < 0) {
            log.warning("Badly defined string: %.80s", Buffer);
            if (calcbuffer) free(calcbuffer);
            return ERR_InvalidData;
         }
      }

      if ((Buffer[pos] IS '[') and ((Buffer[pos+1] IS '@') or (Buffer[pos+1] IS '%'))) {
         // Ignore arguments, e.g. [@id] or [%id].  It's also useful for ignoring [@attrib] in xpath.
         pos--;
      }
      else if (Buffer[pos] IS '[') {
         // Make sure that there is a closing bracket

         WORD endbracket;
         WORD balance = 0;
         for (endbracket=pos; Buffer[endbracket]; endbracket++) {
            if (Buffer[endbracket] IS '[') balance++;
            else if (Buffer[endbracket] IS ']') {
               balance--;
               if (!balance) break;
            }
         }

         if (Buffer[endbracket] != ']') {
            log.warning("Unbalanced string: %.90s ...", Buffer);
            if (calcbuffer) free(calcbuffer);
            return ERR_InvalidData;
         }

         if (Buffer[pos+1] IS '=') { // Perform a calculation
            DOUBLE value;
            char num[endbracket-pos];

            CopyMemory(Buffer+pos+2, num, endbracket-(pos+2));
            num[endbracket-(pos+2)] = 0;

            if ((calcbuffer) or (BufferLength > 2048)) {
               if (!calcbuffer) {
                  if (!(calcbuffer = (char *)malloc(BufferLength))) {
                     return ERR_AllocMemory;
                  }
               }
               calc(num, &value, calcbuffer, BufferLength);
               if (insert_string(calcbuffer, Buffer, BufferLength, pos, endbracket-pos+1)) {
                  log.warning("Buffer overflow (%d bytes) while inserting to buffer \"%.30s\"", BufferLength, Buffer);
                  free(calcbuffer);
                  return ERR_BufferOverflow;
               }
            }
            else {
               char calcbuffer[2048];
               calc(num, &value, calcbuffer, sizeof(calcbuffer));
               if (insert_string(calcbuffer, Buffer, BufferLength, pos, endbracket-pos+1)) {
                  log.warning("Buffer overflow (%d bytes) while inserting to buffer \"%.30s\"", BufferLength, Buffer);
                  return ERR_BufferOverflow;
               }
            }
         }
         else if (Buffer[pos+1] IS '$') { // Escape sequence - e.g. translates [$ABC] to ABC.  Note: Use [rb] and [lb] instead for brackets.
            if (Flags & SEF_KEEP_ESCAPE); // Special option to ignore escape sequences.
            else {
               for (i=pos+1, j=pos+2; Buffer[j]; i++,j++) Buffer[i] = Buffer[j];
               Buffer[i] = 0;
            }
            pos--;
            continue;
         }
         else {
            char name[MAX_NAME_LEN];

            LONG j = 0;
            for (i=pos+1; (Buffer[i] != '.') and (i < endbracket); i++) {
               if ((size_t)j < sizeof(name)-1) name[j++] = std::tolower(Buffer[i]);
            }
            name[j] = 0;

            // Check for [lb] and [rb] escape codes

            char code = 0;
            if (j IS 2) {
               if ((name[0] IS 'r') and (name[1] IS 'b')) code = ']';
               else if ((name[0] IS 'l') and (name[1] IS 'b')) code = '[';
            }

            if (code) {
               Buffer[pos] = code;
               for (i=pos+j+2, j=pos+1; Buffer[i]; i++) Buffer[j++] = Buffer[i];
               Buffer[j] = 0;
               pos--;
               continue;
            }
            else {
               // Get the object ID

               OBJECTID objectid = 0;

               if (name[0]) {
                  if (!StrMatch(name, "self")) {
                     objectid = CurrentContext()->UID;
                  }
                  else FindObject(name, 0, FOF_SMART_NAMES, &objectid);
               }

               if (objectid) {
                  OBJECTPTR object = NULL;
                  Self->TBuffer[0] = 0;
                  if (Buffer[i] IS '.') {
                     // Get the field from the object
                     i++;

                     LONG j = 0;
                     char field[60];
                     while ((i < endbracket) and ((size_t)j < sizeof(field)-1)) {
                        field[j++] = Buffer[i++];
                     }
                     field[j] = 0;
                     if (!AccessObjectID(objectid, 2000, &object)) {
                        OBJECTPTR target;
                        if (((classfield = find_field(object, field, &target))) and (classfield->Flags & FD_STRING)) {
                           error = GetField(object, (FIELD)classfield->FieldID|TSTR, &Self->TBuffer);
                        }
                        else {
                           // Get field as an unlisted type and manage any buffer overflow
repeat:
                           Self->TBuffer[Self->TBufferSize-1] = 0;
                           GetFieldVariable(object, field, Self->TBuffer, Self->TBufferSize);

                           if (Self->TBuffer[Self->TBufferSize-1]) {
                              STRING newbuf;
                              if (!AllocMemory(Self->TBufferSize + 1024, MEM_STRING, &newbuf)) {
                                 FreeResource(Self->TBuffer);
                                 Self->TBuffer = newbuf;
                                 Self->TBufferSize = Self->TBufferSize + 1024;
                                 goto repeat;
                              }
                           }
                        }
                        error = ERR_Okay; // For fields, error code is always Okay so that the reference evaluates to NULL
                     }
                     else error = ERR_AccessObject;
                  }
                  else {
                     // Convert the object reference to an ID
                     Self->TBuffer[0] = '#';
                     IntToStr(objectid, Self->TBuffer+1, Self->TBufferSize-1);
                     error = ERR_Okay;
                  }

                  if (!error) {
                     error = insert_string(Self->TBuffer, Buffer, BufferLength, pos, endbracket-pos+1);
                     if (object) ReleaseObject(object);

                     if (error) {
                        log.warning("Buffer overflow (%d bytes) while inserting to buffer \"%.30s\"", BufferLength, Buffer);
                        if (calcbuffer) free(calcbuffer);
                        return ERR_BufferOverflow;
                     }
                  }
                  else if (object) ReleaseObject(object);
               }
               else {
                  error = ERR_NoMatchingObject;
                  log.traceWarning("Failed to find object '%s'", name);
               }
            }
         }

         if (error != ERR_Okay) {
            if (Flags & SEF_STRICT) {
               // Do not delete everything in square brackets if the STRICT flags is used and retain the error code.
               pos--;
               majorerror = error;
            }
            else {
               // If an error occurred, delete everything contained by the square brackets to prevent recursion errors.

               for (i=endbracket+1; Buffer[i]; i++) Buffer[pos++] = Buffer[i];
               Buffer[pos] = 0;
            }
            error = ERR_Okay;
         }
      }
      else pos--;
   }

   log.trace("Result: %s", Buffer);

   if (calcbuffer) free(calcbuffer);
   return majorerror;
}

//********************************************************************************************************************

static bool eval_condition(CSTRING String)
{
   parasol::Log log(__FUNCTION__);

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

   if (!String) return false;
   while ((*String) and (*String <= 0x20)) String++;

   bool reverse = false;

   // Find the condition statement

   LONG i;
   for (i=0; String[i]; i++) {
      if ((String[i] IS '!') and (String[i+1] IS '=')) break;
      if (String[i] IS '>') break;
      if (String[i] IS '<') break;
      if (String[i] IS '=') break;
   }

   // If there is no condition statement, evaluate the statement as an integer

   if (!String[i]) {
      if (StrToInt(String)) return true;
      else return false;
   }

   LONG cpos = i;

   // Test field

   while ((i > 0) and (String[i-1] IS ' ')) i--;
   char test[i+1];
   CopyMemory(String, test, i);
   test[i] = 0;

   // Condition field

   LONG condition = 0;
   {
      char cond[3];
      UBYTE c;
      for (i=cpos,c=0; (c < 2) and ((String[i] IS '!') or (String[i] IS '=') or (String[i] IS '>') or (String[i] IS '<')); i++) {
         cond[c++] = String[i];
      }
      cond[c] = 0;

      LONG j;
      for (j=0; table[j].Name; j++) {
         if (!StrMatch(cond, table[j].Name)) {
            condition = table[j].Value;
            break;
         }
      }
   }

   while ((String[i]) and (String[i] <= 0x20)) i++; // skip white-space

   bool truth = false;
   if (test[0]) {
      if (condition) {
         truth = test_statement(test, String+i, condition);
      }
      else log.warning("No test condition in \"%s\".", String);
   }
   else log.warning("No test value in \"%s\".", String);

   if (reverse) return truth ^ 1;
   else return truth;
}

//********************************************************************************************************************

INLINE BYTE sortseg_compare(extDocument *Self, SortSegment *Left, SortSegment *Right)
{
   if (Left->Y < Right->Y) return 1;
   else if (Left->Y > Right->Y) return -1;
   else {
      if (Self->Segments[Left->Segment].X < Self->Segments[Right->Segment].X) return 1;
      else if (Self->Segments[Left->Segment].X > Self->Segments[Right->Segment].X) return -1;
      else return 0;
   }
}

//********************************************************************************************************************
// Assists in the translation of URI strings where escape codes may be used.

INLINE LONG uri_char(CSTRING *Source, STRING Dest, LONG Size)
{
   CSTRING src = Source[0];
   if ((src[0] IS '%') and (src[1] >= '0') and (src[1] <= '9') and (src[2] >= '0') and (src[2] <= '9')) {
      Dest[0] = ((src[1] - '0') * 10) | (src[2] - '0');
      src += 3;
      Source[0] = src;
      return 1;
   }
   else if (Size > 1) {
      *Dest = src[0];
      Source[0] = Source[0] + 1;
      return 1;
   }
   else return 0;
}

//********************************************************************************************************************

static ERROR consume_input_events(const InputEvent *Events, LONG Handle)
{
   auto Self = (extDocument *)CurrentContext();

   for (auto input=Events; input; input=input->Next) {
      if (input->Flags & JTYPE_MOVEMENT) {
         for (auto scan=input->Next; (scan) and (scan->Flags & JTYPE_MOVEMENT); scan=scan->Next) {
            input = scan;
         }

         if (input->OverID IS Self->PageID) Self->MouseOver = TRUE;
         else Self->MouseOver = FALSE;

         check_mouse_pos(Self, input->X, input->Y);

         // Note that this code has to 'drop through' due to the movement consolidation loop earlier in this subroutine.
      }

      if (input->Type IS JET_LMB) {
         if (input->Value > 0) {
            Self->LMB = TRUE;
            check_mouse_click(Self, input->X, input->Y);
         }
         else {
            Self->LMB = FALSE;
            check_mouse_release(Self, input->X, input->Y);
         }
      }
   }

   return ERR_Okay;
}

//********************************************************************************************************************
// Checks if the file path is safe, i.e. does not refer to an absolute file location.

static LONG safe_file_path(extDocument *Self, CSTRING Path)
{
   if (Self->Flags & DCF_UNRESTRICTED) return TRUE;





   return FALSE;
}

//********************************************************************************************************************
// Used by if, elseif, while statements to check the satisfaction of conditions.

static BYTE check_tag_conditions(extDocument *Self, XMLTag *Tag)
{
   parasol::Log log("eval");

   BYTE satisfied = FALSE;
   BYTE reverse = FALSE;
   for (LONG i=0; i < Tag->TotalAttrib; i++) {
      if (!StrMatch("statement", Tag->Attrib[i].Name)) {
         satisfied = eval_condition(Tag->Attrib[i].Value);
         log.trace("Statement: %s", Tag->Attrib[i].Value);
         break;
      }
      else if (!StrMatch("exists", Tag->Attrib[i].Name)) {
         OBJECTID object_id;
         if (!FindObject(Tag->Attrib[i].Value, 0, FOF_SMART_NAMES, &object_id)) {
            if (valid_objectid(Self, object_id)) {
               satisfied = TRUE;
            }
         }
         break;
      }
      else if (!StrMatch("notnull", Tag->Attrib[i].Name)) {
         log.trace("NotNull: %s", Tag->Attrib[i].Value);
         if (Tag->Attrib[i].Value) {
            if (!Tag->Attrib[i].Value[0]) {
               satisfied = FALSE;
            }
            else if ((Tag->Attrib[i].Value[0] IS '0') and (!Tag->Attrib[i].Value[1])) {
               satisfied = FALSE;
            }
            else satisfied = TRUE;
         }
         else satisfied = FALSE;
      }
      else if ((!StrMatch("isnull", Tag->Attrib[i].Name)) or (!StrMatch("null", Tag->Attrib[i].Name))) {
         log.trace("IsNull: %s", Tag->Attrib[i].Value);
         if (Tag->Attrib[i].Value) {
            if (!Tag->Attrib[i].Value[0]) {
               satisfied = TRUE;
            }
            else if ((Tag->Attrib[i].Value[0] IS '0') and (!Tag->Attrib[i].Value[1])) {
               satisfied = TRUE;
            }
            else satisfied = FALSE;
         }
         else satisfied = TRUE;
      }
      else if (!StrMatch("not", Tag->Attrib[i].Name)) {
         reverse = TRUE;
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

static ERROR insert_xml(extDocument *Self, objXML *XML, XMLTag *Tag, LONG Index, UBYTE Flags)
{
   parasol::Log log(__FUNCTION__);
   objFont *font;
   LONG insert_index, start;

   if (!Tag) return ERR_Okay;

   if (Index < 0) Index = Self->StreamLen;

   log.traceBranch("Index: %d, Flags: $%.2x, Tag: %s", Index, Flags, Tag->Attrib->Name);

   // Retrieve the most recent font definition and use that as the style that we're going to start with.

   if (Flags & IXF_HOLDSTYLE) {
      // Do nothing to change the style
   }
   else {
      ClearMemory(&Self->Style, sizeof(Self->Style));
      Self->Style.FontStyle.Index = -1;
      Self->Style.FontChange = FALSE;
      Self->Style.StyleChange = FALSE;

      if (Flags & IXF_RESETSTYLE) {
         // Do not search for the most recent font style
      }
      else {
         auto str = Self->Stream;
         LONG i   = Index;
         PREV_CHAR(str, i);
         while (i > 0) {
            if ((str[i] IS CTRL_CODE) and (ESCAPE_CODE(str, i) IS ESC_FONT)) {
               CopyMemory(escape_data<BYTE>(str, i), &Self->Style.FontStyle, sizeof(escFont));
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
         Self->Style.FontChange = TRUE;
      }

      if ((font = lookup_font(Self->Style.FontStyle.Index, "insert_xml"))) {
         StrCopy(font->Face, Self->Style.Face, sizeof(Self->Style.Face));
         Self->Style.Point = font->Point;
      }
   }

   // Parse content and insert it at the end of the stream (we will move it to the insertion point afterwards).

   start = Self->StreamLen;
   insert_index = Self->StreamLen;
   if (Flags & IXF_SIBLINGS) {
      parse_tag(Self, XML, Tag, &insert_index, 0);
   }
   else {
      auto save = Tag->Next;
      Tag->Next = NULL;
      parse_tag(Self, XML, Tag, &insert_index, 0);
      Tag->Next = save;
   }

   if (Flags & IXF_CLOSESTYLE) {
      style_check(Self, &insert_index);
   }

   // Sanity checks

   if (insert_index != Self->StreamLen) {
      log.warning("Index %d does not match stream length %d", Index, Self->StreamLen);
      Self->StreamLen = insert_index;
   }

   if (Self->StreamLen <= start) {
      log.trace("parse_tag() did not insert any content into the stream.");
      return ERR_NothingDone;
   }

   // Move the content from the end of the stream to the requested insertion point

   if (Index < start) {
      STRING content;
      LONG length = Self->StreamLen - start;
      log.trace("Moving new content of %d bytes to the insertion point at index %d", Index, length);
      if (!AllocMemory(length, MEM_DATA|MEM_NO_CLEAR, &content)) {
         CopyMemory(Self->Stream + start, content, length); // Take a copy of the inserted data
         CopyMemory(Self->Stream + Index, Self->Stream + Index + length, start - Index); // Make room for the data at the insertion point
         CopyMemory(content, Self->Stream + Index, length); // Copy data to the insertion point
         FreeResource(content);
      }
   }

   // Check that the FocusIndex is valid (there's a slim possibility that it may not be if AC_Focus has been
   // incorrectly used).

   if (Self->FocusIndex >= Self->TabIndex) Self->FocusIndex = -1;

   return ERR_Okay;
}

//********************************************************************************************************************
// Supported Flags:
//   IPF_NOCONTENT:
//   IPF_STRIPFEEDS:

#define SAVE_ARGS(tag) \
   b_revert = Self->BufferIndex; \
   s_revert = Self->ArgIndex; \
   e_revert = 0; \
   if (convert_xml_args(Self, (tag)->Attrib, (tag)->TotalAttrib) != ERR_Okay) goto next; \
   e_revert = Self->ArgIndex;

#define RESTORE_ARGS() \
   if (e_revert > s_revert) { \
      while (e_revert > s_revert) { \
         e_revert--; \
         Self->VArg[e_revert].Attrib[0] = Self->VArg[e_revert].String; \
      } \
   } \
   Self->BufferIndex = b_revert; \
   Self->ArgIndex = s_revert;

static LONG parse_tag(extDocument *Self, objXML *XML, XMLTag *Tag, LONG *Index, LONG Flags)
{
   parasol::Log log(__FUNCTION__);
   XMLTag *child, *object_template;
   CSTRING content;
   LONG i, b_revert, result, filter;
   UBYTE s_revert, e_revert, template_match;

   if (!Tag) {
      log.traceWarning("Tag parameter not specified.");
      return 0;
   }

   if (Self->Error) {
      log.traceWarning("Error field is set, returning immediately.");
      return 0;
   }

   #ifdef DEBUG
      char tagarg[30];

      if (Tag->Attrib->Name) {
         for (i=0; ((size_t)i < sizeof(tagarg)-1) and (Tag->Attrib->Name[i]); i++) tagarg[i] = Tag->Attrib->Name[i];
      }
      else if (Tag->Attrib->Value) {
         for (i=0; ((size_t)i < sizeof(tagarg)-1) and (Tag->Attrib->Value[i]); i++) {
            if (Tag->Attrib->Value[i] < 0x20) tagarg[i] = '.';
            else tagarg[i] = Tag->Attrib->Value[i];
         }
      }
      else i = 0;

      tagarg[i] = 0;

      log.traceBranch("XML: %d, First-Tag: '%.30s', Face: %.20s, Tag: %p, Flags: $%.8x", XML->UID, tagarg, Self->Style.Face, Tag, Flags);

   #endif

   CSTRING tagname = NULL;
   filter = Flags & 0xffff0000;
   result = 0;

   while (Tag) {
      SAVE_ARGS(Tag);

      #ifdef DEBUG
          if (Tag->Attrib->Name) log.trace("Tag: %s", Tag->Attrib->Name);
          else if (Tag->Attrib->Value) {
             for (i=0; ((size_t)i < sizeof(tagarg)-1) and (Tag->Attrib->Value[i]); i++) {
                if (Tag->Attrib->Value[i] < 0x20) tagarg[i] = '.';
                else tagarg[i] = Tag->Attrib->Value[i];
             }
             tagarg[i] = 0;
             log.trace("Content: %s", tagarg);
          }
      #endif

      child = Tag->Child;

      // If the tag is content based, process it

      if (!(tagname = Tag->Attrib->Name)) {
         if (!(Flags & IPF_NOCONTENT)) {
            if ((content = Tag->Attrib->Value)) {
               if (Flags & IPF_STRIPFEEDS) {
                  while ((*content IS '\n') or (*content IS '\r')) content++;
                  Flags &= ~IPF_STRIPFEEDS;
               }

               if (Self->CurrentObject) {
                  // Objects do not normally accept document content (user should use <xml>)
                  // An exception is made for content that is injected within an object tag.

                  if (XML IS Self->InjectXML) {
                     acDataContent(Self->CurrentObject, content);
                  }
               }
               else if (Self->ParagraphDepth) { // We must be in a paragraph to accept content as text
                  insert_text(Self, Index, content, StrLength(content),
                     (Self->Style.FontStyle.Options & FSO_PREFORMAT) ? TRUE : FALSE);
               }
            }
            else log.traceWarning("Content tag contains no content string.");
         }

         goto next;
      }

      if (tagname[0] IS '$') tagname++;

      // Check for templates first, as they can be used to override the default RPL tag names.

      object_template = NULL;
      template_match = FALSE;
      if (Self->Templates) {
         for (auto scan=Self->Templates->Tags[0]; scan; scan=scan->Next) {
            for (LONG i=0; i < scan->TotalAttrib; i++) {
               if ((!StrMatch("class", scan->Attrib[i].Name)) and
                   (!StrMatch(tagname, scan->Attrib[i].Value))) {
                  object_template = scan;
                  template_match = TRUE;
               }
               else if ((!StrMatch("Name", scan->Attrib[i].Name)) and
                   (!StrMatch(tagname, scan->Attrib[i].Value))) {
                  template_match = TRUE;
               }
            }

            if (template_match) {
               if (object_template) {
                  goto process_object;
               }
               else {
                  // Process the template by jumping into it.  Arguments in the tag are added to a sequential
                  // list that will be processed in reverse by convert_xml_args().

                  if (Self->ArgNestIndex < ARRAYSIZE(Self->ArgNest)) {
                     parasol::Log log(__FUNCTION__);

                     START_TEMPLATE(Tag->Child, XML, Tag); // Required for the <inject/> feature to work inside the template

                        log.traceBranch("Executing template '%s'.", tagname);

                        Self->ArgNest[Self->ArgNestIndex++] = Tag;

                           parse_tag(Self, Self->Templates, scan->Child, Index, Flags);

                        Self->ArgNestIndex--;

                     END_TEMPLATE();
                  }
                  else log.warning("Template nesting limit of %d exceeded - cannot continue.", ARRAYSIZE(Self->ArgNest));

                  goto next;
               }
            }
         }
      }

      // Check if the instruction refers to a reserved tag name listed in glTags.
      // If a match is found, a routine specific to that instruction is executed.
      {
         ULONG taghash = StrHash(tagname, 0);

         for (LONG i=0; glTags[i].TagHash; i++) {
            if (glTags[i].TagHash IS taghash) {
               if ((glTags[i].Flags & FILTER_ALL) and (!(glTags[i].Flags & filter))) {
                  // A filter applies to this tag and the filter flags do not match
                  log.warning("Invalid use of tag '%s' - Not applied to the correct tag parent.", tagname);
                  Self->Error = ERR_InvalidData;
               }
               else if (glTags[i].Routine) {
                  //log.traceBranch("%s", tagname);

                  if ((Self->CurrentObject) and (!(glTags[i].Flags & (TAG_OBJECTOK|TAG_CONDITIONAL)))) {
                     log.warning("Illegal use of tag %s within object of class '%s'.", tagname, Self->CurrentObject->Class->ClassName);
                     continue;
                  }

                  if (glTags[i].Flags & TAG_PARAGRAPH) Self->ParagraphDepth++;

                  if ((Flags & IPF_NOCONTENT) and (glTags[i].Flags & TAG_CONTENT)) {
                     // Do nothing when content is not allowed
                     log.trace("Content disabled on '%s', tag not processed.", tagname);
                  }
                  else if (glTags[i].Flags & TAG_CHILDREN) {
                     // Child content is compulsory or tag has no effect
                     if (child) glTags[i].Routine(Self, XML, Tag, child, Index, Flags);
                     else log.trace("No content found in tag '%s'", tagname);
                  }
                  else glTags[i].Routine(Self, XML, Tag, child, Index, Flags);

                  if (glTags[i].Flags & TAG_PARAGRAPH) Self->ParagraphDepth--;
               }
               goto next;
            }
         }
      }
      // Some reserved tag names are not listed in glTags as they affect the flow of the parser and are dealt with here.

      if (!StrMatch("break", tagname)) {
         // Breaking stops executing all tags (within this section) beyond the breakpoint.  If in a loop, the loop
         // will stop executing.

         result |= TRF_BREAK;
         RESTORE_ARGS();
         break;
      }
      else if (!StrMatch("continue", tagname)) {
         // Continuing - does the same thing as a break but the loop continues.
         // If used when not in a loop, then all sibling tags are skipped.

         result |= TRF_CONTINUE;
         RESTORE_ARGS();
         break;
      }
      else if (!StrMatch("if", tagname)) {
         if (check_tag_conditions(Self, Tag)) {
            result = parse_tag(Self, XML, Tag->Child, Index, Flags);
         }
         else if (Tag->Next) {
            // Check for an else statement in the next tag, if discovered then we can jump to it immediately.

            RESTORE_ARGS();

            auto scan = Tag->Next; // Skip content tags
            while ((scan) and (!scan->Attrib->Name)) scan = scan->Next;

            while (scan) {
               if (!StrMatch("elseif", scan->Attrib->Name)) {
                  Tag = scan;

                  SAVE_ARGS(scan);
                  i = check_tag_conditions(Self, scan);
                  RESTORE_ARGS();

                  if (i) {
                     result = parse_tag(Self, XML, scan->Child, Index, Flags);
                     break;
                  }
               }
               else if (!StrMatch("else", scan->Attrib->Name)) {
                  Tag = scan;
                  result = parse_tag(Self, XML, scan->Child, Index, Flags);
                  break;
               }
               else if (!scan->Attrib->Name); // Ignore any content inbetween if/else tags
               else break;

               scan = scan->Next;
            }

            goto next_skiprestore;
         }

         if (result & (TRF_CONTINUE|TRF_BREAK)) {
            RESTORE_ARGS();
            break;
         }
      }
      else if (!StrMatch("while", tagname)) {
         if ((Tag->Child) and (check_tag_conditions(Self, Tag))) {
            // Save/restore the statement string on each cycle to fully evaluate the condition each time.

            LONG saveindex = Self->LoopIndex;
            Self->LoopIndex = 0;

            RESTORE_ARGS();

            i = TRUE;
            while (i) {
               SAVE_ARGS(Tag);
               i = check_tag_conditions(Self, Tag);
               RESTORE_ARGS();

               if ((i) and (parse_tag(Self, XML, Tag->Child, Index, Flags) & TRF_BREAK)) break;

               Self->LoopIndex++;
            }

            Self->LoopIndex = saveindex;

            goto next_skiprestore;
         }
      }
      else {
process_object:
         if (!(Flags & IPF_NOCONTENT)) {
            // Check if the tagname refers to a class.  For security reasons, we limit the classes that can be embedded
            // in functional pages.

            if (!StrCompare("obj:", tagname, 4, 0)) tagname += 4;

            CSTRING pagetarget = NULL;
            CLASSID class_id = 0;
            for (i=0; i < ARRAYSIZE(glDocClasses); i++) {
               if (!StrMatch(tagname, glDocClasses[i].ClassName)) {
                  pagetarget = glDocClasses[i].PageTarget;
                  class_id = glDocClasses[i].ClassID;
                  break;
               }
            }

            if ((i >= ARRAYSIZE(glDocClasses)) and (Self->Flags & DCF_UNRESTRICTED)) {
               class_id = ResolveClassName(tagname);
            }

            if (class_id) {
               tag_object(Self, pagetarget, class_id, object_template, XML, Tag, child, Index, 0, &s_revert, &e_revert, &b_revert);
            }
            else log.warning("Tag '%s' unsupported as an instruction, template or class.", tagname);
         }
         else log.warning("Unrecognised tag '%s' used in a content-restricted area.", tagname);
      }

next:

      RESTORE_ARGS();

next_skiprestore:

      if (Self->Error) break;
      Tag = Tag->Next;
   }

   return result;
}

//********************************************************************************************************************

static void style_check(extDocument *Self, LONG *Index)
{
   if (Self->Style.FontChange) {
      // Create a new font object for the current style

      CSTRING style_name = get_font_style(Self->Style.FontStyle.Options);
      Self->Style.FontStyle.Index = create_font(Self->Style.Face, style_name, Self->Style.Point);
      //log.trace("Changed font to index %d, face %s, style %s, size %d.", Self->Style.FontStyle.Index, Self->Style.Face, style_name, Self->Style.Point);
      Self->Style.FontChange  = FALSE;
      Self->Style.StyleChange = TRUE;
   }

   if (Self->Style.StyleChange) {
      // Insert a font change into the text stream
      //log.trace("Style change detected.");
      insert_escape(Self, Index, ESC_FONT, &Self->Style.FontStyle, sizeof(Self->Style.FontStyle));
      Self->Style.StyleChange = FALSE;
   }
}

//********************************************************************************************************************
// Inserts plain UTF8 text into the document stream.  Insertion can be at any byte index, indicated by the Index
// parameter.  The Index value will be increased by the number of bytes to insert, indicated by Length.  The Document's
// StreamLen will have increased by Length on this function's return.
//
// Preformat must be set to TRUE if all consecutive whitespace characters in Text are to be inserted.

static ERROR insert_text(extDocument *Self, LONG *Index, CSTRING Text, LONG Length, BYTE Preformat)
{
   UBYTE *stream;
   LONG pos, i;

#ifdef DBG_STREAM
   parasol::Log log(__FUNCTION__);
   log.trace("Index: %d, WSpace: %d", *Index, Self->NoWhitespace);
#endif

   if (!Length) return ERR_Okay;

   // Check if there is content to be processed

   if ((Preformat IS FALSE) and (Self->NoWhitespace)) {
      for (i=0; i < Length; i++) if (Text[i] > 0x20) break;
      if (i IS Length) return ERR_Okay;
   }

   style_check(Self, Index);

   LONG size = Self->StreamLen + Length + 1;
   if (size >= Self->StreamSize) {
      // The size of the text extends past the stream buffer, so allocate more space.

      size += (BUFFER_BLOCK > Length) ? BUFFER_BLOCK : Length;
      if (!AllocMemory(size, MEM_NO_CLEAR, &stream)) {
         if (*Index > 0) CopyMemory(Self->Stream, stream, *Index);

         pos = *Index;

         if (Preformat) {
            for (i=0; i < Length; i++) {
               if (Text[i] IS CTRL_CODE) stream[pos+i] = ' ';
               else stream[pos+i] = Text[i];
            }
            pos += Length;
         }
         else {
            for (i=0; i < Length; ) {
               if (Text[i] <= 0x20) { // Whitespace eliminator, also handles any unwanted presence of ESC_CODE which is < 0x20
                  while ((Text[i] <= 0x20) and (i < Length)) i++;
                  if (!Self->NoWhitespace) {
                     stream[pos++] = ' ';
                  }
                  Self->NoWhitespace = TRUE;
               }
               else {
                  stream[pos++] = Text[i++];
                  Self->NoWhitespace = FALSE;
               }
            }
            Length = pos - *Index;
         }

         if (*Index < Self->StreamLen) {
            CopyMemory(Self->Stream + *Index, stream+pos, Self->StreamLen - *Index);
            pos += Self->StreamLen - *Index;
         }
         stream[pos] = 0;

         FreeResource(Self->Stream);
         Self->Stream = stream;
         Self->StreamSize = size;
      }
      else return ERR_AllocMemory;
   }
   else {
      if (Self->Stream[*Index]) {
         // Make room at the insertion point
         CopyMemory(Self->Stream + *Index,
            Self->Stream + *Index + Length,
            Self->StreamLen - *Index);
      }

      auto stream = Self->Stream;
      pos = *Index;

      if (Preformat) {
         for (LONG i=0; i < Length; i++) {
            if (Text[i] IS CTRL_CODE) stream[pos+i] = ' ';
            else stream[pos+i] = Text[i];
         }
         pos += Length;
      }
      else {
         for (LONG i=0; i < Length; ) {
            if (Text[i] <= 0x20) { // Whitespace eliminator, also handles any unwanted presence of ESC_CODE which is < 0x20
               while ((Text[i] <= 0x20) and (i < Length)) i++;
               if (!Self->NoWhitespace) stream[pos++] = ' ';
               Self->NoWhitespace = TRUE;
            }
            else {
               stream[pos++] = Text[i++];
               Self->NoWhitespace = FALSE;
            }
         }
         Length = pos - *Index; // Recalc the length
      }
   }

   *Index += Length;
   Self->StreamLen += Length;
   Self->Stream[Self->StreamLen] = 0;
   return ERR_Okay;
}

//********************************************************************************************************************
// Inserts an escape sequence into the text stream.
//
// ESC,Code,Length[2],...Data...,Length[2],ESC

static ERROR insert_escape(extDocument *Self, LONG *Index, WORD EscapeCode, APTR Data, LONG Length)
{
   parasol::Log log(__FUNCTION__);
   UBYTE *stream;

#ifdef DBG_STREAM
   log.trace("Index: %d, Code: %s (%d), Length: %d", *Index, strCodes[EscapeCode], EscapeCode, Length);
#else
   //log.trace("Index: %d, Code: %d, Length: %d", *Index, EscapeCode, Length);
#endif

   if (Length > 0xffff - ESC_LEN) {
      log.warning("Escape code length of %d exceeds %d bytes.", Length, 0xffff - ESC_LEN);
      return ERR_BufferOverflow;
   }

   LONG element_id = ++Self->ElementCounter;
   LONG size = Self->StreamLen + Length + ESC_LEN + 1;
   LONG total_length = Length + ESC_LEN;
   if (size >= Self->StreamSize) {
      size += BUFFER_BLOCK;
      if (!AllocMemory(size, MEM_NO_CLEAR, &stream)) {
         if (*Index > 0) CopyMemory(Self->Stream, stream, *Index);

         LONG pos = *Index;
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

         if (*Index < Self->StreamLen) {
            CopyMemory(Self->Stream + *Index, stream+pos, Self->StreamLen - *Index);
            pos += Self->StreamLen - *Index;
         }

         FreeResource(Self->Stream);
         Self->Stream = stream;
         Self->StreamSize = size;
      }
      else return ERR_AllocMemory;
   }
   else {
      if (Self->Stream[*Index]) {
         CopyMemory(Self->Stream + *Index,
            Self->Stream + *Index + Length + ESC_LEN,
            Self->StreamLen - *Index);
      }

      stream = Self->Stream;
      LONG pos = *Index;
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
   }

   Self->StreamLen += Length + ESC_LEN;
   Self->Stream[Self->StreamLen] = 0;
   *Index += Length + ESC_LEN;
   return ERR_Okay;
}

//********************************************************************************************************************
// This function is called only when a paragraph or explicit line-break (\n) is encountered.

static void end_line(extDocument *Self, layout *l, LONG NewLine, LONG Index, DOUBLE Spacing, LONG RestartIndex, CSTRING Caller)
{
   LONG i, bottomline, new_y;

   if ((!l->line_height) and (l->wordwidth)) {
      // If this is a one-word line, the line height will not have been defined yet
      //log.trace("Line Height being set to font (currently %d/%d)", l->line_height, l->base_line);
      l->line_height = l->font->LineSpacing;
      l->base_line   = l->font->Ascent;
   }

   LAYOUT("~end_line()","%s: CursorY: %d, ParaY: %d, ParaEnd: %d, Line Height: %d * %.2f, Index: %d/%d, Restart: %d", Caller, l->cursory, l->paragraph_y, l->paragraph_end, l->line_height, Spacing, l->line_index, Index, RestartIndex);

   for (LONG i=l->start_clips; i < Self->TotalClips; i++) {
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

      if ((i = Index) > 0) {
         PREV_CHAR(Self->Stream, i);
         while (i > 0) {
            if (Self->Stream[i] IS CTRL_CODE) {
               if ((ESCAPE_CODE(Self->Stream, i) IS ESC_PARAGRAPH_END) or
                   (ESCAPE_CODE(Self->Stream, i) IS ESC_PARAGRAPH_START)) {

                  if (ESCAPE_CODE(Self->Stream, i) IS ESC_PARAGRAPH_START) {
                     // Check if a custom string is specified in the paragraph, in which case the paragraph counts
                     // as content.

                     auto para = escape_data<escParagraph>(Self->Stream, i);
                     if (para->CustomString) break;
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
   l->split_start = Self->SegCount;
   l->line_index  = RestartIndex;
   l->wordindex   = l->line_index;
   l->kernchar    = 0;
   l->wordwidth   = 0;
   l->paragraph_end = 0;
   LAYOUT_LOGRETURN();
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
   parasol::Log log(__FUNCTION__);
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

   if (Self->TotalClips > 0) check_clips(Self, Index, l, ObjectIndex, GraphicX, GraphicY, GraphicWidth, GraphicHeight);

   if (*GraphicX + GraphicWidth > l->wrapedge) {
      if ((*Width < WIDTH_LIMIT) and ((*GraphicX IS l->left_margin) or (l->nowrap))) {
         // Force an extension of the page width and recalculate from scratch
         minwidth = *GraphicX + GraphicWidth + l->right_margin - X;
         if (minwidth > *Width) {
            *Width = minwidth;
            WRAP("check_wrap:","Forcing an extension of the page width to %d", minwidth);
         }
         else *Width += 1;
         WRAP_LOGRETURN();
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
         l->split_start  = Self->SegCount;
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

   if ((l->link) and (l->link_open IS FALSE)) {
      // A link is due to be closed
      add_link(Self, ESC_LINK, l->link, l->link_x, *GraphicY, *GraphicX + GraphicWidth - l->link_x, l->line_height ? l->line_height : l->font->LineSpacing, "check_wrap");
      l->link = NULL;
   }

   #ifdef DBG_WORDWRAP
      if (result IS WRAP_WRAPPED) WRAP("check_wrap","A wrap to Y coordinate %d has occurred.", l->cursory);
   #endif

   WRAP_LOGRETURN();
   return result;
}

static void check_clips(extDocument *Self, LONG Index, layout *l,
   LONG ObjectIndex, LONG *GraphicX, LONG *GraphicY, LONG GraphicWidth, LONG GraphicHeight)
{
   LONG clip, i, height;
   UBYTE reset_link;

   WRAP("~check_clips()","Index: %d-%d, ObjectIndex: %d, Graphic: %dx%d,%dx%d, TotalClips: %d", l->line_index, Index, ObjectIndex, *GraphicX, *GraphicY, GraphicWidth, GraphicHeight, Self->TotalClips);

   for (clip=l->start_clips; clip < Self->TotalClips; clip++) {
      if (Self->Clips[clip].Transparent) continue;
      if (*GraphicY + GraphicHeight < Self->Clips[clip].Clip.Top) continue;
      if (*GraphicY >= Self->Clips[clip].Clip.Bottom) continue;
      if (*GraphicX >= Self->Clips[clip].Clip.Right) continue;
      if (*GraphicX + GraphicWidth < Self->Clips[clip].Clip.Left) continue;

      if (Self->Clips[clip].Clip.Left < l->alignwidth) l->alignwidth = Self->Clips[clip].Clip.Left;

      WRAP("check_clips:","Word: \"%.20s\" (%dx%d,%dx%d) advances over clip %d-%d",
         printable(Self->Stream+ObjectIndex, 60), *GraphicX, *GraphicY, GraphicWidth, GraphicHeight,
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

         WRAP_LOGRETURN();

         reset_link = TRUE;
      }
      else reset_link = FALSE;

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

   WRAP_LOGRETURN();
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
   layout Layout;
   LONG Index;
   LONG TotalClips;
   LONG TotalLinks;
   LONG SegCount;
   LONG ECIndex;

   LAYOUT_STATE() {
      Index = 0;
      TotalClips = 0;
      TotalLinks = 0;
      SegCount = 0;
      ECIndex = 0;
   }
};

#define SAVE_STATE(s) \
   CopyMemory(&l, &s.Layout, sizeof(l)); \
   s.Index      = i; \
   s.TotalClips = Self->TotalClips; \
   s.TotalLinks = Self->TotalLinks; \
   s.ECIndex    = Self->ECIndex; \
   s.SegCount   = Self->SegCount;

// You must execute a goto to the point at which SAVE_STATE() was used after calling this macro

#define RESTORE_STATE(s) \
   restore_state(Self, s); \
   l = (s)->Layout; \
   i = (s)->Index;

INLINE void restore_state(extDocument *Self, LAYOUT_STATE *State)
{
   LAYOUT("layout_restore:","Restoring earlier layout state to index %d", State->Index);

   Self->TotalClips = State->TotalClips;
   Self->TotalLinks = State->TotalLinks;
   Self->SegCount   = State->SegCount;
   Self->ECIndex    = State->ECIndex;
}

static LONG layout_section(extDocument *Self, LONG Offset, objFont **Font,
   LONG AbsX, LONG AbsY, LONG *Width, LONG *Height,
   LONG LeftMargin, LONG TopMargin, LONG RightMargin, LONG BottomMargin, BYTE *VerticalRepass)
{
   parasol::Log log(__FUNCTION__);
   layout l;
   escFont *style;
   escAdvance *advance;
   escObject *escobj;
   escList *esclist;
   escCell *esccell;
   escRow *escrow, *lastrow;
   DocEdit *edit;
   escTable *esctable;
   escParagraph *escpara;
   LAYOUT_STATE tablestate, rowstate, liststate;
   LONG start_ecindex, start_links, start_SegCount, unicode, i, j, page_height, lastheight, lastwidth, edit_segment;
   UBYTE checkwrap;
   BYTE object_vertical_repass;

   if ((!Self->Stream) or (!Self->Stream[Offset]) or (!Font)) {
      log.trace("No document stream to be processed.");
      return 0;
   }

   if (Self->Depth >= MAX_DEPTH) {
      log.trace("Depth limit exceeded (too many tables-within-tables).");
      return 0;
   }

   start_links    = Self->TotalLinks;
   start_SegCount = Self->SegCount;
   l.start_clips  = Self->TotalClips;
   start_ecindex  = Self->ECIndex;
   page_height    = *Height;
   object_vertical_repass = FALSE;

   *VerticalRepass = FALSE;

   LAYOUT("~layout_section()","Dimensions: %dx%d,%dx%d (edge %d), LM %d RM %d TM %d BM %d",
      AbsX, AbsY, *Width, *Height, AbsX + *Width - RightMargin,
      LeftMargin, RightMargin, TopMargin, BottomMargin);

   Self->Depth++;

extend_page:
   if (*Width > WIDTH_LIMIT) {
      LAYOUT("layout_section:","Restricting page width from %d to %d", *Width, WIDTH_LIMIT);
      *Width = WIDTH_LIMIT;
      if (Self->BreakLoop > 4) Self->BreakLoop = 4; // Very large page widths normally means that there's a parsing problem
   }

   if (Self->Error) {
      Self->Depth--;
      LAYOUT_LOGRETURN();
      return 0;
   }
   else if (!Self->BreakLoop) {
      Self->Error = ERR_Loop;
      Self->Depth--;
      LAYOUT_LOGRETURN();
      return 0;
   }
   Self->BreakLoop--;

   Self->TotalLinks = start_links;     // Also refer to SAVE_STATE() and restore_state()
   Self->SegCount   = start_SegCount;
   Self->TotalClips = l.start_clips;
   Self->ECIndex    = start_ecindex;

   lastrow     = NULL; // For table management
   lastwidth   = *Width;
   lastheight  = page_height;
   esclist     = NULL;
   escrow      = NULL;
   esctable    = NULL;
   escpara     = NULL;
   edit        = NULL;
   esccell     = NULL;
   style       = NULL;
   edit_segment = 0;
   checkwrap   = FALSE; // TRUE if a wordwrap or collision check is required
   l.anchor        = FALSE; // TRUE if in an anchored section (objects are anchored to the line)
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
   l.split_start   = Self->SegCount;  // Set to the previous line index if line is segmented.  Used for ensuring that all distinct entries on the line use the same line height
   l.font          = *Font;
   l.nowrap        = FALSE; // TRUE if word wrapping is to be turned off
   l.link_open     = FALSE;
   l.setsegment    = FALSE;
   l.textcontent   = FALSE;
   l.spacewidth    = fntCharWidth(l.font, ' ', 0, NULL);

   i = Offset;

   while (1) {
      // For certain graphics-related escape codes, set the line segment up to the encountered escape code if the text
      // string will be affected (e.g. if the string will be broken up due to a clipping region etc).

      if (Self->Stream[i] IS CTRL_CODE) {
         // Any escape code that sets l.setsegment to TRUE in its case routine, must set breaksegment to TRUE now so
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
                     style = escape_data<escFont>(Self->Stream, i);
                     objFont *font = lookup_font(style->Index, "ESC_FONT");
                     if (l.font != font) breaksegment = 1;
                  }
                  break;

               case ESC_OBJECT:
                  escobj = escape_data<escObject>(Self->Stream, i);
                  if (escobj->Graphical) breaksegment = 1;
                  break;

               case ESC_INDEX_START: {
                  auto index = escape_data<escIndex>(Self->Stream, i);
                  if (!index->Visible) breaksegment = 1;
                  break;
               }
            }

            if (breaksegment) {
               LAYOUT("~layout_section:","Setting line at escape '%s', index %d, line_x: %d, wordwidth: %d", strCodes[(UBYTE)ESCAPE_CODE(Self->Stream,i)], l.line_index, l.line_x, l.wordwidth);
                  l.cursorx += l.wordwidth;
                  add_drawsegment(Self, l.line_index, i, &l, l.cursory, l.cursorx - l.line_x, l.alignwidth - l.line_x, "Esc:Object");
                  RESET_SEGMENT_WORD(i, l.cursorx, &l);
                  l.alignwidth = l.wrapedge;
               LAYOUT_LOGRETURN();
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
                  checkwrap = TRUE;
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
         else checkwrap = TRUE;

         if (checkwrap) {
            LONG wrap_result;

            checkwrap = FALSE;

            wrap_result = check_wordwrap("Text", Self, i, &l, AbsX, Width, l.wordindex, &l.cursorx, &l.cursory, (l.wordwidth < 1) ? 1 : l.wordwidth, (l.line_height < 1) ? 1 : l.line_height);

            if (wrap_result IS WRAP_EXTENDPAGE) {
               LAYOUT("layout_section:","Expanding page width on wordwrap request.");
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
         LAYOUT("layout_section","ESC_%s: %p, Index: %d-%d-%d, Length: %d, WordWidth: %d", strCodes[ESCAPE_CODE(Self->Stream, i)], esctable, l.line_index, i, l.wordindex, ESCAPE_LEN(Self->Stream+i), l.wordwidth);
#endif
         l.setsegment = FALSE; // Escape codes that draw something in draw_document() (e.g. object, table) should set this flag to TRUE in their case statement
         l.len = ESCAPE_LEN(Self->Stream+i);
         switch (ESCAPE_CODE(Self->Stream, i)) {
            case ESC_ADVANCE:
               advance = escape_data<escAdvance>(Self->Stream, i);
               l.cursorx += advance->X;
               l.cursory += advance->Y;
               if (advance->X) {
                  RESET_SEGMENT_WORD(i, l.cursorx, &l);
               }
               break;

            case ESC_FONT:
               style = escape_data<escFont>(Self->Stream, i);
               l.font = lookup_font(style->Index, "ESC_FONT");

               if (l.font) {
                  if (style->Options & FSO_ALIGN_RIGHT) l.font->Align = ALIGN_RIGHT;
                  else if (style->Options & FSO_ALIGN_CENTER) l.font->Align = ALIGN_HORIZONTAL;
                  else l.font->Align = 0;

                  if (style->Options & FSO_ANCHOR) l.anchor = TRUE;
                  else l.anchor = FALSE;

                  if (style->Options & FSO_NO_WRAP) {
                     l.nowrap = TRUE;
                     //wrapedge = 1000;
                  }
                  else l.nowrap = FALSE;

                  LAYOUT("layout_section:","Font Index: %d, LineSpacing: %d, Height: %d, Ascent: %d, Cursor: %dx%d", style->Index, l.font->LineSpacing, l.font->Height, l.font->Ascent, l.cursorx, l.cursory);
                  l.spacewidth = fntCharWidth(l.font, ' ', 0, 0);

                  // Treat the font as if it is a text character by setting the wordindex.  This ensures it is included in the drawing process

                  if (!l.wordwidth) l.wordindex = i;
               }
               else LAYOUT("layout_section:","ESC_FONT: Unable to lookup font using style index %d.", style->Index);

               break;

            case ESC_INDEX_START: {
               // Indexes don't do anything, but recording the cursor's Y value when they are encountered
               // makes it really easy to scroll to a bookmark when requested (show_bookmark()).

               LONG end;
               auto escindex = escape_data<escIndex>(Self->Stream, i);
               escindex->Y = l.cursory;

               if (!escindex->Visible) {
                  // If Visible is false, then all content within the index is not to be displayed

                  end = i;
                  while (Self->Stream[end]) {
                     if (Self->Stream[end] IS CTRL_CODE) {
                        if (ESCAPE_CODE(Self->Stream, end) IS ESC_INDEX_END) {
                           escIndexEnd *iend = escape_data<escIndexEnd>(Self->Stream, end);
                           if (iend->ID IS escindex->ID) break;
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

            case ESC_SETMARGINS: {
               auto escmargins = escape_data<escSetMargins>(Self->Stream, i);

               if (escmargins->Left != 0x7fff) {
                  l.cursorx     += escmargins->Left;
                  l.line_x      += escmargins->Left;
                  l.left_margin += escmargins->Left;
               }

               if (escmargins->Right != 0x7fff) {
                  l.right_margin += escmargins->Right;
                  l.alignwidth -= escmargins->Right;
                  l.wrapedge   -= escmargins->Right;
               }

               if (escmargins->Top != 0x7fff) {
                  if (l.cursory < AbsY + escmargins->Top) l.cursory = AbsY + escmargins->Top;
               }

               if (escmargins->Bottom != 0x7fff) {
                  BottomMargin += escmargins->Bottom;
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

               l.link       = escape_data<escLink>(Self->Stream, i);
               l.link_x     = l.cursorx + l.wordwidth;
               l.link_index = i;
               l.link_open  = TRUE;
               l.link_align = l.font->Align;
               break;
            }

            case ESC_LINK_END: {
               // We don't call add_link() unless the entire word that contains the link has
               // been processed.  This is necessary due to the potential for a word-wrap.

               if (l.link) {
                  l.link_open = FALSE;

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

               SAVE_STATE(liststate);

               if (esclist) {
                  auto ptr = esclist;
                  esclist = escape_data<escList>(Self->Stream, i);
                  esclist->Stack = ptr;
               }
               else {
                  esclist = escape_data<escList>(Self->Stream, i);
                  esclist->Stack = NULL;
               }
list_repass:
               esclist->Repass = FALSE;
               break;

            case ESC_LIST_END:
               // If it is a custom list, a repass is required

               if ((esclist) and (esclist->Type IS LT_CUSTOM) and (esclist->Repass)) {
                  LAYOUT("esc_list","Repass for list required, commencing...");
                  RESTORE_STATE(&liststate);
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
               SurfaceClip cell;
               OBJECTID object_id;

               // Tell the object our CursorX and CursorY positions so that it can position itself within the stream
               // layout.  The object will tell us its clipping boundary when it returns (if it has a clipping boundary).

               auto escobj = escape_data<escObject>(Self->Stream, i);
               if (!(object_id = escobj->ObjectID)) break;
               if (!escobj->Graphical) break; // Do not bother with objects that do not draw anything
               if (escobj->Owned) break; // Do not manipulate objects that have owners

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
               if (!(error = AccessObjectID(object_id, 5000, &object))) {
                  LAYOUT("layout_object:","[Idx:%d] The %s's available page area is %d-%d,%d-%d, margins %dx%d,%d, cursor %dx%d", i, object->Class->ClassName, cell.Left, cell.Right, cell.Top, cell.Bottom, l.left_margin-AbsX, l.right_margin, TopMargin, l.cursorx, l.cursory);

                  LONG cellwidth, cellheight, align, leftmargin, lineheight, zone_height;
                  OBJECTID layout_surface_id;

                  if ((FindField(object, FID_LayoutSurface, NULL)) and (!object->get(FID_LayoutSurface, &layout_surface_id))) {
                     objSurface *surface;
                     LONG new_x, new_y, new_width, new_height, calc_x;

                     // This layout method is used for objects that do not have a Layout object for graphics management and
                     // simply rely on a Surface object instead.

                     if (!(error = AccessObjectID(layout_surface_id, 3000, &surface))) {
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
                           LAYOUT("@layout_obj","No width specified for %s #%d (dimensions $%x), defaulting to 1 pixel.", object->Class->ClassName, object->UID, surface->Dimensions);
                           new_width = 1;
                        }

                        // X COORD

                        if ((align & ALIGN_HORIZONTAL) and (surface->Dimensions & DMF_WIDTH)) {
                           new_x = new_x + ((cellwidth - new_width)/2);
                        }
                        else if ((align & ALIGN_RIGHT) and (surface->Dimensions & DMF_WIDTH)) {
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

                        LAYOUT("layout_object","Clip region is being restricted to the bounds: %dx%d,%dx%d", new_x, new_y, new_width, new_height);

                        cell.Left  = new_x;
                        cell.Top   = new_y;
                        cell.Right = new_x + new_width;
                        cell.Bottom = new_y + new_height;

                        // If LAYOUT_RIGHT is set, no text may be printed to the right of the object.  This has no impact
                        // on the object's bounds.

                        if (layoutflags & LAYOUT_RIGHT) {
                           LAYOUT("layout_object","LAYOUT_RIGHT: Expanding clip.right boundary from %d to %d.", cell.Right, AbsX + *Width - l.right_margin);
                           cell.Right = (AbsX + *Width) - l.right_margin; //cellwidth;
                        }

                        // If LAYOUT_LEFT is set, no text may be printed to the left of the object (but not
                        // including text that has already been printed).  This has no impact on the object's
                        // bounds.

                        if (layoutflags & LAYOUT_LEFT) {
                           LAYOUT("layout_object","LAYOUT_LEFT: Expanding clip.left boundary from %d to %d.", cell.Left, AbsX);
                           cell.Left  = AbsX; //leftmargin;
                        }

                        if (layoutflags & LAYOUT_IGNORE_CURSOR) width_check = cell.Right - AbsX;
                        else width_check = cell.Right + l.right_margin;

                        LAYOUT("layout_object","#%d, Pos: %dx%d,%dx%d, Align: $%.8x, From: %dx%d,%dx%d,%dx%d, WidthCheck: %d/%d", object->UID, new_x, new_y, new_width, new_height, align, F2T(surface->X), F2T(surface->Y), F2T(surface->Width), F2T(surface->Height), F2T(surface->XOffset), F2T(surface->YOffset), width_check, *Width);
                        LAYOUT("layout_object","Clip Size: %dx%d,%dx%d, LineHeight: %d, LayoutFlags: $%.8x", cell.Left, cell.Top, cellwidth, cellheight, lineheight, layoutflags);

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

                           if (align & ALIGN_HORIZONTAL) {
                              layout->GraphicX = cell.Left + layout->LeftMargin + ((cellwidth - layout->GraphicWidth)>>1);
                           }
                           else if (align & ALIGN_RIGHT) layout->GraphicX = cell.Left + cellwidth - layout->RightMargin - layout->GraphicWidth;
                           else if (!layout->PresetX) {
                              if (!layout->PresetWidth) {
                                 layout->GraphicX = cell.Left + layout->LeftMargin;
                              }
                              else layout->GraphicX = l.cursorx + layout->LeftMargin;
                           }

                           if (align & ALIGN_VERTICAL) layout->GraphicY = cell.Top + ((cellheight - layout->TopMargin - layout->BottomMargin - F2T(layout->GraphicHeight))>>1);
                           else if (align & ALIGN_BOTTOM) layout->GraphicY = cell.Top + cellheight - layout->BottomMargin - layout->GraphicHeight;
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

                           LAYOUT("layout_object","X/Y: %dx%d, W/H: %dx%d (Width/Height are preset)", layout->BoundX, layout->BoundY, layout->BoundWidth, layout->BoundHeight);
                        }
                        else {
                           // If the object does not use preset GraphicWidth and GraphicHeight, then
                           // we just want to use the preset X/Y/Width/Height in relation to the available
                           // space within the cell.

                           layout->ParentSurface.Width = cell.Right - cell.Left;
                           layout->ParentSurface.Height = cell.Bottom - cell.Top;

                           GetFields(object, FID_X|TLONG, &layout->BoundX,
                                             FID_Y|TLONG, &layout->BoundY,
                                             FID_Width|TLONG,  &layout->BoundWidth,
                                             FID_Height|TLONG, &layout->BoundHeight,
                                             TAGEND);

                           layout->BoundX += cell.Left;
                           layout->BoundY += cell.Top;


                           LAYOUT("layout_object","X/Y: %dx%d, W/H: %dx%d, Parent W/H: %dx%d (Width/Height not preset), Dimensions: $%.8x", layout->BoundX, layout->BoundY, layout->BoundWidth, layout->BoundHeight, layout->ParentSurface.Width, layout->ParentSurface.Height, layout->Dimensions);
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

                        if ((align & ALIGN_HORIZONTAL) and (layout->GraphicWidth)) {
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
                        else if ((align & ALIGN_RIGHT) and (layout->GraphicWidth)) {
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
                              LAYOUT("layout_object","Setting BoundWidth from %d to preset GraphicWidth of %d", layout->BoundWidth, layout->GraphicWidth);
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
                              if ((align & ALIGN_HORIZONTAL) and (layout->Dimensions & DMF_WIDTH)) {
                                 LONG new_x = layout->BoundX + ((cellwidth - (layout->BoundWidth + layout->LeftMargin + layout->RightMargin))/2);
                                 if (new_x > layout->BoundX) layout->BoundX = new_x;
                                 layout->BoundX += layout->LeftMargin;
                                 extclip_left = layout->LeftMargin;
                                 extclip_right = layout->RightMargin;
                              }
                              else if ((align & ALIGN_RIGHT) and (layout->Dimensions & DMF_WIDTH)) {
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
                           LAYOUT("layout_object","Expanding BoundHeight from %d to preset GraphicHeight of %d", layout->BoundHeight, layout->GraphicHeight);
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

                           LAYOUT("layout_object","Clip region is being restricted to the bounds: %dx%d,%dx%d", layout->BoundX, layout->BoundY, layout->BoundWidth, layout->BoundHeight);

                           cell.Left  = layout->BoundX - extclip_left;
                           cell.Top   = layout->BoundY - layout->TopMargin;
                           cell.Right = layout->BoundX + layout->BoundWidth + extclip_right;
                           cell.Bottom = layout->BoundY + layout->BoundHeight + layout->BottomMargin;

                           // If LAYOUT_RIGHT is set, no text may be printed to the right of the object.  This has no impact
                           // on the object's bounds.

                           if (layoutflags & LAYOUT_RIGHT) {
                              LAYOUT("layout_object","LAYOUT_RIGHT: Expanding clip.right boundary from %d to %d.", cell.Right, AbsX + *Width - l.right_margin);
                              LONG new_right = (AbsX + *Width) - l.right_margin; //cellwidth;
                              if (new_right > cell.Right) cell.Right = new_right;
                           }

                           // If LAYOUT_LEFT is set, no text may be printed to the left of the object (but not
                           // including text that has already been printed).  This has no impact on the object's
                           // bounds.

                           if (layoutflags & LAYOUT_LEFT) {
                              LAYOUT("layout_object","LAYOUT_LEFT: Expanding clip.left boundary from %d to %d.", cell.Left, AbsX);

                              if (layoutflags & LAYOUT_IGNORE_CURSOR) cell.Left = AbsX;
                              else cell.Left  = l.left_margin;
                           }

                           if (layoutflags & LAYOUT_IGNORE_CURSOR) width_check = cell.Right - AbsX;
                           else width_check = cell.Right + l.right_margin;

                           LAYOUT("layout_object","#%d, Pos: %dx%d,%dx%d, Align: $%.8x, From: %dx%d,%dx%d,%dx%d, WidthCheck: %d/%d", object->UID, layout->BoundX, layout->BoundY, layout->BoundWidth, layout->BoundHeight, align, F2T(layout->X), F2T(layout->Y), F2T(layout->Width), F2T(layout->Height), F2T(layout->XOffset), F2T(layout->YOffset), width_check, *Width);
                           LAYOUT("layout_object","Clip Size: %dx%d,%dx%d, LineHeight: %d, GfxSize: %dx%d, LayoutFlags: $%.8x", cell.Left, cell.Top, cellwidth, cellheight, lineheight, layout->GraphicWidth, layout->GraphicHeight, layoutflags);

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

                  checkwrap = TRUE;

                  // Check if the clipping region is invalid.  Invalid clipping regions are not added to the clip
                  // region list (i.e. layout of document text will ignore the presence of the object).

                  if ((cell.Bottom <= cell.Top) or (cell.Right <= cell.Left)) {
                     CSTRING name;
                     if ((name = GetName(object))) log.warning("%s object %s returned an invalid clip region of %dx%d,%dx%d", object->Class->ClassName, name, cell.Left, cell.Top, cell.Right, cell.Bottom);
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
                        LAYOUT("@layout_object:","Restarting as %s clip.left %d < %d and extends past the page width (%d > %d).", object->Class->ClassName, cell.Left, left_check, width_check, *Width);
                        *Width = cmp_width;
                        goto extend_page;
                     }
                  }
                  else if (width_check > *Width) {
                     // Perform a wrapping check if the object possibly extends past the width of the page/cell.

                     LAYOUT("layout_object","Wrapping %s object #%d as it extends past the page width (%d > %d).  Pos: %dx%d", object->Class->ClassName, object->UID, width_check, *Width, cell.Left, cell.Top);

                     j = check_wordwrap("Object", Self, i, &l, AbsX, Width, i, &cell.Left, &cell.Top, cell.Right - cell.Left, cell.Bottom - cell.Top);

                     if (j IS WRAP_EXTENDPAGE) {
                        LAYOUT("layout_object","Expanding page width due to object size.");
                        goto extend_page;
                     }
                     else if (j IS WRAP_WRAPPED) {
                        LAYOUT("layout_object","Object coordinates wrapped to %dx%d", cell.Left, cell.Top);
                        // The check_wordwrap() function will have reset l.cursorx and l.cursory, so
                        // on our repass, the cell.left and cell.top will reflect this new cursor position.

                        goto wrap_object;
                     }
                  }

                  LAYOUT("layout_object:","Adding %s clip to the list: %dx%d,%dx%d", object->Class->ClassName, cell.Left, cell.Top, cell.Right-cell.Left, cell.Bottom-cell.Top);

                  if ((error = add_clip(Self, &cell, i, object->Class->ClassName, layoutflags & (LAYOUT_BACKGROUND|LAYOUT_FOREGROUND) ? TRUE : FALSE))) {
                     LAYOUT("layout_object:","Error adding clip area.");
                     Self->Error = ERR_Memory;
                     goto exit;
                  }

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
                  LAYOUT("layout_object","Error code #%d during object layout: %s", error, GetErrorMsg(error));
               }

               l.setsegment = TRUE;

               // If the object uses a relative height or vertical offset, a repass will be required if the page height
               // increases.

               if ((dimensions & (DMF_RELATIVE_HEIGHT|DMF_FIXED_Y_OFFSET|DMF_RELATIVE_Y_OFFSET)) and (layoutflags & (LAYOUT_BACKGROUND|LAYOUT_IGNORE_CURSOR))) {
                  LAYOUT("layout_object","Vertical repass may be required.");
                  object_vertical_repass = TRUE;
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

               LONG width;

               SAVE_STATE(tablestate);

               if (esctable) {
                  auto ptr = esctable;
                  esctable = escape_data<escTable>(Self->Stream, i);
                  esctable->Stack = ptr;
               }
               else {
                  esctable = escape_data<escTable>(Self->Stream, i);
                  esctable->Stack = NULL;
               }

               esctable->ResetRowHeight = TRUE; // All rows start with a height of MinHeight up until TABLE_END in the first pass
               esctable->ComputeColumns = 1;
               esctable->Width = -1;

               for (j=0; j < esctable->TotalColumns; j++) esctable->Columns[j].MinWidth = 0;

wrap_table_start:
               // Calculate starting table width, ensuring that the table meets the minimum width according to the cell
               // spacing and padding values.

               if (esctable->WidthPercent) {
                  width = ((*Width - (l.cursorx - AbsX) - l.right_margin) * esctable->MinWidth) / 100;
               }
               else width = esctable->MinWidth;

               if (width < 0) width = 0;

               {
                  LONG min = (esctable->Thickness * 2) + (esctable->CellHSpacing * (esctable->TotalColumns-1)) + (esctable->CellPadding * 2 * esctable->TotalColumns);
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
               esctable->CursorX = l.cursorx;
               esctable->CursorY = l.cursory;
               esctable->X  = l.cursorx;
               esctable->Y  = l.cursory;
               esctable->RowIndex   = 0;
               esctable->TotalClips = Self->TotalClips;
               esctable->Height     = esctable->Thickness;

               LAYOUT("~layout_table:","(i%d) Laying out table of %dx%d, coords %dx%d,%dx%d%s, page width %d.", i, esctable->TotalColumns, esctable->Rows, esctable->X, esctable->Y, esctable->Width, esctable->MinHeight, esctable->HeightPercent ? "%" : "", *Width);
               // NB: LOGRETURN() is matched in ESC_TABLE_END

               if (esctable->ComputeColumns) {
                  // Compute the default column widths

                  esctable->ComputeColumns = 0;
                  esctable->CellsExpanded = FALSE;

                  if (esctable->Columns) {
                     for (j=0; j < esctable->TotalColumns; j++) {
                        //if (esctable->ComputeColumns IS 1) {
                        //   esctable->Columns[j].Width = 0;
                        //   esctable->Columns[j].MinWidth = 0;
                        //}

                        if (esctable->Columns[j].PresetWidth & 0x8000) {
                           // Percentage width value
                           esctable->Columns[j].Width = (DOUBLE)((esctable->Columns[j].PresetWidth & 0x7fff) * esctable->Width) * 0.01;
                        }
                        else if (esctable->Columns[j].PresetWidth) {
                           // Fixed width value
                           esctable->Columns[j].Width = esctable->Columns[j].PresetWidth;
                        }
                        else {
                           esctable->Columns[j].Width = 0;
                        }

                        if (esctable->Columns[j].MinWidth > esctable->Columns[j].Width) esctable->Columns[j].Width = esctable->Columns[j].MinWidth;
                     }
                  }
                  else {
                     log.warning("No columns array defined for table.");
                     esctable->TotalColumns = 0;
                  }
               }

               LAYOUT("layout_table:","Checking for table collisions before layout (%dx%d).  ResetRowHeight: %d", esctable->X, esctable->Y, esctable->ResetRowHeight);

               j = check_wordwrap("Table", Self, i, &l, AbsX, Width, i, &esctable->X, &esctable->Y, (esctable->Width < 1) ? 1 : esctable->Width, esctable->Height);
               if (j IS WRAP_EXTENDPAGE) {
                  LAYOUT("layout_table:","Expanding page width due to table size.");
                  LAYOUT_LOGRETURN();
                  goto extend_page;
               }
               else if (j IS WRAP_WRAPPED) {
                  // The width of the table and positioning information needs
                  // to be recalculated in the event of a table wrap.

                  LAYOUT("layout_table:","Restarting table calculation due to page wrap to position %dx%d.", l.cursorx, l.cursory);
                  esctable->ComputeColumns = 1;
                  LAYOUT_LOGRETURN();
                  goto wrap_table_start;
               }
               l.cursorx = esctable->X;
               l.cursory = esctable->Y;

               l.setsegment = TRUE;

               l.cursory += esctable->Thickness + esctable->CellVSpacing;
               lastrow = NULL;

               break;
            }

            case ESC_TABLE_END: {
               SurfaceClip clip;
               LONG minheight, totalclips;

               if (esctable->CellsExpanded IS FALSE) {
                  DOUBLE cellwidth;
                  LONG unfixed, colwidth;

                  // Table cells need to match the available width inside the table.
                  // This routine checks for that - if the cells are short then the
                  // table processing is restarted.

                  LAYOUT("layout_table_end:","Checking table @ index %d for cell/table widening.  Table width: %d", i, esctable->Width);

                  esctable->CellsExpanded = TRUE;

                  if (esctable->Columns) {
                     colwidth = (esctable->Thickness * 2) + esctable->CellHSpacing;
                     for (j=0; j < esctable->TotalColumns; j++) {
                        colwidth += esctable->Columns[j].Width + esctable->CellHSpacing;
                     }
                     if (esctable->Thin) colwidth -= esctable->CellHSpacing * 2; // Thin tables have no spacing allocated on the sides

                     if (colwidth < esctable->Width) { // Cell layout is less than the pre-determined table width
                        LONG avail_width;

                        // Calculate the amount of additional space that is available for cells to expand into

                        avail_width = esctable->Width - (esctable->Thickness * 2) -
                           (esctable->CellHSpacing * (esctable->TotalColumns - 1));

                        if (!esctable->Thin) avail_width -= (esctable->CellHSpacing * 2);

                        // Count the number of columns that do not have a fixed size

                        unfixed = 0;
                        for (j=0; j < esctable->TotalColumns; j++) {
                           if (esctable->Columns[j].PresetWidth) avail_width -= esctable->Columns[j].Width;
                           else unfixed++;
                        }

                        // Adjust for expandable columns that we know have exceeded the pre-calculated cell width
                        // on previous passes (we want to treat them the same as the PresetWidth columns)  Such cells
                        // will often exist that contain large graphics for example.

                        if (unfixed > 0) {
                           cellwidth = avail_width / unfixed;
                           for (LONG j=0; j < esctable->TotalColumns; j++) {
                              if ((esctable->Columns[j].MinWidth) and (esctable->Columns[j].MinWidth > cellwidth)) {
                                 avail_width -= esctable->Columns[j].MinWidth;
                                 unfixed--;
                              }
                           }

                           if (unfixed > 0) {
                              cellwidth = avail_width / unfixed;
                              bool expanded = FALSE;

                              //total = 0;
                              for (LONG j=0; j < esctable->TotalColumns; j++) {
                                 if (esctable->Columns[j].PresetWidth) continue; // Columns with preset-widths are never auto-expanded
                                 if (esctable->Columns[j].MinWidth > cellwidth) continue;

                                 if (esctable->Columns[j].Width < cellwidth) {
                                    LAYOUT("layout_table_end","Expanding column %d from width %d to %.2f", j, esctable->Columns[j].Width, cellwidth);
                                    esctable->Columns[j].Width = cellwidth;
                                    //if (total - (DOUBLE)F2I(total) >= 0.5) esctable->Columns[j].Width++; // Fractional correction

                                    expanded = TRUE;
                                 }
                                 //total += cellwidth;
                              }

                              if (expanded) {
                                 LAYOUT("layout_table:","At least one cell was widened - will repass table layout.");
                                 RESTORE_STATE(&tablestate);
                                 LAYOUT_LOGRETURN();
                                 goto wrap_table_end;
                              }
                           }
                        }
                     }
                  }
                  else LAYOUT("layout_table_end:","Table is missing its columns array.");
               }
               else LAYOUT("layout_table_end:","Cells already widened - keeping table width of %d.", esctable->Width);

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
                     LAYOUT("layout_table:","Extending table height to %d (row %d+%d) due to a minimum height of %d at coord %d", minheight, lastrow->RowHeight, j, esctable->MinHeight, esctable->Y);
                     lastrow->RowHeight += j;
                     RESTORE_STATE(&rowstate);
                     LAYOUT_LOGRETURN();
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
                  LAYOUT("layout_table:","Table width (%d+%d) increases page width to %d, layout restart forced.", esctable->X, esctable->Width, j);
                  *Width = j;
                  LAYOUT_LOGRETURN();
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

               // Check if the table collides with clipping boundaries and
               // adjust its position accordingly.  Such a check is performed in
               // ESC_TABLE_START - this second check is required only if the width
               // of the table has been extended.
               //
               // Note that the total number of clips is adjusted so that only
               // clips up to the TABLE_START are considered (otherwise, clips
               // inside the table cells will cause collisions against the parent
               // table).


               LAYOUT("layout_table:","Checking table collisions (%dx%d).", esctable->X, esctable->Y);

               totalclips = Self->TotalClips;
               Self->TotalClips = esctable->TotalClips;
               j = check_wordwrap("Table", Self, i, &l, AbsX, Width, i, &esctable->X, &esctable->Y, esctable->Width, esctable->Height);
               Self->TotalClips = totalclips;

               if (j IS WRAP_EXTENDPAGE) {
                  LAYOUT("layout_table:","Table wrapped - expanding page width due to table size/position.");
                  LAYOUT_LOGRETURN();
                  goto extend_page;
               }
               else if (j IS WRAP_WRAPPED) {
                  // A repass is necessary as everything in the table will need to be rearranged
                  LAYOUT("layout_table:","Table wrapped - rearrangement necessary.");

                  RESTORE_STATE(&tablestate);
                  LAYOUT_LOGRETURN();
                  goto wrap_table_end;
               }

               //LAYOUT("layout_table:","new table pos: %dx%d", esctable->X, esctable->Y);

               // The table sets a clipping region in order to state its placement (the surrounds of a table are
               // effectively treated as a graphical object, since it's not text).

               clip.Left   = esctable->X;
               clip.Top    = esctable->Y;
               clip.Right  = clip.Left + esctable->Width;
               clip.Bottom = clip.Top + esctable->Height;

               //if (clip.Left IS l.left_margin) clip.Left = 0; // Extending the clipping to the left doesn't hurt

               add_clip(Self, &clip, i, "Table", FALSE);

               l.cursorx = esctable->X + esctable->Width;
               l.cursory = esctable->Y;

               LAYOUT("layout_table:","Final Table Size: %dx%d,%dx%d", esctable->X, esctable->Y, esctable->Width, esctable->Height);

               esctable = esctable->Stack;

               l.setsegment = TRUE;

               LAYOUT_LOGRETURN();
               break;
            }

            case ESC_ROW:
               if (escrow) {
                  auto ptr = escrow;
                  escrow = escape_data<escRow>(Self->Stream, i);
                  escrow->Stack = ptr;
               }
               else {
                  escrow = escape_data<escRow>(Self->Stream, i);
                  escrow->Stack = NULL;
               }

               SAVE_STATE(rowstate);

               if (esctable->ResetRowHeight) escrow->RowHeight = escrow->MinHeight;

repass_row_height_ext:
               escrow->VerticalRepass = FALSE;
               escrow->Y = l.cursory;
               esctable->RowWidth = (esctable->Thickness<<1) + esctable->CellHSpacing;

               l.setsegment = TRUE;
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
                  LAYOUT("layout_row:","Row ends, advancing down by %d+%d, new height: %d, y-cursor: %d",
                     escrow->RowHeight, esctable->CellVSpacing, esctable->Height, l.cursory);

               if (esctable->RowWidth > esctable->Width) esctable->Width = esctable->RowWidth;

               lastrow = escrow;
               escrow = escrow->Stack;
               l.setsegment = TRUE;
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


               BYTE savechar, vertical_repass;

               esccell = escape_data<escCell>(Self->Stream, i);

               if (!esctable) {
                  log.warning("escTable variable not defined for cell @ index %d - document byte code is corrupt.", i);
                  goto exit;
               }

               if (esccell->Column >= esctable->TotalColumns) {
                  LAYOUT("@layout_cell:","Cell %d exceeds total table column limit of %d.", esccell->Column, esctable->TotalColumns);
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
               //LAYOUT("Min:","%d / %d", escrow->MinHeight, escrow->RowHeight);

               LAYOUT("~layout_cell:","Index %d, Processing cell at %dx %dy, size %dx%d, column %d", i, l.cursorx, l.cursory, esccell->Width, esccell->Height, esccell->Column);

               // Find the matching CELL_END

               LONG cell_end = i;
               while (Self->Stream[cell_end]) {
                  if (Self->Stream[cell_end] IS CTRL_CODE) {
                     if (ESCAPE_CODE(Self->Stream, cell_end) IS ESC_CELL_END) {
                        escCellEnd *end = escape_data<escCellEnd>(Self->Stream, cell_end);
                        if (end->CellID IS esccell->CellID) break;
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
                  LONG segcount = Self->SegCount;
                  savechar = Self->Stream[cell_end];
                  Self->Stream[cell_end] = 0;

                  if (esccell->EditHash) Self->EditMode = TRUE;
                  else Self->EditMode = FALSE;

                     i = layout_section(Self, i, &l.font,
                            esccell->AbsX, esccell->AbsY,
                            &esccell->Width, &esccell->Height,
                            esctable->CellPadding, esctable->CellPadding, esctable->CellPadding, esctable->CellPadding, &vertical_repass);

                  if (esccell->EditHash) Self->EditMode = FALSE;

                  Self->Stream[cell_end] = savechar;

                  if (esccell->EditHash) {
                     // Edit cells have a minimum width/height so that the user can still interact with them when empty.

                     if (Self->SegCount IS segcount) {
                        // No content segments were created, which means that there's nothing for the cursor to attach
                        // itself too.

                        //do we really want to do something here?
                        //I'd suggest that we instead break up the segments a bit more???  ANother possibility - create an ESC_NULL
                        //type that gets placed at the start of the edit cell.  If there's no genuine content, then we at least have the ESC_NULL
                        //type for the cursor to be attached to?  ESC_NULL does absolutely nothing except act as faux content.


#warning Work on this next




                     }

                     if (esccell->Width < 16) esccell->Width = 16;
                     if (esccell->Height < l.font->LineSpacing) {
                        esccell->Height = l.font->LineSpacing;
                     }
                  }
               }

               LAYOUT_LOGRETURN();

               if (!i) goto exit;

               LAYOUT("layout_cell","Cell (%d:%d) is size %dx%d (min width %d)", esctable->RowIndex, esccell->Column, esccell->Width, esccell->Height, esctable->Columns[esccell->Column].Width);

               // Increase the overall width for the entire column if this cell has increased the column width.
               // This will affect the entire table, so a restart from TABLE_START is required.

               if (esctable->Columns[esccell->Column].Width < esccell->Width) {
                  LAYOUT("layout_cell","Increasing column width of cell (%d:%d) from %d to %d (table_start repass required).", esctable->RowIndex, esccell->Column, esctable->Columns[esccell->Column].Width, esccell->Width);
                  esctable->Columns[esccell->Column].Width = esccell->Width; // This has the effect of increasing the minimum column width for all cells in the column

                  // Percentage based and zero columns need to be recalculated.  The easiest thing to do
                  // would be for a complete recompute (ComputeColumns = TRUE) with the new minwidth.  The
                  // problem with ComputeColumns is that it does it all from scratch - we need to adjust it
                  // so that it can operate in a second style of mode where it recognises temporary width values.

                  esctable->Columns[esccell->Column].MinWidth = esccell->Width; // Column must be at least this size
                  esctable->ComputeColumns = 2;

                  esctable->ResetRowHeight = TRUE; // Row heights need to be reset due to the width increase
                  RESTORE_STATE(&tablestate);
                  LAYOUT_LOGRETURN(); // WHAT DOES THIS MATCH TO?
                  goto wrap_table_cell;
               }

               // Advance the width of the entire row and adjust the row height

               esctable->RowWidth += esctable->Columns[esccell->Column].Width;

               if (!esctable->Thin) esctable->RowWidth += esctable->CellHSpacing;
               else if ((esccell->Column + esccell->ColSpan) < esctable->TotalColumns-1) esctable->RowWidth += esctable->CellHSpacing;

               if ((esccell->Height > escrow->RowHeight) or (escrow->VerticalRepass)) {
                  // A repass will be required if the row height has increased
                  // and objects or tables have been used in earlier cells, because
                  // objects need to know the final dimensions of their table cell.

                  if (esccell->Column IS esctable->TotalColumns-1) {
                     LAYOUT("layout_cell","Extending row height from %d to %d (row repass required)", escrow->RowHeight, esccell->Height);
                  }

                  escrow->RowHeight = esccell->Height;
                  if ((esccell->Column + esccell->ColSpan) >= esctable->TotalColumns) {
                     RESTORE_STATE(&rowstate);
                     goto repass_row_height_ext;
                  }
                  else escrow->VerticalRepass = TRUE; // Make a note to do a vertical repass once all columns on this row have been processed
               }

               l.cursorx += esctable->Columns[esccell->Column].Width;

               if (!esctable->Thin) l.cursorx += esctable->CellHSpacing;
               else if ((esccell->Column + esccell->ColSpan) < esctable->TotalColumns) l.cursorx += esctable->CellHSpacing;

               if (esccell->Column IS 0) {
                  l.cursorx += esctable->Thickness;
               }

               break;
            }

            case ESC_CELL_END: {
               // CELL_END helps draw_document(), so set the segment to ensure that it is
               // included in the draw stream.  Please refer to ESC_CELL to see how content is
               // processed and how the cell dimensions are formed.

               l.setsegment = TRUE;

               if ((esccell) and (esccell->OnClick)) {
                  add_link(Self, ESC_CELL, esccell, esccell->AbsX, esccell->AbsY, esccell->Width, esccell->Height, "esc_cell_end");
               }

               if ((esccell) and (esccell->EditHash)) {
                  // The area of each edit cell is logged in an array, which is used for assisting interaction between
                  // the mouse pointer and the edit cells.

                  if (Self->ECIndex >= Self->ECMax) {
                     EditCell *cells;
                     if (!AllocMemory(sizeof(Self->EditCells[0]) * (Self->ECMax + 10), MEM_NO_CLEAR, &cells)) {
                        if (Self->EditCells) {
                           CopyMemory(Self->EditCells, cells, sizeof(Self->EditCells[0]) * Self->ECMax);
                           FreeResource(Self->EditCells);
                        }
                        Self->ECMax += 10;
                        Self->EditCells = cells;
                     }
                     else {
                        Self->Error = ERR_AllocMemory;
                        break;
                     }
                  }
                  Self->EditCells[Self->ECIndex].CellID = esccell->CellID;
                  Self->EditCells[Self->ECIndex].X      = esccell->AbsX;
                  Self->EditCells[Self->ECIndex].Y      = esccell->AbsY;
                  Self->EditCells[Self->ECIndex].Width  = esccell->Width;
                  Self->EditCells[Self->ECIndex].Height = esccell->Height;
                  Self->ECIndex++;
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
                  escpara = escape_data<escParagraph>(Self->Stream, i);
                  escpara->Stack = ptr;
               }
               else {
                  escpara = escape_data<escParagraph>(Self->Stream, i);
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
                     escpara->Relative = FALSE;

                     if (escpara->CustomString) {
                        LONG strwidth;
                        strwidth = fntStringWidth(l.font, (STRING)(escpara + 1), -1) + 10;
                        if (strwidth > esclist->ItemIndent) {
                           esclist->ItemIndent = strwidth;
                           escpara->ItemIndent = strwidth;
                           esclist->Repass     = TRUE;
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

            if ((l.link) and (l.link_open IS FALSE)) {
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
               if (tabwidth) l.cursorx += ROUNDUP(l.cursorx, tabwidth);
               i++;
            }
            else {
               l.cursorx += l.wordwidth + l.spacewidth;
               i++;
            }

            l.kernchar  = 0;
            l.wordwidth = 0;
            l.textcontent = TRUE;
         }
         else {
            LONG kerning;

            if (!l.wordwidth) l.wordindex = i;   // Record the index of the new word (if this is one)

            i += getutf8((CSTRING)Self->Stream+i, &unicode);
            l.wordwidth += fntCharWidth(l.font, unicode, l.kernchar, &kerning);
            l.wordwidth += kerning;
            l.kernchar = unicode;
            l.textcontent = TRUE;
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
      LAYOUT("layout_section:","============================================================");
      LAYOUT("layout_section:","SECOND PASS [%d]: Root page height increased from %d to %d", Offset, lastheight, page_height);
      goto extend_page;
   }

   *Font = l.font;
   if (page_height > *Height) *Height = page_height;

   *VerticalRepass = object_vertical_repass;

   Self->Depth--;
   LAYOUT_LOGRETURN();
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
   LONG last = Self->SegCount-1;
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

   for (LONG j=FirstClip; j < Self->TotalClips; j++) {
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
// Terminates all links and frees the memory.  Another method to clear the links
// is to set the TotalLinks to zero, which retains the link cache allocation.

static void free_links(extDocument *Self)
{
   if (!Self->Links) return;

   FreeResource(Self->Links);
   Self->Links = NULL;

   Self->MaxLinks = 0;
   Self->TotalLinks = 0;
}

//********************************************************************************************************************
// Record a clickable link, cell, or other form of clickable area.

static void add_link(extDocument *Self, UBYTE EscapeCode, APTR Escape, LONG X, LONG Y, LONG Width, LONG Height, CSTRING Caller)
{
   parasol::Log log(__FUNCTION__);

   if ((!Self) or (!Escape)) return;

   if ((Width < 1) or (Height < 1)) {
      log.traceWarning("Illegal width/height for link @ %dx%d, W/H %dx%d [%s]", X, Y, Width, Height, Caller);
      return;
   }

   if (!Self->Links) {
      Self->TotalLinks = 0;
      Self->MaxLinks = 20;
      if (!AllocMemory(sizeof(DocLink) * Self->MaxLinks, MEM_DATA|MEM_NO_CLEAR, &Self->Links)) {

      }
      else return;
   }
   else if (Self->TotalLinks+1 >= Self->MaxLinks) {
      #define LINK_INC 40
      if (!ReallocMemory(Self->Links, sizeof(DocLink) * (Self->MaxLinks + LINK_INC), &Self->Links, NULL)) {
         Self->MaxLinks += LINK_INC;
      }
      else return;
   }

   LAYOUT("add_link()","%dx%d,%dx%d, %s", X, Y, Width, Height, Caller);

   LONG index = Self->TotalLinks;
   Self->Links[index].EscapeCode = EscapeCode;
   Self->Links[index].Escape = Escape;
   Self->Links[index].X = X;
   Self->Links[index].Y = Y;
   Self->Links[index].Width = Width;
   Self->Links[index].Height = Height;
   Self->Links[index].Segment = Self->SegCount;
   Self->TotalLinks++;
}

//********************************************************************************************************************

static void draw_background(extDocument *Self, objSurface *Surface, objBitmap *Bitmap)
{
   gfxDrawRectangle(Bitmap, 0, 0, Surface->Width, Surface->Height, Bitmap->packPixel(Self->Background), BAF_FILL);
}

//********************************************************************************************************************
// Note that this function also controls the drawing of objects that have loaded into the document (see the
// subscription hook in the layout process).

static void draw_document(extDocument *Self, objSurface *Surface, objBitmap *Bitmap)
{
   parasol::Log log(__FUNCTION__);
   escList *esclist;
   escLink *esclink;
   escParagraph *escpara;
   escTable *esctable;
   escCell *esccell;
   escRow *escrow;
   escObject *escobject;
   DocSegment *segment;
   RGB8 link_save_rgb;
   UBYTE tabfocus, oob, cursor_drawn;
   LONG fx, si, i;

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

   #ifdef DEBUG
   if ((!Self->Stream[0]) or (!Self->SegCount)) {
      //log.traceWarning("No content in stream or no segments.");
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
   tabfocus = FALSE;
   cursor_drawn = FALSE;

   #ifdef GUIDELINES

      // Page boundary is marked in blue
      gfxDrawRectangle(Bitmap, Self->LeftMargin-1, Self->TopMargin-1,
         Self->CalcWidth - Self->RightMargin - Self->LeftMargin + 2, Self->PageHeight - Self->TopMargin - Self->BottomMargin + 2,
         Bitmap->packPixel(0, 0, 255), 0);

      // Special clip regions are marked in grey
      for (i=0; i < Self->TotalClips; i++) {
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

   UBYTE alpha = Bitmap->Opacity;
   for (LONG seg=0; seg < Self->SegCount; seg++) {
      segment = Self->Segments + seg;

      // Don't process segments that are out of bounds.  This can't be applied to objects, as they can draw anywhere.

      oob = FALSE;
      if (!segment->ObjectContent) {
         if (segment->Y >= Bitmap->Clip.Bottom) oob = TRUE;
         if (segment->Y + segment->Height < Bitmap->Clip.Top) oob = TRUE;
         if (segment->X + segment->Width < Bitmap->Clip.Left) oob = TRUE;
         if (segment->X >= Bitmap->Clip.Right) oob = TRUE;
      }

      // Highlighting of selected text

      if ((select_start <= segment->Stop) and (select_end > segment->Index)) {
         if (select_start != select_end) {
            Bitmap->Opacity = 80;
            if ((select_start > segment->Index) and (select_start < segment->Stop)) {
               if (select_end < segment->Stop) {
                  gfxDrawRectangle(Bitmap, segment->X + select_startx, segment->Y,
                     select_endx - select_startx, segment->Height, Bitmap->packPixel(0, 128, 0), BAF_FILL);
               }
               else {
                  gfxDrawRectangle(Bitmap, segment->X + select_startx, segment->Y,
                     segment->Width - select_startx, segment->Height, Bitmap->packPixel(0, 128, 0), BAF_FILL);
               }
            }
            else if (select_end < segment->Stop) {
               gfxDrawRectangle(Bitmap, segment->X, segment->Y, select_endx, segment->Height,
                  Bitmap->packPixel(0, 128, 0), BAF_FILL);
            }
            else {
               gfxDrawRectangle(Bitmap, segment->X, segment->Y, segment->Width, segment->Height,
                  Bitmap->packPixel(0, 128, 0), BAF_FILL);
            }
            Bitmap->Opacity = 255;
         }
      }

      if ((Self->ActiveEditDef) and (Self->CursorState) and (!cursor_drawn)) {
         if ((Self->CursorIndex >= segment->Index) and (Self->CursorIndex <= segment->Stop)) {
            if ((Self->CursorIndex IS segment->Stop) and (Self->Stream[Self->CursorIndex-1] IS '\n')); // The -1 looks naughty, but it works as CTRL_CODE != \n, so use of PREV_CHAR() is unnecessary
            else {
               if (gfxGetUserFocus() IS Self->PageID) { // Standard text cursor
                  gfxDrawRectangle(Bitmap, segment->X + Self->CursorCharX, segment->Y, 2, segment->BaseLine,
                     Bitmap->packPixel(255, 0, 0), BAF_FILL);
                  cursor_drawn = TRUE;
               }
            }
         }
      }

      #ifdef GUIDELINES_CONTENT
         if (segment->TextContent) {
            gfxDrawRectangle(Bitmap,
               segment->X, segment->Y,
               (segment->Width > 0) ? segment->Width : 5, segment->Height,
               Bitmap->packPixel(0, 255, 0), 0);
         }
      #endif

      char strbuffer[segment->Stop - segment->Index + 1];

      fx = segment->X;
      i  = segment->Index;
      si = 0;

      while (i < segment->TrimStop) {
         if (Self->Stream[i] IS CTRL_CODE) {
            switch (ESCAPE_CODE(Self->Stream, i)) {
               case ESC_OBJECT: {
                  OBJECTPTR object;

                  escobject = escape_data<escObject>(Self->Stream, i);

                  if ((escobject->Graphical) and (!escobject->Owned)) {
                     if (escobject->ObjectID < 0) {
                        object = NULL;
                        AccessObjectID(escobject->ObjectID, 3000, &object);
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
                  auto style = escape_data<escFont>(Self->Stream, i);
                  if ((font = lookup_font(style->Index, "draw_document"))) {
                     font->Bitmap = Bitmap;
                     if (tabfocus IS FALSE) font->Colour = style->Colour;
                     else font->Colour = Self->SelectColour;

                     if (style->Options & FSO_ALIGN_RIGHT) font->Align = ALIGN_RIGHT;
                     else if (style->Options & FSO_ALIGN_CENTER) font->Align = ALIGN_HORIZONTAL;
                     else font->Align = 0;

                     if (style->Options & FSO_UNDERLINE) font->Underline = font->Colour;
                     else font->Underline.Alpha = 0;
                  }
                  break;
               }

               case ESC_LIST_START:
                  if (esclist) {
                     auto ptr = esclist;
                     esclist = escape_data<escList>(Self->Stream, i);
                     esclist->Stack = ptr;
                  }
                  else esclist = escape_data<escList>(Self->Stream, i);
                  break;

               case ESC_LIST_END:
                  if (esclist) esclist = esclist->Stack;
                  break;

               case ESC_PARAGRAPH_START:
                  if (escpara) {
                     auto ptr = escpara;
                     escpara = escape_data<escParagraph>(Self->Stream, i);
                     escpara->Stack = ptr;
                  }
                  else escpara = escape_data<escParagraph>(Self->Stream, i);

                  if ((esclist) and (escpara->ListItem)) {
                     // Handling for paragraphs that form part of a list

                     if ((esclist->Type IS LT_CUSTOM) or (esclist->Type IS LT_ORDERED)) {
                        if (escpara->CustomString) {
                           font->X = fx - escpara->ItemIndent;
                           font->Y = segment->Y + font->Leading + (segment->BaseLine - font->Ascent);
                           font->AlignWidth = segment->AlignWidth;
                           font->set(FID_String, (STRING)(escpara + 1));
                           font->draw();
                        }
                     }
                     else if (esclist->Type IS LT_BULLET) {
                        #define SIZE_BULLET 5
                        // TODO: Requires conversion to vector
                        //gfxDrawEllipse(Bitmap,
                        //   fx - escpara->ItemIndent, segment->Y + ((segment->BaseLine - SIZE_BULLET)/2),
                        //   SIZE_BULLET, SIZE_BULLET, Bitmap->packPixel(esclist->Colour), TRUE);
                     }
                  }
                  break;

               case ESC_PARAGRAPH_END:
                  if (escpara) escpara = escpara->Stack;
                  break;

               case ESC_TABLE_START: {
                  if (esctable) {
                     auto ptr = esctable;
                     esctable = escape_data<escTable>(Self->Stream, i);
                     esctable->Stack = ptr;
                  }
                  else esctable = escape_data<escTable>(Self->Stream, i);

                  //log.trace("Draw Table: %dx%d,%dx%d", esctable->X, esctable->Y, esctable->Width, esctable->Height);

                  if (esctable->Colour.Alpha > 0) {
                     gfxDrawRectangle(Bitmap,
                        esctable->X+esctable->Thickness, esctable->Y+esctable->Thickness,
                        esctable->Width-(esctable->Thickness<<1), esctable->Height-(esctable->Thickness<<1),
                        Bitmap->packPixel(esctable->Colour), BAF_FILL|BAF_BLEND);
                  }

                  if (esctable->Shadow.Alpha > 0) {
                     Bitmap->Opacity = esctable->Shadow.Alpha;
                     for (LONG j=0; j < esctable->Thickness; j++) {
                        gfxDrawRectangle(Bitmap,
                           esctable->X+j, esctable->Y+j,
                           esctable->Width-(j<<1), esctable->Height-(j<<1),
                           Bitmap->packPixel(esctable->Shadow), 0);
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
                     escrow = escape_data<escRow>(Self->Stream, i);
                     escrow->Stack = ptr;
                  }
                  else escrow = escape_data<escRow>(Self->Stream, i);

                  if (escrow->Colour.Alpha) {
                     gfxDrawRectangle(Bitmap, esctable->X, escrow->Y, esctable->Width, escrow->RowHeight,
                        Bitmap->packPixel(escrow->Colour), BAF_FILL|BAF_BLEND);
                  }
                  break;
               }

               case ESC_ROW_END:
                  if (escrow) escrow = escrow->Stack;
                  break;

               case ESC_CELL: {
                  if (esccell) {
                     auto ptr = esccell;
                     esccell = escape_data<escCell>(Self->Stream, i);
                     esccell->Stack = ptr;
                  }
                  else esccell = escape_data<escCell>(Self->Stream, i);

                  Self->CurrentCell = esccell;

                  if (esccell->Colour.Alpha > 0) { // Fill colour
                     WORD border;
                     if (esccell->Shadow.Alpha > 0) border = 1;
                     else border = 0;

                     gfxDrawRectangle(Bitmap, esccell->AbsX+border, esccell->AbsY+border,
                        esctable->Columns[esccell->Column].Width-border, escrow->RowHeight-border,
                        Bitmap->packPixel(esccell->Colour), BAF_FILL|BAF_BLEND);
                  }

                  if (esccell->Shadow.Alpha > 0) { // Border colour
                     gfxDrawRectangle(Bitmap, esccell->AbsX, esccell->AbsY, esctable->Columns[esccell->Column].Width,
                        escrow->RowHeight, Bitmap->packPixel(esccell->Shadow), 0);
                  }
                  break;
               }

               case ESC_CELL_END:
                  if (esccell) esccell = esccell->Stack;
                  Self->CurrentCell = esccell;
                  break;

               case ESC_LINK: {
                  esclink = escape_data<escLink>(Self->Stream, i);
                  if (Self->HasFocus) {
                     if ((Self->Tabs[Self->FocusIndex].Type IS TT_LINK) and (Self->Tabs[Self->FocusIndex].Ref IS esclink->ID) and (Self->Tabs[Self->FocusIndex].Active)) {
                        link_save_rgb = font->Colour;
                        font->Colour = Self->SelectColour;
                        tabfocus = TRUE;
                     }
                  }

                  break;
               }

               case ESC_LINK_END:
                  if (tabfocus) {
                     font->Colour = link_save_rgb;
                     tabfocus = FALSE;
                  }
                  break;
            }

            i += ESCAPE_LEN(Self->Stream+i);
         }
         else if (!oob) {
            if (Self->Stream[i] <= 0x20) { strbuffer[si++] = ' '; i++; }
            else strbuffer[si++] = Self->Stream[i++];

            // Print the string buffer content if the next string character is an escape code.

            if (Self->Stream[i] IS CTRL_CODE) {
               strbuffer[si] = 0;
               font->X = fx;
               font->Y = segment->Y + font->Leading + (segment->BaseLine - font->Ascent);
               font->AlignWidth = segment->AlignWidth;
               font->set(FID_String, strbuffer);
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
         font->Y = segment->Y + font->Leading + (segment->BaseLine - font->Ascent);
         font->AlignWidth = segment->AlignWidth;
         font->set(FID_String, strbuffer);
         font->draw();
         fx = font->EndX;
      }
   } // for loop
}

//********************************************************************************************************************

static void draw_border(extDocument *Self, objSurface *Surface, objBitmap *Bitmap)
{
   if ((!Self->BorderEdge) or (Self->BorderEdge IS (DBE_TOP|DBE_BOTTOM|DBE_LEFT|DBE_RIGHT))) {
      gfxDrawRectangle(Bitmap, 0, 0, Surface->Width, Surface->Height, Bitmap->packPixel(Self->Border), 0);
   }
   else {
      if (Self->BorderEdge & DBE_TOP) {
         gfxDrawRectangle(Bitmap, 0, 0, Surface->Width, 1, Bitmap->packPixel(Self->Border), 0);
      }
      if (Self->BorderEdge & DBE_LEFT) {
         gfxDrawRectangle(Bitmap, 0, 0, 1, Surface->Height, Bitmap->packPixel(Self->Border), 0);
      }
      if (Self->BorderEdge & DBE_RIGHT) {
         gfxDrawRectangle(Bitmap, Surface->Width-1, 0, 1, Surface->Height, Bitmap->packPixel(Self->Border), 0);
      }
      if (Self->BorderEdge & DBE_BOTTOM) {
         gfxDrawRectangle(Bitmap, 0, Surface->Height-1, Surface->Width, 1, Bitmap->packPixel(Self->Border), 0);
      }
   }
}

//********************************************************************************************************************

static LONG xml_content_len(XMLTag *Tag)
{
   LONG len;

   if (!Tag->Attrib->Name) {
      for (len=0; Tag->Attrib->Value[len]; len++);
   }
   else if ((Tag = Tag->Child)) {
      len = 0;
      while (Tag) {
         len += xml_content_len(Tag);
         Tag = Tag->Next;
      }
   }
   else return 0;

   return len;
}

//********************************************************************************************************************

static void xml_extract_content(XMLTag *Tag, char *Buffer, LONG *Index, BYTE Whitespace)
{
   if (!Tag->Attrib->Name) {
      CSTRING content;
      LONG pos = *Index;
      if ((content = Tag->Attrib->Value)) {
         if (Whitespace) {
            for (LONG i=0; content[i]; i++) Buffer[pos++] = content[i];
         }
         else for (LONG i=0; content[i]; i++) { // Skip whitespace
            if (content[i] <= 0x20) while ((content[i+1]) and (content[i+1] <= 0x20)) i++;
            Buffer[pos++] = content[i];
         }
      }
      Buffer[pos] = 0;
      *Index = pos;
   }
   else if ((Tag = Tag->Child)) {
      while (Tag) {
         xml_extract_content(Tag, Buffer, Index, Whitespace);
         Tag = Tag->Next;
      }
   }
}

//********************************************************************************************************************

static ERROR keypress(extDocument *Self, LONG Flags, LONG Value, LONG Unicode)
{
   parasol::Log log(__FUNCTION__);
   struct acScroll scroll;

   log.function("Value: %d, Flags: $%.8x, ActiveEdit: %p", Value, Flags, Self->ActiveEditDef);

   if ((Self->ActiveEditDef) and (gfxGetUserFocus() != Self->PageID)) {
      deactivate_edit(Self, TRUE);
   }

   if (Self->ActiveEditDef) {
      UBYTE *stream;

      reset_cursor(Self);

      if (!(stream = Self->Stream)) return ERR_ObjectCorrupt;

      if (Unicode) {
         // Delete any text that is selected

         if ((Self->SelectIndex != -1) and (Self->SelectIndex != Self->CursorIndex)) {
            LONG start, end;
            if (Self->SelectIndex < Self->CursorIndex) { start = Self->SelectIndex; end = Self->CursorIndex; }
            else { start = Self->CursorIndex; end = Self->SelectIndex; }

            CopyMemory(stream + end, stream + start, Self->StreamLen + 1 - end);
            Self->StreamLen -= end - start;

            Self->SelectIndex = -1;
            Self->CursorIndex = start;
         }

         // Output the character

         char string[12];
         UTF8WriteValue(Unicode, string, sizeof(string));
         docInsertText(Self, string, Self->CursorIndex, TRUE); // Will set UpdateLayout to TRUE
         Self->CursorIndex += StrLength(string); // Reposition the cursor

         layout_doc_fast(Self);

         resolve_fontx_by_index(Self, Self->CursorIndex, &Self->CursorCharX);

         DRAW_PAGE(Self);
         return ERR_Okay;
      }

      switch(Value) {
         case K_TAB: {
            log.branch("Key: Tab");
            if (Self->TabFocusID) acFocus(Self->TabFocusID);
            else {
               if (Flags & KQ_SHIFT) advance_tabfocus(Self, -1);
               else advance_tabfocus(Self, 1);
            }
            break;
         }

         case K_ENTER: {
            // Delete any text that is selected

            if ((Self->SelectIndex != -1) and (Self->SelectIndex != Self->CursorIndex)) {
               LONG start, end;
               if (Self->SelectIndex < Self->CursorIndex) { start = Self->SelectIndex; end = Self->CursorIndex; }
               else { start = Self->CursorIndex; end = Self->SelectIndex; }

               CopyMemory(stream + end, stream + start, Self->StreamLen + 1 - end);
               Self->StreamLen -= end - start;

               Self->SelectIndex = -1;
               Self->CursorIndex = start;
            }

            docInsertXML(Self, "<br/>", Self->CursorIndex);
            NEXT_CHAR(stream, Self->CursorIndex);

            layout_doc_fast(Self);
            resolve_fontx_by_index(Self, Self->CursorIndex, &Self->CursorCharX);
            DRAW_PAGE(Self);
            break;
         }

         case K_LEFT: {
            Self->SelectIndex = -1;

            LONG index = Self->CursorIndex;
            if ((stream[index] IS CTRL_CODE) and (ESCAPE_CODE(stream, index) IS ESC_CELL)) {
               // Cursor cannot be moved any further left.  The cursor index should never end up here, but
               // better to be safe than sorry.

            }
            else {
               while (index > 0) {
                  PREV_CHAR(stream, index);
                  if (stream[index] IS CTRL_CODE) {
                     if (ESCAPE_CODE(stream, index) IS ESC_CELL) {
                        auto cell = escape_data<escCell>(stream, index);
                        if (cell->CellID IS Self->ActiveEditCellID) break;
                     }
                     else if (ESCAPE_CODE(stream, index) IS ESC_OBJECT);
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

         case K_RIGHT: {
            LONG code;

            Self->SelectIndex = -1;

            LONG index = Self->CursorIndex;
            while (stream[index]) {
               if (stream[index] IS CTRL_CODE) {
                  code = ESCAPE_CODE(stream, index);
                  if (code IS ESC_CELL_END) {
                     auto cell_end = escape_data<escCellEnd>(stream, index);
                     if (cell_end->CellID IS Self->ActiveEditCellID) {
                        // End of editing zone - cursor cannot be moved any further right
                        break;
                     }
                  }
                  else if (code IS ESC_OBJECT); // Objects are treated as content, so do nothing special for these and drop through to next section
                  else {
                     NEXT_CHAR(stream, index);
                     continue;
                  }
               }

               // The current index references a content character or object.  Advance the cursor to the next index.

               NEXT_CHAR(stream, index);
               if (!resolve_fontx_by_index(Self, index, &Self->CursorCharX)) {
                  Self->CursorIndex = index;
                  DRAW_PAGE(Self);
                  log.warning("RightCursor: %d, X: %d", Self->CursorIndex, Self->CursorCharX);
               }
               break;
            }
            break;
         }

         case K_HOME: {
            break;
         }

         case K_END: {
            break;
         }

         case K_UP:
            break;

         case K_DOWN:
            break;

         case K_BACKSPACE: {
            LONG index = Self->CursorIndex;
            if ((stream[index] IS CTRL_CODE) and (ESCAPE_CODE(stream, index) IS ESC_CELL)) {
               // Cursor cannot be moved any further left
            }
            else {
               PREV_CHAR(stream, index);

               if ((stream[index] IS CTRL_CODE) and (ESCAPE_CODE(stream, index) IS ESC_CELL));
               else {
                  // Delete the character/escape code

                  if ((Self->SelectIndex != -1) and (Self->SelectIndex != Self->CursorIndex)) {
                     LONG start, end;
                     if (Self->SelectIndex < Self->CursorIndex) { start = Self->SelectIndex; end = Self->CursorIndex; }
                     else { start = index; end = Self->SelectIndex; }

                     CopyMemory(stream + end, stream + start, Self->StreamLen + 1 - end);
                     Self->StreamLen -= end - start;
                     Self->CursorIndex = start;
                  }
                  else {
                     CopyMemory(stream + Self->CursorIndex, stream + index, Self->StreamLen + 1 - Self->CursorIndex);
                     Self->StreamLen -= (Self->CursorIndex - index);
                     Self->CursorIndex = index;
                  }

                  Self->SelectIndex = -1;

                  Self->UpdateLayout = TRUE;
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

         case K_DELETE: {
            LONG end;

            LONG index = Self->CursorIndex;
            if ((stream[index] IS CTRL_CODE) and (ESCAPE_CODE(stream, index) IS ESC_CELL_END)) {
               // Not allowed to delete the end point
            }
            else {
               if ((Self->SelectIndex != -1) and (Self->SelectIndex != Self->CursorIndex)) {
                  LONG start;
                  if (Self->SelectIndex < Self->CursorIndex) { start = Self->SelectIndex; end = Self->CursorIndex; }
                  else { start = Self->CursorIndex; end = Self->SelectIndex; }

                  CopyMemory(stream + end, stream + start, Self->StreamLen + 1 - end);
                  Self->StreamLen -= end - start;

                  if (Self->CursorIndex > Self->SelectIndex) {
                     Self->CursorIndex = Self->SelectIndex;
                  }

                  Self->SelectIndex = -1;

               }
               else {
                  end = index;
                  NEXT_CHAR(stream, end);

                  CopyMemory(stream + end, stream + Self->CursorIndex, Self->StreamLen + 1 - end);
                  Self->StreamLen -= (end - Self->CursorIndex);

               }

               Self->UpdateLayout = TRUE;
               layout_doc_fast(Self);
               resolve_fontx_by_index(Self, Self->CursorIndex, &Self->CursorCharX);
               DRAW_PAGE(Self);

               #ifdef DBG_STREAM
                  print_stream(Self, stream);
               #endif
            }

            break;
         }
      }
   }
   else switch (Value) {
      // NB: When not in edit mode, only the navigation keys are enabled
      case K_TAB:
         log.branch("Key: Tab");
         if (Self->TabFocusID) acFocus(Self->TabFocusID);
         else if (Flags & KQ_SHIFT) advance_tabfocus(Self, -1);
         else advance_tabfocus(Self, 1);
         break;

      case K_ENTER: {
         LONG j;
         LONG tab = Self->FocusIndex;
         if ((tab >= 0) and (tab < Self->TabIndex)) {
            log.branch("Key: Enter, Tab: %d/%d, Type: %d", tab, Self->TabIndex, Self->Tabs[tab].Type);

            if ((Self->Tabs[tab].Type IS TT_LINK) and (Self->Tabs[tab].Active)) {
               for (j=0; j < Self->TotalLinks; j++) {
                  if ((Self->Links[j].EscapeCode IS ESC_LINK) and (Self->Links[j].Link->ID IS Self->Tabs[tab].Ref)) {
                     exec_link(Self, j);
                     break;
                  }
               }
            }
         }
         break;
      }

      case K_PAGE_DOWN:
         scroll.DeltaX = 0;
         scroll.DeltaY = Self->AreaHeight;
         scroll.DeltaZ = 0;
         QueueAction(AC_Scroll, Self->SurfaceID, &scroll);
         break;

      case K_PAGE_UP:
         scroll.DeltaX = 0;
         scroll.DeltaY = -Self->AreaHeight;
         scroll.DeltaZ = 0;
         QueueAction(AC_Scroll, Self->SurfaceID, &scroll);
         break;

      case K_LEFT:
         scroll.DeltaX = -10;
         scroll.DeltaY = 0;
         scroll.DeltaZ = 0;
         QueueAction(AC_Scroll, Self->SurfaceID, &scroll);
         break;

      case K_RIGHT:
         scroll.DeltaX = 10;
         scroll.DeltaY = 0;
         scroll.DeltaZ = 0;
         QueueAction(AC_Scroll, Self->SurfaceID, &scroll);
         break;

      case K_DOWN:
         scroll.DeltaX = 0;
         scroll.DeltaY = 10;
         scroll.DeltaZ = 0;
         QueueAction(AC_Scroll, Self->SurfaceID, &scroll);
         break;

      case K_UP:
         scroll.DeltaX = 0;
         scroll.DeltaY = -10;
         scroll.DeltaZ = 0;
         QueueAction(AC_Scroll, Self->SurfaceID, &scroll);
         break;
   }

   return ERR_Okay;
}

//********************************************************************************************************************

static ERROR load_doc(extDocument *Self, CSTRING Path, BYTE Unload, BYTE UnloadFlags)
{
   parasol::Log log(__FUNCTION__);

   log.branch("Loading file '%s', page '%s'", Path, Self->PageName);

   if (Unload) unload_doc(Self, UnloadFlags);

   process_parameters(Self, Path);

   // Generate a path without parameter values.

   LONG i;
   for (i=0; Path[i]; i++) {
      if ((Path[i] IS '&') or (Path[i] IS '#') or (Path[i] IS '?')) break;
   }
   char path[i+1];
   CopyMemory(Path, path, i);
   path[i] = 0;

   if (!AnalysePath(path, NULL)) {
      OBJECTPTR task;
      if ((task = CurrentTask())) task->set(FID_Path, path);

      if (auto xml = objXML::create::integral(
         fl::Flags(XMF_ALL_CONTENT|XMF_PARSE_HTML|XMF_STRIP_HEADERS|XMF_WELL_FORMED),
         fl::Path(path), fl::ReadOnly(TRUE))) {

         if (Self->XML) acFree(Self->XML);
         Self->XML = xml;

         AdjustLogLevel(3);
         Self->Error = process_page(Self, xml);
         AdjustLogLevel(-3);

         return Self->Error;
      }
      else {
         char msg[300];
         snprintf(msg, sizeof(msg), "Failed to load document file '%s'", path);
         error_dialog("Document Load Error", msg, Self->Error);
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
   parasol::Log log(__FUNCTION__);
   objFont *font;
   LONG pagewidth, pageheight, hscroll_offset;
   BYTE vertical_repass;

   if (Self->UpdateLayout IS FALSE) return;
   if ((!Self->Stream) or (!Self->Stream[0])) return;

   // Initial height is 1, not the surface height because we want to accurately report the final height of the page.

   pageheight = 1;

   LAYOUT("~layout_doc()","Area: %dx%d,%dx%d Visible: %d ----------",
      Self->AreaX, Self->AreaY, Self->AreaWidth, Self->AreaHeight, Self->VScrollVisible);

   Self->BreakLoop = MAXLOOP;

restart:
   Self->BreakLoop--;

   hscroll_offset = 0;

   if (Self->PageWidth <= 0) {
      // If no preferred page width is set, maximise the page width to the available viewing area
      pagewidth = Self->AreaWidth - hscroll_offset;
   }
   else {
      if (!Self->RelPageWidth) {
         // Page width is fixed
         pagewidth = Self->PageWidth;
      }
      else {
         // Page width is relative
         pagewidth = (Self->PageWidth * (Self->AreaWidth - hscroll_offset)) / 100;
      }
   }

   if (pagewidth < Self->MinPageWidth) pagewidth = Self->MinPageWidth;

   Self->SegCount = 0;
   Self->SortCount = 0;
   Self->TotalClips = 0;
   Self->TotalLinks = 0;
   Self->ECIndex = 0;
   Self->PageProcessed = FALSE;
   Self->Error = ERR_Okay;
   Self->Depth = 0;

   if (!(font = lookup_font(0, "layout_doc"))) { // There is no content loaded for display
      LAYOUT_LOGRETURN();
      return;
   }

   layout_section(Self, 0, &font, 0, 0, &pagewidth, &pageheight, Self->LeftMargin, Self->TopMargin, Self->RightMargin,
      Self->BottomMargin, &vertical_repass);

   LAYOUT("layout_doc:","Section layout complete.");

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
            LAYOUT("layout_doc","Vertical scrollbar visibility needs to be enabled, restarting...");
            Self->VScrollVisible = TRUE;
            Self->BreakLoop = MAXLOOP;
            goto restart;
         }
      }
      else {
         // Page height is smaller than the surface, so the scrollbar needs to be invisible.

         if (Self->VScrollVisible) {
            LAYOUT("layout_doc","Vertical scrollbar needs to be invisible, restarting...");
            Self->VScrollVisible = FALSE;
            Self->BreakLoop = MAXLOOP;
            goto restart;
         }
      }
   }

   // Look for clickable links that need to be aligned and adjust them (links cannot be aligned until the entire
   // width of their line is known, hence it's easier to make a final adjustment for all links post-layout).

   if (!Self->Error) {
      DocLink *link;
      DocSegment *segment;
      escLink *esclink;
      LONG i;

      for (i=0; i < Self->TotalLinks; i++) {
         if (Self->Links[i].EscapeCode != ESC_LINK) continue;

         link = &Self->Links[i];
         esclink = link->Link;
         if (esclink->Align & (FSO_ALIGN_RIGHT|FSO_ALIGN_CENTER)) {
            segment = &Self->Segments[link->Segment];
            if (esclink->Align & FSO_ALIGN_RIGHT) {
               link->X = segment->X + segment->AlignWidth - link->Width;
            }
            else if (esclink->Align & FSO_ALIGN_CENTER) {
               link->X = link->X + ((segment->AlignWidth - link->Width) / 2);
            }
         }
      }
   }

   // Build the sorted segment array

   if (!Self->Error) {
      if (Self->SortSegments) {
         FreeResource(Self->SortSegments);
         Self->SortSegments = NULL;
      }

      if (Self->SegCount) {
         if (!AllocMemory(sizeof(Self->SortSegments[0]) * Self->SegCount, MEM_NO_CLEAR, &Self->SortSegments)) {
            LONG seg, i, j;

            for (i=0, seg=0; seg < Self->SegCount; seg++) {
               if ((Self->Segments[seg].Height > 0) and (Self->Segments[seg].Width > 0)) {
                  Self->SortSegments[i].Segment = seg;
                  Self->SortSegments[i].Y       = Self->Segments[seg].Y;
                  i++;
               }
            }
            Self->SortCount = i;

            // Shell sort

            LONG h = 1;
            while (h < Self->SortCount / 9) h = 3 * h + 1;

            for (; h > 0; h /= 3) {
               for (i=h; i < Self->SortCount; i++) {
                  SortSegment temp = Self->SortSegments[i];
                  for (j=i; (j >= h) and (sortseg_compare(Self, Self->SortSegments + j - h, &temp) < 0); j -= h) {
                     Self->SortSegments[j] = Self->SortSegments[j - h];
                  }
                  Self->SortSegments[j] = temp;
               }
            }
         }
         else Self->Error = ERR_AllocMemory;
      }
   }

   Self->UpdateLayout = FALSE;

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

      CSTRING errstr;
      if (Self->Error IS ERR_Loop) errstr = "This page cannot be rendered correctly due to its design.";
      else errstr = GetErrorMsg(Self->Error);

      char msg[200];
      LONG i = StrCopy("A failure occurred during the layout of this document - it cannot be displayed.\n\nDetails: ", msg, sizeof(msg));
      StrCopy(errstr, msg+i, sizeof(msg)-i);

      error_dialog("Document Layout Error", msg, 0);
   }
   else {
      for (auto trigger=Self->Triggers[DRT_AFTER_LAYOUT]; trigger; trigger=trigger->Next) {
         if (trigger->Function.Type IS CALL_SCRIPT) {
            OBJECTPTR script;
            if ((script = trigger->Function.Script.Script)) {
               const ScriptArg args[] = {
                  { "ViewWidth",  FD_LONG, { .Long = Self->AreaWidth } },
                  { "ViewHeight", FD_LONG, { .Long = Self->AreaHeight } },
                  { "PageWidth",  FD_LONG, { .Long = Self->CalcWidth } },
                  { "PageHeight", FD_LONG, { .Long = Self->PageHeight } }
               };
               scCallback(script, trigger->Function.Script.ProcedureID, args, ARRAYSIZE(args), NULL);
            }
         }
         else if (trigger->Function.Type IS CALL_STDC) {
            auto routine = (void (*)(APTR, extDocument *, LONG, LONG, LONG, LONG))trigger->Function.StdC.Routine;
            if (routine) {
               parasol::SwitchContext context(trigger->Function.StdC.Context);
               routine(trigger->Function.StdC.Context, Self, Self->AreaWidth, Self->AreaHeight, Self->CalcWidth, Self->PageHeight);
            }
         }
      }
   }

   LAYOUT_LOGRETURN();
}

//********************************************************************************************************************
// Converts XML into RIPPLE bytecode, then displays the page that is referenced by the PageName field by calling
// layout_doc().  If the PageName is unspecified, we use the first <page> that has no name, otherwise the first page
// irrespective of the name.
//
// This function does not clear existing data, so you can use it to append new content to existing document content.

static ERROR process_page(extDocument *Self, objXML *xml)
{
   DOUBLE x, y;

   if (!xml) return ERR_NoData;

   parasol::Log log(__FUNCTION__);

   log.branch("Page: %s, XML: %d, Tags: %d", Self->PageName, xml->UID, xml->TagCount);

   // Look for the first page that matches the requested page name (if a name is specified).  Pages can be located anywhere
   // within the XML source - they don't have to be at the root.

   XMLTag **firstpage = NULL;
   XMLTag **page;
   if ((page = xml->Tags)) {
      while (*page) {
         if (!StrMatch("page", page[0]->Attrib->Name)) {
            if (!firstpage) firstpage = page;
            CSTRING name = XMLATTRIB(page[0], "name");
            if (!name) {
               if (!Self->PageName) break;
            }
            else if (!StrMatch(Self->PageName, name)) break;
         }
         page++;
      }

      if ((!Self->PageName) and (firstpage)) page = firstpage;
   }

   Self->Error = ERR_Okay;
   if ((page) and (*page) and (page[0]->Child)) {
      Self->PageTag = *page;

      UBYTE noheader = FALSE;
      UBYTE nofooter = FALSE;
      if (XMLATTRIB(page[0], "noheader")) noheader = TRUE;
      if (XMLATTRIB(page[0], "nofooter")) nofooter = TRUE;

      if (Self->Segments) ClearMemory(Self->Segments, sizeof(Self->Segments[0]) * Self->MaxSegments);

      if (!Self->Buffer) {
         Self->BufferSize = 65536;
         if (AllocMemory(Self->BufferSize, MEM_NO_CLEAR, &Self->Buffer)) {
            return ERR_AllocMemory;
         }
      }

      if (!Self->Temp) {
         Self->TempSize = 65536;
         if (AllocMemory(Self->TempSize, MEM_NO_CLEAR, &Self->Temp)) {
            return ERR_AllocMemory;
         }
      }

      if (!Self->VArg) {
         if (AllocMemory(sizeof(Self->VArg[0]) * MAX_ARGS, MEM_NO_CLEAR, &Self->VArg)) {
            return ERR_AllocMemory;
         }
      }

      Self->SegCount     = 0;
      Self->SortCount    = 0;
      Self->XPosition    = 0;
      Self->YPosition    = 0;
      Self->ClickHeld    = FALSE;
      Self->SelectStart  = 0;
      Self->SelectEnd    = 0;
      Self->UpdateLayout = TRUE;
      Self->ArgIndex     = 0;
      Self->ArgNestIndex = 0;
      Self->BufferIndex  = 0;
      Self->Error        = ERR_Okay;

      //drwForbidDrawing(); // We do this to prevent objects from posting draw messages on their creation

      // Process tags at the root level, but only those that we allow up to the first <page> entry.

      {
         parasol::Log log(__FUNCTION__);

         log.traceBranch("Processing root level tags.");

         Self->BodyTag   = NULL;
         Self->HeaderTag = NULL;
         Self->FooterTag = NULL;
         for (LONG i=0; xml->Tags[i]; i++) {
            if (!xml->Tags[i]->Attrib->Name) continue; // Ignore content

            ULONG tag_hash = StrHash(xml->Tags[i]->Attrib->Name, 0);

            if (tag_hash IS HASH_page) {
               break;
            }
            else if (tag_hash IS HASH_body) {
               // If a <body> tag contains any children, it is treated as a template and must contain an <inject/> tag so
               // that the XML insertion point is known.

               Self->BodyTag = xml->Tags[i]->Child;
               insert_xml(Self, xml, xml->Tags[i], Self->StreamLen, 0);
               i = xml->Tags[i]->Next->Index - 1;
            }
            else if (tag_hash IS HASH_background) {
               insert_xml(Self, xml, xml->Tags[i], Self->StreamLen, 0);
               i = xml->Tags[i]->Next->Index - 1;
            }
            else if (tag_hash IS HASH_editdef) {
               insert_xml(Self, xml, xml->Tags[i], Self->StreamLen, 0);
               i = xml->Tags[i]->Next->Index - 1;
            }
            else if (tag_hash IS HASH_template) {
               insert_xml(Self, xml, xml->Tags[i], Self->StreamLen, 0);
               i = xml->Tags[i]->Next->Index - 1;
            }
            else if (tag_hash IS HASH_head) {
               insert_xml(Self, xml, xml->Tags[i], Self->StreamLen, 0);
               i = xml->Tags[i]->Next->Index - 1;
            }
            else if (tag_hash IS HASH_info) {
               insert_xml(Self, xml, xml->Tags[i], Self->StreamLen, 0);
               i = xml->Tags[i]->Next->Index - 1;
            }
            else if (tag_hash IS HASH_include) {
               insert_xml(Self, xml, xml->Tags[i], Self->StreamLen, 0);
               i = xml->Tags[i]->Next->Index - 1;
            }
            else if (tag_hash IS HASH_parse) {
               insert_xml(Self, xml, xml->Tags[i], Self->StreamLen, 0);
               i = xml->Tags[i]->Next->Index - 1;
            }
            else if (tag_hash IS HASH_script) {
               insert_xml(Self, xml, xml->Tags[i], Self->StreamLen, 0);
               i = xml->Tags[i]->Next->Index - 1;
            }
            else if (tag_hash IS HASH_header) {
               Self->HeaderTag = xml->Tags[i]->Child;
               i = xml->Tags[i]->Next->Index - 1;
            }
            else if (tag_hash IS HASH_footer) {
               Self->FooterTag = xml->Tags[i]->Child;
               i = xml->Tags[i]->Next->Index - 1;
            }
            else log.warning("Tag '%s' Not supported at the root level.", xml->Tags[i]->Attrib->Name);
         }
      }

      if ((Self->HeaderTag) and (noheader IS FALSE)) {
         parasol::Log log(__FUNCTION__);
         log.traceBranch("Processing header.");
         insert_xml(Self, xml, Self->HeaderTag, Self->StreamLen, IXF_SIBLINGS|IXF_RESETSTYLE);
      }

      if (Self->BodyTag) {
         parasol::Log log(__FUNCTION__);
         log.traceBranch("Processing this page through the body tag.");

         START_TEMPLATE(page[0]->Child, xml, NULL)

            insert_xml(Self, xml, Self->BodyTag, Self->StreamLen, IXF_SIBLINGS|IXF_RESETSTYLE);

         END_TEMPLATE()
      }
      else {
         parasol::Log log(__FUNCTION__);
         log.traceBranch("Processing page '%s'.", XMLATTRIB(page[0], "name"));
         insert_xml(Self, xml, page[0]->Child, Self->StreamLen, IXF_SIBLINGS|IXF_RESETSTYLE);
      }

      if ((Self->FooterTag) and (!nofooter)) {
         parasol::Log log(__FUNCTION__);
         log.traceBranch("Processing footer.");
         insert_xml(Self, xml, Self->FooterTag, Self->StreamLen, IXF_SIBLINGS|IXF_RESETSTYLE);
      }

      //drwPermitDrawing();

      #ifdef DBG_STREAM
         print_stream(Self, Self->Stream);
      #endif

      // If an error occurred then we have to kill the document as the stream may contain disconnected escape
      // sequences (e.g. an unterminated ESC_TABLE sequence).

      if (Self->Error) unload_doc(Self, 0);

      Self->UpdateLayout = TRUE;
      if (Self->initialised()) redraw(Self, TRUE);

      if (Self->Buffer) { FreeResource(Self->Buffer); Self->Buffer = NULL; }
      if (Self->Temp)   { FreeResource(Self->Temp); Self->Temp = NULL; }
      if (Self->VArg)   { FreeResource(Self->VArg); Self->VArg = NULL; }

      #ifdef RAW_OUTPUT
         objFile::create file = { fl::Path("drive1:doc-stream.bin"), fl::Flags(FL_NEW|FL_WRITE) };
         file->write(Self->Stream, Self->StreamLen);
      #endif
   }
   else {
      if (Self->PageName) {
         char buffer[200];
         snprintf(buffer, sizeof(buffer), "Failed to find page '%s' in document '%s'.", Self->PageName, Self->Path);
         error_dialog("Load Failed", buffer, 0);
      }
      else {
         char buffer[200];
         snprintf(buffer, sizeof(buffer), "Failed to find a valid page in document '%s'.", Self->Path);
         error_dialog("Load Failed", buffer, 0);
      }
      Self->Error = ERR_Search;
   }

   if ((!Self->Error) and (Self->MouseOver)) {
      if (!gfxGetRelativeCursorPos(Self->PageID, &x, &y)) {
         check_mouse_pos(Self, x, y);
      }
   }

   if (!Self->PageProcessed) {
      for (auto trigger=Self->Triggers[DRT_PAGE_PROCESSED]; trigger; trigger=trigger->Next) {
         if (trigger->Function.Type IS CALL_SCRIPT) {
            if (auto script = trigger->Function.Script.Script) {
               scCallback(script, trigger->Function.Script.ProcedureID, NULL, 0, NULL);
            }
         }
         else if (trigger->Function.Type IS CALL_STDC) {
            if (auto routine = (void (*)(APTR, extDocument *))trigger->Function.StdC.Routine) {
               parasol::SwitchContext context(trigger->Function.StdC.Context);
               routine(trigger->Function.StdC.Context, Self);
            }
         }
      }
   }

   Self->PageProcessed = TRUE;
   return Self->Error;
}

//********************************************************************************************************************

static docresource * add_resource_id(extDocument *Self, LONG ID, LONG Type)
{
   docresource *r;
   if (!AllocMemory(sizeof(docresource), MEM_NO_CLEAR, &r)) {
      r->ObjectID = ID;
      r->Type     = Type;
      r->ClassID  = 0;
      r->Prev     = NULL;
      r->Next     = Self->Resources;
      if (Self->Resources) Self->Resources->Prev = r;
      Self->Resources = r;
      return r;
   }
   else return NULL;
}

//********************************************************************************************************************

static docresource * add_resource_ptr(extDocument *Self, APTR Address, LONG Type)
{
   docresource *r;
   if (!AllocMemory(sizeof(docresource), MEM_NO_CLEAR, &r)) {
      r->Address = Address;
      r->Type    = Type;
      r->Prev    = NULL;
      r->Next    = Self->Resources;
      if (Self->Resources) Self->Resources->Prev = r;
      Self->Resources = r;
      return r;
   }
   else return NULL;
}

//********************************************************************************************************************
// This function removes all allocations that were made in displaying the current page, and resets a number of
// variables that they are at the default settings for the next page.
//
// Set Terminate to TRUE only if the document object is being destroyed.
//
// The PageName is not freed because the desired page must not be dropped during refresh of manually loaded XML for
// example.

static ERROR unload_doc(extDocument *Self, BYTE Flags)
{
   parasol::Log log(__FUNCTION__);

   log.branch("Flags: $%.2x", Flags);

   #ifdef DBG_STREAM
      print_stream(Self, Self->Stream);
   #endif

   //drwForbidDrawing();

   log.trace("Resetting variables.");

   Self->FontColour.Red   = 0;
   Self->FontColour.Green = 0;
   Self->FontColour.Blue  = 0;
   Self->FontColour.Alpha = 1.0;

   Self->Highlight = glHighlight;

   Self->CursorColour.Red   = 0.4;
   Self->CursorColour.Green = 0.4;
   Self->CursorColour.Blue  = 0.8;
   Self->CursorColour.Alpha = 1.0;

   Self->LinkColour.Red   = 0;
   Self->LinkColour.Green = 0;
   Self->LinkColour.Blue  = 1.0;
   Self->LinkColour.Alpha = 1.0;

   Self->Background.Red   = 1.0;
   Self->Background.Green = 1.0;
   Self->Background.Blue  = 1.0;
   Self->Background.Alpha = 1.0;

   Self->SelectColour.Red   = 1.0;
   Self->SelectColour.Green = 0;
   Self->SelectColour.Blue  = 0;
   Self->SelectColour.Alpha = 1.0;

   Self->LeftMargin    = 10;
   Self->RightMargin   = 10;
   Self->TopMargin     = 10;
   Self->BottomMargin  = 10;
   Self->XPosition     = 0;
   Self->YPosition     = 0;
//   Self->ScrollVisible = FALSE;
   Self->PageHeight    = 0;
   Self->Invisible     = 0;
   Self->PageWidth     = 0;
   Self->CalcWidth     = 0;
   Self->LineHeight    = LINE_HEIGHT; // Default line height for measurements concerning the page (can be derived from a font)
   Self->RelPageWidth  = FALSE;
   Self->MinPageWidth  = MIN_PAGE_WIDTH;
   Self->DefaultScript = NULL;
   Self->SegCount      = 0;
   Self->SortCount     = 0;
   Self->BkgdGfx       = 0;
   Self->DrawIntercept = 0;
   Self->FontSize      = DEFAULT_FONTSIZE;
   Self->FocusIndex    = -1;
   Self->PageProcessed = FALSE;
   Self->MouseOverSegment = -1;
   Self->SelectIndex      = -1;
   Self->CursorIndex      = -1;
   Self->ActiveEditCellID = 0;
   Self->ActiveEditDef    = NULL;

   if (Self->ActiveEditDef) {
      deactivate_edit(Self, FALSE);
   }

   free_links(Self);

   Self->ECIndex = 0;
   Self->ECMax   = 0;
   if (Self->EditCells) { FreeResource(Self->EditCells); Self->EditCells = NULL; }

   if (Self->LinkIndex != -1) {
      Self->LinkIndex = -1;
      gfxRestoreCursor(PTR_DEFAULT, Self->UID);
   }

   if (Self->FontFace) FreeResource(Self->FontFace);
   if (Flags & ULD_TERMINATE) Self->FontFace = NULL;
   else Self->FontFace = StrClone("Open Sans");

   if (Self->Stream) FreeResource(Self->Stream);
   if (Flags & ULD_TERMINATE) Self->Stream = NULL;
   else Self->Stream = (UBYTE *)StrClone("");

   Self->StreamLen  = 0;
   Self->StreamSize = 0;
   Self->PageTag    = NULL;

   if (Self->SortSegments) { FreeResource(Self->SortSegments); Self->SortSegments = NULL; }

   if (Self->Segments) {
      FreeResource(Self->Segments);
      Self->Segments    = NULL;
      Self->MaxSegments = 0;
   }

   if (!(Flags & ULD_TERMINATE)) {
      Self->MaxSegments = 100;
      if (AllocMemory(sizeof(Self->Segments[0]) * Self->MaxSegments, MEM_NO_CLEAR, &Self->Segments) != ERR_Okay) {
         return ERR_AllocMemory;
      }
   }

   for (LONG i=0; i < ARRAYSIZE(Self->Triggers); i++) {
      if (auto trigger = Self->Triggers[i]) {
         while (trigger) {
            auto next = trigger->Next;
            FreeResource(trigger);
            trigger = next;
         }
         Self->Triggers[i] = NULL;
      }
   }

   if (Flags & ULD_TERMINATE) {
      if (Self->Vars) { FreeResource(Self->Vars); Self->Vars = NULL; }
   }

   if (Self->Params)      { FreeResource(Self->Params); Self->Params = NULL; }
   if (Self->Clips)       { FreeResource(Self->Clips); Self->Clips = NULL; }
   if (Self->Keywords)    { FreeResource(Self->Keywords); Self->Keywords = NULL; }
   if (Self->Author)      { FreeResource(Self->Author); Self->Author = NULL; }
   if (Self->Copyright)   { FreeResource(Self->Copyright); Self->Copyright = NULL; }
   if (Self->Description) { FreeResource(Self->Description); Self->Description = NULL; }
   if (Self->Title)       { FreeResource(Self->Title); Self->Title = NULL; }

   if (Self->EditDefs) {
      auto edit = Self->EditDefs;
      while (edit) {
         auto next = edit->Next;
         FreeResource(edit);
         edit = next;
      }
      Self->EditDefs = NULL;
   }

   auto mouseover = Self->MouseOverChain;
   while (mouseover) {
      auto mousenext = mouseover->Next;
      FreeResource(mouseover);
      mouseover = mousenext;
   }
   Self->MouseOverChain = NULL;

   // Free templates only if they have been modified (no longer at the default settings).

   if (Self->Templates) {
      if (Self->TemplatesModified != Self->Templates->Modified) {
         acFree(Self->Templates);
         Self->Templates = NULL;
      }
   }

   if (Self->Tabs) {
      FreeResource(Self->Tabs);
      Self->Tabs     = NULL;
      Self->MaxTabs  = 0;
      Self->TabIndex = 0;
   }

   // Remove all page related resources

   {
      parasol::Log log(__FUNCTION__);
      log.traceBranch("Freeing page-allocated resources.");

      auto resource = Self->Resources;
      while (resource) {
         if (resource->Type IS RT_MEMORY) {
            FreeResource(resource->Memory);
         }
         else if ((resource->Type IS RT_PERSISTENT_SCRIPT) or (resource->Type IS RT_PERSISTENT_OBJECT)) {
            if (Flags & ULD_REFRESH) {
               // Persistent objects and scripts will survive refreshes
               resource = resource->Next;
               continue;
            }
            else if (Flags & ULD_TERMINATE) acFree(resource->ObjectID);
            else QueueAction(AC_Free, resource->ObjectID);
         }
         else if (resource->Type IS RT_OBJECT_UNLOAD_DELAY) {
            if (Flags & ULD_TERMINATE) acFree(resource->ObjectID);
            else QueueAction(AC_Free, resource->ObjectID);
         }
         else acFree(resource->ObjectID);

         if (resource IS Self->Resources) Self->Resources = resource->Next;
         if (resource->Prev) resource->Prev->Next = resource->Next;
         if (resource->Next) resource->Next->Prev = resource->Prev;

         auto next = resource->Next;
         FreeResource(resource);
         resource = next;
      }
   }

   if (!Self->Templates) {
      if (!(Self->Templates = objXML::create::integral(fl::Name("xmlTemplates"), fl::Statement(glDefaultStyles),
         fl::Flags(XMF_PARSE_HTML|XMF_STRIP_HEADERS)))) return ERR_CreateObject;

      Self->TemplatesModified = Self->Templates->Modified;
   }

   Self->NoWhitespace = TRUE; // Reset whitespace flag

   if (Self->PageID) acMoveToPoint(Self->PageID, 0, 0, 0, MTF_X|MTF_Y);

   //drwPermitDrawing();

   Self->UpdateLayout = TRUE;
   Self->GeneratedID = AllocateID(IDTYPE_GLOBAL);

   if (Flags & ULD_REDRAW) {
      DRAW_PAGE(Self);
   }

   return ERR_Okay;
}

//********************************************************************************************************************
// If the layout needs to be recalculated, set the UpdateLayout field before calling this function.

static void redraw(extDocument *Self, BYTE Focus)
{
   parasol::Log log(__FUNCTION__);

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

static void error_dialog(CSTRING Title, CSTRING Message, ERROR Error)
{
   parasol::Log log(__FUNCTION__);
   static OBJECTID dialog_id = 0;

   log.warning("%s", Message);

   if (dialog_id) {
      if (CheckObjectExists(dialog_id) IS ERR_True) return;
   }

   OBJECTPTR dialog;
   if (!NewObject(ID_SCRIPT, &dialog)) {
      SetFields(dialog,
         FID_Name|TSTR,    "scDialog",
         FID_Owner|TLONG,  CurrentTaskID(),
         FID_Path|TSTR,    "scripts:gui/dialog.fluid",
         TAGEND);

      acSetVar(dialog, "modal", "1");
      acSetVar(dialog, "title", Title);
      acSetVar(dialog, "options", "okay");
      acSetVar(dialog, "type", "error");

      CSTRING errstr;
      if ((Error) and (errstr = GetErrorMsg(Error))) {
         LONG len = strlen(Message);
         char buffer[len+120];
         if (Message) {
            len = StrCopy(Message, buffer, sizeof(buffer));
            len += StrCopy("\n\nDetails: ", buffer+len, sizeof(buffer)-len);
         }
         else len = StrCopy("Error: ", buffer, sizeof(buffer));

         StrCopy(errstr, buffer+len, sizeof(buffer)-len);
         acSetVar(dialog, "message", buffer);
      }
      else acSetVar(dialog, "message", Message);

      if ((!acInit(dialog)) and (!acActivate(dialog))) {
         CSTRING *results;
         LONG size;
         if ((!GetFieldArray(dialog, FID_Results, (APTR *)&results, &size)) and (size > 0)) {
            dialog_id = StrToInt(results[0]);
         }
      }
   }
}

//********************************************************************************************************************

static void add_template(extDocument *Self, objXML *XML, XMLTag *Tag)
{
   parasol::Log log(__FUNCTION__);
   LONG i;

   // Validate the template (must have a name)

   for (i=1; i < Tag->TotalAttrib; i++) {
      if ((!StrMatch("name", Tag->Attrib[i].Name)) and (Tag->Attrib[i].Value[0])) break;
      if ((!StrMatch("class", Tag->Attrib[i].Name)) and (Tag->Attrib[i].Value[0])) break;
   }

   if (i >= Tag->TotalAttrib) {
      log.warning("A <template> is missing a name or class attribute.");
      return;
   }

   // Note: It would be nice if we scanned the existing templates and
   // replaced them correctly, however we're going to be lazy and override
   // styles by placing updated definitions at the end of the style list.

   STRING strxml;
   if (!xmlGetString(XML, Tag->Index, 0, &strxml)) {
      xmlInsertXML(Self->Templates, 0, XMI_PREV, strxml, 0);
      FreeResource(strxml);
   }
   else log.warning("Failed to convert template %d to an XML string.", Tag->Index);
}

//********************************************************************************************************************

static CSTRING get_font_style(LONG Options)
{
   if ((Options & (FSO_BOLD|FSO_ITALIC)) IS (FSO_BOLD|FSO_ITALIC)) return "Bold Italic";
   else if (Options & FSO_BOLD) return "Bold";
   else if (Options & FSO_ITALIC) return "Italic";
   else return "Regular";
}

//********************************************************************************************************************
// Converts a font index into a font structure.

static objFont * lookup_font(LONG Index, CSTRING Caller)
{
   if ((glFonts) and (Index < glTotalFonts) and (Index >= 0)) return glFonts[Index].Font;
   else {
      parasol::Log log(__FUNCTION__);
      log.warning("Bad font index %d.  Max: %d.  Caller: %s", Index, glTotalFonts, Caller);
      if (glFonts) return glFonts[0].Font; // Always try to return a font rather than NULL
      else return NULL;
   }
}

//********************************************************************************************************************
// Creates a font (if it doesn't already exist) and returns an index.
//
// Created fonts belong to the Document module rather than the current object, so they can be reused between multiple
// open documents.

static LONG create_font(CSTRING Face, CSTRING Style, LONG Point)
{
   parasol::Log log(__FUNCTION__);
   LONG i;
   #define FONT_BLOCK_SIZE 20

   if (!Style) Style = "Regular";
   if (Point < 3) Point = DEFAULT_FONTSIZE;

   // If we already have loaded this font, return it.

   for (i=0; i < glTotalFonts; i++) {
      if ((!StrMatch(Face, glFonts[i].Font->Face)) and (!StrMatch(Style, glFonts[i].Font->Style)) and (Point IS glFonts[i].Point)) {
         log.trace("Match %d = %s(%s,%d)", i, Face, Style, Point);
         return i;
      }
   }

   log.branch("Index: %d, %s, %s, %d.  Cached: %d", glTotalFonts, Face, Style, Point, glTotalFonts);

   AdjustLogLevel(2);

   objFont *font = objFont::create::integral(
      fl::Owner(modDocument->UID), fl::Face(Face), fl::Style(Style), fl::Point(Point), fl::Flags(FTF_ALLOW_SCALE));

   if (font) {
      // Perform a second check in case the font we ended up with is in our cache.  This can occur if the font we have acquired
      // is a little different to what we requested (e.g. scalable instead of fixed, or a different face).

      for (i=0; i < glTotalFonts; i++) {
         if ((!StrMatch(font->Face, glFonts[i].Font->Face)) and (!StrMatch(font->Style, glFonts[i].Font->Style)) and (font->Point IS glFonts[i].Point)) {
            log.trace("Match %d = %s(%s,%d)", i, Face, Style, Point);
            acFree(font);
            AdjustLogLevel(-2);
            return i;
         }
      }

      if (glTotalFonts IS glMaxFonts) {
         log.msg("Extending font array.");
         FontEntry *array;
         if (!AllocMemory((glMaxFonts + FONT_BLOCK_SIZE) * sizeof(FontEntry), MEM_UNTRACKED, &array)) {
            glMaxFonts += FONT_BLOCK_SIZE;
            if (glFonts) {
               CopyMemory(glFonts, array, sizeof(FontEntry) * glTotalFonts);
               FreeResource(glFonts);
            }
            glFonts = array;
         }
         else {
            i = -1;
            goto exit;
         }
      }
      i = glTotalFonts++;
      glFonts[i].Font = font;
      glFonts[i].Point = Point;
   }
   else i = -1;

exit:
   AdjustLogLevel(-2);
   return i;
}

//********************************************************************************************************************
// This function creates segments, which are used during the drawing process as well as user interactivity, e.g. to
// determine the character that the mouse is positioned over.  A segment will usually consist of a sequence of
// text characters or escape sequences.
//
// Offset: The start of the line within the stream.
// Stop:   The stream index at which the line stops.

static LONG add_drawsegment(extDocument *Self, LONG Offset, LONG Stop, layout *Layout,
   LONG Y, LONG Width, LONG AlignWidth, CSTRING Debug)
{
   parasol::Log log(__FUNCTION__);
   LONG i;

   // Determine trailing whitespace at the end of the line.  This helps
   // to prevent situations such as underlining occurring in whitespace
   // at the end of the line during word-wrapping.

   LONG trimstop = Stop;
   while ((Self->Stream[trimstop-1] <= 0x20) and (trimstop > Offset)) {
      if (Self->Stream[trimstop-1] IS CTRL_CODE) break;
      trimstop--;
   }

   if (Offset >= Stop) {
      LAYOUT("add_drawsegment()","Cancelling addition, no content in line to add (bytes %d-%d) \"%.20s\" (%s)", Offset, Stop, printable(Self->Stream+Offset, 60), Debug);
      return -1;
   }

   // Check the new segment to see if there are any text characters or escape codes relevant to drawing

   bool text_content = FALSE;
   bool control_content = FALSE;
   bool object_content = FALSE;
   bool allow_merge = TRUE;
   for (i=Offset; i < Stop;) {
      if (Self->Stream[i] IS CTRL_CODE) {
         LONG code = ESCAPE_CODE(Self->Stream, i);
         control_content = TRUE;
         if (code IS ESC_OBJECT) object_content = TRUE;
         if ((code IS ESC_OBJECT) or (code IS ESC_TABLE_START) or (code IS ESC_TABLE_END) or (code IS ESC_FONT)) {
            allow_merge = FALSE;
         }
      }
      else {
         text_content = TRUE;
         allow_merge = FALSE;
      }

      NEXT_CHAR(Self->Stream, i);
   }

   LONG Height = Layout->line_height;
   LONG BaseLine = Layout->base_line;
   if (text_content) {
      if (Height <= 0) {
         // No line-height given and there is text content - use the most recent font to determine the line height
         Height = Layout->font->LineSpacing;
         BaseLine = Layout->font->Ascent;
      }
      else if (!BaseLine) {
         // If base-line is missing for some reason, define it
         BaseLine = Layout->font->Ascent;
      }
   }
   else {
      if (Height <= 0) Height = 0;
      if (BaseLine <= 0) BaseLine = 0;
   }

#ifdef DBG_STREAM
   LAYOUT("add_drawsegment()","#%d, Bytes: %d-%d, Area: %dx%d,%d:%dx%d, WordWidth: %d, CursorY: %d, [%.20s]...[%.20s] (%s)",
      Self->SegCount, Offset, Stop, Layout->line_x, Y, Width, AlignWidth, Height, Layout->wordwidth,
      Layout->cursory, printable(Self->Stream + Offset, Stop-Offset), printable2(Self->Stream+Stop, 60), Debug);
#endif

   LONG segment = Self->SegCount;
   LONG x = Layout->line_x;
   BYTE patched = FALSE;

   if ((segment > 0) and (Offset < Self->Segments[segment-1].Stop)) {
      // Patching: If the start of the new segment is < the end of the previous segment,
      // adjust the previous segment so that it stops at the beginning of our new segment.
      // This prevents overlapping between segments and the two segments will be patched
      // together in the next section of this routine.

      if (Offset <= Self->Segments[segment-1].Index) {
         // If the start of the new segment retraces to an index that has already been configured,
         // then we have actually encountered a coding flaw and the caller should be investigated.

         log.warning("(%s) New segment #%d retraces to index %d, which has been configured by previous segments.", Debug, segment, Offset);
         return -1;
      }
      else {
         LAYOUT("add_drawsegment()","New segment #%d start index is less than (%d < %d) the end of previous segment - will patch up.", segment, Offset, Self->Segments[segment-1].Stop);
         Self->Segments[segment-1].Stop = Offset;
      }
   }

   // Is the new segment a continuation of the previous one, and does the previous segment contain content?
   if ((allow_merge) and (segment > 0) and (Self->Segments[segment-1].Stop IS Offset) and
       (Self->Segments[segment-1].AllowMerge IS TRUE)) {
      // We are going to extend the previous line rather than add a new one, as the two
      // segments only contain control codes.

      segment--;

      Offset = Self->Segments[segment].Index;
      x      = Self->Segments[segment].X;
      Width += Self->Segments[segment].Width;
      AlignWidth += Self->Segments[segment].AlignWidth;
      if (Self->Segments[segment].Height > Height) {
         Height = Self->Segments[segment].Height;
         BaseLine = Self->Segments[segment].BaseLine;
      }

      patched = TRUE;
   }

   if (segment >= Self->MaxSegments) {
      DocSegment *lines;
      if (!AllocMemory(sizeof(Self->Segments[0]) * (Self->MaxSegments + 100), MEM_NO_CLEAR, &lines)) {
         CopyMemory(Self->Segments, lines, sizeof(Self->Segments[0]) * Self->MaxSegments);
         FreeResource(Self->Segments);
         Self->Segments = lines;
         Self->MaxSegments += 100;
      }
      else return -1;
   }

#ifdef DEBUG
   // If this is a segmented line, check if any previous entries have greater
   // heights.  If so, this is considered an internal programming error.

   if ((Layout->split_start != NOTSPLIT) and (Height > 0)) {
      for (i=Layout->split_start; i < segment; i++) {
         if (Self->Segments[i].Depth != Self->Depth) continue;
         if (Self->Segments[i].Height > Height) {
            log.warning("A previous entry in segment %d has a height larger than the new one (%d > %d)", i, Self->Segments[i].Height, Height);
            BaseLine = Self->Segments[i].BaseLine;
            Height = Self->Segments[i].Height;
         }
      }
   }
#endif

   Self->Segments[segment].Index    = Offset;
   Self->Segments[segment].Stop     = Stop;
   Self->Segments[segment].TrimStop = trimstop;
   Self->Segments[segment].X   = x;
   Self->Segments[segment].Y   = Y;
   Self->Segments[segment].Height   = Height;
   Self->Segments[segment].BaseLine = BaseLine;
   Self->Segments[segment].Width    = Width;
   Self->Segments[segment].Depth    = Self->Depth;
   Self->Segments[segment].AlignWidth = AlignWidth;
   Self->Segments[segment].TextContent  = text_content;
   Self->Segments[segment].ControlContent = control_content;
   Self->Segments[segment].ObjectContent = object_content;
   Self->Segments[segment].AllowMerge = allow_merge;
   Self->Segments[segment].Edit = Self->EditMode;

   // If a line is segmented, we need to backtrack for earlier line segments and ensure that their height and baseline
   // is matched to that of the last line (which always contains the maximum height and baseline values).

   if ((Layout->split_start != NOTSPLIT) and (Height)) {
      if (segment != Layout->split_start) {
         LAYOUT("add_drawsegment:","Resetting height (%d) & base (%d) of segments index %d-%d.  Array: %p", Height, BaseLine, segment, Layout->split_start, Self->Segments);
         for (i=Layout->split_start; i < segment; i++) {
            if (Self->Segments[i].Depth != Self->Depth) continue;
            Self->Segments[i].Height = Height;
            Self->Segments[i].BaseLine = BaseLine;
         }
      }
   }

   if (!patched) {
      Self->SegCount++;
   }

   return segment;
}

/*********************************************************************************************************************
** convert_xml_args()
**
**   Attrib: Pointer to an array of XMLAttribs that are to be analysed for argument references.
**   Total:  The total number of XMLAttribs in the Attrib array.
**
** Args originate from Self->ArgNest, which is controlled by START_TEMPLATE().
**
** This function converts arguments such as [@arg] in an XMLTag.  The converted tag strings are stored in our internal
** buffer.  If the @ symbol is not present in the [ ] brackets, object/field translation occurs.
**
** Calculations can also be performed, e.g. [=5+7]
**
** The escape code for brackets are &lsqr; and &rsqr; (not in the XML escape code standard and thus are unconverted up
** until this function is reached).
**
** If an attribute name is prefixed with '$' then no translation of the attribute value is attempted.
**
** If a major error occurs during processing, the function will abort, returning the error and also setting the Error
** field to the resulting error code.  The most common reason for an abort is a buffer overflow or memory allocation
** problems, so a complete abort of document processing is advisable.
**
** RESERVED WORDS
**    %index       Current loop index, if within a repeat loop.
**    %id          A unique ID that is regenerated on each document refresh.
**    %self        ID of the document object.
**    %platform    Windows, Linux or Native.
**    %random      Random string of 9 digits.
**    %currentpage Name of the current page.
**    %nextpage    Name of the next page.
**    %prevpage    Name of the previous page.
**    %path        Current working path.
**    %author      Document author.
**    %description Document description.
**    %copyright   Document copyright.
**    %keywords    Document keywords.
**    %title       Document title.
**    %font        Face, point size and style of the current font.
**    %fontface    Face of the current font.
**    %fontcolour  Colour of the current font.
**    %fontsize    Point size of the current font.
**    %lineno      The current 'line' (technically segmented line) in the document.
**    %content     Inject content (same as <inject/> but usable inside tag attributes)
**    %tm-day      The current day (0 - 31)
**    %tm-month    The current month (1 - 12)
**    %tm-year     The current year (2008+)
**    %tm-hour     The current hour (0 - 23)
**    %tm-minute   The current minute (0 - 59)
**    %tm-second   The current second (0 - 59)
**    %viewheight  Height of the document's available viewing area
**    %viewwidth   Width of the the document's available viewing area.
*/

static ERROR convert_xml_args(extDocument *Self, XMLAttrib *Attrib, LONG Total)
{
   parasol::Log log(__FUNCTION__);
   Field *classfield;
   STRING src, Buffer;
   CSTRING str;
   LONG attrib, pos, arg, buf_end, i, j, end;
   OBJECTID objectid;
   OBJECTPTR object;
   WORD balance;
   char name[120];
   bool mod;
   BYTE save;

   if (!Attrib->Name) return ERR_Okay; // Do not translate content tags

   log.trace("Attrib: %p, Total: %d", Attrib, Total);

   if (!Self->TBuffer) {
      Self->TBufferSize = 0xffff;
      if (AllocMemory(Self->TBufferSize, MEM_STRING|MEM_NO_CLEAR, &Self->TBuffer)) {
         Self->Error = ERR_AllocMemory;
         return ERR_AllocMemory;
      }
      Self->TBuffer[0] = 0;
   }

   Buffer = Self->Buffer;
   ERROR error = ERR_Okay;
   for (attrib=1; (attrib < Total) and (Self->ArgIndex < MAX_ARGS) and (!error); attrib++) {
      if (Attrib[attrib].Name[0] IS '$') continue;
      if (!(src = Attrib[attrib].Value)) continue;

      // Do nothing if there are no special references being used

      for (i=0; src[i]; i++) {
         if (src[i] IS '[') break;
         if ((src[i] IS '&') and ((!StrCompare("&lsqr;", src+i, 0, 0)) or (!StrCompare("&rsqr;", src+i, 0, 0)))) break;
      }
      if (!src[i]) continue;

      // Copy the value into the buffer and translate it

      pos = Self->BufferIndex;
      pos += StrCopy(Attrib[attrib].Value, Buffer+pos, Self->BufferSize-pos);
      if (pos >= Self->BufferSize) return (Self->Error = ERR_BufferOverflow);

      // NB: Translation works backwards, from pos back to BufferIndex

      buf_end = pos;
      mod = false;

      while ((pos >= Self->BufferIndex) and (!error)) {
         if (Buffer[pos] IS '&') {
            if (!StrCompare("&lsqr;", Buffer+pos, 0, 0)) {
               Buffer[pos] = '[';
               CopyMemory(Buffer+pos+6, Buffer+pos+1, StrLength(Buffer+pos+6)+1);
               mod = true;
            }
            else if (!StrCompare("&rsqr;", Buffer+pos, 0, 0)) {
               Buffer[pos] = ']';
               CopyMemory(Buffer+pos+6, Buffer+pos+1, StrLength(Buffer+pos+6)+1);
               mod = true;
            }
         }
         else if (Buffer[pos] IS '[') {
            if (Buffer[pos+1] IS '=') {
               // Perform a calcuation within [= ... ]

               // Copy the section to temp, calculate it, then insert_string() the calculation

               mod = true;
               j = 0;
               end = pos+2;
               while (Buffer[end]) {
                  if (Buffer[end] IS ']') break;
                  if (Buffer[end] IS '\'') {
                     Self->Temp[j++] = '\'';
                     for (++end; (Buffer[end]) and (Buffer[end] != '\''); end++) Self->Temp[j++] = Buffer[end];
                     if (Buffer[end]) Self->Temp[j++] = Buffer[end++];
                  }
                  else if (Buffer[end] IS '"') {
                     Self->Temp[j++] = '"';
                     for (++end; (Buffer[end]) and (Buffer[end] != '"'); end++);
                     if (Buffer[end]) Self->Temp[j++] = Buffer[end++];
                  }
                  else Self->Temp[j++] = Buffer[end++];
               }
               Self->Temp[j] = 0;
               if (Buffer[end] IS ']') end++;
               calc(Self->Temp, 0, Self->Temp+j+1, Self->TempSize-j-1);
               error = insert_string(Self->Temp+j+1, Buffer, Self->BufferSize, pos, end-pos);
            }
            else if (Buffer[pos+1] IS '%') {
               // Check against reserved keywords

               BYTE savemod = mod;
               mod = true;
               str = Buffer + pos + 2;
               if (!StrCompare("index]", str, 0, 0)) {
                  error = insert_string(std::to_string(Self->LoopIndex).c_str(), Buffer, Self->BufferSize, pos, sizeof("[%index]")-1);
               }
               else if (!StrCompare("id]", str, 0, 0)) {
                  error = insert_string(std::to_string(Self->GeneratedID).c_str(), Buffer, Self->BufferSize, pos, sizeof("[%id]")-1);
               }
               else if (!StrCompare("self]", str, 0, 0)) {
                  error = insert_string(std::to_string(Self->UID).c_str(), Buffer, Self->BufferSize, pos, sizeof("[%self]")-1);
               }
               else if (!StrCompare("platform]", str, 0, 0)) {
                  auto state = GetSystemState();
                  insert_string(state->Platform, Buffer, Self->BufferSize, pos, sizeof("[%platform]")-1);
               }
               else if (!StrCompare("random]", str, 0, 0)) {
                  // Generate a random string of digits
                  char random[10];
                  for (j=0; (size_t)j < sizeof(random)-1; j++) random[j] = '0' + (rand() % 10);
                  random[j] = 0;
                  insert_string(random, Buffer, Self->BufferSize, i, sizeof("[%random]")-1);
               }
               else if (!StrCompare("currentpage]", str, 0, 0)) {
                  str = "";
                  if (Self->PageTag) str = XMLATTRIB(Self->PageTag, "name");
                  insert_string(str, Buffer, Self->BufferSize, pos, sizeof("[%currentpage]")-1);
               }
               else if (!StrCompare("nextpage]", str, 0, 0)) {
                  str = "";
                  if (Self->PageTag) {
                     for (auto tag=Self->PageTag->Next; tag; tag=tag->Next) {
                        if (!StrMatch("page", tag->Attrib->Name)) {
                           if (!XMLATTRIB(tag, "hidden")) { // Ignore hidden pages
                              if (((str = XMLATTRIB(tag, "name"))) and (*str)) break;
                              else str = "";
                           }
                        }
                     }
                  }

                  insert_string(str, Buffer, Self->BufferSize, pos, sizeof("[%nextpage]")-1);
               }
               else if (!StrCompare("prevpage]", str, 0, 0)) {
                  str = "";
                  if (Self->PageTag) {
                     for (auto tag=Self->PageTag->Prev; tag; tag=tag->Prev) {
                        if (!StrMatch("page", tag->Attrib->Name)) {
                           if (!XMLATTRIB(tag, "hidden")) { // Ignore hidden pages
                              if (((str = XMLATTRIB(tag, "name"))) and (*str)) break;
                              else str = "";
                           }
                        }
                     }
                  }

                  insert_string(str, Buffer, Self->BufferSize, pos, sizeof("[%prevpage]")-1);
               }
               else if (!StrCompare("path]", str, 0, 0)) {
                  CSTRING workingpath = "";
                  GET_WorkingPath(Self, &workingpath);
                  if (!workingpath) workingpath = "";
                  error = insert_string(workingpath, Buffer, Self->BufferSize, pos, sizeof("[%path]")-1);
               }
               else if (!StrCompare("author]", str, 0, 0)) {
                  error = insert_string(Self->Author, Buffer, Self->BufferSize, pos, sizeof("[%author]")-1);
               }
               else if (!StrCompare("description]", str, 0, 0)) {
                  error = insert_string(Self->Description, Buffer, Self->BufferSize, pos, sizeof("[%description]")-1);
               }
               else if (!StrCompare("copyright]", str, 0, 0)) {
                  error = insert_string(Self->Copyright, Buffer, Self->BufferSize, pos, sizeof("[%copyright]")-1);
               }
               else if (!StrCompare("keywords]", str, 0, 0)) {
                  error = insert_string(Self->Keywords, Buffer, Self->BufferSize, pos, sizeof("[%keywords]")-1);
               }
               else if (!StrCompare("title]", str, 0, 0)) {
                  error = insert_string(Self->Title, Buffer, Self->BufferSize, pos, sizeof("[%title]")-1);
               }
               else if (!StrCompare("font]", str, 0, 0)) {
                  objFont *font;
                  char fullfont[256];
                  if ((font = lookup_font(Self->Style.FontStyle.Index, "convert_xml"))) {
                     snprintf(fullfont, sizeof(fullfont), "%s:%.4f:%s", font->Face, font->Point, font->Style);
                     error = insert_string(fullfont, Buffer, Self->BufferSize, pos, sizeof("[%font]")-1);
                  }
               }
               else if (!StrCompare("fontface]", str, 0, 0)) {
                  objFont *font;
                  if ((font = lookup_font(Self->Style.FontStyle.Index, "convert_xml"))) {
                     error = insert_string(font->Face, Buffer, Self->BufferSize, pos, sizeof("[%fontface]")-1);
                  }
               }
               else if (!StrCompare("fontcolour]", str, 0, 0)) {
                  objFont *font;
                  char colour[28];
                  if ((font = lookup_font(Self->Style.FontStyle.Index, "convert_xml"))) {
                     snprintf(colour, sizeof(colour), "#%.2x%.2x%.2x%.2x", font->Colour.Red, font->Colour.Green, font->Colour.Blue, font->Colour.Alpha);
                     error = insert_string(colour, Buffer, Self->BufferSize, pos, sizeof("[%fontcolour]")-1);
                  }
               }
               else if (!StrCompare("fontsize]", str, 0, 0)) {
                  objFont *font;
                  if ((font = lookup_font(Self->Style.FontStyle.Index, "convert_xml"))) {
                     char num[28];
                     IntToStr(font->Point, num, sizeof(num));
                     error = insert_string(num, Buffer, Self->BufferSize, pos, sizeof("[%fontsize]")-1);
                  }
               }
               else if (!StrCompare("lineno]", str, 0, 0)) {
                  char num[28];
                  IntToStr(Self->SegCount, num, sizeof(num));
                  error = insert_string(num, Buffer, Self->BufferSize, pos, sizeof("[%lineno]")-1);
               }
               else if (!StrCompare("content]", str, 0, 0)) {
                  if ((Self->InTemplate) and (Self->InjectTag)) {
                     if (!xmlGetContent(Self->InjectXML, Self->InjectTag->Index, Self->Temp, Self->TempSize)) {
                        STRING tmp;
                        if (!xmlGetString(Self->InjectXML, Self->InjectTag->Index, XMF_INCLUDE_SIBLINGS, &tmp)) {
                           insert_string(tmp, Buffer, Self->BufferSize, pos, sizeof("[%content]")-1);
                           FreeResource(tmp);
                        }
                     }
                  }
               }
               else if (!StrCompare("tm-day]", str, 0, 0)) {

               }
               else if (!StrCompare("tm-month]", str, 0, 0)) {

               }
               else if (!StrCompare("tm-year]", str, 0, 0)) {

               }
               else if (!StrCompare("tm-hour]", str, 0, 0)) {

               }
               else if (!StrCompare("tm-minute]", str, 0, 0)) {

               }
               else if (!StrCompare("tm-second]", str, 0, 0)) {

               }
               else if (!StrCompare("version]", str, 0, 0)) {
                  error = insert_string(RIPPLE_VERSION, Buffer, Self->BufferSize, pos, sizeof("[%version]")-1);
               }
               else if (!StrCompare("viewheight]", str, 0, 0)) {
                  char num[28];
                  IntToStr(Self->AreaHeight, num, sizeof(num));
                  error = insert_string(num, Buffer, Self->BufferSize, pos, sizeof("[%viewheight]")-1);
               }
               else if (!StrCompare("viewwidth]", str, 0, 0)) {
                  char num[28];
                  IntToStr(Self->AreaWidth, num, sizeof(num));
                  error = insert_string(num, Buffer, Self->BufferSize, pos, sizeof("[%viewwidth]")-1);
               }
               else mod = savemod;
            }
            else if (Buffer[pos+1] IS '@') { // Translate argument reference
               mod = true; // Indicate that the buffer will be used, even if the argument is not found, it will be replaced with an empty string
               BYTE processed = FALSE;
               for (WORD ni=Self->ArgNestIndex-1; (ni >= 0) and (!processed); ni--) {
                  XMLTag *args = Self->ArgNest[ni];
                  for (arg=1; arg < args->TotalAttrib; arg++) {
                     if (!StrCompare(args->Attrib[arg].Name, Buffer+pos+2, 0, 0)) {
                        end = pos + 2 + StrLength(args->Attrib[arg].Name);
                        if ((Buffer[end] IS ']') or (Buffer[end] IS ':')) {

                           if (Buffer[end] IS ':') {
                              end++;
                              if (Buffer[end] IS '\'') for (++end; (Buffer[end]) and (Buffer[end] != '\''); end++);
                              else if (Buffer[end] IS '"') for (++end; (Buffer[end]) and (Buffer[end] != '"'); end++);
                              while ((Buffer[end]) and (Buffer[end] != ']')) end++;
                              if (Buffer[end] IS ']') end++;
                           }
                           else end++;

                           error = insert_string(args->Attrib[arg].Value, Buffer, Self->BufferSize, pos, end-pos);

                           processed = TRUE;
                           break;
                        }
                     }
                  }
               }

               if (processed) continue;

               // Check against global arguments / variables

               for (j=pos+2; Buffer[j] and (Buffer[j] != ']') and (Buffer[j] != ':') and (Buffer[j] != '('); j++);
               save = Buffer[j];
               Buffer[j] = 0;
               if (!(str = VarGetString(Self->Vars, Buffer+pos+2))) {
                  str = VarGetString(Self->Params, Buffer+pos+2);
               }
               Buffer[j] = save;

               if (str) {
                  end = pos + 2;
                  while ((Buffer[end]) and (Buffer[end] != ':') and (Buffer[end] != ']')) end++;
                  if (Buffer[end] IS ':') {
                     end++;
                     if (Buffer[end] IS '\'') for (++end; (Buffer[end]) and (Buffer[end] != '\''); end++);
                     else if (Buffer[end] IS '"') for (++end; (Buffer[end]) and (Buffer[end] != '"'); end++);
                     while ((Buffer[end]) and (Buffer[end] != ']')) end++;
                     if (Buffer[end] IS ']') end++;
                  }
                  else if (Buffer[end] IS ']') end++;

                  error = insert_string(str, Buffer, Self->BufferSize, pos, end-pos);
                  continue;
               }

               // Look for a default arg value and use it (identified after use of the : symbol)

               UBYTE terminator;

               for (end=pos; Buffer[end] and (Buffer[end] != ']') and (Buffer[end] != ':'); end++);
               if (Buffer[end] IS ':') {
                  end++;
                  if (Buffer[end] IS '\'') { terminator = '\''; end++; }
                  else if (Buffer[end] IS '"') { terminator = '"'; end++; }
                  else terminator = ']';

                  for (i=0; (Buffer[end]) and (Buffer[end] != terminator) and ((size_t)i < sizeof(name)-1); i++,end++) {
                     name[i] = Buffer[end];
                  }
                  name[i] = 0;

                  while ((Buffer[end]) and (Buffer[end] != ']')) end++;
                  if (Buffer[end] IS ']') end++;

                  insert_string(name, Buffer, Self->BufferSize, pos, end-pos);
               }
               else insert_string("", Buffer, Self->BufferSize, pos, end+1-pos);
            }
            else {
               LONG j;

               // Object translation

               // Make sure that there is a closing bracket

               balance = 0;
               for (end=pos; Buffer[end]; end++) {
                  if (Buffer[end] IS '[') balance++;
                  else if (Buffer[end] IS ']') {
                     balance--;
                     if (!balance) break;
                  }
               }

               if (Buffer[end] != ']') {
                  log.warning("Imbalanced object/argument reference detected.");
                  break;
               }
               end++;

               mod = true;

               // Retrieve the name of the object

               for (j=0,i=pos+1; (Buffer[i] != '.') and (Buffer[i] != ']') and ((size_t)j < sizeof(name)-1); i++,j++) {
                  name[j] = Buffer[i];
               }
               name[j] = 0;

               // Get the object ID

               objectid = 0;
               if (*name) {
                  if (!StrMatch(name, "self")) {
                     // [self] can't be used in RIPPLE, because arguments are parsed prior to object
                     // creation.  We print a message to remind the developer of this rather than
                     // failing quietly.

                     log.warning("It is not possible for an object to reference itself via [self] in RIPPLE.");
                  }
                  else if (!StrMatch(name, "owner")) {
                     if (Self->CurrentObject) objectid = Self->CurrentObject->UID;
                  }
                  else {
                     if (!FindObject(name, 0, FOF_SMART_NAMES, &objectid)) {
                        if (!(Self->Flags & DCF_UNRESTRICTED)) {
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
                  }

                  if (objectid) {
                     if (valid_objectid(Self, objectid)) {
                        OBJECTPTR target;
                        STRING strbuf = Self->TBuffer;
                        object = NULL;
                        strbuf[0] = 0;
                        if (Buffer[i] IS '.') { // Get the field from the object
                           i++;
                           LONG j = 0;
                           while ((i < end-1) and ((size_t)j < sizeof(name)-1)) name[j++] = Buffer[i++];
                           name[j] = 0;
                           if (!AccessObjectID(objectid, 2000, &object)) {
                              if (((classfield = FindField(object, StrHash(name, FALSE), &target))) and (classfield->Flags & FD_STRING)) {
                                 error = object->get(classfield->FieldID, &strbuf);
                              }
                              else { // Get field as a variable type and manage any buffer overflow
repeat:
                                 Self->TBuffer[Self->TBufferSize-1] = 0;
                                 GetFieldVariable(object, name, Self->TBuffer, Self->TBufferSize);
                                 if (Self->TBuffer[Self->TBufferSize-1]) {
                                    STRING newbuf;
                                    parasol::SwitchContext context(modDocument);
                                    if (!AllocMemory(Self->TBufferSize + 1024, MEM_STRING|MEM_NO_CLEAR, &newbuf)) {
                                       FreeResource(Self->TBuffer);
                                       Self->TBuffer = newbuf;
                                       Self->TBufferSize = Self->TBufferSize + 1024;
                                       goto repeat;
                                    }
                                 }
                              }
                              error = ERR_Okay; // For fields, error code is always Okay so that the reference evaluates to NULL
                           }
                           else error = ERR_AccessObject;
                        }
                        else { // Convert the object reference to an ID
                           Self->TBuffer[0] = '#';
                           IntToStr(objectid, Self->TBuffer+1, Self->TBufferSize-1);
                        }

                        if (!error) error = insert_string(strbuf, Buffer, Self->BufferSize, pos, end-pos);

                        if (object) ReleaseObject(object);
                     }
                     else log.warning("Access denied to object '%s' #%d (tag %s, %s=%s)", name, objectid, Attrib->Name, Attrib[attrib].Name, Attrib[attrib].Value);
                  }
                  else log.warning("Object '%s' does not exist.", name);
               }
            }
         }

         pos--;
      } // while pos > BufferIndex

      if (mod) {
         //log.warning("Check: %s", Buffer+Self->BufferIndex);
         // Remember the changes that are being made to the XML tag values
         Self->VArg[Self->ArgIndex].Attrib = &Attrib[attrib].Value;
         Self->VArg[Self->ArgIndex].String = Attrib[attrib].Value;
         Self->ArgIndex++;

         // Point the existing tag value to our buffer
         Attrib[attrib].Value = Buffer + Self->BufferIndex;

         Self->BufferIndex += StrLength(Buffer + Self->BufferIndex) + 1;
      }
   } // for (attrib...)

   if (error) Self->Error = log.warning(error);
   return error;
}

/*********************************************************************************************************************
** Insert: The string to be inserted.
** Buffer: The start of the buffer region.
** Size:   The complete size of the target buffer.
** Pos:    The target position for the insert.
** ReplaceChars: If characters will be replaced, specify the number here.
**
** The only error that this function can return is a buffer overflow.
*/

static ERROR insert_string(CSTRING Insert, STRING Buffer, LONG BufferSize, LONG Pos, LONG ReplaceLen)
{
   LONG inlen, i, buflen, j;

   if (!Insert) Insert = "";
   for (inlen=0; Insert[inlen]; inlen++);

   Buffer += Pos;
   BufferSize -= Pos;
   Pos = 0;

   if (inlen < ReplaceLen) {
      // The string to insert is smaller than the number of characters to replace.
      StrCopy(Insert, Buffer, COPY_ALL);
      i = ReplaceLen;
      while (Buffer[i]) Buffer[inlen++] = Buffer[i++];
      Buffer[inlen] = 0;
   }
   else if (inlen IS ReplaceLen) {
      while (*Insert) *Buffer++ = *Insert++;
   }
   else {
      // Check if an overflow will occur

      for (buflen=0; Buffer[buflen]; buflen++);

      if ((BufferSize - 1) < (buflen - ReplaceLen + inlen)) {
         return ERR_BufferOverflow;
      }

      // Expand the string
      i = buflen + (inlen - ReplaceLen) + 1;
      buflen += 1;
      j = buflen - ReplaceLen + 1;
      while (j > 0) {
         Buffer[i--] = Buffer[buflen--];
         j--;
      }

      for (i=0; i < inlen; i++) Buffer[i] = Insert[i];
   }

   return ERR_Okay;
}

//********************************************************************************************************************
// Checks if an object reference is a valid member of the document.

static BYTE valid_object(extDocument *Self, OBJECTPTR Object)
{
   OBJECTPTR obj;

   if (Self->Flags & DCF_UNRESTRICTED) return TRUE;

   obj = Object;
   while (obj) {
      if (!obj->OwnerID) return FALSE;
      if (obj->OwnerID < 0) return valid_objectid(Self, obj->UID); // Switch to scanning public objects
      obj = GetObjectPtr(obj->OwnerID);
      if (obj IS Self) return TRUE;
   }
   return FALSE;
}

//********************************************************************************************************************
//Checks if an object reference is a valid member of the document.

static BYTE valid_objectid(extDocument *Self, OBJECTID ObjectID)
{
   if (Self->Flags & DCF_UNRESTRICTED) return TRUE;

   while (ObjectID) {
      ObjectID = GetOwnerID(ObjectID);
      if (ObjectID IS Self->UID) return TRUE;
   }
   return FALSE;
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
   parasol::Log log(__FUNCTION__);
   escCell *cell;
   DocEdit *edit;
   UBYTE *stream;

   if (!(stream = Self->Stream)) return log.warning(ERR_NoData);
   if ((CellIndex < 0) or (CellIndex >= Self->StreamLen)) return log.warning(ERR_OutOfRange);

   log.branch("Cell Index: %d, Cursor Index: %d", CellIndex, CursorIndex);

   // Check the validity of the index

   if ((stream[CellIndex] != CTRL_CODE) or (ESCAPE_CODE(stream, CellIndex) != ESC_CELL)) {
      return log.warning(ERR_Failed);
   }

   cell = escape_data<escCell>(stream, CellIndex);
   if (CursorIndex <= 0) { // Go to the start of the cell content
      CursorIndex = CellIndex;
      NEXT_CHAR(stream, CursorIndex);
   }

   // Skip any non-content control codes - it's always best to place the cursor ahead of things like
   // font styles, paragraph formatting etc.

   while (CursorIndex < Self->StreamLen) {
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

   if (!(edit = find_editdef(Self, cell->EditHash))) return log.warning(ERR_Search);

   deactivate_edit(Self, FALSE);

   if (edit->OnChange) { // Calculate a CRC for the cell content
      LONG i = CellIndex;
      while (i < Self->StreamLen) {
         if ((stream[i] IS CTRL_CODE) and (ESCAPE_CODE(stream, i) IS ESC_CELL_END)) {
            auto end = escape_data<escCellEnd>(stream, i);
            if (end->CellID IS cell->CellID) {
               Self->ActiveEditCRC = GenCRC32(0, stream + CellIndex, i - CellIndex);
               break;
            }
         }
         NEXT_CHAR(stream, i);
      }
   }

   Self->ActiveEditCellID = cell->CellID;
   Self->ActiveEditDef = edit;
   Self->CursorIndex  = CursorIndex;
   Self->SelectIndex  = -1;

   log.msg("Activated cell %d, cursor index %d, EditDef: %p, CRC: $%.8x", Self->ActiveEditCellID, Self->CursorIndex, Self->ActiveEditDef, Self->ActiveEditCRC);

   // Set the focus index to the relevant TT_EDIT entry

   LONG tab;
   for (tab=0; tab < Self->TabIndex; tab++) {
      if ((Self->Tabs[tab].Type IS TT_EDIT) and (Self->Tabs[tab].Ref IS cell->CellID)) {
         Self->FocusIndex = tab;
         break;
      }
   }

   resolve_fontx_by_index(Self, Self->CursorIndex, &Self->CursorCharX);

   reset_cursor(Self); // Reset cursor flashing

   // User callbacks

   if (edit->OnEnter) {
      OBJECTPTR script;
      CSTRING function_name, argstring;

      log.msg("Calling onenter callback function.");

      if (!extract_script(Self, ((STRING)edit) + edit->OnEnter, &script, &function_name, &argstring)) {
         ScriptArg args[] = { { "ID", FD_LONG, { .Long = (LONG)edit->NameHash } } };
         scExec(script, function_name, args, ARRAYSIZE(args));
      }
   }

   DRAW_PAGE(Self);
   return ERR_Okay;
}

//********************************************************************************************************************

static void deactivate_edit(extDocument *Self, BYTE Redraw)
{
   parasol::Log log(__FUNCTION__);
   UBYTE *stream;

   if (!(stream = Self->Stream)) return;
   if (!Self->ActiveEditDef) return;

   log.branch("Redraw: %d, CellID: %d", Redraw, Self->ActiveEditCellID);

   if (Self->FlashTimer) {
      UpdateTimer(Self->FlashTimer, 0); // Turn off the timer
      Self->FlashTimer = 0;
   }

   // The edit tag needs to be found so that we can determine if OnExit needs to be called or not.

   DocEdit *edit = Self->ActiveEditDef;
   LONG cell_index = find_cell(Self, Self->ActiveEditCellID, 0);

   Self->ActiveEditCellID = 0;
   Self->ActiveEditDef = NULL;
   Self->CursorIndex = -1;
   Self->SelectIndex = -1;

   if (Redraw) DRAW_PAGE(Self);

   if (cell_index >= 0) {
      if (edit->OnChange) {
         auto cell = escape_data<escCell>(Self->Stream, cell_index);

         // CRC comparison - has the cell content changed?

         LONG i = cell_index;
         while (i < Self->StreamLen) {
            if ((stream[i] IS CTRL_CODE) and (ESCAPE_CODE(stream, i) IS ESC_CELL_END)) {
               auto end = escape_data<escCellEnd>(stream, i);
               if (end->CellID IS cell->CellID) {
                  ULONG crc = GenCRC32(0, stream + cell_index, i - cell_index);
                  if (crc != Self->ActiveEditCRC) {
                     OBJECTPTR script;
                     CSTRING function_name, argstring;

                     log.trace("Change detected in editable cell %d", cell->CellID);

                     if (!extract_script(Self, ((STRING)edit) + edit->OnChange, &script, &function_name, &argstring)) {
                        LONG cell_content = cell_index;
                        NEXT_CHAR(stream, cell_content);

                        ScriptArg args[40] = {
                           { "CellID", FD_LONG, { .Long = (LONG)edit->NameHash } },
                           { "Start", FD_LONG,  { .Long = cell_content } },
                           { "End", FD_LONG,    { .Long = i } }
                        };

                        LONG argindex = 3;
                        if (cell->Args > 0) {
                           CSTRING arg = ((STRING)cell) + cell->Args;
                           for (LONG i=0; (i < cell->Args) and (argindex < ARRAYSIZE(args)); i++) {
                              CSTRING value = arg + StrLength(arg) + 1;

                              if (argindex < ARRAYSIZE(args)) {
                                 args[argindex].Name    = NULL;
                                 args[argindex].Type    = FD_STRING;
                                 args[argindex].Address = (APTR)value;
                                 argindex++;
                              }
                              arg = value + StrLength(value) + 1;
                           }
                        }

                        scExec(script, function_name, args, argindex);
                     }
                  }

                  break;
               }
            }
            NEXT_CHAR(stream, i);
         }
      }

      if (edit->OnExit) {



      }
   }
   else log.warning("Failed to find cell ID %d", Self->ActiveEditCellID);
}

//********************************************************************************************************************
// Clip: The clipping area in absolute coordinates.
// Index: The stream index of the object/table/item that is creating the clip.
// Transparent: If TRUE, wrapping will not be performed around the clip region.

static ERROR add_clip(extDocument *Self, SurfaceClip *Clip, LONG Index, CSTRING Name, BYTE Transparent)
{
   LAYOUT("add_clip()","%d: %dx%d,%dx%d, Transparent: %d", Self->TotalClips, Clip->Left, Clip->Top, Clip->Right, Clip->Bottom, Transparent);

   if (!Self->Clips) {
      Self->MaxClips = CLIP_BLOCK;
      if (AllocMemory(sizeof(DocClip) * Self->MaxClips, MEM_NO_CLEAR, &Self->Clips) != ERR_Okay) return ERR_AllocMemory;
   }
   else if (Self->TotalClips >= Self->MaxClips) {
      DocClip *clip;

      // Extend the size of the clip array if we're out of space.

      if (!AllocMemory(sizeof(DocClip) * (Self->MaxClips + CLIP_BLOCK), MEM_NO_CLEAR, &clip)) {
         CopyMemory(Self->Clips, clip, sizeof(DocClip) * Self->MaxClips);
         FreeResource(Self->Clips);
         Self->Clips = clip;
         Self->MaxClips += CLIP_BLOCK;
      }
      else return ERR_AllocMemory;
   }

   CopyMemory(Clip, &Self->Clips[Self->TotalClips].Clip, sizeof(SurfaceClip));
   Self->Clips[Self->TotalClips].Index = Index;
   Self->Clips[Self->TotalClips].Transparent = Transparent;

   #ifdef DBG_WORDWRAP
      StrCopy(Name, Self->Clips[Self->TotalClips].Name, sizeof(Self->Clips[0].Name));
   #endif

   Self->TotalClips++;

   return ERR_Okay;
}

//********************************************************************************************************************
// Sends motion events for zones that the mouse pointer has departed.

static void check_pointer_exit(extDocument *Self, LONG X, LONG Y)
{
   MouseOver *prev = NULL;
   for (auto scan=Self->MouseOverChain; scan;) {
      if ((X < scan->Left) or (Y < scan->Top) or (X >= scan->Right) or (Y >= scan->Bottom)) {
         // Pointer has left this zone

         CSTRING function_name, argstring;
         OBJECTPTR script;
         if (!extract_script(Self, (STRING)(scan + 1), &script, &function_name, &argstring)) {
            ScriptArg args[] = {
               { "Element", FD_LONG, { .Long = scan->ElementID } },
               { "Status",  FD_LONG, { .Long = 0 } },
               { "Args",    FD_STR,  { .Address = (STRING)argstring } }
            };

            scExec(script, function_name, args, ARRAYSIZE(args));
         }

         auto next = scan->Next;
         FreeResource(scan);
         if (scan IS Self->MouseOverChain) Self->MouseOverChain = next;
         if (prev) prev->Next = next;
         scan = next;
      }
      else {
         prev = scan;
         scan = scan->Next;
      }
   }
}

//********************************************************************************************************************

static void pointer_enter(extDocument *Self, LONG Index, CSTRING Function, LONG Left, LONG Top, LONG Right, LONG Bottom)
{
   parasol::Log log(__FUNCTION__);
   MouseOver *mouseover;
   CSTRING function_name, argstring;

   log.traceBranch("%s, %dx%d to %dx%d", Function, Left, Top, Right, Bottom);

   if (!AllocMemory(sizeof(*mouseover) + StrLength(Function) + 1, MEM_DATA, &mouseover)) {
      mouseover->Left      = Left;
      mouseover->Top       = Top;
      mouseover->Right     = Right;
      mouseover->Bottom    = Bottom;
      mouseover->ElementID = ESC_ELEMENTID(Self->Stream + Index);
      StrCopy(Function, (STRING)(mouseover + 1), COPY_ALL);

      // Insert at the start of the chain

      mouseover->Next      = Self->MouseOverChain;
      Self->MouseOverChain = mouseover;

      // Send feedback

      OBJECTPTR script;
      if (!extract_script(Self, Function, &script, &function_name, &argstring)) {
         ScriptArg args[] = {
            { "Element", FD_LONG, { .Long = mouseover->ElementID } },
            { "Status",  FD_LONG, { .Long = 1 } },
            { "Args",    FD_STR,  { .Address = (STRING)argstring } }
         };

         scExec(script, function_name, args, ARRAYSIZE(args));
      }
   }
}

//********************************************************************************************************************

static void check_mouse_click(extDocument *Self, DOUBLE X, DOUBLE Y)
{
   parasol::Log log(__FUNCTION__);
   LONG segment, bytepos;

   Self->ClickX = X;
   Self->ClickY = Y;
   Self->ClickSegment = Self->MouseOverSegment;

   segment = Self->MouseOverSegment;

   if (segment IS -1) {
      // The mouse is not positioned over a segment.  Check if the mosue is positioned within
      // an editing cell.  If it is, we need to find the segment nearest to the mouse pointer
      // and position the cursor at the end of that segment.

      LONG i, sortseg;

      log.warning("%d", Self->ECIndex);

      for (i=0; i < Self->ECIndex; i++) {
         if ((X >= Self->EditCells[i].X) and (X < Self->EditCells[i].X + Self->EditCells[i].Width) and
             (Y >= Self->EditCells[i].Y) and (Y < Self->EditCells[i].Y + Self->EditCells[i].Height)) {
            break;
         }
      }

      if (i < Self->ECIndex) {
         // Mouse is within an editable segment.  Find the start and ending indexes of the editable area

         LONG cell_start, cell_end, last_segment;

         cell_start = find_cell(Self, Self->EditCells[i].CellID, 0);
         cell_end = cell_start;
         while (Self->Stream[cell_end]) {
            if (Self->Stream[cell_end] IS CTRL_CODE) {
               if (ESCAPE_CODE(Self->Stream, cell_end) IS ESC_CELL_END) {
                  auto end = escape_data<escCellEnd>(Self->Stream, cell_end);
                  if (end->CellID IS Self->EditCells[i].CellID) break;
               }
            }

            NEXT_CHAR(Self->Stream, cell_end);
         }

         if (!Self->Stream[cell_end]) return; // No matching cell end - document stream is corrupt

         log.warning("Analysing cell area %d - %d", cell_start, cell_end);

         last_segment = -1;
         for (sortseg=0; sortseg < Self->SortCount; sortseg++) {
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
      if (!resolve_font_pos(Self, segment, X, &Self->CursorCharX, &bytepos)) {
         if (Self->CursorIndex != -1) deselect_text(Self); // A click results in the deselection of existing text

         if (!Self->Segments[segment].Edit) deactivate_edit(Self, TRUE);

         // Set the new cursor information

         Self->CursorIndex = Self->Segments[segment].Index + bytepos;
         Self->SelectIndex = -1; //Self->Segments[segment].Index + bytepos; // SelectIndex is for text selections where the user holds the LMB and drags the mouse
         Self->SelectCharX = Self->CursorCharX;

         log.msg("User clicked on point %.2fx%.2f in segment %d, cursor index: %d, char x: %d", X, Y, segment, Self->CursorIndex, Self->CursorCharX);

         if (Self->Segments[segment].Edit) {
            // If the segment is editable, we'll have to turn on edit mode so
            // that the cursor flashes.  Work backwards to find the edit cell.

            LONG cellindex = Self->Segments[segment].Index;
            while (cellindex > 0) {
               if ((Self->Stream[cellindex] IS CTRL_CODE) and (ESCAPE_CODE(Self->Stream, cellindex) IS ESC_CELL)) {
                  auto cell = escape_data<escCell>(Self->Stream, cellindex);
                  if (cell->EditHash) {
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
         deactivate_edit(Self, TRUE);
      }
   }
}

//********************************************************************************************************************

static void check_mouse_release(extDocument *Self, DOUBLE X, DOUBLE Y)
{
   if ((std::abs(X - Self->ClickX) > 3) or (std::abs(Y - Self->ClickY) > 3)) {
      parasol::Log log(__FUNCTION__);
      log.trace("User click cancelled due to mouse shift.");
      return;
   }

   if (Self->LinkIndex != -1) exec_link(Self, Self->LinkIndex);
}

//********************************************************************************************************************

static void check_mouse_pos(extDocument *Self, DOUBLE X, DOUBLE Y)
{
   DocEdit *edit;
   UBYTE found;

   Self->MouseOverSegment = -1;
   Self->PointerX = X;
   Self->PointerY = Y;

   check_pointer_exit(Self, X, Y); // For function callbacks

   if (Self->MouseOver) {
      LONG row;
      for (row=0; (row < Self->SortCount) and (Y < Self->SortSegments[row].Y); row++);

      for (; row < Self->SortCount; row++) {
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
         LONG bytepos, i, cursor_x, cursor_index;
         if (!resolve_font_pos(Self, Self->MouseOverSegment, X, &cursor_x, &bytepos)) {
            cursor_index = Self->Segments[Self->MouseOverSegment].Index + bytepos;

            if ((edit = Self->ActiveEditDef)) {
               // For select-dragging, we must check that the selection is within the bounds of the editing area.

               if ((i = find_cell(Self, Self->ActiveEditCellID, 0)) >= 0) {
                  NEXT_CHAR(Self->Stream, i);
                  if (cursor_index < i) {
                     // If the cursor index precedes the start of the editing area, reset it

                     if (!resolve_fontx_by_index(Self, i, &cursor_x)) {
                        cursor_index = i;
                     }
                  }
                  else {
                     // If the cursor index exceeds the end of the editing area, reset it

                     while (i < Self->StreamLen) {
                        if ((Self->Stream[i] IS CTRL_CODE) and (ESCAPE_CODE(Self->Stream, i) IS ESC_CELL_END)) {
                           auto cell_end = escape_data<escCellEnd>(Self->Stream, i);
                           if (cell_end->CellID IS Self->ActiveEditCellID) {
                              LONG seg;
                              if ((seg = find_segment(Self, i, FALSE)) > 0) {
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
               else deactivate_edit(Self, FALSE);
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

   found = FALSE;
   if ((Self->MouseOver) and (!Self->LMB)) {
      for (LONG i=Self->TotalLinks-1; i >= 0; i--) { // Search from front to back
         if ((X >= Self->Links[i].X) and (Y >= Self->Links[i].Y) and
             (X < Self->Links[i].X + Self->Links[i].Width) and
             (Y < Self->Links[i].Y + Self->Links[i].Height)) {
            // The mouse pointer is inside a link

            if (Self->LinkIndex IS -1) {
               gfxSetCursor(0, CRF_BUFFER, PTR_HAND, 0, Self->UID);
               Self->CursorSet = TRUE;
            }

            if ((Self->Links[i].EscapeCode IS ESC_LINK) and (Self->Links[i].Link->PointerMotion)) {
               pointer_enter(Self, (MAXINT)Self->Links[i].Link - ESC_LEN_START - (MAXINT)Self->Stream,
                  ((CSTRING)Self->Links[i].Link) + Self->Links[i].Link->PointerMotion,
                  Self->Links[i].X, Self->Links[i].Y, Self->Links[i].X + Self->Links[i].Width, Self->Links[i].Y + Self->Links[i].Height);
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
         gfxSetCursor(0, CRF_BUFFER, PTR_TEXT, 0, Self->UID);
         Self->CursorSet = TRUE;
      }
      return;
   }

   for (LONG i=0; i < Self->ECIndex; i++) {
      if ((X >= Self->EditCells[i].X) and (X < Self->EditCells[i].X + Self->EditCells[i].Width) and
          (Y >= Self->EditCells[i].Y) and (Y < Self->EditCells[i].Y + Self->EditCells[i].Height)) {
         gfxSetCursor(0, CRF_BUFFER, PTR_TEXT, 0, Self->UID);
         Self->CursorSet = TRUE;
         return;
      }
   }

   // Reset the cursor to the default

   if (Self->CursorSet) {
      Self->CursorSet = FALSE;
      gfxRestoreCursor(PTR_DEFAULT, Self->UID);
   }
}

//********************************************************************************************************************

static ERROR resolve_font_pos(extDocument *Self, LONG Segment, LONG X, LONG *CharX, LONG *BytePos)
{
   parasol::Log log(__FUNCTION__);
   LONG i, index;

   if ((Segment >= 0) and (Segment < Self->SegCount)) {
      // Find the font that represents the start of the stream

      auto str = Self->Stream;
      escFont *style = NULL;

      // First, go forwards to try and find the correct font

      LONG fi = Self->Segments[Segment].Index;
      while (fi < Self->Segments[Segment].Stop) {
         if ((str[fi] IS CTRL_CODE) and (ESCAPE_CODE(str, fi) IS ESC_FONT)) {
            style = escape_data<escFont>(str, fi);
         }
         else if (str[fi] != CTRL_CODE) break;
         NEXT_CHAR(str, fi);
      }

      // Didn't work?  Try going backwards

      if (!style) {
         fi = Self->Segments[Segment].Index;
         while (fi >= 0) {
            if ((str[fi] IS CTRL_CODE) and (ESCAPE_CODE(str, fi) IS ESC_FONT)) {
               style = escape_data<escFont>(str, fi);
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

      UBYTE buffer[Self->Segments[Segment].Stop - Self->Segments[Segment].Index + 1];
      LONG pos = 0;
      i = Self->Segments[Segment].Index;
      while (i < Self->Segments[Segment].Stop) {
         if (Self->Stream[i] != CTRL_CODE) buffer[pos++] = Self->Stream[i++];
         else i += ESCAPE_LEN(Self->Stream+i);
      }
      buffer[pos] = 0;

      if (!fntConvertCoords(font, (CSTRING)buffer, X - Self->Segments[Segment].X, 0, NULL, NULL, NULL, &index, CharX)) {
         // Convert the character position to the correct byte position - i.e. take control codes into account.

         for (i=Self->Segments[Segment].Index; (index > 0); ) {
            if (Self->Stream[i] IS CTRL_CODE) i += ESCAPE_LEN(Self->Stream+i);
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
   parasol::Log log("resolve_fontx");
   LONG segment;

   log.branch("Index: %d", Index);

   escFont *style = NULL;

   // First, go forwards to try and find the correct font

   LONG fi = Index;
   while ((Self->Stream[fi] != CTRL_CODE) and (fi < Self->StreamLen)) {
      if ((Self->Stream[fi] IS CTRL_CODE) and (ESCAPE_CODE(Self->Stream, fi) IS ESC_FONT)) {
         style = escape_data<escFont>(Self->Stream, fi);
      }
      else if (Self->Stream[fi] != CTRL_CODE) break;
      NEXT_CHAR(Self->Stream, fi);
   }

   // Didn't work?  Try going backwards

   if (!style) {
      fi = Index;
      while (fi >= 0) {
         if ((Self->Stream[fi] IS CTRL_CODE) and (ESCAPE_CODE(Self->Stream, fi) IS ESC_FONT)) {
            style = escape_data<escFont>(Self->Stream, fi);
            break;
         }
         PREV_CHAR(Self->Stream, fi);
      }
   }

   auto font = lookup_font(style ? style->Index : 0, "check_mouse_click");

   if (!font) return log.warning(ERR_Search);

   // Find the segment associated with this index.  This is so that we can derive an X coordinate for the character
   // string.

   if ((segment = find_segment(Self, Index, TRUE)) >= 0) {
      // Normalise the segment into a plain character string

      UBYTE buffer[(Self->Segments[segment].Stop+1) - Self->Segments[segment].Index + 1];
      LONG pos = 0;
      LONG i = Self->Segments[segment].Index;
      while ((i <= Self->Segments[segment].Stop) and (i < Index)) {
         if (Self->Stream[i] != CTRL_CODE) buffer[pos++] = Self->Stream[i++];
         else i += ESCAPE_LEN(Self->Stream+i);
      }
      buffer[pos] = 0;

      if (pos > 0) *CharX = fntStringWidth(font, (CSTRING)buffer, -1);
      else *CharX = 0;

      return ERR_Okay;
   }
   else log.warning("Failed to find a segment for index %d.", Index);

   return ERR_Search;
}

//********************************************************************************************************************

static LONG find_segment(extDocument *Self, LONG Index, LONG InclusiveStop)
{
   LONG segment;

   if (InclusiveStop) {
      for (segment=0; segment < Self->SegCount; segment++) {
         if ((Index >= Self->Segments[segment].Index) and (Index <= Self->Segments[segment].Stop)) {
            if ((Index IS Self->Segments[segment].Stop) and (Self->Stream[Index-1] IS '\n'));
            else return segment;
         }
      }
   }
   else {
      for (segment=0; segment < Self->SegCount; segment++) {
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
   parasol::Log log(__FUNCTION__);

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
   LONG top = 0;
   LONG bottom = Self->SegCount-1;
   LONG last = -1;
   while ((mid = (bottom-top)>>1) != last) {
      last = mid;
      mid += top;
      if (start >= Self->Segments[mid].Index) top = mid;
      else if (end < Self->Segments[mid].Stop) bottom = mid;
      else break;
   }

   LONG startseg = mid; // Start is now set to the segment rather than stream index

   // Find the end

   top = startseg;
   bottom = Self->SegCount-1;
   last = -1;
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

static void split_command(std::string &Command, std::string &Args)
{
   if (Command[0] IS '"') {
      Command.erase(0, 1);
      LONG i = 0;
      while ((Command[i]) and (Command[i] != '"')) i++;
      if (Command[i] IS '"') {
         LONG a = i + 1;
         while ((Command[a]) and (Command[a] <= 0x20)) a++;
         Args = Command.substr(a);

         Command.erase(i);
      }
   }
   else {
      LONG i = 0;
      while (Command[i] > 0x20) i++;

      if (Command[i]) {
         LONG a = i;
         while ((Command[a]) and (Command[a] <= 0x20)) a++;
         Args = Command.substr(a);

         Command.resize(i);
      }
   }
}

//********************************************************************************************************************

static LONG find_tabfocus(extDocument *Self, UBYTE Type, LONG Reference)
{
   for (LONG i=0; i < Self->TabIndex; i++) {
      if ((Self->Tabs[i].Type IS Type) and (Reference IS Self->Tabs[i].Ref)) return i;
   }
   return -1;
}

//********************************************************************************************************************
// This function is used in tags.c by the link and object insertion code.

static LONG add_tabfocus(extDocument *Self, UBYTE Type, LONG Reference)
{
   parasol::Log log(__FUNCTION__);

   //log.function("Type: %d, Ref: %d", Type, Reference);

   // Allocate tab management array

   #define TAB_BLOCK 50

   if (!Self->Tabs) {
      Self->MaxTabs = TAB_BLOCK;
      if (!AllocMemory(sizeof(Self->Tabs[0]) * Self->MaxTabs, MEM_DATA|MEM_NO_CLEAR, &Self->Tabs)) {
         Self->TabIndex = 0;
      }
      else return -1;
   }
   else if (Self->TabIndex >= Self->MaxTabs) {
      if (!ReallocMemory(Self->Tabs, sizeof(Self->Tabs[0]) * (Self->MaxTabs + TAB_BLOCK), &Self->Tabs, NULL)) {
         Self->MaxTabs += TAB_BLOCK;
      }
      else return -1;
   }

   // For TT_LINK types, check that the link isn't already registered

   if (Type IS TT_LINK) {
      LONG i;
      for (i=0; i < Self->TabIndex; i++) {
         if ((Self->Tabs[i].Type IS TT_LINK) and (Self->Tabs[i].Ref IS Reference)) {
            return i;
         }
      }
   }

   // Add the tab entry

   LONG index = Self->TabIndex;
   Self->Tabs[index].Type = Type;
   Self->Tabs[index].Ref  = Reference;
   Self->Tabs[index].XRef = 0;
   Self->Tabs[index].Active = Self->Invisible ^ 1;

   if (Type IS TT_OBJECT) {
      OBJECTPTR object;
      OBJECTID regionid;

      // Find out if the object has a surface and if so, place it in the XRef field.

      if (GetClassID(Reference) != ID_SURFACE) {
         if (!AccessObjectID(Reference, 3000, &object)) {
            regionid = 0;
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

            Self->Tabs[index].XRef = regionid;

            ReleaseObject(object);
         }
      }
      else Self->Tabs[index].XRef = Reference;
   }

   Self->TabIndex++;
   return index;
}

//********************************************************************************************************************
// Changes the focus to an object or link in the document.  The new index is stored in the FocusIndex field.  If the
// Index is set to -1, set_focus() will focus on the first element, but only if it is an object.

static void set_focus(extDocument *Self, LONG Index, CSTRING Caller)
{
   parasol::Log log(__FUNCTION__);

   if ((!Self->Tabs) or (Self->TabIndex < 1)) {
      log.trace("No tab markers in document.");
      return;
   }

   if ((Index < -1) or (Index >= Self->TabIndex)) {
      log.traceWarning("Index %d out of bounds.", Index);
      return;
   }

   log.branch("Index: %d/%d, Type: %d, Ref: %d, HaveFocus: %d, Caller: %s", Index, Self->TabIndex, Index != -1 ? Self->Tabs[Index].Type : -1, Index != -1 ? Self->Tabs[Index].Ref : -1, Self->HasFocus, Caller);

   if (Self->ActiveEditDef) deactivate_edit(Self, TRUE);

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
      if ((cell_index = find_cell(Self, Self->Tabs[Self->FocusIndex].Ref, 0)) >= 0) {
         activate_edit(Self, cell_index, -1);
      }
   }
   else if (Self->Tabs[Index].Type IS TT_OBJECT) {
      if (Self->HasFocus) {
         CLASSID class_id = GetClassID(Self->Tabs[Index].Ref);
         OBJECTPTR input;
         if (class_id IS ID_VECTORTEXT) {
            if (!AccessObjectID(Self->Tabs[Index].Ref, 1000, &input)) {
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
         LONG i;
         for (i=0; i < Self->TotalLinks; i++) {
            if ((Self->Links[i].EscapeCode IS ESC_LINK) and (Self->Links[i].Link->ID IS Self->Tabs[Index].Ref)) break;
         }

         if (i < Self->TotalLinks) {
            LONG link_x = Self->Links[i].X;
            LONG link_y = Self->Links[i].Y;
            LONG link_bottom = link_y + Self->Links[i].Height;
            LONG link_right = link_x + Self->Links[i].Width;

            for (++i; i < Self->TotalLinks; i++) {
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
   parasol::Log log(__FUNCTION__);

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
      acScrollToPoint(Self, view_x, view_y, 0, STP_X|STP_Y);
      return TRUE;
   }
   else return FALSE;
}

//********************************************************************************************************************

static void advance_tabfocus(extDocument *Self, BYTE Direction)
{
   parasol::Log log(__FUNCTION__);
   LONG i;

   if ((!Self->Tabs) or (Self->TabIndex < 1)) return;

   // Check that the FocusIndex is accurate (it may have changed if the user clicked on a gadget).

   OBJECTID currentfocus = gfxGetUserFocus();
   for (i=0; i < Self->TabIndex; i++) {
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

   i = Self->TabIndex; // This while loop is designed to stop if no tab indexes are found to be active
   while (i > 0) {
      i--;

      if (Direction IS -1) {
         Self->FocusIndex--;
         if (Self->FocusIndex < 0) Self->FocusIndex = Self->TabIndex - 1;
      }
      else {
         Self->FocusIndex++;
         if (Self->FocusIndex >= Self->TabIndex) Self->FocusIndex = 0;
      }

      if (!Self->Tabs[Self->FocusIndex].Active) continue;

      if ((Self->Tabs[Self->FocusIndex].Type IS TT_OBJECT) and (Self->Tabs[Self->FocusIndex].XRef)) {
         SURFACEINFO *info;
         if (!gfxGetSurfaceInfo(Self->Tabs[Self->FocusIndex].XRef, &info)) {
            if (info->Flags & RNF_DISABLED) continue;
         }
      }
      break;
   }

   if (i >= 0) set_focus(Self, Self->FocusIndex, "adv_tabfocus");
}

//********************************************************************************************************************
// scheme://domain.com/path?param1=value&param2=value#fragment:bookmark

static void process_parameters(extDocument *Self, CSTRING String)
{
   parasol::Log log(__FUNCTION__);
   LONG i, setsize;
   STRING set;

   log.branch("%s", String);

   if (Self->Params) { FreeResource(Self->Params); Self->Params = NULL; }

   BYTE pagename_processed = FALSE;
   while (*String) {
      if ((*String IS '#') and (!pagename_processed)) {
         // Reference is '#fragment:bookmark' where 'fragment' refers to a page in the loaded XML file and 'bookmark' is an optional bookmark reference within that page.

         pagename_processed = TRUE;

         if (Self->PageName) { FreeResource(Self->PageName); Self->PageName = NULL; }
         if (Self->Bookmark) { FreeResource(Self->Bookmark); Self->Bookmark = NULL; }

         if (!String[1]) break;

         Self->PageName = StrClone(String + 1);

         for (LONG i=0; Self->PageName[i]; i++) {
            if (Self->PageName[i] IS ':') { // Check for bookmark separator
               Self->PageName[i++] = 0;

               if (Self->PageName[i]) {
                  Self->Bookmark = StrClone(Self->PageName + i);

                  for (LONG i=0; Self->Bookmark[i]; i++) { // Remove parameter separator
                     if (Self->Bookmark[i] IS '?') {
                        Self->Bookmark[i] = 0;
                        break;
                     }
                  }
               }

               break;
            }
            else if (Self->PageName[i] IS '?') { // Stop if parameter separator encountered
               Self->PageName[i] = 0;
               break;
            }
         }

         break;
      }
      else if (*String IS '?') {
         // Arguments follow, separated by & characters for separation

         String++;
         setsize = 0xffff;
         if (!AllocMemory(setsize, MEM_STRING|MEM_NO_CLEAR, &set)) { // Allocation is temporary
            char arg[80];

            while (*String) {
               // Extract the parameter name into arg

               for (i=0; (*String) and (*String != '#') and (*String != '&') and (*String != ';') and (*String != '=') and ((size_t)i < sizeof(arg)-1);) {
                  i += uri_char(&String, arg+i, sizeof(arg)-i);
               }
               arg[i] = 0;

               // Extract the parameter value into set

               if (*String IS '=') {
                  String++;
                  for (i=0; (*String) and (*String != '#') and (*String != '&') and (*String != ';') and (i < setsize-1);) {
                     i += uri_char(&String, set+i, setsize-i);
                  }
                  set[i] = 0;
               }
               else {
                  set[0] = '1';
                  set[1] = 0;
               }

               // Please note that it is okay to set zero-length parameters

               VarSetString(Self->Params, arg, set);

               while ((*String) and (*String != '#') and (*String != '&') and (*String != ';')) String++;
               if ((*String != '&') and (*String != ';')) break;
               String++;
            }

            FreeResource(set);
         }
      }
      else String++;
   }

   log.msg("Reset page name to '%s', bookmark '%s'", Self->PageName, Self->Bookmark);
}

//********************************************************************************************************************
// Obsoletion of the old scrollbar code means that we should be adjusting page size only and let the scrollbars
// automatically adjust in the background.

static void calc_scroll(extDocument *Self) __attribute__((unused));
static void calc_scroll(extDocument *Self)
{
   parasol::Log log(__FUNCTION__);

   log.traceBranch("PageHeight: %d/%d, PageWidth: %d/%d, XPos: %d, YPos: %d", Self->PageHeight, Self->AreaHeight, Self->CalcWidth, Self->AreaWidth, Self->XPosition, Self->YPosition);
}

/*********************************************************************************************************************
** Resolves function references.
*/

static ERROR extract_script(extDocument *Self, CSTRING Link, OBJECTPTR *Script, CSTRING *Function, CSTRING *Args)
{
   parasol::Log log(__FUNCTION__);
   LONG len, pos;
   STRING scriptref;

   if ((!Link) or (!*Link)) return ERR_NoData;

   LONG dot = -1;
   LONG bracket = -1;
   for (len=0; Link[len]; len++) {
      if ((Link[len] IS '.') and (dot IS -1)) dot = len;
      if ((Link[len] IS '(') and (bracket IS -1)) bracket = len;
   }
   len += 2;

   if ((bracket != -1) and (bracket < dot)) {
      log.warning("Malformed function reference: %s", Link);
      return ERR_InvalidData;
   }

   if (len > exsbuffer_size) {
      if (exsbuffer) { FreeResource(exsbuffer); exsbuffer = NULL; }
      if (AllocMemory(len, MEM_STRING|MEM_UNTRACKED, &exsbuffer) != ERR_Okay) return ERR_AllocMemory;
      exsbuffer_size = len;
   }

   // Copy script name

   if (dot != -1) {
      CopyMemory(Link, exsbuffer, dot);
      pos = dot;
      scriptref = exsbuffer;
      exsbuffer[pos++] = 0;
   }
   else {
      scriptref = NULL;
      pos = 0;
   }

   // Copy function name

   if (dot != -1) dot++;
   else dot = 0;

   *Function = exsbuffer + pos;
   if (bracket != -1) {
      CopyMemory(Link+dot, exsbuffer+pos, bracket-dot);
      pos += bracket - dot;
      exsbuffer[pos++] = 0;

      if (Args) { // Copy args
         bracket++;
         while ((len > bracket) and (Link[len] != ')')) len--;
         if (Link[len] IS ')') {
            *Args = exsbuffer + pos;
            CopyMemory(Link+bracket, exsbuffer+pos, len-bracket);
            pos += len - bracket;
            exsbuffer[pos++] = 0;
         }
         else log.warning("Malformed function args: %s", Link);
      }
   }
   else pos += StrCopy(Link+dot, exsbuffer+pos, COPY_ALL);

   #ifdef DEBUG
   if (pos > len) {
      log.warning("Buffer overflow (%d > %d) translating: %s", pos, len, Link);
   }
   #endif

   // Find the script that has been referenced

   if (Script) {
      if (scriptref) {
         OBJECTID id;
         if (!FindObject(scriptref, ID_SCRIPT, 0, &id)) {
            // Security checks

            *Script = GetObjectPtr(id);

            if ((Script[0]->OwnerID != Self->UID) and (!(Self->Flags & DCF_UNRESTRICTED))) {
               log.warning("Script '%s' does not belong to this document.  Action ignored due to security restrictions.", scriptref);
               return ERR_NoPermission;
            }
         }
         else {
            log.warning("Unable to find '%s'", scriptref);
            return ERR_Search;
         }
      }
      else if (!(*Script = Self->DefaultScript)) {
         if (!(*Script = Self->UserDefaultScript)) {
            log.warning("Cannot call function '%s', no default script in document.", Link);
            return ERR_Search;
         }
      }
   }

   return ERR_Okay;
}

//********************************************************************************************************************

static void exec_link(extDocument *Self, LONG Index)
{
   parasol::Log log(__FUNCTION__);

   if ((Index IS -1) or (Index >= Self->TotalLinks)) {
      log.trace("No links in document or %d >= %d.", Index, Self->TotalLinks);
      return;
   }

   log.branch("Link: %d", Index);

   Self->Processing++;

   if ((Self->Links[Index].EscapeCode IS ESC_LINK) and (Self->EventMask & DEF_LINK_ACTIVATED)) {
      deLinkActivated params;
      if ((params.Parameters = VarNew(0, 0L))) {
         escLink *link = Self->Links[Index].Link;
         CSTRING strlink = (CSTRING)(link + 1);

         if (link->Type IS LINK_FUNCTION) {
            CSTRING function_name;
            if (!extract_script(Self, strlink, NULL, &function_name, NULL)) {
               VarSetString(params.Parameters, "onclick", function_name);
            }
         }
         else if (link->Type IS LINK_HREF) {
            VarSetString(params.Parameters, "href", strlink);
         }

         if (link->Args > 0) {
            CSTRING arg = strlink + StrLength(strlink) + 1;
            for (LONG i=0; i < link->Args; i++) {
               CSTRING value = arg + StrLength(arg) + 1;
               VarSetString(params.Parameters, arg, value);
               arg = value + StrLength(value) + 1;
            }
         }

         ERROR result = report_event(Self, DEF_LINK_ACTIVATED, &params, "deLinkActivated:Parameters");
         FreeResource(params.Parameters);
         params.Parameters = NULL;
         if (result IS ERR_Skip) goto end;
      }
   }

   if (Self->Links[Index].EscapeCode IS ESC_LINK) {
      OBJECTPTR script;
      CSTRING function_name;
      CLASSID class_id, subclass_id;

      escLink *link = Self->Links[Index].Link;
      CSTRING strlink = (STRING)(link + 1);
      if (link->Type IS LINK_FUNCTION) { // Function is in the format 'function()' or 'script.function()'
         if (!extract_script(Self, strlink, &script, &function_name, NULL)) {
            ScriptArg args[40];

            LONG index = 0;
            if (link->Args > 0) {
               CSTRING arg = strlink + StrLength(strlink) + 1;
               for (LONG i=0; (i < link->Args) and (index < ARRAYSIZE(args)); i++) {
                  CSTRING value = arg + StrLength(arg) + 1;

                  if (arg[0] IS '_') { // Global variable setting
                     acSetVar(script, arg+1, value);
                  }
                  else if (index < ARRAYSIZE(args)) {
                     args[index].Name    = NULL;
                     args[index].Type    = FD_STRING;
                     args[index].Address = (APTR)value;
                     index++;
                  }
                  arg = value + StrLength(value) + 1;
               }
            }

            scExec(script, function_name, args, index);
         }
      }
      else if (link->Type IS LINK_HREF) {
         if (strlink[0] IS ':') {
            if (Self->Bookmark) FreeResource(Self->Bookmark);
            Self->Bookmark = StrClone(strlink+1);
            show_bookmark(Self, Self->Bookmark);
         }
         else {
            if ((strlink[0] IS '#') or (strlink[0] IS '?')) {
               log.trace("Switching to page '%s'", strlink);

               if (Self->Path) {
                  LONG end;
                  for (end=0; Self->Path[end]; end++) {
                     if ((Self->Path[end] IS '&') or (Self->Path[end] IS '#') or (Self->Path[end] IS '?')) break;
                  }
                  auto path = std::string(Self->Path, end) + strlink;
                  Self->set(FID_Path, path);
               }
               else Self->set(FID_Path, strlink);

               if (Self->Bookmark) show_bookmark(Self, Self->Bookmark);
            }
            else {
               log.trace("Link is a file reference.");

               std::string lk;

               if (Self->Path) {
                  bool abspath = false; // Is the link an absolute path indicated by a volume name?
                  for (LONG j=0; strlink[j]; j++) {
                     if ((strlink[j] IS '/') or (strlink[j] IS '\\')) break;
                     if (strlink[j] IS ':') { abspath = true; break; }
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

               lk += strlink;

               LONG end;
               for (end=0; lk[end]; end++) {
                  if ((lk[end] IS '?') or (lk[end] IS '#') or (lk[end] IS '&')) break;
               }

               STRING cmd;
               if (!IdentifyFile(lk.substr(0, end).c_str(), "Open", 0, &class_id, &subclass_id, &cmd)) {
                  if (class_id IS ID_DOCUMENT) {
                     Self->set(FID_Path, lk);

                     if (Self->Bookmark) show_bookmark(Self, Self->Bookmark);
                     else log.msg("No bookmark was preset.");
                  }
                  else if (cmd) { // If the link is supported by a program, run that program and pass it the link.
                     std::string scmd(cmd);
                     auto i = scmd.find("[@file]");
                     if (i != std::string::npos) scmd.replace(i, 7, strlink);

                     std::string args;
                     split_command(scmd, args);

                     objTask::create task = { fl::Path(scmd), fl::Args(args) };
                     if (task.ok()) task->activate();
                  }
                  FreeResource(cmd);
               }
               else {
                  auto msg = std::string("It is not possible to follow this link as the type of file is not recognised.  The referenced link is:\n\n") + lk;
                  error_dialog("Action Cancelled", msg.c_str(), 0);
               }
            }
         }
      }
   }
   else if (Self->Links[Index].EscapeCode IS ESC_CELL) {
      OBJECTPTR script;
      CSTRING function_name;

      escCell *cell = Self->Links[Index].Cell;
      CSTRING str = ((CSTRING)cell) + cell->OnClick;

      if (!extract_script(Self, str, &script, &function_name, NULL)) {
         LONG index = 0;
         ScriptArg args[40];
         if (cell->TotalArgs > 0) {
            CSTRING arg = ((CSTRING)cell) + cell->Args;
            for (LONG i=0; (i < cell->Args) and (index < ARRAYSIZE(args)); i++) {
               CSTRING value = arg + StrLength(arg) + 1;

               if (arg[0] IS '_') { // Global variable setting
                  acSetVar(script, arg+1, value);
               }
               else if (index < ARRAYSIZE(args)) {
                  args[index].Name    = NULL;
                  args[index].Type    = FD_STRING;
                  args[index].Address = (APTR)value;
                  index++;
               }
               arg = value + StrLength(value) + 1;
            }
         }

         scExec(script, function_name, args, index);
      }
   }
   else log.trace("Link index does not refer to a supported link type.");

end:
   Self->Processing--;
}

//********************************************************************************************************************
// If an object supports the LayoutStyle field, this function configures the DocStyle structure and then sets the
// LayoutStyle field for that object.
//
// Information about the current font can be pulled from the Font object referenced in the Font field.  Supplementary
// information like the font colour is provided separately.

static void set_object_style(extDocument *Self, OBJECTPTR Object)
{
   DOCSTYLE style;

   style.Version    = VER_DOCSTYLE;
   style.Document   = Self;
   style.Font       = lookup_font(Self->Style.FontStyle.Index, "set_object_style");
   style.FontColour = Self->Style.FontStyle.Colour;

   if (Self->Style.FontStyle.Options & FSO_UNDERLINE) style.FontUnderline = Self->Style.FontStyle.Colour;
   else style.FontUnderline.Alpha = 0;

   style.StyleFlags = Self->Style.FontStyle.Options;

   Object->set(FID_LayoutStyle, &style);
}

//********************************************************************************************************************

static void show_bookmark(extDocument *Self, CSTRING Bookmark)
{
   parasol::Log log(__FUNCTION__);

   log.branch("%s", Bookmark);

   // Find the indexes for the bookmark name

   LONG start, end;
   if (!docFindIndex(Self, Bookmark, &start, &end)) {
      // Get the vertical position of the index and scroll to it

      auto esc_index = escape_data<escIndex>(Self->Stream, start);
      acScrollToPoint(Self,  0, esc_index->Y - 4, 0, MTF_Y);
   }
   else log.warning("Failed to find bookmark '%s'", Bookmark);
}

//********************************************************************************************************************

static void key_event(extDocument *Self, evKey *Event, LONG Size)
{
   if (Event->Qualifiers & KQ_PRESSED) {
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
   parasol::Log log(__FUNCTION__);

   log.function("");

   Self->CursorState = 1;
   if (Self->FlashTimer) UpdateTimer(Self->FlashTimer, 0.5);
   else {
      auto call = make_function_stdc(flash_cursor);
      SubscribeTimer(0.5, &call, (APTR)&Self->FlashTimer);
   }
}

//********************************************************************************************************************

static ERROR report_event(extDocument *Self, LARGE Event, APTR EventData, CSTRING StructName)
{
   parasol::Log log(__FUNCTION__);
   ERROR result = ERR_Okay;

   if (Event & Self->EventMask) {
      log.branch("Reporting event $%.8x", (LONG)Event);

      if (Self->EventCallback.Type IS CALL_STDC) {
         auto routine = (ERROR (*)(extDocument *, LARGE, APTR))Self->EventCallback.StdC.Routine;
         parasol::SwitchContext context(Self->EventCallback.StdC.Context);
         result = routine(Self, Event, EventData);
      }
      else if (Self->EventCallback.Type IS CALL_SCRIPT) {
         OBJECTPTR script;
         if ((script = Self->EventCallback.Script.Script)) {
            ScriptArg args[3];
            args[0].Name    = "Document";
            args[0].Type    = FD_OBJECTPTR;
            args[0].Address = Self;

            args[1].Name  = "EventMask";
            args[1].Type  = FD_LARGE;
            args[1].Large = Event;

            LONG argsize;
            if ((EventData) and (StructName)) {
               args[2].Name    = StructName;
               args[2].Type    = FD_STRUCT;
               args[2].Address = EventData;
               argsize = 3;
            }
            else argsize = 2;

            scCallback(script, Self->EventCallback.Script.ProcedureID, args, argsize, &result);
         }
      }
   }
   else log.trace("No subscriber for event $%.8x", (LONG)Event);

   return result;
}
