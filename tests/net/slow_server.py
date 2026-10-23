"""Serves one file part way over HTTP and then stalls with the connection open (I195b, a radio stream that never
finishes). argv: portfile, file, repeat, percent, lifetime-seconds. The body is the file repeated `repeat` times;
the response advertises its full length, sends `percent` of it and goes quiet. Exits after the lifetime."""
import socket
import sys
import threading
import time

port_file, path, repeat, percent, lifetime = sys.argv[1], sys.argv[2], int(sys.argv[3]), int(sys.argv[4]), float(sys.argv[5])
with open(path, "rb") as f:
    body = f.read() * repeat
sent_n = len(body) * percent // 100

srv = socket.socket()
srv.bind(("127.0.0.1", 0))
srv.listen(8)
with open(port_file, "w") as f:
    f.write(str(srv.getsockname()[1]))
held = []


def serve(c):
    c.recv(4096)
    head = "HTTP/1.1 200 OK\r\nContent-Type: audio/mpeg\r\nContent-Length: %d\r\nConnection: close\r\n\r\n" % len(body)
    c.sendall(head.encode() + body[:sent_n])
    held.append(c)


def accept():
    while True:
        c, _ = srv.accept()
        threading.Thread(target=serve, args=(c,), daemon=True).start()


threading.Thread(target=accept, daemon=True).start()
time.sleep(lifetime)
