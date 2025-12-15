// Native array library.
// Copyright (C) 2025 Paul Manias.

#define lib_array_c
#define LUA_LIB

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

#include "lj_obj.h"
#include "lj_gc.h"
#include "lj_err.h"
#include "lj_tab.h"
#include "lj_str.h"
#include "lj_array.h"
#include "lib.h"

#include <cstdio>
#include <cstring>
#include <parasol/strings.hpp>

#define LJLIB_MODULE_fastarray

constexpr auto HASH_INT     = pf::strhash("int");
constexpr auto HASH_BYTE    = pf::strhash("char");
constexpr auto HASH_INT16   = pf::strhash("int16");
constexpr auto HASH_INT64   = pf::strhash("int64");
constexpr auto HASH_FLOAT   = pf::strhash("float");
constexpr auto HASH_DOUBLE  = pf::strhash("double");
constexpr auto HASH_STRING  = pf::strhash("string");
constexpr auto HASH_STRUCT  = pf::strhash("struct");
constexpr auto HASH_POINTER = pf::strhash("pointer");

//********************************************************************************************************************
// Helper function to check argument is an array

static GCarray * lib_checkarray(lua_State *L, int NArg)
{
   TValue *o = L->base + NArg - 1;
   if (o >= L->top or not tvisarray(o)) {
      lj_err_arg(L, NArg, ErrMsg::ARRARG);
   }
   return arrayV(o);
}

//********************************************************************************************************************
// Helper function to check optional array argument (may be nil)

static GCarray * lib_optarray(lua_State *L, int NArg)
{
   TValue *o = L->base + NArg - 1;
   if (o >= L->top or tvisnil(o)) return nullptr;
   if (not tvisarray(o)) lj_err_arg(L, NArg, ErrMsg::ARRARG);
   return arrayV(o);
}

//********************************************************************************************************************
// Helper to parse element type string

static uint8_t parse_elemtype(lua_State *L, int NArg)
{
   GCstr *type_str = lj_lib_checkstr(L, NArg);

   switch (type_str->hash) {
      case HASH_INT:     return ARRAY_ELEM_INT32;
      case HASH_BYTE:    return ARRAY_ELEM_BYTE;
      case HASH_INT16:   return ARRAY_ELEM_INT16;
      case HASH_INT64:   return ARRAY_ELEM_INT64;
      case HASH_FLOAT:   return ARRAY_ELEM_FLOAT;
      case HASH_DOUBLE:  return ARRAY_ELEM_DOUBLE;
      case HASH_STRING:  return ARRAY_ELEM_STRING;
      case HASH_STRUCT:  return ARRAY_ELEM_STRUCT;
      case HASH_POINTER: return ARRAY_ELEM_PTR;
   }

   lj_err_argv(L, NArg, ErrMsg::BADTYPE, "invalid array type", strdata(type_str));
   return 0;  // unreachable
}

//********************************************************************************************************************
// Helper to get element type name

static const char* elemtype_name(uint8_t Type)
{
   switch (Type) {
      case ARRAY_ELEM_BYTE:   return "char";
      case ARRAY_ELEM_INT16:  return "int16";
      case ARRAY_ELEM_INT32:  return "int";
      case ARRAY_ELEM_INT64:  return "int64";
      case ARRAY_ELEM_FLOAT:  return "float";
      case ARRAY_ELEM_DOUBLE: return "double";
      case ARRAY_ELEM_PTR:    return "pointer";
      case ARRAY_ELEM_STRING: return "string";
      case ARRAY_ELEM_STRUCT: return "struct";
      default: return "unknown";
   }
}

//********************************************************************************************************************
// fastarray.new(size, type)
// Creates a new array of the specified size and element type.
//
// Parameters:
//   size: number of elements (must be non-negative)
//   type: element type string ("char", "int16", "int", "int64", "float", "double", "pointer", "string", "struct")
//
// Returns: new array
//
// Example: local arr = fastarray.new(100, "int")

