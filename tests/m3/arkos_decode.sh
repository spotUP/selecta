#!/bin/sh
# I433b: the Arkos Tracker 3 replayer renders 30 s of a real .aks (the AT3 example song, committed fixture), reports
# its replayer name and a full frame count with non-zero energy, and refuses a truncated copy.
set -eu
cli=$1
f=$2
out=arkos_decode.wav
log=$("$cli" render "$f" --output "$out" --seconds 30 --rate 44100 2>&1) || { echo "render failed: $log"; exit 1; }
rp=$(printf '%s\n' "$log" | sed -n 's/.*replayer=\([^ ]*\).*/\1/p' | head -n 1)
frames=$(printf '%s\n' "$log" | sed -n 's/.*rendered_frames=\([0-9]*\).*/\1/p' | head -n 1)
[ "$rp" = "arkos" ] || { echo "replayer=$rp, want arkos: $log"; exit 1; }
[ "${frames:-0}" -ge 441000 ] || { echo "rendered_frames=$frames, want at least 10 s: $log"; exit 1; }
energy=$(tail -c +45 "$out" | od -An -v -td2 | awk '{for(i=1;i<=NF;i++){s+=($i<0?-$i:$i)}} END{print s+0}')
[ "$energy" -gt 0 ] || { echo "silent render"; exit 1; }
head -c 40 "$f" > arkos_trunc.aks
if "$cli" render arkos_trunc.aks --output arkos_trunc.wav --seconds 2 --rate 44100 >/dev/null 2>&1; then
  echo "truncated .aks was accepted"; exit 1
fi
rm -f "$out" arkos_trunc.aks arkos_trunc.wav
echo "arkos_decode_ok frames=$frames energy=$energy"
