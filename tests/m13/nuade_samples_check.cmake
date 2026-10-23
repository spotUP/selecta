# T46 nuade scan path (row I104b). Variables: CLI SCRIPT MOD WORKDIR
# selecta-cli script runs OPEN, SAMPLE_LIST and SAMPLE_EXPORT on selecta-test.mod, which the nuade
# replayer plays; its RP_SAMPLE_GET catalogue is built by scanning the module. The expected values
# come from the module file itself (header sample table, then the sample data after the patterns):
# Slot numbers are catalogue positions in order of first use (plan Q27 default), not module sample
# numbers: this module's song uses samples 1, 2, 4, 3 in that order, so slot k shows module sample
# slot_to_mod[k].
# 1. SAMPLE lines: slot, length in bytes (the module's), rate 8287, four lines.
# 2. The 8SVX BODY of slots 1 and 3 equals the bytes of module samples 1 and 4.
# 3. The WAV of sample 1 is 44 + length bytes.
# 4. Slot 9 answers ERROR range.
file(REMOVE "${WORKDIR}/nuade-samples-1.8svx" "${WORKDIR}/nuade-samples-3.8svx" "${WORKDIR}/nuade-samples-1.wav")
execute_process(COMMAND "${CLI}" script "${SCRIPT}" WORKING_DIRECTORY "${WORKDIR}"
  OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "script exited ${rc}\n${out}\n${err}")
endif()
file(READ "${MOD}" mod HEX)
# selecta-test.mod is an IFF MODL file: the ProTracker module is the PTDT chunk body.
string(FIND "${mod}" "50544454" ptdt)
if(ptdt LESS 0)
  message(FATAL_ERROR "no PTDT chunk in ${MOD}")
endif()
math(EXPR base "${ptdt} / 2 + 8")

function(mod_byte off var) # offset inside the ProTracker module
  math(EXPR h "(${base} + ${off}) * 2")
  string(SUBSTRING "${mod}" ${h} 2 b)
  math(EXPR v "0x${b}")
  set(${var} ${v} PARENT_SCOPE)
endfunction()

# Pattern count: highest order entry + 1.
set(maxord 0)
foreach(i RANGE 0 127)
  math(EXPR off "952 + ${i}")
  mod_byte(${off} o)
  if(o GREATER maxord)
    set(maxord ${o})
  endif()
endforeach()
math(EXPR data_off "1084 + 1024 * (${maxord} + 1)")

set(pos ${data_off})
set(want "")
foreach(n RANGE 1 4)
  math(EXPR hdr "20 + 30 * (${n} - 1) + 22")
  mod_byte(${hdr} hi)
  math(EXPR lo_off "${hdr} + 1")
  mod_byte(${lo_off} lo)
  math(EXPR len "(${hi} * 256 + ${lo}) * 2")
  set(len_${n} ${len})
  set(pos_${n} ${pos})
  math(EXPR pos "${pos} + ${len}")
endforeach()

set(slot_to_mod 1 2 4 3)
string(REGEX MATCHALL "SAMPLE [0-9]+ [0-9]+ [0-9]+ [0-9]+ [0-9]+ \"[^\"]*\"" lines "${out}")
list(LENGTH lines nlines)
if(NOT nlines EQUAL 4)
  message(FATAL_ERROR "expected 4 SAMPLE lines, got ${nlines}\n${out}")
endif()
foreach(n RANGE 1 4)
  math(EXPR idx "${n} - 1")
  list(GET lines ${idx} line)
  list(GET slot_to_mod ${idx} m)
  if(NOT line MATCHES "^SAMPLE ${n} ${len_${m}} [0-9]+ [0-9]+ 8287 ")
    message(FATAL_ERROR "SAMPLE ${n} should have length ${len_${m}} (module sample ${m}) at 8287 Hz: ${line}")
  endif()
endforeach()
if(NOT out MATCHES "ERROR range SAMPLE_EXPORT")
  message(FATAL_ERROR "slot 9 did not answer ERROR range\n${out}")
endif()

function(body_of path n m) # catalogue slot n shows module sample m
  if(NOT EXISTS "${path}")
    message(FATAL_ERROR "missing ${path}\n${out}")
  endif()
  file(READ "${path}" svx HEX)
  math(EXPR bytes "${len_${m}}")
  math(EXPR hexlen "${bytes} * 2")
  math(EXPR mod_hex_off "(${base} + ${pos_${m}}) * 2")
  string(SUBSTRING "${mod}" ${mod_hex_off} ${hexlen} want_body)
  string(FIND "${svx}" "424f4459" pb)
  math(EXPR body_hex_off "${pb} + 16")
  string(SUBSTRING "${svx}" ${body_hex_off} ${hexlen} got_body)
  if(NOT got_body STREQUAL want_body)
    message(FATAL_ERROR "${path} BODY differs from module sample ${m}\nwant ${want_body}\ngot  ${got_body}")
  endif()
endfunction()
body_of("${WORKDIR}/nuade-samples-1.8svx" 1 1)
body_of("${WORKDIR}/nuade-samples-3.8svx" 3 4)

file(SIZE "${WORKDIR}/nuade-samples-1.wav" wav_size)
math(EXPR wav_want "44 + ${len_1}")
if(NOT wav_size EQUAL wav_want)
  message(FATAL_ERROR "nuade-samples-1.wav is ${wav_size} bytes, want ${wav_want}")
endif()
message(STATUS "nuade samples: ${nlines} SAMPLE lines, BODY of 1 and 4 equal the module bytes")
