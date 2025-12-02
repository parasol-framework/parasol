/*
** String library.
** Copyright (C) 2005-2022 Mike Pall. See Copyright Notice in luajit.h
**
** Major portions taken verbatim or adapted from the Lua interpreter.
** Copyright (C) 1994-2008 Lua.org, PUC-Rio. See Copyright Notice in lua.h
*/

#define lib_string_c
#define LUA_LIB

#include <cctype>

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
#include "lib.h"

// NOTE: Any string function marked with the ASM macro uses a custom assembly implementation in the
// .dasc files.  Changing the C++ code here will have no effect in such cases.

#define LJLIB_MODULE_string

LJLIB_LUA(string_len) /*
  function(s)
    CHECK_str(s)
    return #s
  end
*/

// NOTE: ASM version exists

LJLIB_ASM(string_byte)      LJLIB_REC(string_range 0)
{
   GCstr* s = lj_lib_checkstr(L, 1);
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
   if ((uint32_t)n > LUAI_MAXCSTACK)
      lj_err_caller(L, ErrMsg::STRSLC);
   lj_state_checkstack(L, (MSize)n);
   p = (const unsigned char*)strdata(s) + start;
   for (i = 0; i < n; i++)
      setintV(L->base + i - 1 - LJ_FR2, p[i]);
   return FFH_RES(n);
}

// NOTE: ASM version exists

LJLIB_ASM(string_char)      LJLIB_REC(.)
{
   int i, nargs = (int)(L->top - L->base);
   char* buf = lj_buf_tmp(L, (MSize)nargs);
   for (i = 1; i <= nargs; i++) {
      int32_t k = lj_lib_checkint(L, i);
      if (!checku8(k)) lj_err_arg(L, i, ErrMsg::BADVAL);
      buf[i - 1] = (char)k;
   }
   setstrV(L, L->base - 1 - LJ_FR2, lj_str_new(L, buf, (size_t)nargs));
   return FFH_RES(1);
}

// NOTE: Backed by an ASM implementation
// If you switch to the C implementation then you need to reduce GG_NUM_ASMFF in lj_dispatch.h

#if 1
// string_sub:	Declares an assembly ffunc as its primary implementation. The C code that follows is the fallback (called when the ffunc jumps to ->fff_fallback).
// string_range 1: Tells the JIT recorder how to handle this function. string_range is the recorder function name, 1 is a parameter distinguishing it from other range operations.

LJLIB_ASM(string_sub)      LJLIB_REC(string_range 1)
{
   lj_lib_checkstr(L, 1);
   lj_lib_checkint(L, 2);
   setintV(L->base + 2, lj_lib_optint(L, 3, -1));
   return FFH_RETRY;
}
#else
LJ_LIB_CF(string_sub)
{
   GCstr* s = lj_lib_checkstr(L, 1);
   int32_t len = (int32_t)s->len;
   int32_t start = lj_lib_checkint(L, 2);
   int32_t end = lj_lib_optint(L, 3, -1);

   if (end < 0) end += len;
   if (start < 0) start += len;
   if (start < 0) start = 0;
   if (end > len - 1) end = len - 1;
   if (start > end) {
      setstrV(L, L->top - 1, &G(L)->strempty);
      return 1;
   }

   int32_t sublen = end - start + 1;
   GCstr* result = lj_str_new(L, strdata(s) + start, (size_t)sublen);
   setstrV(L, L->top - 1, result);
   lj_gc_check(L);
   return 1;
}
#endif

// NOTE: Backed by an ASM implementation
// If you switch to the C++ implementation then you need to reduce GG_NUM_ASMFF in lj_dispatch.h
//
// string.substr() is identical to string.sub() except that the end parameter is exclusive.
// This matches the behaviour of JavaScript's substring() and Python's slicing.

#if 1
LJLIB_ASM(string_substr)      LJLIB_REC(string_range 1)
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
#else
LJ_LIB_CF(string_substr)
{
   GCstr* s = lj_lib_checkstr(L, 1);
   int32_t len = (int32_t)s->len;
   int32_t start = lj_lib_checkint(L, 2);
   int32_t end = lj_lib_optint(L, 3, len);  // Default to length for exclusive semantics

   // Handle negative indices first (before exclusive-to-inclusive conversion)
   if (end < 0) end += len;
   if (start < 0) start += len;

   // Convert exclusive end to inclusive by subtracting 1
   // This must happen AFTER negative index conversion
   end--;

   // Clamp to valid range
   if (start < 0) start = 0;
   if (end > len - 1) end = len - 1;
   if (start > end) {
      setstrV(L, L->top - 1, &G(L)->strempty);
      return 1;
   }

   int32_t sublen = end - start + 1;
   GCstr* result = lj_str_new(L, strdata(s) + start, (size_t)sublen);
   setstrV(L, L->top - 1, result);
   lj_gc_check(L);
   return 1;
}
#endif

