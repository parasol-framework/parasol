# PARC Module

set (MOD parc)

add_library (${MOD} SHARED)

set_module_defaults (${MOD})

target_sources (${MOD} PRIVATE "${MOD}.c")

# flute_test(${MOD}_test, "${CMAKE_CURRENT_SOURCE_DIR}/test/test.fluid")
