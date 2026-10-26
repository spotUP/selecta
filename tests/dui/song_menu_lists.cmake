# dui-song-menu-lists (DUI-P3-16 song menu, I371; label song-menu, not a plan id): every list whose rows are songs registers them with the shared song menu.
#   -DSELECTA -DSONGS -DWORK
#   Violated invariant: a song row anywhere offers the song menu. The Library Tree leaves, the Files rows and the Songs page
#   rows must be registered while they draw (song_row_reg), so a right-click on one opens the menu with its 4 items. Three
#   songs are scanned; each list is opened by its tab (or the Songs view), then a script right-click hits the first song row.
get_filename_component(HERE "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
include("${HERE}/dui_run.cmake")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}/songs")
foreach(n a b c)
  file(COPY_FILE "${SONGS}/selecta-test.mod" "${WORK}/songs/${n}.mod")
endforeach()
set(db "${WORK}/library.db")

set(SL_CANVAS 900x600)
function(sl_run outvar view input)
  file(REMOVE_RECURSE "${WORK}/cfg")
  file(WRITE "${WORK}/in.txt" "${input}")
  dui_run(o --start-dir "${WORK}/songs" --db "${db}" --ui desktop --backend mem --view ${view} --canvas ${SL_CANVAS} --clock fixed --stats
          --exec "QUEUE_ADD ${WORK}/songs/a.mod" --exec "QUEUE_ADD ${WORK}/songs/b.mod" --input "${WORK}/in.txt")
  set(${outvar} "${o}" PARENT_SCOPE)
endfunction()

function(sl_opens outvar out)
  if(NOT "${out}" MATCHES "song_menu open=([0-9]+) items=([0-9]+) opens=([0-9]+)")
    message(FATAL_ERROR "no song_menu line in:\n${out}")
  endif()
  set(${outvar} "${CMAKE_MATCH_3}" PARENT_SCOPE)
endfunction()

# expect_menu(<what> <view> <prefix input> <rect regex over the dump> <extra y rows>): right-click the rect found by the regex.
function(expect_menu what view pre rx)
  sl_run(dump ${view} "${pre}")
  if(NOT "${dump}" MATCHES "${rx}")
    message(FATAL_ERROR "${what}: no row rectangle matching ${rx} in:\n${dump}")
  endif()
  math(EXPR x "${CMAKE_MATCH_1} + 20")
  math(EXPR y "${CMAKE_MATCH_2} + ${CMAKE_MATCH_4} / 2")
  sl_run(res ${view} "${pre}move ${x} ${y}\nrclick ${x} ${y}\nwait 1\n")
  sl_opens(opens "${res}")
  if(NOT opens EQUAL 1)
    message(FATAL_ERROR "${what}: right-click on the song row opened the menu ${opens} times, expected 1:\n${res}")
  endif()
endfunction()

set(scan "wait 1\nexec LIBRARY_SCAN ${WORK}/songs\nwait 4\n")
# Library Tree: expand the first group, then the first leaf (list row 1).
sl_run(t0 library "${scan}")
if(NOT "${t0}" MATCHES "tab 1 ([0-9]+) ([0-9]+) ([0-9]+) ([0-9]+)\n")
  message(FATAL_ERROR "no tab 1 in:\n${t0}")
endif()
math(EXPR t1x "${CMAKE_MATCH_1} + 10")
math(EXPR t1y "${CMAKE_MATCH_2} + 8")
if(NOT "${t0}" MATCHES "tab 3 ([0-9]+) ([0-9]+) ([0-9]+) ([0-9]+)\n")
  message(FATAL_ERROR "no tab 3 in:\n${t0}")
endif()
math(EXPR t3x "${CMAKE_MATCH_1} + 10")
math(EXPR t3y "${CMAKE_MATCH_2} + 8")
set(tree "${scan}move ${t1x} ${t1y}\nclick ${t1x} ${t1y}\nwait 2\n")
sl_run(t1 library "${tree}")
if(NOT "${t1}" MATCHES "vrect 0 ([0-9]+) ([0-9]+) ([0-9]+) ([0-9]+)\n")
  message(FATAL_ERROR "no tree row 0 in:\n${t1}")
