#!/bin/sh
# I709a: ZX Spectrum AY songs play through `selecta-cli render` as a RENDER-kind replayer (bbmp_ay module, GPL-3.0-or-later
# bbmp csrc/ay engines on the Ayumi chip). One real song per format: Flash Tracker .fls, Fuxoft AY Language .fxm and its
# AY-file wrapping .amad, Sound Tracker compiled by Raxoft (.strc). The songs have no stated redistribution licence, so
# they are fetched by checksum, never committed (tests/m3/fixtures/bbmp_ay.SOURCE). Exit 77 = skipped (no network).
# One reachability test through the product entry point; the sentinel is the `replayer=bbmp_ay` line of render.
# Asserts per song: the plugin ran, the WAV is not silent (AY_ENERGY_MIN raises the floor for red proofs) and the song
# lasts at least its floor in seconds (AY_MIN_SECONDS, when set, replaces every per-song floor, for red proofs).
# Needs SELECTA_PLUGIN_PATH to hold the bbmp_ay module (set by the test registration).
# usage: bbmp_ay_decode.sh <selecta-cli>
set -u
cli=$1
here=$(dirname "$0")
M=https://modland.com/pub/modules
field() { printf '%s\n' "$1" | tr ' ' '\n' | sed -n "s/^$2=//p" | head -1; }
fail() { echo "FAIL: $*"; exit 1; }
energy() { od -An -v -t d2 -j 44 "$1" | tr -s " " "\n" | awk '$1 != "" { s += $1 * $1 } END { printf "%d", s }'; }
rate=44100
check() { # <url> <sha256> <name> <min seconds>
  f=$("$here/fetch_fixture.sh" "$1" "$2" "$3") || exit $?
  rm -f fd_ay.wav
  out=$("$cli" render "$f" --output fd_ay.wav --seconds 300 --rate "$rate" 2>/dev/null) || fail "render failed: $3"
  [ "$(field "$out" replayer)" = "bbmp_ay" ] || fail "$3: replayer=$(field "$out" replayer), want bbmp_ay"
  got=$(field "$out" rendered_frames)
  min=${AY_MIN_SECONDS:-$4}
  [ "$got" -ge $((rate * min)) ] || fail "$3: rendered_frames=$got, want at least $((rate * min))"
  e=$(energy fd_ay.wav)
  [ "$e" -gt "${AY_ENERGY_MIN:-1000000}" ] || fail "$3: silent output (energy $e)"
  echo "$3 frames=$got energy=$e"
  rm -f fd_ay.wav
}
check "$M/Spectrum/Flash%20Tracker/KVS/simpletongift1.fls" c97891632224db086c03573f878c052a1a30caa5fb39a43e55c2c43b90613611 simpletongift1.fls 250
check "$M/Spectrum/Fuxoft%20AY%20Language/Fuxoft/belegost.fxm" 96b874b8254d5ad6a6a5527d85ecf4967d00c00a3545f8d5ae2dfcdc6bce5f38 belegost.fxm 100
check "$M/AY%20Amadeus/Frantisek%20Fuka/belegost.amad" ae6df48f75986cd8f5a1ce49d356aaee84fe4e74c1e868e377f5365e7ced8fe0 belegost.amad 100
check "$M/AY%20STRC/_unknown/mega%20mix%201.strc" 5dcae12c0ea0125127954fd785a66acd42a4f0dc2aac68d6da9df5dfa87ef9ed mega_mix_1.strc 100
echo bbmp_ay_decode_ok
