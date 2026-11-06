#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# I710u: builds bbmp's Psycle helper and Psycle's native plugins from the vendored sources. Adapted from bbmp tools/build-psycle.sh
# (bbmp 19d87098, GPL-3.0-or-later): no svn (the Psycle r12005 tree is in src/), the patches come from third_party/patches/bbmp_psycle,
# sed is portable (macOS and GNU). Needs a C++14 g++/clang++, python3, patch, tar, bzip2.
#   build.sh <out dir> <work dir>      out: bin/bbmp-psycle, lib/psycle-plugins/*.dylib|so, COPYING.psycle
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
PATCHES="$HERE/../patches/bbmp_psycle"
OUT="$(mkdir -p "$1" && cd "$1" && pwd)"
W="$2"
JOBS="${JOBS:-2}"
export JOBS
STK_SHA256=35c7d5dec90c6a23c907d4cfb5f1dd5800242b89eaafd89a17d90808747eed4a
MY="$HERE/bbmp"
sedi() { sed -i.orig "$@"; }
echo "$STK_SHA256  $HERE/src/stk-4.5.0.tar.bz2" | shasum -a 256 -c - >/dev/null

rm -rf "$W"; mkdir -p "$W/external-packages" "$W/obj/host" "$W/obj/pic"
cp -R "$HERE/src/psycle" "$HERE/src/psycle-helpers" "$HERE/src/universalis" "$HERE/src/diversalis" "$HERE/src/psycle-plugins" "$W/"
tar xjf "$HERE/src/stk-4.5.0.tar.bz2" -C "$W/external-packages"
patch -s -d "$W" -p1 < "$PATCHES/psycle-r12005-linux.patch"
{ grep -rlF 'throw(...)' "$W/psycle/src/psycle/host" "$W/psycle-plugins/src/psycle/plugin_interface.hpp" || true; } \
  | xargs -I{} sed -i.orig 's/throw(\.\.\.)/noexcept(false)/g' {}
H="$W/psycle/src/psycle/host"
rm -f "$H"/{Psycle,PsycleConfig,VstHost24,LuaPlugin,LuaHost,LuaArray,LuaHelper,LuaInternals,ladspahost}.hpp "$H/ladspamachine.h"
python3 "$MY/fixcase.py" "$W/psycle/src" "$W/psycle-helpers/src" "$W/universalis/src" "$W/diversalis/src" "$W/psycle-plugins/src"
patch -s -d "$W" -p1 < "$PATCHES/psycle-r12005-damaged.patch"
grep -q 'modland: the sizes are checked' "$H/Instrument.cpp" && grep -q 'modland: a sample is no longer than the file' "$H/Song.cpp" \
  && grep -q 'modland: a byte of the file is a bool' "$H/FileIO.hpp" \
  || { echo "the patch for damaged files did not apply" >&2; exit 1; }

INC="-I$MY/compat -I$MY/compat/host -I$MY -I$W/psycle/src -I$W/psycle-helpers/src -I$W/universalis/src -I$W/diversalis/src -I$W/psycle-plugins/src"
CXX="${CXX:-c++} -std=gnu++14 -O2 -w -fpermissive -DNDEBUG"
sedi 's/static int XMSampler::CalcLPBFromSpeed/static int CalcLPBFromSpeed/' "$H/XMSampler.hpp"
sedi -E 's/([a-zA-Z_]+ )Player::CalcPosition/\1CalcPosition/' "$H/Player.hpp"
sedi 's/inline void WaveDataController::Work(/inline void Work(/' "$H/Sampler.hpp"
sedi 's/process_cputime_supported = false;/process_cpu_time_supported = false;/' "$W/universalis/src/universalis/os/clocks.cpp"
sedi '/^ *template void XMInstrument::WaveData<short>::Init();/d' "$H/XMInstrument.cpp"
sedi 's/#elif defined __arm__$/#elif defined __arm__ || defined __aarch64__/' "$W/diversalis/src/diversalis/cpu.hpp"
UNIQUE=-fno-gnu-unique
if [ "$(uname -s)" = Darwin ]; then CXX="$CXX -Wno-c++11-narrowing -include $MY/compat/darwin.h"; UNIQUE=; LIBDL=; else LIBDL=-ldl; fi
U="$W/universalis/src/universalis"
UNIVERSALIS="$U/exception.cpp $U/compiler/exception.compiler.cpp $U/compiler/typenameof.cpp $U/cpu/exception.cpu.cpp $U/os/clocks.cpp
  $U/os/exception.os.cpp $U/os/loggers.cpp $U/os/terminal.cpp $U/os/thread_name.cpp $U/stdlib/exception.stdlib.cpp"
HELPERS="$(ls "$W"/psycle-helpers/src/psycle/helpers/*.cpp)"
compile() {   # <folder of objects> <flags> <sources...>
  local out="$1" flags="$2"; shift 2
  printf '%s\n' "$@" | xargs -P "$JOBS" -I{} bash -c '
    o="$0/$(basename "${2%.cpp}").o"
    $1 -c "$2" -o "$o" || { echo "failed: $2" >&2; exit 255; }' "$out" "$CXX $flags" {}
}
HOST=""
for f in AudioDriver FileIO Instrument internal_machines Machine Player Plugin Preset Sampler Song XMInstrument XMSampler ExclusiveLock InstPreview LoaderHelper; do HOST="$HOST $H/$f.cpp"; done
compile "$W/obj/host" "-DWINAMP_PLUGIN -include $MY/compat/compat.hpp $INC" $HOST $HELPERS $UNIVERSALIS "$U/os/sched.cpp" "$MY/host.cpp" "$MY/bbmp-psycle.cpp"
mkdir -p "$OUT/bin" "$OUT/lib/psycle-plugins/before-2010"
${CXX%% *} -o "$OUT/bin/bbmp-psycle" "$W"/obj/host/*.o $LIBDL -lpthread
compile "$W/obj/pic" "-fPIC $UNIQUE -include $MY/compat/plugin.hpp $INC" $HELPERS $UNIVERSALIS
mkdir -p "$W/plugins/.obj"; rm -f "$W/plugins/.obj/libhelpers.a"
ar rcs "$W/plugins/.obj/libhelpers.a" "$W"/obj/pic/*.o
python3 "$MY/build_plugins.py" "$W" "$W/plugins" "$MY/compat"
cp "$W"/plugins/*.so "$OUT/lib/psycle-plugins/"
cp "$W"/plugins/before-2010/*.so "$OUT/lib/psycle-plugins/before-2010/"
cp "$HERE/src/COPYING" "$OUT/COPYING.psycle"
echo "built $OUT/bin/bbmp-psycle and $(find "$OUT/lib/psycle-plugins" -name '*.so' | wc -l | tr -d ' ') plugins"
