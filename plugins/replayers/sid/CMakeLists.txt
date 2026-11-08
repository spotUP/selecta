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

# Embedded OpenROMs images (D10): kernal, basic and chargen of MEGA65/open-roms (LGPL-3.0-or-later), read as hex at
# configure time and written as one generated C++ file in the build tree.
set(SEL_OPENROMS_DIR "${PROJECT_SOURCE_DIR}/third_party/openroms")
set(SEL_OPENROMS_CPP "${CMAKE_CURRENT_BINARY_DIR}/openroms.cpp")
set(SEL_OPENROMS_BODY "#include \"openroms.h\"\n")
foreach(rom kernal:kernal_generic basic:basic_generic chargen:chargen_openroms)
  string(REPLACE ":" ";" rom_parts "${rom}")
  list(GET rom_parts 0 rom_name)
  list(GET rom_parts 1 rom_file)
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${SEL_OPENROMS_DIR}/${rom_file}.rom")
  file(READ "${SEL_OPENROMS_DIR}/${rom_file}.rom" rom_hex HEX)
  string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," rom_hex "${rom_hex}")
  string(APPEND SEL_OPENROMS_BODY "const uint8_t sid_openrom_${rom_name}[] = {${rom_hex}};\n")
endforeach()
# Polly Tracker player (I710ar): the RSID header and 962 player bytes the sid plugin puts in front of a Polly module.
set(SEL_POLLY_PLAYER "${PROJECT_SOURCE_DIR}/third_party/polly_player/polly_player.bin")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${SEL_POLLY_PLAYER}")
file(READ "${SEL_POLLY_PLAYER}" polly_hex HEX)
string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," polly_hex "${polly_hex}")
string(APPEND SEL_OPENROMS_BODY "const uint8_t sid_polly_player[SID_POLLY_PLAYER_SIZE] = {${polly_hex}};\n")
file(WRITE "${SEL_OPENROMS_CPP}.tmp" "${SEL_OPENROMS_BODY}")
execute_process(COMMAND ${CMAKE_COMMAND} -E copy_if_different "${SEL_OPENROMS_CPP}.tmp" "${SEL_OPENROMS_CPP}")

set(SEL_MODULE_OUTPUT_DIR "${CMAKE_BINARY_DIR}/modules")
sel_add_plugin(sid 1 MODULE adapter.c shim.cpp "${SEL_OPENROMS_CPP}" THIRD_PARTY_INCLUDES "${SIDPLAYFP_DIR}" "${RESIDFP_DIR}")
target_include_directories(sel_module_sid PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(sel_module_sid PRIVATE selecta_sidplayfp ${CMAKE_CXX_IMPLICIT_LINK_LIBRARIES})
