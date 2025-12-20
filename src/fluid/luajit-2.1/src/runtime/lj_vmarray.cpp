// Array helper functions for assembler VM.
// Copyright (C) 2025 Paul Manias

#define lj_vmarray_c
#define LUA_CORE

#include "lj_obj.h"
#include "lj_gc.h"
#include "lj_err.h"
#include "lj_tab.h"
#include "lj_meta.h"
#include "lj_array.h"
#include "lj_str.h"
#include "lj_vmarray.h"
#include "lj_vm.h"
#include "lj_frame.h"

#include <string>

#ifndef CSTRING
#define CSTRING const char *
#endif

//********************************************************************************************************************
// Helper to convert TValue index to integer, returns -1 if invalid

static int32_t arr_idx_from_tv(cTValue *K)
{
   if (tvisint(K)) return intV(K);
   else if (tvisnum(K)) {
      lua_Number n = numV(K);
      int32_t i = lj_num2int(n);
      if ((lua_Number)i IS n) return i;
   }
   return -1;  // Invalid index
}

//********************************************************************************************************************
// Helper to retrieve array element into TValue based on element type

static void arr_load_elem(lua_State *L, GCarray *Array, uint32_t Idx, TValue *Result)
{
   void *elem = lj_array_index(Array, Idx);

   switch (Array->elemtype) {
      case AET::_BYTE:   setintV(Result, *(uint8_t*)elem); break;
      case AET::_INT16:  setintV(Result, *(int16_t*)elem); break;
      case AET::_INT32:  setintV(Result, *(int32_t*)elem); break;
      case AET::_INT64:  setnumV(Result, lua_Number(*(int64_t*)elem)); break;
      case AET::_FLOAT:  setnumV(Result, *(float*)elem); break;
      case AET::_DOUBLE: setnumV(Result, *(double*)elem); break;

      case AET::_CSTRING: {
         if (auto str = *(CSTRING *)elem) setstrV(L, Result, lj_str_newz(L, str));
         else setnilV(Result);
         break;
      }

      case AET::_STRING_CPP: {
         auto str = (std::string*)elem;
         if (str->empty()) setnilV(Result);
         else setstrV(L, Result, lj_str_new(L, str->data(), str->size()));
         break;
      }

      case AET::_PTR:
         // Store raw pointer value as light userdata
         setrawlightudV(Result, *(void**)elem);
         break;

      case AET::_STRING_GC: {
         GCRef ref = *(GCRef*)elem;
         if (gcref(ref)) setstrV(L, Result, gco2str(gcref(ref)));
         else setnilV(Result);
         break;
      }

      case AET::_TABLE: {
         GCRef ref = *(GCRef*)elem;
         if (gcref(ref)) settabV(L, Result, gco2tab(gcref(ref)));
         else setnilV(Result);
         break;
      }

      case AET::_ANY: {
         TValue *source = (TValue*)elem;
         copyTV(L, Result, source);
         break;
      }

      default: setnilV(Result); break;
   }
}

//********************************************************************************************************************
// Helper to store TValue into array element based on element type

static void arr_store_elem(lua_State *L, GCarray *Array, uint32_t Idx, cTValue *Val)
{
   void *elem = lj_array_index(Array, Idx);

   // Handle non-numeric types first (string, table) - these don't accept numeric values

   if (Array->elemtype IS AET::_STRING_GC) {
      if (tvisstr(Val)) {
         GCstr *str = strV(Val);
         setgcref(*(GCRef*)elem, obj2gco(str));
         lj_gc_objbarrier(L, Array, str);
      }
      else if (tvisnil(Val)) setgcrefnull(*(GCRef*)elem);
      else lj_err_msgv(L, ErrMsg::ARRTYPE);
      return;
   }
   else if (Array->elemtype IS AET::_TABLE) {
      if (tvistab(Val)) {
         GCtab *tab = tabV(Val);
         setgcref(*(GCRef*)elem, obj2gco(tab));
         lj_gc_objbarrier(L, Array, tab);
      }
      else if (tvisnil(Val)) setgcrefnull(*(GCRef*)elem);
      else lj_err_msgv(L, ErrMsg::ARRTYPE);
      return;
   }
   else if (Array->elemtype IS AET::_ANY) {
      TValue *dest = (TValue*)elem;
      copyTV(L, dest, Val);
      // Apply write barrier for any GC value
      if (tvisgcv(Val)) lj_gc_objbarrier(L, Array, gcV(Val));
      return;
   }

   // Get numeric value from TValue for primitive types

   lua_Number num = 0;
   if (tvisint(Val)) num = lua_Number(intV(Val));
   else if (tvisnum(Val)) num = numV(Val);
   else if (Array->elemtype IS AET::_PTR and tvislightud(Val)) [[unlikely]] {
      // Extract raw pointer (note: lightudV on 64-bit requires global_State)
      *(void**)elem = (void*)(Val->u64 & LJ_GCVMASK);
      return;
   }
   else if ((Array->elemtype IS AET::_CSTRING) or (Array->elemtype IS AET::_STRING_CPP)) [[unlikely]] {
      // Storing pointers to Lua strings is somewhat feasible but unsafe; for this reason we disallow it.
      lj_err_msgv(L, ErrMsg::ARRTYPE);
      return;
   }
   else { // Type mismatch - attempt numeric conversion or error
      if (tvisnil(Val)) num = 0;
      else lj_err_msgv(L, ErrMsg::ARRTYPE);
   }

   // Primitive types

   switch (Array->elemtype) {
      case AET::_BYTE:   *(uint8_t*)elem = uint8_t(num); break;
      case AET::_INT16:  *(int16_t*)elem = int16_t(num); break;
      case AET::_INT32:  *(int32_t*)elem = int32_t(num); break;
      case AET::_INT64:  *(int64_t*)elem = int64_t(num); break;
      case AET::_FLOAT:  *(float*)elem = float(num); break;
      case AET::_DOUBLE: *(double*)elem = num; break;
      default: lj_err_msgv(L, ErrMsg::ARRTYPE); break;
   }
}

