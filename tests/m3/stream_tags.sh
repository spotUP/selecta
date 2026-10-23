#!/bin/sh
# I194b: ID3v2.3/2.4 (UTF-16 and UTF-8 frames), ID3v1 and Vorbis comment tags reach the library through the
# replayer metadata path. One reachability test through the product entry point: `selecta-cli library scan --dump`
# prints the title, artist and album columns read back from the database.
# usage: stream_tags.sh <selecta-cli> <fixture-dir>
set -u
cli=$1
dir=$2
fail() { echo "FAIL: $*"; exit 1; }

rm -rf st_dir st.db
mkdir st_dir
cp "$dir"/tagged_v23.mp3 "$dir"/tagged_v24.mp3 "$dir"/tagged_v1.mp3 "$dir"/tagged.ogg st_dir/
out=$("$cli" library scan --db st.db --dump st_dir 2>/dev/null) || fail "scan failed"

want() { # <file> <title|artist|album>
    printf '%s\n' "$out" | grep -F "song=st_dir/$1|" | grep -qF "|$2" || fail "$1: want |$2 in: $(printf '%s\n' "$out" | grep -F "$1")"
}
want tagged_v23.mp3 "Tést Title|Ärtist Name|Albüm One"
want tagged_v24.mp3 "Tést Title|Ärtist Name|Albüm One"
want tagged_v1.mp3 "V1 Tité|V1 Artist|V1 Album"
want tagged.ogg "Tést Title|Ärtist Name|Albüm One"
echo stream_tags_ok
