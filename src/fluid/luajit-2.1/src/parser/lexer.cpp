// Lexical analyser.
//
// Copyright (C) 2025 Paul Manias

#define LUA_CORE

#include <array>
#include <cctype>
#include <cmath>
#include <concepts>
#include <string_view>

#include "lj_obj.h"
#include "lj_gc.h"
#include "lj_err.h"
#include "lj_buf.h"
#include "lj_str.h"

#if LJ_HASFFI
#include "lj_tab.h"
#include "lj_ctype.h"
#include "lj_cdata.h"
#include "lualib.h"
#endif

#include "lj_state.h"
#include "lexer.h"
#include "parser.h"
#include "parser_context.h"
#include "../../defs.h"
#include "lj_char.h"
#include "lj_strscan.h"
#include "lj_strfmt.h"
#include "../bytecode/lj_bcdump.h"

//********************************************************************************************************************
// Compile-time token name generation using TOKEN_DEFINITIONS from lexer.h

namespace {
   // Generate token symbol array from TOKEN_DEFINITIONS at compile time
   constexpr auto generate_token_symbols() noexcept {
      std::array<const char*, TOKEN_DEFINITIONS.size()> symbols{};
      // Note: We store pointers to the symbol data. Since TOKEN_DEFINITIONS contains
      // string_view literals, the underlying data has static storage duration.
      for (size_t i = 0; i < TOKEN_DEFINITIONS.size(); ++i) {
         symbols[i] = TOKEN_DEFINITIONS[i].symbol.data();
      }
      return symbols;
   }

   constexpr auto token_names = generate_token_symbols();
} // anonymous namespace

//********************************************************************************************************************

// Character handling concepts and utilities

namespace {
   constexpr LexChar LEX_EOF = -1;

   constexpr bool lex_iseol(LexChar c) noexcept {
      return c IS '\n' or c IS '\r';
   }
} // anonymous namespace

//********************************************************************************************************************
// Character stream operations

// Peek at character at offset from current position (0 = next unread char)
[[nodiscard]] LexChar LexState::peek(size_t Offset) const noexcept
{
   size_t idx = this->pos + Offset;
   if (idx >= this->source.size()) return LEX_EOF;
   return LexChar(uint8_t(this->source[idx]));
}

// Peek at next character without advancing
[[nodiscard]] LexChar LexState::peek_next() const noexcept
{
   return this->peek(0);
}

static LJ_AINLINE LexChar lex_next(LexState *State) noexcept
{
   if (State->pos >= State->source.size()) {
      State->c = LEX_EOF;
      return LEX_EOF;
   }
   State->current_offset = State->pos;
   State->c = LexChar(uint8_t(State->source[State->pos++]));
   return State->c;
}

static LJ_AINLINE void lex_save(LexState *State, LexChar c) noexcept
{
   lj_buf_putb(&State->sb, c);
}

static LJ_AINLINE LexChar lex_savenext(LexState *State) noexcept
{
   lex_save(State, State->c);
   return lex_next(State);
}

//********************************************************************************************************************
// Line break handling

static void lex_newline(LexState *State)
{
   LexChar old = State->c;
   State->assert_condition(lex_iseol(State->c), "bad usage");
   lex_next(State);  // Skip "\n" or "\r".
   if (lex_iseol(State->c) and State->c != old) lex_next(State);  // Skip "\n\r" or "\r\n".

   if (uint32_t(++State->linenumber) >= LJ_MAX_LINE)
      lj_lex_error(State, State->tok, ErrMsg::XLINES);

   State->line_start_offset = State->current_offset;
}

//********************************************************************************************************************
// Numeric literal scanning

namespace {
   constexpr uint32_t scan_options() noexcept {
      return (LJ_DUALNUM ? STRSCAN_OPT_TOINT : STRSCAN_OPT_TONUM) |
             (LJ_HASFFI ? (STRSCAN_OPT_LL | STRSCAN_OPT_IMAG) : 0);
   }

   constexpr bool is_exponent_sign(LexChar c, LexChar exponent) noexcept {
      return (c IS '-' or c IS '+') and ((exponent | 0x20) IS 'e' or (exponent | 0x20) IS 'p');
   }

   constexpr bool is_number_char(LexChar c, LexChar exponent) noexcept {
      return lj_char_isident(c) or c IS '.' or is_exponent_sign(c, exponent);
   }
} // anonymous namespace

//********************************************************************************************************************
// Parse numeric literal for lex_scan()

