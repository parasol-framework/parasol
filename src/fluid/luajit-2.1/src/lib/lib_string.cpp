// String library
//
// Copyright (C) 2025-2026 Paul Manias
// Copyright (C) 2005-2022 Mike Pall. See Copyright Notice in luajit.h
//
// Major portions taken verbatim or adapted from the Lua interpreter.
// Copyright (C) 1994-2008 Lua.org, PUC-Rio. See Copyright Notice in lua.h

#define lib_string_c
#define LUA_LIB

#include <cctype>
#include <array>
#include <string_view>
#include <vector>
#include <algorithm>

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
#include "lj_obj.h"
#include "lj_gc.h"
#include "lj_err.h"
#include "lj_buf.h"
#include "lj_str.h"
#include "lj_tab.h"
#include "lj_meta.h"
#include "lj_state.h"
#include "lj_ff.h"
#include "lj_bcdump.h"
#include "lj_char.h"
#include "lj_strfmt.h"
#include "lj_array.h"
#include "lib.h"
#include "lib_utils.h"
#include "lib_range.h"
#include "lj_proto_registry.h"
#include "debug/error_guard.h"

#include <parasol/modules/regex.h>

#include "../../defs.h"

#define L_ESC      '%'

//********************************************************************************************************************
// Regex-based pattern matching support

// Ensure regex module is loaded (for modular builds)
static ERR load_regex_module(void)
{
#ifndef PARASOL_STATIC
   if (not modRegex) {
      pf::SwitchContext ctx(glFluidContext);
      if (objModule::load("regex", &modRegex, &RegexBase) != ERR::Okay) return ERR::InitModule;
   }
#endif
   return ERR::Okay;
}

// Result of converting a Lua pattern to regex
struct PatternConversionResult {
   std::string regex_pattern;
   std::vector<int> position_capture_indices;  // 1-based indices of position captures
   bool has_start_anchor = false;
   bool has_end_anchor = false;
   std::string error_message;
};

// Emit regex character class content (without brackets, for use inside [...])

static void emit_regex_class_content(std::string &out, int cl)
{
   switch (cl) {
      case 'a': out += "A-Za-z"; break;
      case 'c': out += "\\x00-\\x1f\\x7f"; break;
      case 'd': out += "0-9"; break;
      case 'g': out += "\\x21-\\x7e"; break;
      case 'l': out += "a-z"; break;
      case 'p': out += "!\"#$%&'()*+,\\-./:;<=>?@\\[\\\\\\]^_`{|}~"; break; // Punctuation characters
      case 's': out += "\\s"; break;
      case 'u': out += "A-Z"; break;
      case 'w': out += "\\w"; break;
      case 'x': out += "0-9A-Fa-f"; break;
      case 'z': out += "\\x00"; break;
      default: out += char(cl); break;
   }
}

// Emit regex character class for Lua pattern class character

static void emit_regex_class(std::string &out, int cl, bool negated)
{
   if (negated) out += "[^";
   else out += '[';

   emit_regex_class_content(out, cl);

   out += ']';
}

// Check if character needs escaping in regex

static bool is_regex_special(int c)
{
   return c IS '\\' or c IS '|' or c IS '{' or c IS '}' or c IS '/' or
          c IS '.' or c IS '*' or c IS '+' or c IS '?' or c IS '(' or
          c IS ')' or c IS '[' or c IS ']' or c IS '^' or c IS '$';
}

// Check if character needs escaping inside a regex character class (SRELL Unicode mode)

static bool needs_escape_in_charclass(int c)
{
   // SRELL requires these to be escaped: ( ) [ ] { } / | backslash
   // Note: '-' is conditionally escaped if used in classes at the first or last position, i.e. [\-A] or [A\-], not [A-Z]
   return c IS '(' or c IS ')' or c IS '[' or c IS ']' or c IS '{' or c IS '}' or
          c IS '/' or c IS '|' or c IS '\\';
}

// Convert Lua pattern to regex

static ERR convert_lua_pattern_to_regex(CSTRING pattern, size_t len, PatternConversionResult &result)
{
   pf::Log log("lua_to_regex");
   CSTRING p = pattern;
   CSTRING end = pattern + len;
   int capture_index = 0;  // Current capture group index (1-based when stored)

   result.regex_pattern.clear();
   result.position_capture_indices.clear();
   result.has_start_anchor = false;
   result.has_end_anchor = false;
   result.error_message.clear();

   // Handle start anchor
   if (p < end and *p IS '^') {
      result.has_start_anchor = true;
      result.regex_pattern += '^';
      p++;
   }

   while (p < end) {
      int c = uint8_t(*p++);

      if (c IS '%') {  // Escape sequence
         if (p >= end) {
            result.error_message = "Pattern ends with '%'";
            return ERR::Syntax;
         }
         int cl = uint8_t(*p++);

         // Check for unsupported patterns
         if (cl IS 'b') {
            result.error_message = "Unsupported Lua pattern: %b (balanced matching) has no regex equivalent";
            return ERR::NoSupport;
         }
         if (cl IS 'f') {
            result.error_message = "Unsupported Lua pattern: %f (frontier pattern) has no regex equivalent";
            return ERR::NoSupport;
         }

         // Handle backreferences %1-%9

         if (cl >= '1' and cl <= '9') {
            result.regex_pattern += '\\';
            result.regex_pattern += char(cl);
            continue;
         }

         // Handle character classes

         int lower_cl = std::tolower(cl);
         if (lower_cl IS 'a' or lower_cl IS 'c' or lower_cl IS 'd' or lower_cl IS 'g' or
             lower_cl IS 'l' or lower_cl IS 'p' or lower_cl IS 's' or lower_cl IS 'u' or
             lower_cl IS 'w' or lower_cl IS 'x' or lower_cl IS 'z') {
            emit_regex_class(result.regex_pattern, lower_cl, std::isupper(cl));
         }
         else if (cl IS '%') { // %% -> literal %
            result.regex_pattern += '%';
         }
         else if (cl IS '-') { // %- -> literal hyphen (no escape needed in regex outside char class)
            result.regex_pattern += '-';
         }
         else { // Escaped special character - emit as regex escape
            result.regex_pattern += '\\';
            result.regex_pattern += char(cl);
         }
      }
      else if (c IS '-') {
         // Lua's non-greedy quantifier - convert to *?
         result.regex_pattern += "*?";
      }
      else if (c IS '(') { // Check for position capture: ()
         if (p < end and *p IS ')') { // Position capture - record it and emit empty capture group
            capture_index++;
            result.position_capture_indices.push_back(capture_index);
            result.regex_pattern += "()";
            p++;  // Skip the ')'
         }
         else { // Regular capture group
            capture_index++;
            result.regex_pattern += '(';
         }
      }
      else if (c IS ')') {
         result.regex_pattern += ')';
      }
      else if (c IS '[') { // Character class - copy through, handling nested escapes
         result.regex_pattern += '[';
         bool first = true;
         bool found_close = false;

         while (p < end) {
            c = uint8_t(*p++);
            if (c IS ']' and not first) {
               result.regex_pattern += ']';
               found_close = true;
               break;
            }

            if (c IS '%' and p < end) { // Escaped character inside class
               int esc = uint8_t(*p++);
               int lower_esc = std::tolower(esc);

               if (lower_esc IS 'a' or lower_esc IS 'c' or lower_esc IS 'd' or lower_esc IS 'g' or
                   lower_esc IS 'l' or lower_esc IS 'p' or lower_esc IS 's' or lower_esc IS 'u' or
                   lower_esc IS 'w' or lower_esc IS 'x' or lower_esc IS 'z') {
                  bool neg = std::isupper(esc);
                  if (neg) { // Can't easily negate inside a character class, emit as escaped
                     result.regex_pattern += '\\';
                     result.regex_pattern += char(esc);
                  }
                  else emit_regex_class_content(result.regex_pattern, lower_esc);
               }
               else if (esc IS '%') result.regex_pattern += '%';
               else { // Other escaped char
                  result.regex_pattern += '\\';
                  result.regex_pattern += char(esc);
               }
            }
            else if (needs_escape_in_charclass(c)) { // Escape characters that SRELL requires escaped in Unicode mode
               result.regex_pattern += '\\';
               result.regex_pattern += char(c);
            }
            else result.regex_pattern += char(c);
            first = false;
         }

         if (not found_close) {
            result.error_message = "Malformed pattern (missing ']')";
            return ERR::Syntax;
         }
      }
      else if (c IS '$' and p IS end) { // End anchor (only at end of pattern)
         result.has_end_anchor = true;
         result.regex_pattern += '$';
      }
      else if (c IS '.') { // Lua's . matches any char except newline, same as regex default
         result.regex_pattern += '.';
      }
      else if (c IS '*' or c IS '+' or c IS '?') { // Quantifiers pass through
         result.regex_pattern += char(c);
      }
      else if (is_regex_special(c)) { // Escape regex-special chars
         result.regex_pattern += '\\';
         result.regex_pattern += char(c);
      }
      else { // Regular character
         result.regex_pattern += char(c);
      }
   }

   log.function("Pattern: \"%s\" -> \"%s\"", pattern, result.regex_pattern.c_str());

   return ERR::Okay;
}

