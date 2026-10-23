# T46: sample-export. Variables: CLI SCRIPT EXPECTED WORKDIR GOLDEN_DIR
# 1. SAMPLE lines and ERROR lines of the script equal samples.expected.
# 2. samples-1.wav: RIFF/WAVE, mono 8-bit PCM at 8287 Hz, data = the sample bytes + 128.
# 3. samples-1.8svx / samples-4.8svx: FORM/8SVX, VHDR one-shot and repeat sizes, BODY = signed bytes.
# 4. SHA-256 of the files equal GOLDEN_DIR/<name>.sha256 (golden written when T46 went green).
file(REMOVE "${WORKDIR}/samples-1.wav" "${WORKDIR}/samples-1.8svx" "${WORKDIR}/samples-4.8svx")
execute_process(COMMAND "${CLI}" script "${SCRIPT}" WORKING_DIRECTORY "${WORKDIR}"
  OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "script exited ${rc}\n${out}\n${err}")
endif()
string(REGEX MATCHALL "(SAMPLE [^\n]*|ERROR [^\n]*)\n" lines "${out}")
string(REPLACE ";" "" got "${lines}")
file(READ "${EXPECTED}" want)
if(NOT got STREQUAL want)
  message(FATAL_ERROR "SAMPLE lines differ.\nexpected:\n${want}\ngot:\n${got}\nfull output:\n${out}")
endif()

function(hex_of path var)
  if(NOT EXISTS "${path}")
    message(FATAL_ERROR "missing ${path}\n${out}")
  endif()
  file(READ "${path}" h HEX)
  set(${var} "${h}" PARENT_SCOPE)
endfunction()

# The square wave: 32 bytes 100, 32 bytes 156 (signed -100); +128 bias for WAV.
set(signed1 "")
set(biased1 "")
foreach(i RANGE 0 63)
  if(i LESS 32)
    string(APPEND signed1 "64")
    string(APPEND biased1 "e4")
  else()
    string(APPEND signed1 "9c")
    string(APPEND biased1 "1c")
  endif()
endforeach()
# MOD data is signed: 100 = 0x64, 156 = 0x9c as stored. WAV unsigned = stored + 128 mod 256:
# 0x64 + 0x80 = 0xe4, 0x9c + 0x80 = 0x1c.

hex_of("${WORKDIR}/samples-1.wav" wav)
# RIFF, size 36+64=100 (0x64), WAVE, fmt , 16, PCM 1, mono 1, 8287 (0x205f), byte rate 8287, align 1, bits 8, data, 64
set(wav_head "52494646" "64000000" "57415645" "666d7420" "10000000" "0100" "0100" "5f200000" "5f200000" "0100" "0800" "64617461" "40000000")
string(REPLACE ";" "" wav_head "${wav_head}")
if(NOT wav STREQUAL "${wav_head}${biased1}")
  message(FATAL_ERROR "samples-1.wav differs.\nexpected ${wav_head}${biased1}\ngot      ${wav}")
endif()

hex_of("${WORKDIR}/samples-1.8svx" svx1)
# FORM, size, 8SVX, VHDR 20: oneShot 0, repeat 64, cycle 0, rate 8287, octaves 1, compression 0, volume 0x10000
string(FIND "${svx1}" "464f524d" p0)
string(SUBSTRING "${svx1}" 16 8 form_type)
if(NOT p0 EQUAL 0 OR NOT form_type STREQUAL "38535658")
  message(FATAL_ERROR "samples-1.8svx is not FORM/8SVX: ${svx1}")
endif()
set(vhdr1 "56484452" "00000014" "00000000" "00000040" "00000000" "205f" "01" "00" "00010000")
string(REPLACE ";" "" vhdr1 "${vhdr1}")
string(FIND "${svx1}" "${vhdr1}" pv)
string(FIND "${svx1}" "424f4459" pb)
if(pv LESS 0 OR pb LESS 0)
  message(FATAL_ERROR "samples-1.8svx lacks the expected VHDR or BODY: ${svx1}")
endif()
string(FIND "${svx1}" "00000040${signed1}" pbody)
if(pbody LESS 0)
  message(FATAL_ERROR "samples-1.8svx BODY is not the signed bytes: ${svx1}")
endif()

hex_of("${WORKDIR}/samples-4.8svx" svx4)
# One-shot click: oneShot 512, repeat 0.
string(FIND "${svx4}" "5648445200000014000002000000000000000000" pv4)
if(pv4 LESS 0)
  message(FATAL_ERROR "samples-4.8svx VHDR is not one-shot 512: ${svx4}")
endif()

foreach(f samples-1.wav samples-1.8svx samples-4.8svx)
  file(SHA256 "${WORKDIR}/${f}" h)
  if(NOT EXISTS "${GOLDEN_DIR}/${f}.sha256")
    message(FATAL_ERROR "golden missing: ${GOLDEN_DIR}/${f}.sha256 (hash ${h})")
  endif()
  file(READ "${GOLDEN_DIR}/${f}.sha256" pinned)
  string(STRIP "${pinned}" pinned)
  if(NOT h STREQUAL pinned)
    message(FATAL_ERROR "${f} hash ${h} differs from golden ${pinned}")
  endif()
endforeach()
