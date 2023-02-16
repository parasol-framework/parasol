/*********************************************************************************************************************

The source code of the Parasol Framework is made publicly available under the terms described in the LICENSE.TXT file
that is distributed with this package.  Please refer to it for further information on licensing.

-CATEGORY-
Name: System
-END-

This file contains all logging functions.

Log levels are:

0  CRITICAL Display the message irrespective of the log level.
1  ERROR Major errors that should be displayed to the user.
2  WARN Any error suitable for display to a developer or technically minded user.
3  Application log message, level 1
4  INFO Application log message, level 2
5  API Top-level API messages, e.g. function entry points (default)
6  EXTAPI Extended API messages.  For messages within functions, and entry-points for minor functions.
7  DEBUG More detailed API messages.
8  TRACE Extremely detailed API messages suitable for intensive debugging only.
9  Noisy debug messages that will appear frequently, e.g. being used in inner loops.

*********************************************************************************************************************/

#include <stdio.h>
#include <unistd.h>
#include <stdarg.h>
#include <fcntl.h>

#ifdef __ANDROID__
#include <android/log.h>
#endif

#include "defs.h"

static void fmsg(CSTRING, STRING, BYTE, BYTE);

#ifdef __ANDROID__
static const int COLUMN1 = 40;
#else
static const int COLUMN1 = 30;
#endif

//#if !defined(_WIN32)
   #define ESC_OUTPUT 1
//#endif

enum { MS_NONE, MS_FUNCTION, MS_MSG };
enum { EL_NONE=0, EL_MINOR, EL_MAJOR, EL_MAJORBOLD };

static THREADVAR LONG tlBaseLine = 0;

/*********************************************************************************************************************

-FUNCTION-
AdjustLogLevel: Adjusts the base-line of all log messages.

This function adjusts the detail level of all messages that are sent to the program log via ~LogF().  To illustrate by
example, setting the Adjust value to 1 would result in level 5 log messages being raised to level 6.  If the user's
maximum log level output is 5, such messages would no longer appear in the log until the base-line is reduced to normal.

The main purpose of AdjustLogLevel() is to reduce log noise.  For instance, creating a new desktop window will result
in a large number of new log messages.  Raising the base-line by 2 before creating the window would be a reasonable
means of eliminating that noise if the user has the log level set to 5 (debug).  If there is a need to see the mesages,
re-running the program with a deeper log level of 7 or more would make them visible.

Adjustments to the base-line are accumulative, so small increments of 1 or 2 are encouraged.  To revert logging to the
previous base-line, call this function again with a negation of the previously passed value.

-INPUT-
int Adjust: The level of adjustment to make to new log messages.  Zero is the default (no change).  The maximum level is 6.

-RESULT-
int: Returns the absolute base-line value that was active prior to calling this function.

*********************************************************************************************************************/

LONG AdjustLogLevel(LONG BaseLine)
{
   if (glLogLevel >= 9) return tlBaseLine; // Do nothing if trace logging is active.
   LONG old_level = tlBaseLine;
   if ((BaseLine >= -6) and (BaseLine <= 6)) tlBaseLine += BaseLine;
   return old_level;
}

/*********************************************************************************************************************

-FUNCTION-
LogF: Sends formatted messages to the standard log.
Attribute: __attribute__((format(printf, 2, 3)))
Status: private

The LogF() function follows the same functionality and rules as the ANSI printf() function, with the difference that
it is passed through a log filter before appearing as output.  Imposed limits restrict the length of the message to
256 bytes, although a 120 byte limit is recommended where possible.

The following example will print the default width of a Display object to the log.

<pre>
if (!NewObject(ID_DISPLAY, 0, &display)) {
   if (!display->init(display)) {
      LogF("Demo","The width of the display is: %d", display-&gt;Width);
   }
   acFree(display);
}
</pre>

-INPUT-
cstr Header: A short name for the first column. Typically function names are placed here, so that the origin of the message is obvious.
cstr Message: A formatted message to print.
printf Tags: As per printf() rules, one parameter for every % symbol that has been used in the Message string is required.
-END-

*********************************************************************************************************************/

