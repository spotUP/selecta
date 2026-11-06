#!/bin/sh
# I710ab: Skale Tracker songs (.skm) play through `selecta-cli render` as a RENDER-kind replayer (bbmp_skale module: bbmp csrc/synth
# skale_engine, GPL-3.0-or-later, over the vendored libopenmpt with Luke Preiner's SKM loader patched in). Four real Modland songs; modules are
# fetched by checksum, never committed (tests/m3/fixtures/bbmp_skale.SOURCE). Exit 77 = skipped (no network). Sentinel: the `replayer=bbmp_skale`
# line of render. Asserts per song: the plugin ran, the WAV is not silent (SKALE_ENERGY_MIN raises the floor, for red proofs) and the
# rendered frames reach the song length (SKALE_MIN_SECONDS, when set, replaces the floor, for red proofs).
# Also: a file that is not an .skm is not claimed, and a cut-off song is not played by bbmp_skale.
# usage: skale_decode.sh <selecta-cli>
set -u
cli=$1
here=$(dirname "$0")
M="https://modland.com/pub/modules/Skale%20Tracker"
field() { printf '%s\n' "$1" | tr ' ' '\n' | sed -n "s/^$2=//p" | head -1; }
fail() { echo "FAIL: $*"; exit 1; }
energy() { od -An -v -t d2 -j 44 "$1" | tr -s " " "\n" | awk '$1 != "" { s += $1 * $1 } END { printf "%d", s }'; }
rate=44100
fetch() { "$here/fetch_fixture.sh" "$1" "$2" "$3" || exit $?; }
check() { # <song path> <min seconds> <max seconds>
  rm -f fd_skale.wav
  out=$("$cli" render "$1" --output fd_skale.wav --seconds 600 --rate "$rate" 2>/dev/null) || fail "render failed: $1"
  [ "$(field "$out" replayer)" = "bbmp_skale" ] || fail "$1: replayer=$(field "$out" replayer), want bbmp_skale"
  got=$(field "$out" rendered_frames)
  min=${SKALE_MIN_SECONDS:-$2}
  [ "$got" -ge $((rate * min)) ] || fail "$1: rendered_frames=$got, want at least $((rate * min))"
  [ "$got" -le $((rate * $3)) ] || fail "$1: rendered_frames=$got, far beyond the song length"
  e=$(energy fd_skale.wav)
  [ "$e" -gt "${SKALE_ENERGY_MIN:-1000000}" ] || fail "$1: silent output (energy $e)"
  echo "$1 frames=$got energy=$e"
  rm -f fd_skale.wav
}
src="$here/fixtures/bbmp_skale.SOURCE"
song() { # <line number among the url lines> -> cached path
  set -- $(grep '^https' "$src" | sed -n "${1}p")
  fetch "$1" "$2" "skale_$(printf '%s' "$1" | sed 's/.*\///; s/%20/_/g')"
}
a=$(song 1); b=$(song 2); c=$(song 3); d=$(song 4)
check "$a" 20 24
check "$b" 47 52
check "$c" 81 86
check "$d" 102 108

# A file that is not an .skm, and a cut-off song, must not be played by bbmp_skale (a renderer may still refuse it or another module may take it).
not_claimed() { # <path>
  out=$("$cli" render "$1" --output fd_skale.wav --seconds 5 --rate "$rate" 2>/dev/null) || return 0
  [ "$(field "$out" replayer)" != "bbmp_skale" ] || fail "$1: claimed by bbmp_skale"
}
cp "$a" fd_notskale.mod; not_claimed fd_notskale.mod
head -c 600 "$a" > fd_cut.skm; not_claimed fd_cut.skm
rm -f fd_notskale.mod fd_cut.skm fd_skale.wav
echo "skale_decode_ok"
