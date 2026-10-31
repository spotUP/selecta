#!/bin/sh
# I725j: 68k static libs for the bench (libopus FIXED_POINT via cmake + repo toolchain file; libFLAC by explicit gcc lines).
# Usage: build_libs_amiga.sh <repo> <dl-dir> <out-dir>   (out-dir/lib gets libopus.a, libFLAC.a)
set -e
R=$1; D=$2; O=$3; mkdir -p "$O/lib" "$O/o"
nice -n 19 cmake -S "$D/opus/src" -B "$O/o/opus" -G "Unix Makefiles" -DCMAKE_TOOLCHAIN_FILE="$R/cmake/amiga-68020.cmake" \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_FLAGS_RELEASE="-O2 -DNDEBUG" -DOPUS_FIXED_POINT=ON -DOPUS_DISABLE_INTRINSICS=ON \
  -DOPUS_BUILD_PROGRAMS=OFF -DOPUS_BUILD_TESTING=OFF -DOPUS_STACK_PROTECTOR=OFF -DOPUS_FORTIFY_SOURCE=OFF >"$O/o/opus.log" 2>&1
nice -n 19 cmake --build "$O/o/opus" -j2 >>"$O/o/opus.log" 2>&1
cp "$O/o/opus/libopus.a" "$O/lib/libopus.a"
CC="$HOME/opt/amiga/bin/m68k-amigaos-gcc"; L=$D/flac/src/src/libFLAC; mkdir -p "$O/o/flac"; : > "$O/o/flac/config.h"
for f in "$L"/*.c; do
  case $f in *ogg*|*windows*|*_sse*|*_avx*|*_neon*|*_intrin*|*ppc*) continue;; esac
  nice -n 19 $CC -O2 -m68020 -mcrt=nix20 -fomit-frame-pointer -w -DFLAC__NO_DLL -DHAVE_STDINT_H=1 -DHAVE_INTTYPES_H=1 \
    -DHAVE_LROUND=1 -DHAVE_STDLIB_H=1 -DHAVE_STRING_H=1 -DFLAC__HAS_OGG=0 -DPACKAGE_VERSION='"1.5"' -DFLAC__ALIGN_MALLOC_DATA=0 \
    -I"$O/o/flac" -I"$D/flac/src/include" -I"$L/include" -I"$D/flac/src/src/libFLAC/include" -c "$f" -o "$O/o/flac/$(basename "$f" .c).o"
done
$HOME/opt/amiga/bin/m68k-amigaos-ar rcs "$O/lib/libFLAC.a" "$O/o/flac"/*.o
echo done >"$O/DONE"
