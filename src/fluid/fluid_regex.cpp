/*********************************************************************************************************************

This source code implements compiled regex objects for Fluid, providing high-performance regular expression
capabilities through C++ std::regex.

Features:

- Compiled regex objects with automatic garbage collection
- Full capture group support
- Global and single match operations
- String replacement with backreferences
- Pattern-based string splitting
- Comprehensive error handling for invalid patterns

Examples:

  local reg = regex.new("\\d+", regex.ICASE)
  local matches = reg:match("Hello 123 World")
  local result = reg:replace("abc123def", "XXX")

*********************************************************************************************************************/

#define PRV_SCRIPT
#define PRV_FLUID
#define PRV_FLUID_MODULE
#include <parasol/main.h>
#include <parasol/modules/fluid.h>
#include <parasol/strings.hpp>
#include <regex>
#include <string>

extern "C" {
 #include "lauxlib.h"
 #include "lj_obj.h"
}

#include "hashes.h"
#include "defs.h"

// Regex flag constants
const int REGEX_ICASE     = 1;   // Case insensitive
const int REGEX_MULTILINE = 2;   // ^ and $ match line boundaries
const int REGEX_DOTALL    = 4;   // . matches newlines
const int REGEX_EXTENDED  = 8;   // Allow whitespace and comments

//********************************************************************************************************************
// Convert std::smatch results to Lua table

static int push_match_table(lua_State *Lua, const std::smatch &matches)
{
   if (matches.empty()) {
      lua_pushnil(Lua);
      return 1;
   }

   lua_createtable(Lua, (int)matches.size(), 0);

   for (size_t i = 0; i < matches.size(); ++i) {
      const auto &match = matches[i];

      if (match.matched) {
         lua_pushinteger(Lua, (lua_Integer)(i + 1));
         lua_pushlstring(Lua, match.str().c_str(), match.length());
         lua_settable(Lua, -3);
      }
   }

   return 1;
}

//********************************************************************************************************************
// Constructor: regex.new(pattern [, flags])

