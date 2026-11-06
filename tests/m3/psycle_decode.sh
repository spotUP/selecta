#!/bin/sh
# I710u: Psycle (.psy) songs play through `selecta-cli render` as a RENDER-kind replayer (bbmp_psycle module: Psycle r12005
# engine plus native plugins run by a helper child process, GPL). Two real songs by Alk, native machines only, fetched by
# checksum, never committed (tests/m3/fixtures/bbmp_psycle.SOURCE). Exit 77 = skipped (no network).
# Asserts per song: sentinel `replayer=bbmp_psycle`, the WAV is not silent (PSYCLE_ENERGY_MIN raises the floor for red
# proofs), and rendered frames equal the song length from 2 s below to 8 s above (the helper plays the reverb tail out) (PSYCLE_MIN_SECONDS, when set, replaces the floor).
# Layout: PSYCLE_ISOLATED=<dir> renders with SELECTA_PLUGIN_PATH=<dir> only (red proof: an empty dir must fail).
# usage: psycle_decode.sh <selecta-cli>
set -u
cli=$1
here=$(dirname "$0")
M=https://modland.com/pub/modules/Psycle/Alk
field() { printf '%s\n' "$1" | tr ' ' '\n' | sed -n "s/^$2=//p" | head -1; }
fail() { echo "FAIL: $*"; exit 1; }
energy() { od -An -v -t d2 -j 44 "$1" | tr -s " " "\n" | awk '$1 != "" { s += $1 * $1 } END { printf "%d", s }'; }
rate=44100
fetch() { "$here/fetch_fixture.sh" "$1" "$2" "$3" || exit $?; }
check() { # <song path> <seconds>
  rm -f fd_psy.wav
  out=$("$cli" render "$1" --output fd_psy.wav --seconds 600 --rate "$rate" 2>/dev/null) || fail "render failed: $1"
  [ "$(field "$out" replayer)" = "bbmp_psycle" ] || fail "$1: replayer=$(field "$out" replayer), want bbmp_psycle"
  got=$(field "$out" rendered_frames)
  min=${PSYCLE_MIN_SECONDS:-$(($2 - 2))}
  [ "$got" -ge $((rate * min)) ] || fail "$1: rendered_frames=$got, want at least $((rate * min))"
  [ "$got" -le $((rate * ($2 + 8))) ] || fail "$1: rendered_frames=$got, beyond the song length"
  e=$(energy fd_psy.wav)
  [ "$e" -gt "${PSYCLE_ENERGY_MIN:-1000000}" ] || fail "$1: silent output (energy $e)"
  echo "$1 frames=$got energy=$e"
  rm -f fd_psy.wav
}
s1=$(fetch "$M/8bit05.psy" 1750bae337c1d3d23a5d5f84bc5fc403ed0e7d05943193227366f41269f07fca 8bit05.psy) || exit $?
s2=$(fetch "$M/acidate.psy" 4ac811a8ea0392b9dfc85a3e9677aed86efbc33739649a9efa14a6f7c96c13c7 acidate.psy) || exit $?
[ -z "${PSYCLE_ISOLATED:-}" ] || export SELECTA_PLUGIN_PATH=$PSYCLE_ISOLATED
check "$s1" 85
check "$s2" 54
echo psycle_decode_ok
