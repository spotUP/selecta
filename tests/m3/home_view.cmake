# T91 home-view (plan 2026-10-25-m3-library.md, I191).
#   -DCLI=<selecta-cli> -DSELECTA=<exe> -DCAT=<catalogue.tsv> -DSEL=<history.sel> -DSQLITE3=<sqlite3 shell>
#   -DKEYS=<keys> -DGOLDEN=<screen golden> -DWORK=<dir> -DCWD=<dir holding selecta-test.mod>
# history.sel writes a finished play and a skip through the engine. sqlite3 then seeds an album on the skipped song, a
# newer play and the modland "new this week" count a changelog sync stores. The real selecta binary opens that file and
# key H shows the home view. home_rows= must equal the sum counted from the database here with separate queries:
# distinct songs played + albums with an unfinished play + the new-this-week count.
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
set(db "${WORK}/h.db")
execute_process(COMMAND "${CLI}" script "${SEL}" --catalogue "${CAT}" --db "${db}"
  WORKING_DIRECTORY "${CWD}" OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "selecta-cli exited ${rc}\nstdout:\n${out}\nstderr:\n${err}")
endif()
function(sql text result)
  execute_process(COMMAND "${SQLITE3}" "${db}" "PRAGMA trusted_schema = 1; ${text}" OUTPUT_VARIABLE o ERROR_VARIABLE e
    RESULT_VARIABLE rc)
  if(NOT rc EQUAL 0)
    message(FATAL_ERROR "sqlite3 failed: ${e}")
  endif()
  string(STRIP "${o}" o)
  set(${result} "${o}" PARENT_SCOPE)
endfunction()
string(CONCAT seed
  "UPDATE songs SET album = 'Hardcore Heroes' WHERE key = 'selecta-test-mk.mod'; "
  "INSERT INTO history(key, played_at, skipped) VALUES('selecta-test.mod', 4102444800, 0); "
  "INSERT INTO meta(key, value) VALUES('modland.new_this_week', '7') ON CONFLICT(key) DO UPDATE SET value = '7'")
sql("${seed}" ignore)
sql("SELECT COUNT(DISTINCT key) FROM history;" recent)
string(CONCAT jumpq
  "SELECT COUNT(DISTINCT s.album) FROM history h JOIN songs s ON s.key = h.key WHERE h.skipped = 1 AND s.album IS NOT NULL "
  "AND NOT EXISTS (SELECT 1 FROM history f WHERE f.key = h.key AND f.skipped = 0);")
sql("${jumpq}" jump)
sql("SELECT value FROM meta WHERE key = 'modland.new_this_week';" fresh)
math(EXPR want "${recent} + ${jump} + ${fresh}")
if(NOT recent EQUAL 2 OR NOT jump EQUAL 1 OR NOT fresh EQUAL 7)
  message(FATAL_ERROR "seed counts recent=${recent} jump=${jump} new=${fresh}, want 2 1 7")
endif()
execute_process(
  COMMAND "${SELECTA}" --output null --size 80x25 --start-dir "${CWD}" --db "${db}"
          --keys "${KEYS}" --screen-dump "${WORK}/home.txt"
  WORKING_DIRECTORY "${CWD}" OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "selecta exited ${rc}\nstdout:\n${out}\nstderr:\n${err}")
endif()
if(NOT out MATCHES "home_rows=([0-9]+)")
  message(FATAL_ERROR "no home_rows= sentinel in:\n${out}")
endif()
if(NOT CMAKE_MATCH_1 EQUAL want)
  message(FATAL_ERROR "home_rows=${CMAKE_MATCH_1}, want ${want} counted from the database")
endif()
file(READ "${WORK}/home.txt" screen)
if(NOT EXISTS "${GOLDEN}")
  file(WRITE "${GOLDEN}" "${screen}")
  message(FATAL_ERROR "golden written; review it")
