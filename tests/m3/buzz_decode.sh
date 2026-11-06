#!/bin/sh
# I710d: Jeskola Buzz (.bmx) songs play through `selecta-cli render` as a RENDER-kind replayer (bbmp_buzz module: bbmp
# csrc/pc/buzz_engine.c, GPL-3.0-or-later, on BmxPlay's public-domain player). Two real songs that use only BmxPlay's machines. The
# songs have no stated redistribution licence, so they are fetched by checksum, never committed (tests/m3/fixtures/bbmp_buzz.SOURCE).
# Exit 77 = skipped (no network). Sentinel: the `replayer=bbmp_buzz` line of render. Asserts per song: the plugin ran, the WAV is not
# silent (BUZZ_ENERGY_MIN raises the floor for red proofs) and the rendered frames match the song length the engine reports
# (BUZZ_MIN_SECONDS, when set, replaces the floor, for red proofs). Also: a song that needs a Windows-only Buzz machine is not
# played by bbmp_buzz (the engine refuses it by name).
# usage: buzz_decode.sh <selecta-cli>
set -u
cli=$1
here=$(dirname "$0")
M=https://modland.com/pub/modules/Buzz
field() { printf '%s\n' "$1" | tr ' ' '\n' | sed -n "s/^$2=//p" | head -1; }
fail() { echo "FAIL: $*"; exit 1; }
energy() { od -An -v -t d2 -j 44 "$1" | tr -s " " "\n" | awk '$1 != "" { s += $1 * $1 } END { printf "%d", s }'; }
rate=44100
fetch() { "$here/fetch_fixture.sh" "$1" "$2" "$3" || exit $?; }
check() { # <song path> <min seconds>
  rm -f fd_buzz.wav
  out=$("$cli" render "$1" --output fd_buzz.wav --seconds 600 --rate "$rate" 2>/dev/null) || fail "render failed: $1"
  [ "$(field "$out" replayer)" = "bbmp_buzz" ] || fail "$1: replayer=$(field "$out" replayer), want bbmp_buzz"
  got=$(field "$out" rendered_frames)
  min=${BUZZ_MIN_SECONDS:-$2}
  [ "$got" -ge $((rate * min)) ] || fail "$1: rendered_frames=$got, want at least $((rate * min))"
  [ "$got" -le $((rate * (min + 3))) ] || fail "$1: rendered_frames=$got, far beyond the song length"
  e=$(energy fd_buzz.wav)
  [ "$e" -gt "${BUZZ_ENERGY_MIN:-1000000}" ] || fail "$1: silent output (energy $e)"
  echo "$1 frames=$got energy=$e"
  rm -f fd_buzz.wav
}
s1=$(fetch "$M/_unknown/atb-gm.bmx" eafd9eeb2e4e709c79acc31442fe7aeb6d6bf13c21eac239f9d473c79c8ff13a atb_gm.bmx) || exit $?
s2=$(fetch "$M/_unknown/axel2.bmx" 805bb0964d6bd59b37dec594cab14dfb6bfc2cca731723bce552cf7e2d7c86d2 axel2.bmx) || exit $?
s3=$(fetch "$M/And/outside%20of%20the%20dream.bmx" 691de052eb12076d8059b07352890c185a3ea680c696e1b39605f091b587a808 and_outside.bmx) || exit $?
check "$s1" 15
check "$s2" 121
# A song needing a Windows-only Buzz machine ("And XGEN") must not be played by bbmp_buzz.
out=$("$cli" render "$s3" --output fd_buzz.wav --seconds 1 --rate "$rate" 2>/dev/null) || true
[ "$(field "$out" replayer)" != "bbmp_buzz" ] || fail "bbmp_buzz played a song that needs a Windows-only machine"
rm -f fd_buzz.wav
echo buzz_decode_ok
