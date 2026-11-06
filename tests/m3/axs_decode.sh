#!/bin/sh
# I710a: AXS songs (.axs) play through `selecta-cli render` as a RENDER-kind replayer (bbmp_axs module, GPL-3.0-or-later bbmp
# csrc/axs clean-room AXS engine). Two real songs: "emile50" by Emile and "spectral" by Rez. Fetched by checksum, never
# committed (tests/m3/fixtures/bbmp_axs.SOURCE). Exit 77 = skipped (no network). Sentinel: the `replayer=bbmp_axs` line of
# render. Asserts per song: the plugin ran, the WAV is not silent (AXS_ENERGY_MIN raises the floor, for red proofs) and the
# rendered frames match the song length the engine measures (AXS_MIN_SECONDS, when set, replaces the floor, for red proofs).
# Also: a file that is not an AXS song is not claimed.
# usage: axs_decode.sh <selecta-cli>
set -u
cli=$1
here=$(dirname "$0")
M=https://modland.com/pub/modules/AXS
field() { printf '%s\n' "$1" | tr ' ' '\n' | sed -n "s/^$2=//p" | head -1; }
fail() { echo "FAIL: $*"; exit 1; }
energy() { od -An -v -t d2 -j 44 "$1" | tr -s " " "\n" | awk '$1 != "" { s += $1 * $1 } END { printf "%d", s }'; }
rate=44100
fetch() { "$here/fetch_fixture.sh" "$1" "$2" "$3" || exit $?; }
check() { # <song path> <min seconds> <max seconds>
  rm -f fd_axs.wav
  out=$("$cli" render "$1" --output fd_axs.wav --seconds 900 --rate "$rate" 2>/dev/null) || fail "render failed: $1"
  [ "$(field "$out" replayer)" = "bbmp_axs" ] || fail "$1: replayer=$(field "$out" replayer), want bbmp_axs"
  got=$(field "$out" rendered_frames)
  min=${AXS_MIN_SECONDS:-$2}
  [ "$got" -ge $((rate * min)) ] || fail "$1: rendered_frames=$got, want at least $((rate * min))"
  [ "$got" -le $((rate * $3)) ] || fail "$1: rendered_frames=$got, far beyond the song length"
  e=$(energy fd_axs.wav)
  [ "$e" -gt "${AXS_ENERGY_MIN:-1000000}" ] || fail "$1: silent output (energy $e)"
  echo "$1 frames=$got energy=$e"
  rm -f fd_axs.wav
}
s1=$(fetch "$M/Emile/emile50.axs" 389bcd386032c0d0e473d0ee83e48355d1199fe8d4516a56587bfa10dfe682b7 emile50.axs) || exit $?
s2=$(fetch "$M/Rez/spectral.axs" 470e1b16abcd5a974eb14c9f6abd4af9ca1e3b7ea24603914145f6130618a61c spectral.axs) || exit $?
check "$s1" 221 222
check "$s2" 272 273
# A file that is not an AXS song is not claimed, whatever its name says.
head -c 70000 /dev/zero | tr '\0' 'A' > notaxs.axs
out=$("$cli" render notaxs.axs --output fd_axs.wav --seconds 1 --rate "$rate" 2>/dev/null) || true
[ "$(field "$out" replayer)" != "bbmp_axs" ] || fail "bbmp_axs claimed a file that is not an AXS song"
# The same with an AXS header and nothing behind it.
{ printf 'FORM\0\0\0\0AXSF'; head -c 2000 /dev/zero; } > hollow.axs
out=$("$cli" render hollow.axs --output fd_axs.wav --seconds 1 --rate "$rate" 2>/dev/null) || true
[ "$(field "$out" replayer)" != "bbmp_axs" ] || fail "bbmp_axs claimed an AXS header with no song behind it"
rm -f notaxs.axs hollow.axs fd_axs.wav
echo axs_decode_ok
