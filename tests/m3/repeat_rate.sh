#!/bin/sh
# I1429 driver: runs repeat_rate.sel; any ERROR line (no_song) fails. usage: repeat_rate.sh <selecta-cli> <catalogue.tsv> <sel>
set -u
out=$("$1" script "$3" --catalogue "$2" --db repeat_rate.db 2>&1 | grep -E '^ERROR')
rm -f repeat_rate.db
[ -z "$out" ] || { echo "repeat rate: $out"; exit 1; }
echo repeat_rate_ok
