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

/*****************************************************************************

-FUNCTION-
StrFormatDate: Formats the content of a Time object into a date string.

The StrFormatDate() function is used to format dates into a human readable string format.  The Format argument defines
how the time information is printed to the display.  Time formatting follows accepted conventions, as illustrated in
the following format table:

<types type="Format">
<type name="a">am/pm (am)</>
<type name="A">AM/PM (AM)</>
<type name="h">24-Hour (9)</>
<type name="hh">24-Hour two-digits (09)</>
<type name="H">12-Hour (9)</>
<type name="HH">12-Hour two-digits (09)</>
<type name="n">Minute (7)</>
<type name="nn">Minute two-digits (07)</>
<type name="s">Second (42)</>
<type name="ss">Second two-digits (42)</>
<type name="d">Day (3)</>
<type name="dd">Day two-digits (03)</>
<type name="D">Day of week (S)</>
<type name="ddd">Day of week (sat)</>
<type name="dddd">Day of week (saturday)</>
<type name="m">Month (11)</>
<type name="mm">Month (11)</>
<type name="mmm">Month (nov)</>
<type name="mmmm">Month (november)</>
<type name="yy">Year (03)</>
<type name="yyyy">Year (2003)</>
</>

By default, strings are printed in lower-case.  For example, `ddd` prints `sat`.  To capitalise the characters, use
capitals in the string formatting instead of the default lower case.  For example `Ddd` prints `Sat` and `DDD` prints
`SAT`.

Where the interpreter encounters unrecognised characters, it will print them out unaltered.  For example `hh:nn:ss`
might print `05:34:19` with the colons intact.  If you need to include characters such as `h` or `d` and want to
prevent them from being translated, precede them with the backslash character.  For instance to print `0800 hours`, we
would use the format `hh00 \hours`.

The Time structure that you provide needs to contain values in the Year, Month, Day, Hour, Minute and Second fields.
If you know that you do not plan on printing information on any particular date value, you can optionally choose not to
set that field.

-INPUT-
buf(str) Buffer: Pointer to a buffer large enough to receive the formatted data.
bufsize Size: The length of the Buffer, in bytes.
cstr Format: String specifying the required date format.
struct(DateTime) Time: Pointer to a DateTime structure that contains the date to be formatted.  If NULL, the current system time will be used.

-ERRORS-
Okay:
Args:
BufferOverflow: The buffer was not large enough to hold the formatted string (the result will be correct, but truncated).
-END-

*****************************************************************************/

