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
endfunction()

# 1. every door, clicked from the Home frame
set(door_pages home search library playlists queue nowplaying)
nav_run(h0 home "wait 100\n")
expect_eq("Home frame page before any click" "${h0_page}" "home")
expect_eq("no click yet" "${h0_clicks}" 0)
expect_eq("six doors" "${h0_doors}" 6)
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

# 4. --view lands on the page name a door reaches
foreach(pg queue playlists library songinfo)
  nav_run(v${pg} ${pg} "wait 100\n")
  expect_eq("--view ${pg}" "${v${pg}_page}" "${pg}")
endforeach()