LJLIB_CF(fastarray_new)
{
   int32_t size = lj_lib_checkint(L, 1);
   if (size < 0) lj_err_argv(L, 1, ErrMsg::NUMRNG, "non-negative", "negative");

   uint8_t elemtype = parse_elemtype(L, 2);
   GCarray *arr = lj_array_new(L, uint32_t(size), elemtype);

   // Set metatable from registry

   lua_getfield(L, LUA_REGISTRYINDEX, "fastarray_metatable");
   if (tvistab(L->top - 1)) {
      GCtab* mt = tabV(L->top - 1);
      setgcref(arr->metatable, obj2gco(mt));
      lj_gc_objbarrier(L, arr, mt);
   }
   L->top--;  // Pop metatable

   setarrayV(L, L->top++, arr);
   return 1;
}

//********************************************************************************************************************
// fastarray.table(arr)
// Converts an array to a Lua table.
//
// Parameters:
//   arr: the array to convert
//
// Returns: new table with array contents (0-based indexing)

LJLIB_CF(fastarray_table)
{
   GCarray *arr = lib_checkarray(L, 1);
   GCtab *t = lj_array_to_table(L, arr);
   settabV(L, L->top++, t);
   return 1;
}

//********************************************************************************************************************
// fastarray.copy(dst, src [, dstidx [, srcidx [, count]]])
// Copies elements from source array to destination array.
//
// Parameters:
//   dst: destination array
//   src: source array
//   dstidx: starting index in destination (0-based, default 0)
//   srcidx: starting index in source (0-based, default 0)
//   count: number of elements to copy (default: all remaining elements in source)
//
// Both arrays must have the same element type.
// The destination array must not be read-only.

LJLIB_CF(fastarray_copy)
{
   GCarray *dst = lib_checkarray(L, 1);
   GCarray *src = lib_checkarray(L, 2);

   uint32_t dstidx = uint32_t(lj_lib_optint(L, 3, 0));
   uint32_t srcidx = uint32_t(lj_lib_optint(L, 4, 0));
   uint32_t count  = uint32_t(lj_lib_optint(L, 5, int32_t(src->len - srcidx)));

   lj_array_copy(L, dst, dstidx, src, srcidx, count);
   return 0;
}

//********************************************************************************************************************
// fastarray.getstring(arr [, start [, len]])
// Extracts a string from a byte array.
//
// Parameters:
//   arr: byte array
//   start: starting index (0-based, default 0)
//   len: number of bytes to extract (default: remaining bytes from start)
//
// Returns: string containing the bytes

LJLIB_CF(fastarray_getstring)
{
   GCarray *arr = lib_checkarray(L, 1);

   if (arr->elemtype != ARRAY_ELEM_BYTE) {
      lj_err_caller(L, ErrMsg::ARRSTR);
   }

   uint32_t start = uint32_t(lj_lib_optint(L, 2, 0));
   uint32_t len = uint32_t(lj_lib_optint(L, 3, int32_t(arr->len - start)));

   if (start + len > arr->len) {
      lj_err_caller(L, ErrMsg::IDXRNG);
   }

   const char* data = (const char*)mref(arr->data, void) + start;
   GCstr* s = lj_str_new(L, data, len);
   setstrV(L, L->top++, s);
   return 1;
}

//********************************************************************************************************************
// fastarray.setstring(arr, str [, start])
// Copies string bytes into a byte array.
//
// Parameters:
//   arr: byte array (must not be read-only)
//   str: string to copy
//   start: starting index in array (0-based, default 0)
//
// Returns: number of bytes written

LJLIB_CF(fastarray_setstring)
{
   GCarray *arr = lib_checkarray(L, 1);
   GCstr *str = lj_lib_checkstr(L, 2);

   if (arr->elemtype != ARRAY_ELEM_BYTE) lj_err_caller(L, ErrMsg::ARRSTR);
   if (arr->flags & ARRAY_FLAG_READONLY) lj_err_caller(L, ErrMsg::ARRRO);

   uint32_t start = uint32_t(lj_lib_optint(L, 3, 0));
   uint32_t len = str->len;

   // Clamp length to fit in array

   if (start >= arr->len) {
      setintV(L->top++, 0);
      return 1;
   }

   if (start + len > arr->len) len = arr->len - start;

   auto data = (char *)mref(arr->data, void) + start;
   memcpy(data, strdata(str), len);

   setintV(L->top++, int32_t(len));
   return 1;
}

//********************************************************************************************************************
// Returns the length of an array.

