// Native array library.
// Copyright (C) 2025 Paul Manias.
//
// TODO: Allow array lifetimes to be linked to Parasol objects.  This would allow external array data to be managed
// safely without having to be cached.  In the event that the object is destroyed, the array should be marked as invalid
// and the length reduced to 0 to prevent usage.
//

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
#include "lj_meta.h"
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
constexpr auto HASH_TABLE   = pf::strhash("table");

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
      case HASH_STRING:  return AET::_STRING_GC;
      case HASH_STRUCT:  return AET::_STRUCT;
      case HASH_POINTER: return AET::_PTR;
      case HASH_TABLE:   return AET::_TABLE;
   }

   lj_err_argv(L, NArg, ErrMsg::BADTYPE, "valid array type", strdata(type_str));
   return AET(0);  // unreachable
}

//********************************************************************************************************************
// Helper to get element type name

static CSTRING elemtype_name(AET Type)
{
   switch (Type) {
      case AET::_BYTE:       return "char";
      case AET::_INT16:      return "int16";
      case AET::_INT32:      return "int";
      case AET::_INT64:      return "int64";
      case AET::_FLOAT:      return "float";
      case AET::_DOUBLE:     return "double";
      case AET::_PTR:        return "pointer";
      case AET::_STRUCT:     return "struct";
      case AET::_TABLE:      return "table";
      case AET::_CSTRING:
      case AET::_STRING_GC:
      case AET::_STRING_CPP: return "string";
      default: return "unknown";
   }
}

//********************************************************************************************************************
// Usage: array.new(size, type) or array.new('string')
//
// Creates a new array of the specified size and element type.
//
//   size: number of elements (must be non-negative)
//   type: element type string ("char", "int16", "int", "int64", "float", "double", "string", "StructName")

LJLIB_CF(array_new)
{
   GCarray *arr;

   auto type = lua_type(L, 1);
   if (type IS LUA_TSTRING) {
      TValue *o = L->base;
      GCstr *s = strV(o);
      auto elem_type = AET::_BYTE;
      arr = lj_array_new(L, s->len, elem_type);

      pf::copymem(strdata(s), arr->data.get<CSTRING>(), s->len);
   }
   else {
      auto size = lj_lib_checkint(L, 1);
      if (size < 0) lj_err_argv(L, 1, ErrMsg::NUMRNG, "non-negative", "negative");
      auto elem_type = parse_elemtype(L, 2);

      if (elem_type IS AET::_PTR) lj_err_argv(L, 2, ErrMsg::ARRTYPE); // For Parasol functions only
      else if (elem_type IS AET::_STRUCT) lj_err_argv(L, 2, ErrMsg::ARRTYPE); // For Parasol functions only (for now)

      arr = lj_array_new(L, uint32_t(size), elem_type);
   }

   // Per-instance metatable is null - base metatable will be used automatically

   setarrayV(L, L->top++, arr);
   return 1;
}

//********************************************************************************************************************
// Usage: array.table(arr)
//
// Converts an array to a Lua table.
//
//   arr: the array to convert

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
         case AET::_STRING_GC: {
            GCRef ref = arr->data.get<GCRef>()[i];
            if (gcref(ref)) snprintf(buffer, sizeof(buffer), format, strdata(gco2str(gcref(ref))));
            else snprintf(buffer, sizeof(buffer), format, "");
            break;
         }
         case AET::_CSTRING:
            snprintf(buffer, sizeof(buffer), format, arr->data.get<CSTRING>()[i]);
            break;
         case AET::_STRING_CPP:
            snprintf(buffer, sizeof(buffer), format, arr->data.get<std::string>()[i].c_str());
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
         case AET::_TABLE:
            luaL_error(L, "concat() does not support table arrays.");
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

LJLIB_CF(array_clear)
{
   GCarray *arr = lj_lib_checkarray(L, 1);
   if (arr->flags & ARRAY_READONLY) lj_err_caller(L, ErrMsg::ARRRO);
   arr->clear();
   return 0;
}

