#!/bin/sh
# T89 loudness-analysis driver (I187): R128 integrated loudness and the RMS+peak fallback on generated
# reference signals, the ANALYSE command on a real song (faster than real time, gain stored per song key),
# and `library analyse source:local` over a scanned folder.
# usage: loudness_analysis.sh <selecta-cli> <song.mod>
set -u
cli=$1
song=$2
field() { printf '%s\n' "$1" | tr ' ' '\n' | sed -n "s/^$2=//p" | head -1; }
near() { awk -v a="$1" -v b="$2" -v t="$3" 'BEGIN { d = a - b; if (d < 0) d = -d; exit !(d <= t) }'; }

for db in -23 -33; do
    out=$("$cli" analyse --sine "1000:$db") || { echo "sine $db failed: $out"; exit 1; }
    lufs=$(field "$out" lufs)
    rms=$(field "$out" rms_db)
    speed=$(field "$out" analyse_speed)
    near "$lufs" "$db" 0.1 || { echo "sine $db dBFS: $lufs LUFS, want $db +-0.1"; exit 1; }
    near "$rms" "$db" 1.0 || { echo "sine $db dBFS: fallback rms $rms dB, want $db +-1"; exit 1; }
    [ "$speed" -ge 10 ] || { echo "sine analyse_speed=$speed, want >= 10"; exit 1; }
    echo "sine_${db}_lufs=$lufs fallback_rms=$rms"
done

rm -f loudness.db
out=$("$cli" analyse --db loudness.db "$song") || { echo "analyse song failed: $out"; exit 1; }
printf '%s\n' "$out"
jobs=$(field "$out" analyse_jobs)
speed=$(field "$out" analyse_speed)
[ "$jobs" = 1 ] || { echo "analyse_jobs=$jobs, want 1"; exit 1; }
[ "$speed" -ge 10 ] || { echo "analyse_speed=$speed, want >= 10"; exit 1; }
[ "$(field "$out" stored)" = 1 ] || { echo "gain not stored per song key"; exit 1; }
[ "$(field "$out" stored_gain_cdb)" = "$(field "$out" gain_cdb)" ] || { echo "stored gain differs from the measured gain"; exit 1; }
rm -f loudness.db

rm -rf loudness_dir loudness_lib.db
mkdir loudness_dir
cp "$song" loudness_dir/a.mod
"$cli" library scan --db loudness_lib.db loudness_dir > /dev/null || { echo "scan failed"; exit 1; }
out=$("$cli" library analyse --db loudness_lib.db 'source:local') || { echo "library analyse failed: $out"; exit 1; }
printf '%s\n' "$out"
[ "$(field "$out" analysed)" = 1 ] || { echo "library analyse did not analyse 1 song"; exit 1; }
rm -rf loudness_dir loudness_lib.db
echo "analyse_jobs=$jobs analyse_speed=$speed"
echo loudness_analysis_ok
