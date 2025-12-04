// Public Lua/C API.
// Copyright (C) 2005-2022 Mike Pall. See Copyright Notice in luajit.h
//
// Major portions taken verbatim or adapted from the Lua interpreter.
// Copyright (C) 1994-2008 Lua.org, PUC-Rio. See Copyright Notice in lua.h

#define lj_api_c
#define LUA_CORE

#include "lj_obj.h"
#include "lj_gc.h"
#include "lj_err.h"
#include "lj_debug.h"
#include "lj_str.h"
#include "lj_tab.h"
#include "lj_func.h"
#include "lj_udata.h"
#include "lj_meta.h"
#include "lj_state.h"
#include "lj_bc.h"
#include "lj_frame.h"
#include "lj_trace.h"
#include "lj_vm.h"
#include "lj_strscan.h"
#include "lj_strfmt.h"
#include "lib/lib_utils.h"
#include "runtime/stack_helpers.h"

// Common helper functions

#define lj_checkapi_slot(idx) \
  lj_checkapi((idx) <= (L->top - L->base), "stack slot %d out of range", (idx))

static TValue * index2adr(lua_State *L, int idx)
{
   if (idx > 0) {
      TValue * o = L->base + (idx - 1);
      return o < L->top ? o : niltv(L);
   }
   else if (idx > LUA_REGISTRYINDEX) {
      lj_checkapi(idx != 0 and -idx <= L->top - L->base,
         "bad stack slot %d", idx);
      return L->top + idx;
   }
   else if (idx == LUA_GLOBALSINDEX) {
      TValue * o = &G(L)->tmptv;
      settabV(L, o, tabref(L->env));
      return o;
   }
   else if (idx == LUA_REGISTRYINDEX) {
      return registry(L);
   }
   else {
      GCfunc* fn = curr_func(L);
      lj_checkapi(fn->c.gct == ~LJ_TFUNC and !isluafunc(fn),
         "calling frame is not a C function");
      if (idx == LUA_ENVIRONINDEX) {
         TValue * o = &G(L)->tmptv;
         settabV(L, o, tabref(fn->c.env));
         return o;
      }
      else {
         idx = LUA_GLOBALSINDEX - idx;
         return idx <= fn->c.nupvalues ? &fn->c.upvalue[idx - 1] : niltv(L);
      }
   }
}

static LJ_AINLINE TValue * index2adr_check(lua_State *L, int idx)
{
   TValue * o = index2adr(L, idx);
   lj_checkapi(o != niltv(L), "invalid stack slot %d", idx);
   return o;
}

static TValue * index2adr_stack(lua_State *L, int idx)
{
   if (idx > 0) {
      TValue * o = L->base + (idx - 1);
      if (o < L->top) {
         return o;
      }
      else {
         lj_checkapi(0, "invalid stack slot %d", idx);
         return niltv(L);
      }
      return o < L->top ? o : niltv(L);
   }
   else {
      lj_checkapi(idx != 0 and -idx <= L->top - L->base,
         "invalid stack slot %d", idx);
      return L->top + idx;
   }
}

static GCtab* getcurrenv(lua_State *L)
{
   GCfunc* fn = curr_func(L);
   return fn->c.gct == ~LJ_TFUNC ? tabref(fn->c.env) : tabref(L->env);
}

// Deferred expression resolution helpers
//
// NOTE: These helpers work correctly when called from regular C API functions.
// However, calling lj_vm_call from within a fast function (LJLIB_ASM/LJLIB_CF)
// that was itself dispatched by the VM can cause stack corruption.
// This will be addressed by storing type information with deferred expressions.

// Evaluates deferred expression if present, replaces stack slot, returns pointer to value.
// This function modifies the stack slot to replace the deferred expression with its evaluated result.

static TValue * deferred_resolve(lua_State *L, int idx)
{
   TValue * o = index2adr_stack(L, idx);   // Must be on stack, not pseudo-index
   if (o and tvisfunc(o)) {
      GCfunc* fn = funcV(o);
      if (isdeferred(fn)) {
         // Use StackRef to safely track slot across potential stack reallocation
         StackRef slot(L, o);

         // Use SimpleDeferredCall to handle frame setup and invocation
         SimpleDeferredCall call(L);
         TValue* result = call.evaluate(o);

         // Restore slot pointer (may have moved during call) and copy result
         o = slot.get();
         copyTV(L, o, result);
      }
   }
   return o;
}

// Thread-local flag to prevent recursive deferred resolution
static thread_local bool resolving_deferred = false;

// Resolve all deferred expression arguments before a function call.
// This is called from lua_call/lua_pcall to ensure arguments are evaluated
// before being passed to the called function.
// Stack layout: [func][arg1][arg2]...[argN] where top points past argN

static void resolve_call_args(lua_State *L, int nargs)
{
   // Prevent recursive resolution (when lua_call is used to resolve a deferred)
   if (resolving_deferred) return;

   TValue *arg = L->top - nargs;  // First argument
   TValue *end = L->top;          // Past last argument

   for (; arg < end; arg++) {
      if (tvisfunc(arg)) {
         GCfunc *fn = funcV(arg);
         if (isdeferred(fn)) {
            // Track slot position (may move during nested calls)
            ptrdiff_t slot_offset = savestack(L, arg);

            // Push the deferred function for calling
            copyTV(L, L->top, arg);
            L->top++;

            // Set flag to prevent infinite recursion, then call via lua_call
            resolving_deferred = true;
            lua_call(L, 0, 1);  // Call deferred with 0 args, 1 result
            resolving_deferred = false;

            // Result is now at L->top - 1
            TValue *result = L->top - 1;

            // Restore arg pointer (stack may have been reallocated)
            arg = restorestack(L, slot_offset);
            end = L->top - 1;  // Adjust end since we consumed the result slot

            // Copy result to the argument slot
            copyTV(L, arg, result);

            // Pop the result (it's now in the arg slot)
            L->top--;
         }
      }
   }
}

// Variant that returns const pointer for read-only access.
// For pseudo-indices (upvalues, registry, etc.), can't evaluate deferred expressions.