endif()
math(EXPR nx "${CMAKE_MATCH_1} + 10")
math(EXPR ny "${CMAKE_MATCH_2} + 8")
set(tree "${tree}move ${nx} ${ny}\nclick ${nx} ${ny}\nwait 2\n")
expect_menu("Library Tree leaf" library "${tree}" "vrect 1 ([0-9]+) ([0-9]+) ([0-9]+) ([0-9]+)\n")
# Library Files: the browse listing of the start directory: row 0 is the parent directory, row 1 the first file.
set(files "${scan}move ${t3x} ${t3y}\nclick ${t3x} ${t3y}\nwait 2\n")
expect_menu("Library Files row" library "${files}" "vrect 1 ([0-9]+) ([0-9]+) ([0-9]+) ([0-9]+)\n")
# Songs page: the two queued songs are rows of the page; the first row is at the top of the list under the header.
sl_run(s0 songs "wait 1\n")
if(NOT "${s0}" MATCHES "song_menu open=0 items=0 opens=0 picks=0 playlist_adds=0 rows=([0-9]+)" OR CMAKE_MATCH_1 LESS 2)
  message(FATAL_ERROR "the Songs page registered fewer than 2 song rows:\n${s0}")
endif()

# I372 (DUI-P4-16): the remaining song places. Each is a right-click at a point read from the dump (hover_regions for the
# main and detail rectangles, hover_ctl for Home tile 0 and the Now Playing like button) and must open the menu once with
# the 4 items of the table (g_menu_items: Play, Play Next, Add to Queue, Remove).
function(expect_menu_at what view pre x y)
  sl_run(res ${view} "${pre}move ${x} ${y}\nrclick ${x} ${y}\nwait 1\n")
  sl_opens(opens "${res}")
  if(NOT opens EQUAL 1 OR NOT "${res}" MATCHES "song_menu open=1 items=5 ")
    message(FATAL_ERROR "${what}: right-click at ${x},${y} opened the menu ${opens} times, expected 1 with 5 items:\n${res}")
  endif()
endfunction()

sl_run(g0 library "wait 1\n")
if(NOT "${g0}" MATCHES "hover_regions sidebar [-0-9]+ [-0-9]+ [0-9]+ [0-9]+ main ([0-9]+) ([0-9]+) ([0-9]+) ([0-9]+) detail ([0-9]+) ([0-9]+) ([0-9]+) ([0-9]+)")
  message(FATAL_ERROR "no hover_regions line in:\n${g0}")
endif()
set(mnx ${CMAKE_MATCH_1}) 
set(mny ${CMAKE_MATCH_2})
set(mnw ${CMAKE_MATCH_3})
# Library grid tile 0 (margin 24, list top 88, art 160) and the cover flow centre (240 wide, list top 88), queue source.
set(libstyle "exec VIEW_SOURCE queue\nexec VIEW_WINDOW 8\nexec COMPOSERS_SHOW\n")
math(EXPR gx "${mnx} + 24 + 40")
math(EXPR gy "${mny} + 88 + 60")
expect_menu_at("Library grid tile" library "${libstyle}exec VIEW_STYLE grid\nwait 100\n" ${gx} ${gy})
math(EXPR fx "${mnx} + ${mnw} / 2")
math(EXPR fy "${mny} + 88 + 120")
expect_menu_at("Library cover flow centre" library "${libstyle}exec VIEW_STYLE coverflow\nwait 100\n" ${fx} ${fy})
# Home tile grid: tile 0 of the queue tiles (hover_ctl main is tile 0).
set(playing "exec OPEN ${WORK}/songs/a.mod\nwait 100\n")
# The detail panel only exists on a wide canvas (900x600 gives it 0 0 0 0).
set(SL_CANVAS 1400x800)
sl_run(h0 home "${playing}")
if(NOT "${h0}" MATCHES "hover_ctl sidebar [-0-9]+ [-0-9]+ [0-9]+ [0-9]+ main ([0-9]+) ([0-9]+) ([0-9]+) ([0-9]+) player [-0-9]+ [-0-9]+ [0-9]+ [0-9]+[ \n]+detail ([0-9]+) ([0-9]+)")
  message(FATAL_ERROR "no hover_ctl line in:\n${h0}")
endif()
math(EXPR hx "${CMAKE_MATCH_1} + 30")
math(EXPR hy "${CMAKE_MATCH_2} + 30")
# The detail field pair is the Now Playing like button: the title and artist lines sit 44 to 4 px above it.
math(EXPR nx "${CMAKE_MATCH_5} + 20")
math(EXPR ny "${CMAKE_MATCH_6} - 20")
expect_menu_at("Home tile" home "wait 1\n" ${hx} ${hy})
expect_menu_at("Now Playing title" home "${playing}" ${nx} ${ny})
set(SL_CANVAS 900x600)
# Song info header: the "Song Information" title row (16 px under the main top, 40 px high, margin 24).
math(EXPR sx "${mnx} + 24 + 20")
math(EXPR sy "${mny} + 36")
expect_menu_at("Song info header" songinfo "${playing}" ${sx} ${sy})
