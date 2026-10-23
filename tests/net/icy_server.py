"""ICY (SHOUTcast) mock stream for I196a/I196c. argv: portfile, mp3 file, lifetime-seconds.
Every request is logged in <portfile>.requests as "icy=1|0" (was `Icy-MetaData: 1` sent). A client that sent it gets
`icy-metaint: 512` and, after every 512 audio bytes, a metadata block: chunk 1 -> StreamTitle 'Artist - First Song',
chunk 8 -> 'Artist - Second Song', chunk 12 -> the same title again (no new song), every other chunk -> an empty block
(length byte 0). 21 chunks, then the connection closes. A client that did not send the header gets the plain audio.
The audio the decoder and the recorder must see (metadata removed) is written to <portfile>.all, and split at the
title change to <portfile>.seg1 and <portfile>.seg2. The title changes after chunk 8 (audio byte 4096), mid-frame; the
recorder cuts at the first MPEG frame start at or after it (I196d), so seg1 ends and seg2 starts on a whole frame."""
import socket
import sys
import threading
import time

port_file, path, lifetime = sys.argv[1], sys.argv[2], float(sys.argv[3])
CHUNK, COUNT = 512, 21
with open(path, "rb") as f:
    one = f.read()
audio = (one * (CHUNK * COUNT // len(one) + 1))[: CHUNK * COUNT]
TITLES = {1: "Artist - First Song", 8: "Artist - Second Song", 12: "Artist - Second Song"}


def frame_len(h):
    """Length of the MPEG 1 Layer III frame whose header is h, 0 if h is none (the fixture is MPEG 1 Layer III)."""
    kbps = (0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320)
    if len(h) < 4 or h[0] != 0xFF or (h[1] & 0xFE) != 0xFA:
        return 0
    idx, rate, pad = h[2] >> 4, (h[2] >> 2) & 3, (h[2] >> 1) & 1
    if idx in (0, 15) or rate == 3:
        return 0
    return 144 * kbps[idx] * 1000 // (44100, 48000, 32000)[rate] + pad


def frame_cut(data, at):
    """The first frame start at or after `at`, walking frames from the start as the recorder does."""
    i = 0
    while i < len(data):
        if i >= at and frame_len(data[i:i + 4]):
            return i
        i += frame_len(data[i:i + 4]) or 1
    return len(data)


CUT = frame_cut(audio, 8 * CHUNK)
for name, data in ((".all", audio), (".seg1", audio[:CUT]), (".seg2", audio[CUT:])):
    with open(port_file + name, "wb") as f:
        f.write(data)


def block(n):
    if n not in TITLES:
        return b"\x00"
    text = ("StreamTitle='%s';" % TITLES[n]).encode()
    pad = (-len(text)) % 16
    return bytes([(len(text) + pad) // 16]) + text + b"\x00" * pad


srv = socket.socket()
srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
srv.bind(("127.0.0.1", 0))
srv.listen(8)
with open(port_file, "w") as f:
    f.write(str(srv.getsockname()[1]))


def serve(c):
    req = c.recv(4096).decode("latin-1").lower()
    icy = "icy-metadata: 1" in req
    with open(port_file + ".requests", "a") as rq:
        rq.write("icy=%d\n" % (1 if icy else 0))
    if icy:
        head = "ICY 200 OK\r\nicy-name: Test\r\nicy-metaint: %d\r\nContent-Type: audio/mpeg\r\nConnection: close\r\n\r\n" % CHUNK
        body = b"".join(audio[i * CHUNK:(i + 1) * CHUNK] + block(i + 1) for i in range(COUNT))
    else:
        head = "HTTP/1.1 200 OK\r\nContent-Type: audio/mpeg\r\nConnection: close\r\n\r\n"
        body = audio
    c.sendall(head.encode() + body)
    time.sleep(0.2)
    c.close()


def accept():
    while True:
        c, _ = srv.accept()
        threading.Thread(target=serve, args=(c,), daemon=True).start()


threading.Thread(target=accept, daemon=True).start()
time.sleep(lifetime)
