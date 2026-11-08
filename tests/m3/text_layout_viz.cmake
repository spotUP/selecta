# I1185 text-ui-layout-viz: the text UI answers the shared table's layout keys (Ctrl-1..4, PREF_SET ui.desktop.mode) and
# visualizer keys (Ctrl-5..8, PREF_SET ui.desktop.viz), including the SID registers panel.
#   -DSELECTA=<exe> -DCWD=<dir holding selecta-test.mod> -DSID_MODULES=<dir of the sidfeed fixture module>
#   -DKEYS_<A..E>=<keys files> -DWORK=<dir>
# Each run drives the real selecta binary through --keys. The exit report prints text_table_actions / log (the table lines
# tui_key sent), text_layout / layout_sets (the layout state and the keys taken) and sid_frames (SID panels drawn from a feed).
file(REMOVE_RECURSE "${WORK}")
set(songs "${WORK}/songs")
file(MAKE_DIRECTORY "${songs}" "${WORK}/cfg")
file(COPY "${CWD}/selecta-test.mod" DESTINATION "${songs}")
file(WRITE "${songs}/feed.selsid" "SELSID fixture")
set(ENV{SELECTA_PLUGIN_PATH} "${SID_MODULES}")

function(run tag song keys)
  execute_process(
    COMMAND "${SELECTA}" --output null --size 100x25 --start-dir "${songs}" --config-dir "${WORK}/cfg"
            --exec "OPEN ${songs}/${song}" --exec "PLAY" --keys "${keys}" --screen-dump "${WORK}/${tag}.txt"
    WORKING_DIRECTORY "${songs}"
    OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE rc)
  if(NOT rc EQUAL 0)
    message(FATAL_ERROR "run ${tag} exited ${rc}\n${out}\n${err}")
  endif()
  set(out_${tag} "${out}" PARENT_SCOPE)
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

# a: Ctrl-8 on a SID feed draws the register panel decoded from the fixture's registers.
run(a feed.selsid "${KEYS_A}")
expect(a out_a "text_layout=0 layout_sets=1 sid_frames=")
expect(a out_a "log=PREF_SET ui.desktop.viz viz_sidregs")
reject(a out_a "sid_frames=0")
expect(a scr_a "SID registers")
expect(a scr_a "Voice1 Fr 1000 Pw 800 Gate Pul")
expect(a scr_a "Flt")
expect(a scr_a "LP")
expect(a scr_a "Volume 15")

# b: Ctrl-5, Ctrl-6, Ctrl-7 pick Scope, Levels, Spectrum; Ctrl-8 without a register feed says so and counts no SID frame.
run(b selecta-test.mod "${KEYS_B}")
expect(b out_b "log=PREF_SET ui.desktop.viz viz_scope|PREF_SET ui.desktop.viz viz_levels|PREF_SET ui.desktop.viz viz_spectrum|PREF_SET ui.desktop.viz viz_sidregs")
expect(b out_b "layout_sets=4 sid_frames=0")
expect(b scr_b "SID registers")
expect(b scr_b "No SID register feed")

# c: Ctrl-3 Mini keeps only the header; Ctrl-1 Full brings the list back.
run(c selecta-test.mod "${KEYS_C}")
expect(c out_c "text_layout=2 layout_sets=1")
reject(c scr_c "Backspace Parent")
reject(c scr_c "Open")
run(d selecta-test.mod "${KEYS_D}")
expect(d out_d "text_layout=0 layout_sets=2")
expect(d scr_d "Backspace Parent")

# e: Ctrl-4 Visual shows the visualizer panel alone.
run(e selecta-test.mod "${KEYS_E}")
expect(e out_e "text_layout=3 layout_sets=1")
expect(e scr_e "Scope")
reject(e scr_e "Backspace Parent")
