# PARC Module

set (MOD parc)

idl_all ("${MOD}.fdl" NAME ${MOD}_defs APPEND_IDL "${MOD}_def.c" OUTPUT "${INCLUDE_OUTPUT}/modules/${MOD}.h")

add_library (${MOD} SHARED)

set_module_defaults (${MOD})

target_sources (${MOD} PRIVATE "${MOD}.cpp")

# flute_test(${MOD}_test, "${CMAKE_CURRENT_SOURCE_DIR}/test/test.fluid")
