#!/bin/sh
# T35 control-socket (plan 2026-10-23-m1-2, phase 6). Usage:
#   control_socket.sh <selecta> <selecta-cli> <wait.keys> <workdir>
# Starts the shell with the null output on a per-test socket, sends NEXT and
# PAUSE through `selecta-cli send`, then checks the replies, the exit codes,
# the shell's socket_cmds=2 line and that the socket file is gone. A command
# line that does not parse gets an ERROR reply (exit 1) and is not counted.
SELECTA=$1
CLI=$2
KEYS=$3
WORK=$4
rm -rf "$WORK"
mkdir -p "$WORK"
cd "$WORK" || exit 1
SELECTA_SOCKET=ctl.sock # relative: an absolute build-dir path can exceed sun_path (104)
export SELECTA_SOCKET

fail() {
    echo "FAIL: $*"
    cat "$WORK/shell.out" 2>/dev/null
    exit 1
}

# No shell running yet: exit 2 and the documented message.
msg=$("$CLI" send NEXT 2>&1)
rc=$?
[ "$rc" = 2 ] || fail "send without a shell exited $rc, want 2"
[ "$msg" = "no selecta running at $SELECTA_SOCKET" ] || fail "message was: $msg"

"$SELECTA" --output null --start-dir "$WORK" --keys "$KEYS" >"$WORK/shell.out" 2>&1 &
pid=$!
i=0
while [ ! -S "$SELECTA_SOCKET" ] && [ $i -lt 100 ]; do
    sleep 0.1
    i=$((i + 1))
done
[ -S "$SELECTA_SOCKET" ] || fail "socket never appeared"

r1=$("$CLI" send NEXT)
rc1=$?
r2=$("$CLI" send PAUSE)
rc2=$?
[ "$rc1" = 0 ] && [ "$r1" = "OK" ] || fail "NEXT: rc=$rc1 reply=$r1"
[ "$rc2" = 0 ] && [ "$r2" = "OK" ] || fail "PAUSE: rc=$rc2 reply=$r2"
r3=$("$CLI" send NOSUCHVERB)
rc3=$?
[ "$rc3" = 1 ] || fail "bad verb exited $rc3, want 1 (reply: $r3)"
case "$r3" in ERROR*) ;; *) fail "bad verb reply: $r3" ;; esac

wait $pid
grep -q '^socket_cmds=2$' "$WORK/shell.out" || fail "no socket_cmds=2 line"
[ ! -e "$SELECTA_SOCKET" ] || fail "socket file left behind"
echo "control-socket ok"
