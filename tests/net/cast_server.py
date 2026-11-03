#!/usr/bin/env python3
"""I892e1: fixture Chromecast receiver. TLS (self-signed certificate made with the openssl CLI) on 127.0.0.1, CASTV2
framing (4-byte big-endian length + protobuf CastMessage), hand-decoded like the sender does.

usage: cast_server.py <portfile> <seconds> <certdir>
Writes the port into <portfile>, one line per received message into <portfile>.requests as
"<namespace> <type>" (CONNECT, PING, PONG, GET_STATUS). Answers:
  CONNECT      -> sends one receiver PING (the sender must answer PONG)
  PING         -> PONG
  GET_STATUS   -> RECEIVER_STATUS with one application and a volume, requestId echoed
I892e2 additions (receiver + media namespaces):
  LAUNCH       -> RECEIVER_STATUS listing the launched appId with transportId web-5
  LOAD         -> fetches contentId over HTTP before answering: a full GET (md5 + length), a HEAD, a Range GET
                  (bytes=10-19) and a GET of a wrong token (must be 404); logs "http full <md5> <len>",
                  "http head <len>", "http range <status> <md5>", "http wrong <status>"; then MEDIA_STATUS
                  (mediaSessionId 1, PLAYING, duration from Content-Length is not known so 100.0)
  PLAY / PAUSE / SEEK / GET_STATUS (media) -> MEDIA_STATUS with the new playerState / currentTime
  SET_VOLUME   -> RECEIVER_STATUS with the new volume
"""
import hashlib
import http.client
import json
import os
import socket
import ssl
import struct
import subprocess
import sys
import time
import urllib.parse

NS_CONN = "urn:x-cast:com.google.cast.tp.connection"
NS_HB = "urn:x-cast:com.google.cast.tp.heartbeat"
NS_RECV = "urn:x-cast:com.google.cast.receiver"
NS_MEDIA = "urn:x-cast:com.google.cast.media"
APP_ID = "CC1AD845"


def app_status(request_id, volume):
    return {"type": "RECEIVER_STATUS", "requestId": request_id,
            "status": {"applications": [{"appId": APP_ID, "displayName": "Default Media Receiver",
                                         "namespaces": [{"name": NS_MEDIA}], "sessionId": "sess-1",
                                         "statusText": "Ready", "transportId": "web-5"}],
                       "isActiveInput": True, "volume": volume}}


def media_status(request_id, state, cur, reason=None):
    entry = {"mediaSessionId": 1, "playerState": state, "currentTime": cur,
             "media": {"contentId": "x", "duration": 100.0}, "playbackRate": 1}
    if reason:
        entry["idleReason"] = reason
    return {"type": "MEDIA_STATUS", "requestId": request_id, "status": [entry]}


def http_exchange(url, log):
    """The receiver's side of the LOAD: fetch the served file the way a Chromecast does."""
    u = urllib.parse.urlparse(url)

    def req(method, path, headers=None):
        c = http.client.HTTPConnection(u.hostname, u.port, timeout=10)
        c.request(method, path, headers=headers or {})
        r = c.getresponse()
        body = r.read()
        c.close()
        return r, body

    r, body = req("GET", u.path)
    log.write("http full %d %s %d\n" % (r.status, hashlib.md5(body).hexdigest(), len(body)))
    r, body = req("HEAD", u.path)
    log.write("http head %d %s\n" % (r.status, r.getheader("Content-Length")))
    r, body = req("GET", u.path, {"Range": "bytes=10-19"})
    log.write("http range %d %s %s\n" % (r.status, hashlib.md5(body).hexdigest(), r.getheader("Content-Range")))
    r, body = req("GET", u.path + "x")
    log.write("http wrong %d\n" % r.status)
    r, body = req("GET", "/etc/passwd")
    log.write("http escape %d\n" % r.status)
    log.flush()


def varint(v):
    out = bytearray()
    while v >= 0x80:
        out.append((v & 0x7F) | 0x80)
        v >>= 7
    out.append(v)
    return bytes(out)


def field(num, s):
    b = s.encode()
    return bytes([num << 3 | 2]) + varint(len(b)) + b


def encode(src, dst, ns, payload):
    body = b"\x08\x00" + field(2, src) + field(3, dst) + field(4, ns) + b"\x28\x00" + field(6, payload)
    return struct.pack(">I", len(body)) + body


