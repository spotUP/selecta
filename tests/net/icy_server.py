"""ICY (SHOUTcast) mock stream for I196a/I196c. argv: portfile, mp3 file, lifetime-seconds.
Every request is logged in <portfile>.requests as "icy=1|0" (was `Icy-MetaData: 1` sent). A client that sent it gets
`icy-metaint: 512` and, after every 512 audio bytes, a metadata block: chunk 1 -> StreamTitle 'Artist - First Song',
chunk 8 -> 'Artist - Second Song', chunk 12 -> the same title again (no new song), every other chunk -> an empty block
(length byte 0). 21 chunks, then the connection closes. A client that did not send the header gets the plain audio.
The audio the decoder and the recorder must see (metadata removed) is written to <portfile>.all, and split at the
title change to <portfile>.seg1 and <portfile>.seg2. The title changes after chunk 8 (audio byte 4096), mid-frame; the
recorder cuts at the first MPEG frame start at or after it (I196d), so seg1 ends and seg2 starts on a whole frame.
I725c: it also answers the radio-browser API: /json/stations/search?...codec=mp3|ogg lists stations (the mp3 answer carries
one AAC row the source must skip) and logs "rb_search=<codec>"; /json/url/<uuid> is the click, logs "rb_click=<uuid>"
and names the stream http://icy.test/live."""
import json
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
    target = req.split(" ")[1]
    if target.startswith("/json/"):  # I725c: the radio-browser API
        live = "http://icy.test/live"
        rows = {
            "mp3": [("aaaa-1111", "Alpha-Radio", "MP3"), ("bbbb-2222", "Beta-FM", "MP3"), ("dddd-4444", "Delta-AAC", "AAC")],
            "ogg": [("cccc-3333", "Gamma-Ogg", "OGG")],
        }
        if target.startswith("/json/stations/search"):
            codec = target.split("codec=")[1].split("&")[0]
            with open(port_file + ".requests", "a") as rq:
                rq.write("rb_search=%s\n" % codec)
            body = json.dumps([{"stationuuid": u, "name": n, "url": "http://icy.test/radio.pls", "url_resolved": live,
                                "codec": k, "country": "Sweden", "tags": "demoscene,chip", "bitrate": 128}
                               for u, n, k in rows[codec]]).encode()
        else:
            uuid = target.rsplit("/", 1)[1]
            with open(port_file + ".requests", "a") as rq:
                rq.write("rb_click=%s\n" % uuid)
            body = json.dumps({"ok": True, "message": "retrieved station url", "stationuuid": uuid, "url": live}).encode()
        c.sendall(b"HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: %d\r\nConnection: close\r\n\r\n" % len(body) + body)
        time.sleep(0.2)
        c.close()
        return
    if target.startswith("/v3.0/tracks/"):  # I725e: the Jamendo API (recorded-style answer, 3 tracks, one without audio)
        live = "http://icy.test/live"
        want = target.split("&id=")[1].split("&")[0] if "&id=" in target else ""
        tracks = [("1001", "Alpha-Song", "Artist-One", live), ("1002", "Beta-Song", "Artist-Two", live),
                  ("1003", "No-Audio", "Artist-Three", "")]
        with open(port_file + ".requests", "a") as rq:
            rq.write("jam_play=%s\n" % want if want else "jam_tracks=1\n")
            rq.write("jam_client=%s\n" % target.split("client_id=")[1].split("&")[0])
        body = json.dumps({"headers": {"status": "success", "code": 0, "results_count": len(tracks)},
                           "results": [{"id": i, "name": n, "duration": 180, "artist_id": "9", "artist_name": a,
                                        "album_name": "Fixture", "license_ccurl": "http://creativecommons.org/licenses/by-nc-sa/3.0/",
                                        "position": 1, "audio": u, "shareurl": "https://www.jamendo.com/track/" + i}
                                       for i, n, a, u in tracks if not want or i == want]}).encode()
        c.sendall(b"HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: %d\r\nConnection: close\r\n\r\n" % len(body) + body)
        time.sleep(0.2)
        c.close()
        return
    if req.split(" ")[1].endswith((".pls", ".m3u")):  # I725a: a playlist URL that names the stream below
        with open(port_file + ".requests", "a") as rq:
            rq.write("playlist=1\n")
        if req.split(" ")[1].endswith(".pls"):
            body = b"[playlist]\nNumberOfEntries=2\nFile1=http://icy.test/live\nTitle1=Test\nFile2=http://other.test/x\nVersion=2\n"
        else:
            body = b"#EXTM3U\nhttp://icy.test/live\n"
        c.sendall(b"HTTP/1.1 200 OK\r\nContent-Type: audio/x-scpls\r\nConnection: close\r\n\r\n" + body)
        time.sleep(0.2)
        c.close()
        return
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
