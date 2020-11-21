# Vector module

set (MOD vector)

idl_c ("${MOD}.fdl" NAME ${MOD}_defs OUTPUT "${INCLUDE_OUTPUT}/modules/vector.h"
   FILES "vector_functions.cpp"
   ARGS "--output-defs=module_def.c")

idl_def ("${MOD}.fdl" NAME ${MOD}_idl OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/idl.h")

add_library (${MOD} SHARED)

set_module_defaults (${MOD})

target_compile_options (${MOD} PRIVATE
  "$<${GCC_COMPAT}:$<BUILD_INTERFACE:-Wno-narrowing;-Wno-uninitialized>>")

target_sources (${MOD} PRIVATE
   "${CMAKE_CURRENT_SOURCE_DIR}/vector.cpp"
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
   "${CMAKE_CURRENT_SOURCE_DIR}/agg/src/agg_rounded_rect.cpp"
   "${CMAKE_CURRENT_SOURCE_DIR}/agg/src/agg_sqrt_tables.cpp"
   "${CMAKE_CURRENT_SOURCE_DIR}/agg/src/agg_trans_affine.cpp"
   #${CMAKE_CURRENT_SOURCE_DIR}/agg/src/gpc/gpc.c
   #${CMAKE_CURRENT_SOURCE_DIR}/agg/src/agg_trans_double_path.cpp"
   "${CMAKE_CURRENT_SOURCE_DIR}/agg/src/agg_trans_single_path.cpp"
   "${CMAKE_CURRENT_SOURCE_DIR}/agg/src/agg_vpgen_clip_polygon.cpp"
   "${CMAKE_CURRENT_SOURCE_DIR}/agg/src/agg_vpgen_clip_polyline.cpp"
   "${CMAKE_CURRENT_SOURCE_DIR}/agg/src/agg_vpgen_segmentator.cpp")

target_include_directories (${MOD} PRIVATE
   "${CMAKE_CURRENT_SOURCE_DIR}/agg/include"
   "${CMAKE_CURRENT_SOURCE_DIR}/../font/freetype-2.10.2/include")

target_link_libraries (${MOD} PRIVATE "m")
