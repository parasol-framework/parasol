// Base and coroutine library.
// Copyright (C) 2005-2022 Mike Pall. See Copyright Notice in luajit.h
//
// Major portions taken verbatim or adapted from the Lua interpreter.
// Copyright (C) 1994-2011 Lua.org, PUC-Rio. See Copyright Notice in lua.h

#include <stdio.h>
#include <cctype>

#define lib_base_c
#define LUA_LIB

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

#include "lj_obj.h"
#include "lj_gc.h"
#include "lj_err.h"
#include "lj_debug.h"
#include "lj_buf.h"
#include "lj_str.h"
#include "lj_tab.h"
#include "lj_meta.h"
#include "lj_state.h"
#include "lj_frame.h"
#if LJ_HASFFI
#include "lj_ctype.h"
#include "lj_cconv.h"
#endif
#include "lj_bc.h"
#include "lj_ff.h"
#include "lj_dispatch.h"
#include "lj_char.h"
#include "lj_strscan.h"
#include "lj_strfmt.h"
#include "lib.h"
#include "lib_utils.h"
#include "lj_array.h"
#include "lj_vmarray.h"
#include "runtime/stack_utils.h"
#include "runtime/lj_thunk.h"
#include "debug/error_guard.h"

//********************************************************************************************************************
// Base library: checks

#define LJLIB_MODULE_base

LJLIB_ASM(assert)      LJLIB_REC(.)
{
   lj_lib_checkany(L, 1);
   if (L->top IS L->base + 1) lj_err_caller(L, ErrMsg::ASSERT);
   else if (is_any_type<LJ_TSTR, LJ_TNUMX>(L->base + 1)) lj_err_callermsg(L, strdata(lj_lib_checkstr(L, 2)));
   else lj_err_run(L);
   return FFH_UNREACHABLE;
}

//********************************************************************************************************************
// ORDER LJ_T

LJLIB_PUSH("nil")
LJLIB_PUSH("boolean")
LJLIB_PUSH(top-1)  //  boolean
LJLIB_PUSH("userdata")
LJLIB_PUSH("string")
LJLIB_PUSH("upval")
LJLIB_PUSH("thread")
LJLIB_PUSH("proto")
LJLIB_PUSH("function")
LJLIB_PUSH("trace")
LJLIB_PUSH("cdata")
LJLIB_PUSH("table")
LJLIB_PUSH(top-9)  //  userdata
LJLIB_PUSH("array")
LJLIB_PUSH("number")
LJLIB_ASM(type)      LJLIB_REC(.)
{
   // C fallback for type() - handles thunks with declared types
   TValue *o = L->base;
   if (tvisudata(o)) {
      GCudata *ud = udataV(o);
      if (ud->udtype IS UDTYPE_THUNK) {
         ThunkPayload *payload = thunk_payload(ud);
         if (payload->expected_type != 0xFF) {
            // Use the declared type string from the upvalue array
            GCfunc *fn = funcV(L->base - 1 - LJ_FR2);
            GCstr *type_str = strV(&fn->c.upvalue[payload->expected_type]);
            setstrV(L, L->base - 1 - LJ_FR2, type_str);
            return FFH_RES(1);
         }
      }
   }
   // For non-thunk userdata, return "userdata" string (upvalue index 3)
   GCfunc *fn = funcV(L->base - 1 - LJ_FR2);
   setstrV(L, L->base - 1 - LJ_FR2, strV(&fn->c.upvalue[3]));
   return FFH_RES(1);
}

// Recycle the lj_lib_checkany(L, 1) from assert.

//********************************************************************************************************************
// Base library: iterators

static_assert((int)FF_next IS FF_next_N); // This solves a circular dependency problem -- change FF_next_N as needed.

LJLIB_ASM(next) LJLIB_REC(.) // Use of '.' indicates the function name in the recorder is unchanged
{
   lj_lib_checktab(L, 1);
   lj_err_msg(L, ErrMsg::NEXTIDX);
   return FFH_UNREACHABLE;
}

//********************************************************************************************************************

