# SID replayer (I434): libsidplayfp (GPL-2) with the reSIDfp engine (libresidfp, GPL-2), the engine bbmp uses. GPL, so
# it only ever builds as a loadable module, and only when SELECTA_GPL_PLUGINS=ON; the core binaries never link it.
# Host only: the Amiga SID path is a native 68k library (row I434a), not this plugin.
enable_language(CXX)
set(SIDPLAYFP_DIR "${PROJECT_SOURCE_DIR}/third_party/libsidplayfp/src")
set(RESIDFP_DIR "${PROJECT_SOURCE_DIR}/third_party/libresidfp/src")

file(GLOB SEL_SIDPLAYFP_SOURCES CONFIGURE_DEPENDS
  ${SIDPLAYFP_DIR}/*.cpp ${SIDPLAYFP_DIR}/c64/*.cpp ${SIDPLAYFP_DIR}/c64/CIA/*.cpp ${SIDPLAYFP_DIR}/c64/CPU/*.cpp
  ${SIDPLAYFP_DIR}/c64/VIC_II/*.cpp ${SIDPLAYFP_DIR}/sidplayfp/*.cpp ${SIDPLAYFP_DIR}/sidtune/*.cpp
  ${SIDPLAYFP_DIR}/builders/residfp-builder/*.cpp)
file(GLOB SEL_RESIDFP_SOURCES CONFIGURE_DEPENDS ${RESIDFP_DIR}/*.cpp ${RESIDFP_DIR}/residfp/*.cpp ${RESIDFP_DIR}/resample/*.cpp)

add_library(selecta_sidplayfp STATIC ${SEL_SIDPLAYFP_SOURCES} ${SEL_RESIDFP_SOURCES})
target_include_directories(selecta_sidplayfp PUBLIC "${SIDPLAYFP_DIR}" "${RESIDFP_DIR}")
target_compile_definitions(selecta_sidplayfp PRIVATE HAVE_CONFIG_H)
target_compile_options(selecta_sidplayfp PRIVATE -w -fvisibility=hidden)
set_target_properties(selecta_sidplayfp PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF
  POSITION_INDEPENDENT_CODE ON)

set(SEL_MODULE_OUTPUT_DIR "${CMAKE_BINARY_DIR}/modules")
sel_add_plugin(sid 1 MODULE adapter.c shim.cpp THIRD_PARTY_INCLUDES "${SIDPLAYFP_DIR}" "${RESIDFP_DIR}")
target_link_libraries(sel_module_sid PRIVATE selecta_sidplayfp ${CMAKE_CXX_IMPLICIT_LINK_LIBRARIES})
