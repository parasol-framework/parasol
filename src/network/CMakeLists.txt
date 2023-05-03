# Network module

set (MOD network)

idl_all ("${MOD}.fdl" NAME "${MOD}_defs" OUTPUT "${INCLUDE_OUTPUT}/modules/${MOD}.h"
   APPEND_IDL "module_def.c"
   FILES "network.cpp"
   ARGS "--output-defs=module_def.c" "--output-proto=module_def.c" "--prototypes=static")

add_library (${MOD})

set_module_defaults (${MOD})

target_sources (${MOD} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/network.cpp")

if (OPENSSL_FOUND)
   message (STATUS "Network module will be compiled with SSL support.")
   # Linking against the .a files is necessary to ensure a static link.
   list (APPEND NETWORK_LIBS ssl.a crypto.a)
   target_include_directories (${MOD} PRIVATE "${OPENSSL_INCLUDE_DIR}")
   add_definitions ("-DENABLE_SSL")
else ()
   message (STATUS "Network module will not be compiled with SSL support.")
endif ()

if (WIN32)
   target_sources (${MOD} PRIVATE "win32/winsockwrappers.cpp")
   list (APPEND NETWORK_LIBS wsock32 gdi32 ws2_32)
endif ()

target_link_libraries (${MOD} PRIVATE ${NETWORK_LIBS})

add_definitions ("-DMAX_ALIASES=64" "-DMAX_ADDRESSES=32" "-DHAVE_CONFIG_H=1")

flute_test (network_dns "${CMAKE_CURRENT_SOURCE_DIR}/tests/dns-parallel.fluid")
flute_test (network_server_io "${CMAKE_CURRENT_SOURCE_DIR}/tests/server-io.fluid")
flute_test (network_client_server "${CMAKE_CURRENT_SOURCE_DIR}/tests/client-server.fluid")
