# ZXTune replayer (I421): Spectrum and chiptune trackers (Pro Tracker 1/2/3, Sound Tracker, ASC, SQ Tracker, Pro Sound
# Creator, Vortex, PSG, Sam Coupe COP, Digital Studio, TurboFM, ...) through ZXTune (LGPL-3.0), built from the upstream
# revision bbmp uses (ZXTUNE_REVISION) with bbmp's patches. The adapter and wrapper are GPL-3.0-or-later (LICENSE), so
# this only ever builds as a loadable module, and only when SELECTA_GPL_PLUGINS=ON; the core binaries never link it.
# Host only: the Amiga cost is measured in thoughts/spot/agent-reports/I421.md.
#
# ZXTune is far too big to vendor (about 650 MB with its sample corpus). Point SELECTA_ZXTUNE_SRC at a clean checkout of
# github.com/vitamin-caig/zxtune at the revision in ZXTUNE_REVISION; without it the plugin is skipped with a notice.
# build_zxtune.sh copies the checkout into the build tree, so the checkout stays untouched.
enable_language(CXX)
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/ZXTUNE_REVISION" SEL_ZXTUNE_REVISION LIMIT_COUNT 1)
set(SELECTA_ZXTUNE_SRC "" CACHE PATH "Clean checkout of vitamin-caig/zxtune at ${SEL_ZXTUNE_REVISION}")

if(NOT SELECTA_ZXTUNE_SRC OR NOT EXISTS "${SELECTA_ZXTUNE_SRC}/src/core")
  message(STATUS "zxtune plugin skipped: set SELECTA_ZXTUNE_SRC to a zxtune checkout at ${SEL_ZXTUNE_REVISION}")
  return()
endif()

if(APPLE)
  set(SEL_ZXTUNE_PLATFORM darwin)
else()
  set(SEL_ZXTUNE_PLATFORM linux)
endif()
set(SEL_ZXTUNE_WORK "${CMAKE_CURRENT_BINARY_DIR}/zxtune")
set(SEL_ZXTUNE_LIBDIR "${SEL_ZXTUNE_WORK}/lib/${SEL_ZXTUNE_PLATFORM}/release")
set(SEL_ZXTUNE_STAMP "${CMAKE_CURRENT_BINARY_DIR}/zxtune.stamp")
set(SEL_ZXTUNE_LIBS
  analysis async binary binary_compression binary_format core core_plugins_players core_plugins_archives debug
  devices_aym devices_aym_dumper devices_beeper devices_dac devices_fm devices_saa formats_archived
  formats_archived_multitrack formats_chiptune formats_packed formats_multitrack io l10n_stub module module_conversion
  module_properties parameters platform platform_version sound strings tools hvl lazyusf2 v2m lhasa lzma)

file(GLOB SEL_ZXTUNE_BUILD_INPUTS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/patches/*.patch")
list(APPEND SEL_ZXTUNE_BUILD_INPUTS "${CMAKE_CURRENT_SOURCE_DIR}/build_zxtune.sh"
  "${CMAKE_CURRENT_SOURCE_DIR}/trim_players.py" "${CMAKE_CURRENT_SOURCE_DIR}/app.Makefile" "${CMAKE_CURRENT_SOURCE_DIR}/lazyusf2.Makefile")
add_custom_command(OUTPUT "${SEL_ZXTUNE_STAMP}"
  COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/build_zxtune.sh" "${SELECTA_ZXTUNE_SRC}" "${SEL_ZXTUNE_WORK}"
    "${CMAKE_CURRENT_SOURCE_DIR}" "${SEL_ZXTUNE_STAMP}"
  DEPENDS ${SEL_ZXTUNE_BUILD_INPUTS}
  COMMENT "Building the ZXTune libraries (long on a first build)"
  VERBATIM)
add_custom_target(zxtune_libs DEPENDS "${SEL_ZXTUNE_STAMP}")

set(SEL_MODULE_OUTPUT_DIR "${CMAKE_BINARY_DIR}/modules")
sel_add_plugin(zxtune 1 MODULE adapter.c shim.cpp THIRD_PARTY_INCLUDES
  "${SEL_ZXTUNE_WORK}" "${SEL_ZXTUNE_WORK}/include" "${SEL_ZXTUNE_WORK}/src")
add_dependencies(sel_module_zxtune zxtune_libs)
target_include_directories(sel_module_zxtune PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_options(sel_module_zxtune PRIVATE
  $<$<COMPILE_LANGUAGE:CXX>:-std=c++20 -funsigned-char -fvisibility=hidden -w
  -D_LIBCPP_ENABLE_CXX20_REMOVED_FEATURES -isystem /opt/homebrew/include
  -include "${CMAKE_CURRENT_SOURCE_DIR}/stdint-both.h">)
set(SEL_ZXTUNE_ARCHIVES "")
foreach(lib ${SEL_ZXTUNE_LIBS})
  list(APPEND SEL_ZXTUNE_ARCHIVES "${SEL_ZXTUNE_LIBDIR}/lib${lib}.a")
endforeach()
if(APPLE)
  target_link_libraries(sel_module_zxtune PRIVATE ${SEL_ZXTUNE_ARCHIVES} z ${CMAKE_CXX_IMPLICIT_LINK_LIBRARIES})
else()
  target_link_libraries(sel_module_zxtune PRIVATE -Wl,--start-group ${SEL_ZXTUNE_ARCHIVES} -Wl,--end-group z
    ${CMAKE_CXX_IMPLICIT_LINK_LIBRARIES})
endif()
