# Copyright (C) 2013-2023 by
# David Turner, Robert Wilhelm, and Werner Lemberg.
#
# Written originally by John Cary <cary@txcorp.com>
#
# This file is part of the FreeType project, and may only be used, modified,
# and distributed under the terms of the FreeType project license,
# LICENSE.TXT.  By continuing to use, modify, or distribute this file you
# indicate that you have read the license and understand and accept it
# fully.

include(CheckIncludeFile)

#-DFT_DEBUG_LEVEL_ERROR -DFT_DEBUG_LEVEL_TRACE

option(FT_DISABLE_BZIP2 "" ON)
option(FT_DISABLE_BROTLI "" ON)
option(FT_ENABLE_ERROR_STRINGS "" OFF)
option(FT_CONFIG_OPTION_USE_LZW "" OFF)
option(FT_CONFIG_OPTION_MAC_FONTS "" OFF)
option(FT_CONFIG_OPTION_INCREMENTAL "" OFF)
option(TT_CONFIG_OPTION_BDF "" OFF)
option(FT_CONFIG_OPTION_USE_ZLIB "" OFF)
option(TT_CONFIG_OPTION_GX_VAR_SUPPORT "" OFF)

# CMAKE_TOOLCHAIN_FILE must be set before `project' is called, which
# configures the base build environment and references the toolchain file
if (APPLE)
  if (DEFINED IOS_PLATFORM)
    if (NOT "${IOS_PLATFORM}" STREQUAL "OS"
        AND NOT "${IOS_PLATFORM}" STREQUAL "SIMULATOR"
        AND NOT "${IOS_PLATFORM}" STREQUAL "SIMULATOR64")
      message(FATAL_ERROR
        "IOS_PLATFORM must be set to either OS, SIMULATOR, or SIMULATOR64")
    endif ()
    if (NOT "${CMAKE_GENERATOR}" STREQUAL "Xcode")
      message(AUTHOR_WARNING
        "You should use Xcode generator with IOS_PLATFORM enabled to get Universal builds.")
    endif ()
    if (BUILD_SHARED_LIBS)
      message(FATAL_ERROR
        "BUILD_SHARED_LIBS can not be on with IOS_PLATFORM enabled")
    endif ()
    if (BUILD_FRAMEWORK)
      message(FATAL_ERROR
        "BUILD_FRAMEWORK can not be on with IOS_PLATFORM enabled")
    endif ()

    # iOS only uses static libraries
    set(BUILD_SHARED_LIBS OFF)

    set(CMAKE_TOOLCHAIN_FILE
      ${CMAKE_SOURCE_DIR}/builds/cmake/iOS.cmake)
  endif ()
else ()
  if (DEFINED IOS_PLATFORM)
    message(FATAL_ERROR "IOS_PLATFORM is not supported on this platform")
  endif ()
endif ()

project(freetype C)

set(VERSION_MAJOR "2")
set(VERSION_MINOR "13")
set(VERSION_PATCH "2")

# Generate LIBRARY_VERSION and LIBRARY_SOVERSION.
set(LIBTOOL_REGEX "version_info='([0-9]+):([0-9]+):([0-9]+)'")

file(STRINGS "${PROJECT_SOURCE_DIR}/builds/unix/configure.raw" VERSION_INFO REGEX ${LIBTOOL_REGEX})

string(REGEX REPLACE ${LIBTOOL_REGEX} "\\1" LIBTOOL_CURRENT "${VERSION_INFO}")
string(REGEX REPLACE ${LIBTOOL_REGEX} "\\2" LIBTOOL_REVISION "${VERSION_INFO}")
string(REGEX REPLACE ${LIBTOOL_REGEX} "\\3" LIBTOOL_AGE "${VERSION_INFO}")

# This is what libtool does internally on Unix platforms.
math(EXPR LIBRARY_SOVERSION "${LIBTOOL_CURRENT} - ${LIBTOOL_AGE}")
set(LIBRARY_VERSION "${LIBRARY_SOVERSION}.${LIBTOOL_AGE}.${LIBTOOL_REVISION}")

option(FT_ENABLE_ERROR_STRINGS "Enable support for meaningful error descriptions." OFF)

# Add local cmake modules
list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/builds/cmake)

# Create the configuration file
if (UNIX)
  check_include_file("unistd.h" HAVE_UNISTD_H)
  check_include_file("fcntl.h" HAVE_FCNTL_H)

  file(READ "${PROJECT_SOURCE_DIR}/builds/unix/ftconfig.h.in"
    FTCONFIG_H)
  if (HAVE_UNISTD_H)
    string(REGEX REPLACE
      "#undef +(HAVE_UNISTD_H)" "#define \\1 1"
      FTCONFIG_H "${FTCONFIG_H}")
  endif ()
  if (HAVE_FCNTL_H)
    string(REGEX REPLACE
      "#undef +(HAVE_FCNTL_H)" "#define \\1 1"
      FTCONFIG_H "${FTCONFIG_H}")
  endif ()
else ()
  file(READ "${PROJECT_SOURCE_DIR}/include/freetype/config/ftconfig.h"
    FTCONFIG_H)
endif ()

set(FTCONFIG_H_NAME "${PROJECT_BINARY_DIR}/include/freetype/config/ftconfig.h")
if (EXISTS "${FTCONFIG_H_NAME}")
  file(READ "${FTCONFIG_H_NAME}" ORIGINAL_FTCONFIG_H)
