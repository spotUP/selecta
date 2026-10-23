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
[ "$(field archives)" = 3 ] || { echo "FAIL archives != 3 (zip, lzh, broken zip)"; exit 1; }
[ "$(field archive_members)" = 2 ] || { echo "FAIL archive_members != 2"; exit 1; }
[ "$(field scanned_files)" = 2 ] || { echo "FAIL scanned_files != 2"; exit 1; }
[ "$(field db_songs)" = 2 ] || { echo "FAIL db_songs != 2"; exit 1; }
[ "$(field probed)" -gt 0 ] || { echo "FAIL probed"; exit 1; }
hits=$("$cli" library search --db "$root/lib.db" "source:local")
echo "$hits"
echo "$hits" | grep -q "^key=local:$root/pack.zip/songs/inzip.mod\$" || { echo "FAIL missing zip member"; exit 1; }
echo "$hits" | grep -q "^key=local:$root/pack.lzh/songs/inlha.mod\$" || { echo "FAIL missing lha member"; exit 1; }
echo "source_archive_ok members=2"
