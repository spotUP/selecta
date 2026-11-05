#!/bin/sh
# I706a: a SunVox song (.sunvox) plays through `selecta-cli render` as a RENDER-kind replayer (sunvox module over the BSD engine).
# One reachability test through the product entry point; the sentinel is the `replayer=sunvox` line of render.
# Asserts: the plugin ran, the WAV is not silent (SV_ENERGY_MIN raises the floor for red proofs) and the song lasts
# at least SV_MIN_SECONDS (default 5; the song ends by itself, so --seconds is set far above it).
# Fixture: tests/m3/fixtures/sunvox_tune.sunvox (source and licence note in sunvox_tune.sunvox.SOURCE).
# Needs SELECTA_PLUGIN_PATH to hold the sunvox module (set by the test registration).
# usage: sunvox_decode.sh <selecta-cli> <fixture>...
set -u
cli=$1
shift
field() { printf '%s\n' "$1" | tr ' ' '\n' | sed -n "s/^$2=//p" | head -1; }
fail() { echo "FAIL: $*"; exit 1; }
# I706b: the I706a test covers the BSD engine alone, so it runs with a plugin folder that holds only sel_sunvox.* (the
# sunvox_lib module would otherwise win on desktop).
iso=sv_bsd_only
rm -rf "$iso"
mkdir "$iso" || fail "mkdir $iso"
for dir in $(printf '%s' "${SELECTA_PLUGIN_PATH:-}" | tr ':' ' '); do
    for m in "$dir"/sel_sunvox.*; do [ -f "$m" ] && cp "$m" "$iso/"; done
done
SELECTA_PLUGIN_PATH="$PWD/$iso"
export SELECTA_PLUGIN_PATH
rate=44100
for f in "$@"; do
    rm -f fd_sv.wav
    out=$("$cli" render "$f" --output fd_sv.wav --seconds 300 --rate "$rate" 2>/dev/null) || fail "render failed: $f"
    [ "$(field "$out" replayer)" = "sunvox" ] || fail "$f: replayer=$(field "$out" replayer), want sunvox"
    got=$(field "$out" rendered_frames)
    [ "$got" -ge $((rate * ${SV_MIN_SECONDS:-5})) ] || fail "$f: rendered_frames=$got, want at least $((rate * ${SV_MIN_SECONDS:-5}))"
    e=$(od -An -v -t d2 -j 44 fd_sv.wav | tr -s " " "\n" | awk '$1 != "" { s += $1 * $1 } END { printf "%d", s }')
    [ "$e" -gt "${SV_ENERGY_MIN:-1000000000}" ] || fail "$f: silent output (energy $e)"
done
echo sunvox_decode_ok
# Refusal (a song with a newer VERS than the 2009 engine reads): no crash, selecta-cli fails the render, no sunvox sentinel.
for f in "$@"; do
    new=fd_sv_new.sunvox
    { printf 'SVOX\000\000\000\000VERS\004\000\000\000\000\001\011\001'; tail -c +21 "$f"; } > "$new"
    out=$("$cli" render "$new" --output fd_sv_new.wav --seconds 5 --rate "$rate" 2>/dev/null)
    rc=$?
    [ "$rc" -lt 128 ] || fail "newer-version song crashed the host (rc=$rc)"
    [ "$(field "$out" replayer)" != "sunvox" ] || fail "newer-version song was accepted by the sunvox plugin"
done
echo sunvox_refusal_ok
