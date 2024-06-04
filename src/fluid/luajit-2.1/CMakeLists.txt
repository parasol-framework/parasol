# Copyright (C) 2007-2011 LuaDist.  Created by Peter Drahoš
# Redistribution and use of this file is allowed according to the terms of the MIT license.
#
# Currently disabled.  For this build file to produce correct code on Windows platforms, it needs to perform the equivalent of the following:
#
# cl /nologo /c /O2 /W3 /D_CRT_SECURE_NO_DEPRECATE /D_CRT_STDIO_INLINE=__declspec(dllexport)__inline host\minilua.c
# link /nologo /out:minilua.exe minilua.obj
# if exist minilua.exe.manifest mt /nologo -manifest minilua.exe.manifest -outputresource:minilua.exe
# minilua ..\dynasm\dynasm.lua -LN -D WIN -D JIT -D FFI -D P64 -o host\buildvm_arch.h vm_x64.dasc
# cl /nologo /c /O2 /W3 /D_CRT_SECURE_NO_DEPRECATE /D_CRT_STDIO_INLINE=__declspec(dllexport)__inline /I "." /I ..\dynasm host\buildvm*.c
# link /nologo /out:buildvm.exe buildvm*.obj
# if exist buildvm.exe.manifest mt /nologo -manifest buildvm.exe.manifest -outputresource:buildvm.exe
# buildvm -m peobj -o lj_vm.obj
# buildvm -m bcdef -o lj_bcdef.h lib_base.c lib_math.c lib_bit.c lib_string.c lib_table.c lib_io.c lib_os.c lib_package.c lib_debug.c lib_jit.c lib_ffi.c lib_buffer.c
# buildvm -m ffdef -o lj_ffdef.h lib_base.c lib_math.c lib_bit.c lib_string.c lib_table.c lib_io.c lib_os.c lib_package.c lib_debug.c lib_jit.c lib_ffi.c lib_buffer.c
# buildvm -m libdef -o lj_libdef.h lib_base.c lib_math.c lib_bit.c lib_string.c lib_table.c lib_io.c lib_os.c lib_package.c lib_debug.c lib_jit.c lib_ffi.c lib_buffer.c
# buildvm -m recdef -o lj_recdef.h lib_base.c lib_math.c lib_bit.c lib_string.c lib_table.c lib_io.c lib_os.c lib_package.c lib_debug.c lib_jit.c lib_ffi.c lib_buffer.c
# buildvm -m vmdef -o jit\vmdef.lua lib_base.c lib_math.c lib_bit.c lib_string.c lib_table.c lib_io.c lib_os.c lib_package.c lib_debug.c lib_jit.c lib_ffi.c lib_buffer.c
# buildvm -m folddef -o lj_folddef.h lj_opt_fold.c
# cl /nologo /c /O2 /W3 /D_CRT_SECURE_NO_DEPRECATE /D_CRT_STDIO_INLINE=__declspec(dllexport)__inline lj_*.c lib_*.c
# lib /nologo /nodefaultlib /OUT:lua51.lib lj_*.obj lib_*.obj

project(LuaJIT C ASM)

if (APPLE)
  set (CMAKE_MACOSX_RPATH TRUE) # @rpath in libs
endif ()

