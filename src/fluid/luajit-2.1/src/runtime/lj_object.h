// Native Parasol object handling for LuaJIT.
// Copyright (C) 2026 Paul Manias

#pragma once

#include "lj_obj.h"
#include "lj_gc.h"
#include "lj_ir.h"
#include "lj_bc.h"
#include <parasol/main.h>

extern GCobject * lj_object_new(lua_State *, OBJECTID, OBJECTPTR, class objMetaClass *, uint8_t);
extern void lj_object_finalize(lua_State *, GCobject *);
extern void lj_object_free(global_State *, GCobject *);
extern int lj_object_pairs(lua_State *);
extern int lj_object_ipairs(lua_State *);

// Fast path bytecode handlers for BC_OBGETF and BC_OBSETF with inline caching.
// ExtWord points to the extension word containing the IC slot index (0xFFFF = uncached).
extern "C" void bc_object_getfield(lua_State *, GCobject *, GCstr *, TValue *, BCIns *ExtWord);
extern "C" void bc_object_setfield(lua_State *, GCobject *, GCstr *, TValue *, BCIns *ExtWord);

extern "C" int ir_object_field_type(GCobject *Obj, GCstr *Key);
