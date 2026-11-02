#!/bin/sh
# MPS-2: SID voice mute through the engine top level (`selecta-cli render --script` sends MUTE / SOLO commands).
#  - MUTE 1 on reaches the replayer's RP_MUTE exactly once (sentinel line rp_mute_calls=1),
#  - the muted render differs from the unmuted one and carries less energy (the fixture's tone is on voice 1),
#  - MUTE 1 on then MUTE 1 off leaves the mask clear: the render equals the unmuted golden hash again,
#  - SOLO 2 mutes every other channel (low nibble of mute_mask is d; a script runs before the first slot is active,
#    so the engine has no voice_count yet and mutes all 32 bits).
# usage: sid_mute.sh <selecta-cli> <sid-file>
set -u
cli=$1
sid=$2
field() { printf '%s\n' "$1" | tr ' ' '\n' | sed -n "s/^$2=//p" | head -1; }
fail() { echo "FAIL: $*"; exit 1; }
printf 'MUTE 1 on\n' >mute.sel
printf 'MUTE 1 on\nMUTE 1 off\n' >roundtrip.sel
printf 'SOLO 2\n' >solo.sel
run() { # tag script-or-empty
    rm -f "$1.wav"
    if [ -n "$2" ]; then
        "$cli" render "$sid" --output "$1.wav" --seconds 4 --rate 44100 --script "$2" 2>/dev/null
    else
        "$cli" render "$sid" --output "$1.wav" --seconds 4 --rate 44100 2>/dev/null
    fi
}
out_plain=$(run plain "") || fail "plain render"
out_mute=$(run mute mute.sel) || fail "mute render"
out_rt=$(run rt roundtrip.sel) || fail "roundtrip render"
out_solo=$(run solo solo.sel) || fail "solo render"
[ -z "$(field "$out_plain" rp_mute_calls)" ] || fail "plain render called RP_MUTE"
[ "$(field "$out_mute" rp_mute_calls)" = "1" ] || fail "MUTE 1 on: rp_mute_calls=$(field "$out_mute" rp_mute_calls), want 1"
m=$(field "$out_solo" mute_mask)
case $m in 0x*d) ;; *) fail "SOLO 2: mute_mask=$m, want low nibble d" ;; esac
h() { shasum "$1.wav" | cut -d' ' -f1; }
[ "$(h plain)" != "$(h mute)" ] || fail "muting voice 1 did not change the render"
[ "$(h plain)" != "$(h solo)" ] || fail "SOLO 2 (the tone voice silenced) did not change the render"
rms() { od -An -v -t d2 -j 44 "$1" | tr -s " " "\n" | awk '$1 != "" { d = $1 - p; p = $1; s += d * d } END { printf "%.0f", s }'; }
# energy of the first difference: a muted SID leaves a constant offset, which is not sound
ep=$(rms plain.wav)
em=$(rms mute.wav)
[ "$ep" -gt 0 ] || fail "unmuted render is silent"
[ "$em" -lt "$ep" ] || fail "muting voice 1 did not lower the movement ($em vs $ep)"
[ "$(h plain)" = "$(h rt)" ] || fail "mute then unmute does not restore the unmuted render"
echo sid_mute_ok
