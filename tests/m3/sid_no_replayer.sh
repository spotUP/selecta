#!/bin/sh
# I434: in a build without the GPL plugins (SELECTA_GPL_PLUGINS OFF) no replayer claims a SID file, so the engine
# answers exactly `error no_replayer <path>` (the detail is the song path, cut to the 63 characters a message
# string holds) and nothing renders; the render entry point exits 1. Fails when a replayer claims the file (no
# error line, exit 0) or the error code or detail changes.
# usage: sid_no_replayer.sh <selecta-cli> <sid-file>
set -u
cli=$1
sid=$2
rm -f nr.wav
out=$("$cli" render "$sid" --output nr.wav --seconds 1 --rate 44100 2>&1)
rc=$?
want="error no_replayer $(printf '%s' "$sid" | cut -c1-63)"
line=$(printf '%s\n' "$out" | head -1)
echo "got: $line"
[ "$rc" -eq 1 ] || { echo "FAIL: exit $rc, want 1"; exit 1; }
[ "$line" = "$want" ] || { echo "FAIL: want '$want'"; exit 1; }
echo sid_no_replayer_ok
