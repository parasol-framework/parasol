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
#include "lib_range.h"

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
// Usage: array.new(size, type) or array.new('string')
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
   GCarray *arr;
   int32_t size = 0;
   AET elem_type;
   auto type = lua_type(L, 1);

   if (type IS LUA_TSTRING) {
      TValue *o = L->base;
      GCstr *s = strV(o);
      elem_type = AET::_BYTE;
      arr = lj_array_new(L, s->len, elem_type);

      pf::copymem(strdata(s), arr->data.get<CSTRING>(), s->len);
   }
   else {
      size = lj_lib_checkint(L, 1);
      if (size < 0) lj_err_argv(L, 1, ErrMsg::NUMRNG, "non-negative", "negative");
      elem_type = parse_elemtype(L, 2);
      arr = lj_array_new(L, uint32_t(size), elem_type);
   }

   // Set metatable from registry

   lua_getfield(L, LUA_REGISTRYINDEX, "array_metatable");
   if (tvistab(L->top - 1)) {
      GCtab *mt = tabV(L->top - 1);
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
   GCarray *arr = lj_lib_checkarray(L, 1);
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
   GCarray *arr = lj_lib_checkarray(L, 1);

   if (arr->len < 1) {
      lua_pushstring(L, "");
      return 1;
   }

   auto format = luaL_checkstring(L, 2);
   auto join_str = luaL_optstring(L, 3, "");

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

   for (MSize i = 0; i < arr->len; i++) {
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
// array.copy(dst, src [, dest_idx [, src_idx [, count]]])
//
// Copies elements from source array to destination array.
//
// Parameters:
//   dest:     destination array
//   src:      source array, string, or table
//   dest_idx: starting index in destination (0-based, default 0)
//   src_idx:  starting index in source (0-based, default 0)
//   count:    number of elements to copy (default: all remaining elements in source)
//
// For array sources, both arrays must have the same element type.
// The destination array must not be read-only.

LJLIB_CF(array_copy)
{
   GCarray *dest = lj_lib_checkarray(L, 1);

   if (dest->flags & ARRAY_FLAG_READONLY) lj_err_caller(L, ErrMsg::ARRRO);

   size_t strlen;
   auto type = lua_type(L, 2);
   if (type IS LUA_TARRAY) {
      GCarray *src = lj_lib_checkarray(L, 2);
      auto dest_idx = lj_lib_optint(L, 3, 0);
      auto src_idx  = lj_lib_optint(L, 4, 0);
      auto count    = lj_lib_optint(L, 5, int32_t(src->len - src_idx));

      lj_array_copy(L, dest, dest_idx, src, src_idx, count);
      return 0;
   }
   else if (type IS LUA_TSTRING) {
      // Treat string sequences as a byte array
      auto str = lua_tolstring(L, 2, &strlen);
      if (!str or strlen < 1) {
         luaL_argerror(L, 2, "String is empty.");
         return 0;
      }

      auto dest_idx   = lj_lib_optint(L, 3, 0);
      auto src_idx    = lj_lib_optint(L, 4, 0);
      auto copy_total = lj_lib_optint(L, 5, int32_t(strlen - src_idx));

      // Bounds check source

      if ((src_idx < 0) or (size_t(src_idx) >= strlen)) {
         luaL_error(L, "Source index %d out of bounds (string length: %d).", src_idx, int(strlen));
         return 0;
      }
      if (size_t(src_idx + copy_total) > strlen) copy_total = int32_t(strlen) - src_idx;

      // Bounds check destination

      if ((dest_idx < 0) or (MSize(dest_idx) >= dest->len)) {
         luaL_error(L, "Destination index %d out of bounds (array size: %d).", dest_idx, int(dest->len));
         return 0;
      }

      if (MSize(dest_idx + copy_total) > dest->len) {
         luaL_error(L, "String copy would exceed array bounds (%d+%d > %d).", dest_idx, copy_total, int(dest->len));
         return 0;
      }

      // Copy string bytes to array
      auto data = (uint8_t*)mref<void>(dest->data) + dest_idx;
      memcpy(data, str + src_idx, copy_total);
      return 0;
   }
   else if (type IS LUA_TTABLE) {
      // Get table length for bounds checking
      MSize table_len = lua_objlen(L, 2);
      if (table_len < 1) {
         luaL_argerror(L, 2, "Table is empty.");
         return 0;
      }

      auto dest_idx   = lj_lib_optint(L, 3, 0);
      auto src_idx    = lj_lib_optint(L, 4, 0);
      auto copy_total = lj_lib_optint(L, 5, int32_t(table_len - src_idx));

      // Bounds check source index
      if ((src_idx < 0) or (MSize(src_idx) >= table_len)) {
         luaL_error(L, "Source index %d out of bounds (table length: %d).", src_idx, int(table_len));
         return 0;
      }

      if (MSize(copy_total) > table_len - MSize(src_idx)) copy_total = int32_t(table_len - MSize(src_idx));

      // Check bounds for destination array
      if ((dest_idx < 0) or (MSize(dest_idx) >= dest->len)) {
         luaL_error(L, "Destination index out of bounds: %d (array size: %d).", dest_idx, dest->len);
         return 0;
      }

      if (MSize(dest_idx + copy_total) > dest->len) {
         luaL_error(L, "Table copy would exceed array bounds (%d+%d > %d).", dest_idx, copy_total, dest->len);
         return 0;
      }

      // Copy table elements using ipairs-style iteration

      auto c_index = dest_idx;

      for (MSize i = 0; i < copy_total; i++) {
         lua_pushinteger(L, src_idx + i);
         lua_gettable(L, 2);        // Get table[src_idx + i]

         MSize dest_index = c_index + i;

         // Convert and store based on array type

         switch(dest->elemtype) {
            case AET::_STRING:
               if (lua_tostring(L, -1)) {
                  luaL_error(L, "Writing to string arrays from tables is not yet supported.");
                  lua_pop(L, 1);
                  return 0;
               }
               break;
            case AET::_PTR:
               luaL_error(L, "Writing to pointer arrays from tables is not supported.");
               lua_pop(L, 1);
               return 0;
            case AET::_FLOAT:
               dest->data.get<float>()[dest_index] = lua_tonumber(L, -1);
               break;
            case AET::_DOUBLE:
               dest->data.get<double>()[dest_index] = lua_tonumber(L, -1);
               break;
            case AET::_INT64:
               dest->data.get<int64_t>()[dest_index] = lua_tointeger(L, -1);
               break;
            case AET::_INT32:
               dest->data.get<int>()[dest_index] = lua_tointeger(L, -1);
               break;
            case AET::_INT16:
               dest->data.get<int16_t>()[dest_index] = lua_tointeger(L, -1);
               break;
            case AET::_BYTE:
               dest->data.get<int8_t>()[dest_index] = lua_tointeger(L, -1);
               break;
            case AET::_STRUCT:
               luaL_error(L, "Writing to struct arrays from tables is not yet supported.");
               lua_pop(L, 1);
               return 0;
            default:
               luaL_error(L, "Unsupported array type $%.8x", dest->elemtype);
               lua_pop(L, 1);
               return 0;
         }

         lua_pop(L, 1); // Remove the value from stack
      }

      return 0;
   }
   else {
      luaL_argerror(L, 2, "String, array or table expected.");
      return 0;
   }
}

//********************************************************************************************************************
// array.getString(arr [, start [, len]])
// Extracts a string from a byte array.
//
// Parameters:
//   arr: byte array
//   start: starting index (0-based, default 0)
//   len: number of bytes to extract (default: remaining bytes from start)
//
// Returns: string containing the bytes

LJLIB_CF(array_getString)
{
   GCarray *arr = lj_lib_checkarray(L, 1);

   if (arr->elemtype != AET::_BYTE) lj_err_caller(L, ErrMsg::ARRSTR);

   auto start = lj_lib_optint(L, 2, 0);
   if (start < 0) lj_err_caller(L, ErrMsg::IDXRNG);
   auto len = lj_lib_optint(L, 3, arr->len - start);
   if (len < 0) lj_err_caller(L, ErrMsg::IDXRNG);
   if (start + len > arr->len) lj_err_caller(L, ErrMsg::IDXRNG);

   auto data = (CSTRING)mref<void>(arr->data) + start;
   GCstr* s = lj_str_new(L, data, len);
   setstrV(L, L->top++, s);
   return 1;
}

//********************************************************************************************************************
// Usage: array.setString(arr, str [, start])
//
// Copies string bytes into a byte array.
//
// Parameters:
//   arr: byte array (must not be read-only)
//   str: string to copy
//   start: starting index in array (0-based, default 0)
//
// Returns: number of bytes written

LJLIB_CF(array_setString)
{
   GCarray *arr = lj_lib_checkarray(L, 1);
   GCstr *str = lj_lib_checkstr(L, 2);

   if (arr->elemtype != AET::_BYTE) lj_err_caller(L, ErrMsg::ARRSTR);
   if (arr->flags & ARRAY_FLAG_READONLY) lj_err_caller(L, ErrMsg::ARRRO);

   auto start = lj_lib_optint(L, 3, 0);
   if (start < 0) lj_err_caller(L, ErrMsg::IDXRNG);

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
   GCarray *arr = lj_lib_checkarray(L, 1);
   setintV(L->top++, int32_t(arr->len));
   return 1;
}

//********************************************************************************************************************
// Returns the element type of an array as a string.
//
// Returns: element type string ("byte", "int16", "int", etc.)

LJLIB_CF(array_type)
{
   GCarray *arr = lj_lib_checkarray(L, 1);
   auto name = elemtype_name(arr->elemtype);
   GCstr *s = lj_str_newz(L, name);
   setstrV(L, L->top++, s);
   return 1;
}

//********************************************************************************************************************
// array.readOnly(arr)
// Returns whether the array is read-only.
//
// Parameters:
//   arr: the array
//
// Returns: true if read-only, false otherwise

LJLIB_CF(array_readOnly)
{
   GCarray *arr = lj_lib_checkarray(L, 1);
   setboolV(L->top++, (arr->flags & ARRAY_FLAG_READONLY) != 0);
   return 1;
}

//********************************************************************************************************************
// Helper function to fill array elements with a value
// Used by array_fill to handle both integer indices and range-based filling

static void fill_array_elements(GCarray *Arr, lua_Number Value, int32_t Start, int32_t Stop, int32_t Step)
{
   auto base = (uint8_t *)mref<void>(Arr->data);
   auto elemsize = Arr->elemsize;

   if (Step > 0) {
      for (int32_t i = Start; i <= Stop; i += Step) {
         void *elem = base + i * elemsize;
         switch (Arr->elemtype) {
            case AET::_BYTE:   *(uint8_t *)elem = uint8_t(Value); break;
            case AET::_INT16:  *(int16_t *)elem = int16_t(Value); break;
            case AET::_INT32:  *(int32_t *)elem = int32_t(Value); break;
            case AET::_INT64:  *(int64_t *)elem = int64_t(Value); break;
            case AET::_FLOAT:  *(float *)elem = float(Value); break;
            case AET::_DOUBLE: *(double *)elem = Value; break;
            default: break;
         }
      }
   }
   else {
      for (int32_t i = Start; i >= Stop; i += Step) {
         void *elem = base + i * elemsize;
         switch (Arr->elemtype) {
            case AET::_BYTE:   *(uint8_t *)elem = uint8_t(Value); break;
            case AET::_INT16:  *(int16_t *)elem = int16_t(Value); break;
            case AET::_INT32:  *(int32_t *)elem = int32_t(Value); break;
            case AET::_INT64:  *(int64_t *)elem = int64_t(Value); break;
            case AET::_FLOAT:  *(float *)elem = float(Value); break;
            case AET::_DOUBLE: *(double *)elem = Value; break;
            default: break;
         }
      }
   }
}

//********************************************************************************************************************
// array.fill(arr, value [, start [, count]])
// array.fill(arr, value, range)
//
// Fills array elements with a value.
//
// Parameters (integer form):
//   arr: the array (must not be read-only)
//   value: value to fill with (number)
//   start: starting index (0-based, default 0)
//   count: number of elements to fill (default: all remaining)
//
// Parameters (range form):
//   arr: the array (must not be read-only)
//   value: value to fill with (number)
//   range: range object specifying which elements to fill

LJLIB_CF(array_fill)
{
   GCarray *arr = lj_lib_checkarray(L, 1);
   lua_Number value = lj_lib_checknum(L, 2);

   if (arr->flags & ARRAY_FLAG_READONLY) lj_err_caller(L, ErrMsg::ARRRO);

   // Check if third argument is a range
   fluid_range *r = check_range(L, 3);
   if (r) {
      int32_t len = int32_t(arr->len);
      int32_t start = r->start;
      int32_t stop = r->stop;
      int32_t step = r->step;

      // Handle negative indices
      bool use_inclusive = r->inclusive;
      if (start < 0 or stop < 0) {
         use_inclusive = true;
         if (start < 0) start += len;
         if (stop < 0) stop += len;
      }

      // Determine iteration direction
      bool forward = (start <= stop);
      if (step IS 0) step = forward ? 1 : -1;
      if (forward and step < 0) step = 1;
      if (not forward and step > 0) step = -1;

      // Calculate effective stop for exclusive ranges
      int32_t effective_stop = stop;
      if (not use_inclusive) {
         if (forward) effective_stop = stop - 1;
         else effective_stop = stop + 1;
      }

      // Bounds clipping
      if (forward) {
         if (start < 0) start = 0;
         if (effective_stop >= len) effective_stop = len - 1;
      }
      else {
         if (start >= len) start = len - 1;
         if (effective_stop < 0) effective_stop = 0;
      }

      // Check for empty/invalid ranges
      if (len IS 0 or (forward and start > effective_stop) or (not forward and start < effective_stop)) {
         return 0;
      }

      fill_array_elements(arr, value, start, effective_stop, step);
      return 0;
   }

   // Original integer-based fill
   auto start = lj_lib_optint(L, 3, 0);
   if (start < 0) lj_err_caller(L, ErrMsg::IDXRNG);

   auto count = lj_lib_optint(L, 4, int32_t(arr->len - start));
   if (count < 0) lj_err_caller(L, ErrMsg::IDXRNG);

   if (MSize(start) >= arr->len) return 0;
   if (MSize(start + count) > arr->len) count = int32_t(arr->len) - start;

   fill_array_elements(arr, value, start, start + count - 1, 1);
   return 0;
}

//********************************************************************************************************************
// Helper to check if an array element matches a value

static bool element_matches(GCarray *Arr, int32_t Idx, lua_Number Value)
{
   auto base = (uint8_t *)mref<void>(Arr->data);
   void *elem = base + Idx * Arr->elemsize;

   switch (Arr->elemtype) {
      case AET::_BYTE:   return (*(uint8_t *)elem IS uint8_t(Value));
      case AET::_INT16:  return (*(int16_t *)elem IS int16_t(Value));
      case AET::_INT32:  return (*(int32_t *)elem IS int32_t(Value));
      case AET::_INT64:  return (*(int64_t *)elem IS int64_t(Value));
      case AET::_FLOAT:  return (*(float *)elem IS float(Value));
      case AET::_DOUBLE: return (*(double *)elem IS Value);
      default: return false;
   }
}

//********************************************************************************************************************
// array.find(arr, value [, start])
// array.find(arr, value, range)
//
// Searches for a value in the array.
//
// Parameters (integer form):
//   arr: the array to search
//   value: the value to find
//   start: starting index (0-based, default 0)
//
// Parameters (range form):
//   arr: the array to search
//   value: the value to find
//   range: range object specifying which elements to search
//
// Returns: index of first occurrence, or nil if not found

LJLIB_CF(array_find)
{
   GCarray *arr = lj_lib_checkarray(L, 1);
   lua_Number value = lj_lib_checknum(L, 2);

   // Check if third argument is a range
   fluid_range *r = check_range(L, 3);
   if (r) {
      int32_t len = int32_t(arr->len);
      int32_t start = r->start;
      int32_t stop = r->stop;
      int32_t step = r->step;

      // Handle negative indices
      bool use_inclusive = r->inclusive;
      if (start < 0 or stop < 0) {
         use_inclusive = true;
         if (start < 0) start += len;
         if (stop < 0) stop += len;
      }

      // Determine iteration direction
      bool forward = (start <= stop);
      if (step IS 0) step = forward ? 1 : -1;
      if (forward and step < 0) step = 1;
      if (not forward and step > 0) step = -1;

      // Calculate effective stop for exclusive ranges
      int32_t effective_stop = stop;
      if (not use_inclusive) {
         if (forward) effective_stop = stop - 1;
         else effective_stop = stop + 1;
      }

      // Bounds clipping
      if (forward) {
         if (start < 0) start = 0;
         if (effective_stop >= len) effective_stop = len - 1;
      }
      else {
         if (start >= len) start = len - 1;
         if (effective_stop < 0) effective_stop = 0;
      }

      // Check for empty/invalid ranges
      if (len IS 0 or (forward and start > effective_stop) or (not forward and start < effective_stop)) {
         lua_pushnil(L);
         return 1;
      }

      // Search within the range
      if (forward) {
         for (int32_t i = start; i <= effective_stop; i += step) {
            if (element_matches(arr, i, value)) {
               setintV(L->top++, i);
               return 1;
            }
         }
      }
      else {
         for (int32_t i = start; i >= effective_stop; i += step) {
            if (element_matches(arr, i, value)) {
               setintV(L->top++, i);
               return 1;
            }
         }
      }

      lua_pushnil(L);
      return 1;
   }

   // Original integer-based find
   auto start = lj_lib_optint(L, 3, 0);

   if (start < 0) start = 0;
   if (MSize(start) >= arr->len) {
      lua_pushnil(L);
      return 1;
   }

   for (MSize i = MSize(start); i < arr->len; i++) {
      if (element_matches(arr, int32_t(i), value)) {
         setintV(L->top++, int32_t(i));
         return 1;
      }
   }

   lua_pushnil(L);
   return 1;
}

//********************************************************************************************************************
// array.reverse(arr)
//
// Reverses the array elements in place.
//
// Parameters:
//   arr: the array to reverse (must not be read-only)

LJLIB_CF(array_reverse)
{
   GCarray *arr = lj_lib_checkarray(L, 1);

   if (arr->flags & ARRAY_FLAG_READONLY) lj_err_caller(L, ErrMsg::ARRRO);
   if (arr->len < 2) return 0;

   auto base = (uint8_t *)mref<void>(arr->data);
   auto elemsize = arr->elemsize;

   // Allocate temporary buffer on the stack for element swapping
   uint8_t temp[16];  // Large enough for any element type (max is double = 8 bytes)

   for (MSize i = 0; i < arr->len / 2; i++) {
      MSize j = arr->len - 1 - i;
      void *elem_i = base + i * elemsize;
      void *elem_j = base + j * elemsize;

      memcpy(temp, elem_i, elemsize);
      memcpy(elem_i, elem_j, elemsize);
      memcpy(elem_j, temp, elemsize);
   }

   return 0;
}

//********************************************************************************************************************
// array.slice(arr, range)
//
// Creates a new array containing elements specified by the range.
//
// Parameters:
//   arr: the source array
//   range: a range object specifying start, stop, step, and inclusivity
//
// Returns: new array containing the slice
//
// Delegates to range.slice() for the actual implementation.

LJLIB_CF(array_slice)
{
   lj_lib_checkarray(L, 1);  // Validate first arg is an array
   return lj_range_slice(L);
}

//********************************************************************************************************************
// array.sort(arr [, descending])
//
// Sorts the numeric array in place using quicksort.
//
// Parameters:
//   arr: the array to sort (must not be read-only, must be numeric type)
//   descending: if true, sort in descending order (default: false/ascending)
//
// Note: Does not support string, pointer, or struct arrays.

template<typename T>
static void quicksort(T *Data, int32_t Left, int32_t Right, bool Descending)
{
   if (Left >= Right) return;

   // Partition
   T pivot = Data[(Left + Right) / 2];
   int32_t i = Left, j = Right;

   while (i <= j) {
      if (Descending) {
         while (Data[i] > pivot) i++;
         while (Data[j] < pivot) j--;
      }
      else {
         while (Data[i] < pivot) i++;
         while (Data[j] > pivot) j--;
      }

      if (i <= j) {
         T temp = Data[i];
         Data[i] = Data[j];
         Data[j] = temp;
         i++;
         j--;
      }
   }

   if (Left < j) quicksort(Data, Left, j, Descending);
   if (i < Right) quicksort(Data, i, Right, Descending);
}

LJLIB_CF(array_sort)
{
   GCarray *arr = lj_lib_checkarray(L, 1);
   bool descending = lua_toboolean(L, 2);

   if (arr->flags & ARRAY_FLAG_READONLY) lj_err_caller(L, ErrMsg::ARRRO);
   if (arr->len < 2) return 0;

   switch (arr->elemtype) {
      case AET::_BYTE: quicksort(arr->data.get<uint8_t>(), 0, int32_t(arr->len - 1), descending); break;
      case AET::_INT16: quicksort(arr->data.get<int16_t>(), 0, int32_t(arr->len - 1), descending); break;
      case AET::_INT32: quicksort(arr->data.get<int32_t>(), 0, int32_t(arr->len - 1), descending); break;
      case AET::_INT64: quicksort(arr->data.get<int64_t>(), 0, int32_t(arr->len - 1), descending); break;
      case AET::_FLOAT: quicksort(arr->data.get<float>(), 0, int32_t(arr->len - 1), descending); break;
      case AET::_DOUBLE: quicksort(arr->data.get<double>(), 0, int32_t(arr->len - 1), descending); break;
      default: luaL_error(L, "sort() does not support this array type."); return 0;
   }

   return 0;
}

//********************************************************************************************************************
// Metamethod: __len
// Returns array length for # operator.

LJLIB_CF(array___len)
{
   GCarray *arr = lj_lib_checkarray(L, 1);
   setintV(L->top++, int32_t(arr->len));
   return 1;
}

//********************************************************************************************************************
// Metamethod: __tostring
// Returns string representation of array.

LJLIB_CF(array___tostring)
{
   GCarray* arr = lj_lib_checkarray(L, 1);
   const char* type_name = elemtype_name(arr->elemtype);

   // Format: array(SIZE, "TYPE")
   char buf[128];
   snprintf(buf, sizeof(buf), "array(%u, \"%s\")", uint32_t(arr->len), type_name);

   GCstr* s = lj_str_newz(L, buf);
   setstrV(L, L->top++, s);
   return 1;
}

//********************************************************************************************************************
// Registers the array library and sets up the array metatable.
// Unlike the Lua table, arrays are created via conventional means, i.e. array.new().

#include "lj_libdef.h"

extern "C" int luaopen_array(lua_State *L)
{
   LJ_LIB_REG(L, "array", array);

   // Store the array library table as the metatable for arrays.
   // This allows method calls like arr:concat() to find the concat function
   // in the library table. The library table is left on top of the stack by LJ_LIB_REG.

   lua_pushvalue(L, -1);  // Duplicate library table
   lua_setfield(L, LUA_REGISTRYINDEX, "array_metatable");

   return 1;
}
