#!/bin/sh
# I725j: run every built Mac bench on its clip. Usage: run_mac.sh <bin-dir> <clip-dir> [reps]
b=$1; m=$2; r=${3:-3}
for t in stb_vorbis:t.ogg tremor:t.ogg tremolo:t.ogg opus:t.opus opusfix:t.opus faad2:t.aac fdk:t.aac drflac:t.flac libflac:t.flac; do
  d=${t%%:*}; f=${t##*:}
  [ -x "$b/bench_$d" ] && nice -n 19 "$b/bench_$d" "$m/$f" "$r"
done
