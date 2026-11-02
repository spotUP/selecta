# I916: a bad --view makes the start fail with rc 1, and the start-up report carries the UI's own reason
# (sel_ui_open_args.why) instead of "the UI could not open (no terminal?)". Run: cmake -DSEL=<selecta> -P this.
execute_process(COMMAND "${SEL}" --stats --ui desktop --backend mem --view bogusview --output null
  RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
set(all "${out}${err}")
if(NOT rc EQUAL 1)
  message(FATAL_ERROR "I916: expected rc 1, got ${rc}: ${all}")
endif()
if(NOT all MATCHES "selecta: the UI 'desktop' could not open: desktop view 'bogusview' is not testcard")
  message(FATAL_ERROR "I916: startup failure text does not name the view: ${all}")
endif()
message(STATUS "startup_ui_reason_ok")
