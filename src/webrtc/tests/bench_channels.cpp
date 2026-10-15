#include <kotuku/startup.h>
#include <kotuku/main.h>
#include <kotuku/modules/module.h>
#include <kotuku/modules/webrtc.h>
#include "../sctp_association.h"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <cmath>
#include <deque>
#include <fstream>
#include <iterator>
#include <charconv>

CSTRING ProgName = "WebChannelBenchmark";
namespace {
using Clock = std::chrono::steady_clock;
std::vector<double> glRtt, glConcurrent;
std::vector<uint8_t> glPayload;
std::string glRelayConfig;
bool glPing = false, glBulk = false, glFailed = false;
size_t glQueueHigh = 0;
unsigned glWaitSeconds = 90;
Clock::time_point glBulkReceived;
void record(std::vector<double> &Values, double Value) { Values.push_back(Value); }
double percentile(std::vector<double> Values, double Fraction) {
   if (Values.empty()) return 0;
   std::sort(Values.begin(), Values.end());
   return Values[size_t(std::ceil(Values.size() * Fraction)) - 1];
}
void report(const char *Name, const std::vector<double> &Rtt, const std::vector<double> &Concurrent,
   double Seconds, double Cpu, size_t High, int Retransmissions, int Delay, int Loss) {
   std::printf("{\"name\":\"%s\",\"rtt_samples\":%zu,\"rtt_p50_ms\":%.3f,\"rtt_p95_ms\":%.3f,"
      "\"rtt_p99_ms\":%.3f,\"concurrent_samples\":%zu,\"concurrent_p50_ms\":%.3f,"
      "\"concurrent_p95_ms\":%.3f,\"concurrent_p99_ms\":%.3f,\"bulk_bytes\":%zu,"
      "\"goodput_mib_s\":%.3f,\"cpu_seconds\":%.3f,\"queue_high_bytes\":%zu,\"retransmissions\":%d,"
      "\"one_way_delay_ms\":%d,\"loss_percent\":%d}\n", Name, Rtt.size(), percentile(Rtt, .5),
      percentile(Rtt, .95), percentile(Rtt, .99), Concurrent.size(), percentile(Concurrent, .5),
      percentile(Concurrent, .95), percentile(Concurrent, .99), glPayload.size(),
      glPayload.size() / Seconds / 1048576., Cpu, High, Retransmissions, Delay, Loss);
}
ERR echo(objWebChannel *Channel, const void *Data, int Size, WCM Type, APTR) {
   return Channel->send(std::span<const int8_t>((const int8_t *)Data, size_t(Size)), Type);
}
ERR received(objWebChannel *Channel, const void *Data, int Size, WCM Type, APTR) {
   int id = -1;
   if (Channel->getID(id) != ERR::Okay) { glFailed = true; return ERR::Failed; }
   if (id IS 42) {
      if (Type != WCM::BINARY or size_t(Size) != glPayload.size() or
          not std::equal(glPayload.begin(), glPayload.end(), (const uint8_t *)Data)) glFailed = true;
      if (glFailed) std::fprintf(stderr, "Bulk mismatch: type=%d size=%d expected=%zu id=%d\n",
         int(Type), Size, glPayload.size(), id);
      glBulkReceived = Clock::now();
      glBulk = true;
   } else {
      if (Type != WCM::TEXT or Size != 4 or std::string_view((const char *)Data, Size) != "ping") glFailed = true;
      if (glFailed) std::fprintf(stderr, "Ping mismatch: type=%d size=%d id=%d\n", int(Type), Size, id);
      glPing = true;
   }
   return ERR::Okay;
}
template<class Predicate> bool wait_for(Predicate Ready) {
   auto deadline = Clock::now() + std::chrono::seconds(glWaitSeconds);
   while (not Ready() and not glFailed and Clock::now() < deadline) ProcessMessages(PMF::NIL, 1);
   return Ready() and not glFailed;
}
bool native() {
   auto first = objPeerConnection::create::local(
      fl::Flags(glRelayConfig.empty() ? RTCF::NO_MDNS : RTCF::RELAY_ONLY | RTCF::DISABLE_IPV6),
      kt::FieldValue(kt::fieldhash("IceServers"), glRelayConfig.empty() ? "[]" : glRelayConfig),
      kt::FieldValue(kt::fieldhash("ReceiveLimit"), 16777216));
   if (not first) { std::fprintf(stderr, "Native benchmark stage 1\n"); return false; }
   auto cleanup_a = kt::Defer([&]() { FreeResource(first); });
   auto second = objPeerConnection::create::local(
      fl::Flags(glRelayConfig.empty() ? RTCF::NO_MDNS : RTCF::RELAY_ONLY | RTCF::DISABLE_IPV6),
      kt::FieldValue(kt::fieldhash("IceServers"), glRelayConfig.empty() ? "[]" : glRelayConfig),
      kt::FieldValue(kt::fieldhash("ReceiveLimit"), 16777216));
   if (not second) { std::fprintf(stderr, "Native benchmark stage 2\n"); return false; }
   auto cleanup_b = kt::Defer([&]() { FreeResource(second); });
   objWebChannel *bulk = nullptr, *ping = nullptr, *remote_bulk = nullptr, *remote_ping = nullptr;
   if (first->createWebChannel("bulk", "", true, true, 42, -1, -1, &bulk) != ERR::Okay or
       first->createWebChannel("ping", "", true, true, 44, -1, -1, &ping) != ERR::Okay or
       second->createWebChannel("bulk", "", true, true, 42, -1, -1, &remote_bulk) != ERR::Okay or
       second->createWebChannel("ping", "", true, true, 44, -1, -1, &remote_ping) != ERR::Okay) {
      std::fprintf(stderr, "Native benchmark stage 3\n"); return false;
   }
   if (bulk->setOnMessage(C_FUNCTION(received)) != ERR::Okay or
       ping->setOnMessage(C_FUNCTION(received)) != ERR::Okay or
       remote_bulk->setOnMessage(C_FUNCTION(echo)) != ERR::Okay or
       remote_ping->setOnMessage(C_FUNCTION(echo)) != ERR::Okay) {
      std::fprintf(stderr, "Native benchmark stage 4\n"); return false;
   }
   if (first->createOffer() != ERR::Okay or
       not wait_for([&]() { return first->GatheringState IS RTCGATHER::COMPLETE; }) or first->setLocalDescription(first->LocalDescription) != ERR::Okay or
       second->setRemoteDescription(first->LocalDescription) != ERR::Okay or second->createAnswer() != ERR::Okay or
       not wait_for([&]() { return second->GatheringState IS RTCGATHER::COMPLETE; }) or
       second->setLocalDescription(second->LocalDescription) != ERR::Okay or
       first->setRemoteDescription(second->LocalDescription) != ERR::Okay) {
      std::fprintf(stderr, "Native benchmark stage 5\n"); return false;
   }
   if (not wait_for([&]() { return bulk->State IS WCS::OPEN and ping->State IS WCS::OPEN; })) {
      std::fprintf(stderr, "Native benchmark stage 6\n"); return false;
   }
   for (unsigned i = 0; i < 100; ++i) {
      glPing = false;
      auto start = Clock::now();
      if (ping->send(std::span<const int8_t>((const int8_t *)"ping", 4), WCM::TEXT) != ERR::Okay or
          not wait_for([]() { return glPing; })) { std::fprintf(stderr, "Native benchmark stage 7\n"); return false; }
      record(glRtt, std::chrono::duration<double, std::milli>(Clock::now() - start).count());
   }
   auto start = Clock::now(); auto cpu = std::clock();
   if (bulk->send(std::span<const int8_t>((const int8_t *)glPayload.data(), glPayload.size()),
       WCM::BINARY) != ERR::Okay) { std::fprintf(stderr, "Native benchmark stage 8\n"); return false; }
   int64_t buffered = 0;
   if (bulk->getBufferedAmount(buffered) != ERR::Okay) {
      std::fprintf(stderr, "Native benchmark stage 9\n"); return false;
   }
   glQueueHigh = size_t(buffered);
   for (unsigned i = 0; i < 10; ++i) {
      glPing = false; auto ping_start = Clock::now();
      if (ping->send(std::span<const int8_t>((const int8_t *)"ping", 4), WCM::TEXT) != ERR::Okay or
          not wait_for([]() { return glPing; })) { std::fprintf(stderr, "Native benchmark stage 10\n"); return false; }
      record(glConcurrent, std::chrono::duration<double, std::milli>(Clock::now() - ping_start).count());
   }
   if (not wait_for([]() { return glBulk; })) { std::fprintf(stderr, "Native benchmark stage 11\n"); return false; }
   STRING snapshot = nullptr;
   if (first->getStats(&snapshot) != ERR::Okay or not snapshot) return false;
   std::printf("Route evidence: %s\n", snapshot);
   std::string stats(snapshot);
   FreeResource(snapshot);
   if (not glRelayConfig.empty() and stats.find("\"localType\":\"relay\"") IS std::string::npos) return false;
   auto marker = stats.find("\"retransmissions\":\"");
   if (marker IS std::string::npos) return false;
   marker += std::string_view("\"retransmissions\":\"").size();
   int retransmissions = 0;
   auto parsed = std::from_chars(stats.data() + marker, stats.data() + stats.size(), retransmissions);
   if (parsed.ec != std::errc()) return false;
   report(glRelayConfig.empty() ? "public_native_loopback_echo" : "public_native_relay_echo", glRtt, glConcurrent,
      std::chrono::duration<double>(glBulkReceived - start).count(), double(std::clock() - cpu) / CLOCKS_PER_SEC,
      glQueueHigh, retransmissions, 0, 0);
   return true;
}
struct Datagram { uint64_t due; bool to_b; std::vector<uint8_t> wire; };
bool simulated(int Delay, int Loss) {
   rtc::SctpConfig config;
   config.local_port = 5000; config.remote_port = 5001; config.receive_limit = 16777216;
   config.delayed_ack_us = 2000; config.min_rto_us = 10000; config.initial_rto_us = 40000;
   rtc::SctpAssociation a(config);
   config.local_port = 5001; config.remote_port = 5000;
   rtc::SctpAssociation b(config);
   uint64_t now = 0, ping_start = 0, bulk_start = 0, bulk_received = 0;
   unsigned sequence = 0, pings = 0;
   bool ping_pending = false, bulk_started = false, bulk_done = false;
   std::vector<double> rtt, concurrent;
   std::deque<Datagram> queue;
   size_t high = 0;
   auto cpu = std::clock();
   if (a.start(now) != ERR::Okay) return false;
   for (unsigned step = 0; step < 480000; ++step) {
      auto route = [&](rtc::SctpAssociation &Sender, bool ToB) {
         for (auto &wire : Sender.take_output()) {
            Sender.output_accepted(wire);
            ++sequence;
            if (Loss and sequence % 100 < unsigned(Loss)) continue;
            queue.push_back({now + uint64_t(Delay) * 1000, ToB, std::move(wire)});
         }
      };
      route(a, true); route(b, false);
      if (queue.size() > 4096) return false;
      while (not queue.empty() and queue.front().due <= now) {
         auto packet = std::move(queue.front()); queue.pop_front();
         if ((packet.to_b ? b : a).feed(packet.wire, now) != ERR::Okay) return false;
      }
      for (auto &message : b.take_messages()) {
         if (message.stream IS 0) {
            if (message.payload != glPayload) return false;
            bulk_received = now;
            bulk_done = true;
         } else if (b.enqueue(1, 53, message.payload, false, now) != ERR::Okay) return false;
      }
      for (auto &message : a.take_messages()) {
         if (message.stream != 1 or message.payload != std::vector<uint8_t>({1, 2, 3, 4})) return false;
         record(bulk_started ? concurrent : rtt, double(now - ping_start) / 1000.);
         ping_pending = false; ++pings;
      }
      if (a.state() IS rtc::SctpState::ESTABLISHED and b.state() IS rtc::SctpState::ESTABLISHED) {
         if (pings IS 100 and not bulk_started) {
            bulk_started = true; bulk_start = now;
            if (a.enqueue(0, 53, glPayload, false, now) != ERR::Okay) return false;
         }
         if (not ping_pending and pings < 110) {
            ping_pending = true; ping_start = now;
            const uint8_t ping[] = {1, 2, 3, 4};
            if (a.enqueue(1, 53, ping, false, now) != ERR::Okay) return false;
         }
         high = std::max(high, a.diagnostics().retained_bytes);
         if (bulk_done and pings IS 110 and a.diagnostics().retained_bytes IS 0) {
            report("simulated_sctp", rtt, concurrent, double(bulk_received - bulk_start) / 1000000.,
               double(std::clock() - cpu) / CLOCKS_PER_SEC, high, a.diagnostics().retransmissions, Delay, Loss);
            return true;
         }
      }
      now += 250;
      if (a.tick(now) != ERR::Okay or b.tick(now) != ERR::Okay) return false;
   }
   return false;
}
}
int main(int Argc, CSTRING *Argv) {
   if (auto error = init_kotuku(Argc, Argv)) { std::fprintf(stderr, "%s\n", error); return 1; }
   size_t payload_size = 1048576;
   for (int i = 1; i < Argc; ++i) {
      std::string_view argument(Argv[i]);
      if (argument.starts_with("--timeout=")) {
         auto parsed = std::from_chars(argument.data() + 10, argument.data() + argument.size(), glWaitSeconds);
         if (parsed.ec != std::errc{} or parsed.ptr != argument.data() + argument.size() or
             glWaitSeconds < 1 or glWaitSeconds > 300) {
            std::fprintf(stderr, "Benchmark timeout must be 1 to 300 seconds\n");
            close_kotuku(); return 1;
         }
         continue;
      }
      if (not argument.starts_with("--bytes=")) continue;
      auto parsed = std::from_chars(argument.data() + 8, argument.data() + argument.size(), payload_size);
      if (parsed.ec != std::errc{} or parsed.ptr != argument.data() + argument.size() or
          payload_size < 4096 or payload_size > 16777216) {
         std::fprintf(stderr, "Benchmark payload must be 4096 to 16777216 bytes\n");
         close_kotuku(); return 1;
      }
   }
   if (Argc > 1 and Argv[1][0] and Argv[1][0] != '-') {
      std::ifstream file(Argv[1]);
      if (not file) { close_kotuku(); return 1; }
      glRelayConfig.assign(std::istreambuf_iterator<char>(file), {});
   }
   OBJECTPTR module;
   glPayload.resize(payload_size);
   for (size_t i = 0; i < glPayload.size(); ++i) glPayload[i] = uint8_t(i * 13 + i / 113);
   bool okay = objModule::load("webrtc", &module) IS ERR::Okay;
   if (okay) { okay = native(); if (not okay) std::fprintf(stderr, "Native benchmark failed\n"); }
   if (okay and glRelayConfig.empty()) { okay = simulated(0, 0); if (not okay) std::fprintf(stderr, "Unimpaired simulation failed\n"); }
   if (okay and glRelayConfig.empty()) { okay = simulated(5, 1); if (not okay) std::fprintf(stderr, "Impaired simulation failed\n"); }
   close_kotuku();
   if (not okay) std::fprintf(stderr, "Channel benchmark failed payload validation or bounded completion\n");
   return okay ? 0 : 1;
}
