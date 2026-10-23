#!/bin/sh
# T71 source-local (scan part): selecta-cli library scan walks a tree, probes every file with the
# registered replayers (probed > 0 is the sentinel that the scan code ran) and writes one song per module.
# usage: source_local.sh <selecta-cli> <manifest> <mod> <mod>
set -eu
cli=$1
manifest=$2
modA=$3
modB=$4
root=$(mktemp -d "${TMPDIR:-/tmp}/sel-t71.XXXXXX")
trap 'rm -rf "$root"' EXIT
mkdir -p "$root/sub"
cp "$modA" "$root/one.mod"
cp "$modB" "$root/sub/two.mod"
printf 'not a module\n' > "$root/readme.txt"
expect=$(grep -c . "$manifest")
out=$("$cli" library scan --db "$root/lib.db" "$root")
echo "$out"
field() { echo "$out" | tr ' ' '\n' | sed -n "s/^$1=//p"; }
[ "$(field scanned_files)" = "$expect" ] || { echo "FAIL scanned_files != $expect"; exit 1; }
[ "$(field db_songs)" = "$expect" ] || { echo "FAIL db_songs != $expect"; exit 1; }
[ "$(field skipped_files)" -ge 1 ] || { echo "FAIL skipped_files"; exit 1; }
[ "$(field probed)" -gt 0 ] || { echo "FAIL probed (scan code did not run)"; exit 1; }
[ "$(field scan_batches)" -ge 1 ] || { echo "FAIL scan_batches"; exit 1; }
[ "$(field dirs)" = 2 ] || { echo "FAIL dirs != 2"; exit 1; }
# Rescan is idempotent: same song count.
out=$("$cli" library scan --db "$root/lib.db" "$root")
[ "$(field db_songs)" = "$expect" ] || { echo "FAIL rescan db_songs"; exit 1; }
# LIBRARY_SEARCH over the scanned rows (I158a): both scanned files are source:local hits, and the
# search goes through the same cfg.library handle as the scan.
hits=$("$cli" library search --db "$root/lib.db" "source:local")
echo "$hits"
[ "$(echo "$hits" | sed -n 's/^src_count=//p')" = "$expect" ] || { echo "FAIL search src_count != $expect"; exit 1; }
echo "$hits" | grep -q "^key=local:$root/one.mod\$" || { echo "FAIL search missing one.mod"; exit 1; }
echo "$hits" | grep -q "^key=local:$root/sub/two.mod\$" || { echo "FAIL search missing two.mod"; exit 1; }
none=$("$cli" library search --db "$root/lib.db" "source:modland")
[ "$(echo "$none" | sed -n 's/^src_count=//p')" = 0 ] || { echo "FAIL source:modland should have no hits"; exit 1; }
echo "source_local_ok scanned=$expect"
