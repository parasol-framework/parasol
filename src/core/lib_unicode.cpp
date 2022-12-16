/*****************************************************************************
-CATEGORY-
Name: Unicode
-END-
*****************************************************************************/

/*****************************************************************************

-FUNCTION-
UTF8CharOffset: Retrieves the byte position of a character in a UTF-8 string.

Determines the true byte position of a character within a UTF-8 string, where Offset is a
column number.

-INPUT-
cstr String: A null-terminated UTF-8 string.
int Offset: The character number to translate to a byte offset.

-RESULT-
int: Returns the byte offset of the character.

*****************************************************************************/

LONG UTF8CharOffset(CSTRING String, LONG Index)
{
   if (!String) return 0;

   LONG offset = 0;
   while ((String[offset]) and (Index > 0)) {
      for (++offset; ((String[offset] & 0xc0) IS 0x80); offset++);
      Index--;
   }
   return offset;
}

/*****************************************************************************

-FUNCTION-
UTF8CharLength: Returns the number of bytes used to define a single UTF-8 character.

This function will return the total number of bytes used to create a single UTF-8 character.  A pointer to the start of
the character string that is to be analysed is required.

-INPUT-
cstr String: Pointer to a UTF-8 string.

-RESULT-
int: Returns the number of bytes used to create the UTF-8 character referred to by the String argument.

*****************************************************************************/

LONG UTF8CharLength(CSTRING String)
{
   if ((!String) or (!*String)) return 0;

   LONG total;
   for (total=1; ((String[total] & 0xc0) IS 0x80); total++);
   return total;
}

/*****************************************************************************

-FUNCTION-
UTF8Copy: Copies the characters of one string to another (UTF8).

This function copies part of one string over to another.  If the Chars value is `COPY_ALL` then this function will
copy the entire Src string to Dest.  If this function encounters the end of the Src string (null byte) while
copying, it will terminate the output string at the same point and return.

Please note that the Dest string will <i>always</i> be null-terminated by this function regardless of the Chars valuae.
For example, if `123` is copied into the middle of string `ABCDEFGHI` then the result would be `ABC123` and the `GHI`
part of the string would be lost.

-INPUT-
cstr Src:  Pointer to the source string.
str Dest:  Pointer to the destination buffer.
int Chars: The maximum number of UTF8 characters to copy.  Can be set to COPY_ALL to copy up to the null terminator of the Src.
int Size:  Byte size of the destination buffer.

-RESULT-
int: Returns the total amount of <i>bytes</i> that were copied, not including the null byte at the end.

*****************************************************************************/

LONG UTF8Copy(CSTRING String, STRING Dest, LONG Chars, LONG Size)
{
   if (!Dest) return 0;
   if (Size < 1) return 0;
   if ((Chars < 0) or (!String)) {
      Dest[0] = 0;
      return 0;
   }

   UBYTE copy;
   LONG i = 0;
   while (Chars > 0) {
      // Determine the number of bytes to copy for this character

      if (!*String) break;
      else if ((UBYTE)(*String) < 128) copy = 1;
      else if ((*String & 0xe0) IS 0xc0) copy = 2;
      else if ((*String & 0xf0) IS 0xe0) copy = 3;
      else if ((*String & 0xf8) IS 0xf0) copy = 4;
      else if ((*String & 0xfc) IS 0xf8) copy = 5;
      else if ((*String & 0xfc) IS 0xfc) copy = 6;
      else copy = 1;

      // Check if there's enough room to accept the number of bytes and the null byte

      if (i + copy + 1 >= Size) break;

      // Do the copy

      Dest[i++] = *String++; // First character
      for (UBYTE j=1; j < copy; j++) { // Subsequent characters are subject to UTF8 validity
         if ((*String & 0xc0) != 0x80) break;
         Dest[i++] = *String++;
      }

      Chars--;
   }

   Dest[i] = 0;
   return i;
}

