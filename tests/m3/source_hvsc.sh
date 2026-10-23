#!/bin/sh
# T73 source-hvsc (I162): `selecta-cli library sync hvsc` against a listing, a Songlengths.md5 excerpt and a
# STIL excerpt, offline. Expected counts come from the fixture files here, not typed in; the named song's
# lengths and url are the checked values.
# usage: source_hvsc.sh <selecta-cli> <tests/m3 dir>
set -eu
cli=$1
fx=$2/hvsc
root=$(mktemp -d "${TMPDIR:-/tmp}/sel-t73.XXXXXX")
trap 'rm -rf "$root"' EXIT
field() { echo "$out" | tr ' ' '\n' | sed -n "s/^$1=//p"; }
fail() { echo "FAIL $1"; exit 1; }

cp "$fx"/hvsc_listing.txt "$fx"/Songlengths.md5 "$fx"/STIL.txt "$root/"
listed=$(grep -c '^/\(MUSICIANS\|GAMES\|DEMOS\)/' "$fx/hvsc_listing.txt")
skipped=$(grep -vc '^/\(MUSICIANS\|GAMES\|DEMOS\)/' "$fx/hvsc_listing.txt" || true)
lengths=$(grep -c '^[0-9a-f]\{32\}=' "$fx/Songlengths.md5")
subtunes=$(grep '^[0-9a-f]\{32\}=' "$fx/Songlengths.md5" | sed 's/^[^=]*=//' | wc -w | tr -d ' ')
stil=$(grep -c '^/' "$fx/STIL.txt")
[ "$listed" -gt 0 ] && [ "$lengths" -gt 0 ] && [ "$stil" -gt 0 ] || fail "fixture is empty"

out=$("$cli" library sync --db "$root/h.db" --now 1792756800 --dump hvsc "$root")
echo "$out" | grep -v '^hvsc_' || true
[ "$(field listing_rows)" = "$listed" ] || fail "listing_rows != $listed"
[ "$(field skipped_rows)" = "$skipped" ] || fail "skipped_rows != $skipped"
[ "$(field songlengths)" = "$lengths" ] || fail "songlengths != $lengths"
[ "$(field subtunes)" = "$subtunes" ] || fail "subtunes != $subtunes"
[ "$(field stil_entries)" = "$stil" ] || fail "stil_entries != $stil"
[ "$(field db_songs)" = "$listed" ] || fail "db_songs != $listed"

rows=$(echo "$out" | sed -n 's/^hvsc_row=//p')
[ "$(echo "$rows" | grep -c .)" = "$listed" ] || fail "dump rows != $listed"

# subtune counts and lengths equal the fixture to the ms (Monty: 3:45 0:30.500 1:02.125(G))
monty=$(echo "$rows" | grep '^MUSICIANS/H/Hubbard_Rob/Monty_on_the_Run.sid|')
[ "$(echo "$monty" | cut -d'|' -f4)" = 225000 ] || fail "Monty duration_ms"
[ "$(echo "$monty" | cut -d'|' -f5)" = 3 ] || fail "Monty subtunes"
[ "$(echo "$monty" | cut -d'|' -f6)" = "225000,30500,62125" ] || fail "Monty lengths_ms"
cyb=$(echo "$rows" | grep '^MUSICIANS/T/Tel_Jeroen/Cybernoid.sid|')
[ "$(echo "$cyb" | cut -d'|' -f6)" = "45000,7000,60500,754007" ] || fail "Cybernoid lengths_ms"
cher=$(echo "$rows" | grep '^DEMOS/0-9/1000_Cherries.sid|')
[ "$(echo "$cher" | cut -d'|' -f6)" = 300500 ] || fail "Cherries lengths_ms"
[ "$(echo "$cher" | cut -d'|' -f2)" = "HVSC - Demos" ] || fail "Cherries format"
# md5 attached from Songlengths
[ "$(echo "$monty" | cut -d'|' -f3)" = 1b2c3d4e5f60718293a4b5c6d7e8f9a0 ] || fail "Monty md5"
# the listing-only song has no lengths
lo=$(echo "$rows" | grep '^MUSICIANS/D/DRAX/Listing_Only.sid|')
[ "$(echo "$lo" | cut -d'|' -f4)" = 0 ] || fail "listing-only duration"
[ -z "$(echo "$lo" | cut -d'|' -f5)" ] || fail "listing-only has subtunes"
# hosted by Modland: the url resolves through the Modland host path layout
[ "$(echo "$monty" | cut -d'|' -f7)" = "https://modland.com/pub/modules/HVSC/MUSICIANS/H/Hubbard_Rob/Monty_on_the_Run.sid" ] || fail "Monty url"
echo "$cher" | cut -d'|' -f7 | grep -qF 'https://modland.com/pub/modules/HVSC/DEMOS/0-9/1000_Cherries.sid' || fail "Cherries url"

# STIL notes attach to their song keys, per file and per subtune
st=$(echo "$out" | sed -n 's/^hvsc_sticker=//p')
echo "$st" | grep -qxF 'MUSICIANS/H/Hubbard_Rob/Monty_on_the_Run.sid|stil.1.title|Monty on the Run (main)' || fail "STIL title subtune 1"
echo "$st" | grep -qxF 'MUSICIANS/H/Hubbard_Rob/Monty_on_the_Run.sid|stil.1.artist|Rob Hubbard' || fail "STIL artist subtune 1"
echo "$st" | grep -qxF 'MUSICIANS/H/Hubbard_Rob/Monty_on_the_Run.sid|stil.2.comment|Short jingle version.' || fail "STIL comment subtune 2"
echo "$st" | grep -qxF 'MUSICIANS/H/Hubbard_Rob/Monty_on_the_Run.sid|stil.comment|Whole-file note for Monty.' || fail "STIL file comment"
echo "$st" | grep -qxF "MUSICIANS/H/Hubbard_Rob/Commando.sid|stil.comment|Rob's title tune for the 1985 Elite conversion of the Capcom arcade game." || fail "STIL continuation"
echo "$st" | grep -qxF 'MUSICIANS/T/Tel_Jeroen/Cybernoid.sid|stil.2.name|Cybernoid second tune' || fail "STIL name"
if echo "$st" | grep -qF 'Unlisted.sid'; then fail "STIL note attached to a song without a row"; fi

echo "source_hvsc_ok rows=$listed subtunes=$subtunes songlengths=$lengths stil_entries=$stil"
