#!/bin/sh
# I710m: MikMod UNITRK songs (.uni) play through `selecta-cli render` as a RENDER-kind replayer (bbmp_unitrk module, libmikmod 3.3.14, LGPL 2.1).
# Three real songs (UN04, UN05 headers). Fetched by checksum, never committed (tests/m3/fixtures/bbmp_unitrk.SOURCE). Exit 77 = skipped (no
# network). Sentinel: the `replayer=bbmp_unitrk` line of render. Asserts per song: the plugin ran, the WAV is not silent (UN_ENERGY_MIN raises
# the floor, for red proofs) and the rendered frames reach the song length libmikmod measures when it plays the song once (UN_MIN_SECONDS, when
# set, replaces the floor, for red proofs). The render is capped at UN_CAP seconds (default 200). Also: a file that is not UNITRK is not claimed.
# usage: unitrk_decode.sh <selecta-cli>
set -u
cli=$1
here=$(dirname "$0")
M=https://modland.com/pub/modules/MikMod%20UNITRK
field() { printf '%s\n' "$1" | tr ' ' '\n' | sed -n "s/^$2=//p" | head -1; }
fail() { echo "FAIL: $*"; exit 1; }
energy() { od -An -v -t d2 -j 44 "$1" | tr -s " " "\n" | awk '$1 != "" { s += $1 * $1 } END { printf "%d", s }'; }
rate=44100
cap=${UN_CAP:-200}
fetch() { "$here/fetch_fixture.sh" "$1" "$2" "$3" || exit $?; }
check() { # <song path> <min seconds>
  rm -f fd_unitrk.wav
  out=$("$cli" render "$1" --output fd_unitrk.wav --seconds "$cap" --rate "$rate" 2>/dev/null) || fail "render failed: $1"
  [ "$(field "$out" replayer)" = "bbmp_unitrk" ] || fail "$1: replayer=$(field "$out" replayer), want bbmp_unitrk"
  got=$(field "$out" rendered_frames)
  min=${UN_MIN_SECONDS:-$2}
  [ "$got" -ge $((rate * min)) ] || fail "$1: rendered_frames=$got, want at least $((rate * min))"
  [ "$got" -le $((rate * cap)) ] || fail "$1: rendered_frames=$got, beyond the cap"
  e=$(energy fd_unitrk.wav)
  [ "$e" -gt "${UN_ENERGY_MIN:-1000000}" ] || fail "$1: silent output (energy $e)"
  echo "$1 frames=$got energy=$e"
  rm -f fd_unitrk.wav
}
s1=$(fetch "$M/Croaker/mulla%20on%20porkkanaa.uni" 599af18e7521f964a3f4897eabb3c14f7738d2aab9992fc313f863a4af70cf30 porkkanaa.uni) || exit $?
s2=$(fetch "$M/Gatekeeper/leroy%20-%20intro.uni" b042e32e5358172cc8fef8df7226e6e6b89ed2a1a2b30a336d271d2e8c950a9f leroy_intro.uni) || exit $?
s3=$(fetch "$M/Javier%20Tienda/fray%20luis%20-%20cuatra.uni" 0371439e43d382b62ca8a159f1d002bc4ecc4778c8ac60cfbfd4c1c1fb79eb19 cuatra.uni) || exit $?
check "$s1" 195
check "$s2" 61
check "$s3" 26
# A file that is not UNITRK is not claimed, whatever its name says.
head -c 70000 /dev/zero | tr '\0' 'A' > notuni.uni
out=$("$cli" render notuni.uni --output fd_unitrk.wav --seconds 1 --rate "$rate" 2>/dev/null) || true
[ "$(field "$out" replayer)" != "bbmp_unitrk" ] || fail "bbmp_unitrk claimed a file that is not UNITRK"
rm -f notuni.uni fd_unitrk.wav
echo unitrk_decode_ok
