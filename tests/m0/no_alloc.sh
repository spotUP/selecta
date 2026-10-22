#!/bin/sh
# no-alloc: play-null must not allocate between SEL_ALLOC_FREEZE and
# SEL_ALLOC_THAW (plan Phase 5, allocation rule). macOS host only.
# usage: no_alloc.sh <selecta-cli> <count_malloc.dylib> [<song> <wav-out>]
# With a song and output path the render command runs instead of play-null (T17).
# With "--script <file>" the script command runs; the script's @alloc_guard on
# brackets each engine render call with the markers (T26, queue auto-advance).
# The shim reports after every THAW; the last report is the cumulative count.
set -eu
cli=$1
shim=$2
if [ "${3:-}" = "--script" ]; then
    err=$(DYLD_INSERT_LIBRARIES="$shim" "$cli" script "$4" 2>&1 >/dev/null)
elif [ $# -ge 4 ]; then
    mkdir -p "$(dirname "$4")"
    err=$(DYLD_INSERT_LIBRARIES="$shim" "$cli" render "$3" --output "$4" --seconds 2 --rate 44100 --resample linear 2>&1 >/dev/null)
else
    err=$(DYLD_INSERT_LIBRARIES="$shim" "$cli" play-null --rate 44100 --frames 4410 2>&1 >/dev/null)
fi
before=$(printf '%s\n' "$err" | sed -n 's/^SEL_ALLOC_BEFORE=//p' | tail -n 1)
count=$(printf '%s\n' "$err" | sed -n 's/^SEL_ALLOC_COUNT=//p' | tail -n 1)
if [ -z "$count" ] || [ -z "$before" ]; then
    echo "[ERROR] shim did not report; markers missing or shim not loaded"
    printf '%s\n' "$err"
    exit 1
fi
if [ "$before" -eq 0 ]; then
    echo "[ERROR] shim counted no allocation before the freeze; it is not interposing"
    exit 1
fi
if [ "$count" -ne 0 ]; then
    echo "[ERROR] $count allocations on the render path"
    exit 1
fi
echo "[OK] before=$before window=0"
