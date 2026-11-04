# CHIPNSFX replayer (I432): CNGSoft's tracker code (third_party/chipnsfx, GPL-3.0-or-later) behind a RENDER adapter. GPL, so
# it only ever builds as a loadable module, and only when SELECTA_GPL_PLUGINS=ON; the core binaries never link it.
# Host only: the Amiga build has no GPL module path (exemption in ledger row I432, RULES 4a).
set(CHIPNSFX_DIR "${PROJECT_SOURCE_DIR}/third_party/chipnsfx")

# The tracker is old-style C with its own warning set; the adapter keeps Selecta's.
add_library(selecta_chipnsfx STATIC engine.c)
target_include_directories(selecta_chipnsfx PRIVATE "${CHIPNSFX_DIR}")
target_include_directories(selecta_chipnsfx PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_options(selecta_chipnsfx PRIVATE -w -fvisibility=hidden)
set_target_properties(selecta_chipnsfx PROPERTIES C_STANDARD 11 C_EXTENSIONS ON POSITION_INDEPENDENT_CODE ON)

set(SEL_MODULE_OUTPUT_DIR "${CMAKE_BINARY_DIR}/modules")
sel_add_plugin(chipnsfx 1 MODULE adapter.c THIRD_PARTY_INCLUDES "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(sel_module_chipnsfx PRIVATE selecta_chipnsfx)
