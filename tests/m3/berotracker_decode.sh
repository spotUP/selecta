#!/bin/sh
# I710b: BeRoTracker songs (.brt) play through `selecta-cli render` as a RENDER-kind replayer (bbmp_berotracker module, GPL-3.0-or-later bbmp
# csrc/berotracker clean-room engine). Two real songs by BeRo: "aa2s" and "b2". Fetched by checksum, never
# committed (tests/m3/fixtures/bbmp_berotracker.SOURCE). Exit 77 = skipped (no network). Sentinel: the `replayer=bbmp_berotracker` line of
# render. Asserts per song: the plugin ran, the WAV is not silent (BRT_ENERGY_MIN raises the floor, for red proofs) and the
# rendered frames match the song length the engine measures (BRT_MIN_SECONDS, when set, replaces the floor, for red proofs).
# Also: a file that is not a BeRoTracker song is not claimed.
# usage: berotracker_decode.sh <selecta-cli>
set -u
cli=$1
here=$(dirname "$0")
M=https://modland.com/pub/modules/BeRoTracker/BeRo
field() { printf '%s\n' "$1" | tr ' ' '\n' | sed -n "s/^$2=//p" | head -1; }
fail() { echo "FAIL: $*"; exit 1; }
energy() { od -An -v -t d2 -j 44 "$1" | tr -s " " "\n" | awk '$1 != "" { s += $1 * $1 } END { printf "%d", s }'; }
rate=44100
fetch() { "$here/fetch_fixture.sh" "$1" "$2" "$3" || exit $?; }
check() { # <song path> <min seconds> <max seconds>
  rm -f fd_berotracker.wav
  out=$("$cli" render "$1" --output fd_berotracker.wav --seconds 900 --rate "$rate" 2>/dev/null) || fail "render failed: $1"
  [ "$(field "$out" replayer)" = "bbmp_berotracker" ] || fail "$1: replayer=$(field "$out" replayer), want bbmp_berotracker"
  got=$(field "$out" rendered_frames)
  min=${BRT_MIN_SECONDS:-$2}
  [ "$got" -ge $((rate * min)) ] || fail "$1: rendered_frames=$got, want at least $((rate * min))"
  [ "$got" -le $((rate * $3)) ] || fail "$1: rendered_frames=$got, far beyond the song length"
  e=$(energy fd_berotracker.wav)
  [ "$e" -gt "${BRT_ENERGY_MIN:-1000000}" ] || fail "$1: silent output (energy $e)"
  echo "$1 frames=$got energy=$e"
  rm -f fd_berotracker.wav
}
s1=$(fetch "$M/aa2s.brt" ae3917b011bafbac1d486bcaa47e76a107a01ed8871a62ef573b640289980da7 aa2s.brt) || exit $?
s2=$(fetch "$M/b2.brt" c5097a3e2964c2b81f115a23b6a8b6fec9a6bd5107b2df85a650a2bcfc75a4be b2.brt) || exit $?
check "$s1" 7 8
check "$s2" 6 7
# A file that is not a BeRoTracker song is not claimed, whatever its name says.
head -c 70000 /dev/zero | tr '\0' 'A' > notbrt.brt
out=$("$cli" render notbrt.brt --output fd_berotracker.wav --seconds 1 --rate "$rate" 2>/dev/null) || true
[ "$(field "$out" replayer)" != "bbmp_berotracker" ] || fail "bbmp_berotracker claimed a file that is not a BeRoTracker song"
rm -f notbrt.brt fd_berotracker.wav
echo berotracker_decode_ok
