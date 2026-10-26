# Internal helper library shared by several plugins (RULES.md rule 3, I228). Not public SDK: only the plugins link it,
# and cmake/sel_check_includes.cmake whitelists this directory next to include/selecta. Include root is plugins/shared,
# so a plugin writes #include "paula/paula_voices.h" or #include "stream/stream_pcm.h".
#   paula/   Paula register bus and voice sink (paula, paula_shadow, paula14 outputs; integer only, runs in the audio task)
#   viz/     header-only pixel helpers and descriptor of the viz plugins (sel_surface clear, rect, line)
#   stream/  header-only helpers of the stream decoders (ID3 tags, PCM resampling for minimp3 and stb_vorbis)
add_library(selecta_plugin_shared STATIC paula/paula_voices.c paula/paula_hw.c)
target_include_directories(selecta_plugin_shared PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_include_directories(selecta_plugin_shared PRIVATE "${PROJECT_SOURCE_DIR}/include")

# I517 phase 2: the pure helpers the core sources and the source plugins (plugins/sources/<id>) both need. One copy
# each, linked by selecta_library and by every plugin that uses them.
#   source/   json_scan, url_path (incl. sel_url_unquote), latin1 (sel_latin1_to_utf8), sync_text.h (chomp, safe path)
#   archive/  zip / lha / lzx / pp20 adapters over the vendored miniz and lhasa (selecta_archive)
add_library(selecta_shared_source STATIC source/json_scan.c source/url_path.c source/latin1.c)
target_include_directories(selecta_shared_source PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_include_directories(selecta_shared_source PRIVATE "${PROJECT_SOURCE_DIR}/include")
add_library(selecta_shared_archive STATIC archive/archive.c archive/archive_lzx.c archive/archive_pp20.c)
target_include_directories(selecta_shared_archive PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_include_directories(selecta_shared_archive PRIVATE "${PROJECT_SOURCE_DIR}/include")
target_link_libraries(selecta_shared_archive PUBLIC selecta_archive)
