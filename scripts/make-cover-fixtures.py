#!/usr/bin/env python3
"""Regenerate tests/library/fixtures/cover_tagged.{mp3,flac}: 2 s of a quiet tone with an embedded 200x200 PNG cover
(ledger I425). The cover is synthetic (diagonal three-stop gradient plus hard-edged shapes), so an INDEX8 quantiser
has real work to do; no third-party art. Needs ffmpeg with libmp3lame and flac at generation time only; the generated
files are committed, so tests never run this.

  scripts/make-cover-fixtures.py [OUTDIR]     (default tests/library/fixtures)
"""
import os, struct, subprocess, sys, tempfile, zlib

W = H = 200


def lerp(a, b, t):
    return tuple(int(a[i] + (b[i] - a[i]) * t) for i in range(3))


def pixel(x, y):
    t = (x + y) / (W + H - 2)
    c = lerp((200, 40, 20), (20, 60, 200), t * 2) if t < 0.5 else lerp((20, 60, 200), (255, 230, 120), t * 2 - 1)
    if 20 <= x < 80 and 20 <= y < 80:
        return (250, 250, 250)
    if (x - 140) ** 2 + (y - 60) ** 2 < 30 ** 2:
        return (10, 10, 10)
    if 100 <= x < 180 and 130 <= y < 170 and (x - 100) % 16 < 8:
        return (30, 180, 60)
    if abs(x - y) < 3:
        return (230, 30, 150)
    return c


def png():
    raw = b"".join(b"\0" + bytes(v for x in range(W) for v in pixel(x, y)) for y in range(H))
    def chunk(k, d):
        return struct.pack(">I", len(d)) + k + d + struct.pack(">I", zlib.crc32(k + d))
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", W, H, 8, 2, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b""))


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(__file__), "..", "tests", "library", "fixtures")
    os.makedirs(out, exist_ok=True)
    with tempfile.TemporaryDirectory() as tmp:
        cover = os.path.join(tmp, "cover.png")
        open(cover, "wb").write(png())
        for ext, codec in (("mp3", ["-c:a", "libmp3lame", "-b:a", "32k", "-id3v2_version", "3"]), ("flac", ["-c:a", "flac", "-compression_level", "8"])):
            subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-f", "lavfi", "-i", "sine=frequency=440:duration=2:sample_rate=22050",
                            "-i", cover, "-map", "0:a", "-map", "1:v", *codec, "-c:v", "copy", "-ac", "1",
                            "-metadata", "title=Cover Fixture", "-metadata", "artist=Selecta Test", "-metadata", "album=Cover Fixture",
                            "-metadata:s:v", "title=Album cover", "-disposition:v", "attached_pic",
                            "-map_metadata", "-1" if False else "0", os.path.join(out, "cover_tagged." + ext)], check=True)


main()
