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

rm -f lp_*.wav lp_*.txt lp_*.raw lp_*.sel lp.db lp.db-wal lp.db-shm lp3.db* lp4.db*
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

# headroom (I189b): +6 dB on a source whose peak is near full scale puts the boosted signal about 3 dB past
# full scale. The gain must reach the limiter unclipped, so the output is exactly the limiter model applied to
# plain * gain in wide arithmetic (a clip at 16 bits before the limiter changes the waveform), the limiter
# engages, and no output sample sits at the 16-bit rails.
peak=$(awk '{ v = $1 < 0 ? -$1 : $1; if (v > m) m = v } END { print m + 0 }' lp_plain.txt)
[ "$peak" -ge 16384 ] || fail "plain peak $peak too low for the boost case"
boost=$(render lp_boost.wav --loudness track --gain-cdb 600)
bq16=$(field "$boost" gain_q16)
awk -v p="$peak" -v g="$bq16" 'BEGIN { exit !(p * g / 65536 > 32767) }' || fail "boost $bq16 does not push peak $peak past full scale"
[ "$(field "$boost" limiter_hits)" -gt 0 ] || fail "limiter never engaged on the +6 dB render"
samples lp_boost.wav > lp_boost.txt
paste lp_plain.txt lp_boost.txt | awk -v g="$bq16" '
    BEGIN { lim = 65536; ceil = 29203 }
    function fl(x) { return x < 0 ? -int((-x + 65535) / 65536) : int(x / 65536) }  # floor(x / 65536)
    function clamp(v) { return v > 32767 ? 32767 : (v < -32768 ? -32768 : v) }
    { n++
      if (n % 2 == 1) { l = fl($1 * g + 32768); lw = $2 + 0; next }
      r = fl($1 * g + 32768); rw = $2 + 0
      al = l < 0 ? -l : l; ar = r < 0 ? -r : r; m = al > ar ? al : ar
      if (lim < 65536) { lim += int((65536 - lim) / 1024) + 1; if (lim > 65536) lim = 65536 }
      if (m * lim > ceil * 65536) { lim = int(ceil * 65536 / m) }
      if (clamp(fl(l * lim)) != lw || clamp(fl(r * lim)) != rw) { if (!bad) first = n / 2; bad++ }
      a = lw < 0 ? -lw : lw; b = rw < 0 ? -rw : rw
      if (a >= 32767 || b >= 32767) rail++
    }
    END { if (bad > 0 || rail > 0) { print "boost mismatches=" bad + 0 " first_frame=" first " rail=" rail + 0; exit 1 } }' || fail "boosted output is not the limiter applied to the unclipped plain * gain"
echo "boost_peak=$peak boost_hits=$(field "$boost" limiter_hits)"

# crossfade (I189b): the same two-song queue, plain and with stored gains. Outside the overlap each song is its own
# plain samples times its own gain, within 1 LSB (rounding at exact ties differs between the awk model and the engine); with the same gain on both songs the
# overlap is the plain crossfade times that gain, so the gain acts before the equal-power mix.
cp "$song" lp_b.mod
cat > lp_xf.sel <<EOS
OPTION max_song_ms 3000
OPTION fade_ms 1000
OPTION crossfade 1000
QUEUE_ADD SONGPATH
QUEUE_ADD lp_b.mod
@dump DUMP
PLAY
@render_until_end
@render_until_end
@stats
@dump off
EOS
xf() { # <dump> <db args...>
    d=$1; shift
    sed -e "s/DUMP/$d/" -e "s#SONGPATH#$song#" lp_xf.sel > lp_xf_run.sel
    "$cli" script lp_xf_run.sel "$@" 2>&1 | sed -n 's/^STATS xfade_frames=\([0-9]*\) .*/\1/p'
}
seed() { # <db> <song> <cdb>
    "$cli" render "$2" --output lp_x.wav --seconds 1 --loudness track --db "$1" --gain-cdb "$3" > /dev/null 2>&1 || fail "seed $2 $3"
}
xf_plain=$(xf lp_xf_plain.raw)
[ "${xf_plain:-0}" -gt 20000 ] || fail "plain crossfade did not overlap: xfade_frames=$xf_plain"
seed lp3.db "$song" -600
seed lp3.db lp_b.mod -1200
xf_diff=$(xf lp_xf_diff.raw --db lp3.db)
[ "$xf_diff" = "$xf_plain" ] || fail "xfade_frames with gains $xf_diff differs from plain $xf_plain"
seed lp4.db "$song" -600
seed lp4.db lp_b.mod -600
xf_same=$(xf lp_xf_same.raw --db lp4.db)
[ "$xf_same" = "$xf_plain" ] || fail "xfade_frames with equal gains $xf_same differs from plain $xf_plain"
for k in plain diff same; do od -An -v -t d2 lp_xf_$k.raw | tr -s " " "\n" | sed "/^$/d" > lp_xf_$k.txt; done
awk -v ga="$ga" -v gb="$gb" -v xf="$xf_plain" '
    function sc(p, g,   x) { x = p * g; return x < 0 ? -int((-x + 32768) / 65536) : int((x + 32768) / 65536) }
    function ab(v) { return v < 0 ? -v : v }
    FILENAME == "lp_xf_plain.txt" { plain[FNR] = $1 + 0; np = FNR; next }
    FILENAME == "lp_xf_diff.txt" { d[FNR] = $1 + 0; nd = FNR; next }
    { s[FNR] = $1 + 0; ns = FNR }
    END {
        if (np != nd || np != ns || np < 200000) { print "dump lengths " np " " nd " " ns; exit 1 }
        for (i = 1; i <= np; i++) { if (ab(d[i] - sc(plain[i], ga)) > 1) { s0 = i; break } }
        for (i = np; i >= 1; i--) { if (ab(d[i] - sc(plain[i], gb)) > 1) { s1 = i; break } }
        fr0 = int((s0 + 1) / 2); fr1 = int((s1 + 1) / 2)
        if (s0 == 0 || s1 == 0 || fr0 < 20000 || fr1 >= np / 2 - 1000) { print "no leading/trailing exact segment s0=" s0 " s1=" s1; exit 1 }
        len = fr1 - fr0 + 1
        if (len > xf || xf - len > 6000) { print "overlap " len " frames, stats say " xf; exit 1 }
        for (i = 1; i <= np; i++) { w = sc(plain[i], ga); if (ab(s[i] - w) > 2) { bad++; if (!first) first = i } }
        if (bad > 0) { print "equal-gain overlap mismatches=" bad " first=" first; exit 1 }
        print "xf_overlap=" len " lead_frames=" fr0 }' lp_xf_plain.txt lp_xf_diff.txt lp_xf_same.txt || fail "crossfade gain at the switch is wrong"
rm -f lp_b.mod lp_x.wav lp_xf.sel lp_xf_run.sel lp_xf_*.raw lp_xf_*.txt lp3.db* lp4.db*

rm -f lp_*.wav lp_*.txt lp_*.raw lp_*.sel lp.db lp.db-wal lp.db-shm lp3.db* lp4.db*
echo loudness_playback_ok