static int ffh_pairs(lua_State* L, MMS mm)
{
   TValue* o = lj_lib_checkany(L, 1);
   cTValue* mo = lj_meta_lookup(L, o, mm);
   if (not tvisnil(mo)) {
      L->top = o + 1;  //  Only keep one argument.
      copyTV(L, L->base - 2, mo);  //  Replace callable.
      return FFH_TAILCALL;
   }
   else {
      LJ_CHECK_TYPE(L, 1, o, LUA_TTABLE);
      copyTV(L, o - 1, o);
      o--;
      setfuncV(L, o - 1, funcV(lj_lib_upvalue(L, 1)));
      if (mm IS MM_pairs) setnilV(o + 1);
      else setintV(o + 1, -1);  // ipairs starts at -1, increments to 0
      return FFH_RES(3);
   }
}

//********************************************************************************************************************

LJLIB_PUSH(lastcl)
LJLIB_ASM(pairs)      LJLIB_REC(xpairs 0)
{
   return ffh_pairs(L, MM_pairs);
}

//********************************************************************************************************************

LJLIB_NOREGUV LJLIB_ASM(ipairs_aux)   LJLIB_REC(.)
{
   lj_lib_checktab(L, 1);
   lj_lib_checkint(L, 2);
   return FFH_UNREACHABLE;
}

//********************************************************************************************************************

LJLIB_PUSH(lastcl)
LJLIB_ASM(ipairs)      LJLIB_REC(xpairs 1)
{
   return ffh_pairs(L, MM_ipairs);
}

//********************************************************************************************************************
// values() iterator - iterates over table values only, discarding keys
// Usage: for v in values(tbl) do ... end
// Equivalent to: for _, v in pairs(tbl) do ... end

static int values_iterator_next(lua_State* L)
{
   // Upvalue 1: the table being iterated
   // Upvalue 2: state table containing the current key at index 0
   GCfunc* fn = curr_func(L);
   GCtab* t = tabV(&fn->c.upvalue[0]);
   GCtab* state = tabV(&fn->c.upvalue[1]);
   TValue* key_slot = lj_tab_setint(L, state, 0);  // Get mutable slot

   TValue result[2];
   if (lj_tab_next(t, key_slot, result)) {
      // Update the key in state table for next iteration
      copyTV(L, key_slot, &result[0]);
      // Return only the value
      copyTV(L, L->top, &result[1]);
      L->top++;
      return 1;
   }
   return 0;  // End of iteration
}

//********************************************************************************************************************
// values() iterator for arrays - iterates over array values only
// Upvalue 1: the array being iterated
// Upvalue 2: current index (stored as integer, mutable)

static int values_array_iterator_next(lua_State* L)
{
   GCfunc *fn = curr_func(L);
   GCarray *arr = arrayV(&fn->c.upvalue[0]);
   TValue *idx_tv = &fn->c.upvalue[1];

   int32_t idx = numberVint(idx_tv);
   if (idx < 0 or MSize(idx) >= arr->len) return 0;  // End of iteration

   // Get the element value
   lj_arr_getidx(L, arr, idx, L->top);
   L->top++;
     
   setintV(idx_tv, idx + 1);  // Advance index for next iteration
   return 1;
}

LJLIB_CF(values)
{
   TValue* o = lj_lib_checkany(L, 1);

   if (tvistab(o)) {
      GCtab* t = tabV(o);

      settabV(L, L->top, t); // Push the table as upvalue 1
      L->top++;

      // Create state table to hold the mutable key (upvalue 2)
      GCtab* state = lj_tab_new(L, 0, 1);
      settabV(L, L->top, state);
      TValue* key_slot = lj_tab_setint(L, state, 0);
      setnilV(key_slot);
      L->top++;

      // Create closure with 2 upvalues
      lua_pushcclosure(L, values_iterator_next, 2);
   }
   else if (tvisarray(o)) {
      GCarray *arr = arrayV(o);
      setarrayV(L, L->top, arr); // Push the array as upvalue 1
      L->top++;
      setintV(L->top, 0); // Push the starting index as upvalue 2
      L->top++;

      // Create closure with 2 upvalues
      lua_pushcclosure(L, values_array_iterator_next, 2);
   }
   else lj_err_argt(L, 1, LUA_TTABLE);  // Expected table or array

   lua_pushnil(L);  // State (not used)
   lua_pushnil(L);  // Initial control variable
   return 3;
}

