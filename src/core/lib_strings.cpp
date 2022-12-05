/*****************************************************************************
-CATEGORY-
Name: Strings
-END-
*****************************************************************************/

#include "defs.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <parasol/modules/fluid.h>

#ifdef __ANDROID__
#include <android/configuration.h>
#include <parasol/modules/android.h>
#endif

static void sift(STRING Buffer, LONG *, LONG, LONG);
static ERROR  insert_string(CSTRING, STRING, LONG, LONG, LONG);

typedef void * iconv_t;
iconv_t (*iconv_open)(const char* tocode, const char* fromcode);
size_t  (*iconv)(iconv_t cd, const char** inbuf, size_t* inbytesleft,   char** outbuf, size_t* outbytesleft);
int     (*iconv_close)(iconv_t cd);
void    (*iconvlist)(int (*do_one)(unsigned int namescount, const char* const* names, void* data), void* data);

STRING glIconvBuffer = NULL;
MEMORYID glTranslateMID = 0;
BYTE glTranslateLoad = FALSE; // Set to TRUE once the first attempt to load the translation table has been made
char glTranslateBuffer[120];
OBJECTPTR modIconv = NULL;
static iconv_t glIconv = NULL;
struct FluidBase *FluidBase = 0; // Must be zero

static void refresh_locale(void)
{
   if (glLocale) { acFree(glLocale); glLocale = NULL; }
}

void free_iconv(void)
{
   if (modIconv) {
      if (glIconv) { iconv_close(glIconv); glIconv = NULL; }
      if (glIconvBuffer) { FreeResource(glIconvBuffer); glIconvBuffer = NULL; }

      acFree(modIconv);
      modIconv = NULL;
   }
}

/*****************************************************************************

-FUNCTION-
CharCopy: Copies the characters of one string to another.

This function copies a string of characters to a destination.  It will copy the exact number of characters as specified
by Length, unless the source String terminates before the total number of characters have been copied.

This function will not null-terminate the destination.  This function is not 'safe', in that it is capable of
overflowing the destination buffer (this capability is intentional).  Use StrCopy() if you want to copy a complete
string with a limit on the destination buffer size.

-INPUT-
cstr Source: Pointer to the string that you are copying from.
str Dest:   Pointer to the buffer that you are copying to.
int Length: The number of characters to copy.

-RESULT-
int: Returns the total amount of characters that were copied.

*****************************************************************************/

LONG CharCopy(CSTRING String, STRING Dest, LONG Length)
{
   LONG i;

   if ((String) and (Dest)) {
      for (i=0; (i < Length) and (String[i]); i++) Dest[i] = String[i];
      return i;
   }
   else return 0;
}

/*****************************************************************************

-FUNCTION-
StrBuildArray: Builds an array of strings from a sequential string list.

This function is helpful for converting a buffer of sequential values into a more easily managed string array.  A
"sequential list of values" is any number of strings arranged one after the other in a single byte array.  It is
similar in arrangement to a CSV file, but null-terminators signal an end to each string value.

To convert such a string list into an array, you need to know the total byte size of the list, as well as the total
number of strings in the list. If you don't know this information, you can either alter your routine to make provisions
for it, or you can pass the `SBF_CSV` flag if the List is in CSV format.  CSV mode incurs a performance hit as the string
needs to be analysed first.

Once you call this function, it will allocate a memory block to contain the array and string information.  The list
will then be converted into the array, which will be terminated with a NULL pointer at its end.  If you have specified
the `SBF_SORT` or `SBF_NO_DUPLICATES` Flags then the array will be sorted into alphabetical order for your convenience.  The
`SBF_NO_DUPLICATES` flag also removes duplicated strings from the array.

Remember to free the allocated array when it is no longer required.

-INPUT-
str List:  Pointer to a string of sequentially arranged values.
int Size:  The total byte size of the List, not including the terminating byte.
int Total: The total number of strings specified in the List.
int(SBF) Flags: Set to SBF_SORT to sort the list, SBF_NO_DUPLICATES to sort the list and remove duplicated strings, Use SBF_CSV if the List is in CSV format, in which case the Size and Total values are ignored (note - the string will be modified if in CSV).

-RESULT-
!array(str): Returns an array of STRING pointers, or NULL on failure.  The pointer is a memory block that must be freed after use.

*****************************************************************************/

STRING * StrBuildArray(STRING List, LONG Size, LONG Total, LONG Flags)
{
   parasol::Log log(__FUNCTION__);

   if (!List) return NULL;

   LONG i;
   char *csvbuffer_alloc = NULL;
   char csvbuffer[1024];
   if (Flags & SBF_CSV) {
      // Note that empty strings (commas following no content) are allowed and are treated as null strings.

      // We are going to modify the string with some nulls, so make a copy of it

      i = StrLength(List);
      if ((size_t)i < sizeof(csvbuffer)) {
         CopyMemory(List, csvbuffer, i+1);
         List = csvbuffer;
      }
      else if ((csvbuffer_alloc = (char *)malloc(i+1))) {
         CopyMemory(List, csvbuffer_alloc, i+1);
         List = csvbuffer_alloc;
      }

      Size = 0;
      Total = 0;
      for (i=0; List[i];) {
         while ((List[i]) and (List[i] <= 0x20)) i++; // Skip leading whitespace
         if (!List[i]) break;
         Total++;

         if (List[i] IS '"') {
            i++;
            while ((List[i]) and (List[i] != '"')) i++;
            if (List[i] IS '"') i++;
         }
         else if (List[i] IS '\'') {
            i++;
            while ((List[i]) and (List[i] != '\'')) i++;
            if (List[i] IS '\'') i++;
         }
         else while ((List[i]) and (List[i] != ',') and (List[i] != '\n')) i++;

         if ((List[i] IS ',') or (List[i] IS '\n')) List[i++] = 0;
      }
      Size = i;
   }

   if ((Size) and (Total > 0)) {
      CSTRING *array;
      if (!AllocMemory(Size + 1 + ((Total + 1) * sizeof(STRING)), MEM_DATA, (APTR *)&array, NULL)) {
         // Build the array

         STRING str = (STRING)(array + Total + 1);
         for (i=0; i < Size; i++) str[i] = List[i];
         LONG pos = 0;
         for (i=0; i < Total; i++) {
            array[i] = str+pos;
            while ((str[pos]) and (pos < Size)) pos++;
            if (str[pos]) {
               log.warning("The string buffer exceeds its specified length of %d bytes.", Size);
               break;
            }
            pos++;
         }
         array[i] = NULL;

         // Remove duplicate strings and/or do sorting

         if (Flags & SBF_NO_DUPLICATES) {
            StrSort(array, 0);
            for (i=1; array[i]; i++) {
               if (!StrMatch(array[i-1], array[i])) {
                  LONG j;
                  for (j=i; array[j]; j++) array[j] = array[j+1];
                  i--;
                  Total--;
               }
            }
            array[Total] = NULL;
         }
         else if (Flags & SBF_SORT) StrSort(array, 0);

         if (csvbuffer_alloc) free(csvbuffer_alloc);
         return (STRING *)array;
      }
   }

   if (csvbuffer_alloc) free(csvbuffer_alloc);
   return NULL;
}

