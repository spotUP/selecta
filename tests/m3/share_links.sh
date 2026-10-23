#!/bin/sh
# T84 share-links (I180, D-M3-11): SHARE verb, link parse in selecta-cli open and in the real selecta binary.
# usage: share_links.sh <selecta-cli> <selecta> <catalogue.tsv> <keys file>   (cwd holds selecta-test.mod)
set -u
cli=$1
sel=$2
cat=$3
keys=$4
fail() { echo "FAIL: $1"; exit 1; }

# (a) codec: selecta:// and DEViLBOX forms, sub=2 and t=5000 survive a round trip.
out=$("$cli" share make song modland p/purple.xm --sub 2 --t 5000 2>&1) || fail "share make: $out"
printf '%s\n' "$out" | grep -q '^LINK selecta://song/modland/p/purple.xm?sub=2&t=5000$' || fail "selecta link: $out"
printf '%s\n' "$out" | grep -q '^LINK_DEVILBOX https://devilbox.uprough.net/?song=modland%2Fp%2Fpurple.xm&sub=2&t=5000$' || fail "devilbox link: $out"
printf '%s\n' "$out" | grep -q '^ROUNDTRIP selecta=equal devilbox=equal$' || fail "roundtrip: $out"
out=$("$cli" share make playlist modland mix/one 2>&1) || fail "playlist make: $out"
printf '%s\n' "$out" | grep -q '^ROUNDTRIP selecta=equal devilbox=equal$' || fail "playlist roundtrip: $out"
out=$("$cli" share make view 'genre:chip bpm>120' 2>&1) || fail "view make: $out"
printf '%s\n' "$out" | grep -q '^ROUNDTRIP selecta=equal devilbox=equal$' || fail "view roundtrip: $out"

# (b) both forms name the same key.
a=$("$cli" share parse 'selecta://song/modland/p/purple.xm?sub=2&t=5000' 2>&1) || fail "parse selecta: $a"
b=$("$cli" share parse 'https://devilbox.uprough.net/?song=modland%2Fp%2Fpurple.xm&sub=2&t=5000' 2>&1) || fail "parse devilbox: $b"
[ "$a" = "$b" ] || fail "forms differ: $a / $b"
printf '%s\n' "$a" | grep -q 'sub=2 t=5000 key=modland:p/purple.xm' || fail "parsed fields: $a"

# (c) malformed links are refused.
out=$("$cli" share parse 'selecta://song/nosource' 2>&1) && fail "bad link parsed"
printf '%s\n' "$out" | grep -q 'ERROR bad_link' || fail "no bad_link: $out"
out=$("$cli" open 'selecta://nonsense/x/y' --catalogue "$cat" 2>&1)
printf '%s\n' "$out" | grep -q 'bad_link' || fail "open without bad_link: $out"

# (d) SHARE position makes a link carrying the current position.
out=$("$cli" share position selecta-test.mod --at 5000 --catalogue "$cat" 2>&1) || fail "share position: $out"
printf '%s\n' "$out" | grep -q 't=5000' || fail "no t=5000 in position link: $out"

# (e) selecta-cli open: the link plays the song at the position, counters prove the parser ran.
out=$("$cli" open 'selecta://song/local/selecta-test.mod?t=5000' --catalogue "$cat" 2>&1) || fail "cli open: $out"
printf '%s\n' "$out" | grep -q '^SEEKED 220500' || fail "cli open SEEKED: $out"
printf '%s\n' "$out" | grep -q '^SHARE links_parsed=1 links_made=0 songs_opened=1' || fail "cli open counters: $out"

# (f) the real selecta binary takes the link as an argument.
out=$("$sel" --output null --size 80x25 --catalogue "$cat" --keys "$keys" 'selecta://song/local/selecta-test.mod?t=5000' 2>&1) || fail "selecta: $out"
printf '%s\n' "$out" | grep -q 'links_parsed=1' || fail "selecta links_parsed: $out"
printf '%s\n' "$out" | grep -q 'songs_opened=1' || fail "selecta songs_opened: $out"
echo "share_ok links_parsed=1 songs_opened=1"
