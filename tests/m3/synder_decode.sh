#!/bin/sh
# I710an: Synder SNG-Player songs (.sng, C64 tunes recorded as synthesizer settings) play through `selecta-cli render` as a RENDER-kind replayer (bbmp_synder
# module, GPL-3.0-or-later bbmp csrc/synder/bbmp-synder.cpp). Two plain (3 voice) and one Stereo (6 voice) song. Modules are fetched by checksum, never committed
# (tests/m3/fixtures/bbmp_synder.SOURCE). Exit 77 = skipped (no network). Sentinel: the `replayer=bbmp_synder` line of render.
# Asserts per song: the plugin ran, the WAV is not silent (SYNDER_ENERGY_MIN raises the floor, for red proofs) and the rendered frames
# match the song length (SYNDER_MIN_SECONDS, when set, replaces the floor, for red proofs).
# Negative cases (.sng is shared with AdPlug, nuade, GoatTracker and SCC-Musixx): a GoatTracker-style "GTS5" file, a zero file and a song cut off are not claimed.
# usage: synder_decode.sh <selecta-cli>
set -u
cli=$1
here=$(dirname "$0")
M="https://modland.com/pub/modules"
field() { printf '%s\n' "$1" | tr ' ' '\n' | sed -n "s/^$2=//p" | head -1; }
fail() { echo "FAIL: $*"; exit 1; }
energy() { od -An -v -t d2 -j 44 "$1" | tr -s " " "\n" | awk '$1 != "" { s += $1 * $1 } END { printf "%d", s }'; }
rate=44100
fetch() { "$here/fetch_fixture.sh" "$1" "$2" "$3" || exit $?; }
check() { # <song path> <min seconds> <max seconds>
  rm -f fd_syn.wav
  out=$("$cli" render "$1" --output fd_syn.wav --seconds 600 --rate "$rate" 2>/dev/null) || fail "render failed: $1"
  [ "$(field "$out" replayer)" = "bbmp_synder" ] || fail "$1: replayer=$(field "$out" replayer), want bbmp_synder"
  got=$(field "$out" rendered_frames)
  min=${SYNDER_MIN_SECONDS:-$2}
  [ "$got" -ge $((rate * min)) ] || fail "$1: rendered_frames=$got, want at least $((rate * min))"
  [ "$got" -le $((rate * $3)) ] || fail "$1: rendered_frames=$got, far beyond the song length"
  e=$(energy fd_syn.wav)
  [ "$e" -gt "${SYNDER_ENERGY_MIN:-1000000}" ] || fail "$1: silent output (energy $e)"
  echo "$1 frames=$got energy=$e"
  rm -f fd_syn.wav
}
s1=$(fetch "$M/Synder%20SNG-Player/Brite-Lite/last%20try.sng" f98796b3eca68857791c7069502b20da86e5051a64e3c4bbdab33977e4459e22 lasttry.sng) || exit $?
s2=$(fetch "$M/Synder%20SNG-Player/Daf/digital%20world%202.sng" d643a74aeb0dd388e496edf882c4f003839594c62458b275c569e9d100c750a8 dw2.sng) || exit $?
s3=$(fetch "$M/Synder%20SNG-Player%20Stereo/Daf/digital%20world%202.sng" d1310f2fd3e9137ce0d979fafa12e5e75e43b5c0c20ba92b3610a4b27acc3c90 dw2stereo.sng) || exit $?
check "$s1" 183 185
check "$s2" 174 176
check "$s3" 174 176
# Another .sng format (GoatTracker "GTS5" header, 20000 bytes) is not claimed.
{ printf 'GTS5'; head -c 19996 /dev/zero | tr '\0' 'A'; } > goat.sng
out=$("$cli" render goat.sng --output fd_syn.wav --seconds 1 --rate "$rate" 2>/dev/null) || true
[ "$(field "$out" replayer)" != "bbmp_synder" ] || fail "bbmp_synder claimed a GoatTracker-style .sng file"
# A zero-filled .sng is not claimed.
head -c 20000 /dev/zero > zero.sng
out=$("$cli" render zero.sng --output fd_syn.wav --seconds 1 --rate "$rate" 2>/dev/null) || true
[ "$(field "$out" replayer)" != "bbmp_synder" ] || fail "bbmp_synder claimed a zero-filled .sng file"
# A song cut off inside its packed frame data is not played by bbmp_synder.
head -c 1500 "$s2" > cutoff.sng
out=$("$cli" render cutoff.sng --output fd_syn.wav --seconds 1 --rate "$rate" 2>/dev/null) || true
[ "$(field "$out" replayer)" != "bbmp_synder" ] || fail "bbmp_synder played a song cut off inside its frame data"
rm -f goat.sng zero.sng cutoff.sng fd_syn.wav
echo synder_decode_ok
