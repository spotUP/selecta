#!/bin/sh
# I710y: PlayStation SPU logs (.spu) play through `selecta-cli render` as a RENDER-kind replayer (bbmp_spu module, GPL-2.0-or-later
# bbmp csrc/console SPU engine on the P.E.Op.S. SPU of the Audio Overload SDK). Two real logs (old log form): Depth "peaceall" and
# Dezaemon Plus "dezaemon17". Fetched by checksum, never committed (tests/m3/fixtures/bbmp_spu.SOURCE). Exit 77 = skipped (no
# network). Sentinel: the `replayer=bbmp_spu` line of render. Asserts per song: the plugin ran, the WAV is not silent
# (SPU_ENERGY_MIN raises the floor, for red proofs) and the rendered frames match the song length the engine measures
# (SPU_MIN_SECONDS, when set, replaces the floor, for red proofs). Also: a file that is not an SPU log is not claimed.
# usage: spu_decode.sh <selecta-cli>
set -u
cli=$1
here=$(dirname "$0")
M=https://modland.com/pub/modules/SPU/_unknown
field() { printf '%s\n' "$1" | tr ' ' '\n' | sed -n "s/^$2=//p" | head -1; }
fail() { echo "FAIL: $*"; exit 1; }
energy() { od -An -v -t d2 -j 44 "$1" | tr -s " " "\n" | awk '$1 != "" { s += $1 * $1 } END { printf "%d", s }'; }
rate=44100
fetch() { "$here/fetch_fixture.sh" "$1" "$2" "$3" || exit $?; }
check() { # <song path> <min seconds> <max seconds>
  rm -f fd_spu.wav
  out=$("$cli" render "$1" --output fd_spu.wav --seconds 900 --rate "$rate" 2>/dev/null) || fail "render failed: $1"
  [ "$(field "$out" replayer)" = "bbmp_spu" ] || fail "$1: replayer=$(field "$out" replayer), want bbmp_spu"
  got=$(field "$out" rendered_frames)
  min=${SPU_MIN_SECONDS:-$2}
  [ "$got" -ge $((rate * min)) ] || fail "$1: rendered_frames=$got, want at least $((rate * min))"
  [ "$got" -le $((rate * $3)) ] || fail "$1: rendered_frames=$got, far beyond the song length"
  e=$(energy fd_spu.wav)
  [ "$e" -gt "${SPU_ENERGY_MIN:-1000000}" ] || fail "$1: silent output (energy $e)"
  echo "$1 frames=$got energy=$e"
  rm -f fd_spu.wav
}
s1=$(fetch "$M/Depth/depth-peaceall.spu" abea883de48226503a9e7be4cccbf2efb0be6b48d8036883026b7fc003ef4f12 depth-peaceall.spu) || exit $?
s2=$(fetch "$M/Dezaemon%20Plus/dezaemon17.spu" f634f13eb9f37d2f19a2e45ed4022c61cfbf3b28a790bed23d5159f58747b405 dezaemon17.spu) || exit $?
check "$s1" 122 130
check "$s2" 65 72
# A file that is not an SPU log is not claimed, whatever its name says.
head -c 700000 /dev/zero | tr '\0' 'A' > notspu.spu
out=$("$cli" render notspu.spu --output fd_spu.wav --seconds 1 --rate "$rate" 2>/dev/null) || true
[ "$(field "$out" replayer)" != "bbmp_spu" ] || fail "bbmp_spu claimed a file that is not an SPU log"
rm -f notspu.spu fd_spu.wav
echo spu_decode_ok
