# mdxmini replayer (I699): the mdxmini library (third_party/mdxmini, GPL-2.0-or-later, plain C) behind a RENDER adapter
# for Sharp X68000 .mdx songs (+ the .pdx sample bank next to them). GPL, so it only ever builds as a loadable module,
# and only when SELECTA_GPL_PLUGINS=ON; the core binaries never link it.
# Host only: the Amiga build has no GPL module path (exemption in ledger row I699, RULES 4a).
set(MDXMINI_DIR "${PROJECT_SOURCE_DIR}/third_party/mdxmini/src")

# Old-style C with its own warning set; the adapter keeps Selecta's. No USE_NLG (the NLG logger is not part of the build).
add_library(selecta_mdxmini STATIC
  "${MDXMINI_DIR}/mdx2151.c" "${MDXMINI_DIR}/mdxfile.c" "${MDXMINI_DIR}/mdxmini.c" "${MDXMINI_DIR}/mdxmml_ym2151.c"
  "${MDXMINI_DIR}/pcm8.c" "${MDXMINI_DIR}/pdxfile.c" "${MDXMINI_DIR}/ym2151.c")
target_include_directories(selecta_mdxmini SYSTEM PUBLIC "${MDXMINI_DIR}")
target_compile_options(selecta_mdxmini PRIVATE -w -fvisibility=hidden)
set_target_properties(selecta_mdxmini PROPERTIES C_STANDARD 11 C_EXTENSIONS ON POSITION_INDEPENDENT_CODE ON)

set(SEL_MODULE_OUTPUT_DIR "${CMAKE_BINARY_DIR}/modules")
sel_add_plugin(mdxmini 1 MODULE adapter.c)
target_link_libraries(sel_module_mdxmini PRIVATE selecta_mdxmini)
