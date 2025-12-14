// Array helper functions for assembler VM.
// Copyright (C) 2025 Paul Manias

#pragma once

#include "lj_obj.h"

// C helpers for array bytecodes, called from assembler VM.
// Returns TValue* result or nullptr if metamethod needs to be called.
extern "C" [[nodiscard]] cTValue* lj_arr_get(lua_State* L, cTValue* o, cTValue* k);
extern "C" [[nodiscard]] TValue* lj_arr_set(lua_State* L, cTValue* o, cTValue* k);

// Direct array access helpers (no metamethod support, used after type check passes)
extern "C" void lj_arr_getidx(lua_State* L, GCarray* arr, int32_t idx, TValue* result);
extern "C" void lj_arr_setidx(lua_State* L, GCarray* arr, int32_t idx, cTValue* val);
