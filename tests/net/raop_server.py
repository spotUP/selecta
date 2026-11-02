"""I892c2/I892c3: fixture RAOP (AirPlay 1) receiver and test driver, standard library only.

    raop_server.py <raop_stream_test exe> <work dir> [drop=N]

Writes a known PCM block (20 packets of 352 stereo frames) to <work dir>/sent.pcm, listens for RTSP on an ephemeral TCP
port and for RTP on an ephemeral UDP port, starts the sender executable against it, then checks: the RTSP methods came in
order (OPTIONS ANNOUNCE SETUP RECORD SET_PARAMETER FLUSH TEARDOWN), the RTP header fields (payload type 0x60, marker on
the first packet, sequence +1, timestamp +352, one ssrc), the volume parameter, and that the decrypted, unpacked PCM has
the same md5 as the PCM sent. Prints `raop_stream_ok` on success.

I892c3: the fixture also checks the first sync packet on the control port (extension bit, type 0xD4, stream time = first
RTP timestamp), answers nothing itself on the timing port but sends one timing request and expects the NTP-style reply
with its send time echoed. With `drop=N` the fixture discards the Nth audio packet on arrival, asks for it on the
sender's control port when the next packet comes (type 0xD5), and takes the answer (type 0xD6 + original packet) from its
control socket; md5 and `raop_resends=1` must still hold.

Decrypt path: rsaaeskey -> RSA-OAEP(SHA-1) with the AirPort Express private key (public source, shairport-sync common.c
`super_secret_key`; test file only, it pairs with the modulus in plugins/shared/raop/raop.c) done with pow(); AES-128-CBC
per packet through `openssl enc` (Python has no AES in its standard library). Whole 16-byte blocks only, tail clear."""
import base64
import hashlib
import random
import re
import select
import socket
import struct
import subprocess
import sys
import threading
import time

AIRPORT_KEY_PEM = """MIIEpQIBAAKCAQEA59dE8qLieItsH1WgjrcFRKj6eUWqi+bGLOX1HL3U3GhC/j0Qg90u3sG/1CUt
wC5vOYvfDmFI6oSFXi5ELabWJmT2dKHzBJKa3k9ok+8t9ucRqMd6DZHJ2YCCLlDRKSKv6kDqnw4U
wPdpOMXziC/AMj3Z/lUVX1G7WSHCAWKf1zNS1eLvqr+boEjXuBOitnZ/bDzPHrTOZz0Dew0uowxf
/+sG+NCK3eQJVxqcaJ/vEHKIVd2M+5qL71yJQ+87X6oV3eaYvt3zWZYD6z5vYTcrtij2VZ9Zmni/
UAaHqn9JdsBWLUEpVviYnhimNVvYFZeCXg/IdTQ+x4IRdiXNv5hEewIDAQABAoIBAQDl8Axy9XfW
BLmkzkEiqoSwF0PsmVrPzH9KsnwLGH+QZlvjWd8SWYGN7u1507HvhF5N3drJoVU3O14nDY4TFQAa
LlJ9VM35AApXaLyY1ERrN7u9ALKd2LUwYhM7Km539O4yUFYikE2nIPscEsA5ltpxOgUGCY7b7ez5
NtD6nL1ZKauw7aNXmVAvmJTcuPxWmoktF3gDJKK2wxZuNGcJE0uFQEG4Z3BrWP7yoNuSK3dii2jm
lpPHr0O/KnPQtzI3eguhe0TwUem/eYSdyzMyVx/YpwkzwtYL3sR5k0o9rKQLtvLzfAqdBxBurciz
aaA/L0HIgAmOit1GJA2saMxTVPNhAoGBAPfgv1oeZxgxmotiCcMXFEQEWflzhWYTsXrhUIuz5jFu
a39GLS99ZEErhLdrwj8rDDViRVJ5skOp9zFvlYAHs0xh92ji1E7V/ysnKBfsMrPkk5KSKPrnjndM
oPdevWnVkgJ5jxFuNgxkOLMuG9i53B4yMvDTCRiIPMQ++N2iLDaRAoGBAO9v//mU8eVkQaoANf0Z
oMjW8CN4xwWA2cSEIHkd9AfFkftuv8oyLDCG3ZAf0vrhrrtkrfa7ef+AUb69DNggq4mHQAYBp7L+
k5DKzJrKuO0r+R0YbY9pZD1+/g9dVt91d6LQNepUE/yY2PP5CNoFmjedpLHMOPFdVgqDzDFxU8hL
AoGBANDrr7xAJbqBjHVwIzQ4To9pb4BNeqDndk5Qe7fT3+/H1njGaC0/rXE0Qb7q5ySgnsCb3DvA
cJyRM9SJ7OKlGt0FMSdJD5KG0XPIpAVNwgpXXH5MDJg09KHeh0kXo+QA6viFBi21y340NonnEfdf
54PX4ZGS/Xac1UK+pLkBB+zRAoGAf0AY3H3qKS2lMEI4bzEFoHeK3G895pDaK3TFBVmD7fV0Zhov
17fegFPMwOII8MisYm9ZfT2Z0s5Ro3s5rkt+nvLAdfC/PYPKzTLalpGSwomSNYJcB9HNMlmhkGzc
1JnLYT4iyUyx6pcZBmCd8bD0iwY/FzcgNDaUmbX9+XDvRA0CgYEAkE7pIPlE71qvfJQgoA9em0gI
LAuE4Pu13aKiJnfft7hIjbK+5kyb3TysZvoyDnb3HOKvInK7vXbKuU4ISgxB2bB3HcYzQMGsz1qJ
2gG0N5hvJpzwwhbhXqFKA4zaaSrw622wDniAK5MlIE0tIAKKP4yxNGjoD2QYjhBGuhvkWKY="""

