#!/usr/bin/env python3
"""Writes tests/m3/fixtures/xmp_test.mgt: a minimal Megatracker (MGT, Atari Falcon) module for the xmp-decode test (I697).
Layout follows libxmp's mgt_load.c: 4 channels, one 64-row pattern, one track that plays a rising note every 4 rows on
channel 0, one looping 256-byte saw sample. About 8 s at speed 6, 125 BPM. No third-party content; MIT, Selecta's own."""
import struct
import sys

CHN, ROWS, SMP = 4, 64, 256
w16 = lambda v: struct.pack(">H", v)
w32 = lambda v: struct.pack(">I", v)

track = w16(ROWS)
notes = [0x30, 0x34, 0x37, 0x3C]
for i in range(16):
    track += bytes([0x0C | (0x03 if i else 0x00), notes[i % 4] + (i // 4) * 0, 1])
track += bytes([0x02])  # empty event, skips to the end of the 64 rows

hdr_len = 58
song_ptr = hdr_len
seq_ptr = song_ptr + 32 + 4 + 2 + 2 + 1 + 1 + 2 + 1 + 1 + CHN * 2
ins_ptr = seq_ptr + 2
pat_ptr = ins_ptr + 80
trk_ptr = pat_ptr + 2 + CHN * 2
trk0 = trk_ptr + 2 * 4
smp_ptr = trk0 + len(track)

head = b"MGT" + bytes([0x10]) + b"\xbdMCS"
head += w16(CHN) + w16(1) + w16(1) + w16(1) + w16(2) + w16(1) + w16(0) + w32(0)
head += w32(song_ptr) + w32(seq_ptr) + w32(ins_ptr) + w32(pat_ptr) + w32(trk_ptr) + w32(smp_ptr) + w32(SMP) + w32(0)
song = b"selecta xmp test".ljust(32, b"\0") + w32(seq_ptr) + w16(1) + w16(0) + bytes([125, 6]) + w16(0x400) + b"\x80\x80"
song += b"".join(w16(0x80) for _ in range(CHN))
seq = w16(0)
ins = b"saw".ljust(32, b"\0") + w32(smp_ptr) + w32(SMP) + w32(0) + w32(SMP) + w32(0) + w32(0) + w32(8363)
ins += w16(0x400) + b"\x80\x80" + bytes([0x01, 0]) + bytes(4) + w16(0) + w32(0) + w32(0)
pat = w16(ROWS) + w16(2) + w16(1) * (CHN - 1)
trk_tab = w32(0) + w32(trk0)
smp = bytes(((i * 256 // SMP) - 128) & 0xFF for i in range(SMP))
data = head + song + seq + ins + pat + trk_tab + track + smp
assert len(head) == hdr_len and len(song) == seq_ptr - song_ptr and len(ins) == 80
open(sys.argv[1], "wb").write(data)
