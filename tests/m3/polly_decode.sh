#!/bin/sh
# I710ar: Polly Tracker songs (.mod) play through `selecta-cli render` as a RENDER-kind replayer: the sid module (libsidplayfp) puts the song behind
# the vendored 962-byte C64 player (third_party/polly_player) and plays the RSID. Five real Modland songs, fetched by checksum, never committed
# (tests/m3/fixtures/polly.SOURCE). Exit 77 = skipped (no network). Sentinel: the `replayer=sid` line of render for a .mod name; no other replayer
# plays these files (openmpt refuses them), so the sid line proves the Polly path ran.
# Asserts per song: the plugin ran, the WAV is not silent (POLLY_ENERGY_MIN raises the floor, for red proofs) and 20 s render fully (the song has no
# length, the host stops it; POLLY_MIN_SECONDS replaces the floor, for red proofs).
# Negative cases: a zero-filled .mod, an Amiga M.K. shaped file (first byte 0xAE too) and an oversize file are not claimed by sid.
# usage: polly_decode.sh <selecta-cli>
set -u
cli=$1
here=$(dirname "$0")
M="https://modland.com/pub/modules/Pollytracker"
field() { printf '%s\n' "$1" | tr ' ' '\n' | sed -n "s/^$2=//p" | head -1; }
fail() { echo "FAIL: $*"; exit 1; }
energy() { od -An -v -t d2 -j 44 "$1" | tr -s " " "\n" | awk '$1 != "" { s += $1 * $1 } END { printf "%d", s }'; }
rate=44100
fetch() { "$here/fetch_fixture.sh" "$1" "$2" "$3" || exit $?; }
check() { # <song path>
  rm -f fd_polly.wav
  out=$("$cli" render "$1" --output fd_polly.wav --seconds 20 --rate "$rate" 2>/dev/null) || fail "render failed: $1"
  [ "$(field "$out" replayer)" = "sid" ] || fail "$1: replayer=$(field "$out" replayer), want sid"
  got=$(field "$out" rendered_frames)
  min=${POLLY_MIN_SECONDS:-20}
  [ "$got" -ge $((rate * min)) ] || fail "$1: rendered_frames=$got, want at least $((rate * min))"
  e=$(energy fd_polly.wav)
  [ "$e" -gt "${POLLY_ENERGY_MIN:-1000000}" ] || fail "$1: silent output (energy $e)"
  echo "$1 frames=$got energy=$e"
  rm -f fd_polly.wav
}
s1=$(fetch "$M/Heatbeat/backwoods.mod" 47f93d33a982de0549b7330fffb7d5774003a47316e45762542ab8e9b2b7b06c polly_backwoods.mod) || exit $?
s2=$(fetch "$M/Reed/dirty%2764.mod" fb975adafeb3f4c36e09d655b8b8717dc24859924f5c5b31fdcc37aca1d13252 polly_dirty.mod) || exit $?
s3=$(fetch "$M/_unknown/classic.mod" 52c793ebbfb3bdc21aae4776bedeb6f0e7461d9b4871fac70ce5ba6961dfa2df polly_classic.mod) || exit $?
s4=$(fetch "$M/_unknown/jump.mod" 8aa9bca519e77e5c5f96710fbc2fb414e98a87db0f4fbcbba94fed3fc8f3d0c5 polly_jump.mod) || exit $?
s5=$(fetch "$M/_unknown/polly%20rock.mod" 67dd1d8153436950d890f5b546d5c1a8fd5aa0c0a23c39b98a7682ad4659436c polly_rock.mod) || exit $?
for s in "$s1" "$s2" "$s3" "$s4" "$s5"; do check "$s"; done
# not claimed: zero-filled, an Amiga M.K. shaped file starting with 0xAE, a file too large for the C64's memory
head -c 4096 /dev/zero > zero.mod
{ printf '\256'; head -c 1079 /dev/zero; printf 'M.K.'; head -c 2048 /dev/zero; } > mk.mod
{ printf '\256'; head -c 70000 /dev/zero; } > big.mod
for f in zero.mod mk.mod big.mod; do
  out=$("$cli" render "$f" --output fd_polly.wav --seconds 1 --rate "$rate" 2>/dev/null) || true
  [ "$(field "$out" replayer)" != "sid" ] || fail "sid claimed $f"
done
rm -f zero.mod mk.mod big.mod fd_polly.wav
echo polly_decode_ok
