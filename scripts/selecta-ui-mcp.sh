#!/bin/sh
# Start the Selecta live UI MCP server (I1013): stdio JSON-RPC, six tools over the running shell's control socket.
# Register once:  claude mcp add selecta-ui -- sh scripts/selecta-ui-mcp.sh
# The socket is $SELECTA_SOCKET, else $XDG_RUNTIME_DIR/selecta.sock, else $TMPDIR/selecta-<uid>.sock.
exec python3 -I "$(dirname "$0")/selecta-ui-mcp.py"
