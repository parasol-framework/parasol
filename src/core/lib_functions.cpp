/*********************************************************************************************************************

The source code of the Parasol Framework is made publicly available under the terms described in the LICENSE.TXT file
that is distributed with this package.  Please refer to it for further information on licensing.

-CATEGORY-
Name: System
-END-

*********************************************************************************************************************/

#include <stdlib.h>

#ifdef __unix__
 #include <stdio.h>
 #include <unistd.h>
 #include <signal.h>

 #ifndef __ANDROID__
  #include <sys/msg.h>
 #endif

 #include <sys/time.h>
 #ifdef __linux__
  #include <sys/sysinfo.h>
 #elif __APPLE__
  #include <sys/sysctl.h>
 #endif
 #include <fcntl.h>
 #include <time.h>
 #include <string.h> // Required for memset() on OS X

#elif _WIN32
 #include <string.h> // Required for memmove()
#endif

#ifdef __ANDROID__
 #include <android/log.h>
#endif

#include <math.h>

#include "defs.h"

using namespace pf;

/*********************************************************************************************************************

-FUNCTION-
AllocateID: Generates unique ID's for general purposes.

This function generates unique ID's that can be used in other Core functions.  A type ID is required and the resulting
number will be unique to that type only.

ID allocations are permanent, so there is no need to free the allocated ID once it is no longer required.

-INPUT-
int(IDTYPE) Type: The type of ID that is required.

-RESULT-
int: A unique ID matching the requested type will be returned.  This function can return zero if the Type is unrecognised, or if an internal error occurred.

*********************************************************************************************************************/

LONG AllocateID(LONG Type)
{
   pf::Log log(__FUNCTION__);

   if (Type IS IDTYPE_MESSAGE) {
      LONG id = __sync_add_and_fetch(&glMessageIDCount, 1);
      log.function("MessageID: %d", id);
      return id;
   }
   else if (Type IS IDTYPE_GLOBAL) {
      LONG id = __sync_add_and_fetch(&glGlobalIDCount, 1);
      return id;
   }
   else if (Type IS IDTYPE_FUNCTION) {
      UWORD id = __sync_add_and_fetch(&glFunctionID, 1);
      return id;
   }

   return 0;
}

/*********************************************************************************************************************

-FUNCTION-
CurrentTask: Returns the active Task object.

This function returns the @Task object of the active process.

If there is a legitimate circumstance where there is no current task (e.g. if the function is called during
Core initialisation) then the "system task" may be returned, which has ownership of Core resources.

-RESULT-
obj(Task): Returns a pointer to the current Task object or NULL if failure.

*********************************************************************************************************************/

objTask * CurrentTask(void)
{
   return glCurrentTask;
}

/*********************************************************************************************************************

-FUNCTION-
GetErrorMsg: Translates error codes into human readable strings.
Category: Logging

The GetErrorMsg() function converts error codes into human readable strings.  If the Code is invalid, a string of
"Unknown error code" is returned.

-INPUT-
error Error: The error code to lookup.

-RESULT-
cstr: A human readable string for the error code is returned.  By default error codes are returned in English, however if a translation table exists for the user's own language, the string will be translated.

*********************************************************************************************************************/

CSTRING GetErrorMsg(ERROR Code)
{
   if ((Code < glTotalMessages) and (Code > 0)) {
      return glMessages[Code];
   }
   else if (!Code) return "Operation successful.";
   else return "Unknown error code.";
}

/*********************************************************************************************************************

-FUNCTION-
GenCRC32: Generates 32-bit CRC checksum values.

This function is used internally for the generation of 32-bit CRC checksums.  You may use it for your own purposes to
generate CRC values over a length of buffer space.  This function may be called repeatedly by feeding it previous
CRC values, making it ideal for processing streamed data.

-INPUT-
uint CRC: If streaming data to this function, this value should reflect the most recently returned CRC integer.  Otherwise set to zero.
ptr Data: The data to generate a CRC value for.
uint Length: The length of the Data buffer.

-RESULT-
uint: Returns the computed 32 bit CRC value for the given data.
-END-

*********************************************************************************************************************/

#if 1

