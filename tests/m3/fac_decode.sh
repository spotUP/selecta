#!/bin/sh
# I710ak: FAC SoundTracker (MSX .mus) songs play through `selecta-cli render` as a RENDER-kind replayer (bbmp_fac module,
# GPL-3.0-or-later bbmp csrc/fac on emu8950 / emu2413). Two real songs: 2nd Rendez-Vous (MSX-AUDIO, with its drum kit
# 2NDR-V.SM1/.SM2 beside the song) and Equinoxe IV (a second song, other kit missing). The songs have no stated
# redistribution licence, so they are fetched by checksum, never committed (tests/m3/fixtures/bbmp_fac.SOURCE).
# Exit 77 = skipped (no network). Sentinel: the `replayer=bbmp_fac` line of render. Asserts per song: the plugin ran, the
# WAV is not silent (FAC_ENERGY_MIN raises the floor for red proofs) and the rendered frames match the song length the
# engine reports (the file's own measured length, +-1 s; FAC_MIN_SECONDS, when set, replaces the floor, for red proofs).
# Also: a .mus that is not FAC (16391 bytes of a different pattern) is not claimed by bbmp_fac.
# usage: fac_decode.sh <selecta-cli>
set -u
cli=$1
here=$(dirname "$0")
M=https://modland.com/pub/modules/FAC%20SoundTracker
field() { printf '%s\n' "$1" | tr ' ' '\n' | sed -n "s/^$2=//p" | head -1; }
fail() { echo "FAIL: $*"; exit 1; }
energy() { od -An -v -t d2 -j 44 "$1" | tr -s " " "\n" | awk '$1 != "" { s += $1 * $1 } END { printf "%d", s }'; }
rate=44100
fetch() { "$here/fetch_fixture.sh" "$1" "$2" "$3" || exit $?; }
check() { # <song path> <min seconds>
  rm -f fd_fac.wav
  out=$("$cli" render "$1" --output fd_fac.wav --seconds 600 --rate "$rate" 2>/dev/null) || fail "render failed: $1"
  [ "$(field "$out" replayer)" = "bbmp_fac" ] || fail "$1: replayer=$(field "$out" replayer), want bbmp_fac"
  got=$(field "$out" rendered_frames)
  min=${FAC_MIN_SECONDS:-$2}
  [ "$got" -ge $((rate * min)) ] || fail "$1: rendered_frames=$got, want at least $((rate * min))"
  [ "$got" -le $((rate * (min + 120))) ] || fail "$1: rendered_frames=$got, far beyond the song length"
  e=$(energy fd_fac.wav)
  [ "$e" -gt "${FAC_ENERGY_MIN:-1000000}" ] || fail "$1: silent output (energy $e)"
  echo "$1 frames=$got energy=$e"
  rm -f fd_fac.wav
}
s1=$(fetch "$M/Coen%20van%20der%20Geest/fmusic%204/2nd%20Rendez-Vous.MUS" 6f54a2c717c7df2b726c370c160967d1d21a7c89ffaf5bbc15737de7435da417 2nd_rendez-vous.mus) || exit $?
fetch "$M/Coen%20van%20der%20Geest/fmusic%204/2NDR-V.SM1" a74fe2834fea22a527a29c5b27ceccf802e44fffb0c3daf13d395cc4ab3bc07c 2NDR-V.SM1 >/dev/null || exit $?
fetch "$M/Coen%20van%20der%20Geest/fmusic%204/2NDR-V.SM2" c6d15417841060f04ffd13489db5e96e6dd3e8b7e53034e3d5052c4c437703fe 2NDR-V.SM2 >/dev/null || exit $?
s2=$(fetch "$M/A.C.E%20%28J.A.%20Koets%29/Jarredisk/Equinoxe%20IV%20%281978%29.MUS" 80d54bd3d7aab7ef03e410e77dee67625a3faddcc5e4812ffbaec9c1fd991e8e equinoxe4.mus) || exit $?
check "$s1" 94
check "$s2" 178
# A .mus that is not FAC must not be claimed by bbmp_fac (it stays with whichever player claims it, or fails to open).
head -c 16391 /dev/zero | tr '\0' 'A' > notfac.mus
out=$("$cli" render notfac.mus --output fd_fac.wav --seconds 1 --rate "$rate" 2>/dev/null) || true
[ "$(field "$out" replayer)" != "bbmp_fac" ] || fail "bbmp_fac claimed a .mus that is not FAC"
rm -f notfac.mus fd_fac.wav
echo fac_decode_ok