//********************************************************************************************************************
// Helper for AGETV/AGETB. Array get with metamethod support.
// Returns pointer to result TValue, or nullptr to trigger metamethod call.

extern "C" cTValue * lj_arr_get(lua_State *L, cTValue *O, cTValue *K)
{
   if (not tvisarray(O)) { // Not an array - check for __index metamethod
      cTValue *mo = lj_meta_lookup(L, O, MM_index);
      if (tvisnil(mo)) {
         lj_err_optype(L, O, ErrMsg::OPINDEX);
         return nullptr;  // unreachable
      }

      // Would need to trigger metamethod (none implemented yet) - for now, error
      lj_err_optype(L, O, ErrMsg::OPINDEX);
      return nullptr;
   }

   GCarray *arr = arrayV(O);

   // Check if key is a string (method lookup like arr:concat())

   if (tvisstr(K)) {
      // Look up directly in array's metatable for methods (per-instance first, then base)
      GCtab *mt = tabref(arr->metatable);
      if (not mt) mt = tabref(basemt_it(G(L), LJ_TARRAY));

      if (mt) {
         cTValue *tv = lj_tab_get(L, mt, K);
         if (not tvisnil(tv)) return tv;  // Found method in metatable
      }
      // String key not recognised as a method - raise error
      lj_err_optype(L, O, ErrMsg::BADKEY);
      return nullptr;
   }

   // Convert index to integer (0-based internally)

   int32_t idx = arr_idx_from_tv(K);
   if ((idx < 0) or (idx >= int32_t(arr->len))) {
      // Check for __index metamethod on array's metatable (per-instance first, then base)
      GCtab *mt = tabref(arr->metatable);
      if (not mt) mt = tabref(basemt_it(G(L), LJ_TARRAY));

      if (mt and lj_meta_fast(L, mt, MM_index)) {
         // Metamethod exists - return nullptr to trigger it
         // The assembler VM will handle calling the metamethod
         return nullptr;
      }

      // No metamethod - raise error
      lj_err_msgv(L, ErrMsg::ARROB, idx, int(arr->len));
      return nullptr;  // unreachable
   }

   // Load element into a static result TValue
   // Note: This uses a thread-local or static buffer that the assembly caller is expected to copy

   static thread_local TValue result;
   arr_load_elem(L, arr, uint32_t(idx), &result);
   return &result;
}

//********************************************************************************************************************
// Helper for ASETV/ASETB. Array set with metamethod support.
// Performs the actual store. Returns 1 on success, 0 to trigger metamethod call.

extern "C" int lj_arr_set(lua_State *L, cTValue *O, cTValue *K, cTValue *V)
{
   if (not tvisarray(O)) {
      // Not an array - check for __newindex metamethod
      cTValue *mo = lj_meta_lookup(L, O, MM_newindex);
      if (tvisnil(mo)) {
         lj_err_optype(L, O, ErrMsg::OPINDEX);
         return 0;  // unreachable
      }
      // Would need to trigger metamethod - for now, error
      lj_err_optype(L, O, ErrMsg::OPINDEX);
      return 0;
   }

   GCarray *arr = arrayV(O);

   if (arr->flags & ARRAY_READONLY) {
      lj_err_msg(L, ErrMsg::ARRRO);
      return 0;  // unreachable
   }

   // Convert index to integer (0-based internally)
   int32_t idx = arr_idx_from_tv(K);
   if (idx < 0 or MSize(idx) >= arr->len) {
      // Check for __newindex metamethod on array's metatable (per-instance first, then base)
      GCtab *mt = tabref(arr->metatable);
      if (not mt) mt = tabref(basemt_it(G(L), LJ_TARRAY));

      if (mt) {
         cTValue *mo = lj_meta_fast(L, mt, MM_newindex);
         if (mo) return 0; // Metamethod exists - return 0 to trigger it
      }
      // No metamethod - raise error
      lj_err_msgv(L, ErrMsg::ARROB, idx, int(arr->len));
      return 0;  // unreachable
   }

   // Perform the actual store
   arr_store_elem(L, arr, uint32_t(idx), V);
   return 1;  // Success
}

//********************************************************************************************************************
// Direct array get by index - called after type and bounds checks pass

extern "C" void lj_arr_getidx(lua_State *L, GCarray *Array, int32_t Idx, TValue *Result)
{
   if (Idx < 0 or MSize(Idx) >= Array->len) lj_err_msgv(L, ErrMsg::ARROB, Idx, int(Array->len));
   arr_load_elem(L, Array, uint32_t(Idx), Result);
}

//********************************************************************************************************************
// Direct array set by index - called after type and bounds checks pass

extern "C" void lj_arr_setidx(lua_State *L, GCarray *Array, int32_t Idx, cTValue *Val)
{
   if (Idx < 0 or MSize(Idx) >= Array->len) lj_err_msgv(L, ErrMsg::ARROB, Idx, int(Array->len));
   if (Array->flags & ARRAY_READONLY) lj_err_msg(L, ErrMsg::ARRRO);
   arr_store_elem(L, Array, uint32_t(Idx), Val);
}
