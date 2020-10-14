# Window Module

set (MOD window)

idl_all ("${MOD}.fdl" NAME ${MOD}_defs OUTPUT "${INCLUDE_OUTPUT}/modules/${MOD}.h"
   ARGS "--output-def=${CMAKE_CURRENT_SOURCE_DIR}/${MOD}_def.c"
   APPEND_IDL "${MOD}_def.c" )

add_library (${MOD} SHARED)

set_module_defaults (${MOD})

target_sources (${MOD} PRIVATE window.c)
