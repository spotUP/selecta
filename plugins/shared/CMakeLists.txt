# Internal helper library shared by several plugins (RULES.md rule 3, I228). Not public SDK: only the plugins link it,
# and cmake/sel_check_includes.cmake whitelists this directory next to include/selecta. Include root is plugins/shared,
# so a plugin writes #include "paula/paula_voices.h" or #include "stream/stream_pcm.h".
#   paula/   Paula register bus and voice sink (paula, paula_shadow, paula14 outputs; integer only, runs in the audio task)
#   viz/     header-only pixel helpers and descriptor of the viz plugins (sel_surface clear, rect, line)
#   stream/  header-only helpers of the stream decoders (ID3 tags, PCM resampling for minimp3 and stb_vorbis)
add_library(selecta_plugin_shared STATIC paula/paula_voices.c paula/paula_hw.c)
target_include_directories(selecta_plugin_shared PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_include_directories(selecta_plugin_shared PRIVATE "${PROJECT_SOURCE_DIR}/include")