/*****************************************************************************

-FUNCTION-
StrCapitalise: Capitalises a string.

This function will will capitalise a string so that every word starts with a capital letter.  All letters following the
first capital of each word will be driven to lower-case characters.  Numbers and other non-alphabetic characters will
not be affected by this function.  Here is an example:

<pre>"every WOrd starts WITH a 2apital" = "Every Word Starts With A 2apital"</pre>

-INPUT-
str String: Points to the string that is to be capitalised.

-END-

*****************************************************************************/

// BUGS: Needs to support capitalisation of European chars in UTF-8 space (umlauts etc)

void StrCapitalise(STRING String)
{
  while (*String) {
     while ((*String) and (*String <= 0x20)) String++; // Skip whitespace
     if (!*String) return;

     // Capitalise the first character

     if ((*String >= 'a') and (*String <= 'z')) {
        *String = *String - 'a' + 'A';
     }

     String++;

     // Lower-case all following characters

     while ((*String) and (*String > 0x20)) {
        if ((*String >= 'A') and (*String <= 'Z')) *String = *String - 'A' + 'a';
        String++;
     }
  }
}

/*****************************************************************************

-FUNCTION-
StrClone: Clones string data.

This function creates an exact duplicate of a string.  It analyses the length of the supplied String, allocates a
private memory block for it and then copies the string characters into the new string buffer.

You are expected to free the resulting memory block once it is no longer required with ~FreeResource().

-INPUT-
cstr String: The string that is to be cloned.

-RESULT-
str: Returns an exact duplicate of the String.  If this function fails to allocate the memory or if the String argument is NULL, NULL is returned.

*****************************************************************************/

STRING StrClone(CSTRING String)
{
   if (!String) return NULL;

   LONG i;
   for (i=0; String[i]; i++);

   STRING newstr;
   if ((!AllocMemory(i+1, MEM_STRING, (APTR *)&newstr, NULL))) {
      for (i=0; String[i]; i++) newstr[i] = String[i];
      newstr[i] = 0;
      return newstr;
   }
   else return NULL;
}

/*****************************************************************************

-FUNCTION-
StrCompare: Compares strings to see if they are identical.

This function compares two strings against each other.  If the strings match then it returns ERR_Okay, otherwise it
returns ERR_False.  By default the function is not case sensitive, but you can turn on case sensitivity by
specifying the `STR_CASE` flag.

If you set the Length to 0, the function will compare both strings for differences until a string terminates.  If all
characters matched up until the termination, ERR_Okay will be returned regardless of whether or not one of the strings
happened to be longer than the other.

If the Length is not 0, then the comparison will stop once the specified number of characters to match has been
reached.  If one of the strings terminates before the specified Length is matched, ERR_False will be returned.

If the `STR_MATCH_LEN` flag is specified, you can force the function into returning an ERR_Okay code only on the
condition that both strings are of matching lengths.  This flag is typically specified if the Length argument has
been set to 0.

If the `STR_WILDCARD` flag is set, the first string that is passed may contain wild card characters, which gives special
meaning to the asterisk and question mark characters.  This allows you to make abstract comparisons, for instance
`ABC*` would match to `ABCDEF` and `1?3` would match to `1x3`.

-INPUT-
cstr String1: Pointer to the first string.
cstr String2: Pointer to the second string.
int Length:  The maximum number of characters to compare.  Does not apply to wildcard comparisons.
int(STR) Flags:   Optional flags.

-ERRORS-
Okay:  The strings match.
False: The strings do not match.
NullArgs:

*****************************************************************************/

