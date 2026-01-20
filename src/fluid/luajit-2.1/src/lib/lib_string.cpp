// String library.
// Copyright (C) 2005-2022 Mike Pall. See Copyright Notice in luajit.h
//
// Major portions taken verbatim or adapted from the Lua interpreter.
// Copyright (C) 1994-2008 Lua.org, PUC-Rio. See Copyright Notice in lua.h

#define lib_string_c
#define LUA_LIB

#include <cctype>
#include <array>
#include <string_view>

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

#define L_ESC      '%'

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
   const unsigned char* p;
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

static const char* find_separator(const char *Pos, const char *End, const char *Sep, MSize SepLen, bool IsWhitespace)
{
   if (IsWhitespace) {
      // Whitespace separators: scan the entire range, independent of SepLen.
      for (const char *p = Pos; p < End; p++) {
         if (*p IS ' ' or *p IS '\t' or *p IS '\n' or *p IS '\r') return p;
      }
      return nullptr;
   }

   if (SepLen IS 1) return (const char*)memchr(Pos, Sep[0], End - Pos);

   // Multi-character separator.
   for (const char *p = Pos; p <= End - SepLen; p++) {
      if (memcmp(p, Sep, SepLen) IS 0) return p;
   }
   return nullptr;
}

//********************************************************************************************************************

