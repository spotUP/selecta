#!/usr/bin/env python3
"""Writes tests/m3/fixtures/xmp_test.med2, .med3 and .med4: minimal Music Editor (MED 1.12, 2.00, 2.10) songs for the
xmp-med-decode test (I1264). Layouts follow libxmp's med2_load.c, med3_load.c and med4_load.c: one empty 64-row pattern,
one order entry, no instruments (these are song files, whose samples are separate files). No third-party content; MIT,
Selecta's own. usage: make_xmp_med_fixtures.py <out-dir>"""
import struct
import sys

w16 = lambda v: struct.pack(">H", v)
w32 = lambda v: struct.pack(">I", v)

# MED2: magic, 40 skipped bytes, 31 x 40-byte names, volumes, loop starts, loop lengths, pattern count, 100 order bytes,
# order length, tempo, flags, sliding, jump mask, 16 rgb bytes, then per pattern a skipped long and 64 rows x 4 x 4 bytes.
med2 = b"MED\x02" + bytes(40) + bytes(31 * 40) + bytes(1 + 31) + bytes(2 + 62) + bytes(2 + 62)
med2 += w16(1) + bytes(100) + w16(1) + w16(33) + w16(0) + w16(0) + w32(0) + bytes(16)
med2 += w32(0) + bytes(64 * 4 * 4) + bytes(0)

# MED3: magic, 32 empty (NUL) names, four zero masks (volumes, loops, loop lengths), pattern count, order length,
# order, tempo, transpose, flags, sliding, jump mask, 16 rgb bytes, two zero masks (midi), then per pattern: tracks,
# a byte whose four *0 bits say all line and effect masks are empty, a packed size of 0; last, the zero instrument mask.
med3 = b"MED\x03" + bytes(32) + w32(0) * 3 + w16(1) + w16(1) + bytes(1) + w16(33) + bytes(1) + bytes(1) + w16(0)
med3 += w32(0) + bytes(16) + w32(0) + w32(0)
med3 += bytes([4, 0x10 | 0x20 | 0x40 | 0x80]) + w16(0) + w32(0)

# MED4: magic, no instrument mask (0 byte), pattern count, order length, order, tempo, transpose, flags (0 = song
# file), speed, 20 skipped bytes, 16 track volumes, master volume, then per pattern: control block size, channels,
# rows - 1, packed size, one control byte (line and effect masks empty for both 32-row halves).
med4 = b"MED\x04" + bytes(1) + w16(1) + w16(1) + bytes(1) + w16(33) + bytes(1) + bytes(1) + w16(6)
med4 += bytes(20) + bytes(16) + bytes(1)
med4 += bytes([5, 4, 63]) + w16(0) + bytes([0x55])

out = sys.argv[1]
for ext, data in (("med2", med2), ("med3", med3), ("med4", med4)):
    open(f"{out}/xmp_test.{ext}", "wb").write(data)
