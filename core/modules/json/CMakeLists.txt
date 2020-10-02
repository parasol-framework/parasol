# JSON Module

set (MOD json)

add_library (${MOD} SHARED)
set_module_defaults (${MOD})
target_sources (${MOD} PRIVATE ${MOD}.c)
