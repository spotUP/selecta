# I1137: a fresh install copies the shipped library-template.db. The built app (the staged tree: bundle Contents/Resources on macOS,
# the executable's directory elsewhere) is started through its real entry point with an empty config directory; the --log trace must say
# `template copied <bytes>` and never `final schema created`, and library.db must exist afterwards.
# Inputs: SELECTA (executable), TEMPLATE_DIR (where the build staged library-template.db), WORK (scratch directory).
if(NOT EXISTS "${TEMPLATE_DIR}/library-template.db")
  message(FATAL_ERROR "[ERROR] library-template.db is not staged in ${TEMPLATE_DIR}")
endif()
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}/xdg/selecta")
file(WRITE "${WORK}/quit.keys" "q\n")
set(LOG "${WORK}/start.log")
# A scripted run (--keys, --output null) never opens the default database (main.c), so --db names the file a user's first start would make
# in an empty config directory; no template lies beside it, so the copy must come from the app's own directory.
execute_process(COMMAND "${SELECTA}" --db "${WORK}/xdg/selecta/library.db" --output null --start-dir "${WORK}" --keys "${WORK}/quit.keys" --screen-dump "${WORK}/screen.txt" --log "${LOG}"
  RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT EXISTS "${LOG}")
  message(FATAL_ERROR "no log file written (rc=${rc})")
endif()
file(READ "${LOG}" text)
if(NOT text MATCHES "template copied [0-9]+")
  message(FATAL_ERROR "[ERROR] no `template copied` in the trace of a fresh start:\n${text}")
endif()
if(text MATCHES "final schema created")
  message(FATAL_ERROR "[ERROR] the DDL path ran although the template is shipped:\n${text}")
endif()
if(NOT EXISTS "${WORK}/xdg/selecta/library.db")
  message(FATAL_ERROR "[ERROR] library.db was not made in the fresh config directory")
endif()
message(STATUS "[OK] fresh start copied the template")
