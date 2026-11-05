#!/bin/sh
# I705b: snsf-real-decode fetches a real SNES game set (.minisnsf + .snsflib, game ROM data: not redistributable, not in
# git) via fetch_fixture.sh, then runs snsf_decode.sh. Sources and checksums: tests/m3/fixtures/snsf_real.SOURCE.
# Proves the SNSF layout parsing (version 0x23 exe records, _lib chain) and the clean-room SPC700 boot ROM against files
# written by the real ripping tools: the game's own sound driver only runs if the boot ROM took its upload.
# Exit 77 (SKIP) when offline.
# usage: snsf_real_decode.sh <selecta-cli> wolfenstein|blackthorne
set -u
here=$(dirname "$0")
case $2 in
wolfenstein) # HiROM
  base='https://modland.antarctica.no/pub/modules/Super%20Nintendo%20Sound%20Format/Robert%20Prince/Wolfenstein%203D'
  song=$("$here/fetch_fixture.sh" "$base/wolfenstein3d-0000.minisnsf" 88af26eaf058bb5ec082d0bc36cff3094f3693302141f502504684cf6fe279f2 wolfenstein3d-0000.minisnsf) || exit $?
  lib=$("$here/fetch_fixture.sh" "$base/wolfenstein3d.snsflib" 5498ae1660d5df81d37f5cf205cedfc8db0190a46456e8c0c65a1ac9392f7006 wolfenstein3d.snsflib) || exit $?
  ;;
blackthorne) # LoROM
  base='https://modland.antarctica.no/pub/modules/Super%20Nintendo%20Sound%20Format/Glenn%20Stafford/Blackthorne'
  song=$("$here/fetch_fixture.sh" "$base/blackthorne-0000.minisnsf" 2420f41ce2e196f20b04287c8057f44d87d3a47538081ff5146830569d5f8568 blackthorne-0000.minisnsf) || exit $?
  lib=$("$here/fetch_fixture.sh" "$base/blackthorne.snsflib" 8917187eacb2134d9e990f80fc3a4820e2c5f3bb631c132a299cc4d63eb36167 blackthorne.snsflib) || exit $?
  ;;
*) echo "FAIL: unknown game $2"; exit 1 ;;
esac
# no length tag: the plugin default is 180 s + 10 s fade = exactly 190 s. Energy measured 7.4e12 (wolfenstein).
SNSF_LIB=$lib SNSF_MIN_SECONDS=190 SNSF_MAX_SECONDS=191 SNSF_ENERGY_MIN=${SNSF_ENERGY_MIN:-1000000000000} \
  exec sh "$here/snsf_decode.sh" "$1" "$song"
