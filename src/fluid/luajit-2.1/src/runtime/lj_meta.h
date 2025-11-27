/*
** Metamethod handling.
** Copyright (C) 2005-2022 Mike Pall. See Copyright Notice in luajit.h
*/

#pragma once

#include "lj_obj.h"

// Metamethod handling
LJ_FUNC void lj_meta_init(lua_State* L);
LJ_FUNC [[nodiscard]] cTValue* lj_meta_cache(GCtab* mt, MMS mm, GCstr* name);
LJ_FUNC [[nodiscard]] cTValue* lj_meta_lookup(lua_State* L, cTValue* o, MMS mm);
#if LJ_HASFFI
LJ_FUNC int lj_meta_tailcall(lua_State* L, cTValue* tv);
#endif

[[nodiscard]] inline cTValue* lj_meta_fastg(global_State* g, GCtab* mt, MMS mm) noexcept
{
   return (mt) IS nullptr ? nullptr : ((mt)->nomm & (1u << (mm))) ? nullptr :
      lj_meta_cache(mt, mm, mmname_str(g, mm));
}

[[nodiscard]] inline cTValue* lj_meta_fast(lua_State* L, GCtab* mt, MMS mm) noexcept
{
   return lj_meta_fastg(G(L), mt, mm);
}

// C helpers for some instructions, called from assembler VM.
LJ_FUNCA [[nodiscard]] cTValue* lj_meta_tget(lua_State* L, cTValue* o, cTValue* k);
LJ_FUNCA [[nodiscard]] TValue* lj_meta_tset(lua_State* L, cTValue* o, cTValue* k);
LJ_FUNCA [[nodiscard]] TValue* lj_meta_arith(lua_State* L, TValue* ra, cTValue* rb,
   cTValue* rc, BCREG op);
LJ_FUNCA [[nodiscard]] TValue* lj_meta_cat(lua_State* L, TValue* top, int left);
LJ_FUNCA [[nodiscard]] TValue* LJ_FASTCALL lj_meta_len(lua_State* L, cTValue* o);
LJ_FUNCA [[nodiscard]] TValue* lj_meta_equal(lua_State* L, GCobj* o1, GCobj* o2, int ne);
LJ_FUNCA [[nodiscard]] TValue* LJ_FASTCALL lj_meta_equal_cd(lua_State* L, BCIns ins);
LJ_FUNCA [[nodiscard]] TValue* lj_meta_comp(lua_State* L, cTValue* o1, cTValue* o2, int op);
LJ_FUNCA void lj_meta_istype(lua_State* L, BCREG ra, BCREG tp);
LJ_FUNCA void lj_meta_call(lua_State* L, TValue* func, TValue* top);
LJ_FUNCA void LJ_FASTCALL lj_meta_for(lua_State* L, TValue* o);

// Helper for __close metamethod during scope exit. Returns error code (0 = success).
LJ_FUNC int lj_meta_close(lua_State* L, TValue* o, TValue* err);
