# bbmp Ad Lib helper formats (I710as): Pixel Painters, SIG2, Lemmings, First Samurai, Shadowlands, Vibrants, Sagitta FM Tracker through bbmp's csrc/adlib engines (GPL-3.0-or-later,
# clean room) on AdPlug's Nuked OPL3 (adplug_engine, LGPL, from replayers/adplug, so that directory must be added first). GPL, so its own loadable module
# under SELECTA_GPL_PLUGINS. Host only: C++17 engines (exemption in row I710as).
enable_language(CXX)
set(BBADLIB_DIR "${PROJECT_SOURCE_DIR}/third_party/bbmp_adlib")
set(BBADLIB_ENGINES
  "${BBADLIB_DIR}/ppf_cr.cpp" "${BBADLIB_DIR}/sig_cr.cpp" "${BBADLIB_DIR}/lem_cr.cpp"
  "${BBADLIB_DIR}/mus_cr.cpp" "${BBADLIB_DIR}/cal_cr.cpp" "${BBADLIB_DIR}/vib_cr.cpp" "${BBADLIB_DIR}/fmt_cr.cpp")
set(SEL_MODULE_OUTPUT_DIR "${CMAKE_BINARY_DIR}/modules")
sel_add_plugin(bbmp_adlib 1 MODULE adapter.c shim.cpp ${BBADLIB_ENGINES} THIRD_PARTY_INCLUDES "${BBADLIB_DIR}")
# Our include/adplug/*.h forward bbmp's <adplug/opl.h> and <adplug/nemuopl.h> to the AdPlug sources.
target_include_directories(sel_module_bbmp_adlib PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
# -ffp-contract=off: bbmp checks the engines bit for bit; no fused multiply-add.
set_source_files_properties(${BBADLIB_ENGINES} shim.cpp PROPERTIES COMPILE_OPTIONS "-w;-ffp-contract=off")
target_link_libraries(sel_module_bbmp_adlib PRIVATE adplug_engine ${CMAKE_CXX_IMPLICIT_LINK_LIBRARIES})
# shim.cpp converts CP437 song texts with iconv: libSystem has it on Linux (glibc), macOS needs libiconv.
if(APPLE)
  target_link_libraries(sel_module_bbmp_adlib PRIVATE iconv)
endif()
set_target_properties(sel_module_bbmp_adlib PROPERTIES CXX_STANDARD 17)
