#!/bin/sh
# I421: Spectrum tracker modules play through `selecta-cli render` as a RENDER-kind replayer (zxtune). One
# reachability test through the product entry point, run for a Pro Tracker 3 and a Pro Tracker 2 module; the
# sentinel is the `replayer=zxtune` line of render (the engine picked the plugin and ran its adapter). Fixtures:
# tests/m3/fixtures/zxtune_test.pt3 and zxtune_test.pt2 (ZXTune's sample corpus, sources in the .SOURCE files).
# Asserts per file: the plugin ran, the WAV is not silent (energy of the 16-bit samples) and the rendered frame
# count equals 5 s +-1% at the output rate (both songs are longer than 5 s).
# usage: zxtune_decode.sh <selecta-cli> <pt3-file> <pt2-file>
set -u
cli=$1
field() { printf '%s\n' "$1" | tr ' ' '\n' | sed -n "s/^$2=//p" | head -1; }
fail() { echo "FAIL: $*"; exit 1; }
rate=44100
for f in "$2" "$3"; do
    rm -f fd_zxtune.wav
    out=$("$cli" render "$f" --output fd_zxtune.wav --seconds 5 --rate "$rate" 2>/dev/null) || fail "render failed: $f"
    [ "$(field "$out" replayer)" = "zxtune" ] || fail "$f: replayer=$(field "$out" replayer), want zxtune"
    got=$(field "$out" rendered_frames)
    awk -v g="$got" -v w=$((rate * 5)) 'BEGIN { d = g - w; if (d < 0) d = -d; exit !(d <= w * 0.01) }' ||
        fail "$f: rendered_frames=$got, want $((rate * 5)) +-1%"
    e=$(od -An -v -t d2 -j 44 fd_zxtune.wav | tr -s " " "\n" | awk '$1 != "" { s += $1 * $1 } END { printf "%d", s }')
    [ "$e" -gt 1000000 ] || fail "$f: silent output (energy $e)"
done
echo zxtune_decode_ok
