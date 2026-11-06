#!/bin/sh
# I710af: T'SoundSystem songs (.tss MML, .tsd compiled) play through `selecta-cli render` as a RENDER-kind replayer (bbmp_tss module, BSD-3-Clause
# bbmp csrc/retro tss_engine). Songs are fetched by checksum (tests/m3/fixtures/bbmp_tss.SOURCE). Exit 77 = skipped (no network).
# Checks per song: the plugin ran (replayer=bbmp_tss sentinel), the WAV is not silent (TS_ENERGY_MIN raises the floor, for red proofs) and
# rendered_frames is within the measured band (TS_MIN_FRAMES raises the floor). A file that is not a TSS song is not claimed.
# usage: tss_decode.sh <selecta-cli>
set -u
cli=$1
here=$(dirname "$0")
M="https://modland.com/pub/modules/TSS/_unknown"
field() { printf '%s\n' "$1" | tr ' ' '\n' | sed -n "s/^$2=//p" | head -1; }
fail() { echo "FAIL: $*"; exit 1; }
energy() { od -An -v -t d2 -j 44 "$1" | tr -s " " "\n" | awk '$1 != "" { s += $1 * $1 } END { printf "%d", s }'; }
rate=44100
fetch() { "$here/fetch_fixture.sh" "$1" "$2" "$3" || exit $?; }
s1=$(fetch "$M/gradius%20st1.tss" 5f6fb1aad10030b2e2e6346e7df5035deb1d0b662ebd0b40e152553796e6b3f6 tss_gradius_st1.tss) || exit $?
s2=$(fetch "$M/galg%20title.tsd" 138e7b6395918cd12efc9d0622bdddc8bacf6e6888b3b04dca940760c0b3d455 tss_galg_title.tsd) || exit $?
# A file that is not a TSS song is not claimed, whatever its name says.
head -c 700 /dev/zero | tr '\0' 'A' > nottss.tss
out=$("$cli" render nottss.tss --output fd_ts.wav --seconds 1 --rate "$rate" 2>/dev/null) || true
[ "$(field "$out" replayer)" != "bbmp_tss" ] || fail "bbmp_tss claimed a file that is not a TSS song"
rm -f nottss.tss fd_ts.wav
# Measured 2026-11-06: gradius st1 1191584 frames (27.0 s), energy 3.7e14; galg title 2060064 frames (46.7 s), energy 9.1e14; frames at 44100 Hz.
for s in "$s1" "$s2"; do
  case "$s" in
    *gradius*) fmin=1130000; fmax=1430000; emin=30000000000000 ;;
    *) fmin=1950000; fmax=2480000; emin=90000000000000 ;;
  esac
  rm -f fd_ts.wav
  out=$("$cli" render "$s" --output fd_ts.wav --seconds 900 --rate "$rate" 2>/dev/null) || fail "render failed: $s"
  [ "$(field "$out" replayer)" = "bbmp_tss" ] || fail "$s: replayer=$(field "$out" replayer), want bbmp_tss"
  got=$(field "$out" rendered_frames)
  [ "$got" -ge "${TS_MIN_FRAMES:-$fmin}" ] || fail "$s: rendered_frames=$got, want at least ${TS_MIN_FRAMES:-$fmin}"
  [ "$got" -le "$fmax" ] || fail "$s: rendered_frames=$got, want at most $fmax"
  e=$(energy fd_ts.wav)
  [ "$e" -gt "${TS_ENERGY_MIN:-$emin}" ] || fail "$s: silent output (energy $e)"
  echo "$s frames=$got energy=$e"
done
rm -f fd_ts.wav
echo tss_decode_ok