LJLIB_CF(string_rep)      LJLIB_REC(.)
{
   GCstr* s = lj_lib_optstr(L, 1);
   if (!s) s = &G(L)->strempty;
   int32_t rep = lj_lib_checkint(L, 2);
   GCstr* sep = lj_lib_optstr(L, 3);
   SBuf* sb = lj_buf_tmp_(L);
   if (sep and rep > 1) {
      GCstr* s2 = lj_buf_cat2str(L, sep, s);
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
   if (size < 0) lj_err_arg(L, 1, ErrMsg::NUMRNG);
   SBuf* sb = lj_buf_tmp_(L);
   lj_buf_reset(sb);
   lj_buf_need(sb, (MSize)size);
   sb->w += size;  //  Advance write pointer to reserve space
   setstrV(L, L->top - 1, lj_buf_str(L, sb));
   lj_gc_check(L);
   return 1;
}

LJLIB_CF(string_split)
{
   GCstr* s = lj_lib_checkstr(L, 1);
   GCstr* sep = lj_lib_optstr(L, 2);
   const char* str = strdata(s);
   const char* sepstr;
   MSize seplen;
   MSize slen = s->len;
   GCtab* t;
   int32_t idx = 0;  // 0-based

   if ((!sep) or (sep->len == 0)) {
      sepstr = " \t\n\r";  //  Default whitespace separators
      seplen = 4;
   }
   else {
      sepstr = strdata(sep);
      seplen = sep->len;
   }

   lua_createtable(L, 8, 0);  //  Initial array size estimate
   t = tabV(L->top - 1);

   if (slen == 0) return 1;  //  Return empty table for empty string

   const char* start = str;
   const char* end = str + slen;
   const char* pos = start;

   while (pos <= end) {
      const char* found = nullptr;

      // Find next separator
      if (seplen == 1) {
         found = (const char*)memchr(pos, sepstr[0], end - pos);
      }
      else {
         // Multi-character separator or whitespace
         for (const char* p = pos; p <= end - seplen; p++) {
            if (seplen == 4 and (*p == ' ' or *p == '\t' or *p == '\n' or *p == '\r')) {
               found = p;
               break;
            }
            else if (memcmp(p, sepstr, seplen) == 0) {
               found = p;
               break;
            }
         }
      }

      if (found) {
         // Add substring to table
         GCstr* substr = lj_str_new(L, pos, found - pos);
         setstrV(L, lj_tab_setint(L, t, idx), substr);
         idx++;
         pos = found + (seplen == 4 ? 1 : seplen);  //  Skip separator
      }
      else {
         // Add final substring
         GCstr* substr = lj_str_new(L, pos, end - pos);
         setstrV(L, lj_tab_setint(L, t, idx), substr);
         idx++;
         break;
      }
   }

   lj_gc_check(L);
   return 1;
}

LJLIB_CF(string_trim)
{
   GCstr* s = lj_lib_optstr(L, 1);
   if (!s) {
      setstrV(L, L->top - 1, &G(L)->strempty);
      return 1;
   }

   const char* str = strdata(s);
   MSize len = s->len;
   const char* start = str;
   const char* end = str + len;

   if (len == 0) {
      setstrV(L, L->top - 1, &G(L)->strempty);
      return 1;
   }

   // Skip leading whitespace
   while (start < end and (*start == ' ' or *start == '\t' or *start == '\n' or *start == '\r'))
      start++;

   // Skip trailing whitespace
   while (end > start and (end[-1] == ' ' or end[-1] == '\t' or end[-1] == '\n' or end[-1] == '\r'))
      end--;

   // If all whitespace, return empty string
   if (start >= end) {
      setstrV(L, L->top - 1, &G(L)->strempty);
      return 1;
   }

   // Create trimmed string
   GCstr* result = lj_str_new(L, start, end - start);
   setstrV(L, L->top - 1, result);
   lj_gc_check(L);
   return 1;
}

LJLIB_CF(string_rtrim)
{
   GCstr* s = lj_lib_optstr(L, 1);
   if (!s) {
      setstrV(L, L->top - 1, &G(L)->strempty);
      return 1;
   }

   const char* str = strdata(s);
   MSize len = s->len;
   const char* end = str + len;

   if (len == 0) {
      setstrV(L, L->top - 1, s);  //  Return original empty string
      return 1;
   }

   // Find end of non-whitespace
   while (end > str and (end[-1] == ' ' or end[-1] == '\t' or end[-1] == '\n' or end[-1] == '\r'))
      end--;

   // Create right-trimmed string
   GCstr* result = lj_str_new(L, str, end - str);
   setstrV(L, L->top - 1, result);
   lj_gc_check(L);
   return 1;
}

LJLIB_CF(string_startsWith)
{
   GCstr* s = lj_lib_checkstr(L, 1);
   GCstr* prefix = lj_lib_checkstr(L, 2);
   const char* str = strdata(s);
   const char* prefixstr = strdata(prefix);
   MSize slen = s->len;
   MSize prefixlen = prefix->len;

   // Empty prefix always matches
   if (prefixlen == 0) {
      setboolV(L->top - 1, 1);
      return 1;
   }

   // Prefix longer than string cannot match
   if (prefixlen > slen) {
      setboolV(L->top - 1, 0);
      return 1;
   }

   // Compare prefix with start of string
   int matches = (memcmp(str, prefixstr, prefixlen) == 0);
   setboolV(L->top - 1, matches);
   return 1;
}

LJLIB_CF(string_endsWith)
{
   GCstr* s = lj_lib_checkstr(L, 1);
   GCstr* suffix = lj_lib_checkstr(L, 2);
   const char* str = strdata(s);
   const char* suffixstr = strdata(suffix);
   MSize slen = s->len;
   MSize suffixlen = suffix->len;

   // Empty suffix always matches
   if (suffixlen == 0) {
      setboolV(L->top - 1, 1);
      return 1;
   }

   // Suffix longer than string cannot match
   if (suffixlen > slen) {
      setboolV(L->top - 1, 0);
      return 1;
   }

   // Compare suffix with end of string
   int matches = (memcmp(str + slen - suffixlen, suffixstr, suffixlen) == 0);
   setboolV(L->top - 1, matches);
   return 1;
}

LJLIB_CF(string_join)
{
   GCtab* t = lj_lib_checktab(L, 1);
   GCstr* sep = lj_lib_optstr(L, 2);
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
      cTValue* tv = lj_tab_getint(t, i);
      if (tv and !tvisnil(tv)) {
         int isValidType = 0;

         // Check if we have a valid type to process
         if (tvisstr(tv) or tvisnum(tv)) {
            isValidType = 1;
         }

         if (isValidType) {
            // Add separator before non-first elements
            if (sb->w > sb->b and seplen > 0) {
               lj_buf_putmem(sb, sepstr, seplen);
            }

            if (tvisstr(tv)) {
               // Add string content
               lj_buf_putstr(sb, strV(tv));
            }
            else if (tvisnum(tv)) {
               // Convert number to string directly into buffer
               sb = lj_strfmt_putnum(sb, tv);
            }
         }
      }
   }

   setstrV(L, L->top - 1, lj_buf_str(L, sb));
   lj_gc_check(L);
   return 1;
}

