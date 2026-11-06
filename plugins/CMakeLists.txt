# Plugins register with sel_add_plugin (cmake/selecta_plugin.cmake).
# Kinds: 1 replayer, 2 output, 3 dsp, 4 viz, 5 ui, 6 converter, 7 source,
# 8 integration, 9 scripting (include/selecta/plugin.h).

# Internal helper library shared by several plugins (rule 3): plugins/shared/<topic>/, include root plugins/shared.
add_subdirectory(shared)

sel_add_plugin(null 2 outputs/null/null.c)
sel_add_plugin(voices_test 2 outputs/voices_test/voices_test.c)
sel_add_plugin(paula_shadow 2 outputs/paula/shadow.c outputs/paula/paula_out.c)
target_link_libraries(sel_plugin_paula_shadow PUBLIC selecta_plugin_shared)
# The hardware output shares paula_out.c and paula_voices.c with the shadow; only the register bus (paula_hw.c) differs.
if(SEL_AMIGA)
  sel_add_plugin(paula 2 outputs/paula/paula.c outputs/paula/paula_out.c)
  target_link_libraries(sel_plugin_paula PUBLIC selecta_plugin_shared)
endif()
# The Paula voice sink runs from the audio task on Amiga: integer only. Its objects join the T54 list
# (src/CMakeLists.txt writes m2-interrupt-objects.txt from the targets).
if(SEL_AMIGA)
  add_library(selecta_paula_irq OBJECT shared/paula/paula_voices.c shared/paula/paula_hw.c)
  target_include_directories(selecta_paula_irq PRIVATE ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/plugins/shared)
endif()
# paula14 (M2 phase 4, I122/I123): portable split tables and chip double buffer in common/; the shadow twin counts
# register writes, the hardware output (Amiga) drives the custom chip through paula_hw.c.
set(SEL_P14_COMMON outputs/paula14/common/p14_split.c outputs/paula14/common/p14_sink.c outputs/paula14/common/p14_out.c)
sel_add_plugin(paula14_shadow 2 outputs/paula14/shadow.c ${SEL_P14_COMMON})
target_link_libraries(sel_plugin_paula14_shadow PUBLIC selecta_plugin_shared)
if(SEL_AMIGA)
  sel_add_plugin(paula14 2 outputs/paula14/paula14.c ${SEL_P14_COMMON})
  target_link_libraries(sel_plugin_paula14 PUBLIC selecta_plugin_shared)
  add_library(selecta_paula14_irq OBJECT outputs/paula14/common/p14_split.c outputs/paula14/common/p14_sink.c
    outputs/paula14/common/p14_out.c)
  target_include_directories(selecta_paula14_irq PRIVATE ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/plugins/shared)
endif()
# ahi (M2 phase 4, I124/I125): per-CPU buffer table and double-buffer bookkeeping in common/, the shadow twin counts
# the hand-offs, the Amiga output sends CMD_WRITE requests to ahi.device. Not in the interrupt path (rig-only).
set(SEL_AHI_COMMON outputs/ahi/common/ahi_cfg.c outputs/ahi/common/ahi_out.c)
sel_add_plugin(ahi_shadow 2 outputs/ahi/shadow.c ${SEL_AHI_COMMON})
if(SEL_AMIGA)
  sel_add_plugin(ahi 2 outputs/ahi/amiga/ahi.c ${SEL_AHI_COMMON})
endif()
sel_add_plugin(wav 2 outputs/wav/wav.c)
sel_add_plugin(crossfeed 3 dsp/crossfeed/crossfeed.c)
# Viz plugins (DUI-P4-11): render one sel_viz_frame into a sel_surface; the pixel helpers are header-only in shared/viz.
foreach(viz scope levels spectrum sidregs)
  sel_add_plugin(viz_${viz} 4 viz/${viz}/${viz}.c)
  target_link_libraries(sel_plugin_viz_${viz} PUBLIC selecta_plugin_shared)
