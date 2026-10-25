#!/bin/sh
# I276: `selecta-cli library scan <dir>` without --db fills the app's library.db (the config directory), not a throwaway
# :memory: database; `--db :memory:` still leaves no file behind. XDG_CONFIG_HOME points at a scratch directory, so the
# user's own library.db is never touched.
# usage: scan_default_db.sh <selecta-cli> <mod>
set -eu
cli=$1
mod=$2
root=$(mktemp -d "${TMPDIR:-/tmp}/sel-i276.XXXXXX")
trap 'rm -rf "$root"' EXIT
mkdir -p "$root/songs" "$root/cfg"
cp "$mod" "$root/songs/one.mod"
field() { echo "$out" | tr ' ' '\n' | sed -n "s/^$1=//p"; }
export XDG_CONFIG_HOME="$root/cfg"
out=$("$cli" library scan --db :memory: "$root/songs")
[ "$(field db_songs)" = 1 ] || { echo "FAIL memory db_songs"; exit 1; }
[ ! -e "$root/cfg/selecta/library.db" ] || { echo "FAIL --db :memory: wrote the default db"; exit 1; }
out=$("$cli" library scan "$root/songs")
[ "$(field db_songs)" = 1 ] || { echo "FAIL default db_songs"; exit 1; }
[ -s "$root/cfg/selecta/library.db" ] || { echo "FAIL default library.db missing"; exit 1; }
# A second scan finds the first scan's row in the same file: one song after the rescan, from the app's database.
out=$("$cli" library scan "$root/songs")
[ "$(field db_songs)" = 1 ] || { echo "FAIL rescan db_songs"; exit 1; }
echo "scan_default_db_ok"