void LogF(CSTRING Header, CSTRING Format, ...)
{
   CSTRING name, action;
   BYTE msglevel, new_branch, msgstate, adjust = 0;

   if (tlLogStatus <= 0) return;

   ThreadLock lock(TL_PRINT, -1);

   if (!Format) Format = "";

   if (Header) {
      if (*Header IS '~') { // ~ is the create-branch code
         new_branch = TRUE;
         msgstate = MS_FUNCTION;
         Header++;
      }
      else {
         new_branch = FALSE;
         msgstate = MS_MSG;
      }

      if (*Header IS '!') { // ! and @ are error level codes
         msglevel = 1;
         Header++;

         if (*Header IS '!') { // Extremely important error message that the user should see '!!'
            #ifdef __ANDROID__
               va_list arg;
               va_start(arg, Format);
               __android_log_vprint(ANDROID_LOG_ERROR, Header, Format, arg);
               va_end(arg);
            #else
               #ifdef ESC_OUTPUT
                  #ifdef _WIN32
                     fprintf(stderr, "!%s ", Header);
                  #else
                     fprintf(stderr, "\033[1m%s ", Header);
                  #endif
               #else
                  fprintf(stderr, "%s ", Header);
               #endif

               va_list arg;
               va_start(arg, Format);
               vfprintf(stderr, Format, arg);
               va_end(arg);

               #ifdef ESC_OUTPUT
                  fprintf(stderr, "\033[0m");
               #endif

               fprintf(stderr, "\n");
            #endif

            goto exit;
         }
      }
      else if (*Header IS '@') {
         if (glLogLevel < 2) goto exit;
         msglevel = 2;
         Header++;
      }
      else if (*Header IS '#') {
         msgstate = MS_FUNCTION;
         msglevel = 3;
         Header++;
      }
      else {
         msglevel = 3;
         if ((*Header >= '1') and (*Header <= '9')) { // Numbers are detail indicators
            msglevel = *Header - '1' + 3;
            Header++;
         }
      }

      if (!*Header) Header = NULL;
   }
   else {
      if (glLogLevel < 3) return;
      msglevel = 3;
      new_branch = FALSE;
      msgstate = MS_NONE;
   }

   msglevel += tlBaseLine;
   if (glLogLevel >= msglevel) {
      //fprintf(stderr, "%.8d. ", winGetCurrentThreadId());

      #if defined(__unix__) and !defined(__ANDROID__)
         BYTE flushdbg;
         if (glLogLevel >= 3) {
            flushdbg = TRUE;
            if (tlPublicLockCount) flushdbg = FALSE;
            if (flushdbg) fcntl(STDERR_FILENO, F_SETFL, glStdErrFlags & (~O_NONBLOCK));
         }
         else flushdbg = FALSE;
      #endif

      #ifdef ESC_OUTPUT
         if ((glLogLevel > 2) and (msglevel <= 2)) {
            #ifdef _WIN32
               fprintf(stderr, "!");
               adjust = 1;
            #else
               fprintf(stderr, "\033[1m");
            #endif
         }
      #endif

      // If no header is provided, make one to match the current context

      auto ctx = tlContext;
      auto obj = tlContext->object();
      if (ctx->Action) {
         if (ctx->Action < 0) {
            auto mc = obj->Class;
            if ((mc) and (mc->Methods) and (-ctx->Action < mc->TotalMethods)) {
               action = mc->Methods[-ctx->Action].Name;
            }
            else action = "Method";
         }
         else action = ActionTable[ctx->Action].Name;
      }
      else action = "App";

      if (!Header) {
         Header = action;
         action = NULL;
      }

      #ifdef __ANDROID__
         char msgheader[COLUMN1+1];

         fmsg(Header, msgheader, msgstate, 0);

         if (obj->Class) {
            char msg[180];

            if (obj->Name[0]) name = obj->Name;
            else name = obj->Class->Name;

            if (glLogLevel > 5) {
               if (ctx->Field) snprintf(msg, sizeof(msg), "[%s%s%s:%d:%s] %s", (action) ? action : (STRING)"", (action) ? ":" : "", name, obj->UID, ctx->Field->Name, Format);
               else snprintf(msg, sizeof(msg), "[%s%s%s:%d] %s", (action) ? action : (STRING)"", (action) ? ":" : "", name, obj->UID, Format);
            }
            else {
               if (ctx->Field) snprintf(msg, sizeof(msg), "[%s:%d:%s] %s", name, obj->UID, ctx->Field->Name, Format);
               else snprintf(msg, sizeof(msg), "[%s:%d] %s", name, obj->UID, Format);
            }

            va_list arg;
            va_start(arg, Format);
            __android_log_vprint((msglevel <= 2) ? ANDROID_LOG_ERROR : ANDROID_LOG_INFO, msgheader, msg, arg);
            va_end(arg);
         }
         else {
            va_list arg;
            va_start(arg, Format);
            __android_log_vprint((msglevel <= 2) ? ANDROID_LOG_ERROR : ANDROID_LOG_INFO, msgheader, Format, arg);
            va_end(arg);
         }

      #else
         char msgheader[COLUMN1+1];
         if (glLogLevel > 2) {
            fmsg(Header, msgheader, msgstate, adjust); // Print header with indenting
         }
         else {
            size_t len;
            for (len=0; (Header[len]) and (len < sizeof(msgheader)-2); len++) msgheader[len] = Header[len];
            msgheader[len++] = ' ';
            msgheader[len] = 0;
         }

         if (obj->Class) {
            if (obj->Name[0]) name = obj->Name;
            else name = obj->Class->ClassName;

            if (glLogLevel > 5) {
               if (ctx->Field) {
                  fprintf(stderr, "%s[%s%s%s:%d:%s] ", msgheader, (action) ? action : (STRING)"", (action) ? ":" : "", name, obj->UID, ctx->Field->Name);
               }
               else fprintf(stderr, "%s[%s%s%s:%d] ", msgheader, (action) ? action : (STRING)"", (action) ? ":" : "", name, obj->UID);
            }
            else if (ctx->Field) {
               fprintf(stderr, "%s[%s:%d:%s] ", msgheader, name, obj->UID, ctx->Field->Name);
            }
            else fprintf(stderr, "%s[%s:%d] ", msgheader, name, obj->UID);
         }
         else fprintf(stderr, "%s", msgheader);

         va_list arg;
         va_start(arg, Format);
         vfprintf(stderr, Format, arg);
         va_end(arg);

         #if defined(ESC_OUTPUT) and !defined(_WIN32)
            if ((glLogLevel > 2) and (msglevel <= 2)) fprintf(stderr, "\033[0m");
         #endif

         fprintf(stderr, "\n");

         #if defined(__unix__) and !defined(__ANDROID__)
            if (flushdbg) {
               fflush(0); // A fflush() appears to be enough - using fsync() will synchronise to disk, which we don't want by default (slow)
               if (glSync) fsync(STDERR_FILENO);
               fcntl(STDERR_FILENO, F_SETFL, glStdErrFlags);
            }
         #endif
      #endif
   }

exit:
   if (new_branch) tlDepth++;
}