//********************************************************************************************************************
// keys() iterator - iterates over table keys only, discarding values
// Usage: for k in keys(tbl) do ... end
// Equivalent to: for k, _ in pairs(tbl) do ... end

static int keys_iterator_next(lua_State* L)
{
   // Upvalue 1: the table being iterated
   // Upvalue 2: state table containing the current key at index 0
   GCfunc* fn = curr_func(L);
   GCtab* t = tabV(&fn->c.upvalue[0]);
   GCtab* state = tabV(&fn->c.upvalue[1]);
   TValue* key_slot = lj_tab_setint(L, state, 0);  // Get mutable slot

   TValue result[2];
   if (lj_tab_next(t, key_slot, result)) {
      // Update the key in state table for next iteration
      copyTV(L, key_slot, &result[0]);
      // Return only the key
      copyTV(L, L->top, &result[0]);
      L->top++;
      return 1;
   }
   return 0;  // End of iteration
}

LJLIB_CF(keys)
{
   GCtab* t = lj_lib_checktab(L, 1);

   // Push the table as upvalue 1
   settabV(L, L->top, t);
   L->top++;

   // Create state table to hold the mutable key (upvalue 2)
   GCtab* state = lj_tab_new(L, 0, 1);
   settabV(L, L->top, state);
   TValue* key_slot = lj_tab_setint(L, state, 0);
   setnilV(key_slot);
   L->top++;

   // Create closure with 2 upvalues
   lua_pushcclosure(L, keys_iterator_next, 2);
   lua_pushnil(L);  // State (not used)
   lua_pushnil(L);  // Initial control variable
   return 3;
}

//********************************************************************************************************************
// Base library: getters and setters

LJLIB_ASM_(getmetatable)   LJLIB_REC(.)

//********************************************************************************************************************
// Recycle the lj_lib_checkany(L, 1) from assert.

LJLIB_ASM(setmetatable)      LJLIB_REC(.)
{
   GCtab* t = lj_lib_checktab(L, 1);
   GCtab* mt = lj_lib_checktabornil(L, 2);
   if (!tvisnil(lj_meta_lookup(L, L->base, MM_metatable))) lj_err_caller(L, ErrMsg::PROTMT);
   setgcref(t->metatable, obj2gco(mt));
   if (mt) { lj_gc_objbarriert(L, t, mt); }
   settabV(L, L->base - 2, t);
   return FFH_RES(1);
}

//********************************************************************************************************************

LJLIB_ASM(rawget)      LJLIB_REC(.)
{
   lj_lib_checktab(L, 1);
   lj_lib_checkany(L, 2);
   return FFH_UNREACHABLE;
}

//********************************************************************************************************************

LJLIB_CF(rawset)      LJLIB_REC(.)
{
   lj_lib_checktab(L, 1);
   lj_lib_checkany(L, 2);
   L->top = 1 + lj_lib_checkany(L, 3);
   lua_rawset(L, 1);
   return 1;
}

//********************************************************************************************************************

LJLIB_CF(rawequal)      LJLIB_REC(.)
{
   cTValue* o1 = lj_lib_checkany(L, 1);
   cTValue* o2 = lj_lib_checkany(L, 2);
   setboolV(L->top - 1, lj_obj_equal(o1, o2));
   return 1;
}

//********************************************************************************************************************

LJLIB_CF(rawlen)      LJLIB_REC(.)
{
   cTValue* o = L->base;
   int32_t len;
   if (L->top > o and tvisstr(o))
      len = (int32_t)strV(o)->len;
   else
      len = (int32_t)lj_tab_len(lj_lib_checktab(L, 1));
   setintV(L->top - 1, len);
   return 1;
}

//********************************************************************************************************************
// __filter(mask, count, trailing_keep, ...)
// Filters return values based on a bitmask pattern.
// mask: uint64 bitmask where bit N=1 means keep value at position N
// count: number of explicitly specified positions in the pattern
// trailing_keep: true if excess values should be kept, false to drop
//
// RAII Pattern: Uses StackFrame to ensure L->top is restored on error paths.
// The frame automatically cleans up if an error is thrown, preventing stack leaks.

