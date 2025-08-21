#pragma once

#include <sstream>
#include <algorithm>
#include <cctype>
#include <string_view>
#include <charconv>

namespace pf {

// USAGE: std::vector<std::string> list; pf::split(value, std::back_inserter(list));

template <class InType, class OutIt>
void split(InType Input, OutIt Output, char Sep = ',') noexcept
{
   auto begin = Input.begin();
   auto end = Input.end();
   auto current = begin;
   while (begin != end) {
      if (*begin IS Sep) {
         *Output++ = std::string(current, begin);
         current = ++begin;
      }
      else ++begin;
   }
   *Output++ = std::string(current, begin);
}

inline void ltrim(std::string_view &String, const std::string &Whitespace = " \n\r\t") noexcept
{
   const auto start = String.find_first_not_of(Whitespace);
   if (start != std::string::npos) String.remove_prefix(start);
}

inline void ltrim(std::string &String, const std::string &Whitespace = " \n\r\t") noexcept
{
   const auto start = String.find_first_not_of(Whitespace);
   if (start != std::string::npos) String.erase(0, start);
}

inline void rtrim(std::string &String, const std::string &Whitespace = " \n\r\t") noexcept
{
   const auto end = String.find_last_not_of(Whitespace);
   if (end != std::string::npos) String.erase(end + 1);
}

inline void trim(std::string &String, const std::string &Whitespace = " \n\r\t") noexcept
{
   ltrim(String, Whitespace);
   rtrim(String, Whitespace);
}

inline void camelcase(std::string &s) noexcept {
   bool raise = true;
   for (auto &ch : s) {
      if (raise) {
         ch = std::toupper(ch);
         raise = false;
      }
      else if (unsigned(ch) <= 0x20) raise = true;
   }
}

// Case-insensitive string comparison, both of which must be the same length.

[[nodiscard]] inline bool iequals(const std::string_view lhs, const std::string_view rhs) noexcept
{
   if (lhs.size() != rhs.size()) return false;
   return std::equal(lhs.begin(), lhs.end(), rhs.begin(), rhs.end(), [](char a, char b) {
       return std::tolower(static_cast<unsigned char>(a)) IS std::tolower(static_cast<unsigned char>(b));
   });
}

[[nodiscard]] inline bool wildcmp(const std::string_view Wildcard, std::string_view String, bool Case = false) noexcept
{
   auto Original = String;

   if (Wildcard.empty()) return true;

   std::size_t w = 0, s = 0;
   while ((w < Wildcard.size()) and (s < String.size())) {
      bool fail = false;
      if (Wildcard[w] IS '*') {
         while (w < Wildcard.size() and Wildcard[w] IS '*') w++;
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
                  auto char1 = std::tolower(static_cast<unsigned char>(Wildcard[w]));
                  auto char2 = std::tolower(static_cast<unsigned char>(String[s]));
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
            auto char1 = std::tolower(static_cast<unsigned char>(Wildcard[w++]));
            auto char2 = std::tolower(static_cast<unsigned char>(String[s++]));
            if (char1 != char2) fail = true;
         }
      }
      else if ((Wildcard[w] IS '|') and (w + 1 < Wildcard.size())) {
         w++;
         String = Original; // Restart the comparison
         s = 0;
      }
      else {
         if (Case) {
            if (Wildcard[w++] != String[s++]) fail = true;
         }
         else {
            auto char1 = std::tolower(static_cast<unsigned char>(Wildcard[w++]));
            auto char2 = std::tolower(static_cast<unsigned char>(String[s++]));
            if (char1 != char2) fail = true;
         }
      }

      if (fail) {
         // Check for an or character, if we find one, we can restart the comparison process.

         auto or_index = Wildcard.find('|', w);
         if (or_index IS std::string::npos) return false;

         w = or_index + 1;
         String = Original;
         s = 0;
      }
   }

   if (String.size() IS s) {
      if (w IS Wildcard.size() or Wildcard[w] IS '|') return true;
   }

   if ((w < Wildcard.size()) and (Wildcard[w] IS '*')) return true;

   return false;
}

// A case insensitive alternative to std::string_view.starts_with()

