# bbmp ned replayer (I1223c): NerdTracker II songs (.ned) through bbmp's csrc/retro/ned_engine.cpp (GPL-3.0-or-later), which builds an NSF image
# around NT2's replay routine and plays it with the existing selecta_gme (Game Music Emu), no second libgme copy. The replay routine is giftware
# (non-commercial use only) and is NOT built or shipped: it is two I710ga system files fetched at use. GPL engine, so its own loadable module
# under SELECTA_GPL_PLUGINS. Host only (C++17 engine, no Amiga module path).
enable_language(CXX)
set(BBNED_DIR "${PROJECT_SOURCE_DIR}/third_party/bbmp_ned")
set(SEL_MODULE_OUTPUT_DIR "${CMAKE_BINARY_DIR}/modules")
# ned_engine.cpp includes <gme/gme.h>: third_party/ is the include root that has gme/gme.h.
sel_add_plugin(bbmp_ned 1 MODULE adapter.c "${BBNED_DIR}/bned_glue.cpp"
  THIRD_PARTY_INCLUDES "${BBNED_DIR}" "${PROJECT_SOURCE_DIR}/third_party")
set_source_files_properties("${BBNED_DIR}/bned_glue.cpp" PROPERTIES COMPILE_OPTIONS "-w;-fvisibility=hidden")
target_link_libraries(sel_module_bbmp_ned PRIVATE selecta_gme ${CMAKE_CXX_IMPLICIT_LINK_LIBRARIES})
set_target_properties(sel_module_bbmp_ned PROPERTIES CXX_STANDARD 17)
