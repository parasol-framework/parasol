# Fluid has a dependency on the installation of libffi, found in the 3rdparty folder.

set (MOD "fluid")

# IDL processing for headers and documentation

idl_c ("hashes.fdl" NAME ${MOD}_hashes OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/hashes.h")
idl_c ("${MOD}.fdl" NAME ${MOD}_defs OUTPUT "${INCLUDE_OUTPUT}/modules/${MOD}.h" FILES "${MOD}.c")

link_directories("${CMAKE_BINARY_DIR}/libffi-3.3/lib" "${CMAKE_CURRENT_SOURCE_DIR}/luajit-2.0.5/src")

# FFI library custom build

ExternalProject_Add(
   libffi
   SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libffi-3.3"
   CONFIGURE_COMMAND "sh" "${CMAKE_CURRENT_SOURCE_DIR}/libffi-3.3/configure"
      "--srcdir=${CMAKE_CURRENT_SOURCE_DIR}/libffi-3.3"
      "--disable-dependency-tracking"
      "--disable-docs"
      "--prefix=${CMAKE_BINARY_DIR}/libffi-3.3"
      "--includedir=${CMAKE_BINARY_DIR}/libffi-3.3/include"
      "CFLAGS=${CFLAGS} -fPIC" # fPIC has to be forced because some Linux environments need it for the static build
   BUILD_COMMAND "${CMAKE_MAKE_PROGRAM}"
   INSTALL_COMMAND "${CMAKE_MAKE_PROGRAM}" "install"
   BUILD_BYPRODUCTS "${CMAKE_BINARY_DIR}/libffi-3.3/lib/libffi.a"
   WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/libffi-3.3"
   COMMENT "Compiling FFI library")

# LuaJIT library custom build

ExternalProject_Add(
   luajit
   SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/luajit-2.0.5/src"
   CONFIGURE_COMMAND ""
   BUILD_COMMAND cd "${CMAKE_CURRENT_SOURCE_DIR}/luajit-2.0.5" && "${CMAKE_MAKE_PROGRAM}" "amalg" "CFLAGS=-fPIC" "DESTDIR=${CMAKE_BINARY_DIR}/luajit-2.0.5"
   INSTALL_COMMAND cd "${CMAKE_CURRENT_SOURCE_DIR}/luajit-2.0.5" && "${CMAKE_MAKE_PROGRAM}" "install" "DESTDIR=${CMAKE_BINARY_DIR}/luajit-2.0.5" "PREFIX="
   BUILD_BYPRODUCTS "${CMAKE_BINARY_DIR}/luajit-2.0.5/lib/libluajit-5.1.a"
   WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/luajit-2.0.5"
   COMMENT "Compiling LuaJIT library")

# Fluid library build

add_library (${MOD} SHARED)

set_module_defaults (${MOD})

add_dependencies (${MOD} libffi luajit)

target_sources (${MOD} PRIVATE "${MOD}.c" "fluid_module.c" "fluid_thread.cpp" "fluid_struct.c"
   "fluid_number.cpp" "fluid_input.cpp" "fluid_functions.c" "fluid_objects.c" "fluid_array.cpp" "fluid_class.c")

target_include_directories (${MOD} PRIVATE
   "${CMAKE_BINARY_DIR}/libffi-3.3/include"
   "luajit-2.0.5/src")

target_link_libraries (${MOD} PRIVATE
   "${CMAKE_BINARY_DIR}/libffi-3.3/lib/libffi.a"
   "${CMAKE_BINARY_DIR}/luajit-2.0.5/lib/libluajit-5.1.a"
   m) # The link order matters, math must come last

add_definitions ("-DLUAJIT_ENABLE_LUA52COMPAT")

flute_test(fluid_catch "${CMAKE_CURRENT_SOURCE_DIR}/tests/catch.fluid")
flute_test(fluid_threads "${CMAKE_CURRENT_SOURCE_DIR}/tests/action_threads.fluid")
flute_test(fluid_nz "${CMAKE_CURRENT_SOURCE_DIR}/tests/nz.fluid")
flute_test(fluid_object "${CMAKE_CURRENT_SOURCE_DIR}/tests/object.fluid")
flute_test(fluid_struct "${CMAKE_CURRENT_SOURCE_DIR}/tests/struct.fluid")
#flute_test(fluid_calls "${CMAKE_CURRENT_SOURCE_DIR}/tests/module_calls.fluid")
