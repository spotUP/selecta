# 98fmplayer replayer (I700): PC-98 PMD and FMP (PLAY6) songs through takamichih's fmdriver + libopna (third_party/98fmplayer,
# BSD-2-Clause, plain C99) behind a RENDER adapter. Built as a loadable module under SELECTA_GPL_PLUGINS like every bbmp
# port; the licence itself would allow the core, the gate keeps the one rule for the whole replayer track.
# Host only: the Amiga build has no module path for these (exemption in ledger row I700, RULES 4a).
set(FMP98_DIR "${PROJECT_SOURCE_DIR}/third_party/98fmplayer")

file(GLOB SEL_FMP98_SOURCES CONFIGURE_DEPENDS "${FMP98_DIR}/fmdriver/*.c" "${FMP98_DIR}/libopna/*.c")
list(FILTER SEL_FMP98_SOURCES EXCLUDE REGEX "s98gen\\.c$")

file(GLOB SEL_FMP98_LEVEL CONFIGURE_DEPENDS "${FMP98_DIR}/leveldata/*.c" "${FMP98_DIR}/oscillo/*.c")
list(APPEND SEL_FMP98_SOURCES ${SEL_FMP98_LEVEL})
add_library(selecta_98fmplayer STATIC ${SEL_FMP98_SOURCES})
target_include_directories(selecta_98fmplayer SYSTEM PUBLIC "${FMP98_DIR}" "${FMP98_DIR}/fmdriver" "${FMP98_DIR}/libopna")
target_compile_options(selecta_98fmplayer PRIVATE -w -fvisibility=hidden)
set_target_properties(selecta_98fmplayer PROPERTIES C_STANDARD 11 C_EXTENSIONS ON POSITION_INDEPENDENT_CODE ON)

set(SEL_MODULE_OUTPUT_DIR "${CMAKE_BINARY_DIR}/modules")
sel_add_plugin(fmp98 1 MODULE adapter.c THIRD_PARTY_INCLUDES "${FMP98_DIR}")
target_link_libraries(sel_module_fmp98 PRIVATE selecta_98fmplayer)