ERROR StrCompare(CSTRING String1, CSTRING String2, LONG Length, LONG Flags)
{
   LONG len, i, j;
   UBYTE char1, char2, fail;
   CSTRING Original;
   #define Wildcard String1

   if ((!String1) or (!String2)) return ERR_Args;

   if (String1 IS String2) return ERR_Okay; // Return a match if both addresses are equal

   if (!Length) len = 0x7fffffff;
   else len = Length;

   Original = String2;

   if (Flags & STR_WILDCARD) {
      if (!Wildcard[0]) return ERR_Okay;

      while ((*Wildcard) and (*String2)) {
         fail = FALSE;
         if (*Wildcard IS '*') {
            while (*Wildcard IS '*') Wildcard++;

            for (i=0; (Wildcard[i]) and (Wildcard[i] != '*') and (Wildcard[i] != '|'); i++); // Count the number of printable characters after the '*'

            if (i IS 0) return ERR_Okay; // Nothing left to compare as wildcard string terminates with a *, so return match

            if ((!Wildcard[i]) or (Wildcard[i] IS '|')) {
               // Scan to the end of the string for wildcard situation like "*.txt"

               for (j=0; String2[j]; j++); // Get the number of characters left in the second string
               if (j < i) fail = TRUE; // Quit if the second string has run out of characters to cover itself for the wildcard
               else String2 += j - i; // Skip everything in the second string that covers us for the '*' character
            }
            else {
               // Scan to the first matching wildcard character in the string, for handling wildcards like "*.1*.2"

               while (*String2) {
                  if (Flags & STR_CASE) {
                     if (*Wildcard IS *String2) break;
                  }
                  else {
                     char1 = *String1; if ((char1 >= 'A') and (char1 <= 'Z')) char1 = char1 - 'A' + 'a';
                     char2 = *String2; if ((char2 >= 'A') and (char2 <= 'Z')) char2 = char2 - 'A' + 'a';
                     if (char1 IS char2) break;
                  }
                  String2++;
               }
            }
         }
         else if (*Wildcard IS '?') {
            // Do not compare ? wildcards
            Wildcard++;
            String2++;
         }
         else if ((*Wildcard IS '\\') and (Wildcard[1])) {
            Wildcard++;
            if (Flags & STR_CASE) {
               if (*Wildcard++ != *String2++) fail = TRUE;
            }
            else {
               char1 = *String1++; if ((char1 >= 'A') and (char1 <= 'Z')) char1 = char1 - 'A' + 'a';
               char2 = *String2++; if ((char2 >= 'A') and (char2 <= 'Z')) char2 = char2 - 'A' + 'a';
               if (char1 != char2) fail = TRUE;
            }
         }
         else if ((*Wildcard IS '|') and (Wildcard[1])) {
            Wildcard++;
            String2 = Original; // Restart the comparison
         }
         else {
            if (Flags & STR_CASE) {
               if (*Wildcard++ != *String2++) fail = TRUE;
            }
            else {
               char1 = *String1++; if ((char1 >= 'A') and (char1 <= 'Z')) char1 = char1 - 'A' + 'a';
               char2 = *String2++; if ((char2 >= 'A') and (char2 <= 'Z')) char2 = char2 - 'A' + 'a';
               if (char1 != char2) fail = TRUE;
            }
         }

         if (fail) {
            // Check for an or character, if we find one, we can restart the comparison process.

            while ((*Wildcard) and (*Wildcard != '|')) Wildcard++;

            if (*Wildcard IS '|') {
               Wildcard++;
               String2 = Original;
            }
            else return ERR_False;
         }
      }

      if (!String2[0]) {
         if (!Wildcard[0]) return ERR_Okay;
         else if (Wildcard[0] IS '|') return ERR_Okay;
      }

      if ((Wildcard[0] IS '*') and (Wildcard[1] IS 0)) return ERR_Okay;

      return ERR_False;
   }
   else if (Flags & STR_CASE) {
      while ((len) and (*String1) and (*String2)) {
         if (*String1++ != *String2++) return ERR_False;
         len--;
      }
   }
   else  {
      while ((len) and (*String1) and (*String2)) {
         char1 = *String1;
         char2 = *String2;
         if ((char1 >= 'A') and (char1 <= 'Z')) char1 = char1 - 'A' + 'a';
         if ((char2 >= 'A') and (char2 <= 'Z')) char2 = char2 - 'A' + 'a';
         if (char1 != char2) return ERR_False;

         String1++; String2++;
         len--;
      }
   }

   // If we get here, one of strings has terminated or we have exhausted the number of characters that we have been
   // requested to check.

   if (Flags & (STR_MATCH_LEN|STR_WILDCARD)) {
      if ((*String1 IS 0) and (*String2 IS 0)) return ERR_Okay;
      else return ERR_False;
   }
   else if ((Length) and (len > 0)) return ERR_False;
   else return ERR_Okay;
}

/*****************************************************************************

-FUNCTION-
StrCopy: Copies the characters of one string to another.

This function copies part of one string over to another.  If the Length is set to zero then this function will copy the
entire Src string over to the Dest.  Note that if this function encounters the end of the Src string (i.e. the null
byte) while copying, then it will stop automatically to prevent copying of junk characters.

Please note that the Dest string will <i>always</i> be null-terminated by this function regardless of whether you set
the Length or not.  For example, if you were to copy "123" into the middle of string "ABCDEFGHI" then the result would
be "ABC123". The "GHI" part of the string would be lost.  In situations such as this, functions such as
~CharCopy(), ~StrReplace() or ~StrInsert() should be used instead.

-INPUT-
cstr Src: Pointer to the string that you are copying from.
str Dest: Pointer to the buffer that you are copying to.
int Length: The maximum number of characters to copy, including the NULL byte.  The Length typically indicates the buffer size of the Dest pointer.  Setting the Length to COPY_ALL will copy all characters from the Src string.

-RESULT-
int: Returns the total amount of characters that were copied, not including the null byte at the end.

*****************************************************************************/

LONG StrCopy(CSTRING String, STRING Dest, LONG Length)
{
   parasol::Log log(__FUNCTION__);

   if (Length < 0) return 0;

   LONG i = 0;
   if ((String) and (Dest)) {
      if (!Length) {
         log.warning("Warning - zero length given for copying string \"%s\".", String);
         ((UBYTE *)0)[0] = 0;
      }

      while ((i < Length) and (*String)) {
         Dest[i++] = *String++;
      }

      if ((*String) and (i >= Length)) {
         //log.warning("Overflow: %d/%d \"%.20s\"", i, Length, Dest);
         Dest[i-1] = 0; // If we ran out of buffer space, we have to terminate from one character back
      }
      else Dest[i] = 0;
   }

   return i;
}

/*****************************************************************************

-FUNCTION-
StrDatatype: Determines the data type of a string.

This function analyses a string and returns its data type.  Valid return values are STT_FLOAT for floating point
numbers, STT_NUMBER for whole numbers, STT_HEX for hexadecimal (e.g. 0x1) and STT_STRING for any other string type.  In
order for the string to be recognised as one of the number types, it must be limited to numbers and qualification
characters, such as a decimal point or negative sign.

Any white-space at the start of the string will be skipped.

-INPUT-
cstr String: The string that you want to analyse.

-RESULT-
int(STT): Returns STT_FLOAT, STT_NUMBER, STT_HEX or STT_STRING.

*****************************************************************************/

LONG StrDatatype(CSTRING String)
{
   if (!String) return 0;

   while ((*String) and (*String <= 0x20)) String++; // Skip white-space

   LONG i;
   if ((String[0] IS '0') and (String[1] IS 'x')) {
      for (i=2; String[i]; i++) {
         if (((String[i] >= '0') and (String[i] <= '9')) OR
             ((String[i] >= 'A') and (String[i] <= 'F')) OR
             ((String[i] >= 'a') and (String[i] <= 'f')));
         else return STT_STRING;
      }
      return STT_HEX;
   }

   BYTE is_number = TRUE;
   BYTE is_float  = FALSE;

   for (i=0; (String[i]) and (is_number); i++) {
      if (((String[i] < '0') or (String[i] > '9')) and (String[i] != '.') and (String[i] != '-')) is_number = FALSE;
      if (String[i] IS '.') is_float = TRUE;
   }

   if ((is_float) and (is_number)) return STT_FLOAT;
   else if (is_number) return STT_NUMBER;
   else return STT_STRING;
}

