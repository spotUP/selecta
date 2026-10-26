# Local file browse (plan M1.1 phase 4). The directory iterator is the only
# OS-specific part and lives under src/core/<os>/ (rule 3); macOS and Linux
# share the POSIX file.
if(SEL_AMIGA)
  set(SEL_DIR_SOURCE ${PROJECT_SOURCE_DIR}/src/core/amiga/dir.c)
  set(SEL_NET_SOURCE ${PROJECT_SOURCE_DIR}/src/core/amiga/net.c)
  set(SEL_WATCH_SOURCE ${PROJECT_SOURCE_DIR}/src/core/amiga/watch.c)
else()
  if(APPLE)
    set(SEL_WATCH_SOURCE ${PROJECT_SOURCE_DIR}/src/core/macos/watch.c)
  else()
    set(SEL_WATCH_SOURCE ${PROJECT_SOURCE_DIR}/src/core/linux/watch.c)
  endif()
  set(SEL_NET_SOURCE ${PROJECT_SOURCE_DIR}/src/core/posix/net.c)
  set(SEL_DIR_SOURCE ${PROJECT_SOURCE_DIR}/src/core/macos/dir.c)
endif()
# Playlist codecs (plan M1.2 phase 3): portable C, no OS code.
add_library(selecta_library STATIC local.c ${SEL_DIR_SOURCE}
  playlist/playlist.c playlist/formats/hippo.c playlist/formats/m3u.c playlist/formats/pls.c playlist/formats/xspf.c export/wav.c export/svx.c)
target_include_directories(selecta_library PUBLIC ${PROJECT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR})

# Library database and worker (plan 2026-10-25-m3-library.md Phase 1, D-M3-1/D-M3-2): vendored
# SQLite, schema v1 with migrations, the memsys5 heap and the worker with its SPSC queues. The
# Amiga build runs requests on the caller's task until the library subtask (Phase 12).
add_subdirectory(${PROJECT_SOURCE_DIR}/third_party/sqlite ${CMAKE_BINARY_DIR}/third_party/sqlite)
target_sources(selecta_library PRIVATE db/amiga_vfs.c db/db.c db/index.c db/schema.c db/worker.c db/search.c db/scan.c db/user.c db/folders.c db/fetch.c db/rating_push.c db/vote_push.c db/lyrics_fetch.c db/job.c db/genre.c db/art.c db/songdb.c db/similar.c db/duplicates.c devilbox_loudness.c devilbox_lyrics.c stream_open.c record_name.c latin1.c meta_utf8.c radio.c composer.c xxh32.c lyrics.c query.c regex.c share.c)
target_include_directories(selecta_library PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/db)
target_link_libraries(selecta_library PUBLIC selecta_sqlite)
if(SEL_AMIGA)
  target_compile_definitions(selecta_library PRIVATE SEL_LIBRARY_NO_THREAD)
endif()

# Network (plan 2026-10-25-m3-library.md Phase 3, D-M3-4): the socket table with the guard and host
# map (one file per OS under src/core/) and the HTTP/1.1 client on top of it.
target_sources(selecta_library PRIVATE ${SEL_NET_SOURCE} net/netpolicy.c net/http.c net/tls.c net/ftp.c net/mirrors.c net/devilbox.c md5.c cache.c)
if(SEL_AMIGA)
  # Plain HTTP and FTP on the Amiga (D-M3-4): no TLS library, tls.c compiles its stubs.
  target_compile_definitions(selecta_library PRIVATE SEL_NO_TLS)
else()
  add_subdirectory(${PROJECT_SOURCE_DIR}/third_party/mbedtls ${CMAKE_BINARY_DIR}/third_party/mbedtls)
  target_link_libraries(selecta_library PUBLIC selecta_mbedtls)
endif()