endforeach()
# OS headers live under plugins/*/<os>/ (RULES.md rule 3).
if(APPLE)
  # I1039: coreaudio_av.m is the AVSampleBufferAudioRenderer path that follows the app's AirPlay route pick (ARC).
  enable_language(OBJC)
  sel_add_plugin(coreaudio 2 outputs/coreaudio/macos/coreaudio.c outputs/coreaudio/macos/coreaudio_av.m)
  target_compile_options(sel_plugin_coreaudio PRIVATE $<$<COMPILE_LANGUAGE:OBJC>:-fobjc-arc>)
  target_compile_options(sel_sdkcheck_coreaudio PRIVATE $<$<COMPILE_LANGUAGE:OBJC>:-fobjc-arc>)
  target_link_libraries(sel_plugin_coreaudio PUBLIC "-framework AudioToolbox" "-framework CoreAudio"
    "-framework AVFoundation" "-framework CoreMedia" "-framework Foundation")
endif()

# T5 negative path only; see tests/m0/bad_plugin.cmake.
if(SEL_TEST_BAD_PLUGIN)
  sel_add_plugin(bad 2 "${PROJECT_SOURCE_DIR}/tests/m0/bad_plugin.c")
endif()

add_subdirectory(replayers/nuade)

# Stream decoders (M3 phase 11, D-M3-8): single-header libraries, float inside, tagged SEL_RP_NEEDS_FLOAT.
add_subdirectory(replayers/minimp3)
add_subdirectory(replayers/stb_vorbis)
add_subdirectory(replayers/dr_flac) # I436: FLAC, integer decoder
add_subdirectory(replayers/arkos) # I433a: Arkos Tracker 3 song model + .aks importer (MIT, C, all targets)

# Game Music Emu (I426): C++ library, desktop first (no 68k build yet).
if(NOT SEL_AMIGA)
  add_subdirectory(replayers/gme)
endif()

# StSound (I428): Atari ST / Amstrad YM2149 chip music, C++ library, desktop first (no 68k build yet).
if(NOT SEL_AMIGA)
  add_subdirectory(replayers/ym)
endif()

# SNDH via sc68 (I427b): GPL-3, loadable module only, opt-in.
if(SELECTA_GPL_PLUGINS AND NOT SEL_AMIGA AND NOT EMSCRIPTEN AND NOT WIN32)
  add_subdirectory(replayers/sndh)
endif()

# PSG play (I707): GPL-2 module over psgplay, a second SNDH backend; sc68 stays the default through SEL_RP_PRIORITY. Host only.
if(SELECTA_GPL_PLUGINS AND NOT SEL_AMIGA AND NOT EMSCRIPTEN AND NOT WIN32)
  add_subdirectory(replayers/psgplay)
endif()

# SID via libsidplayfp (I434): GPL-2, loadable module only, opt-in, host only (Amiga SID is row I434a).
if(SELECTA_GPL_PLUGINS AND NOT SEL_AMIGA AND NOT EMSCRIPTEN AND NOT WIN32)
  add_subdirectory(replayers/sid)
endif()

# CHIPNSFX via CNGSoft's tracker code (I432): GPL-3 module, loadable module only, opt-in, host only.
if(SELECTA_GPL_PLUGINS AND NOT SEL_AMIGA AND NOT EMSCRIPTEN AND NOT WIN32)
  add_subdirectory(replayers/chipnsfx)
endif()

# mdxmini (I699): GPL-2 module over the mdxmini library, loadable module only, opt-in, host only.
if(SELECTA_GPL_PLUGINS AND NOT SEL_AMIGA AND NOT EMSCRIPTEN AND NOT WIN32)
  add_subdirectory(replayers/mdxmini)
endif()

# libkss (I698): MSX music (.kss and driver formats); ships the driver binaries, so it has its own option. Host only.
if(SELECTA_KSS_DRIVERS AND NOT SEL_AMIGA AND NOT EMSCRIPTEN AND NOT WIN32)
  add_subdirectory(replayers/kss)
endif()

# TIATracker (I429): GPL-2 module over TIATracker's replay code and Stella's TIA sound, loadable module only, opt-in, host only.
if(SELECTA_GPL_PLUGINS AND NOT SEL_AMIGA AND NOT EMSCRIPTEN AND NOT WIN32)
  add_subdirectory(replayers/tiatracker)
endif()

# QSF (I704): GPL-3 module over Highly Quixotic (Capcom QSound) and psflib, loadable module only, opt-in, host only.
if(SELECTA_GPL_PLUGINS AND NOT SEL_AMIGA AND NOT EMSCRIPTEN AND NOT WIN32)
  add_subdirectory(replayers/qsf)
