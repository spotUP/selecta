#!/bin/sh
# I704: a QSF song plays through `selecta-cli render` as a RENDER-kind replayer (qsf module, Highly Quixotic core).
# One reachability test through the product entry point; the sentinel is the `replayer=qsf` line of render.
# Asserts: the plugin ran, the WAV is not silent (QSF_ENERGY_MIN raises the floor for red proofs) and the song lasts at
# least QSF_MIN_SECONDS (default 6: the length tag of the fixture, plus a 1 s fade; --seconds is far above).
# Fixture: tests/m3/fixtures/qsf_test.qsf (generated, see qsf_test.qsf.SOURCE).
# Real game set (qsf-real-decode): QSF_LIB names the .qsflib next to the song (staged beside it, it carries the
# Kabuki key and the ROM banks), QSF_MAX_SECONDS caps the length (the song's length tag plus fade, with a 1 s margin).
# usage: qsf_decode.sh <selecta-cli> <fixture.qsf>
set -u
cli=$1
f=$2
field() { printf '%s\n' "$1" | tr ' ' '\n' | sed -n "s/^$2=//p" | head -1; }
fail() { echo "FAIL: $*"; exit 1; }
energy() { od -An -v -t d2 -j 44 "$1" | tr -s " " "\n" | awk '$1 != "" { s += $1 * $1 } END { printf "%d", s }'; }
rate=44100
rm -rf fd_qsf fd_qsf.wav
mkdir fd_qsf && cp "$f" fd_qsf/ || fail "cannot stage $f"
[ -z "${QSF_LIB:-}" ] || cp "$QSF_LIB" fd_qsf/ || fail "cannot stage library $QSF_LIB"
out=$("$cli" render "fd_qsf/$(basename "$f")" --output fd_qsf.wav --seconds 300 --rate "$rate" 2>/dev/null) || fail "render failed: $f"
[ "$(field "$out" replayer)" = "qsf" ] || fail "$f: replayer=$(field "$out" replayer), want qsf"
got=$(field "$out" rendered_frames)
[ "$got" -ge $((rate * ${QSF_MIN_SECONDS:-6})) ] || fail "$f: rendered_frames=$got, want at least $((rate * ${QSF_MIN_SECONDS:-6}))"
[ -z "${QSF_MAX_SECONDS:-}" ] || [ "$got" -le $((rate * QSF_MAX_SECONDS)) ] || fail "$f: rendered_frames=$got, want at most $((rate * QSF_MAX_SECONDS))"
e0=$(energy fd_qsf.wav)
[ "$e0" -gt "${QSF_ENERGY_MIN:-1000000}" ] || fail "$f: silent output (energy $e0)"
rm -rf fd_qsf fd_qsf.wav
echo qsf_decode_ok
