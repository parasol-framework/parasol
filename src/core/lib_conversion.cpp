/****************************************************************************

The source code of the Parasol Framework is made publicly available under the
terms described in the LICENSE.TXT file that is distributed with this package.
Please refer to it for further information on licensing.

-CATEGORY-
Name: Strings
-END-

****************************************************************************/

/*****************************************************************************

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

*****************************************************************************/

ERROR StrReadLocale(CSTRING Key, CSTRING *Value)
{
   parasol::Log log(__FUNCTION__);

   if ((!Key) or (!Value)) return ERR_NullArgs;

   #ifdef __ANDROID__
      // Android doesn't have locale.cfg, we have to load that information from the system.

      if (!StrMatch("Language", Key)) {
         static char code[4] = { 0, 0, 0, 0 };
         if (!code[0]) {
            if (!AndroidBase) {  // Note that the module is terminated through resource tracking, we don't free it during our CMDExpunge() sequence for system integrity reasons.
               parasol::SwitchContext ctx(CurrentTask());
               OBJECTPTR module;
               LoadModule("android", MODVERSION_FLUID, &module, &AndroidBase);
               if (!AndroidBase) return NULL;
            }

            AConfiguration *config;
            if (!adGetConfig(&config)) {
               AConfiguration_getLanguage(config, code);

               // Convert the two letter code to three letters.

               if (code[0]) {
                  code[0] = LCASE(code[0]);
                  code[1] = LCASE(code[1]);
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
      if (!CreateObject(ID_CONFIG, NF_UNTRACKED, &glLocale, FID_Path|TSTR, "user:config/locale.cfg", TAGEND)) {
      }
   }

   if (!glLocale) return ERR_NoData;

   if (!cfgReadValue(glLocale, "LOCALE", Key, Value)) {
      if (!*Value) *Value = ""; // It is OK for some values to be empty strings.
      return ERR_Okay;
   }
   else return ERR_Search;
}

/*****************************************************************************

-FUNCTION-
StrToColour: Converts a colour string into an RGB8 value structure.

This function converts a colour from its string format to equivalent red, green, blue and alpha values.  The colour
that is referenced must be in hexadecimal or separated-decimal format.  For example a pure red colour may be expressed
as a string of `#ff0000` or `255,0,0`.

-INPUT-
cstr Colour: Pointer to a string containing the colour.
buf(struct(RGB8)) RGB: Pointer to an RGB8 structure that will receive the colour values.

-ERRORS-
Okay
NullArgs
Syntax

*****************************************************************************/

INLINE char read_nibble(CSTRING Str)
{
   if ((*Str >= '0') and (*Str <= '9')) return (*Str - '0');
   else if ((*Str >= 'A') and (*Str <= 'F')) return ((*Str - 'A')+10);
   else if ((*Str >= 'a') and (*Str <= 'f')) return ((*Str - 'a')+10);
   else return -1;
}

ERROR StrToColour(CSTRING Colour, struct RGB8 *RGB)
{
   if ((!Colour) or (!RGB)) return ERR_NullArgs;

   if (*Colour IS '#') {
      Colour++;
      char nibbles[8];
      UBYTE n = 0;
      while ((*Colour) and (n < ARRAYSIZE(nibbles))) nibbles[n++] = read_nibble(Colour++);

      if (n IS 3) {
         RGB->Red   = nibbles[0]<<4;
         RGB->Green = nibbles[1]<<4;
         RGB->Blue  = nibbles[2]<<4;
         RGB->Alpha = 255;
      }
      else if (n IS 6) {
         RGB->Red   = (nibbles[0]<<4) | nibbles[1];
         RGB->Green = (nibbles[2]<<4) | nibbles[3];
         RGB->Blue  = (nibbles[4]<<4) | nibbles[5];
         RGB->Alpha = 255;
      }
      else if (n IS 8) {
         RGB->Red   = (nibbles[0]<<4) | nibbles[1];
         RGB->Green = (nibbles[2]<<4) | nibbles[3];
         RGB->Blue  = (nibbles[4]<<4) | nibbles[5];
         RGB->Alpha = (nibbles[6]<<4) | nibbles[7];
      }
      else return ERR_Syntax;
   }
   else {
      RGB->Red = StrToInt(Colour);
      while ((*Colour) and (*Colour != ',')) { Colour++; if (*Colour IS '%') RGB->Red = (RGB->Red * 255) / 100; } if (*Colour) Colour++;
      RGB->Green = StrToInt(Colour);
      while ((*Colour) and (*Colour != ',')) { Colour++; if (*Colour IS '%') RGB->Green = (RGB->Green * 255) / 100; } if (*Colour) Colour++;
      RGB->Blue = StrToInt(Colour);
      while ((*Colour) and (*Colour != ',')) { Colour++; if (*Colour IS '%') RGB->Blue = (RGB->Blue * 255) / 100; } if (*Colour) Colour++;
      while ((*Colour) and (*Colour <= 0x20)) Colour++;
      if (*Colour) {
         RGB->Alpha = StrToInt(Colour);
         while ((*Colour >= '0') and (*Colour <= '9')) Colour++;
         if (*Colour IS '%') RGB->Alpha = (RGB->Alpha * 255) / 100;
      }
      else RGB->Alpha = 255;
   }
   return ERR_Okay;
}

/*****************************************************************************

-FUNCTION-
StrToFloat: Converts strings to floating point numbers.

This function converts strings into 64-bit floating point numbers.  It supports negative numbers (if a minus sign is at
the front) and skips leading spaces and non-numeric characters that occur before any digits.

If the function encounters a non-numeric character once it has started its number crunching, it immediately stops and
returns the value that has been calculated up to that point.

-INPUT-
cstr String: Pointer to the string that is to be converted to a floating point number.

-RESULT-
double: Returns the floating point value that was calculated from the String.

*****************************************************************************/

DOUBLE StrToFloat(CSTRING String)
{
   if (!String) return 0;

   // Ignore any leading characters

   while ((*String != '-') and (*String != '.') and ((*String < '0') or (*String > '9'))) {
      if (!*String) return 0;
      String++;
   }

   return strtod(String, NULL);
}

/*****************************************************************************

-FUNCTION-
StrToHex: Converts a string from printed hex to a number.

This function converts a String to its hex-integer equivalent.  It will skip leading junk characters until it
encounters a valid hex string and converts it.

If the function encounters a non-numeric character before the end of the string is reached, it returns the result
calculated up to that point.

Here are some string conversion examples:

<list type="Custom">
<b><li value="String">Result</></b>
<li value="$183">183</>
<li value="..$2902z6">2902</>
<li value="hx239">239</>
<li value="0xffe8">0xffe8</>
<li value=".009ab">0x9ab</>
</>

-INPUT-
cstr String: Pointer to the string that is to be converted to a hex-integer.

-RESULT-
large: Returns the value that was calculated from the String.

*****************************************************************************/

LARGE StrToHex(CSTRING String)
{
   if (!String) return 0;

   while (*String) {
      if ((*String >= '0') and (*String <= '9')) break;
      if ((*String >= 'A') and (*String <= 'F')) break;
      if ((*String >= 'a') and (*String <= 'f')) break;
      if (*String IS '$') break;
      if (*String IS '#') break;
      String++;
   }

   if ((String[0] IS '0') and ((String[1] IS 'X') or (String[1] IS 'x'))) String += 2;
   else if (String[0] IS '$') String++;
   else if (String[0] IS '#') String++;

   LARGE result = 0;
   while (*String) {
      if ((*String >= '0') and (*String <= '9')) {
         result <<= 4;
         result += *String - '0';
      }
      else if ((*String >= 'a') and (*String <= 'f')) {
         result <<= 4;
         result += *String - 'a' + 10;
      }
      else if ((*String >= 'A') and (*String <= 'F')) {
         result <<= 4;
         result += *String - 'A' + 10;
      }
      else break;

      String++;
   }

   return result;
}

/*****************************************************************************

-FUNCTION-
StrToInt: Converts a string to an integer.

This function converts a String to its integer equivalent.  It supports negative numbers (if a minus sign is at the
front) and skips leading spaces and non-numeric characters that occur before any digits.

If the function encounters a non-numeric character once it has started its digit processing, it immediately stops and
returns the result calculated up to that point.

Here are some string conversion examples:

<list type="custom">
<b><li value="String">Result</li></b>
<li value="183">183</>
<li value=",,2902a6">2902</>
<li value="hx239">239</>
<li value="-45">-45</>
<li value=".jff-9">-9</>
</>

-INPUT-
cstr String: Pointer to the string that is to be converted to an integer.

-RESULT-
large: Returns the integer value that was calculated from the String.
-END-

*****************************************************************************/

LARGE StrToInt(CSTRING String)
{
   if (!String) return 0;

   while ((*String < '0') or (*String > '9')) { // Ignore any leading characters
      if (!String[0]) return 0;
      else if (*String IS '-') break;
      else if (*String IS '+') break;
      else String++;
   }

   return strtol(String, NULL, 10);
}
