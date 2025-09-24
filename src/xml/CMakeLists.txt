# XML Module

set (MOD xml)
set (INC_MOD_XML TRUE PARENT_SCOPE)

idl_gen ("${MOD}.fdl" NAME ${MOD}_defs
   OUTPUT "${INCLUDE_OUTPUT}/modules/${MOD}.h"
   APPEND_IDL "${MOD}_def.c")

add_library (${MOD})
target_link_libraries (${MOD} PUBLIC unicode)
set_module_defaults (${MOD} "Xml")

target_sources (${MOD} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/${MOD}.cpp")

flute_test (${MOD}_basic "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_basic.fluid")
flute_test (${MOD}_advanced "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_advanced_features.fluid")
flute_test (${MOD}_data_sources "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_data_sources.fluid")
flute_test (${MOD}_manipulation "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_xml_manipulation.fluid")
flute_test (${MOD}_xpath_core "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_xpath_core.fluid")
flute_test (${MOD}_xpath_predicates "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_xpath_predicates.fluid")
flute_test (${MOD}_xpath_axes "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_xpath_axes.fluid")
flute_test (${MOD}_parsing "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_xml_parsing.fluid")
flute_test (${MOD}_namespaces "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_namespaces.fluid")

if (INSTALL_TESTS)
   add_executable (test_xml_count_debug "tests/test_count_debug.cpp")
   target_link_libraries (test_xml_count_debug PRIVATE ${INIT_LINK})
   set_target_properties (test_xml_count_debug PROPERTIES CXX_STANDARD 20 LINK_FLAGS_RELEASE "-s")
   install (TARGETS test_xml_count_debug DESTINATION "tests/")
endif ()
