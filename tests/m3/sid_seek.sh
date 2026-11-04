#!/bin/sh
# I434: --seek-ms on a SID file goes through the plugin's own seek hook (seek_path=native, not the skip fallback)
# and the plugin still renders 5 s of non-silent audio after the jump. Fails when the SEL_RP_SEEK hook is missing
# (seek_path=skip), the seek leaves the player dead, or the hook returns without moving: the first second after
# the seek must match second 3 of an unseeked render (SID emulation is deterministic, so a real seek matches exactly).
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
rm -f sk_ref.wav
"$cli" render "$sid" --output sk_ref.wav --seconds 4 --rate "$rate" >/dev/null 2>&1 || fail "reference render failed"
sec() { od -An -v -t d2 -j $((44 + $2 * 4)) -N $((rate * 4)) "$1" | tr -s " " "\n" | grep -v "^$"; }
sec sk_ref.wav $((rate * 3)) > sk_ref.txt
sec sk_sid.wav 0 > sk_got.txt
paste sk_ref.txt sk_got.txt | awk '{ d = $1 - $2; s += d * d; e += $1 * $1 } END { exit !(NR > 0 && s <= e * 0.01) }' ||
    fail "audio after --seek-ms 3000 does not match second 3 of an unseeked render"
echo sid_seek_ok
