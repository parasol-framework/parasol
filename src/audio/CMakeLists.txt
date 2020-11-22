# Audio module
# Debian system preparation: "apt-get install libasound2-dev"

set (MOD "audio")

idl_all ("${MOD}.fdl" NAME ${MOD}_defs
   OUTPUT "${INCLUDE_OUTPUT}/modules/${MOD}.h"
   APPEND_IDL "module_def.c"
   FILES "${MOD}.c"
   ARGS "--output-defs=module_def.c" "--output-proto=module_def.c" "--prototypes=static")

set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast")

if (WIN32)
   set (C_INCLUDE_PATH "")
endif ()

add_library (${MOD} SHARED)

set_module_defaults (${MOD})

target_sources (${MOD} PRIVATE "audio.c")

if (WIN32)
   target_sources (${MOD} PRIVATE "dsound.c")
else ()
   target_link_libraries (${MOD} PRIVATE "asound")
endif ()
