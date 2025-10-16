# XPath Module

set (MOD xpath)
set (INC_MOD_XPATH TRUE PARENT_SCOPE)

idl_gen ("${MOD}.fdl"
   NAME ${MOD}_defs
   OUTPUT "${INCLUDE_OUTPUT}/modules/${MOD}.h"
   FILES "xpath.cpp"
   ARGS  "output-defs=xpath_def.c" "output-proto=xpath_def.c"
   APPEND_IDL "${MOD}_def.c")

add_library (${MOD})
target_link_libraries (${MOD} PUBLIC unicode)
set_module_defaults (${MOD} "Xp")

# XPath Support Library

add_library (xml_xpath_functions OBJECT
   "${CMAKE_CURRENT_SOURCE_DIR}/eval_common.cpp"
   "${CMAKE_CURRENT_SOURCE_DIR}/eval_navigation.cpp"
   "${CMAKE_CURRENT_SOURCE_DIR}/eval_context.cpp"
   "${CMAKE_CURRENT_SOURCE_DIR}/eval_predicates.cpp"
   "${CMAKE_CURRENT_SOURCE_DIR}/eval_values.cpp"
   "${CMAKE_CURRENT_SOURCE_DIR}/eval_expression.cpp"
   "${CMAKE_CURRENT_SOURCE_DIR}/eval_flwor.cpp"
   "${CMAKE_CURRENT_SOURCE_DIR}/eval.cpp"
   "${CMAKE_CURRENT_SOURCE_DIR}/xpath_functions.cpp"
   "${CMAKE_CURRENT_SOURCE_DIR}/xpath_axis.cpp"
   "${CMAKE_CURRENT_SOURCE_DIR}/xpath_parser.cpp"
   "${CMAKE_CURRENT_SOURCE_DIR}/xpath_tokenizer.cpp")

target_include_directories (xml_xpath_functions PRIVATE "${PROJECT_SOURCE_DIR}/include")
set_target_properties (xml_xpath_functions PROPERTIES CXX_STANDARD 20 POSITION_INDEPENDENT_CODE ON)
add_compile_definitions("PRV_XPATH_MODULE" "PRV_XML")
add_dependencies (xml_xpath_functions build_headers)

# Module Code

target_sources (${MOD} PRIVATE
   "${CMAKE_CURRENT_SOURCE_DIR}/${MOD}.cpp"
   $<TARGET_OBJECTS:xml_xpath_functions>
   $<TARGET_OBJECTS:xml_schema>)
