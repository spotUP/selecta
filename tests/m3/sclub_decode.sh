#!/bin/sh
# I710ac: Sound Club (.sn) and Sound Club 2 (.sn2) songs play through `selecta-cli render` as a RENDER-kind replayer (bbmp_sclub
# module, GPL-3.0-or-later bbmp csrc/misc/sclub_cr.cpp). Two real songs, one per format, neither needing Sound Club's instrument
# bank (BlueMoon's sclub.bnk is not shipped). The songs have no stated redistribution licence, so they are fetched by checksum,
# never committed (tests/m3/fixtures/bbmp_sclub.SOURCE). Exit 77 = skipped (no network). Sentinel: the `replayer=bbmp_sclub`
# line of render. Asserts per song: the plugin ran, the WAV is not silent (SCLUB_ENERGY_MIN raises the floor for red proofs) and
# the rendered frames match the song length the engine reports (SCLUB_MIN_SECONDS, when set, replaces the floor, for red proofs).
# Also: a .sn that is not Sound Club is not claimed by bbmp_sclub.
# usage: sclub_decode.sh <selecta-cli>
set -u
cli=$1
here=$(dirname "$0")
M=https://modland.com/pub/modules
field() { printf '%s\n' "$1" | tr ' ' '\n' | sed -n "s/^$2=//p" | head -1; }
fail() { echo "FAIL: $*"; exit 1; }
energy() { od -An -v -t d2 -j 44 "$1" | tr -s " " "\n" | awk '$1 != "" { s += $1 * $1 } END { printf "%d", s }'; }
rate=44100
fetch() { "$here/fetch_fixture.sh" "$1" "$2" "$3" || exit $?; }
check() { # <song path> <min seconds>
  rm -f fd_sclub.wav
  out=$("$cli" render "$1" --output fd_sclub.wav --seconds 600 --rate "$rate" 2>/dev/null) || fail "render failed: $1"
  [ "$(field "$out" replayer)" = "bbmp_sclub" ] || fail "$1: replayer=$(field "$out" replayer), want bbmp_sclub"
  got=$(field "$out" rendered_frames)
  min=${SCLUB_MIN_SECONDS:-$2}
  [ "$got" -ge $((rate * min)) ] || fail "$1: rendered_frames=$got, want at least $((rate * min))"
  [ "$got" -le $((rate * (min + 60))) ] || fail "$1: rendered_frames=$got, far beyond the song length"
  e=$(energy fd_sclub.wav)
  [ "$e" -gt "${SCLUB_ENERGY_MIN:-1000000}" ] || fail "$1: silent output (energy $e)"
  echo "$1 frames=$got energy=$e"
  rm -f fd_sclub.wav
}
s1=$(fetch "$M/Sound%20Club/Fish/motherfuckin%27xtc%21.sn" 8ad5adba24b4241df45820d03c2f738902d5f4b24af9e398f14f035f50fe129f xtc.sn) || exit $?
s2=$(fetch "$M/Sound%20Club%202/Ott%20Aaloe/country%20funk.sn2" 3c9ec5ddd2b0764afe5dd377f5753dab61c7ed6685aa4b62c19aa51bc2d7f655 country_funk.sn2) || exit $?
check "$s1" 190
check "$s2" 78
# A .sn that is not Sound Club must not be claimed by bbmp_sclub.
head -c 4096 /dev/zero | tr '\0' 'A' > notsc.sn
out=$("$cli" render notsc.sn --output fd_sclub.wav --seconds 1 --rate "$rate" 2>/dev/null) || true
[ "$(field "$out" replayer)" != "bbmp_sclub" ] || fail "bbmp_sclub claimed a .sn that is not Sound Club"
rm -f notsc.sn fd_sclub.wav
echo sclub_decode_ok