LJLIB_CF(string_cap)
{
   GCstr* s = lj_lib_checkstr(L, 1);
   const char* str = strdata(s);
   MSize len = s->len;

   if (len == 0) {
      setstrV(L, L->top - 1, &G(L)->strempty);
      return 1;
   }

   // Create new string with first character uppercased
   SBuf* sb = lj_buf_tmp_(L);
   lj_buf_reset(sb);

   // Convert first character to uppercase
   char first = str[0];
   if (first >= 'a' and first <= 'z') {
      first = first - 32;  //  Convert to uppercase
   }
   lj_buf_putb(sb, first);

   // Add remaining characters unchanged
   if (len > 1) lj_buf_putmem(sb, str + 1, len - 1);

   setstrV(L, L->top - 1, lj_buf_str(L, sb));
   lj_gc_check(L);
   return 1;
}

LJLIB_CF(string_decap)
{
   GCstr* s = lj_lib_checkstr(L, 1);
   const char* str = strdata(s);
   MSize len = s->len;

   if (len == 0) {
      setstrV(L, L->top - 1, &G(L)->strempty);
      return 1;
   }

   // Create new string with first character lowercased
   SBuf* sb = lj_buf_tmp_(L);
   lj_buf_reset(sb);

   // Convert first character to lowercase
   char first = str[0];
   if (first >= 'A' and first <= 'Z') {
      first = first + 32;  //  Convert to lowercase
   }
   lj_buf_putb(sb, first);

   if (len > 1) lj_buf_putmem(sb, str + 1, len - 1);

   setstrV(L, L->top - 1, lj_buf_str(L, sb));
   lj_gc_check(L);
   return 1;
}

