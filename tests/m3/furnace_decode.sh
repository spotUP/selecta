#!/bin/sh
# I430: Furnace modules (.fur, .dmf, .ftm, .tfm) play through `selecta-cli render` as a RENDER-kind replayer (furnace,
# headless Furnace engine). One reachability test through the product entry point; the sentinel is the `replayer=furnace`
# line of render. Fixture: tests/m3/fixtures/furnace_test.fur (original, see furnace_test.SOURCE). Asserts: the plugin
# ran, the WAV is not silent (energy of the 16-bit samples), rendered_frames is within 1% of the expected length
# (EXPECTED_SECONDS, measured independently by the fixture generator from the song's tick count), then
# `plugins --formats furnace` lists the family.
# usage: furnace_decode.sh <selecta-cli> <fur-file> <expected-seconds>
set -u
cli=$1
field() { printf '%s\n' "$1" | tr ' ' '\n' | sed -n "s/^$2=//p" | head -1; }
fail() { echo "FAIL: $*"; exit 1; }
rate=44100
rm -f fd_furnace.wav
out=$("$cli" render "$2" --output fd_furnace.wav --seconds 60 --rate "$rate" 2>/dev/null) || fail "render failed: $2"
[ "$(field "$out" replayer)" = "furnace" ] || fail "$2: replayer=$(field "$out" replayer), want furnace"
got=$(field "$out" rendered_frames)
awk -v g="$got" -v w="$(awk -v s="$3" -v r="$rate" 'BEGIN { printf "%d", s * r }')" \
    'BEGIN { d = g - w; if (d < 0) d = -d; exit !(d <= w * 0.01) }' ||
    fail "$2: rendered_frames=$got, want $3 s +-1%"
e=$(od -An -v -t d2 -j 44 fd_furnace.wav | tr -s " " "\n" | awk '$1 != "" { s += $1 * $1 } END { printf "%d", s }')
[ "$e" -gt 1000000 ] || fail "$2: silent output (energy $e)"
fm=$("$cli" plugins --formats furnace 2>&1) || fail "plugins --formats furnace: $fm"
for id in fur dmf ftm tfm; do
    printf '%s\n' "$fm" | grep -q "^player $id\$" || fail "format $id not listed: $fm"
done
echo furnace_decode_ok
