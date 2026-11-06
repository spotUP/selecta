#!/bin/sh
# I710z: WonderSwan (.wsr) songs play through `selecta-cli render` as a RENDER-kind replayer (bbmp_wsr module, bbmp
# csrc/console). Two real songs fetched by checksum, never committed (tests/m3/fixtures/bbmp_wsr.SOURCE). Exit 77 =
# skipped (no network). Sentinel: the `replayer=bbmp_wsr` line of render. Asserts per song: the plugin ran, the WAV is
# not silent (WSR_ENERGY_MIN raises the floor, for red proofs) and the rendered frames are within the song length (a lower
# floor of 3 s, WSR_MIN_SECONDS replaces it for red proofs; at most 606 s, the engine look limit plus fade). A 64 KiB file with the
# right extension and no WSRF footer must not be claimed.
# usage: wsr_decode.sh <selecta-cli>
set -u
cli=$1
here=$(dirname "$0")
M=https://modland.com/pub/modules/WonderSwan/_unknown
field() { printf '%s\n' "$1" | tr ' ' '\n' | sed -n "s/^$2=//p" | head -1; }
fail() { echo "FAIL: $*"; exit 1; }
energy() { od -An -v -t d2 -j 44 "$1" | tr -s " " "\n" | awk '$1 != "" { s += $1 * $1 } END { printf "%d", s }'; }
rate=44100
fetch() { "$here/fetch_fixture.sh" "$1" "$2" "$3" || exit $?; }
check() { # <song path>
  rm -f fd_wsr.wav
  out=$("$cli" render "$1" --output fd_wsr.wav --seconds 1000 --rate "$rate" 2>/dev/null) || fail "render failed: $1"
  [ "$(field "$out" replayer)" = "bbmp_wsr" ] || fail "$1: replayer=$(field "$out" replayer), want bbmp_wsr"
  got=$(field "$out" rendered_frames)
  min=${WSR_MIN_SECONDS:-3}
  [ "$got" -ge $((rate * min)) ] || fail "$1: rendered_frames=$got, want at least $((rate * min))"
  [ "$got" -le $((rate * 610)) ] || fail "$1: rendered_frames=$got, beyond the engine cap"
  e=$(energy fd_wsr.wav)
  [ "$e" -gt "${WSR_ENERGY_MIN:-1000000}" ] || fail "$1: silent output (energy $e)"
  echo "$1 frames=$got energy=$e"
  rm -f fd_wsr.wav
}
s1=$(fetch "$M/chaos%20gear.wsr" 2c8e641e940d5f4ca192e21ae7cdcd51007a12bac763a4e8eb4d9c93f8cdf943 chaosgear.wsr) || exit $?
s2=$(fetch "$M/densha%20de%20go%21.wsr" 747af1e077ac4035daf88203daafa7ba106d464f1f1f13b0edcce369978d25b4 densha.wsr) || exit $?
check "$s1"
check "$s2"
head -c 65536 /dev/zero | tr '\0' 'A' > notwsr.wsr
out=$("$cli" render notwsr.wsr --output fd_wsr.wav --seconds 1 --rate "$rate" 2>/dev/null) || true
[ "$(field "$out" replayer)" != "bbmp_wsr" ] || fail "bbmp_wsr claimed a .wsr without the WSRF footer"
rm -f notwsr.wsr fd_wsr.wav
echo wsr_decode_ok