endif()

# SNSF (I705a): GPL-3 module over the vendored bsnes core (third_party/bsnes) and psflib, loadable module only, opt-in, host only.
if(SELECTA_GPL_PLUGINS AND NOT SEL_AMIGA AND NOT EMSCRIPTEN AND NOT WIN32)
  add_subdirectory(replayers/snsf)
endif()

# ZXTune via bbmp's patched build (I421): GPL-3 module, loadable module only, opt-in, host only. Needs
# SELECTA_ZXTUNE_SRC (a zxtune checkout); without it the subdirectory prints a notice and builds nothing.
if(SELECTA_GPL_PLUGINS AND NOT SEL_AMIGA AND NOT EMSCRIPTEN AND NOT WIN32)
  add_subdirectory(replayers/zxtune)
endif()

# KPlayer v33 (HippoPlayer, MIT): ProTracker M.K. on Amiga, HW kind, drives Paula through the CIA timer (I219).
if(SEL_AMIGA)
  add_subdirectory(replayers/kplayer)
endif()

# Native playsid.library player (I434b): PSID/RSID through LIBS:playsid.library (not bundled), HW kind, Amiga only.
# Host builds keep libsidplayfp above.
if(SEL_AMIGA)
  add_subdirectory(replayers/playsid)
endif()

# libopenmpt is C++17 and desktop only (plan M1.1, no 68k build of it).
if(NOT SEL_AMIGA)
  add_subdirectory(replayers/openmpt)
endif()

# bbmp_mxm (I710f): Cubic Tiny XM songs (.mxm), bbmp csrc/pc/mxm_engine (clean-room C++ converter to XM, GPL 3 or later) played by the
# vendored libopenmpt, behind the GPL-plugin gate. Host only (C++17 engine and libopenmpt, no Amiga module path). Needs selecta_openmpt.
if(SELECTA_GPL_PLUGINS AND NOT SEL_AMIGA AND NOT EMSCRIPTEN AND NOT WIN32)
  add_subdirectory(replayers/bbmp_mxm)
endif()

# Text UI building blocks (plan M1.1 phase 5): cell grid, keys, theme and the
# POSIX terminal device. Library for grid-unit; the UI plugin entry
# (text_ui.c, row I60) registers from ui/text/CMakeLists.txt. Desktop only until M2.
add_subdirectory(ui/text)
add_subdirectory(ui/desktop)
add_subdirectory(ui/mui)
add_subdirectory(sources)
if(NOT SEL_AMIGA)
  add_subdirectory(integrations)
endif()

# ARexx port (M4-AX-3-1, kind 9). arexx_core.c is portable and compiled by tests/m4/arexx_test.c on the host; the
# plugin itself is Amiga only (exec/rexxsyslib).
if(SEL_AMIGA)
  sel_add_plugin(arexx 9 scripting/arexx/arexx_plugin.c scripting/arexx/arexx_core.c scripting/arexx/arexx_amiga.c)
endif()

# VGM/S98/DRO/GYM via libvgm (I431): GPL-2.0-or-later module (GPL sound cores), loadable module only, opt-in, host only.
if(SELECTA_GPL_PLUGINS AND NOT SEL_AMIGA AND NOT EMSCRIPTEN AND NOT WIN32)
  add_subdirectory(replayers/libvgm)
endif()

# OPL/AdLib songs via AdPlug (I696): LGPL-2.1-or-later engine linked statically, own opt-in module, host only.
if(SELECTA_GPL_PLUGINS AND NOT SEL_AMIGA AND NOT EMSCRIPTEN AND NOT WIN32)
  add_subdirectory(replayers/adplug)
endif()

# Tracker formats libopenmpt and UADE do not play, via libxmp (I697): MIT module, loadable module only, host only.
if(NOT SEL_AMIGA AND NOT EMSCRIPTEN AND NOT WIN32)
  add_subdirectory(replayers/xmp)
endif()

# Furnace, DefleMask, FamiTracker and TFM Music Maker modules via the Furnace engine (I430): GPL-2.0-or-later engine
# linked statically, own opt-in module, host only.
if(SELECTA_GPL_PLUGINS AND NOT SEL_AMIGA AND NOT EMSCRIPTEN AND NOT WIN32)
  add_subdirectory(replayers/furnace)
