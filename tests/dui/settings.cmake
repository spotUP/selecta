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
if(NOT table EQUAL 15)
  message(FATAL_ERROR "host Settings shows ${table} rows, expected 15 (MPS-1b + MPS-1c ROM rows machine filter)")
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

# MPS-1c: the C64 ROM rows, through the same shell. A file chosen with the row's button (the dialog answers --dialog-answer) is
# size-checked and copied into <cfg> as kernal / basic / chargen; the row reports "Your ROM" until Reset to Bundled removes it.
function(rom_run outvar answer clicks)
  file(WRITE "${WORK}/in.txt" "wait 300\nmove ${ex} ${ey}\nclick ${ex} ${ey}\nwait 200\n${clicks}wait 300\n")
  dui_run(o --db "${WORK}/library.db" --ui desktop --backend mem --view home --canvas 1280x1200 --clock fixed --stats
    --input "${WORK}/in.txt" --dialog-answer "${answer}")
  set(${outvar} "${o}" PARENT_SCOPE)
endfunction()

# The canvas is tall enough that the last rows clear the player bar (a click under it never reaches the page).
# Find a ROM row's two buttons on the opened page.
function(rom_buttons outvar key)
  rom_run(page "${WORK}/none" "")
  if(NOT "${page}" MATCHES "settings_row ([0-9]+) (-?[0-9]+) (-?[0-9]+) ([0-9]+) ([0-9]+) key=${key}\nsettings_reset [0-9]+ (-?[0-9]+) (-?[0-9]+) ([0-9]+) ([0-9]+)\n")
    message(FATAL_ERROR "no ${key} row with a reset button:\n${page}")
  endif()
  math(EXPR chx "${CMAKE_MATCH_2} + ${CMAKE_MATCH_4} / 2")
  math(EXPR chy "${CMAKE_MATCH_3} + ${CMAKE_MATCH_5} / 2")
  math(EXPR rsx "${CMAKE_MATCH_6} + ${CMAKE_MATCH_8} / 2")
  math(EXPR rsy "${CMAKE_MATCH_7} + ${CMAKE_MATCH_9} / 2")
  set(${outvar} "move ${chx} ${chy}\nclick ${chx} ${chy}\nwait 200\n" PARENT_SCOPE)
  set(${outvar}_reset "move ${rsx} ${rsy}\nclick ${rsx} ${rsy}\nwait 200\n" PARENT_SCOPE)
endfunction()

string(REPEAT "K" 8192 kernal_bytes)
file(WRITE "${WORK}/good_kernal.bin" "${kernal_bytes}")
file(WRITE "${WORK}/short.bin" "too short")
file(REMOVE "${WORK}/cfg/kernal" "${WORK}/cfg/basic" "${WORK}/cfg/chargen")

rom_buttons(kernal_pick sid.rom.kernal)
rom_buttons(basic_pick sid.rom.basic)
rom_run(fresh "${WORK}/none" "")
if(NOT "${fresh}" MATCHES "rom loads=0 resets=0 refused=0\nrom_row 0 source=Open ROMs \\(bundled\\) err=\nrom_row 1 source=Open ROMs \\(bundled\\) err=\n")
  message(FATAL_ERROR "ROM rows do not start on the bundled ROMs:\n${fresh}")
endif()

# A wrong-sized file: inline error, nothing copied.
rom_run(bad "${WORK}/short.bin" "${basic_pick}")
if(NOT "${bad}" MATCHES "rom loads=0 resets=0 refused=1\n" OR NOT "${bad}" MATCHES "rom_row 1 source=Open ROMs \\(bundled\\) err=Wrong size: 9 bytes, expected 8192\\.")
  message(FATAL_ERROR "the wrong-sized ROM was not refused inline:\n${bad}")
endif()
if(EXISTS "${WORK}/cfg/basic")
  message(FATAL_ERROR "a refused ROM was copied")
endif()

# A right-sized file: copied, the row flips to Your ROM.
rom_run(good "${WORK}/good_kernal.bin" "${kernal_pick}")
if(NOT "${good}" MATCHES "rom loads=1 resets=0 refused=0\nrom_row 0 source=Your ROM err=\n")
  message(FATAL_ERROR "the ROM was not loaded through the Settings button:\n${good}")
endif()
file(SIZE "${WORK}/cfg/kernal" kernal_size)
if(NOT kernal_size EQUAL 8192)
  message(FATAL_ERROR "cfg/kernal is ${kernal_size} bytes")
endif()

# Reset to Bundled removes the copy (a new process: the source comes from the file, not from memory).
rom_run(reset "${WORK}/none" "${kernal_pick_reset}")
if(NOT "${reset}" MATCHES "rom loads=0 resets=1 refused=0\nrom_row 0 source=Open ROMs \\(bundled\\) err=\n" OR EXISTS "${WORK}/cfg/kernal")
  message(FATAL_ERROR "Reset to Bundled did not remove the ROM:\n${reset}")
endif()
message(STATUS "dui-settings: ROM rows load, refuse a wrong size and reset")
