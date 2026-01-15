# This module integrates LuaJIT 2.1 with the Parasol framework to provide the Fluid scripting language. The build
# process generates multiple header files through a multi-stage compilation process.
#
# SPECIAL FEATURES:
#  Enable LUAJIT_USE_SYSMALLOC temporarily if you need to figure out memory management and overflow issues.
#  Enable LUAJIT_USE_GDBJIT temporarily if debugging with GDB.
#  Enable ENABLE_UNIT_TESTS to include unit tests for non-release builds.  You will need to run test_unit_tests.fluid with
#    the --log-api option to view any output.
#
# GENERATED HEADERS OVERVIEW:
#
# From FDL files (Parasol Interface Definition Language):
#   hashes.h                  - Hash definitions for Fluid internal use
#   include/parasol/modules/fluid.h - Public API header for Fluid module
#
# From LuaJIT build process:
#   lj_bcdef.h     - Bytecode definitions (opcode metadata)
#   lj_ffdef.h     - Fast function definitions (C function bindings)
#   lj_libdef.h    - Library definitions (standard library metadata)
#   lj_recdef.h    - Recorder definitions (trace compiler metadata)
#   lj_folddef.h   - Folding definitions (constant folding rules)
#   buildvm_arch.h - Architecture-specific VM definitions
#   jit/vmdef.lua  - VM definitions for LuaJIT runtime
#
# LUAJIT BUILD STAGES:
#
# 1. Host Tool Compilation (tools that run during build):
#    - minilua: Minimal Lua interpreter for running dynasm
#    - buildvm: Code generator that produces headers and VM code
#
# 2. Architecture Detection:
#    - Preprocesses lj_arch.h to detect target architecture
#    - Determines platform-specific flags for dynasm
#
# 3. Code Generation:
#    - dynasm generates buildvm_arch.h from vm_*.dasc files
#    - buildvm generates bytecode/library/recorder definitions
#    - buildvm generates VM assembly or object code
#
# 4. Library Compilation:
#    - Links with generated VM code to create static library

set (MOD "fluid")
set (INC_MOD_FLUID TRUE PARENT_SCOPE)

# Enable unit tests for deep testing of non-release builds only.
# set (ENABLE_UNIT_TESTS ON)

# IDL processing for headers and documentation

idl_gen ("hashes.fdl" NAME ${MOD}_hashes OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/hashes.h")

idl_gen ("${MOD}.fdl" NAME ${MOD}_defs
   OUTPUT "${INCLUDE_OUTPUT}/modules/${MOD}.h"
   APPEND_IDL "module_def.cpp"
   ARGS "output-defs=module_def.cpp"
   FILES "${MOD}.cpp")

# We build libFFI as a release build in all situations because ASAN doesn't like Debug builds of libFFI.

if (MSVC)
   set (FFI_LINK "${CMAKE_BINARY_DIR}/libffi-3.3/lib/libffi.lib")
else ()
   set (FFI_LINK "${CMAKE_BINARY_DIR}/libffi-3.3/lib/liblibffi.a")
endif ()

ExternalProject_Add(libffi-3.3
   SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/libffi-3.3
   CMAKE_ARGS
      ${GLOBAL_DEFAULT_ARGS}
      ${GLOBAL_THIRDPARTY_LIB_ARGS}
      -DCMAKE_BUILD_TYPE=Release
      -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}/libffi-3.3
   INSTALL_DIR ${CMAKE_BINARY_DIR}/libffi-3.3
   BUILD_BYPRODUCTS ${FFI_LINK}
   COMMENT "Compiling FFI library")

# LuaJIT library custom build

set (LUAJIT_SRC "${CMAKE_CURRENT_SOURCE_DIR}/luajit-2.1/src")

# Define build directory for generated files (avoids polluting source tree)

set (LUAJIT_BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}/luajit-generated")
file(MAKE_DIRECTORY "${LUAJIT_BUILD_DIR}")
file(MAKE_DIRECTORY "${LUAJIT_BUILD_DIR}/jit")

# Generated header files

set (LUAJIT_GENERATED_HEADERS
   "${LUAJIT_BUILD_DIR}/lj_bcdef.h"
   "${LUAJIT_BUILD_DIR}/lj_ffdef.h"
   "${LUAJIT_BUILD_DIR}/lj_libdef.h"
   "${LUAJIT_BUILD_DIR}/lj_recdef.h"
   "${LUAJIT_BUILD_DIR}/lj_folddef.h"
)

set (LUAJIT_VMDEF_LUA "${LUAJIT_BUILD_DIR}/jit/vmdef.lua")
set (LUAJIT_COMMON_DEFS LUAJIT_DISABLE_FFI)
set (LUAJIT_NON_MSVC_DEFS -DLUAJIT_DISABLE_FFI -DLUAJIT_DISABLE_BUFFER)

option(PARASOL_PARSER_TRACE "Emit parser token and expectation traces" OFF)
option(INCLUDE_TIPS "Include the parser tips system for code analysis" ON)

if (PARASOL_PARSER_TRACE)
   list(APPEND LUAJIT_COMMON_DEFS PARASOL_PARSER_TRACE)
   list(APPEND LUAJIT_NON_MSVC_DEFS -DPARASOL_PARSER_TRACE)
endif ()

