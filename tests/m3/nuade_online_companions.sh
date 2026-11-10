#!/bin/sh
# I1242i: nuade formats whose samples/data live in a second file (jpn, mcr, pap, pn, sdr, sjs, tpu) play from an ONLINE key.
# Per format one real modland key is opened through `selecta-cli script` (OPEN modland:<path>, PLAY, @render) with an EMPTY
# SELECTA_CACHE_DIR. The library cache must bring the companion file (smp.*, mcs.*, pn.*.info, smp.set) beside the song through the
# nuade SEL_RP_COMPANIONS rule table; without the rule the player has no samples and the render is silent or the open fails.
# Sentinels: the companion file exists in the cache, and the rendered WAV is not silent (NUADE_COMPANION_ENERGY_MIN raises the floor).
# Songs are fetched at test time through the product's own mirror transport, never committed. Exit 77 = skipped (no network).
# usage: nuade_online_companions.sh <selecta-cli>
set -u
cli=$1
work=${NUADE_COMPANION_WORK:-nuade_companions_work}
rm -rf "$work"
mkdir -p "$work" || exit 1
cache="$PWD/$work/cache"
export SELECTA_CACHE_DIR="$cache"
# The built-in mirror list is not the point here; modland.com serves the same tree, over https.
export SELECTA_MIRRORS_MODLAND="${NUADE_COMPANION_MIRROR:-https://modland.com/pub/modules/}"
bad=0
fail() { echo "FAIL: $*"; bad=1; }
frames=220500
if ! curl -fsS -m 20 -o /dev/null -I "https://modland.com/pub/modules/Jason%20Page/Jason%20Page/jpn.empiresoccer94" 2>/dev/null; then
  echo "SKIP: modland.com not reachable"
  exit 77
fi
energy() { od -An -v -t d2 "$1" | tr -s " " "\n" | awk '$1 != "" { s += $1 * $1 } END { printf "%d", s }'; }
n=0
check() { # <format> <modland path of the song> <companion path that must land in the cache>
  fmt=$1; song=$2; comp=$3
  n=$((n + 1))
  raw="$work/$fmt.raw"
  sel="$work/$fmt.sel"
  printf '@dump %s\nOPEN "modland:%s"\nPLAY\n@render %s\nSTOP\n' "$raw" "$song" "$frames" > "$sel"
  out=$("$cli" script "$sel" --db "$work/$fmt.db" 2>&1)
  rc=$?
  if [ -f "$cache/files/modland/$comp" ]; then have=yes; else have=no; fi
  if [ "$rc" -ne 0 ] || printf '%s' "$out" | grep -q -E "ERROR|open_failed"; then
    if [ "$have" = no ]; then
      fail "$fmt: open failed and companion $comp was never fetched (no companion rule)"
      return
    fi
    fail "$fmt: open failed: $(printf '%s' "$out" | tail -n 2)"
    return
  fi
  [ "$have" = yes ] || { fail "$fmt: companion $comp is not in the cache"; return; }
  [ -f "$raw" ] || { fail "$fmt: no PCM dumped"; return; }
  e=$(energy "$raw")
  [ "$e" -gt "${NUADE_COMPANION_ENERGY_MIN:-1000000}" ] || { fail "$fmt: silent output (energy $e)"; return; }
  echo "$fmt PLAY energy=$e companion=$comp"
  rm -f "$raw"
}
check jpn "Jason Page/Jason Page/jpn.empiresoccer94" "Jason Page/Jason Page/smp.empiresoccer94"
check mcr "Mark Cooksey Old/Mark Cooksey/mcr.aquablast" "Mark Cooksey Old/Mark Cooksey/mcs.aquablast"
check pap "Pierre Adane Packer/Pierre-Eric Loriaux/Pang/pang - ingame 01.pap" "Pierre Adane Packer/Pierre-Eric Loriaux/Pang/smp.set"
check pn "PokeyNoise/Adam Gilmore/pn.zybex" "PokeyNoise/Adam Gilmore/pn.zybex.info"
check sdr "Synth Dream/Laurens Tummers/sdr.nobuddiesland hiscore" "Synth Dream/Laurens Tummers/smp.nobuddiesland hiscore"
check sjs "SoundPlayer/Scott Johnston/sjs.awesome" "SoundPlayer/Scott Johnston/smp.awesome"
check tpu "Dirk Bialluch/Dirk Bialluch/tpu.timelock title" "Dirk Bialluch/Dirk Bialluch/smp.timelock title"
[ "$bad" -eq 0 ] || exit 1
rm -rf "$work"
echo nuade_online_companions_ok
