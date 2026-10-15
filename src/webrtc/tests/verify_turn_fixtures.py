#!/usr/bin/env python3
"""Generate/check independent RFC 8656 wire fixtures with Python's struct and hashlib (no production codec).

Run with --write to regenerate; normal execution checks committed bytes.
"""
import hashlib
import hmac
from pathlib import Path
import struct
import sys


def attribute(kind, value):
    return struct.pack('!HH', kind, len(value)) + value + bytes((-len(value)) % 4)


def message(kind, serial, attributes, authenticated=False):
    transaction = struct.pack('<I', serial) + bytes(8)
    payload = b''.join(attribute(k, v) for k, v in attributes)
    header = lambda length: struct.pack('!HHI', kind, length, 0x2112A442) + transaction
    if authenticated:
        key = hashlib.md5(b'user:example.org:pass').digest()
        digest = hmac.new(key, header(len(payload) + 24) + payload, hashlib.sha1).digest()
        payload += attribute(0x0008, digest)
    return header(len(payload)) + payload


fixtures = {
    'turn_allocate_401.turn': message(0x0113, 1, [
        (0x0009, bytes([0, 0, 4, 1])), (0x0014, b'example.org'), (0x0015, b'first-nonce')]),
    'turn_allocate_success.turn': message(0x0103, 2, [
        # 192.0.2.2:50000 and 192.0.2.3:40000, XORed with the RFC magic cookie.
        (0x0016, bytes.fromhex('0001e242e112a640')),
        (0x0020, bytes.fromhex('0001bd52e112a641')),
        (0x000d, struct.pack('!I', 600))], authenticated=True),
}
folder = Path(__file__).parent / 'fixtures'
for name, wire in fixtures.items():
    path = folder / name
    if '--write' in sys.argv:
        path.write_bytes(wire)
    else:
        assert path.read_bytes() == wire, f'Fixture mismatch: {name}'
print(f'{len(fixtures)} independent TURN fixtures verified')