LJLIB_CF(fastarray_len)
{
   GCarray* arr = lib_checkarray(L, 1);
   setintV(L->top++, int32_t(arr->len));
   return 1;
}

//********************************************************************************************************************
// Returns the element type of an array as a string.
//
// Returns: element type string ("char", "int16", "integer", etc.)

LJLIB_CF(fastarray_type)
{
   GCarray* arr = lib_checkarray(L, 1);
   const char* name = elemtype_name(arr->elemtype);
   GCstr* s = lj_str_newz(L, name);
   setstrV(L, L->top++, s);
   return 1;
}

//********************************************************************************************************************
// fastarray.readonly(arr)
// Returns whether the array is read-only.
//
// Parameters:
//   arr: the array
//
// Returns: true if read-only, false otherwise

LJLIB_CF(fastarray_readonly)
{
   GCarray* arr = lib_checkarray(L, 1);
   setboolV(L->top++, (arr->flags & ARRAY_FLAG_READONLY) != 0);
   return 1;
}

//********************************************************************************************************************
// fastarray.fill(arr, value [, start [, count]])
// Fills array elements with a value.
//
// Parameters:
//   arr: the array (must not be read-only)
//   value: value to fill with (number)
//   start: starting index (0-based, default 0)
//   count: number of elements to fill (default: all remaining)

LJLIB_CF(fastarray_fill)
{
   GCarray* arr = lib_checkarray(L, 1);
   lua_Number value = lj_lib_checknum(L, 2);

   if (arr->flags & ARRAY_FLAG_READONLY) {
      lj_err_caller(L, ErrMsg::ARRRO);
   }

   uint32_t start = uint32_t(lj_lib_optint(L, 3, 0));
   uint32_t count = uint32_t(lj_lib_optint(L, 4, int32_t(arr->len - start)));

   if (start >= arr->len) return 0;
   if (start + count > arr->len) count = arr->len - start;

   uint8_t* base = (uint8_t*)mref(arr->data, void);

   for (uint32_t i = 0; i < count; i++) {
      void* elem = base + (start + i) * arr->elemsize;
      switch (arr->elemtype) {
         case ARRAY_ELEM_BYTE:   *(uint8_t*)elem = uint8_t(value); break;
         case ARRAY_ELEM_INT16:  *(int16_t*)elem = int16_t(value); break;
         case ARRAY_ELEM_INT32:  *(int32_t*)elem = int32_t(value); break;
         case ARRAY_ELEM_INT64:  *(int64_t*)elem = int64_t(value); break;
         case ARRAY_ELEM_FLOAT:  *(float*)elem = float(value); break;
         case ARRAY_ELEM_DOUBLE: *(double*)elem = value; break;
         default: break;
      }
   }

   return 0;
}

//********************************************************************************************************************
// Metamethod: __index
// Handles array[idx] access.

LJLIB_CF(fastarray___index)
{
   GCarray* arr = lib_checkarray(L, 1);
   int32_t idx = lj_lib_checkint(L, 2);

   if (idx < 0 or MSize(idx) >= arr->len) {
      lj_err_callerv(L, ErrMsg::ARROB, idx, int(arr->len));
   }

   void* elem = lj_array_index(arr, uint32_t(idx));

   switch (arr->elemtype) {
      case ARRAY_ELEM_BYTE:   setintV(L->top, *(uint8_t*)elem); break;
      case ARRAY_ELEM_INT16:  setintV(L->top, *(int16_t*)elem); break;
      case ARRAY_ELEM_INT32:  setintV(L->top, *(int32_t*)elem); break;
      case ARRAY_ELEM_INT64:  setnumV(L->top, lua_Number(*(int64_t*)elem)); break;
      case ARRAY_ELEM_FLOAT:  setnumV(L->top, *(float*)elem); break;
      case ARRAY_ELEM_DOUBLE: setnumV(L->top, *(double*)elem); break;
      case ARRAY_ELEM_PTR:
         setrawlightudV(L->top, *(void**)elem);
         break;
      case ARRAY_ELEM_STRING: {
         GCRef ref = *(GCRef*)elem;
         if (gcref(ref)) setstrV(L, L->top, gco2str(gcref(ref)));
         else setnilV(L->top);
         break;
      }
      default: setnilV(L->top); break;
   }
   L->top++;
   return 1;
}

