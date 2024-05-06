# SVG module

set (MOD svg)
set (INC_MOD_SVG TRUE PARENT_SCOPE)

idl_gen ("${MOD}.fdl" NAME ${MOD}_defs
   OUTPUT "${INCLUDE_OUTPUT}/modules/${MOD}.h"
   FILES "class_rsvg.cpp" "class_svg.cpp")

idl_compile ("${MOD}.fdl" NAME ${MOD}_idl OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/${MOD}_def.c")

add_library (${MOD})

set_module_defaults (${MOD})

if (MSVC)
   # For Katana
   add_compile_definitions ("YY_NO_UNISTD_H")
endif ()

target_sources (${MOD} PRIVATE
   "${CMAKE_CURRENT_SOURCE_DIR}/${MOD}.cpp"
   "${CMAKE_CURRENT_SOURCE_DIR}/katana-parser/src/foundation.c"
   "${CMAKE_CURRENT_SOURCE_DIR}/katana-parser/src/katana.lex.c"
   "${CMAKE_CURRENT_SOURCE_DIR}/katana-parser/src/katana.tab.c"
   "${CMAKE_CURRENT_SOURCE_DIR}/katana-parser/src/parser.c"
   "${CMAKE_CURRENT_SOURCE_DIR}/katana-parser/src/selector.c"
   "${CMAKE_CURRENT_SOURCE_DIR}/katana-parser/src/tokenizer.c")

target_include_directories (${MOD} PRIVATE
   "${CMAKE_CURRENT_SOURCE_DIR}/katana-parser/src")

target_link_libraries (${MOD} PUBLIC linear_rgb)

target_link_libraries (${MOD} PRIVATE ${MATH_LINK})

flute_test (svg_output "tests/test-svg.fluid")
