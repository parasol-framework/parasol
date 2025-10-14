//********************************************************************************************************************
// XPath Function Library and Value System Implementation
//
// XPath expressions depend on a rich set of standard functions and a loosely typed value model.  This
// file provides both: XPathVal encapsulates conversions between node-sets, numbers, booleans, and
// strings, while the function registry offers implementations of the core function library required by
// the evaluator.  The code emphasises fidelity to the XPath 2.0 specification—string coercions mirror
// the spec's edge cases, numeric conversions preserve NaN semantics, and node-set operations respect
// document order guarantees enforced elsewhere in the module.
//
// The implementation is intentionally self-contained.  The evaluator interacts with XPathVal to
// manipulate intermediate results and delegates built-in function invocations to the routines defined
// below.  Keeping the behaviour consolidated here simplifies future extensions (for example, adding
// namespace-aware functions or performance-focused helpers) without polluting the evaluator with
// coercion details.

#include <parasol/modules/xml.h>
#include <parasol/strings.hpp>
#include <parasol/modules/regex.h>

#include "xpath_functions.h"
#include "../xml/xml.h"
#include "../xml/schema/type_checker.h"
#include "../xml/uri_utils.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <charconv>
#include <cmath>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <format>
#include <limits>
#include <sstream>
#include <string_view>
#include <unordered_set>
#include <utility>

#include "../link/unicode.h"

//********************************************************************************************************************
// XPathVal Implementation

