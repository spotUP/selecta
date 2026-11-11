# selecta_stb_vorbis: the vendored stb_vorbis.c (third_party/stb_vorbis) compiled once as a static library with
# the vendor's own warning set. Memory input only (no stdio); the push-data API serves live streams (I195d). The adapter includes the
# header-only view of the same file (STB_VORBIS_HEADER_ONLY) and sees nothing else outside include/selecta (T7).
add_library(selecta_stb_vorbis STATIC "${PROJECT_SOURCE_DIR}/third_party/stb_vorbis/stb_vorbis.c")
target_include_directories(selecta_stb_vorbis PUBLIC "${PROJECT_SOURCE_DIR}/third_party/stb_vorbis")
target_compile_definitions(selecta_stb_vorbis PRIVATE STB_VORBIS_NO_STDIO)
set_target_properties(selecta_stb_vorbis PROPERTIES COMPILE_OPTIONS "-Wall")

# I427e4: on the Amiga the decoder is the LoadSeg module Players/sel_stb_vorbis (adapter.c + selecta_stb_vorbis, FPU
# objects); the registry links only the probe.c manifest (plain integer C, no FPU flags; the hively pattern, ea6eed5a).
if(SEL_AMIGA)
  sel_add_plugin(stb_vorbis 1 adapter.c probe.c AMIGA_MANIFEST probe.c
    THIRD_PARTY_INCLUDES "${PROJECT_SOURCE_DIR}/third_party/stb_vorbis")
  target_link_libraries(sel_seg_stb_vorbis PRIVATE selecta_stb_vorbis m)
  set(_sv_fpu -O2 -m68881 -ffast-math -include "${CMAKE_CURRENT_SOURCE_DIR}/fpu_math.h")
  target_compile_options(selecta_stb_vorbis PRIVATE ${_sv_fpu})
  target_compile_options(sel_seg_stb_vorbis PRIVATE ${_sv_fpu})
else()
  sel_add_plugin(stb_vorbis 1 adapter.c probe.c THIRD_PARTY_INCLUDES "${PROJECT_SOURCE_DIR}/third_party/stb_vorbis")
  target_link_libraries(sel_plugin_stb_vorbis PUBLIC selecta_stb_vorbis)
  target_link_libraries(sel_plugin_stb_vorbis PUBLIC selecta_plugin_shared)
endif()
