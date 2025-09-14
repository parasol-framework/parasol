# HTTP Module

set (MOD http)
set (INC_MOD_HTTP TRUE PARENT_SCOPE)

# Use XXD to convert the auth.fluid script to a C binary file

if (XXD_CMD STREQUAL "XXD_CMD-NOTFOUND")
	message (STATUS "The xxd command is unavailable on this system")
else ()
   add_custom_command (OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/auth.c"
      COMMAND bash -c "\"${XXD_CMD}\" -c20 -i \"${CMAKE_CURRENT_SOURCE_DIR}/auth.fluid\" | sed 's/.*_auth_fluid\\[\\]/unsigned char glAuthScript[]/g;s/.*_auth_fluid_len/unsigned int glAuthScriptLength/g' > auth.c"
      WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
      COMMENT "Generating C binary from auth.fluid"
      VERBATIM)

   add_custom_target (http_auth DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/auth.c")
   add_dependencies (build_headers http_auth)
endif ()

idl_gen ("${MOD}.fdl" NAME ${MOD}_defs OUTPUT "${INCLUDE_OUTPUT}/modules/${MOD}.h" APPEND_IDL "${MOD}_def.c")

add_library (${MOD})
target_link_libraries (${MOD} PUBLIC base64)
set_module_defaults (${MOD} "Http")
target_sources (${MOD} PRIVATE "${MOD}.cpp" "auth.c")

flute_test (http "tests/test_http.fluid")
flute_test (http_validate "tests/test_validate_header.fluid")
