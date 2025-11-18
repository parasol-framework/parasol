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
6  DETAIL Detailed API messages.  For messages within functions, and entry-points for minor functions.
8  TRACE Extremely detailed API messages suitable for intensive debugging only.
9  Noisy debug messages that will appear frequently, e.g. being used in inner loops.

*********************************************************************************************************************/

#include <stdio.h>

#ifdef _MSC_VER

#else
 #include <unistd.h>
#endif

#include <stdarg.h>
#include <fcntl.h>
#include <array>
#include <format>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <source_location>

#ifdef __ANDROID__
#include <android/log.h>
#endif

#include "defs.h"

static void fmsg(CSTRING, STRING, int8_t, int8_t);

#ifdef __ANDROID__
static const int COLUMN1 = 40;
#else
static const int COLUMN1 = 30;
#endif

enum { MS_NONE, MS_FUNCTION, MS_MSG };
enum { EL_NONE=0, EL_MINOR, EL_MAJOR, EL_MAJORBOLD };

static thread_local int tlBaseLine = 0;

namespace {

struct PreparedLogLine {
   std::array<char, COLUMN1+1> Header{};
   std::string Context;
   std::string Message;
   int Level = 0;
   VLF Flags = VLF::NIL;
   bool Highlight = false;
   bool PrintThread = false;
   int ThreadId = 0;
};

#ifndef __ANDROID__
struct TerminalSink {
   TerminalSink();
   void operator()(const PreparedLogLine &Line) const;

