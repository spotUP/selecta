# bbmp maxtrax replayer (I710k): .mxtx, bbmp's csrc/amiga/maxtrax_engine over ScummVM's MaxTrax player and Paula (GPL 3 or later,
# third_party/bbmp_maxtrax). GPL, so its own loadable module behind SELECTA_GPL_PLUGINS; the MIT core never links it.
# Host only here: the engine is C++17 on ScummVM-shaped headers; Amiga status in the ledger row I710k.
enable_language(CXX)
set(MX_DIR "${PROJECT_SOURCE_DIR}/third_party/bbmp_maxtrax")
set(SEL_MODULE_OUTPUT_DIR "${CMAKE_BINARY_DIR}/modules")
sel_add_plugin(bbmp_maxtrax 1 MODULE adapter.c shim.cpp "${MX_DIR}/maxtrax_engine.cpp"
  "${MX_DIR}/audio/mods/maxtrax.cpp" "${MX_DIR}/audio/mods/paula.cpp"
  THIRD_PARTY_INCLUDES "${MX_DIR}" "${MX_DIR}/shim")
target_compile_definitions(sel_module_bbmp_maxtrax PRIVATE ENABLE_KYRA)  # ScummVM compiles maxtrax.h only with this
target_compile_options(sel_module_bbmp_maxtrax PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-w>)
target_link_libraries(sel_module_bbmp_maxtrax PRIVATE ${CMAKE_CXX_IMPLICIT_LINK_LIBRARIES})
set_target_properties(sel_module_bbmp_maxtrax PROPERTIES CXX_STANDARD 17)
