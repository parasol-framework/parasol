# Network module
# If you need to do a deep debug of the network log, enable PARASOL_VLOG at project level (it won't work
# at module level).

set (MOD network)
set (INC_MOD_NETWORK TRUE PARENT_SCOPE)

idl_gen ("${MOD}.fdl" NAME "${MOD}_defs" OUTPUT "${INCLUDE_OUTPUT}/modules/${MOD}.h"
   APPEND_IDL "module_def.c"
   FILES "network.cpp"
   ARGS "output-defs=module_def.c" "output-proto=module_def.c")

add_library (${MOD})

set_module_defaults (${MOD} "Net")

target_sources (${MOD} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/network.cpp")

if (DISABLE_SSL)
   message (STATUS "Network module will not be compiled with SSL support (disabled by DISABLE_SSL).")
elseif (WIN32)
   message (STATUS "Using Windows native SSL support.")
   add_definitions ("-DENABLE_SSL")
   target_sources (${MOD} PRIVATE "win32/ssl_wrapper.cpp")
   list (APPEND NETWORK_LIBS secur32 crypt32)
elseif (OPENSSL_FOUND)
   message (STATUS "Network module will be compiled with OpenSSL support.")
   # Linking against the .a files is necessary to ensure a static link.
   list (APPEND NETWORK_LIBS ssl.a crypto.a)
   target_include_directories (${MOD} PRIVATE "${OPENSSL_INCLUDE_DIR}")
else ()
   message (STATUS "Network module will not be compiled with SSL support (OpenSSL not found).")
endif ()

if (WIN32)
   add_compile_definitions ("_WINSOCK_DEPRECATED_NO_WARNINGS")
   target_sources (${MOD} PRIVATE "win32/winsockwrappers.cpp")
   list (APPEND NETWORK_LIBS wsock32 gdi32 ws2_32)
endif ()

target_link_libraries (${MOD} PRIVATE ${NETWORK_LIBS})

add_compile_definitions ("MAX_ALIASES=64" "MAX_ADDRESSES=32" "HAVE_CONFIG_H=1")

flute_test (network_dns "${CMAKE_CURRENT_SOURCE_DIR}/tests/dns-parallel.fluid")
flute_test (network_server_io "${CMAKE_CURRENT_SOURCE_DIR}/tests/server-io.fluid")
flute_test (network_client_server "${CMAKE_CURRENT_SOURCE_DIR}/tests/client-server.fluid")
flute_test (network_error_handling "${CMAKE_CURRENT_SOURCE_DIR}/tests/test-error-handling.fluid")
