#!/bin/sh
# I710g: CyberTracker C64 songs (.ct) play through `selecta-cli render` as a RENDER-kind replayer (bbmp_cybertracker module, GPL-3.0-or-later
# bbmp csrc/retro clean-room engine, which runs the user's own CyberTracker 1.01 disk image on an emulated C64). Two real songs by CyberBrain
# ("arpeggio", "china") and the tracker's disk image are fetched by checksum, never committed (tests/m3/fixtures/bbmp_cybertracker.SOURCE).
# Exit 77 = skipped (no network). Sentinel: the `replayer=bbmp_cybertracker` line of render.
# Asserts per song: the plugin ran, the WAV is not silent (CT_ENERGY_MIN raises the floor, for red proofs) and the rendered frames match
# the song length measured at the engine (CT_MIN_SECONDS, when set, replaces the floor, for red proofs).
# Also: without the disk image the song is not claimed; a file that is not a CyberTracker song is not claimed.
# usage: cybertracker_decode.sh <selecta-cli>
set -u
cli=$1
here=$(dirname "$0")
M="https://modland.com/pub/modules/Cybertracker%20C64/CyberBrain"
field() { printf '%s\n' "$1" | tr ' ' '\n' | sed -n "s/^$2=//p" | head -1; }
fail() { echo "FAIL: $*"; exit 1; }
energy() { od -An -v -t d2 -j 44 "$1" | tr -s " " "\n" | awk '$1 != "" { s += $1 * $1 } END { printf "%d", s }'; }
rate=44100
fetch() { "$here/fetch_fixture.sh" "$1" "$2" "$3" || exit $?; }
zip=$(fetch "https://csdb.dk/getinternalfile.php/63969/cybertracker1_01-d64.zip" cdecc06385ebb5cd1d941889cae8c1ecf33e72965116db652a4e43c50d4040fe cybertracker1_01-d64.zip) || exit $?
rm -rf ctdisk && mkdir ctdisk && (cd ctdisk && unzip -o -q "$zip") || fail "cannot unpack the disk image"
disk=$(ls ctdisk/*.d64 | head -1)
[ -f "$disk" ] || fail "no .d64 in the CyberTracker zip"
cp "$disk" ctdisk/cybertracker101.d64
SELECTA_CYBERTRACKER_DISK=$PWD/ctdisk/cybertracker101.d64
export SELECTA_CYBERTRACKER_DISK
check() { # <song path> <min seconds> <max seconds>
  rm -f fd_ct.wav
  out=$("$cli" render "$1" --output fd_ct.wav --seconds 900 --rate "$rate" 2>/dev/null) || fail "render failed: $1"
  [ "$(field "$out" replayer)" = "bbmp_cybertracker" ] || fail "$1: replayer=$(field "$out" replayer), want bbmp_cybertracker"
  got=$(field "$out" rendered_frames)
  min=${CT_MIN_SECONDS:-$2}
  [ "$got" -ge $((rate * min)) ] || fail "$1: rendered_frames=$got, want at least $((rate * min))"
  [ "$got" -le $((rate * $3)) ] || fail "$1: rendered_frames=$got, far beyond the song length"
  e=$(energy fd_ct.wav)
  [ "$e" -gt "${CT_ENERGY_MIN:-1000000}" ] || fail "$1: silent output (energy $e)"
  echo "$1 frames=$got energy=$e"
  rm -f fd_ct.wav
}
s1=$(fetch "$M/arpeggio.ct" d3ae040598e13d825d3f851e6ad18b7efe811f8e83daa0f5cbee0fda780ce40d arpeggio.ct) || exit $?
s2=$(fetch "$M/china.ct" 503f798e7f6c5f72094a47a98836748d01439eeced2d17c51d3a46a324283a62 china.ct) || exit $?
check "$s1" 110 111
check "$s2" 224 226
# Without the tracker's disk image the song is not claimed (the player never ships the tracker).
out=$(SELECTA_CYBERTRACKER_DISK=$PWD/ctdisk/missing.d64 BBMP_CYBERTRACKER_DISK= "$cli" render "$s1" --output fd_ct.wav --seconds 1 --rate "$rate" 2>/dev/null) || true
[ "$(field "$out" replayer)" != "bbmp_cybertracker" ] || fail "bbmp_cybertracker claimed a song with no disk image"
# A file that is not a CyberTracker song is not claimed, whatever its name says.
head -c 70000 /dev/zero | tr '\0' 'A' > notct.ct
out=$("$cli" render notct.ct --output fd_ct.wav --seconds 1 --rate "$rate" 2>/dev/null) || true
[ "$(field "$out" replayer)" != "bbmp_cybertracker" ] || fail "bbmp_cybertracker claimed a file that is not a CyberTracker song"
# The same with the magic and the block count in front and the blocks cut off.
{ printf '\0\4NNTRKMZX\0\1\21'; head -c 3 /dev/zero; } > hollow.ct
out=$("$cli" render hollow.ct --output fd_ct.wav --seconds 1 --rate "$rate" 2>/dev/null) || true
[ "$(field "$out" replayer)" != "bbmp_cybertracker" ] || fail "bbmp_cybertracker claimed a header whose blocks are cut off"
rm -rf notct.ct hollow.ct fd_ct.wav ctdisk
echo cybertracker_decode_ok
