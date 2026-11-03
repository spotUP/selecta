"""mDNS responder mock for I892e1 (Chromecast discovery). argv: portfile, lifetime-seconds.
Binds a UDP socket on 127.0.0.1 (free port), writes the port to <portfile>, and answers by unicast to the sender (the
legacy-unicast query path of RFC 6762 section 6.7; real multicast is not used, so a CI box without it works).
Behaviour, to exercise the client's follow-up logic:
 - PTR _googlecast._tcp.local: answers with ONLY the PTR record (name-compressed), twice (a duplicate the client must fold).
 - ANY/SRV/TXT for the instance: answers SRV + TXT, with the SRV target name compressed against the instance.
 - A for abc123.local: answers the A record 127.0.0.1.
Every query is logged in <portfile>.requests as "mdns_q=<type>:<name>"."""
import socket
import struct
import sys
import time

port_file, lifetime = sys.argv[1], float(sys.argv[2])
SVC = "_googlecast._tcp.local"
INST = "Chromecast-abc123." + SVC
HOST = "abc123.local"
TXT = [b"id=0123456789abcdef", b"md=Chromecast Audio", b"fn=Living Room", b"ca=4101"]


def enc(name):
    return b"".join(bytes([len(p)]) + p.encode() for p in name.split(".")) + b"\0"


def rr(owner, rtype, rdata):
    return owner + struct.pack(">HHIH", rtype, 0x8001, 120, len(rdata)) + rdata


def parse_q(pkt):
    labels, p = [], 12
    while pkt[p]:
        labels.append(pkt[p + 1:p + 1 + pkt[p]].decode())
        p += 1 + pkt[p]
    return ".".join(labels), struct.unpack(">H", pkt[p + 1:p + 3])[0]


sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind(("127.0.0.1", 0))
sock.settimeout(0.2)
open(port_file, "w").write(str(sock.getsockname()[1]))
log = open(port_file + ".requests", "w")
end = time.time() + lifetime
while time.time() < end:
    try:
        pkt, addr = sock.recvfrom(1500)
    except socket.timeout:
        continue
    if len(pkt) < 17 or pkt[2] & 0x80:
        continue
    name, qtype = parse_q(pkt)
    log.write(f"mdns_q={qtype}:{name}\n")
    log.flush()
    an = []
    if name == SVC and qtype == 12:
        # the instance name is written as label + pointer to the service name at offset 12 (the question's own name)
        an = [rr(b"\xc0\x0c", 12, bytes([len("Chromecast-abc123")]) + b"Chromecast-abc123" + b"\xc0\x0c")] * 2
    elif name == INST and qtype in (255, 33, 16):
        owner = b"\xc0\x0c"  # the question name starts at offset 12 of the query, which is the instance name
        srv = struct.pack(">HHH", 0, 0, 8009) + bytes([6]) + b"abc123" + bytes([5]) + b"local\0"
        an = [rr(owner, 33, srv), rr(owner, 16, b"".join(bytes([len(t)]) + t for t in TXT))]
    elif name == HOST and qtype == 1:
        an = [rr(b"\xc0\x0c", 1, socket.inet_aton("127.0.0.1"))]
    if an:
        q = pkt[12:]  # echo the question so offset 12 stays valid for the compression pointers
        q = q[:q.index(b"\0") + 5]
        sock.sendto(struct.pack(">HHHHHH", 0, 0x8400, 1, len(an), 0, 0) + q + b"".join(an), addr)
