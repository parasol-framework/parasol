/*
** Debug library.
** Copyright (C) 2005-2022 Mike Pall. See Copyright Notice in luajit.h
**
** Major portions taken verbatim or adapted from the Lua interpreter.
** Copyright (C) 1994-2008 Lua.org, PUC-Rio. See Copyright Notice in lua.h
*/

#define lib_debug_c
#define LUA_LIB

#include <parasol/strings.hpp>

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

#include "lj_obj.h"
#include "lj_gc.h"
#include "lj_err.h"
#include "lj_debug.h"
#include "lib.h"
#include "debug/error_guard.h"

#include <cctype>       // For isalnum, isdigit
#include <cstdlib>      // For strtod
#include <string_view>  // For std::string_view
#include <charconv>     // For std::from_chars

//********************************************************************************************************************

#define LJLIB_MODULE_debug

LJLIB_CF(debug_getregistry)
{
   copyTV(L, L->top++, registry(L));
   return 1;
}

//********************************************************************************************************************

LJLIB_CF(debug_getmetatable)   LJLIB_REC(.)
{
   lj_lib_checkany(L, 1);
   if (!lua_getmetatable(L, 1)) {
      setnilV(L->top - 1);
   }
   return 1;
}

//********************************************************************************************************************

LJLIB_CF(debug_setmetatable)
{
   lj_lib_checktabornil(L, 2);
   L->top = L->base + 2;
   lua_setmetatable(L, 1);
   return 1;
}

//********************************************************************************************************************

LJLIB_CF(debug_getfenv)
{
   lj_lib_checkany(L, 1);
   lua_getfenv(L, 1);
   return 1;
}

//********************************************************************************************************************

LJLIB_CF(debug_setfenv)
{
   lj_lib_checktab(L, 2);
   L->top = L->base + 2;
   if (!lua_setfenv(L, 1)) lj_err_caller(L, ErrMsg::SETFENV);
   return 1;
}

//********************************************************************************************************************

static void settabss(lua_State *L, CSTRING i, CSTRING v)
{
   lua_pushstring(L, v);
   lua_setfield(L, -2, i);
}

//********************************************************************************************************************

static void settabsi(lua_State *L, CSTRING i, int v)
{
   lua_pushinteger(L, v);
   lua_setfield(L, -2, i);
}

//********************************************************************************************************************

static void settabsb(lua_State *L, CSTRING i, int v)
{
   lua_pushboolean(L, v);
   lua_setfield(L, -2, i);
}

//********************************************************************************************************************
// Convert 0-based user index to 1-based internal slot number.
// Negative indices (varargs) pass through unchanged.

static int32_t debug_idx(int32_t Index)
{
   if (Index >= 0) return Index + 1;  // 0-based: slot 0 → internal 1
   return Index;  // Negative (varargs) unchanged
}

//********************************************************************************************************************

static lua_State* getthread(lua_State *L, int* arg)
{
   if (L->base < L->top and tvisthread(L->base)) {
      *arg = 1;
      return threadV(L->base);
   }
   else {
      *arg = 0;
      return L;
   }
}

//********************************************************************************************************************

static void treatstackoption(lua_State *L, lua_State *L1, CSTRING fname)
{
   if (L == L1) {
      lua_pushvalue(L, -2);
      lua_remove(L, -3);
   }
   else lua_xmove(L1, L, 1);
   lua_setfield(L, -2, fname);
}

//********************************************************************************************************************

