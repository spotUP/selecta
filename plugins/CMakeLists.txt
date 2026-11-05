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

# pxtone (I702): PiSTOL Collage songs (.ptcop, .pttune), permissively licensed play sample sources. C++ engine: host module
# and static Amiga plugin (I702a, bebbo m68k-amigaos-c++).
if(SEL_AMIGA OR (NOT EMSCRIPTEN AND NOT WIN32))
  add_subdirectory(replayers/pxtone)
endif()

# sunvox (I706a): SunVox songs (.sunvox) over the BSD 2009 engine (third_party/sunvox_bsd). Core plugin: host module and static Amiga plugin.
if(SEL_AMIGA OR (NOT EMSCRIPTEN AND NOT WIN32))
  add_subdirectory(replayers/sunvox)
endif()

# sunvox_lib (I706b): the same songs through the official precompiled SunVox Library, desktop only; wins over the BSD plugin above.
if(NOT SEL_AMIGA AND NOT EMSCRIPTEN AND NOT WIN32)
  add_subdirectory(replayers/sunvox_lib)
endif()

# organya (I703, I703a): Organya songs (.org, Cave Story) via organya.h (ISC-style licence), MIT module on the host, static soft-float plugin on the Amiga.
if(NOT EMSCRIPTEN AND NOT WIN32)
  add_subdirectory(replayers/organya)
endif()
