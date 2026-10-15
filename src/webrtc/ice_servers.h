// Copied, bounded public ICE-server configuration; parsing never logs credentials.
#pragma once
#include <kotuku/main.h>
#include <string>
#include <string_view>
#include <vector>
namespace rtc {
enum class ServerTransport { STUN, UDP, TCP, TLS };
struct IceServer {
   std::string host, username, credential;
   uint16_t port = 3478;
   ServerTransport transport = ServerTransport::STUN;
};
constexpr uint32_t POLICY_RELAY_ONLY = 1, POLICY_NO_HOST = 2, POLICY_NO_MDNS = 4, POLICY_DISABLE_IPV6 = 8;
ERR parse_ice_servers(std::string_view Text, std::vector<IceServer> &Result);
}