LJLIB_CF(debug_getinfo)
{
   lj_Debug ar;
   int arg, opt_f = 0, opt_L = 0;
   lua_State *L1 = getthread(L, &arg);
   CSTRING options = luaL_optstring(L, arg + 2, "flnSu");
   if (lua_isnumber(L, arg + 1)) {
      if (!lua_getstack(L1, (int)lua_tointeger(L, arg + 1), (lua_Debug*)&ar)) {
         setnilV(L->top - 1);
         return 1;
      }
   }
   else if (L->base + arg < L->top and tvisfunc(L->base + arg)) {
      options = lua_pushfstring(L, ">%s", options);
      setfuncV(L1, L1->top++, funcV(L->base + arg));
   }
   else lj_err_arg(L, arg + 1, ErrMsg::NOFUNCL);

   if (!lj_debug_getinfo(L1, options, &ar, 1)) lj_err_arg(L, arg + 2, ErrMsg::INVOPT);

   lua_createtable(L, 0, 16);  //  Create result table.
   for (; *options; options++) {
      switch (*options) {
         case 'S':
            settabss(L, "source", ar.source);
            settabss(L, "shortSource", ar.short_src);
            settabsi(L, "lineDefined", ar.linedefined);
            settabsi(L, "lastLineDefined", ar.lastlinedefined);
            settabss(L, "what", ar.what);
            break;
         case 'l':
            settabsi(L, "currentLine", ar.currentline);
            break;
         case 'u':
            settabsi(L, "nups", ar.nups);
            settabsi(L, "nParams", ar.nparams);
            settabsb(L, "isVarArg", ar.isvararg);
            break;
         case 'n':
            settabss(L, "name", ar.name);
            settabss(L, "nameWhat", ar.namewhat);
            break;
         case 'f': opt_f = 1; break;
         case 'L': opt_L = 1; break;
         default: break;
      }
   }
   if (opt_L) treatstackoption(L, L1, "activeLines");
   if (opt_f) treatstackoption(L, L1, "func");
   return 1;  //  Return result table.
}

//********************************************************************************************************************

LJLIB_CF(debug_getlocal)
{
   int arg;
   lua_State *L1 = getthread(L, &arg);
   lua_Debug ar;

   int slot = debug_idx(lj_lib_checkint(L, arg + 2));
   if (tvisfunc(L->base + arg)) {
      L->top = L->base + arg + 1;
      lua_pushstring(L, lua_getlocal(L, nullptr, slot));
      return 1;
   }

   if (!lua_getstack(L1, lj_lib_checkint(L, arg + 1), &ar)) lj_err_arg(L, arg + 1, ErrMsg::LVLRNG);

   if (auto name = lua_getlocal(L1, &ar, slot)) {
      lua_xmove(L1, L, 1);
      lua_pushstring(L, name);
      lua_pushvalue(L, -2);
      return 2;
   }
   else {
      setnilV(L->top - 1);
      return 1;
   }
}

//********************************************************************************************************************

LJLIB_CF(debug_setlocal)
{
   int arg;
   lua_State *L1 = getthread(L, &arg);
   lua_Debug ar;
   TValue* tv;
   if (!lua_getstack(L1, lj_lib_checkint(L, arg + 1), &ar)) lj_err_arg(L, arg + 1, ErrMsg::LVLRNG);
   tv = lj_lib_checkany(L, arg + 3);
   copyTV(L1, L1->top++, tv);
   lua_pushstring(L, lua_setlocal(L1, &ar, debug_idx(lj_lib_checkint(L, arg + 2))));
   return 1;
}

//********************************************************************************************************************

static int debug_getupvalue(lua_State *L, int get)
{
   int32_t n = debug_idx(lj_lib_checkint(L, 2));
   lj_lib_checkfunc(L, 1);

   auto name = get ? lua_getupvalue(L, 1, n) : lua_setupvalue(L, 1, n);
   if (name) {
      lua_pushstring(L, name);
      if (!get) return 1;
      copyTV(L, L->top, L->top - 2);
      L->top++;
      return 2;
   }
   return 0;
}

//********************************************************************************************************************

LJLIB_CF(debug_getupvalue)
{
   return debug_getupvalue(L, 1);
}

LJLIB_CF(debug_setupvalue)
{
   lj_lib_checkany(L, 3);
   return debug_getupvalue(L, 0);
}

//********************************************************************************************************************