[[nodiscard]] inline bool startswith(const std::string_view StringA, const std::string_view StringB) noexcept
{
   if (StringA.size() > StringB.size()) return false;
   return std::equal(StringA.begin(), StringA.end(), StringB.begin(), [](char a, char b) {
       return std::tolower(static_cast<unsigned char>(a)) IS std::tolower(static_cast<unsigned char>(b));
   });
}

[[nodiscard]] inline bool startswith(const std::string_view StringA, CSTRING StringB) noexcept
{
   for (std::size_t i = 0; i < StringA.size(); i++) {
      if (std::tolower(StringA[i]) != std::tolower(StringB[i])) return false;
   }
   return true;
}

// Inline C++ implementations of the StrHash() function

[[nodiscard]] constexpr inline uint32_t strhash(const std::string_view String) noexcept
{
   uint32_t hash = 5381;
   std::for_each(String.begin(), String.end(), [&hash](char a) {
      hash = (hash<<5) + hash + a;
   });
   return hash;
}

[[nodiscard]] constexpr inline uint32_t strihash(const std::string_view String) noexcept
{
   uint32_t hash = 5381;
   std::for_each(String.begin(), String.end(), [&hash](char c) {
      if ((c >= 'A') and (c <= 'Z')) c = c - 'A' + 'a';
      hash = (hash<<5) + hash + c;
   });
   return hash;
}

template <class T> inline LONG strcopy(T &&Source, STRING Dest, LONG Length = 0x7fffffff) noexcept
{
   auto src = to_cstring(Source);
   if ((Length > 0) and (src) and (Dest)) {
      LONG i = 0;
      while (*src) {
         if (i IS Length) {
            Dest[i-1] = 0;
            return i;
         }
         Dest[i++] = *src++;
      }

      Dest[i] = 0;
      return i;
   }
   else return 0;
}

// Case-sensitive keyword search

[[nodiscard]] inline LONG strsearch(const std::string_view Keyword, CSTRING String) noexcept
{
   size_t i;
   size_t pos = 0;
   while (String[pos]) {
      for (i=0; i < Keyword.size(); i++) if (String[pos+i] != Keyword[i]) break;
      if (i IS Keyword.size()) return pos;
      for (++pos; (String[pos] & 0xc0) IS 0x80; pos++);
   }

   return -1;
}

// Case-insensitive keyword search

[[nodiscard]] inline LONG strisearch(const std::string_view Keyword, CSTRING String) noexcept
{
   size_t i;
   size_t pos = 0;
   while (String[pos]) {
      for (i=0; i < Keyword.size(); i++) if (std::toupper(String[pos+i]) != std::toupper(Keyword[i])) break;
      if (i IS Keyword.size()) return pos;
      for (++pos; (String[pos] & 0xc0) IS 0x80; pos++);
   }

   return -1;
}

[[nodiscard]] inline STRING strclone(const std::string_view String) noexcept
{
   STRING newstr;
   if (AllocMemory(String.size()+1, MEM::STRING, (APTR *)&newstr, NULL) IS ERR::Okay) {
      copymem(String.data(), newstr, String.size()+1);
      return newstr;
   }
   else return NULL;
}

// std::string_view conversion to numeric type.  Returns zero on error.
// Leading whitespace is not ignored, unlike strtol() and strtod()

template <class T> [[nodiscard]] T svtonum(const std::string_view String) noexcept {
   T val;
   auto [ v, error ] = std::from_chars(String.data(), String.data() + String.size(), val);
   if (error IS std::errc()) return val;
   else return 0;
}

// Speed efficient way of setting a string field that is managed with AllocMemory().

inline ERR set_string_field(const std::string_view Source, STRING &Dest)
{
   MemInfo info;
   if (auto error = MemoryIDInfo(GetMemoryID(Dest), &info, sizeof(info)); error IS ERR::Okay) {
      if (Source.size()+1 < info.Size) {
         copymem(Source.data(), Dest, Source.size());
         Dest[Source.size()] = 0;
         return ERR::Okay;
      }
      else {
         FreeResource(GetMemoryID(Dest));
         if (AllocMemory(Source.size() + 1, MEM::STRING|MEM::NO_CLEAR, (APTR *)&Dest, NULL) IS ERR::Okay) {
            copymem(Source.data(), Dest, Source.size());
            Dest[Source.size()] = 0;
            return ERR::Okay;
         }
         else return ERR::AllocMemory;
      }
   }
   else return error;
}

} // namespace
