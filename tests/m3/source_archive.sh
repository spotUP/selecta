#!/bin/sh
# T71 source-local (archive part, I159): zip and lha archives scan as folders. Each archive holds one
# module and one text file; the module becomes a song at <archive>/<member>. `archives` and
# `archive_members` count the new code (archives opened, members recognised).
# usage: source_archive.sh <selecta-cli> <make_archives.py> <mod>
set -eu
cli=$1
maker=$2
mod=$3
root=$(mktemp -d "${TMPDIR:-/tmp}/sel-t71a.XXXXXX")
trap 'rm -rf "$root"' EXIT
python3 -I "$maker" "$mod" "$root"
printf 'PK\003\004garbage' > "$root/broken.zip"
out=$("$cli" library scan --db "$root/lib.db" "$root")
echo "$out"
field() { echo "$out" | tr ' ' '\n' | sed -n "s/^$1=//p"; }
[ "$(field archives)" = 5 ] || { echo "FAIL archives != 5 (zip, lzh, 2 lzx, broken zip)"; exit 1; }
[ "$(field archive_lzx)" = 2 ] || { echo "FAIL archive_lzx != 2 (stored and compressed)"; exit 1; }
[ "$(field depack_pp)" = 1 ] || { echo "FAIL depack_pp != 1"; exit 1; }
[ "$(field archive_members)" = 4 ] || { echo "FAIL archive_members != 4"; exit 1; }
[ "$(field scanned_files)" = 5 ] || { echo "FAIL scanned_files != 5"; exit 1; }
[ "$(field db_songs)" = 5 ] || { echo "FAIL db_songs != 5"; exit 1; }
[ "$(field probed)" -gt 0 ] || { echo "FAIL probed"; exit 1; }
hits=$("$cli" library search --db "$root/lib.db" "source:local")
echo "$hits"
echo "$hits" | grep -q "^key=local:$root/pack.zip/songs/inzip.mod\$" || { echo "FAIL missing zip member"; exit 1; }
echo "$hits" | grep -q "^key=local:$root/pack.lzh/songs/inlha.mod\$" || { echo "FAIL missing lha member"; exit 1; }
echo "$hits" | grep -q "^key=local:$root/pack.lzx/songs/instore.mod\$" || { echo "FAIL missing stored lzx member"; exit 1; }
echo "$hits" | grep -q "^key=local:$root/packc.lzx/songs/inlzx.mod\$" || { echo "FAIL missing compressed lzx member"; exit 1; }
echo "$hits" | grep -q "^key=local:$root/song.pp\$" || { echo "FAIL missing depacked PowerPacker file"; exit 1; }
# LIBRARY_BROWSE into archives: members list as a folder, nested folders are synthesised. The browser hides files
# with no recognised format (I218i), so readme.txt is not listed and only songs/ remains.
for a in pack.zip pack.lzh pack.lzx packc.lzx; do
    b=$("$cli" library browse "$root/$a")
    echo "$b" | grep -q "dir_entries=1" || { echo "FAIL browse $a: expected songs/ only (readme.txt hidden)"; echo "$b"; exit 1; }
    b=$("$cli" library browse "$root/$a/songs")
    echo "$b" | grep -q "dir_entries=1" || { echo "FAIL browse $a/songs: expected one module"; echo "$b"; exit 1; }
done
echo "source_archive_ok members=4"
