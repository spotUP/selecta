#!/bin/sh
# T96 desktop half (plan 2026-10-25-m3-library.md, Phase 12, I197/I143, D-M3-16).
# `library export-index` builds a read-only index of one source; `library search|stats --index` ATTACHes it
# beside a writable user DB holding the other rows. Search must give the T64 golden keys across both files,
# stats must count both, and the index file must stay byte-identical (read-only ATTACH).
# Prints `index_checks=<n>`; fails unless all checks pass.
# usage: index_attach.sh <selecta-cli> <catalogue.tsv> <search.golden> <workdir>
set -u
cli=$1
cat=$2
golden=$3
work=$4
TAB=$(printf '\t')
fail=0
n=0
rm -rf "$work"
mkdir -p "$work"

check() { # name, want, got
    if [ "$2" = "$3" ]; then n=$((n + 1)); else echo "RED[T96]: $1: want '$2' got '$3'"; fail=1; fi
}

grep -v '^modland' "$cat" > "$work/local.tsv"
"$cli" userdata --catalogue "$cat" --db "$work/all.db" > /dev/null
"$cli" userdata --catalogue "$work/local.tsv" --db "$work/user.db" > /dev/null
out=$("$cli" library export-index --db "$work/all.db" --out "$work/idx.db" modland 2>&1)
check export_rows 3 "$(printf '%s\n' "$out" | sed -n 's/^index_rows=\([0-9]*\) .*/\1/p')"
before=$(cksum < "$work/idx.db")

while IFS="$TAB" read -r q fts keys; do
    case $q in '#'*|'') continue ;; esac
    case $keys in 'ERROR '*) continue ;; esac
    out=$("$cli" library search --db "$work/user.db" --index "$work/idx.db" -- "$q" 2>&1)
    gkeys=$(printf '%s\n' "$out" | sed -n 's/^key=//p' | tr '\n' ' ' | sed 's/ $//')
    check "search '$q'" "$keys" "$gkeys"
done < "$golden"

out=$("$cli" library stats --db "$work/user.db" --index "$work/idx.db" 2>&1)
check stats_songs 5 "$(printf '%s\n' "$out" | sed -n 's/.* songs=\([0-9]*\).*/\1/p')"
check stats_index_songs 3 "$(printf '%s\n' "$out" | sed -n 's/^index_songs=\([0-9]*\) .*/\1/p')"
check index_untouched "$before" "$(cksum < "$work/idx.db")"

out=$("$cli" library search --db "$work/user.db" --index "$work/missing.db" -- purple 2>&1)
check missing_index "ERROR index_open" "$out"

echo "index_checks=$n"
[ "$fail" = 0 ]
