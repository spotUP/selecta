#!/bin/sh
# I431: VGM and gzip-packed VGZ play through `selecta-cli render` as a RENDER-kind replayer (libvgm). One
# reachability test through the product entry point; the sentinel is the `replayer=libvgm` line of render (the engine
# picked the plugin and ran its adapter). Fixtures: tests/m3/fixtures/vgm_test.vgm and vgm_test.vgz (hand made, 4 s
# SN76489 tone, see vgm_test.SOURCE). Asserts per file: the plugin ran, the WAV is not silent (energy of the 16-bit
# samples) and the rendered frame count equals the 4 s song at the output rate +-1% (--seconds 10 is longer, so the
# song's own end stops the render). Then `plugins --formats libvgm` lists the formats.
# usage: libvgm_decode.sh <selecta-cli> <vgm-file> <vgz-file>
set -u
cli=$1
field() { printf '%s\n' "$1" | tr ' ' '\n' | sed -n "s/^$2=//p" | head -1; }
fail() { echo "FAIL: $*"; exit 1; }
rate=44100
for f in "$2" "$3"; do
    rm -f fd_libvgm.wav
    out=$("$cli" render "$f" --output fd_libvgm.wav --seconds 10 --rate "$rate" 2>/dev/null) || fail "render failed: $f"
    [ "$(field "$out" replayer)" = "libvgm" ] || fail "$f: replayer=$(field "$out" replayer), want libvgm"
    got=$(field "$out" rendered_frames)
    awk -v g="$got" -v w=$((rate * 4)) 'BEGIN { d = g - w; if (d < 0) d = -d; exit !(d <= w * 0.01) }' ||
        fail "$f: rendered_frames=$got, want $((rate * 4)) +-1%"
    e=$(od -An -v -t d2 -j 44 fd_libvgm.wav | tr -s " " "\n" | awk '$1 != "" { s += $1 * $1 } END { printf "%d", s }')
    [ "$e" -gt 1000000 ] || fail "$f: silent output (energy $e)"
done
fm=$("$cli" plugins --formats libvgm 2>&1) || fail "plugins --formats libvgm: $fm"
for id in vgm vgz s98 dro gym; do
    printf '%s\n' "$fm" | grep -q "^player $id\$" || fail "format $id not listed: $fm"
done
echo libvgm_decode_ok