/*********************************************************************************************************************

-FUNCTION-
VLogF: Sends formatted messages to the standard log.
ExtPrototype: int Flags, const char *Header, const char *Message, va_list Args
Status: private

Please refer to LogF().  This function is not intended for external use.

-INPUT-
int(VLF) Flags: Optional flags
cstr Header: A short name for the first column. Typically function names are placed here, so that the origin of the message is obvious.
cstr Message: A formatted message to print.
va_list Args: A va_list corresponding to the arguments referenced in Message.
-END-

*********************************************************************************************************************/

void VLogF(LONG Flags, CSTRING Header, CSTRING Message, va_list Args)
{
   if (tlLogStatus <= 0) { if (Flags & VLF_BRANCH) tlDepth++; return; }

   static const ULONG log_levels[10] = {
      VLF_CRITICAL,
      VLF_ERROR|VLF_CRITICAL,
      VLF_WARNING|VLF_ERROR|VLF_CRITICAL,
      VLF_INFO|VLF_WARNING|VLF_ERROR|VLF_CRITICAL,
      VLF_INFO|VLF_WARNING|VLF_ERROR|VLF_CRITICAL,
      VLF_API|VLF_INFO|VLF_WARNING|VLF_ERROR|VLF_CRITICAL,
      VLF_EXTAPI|VLF_API|VLF_INFO|VLF_WARNING|VLF_ERROR|VLF_CRITICAL,
      VLF_DEBUG|VLF_EXTAPI|VLF_API|VLF_INFO|VLF_WARNING|VLF_ERROR|VLF_CRITICAL,
      VLF_TRACE|VLF_DEBUG|VLF_EXTAPI|VLF_API|VLF_INFO|VLF_WARNING|VLF_ERROR|VLF_CRITICAL,
      VLF_TRACE|VLF_DEBUG|VLF_EXTAPI|VLF_API|VLF_INFO|VLF_WARNING|VLF_ERROR|VLF_CRITICAL
   };

   if (Flags & VLF_CRITICAL) { // Print the message irrespective of the log level
      #ifdef __ANDROID__
         __android_log_vprint(ANDROID_LOG_ERROR, Header ? Header : "Parasol", Message, Args);
      #else
         #ifdef ESC_OUTPUT
            if (!Header) Header = "";
            #ifdef _WIN32
               fprintf(stderr, "!%s ", Header);
            #else
               fprintf(stderr, "\033[1m%s ", Header);
            #endif
         #else
            if (Header) fprintf(stderr, "%s ", Header);
         #endif

         vfprintf(stderr, Message, Args);

         #ifdef ESC_OUTPUT
            #ifndef _WIN32
               fprintf(stderr, "\033[0m");
            #endif
         #endif

         fprintf(stderr, "\n");
      #endif

      if (Flags & VLF_BRANCH) tlDepth++;
      return;
   }

   LONG level = glLogLevel - tlBaseLine;
   if (level > 9) level = 9;
   else if (level < 0) level = 0;

   if (((log_levels[level] & Flags) != 0) or
       ((glLogLevel > 1) and (Flags & (VLF_WARNING|VLF_ERROR|VLF_CRITICAL))))  {
      CSTRING name, action;
      BYTE msgstate;
      BYTE adjust = 0;

      ThreadLock lock(TL_PRINT, -1);

      if ((Header) and (!*Header)) Header = NULL;

      if (Flags & (VLF_BRANCH|VLF_FUNCTION)) msgstate = MS_FUNCTION;
      else msgstate = MS_MSG;

      //fprintf(stderr, "%.8d. ", winGetCurrentThreadId());
      //fprintf(stderr, "%p ", ctx);

      #if defined(__unix__) and !defined(__ANDROID__)
         BYTE flushdbg;
         if (glLogLevel >= 3) {
            flushdbg = TRUE;
            if (tlPublicLockCount) flushdbg = FALSE;
            if (flushdbg) fcntl(STDERR_FILENO, F_SETFL, glStdErrFlags & (~O_NONBLOCK));
         }
         else flushdbg = FALSE;
      #endif

      #ifdef ESC_OUTPUT // Highlight errors if the log output is crowded
         if ((glLogLevel > 2) and (Flags & (VLF_ERROR|VLF_WARNING))) {
            #ifdef _WIN32
               fprintf(stderr, "!");
               adjust = 1;
            #else
               fprintf(stderr, "\033[1m");
            #endif
         }
      #endif

      // If no header is provided, make one to match the current context

      auto ctx = tlContext;
      auto obj = tlContext->object();
      if (ctx->Action) {
         if (ctx->Action < 0) {
            auto mc = obj->Class;
            if ((mc) and (mc->Methods) and (-ctx->Action < mc->TotalMethods)) {
               action = mc->Methods[-ctx->Action].Name;
            }
            else action = "Method";
         }
         else action = ActionTable[ctx->Action].Name;
      }
      else action = "App";

      if (!Header) {
         Header = action;
         action = NULL;
      }

      #ifdef __ANDROID__
         char msgheader[COLUMN1+1];

         fmsg(Header, msgheader, msgstate, 0);

         if (obj->Class) {
            char msg[180];

            if (obj->Name[0]) name = obj->Name;
            else name = obj->Class->Name;

            if (glLogLevel > 5) {
               if (ctx->Field) snprintf(msg, sizeof(msg), "[%s%s%s:%d:%s] %s", (action) ? action : (STRING)"", (action) ? ":" : "", name, obj->UID, ctx->Field->Name, Message);
               else snprintf(msg, sizeof(msg), "[%s%s%s:%d] %s", (action) ? action : (STRING)"", (action) ? ":" : "", name, obj->UID, Message);
            }
            else {
               if (ctx->Field) snprintf(msg, sizeof(msg), "[%s:%d:%s] %s", name, obj->UID, ctx->Field->Name, Message);
               else snprintf(msg, sizeof(msg), "[%s:%d] %s", name, obj->UID, Message);
            }

            __android_log_vprint((level <= 2) ? ANDROID_LOG_ERROR : ANDROID_LOG_INFO, msgheader, msg, Args);
         }
         else {
            __android_log_vprint((level <= 2) ? ANDROID_LOG_ERROR : ANDROID_LOG_INFO, msgheader, Message, Args);
         }

      #else
         char msgheader[COLUMN1+1];
         if (glLogLevel > 2) {
            fmsg(Header, msgheader, msgstate, adjust); // Print header with indenting
         }
         else {
            size_t len;
            for (len=0; (Header[len]) and (len < sizeof(msgheader)-2); len++) msgheader[len] = Header[len];
            msgheader[len++] = ' ';
            msgheader[len] = 0;
         }

         if (obj->Class) {
            if (obj->Name[0]) name = obj->Name;
            else name = obj->Class->ClassName;

            if (glLogLevel > 5) {
               if (ctx->Field) {
                  fprintf(stderr, "%s[%s%s%s:%d:%s] ", msgheader, (action) ? action : (STRING)"", (action) ? ":" : "", name, obj->UID, ctx->Field->Name);
               }
               else fprintf(stderr, "%s[%s%s%s:%d] ", msgheader, (action) ? action : (STRING)"", (action) ? ":" : "", name, obj->UID);
            }
            else if (ctx->Field) {
               fprintf(stderr, "%s[%s:%d:%s] ", msgheader, name, obj->UID, ctx->Field->Name);
            }
            else fprintf(stderr, "%s[%s:%d] ", msgheader, name, obj->UID);
         }
         else fprintf(stderr, "%s", msgheader);

         vfprintf(stderr, Message, Args);

         #if defined(ESC_OUTPUT) and !defined(_WIN32)
            if ((glLogLevel > 2) and (Flags & (VLF_ERROR|VLF_WARNING))) fprintf(stderr, "\033[0m");
         #endif

         fprintf(stderr, "\n");

         #if defined(__unix__) and !defined(__ANDROID__)
            if (flushdbg) {
               fflush(0); // A fflush() appears to be enough - using fsync() will synchronise to disk, which we don't want by default (slow)
               if (glSync) fsync(STDERR_FILENO);
               fcntl(STDERR_FILENO, F_SETFL, glStdErrFlags);
            }
         #endif
      #endif
   }

   if (Flags & VLF_BRANCH) tlDepth++;
}

