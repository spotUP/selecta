#!/bin/sh
# I725j: Mac static libs for the bench (opus fixed and float, fdk-aac, libFLAC). Usage: build_libs_mac.sh <dl-dir> <build-dir>
# Every cmake invocation is explicit; nothing downloaded is executed except the build itself.
set -e
D=$1; B=$2; mkdir -p "$B"
cm() { nice -n 19 cmake "$@"; }
cm -S "$D/opus/src" -B "$B/opusfix" -DCMAKE_BUILD_TYPE=Release -DOPUS_FIXED_POINT=ON -DOPUS_BUILD_PROGRAMS=OFF -DOPUS_BUILD_TESTING=OFF -DOPUS_DISABLE_INTRINSICS=ON >"$B/o1.log" 2>&1
cm --build "$B/opusfix" -j2 >>"$B/o1.log" 2>&1
cm -S "$D/opus/src" -B "$B/opusflt" -DCMAKE_BUILD_TYPE=Release -DOPUS_BUILD_PROGRAMS=OFF -DOPUS_BUILD_TESTING=OFF >"$B/o2.log" 2>&1
cm --build "$B/opusflt" -j2 >>"$B/o2.log" 2>&1
cm -S "$D/fdkaac/src" -B "$B/fdk" -DCMAKE_BUILD_TYPE=Release -DBUILD_PROGRAMS=OFF >"$B/f.log" 2>&1
cm --build "$B/fdk" -j2 >>"$B/f.log" 2>&1
cm -S "$D/flac/src" -B "$B/flac" -DCMAKE_BUILD_TYPE=Release -DWITH_OGG=OFF -DBUILD_PROGRAMS=OFF -DBUILD_EXAMPLES=OFF -DBUILD_TESTING=OFF -DBUILD_DOCS=OFF -DINSTALL_MANPAGES=OFF >"$B/l.log" 2>&1
cm --build "$B/flac" -j2 >>"$B/l.log" 2>&1
echo done >"$B/DONE"