if (INCLUDE_TIPS)
   list(APPEND LUAJIT_COMMON_DEFS INCLUDE_TIPS)
   list(APPEND LUAJIT_NON_MSVC_DEFS -DINCLUDE_TIPS)
endif ()

if (ENABLE_UNIT_TESTS)
   list(APPEND LUAJIT_COMMON_DEFS ENABLE_UNIT_TESTS)
   list(APPEND LUAJIT_NON_MSVC_DEFS -DENABLE_UNIT_TESTS)
endif ()

# LUA_USE_ASSERT enables runtime assertions for early detection of VM state issues.
# This catches problems like invalid stack state (L->top < L->base) in gc_traverse_frames.
# See VMHelperGuard in stack_helpers.h for context on the issues this helps catch.
#
# IMPORTANT: LUA_USE_ASSERT is NOT added to LUAJIT_COMMON_DEFS or LUAJIT_NON_MSVC_DEFS
# because host tools (minilua, buildvm) use those lists but don't link with the runtime
# that provides lj_assert_fail(). Instead, we use:
#   - Generator expressions for MSVC multi-config builds (in target_compile_definitions)
#   - LUAJIT_LIBRARY_DEFS for non-MSVC single-config builds (in custom commands)

set(LUAJIT_LIBRARY_DEFS "")
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
   set(LUAJIT_LIBRARY_DEFS -DLUA_USE_ASSERT)
endif ()

# Common host tool sources (used by both MSVC and non-MSVC builds)
set (LUAJIT_HOST_DIR "${LUAJIT_SRC}/host")
set (LUAJIT_DYNASM_DIR "${CMAKE_CURRENT_SOURCE_DIR}/luajit-2.1/dynasm")
set (MINILUA_SRC "${LUAJIT_HOST_DIR}/minilua.cpp")

# Library source files needed for buildvm header generation (used in both build paths)

set (LJLIB_C
   "${LUAJIT_SRC}/lib/lib_base.cpp"
   "${LUAJIT_SRC}/lib/lib_math.cpp"
   "${LUAJIT_SRC}/lib/lib_bit.cpp"
   "${LUAJIT_SRC}/lib/lib_string.cpp"
   "${LUAJIT_SRC}/lib/lib_table.cpp"
   "${LUAJIT_SRC}/lib/lib_range.cpp"
   "${LUAJIT_SRC}/lib/lib_array.cpp"
   "${LUAJIT_SRC}/lib/lib_object.cpp"
   "${LUAJIT_SRC}/lib/lib_debug.cpp"
   "${LUAJIT_SRC}/lib/lib_jit.cpp"
)

# Buildvm source files (used in both build paths)
set (BUILDVM_SOURCES
   "${LUAJIT_HOST_DIR}/buildvm.cpp"
   "${LUAJIT_HOST_DIR}/buildvm_asm.cpp"
   "${LUAJIT_HOST_DIR}/buildvm_peobj.cpp"
   "${LUAJIT_HOST_DIR}/buildvm_lib.cpp"
   "${LUAJIT_HOST_DIR}/buildvm_fold.cpp"
)

# Common compiler flags and paths (use lists instead of strings for proper handling)
set (LUAJIT_HOST_CFLAGS -O2 -Wall)
set (LUAJIT_INCLUDE_DIRS -I${PROJECT_SOURCE_DIR}/include -I${CMAKE_BINARY_DIR} -I${LUAJIT_SRC} -I${LUAJIT_BUILD_DIR} -I${LUAJIT_SRC}/bytecode -I${LUAJIT_SRC}/debug -I${LUAJIT_SRC}/jit -I${LUAJIT_SRC}/lib -I${LUAJIT_SRC}/parser -I${LUAJIT_SRC}/runtime)
set (LUAJIT_COMMON_CXXFLAGS -fPIC -O2 -fomit-frame-pointer -Wall -std=c++20)

# Platform-specific math library (empty on Windows, -lm on Unix)
if (WIN32)
   set(LUAJIT_MATH_LIB "")
else ()
   set(LUAJIT_MATH_LIB "-lm")
endif ()