//********************************************************************************************************************
// Usage: array.copy(dst, src [, dest_idx [, src_idx [, count]]])
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

   if (dest->flags & ARRAY_READONLY) lj_err_caller(L, ErrMsg::ARRRO);

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

      for (int i = 0; i < copy_total; i++) {
         lua_pushinteger(L, src_idx + i);
         lua_gettable(L, 2);        // Get table[src_idx + i]

         MSize dest_index = c_index + i;

         // Convert and store based on array type

         switch(dest->elemtype) {
            case AET::_STRING_CPP:
               if (lua_tostring(L, -1)) dest->data.get<std::string>()[dest_index].assign(lua_tostring(L, -1));
               else dest->data.get<std::string>()[dest_index].clear();
               break;
            case AET::_STRING_GC:
               if (lua_tostring(L, -1)) {
                  luaL_error(L, "Writing to string arrays from tables is not yet supported.");
                  lua_pop(L, 1);
                  return 0;
               }
               break;
            case AET::_CSTRING:
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
               // TODO: We should check the struct fields to confirm if its content can be safely copied.
               // This would only have to be done once per struct type, so we could cache the result.
               luaL_error(L, "Writing to struct arrays from tables is not yet supported.");
               lua_pop(L, 1);
               return 0;
            case AET::_TABLE:
               if (lua_istable(L, -1)) {
                  TValue *tv = L->top - 1;
                  GCtab *tab = tabV(tv);
                  setgcref(dest->data.get<GCRef>()[dest_index], obj2gco(tab));
                  lj_gc_objbarrier(L, dest, tab);
               }
               else if (lua_isnil(L, -1)) {
                  setgcrefnull(dest->data.get<GCRef>()[dest_index]);
               }
               else {
                  luaL_error(L, "Expected table value at index %d.", src_idx + i);
                  lua_pop(L, 1);
                  return 0;
               }
               break;
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
// Usage: array.getString(arr [, start [, len]])
//
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
   if (start + len > int(arr->len)) lj_err_caller(L, ErrMsg::IDXRNG);

   auto data = (CSTRING)mref<void>(arr->data) + start;
   GCstr *s = lj_str_new(L, data, len);
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
   if (arr->flags & ARRAY_READONLY) lj_err_caller(L, ErrMsg::ARRRO);

   auto start = lj_lib_optint(L, 3, 0);
   if (start < 0) lj_err_caller(L, ErrMsg::IDXRNG);

   auto len = int(str->len);

   // Clamp length to fit in array

   if (start >= int(arr->len)) {
      setintV(L->top++, 0);
      return 1;
   }

   if (start + len > int(arr->len)) len = arr->len - start;

   auto data = (char *)mref<void>(arr->data) + start;
   memcpy(data, strdata(str), len);

   setintV(L->top++, int32_t(len));
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
// Usage: array.readOnly(arr)
//
// Returns whether the array is read-only.
//
// Parameters:
//   arr: the array
//
// Returns: true if read-only, false otherwise

LJLIB_CF(array_readOnly)
{
   GCarray *arr = lj_lib_checkarray(L, 1);
   setboolV(L->top++, (arr->flags & ARRAY_READONLY) != 0);
   return 1;
}

//********************************************************************************************************************
// Template-based fill for contiguous ranges (step=1). Uses std::fill for optimal performance.

template<typename T>
static void fill_contiguous(void *Data, int32_t Start, int32_t Count, lua_Number Value)
{
   T *ptr = static_cast<T *>(Data) + Start;
   std::fill(ptr, ptr + Count, T(Value));
}

//********************************************************************************************************************
// Template-based fill for stepped ranges. Hoists type dispatch outside the loop.

template<typename T>
static void fill_stepped(void *Data, int32_t Start, int32_t Stop, int32_t Step, lua_Number Value)
{
   T *base = static_cast<T *>(Data);
   T val = T(Value);
   if (Step > 0) {
      for (int32_t i = Start; i <= Stop; i += Step) base[i] = val;
   }
   else {
      for (int32_t i = Start; i >= Stop; i += Step) base[i] = val;
   }
}

//********************************************************************************************************************
// Helper function to fill array elements with a value.
// Uses optimised contiguous fill when step=1, otherwise falls back to stepped fill.

static void fill_array_elements(GCarray *Arr, lua_Number Value, int32_t Start, int32_t Stop, int32_t Step)
{
   void *data = Arr->data.get<void>();

   // Optimised path for contiguous fills (step=1, forward direction)
   if (Step IS 1) {
      int32_t count = Stop - Start + 1;
      switch (Arr->elemtype) {
         case AET::_BYTE:   fill_contiguous<uint8_t>(data, Start, count, Value); return;
         case AET::_INT16:  fill_contiguous<int16_t>(data, Start, count, Value); return;
         case AET::_INT32:  fill_contiguous<int32_t>(data, Start, count, Value); return;
         case AET::_INT64:  fill_contiguous<int64_t>(data, Start, count, Value); return;
         case AET::_FLOAT:  fill_contiguous<float>(data, Start, count, Value); return;
         case AET::_DOUBLE: fill_contiguous<double>(data, Start, count, Value); return;
         default: return;
      }
   }

   // Stepped fill path (non-contiguous or reverse direction)
   switch (Arr->elemtype) {
      case AET::_BYTE:   fill_stepped<uint8_t>(data, Start, Stop, Step, Value); break;
      case AET::_INT16:  fill_stepped<int16_t>(data, Start, Stop, Step, Value); break;
      case AET::_INT32:  fill_stepped<int32_t>(data, Start, Stop, Step, Value); break;
      case AET::_INT64:  fill_stepped<int64_t>(data, Start, Stop, Step, Value); break;
      case AET::_FLOAT:  fill_stepped<float>(data, Start, Stop, Step, Value); break;
      case AET::_DOUBLE: fill_stepped<double>(data, Start, Stop, Step, Value); break;
      default: break;
   }
}

//********************************************************************************************************************
// Usage: array.fill(arr, value [, start [, count]]) or array.fill(arr, value, range)
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

   if (arr->flags & ARRAY_READONLY) lj_err_caller(L, ErrMsg::ARRRO);

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
// Template-based find for contiguous forward search (step=1). Hoists type dispatch outside the loop.

template<typename T>
static int32_t find_forward_contiguous(const void *Data, int32_t Start, int32_t Stop, lua_Number Value)
{
   const T *base = static_cast<const T *>(Data);
   T val = T(Value);
   for (int32_t i = Start; i <= Stop; i++) {
      if (base[i] IS val) return i;
   }
   return -1;
}

//********************************************************************************************************************
// Template-based find for stepped ranges. Hoists type dispatch outside the loop.

template<typename T>
static int32_t find_stepped(const void *Data, int32_t Start, int32_t Stop, int32_t Step, lua_Number Value)
{
   const T *base = static_cast<const T *>(Data);
   T val = T(Value);
   if (Step > 0) {
      for (int32_t i = Start; i <= Stop; i += Step) {
         if (base[i] IS val) return i;
      }
   }
   else {
      for (int32_t i = Start; i >= Stop; i += Step) {
         if (base[i] IS val) return i;
      }
   }
   return -1;
}

//********************************************************************************************************************
// Dispatches find operation based on array element type.
// Returns index if found, -1 if not found.

static int32_t find_in_array(GCarray *Arr, lua_Number Value, int32_t Start, int32_t Stop, int32_t Step)
{
   const void *data = Arr->data.get<void>();

   // Optimised path for contiguous forward search (step=1)
   if (Step IS 1) {
      switch (Arr->elemtype) {
         case AET::_BYTE:   return find_forward_contiguous<uint8_t>(data, Start, Stop, Value);
         case AET::_INT16:  return find_forward_contiguous<int16_t>(data, Start, Stop, Value);
         case AET::_INT32:  return find_forward_contiguous<int32_t>(data, Start, Stop, Value);
         case AET::_INT64:  return find_forward_contiguous<int64_t>(data, Start, Stop, Value);
         case AET::_FLOAT:  return find_forward_contiguous<float>(data, Start, Stop, Value);
         case AET::_DOUBLE: return find_forward_contiguous<double>(data, Start, Stop, Value);
         default: return -1;
      }
   }

   // Stepped search path (non-contiguous or reverse direction)
   switch (Arr->elemtype) {
      case AET::_BYTE:   return find_stepped<uint8_t>(data, Start, Stop, Step, Value);
      case AET::_INT16:  return find_stepped<int16_t>(data, Start, Stop, Step, Value);
      case AET::_INT32:  return find_stepped<int32_t>(data, Start, Stop, Step, Value);
      case AET::_INT64:  return find_stepped<int64_t>(data, Start, Stop, Step, Value);
      case AET::_FLOAT:  return find_stepped<float>(data, Start, Stop, Step, Value);
      case AET::_DOUBLE: return find_stepped<double>(data, Start, Stop, Step, Value);
      default: return -1;
   }
}

//********************************************************************************************************************
// Usage: array.find(arr, value [, start]) or array.find(arr, value, range)
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

      // Search within the range using optimised template dispatch
      int32_t result = find_in_array(arr, value, start, effective_stop, step);
      if (result >= 0) {
         setintV(L->top++, result);
         return 1;
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

   int32_t result = find_in_array(arr, value, start, int32_t(arr->len - 1), 1);
   if (result >= 0) {
      setintV(L->top++, result);
      return 1;
   }

   lua_pushnil(L);
   return 1;
}

//********************************************************************************************************************
// Usage: array.reverse(arr)
//
// Reverses the array elements in place.
//
// Parameters:
//   arr: the array to reverse (must not be read-only)

LJLIB_CF(array_reverse)
{
   GCarray *arr = lj_lib_checkarray(L, 1);

   if (arr->flags & ARRAY_READONLY) lj_err_caller(L, ErrMsg::ARRRO);
   if (arr->len < 2) return 0;

   void *data = arr->data.get<void>();

   // Use std::reverse with typed pointers for optimal performance
   switch (arr->elemtype) {
      case AET::_BYTE: {
         auto *p = static_cast<uint8_t *>(data);
         std::reverse(p, p + arr->len);
         break;
      }
      case AET::_INT16: {
         auto *p = static_cast<int16_t *>(data);
         std::reverse(p, p + arr->len);
         break;
      }
      case AET::_INT32: {
         auto *p = static_cast<int32_t *>(data);
         std::reverse(p, p + arr->len);
         break;
      }
      case AET::_INT64: {
         auto *p = static_cast<int64_t *>(data);
         std::reverse(p, p + arr->len);
         break;
      }
      case AET::_FLOAT: {
         auto *p = static_cast<float *>(data);
         std::reverse(p, p + arr->len);
         break;
      }
      case AET::_DOUBLE: {
         auto *p = static_cast<double *>(data);
         std::reverse(p, p + arr->len);
         break;
      }
      case AET::_PTR: {
         auto *p = static_cast<void **>(data);
         std::reverse(p, p + arr->len);
         break;
      }
      case AET::_STRING_GC:
      case AET::_TABLE: {
         auto *p = static_cast<GCRef *>(data);
         std::reverse(p, p + arr->len);
         break;
      }
      default: {
         // Fallback for struct types using byte-level swap
         auto *base = static_cast<uint8_t *>(data);
         auto elemsize = arr->elemsize;
         uint8_t temp[64];  // Large enough for typical struct sizes
         for (MSize i = 0; i < arr->len / 2; i++) {
            MSize j = arr->len - 1 - i;
            void *elem_i = base + i * elemsize;
            void *elem_j = base + j * elemsize;
            memcpy(temp, elem_i, elemsize);
            memcpy(elem_i, elem_j, elemsize);
            memcpy(elem_j, temp, elemsize);
         }
         break;
      }
   }

   return 0;
}

//********************************************************************************************************************
// Usage: array.slice(arr, range)
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
// Usage: array.sort(arr [, descending])
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

   if (arr->flags & ARRAY_READONLY) lj_err_caller(L, ErrMsg::ARRRO);
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
// Registers the array library and sets up the base metatable for arrays.
// Unlike the Lua table, arrays are created via conventional means, i.e. array.new().
//
// The array library table itself serves as the base metatable, allowing direct method
// lookup (arr:concat(), arr:sort(), etc.) via lj_tab_get in the VM array helpers.

#include "lj_libdef.h"

extern "C" int luaopen_array(lua_State *L)
{
   LJ_LIB_REG(L, "array", array);
   // Stack: [..., array_lib_table]

   // Use the library table directly as the base metatable for arrays.
   // This allows lj_arr_get to find methods like concat, sort, etc. via direct table lookup.
   GCtab *lib = tabV(L->top - 1);
   global_State *g = G(L);

   // NOBARRIER: basemt is a GC root.
   setgcref(basemt_it(g, LJ_TARRAY), obj2gco(lib));

   return 1;
}
