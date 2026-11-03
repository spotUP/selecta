"""Selecta live UI MCP server (I1013): stdio JSON-RPC 2.0, stdlib only.

Six tools drive the running Selecta shell through its control socket (one connection per line, reply read to EOF):
ui_state, ui_input, ui_frame, ui_dump, ui_sleep, selecta_send.
"""
import json
import os
import socket
import sys
import time


def socket_path():
    p = os.environ.get("SELECTA_SOCKET")
    if p:
        return p
    xdg = os.environ.get("XDG_RUNTIME_DIR")
    if xdg:
        return os.path.join(xdg, "selecta.sock")
    return os.path.join(os.environ.get("TMPDIR", "/tmp"), "selecta-%d.sock" % os.getuid())


def send(line):
    s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    s.settimeout(30)
    try:
        s.connect(socket_path())
        s.sendall(line.encode() + b"\n")
        s.shutdown(socket.SHUT_WR)
        buf = b""
        while True:
            c = s.recv(65536)
            if not c:
                break
            buf += c
        return buf.decode(errors="replace")
    except OSError as e:
        return "ERROR cannot reach selecta at %s: %s\n" % (socket_path(), e)
    finally:
        s.close()


def ui_input(args):
    lines = args.get("lines") or []
    settle = int(args.get("settle_ms", 3000))
    out = send("ui.input " + "; ".join(lines))
    if "pending=1" in out:
        deadline = time.time() + settle / 1000.0
        while time.time() < deadline:
            time.sleep(0.05)
            if "input_pending=0" in send("ui.state"):
                break
    return out


def ui_sleep(args):
    time.sleep(int(args["ms"]) / 1000.0)
    return "OK\n"


TOOLS = [
    ("ui_state", "Current page, route history, focus, scroll, visible rows and the control counters.", {}, lambda a: send("ui.state")),
    ("ui_input", "Feed replay-script lines (pointer, key, wheel, door verbs) to the UI; waits until they finish.",
     {"lines": {"type": "array", "items": {"type": "string"}}, "settle_ms": {"type": "integer"}}, ui_input),
    ("ui_frame", "Write the current frame to an absolute .png or .ppm path.", {"path": {"type": "string"}},
     lambda a: send("ui.frame " + a["path"])),
    ("ui_dump", "Write the full library dump to an absolute path.", {"path": {"type": "string"}},
     lambda a: send("ui.state dump " + a["path"])),
    ("ui_sleep", "Wait ms milliseconds.", {"ms": {"type": "integer"}}, ui_sleep),
    ("selecta_send", "Send one control-socket line (for example PAUSE) and return the reply.", {"line": {"type": "string"}},
     lambda a: send(a["line"])),
]


def tool_list():
    return [{"name": n, "description": d, "inputSchema": {"type": "object", "properties": p}} for n, d, p, _ in TOOLS]


def handle(req):
    m = req.get("method")
    if m == "initialize":
        return {"protocolVersion": "2024-11-05", "capabilities": {"tools": {}}, "serverInfo": {"name": "selecta-ui", "version": "1"}}
    if m == "tools/list":
        return {"tools": tool_list()}
    if m == "tools/call":
        p = req.get("params", {})
        for n, _, _, fn in TOOLS:
            if n == p.get("name"):
                text = fn(p.get("arguments") or {})
                return {"content": [{"type": "text", "text": text}], "isError": text.startswith("ERROR")}
        return {"content": [{"type": "text", "text": "unknown tool"}], "isError": True}
    if m == "ping":
        return {}
    return None


def main():
    for raw in sys.stdin:
        raw = raw.strip()
        if not raw:
            continue
        req = json.loads(raw)
        if "id" not in req:
            continue
        res = handle(req)
        if res is None:
            out = {"jsonrpc": "2.0", "id": req["id"], "error": {"code": -32601, "message": "method not found"}}
        else:
            out = {"jsonrpc": "2.0", "id": req["id"], "result": res}
        sys.stdout.write(json.dumps(out) + "\n")
        sys.stdout.flush()


main()