LJLIB_CF(debug_upvalueid)
{
   GCfunc* fn = lj_lib_checkfunc(L, 1);
   int32_t n = debug_idx(lj_lib_checkint(L, 2)) - 1;
   if ((uint32_t)n >= fn->l.nupvalues) lj_err_arg(L, 2, ErrMsg::IDXRNG);
   lua_pushlightuserdata(L, isluafunc(fn) ? (void*)gcref(fn->l.uvptr[n]) : (void*)&fn->c.upvalue[n]);
   return 1;
}

//********************************************************************************************************************

LJLIB_CF(debug_upvaluejoin)
{
   GCfunc * fn[2];
   GCRef * p[2];

   for (int i = 0; i < 2; i++) {
      int32_t n;
      fn[i] = lj_lib_checkfunc(L, 2 * i + 1);
      if (!isluafunc(fn[i])) lj_err_arg(L, 2 * i + 1, ErrMsg::NOLFUNC);
      n = debug_idx(lj_lib_checkint(L, 2 * i + 2)) - 1;
      if ((uint32_t)n >= fn[i]->l.nupvalues) lj_err_arg(L, 2 * i + 2, ErrMsg::IDXRNG);
      p[i] = &fn[i]->l.uvptr[n];
   }
   setgcrefr(*p[0], *p[1]);
   lj_gc_objbarrier(L, fn[0], gcref(*p[1]));
   return 0;
}

//********************************************************************************************************************

LJLIB_CF(debug_getuservalue)
{
   TValue* o = L->base;
   if (o < L->top and tvisudata(o)) settabV(L, o, tabref(udataV(o)->env));
   else setnilV(o);
   L->top = o + 1;
   return 1;
}

//********************************************************************************************************************

LJLIB_CF(debug_setuservalue)
{
   TValue* o = L->base;
   if (!(o < L->top and tvisudata(o))) lj_err_argt(L, 1, LUA_TUSERDATA);
   if (!(o + 1 < L->top and tvistab(o + 1))) lj_err_argt(L, 2, LUA_TTABLE);
   L->top = o + 2;
   lua_setfenv(L, 1);
   return 1;
}

//********************************************************************************************************************

#define KEY_HOOK   (U64x(80000000,00000000)|'h')

static void hookf(lua_State *L, lua_Debug* ar)
{
   static CSTRING const hooknames[] =
   { "call", "return", "line", "count", "tail return" };
   (L->top++)->u64 = KEY_HOOK;
   lua_rawget(L, LUA_REGISTRYINDEX);
   if (lua_isfunction(L, -1)) {
      lua_pushstring(L, hooknames[(int)ar->event]);
      if (ar->currentline >= 0) lua_pushinteger(L, ar->currentline);
      else lua_pushnil(L);
      lua_call(L, 2, 0);
   }
}

//********************************************************************************************************************

static int makemask(CSTRING smask, int count)
{
   int mask = 0;
   if (strchr(smask, 'c')) mask |= LUA_MASKCALL;
   if (strchr(smask, 'r')) mask |= LUA_MASKRET;
   if (strchr(smask, 'l')) mask |= LUA_MASKLINE;
   if (count > 0) mask |= LUA_MASKCOUNT;
   return mask;
}

//********************************************************************************************************************

static char* unmakemask(int mask, char* smask)
{
   int i = 0;
   if (mask & LUA_MASKCALL) smask[i++] = 'c';
   if (mask & LUA_MASKRET) smask[i++] = 'r';
   if (mask & LUA_MASKLINE) smask[i++] = 'l';
   smask[i] = '\0';
   return smask;
}

//********************************************************************************************************************

LJLIB_CF(debug_sethook)
{
   int arg, mask, count;
   lua_Hook func;
   (void)getthread(L, &arg);
   if (lua_isnoneornil(L, arg + 1)) {
      lua_settop(L, arg + 1);
      func = nullptr; mask = 0; count = 0;  //  turn off hooks
   }
   else {
      CSTRING smask = luaL_checkstring(L, arg + 2);
      luaL_checktype(L, arg + 1, LUA_TFUNCTION);
      count = luaL_optint(L, arg + 3, 0);
      func = hookf; mask = makemask(smask, count);
   }
   (L->top++)->u64 = KEY_HOOK;
   lua_pushvalue(L, arg + 1);
   lua_rawset(L, LUA_REGISTRYINDEX);
   lua_sethook(L, func, mask, count);
   return 0;
}

