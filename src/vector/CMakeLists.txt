# Vector module

set (MOD vector)
set (INC_MOD_VECTOR TRUE PARENT_SCOPE)

# Check for SIMD support
include(CheckCXXCompilerFlag)
check_cxx_compiler_flag("-msse4.1" HAS_SSE41)
if(HAS_SSE41)
    target_compile_options(${MOD} PRIVATE "-msse4.1")
endif()

idl_gen ("${MOD}.fdl" NAME ${MOD}_defs OUTPUT "${INCLUDE_OUTPUT}/modules/vector.h"
   FILES "vector_functions.cpp"
   ARGS "output-defs=module_def.c")

idl_compile ("${MOD}.fdl" NAME ${MOD}_idl OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/idl.h")

add_library (${MOD})

set_module_defaults (${MOD})

if (MSVC)
    # Enable SSE4.1 for MSVC if available
    if(HAS_SSE41)
        target_compile_options(${MOD} PRIVATE "/arch:SSE4.1")
    endif()
else ()
target_compile_options (${MOD} PRIVATE
  "$<${GCC_COMPAT}:$<BUILD_INTERFACE:-Wno-narrowing;-Wno-uninitialized>>")
endif ()

#add_definitions ("-DDBG_DRAW=1")

target_sources (${MOD} PRIVATE
   "${CMAKE_CURRENT_SOURCE_DIR}/vector.cpp"
   "${CMAKE_CURRENT_SOURCE_DIR}/vector_functions.cpp"
   "${CMAKE_CURRENT_SOURCE_DIR}/filters/filter.cpp"
   "${CMAKE_CURRENT_SOURCE_DIR}/scene/scene.cpp"
   "${CMAKE_CURRENT_SOURCE_DIR}/painters/painters.cpp"
   "${CMAKE_CURRENT_SOURCE_DIR}/transformers/transformers.cpp"
   "${CMAKE_CURRENT_SOURCE_DIR}/colourmaps.cpp"
   "${CMAKE_CURRENT_SOURCE_DIR}/agg/src/agg_arc.cpp"
   "${CMAKE_CURRENT_SOURCE_DIR}/agg/src/agg_arrowhead.cpp"
   "${CMAKE_CURRENT_SOURCE_DIR}/agg/src/agg_bezier_arc.cpp"
   "${CMAKE_CURRENT_SOURCE_DIR}/agg/src/agg_bspline.cpp"
   "${CMAKE_CURRENT_SOURCE_DIR}/agg/src/agg_curves.cpp"
   "${CMAKE_CURRENT_SOURCE_DIR}/agg/src/agg_vcgen_contour.cpp"
   "${CMAKE_CURRENT_SOURCE_DIR}/agg/src/agg_vcgen_dash.cpp"
   "${CMAKE_CURRENT_SOURCE_DIR}/agg/src/agg_vcgen_markers_term.cpp"
   "${CMAKE_CURRENT_SOURCE_DIR}/agg/src/agg_vcgen_smooth_poly1.cpp"
   "${CMAKE_CURRENT_SOURCE_DIR}/agg/src/agg_vcgen_stroke.cpp"
   "${CMAKE_CURRENT_SOURCE_DIR}/agg/src/agg_vcgen_bspline.cpp"
   "${CMAKE_CURRENT_SOURCE_DIR}/agg/src/agg_image_filters.cpp"
   "${CMAKE_CURRENT_SOURCE_DIR}/agg/src/agg_line_aa_basics.cpp"
   "${CMAKE_CURRENT_SOURCE_DIR}/agg/src/agg_line_profile_aa.cpp"
   "${CMAKE_CURRENT_SOURCE_DIR}/agg/src/agg_sqrt_tables.cpp"
   "${CMAKE_CURRENT_SOURCE_DIR}/agg/src/agg_trans_affine.cpp"
   #${CMAKE_CURRENT_SOURCE_DIR}/agg/src/gpc/gpc.c
   #${CMAKE_CURRENT_SOURCE_DIR}/agg/src/agg_trans_double_path.cpp"
   "${CMAKE_CURRENT_SOURCE_DIR}/agg/src/agg_trans_single_path.cpp"
   "${CMAKE_CURRENT_SOURCE_DIR}/agg/src/agg_vpgen_clip_polygon.cpp"
   "${CMAKE_CURRENT_SOURCE_DIR}/agg/src/agg_vpgen_clip_polyline.cpp"
   "${CMAKE_CURRENT_SOURCE_DIR}/agg/src/agg_vpgen_segmentator.cpp")

target_include_directories (${MOD} PRIVATE
   "${CMAKE_CURRENT_SOURCE_DIR}/agg/include")

# Pre-compiling the main header speeds up compilation and eliminates the need to include it in source files.

target_precompile_headers(${MOD} PRIVATE
   "vector.h" "font.h")

target_link_libraries (${MOD} PUBLIC unicode)
target_link_libraries (${MOD} PUBLIC linear_rgb)
target_link_libraries (${MOD} PRIVATE ${MATH_LINK} freetype)
