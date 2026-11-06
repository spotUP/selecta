#!/bin/sh
# I710am: SCC-Musixx files (.sng, MSX Konami SCC) play through `selecta-cli render` as a RENDER-kind replayer (bbmp_smx module,
# GPL-3.0-or-later bbmp csrc/retro/smx_engine.cpp on emu2212 MIT). Three real songs by BDD. Modules are fetched by checksum, never committed
# (tests/m3/fixtures/bbmp_smx.SOURCE). Exit 77 = skipped (no network). Sentinel: the `replayer=bbmp_smx` line of render.
# Asserts per song: the plugin ran, the WAV is not silent (SMX_ENERGY_MIN raises the floor, for red proofs) and the rendered frames
# match the song length (SMX_MIN_SECONDS, when set, replaces the floor, for red proofs).
# Negative cases (.sng is shared with AdPlug, nuade and GoatTracker): a GoatTracker-style "GTS5" file, a zero file and a song cut off are not claimed.
# usage: smx_decode.sh <selecta-cli>
set -u
cli=$1
here=$(dirname "$0")
M="https://modland.com/pub/modules/SCC-Musixx/BDD%20%28Ben%20den%20Dulk%29"
field() { printf '%s\n' "$1" | tr ' ' '\n' | sed -n "s/^$2=//p" | head -1; }
fail() { echo "FAIL: $*"; exit 1; }
energy() { od -An -v -t d2 -j 44 "$1" | tr -s " " "\n" | awk '$1 != "" { s += $1 * $1 } END { printf "%d", s }'; }
rate=44100
fetch() { "$here/fetch_fixture.sh" "$1" "$2" "$3" || exit $?; }
check() { # <song path> <min seconds> <max seconds>
  rm -f fd_smx.wav
  out=$("$cli" render "$1" --output fd_smx.wav --seconds 600 --rate "$rate" 2>/dev/null) || fail "render failed: $1"
  [ "$(field "$out" replayer)" = "bbmp_smx" ] || fail "$1: replayer=$(field "$out" replayer), want bbmp_smx"
  got=$(field "$out" rendered_frames)
  min=${SMX_MIN_SECONDS:-$2}
  [ "$got" -ge $((rate * min)) ] || fail "$1: rendered_frames=$got, want at least $((rate * min))"
  [ "$got" -le $((rate * $3)) ] || fail "$1: rendered_frames=$got, far beyond the song length"
  e=$(energy fd_smx.wav)
  [ "$e" -gt "${SMX_ENERGY_MIN:-1000000}" ] || fail "$1: silent output (energy $e)"
  echo "$1 frames=$got energy=$e"
  rm -f fd_smx.wav
}
s1=$(fetch "$M/BDDSCC2%20%5B50Hz%5D.SNG" a45b1325de59c83e3b7d84ced7e28c404ded6c3a7396b91fef60ae6aab0fd693 bddscc2.sng) || exit $?
s2=$(fetch "$M/CLAURA%20%5B50Hz%5D.SNG" 52f6bc76ff07a6464b1dedd297378724328b536c045a1bec0a43a3baf7143530 claura.sng) || exit $?
s3=$(fetch "$M/BDDT3%20%5B50Hz%5D.SNG" 42906c51b092bd77b4cbd59830b299da99abc91689bd2496a19ce85dcb41fdc1 bddt3.sng) || exit $?
check "$s1" 90 93
check "$s2" 45 47
check "$s3" 60 63
# Another .sng format (GoatTracker "GTS5" header, 20000 bytes) is not claimed.
{ printf 'GTS5'; head -c 19996 /dev/zero | tr '\0' 'A'; } > goat.sng
out=$("$cli" render goat.sng --output fd_smx.wav --seconds 1 --rate "$rate" 2>/dev/null) || true
[ "$(field "$out" replayer)" != "bbmp_smx" ] || fail "bbmp_smx claimed a GoatTracker-style .sng file"
# A zero-filled .sng is not claimed (no song length).
head -c 20000 /dev/zero > zero.sng
out=$("$cli" render zero.sng --output fd_smx.wav --seconds 1 --rate "$rate" 2>/dev/null) || true
[ "$(field "$out" replayer)" != "bbmp_smx" ] || fail "bbmp_smx claimed a zero-filled .sng file"
# A song cut off inside its patterns is not played by bbmp_smx.
head -c 3000 "$s1" > cutoff.sng
out=$("$cli" render cutoff.sng --output fd_smx.wav --seconds 1 --rate "$rate" 2>/dev/null) || true
[ "$(field "$out" replayer)" != "bbmp_smx" ] || fail "bbmp_smx played a song cut off inside its tables"
rm -f goat.sng zero.sng cutoff.sng fd_smx.wav
echo smx_decode_ok