static void lex_number(LexState *State, TValue* tv)
{
   State->assert_condition(isdigit(State->c), "bad usage");

   // Determine if hexadecimal (uses 'p' exponent) or decimal (uses 'e' exponent)
   LexChar c = State->c;
   LexChar exponent = 'e';
   if (c IS '0' and (lex_savenext(State) | 0x20) IS 'x') exponent = 'p';

   // Scan all number characters.
   // Special case: Stop before '..' to allow range literals like {1..5}
   while (is_number_char(State->c, c)) {
      // If we see '.', check if next character is also '.' (range operator)
      if (State->c IS '.' and State->peek_next() IS '.') {
         break;  // Don't consume '.', let parser handle '..'
      }
      c = State->c;
      lex_savenext(State);
   }
   lex_save(State, '\0');
   auto fmt = lj_strscan_scan((const uint8_t*)State->sb.b, sbuflen(&State->sb) - 1, tv, scan_options());

   if (LJ_DUALNUM and fmt IS STRSCAN_INT) {
      setitype(tv, LJ_TISNUM);
      return;
   }

   if (fmt IS STRSCAN_NUM) return; // Already in correct format

#if LJ_HASFFI
   if (fmt != STRSCAN_ERROR) {
      State->assert_condition(fmt IS STRSCAN_I64 or fmt IS STRSCAN_U64 or fmt IS STRSCAN_IMAG,
         "unexpected number format %d", fmt);

      lua_State* L = State->L;
      ctype_loadffi(L);

      GCcdata* cd;
      if (fmt IS STRSCAN_IMAG) {
         cd = lj_cdata_new_(L, CTID_COMPLEX_DOUBLE, 2 * sizeof(double));
         auto* complex_ptr = (double*)cdataptr(cd);
         complex_ptr[0] = 0.0;
         complex_ptr[1] = numV(tv);
      }
      else {
         cd = lj_cdata_new_(L, fmt IS STRSCAN_I64 ? CTID_INT64 : CTID_UINT64, 8);
         *(uint64_t*)(cdataptr(cd)) = tv->u64;
      }
      lj_parse_keepcdata(State, tv, cd);
      return;
   }
#endif

   State->assert_condition(fmt IS STRSCAN_ERROR, "unexpected number format %d", fmt);
   lj_lex_error(State, TK_number, ErrMsg::XNUMBER);
}

//********************************************************************************************************************
// Long bracket delimiter parsing

static int lex_skipeq(LexState *State)
{
   int count = 0;
   LexChar s = State->c;
   State->assert_condition(s IS '[' or s IS ']', "bad usage");

   constexpr int MAX_BRACKET_LEVEL = 0x20000000;
   while (lex_savenext(State) IS '=' and count < MAX_BRACKET_LEVEL) count++;

   return (State->c IS s) ? count : (-count) - 1;
}

//********************************************************************************************************************
// Long string and long comment parsing

static void lex_longstring(LexState *State, TValue* tv, int sep)
{
   lex_savenext(State);  // Skip second '['.
   if (lex_iseol(State->c)) lex_newline(State); // Skip initial newline.

   while (true) {
      switch (State->c) {
         case LEX_EOF:
            lj_lex_error(State, TK_eof, tv ? ErrMsg::XLSTR : ErrMsg::XLCOM);
            break;

         case ']':
            if (lex_skipeq(State) IS sep) {
               lex_savenext(State);  // Skip second ']'.
               goto endloop;
            }
            break;

         case '\n':
         case '\r':
            lex_save(State, '\n');
            lex_newline(State);
            if (not tv) lj_buf_reset(&State->sb);  // Don't waste space for comments.
            break;

         default:
            lex_savenext(State);
            break;
      }
   }

endloop:
   if (tv) {
      auto offset = 2 + MSize(sep);
      auto length = sbuflen(&State->sb) - 2 * offset;
      GCstr* str = State->keepstr(std::string_view(State->sb.b + offset, length));
      setstrV(State->L, tv, str);
   }
}

//********************************************************************************************************************
// String literal escape sequence handling

namespace {
   constexpr uint32_t MAX_UNICODE = 0x110000;
   constexpr uint32_t SURROGATE_START = 0xd800;
   constexpr uint32_t SURROGATE_END = 0xe000;

   // Parse hexadecimal escape sequence \xXX
   LexChar parse_hex_escape(LexState *State) {
      LexChar c = (lex_next(State) & 15u) << 4;
      if (not isdigit(State->c)) {
         if (not isxdigit(State->c)) return -1;
         c += 9 << 4;
      }
      c += (lex_next(State) & 15u);
      if (not isdigit(State->c)) {
         if (not isxdigit(State->c)) return -1;
         c += 9;
      }
      return c;
   }

