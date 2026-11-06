"""UPnP MediaServer mock for I894a1. argv: portfile, lifetime-seconds.
Binds a UDP socket (the SSDP responder) and a TCP HTTP server (the device description) on 127.0.0.1, both on free
ports, and writes "<udp port> <http port>" to <portfile>. An M-SEARCH for the MediaServer:1 target gets one unicast
200 response whose LOCATION is the description URL (the real multicast address is not used, so a CI box without
multicast works). Every request is logged in <portfile>.requests as "ssdp_search=<ST>" and "desc_get=<path>".
The description has a friendlyName with an entity, a relative controlURL in a ContentDirectory service that comes
after a ConnectionManager service, and no URLBase: the client must resolve the control path against LOCATION.
I894a2: POST /upnp/cd/control answers ContentDirectory Browse (BrowseDirectChildren) and logs "browse=<id>:<start>:<count>".
Tree: "0" holds containers "1" (Rock & Roll, childCount 151) and "2" (Jazz, childCount 0). "1" holds the
container "1$sub" followed by 150 items "1$001".."1$150" = 151 children; the server never returns more than 100 per
page, so a client has to page. The Result is entity-escaped DIDL-Lite inside the SOAP envelope."""
import http.server
import re
from xml.sax.saxutils import escape
import socket
import socketserver
import sys
import threading
import time

port_file, lifetime = sys.argv[1], float(sys.argv[2])
MEDIA = open(sys.argv[3], "rb").read() if len(sys.argv) > 3 else b""  # I894a3: served for GET /media/*.mp3
# I894a4: argv[4] picks the tree. "" = the 153-song tree below; "synology" = a Synology-like tree where every file is
# listed under Artist, Album, Genre and Folder views (root > Music > ...); "synology-plain" = the same tree with the
# folder view renamed so a client finds no folder container and has to do a full walk.
TREE = sys.argv[4] if len(sys.argv) > 4 else ""
# I894a5: "cd3" = the default tree on a ContentDirectory:3 server (GetServiceResetToken + Search by objectUpdateID); every
# mode answers GetSystemUpdateID. GET /admin/add?c=<container>&n=<num> and /admin/del?c=&n= change the tree, bump
# SystemUpdateID and set that container's objectUpdateID.
CD = 3 if TREE == "cd3" else 1
SYSID = 1
CUPD = {}  # container id -> objectUpdateID
EXTRA = {}  # container id -> item numbers added
GONE = set()  # item ids removed
NAME = "Fixture Media &amp; Music"
DESC = f"""<?xml version="1.0"?>
<root xmlns="urn:schemas-upnp-org:device-1-0">
 <specVersion><major>1</major><minor>0</minor></specVersion>
 <device>
  <deviceType>urn:schemas-upnp-org:device:MediaServer:1</deviceType>
  <friendlyName>{NAME}</friendlyName>
  <UDN>uuid:fixture-0001</UDN>
  <serviceList>
   <service>
    <serviceType>urn:schemas-upnp-org:service:ConnectionManager:1</serviceType>
    <controlURL>/upnp/cm/control</controlURL>
   </service>
   <service>
    <serviceType>urn:schemas-upnp-org:service:ContentDirectory:{CD}</serviceType>
    <serviceId>urn:upnp-org:serviceId:ContentDirectory</serviceId>
    <controlURL>/upnp/cd/control</controlURL>
    <eventSubURL>/upnp/cd/event</eventSubURL>
   </service>
  </serviceList>
 </device>
</root>
""".encode()
log_lock = threading.Lock()


def log(line):
    with log_lock, open(port_file + ".requests", "a") as f:
        f.write(line + "\n")


NS = ('xmlns="urn:schemas-upnp-org:metadata-1-0/DIDL-Lite/" xmlns:dc="http://purl.org/dc/elements/1.1/" '
      'xmlns:upnp="urn:schemas-upnp-org:metadata-1-0/upnp/"')
SERVER_PAGE_CAP = 100


def container(cid, parent, title, count):
    return (f'<container id="{cid}" parentID="{parent}" childCount="{count}" restricted="1">'
            f'<dc:title>{escape(title)}</dc:title><upnp:class>object.container</upnp:class></container>')


def item(n, parent="1"):
    title = f"Song {n:03d} & More" if n == 42 else (f"Track-{n:03d}" if n >= 200 else f"Track {n:03d}")
    album = "Album <A>" if n == 42 else "Album"
    url = f"http://127.0.0.1:{http_port}/media/{n:03d}.mp3?a=1&b=2"
    return (f'<item id="{parent}${n:03d}" parentID="{parent}" restricted="1"><dc:title>{escape(title)}</dc:title>'
            f'<upnp:artist>{"Artist-" if n >= 200 else "Artist "}{n:03d}</upnp:artist><upnp:album>{escape(album)}</upnp:album>'
            f'<upnp:class>object.item.audioItem.musicTrack</upnp:class>'
            f'<res protocolInfo="http-get:*:audio/mpeg:DLNA.ORG_PN=MP3" duration="0:03:25.500" size="1234567">'
            f'{escape(url)}</res></item>')


