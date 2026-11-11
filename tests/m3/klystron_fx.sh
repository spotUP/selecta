#!/bin/sh
# I1312: a klystrack song's own effects (reverb, chorus, crush) reach the klystron engine through `selecta-cli render`.
# The adapter must copy the song's fx units into the engine (mus_set_fx) before the song starts, as klystron's ksnd.c
# KSND_PlaySong does; without it every song plays dry. ringmod.kt (format v6, fetched by checksum, never committed) turns
# reverb on in its song flags and carries 8 reverb taps after the title. The test writes a copy into the work dir with
# all 8 tap gains set to -1000 (below CYDRVB_LOW_LIMIT, i.e. silent taps; layout unchanged) and renders both: the PCM
# must differ, so the song's reverb settings move the output. Without mus_set_fx both render the same dry PCM.
# Sentinel: the `replayer=klystron` line of render. Exit 77 = skipped (no network).
# usage: klystron_fx.sh <selecta-cli> [local ringmod.kt instead of the fetched one]
set -u
cli=$1
here=$(dirname "$0")
f=${2:-}
field() { printf '%s\n' "$1" | tr ' ' '\n' | sed -n "s/^$2=//p" | head -1; }
fail() { echo "FAIL: $*"; exit 1; }
if [ -z "$f" ]; then
  f=$("$here/fetch_fixture.sh" https://raw.githubusercontent.com/kometbomb/klystrack/master/examples/songs/ringmod.kt \
        b79cb551c216338b873b992a29772b9618f43c8ec21c017eb947a1ed124ba9f1 ringmod.kt) || exit $?
fi
# v6 layout: flags (Uint32) at 0x1e, 17-byte title, then 8 x (Sint32 gain, Sint32 delay) taps from 0x33.
[ "$(od -An -t u1 -j 30 -N 1 "$f" | tr -d ' ')" = "1" ] || fail "$f: song flags do not enable reverb"
rm -f fx_dry.kt fx_on.wav fx_dry.wav
cp "$f" fx_dry.kt || fail "copy failed"
i=0
while [ $i -lt 8 ]; do
  printf '\030\374\377\377' | dd of=fx_dry.kt bs=1 seek=$((51 + 8 * i)) conv=notrunc 2>/dev/null || fail "patch failed"
  i=$((i + 1))
done
for v in on dry; do
  src=$f
  [ $v = dry ] && src=fx_dry.kt
  out=$("$cli" render "$src" --output "fx_$v.wav" --seconds 12 --rate 44100 2>/dev/null) || fail "render failed: $src"
  [ "$(field "$out" replayer)" = "klystron" ] || fail "$src: replayer=$(field "$out" replayer), want klystron"
done
cmp -s fx_on.wav fx_dry.wav && fail "song reverb does not change the PCM (song fx never reach the engine)"
rm -f fx_dry.kt fx_on.wav fx_dry.wav
echo "klystron_fx_ok"
