/*********************************************************************************************************************
-CATEGORY-
Name: Strings
-END-
*********************************************************************************************************************/

#include "defs.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#ifdef __ANDROID__
#include <android/configuration.h>
#include <parasol/modules/android.h>
#endif

typedef void * iconv_t;
iconv_t (*iconv_open)(const char* tocode, const char* fromcode);
size_t  (*iconv)(iconv_t cd, const char** inbuf, size_t* inbytesleft,   char** outbuf, size_t* outbytesleft);
int     (*iconv_close)(iconv_t cd);
void    (*iconvlist)(int (*do_one)(unsigned int namescount, const char* const* names, void* data), void* data);

STRING glIconvBuffer = NULL;
OBJECTPTR modIconv = NULL;
static iconv_t glIconv = NULL;

//********************************************************************************************************************

#ifdef __ANDROID__
struct LanguageCode {
   const char Two[2];
   const char Three[3];
   CSTRING Name;
};

static const struct LanguageCode glLanguages[] = {
   { { 'a','b' }, { 'a','b','k' }, "Abkhaz" },
   { { 'a','a' }, { 'a','a','r' }, "Afar" },
   { { 'a','f' }, { 'a','f','r' }, "Afrikaans" },
   { { 'a','k' }, { 'a','k','a' }, "Akan" },
   { { 's','q' }, { 's','q','i' }, "Albanian" },
   { { 'a','m' }, { 'a','m','h' }, "Amharic" },
   { { 'a','r' }, { 'a','r','a' }, "Arabic" },
   { { 'a','n' }, { 'a','r','g' }, "Aragonese" },
   { { 'h','y' }, { 'h','y','e' }, "Armenian" },
   { { 'a','s' }, { 'a','s','m' }, "Assamese" },
   { { 'a','v' }, { 'a','v','a' }, "Avaric" },
   { { 'a','e' }, { 'a','v','e' }, "Avestan" },
   { { 'a','y' }, { 'a','y','m' }, "Aymara" },
   { { 'a','z' }, { 'a','z','e' }, "Azerbaijani" },
   { { 'b','m' }, { 'b','a','m' }, "Bambara" },
   { { 'b','a' }, { 'b','a','k' }, "Bashkir" },
   { { 'e','u' }, { 'e','u','s' }, "Basque" },
   { { 'b','e' }, { 'b','e','l' }, "Belarusian" },
   { { 'b','n' }, { 'b','e','n' }, "Bengali" },
   { { 'b','h' }, { 'b','i','h' }, "Bihari" },
   { { 'b','i' }, { 'b','i','s' }, "Bislama" },
   { { 'b','s' }, { 'b','o','s' }, "Bosnian" },
   { { 'b','r' }, { 'b','r','e' }, "Breton" },
   { { 'b','g' }, { 'b','u','l' }, "Bulgarian" },
   { { 'm','y' }, { 'm','y','a' }, "Burmese" },
   { { 'c','a' }, { 'c','a','t' }, "Catalan" },
   { { 'c','h' }, { 'c','h','a' }, "Chamorro" },
   { { 'c','e' }, { 'c','h','e' }, "Chechen" },
   { { 'n','y' }, { 'n','y','a' }, "Chichewa" },
   { { 'z','h' }, { 'z','h','o' }, "Chinese" },
   { { 'c','v' }, { 'c','h','v' }, "Chuvash" },
   { { 'k','w' }, { 'c','o','r' }, "Cornish" },
   { { 'c','o' }, { 'c','o','s' }, "Corsican" },
   { { 'c','r' }, { 'c','r','e' }, "Cree" },
   { { 'h','r' }, { 'h','r','v' }, "Croatian" },
   { { 'c','s' }, { 'c','e','s' }, "Czech" },
   { { 'd','a' }, { 'd','a','n' }, "Danish" },
   { { 'd','v' }, { 'd','i','v' }, "Divehi" },
   { { 'n','l' }, { 'n','l','d' }, "Dutch" },
   { { 'd','z' }, { 'd','z','o' }, "Dzongkha" },
   { { 'e','n' }, { 'e','n','g' }, "English" },
   { { 'e','o' }, { 'e','p','o' }, "Esperanto" },
   { { 'e','t' }, { 'e','s','t' }, "Estonian" },
   { { 'e','e' }, { 'e','w','e' }, "Ewe" },
   { { 'f','o' }, { 'f','a','o' }, "Faroese" },
   { { 'f','j' }, { 'f','i','j' }, "Fijian" },
   { { 'f','i' }, { 'f','i','n' }, "Finnish" },
   { { 'f','r' }, { 'f','r','a' }, "French" },
   { { 'f','f' }, { 'f','u','l' }, "Fula" },
   { { 'g','l' }, { 'g','l','g' }, "Galician" },
   { { 'k','a' }, { 'k','a','t' }, "Georgian" },
   { { 'd','e' }, { 'd','e','u' }, "German" },
   { { 'e','l' }, { 'e','l','l' }, "Greek" },
   { { 'g','n' }, { 'g','r','n' }, "Guaraní" },
   { { 'g','u' }, { 'g','u','j' }, "Gujarati" },
   { { 'h','t' }, { 'h','a','t' }, "Haitian" },
   { { 'h','a' }, { 'h','a','u' }, "Hausa" },
   { { 'h','e' }, { 'h','e','b' }, "Hebrew" },
   { { 'h','z' }, { 'h','e','r' }, "Herero" },
   { { 'h','i' }, { 'h','i','n' }, "Hindi" },
   { { 'h','o' }, { 'h','m','o' }, "Hiri Motu" },
   { { 'h','u' }, { 'h','u','n' }, "Hungarian" },
   { { 'i','a' }, { 'i','n','a' }, "Interlingua" },
   { { 'i','d' }, { 'i','n','d' }, "Indonesian" },
   { { 'i','e' }, { 'i','l','e' }, "Interlingue" },
   { { 'g','a' }, { 'g','l','e' }, "Irish" },
   { { 'i','g' }, { 'i','b','o' }, "Igbo" },
   { { 'i','k' }, { 'i','p','k' }, "Inupiaq" },
   { { 'i','o' }, { 'i','d','o' }, "Ido" },
   { { 'i','s' }, { 'i','s','l' }, "Icelandic" },
   { { 'i','t' }, { 'i','t','a' }, "Italian" },
   { { 'i','u' }, { 'i','k','u' }, "Inuktitut" },
   { { 'j','a' }, { 'j','p','n' }, "Japanese" },
   { { 'j','v' }, { 'j','a','v' }, "Javanese" },
   { { 'k','l' }, { 'k','a','l' }, "Kalaallisut" },
   { { 'k','n' }, { 'k','a','n' }, "Kannada" },
   { { 'k','r' }, { 'k','a','u' }, "Kanuri" },
   { { 'k','s' }, { 'k','a','s' }, "Kashmiri" },
   { { 'k','k' }, { 'k','a','z' }, "Kazakh" },
   { { 'k','m' }, { 'k','h','m' }, "Khmer" },
   { { 'k','i' }, { 'k','i','k' }, "Kikuyu" },
   { { 'r','w' }, { 'k','i','n' }, "Kinyarwanda" },
   { { 'k','y' }, { 'k','i','r' }, "Kyrgyz" },
   { { 'k','v' }, { 'k','o','m' }, "Komi" },
   { { 'k','g' }, { 'k','o','n' }, "Kongo" },
   { { 'k','o' }, { 'k','o','r' }, "Korean" },
   { { 'k','u' }, { 'k','u','r' }, "Kurdish" },
   { { 'k','j' }, { 'k','u','a' }, "Kwanyama" },
   { { 'l','a' }, { 'l','a','t' }, "Latin" },
   { { 'l','b' }, { 'l','t','z' }, "Luxembourgish" },
   { { 'l','g' }, { 'l','u','g' }, "Ganda" },
   { { 'l','i' }, { 'l','i','m' }, "Limburgish" },
   { { 'l','n' }, { 'l','i','n' }, "Lingala" },
   { { 'l','o' }, { 'l','a','o' }, "Lao" },
   { { 'l','t' }, { 'l','i','t' }, "Lithuanian" },
   { { 'l','u' }, { 'l','u','b' }, "Luba-Katanga" },
   { { 'l','v' }, { 'l','a','v' }, "Latvian" },
   { { 'g','v' }, { 'g','l','v' }, "Manx" },
   { { 'm','k' }, { 'm','k','d' }, "Macedonian" },
   { { 'm','g' }, { 'm','l','g' }, "Malagasy" },
   { { 'm','s' }, { 'm','s','a' }, "Malay" },
   { { 'm','l' }, { 'm','a','l' }, "Malayalam" },
   { { 'm','t' }, { 'm','l','t' }, "Maltese" },
   { { 'm','i' }, { 'm','r','i' }, "Māori" },
   { { 'm','r' }, { 'm','a','r' }, "Marathi" },
   { { 'm','h' }, { 'm','a','h' }, "Marshallese" },
   { { 'm','n' }, { 'm','o','n' }, "Mongolian" },
   { { 'n','a' }, { 'n','a','u' }, "Nauru" },
   { { 'n','v' }, { 'n','a','v' }, "Navajo" },
   { { 'n','b' }, { 'n','o','b' }, "Norwegian Bokmål" },
   { { 'n','d' }, { 'n','d','e' }, "North Ndebele" },
   { { 'n','e' }, { 'n','e','p' }, "Nepali" },
   { { 'n','g' }, { 'n','d','o' }, "Ndonga" },
   { { 'n','n' }, { 'n','n','o' }, "Norwegian Nynorsk" },
   { { 'n','o' }, { 'n','o','r' }, "Norwegian" },
   { { 'i','i' }, { 'i','i','i' }, "Nuosu" },
   { { 'n','r' }, { 'n','b','l' }, "South Ndebele" },
   { { 'o','c' }, { 'o','c','i' }, "Occitan" },
   { { 'o','j' }, { 'o','j','i' }, "Ojibwe" },
   { { 'c','u' }, { 'c','h','u' }, "Old Church Slavonic" },
   { { 'o','m' }, { 'o','r','m' }, "Oromo" },
   { { 'o','r' }, { 'o','r','i' }, "Oriya" },
   { { 'o','s' }, { 'o','s','s' }, "Ossetian" },
   { { 'p','a' }, { 'p','a','n' }, "Panjabi" },
   { { 'p','i' }, { 'p','l','i' }, "Pāli" },
   { { 'f','a' }, { 'f','a','s' }, "Persian" },
   { { 'p','l' }, { 'p','o','l' }, "Polish" },
   { { 'p','s' }, { 'p','u','s' }, "Pashto" },
   { { 'p','t' }, { 'p','o','r' }, "Portuguese" },
   { { 'q','u' }, { 'q','u','e' }, "Quechua" },
   { { 'r','m' }, { 'r','o','h' }, "Romansh" },
   { { 'r','n' }, { 'r','u','n' }, "Kirundi" },
   { { 'r','o' }, { 'r','o','n' }, "Romanian" },
   { { 'r','u' }, { 'r','u','s' }, "Russian" },
   { { 's','a' }, { 's','a','n' }, "Sanskrit" },
   { { 's','c' }, { 's','r','d' }, "Sardinian" },
   { { 's','d' }, { 's','n','d' }, "Sindhi" },
   { { 's','e' }, { 's','m','e' }, "Northern Sami" },
   { { 's','m' }, { 's','m','o' }, "Samoan" },
   { { 's','g' }, { 's','a','g' }, "Sango" },
   { { 's','r' }, { 's','r','p' }, "Serbian" },
   { { 'g','d' }, { 'g','l','a' }, "Gaelic" },
   { { 's','n' }, { 's','n','a' }, "Shona" },
   { { 's','i' }, { 's','i','n' }, "Sinhala" },
   { { 's','k' }, { 's','l','k' }, "Slovak" },
   { { 's','l' }, { 's','l','v' }, "Slovene" },
   { { 's','o' }, { 's','o','m' }, "Somali" },
   { { 's','t' }, { 's','o','t' }, "Southern Sotho" },
   { { 'a','z' }, { 'a','z','b' }, "South Azerbaijani" },
   { { 'e','s' }, { 's','p','a' }, "Spanish" },
   { { 's','u' }, { 's','u','n' }, "Sundanese" },
   { { 's','w' }, { 's','w','a' }, "Swahili" },
   { { 's','s' }, { 's','s','w' }, "Swati" },
   { { 's','v' }, { 's','w','e' }, "Swedish" },
   { { 't','a' }, { 't','a','m' }, "Tamil" },
   { { 't','e' }, { 't','e','l' }, "Telugu" },
   { { 't','g' }, { 't','g','k' }, "Tajik" },
   { { 't','h' }, { 't','h','a' }, "Thai" },
   { { 't','i' }, { 't','i','r' }, "Tigrinya" },
   { { 'b','o' }, { 'b','o','d' }, "Tibetan" },
   { { 't','k' }, { 't','u','k' }, "Turkmen" },
   { { 't','l' }, { 't','g','l' }, "Tagalog" },
   { { 't','n' }, { 't','s','n' }, "Tswana" },
   { { 't','o' }, { 't','o','n' }, "Tonga" },
   { { 't','r' }, { 't','u','r' }, "Turkish" },
   { { 't','s' }, { 't','s','o' }, "Tsonga" },
   { { 't','t' }, { 't','a','t' }, "Tatar" },
   { { 't','w' }, { 't','w','i' }, "Twi" },
   { { 't','y' }, { 't','a','h' }, "Tahitian" },
   { { 'u','g' }, { 'u','i','g' }, "Uyghur" },
   { { 'u','k' }, { 'u','k','r' }, "Ukrainian" },
   { { 'u','r' }, { 'u','r','d' }, "Urdu" },
   { { 'u','z' }, { 'u','z','b' }, "Uzbek" },
   { { 'v','e' }, { 'v','e','n' }, "Venda" },
   { { 'v','i' }, { 'v','i','e' }, "Vietnamese" },
   { { 'v','o' }, { 'v','o','l' }, "Volapük" },
   { { 'w','a' }, { 'w','l','n' }, "Walloon" },
   { { 'c','y' }, { 'c','y','m' }, "Welsh" },
   { { 'w','o' }, { 'w','o','l' }, "Wolof" },
   { { 'f','y' }, { 'f','r','y' }, "Western Frisian" },
   { { 'x','h' }, { 'x','h','o' }, "Xhosa" },
   { { 'y','i' }, { 'y','i','d' }, "Yiddish" },
   { { 'y','o' }, { 'y','o','r' }, "Yoruba" },
   { { 'z','a' }, { 'z','h','a' }, "Zhuang" },
   { { 'z','u' }, { 'z','u','l' }, "Zulu" }
};
#endif

