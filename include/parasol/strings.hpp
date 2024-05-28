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

[[nodiscard]] inline bool iequals(std::string_view lhs, std::string_view rhs)
{
   auto ichar_equals = [](char a, char b) {
       return std::tolower(static_cast<unsigned char>(a)) ==
              std::tolower(static_cast<unsigned char>(b));
   };

   return std::ranges::equal(lhs, rhs, ichar_equals);
}

[[nodiscard]] inline bool wildcmp(std::string_view Wildcard, std::string_view String, bool Case = false)
{
   auto Original = String;

   if (Wildcard.empty()) return true;

   std::size_t w = 0, s = 0;
   while ((w < Wildcard.size()) and (s < String.size())) {
      bool fail = false;
      if (Wildcard[w] IS '*') {
         while ((Wildcard[w] IS '*') and (w < Wildcard.size())) w++;
         if (w IS Wildcard.size()) return true; // Wildcard terminated with a '*'; rest of String will match.

         auto i = Wildcard.find_first_of("*|", w); // Count the printable characters after the '*'

         if ((i != std::string::npos) and (Wildcard[i] IS '|')) {
            // Scan to the end of the string for wildcard situation like "*.txt"
            
            auto printable = i - w;
            auto j = String.size() - s; // Number of characters left in the String
            if (j < printable) fail = true; // The string has run out of characters to cover itself for the wildcard
            else s += j - printable; // Skip everything in the second string that covers us for the '*' character
         }
         else { // Skip past the non-matching characters
            while (s < String.size()) {
               if (Case) {
                  if (Wildcard[w] IS String[s]) break;
               }
               else {
                  auto char1 = Wildcard[w]; if ((char1 >= 'A') and (char1 <= 'Z')) char1 = char1 - 'A' + 'a';
                  auto char2 = String[s]; if ((char2 >= 'A') and (char2 <= 'Z')) char2 = char2 - 'A' + 'a';
                  if (char1 IS char2) break;
               }
               s++;
            }
         }
      }
      else if (Wildcard[w] IS '?') { // Do not compare ? wildcards
         w++;
         s++;
      }
      else if ((Wildcard[w] IS '\\') and (w+1 < Wildcard.size())) { // Escape character
         w++;
         if (Case) {
            if (Wildcard[w++] != String[s++]) fail = true;
         }
         else {
            auto char1 = Wildcard[w++]; if ((char1 >= 'A') and (char1 <= 'Z')) char1 = char1 - 'A' + 'a';
            auto char2 = String[s++]; if ((char2 >= 'A') and (char2 <= 'Z')) char2 = char2 - 'A' + 'a';
            if (char1 != char2) fail = true;
         }
      }
      else if ((Wildcard[w] IS '|') and (w + 1 < Wildcard.size())) {
         w++;
         String = Original; // Restart the comparison
      }
      else {
         if (Case) {
            if (Wildcard[w++] != String[s++]) fail = true;
         }
         else {
            auto char1 = Wildcard[w++]; if ((char1 >= 'A') and (char1 <= 'Z')) char1 = char1 - 'A' + 'a';
            auto char2 = String[s++]; if ((char2 >= 'A') and (char2 <= 'Z')) char2 = char2 - 'A' + 'a';
            if (char1 != char2) fail = true;
         }
      }

      if (fail) {
         // Check for an or character, if we find one, we can restart the comparison process.

         auto or_index = Wildcard.find('|', w);
         if (or_index IS std::string::npos) return false;

         w = or_index + 1;
         String = Original;
      }
   }

   if (String.size() IS s) {
      if (Wildcard.size() IS w) return true;
      else if (Wildcard[w] IS '|') return true;
   }

   if ((w < Wildcard.size()) and (Wildcard[w] IS '*')) return true;

   return false;
}

} // namespace