static const TValue * deferred_resolve_const(lua_State *L, int idx)
{
   if (idx <= LUA_REGISTRYINDEX) {
      return index2adr(L, idx);
   }
   return deferred_resolve(L, idx);
}

// Check if a value is a deferred expression
extern int lua_isdeferred(lua_State *L, int idx)
{
   cTValue *o = index2adr(L, idx);
   if (tvisfunc(o)) {
      GCfunc *fn = funcV(o);
      return isdeferred(fn) ? 1 : 0;
   }
   return 0;
}

// Miscellaneous API functions

extern int lua_status(lua_State *L)
{
   return L->status;
}

extern int lua_checkstack(lua_State *L, int size)
{
   if (size > LUAI_MAXCSTACK or (L->top - L->base + size) > LUAI_MAXCSTACK) {
      return 0;  //  Stack overflow.
   }
   else if (size > 0) {
      lj_state_checkstack(L, (MSize)size);
   }
   return 1;
}

extern void luaL_checkstack(lua_State *L, int size, const char* msg)
{
   if (!lua_checkstack(L, size))
      lj_err_callerv(L, ErrMsg::STKOVM, msg);
}

extern void lua_xmove(lua_State *L, lua_State* to, int n)
{
   if (L == to) return;
   lj_checkapi_slot(n);
   lj_checkapi(G(L) == G(to), "move across global states");
   lj_state_checkstack(to, (MSize)n);
   copy_range(to, to->top, L->top - n, n);
   L->top -= n;
   to->top += n;
}

extern const lua_Number * lua_version(lua_State *L)
{
   static const lua_Number version = LUA_VERSION_NUM;
   UNUSED(L);
   return &version;
}

// Stack manipulation

extern int lua_gettop(lua_State *L)
{
   return (int)(L->top - L->base);
}

extern void lua_settop(lua_State *L, int idx)
{
   if (idx >= 0) {
      lj_checkapi(idx <= tvref(L->maxstack) - L->base, "bad stack slot %d", idx);
      if (L->base + idx > L->top) {
         if (L->base + idx >= tvref(L->maxstack))
            lj_state_growstack(L, (MSize)idx - (MSize)(L->top - L->base));
         size_t count = (L->base + idx) - L->top;
         set_range_nil(L->top, count);
         L->top += count;
      }
      else {
         L->top = L->base + idx;
      }
   }
   else {
      lj_checkapi(-(idx + 1) <= (L->top - L->base), "bad stack slot %d", idx);
      L->top += idx + 1;  //  Shrinks top (idx < 0).
   }
}

extern void lua_remove(lua_State *L, int idx)
{
   TValue * p = index2adr_stack(L, idx);
   while (++p < L->top) copyTV(L, p - 1, p);
   L->top--;
}

extern void lua_insert(lua_State *L, int idx)
{
   TValue * q, * p = index2adr_stack(L, idx);
   for (q = L->top; q > p; q--) copyTV(L, q, q - 1);
   copyTV(L, p, L->top);
}

static void copy_slot(lua_State *L, TValue * f, int idx)
{
   if (idx == LUA_GLOBALSINDEX) {
      lj_checkapi(tvistab(f), "stack slot %d is not a table", idx);
      // NOBARRIER: A thread (i.e. L) is never black.
      setgcref(L->env, obj2gco(tabV(f)));
   }
   else if (idx == LUA_ENVIRONINDEX) {
      GCfunc* fn = curr_func(L);
      if (fn->c.gct != ~LJ_TFUNC)
         lj_err_msg(L, ErrMsg::NOENV);
      lj_checkapi(tvistab(f), "stack slot %d is not a table", idx);
      setgcref(fn->c.env, obj2gco(tabV(f)));
      lj_gc_barrier(L, fn, f);
   }
   else {
      TValue * o = index2adr_check(L, idx);
      copyTV(L, o, f);
      if (idx < LUA_GLOBALSINDEX)  //  Need a barrier for upvalues.
         lj_gc_barrier(L, curr_func(L), f);
   }
}

extern void lua_replace(lua_State *L, int idx)
{
   lj_checkapi_slot(1);
   copy_slot(L, L->top - 1, idx);
   L->top--;
}

extern void lua_copy(lua_State *L, int fromidx, int toidx)
{
   copy_slot(L, index2adr(L, fromidx), toidx);
}

extern void lua_pushvalue(lua_State *L, int idx)
{
   copyTV(L, L->top, index2adr(L, idx));
   incr_top(L);
}

// Stack getters

extern int lua_type(lua_State *L, int idx)
{
   cTValue * o;
   // For stack positions, resolve deferred expressions first
   if (idx > LUA_REGISTRYINDEX) {
      o = deferred_resolve_const(L, idx);
   }
   else {
      o = index2adr(L, idx);
   }
   if (tvisnumber(o)) {
      return LUA_TNUMBER;
   }
   else if (o == niltv(L)) {
      return LUA_TNONE;
   }
   else {  // Magic internal/external tag conversion. ORDER LJ_T
      uint32_t t = ~itype(o);
      int tt = (int)((U64x(75a06, 98042110) >> 4 * t) & 15u);
      lj_assertL(tt != LUA_TNIL or tvisnil(o), "bad tag conversion");
      return tt;
   }
}

extern void luaL_checktype(lua_State *L, int idx, int tt)
{
   if (lua_type(L, idx) != tt)
      lj_err_argt(L, idx, tt);
}

extern void luaL_checkany(lua_State *L, int idx)
{
   cTValue * o;
   // Resolve deferred expressions for stack positions
   if (idx > LUA_REGISTRYINDEX) {
      o = deferred_resolve_const(L, idx);
   }
   else {
      o = index2adr(L, idx);
   }
   if (o == niltv(L))
      lj_err_arg(L, idx, ErrMsg::NOVAL);
}

extern const char* lua_typename(lua_State *L, int t)
{
   UNUSED(L);
   return lj_obj_typename[t + 1];
}

