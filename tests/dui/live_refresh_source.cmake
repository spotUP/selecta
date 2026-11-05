# dui-live-refresh-source (I1077a, plan 2026-11-05-I1077-live-view-refresh.md): an open source page follows a library write it did not
# start. -DSELECTA -DWORK
#   Bug (Johan, Mac): the Podcasts page stays empty after Sync until he switches source. A sync end moves library_serial, but the
#   desktop only re-reads folders on it; state.collection (the open source page) is never re-read, and library_reloads stays 0.
#   Scenario: the test source plugin testsrc (its sync writes 3 songs through the catalogue service, the same path a podcast sync
#   takes; no network) is seeded with one row. A click on its sidebar row opens its page; the script then sends `exec LIBRARY_SYNC testsrc`
#   (the line a CLI or ARexx client sends; no source switch, no page change). The dump must show the Formats level (drill rows=) grown by
#   the synced format and library_reloads>=1. RED on main: rows stay at the seed count.
get_filename_component(HERE "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
include("${HERE}/dui_run.cmake")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
find_program(SQLITE3 sqlite3)
if(NOT SQLITE3)
  message(FATAL_ERROR "dui-live-refresh-source needs the sqlite3 command line tool to seed the library")
endif()
set(db "${WORK}/lib.db")
set(common --ui desktop --backend mem --view home --clock fixed --stats --db "${db}" --canvas 1280x1600)
dui_run(init ${common})
execute_process(COMMAND "${SQLITE3}" -cmd "PRAGMA trusted_schema=ON" "${db}"
  "DROP TRIGGER IF EXISTS songs_ai; INSERT INTO songs(key, source, path, name, artist, album, format, added) VALUES ('testsrc:z', 'testsrc', 'z/x.mod', 'seed', 'Probe', '', 'ZSeedFormat', 1)"
  RESULT_VARIABLE rc ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "seeding the source failed (rc=${rc}, sqlite3=${SQLITE3}, db=${db}): ${err}")
endif()
file(COPY_FILE "${db}" "${WORK}/base.db")

function(side_row out dump name)
  if(NOT "${dump}" MATCHES "sidelib_row [0-9]+ ([0-9]+) ([0-9]+) ([0-9]+) ([0-9]+) pinned=[01] open=[01] kind=7 ${name}\n")
    message(FATAL_ERROR "no sidebar row of kind 7 '${name}' in:\n${dump}")
  endif()
  math(EXPR cx "${CMAKE_MATCH_1} + ${CMAKE_MATCH_3} / 2")
  math(EXPR cy "${CMAKE_MATCH_2} + ${CMAKE_MATCH_4} / 2")
  set(${out} "${cx} ${cy}" PARENT_SCOPE)
endfunction()

file(WRITE "${WORK}/idle.txt" "wait 100\n")
dui_run(idle ${common} --input "${WORK}/idle.txt")
side_row(src_xy "${idle}" "Test Source")

# The source page is a drill (source_drill.cmake): the root level lists Formats, Platforms, Artists, New; the Formats level lists one row
# per format in the library. The seed has a format of its own, the sync adds ProTracker, so the Formats row count must grow by one.
file(WRITE "${WORK}/root.txt" "click ${src_xy}\nwait 100\n")
file(COPY_FILE "${WORK}/base.db" "${db}")
dui_run(root ${common} --input "${WORK}/root.txt")
if(NOT "${root}" MATCHES "\ndrill [^\n]* row0=(-?[0-9]+),(-?[0-9]+),([0-9]+),([0-9]+)")
  message(FATAL_ERROR "no drill row0 in:\n${root}")
endif()
math(EXPR r0x "${CMAKE_MATCH_1} + ${CMAKE_MATCH_3} / 2")
math(EXPR r0y "${CMAKE_MATCH_2} + ${CMAKE_MATCH_4} / 2")
function(drill_rows out dump)
  if(NOT "${dump}" MATCHES "\ndrill ([^\n]* )?rows=([0-9]+)")
    message(FATAL_ERROR "no drill rows in:\n${dump}")
  endif()
  set(${out} "${CMAKE_MATCH_2}" PARENT_SCOPE)
endfunction()

# Control: with no sync, the Formats level lists the seeded format.
file(WRITE "${WORK}/ctl.txt" "click ${src_xy}\nwait 100\nclick ${r0x} ${r0y}\nwait 600\n")
file(COPY_FILE "${WORK}/base.db" "${db}")
dui_run(ctl ${common} --input "${WORK}/ctl.txt")
drill_rows(before "${ctl}")
if(NOT "${ctl}" MATCHES "\ndrill ([^\n]* )?page=2 ")
  message(FATAL_ERROR "control: the Formats level did not open:\n${ctl}")
endif()

file(WRITE "${WORK}/live.txt" "click ${src_xy}\nwait 100\nclick ${r0x} ${r0y}\nwait 100\nexec LIBRARY_SYNC testsrc ${WORK}\nwait 600\n")
file(COPY_FILE "${WORK}/base.db" "${db}")
dui_run(o ${common} --input "${WORK}/live.txt")
drill_rows(after "${o}")
if(NOT "${o}" MATCHES "library_reloads=([0-9]+) ")
  message(FATAL_ERROR "no library_reloads in:\n${o}")
endif()
set(reloads "${CMAKE_MATCH_1}")
math(EXPR want "${before} + 1")
if(NOT after EQUAL want OR reloads LESS 1)
  message(FATAL_ERROR "I1077a RED: the open source page did not follow the sync: Formats rows ${after} (want ${want}: ${before} seeded + ProTracker), library_reloads=${reloads} (want >=1)")
endif()
