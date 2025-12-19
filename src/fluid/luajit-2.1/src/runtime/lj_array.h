// Native array handling.
// Copyright (C) 2024 Parasol Framework. See Copyright Notice in parasol.h

#pragma once

#include "lj_obj.h"

extern GCarray * lj_array_new(lua_State *, uint32_t, AET, void *Data = nullptr, uint8_t Flags = 0, std::string_view StructName = {});
extern void LJ_FASTCALL lj_array_free(global_State *, GCarray *);
[[nodiscard]] extern void * lj_array_index_checked(lua_State *L, GCarray *, uint32_t idx);
[[nodiscard]] extern uint8_t lj_array_elemsize(AET);
extern void lj_array_copy(lua_State *, GCarray *, uint32_t dstidx, GCarray *, uint32_t srcidx, uint32_t count);
extern GCtab* lj_array_to_table(lua_State *, GCarray *);

//********************************************************************************************************************

inline void * lj_array_index(GCarray *Array, uint32_t Idx) {
   return (Array->data.get<int8_t>()) + (Idx * Array->elemsize);
}
