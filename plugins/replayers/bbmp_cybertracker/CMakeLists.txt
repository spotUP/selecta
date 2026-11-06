# bbmp cybertracker replayer (I710g): CyberTracker C64 songs (.ct). The engine is bbmp's csrc/retro/cybertracker_engine.cpp
# (GPL-3.0-or-later), a clean-room driver that runs the user's own CyberTracker 1.01 disk image on a 6502 + reSIDfp SID (libresidfp, GPL-2).
# Built unchanged with Selecta's glue (third_party/bbmp_cybertracker/bcybertracker_glue.cpp). GPL, so its own loadable module under
# SELECTA_GPL_PLUGINS. Host only: C++17 engine (exemption in row I710g). The tracker is not shipped.
enable_language(CXX)
set(BBCT_DIR "${PROJECT_SOURCE_DIR}/third_party/bbmp_cybertracker")
set(BBCT_RESID "${PROJECT_SOURCE_DIR}/third_party/libresidfp/src")
file(GLOB BBCT_RESID_SOURCES CONFIGURE_DEPENDS ${BBCT_RESID}/*.cpp ${BBCT_RESID}/residfp/*.cpp ${BBCT_RESID}/resample/*.cpp)
set(SEL_MODULE_OUTPUT_DIR "${CMAKE_BINARY_DIR}/modules")
sel_add_plugin(bbmp_cybertracker 1 MODULE adapter.c "${BBCT_DIR}/bcybertracker_glue.cpp" ${BBCT_RESID_SOURCES}
  THIRD_PARTY_INCLUDES "${BBCT_DIR}" "${BBCT_RESID}")
set_source_files_properties("${BBCT_DIR}/bcybertracker_glue.cpp" ${BBCT_RESID_SOURCES} PROPERTIES COMPILE_OPTIONS "-w;-fvisibility=hidden")
target_link_libraries(sel_module_bbmp_cybertracker PRIVATE ${CMAKE_CXX_IMPLICIT_LINK_LIBRARIES})
set_target_properties(sel_module_bbmp_cybertracker PROPERTIES CXX_STANDARD 17)
