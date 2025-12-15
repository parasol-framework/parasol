// Native array handling.
// Copyright (C) 2024 Parasol Framework. See Copyright Notice in parasol.h

#pragma once

#include "lj_obj.h"

extern GCarray * lj_array_new(lua_State *L, uint32_t len, AET elemtype);
extern GCarray * lj_array_new_external(lua_State *L, void *data, uint32_t len, AET elemtype, uint8_t flags);
extern void LJ_FASTCALL lj_array_free(global_State *g, GCarray *arr);
extern [[nodiscard]] void * lj_array_index(GCarray *arr, uint32_t idx);
extern [[nodiscard]] void * lj_array_index_checked(lua_State *L, GCarray *arr, uint32_t idx);
extern [[nodiscard]] MSize lj_array_elemsize(uint8_t elemtype);
extern void lj_array_copy(lua_State *L, GCarray *dst, uint32_t dstidx, GCarray *src, uint32_t srcidx, uint32_t count);
extern GCtab* lj_array_to_table(lua_State *L, GCarray* arr);
