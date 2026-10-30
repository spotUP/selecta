#!/bin/sh
# I725j: build the decoder bench. Usage: build_bench.sh <mac|amiga> <dl-dir> <out-dir> <decoder>...
# <dl-dir> holds the upstream checkouts (tremor/src opus/src faad2/src fdkaac/src flac/src ogg/src tremolo/z), each fetched
# shallow into its own empty dir. Decoders: stb_vorbis tremor tremolo opus faad2 fdk drflac libflac.
# opus and fdk need a prebuilt static lib (cmake) at <out-dir>/lib/libopus.a / libfdk-aac.a; see the report for the cmake lines.
# amiga: m68k-amigaos-gcc from ~/opt/amiga, -O2 -m68020 (add -m68060 -m68881 etc. via EXTRA).
set -e
tgt=$1; dl=$2; out=$3; shift 3
here=$(cd "$(dirname "$0")" && pwd); repo=$(cd "$here/../.." && pwd)
mkdir -p "$out/gen/ogg" "$out/bin"
cat > "$out/gen/ogg/config_types.h" <<'EOT'
#include <stdint.h>
typedef int16_t ogg_int16_t; typedef uint16_t ogg_uint16_t; typedef int32_t ogg_int32_t; typedef uint32_t ogg_uint32_t;
typedef int64_t ogg_int64_t; typedef uint64_t ogg_uint64_t;
EOT
if [ "$tgt" = amiga ]; then CC="$HOME/opt/amiga/bin/m68k-amigaos-gcc"; CF="-O2 -m68020 -mcrt=nix20 -fomit-frame-pointer $EXTRA"; LIBM="-lm"; else CC=cc; CF="-O2 $EXTRA"; LIBM=""; fi
CF="$CF -w -I$here"
for d in "$@"; do
  b="$out/bin/bench_$d"
  case $d in
    stb_vorbis) $CC $CF -DDEC_STB_VORBIS -I"$repo/third_party/stb_vorbis" "$here/bench.c" -o "$b" $LIBM ;;
    drflac)     $CC $CF -DDEC_DRFLAC -DDR_FLAC_NO_STDIO -I"$repo/third_party/dr_flac" "$here/bench.c" -o "$b" $LIBM ;;
    tremor)     T=$dl/tremor/src; $CC $CF -DDEC_TREMOR -I"$out/gen" -I"$dl/ogg/src/include" -I"$T" "$here/bench.c" \
                  $(for f in block codebook floor0 floor1 info mapping0 mdct registry res012 sharedbook synthesis vorbisfile window; do echo $T/$f.c; done) \
                  "$dl/ogg/src/src/bitwise.c" "$dl/ogg/src/src/framing.c" -o "$b" $LIBM ;;
    tremolo)    T=$dl/tremolo/z; printf "#include <stdlib.h>\n#define mergesort tr_mergesort\n#include \"%s/floor1.c\"\n" "$T" > "$out/gen/floor1_w.c"; $CC $CF -DDEC_TREMOLO -DONLY_C "-DOutput(x)=" -I"$T" "$here/bench.c" \
                  "$out/gen/floor1_w.c" $(for f in bitwise codebook dsp floor0 floor_lookup framing info mapping0 mdct misc res012 vorbisfile; do echo $T/$f.c; done) -o "$b" $LIBM ;;
    opusfix)    $CC $CF -DDEC_OPUS -I"$out/gen" -I"$dl/ogg/src/include" -I"$dl/opus/src/include" "$here/bench.c" "$dl/ogg/src/src/bitwise.c" "$dl/ogg/src/src/framing.c" "$out/lib/libopusfix.a" -o "$b" $LIBM ;;
    opus)       $CC $CF -DDEC_OPUS -I"$out/gen" -I"$dl/ogg/src/include" -I"$dl/opus/src/include" "$here/bench.c" "$dl/ogg/src/src/bitwise.c" "$dl/ogg/src/src/framing.c" "$out/lib/libopus.a" -o "$b" $LIBM ;;
    faad2)      F=$dl/faad2/src/libfaad; $CC $CF -DDEC_FAAD2 -DFIXED_POINT -DHAVE_STDINT_H=1 -DPACKAGE_VERSION=\"2.11\" -I"$F" -I"$dl/faad2/src/include" "$here/bench.c" $(ls $F/*.c | grep -v drm_dec) -o "$b" $LIBM ;;
    fdk)        A=$dl/fdkaac/src; $CC $CF -DDEC_FDK -I$A/cpp/libAACdec/include -I$A/cpp/libSYS/include -I$A/cpp/libFDK/include "$here/bench.c" "$out/lib/libfdk-aac.dylib" -o "$b" $LIBM ;;
    libflac)    $CC $CF -DDEC_LIBFLAC -I"$dl/flac/src/include" "$here/bench.c" "$out/lib/libFLAC.a" -o "$b" $LIBM ;;
  esac
  echo "built $b"
done
