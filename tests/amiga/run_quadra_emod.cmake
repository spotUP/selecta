# I1323 driver: a Quadra Composer .emod loads and renders under vamos (68k). m68k-amigaos-gcc 6.5 at -O2
# miscompiled qc_create when qc_load_module was inlined: it returned the file buffer instead of the module,
# so quadracomposer_start read garbage and vamos died with InvalidMemoryAccessError (rc=1). The host build
# was fine, so only a 68k run catches it. Fixture: tests/fixtures/quadra/mini.emod, a synthetic 244 byte module.
# Inputs: -DVAMOS -DAMIGA_CLI -DSONG; WORKING_DIRECTORY must be the repo root (vamos wants a short relative path).
foreach(v VAMOS AMIGA_CLI SONG)
  if(NOT DEFINED ${v})
    message(FATAL_ERROR "run_quadra_emod.cmake: -D${v} is required")
  endif()
endforeach()

set(dir "build/quadra-emod-test")
file(REMOVE_RECURSE "${dir}")
file(MAKE_DIRECTORY "${dir}")
file(COPY "${SONG}" DESTINATION "${dir}")

include("${CMAKE_CURRENT_LIST_DIR}/vamos_cmd.cmake")
sel_vamos_cmd(SEL_VAMOS_CMD "${AMIGA_CLI}")
execute_process(COMMAND ${SEL_VAMOS_CMD} -- "${AMIGA_CLI}" render "${dir}/mini.emod"
          --output "${dir}/out.wav" --seconds 1 --rate 44100
  OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE rc TIMEOUT 90)
file(REMOVE_RECURSE "${dir}")
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "render mini.emod under vamos: exit '${rc}'\n${out}${err}")
endif()
if(NOT out MATCHES "module_loads=1" OR NOT out MATCHES "ticks=[1-9][0-9]*" OR NOT out MATCHES "mixed_frames=[1-9][0-9]*")
  message(FATAL_ERROR "mini.emod did not play (want module_loads=1, ticks>0, mixed_frames>0):\n${out}${err}")
endif()
message(STATUS "[OK] Quadra Composer module renders under vamos")
