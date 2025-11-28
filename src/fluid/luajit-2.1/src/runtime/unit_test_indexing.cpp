// Unit tests for array indexing configuration.

#include <parasol/main.h>

#ifdef ENABLE_UNIT_TESTS

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

#include "lj_obj.h"
#include "lj_str.h"
#include "lj_tab.h"

#include <array>
#include <string>
#include <string_view>

#include "../../defs.h"

static objScript *glTestScript = nullptr;

namespace {

#undef STRINGIFY
#define STRINGIFY_HELPER(x) #x
#define STRINGIFY(x) STRINGIFY_HELPER(x)

struct LuaStateHolder {
   LuaStateHolder()
   {
      this->state = luaL_newstate(glTestScript);
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

struct TestCase {
   const char* name;
   bool (*fn)(pf::Log& Log);
};

// Execute Lua code and check result
static bool run_lua_test(lua_State* L, std::string_view Code, std::string& Error)
{
   if (luaL_loadbuffer(L, Code.data(), Code.size(), "indexing-test")) {
      Error = lua_tostring(L, -1);
      lua_pop(L, 1);
      return false;
   }
   if (lua_pcall(L, 0, LUA_MULTRET, 0)) {
      Error = lua_tostring(L, -1);
      lua_pop(L, 1);
      return false;
   }
   return true;
}

//********************************************************************************************************************
// Core indexing tests - validate LJ_STARTING_INDEX configuration

static bool test_starting_index_constant(pf::Log& Log)
{
   static_assert(LJ_STARTING_INDEX IS 0 or LJ_STARTING_INDEX IS 1,
      "LJ_STARTING_INDEX must be 0 or 1");

   Log.msg("LJ_STARTING_INDEX = %d", LJ_STARTING_INDEX);
   return true;
}

static bool test_array_first_element_access(pf::Log& Log)
{
   LuaStateHolder Holder;
   lua_State* L = Holder.get();
   if (not L) {
      Log.error("failed to create Lua state");
      return false;
   }
   luaL_openlibs(L);

   std::string Error;
   const char* Code = R"(
      local t = {10, 20, 30}
      return t[)" STRINGIFY(LJ_STARTING_INDEX) R"(]
   )";

   if (not run_lua_test(L, Code, Error)) {
      Log.error("test failed: %s", Error.c_str());
      return false;
   }

   lua_Number Value = lua_tonumber(L, -1);
   if (Value IS 10) {
      return true;
   }

   Log.error("expected first element to be 10, got %g", Value);
   return false;
}

static bool test_table_length_operator(pf::Log& Log)
{
   LuaStateHolder Holder;
   lua_State* L = Holder.get();
   if (not L) {
      Log.error("failed to create Lua state");
      return false;
   }
   luaL_openlibs(L);

   std::string Error;
   if (not run_lua_test(L, "local t = {1, 2, 3, 4, 5} return #t", Error)) {
      Log.error("test failed: %s", Error.c_str());
      return false;
   }

   lua_Number Length = lua_tonumber(L, -1);
   if (Length IS 5) return true;

   Log.error("expected length 5, got %g", Length);
   return false;
}

static bool test_ipairs_starting_index(pf::Log& Log)
{
   LuaStateHolder Holder;
   lua_State* L = Holder.get();
   if (not L) {
      Log.error("failed to create Lua state");
      return false;
   }
   luaL_openlibs(L);

   std::string Error;
   const char* Code = R"(
      local first = nil
      local count = 0
      for i, v in ipairs({10, 20, 30}) do
         if not first then first = i end
         count = count + 1
      end
      return first, count
   )";

   if (not run_lua_test(L, Code, Error)) {
      Log.error("test failed: %s", Error.c_str());
      return false;
   }

   lua_Number First = lua_tonumber(L, -2);
   lua_Number Count = lua_tonumber(L, -1);
   if (First IS LJ_STARTING_INDEX and Count IS 3) return true;

   Log.error("expected first index %d and count 3, got %g and %g", LJ_STARTING_INDEX, First, Count);
   return false;
}

static bool test_table_insert_position(pf::Log& Log)
{
   LuaStateHolder Holder;
   lua_State* L = Holder.get();
   if (not L) {
      Log.error("failed to create Lua state");
      return false;
   }
   luaL_openlibs(L);

   std::string Error;
   const char* Code = R"(
      local t = {}
      table.insert(t, 'a')
      table.insert(t, 'b')
      return t[)" STRINGIFY(LJ_STARTING_INDEX) R"(], t[)" STRINGIFY(LJ_STARTING_INDEX) R"( + 1], #t
   )";

