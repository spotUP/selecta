# bbmp kdm replayer (I710j): Ken's Digital Music songs (.kdm). The engine is bbmp's csrc/synth/kdm_engine.c (GPL-3.0-or-later), a
# clean-room replayer in C. GPL, so its own loadable module under SELECTA_GPL_PLUGINS; the MIT core never links it. Host only
# (exemption in row I710j). The instruments come from waves.kwv next to the song (SEL_RP_COMPANIONS fetches it beside a remote song).
set(BBKDM_DIR "${PROJECT_SOURCE_DIR}/third_party/bbmp_kdm")
set(SEL_MODULE_OUTPUT_DIR "${CMAKE_BINARY_DIR}/modules")
sel_add_plugin(bbmp_kdm 1 MODULE adapter.c "${BBKDM_DIR}/kdm_engine.c" THIRD_PARTY_INCLUDES "${BBKDM_DIR}")
set_source_files_properties("${BBKDM_DIR}/kdm_engine.c" PROPERTIES COMPILE_OPTIONS "-w")
