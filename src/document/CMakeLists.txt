# Document module

set (MOD "document")
set (INC_MOD_DOCUMENT TRUE PARENT_SCOPE)

idl_gen ("defs/${MOD}.fdl" NAME ${MOD}_defs OUTPUT "${INCLUDE_OUTPUT}/modules/${MOD}.h" APPEND_IDL "defs/module_def.c"
   FILES "${MOD}.cpp"
   ARGS "output-defs=defs/module_def.c" "output-proto=defs/module_def.c" "prototypes=static")

idl_gen ("defs/hashes.fdl" NAME ${MOD}_hashes OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/defs/hashes.h")

add_library (${MOD})
set_module_defaults (${MOD})
target_sources (${MOD} PRIVATE "${MOD}.cpp")

flute_test (doc_output "tests/test-doc.fluid")
