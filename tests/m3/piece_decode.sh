#!/bin/sh
# I710s: P/ECE music driver songs (.pmd, Aquaplus) play through `selecta-cli render` as a RENDER-kind replayer (bbmp_piece module,
# GPL-3.0-or-later bbmp csrc/piece/piece.cpp). Three real songs, fetched by checksum, never committed (tests/m3/fixtures/bbmp_piece.SOURCE).
# Exit 77 = skipped (no network). Sentinel: the `replayer=bbmp_piece` line of render.
# Asserts per song: the plugin ran, the WAV is not silent (PIECE_ENERGY_MIN raises the floor, for red proofs) and the rendered frames
# match the song length the engine measures (PIECE_MIN_SECONDS, when set, replaces the floor, for red proofs).
# Also: a .pmd file that is not a P/ECE song is not claimed, a cut-off song is not played, and a song without the .pmd extension
# is not claimed (the extension is part of the probe; P.M.D. of the PC-98 must keep its own files).
# usage: piece_decode.sh <selecta-cli>
set -u
cli=$1
here=$(dirname "$0")
M="https://modland.com/pub/modules/PiECE%20music%20driver"
field() { printf '%s\n' "$1" | tr ' ' '\n' | sed -n "s/^$2=//p" | head -1; }
fail() { echo "FAIL: $*"; exit 1; }
energy() { od -An -v -t d2 -j 44 "$1" | tr -s " " "\n" | awk '$1 != "" { s += $1 * $1 } END { printf "%d", s }'; }
rate=44100
fetch() { "$here/fetch_fixture.sh" "$1" "$2" "$3" || exit $?; }
check() { # <song path> <min seconds> <max seconds>
  rm -f fd_piece.wav
  out=$("$cli" render "$1" --output fd_piece.wav --seconds 600 --rate "$rate" 2>/dev/null) || fail "render failed: $1"
  [ "$(field "$out" replayer)" = "bbmp_piece" ] || fail "$1: replayer=$(field "$out" replayer), want bbmp_piece"
  got=$(field "$out" rendered_frames)
  min=${PIECE_MIN_SECONDS:-$2}
  [ "$got" -ge $((rate * min)) ] || fail "$1: rendered_frames=$got, want at least $((rate * min))"
  [ "$got" -le $((rate * $3)) ] || fail "$1: rendered_frames=$got, far beyond the song length"
  e=$(energy fd_piece.wav)
  [ "$e" -gt "${PIECE_ENERGY_MIN:-1000000}" ] || fail "$1: silent output (energy $e)"
  echo "$1 frames=$got energy=$e"
  rm -f fd_piece.wav
}
s1=$(fetch "$M/Autch/Final%20Fantasy%20-%20Prelude.pmd" c5e2dc0bf3799c2bf10c626a86132cfdfa60dfdfb63398dc66bce9c68aa77aa6 prelude.pmd) || exit $?
s2=$(fetch "$M/Yama/Sample%20BGM1.pmd" 5208a4e79031c40036f5760b9529c46012fdd9ccf5b56659d29749221b446ace sample_bgm1.pmd) || exit $?
s3=$(fetch "$M/Chemool/Calling.pmd" a9001e3e47f224bd017f211e15114053eb21d00ef1a4a007bb503da587ae2326 calling.pmd) || exit $?
check "$s1" 90 98
check "$s2" 32 37
check "$s3" 50 55
# A file with the extension that is not a P/ECE song is not claimed.
head -c 20000 /dev/zero | tr '\0' 'A' > notpc.pmd
out=$("$cli" render notpc.pmd --output fd_piece.wav --seconds 1 --rate "$rate" 2>/dev/null) || true
[ "$(field "$out" replayer)" != "bbmp_piece" ] || fail "bbmp_piece claimed a file that is not a P/ECE song"
# A song cut off inside its header is not played by bbmp_piece.
head -c 12 "$s1" > cutoff.pmd
out=$("$cli" render cutoff.pmd --output fd_piece.wav --seconds 1 --rate "$rate" 2>/dev/null) || true
[ "$(field "$out" replayer)" != "bbmp_piece" ] || fail "bbmp_piece played a cut-off song"
# The extension is part of the test (P.M.D. shares .pmd-like names): the same song without .pmd is not claimed.
cp "$s1" prelude.bin
out=$("$cli" render prelude.bin --output fd_piece.wav --seconds 1 --rate "$rate" 2>/dev/null) || true
[ "$(field "$out" replayer)" != "bbmp_piece" ] || fail "bbmp_piece claimed a song without the .pmd extension"
rm -rf notpc.pmd cutoff.pmd prelude.bin fd_piece.wav
echo piece_decode_ok