//********************************************************************************************************************

LJLIB_CF(debug_gethook)
{
   char buff[5];
   int mask = lua_gethookmask(L);
   lua_Hook hook = lua_gethook(L);
   if (hook != nullptr and hook != hookf) {  // external hook?
      lua_pushliteral(L, "external hook");
   }
   else {
      (L->top++)->u64 = KEY_HOOK;
      lua_rawget(L, LUA_REGISTRYINDEX);   //  get hook
   }
   lua_pushstring(L, unmakemask(mask, buff));
   lua_pushinteger(L, lua_gethookcount(L));
   return 3;
}

//********************************************************************************************************************

LJLIB_CF(debug_debug)
{
   for (;;) {
      char buffer[250];
      fputs("lua_debug> ", stderr);
      if (fgets(buffer, sizeof(buffer), stdin) == 0 ||
         strcmp(buffer, "cont\n") == 0)
         return 0;
      if (luaL_loadbuffer(L, buffer, strlen(buffer), "=(debug command)") ||
         lua_pcall(L, 0, 0, 0)) {
         CSTRING s = lua_tostring(L, -1);
         fputs(s ? s : "(error object is not a string)", stderr);
         fputs("\n", stderr);
      }
      lua_settop(L, 0);  //  remove eventual returns
   }
}

//********************************************************************************************************************

#define LEVELS1   12   //  size of the first part of the stack
#define LEVELS2   10   //  size of the second part of the stack

LJLIB_CF(debug_traceback)
{
   int arg;
   lua_State *L1 = getthread(L, &arg);
   CSTRING msg = lua_tostring(L, arg + 1);
   if (msg == nullptr and L->top > L->base + arg) L->top = L->base + arg + 1;
   else luaL_traceback(L, L1, msg, lj_lib_optint(L, arg + 2, (L == L1)));
   return 1;
}

//********************************************************************************************************************
// Returns the locality of a variable: "local", "upvalue", "global", or "nil".
// Usage: debug.locality(name [, level])
//   name  - the variable name as a string
//   level - optional stack level (default 1 = caller's frame)

LJLIB_CF(debug_locality)
{
   int arg;
   lua_State *L1 = getthread(L, &arg);

   // Check for nil or missing argument
   if (L->base + arg >= L->top or tvisnil(L->base + arg)) {
      lua_pushliteral(L, "nil");
      return 1;
   }

   CSTRING varname = luaL_checkstring(L, arg + 1);
   int level = luaL_optint(L, arg + 2, 1);

   // Get the stack frame at the specified level
   lua_Debug ar;
   if (!lua_getstack(L1, level, &ar)) {
      // Invalid level - check global table only
      lua_getglobal(L1, varname);
      if (lua_isnil(L1, -1)) {
         lua_pop(L1, 1);
         lua_pushliteral(L, "nil");
      }
      else {
         lua_pop(L1, 1);
         lua_pushliteral(L, "global");
      }
      return 1;
   }

   // Search local variables in the frame
   int slot = 1;
   CSTRING name;
   while ((name = lua_getlocal(L1, &ar, slot)) != nullptr) {
      lua_pop(L1, 1);  // Pop the value
      if (strcmp(name, varname) == 0) {
         lua_pushliteral(L, "local");
         return 1;
      }
      slot++;
   }

   // Get the function at this level to check upvalues
   if (lua_getinfo(L1, "f", &ar)) {
      int uv = 1;
      while ((name = lua_getupvalue(L1, -1, uv)) != nullptr) {
         lua_pop(L1, 1);  // Pop the value
         if (strcmp(name, varname) == 0) {
            lua_pop(L1, 1);  // Pop the function
            lua_pushliteral(L, "upvalue");
            return 1;
         }
         uv++;
      }
      lua_pop(L1, 1);  // Pop the function
   }

   // Check global table
   lua_getglobal(L1, varname);
   if (lua_isnil(L1, -1)) {
      lua_pop(L1, 1);
      lua_pushliteral(L, "nil");
   }
   else {
      lua_pop(L1, 1);
      lua_pushliteral(L, "global");
   }
   return 1;
}

