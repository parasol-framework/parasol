"""Verify Step 0 fixture integrity; this is not an SCTP implementation test."""

import json
from pathlib import Path
import struct


def reverse_bits(Value, Width):
   return int(f"{Value:0{Width}b}"[::-1], 2)


def crc32c(Data):
   # MSB-first long division; the future codec can use reflected/table-driven CRC.
   remainder = 0xffffffff
   for byte in Data:
      remainder ^= reverse_bits(byte, 8) << 24
      for _ in range(8):
         top = remainder & 0x80000000
         remainder = (remainder << 1) & 0xffffffff
         if top:
            remainder ^= 0x1edc6f41
   return reverse_bits(remainder, 32) ^ 0xffffffff


def verify_sctp(Fixtures):
   for vector in Fixtures["crc32c"]:
      assert crc32c(bytes.fromhex(vector["hex"])) == int(vector["checksum"], 16)
   for fixture in Fixtures["packets"]:
      wire = bytes.fromhex(fixture["hex"])
      source, destination, tag = struct.unpack("!HHI", wire[:8])
      assert (source, destination, tag) == (
         fixture["source_port"], fixture["destination_port"], fixture["verification_tag"])
      checksum = struct.unpack("<I", wire[8:12])[0]
      correct_crc = checksum == crc32c(wire[:8] + bytes(4) + wire[12:])
      assert correct_crc == (fixture["name"] != "checksum_corruption"), fixture["name"]
      if fixture["valid"]:
         offset = 12
         while offset < len(wire):
            length = struct.unpack("!H", wire[offset + 2:offset + 4])[0]
            assert length >= 4 and offset + length <= len(wire)
            offset += (length + 3) & ~3
         assert offset == len(wire)
   return len(Fixtures["crc32c"]) + len(Fixtures["packets"])


def verify_dcep(Fixtures):
   for fixture in Fixtures["messages"]:
      wire = bytes.fromhex(fixture["hex"])
      valid = wire == b"\x02"
      if len(wire) >= 12 and wire[0] == 3:
         kind, priority, reliability, label_length, protocol_length = struct.unpack("!BHIHH", wire[1:12])
         valid = kind in (0, 1, 2, 128, 129, 130) and len(wire) == 12 + label_length + protocol_length
         if valid:
            try:
               label = wire[12:12 + label_length].decode("utf-8")
               protocol = wire[12 + label_length:].decode("utf-8")
            except UnicodeDecodeError:
               valid = False
         if valid:
            assert priority == 256
            assert (kind, reliability, label, protocol) == (
               fixture["channel_type"], fixture["reliability"], fixture["label"], fixture["protocol"])
      assert valid == fixture["valid"], fixture["name"]
   return len(Fixtures["messages"])


if __name__ == "__main__":
   root = Path(__file__).parent / "fixtures"
   count = verify_sctp(json.loads((root / "sctp_wire.json").read_text()))
   count += verify_dcep(json.loads((root / "dcep_wire.json").read_text()))
   print(f"Verified {count} CRC/wire fixtures; production codec coverage remains pending.")