// Context for regex search callback

struct FindMatchContext {
   CSTRING src_init;           // Start of original subject string
   size_t offset;              // Offset from which search started
   bool matched = false;
   size_t match_start = 0;     // Relative to search start
   size_t match_end = 0;       // Relative to search start
   std::vector<std::string_view> captures;
};

// Callback for first match only
static ERR find_match_callback(int Index, std::vector<std::string_view>& Captures,
   size_t MatchStart, size_t MatchEnd, FindMatchContext& Meta)
{
   Meta.matched = true;
   Meta.match_start = MatchStart;
   Meta.match_end = MatchEnd;
   Meta.captures = Captures;
   return ERR::Terminate;  // Only need first match
}

// Push captures to Lua stack, handling position captures
static int push_regex_captures(lua_State *L, const FindMatchContext &Context,
   const std::vector<int> &position_caps, int find)
{
   // If no explicit captures (only full match), behavior depends on find mode
   if (Context.captures.size() <= 1) {
      if (Context.captures.empty()) return 0;
      if (find IS 0) { // string.match with no captures returns the whole match
         lua_pushlstring(L, Context.captures[0].data(), Context.captures[0].size());
         return 1;
      }
      return 0; // string.find with no captures just returns positions
   }

   // Push explicit captures (skip captures[0] which is full match)
   int count = 0;
   for (size_t i = 1; i < Context.captures.size(); i++) {
      bool is_position_cap = std::find(position_caps.begin(), position_caps.end(), int(i)) != position_caps.end();

      if (is_position_cap) {
         // Position capture: push the offset into the original string (0-based)
         ptrdiff_t pos = Context.captures[i].data() - Context.src_init;
         lua_pushinteger(L, int(pos));
      }
      else lua_pushlstring(L, Context.captures[i].data(), Context.captures[i].size());
      count++;
   }

   return count;
}

//********************************************************************************************************************
// Helper to check if a TValue is a range userdata and extract it

static fluid_range * get_range_from_tvalue(lua_State *L, cTValue *tv)
{
   if (not tvisudata(tv)) return nullptr;

   GCudata* ud = udataV(tv);
   GCtab* mt = tabref(ud->metatable);
   if (not mt) return nullptr;

   // Get the expected metatable for ranges
   lua_getfield(L, LUA_REGISTRYINDEX, RANGE_METATABLE);
   if (lua_isnil(L, -1)) {
      lua_pop(L, 1);
      return nullptr;
   }
   GCtab* range_mt = tabV(L->top - 1);
   lua_pop(L, 1);

   // Compare metatables
   if (mt != range_mt) return nullptr;

   return (fluid_range*)uddata(ud);
}

// NOTE: Any string function marked with the ASM macro uses a custom assembly implementation in the
// .dasc files.  Changing the C++ code here will have no effect in such cases.

#define LJLIB_MODULE_string

LJLIB_CF(string_len)
{
   GCstr *s = lj_lib_checkstr(L, 1);
   int32_t len = (int32_t)s->len;
   setintV(L->top - 1, len);
   return 1;
}

//********************************************************************************************************************
// NOTE: ASM version exists

LJLIB_ASM(string_byte)      LJLIB_REC(string_range 0)
{
   GCstr *s = lj_lib_checkstr(L, 1);
   int32_t len = (int32_t)s->len;
   int32_t start = lj_lib_optint(L, 2, 0);  // 0-based: default start is 0
   int32_t stop = lj_lib_optint(L, 3, start);
   int32_t n, i;
   const unsigned char *p;
   if (stop < 0) stop += len;   // 0-based: -1 → len-1 (last char)
   if (start < 0) start += len;
   if (start < 0) start = 0;
   if (stop > len - 1) stop = len - 1;  // 0-based: max valid index is len-1
   if (start > stop) return FFH_RES(0);  //  Empty interval: return no results.
   n = stop - start + 1;
   if ((uint32_t)n > LUAI_MAXCSTACK) lj_err_caller(L, ErrMsg::STRSLC);
   lj_state_checkstack(L, (MSize)n);
   p = (const unsigned char*)strdata(s) + start;
   for (i = 0; i < n; i++) setintV(L->base + i - 1 - LJ_FR2, p[i]);
   return FFH_RES(n);
}

//********************************************************************************************************************
// NOTE: ASM version exists

LJLIB_ASM(string_char)      LJLIB_REC(.)
{
   int i, nargs = (int)(L->top - L->base);
   char* buf = lj_buf_tmp(L, (MSize)nargs);
   for (i = 1; i <= nargs; i++) {
      int32_t k = lj_lib_checkint(L, i);
      LJ_CHECK_ARG(L, i, checku8(k), ErrMsg::BADVAL);
      buf[i - 1] = (char)k;
   }
   setstrV(L, L->base - 1 - LJ_FR2, lj_str_new(L, buf, (size_t)nargs));
   return FFH_RES(1);
}

//********************************************************************************************************************
// NOTE: Backed by an ASM implementation
// string_sub:	Declares an assembly ffunc as its primary implementation. The C code that follows is the fallback (called when the ffunc jumps to ->fff_fallback).
// string_range 1: Tells the JIT recorder how to handle this function. string_range is the recorder function name, 1 is a parameter distinguishing it from other range operations.

