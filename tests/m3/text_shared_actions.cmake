# I1184 text-ui-shared-actions: the text UI binds the shared action table's keys.
#   -DSELECTA=<exe> -DCWD=<dir holding selecta-test.mod> -DKEYS_A/B/C=<keys> -DWORK=<dir>
# Each run drives the real selecta binary through --keys. The exit report prints the engine option state and the
# sentinel text_table_actions / log: the table lines tui_key sent after a key matched a shared action row.
file(REMOVE_RECURSE "${WORK}")
set(songs "${WORK}/songs")
file(MAKE_DIRECTORY "${songs}" "${WORK}/cfg")
file(COPY "${CWD}/selecta-test.mod" DESTINATION "${songs}")

function(run tag keys)
  execute_process(
    COMMAND "${SELECTA}" --output null --size 100x25 --start-dir "${songs}" --config-dir "${WORK}/cfg"
            --exec "QUEUE_ADD ${songs}/selecta-test.mod" --keys "${keys}" --screen-dump "${WORK}/${tag}.txt"
    WORKING_DIRECTORY "${songs}"
    OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE rc)
  set(out_${tag} "${out}" PARENT_SCOPE)
  set(rc_${tag} "${rc}" PARENT_SCOPE)
  if(EXISTS "${WORK}/${tag}.txt")
    file(READ "${WORK}/${tag}.txt" scr)
  else()
    set(scr "")
  endif()
  set(scr_${tag} "${scr}" PARENT_SCOPE)
endfunction()

function(expect tag text what)
  string(FIND "${${text}}" "${what}" at)
  if(at LESS 0)
    message(FATAL_ERROR "${tag}: missing '${what}' in:\n${${text}}")
  endif()
endfunction()

function(reject tag text what)
  string(FIND "${${text}}" "${what}" at)
  if(NOT at LESS 0)
    message(FATAL_ERROR "${tag}: unexpected '${what}' in:\n${${text}}")
  endif()
endfunction()

run(a "${KEYS_A}")
if(NOT rc_a EQUAL 0)
  message(FATAL_ERROR "run a exited ${rc_a}\n${out_a}")
endif()
expect(a out_a "options random=on repeat=all queue_count=1")
expect(a out_a "text_table_actions=4 log=OPTION random on|REPEAT all|QUEUE_SHUFFLE|RESHUFFLE")
expect(a scr_a "Shuffle")
expect(a scr_a "Repeat all")

run(b "${KEYS_B}")
if(NOT rc_b EQUAL 0)
  message(FATAL_ERROR "run b exited ${rc_b}\n${out_b}")
endif()
expect(b out_b "options random=off repeat=one queue_count=0")
expect(b out_b "log=OPTION random on|OPTION random off|REPEAT one|NEXT_ALBUM|PREV_ALBUM|LIKE|STATS_HIDE|QUEUE_CLEAR")
reject(b scr_b "Shuffle")
expect(b scr_b "Repeat one")

run(c "${KEYS_C}")
expect(c out_c "repeat=off")
expect(c out_c "ui_closed id=text")