/*****************************************************************************

-FUNCTION-
UTF8Length: Returns the total number of characters in a UTF-8 string.

This function will return the total number of decoded unicode characters in a UTF-8 string.

For the total number of bytes in a string, use ~StrLength() instead.

-INPUT-
cstr String: Pointer to a UTF-8 string.

-RESULT-
int: Returns the total number of characters used in the supplied UTF-8 string.

*****************************************************************************/

LONG UTF8Length(CSTRING String)
{
   if (!String) return 0;

   LONG total;
   for (total=0; *String; total++) {
      for (++String; ((*String & 0xc0) IS 0x80); String++);
   }
   return total;
}

/*****************************************************************************

-FUNCTION-
UTF8OffsetToChar: Converts a byte offset into a character position.

This function will convert a byte position in a UTF-8 string to its character column number.

-INPUT-
cstr String: A null-terminated UTF-8 string.
int Offset: The byte offset that you need a character number for.

-RESULT-
int: Returns the number of the character at the given byte position.

*****************************************************************************/

LONG UTF8OffsetToChar(CSTRING String, LONG Offset)
{
   if (!String) return 0;

   LONG pos = 0;
   while ((Offset) and (String[pos])) {
      for (++pos; ((String[pos] & 0xc0) IS 0x80); pos++);
      Offset--;
   }
   return pos;
}

/*****************************************************************************

-FUNCTION-
UTF8PrevLength: Gets the byte length of the previous character at a specific position in a UTF-8 string.

This function calculates the byte-length of the previous character in a string, from an initial position given by
Offset.

-INPUT-
cstr String: Pointer to a UTF-8 string.
int Offset: The byte index from which the size of the previous character should be calculated.

-RESULT-
int: Returns the byte-length of the previous character.

*****************************************************************************/

LONG UTF8PrevLength(CSTRING String, LONG ByteIndex)
{
   LONG len = 0;
   for (--ByteIndex; ByteIndex > 0; --ByteIndex) {
      len++;
      if ((String[ByteIndex] & 0xc0) != 0x80) return len;
   }
   return len;
}

/*****************************************************************************

-FUNCTION-
UTF8ReadValue: Converts UTF-8 characters into 32-bit unicode values.

This function converts UTF-8 character strings into 32-bit unicode values.  To determine how many bytes were used in
making up the UTF-8 character, set the Length argument so that the function can return the total number of bytes.

-INPUT-
cstr String: Pointer to a character in a UTF-8 string that you want to convert.
&int Length: Optional argument that will store the resulting number of bytes that make up the UTF-8 character.

-RESULT-
uint: Returns the extracted unicode value.  If a failure occurs (the encoding is invalid) then a value of zero is returned. Zero can also be returned if the String parameter is NULL or begins with a null character.

*****************************************************************************/

ULONG UTF8ReadValue(CSTRING String, LONG *Length)
{
   ULONG code;
   const char *str;

   if (!(str = String)) {
      if (Length) *Length = 0;
      return 0;
   }

   if (*str IS 0) {
      if (Length) *Length = 0;
      return 0;
   }
   else if ((UBYTE)(*str) < 128) {
      if (Length) *Length = 1;
      return *str;
   }
   else if ((*str & 0xe0) IS 0xc0) {
      if (Length) *Length = 2;
      return ((str[0] & 0x1f)<<6) | (str[1] & 0x3f);
   }
   else if ((*str & 0xf0) IS 0xe0) {
      if (Length) *Length = 3;
      code = *str & 0x0f;
      for (WORD i=1; i < 3; i++) {
         if ((str[i] & 0xc0) != 0x80) return 0;
         code = (code<<6) | (str[i] & 0x3f);
      }
   }
   else if ((*str & 0xf8) IS 0xf0) {
      if (Length) *Length = 4;
      code = *str & 0x07;
      for (WORD i=1; i < 4; i++) {
         if ((str[i] & 0xc0) != 0x80) return 0;
         code = (code<<6) | (str[i] & 0x3f);
      }
   }
   else if ((*str & 0xfc) IS 0xf8) {
      if (Length) *Length = 5;
      code = *str & 0x03;
      for (WORD i=1; i < 5; i++) {
         if ((str[i] & 0xc0) != 0x80) return 0;
         code = (code<<6) | (str[i] & 0x3f);
      }
   }
   else if ((*str & 0xfc) IS 0xfc) {
      if (Length) *Length = 6;
      code = *str & 0x01;
      for (WORD i=1; i < 6; i++) {
         if ((str[i] & 0xc0) != 0x80) return 0;
         code = (code<<6) | (str[i] & 0x3f);
      }
   }
   else {
      if (Length) *Length = 1;
      return 0;
   }

   return code;
}

