#!/bin/sh
# I1242d1: Ben Daglish SID (.bds, nuade player bendaglishsid) renders non-silent through `selecta-cli render`. The driver sets a channel's
# volume only when it sees a loop wrap or queued-sample switch in the voice state (bds_driver.c collect_voice_starts); before the fix the
# host voices shim reported wraps=0 for every voice, so every .bds rendered silent. The song (Modland, no stated redistribution licence) is
# fetched by checksum into the git-ignored fixture cache (fixtures/nuade_bds.SOURCE), never committed. Exit 77 = skipped (no network).
# Asserts: the nuade player ran (voice_ops>0) and the WAV is not silent (BDS_PEAK_MIN raises the floor, for red proofs).
# usage: nuade_bds_voices.sh <selecta-cli>
set -u
cli=$1
here=$(dirname "$0")
field() { printf '%s\n' "$1" | tr ' ' '\n' | sed -n "s/^$2=//p" | head -1; }
fail() { echo "FAIL: $*"; exit 1; }
peak() { od -An -v -t d2 -j 44 "$1" | tr -s " " "\n" | awk '$1 != "" { a = $1 < 0 ? -$1 : $1; if (a > m) m = a } END { printf "%d", m }'; }
song=$("$here/fetch_fixture.sh" "https://modland.com/pub/modules/Ben%20Daglish%20SID/Ben%20Daglish/terramex.bds" \
  bd99a9ba28655b6a599a8fbb990f6c369bed8875239787b2499d080015935aa9 terramex.bds) || exit $?
rm -f bds.wav
out=$("$cli" render "$song" --output bds.wav --seconds 6 --rate 44100 2>/dev/null) || fail "render failed: $song"
[ "$(field "$out" voice_ops)" -gt 0 ] 2>/dev/null || fail "voice_ops=$(field "$out" voice_ops), the nuade player did not run"
p=$(peak bds.wav)
[ "$p" -gt "${BDS_PEAK_MIN:-1000}" ] || fail "silent output (peak $p)"
echo "peak=$p voice_ops=$(field "$out" voice_ops)"
rm -f bds.wav
echo nuade_bds_voices_ok
