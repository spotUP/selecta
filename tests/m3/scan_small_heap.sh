#!/bin/sh
# I577: `library scan` fits the 544 KB SQLite heap of the 2 MB tier (--machine a1200), into a :memory:
# database (the whole DB lives in the heap) and into a database file. memsys5 rounded every page cache
# entry (4096 + header bytes) up to 8 KB, so the empty schema alone took 328 KB and the scan failed with
# out_of_memory; the page cache now has its own exact-size slots.
# usage: scan_small_heap.sh <selecta-cli> <testmod>
cli="$1"
dir=$(dirname "$2")
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
# I687c: the songs(album) index (SCHEMA_V11) is one more page; at 512 KiB a scan root of a scan root of over
# ~35 characters already ran out of memsys5 blocks (SQLITE_NOMEM); the 544 KiB tier carries 85 (deep root below).
deep="$work/pppppppppp"
mkdir -p "$deep"
cp -R "$dir"/. "$deep"/
for kind in memory file; do
 for root in "$dir" "$deep"; do
  db=":memory:"
  [ "$kind" = file ] && db="$work/lib-${#root}.db"
  out=$("$cli" library scan --db "$db" --machine a1200 "$root" 2>&1)
  rc=$?
  line=$(printf '%s\n' "$out" | grep 'scanned_files=')
  scanned=$(printf '%s\n' "$line" | sed -n 's/.* scanned_files=\([0-9]*\).*/\1/p')
  songs=$(printf '%s\n' "$line" | sed -n 's/.* db_songs=\([-0-9]*\).*/\1/p')
  heap=$(printf '%s\n' "$line" | sed -n 's/.* heap_bytes=\([0-9]*\).*/\1/p')
  peak=$(printf '%s\n' "$line" | sed -n 's/.* db_heap_peak=\([0-9]*\).*/\1/p')
  if [ "$rc" -ne 0 ] || [ -z "$scanned" ] || [ "$scanned" -lt 10 ] || [ "$songs" != "$scanned" ] ||
     [ "$heap" != 557056 ] || [ -z "$peak" ] || [ "$peak" -gt "$heap" ]; then
    echo "RED[I577]: scan into $db under the 544 KB heap failed (rc=$rc)"
    printf '%s\n' "$out"
    exit 1
  fi
  echo "root=${#root} db=$db scanned_files=$scanned db_songs=$songs heap_bytes=$heap db_heap_peak=$peak"
 done
done
echo "scan_small_heap_ok"
