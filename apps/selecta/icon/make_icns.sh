#!/bin/sh
# Render the SVG icon source into a macOS .icns (I1023). Usage: make_icns.sh <in.svg> <out.icns>
# Renderer: rsvg-convert when present, else Quick Look (qlmanage) plus sips, both ship with macOS.
set -eu
SVG=$1
OUT=$2
WORK=$(mktemp -d "${TMPDIR:-/tmp}/selecta-icon.XXXXXX")
trap 'rm -rf "$WORK"' EXIT
SET="$WORK/Selecta.iconset"
mkdir -p "$SET"
if command -v rsvg-convert >/dev/null 2>&1; then
    render() { rsvg-convert -w "$1" -h "$1" "$SVG" -o "$2"; }
else
    qlmanage -t -s 1024 -o "$WORK" "$SVG" >/dev/null 2>&1
    BIG="$WORK/$(basename "$SVG").png"
    [ -f "$BIG" ] || { echo "[ERROR] no SVG renderer: install librsvg (rsvg-convert)" >&2; exit 1; }
    render() { sips -z "$1" "$1" "$BIG" --out "$2" >/dev/null; }
fi
for s in 16 32 128 256 512; do
    render "$s" "$SET/icon_${s}x${s}.png"
    render "$((s * 2))" "$SET/icon_${s}x${s}@2x.png"
done
iconutil -c icns "$SET" -o "$OUT"
