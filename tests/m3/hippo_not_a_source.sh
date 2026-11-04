#!/bin/sh
# I1064: sync the offline hippo fixture with the CLI, then run the engine test over that library.
# usage: hippo_not_a_source.sh <selecta-cli> <hippo_not_a_source_test> <tests/m3 dir>
set -eu
root=$(mktemp -d "${TMPDIR:-/tmp}/sel-i1064.XXXXXX")
trap 'rm -rf "$root"' EXIT
mkdir "$root/json"
cp "$3"/hippo/hippo_list.json "$root/json/"
"$1" library sync --db "$root/h.db" --now 1792756800 hippo "$root/json" >/dev/null
"$2" "$root/h.db"
