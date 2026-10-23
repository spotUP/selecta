#!/bin/sh
# T65 play-query driver: runs play_query.sel through `selecta-cli script` and
# diffs the output with play_query.expected. Prints play_query_ok on a match.
# usage: play_query.sh <selecta-cli> <catalogue.tsv> <play_query.sel> <expected>
set -u
out=$("$1" script "$3" --catalogue "$2" 2>&1)
if [ "$out" = "$(cat "$4")" ]; then echo play_query_ok; else printf '%s\n' "$out" | diff - "$4"; exit 1; fi
