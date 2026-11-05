#!/bin/sh
# I702: a pxtone song (.ptcop) plays through `selecta-cli render` as a RENDER-kind replayer (pxtone module).
# One reachability test through the product entry point; the sentinel is the `replayer=pxtone` line of render.
# Asserts: the plugin ran, the WAV is not silent (PX_ENERGY_MIN raises the floor for red proofs) and the song lasts
# at least PX_MIN_SECONDS (default 60; the song ends by itself, so --seconds is set far above it).
# Fixture: tests/m3/fixtures/pxtone_test.ptcop (source and licence note in pxtone_test.ptcop.SOURCE).
# Needs SELECTA_PLUGIN_PATH to hold the pxtone module (set by the test registration).
# usage: pxtone_decode.sh <selecta-cli> <fixture>...
set -u
cli=$1
shift
field() { printf '%s\n' "$1" | tr ' ' '\n' | sed -n "s/^$2=//p" | head -1; }
fail() { echo "FAIL: $*"; exit 1; }
rate=44100
for f in "$@"; do
    rm -f fd_px.wav
    out=$("$cli" render "$f" --output fd_px.wav --seconds 300 --rate "$rate" 2>/dev/null) || fail "render failed: $f"
    [ "$(field "$out" replayer)" = "pxtone" ] || fail "$f: replayer=$(field "$out" replayer), want pxtone"
    got=$(field "$out" rendered_frames)
    [ "$got" -ge $((rate * ${PX_MIN_SECONDS:-60})) ] || fail "$f: rendered_frames=$got, want at least $((rate * ${PX_MIN_SECONDS:-60}))"
    e=$(od -An -v -t d2 -j 44 fd_px.wav | tr -s " " "\n" | awk '$1 != "" { s += $1 * $1 } END { printf "%d", s }')
    [ "$e" -gt "${PX_ENERGY_MIN:-1000000000}" ] || fail "$f: silent output (energy $e)"
done
echo pxtone_decode_ok