# Sources (plan Phase 4, I157, I158): the sel_source interface and registry, song keys, the Modland
# format table and the local source (scan over sel_browse).
target_sources(selecta_library PRIVATE source.c formats_table.c sources/local.c sources/modland.c sources/modland_sync.c sources/modland_net.c sources/hvsc.c sources/hvsc_sync.c sources/hvsc_net.c sources/asma.c sources/asma_sync.c sources/asma_net.c sources/sndh.c sources/sndh_sync.c sources/sndh_net.c sources/zxart.c sources/zxart_sync.c sources/zxart_net.c sources/keygenmusic.c sources/keygenmusic_sync.c sources/keygenmusic_net.c sources/json_scan.c sources/url_path.c sources/sync_net.c sources/sync_services.c sources/hippo.c sources/hippo_sync.c sources/hippo_prg.c sources/hippo_publish.c sources/archive.c sources/archive_lzx.c sources/archive_pp20.c ${SEL_WATCH_SOURCE})
if(APPLE)
  target_link_libraries(selecta_library PUBLIC "-framework CoreServices")
endif()

# Archive adapters (I159, D-M3-7): vendored miniz (zip) and lhasa (lha/lzh), memory in, memory out.
add_library(selecta_archive STATIC
  ${PROJECT_SOURCE_DIR}/third_party/miniz/miniz.c
  ${PROJECT_SOURCE_DIR}/third_party/lhasa/lib/crc16.c
  ${PROJECT_SOURCE_DIR}/third_party/lhasa/lib/ext_header.c
  ${PROJECT_SOURCE_DIR}/third_party/lhasa/lib/lh1_decoder.c
  ${PROJECT_SOURCE_DIR}/third_party/lhasa/lib/lh5_decoder.c
  ${PROJECT_SOURCE_DIR}/third_party/lhasa/lib/lh6_decoder.c
  ${PROJECT_SOURCE_DIR}/third_party/lhasa/lib/lh7_decoder.c
  ${PROJECT_SOURCE_DIR}/third_party/lhasa/lib/lha_arch_unix.c
  ${PROJECT_SOURCE_DIR}/third_party/lhasa/lib/lha_basic_reader.c
  ${PROJECT_SOURCE_DIR}/third_party/lhasa/lib/lha_decoder.c
  ${PROJECT_SOURCE_DIR}/third_party/lhasa/lib/lha_endian.c
  ${PROJECT_SOURCE_DIR}/third_party/lhasa/lib/lha_file_header.c
  ${PROJECT_SOURCE_DIR}/third_party/lhasa/lib/lha_input_stream.c
  ${PROJECT_SOURCE_DIR}/third_party/lhasa/lib/lha_reader.c
  ${PROJECT_SOURCE_DIR}/third_party/lhasa/lib/lhx_decoder.c
  ${PROJECT_SOURCE_DIR}/third_party/lhasa/lib/lk7_decoder.c
  ${PROJECT_SOURCE_DIR}/third_party/lhasa/lib/lz5_decoder.c
  ${PROJECT_SOURCE_DIR}/third_party/lhasa/lib/lzs_decoder.c
  ${PROJECT_SOURCE_DIR}/third_party/lhasa/lib/macbinary.c
  ${PROJECT_SOURCE_DIR}/third_party/lhasa/lib/null_decoder.c
  ${PROJECT_SOURCE_DIR}/third_party/lhasa/lib/pm1_decoder.c
  ${PROJECT_SOURCE_DIR}/third_party/lhasa/lib/pm2_decoder.c)
target_include_directories(selecta_archive PUBLIC ${PROJECT_SOURCE_DIR}/third_party/miniz
  ${PROJECT_SOURCE_DIR}/third_party/lhasa/lib/public PRIVATE ${PROJECT_SOURCE_DIR}/third_party/lhasa/lib)
# Third-party source: no project warning flags. miniz without stdio and time (memory in, memory out).
target_compile_options(selecta_archive PRIVATE -w)
target_compile_definitions(selecta_archive PUBLIC MINIZ_NO_ZLIB_COMPATIBLE_NAMES)
target_link_libraries(selecta_library PUBLIC selecta_archive)
