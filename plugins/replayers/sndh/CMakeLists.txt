# SNDH / SC68 replayer (I427b, design 2026-10-30-plugin-loader-sdk D5). GPL-3 (vendored sc68), so it only ever
# builds as a loadable module, and only when SELECTA_GPL_PLUGINS=ON. The core binaries never link it.
add_subdirectory("${PROJECT_SOURCE_DIR}/third_party/sc68" "${CMAKE_BINARY_DIR}/third_party/sc68")

set(SEL_MODULE_OUTPUT_DIR "${CMAKE_BINARY_DIR}/modules")
sel_add_plugin(sndh 1 MODULE adapter.c
  THIRD_PARTY_INCLUDES "${PROJECT_SOURCE_DIR}/third_party/sc68/libsc68" "${PROJECT_SOURCE_DIR}/third_party/sc68/file68"
    "${PROJECT_SOURCE_DIR}/third_party/sc68/config")
target_link_libraries(sel_module_sndh PRIVATE selecta_sc68)
