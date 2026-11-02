# Vendored SQLite amalgamation (D-M3-1). Same source on desktop and 68k.
add_library(selecta_sqlite STATIC sqlite3.c)
# SQLITE_TEMP_STORE=2: temporary tables and sorts stay in memory by default; a connection can PRAGMA temp_store=FILE.
# With 3 the sorter never spills (mxPmaSize stays 0) and a 594k-song CREATE INDEX held every record in the heap (I787a):
# sel_schema_migrate switches to FILE for the migration only.
target_include_directories(selecta_sqlite PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(selecta_sqlite PRIVATE
  SQLITE_ENABLE_FTS5 SQLITE_ENABLE_MEMSYS5 SQLITE_OMIT_LOAD_EXTENSION
  SQLITE_THREADSAFE=0 SQLITE_DEFAULT_MEMSTATUS=0 SQLITE_DQS=0
  SQLITE_OMIT_DEPRECATED SQLITE_OMIT_SHARED_CACHE SQLITE_OMIT_WAL SQLITE_MAX_MMAP_SIZE=0
  SQLITE_TEMP_STORE=2 SQLITE_DEFAULT_PCACHE_INITSZ=0)
# Third-party source: no project warning flags (the -Werror build must not trip on it).
target_compile_options(selecta_sqlite PRIVATE -w)
if(SEL_AMIGA)
  target_sources(selecta_sqlite PRIVATE amiga_stubs.c)
  # libnix has no inherited stdio to protect: SQLite's fd < 3 guard reopens /dev/null, which is the volume "dev:" (requester).
  target_compile_definitions(selecta_sqlite PRIVATE SQLITE_MINIMUM_FILE_DESCRIPTOR=0)
  # I225: read and write through pread/pwrite (amiga_stubs.c). SQLite's lseek fallback seeks past the end of the
  # file, and libnix lseek grows the file with junk there.
  target_compile_definitions(selecta_sqlite PRIVATE HAVE_PREAD=1 HAVE_PWRITE=1)
endif()

# Host-only sqlite3 CLI for the tests (shell.c, same 3.54.1 release as sqlite3.c). The runner's system sqlite3 has no FTS5.
# It links selecta_sqlite, so it gets exactly the library's SQLITE_ENABLE_* options without a second list.
if(NOT SEL_AMIGA)
  add_executable(sel_sqlite3_shell shell.c)
  target_link_libraries(sel_sqlite3_shell PRIVATE selecta_sqlite)
  # The library is built without extension loading; the shell must be told so it does not call those functions.
  target_compile_definitions(sel_sqlite3_shell PRIVATE SQLITE_OMIT_LOAD_EXTENSION SQLITE_OMIT_DEPRECATED SQLITE_DQS=0)
  target_compile_options(sel_sqlite3_shell PRIVATE -w)
  if(CMAKE_DL_LIBS)
    target_link_libraries(sel_sqlite3_shell PRIVATE ${CMAKE_DL_LIBS})
  endif()
endif()
