# I1187 text-ui-libmode: the text UI answers the Library mode key (c) and the queue scope key (x) with the desktop's modes
# (sel_uim_libmode_lines) and scopes (sel_uim_scope_line). The pane title carries the mode word and the scope label; the exit
# report prints text_libmode (mode in force, 15 none), text_scope (engine scope index), mode_keys and scope_keys, which
# only the code behind those keys moves.
#   -DSELECTA=<exe> -DCWD=<dir holding selecta-test.mod> -DKEYS_<A..G>=<keys files> -DWORK=<dir>
# Exemption: the Sources mode is not offered. The desktop retired it (I678i, nothing sets it) and it lists no view rows.
file(REMOVE_RECURSE "${WORK}")
set(songs "${WORK}/songs")
file(MAKE_DIRECTORY "${songs}" "${WORK}/cfg")
foreach(n alpha bravo)
  file(COPY_FILE "${CWD}/selecta-test.mod" "${songs}/${n}.mod")
endforeach()

function(run tag keys)
  execute_process(
    COMMAND "${SELECTA}" --output null --size 120x25 --start-dir "${songs}" --config-dir "${WORK}/cfg"
            --exec "QUEUE_ADD ${songs}/alpha.mod" --exec "QUEUE_ADD ${songs}/bravo.mod"
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

# a: l opens the Library as Tree; c moves to Columns: the engine lists the drilldown style.
run(a "${KEYS_A}")
expect(a out_a "text_libmode=2 text_scope=0 mode_keys=1 scope_keys=0")
expect(a scr_a "Mode Columns")
expect(a scr_a "Drilldown")

# b: c again is Files: the browse view source.
run(b "${KEYS_B}")
expect(b out_b "text_libmode=3 text_scope=0 mode_keys=2")
expect(b scr_b "Browse")
expect(b scr_b "Mode Files")

# c: History is the home view source.
run(c "${KEYS_C}")
expect(c out_c "text_libmode=4 text_scope=0 mode_keys=3")
expect(c scr_c "Home")
expect(c scr_c "Mode History")

# d: the cycle wraps to Composers (the regular queue list); Sources is never offered.
run(d "${KEYS_D}")
expect(d out_d "text_libmode=0 text_scope=0 mode_keys=4")
expect(d scr_d "Mode Composers")
expect(d scr_d "Queue  2 entries")
string(FIND "${scr_d}" "Mode Sources" at)
if(NOT at LESS 0)
  message(FATAL_ERROR "d: Sources mode is offered:\n${scr_d}")
endif()

# e, f, g: x walks the queue scope Library -> Album -> Playlist -> Library; the title carries the label and the engine took the verb.
run(e "${KEYS_E}")
expect(e out_e "text_scope=1 mode_keys=0 scope_keys=1")
expect(e scr_e "Scope: Album")
run(f "${KEYS_F}")
expect(f out_f "text_scope=2 mode_keys=0 scope_keys=2")
expect(f scr_f "Scope: Playlist")
run(g "${KEYS_G}")
expect(g out_g "text_scope=0 mode_keys=0 scope_keys=3")
expect(g scr_g "Scope: Library")