LJLIB_ASM(string_sub)      LJLIB_REC(string_range 1)
{
   lj_lib_checkstr(L, 1);
   lj_lib_checkint(L, 2);
   int32_t end_val = lj_lib_optint(L, 3, -1);
   // Convert exclusive end to inclusive by subtracting 1, but only for positive indices.
   // Negative indices already reference positions from the end, so no adjustment needed.
   if (end_val > 0) end_val--;
   setintV(L->base + 2, end_val);
   return FFH_RETRY;
}

//********************************************************************************************************************
// string.substr() is now an alias for string.sub() - both use exclusive end semantics.
// The ASM implementation jumps directly to string_sub.

LJLIB_ASM(string_substr)      LJLIB_REC(string_range 1)
{
   // Fallback: just retry with the same arguments - string_sub will handle it
   return FFH_RETRY;
}

//********************************************************************************************************************

LJLIB_CF(string_rep)      LJLIB_REC(.)
{
   GCstr *s = lj_lib_optstr(L, 1);
   if (not s) s = &G(L)->strempty;
   int32_t rep = lj_lib_checkint(L, 2);
   GCstr *sep = lj_lib_optstr(L, 3);
   SBuf *sb = lj_buf_tmp_(L);
   if (sep and rep > 1) {
      GCstr *s2 = lj_buf_cat2str(L, sep, s);
      lj_buf_reset(sb);
      lj_buf_putstr(sb, s);
      s = s2;
      rep--;
   }
   sb = lj_buf_putstr_rep(sb, s, rep);
   setstrV(L, L->top - 1, lj_buf_str(L, sb));
   lj_gc_check(L);
   return 1;
}

//********************************************************************************************************************
// string.alloc() is a quicker version of string.rep() for reserving space without filling it.
//
// 1. Takes a size parameter - Uses lj_lib_checkint(L, 1) to get the size from the first argument
// 2. Validates the size - Checks that size is not negative and throws an error if it is
// 3. Reserves buffer space - Uses lj_buf_need(sb, (MSize)size) to ensure the buffer has enough capacity
// 4. Advances the write pointer - Sets sb->w += size to reserve the space without filling it
// 5. Returns the string - Creates and returns the string with the reserved space

LJLIB_CF(string_alloc)
{
   int32_t size = lj_lib_checkint(L, 1);
   LJ_CHECK_ARG(L, 1, size >= 0, ErrMsg::NUMRNG);
   SBuf *sb = lj_buf_tmp_(L);
   lj_buf_reset(sb);
   (void)lj_buf_need(sb, (MSize)size);
   sb->w += size;  //  Advance write pointer to reserve space
   setstrV(L, L->top - 1, lj_buf_str(L, sb));
   lj_gc_check(L);
   return 1;
}

//********************************************************************************************************************
// Helper to find the next separator in a string
// Returns pointer to separator if found, nullptr otherwise

static CSTRING find_separator(CSTRING Pos, CSTRING End, CSTRING Sep, MSize SepLen, bool IsWhitespace)
{
   if (IsWhitespace) {
      // Whitespace separators: scan the entire range, independent of SepLen.
      for (CSTRING p = Pos; p < End; p++) {
         if (*p IS ' ' or *p IS '\t' or *p IS '\n' or *p IS '\r') return p;
      }
      return nullptr;
   }

   if (SepLen IS 1) return (CSTRING)memchr(Pos, Sep[0], End - Pos);

   // Multi-character separator.
   for (CSTRING p = Pos; p <= End - SepLen; p++) {
      if (memcmp(p, Sep, SepLen) IS 0) return p;
   }
   return nullptr;
}

//********************************************************************************************************************

LJLIB_CF(string_split)
{
   GCstr *s = lj_lib_checkstr(L, 1);
   GCstr *sep = lj_lib_optstr(L, 2);
   CSTRING str = strdata(s);
   CSTRING sepstr;
   MSize seplen;
   MSize slen = s->len;
   bool is_whitespace = false;

   if ((not sep) or (sep->len IS 0)) {
      sepstr = " \t\n\r";  // Default whitespace separators
      seplen = 4;
      is_whitespace = true;
   }
   else {
      sepstr = strdata(sep);
      seplen = sep->len;
   }

   // Handle empty string - return empty array
   if (slen IS 0) {
      GCarray *arr = lj_array_new(L, 0, AET::STR_GC);
      setarrayV(L, L->top++, arr);
      return 1;
   }

   CSTRING end = str + slen;

   // First pass: count separators to determine array size
   uint32_t count = 1;  // At least one element (final segment)
   CSTRING pos = str;
   while (pos < end) {
      CSTRING found = find_separator(pos, end, sepstr, seplen, is_whitespace);
      if (found) {
         count++;
         pos = found + (is_whitespace ? 1 : seplen);
      }
      else break;
   }

   // Create array with exact size
   GCarray *arr = lj_array_new(L, count, AET::STR_GC);
   GCRef *refs = arr->get<GCRef>();

   // Second pass: populate the array
   pos = str;
   uint32_t idx = 0;
   while (pos <= end and idx < count) {
      CSTRING found = find_separator(pos, end, sepstr, seplen, is_whitespace);

      if (found) {
         GCstr *substr = lj_str_new(L, pos, found - pos);
         setgcref(refs[idx], obj2gco(substr));
         lj_gc_objbarrier(L, arr, substr);
         idx++;
         pos = found + (is_whitespace ? 1 : seplen);
      }
      else {
         // Add final substring
         GCstr *substr = lj_str_new(L, pos, end - pos);
         setgcref(refs[idx], obj2gco(substr));
         lj_gc_objbarrier(L, arr, substr);
         idx++;
         break;
      }
   }

   setarrayV(L, L->top++, arr);
   lj_gc_check(L);
   return 1;
}

//********************************************************************************************************************

LJLIB_CF(string_trim)
{
   GCstr *s = lj_lib_optstr(L, 1);
   if (not s) {
      setstrV(L, L->top - 1, &G(L)->strempty);
      return 1;
   }

   CSTRING str = strdata(s);
   MSize len = s->len;
   CSTRING start = str;
   CSTRING end = str + len;

   if (len IS 0) {
      setstrV(L, L->top - 1, &G(L)->strempty);
      return 1;
   }

   // Skip leading whitespace
   while (start < end and (*start IS ' ' or *start IS '\t' or *start IS '\n' or *start IS '\r')) start++;

   // Skip trailing whitespace
   while (end > start and (end[-1] IS ' ' or end[-1] IS '\t' or end[-1] IS '\n' or end[-1] IS '\r')) end--;

   // If all whitespace, return empty string
   if (start >= end) {
      setstrV(L, L->top - 1, &G(L)->strempty);
      return 1;
   }

   // Create trimmed string
   GCstr *result = lj_str_new(L, start, end - start);
   setstrV(L, L->top - 1, result);
   lj_gc_check(L);
   return 1;
}

//********************************************************************************************************************

