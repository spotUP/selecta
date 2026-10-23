# T96 amiga-library-vamos (plan 2026-10-25-m3-library.md, Phase 12, I197/I143, D-M3-16).
# The desktop `selecta-cli library export-index` builds the fixture index; the 68k selecta-cli under vamos
# then runs `library search` and `library stats` against a writable user DB plus that read-only index, on the
# 68020/2 MB tier (--machine a1200, library heap 512 KB). It prints db_heap_peak= sqlite_text_bytes=
# query_ms=. db_heap_peak must fit the heap the tier gets (heap_bytes=). sqlite_text_bytes is the code size
# of the SQLite static library, measured here with the cross size tool (the CLI cannot report its own text).
# Inputs: MODE=vamos VAMOS AMIGA_CLI CLI WORK SRC_DIR (tests/m3) AMIGA_SIZE (optional) AMIGA_BUILD_DIR
include("${CMAKE_CURRENT_LIST_DIR}/lib.cmake")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
set(VAMOS_VOLUMES -V work:${WORK})

# Fixture on the desktop: main DB with all rows, user DB with the local rows, index with the modland rows.
file(READ "${SRC_DIR}/catalogue.tsv" cat)
string(REGEX REPLACE "(^|\n)modland[^\n]*" "" local "${cat}")
file(WRITE "${WORK}/local.tsv" "${local}")
foreach(spec "all;catalogue.tsv" "user;local.tsv")
  list(GET spec 0 name)
  list(GET spec 1 tsv)
  set(src "${SRC_DIR}/${tsv}")
  if(tsv STREQUAL "local.tsv")
    set(src "${WORK}/local.tsv")
  endif()
  execute_process(COMMAND "${CLI}" userdata --catalogue "${src}" --db "${WORK}/${name}.db"
                  RESULT_VARIABLE rc OUTPUT_QUIET ERROR_QUIET)
  if(NOT rc EQUAL 0)
    message(FATAL_ERROR "RED[T96]: fixture ${name}.db not built (rc=${rc}).")
  endif()
endforeach()
execute_process(COMMAND "${CLI}" library export-index --db "${WORK}/all.db" --out "${WORK}/idx.db" modland
                OUTPUT_VARIABLE exp RESULT_VARIABLE rc ERROR_VARIABLE experr)
if(NOT rc EQUAL 0 OR NOT exp MATCHES "index_rows=3")
  message(FATAL_ERROR "RED[T96]: 'library export-index' did not write 3 rows (rc=${rc}): ${exp}${experr}")
endif()

# Search on the 68k: a bare word served by FTS from the index, and a compiled-SQL query over both files.
m2_exec(out rc library search --db work:user.db --index work:idx.db --machine a1200 -- purple)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "RED[T96]: 'library search --index' exited ${rc} on 68k.\n${out}")
endif()
m2_check(T96 "${out}" "key==modland:p/purple.xm,src_count==1,fts_queries==1,index_songs==3,db_heap_peak>0")
m2_get("${out}" heap_bytes heap_bytes)
m2_get("${out}" db_heap_peak search_peak)
m2_get("${out}" query_ms search_ms)
if(search_peak GREATER heap_bytes)
  message(FATAL_ERROR "RED[T96]: search db_heap_peak=${search_peak} exceeds the tier heap ${heap_bytes}.")
endif()

m2_exec(out2 rc2 library search --db work:user.db --index work:idx.db --machine a1200 -- year:1995..1997)
m2_check(T96 "${out2}" "src_count==3,fts_queries==0")
if(NOT out2 MATCHES "key=modland:p/purple.xm" OR NOT out2 MATCHES "key=modland:j/jester.it" OR NOT out2 MATCHES "key=selecta-test-mk.mod")
  message(FATAL_ERROR "RED[T96]: year:1995..1997 missed rows across user DB and index.\n${out2}")
endif()

m2_exec(out3 rc3 library stats --db work:user.db --index work:idx.db --machine a1200)
if(NOT rc3 EQUAL 0)
  message(FATAL_ERROR "RED[T96]: 'library stats --index' exited ${rc3} on 68k.\n${out3}")
endif()
m2_check(T96 "${out3}" "songs==5,index_songs==3,heap_bytes==524288,db_heap_peak>0")
m2_get("${out3}" db_heap_peak stats_peak)
if(stats_peak GREATER 524288)
  message(FATAL_ERROR "RED[T96]: stats db_heap_peak=${stats_peak} exceeds the 512 KB tier heap.")
endif()

