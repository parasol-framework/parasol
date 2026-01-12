// Native Parasol object handling for LuaJIT.
// Copyright (C) 2026 Paul Manias

#pragma once

#include "lj_obj.h"
#include "lj_gc.h"
#include <parasol/main.h>

extern GCobject * lj_object_new(lua_State *, OBJECTID, OBJECTPTR, struct objMetaClass *, uint8_t);
extern void lj_object_finalize(lua_State *L, GCobject *obj);
extern void LJ_FASTCALL lj_object_free(global_State *g, GCobject *obj);

// Set the metatable for a GCobject. Allows objects to have custom metamethods for __index, __newindex, etc.

inline void lj_object_setmetatable(lua_State *L, GCobject *obj, GCtab *mt) {
   setgcref(obj->metatable, obj2gco(mt));
   lj_gc_objbarrier(L, obj, mt);
}
