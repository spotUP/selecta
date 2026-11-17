#!/usr/bin/env python3
"""Serve the Emscripten build with the headers SharedArrayBuffer needs (I694a).

Usage: scripts/serve-web.py [dir=build/web/apps/selecta] [port=8080]
Open http://localhost:8080/selecta.html
"""
import functools
import http.server
import sys

DIR = sys.argv[1] if len(sys.argv) > 1 else "build/web/apps/selecta"
PORT = int(sys.argv[2]) if len(sys.argv) > 2 else 8080


class Handler(http.server.SimpleHTTPRequestHandler):
    extensions_map = {**http.server.SimpleHTTPRequestHandler.extensions_map, ".wasm": "application/wasm"}

    def end_headers(self):
        self.send_header("Cross-Origin-Opener-Policy", "same-origin")
        self.send_header("Cross-Origin-Embedder-Policy", "credentialless")
        self.send_header("Cache-Control", "no-store")
        super().end_headers()


http.server.ThreadingHTTPServer(("127.0.0.1", PORT), functools.partial(Handler, directory=DIR)).serve_forever()