# An index far larger than the 512 KB tier heap must still be readable: the page cache of the ATTACHed index is
# capped, else count(*) over it ends in SQLITE_NOMEM (found by I198 on the 516k-row Modland index).
set(big_rows 20000)
set(big_listing "")
foreach(n RANGE 1 ${big_rows})
  string(APPEND big_listing "${n}0\tProtracker/Artist ${n}/some long song name number ${n}.mod\n")
endforeach()
file(MAKE_DIRECTORY "${WORK}/big")
file(WRITE "${WORK}/big/allmods_md5.txt" "${big_listing}")
execute_process(COMMAND "${CLI}" library sync --db "${WORK}/bigsrc.db" modland "${WORK}/big"
                OUTPUT_VARIABLE bigsync RESULT_VARIABLE rc)
execute_process(COMMAND "${CLI}" library export-index --db "${WORK}/bigsrc.db" --out "${WORK}/bigidx.db" modland
                OUTPUT_VARIABLE bigexp RESULT_VARIABLE rc2)
if(NOT rc EQUAL 0 OR NOT rc2 EQUAL 0 OR NOT bigexp MATCHES "index_rows=${big_rows}")
  message(FATAL_ERROR "RED[T96]: big index not built (rc=${rc}/${rc2}): ${bigsync}${bigexp}")
endif()
m2_exec(out4 rc4 library stats --db work:user.db --index work:bigidx.db --machine a1200)
if(NOT rc4 EQUAL 0)
  message(FATAL_ERROR "RED[T96]: 'library stats' over a ${big_rows}-row index exited ${rc4} on 68k.\n${out4}")
endif()
m2_check(T96 "${out4}" "index_songs==${big_rows},db_heap_peak>0")

# Multi-hit searches over that index (I198a): an FTS prefix matching every row (3 letters, served by the prefix
# index) and a compiled field query (full scan of the index) must return hits inside the 512 KB heap, not ERROR
# out_of_memory/bad_value. A prefix of 4+ letters (`song`, `long`) searches the 3-letter prefix index and
# sel_fts_prefix() checks the whole word (I198b); FTS5 would otherwise merge every doclist in memory.
foreach(q "som" "artist:artist" "song" "long")
  m2_exec(out5 rc5 library search --db work:user.db --index work:bigidx.db --machine a1200 -- ${q})
  # vamos itself logs "path: ERROR: error removing temp dir" when another vamos shares ~/.vamos; the CLI prints
  # "ERROR <code>".
  if(NOT rc5 EQUAL 0 OR out5 MATCHES "(^|\n)ERROR [a-z_]+")
    message(FATAL_ERROR "RED[T96]: multi-hit search '${q}' over a ${big_rows}-row index failed (rc=${rc5}) on 68k.\n${out5}")
  endif()
  m2_check(T96 "${out5}" "index_songs==${big_rows},src_count>0,db_heap_peak>0")
  m2_get("${out5}" db_heap_peak multi_peak)
  m2_get("${out5}" query_ms multi_ms)
  message(STATUS "T96 '${q}' db_heap_peak=${multi_peak} query_ms=${multi_ms}")
  if(multi_peak GREATER 524288)
    message(FATAL_ERROR "RED[T96]: search '${q}' db_heap_peak=${multi_peak} exceeds the 512 KB tier heap.")
  endif()
endforeach()

# sqlite_text_bytes: code size of the SQLite static library in the Amiga build.
set(sqlite_text "unmeasured")
if(AMIGA_SIZE AND AMIGA_BUILD_DIR)
  file(GLOB_RECURSE libs "${AMIGA_BUILD_DIR}/third_party/sqlite/*.a")
  if(libs)
    list(GET libs 0 lib)
    execute_process(COMMAND "${AMIGA_SIZE}" -t "${lib}" OUTPUT_VARIABLE sz ERROR_QUIET)
    string(REGEX MATCH "\n[ \t]*([0-9]+)[ \t]+[0-9]+[ \t]+[0-9]+[ \t]+[0-9]+[ \t]+[0-9a-f]+[ \t]+\\(TOTALS\\)" m "${sz}")
    if(CMAKE_MATCH_1)
      set(sqlite_text "${CMAKE_MATCH_1}")
    endif()
  endif()
endif()
message(STATUS "T96 db_heap_peak=${search_peak} sqlite_text_bytes=${sqlite_text} query_ms=${search_ms} heap_bytes=${heap_bytes}")
message(STATUS "[OK] T96 ${MODE}")
