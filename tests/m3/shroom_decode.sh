#!/bin/sh
# I710aa: Mario Paint songs (.sho) play through `selecta-cli render` as a RENDER-kind replayer (bbmp_shroom module, GPL-3.0-or-later bbmp
# csrc/retro shroom engine). The instrument sounds are Nintendo data: a USER-SUPPLIED system file (mariopants-notes.wav = Mariopants 1.1 data/notes.wav),
# never fetched or committed. Songs are fetched by checksum (tests/m3/fixtures/bbmp_shroom.SOURCE). Exit 77 = skipped (no network).
# Part 1 (always): without the sample set the host reports system_file_missing and bbmp_shroom does not play; a non-song is not claimed.
# Part 2 (needs env SELECTA_SHROOM_SAMPLES = path of notes.wav): the plugin ran (replayer=bbmp_shroom sentinel), the WAV is not silent
# (SH_ENERGY_MIN raises the floor, for red proofs) and rendered_frames is within the measured band (SH_MIN_FRAMES raises the floor).
# Without the env var part 2 exits 77 (SKIP) after part 1 passed.
# usage: shroom_decode.sh <selecta-cli>
set -u
cli=$1
here=$(dirname "$0")
M="https://modland.com/pub/modules/Shroom"
field() { printf '%s\n' "$1" | tr ' ' '\n' | sed -n "s/^$2=//p" | head -1; }
fail() { echo "FAIL: $*"; exit 1; }
energy() { od -An -v -t d2 -j 44 "$1" | tr -s " " "\n" | awk '$1 != "" { s += $1 * $1 } END { printf "%d", s }'; }
rate=44100
fetch() { "$here/fetch_fixture.sh" "$1" "$2" "$3" || exit $?; }
s1=$(fetch "$M/Andy%20Lee/untitled.sho" 05a30d0fa9fd75a424df9c3795cb3c7b70fe0a1082e110ae7e091c3849128e47 andylee.sho) || exit $?
s2=$(fetch "$M/Adam%20Netke/untitled.sho" dec4cbca71f8062ace7627647c495b26664c382c7ccf91a3fd8b56050bfb467a netke.sho) || exit $?
unset SELECTA_SHROOM_NOTES BBMP_SHROOM_NOTES
rm -rf shcfg shempty && mkdir shcfg shempty
# Part 1: no sample set -> system_file_missing, and the plugin does not play.
for s in "$s1" "$s2"; do
  out=$("$cli" render "$s" --output fd_sh.wav --seconds 1 --rate "$rate" --config-dir "$PWD/shempty" 2>&1) || true
  case "$out" in *system_file_missing*) ;; *) fail "$s: no system_file_missing without the sample set: $out" ;; esac
  [ "$(field "$out" replayer)" != "bbmp_shroom" ] || fail "bbmp_shroom played a song with no sample set"
done
# A file that is not a Shroom song is not claimed, whatever its name says.
head -c 700 /dev/zero | tr '\0' 'A' > notsho.sho
out=$("$cli" render notsho.sho --output fd_sh.wav --seconds 1 --rate "$rate" --config-dir "$PWD/shempty" 2>/dev/null) || true
[ "$(field "$out" replayer)" != "bbmp_shroom" ] || fail "bbmp_shroom claimed a file that is not a Shroom song"
rm -f notsho.sho fd_sh.wav
# Part 2: the user's sample set.
samples=${SELECTA_SHROOM_SAMPLES:-}
if [ -z "$samples" ] || [ ! -f "$samples" ]; then
  echo "shroom_decode: part 1 ok (system_file_missing); SELECTA_SHROOM_SAMPLES not set, playback SKIPPED"
  rm -rf shcfg shempty
  exit 77
fi
cp "$samples" shcfg/mariopants-notes.wav
cfg=$PWD/shcfg
# Measured 2026-11-06 (notes.wav sha 039f642e..., rate 44100): andylee 737140 frames (16.7 s), energy 1.39e13; netke 1154941 frames (26.2 s), energy 9.16e12.
# Floors/ceilings: frames within about 5 % below and 20 % above the measurement, energy at about a tenth of it.
for s in "$s1" "$s2"; do
  case "$s" in
    *andylee*) fmin=700000; fmax=890000; emin=1000000000000 ;;
    *) fmin=1100000; fmax=1400000; emin=900000000000 ;;
  esac
  rm -f fd_sh.wav
  out=$("$cli" render "$s" --output fd_sh.wav --seconds 900 --rate "$rate" --config-dir "$cfg" 2>/dev/null) || fail "render failed: $s"
  [ "$(field "$out" replayer)" = "bbmp_shroom" ] || fail "$s: replayer=$(field "$out" replayer), want bbmp_shroom"
  got=$(field "$out" rendered_frames)
  [ "$got" -ge "${SH_MIN_FRAMES:-$fmin}" ] || fail "$s: rendered_frames=$got, want at least ${SH_MIN_FRAMES:-$fmin}"
  [ "$got" -le "$fmax" ] || fail "$s: rendered_frames=$got, want at most $fmax"
  e=$(energy fd_sh.wav)
  [ "$e" -gt "${SH_ENERGY_MIN:-$emin}" ] || fail "$s: silent output (energy $e)"
  echo "$s frames=$got energy=$e"
done
rm -rf shcfg shempty fd_sh.wav
echo shroom_decode_ok
