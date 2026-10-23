"""Accepts TCP connections on 127.0.0.1 and never answers (I173f). Writes its port to argv[1], exits after argv[2] s."""
import socket
import sys
import threading
import time

srv = socket.socket()
srv.bind(("127.0.0.1", 0))
srv.listen(8)
with open(sys.argv[1], "w") as f:
    f.write(str(srv.getsockname()[1]))
held = []


def accept():
    while True:
        c, _ = srv.accept()
        held.append(c)


threading.Thread(target=accept, daemon=True).start()
time.sleep(float(sys.argv[2]))
