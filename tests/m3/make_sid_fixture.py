#!/usr/bin/env python3
"""Writes tests/m3/fixtures/sid_test.sid: a PSID v2 file whose play routine (50 Hz) keeps voice 1 on a triangle tone.
Own work, no third-party content. usage: make_sid_fixture.py <out.sid>"""
import struct
import sys

code = bytes([
    0x60,                    # $1000 init: RTS
    0xA9, 0x0F, 0x8D, 0x18, 0xD4,  # $1001 play: LDA #$0F, STA $D418 (volume)
    0xA9, 0x00, 0x8D, 0x05, 0xD4,  # attack/decay 0
    0xA9, 0xF0, 0x8D, 0x06, 0xD4,  # sustain 15, release 0
    0xA9, 0x25, 0x8D, 0x00, 0xD4,  # frequency low
    0xA9, 0x1D, 0x8D, 0x01, 0xD4,  # frequency high (about 440 Hz on PAL)
    0xA9, 0x11, 0x8D, 0x04, 0xD4,  # triangle + gate
    0x60,                    # RTS
])


def field(text):
    return text.encode("ascii").ljust(32, b"\0")


header = b"PSID" + struct.pack(">HHHHHHHI", 2, 0x7C, 0x1000, 0x1000, 0x1001, 1, 1, 0)
header += field("Selecta test tone") + field("Selecta") + field("MIT")
header += struct.pack(">HBBBB", 0x0014, 0, 0, 0, 0)  # flags (PAL, 6581), reloc, SID2, SID3
assert len(header) == 0x7C, len(header)
open(sys.argv[1], "wb").write(header + code)
