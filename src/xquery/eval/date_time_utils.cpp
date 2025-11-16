#include "date_time_utils.h"

#include "eval_detail.h"

namespace
{
   std::string normalise_timezone(std::string_view Value)
   {
      if (Value.empty()) return std::string();

      if ((Value.size() IS 1) and ((Value[0] IS 'Z') or (Value[0] IS 'z'))) {
         return std::string("Z");
      }

      if ((Value.size() IS 6) and ((Value[0] IS '+') or (Value[0] IS '-')))
      {
         bool zero_offset = (Value[1] IS '0') and (Value[2] IS '0') and (Value[4] IS '0') and (Value[5] IS '0');
         if (zero_offset) return std::string("Z");
         return std::string(Value);
      }

      return std::string(Value);
   }

   size_t locate_timezone_component(std::string_view TimePart)
   {
      for (size_t index = 0; index < TimePart.size(); ++index) {
         char marker = TimePart[index];
         if ((marker IS 'Z') or (marker IS 'z')) return index;
         if ((marker IS '+') or (marker IS '-')) return index;
      }
      return std::string_view::npos;
   }
}

bool is_valid_timezone(std::string_view Value)
{
   if (Value.empty()) return true;
   if (Value.size() IS 1) return (Value[0] IS 'Z') or (Value[0] IS 'z');

   if ((Value.size() IS 6) and ((Value[0] IS '+') or (Value[0] IS '-')))
   {
      if ((Value[3] != ':') or (Value[4] < '0') or (Value[4] > '9') or (Value[5] < '0') or (Value[5] > '9')) return false;
      if ((Value[1] < '0') or (Value[1] > '9') or (Value[2] < '0') or (Value[2] > '9')) return false;

      int hour = (Value[1] - '0') * 10 + (Value[2] - '0');
      int minute = (Value[4] - '0') * 10 + (Value[5] - '0');

      if (hour > 14) return false;
      if (minute >= 60) return false;
      if ((hour IS 14) and (minute != 0)) return false;

      return true;
   }

   return false;
}

