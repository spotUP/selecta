#!/bin/sh
# I710i: JayTrax songs (.jxs) play through `selecta-cli render` as a RENDER-kind replayer (bbmp_jxs module, GPL-3.0-or-later bbmp
# csrc/synth clean-room JayTrax engine). Two real songs by Reinier van Vliet: "hakke" and "jbells". Fetched by checksum, never
# committed (tests/m3/fixtures/bbmp_jxs.SOURCE). Exit 77 = skipped (no network). Sentinel: the `replayer=bbmp_jxs` line of
# render. Asserts per song: the plugin ran, the WAV is not silent (JXS_ENERGY_MIN raises the floor, for red proofs) and the
# rendered frames match the song length the engine measures (JXS_MIN_SECONDS, when set, replaces the floor, for red proofs).
# Also: a file that is not a JayTrax song is not claimed.
# usage: jxs_decode.sh <selecta-cli>
set -u
cli=$1
here=$(dirname "$0")
M=https://modland.com/pub/modules/JayTrax
field() { printf '%s\n' "$1" | tr ' ' '\n' | sed -n "s/^$2=//p" | head -1; }
fail() { echo "FAIL: $*"; exit 1; }
energy() { od -An -v -t d2 -j 44 "$1" | tr -s " " "\n" | awk '$1 != "" { s += $1 * $1 } END { printf "%d", s }'; }
rate=44100
fetch() { "$here/fetch_fixture.sh" "$1" "$2" "$3" || exit $?; }
check() { # <song path> <min seconds> <max seconds>
  rm -f fd_jxs.wav
  out=$("$cli" render "$1" --output fd_jxs.wav --seconds 900 --rate "$rate" 2>/dev/null) || fail "render failed: $1"
  [ "$(field "$out" replayer)" = "bbmp_jxs" ] || fail "$1: replayer=$(field "$out" replayer), want bbmp_jxs"
  got=$(field "$out" rendered_frames)
  min=${JXS_MIN_SECONDS:-$2}
  [ "$got" -ge $((rate * min)) ] || fail "$1: rendered_frames=$got, want at least $((rate * min))"
  [ "$got" -le $((rate * $3)) ] || fail "$1: rendered_frames=$got, far beyond the song length"
  e=$(energy fd_jxs.wav)
  [ "$e" -gt "${JXS_ENERGY_MIN:-1000000}" ] || fail "$1: silent output (energy $e)"
  echo "$1 frames=$got energy=$e"
  rm -f fd_jxs.wav
}
s1=$(fetch "$M/Reinier%20Van%20Vliet/hakke.jxs" 18f670158a7ac622cd63881620207501a24d7aaa01d9d6c1cf2e385f454efdde hakke.jxs) || exit $?
s2=$(fetch "$M/Reinier%20Van%20Vliet/jbells.jxs" 29c6639058e1b6efc82c13f2838260289de98ae3260f8af37e71fd08c83a3686 jbells.jxs) || exit $?
check "$s1" 87 88
check "$s2" 8 9
# A file that is not a JayTrax song is not claimed, whatever its name says.
head -c 70000 /dev/zero | tr '\0' 'A' > notjxs.jxs
out=$("$cli" render notjxs.jxs --output fd_jxs.wav --seconds 1 --rate "$rate" 2>/dev/null) || true
[ "$(field "$out" replayer)" != "bbmp_jxs" ] || fail "bbmp_jxs claimed a file that is not a JayTrax song"
rm -f notjxs.jxs fd_jxs.wav
echo jxs_decode_ok
