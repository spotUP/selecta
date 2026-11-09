# I1190 text-ui-menu: the text UI song menu and playlist menu (key w, overlay) over the uimodel label and line builders. The exit report
# prints text_menu opens / picks / answers / log (the sentinel only the menu code moves). Real content: --db filled by --exec LIBRARY_SCAN.
#   -DSELECTA=<exe> -DSONGS=<dir with .mod files> -DKEYS_<A,B>=<keys files> -DWORK=<dir>
# Exemption: Save to Library for online rows is not in the text UI (no online rows there); no Amiga text UI check (no rig runs).
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")

execute_process(COMMAND "${SELECTA}" --output null --size 100x30 --start-dir "${WORK}" --db "${WORK}/seed.db"
                        --exec "LIBRARY_SCAN ${SONGS}" --keys "${KEYS_B}"
                OUTPUT_VARIABLE o ERROR_VARIABLE e RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "seed scan exited ${rc}\n${o}\n${e}")
endif()

function(run tag keys)
  file(COPY_FILE "${WORK}/seed.db" "${WORK}/${tag}.db")
  execute_process(
    COMMAND "${SELECTA}" --output null --size 100x30 --start-dir "${WORK}" --db "${WORK}/${tag}.db"
            --keys "${keys}" --screen-dump "${WORK}/${tag}.txt"
    OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE rc)
  if(NOT rc EQUAL 0)
    message(FATAL_ERROR "run ${tag} exited ${rc}\n${out}\n${err}")
  endif()
  set(out_${tag} "${out}" PARENT_SCOPE)
  file(READ "${WORK}/${tag}.txt" scr)
  set(scr_${tag} "${scr}" PARENT_SCOPE)
endfunction()

function(expect tag text what)
  string(FIND "${${text}}" "${what}" at)
  if(at LESS 0)
    message(FATAL_ERROR "${tag}: missing '${what}' in:\n${${text}}")
  endif()
endfunction()

# a: menu on the empty playlist tree: New Playlist "MM", then the menu on that playlist: Rename to "Z".
run(a "${KEYS_A}")
expect(a out_a "text_menu opens=2 picks=2 answers=2")
expect(a out_a "PLAYLIST_CREATE \"MM\" 0|PLAYLIST_RENAME 1 \"Z\"")

# b: menu on a song: Play Next sends the engine verb for that song.
run(b "${KEYS_B}")
expect(b out_b "text_menu opens=1 picks=1 answers=0")
expect(b out_b "QUEUE_PLAY_NEXT")
