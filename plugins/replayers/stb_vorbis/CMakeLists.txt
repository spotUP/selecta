# selecta_stb_vorbis: the vendored stb_vorbis.c (third_party/stb_vorbis) compiled once as a static library with
# the vendor's own warning set. Memory input only (no stdio); the push-data API serves live streams (I195d). The adapter includes the
# header-only view of the same file (STB_VORBIS_HEADER_ONLY) and sees nothing else outside include/selecta (T7).
add_library(selecta_stb_vorbis STATIC "${PROJECT_SOURCE_DIR}/third_party/stb_vorbis/stb_vorbis.c")
target_include_directories(selecta_stb_vorbis PUBLIC "${PROJECT_SOURCE_DIR}/third_party/stb_vorbis")
target_compile_definitions(selecta_stb_vorbis PRIVATE STB_VORBIS_NO_STDIO)
set_target_properties(selecta_stb_vorbis PROPERTIES COMPILE_OPTIONS "-Wall")

sel_add_plugin(stb_vorbis 1 adapter.c THIRD_PARTY_INCLUDES "${PROJECT_SOURCE_DIR}/third_party/stb_vorbis")
target_link_libraries(sel_plugin_stb_vorbis PUBLIC selecta_stb_vorbis)

# I213a: Amiga FPU tier. -m68881 -ffast-math makes gcc inline sin, cos, exp and log; fpu_math.h (force-included)
# replaces floor, ldexp and pow, the libm calls that would return doubles in d0:d1 against fp0. Checked on the
# objects by tests/m3/stb_vorbis_fpu.cmake (nm -u). The registry hides the decoder (SEL_RP_NEEDS_FLOAT) on a machine
# without an FPU. -O2 because gcc inlines the 68881 sin, cos, exp and log only when optimising (the default Amiga
# build is -O0, which would leave the libm calls). Never applied to the engine or to nuade.
if(SEL_AMIGA)
  set(_sv_fpu -O2 -m68881 -ffast-math -include "${CMAKE_CURRENT_SOURCE_DIR}/fpu_math.h")
  target_compile_options(selecta_stb_vorbis PRIVATE ${_sv_fpu})
  target_compile_options(sel_plugin_stb_vorbis PRIVATE ${_sv_fpu})
endif()
