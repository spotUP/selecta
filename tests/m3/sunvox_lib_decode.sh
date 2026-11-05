#!/bin/sh
# I706b: SunVox songs play through `selecta-cli render` via the sunvox_lib module (the precompiled SunVox Library).
# Sentinel: the `replayer=sunvox_lib` line of render. Asserts, per case:
#  1. modern song (VERS 2.1.4.3, refused by the 2009 BSD engine): replayer=sunvox_lib, not silent (SVL_ENERGY_MIN raises
#     the floor for red proofs), lasts at least SVL_MIN_SECONDS (default 5)
#  2. old song both plugins read: sunvox_lib wins (SEL_RP_PRIORITY 20 over 10) while both modules are loaded
#  3. library file absent next to the module: the old song falls back to the BSD plugin (replayer=sunvox), the modern
#     one is refused cleanly (no crash, no sentinel)
# Needs SELECTA_PLUGIN_PATH to hold sel_sunvox.* and sel_sunvox_lib.* plus sunvox_lib/ (set by the test registration).
# usage: sunvox_lib_decode.sh <selecta-cli> <old.sunvox> <modern.sunvox>
set -u
cli=$1
old=$2
modern=$3
field() { printf '%s\n' "$1" | tr ' ' '\n' | sed -n "s/^$2=//p" | head -1; }
fail() { echo "FAIL: $*"; exit 1; }
rate=44100
energy() { od -An -v -t d2 -j 44 "$1" | tr -s " " "\n" | awk '$1 != "" { s += $1 * $1 } END { printf "%d", s }'; }

rm -f svl_modern.wav
out=$("$cli" render "$modern" --output svl_modern.wav --seconds 300 --rate "$rate" 2>/dev/null) || fail "render failed: $modern"
[ "$(field "$out" replayer)" = "sunvox_lib" ] || fail "modern: replayer=$(field "$out" replayer), want sunvox_lib"
got=$(field "$out" rendered_frames)
[ "$got" -ge $((rate * ${SVL_MIN_SECONDS:-5})) ] || fail "modern: rendered_frames=$got, want at least $((rate * ${SVL_MIN_SECONDS:-5}))"
e=$(energy svl_modern.wav)
[ "$e" -gt "${SVL_ENERGY_MIN:-1000000000}" ] || fail "modern: silent output (energy $e)"
echo sunvox_lib_decode_ok

rm -f svl_old.wav
out=$("$cli" render "$old" --output svl_old.wav --seconds 300 --rate "$rate" 2>/dev/null) || fail "render failed: $old"
[ "$(field "$out" replayer)" = "sunvox_lib" ] || fail "old: replayer=$(field "$out" replayer), want sunvox_lib to win over sunvox"
echo sunvox_lib_priority_ok

# No library beside the modules: copy both modules into a folder without sunvox_lib/ and without the env override.
iso=svl_iso
rm -rf "$iso"
mkdir "$iso" || fail "mkdir $iso"
for dir in $(printf '%s' "$SELECTA_PLUGIN_PATH" | tr ':' ' '); do
    for m in "$dir"/sel_sunvox.* "$dir"/sel_sunvox_lib.*; do [ -f "$m" ] && cp "$m" "$iso/"; done
done
out=$(SELECTA_PLUGIN_PATH="$PWD/$iso" SELECTA_SUNVOX_LIB= "$cli" render "$old" --output svl_fb.wav --seconds 300 --rate "$rate" 2>/dev/null) || fail "fallback render failed"
[ "$(field "$out" replayer)" = "sunvox" ] || fail "no library: replayer=$(field "$out" replayer), want sunvox (BSD fallback)"
out=$(SELECTA_PLUGIN_PATH="$PWD/$iso" SELECTA_SUNVOX_LIB= "$cli" render "$modern" --output svl_fb2.wav --seconds 5 --rate "$rate" 2>/dev/null)
rc=$?
[ "$rc" -lt 128 ] || fail "modern song without library crashed the host (rc=$rc)"
case "$(field "$out" replayer)" in sunvox | sunvox_lib) fail "modern song accepted without the library" ;; esac
echo sunvox_lib_fallback_ok
