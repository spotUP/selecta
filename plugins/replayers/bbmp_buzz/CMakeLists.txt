# bbmp buzz replayer (I710d): Jeskola Buzz songs (.bmx) that use only BmxPlay's machines. The engine is bbmp's csrc/pc/buzz_engine.c
# (GPL-3.0-or-later) on BmxPlay's public-domain player headers (third_party/bbmp_buzz). GPL, so its own loadable module under
# SELECTA_GPL_PLUGINS; the MIT core never links it. Host only (RULES 4a exemption in ledger row I710d).
set(BBBUZZ_DIR "${PROJECT_SOURCE_DIR}/third_party/bbmp_buzz")
set(SEL_MODULE_OUTPUT_DIR "${CMAKE_BINARY_DIR}/modules")
sel_add_plugin(bbmp_buzz 1 MODULE adapter.c "${BBBUZZ_DIR}/buzz_engine.c" THIRD_PARTY_INCLUDES "${BBBUZZ_DIR}")
set_source_files_properties("${BBBUZZ_DIR}/buzz_engine.c" PROPERTIES COMPILE_OPTIONS "-w")
target_link_libraries(sel_module_bbmp_buzz PRIVATE m)
