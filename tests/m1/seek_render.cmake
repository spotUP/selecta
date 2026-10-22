# T21 (both halves): render 5 s after a 5000 ms seek and compare with the
# second half of the unseeked 10 s render, byte for byte.
# Variables: CLI MOD OUT_DIR RESAMPLE PATH_RE (seek_path line regex) TAG (file name tag)
# POS_LINE (optional): exact 'seek_position order=O row=R pattern=P' line the seeked render
# must print (RP_POSITION right after the seek).
# EXACT (default 1): byte-for-byte comparison with the unseeked render. 0 for a
# native seek (libopenmpt re-enters the song with fresh channel state, so only
# the size and a non-silent body are checked; see the plan's Deviations, I56).
file(MAKE_DIRECTORY "${OUT_DIR}")
set(full "${OUT_DIR}/seek-full-${TAG}.wav")
set(seeked "${OUT_DIR}/seek-5000-${TAG}.wav")
file(REMOVE "${full}" "${seeked}")
execute_process(
  COMMAND "${CLI}" render "${MOD}" --output "${full}" --seconds 10 --rate 44100 --resample ${RESAMPLE}
  RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "full render exited ${rc}\n${out}\n${err}")
endif()
execute_process(
  COMMAND "${CLI}" render "${MOD}" --output "${seeked}" --seek-ms 5000 --seconds 5 --rate 44100
          --resample ${RESAMPLE}
  RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "seeked render exited ${rc}\nstdout:\n${out}\nstderr:\n${err}")
endif()
if(NOT out MATCHES "${PATH_RE}")
  message(FATAL_ERROR "seek path line missing, expected '${PATH_RE}':\n${out}")
endif()
if(NOT "${out}\n${err}" MATCHES "SEEKED 220500")
  message(FATAL_ERROR "event 'SEEKED 220500' missing:\n${out}\n${err}")
endif()
if(DEFINED POS_LINE AND NOT out MATCHES "(^|\n)${POS_LINE}\n")
  message(FATAL_ERROR "seek position line '${POS_LINE}' missing:\n${out}")
endif()
# 44 header bytes, then 5 s of stereo int16 at 44100 Hz = 882000 bytes.
file(SIZE "${seeked}" seeked_size)
if(NOT seeked_size EQUAL 882044)
  message(FATAL_ERROR "seeked WAV size ${seeked_size}, expected 882044")
endif()
if(DEFINED EXACT AND NOT EXACT)
  file(READ "${seeked}" body HEX OFFSET 44 LIMIT 4000)
  if(body MATCHES "^0*$")
    message(FATAL_ERROR "seeked render is silent")
  endif()
  return()
endif()
file(READ "${full}" tail HEX OFFSET 882044)
file(READ "${seeked}" body HEX OFFSET 44)
if(NOT tail STREQUAL body)
  message(FATAL_ERROR "seeked render differs from bytes 882000 onward of the 10 s render")
endif()
