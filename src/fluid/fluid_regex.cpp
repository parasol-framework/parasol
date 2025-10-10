/*********************************************************************************************************************

This source code implements compiled regex objects for Fluid, providing high-performance regular expression
capabilities through the optimized SRELL regex library.

Features:

- Compiled regex objects with automatic garbage collection
- Full capture group support
- Global and single match operations
- String replacement with backreferences
- Pattern-based string splitting
- Comprehensive error handling for invalid patterns
- C++20 optimized SRELL backend for superior performance

Examples:

  local reg = regex.new("\\d+", REGEX_ICASE)
  local matches = reg:match("Hello 123 World")
  local result = reg:replace("abc123def", "XXX")

*********************************************************************************************************************/

#define PRV_SCRIPT
#define PRV_FLUID
#define PRV_FLUID_MODULE
#include <parasol/main.h>
#include <parasol/modules/fluid.h>
#include <parasol/modules/regex.h>
#include <parasol/strings.hpp>
#include <string>

extern "C" {
 #include "lauxlib.h"
 #include "lj_obj.h"
}

#include "hashes.h"
#include "defs.h"

//*********************************************************************************************************************
// Dynamic loader for the Regex functionality.  We only load it as needed due to the size of the module.

static ERR load_regex(void)
{
#ifndef PARASOL_STATIC
   if (not modRegex) {
      pf::SwitchContext ctx(glFluidContext);
      if (objModule::load("regex", &modRegex, &RegexBase) != ERR::Okay) return ERR::InitModule;
   }
#endif
   return ERR::Okay;
}

//*********************************************************************************************************************

static ERR match_many(int Index, std::vector<std::string_view> Captures, std::string_view Prefix, std::string_view Suffix, APTR Meta)
{
   pf::Log log(__FUNCTION__);
   log.warning("%d: %d captures", Index, (int)Captures.size());

   auto Lua = (lua_State *)Meta;

   lua_pushinteger(Lua, Index+1);

   // Create match table for this result
   lua_createtable(Lua, (int)Captures.size(), 0);

   for (int j = 0; j < std::ssize(Captures); ++j) {
      lua_pushinteger(Lua, (lua_Integer)(j + 1));
      lua_pushlstring(Lua, Captures[j].data(), Captures[j].length());
      lua_settable(Lua, -3);
   }

   lua_settable(Lua, -3); // Add match table to results

   return ERR::Okay;
}

static ERR match_one(int Index, std::vector<std::string_view> Captures, std::string_view Prefix, std::string_view Suffix, APTR Meta)
{
   pf::Log log(__FUNCTION__);
   log.warning("%d: %d captures", Index, (int)Captures.size());

   auto Lua = (lua_State *)Meta;

   lua_pushinteger(Lua, Index+1);

   // Create match table for this result
   lua_createtable(Lua, (int)Captures.size(), 0);

   for (int j = 0; j < std::ssize(Captures); ++j) {
      lua_pushinteger(Lua, (lua_Integer)(j + 1));
      lua_pushlstring(Lua, Captures[j].data(), Captures[j].length());
      lua_settable(Lua, -3);
   }

   lua_settable(Lua, -3); // Add match table to results

   return ERR::Terminate;
}

static ERR match_none(int Index, std::vector<std::string_view> Captures, std::string_view Prefix, std::string_view Suffix, APTR Meta)
{
   return ERR::Terminate;
}

//********************************************************************************************************************
// Constructor: regex.new(pattern [, flags])

static int regex_new(lua_State *Lua)
{
   pf::Log log(__FUNCTION__);

   if (load_regex() != ERR::Okay) {
      luaL_error(Lua, "Failed to load regex module");
      return 0;
   }

   const char *pattern = luaL_checkstring(Lua, 1);
   REGEX flags = REGEX(luaL_optint(Lua, 2, 0));

   log.trace("Creating regex with pattern: '%s', flags: %d", pattern, int(flags));

   if (auto r = (struct fregex *)lua_newuserdata(Lua, sizeof(struct fregex))) {
      new (r) fregex(pattern, flags);

      std::string error_msg;

      if (rx::Compile(pattern, flags, &error_msg, &r->regex_obj) != ERR::Okay) {
         luaL_error(Lua, "Regex compilation failed");
      }

      luaL_getmetatable(Lua, "Fluid.regex");
      lua_setmetatable(Lua, -2);
      return 1; // userdata is already on stack
   }
   else {
      luaL_error(Lua, "Failed to create regex object");
      return 0;
   }
}

