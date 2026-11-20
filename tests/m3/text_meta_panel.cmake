# I1465t text-ui-meta-panel (design 2026-11-20-i1465-i1468 section 6 text UI, section 7): the text UI Now Playing panel shows the shared
# sel_uim_np_lines plus the stills count and the video link of the song's META KEY. The played song has no art of its own; a title + composer twin
# (other songs row) holds the stills and the dz_video link, so the lines can only appear through the meta key.
#   -DSEED=<text_meta_seed> -DSELECTA=<exe> -DMOD=<a .mod file> -DWORK=<dir>
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}/cfg")
file(COPY "${MOD}" DESTINATION "${WORK}")
get_filename_component(modname "${MOD}" NAME)
set(song "${WORK}/${modname}")
execute_process(COMMAND "${SEED}" "${WORK}/lib.db" "${song}" OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE rc)
if(NOT rc EQUAL 0 OR NOT out MATCHES "text_meta_seed_ok")
  message(FATAL_ERROR "seed failed rc=${rc}\n${out}\n${err}")
endif()
file(WRITE "${WORK}/k.keys" "wait 1500\n")
execute_process(
  COMMAND "${SELECTA}" --output null --size 140x30 --db "${WORK}/lib.db" --config-dir "${WORK}/cfg"
          --keys "${WORK}/k.keys" --screen-dump "${WORK}/s.txt" "${song}"
  WORKING_DIRECTORY "${WORK}" OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "selecta exited ${rc}\n${out}\n${err}")
endif()
file(READ "${WORK}/s.txt" screen)
# the shared panel lines (sel_uim_np_lines: title row) and the two meta-key lines of the text UI
foreach(want "Now Playing" "Selecta Test" "Rating 0/5" "2 screenshots" "Demo https://x.test/demo")
  string(FIND "${screen}" "${want}" at)
  if(at LESS 0)
    message(FATAL_ERROR "missing '${want}' in:\n${screen}")
  endif()
endforeach()
message(STATUS "text_meta_panel ok")
