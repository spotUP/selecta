#!/bin/sh
# Regenerates chained.ogg (I195f): two Ogg Vorbis streams, one after the other in one file, the way an Ogg radio
# station chains one logical stream per track. Each part is 1.0 s of stereo 22050 Hz sine (440 Hz, then 880 Hz) with
# its own TITLE comment, so the second part starts with a new BOS page, a new serial and new Vorbis headers.
# Needs ffmpeg (native vorbis encoder). Usage: sh make_chained_ogg.sh
set -e
dir=$(cd "$(dirname "$0")" && pwd)
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
ffmpeg -v error -y -f lavfi -i "sine=frequency=440:duration=1:sample_rate=22050" -ac 2 -c:a vorbis -strict -2 \
  -metadata title="Chain One" -metadata artist="Selecta" "$tmp/a.ogg"
ffmpeg -v error -y -f lavfi -i "sine=frequency=880:duration=1:sample_rate=22050" -ac 2 -c:a vorbis -strict -2 \
  -metadata title="Chain Two" -metadata artist="Selecta" "$tmp/b.ogg"
cat "$tmp/a.ogg" "$tmp/b.ogg" > "$dir/chained.ogg"
