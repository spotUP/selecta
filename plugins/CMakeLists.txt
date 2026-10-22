# Plugins register with sel_add_plugin (cmake/selecta_plugin.cmake).
# Kinds: 1 replayer, 2 output, 3 dsp, 4 viz, 5 ui, 6 converter, 7 source,
# 8 integration, 9 scripting (include/selecta/plugin.h).

sel_add_plugin(null 2 outputs/null/null.c)
sel_add_plugin(wav 2 outputs/wav/wav.c)
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