extern int lua_iscfunction(lua_State *L, int idx)
{
   cTValue * o;
   // Resolve deferred expressions for stack positions
   if (idx > LUA_REGISTRYINDEX) {
      o = deferred_resolve_const(L, idx);
   }
   else {
      o = index2adr(L, idx);
   }
   return tvisfunc(o) and !isluafunc(funcV(o));
}

extern int lua_isnumber(lua_State *L, int idx)
{
   cTValue * o;
   // Resolve deferred expressions for stack positions
   if (idx > LUA_REGISTRYINDEX) {
      o = deferred_resolve_const(L, idx);
   }
   else {
      o = index2adr(L, idx);
   }
   TValue tmp;
   return (tvisnumber(o) or (tvisstr(o) and lj_strscan_number(strV(o), &tmp)));
}

extern int lua_isstring(lua_State *L, int idx)
{
   cTValue *o = index2adr(L, idx);
   if (tvisstr(o) or tvisnumber(o)) return 1;

   // Check deferred expression's stored type without evaluating
   if (tvisfunc(o)) {
      GCfunc *fn = funcV(o);
      if (isdeferred(fn)) {
         GCproto *pt = funcproto(fn);
         // deferred_type 4 = string, 13 = number (both coerce to string)
         return (pt->deferred_type IS 4 or pt->deferred_type IS 13) ? 1 : 0;
      }
   }
   return 0;
}

extern int lua_isuserdata(lua_State *L, int idx)
{
   cTValue * o;
   // Resolve deferred expressions for stack positions
   if (idx > LUA_REGISTRYINDEX) {
      o = deferred_resolve_const(L, idx);
   }
   else {
      o = index2adr(L, idx);
   }
   return (tvisudata(o) or tvislightud(o));
}

extern int lua_rawequal(lua_State *L, int idx1, int idx2)
{
   cTValue * o1 = index2adr(L, idx1);
   cTValue * o2 = index2adr(L, idx2);
   return (o1 == niltv(L) or o2 == niltv(L)) ? 0 : lj_obj_equal(o1, o2);
}

extern int lua_equal(lua_State *L, int idx1, int idx2)
{
   cTValue * o1 = index2adr(L, idx1);
   cTValue * o2 = index2adr(L, idx2);
   if (tvisint(o1) and tvisint(o2)) {
      return intV(o1) == intV(o2);
   }
   else if (tvisnumber(o1) and tvisnumber(o2)) {
      return numberVnum(o1) == numberVnum(o2);
   }
   else if (itype(o1) != itype(o2)) {
      return 0;
   }
   else if (tvispri(o1)) {
      return o1 != niltv(L) && o2 != niltv(L);
   }
   else if (gcrefeq(o1->gcr, o2->gcr)) {
      return 1;
   }
   else if (!tvistabud(o1)) {
      return 0;
   }
   else {
      TValue * base = lj_meta_equal(L, gcV(o1), gcV(o2), 0);
      if ((uintptr_t)base <= 1) {
         return int(uintptr_t(base));
      }
      return tvistruecond(MetaCall::invoke(L, base, 2));
   }
}

extern int lua_lessthan(lua_State *L, int idx1, int idx2)
{
   cTValue * o1 = index2adr(L, idx1);
   cTValue * o2 = index2adr(L, idx2);
   if (o1 == niltv(L) or o2 == niltv(L)) return 0;
   else if (tvisint(o1) and tvisint(o2)) return intV(o1) < intV(o2);
   else if (tvisnumber(o1) and tvisnumber(o2)) return numberVnum(o1) < numberVnum(o2);
   else {
      TValue * base = lj_meta_comp(L, o1, o2, 0);
      if ((uintptr_t)base <= 1) return int(uintptr_t(base));
      return tvistruecond(MetaCall::invoke(L, base, 2));
   }
}

extern lua_Number lua_tonumber(lua_State *L, int idx)
{
   cTValue * o;
   // Resolve deferred expressions for stack positions
   if (idx > LUA_REGISTRYINDEX) {
      o = deferred_resolve_const(L, idx);
   }
   else {
      o = index2adr(L, idx);
   }
   TValue tmp;
   if (auto num = try_to_number(o, &tmp)) return *num;
   return 0;
}

extern lua_Number lua_tonumberx(lua_State *L, int idx, int* ok)
{
   cTValue * o;
   // Resolve deferred expressions for stack positions
   if (idx > LUA_REGISTRYINDEX) {
      o = deferred_resolve_const(L, idx);
   }
   else {
      o = index2adr(L, idx);
   }
   TValue tmp;
   if (auto num = try_to_number(o, &tmp)) {
      if (ok) *ok = 1;
      return *num;
   }
   if (ok) *ok = 0;
   return 0;
}

extern lua_Number luaL_checknumber(lua_State *L, int idx)
{
   cTValue * o;
   // Resolve deferred expressions for stack positions
   if (idx > LUA_REGISTRYINDEX) {
      o = deferred_resolve_const(L, idx);
   }
   else {
      o = index2adr(L, idx);
   }
   TValue tmp;
   if (auto num = try_to_number(o, &tmp)) return *num;
   lj_err_argt(L, idx, LUA_TNUMBER);
}

extern lua_Number luaL_optnumber(lua_State *L, int idx, lua_Number def)
{
   cTValue * o;
   // Resolve deferred expressions for stack positions
   if (idx > LUA_REGISTRYINDEX) {
      o = deferred_resolve_const(L, idx);
   }
   else {
      o = index2adr(L, idx);
   }
   TValue tmp;
   if (tvisnil(o)) return def;
   if (auto num = try_to_number(o, &tmp)) return *num;
   lj_err_argt(L, idx, LUA_TNUMBER);
}

extern lua_Integer lua_tointeger(lua_State *L, int idx)
{
   cTValue * o;
   // Resolve deferred expressions for stack positions
   if (idx > LUA_REGISTRYINDEX) {
      o = deferred_resolve_const(L, idx);
   }
   else {
      o = index2adr(L, idx);
   }
   TValue tmp;
   if (auto i = try_to_integer(o, &tmp)) return *i;
   return 0;
}

