#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif

#include <kotuku/startup.h>
#include <kotuku/main.h>
#include <kotuku/modules/module.h>
#include <kotuku/modules/webrtc.h>

#include <chrono>
#include <cstdio>
#include <iostream>
#include <iterator>
#include <string>

static OBJECTPTR glWebRTCModule;
CSTRING ProgName = "WebRTCPublicPeerProbe";

static int run_probe() {
   std::string offer(std::istreambuf_iterator<char>{ std::cin }, {});
   auto peer = objPeerConnection::create::local(fl::Flags(RTCF::NO_MDNS));
   if (not peer) return 2;
   auto release = kt::Defer([&]() { FreeResource(peer); });
   if (peer->setRemoteDescription(offer) != ERR::Okay or peer->createAnswer() != ERR::Okay) return 3;
   auto answer = peer->LocalDescription;
   if (answer.empty() or peer->setLocalDescription(answer) != ERR::Okay) return 4;
   std::cout << answer.size() << '\n' << answer << std::flush;

   auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(12);
   auto linger_until = deadline;
   bool connected = false;
   while (std::chrono::steady_clock::now() < deadline) {
      ProcessMessages(PMF::NIL, 10);
      if (peer->State IS RTCS::CONNECTED and not connected) {
         connected = true;
         linger_until = std::chrono::steady_clock::now() + std::chrono::seconds(2);
         std::cout << "CONNECTED\n" << std::flush;
      }
      if (connected and std::chrono::steady_clock::now() >= linger_until) return 0;
      if (peer->State IS RTCS::FAILED or peer->Error != ERR::Okay) return 5;
   }
   return 6;
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
   if (objModule::load("webrtc", &glWebRTCModule) != ERR::Okay) {
      close_kotuku();
      return 1;
   }
   auto result = run_probe();
   if (result) std::fprintf(stderr, "Public WebRTC probe failed at step %d\n", result);
   close_kotuku();
   return result ? 1 : 0;
}