endif()
file(READ "${GOLDEN}" golden)
if(NOT screen STREQUAL golden)
  message(FATAL_ERROR "screen differs from ${GOLDEN}\nexpected:\n${golden}\ngot:\n${screen}")
endif()
# I191a: Enter on a row opens it. Each run starts from the same database: the dump of the first run is not
# touched; Enter on a row only reads the keys the rows carry.
function(run_keys keys dump result)
  execute_process(
    COMMAND "${SELECTA}" --output null --size 80x25 --start-dir "${CWD}" --db "${db}"
            --keys "${keys}" --screen-dump "${dump}"
    WORKING_DIRECTORY "${CWD}" OUTPUT_VARIABLE o ERROR_VARIABLE e RESULT_VARIABLE rc)
  if(NOT rc EQUAL 0)
    message(FATAL_ERROR "selecta exited ${rc}\nstdout:\n${o}\nstderr:\n${e}")
  endif()
  set(${result} "${o}" PARENT_SCOPE)
endfunction()
# Song row: the song starts. songs_opened counts it, the queue holds it and the status line shows Playing with the module
# title of selecta-test.mod, the newest play in the database.
run_keys("${KEYS_SONG}" "${WORK}/song.txt" song_out)
file(READ "${WORK}/song.txt" song_screen)
sql("SELECT key FROM history GROUP BY key ORDER BY MAX(played_at) DESC, key LIMIT 1;" newest)
if(NOT song_out MATCHES "songs_opened=1 ")
  message(FATAL_ERROR "Enter on a Recently played row did not open the song:\n${song_out}")
endif()
if(NOT song_out MATCHES "playlist_entries=1 ")
  message(FATAL_ERROR "Enter on a Recently played row did not queue the song:\n${song_out}")
endif()
if(NOT newest STREQUAL "selecta-test.mod" OR NOT song_screen MATCHES "Playing  selecta test")
  message(FATAL_ERROR "screen does not show ${newest} (module title selecta test) playing:\n${song_screen}")
endif()
# Album row: the album's songs are listed in the library view (query source) and none starts.
run_keys("${KEYS_ALBUM}" "${WORK}/album.txt" album_out)
file(READ "${WORK}/album.txt" album_screen)
sql("SELECT COUNT(*) FROM songs WHERE album = 'Hardcore Heroes';" album_songs)
if(NOT album_out MATCHES "view_source=query ")
  message(FATAL_ERROR "Enter on a Jump back in row did not open the library view:\n${album_out}")
endif()
if(NOT album_out MATCHES "songs_opened=0 " OR NOT album_songs EQUAL 1 OR NOT album_screen MATCHES "mk tune")
  message(FATAL_ERROR "album view wrong (songs in album ${album_songs}):\n${album_out}\n${album_screen}")
endif()
if(album_screen MATCHES "test tune")
  message(FATAL_ERROR "album view lists a song of another album:\n${album_screen}")
endif()
# I191b: an album name with a quote and a backslash opens too. The query grammar escapes both inside quotes; the engine
# escapes the name it takes from the row. The only Jump back in album is renamed in the database.
sql("UPDATE songs SET album = 'The \"Quoted\" Album \\ Two' WHERE key = 'selecta-test-mk.mod';" ignore)
run_keys("${KEYS_ALBUM}" "${WORK}/quoted.txt" quoted_out)
file(READ "${WORK}/quoted.txt" quoted_screen)
if(NOT quoted_out MATCHES "view_source=query " OR NOT quoted_out MATCHES "songs_opened=0 " OR NOT quoted_screen MATCHES "mk tune")
  message(FATAL_ERROR "album name with quotes did not list its songs:\n${quoted_out}\n${quoted_screen}")
endif()
if(quoted_screen MATCHES "test tune")
  message(FATAL_ERROR "quoted album view lists a song of another album:\n${quoted_screen}")
endif()
message(STATUS "home_rows=${want} song_row=${newest} album_row=Hardcore Heroes")
