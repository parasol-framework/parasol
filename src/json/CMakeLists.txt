# JSON Module

set (MOD json)

add_library (${MOD} SHARED)
set_module_defaults (${MOD})
target_sources (${MOD} PRIVATE ${MOD}.cpp)

flute_test (json "test_json.fluid")
