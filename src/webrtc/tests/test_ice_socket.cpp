#include "socket_support.h"

#include <kotuku/startup.h>
#include <kotuku/main.h>
#include <kotuku/modules/network.h>
#include <kotuku/modules/module.h>

#include "../ice/ice_socket.h"
#include "../stun/stun.h"


#include <array>
#include <chrono>
#include <cstdio>
#include <string>
#include <vector>

JUMPTABLE_NETWORK
static OBJECTPTR glNetworkModule;
CSTRING ProgName = "WebRTCIceSocketTest";

using namespace rtc;

static std::vector<uint8_t> check_request() {
   std::array<uint8_t, 12> tx{ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12 };
   std::array<uint8_t, 4> priority{ 0x6e, 0, 1, 0xff };
   std::array<uint8_t, 8> controlling{ 1, 2, 3, 4, 5, 6, 7, 8 };
   std::string username = "local:remote";
   std::string password = "local-password-0123456789";
   std::array<stun::Attribute, 3> attributes{
      stun::Attribute{ stun::ATTR_USERNAME,
         std::span<const uint8_t>((const uint8_t *)username.data(), username.size()), 0 },
      stun::Attribute{ 0x0024, priority, 0 },
      stun::Attribute{ 0x802a, controlling, 0 }
   };
   std::vector<uint8_t> result;
   auto key = std::span<const uint8_t>((const uint8_t *)password.data(), password.size());
   if (not stun::encode(stun::BINDING_REQUEST, tx, attributes, key, false, true, result)) return {};
   return result;
}

static int run_test() {
   std::shared_ptr<DtlsCertificate> client_cert, server_cert;
   if (DtlsCertificate::generate(client_cert) != ERR::Okay or
       DtlsCertificate::generate(server_cert) != ERR::Okay) return 1;
   bool received_application = false;
   IceDtlsCallbacks server_callbacks;
   server_callbacks.receive = [&](std::span<const uint8_t> Data) { received_application = Data.size() IS 3; };
   IceSocket server(server_cert, "local", "local-password-0123456789", "remote", server_callbacks);
   if (server.set_remote_fingerprint(client_cert->fingerprint()) != ERR::Okay or
       server.open("127.0.0.1", 0) != ERR::Okay or server.port() <= 0) return 2;

   auto fd = rtc::test::udp_socket();
   if (fd IS rtc::test::INVALID_SOCKET_VALUE) return 3;
   sockaddr_in destination{};
   destination.sin_family = AF_INET;
   destination.sin_port = htons(uint16_t(server.port()));
   destination.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
   if (::connect(fd, (sockaddr *)&destination, sizeof(destination)) != 0) { rtc::test::close_socket(fd); return 4; }

   DtlsCallbacks client_callbacks;
   bool send_failed = false;
   client_callbacks.send = [&](std::span<const uint8_t> Data) {
      if (rtc::test::send(fd, Data.data(), Data.size(), 0) != int(Data.size())) send_failed = true;
   };
   DtlsTransport client(client_cert, DtlsRole::CLIENT, client_callbacks);
   if (client.set_remote_fingerprint("sha-256", server_cert->fingerprint()) != ERR::Okay) {
      rtc::test::close_socket(fd);
      return 5;
   }

   auto request = check_request();
   if (request.empty() or rtc::test::send(fd, request.data(), request.size(), 0) != int(request.size())) {
      rtc::test::close_socket(fd);
      return 6;
   }
   std::array<uint8_t, 4096> buffer{};
   bool stun_success = false;
   auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
   while (std::chrono::steady_clock::now() < deadline and not stun_success) {
      ProcessMessages(PMF::NIL, 10);
      auto length = rtc::test::receive(fd, buffer.data(), buffer.size(), 0);
      if (length <= 0) continue;
      stun::Message response;
      stun_success = stun::parse(std::span<const uint8_t>(buffer.data(), size_t(length)), response) and
         response.type IS stun::BINDING_SUCCESS;
   }
   if (not stun_success or client.start() != ERR::Okay) { rtc::test::close_socket(fd); return 7; }

   while (std::chrono::steady_clock::now() < deadline) {
      ProcessMessages(PMF::NIL, 10);
      for (;;) {
         auto length = rtc::test::receive(fd, buffer.data(), buffer.size(), 0);
         if (length <= 0) break;
         if (client.feed(std::span<const uint8_t>(buffer.data(), size_t(length))) != ERR::Okay) {
            rtc::test::close_socket(fd);
            return 8;
         }
      }
      if (server.state() IS DtlsState::CONNECTED and client.state() IS DtlsState::CONNECTED) break;
      if (server.handle_timeout() != ERR::Okay or client.handle_timeout() != ERR::Okay) {
         rtc::test::close_socket(fd);
         return 9;
      }
   }
   if (server.state() != DtlsState::CONNECTED or client.state() != DtlsState::CONNECTED or
       send_failed or server.socket_error() != ERR::Okay) { rtc::test::close_socket(fd); return 10; }
   std::array<uint8_t, 3> payload{ 1, 2, 3 };
   if (client.send(payload) != ERR::Okay) { rtc::test::close_socket(fd); return 11; }
   while (std::chrono::steady_clock::now() < deadline and not received_application) {
      ProcessMessages(PMF::NIL, 10);
   }
   rtc::test::close_socket(fd);
   return received_application ? 0 : 12;
}

int main(int argc, CSTRING *argv) {
   if (auto message = init_kotuku(argc, argv)) {
      std::fprintf(stderr, "%s\n", message);
      return 1;
   }
   if (objModule::load("network", &glNetworkModule, &NetworkBase) != ERR::Okay) {
      close_kotuku();
      return 1;
   }
   auto result = run_test();
   if (result) std::fprintf(stderr, "ICE/DTLS NetSocket loopback failed at step %d\n", result);
   close_kotuku();
   return result ? 1 : 0;
}
