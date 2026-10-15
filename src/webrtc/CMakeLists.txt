# WebRTC
#
# Phase 0 spikes and Phase 1 protocol foundations from docs/plans/webrtc_native_support.md:
#
#   test_webrtc_stun   STUN codec and RFC 5769 vectors (unit test, label `webrtc`)
#   test_webrtc_sdp    Data-channel SDP codec and Chromium offer/answer fixtures (unit test, label `webrtc`)
#   test_webrtc_ice    ICE-lite connectivity checks and nomination (unit test, label `webrtc`)
#   test_webrtc_dtls   DTLS-SRTP transport over OpenSSL with a datagram BIO (unit test, label `webrtc`)
#   bench_webrtc_udp   NetSocket UDP receive throughput at WebRTC packet sizes (benchmark, label `webrtc_bench`)
#   tests/interop/     Chromium interop harness driven by Playwright (label `webrtc_interop`)
#
# Windows uses the same socket-independent OpenSSL DTLS backend; Network TLS remains on Schannel.
if (NOT WIN32 AND NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
   message (STATUS "WebRTC disabled: this platform is unsupported.")
   return ()
endif ()

if (WIN32 AND NOT DISABLE_SSL)
   add_compile_definitions (WIN32_LEAN_AND_MEAN NOMINMAX)
   find_package (OpenSSL 1.1.1 REQUIRED)
   set (KOTUKU_OPENSSL_LIBS OpenSSL::SSL OpenSSL::Crypto ws2_32 iphlpapi)
endif ()

if (DISABLE_SSL OR NOT KOTUKU_OPENSSL_LIBS)
   message (STATUS "WebRTC spikes disabled: OpenSSL is required for DTLS.")
   return ()
endif ()

find_package (Threads REQUIRED)

# GCC's shared ASan runtime must precede an instrumented interposer in LD_PRELOAD.
set (WEBRTC_PRELOAD_PREFIX "")
if (CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND
    (ENABLE_ANALYSIS OR CMAKE_CXX_FLAGS MATCHES "-fsanitize=[^ ]*address"))
   execute_process (COMMAND "${CMAKE_CXX_COMPILER}" -print-file-name=libasan.so
      OUTPUT_VARIABLE WEBRTC_ASAN_RUNTIME OUTPUT_STRIP_TRAILING_WHITESPACE COMMAND_ERROR_IS_FATAL ANY)
   if (NOT EXISTS "${WEBRTC_ASAN_RUNTIME}")
      message (FATAL_ERROR "Instrumented WebRTC interposers require the GCC ASan runtime.")
   endif ()
   set (WEBRTC_PRELOAD_PREFIX "${WEBRTC_ASAN_RUNTIME}:")
endif ()

# The Windows bootstrap locates Core relative to the executable, rather than its working directory.
# Install native integration probes beside Origo so Core also selects this build's module and package paths.
function (webrtc_native_program Target Output)
   if (WIN32)
      install (TARGETS ${Target} RUNTIME DESTINATION .)
      set (${Output} "${CMAKE_INSTALL_PREFIX}/$<TARGET_FILE_NAME:${Target}>" PARENT_SCOPE)
   else ()
      set (${Output} "$<TARGET_FILE:${Target}>" PARENT_SCOPE)
   endif ()
endfunction ()

include (FetchContent)
set (UTF8PROC_INSTALL OFF CACHE BOOL "" FORCE)
set (WEBRTC_SHARED_LIBS_SAVED ${BUILD_SHARED_LIBS})
set (BUILD_SHARED_LIBS OFF)
FetchContent_Declare (utf8proc
   GIT_REPOSITORY https://github.com/JuliaStrings/utf8proc.git
   GIT_TAG a1b99daa2a3393884220264c927a48ba1251a9c6
   EXCLUDE_FROM_ALL
   SYSTEM)
FetchContent_MakeAvailable (utf8proc)
set (BUILD_SHARED_LIBS ${WEBRTC_SHARED_LIBS_SAVED})
include ("${CMAKE_CURRENT_SOURCE_DIR}/cmake/patch_utf8proc.cmake")
patch_utf8proc ("${utf8proc_SOURCE_DIR}/utf8proc.c")
add_library (webrtc_precis STATIC opaque_string.cpp)
set_target_properties (webrtc_precis PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON POSITION_INDEPENDENT_CODE ON)
target_link_libraries (webrtc_precis PUBLIC utf8proc)

add_library (crypto_protocol STATIC ../crypto/protocol_primitives.cpp)
set_target_properties (crypto_protocol PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON POSITION_INDEPENDENT_CODE ON)
target_compile_definitions (crypto_protocol PUBLIC CRYPTO_PROTOCOL_OPENSSL=1)
target_include_directories (crypto_protocol PRIVATE "${OPENSSL_INCLUDE_DIR}")
target_link_libraries (crypto_protocol PUBLIC ${KOTUKU_OPENSSL_LIBS})
set (WEBRTC_OPENSSL_LIBS ${KOTUKU_OPENSSL_LIBS})
list (PREPEND KOTUKU_OPENSSL_LIBS crypto_protocol webrtc_precis)

set (MOD webrtc)
set (INC_MOD_WEBRTC TRUE PARENT_SCOPE)
idl_gen ("${MOD}.tdl" NAME ${MOD}_defs OUTPUT "${INCLUDE_OUTPUT}/modules/${MOD}.h"
   APPEND_IDL "class_peer_connection_def.c" BYPRODUCTS "class_web_channel_def.c")
add_library (${MOD})
set_module_defaults (${MOD} "WebRTC")
target_sources (${MOD} PRIVATE webrtc.cpp peer_connection.cpp ice_socket.cpp turn_socket.cpp turn.cpp ice_servers.cpp
   ice_dtls.cpp ice_agent.cpp
   ice_candidates.cpp ice.cpp stun.cpp sdp.cpp dtls_openssl.cpp sctp_packet.cpp sctp_association.cpp sctp_transfer.cpp
   sctp_streams.cpp dcep.cpp)
target_include_directories (${MOD} PRIVATE "${OPENSSL_INCLUDE_DIR}")
target_link_libraries (${MOD} PRIVATE ${KOTUKU_OPENSSL_LIBS} Threads::Threads)
add_dependencies (${MOD} network)
flute_test (webrtc_turn_configuration "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_turn_configuration.tiri")
flute_test (webrtc_peer_loopback "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_peer_loopback.tiri")

flute_test (webrtc_webchannel_loopback "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_webchannel_loopback.tiri")
flute_test (webrtc_webchannel_lifecycle "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_webchannel_lifecycle.tiri")
set_tests_properties (webrtc_webchannel_loopback PROPERTIES TIMEOUT 150)

if (UNIT_TESTS)
   foreach (SCTP_TEST packet association transfer streams)
      add_executable (test_webrtc_sctp_${SCTP_TEST} tests/test_sctp_${SCTP_TEST}.cpp sctp_packet.cpp)
      set_target_properties (test_webrtc_sctp_${SCTP_TEST} PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
      target_compile_definitions (test_webrtc_sctp_${SCTP_TEST} PRIVATE
         WEBRTC_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures")
      add_test (NAME webrtc_sctp_${SCTP_TEST} COMMAND test_webrtc_sctp_${SCTP_TEST})
      set_tests_properties (webrtc_sctp_${SCTP_TEST} PROPERTIES LABELS webrtc TIMEOUT 60)
   endforeach ()
   add_executable (test_webrtc_sctp_dtls tests/test_sctp_dtls.cpp sctp_packet.cpp sctp_association.cpp
      sctp_transfer.cpp sctp_streams.cpp dcep.cpp dtls_openssl.cpp)
   set_target_properties (test_webrtc_sctp_dtls PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
   target_link_libraries (test_webrtc_sctp_dtls PRIVATE ${KOTUKU_OPENSSL_LIBS} ${CMAKE_DL_LIBS} Threads::Threads)
   add_test (NAME webrtc_sctp_dtls COMMAND test_webrtc_sctp_dtls)
   set_tests_properties (webrtc_sctp_dtls PROPERTIES LABELS webrtc TIMEOUT 60)
   target_sources (test_webrtc_sctp_association PRIVATE sctp_association.cpp sctp_transfer.cpp
      sctp_streams.cpp dcep.cpp)
   target_link_libraries (test_webrtc_sctp_association PRIVATE crypto_protocol)
   target_sources (test_webrtc_sctp_transfer PRIVATE sctp_association.cpp sctp_transfer.cpp sctp_streams.cpp dcep.cpp)
   target_link_libraries (test_webrtc_sctp_transfer PRIVATE crypto_protocol)

   target_sources (test_webrtc_sctp_streams PRIVATE sctp_association.cpp sctp_transfer.cpp sctp_streams.cpp dcep.cpp)
   target_link_libraries (test_webrtc_sctp_streams PRIVATE crypto_protocol)

   add_executable (test_webrtc_opaque_string tests/test_opaque_string.cpp)
   set_target_properties (test_webrtc_opaque_string PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
   target_link_libraries (test_webrtc_opaque_string PRIVATE webrtc_precis)
   add_test (NAME webrtc_opaque_string COMMAND test_webrtc_opaque_string)
   set_tests_properties (webrtc_opaque_string PROPERTIES LABELS webrtc TIMEOUT 60)

   add_executable (test_webrtc_turn tests/test_turn.cpp turn.cpp stun.cpp)
   set_target_properties (test_webrtc_turn PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
   target_include_directories (test_webrtc_turn PRIVATE "${OPENSSL_INCLUDE_DIR}")
   target_link_libraries (test_webrtc_turn PRIVATE ${KOTUKU_OPENSSL_LIBS})
   target_compile_definitions (test_webrtc_turn PRIVATE
      WEBRTC_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures")
   add_test (NAME webrtc_turn COMMAND test_webrtc_turn)
   set_tests_properties (webrtc_turn PROPERTIES LABELS webrtc TIMEOUT 60)

   add_executable (test_webrtc_stun tests/test_stun.cpp stun.cpp)
   set_target_properties (test_webrtc_stun PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
   target_include_directories (test_webrtc_stun PRIVATE "${OPENSSL_INCLUDE_DIR}")
   target_link_libraries (test_webrtc_stun PRIVATE ${KOTUKU_OPENSSL_LIBS})
   add_test (NAME webrtc_stun COMMAND test_webrtc_stun)
   set_tests_properties (webrtc_stun PROPERTIES LABELS webrtc TIMEOUT 60)

   add_executable (test_webrtc_stun_long_term tests/test_stun_long_term.cpp stun.cpp)
   set_target_properties (test_webrtc_stun_long_term PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
   target_include_directories (test_webrtc_stun_long_term PRIVATE "${OPENSSL_INCLUDE_DIR}")
   target_link_libraries (test_webrtc_stun_long_term PRIVATE ${KOTUKU_OPENSSL_LIBS})
   add_test (NAME webrtc_stun_long_term COMMAND test_webrtc_stun_long_term)
   set_tests_properties (webrtc_stun_long_term PROPERTIES LABELS webrtc TIMEOUT 60)

   add_executable (test_webrtc_sdp tests/test_sdp.cpp sdp.cpp)
   set_target_properties (test_webrtc_sdp PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
   target_compile_definitions (test_webrtc_sdp PRIVATE
      WEBRTC_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures")
   add_test (NAME webrtc_sdp COMMAND test_webrtc_sdp)
   set_tests_properties (webrtc_sdp PROPERTIES LABELS webrtc TIMEOUT 60)

   add_executable (test_webrtc_ice tests/test_ice.cpp ice.cpp stun.cpp)
   set_target_properties (test_webrtc_ice PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
   target_include_directories (test_webrtc_ice PRIVATE "${OPENSSL_INCLUDE_DIR}")
   target_link_libraries (test_webrtc_ice PRIVATE ${KOTUKU_OPENSSL_LIBS})
   add_test (NAME webrtc_ice COMMAND test_webrtc_ice)
   set_tests_properties (webrtc_ice PROPERTIES LABELS webrtc TIMEOUT 60)

   add_executable (test_webrtc_ice_agent tests/test_ice_agent.cpp ice_agent.cpp ice.cpp stun.cpp)
   set_target_properties (test_webrtc_ice_agent PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
   target_include_directories (test_webrtc_ice_agent PRIVATE "${OPENSSL_INCLUDE_DIR}")
   target_link_libraries (test_webrtc_ice_agent PRIVATE ${KOTUKU_OPENSSL_LIBS})
   add_test (NAME webrtc_ice_agent COMMAND test_webrtc_ice_agent)
   set_tests_properties (webrtc_ice_agent PROPERTIES LABELS webrtc TIMEOUT 60)

   add_executable (test_webrtc_ice_candidates tests/test_ice_candidates.cpp ice_candidates.cpp stun.cpp)
   set_target_properties (test_webrtc_ice_candidates PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
   target_include_directories (test_webrtc_ice_candidates PRIVATE "${OPENSSL_INCLUDE_DIR}")
   target_link_libraries (test_webrtc_ice_candidates PRIVATE ${KOTUKU_OPENSSL_LIBS})
   add_test (NAME webrtc_ice_candidates COMMAND test_webrtc_ice_candidates)
   set_tests_properties (webrtc_ice_candidates PROPERTIES LABELS webrtc TIMEOUT 60)

   add_executable (test_webrtc_ice_dtls tests/test_ice_dtls.cpp ice_dtls.cpp ice_agent.cpp ice.cpp stun.cpp
      dtls_openssl.cpp)
   set_target_properties (test_webrtc_ice_dtls PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
   target_include_directories (test_webrtc_ice_dtls PRIVATE "${OPENSSL_INCLUDE_DIR}")
   target_link_libraries (test_webrtc_ice_dtls PRIVATE ${KOTUKU_OPENSSL_LIBS} ${CMAKE_DL_LIBS} Threads::Threads)
   add_test (NAME webrtc_ice_dtls COMMAND test_webrtc_ice_dtls)
   set_tests_properties (webrtc_ice_dtls PROPERTIES LABELS webrtc TIMEOUT 60)

   add_executable (test_webrtc_full_ice_dtls tests/test_full_ice_dtls.cpp ice_dtls.cpp ice_agent.cpp ice.cpp
      stun.cpp dtls_openssl.cpp)
   set_target_properties (test_webrtc_full_ice_dtls PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
   target_include_directories (test_webrtc_full_ice_dtls PRIVATE "${OPENSSL_INCLUDE_DIR}")
   target_link_libraries (test_webrtc_full_ice_dtls PRIVATE ${KOTUKU_OPENSSL_LIBS} ${CMAKE_DL_LIBS} Threads::Threads)
   add_test (NAME webrtc_full_ice_dtls COMMAND test_webrtc_full_ice_dtls)
   set_tests_properties (webrtc_full_ice_dtls PROPERTIES LABELS webrtc TIMEOUT 60)

   add_executable (test_webrtc_ice_socket tests/test_ice_socket.cpp ice_socket.cpp turn_socket.cpp turn.cpp
      ice_servers.cpp ice_dtls.cpp ice_agent.cpp
      ice_candidates.cpp ice.cpp stun.cpp dtls_openssl.cpp)
   set_target_properties (test_webrtc_ice_socket PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
   target_include_directories (test_webrtc_ice_socket PRIVATE "${OPENSSL_INCLUDE_DIR}")
   target_link_libraries (test_webrtc_ice_socket PRIVATE ${INIT_LINK} ${KOTUKU_OPENSSL_LIBS} Threads::Threads)
   add_dependencies (test_webrtc_ice_socket network)
   webrtc_native_program (test_webrtc_ice_socket ICE_SOCKET_PROGRAM)
   add_test (NAME webrtc_ice_socket COMMAND "${ICE_SOCKET_PROGRAM}" WORKING_DIRECTORY "${CMAKE_INSTALL_PREFIX}")
   set_tests_properties (webrtc_ice_socket PROPERTIES LABELS webrtc TIMEOUT 60)

   add_executable (test_webrtc_full_ice_socket tests/test_full_ice_socket.cpp ice_socket.cpp turn_socket.cpp turn.cpp
      ice_servers.cpp ice_dtls.cpp
      ice_agent.cpp ice_candidates.cpp ice.cpp stun.cpp dtls_openssl.cpp)
   set_target_properties (test_webrtc_full_ice_socket PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
   target_include_directories (test_webrtc_full_ice_socket PRIVATE "${OPENSSL_INCLUDE_DIR}")
   target_link_libraries (test_webrtc_full_ice_socket PRIVATE ${INIT_LINK} ${KOTUKU_OPENSSL_LIBS} Threads::Threads)
   add_dependencies (test_webrtc_full_ice_socket network)
   webrtc_native_program (test_webrtc_full_ice_socket FULL_ICE_SOCKET_PROGRAM)
   add_test (NAME webrtc_full_ice_socket COMMAND "${FULL_ICE_SOCKET_PROGRAM}"
      WORKING_DIRECTORY "${CMAKE_INSTALL_PREFIX}")
   set_tests_properties (webrtc_full_ice_socket PROPERTIES LABELS webrtc TIMEOUT 60)

   add_executable (test_webrtc_public_turn tests/test_public_turn.cpp)
   set_target_properties (test_webrtc_public_turn PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
   target_link_libraries (test_webrtc_public_turn PRIVATE ${INIT_LINK})
   add_dependencies (test_webrtc_public_turn webrtc)
   webrtc_native_program (test_webrtc_public_turn PUBLIC_TURN_PROGRAM)

   add_executable (test_webrtc_public_webchannel tests/test_public_webchannel.cpp)
   set_target_properties (test_webrtc_public_webchannel PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
   target_link_libraries (test_webrtc_public_webchannel PRIVATE ${INIT_LINK})
   add_dependencies (test_webrtc_public_webchannel webrtc)
   webrtc_native_program (test_webrtc_public_webchannel PUBLIC_WEBCHANNEL_PROGRAM)
   add_test (NAME webrtc_public_webchannel COMMAND "${PUBLIC_WEBCHANNEL_PROGRAM}" --log-warning
      WORKING_DIRECTORY "${CMAKE_INSTALL_PREFIX}")
   set_tests_properties (webrtc_public_webchannel PROPERTIES LABELS webrtc TIMEOUT 200)

   add_executable (test_webrtc_peer_connection tests/test_peer_connection.cpp peer_connection.cpp sctp_packet.cpp
sctp_association.cpp sctp_transfer.cpp sctp_streams.cpp dcep.cpp ice_socket.cpp turn_socket.cpp turn.cpp
      ice_servers.cpp ice_dtls.cpp ice_agent.cpp
      ice_candidates.cpp ice.cpp stun.cpp sdp.cpp dtls_openssl.cpp)
   set_target_properties (test_webrtc_peer_connection PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
   target_include_directories (test_webrtc_peer_connection PRIVATE "${OPENSSL_INCLUDE_DIR}")
   target_link_libraries (test_webrtc_peer_connection PRIVATE ${INIT_LINK} ${KOTUKU_OPENSSL_LIBS} Threads::Threads)
   add_dependencies (test_webrtc_peer_connection network)
   webrtc_native_program (test_webrtc_peer_connection PEER_CONNECTION_PROGRAM)
   add_test (NAME webrtc_peer_connection COMMAND "${PEER_CONNECTION_PROGRAM}"
      WORKING_DIRECTORY "${CMAKE_INSTALL_PREFIX}")
   set_tests_properties (webrtc_peer_connection PROPERTIES LABELS webrtc TIMEOUT 60)

   option (WEBRTC_FUZZ "Build the WebRTC protocol parser libFuzzer targets" OFF)
   if (WEBRTC_FUZZ)
      if (NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR NOT CMAKE_C_COMPILER_ID MATCHES "Clang")
         message (FATAL_ERROR "WEBRTC_FUZZ requires Clang C and C++ compilers with libFuzzer support.")
      endif ()
      foreach (TURN_FUZZ_KIND codec state)
         add_executable (fuzz_webrtc_turn_${TURN_FUZZ_KIND} tests/fuzz_turn_${TURN_FUZZ_KIND}.cpp turn.cpp
            stun.cpp opaque_string.cpp ../crypto/protocol_primitives.cpp "${utf8proc_SOURCE_DIR}/utf8proc.c")
         set_target_properties (fuzz_webrtc_turn_${TURN_FUZZ_KIND} PROPERTIES
            CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
         target_include_directories (fuzz_webrtc_turn_${TURN_FUZZ_KIND} PRIVATE
            "${OPENSSL_INCLUDE_DIR}" "${utf8proc_SOURCE_DIR}")
         target_compile_definitions (fuzz_webrtc_turn_${TURN_FUZZ_KIND} PRIVATE CRYPTO_PROTOCOL_OPENSSL=1 UTF8PROC_STATIC)
         target_link_libraries (fuzz_webrtc_turn_${TURN_FUZZ_KIND} PRIVATE ${WEBRTC_OPENSSL_LIBS})
         target_compile_options (fuzz_webrtc_turn_${TURN_FUZZ_KIND} PRIVATE
            -fsanitize=fuzzer,address,undefined -fno-sanitize-recover=all)
         target_link_options (fuzz_webrtc_turn_${TURN_FUZZ_KIND} PRIVATE
            -fsanitize=fuzzer,address,undefined -fno-sanitize-recover=all)
         add_test (NAME webrtc_turn_${TURN_FUZZ_KIND}_fuzz_regression COMMAND fuzz_webrtc_turn_${TURN_FUZZ_KIND}
            -runs=1 "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/fuzz_turn_${TURN_FUZZ_KIND}")
         set_tests_properties (webrtc_turn_${TURN_FUZZ_KIND}_fuzz_regression PROPERTIES LABELS webrtc TIMEOUT 60)
      endforeach ()
      foreach (FUZZ_KIND codec state)
         add_executable (fuzz_webrtc_sctp_${FUZZ_KIND} tests/fuzz_sctp_${FUZZ_KIND}.cpp sctp_packet.cpp
            sctp_association.cpp sctp_transfer.cpp sctp_streams.cpp dcep.cpp ../crypto/protocol_primitives.cpp)
         set_target_properties (fuzz_webrtc_sctp_${FUZZ_KIND} PROPERTIES
            CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
         target_include_directories (fuzz_webrtc_sctp_${FUZZ_KIND} PRIVATE "${OPENSSL_INCLUDE_DIR}")
         target_compile_definitions (fuzz_webrtc_sctp_${FUZZ_KIND} PRIVATE CRYPTO_PROTOCOL_OPENSSL=1)
         target_link_libraries (fuzz_webrtc_sctp_${FUZZ_KIND} PRIVATE ${WEBRTC_OPENSSL_LIBS})
         target_compile_options (fuzz_webrtc_sctp_${FUZZ_KIND} PRIVATE
            -fsanitize=fuzzer,address,undefined -fno-sanitize-recover=all)
         target_link_options (fuzz_webrtc_sctp_${FUZZ_KIND} PRIVATE
            -fsanitize=fuzzer,address,undefined -fno-sanitize-recover=all)
         add_test (NAME webrtc_sctp_${FUZZ_KIND}_fuzz_regression COMMAND fuzz_webrtc_sctp_${FUZZ_KIND}
            -runs=1 "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/fuzz_sctp_${FUZZ_KIND}")
         set_tests_properties (webrtc_sctp_${FUZZ_KIND}_fuzz_regression PROPERTIES LABELS webrtc TIMEOUT 60)
      endforeach ()
      add_executable (fuzz_webrtc_sdp tests/fuzz_sdp.cpp sdp.cpp)
      set_target_properties (fuzz_webrtc_sdp PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
      target_compile_options (fuzz_webrtc_sdp PRIVATE -fsanitize=fuzzer,address,undefined -fno-sanitize-recover=all)
      target_link_options (fuzz_webrtc_sdp PRIVATE -fsanitize=fuzzer,address,undefined -fno-sanitize-recover=all)
      # Compile the dependency sources into the fuzzer so sanitiser coverage includes credential preparation.
      add_executable (fuzz_webrtc_stun tests/fuzz_stun.cpp stun.cpp opaque_string.cpp
         ../crypto/protocol_primitives.cpp "${utf8proc_SOURCE_DIR}/utf8proc.c")
      set_target_properties (fuzz_webrtc_stun PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
      target_include_directories (fuzz_webrtc_stun PRIVATE "${OPENSSL_INCLUDE_DIR}" "${utf8proc_SOURCE_DIR}")
      target_compile_definitions (fuzz_webrtc_stun PRIVATE CRYPTO_PROTOCOL_OPENSSL=1 UTF8PROC_STATIC)
      target_link_libraries (fuzz_webrtc_stun PRIVATE ${WEBRTC_OPENSSL_LIBS})
      target_compile_options (fuzz_webrtc_stun PRIVATE -fsanitize=fuzzer,address,undefined -fno-sanitize-recover=all)
      target_link_options (fuzz_webrtc_stun PRIVATE -fsanitize=fuzzer,address,undefined -fno-sanitize-recover=all)
      add_test (NAME webrtc_stun_fuzz_regression COMMAND fuzz_webrtc_stun -runs=1
         "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/fuzz_stun/utf8proc_null_buffer.stun")
      set_tests_properties (webrtc_stun_fuzz_regression PROPERTIES LABELS webrtc TIMEOUT 60)
   endif ()

   add_executable (test_webrtc_dtls tests/test_dtls.cpp dtls_openssl.cpp)
   set_target_properties (test_webrtc_dtls PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
   target_include_directories (test_webrtc_dtls PRIVATE "${OPENSSL_INCLUDE_DIR}")
   target_link_libraries (test_webrtc_dtls PRIVATE ${KOTUKU_OPENSSL_LIBS} ${CMAKE_DL_LIBS} Threads::Threads)
   add_test (NAME webrtc_dtls COMMAND test_webrtc_dtls)
   set_tests_properties (webrtc_dtls PROPERTIES LABELS webrtc TIMEOUT 60)
endif ()

# The benchmark boots the Core, so ctest runs it from the install folder.  It is not a pass/fail gate for normal CI
# runs and carries its own label: ctest -L webrtc_bench

if (BUILD_TESTS AND TARGET network)
   add_executable (bench_webrtc_channels tests/bench_channels.cpp sctp_packet.cpp sctp_association.cpp
      sctp_transfer.cpp sctp_streams.cpp dcep.cpp)
   set_target_properties (bench_webrtc_channels PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
   target_link_libraries (bench_webrtc_channels PRIVATE ${INIT_LINK} crypto_protocol)
   add_dependencies (bench_webrtc_channels webrtc)
   webrtc_native_program (bench_webrtc_channels CHANNEL_BENCH_PROGRAM)
   add_test (NAME webrtc_channels_bench COMMAND "${CHANNEL_BENCH_PROGRAM}" --log-warning
      WORKING_DIRECTORY "${CMAKE_INSTALL_PREFIX}")
   set_tests_properties (webrtc_channels_bench PROPERTIES LABELS webrtc_bench TIMEOUT 240 RUN_SERIAL TRUE)

   add_executable (bench_webrtc_udp tests/bench_udp.cpp)
   set_target_properties (bench_webrtc_udp PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
   target_link_libraries (bench_webrtc_udp PRIVATE ${INIT_LINK} Threads::Threads)
   if (WIN32)
      target_link_libraries (bench_webrtc_udp PRIVATE ws2_32)
   endif ()
   add_dependencies (bench_webrtc_udp network)
   webrtc_native_program (bench_webrtc_udp UDP_BENCH_PROGRAM)
   add_test (NAME webrtc_udp_bench COMMAND "${UDP_BENCH_PROGRAM}" WORKING_DIRECTORY "${CMAKE_INSTALL_PREFIX}")
   set_tests_properties (webrtc_udp_bench PROPERTIES LABELS webrtc_bench TIMEOUT 120 RUN_SERIAL TRUE)
endif ()

if (BUILD_TESTS AND TARGET network)
   add_executable (webrtc_native_probe tests/native_probe.cpp ice_socket.cpp turn_socket.cpp turn.cpp ice_servers.cpp
      ice_dtls.cpp ice_agent.cpp
      ice_candidates.cpp ice.cpp stun.cpp
      sdp.cpp dtls_openssl.cpp)
   set_target_properties (webrtc_native_probe PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
   target_include_directories (webrtc_native_probe PRIVATE "${OPENSSL_INCLUDE_DIR}")
   target_link_libraries (webrtc_native_probe PRIVATE ${INIT_LINK} ${KOTUKU_OPENSSL_LIBS} Threads::Threads)
   add_dependencies (webrtc_native_probe network)
   webrtc_native_program (webrtc_native_probe NATIVE_PROBE_PROGRAM)

   add_executable (webrtc_native_full_probe tests/native_full_probe.cpp ice_socket.cpp turn_socket.cpp turn.cpp
      ice_servers.cpp ice_dtls.cpp ice_agent.cpp
      ice_candidates.cpp ice.cpp stun.cpp sdp.cpp dtls_openssl.cpp)
   set_target_properties (webrtc_native_full_probe PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
   target_include_directories (webrtc_native_full_probe PRIVATE "${OPENSSL_INCLUDE_DIR}")
   target_link_libraries (webrtc_native_full_probe PRIVATE ${INIT_LINK} ${KOTUKU_OPENSSL_LIBS} Threads::Threads)
   add_dependencies (webrtc_native_full_probe network)
   webrtc_native_program (webrtc_native_full_probe FULL_PROBE_PROGRAM)

   add_executable (webrtc_native_sctp_probe tests/native_sctp_probe.cpp ice_socket.cpp turn_socket.cpp turn.cpp
      ice_servers.cpp ice_dtls.cpp ice_agent.cpp
      ice_candidates.cpp ice.cpp stun.cpp sdp.cpp dtls_openssl.cpp sctp_packet.cpp sctp_association.cpp
      sctp_transfer.cpp sctp_streams.cpp dcep.cpp)
   set_target_properties (webrtc_native_sctp_probe PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
   target_link_libraries (webrtc_native_sctp_probe PRIVATE ${INIT_LINK} ${KOTUKU_OPENSSL_LIBS} Threads::Threads)
   add_dependencies (webrtc_native_sctp_probe network)
   webrtc_native_program (webrtc_native_sctp_probe SCTP_PROBE_PROGRAM)

   add_executable (webrtc_public_peer_probe tests/public_peer_probe.cpp)
   set_target_properties (webrtc_public_peer_probe PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
   target_link_libraries (webrtc_public_peer_probe PRIVATE ${INIT_LINK})
   add_dependencies (webrtc_public_peer_probe webrtc)
   webrtc_native_program (webrtc_public_peer_probe PUBLIC_PROBE_PROGRAM)
endif ()

# Browser interop harness.  Registered only when Node.js and the playwright package are available, because Chromium is
# not part of the standard build dependencies.  The package is located through `npm root -g` unless
# WEBRTC_INTEROP_NODE_PATH names another node_modules folder.  Run with: ctest -L webrtc_interop

option (WEBRTC_REQUIRE_INTEROP "Require all WebRTC browser interop tests at configure time" OFF)
if (WEBRTC_REQUIRE_INTEROP AND NOT BUILD_TESTS)
   message (FATAL_ERROR "WEBRTC_REQUIRE_INTEROP requires BUILD_TESTS.")
endif ()

if (BUILD_TESTS)
   find_program (NODE_EXECUTABLE node)
   find_program (NPM_EXECUTABLE npm)
   set (WEBRTC_INTEROP_NODE_PATH "" CACHE PATH "node_modules folder containing playwright for the WebRTC interop test")

   if (NODE_EXECUTABLE AND NOT WEBRTC_INTEROP_NODE_PATH AND NPM_EXECUTABLE)
      execute_process (COMMAND "${NPM_EXECUTABLE}" root -g OUTPUT_VARIABLE NPM_GLOBAL_ROOT
         OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET RESULT_VARIABLE NPM_ROOT_RESULT)
      if (NPM_ROOT_RESULT EQUAL 0)
         set (WEBRTC_INTEROP_NODE_PATH "${NPM_GLOBAL_ROOT}" CACHE PATH "" FORCE)
      endif ()
   endif ()

   if (NODE_EXECUTABLE AND WEBRTC_INTEROP_NODE_PATH)
      execute_process (
         COMMAND "${CMAKE_COMMAND}" -E env "NODE_PATH=${WEBRTC_INTEROP_NODE_PATH}"
            "${NODE_EXECUTABLE}" -e "require.resolve('playwright')"
         RESULT_VARIABLE PLAYWRIGHT_RESULT OUTPUT_QUIET ERROR_QUIET)
   endif ()

   if (NODE_EXECUTABLE AND WEBRTC_INTEROP_NODE_PATH AND (PLAYWRIGHT_RESULT EQUAL 0))
      find_program (WEBRTC_CHROME_EXECUTABLE NAMES google-chrome chromium chromium-browser)
      if (WEBRTC_CHROME_EXECUTABLE AND TARGET webrtc_native_probe)
         add_test (NAME webrtc_native_dtls
            COMMAND "${NODE_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/interop/native_dtls.cjs"
               "${NATIVE_PROBE_PROGRAM}" "${WEBRTC_CHROME_EXECUTABLE}"
            WORKING_DIRECTORY "${CMAKE_INSTALL_PREFIX}")
         set_tests_properties (webrtc_native_dtls PROPERTIES
            LABELS webrtc_interop TIMEOUT 30 RUN_SERIAL TRUE
            ENVIRONMENT "NODE_PATH=${WEBRTC_INTEROP_NODE_PATH}")
      endif ()
      if (WEBRTC_CHROME_EXECUTABLE AND TARGET webrtc_native_full_probe)
         add_test (NAME webrtc_native_full_dtls
            COMMAND "${NODE_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/interop/native_dtls.cjs"
               "${FULL_PROBE_PROGRAM}" "${WEBRTC_CHROME_EXECUTABLE}"
            WORKING_DIRECTORY "${CMAKE_INSTALL_PREFIX}")
         set_tests_properties (webrtc_native_full_dtls PROPERTIES
            LABELS webrtc_interop TIMEOUT 30 RUN_SERIAL TRUE
            ENVIRONMENT "NODE_PATH=${WEBRTC_INTEROP_NODE_PATH}")
      endif ()
      if (WEBRTC_CHROME_EXECUTABLE AND TARGET webrtc_public_peer_probe)
         add_test (NAME webrtc_public_peer_dtls
            COMMAND "${NODE_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/interop/native_dtls.cjs"
               "${PUBLIC_PROBE_PROGRAM}" "${WEBRTC_CHROME_EXECUTABLE}"
            WORKING_DIRECTORY "${CMAKE_INSTALL_PREFIX}")
         set_tests_properties (webrtc_public_peer_dtls PROPERTIES
            LABELS webrtc_interop TIMEOUT 30 RUN_SERIAL TRUE
            ENVIRONMENT "NODE_PATH=${WEBRTC_INTEROP_NODE_PATH}")
      endif ()
      if (WEBRTC_CHROME_EXECUTABLE AND TARGET webrtc_native_sctp_probe)
         foreach (DTLS_ROLE client server)
            add_test (NAME webrtc_native_sctp_${DTLS_ROLE}
               COMMAND "${NODE_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/interop/native_sctp.cjs"
                  "${SCTP_PROBE_PROGRAM}" "${WEBRTC_CHROME_EXECUTABLE}" ${DTLS_ROLE}
                  "${CMAKE_CURRENT_BINARY_DIR}/interop/sctp_${DTLS_ROLE}.json"
               WORKING_DIRECTORY "${CMAKE_INSTALL_PREFIX}")
            set_tests_properties (webrtc_native_sctp_${DTLS_ROLE} PROPERTIES
               LABELS webrtc_interop TIMEOUT 30 RUN_SERIAL TRUE
               ENVIRONMENT "NODE_PATH=${WEBRTC_INTEROP_NODE_PATH}")
         endforeach ()
      endif ()
      if (RUN_ANYWHERE)
         set (WEBRTC_ORIGO "${CMAKE_INSTALL_PREFIX}/origo${CMAKE_EXECUTABLE_SUFFIX}")
      else ()
         set (WEBRTC_ORIGO "${CMAKE_INSTALL_PREFIX}/bin/origo${CMAKE_EXECUTABLE_SUFFIX}")
      endif ()

      foreach (PUBLIC_MODE interop bench)
         add_test (NAME webrtc_public_channels_${PUBLIC_MODE}
            COMMAND "${NODE_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/interop/public_channels.cjs"
               "${WEBRTC_ORIGO}" "${PROJECT_SOURCE_DIR}/examples/webrtc/signalling.tiri"
               "${WEBRTC_CHROME_EXECUTABLE}"
               "${CMAKE_CURRENT_BINARY_DIR}/interop/public_channels_${PUBLIC_MODE}.json" ${PUBLIC_MODE}
            WORKING_DIRECTORY "${CMAKE_INSTALL_PREFIX}")
         if (PUBLIC_MODE STREQUAL "bench")
            set (PUBLIC_LABEL webrtc_bench)
         else ()
            set (PUBLIC_LABEL webrtc_interop)
         endif ()
         set_tests_properties (webrtc_public_channels_${PUBLIC_MODE} PROPERTIES
            LABELS ${PUBLIC_LABEL} TIMEOUT 180 RUN_SERIAL TRUE
            ENVIRONMENT "NODE_PATH=${WEBRTC_INTEROP_NODE_PATH}")
      endforeach ()

      add_test (NAME webrtc_interop
         COMMAND "${NODE_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/interop/run_interop.cjs"
            --origo "${WEBRTC_ORIGO}"
            --script "${CMAKE_CURRENT_SOURCE_DIR}/tests/interop/signalling_server.tiri"
            --port 9310
            --chrome "${WEBRTC_CHROME_EXECUTABLE}"
            --out "${CMAKE_CURRENT_BINARY_DIR}/interop")
      set_tests_properties (webrtc_interop PROPERTIES
         LABELS webrtc_interop
         TIMEOUT 120
         RUN_SERIAL TRUE
         ENVIRONMENT "NODE_PATH=${WEBRTC_INTEROP_NODE_PATH}")
      message (STATUS "WebRTC browser interop test enabled (playwright found in ${WEBRTC_INTEROP_NODE_PATH}).")
   else ()
      message (STATUS "WebRTC browser interop test disabled: Node.js with the playwright package was not found.")
   endif ()
endif ()

if (WEBRTC_REQUIRE_INTEROP)
   foreach (TEST_NAME webrtc_native_dtls webrtc_native_full_dtls webrtc_public_peer_dtls webrtc_interop
      webrtc_native_sctp_client webrtc_native_sctp_server webrtc_public_channels_interop)
      if (NOT TEST ${TEST_NAME})
         message (FATAL_ERROR "Required test ${TEST_NAME} is unavailable; configure Node.js, Playwright "
            "and WEBRTC_CHROME_EXECUTABLE.")
      endif ()
   endforeach ()
   if (NOT EXISTS "${WEBRTC_CHROME_EXECUTABLE}")
      message (FATAL_ERROR "WEBRTC_CHROME_EXECUTABLE must name an installed browser.")
   endif ()
endif ()

# Explicit local coturn fixture; CI can require these prerequisites rather than accepting a skipped relay gate.
option (WEBRTC_REQUIRE_TURN "Require the production TURN integration fixture" OFF)
set (WEBRTC_TURN_SERVER "" CACHE FILEPATH "Pinned coturn turnserver executable")
set (WEBRTC_TURN_UCLIENT "" CACHE FILEPATH "Pinned coturn turnutils_uclient executable")
if (WIN32)
   set (WEBRTC_TURN_RELAY_MIN 40000 CACHE STRING "First TURN fixture relay port")
   set (WEBRTC_TURN_RELAY_MAX 40019 CACHE STRING "Last TURN fixture relay port")
   option (WEBRTC_WINDOWS_TRUST_FIXTURE_CA "Temporarily trust the TURN fixture CA for Schannel tests" OFF)
   set (WEBRTC_WINDOWS_TRUST_SCOPE user CACHE STRING "TURN fixture root store: user or elevated machine")
   set_property (CACHE WEBRTC_WINDOWS_TRUST_SCOPE PROPERTY STRINGS user machine)
else ()
   set (WEBRTC_TURN_RELAY_MIN 49160 CACHE STRING "First TURN fixture relay port")
   set (WEBRTC_TURN_RELAY_MAX 49179 CACHE STRING "Last TURN fixture relay port")
endif ()
if (BUILD_TESTS AND TARGET test_webrtc_public_turn AND WEBRTC_TURN_SERVER AND WEBRTC_TURN_UCLIENT)
   find_package (Python3 COMPONENTS Interpreter REQUIRED)
   if (RUN_ANYWHERE)
      set (TURN_ORIGO "${CMAKE_INSTALL_PREFIX}/origo${CMAKE_EXECUTABLE_SUFFIX}")
   else ()
      set (TURN_ORIGO "${CMAKE_INSTALL_PREFIX}/bin/origo${CMAKE_EXECUTABLE_SUFFIX}")
   endif ()
   set (TURN_FIXTURE_ARGS
      --turnserver "${WEBRTC_TURN_SERVER}" --uclient "${WEBRTC_TURN_UCLIENT}"
      --origo "${TURN_ORIGO}" --public-turn "${PUBLIC_TURN_PROGRAM}"
      --turn-adapter "${PEER_CONNECTION_PROGRAM}"
      --relay-min "${WEBRTC_TURN_RELAY_MIN}" --relay-max "${WEBRTC_TURN_RELAY_MAX}"
      --evidence-dir "${CMAKE_CURRENT_BINARY_DIR}/interop/turn")
   find_program (OPENSSL_EXECUTABLE NAMES openssl REQUIRED)
   list (APPEND TURN_FIXTURE_ARGS --openssl "${OPENSSL_EXECUTABLE}")
   if (WIN32 AND WEBRTC_WINDOWS_TRUST_FIXTURE_CA)
      list (APPEND TURN_FIXTURE_ARGS --windows-trust-ca --windows-trust-scope "${WEBRTC_WINDOWS_TRUST_SCOPE}")
   endif ()
   if (WEBRTC_CHROME_EXECUTABLE AND NODE_EXECUTABLE AND (PLAYWRIGHT_RESULT EQUAL 0))
      list (APPEND TURN_FIXTURE_ARGS --browser-turn "${WEBRTC_CHROME_EXECUTABLE}")
   endif ()
   add_test (NAME webrtc_turn_integration COMMAND "${Python3_EXECUTABLE}"
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/interop/run_phase3_fixture.py" ${TURN_FIXTURE_ARGS})
   set_tests_properties (webrtc_turn_integration PROPERTIES LABELS webrtc_turn_integration
      TIMEOUT 480 RUN_SERIAL TRUE ENVIRONMENT "NODE_PATH=${WEBRTC_INTEROP_NODE_PATH}")
endif ()
if (WEBRTC_REQUIRE_TURN)
   if (NOT TEST webrtc_turn_integration OR NOT EXISTS "${WEBRTC_TURN_SERVER}" OR
       NOT EXISTS "${WEBRTC_TURN_UCLIENT}")
      message (FATAL_ERROR "Required TURN fixture needs BUILD_TESTS, UNIT_TESTS and pinned coturn executables.")
   endif ()
   if (NOT WEBRTC_CHROME_EXECUTABLE OR NOT NODE_EXECUTABLE OR NOT (PLAYWRIGHT_RESULT EQUAL 0))
      message (FATAL_ERROR "Required TURN fixture needs Chromium, Node.js and Playwright.")
   endif ()
endif ()

# Opt-in kernel fixtures require disposable Linux user/network namespaces, nftables and netem.
option (WEBRTC_TEST_RELAY_NAMESPACE "Enable isolated blocked-direct and impaired relay fixtures" OFF)
if (WEBRTC_TEST_RELAY_NAMESPACE)
   if (NOT CMAKE_SYSTEM_NAME STREQUAL "Linux" OR NOT TEST webrtc_turn_integration OR
       NOT TARGET bench_webrtc_channels OR NOT WEBRTC_CHROME_EXECUTABLE OR NOT NODE_EXECUTABLE OR
       NOT (PLAYWRIGHT_RESULT EQUAL 0))
      message (FATAL_ERROR "Relay namespace fixtures require Linux, coturn, Chromium, Node.js and Playwright.")
   endif ()
   find_program (WEBRTC_NFT_EXECUTABLE NAMES nft PATHS /usr/sbin /sbin REQUIRED)
   find_program (WEBRTC_TC_EXECUTABLE NAMES tc PATHS /usr/sbin /sbin REQUIRED)
   find_program (WEBRTC_IP_EXECUTABLE NAMES ip REQUIRED)
   find_program (WEBRTC_UNSHARE_EXECUTABLE NAMES unshare REQUIRED)
   foreach (RELAY_MODE firewall impaired)
      add_test (NAME webrtc_${RELAY_MODE}_relay COMMAND "${Python3_EXECUTABLE}"
         "${CMAKE_CURRENT_SOURCE_DIR}/tests/interop/run_isolated_relay.py"
         --mode "${RELAY_MODE}" --nft "${WEBRTC_NFT_EXECUTABLE}" --tc "${WEBRTC_TC_EXECUTABLE}"
         --ip "${WEBRTC_IP_EXECUTABLE}" --unshare "${WEBRTC_UNSHARE_EXECUTABLE}"
         --fixture "${CMAKE_CURRENT_SOURCE_DIR}/tests/interop/run_phase3_fixture.py"
         --turnserver "${WEBRTC_TURN_SERVER}" --uclient "${WEBRTC_TURN_UCLIENT}"
         --origo "${TURN_ORIGO}" --public-turn "${PUBLIC_TURN_PROGRAM}"
         --benchmark "${CHANNEL_BENCH_PROGRAM}" --chrome "${WEBRTC_CHROME_EXECUTABLE}"
         --evidence "${CMAKE_CURRENT_BINARY_DIR}/interop/${RELAY_MODE}")
      set_tests_properties (webrtc_${RELAY_MODE}_relay PROPERTIES LABELS webrtc_topology
         TIMEOUT 1150 RUN_SERIAL TRUE ENVIRONMENT "NODE_PATH=${WEBRTC_INTEROP_NODE_PATH}")
   endforeach ()
endif ()

# Candidate parsing and all PeerConnection implementations use the internal Network mDNS service.
foreach (MDNS_TARGET webrtc test_webrtc_ice_candidates test_webrtc_peer_connection
   webrtc_public_peer_probe test_webrtc_ice_socket test_webrtc_full_ice_socket
   webrtc_native_probe webrtc_native_full_probe webrtc_native_sctp_probe)
   if (TARGET ${MDNS_TARGET})
      target_link_libraries (${MDNS_TARGET} PRIVATE network_mdns)
   endif ()
endforeach ()

flute_test (webrtc_mdns_privacy "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_mdns_privacy.tiri")
if (BUILD_TESTS AND WEBRTC_CHROME_EXECUTABLE AND NODE_EXECUTABLE AND (PLAYWRIGHT_RESULT EQUAL 0))
   add_test (NAME webrtc_mdns_interop COMMAND "${NODE_EXECUTABLE}"
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/interop/public_mdns.cjs"
      "${WEBRTC_ORIGO}"
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/interop/mdns_signalling.tiri"
      "${WEBRTC_CHROME_EXECUTABLE}" "${CMAKE_CURRENT_BINARY_DIR}/interop/mdns.json")
   set_tests_properties (webrtc_mdns_interop PROPERTIES LABELS webrtc_mdns_interop
      TIMEOUT 120 RUN_SERIAL TRUE ENVIRONMENT "NODE_PATH=${WEBRTC_INTEROP_NODE_PATH}")
endif ()

if (TEST webrtc_mdns_interop)
   add_test (NAME webrtc_mdns_channels_interop COMMAND "${NODE_EXECUTABLE}"
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/interop/public_channels.cjs"
      "${WEBRTC_ORIGO}"
      "${PROJECT_SOURCE_DIR}/examples/webrtc/signalling.tiri"
      "${WEBRTC_CHROME_EXECUTABLE}" "${CMAKE_CURRENT_BINARY_DIR}/interop/mdns_channels.json" interop MDNS)
   set_tests_properties (webrtc_mdns_channels_interop PROPERTIES LABELS webrtc_mdns_interop
      TIMEOUT 120 RUN_SERIAL TRUE ENVIRONMENT "NODE_PATH=${WEBRTC_INTEROP_NODE_PATH}")
endif ()
if (WEBRTC_REQUIRE_INTEROP AND NOT TEST webrtc_mdns_interop)
   message (FATAL_ERROR "Required mDNS browser fixture needs BUILD_TESTS, Chromium, Node.js and Playwright.")
endif ()

if (UNIT_TESTS AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
   add_library (webrtc_mdns_unavailable SHARED ../network/mdns/unavailable_fixture.cpp)
   target_link_libraries (webrtc_mdns_unavailable PRIVATE ${CMAKE_DL_LIBS})
   flute_test (webrtc_mdns_unavailable "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_mdns_unavailable.tiri")
   set_tests_properties (webrtc_mdns_unavailable PROPERTIES
      ENVIRONMENT "LD_PRELOAD=${WEBRTC_PRELOAD_PREFIX}$<TARGET_FILE:webrtc_mdns_unavailable>")
endif ()

# Interface notifications are owned by the task driving PeerConnection, including native test implementations.
add_library (webrtc_interfaces STATIC interface_monitor.cpp)
set_target_properties (webrtc_interfaces PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON
   POSITION_INDEPENDENT_CODE ON)
if (WIN32)
   target_link_libraries (webrtc_interfaces PUBLIC ws2_32 iphlpapi)
endif ()
foreach (INTERFACE_TARGET webrtc test_webrtc_peer_connection)
   if (TARGET ${INTERFACE_TARGET})
      target_link_libraries (${INTERFACE_TARGET} PRIVATE webrtc_interfaces)
   endif ()
endforeach ()

if (UNIT_TESTS)
   add_executable (test_webrtc_interface_monitor tests/test_interface_monitor.cpp)
   target_link_libraries (test_webrtc_interface_monitor PRIVATE webrtc_interfaces)
   set_target_properties (test_webrtc_interface_monitor PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
   add_test (NAME webrtc_interface_monitor COMMAND test_webrtc_interface_monitor)
   set_tests_properties (webrtc_interface_monitor PROPERTIES LABELS webrtc TIMEOUT 30)
endif ()
flute_test (webrtc_peer_restart "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_peer_restart.tiri")

if (UNIT_TESTS AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
   add_library (webrtc_interface_change SHARED tests/interface_change_fixture.cpp)
   target_link_libraries (webrtc_interface_change PRIVATE ${CMAKE_DL_LIBS})
   set_target_properties (webrtc_interface_change PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
   if (RUN_ANYWHERE)
      set (RECOVERY_ORIGO "${CMAKE_INSTALL_PREFIX}/origo")
   else ()
      set (RECOVERY_ORIGO "${CMAKE_INSTALL_PREFIX}/bin/origo")
   endif ()
   foreach (RECOVERY_CASE recover free fail)
      if (RECOVERY_CASE STREQUAL "recover")
         set (RECOVERY_FILTER DeferredRecover)
      elseif (RECOVERY_CASE STREQUAL "free")
         set (RECOVERY_FILTER DeferredFree)
      else ()
         set (RECOVERY_FILTER DeferredFail)
      endif ()
      add_test (NAME webrtc_network_change_${RECOVERY_CASE} COMMAND "${RECOVERY_ORIGO}"
         "${PROJECT_SOURCE_DIR}/tools/flute.tiri"
         "file=${CMAKE_CURRENT_SOURCE_DIR}/tests/test_peer_network_change.tiri" "test=${RECOVERY_FILTER}"
         --log-warning --gfx-driver=headless WORKING_DIRECTORY "${CMAKE_INSTALL_PREFIX}")
      set_tests_properties (webrtc_network_change_${RECOVERY_CASE} PROPERTIES
         ENVIRONMENT "LD_PRELOAD=${WEBRTC_PRELOAD_PREFIX}$<TARGET_FILE:webrtc_interface_change>"
         LABELS webrtc TIMEOUT 60)
   endforeach ()
endif ()

option (WEBRTC_TEST_INTERFACE_NAMESPACE "Run real Linux notifications in a disposable user/network namespace" OFF)
option (WEBRTC_TEST_DNS_NAMESPACE "Run bounded Linux DNS teardown in private namespaces" OFF)
if (WEBRTC_TEST_DNS_NAMESPACE)
   if (NOT CMAKE_SYSTEM_NAME STREQUAL "Linux" OR NOT TARGET test_webrtc_public_turn)
      message (FATAL_ERROR "DNS namespace fixture requires Linux and UNIT_TESTS.")
   endif ()
   find_package (Python3 COMPONENTS Interpreter REQUIRED)
   add_test (NAME webrtc_dns_teardown COMMAND "${Python3_EXECUTABLE}"
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/interop/test_linux_dns_close.py" --probe "${PUBLIC_TURN_PROGRAM}")
   set_tests_properties (webrtc_dns_teardown PROPERTIES LABELS webrtc TIMEOUT 45
      WORKING_DIRECTORY "${CMAKE_INSTALL_PREFIX}")
endif ()
if (WEBRTC_TEST_INTERFACE_NAMESPACE)
   if (NOT CMAKE_SYSTEM_NAME STREQUAL "Linux" OR NOT TARGET test_webrtc_interface_monitor)
      message (FATAL_ERROR "Interface namespace fixture requires Linux and UNIT_TESTS.")
   endif ()
   find_package (Python3 COMPONENTS Interpreter REQUIRED)
   add_test (NAME webrtc_interface_notifications COMMAND "${Python3_EXECUTABLE}"
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/interop/test_interface_notifications.py"
      "$<TARGET_FILE:test_webrtc_interface_monitor>")
   set_tests_properties (webrtc_interface_notifications PROPERTIES LABELS webrtc TIMEOUT 30)
   add_test (NAME webrtc_ipv6_path_removal COMMAND "${Python3_EXECUTABLE}"
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/interop/test_linux_path_removal.py" --probe "${PUBLIC_TURN_PROGRAM}"
      --evidence "${CMAKE_CURRENT_BINARY_DIR}/interop/ipv6_path_removal")
   set_tests_properties (webrtc_ipv6_path_removal PROPERTIES LABELS webrtc_topology TIMEOUT 95
      WORKING_DIRECTORY "${CMAKE_INSTALL_PREFIX}")
endif ()

if (UNIT_TESTS AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
   add_library (webrtc_direct_loss SHARED tests/direct_loss_fixture.cpp)
   target_link_libraries (webrtc_direct_loss PRIVATE ${CMAKE_DL_LIBS})
   set_target_properties (webrtc_direct_loss PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
   if (TEST webrtc_turn_integration)
      set_property (TEST webrtc_turn_integration APPEND PROPERTY ENVIRONMENT
         "WEBRTC_PEER_TRANSITION=${PEER_CONNECTION_PROGRAM}"
         "WEBRTC_DIRECT_LOSS=${WEBRTC_PRELOAD_PREFIX}$<TARGET_FILE:webrtc_direct_loss>")
      add_test (NAME webrtc_tls_shutdown_stress COMMAND "${Python3_EXECUTABLE}"
         "${CMAKE_CURRENT_SOURCE_DIR}/tests/interop/test_linux_tls_shutdown.py"
         --origo "${TURN_ORIGO}" --openssl "${OPENSSL_EXECUTABLE}"
         --evidence "${CMAKE_CURRENT_BINARY_DIR}/interop/tls_shutdown")
      set_tests_properties (webrtc_tls_shutdown_stress PROPERTIES LABELS webrtc TIMEOUT 180 RUN_SERIAL TRUE)
   endif ()
endif ()

flute_test (webrtc_peer_stats "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_peer_stats.tiri")

option (WEBRTC_TEST_WINDOWS_NOTIFICATIONS "Run elevated Windows loopback address-change acceptance" OFF)
if (WEBRTC_TEST_WINDOWS_NOTIFICATIONS)
   if (NOT WIN32 OR NOT TARGET test_webrtc_interface_monitor)
      message (FATAL_ERROR "Windows notification acceptance requires Windows and UNIT_TESTS.")
   endif ()
   find_package (Python3 COMPONENTS Interpreter REQUIRED)
   add_test (NAME webrtc_windows_notifications COMMAND "${Python3_EXECUTABLE}"
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/interop/test_windows_notifications.py"
      --monitor "$<TARGET_FILE:test_webrtc_interface_monitor>"
      --origo "${CMAKE_INSTALL_PREFIX}/origo${CMAKE_EXECUTABLE_SUFFIX}")
   set_tests_properties (webrtc_windows_notifications PROPERTIES LABELS webrtc TIMEOUT 180 RUN_SERIAL TRUE)
   add_test (NAME webrtc_windows_path_removal COMMAND "${Python3_EXECUTABLE}"
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/interop/test_windows_path_removal.py"
      --probe "${PUBLIC_TURN_PROGRAM}"
      --evidence "${CMAKE_CURRENT_BINARY_DIR}/interop/windows_path_removal")
   set_tests_properties (webrtc_windows_path_removal PROPERTIES LABELS webrtc TIMEOUT 120 RUN_SERIAL TRUE)
   add_test (NAME webrtc_windows_dns_close COMMAND "${Python3_EXECUTABLE}"
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/interop/test_windows_dns_close.py"
      --probe "${PUBLIC_TURN_PROGRAM}")
   set_tests_properties (webrtc_windows_dns_close PROPERTIES LABELS webrtc TIMEOUT 90 RUN_SERIAL TRUE)
   add_test (NAME webrtc_windows_dns_capacity COMMAND "${Python3_EXECUTABLE}"
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/interop/test_windows_dns_close.py"
      --probe "${PUBLIC_TURN_PROGRAM}" --capacity)
   set_tests_properties (webrtc_windows_dns_capacity PROPERTIES LABELS webrtc TIMEOUT 90 RUN_SERIAL TRUE)
   add_test (NAME webrtc_windows_dns_free COMMAND "${Python3_EXECUTABLE}"
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/interop/test_windows_dns_close.py"
      --probe "${PUBLIC_TURN_PROGRAM}" --destroy)
   set_tests_properties (webrtc_windows_dns_free PROPERTIES LABELS webrtc TIMEOUT 90 RUN_SERIAL TRUE)
endif ()

# Windows development/CI gate.  Keep public behaviour and protocol regressions in the routine run;
# staged interop probes and elevated topology fixtures remain available for targeted acceptance.
if (WIN32)
   set (WEBRTC_WINDOWS_ESSENTIALS
      webrtc_turn_configuration webrtc_webchannel_loopback webrtc_webchannel_lifecycle
      webrtc_sctp_packet webrtc_sctp_association webrtc_sctp_transfer webrtc_sctp_streams webrtc_sctp_dtls
      webrtc_opaque_string webrtc_turn webrtc_stun webrtc_stun_long_term webrtc_sdp
      webrtc_ice webrtc_ice_agent webrtc_ice_candidates webrtc_dtls
      webrtc_peer_connection webrtc_public_webchannel webrtc_interface_monitor
      webrtc_peer_restart webrtc_peer_stats webrtc_mdns_privacy
      webrtc_public_channels_interop webrtc_mdns_channels_interop webrtc_turn_integration)
   get_property (WEBRTC_REGISTERED_TESTS DIRECTORY PROPERTY TESTS)
   foreach (WEBRTC_TEST IN LISTS WEBRTC_REGISTERED_TESTS)
      if (NOT WEBRTC_TEST MATCHES "^webrtc_")
         continue ()
      endif ()
      get_property (WEBRTC_TEST_LABELS TEST ${WEBRTC_TEST} PROPERTY LABELS)
      if ("webrtc_bench" IN_LIST WEBRTC_TEST_LABELS)
         continue ()
      elseif (WEBRTC_TEST IN_LIST WEBRTC_WINDOWS_ESSENTIALS)
         set_property (TEST ${WEBRTC_TEST} APPEND PROPERTY LABELS webrtc_windows_essential)
      else ()
         set_property (TEST ${WEBRTC_TEST} APPEND PROPERTY LABELS webrtc_windows_extended)
      endif ()
   endforeach ()
endif ()
