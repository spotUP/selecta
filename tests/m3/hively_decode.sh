#!/bin/sh
# I1242j: an AHX song plays through `selecta-cli render` as a RENDER-kind replayer (hively module over the HivelyTracker
# replayer). One real song, "raining stars" by AM-FM (Modland AHX/AM-FM), fetched by checksum, never committed (scene
# files are not bundled); exit 77 = skipped (no network). One reachability test through the product entry point; the
# sentinel is the `replayer=hively` line of render. Asserts: the plugin ran for the suffix spelling (raining stars.ahx)
# and for the Modland prefix spelling (ahx.raining stars), the WAV is not silent (HVL_ENERGY_MIN raises the floor, for
# red proofs), the song lasts at least HVL_MIN_SECONDS (default 10), and a file that is not an AHX song is not claimed.
# usage: hively_decode.sh <selecta-cli>
set -u
cli=$1
here=$(dirname "$0")
field() { printf '%s\n' "$1" | tr ' ' '\n' | sed -n "s/^$2=//p" | head -1; }
fail() { echo "FAIL: $*"; exit 1; }
energy() { od -An -v -t d2 -j 44 "$1" | tr -s " " "\n" | awk '$1 != "" { s += $1 * $1 } END { printf "%d", s }'; }
rate=44100
f=$("$here/fetch_fixture.sh" "https://modland.com/pub/modules/AHX/AM-FM/raining%20stars.ahx" \
      d157c0a2bc840b98741ba1de6148510b1d8a4aeb948bf5aa92931e52b280386f hively_raining_stars.ahx) || exit $?
rm -rf fd_hvl && mkdir fd_hvl || fail "cannot stage"
cp "$f" "fd_hvl/raining stars.ahx" && cp "$f" "fd_hvl/ahx.raining stars" || fail "cannot stage $f"
for name in "raining stars.ahx" "ahx.raining stars"; do
  rm -f fd_hvl.wav
  out=$("$cli" render "fd_hvl/$name" --output fd_hvl.wav --seconds 300 --rate "$rate" 2>/dev/null) || fail "render failed: $name"
  [ "$(field "$out" replayer)" = "hively" ] || fail "$name: replayer=$(field "$out" replayer), want hively"
  got=$(field "$out" rendered_frames)
  [ "$got" -ge $((rate * ${HVL_MIN_SECONDS:-10})) ] || fail "$name: rendered_frames=$got, want at least $((rate * ${HVL_MIN_SECONDS:-10}))"
  e=$(energy fd_hvl.wav)
  [ "$e" -gt "${HVL_ENERGY_MIN:-1000000}" ] || fail "$name: silent output (energy $e)"
done
# Not an AHX song: a text file named .ahx is not claimed by hively.
printf 'this is not an AHX song, only text with the extension\n' > fd_hvl/notahx.ahx
out=$("$cli" render fd_hvl/notahx.ahx --output fd_hvl_bad.wav --seconds 5 --rate "$rate" 2>/dev/null)
[ "$(field "$out" replayer)" != "hively" ] || fail "a text file named .ahx was claimed by hively"
rm -rf fd_hvl fd_hvl.wav fd_hvl_bad.wav
echo "hively_decode_ok frames=$got energy=$e"
