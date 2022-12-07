/****************************************************************************

The source code of the Parasol Framework is made publicly available under the
terms described in the LICENSE.TXT file that is distributed with this package.
Please refer to it for further information on licensing.

-CATEGORY-
Name: Strings
-END-

****************************************************************************/

static void read_ordering(char *);

#define EPOCH_YR        1970
#define SECS_DAY        (24 * 60 * 60)
#define LEAPYEAR(year)  (!((year) % 4) && (((year) % 100) || !((year) % 400)))

struct datepart {
   CSTRING String;
   WORD Number;
   BYTE Type;
};

#define DP_DAY   0x01
#define DP_MONTH 0x02
#define DP_YEAR  0x04

static const UBYTE _ytab[2][12] = { { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 }, { 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 } };

// Result: 0 = Sunday, 6 = Saturday
/*
static LONG WEEKDAY(struct DateTime *Time)
{
   LONG a, y, m;
   a = (14 - Time->Month) / 12;
   y = Time->Year - a;
   m = Time->Month + 12 * a - 2;
   return (Time->Day + y + (y / 4) - (y / 100) + (y / 400) + (31 * m) / 12) % 7;
}
*/
static void epoch_to_datetime(LARGE Seconds, struct DateTime *Result)
{
   LONG dayclock, dayno;
   LONG year = EPOCH_YR;
   #define YEARSIZE(year)  (LEAPYEAR(year) ? 366 : 365)

   dayclock = Seconds % SECS_DAY;
   dayno    = Seconds / SECS_DAY;

   Result->Second = dayclock % 60;
   Result->Minute = (dayclock % 3600) / 60;
   Result->Hour   = dayclock / 3600;

   while (dayno >= YEARSIZE(year)) {
      dayno -= YEARSIZE(year);
      year++;
   }
   Result->Year = year;
   Result->Month = 0;
   while (dayno >= _ytab[LEAPYEAR(year)][Result->Month]) {
      dayno -= _ytab[LEAPYEAR(year)][Result->Month];
      Result->Month++;
   }
   Result->Month++;
   Result->Day = dayno + 1;

   //LogF("epoch_to_datetime", PF64() " = %d-%d-%d", Seconds, Result->Year, Result->Month, Result->Day);
}

static LARGE datetime_to_epoch(struct DateTime *DateTime)
{
   LONG year, month;
   LARGE seconds;
   UBYTE leap;

   seconds = 0;
   year = EPOCH_YR;
   while (year < DateTime->Year) {
      if (LEAPYEAR(year)) seconds += 366 * SECS_DAY;
      else seconds += 365 * SECS_DAY;
      year++;
   }

   leap = LEAPYEAR(DateTime->Year);
   month = 1;
   while (month < DateTime->Month) {
      seconds += _ytab[leap][month-1] * SECS_DAY;
      month++;
   }

   seconds += (DateTime->Day-1) * SECS_DAY;
   seconds += DateTime->Hour * 60 * 60;
   seconds += DateTime->Minute * 60;
   seconds += DateTime->Second;

   //LogF("@datetime_to_epoch()","%d-%d-%d = " PF64(), DateTime->Year, DateTime->Month, DateTime->Day, seconds);

   return seconds;
}

static void refresh_locale(void)
{
   if (glLocale) { acFree(glLocale); glLocale = NULL; }
}

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

//****************************************************************************
// Internal: read_ordering()

static void read_ordering(char *ordering_out)
{
   parasol::Log log(__FUNCTION__);

   static UBYTE ordering[4] = { 0, 0, 0, 0 }; /*eg "dmy" or"mdy"*/

   if (ordering[0] != 0) { /*if config has already been loaded in...*/

   }
   else {
      CSTRING str;
      LONG stage;
      BYTE seen_y, seen_m, seen_d, ordering_loaded;

      stage = 0;
      seen_y = FALSE;
      seen_m = FALSE;
      seen_d = FALSE;
      ordering_loaded = FALSE;

      if (!StrReadLocale("ShortDate", &str)) {
         for (; (*str) and (stage < 3); str++) {
            if(((*str IS 'y') or (*str IS 'Y')) and (!seen_y)) {
               ordering[stage++] = DP_YEAR;
               seen_y = TRUE;
            }
            else if (((*str IS 'm') or (*str IS 'M')) and (!seen_m)) {
               ordering[stage++] = DP_MONTH;
               seen_m = TRUE;
            }
            else if (((*str IS 'd') or (*str IS 'D')) and (!seen_d)) {
               ordering[stage++] = DP_DAY;
               seen_d = TRUE;
            }
         }

         if ((seen_y) and (seen_m) and (seen_d)) {
            log.msg("Date ordering loaded: %s", ordering);
            ordering_loaded = TRUE;
         }
      }

      if (!ordering_loaded) {
         // Failed to read short date from locale config, so set a default ordering.

         ordering[0] = DP_DAY;
         ordering[1] = DP_MONTH;
         ordering[2] = DP_YEAR;
         log.warning("Config load failed; using default ordering: %s", ordering);
      }
   }

   ordering_out[0] = ordering[0];
   ordering_out[1] = ordering[1];
   ordering_out[2] = ordering[2];
}
