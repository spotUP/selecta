#!/bin/sh
# I710aw: EdLib D01 songs (Modland "EdLib D01/JCH": JCH header, type 0x80, version 1 song) play through `selecta-cli render`
# (adplug d00 player, a RENDER-kind replayer: sentinel replayer=adplug). Before the fix the adapter did not list "d01" and the d00
# loader rejected the header (type != 0, version 1, extension not .d00), so the files gave no_replayer. The songs have no stated
# redistribution licence: fetched by checksum from Modland into the git-ignored cache (fixtures/adplug_edlib_d01.SOURCE), never
# committed. Exit 77 = skipped (no network).
# Asserts per song: replayer=adplug, WAV not silent (D01_ENERGY_MIN raises the floor, for red proofs), rendered_frames bound to the
# measured length (D01_MIN_SECONDS replaces the floor, for red proofs).
# Measured: gorgeous.d01 3877020 frames (87.9 s), crooner.d01 3414222 frames (77.4 s), the alibi.d01 subsong 1 8230950 frames (186.6 s)
# at 44100 Hz; bounds +-1 s. the alibi.d01 has 2 subsongs: subsong 0 is a 3150-frame stub (energy 973421), the music is subsong 1.
# Negative cases: gorgeous.d01 with the JCH magic broken, and a zero file, are not played.
# usage: adplug_edlib_d01_decode.sh <selecta-cli>
set -u
cli=$1
here=$(dirname "$0")
field() { printf '%s\n' "$1" | tr ' ' '\n' | sed -n "s/^$2=//p" | head -1; }
fail() { echo "FAIL: $*"; exit 1; }
energy() { od -An -v -t d2 -j 44 "$1" | tr -s " " "\n" | awk '$1 != "" { s += $1 * $1 } END { printf "%d", s }'; }
rate=44100
base="https://modland.com/pub/modules/Ad%20Lib/EdLib%20D01/JCH"
gor=$("$here/fetch_fixture.sh" "$base/gorgeous.d01" 0dc77187df97d6429cf89dc4113623ba94d7ebe7cdd42c5f4d35f45548c4bf36 gorgeous.d01) || exit $?
cro=$("$here/fetch_fixture.sh" "$base/crooner.d01" 9766270ed6f889f8cf9a99fe9b4d4e39b02a3f5924a7a185f447cd7b1b54a269 crooner.d01) || exit $?
ali=$("$here/fetch_fixture.sh" "$base/the%20alibi.d01" 2979e3a5a2476100bcd00ac9a1fc7cb48b553484a5f05f56f3e21d0fafb4108e the_alibi.d01) || exit $?
check() { # file min-seconds max-seconds [subsong]
    rm -f fd_d01.wav
    out=$("$cli" render "$1" --output fd_d01.wav --seconds 600 --rate "$rate" ${4:+--subsong "$4"} 2>/dev/null) || fail "render failed: $1"
    [ "$(field "$out" replayer)" = "adplug" ] || fail "$1: replayer=$(field "$out" replayer), want adplug"
    got=$(field "$out" rendered_frames)
    min=${D01_MIN_SECONDS:-$2}
    [ "$got" -ge $((rate * min)) ] || fail "$1: rendered_frames=$got, want at least $((rate * min))"
    [ "$got" -le $((rate * $3)) ] || fail "$1: rendered_frames=$got, far beyond the song length"
    e=$(energy fd_d01.wav)
    [ "$e" -gt "${D01_ENERGY_MIN:-1000000}" ] || fail "$1: silent output (energy $e)"
    echo "$1: frames=$got energy=$e"
    rm -f fd_d01.wav
}
check "$gor" 87 89
check "$cro" 77 79
check "$ali" 186 188 1
cp "$gor" badmagic.d01
printf 'XCH' | dd of=badmagic.d01 bs=1 seek=0 conv=notrunc 2>/dev/null
out=$("$cli" render badmagic.d01 --output fd_d01.wav --seconds 5 --rate "$rate" 2>/dev/null) || true
[ "$(field "$out" replayer)" = "adplug" ] && fail "a song with a broken JCH magic was played"
head -c 3000 /dev/zero > zero.d01
out=$("$cli" render zero.d01 --output fd_d01.wav --seconds 5 --rate "$rate" 2>/dev/null) || true
[ "$(field "$out" replayer)" = "adplug" ] && fail "a zero-filled .d01 file was played"
rm -f fd_d01.wav badmagic.d01 zero.d01
echo adplug_edlib_d01_decode_ok
