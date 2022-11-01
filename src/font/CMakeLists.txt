# Font module

# When upgrading Freetype to a new version:
# Freetype definitions in ftoption.h that are turned off: FT_CONFIG_OPTION_USE_LZW  FT_CONFIG_OPTION_MAC_FONTS FT_CONFIG_OPTION_INCREMENTAL TT_CONFIG_OPTION_BDF FT_CONFIG_OPTION_USE_ZLIB TT_CONFIG_OPTION_GX_VAR_SUPPORT
# base/ftsystem.c is modified to support our file system.
# Change config/ftmodule.h to match the existing changes.
# ./configure --without-png --without-zlib --without-bzip2 --without-png
# Do a standard compile and then look in objs/ to see what .o files were created.

set (MOD "font")

idl_all ("${MOD}.fdl"
   NAME ${MOD}_defs
   OUTPUT "${INCLUDE_OUTPUT}/modules/font.h"
   FILES "font.cpp"
   ARGS "--output-defs=font_def.c" "--output-proto=font_def.c" "--prototypes=static"
   APPEND_IDL "font_def.c")

# A bug in (MinGW?) GCC requires that mno-ms-bitfields is applied to ensure that __packed__ works correctly.

set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mno-ms-bitfields")
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mno-ms-bitfields")

set (FT_SOURCE
   "${CMAKE_CURRENT_SOURCE_DIR}/freetype-2.10.2/src/autofit/autofit.c"
   "${CMAKE_CURRENT_SOURCE_DIR}/freetype-2.10.2/src/psnames/psnames.c"
   "${CMAKE_CURRENT_SOURCE_DIR}/freetype-2.10.2/src/raster/raster.c"
   "${CMAKE_CURRENT_SOURCE_DIR}/freetype-2.10.2/src/cff/cff.c"
   "${CMAKE_CURRENT_SOURCE_DIR}/freetype-2.10.2/src/base/ftglyph.c"
   "${CMAKE_CURRENT_SOURCE_DIR}/freetype-2.10.2/src/sfnt/sfnt.c"
   "${CMAKE_CURRENT_SOURCE_DIR}/freetype-2.10.2/src/base/ftbase.c"
   "${CMAKE_CURRENT_SOURCE_DIR}/freetype-2.10.2/src/base/ftsystem.cpp"
   "${CMAKE_CURRENT_SOURCE_DIR}/freetype-2.10.2/src/smooth/smooth.c"
   "${CMAKE_CURRENT_SOURCE_DIR}/freetype-2.10.2/src/truetype/truetype.c"
   "${CMAKE_CURRENT_SOURCE_DIR}/freetype-2.10.2/src/base/ftinit.c"
   "${CMAKE_CURRENT_SOURCE_DIR}/freetype-2.10.2/src/base/ftstroke.c"
   "${CMAKE_CURRENT_SOURCE_DIR}/freetype-2.10.2/src/base/ftbitmap.c"
   "${CMAKE_CURRENT_SOURCE_DIR}/freetype-2.10.2/src/base/ftdebug.c"
   "${CMAKE_CURRENT_SOURCE_DIR}/freetype-2.10.2/src/pshinter/pshinter.c")

add_library (${MOD} SHARED)

set_module_defaults (${MOD})

target_sources (${MOD} PRIVATE "font.cpp" ${FT_SOURCE})

target_include_directories (${MOD} PRIVATE
   "freetype-2.10.2/include"
   "freetype-2.10.2/src/base"
   "freetype-2.10.2/include/freetype/config"
   "freetype-2.10.2/builds/unix")

target_link_libraries (${MOD} PRIVATE "m")

#-DFT_DEBUG_LEVEL_ERROR -DFT_DEBUG_LEVEL_TRACE
add_definitions ("-DFT_CONFIG_MODULES_H=<ftmodule.h>" "-DFT_CONFIG_CONFIG_H=<ftconfig.h>" "-DFT2_BUILD_LIBRARY" "-DDLL_EXPORT=1")

# TODO: Kerning tests aren't working with the latest version of Freetype
flute_test(${MOD} "${CMAKE_CURRENT_SOURCE_DIR}/tests/font_tests.fluid")
