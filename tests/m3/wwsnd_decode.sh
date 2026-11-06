#!/bin/sh
# I710aj: Westwood SND files (.snd, PC speaker) play through `selecta-cli render` as a RENDER-kind replayer (bbmp_wwsnd module,
# GPL-3.0-or-later bbmp csrc/retro/wwsnd_engine.cpp). Three real songs (Kyrandia, Eye of the Beholder 1 and 2; no companion files). Modules are fetched by checksum, never committed
# (tests/m3/fixtures/bbmp_wwsnd.SOURCE). Exit 77 = skipped (no network). Sentinel: the `replayer=bbmp_wwsnd` line of render.
# Asserts per song: the plugin ran, the WAV is not silent (WWSND_ENERGY_MIN raises the floor, for red proofs) and the
# rendered frames match the song length the engine measures (WWSND_MIN_SECONDS, when set, replaces the floor, for red proofs).
# Also: a file that is not a Westwood SND song is not claimed, and a song cut off inside its tables is not played.
# usage: wwsnd_decode.sh <selecta-cli>
set -u
cli=$1
here=$(dirname "$0")
M="https://modland.com/pub/modules/Westwood%20SND"
field() { printf '%s\n' "$1" | tr ' ' '\n' | sed -n "s/^$2=//p" | head -1; }
fail() { echo "FAIL: $*"; exit 1; }
energy() { od -An -v -t d2 -j 44 "$1" | tr -s " " "\n" | awk '$1 != "" { s += $1 * $1 } END { printf "%d", s }'; }
rate=44100
fetch() { "$here/fetch_fixture.sh" "$1" "$2" "$3" || exit $?; }
check() { # <song path> <min seconds> <max seconds>
  rm -f fd_wwsnd.wav
  out=$("$cli" render "$1" --output fd_wwsnd.wav --seconds 600 --rate "$rate" 2>/dev/null) || fail "render failed: $1"
  [ "$(field "$out" replayer)" = "bbmp_wwsnd" ] || fail "$1: replayer=$(field "$out" replayer), want bbmp_wwsnd"
  got=$(field "$out" rendered_frames)
  min=${WWSND_MIN_SECONDS:-$2}
  [ "$got" -ge $((rate * min)) ] || fail "$1: rendered_frames=$got, want at least $((rate * min))"
  [ "$got" -le $((rate * $3)) ] || fail "$1: rendered_frames=$got, far beyond the song length"
  e=$(energy fd_wwsnd.wav)
  [ "$e" -gt "${WWSND_ENERGY_MIN:-1000000}" ] || fail "$1: silent output (energy $e)"
  echo "$1 frames=$got energy=$e"
  rm -f fd_wwsnd.wav
}
s1=$(fetch "$M/Frank%20Klepacki/legend%20of%20kyrandia%20-%201a.snd" ea462b59eb6aaf7715d866957c3ae41f8576d7ad260e7ef3d28d7edb9f174555 kyrandia_1a.snd) || exit $?
s2=$(fetch "$M/Paul%20Mudra/eob%20-%20intro.snd" ddd4af77101a409fe7b570dbae762a64ab53872a4e0c988041303694d6526117 eob_intro.snd) || exit $?
s3=$(fetch "$M/Paul%20Mudra/eob2%20-%20azure.snd" 60b4319349b2d607502fecb1809b22d0fc7205495c23683c70fecb968e93ba72 eob2_azure.snd) || exit $?
check "$s1" 8 9
check "$s2" 1 2
check "$s3" 1 2
# A file that is not a Westwood SND song is not claimed.
head -c 20000 /dev/zero | tr '\0' 'A' > notww
out=$("$cli" render notww --output fd_wwsnd.wav --seconds 1 --rate "$rate" 2>/dev/null) || true
[ "$(field "$out" replayer)" != "bbmp_wwsnd" ] || fail "bbmp_wwsnd claimed a file that is not a Westwood SND song"
# A file cut off inside its tables is not played by bbmp_wwsnd.
head -c 200 "$s1" > cutoff
out=$("$cli" render cutoff --output fd_wwsnd.wav --seconds 1 --rate "$rate" 2>/dev/null) || true
[ "$(field "$out" replayer)" != "bbmp_wwsnd" ] || fail "bbmp_wwsnd played a song cut off inside its tables"
rm -rf notww cutoff fd_wwsnd.wav
echo wwsnd_decode_ok
