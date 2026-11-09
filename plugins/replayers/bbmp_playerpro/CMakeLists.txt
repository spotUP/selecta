# bbmp PlayerPRO replayer (I710aq): PlayerPRO MAD songs (.mad). The engine is bbmp's csrc/pc/playerpro_engine.c (GPL-3.0-or-later) on PlayerPROCore, PlayerPRO's
# own driver (public domain, third_party/bbmp_playerpro/core). GPL because of the engine file, so its own loadable module under SELECTA_GPL_PLUGINS.
# Host only (RULES 4a exemption in ledger row I710aq).
set(BBPP_DIR "${PROJECT_SOURCE_DIR}/third_party/bbmp_playerpro")
set(BBPP_CORE "${BBPP_DIR}/core")
set(SEL_MODULE_OUTPUT_DIR "${CMAKE_BINARY_DIR}/modules")
set(BBPP_SOURCES
  "${BBPP_DIR}/playerpro_engine.c")
foreach(f DelayOutPut Effects MainDriver FileUtils Interrupt MADDriver MyDebugStr OutPut8 realft TickRemover stub-VSTPlugIn MIDI-Hardware-Stub)
  list(APPEND BBPP_SOURCES "${BBPP_CORE}/${f}.c")
endforeach()
# the three importers all define FillPlug; each gets its own name, as bbmp's build-pc.sh does
foreach(p MADfg MADH MADI)
  list(APPEND BBPP_SOURCES "${BBPP_CORE}/Import-Export/${p}.c")
  set_source_files_properties("${BBPP_CORE}/Import-Export/${p}.c" PROPERTIES COMPILE_DEFINITIONS "FillPlug=FillPlug_${p}")
endforeach()
if(APPLE)
  list(APPEND BBPP_SOURCES "${BBPP_DIR}/playerpro_mac.c")
endif()
sel_add_plugin(bbmp_playerpro 1 MODULE adapter.c ${BBPP_SOURCES} THIRD_PARTY_INCLUDES "${BBPP_DIR}" "${BBPP_CORE}" "${BBPP_DIR}/inc")
set_source_files_properties(${BBPP_SOURCES} PROPERTIES COMPILE_OPTIONS "-w;-std=gnu17;-fno-strict-aliasing")
target_compile_definitions(sel_module_bbmp_playerpro PRIVATE __UNIX__ __LITTLE_ENDIAN__=1 BUILDINGPPRO EMBEDPLUGS=1)
if(NOT APPLE)
  target_compile_definitions(sel_module_bbmp_playerpro PRIVATE MADCFReadStreamType=0x7fff)
endif()
if(APPLE)
  target_link_libraries(sel_module_bbmp_playerpro PRIVATE "-framework CoreFoundation")
endif()
target_link_libraries(sel_module_bbmp_playerpro PRIVATE m)
