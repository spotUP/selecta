# T92 stats-screen (plan 2026-10-25-m3-library.md, I192).
#   -DCLI=<selecta-cli> -DSELECTA=<exe> -DCAT=<catalogue.tsv> -DSEL=<meta_empty.sel> -DSQLITE3=<sqlite3 shell>
#   -DKEYS=<keys> -DWORK=<dir> -DCWD=<dir holding selecta-test.mod>
# A database file is created, sqlite3 seeds songs and plays over three years (skipped plays, a play of a song
# the catalogue lacks, ties). The real selecta binary opens it, key b shows the statistics screen. The expected
# numbers are counted here by direct SQL over the base tables (history, songs), never through the stats_* views
# the screen reads, and compared with the rows of the screen dump.
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
set(db "${WORK}/s.db")
execute_process(COMMAND "${CLI}" script "${SEL}" --catalogue "${CAT}" --db "${db}"
  WORKING_DIRECTORY "${CWD}" OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "selecta-cli exited ${rc}\nstdout:\n${out}\nstderr:\n${err}")
endif()

function(sql query result_var)
  execute_process(COMMAND "${SQLITE3}" -separator "|" "${db}" "${query}"
    OUTPUT_VARIABLE o ERROR_VARIABLE e RESULT_VARIABLE r OUTPUT_STRIP_TRAILING_WHITESPACE)
  if(NOT r EQUAL 0)
    message(FATAL_ERROR "sqlite3 failed: ${e}\n${query}")
  endif()
  set(${result_var} "${o}" PARENT_SCOPE)
endfunction()

# 2022-06-01 = 1654041600, 2023-03-01 = 1677628800, 2024-05-01 = 1714521600, 2025-02-01 = 1738368000 (UTC)
string(CONCAT seed
  "PRAGMA trusted_schema = 1; DELETE FROM history; "
  "INSERT INTO songs(key, source, path, name, artist, format) VALUES"
  " ('st:a', 'local', 'a.mod', 'A', 'Jogeir', 'mod'), ('st:b', 'local', 'b.mod', 'B', 'Jogeir', 'mod'),"
  " ('st:c', 'local', 'c.xm', 'C', 'Purple', 'xm'), ('st:d', 'local', 'd.s3m', 'D', 'Purple', 's3m'),"
  " ('st:e', 'local', 'e.it', 'E', NULL, 'it'); "
  "INSERT INTO history(key, played_at, duration_ms, skipped) VALUES"
  " ('st:a', 1654041600, 61000, 0), ('st:a', 1654041700, 60000, 0), ('st:c', 1654041800, 3600000, 0),"
  " ('st:a', 1677628800, 120000, 0), ('st:b', 1677628900, 130000, 0), ('st:c', 1677629000, 140000, 0),"
  " ('st:c', 1677629100, 150000, 0), ('st:d', 1677629200, 999999, 1), ('st:e', 1677629300, 90000, 0),"
  " ('st:a', 1714521600, 200000, 0), ('st:d', 1714521700, 210000, 0), ('st:d', 1714521800, 220000, 0),"
  " ('st:gone', 1714521900, 230000, 0), ('st:b', 1714522000, 1, 1),"
  " ('st:e', 1738368000, 300000, 0), ('st:e', 1738368100, 310000, 0), ('st:b', 1738368200, NULL, 0);")
execute_process(COMMAND "${SQLITE3}" "${db}" "${seed}" RESULT_VARIABLE rc ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "sqlite3 seed failed: ${err}")
endif()

execute_process(
  COMMAND "${SELECTA}" --output null --size 80x25 --start-dir "${CWD}" --db "${db}"
          --keys "${KEYS}" --screen-dump "${WORK}/s.txt"
  WORKING_DIRECTORY "${CWD}" OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "selecta exited ${rc}\nstdout:\n${out}\nstderr:\n${err}")
endif()
if(NOT out MATCHES "stats_queries=1")
  message(FATAL_ERROR "want stats_queries=1 in:\n${out}")
endif()

# The screen as label -> value: label is columns 2-23, value from column 25.
file(STRINGS "${WORK}/s.txt" lines)
set(screen_rows "")
foreach(l IN LISTS lines)
  string(LENGTH "${l}" n)
  if(n GREATER 24)
    string(SUBSTRING "${l}" 1 22 lab)
    string(SUBSTRING "${l}" 24 -1 val)
    string(STRIP "${lab}" lab)
    string(STRIP "${val}" val)
    if(NOT lab STREQUAL "")
      list(APPEND screen_rows "${lab}|${val}")
    endif()
  endif()
endforeach()

set(want "")
function(hms ms var)
  math(EXPR s "${ms} / 1000")
  math(EXPR h "${s} / 3600")
  math(EXPR m "${s} % 3600 / 60")
  math(EXPR x "${s} % 60")
  foreach(v m x)
    if(${v} LESS 10)
      set(${v} "0${${v}}")
    endif()
  endforeach()
  set(${var} "${h}:${m}:${x}" PARENT_SCOPE)
endfunction()

sql("SELECT COUNT(*) FROM songs" n_songs)
sql("SELECT COUNT(*) FROM history WHERE skipped = 0" n_plays)
sql("SELECT COALESCE(SUM(duration_ms), 0) FROM history WHERE skipped = 0" ms_all)
hms("${ms_all}" t_all)
list(APPEND want "Songs in library|${n_songs}" "Total plays|${n_plays}" "Total play time|${t_all}")
sql("SELECT DISTINCT strftime('%Y', played_at, 'unixepoch') FROM history WHERE skipped = 0 ORDER BY 1 DESC LIMIT 3" years)
string(REPLACE "\n" ";" years "${years}")
foreach(y IN LISTS years)
  sql("SELECT COUNT(*) FROM history WHERE skipped = 0 AND strftime('%Y', played_at, 'unixepoch') = '${y}'" yp)
  sql("SELECT COALESCE(SUM(duration_ms), 0) FROM history WHERE skipped = 0 AND strftime('%Y', played_at, 'unixepoch') = '${y}'" yms)
  hms("${yms}" yt)
  list(APPEND want "${y} plays|${yp} plays, ${yt}")
  foreach(kind format artist)
    string(CONCAT q "SELECT COALESCE(NULLIF(s.${kind}, ''), 'Unknown'), COUNT(*) AS c FROM history h LEFT JOIN songs s ON s.key = h.key"
        " WHERE h.skipped = 0 AND strftime('%Y', h.played_at, 'unixepoch') = '${y}'"
        " GROUP BY 1 ORDER BY c DESC, 1 ASC LIMIT 2")
    sql("${q}" top)
    string(REPLACE "\n" ";" top "${top}")
    set(rank 1)
    foreach(t IN LISTS top)
      string(REPLACE "|" ";" parts "${t}")
      list(GET parts 0 nm)
      list(GET parts 1 cnt)
      list(APPEND want "${y} ${kind} ${rank}|${nm} (${cnt})")
      math(EXPR rank "${rank} + 1")
    endforeach()
  endforeach()
endforeach()

list(LENGTH want n_want)
list(LENGTH screen_rows n_screen)
foreach(w IN LISTS want)
  list(FIND screen_rows "${w}" at)
  if(at LESS 0)
    message(FATAL_ERROR "screen lacks '${w}'\nscreen rows:\n${screen_rows}\n")
  endif()
endforeach()
if(NOT n_want GREATER 15)
  message(FATAL_ERROR "expected more than 15 rows from the seed, got ${n_want}")
endif()
message(STATUS "stats_screen ok rows=${n_want}")
