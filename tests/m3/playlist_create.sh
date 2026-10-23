#!/bin/sh
# I178a driver: QUEUE_ADD two songs, PLAYLIST_CREATE, then the DB holds the playlist with both songs in queue order.
# usage: playlist_create.sh <selecta-cli> <catalogue.tsv> <playlist_create.sel>
set -u
cli=$1
rm -rf plc.db plc_export
out=$("$cli" script "$3" --catalogue "$2" --db plc.db 2>&1) || { echo "$out"; exit 1; }
printf '%s\n' "$out" | grep -q '^  P 1 Saved' || { echo "playlist missing from tree:"; printf '%s\n' "$out"; exit 1; }
exp=$("$cli" library export-folder --db plc.db --dir plc_export --folder 1) || { echo "$exp"; exit 1; }
[ "$exp" = "exported=1 folder=1" ] || { echo "export said: $exp"; exit 1; }
printf '#EXTM3U\nselecta-test-mk.mod\nselecta-test.mod\n' > plc_want.m3u
cat plc_export/*.m3u | diff - plc_want.m3u || { echo "playlist entries differ"; exit 1; }
rm -rf plc.db plc_export plc_want.m3u
echo plc_ok