LJLIB_CF(string_rtrim)
{
   GCstr *s = lj_lib_optstr(L, 1);
   if (not s) {
      setstrV(L, L->top - 1, &G(L)->strempty);
      return 1;
   }

   auto str = strdata(s);
   MSize len = s->len;
   auto end = str + len;

   if (len IS 0) {
      setstrV(L, L->top - 1, s);  //  Return original empty string
      return 1;
   }

   // Find end of non-whitespace
   while (end > str and (end[-1] IS ' ' or end[-1] IS '\t' or end[-1] IS '\n' or end[-1] IS '\r'))
      end--;

   // Create right-trimmed string
   GCstr *result = lj_str_new(L, str, end - str);
   setstrV(L, L->top - 1, result);
   lj_gc_check(L);
   return 1;
}

//********************************************************************************************************************

LJLIB_CF(string_startsWith)
{
   GCstr *s = lj_lib_checkstr(L, 1);
   GCstr *prefix = lj_lib_checkstr(L, 2);
   auto str = strdata(s);
   auto prefixstr = strdata(prefix);
   MSize slen = s->len;
   MSize prefixlen = prefix->len;

   // Empty prefix always matches

   if (prefixlen IS 0) {
      setboolV(L->top - 1, 1);
      return 1;
   }

   // Prefix longer than string cannot match

   if (prefixlen > slen) {
      setboolV(L->top - 1, 0);
      return 1;
   }

   // Compare prefix with start of string

   int matches = (memcmp(str, prefixstr, prefixlen) IS 0);
   setboolV(L->top - 1, matches);
   return 1;
}

//********************************************************************************************************************

LJLIB_CF(string_endsWith)
{
   GCstr *s = lj_lib_checkstr(L, 1);
   GCstr *suffix = lj_lib_checkstr(L, 2);
   CSTRING str = strdata(s);
   CSTRING suffixstr = strdata(suffix);
   MSize slen = s->len;
   MSize suffixlen = suffix->len;

   // Empty suffix always matches
   if (suffixlen IS 0) {
      setboolV(L->top - 1, 1);
      return 1;
   }

   // Suffix longer than string cannot match
   if (suffixlen > slen) {
      setboolV(L->top - 1, 0);
      return 1;
   }

   // Compare suffix with end of string
   int matches = (memcmp(str + slen - suffixlen, suffixstr, suffixlen) IS 0);
   setboolV(L->top - 1, matches);
   return 1;
}

//********************************************************************************************************************

LJLIB_CF(string_join)
{
   GCtab *t = lj_lib_checktab(L, 1);
   GCstr *sep = lj_lib_optstr(L, 2);
   CSTRING sepstr = "";
   MSize seplen = 0;
   SBuf* sb = lj_buf_tmp_(L);
   int32_t len = (int32_t)lj_tab_len(t);
   int32_t last = len - 1;  // 0-based: last index = len-1
   int32_t i;

   if (sep) {
      sepstr = strdata(sep);
      seplen = sep->len;
   }

   lj_buf_reset(sb);

   for (i = 0; i <= last; i++) {
      cTValue *tv = lj_tab_getint(t, i);
      if (tv and !tvisnil(tv)) {
         int isValidType = 0;

         // Check if we have a valid type to process
         if (tvisstr(tv) or tvisnum(tv)) {
            isValidType = 1;
         }

         if (isValidType) { // Add separator before non-first elements
            if (sb->w > sb->b and seplen > 0) {
               lj_buf_putmem(sb, sepstr, seplen);
            }

            if (tvisstr(tv)) { // Add string content
               lj_buf_putstr(sb, strV(tv));
            }
            else if (tvisnum(tv)) { // Convert number to string directly into buffer
               sb = lj_strfmt_putnum(sb, tv);
            }
         }
      }
   }

   setstrV(L, L->top - 1, lj_buf_str(L, sb));
   lj_gc_check(L);
   return 1;
}

//********************************************************************************************************************

LJLIB_CF(string_cap)
{
   GCstr *s = lj_lib_checkstr(L, 1);
   auto str = strdata(s);
   MSize len = s->len;

   if (len IS 0) {
      setstrV(L, L->top - 1, &G(L)->strempty);
      return 1;
   }

   SBuf *sb = lj_buf_tmp_(L);
   lj_buf_reset(sb);

   char first = str[0];
   if (first >= 'a' and first <= 'z') first -= 32;
   lj_buf_putb(sb, first);

   if (len > 1) lj_buf_putmem(sb, str + 1, len - 1);

   setstrV(L, L->top - 1, lj_buf_str(L, sb));
   lj_gc_check(L);
   return 1;
}

//********************************************************************************************************************

LJLIB_CF(string_decap)
{
   GCstr *s = lj_lib_checkstr(L, 1);
   auto str = strdata(s);
   MSize len = s->len;

   if (len IS 0) {
      setstrV(L, L->top - 1, &G(L)->strempty);
      return 1;
   }

   SBuf *sb = lj_buf_tmp_(L);
   lj_buf_reset(sb);

   char first = str[0];
   if (first >= 'A' and first <= 'Z') first += 32;
   lj_buf_putb(sb, first);

   if (len > 1) lj_buf_putmem(sb, str + 1, len - 1);

   setstrV(L, L->top - 1, lj_buf_str(L, sb));
   lj_gc_check(L);
   return 1;
}

//********************************************************************************************************************

LJLIB_CF(string_hash)
{
   GCstr *s = lj_lib_checkstr(L, 1);
   int caseSensitive = 0;  //  Default: case insensitive

   // Check for optional second parameter (boolean)
   if (L->base + 1 < L->top and tvisbool(L->base + 1)) caseSensitive = boolV(L->base + 1);

   auto str = strdata(s);
   uint32_t hash;
   if (caseSensitive) hash = pf::strhash({ str, s->len });
   else hash = pf::strihash({ str, s->len });

   setintV(L->top - 1, (int32_t)hash);
   return 1;
}

//********************************************************************************************************************

LJLIB_CF(string_unescapeXML)
{
   GCstr *s = lj_lib_optstr(L, 1);

   if (not s) {
      setstrV(L, L->top - 1, &G(L)->strempty);
      return 1;
   }

   static constexpr std::array<std::pair<std::string_view, char>, 5> entities = {{
      { "lt;", '<' }, { "gt;", '>' }, { "amp;", '&' }, { "quot;", '"' }, { "apos;", '\'' }
   }};

   std::string_view input(strdata(s), s->len);
   SBuf *sb = lj_buf_tmp_(L);
   lj_buf_reset(sb);

   for (size_t i = 0; i < input.size(); i++) {
      if (input[i] IS '&') {
         std::string_view remaining = input.substr(i + 1);
         bool matched = false;

         for (const auto& [entity, replacement] : entities) {
            if (remaining.starts_with(entity)) {
               lj_buf_putb(sb, replacement);
               i += entity.size();
               matched = true;
               break;
            }
         }

         if (not matched) lj_buf_putb(sb, '&');
      }
      else lj_buf_putb(sb, input[i]);
   }

   setstrV(L, L->top - 1, lj_buf_str(L, sb));
   lj_gc_check(L);
   return 1;
}

//********************************************************************************************************************

