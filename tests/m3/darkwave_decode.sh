#!/bin/sh
# I710h: DarkWave Studio projects (.dwp) play through `selecta-cli render` as a RENDER-kind replayer (bbmp_darkwave module, GPL-3.0-or-later
# DarkWave Studio 4.1.6 engine). Three real projects by Daniel Werner: Chemistry 101, 202, 303. Fetched by checksum, never committed
# (tests/m3/fixtures/bbmp_darkwave.SOURCE). Exit 77 = skipped (no network). Sentinel: the `replayer=bbmp_darkwave` line of render.
# Asserts per song: the plugin ran, the WAV is not silent (DW_ENERGY_MIN raises the floor, for red proofs) and the rendered frames match
# the song length the engine measures (DW_MIN_SECONDS, when set, replaces the floor, for red proofs). The render is capped at DW_CAP
# seconds (default 33) to keep the test short: every project plays at least 30 s (short sequences are looped by the engine), so the cap
# is the length asserted. Also: a file that is not a DarkWave Studio project is not claimed.
# usage: darkwave_decode.sh <selecta-cli>
set -u
cli=$1
here=$(dirname "$0")
M=https://modland.com/pub/modules/DarkWave%20Studio/Daniel%20Werner
field() { printf '%s\n' "$1" | tr ' ' '\n' | sed -n "s/^$2=//p" | head -1; }
fail() { echo "FAIL: $*"; exit 1; }
energy() { od -An -v -t d2 -j 44 "$1" | tr -s " " "\n" | awk '$1 != "" { s += $1 * $1 } END { printf "%d", s }'; }
rate=44100
cap=${DW_CAP:-33}
fetch() { "$here/fetch_fixture.sh" "$1" "$2" "$3" || exit $?; }
check() { # <song path> <min seconds>
  rm -f fd_darkwave.wav
  out=$("$cli" render "$1" --output fd_darkwave.wav --seconds "$cap" --rate "$rate" 2>/dev/null) || fail "render failed: $1"
  [ "$(field "$out" replayer)" = "bbmp_darkwave" ] || fail "$1: replayer=$(field "$out" replayer), want bbmp_darkwave"
  got=$(field "$out" rendered_frames)
  min=${DW_MIN_SECONDS:-$2}
  [ "$got" -ge $((rate * min)) ] || fail "$1: rendered_frames=$got, want at least $((rate * min))"
  [ "$got" -le $((rate * cap)) ] || fail "$1: rendered_frames=$got, beyond the cap"
  e=$(energy fd_darkwave.wav)
  [ "$e" -gt "${DW_ENERGY_MIN:-1000000}" ] || fail "$1: silent output (energy $e)"
  echo "$1 frames=$got energy=$e"
  rm -f fd_darkwave.wav
}
s1=$(fetch "$M/chemistry%20101.dwp" e0ab34a9de1872d7b99ee5ee38c919485ff9ba24f6b5417d46e43c54ed405bac c101.dwp) || exit $?
s2=$(fetch "$M/chemistry%20202.dwp" f321b35addd8b551723396f45dff85f6183c6139a2aef36d3ce8b9f4da1b1817 c202.dwp) || exit $?
s3=$(fetch "$M/chemistry%20303.dwp" faf5142d1ba4b6f09722889b654d54b3fa45deda6430912c604689ae3abd5a56 c303.dwp) || exit $?
check "$s1" 30
check "$s2" 30
check "$s3" 30
# A file that is not a DarkWave Studio project is not claimed, whatever its name says.
head -c 70000 /dev/zero | tr '\0' 'A' > notdwp.dwp
out=$("$cli" render notdwp.dwp --output fd_darkwave.wav --seconds 1 --rate "$rate" 2>/dev/null) || true
[ "$(field "$out" replayer)" != "bbmp_darkwave" ] || fail "bbmp_darkwave claimed a file that is not a DarkWave Studio project"
rm -f notdwp.dwp fd_darkwave.wav
echo darkwave_decode_ok
