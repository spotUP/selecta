#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Builds the ZXTune static libraries the Selecta zxtune plugin links (row I421).
#   usage: build_zxtune.sh <zxtune-checkout> <work-dir> <plugin-dir> <stamp-file>
# <zxtune-checkout> is a clean checkout of github.com/vitamin-caig/zxtune at the revision in ZXTUNE_REVISION (the one
# bbmp builds). It is never modified: the script copies it (without samples and regression data) into <work-dir>,
# applies bbmp's patches, trims the player list to the Spectrum and chiptune families Selecta does not already
# play through another replayer, and runs ZXTune's own make. The libraries land in <work-dir>/lib/<platform>/release.
set -eu
SRC=$1
WORK=$2
PLUGIN=$3
STAMP=$4

case "$(uname -s)" in
  Darwin) PLATFORM=darwin ;;
  *) PLATFORM=linux ;;
esac

if [ ! -d "$WORK/src" ]; then
  mkdir -p "$WORK"
  tar -C "$SRC" --exclude=.git --exclude=samples --exclude=regression --exclude=lib --exclude=obj -cf - . | tar -C "$WORK" -xf -
  for p in pt2-player pt2-patterns pt3-byte-sums no-z80ex no-ps2-bios no-rar-vgmstream no-libvgm no-shared-ptr-unique; do
    patch -d "$WORK" -p1 -s -i "$PLUGIN/patches/$p.patch"
  done
  rm -f "$WORK/src/module/players/xsf/scph10000_he.inc" "$WORK/src/formats/archived/generic/rar.cpp" \
    "$WORK/src/module/players/external/vgmstream.cpp" "$WORK/src/module/players/external/libvgm.cpp"
  python3 -I "$PLUGIN/trim_players.py" "$WORK"
  mkdir -p "$WORK/apps/selecta-zxtune"
  cp "$PLUGIN/app.Makefile" "$WORK/apps/selecta-zxtune/Makefile"
  cp "$PLUGIN/stub.cpp" "$WORK/apps/selecta-zxtune/stub.cpp"
  cp "$PLUGIN/lazyusf2.Makefile" "$WORK/3rdparty/lazyusf2/Makefile"
  cp "$WORK/3rdparty/lazyusf2/usf/resampler.c" "$WORK/3rdparty/lazyusf2/usf/resampler.h" "$WORK/3rdparty/lazyusf2/rsp_hle/audio.c" "$WORK/3rdparty/lazyusf2/"
fi

cd "$WORK/apps/selecta-zxtune"
make -j"${ZXTUNE_JOBS:-2}" platform=$PLATFORM release=1 system.zlib=1 \
  cxx_flags="-D_LIBCPP_ENABLE_CXX20_REMOVED_FEATURES -isystem /opt/homebrew/include -include $PLUGIN/stdint-both.h"
touch "$STAMP"
