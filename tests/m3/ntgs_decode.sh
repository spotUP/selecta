#!/bin/sh
# I710p: Noise Tracker GS 1 (Apple IIGS) modules play through `selecta-cli render` as a RENDER-kind replayer (bbmp_ntgs module,
# GPL-3.0-or-later bbmp csrc/iigs/ntgs_cr.cpp on an Ensoniq 5503 emulation). Three real modules (Megamisque, Modulae, Chicago Song; no
# extension, no companion files). Modules are fetched by checksum, never committed
# (tests/m3/fixtures/bbmp_ntgs.SOURCE). Exit 77 = skipped (no network). Sentinel: the `replayer=bbmp_ntgs` line of render.
# Asserts per song: the plugin ran, the WAV is not silent (NTGS_ENERGY_MIN raises the floor, for red proofs) and the
# rendered frames match the song length the engine measures (NTGS_MIN_SECONDS, when set, replaces the floor, for red proofs).
# Also: a file that is not a Noise Tracker GS module is not claimed, and a cut-off module is not played.
# usage: ntgs_decode.sh <selecta-cli>
set -u
cli=$1
here=$(dirname "$0")
M="https://modland.com/pub/modules/Noise%20Tracker%20GS%201"
field() { printf '%s\n' "$1" | tr ' ' '\n' | sed -n "s/^$2=//p" | head -1; }
fail() { echo "FAIL: $*"; exit 1; }
energy() { od -An -v -t d2 -j 44 "$1" | tr -s " " "\n" | awk '$1 != "" { s += $1 * $1 } END { printf "%d", s }'; }
rate=44100
fetch() { "$here/fetch_fixture.sh" "$1" "$2" "$3" || exit $?; }
check() { # <song path> <min seconds> <max seconds>
  rm -f fd_ntgs.wav
  out=$("$cli" render "$1" --output fd_ntgs.wav --seconds 600 --rate "$rate" 2>/dev/null) || fail "render failed: $1"
  [ "$(field "$out" replayer)" = "bbmp_ntgs" ] || fail "$1: replayer=$(field "$out" replayer), want bbmp_ntgs"
  got=$(field "$out" rendered_frames)
  min=${NTGS_MIN_SECONDS:-$2}
  [ "$got" -ge $((rate * min)) ] || fail "$1: rendered_frames=$got, want at least $((rate * min))"
  [ "$got" -le $((rate * $3)) ] || fail "$1: rendered_frames=$got, far beyond the song length"
  e=$(energy fd_ntgs.wav)
  [ "$e" -gt "${NTGS_ENERGY_MIN:-1000000}" ] || fail "$1: silent output (energy $e)"
  echo "$1 frames=$got energy=$e"
  rm -f fd_ntgs.wav
}
s1=$(fetch "$M/FTA/Megamisque" 0dc535c4800508d1845dff3b3f30ffa7d3a224f8152760cf80d79a9f57e5404a megamisque) || exit $?
s2=$(fetch "$M/FTA/Modulae" 1213fcf45b1a33bf4b4023a687094bc6aacce5a6bb460e8bee88568283da956d modulae) || exit $?
s3=$(fetch "$M/David%20Sanborn/Chicago%20Song" 56faa6be744daa2ef1ec010ac227420b84fe868f72c8c89f6d8f7a99aca9c94e chicago_song) || exit $?
check "$s1" 160 166
check "$s2" 522 530
check "$s3" 236 242
# A file that is not Noise Tracker GS is not claimed.
head -c 20000 /dev/zero | tr '\0' 'A' > notnt
out=$("$cli" render notnt --output fd_ntgs.wav --seconds 1 --rate "$rate" 2>/dev/null) || true
[ "$(field "$out" replayer)" != "bbmp_ntgs" ] || fail "bbmp_ntgs claimed a file that is not Noise Tracker GS"
# A module cut off before its instruments is not played by bbmp_ntgs.
head -c 5000 "$s1" > cutoff
out=$("$cli" render cutoff --output fd_ntgs.wav --seconds 1 --rate "$rate" 2>/dev/null) || true
[ "$(field "$out" replayer)" != "bbmp_ntgs" ] || fail "bbmp_ntgs played a cut-off module"
rm -rf notnt cutoff fd_ntgs.wav
echo ntgs_decode_ok
