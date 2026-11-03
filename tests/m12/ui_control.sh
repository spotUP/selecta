#!/bin/sh
# I1013 ui-control-socket. Usage:
#   ui_control.sh <selecta> <selecta-cli> <fixture_library.sql> <workdir>
# Drives the live desktop UI (SDL dummy video, no audio) through the control socket only: ui.state shows the Library page and its
# first row, ui.input clicks that row's centre, ui.state (polled until input_pending=0) shows the route grew from 2 entries (home, library) to 3 and the
# ctl lines sentinel is 3, ui.frame writes a PNG. QUIT then ends the shell, whose stats line must read ui_ctl lines=3 states>=2
# frames=1; the socket file is gone. An unknown ui verb gets ERROR and exit 1.
SELECTA=$1
CLI=$2
SQL=$3
WORK=$4
rm -rf "$WORK"
mkdir -p "$WORK/cfg"
cd "$WORK" || exit 1
SELECTA_SOCKET=ctl.sock # relative: an absolute build-dir path can exceed sun_path (104)
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
sqlite3 -cmd "PRAGMA trusted_schema=ON" "$WORK/lib.db" "UPDATE songs SET key = 'local:' || path WHERE key LIKE 'fixture:%';" || fail "fixture key"

"$SELECTA" --config-dir "$WORK/cfg" --db "$WORK/lib.db" --ui desktop --backend sdl --view library --canvas 1280x800 --clock fixed --stats \
    >"$WORK/shell.out" 2>&1 &
pid=$!
i=0
while [ ! -S "$SELECTA_SOCKET" ] && [ $i -lt 300 ]; do
    sleep 0.1
    i=$((i + 1))
done
[ -S "$SELECTA_SOCKET" ] || fail "socket never appeared"

# Poll ui.state until the Library page shows its first row (the list loads on the worker).
i=0
st=
while [ $i -lt 100 ]; do
    st=$("$CLI" send ui.state 2>&1)
    case "$st" in *page=library*) case "$st" in *"route n=2"*"row 0 "*) break ;; esac ;; esac
    sleep 0.1
    i=$((i + 1))
done
case "$st" in *page=library*) ;; *) fail "ui.state not on the library page: $st" ;; esac
case "$st" in *"route n=2"*) ;; *) fail "ui.state route n=2 (home, library) missing: $st" ;; esac
row=$(printf '%s\n' "$st" | awk '$1 == "row" && $2 == "0" { print int($3 + $5 / 2), int($4 + $6 / 2); exit }')
[ -n "$row" ] || fail "no row 0 in ui.state: $st"
set -- $row
cx=$1
cy=$2

sends0=$(printf '%s\n' "$st" | sed -n 's/.* sends=\([0-9]*\).*/\1/p')
r=$("$CLI" send "ui.input move $cx $cy; click $cx $cy; dclick $cx $cy")
rc=$?
[ "$rc" = 0 ] || fail "ui.input rc=$rc reply=$r"

i=0
while [ $i -lt 100 ]; do
    st=$("$CLI" send ui.state 2>&1)
    case "$st" in *"input_pending=0"*) break ;; esac
    sleep 0.1
    i=$((i + 1))
done
case "$st" in *"input_pending=0"*) ;; *) fail "input never finished: $st" ;; esac
# A click on a Library row is VIEW_CURSOR + VIEW_ACTIVATE inside the page (it expands or drills the node and pushes no route
# entry), so the click is proved by the engine lines the UI sent, not by the route growing.
sends1=$(printf '%s\n' "$st" | sed -n 's/.* sends=\([0-9]*\).*/\1/p')
[ -n "$sends0" ] && [ -n "$sends1" ] && [ "$sends1" -gt "$sends0" ] || fail "the replayed click sent nothing to the engine (sends $sends0 -> $sends1): $st"
case "$st" in *"ctl lines=3"*) ;; *) fail "ctl lines sentinel is not 3 (three replay lines fed): $st" ;; esac

png="$WORK/frame.png"
r=$("$CLI" send "ui.frame $png")
rc=$?
[ "$rc" = 0 ] || fail "ui.frame rc=$rc reply=$r"
[ -s "$png" ] || fail "no png written"
sig=$(head -c 8 "$png" | od -An -tx1 | tr -d ' \n')
[ "$sig" = "89504e470d0a1a0a" ] || fail "png signature was $sig"

r=$("$CLI" send "ui.frame relative.png")
rc=$?
[ "$rc" = 1 ] || fail "relative frame path rc=$rc, want 1 ($r)"
r=$("$CLI" send ui.nosuch)
rc=$?
[ "$rc" = 1 ] || fail "ui.nosuch rc=$rc, want 1 ($r)"
case "$r" in ERROR*) ;; *) fail "ui.nosuch reply: $r" ;; esac

"$CLI" send QUIT >/dev/null
wait $pid
pid=
grep -E '^ui_ctl lines=3 states=([2-9]|[0-9]{2,}) frames=1$' "$WORK/shell.out" >/dev/null || fail "no ui_ctl lines=3 states>=2 frames=1 line"
[ ! -e "$SELECTA_SOCKET" ] || fail "socket file left behind"
echo "ui-control-socket ok"
