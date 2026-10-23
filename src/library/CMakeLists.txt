# Local file browse (plan M1.1 phase 4). The directory iterator is the only
# OS-specific part and lives under src/core/<os>/ (rule 3); macOS and Linux
# share the POSIX file.
if(SEL_AMIGA)
  set(SEL_DIR_SOURCE ${PROJECT_SOURCE_DIR}/src/core/amiga/dir.c)
  set(SEL_NET_SOURCE ${PROJECT_SOURCE_DIR}/src/core/amiga/net.c)
else()
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
target_sources(selecta_library PRIVATE db/db.c db/schema.c db/worker.c db/search.c db/scan.c query.c regex.c)
target_include_directories(selecta_library PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/db)
target_link_libraries(selecta_library PUBLIC selecta_sqlite)
if(SEL_AMIGA)
  target_compile_definitions(selecta_library PRIVATE SEL_LIBRARY_NO_THREAD)
endif()

# Network (plan 2026-10-25-m3-library.md Phase 3, D-M3-4): the socket table with the guard and host
# map (one file per OS under src/core/) and the HTTP/1.1 client on top of it.
target_sources(selecta_library PRIVATE ${SEL_NET_SOURCE} net/netpolicy.c net/http.c net/tls.c net/ftp.c net/mirrors.c md5.c cache.c)
if(SEL_AMIGA)
  # Plain HTTP and FTP on the Amiga (D-M3-4): no TLS library, tls.c compiles its stubs.
  target_compile_definitions(selecta_library PRIVATE SEL_NO_TLS)
else()
  add_subdirectory(${PROJECT_SOURCE_DIR}/third_party/mbedtls ${CMAKE_BINARY_DIR}/third_party/mbedtls)
  target_link_libraries(selecta_library PUBLIC selecta_mbedtls)
endif()

# Sources (plan Phase 4, I157, I158): the sel_source interface and registry, song keys, the Modland
# format table and the local source (scan over sel_browse).
target_sources(selecta_library PRIVATE source.c formats_table.c sources/local.c sources/modland.c)
