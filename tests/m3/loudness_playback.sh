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

# per-song gain at a song switch (I189a): two songs with different stored gains play gapless in one engine; every
# frame of each song, the first of the second included, is its own plain samples times its own stored gain.
cp "$song" lp_b.mod
"$cli" render "$song" --output lp_x.wav --seconds 1 --loudness track --db lp2.db --gain-cdb -600 > /dev/null 2>&1 || fail "seed A"
"$cli" render lp_b.mod --output lp_x.wav --seconds 1 --loudness track --db lp2.db --gain-cdb -1200 > /dev/null 2>&1 || fail "seed B"

# the next song is prepared in the pump between renders, so render in blocks; 8192-frame blocks put the first
# non-silent frames of song B (frame 4450) in the block right after the switch
{
    printf 'OPTION max_song_ms 1000\nOPTION fade_ms 0\nQUEUE_ADD %s\nQUEUE_ADD lp_b.mod\n@dump lp_two.raw\nPLAY\n' "$song"
    n=0; while [ $n -lt 11 ]; do echo "@render 8192"; n=$((n + 1)); done
    printf '@dump off\n'
} > lp_two.sel
"$cli" script lp_two.sel --db lp2.db > lp_two.out 2>&1 || fail "gapless script failed"
ga=$(awk 'BEGIN { printf "%d", 65536 * exp(log(10) * -600 / 2000) + 0.5 }')
gb=$(awk 'BEGIN { printf "%d", 65536 * exp(log(10) * -1200 / 2000) + 0.5 }')
samples lp_plain.wav | head -n 88200 > lp_one.txt
od -An -v -t d2 lp_two.raw | tr -s " " "\n" | sed "/^$/d" > lp_two.txt
[ "$(wc -l < lp_two.txt)" -ge 176400 ] || fail "gapless dump too short: $(wc -l < lp_two.txt) samples"
# song A is samples 1..88200 (44100 frames), song B follows and must carry gain B from its first frame
awk -v ga="$ga" -v gb="$gb" '
    function sc(p, g,   x) { x = p * g; return x < 0 ? -int((-x + 32768) / 65536) : int((x + 32768) / 65536) }
    FNR == NR { plain[FNR] = $1 + 0; next }
    { i = FNR; if (i > 176400) exit
      if (i <= 88200) { want = sc(plain[i], ga) } else { want = sc(plain[i - 88200], gb) }
      if ($1 + 0 != want) { if (!bad) first = i; bad++ } }
    END { if (bad > 0) { print "switch mismatches=" bad " first_sample=" first; exit 1 } }' lp_one.txt lp_two.txt || fail "song B is not scaled by its own gain from its first frame, or song A by its own"
rm -f lp_b.mod lp_x.wav lp_two.sel lp_two.out lp_two.raw lp2.db lp2.db-wal lp2.db-shm

rm -f lp_*.wav lp_*.txt lp.db lp.db-wal lp.db-shm
echo loudness_playback_ok
