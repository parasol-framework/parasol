# XPath Module

set (MOD xpath)
set (INC_MOD_XPATH TRUE PARENT_SCOPE)

idl_gen ("${MOD}.fdl"
   NAME ${MOD}_defs
   OUTPUT "${INCLUDE_OUTPUT}/modules/${MOD}.h"
   FILES 
     "xpath.cpp" 
     "unit_tests.cpp"
   ARGS  "output-defs=xpath_def.c" "output-proto=xpath_def.c"
   APPEND_IDL "${MOD}_def.c")

add_library (${MOD})
target_link_libraries (${MOD} PUBLIC unicode)
set_module_defaults (${MOD} "Xp")

# XPath Support Library

add_library (xml_xpath_functions OBJECT
   "${CMAKE_CURRENT_SOURCE_DIR}/eval/eval_common.cpp"
   "${CMAKE_CURRENT_SOURCE_DIR}/eval/eval_navigation.cpp"
   "${CMAKE_CURRENT_SOURCE_DIR}/eval/eval_context.cpp"
   "${CMAKE_CURRENT_SOURCE_DIR}/eval/eval_predicates.cpp"
   "${CMAKE_CURRENT_SOURCE_DIR}/eval/eval_values.cpp"
   "${CMAKE_CURRENT_SOURCE_DIR}/eval/eval_expression.cpp"
   "${CMAKE_CURRENT_SOURCE_DIR}/eval/eval_flwor.cpp"
   "${CMAKE_CURRENT_SOURCE_DIR}/eval/eval.cpp"
   "${CMAKE_CURRENT_SOURCE_DIR}/api/xpath_functions.cpp"
   "${CMAKE_CURRENT_SOURCE_DIR}/api/xpath_axis.cpp"
   "${CMAKE_CURRENT_SOURCE_DIR}/api/xquery_prolog.cpp"
   "${CMAKE_CURRENT_SOURCE_DIR}/parse/xpath_parser.cpp"
   "${CMAKE_CURRENT_SOURCE_DIR}/parse/xpath_tokeniser.cpp")

target_include_directories (xml_xpath_functions PRIVATE "${PROJECT_SOURCE_DIR}/include")
set_target_properties (xml_xpath_functions PROPERTIES CXX_STANDARD 20 POSITION_INDEPENDENT_CODE ON)
add_compile_definitions("PRV_XPATH_MODULE" "PRV_XML")
add_dependencies (xml_xpath_functions build_headers)

# Module Code

target_sources (${MOD} PRIVATE
   "${CMAKE_CURRENT_SOURCE_DIR}/${MOD}.cpp"
   "${CMAKE_CURRENT_SOURCE_DIR}/unit_tests.cpp"
   $<TARGET_OBJECTS:xml_xpath_functions>
   $<TARGET_OBJECTS:xml_schema>)

# Tests

add_executable (test_xpath_unit "tests/test_xpath_unit.cpp")
target_link_libraries (test_xpath_unit PRIVATE ${INIT_LINK})
set_target_properties (test_xpath_unit PROPERTIES CXX_STANDARD 20 LINK_FLAGS_RELEASE "-s")

if (PARASOL_STATIC)
   # Unit tests can run from build directory in static mode.  Otherwise needs to be installed.
   add_test (NAME test_xpath_unit COMMAND test_xpath_unit)
   set_tests_properties (test_xpath_unit PROPERTIES LABELS xpath)
endif ()

if (INSTALL_TESTS)
   install (TARGETS test_xpath_unit DESTINATION "tests/")
endif ()

flute_test (${MOD}_xpath_reserved_words "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_reserved_words.fluid")
flute_test (${MOD}_xpath_func_ext "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_func_ext.fluid")
flute_test (${MOD}_xpath_sequences "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_sequences.fluid")
flute_test (${MOD}_xpath_string_uri "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_string_uri.fluid")
flute_test (${MOD}_xpath_duration "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_duration.fluid")
flute_test (${MOD}_xpath_datetime "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_datetime.fluid")
flute_test (${MOD}_xpath_qname "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_qname.fluid")
flute_test (${MOD}_xpath_documents "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_documents.fluid")
flute_test (${MOD}_schema_validation "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_schema_validation.fluid")
flute_test (${MOD}_xpath_accessor "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_accessor.fluid")
flute_test (${MOD}_xpath_constructors "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_constructors.fluid")
flute_test (${MOD}_xquery_prolog "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_xquery_prolog.fluid")
flute_test (${MOD}_xpath_core "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_core.fluid")
flute_test (${MOD}_xpath_predicates "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_predicates.fluid")
flute_test (${MOD}_xpath_axes "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_axes.fluid")
flute_test (${MOD}_xpath_advanced "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_advanced.fluid")
flute_test (${MOD}_xpath_advanced_paths "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_advanced_paths.fluid")
flute_test (${MOD}_xpath_flwor "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_flwor.fluid")
flute_test (${MOD}_xpath_flwor_clauses "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_flwor_clauses.fluid")
