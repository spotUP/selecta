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
message(STATUS "text_regions ok")
