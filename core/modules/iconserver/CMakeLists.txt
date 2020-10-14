# IconServer Module

set (MOD iconserver)

idl_c ("${MOD}.fdl" NAME ${MOD}_defs OUTPUT "${INCLUDE_OUTPUT}/modules/${MOD}.h"
   FILES "${MOD}.c"
   ARGS "--output-defs=module_def.c" "--prototypes=static")

add_library (${MOD} SHARED)

set_module_defaults (${MOD})

target_sources (${MOD} PRIVATE "${MOD}.c")
