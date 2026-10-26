#!/bin/sh
# I437: `library scan` over a folder holding the tagged mp3 and flac fixtures stores the embedded 200x200 cover of each
# as an artwork row. Reachability: the rows exist only when the scan's cover reader ran (scan -> SEL_ART_STORE -> art.c).
# The flac is visited because the dr_flac replayer claims .flac (I436); it shares the mp3's album row.
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
echo "$out" | grep -q 'cover_tagged.flac|' || fail "the flac was not scanned as a song"
n=$(echo "$out" | grep -c '^art=')
[ "$n" = 1 ] || fail "art rows=$n (want 1 album row shared by the mp3 and the flac)"
echo embedded_cover_ok
