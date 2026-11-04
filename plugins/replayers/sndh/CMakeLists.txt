# SNDH / SC68 replayer (I427b, design 2026-10-30-plugin-loader-sdk D5). GPL-3 (vendored sc68), so it only ever
# builds as a loadable module, and only when SELECTA_GPL_PLUGINS=ON. The core binaries never link it.
add_subdirectory("${PROJECT_SOURCE_DIR}/third_party/sc68" "${CMAKE_BINARY_DIR}/third_party/sc68")

set(SEL_MODULE_OUTPUT_DIR "${CMAKE_BINARY_DIR}/modules")
sel_add_plugin(sndh 1 MODULE adapter.c
  THIRD_PARTY_INCLUDES "${PROJECT_SOURCE_DIR}/third_party/sc68/libsc68" "${PROJECT_SOURCE_DIR}/third_party/sc68/file68"
    "${PROJECT_SOURCE_DIR}/third_party/sc68/config")
target_link_libraries(sel_module_sndh PRIVATE selecta_sc68)

# I427d: the separate GPL-3 archive (module + COPYING + sc68 source), never part of the MIT app bundle (D5).
# Build it with: cmake --build <dir> --target selecta-gpl-sndh-archive  -> <dir>/dist/selecta-sndh-gpl3.tar.gz
set(SEL_GPL_SNDH_ARCHIVE "${CMAKE_BINARY_DIR}/dist/selecta-sndh-gpl3.tar.gz")
add_custom_target(selecta-gpl-sndh-archive
  COMMAND "${CMAKE_COMMAND}" "-DMODULE=$<TARGET_FILE:sel_module_sndh>" "-DSOURCE_DIR=${PROJECT_SOURCE_DIR}/third_party/sc68"
    "-DOUT=${SEL_GPL_SNDH_ARCHIVE}" "-DSTAGE=${CMAKE_BINARY_DIR}/dist/stage-sndh"
    -P "${PROJECT_SOURCE_DIR}/cmake/gpl_archive.cmake"
  DEPENDS sel_module_sndh
  COMMENT "GPL-3 SNDH archive" VERBATIM)
