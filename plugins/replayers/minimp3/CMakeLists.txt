# selecta_minimp3: the vendored single header (third_party/minimp3) compiled once as a static library with the
# vendor's own warning set; the project-wide -Werror -pedantic flags are for Selecta code. The adapter (Selecta
# warning set) sees include/selecta, the minimp3 header directory and nothing else (T7).
add_library(selecta_minimp3 STATIC "${PROJECT_SOURCE_DIR}/third_party/minimp3/minimp3_impl.c")
target_include_directories(selecta_minimp3 PUBLIC "${PROJECT_SOURCE_DIR}/third_party/minimp3")
set_target_properties(selecta_minimp3 PROPERTIES COMPILE_OPTIONS "-Wall")

sel_add_plugin(minimp3 1 adapter.c THIRD_PARTY_INCLUDES "${PROJECT_SOURCE_DIR}/third_party/minimp3")
target_link_libraries(sel_plugin_minimp3 PUBLIC selecta_minimp3)

# I213: this float decoder runs on the FPU when one is fitted. The registry hides it (SEL_RP_NEEDS_FLOAT) on a
# machine without one, so FPU code never executes there. Only the float decoders get -m68881, never the engine.
if(SEL_AMIGA)
  target_compile_options(selecta_minimp3 PRIVATE -m68881)
  target_compile_options(sel_plugin_minimp3 PRIVATE -m68881)
endif()
