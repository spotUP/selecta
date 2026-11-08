#!/bin/sh
# I710aq: PlayerPRO MAD songs (.mad) play through `selecta-cli render` as a RENDER-kind replayer (bbmp_playerpro module, GPL-3.0-or-later bbmp
# csrc/pc/playerpro_engine.c on PlayerPROCore, public domain). Two real songs by Benjamin Birney. Songs are fetched by checksum, never committed
# (tests/m3/fixtures/bbmp_playerpro.SOURCE). Exit 77 = skipped (no network). Sentinel: the `replayer=bbmp_playerpro` line of render.
# Asserts per song: the plugin ran, the WAV is not silent (PP_ENERGY_MIN raises the floor, for red proofs) and rendered frames are at least the
# floor (PP_MIN_SECONDS, when set, replaces it, for red proofs) and not far beyond the song length.
# Negative cases (.mad is shared with adplug's Mlat Adlib Tracker, magic "MAD+"): such a file, a zero file and a cut-off song are not claimed.
# usage: playerpro_decode.sh <selecta-cli>
set -u
cli=$1
here=$(dirname "$0")
M="https://modland.com/pub/modules/PlayerPro/Benjamin%20Birney"
field() { printf '%s\n' "$1" | tr ' ' '\n' | sed -n "s/^$2=//p" | head -1; }
fail() { echo "FAIL: $*"; exit 1; }
energy() { od -An -v -t d2 -j 44 "$1" | tr -s " " "\n" | awk '$1 != "" { s += $1 * $1 } END { printf "%d", s }'; }
rate=44100
fetch() { "$here/fetch_fixture.sh" "$1" "$2" "$3" || exit $?; }
check() { # <song path> <min seconds> <max seconds>
  rm -f fd_pp.wav
  out=$("$cli" render "$1" --output fd_pp.wav --seconds 600 --rate "$rate" 2>/dev/null) || fail "render failed: $1"
  [ "$(field "$out" replayer)" = "bbmp_playerpro" ] || fail "$1: replayer=$(field "$out" replayer), want bbmp_playerpro"
  got=$(field "$out" rendered_frames)
  min=${PP_MIN_SECONDS:-$2}
  [ "$got" -ge $((rate * min)) ] || fail "$1: rendered_frames=$got, want at least $((rate * min))"
  [ "$got" -le $((rate * $3)) ] || fail "$1: rendered_frames=$got, far beyond the song length"
  e=$(energy fd_pp.wav)
  [ "$e" -gt "${PP_ENERGY_MIN:-1000000}" ] || fail "$1: silent output (energy $e)"
  echo "$1 frames=$got energy=$e"
  rm -f fd_pp.wav
}
s1=$(fetch "$M/mantra%2001%20opening.mad" 6b3778032b5c056c4a950dce4d2a5b851287be3002dcaea9bd3445392567b6ee mantra01.mad) || exit $?
s2=$(fetch "$M/mantra%2004%20town.mad" 4bdd147c74ceb67d329015d3bde334e563e220f564e2206704cb3cfbda6ba2b1 mantra04.mad) || exit $?
check "$s1" 75 78
check "$s2" 90 94
# An Mlat Adlib Tracker shaped .mad ("MAD+") is not claimed.
{ printf 'MAD+'; head -c 4096 /dev/zero; } > mlat.mad
out=$("$cli" render mlat.mad --output fd_pp.wav --seconds 1 --rate "$rate" 2>/dev/null) || true
[ "$(field "$out" replayer)" != "bbmp_playerpro" ] || fail "bbmp_playerpro claimed an Mlat Adlib Tracker .mad file"
head -c 4096 /dev/zero > zero.mad
out=$("$cli" render zero.mad --output fd_pp.wav --seconds 1 --rate "$rate" 2>/dev/null) || true
[ "$(field "$out" replayer)" != "bbmp_playerpro" ] || fail "bbmp_playerpro claimed a zero-filled .mad file"
# A MADG song cut off inside its header is not played.
head -c 500 "$s1" > cutoff.mad
out=$("$cli" render cutoff.mad --output fd_pp.wav --seconds 1 --rate "$rate" 2>/dev/null) || true
[ "$(field "$out" replayer)" != "bbmp_playerpro" ] || fail "bbmp_playerpro played a song cut off inside its header"
rm -f mlat.mad zero.mad cutoff.mad fd_pp.wav
echo playerpro_decode_ok