set (CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake" "${CMAKE_MODULE_PATH}")

if (MSVC)
   # Turn off ugly warnings
   add_compile_definitions("_CRT_SECURE_NO_WARNINGS" "_CRT_STDIO_INLINE=__declspec(dllexport)__inline")
endif ()

include (CheckLibraryExists)
include (CheckFunctionExists)
include (CheckCSourceCompiles)
include (CheckTypeSize)

add_compile_definitions("LUAJIT_ENABLE_LUA52COMPAT")
add_compile_definitions("LUAJIT_DISABLE_FFI")

if (LUAJIT_DISABLE_JIT)
   add_compile_definitions("LUAJIT_DISABLE_JIT")
endif ()

if (LUAJIT_CPU_SSE2)
   add_compile_definitions("LUAJIT_CPU_SSE2")
endif ()

if (LUAJIT_CPU_NOCMOV)
   add_compile_definitions("LUAJIT_CPU_NOCMOV")
endif ()

if (LUAJIT_USE_SYSMALLOC)
   add_compile_definitions("LUAJIT_USE_SYSMALLOC")
endif ()

if (LUAJIT_USE_GDBJIT)
   add_compile_definitions("LUAJIT_USE_GDBJIT")
endif ()

if (LUA_USE_ASSERT)
   add_compile_definitions("LUA_USE_ASSERT")
endif ()

check_type_size ("void*" SIZEOF_VOID_P)
if (SIZEOF_VOID_P EQUAL 8)
   add_compile_definitions("_FILE_OFFSET_BITS=64" "_LARGEFILE_SOURCE")
endif ()

if ( WIN32 AND NOT CYGWIN )
  add_compile_definitions ("LUAJIT_OS=LUAJIT_OS_WINDOWS")
  add_compile_options("-malign-double")
  set ( LJVM_MODE coffasm )
elseif ( APPLE )
  set ( CMAKE_EXE_LINKER_FLAGS "-pagezero_size 10000 -image_base 100000000 ${CMAKE_EXE_LINKER_FLAGS}" )
  set ( LJVM_MODE machasm )
else ()
  set ( LJVM_MODE elfasm )
endif ()

MACRO(LJ_TEST_ARCH stuff)
  CHECK_C_SOURCE_COMPILES("
#undef ${stuff}
#include \"lj_arch.h\"
#if ${stuff}
int main() { return 0; }
#else
#error \"not defined\"
#endif
" ${stuff})
ENDMACRO()

MACRO(LJ_TEST_ARCH_VALUE stuff value)
  CHECK_C_SOURCE_COMPILES("
#undef ${stuff}
#include \"lj_arch.h\"
#if ${stuff} == ${value}
int main() { return 0; }
#else
#error \"not defined\"
#endif
" ${stuff}_${value})
ENDMACRO()

set (CMAKE_REQUIRED_INCLUDES ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/src)

foreach(arch X64 X86 ARM PPC PPCSPE MIPS)
  LJ_TEST_ARCH(LJ_TARGET_${arch})
  if(LJ_TARGET_${arch})
    string(TOLOWER ${arch} TARGET_LJARCH)
    message(STATUS "LuaJIT Target: ${TARGET_LJARCH}")
    BREAK()
  endif ()
endforeach()

if (NOT TARGET_LJARCH)
  message(FATAL_ERROR "architecture not supported")
else ()
  message(STATUS "LuaJIT target ${TARGET_LJARCH}")
endif ()

set (DASM_ARCH ${TARGET_LJARCH})
set (DASM_FLAGS)
set (TARGET_ARCH)

list (APPEND TARGET_ARCH "LUAJIT_TARGET=LUAJIT_ARCH_${TARGET_LJARCH}")

LJ_TEST_ARCH_VALUE(LJ_ARCH_BITS 64)
if (LJ_ARCH_BITS_64)
  set (DASM_FLAGS ${DASM_FLAGS} -D P64)
endif ()

LJ_TEST_ARCH_VALUE(LJ_HASJIT 1)
if (LJ_HASJIT_1)
  set (DASM_FLAGS ${DASM_FLAGS} -D JIT)
endif ()

LJ_TEST_ARCH_VALUE(LJ_HASFFI 1)
if (LJ_HASFFI_1)
  set (DASM_FLAGS ${DASM_FLAGS} -D FFI)
endif ()

LJ_TEST_ARCH_VALUE(LJ_DUALNUM 1)
if (LJ_DUALNUM_1)
  set (DASM_FLAGS ${DASM_FLAGS} -D DUALNUM)
endif ()

LJ_TEST_ARCH_VALUE(LJ_ARCH_HASFPU 1)
if (LJ_ARCH_HASFPU_1)
  set (DASM_FLAGS ${DASM_FLAGS} -D FPU)
  list (APPEND TARGET_ARCH "LJ_ARCH_HASFPU=1")
else ()
  list (APPEND TARGET_ARCH "LJ_ARCH_HASFPU=0")
endif ()

LJ_TEST_ARCH_VALUE(LJ_ABI_SOFTFP 1)
if (NOT LJ_ABI_SOFTFP_1)
  set (DASM_FLAGS ${DASM_FLAGS} -D HFABI)
  list (APPEND TARGET_ARCH "LJ_ABI_SOFTFP=0")
else ()
  list (APPEND TARGET_ARCH "LJ_ABI_SOFTFP=1")
endif ()

if (WIN32)
  set (DASM_FLAGS ${DASM_FLAGS} -LN -D WIN)
endif ()

if (TARGET_LJARCH STREQUAL "x86")
  LJ_TEST_ARCH_VALUE(__SSE2__ 1)
  if (__SSE2__1)
    set (DASM_FLAGS ${DASM_FLAGS} -D SSE)
  endif ()
endif ()

if (TARGET_LJARCH STREQUAL "x64")
  set (DASM_ARCH "x86")
endif ()

if (TARGET_LJARCH STREQUAL "ppc")
  LJ_TEST_ARCH_VALUE(LJ_ARCH_SQRT 1)
  if (NOT LJ_ARCH_SQRT_1)
    set (DASM_FLAGS ${DASM_FLAGS} -D SQRT)
  endif ()
  LJ_TEST_ARCH_VALUE(LJ_ARCH_PPC64 1)
  if (NOT LJ_ARCH_PPC64_1)
    set (DASM_FLAGS ${DASM_FLAGS} -D GPR64)
  endif ()
endif ()

add_executable (minilua src/host/minilua.c)
set_target_properties (minilua PROPERTIES COMPILE_DEFINITIONS "${TARGET_ARCH}")
check_library_exists (m sin "" MINILUA_USE_LIBM)
if (MINILUA_USE_LIBM)
  target_link_libraries (minilua m)
endif ()

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/buildvm_arch.h
  COMMAND minilua ${CMAKE_CURRENT_SOURCE_DIR}/dynasm/dynasm.lua ${DASM_FLAGS} -o ${CMAKE_CURRENT_BINARY_DIR}/buildvm_arch.h ${CMAKE_CURRENT_SOURCE_DIR}/src/vm_${DASM_ARCH}.dasc
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/dynasm/dynasm.lua minilua
)

set (SRC_LJLIB
   src/lib_base.c src/lib_buffer.c src/lib_math.c src/lib_bit.c src/lib_string.c src/lib_table.c
   src/lib_debug.c src/lib_jit.c src/lib_ffi.c)

set (SRC_LJCORE
   src/lj_gc.c src/lj_err.c src/lj_char.c src/lj_bc.c src/lj_obj.c
   src/lj_str.c src/lj_tab.c src/lj_func.c src/lj_udata.c src/lj_meta.c src/lj_debug.c
   src/lj_state.c src/lj_dispatch.c src/lj_vmevent.c src/lj_vmmath.c src/lj_strscan.c
   src/lj_api.c src/lj_lex.c src/lj_parse.c src/lj_bcread.c src/lj_bcwrite.c src/lj_load.c
   src/lj_ir.c src/lj_opt_mem.c src/lj_opt_fold.c src/lj_opt_narrow.c
   src/lj_opt_dce.c src/lj_opt_loop.c src/lj_opt_split.c src/lj_opt_sink.c
   src/lj_mcode.c src/lj_snap.c src/lj_record.c src/lj_crecord.c src/lj_ffrecord.c
   src/lj_asm.c src/lj_trace.c src/lj_gdbjit.c
   src/lj_ctype.c src/lj_cdata.c src/lj_cconv.c src/lj_ccall.c src/lj_ccallback.c
   src/lj_carith.c src/lj_clib.c src/lj_cparse.c
   src/lj_lib.c src/lj_alloc.c src/lib_aux.c
   ${SRC_LJLIB} src/lib_init.c)

set (SRC_BUILDVM src/host/buildvm.c src/host/buildvm_asm.c
src/host/buildvm_peobj.c src/host/buildvm_lib.c src/host/buildvm_fold.c
${CMAKE_CURRENT_BINARY_DIR}/buildvm_arch.h)

## GENERATE
add_executable (buildvm ${SRC_BUILDVM})
set_target_properties (buildvm PROPERTIES COMPILE_DEFINITIONS "${TARGET_ARCH}")

macro(add_buildvm_target _target _mode)
  add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${_target}
    COMMAND buildvm ARGS -m ${_mode} -o ${CMAKE_CURRENT_BINARY_DIR}/${_target} ${ARGN}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    DEPENDS buildvm ${ARGN}
  )
endmacro(add_buildvm_target)

if (MSVC)
   add_buildvm_target ( lj_vm.obj peobj )
   set (LJ_VM_SRC ${CMAKE_CURRENT_BINARY_DIR}/lj_vm.obj)
else ()
   add_buildvm_target ( lj_vm.s ${LJVM_MODE} )
   set (LJ_VM_SRC ${CMAKE_CURRENT_BINARY_DIR}/lj_vm.s)
endif ()

add_buildvm_target ( lj_ffdef.h   ffdef   ${SRC_LJLIB} )
add_buildvm_target ( lj_bcdef.h  bcdef  ${SRC_LJLIB} )
add_buildvm_target ( lj_folddef.h folddef src/lj_opt_fold.c )
add_buildvm_target ( lj_recdef.h  recdef  ${SRC_LJLIB} )
add_buildvm_target ( lj_libdef.h  libdef  ${SRC_LJLIB} )
add_buildvm_target ( vmdef.lua  vmdef  ${SRC_LJLIB} )

set (DEPS
  ${LJ_VM_SRC}
  ${CMAKE_CURRENT_BINARY_DIR}/lj_ffdef.h
  ${CMAKE_CURRENT_BINARY_DIR}/lj_bcdef.h
  ${CMAKE_CURRENT_BINARY_DIR}/lj_libdef.h
  ${CMAKE_CURRENT_BINARY_DIR}/lj_recdef.h
  ${CMAKE_CURRENT_BINARY_DIR}/lj_folddef.h
  ${CMAKE_CURRENT_BINARY_DIR}/vmdef.lua
  )

include_directories(BEFORE ${CMAKE_CURRENT_BINARY_DIR} dynasm src)

add_library(luajit STATIC src/ljamalg.c ${DEPS})