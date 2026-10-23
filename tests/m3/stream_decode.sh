#!/bin/sh
# T94 decode part (I194, D-M3-8): MP3 (minimp3) and Ogg Vorbis (stb_vorbis) play through `selecta-cli render`
# as RENDER-kind replayers. One reachability test through the product entry point; the sentinel is the
# `replayer=<plugin>` line of render. Fixtures: tests/m3/stream/sine.mp3 (44100 Hz, 14976 frames) and
# sine.ogg (22050 Hz, 0.3 s, 6615 frames).
# usage: stream_decode.sh <selecta-cli> <fixture-dir>
set -u
cli=$1
dir=$2
field() { printf '%s\n' "$1" | tr ' ' '\n' | sed -n "s/^$2=//p" | head -1; }
fail() { echo "FAIL: $*"; exit 1; }
near() { awk -v a="$1" -v b="$2" -v t="$3" 'BEGIN { d = a - b; if (d < 0) d = -d; exit !(d <= t) }'; }
energy() { od -An -v -t d2 -j 44 "$1" | tr -s " " "\n" | awk '$1 != "" { s += $1 * $1 } END { printf "%d", s }'; }

check() { # <file> <plugin> <rate> <frames> <tolerance>
    wav=sd_$2_$3.wav
    out=$("$cli" render "$1" --output "$wav" --seconds 2 --rate "$3" 2>/dev/null) || fail "render $1 at $3 failed"
    [ "$(field "$out" replayer)" = "$2" ] || fail "$1: replayer=$(field "$out" replayer), want $2"
    got=$(field "$out" rendered_frames)
    near "$got" "$4" "$5" || fail "$1 at $3 Hz: rendered_frames=$got, want $4 +-$5"
    [ "$(energy "$wav")" -gt 1000000 ] || fail "$1 at $3 Hz: silent output"
}

rm -f sd_*.wav
check "$dir/sine.mp3" minimp3 44100 14976 0
check "$dir/sine.mp3" minimp3 22050 7488 2
check "$dir/sine.ogg" stb_vorbis 22050 6615 64
check "$dir/sine.ogg" stb_vorbis 44100 13230 128
echo stream_decode_ok
