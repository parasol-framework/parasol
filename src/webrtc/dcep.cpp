#include "dcep.h"
#include "sctp_packet.h"

namespace rtc {
ERR SctpReliability::validate() const {
   if (max_retransmits and max_lifetime_ms) return ERR::InvalidValue;
   if ((max_retransmits and *max_retransmits > 65535) or
       (max_lifetime_ms and *max_lifetime_ms > 65535)) return ERR::OutOfRange;
   return ERR::Okay;
}
namespace dcep {
bool utf8(std::span<const uint8_t> Bytes) {
   for (size_t i = 0; i < Bytes.size();) {
      uint32_t value = Bytes[i++], minimum = 0;
      unsigned count = 0;
      if (value < 0x80) continue;
      if (value >= 0xc2 and value <= 0xdf) { value &= 31; count = 1; minimum = 0x80; }
      else if (value >= 0xe0 and value <= 0xef) { value &= 15; count = 2; minimum = 0x800; }
      else if (value >= 0xf0 and value <= 0xf4) { value &= 7; count = 3; minimum = 0x10000; }
      else return false;
      if (count > Bytes.size() - i) return false;
      while (count--) {
         auto byte = Bytes[i++];
         if ((byte & 0xc0) != 0x80) return false;
         value = (value << 6) | (byte & 63);
      }
      if (value < minimum or value > 0x10ffff or (value >= 0xd800 and value <= 0xdfff)) return false;
   }
   return true;
}
ERR validate(const SctpChannelOptions &Options) {
   auto error = Options.reliability.validate();
   if (error != ERR::Okay) return error;
   if (Options.label.size() > 65535 or Options.protocol.size() > 65535 or
       (Options.id and *Options.id IS 65535)) return ERR::OutOfRange;
   if (Options.negotiated and not Options.id) return ERR::InvalidValue;
   if (not utf8({(const uint8_t *)Options.label.data(), Options.label.size()}) or
       not utf8({(const uint8_t *)Options.protocol.data(), Options.protocol.size()})) return ERR::InvalidData;
   return ERR::Okay;
}
ERR encode_open(const SctpChannelOptions &Options, std::vector<uint8_t> &Wire) {
   auto error = validate(Options);
   if (error != ERR::Okay) return error;
   std::vector<uint8_t> result{3, uint8_t((Options.ordered ? 0 : 0x80) |
      (Options.reliability.max_retransmits ? 1 : Options.reliability.max_lifetime_ms ? 2 : 0))};
   sctp::put16(result, Options.priority);
   sctp::put32(result, Options.reliability.max_retransmits.value_or(
      Options.reliability.max_lifetime_ms.value_or(0)));
   sctp::put16(result, uint16_t(Options.label.size())); sctp::put16(result, uint16_t(Options.protocol.size()));
   result.insert(result.end(), Options.label.begin(), Options.label.end());
   result.insert(result.end(), Options.protocol.begin(), Options.protocol.end());
   Wire = std::move(result);
   return ERR::Okay;
}
ERR decode_open(std::span<const uint8_t> Wire, SctpChannelOptions &Options) {
   if (Wire.size() < 12 or Wire[0] != 3 or (Wire[1] & 0x7f) > 2) return ERR::InvalidData;
   auto label = sctp::read16(Wire, 8), protocol = sctp::read16(Wire, 10);
   if (Wire.size() != 12u + size_t(label) + protocol) return ERR::InvalidData;
   SctpChannelOptions result;
   result.ordered = not (Wire[1] & 0x80);
   result.priority = sctp::read16(Wire, 2);
   auto reliability = sctp::read32(Wire, 4);
   if ((Wire[1] & 0x7f) IS 1) result.reliability.max_retransmits = reliability;
   if ((Wire[1] & 0x7f) IS 2) result.reliability.max_lifetime_ms = reliability;
   result.label.assign((const char *)Wire.data() + 12, label);
   result.protocol.assign((const char *)Wire.data() + 12 + label, protocol);
   auto error = validate(result);
   if (error != ERR::Okay) return error;
   Options = std::move(result);
   return ERR::Okay;
}
bool ack(std::span<const uint8_t> Wire) { return Wire.size() IS 1 and Wire[0] IS 2; }
ERR decode_payload(uint32_t PPID, std::vector<uint8_t> &Payload, bool &Text) {
   Text = PPID IS PPID_TEXT or PPID IS PPID_EMPTY_TEXT;
   if (PPID IS PPID_EMPTY_TEXT or PPID IS PPID_EMPTY_BINARY) {
      // The filler byte carries no application data; its value is intentionally ignored.
      if (Payload.size() != 1) return ERR::InvalidData;
      Payload.clear();
   } else if (PPID != PPID_TEXT and PPID != PPID_BINARY) return ERR::NoSupport;
   if (Text and not utf8(Payload)) return ERR::InvalidData;
   return ERR::Okay;
}
} // namespace dcep
} // namespace rtc