static const ULONG crc_table[256] = {
  0x00000000L, 0x77073096L, 0xee0e612cL, 0x990951baL, 0x076dc419L,
  0x706af48fL, 0xe963a535L, 0x9e6495a3L, 0x0edb8832L, 0x79dcb8a4L,
  0xe0d5e91eL, 0x97d2d988L, 0x09b64c2bL, 0x7eb17cbdL, 0xe7b82d07L,
  0x90bf1d91L, 0x1db71064L, 0x6ab020f2L, 0xf3b97148L, 0x84be41deL,
  0x1adad47dL, 0x6ddde4ebL, 0xf4d4b551L, 0x83d385c7L, 0x136c9856L,
  0x646ba8c0L, 0xfd62f97aL, 0x8a65c9ecL, 0x14015c4fL, 0x63066cd9L,
  0xfa0f3d63L, 0x8d080df5L, 0x3b6e20c8L, 0x4c69105eL, 0xd56041e4L,
  0xa2677172L, 0x3c03e4d1L, 0x4b04d447L, 0xd20d85fdL, 0xa50ab56bL,
  0x35b5a8faL, 0x42b2986cL, 0xdbbbc9d6L, 0xacbcf940L, 0x32d86ce3L,
  0x45df5c75L, 0xdcd60dcfL, 0xabd13d59L, 0x26d930acL, 0x51de003aL,
  0xc8d75180L, 0xbfd06116L, 0x21b4f4b5L, 0x56b3c423L, 0xcfba9599L,
  0xb8bda50fL, 0x2802b89eL, 0x5f058808L, 0xc60cd9b2L, 0xb10be924L,
  0x2f6f7c87L, 0x58684c11L, 0xc1611dabL, 0xb6662d3dL, 0x76dc4190L,
  0x01db7106L, 0x98d220bcL, 0xefd5102aL, 0x71b18589L, 0x06b6b51fL,
  0x9fbfe4a5L, 0xe8b8d433L, 0x7807c9a2L, 0x0f00f934L, 0x9609a88eL,
  0xe10e9818L, 0x7f6a0dbbL, 0x086d3d2dL, 0x91646c97L, 0xe6635c01L,
  0x6b6b51f4L, 0x1c6c6162L, 0x856530d8L, 0xf262004eL, 0x6c0695edL,
  0x1b01a57bL, 0x8208f4c1L, 0xf50fc457L, 0x65b0d9c6L, 0x12b7e950L,
  0x8bbeb8eaL, 0xfcb9887cL, 0x62dd1ddfL, 0x15da2d49L, 0x8cd37cf3L,
  0xfbd44c65L, 0x4db26158L, 0x3ab551ceL, 0xa3bc0074L, 0xd4bb30e2L,
  0x4adfa541L, 0x3dd895d7L, 0xa4d1c46dL, 0xd3d6f4fbL, 0x4369e96aL,
  0x346ed9fcL, 0xad678846L, 0xda60b8d0L, 0x44042d73L, 0x33031de5L,
  0xaa0a4c5fL, 0xdd0d7cc9L, 0x5005713cL, 0x270241aaL, 0xbe0b1010L,
  0xc90c2086L, 0x5768b525L, 0x206f85b3L, 0xb966d409L, 0xce61e49fL,
  0x5edef90eL, 0x29d9c998L, 0xb0d09822L, 0xc7d7a8b4L, 0x59b33d17L,
  0x2eb40d81L, 0xb7bd5c3bL, 0xc0ba6cadL, 0xedb88320L, 0x9abfb3b6L,
  0x03b6e20cL, 0x74b1d29aL, 0xead54739L, 0x9dd277afL, 0x04db2615L,
  0x73dc1683L, 0xe3630b12L, 0x94643b84L, 0x0d6d6a3eL, 0x7a6a5aa8L,
  0xe40ecf0bL, 0x9309ff9dL, 0x0a00ae27L, 0x7d079eb1L, 0xf00f9344L,
  0x8708a3d2L, 0x1e01f268L, 0x6906c2feL, 0xf762575dL, 0x806567cbL,
  0x196c3671L, 0x6e6b06e7L, 0xfed41b76L, 0x89d32be0L, 0x10da7a5aL,
  0x67dd4accL, 0xf9b9df6fL, 0x8ebeeff9L, 0x17b7be43L, 0x60b08ed5L,
  0xd6d6a3e8L, 0xa1d1937eL, 0x38d8c2c4L, 0x4fdff252L, 0xd1bb67f1L,
  0xa6bc5767L, 0x3fb506ddL, 0x48b2364bL, 0xd80d2bdaL, 0xaf0a1b4cL,
  0x36034af6L, 0x41047a60L, 0xdf60efc3L, 0xa867df55L, 0x316e8eefL,
  0x4669be79L, 0xcb61b38cL, 0xbc66831aL, 0x256fd2a0L, 0x5268e236L,
  0xcc0c7795L, 0xbb0b4703L, 0x220216b9L, 0x5505262fL, 0xc5ba3bbeL,
  0xb2bd0b28L, 0x2bb45a92L, 0x5cb36a04L, 0xc2d7ffa7L, 0xb5d0cf31L,
  0x2cd99e8bL, 0x5bdeae1dL, 0x9b64c2b0L, 0xec63f226L, 0x756aa39cL,
  0x026d930aL, 0x9c0906a9L, 0xeb0e363fL, 0x72076785L, 0x05005713L,
  0x95bf4a82L, 0xe2b87a14L, 0x7bb12baeL, 0x0cb61b38L, 0x92d28e9bL,
  0xe5d5be0dL, 0x7cdcefb7L, 0x0bdbdf21L, 0x86d3d2d4L, 0xf1d4e242L,
  0x68ddb3f8L, 0x1fda836eL, 0x81be16cdL, 0xf6b9265bL, 0x6fb077e1L,
  0x18b74777L, 0x88085ae6L, 0xff0f6a70L, 0x66063bcaL, 0x11010b5cL,
  0x8f659effL, 0xf862ae69L, 0x616bffd3L, 0x166ccf45L, 0xa00ae278L,
  0xd70dd2eeL, 0x4e048354L, 0x3903b3c2L, 0xa7672661L, 0xd06016f7L,
  0x4969474dL, 0x3e6e77dbL, 0xaed16a4aL, 0xd9d65adcL, 0x40df0b66L,
  0x37d83bf0L, 0xa9bcae53L, 0xdebb9ec5L, 0x47b2cf7fL, 0x30b5ffe9L,
  0xbdbdf21cL, 0xcabac28aL, 0x53b39330L, 0x24b4a3a6L, 0xbad03605L,
  0xcdd70693L, 0x54de5729L, 0x23d967bfL, 0xb3667a2eL, 0xc4614ab8L,
  0x5d681b02L, 0x2a6f2b94L, 0xb40bbe37L, 0xc30c8ea1L, 0x5a05df1bL,
  0x2d02ef8dL
};