/*****************************************************************************

-FUNCTION-
StrExpand: Expands the size of a string by inserting spaces.

This function will expand a string by inserting spaces into a specified position.  The String that you are expanding
must be placed in a memory area large enough to accept the increased size, or you will almost certainly corrupt other
memory areas.

-INPUT-
str String: The string that you want to expand.
int Offset: The byte position that you want to start expanding from.  Give consideration to UTF-8 formatting.
int TotalChars: The total number of spaces that you want to insert.

-RESULT-
int: Returns the new length of the expanded string, not including the null byte.

*****************************************************************************/

LONG StrExpand(STRING String, LONG Pos, LONG AmtChars)
{
   parasol::Log log(__FUNCTION__);

   if ((String) and (AmtChars)) {
      LONG len, i;
      if ((len = StrLength(String)) > 0) {
         if (Pos < 0) Pos = 0;
         if (Pos > len) Pos = len;

         // Shift the characters at Pos to the right

         String[len + AmtChars] = 0;        // Set new termination
         for (i = len-1; i > Pos-1; i--) {  // Shift the characters across
            String[i + AmtChars] = String[i];
         }

         for (i = Pos; i < (Pos + AmtChars); i++) String[i] = ' ';

         return len + AmtChars;
      }
   }
   else log.warning("Bad arguments.");

   return 0;
}

/*****************************************************************************

-FUNCTION-
StrFormat: Formats a string using printf() style arguments.
ExtPrototype: const void *, LONG, const char *, ...) __attribute__((format(printf, 3, 4))

StrFormat() duplicates the functionality found in the printf() family of functions.  It is provided to assist
portability where there is no guarantee that POSIX is available on the host platform.

-INPUT-
buf(str) Buffer: Reference to a destination buffer for the formatted string.
bufsize Size: The length of the destination Buffer, in bytes.
cstr Format: The string format to use for processing.
tags Parameters: A tag-list of arguments that match the parameters in the Format string.

-RESULT-
int: Returns the total number of characters written to the buffer (not including the NULL terminator).  If the total number of characters is greater or equal to the BufferSize minus 1, then you should assume that a buffer overflow occurred.

*****************************************************************************/

LONG StrFormat(STRING Buffer, LONG BufferSize, CSTRING Format, ...)
{
   if (!Format) return 0;
   if (BufferSize <= 0) return 0;

   va_list arg;
   va_start(arg, Format);
   LONG chars = vsnprintf(Buffer, BufferSize, Format, arg);
   va_end(arg);
   return chars;
}

/*****************************************************************************

-FUNCTION-
StrHash: Convert a string into a 32-bit hash.

This function will convert a string into a 32-bit hash.  The hashing algorithm is consistent throughout our
platform and is therefore guaranteed to be compatible with all areas that make use of hashed values.

Hashing is case insensitive by default.  If case sensitive hashing is desired, please set CaseSensitive to TRUE
when calling this function.  Please keep in mind that a case sensitive hash value will not be interchangeable with a
case insensitive hash of the same string.

-INPUT-
cstr String: Reference to a string that will be processed.
int CaseSensitive: Set to TRUE to enable case sensitivity.

-RESULT-
uint: The 32-bit hash is returned.

*****************************************************************************/

ULONG StrHash(CSTRING String, LONG CaseSensitive)
{
   if (!String) return 0;

   ULONG hash = 5381;
   UBYTE c;
   if (CaseSensitive) {
      while ((c = *String++)) hash = ((hash<<5) + hash) + c;
      return hash;
   }
   else {
      while ((c = *String++)) {
         if ((c >= 'A') and (c <= 'Z')) hash = (hash<<5) + hash + c - 'A' + 'a';
         else hash = (hash<<5) + hash + c;
      }
      return hash;
   }
}

/*****************************************************************************

-FUNCTION-
StrInsert: Inserts a string into a buffer, with optional replace.

This function is used to insert a series of characters into a Buffer.  The position of the insert is determined by the
byte offset declared in the Offset field.  You can opt to replace a specific number of characters in the destination by
setting the ReplaceChars argument to a value greater than zero.  To prevent buffer overflow, you must declare the size
of the Buffer in the BufferSize argument.

-INPUT-
cstr Insert:      Pointer to the character string to be inserted.
buf(str) Buffer:  The string that will receive the characters.
bufsize Size:     The byte size of the Buffer.
int Offset:       The byte position at which the insertion will start.  Give consideration to UTF-8 formatting.
int ReplaceChars: The number of bytes to replace (set to zero for a normal insert).

-ERRORS-
Okay
Args
BufferOverflow: There is not enough space in the destination buffer for the insert.

*****************************************************************************/

ERROR StrInsert(CSTRING Insert, STRING Buffer, LONG Size, LONG Pos, LONG ReplaceChars)
{
   parasol::Log log(__FUNCTION__);

   if (!Insert) Insert = "";

   LONG insertlen;
   for (insertlen=0; Insert[insertlen]; insertlen++);

   // String insertion

   if (insertlen < ReplaceChars) {
      LONG i = Pos + StrCopy(Insert, Buffer+Pos, COPY_ALL);
      i = Pos + ReplaceChars;
      Pos += insertlen;
      while (Buffer[i]) Buffer[Pos++] = Buffer[i++];
      Buffer[Pos] = 0;
   }
   else if (insertlen IS ReplaceChars) {
      while (*Insert) Buffer[Pos++] = *Insert++;
   }
   else {
      // Check if an overflow will occur

      LONG strlen, i, j;
      for (strlen=0; Buffer[strlen]; strlen++);
      if ((Size - 1) < (strlen + (ReplaceChars - insertlen))) {
         log.warning("Buffer overflow: \"%.60s\"", Buffer);
         return ERR_BufferOverflow;
      }

      // Expand the string
      i = strlen + (insertlen - ReplaceChars) + 1;
      strlen += 1;
      j = strlen-Pos-ReplaceChars+1;
      while (j > 0) {
         Buffer[i--] = Buffer[strlen--];
         j--;
      }

      // Copy the insert string into the position
      for (i=0; Insert[i]; i++, Pos++) Buffer[Pos] = Insert[i];
   }

   return ERR_Okay;
}

/*****************************************************************************

-FUNCTION-
StrLength: Calculates the length of a string.

This function will calculate the length of a String, not including the null byte.

-INPUT-
cstr String: Pointer to the string that you want to examine.

-RESULT-
int: Returns the length of the string.

*****************************************************************************/

