#!/bin/sh
# I710e: Buzzic 1 (.buz, versions 1.0 and 1.1) and Buzzic 2 (.buz2) songs play through `selecta-cli render` as a RENDER-kind replayer
# (bbmp_buzzic module, GPL-3.0-or-later bbmp csrc/synth clean-room Buzzic engine). Four real songs fetched by checksum, never
# committed (tests/m3/fixtures/bbmp_buzzic.SOURCE). Exit 77 = skipped (no network). Sentinel: the `replayer=bbmp_buzzic` line of
# render. Asserts per song: the plugin ran, the WAV is not silent (BUZZIC_ENERGY_MIN raises the floor, for red proofs) and the rendered
# frames match the song length the engine measures (BUZZIC_MIN_SECONDS, when set, replaces the floor, for red proofs). Also: a file that
# is not a Buzzic song is not claimed.
# usage: buzzic_decode.sh <selecta-cli>
set -u
cli=$1
here=$(dirname "$0")
M=https://modland.com/pub/modules
field() { printf '%s\n' "$1" | tr ' ' '\n' | sed -n "s/^$2=//p" | head -1; }
fail() { echo "FAIL: $*"; exit 1; }
energy() { od -An -v -t d2 -j 44 "$1" | tr -s " " "\n" | awk '$1 != "" { s += $1 * $1 } END { printf "%d", s }'; }
rate=44100
fetch() { "$here/fetch_fixture.sh" "$1" "$2" "$3" || exit $?; }
check() { # <song path> <min seconds> <max seconds>
  rm -f fd_buzzic.wav
  out=$("$cli" render "$1" --output fd_buzzic.wav --seconds 900 --rate "$rate" 2>/dev/null) || fail "render failed: $1"
  [ "$(field "$out" replayer)" = "bbmp_buzzic" ] || fail "$1: replayer=$(field "$out" replayer), want bbmp_buzzic"
  got=$(field "$out" rendered_frames)
  min=${BUZZIC_MIN_SECONDS:-$2}
  [ "$got" -ge $((rate * min)) ] || fail "$1: rendered_frames=$got, want at least $((rate * min))"
  [ "$got" -le $((rate * $3)) ] || fail "$1: rendered_frames=$got, far beyond the song length"
  e=$(energy fd_buzzic.wav)
  [ "$e" -gt "${BUZZIC_ENERGY_MIN:-1000000}" ] || fail "$1: silent output (energy $e)"
  echo "$1 frames=$got energy=$e"
  rm -f fd_buzzic.wav
}
s1=$(fetch "$M/Buzzic/Stan/sample.buz" 03e40d5bda1bc8ab20d40f6cf15e1057460921359136307e2ab9bbfcfbc623ea sample.buz) || exit $?
s2=$(fetch "$M/Buzzic/Stan/sample1.buz" 5fa6e612d58bd8470a2e19cacf3f1a8dfcd882f5acd8073a976a337c5490516a sample1.buz) || exit $?
s3=$(fetch "$M/Buzzic%202/Stan/outdream.buz2" 1c94b1ae5e4ed4c7fafee3068072feb1582691c3247d3eb9937fe2b81f5f7f99 outdream.buz2) || exit $?
s4=$(fetch "$M/Buzzic%202/Chromag/attacktrack.buz2" 7cee38e32feb45a29520c94a13bc26abac3c0bceaee7fb2107bc348f41bfab27 attacktrack.buz2) || exit $?
check "$s1" 31 33
check "$s2" 60 62
check "$s3" 52 54
check "$s4" 190 192
# A file that is not a Buzzic song is not claimed, whatever its name says.
head -c 45224 /dev/zero | tr '\0' 'A' > notbuz.buz
head -c 70000 /dev/zero | tr '\0' 'A' > notbuz.buz2
for f in notbuz.buz notbuz.buz2; do
  out=$("$cli" render "$f" --output fd_buzzic.wav --seconds 1 --rate "$rate" 2>/dev/null) || true
  [ "$(field "$out" replayer)" != "bbmp_buzzic" ] || fail "bbmp_buzzic claimed $f, which is not a Buzzic song"
done
rm -f notbuz.buz notbuz.buz2 fd_buzzic.wav
echo buzzic_decode_ok
