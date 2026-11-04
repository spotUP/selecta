library_name := lazyusf2
dirs.root := ../..
include $(dirs.root)/make/default.mak

# Selecta: only the cubic resampler of the N64 USF player is needed (libsound links it); the rest stays out.
# build_zxtune.sh copies usf/resampler.{c,h} and rsp_hle/audio.c (the lookup table) next to this Makefile.
source_files := resampler.c audio.c

includes.dirs += . rsp_hle $(dirs.root)/3rdparty/zlib

include $(dirs.root)/makefile.mak
