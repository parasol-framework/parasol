// Range library for Fluid.
// Copyright (C) 2025 Paul Manias.
//
// Implements a Range type as userdata with support for:
// - Exclusive (default) and inclusive ranges
// - Forward and reverse iteration
// - Custom step values
// - Membership testing via contains()
// - Conversion to table via toTable()

#define lib_range_c
#define LUA_LIB

#include <cstdlib>
#include <cmath>
#include <cstring>

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

#include "lj_obj.h"
#include "lj_gc.h"
#include "lj_err.h"
#include "lj_tab.h"
#include "lj_str.h"
#include "lib.h"
#include "lib_range.h"

#define LJLIB_MODULE_range

//********************************************************************************************************************
// Helper to get range userdata from stack with type checking

static fluid_range* get_range(lua_State* L, int idx)
{
   return (fluid_range*)luaL_checkudata(L, idx, RANGE_METATABLE);
}

//********************************************************************************************************************
// Helper to check if a value is a range userdata (returns nullptr if not)

static fluid_range* check_range(lua_State* L, int idx)
{
   void* ud = lua_touserdata(L, idx);
   if (ud) {
      if (lua_getmetatable(L, idx)) {
         lua_getfield(L, LUA_REGISTRYINDEX, RANGE_METATABLE);
         if (lua_rawequal(L, -1, -2)) {
            lua_pop(L, 2);
            return (fluid_range*)ud;
         }
         lua_pop(L, 2);
      }
   }
   return nullptr;
}

//********************************************************************************************************************
// Calculate the number of elements in a range

static int32_t range_length(const fluid_range* r)
{
   if (r->step == 0) return 0;

   int32_t start = r->start;
   int32_t stop = r->stop;
   int32_t step = r->step;

   // Adjust stop for exclusive ranges
   if (not r->inclusive) {
      if (step > 0) {
         stop = stop - 1;
      }
      else {
         stop = stop + 1;
      }
   }

   // Calculate count
   if (step > 0) {
      if (stop < start) return 0;
      return ((stop - start) / step) + 1;
   }
   else {
      if (stop > start) return 0;
      return ((start - stop) / (-step)) + 1;
   }
}

//********************************************************************************************************************
// range.new(start, stop [, inclusive [, step]])
// Creates a new range object

LJLIB_CF(range_new)
{
   // Check required arguments
   if (lua_gettop(L) < 2) {
      lj_err_caller(L, ErrMsg::NUMRNG);
      return 0;
   }

   // Validate start is a number
   if (not lua_isnumber(L, 1)) {
      lj_err_argt(L, 1, LUA_TNUMBER);
      return 0;
   }

   // Validate stop is a number
   if (not lua_isnumber(L, 2)) {
      lj_err_argt(L, 2, LUA_TNUMBER);
      return 0;
   }

   lua_Number start_num = lua_tonumber(L, 1);
   lua_Number stop_num = lua_tonumber(L, 2);

   // Check for integer values
   int32_t start = (int32_t)start_num;
   int32_t stop = (int32_t)stop_num;

   if ((lua_Number)start != start_num) {
      lj_err_arg(L, 1, ErrMsg::NUMRNG);
      return 0;
   }

   if ((lua_Number)stop != stop_num) {
      lj_err_arg(L, 2, ErrMsg::NUMRNG);
      return 0;
   }

   // Get optional inclusive flag (default: false)
   bool inclusive = false;
   if (lua_gettop(L) >= 3 and not lua_isnil(L, 3)) {
      inclusive = lua_toboolean(L, 3);
   }

   // Get optional step value
   int32_t step;
   if (lua_gettop(L) >= 4 and not lua_isnil(L, 4)) {
      if (not lua_isnumber(L, 4)) {
         lj_err_argt(L, 4, LUA_TNUMBER);
         return 0;
      }
      lua_Number step_num = lua_tonumber(L, 4);
      step = (int32_t)step_num;
      if ((lua_Number)step != step_num) {
         lj_err_arg(L, 4, ErrMsg::NUMRNG);
         return 0;
      }
      if (step == 0) {
         lj_err_arg(L, 4, ErrMsg::NUMRNG);
         return 0;
      }
   }
   else {
      // Auto-detect step based on direction
      step = (start <= stop) ? 1 : -1;
   }

   // Create userdata
   fluid_range* r = (fluid_range*)lua_newuserdata(L, sizeof(fluid_range));
   r->start = start;
   r->stop = stop;
   r->step = step;
   r->inclusive = inclusive;

   // Set metatable
   luaL_getmetatable(L, RANGE_METATABLE);
   lua_setmetatable(L, -2);

   return 1;
}

//********************************************************************************************************************
// range.check(value)
// Returns true if the value is a range object

LJLIB_CF(range_check)
{
   fluid_range* r = check_range(L, 1);
   lua_pushboolean(L, r != nullptr);
   return 1;
}