//********************************************************************************************************************
// Method: regex.test(text) -> boolean

static int regex_test(lua_State *Lua)
{
   auto r = (struct fregex *)get_meta(Lua, lua_upvalueindex(1), "Fluid.regex");
   size_t text_len = 0;
   CSTRING text = luaL_checklstring(Lua, 1, &text_len);

   auto cb = C_FUNCTION(match_none, Lua);
   if (rx::Search(r->regex_obj, std::string_view(text, text_len), RMATCH::NIL, nullptr) IS ERR::Okay) {
      lua_pushboolean(Lua, true);
      return 1;
   }
   else {
      lua_pushboolean(Lua, false);
      return 1;
   }
}

//********************************************************************************************************************
// Method: regex.match(text) -> table|nil

static int regex_match(lua_State *Lua)
{
   auto r = (struct fregex *)get_meta(Lua, lua_upvalueindex(1), "Fluid.regex");
   size_t text_len = 0;
   CSTRING text = luaL_checklstring(Lua, 1, &text_len);

   auto cb = C_FUNCTION(match_one, Lua);
   if (rx::Match(r->regex_obj, std::string_view(text, text_len), RMATCH::NIL, &cb) IS ERR::Okay) {
      return 1;
   }
   else {
      lua_pushnil(Lua);
      return 1;
   }
}

//********************************************************************************************************************
// Method: regex.matchAll(text) -> table

static int regex_matchAll(lua_State *Lua)
{
   auto r = (struct fregex *)get_meta(Lua, lua_upvalueindex(1), "Fluid.regex");

   size_t text_len = 0;
   CSTRING text = luaL_checklstring(Lua, 1, &text_len);

   lua_createtable(Lua, 0, 0); // Result table

   auto cb = C_FUNCTION(match_many, Lua);
   if (rx::Match(r->regex_obj, std::string_view(text, text_len), RMATCH::NIL, &cb) IS ERR::Okay) {
      return 1;
   }
   else {
      lua_pushnil(Lua);
      return 1;
   }
}

//********************************************************************************************************************
// Method: regex.replace(text, replacement) -> string

static int regex_replace(lua_State *Lua)
{
   auto r = (struct fregex *)get_meta(Lua, lua_upvalueindex(1), "Fluid.regex");

   size_t text_len = 0, replace_len = 0;
   auto text = luaL_checklstring(Lua, 1, &text_len);
   auto replacement = luaL_checklstring(Lua, 2, &replace_len);

   std::string output;
   rx::Replace(r->regex_obj, std::string_view(text, text_len), std::string_view(replacement, replace_len), &output, RMATCH::NIL);

   lua_pushlstring(Lua, output.c_str(), output.length());
   return 1;
}

//********************************************************************************************************************
// Method: regex.split(text) -> table

static int regex_split(lua_State *Lua)
{/*
   auto r = (struct fregex *)get_meta(Lua, lua_upvalueindex(1), "Fluid.regex");

   const char *text = luaL_checkstring(Lua, 1);

   std::string text_str(text);
   lua_createtable(Lua, 0, 0); // Result table
   int part_index = 1;

   // Use sregex_token_iterator for splitting
   srell::sregex_token_iterator iter(text_str.begin(), text_str.end(), *r->regex_obj, -1);
   srell::sregex_token_iterator end;

   for (; iter != end; ++iter) {
      std::string token = iter->str();
      lua_pushinteger(Lua, part_index++);
      if (token.empty()) lua_pushstring(Lua, "");
      else lua_pushlstring(Lua, token.c_str(), token.length());
      lua_settable(Lua, -3);
   }
   */
   lua_pushnil(Lua); // TEMPORARY
   return 1;
}

//********************************************************************************************************************
// Property and method access: __index

constexpr auto HASH_pattern  = pf::strihash("pattern");
constexpr auto HASH_flags    = pf::strihash("flags");
constexpr auto HASH_error    = pf::strihash("error");
constexpr auto HASH_test     = pf::strihash("test");
constexpr auto HASH_match    = pf::strihash("match");
constexpr auto HASH_matchAll = pf::strihash("matchAll");
constexpr auto HASH_replace  = pf::strihash("replace");
constexpr auto HASH_split    = pf::strihash("split");