LJLIB_CF(string_escXML)
{
   GCstr *s = lj_lib_optstr(L, 1);

   if (not s) { // Handle nil input - return empty string
      setstrV(L, L->top - 1, &G(L)->strempty);
      return 1;
   }

   auto str = strdata(s);
   MSize len = s->len;
   SBuf *sb = lj_buf_tmp_(L);
   MSize i;

   lj_buf_reset(sb);

   for (i = 0; i < len; i++) {
      char c = str[i];
      switch (c) {
         case '&': lj_buf_putmem(sb, "&amp;", 5); break;
         case '<': lj_buf_putmem(sb, "&lt;", 4); break;
         case '>': lj_buf_putmem(sb, "&gt;", 4); break;
         default: lj_buf_putb(sb, c); break;
      }
   }

   setstrV(L, L->top - 1, lj_buf_str(L, sb));
   lj_gc_check(L);
   return 1;
}

//********************************************************************************************************************

LJLIB_ASM(string_reverse)  LJLIB_REC(string_op IRCALL_lj_buf_putstr_reverse)
{
   lj_lib_checkstr(L, 1);
   return FFH_RETRY;
}
LJLIB_ASM_(string_lower)  LJLIB_REC(string_op IRCALL_lj_buf_putstr_lower)
LJLIB_ASM_(string_upper)  LJLIB_REC(string_op IRCALL_lj_buf_putstr_upper)

//********************************************************************************************************************

static int writer_buf(lua_State *L, const void *p, size_t size, void *sb)
{
   lj_buf_putmem((SBuf*)sb, p, (MSize)size);
   return 0;
}

//********************************************************************************************************************

LJLIB_CF(string_dump)
{
   GCfunc* fn = lj_lib_checkfunc(L, 1);
   int strip = L->base + 1 < L->top and tvistruecond(L->base + 1);
   SBuf* sb = lj_buf_tmp_(L);  //  Assumes lj_bcwrite() doesn't use tmpbuf.
   L->top = L->base + 1;
   if (not isluafunc(fn) or lj_bcwrite(L, funcproto(fn), writer_buf, sb, strip)) lj_err_caller(L, ErrMsg::STRDUMP);
   setstrV(L, L->top - 1, lj_buf_str(L, sb));
   lj_gc_check(L);
   return 1;
}

//********************************************************************************************************************
// macro to `unsign' a character

#define uchar(c)   ((unsigned char)(c))

#define CAP_UNFINISHED (-1)
#define CAP_POSITION   (-2)

typedef struct MatchState {
   CSTRING src_init;  //  init of source string
   CSTRING src_end;  //  end (`\0') of source string
   lua_State *L;
   int level;  //  total number of captures (finished or unfinished)
   int depth;
   struct {
      CSTRING init;
      ptrdiff_t len;
   } capture[LUA_MAXCAPTURES];
} MatchState;

static int check_capture(MatchState* ms, int l)
{
   l -= '1';
   if (l < 0 or l >= ms->level or ms->capture[l].len IS CAP_UNFINISHED)
      lj_err_caller(ms->L, ErrMsg::STRCAPI);
   return l;
}

static int capture_to_close(MatchState* ms)
{
   int level = ms->level;
   for (level--; level >= 0; level--)
      if (ms->capture[level].len IS CAP_UNFINISHED) return level;
   lj_err_caller(ms->L, ErrMsg::STRPATC);
   return 0;  //  unreachable
}

static CSTRING classend(MatchState* ms, CSTRING p)
{
   switch (*p++) {
   case L_ESC:
      if (*p IS '\0')
         lj_err_caller(ms->L, ErrMsg::STRPATE);
      return p + 1;
   case '[':
      if (*p IS '^') p++;
      do {  // look for a `]'
         if (*p IS '\0')
            lj_err_caller(ms->L, ErrMsg::STRPATM);
         if (*(p++) IS L_ESC and *p != '\0')
            p++;  //  skip escapes (e.g. `%]')
      } while (*p != ']');
      return p + 1;
   default:
      return p;
   }
}

//********************************************************************************************************************

static const unsigned char match_class_map[32] = {
  0,LJ_CHAR_ALPHA,0,LJ_CHAR_CNTRL,LJ_CHAR_DIGIT,0,0,LJ_CHAR_GRAPH,0,0,0,0,
  LJ_CHAR_LOWER,0,0,0,LJ_CHAR_PUNCT,0,0,LJ_CHAR_SPACE,0,
  LJ_CHAR_UPPER,0,LJ_CHAR_ALNUM,LJ_CHAR_XDIGIT,0,0,0,0,0,0,0
};

static int match_class(int c, int cl)
{
   if ((cl & 0xc0) IS 0x40) {
      int t = match_class_map[(cl & 0x1f)];
      if (t) {
         t = lj_char_isa(c, t);
         return (cl & 0x20) ? t : !t;
      }
      if (cl IS 'z') return c IS 0;
      if (cl IS 'Z') return c != 0;
   }
   return (cl IS c);
}

//********************************************************************************************************************

static int matchbracketclass(int c, CSTRING p, CSTRING ec)
{
   int sig = 1;
   if (*(p + 1) IS '^') {
      sig = 0;
      p++;  //  skip the `^'
   }
   while (++p < ec) {
      if (*p IS L_ESC) {
         p++;
         if (match_class(c, uchar(*p)))
            return sig;
      }
      else if ((*(p + 1) IS '-') and (p + 2 < ec)) {
         p += 2;
         if (uchar(*(p - 2)) <= c and c <= uchar(*p)) return sig;
      }
      else if (uchar(*p) IS c) return sig;
   }
   return !sig;
}

//********************************************************************************************************************

static int singlematch(int c, CSTRING p, CSTRING ep)
{
   switch (*p) {
      case '.':   return 1;  //  matches any char
      case L_ESC: return match_class(c, uchar(*(p + 1)));
      case '[':   return matchbracketclass(c, p, ep - 1);
      default:    return (uchar(*p) IS c);
   }
}

static CSTRING match(MatchState* ms, CSTRING s, CSTRING p);

//********************************************************************************************************************

static CSTRING matchbalance(MatchState* ms, CSTRING s, CSTRING p)
{
   if (*p IS 0 or *(p + 1) IS 0) lj_err_caller(ms->L, ErrMsg::STRPATU);

   if (*s != *p) return nullptr;
   else {
      int b = *p;
      int e = *(p + 1);
      int cont = 1;
      while (++s < ms->src_end) {
         if (*s IS e) {
            if (--cont IS 0) return s + 1;
         }
         else if (*s IS b) cont++;
      }
   }
   return nullptr;  //  string ends out of balance
}

//********************************************************************************************************************

static CSTRING max_expand(MatchState* ms, CSTRING s, CSTRING p, CSTRING ep)
{
   ptrdiff_t i = 0;  //  counts maximum expand for item
   while ((s + i) < ms->src_end and singlematch(uchar(*(s + i)), p, ep)) i++;
   // keeps trying to match with the maximum repetitions
   while (i >= 0) {
      CSTRING res = match(ms, (s + i), ep + 1);
      if (res) return res;
      i--;  //  else didn't match; reduce 1 repetition to try again
   }
   return nullptr;
}

//********************************************************************************************************************

static CSTRING min_expand(MatchState* ms, CSTRING s, CSTRING p, CSTRING ep)
{
   while (true) {
      CSTRING res = match(ms, s, ep + 1);
      if (res != nullptr) return res;
      else if (s < ms->src_end and singlematch(uchar(*s), p, ep)) s++;  //  try with one more repetition
      else return nullptr;
   }
}