//********************************************************************************************************************
// __tostring metamethod
// Returns "{start..stop}" or "{start...stop}" based on inclusivity

static int range_tostring(lua_State* L)
{
   fluid_range* r = get_range(L, 1);
   if (r->inclusive) {
      lua_pushfstring(L, "{%d...%d}", r->start, r->stop);
   }
   else {
      lua_pushfstring(L, "{%d..%d}", r->start, r->stop);
   }
   return 1;
}

//********************************************************************************************************************
// __eq metamethod
// Compares two ranges for equality (all fields must match)

static int range_eq(lua_State* L)
{
   fluid_range* r1 = check_range(L, 1);
   fluid_range* r2 = check_range(L, 2);

   if (not r1 or not r2) {
      lua_pushboolean(L, 0);
      return 1;
   }

   bool equal = (r1->start == r2->start) and
                (r1->stop == r2->stop) and
                (r1->step == r2->step) and
                (r1->inclusive == r2->inclusive);

   lua_pushboolean(L, equal);
   return 1;
}

//********************************************************************************************************************
// __len metamethod
// Returns the number of elements in the range

static int range_len(lua_State* L)
{
   fluid_range* r = get_range(L, 1);
   lua_pushinteger(L, range_length(r));
   return 1;
}

//********************************************************************************************************************
// range:contains(n)
// Returns true if n is within the range (respecting step)

static int range_contains(lua_State* L)
{
   fluid_range* r = (fluid_range*)lua_touserdata(L, lua_upvalueindex(1));
   if (not r) {
      lj_err_caller(L, ErrMsg::BADVAL);
      return 0;
   }

   // Handle both r:contains(n) and r.contains(n) syntax
   // With method syntax, position 1 is self (userdata), position 2 is the value
   // With function syntax via upvalue, position 1 is the value
   int arg_pos = lua_isuserdata(L, 1) ? 2 : 1;

   if (not lua_isnumber(L, arg_pos)) {
      lua_pushboolean(L, 0);
      return 1;
   }

   lua_Number n_num = lua_tonumber(L, arg_pos);
   int32_t n = (int32_t)n_num;

   // Check if it's an integer
   if ((lua_Number)n != n_num) {
      lua_pushboolean(L, 0);
      return 1;
   }

   int32_t start = r->start;
   int32_t stop = r->stop;
   int32_t step = r->step;

   // Calculate effective stop for exclusive ranges
   int32_t effective_stop = stop;
   if (not r->inclusive) {
      if (step > 0) {
         effective_stop = stop - 1;
      }
      else {
         effective_stop = stop + 1;
      }
   }

   // Check bounds
   if (step > 0) {
      if (n < start or n > effective_stop) {
         lua_pushboolean(L, 0);
         return 1;
      }
   }
   else {
      if (n > start or n < effective_stop) {
         lua_pushboolean(L, 0);
         return 1;
      }
   }

   // Check step alignment
   int32_t diff = n - start;
   if (diff % step != 0) {
      lua_pushboolean(L, 0);
      return 1;
   }

   lua_pushboolean(L, 1);
   return 1;
}

//********************************************************************************************************************
// range:toTable()
// Returns an array containing all values in the range

static int range_totable(lua_State* L)
{
   fluid_range* r = (fluid_range*)lua_touserdata(L, lua_upvalueindex(1));
   if (not r) {
      lj_err_caller(L, ErrMsg::BADVAL);
      return 0;
   }

   int32_t len = range_length(r);

   // Create table with appropriate size
   lua_createtable(L, len, 0);

   if (len == 0) return 1;

   int32_t step = r->step;
   int32_t stop = r->stop;

   // Calculate effective stop
   if (not r->inclusive) {
      if (step > 0) {
         stop = stop - 1;
      }
      else {
         stop = stop + 1;
      }
   }

   int32_t idx = 0;
   if (step > 0) {
      for (int32_t i = r->start; i <= stop; i += step) {
         lua_pushinteger(L, i);
         lua_rawseti(L, -2, idx++);
      }
   }
   else {
      for (int32_t i = r->start; i >= stop; i += step) {
         lua_pushinteger(L, i);
         lua_rawseti(L, -2, idx++);
      }
   }

   return 1;
}

//********************************************************************************************************************
// __index metamethod
// Handles property access (.start, .stop, .step, .inclusive, .length)
// and method calls (:contains, :toTable)

