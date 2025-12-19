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
#include "../../struct_def.h"

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
   sizeof(GCRef),        // AET::_TABLE
   0                     // AET::_STRUCT (variable)
};

//********************************************************************************************************************

uint8_t lj_array_elemsize(AET Type)
{
   lj_assertX(int(Type) >= 0 and int(Type) < int(AET::_MAX), "invalid array element type");
   return glElemSizes[int(Type)];
}

//********************************************************************************************************************
// Create a new array structure without placing it on the Lua stack (use lua_createarray otherwise).  Throws on error.
//
// For string arrays (CSTRING/STRING_CPP) with caching:
// - Data points to an array of CSTRING or std::string pointers
// - String content is copied into a vector<char> owned by the array
// - The storage area stores CSTRING pointers into the vector

extern GCarray * lj_array_new(lua_State *L, uint32_t Length, AET Type, void *Data, uint8_t Flags, std::string_view StructName)
{
   MSize elem_size;
   struct_record *sdef = nullptr;

   if (not StructName.empty()) {
      // Struct-backed array
      auto prv = (prvFluid *)L->script->ChildPrivate;
      auto name = struct_name(StructName);
      if (prv->Structs.contains(name)) {
         sdef = &prv->Structs[name];
         elem_size = sdef->Size;
      }
      else {
         lj_err_callerv(L, ErrMsg::NOSTRUCT, "%.*s", StructName.size(), StructName.data());
         return nullptr;
      }
   }
   else elem_size = lj_array_elemsize(Type);

   lj_assertL(elem_size > 0, "invalid element size for array creation");

   if (Data) {
      if (Flags & ARRAY_EXTERNAL) {
         // External data - caller manages lifetime (no storage allocation needed)
         auto arr = (GCarray *)lj_mem_newgco(L, sizeof(GCarray));
         arr->init(Data, Type, elem_size, Length, Flags, sdef);
         return arr;
      }
      else {
         // Cached data - copy into owned storage
         if (Type IS AET::_CSTRING or Type IS AET::_STRING_CPP) {
            // String caching: store CSTRING pointers that point into strcache
            size_t byte_size = Length * sizeof(CSTRING);
            void *storage = lj_mem_new(L, byte_size);
            auto arr = (GCarray *)lj_mem_newgco(L, sizeof(GCarray));
            arr->init(storage, AET::_CSTRING, sizeof(CSTRING), Length, 0, sdef);

            // Calculate total string content size
            size_t content_size = 0;
            if (Type IS AET::_CSTRING) {
               auto strings = (CSTRING *)Data;
               for (uint32_t i = 0; i < Length; i++) {
                  if (strings[i]) content_size += strlen(strings[i]) + 1;
                  else content_size += 1; // For null string, store empty string
               }
            }
            else { // AET::_STRING_CPP
               auto strings = (std::string *)Data;
               for (uint32_t i = 0; i < Length; i++) {
                  content_size += strings[i].size() + 1;
               }
            }

            // Allocate and populate the string cache
            arr->strcache = new std::vector<char>(content_size);
            auto cache_ptr = arr->strcache->data();
            auto ptr_array = (CSTRING *)arr->arraydata();

            if (Type IS AET::_CSTRING) {
               auto strings = (CSTRING *)Data;
               for (uint32_t i = 0; i < Length; i++) {
                  ptr_array[i] = cache_ptr;
                  if (strings[i]) {
                     size_t slen = strlen(strings[i]);
                     std::memcpy(cache_ptr, strings[i], slen + 1);
                     cache_ptr += slen + 1;
                  }
                  else {
                     *cache_ptr++ = '\0';
                  }
               }
            }
            else { // AET::_STRING_CPP
               auto strings = (std::string *)Data;
               for (uint32_t i = 0; i < Length; i++) {
                  ptr_array[i] = cache_ptr;
                  std::memcpy(cache_ptr, strings[i].c_str(), strings[i].size() + 1);
                  cache_ptr += strings[i].size() + 1;
               }
            }

            return arr;
         }
         else if (Type IS AET::_TABLE) {
            // Table arrays not supported for caching (not used by the Parasol API)
            lj_err_callerv(L, ErrMsg::BADVAL);
            return nullptr;
         }
         else {
            // Non-string cached array - allocate storage via GC, then copy data
            size_t byte_size = Length * elem_size;
            void *storage = lj_mem_new(L, byte_size);
            auto arr = (GCarray *)lj_mem_newgco(L, sizeof(GCarray));
            arr->init(storage, Type, elem_size, Length, 0, sdef);
            std::memcpy(storage, Data, byte_size);
            return arr;
         }
      }
   }
   else {
      // New empty array with owned storage allocated via GC
      size_t byte_size = Length * elem_size;
      void *storage = (byte_size > 0) ? lj_mem_new(L, byte_size) : nullptr;
      auto arr = (GCarray *)lj_mem_newgco(L, sizeof(GCarray));
      arr->init(storage, Type, elem_size, Length, 0, sdef);
      if (storage and int(Type) >= int(AET::_VULNERABLE)) arr->clear();
      return arr;
   }
}

//********************************************************************************************************************

void LJ_FASTCALL lj_array_free(global_State *g, GCarray *Array)
{
   // Free owned storage first (external storage is managed by caller)
   size_t storage_size = Array->storage_size();
   if (storage_size > 0) {
      lj_mem_free(g, Array->storage, storage_size);
   }
   Array->~GCarray(); // Call destructor (handles strcache cleanup)
   lj_mem_free(g, Array, sizeof(GCarray));
}

//********************************************************************************************************************

void * lj_array_index_checked(lua_State *L, GCarray *Array, uint32_t Idx)
{
   if (Idx >= Array->len) lj_err_callerv(L, ErrMsg::ARROB, int(Idx), int(Array->len));
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

   auto data = (uint8_t *)Array->arraydata();
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
         case AET::_TABLE: {
            GCRef ref = *(GCRef*)elem;
            if (gcref(ref)) settabV(L, slot, gco2tab(gcref(ref)));
            else setnilV(slot);
            break;
         }
         default: setnilV(slot); break;
      }
   }

   return t;
}