LONG StrLength(CSTRING String)
{
   if (String) return strlen(String);
   else return 0;
}

/*****************************************************************************

-FUNCTION-
StrLineLength: Determines the line-length of a string.

This function calculates the length of a String up to the first carriage return, line-break or null byte.  This
function is commonly used on large text files where the String points to a position inside a large character buffer.

-INPUT-
cstr String: Pointer to an address inside a character buffer.

-RESULT-
int: Returns the length of the first line in the string, not including the return code.

*****************************************************************************/

LONG StrLineLength(CSTRING String)
{
   if (String) {
      LONG i = 0;
      while ((String[i]) and (String[i] != '\n') and (String[i] != '\r')) i++;
      return i;
   }
   else return 0;
}

/*****************************************************************************

-FUNCTION-
StrLower: Changes a string so that all alpha characters are in lower-case.

This function will alter a string so that all upper case characters are changed to lower-case.  Non upper-case
characters are unaffected by this function.  Here is an example:

<pre>"HeLLo world" = "hello world"</pre>

-INPUT-
buf(str) String: Pointer to the string that you want to change to lower-case.

*****************************************************************************/

void StrLower(STRING String)
{
   if (!String) return;

   while (*String) {
      if ((*String >= 'A') and (*String <= 'Z')) *String += 0x20;
      String++;
   }
}

/*****************************************************************************

-FUNCTION-
StrNextLine: Returns a pointer to the next line in a string buffer.

This function scans a character buffer for a carriage return or line feed and returns a pointer to the following line.
If no return code is found, NULL is returned.

This function is commonly used for scanning text files on a line by line basis.

-INPUT-
cstr String: Pointer to an address inside a character buffer.

-RESULT-
cstr: Returns a string pointer for the next line, or NULL if no further lines are present.

*****************************************************************************/

CSTRING StrNextLine(CSTRING String)
{
   if (!String) return NULL;

   while ((*String) and (*String != '\n') and (*String != '\r')) String++;
   while (*String IS '\r') String++;
   if (*String IS '\n') String++;
   while (*String IS '\r') String++;
   if (*String) return String;
   else return NULL;
}

/*****************************************************************************

-FUNCTION-
StrReplace: Replaces all occurrences of a keyword or phrase within a given string.

This function will search a string and replace all occurrences of a Keyword or phrase with the supplied Replacement
string.  If the Keyword is found, a new string will be returned which has all matches replaced with the Replacement
string.

If the Keyword is not found, an error code of ERR_Search will be returned.  If you want to know whether the Keyword
actually exists before performing a replacement, consider calling the ~StrSearch() function first.

The new string will be stored in the Result parameter and must be removed with ~FreeResource() once it is no longer required.

-INPUT-
cstr Src:         Points to the source string that contains occurrences of the Keyword that you are searching for.
cstr Keyword:     Identifies the keyword or phrase that you want to replace.
cstr Replacement: Identifies the string that will replace all occurrences of the Keyword.
!str Result:      Must refer to a STRING variable that will store the resulting memory block.
int(STR) Flags:   Set to STR_CASE if the keyword search should be case-sensitive.

-ERRORS-
Okay
Args
NullArgs
Search:      The Keyword could not be found in the Src string.
AllocMemory: Memory for the resulting string could not be allocated.

*****************************************************************************/

ERROR StrReplace(CSTRING Source, CSTRING Keyword, CSTRING Replacement, STRING *Result, LONG CaseSensitive)
{
   parasol::Log log(__FUNCTION__);

   *Result = NULL;

   if ((!Source) or (!Keyword) or (!Result)) return log.warning(ERR_NullArgs);

   if (!Replacement) Replacement = "";

   // If the Keyword and the replacement are identical, there is no need to do any replacement.

   if (!StrCompare(Keyword, Replacement, 0, STR_MATCH_LEN|STR_CASE)) {
      if ((*Result = StrClone(Source))) return ERR_Okay;
      else return ERR_AllocMemory;
   }

   // Calculate string lengths

   LONG keylen = StrLength(Keyword);
   LONG replen = StrLength(Replacement);
   LONG offset;
   CSTRING orig = Source;
   STRING newstr = NULL;
   BYTE alloc  = FALSE;
   LONG pos = 0;
   while ((offset = StrSearch(Keyword, Source + pos, CaseSensitive)) != -1) {
      pos += offset;
      LONG newsize = StrLength(Source) - keylen + replen + 1;
      if (!AllocMemory(newsize, MEM_STRING|MEM_NO_CLEAR, (APTR *)&newstr, NULL)) {
         LONG i;
         for (i=0; i < pos; i++) newstr[i] = Source[i];  // Copy first set of bytes up to the keyword
         for (LONG j=0; j < replen; j++) newstr[i + j] = Replacement[j];  // Copy the replacement
         for (i=0; Source[pos + keylen + i] != 0; i++) {  // Copy the remaining bytes
            newstr[pos + replen + i] = Source[pos + keylen + i];
         }
         newstr[pos + replen + i] = 0;

         if (alloc IS TRUE) FreeResource(Source);
         Source = newstr;
         alloc = TRUE;
         pos += replen;
      }
      else break;
   }

   if (Source IS orig) { *Result = NULL; return ERR_Search; }
   else *Result = (STRING)Source;

   return ERR_Okay;
}

/*****************************************************************************

-FUNCTION-
StrSearch: Searches a string for a particular keyword/phrase.

This function allows you to search for a particular Keyword or phrase inside a String.  You may search on a case
sensitive or case insensitive basis.

-INPUT-
cstr Keyword: A string that specifies the keyword/phrase you are searching for.
cstr String: The string data that you wish to search.
int(STR) Flags:  Optional flags (currently STR_MATCH_CASE is supported for case sensitivity).

-RESULT-
int: Returns the byte position of the first occurrence of the Keyword within the String (possible values start from position 0).  If the Keyword could not be found, this function returns a value of -1.

*****************************************************************************/

