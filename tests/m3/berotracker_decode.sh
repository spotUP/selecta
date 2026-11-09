#!/bin/sh
# I710b/I710ba: BeRoTracker songs (.brt) play through `selecta-cli render` as a RENDER-kind replayer (bbmp_berotracker module, GPL-3.0-or-later
# bbmp csrc/berotracker clean-room engine). All 19 BeRo songs on Modland. Fetched by checksum, never committed
# (tests/m3/fixtures/bbmp_berotracker.SOURCE). Exit 77 = skipped (no network). Sentinel: the `replayer=bbmp_berotracker` line of
# render. Asserts per song: the plugin ran, the WAV is not silent (BRT_ENERGY_MIN raises the floor, for red proofs) and the
# rendered frames match the song length the engine measures (BRT_MIN_SECONDS, when set, replaces the floor, for red proofs).
# Also: the adapter renders incrementally (I710ba 2): one second of the 385 s song returns within BRT_ONE_SECOND_MAX seconds of wall
# time (default 15; a whole-song render at start takes about 45 s), and a file that is not a BeRoTracker song is not claimed.
# Bit-exactness against the x87 original is not testable on arm64 (I710ba 1 exemption): the check is song length plus energy.
# usage: berotracker_decode.sh <selecta-cli>
set -u
cli=$1
here=$(dirname "$0")
M=https://modland.com/pub/modules/BeRoTracker/BeRo
field() { printf '%s\n' "$1" | tr ' ' '\n' | sed -n "s/^$2=//p" | head -1; }
fail() { echo "FAIL: $*"; exit 1; }
energy() { od -An -v -t d2 -j 44 "$1" | tr -s " " "\n" | awk '$1 != "" { s += $1 * $1 } END { printf "%d", s }'; }
rate=44100
fetch() { "$here/fetch_fixture.sh" "$1" "$2" "$3" || exit $?; }
check() { # <song path> <min seconds> <max seconds>
  rm -f fd_berotracker.wav
  out=$("$cli" render "$1" --output fd_berotracker.wav --seconds 900 --rate "$rate" 2>/dev/null) || fail "render failed: $1"
  [ "$(field "$out" replayer)" = "bbmp_berotracker" ] || fail "$1: replayer=$(field "$out" replayer), want bbmp_berotracker"
  got=$(field "$out" rendered_frames)
  min=${BRT_MIN_SECONDS:-$2}
  [ "$got" -ge $((rate * min)) ] || fail "$1: rendered_frames=$got, want at least $((rate * min))"
  [ "$got" -le $((rate * $3)) ] || fail "$1: rendered_frames=$got, far beyond the song length"
  e=$(energy fd_berotracker.wav)
  [ "$e" -gt "${BRT_ENERGY_MIN:-1000000}" ] || fail "$1: silent output (energy $e)"
  echo "$1 frames=$got energy=$e"
  rm -f fd_berotracker.wav
}
# url name, sha256, cache name, min seconds, max seconds (measured with the engine at 44100 Hz)
songs="aa2s.brt ae3917b011bafbac1d486bcaa47e76a107a01ed8871a62ef573b640289980da7 aa2s.brt 7 8
b2.brt c5097a3e2964c2b81f115a23b6a8b6fec9a6bd5107b2df85a650a2bcfc75a4be b2.brt 6 7
bbb.brt 43b750ce76371b16b788ac08d1de8471f4dc9360b93ae68ab1c9665a8bf6dd74 bbb.brt 6 7
ddds.brt 09e021ab28d959c176c56dfcc58921f49e62c7aabe8f815351ceb1522ed61ea3 ddds.brt 15 16
dirty%20trip%20%28excompo%20version%29.brt 52c0461bebc6c9cbc75880acc08ad3a45db1dd98c578b1a82aeff56286c83b37 dirty_trip__excompo_version_.brt 282 283
drum%27n%27chip.brt b55fc562c4eb4c028735bdc3d57e78b1ce33da907b5c5b07b1b2d2a1dfb3fab0 drum_n_chip.brt 229 230
drum%27n%27dream.brt 5382325a43dcac0bc2ceb989577dca9b8467b621c30967b12d2ebd44e50f92a2 drum_n_dream.brt 385 386
drumbass.brt 5c617bbd19da5589372e6d10e433114d52c8c703ef431cdabe41563f04916f10 drumbass.brt 192 193
ee.brt c2a7fe6f468767dd7d688e3d14e79b552c0f4033e27c2e4452f8e480691173ea ee.brt 59 60
es.brt 6e7d2c27b7ff6f8857fdc6a175127213254ba7963fdc474a25e9bf8125c7da11 es.brt 7 8
fly%20into%20your%20dreams.brt 999824a16d8b4f08639c3b7d05e6c1982e711c7bc33e831a4d2cce6ae17399ae fly_into_your_dreams.brt 348 349
heyduda.brt 538f19fd9a0a81f3f6a2f0d5cbd7208a61a541fad156dcd0db89c4fa4a919bb2 heyduda.brt 22 23
heyduda2.brt abd90d16df24ee614b5ef0a4d84d526b66e4e1187e95e896cac773104d975275 heyduda2.brt 44 45
heydudaa.brt 23b75096307f319dbb7fba64a4d78a8779a62a4c2b83b999a9978d7c236aadaa heydudaa.brt 44 45
randomizer.brt 1371123ae40c0986cd250c901c8e3c3b103604b5355366ac6df4e7d2afe7d1ad randomizer.brt 251 252
reloaded%20dreams.brt 9de251bc16832148f4b16bea041fb1a31f78f91c980117c1f52bc8a03774bbd0 reloaded_dreams.brt 206 207
ssdx.brt e4b0c3fa3de2f9dca593cd2d8db8fbb78977aeefaaa7a2062edc8f8501796844 ssdx.brt 6 7
sss.brt 152b9e5a7570cb7986cf85921ac50b4a723fc0d6c8b609a4a96012f3c9c46363 sss.brt 6 7
wonderful%20dream%20landscapes.brt 47f537a1ab733af67cdbbca0f9b2bf60407f7ea5d655dcababa8da5f6e5545b9 wonderful_dream_landscapes.brt 368 369"
# Fetch all first, so a missing network skips before any render.
paths=""
oldifs=$IFS
IFS='
'
for row in $songs; do
  IFS=$oldifs
  set -- $row
  p=$(fetch "$M/$1" "$2" "$3") || exit $?
  paths="$paths$p $4 $5
