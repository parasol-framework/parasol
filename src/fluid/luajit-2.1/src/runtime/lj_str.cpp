// String handling.
// Copyright (C) 2005-2022 Mike Pall. See Copyright Notice in luajit.h

#define lj_str_c
#define LUA_CORE

#include "lj_obj.h"
#include "lj_gc.h"
#include "lj_err.h"
#include "lj_str.h"
#include "lj_char.h"
#include "lj_prng.h"
#include <parasol/strings.hpp>

#ifndef CSTRING
#define CSTRING const char *
#endif

//********************************************************************************************************************
// Ordered compare of strings. Assumes string data is 4-byte aligned.

int32_t LJ_FASTCALL lj_str_cmp(GCstr *a, GCstr *b)
{
   MSize i, n = a->len > b->len ? b->len : a->len;
   for (i = 0; i < n; i += 4) {
      // Note: innocuous access up to end of string + 3.
      uint32_t va = *(const uint32_t*)(strdata(a) + i);
      uint32_t vb = *(const uint32_t*)(strdata(b) + i);
      if (va != vb) {
#if LJ_LE
         va = lj_bswap(va); vb = lj_bswap(vb);
#endif
         i -= n;
         if ((int32_t)i >= -3) {
            va >>= 32 + (i << 3); vb >>= 32 + (i << 3);
            if (va IS vb) break;
         }
         return va < vb ? -1 : 1;
      }
   }
   return (int32_t)(a->len - b->len);
}

//********************************************************************************************************************
// Find fixed string p inside string s.

const char * lj_str_find(CSTRING s, CSTRING p, MSize slen, MSize plen)
{
   if (plen <= slen) {
      if (plen IS 0) {
         return s;
      }
      else {
         int c = *(const uint8_t*)p++;
         plen--; slen -= plen;
         while (slen) {
            CSTRING q = (CSTRING)memchr(s, c, slen);
            if (!q) break;
            if (memcmp(q + 1, p, plen) IS 0) return q;
            q++; slen -= (MSize)(q - s); s = q;
         }
      }
   }
   return nullptr;
}

//********************************************************************************************************************
// Check whether a string has a pattern matching character.

int lj_str_haspattern(GCstr* s)
{
   const char* p = strdata(s), * q = p + s->len;
   while (p < q) {
      int c = *(const uint8_t*)p++;
      if (lj_char_ispunct(c) and strchr("^$*+?.([%-", c)) return 1;  //  Found a pattern matching char.
   }
   return 0;  //  No pattern matching chars found.
}

//********************************************************************************************************************
// String interning

#define LJ_STR_MAXCOLL      32

// Resize the string interning hash table (grow and shrink).

void lj_str_resize(lua_State* L, MSize newmask)
{
   global_State* g = G(L);
   GCRef *newtab, *oldtab = g->str.tab;
   MSize i;

   // No resizing during GC traversal or if already too big.

   GarbageCollector collector = gc(g);
   if (collector.phase() IS GCPhase::SweepString or newmask >= LJ_MAX_STRTAB - 1)
      return;

   newtab = lj_mem_newvec(L, newmask + 1, GCRef);
   memset(newtab, 0, (newmask + 1) * sizeof(GCRef));

   // Reinsert all strings from the old table into the new table.

   for (i = g->str.mask; i != ~(MSize)0; i--) {
      GCobj* o = (GCobj*)(gcrefu(oldtab[i]) & ~(uintptr_t)1);
      while (o) {
         GCobj *next = gcnext(o);
         GCstr *s = gco2str(o);
         MSize hash = s->hash;
         hash &= newmask;
         // NOBARRIER: The string table is a GC root.
         setgcrefr(o->gch.nextgc, newtab[hash]);
         setgcref(newtab[hash], o);
         o = next;
      }
   }

   // Free old table and replace with new table.
   lj_str_freetab(g);
   g->str.tab = newtab;
   g->str.mask = newmask;
}

//********************************************************************************************************************
// Allocate a new string and add to string interning table.  Throws on failure.

static GCstr * lj_str_alloc(lua_State* L, CSTRING str, MSize len, uint32_t hash)
{
   GCstr *s = lj_mem_newt(L, lj_str_size(len), GCstr);
   global_State *g = G(L);
   uintptr_t u;

   newwhite(g, s);

   s->gct      = ~LJ_TSTR;
   s->len      = len;
   s->hash     = hash;
   s->sid      = g->str.id++;
   s->reserved = 0;

   // Clear last 4 bytes of allocated memory. Implies zero-termination, too.
   *(uint32_t*)(strdatawr(s) + (len & ~(MSize)3)) = 0;
   memcpy(strdatawr(s), str, len);

   // Add to string hash table.
   hash &= g->str.mask;
   u = gcrefu(g->str.tab[hash]);
   setgcrefp(s->nextgc, (u & ~(uintptr_t)1));

   // NOBARRIER: The string table is a GC root.
   setgcrefp(g->str.tab[hash], ((uintptr_t)s | (u & 1)));
   if (g->str.num++ > g->str.mask) { //  Allow a 100% load factor.
      lj_str_resize(L, (g->str.mask << 1) + 1);  //  Grow string table.
   }
   return s;  //  Return newly interned string.
}

//********************************************************************************************************************
// Intern a string and return string object.  Throws on failure.

GCstr * lj_str_new(lua_State* L, CSTRING str, size_t lenx)
{
   global_State *g = G(L);
   if (lenx - 1 < LJ_MAX_STR - 1) {
      auto len = (MSize)lenx;
      auto hash = pf::strhash(std::string_view(str, lenx));
      MSize coll = 0;
      // Check if the string has already been interned.
      GCobj *o = gcref(g->str.tab[hash & g->str.mask]);

      while (o != nullptr) {
         GCstr *sx = gco2str(o);
         if (sx->hash IS hash and sx->len IS len) {
            if (memcmp(str, strdata(sx), len) IS 0) {
               if (isdead(g, o)) flipwhite(o);  //  Resurrect if dead.
               return sx;  //  Return existing string.
            }
            coll++;
         }
         coll++;
         o = gcnext(o);
      }

      // Otherwise allocate a new string.
      return lj_str_alloc(L, str, len, hash);
   }
   else {
      if (lenx) lj_err_msg(L, ErrMsg::STROV);
      return &g->strempty;
   }
}

//********************************************************************************************************************

void LJ_FASTCALL lj_str_free(global_State* g, GCstr* s)
{
   g->str.num--;
   lj_mem_free(g, s, lj_str_size(s->len));
}

//********************************************************************************************************************

void LJ_FASTCALL lj_str_init(lua_State* L)
{
   lj_str_resize(L, LJ_MIN_STRTAB - 1);
}
