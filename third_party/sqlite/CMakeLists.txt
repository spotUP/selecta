# Vendored SQLite amalgamation (D-M3-1). Same source on desktop and 68k.
add_library(selecta_sqlite STATIC sqlite3.c)
target_include_directories(selecta_sqlite PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(selecta_sqlite PRIVATE
  SQLITE_ENABLE_FTS5 SQLITE_ENABLE_MEMSYS5 SQLITE_OMIT_LOAD_EXTENSION
  SQLITE_THREADSAFE=0 SQLITE_DEFAULT_MEMSTATUS=0 SQLITE_DQS=0
  SQLITE_OMIT_DEPRECATED SQLITE_OMIT_SHARED_CACHE SQLITE_OMIT_WAL SQLITE_MAX_MMAP_SIZE=0
  SQLITE_TEMP_STORE=3 SQLITE_DEFAULT_PCACHE_INITSZ=0)
# Third-party source: no project warning flags (the -Werror build must not trip on it).
target_compile_options(selecta_sqlite PRIVATE -w)
if(SEL_AMIGA)
  target_sources(selecta_sqlite PRIVATE amiga_stubs.c)
  # libnix has no inherited stdio to protect: SQLite's fd < 3 guard reopens /dev/null, which is the volume "dev:" (requester).
  target_compile_definitions(selecta_sqlite PRIVATE SQLITE_MINIMUM_FILE_DESCRIPTOR=0)
endif()