LJLIB_CF(__filter)      LJLIB_REC(.)
{
   StackFrame frame(L);

   int32_t nargs = int32_t(L->top - L->base);
   require_arg_count(L, 3);

   // Extract filter parameters
   uint64_t mask = uint64_t(lj_lib_checknum(L, 1));
   int32_t count = lj_lib_checkint(L, 2);
   bool trailing_keep = tvistruecond(L->base + 2);

   // Values to filter start at position 4 (index 3, 0-based)
   int32_t value_count = nargs - 3;

   // First pass: count how many values we'll keep (for stack check)
   int32_t out_count = 0;
   for (int32_t i = 0; i < value_count; i++) {
      bool keep = (i < count) ? ((mask & (1ULL << i)) != 0) : trailing_keep;
      if (keep) out_count++;
   }

   // Ensure we have enough stack space
   if (out_count > 0 and !lua_checkstack(L, out_count)) {
      lj_err_caller(L, ErrMsg::STKOV);
      return 0;  // StackFrame destructor will restore L->top
   }

   // Move kept values into position at L->base (overwriting the args)
   // This is similar to how select() works but with filtering
   TValue* src = L->base + 3;  // Values start after mask, count, trailing_keep
   TValue* dst = L->base;      // Overwrite from the start

   int32_t written = 0;
   for (int32_t i = 0; i < value_count; i++) {
      bool keep = (i < count) ? ((mask & (1ULL << i)) != 0) : trailing_keep;
      if (keep) {
         if (dst + written != src + i) {
            copyTV(L, dst + written, src + i);
         }
         written++;
      }
   }

   // Adjust L->top to reflect the number of returns
   // Note: We manually set L->top here because the results start at L->base,
   // not at the saved_top position.
   L->top = L->base + written;
   frame.disarm();  // Disarm the guard since we manually set L->top

   return written;
}

//********************************************************************************************************************
// Base library: conversions

LJLIB_ASM(tonumber)      LJLIB_REC(.)
{
   int32_t base = lj_lib_optint(L, 2, 10);
   if (base IS 10) {
      TValue* o = lj_lib_checkany(L, 1);
      if (lj_strscan_numberobj(o)) {
         copyTV(L, L->base - 2, o);
         return FFH_RES(1);
      }
#if LJ_HASFFI
      if (tviscdata(o)) {
         CTState* cts = ctype_cts(L);
         CType* ct = lj_ctype_rawref(cts, cdataV(o)->ctypeid);
         if (ctype_isenum(ct->info)) ct = ctype_child(cts, ct);
         if (ctype_isnum(ct->info) or ctype_iscomplex(ct->info)) {
            if (LJ_DUALNUM and ctype_isinteger_or_bool(ct->info) &&
               ct->size <= 4 and !(ct->size IS 4 and (ct->info & CTF_UNSIGNED))) {
               int32_t i;
               lj_cconv_ct_tv(cts, ctype_get(cts, CTID_INT32), (uint8_t*)&i, o, 0);
               setintV(L->base - 2, i);
               return FFH_RES(1);
            }
            lj_cconv_ct_tv(cts, ctype_get(cts, CTID_DOUBLE),
               (uint8_t*)&(L->base - 2)->n, o, 0);
            return FFH_RES(1);
         }
      }
#endif
   }
   else {
      const char* p = strdata(lj_lib_checkstr(L, 1));
      char* ep;
      unsigned int neg = 0;
      unsigned long ul;
      LJ_CHECK_RANGE(L, 2, base, 2, 36, ErrMsg::BASERNG);
      while (isspace((unsigned char)(*p))) p++;
      if (*p IS '-') { p++; neg = 1; }
      else if (*p IS '+') { p++; }
      if (isalnum((unsigned char)(*p))) {
         ul = strtoul(p, &ep, base);
         if (p != ep) {
            while (isspace((unsigned char)(*ep))) ep++;
            if (*ep IS '\0') {
               if (LJ_DUALNUM and LJ_LIKELY(ul < 0x80000000u + neg)) {
                  if (neg) ul = (unsigned long)-(long)ul;
                  setintV(L->base - 2, (int32_t)ul);
               }
               else {
                  lua_Number n = (lua_Number)ul;
                  if (neg) n = -n;
                  setnumV(L->base - 2, n);
               }
               return FFH_RES(1);
            }
         }
      }
   }
   setnilV(L->base - 2);
   return FFH_RES(1);
}

