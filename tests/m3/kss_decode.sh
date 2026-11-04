#!/bin/sh
# I698: MSX songs play through `selecta-cli render` as a RENDER-kind replayer (libkss module). One reachability test
# through the product entry point; the sentinel is the `replayer=libkss` line of render. It covers the plain .kss path
# (where the gme plugin also claims the extension: libkss must win) and one driver format (.mgs, wrapped with the MGSDRV
# binary). Asserts per fixture: the plugin ran, the WAV is not silent and the rendered frame count equals 10 s +-1%.
# Fixtures: tests/m3/fixtures/kss_test.kss and kss_test.mgs (sources in the .SOURCE files).
# Needs SELECTA_PLUGIN_PATH to hold the libkss module (set by the test registration).
# usage: kss_decode.sh <selecta-cli> <kss-file> <driver-file>
set -u
cli=$1
shift
field() { printf '%s\n' "$1" | tr ' ' '\n' | sed -n "s/^$2=//p" | head -1; }
fail() { echo "FAIL: $*"; exit 1; }
rate=44100
for f in "$@"; do
    rm -f fd_kss.wav
    out=$("$cli" render "$f" --output fd_kss.wav --seconds 10 --rate "$rate" 2>/dev/null) || fail "render failed: $f"
    [ "$(field "$out" replayer)" = "libkss" ] || fail "$f: replayer=$(field "$out" replayer), want libkss"
    got=$(field "$out" rendered_frames)
    awk -v g="$got" -v w=$((rate * 10)) 'BEGIN { d = g - w; if (d < 0) d = -d; exit !(d <= w * 0.01) }' ||
        fail "$f: rendered_frames=$got, want $((rate * 10)) +-1%"
    e=$(od -An -v -t d2 -j 44 fd_kss.wav | tr -s " " "\n" | awk '$1 != "" { s += $1 * $1 } END { printf "%d", s }')
    [ "$e" -gt 1000000 ] || fail "$f: silent output (energy $e)"
done
echo kss_decode_ok
