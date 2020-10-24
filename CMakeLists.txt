# Parasol Core Build File
# Prepare: cmake -S . -B release -DCMAKE_BUILD_TYPE=Release -G"MinGW Makefiles"
#          cmake -S . -B debug -DCMAKE_BUILD_TYPE=Debug -G"MinGW Makefiles"
# Build:   cmake --build release -j 8 -- -O
# Install: sudo cmake --install release
# Test:    cd release && ctest

cmake_minimum_required(VERSION 3.13)

include (CheckIncludeFiles)
include (CheckTypeSize)
include (CheckFunctionExists)
include (CheckSymbolExists)
include (CheckCSourceCompiles)
include (CheckStructHasMember)
include (CheckLibraryExists)
include (TestBigEndian)
include (ExternalProject)

if (CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
   include(CTest)
endif()

project (Parasol LANGUAGES C CXX)

set (VERSION "2020.10")
cmake_policy (SET CMP0082 NEW) # Process install commands in the order that they appear

option (PARASOL_STATIC   "Build as a static library" OFF)
option (PARASOL_SHARED   "Build as a shared library" ON)
option (PARASOL_INSTALL  "Create installation targets (chain builders may want to disable this)" ON)
option (PARASOL_VLOG     "Enable verbose log messages in the generated binaries" OFF)
if (UNIX)
   option (RUN_ANYWHERE  "Build a framework that can run directly from the main folder.  Typically used if redefining CMAKE_INSTALL_PREFIX" OFF)
else ()
   set (RUN_ANYWHERE ON)
endif ()
option (BUILD_TESTS      "Build tests" ON)
option (BUILD_DEFS       "Generate IDL files, C/C++ headers and documentation" ON)
option (DISABLE_SSL      "Disable built-in SSL support even if available on this system" OFF)
option (DISABLE_X11      "Disable X11 even if available on this system" OFF)
option (INSTALL_EXAMPLES "Install the example scripts, typically for compiling a distribution" OFF)

set (CMAKE_CXX_STANDARD 11)
set (CMAKE_CXX_STANDARD_REQUIRED True)
set (CMAKE_POSITION_INDEPENDENT_CODE ON) # Enables -fPIC and -fPIE in gcc

set(BUILD_RPATH_USE_ORIGIN TRUE)

set (INCLUDE_OUTPUT "${PROJECT_SOURCE_DIR}/core/include/parasol")
set (DOCS "${PROJECT_SOURCE_DIR}/docs")

if (RUN_ANYWHERE)
   set (PARASOL_ROOT "${CMAKE_INSTALL_PREFIX}")
   set (SHARE_TARGET .)
   set (LIB_TARGET "lib")
else ()
   set (PARASOL_ROOT "${CMAKE_INSTALL_PREFIX}/share/parasol")
   set (SHARE_TARGET "share/parasol")
   set (LIB_TARGET "lib/parasol")
endif ()

test_big_endian (SYSTEM_ENDIAN_STYLE)
if (SYSTEM_ENDIAN_STYLE EQUAL 0) # Little endian (Intel)
   set (REVERSE_BYTEORDER TRUE)
   set (LITTLE_ENDIAN TRUE)
endif ()

if (NOT DISABLE_SSL)
   set (OPENSSL_USE_STATIC_LIBS TRUE) # Doesn't work as expected in MinGW env.
   include (FindOpenSSL)
endif ()

if (NOT DISABLE_X11)
   include (FindX11)
endif ()

if (MINGW) # Undo the use of 'lib' as the default prefix set by MinGW
   set(CMAKE_SHARED_LIBRARY_PREFIX "")
   set(CMAKE_STATIC_LIBRARY_PREFIX "")
endif ()

include_directories("${PROJECT_SOURCE_DIR}/core/include"
   "${PROJECT_SOURCE_DIR}/core/include/parasol/3rdparty"
   "${CMAKE_CURRENT_BINARY_DIR}")

	CHECK_C_SOURCE_COMPILES ("
#include <stdio.h>
int main() {
   printf(\"%lld\", 0x1122334455667788);
   return 0;
}"
	PRINTF64_LLD)

	CHECK_C_SOURCE_COMPILES ("
#include <stdio.h>
int main() {
   printf(\"%I64d\", 0x1122334455667788);
   return 0;
}"
	PRINTF64_ID)

	CHECK_C_SOURCE_COMPILES ("
#include <stdio.h>
#include <inttypes.h>
int main() {
   printf(\"%\"PRId64, 0x1122334455667788)
   return 0;
}"
	PRINTF64_PRID)

find_program (XXD_CMD "xxd")
if (WIN32)
   find_program (PARASOL_CMD "parasol" NO_DEFAULT_PATH PATHS "${CMAKE_INSTALL_PREFIX}")
   find_program (FLUID_CMD "fluid" NO_DEFAULT_PATH PATHS "${CMAKE_INSTALL_PREFIX}")
else ()
   find_program (PARASOL_CMD "parasol" NO_DEFAULT_PATH PATHS "${CMAKE_INSTALL_PREFIX}/bin")
   find_program (FLUID_CMD "fluid" NO_DEFAULT_PATH PATHS "${CMAKE_INSTALL_PREFIX}/bin")
endif ()
find_program (IDL_C_SCRIPT "idl-c.fluid" PATHS "${PROJECT_SOURCE_DIR}/data/scripts/dev/idl")
find_program (IDL_DEF_SCRIPT "idl-def.fluid" PATHS "${PROJECT_SOURCE_DIR}/data/scripts/dev/idl")
find_program (FLUTE_SCRIPT "flute.fluid" PATHS "${PROJECT_SOURCE_DIR}/data/scripts/dev")

if (FLUID_CMD STREQUAL "FLUID_CMD-NOTFOUND")
   message (STATUS "Definitions will not be built and tests are disabled.")
   set (BUILD_DEFS OFF)
else ()
   if (BUILD_DEFS)
      message (STATUS "Definitions will be built with the Fluid program installed @ ${FLUID_CMD}.")
   endif ()
endif ()

if (BUILD_TESTS)
   message (STATUS "Integration tests are enabled.")
   enable_testing()
endif ()

if (IDL_C_SCRIPT STREQUAL "IDL_C_SCRIPT-NOTFOUND")
   message (FATAL_ERROR "The idl-c.fluid script was not found in ${PROJECT_SOURCE_DIR}/data/scripts/dev/idl")
endif ()

if (IDL_DEF_SCRIPT STREQUAL "IDL_DEF_SCRIPT-NOTFOUND")
   message (FATAL_ERROR "The idl-def.fluid script was not found in ${PROJECT_SOURCE_DIR}/data/scripts/dev/idl")
endif ()

if (FLUTE_SCRIPT STREQUAL "FLUTE_SCRIPT-NOTFOUND")
   message (FATAL_ERROR "The flute.fluid script was not found in ${PROJECT_SOURCE_DIR}/scripts/dev")
endif ()

add_custom_target(build_headers ALL COMMENT "Generate headers and definition files on the fly")

# ----------------------------------------------------------------------------
# Use Flute to write tests against your code.  All Flute tests are integration tests, i.e. run them post-install
# against the target installation folder.

function (flute_test name path)
   if (BUILD_TESTS)
      if (RUN_ANYWHERE)
         set (FLUTE_EXE "${CMAKE_INSTALL_PREFIX}/fluid")
      else ()
         set (FLUTE_EXE "${CMAKE_INSTALL_PREFIX}/bin/fluid")
      endif ()
      get_filename_component(FLUTE_FULL_PATH "${path}" ABSOLUTE)
      add_test (NAME ${name}
         COMMAND "${FLUTE_EXE}" "--log-error" "${FLUTE_SCRIPT}" "file=${FLUTE_FULL_PATH}"
         WORKING_DIRECTORY "${CMAKE_INSTALL_PREFIX}")
   endif ()
endfunction ()

# ----------------------------------------------------------------------------
# idl_all() is used to generate both C/C++ headers and IDL strings from FDL files.

function (idl_all)
   cmake_parse_arguments (PARSE_ARGV 0 IDLC "" "OUTPUT;APPEND_IDL;NAME" "ARGS;FILES")

   if (BUILD_DEFS)
      if (DEFINED IDLC_NAME)
         set (OUTPUT_NAME ${IDLC_NAME})
      else ()
         string (REGEX REPLACE "[/:\.]" "_" OUTPUT_NAME ${IDLC_OUTPUT})
      endif ()

      if (DEFINED IDLC_FILES)
         list (APPEND IDLC_ARGS "--files={" ${IDLC_FILES} "}")
      endif ()

      list (APPEND COMMAND_LIST COMMAND ${FLUID_CMD} ${IDL_C_SCRIPT} "--log-error" "--src=${ARGV0}"
         "--output=${IDLC_OUTPUT}" "--sdk=${PROJECT_SOURCE_DIR}" ${IDLC_ARGS})

      if (DEFINED IDLC_APPEND_IDL)
         list (APPEND COMMAND_LIST COMMAND ${FLUID_CMD} ${IDL_DEF_SCRIPT} "--log-error" "--src=${ARGV0}"
            "--output=${IDLC_APPEND_IDL}" "--sdk=${PROJECT_SOURCE_DIR}" "--format=c" "--append")
      endif ()

      add_custom_command (OUTPUT ${IDLC_OUTPUT}
         PRE_BUILD
         ${COMMAND_LIST}
         WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
         DEPENDS ${ARGV0}
         COMMENT "Generating C header file ${IDLC_OUTPUT} from ${ARGV0}"
         VERBATIM)

      add_custom_target (${OUTPUT_NAME} DEPENDS ${IDLC_OUTPUT})

      add_dependencies (build_headers ${OUTPUT_NAME})
   else ()
      message (STATUS "The generation of ${IDLC_OUTPUT} will be skipped.")
   endif ()
endfunction ()

# ----------------------------------------------------------------------------
# idl_c() is used to generate C/C++ headers from FDL files.
# Always provide a full path for OUTPUT
# Use FILES to reference additional source files not in the .def file.

function (idl_c)
   cmake_parse_arguments (PARSE_ARGV 0 IDLC "" "OUTPUT;NAME" "ARGS;FILES")

   if (BUILD_DEFS)
      if (DEFINED IDLC_NAME)
         set (OUTPUT_NAME ${IDLC_NAME})
      else ()
         string (REGEX REPLACE "[/:\.]" "_" OUTPUT_NAME ${IDLC_OUTPUT})
      endif ()

      if (DEFINED IDLC_FILES)
         list (APPEND IDLC_ARGS "--files={" ${IDLC_FILES} "}")
      endif ()

      add_custom_command (OUTPUT ${IDLC_OUTPUT}
         PRE_BUILD
         COMMAND ${FLUID_CMD} ${IDL_C_SCRIPT} "--log-error" "--src=${ARGV0}" "--output=${IDLC_OUTPUT}" "--sdk=${PROJECT_SOURCE_DIR}" ${IDLC_ARGS}
         WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
         DEPENDS ${ARGV0}
         COMMENT "Producing C header file ${IDLC_OUTPUT} from ${ARGV0}"
         VERBATIM)

      add_custom_target (${OUTPUT_NAME} DEPENDS ${IDLC_OUTPUT})

      add_dependencies (build_headers ${OUTPUT_NAME})
   else ()
      message (STATUS "The generation of ${IDLC_OUTPUT} will be skipped.")
   endif ()
endfunction ()

# ----------------------------------------------------------------------------
# idl_def() is used to generate C/C++ data definition files for functions, structural declarations etc
# Always provide a full path for OUTPUT

function (idl_def)
   cmake_parse_arguments (PARSE_ARGV 0 IDLC "" "OUTPUT;NAME" "ARGS;FILES")

   if (BUILD_DEFS)
      if (DEFINED IDLC_NAME)
         set (OUTPUT_NAME ${IDLC_NAME})
      else ()
         string (REGEX REPLACE "[/:\.]" "_" OUTPUT_NAME ${IDLC_OUTPUT})
      endif ()

      if (DEFINED IDLC_FILES)
         list (APPEND IDLC_ARGS "--files={" ${IDLC_FILES} "}")
      endif ()

      add_custom_command (OUTPUT ${IDLC_OUTPUT}
         PRE_BUILD
         COMMAND ${FLUID_CMD} ${IDL_DEF_SCRIPT} "--log-error" "--src=${ARGV0}" "--output=${IDLC_OUTPUT}" "--sdk=${PROJECT_SOURCE_DIR}" "--format=c" ${IDLC_ARGS}
         WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
         DEPENDS ${ARGV0}
         COMMENT "Producing definition file ${IDLC_OUTPUT} from ${ARGV0}"
         VERBATIM)

      add_custom_target (${OUTPUT_NAME} DEPENDS ${IDLC_OUTPUT})

      add_dependencies (build_headers ${OUTPUT_NAME})
   else ()
      message (STATUS "The generation of ${IDLC_OUTPUT} will be skipped.")
   endif ()
endfunction ()

# ----------------------------------------------------------------------------
# This is a convenience function that configures the default settings needed for Parasol modules

function (set_module_defaults ARG_TARGET)
   target_link_options (${ARG_TARGET} PRIVATE "$<${GCC_COMPAT}:$<LINK_ONLY:LINKER:--gc-sections>>")

   target_include_directories (${ARG_TARGET} PRIVATE "${PROJECT_SOURCE_DIR}/core/include")

   add_definitions ("-DMOD_NAME=${ARG_TARGET}")

   if (CMAKE_BUILD_TYPE MATCHES "Release")
      set_target_properties (${ARG_TARGET} PROPERTIES LINK_FLAGS_RELEASE -s)
   endif ()

   set_target_properties (${ARG_TARGET}
      PROPERTIES
      PREFIX ""
      LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/release/lib"
      RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/release/lib")

   if (PARASOL_INSTALL)
      if (WIN32)
         install(TARGETS ${ARG_TARGET} RUNTIME DESTINATION "${LIB_TARGET}")
      else ()
         install(TARGETS ${ARG_TARGET}
            RUNTIME DESTINATION "${LIB_TARGET}"
            ARCHIVE DESTINATION "${LIB_TARGET}"
            LIBRARY DESTINATION "${LIB_TARGET}")
      endif ()
   endif ()
endfunction ()

# ----------------------------------------------------------------------------
# 64-bit checks

if (CMAKE_SIZEOF_VOID_P EQUAL 8)
   message (STATUS "This will be a 64-bit build for processor arch ${CMAKE_HOST_SYSTEM_PROCESSOR}")
   set (_LP64 ON) # Used by config.h
#   set (PARASOL_ARCH "-m64")
else ()
   message (STATUS "This will be a 32-bit build for processor arch ${CMAKE_HOST_SYSTEM_PROCESSOR}")
#   set (PARASOL_ARCH "-m32")
endif ()

set (CMAKE_CXX_STANDARD 11)
set (CMAKE_CXX_STANDARD_REQUIRED True)

# Set system-specific compiler flags

if (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
   add_definitions ("-D_DARWIN_C_SOURCE")
elseif (CMAKE_SYSTEM_NAME STREQUAL "Linux" OR CMAKE_SYSTEM_NAME STREQUAL "Android")
   add_definitions ("-D_GNU_SOURCE")
	set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fdata-sections -ffunction-sections -fno-strict-aliasing -Wno-pointer-sign")
	set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fdata-sections -ffunction-sections -fno-strict-aliasing")
elseif (WIN32)
   add_definitions ("-D_WIN32" "-U__unix__")
	set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu11 -fdata-sections -ffunction-sections -fno-strict-aliasing -Wno-pointer-sign ${PARASOL_ARCH}")
	set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fdata-sections -ffunction-sections -fno-strict-aliasing ${PARASOL_ARCH}")
   if (PRINTF64_ID)
      add_definitions ("-DPRINTF64I")
   endif ()
elseif (CMAKE_SYSTEM_NAME STREQUAL "Android")
# TODO
endif ()

set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wno-empty-body -Wno-unused-result -Wno-format-zero-length -Wno-unused-but-set-variable -Wno-stringop-overflow")
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wno-empty-body -Wno-unused-result -Wno-format-zero-length -Wno-unused-but-set-variable -Wno-stringop-overflow")

# These default compile options are for use by target_compile_options()
# TODO: Utilise target_compile_options() in the makefiles

set (GCC_COMPAT "$<COMPILE_LANG_AND_ID:CXX,ARMClang,AppleClang,Clang,GNU>")
set (DEFAULT_GCC_FLAGS "-Wall" "-Wno-empty-body" "-Wno-unused-result" "-Wno-format-zero-length" "-Wno-unused-but-set-variable" "-Wno-stringop-overflow")
set (DEFAULT_GCXX_FLAGS "-Wall" "-Wno-empty-body" "-Wno-unused-result" "-Wno-format-zero-length" "-Wno-unused-but-set-variable" "-Wno-stringop-overflow")

# Support for building Windows resource files with MINGW

if (MINGW)
  set (CMAKE_RC_COMPILER_INIT windres)
  enable_language (RC)
  set (CMAKE_RC_COMPILE_OBJECT "<CMAKE_RC_COMPILER> -O coff -i <SOURCE> -o <OBJECT>")
endif (MINGW)

set(CMAKE_INSTALL_RPATH "$ORIGIN/lib")
add_subdirectory (core/3rdparty/zlib-1.2.11) # Note that when upgrading, the CMakeLists.txt is customised
add_subdirectory (core/link)
add_subdirectory (core/src)
add_subdirectory (core/modules/audio)
add_subdirectory (core/modules/display)
add_subdirectory (core/modules/document)
add_subdirectory (core/modules/fluid)
add_subdirectory (core/modules/font)
add_subdirectory (core/modules/http)
add_subdirectory (core/modules/iconserver)
add_subdirectory (core/modules/json)
add_subdirectory (core/modules/network)
add_subdirectory (core/modules/parc)
add_subdirectory (core/modules/picture)
add_subdirectory (core/modules/picture_jpeg)
add_subdirectory (core/modules/scintilla)
add_subdirectory (core/modules/surface)
add_subdirectory (core/modules/svg)
add_subdirectory (core/modules/vector)
add_subdirectory (core/modules/widget)
add_subdirectory (core/modules/window)
add_subdirectory (core/modules/xml)
if (X11_Xrandr_FOUND)
   add_subdirectory (core/modules/xrandr)
endif ()

if (WIN32)
   # Note that compiling with -static in Linux is disabled because it appears to cause segfaults when writing __thread variables.
   set(CMAKE_EXE_LINKER_FLAGS "-static") #-static-libgcc -static-libstdc++
endif ()

# ----------------------------------------------------------------------------
# Parasol and Fluid executables

if (RUN_ANYWHERE)
   set (CMAKE_INSTALL_RPATH "$ORIGIN/lib/lib")
else ()
   set (CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib/parasol")
endif ()

if (WIN32)
   # Specifying the WIN32 option will generate a Windows GUI application
   add_executable (parasol WIN32 "core/launcher/parasol.cpp" "core/launcher/sandbox-win.cpp" "core/link/resource.rc")
   target_link_libraries (parasol PRIVATE init-win)

   add_executable (fluid_cmd WIN32 "core/launcher/fluid.cpp" "core/launcher/sandbox-win.cpp" "core/link/resource.rc")
   target_link_libraries (fluid_cmd PRIVATE init-win)
else ()
   add_executable (parasol "core/launcher/parasol.cpp")
   target_link_libraries (parasol PRIVATE init-unix)

   add_executable (fluid_cmd "core/launcher/fluid.cpp")
   target_link_libraries (fluid_cmd PRIVATE init-unix)
endif ()

target_include_directories (parasol PRIVATE "${PROJECT_SOURCE_DIR}/core/link")
target_include_directories (fluid_cmd PRIVATE "${PROJECT_SOURCE_DIR}/core/link")

if (CMAKE_BUILD_TYPE MATCHES "Release")
   message (STATUS "Symbols will be stripped from this build (no debugging).")
   set_target_properties (parasol fluid_cmd PROPERTIES LINK_FLAGS_RELEASE "-s" RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/release")
else ()
   message (STATUS "Symbols will be left intact in this build (debug enabled).")
   set_target_properties (parasol fluid_cmd PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/release")
endif ()

set_target_properties (fluid_cmd PROPERTIES OUTPUT_NAME "fluid")

# Attach headers to the Fluid binary so that we can ensure that all headers are built

add_dependencies (fluid_cmd build_headers)

# ----------------------------------------------------------------------------
# Generate the platform configuration header

configure_file ("core/config.h.in" "parasol/config.h" NEWLINE_STYLE UNIX)

# ----------------------------------------------------------------------------
# Installation steps

if (PARASOL_INSTALL)
   install (DIRECTORY "${INCLUDE_OUTPUT}" DESTINATION "include")
   install (FILES "${CMAKE_BINARY_DIR}/parasol/config.h" DESTINATION "include/parasol/")

   if (RUN_ANYWHERE)
      install (TARGETS parasol DESTINATION .)
      install (TARGETS fluid_cmd DESTINATION .)
   else ()
      install (TARGETS parasol DESTINATION "bin")
      install (TARGETS fluid_cmd DESTINATION "bin")
   endif ()

if (WIN32)
   install (DIRECTORY "${PROJECT_SOURCE_DIR}/data/ssl/" DESTINATION "config/ssl")
   install (DIRECTORY "${PROJECT_SOURCE_DIR}/data/styles/" DESTINATION "styles")
   install (DIRECTORY "${PROJECT_SOURCE_DIR}/data/translations/" DESTINATION "config/translations")
   install (DIRECTORY "${PROJECT_SOURCE_DIR}/data/users/" DESTINATION "config/users")
   install (DIRECTORY "${PROJECT_SOURCE_DIR}/data/fonts/" DESTINATION "fonts")
   install (DIRECTORY "${PROJECT_SOURCE_DIR}/data/icons/" DESTINATION "icons")
   install (DIRECTORY "${PROJECT_SOURCE_DIR}/data/scripts/" DESTINATION "scripts")
   if (INSTALL_EXAMPLES)
      install (DIRECTORY "${PROJECT_SOURCE_DIR}/examples/" DESTINATION "examples")
   endif ()
else ()
   install (DIRECTORY "${PROJECT_SOURCE_DIR}/data/ssl" DESTINATION "${SHARE_TARGET}")
   install (DIRECTORY "${PROJECT_SOURCE_DIR}/data/styles" DESTINATION "${SHARE_TARGET}")
   install (DIRECTORY "${PROJECT_SOURCE_DIR}/data/translations" DESTINATION "${SHARE_TARGET}/config")
   install (DIRECTORY "${PROJECT_SOURCE_DIR}/data/users" DESTINATION "${SHARE_TARGET}/config")
   install (DIRECTORY "${PROJECT_SOURCE_DIR}/data/fonts" DESTINATION "${SHARE_TARGET}")
   install (DIRECTORY "${PROJECT_SOURCE_DIR}/data/icons" DESTINATION "${SHARE_TARGET}")
   install (DIRECTORY "${PROJECT_SOURCE_DIR}/data/scripts" DESTINATION "${SHARE_TARGET}")
   if (INSTALL_EXAMPLES)
      install (DIRECTORY "${PROJECT_SOURCE_DIR}/examples" DESTINATION "${SHARE_TARGET}")
   endif ()
endif ()

if (WIN32) # MSYS2/MinGW builds have library dependencies that we need to take a copy of.
   find_file(MSYS_DEP2 "libstdc++-6.dll" HINTS "/mingw64/bin")
   if (NOT MSYS_DEP2 STREQUAL "MSYS_DEP2-NOTFOUND")
      install (FILES ${MSYS_DEP2} DESTINATION .)
   endif ()

   find_file(MSYS_DEP4 "libgcc_s_seh-1.dll" HINTS "/mingw64/bin")
   if (NOT MSYS_DEP4 STREQUAL "MSYS_DEP4-NOTFOUND")
      install (FILES ${MSYS_DEP4} DESTINATION .)
   endif ()

   find_file(MSYS_DEP5 "libwinpthread-1.dll" HINTS "/mingw64/bin")
   if (NOT MSYS_DEP5 STREQUAL "MSYS_DEP5-NOTFOUND")
      install (FILES ${MSYS_DEP5} DESTINATION .)
   endif ()
endif ()

   install (CODE "file(REMOVE \"${PARASOL_ROOT}/config/classes.bin\" \"${PARASOL_ROOT}/config/modules.bin\")")
   install (CODE "execute_process(COMMAND parasol --log-error \"${PARASOL_ROOT}/scripts/dev/verify-modules.fluid\" WORKING_DIRECTORY \"${CMAKE_INSTALL_PREFIX}\")")

if (WIN32)
   install (CODE "MESSAGE(STATUS \"\nParasol has been installed to '${CMAKE_INSTALL_PREFIX}'.  You may need to add this folder to your PATH variable in order to run parasol.exe and fluid.exe from the command-line.\")")
else ()
   install (CODE "MESSAGE(STATUS \"\nParasol has been installed to '${CMAKE_INSTALL_PREFIX}' and shared files are in '${PARASOL_ROOT}'.\")")
endif ()

endif ()