#define DO1(buf) crc = crc_table[((int)crc ^ (*buf++)) & 0xff] ^ (crc >> 8);
#define DO2(buf)  DO1(buf); DO1(buf);
#define DO4(buf)  DO2(buf); DO2(buf);
#define DO8(buf)  DO4(buf); DO4(buf);

ULONG GenCRC32(ULONG crc, APTR Data, ULONG len)
{
   if (!Data) return 0;

   BYTE *buf = (BYTE *)Data;
   crc = crc ^ 0xffffffff;
   while (len >= 8) {
      DO8(buf);
      len -= 8;
   }

   if (len) do {
      DO1(buf);
   } while (--len);

   return crc ^ 0xffffffff;
}

#else

// CRC calculation routine from Zlib, written by Rodney Brown <rbrown64@csc.com.au>

typedef ULONG u4;
typedef int ptrdiff_t;

#define REV(w) (((w)>>24)+(((w)>>8)&0xff00)+(((w)&0xff00)<<8)+(((w)&0xff)<<24))
static ULONG crc32_little(ULONG, const UBYTE *, unsigned);
static ULONG crc32_big(ULONG, const UBYTE *, unsigned);
static volatile int crc_table_empty = 1;
static ULONG crc_table[8][256];

#define DO1 crc = crc_table[0][((int)crc ^ (*buf++)) & 0xff] ^ (crc >> 8)
#define DO8 DO1; DO1; DO1; DO1; DO1; DO1; DO1; DO1
#define DOLIT4 c ^= *buf4++; \
        c = crc_table[3][c & 0xff] ^ crc_table[2][(c >> 8) & 0xff] ^ \
            crc_table[1][(c >> 16) & 0xff] ^ crc_table[0][c >> 24]
#define DOLIT32 DOLIT4; DOLIT4; DOLIT4; DOLIT4; DOLIT4; DOLIT4; DOLIT4; DOLIT4
#define DOBIG4 c ^= *++buf4; \
        c = crc_table[4][c & 0xff] ^ crc_table[5][(c >> 8) & 0xff] ^ \
            crc_table[6][(c >> 16) & 0xff] ^ crc_table[7][c >> 24]
#define DOBIG32 DOBIG4; DOBIG4; DOBIG4; DOBIG4; DOBIG4; DOBIG4; DOBIG4; DOBIG4

ULONG GenCRC32(ULONG crc, APTR Data, ULONG len)
{
   if (!Data) return 0;

   UBYTE *buf = Data;
   if (crc_table_empty) {
      ULONG c;
      LONG n, k;
      ULONG poly;
      // terms of polynomial defining this crc (except x^32):
      static const UBYTE p[] = {0,1,2,4,5,7,8,10,11,12,16,22,23,26};

      // make exclusive-or pattern from polynomial (0xedb88320UL)

      poly = 0;
      for (n = 0; (size_t)n < sizeof(p)/sizeof(UBYTE); n++) {
         poly |= 1 << (31 - p[n]);
      }

      // generate a crc for every 8-bit value

      for (n = 0; n < 256; n++) {
         c = (ULONG)n;
         for (k = 0; k < 8; k++) {
            c = c & 1 ? poly ^ (c >> 1) : c >> 1;
         }
         crc_table[0][n] = c;
      }

      // Generate CRC for each value followed by one, two, and three zeros, and then the byte reversal of those as well
      // as the first table

      for (n = 0; n < 256; n++) {
         c = crc_table[0][n];
         crc_table[4][n] = REV(c);
         for (k = 1; k < 4; k++) {
            c = crc_table[0][c & 0xff] ^ (c >> 8);
            crc_table[k][n] = c;
            crc_table[k + 4][n] = REV(c);
         }
      }

      crc_table_empty = 0;
   }

   if (sizeof(void *) == sizeof(ptrdiff_t)) {
      if constexpr (std::endian::native == std::endian::little) {
         return crc32_little(crc, buf, len);
      }
      else return crc32_big(crc, buf, len);
   }

   crc = crc ^ 0xffffffff;
   while (len >= 8) {
      DO8;
      len -= 8;
   }

   if (len) do {
      DO1;
   } while (--len);

   return crc ^ 0xffffffff;
}

static ULONG crc32_little(ULONG crc, const UBYTE *buf, unsigned len)
{
   register u4 c;
   register const u4 *buf4;

   c = (u4)crc;
   c = ~c;
   while (len && ((ptrdiff_t)buf & 3)) {
      c = crc_table[0][(c ^ *buf++) & 0xff] ^ (c >> 8);
      len--;
   }

   buf4 = (const u4 *)(const void *)buf;
   while (len >= 32) {
      DOLIT32;
      len -= 32;
   }
   while (len >= 4) {
      DOLIT4;
      len -= 4;
   }
   buf = (const UBYTE *)buf4;

   if (len) do {
      c = crc_table[0][(c ^ *buf++) & 0xff] ^ (c >> 8);
   } while (--len);
   c = ~c;
   return (ULONG)c;
}

