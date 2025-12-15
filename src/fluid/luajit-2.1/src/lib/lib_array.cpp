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
#include <parasol/main.h>

#define LJLIB_MODULE_array

constexpr auto HASH_INT     = pf::strhash("int");
constexpr auto HASH_BYTE    = pf::strhash("byte");
constexpr auto HASH_CHAR    = pf::strhash("char");
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

static AET parse_elemtype(lua_State *L, int NArg)
{
   GCstr *type_str = lj_lib_checkstr(L, NArg);

   switch (type_str->hash) {
      case HASH_INT:     return AET::_INT32;
      case HASH_BYTE:    return AET::_BYTE;
      case HASH_CHAR:    return AET::_BYTE;
      case HASH_INT16:   return AET::_INT16;
      case HASH_INT64:   return AET::_INT64;
      case HASH_FLOAT:   return AET::_FLOAT;
      case HASH_DOUBLE:  return AET::_DOUBLE;
      case HASH_STRING:  return AET::_STRING;
      case HASH_STRUCT:  return AET::_STRUCT;
      case HASH_POINTER: return AET::_PTR;
   }

   lj_err_argv(L, NArg, ErrMsg::BADTYPE, "valid array type", strdata(type_str));
   return AET(0);  // unreachable
}

//********************************************************************************************************************
// Helper to get element type name

static CSTRING elemtype_name(AET Type)
{
   switch (Type) {
      case AET::_BYTE:   return "char";
      case AET::_INT16:  return "int16";
      case AET::_INT32:  return "int";
      case AET::_INT64:  return "int64";
      case AET::_FLOAT:  return "float";
      case AET::_DOUBLE: return "double";
      case AET::_PTR:    return "pointer";
      case AET::_STRING: return "string";
      case AET::_STRUCT: return "struct";
      default: return "unknown";
   }
}

//********************************************************************************************************************
// Usage: array.new(size, type)
//
// Creates a new array of the specified size and element type.
//
// Parameters:
//   size: number of elements (must be non-negative)
//   type: element type string ("char", "int16", "int", "int64", "float", "double", "pointer", "string", "struct")
//
// Returns: new array
//
// Example: local arr = array.new(100, "int")

