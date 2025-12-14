// Unit tests for native array type.
// Copyright (C) 2024 Parasol Framework. See Copyright Notice in parasol.h

#include <parasol/main.h>

#ifdef ENABLE_UNIT_TESTS

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

#include "lj_obj.h"
#include "lj_gc.h"
#include "lj_array.h"
#include "lj_tab.h"

#include <cmath>
#include <cstring>
#include <array>

#include "../../defs.h"

static objScript* glArrayTestScript = nullptr;

namespace {

struct TestCase {
   const char* name;
   bool (*fn)(pf::Log& Log);
};

struct LuaStateHolder {
   LuaStateHolder()
   {
      this->state = luaL_newstate(glArrayTestScript);
   }

   ~LuaStateHolder()
   {
      if (this->state) {
         lua_close(this->state);
      }
   }

   lua_State* get() const { return this->state; }

private:
   lua_State* state = nullptr;
};

//********************************************************************************************************************
// Phase 1 Tests: Core Data Structures
//********************************************************************************************************************

static bool test_array_creation_byte(pf::Log& Log)
{
   LuaStateHolder Holder;
   lua_State* L = Holder.get();
   if (not L) {
      Log.error("failed to create Lua state");
      return false;
   }
   luaL_openlibs(L);

   GCarray* arr = lj_array_new(L, 100, ARRAY_ELEM_BYTE);

   if (not arr) {
      Log.error("byte array creation failed");
      return false;
   }
   if (arr->len != 100) {
      Log.error("byte array has incorrect length: %d", arr->len);
      return false;
   }
   if (arr->elemtype != ARRAY_ELEM_BYTE) {
      Log.error("byte array has incorrect elemtype: %d", arr->elemtype);
      return false;
   }
   if (arr->elemsize != sizeof(uint8_t)) {
      Log.error("byte array has incorrect elemsize: %d", arr->elemsize);
      return false;
   }
   if (!(arr->flags & ARRAY_FLAG_COLOCATED)) {
      Log.error("byte array should be colocated");
      return false;
   }
   if (mref(arr->data, void) IS nullptr) {
      Log.error("byte array data pointer is null");
      return false;
   }

   // Verify zero-initialisation
   uint8_t* data = (uint8_t*)mref(arr->data, void);
   for (int i = 0; i < 100; i++) {
      if (data[i] != 0) {
         Log.error("byte array not zero-initialised at index %d", i);
         return false;
      }
   }

   return true;
}

static bool test_array_creation_int32(pf::Log& Log)
{
   LuaStateHolder Holder;
   lua_State* L = Holder.get();
   if (not L) {
      Log.error("failed to create Lua state");
      return false;
   }
   luaL_openlibs(L);

   GCarray* arr = lj_array_new(L, 50, ARRAY_ELEM_INT32);

   if (not arr) {
      Log.error("int32 array creation failed");
      return false;
   }
   if (arr->len != 50) {
      Log.error("int32 array has incorrect length: %d", arr->len);
      return false;
   }
   if (arr->elemsize != sizeof(int32_t)) {
      Log.error("int32 array has incorrect elemsize: %d", arr->elemsize);
      return false;
   }

   // Write and read back values
   int32_t* data = (int32_t*)mref(arr->data, void);
   for (int i = 0; i < 50; i++) {
      data[i] = i * 100;
   }

   for (int i = 0; i < 50; i++) {
      if (data[i] != i * 100) {
         Log.error("int32 array read/write mismatch at index %d", i);
         return false;
      }
   }

   return true;
}

static bool test_array_creation_double(pf::Log& Log)
{
   LuaStateHolder Holder;
   lua_State* L = Holder.get();
   if (not L) {
      Log.error("failed to create Lua state");
      return false;
   }
   luaL_openlibs(L);

   GCarray* arr = lj_array_new(L, 25, ARRAY_ELEM_DOUBLE);

   if (not arr) {
      Log.error("double array creation failed");
      return false;
   }
   if (arr->elemsize != sizeof(double)) {
      Log.error("double array has incorrect elemsize: %d", arr->elemsize);
      return false;
   }

   double* data = (double*)mref(arr->data, void);
   data[0] = 3.14159265358979;
   data[24] = -2.71828182845904;

   if (std::abs(data[0] - 3.14159265358979) > 1e-10) {
      Log.error("double array does not store pi correctly");
      return false;
   }
   if (std::abs(data[24] + 2.71828182845904) > 1e-10) {
      Log.error("double array does not store e correctly");
      return false;
   }

   return true;
}

static bool test_array_index_access(pf::Log& Log)
{
   LuaStateHolder Holder;
   lua_State* L = Holder.get();
   if (not L) {
      Log.error("failed to create Lua state");
      return false;
   }
   luaL_openlibs(L);

   GCarray* arr = lj_array_new(L, 10, ARRAY_ELEM_INT32);
   int32_t* data = (int32_t*)mref(arr->data, void);

   for (int i = 0; i < 10; i++) {
      data[i] = i + 1;
   }

   // Test lj_array_index
   int32_t* elem0 = (int32_t*)lj_array_index(arr, 0);
   int32_t* elem5 = (int32_t*)lj_array_index(arr, 5);
   int32_t* elem9 = (int32_t*)lj_array_index(arr, 9);

   if (*elem0 != 1) {
      Log.error("array_index returns incorrect element 0: %d", *elem0);
      return false;
   }
   if (*elem5 != 6) {
      Log.error("array_index returns incorrect element 5: %d", *elem5);
      return false;
   }
   if (*elem9 != 10) {
      Log.error("array_index returns incorrect element 9: %d", *elem9);
      return false;
   }

   return true;
}

static bool test_array_elemsize(pf::Log& Log)
{
   if (lj_array_elemsize(ARRAY_ELEM_BYTE) != 1) {
      Log.error("ARRAY_ELEM_BYTE size incorrect");
      return false;
   }
   if (lj_array_elemsize(ARRAY_ELEM_INT16) != 2) {
      Log.error("ARRAY_ELEM_INT16 size incorrect");
      return false;
   }
   if (lj_array_elemsize(ARRAY_ELEM_INT32) != 4) {
      Log.error("ARRAY_ELEM_INT32 size incorrect");
      return false;
   }
   if (lj_array_elemsize(ARRAY_ELEM_INT64) != 8) {
      Log.error("ARRAY_ELEM_INT64 size incorrect");
      return false;
   }
   if (lj_array_elemsize(ARRAY_ELEM_FLOAT) != 4) {
      Log.error("ARRAY_ELEM_FLOAT size incorrect");
      return false;
   }
   if (lj_array_elemsize(ARRAY_ELEM_DOUBLE) != 8) {
      Log.error("ARRAY_ELEM_DOUBLE size incorrect");
      return false;
   }
   if (lj_array_elemsize(ARRAY_ELEM_PTR) != sizeof(void*)) {
      Log.error("ARRAY_ELEM_PTR size incorrect");
      return false;
   }

   return true;
}

static bool test_array_external(pf::Log& Log)
{
   LuaStateHolder Holder;
   lua_State* L = Holder.get();
   if (not L) {
      Log.error("failed to create Lua state");
      return false;
   }
   luaL_openlibs(L);

   // Create external buffer
   int32_t external_data[5] = { 10, 20, 30, 40, 50 };

   GCarray* arr = lj_array_new_external(L, external_data, 5, ARRAY_ELEM_INT32, ARRAY_FLAG_READONLY);

   if (not arr) {
      Log.error("external array creation failed");
      return false;
   }
   if (!(arr->flags & ARRAY_FLAG_EXTERNAL)) {
      Log.error("external array not marked as external");
      return false;
   }
   if (!(arr->flags & ARRAY_FLAG_READONLY)) {
      Log.error("external array not marked as readonly");
      return false;
   }
   if (mref(arr->data, void) != external_data) {
      Log.error("external array does not point to original data");
      return false;
   }

   int32_t* data = (int32_t*)mref(arr->data, void);
   if (data[2] != 30) {
      Log.error("external array reads incorrectly: got %d, expected 30", data[2]);
      return false;
   }

   return true;
}

static bool test_array_to_table(pf::Log& Log)
{
   LuaStateHolder Holder;
   lua_State* L = Holder.get();
   if (not L) {
      Log.error("failed to create Lua state");
      return false;
   }
   luaL_openlibs(L);

   GCarray* arr = lj_array_new(L, 5, ARRAY_ELEM_INT32);
   int32_t* data = (int32_t*)mref(arr->data, void);
   data[0] = 100;
   data[1] = 200;
   data[2] = 300;
   data[3] = 400;
   data[4] = 500;

   GCtab* t = lj_array_to_table(L, arr);
   if (not t) {
      Log.error("table creation from array failed");
      return false;
   }

   TValue* array_part = tvref(t->array);
   if (!tvisint(&array_part[1]) or intV(&array_part[1]) != 100) {
      Log.error("table[1] is not 100");
      return false;
   }
   if (!tvisint(&array_part[3]) or intV(&array_part[3]) != 300) {
      Log.error("table[3] is not 300");
      return false;
   }
   if (!tvisint(&array_part[5]) or intV(&array_part[5]) != 500) {
      Log.error("table[5] is not 500");
      return false;
   }

   return true;
}

static bool test_array_type_tag(pf::Log& Log)
{
   LuaStateHolder Holder;
   lua_State* L = Holder.get();
   if (not L) {
      Log.error("failed to create Lua state");
      return false;
   }
   luaL_openlibs(L);

   GCarray* arr = lj_array_new(L, 10, ARRAY_ELEM_BYTE);

   if (arr->gct != uint8_t(~LJ_TARRAY)) {
      Log.error("array has incorrect GC type tag: %d, expected %d", arr->gct, uint8_t(~LJ_TARRAY));
      return false;
   }

   return true;
}

//********************************************************************************************************************
// Phase 2 Tests: VM Type System Integration
//********************************************************************************************************************

static bool test_tvalue_array(pf::Log& Log)
{
   LuaStateHolder Holder;
   lua_State* L = Holder.get();
   if (not L) {
      Log.error("failed to create Lua state");
      return false;
   }
   luaL_openlibs(L);

   GCarray* arr = lj_array_new(L, 10, ARRAY_ELEM_INT32);

   // Create a TValue holding the array
   TValue tv;
   setgcVraw(&tv, obj2gco(arr), LJ_TARRAY);

   if (itype(&tv) != LJ_TARRAY) {
      Log.error("TValue itype does not match LJ_TARRAY: %u vs %u", itype(&tv), LJ_TARRAY);
      return false;
   }
   if (!tvisarray(&tv)) {
      Log.error("tvisarray check failed");
      return false;
   }
   if (arrayV(&tv) != arr) {
      Log.error("arrayV does not extract correct pointer");
      return false;
   }

   return true;
}

static bool test_typename_array(pf::Log& Log)
{
   const char* name = lj_obj_itypename[~LJ_TARRAY];
   if (strcmp(name, "array") != 0) {
      Log.error("array typename is '%s', expected 'array'", name);
      return false;
   }

   return true;
}

static bool test_setarrayV(pf::Log& Log)
{
   LuaStateHolder Holder;
   lua_State* L = Holder.get();
   if (not L) {
      Log.error("failed to create Lua state");
      return false;
   }
   luaL_openlibs(L);

   GCarray* arr = lj_array_new(L, 5, ARRAY_ELEM_DOUBLE);

   TValue tv;
   setarrayV(L, &tv, arr);

   if (!tvisarray(&tv)) {
      Log.error("setarrayV did not set array type");
      return false;
   }
   if (arrayV(&tv) != arr) {
      Log.error("setarrayV did not store correct pointer");
      return false;
   }

   return true;
}

//********************************************************************************************************************
// Test runner
//********************************************************************************************************************

} // namespace

