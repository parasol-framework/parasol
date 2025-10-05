# XML Module

set (MOD xml)
set (INC_MOD_XML TRUE PARENT_SCOPE)

idl_gen ("${MOD}.fdl" NAME ${MOD}_defs
   OUTPUT "${INCLUDE_OUTPUT}/modules/${MOD}.h"
   APPEND_IDL "${MOD}_def.c")

add_library (${MOD})
target_link_libraries (${MOD} PUBLIC unicode)
target_link_libraries (${MOD} PRIVATE link_regex)
set_module_defaults (${MOD} "Xml")

# Unescape Library

add_library (xml_unescape OBJECT "${CMAKE_CURRENT_SOURCE_DIR}/unescape.cpp")
target_include_directories (xml_unescape PRIVATE "${PROJECT_SOURCE_DIR}/include")
set_target_properties (xml_unescape PROPERTIES CXX_STANDARD 20 POSITION_INDEPENDENT_CODE ON)
add_dependencies (xml_unescape build_headers)

# Schema Support Library

add_library (xml_schema OBJECT
   "${CMAKE_CURRENT_SOURCE_DIR}/schema/schema_types.cpp"
   "${CMAKE_CURRENT_SOURCE_DIR}/schema/schema_parser.cpp"
   "${CMAKE_CURRENT_SOURCE_DIR}/schema/type_checker.cpp")

target_include_directories (xml_schema PRIVATE "${PROJECT_SOURCE_DIR}/include")
set_target_properties (xml_schema PROPERTIES CXX_STANDARD 20 POSITION_INDEPENDENT_CODE ON)
add_dependencies (xml_schema build_headers)

# Module Code

target_sources (${MOD} PRIVATE
   "${CMAKE_CURRENT_SOURCE_DIR}/${MOD}.cpp"
   $<TARGET_OBJECTS:xml_schema>)

# Tests

flute_test (${MOD}_basic "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_basic.fluid")
flute_test (${MOD}_advanced "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_advanced_features.fluid")
flute_test (${MOD}_data_sources "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_data_sources.fluid")
flute_test (${MOD}_manipulation "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_xml_manipulation.fluid")
flute_test (${MOD}_xpath_core "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_xpath_core.fluid")
flute_test (${MOD}_xpath_predicates "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_xpath_predicates.fluid")
flute_test (${MOD}_xpath_axes "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_xpath_axes.fluid")
flute_test (${MOD}_xpath_advanced "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_xpath_advanced.fluid")
flute_test (${MOD}_xpath_advanced_paths "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_xpath_advanced_paths.fluid")
flute_test (${MOD}_xpath_flwor "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_xpath_flwor.fluid")
flute_test (${MOD}_parsing "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_xml_parsing.fluid")
flute_test (${MOD}_namespaces "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_namespaces.fluid")
flute_test (${MOD}_variables "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_setvariable.fluid")
flute_test (${MOD}_xpath_func_ext "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_xpath_func_ext.fluid")
flute_test (${MOD}_xpath_sequences "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_xpath_sequences.fluid")
flute_test (${MOD}_xpath_string_uri "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_xpath_string_uri.fluid")
flute_test (${MOD}_xpath2_duration "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_xpath2_duration.fluid")
flute_test (${MOD}_xpath2_datetime "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_xpath2_datetime.fluid")
flute_test (${MOD}_xpath2_qname "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_xpath2_qname.fluid")
flute_test (${MOD}_xpath_documents "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_xpath_documents.fluid")
flute_test (${MOD}_schema_validation "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_schema_validation.fluid")
flute_test (${MOD}_xpath2_accessor "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_xpath2_accessor.fluid")

if (INSTALL_TESTS)
   # This test is redundant but is being retained as an example for C++
   add_executable (test_xml_count_debug "tests/test_count_debug.cpp")
   target_link_libraries (test_xml_count_debug PRIVATE ${INIT_LINK})
   set_target_properties (test_xml_count_debug PROPERTIES CXX_STANDARD 20 LINK_FLAGS_RELEASE "-s")
   install (TARGETS test_xml_count_debug DESTINATION "tests/")
endif ()