//********************************************************************************************************************

LJLIB_ASM(tostring)      LJLIB_REC(.)
{
   StackFrame frame(L);

   TValue* o = lj_lib_checkany(L, 1);
   cTValue* mo;
   L->top = o + 1;  //  Only keep one argument.
   if (!tvisnil(mo = lj_meta_lookup(L, o, MM_tostring))) {
      copyTV(L, L->base - 2, mo);  //  Replace callable.
      frame.disarm();  // Disarm before tail call
      return FFH_TAILCALL;
   }
   lj_gc_check(L);
   setstrV(L, L->base - 2, lj_strfmt_obj(L, L->base));
   frame.disarm();  // Disarm - result already in place
   return FFH_RES(1);
}

//********************************************************************************************************************
// Base library: throw and catch errors

LJLIB_CF(error)
{
   int32_t level = lj_lib_optint(L, 2, 1);
   lua_settop(L, 1);
   if (lua_isstring(L, 1) and level > 0) {
      luaL_where(L, level);
      lua_pushvalue(L, 1);
      lua_concat(L, 2);
   }
   return lua_error(L);
}

//********************************************************************************************************************

LJLIB_ASM(pcall)      LJLIB_REC(.)
{
   lj_lib_checkany(L, 1);
   lj_lib_checkfunc(L, 2);  //  For xpcall only.
   return FFH_UNREACHABLE;
}
LJLIB_ASM_(xpcall)      LJLIB_REC(.)

//********************************************************************************************************************
// Base library: GC control

LJLIB_CF(collectgarbage)
{
   int opt = lj_lib_checkopt(L, 1, LUA_GCCOLLECT,  //  ORDER LUA_GC*
      "\4stop\7restart\7collect\5count\1\377\4step\10setpause\12setstepmul\1\377\11isrunning");
   int32_t data = lj_lib_optint(L, 2, 0);
   if (opt IS LUA_GCCOUNT) {
      setnumV(L->top, (lua_Number)G(L)->gc.total / 1024.0);
   }
   else {
      int res = lua_gc(L, opt, data);
      if (opt IS LUA_GCSTEP or opt IS LUA_GCISRUNNING) setboolV(L->top, res);
      else setintV(L->top, res);
   }
   L->top++;
   return 1;
}

//********************************************************************************************************************
// Base library: miscellaneous functions

LJLIB_PUSH(top-2)  //  Upvalue holds weak table.
LJLIB_CF(newproxy)
{
   lua_settop(L, 1);
   lua_newuserdata(L, 0);
   if (lua_toboolean(L, 1) IS 0) {  // newproxy(): without metatable.
      return 1;
   }
   else if (lua_isboolean(L, 1)) {  // newproxy(true): with metatable.
      lua_newtable(L);
      lua_pushvalue(L, -1);
      lua_pushboolean(L, 1);
      lua_rawset(L, lua_upvalueindex(1));  //  Remember mt in weak table.
   }
   else {  // newproxy(proxy): inherit metatable.
      int validproxy = 0;
      if (lua_getmetatable(L, 1)) {
         lua_rawget(L, lua_upvalueindex(1));
         validproxy = lua_toboolean(L, -1);
         lua_pop(L, 1);
      }
      if (!validproxy)
         lj_err_arg(L, 1, ErrMsg::NOPROXY);
      lua_getmetatable(L, 1);
   }
   lua_setmetatable(L, 2);
   return 1;
}

//********************************************************************************************************************
// RAII Pattern: Uses StackFrame to ensure L->top is restored if tostring conversion
// fails or triggers an error during the print loop, preventing stack corruption.

