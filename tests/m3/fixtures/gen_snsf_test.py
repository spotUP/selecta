#!/usr/bin/env python3
"""Generate snsf_test.snsf (row I705a): an original SNSF whose cartridge ROM (32 KiB LoROM, hand-assembled 65816) does the
SPC700 boot handshake, uploads a small SPC700 program to RAM $0200 and starts it; that program sets up S-DSP voice 0 as a
full-volume noise voice, so the output is loud and steady. Released as CC0. usage: gen_snsf_test.py <out.snsf>"""
import struct, sys, zlib


def dsp(reg, val):
    return bytes([0x8F, reg, 0xF2, 0x8F, val, 0xF3])  # mov $F2,#reg ; mov $F3,#val


code_spc = b"".join(dsp(r, v) for r, v in (
    (0x5D, 0x02),  # DIR: sample directory at $0200
    (0x6C, 0x30),  # FLG: unmute, echo writes off, noise rate 16
    (0x0C, 0x7F), (0x1C, 0x7F), (0x2C, 0), (0x3C, 0), (0x4D, 0),  # master volume, echo volume, echo off
    (0x00, 0x7F), (0x01, 0x7F), (0x02, 0x00), (0x03, 0x10), (0x04, 0),  # voice 0: volume, pitch $1000, source 0
    (0x05, 0), (0x07, 0x7F),  # direct gain (ADSR off)
    (0x3D, 0x01), (0x4C, 0x01))) + bytes([0x2F, 0xFE])  # noise on voice 0, key on, then bra self

# Layout at $0200: sample directory (start, loop = $0204), one looping BRR block (header 3 = end + loop), then the program.
spc = bytes([0x04, 0x02, 0x04, 0x02, 0x03]) + bytes(8) + code_spc
EXEC = 0x0200 + 13
N = len(spc)
assert N < 250
rom = bytearray(0x8000)
code = bytearray()


def op(*b):
    code.extend(b)


def abs_(o, a):
    op(o, a & 255, a >> 8)


# 65816 in emulation mode, origin $8000
op(0x78)  # sei
w1 = len(code)
abs_(0xAD, 0x2140); op(0xC9, 0xAA); op(0xD0, (w1 - (len(code) + 2)) & 255)  # wait port0 = $AA
abs_(0xAD, 0x2141); op(0xC9, 0xBB); op(0xD0, (w1 - (len(code) + 2)) & 255)  # wait port1 = $BB
op(0xA9, 0x00); abs_(0x8D, 0x2142); op(0xA9, 0x02); abs_(0x8D, 0x2143)  # destination $0200
op(0xA9, 0x01); abs_(0x8D, 0x2141); op(0xA9, 0xCC); abs_(0x8D, 0x2140)  # transfer, start command $CC
w2 = len(code)
abs_(0xAD, 0x2140); op(0xC9, 0xCC); op(0xD0, (w2 - (len(code) + 2)) & 255)  # wait echo $CC
op(0xA2, 0x00)  # ldx #0
loop = len(code)
data_fix = len(code); abs_(0xBD, 0)  # lda data,x (address patched below)
abs_(0x8D, 0x2141); op(0x8A); abs_(0x8D, 0x2140)  # data byte, then counter
w3 = len(code)
abs_(0xCD, 0x2140); op(0xD0, (w3 - (len(code) + 2)) & 255)  # wait echo = counter
op(0xE8, 0xE0, N, 0xD0, (loop - (len(code) + 2)) & 255) if False else None
op(0xE8); op(0xE0, N); op(0xD0, (loop - (len(code) + 2)) & 255)  # inx ; cpx #N ; bne loop
op(0xA9, EXEC & 255); abs_(0x8D, 0x2142); op(0xA9, EXEC >> 8); abs_(0x8D, 0x2143)  # execute address
op(0xA9, 0x00); abs_(0x8D, 0x2141); op(0xA9, N + 2); abs_(0x8D, 0x2140)  # port1 = 0 means jump
here = len(code)
abs_(0x4C, 0x8000 + here)  # jmp self
data_at = len(code)
code[data_fix + 1] = (0x8000 + data_at) & 255
code[data_fix + 2] = (0x8000 + data_at) >> 8
code.extend(spc)
rom[0:len(code)] = code
rom[0x7FC0:0x7FC0 + 21] = b"SELECTA SNSF TEST    "
rom[0x7FD5] = 0x20  # LoROM, slow
rom[0x7FD7] = 0x05  # 32 KiB
rom[0x7FD9] = 0x01
for vec in (0x7FEA, 0x7FEE, 0x7FFA, 0x7FFE):  # native NMI/IRQ, emulation NMI/IRQ: jump-self handler
    rom[vec:vec + 2] = struct.pack("<H", 0x8000 + here)
rom[0x7FFC:0x7FFE] = struct.pack("<H", 0x8000)  # emulation reset
rom[0x7FDC:0x7FE0] = bytes([0xFF, 0xFF, 0, 0])
total = sum(rom) & 0xFFFF
rom[0x7FDE:0x7FE0] = struct.pack("<H", total)
rom[0x7FDC:0x7FDE] = struct.pack("<H", total ^ 0xFFFF)

exe = struct.pack("<II", 0, len(rom)) + bytes(rom)
comp = zlib.compress(exe, 9)
tags = b"[TAG]utf8=1\nlength=0:06\nfade=0:01\ntitle=Selecta SNSF test\nartist=Selecta\ngame=Selecta fixture\nyear=2026\n"
open(sys.argv[1], "wb").write(b"PSF\x23" + struct.pack("<III", 0, len(comp), zlib.crc32(comp) & 0xFFFFFFFF) + comp + tags)