static int range_index(lua_State* L)
{
   fluid_range* r = get_range(L, 1);

   if (lua_type(L, 2) == LUA_TSTRING) {
      const char* key = lua_tostring(L, 2);

      // Properties
      if (strcmp(key, "start") == 0) {
         lua_pushinteger(L, r->start);
         return 1;
      }
      else if (strcmp(key, "stop") == 0) {
         lua_pushinteger(L, r->stop);
         return 1;
      }
      else if (strcmp(key, "step") == 0) {
         lua_pushinteger(L, r->step);
         return 1;
      }
      else if (strcmp(key, "inclusive") == 0) {
         lua_pushboolean(L, r->inclusive);
         return 1;
      }
      else if (strcmp(key, "length") == 0) {
         lua_pushinteger(L, range_length(r));
         return 1;
      }
      // Methods - return closures with range as upvalue
      else if (strcmp(key, "contains") == 0) {
         lua_pushvalue(L, 1);  // Push the range userdata
         lua_pushcclosure(L, range_contains, 1);
         return 1;
      }
      else if (strcmp(key, "toTable") == 0) {
         lua_pushvalue(L, 1);  // Push the range userdata
         lua_pushcclosure(L, range_totable, 1);
         return 1;
      }
   }

   lua_pushnil(L);
   return 1;
}

//********************************************************************************************************************
// __call metamethod for the library table
// Allows range(start, stop, ...) syntax instead of range.new(start, stop, ...)

static int range_lib_call(lua_State* L)
{
   // Remove the table argument (first argument in __call is the table itself)
   lua_remove(L, 1);

   // Check required arguments
   if (lua_gettop(L) < 2) {
      lj_err_caller(L, ErrMsg::NUMRNG);
      return 0;
   }

   // Validate start is a number
   if (not lua_isnumber(L, 1)) {
      lj_err_argt(L, 1, LUA_TNUMBER);
      return 0;
   }

   // Validate stop is a number
   if (not lua_isnumber(L, 2)) {
      lj_err_argt(L, 2, LUA_TNUMBER);
      return 0;
   }

   lua_Number start_num = lua_tonumber(L, 1);
   lua_Number stop_num = lua_tonumber(L, 2);

   // Check for integer values
   int32_t start = (int32_t)start_num;
   int32_t stop = (int32_t)stop_num;

   if ((lua_Number)start != start_num) {
      lj_err_arg(L, 1, ErrMsg::NUMRNG);
      return 0;
   }

   if ((lua_Number)stop != stop_num) {
      lj_err_arg(L, 2, ErrMsg::NUMRNG);
      return 0;
   }

   // Get optional inclusive flag (default: false)
   bool inclusive = false;
   if (lua_gettop(L) >= 3 and not lua_isnil(L, 3)) {
      inclusive = lua_toboolean(L, 3);
   }

   // Get optional step value
   int32_t step;
   if (lua_gettop(L) >= 4 and not lua_isnil(L, 4)) {
      if (not lua_isnumber(L, 4)) {
         lj_err_argt(L, 4, LUA_TNUMBER);
         return 0;
      }
      lua_Number step_num = lua_tonumber(L, 4);
      step = (int32_t)step_num;
      if ((lua_Number)step != step_num) {
         lj_err_arg(L, 4, ErrMsg::NUMRNG);
         return 0;
      }
      if (step == 0) {
         lj_err_arg(L, 4, ErrMsg::NUMRNG);
         return 0;
      }
   }
   else {
      // Auto-detect step based on direction
      step = (start <= stop) ? 1 : -1;
   }

   // Create userdata
   fluid_range* r = (fluid_range*)lua_newuserdata(L, sizeof(fluid_range));
   r->start = start;
   r->stop = stop;
   r->step = step;
   r->inclusive = inclusive;

   // Set metatable
   luaL_getmetatable(L, RANGE_METATABLE);
   lua_setmetatable(L, -2);

   return 1;
}

//********************************************************************************************************************

#include "lj_libdef.h"

//********************************************************************************************************************
// Register the range library

extern "C" int luaopen_range(lua_State* L)
{
   // Create metatable for range objects
   luaL_newmetatable(L, RANGE_METATABLE);

   // Set __name
   lua_pushstring(L, RANGE_METATABLE);
   lua_setfield(L, -2, "__name");

   // Register metamethods
   lua_pushcfunction(L, range_tostring);
   lua_setfield(L, -2, "__tostring");

   lua_pushcfunction(L, range_eq);
   lua_setfield(L, -2, "__eq");

   lua_pushcfunction(L, range_len);
   lua_setfield(L, -2, "__len");

   lua_pushcfunction(L, range_index);
   lua_setfield(L, -2, "__index");

   lua_pop(L, 1);  // Pop metatable

   // Register the library using LuaJIT's library registration system
   LJ_LIB_REG(L, "range", range);

   // At this point the range table is on the stack, add a metatable with __call
   lua_createtable(L, 0, 1);  // Create metatable for the library table
   lua_pushcfunction(L, range_lib_call);
   lua_setfield(L, -2, "__call");
   lua_setmetatable(L, -2);  // Set metatable on the range library table

   return 1;
}