PACKETS, FRAMES = 20, 352
EXPECT_METHODS = ["OPTIONS", "ANNOUNCE", "SETUP", "RECORD", "SET_PARAMETER", "FLUSH", "TEARDOWN"]


def der_ints(der):
    """The INTEGERs of a PKCS#1 RSAPrivateKey: version, n, e, d, ..."""
    def tlv(buf, i):
        tag, ln = buf[i], buf[i + 1]
        i += 2
        if ln & 0x80:
            nb = ln & 0x7F
            ln = int.from_bytes(buf[i:i + nb], "big")
            i += nb
        return tag, i, ln
    tag, i, ln = tlv(der, 0)
    assert tag == 0x30
    out = []
    end = i + ln
    while i < end:
        tag, j, ln = tlv(der, i)
        assert tag == 0x02
        out.append(int.from_bytes(der[j:j + ln], "big"))
        i = j + ln
    return out


KEY = der_ints(base64.b64decode(AIRPORT_KEY_PEM.replace("\n", "")))
N, D = KEY[1], KEY[3]


def mgf1(seed, ln):
    out = b""
    c = 0
    while len(out) < ln:
        out += hashlib.sha1(seed + struct.pack(">I", c)).digest()
        c += 1
    return out[:ln]


def oaep_decrypt(ct):
    k = (N.bit_length() + 7) // 8
    em = pow(int.from_bytes(ct, "big"), D, N).to_bytes(k, "big")
    hl = 20
    masked_seed, masked_db = em[1:1 + hl], em[1 + hl:]
    seed = bytes(a ^ b for a, b in zip(masked_seed, mgf1(masked_db, hl)))
    db = bytes(a ^ b for a, b in zip(masked_db, mgf1(seed, k - hl - 1)))
    assert em[0] == 0 and db[:hl] == hashlib.sha1(b"").digest(), "OAEP check failed"
    return db[db.index(b"\x01", hl) + 1:]


def b64(s):
    return base64.b64decode(s + "=" * (-len(s) % 4))


def aes_cbc_decrypt(key, iv, data):
    return subprocess.run(["openssl", "enc", "-d", "-aes-128-cbc", "-nopad", "-K", key.hex(), "-iv", iv.hex()],
                          input=data, stdout=subprocess.PIPE, check=True).stdout


def alac_unpack(frame):
    """Uncompressed (escape) ALAC frame: 23 header bits, 704 big-endian 16-bit samples, end tag."""
    bits = int.from_bytes(frame, "big")
    total = len(frame) * 8
    assert bits >> (total - 3) == 1, "not an escape frame"
    samples = []
    pos = 23
    for _ in range(FRAMES * 2):
        pos += 16
        samples.append((bits >> (total - pos)) & 0xFFFF)
    return struct.pack("<%dH" % len(samples), *samples)


