#!/bin/sh
# I710av: BP SoundMon 1.0 (mark "BPSM", Modland "reaching for the sky.bp", the only file of the format) plays through
# `selecta-cli render` (nuade soundmon player, a voice-kind replayer: sentinel voice_ops>0). Before the fix the probe knew only
# "V.2"/"V.3" and the file gave no_replayer. The song has no stated redistribution licence: fetched by checksum from Modland
# into the git-ignored fixture cache (tests/m3/fixtures/nuade_soundmon.SOURCE), never committed. Exit 77 = skipped (no network).
# Asserts: the nuade player ran (voice_ops>0), the WAV is not silent (SOUNDMON_ENERGY_MIN raises the floor, for red proofs) and the
# mixed frames match the measured song length (SOUNDMON_MIN_SECONDS replaces the floor, for red proofs).
# Measured: 6633522 frames at 44100 Hz = 150.4 seconds (voice_ops=122044); bounds 149..152 seconds.
# Negative cases: the song with its mark changed to "BPSX" and a zero file are not played.
# usage: nuade_soundmon_decode.sh <selecta-cli>
set -u
cli=$1
here=$(dirname "$0")
field() { printf '%s\n' "$1" | tr ' ' '\n' | sed -n "s/^$2=//p" | head -1; }
fail() { echo "FAIL: $*"; exit 1; }
energy() { od -An -v -t d2 -j 44 "$1" | tr -s " " "\n" | awk '$1 != "" { s += $1 * $1 } END { printf "%d", s }'; }
rate=44100
song=$("$here/fetch_fixture.sh" "https://modland.com/pub/modules/BP%20SoundMon%201/Brian%20Postma/reaching%20for%20the%20sky.bp" \
  dd86c5fdbe91d27646ff8def175bed4b31bd4961d5e240790ae8b3d0c3ec8518 reaching-for-the-sky.bp) || exit $?
rm -f fd_sm.wav
out=$("$cli" render "$song" --output fd_sm.wav --seconds 600 --rate "$rate" 2>/dev/null) || fail "render failed: $song"
[ "$(field "$out" voice_ops)" -gt 0 ] 2>/dev/null || fail "voice_ops=$(field "$out" voice_ops), the nuade player did not run"
got=$(field "$out" mixed_frames)
min=${SOUNDMON_MIN_SECONDS:-149}
[ "$got" -ge $((rate * min)) ] || fail "mixed_frames=$got, want at least $((rate * min))"
[ "$got" -le $((rate * 152)) ] || fail "mixed_frames=$got, far beyond the song length"
e=$(energy fd_sm.wav)
[ "$e" -gt "${SOUNDMON_ENERGY_MIN:-1000000}" ] || fail "silent output (energy $e)"
echo "frames=$got energy=$e"
rm -f fd_sm.wav
cp "$song" badmark.bp
printf 'BPSX' | dd of=badmark.bp bs=1 seek=26 conv=notrunc 2>/dev/null
out=$("$cli" render badmark.bp --output fd_sm.wav --seconds 5 --rate "$rate" 2>/dev/null) || true
[ "$(field "$out" voice_ops)" -gt 0 ] 2>/dev/null && fail "a song with the mark BPSX was played"
head -c 20000 /dev/zero > zero.bp
out=$("$cli" render zero.bp --output fd_sm.wav --seconds 5 --rate "$rate" 2>/dev/null) || true
[ "$(field "$out" voice_ops)" -gt 0 ] 2>/dev/null && fail "a zero-filled .bp file was played"
rm -f fd_sm.wav badmark.bp zero.bp
echo nuade_soundmon_decode_ok
