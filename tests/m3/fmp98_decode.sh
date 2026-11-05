#!/bin/sh
# I700: PC-98 FM songs (PMD .m, FMP .ovi) play through `selecta-cli render` as a RENDER-kind replayer (98fmplayer module).
# One reachability test through the product entry point; the sentinel is the `replayer=98fmplayer` line of render.
# Asserts per fixture: the plugin ran, the WAV is not silent and the rendered frame count is at least 2 s (the driver
# ends the song at its first loop, so no exact length is asserted). FMP98_ENERGY_MIN raises the floor for red proofs.
# Fixtures: tests/m3/fixtures/pmd_test.m and fmp_test.ovi (sources in the .SOURCE files).
# Needs SELECTA_PLUGIN_PATH to hold the 98fmplayer module (set by the test registration).
# usage: fmp98_decode.sh <selecta-cli> <fixture>...
set -u
cli=$1
shift
field() { printf '%s\n' "$1" | tr ' ' '\n' | sed -n "s/^$2=//p" | head -1; }
fail() { echo "FAIL: $*"; exit 1; }
rate=44100
for f in "$@"; do
    rm -f fd_fmp98.wav
    out=$("$cli" render "$f" --output fd_fmp98.wav --seconds 10 --rate "$rate" 2>/dev/null) || fail "render failed: $f"
    [ "$(field "$out" replayer)" = "98fmplayer" ] || fail "$f: replayer=$(field "$out" replayer), want 98fmplayer"
    got=$(field "$out" rendered_frames)
    [ "$got" -ge $((rate * 2)) ] || fail "$f: rendered_frames=$got, want at least $((rate * 2))"
    e=$(od -An -v -t d2 -j 44 fd_fmp98.wav | tr -s " " "\n" | awk '$1 != "" { s += $1 * $1 } END { printf "%d", s }')
    [ "$e" -gt "${FMP98_ENERGY_MIN:-1000000}" ] || fail "$f: silent output (energy $e)"
done
echo fmp98_decode_ok
