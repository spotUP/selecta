# T21 (nuade half): render 5 s after a 5000 ms seek and compare with the
# second half of the unseeked 10 s render, byte for byte.
# Variables: CLI MOD OUT_DIR RESAMPLE
file(MAKE_DIRECTORY "${OUT_DIR}")
set(full "${OUT_DIR}/seek-full.wav")
set(seeked "${OUT_DIR}/seek-5000.wav")
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
if(NOT out MATCHES "seek_path=skip skipped_frames=220500")
  message(FATAL_ERROR "seek path line missing, expected 'seek_path=skip skipped_frames=220500':\n${out}")
endif()
if(NOT "${out}\n${err}" MATCHES "SEEKED 220500")
  message(FATAL_ERROR "event 'SEEKED 220500' missing:\n${out}\n${err}")
endif()
# 44 header bytes, then 5 s of stereo int16 at 44100 Hz = 882000 bytes.
file(SIZE "${seeked}" seeked_size)
if(NOT seeked_size EQUAL 882044)
  message(FATAL_ERROR "seeked WAV size ${seeked_size}, expected 882044")
endif()
file(READ "${full}" tail HEX OFFSET 882044)
file(READ "${seeked}" body HEX OFFSET 44)
if(NOT tail STREQUAL body)
  message(FATAL_ERROR "seeked render differs from bytes 882000 onward of the 10 s render")
endif()
