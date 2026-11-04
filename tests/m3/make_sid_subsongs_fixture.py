#!/usr/bin/env python3
"""Writes a PSID v2 file with 3 subsongs. init stores the subsong number (A, 0-based) at $0300; the 50 Hz play routine
sets voice 1 to a triangle tone whose frequency high byte is 0x10 + 0x10 * subsong, so every subsong is a different
pitch. Own work, no third-party content. usage: make_sid_subsongs_fixture.py <out.sid>"""
import struct
import sys

code = bytes([
    0x8D, 0x00, 0x03, 0x60,        # $1000 init: STA $0300, RTS
    0xA9, 0x0F, 0x8D, 0x18, 0xD4,  # $1004 play: LDA #$0F, STA $D418 (volume)
    0xA9, 0x00, 0x8D, 0x05, 0xD4,  # attack/decay 0
    0xA9, 0xF0, 0x8D, 0x06, 0xD4,  # sustain 15, release 0
    0xA9, 0x25, 0x8D, 0x00, 0xD4,  # frequency low
    0xAD, 0x00, 0x03,              # LDA $0300 (subsong)
    0x0A, 0x0A, 0x0A, 0x0A,        # ASL x4: subsong * 16
    0x18, 0x69, 0x10,              # CLC, ADC #$10
    0x8D, 0x01, 0xD4,              # frequency high
    0xA9, 0x11, 0x8D, 0x04, 0xD4,  # triangle + gate
    0x60,                          # RTS
])


def field(text):
    return text.encode("ascii").ljust(32, b"\0")


header = b"PSID" + struct.pack(">HHHHHHHI", 2, 0x7C, 0x1000, 0x1000, 0x1004, 3, 1, 0)
header += field("Selecta subsong tones") + field("Selecta") + field("MIT")
header += struct.pack(">HBBBB", 0x0014, 0, 0, 0, 0)
assert len(header) == 0x7C, len(header)
open(sys.argv[1], "wb").write(header + code)
