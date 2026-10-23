#!/bin/sh
# T76 cache-budget (row I167): `selecta-cli library trim` over a fixture cache. Six files of 1000 bytes
# whose modification times (the last-access stamp) ascend with their number; file 1 is kept, file 2
# liked. Budget 3500 evicts the oldest unmarked files (3, 4, 5) and stops once the total fits;
# the default budget per fake machine tier is the sel_cache_budget table in src/library/cache.c.
# usage: cache_budget.sh <selecta-cli>
CLI=$1
W=$(mktemp -d "${TMPDIR:-/tmp}/t76.XXXXXX") || exit 1
trap 'rm -rf "$W"' EXIT
fail() { echo "T76 FAIL: $*"; exit 1; }
field() { printf '%s\n' "$OUT" | tr ' ' '\n' | sed -n "s/^$1=//p" | head -n 1; }

D="$W/files/modland/Protracker/a"
mkdir -p "$D" || exit 1
for n in 1 2 3 4 5 6; do
  head -c 1000 /dev/zero > "$D/s$n.mod"
  touch -t "20240101000$n" "$D/s$n.mod"
done
export SELECTA_CACHE_DIR="$W"

OUT=$("$CLI" library trim --budget 3500 --machine a500 \
  --mark kept:modland:Protracker/a/s1.mod --mark liked:modland:Protracker/a/s2.mod) || fail "trim exit"
[ "$(field trim_runs)" = 1 ] || fail "sentinel trim_runs: $OUT"
[ "$(field bytes_before)" = 6000 ] || fail "bytes_before: $OUT"
[ "$(field trimmed)" = 3 ] || fail "trimmed: $OUT"
[ "$(field kept_exempt)" = 2 ] || fail "kept_exempt: $OUT"
[ "$(field bytes_after)" -le 3500 ] || fail "bytes_after over budget: $OUT"
[ "$(field bytes_after)" = 3000 ] || fail "bytes_after: $OUT"
for n in 1 2 6; do [ -f "$D/s$n.mod" ] || fail "s$n evicted"; done
for n in 3 4 5; do [ ! -f "$D/s$n.mod" ] || fail "s$n survived"; done

# Over budget with only exempt files left: the unmarked file goes, the exempt ones stay above the budget.
OUT=$("$CLI" library trim --budget 100 --machine a500 \
  --mark kept:modland:Protracker/a/s1.mod --mark liked:modland:Protracker/a/s2.mod) || fail "trim 2 exit"
[ "$(field trimmed)" = 1 ] || fail "trimmed (second): $OUT"
{ [ -f "$D/s1.mod" ] && [ -f "$D/s2.mod" ]; } || fail "exempt lost"

# Default budget per tier (no --budget).
for t in "a500 8388608" "a1200 8388608" "a1200-fpu 8388608" "a060 536870912"; do
  set -- $t
  OUT=$("$CLI" library trim --machine "$1") || fail "tier $1 exit"
  [ "$(field budget)" = "$2" ] || fail "budget for $1: $OUT"
done
echo "cache_budget_ok"
