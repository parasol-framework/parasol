# XQuery Module

# Enable unit tests for deep testing of non-release builds only.
#set (ENABLE_UNIT_TESTS ON)

set (MOD xquery)
set (INC_MOD_XQUERY TRUE PARENT_SCOPE)

idl_gen ("${MOD}.fdl"
   NAME ${MOD}_defs
   OUTPUT "${INCLUDE_OUTPUT}/modules/${MOD}.h"
   FILES "xquery.cpp"
   ARGS  "output-defs=xquery_def.cpp" "output-proto=xquery_def.cpp"
   APPEND_IDL "${MOD}_def.cpp")

add_library (${MOD})
target_link_libraries (${MOD} PUBLIC unicode)
set_module_defaults (${MOD} "Xq")

# XQuery Support Library

add_library (xquery_lib OBJECT
   "${CMAKE_CURRENT_SOURCE_DIR}/${MOD}.cpp"
   "${CMAKE_CURRENT_SOURCE_DIR}/eval/eval_common.cpp"
   "${CMAKE_CURRENT_SOURCE_DIR}/eval/eval_navigation.cpp"
   "${CMAKE_CURRENT_SOURCE_DIR}/eval/eval_context.cpp"
   "${CMAKE_CURRENT_SOURCE_DIR}/eval/eval_predicates.cpp"
   "${CMAKE_CURRENT_SOURCE_DIR}/eval/eval_values.cpp"
   "${CMAKE_CURRENT_SOURCE_DIR}/eval/eval_expression.cpp"
   "${CMAKE_CURRENT_SOURCE_DIR}/eval/eval_flwor.cpp"
   "${CMAKE_CURRENT_SOURCE_DIR}/eval/date_time_utils.cpp"
   "${CMAKE_CURRENT_SOURCE_DIR}/eval/eval.cpp"
   "${CMAKE_CURRENT_SOURCE_DIR}/api/xquery_functions.cpp"
   "${CMAKE_CURRENT_SOURCE_DIR}/api/xquery_axis.cpp"
   "${CMAKE_CURRENT_SOURCE_DIR}/api/xquery_prolog.cpp"
   "${CMAKE_CURRENT_SOURCE_DIR}/functions/func_math.cpp"
   "${CMAKE_CURRENT_SOURCE_DIR}/parse/xquery_parser.cpp"
   "${CMAKE_CURRENT_SOURCE_DIR}/parse/xquery_tokeniser.cpp")

target_include_directories (xquery_lib PRIVATE "${PROJECT_SOURCE_DIR}/include")
set_target_properties (xquery_lib PROPERTIES CXX_STANDARD 20 POSITION_INDEPENDENT_CODE ON)
target_compile_definitions (xquery_lib PRIVATE "PRV_XML" "PRV_XQUERY_MODULE")
add_dependencies (xquery build_headers xml)

# Enable precompiled headers for the xquery functions object library
# This must be done before the module target to avoid circular dependencies

target_precompile_headers(xquery_lib PRIVATE
   # Heavy STL headers used across most files
   <string>
   <string_view>
   <vector>
   <memory>
   <optional>
   <format>
   <utility>
   <unordered_map>
   <unordered_set>
   <algorithm>
   # Parasol framework headers
   <parasol/main.h>
   <parasol/modules/xml.h>
   <parasol/modules/xquery.h>
   <parasol/strings.hpp>
   # Third-party dependency
   <ankerl/unordered_dense.h>
   # Core internal headers
   "${CMAKE_CURRENT_SOURCE_DIR}/../xml/xml.h"
   "${CMAKE_CURRENT_SOURCE_DIR}/xquery.h")

# Enable unity builds to speed up compilation of the xquery functions library

set_target_properties(xquery_lib PROPERTIES UNITY_BUILD ON)
set_target_properties(xquery_lib PROPERTIES UNITY_BUILD_BATCH_SIZE 8)

# Module Code

target_sources (${MOD} PRIVATE
   $<TARGET_OBJECTS:xquery_lib>
   $<TARGET_OBJECTS:xml_schema>)

if (ENABLE_UNIT_TESTS)
   target_compile_definitions(${MOD} PRIVATE "ENABLE_UNIT_TESTS")
endif ()

# Tests

flute_test (${MOD}_reserved_words "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_reserved_words.fluid")
flute_test (${MOD}_func_ext "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_func_ext.fluid")
flute_test (${MOD}_sequences "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_sequences.fluid")
flute_test (${MOD}_string_uri "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_string_uri.fluid")
flute_test (${MOD}_duration "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_duration.fluid")
flute_test (${MOD}_datetime "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_datetime.fluid")
flute_test (${MOD}_edge_cases "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_edge_cases.fluid")
flute_test (${MOD}_qname "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_qname.fluid")
flute_test (${MOD}_documents "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_documents.fluid")
flute_test (${MOD}_accessor "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_accessor.fluid")
flute_test (${MOD}_constructors "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_constructors.fluid")
flute_test (${MOD}_prolog "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_prolog.fluid")
flute_test (${MOD}_core "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_core.fluid")
flute_test (${MOD}_predicates "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_predicates.fluid")
flute_test (${MOD}_axes "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_axes.fluid")
flute_test (${MOD}_advanced "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_advanced.fluid")
flute_test (${MOD}_advanced_paths "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_advanced_paths.fluid")
flute_test (${MOD}_flwor "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_flwor.fluid")
flute_test (${MOD}_module_loading "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_module_loading.fluid")
flute_test (${MOD}_type_expr "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_type_expr.fluid")
flute_test (${MOD}_type_constructors "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_type_constructors.fluid")
flute_test (${MOD}_sequence_cardinality "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_sequence_cardinality.fluid")
flute_test (${MOD}_math "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_math.fluid")
flute_test (${MOD}_qt_math "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_qt_math.fluid")

if (ENABLE_UNIT_TESTS)
   flute_test (xquery_unit_tests "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_unit_tests.fluid")
endif ()