LJLIB_CF(string_hash)
{
   GCstr* s = lj_lib_checkstr(L, 1);
   int caseSensitive = 0;  //  Default: case insensitive

   // Check for optional second parameter (boolean)
   if (L->base + 1 < L->top and tvisbool(L->base + 1)) {
      caseSensitive = boolV(L->base + 1);
   }
   const char* str = strdata(s);
   MSize len = s->len;
   uint32_t hash = 5381;  //  djb2 hash algorithm
   MSize i;

   if (caseSensitive) {
      for (i = 0; i < len; i++) {
         hash = ((hash << 5) + hash) + (unsigned char)str[i];
      }
   }
   else {
      for (i = 0; i < len; i++) {
         unsigned char c = (unsigned char)str[i];
         if (c >= 0x41 and c <= 0x5A) c = c + 0x20;
         hash = ((hash << 5) + hash) + c;
      }
   }

   setintV(L->top - 1, (int32_t)hash);
   return 1;
}

LJLIB_CF(string_escXML)
{
   GCstr* s = lj_lib_optstr(L, 1);

   // Handle nil input - return empty string
   if (!s) {
      setstrV(L, L->top - 1, &G(L)->strempty);
      return 1;
   }

   const char* str = strdata(s);
   MSize len = s->len;
   SBuf* sb = lj_buf_tmp_(L);
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

LJLIB_ASM(string_reverse)  LJLIB_REC(string_op IRCALL_lj_buf_putstr_reverse)
{
   lj_lib_checkstr(L, 1);
   return FFH_RETRY;
}
LJLIB_ASM_(string_lower)  LJLIB_REC(string_op IRCALL_lj_buf_putstr_lower)
LJLIB_ASM_(string_upper)  LJLIB_REC(string_op IRCALL_lj_buf_putstr_upper)

// ------------------------------------------------------------------------

static int writer_buf(lua_State* L, const void* p, size_t size, void* sb)
{
   lj_buf_putmem((SBuf*)sb, p, (MSize)size);
   UNUSED(L);
   return 0;
}

LJLIB_CF(string_dump)
{
   GCfunc* fn = lj_lib_checkfunc(L, 1);
   int strip = L->base + 1 < L->top and tvistruecond(L->base + 1);
   SBuf* sb = lj_buf_tmp_(L);  //  Assumes lj_bcwrite() doesn't use tmpbuf.
   L->top = L->base + 1;
   if (!isluafunc(fn) or lj_bcwrite(L, funcproto(fn), writer_buf, sb, strip))
      lj_err_caller(L, ErrMsg::STRDUMP);
   setstrV(L, L->top - 1, lj_buf_str(L, sb));
   lj_gc_check(L);
   return 1;
}

// ------------------------------------------------------------------------

// macro to `unsign' a character
#define uchar(c)   ((unsigned char)(c))

#define CAP_UNFINISHED   (-1)
#define CAP_POSITION   (-2)

typedef struct MatchState {
   const char* src_init;  //  init of source string
   const char* src_end;  //  end (`\0') of source string
   lua_State* L;
   int level;  //  total number of captures (finished or unfinished)
   int depth;
   struct {
      const char* init;
      ptrdiff_t len;
   } capture[LUA_MAXCAPTURES];
} MatchState;

#define L_ESC      '%'

static int check_capture(MatchState* ms, int l)
{
   l -= '1';
   if (l < 0 or l >= ms->level or ms->capture[l].len == CAP_UNFINISHED)
      lj_err_caller(ms->L, ErrMsg::STRCAPI);
   return l;
}

static int capture_to_close(MatchState* ms)
{
   int level = ms->level;
   for (level--; level >= 0; level--)
      if (ms->capture[level].len == CAP_UNFINISHED) return level;
   lj_err_caller(ms->L, ErrMsg::STRPATC);
   return 0;  //  unreachable
}

static const char* classend(MatchState* ms, const char* p)
{
   switch (*p++) {
   case L_ESC:
      if (*p == '\0')
         lj_err_caller(ms->L, ErrMsg::STRPATE);
      return p + 1;
   case '[':
      if (*p == '^') p++;
      do {  // look for a `]'
         if (*p == '\0')
            lj_err_caller(ms->L, ErrMsg::STRPATM);
         if (*(p++) == L_ESC and *p != '\0')
            p++;  //  skip escapes (e.g. `%]')
      } while (*p != ']');
      return p + 1;
   default:
      return p;
   }
}

static const unsigned char match_class_map[32] = {
  0,LJ_CHAR_ALPHA,0,LJ_CHAR_CNTRL,LJ_CHAR_DIGIT,0,0,LJ_CHAR_GRAPH,0,0,0,0,
  LJ_CHAR_LOWER,0,0,0,LJ_CHAR_PUNCT,0,0,LJ_CHAR_SPACE,0,
  LJ_CHAR_UPPER,0,LJ_CHAR_ALNUM,LJ_CHAR_XDIGIT,0,0,0,0,0,0,0
};

static int match_class(int c, int cl)
{
   if ((cl & 0xc0) == 0x40) {
      int t = match_class_map[(cl & 0x1f)];
      if (t) {
         t = lj_char_isa(c, t);
         return (cl & 0x20) ? t : !t;
      }
      if (cl == 'z') return c == 0;
      if (cl == 'Z') return c != 0;
   }
   return (cl == c);
}

static int matchbracketclass(int c, const char* p, const char* ec)
{
   int sig = 1;
   if (*(p + 1) == '^') {
      sig = 0;
      p++;  //  skip the `^'
   }
   while (++p < ec) {
      if (*p == L_ESC) {
         p++;
         if (match_class(c, uchar(*p)))
            return sig;
      }
      else if ((*(p + 1) == '-') and (p + 2 < ec)) {
         p += 2;
         if (uchar(*(p - 2)) <= c and c <= uchar(*p)) return sig;
      }
      else if (uchar(*p) == c) return sig;
   }
   return !sig;
}

static int singlematch(int c, const char* p, const char* ep)
{
   switch (*p) {
   case '.': return 1;  //  matches any char
   case L_ESC: return match_class(c, uchar(*(p + 1)));
   case '[': return matchbracketclass(c, p, ep - 1);
   default:  return (uchar(*p) == c);
   }
}

static const char* match(MatchState* ms, const char* s, const char* p);

static const char* matchbalance(MatchState* ms, const char* s, const char* p)
{
   if (*p == 0 or *(p + 1) == 0)
      lj_err_caller(ms->L, ErrMsg::STRPATU);
   if (*s != *p) {
      return nullptr;
   }
   else {
      int b = *p;
      int e = *(p + 1);
      int cont = 1;
      while (++s < ms->src_end) {
         if (*s == e) {
            if (--cont == 0) return s + 1;
         }
         else if (*s == b) {
            cont++;
         }
      }
   }
   return nullptr;  //  string ends out of balance
}

static const char* max_expand(MatchState* ms, const char* s,
   const char* p, const char* ep)
{
   ptrdiff_t i = 0;  //  counts maximum expand for item
   while ((s + i) < ms->src_end and singlematch(uchar(*(s + i)), p, ep))
      i++;
   // keeps trying to match with the maximum repetitions
   while (i >= 0) {
      const char* res = match(ms, (s + i), ep + 1);
      if (res) return res;
      i--;  //  else didn't match; reduce 1 repetition to try again
   }
   return nullptr;
}

static const char* min_expand(MatchState* ms, const char* s,
   const char* p, const char* ep)
{
   for (;;) {
      const char* res = match(ms, s, ep + 1);
      if (res != nullptr)
         return res;
      else if (s < ms->src_end and singlematch(uchar(*s), p, ep))
         s++;  //  try with one more repetition
      else
         return nullptr;
   }
}

static const char* start_capture(MatchState* ms, const char* s,
   const char* p, int what)
{
   const char* res;
   int level = ms->level;
   if (level >= LUA_MAXCAPTURES) lj_err_caller(ms->L, ErrMsg::STRCAPN);
   ms->capture[level].init = s;
   ms->capture[level].len = what;
   ms->level = level + 1;
   if ((res = match(ms, s, p)) == nullptr)  //  match failed?
      ms->level--;  //  undo capture
   return res;
}

static const char* end_capture(MatchState* ms, const char* s,
   const char* p)
{
   int l = capture_to_close(ms);
   const char* res;
   ms->capture[l].len = s - ms->capture[l].init;  //  close capture
   if ((res = match(ms, s, p)) == nullptr)  //  match failed?
      ms->capture[l].len = CAP_UNFINISHED;  //  undo capture
   return res;
}

static const char* match_capture(MatchState* ms, const char* s, int l)
{
   size_t len;
   l = check_capture(ms, l);
   len = (size_t)ms->capture[l].len;
   if ((size_t)(ms->src_end - s) >= len &&
      memcmp(ms->capture[l].init, s, len) == 0)
      return s + len;
   else
      return nullptr;
}

static const char* match(MatchState* ms, const char* s, const char* p)
{
   if (++ms->depth > LJ_MAX_XLEVEL)
      lj_err_caller(ms->L, ErrMsg::STRPATX);
init: //  using goto's to optimize tail recursion
   switch (*p) {
   case '(':  //  start capture
      if (*(p + 1) == ')')  //  position capture?
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
         if (s == nullptr) break;
         p += 4;
         goto init;  //  else s = match(ms, s, p+4);
      case 'f': {  // frontier?
         const char* ep; char previous;
         p += 2;
         if (*p != '[')
            lj_err_caller(ms->L, ErrMsg::STRPATB);
         ep = classend(ms, p);  //  points to what is next
         previous = (s == ms->src_init) ? '\0' : *(s - 1);
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
            if (s == nullptr) break;
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

static void push_onecapture(MatchState* ms, int i, const char* s, const char* e)
{
   if (i >= ms->level) {
      if (i == 0)  //  ms->level == 0, too
         lua_pushlstring(ms->L, s, (size_t)(e - s));  //  add whole match
      else
         lj_err_caller(ms->L, ErrMsg::STRCAPI);
   }
   else {
      ptrdiff_t l = ms->capture[i].len;
      if (l == CAP_UNFINISHED) lj_err_caller(ms->L, ErrMsg::STRCAPU);
      if (l == CAP_POSITION)
         lua_pushinteger(ms->L, ms->capture[i].init - ms->src_init);  // 0-based position
      else
         lua_pushlstring(ms->L, ms->capture[i].init, (size_t)l);
   }
}

static int push_captures(MatchState* ms, const char* s, const char* e)
{
   int i;
   int nlevels = (ms->level == 0 and s) ? 1 : ms->level;
   luaL_checkstack(ms->L, nlevels, "too many captures");
   for (i = 0; i < nlevels; i++)
      push_onecapture(ms, i, s, e);
   return nlevels;  //  number of strings pushed
}

static int str_find_aux(lua_State* L, int find)
{
   GCstr* s = lj_lib_checkstr(L, 1);
   GCstr* p = lj_lib_checkstr(L, 2);
   int32_t start = lj_lib_optint(L, 3, 0);  // 0-based: default start at 0
   MSize st;
   if (start < 0) start += (int32_t)s->len;  // 0-based: -1 → len-1
   if (start < 0) start = 0;
   st = (MSize)start;
   if (st > s->len) {
#if LJ_52
      setnilV(L->top - 1);
      return 1;
#else
      st = s->len;
#endif
   }
   if (find and ((L->base + 3 < L->top and tvistruecond(L->base + 3)) ||
      !lj_str_haspattern(p))) {  // Search for fixed string.
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
      if (*pstr == '^') { pstr++; anchor = 1; }
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

LJLIB_CF(string_find)      LJLIB_REC(.)
{
   return str_find_aux(L, 1);
}

LJLIB_CF(string_match)
{
   return str_find_aux(L, 0);
}

LJLIB_NOREG LJLIB_CF(string_gmatch_aux)
{
   const char* p = strVdata(lj_lib_upvalue(L, 2));
   GCstr* str = strV(lj_lib_upvalue(L, 1));
   const char* s = strdata(str);
   TValue* tvpos = lj_lib_upvalue(L, 3);
   const char* src = s + tvpos->u32.lo;
   MatchState ms;
   ms.L = L;
   ms.src_init = s;
   ms.src_end = s + str->len;
   for (; src <= ms.src_end; src++) {
      const char* e;
      ms.level = ms.depth = 0;
      if ((e = match(&ms, src, p)) != nullptr) {
         int32_t pos = (int32_t)(e - s);
         if (e == src) pos++;  //  Ensure progress for empty match.
         tvpos->u32.lo = (uint32_t)pos;
         return push_captures(&ms, src, e);
      }
   }
   return 0;  //  not found
}

LJLIB_CF(string_gmatch)
{
   lj_lib_checkstr(L, 1);
   lj_lib_checkstr(L, 2);
   L->top = L->base + 3;
   (L->top - 1)->u64 = 0;
   lj_lib_pushcc(L, lj_cf_string_gmatch_aux, FF_string_gmatch_aux, 3);
   return 1;
}

static void add_s(MatchState* ms, luaL_Buffer* b, const char* s, const char* e)
{
   size_t l, i;
   const char* news = lua_tolstring(ms->L, 3, &l);
   for (i = 0; i < l; i++) {
      if (news[i] != L_ESC) luaL_addchar(b, news[i]);
      else {
         i++;  //  skip ESC
         if (not isdigit(uchar(news[i]))) luaL_addchar(b, news[i]);
         else if (news[i] == '0') luaL_addlstring(b, s, (size_t)(e - s));
         else {
            push_onecapture(ms, news[i] - '1', s, e);
            luaL_addvalue(b);  //  add capture to accumulated result
         }
      }
   }
}

static void add_value(MatchState* ms, luaL_Buffer* b,
   const char* s, const char* e)
{
   lua_State* L = ms->L;
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

   if (!lua_toboolean(L, -1)) {  // nil or false?
      lua_pop(L, 1);
      lua_pushlstring(L, s, (size_t)(e - s));  //  keep original text
   }
   else if (!lua_isstring(L, -1)) {
      lj_err_callerv(L, ErrMsg::STRGSRV, luaL_typename(L, -1));
   }
   luaL_addvalue(b);  //  add result to accumulator
}

LJLIB_CF(string_gsub)
{
   size_t srcl;
   const char* src = luaL_checklstring(L, 1, &srcl);
   const char* p = luaL_checkstring(L, 2);
   int  tr = lua_type(L, 3);
   int max_s = luaL_optint(L, 4, (int)(srcl + 1));
   int anchor = (*p == '^') ? (p++, 1) : 0;
   int n = 0;
   MatchState ms;
   luaL_Buffer b;
   if (!(tr == LUA_TNUMBER or tr == LUA_TSTRING ||
      tr == LUA_TFUNCTION or tr == LUA_TTABLE))
      lj_err_arg(L, 3, ErrMsg::NOSFT);
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
      else if (src < ms.src_end)
         luaL_addchar(&b, *src++);
      else
         break;
      if (anchor)
         break;
   }
   luaL_addlstring(&b, src, (size_t)(ms.src_end - src));
   luaL_pushresult(&b);
   lua_pushinteger(L, n);  //  number of substitutions
   return 2;
}

// ------------------------------------------------------------------------

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

extern int luaopen_string(lua_State* L)
{
   LJ_LIB_REG(L, "string", string);
   GCtab *mt = lj_tab_new(L, 0, 1);

   // NOBARRIER: basemt is a GC root.
   global_State *g = G(L);

   // Stores the newly created table mt as the “base metatable” for the string type. basemt_it(g, LJ_TSTR) selects
   // the slot for the string base metatable in the global state, obj2gco(mt) converts the GCtab* to a generic GC
   // object reference, and setgcref writes that GC reference into the global state (so the runtime knows the
   // canonical metatable for strings).

   setgcref(basemt_it(g, LJ_TSTR), obj2gco(mt));

   // Create the entry mt[mmname_str(g, MM_index)] (i.e. __index) and set its value to L->top - 1.

   settabV(L, lj_tab_setstr(L, mt, mmname_str(g, MM_index)), tabV(L->top - 1));

   // Update the metatable’s negative‑metamethod cache (nomm). The bitwise expression clears the bit
   // corresponding to MM_index (and sets other bits), marking that this metamethod slot should not be treated as
   // “absent” by the fast metamethod check. Practically, this tells the runtime the MM_index metamethod is present
   // (so subsequent lookups/optimisations behave accordingly).
   //
   // NOTE: nomm is an 8‑bit negative cache, so MM_index must fit within those bits; the bit trick is intentional
   // to flip the absence/presence semantics used by the VM.

   mt->nomm = (uint8_t)(~(1u << MM_index));
   return 1;
}