SYN_FILES = {"d1": range(301, 311), "d2": range(311, 316)}  # folder -> files; 15 unique files, all in the Folder view


def syn_children(oid):
    """I894a4: Artist / Album / Genre views group the same 15 files as the Folder view (different object ids)."""
    folder = "Folder" if TREE == "synology" else "By Path"
    views = [("a", "Artist"), ("b", "Album"), ("g", "Genre"), ("f", folder)]
    allfiles = [n for r in SYN_FILES.values() for n in r]
    if oid == "0":
        return [container("m", "0", "Music", len(views))]
    if oid == "m":
        return [container(v, "m", t, 3) for v, t in views]
    if oid == "f":
        return [container(f"f${d}", "f", f"dir-{d}", len(r)) for d, r in SYN_FILES.items()]
    if oid in ("a", "b", "g"):  # three groups per view, five files each
        return [container(f"{oid}${k}", oid, f"{oid}-group-{k}", 5) for k in range(3)]
    if oid.startswith("f$"):
        return [item(n, oid) for n in SYN_FILES[oid[2:]]]
    if oid[:1] in ("a", "b", "g") and "$" in oid:
        k = int(oid.split("$")[1])
        return [item(n, oid) for n in allfiles[k * 5:k * 5 + 5]]
    return []


def children(oid):
    out = [c for c in base_children(oid) if not any(f'id="{g}"' in c for g in GONE)]
    return out + [item(n, oid) for n in EXTRA.get(oid, [])]


def base_children(oid):
    if TREE.startswith("synology"):
        return syn_children(oid)
    if oid == "0":
        return [container("1", "0", "Rock & Roll", 151), container("2", "0", "Jazz", 0)]
    if oid == "1":
        return [container("1$sub", "1", "Sub", 3)] + [item(n) for n in range(1, 151)]
    if oid == "1$sub":  # I894a3: a sub-container with 3 items the recursive scan must reach
        return [item(n, "1$sub") for n in range(201, 204)]
    return []