endif()

# 98fmplayer (I700): PC-98 PMD and FMP songs, BSD-2-Clause sources behind the shared GPL-plugin gate. Host only.
if(SELECTA_GPL_PLUGINS AND NOT SEL_AMIGA AND NOT EMSCRIPTEN AND NOT WIN32)
  add_subdirectory(replayers/98fmplayer)
endif()

# eupmini (I701): FM Towns Euphony (.eup) songs, GPL-2.0 EUPPlayer behind the GPL-plugin gate. Host only.
if(SELECTA_GPL_PLUGINS AND NOT SEL_AMIGA AND NOT EMSCRIPTEN AND NOT WIN32)
  add_subdirectory(replayers/eupmini)
endif()

# bbmp_ay (I709a): ZX Spectrum Flash Tracker (.fls), Fuxoft AY Language (.fxm, .amad) and Sound Tracker AY files (.strc), GPL-3.0 bbmp
# engines on the MIT Ayumi chip, behind the GPL-plugin gate. Host only.
if(SELECTA_GPL_PLUGINS AND NOT SEL_AMIGA AND NOT EMSCRIPTEN AND NOT WIN32)
  add_subdirectory(replayers/bbmp_ay)
endif()

# bbmp_sclub (I710ac): Sound Club and Sound Club 2 (.sn, .sn2), GPL-3.0 bbmp engine, behind the GPL-plugin gate. Host only (C++17
# engine, no Amiga module path).
if(SELECTA_GPL_PLUGINS AND NOT SEL_AMIGA AND NOT EMSCRIPTEN AND NOT WIN32)
  add_subdirectory(replayers/bbmp_sclub)
endif()

# bbmp_buzz (I710d): Jeskola Buzz songs (.bmx) on the public-domain BmxPlay machines, GPL-3.0 bbmp engine glue, behind the GPL-plugin
# gate. Host only (the engine glue is GPL, so it stays out of the MIT Amiga core).
if(SELECTA_GPL_PLUGINS AND NOT SEL_AMIGA AND NOT EMSCRIPTEN AND NOT WIN32)
  add_subdirectory(replayers/bbmp_buzz)
endif()

# bbmp_fac (I710ak): FAC SoundTracker for MSX (.mus), GPL-3.0 bbmp engine on the MIT emu8950 / emu2413 chips, behind the GPL-plugin
# gate. Host only (C++17 engine, no Amiga module path).
if(SELECTA_GPL_PLUGINS AND NOT SEL_AMIGA AND NOT EMSCRIPTEN AND NOT WIN32)
  add_subdirectory(replayers/bbmp_fac)
endif()

# bbmp_iigs (I710ad): SoundSmith songs of the Apple IIGS, GPL-3.0 bbmp engine on an Ensoniq 5503 emulation, behind the GPL-plugin
# gate. Host only (C++17 engine, no Amiga module path).
if(SELECTA_GPL_PLUGINS AND NOT SEL_AMIGA AND NOT EMSCRIPTEN AND NOT WIN32)
  add_subdirectory(replayers/bbmp_iigs)
endif()

# bbmp_onyx (I710r): Onyx Music Files (.omf), bbmp csrc/retro/jangle_engine (clean-room converter to a module, GPL 3 or later) played by the
# vendored libopenmpt, behind the GPL-plugin gate. Host only (C++17 engine and libopenmpt, no Amiga module path). Needs selecta_openmpt.
if(SELECTA_GPL_PLUGINS AND NOT SEL_AMIGA AND NOT EMSCRIPTEN AND NOT WIN32)
  add_subdirectory(replayers/bbmp_onyx)
endif()

# bbmp_smufi (I710t): Picatune 1 songs (.smufi), bbmp csrc/retro/smufi_engine (clean-room synthesizer tracker replayer, GPL 3 or later), behind the
# GPL-plugin gate. Host only (C++17 engine, x87/glibc maths, no Amiga module path).
if(SELECTA_GPL_PLUGINS AND NOT SEL_AMIGA AND NOT EMSCRIPTEN AND NOT WIN32)
  add_subdirectory(replayers/bbmp_smufi)
