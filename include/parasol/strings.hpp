#pragma once

#include <sstream>
#include <algorithm>
#include <cctype>
#include <string_view>

namespace pf {

// USAGE: std::vector<std::string> list; pf::split(value, std::back_inserter(list));

template <class InType, class OutIt>
void split(InType Input, OutIt Output)
{
   auto begin = Input.begin();
   auto end = Input.end();
   auto current = begin;
   while (begin != end) {
      if (*begin == ',') {
         *Output++ = std::string(current, begin);
         current = ++begin;
      }
      else ++begin;
   }
   *Output++ = std::string(current, begin);
}

inline void ltrim(std::string &String, const std::string &Whitespace = " \n\r\t")
{
   const auto start = String.find_first_not_of(Whitespace);
   if ((start != std::string::npos) and (start != 0)) String.erase(0, start);
}

inline void rtrim(std::string &String, const std::string &Whitespace = " \n\r\t")
{
   const auto end = String.find_last_not_of(Whitespace);
   if ((end != std::string::npos) and (end != String.size()-1)) String.erase(end+1, String.size()-end);
}

inline void trim(std::string &String, const std::string &Whitespace = " \n\r\t")
{
   const auto start = String.find_first_not_of(Whitespace);
   if ((start != std::string::npos) and (start != 0)) String.erase(0, start);

   const auto end = String.find_last_not_of(Whitespace);
   if ((end != std::string::npos) and (end != String.size()-1)) String.erase(end+1, String.size()-end);
}

inline void camelcase(std::string &s) {
   bool raise = true;
   for (ULONG i=0; i < s.size(); i++) {
      if (raise) {
         s[i] = std::toupper(s[i]);
         raise = false;
      }
      else if (s[i] <= 0x20) raise = true;
   }
}

inline bool iequals(std::string_view lhs, std::string_view rhs)
{
   auto ichar_equals = [](char a, char b) {
       return std::tolower(static_cast<unsigned char>(a)) ==
              std::tolower(static_cast<unsigned char>(b));
   };

   return std::ranges::equal(lhs, rhs, ichar_equals);
}

} // namespace