extern lua_Integer lua_tointegerx(lua_State *L, int idx, int* ok)
{
   cTValue * o;
   // Resolve deferred expressions for stack positions
   if (idx > LUA_REGISTRYINDEX) {
      o = deferred_resolve_const(L, idx);
   }
   else {
      o = index2adr(L, idx);
   }
   TValue tmp;
   if (auto i = try_to_integer(o, &tmp)) {
      if (ok) *ok = 1;
      return *i;
   }
   if (ok) *ok = 0;
   return 0;
}

extern lua_Integer luaL_checkinteger(lua_State *L, int idx)
{
   cTValue * o;
   // Resolve deferred expressions for stack positions
   if (idx > LUA_REGISTRYINDEX) {
      o = deferred_resolve_const(L, idx);
   }
   else {
      o = index2adr(L, idx);
   }
   TValue tmp;
   if (auto i = try_to_integer(o, &tmp)) return *i;
   lj_err_argt(L, idx, LUA_TNUMBER);
}

extern lua_Integer luaL_optinteger(lua_State *L, int idx, lua_Integer def)
{
   cTValue * o;
   // Resolve deferred expressions for stack positions
   if (idx > LUA_REGISTRYINDEX) {
      o = deferred_resolve_const(L, idx);
   }
   else {
      o = index2adr(L, idx);
   }
   TValue tmp;
   if (tvisnil(o)) return def;
   if (auto i = try_to_integer(o, &tmp)) return *i;
   lj_err_argt(L, idx, LUA_TNUMBER);
}

extern int lua_toboolean(lua_State *L, int idx)
{
   cTValue * o;
   // Resolve deferred expressions for stack positions
   if (idx > LUA_REGISTRYINDEX) {
      o = deferred_resolve_const(L, idx);
   }
   else {
      o = index2adr(L, idx);
   }
   return tvistruecond(o);
}

extern const char* lua_tolstring(lua_State *L, int idx, size_t* len)
{
   TValue * o;
   // Resolve deferred expressions for stack positions
   if (idx > LUA_REGISTRYINDEX) {
      o = deferred_resolve(L, idx);
   }
   else {
      o = index2adr(L, idx);
   }
   GCstr* s;
   if (LJ_LIKELY(tvisstr(o))) {
      s = strV(o);
   }
   else if (tvisnumber(o)) {
      lj_gc_check(L);
      // Resolve again as GC may move the stack
      if (idx > LUA_REGISTRYINDEX) {
         o = index2adr_stack(L, idx);
      }
      else {
         o = index2adr(L, idx);
      }
      s = lj_strfmt_number(L, o);
      setstrV(L, o, s);
   }
   else {
      if (len != nullptr) *len = 0;
      return nullptr;
   }
   if (len != nullptr) *len = s->len;
   return strdata(s);
}

extern const char* luaL_checklstring(lua_State *L, int idx, size_t* len)
{
   TValue * o;
   // Resolve deferred expressions for stack positions
   if (idx > LUA_REGISTRYINDEX) {
      o = deferred_resolve(L, idx);
   }
   else {
      o = index2adr(L, idx);
   }
   GCstr* s;
   if (LJ_LIKELY(tvisstr(o))) {
      s = strV(o);
   }
   else if (tvisnumber(o)) {
      lj_gc_check(L);
      // Resolve again as GC may move the stack
      if (idx > LUA_REGISTRYINDEX) {
         o = index2adr_stack(L, idx);
      }
      else {
         o = index2adr(L, idx);
      }
      s = lj_strfmt_number(L, o);
      setstrV(L, o, s);
   }
   else {
      lj_err_argt(L, idx, LUA_TSTRING);
   }
   if (len != nullptr) *len = s->len;
   return strdata(s);
}

extern const char* luaL_optlstring(lua_State *L, int idx,
   const char* def, size_t* len)
{
   TValue * o;
   // Resolve deferred expressions for stack positions
   if (idx > LUA_REGISTRYINDEX) {
      o = deferred_resolve(L, idx);
   }
   else {
      o = index2adr(L, idx);
   }
   GCstr* s;
   if (LJ_LIKELY(tvisstr(o))) {
      s = strV(o);
   }
   else if (tvisnil(o)) {
      if (len != nullptr) *len = def ? strlen(def) : 0;
      return def;
   }
   else if (tvisnumber(o)) {
      lj_gc_check(L);
      // Resolve again as GC may move the stack
      if (idx > LUA_REGISTRYINDEX) {
         o = index2adr_stack(L, idx);
      }
      else {
         o = index2adr(L, idx);
      }
      s = lj_strfmt_number(L, o);
      setstrV(L, o, s);
   }
   else {
      lj_err_argt(L, idx, LUA_TSTRING);
   }
   if (len != nullptr) *len = s->len;
   return strdata(s);
}

extern int luaL_checkoption(lua_State *L, int idx, const char* def,
   const char* const lst[])
{
   ptrdiff_t i;
   const char* s = lua_tolstring(L, idx, nullptr);
   if (s == nullptr and (s = def) == nullptr)
      lj_err_argt(L, idx, LUA_TSTRING);
   for (i = 0; lst[i]; i++)
      if (strcmp(lst[i], s) == 0)
         return (int)i;
   lj_err_argv(L, idx, ErrMsg::INVOPTM, s);
}

extern size_t lua_objlen(lua_State *L, int idx)
{
   TValue * o;
   // Resolve deferred expressions for stack positions
   if (idx > LUA_REGISTRYINDEX) {
      o = deferred_resolve(L, idx);
   }
   else {
      o = index2adr(L, idx);
   }
   if (tvisstr(o)) {
      return strV(o)->len;
   }
   else if (tvistab(o)) {
      return (size_t)lj_tab_len(tabV(o));
   }
   else if (tvisudata(o)) {
      return udataV(o)->len;
   }
   else if (tvisnumber(o)) {
      GCstr* s = lj_strfmt_number(L, o);
      setstrV(L, o, s);
      return s->len;
   }
   else {
      return 0;
   }
}