   // Parse Unicode escape sequence \u{...} and emit UTF-8
   bool parse_unicode_escape(LexState *State) {
      if (lex_next(State) != '{') return false;
      lex_next(State);

      uint32_t c = 0;
      do {
         c = (c << 4) | uint32_t(State->c & 15u);
         if (not isdigit(State->c)) {
            if (not isxdigit(State->c)) return false;
            c += 9;
         }
         if (c >= MAX_UNICODE) return false;
      } while (lex_next(State) != '}');

      // Emit UTF-8 encoded character
      if (c < 0x80) {
         lex_save(State, LexChar(c));
      }
      else if (c < 0x800) {
         lex_save(State, LexChar(0xc0 | (c >> 6)));
         lex_save(State, LexChar(0x80 | (c & 0x3f)));
      }
      else if (c < 0x10000) {
         if (c >= SURROGATE_START and c < SURROGATE_END) return false;  // No surrogates
         lex_save(State, LexChar(0xe0 | (c >> 12)));
         lex_save(State, LexChar(0x80 | ((c >> 6) & 0x3f)));
         lex_save(State, LexChar(0x80 | (c & 0x3f)));
      }
      else {
         lex_save(State, LexChar(0xf0 | (c >> 18)));
         lex_save(State, LexChar(0x80 | ((c >> 12) & 0x3f)));
         lex_save(State, LexChar(0x80 | ((c >> 6) & 0x3f)));
         lex_save(State, LexChar(0x80 | (c & 0x3f)));
      }
      return true;
   }

   // Parse decimal escape sequence \ddd
   LexChar parse_decimal_escape(LexState *State, LexChar first_digit) {
      LexChar c = first_digit - '0';
      if (isdigit(lex_next(State))) {
         c = c * 10 + (State->c - '0');
         if (isdigit(lex_next(State))) {
            c = c * 10 + (State->c - '0');
            if (c > 255) return -1;
            lex_next(State);
         }
      }
      return c;
   }
} // anonymous namespace

static void lex_string(LexState *State, TValue* tv)
{
   LexChar delim = State->c;  // Delimiter is '\'' or '"'.
   lex_savenext(State);

   while (State->c != delim) {
      switch (State->c) {
      case LEX_EOF:
         lj_lex_error(State, TK_eof, ErrMsg::XSTR);
         continue;

      case '\n':
      case '\r':
         lj_lex_error(State, TK_string, ErrMsg::XSTR);
         continue;

      case '\\': {
         LexChar c = lex_next(State);  // Skip the '\'.
         switch (c) {
         case 'a': c = '\a'; break;
         case 'b': c = '\b'; break;
         case 'f': c = '\f'; break;
         case 'n': c = '\n'; break;
         case 'r': c = '\r'; break;
         case 't': c = '\t'; break;
         case 'v': c = '\v'; break;
         case '\\': case '\"': case '\'': break;

         case 'x':
            c = parse_hex_escape(State);
            if (c < 0) goto err_xesc;
            break;

         case 'u':
            if (not parse_unicode_escape(State)) goto err_xesc;
            lex_next(State);
            continue;

         case 'z':  // Skip whitespace
            lex_next(State);
            while (isspace(State->c)) {
               if (lex_iseol(State->c)) lex_newline(State);
               else lex_next(State);
            }
            continue;

         case '\n':
         case '\r':
            lex_save(State, '\n');
            lex_newline(State);
            continue;

         case LEX_EOF:
            continue;

         default:
            if (not isdigit(c)) goto err_xesc;
            c = parse_decimal_escape(State, c);
            if (c < 0) {
            err_xesc:
               lj_lex_error(State, TK_string, ErrMsg::XESC);
            }
            lex_save(State, c);
            continue;
         }

         lex_save(State, c);
         lex_next(State);
         continue;
      }

      default:
         lex_savenext(State);
         break;
      }
   }

   lex_savenext(State);  // Skip trailing delimiter.
   auto str_content = std::string_view(State->sb.b + 1, sbuflen(&State->sb) - 2);
   setstrV(State->L, tv, State->keepstr(str_content));
}

//********************************************************************************************************************
// Token scanner

namespace {
   // Helper for compound assignment operators
   constexpr LexToken check_compound(LexState *State, LexToken compound_tok) noexcept {
      if (State->c IS '=') {
         lex_next(State);
         return compound_tok;
      }
      return 0;
   }

   // Helper for two-character operators
   constexpr LexToken check_double(LexState *State, LexChar match, LexToken double_tok) noexcept {
      if (State->c IS match) {
         lex_next(State);
         return double_tok;
      }
      return 0;
   }
} // anonymous namespace

//********************************************************************************************************************
// Unicode operator recognition

// Shared lookup function that matches Unicode operator sequences without consuming characters.
// Returns the token type if matched, or 0 if no match. Also returns the byte length via output parameter.

