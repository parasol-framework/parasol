# MP3 module

set (MOD "mp3")

add_library (${MOD} SHARED)

set_module_defaults (${MOD})

target_sources (${MOD} PRIVATE "mp3.cpp")

flute_test (mp3_audio "tests/test-mp3.fluid")
