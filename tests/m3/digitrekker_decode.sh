#!/bin/sh
# I710ap: DigiTrekker songs (.dtm, Horst Beham's DOS tracker) play through `selecta-cli render` as a RENDER-kind replayer (bbmp_digitrekker module,
# GPL-3.0-or-later bbmp csrc/pcmusic/digitrekker.cpp). Two Modland songs fetched by checksum, never committed (tests/m3/fixtures/bbmp_digitrekker.SOURCE). Exit 77 = skipped
# (no network). Sentinel: the `replayer=bbmp_digitrekker` line of render. Asserts per song: the plugin ran, the WAV is not silent (DTM_ENERGY_MIN raises the
# floor, for red proofs) and the rendered frames match the song length (DTM_MIN_SECONDS, when set, replaces the floor, for red proofs).
# Negative cases (.dtm is shared with Digital Tracker "D.T." and DeFy "DeFy"): those headers, a zero file and a song cut off are not claimed by bbmp_digitrekker.
# usage: digitrekker_decode.sh <selecta-cli>
set -u
cli=$1
here=$(dirname "$0")
M="https://modland.com/pub/modules/Digitrekker"
field() { printf '%s\n' "$1" | tr ' ' '\n' | sed -n "s/^$2=//p" | head -1; }
fail() { echo "FAIL: $*"; exit 1; }
energy() { od -An -v -t d2 -j 44 "$1" | tr -s " " "\n" | awk '$1 != "" { s += $1 * $1 } END { printf "%d", s }'; }
rate=44100
fetch() { "$here/fetch_fixture.sh" "$1" "$2" "$3" || exit $?; }
check() { # <song path> <min seconds> <max seconds>
  rm -f fd_dtm.wav
  out=$("$cli" render "$1" --output fd_dtm.wav --seconds 900 --rate "$rate" 2>/dev/null) || fail "render failed: $1"
  [ "$(field "$out" replayer)" = "bbmp_digitrekker" ] || fail "$1: replayer=$(field "$out" replayer), want bbmp_digitrekker"
  got=$(field "$out" rendered_frames)
  min=${DTM_MIN_SECONDS:-$2}
  [ "$got" -ge $((rate * min)) ] || fail "$1: rendered_frames=$got, want at least $((rate * min))"
  [ "$got" -le $((rate * $3)) ] || fail "$1: rendered_frames=$got, far beyond the song length"
  e=$(energy fd_dtm.wav)
  [ "$e" -gt "${DTM_ENERGY_MIN:-1000000}" ] || fail "$1: silent output (energy $e)"
  echo "$1 frames=$got energy=$e"
  rm -f fd_dtm.wav
}
refused() { # <file> <what>
  out=$("$cli" render "$1" --output fd_dtm.wav --seconds 1 --rate "$rate" 2>/dev/null) || true
  [ "$(field "$out" replayer)" != "bbmp_digitrekker" ] || fail "bbmp_digitrekker claimed $2"
}
s1=$(fetch "$M/Horst%20Beham/demorave.dtm" 39f0b2cf51b85cda3ec17b93908225c65180cd4f492b802489f256373b3bcbaf demorave.dtm) || exit $?
s2=$(fetch "$M/_unknown/keepcool.dtm" e0cb080daebd9d2071d280a3e78abc0d6757896daa3f3b457d7b14b236da9e55 keepcool.dtm) || exit $?
check "$s1" 120 122
check "$s2" 231 233
# Digital Tracker (libopenmpt Load_dtm) header: "D.T." magic.
{ printf 'D.T.'; head -c 19996 /dev/zero | tr '\0' 'A'; } > dt.dtm
refused dt.dtm "a Digital Tracker .dtm file"
# DeFy (adplug) shaped file: "DeFy" magic.
{ printf 'DeFy'; head -c 19996 /dev/zero | tr '\0' 'A'; } > defy.dtm
refused defy.dtm "a DeFy .dtm file"
# A zero-filled .dtm.
head -c 20000 /dev/zero > zero.dtm
refused zero.dtm "a zero-filled .dtm file"
# A song cut off (half of it): the SONG length no longer matches the file.
head -c 40000 "$s2" > cutoff.dtm
refused cutoff.dtm "a song cut off in the middle"
# Only the DigiTrekker header, no chunks behind it.
head -c 16 "$s2" > header.dtm
refused header.dtm "a bare DigiTrekker header"
rm -f dt.dtm defy.dtm zero.dtm cutoff.dtm header.dtm fd_dtm.wav
echo digitrekker_decode_ok
