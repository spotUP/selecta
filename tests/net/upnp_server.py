"""UPnP MediaServer mock for I894a1. argv: portfile, lifetime-seconds.
Binds a UDP socket (the SSDP responder) and a TCP HTTP server (the device description) on 127.0.0.1, both on free
ports, and writes "<udp port> <http port>" to <portfile>. An M-SEARCH for the MediaServer:1 target gets one unicast
200 response whose LOCATION is the description URL (the real multicast address is not used, so a CI box without
multicast works). Every request is logged in <portfile>.requests as "ssdp_search=<ST>" and "desc_get=<path>".
The description has a friendlyName with an entity, a relative controlURL in a ContentDirectory service that comes
after a ConnectionManager service, and no URLBase: the client must resolve the control path against LOCATION."""
import http.server
import socket
import sys
import threading
import time

port_file, lifetime = sys.argv[1], float(sys.argv[2])
NAME = "Fixture Media &amp; Music"
DESC = f"""<?xml version="1.0"?>
<root xmlns="urn:schemas-upnp-org:device-1-0">
 <specVersion><major>1</major><minor>0</minor></specVersion>
 <device>
  <deviceType>urn:schemas-upnp-org:device:MediaServer:1</deviceType>
  <friendlyName>{NAME}</friendlyName>
  <serviceList>
   <service>
    <serviceType>urn:schemas-upnp-org:service:ConnectionManager:1</serviceType>
    <controlURL>/upnp/cm/control</controlURL>
   </service>
   <service>
    <serviceType>urn:schemas-upnp-org:service:ContentDirectory:1</serviceType>
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


class Desc(http.server.BaseHTTPRequestHandler):
    def do_GET(self):
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


httpd = http.server.HTTPServer(("127.0.0.1", 0), Desc)
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
