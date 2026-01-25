// Native Parasol object handling for LuaJIT.
// Copyright (C) 2026 Paul Manias

#pragma once

#include "lj_obj.h"
#include "lj_gc.h"
#include "lj_ir.h"
#include <parasol/main.h>

extern GCobject * lj_object_new(lua_State *, OBJECTID, OBJECTPTR, class objMetaClass *, uint8_t);
extern void lj_object_finalize(lua_State *, GCobject *);
extern void LJ_FASTCALL lj_object_free(global_State *, GCobject *);
extern int lj_object_pairs(lua_State *);
extern int lj_object_ipairs(lua_State *);

// Fast path bytecode handlers for BC_OBGETF and BC_OBSETF
extern "C" void lj_object_getfield(lua_State *, GCobject *, GCstr *, TValue *);
extern "C" void lj_object_setfield(lua_State *, GCobject *, GCstr *, TValue *);

extern "C" int ir_object_field_type(GCobject *Obj, GCstr *Key);
