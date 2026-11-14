# I1186 text-ui-styles: the text UI answers the shared table's six VIEW_STYLE keys (Alt-6..Alt-0, Alt-c) and draws Grid (cell
# columns), Cover Flow (cursor row in the middle with neighbours) and Compact (one dense line per song).
#   -DSELECTA=<exe> -DCWD=<dir holding selecta-test.mod> -DKEYS_<A..D>=<keys files> -DWORK=<dir>
# Each run drives the real selecta binary through --keys. The exit report prints text_style (the sel_view_style of the last
# pane draw) and style_draws (pane draws done by the Grid / Cover Flow / Compact code).
# Cover art has no text form: the Cover Flow cover itself is exempt (artist and title lines stand in for it).
file(REMOVE_RECURSE "${WORK}")
set(songs "${WORK}/songs")
file(MAKE_DIRECTORY "${songs}" "${WORK}/cfg")
foreach(n alpha bravo charlie delta)
  file(COPY_FILE "${CWD}/selecta-test.mod" "${songs}/${n}.mod")
endforeach()

function(run tag keys)
  execute_process(
    COMMAND "${SELECTA}" --output null --size 160x25 --start-dir "${songs}" --config-dir "${WORK}/cfg"
            --exec "QUEUE_ADD ${songs}/alpha.mod" --exec "QUEUE_ADD ${songs}/bravo.mod"
            --exec "QUEUE_ADD ${songs}/charlie.mod" --exec "QUEUE_ADD ${songs}/delta.mod"
            --keys "${keys}" --screen-dump "${WORK}/${tag}.txt"
    WORKING_DIRECTORY "${songs}"
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

# a: Alt-9 is Grid (style 3): the four songs sit side by side on one row, not one under another.
run(a "${KEYS_A}")
expect(a out_a "text_style=3 style_draws=")
string(REGEX MATCH "[^\n]*alpha[^\n]*bravo[^\n]*charlie[^\n]*" row_a "${scr_a}")
if(row_a STREQUAL "")
  message(FATAL_ERROR "a: grid cells are not in columns:\n${scr_a}")
endif()

# b: Alt-0 is Cover Flow (style 4): the cursor row is bracketed, its neighbours sit on the lines around it.
run(b "${KEYS_B}")
expect(b out_b "text_style=4 style_draws=")
string(REGEX MATCH "<  [^\n]*charlie[^\n]*  >" mid_b "${scr_b}")
if(mid_b STREQUAL "")
  message(FATAL_ERROR "b: no centre row:\n${scr_b}")
endif()
expect(b scr_b "bravo")
expect(b scr_b "delta")

# c: Alt-c is Compact (style 5): one line per song.
run(c "${KEYS_C}")
expect(c out_c "text_style=5 style_draws=")
expect(c scr_c "alpha")
string(REGEX MATCH "[^\n]*alpha[^\n]*bravo[^\n]*" same_c "${scr_c}")
if(NOT same_c STREQUAL "")
  message(FATAL_ERROR "c: compact put two songs on one line:\n${scr_c}")
endif()

# d: Alt-9 then Alt-6 is Regular again (style 0).
run(d "${KEYS_D}")
expect(d out_d "text_style=0 style_draws=")