static ULONG crc32_big(ULONG crc, const UBYTE *buf, unsigned len)
{
   register u4 c;
   register const u4 *buf4;

   c = REV((u4)crc);
   c = ~c;
   while (len && ((ptrdiff_t)buf & 3)) {
       c = crc_table[4][(c >> 24) ^ *buf++] ^ (c << 8);
       len--;
   }

   buf4 = (const u4 *)(const void *)buf;
   buf4--;
   while (len >= 32) {
       DOBIG32;
       len -= 32;
   }
   while (len >= 4) {
       DOBIG4;
       len -= 4;
   }
   buf4++;
   buf = (const UBYTE *)buf4;

   if (len) do {
       c = crc_table[4][(c >> 24) ^ *buf++] ^ (c << 8);
   } while (--len);
   c = ~c;
   return (ULONG)(REV(c));
}

#endif

/*********************************************************************************************************************

-FUNCTION-
GetResource: Retrieves miscellaneous resource identifiers.

The GetResource() function is used to retrieve miscellaneous resource information from the system core.  Refer to the
Resource identifier for the full list of available resource codes and their meaning.

C++ developers should use the GetResourcePtr() macro if a resource identifier is known to return a pointer.

-INPUT-
int(RES) Resource: The ID of the resource that you want to obtain.

-RESULT-
large: Returns the value of the resource that you have requested.  If the resource ID is not known by the Core, NULL is returned.
-END-

*********************************************************************************************************************/

LARGE GetResource(LONG Resource)
{
#ifdef __linux__
   struct sysinfo sys;
#endif

   switch(Resource) {
      case RES_MESSAGE_QUEUE:   return glTaskMessageMID;
      case RES_SHARED_CONTROL:  return (MAXINT)glSharedControl;
      case RES_PRIVILEGED:      return glPrivileged;
      case RES_LOG_LEVEL:       return glLogLevel;
      case RES_PROCESS_STATE:   return (MAXINT)glTaskState;
      case RES_MAX_PROCESSES:   return MAX_TASKS;
      case RES_LOG_DEPTH:       return tlDepth;
      case RES_CURRENT_MSG:     return (MAXINT)tlCurrentMsg;
      case RES_OPEN_INFO:       return (MAXINT)glOpenInfo;
      case RES_JNI_ENV:         return (MAXINT)glJNIEnv;
      case RES_THREAD_ID:       return (MAXINT)get_thread_id();
      case RES_CORE_IDL:        return (MAXINT)glIDL;
      case RES_DISPLAY_DRIVER:  if (glDisplayDriver[0]) return (MAXINT)glDisplayDriver; else return 0;

      case RES_PARENT_CONTEXT: {
         // Return the first parent context that differs to the current context.  This avoids any confusion
         // arising from the the current object making calls to itself.
         auto parent = tlContext->Stack;
         while ((parent) and (parent->object() IS tlContext->object())) parent = parent->Stack;
         return parent ? (MAXINT)parent->object() : 0;
      }

#ifdef __linux__
      // NB: This value is not cached.  Although unlikely, it is feasible that the total amount of physical RAM could
      // change during runtime.

      case RES_TOTAL_MEMORY: {
         if (!sysinfo(&sys)) return (LARGE)sys.totalram * (LARGE)sys.mem_unit;
         else return -1;
      }

      case RES_FREE_MEMORY: {
   #if 0
         // Unfortunately sysinfo() does not report on cached ram, which can be significant
         if (!sysinfo(&sys)) return (LARGE)(sys.freeram + sys.bufferram) * (LARGE)sys.mem_unit; // Buffer RAM is considered as 'free'
   #else
         char str[2048];
         LONG result;
         LARGE freemem = 0;
         if (!ReadFileToBuffer("/proc/meminfo", str, sizeof(str)-1, &result)) {
            LONG i = 0;
            while (i < result) {
               if (!StrCompare("Cached", str+i, sizeof("Cached")-1, 0)) {
                  freemem += (LARGE)StrToInt(str+i) * 1024LL;
               }
               else if (!StrCompare("Buffers", str+i, sizeof("Buffers")-1, 0)) {
                  freemem += (LARGE)StrToInt(str+i) * 1024LL;
               }
               else if (!StrCompare("MemFree", str+i, sizeof("MemFree")-1, 0)) {
                  freemem += (LARGE)StrToInt(str+i) * 1024LL;
               }

               while ((i < result) and (str[i] != '\n')) i++;
               i++;
            }
         }

         return freemem;
   #endif
      }

      case RES_TOTAL_SHARED_MEMORY:
         if (!sysinfo(&sys)) return (LARGE)sys.sharedram * (LARGE)sys.mem_unit;
         else return -1;

      case RES_TOTAL_SWAP:
         if (!sysinfo(&sys)) return (LARGE)sys.totalswap * (LARGE)sys.mem_unit;
         else return -1;

      case RES_FREE_SWAP:
         if (!sysinfo(&sys)) return (LARGE)sys.freeswap * (LARGE)sys.mem_unit;
         else return -1;

      case RES_CPU_SPEED: {
         CSTRING line;
         static LONG cpu_mhz = 0;

         if (cpu_mhz) return cpu_mhz;

         objFile::create file = { fl::Path("drive1:proc/cpuinfo"), fl::Flags(FL_READ|FL_BUFFER) };

         if (file.ok()) {
            while ((line = flReadLine(*file))) {
               if (!StrCompare("cpu Mhz", line, sizeof("cpu Mhz")-1, 0)) {
                  cpu_mhz = StrToInt(line);
               }
            }
         }

         return cpu_mhz;
      }
#elif __APPLE__
#warning TODO: Support for sysctlbyname()
#endif

      default: //log.warning("Unsupported resource ID %d.", Resource);
         break;
   }

   return 0;
}
/*********************************************************************************************************************

-FUNCTION-
GetSystemState: Returns miscellaneous data values from the Core.

The GetSystemState() function is used to retrieve miscellaneous resource and environment values, such as resource
paths, the Core's version number and the name of the host platform.

-RESULT-
cstruct(*SystemState): A read-only SystemState structure is returned.

*********************************************************************************************************************/

