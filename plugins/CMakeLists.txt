# Plugins register with sel_add_plugin (cmake/selecta_plugin.cmake).
# Kinds: 1 replayer, 2 output, 3 dsp, 4 viz, 5 ui, 6 converter, 7 source,
# 8 integration, 9 scripting (include/selecta/plugin.h).

sel_add_plugin(null 2 outputs/null/null.c)
sel_add_plugin(wav 2 outputs/wav/wav.c)

# T5 negative path only; see tests/m0/bad_plugin.cmake.
if(SEL_TEST_BAD_PLUGIN)
  sel_add_plugin(bad 2 "${PROJECT_SOURCE_DIR}/tests/m0/bad_plugin.c")
endif()

add_subdirectory(replayers/nuade)
