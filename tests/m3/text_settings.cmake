# I1188 text-ui-settings: the text UI Settings screen (key S, palette entry "settings") lists the shared pref rows
# (sel_uim_pref_row_at), shows the engine value and sends PREF_SET for every change. The exit report prints
# text_settings_sets / settings_opens / settings_row (the sentinel only the Settings code moves) and the engine pref read back.
#   -DSELECTA=<exe> -DCWD=<dir holding selecta-test.mod> -DKEYS_<A..C>=<keys files> -DWORK=<dir>
# Exemptions: UI Zoom (pixel scale, no meaning on a character grid), the C64 ROM file chooser (the screen shows the ROM source).
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}/songs" "${WORK}/cfg")

function(run tag keys)
  execute_process(
    COMMAND "${SELECTA}" --output null --size 120x30 --start-dir "${WORK}/songs" --config-dir "${WORK}/cfg"
            --keys "${keys}" --screen-dump "${WORK}/${tag}.txt"
    WORKING_DIRECTORY "${WORK}/songs"
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

# a: S opens Settings; Right on Crossfeed (auto -> on), Left on Stereo Width (100 -> 95); both reach the engine.
run(a "${KEYS_A}")
expect(a out_a "text_settings_sets=2 settings_opens=1 settings_row=3 crossfeed=on stereo_width=95")
expect(a scr_a "Settings")
expect(a scr_a "Crossfeed")
expect(a scr_a "< on >")
expect(a scr_a " 95 [")
expect(a scr_a "Loudness")
# the shared table hides the Amiga-only rows on this machine
string(FIND "${scr_a}" "reSID Filter" at)
if(NOT at LESS 0)
  message(FATAL_ERROR "a: Amiga-only row shown on the host:\n${scr_a}")
endif()
file(READ "${WORK}/cfg/prefs.conf" prefs)
expect(a prefs "dsp.crossfeed")

# b: a new process reads the persisted value back.
run(b "${KEYS_B}")
expect(b out_b "text_settings_sets=0 settings_opens=1 settings_row=0 crossfeed=on")
expect(b scr_b "< on >")

# c: the palette entry opens the same screen.
run(c "${KEYS_C}")
expect(c out_c "settings_opens=1")
expect(c scr_c "Settings")
expect(c scr_c "Crossfeed")
