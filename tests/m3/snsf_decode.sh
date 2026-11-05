#!/bin/sh
# I704: a SNSF song plays through `selecta-cli render` as a RENDER-kind replayer (snsf module, bsnes core).
# One reachability test through the product entry point; the sentinel is the `replayer=snsf` line of render.
# Asserts: the plugin ran, the WAV is not silent (SNSF_ENERGY_MIN raises the floor for red proofs) and the song lasts at
# least SNSF_MIN_SECONDS (default 6: the length tag of the fixture, plus a 1 s fade; --seconds is far above).
# Fixture: tests/m3/fixtures/snsf_test.snsf (generated, see snsf_test.snsf.SOURCE).
# usage: snsf_decode.sh <selecta-cli> <fixture.snsf>
set -u
cli=$1
f=$2
field() { printf '%s\n' "$1" | tr ' ' '\n' | sed -n "s/^$2=//p" | head -1; }
fail() { echo "FAIL: $*"; exit 1; }
energy() { od -An -v -t d2 -j 44 "$1" | tr -s " " "\n" | awk '$1 != "" { s += $1 * $1 } END { printf "%d", s }'; }
rate=44100
rm -rf fd_snsf fd_snsf.wav
mkdir fd_snsf && cp "$f" fd_snsf/ || fail "cannot stage $f"
[ -z "${SNSF_LIB:-}" ] || cp "$SNSF_LIB" fd_snsf/ || fail "cannot stage library $SNSF_LIB"
out=$("$cli" render "fd_snsf/$(basename "$f")" --output fd_snsf.wav --seconds 300 --rate "$rate" 2>/dev/null) || fail "render failed: $f"
[ "$(field "$out" replayer)" = "snsf" ] || fail "$f: replayer=$(field "$out" replayer), want snsf"
got=$(field "$out" rendered_frames)
[ "$got" -ge $((rate * ${SNSF_MIN_SECONDS:-6})) ] || fail "$f: rendered_frames=$got, want at least $((rate * ${SNSF_MIN_SECONDS:-6}))"
[ -z "${SNSF_MAX_SECONDS:-}" ] || [ "$got" -le $((rate * SNSF_MAX_SECONDS)) ] || fail "$f: rendered_frames=$got, want at most $((rate * SNSF_MAX_SECONDS))"
e0=$(energy fd_snsf.wav)
[ "$e0" -gt "${SNSF_ENERGY_MIN:-1000000}" ] || fail "$f: silent output (energy $e0)"
rm -rf fd_snsf fd_snsf.wav
echo snsf_decode_ok