//********************************************************************************************************************

void free_iconv(void)
{
   if (modIconv) {
      if (glIconv) { iconv_close(glIconv); glIconv = NULL; }
      if (glIconvBuffer) { FreeResource(glIconvBuffer); glIconvBuffer = NULL; }

      acFree(modIconv);
      modIconv = NULL;
   }
}

//********************************************************************************************************************

static LONG str_cmp(CSTRING Name1, CSTRING Name2)
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

//********************************************************************************************************************

static ERROR str_sort(CSTRING *List, LONG Flags)
{
   if (!List) return ERR_NullArgs;

   // Shell sort.  Similar to bubble sort but much faster because it can copy records over larger distances.

   LONG total, j;

   for (total=0; List[total]; total++);

   LONG h = 1;
   while (h < total / 9) h = 3 * h + 1;

   if (Flags & SBF_DESC) {
      for (; h > 0; h /= 3) {
         for (LONG i=h; i < total; i++) {
            auto temp = List[i];
            for (j=i; (j >= h) and (str_cmp(List[j - h], temp) < 0); j -= h) {
               List[j] = List[j - h];
            }
            List[j] = temp;
         }
      }
   }
   else {
      for (; h > 0; h /= 3) {
         for (LONG i=h; i < total; i++) {
            auto temp = List[i];
            for (j=i; (j >= h) and (str_cmp(List[j - h], temp) > 0); j -= h) {
               List[j] = List[j - h];
            }
            List[j] = temp;
         }
      }
   }

   if (Flags & SBF_NO_DUPLICATES) {
      LONG strflags = STR_MATCH_LEN;
      if (Flags & SBF_CASE) strflags |= STR_MATCH_CASE;

      for (LONG i=1; List[i]; i++) {
         if (!StrCompare(List[i-1], List[i], 0, strflags)) {
            for (j=i; List[j]; j++) List[j] = List[j+1];
            i--;
         }
      }
   }

   return ERR_Okay;
}

