#!/bin/sh
# T64 library-search (plan 2026-10-25-m3-library.md, Phase 2, I149).
# Drives `selecta-cli library search`, which sends LIBRARY_SEARCH to the engine
# with a library DB behind it (catalogue.tsv imported), over every row of
# search.golden. Checks the result keys, src_count= and the fts_queries= sentinel
# (1 when the FTS5 index served the query, 0 for compiled-SQL and regex queries).
# Prints `library_searches=<n>`; fails unless n equals the row count.
# usage: library_search.sh <selecta-cli> <catalogue.tsv> <search.golden>
set -u
cli=$1
cat=$2
golden=$3
TAB=$(printf '\t')
rows=$(grep -vc '^#' "$golden")
n=0
fail=0
while IFS="$TAB" read -r q fts keys; do
    case $q in '#'*|'') continue ;; esac
    out=$("$cli" library search --catalogue "$cat" -- "$q" 2>&1)
    case $keys in
    'ERROR '*)
        if [ "$out" = "$keys" ]; then n=$((n + 1)); else echo "[ERROR] '$q': expected '$keys' got '$out'"; fail=1; fi
        continue
        ;;
    esac
    gfts=$(printf '%s\n' "$out" | sed -n 's/^fts_queries=//p')
    gkeys=$(printf '%s\n' "$out" | sed -n 's/^key=//p' | tr '\n' ' ' | sed 's/ $//')
    gcount=$(printf '%s\n' "$out" | sed -n 's/^src_count=//p')
    want=0
    for k in $keys; do want=$((want + 1)); done
    if [ "$gfts" = "$fts" ] && [ "$gkeys" = "$keys" ] && [ "$gcount" = "$want" ]; then
        n=$((n + 1))
    else
        echo "[ERROR] '$q': fts '$gfts' (want $fts), keys '$gkeys' (want '$keys'), src_count '$gcount' (want $want)"
        fail=1
    fi
done < "$golden"
echo "library_searches=$n"
[ "$fail" = 0 ] && [ "$n" = "$rows" ]
