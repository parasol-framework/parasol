# XPath Module

set (MOD xpath)
set (INC_MOD_XPATH TRUE PARENT_SCOPE)

idl_gen ("${MOD}.fdl"
   NAME ${MOD}_defs
   OUTPUT "${INCLUDE_OUTPUT}/modules/${MOD}.h"
   FILES
     "xpath.cpp"
   ARGS  "output-defs=xpath_def.c" "output-proto=xpath_def.c"
   APPEND_IDL "${MOD}_def.c")

add_library (${MOD})
target_link_libraries (${MOD} PUBLIC unicode)
set_module_defaults (${MOD} "Xp")
# Enable unit tests for deep testing of non-release builds only.
#set (ENABLE_UNIT_TESTS ON)

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
add_compile_definitions("PRV_XML" "PRV_XPATH_MODULE")
add_dependencies (xpath build_headers xml)

# Module Code

target_sources (${MOD} PRIVATE
   "${CMAKE_CURRENT_SOURCE_DIR}/${MOD}.cpp"
   $<TARGET_OBJECTS:xml_xpath_functions>
   $<TARGET_OBJECTS:xml_schema>)

if (ENABLE_UNIT_TESTS)
   target_compile_definitions(${MOD} PRIVATE "ENABLE_UNIT_TESTS")
endif ()

# Tests

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
flute_test (${MOD}_xquery_prolog "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_prolog.fluid")
flute_test (${MOD}_xpath_core "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_core.fluid")
flute_test (${MOD}_xpath_predicates "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_predicates.fluid")
flute_test (${MOD}_xpath_axes "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_axes.fluid")
flute_test (${MOD}_xpath_advanced "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_advanced.fluid")
flute_test (${MOD}_xpath_advanced_paths "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_advanced_paths.fluid")
flute_test (${MOD}_xpath_flwor "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_flwor.fluid")
flute_test (${MOD}_xpath_flwor_clauses "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_flwor_clauses.fluid")
flute_test (${MOD}_xpath_module_loading "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_module_loading.fluid")

if (ENABLE_UNIT_TESTS)
   flute_test (${MOD}_xpath_unit_tests "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_unit_tests.fluid")
endif ()
