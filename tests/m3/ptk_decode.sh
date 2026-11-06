#!/bin/sh
# I710q: NoiseTrekker / ProTrekkr songs (.ntk, .ptk) play through `selecta-cli render` as a RENDER-kind replayer (bbmp_ptk module,
# ProTrekkr replay routine, BSD 2-clause, vendored in third_party/bbmp_ptk). Four real songs, one per format family (ProTrekkr 2.0,
# ProTrekkr 1, NoiseTrekker 2, NoiseTrekker 1). Fetched by checksum, never committed (tests/m3/fixtures/bbmp_ptk.SOURCE). Exit 77 =
# skipped (no network). Sentinel: the `replayer=bbmp_ptk` line of render. Asserts per song: the plugin ran, the WAV is not silent
# (PK_ENERGY_MIN raises the floor, for red proofs) and the rendered frames reach the song length (PK_MIN_SECONDS, when set, replaces
# the floor, for red proofs). The render is capped at PK_CAP seconds. Also: a file that is not a Trekker song is not claimed.
# usage: ptk_decode.sh <selecta-cli>
set -u
cli=$1
here=$(dirname "$0")
M=https://modland.com/pub/modules
field() { printf '%s\n' "$1" | tr ' ' '\n' | sed -n "s/^$2=//p" | head -1; }
fail() { echo "FAIL: $*"; exit 1; }
energy() { od -An -v -t d2 -j 44 "$1" | tr -s " " "\n" | awk '$1 != "" { s += $1 * $1 } END { printf "%d", s }'; }
rate=44100
cap=${PK_CAP:-600}
fetch() { "$here/fetch_fixture.sh" "$1" "$2" "$3" || exit $?; }
check() { # <song path> <min seconds>
  rm -f fd_ptk.wav
  out=$("$cli" render "$1" --output fd_ptk.wav --seconds "$cap" --rate "$rate" 2>/dev/null) || fail "render failed: $1"
  [ "$(field "$out" replayer)" = "bbmp_ptk" ] || fail "$1: replayer=$(field "$out" replayer), want bbmp_ptk"
  got=$(field "$out" rendered_frames)
  min=${PK_MIN_SECONDS:-$2}
  [ "$got" -ge $((rate * min)) ] || fail "$1: rendered_frames=$got, want at least $((rate * min))"
  [ "$got" -le $((rate * cap)) ] || fail "$1: rendered_frames=$got, beyond the cap"
  e=$(energy fd_ptk.wav)
  [ "$e" -gt "${PK_ENERGY_MIN:-1000000}" ] || fail "$1: silent output (energy $e)"
  echo "$1 frames=$got energy=$e"
  rm -f fd_ptk.wav
}
s1=$(fetch "$M/ProTrekkr%202.0/Archangel/wonderland.ptk" 02f9877b537eca02ed57c702e5b6e17507857751fb9f72a263639907b0c9ca48 wonderland.ptk) || exit $?
s2=$(fetch "$M/ProTrekkr/Hitchhikr/303%20test.ptk" c4a3279acf096fed40e3327ffedb55ed6936fcb70ff901318cc9445413c44f85 303_test.ptk) || exit $?
s3=$(fetch "$M/NoiseTrekker%202/ArGuru/unionjack.ntk" 808870dab80faf46c13cb51729cb3922c760e14e916a3af5c21cac633eb8f42f unionjack.ntk) || exit $?
s4=$(fetch "$M/NoiseTrekker/_unknown/robot-ballet.ntk" 7c3487734c3b24f166f2724fdc7ab7d744ba2aa189adc7c942854251079a921d robot_ballet.ntk) || exit $?
check "$s1" 157
check "$s2" 71
check "$s3" 7
check "$s4" 238
# A file that is not a Trekker song is not claimed, whatever its name says.
head -c 70000 /dev/zero | tr '\0' 'A' > notptk.ptk
out=$("$cli" render notptk.ptk --output fd_ptk.wav --seconds 1 --rate "$rate" 2>/dev/null) || true
[ "$(field "$out" replayer)" != "bbmp_ptk" ] || fail "bbmp_ptk claimed a file that is not a Trekker song"
rm -f notptk.ptk fd_ptk.wav
echo ptk_decode_ok
