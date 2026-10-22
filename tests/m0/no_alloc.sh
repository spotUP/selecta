#!/bin/sh
# no-alloc: play-null must not allocate between SEL_ALLOC_FREEZE and
# SEL_ALLOC_THAW (plan Phase 5, allocation rule). macOS host only.
# usage: no_alloc.sh <selecta-cli> <count_malloc.dylib>
set -eu
cli=$1
shim=$2
err=$(DYLD_INSERT_LIBRARIES="$shim" "$cli" play-null --rate 44100 --frames 4410 2>&1 >/dev/null)
before=$(printf '%s\n' "$err" | sed -n 's/^SEL_ALLOC_BEFORE=//p')
count=$(printf '%s\n' "$err" | sed -n 's/^SEL_ALLOC_COUNT=//p')
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
