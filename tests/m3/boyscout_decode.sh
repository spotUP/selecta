#!/bin/sh
# I710c: BoyScout songs (.bsf) play through `selecta-cli render` as a RENDER-kind replayer (bbmp_boyscout module, GPL-3.0-or-later bbmp
# csrc/retro clean-room BoyScout engine). Two real songs: "ladybug" and "numb" (Aegis). Fetched by checksum, never committed
# (tests/m3/fixtures/bbmp_boyscout.SOURCE). Exit 77 = skipped (no network). Sentinel: the `replayer=bbmp_boyscout` line of render.
# Asserts per song: the plugin ran, the WAV is not silent (BS_ENERGY_MIN raises the floor, for red proofs) and the rendered frames
# match the song length measured at the engine (BS_MIN_SECONDS, when set, replaces the floor, for red proofs).
# Also: a file that is not a BoyScout song is not claimed.
# usage: boyscout_decode.sh <selecta-cli>
set -u
cli=$1
here=$(dirname "$0")
M=https://modland.com/pub/modules/BoyScout/Aegis
field() { printf '%s\n' "$1" | tr ' ' '\n' | sed -n "s/^$2=//p" | head -1; }
fail() { echo "FAIL: $*"; exit 1; }
energy() { od -An -v -t d2 -j 44 "$1" | tr -s " " "\n" | awk '$1 != "" { s += $1 * $1 } END { printf "%d", s }'; }
rate=44100
fetch() { "$here/fetch_fixture.sh" "$1" "$2" "$3" || exit $?; }
check() { # <song path> <min seconds> <max seconds>
  rm -f fd_bs.wav
  out=$("$cli" render "$1" --output fd_bs.wav --seconds 900 --rate "$rate" 2>/dev/null) || fail "render failed: $1"
  [ "$(field "$out" replayer)" = "bbmp_boyscout" ] || fail "$1: replayer=$(field "$out" replayer), want bbmp_boyscout"
  got=$(field "$out" rendered_frames)
  min=${BS_MIN_SECONDS:-$2}
  [ "$got" -ge $((rate * min)) ] || fail "$1: rendered_frames=$got, want at least $((rate * min))"
  [ "$got" -le $((rate * $3)) ] || fail "$1: rendered_frames=$got, far beyond the song length"
  e=$(energy fd_bs.wav)
  [ "$e" -gt "${BS_ENERGY_MIN:-1000000}" ] || fail "$1: silent output (energy $e)"
  echo "$1 frames=$got energy=$e"
  rm -f fd_bs.wav
}
s1=$(fetch "$M/ladybug.bsf" 6853b4e5fa30ab0c56cfd54db2b867ace832c09f8e6b51f091a79196fcc205f0 ladybug.bsf) || exit $?
s2=$(fetch "$M/numb.bsf" 0449a089d26347803386e6605d70fb6caeae26582af62ddfc851e4ffe09bab81 numb.bsf) || exit $?
check "$s1" 186 187
check "$s2" 91 92
# A file that is not a BoyScout song is not claimed, whatever its name says.
head -c 70000 /dev/zero | tr '\0' 'A' > notbs.bsf
out=$("$cli" render notbs.bsf --output fd_bs.wav --seconds 1 --rate "$rate" 2>/dev/null) || true
[ "$(field "$out" replayer)" != "bbmp_boyscout" ] || fail "bbmp_boyscout claimed a file that is not a BoyScout song"
# The same with the version number in front and nothing behind it.
{ printf '\1\0\0\0'; head -c 2000 /dev/zero; } > hollow.bsf
out=$("$cli" render hollow.bsf --output fd_bs.wav --seconds 1 --rate "$rate" 2>/dev/null) || true
[ "$(field "$out" replayer)" != "bbmp_boyscout" ] || fail "bbmp_boyscout claimed a version header with no song behind it"
rm -f notbs.bsf hollow.bsf fd_bs.wav
echo boyscout_decode_ok