static LexToken match_unicode_operator(LexState *State, int &ByteLength) noexcept
{
   ByteLength = 0;

   // UTF-8 sequences starting with 0xC2 (Latin-1 Supplement)
   if (State->c IS 0xC2) {
      LexChar second = State->peek_next();
      if (second IS 0xAB) {
         ByteLength = 2;
         return TK_shl;     // «
      }
      else if (second IS 0xBB) {
         ByteLength = 2;
         return TK_shr;     // »
      }
   }

   // UTF-8 sequences starting with 0xC3 (Latin-1 Supplement continued)
   if (State->c IS 0xC3) {
      LexChar second = State->peek_next();
      if (second IS 0x97) {
         ByteLength = 2;
         return '*';      // ×
      }
      else if (second IS 0xB7) {
         ByteLength = 2;
         return '/';      // ÷
      }
   }

   // UTF-8 sequences starting with 0xE2 (3-byte sequences)
   if (State->c IS 0xE2) {
      LexChar second = State->peek(0);
      LexChar third = State->peek(1);

      if (second IS 0x80) {
         if (third IS 0xA5) {
            ByteLength = 3;
            return TK_concat;     // ‥
         }
         else if (third IS 0xA6) {
            ByteLength = 3;
            return TK_dots;       // …
         }
      }

      if (second IS 0x81 and third IS 0x87) {
         ByteLength = 3;
         return TK_if_empty;  // ⁇
      }

      if (second IS 0x89) {
         if (third IS 0xA0) {
            ByteLength = 3;
            return TK_ne;       // ≠
         }
         else if (third IS 0xA4) {
            ByteLength = 3;
            return TK_le;        // ≤
         }
         else if (third IS 0xA5) {
            ByteLength = 3;
            return TK_ge;        // ≥
         }
      }

      if (second IS 0x96 and third IS 0xB7) {
         ByteLength = 3;
         return TK_ternary_sep;  // ▷
      }
      else if (second IS 0xA7 and third IS 0xBA) {
         ByteLength = 3;
         return TK_plusplus;     // ⧺
      }
   }

   return 0;
}

inline bool is_unicode_operator_start(LexState *State) noexcept
{
   // Checks for UTF-8 sequences that represent operators (peek mode, doesn't consume characters).
   int byte_length;
   return match_unicode_operator(State, byte_length) != 0;
}

static LexToken lex_unicode_operator(LexState *State) noexcept
{
   // Lexes a Unicode operator and consumes the appropriate number of bytes.
   int byte_length;
   LexToken tok = match_unicode_operator(State, byte_length);

   if (tok) {
      State->mark_token_start();
      for (int i = 0; i < byte_length; ++i) {
         lex_next(State);
      }
      return tok;
   }

   return 0;
}

//********************************************************************************************************************
// Token scanner, main entry point