LONG StrSearch(CSTRING Keyword, CSTRING String, LONG Flags)
{
   if ((!String) or (!Keyword)) {
      parasol::Log log(__FUNCTION__);
      log.warning(ERR_NullArgs);
      return -1;
   }

   LONG i;
   LONG pos = 0;
   if (Flags & STR_MATCH_CASE) {
      while (String[pos]) {
         for (i=0; Keyword[i]; i++) if (String[pos+i] != Keyword[i]) break;
         if (!Keyword[i]) return pos;
         for (++pos; (String[pos] & 0xc0) IS 0x80; pos++);
      }
   }
   else {
      while (String[pos]) {
         for (i=0; Keyword[i]; i++) if (UCase(String[pos+i]) != UCase(Keyword[i])) break;
         if (!Keyword[i]) return pos;
         for (++pos; (String[pos] & 0xc0) IS 0x80; pos++);
      }
   }

   return -1;
}

/*****************************************************************************

-FUNCTION-
StrShrink: Shrinks strings by destroying data.

This function will shrink a string by removing a sequence of characters and rearranging the remaining data.
For example, `StrShrink("Hello World", 4, 5)` results in `Helrld` by deleting the characters `lo Wo`.

This function modifies the source string in-place, so no memory will be allocated.  The String will be null terminated
and its new length will be returned.

Please note that this function operates on byte positions and is not in compliance with UTF-8 character sequences.

-INPUT-
buf(str) String: Pointer to the string that will be modified.
int Offset:     The byte position that you want to start shrinking the string from.  The position must be shorter than the full length of the string.
int TotalBytes: The total number of bytes to eliminate.

-RESULT-
int: Returns the new length of the shrunken string, not including the null byte.

*****************************************************************************/

LONG StrShrink(STRING String, LONG Offset, LONG TotalBytes)
{
   if ((String) and (Offset >= 0) and (TotalBytes > 0)) {
      STRING orig = String;
      String += Offset;
      const LONG skip = TotalBytes;
      while (String[skip] != 0) { *String = String[skip]; String++; }
      *String = 0;
      return (LONG)(String - orig);
   }
   else return 0;
}

/*****************************************************************************

-FUNCTION-
StrSort: Used to sort string arrays.

This function is used to sort string arrays into alphabetical order.  You will need to provide the list of unsorted
strings in a block of string pointers, terminated with a NULL entry.  For example:

<pre>
CSTRING List[] = {
   "banana",
   "apple",
   "orange",
   "kiwifruit",
   NULL
};
</pre>

The sorting routine will work within the confines of the array that you have provided and will not allocate any memory
when performing the sort.

Optional flags include SBF_NO_DUPLICATES, which strips duplicated strings out of the array; SBF_CASE to acknowledge case
differences when determining string duplication and SBF_DESC to sort in descending order.

-INPUT-
array(cstr) List: Must point to an array of string pointers, terminated with a NULL entry.
int(SBF) Flags: Optional flags may be set here.

-ERRORS-
Okay
NullArgs
-END-

*****************************************************************************/

ERROR StrSort(CSTRING *List, LONG Flags)
{
   if (!List) return ERR_NullArgs;

   // Shell sort.  Similar to bubble sort but much faster because it can copy records over larger distances.

   LONG total, i, j;

   for (total=0; List[total]; total++);

   LONG h = 1;
   while (h < total / 9) h = 3 * h + 1;

   if (Flags & SBF_DESC) {
      for (; h > 0; h /= 3) {
         for (i=h; i < total; i++) {
            auto temp = List[i];
            for (j=i; (j >= h) and (StrSortCompare(List[j - h], temp) < 0); j -= h) {
               List[j] = List[j - h];
            }
            List[j] = temp;
         }
      }
   }
   else {
      for (; h > 0; h /= 3) {
         for (i=h; i < total; i++) {
            auto temp = List[i];
            for (j=i; (j >= h) and (StrSortCompare(List[j - h], temp) > 0); j -= h) {
               List[j] = List[j - h];
            }
            List[j] = temp;
         }
      }
   }

   if (Flags & SBF_NO_DUPLICATES) {
      LONG strflags = STR_MATCH_LEN;
      if (Flags & SBF_CASE) strflags |= STR_MATCH_CASE;

      for (i=1; List[i]; i++) {
         if (!StrCompare(List[i-1], List[i], 0, strflags)) {
            for (j=i; List[j]; j++) List[j] = List[j+1];
            i--;
         }
      }
   }

   return ERR_Okay;
}

/*****************************************************************************

-FUNCTION-
StrSortCompare: Compares two strings for sorting purposes.

This function compares two strings and returns a result that indicates which of the two is 'less than' the other.  The
comparison process takes character-based integers into account, so that in the case of `identical 001` and `identical
999`, the `001` and `999` portions of those strings would be compared as integers and not ASCII characters.  This
prevents string values such as `1` and `10` from being disarranged.

-INPUT-
cstr String1: Pointer to a string.
cstr String2: Pointer to a string.

-RESULT-
int: If both strings are equal, 0 is returned.  If Name1 is greater than Name2, 1 is returned.  If Name1 is lesser than Name2, -1 is returned.

*****************************************************************************/

LONG StrSortCompare(CSTRING Name1, CSTRING Name2)
{
   if ((!Name1) or (!Name2)) return 0;

   while ((*Name1) and (*Name2)) {
      UBYTE char1 = *Name1;
      UBYTE char2 = *Name2;

      if ((char1 >= '0') and (char1 <= '9') and (char2 >= '0') and (char2 <= '9')) {
         // This integer comparison is for human readable sorting
         ULONG val1 = 0;
         while (*Name1 IS '0') Name1++;
         while ((*Name1) and (*Name1 >= '0') and (*Name1 <= '9')) {
            val1 = (val1 * 10) + (*Name1 - '0');
            Name1++;
         }

         ULONG val2 = 0;
         while (*Name2 IS '0') Name2++;
         while ((*Name2) and (*Name2 >= '0') and (*Name2 <= '9')) {
            val2 = (val2 * 10) + (*Name2 - '0');
            Name2++;
         }

         if (val1 > val2) return 1; // Name1 is greater
         else if (val1 < val2) return -1; // Name1 is lesser
         else {
            while ((*Name1 >= '0') and (*Name1 <= '9')) Name1++;
            while ((*Name2 >= '0') and (*Name2 <= '9')) Name2++;
            continue;
         }
      }

      if ((char1 >= 'A') and (char1 <= 'Z')) char1 = char1 - 'A' + 'a';
      if ((char2 >= 'A') and (char2 <= 'Z')) char2 = char2 - 'A' + 'a';

      if (char1 > char2) return 1; // Name1 is greater
      else if (char1 < char2) return -1; // Name1 is lesser

      Name1++;
      Name2++;
   }

   if ((!*Name1) and (!*Name2)) return 0;
   else if (!*Name1) return -1;
   else return 1;
}

