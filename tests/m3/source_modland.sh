#!/bin/sh
# T72 source-modland (I161): `selecta-cli library sync modland` against a 100-row allmods_md5 fixture and a
# changelog fixture, offline. Every expected number is counted from the fixture files here, not typed in.
# usage: source_modland.sh <selecta-cli> <tests/m3 dir>
set -eu
cli=$1
fx=$2
now=1792756800 # 2026-10-23 12:00:00 UTC; "this week" = changelog dates 2026-10-17 .. 2026-10-23
week_from=2026-10-17
week_to=2026-10-23
root=$(mktemp -d "${TMPDIR:-/tmp}/sel-t72.XXXXXX")
trap 'rm -rf "$root"' EXIT
field() { echo "$out" | tr ' ' '\n' | sed -n "s/^$1=//p"; }

listing=$fx/modland_listing/allmods_md5.txt
rows_expect=$(grep -c . "$listing")
[ "$rows_expect" = 100 ] || { echo "FAIL fixture is not 100 rows"; exit 1; }

# Part 1: the listing alone.
mkdir "$root/a"
cp "$listing" "$root/a/"
out=$("$cli" library sync --db "$root/a.db" --now "$now" --dump modland "$root/a")
echo "$out" | grep -v '^row=' || true
[ "$(field listing_rows)" = "$rows_expect" ] || { echo "FAIL listing_rows != $rows_expect"; exit 1; }
[ "$(field new_this_week)" = 0 ] || { echo "FAIL new_this_week without changelogs"; exit 1; }
[ "$(field db_songs)" = "$rows_expect" ] || { echo "FAIL db_songs != $rows_expect"; exit 1; }
echo "$out" | sed -n 's/^row=//p' | LC_ALL=C sort > "$root/rows.txt"
[ "$(grep -c . "$root/rows.txt")" = "$rows_expect" ] || { echo "FAIL files rows != $rows_expect"; exit 1; }
# format and artist per row equal the golden
cut -d'|' -f1-3 "$root/rows.txt" > "$root/fa.txt"
LC_ALL=C sort "$fx/modland_parse.golden" > "$root/golden.txt"
diff "$root/golden.txt" "$root/fa.txt" > "$root/diff.txt" || { echo "FAIL format/artist differ from golden"; head -n 10 "$root/diff.txt"; exit 1; }
# md5 stored: every row carries the listing's md5 for its path
awk -F'\t' '{ print "pub/modules/" $2 "|" $1 }' "$listing" | LC_ALL=C sort > "$root/md5_expect.txt"
awk -F'|' '{ print $1 "|" $4 }' "$root/rows.txt" > "$root/md5_got.txt"
diff "$root/md5_expect.txt" "$root/md5_got.txt" > "$root/diff.txt" || { echo "FAIL md5 not stored"; head -n 10 "$root/diff.txt"; exit 1; }
# url(path_in(u)) == u and the song key form, every row
n=0
while IFS='|' read -r path _; do
    k=$("$cli" library key modland "$path") || { echo "FAIL key round trip: $path"; exit 1; }
    echo "$k" | grep -qxF "key=modland:$path" || { echo "FAIL song key: $path"; exit 1; }
    echo "$k" | grep -qx 'roundtrip=1' || { echo "FAIL roundtrip: $path"; exit 1; }
    n=$((n + 1))
done < "$root/golden.txt"
[ "$n" = "$rows_expect" ] || { echo "FAIL key rows $n"; exit 1; }

# Part 2: listing plus changelogs.
mkdir "$root/b"
cp "$listing" "$root/b/"
cp "$fx"/modland_changelogs/changelog_*.txt "$root/b/"
added=$(cat "$root"/b/changelog_*.txt | grep -c '^added:' || true)
deleted=$(cat "$root"/b/changelog_*.txt | grep -c '^deleted:' || true)
moved=$(cat "$root"/b/changelog_*.txt | grep -c '^moved:' || true)
updated=$(cat "$root"/b/changelog_*.txt | grep -c '^updated:' || true)
week=0
for f in "$root"/b/changelog_*.txt; do
    d=$(basename "$f" .txt)
    d=${d#changelog_}
    if [ "$d" \> "$week_from" ] || [ "$d" = "$week_from" ]; then
        if [ "$d" \< "$week_to" ] || [ "$d" = "$week_to" ]; then
            week=$((week + $(grep -c '^added:' "$f" || true)))
        fi
    fi
done
out=$("$cli" library sync --db "$root/b.db" --now "$now" --dump modland "$root/b")
echo "$out" | grep -v '^row=' || true
[ "$week" -gt 0 ] || { echo "FAIL fixture has no added entry inside the week"; exit 1; }
[ "$(field listing_rows)" = "$rows_expect" ] || { echo "FAIL listing_rows with changelogs"; exit 1; }
[ "$(field new_this_week)" = "$week" ] || { echo "FAIL new_this_week != $week"; exit 1; }
[ "$(field cl_added)" = "$added" ] || { echo "FAIL cl_added != $added"; exit 1; }
[ "$(field cl_moved)" = "$moved" ] || { echo "FAIL cl_moved != $moved"; exit 1; }
[ "$(field cl_updated)" = "$updated" ] || { echo "FAIL cl_updated != $updated"; exit 1; }
[ "$(field changelogs)" = "$(ls "$root"/b/changelog_*.txt | wc -l | tr -d ' ')" ] || { echo "FAIL changelogs count"; exit 1; }
want=$((rows_expect + added - deleted))
[ "$(field db_songs)" = "$want" ] || { echo "FAIL db_songs != $want"; exit 1; }
echo "$out" | grep -qF 'row=pub/modules/Fasttracker 2/Lioz/final pray.xm|' || { echo "FAIL moved row missing"; exit 1; }
if echo "$out" | grep -qF 'row=pub/modules/Fasttracker 2/Hans Der Reuter/final pray.xm|'; then
    echo "FAIL moved row still at old path"
    exit 1
fi
if echo "$out" | grep -qF 'row=pub/modules/Ace Tracker/Nemo/easy.am|'; then
    echo "FAIL deleted row still present"
    exit 1
fi
echo "source_modland_ok listing_rows=$rows_expect new_this_week=$week db_songs=$want"
