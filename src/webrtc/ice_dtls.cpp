#include "ice_dtls.h"

#include <algorithm>
#include <utility>

namespace rtc {

IceDtlsTransport::IceDtlsTransport(std::shared_ptr<DtlsCertificate> Certificate, std::string_view LocalUfrag,
   std::string_view LocalPassword, std::string_view RemoteUfrag, IceDtlsCallbacks Callbacks) :
   ice_responder(LocalUfrag, LocalPassword, RemoteUfrag), callbacks(std::move(Callbacks)),
   dtls(std::move(Certificate), DtlsRole::SERVER, DtlsCallbacks{
      [this](std::span<const uint8_t> Datagram) {
         if (has_active_source and callbacks.send) callbacks.send(Datagram, active_source);
      },
      [this](std::span<const uint8_t> Data) { if (callbacks.receive) callbacks.receive(Data); },
      [this](DtlsState State) { if (callbacks.state) callbacks.state(State); }
   }) { }

ERR IceDtlsTransport::set_remote_fingerprint(std::string_view Fingerprint) {
   return dtls.set_remote_fingerprint("sha-256", Fingerprint);
}

ERR IceDtlsTransport::feed(std::span<const uint8_t> Datagram, const ice::Endpoint &Source) {
   if (Datagram.empty()) return ERR::Okay;
   switch (classify_packet(Datagram[0])) {
      case PacketClass::STUN: {
         auto check = ice_responder.receive(Datagram, Source);
         if (not check.response.empty() and callbacks.send) callbacks.send(check.response, Source);
         if (check.nominated and has_active_source) active_source = Source;
         return ERR::Okay;
      }
      case PacketClass::DTLS:
         if (not ice_responder.validated(Source)) return ERR::Okay;
         if (not has_active_source) {
            active_source = Source;
            has_active_source = true;
         } else if (Source != active_source) {
            auto nominated_source = ice_responder.nominated();
            if (not nominated_source or *nominated_source != Source) return ERR::Okay;
            active_source = Source;
         }
         if (dtls.state() IS DtlsState::NEW) {
            auto error = dtls.start();
            if (error != ERR::Okay) return error;
         }
         return dtls.feed(Datagram);
      default: return ERR::Okay;
   }
}

ERR IceDtlsTransport::handle_timeout() {
   return dtls.handle_timeout();
}

int64_t IceDtlsTransport::timeout_us() const {
   return dtls.timeout_us();
}

FullIceDtlsTransport::FullIceDtlsTransport(std::shared_ptr<DtlsCertificate> Certificate,
   std::string_view LocalUfrag, std::string_view LocalPassword, ice::Role IceRole, DtlsRole DtlsRole,
   IceDtlsCallbacks Callbacks) :
   agent(LocalUfrag, LocalPassword, IceRole), callbacks(std::move(Callbacks)),
   dtls(std::move(Certificate), DtlsRole, DtlsCallbacks{
      [this](std::span<const uint8_t> Datagram) {
         if (not has_active_source) return;
         if (callbacks.send_from) callbacks.send_from(Datagram, active_local, active_source);
         else if (callbacks.send) callbacks.send(Datagram, active_source);
      },
      [this](std::span<const uint8_t> Data) { if (callbacks.receive) callbacks.receive(Data); },
      [this](DtlsState State) { if (callbacks.state) callbacks.state(State); }
   }) { }

ERR FullIceDtlsTransport::set_remote_fingerprint(std::string_view Fingerprint) {
   return dtls.set_remote_fingerprint("sha-256", Fingerprint);
}

ERR FullIceDtlsTransport::set_dtls_role(DtlsRole Role) {
   return dtls.set_role(Role);
}

bool FullIceDtlsTransport::set_remote_credentials(std::string_view Ufrag, std::string_view Password, bool Lite) {
   return agent.set_remote_credentials(Ufrag, Password, Lite);
}

bool FullIceDtlsTransport::add_local_candidate(const ice::Candidate &Value) {
   return agent.add_local_candidate(Value);
}

void FullIceDtlsTransport::retire_local_route(const ice::Endpoint &Local, bool Recover) {
   agent.retire_local_route(Local, Recover);
   if (has_active_source and active_local IS Local) has_active_source = false;
}

bool FullIceDtlsTransport::add_remote_candidate(const ice::Candidate &Value) {
   return agent.add_remote_candidate(Value);
}

void FullIceDtlsTransport::end_remote_candidates() {
   agent.end_remote_candidates();
}

void FullIceDtlsTransport::dispatch(std::vector<ice::Outbound> &Output) {
   for (const auto &packet : Output) {
      if (callbacks.send_from) callbacks.send_from(packet.datagram, packet.local, packet.remote);
      else if (callbacks.send) callbacks.send(packet.datagram, packet.remote);
   }
}

ERR FullIceDtlsTransport::maybe_start_dtls() {
   auto selected = agent.selected_remote();
   if (not selected) return ERR::Okay;
   active_source = *selected;
   if (auto local = agent.selected_local()) active_local = *local;
   has_active_source = true;
   if (dtls.role() IS DtlsRole::CLIENT and dtls.state() IS DtlsState::NEW) return dtls.start();
   return ERR::Okay;
}

bool FullIceDtlsTransport::start_ice(uint64_t NowUs) {
   std::vector<ice::Outbound> output;
   auto started = agent.start(NowUs, output);
   dispatch(output);
   return started;
}

void FullIceDtlsTransport::restart_ice(std::string_view LocalUfrag, std::string_view LocalPassword,
   ice::Role NewRole) {
   agent.restart(LocalUfrag, LocalPassword, NewRole);
   active_source = {};
   active_local = {};
   has_active_source = false;
}

ERR FullIceDtlsTransport::feed(std::span<const uint8_t> Datagram, const ice::Endpoint &Local,
   const ice::Endpoint &Source, uint64_t NowUs) {
   input_accepted = false;
   if (Datagram.empty()) return ERR::Okay;
   switch (classify_packet(Datagram[0])) {
      case PacketClass::STUN: {
         std::vector<ice::Outbound> output;
         agent.receive(Datagram, Local, Source, NowUs, output);
         input_accepted = agent.accepted_input();
         dispatch(output);
         return maybe_start_dtls();
      }
      case PacketClass::DTLS: {
         if (not agent.validated(Source, Local)) return ERR::Okay;
         if (auto selected = agent.selected_remote(); selected and *selected != Source) return ERR::Okay;
         if (auto selected = agent.selected_local(); selected and *selected != Local) return ERR::Okay;
         if (has_active_source and active_source != Source) return ERR::Okay;
         active_source = Source;
         active_local = Local;
         has_active_source = true;
         if (dtls.state() IS DtlsState::NEW) {
            auto error = dtls.start();
            if (error != ERR::Okay) return error;
         }
         auto error = dtls.feed(Datagram);
         input_accepted = error IS ERR::Okay;
         return error;
      }
      default: return ERR::Okay;
   }
}

ERR FullIceDtlsTransport::handle_timeout(uint64_t NowUs) {
   std::vector<ice::Outbound> output;
   agent.tick(NowUs, output);
   dispatch(output);
   auto error = maybe_start_dtls();
   if (error != ERR::Okay) return error;
   if (dtls.timeout_us() IS 0) return dtls.handle_timeout();
   return ERR::Okay;
}

int64_t FullIceDtlsTransport::timeout_us(uint64_t NowUs) const {
   auto ice_due = agent.timeout_us(NowUs);
   auto dtls_due = dtls.timeout_us();
   if (ice_due < 0) return dtls_due;
   if (dtls_due < 0) return ice_due;
   return std::min(ice_due, dtls_due);
}

ERR FullIceDtlsTransport::send(std::span<const uint8_t> Data) {
   if (not agent.selected_remote()) return ERR::NotInitialised;
   return dtls.send(Data);
}

}
