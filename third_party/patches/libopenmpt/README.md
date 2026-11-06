# libopenmpt patches (third_party/libopenmpt, submodule at libopenmpt-0.8.4)

0001-skale-skm-loader.patch (row I710ab): adds Luke Preiner's experimental Skale Tracker loader (github.com/lpreiner/skale-skm-loader-openmpt,
commit 27dde229f19bd8a1c05ba04314ed5e17a086fcee, BSD 2-clause, LICENSE.skm-loader) as soundlib/Load_skm.cpp, declares ProbeFileHeaderSKM/ReadSKM
(Sndfile.h), lists it in ModuleFormatLoaders (Sndfile.cpp) and adds { "Skale Tracker", "skm" } (Tables.cpp). The loader carries the same
additive effect-send hook as bbmp tools/build-synth.sh: two globals (modland_skm_hook, modland_skm_gain[256]) and three lines marked
"// modland"; with modland_skm_hook == 0 (the default, set only by bbmp_skale) the loader behaves as upstream.

Applied at configure time by plugins/replayers/openmpt/CMakeLists.txt (git apply, skipped when already applied). The one shared
selecta_openmpt carries it, so the core openmpt replayer also lists "skm"; bbmp_skale (priority 20) takes the file when the GPL plugins
are on.
