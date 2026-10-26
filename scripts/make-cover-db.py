#!/usr/bin/env python3
"""Build a library.db whose albums carry large synthetic cover art, to judge cover banding on an INDEX8 canvas (ledger N5).

The covers are smooth generated gradients (no copyrighted artwork, no audio): a worst case for a 64-entry tile
palette. The songs and albums are tests/dui/fixture_library.sql; its 4x4 covers are replaced by 256x256 gradients.

  scripts/make-cover-db.py OUT.db [--selecta build/host/apps/selecta/selecta]

Then copy OUT.db to the target and run Selecta with --db OUT.db --format index8 --view home
--exec 'QUEUE_ADD "fixture/1.mod"' (and 2.mod, 3.xm): the queue holds songs whose albums have covers, so
quant_calls rises above 0 and the tile row is quantised.
"""
import os
import sqlite3
import struct
import subprocess
import sys
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
FIXTURE = os.path.join(HERE, "..", "tests", "dui", "fixture_library.sql")
SIDE = 256


def png(rgb_rows):
    raw = b"".join(b"\x00" + bytes(r) for r in rgb_rows)

    def chunk(tag, data):
        body = tag + data
        return struct.pack(">I", len(data)) + body + struct.pack(">I", zlib.crc32(body) & 0xFFFFFFFF)

    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", SIDE, SIDE, 8, 2, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b""))


def gradient(c0, c1, c2):
    """Diagonal blend c0 -> c1 with a radial lift towards c2: many distinct shades, no flat areas."""
    rows = []
    for y in range(SIDE):
        row = []
        for x in range(SIDE):
            t = (x + y) / (2.0 * (SIDE - 1))
            d = min(1.0, (((x - SIDE / 2) ** 2 + (y - SIDE / 2) ** 2) ** 0.5) / (SIDE / 2))
            for k in range(3):
                base = c0[k] + (c1[k] - c0[k]) * t
                row.append(int(base + (c2[k] - base) * (1.0 - d) * 0.6))
        rows.append(row)
    return png(rows)


def main(argv):
    if len(argv) < 2:
        sys.exit(__doc__)
    out = argv[1]
    selecta = argv[argv.index("--selecta") + 1] if "--selecta" in argv else os.path.join(HERE, "..", "build", "host", "apps", "selecta", "selecta")
    if os.path.exists(out):
        os.remove(out)
    subprocess.run([selecta, "--db", out, "--ui", "desktop", "--backend", "mem", "--view", "home", "--canvas", "640x400",
                    "--clock", "fixed", "--stats"], check=True, stdout=subprocess.DEVNULL)
    db = sqlite3.connect(out)
    db.execute("PRAGMA trusted_schema=ON")
    with open(FIXTURE, encoding="utf-8") as f:
        db.executescript(f.read())
    covers = {1: gradient((200, 40, 20), (20, 60, 200), (255, 230, 120)),
              2: gradient((10, 120, 60), (230, 200, 40), (255, 255, 255)),
              3: gradient((90, 20, 140), (240, 110, 60), (60, 220, 230))}
    for album, blob in covers.items():
        db.execute("UPDATE artwork SET data = ? WHERE album_id = ? AND kind = 'cover'", (blob, album))
    db.commit()
    db.close()


if __name__ == "__main__":
    main(sys.argv)