extern lua_CFunction lua_tocfunction(lua_State *L, int idx)
{
   cTValue * o;
   // Resolve deferred expressions for stack positions
   if (idx > LUA_REGISTRYINDEX) {
      o = deferred_resolve_const(L, idx);
   }
   else {
      o = index2adr(L, idx);
   }
   if (tvisfunc(o)) {
      BCOp op = bc_op(*mref(funcV(o)->c.pc, BCIns));
      if (op == BC_FUNCC or op == BC_FUNCCW)
         return funcV(o)->c.f;
   }
   return nullptr;
}

extern void * lua_touserdata(lua_State *L, int idx)
{
   cTValue * o;
   // Resolve deferred expressions for stack positions
   if (idx > LUA_REGISTRYINDEX) {
      o = deferred_resolve_const(L, idx);
   }
   else {
      o = index2adr(L, idx);
   }
   if (tvisudata(o))
      return uddata(udataV(o));
   else if (tvislightud(o))
      return lightudV(G(L), o);
   else
      return nullptr;
}

extern lua_State* lua_tothread(lua_State *L, int idx)
{
   cTValue * o;
   // Resolve deferred expressions for stack positions
   if (idx > LUA_REGISTRYINDEX) {
      o = deferred_resolve_const(L, idx);
   }
   else {
      o = index2adr(L, idx);
   }
   return (!tvisthread(o)) ? nullptr : threadV(o);
}

extern const void * lua_topointer(lua_State *L, int idx)
{
   cTValue * o;
   // Resolve deferred expressions for stack positions
   if (idx > LUA_REGISTRYINDEX) {
      o = deferred_resolve_const(L, idx);
   }
   else {
      o = index2adr(L, idx);
   }
   return lj_obj_ptr(G(L), o);
}

// Stack setters (object creation)

extern void lua_pushnil(lua_State *L)
{
   setnilV(L->top);
   incr_top(L);
}

extern void lua_pushnumber(lua_State *L, lua_Number n)
{
   setnumV(L->top, n);
   if (LJ_UNLIKELY(tvisnan(L->top)))
      setnanV(L->top);  //  Canonicalize injected NaNs.
   incr_top(L);
}

extern void lua_pushinteger(lua_State *L, lua_Integer n)
{
   setintptrV(L->top, n);
   incr_top(L);
}

extern void lua_pushlstring(lua_State *L, const char* str, size_t len)
{
   GCstr* s;
   lj_gc_check(L);
   s = lj_str_new(L, str, len);
   setstrV(L, L->top, s);
   incr_top(L);
}

extern void lua_pushstring(lua_State *L, const char* str)
{
   if (str == nullptr) {
      setnilV(L->top);
   }
   else {
      GCstr* s;
      lj_gc_check(L);
      s = lj_str_newz(L, str);
      setstrV(L, L->top, s);
   }
   incr_top(L);
}

extern const char* lua_pushvfstring(lua_State *L, const char* fmt,
   va_list argp)
{
   lj_gc_check(L);
   return lj_strfmt_pushvf(L, fmt, argp);
}

extern const char* lua_pushfstring(lua_State *L, const char* fmt, ...)
{
   const char* ret;
   va_list argp;
   lj_gc_check(L);
   va_start(argp, fmt);
   ret = lj_strfmt_pushvf(L, fmt, argp);
   va_end(argp);
   return ret;
}

extern void lua_pushcclosure(lua_State *L, lua_CFunction f, int n)
{
   GCfunc* fn;
   lj_gc_check(L);
   lj_checkapi_slot(n);
   fn = lj_func_newC(L, (MSize)n, getcurrenv(L));
   fn->c.f = f;
   L->top -= n;
   while (n--)
      copyTV(L, &fn->c.upvalue[n], L->top + n);
   setfuncV(L, L->top, fn);
   lj_assertL(iswhite(obj2gco(fn)), "new GC object is not white");
   incr_top(L);
}

extern void lua_pushboolean(lua_State *L, int b)
{
   setboolV(L->top, (b != 0));
   incr_top(L);
}

extern void lua_pushlightuserdata(lua_State *L, void * p)
{
   p = lj_lightud_intern(L, p);
   setrawlightudV(L->top, p);
   incr_top(L);
}

extern void lua_createtable(lua_State *L, int narray, int nrec)
{
   lj_gc_check(L);
   settabV(L, L->top, lj_tab_new_ah(L, narray, nrec));
   incr_top(L);
}

extern int luaL_newmetatable(lua_State *L, const char* tname)
{
   GCtab* regt = tabV(registry(L));
   TValue * tv = lj_tab_setstr(L, regt, lj_str_newz(L, tname));
   if (tvisnil(tv)) {
      GCtab* mt = lj_tab_new(L, 0, 1);
      settabV(L, tv, mt);
      settabV(L, L->top++, mt);
      lj_gc_anybarriert(L, regt);
      return 1;
   }
   else {
      copyTV(L, L->top++, tv);
      return 0;
   }
}

extern int lua_pushthread(lua_State *L)
{
   setthreadV(L, L->top, L);
   incr_top(L);
   return (mainthread(G(L)) == L);
}

extern lua_State* lua_newthread(lua_State *L)
{
   lua_State *L1;
   lj_gc_check(L);
   L1 = lj_state_new(L);
   setthreadV(L, L->top, L1);
   incr_top(L);
   return L1;
}

extern void * lua_newuserdata(lua_State *L, size_t size)
{
   GCudata* ud;
   lj_gc_check(L);
   if (size > LJ_MAX_UDATA)
      lj_err_msg(L, ErrMsg::UDATAOV);
   ud = lj_udata_new(L, (MSize)size, getcurrenv(L));
   setudataV(L, L->top, ud);
   incr_top(L);
   return uddata(ud);
}

