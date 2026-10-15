#include "sctp_packet.h"

#include <algorithm>
#include <array>
#include <utility>

namespace rtc::sctp {

uint16_t read16(std::span<const uint8_t> Data, size_t Offset) {
   return uint16_t((uint16_t(Data[Offset]) << 8) | Data[Offset + 1]);
}
uint32_t read32(std::span<const uint8_t> Data, size_t Offset) {
   return (uint32_t(read16(Data, Offset)) << 16) | read16(Data, Offset + 2);
}
uint64_t read64(std::span<const uint8_t> Data, size_t Offset) {
   return (uint64_t(read32(Data, Offset)) << 32) | read32(Data, Offset + 4);
}
void put16(std::vector<uint8_t> &Data, uint16_t Value) {
   Data.push_back(uint8_t(Value >> 8));
   Data.push_back(uint8_t(Value));
}
void put32(std::vector<uint8_t> &Data, uint32_t Value) {
   put16(Data, uint16_t(Value >> 16));
   put16(Data, uint16_t(Value));
}
void put64(std::vector<uint8_t> &Data, uint64_t Value) {
   put32(Data, uint32_t(Value >> 32));
   put32(Data, uint32_t(Value));
}

namespace {
// Reflected CRC32C (Castagnoli) lookup table, one entry per byte value.
constexpr auto CRC_TABLE = []() {
   std::array<uint32_t, 256> table{};
   for (uint32_t i = 0; i < 256; ++i) {
      auto crc = i;
      for (int bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ ((crc & 1) ? 0x82f63b78u : 0u);
      table[i] = crc;
   }
   return table;
}();

uint32_t update_crc(uint32_t Crc, std::span<const uint8_t> Data) {
   for (auto byte : Data) Crc = (Crc >> 8) ^ CRC_TABLE[(Crc ^ byte) & 0xff];
   return Crc;
}

bool items(std::span<const uint8_t> Data, bool Parameters, std::vector<Item> &Result) {
   std::vector<Item> parsed;
   size_t offset = 0;
   while (offset < Data.size()) {
      if (Data.size() - offset < 4 or parsed.size() >= MAX_ITEMS) return false;
      size_t length = read16(Data, offset + 2);
      if (length < 4 or length > Data.size() - offset) return false;
      auto padded = (length + 3) & ~size_t(3);
      // Final parameter padding may be outside the containing chunk's length.
      if (padded > Data.size() - offset) {
         if (not Parameters or length != Data.size() - offset) return false;
         padded = length;
      }
      Item item;
      item.type = Parameters ? read16(Data, offset) : Data[offset];
      item.flags = Parameters ? 0 : Data[offset + 1];
      item.value = Data.subspan(offset + 4, length - 4);
      item.wire = Data.subspan(offset, length);
      parsed.push_back(item);
      offset += padded;
   }
   Result = std::move(parsed);
   return true;
}
}

uint32_t crc32c(std::span<const uint8_t> Data) {
   return ~update_crc(0xffffffffu, Data);
}
uint32_t packet_crc(std::span<const uint8_t> Data) {
   // The checksum field at bytes 8-11 is computed as zero.
   static constexpr uint8_t ZERO[4] = {};
   auto crc = update_crc(0xffffffffu, Data.first(std::min(Data.size(), size_t(8))));
   if (Data.size() > 8) crc = update_crc(crc, std::span(ZERO, std::min(Data.size() - 8, size_t(4))));
   if (Data.size() > 12) crc = update_crc(crc, Data.subspan(12));
   return ~crc;
}
size_t chunk_count(std::span<const uint8_t> Data) {
   size_t count = 0;
   for (size_t offset = HEADER_SIZE; offset + 4 <= Data.size() and count < MAX_ITEMS; ++count) {
      size_t length = read16(Data, offset + 2);
      if (length < 4) return std::max(count, size_t(1));
      offset += (length + 3) & ~size_t(3);
   }
   return std::max(count, size_t(1));
}
bool parse(std::span<const uint8_t> Data, Packet &Result) {
   if (Data.size() < HEADER_SIZE + 4 or Data.size() > MAX_PACKET_SIZE) return false;
   auto checksum = uint32_t(Data[8]) | (uint32_t(Data[9]) << 8) |
      (uint32_t(Data[10]) << 16) | (uint32_t(Data[11]) << 24);
   if (checksum != packet_crc(Data)) return false;
   Packet parsed;
   parsed.source_port = read16(Data, 0);
   parsed.destination_port = read16(Data, 2);
   parsed.verification_tag = read32(Data, 4);
   if (not parsed.source_port or not parsed.destination_port or not items(Data.subspan(12), false, parsed.chunks)) {
      return false;
   }
   Result = std::move(parsed);
   return true;
}
bool parameters(std::span<const uint8_t> Data, std::vector<Item> &Result) {
   if (Data.size() > MAX_PACKET_SIZE) return false;
   return items(Data, true, Result);
}
bool append_parameter(std::vector<uint8_t> &Data, uint16_t Type, std::span<const uint8_t> Value) {
   if (Value.size() > 65531) return false;
   size_t length = Value.size() + 4;
   size_t padding = (4 - (Data.size() % 4)) % 4;
   if (Data.size() > MAX_PACKET_SIZE or length + padding > MAX_PACKET_SIZE - Data.size()) return false;
   // Pad the preceding parameter, leaving final padding to the containing chunk encoder.
   Data.insert(Data.end(), padding, 0);
   put16(Data, Type);
   put16(Data, uint16_t(length));
   Data.insert(Data.end(), Value.begin(), Value.end());
   return true;
}
Encoder::Encoder(uint16_t Source, uint16_t Destination, uint32_t Tag, size_t Capacity)
   : capacity(std::min(Capacity, MAX_PACKET_SIZE)) {
   if (capacity < HEADER_SIZE + 4 or not Source or not Destination) { capacity = 0; return; }
   put16(bytes, Source);
   put16(bytes, Destination);
   put32(bytes, Tag);
   put32(bytes, 0);
}
bool Encoder::chunk(uint8_t Type, uint8_t Flags, std::span<const uint8_t> Value) {
   if (Value.size() > 65531 or bytes.empty()) return false;
   size_t length = Value.size() + 4;
   size_t padded = (length + 3) & ~size_t(3);
   if (padded > capacity - bytes.size()) return false;
   bytes.push_back(Type);
   bytes.push_back(Flags);
   put16(bytes, uint16_t(length));
   bytes.insert(bytes.end(), Value.begin(), Value.end());
   bytes.insert(bytes.end(), padded - length, 0);
   return true;
}
std::vector<uint8_t> Encoder::finish() {
   if (bytes.size() <= HEADER_SIZE) return {};
   auto crc = packet_crc(bytes);
   for (size_t i = 0; i < 4; ++i) bytes[8 + i] = uint8_t(crc >> (i * 8));
   return std::exchange(bytes, {});
}

} // namespace rtc::sctp
