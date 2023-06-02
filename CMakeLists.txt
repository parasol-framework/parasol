# Parasol Core Build File
#
# Release Config:  cmake -S . -B release -DCMAKE_BUILD_TYPE=Release
# Debug Config:    cmake -S . -B debug -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=local -DRUN_ANYWHERE=TRUE
# Git Release:     cmake -S . -B release-git -DCMAKE_BUILD_TYPE=Release -DINSTALL_EXAMPLES=TRUE -DINSTALL_INCLUDES=FALSE -DDISABLE_SSL=TRUE -DCMAKE_INSTALL_PREFIX=parasol-$(date +'%Y%m%d') -DRUN_ANYWHERE=TRUE -DBUILD_TESTS=OFF -DBUILD_DEFS=OFF
#
# For Windows builds -G"MinGW Makefiles" must be added to the above commands.
# A build can target a local folder by adding -DCMAKE_INSTALL_PREFIX=local -DRUN_ANYWHERE=TRUE
#
# Build:   cmake --build release -j 8 -- -O
# Install: sudo cmake --install release
# Test:    make -C release test ARGS=--verbose
#
# A static build can be enabled with -DPARASOL_STATIC=ON if the intention is to embed Parasol into another project.
# This is particularly useful if you intend to use specific features like the vector graphics API.  Static builds
# should be customised with an isolated output folder that is exclusive to the target project.  Sharing a static
# build across multiple projects can otherwise lead to confusing outcomes.

cmake_minimum_required(VERSION 3.15)

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

project (Parasol VERSION 2023.4.19 LANGUAGES C CXX)

cmake_policy (SET CMP0082 NEW) # Process install commands in the order that they appear

# To switch on a custom option use -D, e.g. -DPARASOL_VLOG=ON

option (PARASOL_STATIC   "Build framework as a set of static libraries instead of the default modular build." OFF)
option (PARASOL_INSTALL  "Create installation targets (chain builders may want to disable this)" ON)
option (PARASOL_VLOG     "Enable verbose log messages in the generated binaries" OFF)
if (UNIX)
   option (RUN_ANYWHERE  "Build a framework that can run directly from the main folder.  Typically used if redefining CMAKE_INSTALL_PREFIX" OFF)
else ()
   set (RUN_ANYWHERE ON)
endif ()
option (BUILD_TESTS       "Build/enable tests" ON)
option (BUILD_DEFS        "Auto-generate C/C++ headers and documentation" ON)
option (DISABLE_SSL       "Disable built-in SSL support even if available on this system" OFF)
option (DISABLE_X11       "Disable X11 even if available on this system" OFF)
option (DISABLE_AUDIO     "Disable audio API" OFF)
option (DISABLE_DISPLAY   "Disable display API" OFF)
option (DISABLE_DOCUMENT  "Disable document API" OFF)
option (DISABLE_FONT      "Disable font API" OFF)
option (DISABLE_HTTP      "Disable HTTP API" OFF)
option (DISABLE_MP3       "Disable MP3 support" OFF)
option (DISABLE_NETWORK   "Disable Network API" OFF)
option (DISABLE_PICTURE   "Disable Picture API" OFF)
option (DISABLE_JPEG      "Disable JPEG support" OFF)
option (DISABLE_SCINTILLA "Disable Scintilla API" OFF)
option (DISABLE_SVG       "Disable SVG support" OFF)
option (DISABLE_VECTOR    "Disable Vector API" OFF)
option (INSTALL_EXAMPLES  "Install the example scripts, typically for compiling a distribution" OFF)
option (INSTALL_INCLUDES  "Install the header files.  Turn this off for client releases." OFF)
option (INSTALL_TESTS     "Install the test programs." OFF)
option (ENABLE_ANALYSIS   "Enable run-time program analysis (with performance penalties) if available.  May negatively affect gdb." OFF)

set (CMAKE_CXX_STANDARD 20)
set (CMAKE_CXX_STANDARD_REQUIRED True)
set (CMAKE_POSITION_INDEPENDENT_CODE ON) # Enables -fPIC and -fPIE in gcc

