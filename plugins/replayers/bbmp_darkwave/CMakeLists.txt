# bbmp darkwave replayer (I710h): DarkWave Studio projects (.dwp). The engine is DarkWave Studio 4.1.6's own machines (ES-CoreMachines,
# DarkPlug; Daniel Werner, GPL 3 or later, sources preprocessed by bbmp's tools/build-darkwave.sh and vendored in third_party/bbmp_darkwave/src)
# with bbmp's studio port (studio.cpp) and our in-process entry dw_engine.cpp. GPL, so its own loadable module under SELECTA_GPL_PLUGINS;
# the MIT core never links it. Host only: C++14 engine on SSE2 intrinsics (sse2neon, MIT, on arm64), no Amiga module path (exemption in
# row I710h). The machines' MinBLEP table (a DLL resource upstream) is linked in as a generated C array.
enable_language(CXX)
set(BBDW_DIR "${PROJECT_SOURCE_DIR}/third_party/bbmp_darkwave")
set(SEL_MODULE_OUTPUT_DIR "${CMAKE_BINARY_DIR}/modules")

# minblep.dat -> const char DarkWaveMinBLEP[] (configure time, regenerated when the data changes)
set(BBDW_TABLE_C "${CMAKE_CURRENT_BINARY_DIR}/minblep_table.c")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${BBDW_DIR}/minblep.dat")
file(READ "${BBDW_DIR}/minblep.dat" BBDW_HEX HEX)
string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," BBDW_BYTES "${BBDW_HEX}")
file(WRITE "${BBDW_TABLE_C}" "const char DarkWaveMinBLEP[] __attribute__((aligned(16))) = {${BBDW_BYTES}};\n")

file(GLOB BBDW_SOURCES "${BBDW_DIR}/src/*.cpp")
set(BBDW_ENGINE_SOURCES ${BBDW_SOURCES} "${BBDW_DIR}/studio.cpp" "${BBDW_DIR}/dw_engine.cpp")
set(BBDW_INCLUDES "${BBDW_DIR}/compat" "${BBDW_DIR}/src" "${BBDW_DIR}")
if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64|ARM64")
  list(INSERT BBDW_INCLUDES 0 "${BBDW_DIR}/sse2neon")
endif()
if(APPLE)
  list(INSERT BBDW_INCLUDES 0 "${BBDW_DIR}/darwin")
endif()
sel_add_plugin(bbmp_darkwave 1 MODULE adapter.c ${BBDW_ENGINE_SOURCES} "${BBDW_TABLE_C}"
  THIRD_PARTY_INCLUDES ${BBDW_INCLUDES})
set_source_files_properties(${BBDW_ENGINE_SOURCES} PROPERTIES
  COMPILE_OPTIONS "-w;-fpermissive;-include;${BBDW_DIR}/compat/windows.h"
  COMPILE_DEFINITIONS "DW_HEADLESS;COREMACHINES;NDEBUG")
target_link_libraries(sel_module_bbmp_darkwave PRIVATE ${CMAKE_CXX_IMPLICIT_LINK_LIBRARIES})
set_target_properties(sel_module_bbmp_darkwave PROPERTIES CXX_STANDARD 14 CXX_EXTENSIONS ON)
