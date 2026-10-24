#!/bin/sh
# Start the wasabi MCP server (UP-Term's tools/rig/wasabimcp.py) against the
# Replay A1200 at 192.168.0.74:1234. The wasabid key is read from
# ~/.config/upterm/a1200-wasabi.conf at start and never stored in this repo.
CONF="$HOME/.config/upterm/a1200-wasabi.conf"
WASABI_KEY=$(python3 -I -c 'import os, sys
for l in open(sys.argv[1]):
    l = l.split("#", 1)[0]
    if "=" in l and l.split("=", 1)[0].strip() == "key":
        print(l.split("=", 1)[1].strip()); break' "$CONF") || exit 1
export WASABI_HOST=192.168.0.74 WASABI_PORT=1234 WASABI_KEY
exec python3 "$HOME/Code/UP-Term/vtcon/tools/rig/wasabimcp.py"
