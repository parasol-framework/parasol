set (MOD "fluid")
set (INC_MOD_FLUID TRUE PARENT_SCOPE)

# IDL processing for headers and documentation

idl_c ("hashes.fdl" NAME ${MOD}_hashes OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/hashes.h")
idl_c ("${MOD}.fdl" NAME ${MOD}_defs OUTPUT "${INCLUDE_OUTPUT}/modules/${MOD}.h" FILES "${MOD}.cpp")

set (LUAJIT_SRC "${CMAKE_CURRENT_SOURCE_DIR}/luajit-2.1/src")

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

add_subdirectory (luajit-2.1)

add_library (${MOD})

set_module_defaults (${MOD})

add_dependencies (${MOD} libffi-3.3)

target_sources (${MOD} PRIVATE "${MOD}.cpp" "fluid_module.cpp" "fluid_thread.cpp" "fluid_struct.cpp" "fluid_processing.cpp"
   "fluid_number.cpp" "fluid_input.cpp" "fluid_functions.cpp" "fluid_objects.cpp" "fluid_array.cpp"
   "fluid_class.cpp")

target_include_directories (${MOD} PRIVATE
   "${CMAKE_BINARY_DIR}/libffi-3.3/include"
   "luajit-2.1/src")

target_link_libraries (${MOD} PRIVATE
   "${CMAKE_BINARY_DIR}/libffi-3.3/lib/liblibffi.a"
   luajit
   ${MATH_LINK}) # The link order matters, math must come last

add_compile_definitions ("LUAJIT_ENABLE_LUA52COMPAT")

flute_test(fluid_catch "${CMAKE_CURRENT_SOURCE_DIR}/tests/catch.fluid")
flute_test(fluid_nz "${CMAKE_CURRENT_SOURCE_DIR}/tests/nz.fluid")
flute_test(fluid_object "${CMAKE_CURRENT_SOURCE_DIR}/tests/object.fluid")
flute_test(fluid_processing "${CMAKE_CURRENT_SOURCE_DIR}/tests/processing.fluid")
flute_test(fluid_struct "${CMAKE_CURRENT_SOURCE_DIR}/tests/struct.fluid")
flute_test(fluid_threads "${CMAKE_CURRENT_SOURCE_DIR}/tests/action_threads.fluid")
flute_test(fluid_xml "${CMAKE_CURRENT_SOURCE_DIR}/tests/to_xml.fluid")
