#!/usr/bin/env python3
"""Writes tests/m3/fixtures/org_test.org (Org-02 song) and org_bank_test.wdb (synthetic square-wave bank) for the
organya-decode test (row I703). Both files are original works of this project, released as CC0. Usage: gen_org_test.py <outdir>"""
import struct, sys, os
out = sys.argv[1]
TEMPO, BEATS, STEPS, REPEAT_END = 125, 4, 4, 160   # 160 ticks * 125 ms = 20 s, one pass of the loop
def ch(inst, evs):
    return (inst, evs)
mel = [  # (position, pitch, length, volume, pan)
    [(t, p, 3, 200, 6) for t, p in zip(range(0, 160, 4), [36, 40, 43, 48] * 10)],
    [(t, p, 6, 170, 4) for t, p in zip(range(0, 160, 8), [48, 52, 55, 60, 55, 52, 48, 43] * 3)],
    [(t, p, 14, 220, 8) for t, p in zip(range(0, 160, 16), [24, 24, 31, 29] * 3)],
]
perc = [
    [(t, 36, 1, 230, 6) for t in range(0, 160, 8)],        # drum 0 on beats
    [(t, 36, 1, 200, 6) for t in range(4, 160, 8)],        # drum 2 off beats
]
chans = []
for i in range(8):
    chans.append((i * 6 if i < 3 else 0, mel[i] if i < 3 else []))
for i in range(8):
    chans.append(([0, 2][i] if i < 2 else 0, perc[i] if i < 2 else []))
b = b"Org-02" + struct.pack("<HBBII", TEMPO, BEATS, STEPS, 0, REPEAT_END)
for inst, evs in chans:
    b += struct.pack("<HBBH", 1000, inst, 0, len(evs))
for inst, evs in chans:
    if not evs:
        continue
    b += b"".join(struct.pack("<I", e[0]) for e in evs)
    for k in (1, 2, 3, 4):
        b += bytes(e[k] for e in evs)
open(os.path.join(out, "org_test.org"), "wb").write(b)
bank = b"".join(bytes((100 if i < 128 else 156) for i in range(256)) for _ in range(100))
bank += b"".join(struct.pack("<I", 0) for _ in range(42))
open(os.path.join(out, "org_bank_test.wdb"), "wb").write(bank)
