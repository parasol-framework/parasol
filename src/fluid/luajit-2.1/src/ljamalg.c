/*
** LuaJIT core and libraries amalgamation.
** Copyright (C) 2005-2022 Mike Pall. See Copyright Notice in luajit.h
*/

#define ljamalg_c
#define LUA_CORE

/* To get the mremap prototype. Must be defined before any system includes. */
#if defined(__linux__) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif

#ifndef WINVER
#define WINVER 0x0501
#endif

// Build configuration verification - ensure essential compiler flags were applied during compilation.
// These checks catch misconfiguration where the build system fails to apply required flags
// (e.g. when add_library is used instead of custom commands with explicit flags).

#ifndef LUAJIT_ENABLE_LUA52COMPAT
#error "LUAJIT_ENABLE_LUA52COMPAT not defined - build system failed to apply required flags"
#endif
#ifndef LUAJIT_DISABLE_FFI
#error "LUAJIT_DISABLE_FFI not defined - FFI would be enabled when it should be disabled"
#endif
#ifndef LUAJIT_DISABLE_BUFFER
#error "LUAJIT_DISABLE_BUFFER not defined - buffer API would be enabled when it should be disabled"
#endif
#ifndef LUAJIT_UNWIND_EXTERNAL
#error "LUAJIT_UNWIND_EXTERNAL not defined - unwind tables may not be generated correctly"
#endif
#ifndef LUA_ROOT
#error "LUA_ROOT not defined - Parasol installation path not set, critical for runtime"
#endif

#include "lua.h"
#include "lauxlib.h"

#include "lj_assert.c"
#include "lj_gc.c"
#include "lj_err.c"
#include "lj_char.c"
#include "lj_bc.c"
#include "lj_obj.c"
#include "lj_buf.c"
#include "lj_str.c"
#include "lj_tab.c"
#include "lj_func.c"
#include "lj_udata.c"
#include "lj_meta.c"
#include "lj_debug.c"
#include "lj_prng.c"
#include "lj_state.c"
#include "lj_dispatch.c"
#include "lj_vmevent.c"
#include "lj_vmmath.c"
#include "lj_strscan.c"
#include "lj_strfmt.c"
#include "lj_strfmt_num.c"
#include "lj_serialize.c"
#include "lj_api.c"
#include "lj_profile.c"
#include "lj_lex.c"
#include "lj_parse.c"
#include "lj_bcread.c"
#include "lj_bcwrite.c"
#include "lj_load.c"
#include "lj_ctype.c"
#include "lj_cdata.c"
#include "lj_cconv.c"
#include "lj_ccall.c"
#include "lj_ccallback.c"
#include "lj_carith.c"
#include "lj_clib.c"
#include "lj_cparse.c"
#include "lj_lib.c"
#include "lj_ir.c"
#include "lj_opt_mem.c"
#include "lj_opt_fold.c"
#include "lj_opt_narrow.c"
#include "lj_opt_dce.c"
#include "lj_opt_loop.c"
#include "lj_opt_split.c"
#include "lj_opt_sink.c"
#include "lj_mcode.c"
#include "lj_snap.c"
#include "lj_record.c"
#include "lj_crecord.c"
#include "lj_ffrecord.c"
#include "lj_asm.c"
#include "lj_trace.c"
#include "lj_gdbjit.c"
#include "lj_alloc.c"

// PARASOL PATCHED IN
#include "lib_aux.c"
#include "lib_base.c"
#include "lib_math.c"
#include "lib_string.c"
#include "lib_table.c"
#include "lib_debug.c"
#include "lib_bit.c"
#include "lib_jit.c"
#include "lib_init.c"