//********************************************************************************************************************

static CSTRING start_capture(MatchState *State, CSTRING s, CSTRING p, int what)
{
   CSTRING res;
   auto level = State->level;
   if (level >= LUA_MAXCAPTURES) lj_err_caller(State->L, ErrMsg::STRCAPN);
   State->capture[level].init = s;
   State->capture[level].len = what;
   State->level = level + 1;
   if ((res = match(State, s, p)) IS nullptr) State->level--;
   return res;
}

//********************************************************************************************************************

static CSTRING end_capture(MatchState* State, CSTRING s, CSTRING p)
{
   int l = capture_to_close(State);
   CSTRING res;
   State->capture[l].len = s - State->capture[l].init;  //  close capture
   if ((res = match(State, s, p)) IS nullptr)  //  match failed?
      State->capture[l].len = CAP_UNFINISHED;  //  undo capture
   return res;
}

//********************************************************************************************************************

static CSTRING match_capture(MatchState* State, CSTRING s, int l)
{
   size_t len;
   l = check_capture(State, l);
   len = (size_t)State->capture[l].len;
   if ((size_t)(State->src_end - s) >= len && memcmp(State->capture[l].init, s, len) IS 0) return s + len;
   else return nullptr;
}

//********************************************************************************************************************

static CSTRING match(MatchState* State, CSTRING s, CSTRING p)
{
   if (++State->depth > LJ_MAX_XLEVEL) lj_err_caller(State->L, ErrMsg::STRPATX);

init: //  using goto's to optimize tail recursion

   switch (*p) {
      case '(':  //  start capture
         if (*(p + 1) IS ')') s = start_capture(State, s, p + 2, CAP_POSITION);
         else s = start_capture(State, s, p + 1, CAP_UNFINISHED);
         break;
      case ')':  //  end capture
         s = end_capture(State, s, p + 1);
         break;
      case L_ESC:
         switch (*(p + 1)) {
         case 'b':  //  balanced string?
            s = matchbalance(State, s, p + 2);
            if (s IS nullptr) break;
            p += 4;
            goto init;  //  else s = match(State, s, p+4);
         case 'f': {  // frontier?
            CSTRING ep; char previous;
            p += 2;
            if (*p != '[') lj_err_caller(State->L, ErrMsg::STRPATB);
            ep = classend(State, p);  //  points to what is next
            previous = (s IS State->src_init) ? '\0' : *(s - 1);
            if (matchbracketclass(uchar(previous), p, ep - 1) ||
               !matchbracketclass(uchar(*s), p, ep - 1)) {
               s = nullptr; break;
            }
            p = ep;
            goto init;  //  else s = match(State, s, ep);
         }
         default:
            if (isdigit(uchar(*(p + 1)))) {  // capture results (%0-%9)?
               s = match_capture(State, s, uchar(*(p + 1)));
               if (s IS nullptr) break;
               p += 2;
               goto init;  //  else s = match(State, s, p+2)
            }
            goto dflt;  //  case default
         }
         break;
      case '\0':  // end of pattern
         break;  // match succeeded
      case '$':
         // is the `$' the last char in pattern?
         if (*(p + 1) != '\0') goto dflt;
         if (s != State->src_end) s = nullptr;  //  check end of string
         break;
      default: dflt: {  // it is a pattern item
         CSTRING ep = classend(State, p);  //  points to what is next
         int m = s < State->src_end and singlematch(uchar(*s), p, ep);
         switch (*ep) {
         case '?': {  // optional
            CSTRING res;
            if (m and ((res = match(State, s + 1, ep + 1)) != nullptr)) {
               s = res;
               break;
            }
            p = ep + 1;
            goto init;  // else s = match(State, s, ep+1);
         }
         case '*':  // 0 or more repetitions
            s = max_expand(State, s, p, ep);
            break;
         case '+':  // 1 or more repetitions
            s = (m ? max_expand(State, s + 1, p, ep) : nullptr);
            break;
         case '-':  // 0 or more repetitions (minimum)
            s = min_expand(State, s, p, ep);
            break;
         default:
            if (m) { s++; p = ep; goto init; }  // else s = match(State, s+1, ep);
            s = nullptr;
            break;
         }
         break;
      }
   }

   State->depth--;
   return s;
}

//********************************************************************************************************************

static void push_onecapture(MatchState* State, int i, CSTRING s, CSTRING e)
{
   if (i >= State->level) {
      if (i IS 0) { //  State->level IS 0, too
         lua_pushlstring(State->L, s, (size_t)(e - s));  //  add whole match
      }
      else lj_err_caller(State->L, ErrMsg::STRCAPI);
   }
   else {
      ptrdiff_t l = State->capture[i].len;
      if (l IS CAP_UNFINISHED) lj_err_caller(State->L, ErrMsg::STRCAPU);
      if (l IS CAP_POSITION) lua_pushinteger(State->L, State->capture[i].init - State->src_init);  // 0-based position
      else lua_pushlstring(State->L, State->capture[i].init, (size_t)l);
   }
}

//********************************************************************************************************************

static int push_captures(MatchState* State, CSTRING S, CSTRING E)
{
   int nlevels = (State->level IS 0 and S) ? 1 : State->level;
   luaL_checkstack(State->L, nlevels, "too many captures");
   for (int i = 0; i < nlevels; i++) push_onecapture(State, i, S, E);
   return nlevels;  //  number of strings pushed
}

//********************************************************************************************************************

static int str_find_aux(lua_State *L, int Find)
{
   GCstr *s = lj_lib_checkstr(L, 1);
   GCstr *p = lj_lib_checkstr(L, 2);
   int32_t start = lj_lib_optint(L, 3, 0);  // 0-based: default start at 0
   MSize st;
   if (start < 0) start += (int32_t)s->len;  // 0-based: -1 → len-1
   if (start < 0) start = 0;
   st = (MSize)start;
   if (st > s->len) {
      setnilV(L->top - 1);
      return 1;
   }

   if (Find and ((L->base + 3 < L->top and tvistruecond(L->base + 3)) || !lj_str_haspattern(p))) {  // Search for fixed string.
      CSTRING q = lj_str_find(strdata(s) + st, strdata(p), s->len - st, p->len);
      if (q) {
         setintV(L->top - 2, (int32_t)(q - strdata(s)));  // 0-based start
         setintV(L->top - 1, (int32_t)(q - strdata(s)) + (int32_t)p->len - 1);  // 0-based end (inclusive)
         return 2;
      }
   }
   else {  // Search for pattern using regex.
      // Ensure regex module is loaded
      if (load_regex_module() != ERR::Okay) {
         lj_err_caller(L, ErrMsg::STRCAPI);  // Use available error code
         return 0;
      }

      // Convert Lua pattern to regex
      PatternConversionResult conv;
      if (convert_lua_pattern_to_regex(strdata(p), p->len, conv) != ERR::Okay) {
         lj_err_callermsg(L, conv.error_message.c_str());
         return 0;
      }

      // Compile regex
      std::string error_msg;
      Regex* regex = nullptr;
      if (rx::Compile(conv.regex_pattern, REGEX::NIL, &error_msg, &regex) != ERR::Okay) {
         lj_err_callermsg(L, error_msg.c_str());
         return 0;
      }

      // Search from the specified start position
      std::string_view subject(strdata(s) + st, s->len - st);
      FindMatchContext ctx;
      ctx.src_init = strdata(s);
      ctx.offset = st;
      auto cb = C_FUNCTION(find_match_callback, &ctx);

      // Set flags based on anchoring
      RMATCH flags = RMATCH::NIL;
      if (conv.has_start_anchor) flags = flags | RMATCH::CONTINUOUS;

      rx::Search(regex, subject, flags, &cb);
      FreeResource(regex);

      if (ctx.matched) {
         // Adjust match positions to account for start offset (convert to absolute positions)
         size_t abs_start = st + ctx.match_start;
         size_t abs_end = st + ctx.match_end - 1;  // Convert to inclusive end

         if (Find) {
            setintV(L->top++, int32_t(abs_start));
            setintV(L->top++, int32_t(abs_end));
            return push_regex_captures(L, ctx, conv.position_capture_indices, Find) + 2;
         }
         else {
            return push_regex_captures(L, ctx, conv.position_capture_indices, Find);
         }
      }
   }
   setnilV(L->top - 1);  //  Not found.
   return 1;
}