extern void lua_concat(lua_State *L, int n)
{
   lj_checkapi_slot(n);
   if (n >= 2) {
      // Resolve any deferred expressions in the concat range first
      for (int i = 0; i < n; i++) {
         TValue *slot = L->top - n + i;
         if (tvisfunc(slot)) {
            GCfunc *fn = funcV(slot);
            if (isdeferred(fn)) {
               SimpleDeferredCall call(L);
               TValue *result = call.evaluate(slot);
               // Stack may have moved, recalculate slot
               slot = L->top - n + i;
               copyTV(L, slot, result);
            }
         }
      }
      n--;
      do {
         TValue * top = lj_meta_cat(L, L->top - 1, -n);
         if (top == nullptr) {
            L->top -= n;
            break;
         }
         n -= MetaCall::invokeConcat(L, top);
      } while (--n > 0);
   }
   else if (n == 0) {  // Push empty string.
      setstrV(L, L->top, &G(L)->strempty);
      incr_top(L);
   }
   // else n == 1: nothing to do.
}

// Object getters

extern void lua_gettable(lua_State *L, int idx)
{
   cTValue * t = index2adr_check(L, idx);
   cTValue * v = lj_meta_tget(L, t, L->top - 1);
   if (v == nullptr) {
      v = MetaCall::invokeGet(L);
   }
   copyTV(L, L->top - 1, v);
}

extern void lua_getfield(lua_State *L, int idx, const char* k)
{
   cTValue * v, * t = index2adr_check(L, idx);
   TValue key;
   setstrV(L, &key, lj_str_newz(L, k));
   v = lj_meta_tget(L, t, &key);
   if (v == nullptr) {
      v = MetaCall::invokeGet(L);
   }
   copyTV(L, L->top, v);
   incr_top(L);
}

extern void lua_rawget(lua_State *L, int idx)
{
   cTValue * t = index2adr(L, idx);
   lj_checkapi(tvistab(t), "stack slot %d is not a table", idx);
   copyTV(L, L->top - 1, lj_tab_get(L, tabV(t), L->top - 1));
}

extern void lua_rawgeti(lua_State *L, int idx, int n)
{
   cTValue * v, * t = index2adr(L, idx);
   lj_checkapi(tvistab(t), "stack slot %d is not a table", idx);
   v = lj_tab_getint(tabV(t), n);
   copy_or_nil(L, L->top, v);
   incr_top(L);
}

extern int lua_getmetatable(lua_State *L, int idx)
{
   cTValue * o = index2adr(L, idx);
   GCtab* mt = nullptr;
   if (tvistab(o)) mt = tabref(tabV(o)->metatable);
   else if (tvisudata(o)) mt = tabref(udataV(o)->metatable);
   else mt = tabref(basemt_obj(G(L), o));
   if (mt == nullptr) return 0;
   settabV(L, L->top, mt);
   incr_top(L);
   return 1;
}

extern int luaL_getmetafield(lua_State *L, int idx, const char* field)
{
   if (lua_getmetatable(L, idx)) {
      cTValue * tv = lj_tab_getstr(tabV(L->top - 1), lj_str_newz(L, field));
      if (tv and !tvisnil(tv)) {
         copyTV(L, L->top - 1, tv);
         return 1;
      }
      L->top--;
   }
   return 0;
}

extern void lua_getfenv(lua_State *L, int idx)
{
   cTValue * o = index2adr_check(L, idx);
   if (tvisfunc(o)) settabV(L, L->top, tabref(funcV(o)->c.env));
   else if (tvisudata(o)) settabV(L, L->top, tabref(udataV(o)->env));
   else if (tvisthread(o)) settabV(L, L->top, tabref(threadV(o)->env));
   else setnilV(L->top);
   incr_top(L);
}

extern int lua_next(lua_State *L, int idx)
{
   cTValue * t = index2adr(L, idx);
   int more;
   lj_checkapi(tvistab(t), "stack slot %d is not a table", idx);
   more = lj_tab_next(tabV(t), L->top - 1, L->top - 1);
   if (more > 0) {
      incr_top(L);  //  Return new key and value slot.
   }
   else if (!more) {  // End of traversal.
      L->top--;  //  Remove key slot.
   }
   else lj_err_msg(L, ErrMsg::NEXTIDX);

   return more;
}

extern const char* lua_getupvalue(lua_State *L, int idx, int n)
{
   TValue * val;
   GCobj* o;
   auto name = lj_debug_uvnamev(index2adr(L, idx), (uint32_t)(n - 1), &val, &o);
   if (name) {
      copyTV(L, L->top, val);
      incr_top(L);
   }
   return name;
}

extern void * lua_upvalueid(lua_State *L, int idx, int n)
{
   GCfunc* fn = funcV(index2adr(L, idx));
   n--;
   lj_checkapi((uint32_t)n < fn->l.nupvalues, "bad upvalue %d", n);
   return isluafunc(fn) ? (void *)gcref(fn->l.uvptr[n]) : (void *)&fn->c.upvalue[n];
}

extern void lua_upvaluejoin(lua_State *L, int idx1, int n1, int idx2, int n2)
{
   GCfunc* fn1 = funcV(index2adr(L, idx1));
   GCfunc* fn2 = funcV(index2adr(L, idx2));
   n1--; n2--;
   lj_checkapi(isluafunc(fn1), "stack slot %d is not a Lua function", idx1);
   lj_checkapi(isluafunc(fn2), "stack slot %d is not a Lua function", idx2);
   lj_checkapi((uint32_t)n1 < fn1->l.nupvalues, "bad upvalue %d", n1 + 1);
   lj_checkapi((uint32_t)n2 < fn2->l.nupvalues, "bad upvalue %d", n2 + 1);
   setgcrefr(fn1->l.uvptr[n1], fn2->l.uvptr[n2]);
   lj_gc_objbarrier(L, fn1, gcref(fn1->l.uvptr[n1]));
}

extern void * luaL_testudata(lua_State *L, int idx, const char* tname)
{
   cTValue * o = index2adr(L, idx);
   if (tvisudata(o)) {
      GCudata* ud = udataV(o);
      cTValue * tv = lj_tab_getstr(tabV(registry(L)), lj_str_newz(L, tname));
      if (tv and tvistab(tv) and tabV(tv) == tabref(ud->metatable))
         return uddata(ud);
   }
   return nullptr;  //  value is not a userdata with a metatable
}

