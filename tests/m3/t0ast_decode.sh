#!/bin/sh
# I710ag: T0AST (The 0ok Amazing Synth Tracker, .t0ast) songs play through `selecta-cli render` as a RENDER-kind replayer (bbmp_t0ast module, GPL-3.0-or-later
# bbmp csrc/t0ast clean-room engine). Songs are fetched by checksum (tests/m3/fixtures/bbmp_t0ast.SOURCE). Exit 77 = skipped (no network).
# The plugin ran (replayer=bbmp_t0ast sentinel), the WAV is not silent (T0_ENERGY_MIN raises the floor, for red proofs) and rendered_frames equals the
# song length the engine measures by the tracker's export rule (T0_MIN_FRAMES raises the expected value, for red proofs). A file that is not a T0AST song
# (wrong magic, or a song without the extension) is not claimed.
# usage: t0ast_decode.sh <selecta-cli>
set -u
cli=$1
here=$(dirname "$0")
M="https://modland.com/pub/modules/The%200ok%20Amazing%20Synth%20Tracker/BeRo"
field() { printf '%s\n' "$1" | tr ' ' '\n' | sed -n "s/^$2=//p" | head -1; }
fail() { echo "FAIL: $*"; exit 1; }
energy() { od -An -v -t d2 -j 44 "$1" | tr -s " " "\n" | awk '$1 != "" { s += $1 * $1 } END { printf "%d", s }'; }
rate=44100
fetch() { "$here/fetch_fixture.sh" "$1" "$2" "$3" || exit $?; }
s1=$(fetch "$M/lost%20in%20time.t0ast" 269a19be1eeeea0790403320b0d6d325f5abee9729981d91f631e2487aa0ff11 lost_in_time.t0ast) || exit $?
s2=$(fetch "$M/hard%20and%20harder.t0ast" d91d9ce8a1a149d5a97404bb71c5f97268a7fe7ae07291d508a763531afb0eac hard_and_harder.t0ast) || exit $?
# Not claimed: a non-song with the extension, and a song under another name.
head -c 700 /dev/zero | tr '\0' 'A' > nott0.t0ast
out=$("$cli" render nott0.t0ast --output fd_t0.wav --seconds 1 --rate "$rate" 2>/dev/null) || true
[ "$(field "$out" replayer)" != "bbmp_t0ast" ] || fail "bbmp_t0ast claimed a file that is not a T0AST song"
cp "$s1" renamed.bin
out=$("$cli" render renamed.bin --output fd_t0.wav --seconds 1 --rate "$rate" 2>/dev/null) || true
[ "$(field "$out" replayer)" != "bbmp_t0ast" ] || fail "bbmp_t0ast claimed a song without the extension"
rm -f nott0.t0ast renamed.bin fd_t0.wav
# Measured 2026-11-06 (rate 44100): FRAMES / energy per song, from the engine's own length (measure()).
for s in "$s1" "$s2"; do
  case "$s" in
    *lost_in_time*) want=7628800; emin=28000000000000 ;;
    *) want=12204032; emin=4200000000000 ;;
  esac
  rm -f fd_t0.wav
  out=$("$cli" render "$s" --output fd_t0.wav --seconds 900 --rate "$rate" 2>/dev/null) || fail "render failed: $s"
  [ "$(field "$out" replayer)" = "bbmp_t0ast" ] || fail "$s: replayer=$(field "$out" replayer), want bbmp_t0ast"
  got=$(field "$out" rendered_frames)
  want=${T0_MIN_FRAMES:-$want}
  [ "$got" -eq "$want" ] || fail "$s: rendered_frames=$got, want $want"
  e=$(energy fd_t0.wav)
  [ "$e" -gt "${T0_ENERGY_MIN:-$emin}" ] || fail "$s: silent output (energy $e)"
  echo "$s frames=$got energy=$e"
done
rm -f fd_t0.wav
echo t0ast_decode_ok
