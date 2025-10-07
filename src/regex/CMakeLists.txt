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

target_sources (${MOD} PRIVATE
   "${CMAKE_CURRENT_SOURCE_DIR}/${MOD}.cpp")
