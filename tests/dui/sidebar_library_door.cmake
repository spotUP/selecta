# dui-sidebar-library-door (I673): the sidebar "Your Library" header is the door to the Library page (Spotify), the duplicate Your
# Library door row is gone, and the Albums / Artists / Composers chips open the Library page on that kind. -DSELECTA -DSONGS -DWORK
# Entry point: scripted pointer clicks through the real desktop UI (--input), read back from the `nav`, `sidebar_doors`, `sidehdr title`,
# `sidechev` and `mode` stats lines. Sentinel: nav_clicks counts the header click, and the chip click moves the Library `mode` line.
get_filename_component(HERE "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
include("${HERE}/dui_run.cmake")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
configure_file("${SONGS}/selecta-test.mod" "${WORK}/one.mod" COPYONLY)

function(expect_eq what got want)
  if(NOT "${got}" STREQUAL "${want}")
    message(FATAL_ERROR "${what}: got '${got}', expected '${want}'")
  endif()
endfunction()

function(run tag script)
  file(REMOVE_RECURSE "${WORK}/cfg")
  file(WRITE "${WORK}/${tag}.txt" "${script}")
  dui_run(o --ui desktop --backend mem --view home --canvas 1280x800 --clock fixed --stats
    --exec "QUEUE_ADD ${WORK}/one.mod" --input "${WORK}/${tag}.txt")
  set(out "${o}" PARENT_SCOPE)
  if(NOT "${o}" MATCHES "nav page=([a-z]+) nav_clicks=([0-9]+)")
    message(FATAL_ERROR "no nav line in:\n${o}")
  endif()
  set(${tag}_page "${CMAKE_MATCH_1}" PARENT_SCOPE)
  set(${tag}_clicks "${CMAKE_MATCH_2}" PARENT_SCOPE)
  if("${o}" MATCHES "\nmode ([A-Za-z]+)\n")
    set(${tag}_mode "${CMAKE_MATCH_1}" PARENT_SCOPE)
  else()
    set(${tag}_mode "" PARENT_SCOPE)
  endif()
  if("${o}" MATCHES "sidebar_doors drawn=([0-9]+)")
    set(${tag}_doors "${CMAKE_MATCH_1}" PARENT_SCOPE)
  endif()
endfunction()

# 0. Home frame: where the header title and the chip chevrons are, and which door rows drew.
run(base "wait 100\n")
expect_eq("starts on Home" "${base_page}" "home")
if(NOT "${out}" MATCHES "sidehdr title ([0-9]+) ([0-9]+) ([0-9]+) ([0-9]+)")
  message(FATAL_ERROR "no sidehdr title line in:\n${out}")
endif()
math(EXPR hx "${CMAKE_MATCH_1} + ${CMAKE_MATCH_3} / 2")
math(EXPR hy "${CMAKE_MATCH_2} + ${CMAKE_MATCH_4} / 2")
if(NOT "${out}" MATCHES "sidechev L [0-9]+ [0-9]+ [0-9]+ [0-9]+ R ([0-9]+) ([0-9]+) ([0-9]+) ([0-9]+)")
  message(FATAL_ERROR "no sidechev line in:\n${out}")
endif()
math(EXPR rx "${CMAKE_MATCH_1} + ${CMAKE_MATCH_3} / 2")
math(EXPR ry "${CMAKE_MATCH_2} + ${CMAKE_MATCH_4} / 2")

# 1. The duplicate Your Library door row (door 2, bit 4) is not drawn; Playlists..Song Information (doors 3..6) still are.
math(EXPR lib_door_bit "${base_doors} & 4")
expect_eq("the Your Library door row is gone" "${lib_door_bit}" "0")
math(EXPR rest_doors "${base_doors} & 120")
expect_eq("doors 3..6 still drawn" "${rest_doors}" "120")

# 2. A click on the header title opens the Library page.
run(hdr "wait 100\nclick ${hx} ${hy}\nwait 100\n")
expect_eq("header click opens the Library page" "${hdr_page}" "library")
expect_eq("header click counted as a navigation" "${hdr_clicks}" "1")

# 3. I678b: the chips filter the sidebar list in place and never leave the page (the Library page is opened by the header only).
run(c1 "wait 100\nclick ${rx} ${ry}\nwait 100\n")
expect_eq("Playlists chip stays on Home" "${c1_page}" "home")
run(c2 "wait 100\nclick ${rx} ${ry}\nwait 100\nclick ${rx} ${ry}\nwait 100\n")
expect_eq("Albums chip stays on Home" "${c2_page}" "home")
run(c4 "wait 100\nclick ${rx} ${ry}\nwait 100\nclick ${rx} ${ry}\nwait 100\nclick ${rx} ${ry}\nwait 100\nclick ${rx} ${ry}\nwait 100\n")
expect_eq("Composers chip stays on Home" "${c4_page}" "home")
