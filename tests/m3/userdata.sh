#!/bin/sh
# T78 user-data driver (history part, I170): runs userdata.sel through `selecta-cli script` and diffs the
# LIBRARY (sentinel), RESULTS and ERROR lines with userdata.expected. Prints userdata_ok on a match.
# usage: userdata.sh <selecta-cli> <catalogue.tsv> <userdata.sel> <expected>
set -u
out=$("$1" script "$3" --catalogue "$2" 2>&1 | grep -E '^(LIBRARY |RESULTS |ERROR)')
if [ "$out" = "$(cat "$4")" ]; then echo userdata_ok; else printf '%s\n' "$out" | diff - "$4"; exit 1; fi