/*********************************************************************************************************************

-FUNCTION-
FuncError: Sends basic error messages to the application log.
Status: private

This function outputs a message to the application log.  It uses the codes listed in the system/errors.h file to
display the correct string to the user.  The following example `FuncError(ERR_Write)` would produce input such
as the following: `WriteFile: Error writing data to file.`.

Notice that the Header parameter is not provided in the example.  It is not necessary to supply this parameter in
C/C++ as the function name is automatically entered by the C pre-processor.

-INPUT-
cstr Header: A short string that names the function that is making the call.
error Error: An error code from the "system/errors.h" include file.  Valid error codes and their descriptions can be found in the Parasol SDK manual.

-RESULT-
error: Returns the same code that was specified in the Error parameter.

*********************************************************************************************************************/

ERROR FuncError(CSTRING Header, ERROR Code)
{
   if (tlLogStatus <= 0) return Code;

   // Issue a LogReturn() call if the error code is negative

   BYTE step = FALSE;
   if (Code < 0) {
      Code = -Code;
      step = TRUE;
   }

   if (glLogLevel < 2) {
      if (step) LogReturn();
      return Code;
   }

   if ((tlDepth >= glMaxDepth) or (tlLogStatus <= 0)) {
      if (step) LogReturn();
      return Code;
   }

   if ((Code < glTotalMessages) and (Code > 0)) {
      // Print the header

      auto ctx = tlContext;
      auto obj = tlContext->object();
      if (!Header) {
         if (ctx->Action) {
            if (ctx->Action < 0) {
               auto mc = obj->Class;
               if ((mc) and (mc->Methods) and (-ctx->Action < mc->TotalMethods)) {
                  Header = mc->Methods[-ctx->Action].Name;
               }
               else Header = "Method";
            }
            else Header = ActionTable[ctx->Action].Name;
         }
         else Header = "Function";
      }

      #ifdef __ANDROID__
         if (obj->Class) {
            STRING name;

            if (obj->Name[0]) name = obj->Name;
            else name = obj->Class->Name;

            if (ctx->Field) {
                __android_log_print(ANDROID_LOG_ERROR, Header, "[%s:%d:%s] %s", name, obj->UID, ctx->Field->Name, glMessages[Code]);
            }
            else __android_log_print(ANDROID_LOG_ERROR, Header, "[%s:%d] %s", name, obj->UID, glMessages[Code]);
         }
         else __android_log_print(ANDROID_LOG_ERROR, Header, "%s", glMessages[Code]);
      #else
         char msgheader[COLUMN1+1];
         CSTRING name, histart = "", hiend = "";

         #ifdef ESC_OUTPUT
            if (glLogLevel > 2) {
               #ifdef _WIN32
                  histart = "!";
               #else
                  histart = "\033[1m";
                  hiend = "\033[0m";
               #endif
            }
         #endif

         fmsg(Header, msgheader, MS_MSG, 2);

         if (obj->Class) {
            if (obj->Name[0]) name = obj->Name;
            else name = obj->Class->ClassName;

            if (ctx->Field) {
               fprintf(stderr, "%s%s[%s:%d:%s] %s%s\n", histart, msgheader, name, obj->UID, ctx->Field->Name, glMessages[Code], hiend);
            }
            else fprintf(stderr, "%s%s[%s:%d] %s%s\n", histart, msgheader, name, obj->UID, glMessages[Code], hiend);
         }
         else fprintf(stderr, "%s%s%s%s\n", histart, msgheader, glMessages[Code], hiend);

         #if defined(__unix__) && !defined(__ANDROID__)
            if (glSync) { fflush(0); fsync(STDERR_FILENO); }
         #endif
      #endif
   }
   else LogF(Header,"Code: %d", Code);

   if (step) LogReturn();
   return Code;
}

