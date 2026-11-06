#!/bin/sh
# I710t: Picatune songs (.smufi, music disk Jangle) play through `selecta-cli render` as a RENDER-kind replayer (bbmp_smufi module,
# GPL-3.0-or-later bbmp csrc/retro/smufi_engine.cpp). Three real BeRo songs (cubeman, danceordie, feelthebass; no companion files). Modules are fetched by checksum, never committed
# (tests/m3/fixtures/bbmp_smufi.SOURCE). Exit 77 = skipped (no network). Sentinel: the `replayer=bbmp_smufi` line of render.
# Asserts per song: the plugin ran, the WAV is not silent (SMUFI_ENERGY_MIN raises the floor, for red proofs) and the
# rendered frames match the song length the engine measures (SMUFI_MIN_SECONDS, when set, replaces the floor, for red proofs).
# Also: a file that is not a Picatune song is not claimed, and a cut-off song is not played.
# usage: smufi_decode.sh <selecta-cli>
set -u
cli=$1
here=$(dirname "$0")
M="https://modland.com/pub/modules/Picatune/BeRo"
field() { printf '%s\n' "$1" | tr ' ' '\n' | sed -n "s/^$2=//p" | head -1; }
fail() { echo "FAIL: $*"; exit 1; }
energy() { od -An -v -t d2 -j 44 "$1" | tr -s " " "\n" | awk '$1 != "" { s += $1 * $1 } END { printf "%d", s }'; }
rate=44100
fetch() { "$here/fetch_fixture.sh" "$1" "$2" "$3" || exit $?; }
check() { # <song path> <min seconds> <max seconds>
  rm -f fd_smufi.wav
  out=$("$cli" render "$1" --output fd_smufi.wav --seconds 600 --rate "$rate" 2>/dev/null) || fail "render failed: $1"
  [ "$(field "$out" replayer)" = "bbmp_smufi" ] || fail "$1: replayer=$(field "$out" replayer), want bbmp_smufi"
  got=$(field "$out" rendered_frames)
  min=${SMUFI_MIN_SECONDS:-$2}
  [ "$got" -ge $((rate * min)) ] || fail "$1: rendered_frames=$got, want at least $((rate * min))"
  [ "$got" -le $((rate * $3)) ] || fail "$1: rendered_frames=$got, far beyond the song length"
  e=$(energy fd_smufi.wav)
  [ "$e" -gt "${SMUFI_ENERGY_MIN:-1000000}" ] || fail "$1: silent output (energy $e)"
  echo "$1 frames=$got energy=$e"
  rm -f fd_smufi.wav
}
s1=$(fetch "$M/cubeman.smufi" 90f12e5a1a266565bf1fe8b135eb046343639c3cbef45e70708f6bec832d3f7e cubeman.smufi) || exit $?
s2=$(fetch "$M/danceordie.smufi" 5d5cef50cbf32f65a4f777952d4a178332c1a75c0d380b2d624cd2512bda101b danceordie.smufi) || exit $?
s3=$(fetch "$M/feelthebass.smufi" 7bed31bbcae62607856e026d89b477d540ed494852a60b98fa515d28637fb529 feelthebass.smufi) || exit $?
check "$s1" 70 72
check "$s2" 43 45
check "$s3" 92 94
# A file that is not a Picatune song is not claimed.
head -c 20000 /dev/zero | tr '\0' 'A' > notsmufi
out=$("$cli" render notsmufi --output fd_smufi.wav --seconds 1 --rate "$rate" 2>/dev/null) || true
[ "$(field "$out" replayer)" != "bbmp_smufi" ] || fail "bbmp_smufi claimed a file that is not a Picatune song"
# A song cut off before its samples is not played by bbmp_smufi.
head -c 1500 "$s1" > cutoff
out=$("$cli" render cutoff --output fd_smufi.wav --seconds 1 --rate "$rate" 2>/dev/null) || true
[ "$(field "$out" replayer)" != "bbmp_smufi" ] || fail "bbmp_smufi played a cut-off song"
rm -rf notsmufi cutoff fd_smufi.wav
echo smufi_decode_ok