static int regex_new(lua_State *Lua)
{
   pf::Log log(__FUNCTION__);

   const char *pattern = luaL_checkstring(Lua, 1);
   int flags = luaL_optint(Lua, 2, 0);

   log.trace("Creating regex with pattern: '%s', flags: %d", pattern, flags);

   if (auto r = (struct fregex *)lua_newuserdata(Lua, sizeof(struct fregex))) {
      new (r) fregex(pattern, flags);

      auto reg_flags = std::regex_constants::ECMAScript; // Default syntax
      if (flags & REGEX_ICASE) reg_flags |= std::regex_constants::icase;
      if (flags & REGEX_EXTENDED) {
#ifdef __GNUC__
         // NB: GCC's std::regex implementation has known bugs when combining
         // icase and extended flags, causing crashes in some versions.
         log.warning("2025-08-25 REGEX_EXTENDED flag disabled on GCC builds due to std::regex implementation bugs.");
#else
         reg_flags |= std::regex_constants::extended;
#endif
      }

      // Simple pattern validation - check for basic regex syntax errors
      bool has_error = false;
      std::string error_msg;

      int paren_count   = 0;
      int bracket_count = 0;
      int brace_count   = 0;
      bool in_bracket   = false;

      for (const char *p = pattern; *p; p++) {
         switch (*p) {
            case '(':
               if (!in_bracket) paren_count++;
               break;
            case ')':
               if (!in_bracket) paren_count--;
               if (paren_count < 0) {
                  has_error = true;
                  error_msg = "Unmatched closing parenthesis";
               }
               break;
            case '[':
               if (!in_bracket) {
                  in_bracket = true;
                  bracket_count++;
               }
               break;
            case ']':
               if (in_bracket) {
                  in_bracket = false;
                  bracket_count--;
               }
               break;
            case '{':
               if (!in_bracket) brace_count++;
               break;
            case '}':
               if (!in_bracket) brace_count--;
               if (brace_count < 0) {
                  has_error = true;
                  error_msg = "Unmatched closing brace";
               }
               break;
         }
         if (has_error) break;
      }

      if (!has_error) {
         if (paren_count != 0) {
            has_error = true;
            error_msg = "Unmatched parentheses";
         }
         else if (in_bracket or bracket_count != 0) {
            has_error = true;
            error_msg = "Unmatched brackets";
         }
         else if (brace_count != 0) {
            has_error = true;
            error_msg = "Unmatched braces";
         }
      }

      if (has_error) {
         luaL_error(Lua, "Regex validation failed: %s", error_msg.c_str());
      }
      else {
         r->regex_obj = new (std::nothrow) std::regex(pattern, reg_flags);
         if (!r->regex_obj) luaL_error(Lua, "Regex compilation failed");
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
   const char *text = luaL_checkstring(Lua, 1);

   bool matches = std::regex_search(text, *r->regex_obj);
   lua_pushboolean(Lua, matches);
   return 1;
}

//********************************************************************************************************************
// Method: regex.match(text) -> table|nil

static int regex_match(lua_State *Lua)
{
   auto r = (struct fregex *)get_meta(Lua, lua_upvalueindex(1), "Fluid.regex");
   const char *text = luaL_checkstring(Lua, 1);

   std::smatch matches;
   std::string text_str(text);

   if (std::regex_search(text_str, matches, *r->regex_obj)) {
      return push_match_table(Lua, matches);
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

   const char *text = luaL_checkstring(Lua, 1);

   std::string text_str(text);
   lua_createtable(Lua, 0, 0); // Result table
   int match_index = 1;

   auto begin = std::sregex_iterator(text_str.begin(), text_str.end(), *r->regex_obj);
   auto end = std::sregex_iterator();

   for (std::sregex_iterator i = begin; i != end; ++i) {
      const std::smatch &match = *i;

      lua_pushinteger(Lua, match_index++);

      // Create match table for this result
      lua_createtable(Lua, (int)match.size(), 0);

      for (size_t j = 0; j < match.size(); ++j) {
         if (match[j].matched) {
            lua_pushinteger(Lua, (lua_Integer)(j + 1));
            lua_pushlstring(Lua, match[j].str().c_str(), match[j].length());
            lua_settable(Lua, -3);
         }
      }

      lua_settable(Lua, -3); // Add match table to results
   }

   return 1;
}

//********************************************************************************************************************
// Method: regex.replace(text, replacement) -> string

static int regex_replace(lua_State *Lua)
{
   auto r = (struct fregex *)get_meta(Lua, lua_upvalueindex(1), "Fluid.regex");

   const char *text = luaL_checkstring(Lua, 1);
   const char *replacement = luaL_checkstring(Lua, 2);

   std::string text_str(text);
   std::string replacement_str(replacement);

   // Replace first occurrence only
   std::smatch match;
   if (std::regex_search(text_str, match, *r->regex_obj)) {
      std::string result = match.prefix().str() +
                          std::regex_replace(match.str(), *r->regex_obj, replacement_str) +
                          match.suffix().str();

      lua_pushlstring(Lua, result.c_str(), result.length());
   }
   else {
      // No match, return original text
      lua_pushstring(Lua, text);
   }

   return 1;
}

//********************************************************************************************************************
// Method: regex.replaceAll(text, replacement) -> string

static int regex_replaceAll(lua_State *Lua)
{
   auto r = (struct fregex *)get_meta(Lua, lua_upvalueindex(1), "Fluid.regex");

   const char *text = luaL_checkstring(Lua, 1);
   const char *replacement = luaL_checkstring(Lua, 2);

   std::string text_str(text);
   std::string replacement_str(replacement);

   // Replace all occurrences
   std::string result = std::regex_replace(text_str, *r->regex_obj, replacement_str);

   lua_pushlstring(Lua, result.c_str(), result.length());
   return 1;
}

//********************************************************************************************************************
// Method: regex.split(text) -> table

static int regex_split(lua_State *Lua)
{
   auto r = (struct fregex *)get_meta(Lua, lua_upvalueindex(1), "Fluid.regex");

   const char *text = luaL_checkstring(Lua, 1);

   std::string text_str(text);
   lua_createtable(Lua, 0, 0); // Result table
   int part_index = 1;

   // Use sregex_token_iterator for splitting
   std::sregex_token_iterator iter(text_str.begin(), text_str.end(), *r->regex_obj, -1);
   std::sregex_token_iterator end;

   for (; iter != end; ++iter) {
      std::string token = iter->str();
      lua_pushinteger(Lua, part_index++);
      if (token.empty()) lua_pushstring(Lua, "");
      else lua_pushlstring(Lua, token.c_str(), token.length());
      lua_settable(Lua, -3);
   }

   return 1;
}

//********************************************************************************************************************
// Property and method access: __index

constexpr auto HASH_pattern     = pf::strihash("pattern");
constexpr auto HASH_flags       = pf::strihash("flags");
constexpr auto HASH_error       = pf::strihash("error");
constexpr auto HASH_test        = pf::strihash("test");
constexpr auto HASH_match       = pf::strihash("match");
constexpr auto HASH_matchAll    = pf::strihash("matchAll");
constexpr auto HASH_replace     = pf::strihash("replace");
constexpr auto HASH_replaceAll  = pf::strihash("replaceAll");
constexpr auto HASH_split       = pf::strihash("split");

static int regex_get(lua_State *Lua)
{
   if (auto r = (struct fregex *)luaL_checkudata(Lua, 1, "Fluid.regex")) {
      if (auto field = luaL_checkstring(Lua, 2)) {
         const auto hash = pf::strihash(field);

         switch(hash) {
            case HASH_pattern: lua_pushstring(Lua, r->pattern.c_str()); return 1;
            case HASH_flags: lua_pushinteger(Lua, r->flags); return 1;
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
            case HASH_replaceAll:
               lua_pushvalue(Lua, 1);
               lua_pushcclosure(Lua, regex_replaceAll, 1);
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
      delete r->regex_obj;
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
      if (r->flags) {
         desc += ", flags=";
         desc += std::to_string(r->flags);
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

   // Add flag constants to regex module
   lua_getglobal(Lua, "regex");
   if (lua_istable(Lua, -1)) {
      lua_pushinteger(Lua, REGEX_ICASE);
      lua_setfield(Lua, -2, "ICASE");

      lua_pushinteger(Lua, REGEX_MULTILINE);
      lua_setfield(Lua, -2, "MULTILINE");

      lua_pushinteger(Lua, REGEX_DOTALL);
      lua_setfield(Lua, -2, "DOTALL");

      lua_pushinteger(Lua, REGEX_EXTENDED);
      lua_setfield(Lua, -2, "EXTENDED");
   }
   lua_pop(Lua, 1); // Remove regex table from stack
}