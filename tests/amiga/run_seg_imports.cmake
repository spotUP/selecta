# I1378a seg-imports-unit (design 2026-11-13-amiga-plugins-on-demand, "Design" item 1, Tests). A v2 LoadSeg module may
# leave core functions undefined; generated thunks call them through the import table the loader writes into the
# module header (+24) before init. Runs the 68k selecta-cli under vamos with PROGDIR: on a staged drawer whose
# Players/sel_pxtone is a test variant that calls sel_tag_find from a constructor (tests/amiga/seg_imports_probe.c):
#   1. plain module from the build's Players/ renders a reference WAV;
#   2. the v2 variant must load (replayer==pxtone, module_loads==1) and render the same bytes;
#   3. the over variant claims one import more than the host has: it must be rejected, never initialised.
# Revert proof: without the loader's slot write the v2 constructor jumps through a null table and step 2 fails.
# Inputs: VAMOS, AMIGA_CLI, SEG_DIR (build/amiga/seg-imports-test), SONG (pxtone fixture), WORK (scratch).
set(MODE vamos)
set(MOD "${SONG}")
include("${CMAKE_CURRENT_LIST_DIR}/../m2/lib.cmake")
set(TAG I1378a)

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}/drawer/Players")
configure_file("${SONG}" "${WORK}/test.ptcop" COPYONLY)
foreach(v v2 over)
  if(NOT EXISTS "${SEG_DIR}/${v}/sel_pxtone")
    message(FATAL_ERROR "RED[${TAG}]: ${SEG_DIR}/${v}/sel_pxtone missing (build target sel_seg_pxtone_${v}).")
  endif()
endforeach()

# render(out_var wav progdir): the 68k CLI with PROGDIR: on <progdir> ("" keeps the CLI's own build directory).
function(render out_var wav progdir)
  sel_vamos_cmd(vcmd "${AMIGA_CLI}")
  set(extra "")
  if(progdir)
    list(FIND vcmd "PROGDIR:selbin:" i)
    if(i LESS 0)
      message(FATAL_ERROR "RED[${TAG}]: sel_vamos_cmd no longer assigns PROGDIR:selbin: (tests/amiga/vamos_cmd.cmake changed).")
    endif()
    list(REMOVE_AT vcmd ${i})
    list(INSERT vcmd ${i} "PROGDIR:segtest:")
    set(extra -V "segtest:${progdir}")
  endif()
  execute_process(COMMAND ${vcmd} ${extra} -V "work:${WORK}"
                          -- "${AMIGA_CLI}" render work:test.ptcop --output "work:${wav}" --seconds 2 --rate 44100
                  OUTPUT_VARIABLE o ERROR_VARIABLE e RESULT_VARIABLE r)
  set(${out_var} "${o}${e}\nexit=${r}" PARENT_SCOPE)
endfunction()

render(out ref.wav "")
m2_check(${TAG} "${out}" "replayer==pxtone,module_loads==1")

configure_file("${SEG_DIR}/v2/sel_pxtone" "${WORK}/drawer/Players/sel_pxtone" COPYONLY)
render(out v2.wav "${WORK}/drawer")
m2_check(${TAG} "${out}" "replayer==pxtone,module_loads==1")
file(SHA256 "${WORK}/ref.wav" ref_hash)
file(SHA256 "${WORK}/v2.wav" v2_hash)
if(NOT ref_hash STREQUAL v2_hash)
  message(FATAL_ERROR "RED[${TAG}]: the v2 module rendered different audio than the plain module.\n${out}")
endif()

configure_file("${SEG_DIR}/over/sel_pxtone" "${WORK}/drawer/Players/sel_pxtone" COPYONLY)
render(out over.wav "${WORK}/drawer")
m2_get("${out}" replayer rep)
m2_get("${out}" module_loads loads)
if(rep STREQUAL "pxtone" OR loads STREQUAL "1")
  message(FATAL_ERROR "RED[${TAG}]: a module needing more imports than the host has was loaded.\n${out}")
endif()
file(REMOVE_RECURSE "${WORK}")
message(STATUS "[OK] ${TAG}: v2 module calls sel_tag_find through the import table and renders identical audio; "
               "an over-count module is rejected")
