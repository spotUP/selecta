#!/bin/sh
# I699: an MDX song (.mdx) plays through `selecta-cli render` as a RENDER-kind replayer (mdxmini module over the mdxmini
# library, GPL-2.0-or-later). One reachability test through the product entry point; the sentinel is the `replayer=mdxmini`
# line of render. Fixture: tests/m3/fixtures/mdx_test.mdx (source in mdx_test.mdx.SOURCE). Asserts: the plugin ran, the
# WAV is not silent (energy of the 16-bit samples) and the rendered frame count equals 10 s +-1% at the output rate.
# Needs SELECTA_PLUGIN_PATH to hold the mdxmini module (set by the test registration); with the module absent the
# render fails and so does this test.
# usage: mdxmini_decode.sh <selecta-cli> <mdx-file>
set -u
cli=$1
mdx=$2
field() { printf '%s\n' "$1" | tr ' ' '\n' | sed -n "s/^$2=//p" | head -1; }
fail() { echo "FAIL: $*"; exit 1; }
rate=44100
rm -f fd_mdx.wav
out=$("$cli" render "$mdx" --output fd_mdx.wav --seconds 10 --rate "$rate" 2>/dev/null) || fail "render failed"
[ "$(field "$out" replayer)" = "mdxmini" ] || fail "replayer=$(field "$out" replayer), want mdxmini"
got=$(field "$out" rendered_frames)
awk -v g="$got" -v w=$((rate * 10)) 'BEGIN { d = g - w; if (d < 0) d = -d; exit !(d <= w * 0.01) }' ||
    fail "rendered_frames=$got, want $((rate * 10)) +-1%"
e=$(od -An -v -t d2 -j 44 fd_mdx.wav | tr -s " " "\n" | awk '$1 != "" { s += $1 * $1 } END { printf "%d", s }')
[ "$e" -gt 1000000 ] || fail "silent output (energy $e)"
echo mdxmini_decode_ok
