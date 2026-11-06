#!/bin/sh
# I710ae: StoneTracker songs (.spm + .sps bank) play through `selecta-cli render` as a RENDER-kind replayer (stonetracker module): the authors' no-OS
# StonePlayer_Hard.bin runs on Musashi's 68020 in a minimal Amiga. The player bin is a user system file, never shipped: STONEPLAYER_HARD_BIN names the
# unpacked bin (sha256-checked by the host); without it the test skips (77). Song and bank are fetched by checksum (fixtures/stonetracker.SOURCE).
# Sentinels: replayer=stonetracker, the "68k host ran" trace line (SELECTA_STONETRACKER_TRACE), non-silent energy (ST_ENERGY_MIN raises the floor),
# frames within the song-length band (ST_MIN_FRAMES raises the floor), bank_missing without the .sps, system_file_missing without the bin.
# usage: stonetracker_decode.sh <selecta-cli>
set -u
cli=$1
here=$(dirname "$0")
M="https://modland.com/pub/modules/Stonetracker/Bosco"
field() { printf '%s\n' "$1" | tr ' ' '\n' | sed -n "s/^$2=//p" | head -1; }
fail() { echo "FAIL: $*"; exit 1; }
energy() { od -An -v -t d2 -j 44 "$1" | tr -s " " "\n" | awk '$1 != "" { s += $1 * $1 } END { printf "%d", s }'; }
rate=44100
[ -n "${STONEPLAYER_HARD_BIN:-}" ] && [ -f "$STONEPLAYER_HARD_BIN" ] || { echo "SKIP: set STONEPLAYER_HARD_BIN to the unpacked StonePlayer_Hard.bin (Aminet stonefree2.lha, Players.LHS)"; exit 77; }
fetch() { "$here/fetch_fixture.sh" "$1" "$2" "$3" || exit $?; }
spm=$(fetch "$M/teinteremix22.spm" eb0bf66b1c0a36baf2984fd7e3439f5f6fa6c752a8f5ad3de30dcbdf4b4926a8 teinteremix22.spm) || exit $?
sps=$(fetch "$M/teinteremix22.sps" 01334388e2a5b8994de7ecd79ae644c7608ef7d4c5c68170eff1550a3501a28b teinteremix22.sps) || exit $?
rm -rf stsong stcfg stempty && mkdir stsong stcfg stempty
cp "$spm" stsong/t.spm
cp "$sps" stsong/t.sps
cp "$STONEPLAYER_HARD_BIN" stcfg/StonePlayer_Hard.bin
cfg=$PWD/stcfg
SELECTA_STONETRACKER_TRACE=1
export SELECTA_STONETRACKER_TRACE
rm -f fd_st.wav
out=$("$cli" render stsong/t.spm --output fd_st.wav --seconds 300 --rate "$rate" --config-dir "$cfg" 2>st_err.txt) || { cat st_err.txt; fail "render failed"; }
[ "$(field "$out" replayer)" = "stonetracker" ] || fail "replayer=$(field "$out" replayer), want stonetracker"
grep -q "stonetracker: 68k host ran" st_err.txt || fail "no 68k host sentinel on stderr"
got=$(field "$out" rendered_frames)
# Measured 2026-11-06 (rate 44100, I710aec): the song ends by itself (VL_SongEnd / revisited position, as bbmp) at 3044864 frames = 69.05 s, well under the
# --seconds cap of 300 s, so a render that never detects the end hits the cap (13230000) and fails the upper bound.
[ "$got" -ge "${ST_MIN_FRAMES:-3000000}" ] && [ "$got" -le 3100000 ] || fail "rendered_frames=$got outside 3000000..3100000"
e=$(energy fd_st.wav)
[ "$e" -gt "${ST_ENERGY_MIN:-1000000000000}" ] || fail "silent output (energy $e)"
echo "frames=$got energy=$e"
# A missing bank: the song opens nowhere and is not played.
rm -f stsong/t.sps
out=$("$cli" render stsong/t.spm --output fd_st.wav --seconds 1 --rate "$rate" --config-dir "$cfg" 2>&1) || true
[ "$(field "$out" replayer)" != "stonetracker" ] || fail "stonetracker played a song with no sample bank"
# Without the player bin the host reports system_file_missing (it never ships the player).
cp "$sps" stsong/t.sps
out=$("$cli" render stsong/t.spm --output fd_st.wav --seconds 1 --rate "$rate" --config-dir "$PWD/stempty" 2>&1) || true
case "$out" in *system_file_missing*) ;; *) fail "no system_file_missing without the player bin: $out" ;; esac
rm -f fd_st.wav st_err.txt
echo stonetracker_decode_ok