endif()

# bbmp_vgemm (I710ah): VGM Music Maker songs (.vge), bbmp csrc/retro/vgemm_engine (clean-room, GPL 3 or later) played by libvgm (static libs of replayers/libvgm, added above), behind the
# GPL-plugin gate. Host only (C++17 engine, no Amiga module path).
if(SELECTA_GPL_PLUGINS AND NOT SEL_AMIGA AND NOT EMSCRIPTEN AND NOT WIN32)
  add_subdirectory(replayers/bbmp_vgemm)
endif()

# bbmp_smx (I710am): SCC-Musixx songs (.sng, MSX SCC), bbmp csrc/retro/smx_engine (GPL 3 or later) on emu2212 (MIT), behind the GPL-plugin gate. Host only (C++17 engine,
# no Amiga module path; exemption in row I710am).
if(SELECTA_GPL_PLUGINS AND NOT SEL_AMIGA AND NOT EMSCRIPTEN AND NOT WIN32)
  add_subdirectory(replayers/bbmp_smx)
endif()

# bbmp_synder (I710an): Synder SNG-Player songs (.sng, C64 tunes as synthesizer settings), bbmp csrc/synder (GPL 3 or later), behind the GPL-plugin gate. Host only (C++17 engine,
# no Amiga module path; exemption in row I710an).
if(SELECTA_GPL_PLUGINS AND NOT SEL_AMIGA AND NOT EMSCRIPTEN AND NOT WIN32)
  add_subdirectory(replayers/bbmp_synder)
endif()

# bbmp_monotone (I710ao): MONOTONE songs (.mon, PC speaker tracker), bbmp csrc/pcmusic/monotone.cpp (GPL 3 or later port of a public-domain player), behind the GPL-plugin gate. Host only
# (C++17 engine, no Amiga module path; exemption in row I710ao).
if(SELECTA_GPL_PLUGINS AND NOT SEL_AMIGA AND NOT EMSCRIPTEN AND NOT WIN32)
  add_subdirectory(replayers/bbmp_monotone)
endif()

# bbmp_wwsnd (I710aj): Westwood SND songs (.snd, PC speaker), bbmp csrc/retro/wwsnd_engine (GPL 3 or later), behind the GPL-plugin gate. Host only (C++17 engine,
# no Amiga module path; exemption in row I710aj).
if(SELECTA_GPL_PLUGINS AND NOT SEL_AMIGA AND NOT EMSCRIPTEN AND NOT WIN32)
  add_subdirectory(replayers/bbmp_wwsnd)
endif()

# bbmp_victracker (I710ai): VIC-TRACKER songs (.vt), bbmp csrc/retro/victracker_engine (GPL 3 or later) running the tracker's own BSD player on a 6502 + VIC sound, behind the
# GPL-plugin gate. Host only (C++17 engine, no Amiga module path; exemption in row I710ai).
if(SELECTA_GPL_PLUGINS AND NOT SEL_AMIGA AND NOT EMSCRIPTEN AND NOT WIN32)
  add_subdirectory(replayers/bbmp_victracker)
endif()

# bbmp_tss (I710af): T'SoundSystem songs (.tss, .tsd), bbmp csrc/retro/tss_engine (BSD 3-clause, a port of toyoshim's TSS). Permissive, so a core
# module (no GPL gate). Host only (C++17 engine, no Amiga module path; exemption in row I710af, follow-up I710afa).
if(NOT SEL_AMIGA AND NOT EMSCRIPTEN AND NOT WIN32)
  add_subdirectory(replayers/bbmp_tss)
endif()

# stonetracker (I710ae): StoneTracker songs (.spm + .sps), the authors' StonePlayer_Hard.bin (a user system file) on Musashi's 68020, MIT, so a core
# module. Host only: the Amiga build runs the same bin on the real CPU (row I710aea), so it needs no emulator.
if(NOT SEL_AMIGA AND NOT EMSCRIPTEN AND NOT WIN32)
  add_subdirectory(replayers/stonetracker)
endif()