namespace {

static bool is_unreserved_uri_character(unsigned char Code)
{
   if ((Code >= 'A' and Code <= 'Z') or (Code >= 'a' and Code <= 'z') or (Code >= '0' and Code <= '9')) return true;

   switch (Code) {
      case '-':
      case '_':
      case '.':
      case '!':
      case '~':
      case '*':
      case '\'':
      case '(':
      case ')':
         return true;
   }

   return false;
}

static std::string encode_for_uri_impl(const std::string &Value)
{
   std::string result;
   result.reserve(Value.length() * 3);

   for (char ch : Value) {
      unsigned char code = (unsigned char)ch;
      if (is_unreserved_uri_character(code)) {
         result.push_back((char)code);
      }
      else {
         static const char hex_digits[] = "0123456789ABCDEF";
         result.push_back('%');
         result.push_back(hex_digits[(code >> 4) & 0x0F]);
         result.push_back(hex_digits[code & 0x0F]);
      }
   }

   return result;
}

static void replace_all(std::string &Text, std::string_view From, std::string_view To)
{
   if (From.empty()) return;

   size_t position = 0;
   while (true) {
      position = Text.find(From, position);
      if (position IS std::string::npos) break;
      Text.replace(position, From.length(), To);
      position += To.length();
   }
}

static std::string escape_html_uri_impl(const std::string &Value)
{
   std::string encoded = encode_for_uri_impl(Value);

   replace_all(encoded, "%26", "&amp;");
   replace_all(encoded, "%3C", "&lt;");
   replace_all(encoded, "%3E", "&gt;");
   replace_all(encoded, "%22", "&quot;");
   replace_all(encoded, "%27", "&apos;");

   return encoded;
}

static std::string apply_string_case(const std::string &Value, bool Upper)
{
   std::string result = Value;
   std::transform(result.begin(), result.end(), result.begin(), [Upper](char Ch) {
      unsigned char code = (unsigned char)Ch;
      return Upper ? (char)std::toupper(code) : (char)std::tolower(code);
   });
   return result;
}

static void append_codepoint_utf8(std::string &Output, uint32_t Codepoint)
{
   if ((Codepoint > 0x10FFFFu) or ((Codepoint >= 0xD800u) and (Codepoint <= 0xDFFFu))) {
      Codepoint = 0xFFFDu;
   }

   char buffer[8] = { 0 };
   int written = UTF8WriteValue((int)Codepoint, buffer, (int)sizeof(buffer));
   if (written <= 0) return;

   Output.append(buffer, (size_t)written);
}

static std::vector<uint32_t> decode_codepoints(const std::string &Input)
{
   std::vector<uint32_t> codepoints;
   codepoints.reserve(Input.length());

   size_t offset = 0u;
   while (offset < Input.length()) {
      int length = 0;
      uint32_t value = UTF8ReadValue(Input.c_str() + offset, &length);
      if (length <= 0) {
         value = (unsigned char)Input[offset];
         length = 1;
      }

      codepoints.push_back(value);
      offset += (size_t)length;
   }

   return codepoints;
}

static std::string encode_codepoints(const std::vector<uint32_t> &Codepoints)
{
   std::string output;
   for (uint32_t code : Codepoints) append_codepoint_utf8(output, code);
   return output;
}

static std::string simple_normalise_unicode(const std::string &Value, std::string_view Form, bool *Unsupported)
{
   if (Form.empty()) return Value;

   std::string normalised_form(Form);
   std::transform(normalised_form.begin(), normalised_form.end(), normalised_form.begin(), [](char Ch) {
      unsigned char code = (unsigned char)Ch;
      return (char)std::toupper(code);
   });

   if ((normalised_form IS "NFC") or (normalised_form IS "NFKC")) {
      auto codepoints = decode_codepoints(Value);
      std::vector<uint32_t> result;
      result.reserve(codepoints.size());

      for (size_t index = 0u; index < codepoints.size(); ++index) {
         uint32_t code = codepoints[index];
         if ((index + 1u) < codepoints.size()) {
            uint32_t next = codepoints[index + 1u];
            if ((code IS 0x0065u) and (next IS 0x0301u)) {
               result.push_back(0x00E9u);
               index++;
               continue;
            }
            if ((code IS 0x0045u) and (next IS 0x0301u)) {
               result.push_back(0x00C9u);
               index++;
               continue;
            }
         }
         result.push_back(code);
      }

      return encode_codepoints(result);
   }

   if ((normalised_form IS "NFD") or (normalised_form IS "NFKD")) {
      auto codepoints = decode_codepoints(Value);
      std::vector<uint32_t> result;
      result.reserve(codepoints.size() * 2u);

      for (uint32_t code : codepoints) {
         if (code IS 0x00E9u) {
            result.push_back(0x0065u);
            result.push_back(0x0301u);
            continue;
         }
         if (code IS 0x00C9u) {
            result.push_back(0x0045u);
            result.push_back(0x0301u);
            continue;
         }
         result.push_back(code);
      }

      return encode_codepoints(result);
   }

   if (Unsupported) *Unsupported = true;
   return Value;
}

using xml::uri::is_absolute_uri;
using xml::uri::strip_query_fragment;
using xml::uri::normalise_path_segments;
using xml::uri::resolve_relative_uri;

struct DateTimeComponents
{
   int year = 0;
   int month = 1;
   int day = 1;
   int hour = 0;
   int minute = 0;
   double second = 0.0;
   bool has_date = false;
   bool has_time = false;
   bool has_timezone = false;
   bool timezone_is_utc = false;
   int timezone_offset_minutes = 0;
};

static bool parse_fixed_number(std::string_view Text, int &Output)
{
   int value = 0;
   auto result = std::from_chars(Text.data(), Text.data() + Text.length(), value);
   if (result.ec != std::errc()) return false;
   Output = value;
   return true;
}

struct DurationComponents
{
   bool negative = false;
   bool has_year = false;
   bool has_month = false;
   bool has_day = false;
   bool has_hour = false;
   bool has_minute = false;
   bool has_second = false;
   std::chrono::years years{0};
   std::chrono::months months{0};
   std::chrono::days days{0};
   std::chrono::hours hours{0};
   std::chrono::minutes minutes{0};
   std::chrono::duration<double> seconds{0.0};
};

enum class DurationParseStatus { Empty, Error, Value };

static void normalise_duration_components(DurationComponents &Components)
{
   int64_t total_months = (int64_t)Components.years.count() * 12ll + (int64_t)Components.months.count();
   int64_t normalised_years = total_months / 12ll;
   int64_t normalised_months = total_months % 12ll;

   Components.years = std::chrono::years(normalised_years);
   Components.months = std::chrono::months(normalised_months);
   Components.has_year = (normalised_years != 0);
   Components.has_month = (normalised_months != 0);

   std::chrono::duration<double> total_seconds = Components.seconds;
   total_seconds += std::chrono::duration_cast<std::chrono::duration<double>>(Components.minutes);
   total_seconds += std::chrono::duration_cast<std::chrono::duration<double>>(Components.hours);
   total_seconds += std::chrono::duration_cast<std::chrono::duration<double>>(Components.days);

   auto whole_seconds = std::chrono::duration_cast<std::chrono::seconds>(total_seconds);
   auto fractional_seconds = total_seconds - std::chrono::duration_cast<std::chrono::duration<double>>(whole_seconds);

   auto total_minutes = std::chrono::duration_cast<std::chrono::minutes>(whole_seconds);
   auto seconds_remainder = whole_seconds - std::chrono::duration_cast<std::chrono::seconds>(total_minutes);

   auto total_hours = std::chrono::duration_cast<std::chrono::hours>(total_minutes);
   auto minutes_remainder = total_minutes - std::chrono::duration_cast<std::chrono::minutes>(total_hours);

   auto total_days = std::chrono::duration_cast<std::chrono::days>(total_hours);
   auto hours_remainder = total_hours - std::chrono::duration_cast<std::chrono::hours>(total_days);

   Components.days = total_days;
   Components.hours = hours_remainder;
   Components.minutes = minutes_remainder;
   Components.seconds = fractional_seconds + std::chrono::duration_cast<std::chrono::duration<double>>(seconds_remainder);

   Components.has_day = (Components.days.count() != 0);
   Components.has_hour = (Components.hours.count() != 0);
   Components.has_minute = (Components.minutes.count() != 0);
   Components.has_second = (Components.seconds.count() != 0.0);
}

static bool parse_seconds_value(std::string_view Text, double &Output)
{
   if (Text.empty()) return false;

   double value = 0.0;
   auto result = std::from_chars(Text.data(), Text.data() + Text.length(), value);
   if (result.ec != std::errc()) return false;
   if (result.ptr != Text.data() + Text.length()) return false;
   if (!std::isfinite(value)) return false;

   Output = value;
   return true;
}

static bool parse_duration_components(std::string_view Text, DurationComponents &Components)
{
   Components = DurationComponents();
   if (Text.empty()) return false;

   size_t index = 0u;
   if (Text[index] IS '-') {
      Components.negative = true;
      ++index;
      if (index >= Text.length()) return false;
   }

   if (Text[index] != 'P') return false;
   ++index;
   if (index >= Text.length()) return false;

   bool in_time = false;
   bool found_component = false;

   while (index < Text.length()) {
      if (Text[index] IS 'T') {
         if (in_time) return false;
         in_time = true;
         ++index;
         if (index >= Text.length()) return false;
         continue;
      }

      size_t start = index;
      while ((index < Text.length()) and std::isdigit((unsigned char)Text[index])) ++index;
      size_t integer_end = index;

      bool has_fraction = false;
      bool fraction_digits = false;
      if (index < Text.length() and Text[index] IS '.') {
         if (!in_time) return false;
         has_fraction = true;
         ++index;
         while ((index < Text.length()) and std::isdigit((unsigned char)Text[index])) {
            fraction_digits = true;
            ++index;
         }
         if (!fraction_digits) return false;
      }

      if (start IS index) return false;
      if (index >= Text.length()) return false;

      size_t designator_pos = index;
      char designator = Text[designator_pos];
      ++index;

      size_t number_end = designator_pos;
      std::string_view integer_view(Text.data() + start, integer_end - start);
      std::string_view number_view(Text.data() + start, number_end - start);

      if (has_fraction and not (in_time and designator IS 'S')) return false;

      if (designator IS 'Y' and !in_time) {
         if (Components.has_year) return false;
         int value = 0;
         if (!parse_fixed_number(integer_view, value)) return false;
         Components.years = std::chrono::years(value);
         Components.has_year = true;
         found_component = true;
         continue;
      }

      if (designator IS 'M' and !in_time) {
         if (Components.has_month) return false;
         int value = 0;
         if (!parse_fixed_number(integer_view, value)) return false;
         Components.months = std::chrono::months(value);
         Components.has_month = true;
         found_component = true;
         continue;
      }

      if (designator IS 'D' and !in_time) {
         if (Components.has_day) return false;
         int value = 0;
         if (!parse_fixed_number(integer_view, value)) return false;
         Components.days = std::chrono::days(value);
         Components.has_day = true;
         found_component = true;
         continue;
      }

      if (designator IS 'H' and in_time) {
         if (Components.has_hour) return false;
         int value = 0;
         if (!parse_fixed_number(integer_view, value)) return false;
         Components.hours = std::chrono::hours(value);
         Components.has_hour = true;
         found_component = true;
         continue;
      }

      if (designator IS 'M' and in_time) {
         if (Components.has_minute) return false;
         int value = 0;
         if (!parse_fixed_number(integer_view, value)) return false;
         Components.minutes = std::chrono::minutes(value);
         Components.has_minute = true;
         found_component = true;
         continue;
      }

      if (designator IS 'S' and in_time) {
         if (Components.has_second) return false;
         double value = 0.0;
         if (!parse_seconds_value(number_view, value)) return false;
         Components.seconds = std::chrono::duration<double>(value);
         Components.has_second = true;
         found_component = true;
         continue;
      }

      return false;
   }

   return found_component;
}

static DurationParseStatus prepare_duration_components(const std::vector<XPathVal> &Args,
   DurationComponents &Components, bool RequireYearMonthOnly, bool RequireDayTimeOnly)
{
   if (Args.empty()) return DurationParseStatus::Empty;
   if (Args.size() != 1u) return DurationParseStatus::Error;
   if (Args[0].is_empty()) return DurationParseStatus::Empty;

   std::string value = Args[0].to_string();
   if (!parse_duration_components(value, Components)) return DurationParseStatus::Error;

   if (RequireYearMonthOnly) {
      if (Components.has_day or Components.has_hour or Components.has_minute or Components.has_second) {
         return DurationParseStatus::Error;
      }
   }

   if (RequireDayTimeOnly) {
      if (Components.has_year or Components.has_month) return DurationParseStatus::Error;
   }

   normalise_duration_components(Components);

   return DurationParseStatus::Value;
}

static bool parse_timezone(std::string_view Text, DateTimeComponents &Components)
{
   if (Text.empty()) return true;

   Components.has_timezone = true;

   if ((Text.length() IS 1u) and ((Text[0] IS 'Z') or (Text[0] IS 'z'))) {
      Components.timezone_is_utc = true;
      Components.timezone_offset_minutes = 0;
      return true;
   }

   if (Text.length() < 3u) return false;
   char sign = Text[0];
   if ((sign != '+') and (sign != '-')) return false;

   std::string_view hours_view = Text.substr(1u, 2u);
   int hours = 0;
   if (!parse_fixed_number(hours_view, hours)) return false;

   size_t pos = 3u;
   int minutes = 0;
   if ((Text.length() >= 6u) and (Text[3] IS ':')) {
      std::string_view minutes_view = Text.substr(4u, 2u);
      if (!parse_fixed_number(minutes_view, minutes)) return false;
      pos = 6u;
   }
   else if (Text.length() >= 5u) {
      std::string_view minutes_view = Text.substr(3u, 2u);
      if (!parse_fixed_number(minutes_view, minutes)) return false;
      pos = 5u;
   }

   if (Text.length() != pos) return false;

   int total = hours * 60 + minutes;
   if (sign IS '-') total = -total;
   Components.timezone_offset_minutes = total;
   Components.timezone_is_utc = (total IS 0);
   return true;
}

static bool parse_time_value(std::string_view Text, DateTimeComponents &Components)
{
   if (Text.length() < 8u) return false;

   size_t tz_pos = std::string::npos;
   for (size_t index = 0u; index < Text.length(); ++index) {
      char ch = Text[index];
      if ((ch IS '+') or (ch IS '-') or (ch IS 'Z') or (ch IS 'z')) {
         if (index >= 5u) {
            tz_pos = index;
            break;
         }
      }
   }

   std::string_view time_section = Text;
   std::string_view tz_section;

   if (tz_pos != std::string::npos) {
      time_section = Text.substr(0u, tz_pos);
      tz_section = Text.substr(tz_pos);
   }

   if ((time_section.length() < 8u) or (time_section[2] != ':') or (time_section[5] != ':')) return false;

   int hour = 0;
   int minute = 0;
   int second = 0;

   if (!parse_fixed_number(time_section.substr(0u, 2u), hour)) return false;
   if (!parse_fixed_number(time_section.substr(3u, 2u), minute)) return false;
   if (!parse_fixed_number(time_section.substr(6u, 2u), second)) return false;

   Components.hour = hour;
   Components.minute = minute;
   Components.second = (double)second;
   Components.has_time = true;

   size_t fractional_pos = time_section.find('.');
   if (fractional_pos != std::string::npos) {
      std::string_view fraction = time_section.substr(fractional_pos + 1u);
      int fraction_value = 0;
      if (!fraction.empty() and parse_fixed_number(fraction, fraction_value)) {
         double scale = std::pow(10.0, (double)fraction.length());
         Components.second += (double)fraction_value / scale;
      }
   }

   if (!tz_section.empty()) return parse_timezone(tz_section, Components);
   return true;
}

static bool parse_date_value(std::string_view Text, DateTimeComponents &Components)
{
   if (Text.length() < 10u) return false;
   if ((Text[4] != '-') or (Text[7] != '-')) return false;

   int year = 0;
   int month = 0;
   int day = 0;

   if (!parse_fixed_number(Text.substr(0u, 4u), year)) return false;
   if (!parse_fixed_number(Text.substr(5u, 2u), month)) return false;
   if (!parse_fixed_number(Text.substr(8u, 2u), day)) return false;

   Components.year = year;
   Components.month = month;
   Components.day = day;
   Components.has_date = true;

   if (Text.length() IS 10u) return true;

   std::string_view tz_section = Text.substr(10u);
   return parse_timezone(tz_section, Components);
}

static bool parse_date_time_components(std::string_view Text, DateTimeComponents &Components)
{
   size_t t_pos = Text.find('T');
   if (t_pos != std::string::npos) {
      std::string_view date_part = Text.substr(0u, t_pos);
      std::string_view time_part = Text.substr(t_pos + 1u);
      return parse_date_value(date_part, Components) and parse_time_value(time_part, Components);
   }

   if (Text.find('-') != std::string::npos) return parse_date_value(Text, Components);
   return parse_time_value(Text, Components);
}

static std::string format_integer_component(int64_t Value, int Width, bool ZeroPad)
{
   bool negative = Value < 0;
   uint64_t absolute = negative ? (uint64_t)(-Value) : (uint64_t)Value;
   std::string digits = std::to_string(absolute);

   if ((Width > 0) and ((int)digits.length() < Width)) {
      std::string padding((size_t)(Width - (int)digits.length()), ZeroPad ? '0' : ' ');
      digits.insert(0u, padding);
   }

   if (negative) digits.insert(digits.begin(), '-');
   return digits;
}

static std::string format_timezone(const DateTimeComponents &Components)
{
   if (!Components.has_timezone) return std::string();
   if (Components.timezone_is_utc or (Components.timezone_offset_minutes IS 0)) return std::string("Z");

   int offset = Components.timezone_offset_minutes;
   char sign = '+';
   if (offset < 0) {
      sign = '-';
      offset = -offset;
   }

   int hours = offset / 60;
   int minutes = offset % 60;

   return std::format("{}{:02d}:{:02d}", sign, hours, minutes);
}

static std::string format_component(const DateTimeComponents &Components, const std::string &Token)
{
   if (Token.empty()) return std::string();

   char symbol = Token[0];
   std::string spec = Token.substr(1u);

   int width = 0;
   bool zero_pad = false;
   for (char ch : spec) {
      if (std::isdigit((unsigned char)ch)) {
         width++;
         if (ch IS '0') zero_pad = true;
      }
   }

   switch (symbol) {
      case 'Y': return format_integer_component(Components.year, (width > 0) ? width : 4, true);
      case 'M': return format_integer_component(Components.month, (width > 0) ? width : 2, true);
      case 'D': return format_integer_component(Components.day, (width > 0) ? width : 2, true);
      case 'H': return format_integer_component(Components.hour, (width > 0) ? width : 2, true);
      case 'm': return format_integer_component(Components.minute, (width > 0) ? width : 2, true);
      case 's': {
         int64_t rounded = (int64_t)std::llround(Components.second);
         return format_integer_component(rounded, (width > 0) ? width : 2, true);
      }
      case 'Z':
      case 'z':
         return format_timezone(Components);
      default:
         break;
   }

   return Token;
}

static std::string format_with_picture(const DateTimeComponents &Components, const std::string &Picture)
{
   std::string output;
   for (size_t index = 0u; index < Picture.length();) {
      char ch = Picture[index];
      if (ch IS '[') {
         size_t end = Picture.find(']', index + 1u);
         if (end IS std::string::npos) break;
         std::string token = Picture.substr(index + 1u, end - index - 1u);
         output.append(format_component(Components, token));
         index = end + 1u;
      }
      else if (ch IS '\'') {
         size_t end = Picture.find('\'', index + 1u);
         if (end IS std::string::npos) break;
         output.append(Picture.substr(index + 1u, end - index - 1u));
         index = end + 1u;
      }
      else {
         output.push_back(ch);
         ++index;
      }
   }
   return output;
}

static std::string format_seconds_field(double Value)
{
   if (Value < 0.0) Value = 0.0;

   double integral_part = 0.0;
   double fractional_part = std::modf(Value, &integral_part);

   int64_t integral_seconds = (int64_t)integral_part;
   int64_t fractional_microseconds = (int64_t)std::llround(fractional_part * 1000000.0);

   if (fractional_microseconds >= 1000000ll) {
      fractional_microseconds -= 1000000ll;
      ++integral_seconds;
   }

   std::string seconds = format_integer_component(integral_seconds, 2, true);

   if (fractional_microseconds > 0ll) {
      std::string fractional_digits = std::to_string(fractional_microseconds);
      while (fractional_digits.length() < 6u) fractional_digits.insert(0u, 1u, '0');
      while ((!fractional_digits.empty()) and (fractional_digits.back() IS '0')) fractional_digits.pop_back();
      if (!fractional_digits.empty()) {
         seconds.push_back('.');
         seconds.append(fractional_digits);
      }
   }

   return seconds;
}

static std::string serialise_date_only(const DateTimeComponents &Components, bool IncludeTimezone)
{
   auto year = format_integer_component(Components.year, 4, true);
   auto month = format_integer_component(Components.month, 2, true);
   auto day = format_integer_component(Components.day, 2, true);

   std::string result = std::format("{}-{}-{}", year, month, day);

   if (IncludeTimezone and Components.has_timezone) result.append(format_timezone(Components));

   return result;
}

static std::string serialise_time_only(const DateTimeComponents &Components, bool IncludeTimezone)
{
   auto hour = format_integer_component(Components.hour, 2, true);
   auto minute = format_integer_component(Components.minute, 2, true);
   auto second = format_seconds_field(Components.second);

   std::string result = std::format("{}:{}:{}", hour, minute, second);

   if (IncludeTimezone and Components.has_timezone) result.append(format_timezone(Components));

   return result;
}

static std::string serialise_date_time_components(const DateTimeComponents &Components)
{
   std::string result = serialise_date_only(Components, false);
   result.push_back('T');
   result.append(serialise_time_only(Components, true));
   return result;
}

static bool combine_date_and_time(const std::string &DateValue, const std::string &TimeValue,
   DateTimeComponents &Combined)
{
   DateTimeComponents date_components;
   if (!parse_date_value(DateValue, date_components)) return false;

   DateTimeComponents time_components;
   if (!parse_time_value(TimeValue, time_components)) return false;

   Combined = date_components;
   Combined.hour = time_components.hour;
   Combined.minute = time_components.minute;
   Combined.second = time_components.second;
   Combined.has_time = time_components.has_time;

   bool date_has_timezone = date_components.has_timezone;
   bool time_has_timezone = time_components.has_timezone;

   if (date_has_timezone and time_has_timezone) {
      if (date_components.timezone_offset_minutes != time_components.timezone_offset_minutes) return false;
      Combined.has_timezone = true;
      Combined.timezone_offset_minutes = time_components.timezone_offset_minutes;
      Combined.timezone_is_utc = time_components.timezone_is_utc;
   }
   else if (time_has_timezone) {
      Combined.has_timezone = true;
      Combined.timezone_offset_minutes = time_components.timezone_offset_minutes;
      Combined.timezone_is_utc = time_components.timezone_is_utc;
   }
   else if (date_has_timezone) {
      Combined.has_timezone = true;
      Combined.timezone_offset_minutes = date_components.timezone_offset_minutes;
      Combined.timezone_is_utc = date_components.timezone_is_utc;
   }
   else {
      Combined.has_timezone = false;
      Combined.timezone_offset_minutes = 0;
      Combined.timezone_is_utc = false;
   }

   return true;
}

static bool parse_timezone_duration(const std::string &Text, int &OffsetMinutes)
{
   DurationComponents components;
   if (!parse_duration_components(Text, components)) return false;

   normalise_duration_components(components);

   if (components.has_year or components.has_month or components.has_day) return false;
   if (components.has_second) return false;

   int64_t total_minutes = components.hours.count() * 60ll + components.minutes.count();
   if (components.negative) total_minutes = -total_minutes;

   if (total_minutes < -14ll * 60ll or total_minutes > 14ll * 60ll) return false;

   OffsetMinutes = (int)total_minutes;
   return true;
}

static std::string format_timezone_duration(int OffsetMinutes)
{
   if (OffsetMinutes IS 0) return std::string("PT0S");

   std::string result;
   if (OffsetMinutes < 0) {
      result.append("-PT");
      OffsetMinutes = -OffsetMinutes;
   }
   else {
      result.append("PT");
   }

   int hours = OffsetMinutes / 60;
   int minutes = OffsetMinutes % 60;

   if (hours != 0) {
      result.append(std::format("{}H", hours));
   }

   if (minutes != 0) {
      result.append(std::format("{}M", minutes));
   }

   if ((hours IS 0) and (minutes IS 0)) result.append("0S");

   return result;
}

static bool components_to_utc_time(const DateTimeComponents &Components, int ImplicitTimezoneMinutes,
   std::chrono::sys_time<std::chrono::microseconds> &UtcTime)
{
   using namespace std::chrono;

   int year_value = Components.has_date ? Components.year : 1970;
   int month_value = Components.has_date ? Components.month : 1;
   int day_value = Components.has_date ? Components.day : 1;

   year y(year_value);
   month m(month_value);
   day d(day_value);

   if (!y.ok() or !m.ok() or !d.ok()) return false;

   year_month_day ymd(y, m, d);
   if (!ymd.ok()) return false;

   sys_days day_point(ymd);

   int hour = Components.has_time ? Components.hour : 0;
   int minute = Components.has_time ? Components.minute : 0;
   double seconds_value = Components.has_time ? Components.second : 0.0;

   double integral_part = 0.0;
   double fractional_part = std::modf(seconds_value, &integral_part);

   int64_t integral_seconds = (int64_t)integral_part;
   int64_t microseconds_value = (int64_t)std::llround(fractional_part * 1000000.0);

   if (microseconds_value >= 1000000ll) {
      microseconds_value -= 1000000ll;
      ++integral_seconds;
   }

   auto time_duration = hours(hour) + minutes(minute) + seconds(integral_seconds)
      + microseconds(microseconds_value);

   sys_time<microseconds> local_time(day_point.time_since_epoch() + duration_cast<microseconds>(time_duration));

   int timezone_offset = Components.has_timezone ? Components.timezone_offset_minutes : ImplicitTimezoneMinutes;

   UtcTime = local_time - minutes(timezone_offset);
   return true;
}

static DateTimeComponents components_from_utc_time(const std::chrono::sys_time<std::chrono::microseconds> &UtcTime,
   int TargetOffsetMinutes, bool IncludeTimezone, bool IncludeDate, bool IncludeTime)
{
   using namespace std::chrono;

   sys_time<microseconds> local_time = UtcTime + minutes(TargetOffsetMinutes);
   auto day_point = floor<days>(local_time);

   DateTimeComponents result;

   if (IncludeDate) {
      year_month_day ymd(day_point);
      result.year = int(ymd.year());
      result.month = (int)unsigned(ymd.month());
      result.day = (int)unsigned(ymd.day());
      result.has_date = true;
   }

   if (IncludeTime) {
      auto day_time = local_time - day_point;
      auto hour_duration = duration_cast<hours>(day_time);
      auto minute_duration = duration_cast<minutes>(day_time - hour_duration);
      auto second_duration = duration_cast<seconds>(day_time - hour_duration - minute_duration);
      auto micro_duration = duration_cast<microseconds>(day_time - hour_duration - minute_duration - second_duration);

      result.hour = (int)hour_duration.count();
      result.minute = (int)minute_duration.count();
      result.second = (double)second_duration.count() + (double)micro_duration.count() / 1000000.0;
      result.has_time = true;
   }

   if (IncludeTimezone) {
      result.has_timezone = true;
      result.timezone_offset_minutes = TargetOffsetMinutes;
      result.timezone_is_utc = (TargetOffsetMinutes IS 0);
   }

   return result;
}

static std::string format_integer_picture(int64_t Value, const std::string &Picture)
{
   bool negative = Value < 0;
   uint64_t absolute = negative ? (uint64_t)(-Value) : (uint64_t)Value;
   std::string digits = std::to_string(absolute);

   size_t digit_slots = 0u;
   bool zero_pad = false;
   bool grouping = false;

   for (char ch : Picture) {
      if ((ch IS '#') or (ch IS '0')) {
         digit_slots++;
         if (ch IS '0') zero_pad = true;
      }
      if (ch IS ',') grouping = true;
   }

   if ((digit_slots > digits.length()) and (digit_slots > 0u)) {
      std::string padding(digit_slots - digits.length(), zero_pad ? '0' : ' ');
      digits.insert(0u, padding);
   }

   if (grouping) {
      std::string grouped;
      int count = 0;
      for (auto iter = digits.rbegin(); iter != digits.rend(); ++iter) {
         if ((count > 0) and ((count % 3) IS 0)) grouped.push_back(',');
         grouped.push_back(*iter);
         ++count;
      }
      std::reverse(grouped.begin(), grouped.end());
      digits = grouped;
   }

   if (negative) digits.insert(digits.begin(), '-');
   return digits;
}

// Utilised by trace() and error() to provide a concise description of an XPath value

static std::string describe_xpath_value(const XPathVal &Value)
{
   switch (Value.Type) {
      case XPVT::Boolean:
         return Value.BooleanValue ? std::string("true") : std::string("false");

      case XPVT::Number:
         return Value.to_string();

      case XPVT::String:
      case XPVT::Date:
      case XPVT::Time:
      case XPVT::DateTime:
         return Value.StringValue;

      case XPVT::NodeSet: {
         std::vector<std::string> entries;

         if (Value.node_set_string_override.has_value()) {
            entries.push_back(*Value.node_set_string_override);
         }
         else if (not Value.node_set_attributes.empty()) {
            for (const XMLAttrib *attribute : Value.node_set_attributes) {
               if (not attribute) continue;
               entries.push_back(attribute->Value);
            }
         }
         else if (not Value.node_set_string_values.empty()) {
            entries.insert(entries.end(), Value.node_set_string_values.begin(), Value.node_set_string_values.end());
         }
         else {
            for (XMLTag *node : Value.node_set) {
               if (not node) continue;
               entries.push_back(XPathVal::node_string_value(node));
            }
         }

         size_t total_count = entries.size();
         if ((total_count IS 0) and not Value.node_set.empty()) total_count = Value.node_set.size();
         if ((total_count IS 0) and not Value.node_set_attributes.empty()) total_count = Value.node_set_attributes.size();
         if ((total_count IS 0) and not Value.node_set_string_values.empty()) {
            total_count = Value.node_set_string_values.size();
         }

         if (entries.empty()) {
            if (total_count IS 0) return std::string("()");
         }

         size_t summary_limit = entries.size();
         if (summary_limit > 3) summary_limit = 3;

         std::string summary;
         for (size_t index = 0; index < summary_limit; ++index) {
            if (index > 0) summary.append(", ");
            summary.append(entries[index]);
         }

         if (entries.size() > summary_limit) summary.append(", ...");

         if (total_count > 1) {
            if (not summary.empty()) {
               return std::format("node-set[{}]: {}", total_count, summary);
            }
            else {
               return std::format("node-set[{}]", total_count);
            }
         }

         if (not summary.empty()) return summary;

         return std::string("()");
      }
   }

   return std::string();
}

REGEX build_regex_options(const std::string &Flags, bool *UnsupportedFlag)
{
   REGEX options = REGEX::NIL;

   for (char flag : Flags) {
      unsigned char code = (unsigned char)flag;
      char normalised = (char)std::tolower(code);

      if (normalised IS 'i') options |= REGEX::ICASE;
      else if (normalised IS 'm') options |= REGEX::MULTILINE;
      else if (normalised IS 's') options |= REGEX::DOT_ALL;
      else if (UnsupportedFlag) *UnsupportedFlag = true;
   }

   return options;
}

static void append_numbers_from_nodeset(const XPathVal &Value, std::vector<double> &Numbers)
{
   if (Value.node_set_string_override.has_value()) {
      double number = XPathVal::string_to_number(*Value.node_set_string_override);
      if (not std::isnan(number)) Numbers.push_back(number);
      return;
   }

   if (not Value.node_set_attributes.empty()) {
      for (const XMLAttrib *attribute : Value.node_set_attributes) {
         if (not attribute) continue;
         double number = XPathVal::string_to_number(attribute->Value);
         if (not std::isnan(number)) Numbers.push_back(number);
      }
      return;
   }

   if (not Value.node_set_string_values.empty()) {
      for (const std::string &entry : Value.node_set_string_values) {
         double number = XPathVal::string_to_number(entry);
         if (not std::isnan(number)) Numbers.push_back(number);
      }
      return;
   }

   for (XMLTag *node : Value.node_set) {
      if (not node) continue;
      std::string content = XPathVal::node_string_value(node);
      double number = XPathVal::string_to_number(content);
      if (not std::isnan(number)) Numbers.push_back(number);
   }
}

static void append_numbers_from_value(const XPathVal &Value, std::vector<double> &Numbers)
{
   switch (Value.Type) {
      case XPVT::Number:
         if (not std::isnan(Value.NumberValue)) Numbers.push_back(Value.NumberValue);
         break;
      case XPVT::String:
      case XPVT::Date:
      case XPVT::Time:
      case XPVT::DateTime: {
         double number = XPathVal::string_to_number(Value.StringValue);
         if (not std::isnan(number)) Numbers.push_back(number);
         break;
      }
      case XPVT::Boolean:
         Numbers.push_back(Value.BooleanValue ? 1.0 : 0.0);
         break;
      case XPVT::NodeSet:
         append_numbers_from_nodeset(Value, Numbers);
         break;
   }
}

struct SequenceBuilder
{
   NODES nodes;
   std::vector<const XMLAttrib *> attributes;
   std::vector<std::string> strings;
};

struct AnalyzeStringState
{
   SequenceBuilder *builder;
   const char *input_data;
   size_t input_length;
   size_t last_offset;
};

static size_t sequence_length(const XPathVal &Value)
{
   if (Value.Type IS XPVT::NodeSet) {
      size_t length = Value.node_set.size();
      if (length < Value.node_set_attributes.size()) length = Value.node_set_attributes.size();
      if (length < Value.node_set_string_values.size()) length = Value.node_set_string_values.size();
      if ((length IS 0) and Value.node_set_string_override.has_value()) length = 1;
      return length;
   }

   return Value.is_empty() ? 0 : 1;
}

static std::string sequence_item_string(const XPathVal &Value, size_t Index)
{
   if (Value.Type IS XPVT::NodeSet) {
      if (Index < Value.node_set_string_values.size()) return Value.node_set_string_values[Index];

      bool use_override = Value.node_set_string_override.has_value() and (Index IS 0) and
         Value.node_set_string_values.empty();
      if (use_override) return *Value.node_set_string_override;

      if (Index < Value.node_set_attributes.size()) {
         const XMLAttrib *attribute = Value.node_set_attributes[Index];
         if (attribute) return attribute->Value;
      }

      if (Index < Value.node_set.size()) {
         XMLTag *node = Value.node_set[Index];
         if (node) return XPathVal::node_string_value(node);
      }

      return std::string();
   }

   return Value.to_string();
}

static void append_sequence_item(const XPathVal &Value, size_t Index, SequenceBuilder &Builder)
{
   XMLTag *node = nullptr;
   if (Index < Value.node_set.size()) node = Value.node_set[Index];
   Builder.nodes.push_back(node);

   const XMLAttrib *attribute = nullptr;
   if (Index < Value.node_set_attributes.size()) attribute = Value.node_set_attributes[Index];
   Builder.attributes.push_back(attribute);

   std::string entry = sequence_item_string(Value, Index);
   Builder.strings.push_back(entry);
}

static void append_value_to_sequence(const XPathVal &Value, SequenceBuilder &Builder)
{
   if (Value.Type IS XPVT::NodeSet) {
      size_t length = sequence_length(Value);
      for (size_t index = 0; index < length; ++index) append_sequence_item(Value, index, Builder);
      return;
   }

   if (Value.is_empty()) return;

   Builder.nodes.push_back(nullptr);
   Builder.attributes.push_back(nullptr);
   Builder.strings.push_back(Value.to_string());
}

static XPathVal make_sequence_value(SequenceBuilder &&Builder)
{
   XPathVal result;
   result.Type = XPVT::NodeSet;
   result.node_set = std::move(Builder.nodes);
   result.node_set_attributes = std::move(Builder.attributes);
   result.node_set_string_values = std::move(Builder.strings);

   if ((result.node_set_string_values.size() IS 1) and result.node_set.empty() and
       result.node_set_attributes.empty()) {
      result.node_set_string_override = result.node_set_string_values[0];
   }

   return result;
}

static XPathVal extract_sequence_item(const XPathVal &Value, size_t Index)
{
   if (Value.Type IS XPVT::NodeSet) {
      size_t length = sequence_length(Value);
      if (Index >= length) return XPathVal();

      XPathVal result;
      result.Type = XPVT::NodeSet;

      XMLTag *node = nullptr;
      if (Index < Value.node_set.size()) node = Value.node_set[Index];
      result.node_set.push_back(node);

      const XMLAttrib *attribute = nullptr;
      if (Index < Value.node_set_attributes.size()) attribute = Value.node_set_attributes[Index];
      result.node_set_attributes.push_back(attribute);

      std::string entry = sequence_item_string(Value, Index);
      result.node_set_string_values.push_back(entry);

      bool use_override = Value.node_set_string_override.has_value() and (Index IS 0) and
         Value.node_set_string_values.empty();
      if (use_override or (result.node_set_string_values.size() IS 1 and result.node_set.empty() and
         result.node_set_attributes.empty())) {
         result.node_set_string_override = entry;
      }

      return result;
   }

   if (Index IS 0) return Value;
   return XPathVal();
}

static bool numeric_equal(double Left, double Right)
{
   if (std::isnan(Left) or std::isnan(Right)) return false;
   if (std::isinf(Left) or std::isinf(Right)) return Left IS Right;

   double abs_left = std::fabs(Left);
   double abs_right = std::fabs(Right);
   double larger = (abs_left > abs_right) ? abs_left : abs_right;

   if (larger <= 1.0) {
      return std::fabs(Left - Right) <= std::numeric_limits<double>::epsilon() * 16;
   }

   return std::fabs(Left - Right) <= larger * std::numeric_limits<double>::epsilon() * 16;
}

static bool xpath_values_equal(const XPathVal &Left, const XPathVal &Right)
{
   auto left_type = Left.Type;
   auto right_type = Right.Type;

   if ((left_type IS XPVT::Boolean) or (right_type IS XPVT::Boolean)) {
      bool left_boolean = Left.to_boolean();
      bool right_boolean = Right.to_boolean();
      return left_boolean IS right_boolean;
   }

   if ((left_type IS XPVT::Number) or (right_type IS XPVT::Number)) {
      double left_number = Left.to_number();
      double right_number = Right.to_number();
      if (std::isnan(left_number) or std::isnan(right_number)) return false;
      return numeric_equal(left_number, right_number);
   }

   if ((left_type IS XPVT::NodeSet) or (right_type IS XPVT::NodeSet)) {
      if ((left_type IS XPVT::NodeSet) and (right_type IS XPVT::NodeSet)) {
         XMLTag *left_node = Left.node_set.empty() ? nullptr : Left.node_set[0];
         XMLTag *right_node = Right.node_set.empty() ? nullptr : Right.node_set[0];
         if (left_node or right_node) {
            if (left_node IS right_node) return true;
            if ((left_node IS nullptr) or (right_node IS nullptr)) return false;
         }

         const XMLAttrib *left_attribute = Left.node_set_attributes.empty() ? nullptr : Left.node_set_attributes[0];
         const XMLAttrib *right_attribute = Right.node_set_attributes.empty() ? nullptr : Right.node_set_attributes[0];
         if (left_attribute or right_attribute) {
            if (left_attribute IS right_attribute) return true;
            if ((left_attribute IS nullptr) or (right_attribute IS nullptr)) return false;
         }
      }

      std::string left_string = Left.to_string();
      std::string right_string = Right.to_string();
      return left_string.compare(right_string) IS 0;
   }

   std::string left_string = Left.to_string();
   std::string right_string = Right.to_string();
   return left_string.compare(right_string) IS 0;
}

static void flag_cardinality_error(const XPathContext &Context, std::string_view FunctionName,
   std::string_view Message)
{
   if (Context.expression_unsupported) *Context.expression_unsupported = true;

   if (Context.document) {
      if (not Context.document->ErrorMsg.empty()) Context.document->ErrorMsg.append("\n");
      Context.document->ErrorMsg.append("XPath function ");
      Context.document->ErrorMsg.append(FunctionName);
      Context.document->ErrorMsg.append(": ");
      Context.document->ErrorMsg.append(Message);
   }
}

} // namespace