const SystemState * GetSystemState(void)
{
   static bool initialised = false;
   static SystemState state;

   if (!initialised) {
      initialised = true;

      state.ConsoleFD     = glConsoleFD;
      state.CoreVersion   = VER_CORE;
      state.CoreRevision  = REV_CORE;
      #ifdef __unix__
         state.Platform = "Linux";
      #elif _WIN32
         state.Platform = "Windows";
      #elif __APPLE__
         state.Platform = "OSX";
      #else
         state.Platform = "Unknown";
      #endif
   }

   state.Stage = glSharedControl->SystemState;
   return &state;
}

/*********************************************************************************************************************

-FUNCTION-
PreciseTime: Returns the current system time, in microseconds.

This function returns the current 'system time' in microseconds (1 millionth of a second).  The value is monotonic
if the host platform allows it (typically expressed as the amount of time that has elapsed since the system was
switched on).  The benefit of monotonic time is that it is unaffected by changes to the system clock, such as daylight
savings adjustments or manual changes by the user.

-RESULT-
large: Returns the system time in microseconds.  An error is extremely unlikely, but zero is returned in the event of one.

*********************************************************************************************************************/

LARGE PreciseTime(void)
{
#ifdef __unix__
   struct timespec time;

   if (!clock_gettime(CLOCK_MONOTONIC, &time)) {
      return ((LARGE)time.tv_sec * 1000000LL) + ((LARGE)time.tv_nsec / 1000LL);
   }
   else return 0;
#else
   return winGetTickCount(); // NB: This timer does start from the boot time, but can be adjusted - therefore is not 100% on monotonic status
#endif
}

/*********************************************************************************************************************

-FUNCTION-
RegisterFD: Registers a file descriptor for monitoring when the task is asleep.

This function will register a file descriptor that will be monitored for activity when the task is sleeping.  If
activity occurs on the descriptor then the function specified in the Routine parameter will be called.  The routine
must read all of information from the descriptor, as the running process will not be able to sleep until all the
data is cleared.

The file descriptor should be configured as non-blocking before registration.  Blocking descriptors may cause the
program to hang if not handled carefully.

File descriptors support read and write states simultaneously and a callback routine can be applied to either state.
Set the `RFD_READ` flag to apply the Routine to the read callback and `RFD_WRITE` for the write callback.  If neither
flag is specified, `RFD_READ` is assumed.  A file descriptor may have up to 1 subscription per flag, for example a read
callback can be registered, followed by a write callback in a second call. Individual callbacks can be removed by
combining the read/write flags with `RFD_REMOVE`.

The capabilities of this function and FD handling in general is developed to suit the host platform. On POSIX
compliant systems, standard file descriptors are used.  In Microsoft Windows, object handles are used and blocking
restrictions do not apply, except to sockets.

Call the DeregisterFD() macro to simplify unsubscribing once the file descriptor is no longer needed or is destroyed.

-INPUT-
hhandle FD: The file descriptor that is to be watched.
int(RFD) Flags: Set to one or more of the flags RFD_READ, RFD_WRITE, RFD_EXCEPT, RFD_REMOVE.
fptr(void hhandle ptr) Routine: The routine that will read from the descriptor when data is detected on it.  The template for the function is "void Routine(LONG FD, APTR Data)".
ptr Data: User specific data pointer that will be passed to the Routine.  Separate data pointers apply to the read and write states of operation.

-ERRORS-
Okay: The FD was successfully registered.
Args: The FD was set to a value of -1.
NoSupport: The host platform does not support file descriptors.
-END-

*********************************************************************************************************************/