class Desc(http.server.BaseHTTPRequestHandler):
    def reply(self, name, inner):
        resp = ('<?xml version="1.0"?><s:Envelope xmlns:s="http://schemas.xmlsoap.org/soap/envelope/" '
                's:encodingStyle="http://schemas.xmlsoap.org/soap/encoding/"><s:Body>'
                f'<u:{name}Response xmlns:u="urn:schemas-upnp-org:service:ContentDirectory:{CD}">{inner}'
                f"</u:{name}Response></s:Body></s:Envelope>").encode()
        self.send_response(200)
        self.send_header("Content-Type", 'text/xml; charset="utf-8"')
        self.send_header("Content-Length", str(len(resp)))
        self.end_headers()
        self.wfile.write(resp)

    def do_POST(self):
        body = self.rfile.read(int(self.headers.get("Content-Length", "0"))).decode()
        oid = re.search(r"<ObjectID>(.*?)</ObjectID>", body)
        st = re.search(r"<StartingIndex>(\d+)</StartingIndex>", body)
        rc = re.search(r"<RequestedCount>(\d+)</RequestedCount>", body)
        action = self.headers.get("SOAPACTION", "")
        name = action.rsplit("#", 1)[-1].strip('"')
        if self.path == "/upnp/cd/control" and name == "GetSystemUpdateID":
            log("sysid")
            return self.reply(name, f"<Id>{SYSID}</Id>")
        if self.path == "/upnp/cd/control" and name == "GetServiceResetToken" and CD >= 3:
            log("reset_token")
            return self.reply(name, "<ResetToken>token-1</ResetToken>")
        if self.path == "/upnp/cd/control" and name == "Search" and CD >= 3 and st and rc:
            since = int(re.search(r"objectUpdateID &gt; (\d+)", body).group(1))
            log(f"search={since}")
            hits = [container(c, "0", f"changed-{c}", 0) for c, u in CUPD.items() if u > since]
            page = hits[int(st.group(1)):int(st.group(1)) + int(rc.group(1))]
            didl = f"<DIDL-Lite {NS}>" + "".join(page) + "</DIDL-Lite>"
            return self.reply(name, f"<Result>{escape(didl)}</Result><NumberReturned>{len(page)}</NumberReturned>"
                                    f"<TotalMatches>{len(hits)}</TotalMatches><UpdateID>{SYSID}</UpdateID>")
        if self.path != "/upnp/cd/control" or not (oid and st and rc) or name != "Browse":
            self.send_response(500)
            self.end_headers()
            return
        start, want = int(st.group(1)), int(rc.group(1))
        log(f"browse={oid.group(1)}:{start}:{want}")
        allc = children(oid.group(1))
        page = allc[start:start + min(want if want else SERVER_PAGE_CAP, SERVER_PAGE_CAP)]
        didl = f"<DIDL-Lite {NS}>" + "".join(page) + "</DIDL-Lite>"
        resp = ('<?xml version="1.0"?><s:Envelope xmlns:s="http://schemas.xmlsoap.org/soap/envelope/" '
                's:encodingStyle="http://schemas.xmlsoap.org/soap/encoding/"><s:Body>'
                '<u:BrowseResponse xmlns:u="urn:schemas-upnp-org:service:ContentDirectory:1">'
                f"<Result>{escape(didl)}</Result><NumberReturned>{len(page)}</NumberReturned>"
                f"<TotalMatches>{len(allc)}</TotalMatches><UpdateID>{SYSID}</UpdateID></u:BrowseResponse>"
                "</s:Body></s:Envelope>").encode()
        self.send_response(200)
        self.send_header("Content-Type", 'text/xml; charset="utf-8"')
        self.send_header("Content-Length", str(len(resp)))
        self.end_headers()
        self.wfile.write(resp)

    def do_GET(self):
        global SYSID
        adm = re.match(r"/admin/(add|del)\?c=([^&]*)&n=(\d+)", self.path)
        if adm:
            cid, num = adm.group(2), int(adm.group(3))
            if adm.group(1) == "add":
                EXTRA.setdefault(cid, []).append(num)
            elif num in EXTRA.get(cid, []):
                EXTRA[cid].remove(num)
            else:
                GONE.add(f"{cid}${num:03d}")
            SYSID += 1
            CUPD[cid] = SYSID
            self.send_response(200)
            self.send_header("Content-Length", "0")
            self.end_headers()
            return
        if self.path.startswith("/media/") and MEDIA:
            log("media_get=" + self.path)
            self.send_response(200)
            self.send_header("Content-Type", "audio/mpeg")
            self.send_header("Content-Length", str(len(MEDIA)))
            self.end_headers()
            self.wfile.write(MEDIA)
            return
        log("desc_get=" + self.path)
        if self.path != "/desc.xml":
            self.send_response(404)
            self.end_headers()
            return
        self.send_response(200)
        self.send_header("Content-Type", "text/xml")
        self.send_header("Content-Length", str(len(DESC)))
        self.end_headers()
        self.wfile.write(DESC)

    def log_message(self, *a):
        pass


class Quiet(http.server.HTTPServer):
    def server_bind(self):
        # HTTPServer.server_bind() calls socket.getfqdn(), a reverse-DNS lookup that stalls for 10+ s on the GitHub macOS runner
        # and kept the port file from appearing in time (I1130, same cause as I989d in fixture_server.py). The fixture only
        # binds 127.0.0.1 and never reads server_name.
        socketserver.TCPServer.server_bind(self)
        self.server_name = "127.0.0.1"
        self.server_port = self.server_address[1]


httpd = Quiet(("127.0.0.1", 0), Desc)
udp = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
udp.bind(("127.0.0.1", 0))
http_port = httpd.server_address[1]


def ssdp():
    while True:
        data, addr = udp.recvfrom(2048)
        text = data.decode("latin-1")
        if not text.startswith("M-SEARCH"):
            continue
        st = ""
        for line in text.split("\r\n"):
            if line.lower().startswith("st:"):
                st = line[3:].strip()
        log("ssdp_search=" + st)
        if st != "urn:schemas-upnp-org:device:MediaServer:1":
            continue
        reply = ("HTTP/1.1 200 OK\r\nCACHE-CONTROL: max-age=1800\r\n"
                 f"LOCATION: http://127.0.0.1:{http_port}/desc.xml\r\n"
                 "ST: urn:schemas-upnp-org:device:MediaServer:1\r\n"
                 "USN: uuid:fixture-0001::urn:schemas-upnp-org:device:MediaServer:1\r\n\r\n")
        # Twice: a real server answers on every interface, the client must keep one entry per LOCATION.
        udp.sendto(reply.encode(), addr)
        udp.sendto(reply.encode(), addr)


threading.Thread(target=ssdp, daemon=True).start()
threading.Thread(target=httpd.serve_forever, daemon=True).start()
with open(port_file + ".tmp", "w") as f:
    f.write(f"{udp.getsockname()[1]} {http_port}\n")
import os  # noqa: E402

os.replace(port_file + ".tmp", port_file)
time.sleep(lifetime)
