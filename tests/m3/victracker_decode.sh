#!/bin/sh
# I710ai: VIC-TRACKER songs (.vt) play through `selecta-cli render` as a RENDER-kind replayer (bbmp_victracker module,
# GPL-3.0-or-later bbmp csrc/retro/victracker_engine.cpp running the tracker 6502 player). Three real songs (Daniel Kahlin x2, Warlord; no companion files). Modules are fetched by checksum, never committed
# (tests/m3/fixtures/bbmp_victracker.SOURCE). Exit 77 = skipped (no network). Sentinel: the `replayer=bbmp_victracker` line of render.
# Asserts per song: the plugin ran, the WAV is not silent (VICTRACKER_ENERGY_MIN raises the floor, for red proofs) and the
# rendered frames match the song length the engine measures (VICTRACKER_MIN_SECONDS, when set, replaces the floor, for red proofs).
# Also: a file that is not a VIC-TRACKER song is not claimed, and a song cut off inside its header is not played.
# usage: victracker_decode.sh <selecta-cli>
set -u
cli=$1
here=$(dirname "$0")
M="https://modland.com/pub/modules/Vic-Tracker"
field() { printf '%s\n' "$1" | tr ' ' '\n' | sed -n "s/^$2=//p" | head -1; }
fail() { echo "FAIL: $*"; exit 1; }
energy() { od -An -v -t d2 -j 44 "$1" | tr -s " " "\n" | awk '$1 != "" { s += $1 * $1 } END { printf "%d", s }'; }
rate=44100
fetch() { "$here/fetch_fixture.sh" "$1" "$2" "$3" || exit $?; }
check() { # <song path> <min seconds> <max seconds>
  rm -f fd_victracker.wav
  out=$("$cli" render "$1" --output fd_victracker.wav --seconds 600 --rate "$rate" 2>/dev/null) || fail "render failed: $1"
  [ "$(field "$out" replayer)" = "bbmp_victracker" ] || fail "$1: replayer=$(field "$out" replayer), want bbmp_victracker"
  got=$(field "$out" rendered_frames)
  min=${VICTRACKER_MIN_SECONDS:-$2}
  [ "$got" -ge $((rate * min)) ] || fail "$1: rendered_frames=$got, want at least $((rate * min))"
  [ "$got" -le $((rate * $3)) ] || fail "$1: rendered_frames=$got, far beyond the song length"
  e=$(energy fd_victracker.wav)
  [ "$e" -gt "${VICTRACKER_ENERGY_MIN:-1000000}" ] || fail "$1: silent output (energy $e)"
  echo "$1 frames=$got energy=$e"
  rm -f fd_victracker.wav
}
s1=$(fetch "$M/Daniel%20Kahlin/mystic.vt" 1c11bf3083e90ff29bff1dea503b2d85de0be1bc4dcd8680b860cc8755779cf3 mystic.vt) || exit $?
s2=$(fetch "$M/Daniel%20Kahlin/vt-theme.vt" a0a84bf2c8a1ff6c12c4905f70e84d62ff6fb10a1727308b9af25729669197fa vt-theme.vt) || exit $?
s3=$(fetch "$M/Warlord/toxic%20cave.vt" ac03a0b11f0a120d81326bb37d07b86540f54b1ded8f31369e85546b5919d690 toxic_cave.vt) || exit $?
check "$s1" 17 19
check "$s2" 23 24
check "$s3" 92 93
# A file that is not a VIC-TRACKER song is not claimed.
head -c 20000 /dev/zero | tr '\0' 'A' > notvt
out=$("$cli" render notvt --output fd_victracker.wav --seconds 1 --rate "$rate" 2>/dev/null) || true
[ "$(field "$out" replayer)" != "bbmp_victracker" ] || fail "bbmp_victracker claimed a file that is not a VIC-TRACKER song"
# A song cut off inside its header is not played by bbmp_victracker.
head -c 100 "$s1" > cutoff
out=$("$cli" render cutoff --output fd_victracker.wav --seconds 1 --rate "$rate" 2>/dev/null) || true
[ "$(field "$out" replayer)" != "bbmp_victracker" ] || fail "bbmp_victracker played a song cut off inside its header"
rm -rf notvt cutoff fd_victracker.wav
echo victracker_decode_ok