#ifdef _WIN32
ERROR RegisterFD(HOSTHANDLE FD, LONG Flags, void (*Routine)(HOSTHANDLE, APTR), APTR Data)
#else
ERROR RegisterFD(LONG FD, LONG Flags, void (*Routine)(HOSTHANDLE, APTR), APTR Data)
#endif
{
   pf::Log log(__FUNCTION__);

   // Note that FD's < -1 are permitted for the registering of functions marked with RFD_ALWAYS_CALL

#ifdef _WIN32
   if (FD IS (HOSTHANDLE)-1) return log.warning(ERR_Args);
   if (Flags & RFD_SOCKET) return log.warning(ERR_NoSupport); // In MS Windows, socket handles are managed as window messages (see Network module's Windows code)
#else
   if (FD IS -1) return log.warning(ERR_Args);
#endif

   if (glFDProtected) { // Cache the request while glFDTable is in use.
      glRegisterFD.emplace_back(FD, Routine, Data, Flags);
      return ERR_Okay;
   }

   if (Flags & RFD_REMOVE) {
      if (!(Flags & (RFD_READ|RFD_WRITE|RFD_EXCEPT|RFD_ALWAYS_CALL))) Flags |= RFD_READ|RFD_WRITE|RFD_EXCEPT|RFD_ALWAYS_CALL;

      for (auto it = glFDTable.begin(); it != glFDTable.end();) {
         if ((it->FD IS FD) and ((it->Flags & (RFD_READ|RFD_WRITE|RFD_EXCEPT|RFD_ALWAYS_CALL)) & Flags)) {
            if ((Routine) and (it->Routine != Routine)) it++;
            else it = glFDTable.erase(it);
         }
         else it++;
      }
      return ERR_Okay;
   }

   if (!(Flags & (RFD_READ|RFD_WRITE|RFD_EXCEPT|RFD_REMOVE|RFD_ALWAYS_CALL))) Flags |= RFD_READ;

   for (auto &fd : glFDTable) {
      if ((fd.FD IS FD) and (Flags & (fd.Flags & (RFD_READ|RFD_WRITE|RFD_EXCEPT|RFD_ALWAYS_CALL)))) {
         fd.Routine = Routine;
         fd.Flags   = Flags;
         fd.Data    = Data;
         return ERR_Okay;
      }
   }

   log.function("FD: %" PF64 ", Routine: %p, Flags: $%.2x (New)", (MAXINT)FD, Routine, Flags);

#ifdef _WIN32
   // Nothing to do for Win32
#else
   if ((!Routine) and (FD > 0)) fcntl(FD, F_SETFL, fcntl(FD, F_GETFL) | O_NONBLOCK); // Ensure that the FD is non-blocking
#endif

   glFDTable.emplace_back(FD, Routine, Data, Flags);
   return ERR_Okay;
}

/*********************************************************************************************************************

-FUNCTION-
SetResourcePath: Redefines the location of a system resource path.

The SetResourcePath() function changes the default locations of the Core's resource paths.

To read a resource path, use the ~GetSystemState() function.

-INPUT-
int(RP) PathType: The ID of the resource path to set.
cstr Path: The new location to set for the resource path.

-ERRORS-
Okay:
NullArgs:

*********************************************************************************************************************/

ERROR SetResourcePath(LONG PathType, CSTRING Path)
{
   pf::Log log(__FUNCTION__);

   if (!PathType) return ERR_NullArgs;

   log.function("Type: %d, Path: %s", PathType, Path);

   switch(PathType) {
      case RP_ROOT_PATH:
         if (Path) {
            glRootPath = Path;
            if ((glRootPath.back() != '/') and (glRootPath.back() != '\\')) {
               #ifdef _WIN32
                  glRootPath.push_back('\\');
               #else
                  glRootPath.push_back('/');
               #endif
            }
         }
         return ERR_Okay;

      case RP_SYSTEM_PATH:
         if (Path) {
            glSystemPath = Path;
            if ((glSystemPath.back() != '/') and (glSystemPath.back() != '\\')) {
               #ifdef _WIN32
                  glSystemPath.push_back('\\');
               #else
                  glSystemPath.push_back('/');
               #endif
            }
         }
         return ERR_Okay;

      case RP_MODULE_PATH: // An alternative path to the system modules.  This was introduced for Android, which holds the module binaries in the assets folders.
         if (Path) {
            glModulePath = Path;
            if ((glModulePath.back() != '/') and (glModulePath.back() != '\\')) {
               #ifdef _WIN32
                  glModulePath += '\\';
               #else
                  glModulePath += '/';
               #endif
            }
         }
         return ERR_Okay;

      default:
         return ERR_Args;
   }
}

/*********************************************************************************************************************

-FUNCTION-
SetResource: Sets miscellaneous resource identifiers.

The SetResource() function is used to manipulate miscellaneous system resources.  Currently the following resources
are supported:

<types prefix="RES" type="Resource">
<type name="ALLOC_MEM_LIMIT">Adjusts the memory limit imposed on AllocMemory().  The Value specifies the memory limit in bytes.</>
<type name="LOG_LEVEL">Adjusts the current debug level.  The Value must be between 0 and 9, where 1 is the lowest level of debug output (errors only) and 0 is off.</>
<type name="PRIVILEGED_USER">If the Value is set to 1, this resource option puts the process in privileged mode (typically this enables full administrator rights).  This feature will only work for Unix processes that are granted admin rights when launched.  Setting the Value to 0 reverts to the user's permission settings.  SetResource() will return an error code indicating the level of success.</>
</>

-INPUT-
int(RES) Resource: The ID of the resource to be set.
large Value:    The new value to set for the resource.

-RESULT-
large: Returns the previous value used for the resource that you have set.  If the resource ID that you provide is invalid, NULL is returned.
-END-

*********************************************************************************************************************/

