# dui-nav (I262): the sidebar doors and the page keys through the real shell and the scripted --input feeder.
#   -DSELECTA -DSONGS -DWORK
# The desktop dumps a `nav page=<name> nav_clicks=N nav_keys=N doors=N` line in its stats. Each door is clicked from the
# Home frame (the script's `door N` command clicks the centre of door N through the pointer path, the same one a mouse
# takes), and the page name after the click is checked against the door. A door that is not wired to the click leaves
# the page at home and nav_clicks at zero. The page keys (the text UI's default table: H, l, f, i) go through the key
# path and are counted in nav_keys. --view is checked to land on the same page names the doors reach.
get_filename_component(HERE "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
include("${HERE}/dui_run.cmake")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
foreach(n banana apple cherry)
  configure_file("${SONGS}/selecta-test.mod" "${WORK}/${n}.mod" COPYONLY)
endforeach()

function(expect_eq what got want)
  if(NOT "${got}" STREQUAL "${want}")
    message(FATAL_ERROR "${what}: got '${got}', expected '${want}'")
  endif()
endfunction()

# nav_run(<tag> <view> <script>) sets <tag>_page, <tag>_clicks, <tag>_keys and <tag>_doors from the nav line.
function(nav_run tag view input)
  file(REMOVE_RECURSE "${WORK}/cfg")
  file(WRITE "${WORK}/${tag}.txt" "${input}")
  dui_run(o --ui desktop --backend mem --view ${view} --canvas 1280x800 --clock fixed --stats
    --exec "QUEUE_ADD ${WORK}/banana.mod" --exec "QUEUE_ADD ${WORK}/apple.mod" --exec "QUEUE_ADD ${WORK}/cherry.mod"
    --input "${WORK}/${tag}.txt")
  string(REGEX MATCH "nav page=([a-z]+) nav_clicks=([0-9]+) nav_keys=([0-9]+) doors=([0-9]+)" _m "${o}")
  if(_m STREQUAL "")
    message(FATAL_ERROR "no nav line in:\n${o}")
  endif()
  set(${tag}_page "${CMAKE_MATCH_1}" PARENT_SCOPE)
  set(${tag}_clicks "${CMAKE_MATCH_2}" PARENT_SCOPE)
  set(${tag}_keys "${CMAKE_MATCH_3}" PARENT_SCOPE)
  set(${tag}_doors "${CMAKE_MATCH_4}" PARENT_SCOPE)
  string(REGEX MATCH "hist=([0-9]+) pos=([0-9]+) np_page_draws=([0-9]+) arrow_clicks=([0-9]+) lit=([0-9]+)" _m "${o}")
  set(${tag}_hist "${CMAKE_MATCH_1}" PARENT_SCOPE)
  set(${tag}_pos "${CMAKE_MATCH_2}" PARENT_SCOPE)
  set(${tag}_npdraws "${CMAKE_MATCH_3}" PARENT_SCOPE)
  set(${tag}_arrows "${CMAKE_MATCH_4}" PARENT_SCOPE)
  set(${tag}_lit "${CMAKE_MATCH_5}" PARENT_SCOPE)
  string(REGEX MATCH "search draws [0-9]+ rows [0-9]+ query ([^\n]*)" _m "${o}")
  set(${tag}_query "${CMAKE_MATCH_1}" PARENT_SCOPE)
  # the rects of the stats lines: <tag>_ring, <tag>_field, <tag>_back, <tag>_fwd as "x y w h"
  string(REGEX MATCH "focus_ring (-?[0-9]+ -?[0-9]+ -?[0-9]+ -?[0-9]+)" _m "${o}")
  set(${tag}_ring "${CMAKE_MATCH_1}" PARENT_SCOPE)
  string(REGEX MATCH "search_field (-?[0-9]+ -?[0-9]+ -?[0-9]+ -?[0-9]+) caret_draws=([0-9]+) caret_rect (-?[0-9]+ -?[0-9]+ -?[0-9]+ -?[0-9]+)" _m "${o}")
  set(${tag}_field "${CMAKE_MATCH_1}" PARENT_SCOPE)
  set(${tag}_caret "${CMAKE_MATCH_2}" PARENT_SCOPE)
  set(${tag}_caret_rect "${CMAKE_MATCH_3}" PARENT_SCOPE)
  string(REGEX MATCH "nav_arrows (-?[0-9]+ -?[0-9]+ -?[0-9]+ -?[0-9]+) (-?[0-9]+ -?[0-9]+ -?[0-9]+ -?[0-9]+)" _m "${o}")
  set(${tag}_back "${CMAKE_MATCH_1}" PARENT_SCOPE)
  set(${tag}_fwd "${CMAKE_MATCH_2}" PARENT_SCOPE)
endfunction()

# centre_of(<out_x> <out_y> "x y w h") sets the centre point of a stats rect.
function(centre_of outx outy rect)
  string(REPLACE " " ";" r "${rect}")
  list(GET r 0 x)
  list(GET r 1 y)
  list(GET r 2 w)
  list(GET r 3 h)
  math(EXPR cx "${x} + ${w} / 2")
  math(EXPR cy "${y} + ${h} / 2")
  set(${outx} ${cx} PARENT_SCOPE)
  set(${outy} ${cy} PARENT_SCOPE)
endfunction()

# 1. every door, clicked from the Home frame
set(door_pages home search library playlists queue nowplaying songinfo)
nav_run(h0 home "wait 100\n")
expect_eq("Home frame page before any click" "${h0_page}" "home")
expect_eq("no click yet" "${h0_clicks}" 0)
expect_eq("seven doors" "${h0_doors}" 7)
set(i 0)
foreach(pg ${door_pages})
  nav_run(d${i} home "wait 100\ndoor ${i}\nwait 100\n")
  expect_eq("door ${i} opens ${pg}" "${d${i}_page}" "${pg}")
  expect_eq("door ${i} click counted" "${d${i}_clicks}" 1)
  math(EXPR i "${i} + 1")
endforeach()

# 2. a walk through several doors ends on the last one, counting each click
nav_run(w home "wait 100\ndoor 3\nwait 100\ndoor 4\nwait 100\ndoor 2\nwait 100\ndoor 0\nwait 100\n")
expect_eq("walk ends at Home" "${w_page}" "home")
expect_eq("walk counted four clicks" "${w_clicks}" 4)

# 3. the page keys of the text UI's table
foreach(spec "H:home" "l:library" "f:playlists" "i:songinfo")
  string(REPLACE ":" ";" pair "${spec}")
  list(GET pair 0 k)
  list(GET pair 1 pg)
  nav_run(k${pg} queue "wait 100\nkey ${k}\nwait 100\n")
  expect_eq("key ${k} opens ${pg}" "${k${pg}_page}" "${pg}")
  expect_eq("key ${k} counted" "${k${pg}_keys}" 1)
endforeach()

# 4. --view lands on the page name a door reaches (the same desk_page_goto, so the same page and history)
foreach(pg queue playlists library songinfo nowplaying)
  nav_run(v${pg} ${pg} "wait 100\n")
  expect_eq("--view ${pg}" "${v${pg}_page}" "${pg}")
endforeach()
# --view enters through the same entry as a door, so it is in the page history exactly as the door's page is
nav_run(dq home "wait 100\ndoor 4\nwait 100\n")
expect_eq("--view queue records the page history like the door" "${vqueue_hist}" "${dq_hist}")

# 5. the Now Playing door opens the page of its own, drawn (I266 a), not the song detail
nav_run(np home "wait 100\ndoor 5\nwait 100\n")
expect_eq("Now Playing door opens nowplaying" "${np_page}" "nowplaying")
if(np_npdraws EQUAL 0)
  message(FATAL_ERROR "the Now Playing page was never drawn (np_page_draws=0)")
endif()
nav_run(si home "wait 100\ndoor 6\nwait 100\n")
expect_eq("Song Information door opens songinfo" "${si_page}" "songinfo")

# 6. back and forward (I266 b): two doors, back to the first, forward to the second
nav_run(bk home "wait 100\ndoor 3\nwait 100\ndoor 4\nwait 100\nkey backspace\nwait 100\n")
expect_eq("Backspace goes back" "${bk_page}" "playlists")
nav_run(fw home "wait 100\ndoor 3\nwait 100\ndoor 4\nwait 100\nkey backspace\nwait 100\nkey alt+right\nwait 100\n")
expect_eq("Alt+Right goes forward" "${fw_page}" "queue")
nav_run(al home "wait 100\ndoor 3\nwait 100\ndoor 4\nwait 100\nkey alt+left\nwait 100\n")
expect_eq("Alt+Left goes back" "${al_page}" "playlists")
# the history is bounded and walking it adds no entry
nav_run(wl home "wait 100\ndoor 3\nwait 100\ndoor 4\nwait 100\nkey backspace\nwait 100\nkey alt+right\nwait 100\n")
expect_eq("walking back and forward keeps the history" "${wl_hist}" "${bk_hist}")
# the arrow buttons: both greyed (zero clicks counted) on a fresh start, working after two doors
nav_run(ar home "wait 100\ndoor 3\nwait 100\ndoor 4\nwait 100\n")
centre_of(bx by "${ar_back}")
nav_run(ab home "wait 100\ndoor 3\nwait 100\ndoor 4\nwait 100\nclick ${bx} ${by}\nwait 100\n")
expect_eq("the back arrow goes back" "${ab_page}" "playlists")
expect_eq("the back arrow click counted" "${ab_arrows}" 1)
centre_of(fx fy "${ar_fwd}")
nav_run(af home "wait 100\ndoor 3\nwait 100\ndoor 4\nwait 100\nkey backspace\nwait 100\nclick ${fx} ${fy}\nwait 100\n")
expect_eq("the forward arrow goes forward" "${af_page}" "queue")
# the swipe back goes through the same history
nav_run(sw home "wait 100\ndoor 3\nwait 100\ndoor 4\nwait 100\nswipe right\nwait 100\n")
expect_eq("a swipe is a page back" "${sw_page}" "playlists")

# 7. the Search highlight clears (I266 d): the lit door follows the page, so leaving Search by key or door moves it
nav_run(sh home "wait 100\ndoor 1\nwait 100\nkey /\nwait 100\ndoor 4\nwait 100\n")
expect_eq("leaving Search by a door" "${sh_page}" "queue")
expect_eq("only the Queue door is lit after leaving Search by a door (bit 4)" "${sh_lit}" "16")
nav_run(sk home "wait 100\nkey /\nwait 100\nkey Escape\nwait 100\nkey l\nwait 100\n")
expect_eq("leaving Search by a page key" "${sk_page}" "library")
nav_run(sl home "wait 100\ndoor 1\nwait 100\nkey /\nwait 100\nkey Escape\nwait 100\nkey l\nwait 100\n")
expect_eq("only the Library door is lit after a page key leaves Search (bit 2)" "${sl_lit}" "4")

# 8. a click on the Home search field goes to Search with the field focused (I266 f), the field and its ring one rect (g)
nav_run(hf home "wait 100\n")
centre_of(sx sy "${hf_field}")
nav_run(hc home "wait 100\nclick ${sx} ${sy}\nwait 100\n")
expect_eq("a Home field click opens Search" "${hc_page}" "search")
if(hc_caret EQUAL 0)
  message(FATAL_ERROR "the field was not focused after the Home click (no caret drawn)")
endif()
# the caret is a thin bar (2 px wide at scale 1, 18 px tall), not a typed "|" glyph
string(REPLACE " " ";" cr "${hc_caret_rect}")
list(GET cr 2 caret_w)
list(GET cr 3 caret_h)
expect_eq("the caret is a bar 2 px wide" "${caret_w}" "2")
expect_eq("the caret bar is 18 px tall" "${caret_h}" "18")
string(REPLACE " " ";" f "${hc_field}")
string(REPLACE " " ";" rg "${hc_ring}")
list(GET f 0 fx0)
list(GET f 1 fy0)
list(GET f 2 fw0)
list(GET f 3 fh0)
math(EXPR rx "${fx0} - 2")
math(EXPR ry "${fy0} - 2")
math(EXPR rw "${fw0} + 4")
math(EXPR rh "${fh0} + 4")
expect_eq("the focus ring is the field grown by the ring width" "${hc_ring}" "${rx} ${ry} ${rw} ${rh}")
# the Home frame and the Search page draw the field at one rect
expect_eq("the Home field is the Search field" "${hf_field}" "${hc_field}")
# the Home field click keeps the typed query (I266 f); a door into Search starts empty
nav_run(hq home "wait 100\nkey /\nwait 100\nkey m\nkey o\nkey d\nwait 400\ndoor 0\nwait 100\nclick ${sx} ${sy}\nwait 400\n")
expect_eq("the Home field click lands on Search" "${hq_page}" "search")
expect_eq("the Home field click keeps the query" "${hq_query}" "mod")
nav_run(hd home "wait 100\nkey /\nwait 100\nkey m\nkey o\nkey d\nwait 400\ndoor 0\nwait 100\ndoor 1\nwait 400\n")
expect_eq("a door into Search starts with an empty query" "${hd_query}" "")
