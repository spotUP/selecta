# I427e2: the release drawer's Players/ is where a real install loads its replayers from. Stages the Selecta archive
# tree exactly as the selecta_dist target does (cmake/selecta_dist.cmake, the tree CI packs into Selecta.lha), then runs
# the 68k selecta-cli under vamos with PROGDIR: assigned to that staged drawer, so LoadSeg("PROGDIR:Players/sel_pxtone")
# can only succeed from the drawer's own Players/. A pxtone song must render through the module (replayer==pxtone,
# module_loads==1). Second step, the revert proof built in: with Players/ moved away the same render must not load it.
# Inputs: VAMOS, AMIGA_CLI (build/amiga/apps/selecta-cli/selecta-cli), APP_DIR (build/amiga/apps/selecta: selecta,
# library-template.db, Players/), ROOT (repo), SONG (pxtone fixture), WORK (scratch).
set(MODE vamos)
set(MOD "${SONG}")
include("${CMAKE_CURRENT_LIST_DIR}/../m2/lib.cmake")
set(TAG I427e2)

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
set(drawer "${WORK}/Selecta")
# The template is only baked when build/amiga was configured with -DSELECTA_TEMPLATEBAKE_EXE (src/library/CMakeLists.txt).
# This test checks Players/, not the template, so a build without it stages from a copy of APP_DIR with a stand-in
# template (as the selecta_dist.cmake selftest does) instead of failing on a file it does not test.
set(bin_dir "${APP_DIR}")
if(NOT EXISTS "${APP_DIR}/library-template.db")
  set(bin_dir "${WORK}/bin")
  file(MAKE_DIRECTORY "${bin_dir}")
  file(COPY "${APP_DIR}/selecta" "${APP_DIR}/Players" DESTINATION "${bin_dir}")
  file(WRITE "${bin_dir}/library-template.db" "stand-in template\n")
  message(STATUS "[INFO] ${APP_DIR} has no library-template.db; staging with a stand-in template")
endif()
execute_process(COMMAND "${CMAKE_COMMAND}" -DMODE=stage "-DDEST=${drawer}" "-DBIN_DIR=${bin_dir}" "-DROOT=${ROOT}"
                        -P "${ROOT}/cmake/selecta_dist.cmake"
                RESULT_VARIABLE rc OUTPUT_VARIABLE so ERROR_VARIABLE se)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "RED[${TAG}]: staging the release drawer failed (${rc}).\n${so}${se}")
endif()
if(NOT EXISTS "${drawer}/Players/sel_pxtone")
  message(FATAL_ERROR "RED[${TAG}]: the release drawer has no Players/sel_pxtone (the archive would ship without its players).")
endif()
configure_file("${SONG}" "${WORK}/test.ptcop" COPYONLY)

# render(out): the 68k CLI with PROGDIR: on the staged drawer instead of the CLI's own build directory.
function(render out_var)
  sel_vamos_cmd(vcmd "${AMIGA_CLI}")
  list(FIND vcmd "PROGDIR:selbin:" i)
  if(i LESS 0)
    message(FATAL_ERROR "RED[${TAG}]: sel_vamos_cmd no longer assigns PROGDIR:selbin: (tests/amiga/vamos_cmd.cmake changed).")
  endif()
  list(REMOVE_AT vcmd ${i})
  list(INSERT vcmd ${i} "PROGDIR:seldist:")
  execute_process(COMMAND ${vcmd} -V "seldist:${drawer}" -V "work:${WORK}"
                          -- "${AMIGA_CLI}" render --output ahi-shadow work:test.ptcop --seconds 2
                  OUTPUT_VARIABLE o ERROR_VARIABLE e RESULT_VARIABLE r)
  set(${out_var} "${o}${e}\nexit=${r}" PARENT_SCOPE)
endfunction()

render(out)
m2_check(${TAG} "${out}" "replayer==pxtone,module_loads==1,rendered_frames>=56000,underruns==0")

file(RENAME "${drawer}/Players" "${WORK}/Players.away")
render(out)
m2_get("${out}" module_loads loads)
m2_get("${out}" replayer rep)
if(loads STREQUAL "1" OR rep STREQUAL "pxtone")
  message(FATAL_ERROR "RED[${TAG}]: pxtone still loaded with the drawer's Players/ moved away: the load did not come from "
                      "the release drawer.\n${out}")
endif()
file(REMOVE_RECURSE "${WORK}")
message(STATUS "[OK] ${TAG}: release drawer Players/ loads sel_pxtone under vamos; without Players/ it does not")
