set (MOD "fluid")
set (INC_MOD_FLUID TRUE PARENT_SCOPE)

# IDL processing for headers and documentation

idl_gen ("hashes.fdl" NAME ${MOD}_hashes OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/hashes.h")
idl_gen ("${MOD}.fdl" NAME ${MOD}_defs OUTPUT "${INCLUDE_OUTPUT}/modules/${MOD}.h" FILES "${MOD}.cpp")

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

# Generated header files
set (LUAJIT_GENERATED_HEADERS
   "${LUAJIT_BUILD_DIR}/lj_bcdef.h"
   "${LUAJIT_BUILD_DIR}/lj_ffdef.h"
   "${LUAJIT_BUILD_DIR}/lj_libdef.h"
   "${LUAJIT_BUILD_DIR}/lj_recdef.h"
   "${LUAJIT_BUILD_DIR}/lj_folddef.h"
)

set (LUAJIT_VMDEF_LUA "${LUAJIT_BUILD_DIR}/jit/vmdef.lua")
set (LUAJIT_COMMON_DEFS "LUAJIT_ENABLE_LUA52COMPAT")
set (LUAJIT_NON_MSVC_DEFS "-DLUAJIT_DISABLE_FFI" "-DLUAJIT_ENABLE_LUA52COMPAT")

# Common compiler flags and paths
set (LUAJIT_HOST_CFLAGS "-std=c99" "-O2" "-Wall")
set (LUAJIT_INCLUDE_DIRS "-I${LUAJIT_SRC}" "-I${LUAJIT_BUILD_DIR}")
set (LUAJIT_COMMON_CFLAGS "-fPIC" "-O2" "-fomit-frame-pointer" "-Wall")

# Helper function to set platform-specific executable name
function(set_platform_executable VAR_NAME BASE_NAME)
   if (WIN32)
      set(${VAR_NAME} "${LUAJIT_BUILD_DIR}/${BASE_NAME}.exe" PARENT_SCOPE)
   else ()
      set(${VAR_NAME} "${LUAJIT_BUILD_DIR}/${BASE_NAME}" PARENT_SCOPE)
   endif ()
endfunction()

# Helper function to set platform-specific math library
function(set_platform_math_lib VAR_NAME)
   if (WIN32)
      set(${VAR_NAME} "" PARENT_SCOPE)
   else ()
      set(${VAR_NAME} "-lm" PARENT_SCOPE)
   endif ()
endfunction()

if (MSVC)
   # LuaJIT build for MSVC - separated into code generation and library compilation
   # Code generation creates headers and lj_vm.obj via custom batch script

   set (LUAJIT_VM_OBJ "${LUAJIT_BUILD_DIR}/lj_vm.obj")

   # Expand glob patterns for dependencies (only source headers, not generated ones)
   file(GLOB LUAJIT_DEP_LJ_C "${LUAJIT_SRC}/lj_*.c")
   file(GLOB LUAJIT_DEP_LIB_C "${LUAJIT_SRC}/lib_*.c")
   file(GLOB LUAJIT_DEP_LJ_H "${LUAJIT_SRC}/lj_*.h")

   # Generate headers and VM object file directly to build directory
   add_custom_command(
      OUTPUT ${LUAJIT_GENERATED_HEADERS} ${LUAJIT_VM_OBJ} ${LUAJIT_VMDEF_LUA}
      COMMAND cmd /c "${LUAJIT_SRC}/msvcbuild_codegen.bat" "${LUAJIT_BUILD_DIR}"
      WORKING_DIRECTORY "${LUAJIT_SRC}"
      DEPENDS
         ${LUAJIT_DEP_LJ_C}
         ${LUAJIT_DEP_LIB_C}
         ${LUAJIT_DEP_LJ_H}
      COMMENT "Generating LuaJIT headers and VM object in build directory"
      VERBATIM
   )

   add_custom_target(luajit_codegen DEPENDS ${LUAJIT_GENERATED_HEADERS} ${LUAJIT_VM_OBJ})

   # Collect all LuaJIT source files
   file(GLOB LUAJIT_CORE_SOURCES "${LUAJIT_SRC}/lj_*.c")
   file(GLOB LUAJIT_LIB_SOURCES "${LUAJIT_SRC}/lib_*.c")

   # Create LuaJIT static library
   add_library(luajit_lib STATIC ${LUAJIT_CORE_SOURCES} ${LUAJIT_LIB_SOURCES} ${LUAJIT_VM_OBJ})

   add_dependencies(luajit_lib luajit_codegen)

   set_target_properties(luajit_lib PROPERTIES
      OUTPUT_NAME "lua51"
      ARCHIVE_OUTPUT_DIRECTORY "${LUAJIT_SRC}"
      ARCHIVE_OUTPUT_DIRECTORY_RELEASE "${LUAJIT_SRC}"
      ARCHIVE_OUTPUT_DIRECTORY_DEBUG "${LUAJIT_SRC}"
      ARCHIVE_OUTPUT_DIRECTORY_RELWITHDEBINFO "${LUAJIT_SRC}"
      ARCHIVE_OUTPUT_DIRECTORY_MINSIZEREL "${LUAJIT_SRC}"
   )
   target_compile_definitions(luajit_lib PRIVATE ${LUAJIT_COMMON_DEFS} _CRT_SECURE_NO_DEPRECATE)

   target_include_directories(luajit_lib PRIVATE
      "${LUAJIT_SRC}"
      "${LUAJIT_BUILD_DIR}"  # Include generated headers from build dir
   )

   # Disable specific warnings from LuaJIT
   target_compile_options(luajit_lib PRIVATE /wd4244 /wd5287)

   set (LUAJIT_LINK "${LUAJIT_SRC}/lua51.lib")

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

   # Host tool sources

   set (LUAJIT_HOST_DIR "${LUAJIT_SRC}/host")
   set (LUAJIT_DYNASM_DIR "${CMAKE_CURRENT_SOURCE_DIR}/luajit-2.1/dynasm")

   # Build minilua host tool first

   set (MINILUA_SRC "${LUAJIT_HOST_DIR}/minilua.c")
   set_platform_executable(MINILUA_TARGET "minilua")
   set_platform_math_lib(MINILUA_LIBS)

   add_custom_command(
      OUTPUT ${MINILUA_TARGET}
      COMMAND ${CMAKE_C_COMPILER} ${LUAJIT_HOST_CFLAGS} -o ${MINILUA_TARGET} ${MINILUA_SRC} ${MINILUA_LIBS}
      DEPENDS ${MINILUA_SRC}
      COMMENT "Building minilua host tool"
      VERBATIM
   )

   # Determine target architecture for dynasm by preprocessing lj_arch.h
   # This mimics the Makefile's TARGET_TESTARCH approach

   set (LUAJIT_DETECT_ARCH "${LUAJIT_BUILD_DIR}/detect_arch.c")

   # Only write the file if it doesn't exist to avoid unnecessary rebuilds

   if (NOT EXISTS ${LUAJIT_DETECT_ARCH})
      file(WRITE ${LUAJIT_DETECT_ARCH}
         "#include \"${LUAJIT_SRC}/lj_arch.h\"\n"
         "#include <stdio.h>\n"
         "#include <string.h>\n"
         "int main(void) {\n"
         "#ifdef LJ_TARGET_X64\n"
         "   #ifdef LJ_FR2\n"
         "   puts(\"x64\");\n"
         "   #else\n"
         "   puts(\"x86\");\n"
         "   #endif\n"
         "#elif defined(LJ_TARGET_X86)\n"
         "   puts(\"x86\");\n"
         "#elif defined(LJ_TARGET_ARM)\n"
         "   puts(\"arm\");\n"
         "#elif defined(LJ_TARGET_ARM64)\n"
         "   puts(\"arm64\");\n"
         "#elif defined(LJ_TARGET_PPC)\n"
         "   puts(\"ppc\");\n"
         "#elif defined(LJ_TARGET_MIPS)\n"
         "   puts(\"mips\");\n"
         "#else\n"
         "   puts(\"x64\");\n"
         "#endif\n"
         "   return 0;\n"
         "}\n"
      )
   endif ()

   # Detect architecture

   set_platform_executable(LUAJIT_ARCH_DETECT "detect_arch")

   add_custom_command(
      OUTPUT ${LUAJIT_ARCH_DETECT}
      COMMAND ${CMAKE_C_COMPILER} -I${LUAJIT_SRC} -o ${LUAJIT_ARCH_DETECT} ${LUAJIT_DETECT_ARCH}
      DEPENDS ${LUAJIT_DETECT_ARCH} "${LUAJIT_SRC}/lj_arch.h"
      COMMENT "Detecting LuaJIT target architecture"
      VERBATIM
   )

   # Generate buildvm_arch.h using dynasm
   # Detect architecture at build time and use the appropriate .dasc file
   # Extract architecture flags from lj_arch.h preprocessing to pass to dynasm

   set (BUILDVM_ARCH_H "${LUAJIT_BUILD_DIR}/buildvm_arch.h")

   # Create a script to preprocess lj_arch.h and extract architecture defines
   set (ARCH_DETECT_SCRIPT "${LUAJIT_BUILD_DIR}/detect_arch_flags.cmake")

   if (NOT EXISTS ${ARCH_DETECT_SCRIPT})
      file(WRITE ${ARCH_DETECT_SCRIPT}
         "# Preprocess lj_arch.h to extract architecture defines\n"
         "# Create a temporary C file that includes lj_arch.h\n"
         "file(WRITE \"${LUAJIT_BUILD_DIR}/test_arch.c\" \"#include <lj_arch.h>\\n\")\n"
         "execute_process(\n"
         "   COMMAND ${CMAKE_C_COMPILER} ${LUAJIT_NON_MSVC_DEFS} -I${LUAJIT_SRC} -E -dM ${LUAJIT_BUILD_DIR}/test_arch.c\n"
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
         "if(ARCH_DEFS MATCHES \"LJ_HASFFI 1\")\n"
         "   list(APPEND DASM_AFLAGS \"-D\" \"FFI\")\n"
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
         "# Extract version\n"
         "if(ARCH_DEFS MATCHES \"LJ_ARCH_VERSION ([0-9]+)\")\n"
         "   list(APPEND DASM_AFLAGS \"-D\" \"VER=\${CMAKE_MATCH_1}\")\n"
         "endif()\n"
         "# Platform-specific flags\n"
         "# iOS flag for ARM platforms\n"
         "if(ARCH_DEFS MATCHES \"__APPLE__\" AND ARCH_DEFS MATCHES \"LJ_ARCH_ARM\")\n"
         "   list(APPEND DASM_AFLAGS \"-D\" \"IOS\")\n"
         "endif()\n"
         "# MIPSR6 flag for MIPS R6 architectures\n"
         "if(ARCH_DEFS MATCHES \"LJ_ARCH_MIPSR6 1\")\n"
         "   list(APPEND DASM_AFLAGS \"-D\" \"MIPSR6\")\n"
         "endif()\n"
         "# SQRT, ROUND, and GPR64 flags for PowerPC\n"
         "if(ARCH_DEFS MATCHES \"LJ_ARCH_PPC 1\")\n"
         "   list(APPEND DASM_AFLAGS \"-D\" \"SQRT\")\n"
         "   list(APPEND DASM_AFLAGS \"-D\" \"ROUND\")\n"
         "   if(ARCH_DEFS MATCHES \"LJ_ARCH_PPC64 1\")\n"
         "      list(APPEND DASM_AFLAGS \"-D\" \"GPR64\")\n"
         "   endif()\n"
         "endif()\n"
         "# PPE and TOC flags for PS3 systems\n"
         "if(ARCH_DEFS MATCHES \"__CELLOS_LV2__\")\n"
         "   list(APPEND DASM_AFLAGS \"-D\" \"PPE\")\n"
         "   list(APPEND DASM_AFLAGS \"-D\" \"TOC\")\n"
         "endif()\n"
         "# Write flags to file\n"
         "string(REPLACE \";\" \" \" DASM_AFLAGS_STR \"\${DASM_AFLAGS}\")\n"
         "file(WRITE \"${LUAJIT_BUILD_DIR}/dasm_aflags.txt\" \"\${DASM_AFLAGS_STR}\")\n"
      )
   endif()

   # Use a CMake script to detect architecture and run dynasm

   set (DYNASM_SCRIPT "${LUAJIT_BUILD_DIR}/run_dynasm.cmake")

   # Only write the script if it doesn't exist to avoid unnecessary rebuilds

   if (NOT EXISTS ${DYNASM_SCRIPT})
      file(WRITE ${DYNASM_SCRIPT}
         "file(READ \"${LUAJIT_BUILD_DIR}/arch.txt\" ARCH)\n"
         "string(STRIP \"\${ARCH}\" ARCH)\n"
         "set(DASM_DASC \"${LUAJIT_SRC}/vm_\${ARCH}.dasc\")\n"
         "if(NOT EXISTS \"\${DASM_DASC}\")\n"
         "   message(FATAL_ERROR \"Architecture file \${DASM_DASC} not found\")\n"
         "endif()\n"
         "# Read dynasm flags\n"
         "file(READ \"${LUAJIT_BUILD_DIR}/dasm_aflags.txt\" DASM_AFLAGS_STR)\n"
         "string(STRIP \"\${DASM_AFLAGS_STR}\" DASM_AFLAGS_STR)\n"
         "if(DASM_AFLAGS_STR)\n"
         "   separate_arguments(DASM_AFLAGS UNIX_COMMAND \"\${DASM_AFLAGS_STR}\")\n"
         "else()\n"
         "   set(DASM_AFLAGS \"\")\n"
         "endif()\n"
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
   endif ()

   add_custom_command(
      OUTPUT ${LUAJIT_BUILD_DIR}/arch.txt
      COMMAND ${LUAJIT_ARCH_DETECT} > ${LUAJIT_BUILD_DIR}/arch.txt
      DEPENDS ${LUAJIT_ARCH_DETECT}
      COMMENT "Writing detected architecture to file"
      VERBATIM
   )

   # Generate dynasm architecture flags file
   add_custom_command(
      OUTPUT ${LUAJIT_BUILD_DIR}/dasm_aflags.txt
      COMMAND ${CMAKE_COMMAND} -P ${ARCH_DETECT_SCRIPT}
      DEPENDS "${LUAJIT_SRC}/lj_arch.h"
      COMMENT "Detecting architecture flags for dynasm"
      VERBATIM
   )

   add_custom_command(
      OUTPUT ${BUILDVM_ARCH_H}
      COMMAND ${CMAKE_COMMAND} -E env TARGET_GENDIR=${LUAJIT_BUILD_DIR} ${CMAKE_COMMAND} -P ${DYNASM_SCRIPT}
      DEPENDS ${MINILUA_TARGET} ${LUAJIT_BUILD_DIR}/arch.txt ${LUAJIT_BUILD_DIR}/dasm_aflags.txt ${LUAJIT_DYNASM_DIR}/dynasm.lua ${DYNASM_SCRIPT}
         "${LUAJIT_SRC}/lj_arch.h" "${LUAJIT_SRC}/lua.h" "${LUAJIT_SRC}/luaconf.h"
         "${LUAJIT_SRC}/vm_x64.dasc" "${LUAJIT_SRC}/vm_x86.dasc" "${LUAJIT_SRC}/vm_arm.dasc"
         "${LUAJIT_SRC}/vm_arm64.dasc" "${LUAJIT_SRC}/vm_ppc.dasc" "${LUAJIT_SRC}/vm_mips.dasc"
      COMMENT "Generating buildvm_arch.h via dynasm"
      VERBATIM
   )

   # Build buildvm host tool

   set (BUILDVM_SOURCES
      "${LUAJIT_HOST_DIR}/buildvm.c"
      "${LUAJIT_HOST_DIR}/buildvm_asm.c"
      "${LUAJIT_HOST_DIR}/buildvm_peobj.c"
      "${LUAJIT_HOST_DIR}/buildvm_lib.c"
      "${LUAJIT_HOST_DIR}/buildvm_fold.c"
   )

   set_platform_executable(BUILDVM_TARGET "buildvm")
   set_platform_math_lib(BUILDVM_LIBS)

   add_custom_command(
      OUTPUT ${BUILDVM_TARGET}
      COMMAND ${CMAKE_C_COMPILER} ${LUAJIT_HOST_CFLAGS}
         ${LUAJIT_NON_MSVC_DEFS}
         ${LUAJIT_INCLUDE_DIRS}
         -o ${BUILDVM_TARGET} ${BUILDVM_SOURCES} ${BUILDVM_LIBS}
      DEPENDS ${BUILDVM_SOURCES} ${BUILDVM_ARCH_H}
      COMMENT "Building buildvm host tool"
      VERBATIM
   )

   # Library source files needed for buildvm header generation
   # Note: lib_ffi.c excluded because FFI is disabled

   set (LJLIB_C
      "${LUAJIT_SRC}/lib_base.c"
      "${LUAJIT_SRC}/lib_math.c"
      "${LUAJIT_SRC}/lib_bit.c"
      "${LUAJIT_SRC}/lib_string.c"
      "${LUAJIT_SRC}/lib_table.c"
      "${LUAJIT_SRC}/lib_debug.c"
      "${LUAJIT_SRC}/lib_jit.c"
      "${LUAJIT_SRC}/lib_buffer.c"
   )

   # Generate headers and VM file using buildvm
   # Windows (MinGW) uses peobj to generate .obj directly like MSVC
   # Unix uses elfasm to generate .S assembly source that gets compiled later

   file(MAKE_DIRECTORY "${LUAJIT_BUILD_DIR}/jit")

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
      COMMAND ${BUILDVM_TARGET} -m folddef -o ${LUAJIT_BUILD_DIR}/lj_folddef.h "${LUAJIT_SRC}/lj_opt_fold.c"
      COMMAND ${BUILDVM_TARGET} -m vmdef -o ${LUAJIT_VMDEF_LUA} ${LJLIB_C}
      COMMAND ${BUILDVM_TARGET} -m ${LUAJIT_VM_MODE} -o ${LUAJIT_VM_OUTPUT}
      DEPENDS ${BUILDVM_TARGET} ${LJLIB_C} "${LUAJIT_SRC}/lj_opt_fold.c"
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
      # Unix: Need to compile the assembly source
      add_custom_target(luajit_codegen
         DEPENDS ${LUAJIT_GENERATED_HEADERS} ${LUAJIT_VM_S} ${LUAJIT_VMDEF_LUA}
      )

      # Compile VM assembly file
      add_custom_command(
         OUTPUT ${LUAJIT_VM_O}
         COMMAND ${CMAKE_C_COMPILER} ${LUAJIT_COMMON_CFLAGS}
            -funwind-tables -DLUAJIT_UNWIND_EXTERNAL
            ${LUAJIT_NON_MSVC_DEFS}
            ${LUAJIT_INCLUDE_DIRS}
            -c ${LUAJIT_VM_S} -o ${LUAJIT_VM_O}
         DEPENDS ${LUAJIT_VM_S}
         COMMENT "Compiling LuaJIT VM assembly"
         VERBATIM
      )
   endif ()

   # Compile amalgamated source
   # List all source files included by ljamalg.c to ensure proper dependency tracking

   file(GLOB LUAJIT_AMALG_SOURCES
      "${LUAJIT_SRC}/lj_*.c"
      "${LUAJIT_SRC}/lib_*.c"
   )

   set (LUAJIT_AMALG_O "${LUAJIT_BUILD_DIR}/ljamalg.o")
   add_custom_command(
      OUTPUT ${LUAJIT_AMALG_O}
      COMMAND ${CMAKE_C_COMPILER} ${LUAJIT_COMMON_CFLAGS} -funwind-tables -DLUAJIT_UNWIND_EXTERNAL
         ${LUAJIT_NON_MSVC_DEFS}
         -DLUA_ROOT=\"${CMAKE_INSTALL_PREFIX}\"
         ${LUAJIT_INCLUDE_DIRS}
         -Wno-trigraphs
         -c "${LUAJIT_SRC}/ljamalg.c" -o ${LUAJIT_AMALG_O}
      DEPENDS "${LUAJIT_SRC}/ljamalg.c" ${LUAJIT_GENERATED_HEADERS} ${LUAJIT_AMALG_SOURCES}
      COMMENT "Compiling LuaJIT amalgamated source"
      VERBATIM
   )

   # Create static library

   set (LUAJIT_LIB_DIR "${CMAKE_BINARY_DIR}/luajit-2.1/lib")
   file(MAKE_DIRECTORY "${LUAJIT_LIB_DIR}")
   set (LUAJIT_LINK "${LUAJIT_LIB_DIR}/libluajit-5.1.a")

   if (WIN32)
      # MinGW: Use the .obj file generated by buildvm
      add_custom_command(
         OUTPUT ${LUAJIT_LINK}
         COMMAND ${CMAKE_AR} rcus ${LUAJIT_LINK} ${LUAJIT_VM_OBJ} ${LUAJIT_AMALG_O}
         DEPENDS ${LUAJIT_VM_OBJ} ${LUAJIT_AMALG_O}
         COMMENT "Creating LuaJIT static library"
         VERBATIM
      )
   else ()
      # Unix: Use the compiled .o file
      add_custom_command(
         OUTPUT ${LUAJIT_LINK}
         COMMAND ${CMAKE_AR} rcus ${LUAJIT_LINK} ${LUAJIT_VM_O} ${LUAJIT_AMALG_O}
         DEPENDS ${LUAJIT_VM_O} ${LUAJIT_AMALG_O}
         COMMENT "Creating LuaJIT static library"
         VERBATIM
      )
   endif ()

   add_custom_target(luajit_lib DEPENDS ${LUAJIT_LINK})
   add_dependencies(luajit_lib luajit_codegen)
   add_custom_target(luajit DEPENDS luajit_lib)  # Create an alias for backward compatibility
endif ()

# Fluid library build

add_library (${MOD})

set_module_defaults (${MOD} "Fl")

add_dependencies (${MOD} libffi-3.3 luajit)

if (BUILD_DEFS)
   add_dependencies (${MOD} ${MOD}_hashes)
endif ()

set (FLUID_SOURCES "${MOD}.cpp" "fluid_module.cpp" "fluid_thread.cpp" "fluid_struct.cpp" "fluid_processing.cpp"
   "fluid_number.cpp" "fluid_functions.cpp" "fluid_objects.cpp" "fluid_array.cpp" "fluid_regex.cpp"
   "fluid_io.cpp" "fluid_class.cpp")

if (NOT DISABLE_DISPLAY)
   list (APPEND FLUID_SOURCES "fluid_input.cpp")
endif ()

target_sources (${MOD} PRIVATE ${FLUID_SOURCES})

target_include_directories (${MOD} PRIVATE
   "${CMAKE_BINARY_DIR}/libffi-3.3/include"
   "luajit-2.1/src")

target_link_libraries (${MOD} PRIVATE
   ${FFI_LINK}
   ${LUAJIT_LINK}
   ${MATH_LINK}) # The link order matters, math must come last

add_compile_definitions ("LUAJIT_ENABLE_LUA52COMPAT")

if (DISABLE_DISPLAY)
   target_compile_definitions (${MOD} PRIVATE DISABLE_DISPLAY)
endif ()

# Register Flute tests

set (FLUID_TESTS
   catch defer debuglog io object processing regex struct threads xml strings math array compound
   bitshift bitwise if_empty presence blank safe_nav ternary table
)

foreach (TEST_NAME ${FLUID_TESTS})
   flute_test(fluid_${TEST_NAME} "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_${TEST_NAME}.fluid")
endforeach ()
