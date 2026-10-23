#!/usr/bin/env python3
"""Selecta M3 fixture server (plan 2026-10-25-m3-library.md, D-M3-6, row I141).

Python 3 standard library only. Serves recorded responses from
tests/net/fixtures/<host>/<path> and logs every request.

Net contract (read by src/core/<os>/net.c only, row I152):
  SELECTA_NET_ALLOW=127.0.0.1   comma list of hosts/addresses a connect may reach;
                                anything else fails before any SYN with net_blocked=1.
  SELECTA_NET_MAP=<host>=http://127.0.0.1:<port>[,<host>=...]
                                the client rewrites <host> to the fixture URL
                                and sends the original name in the Host header,
                                which selects the fixtures/<host>/ directory here.
  SELECTA_CA_FILE=<pem>         trust anchor for the TLS listener (test CA).

Routing: Host header (port stripped) picks fixtures/<host>/. The request path
plus query is looked up in fixtures/<host>/routes.json first (exact key
"<path>?<query>" then "<path>"), else the file fixtures/<host>/<path> (a
directory serves index.json, else index.html). Route entry keys:
  file, status, headers{}, redirect (Location), chunked (bool),
  delay_first_byte (s), delay_body (s, sleep between body halves), body (inline str).
Range: single "bytes=a-b" ranges answer 206. Control endpoints (never logged):
  GET /__fixture/log    JSON array of logged requests
  POST /__fixture/reset clears the log
Log line: {n, method, host, path, headers{range,authorization,content-type,
content-length,user-agent}, body_sha256, body_len, status}; a POST also carries body (its first 4096
bytes, latin-1), so a test can check a signed form request (I177 last.fm scrobble).

Commands:
  start --portfile F --pidfile P --logfile L [--tls-port-file T --cert C --key K]
  stop --pidfile P
  selftest
Passive FTP (anonymous): USER PASS TYPE SYST PWD CWD SIZE PASV RETR LIST QUIT, serving
fixtures/<host>/ for a host chosen by --ftp-host (default ftp.modland.com).
"""
import argparse
import hashlib
import http.server
import json
import os
import signal
import socket
import socketserver
import ssl
import sys
import threading
import time
import urllib.parse

HERE = os.path.dirname(os.path.abspath(__file__))
FIXTURES = os.path.join(HERE, "fixtures")
LOG = []
LOG_LOCK = threading.Lock()
LOG_FILE = None
VOTES = {}  # (mkey, token) -> r, the HippoPlayer vote store (row I174)
VOTE_MODE = {"missing": False}  # POST /__fixture/votes_missing makes both vote endpoints answer 404 (T80)
RATINGS = {}  # (token, source, itemKey) -> stars, the DEViLBOX rating store (row I173)
KEEP_HEADERS = ("range", "authorization", "content-type", "content-length", "user-agent")


def record(entry):
    with LOG_LOCK:
        entry["n"] = len(LOG) + 1
        LOG.append(entry)
        if LOG_FILE:
            with open(LOG_FILE, "a") as f:
                f.write(json.dumps(entry, sort_keys=True) + "\n")


def load_routes(host):
    p = os.path.join(FIXTURES, host, "routes.json")
    if os.path.isfile(p):
        with open(p) as f:
            return json.load(f)
    return {}


HOST_ALIAS = {"localhost": "fixture.test"}


def safe_join(host, path):
    root = os.path.realpath(os.path.join(FIXTURES, host))
    full = os.path.realpath(os.path.join(root, path.lstrip("/")))
    if full != root and not full.startswith(root + os.sep):
        return None
    return full