/*****************************************************************************

-FUNCTION-
UTF8ValidEncoding: Corrects invalid UTF-8 encodings and converts string encodings to UTF-8.

This function recovers strings that should be in UTF-8 format but could contain characters from another type of
character encoding.  Such uncertainty can occur - for example - when file names in a filesystem are expected to be in
UTF-8 format by default, but there is no absolute guarantee that this rule has been followed by the user or the
installed programs.

When processing the String, any non-UTF8 characters are converted from the given Encoding into UTF-8 format.  Any
invalid characters will be converted to a unicode value of 0xfffd.

Conversion support is provided by the iconv library.  The following encodings are supported:

<types>
<type name="European">ASCII, ISO-8859-{1,2,3,4,5,7,9,10,13,14,15,16}, KOI8-R, KOI8-U, KOI8-RU, CP{1250,1251,1252,1253,1254,1257}, CP{850,866}, Mac{Roman,CentralEurope,Iceland,Croatian,Romania}, Mac{Cyrillic,Ukraine,Greek,Turkish}, Macintosh, CP{437,737,775,852,853,855,857,858,860,861,863,865,869,1125}</>
<type name="Semitic">ISO-8859-{6,8}, CP{1255,1256}, CP862, Mac{Hebrew,Arabic}, CP864</>
<type name="Japanese">EUC-JP, SHIFT_JIS, CP932, ISO-2022-JP, ISO-2022-JP-2, ISO-2022-JP-1, EUC-JISX0213, Shift_JISX0213, ISO-2022-JP-3</>
<type name="Chinese">EUC-CN, HZ, GBK, GB18030, EUC-TW, BIG5, CP950, BIG5-HKSCS, ISO-2022-CN, ISO-2022-CN-EXT</>
<type name="Korean">EUC-KR, CP949, ISO-2022-KR, JOHAB</>
<type name="Armenian">ARMSCII-8</>
<type name="Georgian">Georgian-Academy, Georgian-PS</>
<type name="Tajik">KOI8-T</>
<type name="Thai">TIS-620, CP874, MacThai</>
<type name="Laotian">MuleLao-1, CP1133</>
<type name="Vietnamese">VISCII, TCVN, CP1258</>
<type name="Platform specifics">HP-ROMAN8, NEXTSTEP</>
<type name="Full Unicode">UTF-8 UCS-2, UCS-2BE, UCS-2LE UCS-4, UCS-4BE, UCS-4LE, UTF-16, UTF-16BE, UTF-16LE, UTF-32, UTF-32BE, UTF-32LE, UTF-7, C99, JAVA, UCS-2-INTERNAL, UCS-4-INTERNAL</>
<type name="Locale Dependent">char, wchar_t</>
<type name="Turkmen">TDS565</>
</types>

-INPUT-
cstr String:   The string to be validated.
cstr Encoding: The encoding that should be tried for invalid UTF-8 characters.  Set to zero if the encoding is unknown (the system default will be used if the encoding cannot be determined).

-RESULT-
cstr: Returns the original string pointer if it is already valid, otherwise a converted string is returned.  The converted string remains valid up until the next call to UTF8ValidEncoding().  A return of NULL is possible if an internal error occurs during the conversion process (e.g. invalid encoding type).

*****************************************************************************/

