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
and names the stream http://icy.test/live.
I1029: the click on uuid eeee-5555 names http://icy.test/endless instead: a radio stream in a format no replayer streams
(ADTS AAC bytes under an audio/mpeg label, like the real "Retro Lofi Chiptune Radio"), sent at a radio's pace (16 KB/s)
with icy-metaint and no Content-Length until the client hangs up; it logs "endless=1" and "endless_bytes=<n>" at the end."""
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
            url = {"eeee-5555": "http://icy.test/endless", "eeee-6666": "http://icy.test/endless-plain"}.get(uuid, live)
            body = json.dumps({"ok": True, "message": "retrieved station url", "stationuuid": uuid, "url": url}).encode()
        c.sendall(b"HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: %d\r\nConnection: close\r\n\r\n" % len(body) + body)
        time.sleep(0.2)
        c.close()
        return
    if target.startswith(("/advancedsearch.php", "/metadata/")):  # I725d: Internet Archive (recorded-style answers)
        cc = "http://creativecommons.org/licenses/by/4.0/"
        docs = [("ia_cc_one", "Fixture Album", "Artist-One", cc),
                ("ia_pd", "Old Tunes", "Artist-Two", "http://creativecommons.org/publicdomain/zero/1.0/"),
                ("ia_none", "No Licence", "Artist Three", ""),
                ("ia_other", "Other Licence", "Artist Four", "http://example.com/license"),
                ("ia_nomp3", "Flac Only", "Artist Five", cc)]
        ua = [l for l in req.split("\r\n") if l.startswith("user-agent:")]
        with open(port_file + ".requests", "a") as rq:
            rq.write("ia_ua=%s\n" % (ua[0].split(":", 1)[1].strip() if ua else "none"))
        status = "200 OK"
        extra = ""
        if target.startswith("/advancedsearch.php"):
            with open(port_file + ".requests", "a") as rq:
                rq.write("ia_search=1\n")
            body = json.dumps({"responseHeader": {"status": 0}, "response": {"numFound": len(docs), "start": 0, "docs": [
                dict([("identifier", i), ("title", t), ("creator", [a] if i == "ia_pd" else a)] + ([("licenseurl", l)] if l else []))
                for i, t, a, l in docs]}}).encode()
        else:
            ident = target.split("/metadata/")[1].split("?")[0]
            with open(port_file + ".requests") as rf:
                seen = rf.read()
            with open(port_file + ".requests", "a") as rq:
                rq.write("ia_meta=%s\n" % ident)
                if ident == "ia_cc_one" and "ia_429" not in seen:
                    rq.write("ia_429=%f\n" % time.time())
                    status = "429 Too Many Requests"
                    extra = "Retry-After: 1\r\n"
            if status.startswith("200"):
                with open(port_file + ".requests", "a") as rq:
                    rq.write("ia_meta_ok=%s %f\n" % (ident, time.time()))
            files = {"ia_cc_one": [("cover.png", "PNG", ""), ("01 alpha.mp3", "VBR MP3", "Alpha-Song"),
                                   ("02 beta.mp3", "VBR MP3", "Beta-Song"), ("01 alpha.ogg", "Ogg Vorbis", "Alpha-Song")],
                     "ia_pd": [("old/tune_one.mp3", "VBR MP3", "")],
                     "ia_nomp3": [("a.flac", "Flac", "A"), ("a.png", "PNG", "")]}.get(ident, [])
            body = json.dumps({"files": [{"name": n, "format": f, "title": t, "source": "derivative"} for n, f, t in files],
                               "metadata": {"identifier": ident}}).encode() if status.startswith("200") else b"slow down"
        c.sendall(("HTTP/1.1 %s\r\nContent-Type: application/json\r\n%sContent-Length: %d\r\nConnection: close\r\n\r\n"
                   % (status, extra, len(body))).encode() + body)
        time.sleep(0.2)
        c.close()
        return
    if target.startswith(("/pod/", "/api/1.0/search/byterm", "/api/1.0/podcasts/trending")):  # I725f: podcast RSS feed, Podcast Index search, Range audio
        ua = [l for l in req.split("\r\n") if l.startswith("user-agent:")]
        uas = ua[0].split(":", 1)[1].strip() if ua else "none"
        hdrs = {l.split(":", 1)[0]: l.split(":", 1)[1].strip() for l in req.split("\r\n")[1:] if ":" in l}
        method = req.split(" ")[0]
        ctype, status, extra = "application/json", "200 OK", ""
        with open(port_file + ".requests", "a") as rq:
            rq.write("pod_ua=%s\n" % uas)
        if target.startswith("/api/1.0/search/byterm"):
            with open(port_file + ".requests", "a") as rq:
                rq.write("pod_search=%s key=%s date=%s auth=%d\n" % (target.split("q=")[1], hdrs.get("x-auth-key", ""),
                                                                    hdrs.get("x-auth-date", ""), len(hdrs.get("authorization", ""))))
                rq.write("pod_auth=%s\n" % hdrs.get("authorization", ""))
            body = json.dumps({"status": "true", "feeds": [{"id": 1, "title": "Fixture Show", "url": "http://pod.test/pod/feed.xml"}],
                               "count": 1}).encode()
        elif target.startswith("/api/1.0/podcasts/trending"):  # I1072a: Podcast Index trending, one fixture feed
            with open(port_file + ".requests", "a") as rq:
                rq.write("pod_trending key=%s auth=%d\n" % (hdrs.get("x-auth-key", ""), len(hdrs.get("authorization", ""))))
            body = json.dumps({"status": "true", "feeds": [{"id": 2, "title": "Trend Show", "url": "http://pod.test/pod/trend.xml"}],
                               "count": 1}).encode()
        elif target.startswith("/pod/home.html"):  # I1072a: a web page where a feed is expected
            ctype = "text/html"
            body = b"<!DOCTYPE html><html><head><title>Podcast Index</title></head><body>not a feed</body></html>"
        elif target.startswith("/pod/trend.xml"):
            ctype = "application/rss+xml"
            body = (b'<?xml version="1.0"?><rss version="2.0"><channel><title>Trend-Show</title><item><title>Trend One</title>'
                    b'<enclosure url="http://icy.test/pod/tr1.mp3" length="1" type="audio/mpeg"/></item></channel></rss>')
        elif target.startswith("/pod/feed.xml"):
            with open(port_file + ".requests", "a") as rq:
                rq.write("pod_feed=1\n")
            ctype = "application/rss+xml"
            items = [("Episode-One", "Mon, 01 Jan 2024 10:00:00 +0000", "00:00:03", "http://icy.test/pod/ep1.mp3"),
                     ("Fish &amp; Chips", "Tue, 02 Jan 2024 10:00:00 +0000", "180", "http://icy.test/pod/ep2.mp3?x=1&amp;y=2"),
                     ("<![CDATA[Cdata Three]]>", "Wed, 03 Jan 2024 10:00:00 +0000", "1:00", "http://icy.test/pod/ep3.mp3")]
            xml = ('<?xml version="1.0"?><rss version="2.0" xmlns:itunes="http://www.itunes.com/dtds/podcast-1.0.dtd"><channel>'
                   '<title>Fixture-Show</title><itunes:image href="http://icy.test/pod/cover.jpg"/>')
            for t, d, du, u in items:
                xml += ("<item><title>%s</title><pubDate>%s</pubDate><itunes:duration>%s</itunes:duration>"
                        '<enclosure url="%s" length="1" type="audio/mpeg"/></item>' % (t, d, du, u))
            xml += "<item><title>No Enclosure</title></item></channel></rss>"
            body = xml.encode()
        else:
            ctype = "audio/mpeg"
            pod = one * 40
            body = pod
            rng = hdrs.get("range", "")
            if rng.startswith("bytes="):
                frm = int(rng[6:].split("-")[0])
                with open(port_file + ".requests", "a") as rq:
                    rq.write("pod_range=%d\n" % frm)
                status = "206 Partial Content"
                extra = "Content-Range: bytes %d-%d/%d\r\n" % (frm, len(pod) - 1, len(pod))
                body = pod[frm:]
            else:
                with open(port_file + ".requests", "a") as rq:
                    rq.write("pod_full=%s\n" % method)
            extra += "Accept-Ranges: bytes\r\n"
        c.sendall(("HTTP/1.1 %s\r\nContent-Type: %s\r\n%sContent-Length: %d\r\nConnection: close\r\n\r\n"
                   % (status, ctype, extra, len(body))).encode() + (b"" if method == "head" else body))
        time.sleep(0.2)
        c.close()
        return
    if target.startswith("/blocked.mp3"):  # I1072c: ccMixter answers every download 403 text/html
        body = b"<html><body>Forbidden</body></html>"
        with open(port_file + ".requests", "a") as rq:
            rq.write("cc_blocked=1\n")
        c.sendall(b"HTTP/1.1 403 Forbidden\r\nContent-Type: text/html\r\nContent-Length: %d\r\nConnection: close\r\n\r\n" % len(body) + body)
        time.sleep(0.2)
        c.close()
        return
    if target.startswith("/api/query"):  # I725g: the ccMixter query API (recorded-style answer, 4 uploads)
        live = "https://ccmixter.org/live"  # the plugin sends a ccmixter.org download through its base
        ups = [(101, "Alpha-Mix", "artist_one", "https://creativecommons.org/licenses/by-nc/4.0/", [("cover.png", live), ("alpha.mp3", live)]),
               (102, "Beta-Mix", "artist_two", "http://creativecommons.org/licenses/nc-sampling+/1.0/", [("beta.mp3", live)]),
               (103, "Other-Licence", "artist_three", "http://example.com/license", [("c.mp3", "https://ccmixter.org/blocked.mp3")]),
               (104, "Wav-Only", "artist_four", "http://creativecommons.org/licenses/by/3.0/", [("d.wav", live)]),
               (105, "Blocked-Mix", "artist_five", "http://creativecommons.org/licenses/by/3.0/", [("e.mp3", "https://ccmixter.org/blocked.mp3")])]
        ua = [l for l in req.split("\r\n") if l.startswith("user-agent:")]
        want = target.split("ids=")[1].split("&")[0] if "ids=" in target else ""
        with open(port_file + ".requests", "a") as rq:
            rq.write("cc_ua=%s\n" % (ua[0].split(":", 1)[1].strip() if ua else "none"))
            rq.write("cc_ids=%s\n" % want if want else "cc_query=1\n")
        body = json.dumps([{"upload_id": i, "upload_name": t, "upload_extra": {"usertags": "a,b", "edpicks": {"x": {"review": "{ \"q\" }"}}},
                            "user_name": u, "license_url": l, "license_name": "x", "qrank": 12,
                            "files": [{"file_id": 7, "file_name": n, "file_format_info": {"ps": "4:35"}, "download_url": d,
                                       "file_filesize": " (8.77MB)"} for n, d in fs]}
                           for i, t, u, l, fs in ups if not want or str(i) == want]).encode()
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
    if target.startswith("/chunkmod"):  # I1031: a finite chunked download (no Content-Length) labelled audio/x-mod
        with open(port_file + ".requests", "a") as rq:
            rq.write("chunkmod=1\n")
        c.sendall(b"HTTP/1.1 200 OK\r\nContent-Type: audio/x-mod\r\nTransfer-Encoding: chunked\r\nConnection: close\r\n\r\n")
        for i in range(0, len(audio), 1024):
            part = audio[i:i + 1024]
            c.sendall(b"%x\r\n" % len(part) + part + b"\r\n")
        c.sendall(b"0\r\n\r\n")
        time.sleep(0.2)
        c.close()
        return
    if target.startswith("/endless"):  # I1029: an endless radio stream no replayer can stream
        with open(port_file + ".requests", "a") as rq:
            rq.write("endless=1\n" if "plain" not in target else "endless_plain=1\n")
        metaint = 4096
        frame = b"\xff\xf1\x50\x80\x02\x1f\xfc" + bytes(249)  # ADTS AAC header, silent payload: no MP3 sync
        chunk = (frame * (metaint // len(frame) + 1))[:metaint] + (b"\x00" if icy and "plain" not in target else b"")
        plain = "plain" in target  # I1031: no icy-* header at all, no Content-Length: only audio/mpeg marks it a stream
        head = "HTTP/1.1 200 OK\r\nContent-Type: audio/mpeg\r\n" + ("" if plain else "icy-name: Echo\r\nicy-br: 128\r\n")
        if icy and not plain:
            head += "icy-metaint: %d\r\n" % metaint
        sent = 0
        try:
            c.sendall((head + "Connection: close\r\n\r\n").encode() + chunk * 8)  # 32 KB at once: the head arrives fast
            sent = 8 * metaint
            while True:
                time.sleep(0.25)
                c.sendall(chunk)
                sent += metaint
        except OSError:
            pass
        with open(port_file + ".requests", "a") as rq:
            rq.write("endless_bytes=%d\n" % sent)
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
