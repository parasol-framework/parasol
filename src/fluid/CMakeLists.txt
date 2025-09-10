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
   set (LUAJIT_LINK "${LUAJIT_SRC}/lua51.lib")

   # NOTE: Binaries are built amongst the source files when building with MSVC

   ExternalProject_Add(
      luajit
      SOURCE_DIR ${LUAJIT_SRC}
      CONFIGURE_COMMAND ""
      BUILD_IN_SOURCE TRUE
      BUILD_ALWAYS TRUE
      BUILD_COMMAND cmd /c "${LUAJIT_SRC}/msvcbuild.bat" "static"
      BUILD_BYPRODUCTS ${LUAJIT_LINK}
      INSTALL_COMMAND ""
      UPDATE_COMMAND ""
      COMMENT "Compiling LuaJIT library")
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

target_sources (${MOD} PRIVATE "${MOD}.cpp" "fluid_module.cpp" "fluid_thread.cpp" "fluid_struct.cpp" "fluid_processing.cpp"
   "fluid_number.cpp" "fluid_input.cpp" "fluid_functions.cpp" "fluid_objects.cpp" "fluid_array.cpp" "fluid_regex.cpp" "fluid_io.cpp"
   "fluid_class.cpp")

target_include_directories (${MOD} PRIVATE
   "${CMAKE_BINARY_DIR}/libffi-3.3/include"
   "luajit-2.1/src")

target_link_libraries (${MOD} PRIVATE
   ${FFI_LINK}
   ${LUAJIT_LINK}
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