LJLIB_PUSH("tostring")
LJLIB_CF(print)
{
   StackFrame frame(L);

   ptrdiff_t i, nargs = L->top - L->base;
   cTValue* tv = lj_tab_getstr(tabref(L->env), strV(lj_lib_upvalue(L, 1)));
   int shortcut;
   if (tv and !tvisnil(tv)) {
      copyTV(L, L->top++, tv);
   }
   else {
      setstrV(L, L->top++, strV(lj_lib_upvalue(L, 1)));
      lua_gettable(L, LUA_GLOBALSINDEX);
      tv = L->top - 1;
   }
   shortcut = (tvisfunc(tv) and funcV(tv)->c.ffid IS FF_tostring) and
      !gcrefu(basemt_it(G(L), LJ_TNUMX));
   for (i = 0; i < nargs; i++) {
      cTValue* o = &L->base[i];
      const char* str;
      size_t size;
      MSize len;
      if (shortcut and (str = lj_strfmt_wstrnum(L, o, &len)) != nullptr) {
         size = len;
      }
      else {
         copyTV(L, L->top + 1, o);
         copyTV(L, L->top, L->top - 1);
         L->top += 2;
         lua_call(L, 1, 1);
         str = lua_tolstring(L, -1, &size);
         if (!str)
            lj_err_caller(L, ErrMsg::PRTOSTR);  // StackFrame will restore L->top
         L->top--;
      }
      if (i)
         putchar('\t');
      fwrite(str, 1, size, stdout);
   }
   putchar('\n');
   frame.commit(0);  // No return values
   return 0;
}

LJLIB_PUSH(top-3)
LJLIB_SET(_VERSION)

//********************************************************************************************************************
// Check if a value is an unresolved thunk

LJLIB_CF(isthunk)
{
   cTValue *o = lj_lib_checkany(L, 1);
   setboolV(L->top++, lj_is_thunk(o));
   return 1;
}

//********************************************************************************************************************
// Explicitly resolve a thunk (returns the value unchanged if not a thunk)

LJLIB_CF(resolve)
{
   cTValue *o = lj_lib_checkany(L, 1);

   if (lj_is_thunk(o)) {
      GCudata *ud = udataV(o);
      TValue *resolved = lj_thunk_resolve(L, ud);
      copyTV(L, L->top++, resolved);
      return 1;
   }

   // Not a thunk - return as-is
   copyTV(L, L->top++, o);
   return 1;
}

//********************************************************************************************************************
// Internal function for creating thunk userdata (called by IR emitter)
// Args: (closure:function, expected_type:number)
// Returns: thunk userdata

LJLIB_CF(__create_thunk)
{
   GCfunc *fn = lj_lib_checkfunc(L, 1);
   int expected_type = (int)lj_lib_checkint(L, 2);

   // Create thunk userdata
   lj_thunk_new(L, fn, expected_type);

   // Thunk userdata is now at L->top-1
   return 1;
}

#include "lj_libdef.h"

//********************************************************************************************************************
// Coroutine library

#define LJLIB_MODULE_coroutine

LJLIB_CF(coroutine_status)
{
   const char* s;
   lua_State* co;
   if (!(L->top > L->base and tvisthread(L->base))) lj_err_arg(L, 1, ErrMsg::NOCORO);
   co = threadV(L->base);
   if (co IS L) s = "running";
   else if (co->status IS LUA_YIELD) s = "suspended";
   else if (co->status != LUA_OK) s = "dead";
   else if (co->base > tvref(co->stack) + 1 + 1) s = "normal";
   else if (co->top IS co->base) s = "dead";
   else s = "suspended";
   lua_pushstring(L, s);
   return 1;
}

//********************************************************************************************************************

LJLIB_CF(coroutine_running)
{
   int ismain = lua_pushthread(L);
   setboolV(L->top++, ismain);
   return 2;
}

//********************************************************************************************************************

LJLIB_CF(coroutine_isyieldable)
{
   setboolV(L->top++, cframe_canyield(L->cframe));
   return 1;
}

//********************************************************************************************************************

