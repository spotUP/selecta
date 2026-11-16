#!/bin/sh
# I1223h: Ken Silverman's AdLib Music (.ksm, Modland "Ken's AdLib Music/Ken Silverman") plays through `selecta-cli render` (adplug ksm
# player, RENDER-kind replayer). The ksm player reads its instruments from "insts.dat" in the SONG'S DIRECTORY (third_party/adplug
# ksm.cpp), so the test fetches insts.dat next to the song in the git-ignored fixture cache; no I751 support-manifest entry is
# involved. The files have no stated redistribution licence: fetched by checksum, never committed. Exit 77 = skipped (no network).
# Measured: labsng05.ksm 706152 frames (16.0 s) at 44100 Hz, energy 3.47e12. Without insts.dat the open fails (no silent playback).
# usage: adplug_ksm_decode.sh <selecta-cli>
set -u
cli=$1
here=$(dirname "$0")
field() { printf '%s\n' "$1" | tr ' ' '\n' | sed -n "s/^$2=//p" | head -1; }
fail() { echo "FAIL: $*"; exit 1; }
energy() { od -An -v -t d2 -j 44 "$1" | tr -s " " "\n" | awk '$1 != "" { s += $1 * $1 } END { printf "%d", s }'; }
rate=44100
base="https://modland.com/pub/modules/Ad%20Lib/Ken%27s%20AdLib%20Music/Ken%20Silverman"
"$here/fetch_fixture.sh" "$base/insts.dat" ac7c430abc54c0b20b9955929ccd46a769d5a67df93207ace9560d333b539659 insts.dat >/dev/null || exit $?
ksm=$("$here/fetch_fixture.sh" "$base/ken%27s%20labyrinth%20-%20labsng05.ksm" 67f924775c9d72cf3273801f8bb17b49acba36a8ddd979388efa9b67afb28f3a labsng05.ksm) || exit $?
out=$("$cli" render "$ksm" --output ksm.wav --seconds 60 --rate "$rate" 2>/dev/null) || fail "render failed"
[ "$(field "$out" replayer)" = "adplug" ] || fail "replayer=$(field "$out" replayer), want adplug"
got=$(field "$out" rendered_frames)
[ "$got" -ge $((rate * 15)) ] && [ "$got" -le $((rate * 17)) ] || fail "rendered_frames=$got, want 15-17 s"
e=$(energy ksm.wav)
[ "$e" -gt "${KSM_ENERGY_MIN:-1000000}" ] || fail "silent output (energy $e)"
rm -f ksm.wav
# Without the instrument file the song must be refused, not played silent.
d=$(mktemp -d) || exit 1
cp "$ksm" "$d/labsng05.ksm"
out=$("$cli" render "$d/labsng05.ksm" --output ksm.wav --seconds 5 --rate "$rate" 2>/dev/null) || true
rm -rf "$d" ksm.wav
[ "$(field "$out" replayer)" = "adplug" ] && fail "played without insts.dat"
echo "frames=$got energy=$e"
echo adplug_ksm_decode_ok
