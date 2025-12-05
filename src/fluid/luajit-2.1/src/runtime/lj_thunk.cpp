// Thunk (deferred evaluation) implementation for Fluid.
// Provides automatic lazy evaluation through userdata metatables.

#include <cmath>
#include <cstring>

#include "lj_obj.h"
#include "lj_gc.h"
#include "lj_err.h"
#include "lj_tab.h"
#include "lj_udata.h"
#include "lj_meta.h"
#include "lj_state.h"
#include "lj_frame.h"
#include "lj_strfmt.h"
#include "lj_str.h"
#include "lualib.h"
#include "lj_thunk.h"

using std::floor;
using std::pow;

// Global reference to thunk metatable (stored in registry)
#define THUNK_METATABLE_NAME  "fluid.thunk"

//********************************************************************************************************************
// Check if a TValue is a thunk

int lj_thunk_isthunk(cTValue *o)
{
   if (tvisudata(o)) {
      GCudata *ud = udataV(o);
      return ud->udtype IS UDTYPE_THUNK;
   }
   return 0;
}

//********************************************************************************************************************
// Create a new thunk userdata

void lj_thunk_new(lua_State *L, GCfunc *func, int expected_type)
{
   // Allocate userdata with ThunkPayload - use global environment for GC traversal
   GCudata *ud = lj_udata_new(L, sizeof(ThunkPayload), tabref(L->env));
   ud->udtype = UDTYPE_THUNK;

   // Initialize payload
   ThunkPayload *payload = thunk_payload(ud);
   setgcref(payload->deferred_func, obj2gco(func));
   setnilV(&payload->cached_value);
   payload->resolved = 0;
   payload->expected_type = (uint8_t)expected_type;
   payload->padding = 0;

   // Set metatable from registry
   cTValue *tv = lj_tab_getstr(tabV(registry(L)), lj_str_newz(L, THUNK_METATABLE_NAME));
   if (tv and tvistab(tv)) {
      setgcref(ud->metatable, obj2gco(tabV(tv)));
   }

   // Push userdata to stack
   setudataV(L, L->top, ud);
   incr_top(L);

   // GC barrier for the function reference
   lj_gc_objbarrier(L, obj2gco(ud), obj2gco(func));
}

//********************************************************************************************************************
// Resolve a thunk if not already resolved

TValue* lj_thunk_resolve(lua_State *L, GCudata *thunk_udata)
{
   ThunkPayload *payload = thunk_payload(thunk_udata);

   // If already resolved, return cached value
   if (payload->resolved) {
      return &payload->cached_value;
   }

   // Get the deferred function
   GCfunc *fn = gco2func(gcref(payload->deferred_func));

   // Push function to stack
   TValue *base = L->top;
   setfuncV(L, base, fn);
   L->top = base + 1;

   // Call the function (0 arguments, 1 result)
   lua_call(L, 0, 1);

   // Result is at L->top-1
   TValue *result = L->top - 1;

   // Cache the result
   copyTV(L, &payload->cached_value, result);
   payload->resolved = 1;

   // Pop the result (it's cached in payload)
   L->top--;

   // GC barrier for the cached value if it's a GC object
   if (tvisgcv(&payload->cached_value)) {
      lj_gc_objbarrier(L, obj2gco(thunk_udata), gcval(&payload->cached_value));
   }

   return &payload->cached_value;
}

//********************************************************************************************************************
// Get the current value of a thunk

cTValue* lj_thunk_getvalue(lua_State *L, cTValue *o)
{
   UNUSED(L);
   if (lj_thunk_isthunk(o)) {
      GCudata *ud = udataV(o);
      ThunkPayload *payload = thunk_payload(ud);
      if (payload->resolved) {
         return &payload->cached_value;
      }
   }
   return o;
}

//********************************************************************************************************************
// Metamethod implementations

// Forward declarations for metamethods
static int thunk_add(lua_State *L);
static int thunk_sub(lua_State *L);
static int thunk_mul(lua_State *L);
static int thunk_div(lua_State *L);
static int thunk_mod(lua_State *L);
static int thunk_pow(lua_State *L);
static int thunk_unm(lua_State *L);
static int thunk_concat(lua_State *L);
static int thunk_eq(lua_State *L);
static int thunk_lt(lua_State *L);
static int thunk_le(lua_State *L);
static int thunk_index(lua_State *L);
static int thunk_newindex(lua_State *L);
static int thunk_len(lua_State *L);
static int thunk_call(lua_State *L);
static int thunk_tostring(lua_State *L);

