#!/bin/sh
# I701: an FM Towns Euphony song (.eup) plays through `selecta-cli render` as a RENDER-kind replayer (eupmini module).
# One reachability test through the product entry point; the sentinel is the `replayer=eupmini` line of render.
# Asserts: the plugin ran, the WAV is not silent (EUP_ENERGY_MIN raises the floor for red proofs) and the song lasts
# at least EUP_MIN_SECONDS (default 100; the fixture runs 125 s and ends by itself, so --seconds is set far above it).
# I1127: the fixture's companion banks (fmtone2.fmb, piano.pmb, named in the song header) sit beside it. The song is
# rendered again from an empty directory (default FM voices, no PCM); the render with banks must carry at least
# EUP_BANK_RATIO (default 3) times that energy, so a bank that is not loaded fails. Red proof: run on a copy of the
# fixture that has no banks beside it.
# Fixture: tests/m3/fixtures/eup_test.eup (source and licence note in eup_test.eup.SOURCE).
# Needs SELECTA_PLUGIN_PATH to hold the eupmini module (set by the test registration).
# usage: eup_decode.sh <selecta-cli> <fixture>...
set -u
cli=$1
shift
field() { printf '%s\n' "$1" | tr ' ' '\n' | sed -n "s/^$2=//p" | head -1; }
fail() { echo "FAIL: $*"; exit 1; }
rate=44100
for f in "$@"; do
    rm -f fd_eup.wav
    out=$("$cli" render "$f" --output fd_eup.wav --seconds 300 --rate "$rate" 2>/dev/null) || fail "render failed: $f"
    [ "$(field "$out" replayer)" = "eupmini" ] || fail "$f: replayer=$(field "$out" replayer), want eupmini"
    got=$(field "$out" rendered_frames)
    [ "$got" -ge $((rate * ${EUP_MIN_SECONDS:-100})) ] || fail "$f: rendered_frames=$got, want at least $((rate * ${EUP_MIN_SECONDS:-100}))"
    e=$(od -An -v -t d2 -j 44 fd_eup.wav | tr -s " " "\n" | awk '$1 != "" { s += $1 * $1 } END { printf "%d", s }')
    [ "$e" -gt "${EUP_ENERGY_MIN:-1000000}" ] || fail "$f: silent output (energy $e)"
    rm -rf fd_eup_nobank
    mkdir fd_eup_nobank && cp "$f" fd_eup_nobank/ || fail "cannot stage $f"
    "$cli" render "fd_eup_nobank/$(basename "$f")" --output fd_eup0.wav --seconds 300 --rate "$rate" >/dev/null 2>&1 || fail "render without banks failed: $f"
    e0=$(od -An -v -t d2 -j 44 fd_eup0.wav | tr -s " " "\n" | awk '$1 != "" { s += $1 * $1 } END { printf "%d", s }')
    [ "$e" -ge $((e0 * ${EUP_BANK_RATIO:-3})) ] || fail "$f: energy with banks $e is under ${EUP_BANK_RATIO:-3}x the energy without banks $e0 (banks not loaded)"
    rm -rf fd_eup_nobank fd_eup0.wav
done
echo eup_decode_ok
