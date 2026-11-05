"""dui-engine-thread-stall (I1082a): a blocking engine open must not stop the desktop UI.

Runs the real Selecta binary (desktop UI, SDL dummy video, paced silent null output) against tests/net/slow_server.py,
which serves a Protracker module that needs the whole file: 10 percent of it, then silence for 10 s. The control
socket sends OPEN <url>; while the engine waits for the rest of the file the UI must keep drawing.
argv: selecta slow_server.py module.mod workdir
Asserts: ui_frames grows by >= 20 between two ui.state samples taken inside the stall, ui_gap_ms_max < 100,
engine_thread=1 and engine_pumps > 0 (the dump lines of apps/selecta/main.c)."""
import os
import re
import shutil
import socket
import subprocess
import sys
import tempfile
import threading
import time

selecta, slow, mod, work = sys.argv[1:5]
shutil.rmtree(work, ignore_errors=True)
os.makedirs(work + "/cfg")
with open(work + "/cfg/prefs.conf", "w") as f:
    f.write("library.sync_stale_days=0\n")
short = tempfile.mkdtemp(prefix="est")  # sun_path is short
sock_path = short + "/c.sock"
port_file = work + "/slow-port"
procs = []


def finish(code, msg):
    for p in procs:
        p.kill()
    shutil.rmtree(short, ignore_errors=True)
    print(msg)
    sys.exit(code)


def ctl(line, timeout):
    s = socket.socket(socket.AF_UNIX)
    s.settimeout(timeout)
    s.connect(sock_path)
    s.sendall((line + "\n").encode())
    data = b""
    while True:
        try:
            b = s.recv(4096)
        except socket.timeout:
            break
        if not b:
            break
        data += b
    s.close()
    return data.decode(errors="replace")


srv = subprocess.Popen([sys.executable, "-I", slow, port_file, mod, "1", "10", "16"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
procs.append(srv)
for _ in range(50):
    if os.path.exists(port_file) and open(port_file).read().strip():
        break
    time.sleep(0.1)
port = open(port_file).read().strip()
env = dict(os.environ, SDL_VIDEODRIVER="dummy", SELECTA_SDL_TEST_LIVE="1", SELECTA_SDL_TEST_TIMED="q@24000",
           SELECTA_SOCKET=sock_path, SELECTA_NULL_PACED="1", SELECTA_NET_ALLOW="127.0.0.1",
           SELECTA_NET_MAP="slow.test=http://127.0.0.1:" + port)
sel = subprocess.Popen([selecta, "--ui", "desktop", "--backend", "sdl", "--output", "null", "--view", "home", "--canvas", "1280x800",
                        "--stats", "--config-dir", work + "/cfg", "--db", work + "/lib.db"], env=env,
                       stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
procs.append(sel)
up = False
for _ in range(100):
    if os.path.exists(sock_path):
        try:
            if "ui_frames=" in ctl("ui.state", 5):
                up = True
                break
        except OSError:
            pass
    time.sleep(0.2)
if not up:
    finish(1, "dui-engine-thread-stall: Selecta never answered ui.state on the control socket")

samples = {}


def sample(name, delay):
    time.sleep(delay)
    t = time.time()
    r = ctl("ui.state", 40)
    m = re.search(r"ui_frames=(\d+)", r)
    samples[name] = (int(m.group(1)) if m else -1, time.time() - t0, time.time() - t)


t0 = time.time()
threading.Thread(target=lambda: ctl("OPEN http://slow.test/long.mod", 40), daemon=True).start()
ths = [threading.Thread(target=sample, args=("b", 2.0)), threading.Thread(target=sample, args=("c", 8.0))]
for t in ths:
    t.start()
for t in ths:
    t.join()
out, err = sel.communicate(timeout=60)


def num(key):
    m = re.search(key + r"=(\d+)", out)
    return int(m.group(1)) if m else -1


fb, fc = samples["b"][0], samples["c"][0]
gap, thread, pumps = num("ui_gap_ms_max"), num("engine_thread"), num("engine_pumps")
line = "frames_b=%d frames_c=%d (reply after %.1fs / %.1fs) ui_gap_ms_max=%d engine_thread=%d engine_pumps=%d" % (
    fb, fc, samples["b"][2], samples["c"][2], gap, thread, pumps)
bad = []
if fb < 0 or fc - fb < 20:
    bad.append("frames grew by %d during the stall, want >= 20" % (fc - fb))
if gap < 0 or gap >= 100:
    bad.append("ui_gap_ms_max is %d, want < 100" % gap)
if thread != 1:
    bad.append("engine_thread is %d, want 1" % thread)
if pumps <= 0:
    bad.append("engine_pumps is %d, want > 0" % pumps)
if bad:
    finish(1, "dui-engine-thread-stall FAILED: " + "; ".join(bad) + "\n" + line)
finish(0, "dui-engine-thread-stall ok " + line)
