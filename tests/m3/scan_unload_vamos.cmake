# I1314b (Johan 2026-11-11, "unload after scan"): player modules a library scan loads leave memory when the scan
# ends; the next play loads the module from disk again. The 68k script scans one Organya song (Players/sel_organya
# is a LoadSeg module and the scan opens the song through it), prints the counters at scan end, then plays the song
# and prints them again. Pass: after the scan module_loads==1, module_unloads==1, modules_resident==0; after the
# play module_loads==2 and modules_resident==1 (the playing song holds its module). free_ram (AvailMem) at both
# points gives the RAM the unload returned. Revert proof: without sel_plugin_unload_since in scan_collect the scan
# end keeps the module (modules_resident=1) and the play reuses it (module_loads stays 1).
# Inputs: MODE=vamos, CLI, VAMOS, AMIGA_CLI, SONG, WORK. Included helpers: tests/m2/lib.cmake.
get_filename_component(HERE "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
include("${HERE}/../m2/lib.cmake")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}/songs")
file(COPY_FILE "${SONG}" "${WORK}/songs/song.org")
file(WRITE "${WORK}/unload.sel"
  "LIBRARY_SCAN work:songs\n@scan_stats\nOPEN work:songs/song.org\nPLAY\n@render 4410\n@scan_stats\nSTOP\n")
set(VAMOS_VOLUMES -V "work:${WORK}")
m2_exec(out rc script work:unload.sel --db work:lib.db)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "RED[I1314b]: script failed rc=${rc}\n${out}")
endif()
string(REGEX MATCHALL "SCAN [^\n]*module_loads=[0-9]+ module_unloads=[0-9]+ modules_resident=[0-9]+ free_ram=[0-9]+"
       lines "${out}")
list(LENGTH lines n)
if(NOT n EQUAL 2)
  message(FATAL_ERROR "RED[I1314b]: want two SCAN lines, got ${n}\n${out}")
endif()
list(GET lines 0 at_scan)
list(GET lines 1 at_play)
m2_get("${at_scan}" scanned_files scanned)
m2_get("${at_scan}" module_loads loads1)
m2_get("${at_scan}" module_unloads unloads1)
m2_get("${at_scan}" modules_resident res1)
m2_get("${at_scan}" free_ram free1)
m2_get("${at_play}" module_loads loads2)
m2_get("${at_play}" modules_resident res2)
m2_get("${at_play}" free_ram free2)
if(NOT scanned EQUAL 1 OR NOT loads1 EQUAL 1)
  message(FATAL_ERROR "RED[I1314b]: the scan did not load the organya module (scanned=${scanned} "
                      "module_loads=${loads1})\n${out}")
endif()
if(NOT unloads1 EQUAL 1 OR NOT res1 EQUAL 0)
  message(FATAL_ERROR "RED[I1314b]: the scan end kept its module: module_unloads=${unloads1} (want 1), "
                      "modules_resident=${res1} (want 0)\n${at_scan}")
endif()
if(NOT loads2 EQUAL 2 OR NOT res2 EQUAL 1)
  message(FATAL_ERROR "RED[I1314b]: the play did not load the module again: module_loads=${loads2} (want 2), "
                      "modules_resident=${res2} (want 1)\n${out}")
endif()
math(EXPR delta "${free1} - ${free2}")
message("scan_unload_vamos_ok free_ram_after_scan=${free1} free_ram_playing=${free2} delta=${delta}")
