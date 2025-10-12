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

if (PARASOL_STATIC)
   add_test (NAME test_regex_match COMMAND test_regex_match)
else () # Have to run from install directory to find Parasol modules
   add_test (NAME test_regex_match COMMAND test_regex_match
      WORKING_DIRECTORY "${CMAKE_INSTALL_PREFIX}")
endif ()

set_tests_properties (test_regex_match PROPERTIES LABELS regex)

if (INSTALL_TESTS)
   install (TARGETS test_regex_match DESTINATION "tests/")
endif ()
