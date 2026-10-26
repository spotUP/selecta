#!/bin/sh
# I436: FLAC plays through `selecta-cli render` as a RENDER-kind replayer (dr_flac). One reachability test through the
# product entry point; the sentinel is the `replayer=dr_flac` line of render. Fixture: tests/library/fixtures/
# cover_tagged.flac (2 s). Asserts: the plugin ran, the WAV is not silent (energy of the 16-bit samples), and the
# rendered frame count equals 2 s +-5% at the output rate.
# usage: flac_decode.sh <selecta-cli> <flac-file>
set -u
cli=$1
flac=$2
field() { printf '%s\n' "$1" | tr ' ' '\n' | sed -n "s/^$2=//p" | head -1; }
fail() { echo "FAIL: $*"; exit 1; }
rate=44100
rm -f fd_flac.wav
out=$("$cli" render "$flac" --output fd_flac.wav --seconds 10 --rate "$rate" 2>/dev/null) || fail "render failed"
[ "$(field "$out" replayer)" = "dr_flac" ] || fail "replayer=$(field "$out" replayer), want dr_flac"
got=$(field "$out" rendered_frames)
awk -v g="$got" -v w=$((rate * 2)) 'BEGIN { d = g - w; if (d < 0) d = -d; exit !(d <= w * 0.05) }' ||
    fail "rendered_frames=$got, want $((rate * 2)) +-5%"
e=$(od -An -v -t d2 -j 44 fd_flac.wav | tr -s " " "\n" | awk '$1 != "" { s += $1 * $1 } END { printf "%d", s }')
[ "$e" -gt 1000000 ] || fail "silent output (energy $e)"
echo flac_decode_ok
