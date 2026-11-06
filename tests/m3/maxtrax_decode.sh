#!/bin/sh
# I710k: MaxTrax (.mxtx) songs play through `selecta-cli render` as a RENDER-kind replayer (bbmp_maxtrax module, bbmp
# csrc/amiga/maxtrax_engine over ScummVM's MaxTrax player). Two real songs fetched by checksum, never committed
# (tests/m3/fixtures/bbmp_maxtrax.SOURCE). Exit 77 = skipped (no network). Sentinel: the `replayer=bbmp_maxtrax` line of render.
# Asserts per song: the plugin ran, the WAV is not silent (MX_ENERGY_MIN raises the floor, for red proofs) and the rendered
# frames match the song length the engine measured (the first song's known length +- MX_TOL_MS; MX_MIN_SECONDS raises the floor,
# for red proofs). A file with the extension and no MXTX header must not be claimed.
# usage: maxtrax_decode.sh <selecta-cli>
set -u
cli=$1
here=$(dirname "$0")
M=https://modland.com/pub/modules/MaxTrax
field() { printf '%s\n' "$1" | tr ' ' '\n' | sed -n "s/^$2=//p" | head -1; }
fail() { echo "FAIL: $*"; exit 1; }
energy() { od -An -v -t d2 -j 44 "$1" | tr -s " " "\n" | awk '$1 != "" { s += $1 * $1 } END { printf "%d", s }'; }
rate=44100
fetch() { "$here/fetch_fixture.sh" "$1" "$2" "$3" || exit $?; }
check() { # <song path> <expected ms> <tolerance ms>
  rm -f fd_mx.wav
  out=$("$cli" render "$1" --output fd_mx.wav --seconds 1000 --rate "$rate" 2>/dev/null) || fail "render failed: $1"
  [ "$(field "$out" replayer)" = "bbmp_maxtrax" ] || fail "$1: replayer=$(field "$out" replayer), want bbmp_maxtrax"
  got=$(field "$out" rendered_frames)
  want=$(($2 * rate / 1000)); tol=$(($3 * rate / 1000))
  [ "$got" -ge $((want - tol)) ] && [ "$got" -le $((want + tol)) ] || fail "$1: rendered_frames=$got, want $want +- $tol"
  [ "$got" -ge $((rate * ${MX_MIN_SECONDS:-1})) ] || fail "$1: rendered_frames=$got, below the floor"
  e=$(energy fd_mx.wav)
  [ "$e" -gt "${MX_ENERGY_MIN:-1000000}" ] || fail "$1: silent output (energy $e)"
  echo "$1 frames=$got energy=$e"
  rm -f fd_mx.wav
}
s1=$(fetch "$M/Steve%20Hales/antmusic.mxtx" e4a2620e61e1091ee714d30c87c0c36a8e17f852aa7f572a463f867128f6c98d antmusic.mxtx) || exit $?
s2=$(fetch "$M/Frank%20Klepacki/dune%202%20endgame.mxtx" 1842fafeda339ae182e25603a5691abf11737eec5551ed084a9163ca5b834e79 dune2endgame.mxtx) || exit $?
check "$s1" 102442 500
check "$s2" 29000 500
head -c 4096 /dev/zero | tr '\0' 'A' > notmx.mxtx
out=$("$cli" render notmx.mxtx --output fd_mx.wav --seconds 1 --rate "$rate" 2>/dev/null) || true
[ "$(field "$out" replayer)" != "bbmp_maxtrax" ] || fail "bbmp_maxtrax claimed a .mxtx without the MXTX header"
rm -f notmx.mxtx fd_mx.wav
echo maxtrax_decode_ok