// Helper: resolve thunk at stack position, return pointer to resolved value
static TValue* resolve_at(lua_State *L, int idx)
{
   TValue *o = L->base + idx;
   if (lj_thunk_isthunk(o)) {
      return lj_thunk_resolve(L, udataV(o));
   }
   return o;
}

// Helper: Get number value from TValue, handling integer and number types
static lua_Number getnumvalue(TValue *o)
{
   if (tvisint(o)) return (lua_Number)intV(o);
   return numV(o);
}

//********************************************************************************************************************
// Binary arithmetic metamethods

static int thunk_add(lua_State *L)
{
   TValue *a = resolve_at(L, 0);
   TValue *b = resolve_at(L, 1);

   if (tvisnumber(a) and tvisnumber(b)) {
      lua_Number result = getnumvalue(a) + getnumvalue(b);
      setnumV(L->top++, result);
      return 1;
   }

   lj_err_optype(L, tvisnumber(a) ? b : a, ErrMsg::OPARITH);
   return 0;
}

static int thunk_sub(lua_State *L)
{
   TValue *a = resolve_at(L, 0);
   TValue *b = resolve_at(L, 1);

   if (tvisnumber(a) and tvisnumber(b)) {
      lua_Number result = getnumvalue(a) - getnumvalue(b);
      setnumV(L->top++, result);
      return 1;
   }

   lj_err_optype(L, tvisnumber(a) ? b : a, ErrMsg::OPARITH);
   return 0;
}

static int thunk_mul(lua_State *L)
{
   TValue *a = resolve_at(L, 0);
   TValue *b = resolve_at(L, 1);

   if (tvisnumber(a) and tvisnumber(b)) {
      lua_Number result = getnumvalue(a) * getnumvalue(b);
      setnumV(L->top++, result);
      return 1;
   }

   lj_err_optype(L, tvisnumber(a) ? b : a, ErrMsg::OPARITH);
   return 0;
}

static int thunk_div(lua_State *L)
{
   TValue *a = resolve_at(L, 0);
   TValue *b = resolve_at(L, 1);

   if (tvisnumber(a) and tvisnumber(b)) {
      lua_Number result = getnumvalue(a) / getnumvalue(b);
      setnumV(L->top++, result);
      return 1;
   }

   lj_err_optype(L, tvisnumber(a) ? b : a, ErrMsg::OPARITH);
   return 0;
}

static int thunk_mod(lua_State *L)
{
   TValue *a = resolve_at(L, 0);
   TValue *b = resolve_at(L, 1);

   if (tvisnumber(a) and tvisnumber(b)) {
      lua_Number na = getnumvalue(a);
      lua_Number nb = getnumvalue(b);
      lua_Number result = na - floor(na / nb) * nb;
      setnumV(L->top++, result);
      return 1;
   }

   lj_err_optype(L, tvisnumber(a) ? b : a, ErrMsg::OPARITH);
   return 0;
}

static int thunk_pow(lua_State *L)
{
   TValue *a = resolve_at(L, 0);
   TValue *b = resolve_at(L, 1);

   if (tvisnumber(a) and tvisnumber(b)) {
      lua_Number result = pow(getnumvalue(a), getnumvalue(b));
      setnumV(L->top++, result);
      return 1;
   }

   lj_err_optype(L, tvisnumber(a) ? b : a, ErrMsg::OPARITH);
   return 0;
}

// Unary minus
static int thunk_unm(lua_State *L)
{
   TValue *o = resolve_at(L, 0);

   if (tvisnumber(o)) {
      lua_Number result = -getnumvalue(o);
      setnumV(L->top++, result);
      return 1;
   }

   lj_err_optype(L, o, ErrMsg::OPARITH);
   return 0;
}

//********************************************************************************************************************
// Concatenation

static int thunk_concat(lua_State *L)
{
   TValue *a = resolve_at(L, 0);
   TValue *b = resolve_at(L, 1);
   copyTV(L, L->top, a);
   L->top++;
   copyTV(L, L->top, b);
   L->top++;
   lua_concat(L, 2);
   return 1;
}

//********************************************************************************************************************
// Comparison metamethods

// Equality - compares resolved values
static int thunk_eq(lua_State *L)
{
   TValue *a = resolve_at(L, 0);
   TValue *b = resolve_at(L, 1);

   int result;
   if (tvisnumber(a) and tvisnumber(b)) {
      result = (getnumvalue(a) == getnumvalue(b));
   } else if (tvisstr(a) and tvisstr(b)) {
      result = (strV(a) == strV(b));  // String interning means pointer comparison works
   } else if (tvistab(a) and tvistab(b)) {
      result = (tabV(a) == tabV(b));  // Reference equality for tables
   } else if (tvisnil(a) and tvisnil(b)) {
      result = 1;
   } else if (tvisbool(a) and tvisbool(b)) {
      result = (boolV(a) == boolV(b));
   } else {
      // Different types are not equal (except number subtypes handled above)
      result = 0;
   }

   setboolV(L->top++, result);
   return 1;
}

