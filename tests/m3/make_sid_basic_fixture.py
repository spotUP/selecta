#!/usr/bin/env python3
"""Writes fixtures/sid_basic.sid: an RSID file with the C64 BASIC flag whose tokenised BASIC program starts a triangle
tone on SID voice 1 and then loops. It only makes sound when a BASIC ROM is mapped (MPS-1). Selecta's own, MIT."""
import struct
import sys

POKE, GOTO = 0x97, 0x89


def line(addr, number, body):
    data = body + b"\x00"
    nxt = addr + 4 + len(data)
    return struct.pack("<HH", nxt, number) + data, nxt


def poke(reg, val):
    return bytes([POKE]) + b"%d,%d" % (reg, val)


addr = 0x0801
l10, addr = line(addr, 10, b":".join([poke(54296, 15), poke(54277, 0), poke(54278, 240), poke(54273, 29),
                                      poke(54272, 37), poke(54276, 17)]))
l20, addr = line(addr, 20, bytes([GOTO]) + b"20")
prog = struct.pack("<H", 0x0801) + l10 + l20 + b"\x00\x00"

def field(s):
    return s.encode().ljust(32, b"\x00")

head = b"RSID" + struct.pack(">HHHHHHHI", 2, 0x7C, 0, 0, 0, 1, 1, 0)
head += field("Selecta BASIC tone") + field("Selecta") + field("MIT")
head += struct.pack(">H", 0x0002) + b"\x00" * 4  # flags: C64 BASIC; start page, page length, 2nd/3rd SID
assert len(head) == 0x7C, len(head)
open(sys.argv[1] if len(sys.argv) > 1 else "fixtures/sid_basic.sid", "wb").write(head + prog)
