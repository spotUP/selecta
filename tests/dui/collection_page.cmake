# dui-collection-page (I678e): a playlist opens as a collection page (PG_COLLECTION, design doc 2026-10-29 section 7) through the real
# desktop UI entry. -DSELECTA -DSONGS -DWORK
# Fixture fixture_library.sql: albums 1 to 3 have 4x4 covers, album 4 none. Playlists: alpha = Main Theme (album 2), Intro (album 1),
# Bass Line (album 1); beta = Intro first (album 1); gamma = Cafe Del Mar first (album 4, no cover).
# The dump prints `collection kind= opens= dominants= cover= color= top= sort= q=[] shown= of= search= sortbtn=x,y,w,h find=x,y,w,h` and
# `plsong <i> ... title=` per drawn row.
# 1. A click on the alpha sidebar row sends COLLECTION_OPEN: opens=1 (the engine sentinel), page=collection, dominants=1 (computed once),
#    cover=1, 3 rows in playlist order.
# 2. The header colour comes from the cover: alpha (album 2 cover) and beta (album 1 cover) differ, gamma (no cover) has cover=0 and
#    the accent colour.
# 3. The sort button cycles Custom Order -> Title: Bass Line, Intro, Main Theme.
# 4. The search field filters: typing "intro" leaves the one row Intro (shown=1 of=3); Backspace x5 shows all three again.
get_filename_component(HERE "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
include("${HERE}/dui_run.cmake")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
find_program(SQLITE3 sqlite3)
if(NOT SQLITE3)
  message(FATAL_ERROR "dui-collection-page needs the sqlite3 command line tool to load the fixture library")
endif()
set(db "${WORK}/lib.db")
dui_run(init --ui desktop --backend mem --view home --canvas 1280x800 --clock fixed --stats --db "${db}")
execute_process(COMMAND "${SQLITE3}" -cmd "PRAGMA trusted_schema=ON" "${db}" INPUT_FILE "${HERE}/fixture_library.sql"
  RESULT_VARIABLE rc ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "loading the fixture failed: ${err}")
endif()

# The fixture keys are fixture:N; the title and cover lookups go by the local:<path> key a played file has.
execute_process(COMMAND "${SQLITE3}" -cmd "PRAGMA trusted_schema=ON" "${db}" "UPDATE songs SET key = 'local:' || path WHERE key LIKE 'fixture:%'"
  RESULT_VARIABLE rc ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "rekeying the fixture failed: ${err}")
endif()

function(expect_eq what got want)
  if(NOT "${got}" STREQUAL "${want}")
    message(FATAL_ERROR "${what}: got '${got}', expected '${want}'")
  endif()
endfunction()
# The centre of the sidebar row called `name` into ${out}.
function(side_row out dump name)
  if(NOT "${dump}" MATCHES "sidelib_row [0-9]+ ([0-9]+) ([0-9]+) ([0-9]+) ([0-9]+) pinned=[01] open=[01] kind=[0-9]+ ${name}\n")
    message(FATAL_ERROR "no sidebar row '${name}' in:\n${dump}")
  endif()
  math(EXPR cx "${CMAKE_MATCH_1} + ${CMAKE_MATCH_3} / 2")
  math(EXPR cy "${CMAKE_MATCH_2} + ${CMAKE_MATCH_4} / 2")
  set(${out} "${cx} ${cy}" PARENT_SCOPE)
endfunction()
# One field of the collection line.
function(col_field out dump field)
  if(NOT "${dump}" MATCHES "collection kind=playlist [^\n]*${field}=([^ \n]*)")
    message(FATAL_ERROR "no collection line with ${field} in:\n${dump}")
  endif()
  set(${out} "${CMAKE_MATCH_1}" PARENT_SCOPE)
endfunction()
function(titles_of out dump)
  string(REGEX MATCHALL "plsong [0-9]+ [0-9]+ [0-9]+ [0-9]+ [0-9]+ file=[^ \n]* title=[^\n]*" lines "${dump}")
  set(names "")
  foreach(l IN LISTS lines)
    string(REGEX MATCH "title=(.*)" m "${l}")
    list(APPEND names "${CMAKE_MATCH_1}")
  endforeach()
  set(${out} "${names}" PARENT_SCOPE)
endfunction()
# The centre of a `name=x,y,w,h` rectangle of the collection line.
function(col_rect out dump name)
  if(NOT "${dump}" MATCHES "collection kind=playlist [^\n]* ${name}=([0-9]+),([0-9]+),([0-9]+),([0-9]+)")
    message(FATAL_ERROR "no ${name} rectangle in:\n${dump}")
  endif()
  math(EXPR cx "${CMAKE_MATCH_1} + ${CMAKE_MATCH_3} / 2")
  math(EXPR cy "${CMAKE_MATCH_2} + ${CMAKE_MATCH_4} / 2")
  set(${out} "${cx} ${cy}" PARENT_SCOPE)
endfunction()

# Each open touches Recents and reorders the sidebar, so the row of `name` is read from an idle run right before the run that clicks it.
# `script` is the input after the click, `out` the dump.
function(open_run out name script)
  dui_run(idle_${name} ${common} --input "${WORK}/idle.txt")
  side_row(xy "${idle_${name}}" "${name}")
  file(WRITE "${WORK}/${name}.txt" "click ${xy}\nwait 100\n${script}")
  dui_run(run_${name} ${common} --input "${WORK}/${name}.txt")
  set(${out} "${run_${name}}" PARENT_SCOPE)
endfunction()

set(common --ui desktop --backend mem --view home --clock fixed --stats --db "${db}" --canvas 1280x800)
dui_run(seed ${common}
  --exec "PLAYLIST_CREATE alpha" --exec "PLAYLIST_ADD_SONG 1 fixture/2.mod" --exec "PLAYLIST_ADD_SONG 1 fixture/1.mod" --exec "PLAYLIST_ADD_SONG 1 fixture/5.sid"
  --exec "PLAYLIST_CREATE beta" --exec "PLAYLIST_ADD_SONG 2 fixture/1.mod"
  --exec "PLAYLIST_CREATE gamma" --exec "PLAYLIST_ADD_SONG 3 fixture/4.it")
file(WRITE "${WORK}/idle.txt" "wait 100\n")

# 1. Open alpha through the sidebar.
open_run(a alpha "")
expect_eq("page" "${a}" "${a}")
if(NOT "${a}" MATCHES "page=collection")
  message(FATAL_ERROR "the page is not the collection page:\n${a}")
endif()
col_field(opens "${a}" opens)
expect_eq("COLLECTION_OPEN sentinel" "${opens}" "1")
col_field(doms "${a}" dominants)
expect_eq("dominant colours computed once" "${doms}" "1")
col_field(cover "${a}" cover)
expect_eq("alpha has a cover colour" "${cover}" "1")
col_field(col_alpha "${a}" color)
titles_of(t_custom "${a}")
expect_eq("custom order" "${t_custom}" "Main Theme;Intro;Bass Line")

# 2. The colour follows the cover.
open_run(b beta "")
col_field(col_beta "${b}" color)
col_field(cover_b "${b}" cover)
expect_eq("beta has a cover colour" "${cover_b}" "1")
if("${col_alpha}" STREQUAL "${col_beta}")
  message(FATAL_ERROR "alpha and beta show the same header colour ${col_alpha}; it does not come from the covers")
endif()
open_run(g gamma "")
col_field(cover_g "${g}" cover)
expect_eq("gamma has no cover" "${cover_g}" "0")
col_field(doms_g "${g}" dominants)
expect_eq("no dominant colour computed without a cover" "${doms_g}" "0")
message(STATUS "header colours: alpha ${col_alpha} beta ${col_beta}")

# 3. Sort: the button cycles Custom Order to Title.
col_rect(sort_xy "${a}" sortbtn)
open_run(s alpha "click ${sort_xy}\nwait 100\n")
col_field(sort "${s}" sort)
expect_eq("sort mode" "${sort}" "1")
titles_of(t_sorted "${s}")
expect_eq("title order" "${t_sorted}" "Bass Line;Intro;Main Theme")

# 4. Search: type into the field.
col_rect(find_xy "${a}" find)
open_run(q alpha "click ${find_xy}\nwait 100\nkey i\nkey n\nkey t\nkey r\nkey o\nwait 100\n")
col_field(shown "${q}" shown)
expect_eq("rows shown for intro" "${shown}" "1")
col_field(of "${q}" of)
expect_eq("rows in the collection" "${of}" "3")
titles_of(t_found "${q}")
expect_eq("filtered row" "${t_found}" "Intro")
open_run(q2 alpha "click ${find_xy}\nkey i\nkey n\nkey t\nkey r\nkey o\nkey backspace\nkey backspace\nkey backspace\nkey backspace\nkey backspace\nwait 100\n")
col_field(shown2 "${q2}" shown)
expect_eq("rows shown after clearing the search" "${shown2}" "3")
