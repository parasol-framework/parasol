# Document module

set (MOD "document")
set (INC_MOD_DOCUMENT TRUE PARENT_SCOPE)

idl_gen ("defs/${MOD}.fdl" NAME ${MOD}_defs OUTPUT "${INCLUDE_OUTPUT}/modules/${MOD}.h" APPEND_IDL "defs/module_def.c"
   FILES "${MOD}.cpp"
   ARGS "output-defs=defs/module_def.c" "output-proto=defs/module_def.c" "prototypes=static")

idl_gen ("defs/hashes.fdl" NAME ${MOD}_hashes OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/defs/hashes.h")

add_library (${MOD})
target_link_libraries (${MOD} PUBLIC unicode)
set_module_defaults (${MOD} "Doc")
target_sources (${MOD} PRIVATE "${MOD}.cpp")

# Enable precompiled headers for the document module to speed up compilation
target_precompile_headers(${MOD} PRIVATE
   <parasol/main.h>
   <parasol/modules/xml.h>
   <parasol/modules/document.h>
   <parasol/modules/font.h>
   <parasol/modules/display.h>
   <parasol/modules/svg.h>)

flute_test (doc_output "tests/test-doc.fluid")
