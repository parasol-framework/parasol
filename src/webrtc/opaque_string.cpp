#include "opaque_string.h"
#include "precis_tables.h"

#include <utf8proc.h>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace rtc::precis {
namespace {

enum class Disposition { VALID, CONTEXT, INVALID };

uint8_t property(std::span<const ucd::Range> Ranges, uint32_t Codepoint) {
   auto found = std::lower_bound(Ranges.begin(), Ranges.end(), Codepoint,
      [](const ucd::Range &Range, uint32_t Value) { return Range.last < Value; });
   return found != Ranges.end() and found->first <= Codepoint ? found->value : 0;
}

uint8_t script(uint32_t Codepoint) {
   return property(ucd::scripts, Codepoint);
}

uint8_t joining_type(uint32_t Codepoint) {
   return property(ucd::joining_types, Codepoint);
}

bool noncharacter(uint32_t Codepoint) {
   return (Codepoint >= 0xfdd0 and Codepoint <= 0xfdef) or
      ((Codepoint & 0xfffe) IS 0xfffe and Codepoint <= 0x10ffff);
}

bool old_hangul_jamo(uint32_t Codepoint) {
   return (Codepoint >= 0x1100 and Codepoint <= 0x11ff) or
      (Codepoint >= 0xa960 and Codepoint <= 0xa97f) or
      (Codepoint >= 0xd7b0 and Codepoint <= 0xd7ff);
}

bool has_compat(uint32_t Codepoint) {
   std::array<utf8proc_uint8_t, 4> encoded{};
   auto length = utf8proc_encode_char(Codepoint, encoded.data());
   if (length <= 0) return false;
   utf8proc_uint8_t *normalised = nullptr;
   auto size = utf8proc_map(encoded.data(), length, &normalised,
      utf8proc_option_t(UTF8PROC_COMPOSE | UTF8PROC_COMPAT));
   if (size < 0) { std::free(normalised); return false; }
   bool changed = size != length or not std::equal(encoded.begin(), encoded.begin() + length, normalised);
   std::free(normalised);
   return changed;
}

std::optional<Disposition> exception(uint32_t Codepoint) {
   switch (Codepoint) {
      case 0x00df: case 0x03c2: case 0x06fd: case 0x06fe: case 0x0f0b: case 0x3007:
         return Disposition::VALID;
      case 0x00b7: case 0x0375: case 0x05f3: case 0x05f4: case 0x30fb:
         return Disposition::CONTEXT;
      case 0x0640: case 0x07fa: case 0x302e: case 0x302f: case 0x3031: case 0x3032:
      case 0x3033: case 0x3034: case 0x3035: case 0x303b:
         return Disposition::INVALID;
      default: break;
   }
   if ((Codepoint >= 0x0660 and Codepoint <= 0x0669) or
       (Codepoint >= 0x06f0 and Codepoint <= 0x06f9)) return Disposition::CONTEXT;
   return std::nullopt;
}

Disposition classify(uint32_t Codepoint) {
   if (auto special = exception(Codepoint)) return *special;
   auto value = utf8proc_get_property(Codepoint);
   if (value->category IS UTF8PROC_CATEGORY_CN) return Disposition::INVALID;
   if (Codepoint >= 0x21 and Codepoint <= 0x7e) return Disposition::VALID;
   if (Codepoint IS 0x200c or Codepoint IS 0x200d) return Disposition::CONTEXT;
   if (old_hangul_jamo(Codepoint) or value->ignorable or noncharacter(Codepoint) or
       value->category IS UTF8PROC_CATEGORY_CC) return Disposition::INVALID;
   if (has_compat(Codepoint)) return Disposition::VALID;
   auto category = value->category;
   if ((category >= UTF8PROC_CATEGORY_LU and category <= UTF8PROC_CATEGORY_NO) or
       (category >= UTF8PROC_CATEGORY_PC and category <= UTF8PROC_CATEGORY_ZS)) return Disposition::VALID;
   return Disposition::INVALID;
}

bool context(std::span<const utf8proc_int32_t> Characters, size_t Index) {
   auto codepoint = uint32_t(Characters[Index]);
   auto before = Index ? uint32_t(Characters[Index - 1]) : 0;
   auto after = Index + 1 < Characters.size() ? uint32_t(Characters[Index + 1]) : 0;
   if (codepoint IS 0x00b7) return before IS 'l' and after IS 'l';
   if (codepoint IS 0x0375) return script(after) IS 1;
   if (codepoint IS 0x05f3 or codepoint IS 0x05f4) return script(before) IS 2;
   if (codepoint IS 0x30fb) {
      for (auto ch : Characters) if (script(ch) >= 3) return true;
      return false;
   }
   if (codepoint >= 0x0660 and codepoint <= 0x0669) {
      return std::none_of(Characters.begin(), Characters.end(),
         [](auto Ch) { return Ch >= 0x06f0 and Ch <= 0x06f9; });
   }
   if (codepoint >= 0x06f0 and codepoint <= 0x06f9) {
      return std::none_of(Characters.begin(), Characters.end(),
         [](auto Ch) { return Ch >= 0x0660 and Ch <= 0x0669; });
   }
   if (codepoint IS 0x200c or codepoint IS 0x200d) {
      if (Index and utf8proc_get_property(before)->combining_class IS 9) return true;
      if (codepoint IS 0x200d) return false;
      auto left = Index;
      while (left and joining_type(Characters[left - 1]) IS 4) --left;
      auto right = Index + 1;
      while (right < Characters.size() and joining_type(Characters[right]) IS 4) ++right;
      if (not left or right IS Characters.size()) return false;
      auto left_type = joining_type(Characters[left - 1]);
      auto right_type = joining_type(Characters[right]);
      return (left_type IS 1 or left_type IS 2) and (right_type IS 2 or right_type IS 3);
   }
   return false;
}

bool decode(std::string_view Input, std::vector<utf8proc_int32_t> &Characters) {
   Characters.clear();
   auto bytes = (const utf8proc_uint8_t *)Input.data();
   size_t offset = 0;
   while (offset < Input.size()) {
      utf8proc_int32_t codepoint = -1;
      auto count = utf8proc_iterate(bytes + offset, Input.size() - offset, &codepoint);
      if (count <= 0) return false;
      Characters.push_back(codepoint);
      offset += size_t(count);
   }
   return true;
}

bool valid(std::span<const utf8proc_int32_t> Characters) {
   for (size_t i = 0; i < Characters.size(); ++i) {
      auto disposition = classify(Characters[i]);
      if (disposition IS Disposition::INVALID or
          (disposition IS Disposition::CONTEXT and not context(Characters, i))) return false;
   }
   return true;
}

}

bool prepare_opaque_string(std::string_view Input, std::string &Output) {
   if (Input.empty()) return false;
   std::vector<utf8proc_int32_t> characters;
   if (not decode(Input, characters) or not valid(characters)) return false;
   std::string mapped;
   mapped.reserve(Input.size());
   for (auto codepoint : characters) {
      if (codepoint != 0x20 and utf8proc_get_property(codepoint)->category IS UTF8PROC_CATEGORY_ZS) {
         codepoint = 0x20;
      }
      std::array<utf8proc_uint8_t, 4> encoded{};
      auto count = utf8proc_encode_char(codepoint, encoded.data());
      if (count <= 0) return false;
      mapped.append((const char *)encoded.data(), size_t(count));
   }
   utf8proc_uint8_t *normalised = nullptr;
   auto count = utf8proc_map((const utf8proc_uint8_t *)mapped.data(), mapped.size(), &normalised,
      UTF8PROC_COMPOSE);
   if (count <= 0) { std::free(normalised); return false; }
   std::string result((const char *)normalised, size_t(count));
   std::free(normalised);
   if (not decode(result, characters) or not valid(characters)) return false;
   Output = std::move(result);
   return true;
}

}
