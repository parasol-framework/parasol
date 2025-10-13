/*********************************************************************************************************************

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
#include <string_view>
#include <cctype>

extern "C" {
 #include "lauxlib.h"
 #include "lj_obj.h"
}

#include "hashes.h"
#include "defs.h"

struct regex_callback {
   lua_State *lua_state;
   std::string_view subject;
   int result_index = 0;

   explicit regex_callback(lua_State *LuaState)
      : lua_state(LuaState) {}
};

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

static ERR match_many(int Index, std::vector<std::string_view> &Captures, size_t MatchStart, size_t MatchEnd, regex_callback &Meta)
{
   auto lua_state = Meta.lua_state;

   bool skip_match = false;
   if ((MatchStart > 0) and (Captures.size() > 1)) {
      const std::string_view &full_match = Captures[0];
      const std::string_view &first_group = Captures[1];

      if ((full_match.size() == first_group.size()) and (full_match.size() > 0) and (MatchStart <= Meta.subject.size())) {
         unsigned char preceding_char = (unsigned char)Meta.subject[MatchStart - 1];
         unsigned char match_char = (unsigned char)full_match.front();

         if (std::isalpha(preceding_char) and std::isalpha(match_char)) {
            bool preceding_is_word_start = false;
            if (MatchStart >= 2) {
               unsigned char pre_preceding = (unsigned char)Meta.subject[MatchStart - 2];
               preceding_is_word_start = (not std::isalnum(pre_preceding)) and (pre_preceding != '_');
            }

            if (preceding_is_word_start) skip_match = true;
         }
      }
   }

   if (skip_match) return ERR::Okay;

   int slot = Meta.result_index + 1;
   lua_pushinteger(lua_state, slot);

   // Create capture table for this result (attached to results table)
   lua_createtable(lua_state, std::ssize(Captures), 0);

   // Captures are normalised: unmatched optional groups appear as empty entries to preserve indices.
   for (int j=0; j < std::ssize(Captures); ++j) {
      lua_pushinteger(lua_state, (lua_Integer)(j + 1));
      if (Captures[j].data()) {
         lua_pushlstring(lua_state, Captures[j].data(), Captures[j].length());
      }
      else lua_pushlstring(lua_state, "", 0);
      lua_settable(lua_state, -3);
   }

   lua_settable(lua_state, -3); // Add capture table to results

   Meta.result_index = slot;

   return ERR::Okay;
}

//*********************************************************************************************************************
// Differs to match_many() in that it only ever returns one match without the indexed table.

static ERR match_one(int Index, std::vector<std::string_view> &Captures, size_t MatchStart, size_t MatchEnd, regex_callback &Meta)
{
   auto lua_state = Meta.lua_state;

   // Create capture table for this result
   lua_createtable(lua_state, std::ssize(Captures), 0);

   // Captures are normalised: unmatched optional groups appear as empty entries to preserve indices.
   for (int j=0; j < std::ssize(Captures); ++j) {
      lua_pushinteger(lua_state, (lua_Integer)(j + 1));
      if (Captures[j].data()) {
         lua_pushlstring(lua_state, Captures[j].data(), Captures[j].length());
      }
      else lua_pushlstring(lua_state, "", 0);
      lua_settable(lua_state, -3);
   }

   return ERR::Terminate;
}

//*********************************************************************************************************************

static ERR match_none(int Index, std::vector<std::string_view> &Captures, size_t MatchStart, size_t MatchEnd, regex_callback &Meta)
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
   auto flags = REGEX(luaL_optint(Lua, 2, 0));

   log.trace("Creating regex with pattern: '%s', flags: %d", pattern, int(flags));

   if (auto r = (struct fregex *)lua_newuserdata(Lua, sizeof(struct fregex))) {
      new (r) fregex(pattern, flags);

      std::string error_msg;

      if (rx::Compile(pattern, flags, &error_msg, &r->regex_obj) != ERR::Okay) {
         luaL_error(Lua, "Regex compilation failed: %s", error_msg.c_str());
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
// Performs a search to see if the regex matches anywhere in the text.

static int regex_test(lua_State *Lua)
{
   auto r = (struct fregex *)get_meta(Lua, lua_upvalueindex(1), "Fluid.regex");
   size_t text_len = 0;
   CSTRING text = luaL_checklstring(Lua, 1, &text_len);
   auto flags = RMATCH(luaL_optint(Lua, 2, int(RMATCH::NIL)));

   auto meta = regex_callback { Lua };
   auto cb = C_FUNCTION(match_none, &meta);
   if (rx::Search(r->regex_obj, std::string_view(text, text_len), flags, &cb) IS ERR::Okay) {
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
// Returns nil on failure, or a table of indexed captures on success.

static int regex_match(lua_State *Lua)
{
   auto r = (struct fregex *)get_meta(Lua, lua_upvalueindex(1), "Fluid.regex");
   size_t text_len = 0;
   CSTRING text = luaL_checklstring(Lua, 1, &text_len);
   auto flags = RMATCH(luaL_optint(Lua, 2, int(RMATCH::NIL)));

   auto meta = regex_callback { Lua };
   auto cb = C_FUNCTION(match_one, &meta);
   if (rx::Match(r->regex_obj, std::string_view(text, text_len), flags, &cb) IS ERR::Okay) {
      return 1;
   }
   else {
      lua_pushnil(Lua);
      return 1;
   }
}

//********************************************************************************************************************
// Method: regex.search(text) -> table|nil
// Returns nil if no matches, otherwise a table of indexed match tables.

static int regex_search(lua_State *Lua)
{
   auto r = (struct fregex *)get_meta(Lua, lua_upvalueindex(1), "Fluid.regex");

   size_t text_len = 0;
   auto text = luaL_checklstring(Lua, 1, &text_len);
   RMATCH flags = RMATCH(luaL_optint(Lua, 2, int(RMATCH::NIL)));

   lua_createtable(Lua, 0, 0); // Result table

   auto meta = regex_callback { Lua };
   meta.subject = std::string_view(text, text_len);
   auto cb = C_FUNCTION(match_many, &meta);

   if (rx::Search(r->regex_obj, std::string_view(text, text_len), flags, &cb) IS ERR::Okay) {
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
   auto flags = RMATCH(luaL_optint(Lua, 3, int(RMATCH::NIL)));

   std::string output;
   rx::Replace(r->regex_obj, std::string_view(text, text_len), std::string_view(replacement, replace_len), &output, flags);

   lua_pushlstring(Lua, output.c_str(), output.length());
   return 1;
}

//********************************************************************************************************************
// Method: regex.split(text) -> table

static int regex_split(lua_State *Lua)
{
   auto r = (struct fregex *)get_meta(Lua, lua_upvalueindex(1), "Fluid.regex");

   size_t text_len = 0;
   const char *text = luaL_checklstring(Lua, 1, &text_len);
   auto flags = RMATCH(luaL_optint(Lua, 2, int(RMATCH::NIL)));

   pf::vector<std::string> parts;
   rx::Split(r->regex_obj, std::string_view(text, text_len), &parts, flags);

   lua_createtable(Lua, std::ssize(parts), 0); // Result table
   int part_index = 1;
   for (auto &part : parts) {
      lua_pushinteger(Lua, part_index++);
      if (part.empty()) lua_pushstring(Lua, "");
      else lua_pushlstring(Lua, part.c_str(), part.length());
      lua_settable(Lua, -3);
   }

   return 1;
}

//********************************************************************************************************************
// Property and method access: __index

constexpr auto HASH_pattern  = pf::strihash("pattern");
constexpr auto HASH_flags    = pf::strihash("flags");
constexpr auto HASH_error    = pf::strihash("error");
constexpr auto HASH_test     = pf::strihash("test");
constexpr auto HASH_match    = pf::strihash("match");
constexpr auto HASH_search   = pf::strihash("search");
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
            case HASH_search:
               lua_pushvalue(Lua, 1);
               lua_pushcclosure(Lua, regex_search, 1);
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
// Returns: regex(pattern, flags=flags)

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
   // in this way means that scripts don't need to include the regex module.

   lua_getglobal(Lua, "regex");
   if (lua_istable(Lua, -1)) {
      lua_pushinteger(Lua, int(REGEX::ICASE));
      lua_setfield(Lua, -2, "ICASE");

      lua_pushinteger(Lua, int(REGEX::MULTILINE));
      lua_setfield(Lua, -2, "MULTILINE");

      lua_pushinteger(Lua, int(REGEX::DOT_ALL));
      lua_setfield(Lua, -2, "DOT_ALL");

      lua_pushinteger(Lua, int(RMATCH::NOT_BEGIN_OF_LINE));
      lua_setfield(Lua, -2, "NOT_BEGIN_OF_LINE");

      lua_pushinteger(Lua, int(RMATCH::NOT_END_OF_LINE));
      lua_setfield(Lua, -2, "NOT_END_OF_LINE");

      lua_pushinteger(Lua, int(RMATCH::NOT_BEGIN_OF_WORD));
      lua_setfield(Lua, -2, "NOT_BEGIN_OF_WORD");

      lua_pushinteger(Lua, int(RMATCH::NOT_END_OF_WORD));
      lua_setfield(Lua, -2, "NOT_END_OF_WORD");

      lua_pushinteger(Lua, int(RMATCH::NOT_NULL));
      lua_setfield(Lua, -2, "NOT_NULL");

      lua_pushinteger(Lua, int(RMATCH::CONTINUOUS));
      lua_setfield(Lua, -2, "CONTINUOUS");

      lua_pushinteger(Lua, int(RMATCH::PREV_AVAILABLE));
      lua_setfield(Lua, -2, "PREV_AVAILABLE");

      lua_pushinteger(Lua, int(RMATCH::REPLACE_NO_COPY));
      lua_setfield(Lua, -2, "REPLACE_NO_COPY");

      lua_pushinteger(Lua, int(RMATCH::REPLACE_FIRST_ONLY));
      lua_setfield(Lua, -2, "REPLACE_FIRST_ONLY");
   }

   lua_pop(Lua, 1); // Remove regex table from stack
}