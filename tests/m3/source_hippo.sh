#!/bin/sh
# T74 source-hippo (I163): `selecta-cli library sync hippo` against the JSON list, the legacy TSV list and a
# fetched .prg, offline. Expected counts come from the fixture files here, not typed in. The fixtures in
# tests/m3/hippo are synthetic (derived from the recorded index, with non-ASCII text and a .prg carrying a
# divider, a stealth entry and a #song= marker); their provenance is stated in uhcplaylists.tsv.
# usage: source_hippo.sh <selecta-cli> <tests/m3 dir>
set -eu
cli=$1
fx=$2/hippo
root=$(mktemp -d "${TMPDIR:-/tmp}/sel-t74.XXXXXX")
trap 'rm -rf "$root"' EXIT
field() { echo "$1" | tr ' ' '\n' | sed -n "s/^$2=//p"; }
fail() { echo "FAIL $1"; exit 1; }

mkdir "$root/json" "$root/tsv"
cp "$fx"/hippo_list.json "$root/json/"
cp "$fx"/synthetic_test.prg "$root/json/LMan_SID_Chip_Club.prg"
cp "$fx"/uhcplaylists.tsv "$root/tsv/"
listed=$(grep -o '"filename"' "$fx/hippo_list.json" | wc -l | tr -d ' ')
tsvrows=$(grep -v '^#' "$fx/uhcplaylists.tsv" | grep -vc '^Filename')
[ "$listed" -gt 0 ] && [ "$listed" = "$tsvrows" ] || fail "fixtures disagree: json=$listed tsv=$tsvrows"
prg="$fx/synthetic_test.prg"
p_entries=$(sed 1,2d "$prg" | LC_ALL=C grep -c .)
p_div=$(LC_ALL=C grep -c "^$(printf '\367')" "$prg")
p_stealth=$(LC_ALL=C grep -c "^$(printf '\242')" "$prg")
p_sub=$(grep -c '#song=' "$prg")
[ "$p_entries" -gt 0 ] && [ "$p_div" -gt 0 ] && [ "$p_stealth" -gt 0 ] && [ "$p_sub" -gt 0 ] || fail "prg fixture is empty"

jout=$("$cli" library sync --db "$root/j.db" --now 1792756800 --dump hippo "$root/json")
tout=$("$cli" library sync --db "$root/t.db" --now 1792756800 --dump hippo "$root/tsv")
echo "$jout" | grep -v '^hippo_row=' || true
echo "$tout" | grep -v '^hippo_row=' || true
[ "$(field "$jout" hippo_lists)" = "$listed" ] || fail "json hippo_lists != $listed"
[ "$(field "$tout" hippo_lists)" = "$listed" ] || fail "tsv hippo_lists != $listed"
[ "$(field "$jout" db_songs)" = "$listed" ] || fail "json db_songs != $listed"
[ "$(field "$tout" db_songs)" = "$listed" ] || fail "tsv db_songs != $listed"

# the .prg keeps dividers, stealth entries and #song= through the Hippo codec
[ "$(field "$jout" prg_files)" = 1 ] || fail "prg_files != 1"
[ "$(field "$jout" prg_entries)" = "$p_entries" ] || fail "prg_entries != $p_entries"
[ "$(field "$jout" prg_dividers)" = "$p_div" ] || fail "prg_dividers != $p_div"
[ "$(field "$jout" prg_stealth)" = "$p_stealth" ] || fail "prg_stealth != $p_stealth"
[ "$(field "$jout" prg_subsongs)" = "$p_sub" ] || fail "prg_subsongs != $p_sub"

# both lists give the same rows (the TSV has no id column; the dump puts id after the url)
jrows=$(echo "$jout" | sed -n 's/^hippo_row=//p' | cut -d'|' -f1-6)
trows=$(echo "$tout" | sed -n 's/^hippo_row=//p' | cut -d'|' -f1-6)
[ "$(echo "$jrows" | grep -c .)" = "$listed" ] || fail "json dump rows != $listed"
[ "$jrows" = "$trows" ] || fail "json rows differ from tsv rows"

# named rows: UTF-8 text from a \u escape (json) and a Latin-1 byte (tsv), url, format, genre
club='LMan_SID_Chip_Club.prg|Hippo - Playlists|Caf'"$(printf '\303\251')"' SID Chip Club|Spot|Acid|https://hippoplayer.se/playlist/LMan_SID_Chip_Club.prg'
echo "$jrows" | grep -qxF "$club" || fail "club row (json)"
echo "$trows" | grep -qxF "$club" || fail "club row (tsv)"
echo "$jout" | grep -q '^hippo_row=LMan_SID_Chip_Club.prg|.*|181|' || fail "hippo_id sticker"

# the fetched .prg is annotated on its own playlist row (stickers after the id: entries|dividers|stealth|subsongs)
echo "$jout" | grep -qF "|181|$p_entries|$p_div|$p_stealth|$p_sub" || fail "prg stickers on the playlist row"

echo "source_hippo_ok lists=$listed prg_entries=$p_entries dividers=$p_div stealth=$p_stealth subsongs=$p_sub"
