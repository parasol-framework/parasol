// Native array handling.
// Copyright (C) 2025 Paul Manias

#define lj_array_c
#define LUA_CORE

#include "lj_obj.h"
#include "lj_gc.h"
#include "lj_err.h"
#include "lj_array.h"
#include "lj_tab.h"

#include <cstring>

// Element sizes for each type

static const uint8_t glElemSizes[] = {
   sizeof(uint8_t),    // AET::BYTE
   sizeof(int16_t),    // AET::INT16
   sizeof(int32_t),    // AET::INT32
   sizeof(int64_t),    // AET::INT64
   sizeof(float),      // AET::FLOAT
   sizeof(double),     // AET::DOUBLE
   sizeof(void*),      // AET::PTR
   sizeof(GCRef),      // AET::STRING
   0                   // AET::STRUCT (variable)
};

//********************************************************************************************************************

uint8_t lj_array_elemsize(AET Type)
{
   lj_assertX(int(Type) >= 0 and int(Type) < int(AET::_MAX), "invalid array element type");
   return glElemSizes[int(Type)];
}

//********************************************************************************************************************
// Create a new array with colocated storage

extern GCarray * lj_array_new(lua_State *L, uint32_t Length, AET Type)
{
   auto elem_size = lj_array_elemsize(Type);
   auto total_size = sizearraycolo(Length, elem_size);

   auto arr = (GCarray *)lj_mem_newgco(L, total_size);
   arr->gct      = ~LJ_TARRAY;
   arr->elemtype = Type;
   arr->flags    = ARRAY_COLOCATED;
   arr->len      = Length;
   arr->capacity = Length;
   arr->elemsize = elem_size;
   setgcrefnull(arr->gclist);
   setgcrefnull(arr->metatable);
   setgcrefnull(arr->structdef);

   // Data is colocated immediately after the header
   auto data = (void *)(arr + 1);
   setmref(arr->data, data);

   memset(data, 0, Length * elem_size);

   return arr;
}

//********************************************************************************************************************
// Create array backed by external memory

extern GCarray * lj_array_new_external(lua_State *L, void *Data, uint32_t Length, AET Type, uint8_t Flags)
{
   auto arr = (GCarray *)lj_mem_newgco(L, sizeof(GCarray));
   arr->gct      = ~LJ_TARRAY;
   arr->elemtype = Type;
   arr->flags    = Flags | ARRAY_EXTERNAL;
   arr->len      = Length;
   arr->capacity = Length;
   arr->elemsize = lj_array_elemsize(Type);
   setgcrefnull(arr->gclist);
   setgcrefnull(arr->metatable);
   setgcrefnull(arr->structdef);
   setmref(arr->data, Data);

   return arr;
}

//********************************************************************************************************************

void LJ_FASTCALL lj_array_free(global_State *g, GCarray *Array)
{
   MSize size;
   if (Array->flags & ARRAY_COLOCATED) {
      size = sizearraycolo(Array->capacity, Array->elemsize);
   }
   else {
      size = sizeof(GCarray);
      // Note: External data is not freed - caller manages it
   }

   lj_mem_free(g, Array, size);
}

//********************************************************************************************************************

void * lj_array_index(GCarray *Array, uint32_t Idx)
{
   uint8_t *base = Array->data.get<uint8_t>();
   return base + (Idx * Array->elemsize);
}

//********************************************************************************************************************

void * lj_array_index_checked(lua_State *L, GCarray *Array, uint32_t Idx)
{
   if (Idx >= Array->len) {
      lj_err_callerv(L, ErrMsg::ARROB, int(Idx + 1), int(Array->len));  // 1-based in error
   }
   return lj_array_index(Array, Idx);
}

//********************************************************************************************************************

void lj_array_copy(lua_State *L, GCarray *Dest, uint32_t DstIdx, GCarray *Src, uint32_t SrcIdx, uint32_t Count)
{
   // Safety checks - unsigned types can't be negative so just check bounds
   if (SrcIdx + Count > Src->len or DstIdx + Count > Dest->len) lj_err_caller(L, ErrMsg::IDXRNG);
   if (Dest->flags & ARRAY_READONLY) lj_err_caller(L, ErrMsg::ARRRO);
   if (Dest->elemtype != Src->elemtype) lj_err_caller(L, ErrMsg::ARRTYPE);

   void *dst_ptr = lj_array_index(Dest, DstIdx);
   void *src_ptr = lj_array_index(Src, SrcIdx);
   size_t byte_count = Count * Dest->elemsize;
   memmove(dst_ptr, src_ptr, byte_count); // Use memmove to handle overlapping regions
}

//********************************************************************************************************************

GCtab * lj_array_to_table(lua_State *L, GCarray *Array)
{
   GCtab *t = lj_tab_new(L, Array->len, 0);  // 0-based: indices 0..len-1
   auto array_part = tvref(t->array);

   auto data = Array->data.get<uint8_t>();
   for (MSize i = 0; i < Array->len; i++) {
      auto slot = &array_part[i];
      void *elem = data + (i * Array->elemsize);

      switch (Array->elemtype) {
         case AET::_BYTE:   setintV(slot, *(uint8_t*)elem); break;
         case AET::_INT16:  setintV(slot, *(int16_t*)elem); break;
         case AET::_INT32:  setintV(slot, *(int32_t*)elem); break;
         case AET::_INT64:  setnumV(slot, lua_Number(*(int64_t*)elem)); break;
         case AET::_FLOAT:  setnumV(slot, *(float*)elem); break;
         case AET::_DOUBLE: setnumV(slot, *(double*)elem); break;
         default: setnilV(slot); break;
      }
   }

   return t;
}
