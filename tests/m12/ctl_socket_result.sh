#!/bin/sh
# M4-AX-0-4 ctl-socket-result, plus the QUIT checks of M4-AX-2-4 (plan 2026-10-25-m4-arexx). Usage:
#   ctl_socket_result.sh <selecta> <selecta-cli> <wait.keys> <workdir> <SEL_CMD_COUNT> <song>
# Through the real shell and `selecta-cli send`: COMMANDS replies "OK", one line per verb, then "."; STATUS replies
# its key=value lines; QUIT over the socket ends the process with exit 0 after its "OK" reply; `--exec QUIT` ends a
# run that would otherwise wait for keys and still prints the stats lines.
SELECTA=$1
CLI=$2
KEYS=$3
WORK=$4
COUNT=$5
SONG=$6
rm -rf "$WORK"
mkdir -p "$WORK"
cd "$WORK" || exit 1
SELECTA_SOCKET=ctl.sock
export SELECTA_SOCKET

fail() {
    echo "FAIL: $*"
    cat "$WORK/shell.out" 2>/dev/null
    exit 1
}

"$SELECTA" --output null --start-dir "$WORK" --keys "$KEYS" --stats >"$WORK/shell.out" 2>&1 &
pid=$!
i=0
while [ ! -S "$SELECTA_SOCKET" ] && [ $i -lt 100 ]; do
    sleep 0.1
    i=$((i + 1))
done
[ -S "$SELECTA_SOCKET" ] || fail "socket never appeared"

"$CLI" send COMMANDS >"$WORK/commands.out"
rc=$?
[ "$rc" = 0 ] || fail "COMMANDS exited $rc"
[ "$(sed -n 1p "$WORK/commands.out")" = "OK" ] || fail "COMMANDS first line: $(sed -n 1p "$WORK/commands.out")"
[ "$(tail -n 1 "$WORK/commands.out")" = "." ] || fail "COMMANDS does not end with a dot line"
lines=$(($(wc -l <"$WORK/commands.out") - 2))
[ "$lines" = "$COUNT" ] || fail "COMMANDS has $lines verb lines, want $COUNT"
grep -q "^VOLUME level$(printf '\t')Volume$(printf '\t')args=i area=transport$" "$WORK/commands.out" || fail "VOLUME line missing or malformed"

"$CLI" send STATUS >"$WORK/status.out"
rc=$?
[ "$rc" = 0 ] || fail "STATUS exited $rc"
grep -q '^state=stopped$' "$WORK/status.out" || fail "STATUS has no state=stopped"
grep -q '^volume=' "$WORK/status.out" || fail "STATUS has no volume="

"$CLI" send "OPEN $SONG" >/dev/null || fail "OPEN failed"
"$CLI" send PLAY >/dev/null || fail "PLAY failed"
"$CLI" send "VOLUME 40" >/dev/null || fail "VOLUME failed"
sleep 0.5
"$CLI" send STATUS >"$WORK/status2.out" || fail "second STATUS failed"
grep -q '^state=playing$' "$WORK/status2.out" || fail "STATUS after PLAY has no state=playing: $(cat "$WORK/status2.out")"
grep -q '^volume=40$' "$WORK/status2.out" || fail "STATUS after VOLUME 40 has no volume=40"
grep -q '^title=.' "$WORK/status2.out" || fail "STATUS after OPEN has an empty title="

"$CLI" send QUIT >"$WORK/quit.out"
rc=$?
[ "$rc" = 0 ] && [ "$(cat "$WORK/quit.out")" = "OK" ] || fail "QUIT: rc=$rc reply=$(cat "$WORK/quit.out")"
wait $pid
rc=$?
[ "$rc" = 0 ] || fail "shell exited $rc after QUIT, want 0"
grep -q '^socket_cmds=7$' "$WORK/shell.out" || fail "no socket_cmds=7 line"

# --exec QUIT: the keys file would keep the run waiting; QUIT ends it at once.
"$SELECTA" --output null --start-dir "$WORK" --keys "$KEYS" --stats --exec "QUIT" >"$WORK/exec.out" 2>&1
rc=$?
[ "$rc" = 0 ] || fail "--exec QUIT exited $rc"
grep -q '^ui_waits=' "$WORK/exec.out" || fail "--exec QUIT printed no stats lines"
waits=$(sed -n 's/^ui_waits=\([0-9]*\).*/\1/p' "$WORK/exec.out")
[ "$waits" -lt 50 ] || fail "--exec QUIT ran $waits turns, the keys file alone gives 150"
echo "ctl-socket-result ok"
