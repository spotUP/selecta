#!/bin/sh
# I437: `library scan` over a folder holding the tagged mp3 and flac fixtures stores the embedded 200x200 cover of each
# as an artwork row. Reachability: the rows exist only when the scan's cover reader ran (scan -> SEL_ART_STORE -> art.c).
# The flac is only visited when a replayer claims .flac (no flac decoder yet, I436); then its row is required too.
# usage: embedded_cover.sh <selecta-cli> <fixture-dir>
set -u
cli=$1
dir=$2
fail() { echo "FAIL: $*"; exit 1; }
root=$(mktemp -d "${TMPDIR:-/tmp}/sel-i437.XXXXXX")
trap 'rm -rf "$root"' EXIT
mkdir -p "$root/music"
cp "$dir/cover_tagged.mp3" "$dir/cover_tagged.flac" "$root/music/"
out=$("$cli" library scan --db "$root/lib.db" --dump "$root/music") || fail "scan failed"
echo "$out"
echo "$out" | grep -q '^art=.*|cover|[0-9]*|200x200$' || fail "no 200x200 cover row for the mp3"
n=$(echo "$out" | grep -c '^art=')
if echo "$out" | grep -q 'cover_tagged.flac|'; then
    [ "$n" = 1 ] || fail "flac scanned as a song but art rows=$n (want 1 shared album row)"
fi
echo embedded_cover_ok
