#!/bin/sh
# I710r: Onyx Music Files (.omf, music disk Jangle) play through `selecta-cli render` as a RENDER-kind replayer (bbmp_onyx module,
# GPL-3.0-or-later bbmp csrc/retro/jangle_engine.cpp over libopenmpt). Three real songs (battle squadron, shadows love, laxity remix; no
# companion files). Modules are fetched by checksum, never committed
# (tests/m3/fixtures/bbmp_onyx.SOURCE). Exit 77 = skipped (no network). Sentinel: the `replayer=bbmp_onyx` line of render.
# Asserts per song: the plugin ran, the WAV is not silent (ONYX_ENERGY_MIN raises the floor, for red proofs) and the
# rendered frames match the song length the engine measures (ONYX_MIN_SECONDS, when set, replaces the floor, for red proofs).
# Also: a file that is not an Onyx Music File is not claimed, and a cut-off song is not played.
# usage: onyx_decode.sh <selecta-cli>
set -u
cli=$1
here=$(dirname "$0")
M="https://modland.com/pub/modules/Onyx%20Music%20File"
field() { printf '%s\n' "$1" | tr ' ' '\n' | sed -n "s/^$2=//p" | head -1; }
fail() { echo "FAIL: $*"; exit 1; }
energy() { od -An -v -t d2 -j 44 "$1" | tr -s " " "\n" | awk '$1 != "" { s += $1 * $1 } END { printf "%d", s }'; }
rate=44100
fetch() { "$here/fetch_fixture.sh" "$1" "$2" "$3" || exit $?; }
check() { # <song path> <min seconds> <max seconds>
  rm -f fd_onyx.wav
  out=$("$cli" render "$1" --output fd_onyx.wav --seconds 600 --rate "$rate" 2>/dev/null) || fail "render failed: $1"
  [ "$(field "$out" replayer)" = "bbmp_onyx" ] || fail "$1: replayer=$(field "$out" replayer), want bbmp_onyx"
  got=$(field "$out" rendered_frames)
  min=${ONYX_MIN_SECONDS:-$2}
  [ "$got" -ge $((rate * min)) ] || fail "$1: rendered_frames=$got, want at least $((rate * min))"
  [ "$got" -le $((rate * $3)) ] || fail "$1: rendered_frames=$got, far beyond the song length"
  e=$(energy fd_onyx.wav)
  [ "$e" -gt "${ONYX_ENERGY_MIN:-1000000}" ] || fail "$1: silent output (energy $e)"
  echo "$1 frames=$got energy=$e"
  rm -f fd_onyx.wav
}
s1=$(fetch "$M/Tirana/battle%20squadron.omf" 7e64beadf16b616c9c6cfeafdff29a63d98388744a6a949d8c9afa3fa8de2967 battle_squadron.omf) || exit $?
s2=$(fetch "$M/Tonza/shadows%20love.omf" a0faa20a7e94888d60c3730f8f348010882687d8f2365d4426da1d337a5c04c4 shadows_love.omf) || exit $?
s3=$(fetch "$M/Hydra/laxity%20remix.omf" e5bc74ba27f2f5519f7d58696b74b582fa0af5fbadcaf167bedf3e4dd8399ccb laxity_remix.omf) || exit $?
check "$s1" 56 59
check "$s2" 125 129
check "$s3" 162 166
# A file that is not an Onyx Music File is not claimed.
head -c 20000 /dev/zero | tr '\0' 'A' > notomf
out=$("$cli" render notomf --output fd_onyx.wav --seconds 1 --rate "$rate" 2>/dev/null) || true
[ "$(field "$out" replayer)" != "bbmp_onyx" ] || fail "bbmp_onyx claimed a file that is not an Onyx Music File"
# A song cut off before its samples is not played by bbmp_onyx.
head -c 3000 "$s1" > cutoff
out=$("$cli" render cutoff --output fd_onyx.wav --seconds 1 --rate "$rate" 2>/dev/null) || true
[ "$(field "$out" replayer)" != "bbmp_onyx" ] || fail "bbmp_onyx played a cut-off song"
rm -rf notomf cutoff fd_onyx.wav
echo onyx_decode_ok