LJLIB_CF(array_new)
{
   int32_t size = lj_lib_checkint(L, 1);
   if (size < 0) lj_err_argv(L, 1, ErrMsg::NUMRNG, "non-negative", "negative");

   auto elemtype = parse_elemtype(L, 2);
   GCarray *arr = lj_array_new(L, uint32_t(size), elemtype);

   // Set metatable from registry

   lua_getfield(L, LUA_REGISTRYINDEX, "array_metatable");
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
// Usage: array.table(arr)
//
// Converts an array to a Lua table.
//
// Parameters:
//   arr: the array to convert
//
// Returns: new table with array contents (0-based indexing)

LJLIB_CF(array_table)
{
   GCarray *arr = lib_checkarray(L, 1);
   GCtab *t = lj_array_to_table(L, arr);
   settabV(L, L->top++, t);
   return 1;
}

//********************************************************************************************************************
// Usage: array.concat(StringFormat, JoinString)
//
// Concatenates array elements into a string using the specified format and join string.
//
// StringFormat specifies how each element should be formatted (e.g., "%d", "%f", "%s").
// JoinString is placed between each concatenated element.

LJLIB_CF(array_concat)
{
   GCarray *arr = lib_checkarray(L, 1);

   if (arr->len < 1) {
      lua_pushstring(L, "");
      return 1;
   }

   auto format = luaL_checkstring(L, 1);
   auto join_str = luaL_optstring(L, 2, "");

   // Validate format string - ensure exactly one format specifier

   int format_count = 0;
   bool in_format = false;
   for (auto p = format; *p; p++) {
      if (*p IS '%') {
         if (*(p+1) IS '%') {
            p++; // Skip escaped %
            continue;
         }

         if (in_format) {
            luaL_error(L, "Invalid format string: multiple format specifiers not allowed");
            return 0;
         }
         in_format = true;
      }
      else if (in_format) {
         // Check for end of format specifier
         if (*p IS 'd' or *p IS 'i' or *p IS 'o' or *p IS 'x' or *p IS 'X' or
             *p IS 'u' or *p IS 'c' or *p IS 's' or *p IS 'p' or
             *p IS 'f' or *p IS 'F' or *p IS 'e' or *p IS 'E' or
             *p IS 'g' or *p IS 'G') {
            format_count++;
            in_format = false;
         }
         // Allow format modifiers and flags
         else if (!(*p IS '-' or *p IS '+' or *p IS ' ' or *p IS '#' or *p IS '0' or
                    (*p >= '1' and *p <= '9') or *p IS '.' or *p IS 'l' or *p IS 'h')) {
            luaL_error(L, "Invalid character '%c' in format string", *p);
            return 0;
         }
      }
   }

   if (in_format) {
      luaL_error(L, "Incomplete format specifier");
      return 0;
   }

   if (format_count != 1) {
      luaL_error(L, "Format string must contain exactly one format specifier, found %d", format_count);
      return 0;
   }

   std::string result;
   result.reserve(arr->len * 16);
   char buffer[256];

   for (int i = 0; i < arr->len; i++) {
      if (i > 0) result += join_str;

      switch(arr->elemtype) {
         case AET::_STRING:
            snprintf(buffer, sizeof(buffer), format, arr->data.get<CSTRING>()[i]);
            break;
         case AET::_PTR:
            snprintf(buffer, sizeof(buffer), format, arr->data.get<void **>()[i]);
            break;
         case AET::_FLOAT:
            snprintf(buffer, sizeof(buffer), format, arr->data.get<float>()[i]);
            break;
         case AET::_DOUBLE:
            snprintf(buffer, sizeof(buffer), format, arr->data.get<double>()[i]);
            break;
         case AET::_INT64:
            snprintf(buffer, sizeof(buffer), format, arr->data.get<long long>()[i]);
            break;
         case AET::_INT32:
            snprintf(buffer, sizeof(buffer), format, arr->data.get<int>()[i]);
            break;
         case AET::_INT16:
            snprintf(buffer, sizeof(buffer), format, arr->data.get<int16_t>()[i]);
            break;
         case AET::_BYTE:
            snprintf(buffer, sizeof(buffer), format, arr->data.get<int8_t>()[i]);
            break;
         case AET::_STRUCT:
            luaL_error(L, "concat() does not support struct arrays.");
            return 0;
         default:
            luaL_error(L, "Unsupported array type $%.8x", arr->elemtype);
            return 0;
      }

      result += buffer;
   }

   lua_pushstring(L, result.c_str());
   return 1;
}

//********************************************************************************************************************
// array.copy(dst, src [, dstidx [, srcidx [, count]]])
// Copies elements from source array to destination array.
//
// Parameters:
//   dst: destination array
//   src: source array
//   dstidx: starting index in destination (0-based, default 0)
//   srcidx: starting index in source (0-based, default 0)
//   count: number of elements to copy (default: all remaining elements in source)
//
// Both arrays must have the same element type.  The destination array must not be read-only.

LJLIB_CF(array_copy)
{
   GCarray *dst = lib_checkarray(L, 1);
   GCarray *src = lib_checkarray(L, 2);

   auto dstidx = uint32_t(lj_lib_optint(L, 3, 0));
   auto srcidx = uint32_t(lj_lib_optint(L, 4, 0));
   auto count  = uint32_t(lj_lib_optint(L, 5, int32_t(src->len - srcidx)));

   lj_array_copy(L, dst, dstidx, src, srcidx, count);
   return 0;
}

//********************************************************************************************************************
// array.getstring(arr [, start [, len]])
// Extracts a string from a byte array.
//
// Parameters:
//   arr: byte array
//   start: starting index (0-based, default 0)
//   len: number of bytes to extract (default: remaining bytes from start)
//
// Returns: string containing the bytes

LJLIB_CF(array_getstring)
{
   GCarray *arr = lib_checkarray(L, 1);

   if (arr->elemtype != AET::_BYTE) lj_err_caller(L, ErrMsg::ARRSTR);

   auto start = uint32_t(lj_lib_optint(L, 2, 0));
   auto len = uint32_t(lj_lib_optint(L, 3, int32_t(arr->len - start)));

   if (start + len > arr->len) lj_err_caller(L, ErrMsg::IDXRNG);

   auto data = (CSTRING)mref<void>(arr->data) + start;
   GCstr* s = lj_str_new(L, data, len);
   setstrV(L, L->top++, s);
   return 1;
}

//********************************************************************************************************************
// array.setstring(arr, str [, start])
// Copies string bytes into a byte array.
//
// Parameters:
//   arr: byte array (must not be read-only)
//   str: string to copy
//   start: starting index in array (0-based, default 0)
//
// Returns: number of bytes written

LJLIB_CF(array_setstring)
{
   GCarray *arr = lib_checkarray(L, 1);
   GCstr *str = lj_lib_checkstr(L, 2);

   if (arr->elemtype != AET::_BYTE) lj_err_caller(L, ErrMsg::ARRSTR);
   if (arr->flags & ARRAY_FLAG_READONLY) lj_err_caller(L, ErrMsg::ARRRO);

   auto start = uint32_t(lj_lib_optint(L, 3, 0));
   auto len = str->len;

   // Clamp length to fit in array

   if (start >= arr->len) {
      setintV(L->top++, 0);
      return 1;
   }

   if (start + len > arr->len) len = arr->len - start;

   auto data = (char *)mref<void>(arr->data) + start;
   memcpy(data, strdata(str), len);

   setintV(L->top++, int32_t(len));
   return 1;
}

//********************************************************************************************************************
// Returns the length of an array.

LJLIB_CF(array_len)
{
   GCarray *arr = lib_checkarray(L, 1);
   setintV(L->top++, int32_t(arr->len));
   return 1;
}

//********************************************************************************************************************
// Returns the element type of an array as a string.
//
// Returns: element type string ("byte", "int16", "int", etc.)

LJLIB_CF(array_type)
{
   GCarray *arr = lib_checkarray(L, 1);
   auto name = elemtype_name(arr->elemtype);
   GCstr *s = lj_str_newz(L, name);
   setstrV(L, L->top++, s);
   return 1;
}

//********************************************************************************************************************
// array.readonly(arr)
// Returns whether the array is read-only.
//
// Parameters:
//   arr: the array
//
// Returns: true if read-only, false otherwise

LJLIB_CF(array_readonly)
{
   GCarray *arr = lib_checkarray(L, 1);
   setboolV(L->top++, (arr->flags & ARRAY_FLAG_READONLY) != 0);
   return 1;
}

//********************************************************************************************************************
// array.fill(arr, value [, start [, count]])
// Fills array elements with a value.
//
// Parameters:
//   arr: the array (must not be read-only)
//   value: value to fill with (number)
//   start: starting index (0-based, default 0)
//   count: number of elements to fill (default: all remaining)

LJLIB_CF(array_fill)
{
   GCarray* arr = lib_checkarray(L, 1);
   lua_Number value = lj_lib_checknum(L, 2);

   if (arr->flags & ARRAY_FLAG_READONLY) lj_err_caller(L, ErrMsg::ARRRO);

   auto start = uint32_t(lj_lib_optint(L, 3, 0));
   auto count = uint32_t(lj_lib_optint(L, 4, int32_t(arr->len - start)));

   if (start >= arr->len) return 0;
   if (start + count > arr->len) count = arr->len - start;

   auto base = (uint8_t *)mref<void>(arr->data);

   for (uint32_t i = 0; i < count; i++) {
      void* elem = base + (start + i) * arr->elemsize;
      switch (arr->elemtype) {
         case AET::_BYTE:   *(uint8_t*)elem = uint8_t(value); break;
         case AET::_INT16:  *(int16_t*)elem = int16_t(value); break;
         case AET::_INT32:  *(int32_t*)elem = int32_t(value); break;
         case AET::_INT64:  *(int64_t*)elem = int64_t(value); break;
         case AET::_FLOAT:  *(float*)elem = float(value); break;
         case AET::_DOUBLE: *(double*)elem = value; break;
         default: break;
      }
   }

   return 0;
}

//********************************************************************************************************************
// Metamethod: __index
// Handles array[idx] access.

LJLIB_CF(array___index)
{
   GCarray *arr = lib_checkarray(L, 1);
   int32_t idx = lj_lib_checkint(L, 2);

   if (idx < 0 or MSize(idx) >= arr->len) lj_err_callerv(L, ErrMsg::ARROB, idx, int(arr->len));

   void* elem = lj_array_index(arr, uint32_t(idx));

   switch (arr->elemtype) {
      case AET::_BYTE:   setintV(L->top, *(uint8_t*)elem); break;
      case AET::_INT16:  setintV(L->top, *(int16_t*)elem); break;
      case AET::_INT32:  setintV(L->top, *(int32_t*)elem); break;
      case AET::_INT64:  setnumV(L->top, lua_Number(*(int64_t*)elem)); break;
      case AET::_FLOAT:  setnumV(L->top, *(float*)elem); break;
      case AET::_DOUBLE: setnumV(L->top, *(double*)elem); break;
      case AET::_PTR:    setrawlightudV(L->top, *(void**)elem); break;
      case AET::_STRING: {
         GCRef ref = *(GCRef *)elem;
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

LJLIB_CF(array___newindex)
{
   GCarray* arr = lib_checkarray(L, 1);
   int32_t idx = lj_lib_checkint(L, 2);
   TValue* val = L->base + 2;

   if (arr->flags & ARRAY_FLAG_READONLY) lj_err_caller(L, ErrMsg::ARRRO);
   if (idx < 0 or MSize(idx) >= arr->len) lj_err_callerv(L, ErrMsg::ARROB, idx, int(arr->len));

   void * elem = lj_array_index(arr, uint32_t(idx));

   // Get numeric value from TValue

   lua_Number num = 0;
   if (tvisint(val)) num = lua_Number(intV(val));
   else if (tvisnum(val)) num = numV(val);
   else if (arr->elemtype IS AET::_STRING and tvisstr(val)) {
      GCstr* str = strV(val);
      setgcref(*(GCRef*)elem, obj2gco(str));
      lj_gc_objbarrier(L, arr, str);
      return 0;
   }
   else if (arr->elemtype IS AET::_PTR and tvislightud(val)) {
      *(void**)elem = (void*)(val->u64 & LJ_GCVMASK);
      return 0;
   }
   else if (tvisnil(val)) num = 0;
   else lj_err_caller(L, ErrMsg::ARRTYPE);

   switch (arr->elemtype) {
      case AET::_BYTE:   *(uint8_t*)elem = uint8_t(num); break;
      case AET::_INT16:  *(int16_t*)elem = int16_t(num); break;
      case AET::_INT32:  *(int32_t*)elem = int32_t(num); break;
      case AET::_INT64:  *(int64_t*)elem = int64_t(num); break;
      case AET::_FLOAT:  *(float*)elem = float(num); break;
      case AET::_DOUBLE: *(double*)elem = num; break;
      default: lj_err_caller(L, ErrMsg::ARRTYPE); break;
   }

   return 0;
}

//********************************************************************************************************************
// Metamethod: __len
// Returns array length for # operator.

LJLIB_CF(array___len)
{
   GCarray *arr = lib_checkarray(L, 1);
   setintV(L->top++, int32_t(arr->len));
   return 1;
}

//********************************************************************************************************************
// Metamethod: __tostring
// Returns string representation of array.

LJLIB_CF(array___tostring)
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
// Registers the array library and sets up the array metatable.

extern "C" int luaopen_array(lua_State *L)
{
   LJ_LIB_REG(L, "array", array);

   // Create and register array metatable using Lua stack operations
   lua_createtable(L, 0, 4);  // metatable

   // Set __index from library function
   lua_getfield(L, -2, "__index");  // Get array.__index
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

   // Store metatable in registry as "array_metatable"
   lua_setfield(L, LUA_REGISTRYINDEX, "array_metatable");

   return 1;
}
