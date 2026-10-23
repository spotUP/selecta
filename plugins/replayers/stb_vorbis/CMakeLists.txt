# selecta_stb_vorbis: the vendored stb_vorbis.c (third_party/stb_vorbis) compiled once as a static library with
# the vendor's own warning set. Memory input only (no stdio), no push-data API. The adapter includes the
# header-only view of the same file (STB_VORBIS_HEADER_ONLY) and sees nothing else outside include/selecta (T7).
add_library(selecta_stb_vorbis STATIC "${PROJECT_SOURCE_DIR}/third_party/stb_vorbis/stb_vorbis.c")
target_include_directories(selecta_stb_vorbis PUBLIC "${PROJECT_SOURCE_DIR}/third_party/stb_vorbis")
target_compile_definitions(selecta_stb_vorbis PRIVATE STB_VORBIS_NO_STDIO STB_VORBIS_NO_PUSHDATA_API)
set_target_properties(selecta_stb_vorbis PROPERTIES COMPILE_OPTIONS "-Wall")

sel_add_plugin(stb_vorbis 1 adapter.c THIRD_PARTY_INCLUDES "${PROJECT_SOURCE_DIR}/third_party/stb_vorbis")
target_link_libraries(sel_plugin_stb_vorbis PUBLIC selecta_stb_vorbis)

# I213: this float decoder runs on the FPU when one is fitted. The registry hides it (SEL_RP_NEEDS_FLOAT) on a
# machine without one, so FPU code never executes there. Only the float decoders get -m68881, never the engine.
if(SEL_AMIGA)
  target_compile_options(selecta_stb_vorbis PRIVATE -m68881)
  target_compile_options(sel_plugin_stb_vorbis PRIVATE -m68881)
endif()
