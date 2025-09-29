//********************************************************************************************************************
// XPath Function Library and Value System Implementation
//********************************************************************************************************************
//
// XPath expressions depend on a rich set of standard functions and a loosely typed value model.  This
// file provides both: XPathValue encapsulates conversions between node-sets, numbers, booleans, and
// strings, while the function registry offers implementations of the core function library required by
// the evaluator.  The code emphasises fidelity to the XPath 1.0 specification—string coercions mirror
// the spec's edge cases, numeric conversions preserve NaN semantics, and node-set operations respect
// document order guarantees enforced elsewhere in the module.
//
// The implementation is intentionally self-contained.  The evaluator interacts with XPathValue to
// manipulate intermediate results and delegates built-in function invocations to the routines defined
// below.  Keeping the behaviour consolidated here simplifies future extensions (for example, adding
// namespace-aware functions or performance-focused helpers) without polluting the evaluator with
// coercion details.

#include "xpath_functions.h"
#include "../xml.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <charconv>
#include <cmath>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <limits>
#include <sstream>
#include <string_view>
#include <unordered_set>
#include <utility>

#include "../../link/regex.h"
#include "../../link/unicode.h"

//********************************************************************************************************************
// XPathValue Implementation

namespace {

// Format a double according to XPath 1.0 number-to-string rules.
static std::string format_xpath_number(double Value)
{
   if (std::isnan(Value)) return std::string("NaN");
   if (std::isinf(Value)) return (Value > 0) ? std::string("Infinity") : std::string("-Infinity");
   if (Value IS 0.0) return std::string("0");

   std::ostringstream stream;
   stream.setf(std::ios::fmtflags(0), std::ios::floatfield);
   stream.precision(15);
   stream << Value;

   std::string result = stream.str();

   if (not result.empty() and (result[0] IS '+')) result.erase(result.begin());

   auto decimal = result.find('.');
   if (decimal != std::string::npos) {
      while ((!result.empty()) and (result.back() IS '0')) result.pop_back();
      if ((!result.empty()) and (result.back() IS '.')) result.pop_back();
   }

   return result;
}

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

static bool is_absolute_uri(std::string_view Uri)
{
   size_t index = 0u;
   while (index < Uri.length()) {
      char ch = Uri[index];
      if (ch IS ':') return index > 0u;
      if ((ch IS '/') or (ch IS '?') or (ch IS '#')) break;
      ++index;
   }
   return false;
}

static std::string strip_query_fragment(std::string_view Uri)
{
   size_t pos = Uri.find_first_of("?#");
   if (pos != std::string::npos) return std::string(Uri.substr(0u, pos));
   return std::string(Uri);
}

static std::string normalise_path_segments(const std::string &Path)
{
   std::vector<std::string> segments;
   bool leading_slash = (!Path.empty()) and (Path.front() IS '/');
   size_t start = leading_slash ? 1u : 0u;

   while (start <= Path.length()) {
      size_t end = Path.find('/', start);
      std::string segment;
      if (end IS std::string::npos) {
         segment = Path.substr(start);
         start = Path.length() + 1u;
      } else {
         segment = Path.substr(start, end - start);
         start = end + 1u;
      }

      if (segment.empty() or (segment IS ".")) continue;
      if (segment IS "..") {
         if (!segments.empty()) segments.pop_back();
         continue;
      }

      segments.push_back(segment);
   }

   std::string result;
   if (leading_slash) result.push_back('/');

   for (size_t index = 0u; index < segments.size(); ++index) {
      if (index > 0u) result.push_back('/');
      result.append(segments[index]);
   }

   if ((!Path.empty()) and (Path.back() IS '/') and (!result.empty()) and (result.back() != '/')) {
      result.push_back('/');
   }

   return result;
}

static std::string resolve_relative_uri(std::string_view Relative, std::string_view Base)
{
   if (Relative.empty()) return std::string(Base);
   if (is_absolute_uri(Relative)) return std::string(Relative);

   std::string base_clean = strip_query_fragment(Base);
   if (base_clean.empty()) return std::string();

   std::string prefix;
   std::string path = base_clean;

   size_t scheme_pos = base_clean.find(':');
   if (scheme_pos != std::string::npos) {
      prefix = base_clean.substr(0u, scheme_pos + 1u);
      path = base_clean.substr(scheme_pos + 1u);

      if (path.rfind("//", 0u) IS 0u) {
         size_t authority_end = path.find('/', 2u);
         if (authority_end IS std::string::npos) {
            std::string combined = prefix + path;
            if ((!Relative.empty()) and (Relative.front() != '/')) combined.push_back('/');
            combined.append(std::string(Relative));
            return combined;
         }

         prefix.append(path.substr(0u, authority_end));
         path = path.substr(authority_end);
      }
   }

   std::string directory = path;
   size_t last_slash = directory.rfind('/');
   if (last_slash != std::string::npos) directory = directory.substr(0u, last_slash + 1u);
   else directory.clear();

   std::string relative_path = std::string(Relative);
   if ((!relative_path.empty()) and (relative_path.front() IS '/')) {
      std::string combined_path = normalise_path_segments(relative_path);
      return prefix + combined_path;
   }

   std::string combined_path = directory;
   combined_path.append(relative_path);
   combined_path = normalise_path_segments(combined_path);

   return prefix + combined_path;
}

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

static std::string format_integer_component(long long Value, int Width, bool ZeroPad)
{
   bool negative = Value < 0;
   unsigned long long absolute = negative ? (unsigned long long)(-Value) : (unsigned long long)Value;
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

   std::string result;
   result.push_back(sign);
   if (hours < 10) result.push_back('0');
   result.append(std::to_string(hours));
   result.push_back(':');
   if (minutes < 10) result.push_back('0');
   result.append(std::to_string(minutes));
   return result;
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
         long long rounded = (long long)std::llround(Components.second);
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

static std::string format_integer_picture(long long Value, const std::string &Picture)
{
   bool negative = Value < 0;
   unsigned long long absolute = negative ? (unsigned long long)(-Value) : (unsigned long long)Value;
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

static std::string describe_xpath_value(const XPathValue &Value)
{
   switch (Value.type) {
      case XPathValueType::Boolean:
         return Value.boolean_value ? std::string("true") : std::string("false");
      case XPathValueType::Number:
         return Value.to_string();
      case XPathValueType::String:
      case XPathValueType::Date:
      case XPathValueType::Time:
      case XPathValueType::DateTime:
         return Value.string_value;
      case XPathValueType::NodeSet: {
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
               entries.push_back(XPathValue::node_string_value(node));
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
            std::string result;
            result.reserve(summary.length() + 24);
            result.append("node-set[");
            result.append(std::to_string(total_count));
            result.append("]");
            if (not summary.empty()) {
               result.append(": ");
               result.append(summary);
            }
            return result;
         }

         if (not summary.empty()) return summary;

         return std::string("()");
      }
   }

   return std::string();
}

pf::SyntaxOptions build_regex_options(const std::string &Flags, bool *UnsupportedFlag)
{
   pf::SyntaxOptions options = pf::SyntaxECMAScript;

   for (char flag : Flags) {
      unsigned char code = (unsigned char)flag;
      char normalised = (char)std::tolower(code);

      if (normalised IS 'i') options |= pf::SyntaxIgnoreCase;
      else if (normalised IS 'm') options |= pf::SyntaxMultiline;
      else if (normalised IS 's') options |= pf::SyntaxDotAll;
      else if (normalised IS 'u') options |= pf::SyntaxUnicodeSets;
      else if (normalised IS 'y') options |= pf::SyntaxSticky;
      else if (normalised IS 'q') options |= pf::SyntaxQuiet;
      else if (normalised IS 'v') options |= pf::SyntaxVerboseMode;
      else if (UnsupportedFlag) *UnsupportedFlag = true;
   }

   return options;
}

static void append_numbers_from_nodeset(const XPathValue &Value, std::vector<double> &Numbers)
{
   if (Value.node_set_string_override.has_value()) {
      double number = XPathValue::string_to_number(*Value.node_set_string_override);
      if (not std::isnan(number)) Numbers.push_back(number);
      return;
   }

   if (not Value.node_set_attributes.empty()) {
      for (const XMLAttrib *attribute : Value.node_set_attributes) {
         if (not attribute) continue;
         double number = XPathValue::string_to_number(attribute->Value);
         if (not std::isnan(number)) Numbers.push_back(number);
      }
      return;
   }

   if (not Value.node_set_string_values.empty()) {
      for (const std::string &entry : Value.node_set_string_values) {
         double number = XPathValue::string_to_number(entry);
         if (not std::isnan(number)) Numbers.push_back(number);
      }
      return;
   }

   for (XMLTag *node : Value.node_set) {
      if (not node) continue;
      std::string content = XPathValue::node_string_value(node);
      double number = XPathValue::string_to_number(content);
      if (not std::isnan(number)) Numbers.push_back(number);
   }
}

static void append_numbers_from_value(const XPathValue &Value, std::vector<double> &Numbers)
{
   switch (Value.type) {
      case XPathValueType::Number:
         if (not std::isnan(Value.number_value)) Numbers.push_back(Value.number_value);
         break;
      case XPathValueType::String:
      case XPathValueType::Date:
      case XPathValueType::Time:
      case XPathValueType::DateTime: {
         double number = XPathValue::string_to_number(Value.string_value);
         if (not std::isnan(number)) Numbers.push_back(number);
         break;
      }
      case XPathValueType::Boolean:
         Numbers.push_back(Value.boolean_value ? 1.0 : 0.0);
         break;
      case XPathValueType::NodeSet:
         append_numbers_from_nodeset(Value, Numbers);
         break;
   }
}

struct SequenceBuilder
{
   std::vector<XMLTag *> nodes;
   std::vector<const XMLAttrib *> attributes;
   std::vector<std::string> strings;
};

static size_t sequence_length(const XPathValue &Value)
{
   if (Value.type IS XPathValueType::NodeSet) {
      size_t length = Value.node_set.size();
      if (length < Value.node_set_attributes.size()) length = Value.node_set_attributes.size();
      if (length < Value.node_set_string_values.size()) length = Value.node_set_string_values.size();
      if ((length IS 0) and Value.node_set_string_override.has_value()) length = 1;
      return length;
   }

   return Value.is_empty() ? 0 : 1;
}

static std::string sequence_item_string(const XPathValue &Value, size_t Index)
{
   if (Value.type IS XPathValueType::NodeSet) {
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
         if (node) return XPathValue::node_string_value(node);
      }

      return std::string();
   }

   return Value.to_string();
}

static void append_sequence_item(const XPathValue &Value, size_t Index, SequenceBuilder &Builder)
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

static void append_value_to_sequence(const XPathValue &Value, SequenceBuilder &Builder)
{
   if (Value.type IS XPathValueType::NodeSet) {
      size_t length = sequence_length(Value);
      for (size_t index = 0; index < length; ++index) append_sequence_item(Value, index, Builder);
      return;
   }

   if (Value.is_empty()) return;

   Builder.nodes.push_back(nullptr);
   Builder.attributes.push_back(nullptr);
   Builder.strings.push_back(Value.to_string());
}

static XPathValue make_sequence_value(SequenceBuilder &&Builder)
{
   XPathValue result;
   result.type = XPathValueType::NodeSet;
   result.node_set = std::move(Builder.nodes);
   result.node_set_attributes = std::move(Builder.attributes);
   result.node_set_string_values = std::move(Builder.strings);

   if ((result.node_set_string_values.size() IS 1) and result.node_set.empty() and
       result.node_set_attributes.empty()) {
      result.node_set_string_override = result.node_set_string_values[0];
   }

   return result;
}

static XPathValue extract_sequence_item(const XPathValue &Value, size_t Index)
{
   if (Value.type IS XPathValueType::NodeSet) {
      size_t length = sequence_length(Value);
      if (Index >= length) return XPathValue();

      XPathValue result;
      result.type = XPathValueType::NodeSet;

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
   return XPathValue();
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

static bool xpath_values_equal(const XPathValue &Left, const XPathValue &Right)
{
   auto left_type = Left.type;
   auto right_type = Right.type;

   if ((left_type IS XPathValueType::Boolean) or (right_type IS XPathValueType::Boolean)) {
      bool left_boolean = Left.to_boolean();
      bool right_boolean = Right.to_boolean();
      return left_boolean IS right_boolean;
   }

   if ((left_type IS XPathValueType::Number) or (right_type IS XPathValueType::Number)) {
      double left_number = Left.to_number();
      double right_number = Right.to_number();
      if (std::isnan(left_number) or std::isnan(right_number)) return false;
      return numeric_equal(left_number, right_number);
   }

   if ((left_type IS XPathValueType::NodeSet) or (right_type IS XPathValueType::NodeSet)) {
      if ((left_type IS XPathValueType::NodeSet) and (right_type IS XPathValueType::NodeSet)) {
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

bool XPathValue::to_boolean() const {
   switch (type) {
      case XPathValueType::Boolean: return boolean_value;
      case XPathValueType::Number: return number_value != 0.0 and !std::isnan(number_value);
      case XPathValueType::String:
      case XPathValueType::Date:
      case XPathValueType::Time:
      case XPathValueType::DateTime:
         return !string_value.empty();
      case XPathValueType::NodeSet: return !node_set.empty();
   }
   return false;
}

// Convert a string to a number using XPath's relaxed numeric parsing rules.
double XPathValue::string_to_number(const std::string &Value)
{
   if (Value.empty()) return std::numeric_limits<double>::quiet_NaN();

   char *end_ptr = nullptr;
   double result = std::strtod(Value.c_str(), &end_ptr);
   if ((end_ptr IS Value.c_str()) or (*end_ptr != '\0')) {
      return std::numeric_limits<double>::quiet_NaN();
   }

   return result;
}

// Recursively append the textual content of a node and all of its descendants.
static void append_node_text(XMLTag *Node, std::string &Output)
{
   if (!Node) return;

   if (Node->isContent()) {
      if ((!Node->Attribs.empty()) and Node->Attribs[0].isContent()) {
         Output.append(Node->Attribs[0].Value);
      }

      for (auto &child : Node->Children) {
         append_node_text(&child, Output);
      }

      return;
   }

   for (auto &child : Node->Children) {
      if (child.Attribs.empty()) continue;

      if (child.Attribs[0].isContent()) {
         Output.append(child.Attribs[0].Value);
      }
      else append_node_text(&child, Output);
   }
}

// Obtain the string-value of a node, following XPath's definition for text and element nodes.
std::string XPathValue::node_string_value(XMLTag *Node)
{
   if (not Node) return std::string();

   std::string value;
   append_node_text(Node, value);
   return value;
}

double XPathValue::to_number() const {
   switch (type) {
      case XPathValueType::Boolean: return boolean_value ? 1.0 : 0.0;
      case XPathValueType::Number: return number_value;
      case XPathValueType::String:
      case XPathValueType::Date:
      case XPathValueType::Time:
      case XPathValueType::DateTime: {
         return string_to_number(string_value);
      }
      case XPathValueType::NodeSet: {
         if (node_set.empty()) return std::numeric_limits<double>::quiet_NaN();
         if (node_set_string_override.has_value()) return string_to_number(*node_set_string_override);
         if (not node_set_attributes.empty()) {
            const XMLAttrib *attribute = node_set_attributes[0];
            if (attribute) return string_to_number(attribute->Value);
         }
         if (not node_set_string_values.empty()) return string_to_number(node_set_string_values[0]);
         return string_to_number(node_string_value(node_set[0]));
      }
   }
   return 0.0;
}

std::string XPathValue::to_string() const {
   switch (type) {
      case XPathValueType::Boolean: return boolean_value ? "true" : "false";
      case XPathValueType::Number: {
         return format_xpath_number(number_value);
      }
      case XPathValueType::String:
      case XPathValueType::Date:
      case XPathValueType::Time:
      case XPathValueType::DateTime:
         return string_value;
      case XPathValueType::NodeSet: {
         if (node_set_string_override.has_value()) return *node_set_string_override;
         if (not node_set_attributes.empty()) {
            const XMLAttrib *attribute = node_set_attributes[0];
            if (attribute) return attribute->Value;
         }
         if (not node_set_string_values.empty()) return node_set_string_values[0];
         if (node_set.empty()) return "";

         return node_string_value(node_set[0]);
      }
   }
   return "";
}

std::vector<XMLTag *> XPathValue::to_node_set() const {
   if (type IS XPathValueType::NodeSet) {
      return node_set;
   }
   return {};
}

bool XPathValue::is_empty() const {
   switch (type) {
      case XPathValueType::Boolean: return false;
      case XPathValueType::Number: return false;
      case XPathValueType::String:
      case XPathValueType::Date:
      case XPathValueType::Time:
      case XPathValueType::DateTime:
         return string_value.empty();
      case XPathValueType::NodeSet: return node_set.empty();
   }
   return true;
}

size_t XPathValue::size() const {
   switch (type) {
      case XPathValueType::NodeSet: return node_set.size();
      case XPathValueType::String:
      case XPathValueType::Date:
      case XPathValueType::Time:
      case XPathValueType::DateTime:
         return is_empty() ? 0 : 1;
      default: return is_empty() ? 0 : 1;
   }
}

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

void XPathFunctionLibrary::register_core_functions() {
   // Node Set Functions
   register_function("last", function_last);
   register_function("position", function_position);
   register_function("count", function_count);
   register_function("id", function_id);
   register_function("local-name", function_local_name);
   register_function("namespace-uri", function_namespace_uri);
   register_function("name", function_name);

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
}

bool XPathFunctionLibrary::has_function(std::string_view Name) const {
   return find_function(Name) != nullptr;
}

XPathValue XPathFunctionLibrary::call_function(std::string_view Name, const std::vector<XPathValue> &Args,
   const XPathContext &Context) const {
   if (const auto *function_ptr = find_function(Name)) {
      return (*function_ptr)(Args, Context);
   }

   *Context.expression_unsupported = true;

   if (Context.document) {
      if (not Context.document->ErrorMsg.empty()) Context.document->ErrorMsg.append("\n");
      Context.document->ErrorMsg.append("Unsupported XPath function: ").append(Name);
   }

   return XPathValue();
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

size_t XPathFunctionLibrary::estimate_concat_size(const std::vector<XPathValue> &Args) {
   size_t total = 0;
   for (const auto &arg : Args) {
      // Conservative estimate based on type
      switch (arg.type) {
         case XPathValueType::String:
         case XPathValueType::Date:
         case XPathValueType::Time:
         case XPathValueType::DateTime:
            total += arg.string_value.length();
            break;
         case XPathValueType::Number:
            total += 32; // Conservative estimate for number formatting
            break;
         case XPathValueType::Boolean:
            total += 5; // "false" is longest
            break;
         case XPathValueType::NodeSet:
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
// Core XPath 1.0 Function Implementations

XPathValue XPathFunctionLibrary::function_last(const std::vector<XPathValue> &Args, const XPathContext &Context) {
   return XPathValue((double)Context.size);
}

XPathValue XPathFunctionLibrary::function_position(const std::vector<XPathValue> &Args, const XPathContext &Context) {
   return XPathValue((double)Context.position);
}

XPathValue XPathFunctionLibrary::function_count(const std::vector<XPathValue> &Args, const XPathContext &Context) {
   if (Args.size() != 1) return XPathValue(0.0);
   if (Args[0].type != XPathValueType::NodeSet) return XPathValue(0.0);
   return XPathValue((double)Args[0].node_set.size());
}

XPathValue XPathFunctionLibrary::function_id(const std::vector<XPathValue> &Args, const XPathContext &Context) {
   std::vector<XMLTag *> results;

   if (Args.empty()) return XPathValue(results);

   std::unordered_set<std::string> requested_ids;

   auto add_tokens = [&](const std::string &Value) {
      size_t start = Value.find_first_not_of(" \t\r\n");
      while (start != std::string::npos) {
         size_t end = Value.find_first_of(" \t\r\n", start);
         std::string token = Value.substr(start, (end IS std::string::npos) ? std::string::npos : end - start);
         if (not token.empty()) requested_ids.insert(token);
         if (end IS std::string::npos) break;
         start = Value.find_first_not_of(" \t\r\n", end);
      }
   };

   auto collect_from_value = [&](const XPathValue &Value) {
      switch (Value.type) {
         case XPathValueType::NodeSet: {
            if (not Value.node_set_string_values.empty()) {
               for (const auto &entry : Value.node_set_string_values) add_tokens(entry);
            }
            else if (Value.node_set_string_override.has_value()) add_tokens(*Value.node_set_string_override);
            else {
               for (auto *node : Value.node_set) {
                  if (not node) continue;
                  add_tokens(node->getContent());
               }
            }
            break;
         }

         case XPathValueType::String:
         case XPathValueType::Date:
         case XPathValueType::Time:
         case XPathValueType::DateTime:
            add_tokens(Value.string_value);
            break;

         case XPathValueType::Boolean:
            add_tokens(Value.to_string());
            break;

         case XPathValueType::Number:
            if (not std::isnan(Value.number_value)) add_tokens(Value.to_string());
            break;
      }
   };

   for (const auto &arg : Args) collect_from_value(arg);

   if (requested_ids.empty()) return XPathValue(results);
   if (not Context.document) return XPathValue(results);

   std::unordered_set<int> seen_tags;

   std::function<void(XMLTag &)> visit = [&](XMLTag &Tag) {
      if (Tag.isTag()) {
         for (size_t index = 1; index < Tag.Attribs.size(); ++index) {
            const auto &attrib = Tag.Attribs[index];
            if (not (pf::iequals(attrib.Name, "id") or pf::iequals(attrib.Name, "xml:id"))) continue;

            size_t start = attrib.Value.find_first_not_of(" \t\r\n");
            while (start != std::string::npos) {
               size_t end = attrib.Value.find_first_of(" \t\r\n", start);
               std::string token = attrib.Value.substr(start, (end IS std::string::npos) ? std::string::npos : end - start);
               if (not token.empty() and (requested_ids.find(token) != requested_ids.end())) {
                  if (seen_tags.insert(Tag.ID).second) results.push_back(&Tag);
                  break;
               }
               if (end IS std::string::npos) break;
               start = attrib.Value.find_first_not_of(" \t\r\n", end);
            }
         }
      }

      for (auto &child : Tag.Children) visit(child);
   };

   for (auto &root : Context.document->Tags) visit(root);

   return XPathValue(results);
}

XPathValue XPathFunctionLibrary::function_local_name(const std::vector<XPathValue> &Args, const XPathContext &Context) {
   XMLTag *target_node = nullptr;
   const XMLAttrib *target_attribute = nullptr;

   if (Args.empty()) {
      target_node = Context.context_node;
      target_attribute = Context.attribute_node;
   }
   else if (Args[0].type IS XPathValueType::NodeSet) target_node = Args[0].node_set.empty() ? nullptr : Args[0].node_set[0];
   else return XPathValue(std::string());

   if (target_attribute) {
      std::string_view name = target_attribute->Name;
      auto colon = name.find(':');
      if (colon IS std::string::npos) return XPathValue(std::string(name));
      return XPathValue(std::string(name.substr(colon + 1)));
   }

   if (not target_node) return XPathValue(std::string());
   if (target_node->Attribs.empty()) return XPathValue(std::string());

   std::string_view node_name = target_node->Attribs[0].Name;
   if (node_name.empty()) return XPathValue(std::string());

   auto colon = node_name.find(':');
   if (colon IS std::string::npos) return XPathValue(std::string(node_name));
   return XPathValue(std::string(node_name.substr(colon + 1)));
}

XPathValue XPathFunctionLibrary::function_namespace_uri(const std::vector<XPathValue> &Args, const XPathContext &Context) {
   XMLTag *target_node = nullptr;
   const XMLAttrib *target_attribute = nullptr;

   if (Args.empty()) {
      target_node = Context.context_node;
      target_attribute = Context.attribute_node;
   }
   else if (Args[0].type IS XPathValueType::NodeSet) target_node = Args[0].node_set.empty() ? nullptr : Args[0].node_set[0];
   else return XPathValue(std::string());

   if (target_attribute) {
      std::string_view name = target_attribute->Name;
      auto colon = name.find(':');
      if (colon IS std::string::npos) return XPathValue(std::string());

      std::string prefix(name.substr(0, colon));
      if (pf::iequals(prefix, "xml")) return XPathValue("http://www.w3.org/XML/1998/namespace");
      if (pf::iequals(prefix, "xmlns")) return XPathValue("http://www.w3.org/2000/xmlns/");

      XMLTag *scope_node = target_node ? target_node : Context.context_node;
      if (not scope_node) return XPathValue(std::string());

      if (Context.document) {
         std::string uri = find_in_scope_namespace(scope_node, Context.document, prefix);
         return XPathValue(uri);
      }

      return XPathValue(std::string());
   }

   if (not target_node) return XPathValue(std::string());

   std::string prefix;
   if (not target_node->Attribs.empty()) {
      std::string_view node_name = target_node->Attribs[0].Name;
      auto colon = node_name.find(':');
      if (colon != std::string::npos) prefix = std::string(node_name.substr(0, colon));
   }

   if (not prefix.empty()) {
      if (pf::iequals(prefix, "xml")) return XPathValue("http://www.w3.org/XML/1998/namespace");
      if (pf::iequals(prefix, "xmlns")) return XPathValue("http://www.w3.org/2000/xmlns/");
   }

   if ((target_node->NamespaceID != 0) and Context.document) {
      if (auto uri = Context.document->getNamespaceURI(target_node->NamespaceID)) return XPathValue(*uri);
   }

   if (Context.document) {
      std::string uri;
      if (not prefix.empty()) uri = find_in_scope_namespace(target_node, Context.document, prefix);
      else uri = find_in_scope_namespace(target_node, Context.document, std::string());
      return XPathValue(uri);
   }

   return XPathValue(std::string());
}

XPathValue XPathFunctionLibrary::function_name(const std::vector<XPathValue> &Args, const XPathContext &Context) {
   XMLTag *target_node = nullptr;
   const XMLAttrib *target_attribute = nullptr;

   if (Args.empty()) {
      target_node = Context.context_node;
      target_attribute = Context.attribute_node;
   }
   else if (Args[0].type IS XPathValueType::NodeSet) target_node = Args[0].node_set.empty() ? nullptr : Args[0].node_set[0];
   else return XPathValue(std::string());

   if (target_attribute) return XPathValue(target_attribute->Name);

   if (not target_node) return XPathValue(std::string());
   if (target_node->Attribs.empty()) return XPathValue(std::string());

   return XPathValue(target_node->Attribs[0].Name);
}

XPathValue XPathFunctionLibrary::function_string(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   if (Args.empty()) {
      if (Context.attribute_node) {
         return XPathValue(Context.attribute_node->Value);
      }

      if (Context.context_node) {
         std::vector<XMLTag *> nodes = { Context.context_node };
         XPathValue node_set_value(nodes);
         return XPathValue(node_set_value.to_string());
      }
      return XPathValue(std::string());
   }
   return XPathValue(Args[0].to_string());
}

XPathValue XPathFunctionLibrary::function_concat(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   // Pre-calculate total length to avoid quadratic behavior
   size_t total_length = 0;
   std::vector<std::string> arg_strings;
   arg_strings.reserve(Args.size());

   for (const auto &arg : Args) {
      arg_strings.emplace_back(arg.to_string());
      total_length += arg_strings.back().length();
   }

   std::string result;
   result.reserve(total_length);

   for (const auto &str : arg_strings) {
      result += str;
   }

   return XPathValue(result);
}

XPathValue XPathFunctionLibrary::function_codepoints_to_string(const std::vector<XPathValue> &Args,
   const XPathContext &Context)
{
   if (Args.empty()) return XPathValue(std::string());

   const XPathValue &sequence = Args[0];
   size_t length = sequence_length(sequence);
   if (length IS 0u) return XPathValue(std::string());

   std::string output;
   output.reserve(length * 4u);

   for (size_t index = 0u; index < length; ++index) {
      XPathValue item = extract_sequence_item(sequence, index);
      double numeric = item.to_number();
      if (std::isnan(numeric)) continue;

      long long rounded = (long long)std::llround(numeric);
      if (rounded < 0) {
         append_codepoint_utf8(output, 0xFFFDu);
         continue;
      }

      append_codepoint_utf8(output, (uint32_t)rounded);
   }

   (void)Context;
   return XPathValue(output);
}

XPathValue XPathFunctionLibrary::function_string_to_codepoints(const std::vector<XPathValue> &Args,
   const XPathContext &Context)
{
   if (Args.empty()) return XPathValue(std::vector<XMLTag *>());

   std::string input = Args[0].to_string();

   SequenceBuilder builder;
   size_t offset = 0u;

   while (offset < input.length()) {
      int length = 0;
      uint32_t code = UTF8ReadValue(input.c_str() + offset, &length);
      if (length <= 0) {
         code = (unsigned char)input[offset];
         length = 1;
      }

      builder.nodes.push_back(nullptr);
      builder.attributes.push_back(nullptr);
      builder.strings.push_back(std::to_string(code));

      offset += (size_t)length;
   }

   (void)Context;
   return make_sequence_value(std::move(builder));
}

XPathValue XPathFunctionLibrary::function_compare(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   if (Args.size() < 2u) return XPathValue();
   if (Args[0].is_empty() or Args[1].is_empty()) return XPathValue();

   std::string left = Args[0].to_string();
   std::string right = Args[1].to_string();
   std::string collation = (Args.size() > 2u) ? Args[2].to_string() : std::string();

   if (!collation.empty() and (collation != "http://www.w3.org/2005/xpath-functions/collation/codepoint") and
       (collation != "unicode")) {
      if (Context.expression_unsupported) *Context.expression_unsupported = true;
      return XPathValue();
   }

   int result = 0;
   if (left < right) result = -1;
   else if (left IS right) result = 0;
   else result = 1;

   return XPathValue((double)result);
}

XPathValue XPathFunctionLibrary::function_codepoint_equal(const std::vector<XPathValue> &Args,
   const XPathContext &Context)
{
   if (Args.size() < 2u) return XPathValue();
   if (Args[0].is_empty() or Args[1].is_empty()) return XPathValue();

   std::string first = Args[0].to_string();
   std::string second = Args[1].to_string();

   (void)Context;
   return XPathValue(first IS second);
}

XPathValue XPathFunctionLibrary::function_starts_with(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   if (Args.size() != 2) return XPathValue(false);
   std::string str = Args[0].to_string();
   std::string prefix = Args[1].to_string();
   return XPathValue(str.substr(0, prefix.length()) IS prefix);
}

XPathValue XPathFunctionLibrary::function_ends_with(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   if (Args.size() != 2u) return XPathValue(false);

   std::string input = Args[0].to_string();
   std::string suffix = Args[1].to_string();
   if (suffix.length() > input.length()) return XPathValue(false);

   return XPathValue(input.compare(input.length() - suffix.length(), suffix.length(), suffix) IS 0);
}

XPathValue XPathFunctionLibrary::function_contains(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   if (Args.size() != 2) return XPathValue(false);
   std::string str = Args[0].to_string();
   std::string substr = Args[1].to_string();
   return XPathValue(str.find(substr) != std::string::npos);
}

XPathValue XPathFunctionLibrary::function_substring_before(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   if (Args.size() != 2) return XPathValue(std::string());

   std::string source = Args[0].to_string();
   std::string pattern = Args[1].to_string();

   if (pattern.empty()) return XPathValue(std::string());

   size_t position = source.find(pattern);
   if (position IS std::string::npos) return XPathValue(std::string());

   return XPathValue(source.substr(0, position));
}

XPathValue XPathFunctionLibrary::function_substring_after(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   if (Args.size() != 2) return XPathValue(std::string());

   std::string source = Args[0].to_string();
   std::string pattern = Args[1].to_string();

   if (pattern.empty()) return XPathValue(source);

   size_t position = source.find(pattern);
   if (position IS std::string::npos) return XPathValue(std::string());

   return XPathValue(source.substr(position + pattern.length()));
}

XPathValue XPathFunctionLibrary::function_substring(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   if (Args.size() < 2 or Args.size() > 3) return XPathValue(std::string());

   std::string str = Args[0].to_string();
   if (str.empty()) return XPathValue(std::string());

   double start_pos = Args[1].to_number();
   if (std::isnan(start_pos) or std::isinf(start_pos)) return XPathValue(std::string());

   // XPath uses 1-based indexing
   int start_index = (int)std::round(start_pos) - 1;
   if (start_index < 0) start_index = 0;
   if (start_index >= (int)str.length()) return XPathValue(std::string());

   if (Args.size() IS 3) {
      double length = Args[2].to_number();
      if (std::isnan(length) or std::isinf(length) or length <= 0) return XPathValue(std::string());

      int len = (int)std::round(length);
      int remaining = (int)str.length() - start_index;
      if (len > remaining) len = remaining;

      // For small substrings, avoid extra allocation overhead
      if (len <= 0) return XPathValue(std::string());

      return XPathValue(str.substr(start_index, len));
   }

   // Return substring from start_index to end
   return XPathValue(str.substr(start_index));
}

XPathValue XPathFunctionLibrary::function_string_length(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   std::string str;
   if (Args.empty()) {
      if (Context.context_node) {
         std::vector<XMLTag *> nodes = { Context.context_node };
         XPathValue node_set_value(nodes);
         str = node_set_value.to_string();
      }
   }
   else str = Args[0].to_string();
   return XPathValue((double)str.length());
}

XPathValue XPathFunctionLibrary::function_normalize_space(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   std::string str;
   if (Args.empty()) {
      if (Context.context_node) {
         std::vector<XMLTag *> nodes = { Context.context_node };
         XPathValue node_set_value(nodes);
         str = node_set_value.to_string();
      }
   }
   else str = Args[0].to_string();

   // Remove leading and trailing whitespace, collapse internal whitespace
   size_t start = str.find_first_not_of(" \t\n\r");
   if (start IS std::string::npos) return XPathValue(std::string());

   size_t end = str.find_last_not_of(" \t\n\r");
   str = str.substr(start, end - start + 1);

   // Collapse internal whitespace
   std::string result;
   result.reserve(estimate_normalize_space_size(str));
   bool in_whitespace = false;
   for (char c : str) {
      if (c IS ' ' or c IS '\t' or c IS '\n' or c IS '\r') {
         if (not in_whitespace) {
            result += ' ';
            in_whitespace = true;
         }
      }
      else {
         result += c;
         in_whitespace = false;
      }
   }

   return XPathValue(result);
}

XPathValue XPathFunctionLibrary::function_normalize_unicode(const std::vector<XPathValue> &Args,
   const XPathContext &Context)
{
   if (Args.empty()) return XPathValue(std::string());

   std::string input = Args[0].to_string();
   std::string form = (Args.size() > 1u) ? Args[1].to_string() : std::string("NFC");

   bool unsupported = false;
   std::string normalised = simple_normalise_unicode(input, form, &unsupported);
   if (unsupported and Context.expression_unsupported) *Context.expression_unsupported = true;

   return XPathValue(normalised);
}

XPathValue XPathFunctionLibrary::function_string_join(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   if (Args.empty()) return XPathValue(std::string());

   const XPathValue &sequence = Args[0];
   std::string separator = (Args.size() > 1u) ? Args[1].to_string() : std::string();

   size_t length = sequence_length(sequence);
   if (length IS 0u) return XPathValue(std::string());

   std::string result;
   for (size_t index = 0u; index < length; ++index) {
      if (index > 0u) result.append(separator);
      result.append(sequence_item_string(sequence, index));
   }

   (void)Context;
   return XPathValue(result);
}

XPathValue XPathFunctionLibrary::function_translate(const std::vector<XPathValue> &Args, const XPathContext &Context) {
   if (Args.size() != 3) return XPathValue(std::string());

   std::string source = Args[0].to_string();
   std::string from = Args[1].to_string();
   std::string to = Args[2].to_string();

   if (source.empty()) return XPathValue(std::string());

   // Pre-size result string based on input length (worst case: no characters removed)
   std::string result;
   result.reserve(source.size());

   // Build character mapping for faster lookups if worth it
   if (from.size() > 10) {
      // Use array mapping for better performance with larger translation sets
      std::array<int, 256> char_map;
      char_map.fill(-1);

      for (size_t i = 0; i < from.size() and i < 256; ++i) {
         unsigned char ch = (unsigned char)from[i];
         if (char_map[ch] IS -1) { // First occurrence takes precedence
            char_map[ch] = (int)i;
         }
      }

      for (char ch : source) {
         unsigned char uch = (unsigned char)ch;
         int index = char_map[uch];
         if (index IS -1) result.push_back(ch);
         else if (index < (int)to.length()) result.push_back(to[index]);
      }
   }
   else {
      // Use simple find for small translation sets
      for (char ch : source) {
         size_t index = from.find(ch);
         if (index IS std::string::npos) result.push_back(ch);
         else if (index < to.length()) result.push_back(to[index]);
      }
   }

   return XPathValue(result);
}

XPathValue XPathFunctionLibrary::function_upper_case(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   std::string input;

   if (Args.empty()) {
      if (Context.attribute_node) input = Context.attribute_node->Value;
      else if (Context.context_node) {
         std::vector<XMLTag *> nodes = { Context.context_node };
         XPathValue node_value(nodes);
         input = node_value.to_string();
      }
      else input = std::string();
   }
   else input = Args[0].to_string();

   return XPathValue(apply_string_case(input, true));
}

XPathValue XPathFunctionLibrary::function_lower_case(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   std::string input;

   if (Args.empty()) {
      if (Context.attribute_node) input = Context.attribute_node->Value;
      else if (Context.context_node) {
         std::vector<XMLTag *> nodes = { Context.context_node };
         XPathValue node_value(nodes);
         input = node_value.to_string();
      }
      else input = std::string();
   }
   else input = Args[0].to_string();

   return XPathValue(apply_string_case(input, false));
}

XPathValue XPathFunctionLibrary::function_iri_to_uri(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   std::string input;

   if (Args.empty()) {
      if (Context.attribute_node) input = Context.attribute_node->Value;
      else if (Context.context_node) {
         std::vector<XMLTag *> nodes = { Context.context_node };
         XPathValue node_value(nodes);
         input = node_value.to_string();
      }
      else input = std::string();
   }
   else input = Args[0].to_string();

   std::string result;
   result.reserve(input.length() * 3u);

   static const char hex_digits[] = "0123456789ABCDEF";

   for (unsigned char code : input) {
      if (code <= 0x7Fu) {
         result.push_back((char)code);
      }
      else {
         result.push_back('%');
         result.push_back(hex_digits[(code >> 4u) & 0x0Fu]);
         result.push_back(hex_digits[code & 0x0Fu]);
      }
   }

   return XPathValue(result);
}

XPathValue XPathFunctionLibrary::function_encode_for_uri(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   std::string input;

   if (Args.empty()) {
      if (Context.attribute_node) input = Context.attribute_node->Value;
      else if (Context.context_node) {
         std::vector<XMLTag *> nodes = { Context.context_node };
         XPathValue node_value(nodes);
         input = node_value.to_string();
      }
      else input = std::string();
   }
   else input = Args[0].to_string();

   return XPathValue(encode_for_uri_impl(input));
}

XPathValue XPathFunctionLibrary::function_escape_html_uri(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   std::string input;

   if (Args.empty()) {
      if (Context.attribute_node) input = Context.attribute_node->Value;
      else if (Context.context_node) {
         std::vector<XMLTag *> nodes = { Context.context_node };
         XPathValue node_value(nodes);
         input = node_value.to_string();
      }
      else input = std::string();
   }
   else input = Args[0].to_string();

   return XPathValue(escape_html_uri_impl(input));
}

XPathValue XPathFunctionLibrary::function_error(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   std::string error_code = "err:FOER0000";
   std::string description = "User-defined error";
   std::string detail;

   if (not Args.empty()) {
      const XPathValue &code_value = Args[0];
      if (not code_value.is_empty()) error_code = code_value.to_string();
   }

   if (Args.size() > 1) {
      const XPathValue &description_value = Args[1];
      if (not description_value.is_empty()) description = description_value.to_string();
   }

   if (Args.size() > 2) {
      const XPathValue &detail_value = Args[2];
      if (not detail_value.is_empty()) detail = describe_xpath_value(detail_value);
   }

   pf::Log log(__FUNCTION__);

   if (detail.empty()) {
      log.warning("XPath error (%s): %s", error_code.c_str(), description.c_str());
   }
   else {
      log.warning("XPath error (%s): %s [%s]", error_code.c_str(), description.c_str(), detail.c_str());
   }

   if (Context.expression_unsupported) {
      *Context.expression_unsupported = true;
   }

   if (Context.document) {
      if (not Context.document->ErrorMsg.empty()) Context.document->ErrorMsg.append("\n");
      Context.document->ErrorMsg.append("XPath error ");
      Context.document->ErrorMsg.append(error_code);
      Context.document->ErrorMsg.append(": ");
      Context.document->ErrorMsg.append(description);
      if (not detail.empty()) {
         Context.document->ErrorMsg.append(" [");
         Context.document->ErrorMsg.append(detail);
         Context.document->ErrorMsg.append("]");
      }
   }

   return XPathValue();
}

XPathValue XPathFunctionLibrary::function_trace(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   if (Args.empty()) return XPathValue();

   const XPathValue &value = Args[0];
   std::string label = "trace";

   if (Args.size() > 1) {
      const XPathValue &label_value = Args[1];
      if (not label_value.is_empty()) label = label_value.to_string();
   }

   if (label.empty()) label = "trace";

   std::string summary = describe_xpath_value(value);
   if (summary.empty()) summary = std::string("()");

   pf::Log log(__FUNCTION__);
   log.msg("XPath trace [%s]: %s", label.c_str(), summary.c_str());

   (void)Context;

   return value;
}

XPathValue XPathFunctionLibrary::function_matches(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   if (Args.size() < 2 or Args.size() > 3) return XPathValue(false);

   std::string input = Args[0].to_string();
   std::string pattern = Args[1].to_string();
   std::string flags = (Args.size() IS 3) ? Args[2].to_string() : std::string();

   pf::Regex compiled;
   if (not compiled.compile(pattern, build_regex_options(flags, Context.expression_unsupported))) {
      return XPathValue(false);
   }

   pf::MatchResult result;
   bool matched = compiled.search(input, result);
   return XPathValue(matched);
}

XPathValue XPathFunctionLibrary::function_replace(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   if (Args.size() < 3 or Args.size() > 4) return XPathValue(std::string());

   std::string input = Args[0].to_string();
   std::string pattern = Args[1].to_string();
   std::string replacement = Args[2].to_string();
   std::string flags = (Args.size() IS 4) ? Args[3].to_string() : std::string();

   pf::Regex compiled;
   if (not compiled.compile(pattern, build_regex_options(flags, Context.expression_unsupported))) {
      return XPathValue(input);
   }

   std::string replaced;
   if (not compiled.replace(input, replacement, replaced)) replaced = input;

   return XPathValue(replaced);
}

XPathValue XPathFunctionLibrary::function_tokenize(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   if (Args.size() < 2 or Args.size() > 3) return XPathValue(std::vector<XMLTag *>());

   std::string input = Args[0].to_string();
   std::string pattern = Args[1].to_string();
   std::string flags = (Args.size() IS 3) ? Args[2].to_string() : std::string();

   std::vector<std::string> tokens;

   if (pattern.empty()) {
      for (size_t index = 0; index < input.length(); ++index) {
         tokens.emplace_back(input.substr(index, 1));
      }
   }
   else {
      pf::SyntaxOptions options = build_regex_options(flags, Context.expression_unsupported);

      pf::Regex compiled;
      if (not compiled.compile(pattern, options)) {
         return XPathValue(std::vector<XMLTag *>());
      }

      compiled.tokenize(input, -1, tokens);

      if (not tokens.empty() and tokens.back().empty()) tokens.pop_back();
   }

   std::vector<XMLTag *> placeholders(tokens.size(), nullptr);
   return XPathValue(placeholders, std::nullopt, tokens);
}

XPathValue XPathFunctionLibrary::function_analyze_string(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   if (Args.size() < 2u or Args.size() > 3u) return XPathValue(std::vector<XMLTag *>());

   std::string input = Args[0].to_string();
   std::string pattern = Args[1].to_string();
   std::string flags = (Args.size() > 2u) ? Args[2].to_string() : std::string();

   pf::Regex compiled;
   if (not compiled.compile(pattern, build_regex_options(flags, Context.expression_unsupported))) {
      return XPathValue(std::vector<XMLTag *>());
   }

   SequenceBuilder builder;
   size_t search_offset = 0u;
   size_t guard = 0u;

   while (search_offset <= input.length()) {
      std::string_view remaining(input.c_str() + search_offset, input.length() - search_offset);
      pf::MatchResult match;
      if (!compiled.search(remaining, match)) {
         if (!remaining.empty()) {
            builder.nodes.push_back(nullptr);
            builder.attributes.push_back(nullptr);
            builder.strings.push_back(std::string("non-match:") + std::string(remaining));
         }
         break;
      }

      if ((match.span.offset != (size_t)std::string::npos) and (match.span.offset > 0u)) {
         std::string unmatched = std::string(remaining.substr(0u, match.span.offset));
         if (!unmatched.empty()) {
            builder.nodes.push_back(nullptr);
            builder.attributes.push_back(nullptr);
            builder.strings.push_back(std::string("non-match:") + unmatched);
         }
      }

      std::string matched_text;
      if (match.span.offset != (size_t)std::string::npos) {
         matched_text = std::string(remaining.substr(match.span.offset, match.span.length));
      }

      builder.nodes.push_back(nullptr);
      builder.attributes.push_back(nullptr);
      builder.strings.push_back(std::string("match:") + matched_text);

      for (size_t index = 1u; index < match.captures.size(); ++index) {
         if (match.capture_spans[index].offset IS (size_t)std::string::npos) continue;
         builder.nodes.push_back(nullptr);
         builder.attributes.push_back(nullptr);
         builder.strings.push_back(std::string("group") + std::to_string(index) + std::string(":") + match.captures[index]);
      }

      size_t advance = 0u;
      if (match.span.offset != (size_t)std::string::npos) advance = match.span.offset;
      if (match.span.length > 0u) advance += match.span.length;
      else advance += 1u;

      search_offset += advance;

      guard++;
      if (guard > input.length() + 8u) break;
   }

   return make_sequence_value(std::move(builder));
}

XPathValue XPathFunctionLibrary::function_resolve_uri(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   if (Args.empty()) return XPathValue();

   std::string relative = Args[0].to_string();
   std::string base;

   if (Args.size() > 1u and not Args[1].is_empty()) base = Args[1].to_string();
   else if (Context.document) base = Context.document->Path;

   if (relative.empty()) {
      if (base.empty()) return XPathValue();
      return XPathValue(base);
   }

   if (is_absolute_uri(relative)) return XPathValue(relative);
   if (base.empty()) return XPathValue();

   std::string resolved = resolve_relative_uri(relative, base);
   return XPathValue(resolved);
}

XPathValue XPathFunctionLibrary::function_boolean(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   if (Args.size() != 1) return XPathValue(false);
   return XPathValue(Args[0].to_boolean());
}

XPathValue XPathFunctionLibrary::function_not(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   if (Args.size() != 1) return XPathValue(true);
   return XPathValue(!Args[0].to_boolean());
}

XPathValue XPathFunctionLibrary::function_true(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   return XPathValue(true);
}

XPathValue XPathFunctionLibrary::function_false(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   return XPathValue(false);
}

XPathValue XPathFunctionLibrary::function_lang(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   if (Args.size() != 1) return XPathValue(false);

   std::string requested = Args[0].to_string();
   if (requested.empty()) return XPathValue(false);

   XMLTag *node = Context.context_node;
   if (not node) return XPathValue(false);

   std::string language = find_language_for_node(node, Context.document);
   if (language.empty()) return XPathValue(false);

   return XPathValue(language_matches(language, requested));
}

XPathValue XPathFunctionLibrary::function_exists(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   if (Args.empty()) return XPathValue(false);

   const XPathValue &value = Args[0];

   if (value.type IS XPathValueType::NodeSet) {
      if (not value.node_set.empty()) return XPathValue(true);

      if (value.node_set_string_override.has_value()) return XPathValue(true);

      if (not value.node_set_string_values.empty()) return XPathValue(true);

      if (not value.node_set_attributes.empty()) return XPathValue(true);

      return XPathValue(false);
   }

   return XPathValue(true);
}

XPathValue XPathFunctionLibrary::function_index_of(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   if (Args.size() < 2) return XPathValue(std::vector<XMLTag *>());

   if ((Args.size() > 2) and Context.expression_unsupported) *Context.expression_unsupported = true;

   const XPathValue &sequence = Args[0];
   const XPathValue &lookup = Args[1];

   size_t length = sequence_length(sequence);
   if (length IS 0) return XPathValue(std::vector<XMLTag *>());

   XPathValue target = extract_sequence_item(lookup, 0);
   SequenceBuilder builder;

   for (size_t index = 0; index < length; ++index) {
      XPathValue item = extract_sequence_item(sequence, index);
      if (xpath_values_equal(item, target)) {
         builder.nodes.push_back(nullptr);
         builder.attributes.push_back(nullptr);
         builder.strings.push_back(format_xpath_number(double(index + 1)));
      }
   }

   return make_sequence_value(std::move(builder));
}

XPathValue XPathFunctionLibrary::function_empty(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   if (Args.empty()) return XPathValue(true);

   size_t length = sequence_length(Args[0]);
   return XPathValue(length IS 0);
}

XPathValue XPathFunctionLibrary::function_distinct_values(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   if (Args.empty()) return XPathValue(std::vector<XMLTag *>());

   if ((Args.size() > 1) and Context.expression_unsupported) *Context.expression_unsupported = true;

   const XPathValue &sequence = Args[0];
   size_t length = sequence_length(sequence);
   if (length IS 0) return XPathValue(std::vector<XMLTag *>());

   std::unordered_set<std::string> seen;
   SequenceBuilder builder;

   for (size_t index = 0; index < length; ++index) {
      std::string key = sequence_item_string(sequence, index);
      auto insert_result = seen.insert(key);
      if (insert_result.second) {
         XPathValue item = extract_sequence_item(sequence, index);
         append_value_to_sequence(item, builder);
      }
   }

   return make_sequence_value(std::move(builder));
}

XPathValue XPathFunctionLibrary::function_insert_before(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   if (Args.size() < 3) {
      if (Args.empty()) return XPathValue(std::vector<XMLTag *>());
      return Args[0];
   }

   const XPathValue &sequence = Args[0];
   double position_value = Args[1].to_number();
   const XPathValue &insertion = Args[2];

   size_t length = sequence_length(sequence);
   size_t insert_index = 0;

   if (std::isnan(position_value)) insert_index = 0;
   else if (std::isinf(position_value)) insert_index = (position_value > 0.0) ? length : 0;
   else {
      long long floored = (long long)std::floor(position_value);
      if (floored <= 1) insert_index = 0;
      else if (floored > (long long)length) insert_index = length;
      else insert_index = (size_t)(floored - 1);
   }

   if (insert_index > length) insert_index = length;

   SequenceBuilder builder;

   for (size_t index = 0; index < length; ++index) {
      if (index IS insert_index) append_value_to_sequence(insertion, builder);
      XPathValue item = extract_sequence_item(sequence, index);
      append_value_to_sequence(item, builder);
   }

   if (insert_index >= length) append_value_to_sequence(insertion, builder);

   return make_sequence_value(std::move(builder));
}

XPathValue XPathFunctionLibrary::function_remove(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   if (Args.size() < 2) {
      if (Args.empty()) return XPathValue(std::vector<XMLTag *>());
      return Args[0];
   }

   const XPathValue &sequence = Args[0];
   double position_value = Args[1].to_number();
   size_t length = sequence_length(sequence);

   if (length IS 0) return XPathValue(std::vector<XMLTag *>());
   if (std::isnan(position_value) or std::isinf(position_value)) return sequence;

   long long floored = (long long)std::floor(position_value);
   if (floored < 1) return sequence;
   if (floored > (long long)length) return sequence;

   size_t remove_index = (size_t)(floored - 1);
   SequenceBuilder builder;

   for (size_t index = 0; index < length; ++index) {
      if (index IS remove_index) continue;
      XPathValue item = extract_sequence_item(sequence, index);
      append_value_to_sequence(item, builder);
   }

   return make_sequence_value(std::move(builder));
}

XPathValue XPathFunctionLibrary::function_reverse(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   if (Args.empty()) return XPathValue(std::vector<XMLTag *>());

   const XPathValue &sequence = Args[0];
   size_t length = sequence_length(sequence);
   SequenceBuilder builder;

   for (size_t index = length; index > 0; --index) {
      XPathValue item = extract_sequence_item(sequence, index - 1);
      append_value_to_sequence(item, builder);
   }

   return make_sequence_value(std::move(builder));
}

XPathValue XPathFunctionLibrary::function_subsequence(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   if (Args.size() < 2) return XPathValue(std::vector<XMLTag *>());

   const XPathValue &sequence = Args[0];
   size_t length = sequence_length(sequence);
   if (length IS 0) return XPathValue(std::vector<XMLTag *>());

   double start_value = Args[1].to_number();
   if (std::isnan(start_value)) return XPathValue(std::vector<XMLTag *>());

   double min_position = std::ceil(start_value);
   if (std::isnan(min_position)) return XPathValue(std::vector<XMLTag *>());
   if (min_position < 1.0) min_position = 1.0;

   double max_position = std::numeric_limits<double>::infinity();
   if (Args.size() > 2) {
      double length_value = Args[2].to_number();
      if (std::isnan(length_value)) return XPathValue(std::vector<XMLTag *>());
      if (length_value <= 0.0) return XPathValue(std::vector<XMLTag *>());

      max_position = std::ceil(start_value + length_value);
      if (std::isnan(max_position)) return XPathValue(std::vector<XMLTag *>());
   }

   SequenceBuilder builder;

   for (size_t index = 0; index < length; ++index) {
      double position = double(index + 1);
      if (position < min_position) continue;
      if ((not std::isinf(max_position)) and (position >= max_position)) break;
      XPathValue item = extract_sequence_item(sequence, index);
      append_value_to_sequence(item, builder);
   }

   return make_sequence_value(std::move(builder));
}

XPathValue XPathFunctionLibrary::function_unordered(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   if (Args.empty()) return XPathValue(std::vector<XMLTag *>());
   return Args[0];
}

XPathValue XPathFunctionLibrary::function_deep_equal(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   if (Args.size() < 2) return XPathValue(false);

   if ((Args.size() > 2) and Context.expression_unsupported) *Context.expression_unsupported = true;

   const XPathValue &left = Args[0];
   const XPathValue &right = Args[1];

   size_t left_length = sequence_length(left);
   size_t right_length = sequence_length(right);
   if (left_length != right_length) return XPathValue(false);

   for (size_t index = 0; index < left_length; ++index) {
      XPathValue left_item = extract_sequence_item(left, index);
      XPathValue right_item = extract_sequence_item(right, index);
      if (not xpath_values_equal(left_item, right_item)) return XPathValue(false);
   }

   return XPathValue(true);
}

XPathValue XPathFunctionLibrary::function_zero_or_one(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   if (Args.empty()) return XPathValue();

   const XPathValue &sequence = Args[0];
   size_t length = sequence_length(sequence);

   if (length <= 1) return sequence;

   flag_cardinality_error(Context, "zero-or-one", "argument has more than one item");
   return XPathValue();
}

XPathValue XPathFunctionLibrary::function_one_or_more(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   if (Args.empty()) return XPathValue();

   const XPathValue &sequence = Args[0];
   size_t length = sequence_length(sequence);

   if (length IS 0) {
      flag_cardinality_error(Context, "one-or-more", "argument is empty");
      return XPathValue();
   }

   return sequence;
}

XPathValue XPathFunctionLibrary::function_exactly_one(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   if (Args.empty()) return XPathValue();

   const XPathValue &sequence = Args[0];
   size_t length = sequence_length(sequence);

   if (length IS 1) return sequence;

   if (length IS 0) flag_cardinality_error(Context, "exactly-one", "argument is empty");
   else flag_cardinality_error(Context, "exactly-one", "argument has more than one item");

   return XPathValue();
}

XPathValue XPathFunctionLibrary::function_number(const std::vector<XPathValue> &Args, const XPathContext &Context) {
   if (Args.empty()) {
      if (Context.context_node) {
         std::vector<XMLTag *> nodes = { Context.context_node };
         XPathValue node_set_value(nodes);
         return XPathValue(node_set_value.to_number());
      }
      return XPathValue(std::numeric_limits<double>::quiet_NaN());
   }
   return XPathValue(Args[0].to_number());
}

XPathValue XPathFunctionLibrary::function_sum(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   if (Args.size() != 1) return XPathValue(0.0);
   if (Args[0].type != XPathValueType::NodeSet) return XPathValue(0.0);

   const auto &nodeset = Args[0];
   double sum = 0.0;

   // Handle attribute nodes if present
   if (not nodeset.node_set_attributes.empty()) {
      for (const XMLAttrib *attr : nodeset.node_set_attributes) {
         if (attr) {
            double value = XPathValue::string_to_number(attr->Value);
            if (not std::isnan(value)) {
               sum += value;
            }
         }
      }
   }
   // Handle string values if present
   else if (not nodeset.node_set_string_values.empty()) {
      for (const std::string &str : nodeset.node_set_string_values) {
         double value = XPathValue::string_to_number(str);
         if (not std::isnan(value)) {
            sum += value;
         }
      }
   }
   // Handle regular element nodes
   else {
      for (XMLTag *node : nodeset.node_set) {
         if (node) {
            std::string node_content = XPathValue::node_string_value(node);
            double value = XPathValue::string_to_number(node_content);
            if (not std::isnan(value)) {
               sum += value;
            }
         }
      }
   }
   return XPathValue(sum);
}

XPathValue XPathFunctionLibrary::function_floor(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   if (Args.size() != 1) return XPathValue(std::numeric_limits<double>::quiet_NaN());
   double value = Args[0].to_number();
   if (std::isnan(value) or std::isinf(value)) return XPathValue(value);
   return XPathValue(std::floor(value));
}

XPathValue XPathFunctionLibrary::function_ceiling(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   if (Args.size() != 1) return XPathValue(std::numeric_limits<double>::quiet_NaN());
   double value = Args[0].to_number();
   if (std::isnan(value) or std::isinf(value)) return XPathValue(value);
   return XPathValue(std::ceil(value));
}

XPathValue XPathFunctionLibrary::function_round(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   if (Args.size() != 1) return XPathValue(std::numeric_limits<double>::quiet_NaN());
   double value = Args[0].to_number();
   if (std::isnan(value) or std::isinf(value)) return XPathValue(value);
   return XPathValue(std::round(value));
}

XPathValue XPathFunctionLibrary::function_round_half_to_even(const std::vector<XPathValue> &Args,
   const XPathContext &Context)
{
   if (Args.empty() or Args.size() > 2) return XPathValue(std::numeric_limits<double>::quiet_NaN());

   double value = Args[0].to_number();
   if (std::isnan(value) or std::isinf(value)) return XPathValue(value);

   int precision = 0;
   if (Args.size() > 1) precision = (int)Args[1].to_number();

   double scaled = value;
   double factor = 1.0;
   bool negative_precision = false;

   if (precision > 0) {
      factor = std::pow(10.0, precision);
      if (std::isnan(factor) or std::isinf(factor) or factor IS 0.0) return XPathValue(value);
      scaled = value * factor;
      if (std::isnan(scaled) or std::isinf(scaled)) return XPathValue(value);
   }
   else if (precision < 0) {
      negative_precision = true;
      factor = std::pow(10.0, -precision);
      if (std::isnan(factor) or std::isinf(factor) or factor IS 0.0) return XPathValue(value);
      scaled = value / factor;
   }

   double rounded_scaled = std::nearbyint(scaled);

   if (std::isnan(rounded_scaled) or std::isinf(rounded_scaled)) return XPathValue(rounded_scaled);

   double result = rounded_scaled;
   if (precision > 0) {
      result = rounded_scaled / factor;
   }
   else if (negative_precision) {
      result = rounded_scaled * factor;
   }

   return XPathValue(result);
}

XPathValue XPathFunctionLibrary::function_abs(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   if (Args.size() != 1) return XPathValue(std::numeric_limits<double>::quiet_NaN());

   double value = Args[0].to_number();
   if (std::isnan(value) or std::isinf(value)) return XPathValue(value);

   return XPathValue(std::fabs(value));
}

XPathValue XPathFunctionLibrary::function_min(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   if (Args.empty()) return XPathValue(std::numeric_limits<double>::quiet_NaN());

   std::vector<double> numbers;
   numbers.reserve(Args.size());

   for (const auto &arg : Args) append_numbers_from_value(arg, numbers);

   if (numbers.empty()) return XPathValue(std::numeric_limits<double>::quiet_NaN());

   double minimum = numbers[0];
   for (size_t index = 1; index < numbers.size(); ++index) {
      if (numbers[index] < minimum) minimum = numbers[index];
   }

   return XPathValue(minimum);
}

XPathValue XPathFunctionLibrary::function_max(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   if (Args.empty()) return XPathValue(std::numeric_limits<double>::quiet_NaN());

   std::vector<double> numbers;
   numbers.reserve(Args.size());

   for (const auto &arg : Args) append_numbers_from_value(arg, numbers);

   if (numbers.empty()) return XPathValue(std::numeric_limits<double>::quiet_NaN());

   double maximum = numbers[0];
   for (size_t index = 1; index < numbers.size(); ++index) {
      if (numbers[index] > maximum) maximum = numbers[index];
   }

   return XPathValue(maximum);
}

XPathValue XPathFunctionLibrary::function_avg(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   if (Args.empty()) return XPathValue(std::numeric_limits<double>::quiet_NaN());

   std::vector<double> numbers;
   numbers.reserve(Args.size());

   for (const auto &arg : Args) append_numbers_from_value(arg, numbers);

   if (numbers.empty()) return XPathValue(std::numeric_limits<double>::quiet_NaN());

   double total = 0.0;
   for (double value : numbers) total += value;

   return XPathValue(total / (double)numbers.size());
}

XPathValue XPathFunctionLibrary::function_format_date(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   if (Args.size() < 2u) return XPathValue(std::string());
   if (Args[0].is_empty()) return XPathValue();

   std::string value = Args[0].to_string();
   std::string picture = Args[1].to_string();

   if (Args.size() > 2u and not Args[2].is_empty()) {
      if (Context.expression_unsupported) *Context.expression_unsupported = true;
   }

   DateTimeComponents components;
   if (!parse_date_value(value, components)) return XPathValue(value);

   std::string formatted = format_with_picture(components, picture);
   return XPathValue(formatted);
}

XPathValue XPathFunctionLibrary::function_format_time(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   if (Args.size() < 2u) return XPathValue(std::string());
   if (Args[0].is_empty()) return XPathValue();

   std::string value = Args[0].to_string();
   std::string picture = Args[1].to_string();

   if (Args.size() > 2u and not Args[2].is_empty()) {
      if (Context.expression_unsupported) *Context.expression_unsupported = true;
   }

   DateTimeComponents components;
   if (!parse_time_value(value, components)) return XPathValue(value);

   std::string formatted = format_with_picture(components, picture);
   return XPathValue(formatted);
}

XPathValue XPathFunctionLibrary::function_format_date_time(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   if (Args.size() < 2u) return XPathValue(std::string());
   if (Args[0].is_empty()) return XPathValue();

   std::string value = Args[0].to_string();
   std::string picture = Args[1].to_string();

   if (Args.size() > 2u and not Args[2].is_empty()) {
      if (Context.expression_unsupported) *Context.expression_unsupported = true;
   }

   DateTimeComponents components;
   if (!parse_date_time_components(value, components)) return XPathValue(value);

   std::string formatted = format_with_picture(components, picture);
   return XPathValue(formatted);
}

XPathValue XPathFunctionLibrary::function_format_integer(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   if (Args.size() < 2u) return XPathValue(std::string());

   double number = Args[0].to_number();
   if (std::isnan(number) or std::isinf(number)) return XPathValue(std::string());

   if (Args.size() > 2u and not Args[2].is_empty()) {
      if (Context.expression_unsupported) *Context.expression_unsupported = true;
   }

   long long rounded = (long long)std::llround(number);
   std::string picture = Args[1].to_string();
   std::string formatted = format_integer_picture(rounded, picture);
   return XPathValue(formatted);
}

namespace {

std::chrono::system_clock::time_point current_utc_time_point()
{
   auto now = std::chrono::system_clock::now();
   return std::chrono::time_point_cast<std::chrono::seconds>(now);
}

std::tm make_utc_tm(std::chrono::system_clock::time_point TimePoint)
{
   std::time_t raw_time = std::chrono::system_clock::to_time_t(TimePoint);
   std::tm utc{};
#if defined(_WIN32)
   gmtime_s(&utc, &raw_time);
#else
   gmtime_r(&raw_time, &utc);
#endif
   return utc;
}

std::string format_utc_date(const std::tm &Tm)
{
   char buffer[32];
   std::snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d", Tm.tm_year + 1900, Tm.tm_mon + 1, Tm.tm_mday);
   return std::string(buffer);
}

std::string format_utc_time(const std::tm &Tm)
{
   char buffer[32];
   std::snprintf(buffer, sizeof(buffer), "%02d:%02d:%02d", Tm.tm_hour, Tm.tm_min, Tm.tm_sec);
   return std::string(buffer);
}

} // namespace

XPathValue XPathFunctionLibrary::function_current_date(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   auto now = current_utc_time_point();
   auto tm = make_utc_tm(now);
   return XPathValue(XPathValueType::Date, format_utc_date(tm));
}

XPathValue XPathFunctionLibrary::function_current_time(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   auto now = current_utc_time_point();
   auto tm = make_utc_tm(now);
   std::string time = format_utc_time(tm);
   time.push_back('Z');
   return XPathValue(XPathValueType::Time, std::move(time));
}

XPathValue XPathFunctionLibrary::function_current_date_time(const std::vector<XPathValue> &Args, const XPathContext &Context)
{
   auto now = current_utc_time_point();
   auto tm = make_utc_tm(now);
   std::string date = format_utc_date(tm);
   std::string time = format_utc_time(tm);
   std::string combined;
   combined.reserve(date.length() + time.length() + 2);
   combined.append(date);
   combined.push_back('T');
   combined.append(time);
   combined.push_back('Z');
   return XPathValue(XPathValueType::DateTime, std::move(combined));
}