static int regex_get(lua_State *Lua)
{
   if (auto r = (struct fregex *)luaL_checkudata(Lua, 1, "Fluid.regex")) {
      if (auto field = luaL_checkstring(Lua, 2)) {
         const auto hash = pf::strihash(field);

         switch(hash) {
            case HASH_pattern: lua_pushstring(Lua, r->pattern.c_str()); return 1;
            case HASH_flags: lua_pushinteger(Lua, int(r->flags)); return 1;
            case HASH_error:
               if (r->error_msg.empty()) lua_pushnil(Lua);
               else lua_pushstring(Lua, r->error_msg.c_str());
               return 1;
            case HASH_test:
               lua_pushvalue(Lua, 1);
               lua_pushcclosure(Lua, regex_test, 1);
               return 1;
            case HASH_match:
               lua_pushvalue(Lua, 1);
               lua_pushcclosure(Lua, regex_match, 1);
               return 1;
            case HASH_matchAll:
               lua_pushvalue(Lua, 1);
               lua_pushcclosure(Lua, regex_matchAll, 1);
               return 1;
            case HASH_replace:
               lua_pushvalue(Lua, 1);
               lua_pushcclosure(Lua, regex_replace, 1);
               return 1;
            case HASH_split:
               lua_pushvalue(Lua, 1);
               lua_pushcclosure(Lua, regex_split, 1);
               return 1;
         }

         luaL_error(Lua, "Unknown regex property/method: %s", field);
      }
      else luaL_error(Lua, "No field reference provided");
   }
   else luaL_error(Lua, "Invalid caller, expected Fluid.regex");

   return 0;
}

//********************************************************************************************************************
// Garbage collection: __gc

static int regex_destruct(lua_State *Lua)
{
   auto r = (struct fregex *)luaL_checkudata(Lua, 1, "Fluid.regex");

   if (r and r->regex_obj) {
      FreeResource(r->regex_obj);
      r->regex_obj = nullptr;
   }

   return 0;
}

//********************************************************************************************************************
// String representation: __tostring

static int regex_tostring(lua_State *Lua)
{
   auto r = (struct fregex *)luaL_checkudata(Lua, 1, "Fluid.regex");
   if (r) {
      std::string desc = "regex(";
      desc += r->pattern;
      if (r->flags != REGEX::NIL) {
         desc += ", flags=" + std::to_string(uint32_t(r->flags));
      }
      desc += ")";

      lua_pushstring(Lua, desc.c_str());
   }
   else lua_pushstring(Lua, "[INVALID REGEX]");

   return 1;
}

//********************************************************************************************************************
// Register the regex interface

void register_regex_class(lua_State *Lua)
{
   static const struct luaL_Reg functions[] = {
      { "new", regex_new },
      { nullptr, nullptr }
   };

   static const struct luaL_Reg methods[] = {
      { "__index",    regex_get },
      { "__gc",       regex_destruct },
      { "__tostring", regex_tostring },
      { nullptr, nullptr }
   };

   pf::Log(__FUNCTION__).trace("Registering regex interface");

   // Create metatable
   luaL_newmetatable(Lua, "Fluid.regex");
   lua_pushstring(Lua, "Fluid.regex");
   lua_setfield(Lua, -2, "__name");
   lua_pushstring(Lua, "__index");
   lua_pushvalue(Lua, -2);  // Push the Fluid.regex metatable
   lua_settable(Lua, -3);   // metatable.__index = metatable
   luaL_openlib(Lua, nullptr, methods, 0);

   // Create regex module
   luaL_openlib(Lua, "regex", functions, 0);

   // Add flag constants to regex module.  These match the REGEX_* flags but making them available
   // in this way means that scritps don't need to include the regex module.

   lua_getglobal(Lua, "regex");
   if (lua_istable(Lua, -1)) {
      lua_pushinteger(Lua, int(REGEX::ICASE));
      lua_setfield(Lua, -2, "ICASE");

      lua_pushinteger(Lua, int(REGEX::MULTILINE));
      lua_setfield(Lua, -2, "MULTILINE");

      lua_pushinteger(Lua, int(REGEX::DOT_ALL));
      lua_setfield(Lua, -2, "DOT_ALL");

      lua_pushinteger(Lua, int(REGEX::EXTENDED));
      lua_setfield(Lua, -2, "EXTENDED");

      lua_pushinteger(Lua, int(REGEX::AWK));
      lua_setfield(Lua, -2, "AWK");

      lua_pushinteger(Lua, int(REGEX::GREP));
      lua_setfield(Lua, -2, "GREP");
   }

   lua_pop(Lua, 1); // Remove regex table from stack
}