extern void * luaL_checkudata(lua_State *L, int idx, const char* tname)
{
   void * p = luaL_testudata(L, idx, tname);
   if (!p) lj_err_argtype(L, idx, tname);
   return p;
}

// Object setters

extern void lua_settable(lua_State *L, int idx)
{
   TValue * o;
   cTValue * t = index2adr_check(L, idx);
   lj_checkapi_slot(2);
   o = lj_meta_tset(L, t, L->top - 2);
   if (o) {
      // NOBARRIER: lj_meta_tset ensures the table is not black.
      L->top -= 2;
      copyTV(L, o, L->top + 1);
   }
   else {
      MetaCall::invokeSetTable(L, L->top);
   }
}

extern void lua_setfield(lua_State *L, int idx, const char* k)
{
   TValue * o;
   TValue key;
   cTValue * t = index2adr_check(L, idx);
   lj_checkapi_slot(1);
   setstrV(L, &key, lj_str_newz(L, k));
   o = lj_meta_tset(L, t, &key);
   if (o) {
      // NOBARRIER: lj_meta_tset ensures the table is not black.
      copyTV(L, o, --L->top);
   }
   else {
      MetaCall::invokeSetField(L, L->top);
   }
}

extern void lua_rawset(lua_State *L, int idx)
{
   GCtab* t = tabV(index2adr(L, idx));
   TValue * dst, * key;
   lj_checkapi_slot(2);
   key = L->top - 2;
   dst = lj_tab_set(L, t, key);
   copyTV(L, dst, key + 1);
   lj_gc_anybarriert(L, t);
   L->top = key;
}

extern void lua_rawseti(lua_State *L, int idx, int n)
{
   GCtab* t = tabV(index2adr(L, idx));
   TValue * dst, * src;
   lj_checkapi_slot(1);
   dst = lj_tab_setint(L, t, n);
   src = L->top - 1;
   copyTV(L, dst, src);
   lj_gc_barriert(L, t, dst);
   L->top = src;
}

extern int lua_setmetatable(lua_State *L, int idx)
{
   GCtab *mt;
   cTValue *o = index2adr_check(L, idx);

   lj_checkapi_slot(1);
   if (tvisnil(L->top - 1)) {
      mt = nullptr;
   }
   else {
      lj_checkapi(tvistab(L->top - 1), "top stack slot is not a table");
      mt = tabV(L->top - 1);
   }

   auto g = G(L);
   if (tvistab(o)) {
      setgcref(tabV(o)->metatable, obj2gco(mt));
      if (mt) lj_gc_objbarriert(L, tabV(o), mt);
   }
   else if (tvisudata(o)) {
      setgcref(udataV(o)->metatable, obj2gco(mt));
      if (mt) lj_gc_objbarrier(L, udataV(o), mt);
   }
   else {
      // Flush cache, since traces specialize to basemt. But not during __gc.
      if (lj_trace_flushall(L)) lj_err_caller(L, ErrMsg::NOGCMM);
      if (tvisbool(o)) {
         // NOBARRIER: basemt is a GC root.
         setgcref(basemt_it(g, LJ_TTRUE), obj2gco(mt));
         setgcref(basemt_it(g, LJ_TFALSE), obj2gco(mt));
      }
      else {
         // NOBARRIER: basemt is a GC root.
         setgcref(basemt_obj(g, o), obj2gco(mt));
      }
   }
   L->top--;
   return 1;
}

extern void luaL_setmetatable(lua_State *L, const char* tname)
{
   lua_getfield(L, LUA_REGISTRYINDEX, tname);
   lua_setmetatable(L, -2);
}

extern int lua_setfenv(lua_State *L, int idx)
{
   cTValue * o = index2adr_check(L, idx);
   GCtab* t;
   lj_checkapi_slot(1);
   lj_checkapi(tvistab(L->top - 1), "top stack slot is not a table");
   t = tabV(L->top - 1);
   if (tvisfunc(o)) {
      setgcref(funcV(o)->c.env, obj2gco(t));
   }
   else if (tvisudata(o)) {
      setgcref(udataV(o)->env, obj2gco(t));
   }
   else if (tvisthread(o)) {
      setgcref(threadV(o)->env, obj2gco(t));
   }
   else {
      L->top--;
      return 0;
   }
   lj_gc_objbarrier(L, gcV(o), t);
   L->top--;
   return 1;
}

extern const char* lua_setupvalue(lua_State *L, int idx, int n)
{
   cTValue *f = index2adr(L, idx);
   TValue *val;
   GCobj *o;

   lj_checkapi_slot(1);
   auto name = lj_debug_uvnamev(f, (uint32_t)(n - 1), &val, &o);
   if (name) {
      L->top--;
      copyTV(L, val, L->top);
      lj_gc_barrier(L, o, L->top);
   }
   return name;
}

// Calls

static TValue * api_call_base(lua_State *L, int nargs)
{
   TValue * o = L->top, * base = o - nargs;
   L->top = o + 1;
   for (; o > base; o--) copyTV(L, o, o - 1);
   setnilV(o);
   return o + 1;
}

extern void lua_call(lua_State *L, int nargs, int nresults)
{
   lj_checkapi(L->status == LUA_OK or L->status == LUA_ERRERR, "thread called in wrong state %d", L->status);
   lj_checkapi_slot(nargs + 1);
   resolve_call_args(L, nargs);  // Resolve deferred expressions in arguments
   lj_vm_call(L, api_call_base(L, nargs), nresults + 1);
}

