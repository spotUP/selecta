#!/bin/sh
# I725j: test clips, I194 recipe (sine 440 Hz + pink noise, stereo) but 10 s. Usage: make_clips.sh <outdir>
set -e
o=${1:?outdir}; mkdir -p "$o"; cd "$o"
src() { ffmpeg -hide_banner -loglevel error -y -f lavfi -i "sine=frequency=440:sample_rate=44100:duration=${DUR:-10}" \
  -f lavfi -i "anoisesrc=color=pink:sample_rate=44100:duration=${DUR:-10}:amplitude=0.3" \
  -filter_complex "amix=inputs=2:duration=first,pan=stereo|c0=c0|c1=c0" "$@"; }
src -c:a vorbis -strict -2 -b:a 128k t.ogg
src -c:a libopus -b:a 96k -ar 48000 t.opus
src -c:a aac -b:a 128k -f adts t.aac
src -c:a flac t.flac
ls -l
