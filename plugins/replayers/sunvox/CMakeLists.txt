# SunVox replayer (I706a): .sunvox songs through the BSD-licensed SunVox engine (Alex Zolotov 2002-2009,
# third_party/sunvox_bsd). Old engine: songs written by newer SunVox versions are refused (I706b covers those on the
# desktop through the precompiled libsunvox). BSD = MIT-compatible core: a loadable module on the host, an on-demand
# LoadSeg module on the Amiga (I427e4, the hively/bbmp_ptk pattern): Players/sel_sunvox holds adapter.c, the C++ engine and
# libstdc++; the registry links only probe.c as the manifest (SVOX/VERS header probe, no engine code in any Amiga
# executable). The engine mixes floats, so the plugin carries SEL_RP_NEEDS_FLOAT.
enable_language(CXX)
set(SV_DIR "${PROJECT_SOURCE_DIR}/third_party/sunvox_bsd")
file(GLOB SV_SOURCES "${SV_DIR}/sunvox_engine/*.cpp" "${SV_DIR}/sunvox_engine/psynth/*.cpp" "${SV_DIR}/shim/*.cpp")
add_library(sunvox_engine STATIC EXCLUDE_FROM_ALL ${SV_SOURCES})
target_include_directories(sunvox_engine PUBLIC "${SV_DIR}/shim" "${SV_DIR}/sunvox_engine")
target_compile_definitions(sunvox_engine PRIVATE STYPE_FLOAT)
target_compile_options(sunvox_engine PRIVATE -w -fvisibility=hidden -fno-exceptions -fno-rtti)
set_target_properties(sunvox_engine PROPERTIES CXX_STANDARD 98)
if(NOT SEL_AMIGA) # -fPIC makes m68k gcc 6.5 hit an internal compiler error; Amiga links statically
  set_target_properties(sunvox_engine PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()

if(SEL_AMIGA)
  # Soft-float objects: no -m68881 (see pxtone, minimp3).
  set_target_properties(sunvox_engine PROPERTIES EXCLUDE_FROM_ALL OFF)
  sel_add_plugin(sunvox 1 adapter.c probe.c AMIGA_MANIFEST probe.c)
  target_link_libraries(sel_seg_sunvox PRIVATE sunvox_engine stdc++ m)
else()
  set(SEL_MODULE_OUTPUT_DIR "${CMAKE_BINARY_DIR}/modules")
  sel_add_plugin(sunvox 1 MODULE adapter.c probe.c)
  target_link_libraries(sel_module_sunvox PRIVATE sunvox_engine ${CMAKE_CXX_IMPLICIT_LINK_LIBRARIES})
endif()