class Handler(http.server.BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"
    server_version = "SelectaFixture/1"

    def log_message(self, *a):
        pass

    def handle_any(self):
        length = int(self.headers.get("Content-Length") or 0)
        body = self.rfile.read(length) if length else b""
        host = (self.headers.get("Host") or "").split(":")[0].lower()
        host = HOST_ALIAS.get(host, host)  # the TLS cert is for localhost; serve the fixture.test tree
        parsed = urllib.parse.urlsplit(self.path)
        if parsed.path == "/__fixture/log":
            return self.send_bytes(200, json.dumps(LOG).encode(), {"Content-Type": "application/json"})
        if parsed.path == "/__fixture/reset":
            with LOG_LOCK:
                LOG.clear()
                RATINGS.clear()
            return self.send_bytes(200, b"{}", {"Content-Type": "application/json"})
        if parsed.path == "/__fixture/votes_missing":
            VOTE_MODE["missing"] = True
            return self.send_bytes(200, b"{}", {"Content-Type": "application/json"})
        hdrs = {k: self.headers[k] for k in KEEP_HEADERS if self.headers.get(k)}
        entry = {"method": self.command, "host": host, "path": self.path, "headers": hdrs,
                 "body_sha256": hashlib.sha256(body).hexdigest(), "body_len": len(body)}
        if self.command in ("POST", "PUT", "DELETE"):
            entry["body"] = body[:4096].decode("latin-1")
        self.entry = entry
        if host == "devilbox.uprough.net" and self.devilbox(parsed, body):
            return
        self.route(host, parsed, entry)

    def json_reply(self, status, obj):
        return self.send_bytes(status, json.dumps(obj, separators=(",", ":")).encode(),
                               {"Content-Type": "application/json"})

    def hip_vote(self, path, q):
        """HippoPlayer votes (row I174): /api/hip/vote?k=<32 hex>&m=<12 hex>&r=0|5 and /api/hip/votes?m=&k=. The reply is
        text "<count> <yours>". 400 "ERR bad" for malformed parameters, 429 "ERR rate" when the same token repeats the
        same vote on the same song, 404 once /__fixture/votes_missing was called."""
        def text(status, line):
            return self.send_bytes(status, line.encode(), {"Content-Type": "text/plain"})
        if VOTE_MODE["missing"]:
            return text(404, "not found\n") or True
        k, m = (q.get("k") or [""])[0], (q.get("m") or [""])[0]
        r = (q.get("r") or [""])[0]
        hexd = "0123456789abcdef"
        if len(k) != 32 or len(m) != 12 or any(c not in hexd for c in k + m):
            return text(400, "ERR bad\n") or True
        rate = False
        with LOG_LOCK:  # send_bytes takes this lock to log, so reply after leaving it
            if path == "/api/hip/vote":
                if r not in ("0", "5"):
                    rate = None
                elif VOTES.get((m, k)) == int(r):
                    rate = True
                else:
                    VOTES[(m, k)] = int(r)
            count = sum(1 for (mm, _), v in VOTES.items() if mm == m and v == 5)
            yours = VOTES.get((m, k), 0)
        if rate is None:
            return text(400, "ERR bad\n") or True
        if rate:
            return text(429, "ERR rate\n") or True
        return text(200, "%d %d\n" % (count, yours)) or True

    def devilbox(self, parsed, body):
        """Stateful account and ratings endpoints of row I173 (like DEViLBOX auth.ts and ratings.ts).
        Login password is "fixture-password". True when the request was answered here."""
        path = parsed.path
        if path in ("/api/hip/vote", "/api/hip/votes") and self.command == "GET":
            return self.hip_vote(path, urllib.parse.parse_qs(parsed.query))
        if path == "/api/auth/login" and self.command == "POST":
            try:
                req = json.loads(body or b"{}")
            except ValueError:
                req = {}
            if not req.get("username") or not req.get("password"):
                self.json_reply(400, {"error": "Username and password required"})
            elif req["password"] != "fixture-password":
                self.json_reply(401, {"error": "Invalid username or password"})
            else:
                self.json_reply(200, {"token": "fixture-jwt-" + req["username"],
                                      "user": {"id": 1, "username": req["username"], "createdAt": 0}})
            return True
        if path == "/api/ratings" and self.command in ("PUT", "DELETE"):
            auth = self.headers.get("Authorization") or ""
            if not auth.startswith("Bearer "):
                self.json_reply(401, {"error": "No token provided"})
                return True
            token = auth[7:]
            if not token.startswith("fixture-jwt-"):
                self.json_reply(403, {"error": "Invalid or expired token"})
                return True
            try:
                req = json.loads(body or b"{}")
            except ValueError:
                req = {}
            source, item, rating = req.get("source"), req.get("itemKey"), req.get("rating")
            if not source or not item or (self.command == "PUT" and rating is None):
                self.json_reply(400, {"error": "source, itemKey, and rating are required"})
            elif source not in ("modland", "hvsc"):
                self.json_reply(400, {"error": "source must be modland or hvsc"})
            elif self.command == "PUT" and (type(rating) is not int or rating < 1 or rating > 5):
                self.json_reply(400, {"error": "rating must be an integer 1-5"})
            else:
                if "slowrate" in item:  # delay knob of T79 (I173c): a push of a "slowrate" key answers after 2 s (below the client first-byte timeout of 3 s)
                    time.sleep(2)
                with LOG_LOCK:
                    if self.command == "PUT":
                        RATINGS[(token, source, item)] = rating
                    else:
                        RATINGS.pop((token, source, item), None)
                    same = [r for (t, s_, i), r in RATINGS.items() if s_ == source and i == item]
                avg = round(sum(same) / len(same), 2) if same else 0
                self.json_reply(200, {"ok": True, "avg": avg, "count": len(same),
                                      "userRating": rating if self.command == "PUT" else None})
            return True
        if path == "/api/ratings/batch" and self.command == "GET":
            q = urllib.parse.parse_qs(parsed.query)
            source, keys_raw = (q.get("source") or [""])[0], (q.get("keys") or [""])[0]
            auth = self.headers.get("Authorization") or ""
            token = auth[7:] if auth.startswith("Bearer ") else None
            if not source or not keys_raw:
                self.json_reply(400, {"error": "source and keys query params are required"})
            elif source not in ("modland", "hvsc"):
                self.json_reply(400, {"error": "source must be modland or hvsc"})
            else:
                keys = [k for k in keys_raw.split(",") if k]
                if len(keys) > 200:
                    self.json_reply(400, {"error": "Maximum 200 keys per batch request"})
                    return True
                out = {}
                with LOG_LOCK:
                    for k in keys:
                        same = [r for (t, s_, i), r in RATINGS.items() if s_ == source and i == k]
                        own = RATINGS.get((token, source, k)) if token else None
                        if same:
                            out[k] = {"avg": round(sum(same) / len(same), 2), "count": len(same)}
                            if own is not None:
                                out[k]["userRating"] = own
                self.json_reply(200, {"ratings": out})
            return True
        return False

    def route(self, host, parsed, entry):
        routes = load_routes(host)
        key_q = parsed.path + ("?" + parsed.query if parsed.query else "")
        route = routes.get(key_q) or routes.get(parsed.path)
        if isinstance(route, list):  # one entry per method: the first whose key_method matches (I188)
            route = next((x for x in route if x.get("key_method", self.command) == self.command), None)
        route = dict(route) if route else {}
        if "key_method" in route and route["key_method"] != self.command:
            route = {}
        if route.get("redirect"):
            return self.send_bytes(route.get("status", 301), b"", {"Location": route["redirect"]})
        data, ctype = None, "application/octet-stream"
        if "body" in route:
            data = route["body"].encode()
        else:
            if self.command not in ("GET", "HEAD"):
                if "status" in route:
                    return self.send_bytes(route["status"], b"", route.get("headers", {}))
                return self.send_bytes(404, b"not found\n", {"Content-Type": "text/plain"})
            fp = safe_join(host, route.get("file", parsed.path))
            if fp and os.path.isdir(fp):
                for idx in ("index.json", "index.html"):
                    if os.path.isfile(os.path.join(fp, idx)):
                        fp = os.path.join(fp, idx)
                        break
            if not fp or not os.path.isfile(fp) or os.path.basename(fp) == "routes.json":
                return self.send_bytes(404, b"not found\n", {"Content-Type": "text/plain"})
            with open(fp, "rb") as f:
                data = f.read()
            ctype = {".json": "application/json", ".txt": "text/plain", ".md5": "text/plain",
                     ".html": "text/html"}.get(os.path.splitext(fp)[1], ctype)
        headers = {"Content-Type": ctype}
        headers.update(route.get("headers", {}))
        status = route.get("status", 200)
        if route.get("delay_first_byte"):
            time.sleep(route["delay_first_byte"])
        rng = self.headers.get("Range")
        if rng and rng.startswith("bytes=") and status == 200:
            a, _, b = rng[6:].partition("-")
            try:
                start = int(a) if a else max(0, len(data) - int(b))
                end = min(int(b), len(data) - 1) if (a and b) else len(data) - 1
            except ValueError:
                start, end = 0, len(data) - 1
            if start >= len(data):
                return self.send_bytes(416, b"", {"Content-Range": "bytes */%d" % len(data)})
            headers["Content-Range"] = "bytes %d-%d/%d" % (start, end, len(data))
            data, status = data[start:end + 1], 206
        headers["Accept-Ranges"] = "bytes"
        if route.get("chunked"):
            return self.send_chunked(status, data, headers, route.get("delay_body", 0))
        if route.get("delay_body"):
            return self.send_bytes(status, data, headers, split_delay=route["delay_body"])
        return self.send_bytes(status, data, headers)

    def send_head(self, status, headers):
        # Log before the first byte leaves, so a client that reads the log
        # right after a response always sees that request.
        if getattr(self, "entry", None) is not None:
            self.entry["status"] = status
            record(self.entry)
            self.entry = None
        self.send_response(status)
        for k, v in headers.items():
            self.send_header(k, v)

    def send_bytes(self, status, data, headers, split_delay=0):
        self.send_head(status, headers)
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        if self.command != "HEAD":
            if split_delay and len(data) > 1:
                half = len(data) // 2
                self.wfile.write(data[:half])
                self.wfile.flush()
                time.sleep(split_delay)
                self.wfile.write(data[half:])
            else:
                self.wfile.write(data)
        return status

    def send_chunked(self, status, data, headers, delay):
        self.send_head(status, headers)
        self.send_header("Transfer-Encoding", "chunked")
        self.end_headers()
        step = max(1, len(data) // 4)
        for i in range(0, len(data), step):
            c = data[i:i + step]
            self.wfile.write(b"%x\r\n" % len(c) + c + b"\r\n")
            self.wfile.flush()
            if delay and i == 0:
                time.sleep(delay)
        self.wfile.write(b"0\r\n\r\n")
        return status

    do_GET = do_HEAD = do_POST = do_PUT = do_DELETE = handle_any


class ThreadedHTTP(socketserver.ThreadingMixIn, http.server.HTTPServer):
    daemon_threads = True
    allow_reuse_address = True


def ftp_session(conn, host):
    """Minimal anonymous passive-mode FTP."""
    f = conn.makefile("rwb", buffering=0)

    def say(s):
        f.write((s + "\r\n").encode())

    say("220 selecta fixture ftp")
    pasv = None
    cwd = "/"
    while True:
        line = f.readline()
        if not line:
            break
        cmd, _, arg = line.decode("latin-1").strip().partition(" ")
        cmd = cmd.upper()
        entry = {"method": "FTP " + cmd, "host": host, "path": arg, "headers": {}, "body_sha256": "", "body_len": 0}
        if cmd == "USER":
            say("331 password please")
        elif cmd == "PASS":
            say("230 logged in")
        elif cmd in ("TYPE", "MODE", "STRU"):
            say("200 ok")
        elif cmd == "SYST":
            say("215 UNIX Type: L8")
        elif cmd == "PWD":
            say('257 "%s"' % cwd)
        elif cmd == "CWD":
            cwd = arg if arg.startswith("/") else cwd.rstrip("/") + "/" + arg
            say("250 ok")
        elif cmd == "SIZE":
            p = safe_join(host, arg if arg.startswith("/") else cwd + "/" + arg)
            if p and os.path.isfile(p):
                say("213 %d" % os.path.getsize(p))
            else:
                say("550 no such file")
        elif cmd == "PASV":
            if pasv:
                pasv.close()
            pasv = socket.socket()
            pasv.bind(("127.0.0.1", 0))
            pasv.listen(1)
            p = pasv.getsockname()[1]
            say("227 Entering Passive Mode (127,0,0,1,%d,%d)" % (p >> 8, p & 255))
        elif cmd in ("RETR", "LIST"):
            if cmd == "RETR":
                p = safe_join(host, arg if arg.startswith("/") else cwd + "/" + arg)
                ok = bool(p and os.path.isfile(p))
                data = open(p, "rb").read() if ok else b""
            else:
                p = safe_join(host, arg or cwd)
                ok = bool(p and os.path.isdir(p))
                data = "".join(n + "\r\n" for n in sorted(os.listdir(p))).encode() if ok else b""
            if not ok or not pasv:
                say("550 failed" if not ok else "425 use PASV first")
            else:
                say("150 opening")
                dc, _ = pasv.accept()
                dc.sendall(data)
                dc.close()
                say("226 done")
            if pasv:
                pasv.close()
                pasv = None
        elif cmd == "QUIT":
            say("221 bye")
            record(entry)
            break
        else:
            say("502 not implemented")
        record(entry)
    conn.close()


def serve_ftp(host, port_file):
    s = socket.socket()
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    s.bind(("127.0.0.1", 0))
    s.listen(8)
    with open(port_file, "w") as f:
        f.write(str(s.getsockname()[1]))

    def loop():
        while True:
            c, _ = s.accept()
            threading.Thread(target=ftp_session, args=(c, host), daemon=True).start()
    threading.Thread(target=loop, daemon=True).start()


def run(args):
    global LOG_FILE
    LOG_FILE = args.logfile
    if LOG_FILE:
        open(LOG_FILE, "w").close()
    httpd = ThreadedHTTP(("127.0.0.1", 0), Handler)
    if args.tls_port_file:
        tls = ThreadedHTTP(("127.0.0.1", 0), Handler)
        ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        ctx.load_cert_chain(args.cert, args.key)
        tls.socket = ctx.wrap_socket(tls.socket, server_side=True)
        with open(args.tls_port_file, "w") as f:
            f.write(str(tls.server_address[1]))
        threading.Thread(target=tls.serve_forever, daemon=True).start()
    if args.ftp_port_file:
        serve_ftp(args.ftp_host, args.ftp_port_file)
    with open(args.portfile, "w") as f:
        f.write(str(httpd.server_address[1]))
    signal.signal(signal.SIGTERM, lambda *a: os._exit(0))
    httpd.serve_forever()


def cmd_start(args):
    # A server left by an earlier start (ctest --repeat reruns the setup test without the cleanup test, a killed
    # run skips it) would lose its pidfile here and leak for good, so stop it first (I215).
    if os.path.exists(args.pidfile):
        cmd_stop(args)
    for p in (args.portfile, args.pidfile):
        if os.path.exists(p):
            os.remove(p)
    pid = os.fork()
    if pid == 0:
        os.setsid()
        sys.stdin.close()
        out = open(os.devnull, "w")
        os.dup2(out.fileno(), 1)
        os.dup2(out.fileno(), 2)
        with open(args.pidfile, "w") as f:
            f.write(str(os.getpid()))
        run(args)
        os._exit(0)
    for _ in range(100):
        if os.path.exists(args.portfile) and os.path.getsize(args.portfile) > 0:
            print("fixture_port=" + open(args.portfile).read())
            return 0
        time.sleep(0.1)
    print("fixture server did not start", file=sys.stderr)
    return 1


def cmd_stop(args):
    if os.path.exists(args.pidfile):
        try:
            os.kill(int(open(args.pidfile).read()), signal.SIGTERM)
        except OSError:
            pass
        os.remove(args.pidfile)
    return 0


def cmd_selftest(_):
    import http.client
    import tempfile
    import ftplib
    d = tempfile.mkdtemp()
    ns = argparse.Namespace(portfile=d + "/p", pidfile=d + "/pid", logfile=d + "/log", tls_port_file=None,
                            cert=None, key=None, ftp_port_file=d + "/ftp", ftp_host="ftp.modland.com")
    assert cmd_start(ns) == 0
    time.sleep(0.3)
    port = int(open(ns.portfile).read())
    fails = []

    def get(host, path, hdrs=None, method="GET"):
        c = http.client.HTTPConnection("127.0.0.1", port, timeout=10)
        c.request(method, path, headers=dict({"Host": host}, **(hdrs or {})))
        r = c.getresponse()
        return r.status, r.read(), r

    def check(name, cond):
        print(("[OK] " if cond else "[FAIL] ") + name)
        if not cond:
            fails.append(name)
    try:
        s, b, _ = get("devilbox.uprough.net", "/api/health")
        check("plain GET serves index.json", s == 200 and b'"status":"ok"' in b)
        s, b, _ = get("hippoplayer.se", "/playlist/LMan_SID_Chip_Club.prg", {"Range": "bytes=0-5"})
        check("Range bytes=0-5 gives 206 and HiPPrg", s == 206 and b == b"HiPPrg")
        s, b, _ = get("devilbox.uprough.net", "/api/nothing")
        check("unknown path is 404", s == 404)
        s, b, _ = get("hippoplayer.se", "/__fixture/log")
        log = json.loads(b)
        check("log has the 3 requests in order %s" % [(e["path"],e["status"]) for e in log], [e["status"] for e in log] == [200, 206, 404])
        check("log records Range header", log[1]["headers"].get("range") == "bytes=0-5")
        # routes.json knobs, through a temporary host directory
        td = os.path.join(FIXTURES, "selftest.invalid")
        os.makedirs(td, exist_ok=True)
        open(os.path.join(td, "data.bin"), "wb").write(bytes(range(256)) * 8)
        json.dump({"/chunk": {"file": "data.bin", "chunked": True},
                   "/hop": {"redirect": "/chunk"},
                   "/slow": {"file": "data.bin", "delay_first_byte": 0.4}}, open(os.path.join(td, "routes.json"), "w"))
        s, b, r = get("selftest.invalid", "/chunk")
        check("chunked body intact", s == 200 and len(b) == 2048 and r.getheader("Transfer-Encoding") == "chunked")
        s, b, r = get("selftest.invalid", "/hop")
        check("redirect gives 301 Location", s == 301 and r.getheader("Location") == "/chunk")
        t = time.time()
        get("selftest.invalid", "/slow")
        check("delay_first_byte honoured", time.time() - t >= 0.4)
        c = http.client.HTTPConnection("127.0.0.1", port, timeout=10)
        c.request("POST", "/x", body=b"abc", headers={"Host": "selftest.invalid"})
        c.getresponse().read()
        s, b, _ = get("x", "/__fixture/log")
        last = json.loads(b)[-1]
        check("POST body hash logged", last["method"] == "POST" and last["body_sha256"] == hashlib.sha256(b"abc").hexdigest())
        ftp = ftplib.FTP()
        ftp.connect("127.0.0.1", int(open(ns.ftp_port_file).read()), timeout=10)
        ftp.login()
        sz = ftp.size("changelog_2026-10-22.txt")
        got = []
        ftp.retrbinary("RETR changelog_2026-10-22.txt", got.append)
        ftp.quit()
        check("passive FTP SIZE and RETR", sz == 171 and sum(map(len, got)) == 171)
    finally:
        cmd_stop(ns)
        for n in ("data.bin", "routes.json"):
            try:
                os.remove(os.path.join(FIXTURES, "selftest.invalid", n))
            except OSError:
                pass
        try:
            os.rmdir(os.path.join(FIXTURES, "selftest.invalid"))
        except OSError:
            pass
    print("selftest %s" % ("FAILED: " + ", ".join(fails) if fails else "passed"))
    return 1 if fails else 0


def main():
    ap = argparse.ArgumentParser()
    sub = ap.add_subparsers(dest="cmd", required=True)
    st = sub.add_parser("start")
    st.add_argument("--portfile", required=True)
    st.add_argument("--pidfile", required=True)
    st.add_argument("--logfile")
    st.add_argument("--tls-port-file")
    st.add_argument("--cert")
    st.add_argument("--key")
    st.add_argument("--ftp-port-file")
    st.add_argument("--ftp-host", default="ftp.modland.com")
    sp = sub.add_parser("stop")
    sp.add_argument("--pidfile", required=True)
    sub.add_parser("selftest")
    a = ap.parse_args()
    sys.exit({"start": cmd_start, "stop": cmd_stop, "selftest": cmd_selftest}[a.cmd](a))


if __name__ == "__main__":
    main()
