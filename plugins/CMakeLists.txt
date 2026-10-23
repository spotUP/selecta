# Plugins register with sel_add_plugin (cmake/selecta_plugin.cmake).
# Kinds: 1 replayer, 2 output, 3 dsp, 4 viz, 5 ui, 6 converter, 7 source,
# 8 integration, 9 scripting (include/selecta/plugin.h).

sel_add_plugin(null 2 outputs/null/null.c)
sel_add_plugin(voices_test 2 outputs/voices_test/voices_test.c)
sel_add_plugin(paula_shadow 2 outputs/paula/shadow.c outputs/paula/paula_out.c outputs/paula/paula_voices.c)
# The hardware output shares paula_out.c and paula_voices.c with the shadow; only the register bus (paula_hw.c) differs.
if(SEL_AMIGA)
  sel_add_plugin(paula 2 outputs/paula/paula.c outputs/paula/paula_hw.c outputs/paula/paula_out.c outputs/paula/paula_voices.c)
endif()
# The Paula voice sink runs from the audio task on Amiga: integer only. Its objects join the T54 list
# (src/CMakeLists.txt writes m2-interrupt-objects.txt from the targets).
if(SEL_AMIGA)
  add_library(selecta_paula_irq OBJECT outputs/paula/paula_voices.c outputs/paula/paula_hw.c)
  target_include_directories(selecta_paula_irq PRIVATE ${PROJECT_SOURCE_DIR}/include)
endif()
# paula14 (M2 phase 4, I122/I123): portable split tables and chip double buffer in common/; the shadow twin counts
# register writes, the hardware output (Amiga) drives the custom chip through paula_hw.c.
set(SEL_P14_COMMON outputs/paula14/common/p14_split.c outputs/paula14/common/p14_sink.c outputs/paula14/common/p14_out.c)
sel_add_plugin(paula14_shadow 2 outputs/paula14/shadow.c ${SEL_P14_COMMON})
if(SEL_AMIGA)
  sel_add_plugin(paula14 2 outputs/paula14/paula14.c outputs/paula/paula_hw.c ${SEL_P14_COMMON})
  add_library(selecta_paula14_irq OBJECT outputs/paula14/common/p14_split.c outputs/paula14/common/p14_sink.c
    outputs/paula14/common/p14_out.c)
  target_include_directories(selecta_paula14_irq PRIVATE ${PROJECT_SOURCE_DIR}/include)
endif()
sel_add_plugin(wav 2 outputs/wav/wav.c)
sel_add_plugin(crossfeed 3 dsp/crossfeed/crossfeed.c)
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

# libopenmpt is C++17 and desktop only (plan M1.1, no 68k build of it).
if(NOT SEL_AMIGA)
  add_subdirectory(replayers/openmpt)
endif()

# Text UI building blocks (plan M1.1 phase 5): cell grid, keys, theme and the
# POSIX terminal device. Library for grid-unit; the UI plugin entry
# (text_ui.c, row I60) registers from ui/text/CMakeLists.txt. Desktop only until M2.
add_subdirectory(ui/text)
if(NOT SEL_AMIGA)
  add_subdirectory(integrations)
endif()
