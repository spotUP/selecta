# TIATracker replayer (I429): TIATracker's song and replay code with Stella's TIA sound (third_party/tiatracker,
# GPL-2.0-only, C++17 with bbmp's Qt stand-in) behind a RENDER adapter for Atari 2600 .ttt songs. GPL, so it only ever
# builds as a loadable module, and only when SELECTA_GPL_PLUGINS=ON; the core binaries never link it.
# Host only: the Amiga build has no GPL module path (exemption in ledger row I429, RULES 4a).
enable_language(CXX)
set(TIA_DIR "${PROJECT_SOURCE_DIR}/third_party/tiatracker")

add_library(selecta_tiatracker STATIC engine.cpp
  "${TIA_DIR}/shim/qjson.cpp"
  "${TIA_DIR}/track/instrument.cpp" "${TIA_DIR}/track/note.cpp" "${TIA_DIR}/track/pattern.cpp"
  "${TIA_DIR}/track/percussion.cpp" "${TIA_DIR}/track/sequence.cpp" "${TIA_DIR}/track/sequenceentry.cpp"
  "${TIA_DIR}/track/track.cpp" "${TIA_DIR}/tiasound/tiasound.cpp"
  "${TIA_DIR}/emulation/player.cpp" "${TIA_DIR}/emulation/TIASnd.cpp")
target_include_directories(selecta_tiatracker PRIVATE "${TIA_DIR}/shim" "${TIA_DIR}" "${TIA_DIR}/track" "${TIA_DIR}/tiasound")
target_include_directories(selecta_tiatracker PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_options(selecta_tiatracker PRIVATE -w -fvisibility=hidden)
set_target_properties(selecta_tiatracker PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS ON
  POSITION_INDEPENDENT_CODE ON)

set(SEL_MODULE_OUTPUT_DIR "${CMAKE_BINARY_DIR}/modules")
sel_add_plugin(tiatracker 1 MODULE adapter.c THIRD_PARTY_INCLUDES "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(sel_module_tiatracker PRIVATE selecta_tiatracker)