LJLIB_CF(coroutine_create)
{
   lua_State* L1;
   if (!(L->base < L->top and tvisfunc(L->base))) lj_err_argt(L, 1, LUA_TFUNCTION);
   L1 = lua_newthread(L);
   setfuncV(L, L1->top++, funcV(L->base));
   return 1;
}

//********************************************************************************************************************

LJLIB_ASM(coroutine_yield)
{
   lj_err_caller(L, ErrMsg::CYIELD);
   return FFH_UNREACHABLE;
}

//********************************************************************************************************************

static int ffh_resume(lua_State* L, lua_State* co, int wrap)
{
   if (co->cframe != nullptr or co->status > LUA_YIELD ||
      (co->status IS LUA_OK and co->top IS co->base)) {
      ErrMsg em = co->cframe ? ErrMsg::CORUN : ErrMsg::CODEAD;
      if (wrap) lj_err_caller(L, em);
      setboolV(L->base - 2, 0);
      setstrV(L, L->base - 1, lj_err_str(L, em));
      return FFH_RES(2);
   }
   lj_state_growstack(co, (MSize)(L->top - L->base));
   return FFH_RETRY;
}

//********************************************************************************************************************

LJLIB_ASM(coroutine_resume)
{
   if (!(L->top > L->base and tvisthread(L->base)))
      lj_err_arg(L, 1, ErrMsg::NOCORO);
   return ffh_resume(L, threadV(L->base), 0);
}

//********************************************************************************************************************

LJLIB_NOREG LJLIB_ASM(coroutine_wrap_aux)
{
   return ffh_resume(L, threadV(lj_lib_upvalue(L, 1)), 1);
}

// Inline declarations.
LJ_ASMF void lj_ff_coroutine_wrap_aux(void);
LJ_FUNCA_NORET void LJ_FASTCALL lj_ffh_coroutine_wrap_err(lua_State* L, lua_State* co);

//********************************************************************************************************************
// Error handler, called from assembler VM.

void LJ_FASTCALL lj_ffh_coroutine_wrap_err(lua_State* L, lua_State* co)
{
   co->top--; copyTV(L, L->top, co->top); L->top++;
   if (tvisstr(L->top - 1)) lj_err_callermsg(L, strVdata(L->top - 1));
   else lj_err_run(L);
}

//********************************************************************************************************************
// Forward declaration.

static void setpc_wrap_aux(lua_State* L, GCfunc* fn);

LJLIB_CF(coroutine_wrap)
{
   GCfunc* fn;
   lj_cf_coroutine_create(L);
   fn = lj_lib_pushcc(L, lj_ffh_coroutine_wrap_aux, FF_coroutine_wrap_aux, 1);
   setpc_wrap_aux(L, fn);
   return 1;
}

#include "lj_libdef.h"

//********************************************************************************************************************
// Fix the PC of wrap_aux. Really ugly workaround.

static void setpc_wrap_aux(lua_State* L, GCfunc* fn)
{
   setmref(fn->c.pc, &L2GG(L)->bcff[lj_lib_init_coroutine[1] + 2]);
}

//********************************************************************************************************************

static void newproxy_weaktable(lua_State* L)
{
   // NOBARRIER: The table is new (marked white).
   GCtab* t = lj_tab_new(L, 0, 1);
   settabV(L, L->top++, t);
   setgcref(t->metatable, obj2gco(t));
   setstrV(L, lj_tab_setstr(L, t, lj_str_newlit(L, "__mode")), lj_str_newlit(L, "kv"));
   t->nomm = (uint8_t)(~(1u << MM_mode));
}

//********************************************************************************************************************

extern int luaopen_base(lua_State* L)
{
   // NOBARRIER: Table and value are the same.
   GCtab* env = tabref(L->env);
   settabV(L, lj_tab_setstr(L, env, lj_str_newlit(L, "_G")), env);
   lua_pushliteral(L, "5.2");  //  top-3. // Lua version number, set as _VERSION
   newproxy_weaktable(L);  //  top-2.
   LJ_LIB_REG(L, "_G", base);
   LJ_LIB_REG(L, LUA_COLIBNAME, coroutine);
   return 2;
}
