#!/bin/sh
# I434: --seek-ms on a SID file goes through the plugin's own seek hook (seek_path=native, not the skip fallback)
# and the plugin still renders 5 s of non-silent audio after the jump. Fails when the SEL_RP_SEEK hook is missing
# (seek_path=skip) or the seek leaves the player dead.
# usage: sid_seek.sh <selecta-cli> <sid-file>
set -u
cli=$1
sid=$2
field() { printf '%s\n' "$1" | tr ' ' '\n' | sed -n "s/^$2=//p" | head -1; }
fail() { echo "FAIL: $*"; exit 1; }
rate=44100
rm -f sk_sid.wav
out=$("$cli" render "$sid" --output sk_sid.wav --seconds 5 --rate "$rate" --seek-ms 3000 2>/dev/null) || fail "render failed"
[ "$(field "$out" replayer)" = "sid" ] || fail "replayer=$(field "$out" replayer), want sid"
[ "$(field "$out" seek_path)" = "native" ] || fail "seek_path=$(field "$out" seek_path), want native"
got=$(field "$out" rendered_frames)
awk -v g="$got" -v w=$((rate * 5)) 'BEGIN { d = g - w; if (d < 0) d = -d; exit !(d <= w * 0.01) }' ||
    fail "rendered_frames=$got, want $((rate * 5)) +-1%"
e=$(od -An -v -t d2 -j 44 sk_sid.wav | tr -s " " "\n" | awk '$1 != "" { s += $1 * $1 } END { printf "%d", s }')
[ "$e" -gt 1000000 ] || fail "silent output after seek (energy $e)"
echo sid_seek_ok