void array_unit_tests(int& Passed, int& Total)
{
   constexpr std::array<TestCase, 11> Tests = { {
      { "array_creation_byte", test_array_creation_byte },
      { "array_creation_int32", test_array_creation_int32 },
      { "array_creation_double", test_array_creation_double },
      { "array_index_access", test_array_index_access },
      { "array_elemsize", test_array_elemsize },
      { "array_external", test_array_external },
      { "array_to_table", test_array_to_table },
      { "array_type_tag", test_array_type_tag },
      { "tvalue_array", test_tvalue_array },
      { "typename_array", test_typename_array },
      { "setarrayV", test_setarrayV }
   } };

   if (NewObject(CLASSID::FLUID, &glArrayTestScript) != ERR::Okay) return;
   glArrayTestScript->setStatement("");
   if (Action(AC::Init, glArrayTestScript, nullptr) != ERR::Okay) return;

   for (const TestCase& Test : Tests) {
      pf::Log Log("ArrayTests");
      Log.branch("Running %s", Test.name);
      ++Total;
      if (Test.fn(Log)) {
         ++Passed;
         Log.msg("%s passed", Test.name);
      }
      else {
         Log.error("%s failed", Test.name);
      }
   }

   FreeResource(glArrayTestScript);
   glArrayTestScript = nullptr;
}

#else

void array_unit_tests(int&, int&)
{
   // Unit tests disabled
}

#endif // ENABLE_UNIT_TESTS
