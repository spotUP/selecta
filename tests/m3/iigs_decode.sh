#!/bin/sh
# I710ad: SoundSmith (Apple IIGS) songs play through `selecta-cli render` as a RENDER-kind replayer (bbmp_iigs module,
# GPL-3.0-or-later bbmp csrc/iigs on an Ensoniq 5503 emulation). Two real songs, each with its wave bank <song>.W beside it:
# Introspection (SoundSmith) and MT Intro (MegaTracker dialect). Songs are fetched by checksum, never committed
# (tests/m3/fixtures/bbmp_iigs.SOURCE). Exit 77 = skipped (no network). Sentinel: the `replayer=bbmp_iigs` line of render.
# Asserts per song: the plugin ran, the WAV is not silent (IIGS_ENERGY_MIN raises the floor, for red proofs) and the
# rendered frames match the song length the engine measures (IIGS_MIN_SECONDS, when set, replaces the floor, for red proofs).
# Also: the song without its bank is not played by bbmp_iigs, and a file that is not SoundSmith is not claimed.
# usage: iigs_decode.sh <selecta-cli>
set -u
cli=$1
here=$(dirname "$0")
M=https://modland.com/pub/modules/SoundSmith
field() { printf '%s\n' "$1" | tr ' ' '\n' | sed -n "s/^$2=//p" | head -1; }
fail() { echo "FAIL: $*"; exit 1; }
energy() { od -An -v -t d2 -j 44 "$1" | tr -s " " "\n" | awk '$1 != "" { s += $1 * $1 } END { printf "%d", s }'; }
rate=44100
fetch() { "$here/fetch_fixture.sh" "$1" "$2" "$3" || exit $?; }
check() { # <song path> <min seconds> <max seconds>
  rm -f fd_iigs.wav
  out=$("$cli" render "$1" --output fd_iigs.wav --seconds 600 --rate "$rate" 2>/dev/null) || fail "render failed: $1"
  [ "$(field "$out" replayer)" = "bbmp_iigs" ] || fail "$1: replayer=$(field "$out" replayer), want bbmp_iigs"
  got=$(field "$out" rendered_frames)
  min=${IIGS_MIN_SECONDS:-$2}
  [ "$got" -ge $((rate * min)) ] || fail "$1: rendered_frames=$got, want at least $((rate * min))"
  [ "$got" -le $((rate * $3)) ] || fail "$1: rendered_frames=$got, far beyond the song length"
  e=$(energy fd_iigs.wav)
  [ "$e" -gt "${IIGS_ENERGY_MIN:-1000000}" ] || fail "$1: silent output (energy $e)"
  echo "$1 frames=$got energy=$e"
  rm -f fd_iigs.wav
}
s1=$(fetch "$M/Cappy/Introspection" 2790fc9cca77079f365c3b03ebb03191a89824525fd01bd1334c742aa3e05df6 introspection) || exit $?
fetch "$M/Cappy/Introspection.W" 9977c38966a04f8e6602c799bd5070fd7142375b42d4268af8a464e28526386b introspection.W >/dev/null || exit $?
s2=$(fetch "$M/Ian%20Schmidt/MT%20Intro" c459ab00b67e236b2350b9b3d8824b76914df870a83706dbbf9e815e9518cd06 mt_intro) || exit $?
fetch "$M/Ian%20Schmidt/MT%20Intro.W" 517587624096c2acd49a7f3065e8760fb8fcb24d071942812bb51fdce96fcfc0 mt_intro.W >/dev/null || exit $?
check "$s1" 120 150
check "$s2" 110 140
# Without its wave bank the song is not played by bbmp_iigs.
mkdir -p nobank && cp "$s1" nobank/introspection
out=$("$cli" render nobank/introspection --output fd_iigs.wav --seconds 1 --rate "$rate" 2>/dev/null) || true
[ "$(field "$out" replayer)" != "bbmp_iigs" ] || fail "bbmp_iigs played a song with no wave bank"
# A file that is not SoundSmith is not claimed.
head -c 20000 /dev/zero | tr '\0' 'A' > notss
out=$("$cli" render notss --output fd_iigs.wav --seconds 1 --rate "$rate" 2>/dev/null) || true
[ "$(field "$out" replayer)" != "bbmp_iigs" ] || fail "bbmp_iigs claimed a file that is not SoundSmith"
rm -rf nobank notss fd_iigs.wav
echo iigs_decode_ok