static LexToken lex_scan(LexState *State, TValue *tv)
{
   lj_buf_reset(&State->sb);

   while (true) {
      // Check for Unicode operators before identifier scanning
      if (LexToken unicode_tok = lex_unicode_operator(State)) {
         return unicode_tok;
      }

      // Identifier or numeric literal
      if (lj_char_isident(State->c)) {
         State->mark_token_start();

         if (isdigit(State->c)) {
            lex_number(State, tv);
            return TK_number;
         }

         // Scan identifier (stop before Unicode operators like ⧺)
         do {
            lex_savenext(State);
         } while (lj_char_isident(State->c) and not is_unicode_operator_start(State));

         auto str_view = std::string_view(State->sb.b, sbuflen(&State->sb));
         GCstr *s = State->keepstr(str_view);
         setstrV(State->L, tv, s);

         // Check for reserved word
         if (s->reserved > 0) return TK_OFS + s->reserved;
         return TK_name;
      }

      // Token dispatch
      switch (State->c) {
         case '\n':
         case '\r':
            lex_newline(State);
            continue;

         case ' ':
         case '\t':
         case '\v':
         case '\f':
            lex_next(State);
            continue;

         case '-':
            State->mark_token_start();
            lex_next(State);
            if (auto tok = check_compound(State, TK_csub)) return tok;
            if (State->c != '-') return '-';

            lex_next(State);
            if (State->c IS '[') {  // Long comment "--[=*[...]=*]"
               int sep = lex_skipeq(State);
               lj_buf_reset(&State->sb);  // lex_skipeq may dirty the buffer
               if (sep >= 0) {
                  lex_longstring(State, nullptr, sep);
                  lj_buf_reset(&State->sb);
                  continue;
               }
            }

            // Short comment "--.*\n"
            while (not lex_iseol(State->c) and State->c != LEX_EOF) lex_next(State);
            continue;

         case '[': {
            State->mark_token_start();
            int sep = lex_skipeq(State);
            if (sep >= 0) {
               lex_longstring(State, tv, sep);
               return TK_string;
            }
            if (sep IS -1) return '[';
            lj_lex_error(State, TK_string, ErrMsg::XLDELIM);
            continue;
         }

         case '+':
            State->mark_token_start();
            lex_next(State);
            if (auto tok = check_compound(State, TK_cadd)) return tok;
            if (auto tok = check_double(State, '+', TK_plusplus)) return tok;
            return '+';

         case '*':
            State->mark_token_start();
            lex_next(State);
            if (auto tok = check_compound(State, TK_cmul)) return tok;
            return '*';

         case '/':
            State->mark_token_start();
            lex_next(State);
            if (auto tok = check_compound(State, TK_cdiv)) return tok;
            if (State->c IS '/') {  // Single-line comment "//"
               while (State->c != '\n' and State->c != LEX_EOF) lex_next(State);
               continue;
            }
            return '/';

         case '%':
            State->mark_token_start();
            lex_next(State);
            if (auto tok = check_compound(State, TK_cmod)) return tok;
            return '%';

         case '!':
            State->mark_token_start();
            lex_next(State);
            if (State->c IS '=') { lex_next(State); return TK_ne; }
            return '!';

         case '=':
            State->mark_token_start();
            lex_next(State);
            if (State->c IS '>') { lex_next(State); return TK_arrow; }
            if (State->c IS '=') { lex_next(State); return TK_eq; }
            return '=';

         case '<':
            State->mark_token_start();
            lex_next(State);
            if (State->c IS '=') { lex_next(State); return TK_le; }
            if (State->c IS '<') { lex_next(State); return TK_shl; }
            if (State->c IS '{') { lex_next(State); return TK_defer_open; }
            // Check for typed deferred expression: <identifier{
            // Only enter this if we see a letter/underscore immediately (no whitespace)
            if (isalpha(State->c) or State->c IS '_') {
               // Save token position before scanning the identifier
               BCLine ident_line = State->linenumber;
               BCLine ident_column = BCLine(State->current_offset - State->line_start_offset + 1);
               size_t ident_offset = State->current_offset;

               // Scan the identifier into the buffer
               do {
                  lex_savenext(State);
               } while (lj_char_isident(State->c));

               // Check if immediately followed by '{'
               if (State->c IS '{') {
                  lex_next(State);  // Consume the '{'
                  // Store the type name in the token value
                  auto str_view = std::string_view(State->sb.b, sbuflen(&State->sb));
                  GCstr *s = State->keepstr(str_view);
                  setstrV(State->L, tv, s);
                  return TK_defer_typed;
               }
               // Not a typed deferred expression (e.g., "x < y" comparison)
               // Push the identifier as a buffered token to be returned after '<'
               auto str_view = std::string_view(State->sb.b, sbuflen(&State->sb));
               GCstr *s = State->keepstr(str_view);

               LexState::BufferedToken buffered;
               buffered.token = (s->reserved > 0) ? (TK_OFS + s->reserved) : TK_name;
               setstrV(State->L, &buffered.value, s);
               buffered.line = ident_line;
               buffered.column = ident_column;
               buffered.offset = ident_offset;
               State->buffered_tokens.push_front(buffered);
            }
            return '<';

         case '>':
            State->mark_token_start();
            lex_next(State);
            if (State->c IS '=') { lex_next(State); return TK_ge; }
            if (State->c IS '>') { lex_next(State); return TK_shr; }
            return '>';

         case '~':  // Deprecated: ~=
            State->mark_token_start();
            lex_next(State);
            if (State->c IS '=') { lex_next(State); return TK_ne; }
            return '~';

         case ':':
            State->mark_token_start();
            lex_next(State);
            if (State->c IS '>') { lex_next(State); return TK_ternary_sep; }
            return ':';

         case '?':
            State->mark_token_start();
            lex_next(State);
            if (State->c IS '.') { lex_next(State); return TK_safe_field; }
            else if (State->c IS '[') { lex_next(State); return TK_safe_index; }
            else if (State->c IS ':') { lex_next(State); return TK_safe_method; }
            else if (State->c IS '=') { lex_next(State); return TK_cif_empty; }
            else if (State->c IS '?') { lex_next(State); return TK_if_empty; }
            else return '?';

         case '"':
         case '\'':
            State->mark_token_start();
            lex_string(State, tv);
            return TK_string;

         case '.':
            State->mark_token_start();
            if (lex_savenext(State) IS '.') {
               lex_next(State);
               if (State->c IS '.') {
                  lex_next(State);
                  return TK_dots;  // ...
               }
               else if (State->c IS '=') { lex_next(State); return TK_cconcat; }
               else return TK_concat;  // ..
            }
            else if (not isdigit(State->c)) return '.';
            else {
               lex_number(State, tv);
               return TK_number;
            }

         case '|':
            State->mark_token_start();
            lex_next(State);
            if (State->c IS '>') {
               // Basic pipe: |>
               lex_next(State);
               setintV(tv, 0);  // 0 = unlimited (default)
               return TK_pipe;
            }
            else if (isdigit(State->c)) {
               // Pipe with limit: |2>, |10>, etc.
               TValue limit_val;
               lex_number(State, &limit_val);
               if (State->c IS '>') {
                  lex_next(State);
                  // Validate limit is a positive integer
                  double num = tvisnum(&limit_val) ? numV(&limit_val) : double(intV(&limit_val));
                  if (num < 1 or num != std::floor(num)) {
                     lj_lex_error(State, TK_pipe, ErrMsg::XSYMBOL);
                  }
                  // Store limit in token payload
                  *tv = limit_val;
                  return TK_pipe;
               }
               else {
                  // Error: expected '>' after number
                  lj_lex_error(State, TK_pipe, ErrMsg::XSYMBOL);
               }
            }
            return '|';  // Bitwise OR

         case '}':
            State->mark_token_start();
            lex_next(State);
            if (State->c IS '>') { lex_next(State); return TK_defer_close; }
            return '}';

         case LEX_EOF:
            State->mark_token_start();
            return TK_eof;

         default: {
            State->mark_token_start();
            LexChar c = State->c;
            lex_next(State);
            return c;  // Single-char tokens
         }
      }
   }
}