//********************************************************************************************************************

LJLIB_CF(string_find)      LJLIB_REC(.)
{
   return str_find_aux(L, 1);
}

//********************************************************************************************************************

LJLIB_CF(string_match)
{
   return str_find_aux(L, 0);
}

//********************************************************************************************************************

LJLIB_NOREG LJLIB_CF(string_gmatch_aux)
{
   CSTRING p = strVdata(lj_lib_upvalue(L, 2));
   GCstr *str = strV(lj_lib_upvalue(L, 1));
   CSTRING s = strdata(str);
   TValue *tvpos = lj_lib_upvalue(L, 3);
   CSTRING src = s + tvpos->u32.lo;
   MatchState ms;
   ms.L = L;
   ms.src_init = s;
   ms.src_end = s + str->len;
   for (; src <= ms.src_end; src++) {
      CSTRING e;
      ms.level = ms.depth = 0;
      if ((e = match(&ms, src, p)) != nullptr) {
         int32_t pos = (int32_t)(e - s);
         if (e IS src) pos++;  //  Ensure progress for empty match.
         tvpos->u32.lo = (uint32_t)pos;
         return push_captures(&ms, src, e);
      }
   }
   return 0;  //  not found
}

//********************************************************************************************************************

LJLIB_CF(string_gmatch)
{
   lj_lib_checkstr(L, 1);
   lj_lib_checkstr(L, 2);
   L->top = L->base + 3;
   (L->top - 1)->u64 = 0;
   lj_lib_pushcc(L, lj_cf_string_gmatch_aux, FF_string_gmatch_aux, 3);
   return 1;
}

//********************************************************************************************************************

static void add_s(MatchState* ms, luaL_Buffer* b, CSTRING s, CSTRING e)
{
   size_t l, i;
   CSTRING news = lua_tolstring(ms->L, 3, &l);
   for (i = 0; i < l; i++) {
      if (news[i] != L_ESC) luaL_addchar(b, news[i]);
      else {
         i++;  //  skip ESC
         if (not isdigit(uchar(news[i]))) luaL_addchar(b, news[i]);
         else if (news[i] IS '0') luaL_addlstring(b, s, (size_t)(e - s));
         else {
            push_onecapture(ms, news[i] - '1', s, e);
            luaL_addvalue(b);  //  add capture to accumulated result
         }
      }
   }
}

//********************************************************************************************************************

static void add_value(MatchState* ms, luaL_Buffer* b, CSTRING s, CSTRING e)
{
   lua_State *L = ms->L;
   switch (lua_type(L, 3)) {
      case LUA_TNUMBER:
      case LUA_TSTRING:
         add_s(ms, b, s, e);
         return;

      case LUA_TFUNCTION: {
         int n;
         lua_pushvalue(L, 3);
         n = push_captures(ms, s, e);
         lua_call(L, n, 1);
         break;
      }

      case LUA_TTABLE:
         push_onecapture(ms, 0, s, e);
         lua_gettable(L, 3);
         break;
   }

   if (not lua_toboolean(L, -1)) {  // nil or false?
      lua_pop(L, 1);
      lua_pushlstring(L, s, (size_t)(e - s));  //  keep original text
   }
   else if (not lua_isstring(L, -1)) {
      lj_err_callerv(L, ErrMsg::STRGSRV, luaL_typename(L, -1));
   }
   luaL_addvalue(b);  //  add result to accumulator
}

//********************************************************************************************************************

LJLIB_CF(string_gsub)
{
   size_t srcl;
   CSTRING src = luaL_checklstring(L, 1, &srcl);
   CSTRING p = luaL_checkstring(L, 2);
   int  tr = lua_type(L, 3);
   int max_s = luaL_optint(L, 4, (int)(srcl + 1));
   int anchor = (*p IS '^') ? (p++, 1) : 0;
   int n = 0;
   MatchState ms;
   luaL_Buffer b;
   if (not (tr IS LUA_TNUMBER or tr IS LUA_TSTRING || tr IS LUA_TFUNCTION or tr IS LUA_TTABLE)) lj_err_arg(L, 3, ErrMsg::NOSFT);
   luaL_buffinit(L, &b);
   ms.L = L;
   ms.src_init = src;
   ms.src_end = src + srcl;
   while (n < max_s) {
      CSTRING e;
      ms.level = ms.depth = 0;
      e = match(&ms, src, p);
      if (e) {
         n++;
         add_value(&ms, &b, src, e);
      }
      if (e and e > src) //  non empty match?
         src = e;  //  skip it
      else if (src < ms.src_end) luaL_addchar(&b, *src++);
      else break;
      if (anchor) break;
   }
   luaL_addlstring(&b, src, (size_t)(ms.src_end - src));
   luaL_pushresult(&b);
   lua_pushinteger(L, n);  //  number of substitutions
   return 2;
}

//********************************************************************************************************************

LJLIB_CF(string_format)      LJLIB_REC(.)
{
   int retry = 0;
   SBuf* sb;
   do {
      sb = lj_buf_tmp_(L);
      retry = lj_strfmt_putarg(L, sb, 1, -retry);
   } while (retry > 0);
   setstrV(L, L->top - 1, lj_buf_str(L, sb));
   lj_gc_check(L);
   return 1;
}

#include "lj_libdef.h"

//********************************************************************************************************************
// Custom __index handler for strings
// Handles numeric keys for single-character access, range userdata for substring extraction,
// and string keys for method lookups (delegated to string library table)

