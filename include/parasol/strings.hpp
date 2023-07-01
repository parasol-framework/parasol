#pragma once

#include <sstream>
#include <algorithm>

namespace pf {

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

inline void ltrim(std::string &String, const std::string &Whitespace = " \t")
{
   const auto start = String.find_first_not_of(Whitespace);
   if ((start != std::string::npos) and (start != 0)) String.erase(0, start);
}

inline void rtrim(std::string &String, const std::string &Whitespace = " \t")
{
   const auto end = String.find_last_not_of(Whitespace);
   if ((end != std::string::npos) and (end != String.size()-1)) String.erase(end+1, String.size()-end);
}

inline void trim(std::string &String, const std::string &Whitespace = " \t")
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

} // namespace