//********************************************************************************************************************
// Metamethod: __newindex
// Handles array[idx] = value assignment.

LJLIB_CF(fastarray___newindex)
{
   GCarray* arr = lib_checkarray(L, 1);
   int32_t idx = lj_lib_checkint(L, 2);
   TValue* val = L->base + 2;

   if (arr->flags & ARRAY_FLAG_READONLY) {
      lj_err_caller(L, ErrMsg::ARRRO);
   }
   if (idx < 0 or MSize(idx) >= arr->len) {
      lj_err_callerv(L, ErrMsg::ARROB, idx, int(arr->len));
   }

   void* elem = lj_array_index(arr, uint32_t(idx));

   // Get numeric value from TValue
   lua_Number num = 0;
   if (tvisint(val)) num = lua_Number(intV(val));
   else if (tvisnum(val)) num = numV(val);
   else if (arr->elemtype IS ARRAY_ELEM_STRING and tvisstr(val)) {
      GCstr* str = strV(val);
      setgcref(*(GCRef*)elem, obj2gco(str));
      lj_gc_objbarrier(L, arr, str);
      return 0;
   }
   else if (arr->elemtype IS ARRAY_ELEM_PTR and tvislightud(val)) {
      *(void**)elem = (void*)(val->u64 & LJ_GCVMASK);
      return 0;
   }
   else if (tvisnil(val)) num = 0;
   else lj_err_caller(L, ErrMsg::ARRTYPE);

   switch (arr->elemtype) {
      case ARRAY_ELEM_BYTE:   *(uint8_t*)elem = uint8_t(num); break;
      case ARRAY_ELEM_INT16:  *(int16_t*)elem = int16_t(num); break;
      case ARRAY_ELEM_INT32:  *(int32_t*)elem = int32_t(num); break;
      case ARRAY_ELEM_INT64:  *(int64_t*)elem = int64_t(num); break;
      case ARRAY_ELEM_FLOAT:  *(float*)elem = float(num); break;
      case ARRAY_ELEM_DOUBLE: *(double*)elem = num; break;
      default: lj_err_caller(L, ErrMsg::ARRTYPE); break;
   }

   return 0;
}

//********************************************************************************************************************
// Metamethod: __len
// Returns array length for # operator.

LJLIB_CF(fastarray___len)
{
   GCarray *arr = lib_checkarray(L, 1);
   setintV(L->top++, int32_t(arr->len));
   return 1;
}

//********************************************************************************************************************
// Metamethod: __tostring
// Returns string representation of array.

LJLIB_CF(fastarray___tostring)
{
   GCarray* arr = lib_checkarray(L, 1);
   const char* type_name = elemtype_name(arr->elemtype);

   // Format: array(SIZE, "TYPE")
   char buf[128];
   snprintf(buf, sizeof(buf), "array(%u, \"%s\")", uint32_t(arr->len), type_name);

   GCstr* s = lj_str_newz(L, buf);
   setstrV(L, L->top++, s);
   return 1;
}

//********************************************************************************************************************

#include "lj_libdef.h"

//********************************************************************************************************************
// Library opener function
// Registers the fastarray library and sets up the array metatable.

extern "C" int luaopen_fastarray(lua_State *L)
{
   LJ_LIB_REG(L, "fastarray", fastarray);

   // Create and register array metatable using Lua stack operations
   lua_createtable(L, 0, 4);  // metatable

   // Set __index from library function
   lua_getfield(L, -2, "__index");  // Get fastarray.__index
   lua_setfield(L, -2, "__index");  // Set it in metatable

   // Set __newindex from library function
   lua_getfield(L, -2, "__newindex");
   lua_setfield(L, -2, "__newindex");

   // Set __len from library function
   lua_getfield(L, -2, "__len");
   lua_setfield(L, -2, "__len");

   // Set __tostring from library function
   lua_getfield(L, -2, "__tostring");
   lua_setfield(L, -2, "__tostring");

   // Store metatable in registry as "fastarray_metatable"
   lua_setfield(L, LUA_REGISTRYINDEX, "fastarray_metatable");

   return 1;
}
