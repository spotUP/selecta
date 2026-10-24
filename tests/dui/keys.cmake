# dui-keys (DUI-P3-3): keyboard parity of the desktop UI, through the real shell and the scripted --input feeder.
#   -DSELECTA -DSONGS -DWORK -DGOLDEN
# 1. Every row of the action table is invoked once by its key (`key ctrl+r` lines generated from `--actions`): the number
#    of actions fired and the number of distinct rows among them both equal the row count printed by the table itself
#    (no hardcoded number). A key shared by two rows, or a row without a key, makes the counts differ.
# 2. Tab moves the focus ring through the layout stops, Shift+Tab goes back, and the ring changes the canvas.
# 3. Down and Up are arrow navigation in lists; `/` starts type-to-search, printable keys go to the query, Enter sends
#    the search and Escape leaves without one.
# 4. Every script verb parses (`click`, `wheel`, `pinch`, `drop`, `resize`, `wait`) and the shell survives them.
get_filename_component(HERE "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
include("${HERE}/dui_run.cmake")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")

dui_run(table --ui desktop --backend mem --actions)
if(NOT table MATCHES "actions count=([0-9]+) ")
  message(FATAL_ERROR "no action count in:\n${table}")
endif()
set(count ${CMAKE_MATCH_1})

# The key names of keybind.c as script specs: C-x is ctrl+x, M-x is alt+x, S-x is shift+x, names are lower case.
string(REPLACE "\n" ";" lines "${table}")
set(script "")
set(rows 0)
foreach(line IN LISTS lines)
  if(line MATCHES "^action id=([a-z_]+) .* key=([^ ]+) line=")
    set(id "${CMAKE_MATCH_1}")
    set(spec "${CMAKE_MATCH_2}")
    math(EXPR rows "${rows} + 1")
    set(prefix "")
    while(spec MATCHES "^([CMS])-(.+)$")
      if(CMAKE_MATCH_1 STREQUAL "C")
        string(APPEND prefix "ctrl+")
      elseif(CMAKE_MATCH_1 STREQUAL "M")
        string(APPEND prefix "alt+")
      else()
        string(APPEND prefix "shift+")
      endif()
      set(spec "${CMAKE_MATCH_2}")
    endwhile()
    if(spec MATCHES "^[A-Za-z][A-Za-z0-9]+$")
      string(TOLOWER "${spec}" spec)
    endif()
    string(APPEND script "key ${prefix}${spec}\n")
  endif()
endforeach()
if(NOT rows EQUAL count)
  message(FATAL_ERROR "the table printed ${rows} action rows but says count=${count}")
endif()
file(WRITE "${WORK}/all.txt" "${script}")
dui_run(all --ui desktop --backend mem --stats --clock fixed --input "${WORK}/all.txt")
dui_stat(fired "${all}" desktop actions_fired)
dui_stat(distinct "${all}" desktop actions_distinct)
if(NOT fired EQUAL count)
  message(FATAL_ERROR "${fired} actions fired by key, the table has ${count}")
endif()
if(NOT distinct EQUAL count)
  message(FATAL_ERROR "${distinct} distinct actions fired by key, the table has ${count} (a shared or missing key)")
endif()

# Focus ring: Tab x2 then Shift+Tab leaves the focus on stop 0 (stat is stop + 1) after 3 moves, and the ring is drawn.
dui_run(base --ui desktop --backend mem --stats --clock fixed --canvas 1280x800)
dui_stat(base_hash "${base}" desktop canvas_hash)
file(WRITE "${WORK}/tab.txt" "key tab\nkey tab\nkey shift+tab\n")
dui_run(tab --ui desktop --backend mem --stats --clock fixed --canvas 1280x800 --input "${WORK}/tab.txt")
dui_stat(moves "${tab}" desktop focus_moves)
dui_stat(focus "${tab}" desktop focus)
dui_stat(tab_hash "${tab}" desktop canvas_hash)
if(NOT moves EQUAL 3)
  message(FATAL_ERROR "3 Tab presses must move the focus 3 times (moved ${moves})")
endif()
if(NOT focus EQUAL 1)
  message(FATAL_ERROR "Tab, Tab, Shift+Tab must leave the focus on stop 0 (stat 1), got stat ${focus}")
endif()
if(tab_hash EQUAL base_hash)
  message(FATAL_ERROR "the focus ring did not change the canvas")
endif()

# Arrow navigation and type-to-search.
file(WRITE "${WORK}/nav.txt" "key down\nkey down\nkey up\nkey /\nkey a\nkey b\nkey backspace\nkey c\nkey enter\n"
  "key /\nkey x\nkey escape\n")
dui_run(nav --ui desktop --backend mem --stats --clock fixed --input "${WORK}/nav.txt")
dui_stat(arrows "${nav}" desktop arrows)
dui_stat(searches "${nav}" desktop searches)
dui_stat(nav_focus "${nav}" desktop focus)
if(NOT arrows EQUAL 3)
  message(FATAL_ERROR "Down, Down, Up must navigate 3 steps (got ${arrows})")
endif()
if(NOT searches EQUAL 1)
  message(FATAL_ERROR "one Enter in the search field must send one search, Escape none (sent ${searches})")
endif()

# Every verb of the script parses and the shell survives it.
file(WRITE "${WORK}/verbs.txt"
  "key ctrl+f\nclick 640 120\nwheel 0 -3\npinch 1.25\ndrop /nonexistent/a.mod\nresize 900 600\nwait 200\n")
dui_run(verbs --ui desktop --backend mem --stats --clock fixed --input "${WORK}/verbs.txt")
