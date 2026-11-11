# Organya replayer (I703): Studio Pixel's Organya songs (.org, Cave Story) through organya.h v1.03 by Strultz
# (third_party/organya, ISC-style licence, so the module is MIT-compatible and stays in a SELECTA_RELEASE_MIT_ONLY bundle).
# No soundbank is vendored (Cave Story's wave100.wdb has no open licence); see third_party/organya/VERSION.txt.
# Amiga (I703a): soft-float objects (no -m68881, see minimp3/pxtone), SEL_RP_NEEDS_FLOAT ranks it lower on a machine without an FPU (I1316).
# An on-demand LoadSeg module (I427e4, the bbmp_ptk pattern): Players/sel_organya holds the replayer; the registry links
# only probe.c as the manifest (header probe "Org-0x", no replayer code in any Amiga executable).
set(ORG_DIR "${PROJECT_SOURCE_DIR}/third_party/organya")
add_library(organya_engine STATIC EXCLUDE_FROM_ALL impl.c)
target_include_directories(organya_engine PUBLIC "${ORG_DIR}")
target_compile_options(organya_engine PRIVATE -w)
target_compile_definitions(organya_engine PUBLIC ORG_USE_STDINT ORG_NO_STDIO)

if(SEL_AMIGA)
  # No -fPIC on 68k (the assembler rejects the PLT/GOT forms); the LoadSeg module links the engine statically.
  set_target_properties(organya_engine PROPERTIES EXCLUDE_FROM_ALL OFF)
  sel_add_plugin(organya 1 adapter.c probe.c AMIGA_MANIFEST probe.c THIRD_PARTY_INCLUDES "${ORG_DIR}")
  target_link_libraries(sel_seg_organya PRIVATE organya_engine m)
else()
  set_target_properties(organya_engine PROPERTIES POSITION_INDEPENDENT_CODE ON C_VISIBILITY_PRESET hidden)
  set(SEL_MODULE_OUTPUT_DIR "${CMAKE_BINARY_DIR}/modules")
  sel_add_plugin(organya 1 MODULE adapter.c probe.c THIRD_PARTY_INCLUDES "${ORG_DIR}")
  target_link_libraries(sel_module_organya PRIVATE organya_engine m)
  set_property(TARGET sel_module_organya PROPERTY SEL_MODULE_MIT ON)
endif()