class Receiver:
    def __init__(self):
        self.methods, self.params = [], {}
        self.key = self.iv = None
        self.packets = {}
        self.order = []
        self.errors = []
        self.tcp = socket.socket()
        self.tcp.bind(("127.0.0.1", 0))
        self.tcp.listen(1)
        self.udp = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.udp.bind(("127.0.0.1", 0))
        self.udp.settimeout(0.2)
        self.udp.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 1 << 20)
        self.control = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.control.bind(("127.0.0.1", 0))
        self.timing = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.timing.bind(("127.0.0.1", 0))
        self.stop = False
        self.drop, self.dropped, self.requested = None, None, False
        self.syncs, self.resent, self.timing_replies = [], 0, []
        self.client_ctl = self.client_tim = 0
        self.timing_ref = bytes(range(1, 9))

    def rtsp(self):
        conn, _ = self.tcp.accept()
        conn.settimeout(10)
        buf = b""
        while True:
            while b"\r\n\r\n" not in buf:
                chunk = conn.recv(4096)
                if not chunk:
                    return
                buf += chunk
            head, buf = buf.split(b"\r\n\r\n", 1)
            lines = head.decode().split("\r\n")
            method = lines[0].split()[0]
            hdr = {l.split(":", 1)[0].lower(): l.split(":", 1)[1].strip() for l in lines[1:] if ":" in l}
            ln = int(hdr.get("content-length", 0))
            while len(buf) < ln:
                buf += conn.recv(4096)
            body, buf = buf[:ln].decode(), buf[ln:]
            self.methods.append(method)
            out = ["RTSP/1.0 200 OK", "CSeq: " + hdr.get("cseq", "0"), "Server: fixture/1"]
            if method == "OPTIONS":
                out.append("Public: ANNOUNCE, SETUP, RECORD, PAUSE, FLUSH, TEARDOWN, OPTIONS, GET_PARAMETER, SET_PARAMETER")
            elif method == "ANNOUNCE":
                self.key = oaep_decrypt(b64(re.search(r"a=rsaaeskey:(\S+)", body).group(1)))
                self.iv = b64(re.search(r"a=aesiv:(\S+)", body).group(1))
                self.fmtp = re.search(r"a=fmtp:(.*)", body).group(1).strip()
            elif method == "SETUP":
                out.append("Transport: RTP/AVP/UDP;unicast;mode=record;server_port=%d;control_port=%d;timing_port=%d"
                           % (self.udp.getsockname()[1], self.control.getsockname()[1], self.timing.getsockname()[1]))
                out.append("Session: 1")
                self.client_ctl = int(re.search(r"control_port=(\d+)", hdr["transport"]).group(1))
                self.client_tim = int(re.search(r"timing_port=(\d+)", hdr["transport"]).group(1))
                if hdr.get("session"):
                    self.errors.append("SETUP carried a Session header")
            elif method == "RECORD":
                out.append("Audio-Latency: 11025")
                self.rtp_info = hdr.get("rtp-info", "")
            elif method == "SET_PARAMETER":
                self.params.update(l.split(": ", 1) for l in body.splitlines() if ": " in l)
            if method in ("RECORD", "SET_PARAMETER", "FLUSH", "TEARDOWN") and hdr.get("session") != "1":
                self.errors.append(method + " without Session: 1")
            conn.sendall(("\r\n".join(out) + "\r\n\r\n").encode())
            if method == "TEARDOWN":
                return

    def audio(self):
        while not self.stop:
            try:
                data, _ = self.udp.recvfrom(4096)
            except socket.timeout:
                continue
            seq = struct.unpack(">H", data[2:4])[0]
            if not self.order and self.dropped is None and not self.timing_replies and self.client_tim:
                req = b"\x80\xd2\x00\x07" + bytes(4) + bytes(16) + self.timing_ref
                self.timing.sendto(req, ("127.0.0.1", self.client_tim))
            if self.drop is not None and self.dropped is None and len(self.order) == self.drop:
                self.dropped = seq  # discarded on arrival; the sender must resend it
                continue
            self.order.append(seq)
            self.packets[seq] = data
            if self.dropped is not None and not self.requested:
                self.requested = True
                self.control.sendto(b"\x80\xd5\x00\x01" + struct.pack(">HH", self.dropped, 1),
                                    ("127.0.0.1", self.client_ctl))

    def aux(self):
        """Control socket (sync packets in, resent packets in) and timing socket (replies in)."""
        while not self.stop:
            r, _, _ = select.select([self.control, self.timing], [], [], 0.1)
            for sock in r:
                data, _ = sock.recvfrom(4096)
                if sock is self.timing and data[1] == 0xD3:
                    self.timing_replies.append(data)
                elif data[1] == 0xD4:
                    self.syncs.append(data)
                elif data[1] == 0xD6:
                    self.packets[struct.unpack(">H", data[6:8])[0]] = data[4:]
                    self.resent += 1


