# Organya replayer (I703): Studio Pixel's Organya songs (.org, Cave Story) through organya.h v1.03 by Strultz
# (third_party/organya, ISC-style licence, so the module is MIT-compatible and stays in a SELECTA_RELEASE_MIT_ONLY bundle).
# No soundbank is vendored (Cave Story's wave100.wdb has no open licence); see third_party/organya/VERSION.txt.
# Host only: the render path is float DSP; the Amiga exemption is written in ledger row I703 (RULES 4a).
set(ORG_DIR "${PROJECT_SOURCE_DIR}/third_party/organya")
add_library(organya_engine STATIC EXCLUDE_FROM_ALL impl.c)
target_include_directories(organya_engine PUBLIC "${ORG_DIR}")
target_compile_options(organya_engine PRIVATE -w)
target_compile_definitions(organya_engine PUBLIC ORG_USE_STDINT ORG_NO_STDIO)
set_target_properties(organya_engine PROPERTIES POSITION_INDEPENDENT_CODE ON C_VISIBILITY_PRESET hidden)

set(SEL_MODULE_OUTPUT_DIR "${CMAKE_BINARY_DIR}/modules")
sel_add_plugin(organya 1 MODULE adapter.c THIRD_PARTY_INCLUDES "${ORG_DIR}")
target_link_libraries(sel_module_organya PRIVATE organya_engine m)
set_property(TARGET sel_module_organya PROPERTY SEL_MODULE_MIT ON)