//********************************************************************************************************************
// Annotation string parser helper - parses a single value
// Returns true on success (pushes value to stack), false on error

static bool parse_annotation_value(lua_State *L, std::string_view& sv)
{
   // Skip leading whitespace
   while (not sv.empty() and (sv.front() IS ' ' or sv.front() IS '\t' or sv.front() IS '\n')) {
      sv.remove_prefix(1);
   }

   if (sv.empty()) return false;

   // String literal (double or single quotes)
   if (sv.front() IS '"' or sv.front() IS '\'') {
      char quote = sv.front();
      sv.remove_prefix(1);

      size_t end_pos = 0;
      while (end_pos < sv.size() and sv[end_pos] != quote) {
         if (sv[end_pos] IS '\\' and end_pos + 1 < sv.size()) end_pos++;  // Skip escaped characters
         end_pos++;
      }
      if (end_pos >= sv.size()) return false;  // Unterminated string

      lua_pushlstring(L, sv.data(), end_pos);
      sv.remove_prefix(end_pos + 1);  // Skip content and closing quote
      return true;
   }

   // Boolean literals
   if (sv.starts_with("true") and (sv.size() IS 4 or (not std::isalnum(sv[4]) and sv[4] != '_'))) {
      lua_pushboolean(L, 1);
      sv.remove_prefix(4);
      return true;
   }
   if (sv.starts_with("false") and (sv.size() IS 5 or (not std::isalnum(sv[5]) and sv[5] != '_'))) {
      lua_pushboolean(L, 0);
      sv.remove_prefix(5);
      return true;
   }

   // Number literal
   if (std::isdigit(sv.front()) or (sv.front() IS '-' and sv.size() > 1 and std::isdigit(sv[1]))) {
      double num;
      auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), num);
      if (ec IS std::errc()) {
         lua_pushnumber(L, num);
         sv.remove_prefix(ptr - sv.data());
         return true;
      }
      return false;
   }

   // Array literal: [item, item, ...] or {item, item, ...}
   if (sv.front() IS '[' or sv.front() IS '{') {
      char close = (sv.front() IS '[') ? ']' : '}';
      sv.remove_prefix(1);
      lua_newtable(L);
      int idx = 0;

      while (not sv.empty() and sv.front() != close) {
         // Skip whitespace
         while (not sv.empty() and (sv.front() IS ' ' or sv.front() IS '\t' or sv.front() IS '\n')) {
            sv.remove_prefix(1);
         }
         if (not sv.empty() and sv.front() IS close) break;

         // Parse array element
         if (not parse_annotation_value(L, sv)) {
            lua_pop(L, 1);  // Pop table
            return false;
         }
         lua_rawseti(L, -2, idx++);

         // Skip whitespace and comma
         while (not sv.empty() and (sv.front() IS ' ' or sv.front() IS '\t' or sv.front() IS '\n')) {
            sv.remove_prefix(1);
         }
         if (not sv.empty() and sv.front() IS ',') sv.remove_prefix(1);
      }

      if (not sv.empty() and sv.front() IS close) sv.remove_prefix(1);
      return true;
   }

   return false;
}

//********************************************************************************************************************
// Parses annotation string syntax like: @Test(name="foo", labels=["a","b"]); @Requires(network=true)
// Returns true on success (pushes annotations array to stack), false on parse error

