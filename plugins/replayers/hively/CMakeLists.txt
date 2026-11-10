# Hively replayer (I1242j): AHX / THX and Hively (.hvl) songs through the HivelyTracker replayer by Pete Gordon
# (third_party/hively, BSD 3-Clause, so the module is MIT-compatible and stays in a SELECTA_RELEASE_MIT_ONLY bundle).
# Amiga: static plugin, soft-float objects (no -m68881, see organya), SEL_RP_NEEDS_FLOAT hides it on a machine without an FPU.
set(HVL_DIR "${PROJECT_SOURCE_DIR}/third_party/hively")
add_library(hively_engine STATIC EXCLUDE_FROM_ALL ${HVL_DIR}/hvl_replay.c ${HVL_DIR}/hvl_tables.c)
target_include_directories(hively_engine PUBLIC "${HVL_DIR}")
target_compile_options(hively_engine PRIVATE -w)

if(SEL_AMIGA)
  set_target_properties(hively_engine PROPERTIES EXCLUDE_FROM_ALL OFF)
  sel_add_plugin(hively 1 adapter.c THIRD_PARTY_INCLUDES "${HVL_DIR}")
  target_link_libraries(sel_plugin_hively PUBLIC hively_engine m)
else()
  set_target_properties(hively_engine PROPERTIES POSITION_INDEPENDENT_CODE ON C_VISIBILITY_PRESET hidden)
  set(SEL_MODULE_OUTPUT_DIR "${CMAKE_BINARY_DIR}/modules")
  sel_add_plugin(hively 1 MODULE adapter.c THIRD_PARTY_INCLUDES "${HVL_DIR}")
  target_link_libraries(sel_module_hively PRIVATE hively_engine m)
  set_property(TARGET sel_module_hively PROPERTY SEL_MODULE_MIT ON)
endif()