/*****************************************************************************

-FUNCTION-
StrTranslateRefresh: Refreshes internal translation tables.

This function refreshes the internal translation tables that convert international English into foreign languages.  It
should only be called if the user changes the default system language, or if the current translation file is updated.

-RESULT-
int: Returns TRUE if the translation table was refreshed.

-END-

*****************************************************************************/

LONG StrTranslateRefresh(void)
{
   parasol::Log log(__FUNCTION__);
   struct translate *translate;
   objConfig *config;
   MEMORYID memoryid;
   CSTRING language;

   log.branch();

   refresh_locale();
   if (StrReadLocale("language", &language)) {
      log.msg("User's preferred language not specified.");
      return FALSE;
   }
   log.msg("Language: %s", language);

   if (glTranslate) {
      if (!StrMatch(language, glTranslate->Language)) {
         log.msg("Language unchanged.");
         return FALSE;
      }
   }

   char path[80];
   LONG i = StrCopy("config:translations/", path, sizeof(path));
   for (LONG j=0; (language[j]) and ((size_t)i < sizeof(path)-1); j++) {
      if ((language[j] >= 'A') and (language[j] <= 'Z')) path[i++] = language[j] - 'A' + 'a';
      else path[i++] = language[j];
   }
   StrCopy(".cfg", path+i, sizeof(path)-i);

   // Load the translation file

   if (!CreateObject(ID_CONFIG, NF_UNTRACKED, (OBJECTPTR *)&config, FID_Path|TSTR, path, TAGEND)) {
      // Count the string lengths to figure out how much memory we need

      LONG total_keys = 0;
      ConfigGroups *sections;
      if ((!GetLong(config, FID_TotalKeys, &total_keys)) and (!GetPointer(config, FID_Data, &sections))) {
         LONG size = sizeof(struct translate) + (sizeof(STRING) * total_keys);

         LONG total = 0;
         for (auto& [section, keys] : sections[0]) {
            for (auto& [k, v] : keys) {
               if (not v.empty()) {
                  size += k.size() + v.size() + 2; // Two trailing null bytes for the strings
                  total++;
               }
            }
         }

         if (!AllocMemory(size, MEM_UNTRACKED|MEM_PUBLIC|MEM_NO_BLOCKING, (APTR *)&translate, &memoryid)) {
            translate->Replaced = FALSE;
            translate->Total = total;
            StrCopy(language, translate->Language, sizeof(translate->Language));
            auto array  = (LONG *)(translate + 1);
            auto str    = (STRING)(array + total);
            auto strbuf = (MAXINT)(array + total);

            for (auto& [section, keys] : sections[0]) {
               for (auto& [k, v] : keys) {
                  if (not v.empty()) {
                     *array = (MAXINT)str - strbuf;
                     array++;

                     str += StrCopy(k.c_str(), str, COPY_ALL) + 1;
                     str += StrCopy(v.c_str(), str, COPY_ALL) + 1;
                  }
               }
            }

            // Sorting

            array = (LONG *)(translate + 1);
            for (i=total/2; i >= 0; i--) sift((STRING)(array+total), array, i, total);

            LONG heapsize = total;
            for (i=heapsize; i > 0; i--) {
               auto temp = array[0];
               array[0] = array[i-1];
               array[i-1] = temp;
               sift((STRING)(array+total), array, 0, --heapsize);
            }

            // If in debug mode, print out any duplicate strings

            if (GetResource(RES_LOG_LEVEL) > 3) {
               array = (LONG *)(translate + 1);
               str = (STRING)(array + total);
               for (i=0; i < total-1; i++) {
                  if (!StrCompare(str+array[i], str+array[i+1], 0, STR_MATCH_LEN)) {
                     log.warning("Duplicate string \"%s\"", str+array[i]);
                  }
               }
            }

            // Update the global translation table

            if (glTranslate) {
               glTranslate->Replaced = TRUE;
               FreeResource(glTranslate);
               ReleaseMemory(glTranslate);
            }

            auto sharectl = (SharedControl *)GetResourcePtr(RES_SHARED_CONTROL);
            sharectl->TranslationMID = memoryid;
            glTranslate = translate;
            glTranslateMID = memoryid;
            acFree(config);

            return TRUE;
         }
         acFree(config);
      }
      else {
         // If there is no translation file for this language, revert to no translation table (which will give the user English).

         if (glTranslate) {
            glTranslate->Replaced = TRUE;
            ReleaseMemoryID(glTranslateMID);
            FreeResourceID(glTranslateMID);
            glTranslate = NULL;
            glTranslateMID = 0;
         }
         auto sharectl = (SharedControl *)GetResourcePtr(RES_SHARED_CONTROL);
         sharectl->TranslationMID = 0;
      }
   }

   return FALSE;
}

/*****************************************************************************

-FUNCTION-
StrTranslateText: Translates text from international English to the user's language.

This function converts International English to the user's preferred language.  The translation process is very
simple - a lookup table is loaded from `config:translations/code.cfg`, which holds conversion details for simple English
words and phrases into another language.  If the String is known, a new string is returned for the translated word(s).
If the String is not known, the original String address pointer is returned without a translation.

If a translation occurs, the resulting string pointer is temporary.  You will need to store the result in a local
buffer, or risk the string address becoming invalid on the next call to StrTranslateText().

-INPUT-
cstr String: A string of international English to translate.

-RESULT-
cstr: If the string was able to be translated, a translation is returned.  Otherwise the original String pointer is returned.

-END-

*****************************************************************************/