def decode(body):
    pos, out = 0, {}
    while pos < len(body):
        tag = 0
        shift = 0
        while True:
            c = body[pos]
            pos += 1
            tag |= (c & 0x7F) << shift
            shift += 7
            if not c & 0x80:
                break
        wt, num = tag & 7, tag >> 3
        if wt == 0:
            v = 0
            shift = 0
            while True:
                c = body[pos]
                pos += 1
                v |= (c & 0x7F) << shift
                shift += 7
                if not c & 0x80:
                    break
            out[num] = v
        elif wt == 2:
            ln = 0
            shift = 0
            while True:
                c = body[pos]
                pos += 1
                ln |= (c & 0x7F) << shift
                shift += 7
                if not c & 0x80:
                    break
            out[num] = body[pos:pos + ln].decode()
            pos += ln
        else:
            raise ValueError("wire type %d" % wt)
    return out


def recv_exact(conn, n):
    buf = b""
    while len(buf) < n:
        chunk = conn.recv(n - len(buf))
        if not chunk:
            return None
        buf += chunk
    return buf


def main():
    portfile, seconds, certdir = sys.argv[1], float(sys.argv[2]), sys.argv[3]
    os.makedirs(certdir, exist_ok=True)
    cert, key = os.path.join(certdir, "cast.pem"), os.path.join(certdir, "cast.key")
    subprocess.run(["openssl", "req", "-x509", "-newkey", "rsa:2048", "-nodes", "-days", "30", "-subj",
                    "/CN=Fixture Chromecast", "-keyout", key, "-out", cert], check=True,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    ctx.load_cert_chain(cert, key)
    srv = socket.socket()
    srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    srv.bind(("127.0.0.1", 0))
    srv.listen(1)
    srv.settimeout(seconds)
    log = open(portfile + ".requests", "w")
    with open(portfile, "w") as f:
        f.write("%d\n" % srv.getsockname()[1])
    deadline = time.time() + seconds
    try:
        raw, _ = srv.accept()
        conn = ctx.wrap_socket(raw, server_side=True)
    except (socket.timeout, ssl.SSLError) as e:
        log.write("tls-failed %s\n" % e)
        return
    conn.settimeout(max(1.0, deadline - time.time()))
    volume = {"level": 0.5, "muted": False}
    state, cur = "IDLE", 0.0
    while time.time() < deadline:
        hdr = recv_exact(conn, 4)
        if hdr is None:
            break
        msg = decode(recv_exact(conn, struct.unpack(">I", hdr)[0]))
        pl = json.loads(msg.get(6, "{}"))
        log.write("%s %s\n" % (msg.get(4), pl.get("type")))
        log.flush()
        src, ns, typ = msg.get(2, ""), msg.get(4, ""), pl.get("type")
        if ns == NS_CONN and typ == "CONNECT":
            conn.sendall(encode("receiver-0", src, NS_HB, json.dumps({"type": "PING"})))
        elif ns == NS_HB and typ == "PING":
            conn.sendall(encode("receiver-0", src, NS_HB, json.dumps({"type": "PONG"})))
        elif ns == NS_RECV and typ == "LAUNCH" and pl.get("appId") == APP_ID:
            conn.sendall(encode("receiver-0", src, NS_RECV, json.dumps(app_status(pl.get("requestId"), volume))))
        elif ns == NS_RECV and typ == "SET_VOLUME":
            volume = {"level": pl["volume"]["level"], "muted": False}
            conn.sendall(encode("receiver-0", src, NS_RECV, json.dumps(app_status(pl.get("requestId"), volume))))
        elif ns == NS_MEDIA and typ in ("LOAD", "PLAY", "PAUSE", "SEEK", "GET_STATUS"):
            if typ == "LOAD":
                http_exchange(pl["media"]["contentId"], log)
                state, cur = "PLAYING", 0.0
            elif typ == "PLAY":
                state = "PLAYING"
            elif typ == "PAUSE":
                state = "PAUSED"
            elif typ == "SEEK":
                cur = float(pl["currentTime"])
            log.write("media %s id=%s state=%s cur=%.1f\n" % (typ, pl.get("mediaSessionId", "-"), state, cur))
            log.flush()
            conn.sendall(encode("web-5", src, NS_MEDIA, json.dumps(media_status(pl.get("requestId"), state, cur))))
        elif ns == NS_RECV and typ == "GET_STATUS":
            st = {"type": "RECEIVER_STATUS", "requestId": pl.get("requestId"),
                  "status": {"applications": [{"appId": "CC1AD845", "displayName": "Default Media Receiver",
                                               "namespaces": [{"name": "urn:x-cast:com.google.cast.media"}],
                                               "sessionId": "sess-1", "statusText": "Ready",
                                               "transportId": "web-5"}],
                             "isActiveInput": True, "volume": {"level": 0.5, "muted": False}}}
            conn.sendall(encode("receiver-0", src, NS_RECV, json.dumps(st)))
    log.close()


if __name__ == "__main__":
    main()