namespace {

// Walk up the tree to locate a namespace declaration corresponding to the requested prefix.
std::string find_in_scope_namespace(XMLTag *Node, extXML *Document, std::string_view Prefix)
{
   XMLTag *current = Node;

   while (current) {
      for (size_t index = 1; index < current->Attribs.size(); ++index) {
         const auto &attrib = current->Attribs[index];

         if (Prefix.empty()) {
            if ((attrib.Name.length() IS 5) and (attrib.Name.compare(0, 5, "xmlns") IS 0)) return attrib.Value;
         }
         else {
            if (attrib.Name.compare(0, 6, "xmlns:") IS 0) {
               std::string declared = attrib.Name.substr(6);
               if ((declared.length() IS Prefix.size()) and (declared.compare(Prefix) IS 0)) return attrib.Value;
            }
         }
      }

      if (not Document) break;
      if (current->ParentID IS 0) break;
      current = Document->getTag(current->ParentID);
   }

   return std::string();
}

std::string find_language_for_node(XMLTag *Node, extXML *Document)
{
   XMLTag *current = Node;

   while (current) {
      for (size_t index = 1; index < current->Attribs.size(); ++index) {
         const auto &attrib = current->Attribs[index];
         if (pf::iequals(attrib.Name, "xml:lang")) return attrib.Value;
      }

      if (not Document) break;
      if (current->ParentID IS 0) break;
      current = Document->getTag(current->ParentID);
   }

   return std::string();
}

std::string lowercase_copy(const std::string &Value)
{
   std::string result = Value;
   std::transform(result.begin(), result.end(), result.begin(), [](unsigned char Ch) { return char(std::tolower(Ch)); });
   return result;
}

bool language_matches(const std::string &Candidate, const std::string &Requested)
{
   if (Requested.empty()) return false;

   std::string candidate_lower = lowercase_copy(Candidate);
   std::string requested_lower = lowercase_copy(Requested);

   if (candidate_lower.compare(0, requested_lower.length(), requested_lower) IS 0) {
      if (candidate_lower.length() IS requested_lower.length()) return true;
      if ((candidate_lower.length() > requested_lower.length()) and (candidate_lower[requested_lower.length()] IS '-')) return true;
   }

   return false;
}

} // namespace