/*********************************************************************************************************************

-FUNCTION-
LogReturn: Revert to the previous branch in the application logging tree.
Status: private

Use LogReturn() to reverse any previous log message that created an indented branch.  Consider the following
example that uses a tilde to create a new branch:

<pre>
LogF("~Hello:","World.");
   LogF("Goodbye:","World.");
LogReturn();
LogF("Log:","Back to normal.");
</pre>

This will produce the following output:

<pre>
Hello         World.
 Goodbye      World.
Log           Back to normal.
</pre>

If the code was missing the LogReturn() call, the log would be permanently out of step as follows:

<pre>
Hello         World.
 Goodbye      World.
 Log          Back to normal.
</pre>

In C++ the scope-managed `parasol::Log` class should be used instead.  It automatically debranches the messaging chain
when code goes out of scope.

-END-

*********************************************************************************************************************/

void LogReturn(void)
{
   if (tlLogStatus <= 0) return;
   if ((--tlDepth) < 0) tlDepth = 0;
}

//********************************************************************************************************************

static void fmsg(CSTRING Header, STRING Buffer, BYTE Colon, BYTE Sub) // Buffer must be COLUMN1+1 in size
{
   if (!Header) Header = "";

   WORD pos = 0;
   WORD depth;
   WORD col = COLUMN1;

   if (glLogLevel < 3) depth = 0;
   else if (tlDepth > col) depth = col;
   else {
      depth = tlDepth;
      #ifdef _WIN32
         if (Sub and (depth > 0)) {
            col--;
            depth--; // Make a correction to the depth level if an error mark is printed.
         }
      #endif
   }

   if (glTimeLog) {
      DOUBLE time = PreciseTime() - glTimeLog;
      pos += snprintf(Buffer+pos, col, "%09.5f ", (DOUBLE)time/1000000.0);
   }

   if (glLogLevel >= 3) {
      while ((depth > 0) and (pos < col)) {
         #ifdef __ANDROID__
            Buffer[pos++] = '_';
         #else
            Buffer[pos++] = ' ';
         #endif
         depth--;
      }

      while ((depth < 0) and (pos < col)) { // Print depth warnings if the counter is negative.
         Buffer[pos++] = '-';
         depth++;
      }
   }

   if (pos < col) { // Print as many function letters as possible.
      WORD len;
      for (len=0; (Header[len]) and (pos < col); len++) Buffer[pos++] = Header[len];
      if ((!Colon) and (Header[len-1] != ':') and (Header[len-1] != ')')) Colon = MS_MSG;
      if (Colon IS MS_MSG) {
         if ((Header[len-1] != ':') and (Header[len-1] != ')') and (pos < col)) Buffer[pos++] = ':';
      }
      else if (Colon IS MS_FUNCTION) {
         if ((Header[len-1] != ':') and (Header[len-1] != ')') and (pos < col-1)) {
            Buffer[pos++] = '(';
            Buffer[pos++] = ')';
         }
      }
      if (glLogLevel >= 3) while (pos < col) Buffer[pos++] = ' '; // Add any extra spaces
   }

   Buffer[pos] = 0; // NB: Buffer is col + 1, so there is always room for the null byte.
}
