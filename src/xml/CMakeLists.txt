# XML Module

set (MOD xml)
set (INC_MOD_XML TRUE PARENT_SCOPE)

idl_gen ("${MOD}.fdl"
   NAME ${MOD}_defs
   OUTPUT "${INCLUDE_OUTPUT}/modules/${MOD}.h"
   FILES "xml.cpp"
   ARGS  "output-defs=xml_def.c" "output-proto=xml_def.c"
   APPEND_IDL "${MOD}_def.c")

add_library (${MOD})
target_link_libraries (${MOD} PUBLIC unicode)
set_module_defaults (${MOD} "Xml")
add_compile_definitions("PRV_XML_MODULE" "PRV_XML")

# Unescape Library

add_library (xml_unescape OBJECT "${CMAKE_CURRENT_SOURCE_DIR}/unescape.cpp")
target_include_directories (xml_unescape PRIVATE "${PROJECT_SOURCE_DIR}/include")
set_target_properties (xml_unescape PROPERTIES CXX_STANDARD 20 POSITION_INDEPENDENT_CODE ON)
add_dependencies (xml_unescape build_headers)

# Schema Support Library

add_library (xml_schema OBJECT
   "${CMAKE_CURRENT_SOURCE_DIR}/schema/schema_types.cpp"
   "${CMAKE_CURRENT_SOURCE_DIR}/schema/schema_parser.cpp"
   "${CMAKE_CURRENT_SOURCE_DIR}/schema/type_checker.cpp"
   "${CMAKE_CURRENT_SOURCE_DIR}/xpath_value.cpp")

target_include_directories (xml_schema PRIVATE "${PROJECT_SOURCE_DIR}/include")
set_target_properties (xml_schema PROPERTIES CXX_STANDARD 20 POSITION_INDEPENDENT_CODE ON)
add_dependencies (xml build_headers)

# Module Code

target_sources (${MOD} PRIVATE
   "${CMAKE_CURRENT_SOURCE_DIR}/${MOD}.cpp"
   $<TARGET_OBJECTS:xml_unescape>
   $<TARGET_OBJECTS:xml_schema>)

# Tests

flute_test (${MOD}_basic "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_basic.fluid")
flute_test (${MOD}_advanced "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_advanced_features.fluid")
flute_test (${MOD}_data_sources "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_data_sources.fluid")
flute_test (${MOD}_manipulation "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_xml_manipulation.fluid")
flute_test (${MOD}_parsing "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_xml_parsing.fluid")
flute_test (${MOD}_namespaces "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_namespaces.fluid")
flute_test (${MOD}_variables "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_setvariable.fluid")
