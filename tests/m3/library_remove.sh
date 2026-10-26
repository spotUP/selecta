#!/bin/sh
# DUI-P4-18 (I313) engine driver: LIBRARY_REMOVE drops one song from the catalogue, LIBRARY_CLEAR drops them all, empties the
# queue and drops the playlists, ratings and notes and keeps settings; the audio files stay on disk. The UI sends these same commands; this test drives the engine.
# usage: library_remove.sh <selecta-cli> <catalogue.tsv> <song.mod>
set -u
cli=$1
cat=$2
rm -rf lrm lrm.db
mkdir lrm
for n in a b c; do cp "$3" "lrm/$n.mod"; done
songs() { echo songs=$(sqlite3 lrm.db "SELECT COUNT(*) FROM songs WHERE path LIKE '%lrm/%'"); }
"$cli" library scan --db lrm.db "$(pwd)/lrm" >/dev/null 2>&1 || { echo "scan failed"; exit 1; }
[ "$(songs)" = songs=3 ] || { echo "scan gave $(songs), want songs=3"; exit 1; }
dir=$(cd lrm && pwd)
printf 'LIBRARY_REMOVE %s/a.mod\n' "$dir" > lrm_remove.sel
out=$("$cli" script lrm_remove.sel --catalogue "$cat" --db lrm.db 2>&1) || { echo "$out"; exit 1; }
[ "$(songs)" = songs=2 ] || { echo "after LIBRARY_REMOVE got $(songs), want songs=2"; exit 1; }
[ -f lrm/a.mod ] || { echo "LIBRARY_REMOVE deleted the audio file"; exit 1; }
sqlite3 lrm.db "INSERT INTO ratings(key, rating) VALUES('x:1', 5); INSERT INTO notes(key, note) VALUES('x:1', 'n'); INSERT INTO meta(key, value) VALUES('keepme', '1')" || { echo "seed failed"; exit 1; }
printf 'QUEUE_ADD %s/b.mod\nPLAYLIST_CREATE Keep\nLIBRARY_CLEAR\n@queue\n' "$dir" > lrm_clear.sel
out=$("$cli" script lrm_clear.sel --catalogue "$cat" --db lrm.db 2>&1) || { echo "$out"; exit 1; }
[ "$(songs)" = songs=0 ] || { echo "after LIBRARY_CLEAR got $(songs), want songs=0"; exit 1; }
ls lrm/b.mod lrm/c.mod >/dev/null 2>&1 || { echo "LIBRARY_CLEAR deleted audio files"; exit 1; }
pl=$(sqlite3 lrm.db 'SELECT COUNT(*) FROM playlists' 2>&1)
[ "$pl" = 0 ] || { echo "playlists left after LIBRARY_CLEAR: $pl"; exit 1; }
for t in ratings notes; do
  [ "$(sqlite3 lrm.db "SELECT COUNT(*) FROM $t")" = 0 ] || { echo "$t left after LIBRARY_CLEAR"; exit 1; }
done
[ "$(sqlite3 lrm.db "SELECT value FROM meta WHERE key='keepme'")" = 1 ] || { echo "setting lost after LIBRARY_CLEAR"; exit 1; }
printf '%s\n' "$out" | grep -q 'b.mod' && { echo "queue still holds a song after LIBRARY_CLEAR:"; printf '%s\n' "$out"; exit 1; }
rm -rf lrm lrm.db lrm_remove.sel lrm_clear.sel
echo lrm_ok
