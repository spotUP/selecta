# pxtone replayer (I702): Pixel's PiSTOL Collage songs (.ptcop, .pttune) through the pxtone play sample sources
# (third_party/pxtone). The licence is permissive (free use and modification, no permission or attribution needed), so this
# is MIT-compatible core: a loadable module on the host, an on-demand LoadSeg module on the Amiga (I427e4, the hively/bbmp_ptk
# pattern): Players/sel_pxtone holds adapter.c, shim.cpp, the C++ engine and libstdc++; the registry links only probe.c as
# the manifest (PTCOLLAGE-/PTTUNE-- header probe, no engine code in any Amiga executable).
enable_language(CXX)
set(PX_DIR "${PROJECT_SOURCE_DIR}/third_party/pxtone")
file(GLOB PX_SOURCES "${PX_DIR}/*.cpp")
add_library(pxtone_engine STATIC EXCLUDE_FROM_ALL ${PX_SOURCES})
target_include_directories(pxtone_engine PUBLIC "${PX_DIR}/..")
target_compile_options(pxtone_engine PRIVATE -w -fvisibility=hidden)
set_target_properties(pxtone_engine PROPERTIES CXX_STANDARD 14)
if(NOT SEL_AMIGA) # -fPIC makes m68k gcc 6.5 hit an internal compiler error (print_operand_address2); Amiga links statically
  set_target_properties(pxtone_engine PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()

if(SEL_AMIGA)
  # The song data path uses float and libm (SEL_RP_NEEDS_FLOAT hides it on a machine without an FPU). Soft-float objects:
  # no -m68881 here, because -m68881 returns doubles in fp0 while libm returns d0:d1 (see minimp3).
  set_target_properties(pxtone_engine PROPERTIES EXCLUDE_FROM_ALL OFF)
  sel_add_plugin(pxtone 1 adapter.c probe.c shim.cpp AMIGA_MANIFEST probe.c THIRD_PARTY_INCLUDES "${PX_DIR}/..")
  # The module links with the C driver: libstdc++ (operator new/delete, std::nothrow) is named here.
  target_link_libraries(sel_seg_pxtone PRIVATE pxtone_engine stdc++ m)
  set_target_properties(sel_seg_pxtone PROPERTIES CXX_STANDARD 14)
  # I1378a seg-imports-unit (tests/amiga/run_seg_imports.cmake): two copies of the module that also call sel_tag_find
  # through the import thunks. v2 carries the real import count and must render exactly like the plain module; over
  # claims one import more than the host has and must be rejected before init. Not in SEL_SEG_MODULES: never shipped.
  foreach(variant v2 over)
    set(count_arg "")
    if(variant STREQUAL "over")
      math(EXPR over_n "${SEL_SEG_IMPORT_COUNT} + 1")
      set(count_arg IMPORT_COUNT ${over_n})
    endif()
    sel_add_seg(sel_seg_pxtone_${variant} pxtone 1 "${CMAKE_BINARY_DIR}/seg-imports-test/${variant}" IMPORTS ${count_arg}
      SOURCES adapter.c probe.c shim.cpp "${PROJECT_SOURCE_DIR}/tests/amiga/seg_imports_probe.c"
      INCLUDES "${CMAKE_CURRENT_SOURCE_DIR}" "${PX_DIR}/..")
    target_link_libraries(sel_seg_pxtone_${variant} PRIVATE pxtone_engine stdc++ m)
    set_target_properties(sel_seg_pxtone_${variant} PROPERTIES CXX_STANDARD 14 EXCLUDE_FROM_ALL ON)
  endforeach()
else()
  set(SEL_MODULE_OUTPUT_DIR "${CMAKE_BINARY_DIR}/modules")
  sel_add_plugin(pxtone 1 MODULE adapter.c probe.c shim.cpp THIRD_PARTY_INCLUDES "${PX_DIR}/..")
  target_link_libraries(sel_module_pxtone PRIVATE pxtone_engine ${CMAKE_CXX_IMPLICIT_LINK_LIBRARIES})
  set_target_properties(sel_module_pxtone PROPERTIES CXX_STANDARD 14)
endif()
