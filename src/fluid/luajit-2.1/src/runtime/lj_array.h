// Native array handling.
// Copyright (C) 2024 Parasol Framework. See Copyright Notice in parasol.h

#pragma once

#include "lj_obj.h"

// Array creation and destruction
LJ_FUNC GCarray* lj_array_new(lua_State* L, uint32_t len, uint8_t elemtype);
LJ_FUNC GCarray* lj_array_new_external(lua_State* L, void* data, uint32_t len,
                                        uint8_t elemtype, uint8_t flags);
LJ_FUNC void LJ_FASTCALL lj_array_free(global_State* g, GCarray* arr);

// Element access - returns pointer to element (caller handles type)
LJ_FUNC [[nodiscard]] void* lj_array_index(GCarray* arr, uint32_t idx);

// Bounds-checked element access - throws error on out-of-bounds
LJ_FUNC [[nodiscard]] void* lj_array_index_checked(lua_State* L, GCarray* arr, uint32_t idx);

// Element size lookup
LJ_FUNC [[nodiscard]] MSize lj_array_elemsize(uint8_t elemtype);

// Copy operations
LJ_FUNC void lj_array_copy(lua_State* L, GCarray* dst, uint32_t dstidx,
                           GCarray* src, uint32_t srcidx, uint32_t count);

// Conversion to table
LJ_FUNC GCtab* lj_array_to_table(lua_State* L, GCarray* arr);
