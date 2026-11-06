# bbmp psycle replayer (I710u): Psycle songs (.psy). The engine is Psycle's own player code (r12005, GPL-2.0-or-later) with
# bbmp's helper glue (GPL-3.0-or-later) and 64 of Psycle's native plugins, built by third_party/bbmp_psycle/build.sh into a
# folder "bbmp_psycle/" beside the module (bin/bbmp-psycle + lib/psycle-plugins); the module runs the helper as a child
# process (the helper keeps process-wide singletons). GPL, so a loadable module under SELECTA_GPL_PLUGINS; the MIT core never
# links it. Host only: C++14 engine with dlopen'd plugin libraries (exemption in ledger row I710u, RULES 4a).
set(BBPSY_DIR "${PROJECT_SOURCE_DIR}/third_party/bbmp_psycle")
set(SEL_MODULE_OUTPUT_DIR "${CMAKE_BINARY_DIR}/modules")
set(BBPSY_STAGE "${SEL_MODULE_OUTPUT_DIR}/bbmp_psycle")
file(GLOB_RECURSE BBPSY_SOURCES CONFIGURE_DEPENDS "${BBPSY_DIR}/src/*" "${BBPSY_DIR}/bbmp/*" "${PROJECT_SOURCE_DIR}/third_party/patches/bbmp_psycle/*")
add_custom_command(OUTPUT "${BBPSY_STAGE}/bin/bbmp-psycle"
  COMMAND /bin/sh -c "JOBS=2 nice -n 19 \"${BBPSY_DIR}/build.sh\" \"${BBPSY_STAGE}\" \"${CMAKE_CURRENT_BINARY_DIR}/work\" > \"${CMAKE_CURRENT_BINARY_DIR}/build.log\" 2>&1 || { tail -n 20 \"${CMAKE_CURRENT_BINARY_DIR}/build.log\"; exit 1; }"
  COMMAND ${CMAKE_COMMAND} -E remove_directory "${CMAKE_CURRENT_BINARY_DIR}/work"
  COMMAND ${CMAKE_COMMAND} -E copy "${PROJECT_SOURCE_DIR}/third_party/bbmp_psycle/COPYING" "${BBPSY_STAGE}/COPYING.bbmp"
  DEPENDS "${BBPSY_DIR}/build.sh" ${BBPSY_SOURCES}
  COMMENT "bbmp_psycle: building the Psycle helper and its 64 native plugins (a few minutes)" VERBATIM)
add_custom_target(bbmp_psycle_helper DEPENDS "${BBPSY_STAGE}/bin/bbmp-psycle")

sel_add_plugin(bbmp_psycle 1 MODULE adapter.c)
target_link_libraries(sel_module_bbmp_psycle PRIVATE ${CMAKE_DL_LIBS})
add_dependencies(sel_module_bbmp_psycle bbmp_psycle_helper)
# The helper folder travels into Selecta.app/Contents/PlugIns/bbmp_psycle (apps/selecta/CMakeLists.txt).
set_target_properties(sel_module_bbmp_psycle PROPERTIES SEL_MODULE_DATA_DIR "${BBPSY_STAGE}")
