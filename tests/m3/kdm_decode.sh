#!/bin/sh
# I710j: Ken's Digital Music songs (.kdm) play through `selecta-cli render` as a RENDER-kind replayer (bbmp_kdm module, GPL-3.0-or-later
# bbmp csrc/synth clean-room KDM engine). Two real songs by Ken Silverman ("kdmsong", "joysong") with his waves.kwv bank beside
# them. Fetched by checksum, never committed (tests/m3/fixtures/bbmp_kdm.SOURCE). Exit 77 = skipped (no network). Sentinel: the
# `replayer=bbmp_kdm` line of render. Asserts per song: the plugin ran, the WAV is not silent (KDM_ENERGY_MIN raises the floor, for
# red proofs) and the rendered frames match the song length the engine measures (KDM_MIN_SECONDS, when set, replaces the floor, for
# red proofs). Also: a file that is not a KDM song is not claimed, and a song without waves.kwv beside it is not claimed.
# usage: kdm_decode.sh <selecta-cli>
set -u
cli=$1
here=$(dirname "$0")
M=https://modland.com/pub/modules/Ken%27s%20Digital%20Music/Ken%20Silverman
field() { printf '%s\n' "$1" | tr ' ' '\n' | sed -n "s/^$2=//p" | head -1; }
fail() { echo "FAIL: $*"; exit 1; }
energy() { od -An -v -t d2 -j 44 "$1" | tr -s " " "\n" | awk '$1 != "" { s += $1 * $1 } END { printf "%d", s }'; }
rate=44100
fetch() { "$here/fetch_fixture.sh" "$1" "$2" "$3" || exit $?; }
check() { # <song path> <min seconds> <max seconds>
  rm -f fd_kdm.wav
  out=$("$cli" render "$1" --output fd_kdm.wav --seconds 900 --rate "$rate" 2>/dev/null) || fail "render failed: $1"
  [ "$(field "$out" replayer)" = "bbmp_kdm" ] || fail "$1: replayer=$(field "$out" replayer), want bbmp_kdm"
  got=$(field "$out" rendered_frames)
  min=${KDM_MIN_SECONDS:-$2}
  [ "$got" -ge $((rate * min)) ] || fail "$1: rendered_frames=$got, want at least $((rate * min))"
  [ "$got" -le $((rate * $3)) ] || fail "$1: rendered_frames=$got, far beyond the song length"
  e=$(energy fd_kdm.wav)
  [ "$e" -gt "${KDM_ENERGY_MIN:-1000000}" ] || fail "$1: silent output (energy $e)"
  echo "$1 frames=$got energy=$e"
  rm -f fd_kdm.wav
}
fetch "$M/waves.kwv" 222e50767edf2551761b87b13d1d56ea3a51984fd54c445cac62a6c01bd5708e waves.kwv >/dev/null || exit $?
s1=$(fetch "$M/kdmsong.kdm" e5d40426d3c48b54bab2ca31b07a8f39882a4ddbedcbbbc149316d73783662e0 kdmsong.kdm) || exit $?
s2=$(fetch "$M/joysong.kdm" 28cd6d53397eb1ebd73a414212ecfed87a92dffeedf48e5224aae63e457105c5 joysong.kdm) || exit $?
check "$s1" 31 33
check "$s2" 64 66
# A song without its waves.kwv beside it is not claimed.
mkdir -p nobank && cp "$s1" nobank/kdmsong.kdm
out=$("$cli" render nobank/kdmsong.kdm --output fd_kdm.wav --seconds 1 --rate "$rate" 2>/dev/null) || true
[ "$(field "$out" replayer)" != "bbmp_kdm" ] || fail "bbmp_kdm claimed a song with no waves.kwv"
rm -rf nobank
# A file that is not a KDM song is not claimed, whatever its name says.
head -c 70000 /dev/zero | tr '\0' 'A' > notkdm.kdm
out=$("$cli" render notkdm.kdm --output fd_kdm.wav --seconds 1 --rate "$rate" 2>/dev/null) || true
[ "$(field "$out" replayer)" != "bbmp_kdm" ] || fail "bbmp_kdm claimed a file that is not a KDM song"
rm -f notkdm.kdm fd_kdm.wav
echo kdm_decode_ok