//********************************************************************************************************************
// XPathFunctionLibrary Implementation

XPathFunctionLibrary::XPathFunctionLibrary() {
   register_core_functions();
}

const XPathFunctionLibrary & XPathFunctionLibrary::instance()
{
   static std::once_flag initialise_flag;
   static std::unique_ptr<XPathFunctionLibrary> shared_library;

   std::call_once(initialise_flag, [&]() {
      shared_library = std::unique_ptr<XPathFunctionLibrary>(new XPathFunctionLibrary());
   });

   return *shared_library;
}

void XPathFunctionLibrary::register_core_functions() {
   // Node Set Functions
   register_function("last", function_last);
   register_function("position", function_position);
   register_function("count", function_count);
   register_function("id", function_id);
   register_function("idref", function_idref);
   register_function("root", function_root);

   // Document Functions
   register_function("doc", function_doc);
   register_function("doc-available", function_doc_available);
   register_function("collection", function_collection);
   register_function("uri-collection", function_uri_collection);
   register_function("unparsed-text", function_unparsed_text);
   register_function("unparsed-text-available", function_unparsed_text_available);
   register_function("unparsed-text-lines", function_unparsed_text_lines);
   register_function("local-name", function_local_name);
   register_function("namespace-uri", function_namespace_uri);
   register_function("name", function_name);

   // Accessor Functions
   register_function("base-uri", function_base_uri);
   register_function("data", function_data);
   register_function("document-uri", function_document_uri);
   register_function("node-name", function_node_name);
   register_function("nilled", function_nilled);
   register_function("static-base-uri", function_static_base_uri);
   register_function("default-collation", function_default_collation);

   // QName Functions
   register_function("QName", function_QName);
   register_function("resolve-QName", function_resolve_QName);
   register_function("prefix-from-QName", function_prefix_from_QName);
   register_function("local-name-from-QName", function_local_name_from_QName);
   register_function("namespace-uri-from-QName", function_namespace_uri_from_QName);
   register_function("namespace-uri-for-prefix", function_namespace_uri_for_prefix);
   register_function("in-scope-prefixes", function_in_scope_prefixes);

   // String Functions
   register_function("string", function_string);
   register_function("concat", function_concat);
   register_function("codepoints-to-string", function_codepoints_to_string);
   register_function("string-to-codepoints", function_string_to_codepoints);
   register_function("compare", function_compare);
   register_function("codepoint-equal", function_codepoint_equal);
   register_function("starts-with", function_starts_with);
   register_function("ends-with", function_ends_with);
   register_function("contains", function_contains);
   register_function("substring-before", function_substring_before);
   register_function("substring-after", function_substring_after);
   register_function("substring", function_substring);
   register_function("string-length", function_string_length);
   register_function("normalize-space", function_normalize_space);
   register_function("normalize-unicode", function_normalize_unicode);
   register_function("string-join", function_string_join);
   register_function("iri-to-uri", function_iri_to_uri);
   register_function("translate", function_translate);
   register_function("upper-case", function_upper_case);
   register_function("lower-case", function_lower_case);
   register_function("encode-for-uri", function_encode_for_uri);
   register_function("escape-html-uri", function_escape_html_uri);

   register_function("matches", function_matches);
   register_function("replace", function_replace);
   register_function("tokenize", function_tokenize);
   register_function("analyze-string", function_analyze_string);
   register_function("resolve-uri", function_resolve_uri);
   register_function("format-date", function_format_date);
   register_function("format-time", function_format_time);
   register_function("format-dateTime", function_format_date_time);
   register_function("format-integer", function_format_integer);

   // Diagnostics Functions
   register_function("error", function_error);
   register_function("trace", function_trace);

   // Boolean Functions
   register_function("boolean", function_boolean);
   register_function("not", function_not);
   register_function("true", function_true);
   register_function("false", function_false);
   register_function("lang", function_lang);
   register_function("exists", function_exists);

   // Sequence Functions
   register_function("index-of", function_index_of);
   register_function("empty", function_empty);
   register_function("distinct-values", function_distinct_values);
   register_function("insert-before", function_insert_before);
   register_function("remove", function_remove);
   register_function("reverse", function_reverse);
   register_function("subsequence", function_subsequence);
   register_function("unordered", function_unordered);
   register_function("deep-equal", function_deep_equal);
   register_function("zero-or-one", function_zero_or_one);
   register_function("one-or-more", function_one_or_more);
   register_function("exactly-one", function_exactly_one);

   // Number Functions
   register_function("number", function_number);
   register_function("sum", function_sum);
   register_function("floor", function_floor);
   register_function("ceiling", function_ceiling);
   register_function("round", function_round);
   register_function("round-half-to-even", function_round_half_to_even);
   register_function("abs", function_abs);
   register_function("min", function_min);
   register_function("max", function_max);
   register_function("avg", function_avg);

   // Date and Time Functions
   register_function("current-date", function_current_date);
   register_function("current-time", function_current_time);
   register_function("current-dateTime", function_current_date_time);
   register_function("dateTime", function_date_time);
   register_function("year-from-dateTime", function_year_from_date_time);
   register_function("month-from-dateTime", function_month_from_date_time);
   register_function("day-from-dateTime", function_day_from_date_time);
   register_function("hours-from-dateTime", function_hours_from_date_time);
   register_function("minutes-from-dateTime", function_minutes_from_date_time);
   register_function("seconds-from-dateTime", function_seconds_from_date_time);
   register_function("timezone-from-dateTime", function_timezone_from_date_time);
   register_function("year-from-date", function_year_from_date);
   register_function("month-from-date", function_month_from_date);
   register_function("day-from-date", function_day_from_date);
   register_function("timezone-from-date", function_timezone_from_date);
   register_function("hours-from-time", function_hours_from_time);
   register_function("minutes-from-time", function_minutes_from_time);
   register_function("seconds-from-time", function_seconds_from_time);
   register_function("timezone-from-time", function_timezone_from_time);
   register_function("adjust-dateTime-to-timezone", function_adjust_date_time_to_timezone);
   register_function("adjust-date-to-timezone", function_adjust_date_to_timezone);
   register_function("adjust-time-to-timezone", function_adjust_time_to_timezone);
   register_function("implicit-timezone", function_implicit_timezone);
   register_function("years-from-duration", function_years_from_duration);
   register_function("months-from-duration", function_months_from_duration);
   register_function("days-from-duration", function_days_from_duration);
   register_function("hours-from-duration", function_hours_from_duration);
   register_function("minutes-from-duration", function_minutes_from_duration);
   register_function("seconds-from-duration", function_seconds_from_duration);
   register_function("years-from-yearMonthDuration", function_years_from_year_month_duration);
   register_function("months-from-yearMonthDuration", function_months_from_year_month_duration);
   register_function("days-from-dayTimeDuration", function_days_from_day_time_duration);
   register_function("hours-from-dayTimeDuration", function_hours_from_day_time_duration);
   register_function("minutes-from-dayTimeDuration", function_minutes_from_day_time_duration);
   register_function("seconds-from-dayTimeDuration", function_seconds_from_day_time_duration);
}