/*********************************************************************************************************************

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

*********************************************************************************************************************/

STRING * StrBuildArray(STRING List, LONG Size, LONG Total, LONG Flags)
{
   pf::Log log(__FUNCTION__);

   if (!List) return NULL;

   LONG i;
   char *csvbuffer_alloc = NULL;
   char csvbuffer[1024]= { 0 };
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
            str_sort(array, 0);
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
         else if (Flags & SBF_SORT) str_sort(array, 0);

         if (csvbuffer_alloc) free(csvbuffer_alloc);
         return (STRING *)array;
      }
   }

   if (csvbuffer_alloc) free(csvbuffer_alloc);
   return NULL;
}

/*********************************************************************************************************************

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

*********************************************************************************************************************/

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

/*********************************************************************************************************************

-FUNCTION-
StrDatatype: Determines the data type of a string.

This function analyses a string and returns its data type.  Valid return values are `STT_FLOAT` for floating point
numbers, `STT_NUMBER` for whole numbers, `STT_HEX` for hexadecimal (e.g. 0x1) and `STT_STRING` for any other string type.
In order for the string to be recognised as one of the number types, it must be limited to numbers and qualification
characters, such as a decimal point or negative sign.

Any white-space at the start of the string will be skipped.

-INPUT-
cstr String: The string that you want to analyse.

-RESULT-
int(STT): Returns STT_FLOAT, STT_NUMBER, STT_HEX or STT_STRING.

*********************************************************************************************************************/