   bool SupportsColour;
};
#else
struct TerminalSink {
   void operator()(const PreparedLogLine &Line) const;
};
#endif

using SinkType = TerminalSink;
static std::array<SinkType, 1> glLogSinks { SinkType{} };

static constexpr std::array<VLF, 10> LOG_LEVELS = {
   VLF::CRITICAL,
   VLF::ERROR|VLF::CRITICAL,
   VLF::WARNING|VLF::ERROR|VLF::CRITICAL,
   VLF::INFO|VLF::WARNING|VLF::ERROR|VLF::CRITICAL,
   VLF::INFO|VLF::WARNING|VLF::ERROR|VLF::CRITICAL,
   VLF::API|VLF::INFO|VLF::WARNING|VLF::ERROR|VLF::CRITICAL,
   VLF::DETAIL|VLF::API|VLF::INFO|VLF::WARNING|VLF::ERROR|VLF::CRITICAL,
   VLF::DETAIL|VLF::API|VLF::INFO|VLF::WARNING|VLF::ERROR|VLF::CRITICAL,
   VLF::TRACE|VLF::DETAIL|VLF::API|VLF::INFO|VLF::WARNING|VLF::ERROR|VLF::CRITICAL,
   VLF::TRACE|VLF::DETAIL|VLF::API|VLF::INFO|VLF::WARNING|VLF::ERROR|VLF::CRITICAL
};

static void dispatch_to_sinks(const PreparedLogLine &Line)
{
   auto sinks = std::span<const SinkType>(glLogSinks);
   for (const auto &sink : sinks) sink(Line);
}

#ifndef __ANDROID__
TerminalSink::TerminalSink()
{
#ifdef _WIN32
   SupportsColour = false;
#else
   SupportsColour = true;
#endif
}

void TerminalSink::operator()(const PreparedLogLine &Line) const
{
   if (Line.PrintThread) fprintf(stderr, "%.4d ", Line.ThreadId);

   if (Line.Highlight) {
      if (SupportsColour) fprintf(stderr, "\033[1m");
      else fputc('!', stderr);
   }

   fprintf(stderr, "%s", Line.Header.data());

   if (!Line.Context.empty()) fprintf(stderr, "%s", Line.Context.c_str());

   fprintf(stderr, "%s", Line.Message.c_str());

   if (Line.Highlight and SupportsColour) fprintf(stderr, "\033[0m");

   fprintf(stderr, "\n");
}
#else
void TerminalSink::operator()(const PreparedLogLine &Line) const
{
   auto tag = Line.Header[0] ? Line.Header.data() : "Parasol";
   auto priority = (Line.Level <= 2) ? ANDROID_LOG_ERROR : ANDROID_LOG_INFO;
   __android_log_print(priority, tag, "%s%s", Line.Context.c_str(), Line.Message.c_str());
}
#endif

static std::string format_legacy_message(CSTRING Message, va_list Args)
{
   if (!Message) return std::string();

   va_list copy;
   va_copy(copy, Args);
   int required = vsnprintf(nullptr, 0, Message, copy);
   va_end(copy);

   if (required <= 0) return std::string();

   std::string buffer;
   buffer.resize(required);
   va_copy(copy, Args);
   vsnprintf(buffer.data(), buffer.size()+1, Message, copy);
   va_end(copy);

   return buffer;
}

static PreparedLogLine prepare_line(pf::LogRecord &Record, int Level, int16_t LogSetting)
{
   PreparedLogLine line;
   line.Flags = Record.Flags;
   line.Level = Level;
   line.Message = std::move(Record.Message);
   line.PrintThread = glLogThreads;
   if (line.PrintThread) line.ThreadId = int(get_thread_id());

   bool highlight = (LogSetting > 2) and ((Record.Flags & (VLF::ERROR|VLF::WARNING)) != VLF::NIL);
   if ((Record.Flags & VLF::CRITICAL) != VLF::NIL) highlight = true;
   line.Highlight = highlight;

   auto &ctx = tlContext.back();
   auto obj = ctx.obj;

   CSTRING action;
   if (ctx.action > AC::NIL) action = ActionTable[int(ctx.action)].Name;
   else if (ctx.action < AC::NIL) {
      if (obj->Class) action = ((extMetaClass *)obj->Class)->Methods[-int(ctx.action)].Name;
      else action = "Method";
   }
   else action = "App";

   auto header = Record.Header;
   if ((header) and (!*header)) header = nullptr;
   if (!header) {
      header = action;
      action = nullptr;
   }
   if (!header) header = "";

   auto msgstate = ((Record.Flags & (VLF::BRANCH|VLF::FUNCTION)) != VLF::NIL) ? MS_FUNCTION : MS_MSG;

#ifndef __ANDROID__
   if (LogSetting > 2) {
      int8_t adjust = 0;
      #ifdef _WIN32
         if (line.Highlight) adjust = 1;
      #endif
      fmsg(header, line.Header.data(), msgstate, adjust);
   }
   else {
      size_t len;
      for (len=0; (header[len]) and (len < line.Header.size()-2); len++) line.Header[len] = header[len];
      line.Header[len++] = ' ';
      line.Header[len] = 0;
   }
#else
   fmsg(header, line.Header.data(), msgstate, 0);
#endif

   if (obj->Class) {
      CSTRING name = obj->Name[0] ? obj->Name : obj->Class->ClassName;
      if (LogSetting > 5) {
         CSTRING action_label = action ? action : (CSTRING)"";
         CSTRING action_sep = action ? ":" : "";
         if (ctx.field) line.Context = std::format("[{}{}{}:{}:{}] ", action_label, action_sep, name, obj->UID, ctx.field->Name);
         else line.Context = std::format("[{}{}{}:{}] ", action_label, action_sep, name, obj->UID);
      }
      else {
         if (ctx.field) line.Context = std::format("[{}:{}:{}] ", name, obj->UID, ctx.field->Name);
         else line.Context = std::format("[{}:{}] ", name, obj->UID);
      }
   }
   else line.Context.clear();

   return line;
}

} // namespace

/*********************************************************************************************************************

-FUNCTION-
AdjustLogLevel: Adjusts the base-line of all log messages.

This function adjusts the detail level of all outgoing log messages.  To illustrate, setting the `Delta`
value to 1 would result in level 5 (API) log messages being bumped to level 6.  If the user's maximum log level
output is 5, no further API messages will be output until the base-line is reduced to normal.

The main purpose of AdjustLogLevel() is to reduce log noise.  For instance, creating a new desktop window will result
in a large number of new log messages.  Raising the base-line by 2 before creating the window would eliminate the
noise if the user has the log level set to 5 (API).  Re-running the program with a log level of 7 or more would
make the messages visible again.

Adjustments to the base-line are accumulative, so small increments of 1 or 2 are encouraged.  To revert logging to the
previous base-line, call this function again with a negation of the previously passed value.

-INPUT-
int Delta: The level of adjustment to make to new log messages.  Zero is no change.  The maximum level is +/- 6.

-RESULT-
int: Returns the absolute base-line value that was active prior to calling this function.

*********************************************************************************************************************/

