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

#include "lua.h"
#include "lauxlib.h"

#include "lj_cmath.cpp"
#include "lj_assert.cpp"
#include "lj_gc.cpp"
#include "lj_err.cpp"
#include "lj_char.cpp"
#include "lj_bc.cpp"
#include "lj_obj.cpp"
#include "lj_buf.cpp"
#include "lj_str.cpp"
#include "lj_tab.cpp"
#include "lj_func.cpp"
#include "lj_udata.cpp"
#include "lj_meta.cpp"
#include "lj_debug.cpp"
#include "lj_prng.cpp"
#include "lj_state.cpp"
#include "lj_dispatch.cpp"
#include "lj_vmevent.cpp"
#include "lj_vmmath.cpp"
#include "lj_strscan.cpp"
#include "lj_strfmt.cpp"
#include "lj_strfmt_num.cpp"
#include "lj_serialize.cpp"
#include "lj_api.cpp"
#include "lj_profile.cpp"
#include "lj_lex.cpp"
#include "lj_parse.cpp"
#include "lj_bcread.cpp"
#include "lj_bcwrite.cpp"
#include "lj_load.cpp"
#include "lj_ctype.cpp"
#include "lj_cdata.cpp"
#include "lj_cconv.cpp"
#include "lj_ccall.cpp"
#include "lj_ccallback.cpp"
#include "lj_carith.cpp"
#include "lj_clib.cpp"
#include "lj_cparse.cpp"
#include "lj_lib.cpp"
#include "lj_ir.cpp"
#include "lj_opt_mem.cpp"
#include "lj_opt_fold.cpp"
#include "lj_opt_narrow.cpp"
#include "lj_opt_dce.cpp"
#include "lj_opt_loop.cpp"
#include "lj_opt_split.cpp"
#include "lj_opt_sink.cpp"
#include "lj_mcode.cpp"
#include "lj_snap.cpp"
#include "lj_record.cpp"
#include "lj_crecord.cpp"
#include "lj_ffrecord.cpp"
#include "lj_asm.cpp"
#include "lj_trace.cpp"
#include "lj_gdbjit.cpp"
#include "lj_alloc.cpp"

// PARASOL PATCHED IN
#include "lib_aux.cpp"
#include "lib_base.cpp"
#include "lib_math.cpp"
#include "lib_string.cpp"
#include "lib_table.cpp"
//#include "lib_io.cpp"
//#include "lib_os.cpp"
//#include "lib_package.cpp"
#include "lib_debug.cpp"
#include "lib_bit.cpp"
#include "lib_jit.cpp"
#include "lib_ffi.cpp"
#include "lib_buffer.cpp"
#include "lib_init.cpp"