// Less than - compares resolved values
static int thunk_lt(lua_State *L)
{
   TValue *a = resolve_at(L, 0);
   TValue *b = resolve_at(L, 1);

   int result;
   if (tvisnumber(a) and tvisnumber(b)) {
      result = (getnumvalue(a) < getnumvalue(b));
   } else if (tvisstr(a) and tvisstr(b)) {
      // String comparison
      GCstr *sa = strV(a);
      GCstr *sb = strV(b);
      size_t len = (sa->len < sb->len) ? sa->len : sb->len;
      int cmp = memcmp(strdata(sa), strdata(sb), len);
      if (cmp IS 0) {
         result = (sa->len < sb->len);
      } else {
         result = (cmp < 0);
      }
   } else {
      lj_err_comp(L, a, b);
      return 0;
   }

   setboolV(L->top++, result);
   return 1;
}

// Less than or equal - compares resolved values
static int thunk_le(lua_State *L)
{
   TValue *a = resolve_at(L, 0);
   TValue *b = resolve_at(L, 1);

   int result;
   if (tvisnumber(a) and tvisnumber(b)) {
      result = (getnumvalue(a) <= getnumvalue(b));
   } else if (tvisstr(a) and tvisstr(b)) {
      // String comparison
      GCstr *sa = strV(a);
      GCstr *sb = strV(b);
      size_t len = (sa->len < sb->len) ? sa->len : sb->len;
      int cmp = memcmp(strdata(sa), strdata(sb), len);
      if (cmp IS 0) {
         result = (sa->len <= sb->len);
      } else {
         result = (cmp <= 0);
      }
   } else {
      lj_err_comp(L, a, b);
      return 0;
   }

   setboolV(L->top++, result);
   return 1;
}

//********************************************************************************************************************
// Index (field access) - resolves thunk then performs table lookup

static int thunk_index(lua_State *L)
{
   TValue *o = resolve_at(L, 0);
   TValue *key = L->base + 1;

   if (tvistab(o)) {
      GCtab *t = tabV(o);
      cTValue *res = lj_tab_get(L, t, key);

      // If not found and table has metatable, try __index
      if (tvisnil(res)) {
         GCtab *mt = tabref(t->metatable);
         if (mt) {
            cTValue *idx = lj_tab_getstr(mt, lj_str_newlit(L, "__index"));
            if (idx and not tvisnil(idx)) {
               if (tvisfunc(idx)) {
                  // __index is a function: call __index(table, key)
                  copyTV(L, L->top, idx);
                  copyTV(L, L->top + 1, o);
                  copyTV(L, L->top + 2, key);
                  L->top += 3;
                  lua_call(L, 2, 1);
                  // Result is already at L->top-1, which is now L->top after lua_call returns
                  return 1;
               } else if (tvistab(idx)) {
                  // __index is a table: look up key in that table
                  res = lj_tab_get(L, tabV(idx), key);
               }
            }
         }
      }
      copyTV(L, L->top++, res);
      return 1;
   }

   // Not a table - error
   lj_err_optype(L, o, ErrMsg::OPINDEX);
   return 0;
}

//********************************************************************************************************************
// Newindex (field assignment) - resolves thunk then performs table assignment

static int thunk_newindex(lua_State *L)
{
   TValue *o = resolve_at(L, 0);
   TValue *key = L->base + 1;
   TValue *val = L->base + 2;

   if (tvistab(o)) {
      GCtab *t = tabV(o);
      TValue *slot = lj_tab_set(L, t, key);
      copyTV(L, slot, val);
      lj_gc_anybarriert(L, t);
      return 0;
   }

   // Not a table - error
   lj_err_optype(L, o, ErrMsg::OPINDEX);
   return 0;
}

//********************************************************************************************************************
// Length operator - resolves thunk then gets length

static int thunk_len(lua_State *L)
{
   TValue *o = resolve_at(L, 0);

   if (tvistab(o)) {
      setintV(L->top++, (int32_t)lj_tab_len(tabV(o)));
      return 1;
   } else if (tvisstr(o)) {
      setintV(L->top++, (int32_t)strV(o)->len);
      return 1;
   }

   // Not a table or string - error
   lj_err_optype(L, o, ErrMsg::OPLEN);
   return 0;
}

//********************************************************************************************************************
// Call operator - resolves thunk then calls the resolved value if callable

