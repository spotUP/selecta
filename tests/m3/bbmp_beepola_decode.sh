#!/bin/sh
# I1223b: a Beepola song (.bbsong, ZX Spectrum beeper) plays through `selecta-cli render` as part of the bbmp_ay RENDER replayer.
# The engine can need zx48.rom, p1d-samples.bin and svg-table-tail.bin; this test gets them through the I751 downloader
# (`selecta-cli support get beepola-data`, consent pref set in a temp config dir, SHA-256 checked) into a cache dir and hands
# the folder to the engine through BBMP_BEEPOLA_DATA. A truncated .bbsong must be refused. The song is fetched by checksum,
# never committed. Exit 77 = no network.
# usage: bbmp_beepola_decode.sh <selecta-cli>
set -u
cli=$1
here=$(dirname "$0")
M=https://modland.com/pub/modules
field() { printf '%s\n' "$1" | tr ' ' '\n' | sed -n "s/^$2=//p" | head -1; }
fail() { echo "FAIL: $*"; exit 1; }
energy() { od -An -v -t d2 -j 44 "$1" | tr -s " " "\n" | awk '$1 != "" { s += $1 * $1 } END { printf "%d", s }'; }
rate=44100
f=$("$here/fetch_fixture.sh" "$M/Beepola/4mat/plastic%20galaxy.bbsong" 906cbd75022bc0e38a004bbcba7ca9a6707aaa4e26189163645fd158cf42406f plastic_galaxy.bbsong) || exit $?
t=$PWD/beepola-cfg
rm -rf "$t"
mkdir -p "$t/cfg" "$t/cache"
echo "library.support_downloads = on" > "$t/cfg/prefs.conf"
"$cli" support get beepola-data --cache-dir "$t/cache" --config-dir "$t/cfg" >"$t/get.log" 2>&1 || { cat "$t/get.log"; echo "SKIP: cannot fetch the Beepola data files"; exit 77; }
[ "$(ls "$t/cache/support" | wc -l)" -ge 3 ] || fail "support get left fewer than 3 files in $t/cache/support"
export BBMP_BEEPOLA_DATA="$t/cache/support"
out=$("$cli" render "$f" --output fd_bb.wav --seconds 30 --rate "$rate" 2>/dev/null) || fail "render failed"
[ "$(field "$out" replayer)" = "bbmp_ay" ] || fail "replayer=$(field "$out" replayer), want bbmp_ay"
got=$(field "$out" rendered_frames)
[ "$got" -ge $((rate * ${BB_MIN_SECONDS:-10})) ] || fail "rendered_frames=$got, want at least $((rate * ${BB_MIN_SECONDS:-10}))"
e=$(energy fd_bb.wav)
[ "$e" -gt "${BB_ENERGY_MIN:-1000000}" ] || fail "silent output (energy $e)"
echo "plastic_galaxy.bbsong frames=$got energy=$e"
head -c 40 "$f" > trunc.bbsong
if "$cli" render trunc.bbsong --output fd_bb2.wav --seconds 5 --rate "$rate" >/dev/null 2>&1; then fail "a truncated .bbsong was accepted"; fi
rm -rf "$t" fd_bb.wav fd_bb2.wav trunc.bbsong
echo bbmp_beepola_decode_ok
