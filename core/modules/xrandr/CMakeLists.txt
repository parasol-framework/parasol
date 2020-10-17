# XRandR Module
# Used by the Display module for full-screen graphics

set (MOD xrandr)

idl_c ("${MOD}.fdl" NAME ${MOD}_defs OUTPUT "${INCLUDE_OUTPUT}/modules/${MOD}.h" FILES "${MOD}.c"
   ARGS "--output-defs=module_def.c" "--prototypes=static")

link_directories("/usr/X11R6/lib" "/usr/lib")

add_library (${MOD} SHARED)

set_module_defaults (${MOD})

target_sources (${MOD} PRIVATE "${MOD}.c")
target_link_libraries (${MOD} PRIVATE Xrender Xrandr Xext) # x11/libXxf86dga.a