static bool lj_parse_annotation_string(lua_State *L, std::string_view sv)
{
   lua_newtable(L);  // Result array
   int anno_idx = 0;

   while (not sv.empty()) {
      // Skip whitespace and semicolons
      while (not sv.empty() and (sv.front() IS ' ' or sv.front() IS '\t' or sv.front() IS '\n' or sv.front() IS ';')) {
         sv.remove_prefix(1);
      }
      if (sv.empty()) break;

      // Expect @

      if (sv.front() != '@') {
         lua_pop(L, 1);
         return false;
      }
      sv.remove_prefix(1);

      // Parse annotation name (identifier)

      size_t name_len = 0;
      while (name_len < sv.size() and (std::isalnum(sv[name_len]) or sv[name_len] IS '_')) {
         name_len++;
      }

      if (name_len IS 0) {
         lua_pop(L, 1);
         return false;
      }

      lua_newtable(L);  // Annotation entry
      lua_pushlstring(L, sv.data(), name_len);
      lua_setfield(L, -2, "name");
      sv.remove_prefix(name_len);

      // Parse optional arguments
      while (not sv.empty() and (sv.front() IS ' ' or sv.front() IS '\t')) {
         sv.remove_prefix(1);
      }

      if (not sv.empty() and sv.front() IS '(') {
         sv.remove_prefix(1);
         lua_newtable(L);  // Args table

         while (not sv.empty() and sv.front() != ')') {
            // Skip whitespace
            while (not sv.empty() and (sv.front() IS ' ' or sv.front() IS '\t' or sv.front() IS '\n')) {
               sv.remove_prefix(1);
            }
            if (not sv.empty() and sv.front() IS ')') break;

            // Parse key or bare identifier
            size_t key_len = 0;
            while (key_len < sv.size() and (std::isalnum(sv[key_len]) or sv[key_len] IS '_')) {
               key_len++;
            }

            if (key_len IS 0) {
               lua_pop(L, 3);  // Pop args, entry, result
               return false;
            }

            std::string_view key = sv.substr(0, key_len);
            sv.remove_prefix(key_len);

            while (not sv.empty() and (sv.front() IS ' ' or sv.front() IS '\t')) {
               sv.remove_prefix(1);
            }

            if (not sv.empty() and sv.front() IS '=') {
               // key=value pair
               sv.remove_prefix(1);
               while (not sv.empty() and (sv.front() IS ' ' or sv.front() IS '\t')) {
                  sv.remove_prefix(1);
               }

               // Parse value (string, number, bool, array)
               if (not parse_annotation_value(L, sv)) {
                  lua_pop(L, 3);  // Pop args, entry, result
                  return false;
               }
               // Use lua_pushlstring to create key, then lua_rawset
               lua_pushlstring(L, key.data(), key.size());  // Push key
               lua_pushvalue(L, -2);  // Copy value
               lua_rawset(L, -4);     // args[key] = value
               lua_pop(L, 1);         // Pop original value
            }
            else {
               // Bare identifier = true
               lua_pushlstring(L, key.data(), key.size());  // Push key
               lua_pushboolean(L, 1);  // Push value
               lua_rawset(L, -3);      // args[key] = true
            }

            // Skip comma
            while (not sv.empty() and (sv.front() IS ' ' or sv.front() IS '\t')) {
               sv.remove_prefix(1);
            }
            if (not sv.empty() and sv.front() IS ',') sv.remove_prefix(1);
         }

         if (not sv.empty() and sv.front() IS ')') sv.remove_prefix(1);
         lua_setfield(L, -2, "args");
      }
      else {
         // No args - set empty args table
         lua_newtable(L);
         lua_setfield(L, -2, "args");
      }

      lua_rawseti(L, -2, anno_idx++);
   }

   return true;
}

//********************************************************************************************************************

#define LJLIB_MODULE_debug_anno

//********************************************************************************************************************
// debug.anno.get(func) - Returns annotation entry for a function, or nil

LJLIB_CF(debug_anno_get)
{
   lj_lib_checkfunc(L, 1);

   // Get _ANNO global
   lua_getglobal(L, "_ANNO");
   if (lua_isnil(L, -1)) {
      lua_pop(L, 1);
      lua_pushnil(L);
      return 1;
   }

   // Look up annotations for this function: _ANNO[func]
   lua_pushvalue(L, 1);  // Push function reference
   lua_gettable(L, -2);  // Get _ANNO[func]
   lua_remove(L, -2);    // Remove _ANNO table
   return 1;
}

