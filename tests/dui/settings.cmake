# dui-settings (I799a, DUI-P4-14): the desktop Settings page, through the real shell and the scripted --input feeder
# (--output null, a temp config dir; ~/.config/selecta is never touched). -DSELECTA -DSONGS -DWORK
#  1. the Settings entry in the top bar is found in the dump and CLICKED (not --view settings);
#  2. sentinel: `settings rows=N table=N` - the page drew exactly one row per sel_uim_pref_row (the machine-filtered table the MUI window reads too, MPS-1b: 12 rows on the host, sid.* in, playsid.* and overscan out);
#  3. the first cycle row (mix.loudness, track,auto,off) is clicked: its control steps track -> auto and the engine writes
#     mix.loudness=auto to <cfg>/prefs.conf (sets>=1 is the sentinel that PREF_SET went out).
get_filename_component(HERE "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
include("${HERE}/dui_run.cmake")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")

function(st_run outvar input)
  file(WRITE "${WORK}/in.txt" "${input}")
  dui_run(o --db "${WORK}/library.db" --ui desktop --backend mem --view home --canvas 1280x800 --clock fixed --stats
    --input "${WORK}/in.txt")
  set(${outvar} "${o}" PARENT_SCOPE)
endfunction()

# Pass 1: find the entry.
st_run(base "wait 300\n")
if(NOT "${base}" MATCHES "settings draws=[0-9]+ rows=[0-9]+ table=([0-9]+) sets=[0-9]+ clicks=[0-9]+ entry_clicks=[0-9]+ entry (-?[0-9]+) (-?[0-9]+) ([0-9]+) ([0-9]+)\n")
  message(FATAL_ERROR "no settings line in:\n${base}")
endif()
set(table "${CMAKE_MATCH_1}")
if(CMAKE_MATCH_4 EQUAL 0)
  message(FATAL_ERROR "the top bar has no Settings entry at 1280x800:\n${base}")
endif()
math(EXPR ex "${CMAKE_MATCH_2} + ${CMAKE_MATCH_4} / 2")
math(EXPR ey "${CMAKE_MATCH_3} + ${CMAKE_MATCH_5} / 2")

# Pass 2: click the entry, read the rows.
st_run(opened "wait 300\nmove ${ex} ${ey}\nclick ${ex} ${ey}\nwait 200\n")
if(NOT "${opened}" MATCHES "settings draws=([0-9]+) rows=([0-9]+) table=([0-9]+) sets=0 clicks=0 entry_clicks=1 ")
  message(FATAL_ERROR "the entry click did not open Settings:\n${opened}")
endif()
if(NOT CMAKE_MATCH_2 EQUAL CMAKE_MATCH_3 OR NOT CMAKE_MATCH_3 EQUAL table OR table LESS 1)
  message(FATAL_ERROR "rows drawn ${CMAKE_MATCH_2} != table ${CMAKE_MATCH_3}")
endif()
if(NOT table EQUAL 12)
  message(FATAL_ERROR "host Settings shows ${table} rows, expected 12 (MPS-1b machine filter)")
endif()
if(NOT "${opened}" MATCHES "settings_row 0 (-?[0-9]+) (-?[0-9]+) ([0-9]+) ([0-9]+) key=mix.loudness\n")
  message(FATAL_ERROR "row 0 is not mix.loudness:\n${opened}")
endif()
math(EXPR cx "${CMAKE_MATCH_1} + ${CMAKE_MATCH_3} / 2")
math(EXPR cy "${CMAKE_MATCH_2} + ${CMAKE_MATCH_4} / 2")

# Pass 3: click the entry, then the enum control.
st_run(changed "wait 300\nmove ${ex} ${ey}\nclick ${ex} ${ey}\nwait 200\nmove ${cx} ${cy}\nclick ${cx} ${cy}\nwait 300\n")
if(NOT "${changed}" MATCHES "settings draws=[0-9]+ rows=${table} table=${table} sets=1 clicks=1 entry_clicks=1 ")
  message(FATAL_ERROR "the control click did not send PREF_SET:\n${changed}")
endif()
file(READ "${WORK}/cfg/prefs.conf" prefs)
if(NOT prefs MATCHES "mix.loudness = auto")
  message(FATAL_ERROR "prefs.conf lacks mix.loudness = auto:\n${prefs}")
endif()
message(STATUS "dui-settings: ${table} rows drawn, entry clicked, mix.loudness=auto written")
