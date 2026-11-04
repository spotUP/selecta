#!/bin/sh
# I432: a CHIPNSFX song (.chp) plays through `selecta-cli render` as a RENDER-kind replayer (chipnsfx module, CNGSoft's
# tracker code, GPL). One reachability test through the product entry point; the sentinel is the `replayer=chipnsfx`
# line of render. Fixture: tests/m3/fixtures/chp_test.chp (source in chp_test.chp.SOURCE). Asserts: the plugin ran, the
# WAV is not silent (energy of the 16-bit samples) and the rendered frame count equals 10 s +-1% at the output rate.
# Needs SELECTA_PLUGIN_PATH to hold the chipnsfx module (set by the test registration); with the module absent the
# render fails and so does this test.
# usage: chipnsfx_decode.sh <selecta-cli> <chp-file>
set -u
cli=$1
chp=$2
field() { printf '%s\n' "$1" | tr ' ' '\n' | sed -n "s/^$2=//p" | head -1; }
fail() { echo "FAIL: $*"; exit 1; }
rate=44100
rm -f fd_chp.wav
out=$("$cli" render "$chp" --output fd_chp.wav --seconds 10 --rate "$rate" 2>/dev/null) || fail "render failed"
[ "$(field "$out" replayer)" = "chipnsfx" ] || fail "replayer=$(field "$out" replayer), want chipnsfx"
got=$(field "$out" rendered_frames)
awk -v g="$got" -v w=$((rate * 10)) 'BEGIN { d = g - w; if (d < 0) d = -d; exit !(d <= w * 0.01) }' ||
    fail "rendered_frames=$got, want $((rate * 10)) +-1%"
e=$(od -An -v -t d2 -j 44 fd_chp.wav | tr -s " " "\n" | awk '$1 != "" { s += $1 * $1 } END { printf "%d", s }')
[ "$e" -gt 1000000 ] || fail "silent output (energy $e)"
echo chipnsfx_decode_ok