LARGE SetResource(LONG Resource, LARGE Value)
{
   pf::Log log(__FUNCTION__);

#ifdef __unix__
   static WORD privileged = 0;
#endif

   LARGE oldvalue = 0;

   switch(Resource) {
      case RES_CONSOLE_FD: glConsoleFD = (HOSTHANDLE)(MAXINT)Value; break;

      case RES_EXCEPTION_HANDLER:
         // Note: You can set your own crash handler, or set a value of NULL - this resets the existing handler which is useful if an external DLL function is suspected to have changed the filter.

         #ifdef _WIN32
            winSetUnhandledExceptionFilter((LONG (*)(LONG, APTR, LONG, LONG *))L64PTR(Value));
         #endif
         break;

      case RES_LOG_LEVEL:
         if ((Value >= 0) and (Value <= 9)) glLogLevel = Value;
         break;

      case RES_LOG_DEPTH: tlDepth = Value; break;

#ifdef _WIN32
      case RES_NET_PROCESSING: glNetProcessMessages = (void (*)(LONG, APTR))L64PTR(Value); break;
#else
      case RES_NET_PROCESSING: break;
#endif

      case RES_JNI_ENV: glJNIEnv = L64PTR(Value); break;

      case RES_PRIVILEGED_USER:
#ifdef __unix__
         log.trace("Privileged User: %s, Current UID: %d, Depth: %d", (Value) ? "TRUE" : "FALSE", geteuid(), privileged);

         if (glPrivileged) return ERR_Okay; // In privileged mode, the user is always an admin

         if (Value) { // Enable admin privileges
            oldvalue = ERR_Okay;
            if (!privileged) {
               if (glUID) {
                  if (glUID != glEUID) {
                     seteuid(glEUID);
                     privileged++;
                  }
                  else {
                     log.msg("Admin privileges not available.");
                     oldvalue = ERR_Failed; // Admin privileges are not available
                  }
               }
               else privileged++;; // The user already has admin privileges
            }
            else privileged++;
         }
         else {
            // Disable admin privileges
            if (privileged > 0) {
               privileged--;
               if (!privileged) {
                  if (glUID != glEUID) seteuid(glUID);
               }
            }
         }
#else
         return ERR_Okay;
#endif
         break;

      default:
         log.warning("Unrecognised resource ID: %d, Value: %" PF64, Resource, Value);
   }

   return oldvalue;
}

/*********************************************************************************************************************

-FUNCTION-
SubscribeTimer: Subscribes an object or function to the timer service.

This function creates a new timer subscription that will be called at regular intervals for the calling object.

A callback function must be provided that follows this prototype: `ERROR Function(OBJECTPTR Subscriber, LARGE Elapsed, LARGE CurrentTime)`

The Elapsed parameter is the total number of microseconds that have elapsed since the last call.  The CurrentTime
parameter is set to the ~PreciseTime() value just prior to the Callback being called.  The callback function
can return `ERR_Terminate` at any time to cancel the subscription.  All other error codes are ignored.  Fluid callbacks
should call `check(ERR_Terminate)` to perform the equivalent of this behaviour.

To change the interval, call ~UpdateTimer() with the new value.  To release a timer subscription, call
~UpdateTimer() with the resulting SubscriptionID and an Interval of zero.

Timer management is provisioned by the ~ProcessMessages() function.  Failure to regularly process incoming
messages will lead to unreliable timer cycles.  It should be noted that the smaller the Interval that has been used,
the more imperative regular message checking becomes.  Prolonged processing inside a timer routine can also impact on
other timer subscriptions that are waiting to be processed.

-INPUT-
double Interval:   The total number of seconds to wait between timer calls.
ptr(func) Callback: A callback function is required that will be called on each time cycle.
&ptr Subscription: Optional.  The subscription will be assigned an identifier that is returned in this parameter.

-ERRORS-
Okay:
NullArgs:
Args:
ArrayFull: The task's timer array is at capacity - no more subscriptions can be granted.
InvalidState: The subscriber is marked for termination.
SystemLocked:

*********************************************************************************************************************/

ERROR SubscribeTimer(DOUBLE Interval, FUNCTION *Callback, APTR *Subscription)
{
   pf::Log log(__FUNCTION__);

   if ((!Interval) or (!Callback)) return log.warning(ERR_NullArgs);
   if (Interval < 0) return log.warning(ERR_Args);

   auto subscriber = tlContext->object();
   if (subscriber->collecting()) return log.warning(ERR_InvalidState);

   if (Callback->Type IS CALL_SCRIPT) log.msg(VLF_BRANCH|VLF_FUNCTION|VLF_DEBUG, "Interval: %.3fs", Interval);
   else log.msg(VLF_BRANCH|VLF_FUNCTION|VLF_DEBUG, "Callback: %p, Interval: %.3fs", Callback->StdC.Routine, Interval);

   ThreadLock lock(TL_TIMER, 200);
   if (lock.granted()) {
      LARGE usInterval = (LARGE)(Interval * 1000000.0); // Scale the interval to microseconds
      if (usInterval <= 40000) {
         // TODO: Rapid timers should be synchronised with other existing timers to limit the number of
         // interruptions that occur per second.
      }

      auto it = glTimers.emplace(glTimers.end());
      LARGE subscribed = PreciseTime();
      it->SubscriberID = subscriber->UID;
      it->Interval     = usInterval;
      it->LastCall     = subscribed;
      it->NextCall     = subscribed + usInterval;
      it->Routine      = *Callback;
      it->Locked       = false;
      it->Cycle        = glTimerCycle - 1;

      if (subscriber->UID > 0) it->Subscriber = subscriber;
      else it->Subscriber = NULL;

      // For resource tracking purposes it is important for us to keep a record of the subscription so that
      // we don't treat the object address as valid when it's been removed from the system.

      subscriber->Flags |= NF::TIMER_SUB;

      if (Subscription) *Subscription = &*it;

      return ERR_Okay;
   }
   else return log.warning(ERR_SystemLocked);
}

