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
#include <parasol/main.h>
#include <parasol/modules/fluid.h>
#include <parasol/strings.hpp>
#include "../../defs.h"

// Element sizes for each type (must match AET enum order)

static const uint8_t glElemSizes[] = {
   sizeof(uint8_t),      // AET::_BYTE
   sizeof(int16_t),      // AET::_INT16
   sizeof(int32_t),      // AET::_INT32
   sizeof(int64_t),      // AET::_INT64
   sizeof(float),        // AET::_FLOAT
   sizeof(double),       // AET::_DOUBLE
   sizeof(void*),        // AET::_PTR
   sizeof(const char*),  // AET::_CSTRING
   sizeof(std::string),  // AET::_STRING_CPP
   sizeof(GCRef),        // AET::_STRING_GC
   0                     // AET::_STRUCT (variable)
};

//********************************************************************************************************************

uint8_t lj_array_elemsize(AET Type)
{
   lj_assertX(int(Type) >= 0 and int(Type) < int(AET::_MAX), "invalid array element type");
   return glElemSizes[int(Type)];
}

//********************************************************************************************************************
// Create a new array

extern GCarray * lj_array_new(lua_State *L, uint32_t Length, AET Type, void *Data, uint8_t Flags, std::string_view StructName)
{
   MSize elem_size;

   if (!StructName.empty()) {
      // Struct-backed array
      auto prv = (prvFluid *)L->script->ChildPrivate;
      auto name = struct_name(StructName);
      if (prv->Structs.contains(name)) {
         auto sdef = &prv->Structs[name];
         elem_size = sdef->Size;
      }
      else {
         lj_err_callerv(L, ErrMsg::NOSTRUCT, "%.*s", StructName.size(), StructName.data());
         return nullptr;
      }
   }
   else elem_size = lj_array_elemsize(Type);

   lj_assertL(elem_size > 0 or Type IS AET::_STRUCT, "invalid element type for array creation");

   if (Data) {
      if (Flags & ARRAY_CACHED) {
         // Copy data into co-located storage
         auto total_size = sizearraycolo(Length, elem_size);
         auto mem = lj_mem_newgco(L, total_size);
         return new (mem) GCarray(Data, Type, elem_size, Length, Flags);
      }
      else {
         // External data (ARRAY_EXTERNAL is implied when Data provided without ARRAY_CACHED)
         return new (lj_mem_newgco(L, sizeof(GCarray))) GCarray(Data, Type, elem_size, Length, Flags | ARRAY_EXTERNAL);
      }
   }
   else {
      auto total_size = sizearraycolo(Length, elem_size);
      auto mem = lj_mem_newgco(L, total_size);
      auto arr = new (mem) GCarray(Type, elem_size, Length);
      return arr;
   }
}

//********************************************************************************************************************

void LJ_FASTCALL lj_array_free(global_State *g, GCarray *Array)
{
   auto size = Array->alloc_size();
   Array->~GCarray(); // Call destructor explicitly (external data is not freed - caller manages it)
   lj_mem_free(g, Array, size);
}

//********************************************************************************************************************

void * lj_array_index_checked(lua_State *L, GCarray *Array, uint32_t Idx)
{
   if (Idx >= Array->len) {
      lj_err_callerv(L, ErrMsg::ARROB, int(Idx), int(Array->len));
   }
   return lj_array_index(Array, Idx);
}

//********************************************************************************************************************

void lj_array_copy(lua_State *L, GCarray *Dest, uint32_t DstIdx, GCarray *Src, uint32_t SrcIdx, uint32_t Count)
{
   // Safety checks - unsigned types can't be negative so just check bounds
   if (SrcIdx + Count > Src->len or DstIdx + Count > Dest->len) lj_err_caller(L, ErrMsg::IDXRNG);
   if (Dest->is_readonly()) lj_err_caller(L, ErrMsg::ARRRO);
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
