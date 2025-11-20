/*
** Character types.
** Donated to the public domain.
*/

#pragma once

#include "lj_def.h"

inline constexpr uint8_t LJ_CHAR_CNTRL = 0x01;
inline constexpr uint8_t LJ_CHAR_SPACE = 0x02;
inline constexpr uint8_t LJ_CHAR_PUNCT = 0x04;
inline constexpr uint8_t LJ_CHAR_DIGIT = 0x08;
inline constexpr uint8_t LJ_CHAR_XDIGIT = 0x10;
inline constexpr uint8_t LJ_CHAR_UPPER = 0x20;
inline constexpr uint8_t LJ_CHAR_LOWER = 0x40;
inline constexpr uint8_t LJ_CHAR_IDENT = 0x80;
inline constexpr uint8_t LJ_CHAR_ALPHA = (LJ_CHAR_LOWER | LJ_CHAR_UPPER);
inline constexpr uint8_t LJ_CHAR_ALNUM = (LJ_CHAR_ALPHA | LJ_CHAR_DIGIT);
inline constexpr uint8_t LJ_CHAR_GRAPH = (LJ_CHAR_ALNUM | LJ_CHAR_PUNCT);

extern const uint8_t lj_char_bits[257];

// Only pass -1 or 0..255 to these functions. Never pass a signed char!
[[nodiscard]] inline uint8_t lj_char_isa(int c, uint8_t t) noexcept {
   return (lj_char_bits+1)[c] & t;
}

[[nodiscard]] inline bool lj_char_iscntrl(int c) noexcept {
   return lj_char_isa(c, LJ_CHAR_CNTRL) != 0;
}

[[nodiscard]] inline bool lj_char_isspace(int c) noexcept {
   return lj_char_isa(c, LJ_CHAR_SPACE) != 0;
}

[[nodiscard]] inline bool lj_char_ispunct(int c) noexcept {
   return lj_char_isa(c, LJ_CHAR_PUNCT) != 0;
}

[[nodiscard]] inline bool lj_char_isdigit(int c) noexcept {
   return lj_char_isa(c, LJ_CHAR_DIGIT) != 0;
}

[[nodiscard]] inline bool lj_char_isxdigit(int c) noexcept {
   return lj_char_isa(c, LJ_CHAR_XDIGIT) != 0;
}

[[nodiscard]] inline bool lj_char_isupper(int c) noexcept {
   return lj_char_isa(c, LJ_CHAR_UPPER) != 0;
}

[[nodiscard]] inline bool lj_char_islower(int c) noexcept {
   return lj_char_isa(c, LJ_CHAR_LOWER) != 0;
}

[[nodiscard]] inline bool lj_char_isident(int c) noexcept {
   return lj_char_isa(c, LJ_CHAR_IDENT) != 0;
}

[[nodiscard]] inline bool lj_char_isalpha(int c) noexcept {
   return lj_char_isa(c, LJ_CHAR_ALPHA) != 0;
}

[[nodiscard]] inline bool lj_char_isalnum(int c) noexcept {
   return lj_char_isa(c, LJ_CHAR_ALNUM) != 0;
}

[[nodiscard]] inline bool lj_char_isgraph(int c) noexcept {
   return lj_char_isa(c, LJ_CHAR_GRAPH) != 0;
}

[[nodiscard]] inline int lj_char_toupper(int c) noexcept {
   return c - int(lj_char_islower(c) >> 1);
}

[[nodiscard]] inline int lj_char_tolower(int c) noexcept {
   return c + int(lj_char_isupper(c));
}
