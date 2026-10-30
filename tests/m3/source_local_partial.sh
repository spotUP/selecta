#!/bin/sh
# I688: a scan never imports an unfinished download. song.mod is a real module; the same bytes under .part,
# .crdownload, .download and .partial names (upper and lower case, and as "song.mp3.part") must not become songs.
# usage: source_local_partial.sh <selecta-cli> <mod>
set -eu
cli=$1
mod=$2
root=$(mktemp -d "${TMPDIR:-/tmp}/sel-i688.XXXXXX")
trap 'rm -rf "$root"' EXIT
cp "$mod" "$root/song.mod"
for n in song.mp3.part song2.mod.PART song3.mod.crdownload song4.mod.download song5.mod.Partial; do
  cp "$mod" "$root/$n"
done
out=$("$cli" library scan --db "$root/lib.db" "$root")
echo "$out"
field() { echo "$out" | tr ' ' '\n' | sed -n "s/^$1=//p"; }
[ "$(field scanned_files)" = 1 ] || { echo "FAIL scanned_files != 1"; exit 1; }
[ "$(field db_songs)" = 1 ] || { echo "FAIL db_songs != 1"; exit 1; }
[ "$(field probed)" -gt 0 ] || { echo "FAIL probed (scan code did not run)"; exit 1; }
echo "source_local_partial_ok"
