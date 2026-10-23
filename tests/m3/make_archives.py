"""Builds the T71 archive fixtures: <out>/pack.zip (deflated) and <out>/pack.lzh (LHA level 0, method
-lh0-, stored), each holding one module as `songs/<name>` plus a text file that no replayer claims.
usage: make_archives.py <module> <out dir>"""
import struct
import sys
import zipfile
import zlib


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


def lzx_entry(name, data, packed, mode, merged):
    nm = name.encode()
    h = bytearray(31)
    h[2:6] = struct.pack("<I", len(data))
    h[6:10] = struct.pack("<I", len(packed))
    h[11] = mode
    h[12] = 1 if merged else 0
    h[22:26] = struct.pack("<I", zlib.crc32(data))
    h[30] = len(nm)
    h[26:30] = struct.pack("<I", zlib.crc32(bytes(h) + nm))
    return bytes(h) + nm + packed


def lzx_literal_stream(payload):
    """One verbatim LZX block that codes every byte as a literal (all 256 literal codes are 8 bits)."""
    bits = []

    def field(v, n):  # least significant bit first
        bits.extend((v >> i) & 1 for i in range(n))

    def code(v, n):  # Huffman code, most significant bit first
        bits.extend((v >> i) & 1 for i in range(n - 1, -1, -1))

    field(2, 3)
    n = len(payload)
    field((n >> 16) & 255, 8)
    field((n >> 8) & 255, 8)
    field(n & 255, 8)
    for sym, cnt in ((9, 256), (0, 512)):  # pretree symbols 0 and 9, one bit each
        for i in range(20):
            field(1 if i in (0, 9) else 0, 4)
        for _ in range(cnt):
            code(1 if sym == 9 else 0, 1)
    for b in payload:
        code(b, 8)
    while len(bits) % 16:
        bits.append(0)
    out = bytearray()
    for i in range(0, len(bits), 16):
        w = sum(bit << k for k, bit in enumerate(bits[i : i + 16]))
        out += bytes((w >> 8, w & 255))
    return bytes(out)


def pp20_literals(data):
    """PowerPacker file whose stream is one literal run: valid PP20, not smaller than the input."""
    s = [0]
    k, r = divmod(len(data) - 1, 3)
    for f in [3] * k + [r]:
        s.extend((f >> 1) & 1 for _ in range(1))
        s.append(f & 1)
    for b in reversed(data):
        s.extend((b >> i) & 1 for i in range(7, -1, -1))
    skip = (-len(s)) % 8
    allbits = [0] * skip + s
    chunks = [sum(allbits[j + i] << i for i in range(8)) for j in range(0, len(allbits), 8)]
    return b"PP20" + bytes([9, 9, 9, 9]) + bytes(reversed(chunks)) + len(data).to_bytes(3, "big") + bytes([skip])


def main():
    mod = open(sys.argv[1], "rb").read()
    out = sys.argv[2]
    readme = b"not a module\n"
    head = b"LZX" + bytes(7)
    with open(out + "/pack.lzx", "wb") as f:  # stored entries (pack mode 0)
        f.write(head)
        f.write(lzx_entry("songs/instore.mod", mod, mod, 0, False))
        f.write(lzx_entry("readme.txt", readme, readme, 0, False))
    with open(out + "/packc.lzx", "wb") as f:  # one merged group, one LZX stream (pack mode 2)
        f.write(head)
        f.write(lzx_entry("songs/inlzx.mod", mod, b"", 2, True))
        f.write(lzx_entry("readme.txt", readme, lzx_literal_stream(mod + readme), 2, False))
    with open(out + "/song.pp", "wb") as f:
        f.write(pp20_literals(mod))
    with zipfile.ZipFile(out + "/pack.zip", "w", zipfile.ZIP_DEFLATED) as z:
        z.writestr("songs/inzip.mod", mod)
        z.writestr("readme.txt", "not a module\n")
    with open(out + "/pack.lzh", "wb") as f:
        f.write(lha_member("songs/inlha.mod", mod))
        f.write(lha_member("readme.txt", b"not a module\n"))
        f.write(b"\x00")


main()
