# dui-home-item-art (I997): one album, one cover on every surface. -DSELECTA -DWORK
# Violated invariant: an album library-item card (I678j: quick tile, Recently Played, Your Albums) got no art_ref, so it drew the
# placeholder while the sidebar and Jump Back In showed the real cover of the same album. The engine now gives an album item row the
# art_ref "\x1e<albums.id>" and the db art provider resolves it by id.
# Fixture fixture_home_shelves.sql (five albums, all played) with a flat cover on every albums row. With the fix the album item cards
# of the quick grid and the shelves draw the library cover: shelf_album_art_draws counts them (quick tiles + shelf cards).
# Without the fix it stays 0 (nothing on Home but item cards draws album art here).
get_filename_component(HERE "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
include("${HERE}/dui_run.cmake")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
find_program(SQLITE3 sqlite3)
if(NOT SQLITE3)
  message(FATAL_ERROR "dui-home-item-art needs the sqlite3 command line tool to load the fixture library")
endif()
set(db "${WORK}/lib.db")
dui_run(init --ui desktop --backend mem --view home --canvas 1280x1400 --clock fixed --stats --db "${db}")
execute_process(COMMAND "${SQLITE3}" -cmd "PRAGMA trusted_schema=ON" "${db}" INPUT_FILE "${HERE}/fixture_home_shelves.sql"
  RESULT_VARIABLE rc ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "loading the fixture failed: ${err}")
endif()
set(flat "89504E470D0A1A0A0000000D49484452000000080000000808020000004B6D29DC0000001149444154789C63F870C2062B62185A120004A77D01143E47B20000000049454E44AE426082")
execute_process(COMMAND "${SQLITE3}" "${db}" "PRAGMA trusted_schema = 1; INSERT INTO artwork(album_id, kind, data) SELECT id, 'cover', X'${flat}' FROM albums"
  RESULT_VARIABLE rc ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "flat covers failed: ${err}")
endif()
file(WRITE "${WORK}/idle.txt" "wait 100\n")
dui_run(m1 --ui desktop --backend mem --view home --clock fixed --stats --db "${db}" --canvas 1280x1400 --input "${WORK}/idle.txt")
if(NOT m1 MATCHES "shelf 0 tile 0 Recently Played\\|5\\|Alb5")
  message(FATAL_ERROR "the engine sent no item shelves (sentinel):\n${m1}")
endif()
dui_stat(draws "${m1}" desktop shelf_album_art_draws)
message(STATUS "shelf_album_art_draws=${draws}")
# 11 = quick grid album tiles Alb5 Alb4 Alb2 (3) + Recently Played album cards in view Alb5 Alb4 Alb2 (3) + Your Albums Alb1..Alb5 (5);
# the playlist, composer and Liked Songs cards draw generated art, and the Recently Played cards past the canvas edge are not painted.
set(EXPECTED_DRAWS 11)
if(NOT draws EQUAL EXPECTED_DRAWS)
  message(FATAL_ERROR "album item cards drew ${draws} library covers, expected ${EXPECTED_DRAWS} (placeholders instead of the cover):\n${m1}")
endif()
