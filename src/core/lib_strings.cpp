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

/*********************************************************************************************************************

-FUNCTION-
StrHash: Convert a string into a 32-bit hash.

This function will convert a string into a 32-bit hash.  The hashing algorithm is consistent throughout our
platform and is therefore guaranteed to be compatible with all areas that make use of hashed values.

Hashing is case insensitive by default.  If case sensitive hashing is desired, set `CaseSensitive` to `true`
when calling this function.  Please keep in mind that a case sensitive hash value will not be interchangeable with a
case insensitive hash of the same string.

-INPUT-
cstr String: Reference to a string that will be processed.
int CaseSensitive: Set to `true` to enable case sensitivity.

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
