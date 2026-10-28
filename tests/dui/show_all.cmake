# dui-show-all (I617): the grey "Show all" link at the right end of every Home shelf header opens that shelf as a grid page. -DSELECTA -DSONGS -DWORK
# 1. Home draws one link per shelf (showall line: draws > 0, a rect with width).
# 2. A pointer click on shelf 1's link (the `showall N` input command, through the pointer path) opens the Shelf page:
#    nav page=shelf, the page laid out exactly shelf 1's tiles (tiles= equals the shelf lines of the dump), the click counted.
# 3. Back (Alt+Left) returns to Home, and the page history holds Home then Shelf.
# 4. Keyboard: Down walks the engine cursor over the Home rows, Enter on a shelf heading opens that shelf (the bitmap tier's way in).
get_filename_component(HERE "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
include("${HERE}/dui_run.cmake")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
find_program(SQLITE3 sqlite3)
if(NOT SQLITE3)
  message(FATAL_ERROR "dui-show-all needs the sqlite3 command line tool to load the fixture library")
endif()
configure_file("${SONGS}/selecta-test.mod" "${WORK}/a.mod" COPYONLY)
set(db "${WORK}/library.db")
dui_run(init --db "${db}" --ui desktop --backend mem --view home --canvas 900x600 --clock fixed --stats)
execute_process(COMMAND "${SQLITE3}" -cmd "PRAGMA trusted_schema=ON" "${db}" INPUT_FILE "${HERE}/fixture_library.sql"
  RESULT_VARIABLE rc ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "loading the fixture failed: ${err}")
endif()
string(CONCAT seed
  "UPDATE songs SET key = '${WORK}/a.mod', path = '${WORK}/a.mod' WHERE id = 4; "
  "INSERT INTO history(key, played_at, skipped) VALUES('fixture:3', 1000, 1); "
  "INSERT INTO history(key, played_at, skipped) VALUES('fixture:1', 2000, 0); "
  "INSERT INTO history(key, played_at, skipped) VALUES('fixture:5', 3000, 1); "
  "INSERT INTO history(key, played_at, skipped) VALUES('fixture:2', 4000, 0); "
  "INSERT INTO history(key, played_at, skipped) VALUES('fixture:6', 5000, 1); "
  "INSERT INTO history(key, played_at, skipped) VALUES('${WORK}/a.mod', 6000, 1)")
execute_process(COMMAND "${SQLITE3}" "${db}" "PRAGMA trusted_schema = 1; ${seed}" RESULT_VARIABLE rc ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "seeding the history failed: ${err}")
endif()

function(sa_run tag script)
  file(REMOVE_RECURSE "${WORK}/cfg")
  file(WRITE "${WORK}/${tag}.txt" "${script}")
  dui_run(o --db "${db}" --ui desktop --backend mem --view home --canvas 1280x800 --clock fixed --stats --input "${WORK}/${tag}.txt")
  if(NOT "${o}" MATCHES "showall draws=([0-9]+) clicks=([0-9]+) page_draws=([0-9]+) opens=([0-9]+) open=([0-9]+) tiles=([0-9]+) at (-?[0-9]+) (-?[0-9]+) ([0-9]+) ")
    message(FATAL_ERROR "no showall line in:\n${o}")
  endif()
  set(${tag}_draws ${CMAKE_MATCH_1} PARENT_SCOPE)
  set(${tag}_clicks ${CMAKE_MATCH_2} PARENT_SCOPE)
  set(${tag}_pdraws ${CMAKE_MATCH_3} PARENT_SCOPE)
  set(${tag}_opens ${CMAKE_MATCH_4} PARENT_SCOPE)
  set(${tag}_open ${CMAKE_MATCH_5} PARENT_SCOPE)
  set(${tag}_tiles ${CMAKE_MATCH_6} PARENT_SCOPE)
  set(${tag}_linkw ${CMAKE_MATCH_9} PARENT_SCOPE)
  string(REGEX MATCH "nav page=([a-z]+)" _m "${o}")
  set(${tag}_page "${CMAKE_MATCH_1}" PARENT_SCOPE)
  foreach(s 0 1 2 3)
    string(REGEX MATCHALL "shelf ${s} tile [0-9]+ " lines "${o}")
    list(LENGTH lines n)
    set(${tag}_shelf${s} ${n} PARENT_SCOPE)
  endforeach()
endfunction()

function(expect_eq what got want)
  if(NOT "${got}" STREQUAL "${want}")
    message(FATAL_ERROR "${what}: got '${got}', want '${want}'")
  endif()
endfunction()

# 1. the links on Home
sa_run(idle "wait 100\n")
if(idle_draws LESS 2 OR idle_linkw LESS 1)
  message(FATAL_ERROR "Home drew no Show all links (draws=${idle_draws}, width of shelf 0's link ${idle_linkw})")
endif()
expect_eq("Home page" "${idle_page}" "home")
expect_eq("page draws before any click" "${idle_pdraws}" 0)
if(idle_shelf1 LESS 1)
  message(FATAL_ERROR "the fixture gives shelf 1 no tiles")
endif()

# 2. click shelf 1's link
sa_run(open "wait 100\nshowall 1\nwait 100\n")
expect_eq("page after Show all" "${open_page}" "shelf")
expect_eq("Show all clicks" "${open_clicks}" 1)
expect_eq("Shelf page opens" "${open_opens}" 1)
expect_eq("the open shelf" "${open_open}" 2)
expect_eq("the grid holds the shelf's items" "${open_tiles}" "${open_shelf1}")
if(open_pdraws LESS 1)
  message(FATAL_ERROR "the Shelf page was never drawn")
endif()

# 3. Back
sa_run(back "wait 100\nshowall 1\nwait 100\nkey alt+left\nwait 100\n")
expect_eq("page after Back" "${back_page}" "home")
expect_eq("Shelf page opens before Back" "${back_opens}" 1)
sa_run(fwd "wait 100\nshowall 0\nwait 100\nkey alt+left\nwait 100\nkey alt+right\nwait 100\n")
expect_eq("page after Forward" "${fwd_page}" "shelf")
expect_eq("Forward shows shelf 0" "${fwd_tiles}" "${fwd_shelf0}")

# 4. Some counts of Down land on a heading: those runs end on the Shelf page with the click counted, the others stay on Home.
set(opened 0)
foreach(downs RANGE 1 14)
  string(REPEAT "key down\nwait 50\n" ${downs} walk)
  sa_run(kb "wait 100\n${walk}key enter\nwait 100\n")
  if("${kb_page}" STREQUAL "shelf")
    math(EXPR opened "${opened} + 1")
    math(EXPR which "${kb_open} - 1")
    expect_eq("Enter on a header counts as a Show all click (downs=${downs})" "${kb_clicks}" 1)
    expect_eq("the grid holds the shelf's items (downs=${downs})" "${kb_tiles}" "${kb_shelf${which}}")
  endif()
endforeach()
if(opened LESS 2)
  message(FATAL_ERROR "Enter on a shelf heading opened ${opened} shelves, want one per shelf")
endif()
