#!/bin/sh
# I1242e: Sawteeth songs (.st, SWTD binary and SWTT text) play through `selecta-cli render` as a RENDER-kind replayer (sawteeth module, Jonas Norberg's
# player library, MIT). Three real upstream songs, pinned to one commit, fetched by checksum, never committed (tests/m3/fixtures/sawteeth.SOURCE).
# Exit 77 = skipped (no network). Sentinel: the `replayer=sawteeth` line of render. Asserts per song: the plugin ran, the WAV is not silent
# (SWT_ENERGY_MIN raises the floor, for red proofs) and the rendered frames are a plausible song length (SWT_MIN_SECONDS, when set, replaces the floor).
# Negative cases (.st is shared with Ultimate Soundtracker and other trackers): a soundtracker-style file, a zero file, a header-only file and a
# song cut off are not claimed or not played as sawteeth.
# usage: sawteeth_decode.sh <selecta-cli>
set -u
cli=$1
here=$(dirname "$0")
U="https://raw.githubusercontent.com/pulkomandy/Sawteeth/9fe13fe32d9b5a65fe50a96605f6c2f69e440e34/songs/Spruce"
field() { printf '%s\n' "$1" | tr ' ' '\n' | sed -n "s/^$2=//p" | head -1; }
fail() { echo "FAIL: $*"; exit 1; }
energy() { od -An -v -t d2 -j 44 "$1" | tr -s " " "\n" | awk '$1 != "" { s += $1 * $1 } END { printf "%d", s }'; }
rate=44100
fetch() { "$here/fetch_fixture.sh" "$1" "$2" "$3" || exit $?; }
check() { # <song path> <min seconds> <max seconds>
  rm -f fd_swt.wav
  out=$("$cli" render "$1" --output fd_swt.wav --seconds 600 --rate "$rate" 2>/dev/null) || fail "render failed: $1"
  [ "$(field "$out" replayer)" = "sawteeth" ] || fail "$1: replayer=$(field "$out" replayer), want sawteeth"
  got=$(field "$out" rendered_frames)
  min=${SWT_MIN_SECONDS:-$2}
  [ "$got" -ge $((rate * min)) ] || fail "$1: rendered_frames=$got, want at least $((rate * min))"
  [ "$got" -le $((rate * $3)) ] || fail "$1: rendered_frames=$got, far beyond the song length"
  e=$(energy fd_swt.wav)
  [ "$e" -gt "${SWT_ENERGY_MIN:-1000000}" ] || fail "$1: silent output (energy $e)"
  echo "$1 frames=$got energy=$e"
  rm -f fd_swt.wav
}
s1=$(fetch "$U/64k.st" f11f5d41754f18e37ef73f306f3482e98d4076dbbc6b5925b9e6318020110ef2 swt_64k.st) || exit $?
s2=$(fetch "$U/glimmer.st" 01e23763b0b6ff707c2d02b1b1b2866b4a5e8fb6e9eb15ca8b7f8ae8c00928d0 swt_glimmer.st) || exit $?
s3=$(fetch "$U/woi.st" 77a6b849adec6e8461921c15aae914ad7bf48c5f3fbffb310153643641836a4c swt_woi.st) || exit $?
check "$s1" 2 600
check "$s2" 2 600
check "$s3" 2 600
# A soundtracker-style .st file (no Sawteeth magic) is not claimed.
head -c 20000 /dev/zero | tr '\0' 'A' > tracker.st
out=$("$cli" render tracker.st --output fd_swt.wav --seconds 1 --rate "$rate" 2>/dev/null) || true
[ "$(field "$out" replayer)" != "sawteeth" ] || fail "sawteeth claimed a soundtracker-style .st file"
# A zero-filled .st is not claimed.
head -c 20000 /dev/zero > zero.st
out=$("$cli" render zero.st --output fd_swt.wav --seconds 1 --rate "$rate" 2>/dev/null) || true
[ "$(field "$out" replayer)" != "sawteeth" ] || fail "sawteeth claimed a zero-filled .st file"
# A bare magic with nothing behind it is refused by the loader, not played.
printf 'SWTD\000\000\000\000\000\000' > magic.st
out=$("$cli" render magic.st --output fd_swt.wav --seconds 1 --rate "$rate" 2>/dev/null) || true
[ "$(field "$out" replayer)" != "sawteeth" ] || fail "sawteeth played a header-only file"
# A song cut off inside its data is refused, not played as sawteeth.
head -c 500 "$s1" > cutoff.st
out=$("$cli" render cutoff.st --output fd_swt.wav --seconds 1 --rate "$rate" 2>/dev/null) || true
[ "$(field "$out" replayer)" != "sawteeth" ] || fail "sawteeth played a song cut off inside its data"
rm -f tracker.st zero.st magic.st cutoff.st fd_swt.wav
echo sawteeth_decode_ok