extern int lua_pcall(lua_State *L, int nargs, int nresults, int errfunc)
{
   global_State* g = G(L);
   uint8_t oldh = hook_save(g);
   ptrdiff_t ef;
   int status;

   lj_checkapi(L->status == LUA_OK or L->status == LUA_ERRERR, "thread called in wrong state %d", L->status);
   lj_checkapi_slot(nargs + 1);
   resolve_call_args(L, nargs);  // Resolve deferred expressions in arguments

   if (errfunc == 0) ef = 0;
   else {
      auto o = index2adr_stack(L, errfunc);
      ef = savestack(L, o);
   }
   status = lj_vm_pcall(L, api_call_base(L, nargs), nresults + 1, ef);
   if (status) hook_restore(g, oldh);
   return status;
}

static TValue * cpcall(lua_State *L, lua_CFunction func, void * ud)
{
   GCfunc* fn = lj_func_newC(L, 0, getcurrenv(L));
   TValue * top = L->top;
   fn->c.f = func;
   setfuncV(L, top++, fn);
   setnilV(top++);
   ud = lj_lightud_intern(L, ud);
   setrawlightudV(top++, ud);
   cframe_nres(L->cframe) = 1 + 0;  //  Zero results.
   L->top = top;
   return top - 1;  //  Now call the newly allocated C function.
}

extern int lua_cpcall(lua_State *L, lua_CFunction func, void * ud)
{
   global_State* g = G(L);
   uint8_t oldh = hook_save(g);
   int status;
   lj_checkapi(L->status == LUA_OK or L->status == LUA_ERRERR,
      "thread called in wrong state %d", L->status);
   status = lj_vm_cpcall(L, func, ud, cpcall);
   if (status) hook_restore(g, oldh);
   return status;
}

extern int luaL_callmeta(lua_State *L, int idx, const char* field)
{
   if (luaL_getmetafield(L, idx, field)) {
      // luaL_getmetafield pushed the metamethod at L->top - 1
      // We need to call it with the object at idx as argument
      TValue* fn = L->top - 1;
      VMCall call(L);
      call.func(fn);
      call.arg(index2adr(L, idx));
      (void)call.invoke(1);  // Result left on stack by VMCall
      return 1;
   }
   return 0;
}

// Coroutine yield and resume

extern int lua_isyieldable(lua_State *L)
{
   return cframe_canyield(L->cframe);
}

extern int lua_yield(lua_State *L, int nresults)
{
   void * cf = L->cframe;
   global_State* g = G(L);
   if (cframe_canyield(cf)) {
      cf = cframe_raw(cf);
      if (!hook_active(g)) {  // Regular yield: move results down if needed.
         cTValue * f = L->top - nresults;
         if (f > L->base) {
            copy_range(L, L->base, f, nresults);
            L->top = L->base + nresults;
         }
         L->cframe = nullptr;
         L->status = LUA_YIELD;
         return -1;
      }
      else {  // Yield from hook: add a pseudo-frame.
         TValue * top = L->top;
         hook_leave(g);
         (top++)->u64 = cframe_multres(cf);
         setcont(top, lj_cont_hook);
         top++;
         setframe_pc(top, cframe_pc(cf) - 1);
         top++;
         setframe_gc(top, obj2gco(L), LJ_TTHREAD);
         top++;
         setframe_ftsz(top, ((char*)(top + 1) - (char*)L->base) + FRAME_CONT);
         L->top = L->base = top + 1;
#if ((defined(__GNUC__) or defined(__clang__)) && (LJ_TARGET_X64 or defined(LUAJIT_UNWIND_EXTERNAL)) && !LJ_NO_UNWIND) or LJ_TARGET_WINDOWS
         lj_err_throw(L, LUA_YIELD);
#else
         L->cframe = nullptr;
         L->status = LUA_YIELD;
         lj_vm_unwind_c(cf, LUA_YIELD);
#endif
      }
   }
   lj_err_msg(L, ErrMsg::CYIELD);
   return 0;  //  unreachable
}

extern int lua_resume(lua_State *L, int nargs)
{
   if (L->cframe == nullptr and L->status <= LUA_YIELD) {
      if (L->status == LUA_OK) resolve_call_args(L, nargs);  // Resolve deferred expressions
      return lj_vm_resume(L, L->status == LUA_OK ? api_call_base(L, nargs) : L->top - nargs, 0, 0);
   }
   L->top = L->base;
   setstrV(L, L->top, lj_err_str(L, ErrMsg::COSUSP));
   incr_top(L);
   return LUA_ERRRUN;
}

// GC and memory management

extern int lua_gc(lua_State *L, int what, int data)
{
   global_State* g = G(L);
   int res = 0;
   switch (what) {
      case LUA_GCSTOP:
         g->gc.threshold = LJ_MAX_MEM;
         break;
      case LUA_GCRESTART:
         g->gc.threshold = data == -1 ? (g->gc.total / 100) * g->gc.pause : g->gc.total;
         break;
      case LUA_GCCOLLECT:
         lj_gc_fullgc(L);
         break;
      case LUA_GCCOUNT:
         res = (int)(g->gc.total >> 10);
         break;
      case LUA_GCCOUNTB:
         res = (int)(g->gc.total & 0x3ff);
         break;
      case LUA_GCSTEP: {
         GCSize a = (GCSize)data << 10;
         g->gc.threshold = (a <= g->gc.total) ? (g->gc.total - a) : 0;
         while (g->gc.total >= g->gc.threshold)
            if (lj_gc_step(L) > 0) {
               res = 1;
               break;
            }
         break;
      }
      case LUA_GCSETPAUSE:
         res = (int)(g->gc.pause);
         g->gc.pause = (MSize)data;
         break;
      case LUA_GCSETSTEPMUL:
         res = (int)(g->gc.stepmul);
         g->gc.stepmul = (MSize)data;
         break;
      case LUA_GCISRUNNING:
         res = (g->gc.threshold != LJ_MAX_MEM);
         break;
      default:
         res = -1;  //  Invalid option.
   }
   return res;
}

extern lua_Alloc lua_getallocf(lua_State *L, void ** ud)
{
   global_State* g = G(L);
   if (ud) *ud = g->allocd;
   return g->allocf;
}

extern void lua_setallocf(lua_State *L, lua_Alloc f, void * ud)
{
   global_State* g = G(L);
   g->allocd = ud;
   g->allocf = f;
}
