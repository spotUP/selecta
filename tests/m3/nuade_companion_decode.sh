#!/bin/sh
# I710au: nuade songs that keep their samples in a companion file beside the song play through `selecta-cli render`
# (a voice-kind replayer: sentinel voice_ops>0). The adapter's NuadeIo reads the song's own directory (before: sel_nuade_io_none, open_failed).
#   Pierre Adane Packer: ivanhoe - bonus.pap + smp.set      Synth Pack: dyter07 title.osp + smp.set
# The songs have no stated redistribution licence: fetched by checksum from Modland into the git-ignored fixture cache,
# never committed. Exit 77 = skipped (no network). Per song: voice_ops>0, non-silent (NUADE_COMP_ENERGY_MIN raises the
# floor for red proofs) and mixed frames above NUADE_COMP_MIN_SECONDS (default 5; --seconds caps the render at 60).
# Also: the same song copied alone into an empty directory (no companion) must not play (the companion is what is read).
# usage: nuade_companion_decode.sh <selecta-cli>
set -u
cli=$1
here=$(dirname "$0")
M=https://modland.com/pub/modules
field() { printf '%s\n' "$1" | tr ' ' '\n' | sed -n "s/^$2=//p" | head -1; }
fail() { echo "FAIL: $*"; exit 1; }
energy() { od -An -v -t d2 -j 44 "$1" | tr -s " " "\n" | awk '$1 != "" { s += $1 * $1 } END { printf "%d", s }'; }
rate=44100
fetch() { "$here/fetch_fixture.sh" "$1" "$2" "$3" || exit $?; }
check() { # <song path>
  rm -f fd_nc.wav
  out=$("$cli" render "$1" --output fd_nc.wav --seconds 60 --rate "$rate" 2>/dev/null) || fail "render failed: $1"
  # nuade is a voice-kind replayer: its sentinel is the voice_ops count (the player drove the Paula voices), not replayer=.
  [ "$(field "$out" voice_ops)" -gt 0 ] 2>/dev/null || fail "$1: voice_ops=$(field "$out" voice_ops), the nuade player did not run"
  got=$(field "$out" mixed_frames)
  [ "$got" -ge $((rate * ${NUADE_COMP_MIN_SECONDS:-5})) ] || fail "$1: mixed_frames=$got, want at least $((rate * ${NUADE_COMP_MIN_SECONDS:-5}))"
  e=$(energy fd_nc.wav)
  [ "$e" -gt "${NUADE_COMP_ENERGY_MIN:-1000000}" ] || fail "$1: silent output (energy $e)"
  echo "$1 frames=$got energy=$e"
  rm -f fd_nc.wav
}
rm -rf nc_pa nc_sp nc_alone
mkdir nc_pa nc_sp nc_alone || fail "cannot create work directories"
P="$M/Pierre%20Adane%20Packer/Pierre-Eric%20Loriaux/Ivanhoe"
pap=$(fetch "$P/ivanhoe%20-%20bonus.pap" 230894ec54e244fc2a19128c9d1f5106fce03b546c75ad9b68c6696c24440381 ivanhoe-bonus.pap) || exit $?
pas=$(fetch "$P/smp.set" d4d050f1fc2540309996245926e81b01d9d7f27c719f7257bf8532639d9f1c47 ivanhoe-smp.set) || exit $?
S="$M/Synth%20Pack/Karsten%20Obarski/Dyter-07"
osp=$(fetch "$S/dyter07%20title.osp" d28f8b16d4c6159f3b1678e2a0d806a5552a34f4491789680c34f2120e4d7528 dyter07-title.osp) || exit $?
sps=$(fetch "$S/smp.set" cf8e7f6376d31ecdeb2c20f17f09276181b6130243b81b04c72eaec06a4bdb51 dyter07-smp.set) || exit $?
cp "$pap" "nc_pa/ivanhoe - bonus.pap" && cp "$pas" nc_pa/smp.set || fail "cannot stage Pierre Adane"
cp "$osp" "nc_sp/dyter07 title.osp" && cp "$sps" nc_sp/smp.set || fail "cannot stage Synth Pack"
check "nc_pa/ivanhoe - bonus.pap"
check "nc_sp/dyter07 title.osp"
# Without the companion the song cannot open.
cp "$pap" "nc_alone/ivanhoe - bonus.pap" || fail "cannot stage the lone song"
if "$cli" render "nc_alone/ivanhoe - bonus.pap" --output fd_nc0.wav --seconds 5 --rate "$rate" >/dev/null 2>&1; then
  fail "the Pierre Adane song rendered without its smp.set"
fi
rm -rf nc_pa nc_sp nc_alone fd_nc0.wav
echo nuade_companion_decode_ok
