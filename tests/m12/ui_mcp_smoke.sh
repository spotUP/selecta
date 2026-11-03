#!/bin/sh
# I1013 ui-control-mcp. Usage: ui_mcp_smoke.sh <selecta> <selecta-cli> <fixture_library.sql> <workdir> <mcp.py>
# Starts the live desktop UI like ui_control.sh, pipes initialize, tools/list and tools/call ui_state into scripts/selecta-ui-mcp.py
# and checks the six tool names and page=library in the ui_state result.
SELECTA=$1
CLI=$2
SQL=$3
WORK=$4
MCP=$5
rm -rf "$WORK"
mkdir -p "$WORK/cfg"
cd "$WORK" || exit 1
SELECTA_SOCKET=ctl.sock
export SELECTA_SOCKET
SDL_VIDEODRIVER=dummy
export SDL_VIDEODRIVER
SELECTA_SDL_TEST_LIVE=1
export SELECTA_SDL_TEST_LIVE

fail() {
    echo "FAIL: $*"
    cat "$WORK/shell.out" 2>/dev/null
    [ -n "$pid" ] && kill "$pid" 2>/dev/null
    exit 1
}

command -v sqlite3 >/dev/null 2>&1 || { echo "SELECTA_SKIP: no sqlite3"; exit 0; }
"$SELECTA" --ui desktop --backend mem --view home --canvas 1280x800 --clock fixed --stats --output null --db "$WORK/lib.db" >/dev/null 2>&1 || fail "db init"
sqlite3 -cmd "PRAGMA trusted_schema=ON" "$WORK/lib.db" <"$SQL" || fail "fixture load"
"$SELECTA" --config-dir "$WORK/cfg" --db "$WORK/lib.db" --ui desktop --backend sdl --view library --canvas 1280x800 --clock fixed --stats \
    >"$WORK/shell.out" 2>&1 &
pid=$!
i=0
while [ ! -S "$SELECTA_SOCKET" ] && [ $i -lt 100 ]; do
    sleep 0.1
    i=$((i + 1))
done
[ -S "$SELECTA_SOCKET" ] || fail "socket never appeared"
sleep 1

out=$(printf '%s\n' \
    '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{}}' \
    '{"jsonrpc":"2.0","method":"notifications/initialized"}' \
    '{"jsonrpc":"2.0","id":2,"method":"tools/list"}' \
    '{"jsonrpc":"2.0","id":3,"method":"tools/call","params":{"name":"ui_state","arguments":{}}}' | python3 -I "$MCP")
for t in ui_state ui_input ui_frame ui_dump ui_sleep selecta_send; do
    case "$out" in *"\"name\": \"$t\""*) ;; *) fail "tool $t missing: $out" ;; esac
done
n=$(printf '%s\n' "$out" | grep -o '"name": "[a-z_]*"' | wc -l | tr -d ' ')
[ "$n" = 6 ] || fail "tool count $n, want 6"
case "$out" in *page=library*) ;; *) fail "ui_state result lacks page=library: $out" ;; esac
"$CLI" send QUIT >/dev/null
wait $pid
pid=
echo "ui-control-mcp ok"