LONG StrDatatype(CSTRING String)
{
   if (!String) return 0;

   while ((*String) and (*String <= 0x20)) String++; // Skip white-space

   LONG i;
   if ((String[0] IS '0') and (String[1] IS 'x')) {
      for (i=2; String[i]; i++) {
         if (((String[i] >= '0') and (String[i] <= '9')) or
             ((String[i] >= 'A') and (String[i] <= 'F')) or
             ((String[i] >= 'a') and (String[i] <= 'f')));
         else return STT_STRING;
      }
      return STT_HEX;
   }

   bool is_number = true;
   bool is_float  = false;

   for (i=0; (String[i]) and (is_number); i++) {
      if (((String[i] < '0') or (String[i] > '9')) and (String[i] != '.') and (String[i] != '-')) is_number = false;
      if (String[i] IS '.') is_float = true;
   }

   if ((is_float) and (is_number)) return STT_FLOAT;
   else if (is_number) return STT_NUMBER;
   else return STT_STRING;
}

/*********************************************************************************************************************

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

*********************************************************************************************************************/

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

/*********************************************************************************************************************

-FUNCTION-
StrReadLocale: Read system locale information.

Use this function to read system-wide locale information.  Settings are usually preset according to the user's location,
but the user also has the power to override individual key value.  The internal nature of this function varies by host
system.  If locale information is not readily available then the locale values will be derived from
`user:config/locale.cfg`.

Available key values are as follows:

<types>
<type name="Language">Three letter ISO code indicating the user's preferred language, e.g. 'eng'</>
<type name="ShortDate">Short date format, e.g. 'dd/mm/yyyy'</>
<type name="LongDate">Long date format, e.g. 'Dddd, d Mmm yyyy'</>
<type name="FileDate">File date format, e.g. 'dd-mm-yy hh:nn'</>
<type name="Time">Basic time format, e.g. hh:nn</>
<type name="CurrencySymbol">Currency symbol, e.g. '$'</>
<type name="Decimal">Decimal place symbol, e.g. '.'</>
<type name="Thousands">Thousands symbol, e.g. ','</>
<type name="Positive">Positive symbol - typically blank or '+'</>
<type name="Negative">Negative symbol, e.g. '-'</>
</types>

-INPUT-
cstr Key: The name of a locale value to read.
&cstr Value: A pointer to the retrieved string value will be returned in this parameter.

-ERRORS-
Okay: Value retrieved.
NullArgs: At least one required argument was not provided.
Search: The Key value was not recognised.
NoData: Locale information is not available.

*********************************************************************************************************************/

