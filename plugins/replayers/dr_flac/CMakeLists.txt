# selecta_dr_flac: the vendored single header (third_party/dr_flac) compiled once as a static library with the
# vendor's own warning set; the project-wide -Werror -pedantic flags are for Selecta code. Memory input only
# (DR_FLAC_NO_STDIO). The adapter (Selecta warning set) sees include/selecta, the dr_flac header directory and
# nothing else (T7). Integer decoder: no float, so no SEL_RP_NEEDS_FLOAT and no FPU flags (I436).
add_library(selecta_dr_flac STATIC "${PROJECT_SOURCE_DIR}/third_party/dr_flac/dr_flac_impl.c")
target_include_directories(selecta_dr_flac PUBLIC "${PROJECT_SOURCE_DIR}/third_party/dr_flac")
set_target_properties(selecta_dr_flac PROPERTIES COMPILE_OPTIONS "-Wall")

sel_add_plugin(dr_flac 1 adapter.c THIRD_PARTY_INCLUDES "${PROJECT_SOURCE_DIR}/third_party/dr_flac")
target_link_libraries(sel_plugin_dr_flac PUBLIC selecta_dr_flac)
target_link_libraries(sel_plugin_dr_flac PUBLIC selecta_plugin_shared)
