#!/bin/sh
# I710ao: MONOTONE songs (.mon, Jim Leonard's PC speaker tracker) play through `selecta-cli render` as a RENDER-kind replayer (bbmp_monotone module,
# GPL-3.0-or-later bbmp csrc/pcmusic/monotone.cpp). Four Modland songs fetched by checksum, never committed (tests/m3/fixtures/bbmp_monotone.SOURCE). Exit 77 = skipped
# (no network). Sentinel: the `replayer=bbmp_monotone` line of render. Asserts per song: the plugin ran, the WAV is not silent (MONOTONE_ENERGY_MIN raises the
# floor, for red proofs) and the rendered frames match the song length (MONOTONE_MIN_SECONDS, when set, replaces the floor, for red proofs).
# Negative cases (.mon is shared with nuade maniacsofnoise): a file with another .mon magic, a zero file and a song cut off are not claimed by bbmp_monotone.
# usage: monotone_decode.sh <selecta-cli>
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
  rm -f fd_mon.wav
  out=$("$cli" render "$1" --output fd_mon.wav --seconds 600 --rate "$rate" 2>/dev/null) || fail "render failed: $1"
  [ "$(field "$out" replayer)" = "bbmp_monotone" ] || fail "$1: replayer=$(field "$out" replayer), want bbmp_monotone"
  got=$(field "$out" rendered_frames)
  min=${MONOTONE_MIN_SECONDS:-$2}
  [ "$got" -ge $((rate * min)) ] || fail "$1: rendered_frames=$got, want at least $((rate * min))"
  [ "$got" -le $((rate * $3)) ] || fail "$1: rendered_frames=$got, far beyond the song length"
  e=$(energy fd_mon.wav)
  [ "$e" -gt "${MONOTONE_ENERGY_MIN:-1000000}" ] || fail "$1: silent output (energy $e)"
  echo "$1 frames=$got energy=$e"
  rm -f fd_mon.wav
}
s1=$(fetch "$M/Monotone/Trixter/understa.mon" 58a992106be511c1094dc6afbd31aa4749fb50280f7ee4c19d8f20624e9bf908 understa.mon) || exit $?
s2=$(fetch "$M/Monotone/Virt/rickroll.mon" a468527517ff39ea49c62e0fba07e3babea52fafe8088001b843e3f84d6af953 rickroll.mon) || exit $?
s3=$(fetch "$M/Monotone/Yesso/prelude.mon" 85cbdc3a440df09b7744f8fbd280f6229d6a21cfb175909a60855623c7f1bf34 prelude.mon) || exit $?
s4=$(fetch "$M/Monotone/Yesso/test.mon" 7bd4a88f56196c63e7631393b394ce7e8a8834cb79cb3c1009d15170bb95fe26 test.mon) || exit $?
check "$s1" 25 26
check "$s2" 37 38
check "$s3" 100 102
check "$s4" 21 23
# Another .mon file (maniacsofnoise-shaped: other magic, 20000 bytes) is not claimed.
{ printf 'MON!'; head -c 19996 /dev/zero | tr '\0' 'A'; } > mon_other.mon
out=$("$cli" render mon_other.mon --output fd_mon.wav --seconds 1 --rate "$rate" 2>/dev/null) || true
[ "$(field "$out" replayer)" != "bbmp_monotone" ] || fail "bbmp_monotone claimed a .mon file with another magic"
# A zero-filled .mon is not claimed.
head -c 20000 /dev/zero > zero.mon
out=$("$cli" render zero.mon --output fd_mon.wav --seconds 1 --rate "$rate" 2>/dev/null) || true
[ "$(field "$out" replayer)" != "bbmp_monotone" ] || fail "bbmp_monotone claimed a zero-filled .mon file"
# A song cut off inside its header (shorter than the 351 byte header and order list) is not played by bbmp_monotone.
head -c 300 "$s2" > cutoff.mon
out=$("$cli" render cutoff.mon --output fd_mon.wav --seconds 1 --rate "$rate" 2>/dev/null) || true
[ "$(field "$out" replayer)" != "bbmp_monotone" ] || fail "bbmp_monotone played a song cut off inside its header"
rm -f mon_other.mon zero.mon cutoff.mon fd_mon.wav
echo monotone_decode_ok