CSTRING UTF8ValidEncoding(CSTRING String, CSTRING Encoding)
{
   static LONG buffersize = 0;
   static ULONG icvhash = 0;
   static BYTE initfailed = FALSE;
   CSTRING str, output, input;
   ULONG uchar, enchash;
   LONG len, in, out;
   size_t inleft, outleft;

   if ((!String) or (initfailed)) {
      if (glIconvBuffer) {
         // Calling this function with a NULL String is an easy/valid way to free the internal buffer
         FreeResource(glIconvBuffer);
         glIconvBuffer = NULL;
         buffersize = 0;
      }
      return NULL;
   }

   struct ObjectContext *context = tlContext;
   tlContext = &glTopContext;

   if (!modIconv) {
      #ifdef _WIN32
         if (CreateObject(ID_MODULE, 0, &modIconv,
               FID_Name|TSTR,   "libiconv2.dll",
               FID_Flags|TLONG, MOF_LINK_LIBRARY|MOF_STATIC,
               TAGEND)) {
            initfailed = TRUE;
            tlContext = context;
            return NULL;
         }

         modResolveSymbol(modIconv, "libiconv_open", (APTR *)&iconv_open);
         modResolveSymbol(modIconv, "libiconv_close", (APTR *)&iconv_close);
         modResolveSymbol(modIconv, "libiconv", (APTR *)&iconv);

         if ((!iconv) or (!iconv_open) or (!iconv_close)) {
            acFree(modIconv);
            modIconv = NULL;
            tlContext = context;
            initfailed = TRUE;
            return NULL;
         }
      #else
         if (CreateObject(ID_MODULE, 0, &modIconv,
               FID_Name|TSTR,   "libiconv2",
               FID_Flags|TLONG, MOF_LINK_LIBRARY,
               TAGEND)) {
            initfailed = TRUE;
            tlContext = context;
            return NULL;
         }
      #endif
   }

   // Check if the string is valid UTF-8 and if so, return

   str    = String;
   in     = 0; // Current input index
   out    = 0; // No of bytes written
   while (str[in]) {
      uchar = UTF8ReadValue(str+in, &len);

      if (!uchar) {
         // An invalid character has been found

         if (!Encoding) {
            Encoding = "char"; // Convert from the system default
         }

         // Initialise iconv

         enchash = StrHash(Encoding, 0);
         if ((glIconv) and (enchash IS icvhash)); // Correct iconv encoding is already loaded.
         else {
            if (glIconv) iconv_close(glIconv);

            glIconv = iconv_open("UTF-8", Encoding);

            if (!glIconv) {
               tlContext = context;
               return NULL;
            }
            icvhash = enchash;
         }

         // Allocate a conversion buffer if we don't already have one

         if (!glIconvBuffer) {
            buffersize = 4096;
            if (buffersize < in) buffersize = in + 1024;

            if (AllocMemory(buffersize, MEM_STRING|MEM_NO_CLEAR, (APTR *)&glIconvBuffer, NULL) != ERR_Okay) {
               tlContext = context;
               return NULL;
            }
         }

         // Copy all characters up to the point at which the last invalid character was encountered.

         if (in > 0) CopyMemory(str, glIconvBuffer, in);

         while (str[in]) {
            // Check/Expand the buffer size

            if (out+12 > buffersize) {
               if (ReallocMemory(glIconvBuffer, buffersize + 4096, (APTR *)&glIconvBuffer, NULL)) {
                  tlContext = context;
                  return NULL;
               }
               buffersize += 4096;
            }

            uchar = UTF8ReadValue(str+in, &len);

            if (!uchar) {
               // Attempt encoding conversion

               input = str + in;
               inleft = 1;
               output = glIconvBuffer + out;
               outleft = buffersize - out;
               if (iconv(glIconv, (const char **)&input, &inleft, (char **)&output, &outleft) != (size_t)-1) {
                  out += (buffersize - out) - outleft;
               }
               else {
                  // Failed to convert character.  Unknown characters are converted to 0xFFFD (the UTF-8 'replacement
                  // character'

                  out += UTF8WriteValue(0xfffd, glIconvBuffer + out, buffersize - out);
               }

               in++;
            }
            else while (len-- > 0) glIconvBuffer[out++] = str[in++];
         }

         glIconvBuffer[out] = 0;
         tlContext = context;
         return glIconvBuffer;
      }
      else {
         if (len) in += len;
         else in++;
      }
   }

   tlContext = context;
   return String;
}