static const CSTRING days[7] = { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" };
static const CSTRING months[13] = { "", "January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December" };

ERROR StrFormatDate(STRING Buffer, LONG BufferSize, CSTRING Format, struct DateTime *Time)
{
   struct DateTime dt;
   STRING str;
   char day[32], month[32], buffer[80];
   WORD pos, i, j;

   if ((!Buffer) OR (BufferSize < 1) OR (!Format)) return ERR_Args;

   if (!Time) {
      if (!glTime) {
         if (CreateObject(ID_TIME, NF_UNTRACKED, (OBJECTPTR *)&glTime,
            FID_Owner|TLONG, 0,
            TAGEND)) return ERR_CreateObject;
      }

      acQuery(&glTime->Head);
      dt.Year   = glTime->Year;
      dt.Month  = glTime->Month;
      dt.Day    = glTime->Day;
      dt.Hour   = glTime->Hour;
      dt.Minute = glTime->Minute;
      dt.Second = glTime->Second;
      Time = &dt;
   }

   if (StrCompare("ShortDate", Format, 9, 0) IS ERR_Okay) {
      CSTRING short_date;
      if (StrReadLocale("ShortDate", &short_date)) short_date = "yyyy-mm-dd";
      i = StrCopy(short_date, buffer, sizeof(buffer));
      StrCopy(Format+9, buffer+i, sizeof(buffer)-i);
      Format = buffer;
   }
   else if (StrCompare("LongDate", Format, 8, 0) IS ERR_Okay) {
      CSTRING long_date;
      if (StrReadLocale("LongDate", &long_date)) long_date = "yyyy-mm-dd";
      i = StrCopy(long_date, buffer, sizeof(buffer));
      StrCopy(Format+8, buffer, sizeof(buffer)-i);
      Format = buffer;
   }

   LONG a = (14 - Time->Month) / 12;
   LONG y = Time->Year - a;
   LONG m = Time->Month + 12 * a - 2;
   WORD dayofweek = (Time->Day + y + (y / 4) - (y / 100) + (y / 400) + (31 * m) / 12) % 7;

   StrCopy(StrTranslateText(days[dayofweek]), day, sizeof(day));

   if ((Time->Month > 0) AND (Time->Month <= 12)) StrCopy(StrTranslateText(months[Time->Month]), month, sizeof(month));
   else StrCopy("-------", month, sizeof(month));

   BufferSize--; // -1 so that we don't have to worry about the null terminator
   str = Buffer;
   pos = 0;
   for (i=0; (Format[i]) AND (pos < BufferSize); i++) {
      if (Format[i] IS '\\') {
         if (Format[i+1]) {
            str[pos++] = Format[i+1];
            i++;
         }
         else str[pos++] = '\\';
      }
      else if (Format[i] IS 'a') { // am/pm
        if (Time->Hour >= 12) {
           if (pos < BufferSize) str[pos++] = 'p';
           if (pos < BufferSize) str[pos++] = 'm';
        }
        else {
           if (pos < BufferSize) str[pos++] = 'a';
           if (pos < BufferSize) str[pos++] = 'm';
        }
      }
      else if (Format[i] IS 'A') { // AM/PM
        if (Time->Hour >= 12) {
           if (pos < BufferSize) str[pos++] = 'P';
           if (pos < BufferSize) str[pos++] = 'M';
        }
        else {
           if (pos < BufferSize) str[pos++] = 'A';
           if (pos < BufferSize) str[pos++] = 'M';
        }
      }
      else if (Format[i] IS 'h') {
         // 24 hour clock

         if (Format[i+1] IS 'h') {
            i++;
            if (Time->Hour < 10) str[pos++] = '0';
         }
         pos += IntToStr(Time->Hour, str + pos, BufferSize-pos);
      }
      else if (Format[i] IS 'H') {
         // 12 hour clock

         if (Format[i+1] IS 'H') {
            i++;
            if (Time->Hour < 10) str[pos++] = '0';
         }
         if (Time->Hour > 12) {
            if (!(j = Time->Hour - 12)) j = 12;
            pos += IntToStr(j, str + pos, BufferSize-pos);
         }
         else pos += IntToStr(Time->Hour, str + pos, BufferSize-pos);
      }
      else if (Format[i] IS 'n') {
         // Minutes

         if (Format[i+1] IS 'n') {
            i++;
            if (Time->Minute < 10) str[pos++] = '0';
         }
         pos += IntToStr(Time->Minute, str + pos, BufferSize-pos);
      }
      else if (Format[i] IS 's') {
         // Seconds

         if (Format[i+1] IS 's') {
            i++;
            if (Time->Second < 10) str[pos++] = '0';
         }
         pos += IntToStr(Time->Second, str + pos, BufferSize-pos);
      }
      else if (Format[i] IS 'y') {
         // Year

         if ((Format[i+1] IS 'y') AND (Format[i+2] != 'y')) {
            WORD tmp = Time->Year % 100;
            if (tmp < 10) str[pos++] = '0';
            pos += IntToStr(tmp, str + pos, BufferSize-pos);
         }
         else pos += IntToStr(Time->Year, str + pos, BufferSize-pos);

         while (Format[i+1] IS 'y') i++;
      }
      else if ((Format[i] IS 'd') OR (Format[i] IS 'D')) {
         // Day: d    = Single digit
         //      D    = Day of Week character
         //      dd   = Double digit
         //      ddd  = Short name
         //      dddd = Long name
         //      Ddd  = Short name caps
         //      Dddd = Long name caps

         if ((Format[i+1] != 'd') AND (Format[i+2] != 'D')) { // d
            if (Format[i] IS 'D') str[pos++] = *day;
            else pos += IntToStr(Time->Day, str + pos, BufferSize-pos);
         }
         else if ((Format[i+2] != 'd') AND (Format[i+2] != 'D')) { // dd
            i++;
            if (Time->Day < 10) str[pos++] = '0';
            pos += IntToStr(Time->Day, str + pos, BufferSize-pos);
         }
         else if ((Format[i+3] != 'd') AND (Format[i+3] != 'D')) { // ddd
            if (Format[i] IS 'D') {
               if (Format[i+1] IS 'D') {
                  for (j=0; (j < 3) AND (day[j]) AND (pos < BufferSize-pos); j++) str[pos++] = UCase(day[j]);
               }
               else  pos += UTF8Copy(day, str+pos, 3, BufferSize-pos);
            }
            else for (j=0; (j < 3) AND (day[j]) AND (pos < BufferSize-pos); j++) str[pos++] = LCase(day[j]);
            i += 2;
         }
         else if ((Format[i+4] != 'd') AND (Format[i+4] != 'D')) { // dddd
            if (Format[i] IS 'D') {
               if (Format[i+1] IS 'D') {
                  for (j=0; (day[j]) AND (pos < BufferSize-pos); j++) str[pos++] = UCase(day[j]);
               }
               else pos += StrCopy(day, str+pos, BufferSize-pos);
            }
            else for (j=0; (day[j]) AND (pos < BufferSize-pos); j++) str[pos++] = LCase(day[j]);

            i += 3;
         }
      }
      else if ((Format[i] IS 'm') OR (Format[i] IS 'M')) {
         // Month: m    = Single digit
         //        mm   = Double digit
         //        mmm  = Short name
         //        mmmm = Long name
         //        Mmm  = Short name caps
         //        Mmmm = Long name caps

         if ((Format[i+1] != 'm') AND (Format[i+2] != 'M')) { // m
            pos += IntToStr(Time->Month, str + pos, BufferSize-pos);
         }
         else if ((Format[i+2] != 'm') AND (Format[i+2] != 'M')) { // mm
            i++;
            if (Time->Month < 10) str[pos++] = '0';
            pos += IntToStr(Time->Month, str + pos, BufferSize-pos);
         }
         else if ((Format[i+3] != 'm') AND (Format[i+3] != 'M')) { // mmm
            if (Format[i] IS 'M') {
               if (Format[i+1] IS 'M') {
                  for (j=0; (j < 3) AND (month[j]) AND (pos < BufferSize-pos); j++) {
                     str[pos++] = UCase(month[j]);
                  }
               }
               else pos += UTF8Copy(month, str+pos, 3, BufferSize-pos);
            }
            else for (j=0; (j < 3) AND (month[j]) AND (pos < BufferSize-pos); j++) str[pos++] = LCase(month[j]);
            i += 2;
         }
         else if ((Format[i+4] != 'm') AND (Format[i+4] != 'M')) { // mmmm
            if (Format[i] IS 'M') {
               if (Format[i+1] IS 'M') {
                  for (j=0; (month[j]) AND (pos < BufferSize-pos); j++) str[pos++] = UCase(month[j]);
               }
               else pos += StrCopy(month, str+pos, BufferSize-pos);
            }
            else for (j=0; (month[j]) AND (pos < BufferSize-pos); j++) str[pos++] = LCase(month[j]);

            i += 3;
         }
      }
      else str[pos++] = Format[i];
   }
   str[pos] = 0;

   return ERR_Okay;
}

/*****************************************************************************

-FUNCTION-
StrReadDate: Reads a date string into a DateTime structure.

Reads a date in string format and returns the year, month, and day in a DateTime structure.  The DateTime struct will
not be altered unless the function returns ERR_Okay.

This function understands named months (e.g. 'March' or 'Mar') and ignores non-numeric characters and white-space not
relevant to time interpretation. It is recommended that years are never shortened to 2 digits as this causes confusion
between months and days - moreover, the correct century will need to be guessed if 2 digit years are used.  Use of
named months is also preferred to prevent confusion between numeric months and days.

Special date formats that are supported include `yesterday`, `today`, `now`, `tomorrow`, `day`, `month` and `year`.

Compressed date formats that are without separators, such as `20090702` are not supported.

-INPUT-
cstr String:  A date string.
struct(DateTime) DateTime: Pointer to a DateTime structure in which the date will be placed.

-ERRORS-
Okay: The date was parsed successfully.
NullArgs:
InvalidData: The date could not be parsed.
CreateObject:
Failed:
-END-

*****************************************************************************/

static CSTRING find_datepart(CSTRING Date, struct datepart *Part)
{
   parasol::Log log(__FUNCTION__);
   UBYTE ch;
   WORD i;

   if (Date) {
   while (*Date) {
      ch = *Date;
      if (is_alpha(ch)) {
         if ((ch >= '0') AND (ch <= '9')) {
            Part->String = Date;
            Part->Number = StrToInt(Date);
            for (i=0; (*Date >= '0') AND (*Date <= '9'); i++) Date++;

            if (i >= 4) Part->Type = DP_YEAR;
            else if (Part->Number > 31) Part->Type = DP_YEAR;
            else if (Part->Number > 12) Part->Type = DP_DAY;
            else Part->Type = 0;
            log.trace("Found date type $%.2x", Part->Type);
            return Date;
         }
         else {
            for (i=1; i <= 12; i++) {
               // Check for match with 3-letter prefix of month string

               if (!StrCompare(months[i], Date, 3, 0)) {
                  if ((Date[3] <= 0x20) OR (!StrCompare(months[i], Date, 0, 0))) {
                     Part->String  = Date;
                     Part->Number  = i;
                     Part->Type    = DP_MONTH;
                     while (glAlphaNumeric[((UBYTE *)Date)[0]]) Date++;
                     log.trace("Found date type 'month'");
                     return Date;
                  }
               }
            }
            if (i <= 12) break;
         }
      }
      Date++;
   }
   }

   Part->String = NULL;
   Part->Number = 0;
   Part->Type = 0;
   return NULL;
}

static void add_days(struct DateTime *Date, LONG Days)
{
   LARGE esecs = datetime_to_epoch(Date);
   esecs += Days * SECS_DAY;
   epoch_to_datetime(esecs, Date);
}

ERROR StrReadDate(CSTRING Date, struct DateTime *Output)
{
   parasol::Log log(__FUNCTION__);
   struct datepart datepart[3];
   char ordering[3] = { 0, 0, 0 };
   LONG i, numparts;
   struct DateTime time;
   CSTRING str;

   if ((!Date) OR (!Output)) return ERR_NullArgs;

   #warning Use of glTime is not thread safe
   if (!glTime) {
      if (CreateObject(ID_TIME, NF_UNTRACKED, (OBJECTPTR *)&glTime,
            TAGEND)) return ERR_CreateObject;
   }

   acQuery(&glTime->Head);

   if (!StrMatch("yesterday", Date)) {
      Output->Year   = glTime->Year;
      Output->Month  = glTime->Month;
      Output->Day    = glTime->Day;
      Output->Hour   = glTime->Hour;
      Output->Minute = glTime->Minute;
      Output->Second = glTime->Second;
      Output->TimeZone = glTime->TimeZone;
      add_days(Output, -1);
      return ERR_Okay;
   }
   else if ((!StrMatch("today", Date)) OR (!StrMatch("now", Date))) {
      Output->Year   = glTime->Year;
      Output->Month  = glTime->Month;
      Output->Day    = glTime->Day;
      Output->Hour   = glTime->Hour;
      Output->Minute = glTime->Minute;
      Output->Second = glTime->Second;
      Output->TimeZone = glTime->TimeZone;
      return ERR_Okay;
   }
   else if (!StrMatch("tomorrow", Date)) {
      Output->Year   = glTime->Year;
      Output->Month  = glTime->Month;
      Output->Day    = glTime->Day;
      Output->Hour   = glTime->Hour;
      Output->Minute = glTime->Minute;
      Output->Second = glTime->Second;
      Output->TimeZone = glTime->TimeZone;
      add_days(Output, 1);
      return ERR_Okay;
   }
   else if (!StrCompare("day", Date, 3, 0)) {
      // Strings such as "day+1", "day-7" are acceptable



      return ERR_NoSupport;
   }
   else if (!StrCompare("month", Date, 5, 0)) {
      // Strings such as "month+1", "month-12" are acceptable



      return ERR_NoSupport;
   }
   else if (!StrCompare("year", Date, 4, 0)) {
      // Strings such as "year+10", "year-2" are acceptable

      Output->Year     = glTime->Year;
      Output->Month    = 1;
      Output->Day      = 1;
      Output->Hour     = 0;
      Output->Minute   = 0;
      Output->Second   = 0;
      Output->TimeZone = glTime->TimeZone;

      Date += 4;
      while (*Date) {
         if ((*Date IS '+') OR (*Date IS '-')) {
            Output->Year += StrToInt(Date);
            break;
         }
         Date++;
      }

      return ERR_Okay;
   }

   ClearMemory(&time, sizeof(struct DateTime));

   // Deconstruct the string into date parts

   if ((str = find_datepart(Date, &datepart[0]))) {
      if ((str = find_datepart(str, &datepart[1]))) {
         if ((str = find_datepart(str, &datepart[2]))) {
            numparts = 3;
         }
         else numparts = 2;
      }
      else numparts = 1;
   }
   else return ERR_InvalidData;

   if (numparts IS 1) {
      time.Year  = glTime->Year;
      time.Month = glTime->Month;
      time.Day   = glTime->Day;

      if (!datepart[0].Type) {
         read_ordering(ordering);
         datepart[0].Type = ordering[0];
      }

      if (datepart[0].Type IS DP_MONTH) {
         time.Month = datepart[0].Number;
         time.Day   = 1;
      }
      else if (datepart[0].Type IS DP_DAY) {
         time.Day = datepart[0].Number;
      }
      else if (datepart[0].Type IS DP_YEAR) {
         time.Day = 1;
         time.Month = 1;
      }
      else return ERR_InvalidData;
   }
   else if (numparts IS 2) {
      // Interpret this as month/year or day/month of current year

      time.Year  = glTime->Year;
      time.Month = glTime->Month;
      time.Day   = glTime->Day;

      if (!datepart[0].Type) {
         if (!datepart[1].Type) {
            read_ordering(ordering);
            datepart[0].Type = ordering[0];
            datepart[1].Type = ordering[1];
         }
         else {
            if (datepart[1].Type IS DP_YEAR) datepart[0].Type = DP_MONTH;
            else if (datepart[1].Type IS DP_DAY) datepart[0].Type = DP_MONTH;
            else datepart[0].Type = DP_DAY;
         }
      }
      else if (!datepart[1].Type) {
         if (datepart[0].Type IS DP_YEAR) datepart[1].Type = DP_MONTH;
         else if (datepart[0].Type IS DP_MONTH) datepart[1].Type = DP_YEAR;
         else datepart[1].Type = DP_MONTH;
      }

      for (i=0; i < numparts; i++) {
         if (datepart[i].Type IS DP_YEAR)       time.Year = datepart[i].Number;
         else if (datepart[i].Type IS DP_MONTH) time.Month = datepart[i].Number;
         else if (datepart[i].Type IS DP_DAY)   time.Day = datepart[i].Number;
      }
   }
   else if (numparts IS 3) {
      UBYTE empty, missing;

      if ((!datepart[0].Type) OR (!datepart[1].Type) OR (!datepart[2].Type)) {
         missing = DP_DAY|DP_MONTH|DP_YEAR;
         for (i=0, empty=0; i < 3; i++) {
            if (!datepart[i].Type) empty++;
            missing = missing & (~datepart[i].Type);
         }

         if (empty IS 1) {
            // Only one of the parts is empty

            for (i=0; datepart[i].Type; i++);

            if (missing & DP_DAY) datepart[i].Type = DP_DAY;
            else if (missing & DP_MONTH) datepart[i].Type = DP_MONTH;
            else datepart[i].Type = DP_YEAR;
         }
         else if (empty IS 3) {
            // All parts are missing - likely to be a date such as 04/02/23
            read_ordering(ordering);
            datepart[0].Type = ordering[0];
            datepart[1].Type = ordering[1];
            datepart[2].Type = ordering[2];
         }
         else {
            // Two parts are missing

            if (datepart[0].Type IS DP_YEAR) {
               datepart[1].Type = DP_MONTH;
               datepart[2].Type = DP_DAY;
            }
            else if (datepart[0].Type IS DP_DAY) {
               datepart[1].Type = DP_MONTH;
               datepart[2].Type = DP_YEAR;
            }
            else if (datepart[0].Type IS DP_MONTH) {
               datepart[1].Type = DP_DAY;
               datepart[2].Type = DP_YEAR;
            }
            else if (datepart[2].Type IS DP_YEAR) {
               read_ordering(ordering);
               if ((ordering[0] != DP_YEAR) AND (ordering[1] != DP_YEAR)) {
                  datepart[0].Type = ordering[0];
                  datepart[1].Type = ordering[1];
               }
               else {
                  datepart[0].Type = DP_DAY;
                  datepart[1].Type = DP_MONTH;
               }
            }
            else if (datepart[2].Type IS DP_DAY) {
               datepart[0].Type = DP_YEAR;
               datepart[1].Type = DP_MONTH;
            }
            else return ERR_InvalidData;
         }
      }

      missing = 0;
      for (i=0; i < numparts; i++) {
         missing |= datepart[i].Type;
         if (datepart[i].Type IS DP_YEAR)       time.Year = datepart[i].Number;
         else if (datepart[i].Type IS DP_MONTH) time.Month = datepart[i].Number;
         else if (datepart[i].Type IS DP_DAY)   time.Day = datepart[i].Number;
      }

      // Abort if any of the parts are missing

      if (missing != (DP_DAY|DP_MONTH|DP_YEAR)) {
         log.trace("Duplicate part detected in date: %s", Date);
         return ERR_InvalidData;
      }
   }
   else return ERR_InvalidData;

   if ((time.Month < 1) OR (time.Month > 12)) {
      log.trace("Invalid month value %d in date: %s", time.Month, Date);
      return ERR_InvalidData;
   }

   if ((time.Day < 1) OR (time.Day > 31)) {
      log.trace("Invalid day value %d in date: %s", time.Day, Date);
      return ERR_InvalidData;
   }

   // Century checking

   if (time.Year < 100) {
      if (time.Year < 50) time.Year += 2000;
      else time.Year += 1900;
   }

   Output->Year   = time.Year;
   Output->Month  = time.Month;
   Output->Day    = time.Day;

   Output->Hour   = 0;
   Output->Minute = 0;
   Output->Second = 0;
   Output->TimeZone = 0;

   if (str) {
      // Time parsing.  The time must be defined as HH:NN:SS - seconds are optional.  Use of pm or am following
      // the time is allowed, otherwise a 24 hour clock is assumed.

      while ((*str) AND ((*str < '0') OR (*str > '9'))) str++;
      Output->Hour = StrToInt(str);
      if (Output->Hour >= 24) Output->Hour = 0;
      while ((*str >= '0') AND (*str <= '9')) str++;
      if (*str IS ':') {
         str++;
         if ((*str >= '0') AND (*str <= '9')) {
            Output->Minute = StrToInt(str);
            if (Output->Minute >= 60) Output->Minute = 0;
            while ((*str >= '0') AND (*str <= '9')) str++;
            if (*str IS ':') {
               str++;
               Output->Second = StrToInt(str);
               if (Output->Second >= 60) Output->Second = 0;
               while ((*str >= '0') AND (*str <= '9')) str++;
            }
            while ((*str) AND (*str <= 0x20)) str++;

            // AM/PM check to convert to 24 hour clock

            if ((!StrCompare("am", str, 2, 0)) OR (!StrCompare("a.m", str, 3, 0))) {
               if (Output->Hour >= 12) Output->Hour = 0;
               str += 2;
            }
            else if ((!StrCompare("pm", str, 2, 0)) OR (!StrCompare("p.m", str, 3, 0))) {
               if (Output->Hour < 12) Output->Hour += 12;
               str += 2;
            }

            // If a time is defined, scan the rest of the string for a timezone in the format GMT, EDT or +0200 or
            // -0200 for example.

            while (*str) {
               if ((*str IS '+') OR (*str IS '-')) {
                  char tz[5];
                  for (i=1; i < 4; i++) {
                     if ((str[i] < '0') OR (str[i] > '9')) break;
                     tz[i-1] = str[i];
                  }
                  if (i IS 4) {
                    tz[4] = 0;
                    Output->TimeZone = StrToInt(tz);
                    if (*str IS '-') Output->TimeZone = -Output->TimeZone;
                    break;
                  }
               }

               str++;
            }
         }
         else Output->Hour = 0;
      }
      else Output->Hour = 0;
   }

   return ERR_Okay;
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

   if ((!Key) OR (!Value)) return ERR_NullArgs;

   #ifdef __ANDROID__
      // Android doesn't have locale.cfg, we have to load that information from the system.

      if (!StrMatch("Language", Key)) {
         static char code[4] = { 0, 0, 0, 0 };
         if (!code[0]) {
            if (!AndroidBase) {  // Note that the module is terminated through resource tracking, we don't free it during our CMDExpunge() sequence for system integrity reasons.
               OBJECTPTR context, module;
               context = SetContext(CurrentTask());
                  LoadModule("android", MODVERSION_FLUID, &module, &AndroidBase);
               SetContext(context);
               if (!AndroidBase) return NULL;
            }

            AConfiguration *config;
            if (!adGetConfig(&config)) {
               AConfiguration_getLanguage(config, code);

               // Convert the two letter code to three letters.

               if (code[0]) {
                  WORD i;
                  code[0] = LCASE(code[0]);
                  code[1] = LCASE(code[1]);
                  for (i=0; i < ARRAYSIZE(glLanguages); i++) {
                     if ((glLanguages[i].Two[0] IS code[0]) AND (glLanguages[i].Two[1] IS code[1])) {
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
   if ((*Str >= '0') AND (*Str <= '9')) return (*Str - '0');
   else if ((*Str >= 'A') AND (*Str <= 'F')) return ((*Str - 'A')+10);
   else if ((*Str >= 'a') AND (*Str <= 'f')) return ((*Str - 'a')+10);
   else return -1;
}

ERROR StrToColour(CSTRING Colour, struct RGB8 *RGB)
{
   if ((!Colour) OR (!RGB)) return ERR_NullArgs;

   if (*Colour IS '#') {
      Colour++;
      char nibbles[8];
      UBYTE n = 0;
      while ((*Colour) AND (n < ARRAYSIZE(nibbles))) nibbles[n++] = read_nibble(Colour++);

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
      while ((*Colour) AND (*Colour != ',')) { Colour++; if (*Colour IS '%') RGB->Red = (RGB->Red * 255) / 100; } if (*Colour) Colour++;
      RGB->Green = StrToInt(Colour);
      while ((*Colour) AND (*Colour != ',')) { Colour++; if (*Colour IS '%') RGB->Green = (RGB->Green * 255) / 100; } if (*Colour) Colour++;
      RGB->Blue = StrToInt(Colour);
      while ((*Colour) AND (*Colour != ',')) { Colour++; if (*Colour IS '%') RGB->Blue = (RGB->Blue * 255) / 100; } if (*Colour) Colour++;
      while ((*Colour) AND (*Colour <= 0x20)) Colour++;
      if (*Colour) {
         RGB->Alpha = StrToInt(Colour);
         while ((*Colour >= '0') AND (*Colour <= '9')) Colour++;
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

   while ((*String != '-') AND (*String != '.') AND ((*String < '0') OR (*String > '9'))) {
      if (!*String) return 0;
      String++;
   }

   UBYTE neg;
   DOUBLE result;
   if (*String IS '.') { result = 0; neg = FALSE; }
   else {
      result = (DOUBLE)StrToInt(String);

      // Skip numbers
      if (*String IS '-') {
         neg = TRUE; // Remember that the number is signed (in case of e.g. '-0.5')
         String++;
      }
      else neg = FALSE;

      while ((*String >= '0') AND (*String <= '9')) String++;
   }

   // Check for decimal place

   if (*String IS '.') {
      String++;
      if ((*String >= '0') AND (*String <= '9')) {
         LONG number;
         if ((number = StrToInt(String))) {
             LONG factor = 1;
             while ((*String >= '0') AND (*String <= '9')) {
                factor = factor * 10;
                String++;
             }

             if (result < 0) result = result - (((DOUBLE)number) / factor);
             else result = result + (((DOUBLE)number) / factor);
         }
      }
   }

   if ((neg) AND (result > 0)) result = -result;

   return result;
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
      if ((*String >= '0') AND (*String <= '9')) break;
      if ((*String >= 'A') AND (*String <= 'F')) break;
      if ((*String >= 'a') AND (*String <= 'f')) break;
      if (*String IS '$') break;
      if (*String IS '#') break;
      String++;
   }

   if ((String[0] IS '0') AND ((String[1] IS 'X') OR (String[1] IS 'x'))) String += 2;
   else if (String[0] IS '$') String++;
   else if (String[0] IS '#') String++;

   LARGE result = 0;
   while (*String) {
      if ((*String >= '0') AND (*String <= '9')) {
         result <<= 4;
         result += *String - '0';
      }
      else if ((*String >= 'a') AND (*String <= 'f')) {
         result <<= 4;
         result += *String - 'a' + 10;
      }
      else if ((*String >= 'A') AND (*String <= 'F')) {
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

LARGE StrToInt(CSTRING str)
{
   parasol::Log log(__FUNCTION__);

   if (!str) return 0;

   // Ignore any leading characters

   BYTE neg = FALSE;
   while ((*str < '0') OR (*str > '9')) {
      if (*str IS 0) return 0;
      if (*str IS '-') neg = TRUE;
      if (*str IS '+') neg = FALSE;
      str++;
   }

   while (*str IS '0') str++; // Ignore leading zeros

   CSTRING start = str;
   LARGE number = 0;
   while (*str) {
      if ((*str >= '0') AND (*str <= '9')) {
         number *= 10LL;
         number += (*str - '0');
      }
      else break;
      str++;
   }

   if (number < 0) { // If the sign reversed during parsing, an overflow occurred
      log.warning("Buffer overflow: %s", start);
      return 0;
   }

   return neg ? -number : number;
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
         for (; (*str) AND (stage < 3); str++) {
            if(((*str IS 'y') OR (*str IS 'Y')) AND (!seen_y)) {
               ordering[stage++] = DP_YEAR;
               seen_y = TRUE;
            }
            else if (((*str IS 'm') OR (*str IS 'M')) AND (!seen_m)) {
               ordering[stage++] = DP_MONTH;
               seen_m = TRUE;
            }
            else if (((*str IS 'd') OR (*str IS 'D')) AND (!seen_d)) {
               ordering[stage++] = DP_DAY;
               seen_d = TRUE;
            }
         }

         if ((seen_y) AND (seen_m) AND (seen_d)) {
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