# bbmp_skale (I710ab): Skale Tracker songs (.skm), bbmp csrc/synth/skale_engine + skale_fx (GPL 3 or later) over the vendored libopenmpt with Luke
# Preiner's SKM loader patched in (third_party/patches/libopenmpt), behind the GPL-plugin gate. Host only (C++17 engine and libopenmpt, no Amiga
# module path; follow-up I710aba). Needs selecta_openmpt.
if(SELECTA_GPL_PLUGINS AND NOT SEL_AMIGA AND NOT EMSCRIPTEN AND NOT WIN32)
  add_subdirectory(replayers/bbmp_skale)
endif()

# bbmp_mgb (I710x): Gameboy Tracker songs (.mgb), bbmp csrc/retro/gbt_engine (clean-room, GPL 3 or later) over the GBSoundSystem driver (MIT) and the
# existing selecta_gme, behind the GPL-plugin gate. Host only (C++17 engine, GBS image played by libgme; Amiga follow-up I710xa).
if(SELECTA_GPL_PLUGINS AND NOT SEL_AMIGA AND NOT EMSCRIPTEN AND NOT WIN32)
  add_subdirectory(replayers/bbmp_mgb)
endif()

# bbmp_piece (I710s): Aquaplus P/ECE music driver songs (.pmd), GPL-3.0 bbmp clean-room engine (piece.cpp), behind the GPL-plugin
# gate. Host only (C++17 engine, no Amiga module path).
if(SELECTA_GPL_PLUGINS AND NOT SEL_AMIGA AND NOT EMSCRIPTEN AND NOT WIN32)
  add_subdirectory(replayers/bbmp_piece)
endif()

# bbmp_ntgs (I710p): Noise Tracker GS 1 modules of the Apple IIGS, GPL-3.0 bbmp engine (ntgs_cr.cpp) on the Ensoniq 5503
# emulation shared with bbmp_iigs, behind the GPL-plugin gate. Host only (C++17 engine, no Amiga module path).
if(SELECTA_GPL_PLUGINS AND NOT SEL_AMIGA AND NOT EMSCRIPTEN AND NOT WIN32)
  add_subdirectory(replayers/bbmp_ntgs)
endif()

# bbmp_musicline (I710n): Musicline Editor (.ml), bbmp csrc/musicline (LGPL replay routine, GPL Paula mixer), behind the GPL-plugin
# gate. Host only (C++17 engine, no Amiga module path).
if(SELECTA_GPL_PLUGINS AND NOT SEL_AMIGA AND NOT EMSCRIPTEN AND NOT WIN32)
  add_subdirectory(replayers/bbmp_musicline)
endif()

# bbmp_spu (I710y): PlayStation SPU logs (.spu), bbmp csrc/console/spu_engine over the P.E.Op.S. SPU (GPL 2 or later), behind the GPL-plugin
# gate. Host only (C++17 engine, no Amiga module path).
if(SELECTA_GPL_PLUGINS AND NOT SEL_AMIGA AND NOT EMSCRIPTEN AND NOT WIN32)
  add_subdirectory(replayers/bbmp_spu)
endif()

# bbmp_jxs (I710i): JayTrax songs (.jxs), bbmp csrc/synth/jxs_engine (clean-room, GPL 3 or later), behind the GPL-plugin gate. Host
# only (C++17 engine, no Amiga module path).
if(SELECTA_GPL_PLUGINS AND NOT SEL_AMIGA AND NOT EMSCRIPTEN AND NOT WIN32)
  add_subdirectory(replayers/bbmp_jxs)
endif()

# bbmp_kdm (I710j): Ken's Digital Music songs (.kdm), bbmp csrc/synth/kdm_engine (clean-room C engine, GPL 3 or later), behind the
# GPL-plugin gate. Host only (no Amiga module build of the GPL tier). The instruments (waves.kwv) are fetched beside the song.
if(SELECTA_GPL_PLUGINS AND NOT SEL_AMIGA AND NOT EMSCRIPTEN AND NOT WIN32)
  add_subdirectory(replayers/bbmp_kdm)
endif()

# bbmp_buzzic (I710e): Buzzic 1 and 2 songs (.buz, .buz2), bbmp csrc/synth/buz_engine (clean-room C++ engine, GPL 3 or later), behind
# the GPL-plugin gate. Host only (C++17 engine, no Amiga module build of the GPL tier). No samples, no companion file.
if(SELECTA_GPL_PLUGINS AND NOT SEL_AMIGA AND NOT EMSCRIPTEN AND NOT WIN32)
  add_subdirectory(replayers/bbmp_buzzic)