ERROR StrReadLocale(CSTRING Key, CSTRING *Value)
{
   pf::Log log(__FUNCTION__);

   if ((!Key) or (!Value)) return ERR_NullArgs;

   #ifdef __ANDROID__
      // Android doesn't have locale.cfg, we have to load that information from the system.

      if (!StrMatch("Language", Key)) {
         static char code[4] = { 0, 0, 0, 0 };
         if (!code[0]) {
            if (!AndroidBase) {  // Note that the module is terminated through resource tracking, we don't free it during our CMDExpunge() sequence for system integrity reasons.
               pf::SwitchContext ctx(CurrentTask());
               OBJECTPTR module;
               objModule::load("android", MODVERSION_FLUID, &module, &AndroidBase);
               if (!AndroidBase) return NULL;
            }

            AConfiguration *config;
            if (!adGetConfig(&config)) {
               AConfiguration_getLanguage(config, code);

               // Convert the two letter code to three letters.

               if (code[0]) {
                  code[0] = std::tolower(code[0]);
                  code[1] = std::tolower(code[1]);
                  for (LONG i=0; i < ARRAYSIZE(glLanguages); i++) {
                     if ((glLanguages[i].Two[0] IS code[0]) and (glLanguages[i].Two[1] IS code[1])) {
                        code[0] = glLanguages[i].Three[0];
                        code[1] = glLanguages[i].Three[1];
                        code[2] = glLanguages[i].Three[2];
                        code[3] = 0;
                        break;
                     }
                  }
               }
            }
         }

         log.msg("Android language code: %s", code);

         if (code[0]) { *Value = code; return ERR_Okay; }
         else return ERR_Failed;
      }
   #endif

   if (!glLocale) {
      if (!(glLocale = objConfig::create::untracked(fl::Path("user:config/locale.cfg")))) {
         return ERR_NoData;
      }
   }

   if (!cfgReadValue(glLocale, "LOCALE", Key, Value)) {
      if (!*Value) *Value = ""; // It is OK for some values to be empty strings.
      return ERR_Okay;
   }
   else return ERR_Search;
}

/*********************************************************************************************************************

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

*********************************************************************************************************************/

LONG StrSearch(CSTRING Keyword, CSTRING String, LONG Flags)
{
   if ((!String) or (!Keyword)) {
      pf::Log log(__FUNCTION__);
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
         for (i=0; Keyword[i]; i++) if (std::toupper(String[pos+i]) != std::toupper(Keyword[i])) break;
         if (!Keyword[i]) return pos;
         for (++pos; (String[pos] & 0xc0) IS 0x80; pos++);
      }
   }

   return -1;
}

//********************************************************************************************************************

#include "lib_base64.cpp"
#include "lib_unicode.cpp"