if (MSVC)
   # LuaJIT build for MSVC - separated into code generation and library compilation
   # Code generation creates headers and lj_vm.obj via custom batch script

   set (LUAJIT_VM_OBJ "${LUAJIT_BUILD_DIR}/lj_vm.obj")

   # Expand glob patterns for dependencies (only source headers, not generated ones)

   file(GLOB LUAJIT_CORE_SOURCES
      "${LUAJIT_SRC}/debug/lj_*.cpp"
      "${LUAJIT_SRC}/debug/try_except.cpp"
      "${LUAJIT_SRC}/runtime/lj_*.cpp"
      "${LUAJIT_SRC}/lj_*.cpp")

   file(GLOB LUAJIT_LIB_SOURCES "${LUAJIT_SRC}/lib/lib_*.cpp")

   # Exclude library and parser files that are compiled into the Fluid module instead
   list(FILTER LUAJIT_LIB_SOURCES EXCLUDE REGEX "lib_.*\\.cpp$")
   list(FILTER LUAJIT_CORE_SOURCES EXCLUDE REGEX "lib\\.cpp$|load\\.cpp$|lj_parse\\.cpp$|lj_lex\\.cpp$|parser_unit_tests\\.cpp$")

   file(GLOB LUAJIT_DEP_LJ_H
      "${LUAJIT_SRC}/bytecode/lj_*.h"
      "${LUAJIT_SRC}/debug/lj_*.h"
      "${LUAJIT_SRC}/runtime/lj_*.h"
      "${LUAJIT_SRC}/lj_*.h")

   # Platform-specific paths
   set (MINILUA_TARGET "${LUAJIT_BUILD_DIR}/minilua.exe")
   set (BUILDVM_TARGET "${LUAJIT_BUILD_DIR}/buildvm.exe")
   set (BUILDVM_ARCH_H "${LUAJIT_BUILD_DIR}/buildvm_arch.h")

   # Enforce 64-bit only architecture
   if (NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
      message(FATAL_ERROR "Fluid requires a 64-bit build environment. 32-bit support has been removed.")
   endif()

   set (LUAJIT_DASC "${LUAJIT_SRC}/jit/vm_x64.dasc")
   set (LUAJIT_DASMFLAGS -D WIN -D JIT -D P64)
   set (LUAJIT_ARCH "x64")

   # Build minilua host tool
   add_custom_command(
      OUTPUT ${MINILUA_TARGET}
      COMMAND ${CMAKE_CXX_COMPILER} /nologo /c /W3 /D_CRT_SECURE_NO_DEPRECATE /std:c++20
         /Fo${LUAJIT_BUILD_DIR}/minilua.obj
         ${MINILUA_SRC}
      COMMAND ${CMAKE_LINKER} /nologo /out:${MINILUA_TARGET}
         ${LUAJIT_BUILD_DIR}/minilua.obj
      DEPENDS ${MINILUA_SRC}
      WORKING_DIRECTORY "${LUAJIT_SRC}"
      COMMENT "Building minilua host tool for ${LUAJIT_ARCH}"
      VERBATIM
   )

   # Generate buildvm_arch.h using dynasm
   add_custom_command(
      OUTPUT ${BUILDVM_ARCH_H}
      COMMAND ${MINILUA_TARGET} ${LUAJIT_DYNASM_DIR}/dynasm.lua -LN ${LUAJIT_DASMFLAGS} -o ${BUILDVM_ARCH_H} ${LUAJIT_DASC}
      DEPENDS ${MINILUA_TARGET} "${LUAJIT_DYNASM_DIR}/dynasm.lua" ${LUAJIT_DASC} "${LUAJIT_SRC}/lj_arch.h"
      WORKING_DIRECTORY "${LUAJIT_SRC}"
      COMMENT "Generating buildvm_arch.h via dynasm for ${LUAJIT_ARCH}"
      VERBATIM
   )

   # Build buildvm host tool
   # Note: lj_obj.h contains MMDEF which defines metamethod tables used by buildvm
   add_custom_command(
      OUTPUT ${BUILDVM_TARGET}
      COMMAND ${CMAKE_CXX_COMPILER} /nologo /c /W3 /std:c++20
         /D_CRT_SECURE_NO_DEPRECATE /DLUAJIT_DISABLE_FFI
         /I${LUAJIT_SRC} /I${LUAJIT_DYNASM_DIR} /I${LUAJIT_BUILD_DIR} /I${LUAJIT_SRC}/bytecode
         /I${LUAJIT_SRC}/debug /I${LUAJIT_SRC}/lib /I${LUAJIT_SRC}/parser /I${LUAJIT_SRC}/runtime
         /I${PROJECT_SOURCE_DIR}/include /I${CMAKE_BINARY_DIR}
         ${BUILDVM_SOURCES}
      COMMAND ${CMAKE_LINKER} /nologo /out:${BUILDVM_TARGET}
         ${LUAJIT_BUILD_DIR}/buildvm.obj
         ${LUAJIT_BUILD_DIR}/buildvm_asm.obj
         ${LUAJIT_BUILD_DIR}/buildvm_peobj.obj
         ${LUAJIT_BUILD_DIR}/buildvm_lib.obj
         ${LUAJIT_BUILD_DIR}/buildvm_fold.obj
      DEPENDS ${BUILDVM_SOURCES} ${BUILDVM_ARCH_H} "${LUAJIT_SRC}/runtime/lj_obj.h"
      WORKING_DIRECTORY "${LUAJIT_BUILD_DIR}"
      COMMENT "Building buildvm host tool for ${LUAJIT_ARCH}"
      VERBATIM
   )

   # Generate headers and VM object file using buildvm
   # lj_obj.h dependency ensures VM is regenerated when metamethod table changes
   add_custom_command(
      OUTPUT ${LUAJIT_GENERATED_HEADERS} ${LUAJIT_VM_OBJ} ${LUAJIT_VMDEF_LUA}
      COMMAND ${BUILDVM_TARGET} -m peobj -o ${LUAJIT_VM_OBJ}
      COMMAND ${BUILDVM_TARGET} -m bcdef -o ${LUAJIT_BUILD_DIR}/lj_bcdef.h ${LJLIB_C}
      COMMAND ${BUILDVM_TARGET} -m ffdef -o ${LUAJIT_BUILD_DIR}/lj_ffdef.h ${LJLIB_C}
      COMMAND ${BUILDVM_TARGET} -m libdef -o ${LUAJIT_BUILD_DIR}/lj_libdef.h ${LJLIB_C}
      COMMAND ${BUILDVM_TARGET} -m recdef -o ${LUAJIT_BUILD_DIR}/lj_recdef.h ${LJLIB_C}
      COMMAND ${BUILDVM_TARGET} -m folddef -o ${LUAJIT_BUILD_DIR}/lj_folddef.h ${LUAJIT_SRC}/lj_opt_fold.cpp
      COMMAND ${BUILDVM_TARGET} -m vmdef -o ${LUAJIT_VMDEF_LUA} ${LJLIB_C}
      DEPENDS ${BUILDVM_TARGET} ${LJLIB_C} "${LUAJIT_SRC}/lj_opt_fold.cpp" "${LUAJIT_SRC}/runtime/lj_obj.h"
      WORKING_DIRECTORY "${LUAJIT_SRC}"
      COMMENT "Generating LuaJIT headers and VM object for ${LUAJIT_ARCH}"
      VERBATIM
   )

   add_custom_target(luajit_codegen DEPENDS ${LUAJIT_GENERATED_HEADERS} ${LUAJIT_VM_OBJ})

   # Create LuaJIT static library
   add_library(luajit_lib STATIC ${LUAJIT_CORE_SOURCES} ${LUAJIT_LIB_SOURCES} ${LUAJIT_VM_OBJ})

   add_dependencies(luajit_lib luajit_codegen)

   set_target_properties(luajit_lib PROPERTIES
      OUTPUT_NAME "lua51"
      ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/luajit-2.1/lib"
   )
   target_compile_definitions(luajit_lib PRIVATE
      ${LUAJIT_COMMON_DEFS}
      _CRT_SECURE_NO_DEPRECATE
      $<$<CONFIG:Debug>:LUA_USE_ASSERT>)

   target_include_directories(luajit_lib PRIVATE
      "${LUAJIT_SRC}"
      "${LUAJIT_SRC}/bytecode"
      "${LUAJIT_SRC}/debug"
      "${LUAJIT_SRC}/jit"
      "${LUAJIT_SRC}/lib"
      "${LUAJIT_SRC}/parser"
      "${LUAJIT_SRC}/runtime"
      "${LUAJIT_BUILD_DIR}"  # Include generated headers from build dir
   )

   # Disable specific warnings from LuaJIT and disable GS security checks for performance
   # /MP enables multi-processor compilation for parallel builds within this target
   target_compile_options(luajit_lib PRIVATE /MP /wd4244 /wd5287 /GS-)

   # Use generator expression for configuration-specific paths in MSVC multi-config builds
   # Check if using a multi-config generator (Visual Studio) vs single-config (Ninja)
   get_property(MULTICONFIG_GENERATOR GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
   if (MULTICONFIG_GENERATOR)
      set (LUAJIT_LINK "${CMAKE_BINARY_DIR}/luajit-2.1/lib/$<CONFIG>/lua51.lib")
   else()
      set (LUAJIT_LINK "${CMAKE_BINARY_DIR}/luajit-2.1/lib/lua51.lib")
   endif()

   # Create INTERFACE library to encapsulate LuaJIT usage requirements
   add_library(luajit_interface INTERFACE)
   target_include_directories(luajit_interface INTERFACE "${LUAJIT_SRC}")
   target_compile_definitions(luajit_interface INTERFACE ${LUAJIT_COMMON_DEFS})

   # Create an alias for backward compatibility
   add_custom_target(luajit DEPENDS luajit_lib)

else ()
   # Non-MSVC build (Linux, MinGW, etc.) - LuaJIT build using CMake
   # Separated into code generation and library compilation
   # On Windows (MinGW), use peobj like MSVC; on Unix, use assembly
   # Library compilation is handled by CMake for proper incremental builds

   if (WIN32)
      # MinGW uses PE object format like MSVC
      set (LUAJIT_VM_OBJ "${LUAJIT_BUILD_DIR}/lj_vm.obj")
   else ()
      # Unix platforms use assembly source
      set (LUAJIT_VM_S "${LUAJIT_BUILD_DIR}/lj_vm.S")
      set (LUAJIT_VM_O "${LUAJIT_BUILD_DIR}/lj_vm.o")
   endif ()

   # Platform-specific paths
   set (MINILUA_TARGET "${LUAJIT_BUILD_DIR}/minilua${CMAKE_EXECUTABLE_SUFFIX}")
   set (BUILDVM_TARGET "${LUAJIT_BUILD_DIR}/buildvm${CMAKE_EXECUTABLE_SUFFIX}")
   set (BUILDVM_ARCH_H "${LUAJIT_BUILD_DIR}/buildvm_arch.h")

   # Build minilua host tool first

   add_custom_command(
      OUTPUT ${MINILUA_TARGET}
      COMMAND ${CMAKE_CXX_COMPILER} ${LUAJIT_HOST_CFLAGS} -o ${MINILUA_TARGET} ${MINILUA_SRC} ${LUAJIT_MATH_LIB}
      DEPENDS ${MINILUA_SRC}
      COMMENT "Building minilua host tool"
      VERBATIM
   )

   # Enforce 64-bit only architecture
   if (NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
      message(FATAL_ERROR "Fluid requires a 64-bit build environment. 32-bit support has been removed.")
   endif()

   # Determine target architecture using CMake built-in variables
   # Normalize to lowercase for case-insensitive matching (Windows/cross-toolchains use uppercase)

   string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" LUAJIT_SYSTEM_PROCESSOR_LOWER)

   if (LUAJIT_SYSTEM_PROCESSOR_LOWER MATCHES "^(aarch64|arm64|arm)")
      set (LUAJIT_TARGET_ARCH "arm64")
   elseif (LUAJIT_SYSTEM_PROCESSOR_LOWER MATCHES "^(powerpc|ppc)")
      set (LUAJIT_TARGET_ARCH "ppc")
   else()
      # Default to x64 for other architectures (all now require 64-bit)
      set (LUAJIT_TARGET_ARCH "x64")
   endif ()

   # Generate buildvm_arch.h using dynasm
   # Use the detected architecture to select the appropriate .dasc file
   # Extract architecture flags from lj_arch.h preprocessing to pass to dynasm

   set (DYNASM_SCRIPT "${LUAJIT_BUILD_DIR}/run_dynasm.cmake")

   # Create test_arch.c for preprocessing
   if (NOT EXISTS "${LUAJIT_BUILD_DIR}/test_arch.c")
      file(WRITE "${LUAJIT_BUILD_DIR}/test_arch.c" "#include <lj_arch.h>\n")
   endif ()

   # Write the dynasm runner script

   file(WRITE ${DYNASM_SCRIPT}
         "# Use architecture determined by CMake\n"
         "set(ARCH \"${LUAJIT_TARGET_ARCH}\")\n"
         "set(DASM_DASC \"${LUAJIT_SRC}/jit/vm_\${ARCH}.dasc\")\n"
         "if(NOT EXISTS \"\${DASM_DASC}\")\n"
         "   message(FATAL_ERROR \"Architecture file \${DASM_DASC} not found\")\n"
         "endif()\n"
         "# Detect architecture flags by preprocessing lj_arch.h\n"
         "execute_process(\n"
         "   COMMAND ${CMAKE_CXX_COMPILER} ${LUAJIT_NON_MSVC_DEFS} -I${LUAJIT_SRC} -E -dM ${LUAJIT_BUILD_DIR}/test_arch.c\n"
         "   WORKING_DIRECTORY ${LUAJIT_SRC}\n"
         "   OUTPUT_VARIABLE ARCH_DEFS\n"
         "   OUTPUT_STRIP_TRAILING_WHITESPACE\n"
         "   ERROR_QUIET\n"
         ")\n"
         "# Build DASM_AFLAGS based on detected defines\n"
         "set(DASM_AFLAGS \"\")\n"
         "if(ARCH_DEFS MATCHES \"LJ_LE 1\")\n"
         "   list(APPEND DASM_AFLAGS \"-D\" \"ENDIAN_LE\")\n"
         "else()\n"
         "   list(APPEND DASM_AFLAGS \"-D\" \"ENDIAN_BE\")\n"
         "endif()\n"
         "if(ARCH_DEFS MATCHES \"LJ_ARCH_BITS 64\")\n"
         "   list(APPEND DASM_AFLAGS \"-D\" \"P64\")\n"
         "endif()\n"
         "if(ARCH_DEFS MATCHES \"LJ_HASJIT 1\")\n"
         "   list(APPEND DASM_AFLAGS \"-D\" \"JIT\")\n"
         "endif()\n"
         "if(ARCH_DEFS MATCHES \"LJ_DUALNUM 1\")\n"
         "   list(APPEND DASM_AFLAGS \"-D\" \"DUALNUM\")\n"
         "endif()\n"
         "if(ARCH_DEFS MATCHES \"LJ_ARCH_HASFPU 1\")\n"
         "   list(APPEND DASM_AFLAGS \"-D\" \"FPU\")\n"
         "endif()\n"
         "if(NOT ARCH_DEFS MATCHES \"LJ_ABI_SOFTFP 1\")\n"
         "   list(APPEND DASM_AFLAGS \"-D\" \"HFABI\")\n"
         "endif()\n"
         "if(ARCH_DEFS MATCHES \"LJ_NO_UNWIND 1\")\n"
         "   list(APPEND DASM_AFLAGS \"-D\" \"NO_UNWIND\")\n"
         "endif()\n"
         "if(WIN32)\n"
         "   list(APPEND DASM_AFLAGS \"-D\" \"WIN\")\n"
         "endif()\n"
         "if(ARCH_DEFS MATCHES \"LJ_ARCH_VERSION ([0-9]+)\")\n"
         "   list(APPEND DASM_AFLAGS \"-D\" \"VER=\${CMAKE_MATCH_1}\")\n"
         "endif()\n"
         "# Platform-specific flags\n"
         "# iOS flag for ARM platforms\n"
         "if(ARCH_DEFS MATCHES \"__APPLE__\" AND ARCH_DEFS MATCHES \"LJ_ARCH_ARM\")\n"
         "   list(APPEND DASM_AFLAGS \"-D\" \"IOS\")\n"
         "endif()\n"
         "# SQRT, ROUND, and GPR64 flags for PowerPC\n"
         "if(ARCH_DEFS MATCHES \"LJ_ARCH_PPC 1\")\n"
         "   list(APPEND DASM_AFLAGS \"-D\" \"SQRT\")\n"
         "   list(APPEND DASM_AFLAGS \"-D\" \"ROUND\")\n"
         "   if(ARCH_DEFS MATCHES \"LJ_ARCH_PPC64 1\")\n"
         "      list(APPEND DASM_AFLAGS \"-D\" \"GPR64\")\n"
         "   endif()\n"
         "endif()\n"
         "# Run dynasm\n"
         "execute_process(\n"
         "   COMMAND \"${MINILUA_TARGET}\" \"${LUAJIT_DYNASM_DIR}/dynasm.lua\" \${DASM_AFLAGS} -o \"${BUILDVM_ARCH_H}\" \"\${DASM_DASC}\"\n"
         "   WORKING_DIRECTORY \"${LUAJIT_SRC}\"\n"
         "   RESULT_VARIABLE DYNASM_RESULT\n"
         "   OUTPUT_QUIET ERROR_QUIET\n"
         ")\n"
         "if(DYNASM_RESULT)\n"
         "   message(FATAL_ERROR \"Dynasm failed with exit code \${DYNASM_RESULT}\")\n"
         "endif()\n"
      )

   add_custom_command(
      OUTPUT ${BUILDVM_ARCH_H}
      COMMAND ${CMAKE_COMMAND} -E env TARGET_GENDIR=${LUAJIT_BUILD_DIR} ${CMAKE_COMMAND} -P ${DYNASM_SCRIPT}
      DEPENDS ${MINILUA_TARGET} ${LUAJIT_DYNASM_DIR}/dynasm.lua ${DYNASM_SCRIPT}
         "${LUAJIT_SRC}/lj_arch.h" "${LUAJIT_SRC}/lua.h" "${LUAJIT_SRC}/luaconf.h"
         "${LUAJIT_SRC}/jit/vm_x64.dasc" "${LUAJIT_SRC}/jit/vm_arm64.dasc" "${LUAJIT_SRC}/jit/vm_ppc.dasc" "${LUAJIT_BUILD_DIR}/test_arch.c"
      COMMENT "Generating buildvm_arch.h via dynasm"
      VERBATIM
   )

   # Build buildvm host tool
   # Note: lj_obj.h contains MMDEF which defines metamethod tables used by buildvm

   add_custom_command(
      OUTPUT ${BUILDVM_TARGET}
      COMMAND ${CMAKE_CXX_COMPILER} ${LUAJIT_COMMON_CXXFLAGS}
         ${LUAJIT_NON_MSVC_DEFS}
         ${LUAJIT_INCLUDE_DIRS}
         -o ${BUILDVM_TARGET} ${BUILDVM_SOURCES} ${LUAJIT_MATH_LIB}
      DEPENDS ${BUILDVM_SOURCES} ${BUILDVM_ARCH_H} "${LUAJIT_SRC}/runtime/lj_obj.h"
      COMMENT "Building buildvm host tool"
      VERBATIM
   )

   # Generate headers and VM file using buildvm
   # Windows (MinGW) uses peobj to generate .obj directly like MSVC
   # Unix uses elfasm to generate .S assembly source that gets compiled later
   # lj_obj.h dependency ensures VM is regenerated when metamethod table changes

   # Set platform-specific VM generation parameters

   if (WIN32)
      # MinGW: Generate PE object file directly (no assembly step needed)
      set(LUAJIT_VM_OUTPUT ${LUAJIT_VM_OBJ})
      set(LUAJIT_VM_MODE "peobj")
      set(LUAJIT_VM_COMMENT "Generating LuaJIT headers and VM object via buildvm")
   else ()
      # Unix: Generate assembly source for later compilation
      set(LUAJIT_VM_OUTPUT ${LUAJIT_VM_S})
      set(LUAJIT_VM_MODE "elfasm")
      set(LUAJIT_VM_COMMENT "Generating LuaJIT headers and VM assembly via buildvm")
   endif ()

   add_custom_command(
      OUTPUT ${LUAJIT_GENERATED_HEADERS} ${LUAJIT_VM_OUTPUT} ${LUAJIT_VMDEF_LUA}
      COMMAND ${BUILDVM_TARGET} -m bcdef -o ${LUAJIT_BUILD_DIR}/lj_bcdef.h ${LJLIB_C}
      COMMAND ${BUILDVM_TARGET} -m ffdef -o ${LUAJIT_BUILD_DIR}/lj_ffdef.h ${LJLIB_C}
      COMMAND ${BUILDVM_TARGET} -m libdef -o ${LUAJIT_BUILD_DIR}/lj_libdef.h ${LJLIB_C}
      COMMAND ${BUILDVM_TARGET} -m recdef -o ${LUAJIT_BUILD_DIR}/lj_recdef.h ${LJLIB_C}
      COMMAND ${BUILDVM_TARGET} -m folddef -o ${LUAJIT_BUILD_DIR}/lj_folddef.h "${LUAJIT_SRC}/lj_opt_fold.cpp"
      COMMAND ${BUILDVM_TARGET} -m vmdef -o ${LUAJIT_VMDEF_LUA} ${LJLIB_C}
      COMMAND ${BUILDVM_TARGET} -m ${LUAJIT_VM_MODE} -o ${LUAJIT_VM_OUTPUT}
      DEPENDS ${BUILDVM_TARGET} ${LJLIB_C} "${LUAJIT_SRC}/lj_opt_fold.cpp" "${LUAJIT_SRC}/runtime/lj_obj.h"
      WORKING_DIRECTORY ${LUAJIT_SRC}
      COMMENT ${LUAJIT_VM_COMMENT}
      VERBATIM
   )

   if (WIN32)
      # MinGW: VM object file is already generated by buildvm, no compilation needed
      add_custom_target(luajit_codegen
         DEPENDS ${LUAJIT_GENERATED_HEADERS} ${LUAJIT_VM_OBJ} ${LUAJIT_VMDEF_LUA}
      )
   else ()
      # Unix: Generate assembly source (compilation happens in library target)
      add_custom_target(luajit_codegen
         DEPENDS ${LUAJIT_GENERATED_HEADERS} ${LUAJIT_VM_S} ${LUAJIT_VMDEF_LUA}
      )
   endif ()

   # Expand glob patterns for source files (mirrors MSVC approach)

   file(GLOB LUAJIT_CORE_SOURCES
      "${LUAJIT_SRC}/debug/lj_*.cpp"
      "${LUAJIT_SRC}/debug/try_except.cpp"
      "${LUAJIT_SRC}/runtime/lj_*.cpp"
      "${LUAJIT_SRC}/lj_*.cpp")

   file(GLOB LUAJIT_LIB_SOURCES "${LUAJIT_SRC}/lib/lib_*.cpp")

   # Exclude library and parser files that are compiled into the Fluid module instead
   list(FILTER LUAJIT_LIB_SOURCES EXCLUDE REGEX "lib_.*\\.cpp$")
   list(FILTER LUAJIT_CORE_SOURCES EXCLUDE REGEX "lib\\.cpp$|load\\.cpp$|lj_parse\\.cpp$|lj_lex\\.cpp$|parser_unit_tests\\.cpp$")

   # Generate list of object files from source files
   set(LUAJIT_OBJECT_FILES "")
   foreach(SOURCE_FILE ${LUAJIT_CORE_SOURCES} ${LUAJIT_LIB_SOURCES})
      get_filename_component(FILENAME ${SOURCE_FILE} NAME_WE)
      set(OBJECT_FILE "${LUAJIT_BUILD_DIR}/${FILENAME}.o")
      list(APPEND LUAJIT_OBJECT_FILES ${OBJECT_FILE})

      # Create custom command to compile each source file with explicit flags
      add_custom_command(
         OUTPUT ${OBJECT_FILE}
         COMMAND ${CMAKE_CXX_COMPILER} ${LUAJIT_COMMON_CXXFLAGS}
            -funwind-tables -DLUAJIT_UNWIND_EXTERNAL
            ${LUAJIT_NON_MSVC_DEFS}
            ${LUAJIT_LIBRARY_DEFS}
            -DLUA_ROOT=\"${CMAKE_INSTALL_PREFIX}\"
            ${LUAJIT_INCLUDE_DIRS}
            -Wno-trigraphs
            -c ${SOURCE_FILE} -o ${OBJECT_FILE}
         DEPENDS ${SOURCE_FILE} ${LUAJIT_GENERATED_HEADERS} "${LUAJIT_SRC}/lj_ircall.h"
         COMMENT "Compiling ${FILENAME}.cpp"
         VERBATIM
      )
   endforeach()

   if (WIN32)
      # MinGW: Create library from individual object files
      add_library(luajit_lib STATIC)
      target_sources(luajit_lib PRIVATE ${LUAJIT_OBJECT_FILES} ${LUAJIT_VM_OBJ})
      set_target_properties(luajit_lib PROPERTIES LINKER_LANGUAGE C)
   else ()
      # Unix: Compile VM assembly with explicit flags, then create library

      # Compile VM assembly file
      add_custom_command(
         OUTPUT ${LUAJIT_VM_O}
         COMMAND ${CMAKE_CXX_COMPILER} ${LUAJIT_COMMON_CXXFLAGS}
            -funwind-tables -DLUAJIT_UNWIND_EXTERNAL
            ${LUAJIT_NON_MSVC_DEFS}
            ${LUAJIT_LIBRARY_DEFS}
            ${LUAJIT_INCLUDE_DIRS}
            -c ${LUAJIT_VM_S} -o ${LUAJIT_VM_O}
         DEPENDS ${LUAJIT_VM_S}
         COMMENT "Compiling LuaJIT VM assembly"
         VERBATIM
      )

      # Create library from individual object files
      add_library(luajit_lib STATIC)
      target_sources(luajit_lib PRIVATE ${LUAJIT_OBJECT_FILES} ${LUAJIT_VM_O})
      set_target_properties(luajit_lib PROPERTIES LINKER_LANGUAGE C)
   endif ()

   add_dependencies(luajit_lib luajit_codegen)

   set_target_properties(luajit_lib PROPERTIES
      OUTPUT_NAME "luajit-5.1"
      ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/luajit-2.1/lib"
   )

   # Set link path for fluid module
   set (LUAJIT_LINK "${CMAKE_BINARY_DIR}/luajit-2.1/lib/${CMAKE_STATIC_LIBRARY_PREFIX}luajit-5.1${CMAKE_STATIC_LIBRARY_SUFFIX}")

   # Create INTERFACE library to encapsulate LuaJIT usage requirements
   add_library(luajit_interface INTERFACE)
   target_include_directories(luajit_interface INTERFACE "${LUAJIT_SRC}")
   target_compile_definitions(luajit_interface INTERFACE ${LUAJIT_COMMON_DEFS})

   add_custom_target(luajit DEPENDS luajit_lib)  # Create an alias for backward compatibility
endif ()

# Fluid library build

add_library (${MOD})

set_module_defaults (${MOD} "Fl")

add_dependencies (${MOD} libffi-3.3 luajit luajit_codegen)

if (BUILD_DEFS)
   add_dependencies (${MOD} ${MOD}_hashes)
endif ()

set (FLUID_SOURCES "${MOD}.cpp" "fluid_module.cpp" "fluid_thread.cpp" "fluid_struct.cpp" "fluid_processing.cpp"
   "fluid_number.cpp" "fluid_functions.cpp" "fluid_regex.cpp"
   "fluid_io.cpp" "fluid_class.cpp"
   "${LUAJIT_SRC}/lib/lib.cpp"
   "${LUAJIT_SRC}/lib/load.cpp"
   "${LUAJIT_SRC}/lib/lib_aux.cpp"
   "${LUAJIT_SRC}/lib/lib_array.cpp"
   "${LUAJIT_SRC}/lib/lib_base.cpp"
   "${LUAJIT_SRC}/lib/lib_bit.cpp"
   "${LUAJIT_SRC}/lib/lib_debug.cpp"
   "${LUAJIT_SRC}/lib/lib_init.cpp"
   "${LUAJIT_SRC}/lib/lib_jit.cpp"
   "${LUAJIT_SRC}/lib/lib_math.cpp"
   "${LUAJIT_SRC}/lib/lib_object.cpp"
   "${LUAJIT_SRC}/lib/lib_range.cpp"
   "${LUAJIT_SRC}/lib/lib_string.cpp"
   "${LUAJIT_SRC}/lib/lib_table.cpp"
   "${LUAJIT_SRC}/bytecode/lj_bc.cpp"
   "${LUAJIT_SRC}/bytecode/lj_bcread.cpp"
   "${LUAJIT_SRC}/bytecode/lj_bcwrite.cpp"
   "${LUAJIT_SRC}/debug/dump_bytecode.cpp"
   "${LUAJIT_SRC}/debug/filesource.cpp"
   "${LUAJIT_SRC}/parser/parser.cpp"
   "${LUAJIT_SRC}/parser/lexer.cpp"
   "${LUAJIT_SRC}/parser/parser_unit_tests.cpp")

if (INCLUDE_TIPS)
   list (APPEND FLUID_SOURCES "${LUAJIT_SRC}/parser/parser_tips.cpp")
endif ()

if (ENABLE_UNIT_TESTS)
   list (APPEND FLUID_SOURCES "${LUAJIT_SRC}/runtime/unit_test_indexing.cpp")
   list (APPEND FLUID_SOURCES "${LUAJIT_SRC}/runtime/unit_test_vm_asm.cpp")
   list (APPEND FLUID_SOURCES "${LUAJIT_SRC}/runtime/unit_tests_arrays.cpp")
   list (APPEND FLUID_SOURCES "${LUAJIT_SRC}/jit/jit_unit_tests.cpp")

   # Add MASM assembly file for register capture on MSVC x64
   if (MSVC AND CMAKE_SIZEOF_VOID_P EQUAL 8)
      enable_language(ASM_MASM)
      list (APPEND FLUID_SOURCES "${LUAJIT_SRC}/runtime/register_capture_x64.asm")
   endif ()
endif ()

if (NOT DISABLE_DISPLAY)
   list (APPEND FLUID_SOURCES "fluid_input.cpp")
endif ()

target_sources (${MOD} PRIVATE ${FLUID_SOURCES})

target_include_directories (${MOD} PRIVATE
   "${LUAJIT_SRC}/bytecode"
   "${LUAJIT_SRC}/debug"
   "${LUAJIT_SRC}/jit"
   "${LUAJIT_SRC}/lib"
   "${LUAJIT_SRC}/parser"
   "${LUAJIT_SRC}/runtime"
   "${LUAJIT_BUILD_DIR}"
   "${CMAKE_BINARY_DIR}/libffi-3.3/include")

target_link_libraries (${MOD} PRIVATE
   luajit_interface
   ${FFI_LINK}
   ${LUAJIT_LINK}
   ${MATH_LINK}) # The link order matters, math must come last

target_compile_definitions(${MOD} PRIVATE
   LUAJIT_DISABLE_FFI
   LUAJIT_DISABLE_BUFFER
   $<$<CONFIG:Debug>:LUA_USE_ASSERT>)

if (MSVC)
   # /MP enables multi-processor compilation for parallel builds within this target
   target_compile_options(${MOD} PRIVATE /MP)
endif ()

if (ENABLE_UNIT_TESTS)
   target_compile_definitions(${MOD} PRIVATE ENABLE_UNIT_TESTS)
endif ()

if (DISABLE_DISPLAY)
   target_compile_definitions (${MOD} PRIVATE DISABLE_DISPLAY)
endif ()

# Register Flute tests

set (FLUID_TESTS
   defer debuglog io object regex struct threads xml strings math array array_any array_manip array_functional array_typed_syntax compound
   bitshift bitwise if_empty presence blank ternary table stress unit_tests jitoptions fstring const
   unicode ellipsis safe_nav close type_annotations type_fixing type_inference jit pipe pipe_iteration arrow_functions result_filter
   thunks deferred_expr shadow_assert ranges annotations locality anonymous_for key_values
   choose_from flow table_slice try_except import
)

foreach (TEST_NAME ${FLUID_TESTS})
   flute_test(fluid_${TEST_NAME} "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_${TEST_NAME}.fluid")
endforeach ()
