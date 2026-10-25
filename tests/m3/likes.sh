#!/bin/sh
# I306 driver: runs likes.sel through `selecta-cli script` and diffs the RESULTS and ERROR lines with likes.expected.
# usage: likes.sh <selecta-cli> <catalogue.tsv> <likes.sel> <expected>
set -u
out=$("$1" script "$3" --catalogue "$2" 2>&1 | grep -E '^(RESULTS |ERROR)')
if [ "$out" = "$(cat "$4")" ]; then echo likes_ok; else printf '%s\n' "$out" | diff - "$4"; exit 1; fi
