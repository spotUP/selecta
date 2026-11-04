#!/bin/sh
# I696: OPL/AdLib songs (HSC, RAW, ...) play through `selecta-cli render` as a RENDER-kind replayer (adplug, Nuked OPL3).
# One reachability test through the product entry point; the sentinel is the `replayer=adplug` line of render (the
# engine picked the plugin and ran its adapter). Fixtures: tests/m3/fixtures/adplug_test.hsc and adplug_test.raw (hand
# made, see adplug_test.SOURCE). Asserts per file: the plugin ran, rendered_frames is within 25% below the requested
# seconds (the songs are longer than --seconds 3, so the render fills the request) and the WAV is not silent (energy of
# the 16-bit samples). Then `plugins --formats adplug` lists the family.
# usage: adplug_decode.sh <selecta-cli> <hsc-file> <raw-file>
set -u
cli=$1
field() { printf '%s\n' "$1" | tr ' ' '\n' | sed -n "s/^$2=//p" | head -1; }
fail() { echo "FAIL: $*"; exit 1; }
rate=44100
for f in "$2" "$3"; do
    rm -f fd_adplug.wav
    out=$("$cli" render "$f" --output fd_adplug.wav --seconds 3 --rate "$rate" 2>/dev/null) || fail "render failed: $f"
    [ "$(field "$out" replayer)" = "adplug" ] || fail "$f: replayer=$(field "$out" replayer), want adplug"
    got=$(field "$out" rendered_frames)
    awk -v g="$got" -v w=$((rate * 3)) 'BEGIN { d = g - w; if (d < 0) d = -d; exit !(d <= w * 0.25) }' ||
        fail "$f: rendered_frames=$got, want $((rate * 3)) +-25%"
    e=$(od -An -v -t d2 -j 44 fd_adplug.wav | tr -s " " "\n" | awk '$1 != "" { s += $1 * $1 } END { printf "%d", s }')
    [ "$e" -gt 1000000 ] || fail "$f: silent output (energy $e)"
done
fm=$("$cli" plugins --formats adplug 2>&1) || fail "plugins --formats adplug: $fm"
for id in hsc raw rad a2m cmf d00 laa; do
    printf '%s\n' "$fm" | grep -q "^player $id\$" || fail "format $id not listed: $fm"
done
echo adplug_decode_ok