   if (not run_lua_test(L, Code, Error)) {
      Log.error("test failed: %s", Error.c_str());
      return false;
   }

   const char* First = lua_tostring(L, -3);
   const char* Second = lua_tostring(L, -2);
   lua_Number Length = lua_tonumber(L, -1);

   if (First and Second and std::string_view(First) IS std::string_view("a") and
      std::string_view(Second) IS std::string_view("b") and Length IS 2) {
      return true;
   }

   Log.error("unexpected table.insert results, got '%s', '%s', len=%g", First ? First : "(nil)",
      Second ? Second : "(nil)", Length);
   return false;
}

static bool test_string_find_returns_correct_index(pf::Log& Log)
{
   LuaStateHolder Holder;
   lua_State* L = Holder.get();
   if (not L) {
      Log.error("failed to create Lua state");
      return false;
   }
   luaL_openlibs(L);

   std::string Error;
   if (not run_lua_test(L, "return string.find('hello', 'l')", Error)) {
      Log.error("test failed: %s", Error.c_str());
      return false;
   }

   lua_Number Index = lua_tonumber(L, -2);
   lua_Number Expected = LJ_STARTING_INDEX + 2;
   if (Index IS Expected) return true;

   Log.error("expected index %g, got %g", Expected, Index);
   return false;
}

static bool test_string_byte_default_start(pf::Log& Log)
{
   LuaStateHolder Holder;
   lua_State* L = Holder.get();
   if (not L) {
      Log.error("failed to create Lua state");
      return false;
   }
   luaL_openlibs(L);

   std::string Error;
   if (not run_lua_test(L, "return string.byte('ABC')", Error)) {
      Log.error("test failed: %s", Error.c_str());
      return false;
   }

   lua_Number Byte = lua_tonumber(L, -1);
   if (Byte IS 65) return true;

   Log.error("expected first byte 65, got %g", Byte);
   return false;
}

static bool test_unpack_default_range(pf::Log& Log)
{
   LuaStateHolder Holder;
   lua_State* L = Holder.get();
   if (not L) {
      Log.error("failed to create Lua state");
      return false;
   }
   luaL_openlibs(L);

   std::string Error;
   if (not run_lua_test(L, "return unpack({10, 20, 30})", Error)) {
      Log.error("test failed: %s", Error.c_str());
      return false;
   }

   lua_Number A = lua_tonumber(L, -3);
   lua_Number B = lua_tonumber(L, -2);
   lua_Number C = lua_tonumber(L, -1);
   if (A IS 10 and B IS 20 and C IS 30) return true;

   Log.error("expected 10, 20, 30, got %g, %g, %g", A, B, C);
   return false;
}

static bool test_table_concat_default_range(pf::Log& Log)
{
   LuaStateHolder Holder;
   lua_State* L = Holder.get();
   if (not L) {
      Log.error("failed to create Lua state");
      return false;
   }
   luaL_openlibs(L);

   std::string Error;
   if (not run_lua_test(L, "return table.concat({'a', 'b', 'c'}, ',')", Error)) {
      Log.error("test failed: %s", Error.c_str());
      return false;
   }

   const char* Result = lua_tostring(L, -1);
   if (Result and std::string_view(Result) IS std::string_view("a,b,c")) return true;

   Log.error("expected 'a,b,c', got '%s'", Result ? Result : "(nil)");
   return false;
}

static bool test_table_sort_operates_on_sequence(pf::Log& Log)
{
   LuaStateHolder Holder;
   lua_State* L = Holder.get();
   if (not L) {
      Log.error("failed to create Lua state");
      return false;
   }
   luaL_openlibs(L);

   std::string Error;
   const char* Code = R"(
      local t = {3, 1, 2}
      table.sort(t)
      return t[)" STRINGIFY(LJ_STARTING_INDEX) R"(], t[)" STRINGIFY(LJ_STARTING_INDEX) R"( + 1], t[)" STRINGIFY(LJ_STARTING_INDEX) R"( + 2]
   )";

   if (not run_lua_test(L, Code, Error)) {
      Log.error("test failed: %s", Error.c_str());
      return false;
   }

   lua_Number A = lua_tonumber(L, -3);
   lua_Number B = lua_tonumber(L, -2);
   lua_Number C = lua_tonumber(L, -1);
   if (A IS 1 and B IS 2 and C IS 3) return true;

   Log.error("expected sorted values 1, 2, 3, got %g, %g, %g", A, B, C);
   return false;
}

