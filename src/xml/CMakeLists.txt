# XML Module

set (MOD xml)
set (INC_MOD_XML TRUE PARENT_SCOPE)

idl_gen ("${MOD}.fdl" NAME ${MOD}_defs
   OUTPUT "${INCLUDE_OUTPUT}/modules/${MOD}.h"
   APPEND_IDL "${MOD}_def.c")

add_library (${MOD})

set_module_defaults (${MOD})

target_sources (${MOD} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/${MOD}.cpp")

flute_test (${MOD}_tests "${CMAKE_CURRENT_SOURCE_DIR}/tests/test.fluid")
