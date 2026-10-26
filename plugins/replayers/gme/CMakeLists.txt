# selecta_gme: libgme 0.6.5 (third_party/gme, LGPL 2.1) as a static C++ library for the adapter plugin (row I426).
# C++ is enabled for this target only; Selecta's own C code keeps -Wall -Wextra -Werror -pedantic. The vendored
# sources use upstream's warning set. Emulators: AY GBS GYM HES KSS NSF NSFE SAP SPC (third_party/gme/gme_types.h);
# no VGM (libvgm owns it, I431), no zlib. The GYM core is the GENS YM2612 (VGM_YM2612_GENS).
enable_language(CXX)
set(GME_DIR "${PROJECT_SOURCE_DIR}/third_party/gme")
file(GLOB SEL_GME_SOURCES CONFIGURE_DEPENDS "${GME_DIR}/*.cpp")
# VRC7 (NES FM) uses the bundled emu2413 (MIT, C).
list(APPEND SEL_GME_SOURCES "${GME_DIR}/ext/emu2413.c")

add_library(selecta_gme STATIC ${SEL_GME_SOURCES})
target_include_directories(selecta_gme PUBLIC "${GME_DIR}")
target_compile_definitions(selecta_gme PRIVATE VGM_YM2612_GENS)
target_compile_options(selecta_gme PRIVATE -Wall -fvisibility=hidden)
set_target_properties(selecta_gme PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)

# The adapter (C99, Selecta warning set) sees include/selecta and libgme's public header directory only (T7).
sel_add_plugin(gme 1 adapter.c THIRD_PARTY_INCLUDES "${GME_DIR}")
target_link_libraries(sel_plugin_gme PUBLIC selecta_gme)
target_link_libraries(selecta_gme PUBLIC ${CMAKE_CXX_IMPLICIT_LINK_LIBRARIES})

# libgme needs the byte order spelled out (upstream's CMake probes it the same way).
if(CMAKE_CXX_BYTE_ORDER STREQUAL "BIG_ENDIAN")
  target_compile_definitions(selecta_gme PRIVATE BLARGG_BIG_ENDIAN=1)
else()
  target_compile_definitions(selecta_gme PRIVATE BLARGG_LITTLE_ENDIAN=1)
endif()
