set (MOD "fluid")
set (INC_MOD_FLUID TRUE PARENT_SCOPE)

# IDL processing for headers and documentation

idl_gen ("hashes.fdl" NAME ${MOD}_hashes OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/hashes.h")
idl_gen ("${MOD}.fdl" NAME ${MOD}_defs OUTPUT "${INCLUDE_OUTPUT}/modules/${MOD}.h" FILES "${MOD}.cpp")

# LuaJIT is now built via CMake (subdirectory) rather than shell scripts.
#
# Windows + Ninja note:
# - When using the Ninja generator with MSVC on Windows, the environment must
#   provide the Visual C++ toolchain INCLUDE/LIB/SDK paths (e.g., by running
#   inside a Visual Studio Developer Prompt or after calling vcvars*.bat).
# - If these environment variables are not initialised, cl.exe may fail to
#   find standard headers when compiling C sources in the LuaJIT subproject
#   (e.g., host/minilua.c can error: cannot open include file 'stddef.h').
# - Recommended on Windows: use the Visual Studio generator, e.g.
#     cmake -S . -B build/agents-vs -G "Visual Studio 17 2022" -A x64
#   Alternatively, keep Ninja but run CMake from a VS Developer shell so the
#   MSVC environment is active.
add_subdirectory(luajit-2.1)

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

# LuaJIT target is provided by the subdirectory as target 'luajit' (OUTPUT_NAME lua51)

# Fluid library build

add_library (${MOD})

set_module_defaults (${MOD} "Fl")

add_dependencies (${MOD} libffi-3.3 luajit)

target_sources (${MOD} PRIVATE "${MOD}.cpp" "fluid_module.cpp" "fluid_thread.cpp" "fluid_struct.cpp" "fluid_processing.cpp"
   "fluid_number.cpp" "fluid_input.cpp" "fluid_functions.cpp" "fluid_objects.cpp" "fluid_array.cpp" "fluid_regex.cpp" "fluid_io.cpp"
   "fluid_class.cpp")

target_include_directories (${MOD} PRIVATE
   "${CMAKE_BINARY_DIR}/libffi-3.3/include")

target_link_libraries (${MOD} PRIVATE
   ${FFI_LINK}
   luajit
   ${MATH_LINK}) # The link order matters, math must come last

add_compile_definitions ("LUAJIT_ENABLE_LUA52COMPAT")

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