LJLIB_CF(string_split)
{
   GCstr *s = lj_lib_checkstr(L, 1);
   GCstr *sep = lj_lib_optstr(L, 2);
   const char *str = strdata(s);
   const char *sepstr;
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

   const char *end = str + slen;

   // First pass: count separators to determine array size
   uint32_t count = 1;  // At least one element (final segment)
   const char *pos = str;
   while (pos < end) {
      const char *found = find_separator(pos, end, sepstr, seplen, is_whitespace);
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
      const char *found = find_separator(pos, end, sepstr, seplen, is_whitespace);

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

   const char* str = strdata(s);
   MSize len = s->len;
   const char* start = str;
   const char* end = str + len;

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
   const char* str = strdata(s);
   const char* suffixstr = strdata(suffix);
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
   const char* sepstr = "";
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

#define CAP_UNFINISHED   (-1)
#define CAP_POSITION   (-2)

typedef struct MatchState {
   const char *src_init;  //  init of source string
   const char *src_end;  //  end (`\0') of source string
   lua_State *L;
   int level;  //  total number of captures (finished or unfinished)
   int depth;
   struct {
      const char *init;
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

static const char* classend(MatchState* ms, const char* p)
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

static int matchbracketclass(int c, const char* p, const char* ec)
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

static int singlematch(int c, const char* p, const char* ep)
{
   switch (*p) {
   case '.': return 1;  //  matches any char
   case L_ESC: return match_class(c, uchar(*(p + 1)));
   case '[': return matchbracketclass(c, p, ep - 1);
   default:  return (uchar(*p) IS c);
   }
}

static const char* match(MatchState* ms, const char* s, const char* p);

//********************************************************************************************************************

static const char* matchbalance(MatchState* ms, const char* s, const char* p)
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

static const char* max_expand(MatchState* ms, const char* s, const char* p, const char* ep)
{
   ptrdiff_t i = 0;  //  counts maximum expand for item
   while ((s + i) < ms->src_end and singlematch(uchar(*(s + i)), p, ep)) i++;
   // keeps trying to match with the maximum repetitions
   while (i >= 0) {
      const char* res = match(ms, (s + i), ep + 1);
      if (res) return res;
      i--;  //  else didn't match; reduce 1 repetition to try again
   }
   return nullptr;
}

//********************************************************************************************************************

static const char* min_expand(MatchState* ms, const char* s, const char* p, const char* ep)
{
   while (true) {
      const char* res = match(ms, s, ep + 1);
      if (res != nullptr) return res;
      else if (s < ms->src_end and singlematch(uchar(*s), p, ep)) s++;  //  try with one more repetition
      else return nullptr;
   }
}

//********************************************************************************************************************

static const char* start_capture(MatchState* ms, const char* s, const char* p, int what)
{
   const char* res;
   int level = ms->level;
   if (level >= LUA_MAXCAPTURES) lj_err_caller(ms->L, ErrMsg::STRCAPN);
   ms->capture[level].init = s;
   ms->capture[level].len = what;
   ms->level = level + 1;
   if ((res = match(ms, s, p)) IS nullptr)  //  match failed?
      ms->level--;  //  undo capture
   return res;
}

//********************************************************************************************************************

static const char* end_capture(MatchState* ms, const char* s, const char* p)
{
   int l = capture_to_close(ms);
   const char* res;
   ms->capture[l].len = s - ms->capture[l].init;  //  close capture
   if ((res = match(ms, s, p)) IS nullptr)  //  match failed?
      ms->capture[l].len = CAP_UNFINISHED;  //  undo capture
   return res;
}

//********************************************************************************************************************

static const char* match_capture(MatchState* ms, const char* s, int l)
{
   size_t len;
   l = check_capture(ms, l);
   len = (size_t)ms->capture[l].len;
   if ((size_t)(ms->src_end - s) >= len && memcmp(ms->capture[l].init, s, len) IS 0) return s + len;
   else return nullptr;
}

//********************************************************************************************************************

static const char* match(MatchState* ms, const char* s, const char* p)
{
   if (++ms->depth > LJ_MAX_XLEVEL)
      lj_err_caller(ms->L, ErrMsg::STRPATX);
init: //  using goto's to optimize tail recursion
   switch (*p) {
   case '(':  //  start capture
      if (*(p + 1) IS ')')  //  position capture?
         s = start_capture(ms, s, p + 2, CAP_POSITION);
      else
         s = start_capture(ms, s, p + 1, CAP_UNFINISHED);
      break;
   case ')':  //  end capture
      s = end_capture(ms, s, p + 1);
      break;
   case L_ESC:
      switch (*(p + 1)) {
      case 'b':  //  balanced string?
         s = matchbalance(ms, s, p + 2);
         if (s IS nullptr) break;
         p += 4;
         goto init;  //  else s = match(ms, s, p+4);
      case 'f': {  // frontier?
         const char* ep; char previous;
         p += 2;
         if (*p != '[') lj_err_caller(ms->L, ErrMsg::STRPATB);
         ep = classend(ms, p);  //  points to what is next
         previous = (s IS ms->src_init) ? '\0' : *(s - 1);
         if (matchbracketclass(uchar(previous), p, ep - 1) ||
            !matchbracketclass(uchar(*s), p, ep - 1)) {
            s = nullptr; break;
         }
         p = ep;
         goto init;  //  else s = match(ms, s, ep);
      }
      default:
         if (isdigit(uchar(*(p + 1)))) {  // capture results (%0-%9)?
            s = match_capture(ms, s, uchar(*(p + 1)));
            if (s IS nullptr) break;
            p += 2;
            goto init;  //  else s = match(ms, s, p+2)
         }
         goto dflt;  //  case default
      }
      break;
   case '\0':  //  end of pattern
      break;  //  match succeeded
   case '$':
      // is the `$' the last char in pattern?
      if (*(p + 1) != '\0') goto dflt;
      if (s != ms->src_end) s = nullptr;  //  check end of string
      break;
   default: dflt: {  // it is a pattern item
      const char* ep = classend(ms, p);  //  points to what is next
      int m = s < ms->src_end and singlematch(uchar(*s), p, ep);
      switch (*ep) {
      case '?': {  // optional
         const char* res;
         if (m and ((res = match(ms, s + 1, ep + 1)) != nullptr)) {
            s = res;
            break;
         }
         p = ep + 1;
         goto init;  //  else s = match(ms, s, ep+1);
      }
      case '*':  //  0 or more repetitions
         s = max_expand(ms, s, p, ep);
         break;
      case '+':  //  1 or more repetitions
         s = (m ? max_expand(ms, s + 1, p, ep) : nullptr);
         break;
      case '-':  //  0 or more repetitions (minimum)
         s = min_expand(ms, s, p, ep);
         break;
      default:
         if (m) { s++; p = ep; goto init; }  // else s = match(ms, s+1, ep);
         s = nullptr;
         break;
      }
      break;
   }
   }
   ms->depth--;
   return s;
}

//********************************************************************************************************************

static void push_onecapture(MatchState* ms, int i, const char* s, const char* e)
{
   if (i >= ms->level) {
      if (i IS 0)  //  ms->level IS 0, too
         lua_pushlstring(ms->L, s, (size_t)(e - s));  //  add whole match
      else lj_err_caller(ms->L, ErrMsg::STRCAPI);
   }
   else {
      ptrdiff_t l = ms->capture[i].len;
      if (l IS CAP_UNFINISHED) lj_err_caller(ms->L, ErrMsg::STRCAPU);
      if (l IS CAP_POSITION) lua_pushinteger(ms->L, ms->capture[i].init - ms->src_init);  // 0-based position
      else lua_pushlstring(ms->L, ms->capture[i].init, (size_t)l);
   }
}

//********************************************************************************************************************

static int push_captures(MatchState* ms, const char* s, const char* e)
{
   int i;
   int nlevels = (ms->level IS 0 and s) ? 1 : ms->level;
   luaL_checkstack(ms->L, nlevels, "too many captures");
   for (i = 0; i < nlevels; i++) push_onecapture(ms, i, s, e);
   return nlevels;  //  number of strings pushed
}

//********************************************************************************************************************

static int str_find_aux(lua_State *L, int find)
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

   if (find and ((L->base + 3 < L->top and tvistruecond(L->base + 3)) || !lj_str_haspattern(p))) {  // Search for fixed string.
      const char* q = lj_str_find(strdata(s) + st, strdata(p), s->len - st, p->len);
      if (q) {
         setintV(L->top - 2, (int32_t)(q - strdata(s)));  // 0-based start
         setintV(L->top - 1, (int32_t)(q - strdata(s)) + (int32_t)p->len - 1);  // 0-based end (inclusive)
         return 2;
      }
   }
   else {  // Search for pattern.
      MatchState ms;
      const char* pstr = strdata(p);
      const char* sstr = strdata(s) + st;
      int anchor = 0;
      if (*pstr IS '^') { pstr++; anchor = 1; }
      ms.L = L;
      ms.src_init = strdata(s);
      ms.src_end = strdata(s) + s->len;
      do {  // Loop through string and try to match the pattern.
         const char* q;
         ms.level = ms.depth = 0;
         q = match(&ms, sstr, pstr);
         if (q) {
            if (find) {
               setintV(L->top++, (int32_t)(sstr - strdata(s)));  // 0-based start
               setintV(L->top++, (int32_t)(q - strdata(s)) - 1);  // 0-based end (inclusive)
               return push_captures(&ms, nullptr, nullptr) + 2;
            }
            else {
               return push_captures(&ms, sstr, q);
            }
         }
      } while (sstr++ < ms.src_end and !anchor);
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
   const char* p = strVdata(lj_lib_upvalue(L, 2));
   GCstr *str = strV(lj_lib_upvalue(L, 1));
   const char* s = strdata(str);
   TValue *tvpos = lj_lib_upvalue(L, 3);
   const char *src = s + tvpos->u32.lo;
   MatchState ms;
   ms.L = L;
   ms.src_init = s;
   ms.src_end = s + str->len;
   for (; src <= ms.src_end; src++) {
      const char* e;
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

static void add_s(MatchState* ms, luaL_Buffer* b, const char* s, const char* e)
{
   size_t l, i;
   const char* news = lua_tolstring(ms->L, 3, &l);
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

static void add_value(MatchState* ms, luaL_Buffer* b, const char* s, const char* e)
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
   const char* src = luaL_checklstring(L, 1, &srcl);
   const char* p = luaL_checkstring(L, 2);
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
      const char* e;
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
   reg_iface_prototype("string", "format", { FluidType::Str }, { FluidType::Str }, FProtoFlags::Variadic);
   reg_iface_prototype("string", "upper", { FluidType::Str }, { FluidType::Str });
   reg_iface_prototype("string", "lower", { FluidType::Str }, { FluidType::Str });
   reg_iface_prototype("string", "find", { FluidType::Num, FluidType::Num }, { FluidType::Str, FluidType::Str });
   reg_iface_prototype("string", "match", { FluidType::Str }, { FluidType::Str, FluidType::Str });
   reg_iface_prototype("string", "gsub", { FluidType::Str, FluidType::Num }, { FluidType::Str, FluidType::Str, FluidType::Any });
   reg_iface_prototype("string", "rep", { FluidType::Str }, { FluidType::Str, FluidType::Num });
   reg_iface_prototype("string", "reverse", { FluidType::Str }, { FluidType::Str });
   reg_iface_prototype("string", "byte", { FluidType::Num }, { FluidType::Str, FluidType::Num }, FProtoFlags::Variadic);
   reg_iface_prototype("string", "char", { FluidType::Str }, {}, FProtoFlags::Variadic);
   reg_iface_prototype("string", "dump", { FluidType::Str }, { FluidType::Func });

   return 1;
}
