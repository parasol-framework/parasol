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
#include "lj_vmarray.h"
#include "lj_vm.h"
#include "lj_frame.h"

//********************************************************************************************************************
// Helper to convert TValue index to integer, returns -1 if invalid

static int32_t arr_idx_from_tv(cTValue *K)
{
   if (tvisint(K)) {
      return intV(K);
   }
   else if (tvisnum(K)) {
      lua_Number n = numV(K);
      int32_t i = lj_num2int(n);
      if ((lua_Number)i IS n) return i;
   }
   return -1;  // Invalid index
}

//********************************************************************************************************************
// Helper to load array element into TValue based on element type

static void arr_load_elem(GCarray *Array, uint32_t Idx, TValue *Result)
{
   void *elem = lj_array_index(Array, Idx);

   switch (Array->elemtype) {
      case ARRAY_ELEM_BYTE:   setintV(Result, *(uint8_t*)elem); break;
      case ARRAY_ELEM_INT16:  setintV(Result, *(int16_t*)elem); break;
      case ARRAY_ELEM_INT32:  setintV(Result, *(int32_t*)elem); break;
      case ARRAY_ELEM_INT64:  setnumV(Result, lua_Number(*(int64_t*)elem)); break;
      case ARRAY_ELEM_FLOAT:  setnumV(Result, *(float*)elem); break;
      case ARRAY_ELEM_DOUBLE: setnumV(Result, *(double*)elem); break;
      case ARRAY_ELEM_PTR:
         // Store raw pointer value as light userdata
         setrawlightudV(Result, *(void**)elem);
         break;
      case ARRAY_ELEM_STRING: {
         GCRef ref = *(GCRef*)elem;
         if (gcref(ref)) setstrV(nullptr, Result, gco2str(gcref(ref)));
         else setnilV(Result);
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

   // Get numeric value from TValue

   lua_Number num = 0;
   if (tvisint(Val)) num = lua_Number(intV(Val));
   else if (tvisnum(Val)) num = numV(Val);
   else if (Array->elemtype IS ARRAY_ELEM_STRING and tvisstr(Val)) {
      // String storage
      GCstr *str = strV(Val);
      setgcref(*(GCRef*)elem, obj2gco(str));
      // Use forward barrier for GC objects
      lj_gc_objbarrier(L, Array, str);
      return;
   }
   else if (Array->elemtype IS ARRAY_ELEM_PTR and tvislightud(Val)) {
      // Extract raw pointer (note: lightudV on 64-bit requires global_State)
      *(void**)elem = (void*)(Val->u64 & LJ_GCVMASK);
      return;
   }
   else { // Type mismatch - attempt numeric conversion or error
      if (tvisnil(Val)) num = 0;
      else lj_err_caller(L, ErrMsg::ARRTYPE);
   }

   switch (Array->elemtype) {
      case ARRAY_ELEM_BYTE:   *(uint8_t*)elem = uint8_t(num); break;
      case ARRAY_ELEM_INT16:  *(int16_t*)elem = int16_t(num); break;
      case ARRAY_ELEM_INT32:  *(int32_t*)elem = int32_t(num); break;
      case ARRAY_ELEM_INT64:  *(int64_t*)elem = int64_t(num); break;
      case ARRAY_ELEM_FLOAT:  *(float*)elem = float(num); break;
      case ARRAY_ELEM_DOUBLE: *(double*)elem = num; break;
      default: lj_err_caller(L, ErrMsg::ARRTYPE); break;
   }
}

//********************************************************************************************************************
// Helper for AGETV/AGETB. Array get with metamethod support.
// Returns pointer to result TValue, or nullptr to trigger metamethod call.

extern "C" cTValue* lj_arr_get(lua_State *L, cTValue *O, cTValue *K)
{
   if (not tvisarray(O)) {
      // Not an array - check for __index metamethod
      cTValue *mo = lj_meta_lookup(L, O, MM_index);
      if (tvisnil(mo)) {
         lj_err_optype(L, O, ErrMsg::OPINDEX);
         return nullptr;  // unreachable
      }
      // Would need to trigger metamethod - for now, error
      lj_err_optype(L, O, ErrMsg::OPINDEX);
      return nullptr;
   }

   GCarray *arr = arrayV(O);

   // Convert index to integer (0-based internally)

   int32_t idx = arr_idx_from_tv(K);
   if (idx < 0 or MSize(idx) >= arr->len) {
      // Check for __index metamethod on array's metatable
      if (GCtab *mt = tabref(arr->metatable)) {
         if (cTValue *mo = lj_meta_fast(L, mt, MM_index)) {
            // Metamethod exists - return nullptr to trigger it
            // The assembler VM will handle calling the metamethod
            return nullptr;
         }
      }

      // No metamethod - raise error
      lj_err_callerv(L, ErrMsg::ARROB, idx, int(arr->len));
      return nullptr;  // unreachable
   }

   // Load element into a static result TValue
   // Note: This uses a thread-local or static buffer that the VM will copy
   static thread_local TValue result;
   arr_load_elem(arr, uint32_t(idx), &result);
   return &result;
}

//********************************************************************************************************************
// Helper for ASETV/ASETB. Array set with metamethod support.
// Returns pointer to store location, or nullptr to trigger metamethod call.

extern "C" TValue * lj_arr_set(lua_State *L, cTValue *O, cTValue *K)
{
   if (not tvisarray(O)) {
      // Not an array - check for __newindex metamethod
      cTValue *mo = lj_meta_lookup(L, O, MM_newindex);
      if (tvisnil(mo)) {
         lj_err_optype(L, O, ErrMsg::OPINDEX);
         return nullptr;  // unreachable
      }
      // Would need to trigger metamethod - for now, error
      lj_err_optype(L, O, ErrMsg::OPINDEX);
      return nullptr;
   }

   GCarray *arr = arrayV(O);

   // Check read-only flag
   if (arr->flags & ARRAY_FLAG_READONLY) {
      lj_err_caller(L, ErrMsg::ARRRO);
      return nullptr;  // unreachable
   }

   // Convert index to integer (0-based internally)
   int32_t idx = arr_idx_from_tv(K);
   if (idx < 0 or MSize(idx) >= arr->len) {
      // Check for __newindex metamethod on array's metatable
      GCtab *mt = tabref(arr->metatable);
      if (mt) {
         cTValue *mo = lj_meta_fast(L, mt, MM_newindex);
         if (mo) { // Metamethod exists - return nullptr to trigger it
            return nullptr;
         }
      }
      // No metamethod - raise error
      lj_err_callerv(L, ErrMsg::ARROB, idx, int(arr->len));
      return nullptr;  // unreachable
   }

   // Return a dummy TValue pointer - the actual store is handled separately
   // The caller will use lj_arr_setidx after getting this non-null return
   static thread_local TValue dummy;
   return &dummy;
}

//********************************************************************************************************************
// Direct array get by index - called after type and bounds checks pass

extern "C" void lj_arr_getidx(lua_State *L, GCarray *Array, int32_t Idx, TValue *Result)
{
   if (Idx < 0 or MSize(Idx) >= Array->len) lj_err_callerv(L, ErrMsg::ARROB, Idx, int(Array->len));
   arr_load_elem(Array, uint32_t(Idx), Result);
}

//********************************************************************************************************************
// Direct array set by index - called after type and bounds checks pass

extern "C" void lj_arr_setidx(lua_State *L, GCarray *Array, int32_t Idx, cTValue *Val)
{
   if (Idx < 0 or MSize(Idx) >= Array->len) lj_err_callerv(L, ErrMsg::ARROB, Idx, int(Array->len));
   if (Array->flags & ARRAY_FLAG_READONLY) lj_err_caller(L, ErrMsg::ARRRO);
   arr_store_elem(L, Array, uint32_t(Idx), Val);
}
