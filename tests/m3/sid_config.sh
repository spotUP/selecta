#!/bin/sh
# MPS-1: the host SID configuration reaches the chip. One reachability test through `selecta-cli render`:
#  - sid.model=6581 and sid.model=8580 render the same tune to two different WAV hashes,
#  - the sentinel line `sid.cfg_applied=1` appears once per start,
#  - an RSID tune whose music is a BASIC program renders non-silent (RMS > 0) with the embedded OpenROMs BASIC.
# usage: sid_config.sh <selecta-cli> <sid-file> <basic-sid-file>
set -u
cli=$1
sid=$2
basic=$3
field() { printf '%s\n' "$1" | tr ' ' '\n' | sed -n "s/^$2=//p" | head -1; }
fail() { echo "FAIL: $*"; exit 1; }
rms() { od -An -v -t d2 -j 44 "$1" | tr -s " " "\n" | awk '$1 != "" { s += $1 * $1 } END { printf "%d", s }'; }
for m in 6581 8580; do
    rm -f "cfg_$m.wav"
    out=$("$cli" render "$sid" --output "cfg_$m.wav" --seconds 4 --rate 44100 --rp-config "sid.model=$m" 2>/dev/null) ||
        fail "render $m failed"
    [ "$(field "$out" sid.cfg_applied)" = "1" ] || fail "model $m: sid.cfg_applied=$(field "$out" sid.cfg_applied), want 1"
done
h1=$(shasum cfg_6581.wav | cut -d' ' -f1)
h2=$(shasum cfg_8580.wav | cut -d' ' -f1)
[ "$h1" != "$h2" ] || fail "6581 and 8580 rendered identical audio ($h1)"
rm -f cfg_basic.wav
out=$("$cli" render "$basic" --output cfg_basic.wav --seconds 4 --rate 44100 2>/dev/null) || fail "basic render failed"
e=$(rms cfg_basic.wav)
[ "$e" -gt 0 ] || fail "BASIC tune is silent with the embedded ROMs (energy $e)"
echo sid_config_ok