//********************************************************************************************************************
// LexState source text constructor

LexState::LexState(lua_State* L, std::string_view Source, std::string_view Chunkarg, std::optional<std::string_view> Mode)
   : fs(nullptr)
   , L(L)
   , source(Source)
   , pos(0)
   , c(-1)
   , tok(0)
   , lookahead(TK_eof)
   , linenumber(1)
   , lastline(1)
   , chunkname(nullptr)
   , chunkarg(Chunkarg.data())
   , mode(Mode.has_value() ? Mode->data() : nullptr)
   , empty_string_constant(nullptr)
   , vstack(nullptr)
   , sizevstack(0)
   , vtop(0)
   , bcstack(nullptr)
   , sizebcstack(0)
   , level(0)
   , ternary_depth(0)
   , pending_if_empty_colon(0)
   , is_bytecode(0)
   , current_offset(0)
   , line_start_offset(0)
   , current_token_line(1)
   , current_token_column(1)
   , current_token_offset(0)
   , lookahead_line(1)
   , lookahead_column(1)
   , lookahead_offset(0)
   , pending_token_line(1)
   , pending_token_column(1)
   , pending_token_offset(0)
   , active_context(nullptr)
{
   lj_buf_init(L, &this->sb);

   // Read first character
   lex_next(this);

   // Skip UTF-8 BOM if present
   constexpr uint8_t BOM[] = { 0xef, 0xbb, 0xbf };
   bool header = false;
   if (this->c IS BOM[0] and this->peek(0) IS BOM[1] and this->peek(1) IS BOM[2]) {
      constexpr size_t BOM_SIZE = 2;
      this->pos += BOM_SIZE;
      this->current_offset += BOM_SIZE;
      this->line_start_offset += BOM_SIZE;
      lex_next(this);
      header = true;
   }

   // Skip POSIX #! header line
   if (this->c IS '#') {
      do {
         lex_next(this);
         if (this->c IS LEX_EOF) return;
      } while (not lex_iseol(this->c));
      lex_newline(this);
      header = true;
   }

   // Check for bytecode signature
   if (this->c IS LUA_SIGNATURE[0]) {
      if (header) {
         // Security: Loading bytecode with an extra header is disabled to prevent
         // circumvention of the usual bytecode vs. source check
         setstrV(L, L->top++, lj_err_str(L, ErrMsg::BCBAD));
         lj_err_throw(L, LUA_ERRSYNTAX);
      }
      this->is_bytecode = 1;
      // Set up p/pe for bytecode reader compatibility (lj_bcread uses these)
      this->p = this->source.data();
      this->pe = this->source.data() + this->source.size();
   }
}

//********************************************************************************************************************
// LexState direct bytecode constructor (for embedded bytecode in libraries)

LexState::LexState(lua_State* L, const char* BytecodePtr, GCstr* ChunkName)
   : fs(nullptr)
   , L(L)
   , pos(0)
   , c(0)
   , tok(0)
   , lookahead(TK_eof)
   , p(BytecodePtr)
   , pe((const char*)~uintptr_t(0))  // Unlimited - bytecode reader handles its own bounds
   , linenumber(1)
   , lastline(1)
   , chunkname(ChunkName)
   , chunkarg(nullptr)
   , mode(nullptr)
   , empty_string_constant(nullptr)
   , vstack(nullptr)
   , sizevstack(0)
   , vtop(0)
   , bcstack(nullptr)
   , sizebcstack(0)
   , level(BCDUMP_F_STRIP | (LJ_BE * BCDUMP_F_BE))
   , ternary_depth(0)
   , pending_if_empty_colon(0)
   , is_bytecode(1)
   , current_offset(0)
   , line_start_offset(0)
   , current_token_line(1)
   , current_token_column(1)
   , current_token_offset(0)
   , lookahead_line(1)
   , lookahead_column(1)
   , lookahead_offset(0)
   , pending_token_line(1)
   , pending_token_column(1)
   , pending_token_offset(0)
   , active_context(nullptr)
{
   lj_buf_init(L, &this->sb);
}

//********************************************************************************************************************
// LexState bytecode streaming constructor (uses lua_Reader for streaming bytecode)