/*********************************************************************************************************************

-FUNCTION-
UpdateTimer: Modify or remove a subscription created by SubscribeTimer().

This function complements ~SubscribeTimer().  It can change the interval for an existing timer subscription,
or remove it if the Interval is set to zero.

-INPUT-
ptr Subscription: The timer subscription to modify.
double Interval: The new interval for the timer (measured in seconds), or zero to remove.

-ERRORS-
Okay:
NullArgs:
SystemLocked:
Search:

*********************************************************************************************************************/

ERROR UpdateTimer(APTR Subscription, DOUBLE Interval)
{
   pf::Log log(__FUNCTION__);

   if (!Subscription) return log.warning(ERR_NullArgs);

   log.msg(VLF_EXTAPI|VLF_BRANCH|VLF_FUNCTION, "Subscription: %p, Interval: %.4f", Subscription, Interval);

   ThreadLock lock(TL_TIMER, 200);
   if (lock.granted()) {
      auto timer = (CoreTimer *)Subscription;
      if (Interval < 0) {
         // Special mode: Preserve existing timer settings for the subscriber (ticker values are not reset etc)
         LARGE usInterval = -((LARGE)(Interval * 1000000.0));
         if (usInterval < timer->Interval) timer->Interval = usInterval;
         return ERR_Okay;
      }
      else if (Interval > 0) {
         LARGE usInterval = (LARGE)(Interval * 1000000.0);
         timer->Interval = usInterval;
         timer->NextCall = PreciseTime() + usInterval;
         return ERR_Okay;
      }
      else {
         if (timer->Locked) {
            // A timer can't be removed during its execution, but we can nullify the function entry
            // and ProcessMessages() will automatically terminate it on the next cycle.
            timer->Routine.Type = 0;
            return log.warning(ERR_AlreadyLocked);
         }

         lock.release();

         if (timer->Routine.Type IS CALL_SCRIPT) {
            scDerefProcedure(timer->Routine.Script.Script, &timer->Routine);
         }

         for (auto it=glTimers.begin(); it != glTimers.end(); it++) {
            if (timer IS &(*it)) {
               glTimers.erase(it);
               break;
            }
         }

         return ERR_Okay;
      }
   }
   else return log.warning(ERR_SystemLocked);
}

/*********************************************************************************************************************

-FUNCTION-
WaitTime: Waits for a specified amount of seconds and/or microseconds.

This function waits for a period of time as specified by the Seconds and MicroSeconds arguments.  While waiting, your
task will continue to process incoming messages in order to prevent the process' message queue from developing a
back-log.

WaitTime() can return earlier than the indicated timeout if a message handler returns `ERR_Terminate`, or if a
`MSGID_QUIT` message is sent to the task's message queue.

-INPUT-
int Seconds:      The number of seconds to wait for.
int MicroSeconds: The number of microseconds to wait for.  Please note that a microsecond is one-millionth of a second - 1/1000000.  The value cannot exceed 999999.

-END-

*********************************************************************************************************************/

void WaitTime(LONG Seconds, LONG MicroSeconds)
{
   bool process_msg;

   if (!tlMainThread) {
      process_msg = false; // Child threads never process messages.
      if (Seconds < 0) Seconds = -Seconds;
      if (MicroSeconds < 0) MicroSeconds = -MicroSeconds;
    }
   else {
      // If the Seconds or MicroSeconds arguments are negative, turn off the ProcessMessages() support.
      process_msg = true;
      if (Seconds < 0) { Seconds = -Seconds; process_msg = false; }
      if (MicroSeconds < 0) { MicroSeconds = -MicroSeconds; process_msg = false; }
   }

   while (MicroSeconds >= 1000000) {
      MicroSeconds -= 1000000;
      Seconds++;
   }

   if (process_msg) {
      LARGE current = PreciseTime() / 1000LL;
      LARGE end = current + (Seconds * 1000) + (MicroSeconds / 1000);
      do {
         if (ProcessMessages(0, end - current) IS ERR_Terminate) break;
         current = (PreciseTime() / 1000LL);
      } while (current < end);
   }
   else {
      #ifdef __unix__
         struct timespec nano;
         nano.tv_sec  = Seconds;
         nano.tv_nsec = MicroSeconds * 100;
         while (nanosleep(&nano, &nano) == -1) continue;
      #elif _WIN32
         winSleep((Seconds * 1000) + (MicroSeconds / 1000));
      #else
         #warn Platform needs support for WaitTime()
      #endif
   }
}
