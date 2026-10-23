"""Builds the T71 archive fixtures: <out>/pack.zip (deflated) and <out>/pack.lzh (LHA level 0, method
-lh0-, stored), each holding one module as `songs/<name>` plus a text file that no replayer claims.
usage: make_archives.py <module> <out dir>"""
import struct
import sys
import zipfile


def crc16(data):
    crc = 0
    for b in data:
        crc ^= b
        for _ in range(8):
            crc = (crc >> 1) ^ 0xA001 if crc & 1 else crc >> 1
    return crc


def lha_member(name, data):
    nm = name.encode()
    body = b"-lh0-" + struct.pack("<II", len(data), len(data)) + struct.pack("<I", 0x5A210000)
    body += bytes([0x20, 0, len(nm)]) + nm + struct.pack("<H", crc16(data))
    return bytes([len(body), sum(body) & 0xFF]) + body + data


def main():
    mod = open(sys.argv[1], "rb").read()
    out = sys.argv[2]
    with zipfile.ZipFile(out + "/pack.zip", "w", zipfile.ZIP_DEFLATED) as z:
        z.writestr("songs/inzip.mod", mod)
        z.writestr("readme.txt", "not a module\n")
    with open(out + "/pack.lzh", "wb") as f:
        f.write(lha_member("songs/inlha.mod", mod))
        f.write(lha_member("readme.txt", b"not a module\n"))
        f.write(b"\x00")


main()