LexState::LexState(lua_State* L, lua_Reader Rfunc, void* Rdata, std::string_view Chunkarg, std::optional<std::string_view> Mode)
   : fs(nullptr)
   , L(L)
   , pos(0)
   , c(0)  // Bytecode reader uses c=0 as valid, c<0 as EOF
   , tok(0)
   , lookahead(TK_eof)
   , p(nullptr)
   , pe(nullptr)
   , rfunc(Rfunc)
   , rdata(Rdata)
   , linenumber(1)
   , lastline(1)
   , chunkname(nullptr)
   , chunkarg(Chunkarg.data())
   , mode(Mode.has_value() ? Mode->data() : nullptr)
   , empty_string_constant(nullptr)
   , vstack(nullptr)
   , sizevstack(0)
   , vtop(0)
   , bcstack(nullptr)
   , sizebcstack(0)
   , level(0)
   , ternary_depth(0)
   , pending_if_empty_colon(0)
   , is_bytecode(0)  // Will be set after checking first character
   , current_offset(0)
   , line_start_offset(0)
   , current_token_line(1)
   , current_token_column(1)
   , current_token_offset(0)
   , lookahead_line(1)
   , lookahead_column(1)
   , lookahead_offset(0)
   , pending_token_line(1)
   , pending_token_column(1)
   , pending_token_offset(0)
   , active_context(nullptr)
{
   lj_buf_init(L, &this->sb);

   // For streaming, read the first chunk to set up source for lex_next
   size_t sz;
   const char* buf = this->rfunc(L, this->rdata, &sz);
   if (buf and sz > 0) {
      this->p = buf;
      this->pe = buf + sz;
      this->source = std::string_view(buf, sz);  // Initialize source for lex_next

      // Read first character using lex_next (like the source constructor does)
      lex_next(this);

      // Check for bytecode signature
      if (this->c IS LUA_SIGNATURE[0]) {
         this->is_bytecode = 1;
      }
   }
   else {
      this->c = -1;  // EOF
   }
}

//********************************************************************************************************************
// LexState destructor

LexState::~LexState()
{
   if (not this->L) return;  // Not properly initialised

   global_State* g = G(this->L);
   if (this->bcstack) lj_mem_freevec(g, this->bcstack, this->sizebcstack, BCInsLine);
   if (this->vstack) lj_mem_freevec(g, this->vstack, this->sizevstack, VarInfo);
   lj_buf_free(g, &this->sb);
}

//********************************************************************************************************************
// Token stream management

void LexState::next()
{
   // Consume lookahead token if available
   if (this->lookahead != TK_eof) {
      this->tok = this->lookahead;
      copyTV(this->L, &this->tokval, &this->lookaheadval);
      this->current_token_line = this->lookahead_line;
      this->current_token_column = this->lookahead_column;
      this->current_token_offset = this->lookahead_offset;
      this->lastline = this->current_token_line;
      this->lookahead = TK_eof;
      return;
   }

   // Consume buffered token if available
   if (not this->buffered_tokens.empty()) {
      auto buffered = this->buffered_tokens.front();
      this->buffered_tokens.pop_front();
      this->apply_buffered_token(buffered);
      return;
   }

   // Scan next token from input
   this->tok = lex_scan(this, &this->tokval);
   this->current_token_line = this->pending_token_line;
   this->current_token_column = this->pending_token_column;
   this->current_token_offset = this->pending_token_offset;
   this->lastline = this->current_token_line;
}

//********************************************************************************************************************

LexToken LexState::lookahead_token()
{
   this->assert_condition(this->lookahead IS TK_eof, "double lookahead");

   if (not this->buffered_tokens.empty()) {
      auto buffered = this->buffered_tokens.front();
      this->buffered_tokens.pop_front();
      this->lookahead = buffered.token;
      copyTV(this->L, &this->lookaheadval, &buffered.value);
      this->lookahead_line = buffered.line;
      this->lookahead_column = buffered.column;
      this->lookahead_offset = buffered.offset;
      return this->lookahead;
   }

   this->lookahead = lex_scan(this, &this->lookaheadval);
   this->lookahead_line = this->pending_token_line;
   this->lookahead_column = this->pending_token_column;
   this->lookahead_offset = this->pending_token_offset;
   return this->lookahead;
}

//********************************************************************************************************************
// Token utilities

const char* LexState::token2str(LexToken Tok)
{
   if (Tok > TK_OFS) return token_names[Tok - TK_OFS - 1];
   if (not iscntrl(Tok)) return lj_strfmt_pushf(this->L, "%c", Tok);
   return lj_strfmt_pushf(this->L, "char(%d)", Tok);
}

void LexState::mark_token_start()
{
   size_t token_offset = (this->c IS LEX_EOF) ? this->pos : this->current_offset;
   this->pending_token_line = this->linenumber;

   if (token_offset >= this->line_start_offset) {
      this->pending_token_column = BCLine((token_offset - this->line_start_offset) + 1);
   }
   else this->pending_token_column = 1;

   this->pending_token_offset = token_offset;
}

