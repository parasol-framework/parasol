# Audio module
# Debian system preparation: "apt-get install libasound2-dev"

set (MOD "audio")
set (INC_MOD_AUDIO TRUE PARENT_SCOPE)

   CHECK_C_SOURCE_COMPILES ("
#include <alsa/asoundlib.h>
int main() {
   return 0;
}"
   ALSA_INSTALLED)

idl_gen ("${MOD}.fdl" NAME ${MOD}_defs
   OUTPUT "${INCLUDE_OUTPUT}/modules/${MOD}.h"
   APPEND_IDL "module_def.c"
   FILES "${MOD}.cpp" "commands.cpp"
   ARGS "output-defs=module_def.c" "output-proto=module_def.c")

set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast")

if (WIN32)
   set (C_INCLUDE_PATH "")
endif ()

add_library (${MOD})

set_module_defaults (${MOD})

target_sources (${MOD} PRIVATE "audio.cpp")

if (WIN32)
   target_sources (${MOD} PRIVATE "dsound.cpp")
else ()
   if (ALSA_INSTALLED)
      add_definitions ("-DALSA_ENABLED")
      target_link_libraries (${MOD} PRIVATE "asound")
   endif ()
endif ()

flute_test (audio "tests/test-audio.fluid")
