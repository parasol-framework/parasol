# Surface module

set (MOD surface)

idl_all ("${MOD}.fdl" NAME ${MOD}_defs
   OUTPUT "${INCLUDE_OUTPUT}/modules/${MOD}.h"
   APPEND_IDL "module_def.c"
   FILES "${MOD}.c"
   ARGS "--output-defs=module_def.c" "--output-proto=module_def.c" "--prototypes=static")

add_library (${MOD} SHARED)

set_module_defaults (${MOD})

if (WIN32)
   target_sources (${MOD} PRIVATE "${MOD}.c" "class_surface/win32/windows.c")
else ()
   target_sources (${MOD} PRIVATE "${MOD}.c")
endif ()
