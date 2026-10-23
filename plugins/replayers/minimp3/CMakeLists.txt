# selecta_minimp3: the vendored single header (third_party/minimp3) compiled once as a static library with the
# vendor's own warning set; the project-wide -Werror -pedantic flags are for Selecta code. The adapter (Selecta
# warning set) sees include/selecta, the minimp3 header directory and nothing else (T7).
add_library(selecta_minimp3 STATIC "${PROJECT_SOURCE_DIR}/third_party/minimp3/minimp3_impl.c")
target_include_directories(selecta_minimp3 PUBLIC "${PROJECT_SOURCE_DIR}/third_party/minimp3")
set_target_properties(selecta_minimp3 PROPERTIES COMPILE_OPTIONS "-Wall")

sel_add_plugin(minimp3 1 adapter.c THIRD_PARTY_INCLUDES "${PROJECT_SOURCE_DIR}/third_party/minimp3")
target_link_libraries(sel_plugin_minimp3 PUBLIC selecta_minimp3)
