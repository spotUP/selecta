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

# c: Ctrl-3 Mini keeps only the player bar (I1222g: the hint row is gone, the "Browse  songs" list heading stands for the list); Ctrl-1 Full brings the list back.
run(c selecta-test.mod "${KEYS_C}")
expect(c out_c "text_layout=2 layout_sets=1")
reject(c scr_c "Browse  songs")
reject(c scr_c "Open")
run(d selecta-test.mod "${KEYS_D}")
expect(d out_d "text_layout=0 layout_sets=2")
expect(d scr_d "Browse  songs")

# e: Ctrl-4 Visual shows the visualizer panel alone.
run(e selecta-test.mod "${KEYS_E}")
expect(e out_e "text_layout=3 layout_sets=1")
expect(e scr_e "Scope")
reject(e scr_e "Browse  songs")

# f (I1205): a real terminal cannot send Ctrl-4..Ctrl-8 as CSI-u. Terminal.app and xterm send the C0 bytes 0x1C 0x1D 0x1E
# 0x1F for Ctrl-4..Ctrl-7 (and 0x7F, Backspace, for Ctrl-8, so Ctrl-V 0x16 is the text UI's key for SID registers). This run
# feeds those raw bytes on stdin, the headless real input path, with no keys-file names.
string(ASCII 28 b28)
string(ASCII 29 b29)
string(ASCII 30 b30)
string(ASCII 31 b31)
string(ASCII 22 b22)
file(WRITE "${WORK}/raw_ctrl.bin" "${b28}${b29}${b30}${b31}${b22}")
execute_process(
  COMMAND "${SELECTA}" --output null --size 100x25 --start-dir "${songs}" --config-dir "${WORK}/cfg"
          --exec "OPEN ${songs}/selecta-test.mod" --exec "PLAY"
  INPUT_FILE "${WORK}/raw_ctrl.bin" WORKING_DIRECTORY "${songs}"
  OUTPUT_VARIABLE out_f ERROR_VARIABLE err_f RESULT_VARIABLE rc_f)
if(NOT rc_f EQUAL 0)
  message(FATAL_ERROR "run f exited ${rc_f}\n${out_f}\n${err_f}")
endif()
expect(f out_f "log=PREF_SET ui.desktop.mode visual|PREF_SET ui.desktop.viz viz_scope|PREF_SET ui.desktop.viz viz_levels|PREF_SET ui.desktop.viz viz_spectrum|PREF_SET ui.desktop.viz viz_sidregs")
expect(f out_f "layout_sets=5")

# g (I1210): the palette row for the PREF_SET verb must not show Ctrl-V. C-v runs only "PREF_SET ui.desktop.viz viz_sidregs",
# so a key label next to the whole verb names an action the key does not run. The palette (query PREF) is dumped; the verb row
# is there (PREF_SET) and carries no C-v.
run(g selecta-test.mod "${KEYS_G}")
expect(g scr_g "PREF_SET")
string(REGEX MATCHALL "[^\n]*PREF_SET[^\n]*" g_rows "${scr_g}")
foreach(row IN LISTS g_rows)
  reject(g row "C-v")
endforeach()

# I1211: the palette and the help page carry one row per action of the shared action table (actions.c) with the key the binding
# table gives that exact line. The query SID lists the viz_sidregs action, whose row shows C-v. (The count of rows against the binding
# table is checked in palette-unit; this is the same rows through the real text UI.)
foreach(tag h i)
  string(TOUPPER "${tag}" utag)
  run(${tag} selecta-test.mod "${KEYS_${utag}}")
  string(REGEX MATCHALL "[^\n]*SID PatternScope[^\n]*" rows "${scr_${tag}}")
  list(LENGTH rows nrows)
  if(NOT nrows EQUAL 1)
    message(FATAL_ERROR "${tag}: expected one SID PatternScope row, got ${nrows} in:\n${scr_${tag}}")
  endif()
  expect(${tag} rows "C-v")
endforeach()