int AdjustLogLevel(int Delta)
{
   if (glLogLevel.load(std::memory_order_relaxed) >= 9) return tlBaseLine; // Do nothing if trace logging is active.
   int old_level = tlBaseLine;
   if ((Delta >= -6) and (Delta <= 6)) tlBaseLine += Delta;
   return old_level;
}

/*********************************************************************************************************************

-FUNCTION-
VLogF: Sends formatted messages to the standard log.
ExtPrototype: VLF Flags, const char *Header, const char *Message, va_list Args
Status: Internal

This function manages the output of application log messages by sending them through a log filter, which must be
enabled by the user.  If no logging is enabled or if the filter is not passed, the function does nothing.

Log message formatting follows the same guidelines as the `printf()` function.

The following example will print the default width of a @Display object to the log.

<pre>
if (!NewObject(CLASSID::DISPLAY, &display)) {
   if (!display->init(display)) {
      VLogF(VLF::API, "Demo","The width of the display is: %d", display-&gt;Width);
   }
   FreeResource(display);
}
</pre>

-INPUT-
int(VLF) Flags: Optional flags
cstr Header: A short name for the first column.  Typically function names are placed here, so that the origin of the message is obvious.
cstr Message: A formatted message to print.
va_list Args: A `va_list` corresponding to the arguments referenced in `Message`.
-END-

*********************************************************************************************************************/

void VLogF(VLF Flags, CSTRING Header, CSTRING Message, va_list Args)
{
   if (pf::detail::ShouldSkipLog(Flags)) return;

   pf::LogRecord record;
   record.Flags = Flags;
   record.Header = Header;
   record.Template = Message ? std::string_view(Message) : std::string_view();
   record.Message = format_legacy_message(Message ? Message : "", Args);
   record.Origin = std::source_location::current();

   pf::detail::SubmitLogRecord(std::move(record));
}

/*********************************************************************************************************************

-FUNCTION-
FuncError: Sends basic error messages to the application log.
Status: Internal

This function outputs a message to the application log.  It uses the codes listed in the system/errors.h file to
display the correct string to the user.  The following example `FuncError(ERR::Write)` would produce input such
as the following: `WriteFile: Error writing data to file.`.

Notice that the Header parameter is not provided in the example.  It is not necessary to supply this parameter in
C/C++ as the function name is automatically entered by the C pre-processor.

-INPUT-
cstr Header: A short string that names the function that is making the call.
error Error: An error code from the `system/errors.h` include file.  Valid error codes and their descriptions can be found in the Parasol Wiki.

-RESULT-
error: Returns the same code that was specified in the `Error` parameter.

*********************************************************************************************************************/

ERR FuncError(CSTRING Header, ERR Code)
{
   if (tlLogStatus <= 0) return Code;
   if (glLogLevel.load(std::memory_order_relaxed) < 2) return Code;
   if ((tlDepth >= glMaxDepth) or (tlLogStatus <= 0)) return Code;

   auto &ctx = tlContext.back();
   auto obj = ctx.obj;
   if (!Header) {
      if (ctx.action > AC::NIL) Header = ActionTable[int(ctx.action)].Name;
      else if (ctx.action < AC::NIL) {
         if (obj->Class) Header = ((extMetaClass *)obj->Class)->Methods[-int(ctx.action)].Name;
         else Header = "Method";
      }
      else Header = "Function";
   }

   #ifdef __ANDROID__
      if (obj->Class) {
         STRING name = obj->Name[0] ? obj->Name : obj->Class->Name;

         if (ctx.field) {
             __android_log_print(ANDROID_LOG_ERROR, Header, "[%s:%d:%s] %s", name, obj->UID, ctx.field->Name, glMessages[Code]);
         }
         else __android_log_print(ANDROID_LOG_ERROR, Header, "[%s:%d] %s", name, obj->UID, glMessages[Code]);
      }
      else __android_log_print(ANDROID_LOG_ERROR, Header, "%s", glMessages[Code]);
   #else
      char msgheader[COLUMN1+1];
      CSTRING histart = "", hiend = "";

      if (glLogLevel.load(std::memory_order_relaxed) > 2) {
         #ifdef _WIN32
            histart = "!";
         #else
            histart = "\033[1m";
            hiend = "\033[0m";
         #endif
      }

      fmsg(Header, msgheader, MS_MSG, 2);

      if (obj->Class) {
         CSTRING name = obj->Name[0] ? obj->Name : obj->Class->ClassName;

         if (ctx.field) {
            fprintf(stderr, "%s%s[%s:%d:%s] %s%s\n", histart, msgheader, name, obj->UID, ctx.field->Name, glMessages[int(Code)], hiend);
         }
         else fprintf(stderr, "%s%s[%s:%d] %s%s\n", histart, msgheader, name, obj->UID, glMessages[int(Code)], hiend);
      }
      else fprintf(stderr, "%s%s%s%s\n", histart, msgheader, glMessages[int(Code)], hiend);

      #if defined(__unix__) && !defined(__ANDROID__)
         if (glSync) { fflush(0); fsync(STDERR_FILENO); }
      #endif
   #endif

   return Code;
}

