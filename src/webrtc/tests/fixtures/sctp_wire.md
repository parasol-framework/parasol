# Initial SCTP and DCEP wire fixtures

Prepared for Phase 2 Step 0, 2026-10-12.  These synthetic packets are independent inputs for the future codec.
No SCTP implementation exists at this stage.  Fixture integrity is checked by:

```bash
python3 src/webrtc/tests/verify_wire_fixtures.py
```

`sctp_wire.json` uses a source port of 5000 and a destination port of 5001 to expose accidental equal-port
assumptions.  Established inbound packets have verification tag 0x11223344; INIT uses zero.  The INIT advertises
32 MiB receive buffering, 512 streams each way and initial TSN 0xfffffffe, without IP address parameters.
DATA uses stream 2, SSN 0, TSN 0xfffffffe and B/E flags.  Text `hi` has PPID 51 and two padding bytes;
empty binary has PPID 57 with a one-byte filler and three padding bytes.  Payload and chunk lengths exclude padding.
COOKIE-ACK is a four-byte control chunk.  The checksum field at bytes 8–11 is zero during CRC calculation;
its computed value is stored least-significant byte first, while other integer fields use network byte order.

CRC known answers include empty input, `123456789`, 32 zero bytes, 32 0xff bytes and the sequence 0–31.
The latter three answers are published in [RFC 3720 Appendix B.4](https://www.rfc-editor.org/rfc/rfc3720#appendix-B.4).
CRC32C uses polynomial 0x1edc6f41 (reflected 0x82f63b78), initial remainder 0xffffffff and final complement.
Packet answers were produced with a reflected bitwise reference and checked using MSB-first long division,
validated against those known answers.  They were not produced by a Kōtuku SCTP encoder.

Negative packets deliberately isolate short/overrun chunk lengths, checksum corruption, association port and
tag mismatches.  All negative cases except checksum corruption have valid CRCs, so future tests reach the intended
validation rule.  `valid` includes the association profile check, not only structural framing.
The current verifier checks CRCs/annotations and positive chunk bounds; future C++ tests must assert actual
rejection and absence of state changes for every negative fixture.

`dcep_wire.json` encodes all six OPEN channel types with priority 256, label `chat` and empty protocol.
Reliable and retransmission-limited examples use a zero reliability parameter, timed examples use 1000 ms.
ACK is 0x02.  Negative messages isolate short fixed headers, label overrun, reserved channel type and invalid UTF-8.
These messages are SCTP user payloads, with no implicit DCEP padding.  Use PPID 50 in future transport tests.

Add browser captures during Step 1 and persist new malformed/fuzz seeds as defects are found.  Keep synthetic
fixture provenance distinct from captured packets and from acceptance results.
