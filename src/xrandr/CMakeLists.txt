# XRandR Module
# Used by the Display module for full-screen graphics

set (MOD xrandr)

   CHECK_C_SOURCE_COMPILES ("
#include <X11/extensions/Xrandr.h>
int main() {
   return 0;
}"
   XRANDR_INSTALLED)

   if (XRANDR_INSTALLED)
      idl_c ("${MOD}.fdl" NAME ${MOD}_defs OUTPUT "${INCLUDE_OUTPUT}/modules/${MOD}.h" FILES "${MOD}.cpp"
         ARGS "--output-defs=module_def.c" "--prototypes=static")

      link_directories("/usr/X11R6/lib" "/usr/lib")

      add_library (${MOD} SHARED)

      set_module_defaults (${MOD})

      target_sources (${MOD} PRIVATE "${MOD}.cpp")
      target_link_libraries (${MOD} PRIVATE Xrender Xrandr Xext) # x11/libXxf86dga.a
   endif ()
