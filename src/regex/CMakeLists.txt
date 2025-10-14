# Regex Module

set (MOD regex)
set (INC_MOD_REGEX TRUE PARENT_SCOPE)

idl_gen ("${MOD}.fdl"
   NAME ${MOD}_defs
   OUTPUT "${INCLUDE_OUTPUT}/modules/${MOD}.h"
   FILES "regex.cpp"
   ARGS  "output-defs=regex_def.c" "output-proto=regex_def.c"
   APPEND_IDL "${MOD}_def.c")

add_library (${MOD})
set_module_defaults (${MOD} "Rx")

target_sources (${MOD} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/${MOD}.cpp")

# ctest --build-config Release --test-dir build/[release-folder] -R test_regex_match -V

add_executable (test_regex_match "tests/test_match.cpp")
target_link_libraries (test_regex_match PRIVATE ${INIT_LINK})
set_target_properties (test_regex_match PROPERTIES CXX_STANDARD 20 LINK_FLAGS_RELEASE "-s")

flute_test (regex_captures "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_captures.fluid")
flute_test (regex_basics "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_basics.fluid")
flute_test (regex_flags_modes "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_flags_modes.fluid")
flute_test (regex_character_classes "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_character_classes.fluid")
flute_test (regex_quantifiers "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_quantifiers.fluid")
flute_test (regex_anchors_boundaries "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_anchors_boundaries.fluid")
flute_test (regex_groups_backrefs "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_groups_backrefs.fluid")
flute_test (regex_lookaround "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_lookaround.fluid")
flute_test (regex_unicode "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_unicode.fluid")
flute_test (regex_replace_behaviour "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_replace_behaviour.fluid")
flute_test (regex_split_tokenisation "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_split_tokenisation.fluid")
flute_test (regex_search_iteration "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_search_iteration.fluid")
flute_test (regex_error_edgecases "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_error_edgecases.fluid")

if (PARASOL_STATIC)
   # Test can run from build directory in static mode.  Otherwise needs to be installed.
   add_test (NAME test_regex_match COMMAND test_regex_match)
   set_tests_properties (test_regex_match PROPERTIES LABELS regex)
endif ()

if (INSTALL_TESTS)
   install (TARGETS test_regex_match DESTINATION "tests/")
endif ()
