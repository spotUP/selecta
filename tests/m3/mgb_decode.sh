#!/bin/sh
# I710x: Gameboy Tracker songs (.mgb) play through `selecta-cli render` as a RENDER-kind replayer (bbmp_mgb module: bbmp gbt_engine.cpp, GPL-3.0,
# GBSoundSystem driver MIT, played by the existing selecta_gme). Three real songs fetched by checksum, never committed (tests/m3/fixtures/
# bbmp_mgb.SOURCE). Exit 77 = skipped (no network). Sentinel: the `replayer=bbmp_mgb` line of render. Each song's rendered frames must reach the
# per-song floor in `check` (MGB_MIN_SECONDS replaces it, for red proofs) and the WAV must not be silent (MGB_ENERGY_MIN raises the floor, for red
# proofs). A file that is not a Gameboy Tracker song is not claimed.
# usage: mgb_decode.sh <selecta-cli>
set -u
cli=$1
here=$(dirname "$0")
M=https://modland.com/pub/modules/Gameboy%20Tracker
field() { printf '%s\n' "$1" | tr ' ' '\n' | sed -n "s/^$2=//p" | head -1; }
fail() { echo "FAIL: $*"; exit 1; }
energy() { od -An -v -t d2 -j 44 "$1" | tr -s " " "\n" | awk '$1 != "" { s += $1 * $1 } END { printf "%d", s }'; }
rate=44100
cap=${MGB_CAP:-600}
fetch() { "$here/fetch_fixture.sh" "$1" "$2" "$3" || exit $?; }
check() { # <song path> <min seconds>
  rm -f fd_mgb.wav
  out=$("$cli" render "$1" --output fd_mgb.wav --seconds "$cap" --rate "$rate" 2>/dev/null) || fail "render failed: $1"
  [ "$(field "$out" replayer)" = "bbmp_mgb" ] || fail "$1: replayer=$(field "$out" replayer), want bbmp_mgb"
  got=$(field "$out" rendered_frames)
  min=${MGB_MIN_SECONDS:-$2}
  [ "$got" -ge $((rate * min)) ] || fail "$1: rendered_frames=$got, want at least $((rate * min))"
  e=$(energy fd_mgb.wav)
  [ "$e" -gt "${MGB_ENERGY_MIN:-1000000}" ] || fail "$1: silent output (energy $e)"
  echo "$1 frames=$got energy=$e"
  rm -f fd_mgb.wav
}
s1=$(fetch "$M/Heatbeat/f-fortress.mgb" f7b4b1ba2a4e0730bbec224b9f8eeb46e147d8fbfc2e4f27b0c0b04b7eb72406 f-fortress.mgb) || exit $?
s2=$(fetch "$M/Nula/chipmunks.mgb" 151ef0af4ba169f1830a45db437731f277b09f5f0bc101ce50d0fc71d161110b chipmunks.mgb) || exit $?
s3=$(fetch "$M/Heatbeat/techtoys.mgb" ec9ca2a72bd3f763c44cad9e06fdb3a5ac7bfc7c615b51d61aa293478c38ba6d techtoys.mgb) || exit $?
check "$s1" 110
check "$s2" 28
check "$s3" 110
# A file that is not a Gameboy Tracker song is not claimed, whatever its name says.
head -c 70000 /dev/zero | tr '\0' 'A' > notmgb.mgb
out=$("$cli" render notmgb.mgb --output fd_mgb.wav --seconds 1 --rate "$rate" 2>/dev/null) || true
[ "$(field "$out" replayer)" != "bbmp_mgb" ] || fail "bbmp_mgb claimed a file that is not a Gameboy Tracker song"
rm -f notmgb.mgb fd_mgb.wav
echo mgb_decode_ok
