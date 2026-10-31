# I427b: the GPL sndh module (SELECTA_GPL_PLUGINS=ON only; tests/CMakeLists.txt adds this directory under that option).
# The module builds into ${CMAKE_BINARY_DIR}/modules (plugins/replayers/sndh/CMakeLists.txt), the one
# $SELECTA_PLUGIN_PATH entry of the render test. sndh-render-missing runs the same render with an empty directory on
# the plugin path and must fail: it proves the render test passes because of the module, not despite it.
set(SEL_SNDH_MODDIR "${CMAKE_BINARY_DIR}/modules")
set(SEL_SNDH_EMPTY "${CMAKE_CURRENT_BINARY_DIR}/empty")
file(MAKE_DIRECTORY "${SEL_SNDH_EMPTY}")
set(SEL_SNDH_SONG "${CMAKE_CURRENT_SOURCE_DIR}/oculus_ice.sndh")

add_test(NAME sndh-render
  COMMAND "${CMAKE_COMMAND}" "-DCLI=$<TARGET_FILE:selecta-cli>" "-DMODDIR=${SEL_SNDH_MODDIR}"
    "-DSONG=${SEL_SNDH_SONG}" "-DOUT=${CMAKE_CURRENT_BINARY_DIR}/sndh.wav" "-DHASH=15b8505cf6309b7a41f698211a08519bbb99cfb206df7c380fd64a22056e53dc"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/render.cmake")
add_test(NAME sndh-render-missing
  COMMAND "${CMAKE_COMMAND}" "-DCLI=$<TARGET_FILE:selecta-cli>" "-DMODDIR=${SEL_SNDH_EMPTY}" "-DEXPECT_FAIL=1"
    "-DSONG=${SEL_SNDH_SONG}" "-DOUT=${CMAKE_CURRENT_BINARY_DIR}/sndh-missing.wav"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/render.cmake")
add_test(NAME sndh-nm
  COMMAND "${CMAKE_COMMAND}" "-DBINS=$<TARGET_FILE:selecta-cli>|$<TARGET_FILE:selecta>" -P "${CMAKE_CURRENT_SOURCE_DIR}/nm.cmake")

# I427b part 2, the desktop reachability test: a SNDH dropped on the desktop UI (the real open path) plays through the
# sc68 module (replayer=sc68, RP_RENDER calls, a non-silent null output) and, with the module absent, raises
# the no-player toast. Label sndh-open, not dui: dui-ratchet counts dui tests.
if(TARGET selecta)
  add_test(NAME sndh-open
    COMMAND "${CMAKE_COMMAND}" "-DSELECTA=$<TARGET_FILE:selecta>" "-DSONG=${SEL_SNDH_SONG}" "-DMODDIR=${SEL_SNDH_MODDIR}"
      "-DEMPTY=${SEL_SNDH_EMPTY}" "-DWORK=${CMAKE_BINARY_DIR}/tests/sndh-open"
      -P "${PROJECT_SOURCE_DIR}/tests/dui/sndh_open.cmake")
  set_tests_properties(sndh-open PROPERTIES LABELS sndh-open TIMEOUT 120)
endif()
