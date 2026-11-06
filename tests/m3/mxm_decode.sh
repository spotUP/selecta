#!/bin/sh
# I710f: Cubic Tiny XM (.mxm) songs play through `selecta-cli render` as a RENDER-kind replayer (bbmp_mxm module, GPL-3.0-or-later bbmp
# csrc/pc clean-room MXM-to-XM converter, played by libopenmpt). Two real songs fetched by checksum, never committed
# (tests/m3/fixtures/bbmp_mxm.SOURCE). Exit 77 = skipped (no network). Sentinel: the `replayer=bbmp_mxm` line of render. Asserts per
# song: the plugin ran, the WAV is not silent (MXM_ENERGY_MIN raises the floor, for red proofs) and the rendered frames match the song
# length (MXM_MIN_SECONDS, when set, replaces the floor, for red proofs). Also: a file that is not an MXM song is not claimed.
# usage: mxm_decode.sh <selecta-cli>
set -u
cli=$1
here=$(dirname "$0")
M=https://modland.com/pub/modules/Cubic%20Tiny%20XM
field() { printf '%s\n' "$1" | tr ' ' '\n' | sed -n "s/^$2=//p" | head -1; }
fail() { echo "FAIL: $*"; exit 1; }
energy() { od -An -v -t d2 -j 44 "$1" | tr -s " " "\n" | awk '$1 != "" { s += $1 * $1 } END { printf "%d", s }'; }
rate=44100
fetch() { "$here/fetch_fixture.sh" "$1" "$2" "$3" || exit $?; }
check() { # <song path> <min seconds> <max seconds>
  rm -f fd_mxm.wav
  out=$("$cli" render "$1" --output fd_mxm.wav --seconds 900 --rate "$rate" 2>/dev/null) || fail "render failed: $1"
  [ "$(field "$out" replayer)" = "bbmp_mxm" ] || fail "$1: replayer=$(field "$out" replayer), want bbmp_mxm"
  got=$(field "$out" rendered_frames)
  min=${MXM_MIN_SECONDS:-$2}
  [ "$got" -ge $((rate * min)) ] || fail "$1: rendered_frames=$got, want at least $((rate * min))"
  [ "$got" -le $((rate * $3)) ] || fail "$1: rendered_frames=$got, far beyond the song length"
  e=$(energy fd_mxm.wav)
  [ "$e" -gt "${MXM_ENERGY_MIN:-1000000}" ] || fail "$1: silent output (energy $e)"
  echo "$1 frames=$got energy=$e"
  rm -f fd_mxm.wav
}
s1=$(fetch "$M/Falcon%20%28PL%29/pulse-sink.mxm" 181523e1f16b49a77b0ea631e0075208010561d500e879b71546f597e27f28f5 pulse-sink.mxm) || exit $?
s2=$(fetch "$M/Oxygener/burra.mxm" f34277f68c5b3a5a19392ce4fab799eb930d253f21cdf53c5371995bb9bebbad burra.mxm) || exit $?
check "$s1" 239 241
check "$s2" 222 224
# A file that is not an MXM song is not claimed, whatever its name says.
head -c 5000 /dev/zero | tr '\0' 'A' > notmxm.mxm
out=$("$cli" render notmxm.mxm --output fd_mxm.wav --seconds 1 --rate "$rate" 2>/dev/null) || true
[ "$(field "$out" replayer)" != "bbmp_mxm" ] || fail "bbmp_mxm claimed notmxm.mxm, which is not an MXM song"
rm -f notmxm.mxm fd_mxm.wav
echo mxm_decode_ok