bool parse_xs_date_components(std::string_view Value, long long &Year, int &Month, int &Day, size_t &NextIndex)
{
   if (Value.empty()) return false;

   size_t index = 0;
   bool negative = false;

   if ((Value[index] IS '+') or (Value[index] IS '-'))
   {
      negative = Value[index] IS '-';
      index++;
      if (index >= Value.size()) return false;
   }

   size_t year_start = index;
   while ((index < Value.size()) and (Value[index] >= '0') and (Value[index] <= '9')) index++;
   if ((index - year_start) < 4) return false;

   Year = 0;
   for (size_t pos = year_start; pos < index; ++pos) {
      Year = Year * 10 + (Value[pos] - '0');
      if (Year > 9999) break;
   }

   if (negative) Year = -Year;

   if ((index + 1 >= Value.size()) or (Value[index] != '-') or (Value[index + 1] < '0') or (Value[index + 1] > '9') or
       (Value[index + 2] < '0') or (Value[index + 2] > '9')) return false;

   Month = (Value[index + 1] - '0') * 10 + (Value[index + 2] - '0');
   index += 3;
   if ((Month < 1) or (Month > 12)) return false;

   if ((index + 1 >= Value.size()) or (Value[index] != '-') or (Value[index + 1] < '0') or (Value[index + 1] > '9') or
       (Value[index + 2] < '0') or (Value[index + 2] > '9')) return false;

   Day = (Value[index + 1] - '0') * 10 + (Value[index + 2] - '0');
   index += 3;

   static const int days_in_month[] = { 0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
   int max_day = days_in_month[Month];

   bool leap = ((Year % 4) IS 0) and (((Year % 100) != 0) or ((Year % 400) IS 0));
   if (leap and (Month IS 2)) max_day = 29;

   if ((Day < 1) or (Day > max_day)) return false;

   NextIndex = index;
   return true;
}

bool is_valid_xs_date_no_timezone(std::string_view Value)
{
   long long year = 0;
   int month = 0;
   int day = 0;
   size_t next_index = 0;
   return parse_xs_date_components(Value, year, month, day, next_index) and (next_index IS Value.size());
}

bool is_valid_xs_date(std::string_view Value)
{
   long long year = 0;
   int month = 0;
   int day = 0;
   size_t next_index = 0;
   if (not parse_xs_date_components(Value, year, month, day, next_index)) return false;

   std::string_view timezone = Value.substr(next_index);
   return is_valid_timezone(timezone);
}

bool is_valid_xs_time(std::string_view Value)
{
   if (Value.empty()) return false;

   size_t index = 0;
   int hour = 0;

   if ((index + 1 >= Value.size()) or (Value[index] < '0') or (Value[index] > '9') or (Value[index + 1] < '0') or
       (Value[index + 1] > '9')) return false;
   hour = (Value[index] - '0') * 10 + (Value[index + 1] - '0');
   index += 2;
   if (hour > 24) return false;

   if ((index >= Value.size()) or (Value[index] != ':')) return false;
   index++;

   if ((index + 1 >= Value.size()) or (Value[index] < '0') or (Value[index] > '9') or (Value[index + 1] < '0') or
       (Value[index + 1] > '9')) return false;
   int minute = (Value[index] - '0') * 10 + (Value[index + 1] - '0');
   index += 2;
   if (minute >= 60) return false;

   if ((index >= Value.size()) or (Value[index] != ':')) return false;
   index++;

   if ((index + 1 >= Value.size()) or (Value[index] < '0') or (Value[index] > '9') or (Value[index + 1] < '0') or
       (Value[index + 1] > '9')) return false;
   int second = (Value[index] - '0') * 10 + (Value[index + 1] - '0');
   index += 2;
   if (second > 60) return false;

   if ((hour IS 24) and ((minute != 0) or (second != 0))) return false;

   if ((index < Value.size()) and (Value[index] IS '.'))
   {
      index++;
      size_t fraction_start = index;
      while ((index < Value.size()) and (Value[index] >= '0') and (Value[index] <= '9')) index++;
      if (index IS fraction_start) return false;
   }

   std::string_view timezone = Value.substr(index);
   return is_valid_timezone(timezone);
}

bool is_valid_xs_datetime(std::string_view Value)
{
   size_t position = Value.find('T');
   if (position IS std::string_view::npos) return false;

   std::string_view date_part = Value.substr(0, position);
   std::string_view time_part = Value.substr(position + 1);
   if (time_part.empty()) return false;

   if (not is_valid_xs_date_no_timezone(date_part)) return false;
   return is_valid_xs_time(time_part);
}

std::optional<std::string> extract_date_from_datetime(std::string_view Value)
{
   if (not is_valid_xs_datetime(Value)) return std::nullopt;

   size_t position = Value.find('T');
   if (position IS std::string_view::npos) return std::nullopt;

   std::string_view date_part = Value.substr(0, position);
   std::string_view time_part = Value.substr(position + 1);
   if (time_part.empty()) return std::nullopt;

   size_t timezone_index = locate_timezone_component(time_part);
   std::string result(date_part);

   if (timezone_index != std::string_view::npos) {
      std::string_view timezone = time_part.substr(timezone_index);
      result.append(normalise_timezone(timezone));
   }

   return result;
}

std::optional<std::string> extract_time_from_datetime(std::string_view Value)
{
   if (not is_valid_xs_datetime(Value)) return std::nullopt;

   size_t position = Value.find('T');
   if (position IS std::string_view::npos) return std::nullopt;

   std::string_view time_part = Value.substr(position + 1);
   if (time_part.empty()) return std::nullopt;

   std::string result(time_part);

   size_t timezone_index = locate_timezone_component(time_part);
   if (timezone_index != std::string_view::npos) {
      std::string_view timezone = time_part.substr(timezone_index);
      result.erase(timezone_index);
      auto normalised = normalise_timezone(timezone);
      if (!normalised.empty()) result.append(normalised);
   }

   return result;
}

std::optional<std::string> canonicalise_xs_date(std::string_view Value)
{
   auto trimmed = trim_view(Value);
   if (trimmed.empty()) return std::nullopt;

   long long year = 0;
   int month = 0;
   int day = 0;
   size_t next_index = 0;

   if (not parse_xs_date_components(trimmed, year, month, day, next_index)) return std::nullopt;

   std::string result(trimmed.substr(0, next_index));
   auto timezone = trimmed.substr(next_index);
   if (!is_valid_timezone(timezone)) return std::nullopt;

   auto normalised = normalise_timezone(timezone);
   if (!normalised.empty()) result.append(normalised);
   return result;
}

std::optional<std::string> canonicalise_xs_time(std::string_view Value)
{
   auto trimmed = trim_view(Value);
   if (trimmed.empty()) return std::nullopt;
   if (not is_valid_xs_time(trimmed)) return std::nullopt;

   size_t timezone_index = locate_timezone_component(trimmed);
   if (timezone_index IS std::string_view::npos) return std::string(trimmed);

   std::string result(trimmed.substr(0, timezone_index));
   auto timezone = trimmed.substr(timezone_index);
   auto normalised = normalise_timezone(timezone);
   if (!normalised.empty()) result.append(normalised);
   return result;
}

std::optional<std::string> canonicalise_xs_datetime(std::string_view Value)
{
   auto trimmed = trim_view(Value);
   if (trimmed.empty()) return std::nullopt;
   if (not is_valid_xs_datetime(trimmed)) return std::nullopt;

   size_t position = trimmed.find('T');
   if (position IS std::string_view::npos) return std::nullopt;

   std::string result(trimmed);
   std::string_view time_part = trimmed.substr(position + 1);
   size_t timezone_index = locate_timezone_component(time_part);
   if (timezone_index IS std::string_view::npos) return result;

   size_t tz_position = position + 1 + timezone_index;
   auto timezone = trimmed.substr(tz_position);
   auto normalised = normalise_timezone(timezone);

   result.erase(tz_position);
   if (!normalised.empty()) result.append(normalised);
   return result;
}
