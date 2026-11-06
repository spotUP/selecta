#!/bin/sh
# I710n: Musicline Editor (.ml) songs play through `selecta-cli render` as a RENDER-kind replayer (bbmp_musicline module, bbmp
# csrc/musicline). Two real songs fetched by checksum, never committed (tests/m3/fixtures/bbmp_musicline.SOURCE). Exit 77 =
# skipped (no network). Sentinel: the `replayer=bbmp_musicline` line of render. Asserts per song: the plugin ran, the WAV is
# not silent (ML_ENERGY_MIN raises the floor, for red proofs) and the rendered frames are within the song length (a lower
# floor of 10 s, ML_MIN_SECONDS replaces it for red proofs; at most 900 s, the engine's cap). A file that is not a Musicline
# module (right extension, wrong content) must not be claimed.
# usage: musicline_decode.sh <selecta-cli>
set -u
cli=$1
here=$(dirname "$0")
M=https://modland.com/pub/modules/Musicline%20Editor
field() { printf '%s\n' "$1" | tr ' ' '\n' | sed -n "s/^$2=//p" | head -1; }
fail() { echo "FAIL: $*"; exit 1; }
energy() { od -An -v -t d2 -j 44 "$1" | tr -s " " "\n" | awk '$1 != "" { s += $1 * $1 } END { printf "%d", s }'; }
rate=44100
fetch() { "$here/fetch_fixture.sh" "$1" "$2" "$3" || exit $?; }
check() { # <song path>
  rm -f fd_ml.wav
  out=$("$cli" render "$1" --output fd_ml.wav --seconds 1000 --rate "$rate" 2>/dev/null) || fail "render failed: $1"
  [ "$(field "$out" replayer)" = "bbmp_musicline" ] || fail "$1: replayer=$(field "$out" replayer), want bbmp_musicline"
  got=$(field "$out" rendered_frames)
  min=${ML_MIN_SECONDS:-10}
  [ "$got" -ge $((rate * min)) ] || fail "$1: rendered_frames=$got, want at least $((rate * min))"
  [ "$got" -le $((rate * 906)) ] || fail "$1: rendered_frames=$got, beyond the engine cap"
  e=$(energy fd_ml.wav)
  [ "$e" -gt "${ML_ENERGY_MIN:-1000000}" ] || fail "$1: silent output (energy $e)"
  echo "$1 frames=$got energy=$e"
  rm -f fd_ml.wav
}
s1=$(fetch "$M/ADE/cybernoid%202.ml" e25fda74e8d21f940a3833be71cdb2af99a9279a42077367e6cac27f46338a3f cybernoid2.ml) || exit $?
s2=$(fetch "$M/Andrew/lonelyness.ml" eb87272d596e6edfb6c65035372052a5c36a6bdb4f6a5ff9b89c5aa006a3b591 lonelyness.ml) || exit $?
check "$s1"
check "$s2"
head -c 4096 /dev/zero | tr '\0' 'A' > notml.ml
out=$("$cli" render notml.ml --output fd_ml.wav --seconds 1 --rate "$rate" 2>/dev/null) || true
[ "$(field "$out" replayer)" != "bbmp_musicline" ] || fail "bbmp_musicline claimed a .ml that is not Musicline"
rm -f notml.ml fd_ml.wav
echo musicline_decode_ok
