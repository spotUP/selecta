#!/bin/sh
# I710v: SBStudio songs (.pac) play through `selecta-cli render` as a RENDER-kind replayer (bbmp_sbstudio module, libpac 0.9.0, ISC,
# vendored in third_party/libpac). Four real songs fetched by checksum, never committed (tests/m3/fixtures/bbmp_sbstudio.SOURCE). Exit 77 =
# skipped (no network). Sentinel: the `replayer=bbmp_sbstudio` line of render. Asserts per song: the plugin ran, the WAV is not silent
# (SB_ENERGY_MIN raises the floor, for red proofs) and the rendered frames reach the song length (SB_MIN_SECONDS, when set, replaces the
# floor, for red proofs). Also: a file that is not an SBStudio song is not claimed.
# usage: sbstudio_decode.sh <selecta-cli>
set -u
cli=$1
here=$(dirname "$0")
M=https://modland.com/pub/modules/SBStudio/Leif-Ronny%20Isaksen
field() { printf '%s\n' "$1" | tr ' ' '\n' | sed -n "s/^$2=//p" | head -1; }
fail() { echo "FAIL: $*"; exit 1; }
energy() { od -An -v -t d2 -j 44 "$1" | tr -s " " "\n" | awk '$1 != "" { s += $1 * $1 } END { printf "%d", s }'; }
rate=44100
cap=${SB_CAP:-600}
fetch() { "$here/fetch_fixture.sh" "$1" "$2" "$3" || exit $?; }
check() { # <song path> <min seconds>
  rm -f fd_sbs.wav
  out=$("$cli" render "$1" --output fd_sbs.wav --seconds "$cap" --rate "$rate" 2>/dev/null) || fail "render failed: $1"
  [ "$(field "$out" replayer)" = "bbmp_sbstudio" ] || fail "$1: replayer=$(field "$out" replayer), want bbmp_sbstudio"
  got=$(field "$out" rendered_frames)
  min=${SB_MIN_SECONDS:-$2}
  [ "$got" -ge $((rate * min)) ] || fail "$1: rendered_frames=$got, want at least $((rate * min))"
  [ "$got" -le $((rate * cap)) ] || fail "$1: rendered_frames=$got, beyond the cap"
  e=$(energy fd_sbs.wav)
  [ "$e" -gt "${SB_ENERGY_MIN:-1000000}" ] || fail "$1: silent output (energy $e)"
  echo "$1 frames=$got energy=$e"
  rm -f fd_sbs.wav
}
s1=$(fetch "$M/darkness.pac" 58558bef69f29ec787fe9238f584f3fdffdb33c1e1d2b2d38eb0d5c2d087b665 darkness.pac) || exit $?
s2=$(fetch "$M/jungle%20fever.pac" fecb5377f2b08776d58f86e5ed8179ef7998f6558b624cd358ae991f34e22e29 jungle_fever.pac) || exit $?
s3=$(fetch "$M/land%20of%20epsilon.pac" 38fd9953097c54d24cfb29d198de92ce1055a560199d019806f2601485608161 land_of_epsilon.pac) || exit $?
s4=$(fetch "$M/silvermoon.pac" c8a38e85f6fbad8a1ecea1d2072d01eba88b8e3d683837c585b95e4455f75f92 silvermoon.pac) || exit $?
check "$s1" 318
check "$s2" 210
check "$s3" 197
check "$s4" 143
# A file that is not an SBStudio song is not claimed, whatever its name says.
head -c 70000 /dev/zero | tr '\0' 'A' > notpac.pac
out=$("$cli" render notpac.pac --output fd_sbs.wav --seconds 1 --rate "$rate" 2>/dev/null) || true
[ "$(field "$out" replayer)" != "bbmp_sbstudio" ] || fail "bbmp_sbstudio claimed a file that is not an SBStudio song"
rm -f notpac.pac fd_sbs.wav
echo sbstudio_decode_ok