/*********************************************************************************************************************

-FUNCTION-
LogReturn: Revert to the previous branch in the application logging tree.
Status: Internal

Use LogReturn() to reverse any previous log message that created an indented branch.  This function is considered
internal, and clients must use the scope-managed `pf::Log` class for branched log output.

-END-

*********************************************************************************************************************/

void LogReturn(void)
{
   if (tlLogStatus <= 0) return;
   if ((--tlDepth) < 0) tlDepth = 0;
}

namespace pf::detail {

bool ShouldSkipLog(VLF Flags)
{
   if (tlLogStatus <= 0) {
      if ((Flags & VLF::BRANCH) != VLF::NIL) tlDepth++;
      return true;
   }
   return false;
}

void SubmitLogRecord(LogRecord &&Record)
{
   auto log_setting = glLogLevel.load(std::memory_order_relaxed);
   auto flags = Record.Flags;

   auto dispatch_record = [&](int level) {
      std::lock_guard lock(glmPrint);
      auto prepared = prepare_line(Record, level, log_setting);
      dispatch_to_sinks(prepared);
   };

   if ((flags & VLF::CRITICAL) != VLF::NIL) {
      dispatch_record(0);
      if ((flags & VLF::BRANCH) != VLF::NIL) tlDepth++;
      return;
   }

   int level = log_setting - tlBaseLine;
   if (level > 9) level = 9;
   else if (level < 0) level = 0;

   bool should_log = ((LOG_LEVELS[level] & flags) != VLF::NIL);
   if ((!should_log) and (log_setting > 1) and ((flags & (VLF::WARNING|VLF::ERROR|VLF::CRITICAL)) != VLF::NIL)) should_log = true;

   if (should_log) {
      #if defined(__unix__) and !defined(__ANDROID__)
         bool flushdbg = false;
         if (log_setting >= 3) {
            flushdbg = true;
            if (tlPublicLockCount) flushdbg = false;
            if (flushdbg) fcntl(STDERR_FILENO, F_SETFL, glStdErrFlags & (~O_NONBLOCK));
         }
      #endif

      dispatch_record(level);

      #if defined(__unix__) and !defined(__ANDROID__)
         if (flushdbg) {
            fflush(0);
            if (glSync) fsync(STDERR_FILENO);
            fcntl(STDERR_FILENO, F_SETFL, glStdErrFlags);
         }
      #endif
   }

   if ((flags & VLF::BRANCH) != VLF::NIL) tlDepth++;
}

} // namespace pf::detail

//********************************************************************************************************************

static void fmsg(CSTRING Header, STRING Buffer, int8_t Colon, int8_t Sub) // Buffer must be COLUMN1+1 in size
{
   if (!Header) Header = "";

   int16_t pos = 0;
   int16_t depth;
   int16_t col = COLUMN1;

   if (glLogLevel.load(std::memory_order_relaxed) < 3) depth = 0;
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
      double time = PreciseTime() - glTimeLog;
      pos += snprintf(Buffer+pos, col, "%09.5f ", time/1000000.0);
   }

   if (glLogLevel.load(std::memory_order_relaxed) >= 3) {
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
      int16_t len;
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
      if (glLogLevel.load(std::memory_order_relaxed) >= 3) while (pos < col) Buffer[pos++] = ' '; // Add any extra spaces
   }

   Buffer[pos] = 0; // NB: Buffer is col + 1, so there is always room for the null byte.
}
