#!/bin/sh
# I1264: Music Editor songs MED2, MED3 and MED4 play through `selecta-cli render` as a RENDER-kind replayer (libxmp).
# One reachability test through the product entry point; the sentinel is the `replayer=xmp` line of render (the engine's
# priority walk picked the xmp plugin and ran its adapter). Fixtures: tests/m3/fixtures/xmp_test.med{2,3,4} (generated
# by make_xmp_med_fixtures.py, see xmp_test.med.SOURCE; empty song files, so the output is silent: the test checks the
# plugin, a finite song length, and that the list of claimed formats names med). Each song is one 64-row pattern, so the
# render ends with the song, well before --seconds 20.
# usage: xmp_med_decode.sh <selecta-cli> <fixture-dir>
set -u
cli=$1
dir=$2
field() { printf '%s\n' "$1" | tr ' ' '\n' | sed -n "s/^$2=//p" | head -1; }
fail() { echo "FAIL: $*"; exit 1; }
for ext in med2 med3 med4; do
    f="$dir/xmp_test.$ext"
    out=$("$cli" render "$f" --output fd_med.wav --seconds 20 --rate 44100 2>/dev/null) || fail "render failed: $f"
    [ "$(field "$out" replayer)" = "xmp" ] || fail "$f: replayer=$(field "$out" replayer), want xmp"
    got=$(field "$out" rendered_frames)
    [ -n "$got" ] && [ "$got" -gt 44100 ] && [ "$got" -lt 882000 ] || fail "$f: rendered_frames=$got, want a finite song"
done
fm=$("$cli" plugins --formats xmp 2>&1) || fail "plugins --formats xmp: $fm"
printf '%s\n' "$fm" | grep -q "^player med\$" || fail "format med not listed: $fm"
echo xmp_med_decode_ok
