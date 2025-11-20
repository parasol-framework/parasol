// $Id: lauxlib.h,v 1.88.1.1 2007/12/27 13:02:25 roberto Exp $
// Auxiliary functions for building Lua libraries
// See Copyright Notice in lua.h

#pragma once

#include <cstddef>
#include <cstdio>
#include "lua.h"

// extra error code for `luaL_load'
constexpr int LUA_ERRFILE = (LUA_ERRERR+1);

struct luaL_Reg {
  const char *name;
  lua_CFunction func;
};

extern void (luaL_openlib) (lua_State *L, const char *libname, const luaL_Reg *l, int nup);
extern void (luaL_register) (lua_State *L, const char *libname, const luaL_Reg *l);
extern int (luaL_getmetafield) (lua_State *L, int obj, const char *e);
extern int (luaL_callmeta) (lua_State *L, int obj, const char *e);
extern int (luaL_typerror) (lua_State *L, int narg, const char *tname);
extern int (luaL_argerror) (lua_State *L, int numarg, const char *extramsg);
extern const char *(luaL_checklstring) (lua_State *L, int numArg, size_t *l);
extern const char *(luaL_optlstring) (lua_State *L, int numArg, const char *def, size_t *l);
extern lua_Number (luaL_checknumber) (lua_State *L, int numArg);
extern lua_Number (luaL_optnumber) (lua_State *L, int nArg, lua_Number def);
extern lua_Integer (luaL_checkinteger) (lua_State *L, int numArg);
extern lua_Integer (luaL_optinteger) (lua_State *L, int nArg, lua_Integer def);
extern void (luaL_checkstack) (lua_State *L, int sz, const char *msg);
extern void (luaL_checktype) (lua_State *L, int narg, int t);
extern void (luaL_checkany) (lua_State *L, int narg);
extern int   (luaL_newmetatable) (lua_State *L, const char *tname);
extern void *(luaL_checkudata) (lua_State *L, int ud, const char *tname);
extern void (luaL_where) (lua_State *L, int lvl);
extern int (luaL_error) (lua_State *L, const char *fmt, ...);
extern int (luaL_checkoption) (lua_State *L, int narg, const char *def, const char *const lst[]);

// pre-defined references
constexpr int LUA_NOREF = (-2);
constexpr int LUA_REFNIL = (-1);

extern int (luaL_ref) (lua_State *L, int t);
extern void (luaL_unref) (lua_State *L, int t, int ref);
extern int (luaL_loadfile) (lua_State *L, const char *filename);
extern int (luaL_loadbuffer) (lua_State *L, const char *buff, size_t sz, const char *name);
extern int (luaL_loadstring) (lua_State *L, const char *s);
extern lua_State *(luaL_newstate) (class objScript *);
extern const char *(luaL_gsub) (lua_State *L, const char *s, const char *p, const char *r);
extern const char *(luaL_findtable) (lua_State *L, int idx, const char *fname, int szhint);
// From Lua 5.2.
extern int (luaL_loadfilex) (lua_State *L, const char *filename, const char *mode);
extern int (luaL_loadbufferx) (lua_State *L, const char *buff, std::size_t sz, const char *name, const char *mode);
extern void luaL_traceback (lua_State *L, lua_State *L1, const char *msg, int level);
extern void (luaL_setfuncs) (lua_State *L, const luaL_Reg *l, int nup);
extern void (luaL_pushmodule) (lua_State *L, const char *modname, int sizehint);
extern void *(luaL_testudata) (lua_State *L, int ud, const char *tname);
extern void (luaL_setmetatable) (lua_State *L, const char *tname);

inline void luaL_argcheck(lua_State *L, bool Cond, int NumArg, const char *ExtraMsg) {
   if (not Cond) luaL_argerror(L, NumArg, ExtraMsg);
}

inline const char *luaL_checkstring(lua_State *L, int N) {
   return luaL_checklstring(L, N, nullptr);
}

inline const char *luaL_optstring(lua_State *L, int N, const char *D) {
   return luaL_optlstring(L, N, D, nullptr);
}

inline int luaL_checkint(lua_State *L, int N) {
   return int(luaL_checkinteger(L, N));
}

inline int luaL_optint(lua_State *L, int N, int D) {
   return int(luaL_optinteger(L, N, D));
}

inline long luaL_checklong(lua_State *L, int N) {
   return long(luaL_checkinteger(L, N));
}

inline long luaL_optlong(lua_State *L, int N, long D) {
   return long(luaL_optinteger(L, N, D));
}

inline const char *luaL_typename(lua_State *L, int I) {
   return lua_typename(L, lua_type(L, I));
}

inline int luaL_dofile(lua_State *L, const char *Fn) {
   return luaL_loadfile(L, Fn) or lua_pcall(L, 0, LUA_MULTRET, 0);
}

inline int luaL_dostring(lua_State *L, const char *S) {
   return luaL_loadstring(L, S) or lua_pcall(L, 0, LUA_MULTRET, 0);
}

inline void luaL_getmetatable(lua_State *L, const char *N) {
   lua_getfield(L, LUA_REGISTRYINDEX, N);
}

// C++ template functions must be outside extern "C"
template<typename F, typename T>
inline auto luaL_opt(lua_State *L, F Func, int N, T Default) {
   return lua_isnoneornil(L, N) ? Default : Func(L, N);
}

// From Lua 5.2.
template<std::size_t N>
inline void luaL_newlibtable(lua_State *L, const luaL_Reg (&Lib)[N]) {
   lua_createtable(L, 0, N - 1);
}

template<std::size_t N>
inline void luaL_newlib(lua_State *L, const luaL_Reg (&Lib)[N]) {
   luaL_newlibtable(L, Lib);
   luaL_setfuncs(L, Lib, 0);
}

struct luaL_Buffer {
  char *p;         /* current position in buffer */
  int lvl;  /* number of strings in the stack (level) */
  lua_State *L;
  char buffer[LUAL_BUFFERSIZE];
};

LUALIB_API void (luaL_buffinit) (lua_State *L, luaL_Buffer *B);
LUALIB_API char *(luaL_prepbuffer) (luaL_Buffer *B);

inline void luaL_addchar(luaL_Buffer *B, char C) {
   if (B->p >= (B->buffer + LUAL_BUFFERSIZE)) luaL_prepbuffer(B);
   *(B->p++) = C;
}

// compatibility only
inline void luaL_putchar(luaL_Buffer *B, char C) {
   luaL_addchar(B, C);
}

inline void luaL_addsize(luaL_Buffer *B, int N) {
   B->p += N;
}

extern void (luaL_addlstring) (luaL_Buffer *B, const char *s, std::size_t l);
extern void (luaL_addstring) (luaL_Buffer *B, const char *s);
extern void (luaL_addvalue) (luaL_Buffer *B);
extern void (luaL_pushresult) (luaL_Buffer *B);