endif()

# bbmp_berotracker (I710b): BeRoTracker songs (.brt), bbmp csrc/berotracker (clean-room, GPL 3 or later), behind the GPL-plugin gate. Host
# only (C++17 engine with x87 long double arithmetic, no Amiga module path).
if(SELECTA_GPL_PLUGINS AND NOT SEL_AMIGA AND NOT EMSCRIPTEN AND NOT WIN32)
  add_subdirectory(replayers/bbmp_berotracker)
endif()

# bbmp_darkwave (I710h): DarkWave Studio projects (.dwp), bbmp csrc/darkwave over DarkWave Studio 4.1.6's own machines (GPL 3 or later), behind
# the GPL-plugin gate. Host only (C++14 engine on SSE2 intrinsics, sse2neon on arm64, no Amiga module path).
if(SELECTA_GPL_PLUGINS AND NOT SEL_AMIGA AND NOT EMSCRIPTEN AND NOT WIN32)
  add_subdirectory(replayers/bbmp_darkwave)
endif()

# bbmp_unitrk (I710m): MikMod UNITRK modules (.uni), libmikmod 3.3.14 (LGPL 2.1 or later) linked statically, behind the GPL-plugin gate like the
# other LGPL engine (adplug). Plain C99; Amiga status in row I710m.
if(SELECTA_GPL_PLUGINS AND NOT SEL_AMIGA AND NOT EMSCRIPTEN AND NOT WIN32)
  add_subdirectory(replayers/bbmp_unitrk)
endif()

# bbmp_ptk (I710q): NoiseTrekker / ProTrekkr songs (.ntk, .ptk), the BSD 2-clause ProTrekkr replay routine (not bbmp's GPL wrapper) plus our own
# shim. No GPL dependency, so no GPL gate. Host only; Amiga exemption in row I710q.
if(NOT SEL_AMIGA AND NOT EMSCRIPTEN AND NOT WIN32)
  add_subdirectory(replayers/bbmp_ptk)
endif()

# bbmp_sbstudio (I710v): SBStudio songs (.pac), libpac 0.9.0 (ISC, plain C) plus our own adapter; bbmp's GPL wrapper is not used. No GPL gate.
# Host only for now; the Amiga build is tried in row I710v.
if(NOT SEL_AMIGA AND NOT EMSCRIPTEN AND NOT WIN32)
  add_subdirectory(replayers/bbmp_sbstudio)
endif()

# bbmp_wsr (I710z): WonderSwan (.wsr), bbmp csrc/console/wsr_engine over Mednafen WonderSwan (GPL 2 only files), behind the GPL-plugin
# gate. Host only (C++17 emulator, forked scan workers, no Amiga module path).
if(SELECTA_GPL_PLUGINS AND NOT SEL_AMIGA AND NOT EMSCRIPTEN AND NOT WIN32)
  add_subdirectory(replayers/bbmp_wsr)
endif()

# bbmp_maxtrax (I710k): MaxTrax scores (.mxtx), bbmp csrc/amiga/maxtrax_engine over ScummVM's MaxTrax player (GPL 3), behind the
# GPL-plugin gate. Host only (C++17 engine on ScummVM-shaped headers, no Amiga module path).
if(SELECTA_GPL_PLUGINS AND NOT SEL_AMIGA AND NOT EMSCRIPTEN AND NOT WIN32)
  add_subdirectory(replayers/bbmp_maxtrax)
endif()

# bbmp_axs (I710a): AXS songs (.axs), bbmp csrc/axs clean-room engine (GPL 3 or later), behind the GPL-plugin gate. Host only (C++17
# engine, no Amiga module path).
if(SELECTA_GPL_PLUGINS AND NOT SEL_AMIGA AND NOT EMSCRIPTEN AND NOT WIN32)
  add_subdirectory(replayers/bbmp_axs)
endif()

# bbmp_boyscout (I710c): BoyScout songs (.bsf), bbmp csrc/retro clean-room engine (GPL 3 or later), behind the GPL-plugin gate. Host only
# (C++17 engine, no Amiga module path).
if(SELECTA_GPL_PLUGINS AND NOT SEL_AMIGA AND NOT EMSCRIPTEN AND NOT WIN32)
  add_subdirectory(replayers/bbmp_boyscout)