//********************************************************************************************************************
// debug.anno.set(func, annotations [, source [, name]]) - Sets annotations for a function
// annotations can be a table or a string to parse
// source is optional and defaults to "<runtime>"
// name is optional function name (falls back to debug info, then "<anonymous>")

LJLIB_CF(debug_anno_set)
{
   lj_lib_checkfunc(L, 1);
   lj_lib_checkany(L, 2);

   auto source = luaL_optstring(L, 3, "<runtime>");
   auto name = luaL_optstring(L, 4, nullptr);

   // Get or create _ANNO global

   lua_getglobal(L, "_ANNO");
   if (lua_isnil(L, -1)) {
      lua_pop(L, 1);
      lua_newtable(L);
      lua_pushvalue(L, -1);
      lua_setglobal(L, "_ANNO");
   }

   // Handle string input - parse annotation syntax

   if (lua_isstring(L, 2)) {
      CSTRING str = lua_tostring(L, 2);
      if (not lj_parse_annotation_string(L, str)) {
         lua_pop(L, 1);  // Pop _ANNO
         return luaL_error(L, "Failed to parse annotation string");
      }
      // Parsed annotations array is now on stack
   }
   else if (lua_istable(L, 2)) {
      lua_pushvalue(L, 2);  // Push annotations table/array
   }
   else {
      lua_pop(L, 1);  // Pop _ANNO
      lj_err_argt(L, 2, LUA_TTABLE);
   }

   // Create entry table with name, source, and annotations

   lua_newtable(L);  // Entry table

   // Get function name: use provided name, fall back to debug info, then "<anonymous>"

   if (name) lua_pushstring(L, name);
   else {
      lua_Debug ar;
      lua_pushvalue(L, 1);
      if (lua_getinfo(L, ">n", &ar) and ar.name) lua_pushstring(L, ar.name);
      else lua_pushliteral(L, "<anonymous>");
   }
   lua_setfield(L, -2, "name");

   // Set source
   lua_pushstring(L, source);
   lua_setfield(L, -2, "source");

   // Set annotations array
   lua_pushvalue(L, -2);  // Push annotations array
   lua_setfield(L, -2, "annotations");
   lua_remove(L, -2);  // Remove standalone annotations array

   // _ANNO[func] = entry
   lua_pushvalue(L, 1);   // Push function reference as key
   lua_pushvalue(L, -2);  // Push entry table as value
   lua_settable(L, -4);   // _ANNO[func] = entry

   lua_remove(L, -2);  // Remove _ANNO table
   return 1;  // Return the entry table
}

//********************************************************************************************************************
// debug.anno.list() - Returns shallow copy of entire _ANNO table

LJLIB_CF(debug_anno_list)
{
   lua_getglobal(L, "_ANNO");
   if (lua_isnil(L, -1)) {
      lua_pop(L, 1);
      lua_newtable(L);  // Return empty table if _ANNO doesn't exist
   }
   else {
      // Create shallow copy
      lua_newtable(L);
      lua_pushnil(L);
      while (lua_next(L, -3) != 0) {
         lua_pushvalue(L, -2);  // Copy key
         lua_pushvalue(L, -2);  // Copy value
         lua_settable(L, -5);   // Set in new table
         lua_pop(L, 1);         // Pop value, keep key for next iteration
      }
      lua_remove(L, -2);  // Remove original _ANNO
   }
   return 1;
}

//********************************************************************************************************************

#include "lj_libdef.h"  // Includes LJLIB_MODULE_debug table

extern int luaopen_debug(lua_State *L)
{
   LJ_LIB_REG(L, LUA_DBLIBNAME, debug);

   // Register debug.anno as a subtable of debug
   lua_getglobal(L, LUA_DBLIBNAME);  // Get the debug table we just created
   LJ_LIB_REG(L, nullptr, debug_anno);  // Create anno table without setting in globals
   lua_setfield(L, -2, "anno");      // debug.anno = anno_table
   lua_pop(L, 1);                     // Pop debug table

   return 1;
}
