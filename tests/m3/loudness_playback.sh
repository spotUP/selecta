#!/bin/sh
# T90 gain and limiter part (I189, D-M3-9): `selecta-cli render` applies the stored per-song gain (16.16) and the
# -1 dBFS limiter. One reachability test through the product entry point; the sentinel is loudness_frames.
# usage: loudness_playback.sh <selecta-cli> <song.mod>
set -u
cli=$1
song=$2
field() { printf '%s\n' "$1" | tr ' ' '\n' | sed -n "s/^$2=//p" | head -1; }
fail() { echo "FAIL: $*"; exit 1; }
render() { # <wav> <args...>
    wav=$1; shift
    "$cli" render "$song" --output "$wav" --seconds 2 "$@" 2>/dev/null || fail "render $* failed"
}
samples() { od -An -v -t d2 -j 44 "$1" | tr -s " " "\n" | sed "/^$/d"; }

rm -f lp_*.wav lp_*.txt lp.db lp.db-wal lp.db-shm
render lp_plain.wav > /dev/null
off=$(render lp_off.wav --loudness off --gain-cdb -600)
out=$(render lp_track.wav --loudness track --gain-cdb -600)

[ "$(field "$off" loudness_source)" = off ] || fail "off: loudness_source=$(field "$off" loudness_source)"
[ "$(field "$off" loudness_frames)" = 0 ] || fail "off ran the gain stage"
[ "$(cksum < lp_off.wav)" = "$(cksum < lp_plain.wav)" ] || fail "off hash differs from the plain render"
[ "$(cksum < lp_track.wav)" != "$(cksum < lp_plain.wav)" ] || fail "track hash equals the plain render"

[ "$(field "$out" loudness_source)" = stored ] || fail "track: loudness_source=$(field "$out" loudness_source)"
q16=$(field "$out" gain_q16)
want=$(awk 'BEGIN { printf "%d", 65536 * exp(log(10) * -600 / 2000) + 0.5 }')
awk -v a="$q16" -v b="$want" 'BEGIN { d = a - b; if (d < 0) d = -d; exit !(d <= 1) }' || fail "gain_q16=$q16 want $want +-1"
[ "$(field "$out" loudness_frames)" -ge 44100 ] || fail "sentinel loudness_frames=$(field "$out" loudness_frames)"
[ "$(field "$out" limiter_hits)" = 0 ] || fail "limiter engaged on an attenuated render"

# every sample is the plain sample times the stored gain, rounded in 16.16
samples lp_plain.wav > lp_plain.txt
samples lp_track.wav > lp_track.txt
paste lp_plain.txt lp_track.txt | awk -v g="$q16" '
    { p = $1 + 0; t = $2 + 0; x = p * g
      if (x < 0) w = -int((-x + 32768) / 65536); else w = int((x + 32768) / 65536)
      n++; if (t != w) bad++; sp += p * p }
    END { if (n == 0 || bad > 0 || sp == 0) { print "samples=" n " mismatches=" bad + 0 " energy=" sp; exit 1 } }' || fail "track samples are not plain * gain"

# the gain comes from the library: a second render with only --db reads the stored value back
render lp_seed.wav --loudness track --db lp.db --gain-cdb -600 > /dev/null
out2=$(render lp_stored.wav --loudness track --db lp.db)
[ "$(field "$out2" gain_q16)" = "$q16" ] || fail "stored gain not read back from --db: $(field "$out2" gain_q16)"
[ "$(cksum < lp_stored.wav)" = "$(cksum < lp_track.wav)" ] || fail "stored-gain render differs from the seeded render"

# hot: +24 dB drives the signal far past full scale; the limiter keeps every sample at or below -1 dBFS
hot=$(render lp_hot.wav --loudness track --gain-cdb 2400)
[ "$(field "$hot" limiter_hits)" -gt 0 ] || fail "limiter never engaged on the hot render"
samples lp_hot.wav | awk '{ v = $1 < 0 ? -$1 : $1; if (v > m) m = v } END { print "hot_peak=" m; exit !(m <= 29203) }' || fail "hot render exceeds -1 dBFS"
echo "q16=$q16 hot_hits=$(field "$hot" limiter_hits)"
rm -f lp_*.wav lp_*.txt lp.db lp.db-wal lp.db-shm
echo loudness_playback_ok
