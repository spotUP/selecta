#!/bin/sh
# I198e: a one-letter search is a per-row scan; it returns at most the queue capacity (256 on an a500) keys
# and reports truncated=1 instead of counting the rest. 400 rows all contain the letter "q".
# usage: library_scan_cap.sh <selecta-cli> <scratch dir>
set -u
cli=$1
dir=$2
mkdir -p "$dir"
tsv="$dir/scan_cap.tsv"
awk 'BEGIN { for (i = 0; i < 400; i++) printf "q%03d.mod\tlocal\tq%03d.mod\tTitle %03d\tartist\tmod\t1990\n", i, i, i }' > "$tsv"
out=$("$cli" library search --catalogue "$tsv" --machine a500 -- q 2>&1)
count=$(printf '%s\n' "$out" | sed -n 's/^src_count=//p')
trunc=$(printf '%s\n' "$out" | sed -n 's/^truncated=//p')
keys=$(printf '%s\n' "$out" | grep -c '^key=')
echo "scan_cap count=$count keys=$keys truncated=$trunc"
[ "$count" = 256 ] && [ "$keys" = 256 ] && [ "$trunc" = 1 ]