static int string_index_handler(lua_State *L)
{
   // Argument 1: the string
   // Argument 2: the key (number, range userdata, or string)

   if (not tvisstr(L->base)) {
      lua_pushnil(L);
      return 1;
   }

   GCstr *str = strV(L->base);
   int32_t len = (int32_t)str->len;
   cTValue *key = L->base + 1;

   // Check for numeric key (single character access)
   if (tvisnum(key) or tvisint(key)) {
      int32_t idx = lj_lib_checkint(L, 2);

      // Handle negative indices (0-based: -1 means last character)
      if (idx < 0) idx += len;

      // Bounds check
      if (idx < 0 or idx >= len) {
         lua_pushnil(L);
         return 1;
      }

      // Return single character as string
      lua_pushlstring(L, strdata(str) + idx, 1);
      return 1;
   }

   // Check for range userdata (substring extraction)
   fluid_range* r = get_range_from_tvalue(L, key);
   if (r) {
      int32_t start = r->start;
      int32_t stop = r->stop;

      // Handle negative indices (always inclusive for negative ranges, per design doc)
      bool use_inclusive = r->inclusive;
      if (start < 0 or stop < 0) {
         use_inclusive = true;  // Negative indices ignore inclusive flag
         if (start < 0) start += len;
         if (stop < 0) stop += len;
      }

      // Apply exclusive semantics if not inclusive
      // For exclusive ranges, stop is NOT included, so we subtract 1 to make it inclusive
      // for the final substring calculation
      int32_t effective_stop = stop;
      if (not use_inclusive) {
         effective_stop = stop - 1;
      }

      // Bounds checking
      if (start < 0) start = 0;
      if (effective_stop >= len) effective_stop = len - 1;

      // Handle empty/invalid ranges
      if (start > effective_stop or start >= len) {
         lua_pushstring(L, "");
         return 1;
      }

      // Return substring
      int32_t sublen = effective_stop - start + 1;
      lua_pushlstring(L, strdata(str) + start, (size_t)sublen);
      return 1;
   }

   // Check for string key (method lookup)
   if (tvisstr(key)) {
      // Get the string library table (stored in upvalue 1)
      lua_pushvalue(L, lua_upvalueindex(1));
      lua_pushvalue(L, 2);  // Push the key
      lua_rawget(L, -2);    // Get string_lib[key] without metamethods
      return 1;
   }

   // Unknown key type
   lua_pushnil(L);
   return 1;
}

extern int luaopen_string(lua_State *L)
{
   LJ_LIB_REG(L, "string", string);
   // At this point, L->top - 1 has the string library table on the Lua stack

   GCtab *mt = lj_tab_new(L, 0, 1);

   // NOBARRIER: basemt is a GC root.
   global_State *g = G(L);

   // Stores the newly created table mt as the "base metatable" for the string type. basemt_it(g, LJ_TSTR) selects
   // the slot for the string base metatable in the global state, obj2gco(mt) converts the GCtab* to a generic GC
   // object reference, and setgcref writes that GC reference into the global state (so the runtime knows the
   // canonical metatable for strings).

   setgcref(basemt_it(g, LJ_TSTR), obj2gco(mt));

   // Create a closure for string_index_handler with the string library table as upvalue.
   // This allows str[idx], str[{0..5}], and str.method() syntax.
   // Stack after LJ_LIB_REG: [..., string_lib_table] at position -1

   lua_pushvalue(L, -1);  // Push copy of string library table for upvalue
   lua_pushcclosure(L, string_index_handler, 1);  // Create closure with 1 upvalue
   // Stack: [..., string_lib_table, closure]

   // Set the closure as __index metamethod
   TValue *index_slot = lj_tab_setstr(L, mt, mmname_str(g, MM_index));
   setfuncV(L, index_slot, funcV(L->top - 1));
   lua_pop(L, 1);  // Pop the closure
   // Stack: [..., string_lib_table]

   // Update the metatable's negative‑metamethod cache (nomm). The bitwise expression clears the bit
   // corresponding to MM_index (and sets other bits), marking that this metamethod slot should not be treated as
   // "absent" by the fast metamethod check. Practically, this tells the runtime the MM_index metamethod is present
   // (so subsequent lookups/optimisations behave accordingly).
   //
   // NOTE: nomm is an 8‑bit negative cache, so MM_index must fit within those bits; the bit trick is intentional
   // to flip the absence/presence semantics used by the VM.

   mt->nomm = (uint8_t)(~(1u << MM_index));

   // Register string interface prototypes for compile-time type inference
   reg_iface_prototype("string", "len", { FluidType::Num }, { FluidType::Str });
   reg_iface_prototype("string", "sub", { FluidType::Str }, { FluidType::Str, FluidType::Num, FluidType::Num });
   reg_iface_prototype("string", "substr", { FluidType::Str }, { FluidType::Str, FluidType::Num, FluidType::Num });
   reg_iface_prototype("string", "format", { FluidType::Str }, { FluidType::Str }, FProtoFlags::Variadic);
   reg_iface_prototype("string", "upper", { FluidType::Str }, { FluidType::Str });
   reg_iface_prototype("string", "lower", { FluidType::Str }, { FluidType::Str });
   reg_iface_prototype("string", "find", { FluidType::Num, FluidType::Num }, { FluidType::Str, FluidType::Str }, FProtoFlags::Variadic);
   reg_iface_prototype("string", "match", { FluidType::Str }, { FluidType::Str, FluidType::Str });
   reg_iface_prototype("string", "gmatch", { FluidType::Func }, { FluidType::Str, FluidType::Str });
   reg_iface_prototype("string", "gsub", { FluidType::Str, FluidType::Num }, { FluidType::Str, FluidType::Str, FluidType::Any });
   reg_iface_prototype("string", "rep", { FluidType::Str }, { FluidType::Str, FluidType::Num });
   reg_iface_prototype("string", "alloc", { FluidType::Str }, { FluidType::Num });
   reg_iface_prototype("string", "reverse", { FluidType::Str }, { FluidType::Str });
   reg_iface_prototype("string", "byte", { FluidType::Num }, { FluidType::Str, FluidType::Num }, FProtoFlags::Variadic);
   reg_iface_prototype("string", "char", { FluidType::Str }, {}, FProtoFlags::Variadic);
   reg_iface_prototype("string", "dump", { FluidType::Str }, { FluidType::Func });
   reg_iface_prototype("string", "split", { FluidType::Array }, { FluidType::Str, FluidType::Str });
   reg_iface_prototype("string", "trim", { FluidType::Str }, { FluidType::Str });
   reg_iface_prototype("string", "rtrim", { FluidType::Str }, { FluidType::Str });
   reg_iface_prototype("string", "startsWith", { FluidType::Bool }, { FluidType::Str, FluidType::Str });
   reg_iface_prototype("string", "endsWith", { FluidType::Bool }, { FluidType::Str, FluidType::Str });
   reg_iface_prototype("string", "join", { FluidType::Str }, { FluidType::Table, FluidType::Str });
   reg_iface_prototype("string", "cap", { FluidType::Str }, { FluidType::Str });
   reg_iface_prototype("string", "decap", { FluidType::Str }, { FluidType::Str });
   reg_iface_prototype("string", "hash", { FluidType::Num }, { FluidType::Str, FluidType::Bool });
   reg_iface_prototype("string", "escXML", { FluidType::Str }, { FluidType::Str });
   reg_iface_prototype("string", "unescapeXML", { FluidType::Str }, { FluidType::Str });
   // These are implemented in translate.fluid
   reg_iface_prototype("string", "translateRefresh", { }, { });
   reg_iface_prototype("string", "translate", { FluidType::Str }, { FluidType::Str });

   return 1;
}
