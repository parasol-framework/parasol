set (MOD "fluid")
set (INC_MOD_FLUID TRUE PARENT_SCOPE)

# IDL processing for headers and documentation

idl_gen ("hashes.fdl" NAME ${MOD}_hashes OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/hashes.h")
idl_gen ("${MOD}.fdl" NAME ${MOD}_defs OUTPUT "${INCLUDE_OUTPUT}/modules/${MOD}.h" FILES "${MOD}.cpp")

set (LUAJIT_SRC "${CMAKE_CURRENT_SOURCE_DIR}/luajit-2.1/src")
#link_directories (${LUAJIT_SRC})

# We build FFI as a release build in all situations because ASAN doesn't like Debug builds of FFI.
# Activate BUILD_ALWAYS if you're working on the library code.

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

if (MSVC)
   # LuaJIT build for MSVC - separated into code generation and library compilation
   # Code generation creates headers and lj_vm.obj via custom batch script
   # Library compilation is handled by CMake for proper incremental builds

   set (LUAJIT_GENERATED_HEADERS
      "${LUAJIT_SRC}/lj_bcdef.h"
      "${LUAJIT_SRC}/lj_ffdef.h"
      "${LUAJIT_SRC}/lj_libdef.h"
      "${LUAJIT_SRC}/lj_recdef.h"
      "${LUAJIT_SRC}/lj_folddef.h"
      "${LUAJIT_SRC}/jit/vmdef.lua"
   )

   set (LUAJIT_VM_OBJ "${LUAJIT_SRC}/lj_vm.obj")

   # Expand glob patterns for dependencies (exclude generated headers to avoid circular deps)
   file(GLOB LUAJIT_DEP_LJ_C "${LUAJIT_SRC}/lj_*.c")
   file(GLOB LUAJIT_DEP_LIB_C "${LUAJIT_SRC}/lib_*.c")
   file(GLOB LUAJIT_DEP_LJ_H "${LUAJIT_SRC}/lj_*.h")

   # Remove generated headers from dependency list to avoid circular dependency
   list(REMOVE_ITEM LUAJIT_DEP_LJ_H
      "${LUAJIT_SRC}/lj_bcdef.h"
      "${LUAJIT_SRC}/lj_ffdef.h"
      "${LUAJIT_SRC}/lj_libdef.h"
      "${LUAJIT_SRC}/lj_recdef.h"
      "${LUAJIT_SRC}/lj_folddef.h"
   )

   # Generate headers and VM object file using buildvm
   add_custom_command(
      OUTPUT ${LUAJIT_GENERATED_HEADERS} ${LUAJIT_VM_OBJ}
      COMMAND cmd /c "${LUAJIT_SRC}/msvcbuild_codegen.bat"
      WORKING_DIRECTORY "${LUAJIT_SRC}"
      DEPENDS
         ${LUAJIT_DEP_LJ_C}
         ${LUAJIT_DEP_LIB_C}
         ${LUAJIT_DEP_LJ_H}
      COMMENT "Generating LuaJIT headers and VM object"
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
   target_compile_definitions(luajit_lib PRIVATE LUAJIT_ENABLE_LUA52COMPAT _CRT_SECURE_NO_DEPRECATE)

   target_include_directories(luajit_lib PRIVATE "${LUAJIT_SRC}")

   # Disable specific warnings from LuaJIT
   target_compile_options(luajit_lib PRIVATE /wd4244 /wd5287)

   set (LUAJIT_LINK "${LUAJIT_SRC}/lua51.lib")

   # Create an alias for backward compatibility
   add_custom_target(luajit DEPENDS luajit_lib)
else ()
   ExternalProject_Add(
      luajit
      SOURCE_DIR ${LUAJIT_SRC}
      CONFIGURE_COMMAND ""
      BUILD_ALWAYS TRUE
      BUILD_COMMAND cd "${CMAKE_CURRENT_SOURCE_DIR}/luajit-2.1" && "${CMAKE_MAKE_PROGRAM}" "amalg" "CFLAGS=-fPIC ${CFLAGS}" "DESTDIR=${CMAKE_BINARY_DIR}/luajit-2.1"
      INSTALL_COMMAND cd "${CMAKE_CURRENT_SOURCE_DIR}/luajit-2.1" && "${CMAKE_MAKE_PROGRAM}" "install" "DESTDIR=${CMAKE_BINARY_DIR}/luajit-2.1" "PREFIX="
      BUILD_BYPRODUCTS "${CMAKE_BINARY_DIR}/luajit-2.1/lib/libluajit-5.1.a"
      # Specifying the WORKING_DIRECTORY triggers a bug in GitHub's auto-build as of 2024-11 (and we don't really need it in order to build)
      #WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/luajit-2.1"
      COMMENT "Compiling LuaJIT library")

   set (LUAJIT_LINK "${CMAKE_BINARY_DIR}/luajit-2.1/lib/libluajit-5.1.a")
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