else ()
  set(ORIGINAL_FTCONFIG_H "")
endif ()
if (NOT (ORIGINAL_FTCONFIG_H STREQUAL FTCONFIG_H))
  file(WRITE "${FTCONFIG_H_NAME}" "${FTCONFIG_H}")
endif ()

# Create the options file
file(READ "${PROJECT_SOURCE_DIR}/include/freetype/config/ftoption.h" FTOPTION_H)

if (FT_ENABLE_ERROR_STRINGS)
  string(REGEX REPLACE "/\\* +(#define +FT_CONFIG_OPTION_ERROR_STRINGS) +\\*/" "\\1"
    FTOPTION_H "${FTOPTION_H}")
endif ()

set(FTOPTION_H_NAME "${PROJECT_BINARY_DIR}/include/freetype/config/ftoption.h")
if (EXISTS "${FTOPTION_H_NAME}")
  file(READ "${FTOPTION_H_NAME}" ORIGINAL_FTOPTION_H)
else ()
  set(ORIGINAL_FTOPTION_H "")
endif ()
if (NOT (ORIGINAL_FTOPTION_H STREQUAL FTOPTION_H))
  file(WRITE "${FTOPTION_H_NAME}" "${FTOPTION_H}")
endif ()

file(GLOB PUBLIC_HEADERS "include/ft2build.h" "include/freetype/*.h")
file(GLOB PUBLIC_CONFIG_HEADERS "include/freetype/config/*.h")
file(GLOB PRIVATE_HEADERS "include/freetype/internal/*.h")

set(BASE_SRCS
  src/autofit/autofit.c
  src/base/ftbase.c
#  src/base/ftbbox.c
#  src/base/ftbdf.c
  src/base/ftbitmap.c
#  src/base/ftcid.c
#  src/base/ftfstype.c
#  src/base/ftgasp.c
  src/base/ftglyph.c
#  src/base/ftgxval.c
  src/base/ftinit.c
  src/base/ftmm.c
#  src/base/ftotval.c
  src/base/ftpatent.c
#  src/base/ftpfr.c
  src/base/ftstroke.c
#  src/base/ftsynth.c
  src/base/fttype1.c
#  src/base/ftwinfnt.c
#  src/bdf/bdf.c
#  src/bzip2/ftbzip2.c
  src/cache/ftcache.c
  src/cff/cff.c
#  src/cid/type1cid.c
#  src/gzip/ftgzip.c
#  src/lzw/ftlzw.c
#  src/pcf/pcf.c
#  src/pfr/pfr.c
#  src/psaux/psaux.c
  src/pshinter/pshinter.c
  src/psnames/psnames.c
  src/raster/raster.c
#  src/sdf/sdf.c
  src/sfnt/sfnt.c
  src/smooth/smooth.c
#  src/svg/svg.c
  src/truetype/truetype.c
#  src/type1/type1.c
#  src/type42/type42.c
#  src/winfonts/winfnt.c
)

if (UNIX)
  list(APPEND BASE_SRCS "builds/unix/ftsystem.c")
elseif (WIN32)
  list(APPEND BASE_SRCS "builds/windows/ftsystem.c")
else ()
  list(APPEND BASE_SRCS "src/base/ftsystem.c")
endif ()

if (WIN32)
  enable_language(RC)
  list(APPEND BASE_SRCS builds/windows/ftdebug.c src/base/ftver.rc)
elseif (WINCE)
  list(APPEND BASE_SRCS builds/wince/ftdebug.c)
else ()
  list(APPEND BASE_SRCS src/base/ftdebug.c)
endif ()

if (NOT DISABLE_FORCE_DEBUG_POSTFIX)
  set(CMAKE_DEBUG_POSTFIX d)
endif ()

add_library(freetype
  ${PUBLIC_HEADERS}
  ${PUBLIC_CONFIG_HEADERS}
  ${PRIVATE_HEADERS}
  ${BASE_SRCS}
)

set_target_properties(freetype PROPERTIES C_VISIBILITY_PRESET hidden)

target_compile_definitions(freetype PRIVATE FT2_BUILD_LIBRARY)

if (WIN32)
  target_compile_definitions(freetype PRIVATE _CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_WARNINGS)
  if (BUILD_SHARED_LIBS)
    target_compile_definitions(freetype PRIVATE DLL_EXPORT)
  endif ()
endif ()

if (BUILD_SHARED_LIBS)
  set_target_properties(freetype PROPERTIES VERSION ${LIBRARY_VERSION} SOVERSION ${LIBRARY_SOVERSION})
endif ()

# Pick up ftconfig.h and ftoption.h generated above, first.
target_include_directories(
  freetype
    PUBLIC
      $<INSTALL_INTERFACE:include/freetype2>
      $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE
      ${CMAKE_CURRENT_BINARY_DIR}/include
      ${CMAKE_CURRENT_SOURCE_DIR}/include

      # Make <ftconfig.h> available for builds/unix/ftsystem.c.
      ${CMAKE_CURRENT_BINARY_DIR}/include/freetype/config
)

if (BUILD_SHARED_LIBS)
    install(TARGETS freetype
        LIBRARY DESTINATION "${LIB_TARGET}/lib"
        ARCHIVE DESTINATION "${LIB_TARGET}/lib"
        RUNTIME DESTINATION "${LIB_TARGET}/lib")
endif ()
