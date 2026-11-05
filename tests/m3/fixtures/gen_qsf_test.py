#!/usr/bin/env python3
"""Generate qsf_test.qsf (row I704): an original QSF with a hand-assembled Z80 program and a 64 sample sine in the QSound
sample ROM. The Z80 program programs QSound voice 0 (looped 64 sample waveform, volume 0x4000) and steps its rate through
four notes forever. Released as CC0. usage: gen_qsf_test.py <out.qsf>"""
import math, struct, sys, zlib

WR = 0x0080  # address of the register write routine
DELAY = 0x00B0  # address of the delay routine


def section(name, offset, data):
    return name + struct.pack("<II", offset, len(data)) + data


def ld_de(v):
    return bytes([0x11, v & 255, v >> 8])


def call(a):
    return bytes([0xCD, a & 255, a >> 8])


def write_reg(reg, val):
    return bytes([0x0E, reg]) + ld_de(val) + call(WR)  # LD C,reg ; LD DE,val ; CALL wr


code = bytearray(0x100)
main = bytes([0x31, 0xFF, 0xCF])  # LD SP,0xCFFF
for reg, val in ((0, 0x8000), (1, 0), (4, 64), (5, 64), (6, 0x4000)):
    main += write_reg(reg, val)
loop_at = len(main)
for rate in (0x1000, 0x1400, 0x1800, 0x2000):
    main += write_reg(2, rate) + call(DELAY)
main += bytes([0xC3, loop_at & 255, loop_at >> 8])  # JP loop
assert len(main) < WR
code[0:len(main)] = main
wr = bytes([0x3A, 0x07, 0xD0, 0xB7, 0xF2, WR & 255, WR >> 8,  # wait until status bit 7 is set
            0x7A, 0x32, 0x00, 0xD0, 0x7B, 0x32, 0x01, 0xD0, 0x79, 0x32, 0x02, 0xD0, 0xC9])
code[WR:WR + len(wr)] = wr
dl = bytes([0x01, 0xFF, 0xFF, 0x0B, 0x78, 0xB1, 0xC2, (DELAY + 3) & 255, (DELAY + 3) >> 8, 0xC9])
code[DELAY:DELAY + len(dl)] = dl

wave = bytes((int(100 * math.sin(2 * math.pi * i / 64)) & 255) for i in range(64))
exe = section(b"KEY", 0, bytes(11)) + section(b"Z80", 0, bytes(code)) + section(b"SMP", 0, wave + bytes(64))
comp = zlib.compress(exe, 9)
tags = b"[TAG]utf8=1\nlength=0:06\nfade=0:01\ntitle=Selecta QSF test\nartist=Selecta\ngame=Selecta fixture\nyear=2026\n"
open(sys.argv[1], "wb").write(b"PSF\x41" + struct.pack("<III", 0, len(comp), zlib.crc32(comp) & 0xFFFFFFFF) + comp + tags)