static int thunk_call(lua_State *L)
{
   TValue *o = resolve_at(L, 0);
   int nargs = (int)(L->top - L->base - 1);  // Number of arguments (exclude thunk itself)

   if (not tvisfunc(o)) {
      // Check if it's a table with __call metamethod
      if (tvistab(o)) {
         GCtab *mt = tabref(tabV(o)->metatable);
         if (mt) {
            cTValue *call_mm = lj_tab_getstr(mt, lj_str_newlit(L, "__call"));
            if (call_mm and tvisfunc(call_mm)) {
               // Build call: __call(resolved_table, arg1, arg2, ...)
               // We need to insert the function at base and shift the table
               // Push the __call function
               copyTV(L, L->top, call_mm);
               // Push resolved table
               copyTV(L, L->top + 1, o);
               // Copy arguments
               for (int i = 0; i < nargs; i++) {
                  copyTV(L, L->top + 2 + i, L->base + 1 + i);
               }
               L->top += 2 + nargs;
               lua_call(L, 1 + nargs, LUA_MULTRET);  // __call + table + original args
               return (int)(L->top - L->base);
            }
         }
      }
      lj_err_optype(L, o, ErrMsg::OPCALL);
      return 0;
   }

   // Resolved to a function - call with the arguments
   // Push function
   copyTV(L, L->top, o);
   // Copy arguments
   for (int i = 0; i < nargs; i++) {
      copyTV(L, L->top + 1 + i, L->base + 1 + i);
   }
   L->top += 1 + nargs;
   lua_call(L, nargs, LUA_MULTRET);

   return (int)(L->top - L->base);
}

//********************************************************************************************************************
// String conversion - resolves thunk then converts to string

static int thunk_tostring(lua_State *L)
{
   TValue *o = resolve_at(L, 0);

   if (tvisstr(o)) {
      copyTV(L, L->top++, o);
      return 1;
   } else if (tvisnumber(o)) {
      GCstr *s = lj_strfmt_number(L, o);
      setstrV(L, L->top++, s);
      return 1;
   } else if (tvisnil(o)) {
      setstrV(L, L->top++, lj_str_newlit(L, "nil"));
      return 1;
   } else if (tvisbool(o)) {
      if (boolV(o)) {
         setstrV(L, L->top++, lj_str_newlit(L, "true"));
      } else {
         setstrV(L, L->top++, lj_str_newlit(L, "false"));
      }
      return 1;
   }

   // For tables and other types, call the global tostring function
   lua_getglobal(L, "tostring");
   copyTV(L, L->top, o);
   L->top++;
   lua_call(L, 1, 1);
   // Result is already at L->top-1 after lua_call
   return 1;
}

//********************************************************************************************************************
// Initialize thunk metatable

void lj_thunk_init(lua_State *L)
{
   // Create metatable
   lua_newtable(L);
   int mt_idx = lua_gettop(L);

   // Register metamethods
   lua_pushcfunction(L, thunk_add);
   lua_setfield(L, mt_idx, "__add");

   lua_pushcfunction(L, thunk_sub);
   lua_setfield(L, mt_idx, "__sub");

   lua_pushcfunction(L, thunk_mul);
   lua_setfield(L, mt_idx, "__mul");

   lua_pushcfunction(L, thunk_div);
   lua_setfield(L, mt_idx, "__div");

   lua_pushcfunction(L, thunk_mod);
   lua_setfield(L, mt_idx, "__mod");

   lua_pushcfunction(L, thunk_pow);
   lua_setfield(L, mt_idx, "__pow");

   lua_pushcfunction(L, thunk_unm);
   lua_setfield(L, mt_idx, "__unm");

   lua_pushcfunction(L, thunk_concat);
   lua_setfield(L, mt_idx, "__concat");

   lua_pushcfunction(L, thunk_eq);
   lua_setfield(L, mt_idx, "__eq");

   lua_pushcfunction(L, thunk_lt);
   lua_setfield(L, mt_idx, "__lt");

   lua_pushcfunction(L, thunk_le);
   lua_setfield(L, mt_idx, "__le");

   lua_pushcfunction(L, thunk_index);
   lua_setfield(L, mt_idx, "__index");

   lua_pushcfunction(L, thunk_newindex);
   lua_setfield(L, mt_idx, "__newindex");

   lua_pushcfunction(L, thunk_len);
   lua_setfield(L, mt_idx, "__len");

   lua_pushcfunction(L, thunk_call);
   lua_setfield(L, mt_idx, "__call");

   lua_pushcfunction(L, thunk_tostring);
   lua_setfield(L, mt_idx, "__tostring");

   // Store in registry
   lua_setfield(L, LUA_REGISTRYINDEX, THUNK_METATABLE_NAME);
}
