#!/bin/sh
# T62 library-db-open (plan 2026-10-25-m3-library.md, Phase 1, I144-I146).
# Drives `selecta-cli library stats` and checks the sentinel
# `db_open=1 schema_version= sqlite_version=`, schema_version=1, fts5=1,
# heap_bytes equal to the sel_machine scaling table, one heap allocation, and
# (macOS, with the count_malloc shim) zero malloc calls between
# SEL_ALLOC_FREEZE and SEL_ALLOC_THAW while the worker runs queries.
# usage: library_db_open.sh <selecta-cli> [<count_malloc.dylib>]
set -u
cli=$1
shim=${2:-}
fail=0

field() { printf '%s\n' "$1" | tr ' ' '\n' | sed -n "s/^$2=//p" | head -n 1; }

check() { # machine expected_heap
    if [ "$1" = "desktop" ]; then
        out=$("$cli" library stats 2>/dev/null)
    else
        out=$("$cli" library stats --machine "$1" 2>/dev/null)
    fi
    rc=$?
    line=$(printf '%s\n' "$out" | grep '^db_open=' | head -n 1)
    if [ $rc -ne 0 ] || [ -z "$line" ]; then
        echo "[ERROR] $1: no db_open= line (rc=$rc)"; fail=1; return
    fi
    [ "$(field "$line" db_open)" = 1 ] || { echo "[ERROR] $1: db_open != 1"; fail=1; }
    [ "$(field "$line" schema_version)" = 1 ] || { echo "[ERROR] $1: schema_version != 1"; fail=1; }
    [ -n "$(field "$line" sqlite_version)" ] || { echo "[ERROR] $1: no sqlite_version"; fail=1; }
    [ "$(field "$line" fts5)" = 1 ] || { echo "[ERROR] $1: fts5 != 1"; fail=1; }
    [ "$(field "$line" heap_allocs)" = 1 ] || { echo "[ERROR] $1: heap_allocs != 1"; fail=1; }
    [ "$(field "$line" fts_hits)" = 1 ] || { echo "[ERROR] $1: fts_hits != 1"; fail=1; }
    [ "$(field "$line" worker_requests)" -ge 2 ] 2>/dev/null || { echo "[ERROR] $1: worker did not run"; fail=1; }
    [ "$(field "$line" heap_bytes)" = "$2" ] || { echo "[ERROR] $1: heap_bytes $(field "$line" heap_bytes) != $2"; fail=1; }
}

# Scaling table (src/library/db/db.c): fast + chip RAM <= 4 MiB 512 KiB,
# <= 16 MiB 2 MiB, <= 128 MiB 8 MiB, above 32 MiB. a500 = 2 MiB chip.
check a500 524288
check a1200 524288
check a1200-fpu 2097152
check a060 8388608
mach=$("$cli" machine 2>/dev/null)
fast=$(printf '%s\n' "$mach" | sed -n 's/^[A-Z_]*FAST_RAM=//p' | head -n 1)
if [ -n "$fast" ] && [ "$fast" -gt 134217728 ] 2>/dev/null; then
    check desktop 33554432
else
    echo "[ERROR] desktop FAST_RAM not above 128 MiB: '$fast'"; fail=1
fi

if [ -n "$shim" ]; then
    err=$(DYLD_INSERT_LIBRARIES="$shim" "$cli" library stats 2>&1 >/dev/null)
    before=$(printf '%s\n' "$err" | sed -n 's/^SEL_ALLOC_BEFORE=//p' | tail -n 1)
    count=$(printf '%s\n' "$err" | sed -n 's/^SEL_ALLOC_COUNT=//p' | tail -n 1)
    if [ -z "$count" ] || [ -z "$before" ] || [ "$before" -eq 0 ]; then
        echo "[ERROR] alloc shim did not report"; fail=1
    elif [ "$count" -ne 0 ]; then
        echo "[ERROR] $count allocations after library start"; fail=1
    fi
fi
[ $fail -eq 0 ] && echo "[OK] library-db-open"
exit $fail
