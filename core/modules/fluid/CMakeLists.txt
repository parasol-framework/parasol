# Fluid has a dependency on the installation of libffi, found in the 3rdparty folder.

set (MOD "fluid")

# IDL processing for headers and documentation

idl_c ("hashes.fdl" NAME ${MOD}_hashes OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/hashes.h")
idl_c ("${MOD}.fdl" NAME ${MOD}_defs OUTPUT "${INCLUDE_OUTPUT}/modules/${MOD}.h" FILES "${MOD}.c")

link_directories("${CMAKE_CURRENT_SOURCE_DIR}/libffi-3.3/release/lib" "${CMAKE_CURRENT_SOURCE_DIR}/luajit-2.0.5/src")

# FFI library custom build

add_custom_command (OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/libffi-3.3"
   PRE_BUILD
   COMMAND "rm" "-fr" "libffi-3.3"
   COMMAND "tar" "-zxf" "libffi-3.3.tar.gz"
   COMMAND "touch" "libffi-3.3"
   WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
   COMMENT "Decompressing FFI archive"
   VERBATIM)

add_custom_command (OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/libffi-3.3/release/lib/libffi.a"
   PRE_BUILD
   COMMAND "sh" "configure"
      "--disable-dependency-tracking"
      "--prefix=${CMAKE_CURRENT_SOURCE_DIR}/libffi-3.3/release"
      "--includedir=${CMAKE_CURRENT_SOURCE_DIR}/libffi-3.3/release/include"
   COMMAND "${CMAKE_MAKE_PROGRAM}" "install"
   WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/libffi-3.3"
   COMMENT "Compiling FFI library"
   VERBATIM)

add_custom_target(ffi_build DEPENDS "libffi-3.3" "${CMAKE_CURRENT_SOURCE_DIR}/libffi-3.3/release/lib/libffi.a")

# LuaJIT library custom build

add_custom_command (OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/luajit-2.0.5/src/libluajit-${CMAKE_HOST_SYSTEM_PROCESSOR}.a"
   PRE_BUILD
   COMMAND "${CMAKE_MAKE_PROGRAM}" "CFLAGS=-fPIC"
   COMMAND "mv" "${CMAKE_CURRENT_SOURCE_DIR}/luajit-2.0.5/src/libluajit.a" "${CMAKE_CURRENT_SOURCE_DIR}/luajit-2.0.5/src/libluajit-${CMAKE_HOST_SYSTEM_PROCESSOR}.a"
   WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/luajit-2.0.5"
   COMMENT "Compiling LuaJIT library"
   VERBATIM)

add_custom_target (lua_build DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/luajit-2.0.5/src/libluajit-${CMAKE_HOST_SYSTEM_PROCESSOR}.a")

# Fluid library build

add_library (${MOD} SHARED)

set_module_defaults (${MOD})

add_dependencies (${MOD} ffi_build lua_build)

target_sources (${MOD} PRIVATE "${MOD}.c")

target_include_directories (${MOD} PRIVATE
   "${CMAKE_CURRENT_SOURCE_DIR}/libffi-3.3/release/include"
   "luajit-2.0.5/src")

target_link_libraries (${MOD} PRIVATE
   "${CMAKE_CURRENT_SOURCE_DIR}/libffi-3.3/release/lib/libffi.a"
   "${CMAKE_CURRENT_SOURCE_DIR}/luajit-2.0.5/src/libluajit-${CMAKE_HOST_SYSTEM_PROCESSOR}.a"
   m) # The link order matters, math must come last

add_definitions ("-DLUAJIT_ENABLE_LUA52COMPAT")

flute_test(fluid_catch "${CMAKE_CURRENT_SOURCE_DIR}/tests/catch.fluid")
flute_test(fluid_threads "${CMAKE_CURRENT_SOURCE_DIR}/tests/action_threads.fluid")
flute_test(fluid_nz "${CMAKE_CURRENT_SOURCE_DIR}/tests/nz.fluid")
flute_test(fluid_object "${CMAKE_CURRENT_SOURCE_DIR}/tests/object.fluid")
flute_test(fluid_struct "${CMAKE_CURRENT_SOURCE_DIR}/tests/struct.fluid")
#flute_test(fluid_calls "${CMAKE_CURRENT_SOURCE_DIR}/tests/module_calls.fluid")
