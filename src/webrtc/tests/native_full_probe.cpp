#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif

#include <kotuku/startup.h>
#include <kotuku/main.h>
#include <kotuku/modules/network.h>
#include <kotuku/modules/module.h>

#include "../ice_socket.h"
#include "../sdp.h"

#include <openssl/rand.h>

#include <chrono>
#include <cstdio>
#include <iostream>
#include <iterator>
#include <memory>
#include <string>

JUMPTABLE_NETWORK
static OBJECTPTR glNetworkModule;
CSTRING ProgName = "WebRTCNativeFullProbe";

static uint64_t now_us() {
   return uint64_t(std::chrono::duration_cast<std::chrono::microseconds>(
      std::chrono::steady_clock::now().time_since_epoch()).count());
}

static std::string random_ice_text(size_t Length) {
   static constexpr char ALPHABET[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
   std::string result(Length, '\0');
   for (size_t i = 0; i < Length; ++i) {
      uint8_t value = 0;
      if (RAND_bytes(&value, 1) != 1) return {};
      result[i] = ALPHABET[value & 63];
   }
   return result;
}

static int run_probe() {
   std::string offer_text(std::istreambuf_iterator<char>{ std::cin }, {});
   rtc::sdp::Description offer;
   if (not rtc::sdp::parse(offer_text, offer) or offer.data.setup != rtc::sdp::Setup::ACTPASS) return 2;
   std::shared_ptr<rtc::DtlsCertificate> certificate;
   if (rtc::DtlsCertificate::generate(certificate) != ERR::Okay) return 3;
   auto ufrag = random_ice_text(8);
   auto password = random_ice_text(32);
   if (ufrag.empty() or password.empty()) return 4;

   rtc::FullIceSocket socket(certificate, ufrag, password, rtc::ice::Role::CONTROLLED,
      rtc::DtlsRole::SERVER, {});
   if (socket.set_remote_fingerprint(offer.data.fingerprint) != ERR::Okay or
       socket.open("127.0.0.1", 0) != ERR::Okay or
       not socket.set_remote_credentials(offer.data.ice_ufrag, offer.data.ice_pwd, offer.ice_lite)) return 5;
   rtc::sdp::Description answer;
   answer.session_id = "1";
   answer.ice_lite = false;
   answer.trickle = false;
   answer.data.mid = offer.data.mid;
   answer.data.ice_ufrag = ufrag;
   answer.data.ice_pwd = password;
   answer.data.fingerprint = certificate->fingerprint();
   answer.data.setup = rtc::sdp::Setup::PASSIVE;
   answer.data.candidates.push_back("candidate:1 1 udp 2130706431 127.0.0.1 " +
      std::to_string(socket.local_address().port) + " typ host");
   answer.data.end_of_candidates = true;
   auto answer_text = rtc::sdp::serialize(answer);
   if (answer_text.empty() or not rtc::sdp::validate_answer(offer, answer)) return 6;
   std::cout << answer_text.size() << '\n' << answer_text << std::flush;
   if (not socket.start_ice(now_us())) return 10;

   auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(12);
   auto linger_until = deadline;
   bool connected = false;
   while (std::chrono::steady_clock::now() < deadline) {
      ProcessMessages(PMF::NIL, 10);
      if (socket.state() IS rtc::DtlsState::CONNECTED and not connected) {
         connected = true;
         linger_until = std::chrono::steady_clock::now() + std::chrono::seconds(2);
         std::cout << "CONNECTED\n" << std::flush;
      }
      if (connected and std::chrono::steady_clock::now() >= linger_until) return 0;
      if (socket.state() IS rtc::DtlsState::FAILED or socket.socket_error() != ERR::Okay) return 7;
      if (socket.handle_timeout(now_us()) != ERR::Okay) return 8;
   }
   return 9;
}

int main(int argc, CSTRING *argv) {
#ifdef _WIN32
   // SDP already contains CRLF; CRT text translation would alter both framing and the declared byte count.
   if (_setmode(_fileno(stdout), _O_BINARY) < 0) return 1;
#endif
   if (auto message = init_kotuku(argc, argv)) {
      std::fprintf(stderr, "%s\n", message);
      return 1;
   }
   if (objModule::load("network", &glNetworkModule, &NetworkBase) != ERR::Okay) {
      close_kotuku();
      return 1;
   }
   auto result = run_probe();
   if (result) std::fprintf(stderr, "Native WebRTC probe failed at step %d\n", result);
   close_kotuku();
   return result ? 1 : 0;
}
