// Single-owner Network adapter.  No application callbacks run inside Network dispatch.
#pragma once
#include "turn.h"
#include "transport_stats.h"
#include "ice_agent.h"
#include "ice_servers.h"
#include <kotuku/modules/network.h>
#include <memory>
namespace rtc {
class TurnSocket {
public:
   TurnSocket(const IceServer &Server, const turn::Identity &Identity);
   ~TurnSocket();
   void retire();
   ERR open(uint64_t NowUs);
   void tick(uint64_t NowUs);
   bool send(const ice::Endpoint &Peer, std::span<const uint8_t> Data, uint64_t NowUs);
   void permit(const ice::Endpoint &Peer, uint64_t NowUs);
   bool pending() const { return error IS ERR::Okay and client.state() != turn::State::ACTIVE; }
   size_t available() const { auto used = buffered(); return used < turn::MAX_QUEUED - turn::MAINTENANCE_RESERVE ?
      turn::MAX_QUEUED - turn::MAINTENANCE_RESERVE - used : 0; }
   bool active() const { return error IS ERR::Okay and client.state() IS turn::State::ACTIVE; }
   ERR failure() const { return error; }
   const char * failure_reason() const;
   ice::Candidate candidate() const;
   std::vector<turn::Datagram> take_input();
   TransportCounters counters;
   ServerTransport client_transport() const { return server.transport; }
   const turn::Identity & identity() const { return id; }
private:
   static ERR incoming_udp(objNetServer *Socket, APTR Context);
   static ERR incoming_stream(objNetSocket *Socket, APTR Context);
   static ERR feedback(objNetSocket *Socket, NTC State, APTR Context);
   void receive(std::span<const uint8_t> Wire, uint64_t NowUs);
   void flush(uint64_t NowUs);
   size_t buffered() const;
   IceServer server;
   turn::Identity id;
   turn::Client client;
   objNetServer *udp = nullptr;
   objNetSocket *stream = nullptr;
   turn::StreamDecoder decoder;
   std::deque<turn::Packet> output;
   std::deque<turn::Datagram> waiting;
   std::vector<turn::Datagram> input;
   std::vector<ice::Endpoint> peers;
   std::vector<uint8_t> stream_input;
   int previous_queue = 0;
   size_t output_bytes = 0, waiting_bytes = 0, offset = 0, input_bytes = 0;
   uint64_t deadline = 0, stall_since = 0;
   bool connected = false, disabled = false, retired = false;
   ERR error = ERR::Okay;
   const char *timeout_reason = "TURN connection deadline exhausted";
};
}