set (BUILD_RPATH_USE_ORIGIN TRUE)

set (INCLUDE_OUTPUT "${PROJECT_SOURCE_DIR}/include/parasol")
set (DOCS "${PROJECT_SOURCE_DIR}/docs/xml")

if (PARASOL_STATIC)
   set (BUILD_SHARED_LIBS OFF)
else ()
   set (BUILD_SHARED_LIBS ON)
endif ()

if (RUN_ANYWHERE)
   set (PARASOL_ROOT "${CMAKE_INSTALL_PREFIX}")
   set (SHARE_TARGET .)
   set (LIB_TARGET "lib")
   set (PARASOL_CMD_PATH "${CMAKE_INSTALL_PREFIX}")
else ()
   set (PARASOL_ROOT "${CMAKE_INSTALL_PREFIX}/share/parasol")
   set (SHARE_TARGET "share/parasol")
   set (LIB_TARGET "lib/parasol")
   set (PARASOL_CMD_PATH "${CMAKE_INSTALL_PREFIX}/bin")
endif ()

set (PARASOL_CMD_TARGET "${PARASOL_CMD_PATH}/parasol")

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

include_directories("${PROJECT_SOURCE_DIR}/include"
   "${PROJECT_SOURCE_DIR}/include/parasol/3rdparty"
   "${CMAKE_CURRENT_BINARY_DIR}")

	CHECK_CXX_SOURCE_COMPILES ("
#include <stdio.h>
int main() {
   printf(\"%lld\", 0x1122334455667788);
   return 0;
}"
	PRINTF64_LLD
   FAIL_REGEX "warning")

	CHECK_CXX_SOURCE_COMPILES ("
#include <stdio.h>
int main() {
   printf(\"%I64lld\", 0x1122334455667788);
   return 0;
}"
	PRINTF64_ID
   FAIL_REGEX "warning")

	CHECK_CXX_SOURCE_COMPILES ("
#include <stdio.h>
#include <inttypes.h>
int main() {
   printf(\"%\" PRId64, 0x1122334455667788);
   return 0;
}"
	PRINTF64_PRID
   FAIL_REGEX "warning")

find_program (XXD_CMD "xxd")
find_program (PARASOL_CMD "parasol" NO_DEFAULT_PATH PATHS "${PARASOL_CMD_PATH}")
find_file (IDL_C_SCRIPT "idl-c.fluid" PATHS "${PROJECT_SOURCE_DIR}/scripts/dev/idl")
find_file (IDL_DEF_SCRIPT "idl-def.fluid" PATHS "${PROJECT_SOURCE_DIR}/scripts/dev/idl")
find_file (FLUTE_SCRIPT "flute.fluid" PATHS "${PROJECT_SOURCE_DIR}/scripts/dev")

if (PARASOL_CMD STREQUAL "PARASOL_CMD-NOTFOUND")
   message (STATUS "Definitions will not be built and tests are disabled.")
   set (BUILD_DEFS OFF)
else ()
   if (BUILD_DEFS)
      message (STATUS "Definitions will be built with the Parasol exe installed @ ${PARASOL_CMD}.")
   endif ()
endif ()

if (BUILD_TESTS)
   message (STATUS "Integration tests are enabled.")
   enable_testing()
endif ()

if (IDL_C_SCRIPT STREQUAL "IDL_C_SCRIPT-NOTFOUND")
   message (FATAL_ERROR "The idl-c.fluid script was not found in ${PROJECT_SOURCE_DIR}/scripts/dev/idl")
endif ()

if (IDL_DEF_SCRIPT STREQUAL "IDL_DEF_SCRIPT-NOTFOUND")
   message (FATAL_ERROR "The idl-def.fluid script was not found in ${PROJECT_SOURCE_DIR}/scripts/dev/idl")
endif ()

if (FLUTE_SCRIPT STREQUAL "FLUTE_SCRIPT-NOTFOUND")
   message (FATAL_ERROR "The flute.fluid script was not found in ${PROJECT_SOURCE_DIR}/scripts/dev")
endif ()

if (NOT MSVC)
   set (MATH_LINK m)
endif ()

if (WIN32)
   if (PARASOL_STATIC)
      # NB: MinGW does not guarantee that libgcc and libstdc++ will be static even if these flags are specified.
      if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
         set (CMAKE_EXE_LINKER_FLAGS "-static -static-libgcc -static-libstdc++")
      endif ()
      set (INIT_LINK "init-win" core)
   else ()
      set (CMAKE_EXE_LINKER_FLAGS "-static")
      set (INIT_LINK "init-win")
   endif ()
else ()
   # Note that compiling with -static in Linux is disabled because it appears to cause segfaults when writing __thread variables.
   if (PARASOL_STATIC)
      set (INIT_LINK "init-unix" core)
   else ()
      set (INIT_LINK "init-unix")
   endif ()
endif ()

add_custom_target(build_headers ALL COMMENT "Generate headers and definition files on the fly")

# --------------------------------------------------------------------------------------------------------------------
# Use Flute to write tests against your code.  All Flute tests are integration tests, i.e. run them post-install
# against the target installation folder.

function (flute_test name path)
   if (BUILD_TESTS)
      if (RUN_ANYWHERE)
         set (FLUTE_EXE "${CMAKE_INSTALL_PREFIX}/parasol")
      else ()
         set (FLUTE_EXE "${CMAKE_INSTALL_PREFIX}/bin/parasol")
      endif ()
      get_filename_component(FLUTE_FULL_PATH "${path}" ABSOLUTE)
      add_test (NAME ${name}
         COMMAND "${FLUTE_EXE}" "--log-warning" "--gfx-driver=headless" "${FLUTE_SCRIPT}" "file=${FLUTE_FULL_PATH}"
         WORKING_DIRECTORY "${CMAKE_INSTALL_PREFIX}")
   endif ()
endfunction ()

# --------------------------------------------------------------------------------------------------------------------
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

      list (APPEND COMMAND_LIST COMMAND ${PARASOL_CMD} ${IDL_C_SCRIPT} "--log-warning" "--src=${ARGV0}"
         "--output=${IDLC_OUTPUT}" "--sdk=${PROJECT_SOURCE_DIR}" ${IDLC_ARGS})

      if (DEFINED IDLC_APPEND_IDL)
         list (APPEND COMMAND_LIST COMMAND ${PARASOL_CMD} ${IDL_DEF_SCRIPT} "--log-warning" "--src=${ARGV0}"
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

# --------------------------------------------------------------------------------------------------------------------
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
         COMMAND "${PARASOL_CMD}" "${IDL_C_SCRIPT}" "--log-warning" "--src=${ARGV0}" "--output=${IDLC_OUTPUT}" "--sdk=${PROJECT_SOURCE_DIR}" ${IDLC_ARGS}
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

# --------------------------------------------------------------------------------------------------------------------
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
         COMMAND "${PARASOL_CMD}" "${IDL_DEF_SCRIPT}" "--log-warning" "--src=${ARGV0}" "--output=${IDLC_OUTPUT}" "--sdk=${PROJECT_SOURCE_DIR}" "--format=c" ${IDLC_ARGS}
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

# --------------------------------------------------------------------------------------------------------------------
# This is a convenience function that configures the default settings needed for Parasol modules

function (set_module_defaults ARG_TARGET)
   target_include_directories (${ARG_TARGET} PRIVATE "${PROJECT_SOURCE_DIR}/include")

   add_compile_definitions ("MOD_NAME=${ARG_TARGET}")

   if (MSVC)

   else ()
      # Collect code and data that is never referenced
      target_link_options (${ARG_TARGET} PRIVATE "$<${GCC_COMPAT}:$<LINK_ONLY:LINKER:--gc-sections>>")
   endif ()

   if (PARASOL_STATIC)
      set_target_properties (${ARG_TARGET}
         PROPERTIES
         CXX_STANDARD 20)
   else ()
      if (CMAKE_BUILD_TYPE MATCHES "Release")
         set_target_properties (${ARG_TARGET} PROPERTIES LINK_FLAGS_RELEASE -s)
      endif ()

      set_target_properties (${ARG_TARGET}
         PROPERTIES
         PREFIX ""
         CXX_STANDARD 20
         CMAKE_POSITION_INDEPENDENT_CODE ON # Enables -fPIC and -fPIE in gcc
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
   endif ()
endfunction ()

# --------------------------------------------------------------------------------------------------------------------
# 64-bit checks

if (CMAKE_SIZEOF_VOID_P EQUAL 8)
   message (STATUS "This will be a 64-bit build for processor arch ${CMAKE_HOST_SYSTEM_PROCESSOR}")
   set (_LP64 ON) # Used by config.h
#   set (PARASOL_ARCH "-m64")
else ()
   message (STATUS "This will be a 32-bit build for processor arch ${CMAKE_HOST_SYSTEM_PROCESSOR}")
#   set (PARASOL_ARCH "-m32")
endif ()

if (PARASOL_VLOG)
   add_compile_definitions ("_DEBUG")
endif ()

# Set system-specific compiler flags

if (MSVC)
   # Turn off ugly warnings
   add_compile_definitions ("_CRT_SECURE_NO_WARNINGS" )
   set (CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")
   # These options override sub-projects linking to incorrect standard libraries.
   # https://learn.microsoft.com/en-us/previous-versions/visualstudio/visual-studio-6.0/aa267384(v=vs.60)?redirectedfrom=MSDN
   add_link_options ("/NODEFAULTLIB:libcmt.lib" "/NODEFAULTLIB:libcmtd.lib" "/NODEFAULTLIB:msvcrtd.lib")
   # add_link_options ("/VERBOSE:LIB")
else ()
   if (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
      add_compile_definitions ("_DARWIN_C_SOURCE")
   elseif (CMAKE_SYSTEM_NAME STREQUAL "Linux" OR CMAKE_SYSTEM_NAME STREQUAL "Android")
      add_compile_definitions ("_GNU_SOURCE")
      set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fdiagnostics-color=auto -fdata-sections -ffunction-sections -fno-strict-aliasing -Wno-pointer-sign")
      set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fdiagnostics-color=auto -fdata-sections -ffunction-sections -fno-strict-aliasing")
   elseif (WIN32)
      add_definitions ("-D_WIN32" "-U__unix__")
      set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu11 -fdiagnostics-color=auto -fdata-sections -ffunction-sections -fno-strict-aliasing -Wno-pointer-sign ${PARASOL_ARCH}")
      set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fdata-sections -fdiagnostics-color=auto -ffunction-sections -fno-strict-aliasing ${PARASOL_ARCH}")
      if (PRINTF64_ID)
         add_compile_definitions ("PRINTF64I")
      endif ()
      if (PRINTF64_PRID)
         add_compile_definitions ("PRINTF64_PRID")
      endif ()
   elseif (CMAKE_SYSTEM_NAME STREQUAL "Android")
      # TODO
   endif ()

   if (CMAKE_BUILD_TYPE STREQUAL "Release")
      set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-exceptions")
   endif ()

   if (ENABLE_ANALYSIS)
      # NB: The use of dlclose() is not permitted when analysis is enabled because it upsets the log output.
      # In addition, gdb breakpoints are unlikely to work with these features enabled.
      set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address -fsanitize=leak -DANALYSIS_ENABLED")
      set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -fsanitize=leak -DANALYSIS_ENABLED")
   endif ()

   set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wno-empty-body -Wno-unused-result -Wno-format-zero-length -Wno-unused-but-set-variable -Wno-stringop-overflow")
   set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wno-empty-body -Wno-unused-result -Wno-format-zero-length -Wno-unused-but-set-variable -Wno-stringop-overflow")

   # These default compile options are for use by target_compile_options()
   # TODO: Utilise target_compile_options() in the makefiles

   set (GCC_COMPAT "$<COMPILE_LANG_AND_ID:CXX,ARMClang,AppleClang,Clang,GNU>")
   set (DEFAULT_GCC_FLAGS "-Wall" "-Wno-empty-body" "-Wno-unused-result" "-Wno-format-zero-length" "-Wno-unused-but-set-variable" "-Wno-stringop-overflow")
   set (DEFAULT_GCXX_FLAGS "-Wall" "-Wno-empty-body" "-Wno-unused-result" "-Wno-format-zero-length" "-Wno-unused-but-set-variable" "-Wno-stringop-overflow")
endif ()

# Support for building Windows resource files with MINGW

if (MINGW)
  set (CMAKE_RC_COMPILER_INIT windres)
  enable_language (RC)
  set (CMAKE_RC_COMPILE_OBJECT "<CMAKE_RC_COMPILER> -O coff -i <SOURCE> -o <OBJECT>")
endif (MINGW)

# --------------------------------------------------------------------------------------------------------------------
# Sub-libraries are defined as linkable objects so that they work correctly between static and modular builds.

add_library(linear_rgb OBJECT src/link/linear_rgb.cpp)

# --------------------------------------------------------------------------------------------------------------------

set(CMAKE_INSTALL_RPATH "$ORIGIN/lib")

add_subdirectory (3rdparty/zlib-1.2.11) # Note that when upgrading, the CMakeLists.txt is customised

add_subdirectory (src/link)
add_subdirectory (src/core)
add_subdirectory (src/fluid)
add_subdirectory (src/xml)
add_subdirectory (src/json)

if (NOT DISABLE_AUDIO)
   add_subdirectory (src/audio)
endif ()

if (NOT DISABLE_DISPLAY)
   add_subdirectory (src/display)
endif ()

if ((NOT DISABLE_DOCUMENT) AND (NOT DISABLE_DISPLAY) AND (NOT DISABLE_VECTOR) AND (NOT DISABLE_FONT))
   #add_subdirectory (src/document)
endif ()

if (NOT DISABLE_FONT AND NOT DISABLE_DISPLAY)
   add_subdirectory (src/font)
endif ()

if (NOT DISABLE_HTTP AND NOT DISABLE_NETWORK)
   add_subdirectory (src/http)
endif ()

if (NOT DISABLE_MP3 AND NOT DISABLE_AUDIO)
   add_subdirectory (src/mp3)
endif ()

if (NOT DISABLE_NETWORK)
   add_subdirectory (src/network)
endif ()

if (NOT DISABLE_PICTURE AND NOT DISABLE_DISPLAY)
   add_subdirectory (src/picture)
endif ()

if (NOT DISABLE_JPEG AND NOT DISABLE_PICTURE)
   add_subdirectory (src/picture_jpeg)
endif ()

if (NOT DISABLE_SCINTILLA AND NOT DISABLE_DISPLAY AND NOT DISABLE_VECTOR AND NOT DISABLE_FONT)
   add_subdirectory (src/scintilla)
endif ()

if (NOT DISABLE_SVG AND NOT DISABLE_VECTOR AND NOT DISABLE_DISPLAY AND NOT DISABLE_FONT)
   add_subdirectory (src/svg)
endif ()

if (NOT DISABLE_VECTOR AND NOT DISABLE_DISPLAY AND NOT DISABLE_FONT)
   add_subdirectory (src/vector)
endif ()

if (X11_Xrandr_FOUND)
   add_subdirectory (src/xrandr)
endif ()

add_subdirectory (src/launcher)

# --------------------------------------------------------------------------------------------------------------------
# Generate the platform configuration header

configure_file ("src/config.h.in" "parasol/config.h" NEWLINE_STYLE UNIX)

# --------------------------------------------------------------------------------------------------------------------
# Icons are installed as a zip file

file(GLOB_RECURSE icon_files "data/icons/*.svg")

add_custom_command (
   OUTPUT  "${CMAKE_CURRENT_BINARY_DIR}/icons/Default.zip"
   PRE_BUILD
   COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/icons"
   COMMAND ${CMAKE_COMMAND} -E tar "cf" "${CMAKE_CURRENT_BINARY_DIR}/icons/Default.zip" --format=zip -- "."
   DEPENDS ${icon_files}
   WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}/data/icons/Default"
   COMMENT "Zipping icons")

add_custom_target (icon_package ALL DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/icons/Default.zip")

# --------------------------------------------------------------------------------------------------------------------
# Installation steps

if (PARASOL_INSTALL)
   if (INSTALL_INCLUDES)
      install (DIRECTORY "${INCLUDE_OUTPUT}" DESTINATION "include")
      install (FILES "${CMAKE_BINARY_DIR}/parasol/config.h" DESTINATION "include/parasol/")
   endif ()

   if (WIN32)
      if (NOT DISABLE_SSL)
         install (DIRECTORY "${PROJECT_SOURCE_DIR}/data/ssl/" DESTINATION "config/ssl")
      endif ()
      install (DIRECTORY "${PROJECT_SOURCE_DIR}/data/users/" DESTINATION "config/users")
      if (NOT DISABLE_FONT)
         install (DIRECTORY "${PROJECT_SOURCE_DIR}/data/fonts/" DESTINATION "config/fonts")
      endif ()
      install (DIRECTORY "${PROJECT_SOURCE_DIR}/data/styles/" DESTINATION "config/styles")
      install (FILES "${CMAKE_BINARY_DIR}/icons/Default.zip" DESTINATION "config/styles/icons")
      install (DIRECTORY "${PROJECT_SOURCE_DIR}/scripts/" DESTINATION "scripts")
      if (INSTALL_EXAMPLES)
         install (DIRECTORY "${PROJECT_SOURCE_DIR}/examples/" DESTINATION "examples")
      endif ()
   else ()
      if (NOT DISABLE_SSL)
         install (DIRECTORY "${PROJECT_SOURCE_DIR}/data/ssl" DESTINATION "${SHARE_TARGET}/config")
      endif ()
      install (DIRECTORY "${PROJECT_SOURCE_DIR}/data/users" DESTINATION "${SHARE_TARGET}/config")
      if (NOT DISABLE_FONT)
         install (DIRECTORY "${PROJECT_SOURCE_DIR}/data/fonts" DESTINATION "${SHARE_TARGET}/config")
      endif ()
      install (DIRECTORY "${PROJECT_SOURCE_DIR}/data/styles" DESTINATION "${SHARE_TARGET}/config")
      install (FILES "${CMAKE_BINARY_DIR}/icons/Default.zip" DESTINATION "${SHARE_TARGET}/config/styles/icons")
      install (DIRECTORY "${PROJECT_SOURCE_DIR}/scripts" DESTINATION "${SHARE_TARGET}")
      if (INSTALL_EXAMPLES)
         install (DIRECTORY "${PROJECT_SOURCE_DIR}/examples" DESTINATION "${SHARE_TARGET}")
      endif ()
   endif ()

   if (NOT PARASOL_STATIC)
      install (CODE "file(REMOVE \"${PARASOL_ROOT}/config/classes.bin\" \"${PARASOL_ROOT}/config/modules.bin\")")
      install (CODE "MESSAGE(STATUS \"${PARASOL_CMD_TARGET} --log-warning ${PARASOL_ROOT}/scripts/dev/verify-modules.fluid\")")
      install (CODE "execute_process(COMMAND \"${PARASOL_CMD_TARGET}\" --log-warning --gfx-driver=headless \"${PARASOL_ROOT}/scripts/dev/verify-modules.fluid\" WORKING_DIRECTORY \"${CMAKE_INSTALL_PREFIX}\")")
   endif ()

   if (RUN_ANYWHERE)
      install (CODE "MESSAGE(STATUS \"\nParasol has been installed to '${CMAKE_INSTALL_PREFIX}'.  You may need to add this folder to your PATH variable in order to run parasol.exe from the command-line.\")")
   else ()
      install (CODE "MESSAGE(STATUS \"\nParasol has been installed to '${CMAKE_INSTALL_PREFIX}' and shared files are in '${PARASOL_ROOT}'.\")")
   endif ()
endif ()