"
  IFS='
'
done
IFS=$oldifs
# Incremental render (I710ba 2): one second of the longest song must not pay for the whole song.
long=$(printf '%s' "$paths" | grep 'drum_n_dream' | cut -d' ' -f1)
t0=$(date +%s)
out=$("$cli" render "$long" --output fd_berotracker.wav --seconds 1 --rate "$rate" 2>/dev/null) || fail "one-second render failed: $long"
t1=$(date +%s)
[ "$(field "$out" replayer)" = "bbmp_berotracker" ] || fail "one-second render: replayer=$(field "$out" replayer)"
[ "$(field "$out" rendered_frames)" = "$rate" ] || fail "one-second render: rendered_frames=$(field "$out" rendered_frames), want $rate"
[ $((t1 - t0)) -le "${BRT_ONE_SECOND_MAX:-15}" ] || fail "one second of the 385 s song took $((t1 - t0)) s: the adapter renders the whole song up front"
echo "one second of drum_n_dream in $((t1 - t0)) s"
rm -f fd_berotracker.wav
IFS='
'
for row in $paths; do
  IFS=$oldifs
  set -- $row
  check "$1" "$2" "$3"
  IFS='
'
done
IFS=$oldifs
# A file that is not a BeRoTracker song is not claimed, whatever its name says.
head -c 70000 /dev/zero | tr '\0' 'A' > notbrt.brt
out=$("$cli" render notbrt.brt --output fd_berotracker.wav --seconds 1 --rate "$rate" 2>/dev/null) || true
[ "$(field "$out" replayer)" != "bbmp_berotracker" ] || fail "bbmp_berotracker claimed a file that is not a BeRoTracker song"
rm -f notbrt.brt fd_berotracker.wav
echo berotracker_decode_ok
