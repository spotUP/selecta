#!/bin/sh
# I184d: local MP3 and Ogg Vorbis files are read through a file-backed byte stream, not whole.
# Scan sentinel: `library scan` prints bytes_read, the bytes read from song files; it must stay below the file size.
# Play sentinel: the `@file_stream_stats` script directive prints FILESTREAM opens=N bytes_read=M after OPEN + @render;
# opens >= 1 proves the engine play path opened the file stream, and the render must produce frames.
# Fixtures smaller than the 16 KiB head read are read in full, so the MP3 is the fixture repeated until it is larger.
# usage: file_stream.sh <selecta-cli> <fixture-dir>
set -u
cli=$1
dir=$2
fail() { echo "FAIL: $*"; exit 1; }
field() { printf '%s\n' "$1" | tr ' ' '\n' | sed -n "s/^$2=//p" | head -1; }
root=$(mktemp -d "${TMPDIR:-/tmp}/sel-i184d.XXXXXX")
trap 'rm -rf "$root"' EXIT
mkdir -p "$root/mp3" "$root/ogg"
i=0
while [ $i -lt 20 ]; do cat "$dir/sine.mp3"; i=$((i + 1)); done > "$root/mp3/big.mp3"
cp "$dir/long.ogg" "$root/ogg/long.ogg"

for kind in mp3 ogg; do
    f=$(ls "$root/$kind"/*)
    size=$(wc -c < "$f" | tr -d ' ')
    out=$("$cli" library scan --db "$root/$kind.db" "$root/$kind") || fail "scan $kind failed"
    echo "$out"
    [ "$(field "$out" db_songs)" = 1 ] || fail "$kind: db_songs != 1"
    got=$(field "$out" bytes_read)
    [ -n "$got" ] || fail "$kind: no bytes_read field"
    [ "$got" -gt 0 ] || fail "$kind: bytes_read=0, scan read nothing"
    [ "$got" -lt "$size" ] || fail "$kind: bytes_read=$got is not below file size $size"

    printf '@dump %s\nOPEN %s\nPLAY\n@render 4410\n@dump off\n@file_stream_stats\n' "$root/$kind.raw" "$f" > "$root/$kind.sel"
    out=$("$cli" script "$root/$kind.sel" 2>&1) || fail "script $kind failed: $out"
    echo "$out" | tail -5
    line=$(echo "$out" | grep '^FILESTREAM') || fail "$kind: no FILESTREAM line"
    [ "$(field "$line" opens)" -ge 1 ] || fail "$kind: play did not open the file stream"
    [ "$(wc -c < "$root/$kind.raw" | tr -d ' ')" = $((4410 * 4)) ] || fail "$kind: rendered block is not 4410 frames"
    energy=$(od -An -v -t d2 "$root/$kind.raw" | tr -s " " "\n" | awk '$1 != "" { s += $1 * $1 } END { printf "%d", s }')
    [ "$energy" -gt 1000000 ] || fail "$kind: play rendered silence (energy $energy)"
    echo "$out" | grep -q "no_replayer\|open_failed" && fail "$kind: play failed"
done
echo file_stream_ok