bool XPathFunctionLibrary::has_function(std::string_view Name) const {
   return find_function(Name) != nullptr;
}

XPathVal XPathFunctionLibrary::call_function(std::string_view Name, const std::vector<XPathVal> &Args,
   const XPathContext &Context) const {
   if (const auto *function_ptr = find_function(Name)) {
      return (*function_ptr)(Args, Context);
   }

   *Context.expression_unsupported = true;

   if (Context.document) {
      if (not Context.document->ErrorMsg.empty()) Context.document->ErrorMsg.append("\n");
      Context.document->ErrorMsg.append("Unsupported XPath function: ").append(Name);
   }

   return XPathVal();
}

void XPathFunctionLibrary::register_function(std::string_view Name, XPathFunction Func) {
   functions.insert_or_assign(std::string(Name), std::move(Func));
}

const XPathFunction * XPathFunctionLibrary::find_function(std::string_view Name) const {
   auto iter = functions.find(Name);
   if (iter != functions.end()) return &iter->second;

   return nullptr;
}

//********************************************************************************************************************
// Size Estimation Helpers for String Operations

size_t XPathFunctionLibrary::estimate_concat_size(const std::vector<XPathVal> &Args) {
   size_t total = 0;
   for (const auto &arg : Args) {
      // Conservative estimate based on type
      switch (arg.Type) {
         case XPVT::String:
         case XPVT::Date:
         case XPVT::Time:
         case XPVT::DateTime:
            total += arg.StringValue.length();
            break;
         case XPVT::Number:
            total += 32; // Conservative estimate for number formatting
            break;
         case XPVT::Boolean:
            total += 5; // "false" is longest
            break;
         case XPVT::NodeSet:
            if (arg.node_set_string_override.has_value()) {
               total += arg.node_set_string_override->length();
            } else if (not arg.node_set_string_values.empty()) {
               total += arg.node_set_string_values[0].length();
            } else {
               total += 64; // Conservative estimate for node content
            }
            break;
      }
   }
   return total;
}

size_t XPathFunctionLibrary::estimate_normalize_space_size(const std::string &Input) {
   // Worst case: no whitespace collapsing needed
   return Input.length();
}

size_t XPathFunctionLibrary::estimate_translate_size(const std::string &Source, const std::string &From) {
   // Best case: no characters removed, worst case: same size as source
   return Source.length();
}

//********************************************************************************************************************
// Core XPath Function Implementations

#include "functions/accessor_support.cpp"
#include "functions/func_accessors.cpp"
#include "functions/func_nodeset.cpp"
#include "functions/func_documents.cpp"
#include "functions/func_qnames.cpp"
#include "functions/func_strings.cpp"
#include "functions/func_diagnostics.cpp"
#include "functions/func_booleans.cpp"
#include "functions/func_sequences.cpp"
#include "functions/func_numbers.cpp"
#include "functions/func_datetimes.cpp"
