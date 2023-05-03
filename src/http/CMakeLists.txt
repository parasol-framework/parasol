# HTTP Module

set (MOD http)

# Use XXD to convert the auth.fluid script to a C binary file

if (XXD_CMD STREQUAL "XXD_CMD-NOTFOUND")
	message (STATUS "The xxd command is unavailable on this system")
else ()
   add_custom_command (OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/auth.c"
      PRE_BUILD
      COMMAND bash -c "${XXD_CMD} -c20 -i ${CMAKE_CURRENT_SOURCE_DIR}/auth.fluid | sed 's/auth_fluid/glAuthScript/g;s/glAuthScript_len/glAuthScriptLength/g' > auth.c"
      WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
      COMMENT "Generating C binary from auth.fluid"
      VERBATIM)

   add_custom_target (http_auth DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/auth.c")
   add_dependencies (build_headers http_auth)
endif ()

idl_all ("${MOD}.fdl" NAME ${MOD}_defs OUTPUT "${INCLUDE_OUTPUT}/modules/${MOD}.h" APPEND_IDL "${MOD}_def.c")

add_library (${MOD})
set_module_defaults (${MOD})
target_sources (${MOD} PRIVATE "${MOD}.cpp" "auth.c")

flute_test (http "test_http.fluid")
