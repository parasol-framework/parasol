# MP3 module

set (MOD "mp3")
set (INC_MOD_MP3 TRUE PARENT_SCOPE)

add_library (${MOD})
set_module_defaults (${MOD})
target_sources (${MOD} PRIVATE "mp3.cpp")

flute_test (mp3_audio "tests/test-mp3.fluid")
