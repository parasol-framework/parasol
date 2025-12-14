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
#include "lj_vmarray.h"

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

   // Helper to get int value from TValue (handles both DUALNUM and non-DUALNUM modes)
   auto getintval = [](cTValue* o) -> int32_t {
      if (tvisint(o)) return intV(o);
      if (tvisnum(o)) return int32_t(numV(o));
      return 0;
   };

   // 0-based indexing: array[0] = 100, array[2] = 300, array[4] = 500
   if (!tvisnumber(&array_part[0])) {
      Log.error("table[0] is not a number, itype=%u", itype(&array_part[0]));
      return false;
   }
   if (getintval(&array_part[0]) != 100) {
      Log.error("table[0] has wrong value, expected 100");
      return false;
   }
   if (!tvisnumber(&array_part[2]) or getintval(&array_part[2]) != 300) {
      Log.error("table[2] is not 300");
      return false;
   }
   if (!tvisnumber(&array_part[4]) or getintval(&array_part[4]) != 500) {
      Log.error("table[4] is not 500");
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
// Phase 3 Tests: Bytecode C Helpers
//********************************************************************************************************************

static bool test_arr_getidx_int32(pf::Log& Log)
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
      data[i] = (i + 1) * 100;  // 100, 200, 300, ...
   }

   TValue result;
   lj_arr_getidx(L, arr, 0, &result);
   if (!tvisint(&result) or intV(&result) != 100) {
      Log.error("arr_getidx at index 0 failed: expected 100");
      return false;
   }

   lj_arr_getidx(L, arr, 5, &result);
   if (!tvisint(&result) or intV(&result) != 600) {
      Log.error("arr_getidx at index 5 failed: expected 600");
      return false;
   }

   lj_arr_getidx(L, arr, 9, &result);
   if (!tvisint(&result) or intV(&result) != 1000) {
      Log.error("arr_getidx at index 9 failed: expected 1000");
      return false;
   }

   return true;
}

static bool test_arr_getidx_double(pf::Log& Log)
{
   LuaStateHolder Holder;
   lua_State* L = Holder.get();
   if (not L) {
      Log.error("failed to create Lua state");
      return false;
   }
   luaL_openlibs(L);

   GCarray* arr = lj_array_new(L, 5, ARRAY_ELEM_DOUBLE);
   double* data = (double*)mref(arr->data, void);
   data[0] = 3.14159;
   data[2] = -2.71828;
   data[4] = 1.41421;

   TValue result;
   lj_arr_getidx(L, arr, 0, &result);
   if (!tvisnum(&result) or std::abs(numV(&result) - 3.14159) > 1e-5) {
      Log.error("arr_getidx double at index 0 failed");
      return false;
   }

   lj_arr_getidx(L, arr, 2, &result);
   if (!tvisnum(&result) or std::abs(numV(&result) + 2.71828) > 1e-5) {
      Log.error("arr_getidx double at index 2 failed");
      return false;
   }

   return true;
}

static bool test_arr_setidx_int32(pf::Log& Log)
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

   // Set values using lj_arr_setidx
   TValue val;
   setintV(&val, 12345);
   lj_arr_setidx(L, arr, 0, &val);

   setintV(&val, 67890);
   lj_arr_setidx(L, arr, 5, &val);

   setintV(&val, -99999);
   lj_arr_setidx(L, arr, 9, &val);

   // Verify values were stored correctly
   if (data[0] != 12345) {
      Log.error("arr_setidx at index 0 failed: got %d, expected 12345", data[0]);
      return false;
   }
   if (data[5] != 67890) {
      Log.error("arr_setidx at index 5 failed: got %d, expected 67890", data[5]);
      return false;
   }
   if (data[9] != -99999) {
      Log.error("arr_setidx at index 9 failed: got %d, expected -99999", data[9]);
      return false;
   }

   return true;
}