CSTRING StrTranslateText(CSTRING Text)
{
   parasol::Log log(__FUNCTION__);

   if (!Text) return Text;

   SharedControl *sharectl = (SharedControl *)GetResourcePtr(RES_SHARED_CONTROL);

   if ((!glTranslate) and (!sharectl->TranslationMID)) {
      if (glTranslateLoad IS FALSE) {
         glTranslateLoad = TRUE;
         if (StrTranslateRefresh() IS FALSE) return Text;
      }
      else return Text;
   }

   // Reload the translation table if it has been replaced with a new one

   if ((!glTranslate) or (glTranslate->Replaced)) {
      log.msg("Reloading the translation table.");
      if (glTranslate) {
         ReleaseMemoryID(glTranslateMID); // Memory is already marked for deletion, so should free itself on the final release
         glTranslate = NULL;
         glTranslateMID = 0;
      }

      if (AccessMemory(sharectl->TranslationMID, MEM_READ|MEM_NO_BLOCKING, 2000, (APTR *)&glTranslate) != ERR_Okay) {
         return Text;
      }
      else glTranslateMID = sharectl->TranslationMID;
   }

   // Scan the translation table for the word.  The array is alphabetically sorted, so this will be quick...

   LONG *array = (LONG *)(glTranslate + 1);
   STRING str = (STRING)(array + glTranslate->Total);

   CSTRING txt = Text;
   WORD pos = 0;
   LONG floor, ceiling, i;
restart:
   floor   = 0;
   ceiling = glTranslate->Total;
   i       = ceiling/2;
   while (1) {
      BYTE result = StrSortCompare(txt, str+array[i]);
      if (result < 0) {
         if (ceiling IS i) break;
         else ceiling = i;
      }
      else if (result > 0) {
         if (floor IS i) break;
         else floor = i;
      }
      else goto found;
      i = floor + ((ceiling - floor)>>1);
   }

   // Do a second search, this time cut off any non-alphabetic characters from the string being translated.

   if ((APTR)txt IS (APTR)Text) {
      for (pos=0; (Text[pos]) and
                  (((Text[pos] >= 'a') and (Text[pos] <= 'z')) or ((Text[pos] >= 'A') and (Text[pos] <= 'Z')) or
                   (Text[pos] IS ' ')); pos++);

      if (Text[pos]) {
         LONG j;
         for (j=0; (j < pos) and ((size_t)j < sizeof(glTranslateBuffer)-1); j++) glTranslateBuffer[j] = Text[j];
         glTranslateBuffer[j] = 0;
         txt = glTranslateBuffer;
         goto restart;
      }
   }

   return Text; // Return the original string

found:
   str = str + array[i];
   while (*str) str++;
   str++;

   LONG j;
   for (j=0; (str[j]) and ((size_t)j < sizeof(glTranslateBuffer)-1); j++) glTranslateBuffer[j] = str[j];

   if (txt IS glTranslateBuffer) { // Copy trailing non-alphabetic symbols
      while ((Text[pos]) and ((size_t)j < sizeof(glTranslateBuffer)-1)) glTranslateBuffer[j++] = Text[pos++];
   }

   glTranslateBuffer[j] = 0;

   // Check the capitalisation of the text

   if ((Text[0] >= 'a') and (Text[0] <= 'z')) { // All lower case
      for (i=0; glTranslateBuffer[i]; i++) {
         if ((glTranslateBuffer[i] >= 'A') and (glTranslateBuffer[i] <= 'Z')) glTranslateBuffer[i] = glTranslateBuffer[i] - 'A' + 'a';
      }
   }
   else if (((Text[0] >= 'A') and (Text[0] <= 'Z')) and ((Text[1] >= 'A') and (Text[1] <= 'A'))) {
      // All upper case
      for (i=0; glTranslateBuffer[i]; i++) {
         if ((glTranslateBuffer[i] >= 'a') and (glTranslateBuffer[i] <= 'z')) glTranslateBuffer[i] = glTranslateBuffer[i] - 'a' + 'A';
      }
   }

   return glTranslateBuffer;
}

/*****************************************************************************

-FUNCTION-
StrUpper: Changes a string so that all alpha characters are in upper-case.

This function will alter a String so that all upper-case characters are changed to lower-case.  Non lower-case
characters are unaffected by this function.  Here is an example:

<pre>"HeLLo world" = "HELLO WORD"</pre>

-INPUT-
buf(str) String: Pointer to a string.

-END-

*****************************************************************************/

void StrUpper(STRING String)
{
   if (!String) return;

   while (*String) {
      if ((*String >= 'a') and (*String <= 'z')) *String -= 0x20;
      String++;
   }
}

/*****************************************************************************
** Insert: The string to be inserted.
** Buffer: The start of the buffer region.
** Size:   The complete size of the target buffer.
** Pos:    The target position for the insert.
** ReplaceChars: If characters will be replaced, specify the number here.
**
** The only error that this function can return is a buffer overflow.
*/

ERROR insert_string(CSTRING Insert, STRING Buffer, LONG Size, LONG Pos, LONG Replace)
{
   LONG inlen, i, strlen, j;

   if (!Insert) Insert = "";

   for (inlen=0; Insert[inlen]; inlen++);

   // String insertion

   if (inlen < Replace) {
      // The string to insert is smaller than the number of characters to replace.

      i = Pos + StrCopy(Insert, Buffer+Pos, COPY_ALL);
      i = Pos + Replace;
      Pos += inlen;
      while (Buffer[i]) Buffer[Pos++] = Buffer[i++];
      Buffer[Pos] = 0;
   }
   else if (inlen IS Replace) {
      while (*Insert) Buffer[Pos++] = *Insert++;
   }
   else {
      // Check if an overflow will occur

      for (strlen=0; Buffer[strlen]; strlen++);

      if ((Size - 1) < (strlen - Replace + inlen)) {
         return ERR_BufferOverflow;
      }

      // Expand the string
      i = strlen + (inlen - Replace) + 1;
      strlen += 1;
      j = strlen-Pos-Replace+1;
      while (j > 0) {
         Buffer[i--] = Buffer[strlen--];
         j--;
      }

      for (i=0; i < inlen; i++, Pos++) Buffer[Pos] = Insert[i];
   }

   return ERR_Okay;
}

//****************************************************************************

static void sift(STRING Buffer, LONG *lookup, LONG i, LONG heapsize)
{
   LONG largest = i;
   do {
      i     = largest;
      LONG left  = (i << 1) + 1;
      LONG right = left + 1;

      if (left < heapsize){
         if (StrSortCompare(Buffer+lookup[largest], Buffer+lookup[left]) < 0) largest = left;

         if (right < heapsize) {
            if (StrSortCompare(Buffer+lookup[largest], Buffer+lookup[right]) < 0) largest = right;
         }
      }

      if (largest != i) {
         auto temp = lookup[i];
         lookup[i] = lookup[largest];
         lookup[largest] = temp;
      }
   } while (largest != i);
}

//****************************************************************************

#include "lib_base64.cpp"
#include "lib_conversion.cpp"
#include "lib_unicode.cpp"
