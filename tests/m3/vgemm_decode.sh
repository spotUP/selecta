#!/bin/sh
# I710ah: VGM Music Maker songs (.vge) play through `selecta-cli render` as a RENDER-kind replayer (bbmp_vgemm module,
# GPL-3.0-or-later bbmp csrc/retro/vgemm_engine.cpp). Three real songs (Shiru, Linde, Groovemaster303; no companion files). Modules are fetched by checksum, never committed
# (tests/m3/fixtures/bbmp_vgemm.SOURCE). Exit 77 = skipped (no network). Sentinel: the `replayer=bbmp_vgemm` line of render.
# Asserts per song: the plugin ran, the WAV is not silent (VGEMM_ENERGY_MIN raises the floor, for red proofs) and the
# rendered frames match the song length the engine measures (VGEMM_MIN_SECONDS, when set, replaces the floor, for red proofs).
# Also: a file that is not a VGM Music Maker song is not claimed, and a song cut off inside its header is not played.
# usage: vgemm_decode.sh <selecta-cli>
set -u
cli=$1
here=$(dirname "$0")
M="https://modland.com/pub/modules/VGM%20Music%20Maker"
field() { printf '%s\n' "$1" | tr ' ' '\n' | sed -n "s/^$2=//p" | head -1; }
fail() { echo "FAIL: $*"; exit 1; }
energy() { od -An -v -t d2 -j 44 "$1" | tr -s " " "\n" | awk '$1 != "" { s += $1 * $1 } END { printf "%d", s }'; }
rate=44100
fetch() { "$here/fetch_fixture.sh" "$1" "$2" "$3" || exit $?; }
check() { # <song path> <min seconds> <max seconds>
  rm -f fd_vgemm.wav
  out=$("$cli" render "$1" --output fd_vgemm.wav --seconds 600 --rate "$rate" 2>/dev/null) || fail "render failed: $1"
  [ "$(field "$out" replayer)" = "bbmp_vgemm" ] || fail "$1: replayer=$(field "$out" replayer), want bbmp_vgemm"
  got=$(field "$out" rendered_frames)
  min=${VGEMM_MIN_SECONDS:-$2}
  [ "$got" -ge $((rate * min)) ] || fail "$1: rendered_frames=$got, want at least $((rate * min))"
  [ "$got" -le $((rate * $3)) ] || fail "$1: rendered_frames=$got, far beyond the song length"
  e=$(energy fd_vgemm.wav)
  [ "$e" -gt "${VGEMM_ENERGY_MIN:-1000000}" ] || fail "$1: silent output (energy $e)"
  echo "$1 frames=$got energy=$e"
  rm -f fd_vgemm.wav
}
s1=$(fetch "$M/Shiru/example%20fm%20psg%20samples.vge" c7d1303c77af2df69142bc90dbf8a809803515ad07cbeec838360f68d8d53624 example_fm_psg_samples.vge) || exit $?
s2=$(fetch "$M/Linde/trinitron%20sunset.vge" a249847ca7ed7ed1f48544d4706e01df60c657362d2ca257cf0e987fbdb015bd trinitron_sunset.vge) || exit $?
s3=$(fetch "$M/Groovemaster303/bonus%20stage%20funk.vge" 78da6fb758e64702fa4c87b5f35b84738283bd7c16373d9482ce3c592f98ea6d bonus_stage_funk.vge) || exit $?
check "$s1" 3 4
check "$s2" 109 111
check "$s3" 81 83
# A file that is not a VGM Music Maker song is not claimed.
head -c 20000 /dev/zero | tr '\0' 'A' > notvgemm
out=$("$cli" render notvgemm --output fd_vgemm.wav --seconds 1 --rate "$rate" 2>/dev/null) || true
[ "$(field "$out" replayer)" != "bbmp_vgemm" ] || fail "bbmp_vgemm claimed a file that is not a VGM Music Maker song"
# A song cut off after its header is not played by bbmp_vgemm.
head -c 100 "$s1" > cutoff
out=$("$cli" render cutoff --output fd_vgemm.wav --seconds 1 --rate "$rate" 2>/dev/null) || true
[ "$(field "$out" replayer)" != "bbmp_vgemm" ] || fail "bbmp_vgemm played a song cut off inside its header"
rm -rf notvgemm cutoff fd_vgemm.wav
echo vgemm_decode_ok
