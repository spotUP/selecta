# Local file browse (plan M1.1 phase 4). The directory iterator is the only
# OS-specific part and lives under src/core/<os>/ (rule 3); macOS and Linux
# share the POSIX file.
if(SEL_AMIGA)
  set(SEL_DIR_SOURCE ${PROJECT_SOURCE_DIR}/src/core/amiga/dir.c)
else()
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
target_sources(selecta_library PRIVATE db/db.c db/schema.c db/worker.c query.c)
target_include_directories(selecta_library PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/db)
target_link_libraries(selecta_library PUBLIC selecta_sqlite)
if(SEL_AMIGA)
  target_compile_definitions(selecta_library PRIVATE SEL_LIBRARY_NO_THREAD)
endif()
