# I1222i text-ui-regions: the text UI places main and detail by the shared layout regions. The detail column shows the
# Now Playing panel content (sel_uim_np_lines): a Next in Queue block and the Like/Rating line. Key i hides it. Tab is
# region focus (switch_view moved to V), so Tab must not change the page.
#   -DSELECTA=<exe> -DCWD=<dir holding selecta-test.mod> -DKEYS_<A..C>=<keys files> -DWORK=<dir>
file(REMOVE_RECURSE "${WORK}")
set(songs "${WORK}/songs")
file(MAKE_DIRECTORY "${songs}" "${WORK}/cfg")
file(COPY_FILE "${CWD}/selecta-test.mod" "${songs}/alpha.mod")

function(run tag keys size)
  execute_process(
    COMMAND "${SELECTA}" --output null --size ${size} --start-dir "${songs}" --config-dir "${WORK}/cfg"
            --exec "QUEUE_ADD ${songs}/alpha.mod" --keys "${keys}" --screen-dump "${WORK}/${tag}.txt"
    WORKING_DIRECTORY "${songs}"
    OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE rc)
  if(NOT rc EQUAL 0)
    message(FATAL_ERROR "run ${tag} exited ${rc}\n${out}\n${err}")
  endif()
  file(READ "${WORK}/${tag}.txt" scr)
  set(scr_${tag} "${scr}" PARENT_SCOPE)
endfunction()

run(a "${KEYS_A}" 120x40)
if(NOT scr_a MATCHES "Next in Queue" OR NOT scr_a MATCHES "Like  Rating")
  message(FATAL_ERROR "a: detail column lacks Now Playing content:\n${scr_a}")
endif()
run(b "${KEYS_B}" 120x40)
if(scr_b MATCHES "Next in Queue")
  message(FATAL_ERROR "b: i did not hide the detail column:\n${scr_b}")
endif()
run(c "${KEYS_C}" 120x40)
if(NOT scr_c STREQUAL scr_a)
  message(FATAL_ERROR "c: Tab changed the page, want focus only:\n${scr_c}")
endif()
# I1222i-2: 80 columns are two-column (design section 3): sidebar 20 and main, no detail column; Now Playing opens in main.
run(d "${KEYS_A}" 80x25)
if(scr_d MATCHES "Next in Queue" OR scr_d MATCHES "Like  Rating")
  message(FATAL_ERROR "d: a detail column at 80 columns:\n${scr_d}")
endif()
# I1222i-3: focus routing. Enter opens the row under the cursor (Browse: ".." first, alpha.mod second), so a j that moved the
# cursor changes the page. Focus on sidebar or main: j moves (e, h). Focus on detail or player: j is swallowed (f, i), the page
# equals the one with no j (g).
run(e "${KEYS_E}" 120x40)
run(f "${KEYS_F}" 120x40)
run(g "${KEYS_G}" 120x40)
run(h "${KEYS_H}" 120x40)
run(i "${KEYS_I}" 120x40)
if(scr_e STREQUAL scr_g)
  message(FATAL_ERROR "e: j did not move the list at sidebar focus:\n${scr_e}")
endif()
if(NOT scr_h STREQUAL scr_e)
  message(FATAL_ERROR "h: j did not move the list at main focus:\n${scr_h}")
endif()
if(NOT scr_f STREQUAL scr_g)
  message(FATAL_ERROR "f: j moved the list at detail focus:\n${scr_f}")
endif()
if(NOT scr_i STREQUAL scr_g)
  message(FATAL_ERROR "i: j moved the list at player focus:\n${scr_i}")
endif()
message(STATUS "text_regions ok")
