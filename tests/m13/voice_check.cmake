# T42: voice control through `selecta-cli render <song> --script <file>`.
# Variables: CLI NUADE_MOD MK_MOD ZERO1_MOD OUT_DIR SCRIPTS GOLDEN
# Contract of render --script: the script lines are sent before the first sample is pulled;
# stdout then carries `mute_mask=0x%08x`, `end_position order=O row=R`, `pitch_factor_q16=N`
# (65536 = 1.0; VOICE kind: the resample step factor, RENDER kind: the replayer pitch factor)
# and every ERROR event as `ERROR <code> <detail>`.
file(MAKE_DIRECTORY "${OUT_DIR}")

function(sel_render tag song script seconds)
  set(wav "${OUT_DIR}/${tag}.wav")
  file(REMOVE "${wav}")
  execute_process(
    COMMAND "${CLI}" render "${song}" --output "${wav}" --seconds ${seconds} --script "${SCRIPTS}/${script}.sel"
    OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE rc)
  if(NOT rc EQUAL 0)
    message(FATAL_ERROR "${tag}: render exited ${rc}\nstdout:\n${out}\nstderr:\n${err}")
  endif()
  file(SHA256 "${wav}" hash)
  set(${tag}_out "${out}" PARENT_SCOPE)
  set(${tag}_hash "${hash}" PARENT_SCOPE)
endfunction()

function(sel_expect tag regex)
  if(NOT ${tag}_out MATCHES "${regex}")
    message(FATAL_ERROR "${tag}: expected /${regex}/ in:\n${${tag}_out}")
  endif()
endfunction()

function(sel_position tag var)
  string(REGEX MATCH "end_position order=[0-9]+ row=[0-9]+" pos "${${tag}_out}")
  if(pos STREQUAL "")
    message(FATAL_ERROR "${tag}: no end_position line in:\n${${tag}_out}")
  endif()
  set(${var} "${pos}" PARENT_SCOPE)
endfunction()

# Mute golden: MUTE 1 on equals the module with voice 1's sample zeroed, and the committed hash.
sel_render(nu_zero ${ZERO1_MOD} none 2)
sel_render(nu_plain ${NUADE_MOD} none 2)
sel_render(nu_mute ${NUADE_MOD} mute 2)
sel_expect(nu_mute "mute_mask=0x00000001")
file(READ "${GOLDEN}" pinned)
string(SUBSTRING "${pinned}" 0 64 pinned)
if(NOT nu_zero_hash STREQUAL pinned)
  message(FATAL_ERROR "zeroed-sample render ${nu_zero_hash} differs from golden ${pinned}")
endif()
if(NOT nu_mute_hash STREQUAL pinned)
  message(FATAL_ERROR "MUTE 1 on render ${nu_mute_hash} differs from golden ${pinned}")
endif()
if(nu_plain_hash STREQUAL pinned)
  message(FATAL_ERROR "unmuted render equals the muted golden")
endif()

sel_render(mk_plain ${MK_MOD} none 2)
sel_render(mk_mute ${MK_MOD} mute 2)
sel_expect(mk_mute "mute_mask=0x00000001")
if(mk_mute_hash STREQUAL mk_plain_hash)
  message(FATAL_ERROR "openmpt: MUTE 1 on did not change the render")
endif()

foreach(song nu mk)
  if(song STREQUAL "nu")
    set(path "${NUADE_MOD}")
  else()
    set(path "${MK_MOD}")
  endif()
  sel_render(${song}_toggle ${path} toggle 1)
  sel_expect(${song}_toggle "mute_mask=0x00000002")
  sel_render(${song}_solo ${path} solo 1)
  sel_expect(${song}_solo "mute_mask=0x0000000b")   # SOLO 2: 0b1011 muted
  sel_render(${song}_solo0 ${path} solo_clear 1)
  sel_expect(${song}_solo0 "mute_mask=0x00000000")

  # PITCH 1200 doubles the factor (Q16 131072); VOICE kind: resample step, RENDER kind: openmpt pitch factor 2.0.
  sel_render(${song}_pitch ${path} pitch 1)
  sel_expect(${song}_pitch "pitch_factor_q16=131072")
  # TEMPO 50: 1.5x tick rate, so 2 s reaches the row 3 s reaches straight.
  sel_render(${song}_tempo ${path} tempo 2)
  sel_render(${song}_t3 ${path} none 3)
  sel_position(${song}_tempo pos_fast)
  sel_position(${song}_t3 pos_straight)
  if(NOT pos_fast STREQUAL pos_straight)
    message(FATAL_ERROR "${song}: TEMPO 50 for 2 s ended at '${pos_fast}', 3 s straight at '${pos_straight}'")
  endif()
  sel_render(${song}_range ${path} pitch_range 1)
  sel_expect(${song}_range "ERROR range PITCH")
endforeach()
