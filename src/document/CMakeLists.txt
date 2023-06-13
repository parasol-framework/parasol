# Document module

set (MOD "document")
set (INC_MOD_DOCUMENT TRUE PARENT_SCOPE)

idl_all ("${MOD}.fdl" NAME ${MOD}_defs OUTPUT "${INCLUDE_OUTPUT}/modules/${MOD}.h" APPEND_IDL "module_def.c"
   FILES "${MOD}.cpp"
   ARGS "output-defs=module_def.c" "output-proto=module_def.c" "prototypes=static")

idl_c ("hashes.fdl" NAME ${MOD}_hashes OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/hashes.h")

add_library (${MOD})

set_module_defaults (${MOD})

target_sources (${MOD} PRIVATE "${MOD}.cpp")
