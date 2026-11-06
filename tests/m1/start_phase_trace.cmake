# I625a: the start-up phase trace through the real entry point (apps/selecta/main.c).
# With --log the phase lines reach the log in order; without --log nothing is written to stderr.
# Inputs: SELECTA (executable), WORK (scratch directory).
file(MAKE_DIRECTORY "${WORK}")
set(LOG "${WORK}/phase.log")
file(REMOVE "${LOG}")
file(WRITE "${WORK}/quit.keys" "q\n") # --keys takes a key file: one key per line
execute_process(COMMAND "${SELECTA}" --output null --start-dir "${WORK}" --keys "${WORK}/quit.keys" --screen-dump "${WORK}/screen.txt" --log "${LOG}"
  RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT EXISTS "${LOG}")
  message(FATAL_ERROR "no log file written (rc=${rc})")
endif()
file(READ "${LOG}" text)
set(pos 0)
foreach(phase "system_time=" "main begin" "registry init and plugin registration done plugins=" "library open done"
    "engine created" "output open done" "UI open returned" "all UIs open" "first pump done" "first frame shown")
  string(FIND "${text}" "${phase}" at)
  if(at LESS 0)
    message(FATAL_ERROR "phase '${phase}' missing from the log:\n${text}")
  endif()
  if(at LESS pos)
    message(FATAL_ERROR "phase '${phase}' out of order:\n${text}")
  endif()
  set(pos ${at})
endforeach()
if(NOT text MATCHES "trace t_ms=[0-9]+ main begin")
  message(FATAL_ERROR "main begin line has no millisecond count:\n${text}")
endif()
# Switch off: the same run without --log prints no trace line.
execute_process(COMMAND "${SELECTA}" --output null --start-dir "${WORK}" --keys "${WORK}/quit.keys" --screen-dump "${WORK}/screen2.txt"
  RESULT_VARIABLE rc2 OUTPUT_VARIABLE out2 ERROR_VARIABLE err2)
if(err2 MATCHES "trace t_ms=" OR out2 MATCHES "trace t_ms=" OR err2 MATCHES "system_time=")
  message(FATAL_ERROR "trace lines appeared without --log:\n${err2}")
endif()
# I625a4: every trace line is flushed (the Amiga stderr buffers into the --log file, so a hung start left no line). A host run
# cannot show this (host stderr is unbuffered, so the log is complete either way); the test pins the flush calls in the source
# instead: one fflush(stderr) inside sel_trace and one after the system_time line. It does not prove the Amiga log is readable
# while the process runs; that is the rig check of the row.
file(READ "${SRC}" main_c)
string(REGEX MATCH "static void sel_trace\\(const sel_host\\* h, const char\\* phase\\) \\{[^}]*\\}[^}]*\\}" trace_fn "${main_c}")
if(NOT trace_fn MATCHES "fflush\\(stderr\\)")
  message(FATAL_ERROR "sel_trace does not flush stderr after the trace line")
endif()
if(NOT main_c MATCHES "sel_trace_system_time\\(\\);[ \t\n]*fflush\\(stderr\\)")
  message(FATAL_ERROR "the system_time trace line is not flushed")
endif()