endif()

# bbmp_cybertracker (I710g): CyberTracker C64 songs (.ct), bbmp csrc/retro clean-room engine (GPL 3 or later) that runs the user's own
# CyberTracker 1.01 disk image on a 6502 + reSIDfp, behind the GPL-plugin gate. Host only (C++17 engine, no Amiga module path).
if(SELECTA_GPL_PLUGINS AND NOT SEL_AMIGA AND NOT EMSCRIPTEN AND NOT WIN32)
  add_subdirectory(replayers/bbmp_cybertracker)
endif()

# bbmp_shroom (I710aa): Mario Paint songs (.sho), bbmp csrc/retro shroom engine (GPL 3 or later) over the user's own Mariopants notes.wav (system
# file, Nintendo data, never shipped), behind the GPL-plugin gate. Host only (C++17 engine, no Amiga build; follow-up I710aaa).
if(SELECTA_GPL_PLUGINS AND NOT SEL_AMIGA AND NOT EMSCRIPTEN AND NOT WIN32)
  add_subdirectory(replayers/bbmp_shroom)
endif()

# bbmp_t0ast (I710ag): T0AST, "The 0ok Amazing Synth Tracker" songs (.t0ast), bbmp csrc/t0ast clean-room engine (GPL 3 or later), behind the GPL-plugin gate.
# Host only (C++17 engine with x87 long double; no Amiga build, exemption in row I710ag).
if(SELECTA_GPL_PLUGINS AND NOT SEL_AMIGA AND NOT EMSCRIPTEN AND NOT WIN32)
  add_subdirectory(replayers/bbmp_t0ast)
endif()

# bbmp_psycle (I710u): Psycle songs (.psy), GPL Psycle engine + bbmp helper and native plugins run as a child process, behind the
# GPL-plugin gate. Host only (C++14 engine, dlopen'd plugin libraries, no Amiga build).
if(SELECTA_GPL_PLUGINS AND NOT SEL_AMIGA AND NOT EMSCRIPTEN AND NOT WIN32)
  add_subdirectory(replayers/bbmp_psycle)
endif()

# pxtone (I702): PiSTOL Collage songs (.ptcop, .pttune), permissively licensed play sample sources. C++ engine: host module
# and static Amiga plugin (I702a, bebbo m68k-amigaos-c++).
if(SEL_AMIGA OR (NOT EMSCRIPTEN AND NOT WIN32))
  add_subdirectory(replayers/pxtone)
endif()

# sunvox (I706a): SunVox songs (.sunvox) over the BSD 2009 engine (third_party/sunvox_bsd). Core plugin: host module and static Amiga plugin.
if(SEL_AMIGA OR (NOT EMSCRIPTEN AND NOT WIN32))
  add_subdirectory(replayers/sunvox)
endif()

# klystron (I708a): Klystrack songs (.kt) over the MIT klystron engine (third_party/klystron, SDL shim). Core plugin: host module and static Amiga plugin.
if(SEL_AMIGA OR (NOT EMSCRIPTEN AND NOT WIN32))
  add_subdirectory(replayers/klystron)
endif()

# sunvox_lib (I706b): the same songs through the official precompiled SunVox Library, desktop only; wins over the BSD plugin above.
if(NOT SEL_AMIGA AND NOT EMSCRIPTEN AND NOT WIN32)
  add_subdirectory(replayers/sunvox_lib)
endif()

# organya (I703, I703a): Organya songs (.org, Cave Story) via organya.h (ISC-style licence), MIT module on the host, static soft-float plugin on the Amiga.
if(NOT EMSCRIPTEN AND NOT WIN32)
  add_subdirectory(replayers/organya)
endif()

# bbmp_nez (I710w): SGC and Game Boy GBR songs, libnezplug (Unlicense / MIT-0, plain C) plus our own adapter. Core module, no GPL gate.
# Host only for now; the Amiga build is tried in row I710w.
if(NOT SEL_AMIGA AND NOT EMSCRIPTEN AND NOT WIN32)
  add_subdirectory(replayers/bbmp_nez)
endif()
