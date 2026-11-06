#!/bin/sh
# I710al: GoatTracker 1 and 2 songs (.sng, C64 SID) play through `selecta-cli render` as a RENDER-kind replayer (goattracker module:
# GoatTracker's own play routine and reSID, Selecta glue GPL-2.0-or-later). Two GT2 ("GTS2") and one GT1 ("GTS!") song. Modules are fetched
# by checksum, never committed (tests/m3/fixtures/goattracker.SOURCE). Exit 77 = skipped (no network). Sentinel: the `replayer=goattracker`
# line of render. Asserts per song: the plugin ran, the WAV is not silent (GT_ENERGY_MIN raises the floor, for red proofs) and the rendered
# frames match the song length found by loop detection (GT_MIN_SECONDS, when set, replaces the floor, for red proofs).
# Negative cases (.sng is shared with AdPlug, nuade and SCC-Musixx): an SCC-Musixx-shaped file, a zero file, a GoatTracker header on junk
# and a song cut off inside its patterns are not claimed.
# usage: goattracker_decode.sh <selecta-cli>
set -u
cli=$1
here=$(dirname "$0")
field() { printf '%s\n' "$1" | tr ' ' '\n' | sed -n "s/^$2=//p" | head -1; }
fail() { echo "FAIL: $*"; exit 1; }
energy() { od -An -v -t d2 -j 44 "$1" | tr -s " " "\n" | awk '$1 != "" { s += $1 * $1 } END { printf "%d", s }'; }
rate=44100
fetch() { "$here/fetch_fixture.sh" "$1" "$2" "$3" || exit $?; }
check() { # <song path> <min seconds> <max seconds>
  rm -f fd_gt.wav
  out=$("$cli" render "$1" --output fd_gt.wav --seconds 600 --rate "$rate" 2>/dev/null) || fail "render failed: $1"
  [ "$(field "$out" replayer)" = "goattracker" ] || fail "$1: replayer=$(field "$out" replayer), want goattracker"
  got=$(field "$out" rendered_frames)
  min=${GT_MIN_SECONDS:-$2}
  [ "$got" -ge $((rate * min)) ] || fail "$1: rendered_frames=$got, want at least $((rate * min))"
  [ "$got" -le $((rate * $3)) ] || fail "$1: rendered_frames=$got, far beyond the song length"
  e=$(energy fd_gt.wav)
  [ "$e" -gt "${GT_ENERGY_MIN:-1000000}" ] || fail "$1: silent output (energy $e)"
  echo "$1 frames=$got energy=$e"
  rm -f fd_gt.wav
}
G2="https://modland.com/pub/modules/GoatTracker%202"
s1=$(fetch "$G2/Cadaver/dojo.sng" 0dc659f11a7fbf1c0199d42e4b6827a3cf9bd67b6bc1f8930ecdace3aea330c5 dojo.sng) || exit $?
s2=$(fetch "$G2/Jammer/my%20own%20hyperspace.sng" 8f888cf8aa157d8bfd0a7b871aaca7e4fc512456ba5c91d21bc776a3fa26cf04 hyperspace.sng) || exit $?
s3=$(fetch "https://modland.com/pub/modules/GoatTracker/Aeuk/metal%20warrior%204%20-%20streets.sng" 34da331cf5ecacc7150caca940e5a631daf3fd871a02acee5e4758c2fa64a4ad streets.sng) || exit $?
check "$s1" 161 165
check "$s2" 251 256
check "$s3" 215 220
claimed() { # <file> <label>
  out=$("$cli" render "$1" --output fd_gt.wav --seconds 1 --rate "$rate" 2>/dev/null) || true
  [ "$(field "$out" replayer)" != "goattracker" ] || fail "goattracker claimed $2"
}
# An SCC-Musixx-shaped file (no GoatTracker magic) is not claimed.
{ printf 'SNG\001'; head -c 19996 /dev/zero | tr '\0' 'A'; } > scc.sng
claimed scc.sng "an SCC-Musixx-shaped .sng file"
# A zero-filled .sng is not claimed.
head -c 20000 /dev/zero > zero.sng
claimed zero.sng "a zero-filled .sng file"
# A GoatTracker magic on junk fails the structure check.
{ printf 'GTS2'; head -c 9000 /dev/zero | tr '\0' 'A'; } > junk.sng
claimed junk.sng "junk behind a GTS2 header"
# Songs cut off inside their orders or patterns are not claimed, GT2 and GT1.
head -c 3000 "$s1" > cut2.sng
claimed cut2.sng "a GT2 song cut off inside its patterns"
head -c 3000 "$s3" > cut1.sng
claimed cut1.sng "a GT1 song cut off inside its patterns"
rm -f scc.sng zero.sng junk.sng cut2.sng cut1.sng fd_gt.wav
echo goattracker_decode_ok