static bool test_empty_table_length(pf::Log& Log)
{
   LuaStateHolder Holder;
   lua_State* L = Holder.get();
   if (not L) {
      Log.error("failed to create Lua state");
      return false;
   }
   luaL_openlibs(L);

   std::string Error;
   if (not run_lua_test(L, "return #{}", Error)) {
      Log.error("test failed: %s", Error.c_str());
      return false;
   }

   lua_Number Length = lua_tonumber(L, -1);
   if (Length IS 0) return true;

   Log.error("expected empty table length 0, got %g", Length);
   return false;
}

static bool test_negative_string_indices_unchanged(pf::Log& Log)
{
   LuaStateHolder Holder;
   lua_State* L = Holder.get();
   if (not L) {
      Log.error("failed to create Lua state");
      return false;
   }
   luaL_openlibs(L);

   std::string Error;
   if (not run_lua_test(L, "return string.sub('hello', -1)", Error)) {
      Log.error("test failed: %s", Error.c_str());
      return false;
   }

   const char* Result = lua_tostring(L, -1);
   if (Result and std::string_view(Result) IS std::string_view("o")) return true;

   Log.error("expected 'o', got '%s'", Result ? Result : "(nil)");
   return false;
}

//********************************************************************************************************************
// Low-level table API tests

static bool test_lj_tab_getint_semantic_index(pf::Log& Log)
{
   LuaStateHolder Holder;
   lua_State* L = Holder.get();
   if (not L) {
      Log.error("failed to create Lua state");
      return false;
   }

   GCtab* Table = lj_tab_new(L, 4, 0);
   TValue Value;
   setintV(&Value, 10);
   copyTV(L, lj_tab_setint(L, Table, LJ_STARTING_INDEX), &Value);
   setintV(&Value, 20);
   copyTV(L, lj_tab_setint(L, Table, LJ_STARTING_INDEX + 1), &Value);
   setintV(&Value, 30);
   copyTV(L, lj_tab_setint(L, Table, LJ_STARTING_INDEX + 2), &Value);

   cTValue* First = lj_tab_getint(Table, LJ_STARTING_INDEX);
   // Note: tvisint() returns false when LJ_DUALNUM is not enabled (the default).
   // In that case, setintV stores the value as a number, so check tvisnumber instead.
   if (First and tvisnumber(First) and numberVnum(First) == 10.0) return true;

   Log.error("lj_tab_getint failed for first element");
   return false;
}

static bool test_lj_tab_len_returns_element_count(pf::Log& Log)
{
   LuaStateHolder Holder;
   lua_State* L = Holder.get();
   if (not L) {
      Log.error("failed to create Lua state");
      return false;
   }

   GCtab* Table = lj_tab_new(L, 4, 0);
   TValue Value;
   for (int Index = 0; Index < 3; Index++) {
      setintV(&Value, (Index + 1) * 10);
      copyTV(L, lj_tab_setint(L, Table, LJ_STARTING_INDEX + Index), &Value);
   }

   MSize Length = lj_tab_len(Table);
   if (Length IS 3) return true;

   Log.error("expected lj_tab_len to return 3, got %u", (unsigned)Length);
   return false;
}

}  // namespace

extern void indexing_unit_tests(int& Passed, int& Total)
{
   constexpr std::array<TestCase, 14> Tests = { {
      { "starting_index_constant", test_starting_index_constant },
      { "array_first_element_access", test_array_first_element_access },
      { "table_length_operator", test_table_length_operator },
      { "ipairs_starting_index", test_ipairs_starting_index },
      { "table_insert_position", test_table_insert_position },
      { "string_find_returns_correct_index", test_string_find_returns_correct_index },
      { "string_byte_default_start", test_string_byte_default_start },
      { "unpack_default_range", test_unpack_default_range },
      { "table_concat_default_range", test_table_concat_default_range },
      { "table_sort_operates_on_sequence", test_table_sort_operates_on_sequence },
      { "empty_table_length", test_empty_table_length },
      { "negative_string_indices_unchanged", test_negative_string_indices_unchanged },
      { "lj_tab_getint_semantic_index", test_lj_tab_getint_semantic_index },
      { "lj_tab_len_returns_element_count", test_lj_tab_len_returns_element_count }
   } };

   if (NewObject(CLASSID::FLUID, &glTestScript) != ERR::Okay) return;
   glTestScript->setStatement("");
   if (Action(AC::Init, glTestScript, nullptr) != ERR::Okay) return;

   for (const TestCase& Test : Tests) {
      pf::Log Log("IndexingTests");
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
}

#endif // ENABLE_UNIT_TESTS
