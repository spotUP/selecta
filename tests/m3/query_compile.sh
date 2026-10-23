#!/bin/sh
# T63 query-compile (plan 2026-10-25-m3-library.md, Phase 2, I147-I148).
# Drives `selecta-cli library query --explain` over every row of query.golden
# (query TAB sql TAB params; `#now N` and `#view name=query` header lines) and
# compares the SQL and the bound parameters. Error rows expect `ERROR <code>`.
# Prints `query_compiles=<n>`; the test fails unless n equals the row count.
# usage: query_compile.sh <selecta-cli> <query.golden>
set -u
cli=$1
golden=$2
TAB=$(printf '\t')
now=$(sed -n 's/^#now //p' "$golden" | head -n 1)
rows=$(grep -vc '^#' "$golden")
set -- --now "$now"
for v in $(sed -n 's/^#view //p' "$golden"); do set -- "$@" --view "$v"; done
n=0
fail=0
while IFS="$TAB" read -r q sql params; do
    case $q in '#'*|'') continue ;; esac
    out=$("$cli" library query --explain "$@" -- "$q" 2>&1)
    case $sql in
    'ERROR '*)
        if [ "$out" = "$sql" ]; then n=$((n + 1)); else echo "[ERROR] '$q': expected '$sql' got '$out'"; fail=1; fi
        ;;
    *)
        gsql=$(printf '%s\n' "$out" | sed -n 's/^sql=//p')
        gpar=$(printf '%s\n' "$out" | sed -n 's/^params=//p')
        terms=$(printf '%s\n' "$out" | sed -n 's/^query_terms=//p')
        if [ "$gsql" = "$sql" ] && [ "$gpar" = "$params" ] && [ "${terms:-0}" -ge 1 ]; then
            n=$((n + 1))
        else
            echo "[ERROR] '$q': expected sql '$sql' params '$params'; got '$out'"; fail=1
        fi
        ;;
    esac
done < "$golden"
echo "query_compiles=$n rows=$rows"
[ "$fail" = 0 ] && [ "$n" = "$rows" ]
