/*********************************************************************************************************************

The source code of the Parasol project is made publicly available under the
terms described in the LICENSE.TXT file that is distributed with this package.
Please refer to it for further information on licensing.

**********************************************************************************************************************

-CLASS-
Time: Simplifies the management of date/time information.

The Time class is available for programs that require time and date recording.  In future, support will also be
provided for the addition and subtraction of date values.

Please note that the Time class uses strict metric interpretations of "millisecond" and "microsecond" terminology. That
is, a millisecond is 1/1000th (one thousandth) of a second, a microsecond is 1/1000000th (one millionth) of a second.

To get the current system time, use the #Query() action.
-END-

*********************************************************************************************************************/

#include "../defs.h"
#include <parasol/main.h>
#include <time.h>

#ifdef __unix__
#include <sys/ioctl.h>
#include <sys/time.h>
 #ifdef __linux__
  #include <linux/rtc.h>
 #endif
#include <fcntl.h>
#include <unistd.h>
#endif

static ERROR GET_TimeStamp(objTime *, LARGE *);

static ERROR TIME_Query(objTime *, APTR);
static ERROR TIME_SetTime(objTime *, APTR);

/*********************************************************************************************************************
-ACTION-
Query: Updates the values in a time object with the current system date and time.
-END-
*********************************************************************************************************************/

static ERROR TIME_Query(objTime *Self, APTR Void)
{
   #ifdef __unix__

      struct timeval tmday;
      struct tm local;

      gettimeofday(&tmday, NULL);  // Get micro-seconds
      time_t tm = time(NULL);
      localtime_r(&tm, &local);     // Get time
      Self->Year        = 1900 + local.tm_year;
      Self->Month       = local.tm_mon + 1;
      Self->Day         = local.tm_mday;
      Self->Hour        = local.tm_hour;
      Self->Minute      = local.tm_min;
      Self->Second      = tmday.tv_sec % 60;
      Self->MilliSecond = tmday.tv_usec / 1000;  // Between 0 and 999
      Self->MicroSecond = tmday.tv_usec;         // Between 0 and 999999
      Self->SystemTime  = (((LARGE)tm) * (LARGE)1000000) + (LARGE)Self->MicroSecond;

   #elif _WIN32

     time_t tm;
     time(&tm);
     struct tm *local = localtime(&tm);
     LONG millisecond = winGetTickCount() / 1000;
     Self->Year   = 1900 + local->tm_year;
     Self->Month  = local->tm_mon + 1;
     Self->Day    = local->tm_mday;
     Self->Hour   = local->tm_hour;
     Self->Minute = local->tm_min;
     Self->Second = local->tm_sec;

     Self->MilliSecond = millisecond;
     Self->MicroSecond = millisecond * 100;
     Self->SystemTime  = (((LARGE)millisecond) * (LARGE)1000);

   #else
      #error Platform requires support for the Time class.
   #endif

   // Calculate the day of the week (0 = Sunday)

   LONG a = (14 - Self->Month) / 12;
   LONG y = Self->Year - a;
   LONG m = Self->Month + 12 * a - 2;
   Self->DayOfWeek = (Self->Day + y + (y / 4) - (y / 100) + (y / 400) + (31 * m) / 12) % 7;

   return ERR_Okay;
}

/*********************************************************************************************************************

-METHOD-
SetTime: Apply the time to the system clock.

This method will apply the time object's values to the BIOS.  Depending on the host platform, this method may only
work if the user is logged in as the administrator.
-END-

*********************************************************************************************************************/

static ERROR TIME_SetTime(objTime *Self, APTR Void)
{
#ifdef __unix__
   pf::Log log;
   struct timeval tmday;
   struct tm time;
   LONG fd;

   log.branch();

   // Set the BIOS clock

   #ifdef __APPLE__
      log.warning("No support for modifying the BIOS clock in OS X build");
   #else
      if ((fd = open("/dev/rtc", O_RDONLY|O_NONBLOCK)) != -1) {
         time.tm_year  = Self->Year - 1900;
         time.tm_mon   = Self->Month - 1;
         time.tm_mday  = Self->Day;
         time.tm_hour  = Self->Hour;
         time.tm_min   = Self->Minute;
         time.tm_sec   = Self->Second;
         time.tm_isdst = -1;
         time.tm_wday  = 0;
         time.tm_yday  = 0;
         ioctl(fd, RTC_SET_TIME, &time);
         close(fd);
      }
      else log.warning("/dev/rtc not available.");
   #endif

   // Set the internal system clock

   time.tm_year  = Self->Year - 1900;
   time.tm_mon   = Self->Month - 1;
   time.tm_mday  = Self->Day;
   time.tm_hour  = Self->Hour;
   time.tm_min   = Self->Minute;
   time.tm_sec   = Self->Second;
   time.tm_isdst = -1;
   time.tm_wday  = 0;
   time.tm_yday  = 0;

   if ((tmday.tv_sec = mktime(&time)) != -1) {
      tmday.tv_usec = 0;
      if (settimeofday(&tmday, NULL) IS -1) {
         log.warning("settimeofday() failed.");
      }
   }
   else log.warning("mktime() failed [%d/%d/%d, %d:%d:%d]", Self->Day, Self->Month, Self->Year, Self->Hour, Self->Minute, Self->Second);

   return ERR_Okay;
#else
   return ERR_NoSupport;
#endif
}

