# GoatTracker 1 and 2 replayer (I710al): GoatTracker 2.77's own play routine and song loader (third_party/goattracker, GPL-2.0) with reSID (GPL-2.0-or-later)
# under Selecta's own glue (gt_engine.cpp, GPL-2.0-or-later). GPL, so its own loadable module under SELECTA_GPL_PLUGINS. Host only: C++ reSID, global
# engine state (exemption in row I710al).
enable_language(CXX)
set(GT_DIR "${PROJECT_SOURCE_DIR}/third_party/goattracker")
set(SEL_MODULE_OUTPUT_DIR "${CMAKE_BINARY_DIR}/modules")
file(GLOB GT_RESID "${GT_DIR}/resid/*.cpp")
set(GT_SRC "${GT_DIR}/gplay.c" "${GT_DIR}/gsong.c" "${GT_DIR}/gtable.c" ${GT_RESID})
# -ffp-contract=off keeps reSID's arithmetic identical across hosts.
sel_add_plugin(goattracker 1 MODULE adapter.c gt_engine.cpp ${GT_SRC} THIRD_PARTY_INCLUDES "${GT_DIR}" "${GT_DIR}/resid" "${CMAKE_CURRENT_SOURCE_DIR}")
set_source_files_properties(gt_engine.cpp ${GT_SRC} PROPERTIES COMPILE_OPTIONS "-w;-ffp-contract=off")
target_link_libraries(sel_module_goattracker PRIVATE ${CMAKE_CXX_IMPLICIT_LINK_LIBRARIES})
set_target_properties(sel_module_goattracker PROPERTIES CXX_STANDARD 17)