def main():
    exe, work = sys.argv[1], sys.argv[2]
    rnd = random.Random(7)
    pcm = b"".join(struct.pack("<h", rnd.randint(-32768, 32767)) for _ in range(PACKETS * FRAMES * 2))
    with open(work + "/sent.pcm", "wb") as f:
        f.write(pcm)
    rx = Receiver()
    if len(sys.argv) > 3 and sys.argv[3].startswith("drop="):
        rx.drop = int(sys.argv[3][5:])
    t3 = threading.Thread(target=rx.aux, daemon=True)
    t3.start()
    t1 = threading.Thread(target=rx.rtsp, daemon=True)
    t2 = threading.Thread(target=rx.audio, daemon=True)
    t1.start()
    t2.start()
    p = subprocess.run([exe, str(rx.tcp.getsockname()[1]), work + "/sent.pcm"], stdout=subprocess.PIPE,
                       stderr=subprocess.STDOUT, timeout=60)
    out = p.stdout.decode().strip()
    t1.join(5)
    time.sleep(0.3)
    rx.stop = True
    t2.join(5)
    t3.join(5)
    print(out)
    fail = []
    if p.returncode != 0:
        fail.append("sender exit %d" % p.returncode)
    if rx.methods != EXPECT_METHODS:
        fail.append("methods %r" % rx.methods)
    if rx.params.get("volume") != "-15.000000":
        fail.append("volume %r" % rx.params.get("volume"))
    fail += rx.errors
    seqs = sorted(rx.packets, key=lambda s: (s - rx.order[0]) & 0xFFFF) if rx.order else []
    got, ts0, ssrc0 = b"", None, None
    for i, s in enumerate(seqs):
        d = rx.packets[s]
        if d[0] != 0x80 or (d[1] & 0x7F) != 0x60 or (d[1] >> 7) != (1 if i == 0 else 0):
            fail.append("rtp header byte0/1 wrong at packet %d: %02x %02x" % (i, d[0], d[1]))
        ts, ssrc = struct.unpack(">II", d[4:12])
        ts0 = ts if ts0 is None else ts0
        ssrc0 = ssrc if ssrc0 is None else ssrc0
        if ts != ts0 + i * FRAMES or ssrc != ssrc0 or ((s - seqs[0]) & 0xFFFF) != i:
            fail.append("rtp seq/timestamp/ssrc wrong at packet %d" % i)
        pay = d[12:]
        whole = len(pay) // 16 * 16
        plain = aes_cbc_decrypt(rx.key, rx.iv, pay[:whole]) + pay[whole:]
        got += alac_unpack(plain)
    want, have = hashlib.md5(pcm).hexdigest(), hashlib.md5(got).hexdigest()
    if len(seqs) != PACKETS or want != have:
        fail.append("received %d packets, md5 %s want %s" % (len(seqs), have, want))
    m = re.search(r"raop_packets_sent=(\d+)", out)
    if not m or int(m.group(1)) != PACKETS:
        fail.append("raop_packets_sent missing or not %d" % PACKETS)
    resends = re.search(r"raop_resends=(\d+)", out)
    if not resends or int(resends.group(1)) != (1 if rx.drop is not None else 0):
        fail.append("raop_resends %r with drop %r" % (resends and resends.group(1), rx.drop))
    if rx.drop is not None and (rx.resent != 1 or not rx.requested):
        fail.append("resend answers received: %d" % rx.resent)
    if not rx.syncs or rx.syncs[0][0] != 0x90 or rx.syncs[0][1] != 0xD4 or len(rx.syncs[0]) != 20:
        fail.append("first sync packet wrong: %r" % (rx.syncs[:1],))
    elif struct.unpack(">I", rx.syncs[0][16:20])[0] != ts0 or struct.unpack(">I", rx.syncs[0][4:8])[0] != (ts0 - 11025) & 0xFFFFFFFF:
        fail.append("sync RTP times wrong")
    if len(rx.timing_replies) != 1 or rx.timing_replies[0][8:16] != rx.timing_ref or len(rx.timing_replies[0]) != 32:
        fail.append("timing reply wrong: %d" % len(rx.timing_replies))
    m = re.search(r"latency_ms=(\d+)", out)
    if not m or int(m.group(1)) != 250:
        fail.append("latency_ms %r, want 250 (11025 frames)" % (m and m.group(1)))
    if fail:
        print("raop_stream_FAIL " + "; ".join(fail))
        sys.exit(1)
    print("raop_stream_ok packets=%d md5=%s" % (len(seqs), have))


if __name__ == "__main__":
    main()
