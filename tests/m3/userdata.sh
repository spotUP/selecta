#!/bin/sh
# T78 user-data driver (history part I170, flags part I171).
# History: runs userdata.sel through `selecta-cli script` and diffs the LIBRARY (sentinel), RESULTS and ERROR
# lines with userdata.expected. Flags: userflags.sel (FLAG wrong/skip, wrong:yes/skip:yes, SHUFFLE never picks the
# skip-flagged song, skip_excluded sentinel) diffed with userflags.expected; then `selecta-cli wrong` writes two
# flags into a database file, exports sounds-wrong.txt and diffs it with the golden file. Prints userdata_ok.
# usage: userdata.sh <selecta-cli> <catalogue.tsv> <userdata.sel> <expected> <userflags.sel> <userflags.expected> <golden>
set -u
cli=$1
out=$("$cli" script "$3" --catalogue "$2" 2>&1 | grep -E '^(LIBRARY |RESULTS |ERROR)')
if [ "$out" != "$(cat "$4")" ]; then printf '%s\n' "$out" | diff - "$4"; exit 1; fi
out=$("$cli" script "$5" --catalogue "$2" 2>&1 | grep -E '^(LIBRARY |RESULTS |ERROR|QUEUE )')
if [ "$out" != "$(cat "$6")" ]; then printf '%s\n' "$out" | diff - "$6"; exit 1; fi
rm -f userdata_wrong.db sounds-wrong.txt
"$cli" wrong --db userdata_wrong.db selecta-test-mk.mod "stutters" > /dev/null || exit 1
res=$("$cli" wrong --db userdata_wrong.db --export sounds-wrong.txt selecta-short.mod "hiss on ch 2") || exit 1
[ "$res" = "flags=2 wrong_exported=2" ] || { echo "wrong: $res"; exit 1; }
diff sounds-wrong.txt "$7" || exit 1
rm -f userdata_wrong.db sounds-wrong.txt
# NOTE and tag edits (I172): played + wrong flag + NOTE + artist edit into a database file.
rm -f userdata_notes.db
res=$("$cli" userdata --db userdata_notes.db --catalogue "$2" --played selecta-test.mod --wrong selecta-test-mk.mod "stutters" \
    --note selecta-test.mod "great bassline" --tag selecta-test.mod artist zedtag) || exit 1
[ "$res" = "history_rows=1 flags=1 notes=1 pending_push=1" ] || { echo "userdata: $res"; exit 1; }
# Restart: a new process finds the song by the edited artist, not by the scanned one.
res=$("$cli" library search --db userdata_notes.db "artist:zedtag") || exit 1
printf '%s\n' "$res" | grep -q '^key=selecta-test.mod' || { echo "tag search: $res"; exit 1; }
res=$("$cli" library search --db userdata_notes.db "artist:lizardking") || exit 1
if printf '%s\n' "$res" | grep -q '^key=selecta-test.mod$'; then echo "old tag still found: $res"; exit 1; fi
# A re-import of the catalogue (rescan) keeps the edit; the note survives; nothing was sent (queue untouched).
res=$("$cli" userdata --db userdata_notes.db --catalogue "$2") || exit 1
[ "$res" = "history_rows=1 flags=1 notes=1 pending_push=1" ] || { echo "restart: $res"; exit 1; }
res=$("$cli" library search --db userdata_notes.db "artist:zedtag") || exit 1
printf '%s\n' "$res" | grep -q '^key=selecta-test.mod' || { echo "tag after reimport: $res"; exit 1; }
rm -f userdata_notes.db
echo userdata_ok
