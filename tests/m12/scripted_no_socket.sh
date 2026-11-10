#!/bin/sh
# I575: a scripted run (--console-frames, --output null) never depends on whether another Selecta holds the default
# control socket (the user's running Selecta.app). Usage: scripted_no_socket.sh <selecta> <workdir>
# A listener holds the default socket path (XDG_RUNTIME_DIR/selecta.sock, short dir: sun_path is 104). Then:
#  - a run with no scripted marker (--exec QUIT, real output) must see the held socket: since I1162b (c0977bb2) it prints
#    the "another instance owns" notice and exits 0 instead of "already running" / exit 1, which proves the listener
#    really blocks a normal launch (the guard is live);
#  - a --console-frames run and an --output null run, both fed keys on stdin, must finish with exit 0.
SELECTA=$1
WORK=$2
rm -rf "$WORK"
mkdir -p "$WORK"
cd "$WORK" || exit 1
SOCKDIR=$(mktemp -d /tmp/selsock.XXXXXX) || exit 1
XDG_RUNTIME_DIR=$SOCKDIR
export XDG_RUNTIME_DIR
unset SELECTA_SOCKET
printf 'q' > q.keys
python3 -I -c '
import socket, sys, time
s = socket.socket(socket.AF_UNIX)
s.bind(sys.argv[1] + "/selecta.sock")
s.listen(8)
open("ready", "w").close()
time.sleep(60)
' "$SOCKDIR" &
HOLDER=$!
cleanup() {
    kill $HOLDER 2>/dev/null
    rm -rf "$SOCKDIR"
}
trap cleanup EXIT
i=0
while [ ! -e ready ] && [ $i -lt 100 ]; do
    sleep 0.1
    i=$((i + 1))
done
[ -e ready ] || { echo "FAIL: listener never came up"; exit 1; }

fail() {
    echo "FAIL: $*"
    cat "$WORK/out.txt" 2>/dev/null
    exit 1
}

"$SELECTA" --size 80x25 --start-dir "" --db plain.db --exec QUIT < q.keys > out.txt 2>&1
rc=$?
[ $rc -eq 0 ] || fail "unmarked run exited $rc, expected 0 (I1162b: runs on without the socket)"
grep -q "another instance owns" out.txt || fail "unmarked run did not report that another instance owns the socket"

"$SELECTA" --output null --size 80x25 --start-dir "" --console-frames 1 --db console.db < q.keys > out.txt 2>&1
rc=$?
[ $rc -eq 0 ] || fail "--console-frames run exited $rc with the socket held"
grep -q "another instance owns" out.txt && fail "--console-frames run hit the held socket"

"$SELECTA" --output null --size 80x25 --start-dir "" --db null.db < q.keys > out.txt 2>&1
rc=$?
[ $rc -eq 0 ] || fail "--output null run exited $rc with the socket held"

echo "scripted-no-socket ok"
