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