/*********************************************************************************************************************

-FIELD-
Day: Day (1 - 31)

-FIELD-
DayOfWeek: Day of week (0 - 6) starting from Sunday.

-FIELD-
Hour: Hour (0 - 23)

-FIELD-
MicroSecond: Microsecond (0 - 999999)

-FIELD-
MilliSecond: Millisecond (0 - 999)

-FIELD-
Minute: Minute (0 - 59)

-FIELD-
Month: Month (1 - 12)

-FIELD-
Second: Second (0 - 59)

-FIELD-
SystemTime: Represents the system time when the time object was last queried.

The SystemTime field returns the system time if the Time object has been queried.  The time is represented in
microseconds.  This field serves no purpose beyond its initial query value.

-FIELD-
TimeZone: No information.
Status: private

-FIELD-
TimeStamp: Read this field to get representation of the time as a single integer.

The TimeStamp field is a 64-bit integer that represents the time object as an approximation of the number of
milliseconds represented in the time object (approximately the total amount of time passed since Zero-AD).  This is
convenient for summarising a time value for comparison with other time stamps, or for storing time in a 64-bit space.

The TimeStamp is dynamically calculated when you read this field.

*********************************************************************************************************************/

static ERROR GET_TimeStamp(objTime *Self, LARGE *Value)
{
   *Value = Self->Second +
            ((LARGE)Self->Minute * 60) +
            ((LARGE)Self->Hour * 60 * 60) +
            ((LARGE)Self->Day * 60 * 60 * 24) +
            ((LARGE)Self->Month * 60 * 60 * 24 * 31) +
            ((LARGE)Self->Year * 60 * 60 * 24 * 12);

   *Value = *Value * 1000000LL;

   *Value += Self->MilliSecond;

   return ERR_Okay;
}

/*********************************************************************************************************************
-FIELD-
Year: Year (-ve for BC, +ve for AD).
-END-
*********************************************************************************************************************/

static const FieldArray TimeFields[] = {
   { "SystemTime",   FDF_LARGE|FDF_RW, 0, 0, 0 },
   { "Year",         FDF_LONG|FDF_RW, 0, 0, 0 },
   { "Month",        FDF_LONG|FDF_RW, 0, 0, 0 },
   { "Day",          FDF_LONG|FDF_RW, 0, 0, 0 },
   { "Hour",         FDF_LONG|FDF_RW, 0, 0, 0 },
   { "Minute",       FDF_LONG|FDF_RW, 0, 0, 0 },
   { "Second",       FDF_LONG|FDF_RW, 0, 0, 0 },
   { "TimeZone",     FDF_LONG|FDF_RW, 0, 0, 0 },
   { "DayOfWeek",    FDF_LONG|FDF_RW, 0, 0, 0 },
   { "MilliSecond",  FDF_LONG|FDF_RW, 0, 0, 0 },
   { "MicroSecond",  FDF_LONG|FDF_RW, 0, 0, 0 },
   // Virtual fields
   { "TimeStamp",    FDF_LARGE|FDF_R, 0, (APTR)GET_TimeStamp, NULL },
   END_FIELD
};

static const ActionArray TimeActions[] = {
   { AC_Query,      (APTR)TIME_Query },
   { AC_Refresh,    (APTR)TIME_Query },
   { 0, NULL }
};

static const MethodArray TimeMethods[] = {
   { MT_TmSetTime, (APTR)TIME_SetTime, "SetTime", 0, 0 },
   { 0, NULL, NULL }
};

//********************************************************************************************************************

extern "C" ERROR add_time_class(void)
{
   TimeClass = objMetaClass::create::global(
      fl::BaseClassID(ID_TIME),
      fl::ClassVersion(VER_TIME),
      fl::Name("Time"),
      fl::Category(CCF_SYSTEM),
      fl::Actions(TimeActions),
      fl::Methods(TimeMethods),
      fl::Fields(TimeFields),
      fl::Size(sizeof(objTime)),
      fl::Path("modules:core"));

   return TimeClass ? ERR_Okay : ERR_AddClass;
}