//********************************************************************************************************************
// Buffered token management

void LexState::apply_buffered_token(const BufferedToken& token)
{
   this->tok = token.token;
   copyTV(this->L, &this->tokval, &token.value);
   this->current_token_line = token.line;
   this->current_token_column = token.column;
   this->current_token_offset = token.offset;
   this->lastline = this->current_token_line;
}

LexState::BufferedToken LexState::scan_buffered_token()
{
   BufferedToken buffered;
   setnilV(&buffered.value);
   buffered.token = lex_scan(this, &buffered.value);
   buffered.line = this->pending_token_line;
   buffered.column = this->pending_token_column;
   buffered.offset = this->pending_token_offset;
   return buffered;
}

void LexState::ensure_lookahead(size_t count)
{
   while (this->available_lookahead() < count) {
      this->buffered_tokens.push_back(this->scan_buffered_token());
   }
}

[[nodiscard]] size_t LexState::available_lookahead() const
{
   size_t available = this->buffered_tokens.size();
   if (this->lookahead != TK_eof) available++;
   return available;
}

[[nodiscard]] const LexState::BufferedToken* LexState::buffered_token(size_t index) const
{
   if (index >= this->buffered_tokens.size()) return nullptr;
   return &this->buffered_tokens[index];
}

[[noreturn]] LJ_NOINLINE void LexState::err_syntax(ErrMsg Message)
{
   if (this->active_context) this->active_context->err_syntax(Message);
   lj_lex_error(this, this->tok, Message);
}

[[noreturn]] LJ_NOINLINE void LexState::err_token(LexToken Token)
{
   if (this->active_context) this->active_context->err_token(Token);
   lj_lex_error(this, this->tok, ErrMsg::XTOKEN, this->token2str(Token));
}

// Check and consume optional token.

int LexState::lex_opt(LexToken Token)
{
   if (this->active_context) return this->active_context->lex_opt(Token);

   if (this->tok IS Token) {
      this->next();
      return 1;
   }
   return 0;
}

// Check and consume token.

void LexState::lex_check(LexToken Token)
{
   if (this->active_context) {
      this->active_context->lex_check(Token);
      return;
   }
   if (this->tok != Token) this->err_token(Token);
   this->next();
}

// Check for matching token.

void LexState::lex_match(LexToken What, LexToken Who, BCLine Line)
{
   if (this->active_context) this->active_context->lex_match(What, Who, Line);
   else if (not this->lex_opt(What)) {
      if (Line IS this->linenumber) this->err_token(What);
      else {
         auto swhat = this->token2str(What);
         auto swho = this->token2str(Who);
         lj_lex_error(this, this->tok, ErrMsg::XMATCH, swhat, swho, Line);
      }
   }
}

// Check for string token.

[[nodiscard]] GCstr * LexState::lex_str()
{
   if (this->active_context) return this->active_context->lex_str();

   if (this->tok != TK_name) this->err_token(TK_name);
   GCstr *s = strV(&this->tokval);
   this->next();
   return s;
}

//********************************************************************************************************************
// Source location tracking

[[nodiscard]] SourceSpan LexState::current_token_span() const
{
   return SourceSpan{
      .line = this->current_token_line,
      .column = this->current_token_column,
      .offset = this->current_token_offset
   };
}

[[nodiscard]] SourceSpan LexState::lookahead_token_span() const
{
   return SourceSpan{
      .line = this->lookahead_line,
      .column = this->lookahead_column,
      .offset = this->lookahead_offset
   };
}

//********************************************************************************************************************
// Error reporting

void lj_lex_error(LexState *State, LexToken tok, ErrMsg em, ...)
{
   va_list argp;
   va_start(argp, em);

   const char* tokstr = nullptr;
   if (tok) {
      if (tok IS TK_name or tok IS TK_string or tok IS TK_number) {
         lex_save(State, '\0');
         tokstr = State->sb.b;
      }
      else tokstr = State->token2str(tok);
   }

   lj_err_lex(State->L, State->chunkname, tokstr, State->linenumber, em, argp);
   va_end(argp);
}

//********************************************************************************************************************
// Reserved word initialisation using TOKEN_DEFINITIONS from lexer.h

void lj_reserve_words(lua_State *Lua)
{
   // Register all reserved words from TOKEN_DEFINITIONS with the string table.
   // Reserved words are identified by their is_reserved() flag in the definition.
   for (uint32_t i = 0; i < TOKEN_DEFINITIONS.size(); i++) {
      if (not TOKEN_DEFINITIONS[i].is_reserved()) break;  // Reserved words are contiguous at the start
      GCstr *s = lj_str_newz(Lua, TOKEN_DEFINITIONS[i].name.data());
      fixstring(s);  // Reserved words are never collected
      s->reserved = uint8_t(i + 1);
   }
}
