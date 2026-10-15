// Object-independent message policies and bidirectional data-channel lifecycle.
#pragma once
#include <kotuku/config.h>
#include <kotuku/system/errors.h>
#include <cstdint>
#include <span>
#include <string>
#include <vector>
#include <optional>
#include <map>

namespace rtc {
class SctpAssociation;
struct SctpReliability {
   // Omission means reliable.  Lifetime is milliseconds from successful enqueue, including queue time.
   std::optional<uint32_t> max_retransmits;
   std::optional<uint32_t> max_lifetime_ms;
   [[nodiscard]] ERR validate() const;
   bool partial() const { return max_retransmits.has_value() or max_lifetime_ms.has_value(); }
};
struct SctpChannelOptions {
   std::string label, protocol;
   bool ordered = true, negotiated = false;
   std::optional<uint16_t> id;
   uint16_t priority = 256;
   SctpReliability reliability;
};
enum class SctpChannelState { CONNECTING, OPEN, CLOSING, CLOSED };
enum class SctpChannelEventType { REMOTE_OPEN, OPEN, MESSAGE, ERROR, CLOSE };
struct SctpChannelEvent {
   uint16_t stream;
   SctpChannelEventType type;
   ERR error = ERR::Okay;
   bool text = false;
   std::vector<uint8_t> payload;
   size_t receive_credit = 0;
   SctpChannelOptions options;
};
struct SctpChannel {
   SctpChannelOptions options;
   SctpChannelState state = SctpChannelState::CONNECTING;
   bool local = true;
   uint64_t close_deadline = 0;
   std::vector<SctpChannelEvent> pending;
   size_t pending_bytes = 0;
};
struct SctpResetEvent { uint16_t stream; bool incoming; };

// Construct after DTLS role and SCTP counts are known.  Events are drained outside transport callbacks.
// Step 4 owns pending creation before role selection and translates these events to public objects.
class SctpChannels {
public:
   explicit SctpChannels(SctpAssociation &Association, bool DtlsClient);
   ~SctpChannels();
   SctpChannels(const SctpChannels &) = delete;
   SctpChannels & operator=(const SctpChannels &) = delete;
   [[nodiscard]] ERR open(const SctpChannelOptions &Options, uint64_t NowUs, uint16_t &ID);
   [[nodiscard]] ERR send(uint16_t ID, std::span<const uint8_t> Payload, bool Text, uint64_t NowUs);
   [[nodiscard]] ERR close(uint16_t ID, uint64_t NowUs);
   [[nodiscard]] ERR pump(uint64_t NowUs);
   [[nodiscard]] const SctpChannel * channel(uint16_t ID) const;
   std::vector<SctpChannelEvent> take_events(bool RetainCredit = false);
   [[nodiscard]] std::optional<uint64_t> next_deadline_us() const;
private:
   ERR process(uint16_t ID, uint32_t PPID, bool Unordered, std::vector<uint8_t> Payload,
      uint64_t NowUs, bool &Retained);
   ERR channel_error(uint16_t ID, ERR Error, uint64_t NowUs);
   bool event(SctpChannelEvent Event);
   SctpAssociation &association;
   bool client;
   std::map<uint16_t, SctpChannel> channels;
   std::vector<SctpChannelEvent> events;
   size_t event_bytes = 0, pending_bytes = 0, label_bytes = 0;
};
} // namespace rtc
