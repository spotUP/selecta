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
foreach(viz scope levels spectrum)
  sel_add_plugin(viz_${viz} 4 viz/${viz}/${viz}.c)
  target_link_libraries(sel_plugin_viz_${viz} PUBLIC selecta_plugin_shared)
endforeach()
# OS headers live under plugins/*/<os>/ (RULES.md rule 3).
if(APPLE)
  sel_add_plugin(coreaudio 2 outputs/coreaudio/macos/coreaudio.c)
  target_link_libraries(sel_plugin_coreaudio PUBLIC "-framework AudioToolbox" "-framework CoreAudio")
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

# Game Music Emu (I426): C++ library, desktop first (no 68k build yet).
if(NOT SEL_AMIGA)
  add_subdirectory(replayers/gme)
endif()

# StSound (I428): Atari ST / Amstrad YM2149 chip music, C++ library, desktop first (no 68k build yet).
if(NOT SEL_AMIGA)
  add_subdirectory(replayers/ym)
endif()

# KPlayer v33 (HippoPlayer, MIT): ProTracker M.K. on Amiga, HW kind, drives Paula through the CIA timer (I219).
if(SEL_AMIGA)
  add_subdirectory(replayers/kplayer)
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
if(NOT SEL_AMIGA)
  add_subdirectory(integrations)
endif()