/*****************************************************************************

-FUNCTION-
UTF8WriteValue: Writes a 32-bit unicode value into a UTF-8 character buffer.

This function is used to write out unicode values in UTF-8 string format.  You need to provide a string buffer, keeping
in mind that anything from 1 to 6 bytes may be written to the buffer, depending on the size of the unicode value.

If the buffer is not large enough to hold the UTF-8 string then the function will do nothing and return a value of
zero.  Otherwise, the character will be written and the total number of bytes used will be returned.

This function will not add a null terminator to the end of the UTF-8 string.

-INPUT-
int Value:  A 32-bit unicode value.
buf(str) Buffer: Pointer to a string buffer that will hold the UTF-8 characters.
bufsize Size:   The size of the destination string buffer (does not need to be any larger than 6 bytes).

-RESULT-
int: Returns the total amount of characters written to the string buffer.

-END-

*****************************************************************************/

LONG UTF8WriteValue(LONG Value, STRING String, LONG StringSize)
{
   if (Value < 128) {
      if (Value < 0) return 0;
      *String = (UBYTE)Value;
      return 1;
   }
   else if (Value < 0x800) {
      if (StringSize < 2) return 0;
      String[1] = (Value & 0x3f) | 0x80;
      Value  = Value>>6;
      String[0] = Value | 0xc0;
      return 2;
   }
   else if (Value < 0x10000) {
      if (StringSize < 3) return 0;
      String[2] = (Value & 0x3f)|0x80;
      Value  = Value>>6;
      String[1] = (Value & 0x3f)|0x80;
      Value  = Value>>6;
      String[0] = Value | 0xe0;
      return 3;
   }
   else if (Value < 0x200000) {
      if (StringSize < 4) return 0;
      String[3] = (Value & 0x3f)|0x80;
      Value  = Value>>6;
      String[2] = (Value & 0x3f)|0x80;
      Value  = Value>>6;
      String[1] = (Value & 0x3f)|0x80;
      Value  = Value>>6;
      String[0] = Value | 0xf0;
      return 4;
   }
   else if (Value < 0x4000000) {
      if (StringSize < 5) return 0;
      String[4]  = (Value & 0x3f)|0x80;
      Value = Value>>6;
      String[3]  = (Value & 0x3f)|0x80;
      Value = Value>>6;
      String[2]  = (Value & 0x3f)|0x80;
      Value = Value>>6;
      String[1]  = (Value & 0x3f)|0x80;
      Value = Value>>6;
      String[0]  = Value | 0xf8;
      return 5;
   }
   else {
      if (StringSize < 6) return 0;
      String[5]  = (Value & 0x3f)|0x80;
      Value = Value>>6;
      String[4]  = (Value & 0x3f)|0x80;
      Value = Value>>6;
      String[3]  = (Value & 0x3f)|0x80;
      Value = Value>>6;
      String[2]  = (Value & 0x3f)|0x80;
      Value = Value>>6;
      String[1]  = (Value & 0x3f)|0x80;
      Value = Value>>6;
      String[0]  = Value | 0xfc;
      return 6;
   }
}