static bool test_arr_setidx_double(pf::Log& Log)
{
   LuaStateHolder Holder;
   lua_State* L = Holder.get();
   if (not L) {
      Log.error("failed to create Lua state");
      return false;
   }
   luaL_openlibs(L);

   GCarray* arr = lj_array_new(L, 5, ARRAY_ELEM_DOUBLE);
   double* data = (double*)mref(arr->data, void);

   TValue val;
   setnumV(&val, 3.14159);
   lj_arr_setidx(L, arr, 0, &val);

   setnumV(&val, -2.71828);
   lj_arr_setidx(L, arr, 2, &val);

   if (std::abs(data[0] - 3.14159) > 1e-5) {
      Log.error("arr_setidx double at index 0 failed");
      return false;
   }
   if (std::abs(data[2] + 2.71828) > 1e-5) {
      Log.error("arr_setidx double at index 2 failed");
      return false;
   }

   return true;
}

static bool test_arr_roundtrip(pf::Log& Log)
{
   LuaStateHolder Holder;
   lua_State* L = Holder.get();
   if (not L) {
      Log.error("failed to create Lua state");
      return false;
   }
   luaL_openlibs(L);

   GCarray* arr = lj_array_new(L, 100, ARRAY_ELEM_INT32);

   // Write values using setidx, read back using getidx
   for (int32_t i = 0; i < 100; i++) {
      TValue val;
      setintV(&val, i * i);
      lj_arr_setidx(L, arr, i, &val);
   }

   for (int32_t i = 0; i < 100; i++) {
      TValue result;
      lj_arr_getidx(L, arr, i, &result);
      if (!tvisint(&result) or intV(&result) != i * i) {
         Log.error("roundtrip failed at index %d: expected %d", i, i * i);
         return false;
      }
   }

   return true;
}

static bool test_arr_byte_type(pf::Log& Log)
{
   LuaStateHolder Holder;
   lua_State* L = Holder.get();
   if (not L) {
      Log.error("failed to create Lua state");
      return false;
   }
   luaL_openlibs(L);

   GCarray* arr = lj_array_new(L, 256, ARRAY_ELEM_BYTE);
   uint8_t* data = (uint8_t*)mref(arr->data, void);

   // Test byte array stores and retrieves correctly
   for (int i = 0; i < 256; i++) {
      TValue val;
      setintV(&val, i);
      lj_arr_setidx(L, arr, i, &val);
   }

   for (int i = 0; i < 256; i++) {
      TValue result;
      lj_arr_getidx(L, arr, i, &result);
      if (!tvisint(&result) or intV(&result) != i) {
         Log.error("byte array roundtrip failed at index %d", i);
         return false;
      }
      if (data[i] != uint8_t(i)) {
         Log.error("byte array data mismatch at index %d", i);
         return false;
      }
   }

   return true;
}

//********************************************************************************************************************
// Test runner
//********************************************************************************************************************

} // namespace

void array_unit_tests(int& Passed, int& Total)
{
   constexpr std::array<TestCase, 17> Tests = { {
      // Phase 1: Core Data Structures
      { "array_creation_byte", test_array_creation_byte },
      { "array_creation_int32", test_array_creation_int32 },
      { "array_creation_double", test_array_creation_double },
      { "array_index_access", test_array_index_access },
      { "array_elemsize", test_array_elemsize },
      { "array_external", test_array_external },
      { "array_to_table", test_array_to_table },
      { "array_type_tag", test_array_type_tag },
      // Phase 2: VM Type System
      { "tvalue_array", test_tvalue_array },
      { "typename_array", test_typename_array },
      { "setarrayV", test_setarrayV },
      // Phase 3: Bytecode C Helpers
      { "arr_getidx_int32", test_arr_getidx_int32 },
      { "arr_getidx_double", test_arr_getidx_double },
      { "arr_setidx_int32", test_arr_setidx_int32 },
      { "arr_setidx_double", test_arr_setidx_double },
      { "arr_roundtrip", test_arr_roundtrip },
      { "arr_byte_type", test_arr_byte_type }
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
