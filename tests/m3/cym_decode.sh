#!/bin/sh
# I710l: Megadrive CYM (.cym, a Capcom CPS-1 YM2151 register log) plays through `selecta-cli render` as a RENDER-kind replayer: the libvgm
# module turns the log into a VGM 1.51 file for one YM2151 and plays it. One real song fetched by checksum, never committed
# (tests/m3/fixtures/bbmp_cym.SOURCE). Exit 77 = skipped (no network). Sentinel: the `replayer=libvgm` line of render. Asserts: the plugin
# ran, the WAV is not silent (CYM_ENERGY_MIN raises the floor, for red proofs) and the rendered frames match the log's length, 2313 waits
# of 1/60 s = 38.55 s (CYM_MIN_SECONDS, when set, replaces the floor, for red proofs). A log with no writes or no waits is not claimed.
# usage: cym_decode.sh <selecta-cli>
set -u
cli=$1
here=$(dirname "$0")
field() { printf '%s\n' "$1" | tr ' ' '\n' | sed -n "s/^$2=//p" | head -1; }
fail() { echo "FAIL: $*"; exit 1; }
energy() { od -An -v -t d2 -j 44 "$1" | tr -s " " "\n" | awk '$1 != "" { s += $1 * $1 } END { printf "%d", s }'; }
rate=44100
s1=$("$here/fetch_fixture.sh" "https://modland.com/pub/modules/Megadrive%20CYM/_unknown/-%20misc/sample.cym" \
  dd5c788e01235eec8267f0536078471c10412623cf0bcf722eb8dcc5d23f81da sample.cym) || exit $?
rm -f fd_cym.wav
out=$("$cli" render "$s1" --output fd_cym.wav --seconds 120 --rate "$rate" 2>/dev/null) || fail "render failed: $s1"
[ "$(field "$out" replayer)" = "libvgm" ] || fail "replayer=$(field "$out" replayer), want libvgm"
got=$(field "$out" rendered_frames)
min=${CYM_MIN_SECONDS:-38}
[ "$got" -ge $((rate * min)) ] || fail "rendered_frames=$got, want at least $((rate * min))"
[ "$got" -le $((rate * 40)) ] || fail "rendered_frames=$got, far beyond the song length"
e=$(energy fd_cym.wav)
[ "$e" -gt "${CYM_ENERGY_MIN:-1000000}" ] || fail "silent output (energy $e)"
echo "$s1 frames=$got energy=$e"
rm -f fd_cym.wav
# A log with waits but no register write is not a CYM song and is not claimed, whatever its name says.
head -c 5000 /dev/zero > empty.cym
out=$("$cli" render empty.cym --output fd_cym.wav --seconds 1 --rate "$rate" 2>/dev/null) || true
[ "$(field "$out" replayer)" != "libvgm" ] || fail "libvgm claimed empty.cym, which has no register write"
rm -f empty.cym fd_cym.wav
echo cym_decode_ok
