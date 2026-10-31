# I847a: wires `selecta-cli library gen` and `library bench` through the real CLI entry point. Generates a small catalogue, runs the bench
# once and requires every measure line of design 2026-10-31-interactive-path.md section 1.4 (the sentinel: each line carries a nonzero
# *_calls count and the bench exits 0 only when every measure read rows). The 600k run stays manual (label perf): see the design doc.
# -DCLI=<selecta-cli> -DDIR=<scratch dir>
set(DB "${DIR}/library_gen_bench.db")
file(REMOVE "${DB}")
execute_process(COMMAND "${CLI}" library gen --songs 5000 --out "${DB}" RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT rc EQUAL 0 OR NOT out MATCHES "gen_songs=5000 ")
  message(FATAL_ERROR "gen failed (${rc}): ${out} ${err}")
endif()
execute_process(COMMAND "${CLI}" library bench --db "${DB}" --runs 2 RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
file(REMOVE "${DB}")
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "bench failed (${rc}): ${out} ${err}")
endif()
foreach(k search_first_page_a search_first_page_ch search_first_page_chip search_first_page_purple_motion search_first_page_4mat
          chip_switch list_switch_artists list_switch_albums source_page home_first_paint play_lookup)
  if(NOT out MATCHES "${k}_p95=[0-9.]+ ${k}_calls=[1-9][0-9]* rows=[1-9]")
    message(FATAL_ERROR "bench line missing or empty for ${k}:\n${out}")
  endif()
endforeach()
message("library_gen_bench_ok")
