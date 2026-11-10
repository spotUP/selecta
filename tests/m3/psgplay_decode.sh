#!/bin/sh
# I707: an SNDH tune plays through `selecta-cli render` on the psgplay module (second SNDH backend beside sc68).
# One reachability test through the product entry point; the sentinel is the `replayer=` line of render.
# 1. Only the psgplay module on the plugin path: replayer=psgplay, the WAV is not silent (energy above PSGPLAY_ENERGY_MIN)
#    and holds at least PSGPLAY_FRAMES_MIN frames (default 2 s).
# 2. Both SNDH modules on the path: the default stays sc68 (psgplay has the lower SEL_RP_PRIORITY).
# 3. No SNDH module on the path: render must fail (the pass above is due to the module).
# Fixture: tests/fixtures/sndh/oculus_ice.sndh (PROVENANCE.md next to it).
# usage: psgplay_decode.sh <selecta-cli> <modules dir> <fixture>
set -u
cli=$1
mods=$2
song=$3
field() { printf '%s\n' "$1" | tr ' ' '\n' | sed -n "s/^$2=//p" | head -1; }
fail() { echo "FAIL: $*"; exit 1; }
export SELECTA_NO_APP_PLUGINS=1 # I1305d: only the named modules, not the CLI plugins folder (I1242b)
rate=44100
rm -rf pg_only pg_both pg_none
mkdir pg_only pg_both pg_none
for m in "$mods"/sel_psgplay.*; do ln -s "$m" pg_only/; ln -s "$m" pg_both/; done
for m in "$mods"/sel_sndh.*; do ln -s "$m" pg_both/; done
[ -n "$(ls pg_only)" ] || fail "no psgplay module in $mods"

out=$(SELECTA_PLUGIN_PATH="$PWD/pg_only" "$cli" render "$song" --output pg.wav --seconds 5 --rate "$rate" 2>/dev/null) || fail "render with psgplay failed"
[ "$(field "$out" replayer)" = "psgplay" ] || fail "replayer=$(field "$out" replayer), want psgplay"
got=$(field "$out" rendered_frames)
[ "$got" -ge "${PSGPLAY_FRAMES_MIN:-$((rate * 2))}" ] || fail "rendered_frames=$got, want at least ${PSGPLAY_FRAMES_MIN:-$((rate * 2))}"
e=$(od -An -v -t d2 -j 44 pg.wav | tr -s " " "\n" | awk '$1 != "" { s += $1 * $1 } END { printf "%d", s }')
[ "$e" -gt "${PSGPLAY_ENERGY_MIN:-1000000}" ] || fail "silent output (energy $e)"

out=$(SELECTA_PLUGIN_PATH="$PWD/pg_both" "$cli" render "$song" --output pg_both.wav --seconds 1 --rate "$rate" 2>/dev/null) || fail "render with both modules failed"
[ "$(field "$out" replayer)" = "sc68" ] || fail "default SNDH replayer=$(field "$out" replayer), want sc68"

if SELECTA_PLUGIN_PATH="$PWD/pg_none" "$cli" render "$song" --output pg_none.wav --seconds 1 --rate "$rate" 2>/dev/null | grep -q 'replayer=\(psgplay\|sc68\)'; then
    fail "render succeeded with no SNDH module on the plugin path"
fi
rm -rf pg_only pg_both pg_none pg.wav pg_both.wav pg_none.wav
echo "psgplay_decode_ok energy=$e frames=$got"
