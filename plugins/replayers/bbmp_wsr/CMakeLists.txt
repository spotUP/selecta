# bbmp WonderSwan replayer (I710z): .wsr, bbmp's csrc/console/wsr_engine over Mednafen's WonderSwan emulation (foo_input_wsr form,
# via rePlayer), all in third_party/bbmp_wsr. Four Mednafen files are GPL 2 only, so the module is GPL and sits behind
# SELECTA_GPL_PLUGINS. Host only: C++17 emulator that forks scan workers; no Amiga module path (exemption in ledger row I710z).
enable_language(CXX)
set(WSR_DIR "${PROJECT_SOURCE_DIR}/third_party/bbmp_wsr")
set(WSR_MDFN "${WSR_DIR}/mednafen")
set(SEL_MODULE_OUTPUT_DIR "${CMAKE_BINARY_DIR}/modules")
sel_add_plugin(bbmp_wsr 1 MODULE adapter.c shim.cpp "${WSR_DIR}/wsr_engine.cpp"
  "${WSR_MDFN}/error.cpp" "${WSR_MDFN}/sound/Blip_Buffer.cpp"
  "${WSR_MDFN}/wswan/interrupt.cpp" "${WSR_MDFN}/wswan/main.cpp" "${WSR_MDFN}/wswan/memory.cpp"
  "${WSR_MDFN}/wswan/sound.cpp" "${WSR_MDFN}/wswan/v30mz.cpp" "${WSR_MDFN}/wswan/tcache.cpp" "${WSR_MDFN}/wswan/gfx.cpp"
  THIRD_PARTY_INCLUDES "${WSR_DIR}" "${WSR_MDFN}")
# gfx.cpp runs the CPU line by line: its v30mz_execute calls go to the engine, which skips the waiting for the next interrupt.
set_source_files_properties("${WSR_MDFN}/wswan/gfx.cpp" PROPERTIES COMPILE_DEFINITIONS "v30mz_execute=v30mz_execute_hooked")
target_compile_definitions(sel_module_bbmp_wsr PRIVATE LSB_FIRST MDFN_DISABLE_PICPIE_ERRWARN)
target_compile_options(sel_module_bbmp_wsr PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-w>)
target_link_libraries(sel_module_bbmp_wsr PRIVATE ${CMAKE_CXX_IMPLICIT_LINK_LIBRARIES})
set_target_properties(sel_module_bbmp_wsr PROPERTIES CXX_STANDARD 17)
