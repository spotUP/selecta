#!/bin/sh
# I426: an NES NSF plays through `selecta-cli render` as a RENDER-kind replayer (gme, Game Music Emu). One
# reachability test through the product entry point; the sentinel is the `replayer=gme` line of render. Fixture:
# tests/m3/fixtures/gme_test.nsf (libgme's own test file, source in gme_test.nsf.SOURCE; it has no stored length, so
# the song runs past the 10 s asked for). Asserts: the plugin ran, the WAV is not silent (energy of the 16-bit
# samples) and the rendered frame count equals 10 s +-1% at the output rate.
# usage: gme_decode.sh <selecta-cli> <nsf-file>
set -u
cli=$1
nsf=$2
field() { printf '%s\n' "$1" | tr ' ' '\n' | sed -n "s/^$2=//p" | head -1; }
fail() { echo "FAIL: $*"; exit 1; }
rate=44100
rm -f fd_gme.wav
out=$("$cli" render "$nsf" --output fd_gme.wav --seconds 10 --rate "$rate" 2>/dev/null) || fail "render failed"
[ "$(field "$out" replayer)" = "gme" ] || fail "replayer=$(field "$out" replayer), want gme"
got=$(field "$out" rendered_frames)
awk -v g="$got" -v w=$((rate * 10)) 'BEGIN { d = g - w; if (d < 0) d = -d; exit !(d <= w * 0.01) }' ||
    fail "rendered_frames=$got, want $((rate * 10)) +-1%"
e=$(od -An -v -t d2 -j 44 fd_gme.wav | tr -s " " "\n" | awk '$1 != "" { s += $1 * $1 } END { printf "%d", s }')
[ "$e" -gt 1000000 ] || fail "silent output (energy $e)"
echo gme_decode_ok
