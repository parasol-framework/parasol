set (MOD "fluid")
set (INC_MOD_FLUID TRUE PARENT_SCOPE)

# IDL processing for headers and documentation

idl_gen ("hashes.fdl" NAME ${MOD}_hashes OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/hashes.h")
idl_gen ("${MOD}.fdl" NAME ${MOD}_defs OUTPUT "${INCLUDE_OUTPUT}/modules/${MOD}.h" FILES "${MOD}.cpp")

# We build libFFI as a release build in all situations because ASAN doesn't like Debug builds of libFFI.
# Activate BUILD_ALWAYS if you're working on the library code.

set (LUAJIT_ORIGINAL_BUILD TRUE)

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

   add_custom_target(luajit_codegen
      DEPENDS ${LUAJIT_GENERATED_HEADERS} ${LUAJIT_VM_OBJ}
   )

   # Collect all LuaJIT source files
   file(GLOB LUAJIT_CORE_SOURCES "${LUAJIT_SRC}/lj_*.c")
   file(GLOB LUAJIT_LIB_SOURCES "${LUAJIT_SRC}/lib_*.c")

   # Create LuaJIT static library
   add_library(luajit_lib STATIC
      ${LUAJIT_CORE_SOURCES}
      ${LUAJIT_LIB_SOURCES}
      ${LUAJIT_VM_OBJ}
   )

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

elseif (LUAJIT_ORIGINAL_BUILD)
   # Build using the original LuaJIT build system (Makefile)

   set (LUAJIT_BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}/luajit-generated")
   file(MAKE_DIRECTORY "${LUAJIT_BUILD_DIR}")

   ExternalProject_Add(
      luajit
      SOURCE_DIR ${LUAJIT_SRC}
      CONFIGURE_COMMAND ""
      BUILD_ALWAYS TRUE
      BUILD_COMMAND cd "${CMAKE_CURRENT_SOURCE_DIR}/luajit-2.1/src" && "${CMAKE_MAKE_PROGRAM}" "amalg" "CFLAGS=-fPIC ${CFLAGS}" "TARGET_GENDIR=${LUAJIT_BUILD_DIR}"
      INSTALL_COMMAND cd "${CMAKE_CURRENT_SOURCE_DIR}/luajit-2.1" && "${CMAKE_MAKE_PROGRAM}" "install" "DESTDIR=${CMAKE_BINARY_DIR}/luajit-2.1" "PREFIX=" "TARGET_GENDIR=${LUAJIT_BUILD_DIR}"
      BUILD_BYPRODUCTS "${CMAKE_BINARY_DIR}/luajit-2.1/lib/libluajit-5.1.a"
      # Specifying the WORKING_DIRECTORY triggers a bug in GitHub's auto-build as of 2024-11 (and we don't really need it in order to build)
      #WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/luajit-2.1"
      COMMENT "Compiling LuaJIT library")

   set (LUAJIT_LINK "${CMAKE_BINARY_DIR}/luajit-2.1/lib/libluajit-5.1.a")
else ()
   # Non-MSVC build (Linux, MinGW, etc.)
   # Separated into code generation and library compilation
   # Code generation creates headers and lj_vm.S via host tools

   set (LUAJIT_VM_S "${LUAJIT_BUILD_DIR}/lj_vm.S")
   set (LUAJIT_VM_O "${LUAJIT_BUILD_DIR}/lj_vm.o")

   # Host tool sources

   set (LUAJIT_HOST_DIR "${LUAJIT_SRC}/host")
   set (LUAJIT_DYNASM_DIR "${CMAKE_CURRENT_SOURCE_DIR}/luajit-2.1/dynasm")

   # Build minilua host tool first

   set (MINILUA_SRC "${LUAJIT_HOST_DIR}/minilua.c")

   if (WIN32)
      set (MINILUA_TARGET "${LUAJIT_BUILD_DIR}/minilua.exe")
      set (MINILUA_LIBS "")
   else ()
      set (MINILUA_TARGET "${LUAJIT_BUILD_DIR}/minilua")
      set (MINILUA_LIBS "-lm")
   endif ()

   add_custom_command(
      OUTPUT ${MINILUA_TARGET}
      COMMAND ${CMAKE_C_COMPILER} -std=c99 -O2 -Wall -o ${MINILUA_TARGET} ${MINILUA_SRC} ${MINILUA_LIBS}
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

   if (WIN32)
      set (LUAJIT_ARCH_DETECT "${LUAJIT_BUILD_DIR}/detect_arch.exe")
   else ()
      set (LUAJIT_ARCH_DETECT "${LUAJIT_BUILD_DIR}/detect_arch")
   endif ()

   add_custom_command(
      OUTPUT ${LUAJIT_ARCH_DETECT}
      COMMAND ${CMAKE_C_COMPILER} -I${LUAJIT_SRC} -o ${LUAJIT_ARCH_DETECT} ${LUAJIT_DETECT_ARCH}
      DEPENDS ${LUAJIT_DETECT_ARCH} "${LUAJIT_SRC}/lj_arch.h"
      COMMENT "Detecting LuaJIT target architecture"
      VERBATIM
   )

   # Generate buildvm_arch.h using dynasm
   # Detect architecture at build time and use the appropriate .dasc file
   # The dynasm step uses minimal flags - architecture-specific flags are handled by dynasm

   set (BUILDVM_ARCH_H "${LUAJIT_BUILD_DIR}/buildvm_arch.h")

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
         "execute_process(\n"
         "   COMMAND \"${MINILUA_TARGET}\" \"${LUAJIT_DYNASM_DIR}/dynasm.lua\" -o \"${BUILDVM_ARCH_H}\" \"\${DASM_DASC}\"\n"
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

   add_custom_command(
      OUTPUT ${BUILDVM_ARCH_H}
      COMMAND ${CMAKE_COMMAND} -E env TARGET_GENDIR=${LUAJIT_BUILD_DIR} ${CMAKE_COMMAND} -P ${DYNASM_SCRIPT}
      DEPENDS ${MINILUA_TARGET} ${LUAJIT_BUILD_DIR}/arch.txt ${LUAJIT_DYNASM_DIR}/dynasm.lua
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

   if (WIN32)
      set (BUILDVM_TARGET "${LUAJIT_BUILD_DIR}/buildvm.exe")
      set (BUILDVM_LIBS "")
   else ()
      set (BUILDVM_TARGET "${LUAJIT_BUILD_DIR}/buildvm")
      set (BUILDVM_LIBS "-lm")
   endif ()

   add_custom_command(
      OUTPUT ${BUILDVM_TARGET}
      COMMAND ${CMAKE_C_COMPILER} -std=c99 -O2 -Wall
         ${LUAJIT_NON_MSVC_DEFS}
         -I${LUAJIT_SRC} -I${LUAJIT_BUILD_DIR}
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

   # Generate headers and VM assembly file using buildvm
   # Determine assembly format: coffasm for Windows (MinGW/MSVC), elfasm for Unix

   if (WIN32)
      set (LUAJIT_ASM_MODE "coffasm")
   else ()
      set (LUAJIT_ASM_MODE "elfasm")
   endif ()

   file(MAKE_DIRECTORY "${LUAJIT_BUILD_DIR}/jit")
   add_custom_command(
      OUTPUT ${LUAJIT_GENERATED_HEADERS} ${LUAJIT_VM_S} ${LUAJIT_VMDEF_LUA}
      COMMAND ${BUILDVM_TARGET} -m bcdef -o ${LUAJIT_BUILD_DIR}/lj_bcdef.h ${LJLIB_C}
      COMMAND ${BUILDVM_TARGET} -m ffdef -o ${LUAJIT_BUILD_DIR}/lj_ffdef.h ${LJLIB_C}
      COMMAND ${BUILDVM_TARGET} -m libdef -o ${LUAJIT_BUILD_DIR}/lj_libdef.h ${LJLIB_C}
      COMMAND ${BUILDVM_TARGET} -m recdef -o ${LUAJIT_BUILD_DIR}/lj_recdef.h ${LJLIB_C}
      COMMAND ${BUILDVM_TARGET} -m folddef -o ${LUAJIT_BUILD_DIR}/lj_folddef.h "${LUAJIT_SRC}/lj_opt_fold.c"
      COMMAND ${BUILDVM_TARGET} -m vmdef -o ${LUAJIT_VMDEF_LUA} ${LJLIB_C}
      COMMAND ${BUILDVM_TARGET} -m ${LUAJIT_ASM_MODE} -o ${LUAJIT_VM_S}
      DEPENDS ${BUILDVM_TARGET} ${LJLIB_C} "${LUAJIT_SRC}/lj_opt_fold.c"
      WORKING_DIRECTORY ${LUAJIT_SRC}
      COMMENT "Generating LuaJIT headers and VM assembly via buildvm"
      VERBATIM
   )

   add_custom_target(luajit_codegen
      DEPENDS ${LUAJIT_GENERATED_HEADERS} ${LUAJIT_VM_S} ${LUAJIT_VMDEF_LUA}
   )

   # Compile VM assembly file

   add_custom_command(
      OUTPUT ${LUAJIT_VM_O}
      COMMAND ${CMAKE_C_COMPILER} -fPIC -O2 -fomit-frame-pointer -Wall
         -funwind-tables -DLUAJIT_UNWIND_EXTERNAL
         ${LUAJIT_NON_MSVC_DEFS}
         -I${LUAJIT_SRC} -I${LUAJIT_BUILD_DIR}
         -c ${LUAJIT_VM_S} -o ${LUAJIT_VM_O}
      DEPENDS ${LUAJIT_VM_S}
      COMMENT "Compiling LuaJIT VM assembly"
      VERBATIM
   )

   # Compile amalgamated source

   set (LUAJIT_AMALG_O "${LUAJIT_BUILD_DIR}/ljamalg.o")
   add_custom_command(
      OUTPUT ${LUAJIT_AMALG_O}
      COMMAND ${CMAKE_C_COMPILER} -fPIC -O2 -fomit-frame-pointer -Wall -funwind-tables -DLUAJIT_UNWIND_EXTERNAL
         ${LUAJIT_NON_MSVC_DEFS}
         -DLUA_ROOT=\"${CMAKE_INSTALL_PREFIX}\"
         -I${LUAJIT_SRC} -I${LUAJIT_BUILD_DIR}
         -c "${LUAJIT_SRC}/ljamalg.c" -o ${LUAJIT_AMALG_O}
      DEPENDS "${LUAJIT_SRC}/ljamalg.c" ${LUAJIT_GENERATED_HEADERS}
      COMMENT "Compiling LuaJIT amalgamated source"
      VERBATIM
   )

   # Create static library

   set (LUAJIT_LIB_DIR "${CMAKE_BINARY_DIR}/luajit-2.1/lib")
   file(MAKE_DIRECTORY "${LUAJIT_LIB_DIR}")
   set (LUAJIT_LINK "${LUAJIT_LIB_DIR}/libluajit-5.1.a")
   add_custom_command(
      OUTPUT ${LUAJIT_LINK}
      COMMAND ${CMAKE_AR} rcus ${LUAJIT_LINK} ${LUAJIT_VM_O} ${LUAJIT_AMALG_O}
      DEPENDS ${LUAJIT_VM_O} ${LUAJIT_AMALG_O}
      COMMENT "Creating LuaJIT static library"
      VERBATIM
   )

   add_custom_target(luajit_lib DEPENDS ${LUAJIT_LINK})
   add_dependencies(luajit_lib luajit_codegen)

   # Create an alias for backward compatibility
   add_custom_target(luajit DEPENDS luajit_lib)
endif ()

# Fluid library build

add_library (${MOD})

set_module_defaults (${MOD} "Fl")

add_dependencies (${MOD} libffi-3.3 luajit)

if (BUILD_DEFS)
   add_dependencies (${MOD} ${MOD}_hashes)
endif ()

set (FLUID_SOURCES
   "${MOD}.cpp"
   "fluid_module.cpp"
   "fluid_thread.cpp"
   "fluid_struct.cpp"
   "fluid_processing.cpp"
   "fluid_number.cpp"
   "fluid_functions.cpp"
   "fluid_objects.cpp"
   "fluid_array.cpp"
   "fluid_regex.cpp"
   "fluid_io.cpp"
   "fluid_class.cpp")

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

flute_test(fluid_catch "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_catch.fluid")
flute_test(fluid_io "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_io.fluid")
flute_test(fluid_nz "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_nz.fluid")
flute_test(fluid_object "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_object.fluid")
flute_test(fluid_processing "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_processing.fluid")
flute_test(fluid_regex "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_regex.fluid")
flute_test(fluid_struct "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_struct.fluid")
flute_test(fluid_threads "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_action_threads.fluid")
flute_test(fluid_xml "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_to_xml.fluid")
flute_test(fluid_strings "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_strings.fluid")
flute_test(fluid_math "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_math.fluid")
flute_test(fluid_array "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_array.fluid")
flute_test(fluid_compound "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_compound.fluid")
flute_test(fluid_bitshift "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_bitshift.fluid")
flute_test(fluid_bitwise "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_bitwise.fluid")
flute_test(fluid_orq "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_orq.fluid")
flute_test(fluid_presence "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_presence.fluid")
