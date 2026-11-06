#!/bin/sh
# I710w: SGC and GBR songs play through `selecta-cli render` as a RENDER-kind replayer (bbmp_nez module, libnezplug, Unlicense / MIT-0, vendored in
# third_party/libnezplug). Four real songs fetched by checksum, never committed (tests/m3/fixtures/bbmp_nez.SOURCE). Exit 77 = skipped (no network).
# Sentinel: the `replayer=bbmp_nez` line of render. Neither format has a song length, the plugin plays 180 s: the rendered frames must reach
# the per-song floor in `check` (NEZ_MIN_SECONDS replaces it, for red proofs), the WAV must not be silent (NEZ_ENERGY_MIN raises the floor, for red proofs).
# A file that is not an SGC/GBR song is not claimed.
# usage: nez_decode.sh <selecta-cli>
set -u
cli=$1
here=$(dirname "$0")
M=https://modland.com/pub/modules
field() { printf '%s\n' "$1" | tr ' ' '\n' | sed -n "s/^$2=//p" | head -1; }
fail() { echo "FAIL: $*"; exit 1; }
energy() { od -An -v -t d2 -j 44 "$1" | tr -s " " "\n" | awk '$1 != "" { s += $1 * $1 } END { printf "%d", s }'; }
rate=44100
cap=${NEZ_CAP:-600}
fetch() { "$here/fetch_fixture.sh" "$1" "$2" "$3" || exit $?; }
check() { # <song path> <min seconds>
  rm -f fd_nez.wav
  out=$("$cli" render "$1" --output fd_nez.wav --seconds "$cap" --rate "$rate" 2>/dev/null) || fail "render failed: $1"
  [ "$(field "$out" replayer)" = "bbmp_nez" ] || fail "$1: replayer=$(field "$out" replayer), want bbmp_nez"
  got=$(field "$out" rendered_frames)
  min=${NEZ_MIN_SECONDS:-$2}
  [ "$got" -ge $((rate * min)) ] || fail "$1: rendered_frames=$got, want at least $((rate * min))"
  [ "$got" -le $((rate * 181)) ] || fail "$1: rendered_frames=$got, beyond the 180 s length"
  e=$(energy fd_nez.wav)
  [ "$e" -gt "${NEZ_ENERGY_MIN:-1000000}" ] || fail "$1: silent output (energy $e)"
  echo "$1 frames=$got energy=$e"
  rm -f fd_nez.wav
}
s1=$(fetch "$M/SGC/Aki%20Hata/Dynamite%20Headdy.sgc" d5e9f3cb535747ce7cc0bd61a634a25f5d9d437cd162e5d0cf02ba91215f5e3c headdy.sgc) || exit $?
s2=$(fetch "$M/SGC/Alex%20Mauer/Pocket%20Parafoil%20-%20Alex%20Mauer.sgc" 8ef35dac20b5a769af462d408dfc3db455fba8548db6c4cf49aea785edb00e42 parafoil.sgc) || exit $?
s3=$(fetch "$M/Gameboy%20Sound%20System%20GBR/Koichi%20Sugiyama/dragon%20quest%203.gbr" 67df14557c68b013761523a0fa7d7ca6a966391ed856a0d085fabfd87c88db3f dq3.gbr) || exit $?
s4=$(fetch "$M/Gameboy%20Sound%20System%20GBR/Koichi%20Sugiyama/dragon%20quest%201%20%26%202.gbr" 9033e20fcdc567992ddaca1dbfcafcf234dce4c7912e253682d675007b5b3cee dq12.gbr) || exit $?
# The engine ends a song at its silence policy, so a song that falls silent early is shorter than the 180 s length: the SGC songs run the
# full length, dragon quest 3 about 106 s, dragon quest 1 and 2 (first song) about 5 s.
check "$s1" 170
check "$s2" 170
check "$s3" 100
check "$s4" 4
# A file that is not an SGC/GBR song is not claimed, whatever its name says.
head -c 70000 /dev/zero | tr '\0' 'A' > notnez.sgc
out=$("$cli" render notnez.sgc --output fd_nez.wav --seconds 1 --rate "$rate" 2>/dev/null) || true
[ "$(field "$out" replayer)" != "bbmp_nez" ] || fail "bbmp_nez claimed a file that is not an SGC/GBR song"
rm -f notnez.sgc fd_nez.wav
echo nez_decode_ok
