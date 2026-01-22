// Native Parasol object handling for LuaJIT.
// Copyright (C) 2026 Paul Manias

#pragma once

#include "lj_obj.h"
#include "lj_gc.h"
#include <parasol/main.h>

extern GCobject * lj_object_new(lua_State *, OBJECTID, OBJECTPTR, class objMetaClass *, uint8_t);
extern void lj_object_finalize(lua_State *L, GCobject *obj);
extern void LJ_FASTCALL lj_object_free(global_State *g, GCobject *obj);
extern int lj_object_pairs(lua_State *L);
extern int lj_object_ipairs(lua_State *L);

// Fast path bytecode handlers for BC_OGETS and BC_OSETS
extern "C" void lj_object_gets(lua_State *L, GCobject *Obj, GCstr *Key, TValue *Dest);
extern "C" void lj_object_sets(lua_State *L, GCobject *Obj, GCstr *Key, TValue *Val);
