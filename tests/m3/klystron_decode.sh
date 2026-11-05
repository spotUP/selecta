#!/bin/sh
# I708a: a real klystrack song (.kt) plays through `selecta-cli render` as a RENDER-kind replayer (klystron module).
# The song is the upstream example songs/ringmod.kt (kometbomb/klystrack); the upstream licence for the songs is not stated,
# so it is fetched by checksum, never committed (tests/m3/fixtures/klystron.SOURCE). Exit 77 = skipped (no network).
# One reachability test through the product entry point; the sentinel is the `replayer=klystron` line of render.
# Asserts: the plugin ran, the WAV is not silent (KT_ENERGY_MIN raises the floor for red proofs) and the song lasts at
# least KT_MIN_SECONDS (default 9: ringmod.kt plays 7.7 s plus a 2 s tail).
# usage: klystron_decode.sh <selecta-cli> [local .kt file instead of the fetched one]
set -u
cli=$1
here=$(dirname "$0")
f=${2:-}
field() { printf '%s\n' "$1" | tr ' ' '\n' | sed -n "s/^$2=//p" | head -1; }
fail() { echo "FAIL: $*"; exit 1; }
energy() { od -An -v -t d2 -j 44 "$1" | tr -s " " "\n" | awk '$1 != "" { s += $1 * $1 } END { printf "%d", s }'; }
if [ -z "$f" ]; then
  f=$("$here/fetch_fixture.sh" https://raw.githubusercontent.com/kometbomb/klystrack/master/examples/songs/ringmod.kt \
        b79cb551c216338b873b992a29772b9618f43c8ec21c017eb947a1ed124ba9f1 ringmod.kt) || exit $?
fi
rate=44100
rm -f fd_kt.wav
out=$("$cli" render "$f" --output fd_kt.wav --seconds 300 --rate "$rate" 2>/dev/null) || fail "render failed: $f"
[ "$(field "$out" replayer)" = "klystron" ] || fail "$f: replayer=$(field "$out" replayer), want klystron"
got=$(field "$out" rendered_frames)
[ "$got" -ge $((rate * ${KT_MIN_SECONDS:-9})) ] || fail "$f: rendered_frames=$got, want at least $((rate * ${KT_MIN_SECONDS:-9}))"
e=$(energy fd_kt.wav)
[ "$e" -gt "${KT_ENERGY_MIN:-1000000}" ] || fail "$f: silent output (energy $e)"
rm -f fd_kt.wav
echo "klystron_decode_ok frames=$got energy=$e"
